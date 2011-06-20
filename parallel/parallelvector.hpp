#ifndef FILE_NGS_PARALLELVECTOR
#define FILE_NGS_PARALLELVECTOR

/* ************************************************************************/
/* File:   parallelvector.hpp                                             */
/* Author: Astrid Sinwel, Joachim Schoeberl                               */
/* Date:   2007,2011                                                      */
/* ************************************************************************/

namespace ngla
{


#ifdef PARALLEL

  // enum PARALLEL_STATUS { DISTRIBUTED, CUMULATED, NOT_PARALLEL };

  using ngparallel::ParallelDofs;

  class NGS_DLL_HEADER ParallelBaseVector : virtual public BaseVector
  {
  protected:
    ParallelDofs * paralleldofs;
    
    mutable PARALLEL_STATUS status;
    
  public:
    ParallelBaseVector ()
      : paralleldofs(NULL) 
    { ; }


    template <typename T> 
    BaseVector & operator= (const VVecExpr<T> & v)
    {
      v.AssignTo (1.0, *this);
      return *this;
    }

    virtual PARALLEL_STATUS Status () const
    { 
      return status; 
    }

    virtual void SetStatus ( PARALLEL_STATUS astatus ) const
    {
      status = astatus;
    }

    virtual PARALLEL_STATUS GetParallelStatus () const { return Status(); }
    virtual void SetParallelStatus (PARALLEL_STATUS stat) const { SetStatus (stat); }
    virtual void Cumulate () const; 


    virtual ParallelDofs * GetParallelDofs () const
    {
      return paralleldofs; 
    }

    virtual bool IsParallelVector () const
    {
      return ( this->Status() != NOT_PARALLEL );
    }
    
    virtual BaseVector & SetScalar (double scal);
    virtual BaseVector & SetScalar (Complex scal);
    
    virtual BaseVector & Set (double scal, const BaseVector & v);
    virtual BaseVector & Set (Complex scal, const BaseVector & v);

    virtual BaseVector & Add (double scal, const BaseVector & v);
    virtual BaseVector & Add (Complex scal, const BaseVector & v);



    virtual void PrintStatus ( ostream & ost ) const
    { cerr << "ERROR -- PrintStatus called for BaseVector, is not parallel" << endl; }
    
    // virtual void AllReduce ( Array<int> * reduceprocs, Array<int> * sendtoprocs=0 ) const;
    
    virtual void Distribute() const
    { cerr << "ERROR -- Distribute called for BaseVector, is not parallel" << endl; }
    
    virtual void ISend ( int dest, MPI_Request & request ) const;
    virtual void Send ( int dest ) const;
    
    virtual void IRecvVec ( int dest, MPI_Request & request )
    { cerr << "ERROR -- IRecvVec called for BaseVector, is not parallel" << endl; }

    virtual void RecvVec ( int dest )
    { cerr << "ERROR -- IRecvVec called for BaseVector, is not parallel" << endl; }
    
    virtual void AddRecvValues( int sender )
    { cerr << "ERROR -- AddRecvValues called for BaseVector, is not parallel" << endl; }

    virtual void SetParallelDofs ( ngparallel::ParallelDofs * aparalleldofs, const Array<int> * procs =0 )
    { 
      if ( aparalleldofs == 0 ) return;
      cerr << "ERROR -- SetParallelDofs called for BaseVector, is not parallel" << endl; 
    }
  };

  
  template <class SCAL>
  class NGS_DLL_HEADER S_ParallelBaseVector 
    : virtual public S_BaseVector<SCAL>, 
      virtual public ParallelBaseVector
  {
  protected:
    virtual SCAL InnerProduct (const BaseVector & v2) const;
  };



  template <typename T = double>
  class ParallelVVector : public VVector<T>, 
			  public S_ParallelBaseVector<typename mat_traits<T>::TSCAL>
  {
    using ParallelBaseVector :: status;
    using ParallelBaseVector :: paralleldofs;
    Table<T> * recvvalues;

  public:
    typedef typename mat_traits<T>::TSCAL TSCAL;

    explicit ParallelVVector (int as);
    explicit ParallelVVector (int as, ngparallel::ParallelDofs * aparalleldofs ) ;
    explicit ParallelVVector (int as, ngparallel::ParallelDofs * aparalleldofs,
			      PARALLEL_STATUS astatus );

    virtual ~ParallelVVector() throw();
 

    virtual void SetParallelDofs ( ngparallel::ParallelDofs * aparalleldofs, const Array<int> * procs=0 );


    virtual void PrintParallelDofs() const;


    /// values from reduceprocs are added up,
    /// vectors in sendtoprocs are set to the cumulated values
    /// default pointer 0 means send to proc 0
    // virtual void AllReduce ( Array<int> * reduceprocs, Array<int> * sendtoprocs = 0 ) const;

    virtual void Distribute() const;

    virtual void PrintStatus ( ostream & ost ) const
    {
      if ( this->status == NOT_PARALLEL )
	ost << "NOT PARALLEL" << endl;
      else if ( this->status == DISTRIBUTED )
	ost << "DISTRIBUTED" << endl ;
      else if (this->status == CUMULATED )
	ost << "CUMULATED" << endl ;
    }

    virtual void  IRecvVec ( int dest, MPI_Request & request );
    virtual void  RecvVec ( int dest );

    virtual BaseVector * CreateVector ( const Array<int> * procs = 0) const;

    virtual ostream & Print (ostream & ost) const;

    virtual void AddRecvValues( int sender );


    virtual double L2Norm () const;
  };



  template <typename T = double>
  class ParallelVFlatVector : public VFlatVector<T>,
			      public S_ParallelBaseVector<typename mat_traits<T>::TSCAL>
  {
    using ParallelBaseVector :: status;
    using ParallelBaseVector :: paralleldofs;

    Table<T> * recvvalues;

  public:
    typedef typename mat_traits<T>::TSCAL TSCAL;

    explicit ParallelVFlatVector (int as, T * adata) ;
    explicit ParallelVFlatVector (int as, T * adata, ngparallel::ParallelDofs * aparalleldofs ) ;
    explicit ParallelVFlatVector (int as, T * adata, ngparallel::ParallelDofs * aparalleldofs, 
				  PARALLEL_STATUS astatus);
    explicit ParallelVFlatVector ();
    virtual ~ParallelVFlatVector() throw();

    virtual void SetParallelDofs ( ngparallel::ParallelDofs * aparalleldofs, const Array<int> * procs=0 );

    virtual class ngparallel::ParallelDofs * GetParallelDofs () const
    { return paralleldofs; }

    virtual void PrintParallelDofs() const;

    virtual void Distribute() const;

    virtual void PrintStatus ( ostream & ost ) const
    {
      if ( this->status == NOT_PARALLEL )
	ost << "NOT PARALLEL" << endl;
      else if ( this->status == DISTRIBUTED )
	ost << "DISTRIBUTED" << endl ;
      else if (this->status == CUMULATED )
	ost << "CUMULATED" << endl ;
    }

    virtual void IRecvVec ( int dest, MPI_Request & request );
    virtual void RecvVec ( int dest );


    virtual BaseVector * CreateVector ( const Array<int> * procs = 0) const;

    virtual ostream & Print (ostream & ost) const;

    virtual void AddRecvValues( int sender );
  };
#endif

}


#endif
