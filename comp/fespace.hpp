#ifndef FILE_FESPACE
#define FILE_FESPACE

/*********************************************************************/
/* File:   fespace.hpp                                               */
/* Author: Joachim Schoeberl                                         */
/* Date:   25. Mar. 2000                                             */
/*********************************************************************/


namespace ngcomp
{

  /*
    Finite Element Space
  */


  /**
    transformation from local to global orientation
    used for low order Nedelec elements
  */
  enum TRANSFORM_TYPE { TRANSFORM_MAT_LEFT = 1,
			TRANSFORM_MAT_RIGHT = 2,
			TRANSFORM_MAT_LEFT_RIGHT = 3,
			TRANSFORM_RHS = 4,
			TRANSFORM_SOL = 8 };
  /**
    coupling types: Each degree of freedom is either
     - a local degree of freedom 
     - an interface degree of freedom 
     or
     - a wirebasket degree of freedom
  */
  enum COUPLING_TYPE {  UNUSED_DOF = 0,
			LOCAL_DOF = 1,
			INTERFACE_DOF = 2,
			NONWIREBASKET_DOF = 3,
			WIREBASKET_DOF = 4,
			EXTERNAL_DOF = 6,
			ANY_DOF = 7
		      };
		     



  /**
     Base class for finite element space.
     Provides finite elements, global degrees of freedom, 
     and transformations of element-matrices and element-vectors
  */
  class NGS_DLL_HEADER FESpace : public NGS_Object
  {
  protected:
    /// order of finite elements
    int order;
    /// how many components
    int dimension;
    /// complex space
    bool iscomplex;

    /// couple (all) neighbouring degrees of freedom (like for jump terms of dg-methods)?
    bool dgjumps;

    /// debug output to testout
    bool print; 

    /// prolongation operators between multigrid levels
    ngmg::Prolongation * prol;// = NULL;
    /// highest multigrid-level for which Update was called (memory allocation)
    int level_updated;

    /// on which subdomains is the space defined ?
    Array<int> definedon;
    /// on which boundaries is the space defined ?
    Array<int> definedonbound;

    /// prototype: what are the Dirichlet boundaries ?
    BitArray dirichlet_boundaries;

    /// dofs on Dirichlet boundary
    BitArray dirichlet_dofs;
    BitArray free_dofs;
    BitArray external_free_dofs;


    Array<bool> dirichlet_vertex;
    Array<bool> dirichlet_edge;
    Array<bool> dirichlet_face;

  
    /// Reference - element (low order only)
    FiniteElement * tet;  // = NULL;
    /// Reference - element (low order only)
    FiniteElement * prism; // = NULL;
    /// Reference - element (low order only) 
    FiniteElement * pyramid;  // = NULL;
    /// Reference - element (low order only)
    FiniteElement * hex; //  = NULL;
    /// Reference - element (low order only)
    FiniteElement * trig; // = NULL;
    /// Reference - element (low order only)
    FiniteElement * quad;// = NULL;
    /// Reference - element (low order only)
    FiniteElement * segm;// = NULL;
    /// Reference - element (low order only)
    FiniteElement * point;// = NULL;


    FiniteElement * dummy_tet; //  = new DummyFE<ET_TET>();
    FiniteElement * dummy_pyramid; // = new DummyFE<ET_PYRAMID>();
    FiniteElement * dummy_prism; // = new DummyFE<ET_PRISM>();
    FiniteElement * dummy_hex; //  = new DummyFE<ET_HEX>();
    FiniteElement * dummy_trig; // = new DummyFE<ET_TRIG>();
    FiniteElement * dummy_quad; // = new DummyFE<ET_QUAD>();
    FiniteElement * dummy_segm; // = new DummyFE<ET_SEGM>();
    FiniteElement * dummy_point; // = new DummyFE<ET_POINT>();

    /// Evaluator for visualization (new style)
    DifferentialOperator * evaluator; // = NULL; 
    /// Evaluator for visualization of boundary data
    DifferentialOperator * boundary_evaluator; // = NULL;
    /// Evaluator for flux
    DifferentialOperator * flux_evaluator; // = NULL;

    /// Evaluator for visualization (old style)
    BilinearFormIntegrator * integrator; // = NULL;
    /// Evaluator for visualization of boundary data
    BilinearFormIntegrator * boundary_integrator; // = NULL;



    /// if non-zero, pointer to low order space
    FESpace * low_order_space; // = NULL;

    /// if directsolverclustered[i] is true, then the unknowns of domain i are clustered
    Array<bool> directsolverclustered;

    Array<string> directsolvermaterials;

    mutable Array<int> adddirectsolverdofs;

    Array<int> directvertexclusters;
    Array<int> directedgeclusters;
    Array<int> directfaceclusters;
    Array<int> directelementclusters;

    Table<int> * element_coloring; // = NULL;
    Table<int> * selement_coloring; // = NULL;
    Array<COUPLING_TYPE> ctofdof;

    ParallelDofs * paralleldofs; // = NULL;
  
  public:
    Vec<4,int> vefc_dofblocks;  // = 2;

    /**
       Constructor.
       Used flags are: \\
       -order=<int>:  finite element order \\
       -dim=<int>:    number of components \\
       -complex:      complex space \\
       -dirichlet=<int-list>: dirichlet boundaries, 1-based \\
    */
    FESpace (const MeshAccess & ama, const Flags & flags, 
             bool checkflags = false);
    ///
    virtual ~FESpace ();
  
    /// update dof-tables, old style
    // virtual void Update();

    /// update dof-table
    virtual void Update(LocalHeap & lh);

    /// update element coloring
    virtual void FinalizeUpdate(LocalHeap & lh);

    /// highest level where update/finalize was called
    int GetLevelUpdated() const { return level_updated; }

    const Table<int> & ElementColoring(VorB vb = VOL) const 
    { return (vb == VOL) ? *element_coloring : *selement_coloring; }

    /// print report to stream
    virtual void PrintReport (ostream & ost);

    /// order of finite elements
    int GetOrder () const { return order; }

    /// how many components
    int GetDimension () const { return dimension; }

    /// complex space ?
    bool IsComplex () const { return iscomplex; }


    /// number of dofs of process
    virtual int GetNDof () const = 0;
    /// number of dofs on the level
    virtual int GetNDofLevel (int level) const;
  
    /// returns finite element. 
    const FiniteElement & GetFE (int elnr, bool boundary, LocalHeap & lh) const
    {
      return boundary ? GetSFE(elnr, lh) : GetFE(elnr, lh);
    }

    /// returns finite element. 
    const FiniteElement & GetFE (ElementId ei, LocalHeap & lh) const
    {
      return ei.IsBoundary() ? GetSFE(ei.Nr(), lh) : GetFE(ei.Nr(), lh);
    }

    /// returns finite element. 
    virtual const FiniteElement & GetFE (int elnr, LocalHeap & lh) const;

    /// get dof-nrs of the element
    virtual void GetDofNrs (int elnr, Array<int> & dnums) const = 0;

    /// get dof-nrs of domain or boundary element elnr
    void GetDofNrs (int elnr, bool boundary, Array<int> & dnums) const
    {
      if (boundary)
	GetSDofNrs (elnr, dnums);
      else
	GetDofNrs (elnr, dnums);
    }

    /// get dof-nrs of domain or boundary element elnr
    void GetDofNrs (ElementId ei, Array<int> & dnums) const
    {
      if (ei.IsBoundary())
	GetSDofNrs (ei.Nr(), dnums);
      else
	GetDofNrs (ei.Nr(), dnums);
    }

    virtual void GetDofRanges (ElementId ei, Array<IntRange> & dranges) const;

    FlatArray<int> GetDofNrs (ElementId ei, LocalHeap & lh) const;
    
    /// get coupling types of dofs
    virtual void GetDofCouplingTypes (int elnr, Array<COUPLING_TYPE> & dnums) const;
    
    /// get coupling types of dof
    virtual COUPLING_TYPE GetDofCouplingType (int dof) const; 
    
    /// get dof-nrs of the element of certain coupling type
    void GetDofNrs (int elnr, Array<int> & dnums, COUPLING_TYPE ctype) const;


    /// get dofs on nr'th node of type nt.
    virtual void GetNodeDofNrs (NODE_TYPE nt, int nr, Array<int> & dnums) const;
    /// get number of low-order dofs for node of type nt
    // virtual int GetNLowOrderNodeDofs ( NODE_TYPE nt ) const;
    // { return lodofs_per_node[nt]; }

    /// get dofs on vertex vnr
    virtual void GetVertexDofNrs (int vnr, Array<int> & dnums) const;
    /// get dofs on edge enr
    virtual void GetEdgeDofNrs (int ednr, Array<int> & dnums) const;
    /// get dofs on face fnr
    virtual void GetFaceDofNrs (int fanr, Array<int> & dnums) const;
    /// get dofs on element (=cell) elnr
    virtual void GetInnerDofNrs (int elnr, Array<int> & dnums) const;

    virtual bool UsesDGCoupling () const {return dgjumps;};

    /// returns surface element for boundary interals
    virtual const FiniteElement & GetSFE (int selnr, LocalHeap & lh) const;
    /// returns dofs of sourface element
    virtual void GetSDofNrs (int selnr, Array<int> & dnums) const = 0;


    /// is the FESpace defined for this sub-domain nr ?
    bool DefinedOn (int domnr) const
    { return !definedon.Size() || definedon[domnr]; }
    /// is the FESpace defined for this boundary nr ?
    bool DefinedOnBoundary (int bnr) const
    {return !definedonbound.Size() || definedonbound[bnr]; }

    /// is the FESpace defined for this sub-domain / boundary nr ?
    bool DefinedOn (int index, bool bound) const
    {
      if (bound)
        return DefinedOnBoundary (index);
      else
        return DefinedOn (index);
    }

    bool DefinedOn (ElementId id) const
    {
      /*
      int idx = ma.GetElIndex (id);
      if (id.IsBoundary())
        return DefinedOnBoundary (idx);
      else
        return DefinedOn (idx);
      */
      if (id.IsBoundary())
        {
          if (!definedonbound.Size()) return true;
          return DefinedOnBoundary (ma.GetElIndex(id));
        }
      else
        {
          if (!definedon.Size()) return true;
          return DefinedOn (ma.GetElIndex(id));
        }
    }

    ///
    void SetDefinedOn (const BitArray & defon);
    ///
    void SetDefinedOnBoundary (const BitArray & defon);

    ///
    void SetDirichletBoundaries (const BitArray & dirbnds);
    /// Get reference element for tet, prism, trig, etc ..
    const FiniteElement & GetFE (ELEMENT_TYPE type) const;

    /// according low-order FESpace (if available)
    FESpace & LowOrderFESpace () { return *low_order_space; }
    /// according low-order FESpace (if available)
    const FESpace & LowOrderFESpace () const { return *low_order_space; }
    ///
    // void SetLowOrderSpace (bool los) { is_low_order_space = los; }
    ///
    // bool IsLowOrderSpace () const { return is_low_order_space; }

    /// non Dirichlet dofs
    virtual const BitArray * GetFreeDofs (bool external = false) const;
    ///
    bool IsDirichletDof (int i) const
    { return dirichlet_dofs.Size() && dirichlet_dofs[i]; }

    bool IsDirichletBoundary (int i) const
    { return dirichlet_boundaries.Size() && dirichlet_boundaries[i]; }

    /// is vertex on Dirichlet boundary ?
    bool IsDirichletVertex (int i) const { return dirichlet_vertex.Size() && dirichlet_vertex[i]; }
    /// is edge on Dirichlet boundary ?
    bool IsDirichletEdge (int i) const { return dirichlet_edge.Size() && dirichlet_edge[i]; }
    /// is face on Dirichlet boundary ?
    bool IsDirichletFace (int i) const { return dirichlet_face.Size() && dirichlet_face[i]; }

    void GetFilteredDofs(COUPLING_TYPE doffilter, BitArray & output, bool freedofsonly=true) const;
    /// 
    virtual Table<int> * CreateSmoothingBlocks (const Flags & flags) const;
    /// for anisotropic plane smoothing:
    virtual Array<int> * CreateDirectSolverClusters (const Flags & flags) const
    { return 0; }

    virtual void AddDirectSolverClusterDof(int dn) const
    { adddirectsolverdofs.Append(dn); }

    virtual Array<int> & DirectVertexClusters(void)
    { return directvertexclusters; }
    virtual Array<int> & DirectEdgeClusters(void)
    { return directedgeclusters; }
    virtual Array<int> & DirectFaceClusters(void)
    { return directfaceclusters; }
    virtual Array<int> & DirectElementClusters(void)
    { return directelementclusters; }


    void TransformMat (int elnr, bool boundary,
		       const SliceMatrix<double> & mat, TRANSFORM_TYPE type) const
    {
      VTransformMR (elnr, boundary, mat, type);
    }
  
    void TransformMat (int elnr, bool boundary,
		       const SliceMatrix<Complex> & mat, TRANSFORM_TYPE type) const
    {
      VTransformMC (elnr, boundary, mat, type);
    }
  


    void TransformVec (int elnr, bool boundary,
		       const FlatVector<double> & vec, TRANSFORM_TYPE type) const
    {
      VTransformVR (elnr, boundary, vec, type);
    }
  
    void TransformVec (int elnr, bool boundary,
		       const FlatVector<Complex> & vec, TRANSFORM_TYPE type) const
    {
      VTransformVC (elnr, boundary, vec, type);
    }


    template < int S, class T >
    void TransformVec (int elnr, bool boundary,
		       const FlatVector< Vec<S,T> >& vec, TRANSFORM_TYPE type) const;


    template < class T >
    void TransformVec (ElementId ei,
		       const T & vec, TRANSFORM_TYPE type) const
    {
      TransformVec (ei.Nr(), ei.IsBoundary(), vec, type);
    }
  

    virtual void VTransformMR (int elnr, bool boundary,
			       const SliceMatrix<double> & mat, TRANSFORM_TYPE type) const
    { ; }
    virtual void VTransformMC (int elnr, bool boundary,
			       const SliceMatrix<Complex> & mat, TRANSFORM_TYPE type) const
    { ; }


    virtual void VTransformVR (int elnr, bool boundary,
			       const FlatVector<double> & vec, TRANSFORM_TYPE type) const
    { ; }
    virtual void VTransformVC (int elnr, bool boundary,
			       const FlatVector<Complex> & vec, TRANSFORM_TYPE type) const
    { ; }
  
  
    /// Returns multigrid-prolongation
    virtual const ngmg::Prolongation * GetProlongation () const
    { return prol; }
    /// Set multigrid prolongation
    // void SetProlongation (ngmg::Prolongation * aprol)
    // { prol = aprol; }


    /// returns function-evaluator
    const DifferentialOperator * GetEvaluator (bool vb = VOL) const
    { 
      if (vb == BND)
	return boundary_evaluator; 
      else
	return evaluator; 
    }

    const DifferentialOperator * GetFluxEvaluator () const
    {
      return flux_evaluator;
    }

    /// returns function-evaluator
    const BilinearFormIntegrator * GetIntegrator (bool vb = VOL) const
    { 
      if (vb == BND)
	return boundary_integrator; 
      else
	return integrator; 
    }


    /// returns function-evaluator for boundary values
    const DifferentialOperator * GetBoundaryEvaluator () const
    { return boundary_evaluator; }
    const BilinearFormIntegrator * GetBoundaryIntegrator () const
    { return boundary_integrator; }


    /// special elements for hacks (used for contact, periodic-boundary-penalty-constraints, ...
    Array<SpecialElement*> specialelements;

    void AppendSpecialElement (SpecialElement * spel)
    { specialelements.Append (spel); }

    const Array<SpecialElement*> & GetSpecialElements() const {return specialelements;}


    ParallelDofs & GetParallelDofs () const { return *paralleldofs; }
    virtual void UpdateParallelDofs ();

    //// is FESpace mpi-distributed ?
    bool IsParallel() const;

    /// ndof over all mpi-partitions
    int GetNDofGlobal() const;

    virtual int GetRelOrder() const
    { 
      cout << "virtual GetRelOrder called for FiniteElementSpace, not available ! " << endl; 
      return 0; 
    } 

    virtual bool VarOrder() const { return 0; }

    bool timing;
    void Timing () const;

  protected:
    template <template <ELEMENT_TYPE ET> class FE>
    void SetDummyFE ()
    {
      delete dummy_tet;
      delete dummy_pyramid;
      delete dummy_prism;
      delete dummy_hex;
      delete dummy_trig;
      delete dummy_quad;
      delete dummy_segm;
      delete dummy_point;
      dummy_tet = new FE<ET_TET>();
      dummy_pyramid = new FE<ET_PYRAMID>();
      dummy_prism = new FE<ET_PRISM>();
      dummy_hex = new FE<ET_HEX>();
      dummy_trig = new FE<ET_TRIG>();
      dummy_quad = new FE<ET_QUAD>();
      dummy_segm = new FE<ET_SEGM>();
      dummy_point = new FE<ET_POINT>();
    }
  };



  template <typename T>
  inline void IterateElements (const FESpace & fes, 
                               VorB vb, 
                               LocalHeap & clh, 
                               const T & func)
  {
    const Table<int> & element_coloring = fes.ElementColoring(vb);
    
#pragma omp parallel 
    {
      LocalHeap lh = clh.Split();

      // lh.ClearValues();
      for (int col = 0; col < element_coloring.Size(); col++)
        {
          FlatArray<int> elscol = element_coloring[col];

#pragma omp for schedule(dynamic)
          for (int i = 0; i < elscol.Size(); i++)
            {
              HeapReset hr(lh);
              func (ElementId(vb, elscol[i]), lh);
            }
        }
      // cout << "lh, used size = " << lh.UsedSize() << endl;
    }
  }



  /**
     A space of continuous finite elements.
     Supports first and second order finite elements.
  */
  class NGS_DLL_HEADER NodalFESpace : public FESpace
  {
    ///
    Array<int> ndlevel;

  public:

    ///
    NodalFESpace (const MeshAccess & ama, const Flags & flags, bool parseflags=false);
    ///
    virtual ~NodalFESpace ();

    ///
    virtual string GetClassName () const
    {
      return "NodalFESpace";
    }

    ///
    virtual void Update (LocalHeap & lh);
    ///
    virtual int GetNDof () const;
    ///
    virtual int GetNDofLevel (int level) const;
    ///
    virtual void GetDofNrs (int elnr, Array<int> & dnums) const;
    ///
    virtual void GetSDofNrs (int selnr, Array<int> & dnums) const;

    virtual void GetDofRanges (ElementId ei, Array<IntRange> & dranges) const;
  
    virtual void GetVertexDofNrs (int vnr, Array<int> & dnums) const;
    virtual void GetEdgeDofNrs (int ednr, Array<int> & dnums) const;
    virtual void GetFaceDofNrs (int fanr, Array<int> & dnums) const;
    virtual void GetInnerDofNrs (int elnr, Array<int> & dnums) const;

    virtual Array<int> * CreateDirectSolverClusters (const Flags & flags) const;
  };






  ///
  class NGS_DLL_HEADER NonconformingFESpace : public FESpace
  {
    ///
    Array<int> ndlevel;

  public:
    NonconformingFESpace (const MeshAccess & ama, const Flags & flags, bool parseflags=false);
    virtual ~NonconformingFESpace ();

    virtual string GetClassName () const
    { return "Nonconforming FESpace"; }

    ///
    virtual void Update(LocalHeap & lh);
    ///
    virtual int GetNDof () const;
    ///
    virtual void GetDofNrs (int elnr, Array<int> & dnums) const;
    ///
    virtual void GetSDofNrs (int selnr, Array<int> & dnums) const;
  };







  ///
  class NGS_DLL_HEADER ElementFESpace : public FESpace
  {
    ///  Array<int> startelement;
    Array<int> ndlevel;
    int n_el_dofs;
  public:
    ///
    ElementFESpace (const MeshAccess & ama, const Flags& flags, bool parseflags=false);

    ///
    ~ElementFESpace ();

    virtual string GetClassName () const
    {
      return "ElementFESpace";
    }

    ///
    virtual void Update(LocalHeap & lh);

    ///
    virtual int GetNDof () const { return ndlevel.Last(); }
  
    ///
    virtual void GetDofNrs (int elnr, Array<int> & dnums) const;

    ///
    virtual int GetNDofLevel (int level) const;

    ///
    virtual void GetSDofNrs (int selnr, Array<int> & dnums) const;


    virtual void GetVertexDofNrs (int vnr, Array<int> & dnums) const 
    { dnums.SetSize (0); }
    virtual void GetEdgeDofNrs (int ednr, Array<int> & dnums) const
    { dnums.SetSize (0); }
    virtual void GetFaceDofNrs (int fanr, Array<int> & dnums) const
    { dnums.SetSize (0); }
    virtual void GetInnerDofNrs (int elnr, Array<int> & dnums) const
    { GetDofNrs (elnr, dnums); }
  };





  /// Non-continous fe space on boundary
  class NGS_DLL_HEADER SurfaceElementFESpace : public FESpace
  {
    ///
    Array<int> ndlevel;
    int n_el_dofs;
  public:
    ///
    SurfaceElementFESpace (const MeshAccess & ama, const Flags& flags, 
                           bool checkflags = false);

    ///
    ~SurfaceElementFESpace ();

    ///
    virtual string GetClassName() const
    { return "SurfaceElement"; }

    ///
    virtual void Update(LocalHeap & lh);

    ///
    virtual int GetNDof () const { return ndlevel.Last(); } 

    ///
    virtual const FiniteElement & GetFE (int elnr, LocalHeap & lh) const;

    ///
    virtual void GetDofNrs (int elnr, Array<int> & dnums) const;

    ///
    virtual int GetNDofLevel (int level) const;

    ///
    virtual void GetSDofNrs (int selnr, Array<int> & dnums) const;
  };







  /// A combination of fe-spaces
  class NGS_DLL_HEADER CompoundFESpace : public FESpace
  {
  protected:
    /// pointers to components
    Array<FESpace*> spaces;
    /// cummlated number of dofs of components
    Array<int> cummulative_nd;
    /// dofs on each multigrid level
    Array<int> ndlevel;
  public:
    /// generates a compound space.
    /// components will be added later
    CompoundFESpace (const MeshAccess & ama,
		     const Flags & flags, bool parseflags = false);
    /// generates a compound space 
    /// components are provided in aspaces
    CompoundFESpace (const MeshAccess & ama,
		     const Array<FESpace*> & aspaces,
		     const Flags & flags, bool parseflags = false);

    /// not much to do.
    /// components will not be deleted
    virtual ~CompoundFESpace ();

    /// add an additional component space
    void AddSpace (FESpace * fes);

    ///
    virtual string GetClassName () const
    {
      return "CompoundFESpace";
    }

    /// updates also components
    virtual void Update(LocalHeap & lh);
    /// updates also components
    virtual void FinalizeUpdate(LocalHeap & lh);

    /// copies dofcoupling from components
    virtual void UpdateCouplingDofArray();

    /// 
    virtual int GetNDof () const { return ndlevel.Last(); }
    ///
    virtual int GetNDofLevel (int level) const { return ndlevel[level]; }

    IntRange GetRange (int spacenr) const
    { 
      return IntRange(cummulative_nd[spacenr], cummulative_nd[spacenr+1]);
    }

    /// get component space
    const FESpace * operator[] (int i) const { return spaces[i]; }
    /// get component space
    FESpace * operator[] (int i) { return const_cast<FESpace*> (spaces[i]); }

    /// returns a compound finite element
    virtual const FiniteElement & GetFE (int elnr, LocalHeap & lh) const;
    ///
    virtual const FiniteElement & GetSFE (int selnr, LocalHeap & lh) const;
    ///
    virtual void GetDofNrs (int elnr, Array<int> & dnums) const;
    ///
    virtual void GetSDofNrs (int selnr, Array<int> & dnums) const;
    ///
    virtual void GetVertexDofNrs (int vnr, Array<int> & dnums) const;
    virtual void GetEdgeDofNrs (int ednr, Array<int> & dnums) const;
    virtual void GetFaceDofNrs (int fanr, Array<int> & dnums) const;
    virtual void GetInnerDofNrs (int elnr, Array<int> & dnums) const;

    virtual void GetDofRanges (ElementId ei, Array<IntRange> & dranges) const;

    template <class MAT> NGS_DLL_HEADER
    void TransformMat (int elnr, bool boundary,
		       MAT & mat, TRANSFORM_TYPE tt) const;

    template <class VEC> NGS_DLL_HEADER
    void TransformVec (int elnr, bool boundary,
		       VEC & vec, TRANSFORM_TYPE tt) const;

    virtual void VTransformMR (int elnr, bool boundary,
			       const SliceMatrix<double> & mat, TRANSFORM_TYPE tt) const;
    virtual void VTransformMC (int elnr, bool boundary,
			       const SliceMatrix<Complex> & mat, TRANSFORM_TYPE tt) const;
    virtual void VTransformVR (int elnr, bool boundary,
			       const FlatVector<double> & vec, TRANSFORM_TYPE tt) const;
    virtual void VTransformVC (int elnr, bool boundary,
			       const FlatVector<Complex> & vec, TRANSFORM_TYPE tt) const;

    /// number of component spaces
    inline int GetNSpaces () const { return spaces.Size(); } 
  };





  /// Registered FESpace classes
  class NGS_DLL_HEADER FESpaceClasses
  {
  public:
    /// descriptor for register fespaces. 
    /// function pointer to create function.
    struct FESpaceInfo
    {
      /// the name
      string name;
      /// function pointer to creator function
      FESpace* (*creator)(const MeshAccess & ma, const Flags & flags);
      /// creates a descriptor
      FESpaceInfo (const string & aname,
		   FESpace* (*acreator)(const MeshAccess & ma, const Flags & flags))
	: name(aname), creator(acreator) {;}
    };
  private:
    Array<FESpaceInfo*> fesa;

  public:
    /// initialize 
    FESpaceClasses() { ; }
    /// cleans up
    ~FESpaceClasses();  

    /// add a descriptor
    void AddFESpace (const string & aname, 
		     FESpace* (*acreator)(const MeshAccess & ma, const Flags & flags));
  
    /// returns all creators
    const Array<FESpaceInfo*> & GetFESpaces() { return fesa; }

    /// returns a creator structure
    const FESpaceInfo * GetFESpace(const string & name);

    /// print available fespaces to stream
    void Print (ostream & ost) const;
  };
 
  /// returns createion object
  extern NGS_DLL_HEADER FESpaceClasses & GetFESpaceClasses ();

  /// creates a fespace of that type
  extern NGS_DLL_HEADER FESpace * CreateFESpace (const string & type,
						 const MeshAccess & ma,
						 const Flags & flags);


  /**
     template for registration of finite element spaces.
     provides static Create - function
   */
  template <typename FES>
  class RegisterFESpace
  {
  public:
    /// constructor registers fespace
    RegisterFESpace (string label)
    {
      GetFESpaceClasses().AddFESpace (label, Create);
      // cout << "register fespace '" << label << "'" << endl;
    }
    
    /// creates an fespace of type FES
    static FESpace * Create (const MeshAccess & ma, const Flags & flags)
    {
      return new FES (ma, flags);
    }
  };















#ifdef PARALLEL

  class ParallelMeshDofs : public ParallelDofs
  {
    const MeshAccess & ma;
    Array<Node> dofnodes;
  public:
    ParallelMeshDofs (const MeshAccess & ama, const Array<Node> & adofnodes, 
		      int dim = 1, bool iscomplex = false);

    const MeshAccess & GetMeshAccess() const { return ma; }
    const Array<Node> & GetDofNodes() const { return dofnodes; }
  };
  
#else


  class ParallelMeshDofs : public ParallelDofs 
  {
  public:
    ParallelMeshDofs (const MeshAccess & ama, const Array<Node> & adofnodes, 
		      int dim = 1, bool iscomplex = false)
    { ndof = adofnodes.Size(); }
  };

#endif

}





#ifdef PARALLEL
namespace ngstd
{
  template<>
  class MPI_Traits<ngcomp::COUPLING_TYPE>
  {
  public:
    /// returns MPI-type 
    static MPI_Datatype MPIType () 
    { 
      if (sizeof(ngcomp::COUPLING_TYPE) == sizeof(int)) return MPI_INT;
      cout << "please provide MPI_Datatype for COUPLING_TYPE" << endl;
      exit(1);
    }
  };
}
#endif


#endif
