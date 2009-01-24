#ifndef FILE_JACOBI
#define FILE_JACOBI

/* *************************************************************************/
/* File:   jacobi.hh                                                      */
/* Author: Joachim Schoeberl                                              */
/* Date:   06. Oct. 96                                                    */
/* *************************************************************************/


/**
   Jacobi and Gauss Seidel smoother
   for scalar, block and system matrices
 */

class BaseJacobiPrecond : virtual public BaseMatrix
{
public:
  virtual void GSSmooth (BaseVector & x, const BaseVector & b) const = 0;
  virtual void GSSmooth (BaseVector & x, const BaseVector & b, BaseVector & y, BaseVector & help) const = 0;
  virtual void GSSmoothBack (BaseVector & x, const BaseVector & b) const = 0;
};

/// A Jaboci preconditioner for general sparse matrices
template <class TM, class TV_ROW, class TV_COL>
class JacobiPrecond : virtual public BaseJacobiPrecond,
		      virtual public S_BaseMatrix<typename mat_traits<TM>::TSCAL>
{
protected:
  const SparseMatrix<TM,TV_ROW,TV_COL> & mat;
  ///
  const BitArray * inner;
  ///
  int height;
  ///
  TM * invdiag;
public:
  // typedef typename mat_traits<TM>::TV_ROW TVX;
  typedef typename mat_traits<TM>::TSCAL TSCAL;

  ///
  JacobiPrecond (const SparseMatrix<TM,TV_ROW,TV_COL> & amat, 
		 const BitArray * ainner = NULL);

  ///
  virtual ~JacobiPrecond ();
  
  ///
  virtual void MultAdd (TSCAL s, const BaseVector & x, BaseVector & y) const;

  virtual void MultTransAdd (TSCAL s, const BaseVector & x, BaseVector & y) const
  { MultAdd (s, x, y); }
  ///
  virtual BaseVector * CreateVector () const;
  ///
  virtual void GSSmooth (BaseVector & x, const BaseVector & b) const;

  /// computes partial residual y
  virtual void GSSmooth (BaseVector & x, const BaseVector & b, BaseVector & y, BaseVector & help) const
  {
    GSSmooth (x, b);
  }

  ///
  virtual void GSSmoothBack (BaseVector & x, const BaseVector & b) const;

  ///
  virtual void GSSmoothNumbering (BaseVector & x, const BaseVector & b,
				  const Array<int> & numbering, 
				  int forward = 1) const;
};









/// A Jaboci preconditioner for symmetric sparse matrices
template <class TM, class TV>
class JacobiPrecondSymmetric : public JacobiPrecond<TM,TV,TV>
{
public:
  typedef TV TVX;

  ///
  JacobiPrecondSymmetric (const SparseMatrixSymmetric<TM,TV> & amat, 
			  const BitArray * ainner = NULL);

  ///
  virtual void GSSmooth (BaseVector & x, const BaseVector & b) const;

  /// computes partial residual y
  virtual void GSSmooth (BaseVector & x, const BaseVector & b, BaseVector & y, BaseVector & help) const;

  ///
  virtual void GSSmoothBack (BaseVector & x, const BaseVector & b) const;

  ///
  virtual void GSSmoothNumbering (BaseVector & x, const BaseVector & b,
				  const Array<int> & numbering, 
				  int forward = 1) const;
};







#endif
