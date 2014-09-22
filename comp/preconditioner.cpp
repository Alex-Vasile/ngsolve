#include <comp.hpp>
#include <multigrid.hpp>
#include <solve.hpp>
#include <parallelngs.hpp>

namespace ngcomp
{
  using namespace ngmg;

  
  Preconditioner :: Preconditioner (const PDE * const apde, const Flags & aflags, 
				    const string aname)
    : NGS_Object(apde->GetMeshAccess(), aname), flags(aflags)
  {
    test = flags.GetDefineFlag ("test");
    timing = flags.GetDefineFlag ("timing");
    print = flags.GetDefineFlag ("print");
    laterupdate = flags.GetDefineFlag ("laterupdate");
    testresult_ok = testresult_min = testresult_max = NULL;
    // using lapack for testing EV
    uselapack = flags.GetDefineFlag ("lapacktest");
    if ( uselapack ) test = 1;

    if(test)
      {
	string testresult_ok_name = flags.GetStringFlag ("testresultok","");
	string testresult_min_name = flags.GetStringFlag ("testresultmin","");
	string testresult_max_name = flags.GetStringFlag ("testresultmax","");
	
	if(testresult_ok_name != "") testresult_ok = &(const_cast<PDE*>(apde)->GetVariable(testresult_ok_name));
	if(testresult_min_name != "") testresult_min = &(const_cast<PDE*>(apde)->GetVariable(testresult_min_name));
	if(testresult_max_name != "") testresult_max = &(const_cast<PDE*>(apde)->GetVariable(testresult_max_name));
      }

    on_proc = int ( flags.GetNumFlag("only_on", -1));
  }
  
  Preconditioner :: ~Preconditioner ()
  {
    ;
  }

  void Preconditioner :: Test () const
  {
    cout << IM(1) << "Compute eigenvalues" << endl;
    const BaseMatrix & amat = GetAMatrix();
    const BaseMatrix & pre = GetMatrix();
    int eigenretval;

    if ( !this->uselapack )
      {
        EigenSystem eigen (amat, pre);
        eigen.SetPrecision(1e-30);
        eigen.SetMaxSteps(1000); 
        
        eigen.SetPrecision(1e-15);
        eigenretval = eigen.Calc();
        eigen.PrintEigenValues (*testout);
        cout << IM(1) << " Min Eigenvalue : "  << eigen.EigenValue(1) << endl; 
        cout << IM(1) << " Max Eigenvalue : " << eigen.MaxEigenValue() << endl; 
        cout << IM(1) << " Condition   " << eigen.MaxEigenValue()/eigen.EigenValue(1) << endl; 
        (*testout) << " Min Eigenvalue : "  << eigen.EigenValue(1) << endl; 
        (*testout) << " Max Eigenvalue : " << eigen.MaxEigenValue() << endl; 
        
        if(testresult_ok) *testresult_ok = eigenretval;
        if(testresult_min) *testresult_min = eigen.EigenValue(1);
        if(testresult_max) *testresult_max = eigen.MaxEigenValue();
        
        
        //    (*testout) << " Condition   " << eigen.MaxEigenValue()/eigen.EigenValue(1) << endl; 
        //    for (int i = 1; i < min2 (10, eigen.NumEigenValues()); i++)
        //      cout << "cond(i) = " << eigen.MaxEigenValue() / eigen.EigenValue(i) << endl;
        (*testout) << " Condition   " << eigen.MaxEigenValue()/eigen.EigenValue(1) << endl;
        
      }

    else
      {
#ifdef LAPACK   
        int n = amat.Height();
	int n_elim = 0;
	BitArray internaldofs (n);
	internaldofs.Clear();

	for ( int i = 0; i < n; i++ )
	  {
	    FlatArray<int> rowindices = 
	      dynamic_cast<const BaseSparseMatrix&>(amat).GetRowIndices(i);
	    if ( rowindices.Size() <= 1 )
	      internaldofs.Set(i);
	    else
	      n_elim++;
	  }

        Matrix<Complex> mat(n_elim), mat2(n_elim), ev(n_elim);
        BaseVector & v1 = *amat.CreateVector();
        BaseVector & v2 = *amat.CreateVector();
        FlatVector<Complex> fv1 = v1.FVComplex();
        // FlatVector<Complex> fv2 = v2.FVComplex();
        
	int i_elim = 0, j_elim = 0;
        for (int i = 0; i < n; i++)
          {
	    if ( internaldofs.Test(i) ) continue;
	    j_elim = 0;
            fv1 = 0.0;
            fv1(i) = 1.0;
            v2 = amat * v1;
            v1 = pre * v2;

            for (int j = 0; j < n; j++)
	      {
		if ( internaldofs.Test(j) ) continue;
		mat(j_elim,i_elim) = fv1(j);
		j_elim++;
	      }
	    i_elim++;
          }

        mat2 = Complex(0.0);
        for (int i = 0; i < n_elim; i++)
          mat2(i,i) = 1.0;
        
        cout << "call lapack" << endl;
        Vector<Complex> lami(n_elim);
        // LapackEigenValues (mat, lami);
        LaEigNSSolve (n_elim, &mat(0,0), &mat2(0,0), &lami(0), 1, &ev(0,0), 0, 'B');

        ofstream out ("eigenvalues.out");
        for (int i = 0; i < n_elim; i++)
          out << lami(i).real() << " " << lami(i).imag() << "\n";
#endif
	;
      }
  }



  void Preconditioner :: Timing () const
  {
    cout << IM(1) << "Timing Preconditioner ... " << flush;
    const BaseMatrix & amat = GetAMatrix();
    const BaseMatrix & pre = GetMatrix();

    clock_t starttime;
    double time;
    starttime = clock();

    BaseVector & vecf = *pre.CreateVector();
    BaseVector & vecu = *pre.CreateVector();

    vecf = 1;
    int steps = 0;
    do
      {
	vecu = pre * vecf;
	steps++;
	time = double(clock() - starttime) / CLOCKS_PER_SEC;
      }
    while (time < 2.0);

    cout << IM(1) << " 1 step takes " << time / steps << " seconds" << endl;


    starttime = clock();
    steps = 0;
    do
      {
	vecu = amat * vecf;
	steps++;
	time = double(clock() - starttime) / CLOCKS_PER_SEC;
      }
    while (time < 2.0);

    cout << IM(1) << ", 1 matrix takes "
	 << time / steps << " seconds" << endl;
  }





  MGPreconditioner :: MGPreconditioner (const PDE & pde, const Flags & aflags, const string aname)
    : Preconditioner (&pde,aflags,aname)
  {
    // cout << "*** MGPreconditioner constructor called" << endl;
    mgtest = flags.GetDefineFlag ("mgtest");
    mgfile = flags.GetStringFlag ("mgfile","mgtest.out"); 
    mgnumber = int(flags.GetNumFlag("mgnumber",1)); 
   
 

    const MeshAccess & ma = pde.GetMeshAccess();
    bfa = pde.GetBilinearForm (flags.GetStringFlag ("bilinearform", NULL));

    shared_ptr<LinearForm> lfconstraint = 
      pde.GetLinearForm (flags.GetStringFlag ("constraint", ""),1);
    shared_ptr<FESpace> fes = bfa->FESpacePtr();
    

    shared_ptr<BilinearForm> lo_bfa = bfa;
    shared_ptr<FESpace> lo_fes = fes;

    if (bfa->GetLowOrderBilinearForm())
      {
	lo_bfa = bfa->GetLowOrderBilinearForm();
	lo_fes = fes->LowOrderFESpacePtr();
      }
    /*
    else if (id == 0 && ntasks > 1 )  // not supported anymore
      {
	lo_bfa = bfa;
	lo_fes = &fes;
      }
    */

    Smoother * sm = NULL;
    //    const char * smoother = flags.GetStringFlag ("smoother", "point");
    smoothertype = flags.GetStringFlag ("smoother", "point");

    if (smoothertype == "point")
      {
	sm = new GSSmoother (ma, *lo_bfa);
      }
    else if (smoothertype == "line")
      {
	sm = new AnisotropicSmoother (ma, *lo_bfa);
      }
    else if (smoothertype == "block") 
      {
	if (!lfconstraint)
	  sm = new BlockSmoother (ma, *lo_bfa, flags);
	else
	  sm = new BlockSmoother (ma, *lo_bfa, *lfconstraint, flags);
      }
    /*
    else if (smoothertype == "potential")
      {
	sm = new PotentialSmoother (ma, *lo_bfa);
      }
    */
    else
      cerr << "Unknown Smoother " << smoothertype << endl;

    if (!sm)
      throw Exception ("smoother could not be allocated"); 

    auto prol = lo_fes->GetProlongation();

    mgp = new MultigridPreconditioner (ma, *lo_fes, *lo_bfa, sm, prol);
    mgp->SetOwnProlongation (0);
    mgp->SetSmoothingSteps (int(flags.GetNumFlag ("smoothingsteps", 1)));
    mgp->SetCycle (int(flags.GetNumFlag ("cycle", 1)));
    mgp->SetIncreaseSmoothingSteps (int(flags.GetNumFlag ("increasesmoothingsteps", 1)));
    mgp->SetCoarseSmoothingSteps (int(flags.GetNumFlag ("coarsesmoothingsteps", 1)));
    mgp->SetUpdateAll( flags.GetDefineFlag( "updateall" ) );

    MultigridPreconditioner::COARSETYPE ct = MultigridPreconditioner::EXACT_COARSE;
    const string & coarse = flags.GetStringFlag ("coarsetype", "direct");
    if (coarse == "smoothing")
      ct = MultigridPreconditioner::SMOOTHING_COARSE;
    else if (coarse == "cg")
      ct = MultigridPreconditioner::CG_COARSE;
    mgp->SetCoarseType (ct);
    

    coarse_pre = 
      pde.GetPreconditioner (flags.GetStringFlag ("coarseprecond", ""), 1);

    if (coarse_pre)
      mgp->SetCoarseType (MultigridPreconditioner::USER_COARSE);

    finesmoothingsteps = int (flags.GetNumFlag ("finesmoothingsteps", 1));

    tlp = 0;
    inversetype = flags.GetStringFlag("inverse", GetInverseName (default_inversetype));
  }

 
  MGPreconditioner :: ~MGPreconditioner()
  {
    delete mgp;
    delete tlp;
  }

  void MGPreconditioner :: FreeSmootherMem ()
  {
    if(mgp) mgp->FreeMem();
    if(tlp) tlp->FreeMem();
  }
  

  void MGPreconditioner :: Update ()
  {
    shared_ptr<BilinearForm> lo_bfa = bfa->GetLowOrderBilinearForm();

    INVERSETYPE invtype, loinvtype;
    invtype = dynamic_cast<const BaseSparseMatrix & > (bfa->GetMatrix()).SetInverseType (inversetype);
    if (lo_bfa)
      loinvtype = dynamic_cast<const BaseSparseMatrix & > (lo_bfa->GetMatrix()) .SetInverseType (inversetype);


    mgp->Update();

    if (coarse_pre)
      {
	mgp->SetCoarseGridPreconditioner (&coarse_pre->GetMatrix());
	mgp->SetOwnCoarseGridPreconditioner(false);
      }

    if (bfa->GetLowOrderBilinearForm()) //  || ntasks > 1) not supported anymore
      {
	delete tlp;

	Smoother * fine_smoother = NULL;

	fine_smoother = new BlockSmoother (bfa->GetMeshAccess(), *bfa, flags);

	tlp = new TwoLevelMatrix (&bfa->GetMatrix(),
				  mgp,
				  fine_smoother,
				  bfa->GetMeshAccess().GetNLevels()-1);
	
	tlp -> SetSmoothingSteps (finesmoothingsteps);
	tlp -> Update();
      }
    else
      tlp = 0;

    if (timing) Timing();
    if (test) Test();
    if (mgtest) MgTest();

    dynamic_cast<const BaseSparseMatrix & > (bfa->GetMatrix()).SetInverseType ( invtype );
    if (lo_bfa)
      dynamic_cast<const BaseSparseMatrix & > (lo_bfa->GetMatrix()) .SetInverseType ( loinvtype );
  }
  
  
  void MGPreconditioner :: CleanUpLevel () 
  { 
    if (bfa->GetLowOrderBilinearForm())
      {
	delete tlp;
	tlp = 0;
      }    
  }


  const ngla::BaseMatrix & MGPreconditioner :: GetMatrix() const
  {
    if (tlp)
      return *tlp;
    else
      return *mgp; 
  }

  void MGPreconditioner::PrintReport (ostream & ost)
  {
    ost << "Multigrid preconditioner" << endl
	<< "bilinear-form = " << bfa->GetName() << endl
	<< "smoothertype = " << smoothertype << endl;
  }


  void MGPreconditioner::MemoryUsage (Array<MemoryUsageStruct*> & mu) const
  {
    int olds = mu.Size();

    if (&GetMatrix()) GetMatrix().MemoryUsage (mu);;

    for (int i = olds; i < mu.Size(); i++)
      mu[i]->AddName (string(" mgpre ")); // +GetName());
  }
    



  void MGPreconditioner :: MgTest () const
  {
    cout << "Compute eigenvalues" << endl;
    const BaseMatrix & amat = GetAMatrix();
    const BaseMatrix & pre = GetMatrix();
    
    int eigenretval;
    
    EigenSystem eigen (amat, pre);
    eigen.SetPrecision(1e-30);
    eigen.SetMaxSteps(1000); 
    eigenretval = eigen.Calc();
    eigen.PrintEigenValues (*testout);
    (cout) << " Min Eigenvalue : "  << eigen.EigenValue(mgnumber) << endl; 
    (cout) << " Max Eigenvalue : " << eigen.MaxEigenValue() << endl; 
    (cout) << " Condition   " << eigen.MaxEigenValue()/eigen.EigenValue(mgnumber) << endl; 
    (*testout) << " Min Eigenvalue : "  << eigen.EigenValue(mgnumber) << endl; 
    (*testout) << " Max Eigenvalue : " << eigen.MaxEigenValue() << endl;
    (*testout) << " Condition   " << eigen.MaxEigenValue()/eigen.EigenValue(mgnumber) << endl;
    static ofstream condout (mgfile.c_str());

    // double cond;

    condout << bfa->GetFESpace().GetNDof() << "\t" << bfa->GetFESpace().GetOrder() << "\t" << eigen.EigenValue(mgnumber) << "\t" << eigen.MaxEigenValue() << "\t" 
	    << eigen.MaxEigenValue()/eigen.EigenValue(mgnumber) <<  "\t" << endl;
    
    if(testresult_ok) *testresult_ok = eigenretval;
    if(testresult_min) *testresult_min = eigen.EigenValue(mgnumber);
    if(testresult_max) *testresult_max = eigen.MaxEigenValue();

  }





  
  // ****************************** DirectPreconditioner **************************


  class NGS_DLL_HEADER DirectPreconditioner : public Preconditioner
  {
    shared_ptr<BilinearForm> bfa;
    BaseMatrix * inverse;
    string inversetype;

  public:
    DirectPreconditioner (const PDE & pde, const Flags & aflags,
			  const string aname = "directprecond")
      : Preconditioner(&pde,aflags,aname)
    {
      bfa = pde.GetBilinearForm (flags.GetStringFlag ("bilinearform", NULL));
      inverse = NULL;
      inversetype = flags.GetStringFlag("inverse", GetInverseName (default_inversetype)); // "sparsecholesky");
    }
    
    ///
    virtual ~DirectPreconditioner()
    {
      delete inverse;
    }
    
    
    ///
    virtual void Update ()
    {
      delete inverse;
      
      try
	{
	  bfa->GetMatrix().SetInverseType (inversetype);
	  const BitArray * freedofs = 
	    bfa->GetFESpace().GetFreeDofs (bfa->UsesEliminateInternal());
	  inverse = bfa->GetMatrix().InverseMatrix(freedofs);
	}
      catch (exception &)
	{
	  throw Exception ("DirectPreconditioner: needs a sparse matrix (or has memory problems)");
	}
    }

    virtual void CleanUpLevel ()
    {
      delete inverse;
      inverse = NULL;
    }

    virtual const BaseMatrix & GetMatrix() const
    {
      return *inverse;
    }

    virtual const BaseMatrix & GetAMatrix() const
    {
      return bfa->GetMatrix(); 
    }

    virtual const char * ClassName() const
    {
      return "Direct Preconditioner"; 
    }
  };







  // ****************************** LocalPreconditioner *******************************



  LocalPreconditioner :: LocalPreconditioner (PDE * pde, const Flags & aflags, 
					      const string aname)
    : Preconditioner (pde,aflags,aname), coarse_pre(0)
  {
    bfa = pde->GetBilinearForm (flags.GetStringFlag ("bilinearform", NULL));
    block = flags.GetDefineFlag ("block");
    jacobi = NULL;
    locprectest = flags.GetDefineFlag ("mgtest");
    locprecfile = flags.GetStringFlag ("mgfile","locprectest.out"); 

    string smoother = flags.GetStringFlag("smoother","");
    if ( smoother == "block" )
      block = true;

    // coarse-grid preconditioner only used in parallel!!
    ct = "NO_COARSE";
    const string & coarse = flags.GetStringFlag ("coarsetype", "nocoarse");
    if (coarse == "smoothing") 
      ct = "SMOOTHING_COARSE";
    else if (coarse == "direct")
      ct = "DIRECT_COARSE";
    

    coarse_pre = 
      pde->GetPreconditioner (flags.GetStringFlag ("coarseprecond", ""), 1);
    if (coarse_pre)
      ct = "USER_COARSE";

  }
   

  LocalPreconditioner :: ~LocalPreconditioner()
  {
    delete jacobi;
  }

  void LocalPreconditioner :: Update ()
  {
    cout << IM(1) << "Update Local Preconditioner" << flush;
    delete jacobi;
    
    // const BaseSparseMatrix& amatrix = dynamic_cast<const BaseSparseMatrix&> (bfa->GetMatrix());
    // 	if ( inversetype != "none" )
    // 	amatrix.SetInverseType ( inversetype );
	
    int blocktype = int (flags.GetNumFlag ( "blocktype", -1));

    // if (MyMPI_GetNTasks() != 1) return;
    bool parallel = (this->on_proc == -1);
    /*
    if ( !parallel && id != this->on_proc )
      {
	jacobi = 0; 
	return;
      }
    */

//     BaseBlockJacobiPrecond::COARSE_TYPE bbct;
//     switch ( ct  )
//       {
//       case NO_COARSE:
// 	bbct = NO_COARSE; break;
//       case DIRECT_COARSE:
// 	bbct = DIRECT_COARSE; break;
//       }

    if ( block && blocktype == -1 ) blocktype = 0;
    if ( blocktype >= 0 )
      {
	// new: blocktypes, specified in fespace
	if (bfa->UsesEliminateInternal())
	  flags.SetFlag("eliminate_internal");
	Table<int> * blocks = bfa->GetFESpace().CreateSmoothingBlocks(flags);
	jacobi = dynamic_cast<const BaseSparseMatrix&> (bfa->GetMatrix())
	  .CreateBlockJacobiPrecond(*blocks, 0, coarse_pre.get(), parallel, bfa->GetFESpace().GetFreeDofs());
	// dynamic_cast<BaseBlockJacobiPrecond&> (*jacobi) . InitCoarseType(ct, bfa->GetFESpace().GetFreeDofs());
      }
    else if (block)
      {
	cout << "\nFlag block deprecated: use -blocktype=<typeno> instead" << endl;
      }
    else
      {
	const BaseMatrix * mat = &bfa->GetMatrix();
#ifdef PARALLEL
	if (dynamic_cast<const ParallelMatrix*> (mat))
	  mat = &(dynamic_cast<const ParallelMatrix*> (mat)->GetMatrix());
#endif
	jacobi = dynamic_cast<const BaseSparseMatrix&> (*mat)
	  .CreateJacobiPrecond(bfa->GetFESpace().GetFreeDofs(bfa->UsesEliminateInternal()));
      }


    
    if (test) Test();
    if(locprectest) LocPrecTest(); 
    //    cout << " done" << endl;
  }

  const ngla::BaseMatrix & LocalPreconditioner :: GetMatrix() const
  {
    return *jacobi;
  }

 void LocalPreconditioner :: LocPrecTest () const
  {
    cout << "Compute eigenvalues" << endl;
    const BaseMatrix & amat = GetAMatrix();
    const BaseMatrix & pre = GetMatrix();
    
    int eigenretval;
    
    EigenSystem eigen (amat, pre);
    eigen.SetPrecision(1e-30);
    eigen.SetMaxSteps(1000); 
    eigenretval = eigen.Calc();
    eigen.PrintEigenValues (*testout);
    (cout) << " Min Eigenvalue : "  << eigen.EigenValue(1) << endl; 
    (cout) << " Max Eigenvalue : " << eigen.MaxEigenValue() << endl; 
    (cout) << " Condition   " << eigen.MaxEigenValue()/eigen.EigenValue(1) << endl; 
    (*testout) << " Min Eigenvalue : "  << eigen.EigenValue(1) << endl; 
    (*testout) << " Max Eigenvalue : " << eigen.MaxEigenValue() << endl;
    (*testout) << " Condition   " << eigen.MaxEigenValue()/eigen.EigenValue(1) << endl;
    static ofstream condout (locprecfile.c_str());

    // double cond;

    condout << bfa->GetFESpace().GetNDof() << "\t" << bfa->GetFESpace().GetOrder() << "\t" << eigen.EigenValue(1) << "\t" << eigen.MaxEigenValue() << "\t" 
	    << eigen.MaxEigenValue()/eigen.EigenValue(1) <<  "\t" << endl;
    
    if(testresult_ok) *testresult_ok = eigenretval;
    if(testresult_min) *testresult_min = eigen.EigenValue(1);
    if(testresult_max) *testresult_max = eigen.MaxEigenValue();

  }







  // ****************************** TwoLevelPreconditioner *******************************


  TwoLevelPreconditioner :: TwoLevelPreconditioner (PDE * apde, const Flags & aflags, const string aname)
    : Preconditioner(apde,aflags,aname)
  {
    pde = apde;
    bfa = pde->GetBilinearForm (flags.GetStringFlag ("bilinearform", NULL));
    cpre = pde->GetPreconditioner (flags.GetStringFlag ("coarsepreconditioner", NULL));
    smoothingsteps = int (flags.GetNumFlag ("smoothingsteps", 1));
    premat = NULL;
  }

  TwoLevelPreconditioner :: ~TwoLevelPreconditioner()
  {
    delete premat;
  }

  void TwoLevelPreconditioner :: Update ()
  {
    /*
      delete premat;

      Smoother * smoother = new GSSmoother (pde->GetMeshAccess(), *bfa);
      //  Smoother * smoother = new EBESmoother (pde->GetMeshAccess(), *bfa);

      premat = new TwoLevelMatrix (&bfa->GetMatrix(),
      &cpre->GetMatrix(),
      smoother,
      pde->GetMeshAccess().GetNLevels());

      //			       bfa->GetMatrix().CreateJacobiPrecond());
      premat -> SetSmoothingSteps (smoothingsteps);
      premat -> Update();
    */

    /*
      cout << "2-Level Preconditioner" << endl;
      EigenSystem eigen (bfa->GetMatrix(), *premat);
      eigen.Calc();
      eigen.PrintEigenValues (cout);
    */
  }



ComplexPreconditioner :: ComplexPreconditioner (PDE * apde, const Flags & aflags, const string aname)
    : Preconditioner(apde,aflags,aname)
  {
    dim = int (flags.GetNumFlag ("dim", 1));
    cm = 0;
    creal = apde->GetPreconditioner (flags.GetStringFlag ("realpreconditioner",""));
  }

  ComplexPreconditioner :: ~ComplexPreconditioner()
  { 
    ;
  }

  void ComplexPreconditioner :: Update ()
  {
    delete cm;
    switch (dim)
      {
      case 1:
	cm = new Real2ComplexMatrix<double,Complex> (&creal->GetMatrix());
	break;
      case 2:
	cm = new Real2ComplexMatrix<Vec<2,double>,Vec<2,Complex> > (&creal->GetMatrix());
	break;
      case 3:
	cm = new Real2ComplexMatrix<Vec<3,double>,Vec<3,Complex> > (&creal->GetMatrix());
	break;
      case 4:
	cm = new Real2ComplexMatrix<Vec<4,double>,Vec<4,Complex> > (&creal->GetMatrix());
	break;
      default:
	cout << "Error: dimension " << dim << " for complex preconditioner not supported!" << endl;
      }
  }



  ChebychevPreconditioner :: ChebychevPreconditioner (PDE * apde, const Flags & aflags, const string aname)
    : Preconditioner(apde,aflags,aname)
  {
    steps = int (flags.GetNumFlag ("steps",10.));
    cm = 0;
    csimple = apde->GetPreconditioner (flags.GetStringFlag ("csimple",""));
    bfa = apde->GetBilinearForm (flags.GetStringFlag ("bilinearform",""));
    test = flags.GetDefineFlag ("test");
  }

  ChebychevPreconditioner :: ~ChebychevPreconditioner()
  { 
    ;
  }

  void ChebychevPreconditioner :: Update ()
  {
    delete cm;

    cout << "Compute eigenvalues csimple" << endl;
    const BaseMatrix & amat = bfa->GetMatrix();
    const BaseMatrix & pre = csimple->GetMatrix();

    EigenSystem eigen (amat, pre);
    eigen.SetPrecision(1e-30);
    eigen.SetMaxSteps(1000); 
    eigen.Calc();

    double lmin = eigen.EigenValue(1); 
    double lmax = eigen.MaxEigenValue(); 
    
    (*testout) << " Min Eigenvalue csimple: "  << eigen.EigenValue(1) << endl; 
    (*testout) << " Max Eigenvalue csimple : " << eigen.MaxEigenValue() << endl; 
    (cout) << " Min Eigenvalue csimple: "  << eigen.EigenValue(1) << endl; 
    (cout)<< " Max Eigenvalue csimple: " << eigen.MaxEigenValue() << endl; 
    (*testout) << " Condition csimple  " << eigen.MaxEigenValue()/eigen.EigenValue(1) << endl; 
    (cout) << " Condition csimple" << eigen.MaxEigenValue()/eigen.EigenValue(1) << endl; 
    eigen.PrintEigenValues(cout); 
  
    cm = new ChebyshevIteration(amat, pre, steps); 
    cm->SetBounds(1-lmax,1-lmin); 
    if(test) Test(); 

  }





  ////////////////////////////////////////////////////////////////////////////////
  // added 08/19/2003, FB

  NonsymmetricPreconditioner :: NonsymmetricPreconditioner (PDE * apde, const Flags & aflags, const string aname)
    : Preconditioner(apde,aflags,aname)
  {
    dim = int (flags.GetNumFlag ("dim", 1));
    cm = 0;
    cbase = apde->GetPreconditioner (flags.GetStringFlag ("basepreconditioner",""));
  }

  NonsymmetricPreconditioner :: ~NonsymmetricPreconditioner()
  { 
    ;
  }

  void NonsymmetricPreconditioner :: Update ()
  {
    delete cm;
    switch (dim)
      {
      case 2:
	cm = new Small2BigNonSymMatrix<double, Vec<2,double> > (&cbase->GetMatrix());
	break;
      case 4:
	cm = new Small2BigNonSymMatrix<Vec<2,double>, Vec<4,double> > (&cbase->GetMatrix());
	break;
      case 6:
	cm = new Small2BigNonSymMatrix<Vec<3,double>, Vec<6,double> > (&cbase->GetMatrix());
	break;
       case 8:
 	cm = new Small2BigNonSymMatrix<Vec<4,double>, Vec<8,double> > (&cbase->GetMatrix());
 	break;
//       case 2:
// 	cm = new Sym2NonSymMatrix<Vec<2,double> > (&cbase->GetMatrix());
// 	break;
//       case 4:
// 	cm = new Sym2NonSymMatrix<Vec<4,double> > (&cbase->GetMatrix());
// 	break;
//       case 6:
// 	cm = new Sym2NonSymMatrix<Vec<6,double> > (&cbase->GetMatrix());
// 	break;
//        case 8:
//  	cm = new Sym2NonSymMatrix<Vec<8,double> > (&cbase->GetMatrix());
//  	break;
      default:
	cout << "Error: dimension " << dim;
	cout << " for nonsymmetric preconditioner not supported!" << endl;
      }
  }





  ////////////////////////////////////////////////////////////////////////////////


  CommutingAMGPreconditioner :: CommutingAMGPreconditioner (PDE * apde, const Flags & aflags, const string aname)
    : Preconditioner (apde,aflags,aname), pde(apde)
  {
    bfa = pde->GetBilinearForm (flags.GetStringFlag ("bilinearform", ""));
    while (bfa->GetLowOrderBilinearForm())
      bfa = bfa->GetLowOrderBilinearForm();

    coefse = pde->GetCoefficientFunction (flags.GetStringFlag ("coefse", ""),1);    
    coefe = pde->GetCoefficientFunction (flags.GetStringFlag ("coefe", ""),1);    
    coeff = pde->GetCoefficientFunction (flags.GetStringFlag ("coeff", ""),1);    

    hcurl = dynamic_cast<const NedelecFESpace*> (&bfa->GetFESpace()) != 0;
    levels = int (flags.GetNumFlag ("levels", 10));
    coarsegrid = flags.GetDefineFlag ("coarsegrid");

    amg = NULL;
  }

  CommutingAMGPreconditioner :: ~CommutingAMGPreconditioner ()
  {
    delete amg;
  }

  void CommutingAMGPreconditioner :: Update ()
  {
    cout << "Update amg" << endl;

#ifdef PARALLEL
    if ( this->on_proc != MyMPI_GetId() && this->on_proc != -1)
      {
	amg = NULL;
	return;
      }
#endif

    const MeshAccess & ma = pde->GetMeshAccess();

    int nedge = ma.GetNEdges(); 
    int nface = ma.GetNFaces(); 
    int nel = ma.GetNE();

    if (coarsegrid && ma.GetNLevels() > 1)
      return;
      

    delete amg;

    // cout << "get edges" << endl;

    Array<INT<2> > e2v (nedge);
    e2v = INT<2> (-1, -1);
    for (int i = 0; i < nedge; i++)
      if (ma.GetClusterRepEdge (i) >= 0)
	ma.GetEdgePNums (i, e2v[i][0], e2v[i][1]);

    // cout << "get faces" << endl;

    Array<INT<4> > f2v (nface);
    Array<int> fpnums;
    for (int i = 0; i < nface; i++)
      {
	if (ma.GetClusterRepFace (i) >= 0)
	  {
	    ma.GetFacePNums (i, fpnums);
	    
	    f2v[i][3] = -1;
	    for (int j = 0; j < fpnums.Size(); j++)
	      f2v[i][j] = fpnums[j];
	  }
	else
	  {
	    for (int j = 0; j < 4; j++)
	      f2v[i][j] = -1;
	  }
      }
    //(*testout) << "f2v " << f2v << endl;

    cout << "compute hi" << endl;

    // compute edge and face weights:

    Array<double> weighte(nedge), weightf(nface);
    Array<double> hi;    // edge length
    Array<double> ai;    // area of face


    hi.SetSize(nedge);
    for (int i = 0; i < nedge; i++)
      if (e2v[i][0] != -1)
	{
	  Vec<3> v = 
	    ma.GetPoint<3> (e2v[i][0]) -
	    ma.GetPoint<3> (e2v[i][1]);
	  hi[i] = L2Norm (v);
	}
    
    if (hcurl)
      {
	ai.SetSize(nface);
	for (int i = 0; i < nface; i++)
	  {
	    Vec<3> p1 = ma.GetPoint<3> (f2v[i][0]);
	    Vec<3> p2 = ma.GetPoint<3> (f2v[i][1]);
	    Vec<3> p3 = ma.GetPoint<3> (f2v[i][2]);
	    Vec<3> vn = Cross (Vec<3> (p2-p1), Vec<3> (p3-p1));
	    ai[i] = L2Norm (vn);
	  }
      }

    Array<int> ednums(12), fanums(12);
    LocalHeap lh (10000, "CommutingAMG");
    IntegrationPoint ip(0, 0, 0, 0);

    cout << "compute weights" << endl;

    weighte = 0;
    weightf = 0;
    for (int i = 0; i < nel; i++)
      {
	HeapReset hr(lh);
	ma.GetElEdges (i, ednums);

	ElementTransformation & eltrans = ma.GetTrafo (i, false, lh);
	MappedIntegrationPoint<3,3> sip(ip, eltrans);

	double vol = ma.ElementVolume (i);
	double vale = Evaluate (*coefe, sip);

	for (int j = 0; j < ednums.Size(); j++)
	  weighte[ednums[j]] += vale * vol / sqr (hi[ednums[j]]);

	if (hcurl)
	  {
	    ma.GetElFaces (i, fanums);
	    double valf = Evaluate (*coeff, sip);
	    for (int j = 0; j < fanums.Size(); j++)
	      weightf[fanums[j]] += valf * vol / sqr (ai[fanums[j]]);
	  }
      }

    int nsel = ma.GetNSE();
    if (coefse)
      for (int i = 0; i < nsel; i++)
	{
	  HeapReset hr(lh);
	  ma.GetSElEdges (i, ednums);
	  ElementTransformation & eltrans = ma.GetTrafo (i, true, lh);

	  MappedIntegrationPoint<2,3> sip(ip, eltrans);

	  double vol = ma.SurfaceElementVolume (i);
	  double vale = Evaluate (*coefse, sip);

	  for (int j = 0; j < ednums.Size(); j++)
	    weighte[ednums[j]] += vale * vol / sqr (hi[ednums[j]]);
	}


    Timer timer_coarse("AMG - Coarsening time");

    timer_coarse.Start();
    Array< Vec<3> > vertices;

    int nv = ma.GetNV();
    vertices.SetSize(nv);
    for (int i = 0; i < nv; i++)
      ma.GetPoint(i, vertices[i]);

    CommutingAMG * amgmat;
    if (hcurl)
      amgmat = new AMG_HCurl (bfa->GetMatrix(), vertices, e2v, f2v, weighte, weightf, levels);
    else
      amgmat = new AMG_H1 (bfa->GetMatrix(), e2v, weighte, levels);
    timer_coarse.Stop();

    cout << "AMG coarsening time = " << timer_coarse.GetTime() << " sec" << endl;

    Timer timer_proj("AMG - projection time");
    timer_proj.Start();
    amgmat->ComputeMatrices (dynamic_cast<const BaseSparseMatrix&> (bfa->GetMatrix()));
    timer_proj.Stop();

    cout << "AMG projection time = " << timer_proj.GetTime() << " sec" << endl;
    cout << "Total NZE = " << amgmat->NZE() << endl;

    amg = amgmat;

    /*
    ChebyshevIteration * cheby =
      new ChebyshevIteration (bfa->GetMatrix(), *amgmat, 10);
    cheby -> SetBounds (0, 0.98);
    amg = cheby;
    */

    cout << "matrices done" << endl;

    if (timing) Timing();
    if (test) Test();
  }


  void CommutingAMGPreconditioner :: CleanUpLevel () 
  { 
    if (coarsegrid)
      return;

    delete amg;
    amg = 0;
  }







  PreconditionerClasses::PreconditionerInfo::
  PreconditionerInfo (const string & aname,
		      Preconditioner* (*acreator)(const PDE & pde, const Flags & flags, const string & name))
    : name(aname), creator(acreator) 
  {
    ;
  }
  
  PreconditionerClasses :: PreconditionerClasses ()
  {
    ;
  }

  PreconditionerClasses :: ~PreconditionerClasses()
  {
    for(int i=0; i<prea.Size(); i++)
      delete prea[i];
  }
  
  void PreconditionerClasses :: 
  AddPreconditioner (const string & aname,
		     Preconditioner* (*acreator)(const PDE & pde, 
						 const Flags & flags,
						 const string & name))
  {
    prea.Append (new PreconditionerInfo(aname, acreator));
  }

  const PreconditionerClasses::PreconditionerInfo * 
  PreconditionerClasses::GetPreconditioner(const string & name)
  {
    for (int i = 0; i < prea.Size(); i++)
      {
	if (name == prea[i]->name)
	  return prea[i];
      }
    return 0;
  }

  void PreconditionerClasses :: Print (ostream & ost) const
  {
    ost << endl << "Preconditioners:" << endl;
    ost <<         "---------" << endl;
    ost << setw(20) << "Name" << endl;
    for (int i = 0; i < prea.Size(); i++)
      ost << setw(20) << prea[i]->name << endl;
  }

 
  PreconditionerClasses & GetPreconditionerClasses ()
  {
    static PreconditionerClasses fecl;
    return fecl;
  }

  RegisterPreconditioner<MGPreconditioner> registerMG("multigrid");
  RegisterPreconditioner<DirectPreconditioner> registerDirect("direct");

}




