#include <comp.hpp>
#include <multigrid.hpp>

#include <parallelngs.hpp>


namespace ngcomp
{
 
  // dummy function header 
  void CalcEigenSystem (FlatMatrix<Complex> & elmat, 
                        FlatVector<Complex> & lami, 
                        FlatMatrix<Complex> & evecs)
  { ; }


  template <typename SCAL>
  void CalcSchur (FlatMatrix<SCAL> a, 
                  FlatMatrix<SCAL> schur,
                  FlatArray<int> keep, FlatArray<int> eliminate)
  {
    // via Cholesky
    // A = L D L^t
    int nk = keep.Size();
    int ne = eliminate.Size();
    int n = nk+ne;
    Matrix<SCAL> temp(n);

    temp.Rows(0,ne).Cols(0,ne) = a.Rows(eliminate).Cols(eliminate);
    temp.Rows(0,ne).Cols(ne,n) = a.Rows(eliminate).Cols(keep);
    temp.Rows(ne,n).Cols(0,ne) = a.Rows(keep).Cols(eliminate);
    temp.Rows(ne,n).Cols(ne,n) = a.Rows(keep).Cols(keep);

    for (int i = 0; i < ne; i++)
      {
        SCAL d = temp(i,i);
        SCAL id = 1.0/d;

        for (int j = i+1; j < n; j++)
          for (int k = i+1; k < n; k++)
            temp(j,k) -= temp(j,i) * id * temp(i,k);
        for (int j = i+1; j < n; j++)
          temp(j,i) *= id;
      }
    schur = temp.Rows(ne,n).Cols(ne,n);
  }


  BilinearForm :: 
  BilinearForm (shared_ptr<FESpace> afespace,
                const string & aname,
                const Flags & flags)
    : NGS_Object(afespace->GetMeshAccess(), aname), fespace(afespace)
  {
    fespace2 = NULL;

    multilevel = true;
    symmetric = true;

    // low_order_bilinear_form = NULL;
    linearform = NULL;


    SetGalerkin( flags.GetDefineFlag( "project" ) );
    SetNonAssemble (flags.GetDefineFlag ("nonassemble"));
    SetDiagonal (flags.GetDefineFlag ("diagonal"));
    if (flags.GetDefineFlag ("nonsym"))  SetSymmetric (0);
    if (flags.GetDefineFlag ("nonmultilevel")) SetMultiLevel (0);
    SetHermitean (flags.GetDefineFlag ("hermitean"));
    SetUnusedDiag (flags.GetNumFlag ("unuseddiag",0.0));
    SetEpsRegularization (flags.GetNumFlag ("regularization",0));
  
    SetPrint (flags.GetDefineFlag ("print"));
    SetPrintElmat (flags.GetDefineFlag ("printelmat"));
    SetElmatEigenValues (flags.GetDefineFlag ("elmatev")); 
    SetTiming (flags.GetDefineFlag ("timing"));
    SetEliminateInternal (flags.GetDefineFlag ("eliminate_internal"));
    SetKeepInternal (eliminate_internal && 
                     !flags.GetDefineFlag ("nokeep_internal"));
    SetStoreInner (flags.GetDefineFlag ("store_inner"));
    precompute = flags.GetDefineFlag ("precompute");
    checksum = flags.GetDefineFlag ("checksum");
    spd = flags.GetDefineFlag ("spd");
    if (spd) symmetric = true;
  }


  BilinearForm :: 
  BilinearForm (shared_ptr<FESpace> afespace,
                shared_ptr<FESpace> afespace2, const string & aname,
                const Flags & flags)
    : NGS_Object(afespace->GetMeshAccess(), aname), fespace(afespace), fespace2(afespace2)
  {
    multilevel = true;
    galerkin = false;
    symmetric = false;
    spd = false;
    hermitean = false;

    SetEpsRegularization (0);

    // low_order_bilinear_form = NULL;
    linearform = NULL;

    timing = false;
    print = false;
    printelmat = false;
    elmat_ev = false;
    eliminate_internal = false;
    // keep_internal = false;


    SetGalerkin( flags.GetDefineFlag( "project" ) );
    SetNonAssemble (flags.GetDefineFlag ("nonassemble"));
    SetDiagonal (flags.GetDefineFlag ("diagonal"));
    if (flags.GetDefineFlag ("nonsym"))  SetSymmetric (0);
    if (flags.GetDefineFlag ("nonmultilevel")) SetMultiLevel (0);
    SetHermitean (flags.GetDefineFlag ("hermitean"));
    SetUnusedDiag (flags.GetNumFlag ("unuseddiag",0.0));
  
    SetPrint (flags.GetDefineFlag ("print"));
    SetPrintElmat (flags.GetDefineFlag ("printelmat"));
    SetElmatEigenValues (flags.GetDefineFlag ("elmatev"));

    if (flags.GetDefineFlag ("timing")) SetTiming (1);
    if (flags.GetDefineFlag ("eliminate_internal")) SetEliminateInternal (1);
    SetKeepInternal (eliminate_internal && 
                     !flags.GetDefineFlag ("nokeep_internal"));
    if (flags.GetDefineFlag ("store_inner")) SetStoreInner (1);

    precompute = flags.GetDefineFlag ("precompute");
    checksum = flags.GetDefineFlag ("checksum");
  }


  
  void BilinearForm :: AddIntegrator (shared_ptr<BilinearFormIntegrator> bfi)
  {
    parts.Append (bfi);
    if (low_order_bilinear_form)
      low_order_bilinear_form -> AddIntegrator (parts.Last());
  }


  /*
    void BilinearForm :: AddIndependentIntegrator (BilinearFormIntegrator * bfi,
    const int master_surface,
    const int slave,
    const bool deletable)
    {
    independent_parts.Append (bfi);
    independent_parts_deletable.Append(deletable);
    Vec<2,int> indices(master_surface,slave); 
    independent_meshindex.Append(indices);
    if (low_order_bilinear_form)
    low_order_bilinear_form -> AddIndependentIntegrator (independent_parts.Last(),
    master_surface,slave,
    deletable);
    }
  */

  BilinearForm :: ~BilinearForm ()
  {
    ; // delete low_order_bilinear_form;
  }

  void BilinearForm :: SetPrint (bool ap)
  { 
    print = ap; 
    if (low_order_bilinear_form)
      low_order_bilinear_form -> SetPrint (ap);
  }

  void BilinearForm :: SetPrintElmat (bool ap)
  { 
    printelmat = ap; 
    if (low_order_bilinear_form)
      low_order_bilinear_form -> SetPrintElmat (ap);
  }

  void BilinearForm ::  SetElmatEigenValues (bool ee)
  { 
    elmat_ev = ee;
    if (low_order_bilinear_form)
      low_order_bilinear_form -> SetElmatEigenValues (ee);
  }



  MatrixGraph * BilinearForm :: GetGraph (int level, bool symmetric)
  {
    static Timer timer ("BilinearForm::GetGraph");
    RegionTimer reg (timer);

    int ndof = fespace->GetNDof();
    int ne = ma->GetNE();
    int nse = ma->GetNSE();
    int nf = ma->GetNFacets();
    const Array<SpecialElement*> & specialelements = fespace->GetSpecialElements();
    int nspe = specialelements.Size();

    Array<int> dnums;
    Array<int> fnums; //facets of one element
    Array<int> elnums; //elements neighbouring one facet
    Array<int> nbelems; //neighbour elements


    int maxind = ne + nse + specialelements.Size();
    if (fespace->UsesDGCoupling()) maxind += nf;

    TableCreator<int> creator(maxind);
    for ( ; !creator.Done(); creator++)
      {

#pragma omp parallel
        {
          Array<int> dnums;

#pragma omp for
          for (int i = 0; i < ne; i++)
            {
              if (!fespace->DefinedOn (ma->GetElIndex(i))) continue;

              if (eliminate_internal)
                fespace->GetDofNrs (i, dnums, EXTERNAL_DOF);
              else
                fespace->GetDofNrs (i, dnums);
              
              for (int d : dnums)
                if (d != -1) creator.Add (i, d);
            }

#pragma omp for
          for (int i = 0; i < nse; i++)
            {
              if (!fespace->DefinedOnBoundary (ma->GetSElIndex(i))) continue;
              
              fespace->GetSDofNrs (i, dnums);
              for (int d : dnums)
                if (d != -1) creator.Add (ne+i, d);
            }
        }

        for (int i = 0; i < specialelements.Size(); i++)
          {
            specialelements[i]->GetDofNrs (dnums);
            for (int d : dnums)
              if (d != -1) creator.Add (ne+nse+i, d);
          }

        if (fespace->UsesDGCoupling())
          //add dofs of neighbour elements as well
          for (int i = 0; i < nf; i++)
            {
              nbelems.SetSize(0);
              ma->GetFacetElements(i,elnums);
              for (int k=0; k<elnums.Size(); k++)
                nbelems.Append(elnums[k]);

              for (int k=0;k<nbelems.Size();k++){
                int elnr=nbelems[k];
                if (!fespace->DefinedOn (ma->GetElIndex(elnr))) continue;
                fespace->GetDofNrs (elnr, dnums);
                for (int j = 0; j < dnums.Size(); j++)
                  if (dnums[j] != -1)
                    creator.Add (ne+nse+nspe+i, dnums[j]);
              }
            }

      }
    
    MatrixGraph * graph;

    if (!fespace2)
      graph = new MatrixGraph (ndof, *creator.GetTable(), *creator.GetTable(), symmetric);
    else
      {
        TableCreator<int> creator2(maxind);
        for ( ; !creator2.Done(); creator2++)
          {
            for (int i = 0; i < ne; i++)
              {
                if (!fespace2->DefinedOn (ma->GetElIndex(i))) continue;
                
                if (eliminate_internal)
                  fespace2->GetDofNrs (i, dnums, EXTERNAL_DOF);
                else
                  fespace2->GetDofNrs (i, dnums);
                
                for (int j = 0; j < dnums.Size(); j++)
                  if (dnums[j] != -1)
                    creator2.Add (i, dnums[j]);
              }
        
            for (int i = 0; i < nse; i++)
              {
                if (!fespace2->DefinedOnBoundary (ma->GetSElIndex(i))) continue;
            
                fespace2->GetSDofNrs (i, dnums);
                for (int j = 0; j < dnums.Size(); j++)
                  if (dnums[j] != -1)
                    creator2.Add (ne+i, dnums[j]);
              }

            /*
              // just not tested ...
            for (int i = 0; i < specialelements.Size(); i++)
              {
                specialelements[i]->GetDofNrs (dnums);
            
                for (int j = 0; j < dnums.Size(); j++)
                  if (dnums[j] != -1)
                    creator2.Add (ne+nse+i, dnums[j]);
              }

            if (fespace2->UsesDGCoupling())
              //add dofs of neighbour elements as well
              for (int i = 0; i < nf; i++)
                {
                  nbelems.SetSize(0);
                  ma->GetFacetElements(i,elnums);
                  for (int k=0; k<elnums.Size(); k++)
                    nbelems.Append(elnums[k]);

                  for (int k=0;k<nbelems.Size();k++){
                    int elnr=nbelems[k];
                    if (!fespace2->DefinedOn (ma->GetElIndex(elnr))) continue;
                    fespace2->GetDofNrs (elnr, dnums);
                    for (int j = 0; j < dnums.Size(); j++)
                      if (dnums[j] != -1)
                        creator2.Add (ne+nse+nspe+i, dnums[j]);
                  }
                }
            */
          }

        graph = new MatrixGraph (fespace2->GetNDof(), *creator2.GetTable(), *creator.GetTable(), symmetric);        
        delete creator2.GetTable();
      }

    delete creator.GetTable();
    graph -> FindSameNZE();
    return graph;
  }






  void BilinearForm :: Assemble (LocalHeap & lh)
  {
    if (mats.Size() == ma->GetNLevels())
      return;


    if (nonassemble)
      {
        mats.Append (make_shared<BilinearFormApplication> (shared_ptr<BilinearForm>(this, NOOP_Deleter))); 
      
        if (precompute)
          {
            precomputed_data.SetSize(0);

            Array<int> dnums;
            int ne = ma->GetNE();
            int nse = ma->GetNSE();
      
            LocalHeap lh (20000000, "biform - assemble");
            
            int hasbound = 0;
            int hasinner = 0;
            
            for (int j = 0; j < NumIntegrators(); j++)
              {
                const BilinearFormIntegrator & bfi = *GetIntegrator(j);
                if (bfi.BoundaryForm())
                  hasbound = 1;
                else
                  hasinner = 1;
              }
            
            
            if (hasinner)
              for (int i = 0; i < ne; i++)
                {
                  lh.CleanUp();
                  
                  if (!fespace->DefinedOn (ma->GetElIndex (i))) continue;
          
                  const FiniteElement & fel = fespace->GetFE (i, lh);
                  ElementTransformation & eltrans = ma->GetTrafo (i, 0, lh);
                  fespace->GetDofNrs (i, dnums);
          
                  for (int j = 0; j < NumIntegrators(); j++)
                    {
                      const BilinearFormIntegrator & bfi = *parts[j];
                      
                      if (bfi.BoundaryForm()) continue;
                      if (!bfi.DefinedOn (ma->GetElIndex (i))) continue;
                      
                      precomputed_data.Append (bfi.PrecomputeData (fel, eltrans, lh));
                    }
                }


            if (hasbound)
              for (int i = 0; i < nse; i++)
                {
                  lh.CleanUp();
                  
                  const FiniteElement & fel = fespace->GetSFE (i, lh);
                  ElementTransformation & eltrans = ma->GetTrafo (i, 1, lh);

                  fespace->GetSDofNrs (i, dnums);
          
                  for (int j = 0; j < NumIntegrators(); j++)
                    {
                      const BilinearFormIntegrator & bfi = *parts[j];
                      
                      if (!bfi.BoundaryForm()) continue;
                      precomputed_data.Append (bfi.PrecomputeData (fel, eltrans, lh));
                    }
                }
          }


        
        if (timing)
          {
            Timer timer("bftimer");

            auto vecf = mats.Last()->CreateVector();
            auto vecu = mats.Last()->CreateVector();
          
            *vecu = 1;
            do
              {
                timer.Start();
                vecf = (*mats.Last()) * vecu;
                timer.Stop();
              }
            while (timer.GetTime() < 2.0);
          
            cout << " 1 application takes " << timer.GetTime() / timer.GetCounts()
                 << " seconds" << endl;
          }


        return;
      }
    
    try
      {

        if (low_order_bilinear_form)
          low_order_bilinear_form->Assemble(lh);

      }
    catch (Exception & e)
      {
        e.Append (string ("\nthrown by Do loworder ") +
                  string (GetName()));
        throw e;
      }

    
    try
      {
        AllocateMatrix ();
      }
    catch (exception & e)
      {
        throw Exception (e.what() + 
                         string ("\nthrown by allocate matrix ") +
                         string (GetName()));
      }
    catch (Exception & e)
      {
        e.Append (string ("\nthrown by allocate matrix ") +
                  string (GetName()));
        throw;
      }


    DoAssemble(lh);


    if (timing)
      {
        double time;
        double starttime = WallTime();
      
        auto vecf = mats.Last()->CreateVector();
        auto vecu = mats.Last()->CreateVector();
      
        vecu = 1;
        int steps = 0;
        do
          {
            vecf = (*mats.Last()) * vecu;
            steps++;
            time = WallTime()-starttime;
          }
        while (time < 2.0);
          
        cout << " 1 application takes "
             << time / steps
             << " seconds" << endl;

        int nze = dynamic_cast<const BaseSparseMatrix &> (*mats.Last()) . NZE();
        cout << "NZE = " << nze << ", MFLOP = " << double (nze * steps) / time * 1e-6 << endl;
        cout << "type = " << typeid(*mats.Last()).name() << endl;
      }

    if (galerkin)
      GalerkinProjection();
  }


  void BilinearForm :: ReAssemble (LocalHeap & lh, bool reallocate)
  {
    if (nonassemble) return;

    if (low_order_bilinear_form)
      low_order_bilinear_form->ReAssemble(lh);

    if (mats.Size() < ma->GetNLevels())
      {
        Assemble(lh);
        return;
      }


    if (reallocate)
      {
        // delete mats.Last();
        mats.DeleteLast();

        Assemble(lh);
        return;
      }

    GetMatrix() = 0.0;
    DoAssemble(lh);

    if (galerkin)
      GalerkinProjection();
  }



  void BilinearForm :: PrintReport (ostream & ost) const
  {
    ost << "on space " << GetFESpace()->GetName() << endl
        << "symmetric   = " << symmetric << endl
        << "multilevel  = " << multilevel << endl
        << "nonassemble = " << nonassemble << endl
        << "printelmat = " << printelmat << endl
        << "elmatev    = " << elmat_ev << endl
        << "eliminate_internal = " << eliminate_internal << endl
        << "keep_internal = " << keep_internal << endl
        << "store_inner = " << store_inner << endl
        << "integrators: " << endl;
  
    for (int i = 0; i < parts.Size(); i++)
      ost << "  " << parts[i]->Name() << endl;
  }


  void BilinearForm :: MemoryUsage (Array<MemoryUsageStruct*> & mu) const
  {
    if (low_order_bilinear_form)
      low_order_bilinear_form -> MemoryUsage (mu);
  
    int olds = mu.Size();

    for (int i = 0; i < mats.Size(); i++)
      if (mats[i]) mats[i]->MemoryUsage (mu);

    for (int i = olds; i < mu.Size(); i++)
      mu[i]->AddName (string(" bf ")+GetName());
  }


  template <class SCAL>
  S_BilinearForm<SCAL> :: ~S_BilinearForm()
  {
    delete harmonicext;
    delete harmonicexttrans;
    delete innersolve;
    delete innermatrix;
  }




  template <class SCAL>
  void S_BilinearForm<SCAL> :: DoAssemble (LocalHeap & clh)
  {
    static Timer mattimer("Matrix assembling");

    static Timer mattimer1("Matrix assembling part 1");
    static Timer mattimerclear("Matrix assembling - clear matrix");
    static Timer mattimer1a("Matrix assembling part 1a");
    static Timer mattimer2("Matrix assembling part 2");
    static Timer mattimer_vol("Matrix assembling vol");
    static Timer mattimer_bound("Matrix assembling bound");

    static Timer timer1 ("Matrix assembling - 1", 3);
    static Timer timer2 ("Matrix assembling - 2", 3);
    static Timer timer3 ("Matrix assembling - 3", 3);
    
    static Timer timerb1 ("Matrix assembling bound - 1", 3);
    static Timer timerb2 ("Matrix assembling bound - 2", 3);
    static Timer timerb3 ("Matrix assembling bound - 3", 3);

    RegionTimer reg (mattimer);

    mattimer1.Start();
    // check if integrators fit to space
    for (int i = 0; i < NumIntegrators(); i++)
      if (parts[i] -> BoundaryForm())
        {
          for (int j = 0; j < ma->GetNSE(); j++)
            {
              HeapReset hr(clh);
              if (parts[i] -> DefinedOn (ma->GetSElIndex(j)))
                parts[i] -> CheckElement (fespace->GetSFE(j, clh));
            }
        }
      else if (parts[i] -> SkeletonForm() && !parts[i] -> BoundaryForm()) 
        { 
          if (!fespace->UsesDGCoupling()) 
            throw Exception("FESpace is not suitable for those integrators (try -dgjumps)");
          //TODO: Check each facet for the neighbouring elements - necessary?
        }
      else
        {
#ifdef FULLSPEED
          int checkels = min (1, ma->GetNE());
#else
          int checkels = ma->GetNE();
#endif
          for (int j = 0; j < checkels; j++)
            {
              HeapReset hr(clh);
              if (parts[i] -> DefinedOn (ma->GetElIndex(j)))
                parts[i] -> CheckElement (fespace->GetFE(j, clh));
            }
        }
    mattimer1.Stop();    
    
    try
      {
        if (!MixedSpaces())
	  
          {
	    mattimer1a.Start();
            ma->PushStatus ("Assemble Matrix");
 
            int ndof = fespace->GetNDof();
            Array<bool> useddof(ndof);
            useddof = false;

            int ne = ma->GetNE();
            int nse = ma->GetNSE();
            int nf = ma->GetNFacets();

	    mattimerclear.Start();
            BaseMatrix & mat = GetMatrix();
            mat = 0.0;
	    mattimerclear.Stop();

            bool hasbound = false;
            bool hasinner = false;
            bool hasskeletonbound = false;
            bool hasskeletoninner = false;
 
            for (int j = 0; j < NumIntegrators(); j++)
              {
                if (parts[j] -> BoundaryForm())
                  if (parts[j] -> SkeletonForm())
                    hasskeletonbound = true;
                  else
                    hasbound = true;
                else
                  if (parts[j] -> SkeletonForm())
                    hasskeletoninner = true;
                  else
                    hasinner = true;
              }

            if (print)
              {
                *testout << " BILINEARFORM TEST:" << endl;
                *testout << " hasinner = " << hasinner << endl;
                *testout << " hasouter = " << hasbound << endl;
                *testout << " hasskeletoninner = " << hasskeletoninner << endl;
                *testout << " hasskeletonouter = " << hasskeletonbound << endl;
              }

            int nrcases = 0;
            int loopsteps = 0;
            if (hasinner) {nrcases++; loopsteps+=ne;}
            if (hasbound) {nrcases++; loopsteps+=nse;}
            if (hasskeletoninner) {nrcases++; loopsteps+=nf;}
            if (hasskeletonbound) {nrcases++; loopsteps+=nse;}
            if (fespace->specialelements.Size()>0) {nrcases++; loopsteps+=fespace->specialelements.Size();}
            int gcnt = 0; //global count (for all cases)

            for (int j = 0; j < preconditioners.Size(); j++)
              preconditioners[j] -> InitLevel();

	    mattimer1a.Stop();

            if (hasinner && !diagonal)
              {
		RegionTimer reg(mattimer_vol);
                ProgressOutput progress (ma, "assemble element", ma->GetNE());


                if (eliminate_internal&&keep_internal)
                  {
                    delete harmonicext;
                    delete harmonicexttrans;
                    delete innersolve;
                    delete innermatrix;

                    harmonicext = new ElementByElementMatrix<SCAL>(ndof, ne);
                    if (!symmetric)
                      harmonicexttrans = new ElementByElementMatrix<SCAL>(ndof, ne);
                    else
                      harmonicexttrans = new Transpose(*harmonicext);
                    innersolve = new ElementByElementMatrix<SCAL>(ndof, ne);
                    if (store_inner)
                      innermatrix = new ElementByElementMatrix<SCAL>(ndof, ne);
                  }

                
                IterateElements 
                  (*fespace, VOL, clh,  [&] (FESpace::Element el, LocalHeap & lh)
                   {
                     if (elmat_ev) 
                       *testout << " Assemble Element " << el.Nr() << endl;  
                          
                     progress.Update ();
                     
                     timer1.Start();
                     
                     const FiniteElement & fel = fespace->GetFE (el, lh);
                     const ElementTransformation & eltrans = ma->GetTrafo (el, lh);
                     FlatArray<int> dnums = el.Dofs();

                     if (fel.GetNDof() != dnums.Size())
                       {
                         cout << "fel:GetNDof() = " << fel.GetNDof() << endl;
                         cout << "dnums.Size() = " << dnums.Size() << endl;

			 *testout << "Info from finite element: " << endl;
			 fel.Print (*testout);
                         (*testout) << "fel:GetNDof() = " << fel.GetNDof() << endl;
                         (*testout) << "dnums.Size() = " << dnums.Size() << endl;
                         (*testout) << "dnums = " << dnums << endl;
                         throw Exception ( "Inconsistent number of degrees of freedom " );
                       }

                     int elmat_size = dnums.Size()*fespace->GetDimension();
                     FlatMatrix<SCAL> sum_elmat(elmat_size, lh);
                     sum_elmat = 0;

                     timer1.Stop();
                     timer2.Start();

                     for (int j = 0; j < NumIntegrators(); j++)
                       {
                         HeapReset hr (lh);
                         BilinearFormIntegrator & bfi = *parts[j];
                      
                         if (!bfi.VolumeForm()) continue;
                         // if (!bfi.DefinedOn (ma->GetElIndex (ei))) continue;
                         if (!bfi.DefinedOn (el.GetIndex())) continue;
                      
                         FlatMatrix<SCAL> elmat(elmat_size, lh);

                         try
                           {
                             static Timer elementtimer ("Element matrix integration", 2);
                             elementtimer.Start();
                             if (!diagonal)
                               bfi.CalcElementMatrix (fel, eltrans, elmat, lh);
                             else
                               {
                                 FlatVector<double> diag(fel.GetNDof(), lh);
                                 bfi.CalcElementMatrixDiag (fel, eltrans, diag, lh);
                                 elmat = 0.0;
                                 elmat.Diag() = diag;
                               }

                             elementtimer.Stop();

                             if (printelmat)
                               {
                                 testout->precision(8);
                                 (*testout) << "elnum = " << el.Nr() << endl;
                                 (*testout) << "eltype = " << fel.ElementType() << endl;
                                 (*testout) << "integrator = " << bfi.Name() << endl;
                                 (*testout) << "dnums = " << endl << dnums << endl;
                                 (*testout) << "elmat = " << endl << elmat << endl;
                               }

                             if (elmat_ev)
                               LapackEigenSystem(elmat, lh);
                           }
                         catch (Exception & e)
                           {
                             e.Append (string("in Assemble Element Matrix, bfi = ") + 
                                       bfi.Name() + string("\n"));

                             throw;
                           }
                         catch (exception & e)
                           {
                             throw (Exception (string(e.what()) +
                                               string("in Assemble Element Matrix, bfi = ") + 
                                               bfi.Name() + string("\n")));
                           }
                      
                         sum_elmat += elmat;
                       }

                     timer2.Stop();
                     timer3.Start();
                     fespace->TransformMat (el.Nr(), false, sum_elmat, TRANSFORM_MAT_LEFT_RIGHT);


                     if (elmat_ev)
                       {
                         (*testout) << "sum matrix:" << endl;
                         LapackEigenSystem(sum_elmat, lh);
                       }

                     int i = el.Nr();
                     if (eliminate_internal)
                       {
                         static Timer statcondtimer("static condensation", 1);
                         RegionTimer regstat (statcondtimer);

                         Array<int> idofs1(dnums.Size(), lh);

                         fespace->GetDofNrs (i, idofs1, LOCAL_DOF);
                         for (int j = 0; j < idofs1.Size(); j++)
                           idofs1[j] = dnums.Pos(idofs1[j]);

                         if (printelmat) 
                           {
                             *testout << "eliminate internal" << endl;
                             *testout << "idofs1 = " << idofs1 << endl;
                           }
                              
                         if (idofs1.Size())
                           {
                             HeapReset hr (lh);
                             
                             int size = sum_elmat.Height();
                             int dim = size / dnums.Size();

                             int sizei = dim * idofs1.Size();
                             int sizeo = size - sizei;

                             FlatArray<int> idofs (sizei, lh);
                             FlatArray<int> odofs (sizeo, lh);

                             for (int j = 0, k = 0; j < idofs1.Size(); j++)
                               for (int jj = 0; jj < dim; jj++)
                                 idofs[k++] = dim*idofs1[j]+jj;

                             for (int j = 0, k = 0; j < size; j++)
                               if (!idofs.Contains(j))
                                 odofs[k++] = j;

                             if (printelmat)
                               {
                                 (*testout) << "idofs = " << endl << idofs << endl;
                                 (*testout) << "odofs = " << endl << odofs << endl;
                               }


                             FlatMatrix<SCAL> 
                               a = sum_elmat.Rows(odofs).Cols(odofs) | lh,
                               b = sum_elmat.Rows(odofs).Cols(idofs) | lh,
                               c = Trans(sum_elmat.Rows(idofs).Cols(odofs)) | lh,
                               d = sum_elmat.Rows(idofs).Cols(idofs) | lh;


                             statcondtimer.AddFlops (double(sizei)*sizei*sizei/3);  // LU fact
                             statcondtimer.AddFlops (double(sizei)*sizei*sizeo);  
                             statcondtimer.AddFlops (double(sizei)*sizeo*sizeo);  
                                  
                             // A := A - B D^{-1} C^T
                             // new Versions, July 07
                             if (!keep_internal) 
                               {
                                 LapackAInvBt (d, b);    // b <--- b d^-1
                                 LapackMultAddABt (b, c, -1, a);                                 
                               }
                             else
                               {
				 Array<int> idnums1(dnums.Size(), lh), 
				   ednums1(dnums.Size(), lh);
                                 fespace->GetDofNrs(i,idnums1,LOCAL_DOF);
                                 fespace->GetDofNrs(i,ednums1,EXTERNAL_DOF);
				 
				 Array<int> idnums(dim*idnums1.Size(), lh);
				 Array<int> ednums(dim*ednums1.Size(), lh);
				 idnums.SetSize(0); 
				 ednums.SetSize(0);
                                 for (int j = 0; j < idnums1.Size(); j++)
                                   idnums += dim*IntRange(idnums1[j], idnums1[j]+1);
                                 for (int j = 0; j < ednums1.Size(); j++)
                                   ednums += dim * IntRange(ednums1[j], ednums1[j]+1);
                                      
                                 if (store_inner)
                                   innermatrix ->AddElementMatrix(i,idnums,idnums,d);
                                 

                                 /*
                                 Matrix<SCAL> hd = d;
                                 Vector<SCAL> diag(d.Height());
                                 for (int i = 0; i < d.Height(); i++)
                                   diag(i) = sqrt(fabs(hd(i,i)));
                                 for (int i = 0; i < d.Height(); i++)
                                   {
                                     hd.Row(i) *= 1.0/diag(i);
                                     hd.Col(i) *= 1.0/diag(i);
                                   }
                                 Vector<SCAL> lam(d.Height());
                                 Matrix<SCAL> evecs(d.Height());
                                 CalcEigenSystem (hd, lam, evecs);
                                 cout << "lam = " << lam << endl;
                                 */
                                 LapackInverse (d);
                                 FlatMatrix<SCAL> he (sizei, sizeo, lh);
                                 he = 0.0;
                                 he -= d * Trans(c) | Lapack;
                                 harmonicext ->AddElementMatrix(i,idnums,ednums,he);
                                 if (!symmetric)
                                   {
                                     FlatMatrix<SCAL> het (sizeo, sizei, lh);
                                     het = 0.0;
                                     LapackMultAddAB (b, d, -1, het);
                                     //  het = -1.0 * b * d;
                                     static_cast<ElementByElementMatrix<SCAL>*>(harmonicexttrans)
                                       ->AddElementMatrix(i,ednums,idnums,het);
                                   }
                                 
                                 innersolve ->AddElementMatrix(i,idnums,idnums,d);

                                 LapackMultAddAB (b, he, 1.0, a);
 
                                 if (spd)
                                   {
                                     // *testout << "schur orig = " << endl << a << endl;
                                     
                                     Matrix<SCAL> schur(odofs.Size());
                                     CalcSchur (sum_elmat, schur, odofs, idofs);
                                     
                                     // *testout << "new schur = " << endl << schur << endl;
                                     a = schur;
                                   }
                               }                             

                             if (printelmat) 
                               {
                                 testout->precision(8);
                                 (*testout) << "Schur elmat = " << endl << a << endl;
                               }
                         
                             if (elmat_ev)
                               {
                                 testout->precision(8);
                                
                                 (*testout) << "EV of Schur complement:" << endl;
                                 LapackEigenSystem(a, lh);
                               }
                                  
                             sum_elmat.Rows(odofs).Cols(odofs) = a;
                             if (linearform && (!keep_internal))
                               {
                                      
                                 FlatVector<SCAL> elvec (size, lh);
                                 linearform -> GetVector().GetIndirect (dnums, elvec);
                                 FlatVector<SCAL> hfi(sizei, lh);
                                 FlatVector<SCAL> hfo(sizeo, lh);
                                      
                                 hfi = elvec(idofs);
                                 hfo = b * hfi;
                                 elvec(odofs) -= hfo;

                                 linearform->GetVector().SetIndirect (dnums, elvec);
                               }

                             for (int k = 0; k < idofs1.Size(); k++)
                               dnums[idofs1[k]] = -1;
                           }
                       }

                     if (printelmat)
                       *testout<< "elem " << i << ", elmat = " << endl << sum_elmat << endl;

                     AddElementMatrix (dnums, dnums, sum_elmat, el, lh);
                          
                     for (int j = 0; j < preconditioners.Size(); j++)
                       preconditioners[j] -> 
                         AddElementMatrix (dnums, sum_elmat, true, i, lh);

                     for (int j = 0; j < dnums.Size(); j++)
                       if (dnums[j] != -1)
                         useddof[dnums[j]] = true;

                     timer3.Stop();
                   });

                progress.Done();
                
                /*
                if (linearform && keep_internal)
                  {
                    cout << IM(3) << "\rmodifying condensated rhs";

                    linearform -> GetVector() += 
                      GetHarmonicExtensionTrans() * linearform -> GetVector();

                    cout << IM(3) << "\t done" << endl;
                  }
                */

                gcnt += ne;
              }


            if (hasinner && diagonal)
              { 
                double prevtime = WallTime();
                Array<int> dnums;
                void * heapp = clh.GetPointer();
                for (int i = 0; i < ne; i++)
                  {
                    gcnt++;

                    if (clock()-prevtime > 0.1 * CLOCKS_PER_SEC)
                      {
                        cout << IM(3) << "\rassemble element " << i << "/" << ne << flush;
                        ma->SetThreadPercentage ( 100.0*gcnt / (loopsteps) );
                        prevtime = clock();
                      }
                
                    clh.CleanUp(heapp);
                  
                    if (!fespace->DefinedOn (ma->GetElIndex (i))) continue;
                  
                    const FiniteElement & fel = fespace->GetFE (i, clh);
                    ElementTransformation & eltrans = ma->GetTrafo (i, VOL, clh);
                    fespace->GetDofNrs (i, dnums);
                  
                    FlatVector<SCAL> sum_diag(dnums.Size()*fespace->GetDimension(), clh);
                    sum_diag = 0;
                    for (int j = 0; j < NumIntegrators(); j++)
                      {
                        const BilinearFormIntegrator & bfi = *parts[j];
                        if (bfi.SkeletonForm()) continue;
                        if (bfi.BoundaryForm()) continue;
                        if (!bfi.DefinedOn (ma->GetElIndex (i))) continue;
                      
                        FlatVector<double> diag;
                        try
                          {
                            bfi.CalcElementMatrixDiag (fel, eltrans, diag, clh);
                          
                            if (printelmat) //  || fel.ElementType() == ET_TET)
                              {
                                testout->precision(8);
                                (*testout) << "elnum= " << i << endl;
                                (*testout) << "integrator " << bfi.Name() << endl;
                                (*testout) << "dnums = " << endl << dnums << endl;
                                (*testout) << "diag-elmat = " << endl << diag << endl;
                              }
                          }
                        catch (Exception & e)
                          {
                            e.Append (string("in Assemble Element Matrix, bfi = ") + 
                                      bfi.Name() + string("\n"));
                            throw;
                          }
                        catch (exception & e)
                          {
                            throw (Exception (string(e.what()) +
                                              string("in Assemble Element Matrix, bfi = ") + 
                                              bfi.Name() + string("\n")));
                          }
                      
                        sum_diag += diag;
                      }


                    AddDiagElementMatrix (dnums, sum_diag, 1, i, clh);

                    for (int j = 0; j < dnums.Size(); j++)
                      if (dnums[j] != -1)
                        useddof[dnums[j]] = true;
                  }
                clh.CleanUp(heapp);
                cout << IM(3) << "\rassemble element " << ne << "/" << ne << endl;
              }

            if (hasbound)
              {
		RegionTimer reg(mattimer_bound);
                ProgressOutput progress (ma, "assemble surface element", ma->GetNSE());

                
                IterateElements 
                  (*fespace, BND, clh, 
                   [&] (ElementId ei, LocalHeap & lh)
                   {
                     int i = ei.Nr();
                     progress.Update ();                  
                     
                     if (!fespace->DefinedOnBoundary (ma->GetSElIndex (i))) return;
                     
                     timerb1.Start();
                      
                     const FiniteElement & fel = fespace->GetFE (ei, lh);
                     ElementTransformation & eltrans = ma->GetTrafo (ei, lh);

                     // FlatArray<int> dnums = fespace->GetDofNrs (ei, lh);
		     Array<int> dnums  (fel.GetNDof(), lh);
                     fespace->GetDofNrs (ei, dnums);
                     
                     if(fel.GetNDof() != dnums.Size())
                       {
                         cout << "Surface fel:GetNDof() = " << fel.GetNDof() << endl;
                         cout << "dnums.Size() = " << dnums.Size() << endl;
                         
                         (*testout) << "fel:GetNDof() = " << fel.GetNDof() << endl;
                         (*testout) << "dnums.Size() = " << dnums.Size() << endl;
                         (*testout) << "dnums = " << dnums << endl;
                          throw Exception ( "Inconsistent number of degrees of freedom " );
                       }
                     
                      timerb1.Stop();
                      int elmat_size = dnums.Size()*fespace->GetDimension();
                      FlatMatrix<SCAL> elmat(elmat_size, lh);
                      FlatMatrix<SCAL> sumelmat(elmat_size, lh);
                      sumelmat = SCAL (0.0);
                      
                      for (int k = 0; k < dnums.Size(); k++)
                        if (dnums[k] != -1)
                          useddof[dnums[k]] = true;

                      for (int j = 0; j < NumIntegrators(); j++)
                        {
                          const BilinearFormIntegrator & bfi = *parts[j];
                    
                          if (!bfi.BoundaryForm()) continue;
                          if (bfi.SkeletonForm()) continue;
                          if (!bfi.DefinedOn (ma->GetSElIndex (i))) continue;                
                          
                          timerb2.Start();

                          bfi.CalcElementMatrix (fel, eltrans, elmat, lh);
                          fespace->TransformMat (i, true, elmat, TRANSFORM_MAT_LEFT_RIGHT);

                          timerb2.Stop();

                          if (printelmat)
                            {
                              testout->precision(8);
                              
                              (*testout) << "surface-elnum= " << i << endl;
                              (*testout) << "integrator " << bfi.Name() << endl;
                              (*testout) << "dnums = " << endl << dnums << endl;
                              (*testout) << "ct = ";
                              for (int j = 0; j < dnums.Size(); j++)
                                if (dnums[j] == -1)
                                  *testout << "0 ";
                                else
                                  *testout << fespace->GetDofCouplingType (dnums[j]) << " ";
                              *testout << endl;
                              (*testout) << "element-index = " << eltrans.GetElementIndex() << endl;
                              (*testout) << "elmat = " << endl << elmat << endl;
                            }
                          
                          if (elmat_ev)
                            {
                              testout->precision(8);
                              (*testout) << "elind = " << eltrans.GetElementIndex() << endl;
                              LapackEigenSystem(elmat, lh);
                            }
                          
                          sumelmat += elmat;
                        }
		      
                      timerb3.Start();
                      
                      // #pragma omp critical (addelmatboundary)
                      {
                        AddElementMatrix (dnums, dnums, sumelmat, ElementId (BND, i), lh);
                      }
                      
                      for (int j = 0; j < preconditioners.Size(); j++)
                        preconditioners[j] -> AddElementMatrix (dnums, sumelmat, false, i, lh);
                      
                      timerb3.Stop();
                   }); // }//endof parallel 
                
                
                progress.Done();
                gcnt += nse;

              }//endof hasbound


	    RegionTimer reg(mattimer2);
            if (hasskeletonbound)
              {
                int cnt = 0;          
#pragma omp parallel
                {
                  LocalHeap lh = clh.Split();
                  Array<int> fnums, elnums, vnums, dnums;
                
#pragma omp for 
                  for (int i = 0; i < nse; i++)
                    {
#pragma omp critical(printmatasstatus2)
                      {
                        cnt++;
                        gcnt++;
                        if (cnt % 10 == 0)
                          cout << "\rassemble facet surface element " << cnt << "/" << nse << flush;
                        ma->SetThreadPercentage ( 100.0*(gcnt) / (loopsteps) );
                      }
                  
                      HeapReset hr(lh);
                      
                      if (!fespace->DefinedOnBoundary (ma->GetSElIndex (i))) continue;
                      ma->GetSElFacets(i,fnums);
                      int fac = fnums[0];
                      ma->GetFacetElements(fac,elnums);
                      int el = elnums[0];
                      ma->GetElFacets(el,fnums);
                                  
                      const FiniteElement & fel = fespace->GetFE (el, lh);
                      int facnr = 0;
                      for (int k=0; k<fnums.Size(); k++)
                        if(fac==fnums[k]) facnr = k;
                      ma->GetElVertices (el, vnums);     

                      ElementTransformation & eltrans = ma->GetTrafo (el, VOL, lh);
                      ElementTransformation & seltrans = ma->GetTrafo (i, BND, lh);
                      
                      fespace->GetDofNrs (el, dnums);
                      if(fel.GetNDof() != dnums.Size())
                        {
                          cout << "Surface fel:GetNDof() = " << fel.GetNDof() << endl;
                          cout << "dnums.Size() = " << dnums.Size() << endl;

                          (*testout) << "fel:GetNDof() = " << fel.GetNDof() << endl;
                          (*testout) << "dnums.Size() = " << dnums.Size() << endl;
                          (*testout) << "dnums = " << dnums << endl;
                          throw Exception ( "Inconsistent number of degrees of freedom " );
                        }

                      for (int j = 0; j < NumIntegrators(); j++)
                        {
                          const BilinearFormIntegrator & bfi = *parts[j];
              
                          if (!bfi.BoundaryForm()) continue;
                          if (!bfi.SkeletonForm()) continue;

                          if (!bfi.DefinedOn (ma->GetSElIndex(i) )) continue;                

                          for (int k = 0; k < dnums.Size(); k++)
                            if (dnums[k] != -1)
                              useddof[dnums[k]] = true;

                          int elmat_size = dnums.Size()*fespace->GetDimension();
                          FlatMatrix<SCAL> elmat(elmat_size, lh);

                          // original version did not compile on MacOS V
                          const FacetBilinearFormIntegrator & fbfi = 
                            dynamic_cast<const FacetBilinearFormIntegrator&>(bfi);  
                          fbfi.CalcFacetMatrix (fel,facnr,eltrans,vnums, seltrans, elmat, lh);
                          

                          fespace->TransformMat (i, false, elmat, TRANSFORM_MAT_LEFT_RIGHT);
                    
                          if (printelmat)
                            {
                              testout->precision(8);
                        
                              (*testout) << "surface-elnum= " << i << endl;
                              (*testout) << "integrator " << bfi.Name() << endl;
                              (*testout) << "dnums = " << endl << dnums << endl;
                              (*testout) << "element-index = " << eltrans.GetElementIndex() << endl;
                              (*testout) << "elmat = " << endl << elmat << endl;
                            }
                    
                    
                          if (elmat_ev)
                            {
                              testout->precision(8);
                        
                              (*testout) << "elind = " << eltrans.GetElementIndex() << endl;
#ifdef LAPACK
                              LapackEigenSystem(elmat, lh);
#else
                              Vector<SCAL> lami(elmat.Height());
                              Matrix<SCAL> evecs(elmat.Height());
                        
                              CalcEigenSystem (elmat, lami, evecs);
                              (*testout) << "lami = " << endl << lami << endl;
#endif
                              // << "evecs = " << endl << evecs << endl;
                            } 

                          //                    for(int k=0; k<elmat.Height(); k++)
                          //                      if(fabs(elmat(k,k)) < 1e-7 && dnums[k] != -1)
                          //                        cout << "dnums " << dnums << " elmat " << elmat << endl; 
#pragma omp critical(addelemfacbnd)
                          {
                            AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), lh);
                          }
                        }//end for (numintegrators)
                    }//end for nse                  
                }//end of parallel
                cout << "\rassemble facet surface element " << nse << "/" << nse << endl;         
              }//end of hasskeletonbound
            if (hasskeletoninner)
              {
                int dim = ma->GetDimension();
                BitArray fine_facet(nf);
                fine_facet.Clear();
                Array<int> elfacets;
                for (int i = 0; i < ne; ++i)
                  {
                    if(dim==2)
                      ma->GetElEdges(i,elfacets);
                    else
                      ma->GetElFaces(i,elfacets);
                    for (int j=0;j<elfacets.Size();j++)
                      fine_facet.Set(elfacets[j]);
                  }

                int cnt = 0;
#pragma omp parallel 
                {
                  LocalHeap lh = clh.Split();

                  // ElementTransformation eltrans1, eltrans2;
                  Array<int> dnums, dnums1, dnums2, elnums, fnums, vnums1, vnums2;
#pragma omp for 
                  for (int i = 0; i < nf; i++)
                    {
                      if (!fine_facet.Test(i)) continue;
                      HeapReset hr(lh);
                      
                      int el1 = -1;
                      int el2 = -1;
                      int facnr1 = -1;
                      int facnr2 = -1;

                      ma->GetFacetElements(i,elnums);
                      el1 = elnums[0];

                      if(elnums.Size()<2) continue;
                      el2 = elnums[1];

                      ma->GetElFacets(el1,fnums);
                      for (int k=0; k<fnums.Size(); k++)
                        if(i==fnums[k]) facnr1 = k;

                      ma->GetElFacets(el2,fnums);
                      for (int k=0; k<fnums.Size(); k++)
                        if(i==fnums[k]) facnr2 = k;
                      
#pragma omp critical(printmatasstatus2)
                      {
                        cnt++;
                        gcnt++;
                        if (cnt % 10 == 0)
                          cout << "\rassemble inner facet element " << cnt << "/" << nf << flush;
                        ma->SetThreadPercentage ( 100.0*(gcnt) / (loopsteps) );
                      }
                      
                  
                      const FiniteElement & fel1 = fespace->GetFE (el1, lh);
                      const FiniteElement & fel2 = fespace->GetFE (el2, lh);

                      ElementTransformation & eltrans1 = ma->GetTrafo (el1, false, lh);
                      ElementTransformation & eltrans2 = ma->GetTrafo (el2, false, lh);


                      fespace->GetDofNrs (el1, dnums1);
                      dnums=dnums1;
                      fespace->GetDofNrs (el2, dnums2);
                      for (int d=0; d < dnums2.Size(); d++)
                        dnums.Append(dnums2[d]);
                      ma->GetElVertices (el1, vnums1);
                      ma->GetElVertices (el2, vnums2);
                      if(fel1.GetNDof() != dnums1.Size() || ((elnums.Size()>1) && (fel2.GetNDof() != dnums2.Size() )))
                        {
                          cout << "facet, neighbouring fel(1): GetNDof() = " << fel1.GetNDof() << endl;
                          cout << "facet, neighbouring fel(2): GetNDof() = " << fel2.GetNDof() << endl;
                          cout << "facet, neighbouring fel(1): dnums.Size() = " << fel1.GetNDof() << endl;
                          cout << "facet, neighbouring fel(2): dnums.Size() = " << fel2.GetNDof() << endl;
                          throw Exception ( "Inconsistent number of degrees of freedom " );
                        }
                      for (int j = 0; j < NumIntegrators(); j++)
                        {
                          const BilinearFormIntegrator & bfi = *parts[j];
                    
                          if (!bfi.SkeletonForm()) continue;
                          if (bfi.BoundaryForm()) continue;
                          if (!bfi.DefinedOn (ma->GetElIndex (el1))) continue; //TODO: treat as surface element
                          if (!bfi.DefinedOn (ma->GetElIndex (el2))) continue; //TODO    

                          for (int k = 0; k < dnums.Size(); k++)
                            if (dnums[k] != -1)
                              useddof[dnums[k]] = true;

                          int elmat_size = (dnums1.Size()+dnums2.Size())*fespace->GetDimension();
                          FlatMatrix<SCAL> elmat(elmat_size, lh);

                          const FacetBilinearFormIntegrator & fbfi = 
                            dynamic_cast<const FacetBilinearFormIntegrator&>(bfi);
                          fbfi.CalcFacetMatrix (fel1,facnr1,eltrans1,vnums1,
                                                fel2,facnr2,eltrans2,vnums2, elmat, lh);
                          *testout << "elmat : \n" << elmat << endl;

                          fespace->TransformMat (el1, false, elmat.Rows(0,dnums1.Size()), TRANSFORM_MAT_LEFT);
                          fespace->TransformMat (el2, false, elmat.Rows(dnums1.Size(),dnums2.Size()), TRANSFORM_MAT_LEFT);
                          fespace->TransformMat (el1, false, elmat.Cols(0,dnums1.Size()), TRANSFORM_MAT_RIGHT);
                          fespace->TransformMat (el2, false, elmat.Cols(dnums1.Size(),dnums2.Size()), TRANSFORM_MAT_RIGHT);

                          if (printelmat)
                            {
                              testout->precision(8);

                              (*testout) << "facet-elnum= " << i << endl;
                              (*testout) << "integrator " << bfi.Name() << endl;
                              (*testout) << "dnums1 = " << endl << dnums1 << endl;
                              (*testout) << "dnums2 = " << endl << dnums2 << endl;
                              (*testout) << "element1-index = " << eltrans1.GetElementIndex() << endl;
                              (*testout) << "element2-index = " << eltrans2.GetElementIndex() << endl;
                              (*testout) << "elmat = " << endl << elmat << endl;
                            }

                    
                          if (elmat_ev)
                            {
                              testout->precision(8);

                              (*testout) << "elind1 = " << eltrans1.GetElementIndex() << endl;
                              (*testout) << "elind2 = " << eltrans2.GetElementIndex() << endl;
#ifdef LAPACK
                              LapackEigenSystem(elmat, lh);
#else
                              Vector<SCAL> lami(elmat.Height());
                              Matrix<SCAL> evecs(elmat.Height());
                            
                              CalcEigenSystem (elmat, lami, evecs);
                              (*testout) << "lami = " << endl << lami << endl;
#endif
                              // << "evecs = " << endl << evecs << endl;
                            }

                          //                    for(int k=0; k<elmat.Height(); k++)
                          //                      if(fabs(elmat(k,k)) < 1e-7 && dnums[k] != -1)
                          //                        cout << "dnums " << dnums << " elmat " << elmat << endl; 

#pragma omp critical(addelemfacin)
                          {
                            AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), lh);
                          }
                        }
                    }
                }
                cout << "\rassemble inner facet element " << nf << "/" << nf << endl;     
              }
            ma->SetThreadPercentage ( 100.0 );

            /*
              if(NumIndependentIntegrators() > 0)
              {
              DoAssembleIndependent(useddof,clh);
              }
            */


            clh.CleanUp();

            bool assembledspecialelements = false;
            
            
            int nspecel = 0;
#pragma omp parallel 
            {
              LocalHeap lh = clh.Split();
              Array<int> dnums;
              
#pragma omp for 
              for (int i = 0; i < fespace->specialelements.Size(); i++)
                {
#pragma omp critical(printmatspecel)
                  {
                    gcnt++;
                    nspecel++;
                    if (i % 10 == 0)
                      cout << "\rassemble special element " << nspecel << "/" << fespace->specialelements.Size() << flush;
                    ma->SetThreadPercentage ( 100.0*(gcnt) / (loopsteps) );
                  }
                  
                  const SpecialElement & el = *fespace->specialelements[i];
                
                  el.GetDofNrs (dnums);
                
                  FlatMatrix<SCAL> elmat(dnums.Size(), lh);
                  el.Assemble (elmat, lh);
                  
#pragma omp critical(printmatspecel2)
                  {
                    for (int j = 0; j < dnums.Size(); j++)
                      if (dnums[j] != -1)
                        useddof[dnums[j]] = true;
                    
                    AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), lh);
                  }
                  
                  assembledspecialelements = true;
                  lh.CleanUp();
                }
            }
            if(assembledspecialelements) cout << "\rassemble special element " 
                                              << fespace->specialelements.Size() << "/" << fespace->specialelements.Size() << endl;
            
          
            // add eps to avoid empty lines
            FlatMatrix<SCAL> elmat (fespace->GetDimension(), clh);
            elmat = 0;
            Array<int> dnums;
            dnums.SetSize(1);
            
            if (eps_regularization != 0)
              {
                for (int i = 0; i < elmat.Height(); i++)
                  elmat(i, i) = eps_regularization;
                for (int i = 0; i < ndof; i++)
                  {
                    dnums[0] = i; 
                    AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), clh);
                  }
              }
            if (unuseddiag != 0)
              {
                for (int i = 0; i < elmat.Height(); i++)
                  elmat(i, i) = unuseddiag;
                for (int i = 0; i < ndof; i++)
                  if (!useddof[i])
                    {
                      dnums[0] = i;
                      AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), clh);
                    }
              }

            for (int j = 0; j < preconditioners.Size(); j++)
              preconditioners[j] -> FinalizeLevel();

            if (print)
              (*testout) << "mat = " << endl << GetMatrix() << endl;

            int cntused = 0;
            for (int i = 0; i < useddof.Size(); i++)
              if (useddof[i])
                cntused++;
            
            // if ( (ntasks == 1) && (cntused < useddof.Size()))
            cout << IM(4) << "used " << cntused
                 << ", unused = " << useddof.Size()-cntused
                 << ", total = " << useddof.Size() << endl;
            

            int MASK = eliminate_internal ? EXTERNAL_DOF : ANY_DOF;
            bool first_time = true;

            if (MyMPI_GetNTasks() == 1)
              for (int i = 0; i < useddof.Size(); i++)
                if (useddof[i] != 
                    ((fespace->GetDofCouplingType(i) & MASK) != 0) )
                  {
                    *testout << "used dof inconsistency: " 
                             << " dof " << i << " used = " << int(useddof[i])
                             << " ct = " << fespace->GetDofCouplingType(i) << endl;
                  
                    if (first_time)
                      cerr << "used dof inconsistency" << endl;
                    first_time = false;
                  }
            

            ma->PopStatus ();
          }

        else

          {
            // mixed spaces

            cout << "assemble mixed bilinearform" << endl;
      
            BaseMatrix & mat = GetMatrix();
            mat = 0.0;


            bool hasbound = false;
            bool hasinner = false;

            for (int j = 0; j < NumIntegrators(); j++)
              {
                const BilinearFormIntegrator & bfi = *GetIntegrator(j);
                if (bfi.BoundaryForm())
                  hasbound = true;
                else
                  hasinner = true;
              }
            
            if (hasinner)
              IterateElements 
                (*fespace, VOL, clh,          // coloring for 1 space is enough
                 [&] (ElementId ei, LocalHeap & lh)
                 {
                   const FiniteElement & fel1 = fespace->GetFE (ei, lh);
                   const FiniteElement & fel2 = fespace2->GetFE (ei, lh);
                   
                   Array<int> dnums1(fel1.GetNDof(), lh);
                   Array<int> dnums2(fel2.GetNDof(), lh);
                   const ElementTransformation & eltrans = ma->GetTrafo (ei, lh);
                   fespace->GetDofNrs (ei, dnums1);
                   fespace2->GetDofNrs (ei, dnums2);
          
                   FlatMatrix<SCAL> elmat(dnums2.Size(), dnums1.Size(), lh);
                   for (int j = 0; j < NumIntegrators(); j++)
                     {
                       const BilinearFormIntegrator & bfi = *GetIntegrator(j);
                       if (bfi.BoundaryForm()) continue;

                       // ArrayMem<const FiniteElement*,2> fea = { &fel1, &fel2 };
                       ArrayMem<const FiniteElement*,2> fea(2);
                       fea[0] = &fel1;
                       fea[1] = &fel2;
                       CompoundFiniteElement cfel(fea);

                       bfi.CalcElementMatrix (cfel, eltrans, elmat, lh);

                       /*
                        fespace->Transform (i, true, elmat, TRANSFORM_MAT_RIGHT);
                        fespace2->Transform (i, true, elmat, TRANFORM_MAT_LEFT);
                       */

                       AddElementMatrix (dnums2, dnums1, elmat, ei, lh);
                     }
                 });


            if (hasbound)
              IterateElements 
                (*fespace, BND, clh,          // coloring for 1 space is enough
                 [&] (ElementId ei, LocalHeap & lh)
                 {
                   const FiniteElement & fel1 = fespace->GetFE (ei, lh);
                   const FiniteElement & fel2 = fespace2->GetFE (ei, lh);
                   
                   Array<int> dnums1(fel1.GetNDof(), lh);
                   Array<int> dnums2(fel2.GetNDof(), lh);
                   const ElementTransformation & eltrans = ma->GetTrafo (ei, lh);
                   fespace->GetDofNrs (ei, dnums1);
                   fespace2->GetDofNrs (ei, dnums2);
          
                   FlatMatrix<SCAL> elmat(dnums2.Size(), dnums1.Size(), lh);
                   for (int j = 0; j < NumIntegrators(); j++)
                     {
                       const BilinearFormIntegrator & bfi = *GetIntegrator(j);
                       if (!bfi.BoundaryForm()) continue;

                       // ArrayMem<const FiniteElement*,2> fea = { &fel1, &fel2 };
                       ArrayMem<const FiniteElement*,2> fea(2);
                       fea[0] = &fel1;
                       fea[1] = &fel2;
                       CompoundFiniteElement cfel(fea);

                       bfi.CalcElementMatrix (cfel, eltrans, elmat, lh);

                       /*
                        fespace->Transform (i, true, elmat, TRANSFORM_MAT_RIGHT);
                        fespace2->Transform (i, true, elmat, TRANFORM_MAT_LEFT);
                       */

                       AddElementMatrix (dnums2, dnums1, elmat, ei, lh);
                     }
                 });

            if (print) *testout << "mat = " << mat << endl;
            
            cout << endl;
          }

        //  WriteMatrix (*testout);

        if (checksum)
          cout << "|matrix| = " 
               << setprecision(16) << L2Norm (GetMatrix().AsVector()) << endl;
      }
    catch (Exception & e)
      {
        cout << "catch in AssembleBilinearform 2" << endl;
        e.Append (string ("in Assemble BilinearForm '") + 
                  GetName() + string("'\n"));
        throw;
      }
    catch (exception & e)
      {
        throw (Exception (string(e.what()) +
                          string("\n in Assemble BilinearForm '" + GetName() + "'\n")));
      }
  }

  
  template <class SCAL>
  void S_BilinearForm<SCAL> :: 
  ModifyRHS (BaseVector & f) const
  {
    if (keep_internal)
      f += GetHarmonicExtensionTrans() * f;
  }

  template <class SCAL>
  void S_BilinearForm<SCAL> :: 
  ComputeInternal (BaseVector & u, const BaseVector & f, LocalHeap & clh) const
  {
    if (!eliminate_internal) return;

    static Timer timer ("Compute Internal");
    RegionTimer reg (timer);


    try
      {
        ma->PushStatus ("Compute Internal");

        int ne = ma->GetNE();

        bool hasinner = false;
        for (int j = 0; j < NumIntegrators(); j++)
          if (!parts[j] -> BoundaryForm()) hasinner = true;
         
        if (hasinner)
          {
            if (keep_internal)
              {
                cout << IM(1) << "compute internal element ... ";
                
                //Set u_inner to zero
                for (int i = 0; i < ne; i++)
                  {
                    HeapReset hr(clh);
                    Array<int> dnums;
                    fespace->GetDofNrs (i, dnums, LOCAL_DOF);            
                    FlatVector<SCAL> elu (dnums.Size(), clh);
                    elu = 0.0;
                    u.SetIndirect (dnums, elu);
                  }
                
                if (linearform)
                  u += GetInnerSolve() * linearform->GetVector();
                else
                  u += GetInnerSolve() * f;
                
                u += GetHarmonicExtension() * u;
                cout << IM(1) << endl;
              }
            else
              {  
                ProgressOutput progress (ma, "compute internal element", ma->GetNE());

                IterateElements 
                  (*fespace, VOL, clh, 
                   [&] (ElementId ei, LocalHeap & lh)
                   {
                     progress.Update ();

                     const FiniteElement & fel = fespace->GetFE (ei, lh);
                     ElementTransformation & eltrans = ma->GetTrafo (ei, lh);
                     // FlatArray<int> dnums = fespace->GetDofNrs (ei, lh);
                     Array<int> dnums (fel.GetNDof(), lh);
                     fespace->GetDofNrs (ei, dnums);

                     Array<int> idofs(dnums.Size(), lh);
                     fespace->GetDofNrs (ei.Nr(), idofs, LOCAL_DOF);
                     if (!idofs.Size()) return;
                     for (int j = 0; j < idofs.Size(); j++)
                       idofs[j] = dnums.Pos(idofs[j]);
                     
                     int elmat_size = dnums.Size()*fespace->GetDimension();
                     FlatMatrix<SCAL> sum_elmat(elmat_size, lh);
                     sum_elmat = 0;
                     for (int j = 0; j < NumIntegrators(); j++)
                       {
                         HeapReset hr(lh);
                         const BilinearFormIntegrator & bfi = *parts[j];
                         
                         if (!bfi.VolumeForm()) continue;
                         if (!bfi.DefinedOn (eltrans.GetElementIndex())) continue;
                         
                         FlatMatrix<SCAL> elmat(elmat_size, lh);
                         bfi.CalcElementMatrix (fel, eltrans, elmat, lh);
                         
                         sum_elmat += elmat;
                       }
                
                     fespace->TransformMat (ei.Nr(), false, sum_elmat, TRANSFORM_MAT_LEFT_RIGHT);
                     
                     if (idofs.Size())
                       {
                         int dim = sum_elmat.Height() / dnums.Size();
                         
                         FlatVector<SCAL> elf (sum_elmat.Height(), lh);
                         FlatVector<SCAL> elu (sum_elmat.Height(), lh);
                         FlatVector<SCAL> resi(dim*idofs.Size(), lh);
                         FlatVector<SCAL> wi(dim*idofs.Size(), lh);
                         
                         FlatMatrix<SCAL> ai(dim*idofs.Size(), lh);
                         
                         if (linearform)
                           linearform->GetVector().GetIndirect (dnums, elf);
                         else
                           f.GetIndirect (dnums, elf);
                         u.GetIndirect (dnums, elu);
                          
                         // compute residuum
                         elf -= sum_elmat * elu;
                          
                         for (int jj = 0; jj < idofs.Size(); jj++)
                           for (int j = 0; j < dim; j++)
                             {
                               for (int kk = 0; kk < idofs.Size(); kk++)
                                 for (int k = 0; k < dim; k++)
                                   {
                                     ai(jj*dim+j, kk*dim+k) =
                                       sum_elmat(idofs[jj]*dim+j, idofs[kk]*dim+k);
                                   }
                               resi(jj*dim+j) = elf(idofs[jj]*dim+j);
                             }
                         
                         // *testout << "residuum = " << resi << endl;
                         
                         LapackInverse (ai);
                         wi = ai * resi;
                         
                         // *testout << "inv_ai = " << endl << inv_ai << endl;
                         
                         for (int jj = 0; jj < idofs.Size(); jj++)
                           for (int j = 0; j < dim; j++)
                             elu(idofs[jj]*dim+j) += wi(jj*dim+j);
                         
                         //  *testout << "elu = " << endl << elu << endl;
                         
                         u.SetIndirect (dnums, elu);
                       }
                   });
                
                progress.Done();
                
              }//end of keep_internal-if
          }
        ma->PopStatus ();
      }

    catch (Exception & e)
      {
        e.Append (string("in ComputeInternal\n"));
        throw;
      }
    catch (exception & e)
      {
        throw (Exception (string(e.what()) +
                          string("\n in ComputeInternal\n")));
      }
  }










  template <class SCAL>
  void S_BilinearForm<SCAL> :: AssembleLinearization (const BaseVector & lin,
                                                      LocalHeap & clh, 
                                                      bool reallocate)
  {
    static Timer timer ("Assemble Linearization");
    static Timer timervol ("Assemble Linearization - volume");
    static Timer timerbound ("Assemble Linearization - boundary");

    RegionTimer reg (timer);

    try
      {
        int ndof = fespace->GetNDof();
        BitArray useddof(ndof);
        useddof.Clear();
      
        int ne = ma->GetNE();
      
        BaseMatrix & mat = GetMatrix();
        mat = 0.0;
      
        cout << IM(3) << "Assemble linearization" << endl;
      
        Array<int> dnums;
        // ElementTransformation eltrans;
      
        bool hasbound = 0;
        bool hasinner = 0;
      
        for (int j = 0; j < NumIntegrators(); j++)
          {
            if (parts[j] -> BoundaryForm())
              hasbound = true;
            else
              hasinner = true;
          }
      

        for (int j = 0; j < preconditioners.Size(); j++)
          preconditioners[j] -> InitLevel();

        
        if (hasinner)
          {
            RegionTimer reg(timervol);
            ProgressOutput progress (ma, "assemble element", ma->GetNE());


            /*
            int cnt = 0;

            const Table<int> * element_coloring = &fespace->ElementColoring();
            int ncolors = (element_coloring) ? element_coloring->Size() : 1;
            for (int icol = 0; icol < ncolors; icol++)
              {
#pragma omp parallel 
                {
                  LocalHeap & clh = lh;
                  LocalHeap lh = clh.Split();
                  
                  Array<int> dnums, idofs, idofs1, odofs;
                  int nec = (element_coloring) ? (*element_coloring)[icol].Size() : ne;
                  
#pragma omp for schedule(dynamic) 
                  for (int ii = 0; ii < nec; ii++)
                    {
                      int i = (element_coloring) ? (*element_coloring)[icol][ii] : ii;
                      
                      
#pragma omp atomic
                      cnt++;
                      progress.Update (cnt);
                      HeapReset hr(lh);
                      if (!fespace->DefinedOn (ma->GetElIndex (i))) continue;
            */


            IterateElements 
              (*fespace, VOL, clh,  [&] (FESpace::Element el, LocalHeap & lh)
                   {
                     int i = el.Nr();
                     Array<int> dnums, idofs, idofs1, odofs;
                     
                     const FiniteElement & fel = fespace->GetFE (i, lh);
                     ElementTransformation & eltrans = ma->GetTrafo (i, false, lh);

                      fespace->GetDofNrs (i, dnums);
                      
                      if(fel.GetNDof() != dnums.Size())
                        {
                          cout << "fel::GetNDof() = " << fel.GetNDof() << endl;
                          cout << "dnums.Size() = " << dnums.Size() << endl;
                        }

                      for (int j = 0; j < dnums.Size(); j++)
                        if (dnums[j] != -1)
                          useddof.Set (dnums[j]);
                      
                      FlatMatrix<SCAL> sum_elmat(dnums.Size()*fespace->GetDimension(), lh);
                      FlatMatrix<SCAL> elmat(dnums.Size()*fespace->GetDimension(), lh);
                      sum_elmat = 0;
                      
                      FlatVector<SCAL> elveclin (dnums.Size()*fespace->GetDimension(), lh);
                      lin.GetIndirect (dnums, elveclin);
                      fespace->TransformVec (i, false, elveclin, TRANSFORM_SOL);
                      
                      for (int j = 0; j < NumIntegrators(); j++)
                        {
                          const BilinearFormIntegrator & bfi = *parts[j];
                          
                          if (bfi.BoundaryForm()) continue;
                          if (!bfi.DefinedOn (ma->GetElIndex (i))) continue;
                          
                          try
                            {
                              bfi.CalcLinearizedElementMatrix (fel, eltrans, elveclin, elmat, lh);
                              
                              if (printelmat) 
                                {
                                  testout->precision(8);
                                  (*testout) << "elnum= " << i << endl;
                                  (*testout) << "eltype " << fel.ElementType() << endl;
                                  (*testout) << "integrator " << bfi.Name() << endl;
                                  (*testout) << "dnums = " << endl << dnums << endl;
                                  (*testout) << "elveclin = " << endl << elveclin << endl;
                                  (*testout) << "elmat = " << endl << elmat << endl;
                                }


                              //////////////////////////////////////////////////
                              /*                      
                              //                      cout << " material: " << ma->GetElMaterial(i) << endl;
                                        
                              cout << " assemble linearization, elmat: " << endl;
                              cout << elmat << endl;

                              FlatMatrix<SCAL> new_elmat(dnums.Size()*fespace->GetDimension(), lh);
                              FlatVector<SCAL> e(dnums.Size()*fespace->GetDimension(), lh);
                              FlatVector<SCAL> temp(dnums.Size()*fespace->GetDimension(), lh);
                              FlatVector<SCAL> y1(dnums.Size()*fespace->GetDimension(), lh);
                              FlatVector<SCAL> y2(dnums.Size()*fespace->GetDimension(), lh);

                              double eps = 1e-5;
                              for ( int ii=0; ii<e.Size(); ii++ )
                              {
                              e = 0;
                              e(ii) = 1;
                              temp = elveclin + eps*e;
                              bfi.ApplyElementMatrix(fel,eltrans,temp,y1,0, lh);
                              bfi.ApplyElementMatrix(fel,eltrans,elveclin,y2, 0, lh);
                              temp = y1-y2;
                              for ( int jj=0; jj<new_elmat.Width(); jj++ ) new_elmat(jj,ii) = temp(jj)/eps;
                              }

                              cout << " elmat by num. diff:" << endl << new_elmat << endl;
                              */
                              //////////////////////////////////////////////////

                            }
                          catch (Exception & e)
                            {
                              e.Append (string("in Assemble Element Mat, bfi = ") + 
                                        bfi.Name() + string("\n"));
                              throw;
                            }
                          catch (exception & e)
                            {
                              throw (Exception (string(e.what()) +
                                                string("in Assemble Element Mat, bfi = ") + 
                                                bfi.Name() + string("\n")));
                            }
                  
                          sum_elmat += elmat;
                        }
              
                      fespace->TransformMat (i, false, sum_elmat, TRANSFORM_MAT_LEFT_RIGHT);
                       
                      if (printelmat)
                        *testout << "summat = " << sum_elmat << endl;


                      if (eliminate_internal)
                        {
                          static Timer statcondtimer("static condensation");
                          RegionTimer regstat (statcondtimer);
                          
                          fespace->GetDofNrs (i, idofs1, LOCAL_DOF);
                          for (int j = 0; j < idofs1.Size(); j++)
                            idofs1[j] = dnums.Pos(idofs1[j]);
                          
                          
                          if (printelmat) 
                            {
                              *testout << "eliminate internal" << endl;
                              *testout << "idofs1 = " << idofs1 << endl;
                            }
                          


                          if (idofs1.Size())
                            {
                              HeapReset hr (lh);
                              
                              int size = sum_elmat.Height();
                              int dim = size / dnums.Size();
                              
                              idofs.SetSize (0);
                              for (int j = 0; j < idofs1.Size(); j++)
                                for (int jj = 0; jj < dim; jj++)
                                  idofs.Append (dim*idofs1[j]+jj);
                              
                              int sizei = idofs.Size();
                              int sizeo = size - sizei;
                              
                              odofs.SetSize (0);
                              for (int j = 0; j < size; j++)
                                if (!idofs.Contains(j))
                                  odofs.Append(j);
                              
                              if (printelmat)
                                {
                                  (*testout) << "idofs = " << endl << idofs << endl;
                                  (*testout) << "odofs = " << endl << odofs << endl;
                                }
                              
                              FlatMatrix<SCAL> a(sizeo, sizeo, lh);
                              FlatMatrix<SCAL> b(sizeo, sizei, lh);
                              FlatMatrix<SCAL> c(sizeo, sizei, lh);
                              FlatMatrix<SCAL> d(sizei, sizei, lh);
                              
                              a = sum_elmat.Rows(odofs).Cols(odofs);
                              b = sum_elmat.Rows(odofs).Cols(idofs);
                              c = Trans(sum_elmat.Rows(idofs).Cols(odofs));
                              d = sum_elmat.Rows(idofs).Cols(idofs);
                              
                              
                              
                              
                              // A := A - B D^{-1} C^T
                              // new Versions, July 07
                              if (!keep_internal)
                                {
                                  LapackAInvBt (d, b);
                                  LapackMultAddABt (b, c, -1, a);
                                }
                              else
                                {
                                  ArrayMem<int,50> idnums1, idnums;
                                  ArrayMem<int,50> ednums1, ednums;

                                  fespace->GetDofNrs(i,idnums1,LOCAL_DOF);
                                  fespace->GetDofNrs(i,ednums1,EXTERNAL_DOF);

                                  for (int j = 0; j < idnums1.Size(); j++)
                                    idnums += dim*IntRange(idnums1[j], idnums1[j]+1);
                                  for (int j = 0; j < ednums1.Size(); j++)
                                    ednums += dim * IntRange(ednums1[j], ednums1[j]+1);

                                  if (store_inner)
                                    static_cast<ElementByElementMatrix<SCAL>*>(innermatrix)
                                      ->AddElementMatrix(i,idnums,idnums,d);
                                  
                                  LapackInverse (d);
                                  FlatMatrix<SCAL> he (sizei, sizeo, lh);
                                  he=0.0;
                                  LapackMultAddABt (d, c, -1, he);
                                  // he = -1.0 * d * Trans(c);
                                  static_cast<ElementByElementMatrix<SCAL>*>(harmonicext)
                                    ->AddElementMatrix(i,idnums,ednums,he);
                                  
                                  if (!symmetric)
                                    {
                                      FlatMatrix<SCAL> het (sizeo, sizei, lh);
                                      het = 0.0;
                                      LapackMultAddAB (b, d, -1, het);
                                      //  het = -1.0 * b * d;
                                      static_cast<ElementByElementMatrix<SCAL>*>(harmonicexttrans)
                                        ->AddElementMatrix(i,ednums,idnums,het);
                                    }
                                  static_cast<ElementByElementMatrix<SCAL>*>(innersolve)
                                    ->AddElementMatrix(i,idnums,idnums,d);
                                  
                                  LapackMultAddAB (b, he, 1.0, a);
                                }                                 
                              

                              if (printelmat)
                                *testout << "schur matrix = " << a << endl;

                              sum_elmat.Rows(odofs).Cols(odofs) = a;

                              for (int k = 0; k < idofs1.Size(); k++)
                                dnums[idofs1[k]] = -1;
                            }
                        }




                      AddElementMatrix (dnums, dnums, sum_elmat, ElementId(VOL,i), lh);

                      for (int j = 0; j < preconditioners.Size(); j++)
                        preconditioners[j] -> 
                          AddElementMatrix (dnums, sum_elmat, true, i, lh);
                    }
                }
              }
            progress.Done();
          }
      

      
        int nse = ma->GetNSE();
        if (hasbound)
          {
            RegionTimer reg(timerbound);
            ProgressOutput progress (ma, "assemble surface element", nse);


            /*
            int cnt = 0;
#pragma omp parallel 
            {
              LocalHeap & clh = lh;
              LocalHeap lh = clh.Split();
              
              Array<int> dnums;
#pragma omp for 
              for (int i = 0; i < nse; i++)
                {
                  HeapReset hr(lh);

#pragma omp atomic
                  cnt++;
                  progress.Update (cnt);
                  if (!fespace->DefinedOnBoundary (ma->GetSElIndex (i))) continue;

            */

            
            IterateElements 
              (*fespace, BND, clh,  [&] (FESpace::Element el, LocalHeap & lh)
               {
                 int i = el.Nr();
                 
                 progress.Update();
                  
                  const FiniteElement & fel = fespace->GetSFE (i, lh);
                  
                  ElementTransformation & eltrans = ma->GetTrafo (i, true, lh);
                  Array<int> dnums;
                  fespace->GetSDofNrs (i, dnums);
                    
                  for (int j = 0; j < dnums.Size(); j++)
                    if (dnums[j] != -1)
                      useddof.Set (dnums[j]);
                  
                  FlatVector<SCAL> elveclin (dnums.Size()*fespace->GetDimension(), lh);
                  FlatMatrix<SCAL> elmat (dnums.Size()*fespace->GetDimension(), lh);
                  FlatMatrix<SCAL> sum_elmat (dnums.Size()*fespace->GetDimension(), lh);
                  sum_elmat = SCAL(0.0);
                  
                  lin.GetIndirect (dnums, elveclin);
                  fespace->TransformVec (i, true, elveclin, TRANSFORM_SOL);
                  
                  for (int j = 0; j < NumIntegrators(); j++)
                    {
                      const BilinearFormIntegrator & bfi = *parts[j];
                      
                      if (!bfi.BoundaryForm()) continue;
                      
                      bfi.CalcLinearizedElementMatrix (fel, eltrans, elveclin, elmat, lh);
                      
                      fespace->TransformMat (i, true, elmat, TRANSFORM_MAT_LEFT_RIGHT);
                      
                      sum_elmat += elmat;
                      
                        
                      if (printelmat) 
                        // #pragma omp critical (addelmatboundary1)
                        {
                          testout->precision(8);
                          (*testout) << "surface-elnum= " << i << endl;
                          (*testout) << "eltype " << fel.ElementType() << endl;
                          (*testout) << "boundary = " << ma->GetSElBCName (i) << endl;
                          (*testout) << "integrator " << bfi.Name() << endl;
                          (*testout) << "dnums = " << endl << dnums << endl;
                          (*testout) << "elveclin = " << endl << elveclin << endl;
                          (*testout) << "elmat = " << endl << elmat << endl;
                        }
                    }
                  
                  // #pragma omp critical (addelmatboundary)
                  {
                    AddElementMatrix (dnums, dnums, sum_elmat, ElementId(BND,i), lh);
                    
                    for (int j = 0; j < preconditioners.Size(); j++)
                      preconditioners[j] -> 
                        AddElementMatrix (dnums, sum_elmat, false, i, lh);
                  }
                }
            }
            progress.Done();
          }
        
      
        if (fespace->specialelements.Size())
          cout << "special elements: " << fespace->specialelements.Size() << endl;

        for (int i = 0; i < fespace->specialelements.Size(); i++)
          {
            HeapReset hr(lh);
            const SpecialElement & el = *fespace->specialelements[i];
            el.GetDofNrs (dnums);
          
            for (int j = 0; j < dnums.Size(); j++)
              if (dnums[j] != -1)
                useddof.Set (dnums[j]);
          
            FlatMatrix<SCAL> elmat;
            el.Assemble (elmat, lh);
          
            AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), lh);
          }
      
      
        // add eps to avoid empty lines
        FlatMatrix<SCAL> elmat (fespace->GetDimension(), lh);
        elmat = 0;
        dnums.SetSize(1);
      
        if (eps_regularization != 0)
          {
            for (int i = 0; i < elmat.Height(); i++)
              elmat(i, i) = eps_regularization;
            for (int i = 0; i < ndof; i++)
              {
                dnums[0] = i; 
                AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), lh);
              }
          }
        if (unuseddiag != 0)
          {
            for (int i = 0; i < elmat.Height(); i++)
              elmat(i, i) = unuseddiag;
            for (int i = 0; i < ndof; i++)
              if (!useddof.Test (i))
                {
                  dnums[0] = i;
                  AddElementMatrix (dnums, dnums, elmat, ElementId(BND,i), lh);
                }
          }
      
        if (print)
          {
            (*testout) << "mat = " << endl << GetMatrix() << endl;
            if (harmonicext) (*testout) << "harm ext = " << endl << *harmonicext << endl;
            if (harmonicexttrans) (*testout) << "harm ext = " << endl << *harmonicexttrans << endl;
            if (innermatrix) (*testout) << "harm ext = " << endl << *innermatrix << endl;
          }
        /*
          if (mat.Height() < 100)
          mat.Print (cout);
        */
        int cntused = 0;
        for (int i = 0; i < useddof.Size(); i++)
          if (useddof.Test(i))
            cntused++;
        cout << IM(5) << "used " << cntused
             << ", unused = " << useddof.Size()-cntused
             << ", total = " << useddof.Size() << endl;
  

        for (int j = 0; j < preconditioners.Size(); j++)
          preconditioners[j] -> FinalizeLevel();
      }
    catch (Exception & e)
      {
        stringstream ost;
        ost << "in Assemble BilinearForm" << endl;
        e.Append (ost.str());
        throw;
      }
    catch (exception & e)
      {
        throw (Exception (string(e.what()) +
                          string("\n in Assemble BilinearForm\n")));
      }
  }









  template <class SCAL>
  void S_BilinearForm<SCAL> :: AddMatrix1 (SCAL val,
                                           const BaseVector & x,
                                           BaseVector & y) const
  {
    static Timer timer ("Apply Matrix");
    static Timer timervol ("Apply Matrix - volume");
    static Timer timerbound ("Apply Matrix - boundary");
    RegionTimer reg (timer);


    static int lh_size = 5000000;

    if (!MixedSpaces())

      {
        // int ne = ma->GetNE();
      
        bool hasbound = false;
        bool hasinner = false;
        bool hasskeletonbound = false;
        bool hasskeletoninner = false;

        for (int j = 0; j < NumIntegrators(); j++)
          {
            const BilinearFormIntegrator & bfi = *GetIntegrator(j);
            if (bfi.BoundaryForm())
              if (bfi.SkeletonForm())
                hasskeletonbound = true;
              else
                hasbound = true;
            else
              if (bfi.SkeletonForm())
                hasskeletoninner = true; 
              else
                hasinner = true;
          }

        bool done = false;
        int atempt = 0;

        while(!done && atempt < 3)
          {
            try
              {

                int cnt = 0;

                if (hasinner)
                  {
                    RegionTimer reg (timervol);
#ifdef _OPENMP
		    LocalHeap clh (lh_size*omp_get_max_threads(), "biform-AddMatrix - Heap");
#else
		    LocalHeap clh (lh_size, "biform-AddMatrix - Heap");
#endif
		    IterateElements 
		      (*fespace, VOL, clh, 
		       [&] (ElementId ei, LocalHeap & lh)
                       
		       {
			 if (!fespace->DefinedOn (ei)) return;
			 
			 const FiniteElement & fel = fespace->GetFE (ei, lh);
			 ElementTransformation & eltrans = ma->GetTrafo (ei, lh);
			 Array<int> dnums (fel.GetNDof(), lh);
			 fespace->GetDofNrs (ei, dnums);
			 
			 ApplyElementMatrix(x,y,val,dnums,eltrans, ei.Nr(),0,cnt,lh,&fel);
		       });
                  }

                        
                // int nse = ma->GetNSE();
                if (hasbound)
                  {
                    RegionTimer reg (timerbound);

#ifdef _OPENMP
		    LocalHeap clh (lh_size*omp_get_max_threads(), "biform-AddMatrix - Heap");
#else
		    LocalHeap clh (lh_size, "biform-AddMatrix - Heap");
#endif
                    
		    IterateElements 
		      (*fespace, BND, clh, 
		       [&] (ElementId ei, LocalHeap & lh)
                       
		       {
                         HeapReset hr(lh);
                         
                         const FiniteElement & fel = fespace->GetFE (ei, lh);
                         ElementTransformation & eltrans = ma->GetTrafo (ei, lh);
			 Array<int> dnums (fel.GetNDof(), lh);
                         fespace->GetDofNrs (ei, dnums);
                         
                         ApplyElementMatrix(x,y,val,dnums,eltrans,ei.Nr(),1,cnt,lh,&fel);
                       });
                  }

                if (hasskeletonbound||hasskeletoninner)
                  throw Exception ("No BilinearFormApplication-Implementation for Facet-Integrators yet");
                  
                if (fespace->specialelements.Size())
                  {
                    LocalHeap lh(lh_size, "biform-AddMatrix (c)");
                    Array<int> dnums;
                    ElementTransformation * dummy_eltrans = NULL;
                    for (int i = 0; i < fespace->specialelements.Size(); i++)
                      {
                        HeapReset hr(lh);
                        const SpecialElement & el = *fespace->specialelements[i];
                        el.GetDofNrs (dnums);
                        
                        ApplyElementMatrix(x,y,val,dnums,*dummy_eltrans,i,2,cnt,lh,NULL,&el);
                      }
                  }
                done = true;
              }
            catch (LocalHeapOverflow lhex)
              {
                lh_size *= 2;
                atempt++;
                cerr << "Trying automatic heapsize increase to " << lh_size << endl;
              }
          }
      }
    else
      { 
        cout << "apply not implemented for mixed" << endl;
      }
  }






  template <class SCAL>
  void S_BilinearForm<SCAL> :: ApplyLinearizedMatrixAdd1 (SCAL val,
                                                          const BaseVector & lin,
                                                          const BaseVector & x,
                                                          BaseVector & y) const
  {
    if (!MixedSpaces())

      {
        Array<int> dnums;
      
        int ne = ma->GetNE();
        int dim = GetFESpace()->GetDimension(); 
        LocalHeap lh (2000000, "biform-ApplyLinearized");

        bool hasbound = false;
        bool hasinner = false;

        for (int j = 0; j < NumIntegrators(); j++)
          {
            const BilinearFormIntegrator & bfi = *GetIntegrator(j);
            if (bfi.BoundaryForm())
              hasbound = true;
            else
              hasinner = true;
          }


        if (hasinner)
          for (int i = 0; i < ne; i++)
            {
              HeapReset hr(lh);
          
              const FiniteElement & fel = fespace->GetFE (i, lh);
              ElementTransformation & eltrans = ma->GetTrafo (i, false, lh);
              fespace->GetDofNrs (i, dnums);
          
              FlatVector<SCAL> elveclin (dnums.Size() * dim, lh);
              FlatVector<SCAL> elvecx (dnums.Size() * dim, lh);
              FlatVector<SCAL> elvecy (dnums.Size() * dim, lh);

              lin.GetIndirect (dnums, elveclin);
              fespace->TransformVec (i, false, elveclin, TRANSFORM_SOL);

              x.GetIndirect (dnums, elvecx);
              fespace->TransformVec (i, false, elvecx, TRANSFORM_SOL);

              for (int j = 0; j < NumIntegrators(); j++)
                {
                  const BilinearFormIntegrator & bfi = *parts[j];

                  if (bfi.BoundaryForm()) continue;
                  if (!bfi.DefinedOn (ma->GetElIndex (i))) continue;


                  bfi.ApplyLinearizedElementMatrix (fel, eltrans, elveclin, elvecx, elvecy, lh);

                  fespace->TransformVec (i, false, elvecy, TRANSFORM_RHS);

                  elvecy *= val;

                  y.AddIndirect (dnums, elvecy);
                }
            }

        int nse = ma->GetNSE();
        if (hasbound)
          for (int i = 0; i < nse; i++)
            {
              HeapReset hr(lh);
              const FiniteElement & fel = fespace->GetSFE (i, lh);
              ElementTransformation & eltrans = ma->GetTrafo (i, true, lh);
              fespace->GetSDofNrs (i, dnums);
            
              FlatVector<SCAL> elveclin (dnums.Size() * dim, lh);
              FlatVector<SCAL> elvecx (dnums.Size() * dim, lh);
              FlatVector<SCAL> elvecy (dnums.Size() * dim, lh);
            
              lin.GetIndirect (dnums, elveclin);
              fespace->TransformVec (i, true, elveclin, TRANSFORM_SOL);
              x.GetIndirect (dnums, elvecx);
              fespace->TransformVec (i, true, elvecx, TRANSFORM_SOL);
          
              for (int j = 0; j < NumIntegrators(); j++)
                {
                  const BilinearFormIntegrator & bfi = *parts[j];
                
                  if (!bfi.BoundaryForm()) continue;
                  if (!bfi.DefinedOn (eltrans.GetElementIndex())) continue;
              
                  bfi.ApplyLinearizedElementMatrix (fel, eltrans, elveclin, elvecx, elvecy, lh);
                  fespace->TransformVec (i, true, elvecy, TRANSFORM_RHS);
                  elvecy *= val;
                  y.AddIndirect (dnums, elvecy);
                }
            }

        for (int i = 0; i < fespace->specialelements.Size(); i++)
          {
            HeapReset hr(lh);
            const SpecialElement & el = *fespace->specialelements[i];
            el.GetDofNrs (dnums);
          
            FlatVector<SCAL> elvecx (dnums.Size() * dim, lh);
            FlatVector<SCAL> elvecy (dnums.Size() * dim, lh);

            x.GetIndirect (dnums, elvecx);
            el.Apply (elvecx, elvecy, lh);

            elvecy *= val;
            y.AddIndirect (dnums, elvecy);
            lh.CleanUp();
          }
      }
    else
      { 
        cout << "apply not implemented for mixed" << endl;
      }
  }







  template <class SCAL>
  double S_BilinearForm<SCAL> :: Energy (const BaseVector & x) const
  {
    double energy = 0;

    if (!MixedSpaces())
      {
        LocalHeap lh (2000000, "biform-energy");

        bool hasbound = false;
        bool hasinner = false;

        for (int j = 0; j < NumIntegrators(); j++)
          {
            const BilinearFormIntegrator & bfi = *GetIntegrator(j);
            if (bfi.BoundaryForm())
              hasbound = true;
            else
              hasinner = true;
          }

        if (hasinner)
          IterateElements 
            (*fespace, VOL, lh, 
             [&] (ElementId ei, LocalHeap & lh)
             {
               const FiniteElement & fel = fespace->GetFE (ei, lh);
               ElementTransformation & eltrans = ma->GetTrafo (ei, lh);
               Array<int> dnums (fel.GetNDof(), lh);
               fespace->GetDofNrs (ei, dnums);
               
               FlatVector<SCAL> elvecx (dnums.Size()*GetFESpace()->GetDimension(), lh);
               
               x.GetIndirect (dnums, elvecx);
               fespace->TransformVec (ei, elvecx, TRANSFORM_SOL);
               
               double energy_T = 0;
               for (int j = 0; j < NumIntegrators(); j++)
                 {
                   auto bfi = parts[j];
                   
                   if (bfi->BoundaryForm()) continue;
		   if (!bfi->DefinedOn (ma->GetElIndex (ei))) continue;

                   energy_T += bfi->Energy (fel, eltrans, elvecx, lh);
                 }
#pragma omp atomic
               energy += energy_T;
             });

        int nse = ma->GetNSE();
        Array<int> dnums;
        if (hasbound)
          for (int i = 0; i < nse; i++)
            {
              HeapReset hr(lh);

              const FiniteElement & fel = fespace->GetSFE (i, lh);
              ElementTransformation & eltrans = ma->GetTrafo (i, true, lh);
              fespace->GetSDofNrs (i, dnums);
            
              FlatVector<SCAL> elvecx (dnums.Size() * GetFESpace()->GetDimension(), lh);
              x.GetIndirect (dnums, elvecx);
              fespace->TransformVec (i, true, elvecx, TRANSFORM_SOL);
          
              for (int j = 0; j < NumIntegrators(); j++)
                {
                  const BilinearFormIntegrator & bfi = *parts[j];
		  if (!bfi.DefinedOn (ma->GetElIndex (ElementId(BND,i)))) continue;
                
                  if (!bfi.BoundaryForm()) continue;
                  energy += bfi.Energy (fel, eltrans, elvecx, lh);
                }
            }

        for (int i = 0; i < fespace->specialelements.Size(); i++)
          {
            HeapReset hr(lh);

            const SpecialElement & el = *fespace->specialelements[i];
            el.GetDofNrs (dnums);

            FlatVector<SCAL> elvecx (dnums.Size() * GetFESpace()->GetDimension(), lh);
            x.GetIndirect (dnums, elvecx);
          
            energy += el.Energy (elvecx, lh);
          }
      }
    return energy;
  }


  template <class SCAL>
  void S_BilinearForm<SCAL> :: 
  AddDiagElementMatrix (const Array<int> & dnums1,
                        const FlatVector<SCAL> & diag,
                        bool inner_element, int elnr,
                        LocalHeap & lh)
  {
    throw Exception ("Baseclass::AddDiagElementMatrix");
  }
 



  template class S_BilinearForm<double>;
  template class S_BilinearForm<Complex>;

  template <class TM, class TV>
  T_BilinearForm<TM,TV>::
  T_BilinearForm (shared_ptr<FESpace> afespace, const string & aname, const Flags & flags)
    : S_BilinearForm<TSCAL> (afespace, aname, flags)
  { 
    if (this->fespace->LowOrderFESpacePtr())
      this->low_order_bilinear_form = 
        make_shared<T_BilinearForm<TM,TV>> 
        (this->fespace->LowOrderFESpacePtr(), aname+string(" low-order"), flags);
  }

  template <class TM, class TV>
  T_BilinearForm<TM,TV>::
  T_BilinearForm (shared_ptr<FESpace> afespace, 
                  shared_ptr<FESpace> afespace2,
                  const string & aname,
                  const Flags & flags)
    : S_BilinearForm<TSCAL> (afespace, afespace2, aname, flags) 
  {
    ;
  }

  template <class TM, class TV>
  T_BilinearForm<TM,TV>::
  ~T_BilinearForm ()
  {
    /*
    for (int i = 0; i < this->mats.Size(); i++)
      {
      delete this->mats[i];
        this->mats[i].reset();
      }
    */
  }



  template <class TM, class TV>
  void T_BilinearForm<TM,TV>::
  AllocateMatrix ()
  {
    if (this->mats.Size() == this->ma->GetNLevels())
      return;

    MatrixGraph * graph = this->GetGraph (this->ma->GetNLevels()-1, false);

    shared_ptr<BaseMatrix> mat = make_shared<SparseMatrix<TM,TV,TV>> (*graph, 1);
#ifdef PARALLEL
    if ( this->GetFESpace()->IsParallel() )
      mat = make_shared<ParallelMatrix> (mat, &this->GetFESpace()->GetParallelDofs());
#endif
    this->mats.Append (mat);

    delete graph;

    if (!this->multilevel || this->low_order_bilinear_form)
      for (int i = 0; i < this->mats.Size()-1; i++)
        {
          /*
          delete this->mats[i];
          this->mats[i] = 0;
          */
          this->mats[i].reset();
        }
  }


  template <class TM, class TV>
  void T_BilinearForm<TM,TV>::
  CleanUpLevel ()
  {
    if (!this->multilevel || this->low_order_bilinear_form)
      for (int i = 0; i < this->mats.Size(); i++)
        {
          /*
          delete this->mats[i];
          this->mats[i] = 0;
          */
          this->mats[i].reset();
        }
  }



  template <class TM, class TV>
  shared_ptr<BaseVector> T_BilinearForm<TM, TV>::
  CreateVector() const
  {
    auto afespace = this->fespace;
#ifdef PARALLEL
    if ( afespace->IsParallel() )
      return make_shared<ParallelVVector<TV>> (afespace->GetNDof(), &afespace->GetParallelDofs());
    else
#endif
      return make_shared<VVector<TV>> (afespace->GetNDof());
  }





  template <class TM, class TS>
  inline void AddPartOfElementMatrix(TM & dest, const FlatMatrix<TS> & source,
                                     const int start1, const int start2)
  {
    int hi = Height(dest);
    int wi = Width(dest);
          
    for (int k = 0; k < hi; k++)
      for (int l = 0; l < wi; l++)
        dest(k,l) += source(start1*hi+k, start2*wi+l);
  }
  
  template <>
  inline void AddPartOfElementMatrix(double & dest, 
                                     const FlatMatrix<double> & source,
                                     const int start1, const int start2)
  {
    dest += source(start1, start2);
  }

  template <>
  inline void AddPartOfElementMatrix(Complex & dest, 
                                     const FlatMatrix<Complex> & source,
                                     const int start1, const int start2)
  {
    dest += source(start1, start2);
  }
    
  
  template <class TM, class TV>
  void T_BilinearForm<TM,TV>::
  AddElementMatrix (FlatArray<int> dnums1,
                    FlatArray<int> dnums2,
                    FlatMatrix<TSCAL> elmat,
                    ElementId id,
                    LocalHeap & lh) 
  {
    BaseMatrix * hmat = this->mats.Last().get();
    
#ifdef PARALLEL
    ParallelMatrix * parmat = dynamic_cast<ParallelMatrix*> (hmat);
    if (parmat) hmat = &parmat->GetMatrix();
#endif   

    TMATRIX & mat = dynamic_cast<TMATRIX&> (*hmat);

    mat.AddElementMatrix (dnums1, dnums2, elmat);
  }


  template <class TM, class TV>
  void T_BilinearForm<TM,TV>::LapackEigenSystem(FlatMatrix<TSCAL> & elmat, LocalHeap & lh) const 
  {
    Vector<Complex> lami(elmat.Height());
    Matrix<TSCAL> evecs(elmat.Height());
    FlatMatrix<TSCAL> elmat_save(elmat.Height(), elmat.Width(), lh);
    elmat_save = elmat;
#ifdef LAPACK
    LapackEigenValues (elmat_save, lami, evecs);
    (*testout) << "lami = " 
               << endl << lami << endl << "evecs: " << endl << evecs << endl;
#endif
  }



  template <class TM, class TV>
  void T_BilinearForm<TM,TV>::ApplyElementMatrix(const BaseVector & x,
                                                 BaseVector & y,
                                                 const TSCAL & val,
                                                 const Array<int> & dnums,
                                                 const ElementTransformation & eltrans,
                                                 const int elnum,
                                                 const int type,
                                                 int & cnt,
                                                 LocalHeap & lh,
                                                 const FiniteElement * fel,
                                                 const SpecialElement * sel) const
  {
    FlatVector<typename mat_traits<TV>::TSCAL> elvecx (dnums.Size() * this->GetFESpace()->GetDimension(), lh);
    FlatVector<typename mat_traits<TV>::TSCAL> elvecy (dnums.Size() * this->GetFESpace()->GetDimension(), lh);

    x.GetIndirect (dnums, elvecx);

    if(type == 0 || type == 1)
      {
        this->fespace->TransformVec (elnum, (type == 1), elvecx, TRANSFORM_SOL);

        for (int j = 0; j < this->NumIntegrators(); j++)
          {
            BilinearFormIntegrator & bfi = *this->parts[j];
            
            if (bfi.SkeletonForm()) continue;
            if (type == 0 && bfi.BoundaryForm()) continue;
            if (type == 0 && !bfi.DefinedOn (this->ma->GetElIndex (elnum))) continue;
            if (type == 1 && !bfi.BoundaryForm()) continue;
            
            
            // static Timer elementtimer ("Element matrix application");
            // elementtimer.Start();

            if (this->precompute)
              // bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, this->precomputed_data[cnt++], lh);
              bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, 
                                      this->precomputed_data[elnum*this->NumIntegrators()+j], lh);
            else
              bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, 0, lh);
	    // elvecy = 0.0;

            // elementtimer.Stop();
            
            BilinearForm::GetFESpace()->TransformVec (elnum, (type == 1), elvecy, TRANSFORM_RHS);
        
            elvecy *= val;
            y.AddIndirect (dnums, elvecy);  // coloring	      
          }
        *testout << "||y|| = " << L2Norm(y) << endl;
      }
    else if (type == 2)
      {
        sel->Apply (elvecx, elvecy, lh);
        elvecy *= val;
        y.AddIndirect (dnums, elvecy);
      }
                      
  }


  template <class TM, class TV>
  T_BilinearFormSymmetric<TM,TV> :: 
  T_BilinearFormSymmetric (shared_ptr<FESpace> afespace, const string & aname,
                           const Flags & flags)
    : S_BilinearForm<TSCAL> (afespace, aname, flags) 
  {
    if (this->fespace->LowOrderFESpacePtr())
      {
        this->low_order_bilinear_form = 
          make_shared<T_BilinearFormSymmetric<TM,TV>> 
          (this->fespace->LowOrderFESpacePtr(), aname+string(" low-order"), flags);
      }
  }

  template <class TM, class TV>
  T_BilinearFormSymmetric<TM,TV> :: 
  ~T_BilinearFormSymmetric ()
  {
    /*
    for (int i = 0; i < this->mats.Size(); i++)
      delete this->mats[i];
    */
  }


  template <class TM, class TV>
  void T_BilinearFormSymmetric<TM,TV> :: 
  AllocateMatrix ()
  {
    if (this->mats.Size() == this->ma->GetNLevels())
      return;

    MatrixGraph * graph = this->GetGraph (this->ma->GetNLevels()-1, true);



    auto spmat = make_shared<SparseMatrixSymmetric<TM,TV>> (*graph, 1);
    if (this->spd) spmat->SetSPD();
    shared_ptr<BaseMatrix> mat = spmat;

#ifdef PARALLEL
    if ( this->GetFESpace()->IsParallel() )
      mat = make_shared<ParallelMatrix> (mat, &this->GetFESpace()->GetParallelDofs());
#endif
    this->mats.Append (mat);


    delete graph;

    if (!this->multilevel || this->low_order_bilinear_form)
      for (int i = 0; i < this->mats.Size()-1; i++)
        {
          /*
          delete this->mats[i];
          this->mats[i] = 0;
          */
          this->mats[i].reset();
        }
  }


  template <class TM, class TV>
  void T_BilinearFormSymmetric<TM,TV>::
  CleanUpLevel ()
  {
    if (!this->multilevel || this->low_order_bilinear_form)
      for (int i = 0; i < this->mats.Size(); i++)
        {
          /*
          delete this->mats[i];
          this->mats[i] = 0;
          */
          this->mats[i].reset();
        }
  }


  template <class TM, class TV>
  shared_ptr<BaseVector> T_BilinearFormSymmetric<TM, TV>::
  CreateVector() const
  {
    auto afespace = this->fespace;
#ifdef PARALLEL
    if ( afespace->IsParallel() )
      return make_shared<ParallelVVector<TV>> (afespace->GetNDof(), &afespace->GetParallelDofs());
    else
#endif
      // return new VVector<TV> (afespace->GetNDof());
      return make_shared<VVector<TV>> (afespace->GetNDof());
  }



  template <class TM, class TV>
  void T_BilinearFormSymmetric<TM,TV> :: 
  AddElementMatrix (FlatArray<int> dnums1,
                    FlatArray<int> dnums2,
                    FlatMatrix<TSCAL> elmat,
                    ElementId id, 
                    LocalHeap & lh) 
  {
    BaseMatrix * hmat = this->mats.Last().get();

#ifdef PARALLEL
    ParallelMatrix * parmat = dynamic_cast<ParallelMatrix*> (hmat);
    if (parmat) hmat = &parmat->GetMatrix();
#endif   

    TMATRIX & mat = dynamic_cast<TMATRIX&> (*hmat);

    mat.AddElementMatrix (dnums1, elmat);
  }






  template <class TM, class TV>
  void T_BilinearFormSymmetric<TM,TV>::LapackEigenSystem(FlatMatrix<TSCAL> & elmat, LocalHeap & lh) const
  {
    if ( !this->fespace->IsComplex() )
      {
        Vector<TSCAL> lami(elmat.Height());
        Matrix<TSCAL> evecs(elmat.Height());
#ifdef LAPACK
        LapackEigenValuesSymmetric (elmat, lami, evecs);
#else
        CalcEigenSystem (elmat, lami, evecs);
#endif
        (*testout) << "lami = " << endl << lami << endl << "evecs: " << endl << evecs << endl;
      }
    else
      {
        Vector<Complex> lami(elmat.Height());
        Matrix<TSCAL> evecs(elmat.Height());
        FlatMatrix<TSCAL> elmat_save(elmat.Height(), elmat.Width(), lh);
        elmat_save = elmat;
#ifdef LAPACK
        LapackEigenValues (elmat_save, lami, evecs);
        (*testout) << "LAPACK NS for complex symmetric problem \nlami = " 
                   << endl << lami << endl << "evecs: " << endl << evecs << endl;
#endif
      }
  }


  template <class TM, class TV>
  void T_BilinearFormSymmetric<TM,TV>::ApplyElementMatrix(const BaseVector & x,
                                                          BaseVector & y,
                                                          const TSCAL & val,
                                                          const Array<int> & dnums,
                                                          const ElementTransformation & eltrans,
                                                          const int elnum,
                                                          const int type,
                                                          int & cnt,
                                                          LocalHeap & lh,
                                                          const FiniteElement * fel,
                                                          const SpecialElement * sel) const
  {
    FlatVector<typename mat_traits<TV>::TSCAL> elvecx (dnums.Size() * this->GetFESpace()->GetDimension(), lh);
    FlatVector<typename mat_traits<TV>::TSCAL> elvecy (dnums.Size() * this->GetFESpace()->GetDimension(), lh);
                      
    x.GetIndirect (dnums, elvecx);

    if(type == 0 || type == 1)
      {
        this->fespace->TransformVec (elnum, (type == 1), elvecx, TRANSFORM_SOL);

        for (int j = 0; j < this->NumIntegrators(); j++)
          {
            BilinearFormIntegrator & bfi = *this->parts[j];
            
            if (bfi.SkeletonForm()) continue;
            if (type == 0 && bfi.BoundaryForm()) continue;
            if (type == 0 && !bfi.DefinedOn (this->ma->GetElIndex (elnum))) continue;
            if (type == 1 && !bfi.BoundaryForm()) continue;
            
            
            static Timer elementtimer ("Element matrix application");
            elementtimer.Start();
            
            
            if (this->precompute)
              // bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, this->precomputed_data[cnt++], lh);
              bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, 
                                      this->precomputed_data[elnum*this->NumIntegrators()+j], lh);
            else
              bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, 0, lh);
            
            elementtimer.Stop();
            
            /*
              testout->precision (12);
              (*testout) << "el " << i << ", dom = " << ma->GetElIndex(i) << ",integrator = " << typeid(bfi).name() << endl
              << "elx = " << elvecx 
              << "ely = " << elvecy << endl;
            */
            BilinearForm::GetFESpace()->TransformVec (elnum, (type == 1), elvecy, TRANSFORM_RHS);
            elvecy *= val;
            y.AddIndirect (dnums, elvecy);
          }
      }
    else if (type == 2)
      {
        sel->Apply (elvecx, elvecy, lh);
        elvecy *= val;
        y.AddIndirect (dnums, elvecy);
      }
                      
  }







  template <class TM>
  T_BilinearFormDiagonal<TM> :: 
  T_BilinearFormDiagonal (shared_ptr<FESpace> afespace, const string & aname,
                          const Flags & flags)
    : S_BilinearForm<TSCAL> (afespace, aname, flags) 
  { 
    this->diagonal = 1;
    cout << " !!!!!!!!!!!!!!!!!! allocated diagonal matrix !!!!!!!!!!!!!" << endl;

    if (this->fespace->LowOrderFESpacePtr())
      {
        this->low_order_bilinear_form = 
          make_shared<T_BilinearFormSymmetric<TM>> 
          (this->fespace->LowOrderFESpacePtr(), aname+string(" low-order"), flags);
        this->low_order_bilinear_form -> SetDiagonal (0);
      }
  }

  template <class TM>
  T_BilinearFormDiagonal<TM> :: 
  ~T_BilinearFormDiagonal ()
  {
    /*
    for (int i = 0; i < this->mats.Size(); i++)
      delete this->mats[i];
    */
  }

  ///
  template <class TM>
  void T_BilinearFormDiagonal<TM> :: 
  AllocateMatrix ()
  {
    if (this->mats.Size() == this->ma->GetNLevels())
      return;

    int ndof = this->fespace->GetNDof();
    MatrixGraph * graph = new MatrixGraph (ndof, 1);
    for (int i = 0; i < ndof; i++)
      graph->CreatePosition (i, i);

    // graphs.Append (graph);
    this->mats.Append (make_shared<SparseMatrixSymmetric<TM>> (*graph, 1));
    delete graph;

    if (!this->multilevel || this->low_order_bilinear_form)
      for (int i = 0; i < this->mats.Size()-1; i++)
        {
          /*
          delete this->mats[i];
          this->mats[i] = 0;
          */
          this->mats[i].reset();
        }
  }


  template <class TM>
  shared_ptr<BaseVector> T_BilinearFormDiagonal<TM> :: 
  CreateVector() const
  {
    auto afespace = this->fespace;
#ifdef PARALLEL
    if ( afespace->IsParallel() )
      return make_shared<ParallelVVector<TV_COL>> (afespace->GetNDof(), &afespace->GetParallelDofs());
    else
#endif
      // return new VVector<TV_COL> (afespace->GetNDof());
      return make_shared<VVector<TV_COL>> (afespace->GetNDof());
  }

  ///
  template <class TM>
  void T_BilinearFormDiagonal<TM> :: 
  AddElementMatrix (FlatArray<int> dnums1,
                    FlatArray<int> dnums2,
                    FlatMatrix<TSCAL> elmat,
                    ElementId id, 
                    LocalHeap & lh) 
  {
    TMATRIX & mat = dynamic_cast<TMATRIX&> (*this->mats.Last());

    // #pragma omp critical (addelmat)
    {

      for (int i = 0; i < dnums1.Size(); i++)
        if (dnums1[i] != -1)
          {
            TM & mij = mat(dnums1[i], dnums1[i]);
            int hi = Height (mij);
            int wi = Width (mij);
          
            for (int k = 0; k < hi; k++)
              for (int l = 0; l < wi; l++)
                mij(k,l) += elmat(i*hi+k, i*wi+l);
          }
    }
  }



  ///
  template <> void T_BilinearFormDiagonal<double>::
  AddElementMatrix (FlatArray<int> dnums1,
                    FlatArray<int> dnums2,
                    FlatMatrix<double> elmat,
                    ElementId id, 
                    LocalHeap & lh) 
  {
    TMATRIX & mat = dynamic_cast<TMATRIX&> (GetMatrix());

    // #pragma omp critical (addelmat)
    {
      for (int i = 0; i < dnums1.Size(); i++)
        if (dnums1[i] != -1)
          mat(dnums1[i], dnums1[i]) += elmat(i, i);
    }
  }




  ///
  template <> void T_BilinearFormDiagonal<Complex>::
  AddElementMatrix (FlatArray<int> dnums1,
                    FlatArray<int> dnums2,
                    FlatMatrix<Complex> elmat,
                    ElementId id, 
                    LocalHeap & lh) 
  {
    TMATRIX & mat = dynamic_cast<TMATRIX&> (GetMatrix()); 

    // #pragma omp critical (addelmat)
    {
      for (int i = 0; i < dnums1.Size(); i++)
        if (dnums1[i] != -1)
          mat(dnums1[i], dnums1[i]) += elmat(i, i);
    }
  }



  template <class TM >
  void T_BilinearFormDiagonal<TM>::ApplyElementMatrix(const BaseVector & x,
                                                      BaseVector & y,
                                                      const TSCAL & val,
                                                      const Array<int> & dnums,
                                                      const ElementTransformation & eltrans,
                                                      const int elnum,
                                                      const int type,
                                                      int & cnt,
                                                      LocalHeap & lh,
                                                      const FiniteElement * fel,
                                                      const SpecialElement * sel) const
  {
    typedef typename mat_traits<TM>::TV_ROW TV_ROW;
    typedef typename mat_traits<TM>::TV_COL TV_COL;

    FlatVector<typename mat_traits<TV_ROW>::TSCAL> elvecx (dnums.Size() * this->GetFESpace()->GetDimension(), lh);
    FlatVector<typename mat_traits<TV_COL>::TSCAL> elvecy (dnums.Size() * this->GetFESpace()->GetDimension(), lh);
                      
    x.GetIndirect (dnums, elvecx);

    if(type == 0 || type == 1)
      {
        this->fespace->TransformVec (elnum, (type == 1), elvecx, TRANSFORM_SOL);

        for (int j = 0; j < this->NumIntegrators(); j++)
          {
            BilinearFormIntegrator & bfi = *this->parts[j];
            if (bfi.SkeletonForm()) continue;
            if (type == 0 && bfi.BoundaryForm()) continue;
            if (type == 0 && !bfi.DefinedOn (this->ma->GetElIndex (elnum))) continue;
            if (type == 1 && !bfi.BoundaryForm()) continue;
            
            
            static Timer elementtimer ("Element matrix application");
            elementtimer.Start();
            
            if (this->precompute)
              bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, this->precomputed_data[cnt++], lh);
            else
              bfi.ApplyElementMatrix (*fel, eltrans, elvecx, elvecy, 0, lh);

            elementtimer.Stop();
            
            /*
              testout->precision (12);
              (*testout) << "el " << i << ", dom = " << ma->GetElIndex(i) << ",integrator = " << typeid(bfi).name() << endl
              << "elx = " << elvecx 
              << "ely = " << elvecy << endl;
            */
            BilinearForm::GetFESpace()->TransformVec (elnum, (type == 1), elvecy, TRANSFORM_RHS);
        
            elvecy *= val;
            y.AddIndirect (dnums, elvecy);
          }
      }
    else if (type == 2)
      {
        sel->Apply (elvecx, elvecy, lh);
        elvecy *= val;
        y.AddIndirect (dnums, elvecy);
      }
                      
  }



  template <class TM>
  void T_BilinearFormDiagonal<TM> :: 
  AddDiagElementMatrix (const Array<int> & dnums1,
                        const FlatVector<TSCAL> & diag,
                        bool inner_element, int elnr,
                        LocalHeap & lh) 
  {
    throw Exception ("generic AddDiagElementMatrix not implemented");
    /*
      TMATRIX & mat = dynamic_cast<TMATRIX&> (*this->mats.Last());
    
      for (int i = 0; i < dnums1.Size(); i++)
      if (dnums1[i] != -1)
      {
      TM mij = diag(dnums1[i]);
      int hi = Height (mij);
      int wi = Width (mij);
          
      for (int k = 0; k < hi; k++)
      for (int l = 0; l < wi; l++)
      mij(k,l) += elmat(i*hi+k, i*wi+l);
      }
    */
  }



  ///
  template <> void T_BilinearFormDiagonal<double>::
  AddDiagElementMatrix (const Array<int> & dnums1,
                        const FlatVector<double> & diag,
                        bool inner_element, int elnr,
                        LocalHeap & lh) 
  {
    TMATRIX & mat = dynamic_cast<TMATRIX&> (GetMatrix());

    for (int i = 0; i < dnums1.Size(); i++)
      if (dnums1[i] != -1)
        mat(dnums1[i], dnums1[i]) += diag(i);
  }

  ///
  template <> void T_BilinearFormDiagonal<Complex>::
  AddDiagElementMatrix (const Array<int> & dnums1,
                        const FlatVector<Complex> & diag,
                        bool inner_element, int elnr,
                        LocalHeap & lh) 
  {
    TMATRIX & mat = dynamic_cast<TMATRIX&> (GetMatrix()); 

    for (int i = 0; i < dnums1.Size(); i++)
      if (dnums1[i] != -1)
        mat(dnums1[i], dnums1[i]) += diag(i);
  }




















  template <int CBSIZE>
  shared_ptr<BilinearForm> CreateBilinearForm1 (int cb_size,
                                                shared_ptr<FESpace> space, const string & name, const Flags & flags)
  {
    if (CBSIZE == cb_size)
      return make_shared<T_BilinearFormSymmetric<double, Vec<CBSIZE,Complex>>> (space, name, flags);
    else
      return shared_ptr<BilinearForm> (CreateBilinearForm1<CBSIZE-1> (cb_size, space, name, flags));
  }

  template <> 
  shared_ptr<BilinearForm> CreateBilinearForm1<1> (int cb_size,
                                         shared_ptr<FESpace> space, const string & name, const Flags & flags)
  {
    return make_shared<T_BilinearFormSymmetric<double, Complex>> (space, name, flags);
  }

  template <> 
  shared_ptr<BilinearForm> CreateBilinearForm1<0> (int cb_size,
                                                   shared_ptr<FESpace> space, const string & name, const Flags & flags)
  {
    throw Exception ("Illegal cacheblocksize" + ToString (cb_size));
  }
  

  shared_ptr<BilinearForm> CreateBilinearForm (shared_ptr<FESpace> space,
                                               const string & name,
                                               const Flags & flags)
  {
    BilinearForm * bf = NULL;

    if (flags.GetDefineFlag ("ebe")){
      if ( space->IsComplex() )
        return make_shared<ElementByElement_BilinearForm<Complex>> (space, name, flags);
      else 
        return make_shared<ElementByElement_BilinearForm<double>> (space, name, flags);
    }
    
    if (flags.GetDefineFlag ("symmetric"))
      {

        if (space->IsComplex() && flags.GetDefineFlag ("real"))
          {
            if(flags.NumFlagDefined("cacheblocksize"))
              {
                int cacheblocksize = int(flags.GetNumFlag("cacheblocksize", 1)); 
                /*
#if MAX_CACHEBLOCKS >= 2 
#ifdef GOLD
                if (cacheblocksize == 2)
                  return new T_BilinearFormSymmetric<double,MD<2>> (*space, name, flags);
#endif
#endif
                */
                return CreateBilinearForm1<MAX_CACHEBLOCKS> (cacheblocksize, space, name, flags);
                /*
                  switch(int(flags.GetNumFlag("cacheblocksize",1)))
                  {
                  #if MAX_CACHEBLOCKS >= 2
                  case 2:
                  return new T_BilinearFormSymmetric<double,Vec<2,Complex> > (*space, name, flags);
                  #endif
                  #if MAX_CACHEBLOCKS >= 3
                  case 3:
                  return new T_BilinearFormSymmetric<double,Vec<3,Complex> > (*space, name, flags);
                  case 4:
                  return new T_BilinearFormSymmetric<double,Vec<4,Complex> > (*space, name, flags);
                  #endif
                  #if MAX_CACHEBLOCKS >= 5
                  case 5:
                  return new T_BilinearFormSymmetric<double,Vec<5,Complex> > (*space, name, flags);
                  case 6:
                  return new T_BilinearFormSymmetric<double,Vec<6,Complex> > (*space, name, flags);
                  case 7:
                  return new T_BilinearFormSymmetric<double,Vec<7,Complex> > (*space, name, flags);
                  case 8:
                  return new T_BilinearFormSymmetric<double,Vec<8,Complex> > (*space, name, flags);
                  case 9:
                  return new T_BilinearFormSymmetric<double,Vec<9,Complex> > (*space, name, flags);
                  case 10:
                  return new T_BilinearFormSymmetric<double,Vec<10,Complex> > (*space, name, flags);
                  case 11:
                  return new T_BilinearFormSymmetric<double,Vec<11,Complex> > (*space, name, flags);
                  case 12:
                  return new T_BilinearFormSymmetric<double,Vec<12,Complex> > (*space, name, flags);
                  case 13:
                  return new T_BilinearFormSymmetric<double,Vec<13,Complex> > (*space, name, flags);
                  case 14:
                  return new T_BilinearFormSymmetric<double,Vec<14,Complex> > (*space, name, flags);
                  case 15:
                  return new T_BilinearFormSymmetric<double,Vec<15,Complex> > (*space, name, flags);
                  #endif
                  }
                */
              }
            else
              return make_shared<T_BilinearFormSymmetric<double,Complex>> (space, name, flags);
          }

        if(flags.NumFlagDefined("cacheblocksize"))
          {
            CreateSymMatObject4 (bf, T_BilinearFormSymmetric, 
                                 space->GetDimension(),
                                 int(flags.GetNumFlag("cacheblocksize",1)),
                                 space->IsComplex(),   
                                 space, name, flags);
          }
        else
          {
            //bf = CreateSymMatObject<T_BilinearFormSymmetric, BilinearForm> //, const FESpace, const string, const Flags>
            //  (space->GetDimension(), space->IsComplex(), *space, name, flags);
            
            CreateSymMatObject3(bf, T_BilinearFormSymmetric,
                                space->GetDimension(), space->IsComplex(),
                                space, name, flags);
          }
      }
    else if (flags.GetDefineFlag ("diagonal"))
      {
	/*
        bf = CreateSymMatObject<T_BilinearFormDiagonal, BilinearForm> //, const FESpace, const string, const Flags>
          (space->GetDimension(), space->IsComplex(), *space, name, flags);
	*/
        
        CreateSymMatObject3 (bf, T_BilinearFormDiagonal, 
                             space->GetDimension(), space->IsComplex(),   
                             space, name, flags);
      }
    else
      {
        if (space->IsComplex() && flags.GetDefineFlag ("real"))
          {
            
            if(flags.NumFlagDefined("cacheblocksize"))
              {
                switch(int(flags.GetNumFlag("cacheblocksize",1)))
                  {
#if MAX_CACHEBLOCKS >= 2
                  case 2:
                    return new T_BilinearForm<double,Vec<2,Complex> > (*space, name, flags);
#endif
#if MAX_CACHEBLOCKS >= 3
                  case 3:
                    return new T_BilinearForm<double,Vec<3,Complex> > (*space, name, flags);
                  case 4:
                    return new T_BilinearForm<double,Vec<4,Complex> > (*space, name, flags);
#endif
#if MAX_CACHEBLOCKS >= 5
                  case 5:
                    return new T_BilinearForm<double,Vec<5,Complex> > (*space, name, flags);
                  case 6:
                    return new T_BilinearForm<double,Vec<6,Complex> > (*space, name, flags);
                  case 7:
                    return new T_BilinearForm<double,Vec<7,Complex> > (*space, name, flags);
                  case 8:
                    return new T_BilinearForm<double,Vec<8,Complex> > (*space, name, flags);
                  case 9:
                    return new T_BilinearForm<double,Vec<9,Complex> > (*space, name, flags);
                  case 10:
                    return new T_BilinearForm<double,Vec<10,Complex> > (*space, name, flags);
                  case 11:
                    return new T_BilinearForm<double,Vec<11,Complex> > (*space, name, flags);
                  case 12:
                    return new T_BilinearForm<double,Vec<12,Complex> > (*space, name, flags);
                  case 13:
                    return new T_BilinearForm<double,Vec<13,Complex> > (*space, name, flags);
                  case 14:
                    return new T_BilinearForm<double,Vec<14,Complex> > (*space, name, flags);
                  case 15:
                    return new T_BilinearForm<double,Vec<15,Complex> > (*space, name, flags);
#endif
                  }
              }
            else
              return make_shared<T_BilinearForm<double,Complex>> (space, name, flags);
          }
        
        if(flags.NumFlagDefined("cacheblocksize"))
          {
            CreateSymMatObject4 (bf, T_BilinearForm, 
                                 space->GetDimension(),
                                 int(flags.GetNumFlag("cacheblocksize",1)),
                                 space->IsComplex(),   
                                 space, name, flags);
          }
        else
          {
            //bf = CreateSymMatObject<T_BilinearForm, BilinearForm> 
            //  (space->GetDimension(), space->IsComplex(), *space, name, flags);
            
            
            CreateSymMatObject3 (bf, T_BilinearForm,
                                 space->GetDimension(), space->IsComplex(),
                                 space, name, flags);
          }
      }

    return shared_ptr<BilinearForm> (bf);
  }










  class ApplyFineMatrix : public BaseMatrix
  {
    const BaseMatrix & finemat;
    const ngmg::Prolongation & prol;
    int level;
    
  public:
    ApplyFineMatrix (const BaseMatrix & afinemat,
                     const ngmg::Prolongation & aprol,
                     int alevel);
  
    virtual void Mult (const BaseVector & x, BaseVector & y) const;
    virtual AutoVector CreateVector () const;
  };

  ApplyFineMatrix :: 
  ApplyFineMatrix (const BaseMatrix & afinemat,
                   const ngmg::Prolongation & aprol,
                   int alevel)
    : finemat(afinemat), prol(aprol), level(alevel)
  {
    ;
  }

  void ApplyFineMatrix :: 
  Mult (const BaseVector & x, BaseVector & y) const
  {
    /*
      BaseVector & fx = *finemat.CreateVector();
      BaseVector & fy = *finemat.CreateVector();
  
      fx.SetScalar (0);
      fx.AddPart (1, 1, x);
      //  prol.ProlongateInline (level, fx);
      finemat.Mult (fx, fy);
      //  prol.RestrictInline (level, fy);
      fy.GetPart (1, y);

      delete &fx;
      delete &fy;
    */
    cout << "Apply Matrix currently not implemented" << endl;
  }

  AutoVector ApplyFineMatrix :: CreateVector () const
  {
    cerr << "ApplyFineMatrix::CreateVector:  Need Help !!!" << endl;
    return shared_ptr<BaseVector>();
  }


  void BilinearForm :: GalerkinProjection ()
  {
    auto prol = fespace->GetProlongation();
    SparseMatrix<double>* prolMat = NULL;

    if ( !low_order_bilinear_form )
      for (int i = GetNLevels()-1; i>0; i--)
        {
          prolMat = prol->CreateProlongationMatrix( i );
          
          /*
            GetMatrix( i-1 ) = 
            *( dynamic_cast< const BaseSparseMatrix& >( GetMatrix( i ) ).
            Restrict( *prolMat, &( dynamic_cast< BaseSparseMatrix& >
            ( GetMatrix( i-1 ) ) ) ) );
          */
          mats[i-1] =
            shared_ptr<BaseMatrix>
            ( dynamic_cast< const BaseSparseMatrix& >(GetMatrix(i) ).
              Restrict( *prolMat, &( dynamic_cast< BaseSparseMatrix& >
                                     (GetMatrix( i-1 ) ) ) ) );
          
          delete prolMat;
        }
  }

  
  BilinearFormApplication :: 
  BilinearFormApplication (shared_ptr<BilinearForm> abf)
    : bf (abf)
  {
    ;
  }

  void BilinearFormApplication :: 
  Mult (const BaseVector & v, BaseVector & prod) const
  {
    v.Cumulate();

    prod = 0;
    bf -> AddMatrix (1, v, prod);

    prod.SetParallelStatus (DISTRIBUTED);
  }

  void BilinearFormApplication :: 
  MultAdd (double val, const BaseVector & v, BaseVector & prod) const
  {
    v.Cumulate();
    prod.Distribute();

    bf -> AddMatrix (val, v, prod);
  }

  void BilinearFormApplication :: 
  MultAdd (Complex val, const BaseVector & v, BaseVector & prod) const
  {
    v.Cumulate();
    prod.Distribute();

    bf -> AddMatrix (val, v, prod);
  }

  /*
    void BilinearFormApplication :: 
    MultTransAdd (double val, const BaseVector & v, BaseVector & prod) const 
    {
    bf -> AddMatrix (val, v, prod);
    }
  */


  AutoVector BilinearFormApplication :: 
  CreateVector () const
  {
    return bf -> CreateVector();
  }

  LinearizedBilinearFormApplication ::
  LinearizedBilinearFormApplication (shared_ptr<BilinearForm> abf,
                                     const BaseVector * aveclin)
    : BilinearFormApplication (abf), veclin(aveclin)
  {
    ;
  }

  void  LinearizedBilinearFormApplication :: 
  Mult (const BaseVector & v, BaseVector & prod) const
  {
    prod = 0;
    bf->ApplyLinearizedMatrixAdd (1, *veclin, v, prod);
  }

  void LinearizedBilinearFormApplication :: 
  MultAdd (double val, const BaseVector & v, BaseVector & prod) const
  {
    bf->ApplyLinearizedMatrixAdd (val, *veclin, v, prod);
  }

  void LinearizedBilinearFormApplication :: 
  MultAdd (Complex val, const BaseVector & v, BaseVector & prod) const
  {
    bf->ApplyLinearizedMatrixAdd (val, *veclin, v, prod);
  }




  template <class SCAL>
  ElementByElement_BilinearForm<SCAL> :: 
  ElementByElement_BilinearForm (shared_ptr<FESpace> afespace, const string & aname,
                                 const Flags & flags)
    : S_BilinearForm<SCAL> (afespace, aname, flags)
  { ; }

  template <class SCAL>
  ElementByElement_BilinearForm<SCAL> :: ~ElementByElement_BilinearForm ()
  { ; }



  
  template <class SCAL>
  void ElementByElement_BilinearForm<SCAL> :: AllocateMatrix ()
  {
    auto fespace = this->fespace;
    this->mats.Append (make_shared<ElementByElementMatrix<SCAL>> (fespace->GetNDof(), this->ma->GetNE()+this->ma->GetNSE() ));
  }


  template<class SCAL>
  shared_ptr<BaseVector> ElementByElement_BilinearForm<SCAL> :: CreateVector() const
  {
    return make_shared<VVector<SCAL>> (this->fespace->GetNDof());
  }

  template<class SCAL>
  void ElementByElement_BilinearForm<SCAL> :: 
  AddElementMatrix (FlatArray<int> dnums1,
                    FlatArray<int> dnums2,
                    FlatMatrix<SCAL> elmat,
                    ElementId id,
                    LocalHeap & lh)
  {
    int nr = id.Nr();
    if (id.IsBoundary()) nr += this->ma->GetNE();
    dynamic_cast<ElementByElementMatrix<SCAL>&> (this->GetMatrix()).AddElementMatrix (nr, dnums1, dnums2, elmat);
  }
  

  template class ElementByElement_BilinearForm<double>;
  template class ElementByElement_BilinearForm<Complex>;

  template class T_BilinearForm<double,double>;
}
