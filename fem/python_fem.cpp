#ifdef NGS_PYTHON
#include "../ngstd/python_ngstd.hpp"
#include <fem.hpp>
#include <mutex>
using namespace ngfem;
using ngfem::ELEMENT_TYPE;

struct PythonCoefficientFunction : public CoefficientFunction {
    PythonCoefficientFunction() { }

    virtual double EvaluateXY (double x, double y) const = 0;

    bp::list GetCoordinates(const BaseMappedIntegrationPoint &bip ) {
        double x[3]{0};
        int dim = bip.GetTransformation().SpaceDim();
        const DimMappedIntegrationPoint<1,double> *ip1;
        const DimMappedIntegrationPoint<2,double> *ip2;
        const DimMappedIntegrationPoint<3,double> *ip3;
        switch(dim) {

            case 1:
                ip1 = static_cast<const DimMappedIntegrationPoint<1,double>*>(&bip);
                x[0] = ip1->GetPoint()[0];
                break;
            case 2:
                ip2 = static_cast<const DimMappedIntegrationPoint<2,double>*>(&bip);
                x[0] = ip2->GetPoint()[0];
                x[1] = ip2->GetPoint()[1];
                break;
            case 3:
                ip3 = static_cast<const DimMappedIntegrationPoint<3,double>*>(&bip);
                x[0] = ip3->GetPoint()[0];
                x[1] = ip3->GetPoint()[1];
                x[2] = ip3->GetPoint()[2];
                break;
            default:
                break;
        }
        bp::list list;
        for(int i=0; i<dim; i++)
            list.append(x[i]);

        return list;
    }
};

class PythonCFWrap : public PythonCoefficientFunction , public bp::wrapper<PythonCoefficientFunction> {
    static std::mutex m;
    public:
        PythonCFWrap () : PythonCoefficientFunction() { ; }
        double EvaluateXY (double x, double y) const {
            return this->get_override("EvaluateXY")(x,y);
        }

        double Evaluate (const BaseMappedIntegrationPoint & bip) const {
            double ret = 0;
            m.lock();
            try { 
                ret = this->get_override("Evaluate")(boost::ref(bip)); 
            }
            catch (bp::error_already_set const &) {
                PyErr_Print();
            }
            catch(...) {
                cout << "caught Exception in PythonCoefficientFunction::Evaluate" << endl;
            }
            m.unlock();
            return ret;
        }
};

std::mutex PythonCFWrap::m;


void ExportNgfem() {
    std::string nested_name = "fem";
    if( bp::scope() )
      nested_name = bp::extract<std::string>(bp::scope().attr("__name__") + ".fem");

    bp::object module(bp::handle<>(bp::borrowed(PyImport_AddModule(nested_name.c_str()))));

    cout << "exporting fem as " << nested_name << endl;
    bp::object parent = bp::scope() ? bp::scope() : bp::import("__main__");
    parent.attr("fem") = module ;

    bp::scope ngbla_scope(module);


  bp::enum_<ELEMENT_TYPE>("ET")
    .value("POINT", ET_POINT)     .value("SEGM", ET_SEGM)
    .value("TRIG", ET_TRIG)       .value("QUAD", ET_QUAD)
    .value("TET", ET_TET)         .value("PRISM", ET_PRISM)
    .value("PYRAMID", ET_PYRAMID) .value("HEX", ET_HEX)
    .export_values()
    ;

  bp::class_<FiniteElement, boost::noncopyable>("FiniteElement", bp::no_init)
    .add_property("ndof", &FiniteElement::GetNDof)    
    .add_property("order", &FiniteElement::Order)    
    .add_property("type", &FiniteElement::ElementType)    
    // .add_property("classname", &FiniteElement::ClassName)   // crashes ???
    ;
  
  bp::def("H1FE", FunctionPointer
          ([](ELEMENT_TYPE et, int order)
           {
             FiniteElement * fe = NULL;
             switch (et)
               {
               case ET_TRIG: fe = new H1HighOrderFE<ET_TRIG>(order); break;
               case ET_QUAD: fe = new H1HighOrderFE<ET_QUAD>(order); break;
               case ET_TET: fe = new H1HighOrderFE<ET_TET>(order); break;
               default: cerr << "cannot make fe " << et << endl;
               }
             return fe;
           }),
          bp::return_value_policy<bp::manage_new_object>()
          );

  bp::def("L2FE", FunctionPointer
          ([](ELEMENT_TYPE et, int order)
           {
             FiniteElement * fe = NULL;
             switch (et)
               {
               case ET_TRIG: fe = new L2HighOrderFE<ET_TRIG>(order); break;
               case ET_QUAD: fe = new L2HighOrderFE<ET_QUAD>(order); break;
               case ET_TET: fe = new L2HighOrderFE<ET_TET>(order); break;
               default: cerr << "cannot make fe " << et << endl;
               }
             return fe;
           }),
          bp::return_value_policy<bp::manage_new_object>()
          );
    
  bp::class_<ElementTransformation, boost::noncopyable>("ElementTransformation", bp::no_init)
    .def ("IsBoundary", &ElementTransformation::Boundary)
    .add_property("spacedim", &ElementTransformation::SpaceDim)
    ;
  
  bp::class_<BilinearFormIntegrator, shared_ptr<BilinearFormIntegrator>, boost::noncopyable>
    ("BFI", bp::no_init)
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](string name, int dim, shared_ptr<CoefficientFunction> coef)
                           {
                             auto bfi = GetIntegrators().CreateBFI (name, dim, coef);
                             if (!bfi) cerr << "undefined integrator '" << name 
                                            << "' in " << dim << " dimension having 1 coefficient"
                                            << endl;
                             return bfi;
                           }),
          bp::default_call_policies(),        // need it to use named arguments
          (bp::arg("name")=NULL,bp::arg("dim")=2,bp::arg("coef"))))
    
    .def("CalcElementMatrix", 
         static_cast<void(BilinearFormIntegrator::*) (const FiniteElement&, 
                                                      const ElementTransformation&,
                                                      FlatMatrix<double>,LocalHeap&)const>
         (&BilinearFormIntegrator::CalcElementMatrix))
    ;
  bp::class_<LinearFormIntegrator, shared_ptr<LinearFormIntegrator>, boost::noncopyable>
    ("LFI", bp::no_init)
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](string name, int dim, shared_ptr<CoefficientFunction> coef)
                           {
                             auto lfi = GetIntegrators().CreateLFI (name, dim, coef);
                             if (!lfi) cerr << "undefined integrator '" << name 
                                            << "' in " << dim << " dimension having 1 coefficient"
                                            << endl;
                             return lfi;
                           }),
          bp::default_call_policies(),        // need it to use named arguments
          (bp::arg("name")=NULL,bp::arg("dim")=2,bp::arg("coef"))))

    .def("CalcElementVector", 
         static_cast<void(LinearFormIntegrator::*)(const FiniteElement&, const ElementTransformation&, FlatVector<double>,LocalHeap&)const>
         (&LinearFormIntegrator::CalcElementVector))
    ;

  /*
  bp::def("CreateBFI", FunctionPointer 
          ([](string name, int dim, shared_ptr<CoefficientFunction> coef)
           {
             auto bfi = GetIntegrators().CreateBFI (name, dim, coef);
             if (!bfi) cerr << "undefined integrator '" << name 
                            << "' in " << dim << " dimension having 1 coefficient"
                            << endl;
             return bfi;
           }),
          (bp::arg("name")=NULL,bp::arg("dim")=2,bp::arg("coef")))
    ;
  bp::def("CreateLFI", FunctionPointer
          ([](string name, int dim, shared_ptr<CoefficientFunction> coef)
           {
             auto lfi = GetIntegrators().CreateLFI (name, dim, coef);
             if (!lfi) cerr << "undefined integrator '" << name 
                             << "' in " << dim << " dimension having 1 coefficient"
                             << endl;
             
             return lfi;
           }),
          (bp::arg("name")=NULL,bp::arg("dim")=2,bp::arg("coef")))
    ;
  */
  
  bp::class_<BaseMappedIntegrationPoint, boost::noncopyable>( "BaseMappedIntegrationPoint", bp::no_init);  

  bp::class_<CoefficientFunction, shared_ptr<CoefficientFunction>, boost::noncopyable> 
    ("CoefficientFunction", bp::no_init)
    .def("Evaluate", static_cast<double (CoefficientFunction::*)(const BaseMappedIntegrationPoint &) const>(&CoefficientFunction::Evaluate))
    ;

  bp::class_<ConstantCoefficientFunction,bp::bases<CoefficientFunction>,
    shared_ptr<ConstantCoefficientFunction>, boost::noncopyable>
    ("ConstantCF", bp::init<double>())
    ;
  
  bp::implicitly_convertible 
    <shared_ptr<ConstantCoefficientFunction>, 
    shared_ptr<CoefficientFunction> >(); 

  bp::class_<PythonCFWrap ,bp::bases<CoefficientFunction>, shared_ptr<PythonCFWrap>, boost::noncopyable>("PythonCF", bp::init<>())
//     .def("MyEvaluate", bp::pure_virtual(static_cast<double (PythonCoefficientFunction::*)(double,double) const>(&PythonCoefficientFunction::MyEvaluate))) 
    .def("Evaluate", bp::pure_virtual(static_cast<double (CoefficientFunction::*)(const BaseMappedIntegrationPoint &) const>(&CoefficientFunction::Evaluate))) 
    .def("GetCoordinates", &PythonCoefficientFunction::GetCoordinates)
//     .def("MyEvaluate", bp::pure_virtual(&PythonCoefficientFunction::MyEvaluate)) 
    ;

  bp::implicitly_convertible 
    <shared_ptr<PythonCFWrap>, 
    shared_ptr<CoefficientFunction> >(); 

  
  bp::class_<DomainVariableCoefficientFunction,bp::bases<CoefficientFunction>, 
    shared_ptr<DomainVariableCoefficientFunction>, boost::noncopyable>
    ("VariableCF", bp::no_init)
    .def("__init__", bp::make_constructor 
         (FunctionPointer ([](string str)
                           {
                             // mem-leak -> smart pointer !!!
                             EvalFunction * ef = new EvalFunction (str);
                             return shared_ptr<DomainVariableCoefficientFunction>
                               (new DomainVariableCoefficientFunction (*ef));
                           })))
    ;

  bp::implicitly_convertible
    <shared_ptr<DomainVariableCoefficientFunction>, 
    shared_ptr<CoefficientFunction> >(); 
}


void ExportNgstd();
void ExportNgbla();

BOOST_PYTHON_MODULE(libngfem) {
  // ExportNgstd();
  // ExportNgbla();
  ExportNgfem();
}



#endif
