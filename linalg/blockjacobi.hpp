#ifndef FILE_BLOCK_JACOBI
#define FILE_BLOCK_JACOBI

/* *************************************************************************/
/* File:   blockjacobi.hpp                                                  */
/* Author: Joachim Schoeberl                                               */
/* Date:   06. Oct. 96                                                     */
/* *************************************************************************/

namespace ngla
{

  /**
     Base class for Block - Jacobi and Block Gauss Seidel smoother.
  */
  class NGS_DLL_HEADER BaseBlockJacobiPrecond : virtual public BaseMatrix
  {
  public:
    // enum COARSE_TYPE { NO_COARSE = 0, USER_COARSE = 1, DIRECT_COARSE = 2, SMOOTHING_COARSE = 3 };
  protected:
    /// the table defining the blocks
    Table<int> & blocktable;
    /// maximal block size
    int maxbs;

    // COARSE_TYPE ct;
  public:
    /// the blocktable define the blocks. ATTENTION: entries will be reordered !
    BaseBlockJacobiPrecond (Table<int> & ablocktable);

    /// deletes the table
    virtual ~BaseBlockJacobiPrecond ();

    /// performs steps Gauss-Seidel steps for the equation A x = b
    virtual void GSSmooth (BaseVector & x, const BaseVector & b,
			   int steps = 1) const = 0;

    /// performs steps Gauss-Seidel steps for the equation A x = b with partial residual y
    virtual void GSSmoothPartial (BaseVector & x, const BaseVector & b, BaseVector & y) const 
    {
      GSSmooth (x, b, 1);
    }


    /// does smoothing. The vector res contains the residuum (b-Ax) before and after the smoothing
    virtual void GSSmoothResiduum (BaseVector & x, const BaseVector & b,
				   BaseVector & res, int steps = 1) const = 0;

    /// does smoothing in reversed order
    virtual void GSSmoothBack (BaseVector & x, const BaseVector & b,
			       int steps = 1) const = 0;

    virtual void GSSmoothBackPartial (BaseVector & x, const BaseVector & b, BaseVector & y) const 
    {
      GSSmoothBack (x, b, 1);
    }


    /// reorders block entries for band-width minimization
    int Reorder (FlatArray<int> block, const MatrixGraph & graph,
		 FlatArray<int> usedflags,        // in and out: array of -1, size = graph.size
		 LocalHeap & lh);

    /*
    virtual void SetCoarseType ( string act) 
    {
      if ( strcmp ( act.c_str(), "DIRECT_COARSE") == 0 )
	{
	  ct = DIRECT_COARSE;
	}
      else if ( strcmp ( act.c_str(), "NO_COARSE") == 0 )
	{
	  ct = NO_COARSE;
	}
      else if ( strcmp ( act.c_str(), "USER_COARSE") == 0 )
	{
	  ct = USER_COARSE;
	}
      else if ( strcmp ( act.c_str(), "SMOOTHING_COARSE") == 0 )
	{
	  ct = SMOOTHING_COARSE;
	}
      else
	{
	  ct = NO_COARSE;
	}
    }
    */

    /*
    virtual void InitCoarseType (string act, const BitArray * freedofs) 
    {
      cerr << "BaseBlockJacobiPrecond :: InitCoarseType not implemented!" << endl;
    }
    */
  };




  /**
     A block-Jacobi preconditioner.
     The blocks are specified by a table container
  */
  template <class TM, class TV_ROW, class TV_COL>
  class  NGS_DLL_HEADER BlockJacobiPrecond : virtual public BaseBlockJacobiPrecond,
                                         virtual public S_BaseMatrix<typename mat_traits<TM>::TSCAL>
  {
  protected:
    /// a reference to the matrix
    const SparseMatrix<TM,TV_ROW,TV_COL> & mat;
    /// inverses of the small blocks
    Array<Matrix<TM>*> invdiag;


  public:
    // typedef typename mat_traits<TM>::TV_ROW TVX;
    typedef TV_ROW TVX;
    typedef typename mat_traits<TM>::TSCAL TSCAL;

    ///
    BlockJacobiPrecond (const SparseMatrix<TM,TV_ROW,TV_COL> & amat, 
			Table<int> & ablocktable);
    ///
    virtual ~BlockJacobiPrecond ();

    int Height() const { return mat.Height(); }
    virtual int VHeight() const { return mat.Height(); }
    int Width() const { return mat.Width(); }
    virtual int VWidth() const { return mat.Width(); }
    
    ///
    virtual shared_ptr<BaseVector> CreateVector () const 
    {
      return mat.CreateVector();
    }


    ///
    virtual void MultAdd (TSCAL s, const BaseVector & x, BaseVector & y) const 
    {
      static Timer timer("BlockJacobi::MultAdd");
      RegionTimer reg (timer);

      FlatVector<TVX> fx = x.FV<TVX> ();
      FlatVector<TVX> fy = y.FV<TVX> ();

      Vector<TVX> hxmax(maxbs);

      for (int i = 0; i < blocktable.Size(); i++)
	{
	  FlatArray<int> ind = blocktable[i];
	  if (!ind.Size()) continue;

	  FlatVector<TVX> hx = hxmax.Range(0, ind.Size()); // (ind.Size(), hxmax.Addr(0));

	  hx = s * fx(ind);
	  fy(ind) += *invdiag[i] * hx;
	}
    }


    ///
    virtual void MultTransAdd (TSCAL s, const BaseVector & x, BaseVector & y) const 
    {
      static int timer = NgProfiler::CreateTimer ("BlockJacobi::MultTransAdd");
      NgProfiler::RegionTimer reg (timer);

      int i, j;
      FlatVector<TVX> fx = x.FV<TVX> ();
      FlatVector<TVX> fy = y.FV<TVX> ();

      Vector<TVX> hxmax(maxbs);
      Vector<TVX> hymax(maxbs);

      for (i = 0; i < blocktable.Size(); i++)
	{
	  int bs = blocktable[i].Size();
	  if (!bs) continue;

	  FlatVector<TVX> hx = hxmax.Range(0,bs); // (bs, hxmax.Addr(0));
	  FlatVector<TVX> hy = hymax.Range(0,bs); // (bs, hymax.Addr(0));

	  for (j = 0; j < bs; j++)
	    hx(j) = fx(blocktable[i][j]);
	
	  hy = Trans(*invdiag[i]) * hx;

	  for (j = 0; j < bs; j++)
	    fy(blocktable[i][j]) += s * hy(j);
	}
    }


    ///
    virtual void GSSmooth (BaseVector & x, const BaseVector & b,
			   int steps = 1) const 
    {
      FlatVector<TVX> fb = b.FV<TVX> (); 
      FlatVector<TVX> fx = x.FV<TVX> ();

      Vector<TVX> hxmax(maxbs);
      Vector<TVX> hymax(maxbs);
      for (int k = 0; k < steps; k++)
	for (int i = 0; i < blocktable.Size(); i++)
	  {
	    int bs = blocktable[i].Size();
	    if (!bs) continue;
	  
	    FlatVector<TVX> hx = hxmax.Range(0,bs); // (bs, hxmax.Addr(0));
	    FlatVector<TVX> hy = hymax.Range(0,bs); // (bs, hymax.Addr(0));
	  
	    for (int j = 0; j < bs; j++)
	      {
		int jj = blocktable[i][j];
		hx(j) = fb(jj) - mat.RowTimesVector (jj, fx);
	      }
	  
	    hy = (*invdiag[i]) * hx;
	  
	    for (int j = 0; j < bs; j++)
	      fx(blocktable[i][j]) += hy(j);
	  }
    }

  
    virtual void GSSmoothResiduum (BaseVector & x, const BaseVector & b,
				   BaseVector & res, int steps = 1) const 
    {
      GSSmooth (x, b, 1);
      res = b - mat * x;
    }



  
    ///
    virtual void GSSmoothBack (BaseVector & x, const BaseVector & b,
			       int steps = 1) const 
    {
      const FlatVector<TVX> fb = b.FV<TVX> (); 
      FlatVector<TVX> fx = x.FV<TVX> ();

      Vector<TVX> hxmax(maxbs);
      Vector<TVX> hymax(maxbs);

      for (int k = 0; k < steps; k++)
	for (int i = blocktable.Size()-1; i >= 0; i--)
	  {
	    int bs = blocktable[i].Size();
	    if (!bs) continue;
	  
	    FlatVector<TVX> hx = hxmax.Range (0, bs); // (bs, hxmax.Addr(0));
	    FlatVector<TVX> hy = hymax.Range (0, bs); // (bs, hymax.Addr(0));
	  
	    for (int j = 0; j < bs; j++)
	      {
		int jj = blocktable[i][j];
		hx(j) = fb(jj) - mat.RowTimesVector (jj, fx);
	      }

	    hy = (*invdiag[i]) * hx;
	  
	    for (int j = 0; j < bs; j++)
	      fx(blocktable[i][j]) += hy(j);
	  }  
    }

    ///
    virtual void GSSmoothNumbering (BaseVector & x, const BaseVector & b,
				    const Array<int> & numbering, 
				    int forward = 1) const
    {
      ;
    }

    virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const
    {
      int nels = 0;
      for (int i = 0; i < blocktable.Size(); i++)
	{
	  int bs = blocktable[i].Size();
	  nels += bs*bs;
	}
      mu.Append (new MemoryUsageStruct ("BlockJac", nels*sizeof(TM), blocktable.Size()));
    }


  };





  /* **************** SYMMETRIC ****************** */


  ///
  template <class TM, class TV>
  class BlockJacobiPrecondSymmetric : 
    virtual public BaseBlockJacobiPrecond,
    virtual public S_BaseMatrix<typename mat_traits<TM>::TSCAL>
  {
  protected:
    const SparseMatrixSymmetric<TM,TV> & mat;

    // 
    enum { NBLOCKS = 20 };
    // DynamicMem<int> blockstart, blocksize, blockbw;
    // DynamicMem<TM> data[NBLOCKS];

    Array<int, size_t> blockstart, blocksize, blockbw;
    Array<TM, size_t> data[NBLOCKS];


    bool lowmem;
  public:
  
    typedef TV TVX;
    typedef typename mat_traits<TM>::TSCAL TSCAL;
  
    ///
    NGS_DLL_HEADER BlockJacobiPrecondSymmetric (const SparseMatrixSymmetric<TM,TV> & amat, 
				 Table<int> & ablocktable);
    
    /*
    BlockJacobiPrecondSymmetric (const SparseMatrixSymmetric<TM,TV> & amat, 
				 const FlatVector<TVX> & constraint,
				 Table<int> & ablocktable);
    */
    ///
    virtual ~BlockJacobiPrecondSymmetric ();
  
    FlatBandCholeskyFactors<TM> InvDiag (int i) const
    {
      return FlatBandCholeskyFactors<TM> (blocksize[i], 
					  blockbw[i], 
					  const_cast<TM*>(&data[i%NBLOCKS][blockstart[i]]));
    }

    void ComputeBlockFactor (FlatArray<int> block, int bw, FlatBandCholeskyFactors<TM> & inv) const;
  
    ///
    virtual void MultAdd (TSCAL s, const BaseVector & x, BaseVector & y) const;

    ///
    virtual void MultTransAdd (TSCAL s, const BaseVector & x, BaseVector & y) const;

    ///
    virtual shared_ptr<BaseVector> CreateVector () const 
    {
      return mat.CreateVector();
    }


    int Height() const { return mat.Height(); }
    virtual int VHeight() const { return mat.Height(); }
    int Width() const { return mat.Width(); }
    virtual int VWidth() const { return mat.Width(); }


    ///
    virtual void GSSmooth (BaseVector & x, const BaseVector & b,
			   int steps = 1) const;
  
    virtual void GSSmoothPartial (BaseVector & x, const BaseVector & b,
				  BaseVector & y) const;
  


    ///
    virtual void GSSmoothResiduum (BaseVector & x, const BaseVector & b,
				   BaseVector & res, int steps = 1) const;

  
    ///
    virtual void GSSmoothBack (BaseVector & x, const BaseVector & b,
			       int steps = 1) const; 
 

    virtual void GSSmoothBackPartial (BaseVector & x, const BaseVector & b,
				      BaseVector & y) const; 
 




    void SmoothBlock (int i, 
		      FlatVector<TVX> & x,
		      // const FlatVector<TVX> & b,
		      FlatVector<TVX> & y) const;
 

    ///
    virtual void GSSmoothNumbering (BaseVector & x, const BaseVector & b,
				    const Array<int> & numbering, 
				    int forward = 1) const
    {
      ;
    }
  
    /*  
	virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const
	{
	// Array<BandCholeskyFactors<TM>*> invdiag;


	int nels = 0;
	for (int i = 0; i < blocktable.Size(); i++)
	if (invdiag[i])
	{
	int s = invdiag[i]->Size();
	int bw = invdiag[i]->BandWidth();
	nels += s*bw - (bw * (bw-1)) / 2;
	nels += s;
	}
	mu.Append (new MemoryUsageStruct ("BlockJacSym Table", blocktable.NElements()*sizeof(int), 1));
	mu.Append (new MemoryUsageStruct ("BlockJacSym", nels*sizeof(TM), 2*blocktable.Size()));
	}
    */
  };


}

#endif
