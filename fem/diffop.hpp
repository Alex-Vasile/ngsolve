#ifndef FILE_DIFFOP
#define FILE_DIFFOP

/*********************************************************************/
/* File:   diffop.hpp                                                */
/* Author: Joachim Schoeberl                                         */
/* Date:   24. Nov. 2009                                             */
/*********************************************************************/

namespace ngfem
{



  /**
     Differential Operator.
     Base-class for template-polymorphismus.
     Provides application and transpose-application.
     Operations can be applied for one integration point, or for the whole integration rule at once.
  */
  template<class DOP>
  class DiffOp
  {
  public:

    static string Name() { return typeid(DiffOp<DOP>()).name(); }
    static constexpr bool SUPPORT_PML = false;
  
    /**
       Computes the B-matrix. 
       The height is DIM_DMAT, the width is fel.GetNDof(). 
       FEL is the FiniteElement type specified in the BDB-Integrator 
       mip is the mapped integration point containing the Jacobi-Matrix 
       MAT is the resulting matrix (usually a FixedHeightMatrix)
    */

    template <typename FEL, typename MIP, typename MAT>
    static void GenerateMatrix (const FEL & fel, const MIP & mip,
				MAT & mat, LocalHeap & lh)
    { 
      cout << "bad function calling" << endl;
    }

    /// tbd
    template <typename FEL, typename MIR, typename MAT>
    static void GenerateMatrixIR (const FEL & fel, const MIR & mir,
                                  MAT & mat, LocalHeap & lh)
    {
      for (int i = 0; i < mir.Size(); i++)
        DOP::GenerateMatrix (fel, mir[i], mat.Rows(i*DOP::DIM_DMAT, (i+1)*DOP::DIM_DMAT), lh);
    }

    template <typename FEL, typename MIR>
    static void GenerateMatrixSIMDIR (const FEL & fel, const MIR & mir, ABareMatrix<> mat)
    {
      throw ExceptionNOSIMD (string("generate matrix simdir not implemented for diffop ") + typeid(DOP).name());
    }
    /**
       Applies the B-matrix.
       Computes matrix-vector product with the B-matrix
    */
    template <typename FEL, typename MIP, class TVX, class TVY>
    static void Apply (const FEL & fel, const MIP & mip,
		       const TVX & x, TVY & y,
		       LocalHeap & lh)
    {
      // typedef typename TVY::TSCAL TSCAL;
      typedef typename MIP::TSCAL TSCAL;
      HeapReset hr(lh);

      FlatMatrixFixHeight<DOP::DIM_DMAT, TSCAL> mat(DOP::DIM*fel.GetNDof(), lh);
      DOP::GenerateMatrix (fel, mip, mat, lh);
      y = mat * x;
    }

    /// Computes B-matrix times element vector in many points
    template <typename FEL, class MIR, class TVX, class TVY>
    static void ApplyIR (const FEL & fel, const MIR & mir,
			 const TVX & x, TVY & y,
			 LocalHeap & lh)
    {
      for (int i = 0; i < mir.Size(); i++)
        DOP::Apply (fel, mir[i], x, y.Row(i), lh);
    }

    /// Computes B-matrix times element vector in many points
    template <typename FEL, class MIR, class TVX, class TVY>
    static void ApplySIMDIR (const FEL & fel, const MIR & mir,
                             const TVX & x, TVY & y)
    // LocalHeap & lh)
    {
      throw ExceptionNOSIMD (string("apply simdir not implemented for diffop ") + typeid(DOP).name());
    }


    /// Computes Transpose (B-matrix) times point value
    template <typename FEL, typename MIP, class TVX, class TVY>
    static void ApplyTrans (const FEL & fel, const MIP & mip,
			    const TVX & x, TVY & y,
			    LocalHeap & lh) 
    {
      // typedef typename TVY::TSCAL TSCAL;
      typedef typename MIP::TSCAL TSCAL;

      HeapReset hr(lh);

      FlatMatrixFixHeight<DOP::DIM_DMAT, TSCAL> mat(DOP::DIM*fel.GetNDof(), lh);
      DOP::GenerateMatrix (fel, mip, mat, lh);
      y = Trans (mat) * x;
    }

    /// Computes Transpose (B-matrix) times point value
    template <typename FEL, typename MIP, class TVX, class TVY>
    static void ApplyTransAdd (const FEL & fel, const MIP & mip,
                               const TVX & x, TVY & y,
                               LocalHeap & lh) 
    {
      typedef typename MIP::TSCAL TSCAL;

      HeapReset hr(lh);

      FlatMatrixFixHeight<DOP::DIM_DMAT, TSCAL> mat(DOP::DIM*fel.GetNDof(), lh);
      DOP::GenerateMatrix (fel, mip, mat, lh);
      y += Trans (mat) * x;
    }


    /// Computes Transpose (B-matrix) times point value
    template <typename FEL, typename MIR, class TVX, class TVY>
    static void ApplyTransIR (const FEL & fel, const MIR & mir,
			      const TVX & x, TVY & y,
			      LocalHeap & lh) 
    {
      y = 0.0;
      for (int i = 0; i < mir.Size(); i++)
        ApplyTransAdd (fel, mir[i], x.Row(i), y, lh);
    }

    /// Computes Transpose (B-matrix) times point value    
    template <typename FEL, class MIR, class TVX, class TVY>
    static void AddTransSIMDIR (const FEL & fel, const MIR & mir,
                                const TVX & x, TVY & y)
    // LocalHeap & lh)
    {
      throw ExceptionNOSIMD (string("AddTrans simdir not implemented for diffop ") + typeid(DOP).name());
    }
    
  };





  

  /**
     Differential Operator.
     Base-class for run-time polymorphismus.
     Provides application and transpose-application
  */
  class DifferentialOperator
  {
    int dim;
    int blockdim;
    bool boundary;
    int difforder;
  public:
    NGS_DLL_HEADER DifferentialOperator(int adim, int ablockdim, bool aboundary, int adifforder)
      : dim(adim), blockdim(ablockdim), boundary(aboundary), difforder(adifforder)
    { ; }
    /// destructor
    NGS_DLL_HEADER virtual ~DifferentialOperator ();
    ///
    virtual string Name() const { return typeid(*this).name(); }
    /// dimension of range
    // NGS_DLL_HEADER virtual int Dim() const = 0;
    int Dim() const { return dim; }
    /// number of copies of finite element by BlockDifferentialOperator
    // NGS_DLL_HEADER virtual int BlockDim() const { return 1; }
    int BlockDim() const { return blockdim; }
    /// does it live on the boundary ?
    //virtual bool Boundary() const { return false; }
    bool Boundary() const { return boundary; }

    /// total polynomial degree is reduced by this order (i.e. minimal difforder)
    // virtual int DiffOrder() const = 0;
    int DiffOrder() const { return difforder; } 

    virtual IntRange UsedDofs(const FiniteElement & fel) const { return IntRange(0, fel.GetNDof()); }

    virtual bool operator== (const DifferentialOperator & diffop2) const { return false; }
    
    /// calculates the matrix
    NGS_DLL_HEADER virtual void
    CalcMatrix (const FiniteElement & fel,
		const BaseMappedIntegrationPoint & mip,
		SliceMatrix<double,ColMajor> mat,   
		LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    CalcMatrix (const FiniteElement & bfel,
		const BaseMappedIntegrationPoint & bmip,
		SliceMatrix<Complex,ColMajor> mat, 
		LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    CalcMatrix (const FiniteElement & fel,
		const BaseMappedIntegrationRule & mir,
		SliceMatrix<double,ColMajor> mat,   
		LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    CalcMatrix (const FiniteElement & fel,
		const BaseMappedIntegrationRule & mir,
		SliceMatrix<Complex,ColMajor> mat,   
		LocalHeap & lh) const;
    
    NGS_DLL_HEADER virtual void
    CalcMatrix (const FiniteElement & fel,
		const SIMD_BaseMappedIntegrationRule & mir,
		ABareMatrix<double> mat) const;

    NGS_DLL_HEADER virtual void
    Apply (const FiniteElement & fel,
	   const BaseMappedIntegrationPoint & mip,
	   FlatVector<double> x, 
	   FlatVector<double> flux,
	   LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    Apply (const FiniteElement & fel,
	   const BaseMappedIntegrationPoint & mip,
	   FlatVector<Complex> x, 
	   FlatVector<Complex> flux,
	   LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    Apply (const FiniteElement & fel,
	   const BaseMappedIntegrationRule & mir,
	   FlatVector<double> x, 
	   FlatMatrix<double> flux,
	   LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    Apply (const FiniteElement & fel,
	   const BaseMappedIntegrationRule & mir,
	   FlatVector<Complex> x, 
	   FlatMatrix<Complex> flux,
	   LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    Apply (const FiniteElement & bfel,
	   const SIMD_BaseMappedIntegrationRule & bmir,
	   BareSliceVector<double> x, 
	   ABareMatrix<double> flux) const;
    // LocalHeap & lh) const;
    
    NGS_DLL_HEADER virtual void
    ApplyTrans (const FiniteElement & fel,
		const BaseMappedIntegrationPoint & mip,
		FlatVector<double> flux,
		FlatVector<double> x, 
		LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    ApplyTrans (const FiniteElement & fel,
		const BaseMappedIntegrationPoint & mip,
		FlatVector<Complex> flux,
		FlatVector<Complex> x, 
		LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    ApplyTrans (const FiniteElement & fel,
		const BaseMappedIntegrationRule & mir,
		FlatMatrix<double> flux,
		FlatVector<double> x, 
		LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    ApplyTrans (const FiniteElement & fel,
		const BaseMappedIntegrationRule & mir,
		FlatMatrix<Complex> flux,
		FlatVector<Complex> x, 
		LocalHeap & lh) const;

    NGS_DLL_HEADER virtual void
    AddTrans (const FiniteElement & bfel,
              const SIMD_BaseMappedIntegrationRule & bmir,
              ABareMatrix<double> flux,
              BareSliceVector<double> x) const;
    // LocalHeap & lh) const;
  };



  class BlockDifferentialOperator : public DifferentialOperator
  {
  protected:
    shared_ptr<DifferentialOperator> diffop;
    int dim;
    int comp;
  public:
    BlockDifferentialOperator (shared_ptr<DifferentialOperator> adiffop, 
			       int adim, int acomp = -1)
      : DifferentialOperator(adim*adiffop->Dim(), adim*adiffop->BlockDim(),
                             adiffop->Boundary(), adiffop->DiffOrder()),
        diffop(adiffop), dim(adim), comp(acomp) { ; }

    virtual ~BlockDifferentialOperator ();
    
    virtual string Name() const { return diffop->Name(); }
    /// dimension of range
    /*
    virtual int Dim() const { return dim*diffop->Dim(); }
    virtual int BlockDim() const { return dim*diffop->BlockDim(); }
    virtual bool Boundary() const { return diffop->Boundary(); }
    virtual int DiffOrder() const { return diffop->DiffOrder(); }
    */
    shared_ptr<DifferentialOperator> BaseDiffOp() const { return diffop; } 
    virtual IntRange UsedDofs(const FiniteElement & fel) const { return dim*diffop->UsedDofs(fel); }

    NGS_DLL_HEADER virtual void
    CalcMatrix (const FiniteElement & fel,
		const BaseMappedIntegrationPoint & mip,
		SliceMatrix<double,ColMajor> mat, 
		LocalHeap & lh) const;    

    NGS_DLL_HEADER virtual void
    CalcMatrix (const FiniteElement & fel,
		const SIMD_BaseMappedIntegrationRule & mir,
		ABareMatrix<double> mat) const;
    
    NGS_DLL_HEADER virtual void
    Apply (const FiniteElement & fel,
	   const BaseMappedIntegrationPoint & mip,
	   FlatVector<double> x, 
	   FlatVector<double> flux,
	   LocalHeap & lh) const;

    virtual void
    Apply (const FiniteElement & bfel,
	   const SIMD_BaseMappedIntegrationRule & bmir,
	   BareSliceVector<double> x, 
	   ABareMatrix<double> flux) const;
    // LocalHeap & lh) const;
    
    NGS_DLL_HEADER virtual void
    ApplyTrans (const FiniteElement & fel,
                const BaseMappedIntegrationPoint & mip,
                FlatVector<double> flux,
                FlatVector<double> x, 
                LocalHeap & lh) const;
    
    NGS_DLL_HEADER virtual void
    ApplyTrans (const FiniteElement & fel,
                const BaseMappedIntegrationPoint & mip,
                FlatVector<Complex> flux,
                FlatVector<Complex> x, 
                LocalHeap & lh) const;

    virtual void
    AddTrans (const FiniteElement & bfel,
              const SIMD_BaseMappedIntegrationRule & bmir,
              ABareMatrix<double> flux,
              BareSliceVector<double> x) const;
    // LocalHeap & lh) const;    
  };



  /**
     Connect compile-time polymorph DiffOp to run-time polymorph DifferentialOperator.
   */
  template <typename DIFFOP>
  class T_DifferentialOperator : public DifferentialOperator
  {
  protected:
    enum { DIM_SPACE   = DIFFOP::DIM_SPACE };
    enum { DIM_ELEMENT = DIFFOP::DIM_ELEMENT };
    enum { DIM_DMAT    = DIFFOP::DIM_DMAT };
    enum { DIM         = DIFFOP::DIM };

  public:
    T_DifferentialOperator()
      : DifferentialOperator(DIFFOP::DIM_DMAT, 1, int(DIM_SPACE) > int(DIM_ELEMENT), DIFFOP::DIFFORDER)
    { ; }
    /*
    virtual int Dim() const { return DIFFOP::DIM_DMAT; }
    virtual bool Boundary() const { return int(DIM_SPACE) > int(DIM_ELEMENT); }
    virtual int DiffOrder() const { return DIFFOP::DIFFORDER; }
    */
    virtual string Name() const { return DIFFOP::Name(); }
    
    virtual bool operator== (const DifferentialOperator & diffop2) const
    { return typeid(*this) == typeid(diffop2); }

    
    virtual void
    CalcMatrix (const FiniteElement & bfel,
		const BaseMappedIntegrationPoint & bmip,
		SliceMatrix<double,ColMajor> mat, 
		LocalHeap & lh) const;

    virtual void
    CalcMatrix (const FiniteElement & bfel,
		const BaseMappedIntegrationPoint & bmip,
		SliceMatrix<Complex,ColMajor> mat, 
		LocalHeap & lh) const;

    virtual void
    CalcMatrix (const FiniteElement & bfel,
		const BaseMappedIntegrationRule & bmir,
		SliceMatrix<double,ColMajor> mat, 
		LocalHeap & lh) const;

    virtual void
    CalcMatrix (const FiniteElement & fel,
		const SIMD_BaseMappedIntegrationRule & mir,
		ABareMatrix<double> mat) const;
    
#ifndef FASTCOMPILE
    virtual void
    Apply (const FiniteElement & bfel,
	   const BaseMappedIntegrationPoint & bmip,
	   FlatVector<double> x, 
	   FlatVector<double> flux,
	   LocalHeap & lh) const;

    virtual void
    Apply (const FiniteElement & bfel,
	   const BaseMappedIntegrationRule & bmir,
	   FlatVector<double> x, 
	   FlatMatrix<double> flux,
	   LocalHeap & lh) const;

    virtual void
    Apply (const FiniteElement & bfel,
	   const BaseMappedIntegrationPoint & bmip,
	   FlatVector<Complex> x, 
	   FlatVector<Complex> flux,
	   LocalHeap & lh) const;

    virtual void
    Apply (const FiniteElement & bfel,
	   const BaseMappedIntegrationRule & bmir,
	   FlatVector<Complex> x, 
	   FlatMatrix<Complex> flux,
	   LocalHeap & lh) const;

    virtual void
    Apply (const FiniteElement & bfel,
	   const SIMD_BaseMappedIntegrationRule & bmir,
	   BareSliceVector<double> x, 
	   ABareMatrix<double> flux) const;
    // LocalHeap & lh) const;

    virtual void
    ApplyTrans (const FiniteElement & bfel,
		const BaseMappedIntegrationPoint & bmip,
		FlatVector<double> flux,
		FlatVector<double> x, 
		LocalHeap & lh) const;

    virtual void
    ApplyTrans (const FiniteElement & bfel,
		const BaseMappedIntegrationPoint & bmip,
		FlatVector<Complex> flux,
		FlatVector<Complex> x, 
		LocalHeap & lh) const;

    virtual void
    ApplyTrans (const FiniteElement & bfel,
		const BaseMappedIntegrationRule & bmir,
		FlatMatrix<double> flux,
		FlatVector<double> x, 
		LocalHeap & lh) const;

    virtual void
    ApplyTrans (const FiniteElement & bfel,
		const BaseMappedIntegrationRule & bmir,
		FlatMatrix<Complex> flux,
		FlatVector<Complex> x, 
		LocalHeap & lh) const;

    virtual void
    AddTrans (const FiniteElement & bfel,
              const SIMD_BaseMappedIntegrationRule & bmir,
              ABareMatrix<double> flux,
              BareSliceVector<double> x) const;
    // LocalHeap & lh) const;    
#endif
  };








  
  // new design, code is still experimental ...
  
  template <typename DOP, typename F>
  class T_FunctionDiffOp : public DifferentialOperator
  {

    // possible conversion from vector to scalar 
    class V2VS 
    {
      FlatVector<> v;
      FlatVector<Complex> vc;
    public:
      V2VS (FlatVector<> av) : v(av), vc(0,(Complex*)nullptr) { ; }
      V2VS (FlatVector<Complex> avc) : v(0,(double*)nullptr), vc(avc) { ; }
      
      template <int D>
      V2VS (Vec<D> av) : v(av) { ; }
      
      operator double () { return v(0); }
      
      operator FlatVector<> () { return v; }
      
      template <int D>
      operator Vec<D> () { return v; }

      template <int D>
      operator Vec<D,Complex> () { return vc; }
    };
    
    
    int dim;
    const F & func;
  public:
    
    T_FunctionDiffOp (const F & afunc, int adim) : func(afunc), dim(adim) { ; }
    
    virtual int Dim() const { return dim; }
    virtual int DiffOrder () const { return 0; }
    virtual void Apply (const FiniteElement & fel,
			const BaseMappedIntegrationPoint & mip,
			FlatVector<double> x, 
			FlatVector<double> flux,
			LocalHeap & lh) const 
    {
      Vec<DOP::DIM_DMAT> u;
      DOP::Apply (fel, static_cast<const MappedIntegrationPoint<3,3>&> (mip), x, u, lh);
      flux = func(V2VS(u));
    }

/*
    virtual void Apply (const FiniteElement & fel,
			const BaseMappedIntegrationPoint & mip,
			FlatVector<Complex> x, 
			FlatVector<Complex> flux,
			LocalHeap & lh) const 
    {
      Vec<DOP::DIM_DMAT,Complex> u;
      DOP::Apply (fel, static_cast<const MappedIntegrationPoint<3,3>&> (mip), x, u, lh);
      flux = func(V2VS(u));
    }
*/
  };
  
  
  template <typename DOP, typename F>
  shared_ptr<DifferentialOperator> CreateFunctionDiffOp (const DOP & dop, 
                                                         const F & func, int dim = 1)
  {
    return make_shared<T_FunctionDiffOp<DOP, F>> (func, dim);
  }


  /* examples:
     
  double myexp (double x)  { return exp(x); }
  Vec<1> myexpVec (FlatVector<> x)  { return Vec<1> (exp(x(0))); }

  CreateFunctionDiffOp(DiffOpId<2>(), myexpVec));
  */







  
}


#endif
