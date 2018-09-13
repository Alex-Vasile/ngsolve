#include <comp.hpp>
using namespace ngcomp;



#include "../linalg/concurrentqueue.h"
typedef moodycamel::ConcurrentQueue<size_t> TQueue; 
typedef moodycamel::ProducerToken TPToken; 
typedef moodycamel::ConsumerToken TCToken; 



namespace ngcomp
{

  static TQueue queue;

  template <typename TFUNC>
  void RunParallelDependency (FlatTable<int> dag,
                              TFUNC func)
  {
    Array<atomic<int>> cnt_dep(dag.Size());

    for (auto & d : cnt_dep) 
      d.store (0, memory_order_relaxed);

    static Timer t_cntdep("count dep");
    t_cntdep.Start();
    ParallelFor (Range(dag),
                 [&] (int i)
                 {
                   for (int j : dag[i])
                     cnt_dep[j]++;
                 });
    t_cntdep.Stop();    

    atomic<size_t> num_ready(0), num_final(0);
    ParallelForRange (cnt_dep.Size(), [&] (IntRange r)
                      {
                        size_t my_ready = 0, my_final = 0;
                        for (size_t i : r)
                          {
                            if (cnt_dep[i] == 0) my_ready++;
                            if (dag[i].Size() == 0) my_final++;
                          }
                        num_ready += my_ready;
                        num_final += my_final;
                      });

    Array<int> ready(num_ready);
    ready.SetSize0();
    for (int j : Range(cnt_dep))
      if (cnt_dep[j] == 0) ready.Append(j);

    
    if (!task_manager)
      // if (true)
      {
        while (ready.Size())
          {
            int size = ready.Size();
            int nr = ready[size-1];
            ready.SetSize(size-1);
            
            func(nr);
            
            for (int j : dag[nr])
              {
                cnt_dep[j]--;
                if (cnt_dep[j] == 0)
                  ready.Append(j);
              }
          }
        return;
      }

    atomic<int> cnt_final(0);
    SharedLoop2 sl(Range(ready));

    task_manager -> CreateJob 
      ([&] (const TaskInfo & ti)
       {
         size_t my_final = 0;
         TPToken ptoken(queue); 
         TCToken ctoken(queue); 
        
         for (int i : sl)
           queue.enqueue (ptoken, ready[i]);

         while (1)
           {
             if (cnt_final >= num_final) break;

             int nr;
             if(!queue.try_dequeue_from_producer(ptoken, nr)) 
               if(!queue.try_dequeue(ctoken, nr))
                 {
                   if (my_final)
                     {
                       cnt_final += my_final;
                       my_final = 0;
                     }
                   continue;
                 }

             if (dag[nr].Size() == 0)
               my_final++;
             // cnt_final++;

             func(nr);

             for (int j : dag[nr])
               {
                 if (--cnt_dep[j] == 0)
                   queue.enqueue (ptoken, j);
               }
           }
       });
  }








  

  template <class SCAL>
  class H1AMG_Matrix : public BaseMatrix
  {
    size_t size;
    shared_ptr<SparseMatrixTM<SCAL>> mat;
    shared_ptr<BaseBlockJacobiPrecond> smoother;
    shared_ptr<SparseMatrixTM<double>> prolongation, restriction;
    shared_ptr<BaseMatrix> coarse_precond;
    int smoothing_steps = 1;
    
  public:
    H1AMG_Matrix (shared_ptr<SparseMatrixTM<SCAL>> amat,
                  shared_ptr<BitArray> freedofs,
                  FlatArray<INT<2>> e2v,
                  FlatArray<double> edge_weights,
                  FlatArray<double> vertex_weights)
      : mat(amat)
    {
      size_t num_edges = edge_weights.Size();
      size_t num_vertices = vertex_weights.Size();

      cout << "H1AMG: ne = " << num_edges << ", nv = " << num_vertices << endl;

      size = mat->Height();

      Array<double> edge_collapse_weights(num_edges);
      Array<double> sum_vertex_weights(num_vertices);
      for (auto i : Range(num_vertices))
        sum_vertex_weights[i] = vertex_weights[i];

      ParallelFor (num_edges, [&] (size_t i)
                   {
                     for (size_t j = 0; j < 2; j++) 
                       AsAtomic(sum_vertex_weights[e2v[i][j]]) += edge_weights[i];
                   });
    
      ParallelFor (num_edges, [&] (size_t i)
                   {
                     double vstr1 = sum_vertex_weights[e2v[i][0]];
                     double vstr2 = sum_vertex_weights[e2v[i][1]];
                   
                     edge_collapse_weights[i] = edge_weights[i] * (vstr1+vstr2) / (vstr1 * vstr2);
                   });

      // sort edges by collapse weights
    
      Array<int> indices(num_edges);
      ParallelFor (num_edges, [&] (size_t edge)
                   { indices[edge] = edge; });

      SampleSortI(edge_collapse_weights, indices);
    
      Array<size_t> invindices(num_edges);
      ParallelFor (num_edges, [&] (size_t edge)
                   {
                     invindices[indices[edge]] = edge;
                   });


      // which edges to collapse ? 
    
      Array<bool> vertex_collapse(num_vertices);
      Array<bool> edge_collapse(num_edges);
      edge_collapse = false;
      vertex_collapse = false;
    
      TableCreator<int> v2e_creator(num_vertices);
      for ( ; !v2e_creator.Done(); v2e_creator++)
        ParallelFor (num_edges, [&] (size_t e)
                     {
                       for (int j = 0; j < 2; j++)
                         v2e_creator.Add (e2v[e][j], e);
                     });
      Table<int> v2e = v2e_creator.MoveTable();
    
      ParallelFor (v2e.Size(), [&] (size_t vnr)
                   {
                     QuickSortI (invindices, v2e[vnr]);
                   }, TasksPerThread(5));
    
      // build edge dependency
      TableCreator<int> edge_dag_creator(num_edges);
      for ( ; !edge_dag_creator.Done(); edge_dag_creator++)  
        ParallelFor (v2e.Size(), [&] (size_t vnr)
                     {
                       auto vedges = v2e[vnr];
                       for (int j = 0; j+1 < vedges.Size(); j++)
                         edge_dag_creator.Add (vedges[j+1], vedges[j]);
                     }, TasksPerThread(5));
      Table<int> edge_dag = edge_dag_creator.MoveTable();
    
      RunParallelDependency (edge_dag,
                             [&] (int edgenr)
                             {
                               auto v0 = e2v[edgenr][0];
                               auto v1 = e2v[edgenr][1];
                               if (edge_collapse_weights[edgenr] >= 0.01 && !vertex_collapse[v0] && !vertex_collapse[v1])
                                 {
                                   edge_collapse[edgenr] = true;
                                   vertex_collapse[v0] = true;
                                   vertex_collapse[v1] = true;
                                 }
                             });

      // collapse the larger vertex
      vertex_collapse = false;
      for (int e = 0; e < num_edges; e++)
        if (edge_collapse[e])
          {
            auto v0 = e2v[e][0];
            auto v1 = e2v[e][1];
            vertex_collapse[max2(v0,v1)] = true;
          }
    
      
      // vertex 2 coarse vertex
      Array<size_t> v2cv(num_vertices);
      size_t num_coarse_vertices = 0;
      for (size_t i = 0; i < num_vertices; i++)
        if (!vertex_collapse[i])
          v2cv[i] = num_coarse_vertices++;
      for (size_t e = 0; e < num_edges; e++)
        if (edge_collapse[e])
          {
            auto v0 = e2v[e][0];
            auto v1 = e2v[e][1];
            if (v0 > v1) Swap (v0,v1);
            v2cv[v1] = v2cv[v0];
          }

      // edge to coarse edge
      
      Array<size_t> e2ce(num_edges);

      ParallelHashTable<INT<2>, int> coarse_edge_ht;
      
      ParallelFor (num_edges, [&] (size_t edge) 
                   {
                     auto verts = e2v[edge];
                     size_t cv1 = v2cv[e2v[edge][0]];
                     size_t cv2 = v2cv[e2v[edge][1]];

                     // only edges where both coarse vertices are different and don't
                     // collapse to ground will be coarse edges
                     if (cv1 != -1 && cv2 != -1 && cv1 != cv2) {
                       coarse_edge_ht.Do(INT<2>(cv1, cv2).Sort(), [](auto & val) { val = -1; });
                     }
                   });
      
      
      Array<int> prefixsums(coarse_edge_ht.NumBuckets());
      size_t num_coarse_edges = 0;
      for (size_t i = 0; i < coarse_edge_ht.NumBuckets(); i++)
        {
          prefixsums[i] = num_coarse_edges;
          num_coarse_edges += coarse_edge_ht.Used(i);
        }

      Array<INT<2>> coarse_e2v(num_coarse_edges);

      ParallelFor (coarse_edge_ht.NumBuckets(),
               [&] (size_t nr)
               {
                 int cnt = prefixsums[nr];
                 coarse_edge_ht.Bucket(nr).Iterate
                   ([&cnt] (INT<2> key, int & val)
                    {
                      val = cnt++;
                    });
               });


      ParallelFor (coarse_edge_ht.NumBuckets(),
                   [&] (size_t nr)
                   {               
                     coarse_edge_ht.Bucket(nr).Iterate
                       ([&coarse_e2v] (INT<2> key, int val)
                        {
                          coarse_e2v[val] = key;
                        });
                   });

      
      ParallelFor(num_edges, [&] (int edge)
                  {
                    int vertex1 = v2cv[e2v[edge][0]];
                    int vertex2 = v2cv[e2v[edge][1]];

                    if (vertex1 != -1 && vertex2 != -1 && vertex1 != vertex2) 
                      e2ce[edge] = coarse_edge_ht.Get(INT<2>(vertex1, vertex2).Sort());
                    else 
                      e2ce[edge] = -1;
                  });

      
      Array<double> coarse_edge_weights (num_coarse_edges);
      Array<double> coarse_vertex_weights (num_coarse_vertices);

      coarse_edge_weights = 0.0;
      coarse_vertex_weights = 0.0;
      
      ParallelFor(e2ce.Size(), [&] (int e)
                  {
                    if (e2ce[e] != -1) 
                      AsAtomic(coarse_edge_weights[e2ce[e]]) += edge_weights[e];
                  });
      
      ParallelFor(v2cv.Size(), [&] (int v)
                  {
                    if (v2cv[v] != -1) 
                      AsAtomic(coarse_vertex_weights[v2cv[v]]) += vertex_weights[v];
                  });
      

      // build smoother
      TableCreator<int> smoothing_blocks_creator(num_coarse_vertices);
      for ( ; !smoothing_blocks_creator.Done(); smoothing_blocks_creator++)
        ParallelFor (v2cv.Size(), [&] (size_t v)
                     {
                       if (v2cv[v] != -1 && (*freedofs)[v])
                         smoothing_blocks_creator.Add (v2cv[v], v);
                     });

      auto blocks = make_shared<Table<int>> (smoothing_blocks_creator.MoveTable());
      smoother = mat->CreateBlockJacobiPrecond(blocks);

      // build prolongation
      Array<int> nne(num_vertices);
      nne = 1;
      prolongation = make_shared<SparseMatrix<double>> (nne, num_coarse_vertices);
      for (int i = 0; i < num_vertices; i++)
        (*prolongation)(i, v2cv[i]) = 1;

      restriction = TransposeMatrix (*prolongation);
      
      auto coarsemat = mat -> Restrict (*prolongation);

      // coarse freedofs
      auto coarse_freedofs = make_shared<BitArray> (num_coarse_vertices);
      coarse_freedofs->Clear();
      ParallelFor(v2cv.Size(), [&] (int v)
                  {
                    if (v2cv[v] != -1)
                      coarse_freedofs->Set(v2cv[v]);
                  });
      

      if (num_coarse_vertices < 10)
        coarse_precond = coarsemat->InverseMatrix(coarse_freedofs);
      else
        coarse_precond = make_shared<H1AMG_Matrix> (dynamic_pointer_cast<SparseMatrixTM<SCAL>> (coarsemat), coarse_freedofs,
                                                    coarse_e2v, coarse_edge_weights, coarse_vertex_weights);
    }

    virtual int VHeight() const { return size; }
    virtual int VWidth() const { return size; }

    virtual AutoVector CreateRowVector () const override { return mat->CreateColVector(); }
    virtual AutoVector CreateColVector () const override { return mat->CreateRowVector(); }
    
    

    virtual void Mult (const BaseVector & b, BaseVector & x) const
    {
      x = 0;

      smoother->GSSmooth(x, b, smoothing_steps);
      auto residuum = b.CreateVector();
      residuum = b - (*mat) * x;

      auto coarse_residuum = coarse_precond->CreateColVector();
      coarse_residuum = *restriction * residuum;

      auto coarse_x = coarse_precond->CreateColVector();
      coarse_precond->Mult(coarse_residuum, coarse_x);
    
      x += *prolongation * coarse_x;
      smoother->GSSmoothBack (x, b, smoothing_steps);
    }
  };

  template <class SCAL>
  class H1AMG_Preconditioner : public Preconditioner
  {
    shared_ptr<BitArray> freedofs;
    shared_ptr<H1AMG_Matrix<SCAL>> mat;

    ParallelHashTable<INT<2>,double> edge_weights_ht;
    ParallelHashTable<INT<1>,double> vertex_weights_ht;
  
  public:
  
    H1AMG_Preconditioner (shared_ptr<BilinearForm> abfa, const Flags & aflags,
                          const string aname = "H1AMG_cprecond")
      : Preconditioner (abfa, aflags, aname)
    {
      cout << "Create H1AMG" << endl;
    }

    H1AMG_Preconditioner (const PDE & pde, const Flags & aflags, const string & aname)
      : H1AMG_Preconditioner (pde.GetBilinearForm (aflags.GetStringFlag ("bilinearform")), 
                              aflags, aname)
    { ; }

  
    virtual void InitLevel (shared_ptr<BitArray> _freedofs) 
    {
      freedofs = _freedofs;
      cout << "init level" << endl;
    }

    virtual void FinalizeLevel (const BaseMatrix * matrix)
    {
      cout << "Finalize" << endl;
      auto smat = dynamic_pointer_cast<SparseMatrixTM<SCAL>> (const_cast<BaseMatrix*>(matrix)->shared_from_this());

      size_t num_vertices = matrix->Height();
      size_t num_edges = edge_weights_ht.Used();

      Array<double> edge_weights (num_edges);
      Array<INT<2> > e2v (num_edges);
    
      edge_weights_ht.IterateParallel
        ([&edge_weights,&e2v] (size_t i, INT<2> key, double weight)
         {
           edge_weights[i] = weight;
           e2v[i] = key;
         });

    
      Array<double> vertex_weights(num_vertices);
      vertex_weights_ht.IterateParallel
        ([&vertex_weights] (size_t i, INT<1> key, double weight)
         {
           vertex_weights[i] = weight;
         });
      mat = make_shared<H1AMG_Matrix<double>> (smat, freedofs, e2v, edge_weights, vertex_weights);
    }


    virtual void AddElementMatrix (FlatArray<int> dnums,
                                   const FlatMatrix<SCAL> & elmat,
                                   ElementId id, 
                                   LocalHeap & lh)
    {
      // vertex weights
      BitArray used(dnums.Size());
      FlatMatrix<SCAL> schur_vertex(1,1,lh);
      for (size_t i = 0; i < dnums.Size(); i++)
        {
          used.Clear();
          used.Set(i);
          CalcSchurComplement(elmat, schur_vertex, used, lh);
          double weight = schur_vertex(0,0);
          vertex_weights_ht.Do(INT<1>(dnums[i]), [weight] (auto & v) { v += weight; });
        }

      // edge weights
      FlatMatrix<SCAL> schur_edge(2,2,lh);
      for (size_t i = 0; i < dnums.Size(); i++)
        for (size_t j = 0; j < i; j++)
          {
            used.Clear();
            used.Set(i);
            used.Set(j);
            CalcSchurComplement(elmat, schur_edge, used, lh);
            double weight = schur_edge(0,0);
            edge_weights_ht.Do(INT<2>(dnums[j], dnums[i]), [weight] (auto & v) { v += weight; });
          }
    }

    virtual void Update () { ; } 

    virtual const BaseMatrix & GetMatrix() const
    {
      return *mat;
    }

  
  };


  static RegisterPreconditioner<H1AMG_Preconditioner<double> > initpre ("h1amg");
}

