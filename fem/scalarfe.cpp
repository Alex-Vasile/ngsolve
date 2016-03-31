/*********************************************************************/
/* File:   scalarfe.cpp                                              */
/* Author: Joachim Schoeberl                                         */
/* Date:   25. Mar. 2000                                             */
/*********************************************************************/


/* 
   Finite Element Definitions
*/

#define FILE_SCALARFE_CPP


#include <fem.hpp>
#include "h1lofe.hpp"
#include "l2hofe.hpp"

namespace ngfem
{

  /*
  template <int D>
  ScalarFiniteElement<D> :: ~ScalarFiniteElement () 
  { ; }
  */

  template <int D>
  string ScalarFiniteElement<D> :: ClassName() const 
  {
    return "ScalarFiniteElement"; 
  }

  template <int D>
  void ScalarFiniteElement<D> :: 
  GetPolOrders (FlatArray<PolOrder<D> > orders) const
  {
#ifndef __CUDA_ARCH__
    throw Exception (string ("GetPolOrders not implemnted for element") + ClassName());
#endif
  }



#ifdef UNSUPPORTED
  template <int D>
  void ScalarFiniteElement<D> ::
  CalcDShape (const IntegrationPoint & ip, 
	      FlatMatrixFixWidth<D> dshape) const
  
  {
    static bool firsttime = true;
    if (firsttime)
      {
	cout << "WARNING: CalcDShape not overloaded for class, using numerical differentiation " << typeid(this).name() << ", ndof = " << ndof << endl;
        firsttime = false;
      }
    
    int nd = GetNDof();
    int sdim = D;

    double eps = 2e-5;
    ArrayMem<double, 100> hm1(nd), hm2(nd), hm3(nd), hm4(nd);
    FlatVector<> 
      shape1(nd, &hm1[0]), 
      shape2(nd, &hm2[0]), 
      shape3(nd, &hm3[0]), 
      shape4(nd, &hm4[0]);

    for (int i = 0; i < sdim; i++)
      {
	IntegrationPoint ip1 = ip;
	IntegrationPoint ip2 = ip;
        ip1(i) -= eps;
        ip2(i) += eps;
	CalcShape (ip1, shape1);
	CalcShape (ip2, shape2);

        ip1(i) -= eps;
        ip2(i) += eps;
	CalcShape (ip1, shape3);
	CalcShape (ip2, shape4);

	for (int j = 0; j < nd; j++)
	  dshape(j, i) = 
	    2/(3*eps) * (shape2(j) - shape1(j)) 
	    -1/(12*eps) * (shape4(j) - shape3(j));
      }
  }
#endif 
 
  /*
    a ( eps - (-eps) ) + b ( 2 eps - (-2eps) )  = 1
    a ( eps^3 - (-eps)^3) + b ( 8 eps^3 - -8 eps^3 ) = 0
    
    2 a + 4 b = 1 / eps
    2 a + 16 b = 0  

    b = -1 / 12 eps
    a = 2 / 3 eps
  */

  /*
  /// compute dshape, matrix: ndof x spacedim
  template<int D>
  void ScalarFiniteElement<D> :: 
  CalcMappedDShape (const MappedIntegrationPoint<D,D> & mip, 
                    FlatMatrixFixWidth<D> dshape) const
  {
    CalcDShape (mip.IP(), dshape);
    for (int i = 0; i < dshape.Height(); i++)
      {
        Vec<D> hv = dshape.Row(i);
        dshape.Row(i) = Trans (mip.GetJacobianInverse ()) * hv;
      }
  }
  */

  template <int D>
  void ScalarFiniteElement<D> :: 
  CalcShape (const IntegrationRule & ir, 
	     SliceMatrix<> shape) const
  {
    for (int i = 0; i < ir.Size(); i++)
      CalcShape (ir[i], shape.Col(i));
  }


  template<int D>
  void ScalarFiniteElement<D> :: 
  CalcMappedDShape (const MappedIntegrationPoint<D,D> & mip, 
                    SliceMatrix<> dshape) const
  {
    CalcDShape (mip.IP(), dshape);
    for (int i = 0; i < dshape.Height(); i++)
      {
        Vec<D> hv = dshape.Row(i);
        FlatVec<D> (&dshape(i,0)) = Trans (mip.GetJacobianInverse ()) * hv;
      }
  }



  template<int D>
  void ScalarFiniteElement<D> :: 
  CalcMappedDShape (const MappedIntegrationRule<D,D> & mir, 
                    SliceMatrix<> dshapes) const
  {
    for (int i = 0; i < mir.Size(); i++)
      CalcMappedDShape (mir[i], dshapes.Cols(i*D,(i+1)*D));
  }

 /*
  template<int D>
  void ScalarFiniteElement<D> :: 
  CalcDShape (const IntegrationPoint & ip, 
	      const std::function<void(int,Vec<D>)> & callback) const
  {
    cout << "ScalarFE<D>::CalcMappedDShape(callback) not implemented" << endl;
  }  
  */

  template<int D>
  double ScalarFiniteElement<D> :: 
  Evaluate (const IntegrationPoint & ip, SliceVector<double> x) const
  {
    VectorMem<20, double> shape(ndof);
    CalcShape (ip, shape);
    return InnerProduct (shape, x);
  }  
  
  template<int D>
  Vec<D> ScalarFiniteElement<D> :: 
  EvaluateGrad (const IntegrationPoint & ip, SliceVector<double> x) const
  {
    MatrixFixWidth<D> dshape(ndof);
    CalcDShape (ip, dshape);
    Vec<D> grad = Trans (dshape) * x;
    return grad;
  }  


  template<int D>
  void ScalarFiniteElement<D> :: 
  Evaluate (const IntegrationRule & ir, SliceVector<double> coefs, FlatVector<double> vals) const
  {
    for (int i = 0; i < ir.GetNIP(); i++)
      vals(i) = Evaluate (ir[i], coefs);
  }

  template<int D>
  void ScalarFiniteElement<D> :: 
  Evaluate (const SIMD_IntegrationRule & ir, SliceVector<> coefs, AFlatVector<double> values) const
  {
    cout << "Evaluate (simd) not implemented for class " << typeid(*this).name() << endl;
  }

  template<int D>
  void ScalarFiniteElement<D> :: 
  Evaluate (const IntegrationRule & ir, SliceMatrix<> coefs, SliceMatrix<> values) const
  {
    VectorMem<100> shapes(coefs.Height());
    for (int i = 0; i < ir.Size(); i++)
      {
        CalcShape (ir[i], shapes);
        values.Row(i) = Trans(coefs) * shapes;
      }
  }


  template<int D>
  void ScalarFiniteElement<D> :: 
  EvaluateGrad (const IntegrationRule & ir, SliceVector<double> coefs, FlatMatrixFixWidth<D,double> vals) const
  {
    for (int i = 0; i < ir.GetNIP(); i++)
      vals.Row(i) = EvaluateGrad (ir[i], coefs);
  }

  template<int D>
  void ScalarFiniteElement<D> :: 
  EvaluateTrans (const IntegrationRule & ir, FlatVector<double> vals, SliceVector<double> coefs) const
  {
    VectorMem<20, double> shape(ndof);
    coefs = 0.0;
    for (int i = 0; i < ir.GetNIP(); i++)
      {
	CalcShape (ir[i], shape);
	coefs += vals(i) * shape;
      }
  }

  template<int D>
  void ScalarFiniteElement<D> :: 
  AddTrans (const SIMD_IntegrationRule & ir, AFlatVector<double> values, SliceVector<> coefs) const
  {
    cout << "EvaluateTrans (simd) not implemented for class " << typeid(*this).name() << endl;    
  }

  
  template<int D>
  void ScalarFiniteElement<D> :: 
  EvaluateGradTrans (const IntegrationRule & ir, FlatMatrixFixWidth<D,double> vals, 
                     SliceVector<double> coefs) const
  {
    MatrixFixWidth<D> dshape(ndof);
    coefs = 0.0;
    for (int i = 0; i < ir.GetNIP(); i++)
      {
	CalcDShape (ir[i], dshape);
	coefs += dshape * vals.Row(i);
      }
  }


  template<int D>
  void ScalarFiniteElement<D> :: 
  EvaluateGradTrans (const IntegrationRule & ir, SliceMatrix<> values, SliceMatrix<> coefs) const
  {
#ifndef __CUDA_ARCH__
    cout << "EvalGradTrans not overloaded" << endl;
#endif
  }

  template<int D>
  void ScalarFiniteElement<D> :: 
  AddGradTrans (const SIMD_BaseMappedIntegrationRule & ir, AFlatMatrix<double> values,
                SliceVector<> coefs) const
  {
    throw Exception ("evaluategradtrans (simd) not overloaded");
  }


  








  template<int D>
  void ScalarFiniteElement<D> :: CalcDDShape (const IntegrationPoint & ip, 
                                              FlatMatrix<> ddshape) const
  {
    int nd = GetNDof();
    int sdim = D;

    double eps = 1e-7;
    Matrix<> dshape1(nd, sdim), dshape2(nd, sdim);

    for (int i = 0; i < sdim; i++)
      {
	IntegrationPoint ip1 = ip;
	IntegrationPoint ip2 = ip;
        
        ip1(i) -= eps;
        ip2(i) += eps;

	CalcDShape (ip1, dshape1);
	CalcDShape (ip2, dshape2);
	dshape2 -= dshape1;
	dshape2 *= (0.5 / eps);
	for (int j = 0; j < nd; j++)
	  for (int k = 0; k < sdim; k++)
	    ddshape(j,sdim*i+k) = dshape2(j,k);
      }  
  }


  template<int D>
  void ScalarFiniteElement<D> :: CalcMappedDDShape (const MappedIntegrationPoint<D,D> & mip, 
                                                    FlatMatrix<> ddshape) const
  {
    int nd = GetNDof();

    double eps = 1e-7;
    MatrixFixWidth<D> dshape1(nd), dshape2(nd);
    const ElementTransformation & eltrans = mip.GetTransformation();

    for (int i = 0; i < D; i++)
      {
	IntegrationPoint ip1 = mip.IP();
	IntegrationPoint ip2 = mip.IP();
        ip1(i) -= eps;
        ip2(i) += eps;
        MappedIntegrationPoint<D,D> mip1(ip1, eltrans);
        MappedIntegrationPoint<D,D> mip2(ip2, eltrans);

	CalcMappedDShape (mip1, dshape1);
	CalcMappedDShape (mip2, dshape2);

        ddshape.Cols(D*i,D*(i+1)) = (0.5/eps) * (dshape2-dshape1);
      }  

    for (int j = 0; j < D; j++)
      {
        for (int k = 0; k < nd; k++)
          for (int l = 0; l < D; l++)
            dshape1(k,l) = ddshape(k, l*D+j);
        
        dshape2 = dshape1 * mip.GetJacobianInverse();
        
        for (int k = 0; k < nd; k++)
          for (int l = 0; l < D; l++)
            ddshape(k, l*D+j) = dshape2(k,l);
      }
    
  }



				  









  template <int D>
  void DGFiniteElement<D>:: 
  GetDiagMassMatrix (FlatVector<> mass) const
  {
#ifndef __CUDA_ARCH__
    IntegrationRule ir(this->ElementType(), 2*order);
    VectorMem<50> shape(ndof);
    mass = 0;
    for (int i = 0; i < ir.Size(); i++)
      {
        this -> CalcShape (ir[i], shape);
        for (int j = 0; j < ndof; j++)
          mass(j) += ir[i].Weight() * sqr (shape(j));
      }
#endif
  }

  /*
  void MyFunc(int order)
  {
    L2HighOrderFE<ET_SEGM> fe;
  }
  */


  template <int D>
  void DGFiniteElement<D>:: 
  CalcTraceMatrix (int facet, FlatMatrix<> trace) const
  {
    ELEMENT_TYPE ftype = ElementTopology::GetFacetType (this->ElementType(), facet);
    Facet2ElementTrafo f2el(this->ElementType(), FlatArray<int> (8, const_cast<int*> (vnums)) );
    const IntegrationRule & ir = SelectIntegrationRule (ftype, 2*order);

    ScalarFiniteElement<0> * facetfe0 = NULL;
    ScalarFiniteElement<1> * facetfe1 = NULL;
    ScalarFiniteElement<2> * facetfe2 = NULL;
    switch (ftype)
      {
      case ET_POINT : facetfe0 = new FE_Point; break;
      case ET_SEGM : facetfe1 = new L2HighOrderFE<ET_SEGM> (order); break;
      case ET_TRIG : facetfe2 = new L2HighOrderFE<ET_TRIG> (order); break;
      case ET_QUAD : facetfe2 = new L2HighOrderFE<ET_QUAD> (order); break;
      default:
	;
      }

    int ndof_facet = trace.Height();
    Vector<> shape(ndof);
    Vector<> fshape(ndof_facet);
    Vector<> norms(ndof_facet);

    trace = 0.0;
    norms = 0.0;
    for (int i = 0; i < ir.Size(); i++)
      {
	if (D == 1) 
          facetfe0 -> CalcShape (ir[i], fshape);
	else if (D == 2) 
          facetfe1 -> CalcShape (ir[i], fshape);
	else            
          facetfe2 -> CalcShape (ir[i], fshape);

	this -> CalcShape (f2el (facet, ir[i]), shape);

	trace += ir[i].Weight() * fshape * Trans (shape);
	for (int j = 0; j < norms.Size(); j++)
	  norms(j) += ir[i].Weight() * sqr (fshape(j));
      }

    for (int j = 0; j < fshape.Size(); j++)
      trace.Row(j) /= norms(j);

    delete facetfe0;
    delete facetfe1;
    delete facetfe2;
  }

  template <int D>
  void DGFiniteElement<D>:: 
  CalcGradientMatrix (FlatMatrix<> gmat) const
  {
    IntegrationRule ir (this->ElementType(), 2*order);

    Vector<> shape(ndof);
    MatrixFixWidth<D> dshape(ndof);
    Vector<> norms(ndof);
    
    gmat = 0.0;
    norms = 0.0;
    for (int i = 0; i < ir.Size(); i++)
      {
	this -> CalcShape (ir[i], shape);
	this -> CalcDShape (ir[i], dshape);
        
        for (int j = 0; j < ndof; j++)
          for (int k = 0; k < ndof; k++)
            for (int l = 0; l < D; l++)
              gmat(k*D+l, j) += ir[i].Weight() * dshape(j,l) * shape(k);

	for (int j = 0; j < norms.Size(); j++)
	  norms(j) += ir[i].Weight() * sqr (shape(j));
      }
    for (int j = 0; j < ndof; j++)
      gmat.Rows(D*j, D*(j+1)) /= norms(j);
  }


  template <int D>
  void DGFiniteElement<D>:: 
  GetGradient (FlatVector<> coefs, FlatMatrixFixWidth<D> grad) const
  {
    Matrix<> gmat(D*grad.Height(), coefs.Size());
    CalcGradientMatrix (gmat);
    FlatVector<> vgrad(gmat.Height(), &grad(0,0));
    vgrad = gmat * coefs;
  }
  
  template <int D>
  void DGFiniteElement<D>:: 
  GetGradientTrans (FlatMatrixFixWidth<D> grad, FlatVector<> coefs) const 
  {
    Matrix<> gmat(D*grad.Height(), coefs.Size());
    CalcGradientMatrix (gmat);
    FlatVector<> vgrad(gmat.Height(), &grad(0,0));
    coefs = Trans (gmat) * vgrad;
  }
  
  template <int D>
  void DGFiniteElement<D>:: 
  GetTrace (int facet, FlatVector<> coefs, FlatVector<> fcoefs) const
  {
    Matrix<> trace(fcoefs.Size(), coefs.Size());
    CalcTraceMatrix(facet, trace);
    fcoefs = trace * coefs;
  }
  
  template <int D>
  void DGFiniteElement<D>:: 
  GetTraceTrans (int facet, FlatVector<> fcoefs, FlatVector<> coefs) const
  {
    Matrix<> trace(fcoefs.Size(), coefs.Size());
    CalcTraceMatrix(facet, trace);
    coefs = Trans (trace) * fcoefs;
  }



  template class ScalarFiniteElement<0>;
  template class ScalarFiniteElement<1>;
  template class ScalarFiniteElement<2>;
  template class ScalarFiniteElement<3>;


  template class DGFiniteElement<0>;
  template class DGFiniteElement<1>;
  template class DGFiniteElement<2>;
  template class DGFiniteElement<3>;
  

}

