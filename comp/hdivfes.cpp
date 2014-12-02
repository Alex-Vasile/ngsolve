/*********************************************************************/
/* File:   hdivfes.cpp                                               */
/* Author: Joachim Schoeberl                                         */
/* Date:   27. Jan. 2003                                             */
/*********************************************************************/

/* 
   Finite Element Space for H(div)
*/

#include <comp.hpp>
#include <../fem/hdivlofe.hpp>

namespace ngcomp
{
  using namespace ngcomp;

  /*
  RaviartThomasFESpace :: 
  RaviartThomasFESpace (shared_ptr<MeshAccess> ama,
			int adim, bool acomplex)
    
    : FESpace (ama, 1, adim, acomplex)
  {
    name="RaviartThomasFESpace(hdiv)";
    
    trig    = new FE_RTTrig0;
    segm    = new HDivNormalSegm0;

    if (ma.GetDimension() == 2)
      {
	Array<CoefficientFunction*> coeffs(1);
	coeffs[0] = new ConstantCoefficientFunction(1);
	evaluator = GetIntegrators().CreateBFI("masshdiv", 2, coeffs);
      }
  }
  */

  RaviartThomasFESpace :: RaviartThomasFESpace (shared_ptr<MeshAccess> ama, const Flags& flags, bool parseflags)
  : FESpace (ama, flags)
  {
    name="RaviartThomasFESpace(hdiv)";
    // defined flags
    DefineDefineFlag("hdiv");
    
    // parse flags
    if(parseflags) CheckFlags(flags);
    
    order = 1; // he: see above constructor!
        
    trig    = new FE_RTTrig0;
    segm    = new HDivNormalSegm0;

    SetDummyFE<HDivDummyFE> ();
    
    if (ma->GetDimension() == 2)
    {
      Array<shared_ptr<CoefficientFunction>> coeffs(1);
      coeffs[0] = shared_ptr<CoefficientFunction> (new ConstantCoefficientFunction(1));
      integrator = GetIntegrators().CreateBFI("masshdiv", 2, coeffs);
    }
  }
      
    RaviartThomasFESpace :: ~RaviartThomasFESpace ()
  {
    ;
  }


  shared_ptr<FESpace> RaviartThomasFESpace :: 
  Create (shared_ptr<MeshAccess> ma, const Flags & flags)
  {
    int order = int(flags.GetNumFlag ("order", 0));

    if (order <= 0)
      return make_shared<RaviartThomasFESpace> (ma, flags, true);      
    else
      return make_shared<HDivHighOrderFESpace> (ma, flags, true);
  }

  
  void RaviartThomasFESpace :: Update(LocalHeap & lh)
  {
    shared_ptr<MeshAccess> ma = GetMeshAccess();
    int level = ma->GetNLevels();
    
    if (level == ndlevel.Size())
      return;
    
    if (ma->GetDimension() == 2)
      ndlevel.Append (ma->GetNEdges());
    else
      ndlevel.Append (ma->GetNFaces());

    // FinalizeUpdate (lh);
  }

  
  int RaviartThomasFESpace :: GetNDof () const throw()
  {
    return ndlevel.Last();
  }
  
  int RaviartThomasFESpace :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }
  
  
  
  void RaviartThomasFESpace :: GetDofNrs (int elnr, Array<int> & dnums) const
  {
    Array<int> forient(6);
    
    if (ma->GetDimension() == 2)
      ma->GetElEdges (elnr, dnums, forient);
    else
      ma->GetElFaces (elnr, dnums, forient);
    
    
    if (!DefinedOn (ma->GetElIndex (elnr)))
      dnums = -1;

    
    // (*testout) << "el = " << elnr << ", dofs = " << dnums << endl;
  }
  
  
  void RaviartThomasFESpace :: GetSDofNrs (int selnr, Array<int> & dnums) const
  {
    if (ma->GetDimension() == 2)
      {
	int eoa[12];
	Array<int> eorient(12, eoa);
	ma->GetSElEdges (selnr, dnums, eorient);
	
	if (!DefinedOnBoundary (ma->GetSElIndex (selnr)))
	  dnums = -1;

	return;
      }


    /*
      int eoa[12];
      Array<int> eorient(12, eoa);
      GetMeshAccess().GetSElEdges (selnr, dnums, eorient);
      
      if (!DefinedOnBoundary (ma->GetSElIndex (selnr)))
      dnums = -1;
    */
    dnums.SetSize (1);
    dnums = -1;
    
    // (*testout) << "sel = " << selnr << ", dofs = " << dnums << endl;
  }

  Table<int> * RaviartThomasFESpace :: CreateSmoothingBlocks (int type) const
  {
    return 0;
  }
  
  
  void RaviartThomasFESpace :: 
  VTransformMR (int elnr, bool boundary,
		FlatMatrix<double> & mat, TRANSFORM_TYPE tt) const
  {
    int nd = 3;
    
    if (boundary) return;

    Vector<> fac(nd);
    
    GetTransformationFactors (elnr, fac);
    
    int i, j, k, l;
    
    if (tt & TRANSFORM_MAT_LEFT)
      for (k = 0; k < dimension; k++)
	for (i = 0; i < nd; i++)
	  for (j = 0; j < mat.Width(); j++)
	    mat(k+i*dimension, j) *= fac(i);
	
    if (tt & TRANSFORM_MAT_RIGHT)
      for (l = 0; l < dimension; l++)
	for (i = 0; i < mat.Height(); i++)
	  for (j = 0; j < nd; j++)
	    mat(i, l+j*dimension) *= fac(j);
  }
  
    
  void  RaviartThomasFESpace ::
  VTransformVR (int elnr, bool boundary,
		FlatVector<double> & vec, TRANSFORM_TYPE tt) const
  {
    int nd = 3;
    
    if (boundary) 
      {
	ArrayMem<int,4> edge_nums, edge_orient;
	ma->GetSElEdges (elnr, edge_nums, edge_orient);
	vec *= edge_orient[0];
	return;
      }

    Vector<> fac(nd);
    
    GetTransformationFactors (elnr, fac);
    
    if ((tt & TRANSFORM_RHS) || (tt & TRANSFORM_SOL))
      {
	for (int k = 0; k < dimension; k++)
	  for (int i = 0; i < nd; i++)
	    vec(k+i*dimension) *= fac(i);
      }
  }
  
  void RaviartThomasFESpace ::
  GetTransformationFactors (int elnr, FlatVector<> & fac) const
  {
    Array<int> edge_nums, edge_orient;
    
    fac = 1;

    ma->GetElEdges (elnr, edge_nums, edge_orient);
    for (int i = 0; i < 3; i++)
      fac(i) = edge_orient[i];
  }



  
  // register FESpaces
  namespace hdivfes_cpp
  {
    class Init
    { 
    public: 
      Init ();
    };
    
    Init::Init()
    {
      GetFESpaceClasses().AddFESpace ("hdiv", RaviartThomasFESpace::Create);
    }
    
    Init init;
  }
}
