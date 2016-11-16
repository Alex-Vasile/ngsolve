/*********************************************************************/
/* File:   l2hofe_trig.cpp                                           */
/* Author: Start                                                     */
/* Date:   6. Feb. 2003                                              */
/*********************************************************************/

#define FILE_L2HOFE_CPP

#include <fem.hpp>
#include <tscalarfe_impl.hpp>
#include <l2hofe_impl.hpp>
#include "l2hofefo.hpp"

namespace ngfem
{
  
  template class L2HighOrderFE<ET_TRIG>;  
  template class T_ScalarFiniteElement<L2HighOrderFE_Shape<ET_TRIG>, ET_TRIG, DGFiniteElement<2> >;
  
  template<>
  ScalarFiniteElement<2> * CreateL2HighOrderFE<ET_TRIG> (int order, FlatArray<int> vnums, Allocator & lh)
  {
    DGFiniteElement<2> * hofe = 0;

    // now we orient trigs such that the first vertex is the lowest
    if (vnums[0] < vnums[1] && vnums[0] < vnums[2] )
      {
        if (vnums[1] < vnums[2])
          {
            switch (order)
              {
              case 0: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,0, FixedOrientation<0,1,2>> (); break;
              case 1: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,1, FixedOrientation<0,1,2>> (); break;
              case 2: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,2, FixedOrientation<0,1,2>> (); break;
              case 3: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,3, FixedOrientation<0,1,2>> (); break;
              case 4: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,4, FixedOrientation<0,1,2>> (); break;
              default: ; 
              }
          }
        else
          {
            switch (order)
              {
              case 0: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,0, FixedOrientation<0,2,1>> (); break;
              case 1: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,1, FixedOrientation<0,2,1>> (); break;
              case 2: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,2, FixedOrientation<0,2,1>> (); break;
              case 3: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,3, FixedOrientation<0,2,1>> (); break;
              case 4: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,4, FixedOrientation<0,2,1>> (); break;
              default: ; 
              }
          }
      }

    if (!hofe)
      switch (order)
        {
        case 0: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,0> (); break;
        case 1: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,1> (); break;
        case 2: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,2> (); break;
        case 3: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,3> (); break;
        case 4: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,4> (); break;
          // case 5: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,5> (); break;
          // case 6: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,6> (); break;
        // case 10: hofe = new (lh)  L2HighOrderFEFO<ET_TRIG,10> (); break;
        default: hofe = new (lh) L2HighOrderFE<ET_TRIG> (order); break;
      }
    
    for (int j = 0; j < 3; j++)
      hofe->SetVertexNumber (j, vnums[j]);
    
    return hofe;
  }


  
}
