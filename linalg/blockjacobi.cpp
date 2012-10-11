/**************************************************************************/
/* File:   blockjacobi.cpp                                                */
/* Author: Joachim Schoeberl                                              */
/* Date:   14. Aug. 2002                                                  */
/**************************************************************************/


#include <la.hpp>


namespace ngla
{
  
  BaseBlockJacobiPrecond :: 
  BaseBlockJacobiPrecond (Table<int> & ablocktable)
    : blocktable(ablocktable)
  {
    maxbs = 0;
    for (int i = 0; i < blocktable.Size(); i++)
      if (blocktable[i].Size() > maxbs)
	maxbs = blocktable[i].Size();
  }


  BaseBlockJacobiPrecond :: 
  ~BaseBlockJacobiPrecond ()
  {
    delete &blocktable;
  }


  int BaseBlockJacobiPrecond ::
  Reorder (FlatArray<int> block, const MatrixGraph & graph,
	   FlatArray<int> block_inv,
	   LocalHeap & lh)
  {
    try
      {
	// a cheap reordering algorithm:
	
	// bool print = 0;
	
	int n = block.Size();
	
	void * heapp = lh.GetPointer();


	FlatArray<int> reorder(n, lh), newnum(n, lh), dist(n, lh), cluster0(n,lh);
	// FlatArray<int> block_inv(graph.Size(), lh);
	
	// for(int i=0; i<graph.Size(); i++)
	// block_inv[i] = -1;


	for (int i = 0; i < n; i++)
	  if (block[i] >= 0 && block[i] < graph.Size())
	    {
	      if (block_inv[block[i]] != -1)
		{
		  cout << "block has double elements " << endl;
		  cout << block_inv[block[i]] << " and " << i << endl;
		  cout << block << endl;
		}
	      block_inv[block[i]] = i;
	    }
	  else
	    {
	      cerr << "block[" << i << "] out of range" << endl;
	      cerr << "block = " << block << endl;
	      (*testout) << "block[" << i << "] out of range" << endl;
	      (*testout) << "block = " << block << endl;
	    }

	/*
	// debug only:
	for (int i = 0; i < n; i++)
	for (int j = 0; j < n; j++)
	if (i != j && block[i] == block[j])
	{
	cout << "block has double elements" << endl;
	cout << "i = " << i << ", j = " << j << endl;
	cout << block << endl;
	}
	*/

	// check for sperated blocks:
	cluster0 = 0;
	cluster0[0] = 1;

	bool changed;
	do
	  {
	    changed = 0;
	    for (int j = 0; j < n; j++)
	      {
		FlatArray<int> row = graph.GetRowIndices(block[j]);
		for (int k = 0; k < row.Size(); k++)
		  {
		    int kk = block_inv[row[k]];
		    if (kk >= 0 && kk < n)
		      if (block[kk] == row[k])
			{
			  if (cluster0[j] != cluster0[kk]) 
			    {
			      cluster0[j] = 1;
			      cluster0[kk] = 1;
			      changed = 1;
			    }
			  //			  if (cluster0[j])  cluster0[kk] = 1;
			  //			  if (cluster0[kk]) cluster0[j] = 1;
			}
		  }
	      }
	  }
	while (changed);
	
	/*
	// cout << "cluster0, 2 = " << cluster0 << endl;
	cluster0 = 0;
	cluster0[0] = 1;
	for (i = 0; i < n; i++)
	for (j = 0; j < n; j++)
	for (k = 0; k < j; k++)
	if ((cluster0[j] || cluster0[k]) && 
	connected.Test(j,k))
	{
	cluster0[j] = 1;
	cluster0[k] = 1;
	}

	// cout << "cluster0, 1 = " << cluster0 << endl;
	*/

	int cnt = 0;
	for (int i = 0; i < n; i++)
	  if (cluster0[i])
	    {
	      newnum[cnt] = block[i];
	      cnt++;
	    }

	if (cnt < n)
	  {
	    // seperated clusters
	    int cnt2 = cnt;
	    for (int i = 0; i < n; i++)
	      if (!cluster0[i])
		newnum[cnt2++] = block[i];
	    
	    for (int i = 0; i < n; i++)
	      block[i] = newnum[i];

	    for (int i = 0; i < n; i++)
	      block_inv[block[i]] = -1;

	    // block = newnum;

	    lh.CleanUp(heapp);
	    
	    int bw1 = 
	      Reorder (FlatArray<int> (cnt, &block[0]), graph, block_inv, lh);
	    int bw2 =
	      Reorder (FlatArray<int> (cnt2-cnt, &block[cnt]), graph, block_inv, lh);
	    
	    return max2(bw1, bw2);
	  }
	

	// compute distance function

	int pstart = 0;
	for (int step = 0; step < 3; step++)
	  {
	    /*
	      dist = n+1;
	      dist[pstart] = 0;
	
	      bool changed;

	      do
	      {
	      changed = 0;
	      for (i = 0; i < n; i++)
	      for (j = 0; j < n; j++)
	      if (i != j && connected.Test (i,j))
	      {
	      if (dist[i] > dist[j]+1)
	      {
	      dist[i] = dist[j]+1;
	      changed = 1;
	      }
	      else if (dist[j] > dist[i]+1)
	      {
	      dist[j] = dist[i]+1;
	      changed = 1;
	      }
	      }
	      }
	      while (changed);
	    */

	    bool changed;
	    dist = n+1;
	    dist[pstart] = 0;
	    do
	      {
		changed = 0;
		for (int i = 0; i < n; i++)
		  {
		    FlatArray<int> row = graph.GetRowIndices(block[i]);

		    for (int j = 0; j < row.Size(); j++)
		      {
			int jj = block_inv[row[j]];

			if (jj >= 0 && jj < n)
			  if (block[jj] == row[j])		    
			    {
			      if (dist[i] > dist[jj]+1)
				{
				  dist[i] = dist[jj]+1;
				  changed = 1;
				}
			      else if (dist[jj] > dist[i]+1)
				{
				  dist[jj] = dist[i]+1;
				  changed = 1;
				}
			    }
		      }
		  }
	      }
	    while (changed);

	    
	    int maxval = 0;
	    for (int i = 0; i < n; i++)
	      if (dist[i] > maxval)
		{
		  maxval = dist[i];
		  pstart = i;
		}
	    if (maxval > n)
	      {
		cerr << "Blockjacobi, reorder: separated block" << endl;
		cout << "block: " << block << endl;

		(*testout) << "Blockjacobi, reorder: separated block" << endl;
		(*testout) << "block: " << block << endl;
	      }	      
	  }
	

	cnt = 0;
	for (int i = 0; i < n; i++)
	  for (int j = 0; j < n; j++)
	    if (dist[j] == i)
	      {
		reorder[cnt] = j;
		cnt++;
	      }
	if (cnt != n) cerr << "BlockJac, reorder: n = " << n << " != cnt = " << cnt << endl;

	int bw = 1; 
	/*
	  for (i = 0; i < n; i++)
	  for (j = 0; j < n; j++)
	  if (connected.Test(reorder[i], reorder[j]))
	  bw = max2(bw, abs(i-j)+1);
	*/
	for (int i = 0; i < n; i++)
	  newnum[reorder[i]] = i;
	for (int j = 0; j < n; j++)
	  {
	    FlatArray<int> row = graph.GetRowIndices(block[j]);
	    for (int k = 0; k < row.Size(); k++)
	      {
		int kk = block_inv[row[k]];
		if (kk >= 0 && kk < n)
		  if (block[kk] == row[k])
		    {
		      int inv_j = newnum[j];
		      int inv_k = newnum[kk];
		      bw = max2(bw, abs(inv_j-inv_k)+1);
		    }
	      }
	  }


	for (int i = 0; i < n; i++)
	  newnum[i] = block[reorder[i]];
	for (int i = 0; i < n; i++)
	  block[i] = newnum[i];


	lh.CleanUp(heapp);

	for (int i = 0; i < n; i++)
	  block_inv[block[i]] = -1;

	return bw;
      }
    catch (Exception & e)
      {
	e.Append ("in Reorder\n");
	throw;
      }
  }



  ///
  template <class TM, class TV_ROW, class TV_COL>
  BlockJacobiPrecond<TM, TV_ROW, TV_COL> ::
  BlockJacobiPrecond (const SparseMatrix<TM,TV_ROW,TV_COL> & amat, 
		      Table<int> & ablocktable)
    : BaseBlockJacobiPrecond(ablocktable), mat(amat), 
      invdiag(ablocktable.Size())
  { 
    cout << "BlockJacobi Preconditioner, constructor called, #blocks = " << blocktable.Size() << endl;


    clock_t prevtime = clock();
    for (int i = 0; i < blocktable.Size(); i++)
      {
        if (clock()-prevtime > 0.1 * CLOCKS_PER_SEC)
          {
            cout << "\rBuilding block " << i << flush;
            prevtime = clock();
          }


	int bs = blocktable[i].Size();
	if (!bs) 
	  {
	    invdiag[i] = 0;
	    continue;
	  }
	
	Matrix<TM> blockmat(bs);
	invdiag[i] = new Matrix<TM> (bs);
        
	for (int j = 0; j < bs; j++)
	  for (int k = 0; k < bs; k++)
	    blockmat(j,k) = mat(blocktable[i][j], blocktable[i][k]);
	
        CalcInverse (blockmat, *invdiag[i]);
      }
    cout << "\rBlockJacobi Preconditioner built" << endl;
  }

  ///
  template <class TM, class TV_ROW, class TV_COL>
  BlockJacobiPrecond<TM, TV_ROW, TV_COL> ::
  ~BlockJacobiPrecond () 
  {
    for (int i = 0; i < invdiag.Size(); i++)
      delete invdiag[i];
  }




  ///
  template <class TM, class TV>
  BlockJacobiPrecondSymmetric<TM,TV> ::
  BlockJacobiPrecondSymmetric (const SparseMatrixSymmetric<TM,TV> & amat, 
			       Table<int> & ablocktable)
    : BaseBlockJacobiPrecond(ablocktable), mat(amat)
  { 
    cout << "symmetric BlockJacobi Preconditioner 2, constructor called, #blocks = " << blocktable.Size() << endl;

    lowmem = false;
    // lowmem = true;
    
    int maxbs = 0;
    int n = blocktable.Size();
	
    for (int i = 0; i < n; i++)
      if (blocktable[i].Size() > maxbs)
	maxbs = blocktable[i].Size();


    blockstart.Alloc(n);
    blocksize.Alloc(n);
    blockbw.Alloc(n);

    blockstart.SetName ("BlockJacobi, Table 1");
    blocksize.SetName ("BlockJacobi, Table 2");
    blockbw.SetName ("BlockJacobi, Table 3");

    for (int i = 0; i < NBLOCKS; i++)
      data[i].SetName ("BlockJacobi");

    // int alloc = n;
    int memneed[NBLOCKS];
    for (int i = 0; i < NBLOCKS; i++)
      memneed[i] = 0;

    {
      LocalHeap lh (20000 + 5*sizeof(int)*maxbs, "blockjacobi-heap"); 
      Array<int> block_inv(amat.Height());
      block_inv = -1;

      for (int i = 0; i < blocktable.Size(); i++)
	{
	  int bs = blocktable[i].Size();
	  
	  if (!bs) continue;
	  
	  blockbw[i] = Reorder (blocktable[i], mat, block_inv, lh);
	  blocksize[i] = bs;

	  blockstart[i] = memneed[i%NBLOCKS];
	  memneed[i%NBLOCKS] += FlatBandCholeskyFactors<TM>::RequiredMem (bs, blockbw[i]);
	  lh.CleanUp();
	}
    }

    if (!lowmem)
      {
	for (int i = 0; i < NBLOCKS; i++)
	  data[i].Alloc (memneed[i]);

        clock_t prevtime = clock();

	
	for (int i = 0; i < blocktable.Size(); i++)
	  {
            if (clock()-prevtime > 0.1 * CLOCKS_PER_SEC)
              {
                cout << "\rBuilding block " << i << flush;
                prevtime = clock();
              }

	    int bs = blocktable[i].Size();
	    
	    if (!bs) continue;
	    int bw = blockbw[i];

	    try
	      {
		FlatBandCholeskyFactors<TM> inv (bs, bw, &data[i%NBLOCKS][blockstart[i]]);
		ComputeBlockFactor (blocktable[i], bw, inv);
	      }
	    catch (Exception & e)
	      {
		cout << "block singular !" << endl;
		(*testout) << "block nr = " << i << endl;
		(*testout) << "caught: " << e.What() << endl;
		(*testout) << "entries = " << blocktable[i] << endl;
		throw;
	      }
	  }
      }

    cout << "\rBuilt symmetric BlockJacobi Preconditioner" << endl;
  }




  template <class TM, class TV>
  BlockJacobiPrecondSymmetric<TM,TV> ::
  ~BlockJacobiPrecondSymmetric ()
  {
    ;
  }

  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  ComputeBlockFactor (FlatArray<int> block, int bw, FlatBandCholeskyFactors<TM> & inv) const
  {
    int bs = block.Size();

    ArrayMem<TM, 10000/sizeof(TM)+1> mem(bs*bw);
    FlatSymBandMatrix<TM> blockmat(bs, bw, &mem[0]);


    blockmat = TM(0);
    for (int j = 0; j < bs; j++)
      for (int k = 0; k < bs; k++)
	if (block[j] >= block[k])
	  {
	    if (abs (j-k) < bw)
	      {
		TM val = mat(block[j], block[k]);
		if (j >= k)
		  blockmat(j,k) = val;
		else
		  blockmat(k,j) = Trans (val);
	      }
	  }    

    inv.Factor (blockmat);
  } 





  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  MultAdd (TSCAL s, const BaseVector & x, BaseVector & y) const 
  {
    static Timer timer("BlockJacobiSymmetric::MultAdd");
    RegionTimer reg (timer);

    FlatVector<TVX> fx = x.FV<TVX> ();
    FlatVector<TVX> fy       = y.FV<TVX> ();

    Vector<TVX> hxmax(maxbs);
    Vector<TVX> hymax(maxbs);

    for (int i = 0; i < blocktable.Size(); i++)
      {
	int bs = blocktable[i].Size();
	if (!bs) continue;

	FlatVector<TVX> hx(bs, hxmax.Addr(0));
	FlatVector<TVX> hy(bs, hymax.Addr(0));

	for (int j = 0; j < bs; j++)
	  hx(j) = fx(blocktable[i][j]);
	
	InvDiag(i).Mult (hx, hy);

	for (int j = 0; j < bs; j++)
	  fy(blocktable[i][j]) += s * hy(j);
      }
  }


  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  MultTransAdd (TSCAL s, const BaseVector & x, BaseVector & y) const 
  {
    MultAdd (s, x, y);
  }
  


  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  GSSmooth (BaseVector & x, const BaseVector & b, int steps) const 
  {
    static Timer timer ("BlockJacobiPrecondSymmetric::GSSmooth");
    RegionTimer reg(timer);

    FlatVector<TVX> fb = b.FV<TVX> ();
    FlatVector<TVX> fx = x.FV<TVX> ();

    Vector<TVX> fy(fx.Size());

    // y = b - (D L^T) x
    fy = fb;
    for (int j = 0; j < mat.Height(); j++)
      mat.AddRowTransToVector (j, -fx(j), fy);

    for (int k = 1; k <= steps; k++)
      for (int i = 0; i < blocktable.Size(); i++)
	SmoothBlock (i, fx, /* fb, */ fy);
  }
  
  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  GSSmoothPartial (BaseVector & x, const BaseVector & b,
	    BaseVector & y) const 
  {
    static Timer timer ("BlockJacobiPrecondSymmetric::GSSmooth - partial res");
    RegionTimer reg(timer);

    FlatVector<TVX> fx = x.FV<TVX> ();
    FlatVector<TVX> fy = y.FV<TVX> ();

    for (int i = 0; i < blocktable.Size(); i++)
      SmoothBlock (i, fx, fy);
  }
  


  ///
  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  GSSmoothResiduum (BaseVector & x, const BaseVector & b,
		    BaseVector & res, int steps) const 
  {
    static Timer timer ("BlockJacobiPrecondSymmetric::GSSmooth - residuum");
    RegionTimer reg(timer);


    // x is 0 on input
    // b is res
    
    res = b;

    // res is partial residual
    for (int k = 1; k <= steps; k++)
      GSSmoothPartial (x, b, res);

    mat.MultAdd1 (-1, x, res);
    // res = b - mat * x;
  }
  
  ///
  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  GSSmoothBack (BaseVector & x, const BaseVector & b,
		int steps) const 
  {
    static Timer timer ("BlockJacobiPrecondSymmetric::SmoothBack");
    RegionTimer reg(timer);

    VVector<TVX> y(x.Size());

    // y = b - (D L^T) x
    y = 1.0*b;
    mat.MultAdd2 (-1, x, y);

    for (int k = 1; k <= steps; k++)
      GSSmoothBackPartial (x, b, y);
  }


  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  GSSmoothBackPartial (BaseVector & x, const BaseVector & b,
		       BaseVector & y) const 
  {
    static Timer timer ("BlockJacobiPrecondSymmetric::GSSmoothBack - partial res");
    RegionTimer reg(timer);

    FlatVector<TVX> fx = x.FV<TVX> ();
    FlatVector<TVX> fy = y.FV<TVX> ();

    for (int i = blocktable.Size()-1; i >= 0; i--)
      SmoothBlock (i, fx, fy);
  }



  template <class TM, class TV>
  void BlockJacobiPrecondSymmetric<TM,TV> :: 
  SmoothBlock (int i, 
	       FlatVector<TVX> & x,
	       FlatVector<TVX> & y) const
  {
    FlatArray<int> row = blocktable[i];

    int bs = row.Size();
    if (bs == 0) return;

    VectorMem<1000,TVX> di (bs);
    VectorMem<1000,TVX> wi (bs);

    // di = P_i (y - L x)
    for (int j = 0; j < bs; j++)
      di(j) = y(row[j]) - mat.RowTimesVectorNoDiag (row[j], x);
    
    if (!lowmem)
      InvDiag(i).Mult (di, wi);
    else
      {
	int bw = blockbw[i];
	int bs = blocktable[i].Size();
	ArrayMem<TM, 10000/sizeof(TM)+1> mem(bs*bw);
	FlatBandCholeskyFactors<TM> inv(bs, bw, &mem[0]);

	ComputeBlockFactor (blocktable[i], bw, inv);

	inv.Mult (di, wi);
      }

    // x += P_i w
    // y -= (D L^t) P_i w
    for (int j = 0; j < bs; j++)
      {
	x(row[j]) += wi(j);
	mat.AddRowTransToVector (row[j], -wi(j), y);
      }
  }








  // compiled separately, for testing only
  template class BlockJacobiPrecond<double>;
  template class BlockJacobiPrecond<Complex>;
  template class BlockJacobiPrecond<double, Complex, Complex>;

  template class BlockJacobiPrecondSymmetric<double>;
  template class BlockJacobiPrecondSymmetric<Complex>;
  template class BlockJacobiPrecondSymmetric<double, Complex>;

#if MAX_SYS_DIM >= 1
  template class BlockJacobiPrecond<Mat<1,1,double> >;
  template class BlockJacobiPrecond<Mat<1,1,Complex> >;
#endif
#if MAX_SYS_DIM >= 2
  template class BlockJacobiPrecond<Mat<2,2,double> >;
  template class BlockJacobiPrecond<Mat<2,2,Complex> >;
#endif
#if MAX_SYS_DIM >= 3
  template class BlockJacobiPrecond<Mat<3,3,double> >;
  template class BlockJacobiPrecond<Mat<3,3,Complex> >;
#endif
#if MAX_SYS_DIM >= 4
  template class BlockJacobiPrecond<Mat<4,4,double> >;
  template class BlockJacobiPrecond<Mat<4,4,Complex> >;
#endif
#if MAX_SYS_DIM >= 5
  template class BlockJacobiPrecond<Mat<5,5,double> >;
  template class BlockJacobiPrecond<Mat<5,5,Complex> >;
#endif
#if MAX_SYS_DIM >= 6
  template class BlockJacobiPrecond<Mat<6,6,double> >;
  template class BlockJacobiPrecond<Mat<6,6,Complex> >;
#endif
#if MAX_SYS_DIM >= 7
  template class BlockJacobiPrecond<Mat<7,7,double> >;
  template class BlockJacobiPrecond<Mat<7,7,Complex> >;
#endif
#if MAX_SYS_DIM >= 8
  template class BlockJacobiPrecond<Mat<8,8,double> >;
  template class BlockJacobiPrecond<Mat<8,8,Complex> >;
#endif
  
#if MAX_SYS_DIM >= 1
  template class BlockJacobiPrecondSymmetric<Mat<1,1,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<1,1,Complex> >;
#endif
#if MAX_SYS_DIM >= 2
  template class BlockJacobiPrecondSymmetric<Mat<2,2,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<2,2,Complex> >;
#endif
#if MAX_SYS_DIM >= 3
  template class BlockJacobiPrecondSymmetric<Mat<3,3,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<3,3,Complex> >;
#endif
#if MAX_SYS_DIM >= 4
  template class BlockJacobiPrecondSymmetric<Mat<4,4,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<4,4,Complex> >;
#endif
#if MAX_SYS_DIM >= 5
  template class BlockJacobiPrecondSymmetric<Mat<5,5,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<5,5,Complex> >;
#endif
#if MAX_SYS_DIM >= 6
  template class BlockJacobiPrecondSymmetric<Mat<6,6,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<6,6,Complex> >;
#endif
#if MAX_SYS_DIM >= 7
  template class BlockJacobiPrecondSymmetric<Mat<7,7,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<7,7,Complex> >;
#endif
#if MAX_SYS_DIM >= 8
  template class BlockJacobiPrecondSymmetric<Mat<8,8,double> >;
  template class BlockJacobiPrecondSymmetric<Mat<8,8,Complex> >;
#endif

  
#ifdef CACHEBLOCKSIZE
  template class BlockJacobiPrecond<double, Vec<CACHEBLOCKSIZE>, Vec<CACHEBLOCKSIZE> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<CACHEBLOCKSIZE> >;
#endif

#if MAX_CACHEBLOCKS >= 2
  template class BlockJacobiPrecond<double, Vec<2,double>, Vec<2,double> >;
#endif
#if MAX_CACHEBLOCKS >= 3
  template class BlockJacobiPrecond<double, Vec<3,double>, Vec<3,double> >;
  template class BlockJacobiPrecond<double, Vec<4,double>, Vec<4,double> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class BlockJacobiPrecond<double, Vec<5,double>, Vec<5,double> >;
  template class BlockJacobiPrecond<double, Vec<6,double>, Vec<6,double> >;
  template class BlockJacobiPrecond<double, Vec<7,double>, Vec<7,double> >;
  template class BlockJacobiPrecond<double, Vec<8,double>, Vec<8,double> >;
  template class BlockJacobiPrecond<double, Vec<9,double>, Vec<9,double> >;
  template class BlockJacobiPrecond<double, Vec<10,double>, Vec<10,double> >;
  template class BlockJacobiPrecond<double, Vec<11,double>, Vec<11,double> >;
  template class BlockJacobiPrecond<double, Vec<12,double>, Vec<12,double> >;
  template class BlockJacobiPrecond<double, Vec<13,double>, Vec<13,double> >;
  template class BlockJacobiPrecond<double, Vec<14,double>, Vec<14,double> >;
  template class BlockJacobiPrecond<double, Vec<15,double>, Vec<15,double> >;
#endif

#if MAX_CACHEBLOCKS >= 2
  template class BlockJacobiPrecond<Complex, Vec<2,Complex>, Vec<2,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 3
  template class BlockJacobiPrecond<Complex, Vec<3,Complex>, Vec<3,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<4,Complex>, Vec<4,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class BlockJacobiPrecond<Complex, Vec<5,Complex>, Vec<5,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<6,Complex>, Vec<6,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<7,Complex>, Vec<7,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<8,Complex>, Vec<8,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<9,Complex>, Vec<9,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<10,Complex>, Vec<10,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<11,Complex>, Vec<11,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<12,Complex>, Vec<12,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<13,Complex>, Vec<13,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<14,Complex>, Vec<14,Complex> >;
  template class BlockJacobiPrecond<Complex, Vec<15,Complex>, Vec<15,Complex> >;
#endif

#if MAX_CACHEBLOCKS >= 2
  template class BlockJacobiPrecond<double, Vec<2,Complex>, Vec<2,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 3
  template class BlockJacobiPrecond<double, Vec<3,Complex>, Vec<3,Complex> >;
  template class BlockJacobiPrecond<double, Vec<4,Complex>, Vec<4,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class BlockJacobiPrecond<double, Vec<5,Complex>, Vec<5,Complex> >;
  template class BlockJacobiPrecond<double, Vec<6,Complex>, Vec<6,Complex> >;
  template class BlockJacobiPrecond<double, Vec<7,Complex>, Vec<7,Complex> >;
  template class BlockJacobiPrecond<double, Vec<8,Complex>, Vec<8,Complex> >;
  template class BlockJacobiPrecond<double, Vec<9,Complex>, Vec<9,Complex> >;
  template class BlockJacobiPrecond<double, Vec<10,Complex>, Vec<10,Complex> >;
  template class BlockJacobiPrecond<double, Vec<11,Complex>, Vec<11,Complex> >;
  template class BlockJacobiPrecond<double, Vec<12,Complex>, Vec<12,Complex> >;
  template class BlockJacobiPrecond<double, Vec<13,Complex>, Vec<13,Complex> >;
  template class BlockJacobiPrecond<double, Vec<14,Complex>, Vec<14,Complex> >;
  template class BlockJacobiPrecond<double, Vec<15,Complex>, Vec<15,Complex> >;
#endif

#if MAX_CACHEBLOCKS >= 2
  template class BlockJacobiPrecondSymmetric<double, Vec<2,double> >;
#endif
#if MAX_CACHEBLOCKS >= 3
  template class BlockJacobiPrecondSymmetric<double, Vec<3,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<4,double> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class BlockJacobiPrecondSymmetric<double, Vec<5,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<6,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<7,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<8,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<9,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<10,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<11,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<12,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<13,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<14,double> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<15,double> >;
#endif

#if MAX_CACHEBLOCKS >= 2
  template class BlockJacobiPrecondSymmetric<Complex, Vec<2,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 3
  template class BlockJacobiPrecondSymmetric<Complex, Vec<3,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<4,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class BlockJacobiPrecondSymmetric<Complex, Vec<5,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<6,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<7,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<8,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<9,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<10,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<11,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<12,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<13,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<14,Complex> >;
  template class BlockJacobiPrecondSymmetric<Complex, Vec<15,Complex> >;
#endif

#if MAX_CACHEBLOCKS >= 2
  template class BlockJacobiPrecondSymmetric<double, Vec<2,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 3
  template class BlockJacobiPrecondSymmetric<double, Vec<3,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<4,Complex> >;
#endif
#if MAX_CACHEBLOCKS >= 5
  template class BlockJacobiPrecondSymmetric<double, Vec<5,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<6,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<7,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<8,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<9,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<10,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<11,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<12,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<13,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<14,Complex> >;
  template class BlockJacobiPrecondSymmetric<double, Vec<15,Complex> >;
#endif
  

}
