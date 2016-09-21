#ifdef __CUDA_ARCH__
#include "tscalarfe_impl_cuda.hpp"
#endif


#ifndef FILE_TSCALARFE_IMPL
#define FILE_TSCALARFE_IMPL

 
namespace ngfem
{

  template <int DIM> 
  INLINE Vec<DIM, AutoDiff<DIM>> Ip2Ad (const IntegrationPoint & ip)
  {
    Vec<DIM, AutoDiff<DIM> > adp;
    for (int i = 0; i < DIM; i++)
      adp[i] = AutoDiff<DIM> (ip(i), i);
    return adp;
  }

  




  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  CalcShape (const IntegrationPoint & ip, SliceVector<> shape) const
  {
    /*
    Vec<DIM> pt = ip.Point();
    T_CalcShape (TIP<DIM,double> (ip), shape);
    */
    TIP<DIM,double> pt = ip;
    T_CalcShape (pt, shape);    
  }

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  CalcDShape (const IntegrationPoint & ip, 
              SliceMatrix<> dshape) const
  {
    // Vec<DIM, AutoDiff<DIM> > adp = ip;
    TIP<DIM,AutoDiff<DIM>> tip = ip;
    T_CalcShape (tip, // TIP<DIM,AutoDiff<DIM>> (ip),
                 SBLambda ([&] (int i, AutoDiff<DIM> shape)
                           { shape.StoreGradient (&dshape(i,0)) ; }));
  }

#ifndef FASTCOMPILE

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  CalcShape (const IntegrationRule & ir, SliceMatrix<> shape) const
  {
    for (int i = 0; i < ir.Size(); i++)
      {
        /*
	Vec<DIM> pt = ir[i].Point();
	T_CalcShape (TIP<DIM,double> (pt), shape.Col(i));
        */
        // TIP<DIM,double> pt = ir[i];
	T_CalcShape (TIP<DIM,double> (ir[i]), shape.Col(i));        
      }
  }


  template <class FEL, ELEMENT_TYPE ET, class BASE>
  double T_ScalarFiniteElement<FEL,ET,BASE> :: 
  Evaluate (const IntegrationPoint & ip, SliceVector<double> x) const
  {
    // Vec<DIM> pt = ip.Point();

    double sum = 0;
    T_CalcShape (TIP<DIM,double> (ip), SBLambda ( [&](int i, double val) { sum += x(i)*val; } ));
    return sum;
  }  


  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  Evaluate (const IntegrationRule & ir, SliceVector<double> coefs, FlatVector<double> vals) const
  {
    for (int i = 0; i < ir.GetNIP(); i++)
      {
        // Vec<DIM> pt = ir[i].Point();
        TIP<DIM,double> ip = ir[i];
        double sum = 0;
        // T_CalcShape (TIP<DIM,double> (ir[i]), SBLambda ( [&](int i, double shape) { sum += coefs(i)*shape; } ));
        T_CalcShape (ip, SBLambda ( [&](int j, double shape) { sum += coefs(j)*shape; } ));
        vals(i) = sum;
      }
  }

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  Evaluate (const SIMD_IntegrationRule & ir, BareSliceVector<> coefs, ABareVector<double> values) const
  {
    // static Timer t("ScalarFE::Evaluate", 2); RegionTimer reg(t);
    // t.AddFlops (ir.GetNIP()*ndof);

    /*
    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    for (int i = 0; i < hir.Size(); i++)
      {
        Vec<DIM,SIMD<double>> pt = hir[i];
        SIMD<double> sum = 0;
        T_CalcShape (&pt(0), SBLambda ( [&](int j, SIMD<double> shape) { sum += coefs(j)*shape; } ));
        values.Get(i) = sum.Data();
      }
    */

    /*
    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    for (int i = 0; i < hir.Size(); i++)
      {
        TIP<DIM,SIMD<double>> pt = hir[i];
        SIMD<double> sum = 0;
        static_cast<const FEL*> (this) -> 
          T_CalcShape (pt, SBLambda ( [&](int j, SIMD<double> shape) { sum += coefs(j)*shape; } ));
        values.Get(i) = sum.Data();
      }
    */

    /*
    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    for (int i = 0; i < hir.Size(); i+=2)
      {
        Vec<DIM,SIMD<double>> pt1 = hir[i];
        Vec<DIM,SIMD<double>> pt2 = (i+1 < hir.Size()) ? hir[i+1] : hir[i];
        Vec<DIM,MultiSIMD<2,double>> pt;
        for (int i = 0; i < DIM; i++)
          pt(i) = MultiSIMD<2,double> (pt1(i), pt2(i));
        MultiSIMD<2,double> sum = 0;
        // T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<2,double> shape) { sum += coefs(j)*shape; } ));
        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<2,double> shape)
                                        { sum += (*pcoefs)*shape; pcoefs += dist; } ));
        
        values.Get(i) = sum.template Get<0>().Data();
        if (i+1 < hir.Size())
          values.Get(i+1) = sum.template Get<1>().Data();          
        }
    */


    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    int i = 0;
    for ( ; i < hir.Size()-1; i+=2)
      {
        /*
        Vec<DIM,SIMD<double>> pt1 = hir[i];
        Vec<DIM,SIMD<double>> pt2 = hir[i+1];
        Vec<DIM,MultiSIMD<2,double>> pt;
        for (int i = 0; i < DIM; i++)
          pt(i) = MultiSIMD<2,double> (pt1(i), pt2(i));
        */
        MultiSIMD<2,double> sum = 0;
        // T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<2,double> shape) { sum += coefs(j)*shape; } ));
        TIP<DIM,SIMD<double>> tip1 = hir[i].TIp<DIM>();
        TIP<DIM,SIMD<double>> tip2 = hir[i+1].TIp<DIM>();
        TIP<DIM,MultiSIMD<2,double>> tip(tip1,tip2);
        
        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        T_CalcShape (tip, // TIP<DIM,MultiSIMD<2,double>> (pt),
                     SBLambda ( [&](int j, MultiSIMD<2,double> shape)
                                {
                                  // sum += *pcoefs * shape;
                                  sum = FMA(MultiSIMD<2,double>(*pcoefs), shape, sum);
                                  pcoefs += dist; }
                                ));
        
        values.Get(i) = sum.template Get<0>().Data();
        values.Get(i+1) = sum.template Get<1>().Data();          
      }

    if (i < hir.Size())
      {
        TIP<DIM,SIMD<double>> pt = hir[i].TIp<DIM>();
        SIMD<double> sum = 0;
        // T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<2,double> shape) { sum += coefs(j)*shape; } ));
        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        T_CalcShape (pt, // TIP<DIM,SIMD<double>> (hir[i]), 
                     SBLambda ( [&](int j, SIMD<double> shape)
                                { sum += (*pcoefs)*shape; pcoefs += dist; } ));
        values.Get(i) = sum.Data();
      }
    
    /*
    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    for (int i = 0; i < hir.Size(); i+=3)
      {
        Vec<DIM,SIMD<double>> pt1 = hir[i];
        Vec<DIM,SIMD<double>> pt2 = (i+1 < hir.Size()) ? hir[i+1] : hir[i];
        Vec<DIM,SIMD<double>> pt3 = (i+2 < hir.Size()) ? hir[i+2] : hir[i];
        Vec<DIM,MultiSIMD<3,double>> pt;
        for (int i = 0; i < DIM; i++)
          {
            pt(i).template Get<0> () = pt1(i);
            pt(i).template Get<1> () = pt2(i);
            pt(i).template Get<2> () = pt3(i);
          }
        MultiSIMD<3,double> sum = 0;
        // T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<3,double> shape) { sum += coefs(j)*shape; } ));

        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<3,double> shape)
                                        { sum += (*pcoefs)*shape; pcoefs += dist; } ));

        values.Get(i) = sum.template Get<0>().Data();
        if (i+1 < hir.Size())
          values.Get(i+1) = sum.template Get<1>().Data();          
        if (i+2 < hir.Size())
          values.Get(i+2) = sum.template Get<2>().Data();          
      }
    */

    /*
    // 3 pnts with extra treatment of left-over
    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    int i = 0;
    for ( ; i < hir.Size()-2; i+=3)
      {
        Vec<DIM,SIMD<double>> pt1 = hir[i];
        Vec<DIM,SIMD<double>> pt2 = hir[i+1];
        Vec<DIM,SIMD<double>> pt3 = hir[i+2];
        Vec<DIM,MultiSIMD<3,double>> pt;
        for (int i = 0; i < DIM; i++)
          {
            pt(i).template Get<0> () = pt1(i);
            pt(i).template Get<1> () = pt2(i);
            pt(i).template Get<2> () = pt3(i);
          }
        MultiSIMD<3,double> sum = 0;
        T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<3,double> shape) { sum += coefs(j)*shape; } ));
        values.Get(i) = sum.template Get<0>().Data();
        values.Get(i+1) = sum.template Get<1>().Data();          
        values.Get(i+2) = sum.template Get<2>().Data();          
      }

    if (i < hir.Size())
      {
        Vec<DIM,SIMD<double>> pt1 = hir[i];
        Vec<DIM,SIMD<double>> pt2 = (i+1 < hir.Size()) ? hir[i+1] : hir[i];
        Vec<DIM,MultiSIMD<2,double>> pt;
        for (int i = 0; i < DIM; i++)
          {
            pt(i).template Get<0> () = pt1(i);
            pt(i).template Get<1> () = pt2(i);
          }
        MultiSIMD<2,double> sum = 0;
        T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<2,double> shape) { sum += coefs(j)*shape; } ));
        values.Get(i) = sum.template Get<0>().Data();
        if (i+1 < hir.Size())
          values.Get(i+1) = sum.template Get<1>().Data();          
      }
    */

    
    /*
    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    for (int i = 0; i < hir.Size(); i+=4)
      {
        Vec<DIM,SIMD<double>> pt1 = hir[i];
        Vec<DIM,SIMD<double>> pt2 = (i+1 < hir.Size()) ? hir[i+1] : hir[i];
        Vec<DIM,SIMD<double>> pt3 = (i+2 < hir.Size()) ? hir[i+2] : hir[i];
        Vec<DIM,SIMD<double>> pt4 = (i+3 < hir.Size()) ? hir[i+3] : hir[i];
        Vec<DIM,MultiSIMD<4,double>> pt;
        for (int i = 0; i < DIM; i++)
          {
            pt(i).template Get<0> () = pt1(i);
            pt(i).template Get<1> () = pt2(i);
            pt(i).template Get<2> () = pt3(i);
            pt(i).template Get<3> () = pt4(i);
          }
        MultiSIMD<4,double> sum = 0;
        // T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<4,double> shape) { sum += coefs(j)*shape; } ));

        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<4,double> shape)
                                        { sum += (*pcoefs)*shape; pcoefs += dist; } ));

        
        values.Get(i) = sum.template Get<0>().Data();
        if (i+1 < hir.Size())
          values.Get(i+1) = sum.template Get<1>().Data();          
        if (i+2 < hir.Size())
          values.Get(i+2) = sum.template Get<2>().Data();          
        if (i+3 < hir.Size())
          values.Get(i+3) = sum.template Get<3>().Data();          
      }
    */
  }
    

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  Evaluate (const SIMD_IntegrationRule & ir,
            SliceMatrix<> coefs,
            ABareMatrix<double> values) const
  {
    FlatArray<SIMD<IntegrationPoint>> hir = ir;    
    size_t j = 0;
    for ( ; j+4 <= coefs.Width(); j+=4)
      {
        for (size_t i = 0; i < hir.Size(); i++)
          {
            TIP<DIM,SIMD<double>> pt = hir[i].TIp<DIM>();
            SIMD<double> sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;
            double * pcoefs = &coefs(j);
            size_t dist = coefs.Dist();
            T_CalcShape (pt, 
                         SBLambda ( [&](int j, SIMD<double> shape)
                                    {
                                      sum1 += pcoefs[0]*shape;
                                      sum2 += pcoefs[1]*shape;
                                      sum3 += pcoefs[2]*shape;
                                      sum4 += pcoefs[3]*shape;
                                      pcoefs += dist;
                                    } ));
            values.Get(j,i) = sum1.Data();
            values.Get(j+1,i) = sum2.Data();
            values.Get(j+2,i) = sum3.Data();
            values.Get(j+3,i) = sum4.Data();
          }
      }
    switch (coefs.Width()&3)
      {
      case 0: break;
      case 1: Evaluate (ir, coefs.Col(j), values.Row(j)); break;
      case 2:
        {
          for (size_t i = 0; i < hir.Size(); i++)
            {
              TIP<DIM,SIMD<double>> pt = hir[i].TIp<DIM>();
              SIMD<double> sum1 = 0, sum2 = 0;
              double * pcoefs = &coefs(j);
              size_t dist = coefs.Dist();
              T_CalcShape (pt, 
                           SBLambda ( [&](int j, SIMD<double> shape)
                                      {
                                        sum1 += pcoefs[0]*shape;
                                        sum2 += pcoefs[1]*shape;
                                        pcoefs += dist;
                                      } ));
              values.Get(j,i) = sum1.Data();
              values.Get(j+1,i) = sum2.Data();
            }
          break;
        case 3:
          {
            for (size_t i = 0; i < hir.Size(); i++)
              {
                TIP<DIM,SIMD<double>> pt = hir[i].TIp<DIM>();
                SIMD<double> sum1 = 0, sum2 = 0, sum3 = 0;
                double * pcoefs = &coefs(j);
                size_t dist = coefs.Dist();
                T_CalcShape (pt, 
                             SBLambda ( [&](int j, SIMD<double> shape)
                                        {
                                          sum1 += pcoefs[0]*shape;
                                          sum2 += pcoefs[1]*shape;
                                          sum3 += pcoefs[2]*shape;
                                          pcoefs += dist;
                                        } ));
                values.Get(j,i) = sum1.Data();
                values.Get(j+1,i) = sum2.Data();
                values.Get(j+2,i) = sum3.Data();
              }
            break;
          }
        }
      }
        /*
    for ( ; j < coefs.Width(); j++)
      Evaluate (ir, coefs.Col(j), values.Row(j));
        */
  }

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  Evaluate (const IntegrationRule & ir, SliceMatrix<> coefs, SliceMatrix<> values) const
  {
    for (int i = 0; i < ir.GetNIP(); i++)
      {
        // Vec<DIM> pt = ir[i].Point();
        TIP<DIM,double> tip(ir[i]);
        values.Row(i) = 0.0;
        T_CalcShape (tip,
                     SBLambda ( [&](int j, double shape) 
                                { 
                                  // sum += coefs(i)*shape; 
                                  values.Row(i) += shape * coefs.Row(j); 
                                } 
                                ));
      }
  }

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  EvaluateTrans (const IntegrationRule & ir, FlatVector<> vals, SliceVector<double> coefs) const
  {
    coefs = 0.0;
    for (int i = 0; i < ir.GetNIP(); i++)
      {
        // Vec<DIM> pt = ir[i].Point();
        TIP<DIM,double> tip(ir[i]);
        T_CalcShape (tip,
                     SBLambda ( [&](int j, double shape) 
                                { coefs(j) += vals(i)*shape; } ));
      }
  }

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  AddTrans (const SIMD_IntegrationRule & ir, ABareVector<double> values,
            BareSliceVector<> coefs) const
  {
    FlatArray<SIMD<IntegrationPoint>> hir = ir;
    /*
    for (int i = 0; i < hir.Size(); i++)
      {
        Vec<DIM,SIMD<double>> pt = hir[i];
        SIMD<double> val = values.Get(i);
        T_CalcShape (&pt(0), SBLambda ( [&](int j, SIMD<double> shape) { coefs(j) += HSum(val*shape); } ));
      }
    */

    size_t i = 0;
    for ( ; i+2 <= hir.Size(); i+=2)
      {
        TIP<DIM,SIMD<double>> tip1 = hir[i].TIp<DIM>();
        // TIP<DIM,SIMD<double>> tip2 = hir[(i+1 < hir.Size()) ? i+1 : i].TIp<DIM>();
        TIP<DIM,SIMD<double>> tip2 = hir[i+1].TIp<DIM>();
        TIP<DIM,MultiSIMD<2,double>> tip(tip1,tip2);

        MultiSIMD<2,double> val (values.Get(i), values.Get(i+1));
        // i+1 < hir.Size() ? values.Get(i+1) : SIMD<double> (0.0));

        // T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<2,double> shape) { coefs(j) += HSum(val*shape); } ));

        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        /*
        T_CalcShape (tip, 
                     SBLambda
                     ([&](int j, MultiSIMD<2,double> shape)
                      { *pcoefs += HSum(val*shape); pcoefs += dist; }
                      ));
        */
        T_CalcShape (tip, 
                     SBLambdaDuo
                     ([&](int j, MultiSIMD<2,double> shape)
                      {
                        *pcoefs += HSum(val*shape); pcoefs += dist;
                      },
                      [&](int j, MultiSIMD<2,double> shape, int j2, MultiSIMD<2,double> shape2)
                      {
                        auto v2 = HSum(val*shape, val*shape2);
                        *pcoefs += get<0>(v2); pcoefs += dist;
                        *pcoefs += get<1>(v2); pcoefs += dist;
                      }
                      ));        
      }

    for ( ; i < hir.Size(); i++)
      {
        TIP<DIM,SIMD<double>> tip = hir[i].TIp<DIM>();
        SIMD<double> val (values.Get(i));

        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        T_CalcShape (tip, 
                     SBLambdaDuo
                     ([&](int j, SIMD<double> shape)
                      {
                        *pcoefs += HSum(val*shape); pcoefs += dist;
                      },
                      [&](int j, SIMD<double> shape, int j2, SIMD<double> shape2)
                      {
                        auto v2 = HSum(val*shape, val*shape2);
                        *pcoefs += get<0>(v2); pcoefs += dist;
                        *pcoefs += get<1>(v2); pcoefs += dist;
                      }
                      ));
      }


    
    
    /*
    for (int i = 0; i < hir.Size(); i+=3)
      {
        Vec<DIM,SIMD<double>> pt1 = hir[i];
        Vec<DIM,SIMD<double>> pt2 = hir[(i+1 < hir.Size()) ? i+1 : i];
        Vec<DIM,SIMD<double>> pt3 = hir[(i+2 < hir.Size()) ? i+2 : i];
        
        Vec<DIM,MultiSIMD<3,double>> pt;
        for (int i = 0; i < DIM; i++)
          pt(i) = MultiSIMD<3,double> (pt1(i), pt2(i), pt3(i));          
        MultiSIMD<3,double> val (values.Get(i),
                                 i+1 < hir.Size() ? values.Get(i+1) : SIMD<double> (0.0),
                                 i+2 < hir.Size() ? values.Get(i+2) : SIMD<double> (0.0));

        // T_CalcShape (&pt(0), SBLambda ( [&](int j, MultiSIMD<3,double> shape) { coefs(j) += HSum(val*shape); } ));

        double * pcoefs = &coefs(0);
        size_t dist = coefs.Dist();
        T_CalcShape (TIP<DIM,MultiSIMD<3,double>> (pt),
                     SBLambda ( [&](int j, MultiSIMD<3,double> shape)
                                { *pcoefs += HSum(val*shape); pcoefs += dist; } ));
        
      }
    */
  }

#ifdef __AVX__
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  AddTrans (const SIMD_IntegrationRule & ir,
            ABareMatrix<double> values,
            SliceMatrix<> coefs) const
  {
    FlatArray<SIMD<IntegrationPoint>> hir = ir;    
    size_t j = 0;
    for ( ; j+4 <= coefs.Width(); j+=4)
      {
        for (size_t i = 0; i < hir.Size(); i++)
          {
            TIP<DIM,SIMD<double>> pt = hir[i].TIp<DIM>();
            SIMD<double> val1 = values.Get(j,i);
            SIMD<double> val2 = values.Get(j+1,i);
            SIMD<double> val3 = values.Get(j+2,i);
            SIMD<double> val4 = values.Get(j+3,i);
            double * pcoefs = &coefs(j);
            size_t dist = coefs.Dist();
            T_CalcShape (pt, 
                         SBLambda ( [&](int j, SIMD<double> shape)
                                    {
                                      auto val = HSum(shape*val1, shape*val2, shape*val3, shape*val4);
                                      val += SIMD<double> (pcoefs);
                                      _mm256_storeu_pd (pcoefs, val.Data());
                                      pcoefs += dist;
                                    } ));
          }
      }
    switch (coefs.Width()&3)
      {
      case 0: break;
      case 1: AddTrans (ir, values.Row(j), coefs.Col(j)); break;
      case 2:
        {
          for (size_t i = 0; i < hir.Size(); i++)
            {
              TIP<DIM,SIMD<double>> pt = hir[i].TIp<DIM>();
              SIMD<double> val1 = values.Get(j,i);
              SIMD<double> val2 = values.Get(j+1,i);
              __m256i mask = _mm256_set_epi64x(0, 0, -1, -1);
              double * pcoefs = &coefs(j);
              size_t dist = coefs.Dist();
              T_CalcShape (pt, 
                           SBLambda ( [&](int j, SIMD<double> shape)
                                      {
                                        auto val = HSum(shape*val1, shape*val2, shape*val2, shape*val2);
                                        val += SIMD<double> (_mm256_maskload_pd (pcoefs, mask));
                                        _mm256_maskstore_pd (pcoefs, mask, val.Data());
                                        pcoefs += dist;
                                      } ));
            }
          break;
        }
      case 3:
        {
          for (size_t i = 0; i < hir.Size(); i++)
            {
              TIP<DIM,SIMD<double>> pt = hir[i].TIp<DIM>();
              SIMD<double> val1 = values.Get(j,i);
              SIMD<double> val2 = values.Get(j+1,i);
              SIMD<double> val3 = values.Get(j+2,i);
              __m256i mask = _mm256_set_epi64x(0, -1, -1, -1);
              double * pcoefs = &coefs(j);
              size_t dist = coefs.Dist();
              T_CalcShape (pt, 
                           SBLambda ( [&](int j, SIMD<double> shape)
                                      {
                                        auto val = HSum(shape*val1, shape*val2, shape*val3, shape*val3);
                                        val += SIMD<double> (_mm256_maskload_pd (pcoefs, mask));
                                        _mm256_maskstore_pd (pcoefs, mask, val.Data());
                                        pcoefs += dist;
                                      } ));
            }
          break;
        }
      }
        /*
    for ( ; j < coefs.Width(); j++)
      Evaluate (ir, coefs.Col(j), values.Row(j));
        */
  }
#endif
  

  
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  auto T_ScalarFiniteElement<FEL,ET,BASE> :: 
  EvaluateGrad (const IntegrationPoint & ip, SliceVector<double> coefs) const -> Vec<DIM>
  {
    // Vec<DIM, AutoDiff<DIM>> adp = ip;
    TIP<DIM,AutoDiff<DIM>> tip = ip;
    AutoDiff<DIM> sum = 0.0;
    T_CalcShape (tip, // TIP<DIM, AutoDiff<DIM>> (adp),
                 SBLambda ( [&](int i, AutoDiff<DIM> val) 
                            { 
                              sum += coefs(i) * val;
                            }));
    return AD2Vec<DIM> (sum);
  }

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  EvaluateGrad (const IntegrationRule & ir, SliceVector<double> coefs, 
                FlatMatrixFixWidth<DIM> vals) const
  {
    for (int i = 0; i < ir.GetNIP(); i++)
      {
        // Vec<DIM, AutoDiff<DIM>> adp = ir[i]; 
        TIP<DIM,AutoDiff<DIM>> tip = ir[i];
        Vec<DIM> sum = 0.0;
        T_CalcShape (tip, // TIP<DIM, AutoDiff<DIM>> (adp),
                     SBLambda ([&] (int j, AD2Vec<DIM> shape)
                               { sum += coefs(j) * shape; }));
        vals.Row(i) = sum;
      }
  }


  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  EvaluateGrad (const SIMD_BaseMappedIntegrationRule & bmir,
                BareSliceVector<> coefs,
                ABareMatrix<double> values) const
  {
    if ((DIM == 3) || (bmir.DimSpace() == DIM))
      {
        auto & mir = static_cast<const SIMD_MappedIntegrationRule<DIM,DIM>&> (bmir);
        for (int i = 0; i < mir.Size(); i++)
          {
            Vec<DIM, AutoDiff<DIM,SIMD<double>>> adp = mir[i];
            Vec<DIM,SIMD<double>> sum(0.0);
            T_CalcShape (TIP<DIM,AutoDiff<DIM,SIMD<double>>>(adp),
                         SBLambda ([&] (int j, AD2Vec<DIM,SIMD<double>> shape)
                                   { sum += coefs(j) * shape; }));
            for (int k = 0; k < DIM; k++)
              values.Get(k,i) = sum(k).Data();
          }
      }
    else
      {
        cout << "EvaluateGrad(simd) called for boudnary (not implemented)" << endl;        
      }
  }

  
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  EvaluateGrad (const SIMD_IntegrationRule & ir,
                BareSliceVector<> coefs,
                ABareMatrix<double> values) const
  {
    for (int i = 0; i < ir.Size(); i++)
      {
        Vec<DIM, AutoDiff<DIM,SIMD<double>>> adp = ir[i];
        Vec<DIM,SIMD<double>> sum(0.0);
        T_CalcShape (TIP<DIM,AutoDiff<DIM,SIMD<double>>> (adp),
                     SBLambda ([&] (int j, AD2Vec<DIM,SIMD<double>> shape)
                               { sum += coefs(j) * shape; }));
        for (int k = 0; k < DIM; k++)
          values.Get(k,i) = sum(k).Data();
      }
  }

  
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  EvaluateGradTrans (const IntegrationRule & ir, 
                     FlatMatrixFixWidth<DIM> vals, SliceVector<double> coefs) const
  {
    coefs = 0.0;
    for (int i = 0; i < ir.GetNIP(); i++)
      {
        Vec<DIM, AutoDiff<DIM>> adp = ir[i];
        T_CalcShape (TIP<DIM,AutoDiff<DIM>> (adp),
                     SBLambda ([&] (int j, AD2Vec<DIM> shape)
                               { coefs(j) += InnerProduct (vals.Row(i), shape); }));
      }
  }
  
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  EvaluateGradTrans (const IntegrationRule & ir, SliceMatrix<> values, SliceMatrix<> coefs) const
  {
    int nels = coefs.Width();
    coefs = 0.0;
    for (int i = 0; i < ir.GetNIP(); i++)
      {
        Vec<DIM, AutoDiff<DIM>> adp = ir[i];  
        T_CalcShape (TIP<DIM, AutoDiff<DIM>> (adp),
                     SBLambda ([&] (int j, AD2Vec<DIM> shape)
                               { 
                                 Vec<DIM> grad = shape;
                                          FlatMatrixFixWidth<DIM> mvals(nels, &values(i,0));
                                          coefs.Row(j) += mvals * grad;
                               }));
      }
  }

  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  AddGradTrans (const SIMD_BaseMappedIntegrationRule & bmir,
                ABareMatrix<double> values,
                BareSliceVector<> coefs) const
  {
    if ((DIM == 3) || (bmir.DimSpace() == DIM))
      {
        auto & mir = static_cast<const SIMD_MappedIntegrationRule<DIM,DIM>&> (bmir);
        for (int i = 0; i < mir.Size(); i++)
          {
            Vec<DIM, AutoDiff<DIM,SIMD<double>>> adp = mir[i];
            T_CalcShape (TIP<DIM, AutoDiff<DIM,SIMD<double> >> (adp),
                         SBLambda ([&] (int j, AD2Vec<DIM,SIMD<double>> shape)
                                   {
                                     SIMD<double> sum = 0.0;
                                     for (int k = 0; k < DIM; k++)
                                       sum += shape(k) * values.Get(k,i);
                                     coefs(j) += HSum(sum);
                                   }));
          }
      }
    else
      {
        cout << "AddGradTrans called for boudnary (not implemented)" << endl;
      }
  }
  
  /*
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  CalcDShape (const IntegrationPoint & ip, 
	      const std::function<void(int,Vec<DIM>)> & callback) const
  {
    Vec<DIM, AutoDiff<DIM> > adp;
    for (int i = 0; i < DIM; i++)
      adp[i] = AutoDiff<DIM> (ip(i), i);
      
    // DShapeAssign<DIM> ds(dshape); 
    // T_CalcShape (&adp(0), ds);


    T_CalcShape (&adp(0), SBLambda ([&] (int i, AutoDiff<DIM> shape)
                                    {
				      Vec<DIM> v;
				      shape.StoreGradient (&v(0));
				      callback (i,v);
				    }));
  }
  */



  /*
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  CalcMappedDShape (const MappedIntegrationPoint<DIM,DIM> & mip, 
		    FlatMatrixFixWidth<DIM> dshape) const
  {
    Vec<DIM, AutoDiff<DIM> > adp;   
    for (int i = 0; i < DIM; i++)
      adp[i].Value() = mip.IP()(i);
      
    for (int i = 0; i < DIM; i++)
      for (int j = 0; j < DIM; j++)
	adp[i].DValue(j) = mip.GetJacobianInverse()(i,j);

    T_CalcShape (&adp(0), SBLambda ([&] (int i, AutoDiff<DIM> shape)
                                    { shape.StoreGradient (&dshape(i,0)) ; }));
  }
  */


  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  CalcMappedDShape (const MappedIntegrationPoint<DIM,DIM> & mip, 
		    SliceMatrix<> dshape) const
  {
    Vec<DIM, AutoDiff<DIM>> adp = mip;

    T_CalcShape (TIP<DIM, AutoDiff<DIM>> (adp),
                 SBLambda ([&] (int i, AutoDiff<DIM> shape)
                           { shape.StoreGradient (&dshape(i,0)) ; }));
  }


  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  CalcMappedDShape (const MappedIntegrationRule<DIM,DIM> & mir, 
		    SliceMatrix<> dshape) const
  {
    for (int i = 0; i < mir.Size(); i++)
      T_ScalarFiniteElement::CalcMappedDShape (mir[i], dshape.Cols(i*DIM,(i+1)*DIM));
  }


  /*
    ... not yet working
  template <class FEL, ELEMENT_TYPE ET, class BASE>
  void T_ScalarFiniteElement<FEL,ET,BASE> :: 
  GetPolOrders (FlatArray<PolOrder<DIM> > orders) const
  {
    Vec<DIM,PolOrder<DIM>> po;

    switch (ET)
      {
      case ET_TRIG:
        po[0] = INT<DIM> (1,1); 
        po[1] = INT<DIM> (1,1); 
        break;
      case ET_QUAD:
        po[0] = INT<DIM> (1,0); 
        po[1] = INT<DIM> (0,1); 
        break;
      case ET_TET:
        po[0] = INT<DIM> (1,1,1); 
        po[1] = INT<DIM> (1,1,1); 
        po[2] = INT<DIM> (1,1,1); 
        break;
      case ET_PRISM:
        po[0] = INT<DIM> (1,1,0); 
        po[1] = INT<DIM> (1,1,0); 
        po[2] = INT<DIM> (0,0,1); 
        break;

      default:
        for (int i = 0; i < DIM; i++)
          for (int j = 0; j < DIM; j++)
            po[i](j) = 1;
      }

    T_CalcShape (&po[0], orders);
    // did not work for old tensor productelements: order cancelation for lam_e
  }
  */


#endif

}



#endif
