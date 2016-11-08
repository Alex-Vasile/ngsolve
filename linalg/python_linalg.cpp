#ifdef NGS_PYTHON
#include <la.hpp>
#include "../ngstd/python_ngstd.hpp"
using namespace ngla;

shared_ptr<BaseVector> CreateBaseVector(int size, bool is_complex, int es)
{
  shared_ptr<BaseVector> res;
  if(es > 1)
    {
      if(is_complex)
        res = make_shared<S_BaseVectorPtr<Complex>> (size, es);
      else
        res = make_shared<S_BaseVectorPtr<double>> (size, es);
    }

  if (is_complex)
    res = make_shared<VVector<Complex>> (size);
  else
    res = make_shared<VVector<double>> (size);
  return res;
}

void NGS_DLL_HEADER ExportNgla(py::module &m) {
    cout << IM(1) << "exporting linalg" << endl;
    // TODO
//     py::object expr_module = py::import("ngsolve.__expr");
//     py::object expr_namespace = expr_module.attr("__dict__");


//     struct BaseVector_pickle_suite : py::pickle_suite
//     {
//       static
//       py::tuple getinitargs(const BaseVector & v)
//       {
//         return py::make_tuple(v.Size(), v.IsComplex(), v.EntrySize()); 
//       }
// 
//       static
//       py::tuple getstate(py::object obj)
//       {
//         /*
//         const BaseVector & vec = py::extract<BaseVector const&>(obj)();
//         py::list data;
//         for (int i : Range(vec))
//           data.append (vec.FV<double>()(i));
//         */
//         auto vec = py::extract<BaseVector const&>(obj)().FV<double>();
//         py::list data;
//         for (int i : Range(vec))
//           data.append (vec(i));
//         return py::make_tuple (obj.attr("__dict__"), data);
//       }
//     
//       static
//       void setstate(py::object obj, py::tuple state)
//       {
//         py::dict d = py::extract<py::dict>(obj.attr("__dict__"))();
//         d.update(state[0]);
//         py::list data = py::extract<py::list>(state[1]);
//         auto vec = py::extract<BaseVector const&>(obj)().FV<double>();
//         for (int i : Range(vec))
//           vec(i) = py::extract<double> (data[i]);
//         /*
//         BaseVector & vec = py::extract<BaseVector&>(obj)();
//         for (int i : Range(vec))
//           vec.FV<double>()(i) = py::extract<double> (data[i]);
//         */
//       }
// 
//     static bool getstate_manages_dict() { return true; }
//   };
    
    
  
    typedef PyWrapper<BaseVector> PyBaseVector;
    typedef PyWrapper<BaseMatrix> PyBaseMatrix;
  py::class_<PyBaseVector>(m, "BaseVector",
        py::dynamic_attr() // add dynamic attributes
      )
    .def("__init__", 
         [](PyBaseVector *instance, int size, bool is_complex, int es)
                           {
                             auto vec = CreateBaseVector(size, is_complex, es);
                             new (instance) PyBaseVector(vec);
                           },
          py::arg("size"), py::arg("complex")=false, py::arg("entrysize")=1
          )
  .def("__ngsid__", FunctionPointer( [] ( PyBaseVector & self)
      { return reinterpret_cast<std::uintptr_t>(self.Get().get()); } ) )
    
    .def("__str__", [](PyBaseVector self) { return ToString<BaseVector>(*self.Get()); } )
    .def("__repr__", [](PyBaseVector self) { return "basevector"; } )
    .def_property_readonly("size", py::cpp_function( [] (PyBaseVector self) { return self->Size(); } ) )
    .def("__len__", [] (PyBaseVector self) { return self->Size(); })
    .def("__getstate__", [] (py::object self_object) {
        auto self = self_object.cast<PyBaseVector>();
        auto dict = self_object.attr("__dict__");
        auto vec = self->FV<double>();
        py::list values;
        for (int i : Range(vec))
          values.append (py::cast(vec(i)));
       return py::make_tuple( vec.Size(), self->IsComplex(), self->EntrySize(), values, dict );
     })
    .def("__setstate__", [] (PyBaseVector &self, py::tuple t) {
         auto vec = CreateBaseVector( t[0].cast<int>(), t[1].cast<bool>(), t[2].cast<int>());
         new (&self) PyBaseVector(vec);
         py::object vec_object = py::cast(self);
         auto fvec = vec->FVDouble();
         py::list values = t[3].cast<py::list>();
         for (int i : Range(fvec.Size()))
                fvec[i] = values[i].cast<double>();
         // restore dynamic attributes (necessary for persistent id in NgsPickler)
         vec_object.attr("__dict__") = t[4];
     })
    .def("CreateVector", [] ( PyBaseVector & self) -> PyBaseVector
        { return shared_ptr<BaseVector>(self->CreateVector()); } )

    /*
    .def("Assign", FunctionPointer([](PyBaseVector & self, BaseVector & v2, double s)->void { self->Set(s, v2); }))
    .def("Add", FunctionPointer([](PyBaseVector & self, BaseVector & v2, double s)->void { self->Add(s, v2); }))
    .def("Assign", FunctionPointer([](PyBaseVector & self, BaseVector & v2, Complex s)->void { self->Set(s, v2); }))
    .def("Add", FunctionPointer([](PyBaseVector & self, BaseVector & v2, Complex s)->void { self->Add(s, v2); }))
    */
    .def("Assign",[](PyBaseVector & self, PyBaseVector & v2, py::object s)->void 
                                   { 
                                     if ( py::extract<double>(s).check() )
                                       {
                                         self->Set (py::extract<double>(s)(), *v2);
                                         return;
                                       }
                                     if ( py::extract<Complex>(s).check() )
                                       {
                                         self->Set (py::extract<Complex>(s)(), *v2);
                                         return;
                                       }
                                     throw Exception ("BaseVector::Assign called with non-scalar type");
                                   })
    .def("Add",[](PyBaseVector & self, PyBaseVector & v2, py::object s)->void 
                                   { 
                                     if ( py::extract<double>(s).check() )
                                       {
                                         self->Add (py::extract<double>(s)(), *v2);
                                         return;
                                       }
                                     if ( py::extract<Complex>(s).check() )
                                       {
                                         self->Add (py::extract<Complex>(s)(), *v2);
                                         return;
                                       }
                                     throw Exception ("BaseVector::Assign called with non-scalar type");
                                   })


    // TODO
//     .add_property("expr", py::object(expr_namespace["VecExpr"]) )
//     .add_property("data", py::object(expr_namespace["VecExpr"]), py::object(expr_namespace["expr_data"] ))
//     .def("__add__" , py::object(expr_namespace["expr_add"]) )
//     .def("__sub__" , py::object(expr_namespace["expr_sub"]) )
//     .def("__rmul__" , py::object(expr_namespace["expr_rmul"]) )
    .def("__getitem__",
          [](PyBaseVector & self,  int ind )
           {
             if (ind < 0 || ind >= self->Size()) 
               throw py::index_error();
             int entrysize = self->EntrySize();
             if( self->IsComplex() ) entrysize/=2;
             if(entrysize == 1)
             {
                 if( !self->IsComplex() )
                     return py::cast(self->FVDouble()[ind]);
                 else
                     return py::cast(self->FVComplex()[ind]);
             }
             else
             {
                 // return FlatVector<T>
                 if( self->IsComplex() )
                   return py::cast(self->SV<Complex>()(ind));
                 else
                   return py::cast(self->SV<double>()(ind));
             }
           } )
    .def("__getitem__", [](PyBaseVector & self,  py::slice inds )
      {
          size_t start, step, n;
          InitSlice( inds, self->Size(), start, step, n );
          return shared_ptr<BaseVector>(self->Range(start, start+n));
      } )
    .def("__setitem__", [](PyBaseVector & self,  int ind, double d )
      {
          self->Range(ind,ind+1) = d;
      } )
    .def("__setitem__", [](PyBaseVector & self,  int ind, Complex z ) -> void
      {
        self->Range(ind,ind+1) = z;
      } )
    .def("__setitem__", [](PyBaseVector & self,  py::slice inds, double d )
      {
          size_t start, step, n;
          InitSlice( inds, self->Size(), start, step, n );
          self->Range(start,start+n) = d;
      } )
    .def("__setitem__", [](PyBaseVector & self,  py::slice inds, Complex z )
      {
          size_t start, step, n;
          InitSlice( inds, self->Size(), start, step, n );
          self->Range(start,start+n) = z;
      } )
    .def("__setitem__", [](PyBaseVector & self,  int ind, FlatVector<double> & v )
      {
          if( self->IsComplex() )
            self->SV<Complex>()(ind) = v;
          else
            self->SV<double>()(ind) = v;
      } )
    .def("__setitem__", [](PyBaseVector & self,  int ind, FlatVector<Complex> & v )
      {
          if( self->IsComplex() )
            self->SV<Complex>()(ind) = v;
          else
            throw py::index_error("cannot assign complex values to real vector");
      } )
//     .def(py::self+=py::self)
//     .def(py::self-=py::self)
//     .def(py::self*=double())
    .def("InnerProduct", [](PyBaseVector & self, PyBaseVector & other)
                                          {
                                            if (self->IsComplex())
                                              return py::cast (S_InnerProduct<ComplexConjugate> (*self, *other));
                                            else
                                              return py::cast (InnerProduct (*self, *other));
                                          })
    .def("Norm",  [](PyBaseVector & self) { return self->L2Norm(); })
    .def("Range", [](PyBaseVector & self, int from, int to) -> shared_ptr<BaseVector>
                                   {
                                     return self->Range(from,to);
                                   })
    .def("FV", FunctionPointer( [] (PyBaseVector & self) -> FlatVector<double>
                                {
                                  return self->FVDouble();
                                }))
    ;       

  // m.def("InnerProduct",[](BaseVector & v1, BaseVector & v2)->double { return InnerProduct(v1,v2); })
  m.def ("InnerProduct",
           [] (py::object x, py::object y) -> py::object
                            { return py::handle(x.attr("InnerProduct")) (y); });
  ;
  

  typedef PyBaseMatrix BM;
  // typedef BaseVector BV;

  py::class_<PyBaseMatrix>(m, "BaseMatrix")
    .def("__str__", &ToString<BaseMatrix>)
    .def_property_readonly("height", [] ( PyBaseMatrix & self)
        { return self->Height(); } )
    .def_property_readonly("width", [] ( PyBaseMatrix & self)
        { return self->Width(); } )

    // .def("CreateMatrix", &BaseMatrix::CreateMatrix)
    .def("CreateMatrix", [] ( PyBaseMatrix & self) -> PyBaseMatrix
        { return self->CreateMatrix(); } )

    
    .def("CreateRowVector", [] ( PyBaseMatrix & self) -> PyBaseVector
        { return shared_ptr<BaseVector>(self->CreateRowVector()); } )
    .def("CreateColVector", [] ( PyBaseMatrix & self) -> PyBaseVector
        { return shared_ptr<BaseVector>(self->CreateColVector()); } )

    .def("AsVector", [] (BM & m) -> PyBaseVector
                                      {
                                        return shared_ptr<BaseVector> (&m->AsVector(), NOOP_Deleter);
                                      })
    .def("COO", [] (BM & m) -> py::object
                                 {
                                   SparseMatrix<double> * sp = dynamic_cast<SparseMatrix<double>*> (&m);
                                   if (sp)
                                     {
                                       Array<int> ri, ci;
                                       Array<double> vals;
                                       for (int i = 0; i < sp->Height(); i++)
                                         {
                                           FlatArray<int> ind = sp->GetRowIndices(i);
                                           FlatVector<double> rv = sp->GetRowValues(i);
                                           for (int j = 0; j < ind.Size(); j++)
                                             {
                                               ri.Append (i);
                                               ci.Append (ind[j]);
                                               vals.Append (rv[j]);
                                             }
                                         }

                                       py::object pyri = py::cast(ri);
                                       py::object pyci = py::cast(ci);
                                       py::object pyvals = py::cast(vals);
                                       return py::make_tuple (pyri, pyci, pyvals);
                                     }
				   
				   SparseMatrix<Complex> * spc
				     = dynamic_cast<SparseMatrix<Complex>*> (&m);
				   if (spc)
				     {
					 Array<int> ri, ci;
					 Array<double> vals_real;
					 Array<double> vals_imag;
					 Array<Complex> vals;
					 for (int i = 0; i < spc->Height(); i++)
					   {
					     FlatArray<int> ind = spc->GetRowIndices(i);
					     FlatVector<Complex> rv = spc->GetRowValues(i);
					     for (int j = 0; j < ind.Size(); j++)
					       {
						 ri.Append (i);
						 ci.Append (ind[j]);
						 vals.Append (rv[j]);
					       }
					   }
					 py::object pyri  = py::cast(ri);
					 py::object pyci  = py::cast(ci);
					 py::object pyvals = py::cast(vals);
					 return py::make_tuple (pyri, pyci, pyvals);
				     }
				   
				   throw Exception ("COO needs real or complex-valued sparse matrix");
                                 })

    .def("Mult",        FunctionPointer( [](BM &m, PyBaseVector &x, PyBaseVector &y, double s) { m->Mult (*x,*y); *y *= s; }) )
    .def("MultAdd",     FunctionPointer( [](BM &m, PyBaseVector &x, PyBaseVector &y, double s) { m->MultAdd (s, *x, *y); }))
    // .def("MultTrans",   FunctionPointer( [](BM &m, PyBaseVector &x, PyBaseVector &y, double s) { y  = s*Trans(m)*x; }) )
    // .def("MultTransAdd",FunctionPointer( [](BM &m, PyBaseVector &x, PyBaseVector &y, double s) { y += s*Trans(m)*x; }) )

//     .add_property("expr", py::object(expr_namespace["MatExpr"]) )
//     .def("__mul__" , py::object(expr_namespace["expr_mul"]) )
//     .def("__rmul__" , py::object(expr_namespace["expr_rmul"]) )

    .def("__iadd__", [] (BM &m, BM &m2) { 
        m->AsVector()+=m2->AsVector();
    })

    .def("GetInverseType", [](BM & m)
                                            {
                                              return GetInverseName( m->GetInverseType());
                                            })

    .def("Inverse", [](BM &m, BitArray * freedofs, string inverse)
                                     -> PyBaseMatrix
                                     { 
                                       if (inverse != "") m->SetInverseType(inverse);
                                       return m->InverseMatrix(freedofs);
                                     }
         ,"Inverse", py::arg("freedofs"), py::arg("inverse")=py::str("")
         )
    .def("Inverse", [](BM &m)-> PyBaseMatrix
                                     { return m->InverseMatrix(); })
    .def("Transpose", [](BM &m)-> PyBaseMatrix
                                       { return make_shared<Transpose> (*m); })
    // py::return_value_policy<py::manage_new_object>())
    ;

  typedef PyWrapper<Projector> PyProjector;
  py::class_<PyProjector, PyBaseMatrix> (m, "Projector")
  .def("__init__", []( PyProjector *instance, const BitArray &array, bool b ) {
      new (instance) PyProjector(make_shared<Projector>(array, b));
      })
    ;

  py::class_<PyWrapper<CGSolver<double>> ,PyBaseMatrix> (m, "CGSolverD")
    .def("GetSteps", &CGSolver<double>::GetSteps)
    ;

  py::class_<PyWrapper<CGSolver<Complex>> ,PyBaseMatrix> (m, "CGSolverC")
    .def("GetSteps", &CGSolver<Complex>::GetSteps)
    ;

  m.def("CGSolver", [](const PyBaseMatrix & mat, const PyBaseMatrix & pre,
                                          bool iscomplex, bool printrates, 
                                          double precision, int maxsteps) -> PyBaseMatrix 
                                       {
                                         KrylovSpaceSolver * solver;
                                         if(mat->IsComplex()) iscomplex = true;
                                         
                                         if (iscomplex)
                                           solver = new CGSolver<Complex> (*mat, *pre);
                                         else
                                           solver = new CGSolver<double> (*mat, *pre);
                                         solver->SetPrecision(precision);
                                         solver->SetMaxSteps(maxsteps);
                                         solver->SetPrintRates (printrates);
                                         return solver;
                                       },
          "CG Solver", py::arg("mat"), py::arg("pre"), py::arg("complex") = false, py::arg("printrates")=true,
           py::arg("precision")=1e-8, py::arg("maxsteps")=200
//           py::return_value_policy<py::manage_new_object>()
          )
    ;

  py::class_<PyWrapper<GMRESSolver<double>>, PyBaseMatrix> (m, "GMRESSolverD")
    ;
  py::class_<PyWrapper<GMRESSolver<Complex>>, PyBaseMatrix> (m, "GMRESSolverC")
    ;

  m.def("GMRESSolver", [](const PyBaseMatrix & mat, const PyBaseMatrix & pre,
                                           bool printrates, 
                                           double precision, int maxsteps) -> PyBaseMatrix
                                        {
                                          KrylovSpaceSolver * solver;
                                          if (!mat->IsComplex())
                                            solver = new GMRESSolver<double> (*mat, *pre);
                                          else
                                            solver = new GMRESSolver<Complex> (*mat, *pre);                                            
                                          solver->SetPrecision(precision);
                                          solver->SetMaxSteps(maxsteps);
                                          solver->SetPrintRates (printrates);
                                          return solver;
                                        },
          "GMRES Solver", py::arg("mat"), py::arg("pre"), py::arg("printrates")=true,
           py::arg("precision")=1e-8, py::arg("maxsteps")=200
          )
    ;
  
  py::class_<PyWrapper<QMRSolver<double>>, PyBaseMatrix> (m, "QMRSolverD")
    ;
  py::class_<PyWrapper<QMRSolver<Complex>>, PyBaseMatrix> (m, "QMRSolverC")
    ;

  m.def("QMRSolver", [](const PyBaseMatrix & mat, const PyBaseMatrix & pre,
                                           bool printrates, 
                                           double precision, int maxsteps) -> PyBaseMatrix
                                        {
                                          KrylovSpaceSolver * solver;
                                          if (!mat->IsComplex())
                                            solver = new QMRSolver<double> (*mat, *pre);
                                          else
                                            solver = new QMRSolver<Complex> (*mat, *pre);                                            
                                          solver->SetPrecision(precision);
                                          solver->SetMaxSteps(maxsteps);
                                          solver->SetPrintRates (printrates);
                                          return solver;
                                        },
          "QMR Solver", py::arg("mat"), py::arg("pre"), py::arg("printrates")=true,
           py::arg("precision")=1e-8, py::arg("maxsteps")=200
          )
    ;
  
  m.def("ArnoldiSolver", [](PyBaseMatrix & mata, PyBaseMatrix & matm, const BitArray & freedofs,
                                               py::list vecs, py::object bpshift)
                                            {
                                              if (mata->IsComplex())
                                                {
                                                  Arnoldi<Complex> arnoldi (*mata, *matm, &freedofs);
                                                  Complex shift = 0.0;
//                                                   if (py::cast<Complex>(bpshift).check())
                                                    shift = py::cast<Complex>(bpshift);
                                                  cout << "shift = " << shift << endl;
                                                  arnoldi.SetShift (shift);
                                                  
                                                  int nev = py::len(vecs);
                                                  cout << "num vecs: " << nev << endl;
                                                  Array<shared_ptr<BaseVector>> evecs(nev);
                                                  
                                                  Array<Complex> lam(nev);
                                                  arnoldi.Calc (2*nev+1, lam, nev, evecs, 0);
                                            
                                                  for (int i = 0; i < nev; i++)
                                                    * py::extract<shared_ptr<BaseVector>>(vecs[i])() = *evecs[i];

                                                  Vector<Complex> vlam(nev);
                                                  for (int i = 0; i < nev; i++)
                                                    vlam(i) = lam[i];
                                                  return vlam;
                                                }
                                              
                                              cout << "real Arnoldi not supported" << endl;
                                              Vector<Complex> lam(5);
                                              return lam;
                                            },
          "Arnoldi Solver", py::arg("mata"), py::arg("matm"), py::arg("freedofs"), py::arg("vecs"), py::arg("shift")=DummyArgument()
          )
    ;

  

  m.def("DoArchive" , [](shared_ptr<Archive> & arch, PyBaseMatrix & mat) 
                                         { cout << "output basematrix" << endl;
                                           mat->DoArchive(*arch); return arch; });
                                           
}



PYBIND11_PLUGIN(libngla) {
  py::module m("la", "pybind la");
  ExportNgla(m);
  return m.ptr();
}




#endif // NGS_PYTHON
