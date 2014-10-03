#ifdef NGS_PYTHON
#include "../ngstd/python_ngstd.hpp"
#include <comp.hpp>
using namespace ngcomp;



template <> class cl_NonElement<ElementId>
{
public:
  static ElementId Val() { return ElementId(VOL,-1); }
};


class PyNumProc : public NumProc
{

public:
  PyNumProc (PDE & pde, const Flags & flags) : NumProc (pde, flags) { ; }
  shared_ptr<PDE> GetPDE() const { return shared_ptr<PDE> (&pde,&NOOP_Deleter); }
  // virtual void Do (LocalHeap & lh) { ; }
};

class NumProcWrap : public PyNumProc, public bp::wrapper<PyNumProc> {
public:
  NumProcWrap (PDE & pde, const Flags & flags) : PyNumProc(pde, flags) { ; }
  void Do(LocalHeap & lh)  {
    // cout << "numproc wrap - do" << endl;
    AcquireGIL gil_lock;
    this->get_override("Do")(lh);
  }
};




void ExportNgcomp() 
{
  bp::docstring_options local_docstring_options(true, true, false);
  
  std::string nested_name = "comp";
  if( bp::scope() )
    nested_name = bp::extract<std::string>(bp::scope().attr("__name__") + ".comp");
  
  bp::object module(bp::handle<>(bp::borrowed(PyImport_AddModule(nested_name.c_str()))));
  
  cout << "exporting comp as " << nested_name << endl;
  bp::object parent = bp::scope() ? bp::scope() : bp::import("__main__");
  parent.attr("comp") = module;
  
  bp::scope local_scope(module);

  //////////////////////////////////////////////////////////////////////////////////////////

  bp::enum_<VorB>("VorB")
    .value("VOL", VOL)
    .value("BND", BND)
    .export_values()
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  bp::enum_<COUPLING_TYPE> ("COUPLING_TYPE")
    .value("UNUSED_DOF", UNUSED_DOF)
    .value("LOCAL_DOF", LOCAL_DOF)
    .value("INTERFACE_DOF", INTERFACE_DOF)
    .value("NONWIREBASKET_DOF", NONWIREBASKET_DOF)
    .value("WIREBASKET_DOF", WIREBASKET_DOF)
    .value("EXTERNAL_DOF", EXTERNAL_DOF)
    .value("ANY_DOF", ANY_DOF)
    // .export_values()
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  bp::class_<ElementId> ("ElementId", 
                         "an element identifier containing element number and Volume/Boundary flag",
                         bp::no_init)
    .def(bp::init<VorB,int>())
    .def("__str__", &ToString<ElementId>)
    .add_property("nr", &ElementId::Nr, "the element number")    
    .def("IsVolume", &ElementId::IsVolume, "is it a boundary element ?")
    .def("IsBoundary", &ElementId::IsBoundary, "is it a volume element ?")
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  bp::class_<ElementRange,bp::bases<IntRange>> ("ElementRange",bp::init<VorB,IntRange>())
    .def(PyDefIterable2<ElementRange>())
    // .def("__iter__", bp::iterator<ElementRange>())
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  bp::class_<Ngs_Element>("Ngs_Element", bp::no_init)
    .add_property("vertices", FunctionPointer([](Ngs_Element &el) {return bp::tuple(Array<int>(el.Vertices()));} ))
    .add_property("edges", FunctionPointer([](Ngs_Element &el) { return bp::tuple(Array<int>(el.Edges()));} ))
    .add_property("faces", FunctionPointer([](Ngs_Element &el) { return bp::tuple(Array<int>(el.Faces()));} ))
    .add_property("type", &Ngs_Element::GetType)
    ;


  //////////////////////////////////////////////////////////////////////////////////////////

  bp::class_<MeshAccess>("Mesh", 
                         "the mesh", 
                         bp::init<string>())
    .def(bp::init<shared_ptr<netgen::Mesh>>())

    .def("LoadMesh", static_cast<void(MeshAccess::*)(const string &)>(&MeshAccess::LoadMesh),
         "Load mesh from file")
    
    .def("Elements", static_cast<ElementRange(MeshAccess::*)(VorB)const> (&MeshAccess::Elements),
         (bp::arg("VOL_or_BND")=VOL))

    .def("GetElement", static_cast<Ngs_Element(MeshAccess::*)(ElementId)const> (&MeshAccess::GetElement))
    .def("__getitem__", static_cast<Ngs_Element(MeshAccess::*)(ElementId)const> (&MeshAccess::operator[]))

    .def ("GetNE", static_cast<int(MeshAccess::*)(VorB)const> (&MeshAccess::GetNE))
    .add_property ("nv", &MeshAccess::GetNV, "number of vertices")
    .add_property ("ne", static_cast<int(MeshAccess::*)()const> (&MeshAccess::GetNE))

    .def ("GetTrafo", 
          static_cast<ElementTransformation&(MeshAccess::*)(ElementId,LocalHeap&)const>
          (&MeshAccess::GetTrafo), 
          bp::return_value_policy<bp::reference_existing_object>())

    // first attempts, but keep for a moment ...
    .def("GetElement", static_cast< Ngs_Element (MeshAccess::*)(int, bool)const> (&MeshAccess::GetElement),
         (bp::arg("arg1")=NULL,bp::arg("arg2")=0))
    ;


  //////////////////////////////////////////////////////////////////////////////////////////
  
  bp::class_<NGS_Object, shared_ptr<NGS_Object>,  boost::noncopyable>("NGS_Object", bp::no_init)
    .add_property("name", FunctionPointer
                  ([](const NGS_Object & self)->string { return self.GetName();}))
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  bp::class_<FESpace, shared_ptr<FESpace>,  boost::noncopyable>("FESpace", bp::no_init)
    .def("__init__", bp::make_constructor 
         (FunctionPointer ([](const string & type, const MeshAccess & ma, const Flags & flags)
                           { return CreateFESpace (type, ma, flags); } )))

    .def("Update", FunctionPointer([](FESpace & self, int heapsize)
                                   { 
                                     LocalHeap lh (heapsize, "FESpace::Update-heap");
                                     self.Update(lh);
                                     self.FinalizeUpdate(lh);
                                   }),
         (bp::arg("self")=NULL,bp::arg("heapsize")=1000000))

    .add_property ("ndof", FunctionPointer([](FESpace & self) { return self.GetNDof(); }))
    .def("__str__", &ToString<FESpace>)
    
    .def("GetDofNrs", FunctionPointer([](FESpace & self, ElementId ei) 
                                   {
                                     Array<int> tmp; self.GetDofNrs(ei,tmp); 
                                     return bp::tuple (tmp); 
                                   }))

    .def("CouplingType", &FESpace::GetDofCouplingType)

    // Define function instead of property because the python autocomplete package (rlcompleter) tries to evaluate properties -> lock in mpi function
    .def("ndofglobal", FunctionPointer([](FESpace & self) { return self.GetNDofGlobal(); }))

    .def ("GetFE", 
          static_cast<const FiniteElement&(FESpace::*)(ElementId,LocalHeap&)const>
          (&FESpace::GetFE), 
          bp::return_value_policy<bp::reference_existing_object>())

    .def("FreeDofs", FunctionPointer
         ( [] (const FESpace &self, bool coupling) -> const BitArray &{ return *self.GetFreeDofs(coupling); } ),
         bp::return_value_policy<bp::reference_existing_object>(),
         (bp::arg("self"), bp::arg("coupling")=0))
    ;
  
  bp::class_<CompoundFESpace, shared_ptr<CompoundFESpace>, bp::bases<FESpace>, boost::noncopyable>
    ("CompoundFESpace", bp::no_init)
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  typedef GridFunction GF;
  bp::class_<GF, shared_ptr<GF>, boost::noncopyable>
    ("GridFunction",  "a field approximated in some finite element space", bp::no_init)

    .def("__init__", bp::make_constructor
         (FunctionPointer ([](shared_ptr<FESpace> fespace, string name)
                           {
                             Flags flags;
                             return CreateGridFunction (fespace, name, flags);
                           }),
          bp::default_call_policies(),        // need it to use arguments
          (bp::arg("space"), bp::arg("name")="gfu")),
         "creates a gridfunction in finite element space"
         )

    .def("__str__", &ToString<GF>)
    .add_property("space", &GF::FESpacePtr, "the finite element spaces")

    .def("Update", FunctionPointer ([](GF & self) { self.Update(); }),
         "update vector size to finite element space dimension after mesh refinement")
    
    .def("Set", FunctionPointer
         ([](GF & self, shared_ptr<CoefficientFunction> cf)
          {
            LocalHeap lh(1000000, "tmplh");
            SetValues (cf, self, false, NULL, lh);
          }))

    /*
    .def("Vector", 
         FunctionPointer([](GF & self) { return self.VectorPtr(); }),
         "vector of coefficients"
         )
    */
    
    .add_property("vec",
                  FunctionPointer([](GF & self) { return self.VectorPtr(); }),
                  "vector of coefficients of first multidim dimension")

    .add_property("vecs", FunctionPointer
                  ([](GF & self)-> bp::list 
                   { 
                     bp::list vecs;
                     for (int i = 0; i < self.GetMultiDim(); i++) 
                       vecs.append(self.VectorPtr(i));
                     return vecs;
                   }),
                  "list of vectors of coefficients")
    
    .def("__call__", FunctionPointer
         ([](GF & self, double x, double y, double z)
          {
            auto space = self.FESpacePtr();
            auto evaluator = space->GetEvaluator();
            LocalHeap lh(10000, "ngcomp::GridFunction::Eval");

            IntegrationPoint ip;
            int elnr = space->GetMeshAccess().FindElementOfPoint(Vec<3>(x, y, z), ip, false);
            if (elnr < 0) throw Exception ("point out of domain");

            const FiniteElement & fel = space->GetFE(elnr, lh);

            Array<int> dnums(fel.GetNDof(), lh);
            space->GetDofNrs(elnr, dnums);

            if (space->IsComplex())
              {
                Vector<Complex> elvec(fel.GetNDof()*space->GetDimension());
                Vector<Complex> values(evaluator->Dim());
                self.GetElementVector(dnums, elvec);

                auto & trafo = space->GetMeshAccess().GetTrafo(elnr, false, lh);
                evaluator->Apply(fel, trafo(ip, lh), elvec, values, lh);

                return (values.Size() > 1) ? bp::object(values) : bp::object(values(0));
              }
            else
              {
                Vector<> elvec(fel.GetNDof()*space->GetDimension());
                Vector<> values(evaluator->Dim());

                self.GetElementVector(dnums, elvec);
                int dim_mesh = space->GetMeshAccess().GetDimension();
                if (dim_mesh == 2)
                  {
                    MappedIntegrationPoint<2, 2> mip(ip, space->GetMeshAccess().GetTrafo(elnr, false, lh));
                    evaluator->Apply(fel, mip, elvec, values, lh);
                  }
                else if (dim_mesh == 3)
                  {
                    MappedIntegrationPoint<3, 3> mip(ip, space->GetMeshAccess().GetTrafo(elnr, false, lh));
                    evaluator->Apply(fel, mip, elvec, values, lh);
                  }
                if (values.Size() > 1)
                  return bp::object(values);
                else
                  return bp::object(values(0));
              }
          }
          ), (bp::arg("self"), bp::arg("x") = 0.0, bp::arg("y") = 0.0, bp::arg("z") = 0.0))


    .def("D", FunctionPointer
         ([](GF & self, const double &x, const double &y, const double &z)
          {
            const FESpace & space = self.GetFESpace();
            IntegrationPoint ip;
            int dim_mesh = space.GetMeshAccess().GetDimension();
            auto evaluator = space.GetFluxEvaluator();
            cout << evaluator->Name() << endl;
            int dim = evaluator->Dim();
            LocalHeap lh(10000, "ngcomp::GridFunction::Eval");
            int elnr = space.GetMeshAccess().FindElementOfPoint(Vec<3>(x, y, z), ip, false);
            Array<int> dnums;
            space.GetDofNrs(elnr, dnums);
            const FiniteElement & fel = space.GetFE(elnr, lh);
            if (space.IsComplex())
              {
                Vector<Complex> elvec;
                Vector<Complex> values(dim);
                elvec.SetSize(fel.GetNDof());
                self.GetElementVector(dnums, elvec);
                if (dim_mesh == 2)
                  {
                    MappedIntegrationPoint<2, 2> mip(ip, space.GetMeshAccess().GetTrafo(elnr, false, lh));
                    evaluator->Apply(fel, mip, elvec, values, lh);
                  }
                else if (dim_mesh == 3)
                  {
                    MappedIntegrationPoint<3, 3> mip(ip, space.GetMeshAccess().GetTrafo(elnr, false, lh));
                    evaluator->Apply(fel, mip, elvec, values, lh);
                  }
                if (dim > 1)
                  return bp::object(values);
                else
                  return bp::object(values(0));
              }
            else
              {
                Vector<> elvec;
                Vector<> values(dim);
                elvec.SetSize(fel.GetNDof());
                self.GetElementVector(dnums, elvec);
                if (dim_mesh == 2)
                  {
                    MappedIntegrationPoint<2, 2> mip(ip, space.GetMeshAccess().GetTrafo(elnr, false, lh));
                    evaluator->Apply(fel, mip, elvec, values, lh);
                  }
                else if (dim_mesh == 3)
                  {
                    MappedIntegrationPoint<3, 3> mip(ip, space.GetMeshAccess().GetTrafo(elnr, false, lh));
                    evaluator->Apply(fel, mip, elvec, values, lh);
                  }
                if (dim > 1)
                  return bp::object(values);
                else
                  return bp::object(values(0));
              }
          }
          ), (bp::arg("self"), bp::arg("x") = 0.0, bp::arg("y") = 0.0, bp::arg("z") = 0.0))
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  typedef BilinearForm BF;
  bp::class_<BF, shared_ptr<BF>, boost::noncopyable>("BilinearForm", bp::no_init)
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](shared_ptr<FESpace> fespace, string name, const Flags & flags) 
                           { return CreateBilinearForm (fespace, name, flags); }),
          bp::default_call_policies(),        // need it to use arguments
          (bp::arg("space"), bp::arg("name")="bfa", bp::arg("flags") = bp::dict())))

    .def("__str__", &ToString<BF>)

    .def("Add", FunctionPointer([](BF & self, shared_ptr<BilinearFormIntegrator> bfi) -> BF&
                                { self.AddIntegrator (bfi); return self; }),
         bp::return_value_policy<bp::reference_existing_object>(),
         "add integrator to bilinear-form")

    .def("Assemble", FunctionPointer([](BF & self, int heapsize)
                                     {
                                       LocalHeap lh (heapsize, "BiinearForm::Assemble-heap");
                                       self.Assemble(lh);
                                     }),
         (bp::arg("self")=NULL,bp::arg("heapsize")=1000000))

    .add_property("mat", &BilinearForm::MatrixPtr)
    // .def("Matrix", FunctionPointer([](const BF & self) { return self.MatrixPtr(); }))
    .def("Energy", &BilinearForm::Energy)
    .def("Apply", &BilinearForm::ApplyMatrix)
    .def("AssembleLinearization", FunctionPointer
	 ([](BF & self, BaseVector & ulin, int heapsize)
	  {
	    LocalHeap lh (heapsize, "BiinearForm::Assemble-heap");
	    self.AssembleLinearization (ulin, lh);
	  }),
         (bp::arg("self")=NULL,bp::arg("ulin"),bp::arg("heapsize")=1000000))
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  typedef LinearForm LF;
  bp::class_<LF, shared_ptr<LF>, boost::noncopyable>("LinearForm", bp::no_init)
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](shared_ptr<FESpace> fespace, string name, Flags flags) // -> shared_ptr<LinearForm>
                           {
                             return CreateLinearForm (fespace, name, flags);
                           }),
          bp::default_call_policies(),        // need it to use arguments
          (bp::arg("space"), bp::arg("name")="lff", bp::arg("flags") = bp::dict()))
         )
    .def("__str__", &ToString<LF>)

    .add_property("vec", &LinearForm::VectorPtr)

    .def("Add", FunctionPointer
         ([](LF & self, shared_ptr<LinearFormIntegrator> lfi) -> LF&
          { self.AddIntegrator (lfi); return self; }),
         bp::return_value_policy<bp::reference_existing_object>())

    .def("Assemble", FunctionPointer
         ([](LF & self, int heapsize)
          {
            LocalHeap lh (heapsize, "LinearForm::Assemble-heap");
            self.Assemble(lh);
          }),
         (bp::arg("self")=NULL,bp::arg("heapsize")=1000000))
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  typedef Preconditioner PRE;
  bp::class_<PRE, shared_ptr<PRE>, boost::noncopyable>("Preconditioner", bp::no_init)
    .add_property("mat", FunctionPointer
                  ([](shared_ptr<Preconditioner> self) 
                   {
                     return shared_ptr<BaseMatrix> (const_cast<BaseMatrix*> (&self->GetMatrix()),
                                                    NOOP_Deleter);
                   }))
    ;

  //////////////////////////////////////////////////////////////////////////////////////////

  bp::class_<NumProc, shared_ptr<NumProc>,bp::bases<NGS_Object>,boost::noncopyable> ("NumProc", bp::no_init)
    ;

  // die geht
  bp::class_<NumProcWrap,bp::bases<NumProc>,boost::noncopyable>("PyNumProc", bp::init<PDE&, const Flags&>())
    .def("Do", bp::pure_virtual(&PyNumProc::Do)) 
    .add_property("pde", &PyNumProc::GetPDE)
    ;
  

  //////////////////////////////////////////////////////////////////////////////////////////

  PyExportSymbolTable<shared_ptr<FESpace>> ();
  PyExportSymbolTable<shared_ptr<CoefficientFunction>> ();
  PyExportSymbolTable<shared_ptr<GridFunction>> ();
  PyExportSymbolTable<shared_ptr<BilinearForm>> ();
  PyExportSymbolTable<shared_ptr<LinearForm>> ();
  PyExportSymbolTable<shared_ptr<Preconditioner>> ();
  PyExportSymbolTable<shared_ptr<NumProc>> ();
  PyExportSymbolTable<double> ();
  PyExportSymbolTable<shared_ptr<double>> ();
  
  bp::class_<PDE,shared_ptr<PDE>> ("PDE", bp::init<>())

    .def(bp::init<const string&>())

    .def("Load", static_cast<void(PDE::*)(const string &, const bool, const bool)> 
         (&PDE::LoadPDE),
         (boost::python::arg("filename"), 
          boost::python::arg("meshload")=0, 
          boost::python::arg("nogeometryload")=0))

    .def("Mesh",  static_cast<MeshAccess&(PDE::* const)(int)>(&PDE::GetMeshAccess),
         bp::return_value_policy<bp::reference_existing_object>(),
         (bp::arg("nr")=0))

    .def("Solve", &PDE::Solve)

    .def("Add", FunctionPointer([](PDE & self, shared_ptr<FESpace> space)
                                {
                                  self.AddFESpace (space->GetName(), space);
                                }))

    .def("Add", FunctionPointer([](PDE & self, shared_ptr<GridFunction> gf)
                                {
                                  self.AddGridFunction (gf->GetName(), gf);
                                }))

    .def("Add", FunctionPointer([](PDE & self, shared_ptr<BilinearForm> bf)
                                {
                                  self.AddBilinearForm (bf->GetName(), bf);
                                }))

    .def("Add", FunctionPointer([](PDE & self, PyNumProc &np)
                                {
                                  cout << "add pynumproc - ref" << endl;
                                  self.AddNumProc ("pynumproc", shared_ptr<NumProc> (&np, &NOOP_Deleter));
                                }))
    
    .def("Add", FunctionPointer([](PDE & self, shared_ptr<NumProc> np)
                                {
                                  cout << "add numproc - sp" << endl;
                                  self.AddNumProc ("pynumproc", np);
                                }))

    .def("Add", FunctionPointer([](PDE & self, const bp::list &l)
                                {
                                  for (int i=0; i<bp::len(l); i++)
                                    {
                                      bp::extract<shared_ptr<PyNumProc>> np(l[i]);
                                      if(np.check())
                                        {
                                          self.AddNumProc (np()->GetName(), np());
                                          continue;
                                        }
                                      
                                      bp::extract<shared_ptr<NumProc>> pnp(l[i]);
                                      if(np.check())
                                        {
                                          self.AddNumProc (pnp()->GetName(), pnp());
                                          continue;
                                        }
                                      
                                      bp::extract<shared_ptr<GridFunction>> gf(l[i]);
                                      if(gf.check())
                                        {
                                          self.AddGridFunction (gf()->GetName(), gf());
                                          continue;
                                        }
                                      
                                      cout << "warning: unknown object at position " << i << endl;
                                    }
                                }))
    
    .add_property ("constants", FunctionPointer([](PDE & self) { return bp::object(self.GetConstantTable()); }))
    .add_property ("variables", FunctionPointer([](PDE & self) { return bp::object(self.GetVariableTable()); }))
    .add_property ("coefficients", FunctionPointer([](PDE & self) { return bp::object(self.GetCoefficientTable()); }))
    .add_property ("spaces", FunctionPointer([](PDE & self) { return bp::object(self.GetSpaceTable()); }))
    .add_property ("gridfunctions", FunctionPointer([](PDE & self) { return bp::object(self.GetGridFunctionTable()); }))
    .add_property ("bilinearforms", FunctionPointer([](PDE & self) { return bp::object(self.GetBilinearFormTable()); }))
    .add_property ("linearforms", FunctionPointer([](PDE & self) { return bp::object(self.GetLinearFormTable()); }))
    .add_property ("preconditioners", FunctionPointer([](PDE & self) { return bp::object(self.GetPreconditionerTable()); }))
    .add_property ("numprocs", FunctionPointer([](PDE & self) { return bp::object(self.GetNumProcTable()); }))
    ;
  
  
}





BOOST_PYTHON_MODULE(libngcomp) 
{
  ExportNgcomp();
}



#endif // NGS_PYTHON
