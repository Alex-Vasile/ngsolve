#ifndef FILE_NGS_SPARSEMATRIX_DYN
#define FILE_NGS_SPARSEMATRIX_DYN

/**************************************************************************/
/* File:   sparsematrix_dyn.hpp                                           */
/* Author: Joachim Schoeberl                                              */
/* Date:   July 2019                                                      */
/**************************************************************************/


namespace ngla
{

/// Sparse matrix with dynamic block size (still experimental)
  template<class TSCAL>
  class  NGS_DLL_HEADER SparseMatrixDynamic : public BaseSparseMatrix, 
                                              public S_BaseMatrix<TSCAL>
  {
  protected:
    size_t bh, bw, bs;
    Array<TSCAL> data;
    TSCAL nul;
    
  public:
    template <typename TM>
      SparseMatrixDynamic (const SparseMatrixTM<TM> & mat)
      : BaseSparseMatrix (mat, false)
    {
      width = mat.Width();
      bh = mat_traits<TM>::HEIGHT;
      bw = mat_traits<TM>::WIDTH;
      bs = bh*bw;
      nze = mat.NZE();
      data.SetSize(nze*bs);
      auto matvec = mat.AsVector().template FV<TM>();
      for (size_t i = 0; i < nze; i++)
        {
          FlatMatrix<TSCAL> fm(bh, bw, &data[i*bs]);
          fm = matvec(i);
        }
    }

    virtual int VHeight() const override { return size; }
    virtual int VWidth() const override { return width; }

    virtual void Mult (const BaseVector & x, BaseVector & y) const override;
    virtual void MultAdd (double s, const BaseVector & x, BaseVector & y) const override;
  };



  template <class TSCAL>
  class  NGS_DLL_HEADER SparseMatrixVariableBlocks : public S_BaseMatrix<TSCAL>
  {
  protected:
    size_t height, width, nblocks;
    Array<int> colnr;
    Array<TSCAL> data;
    Array<size_t> firsti_colnr, firsti_data;
    Array<int> cum_block_size;
    TSCAL nul;
    
  public:
    SparseMatrixVariableBlocks (const SparseMatrixTM<TSCAL> & mat);

    virtual int VHeight() const override { return height; }
    virtual int VWidth() const override { return width; }

    virtual void MultAdd (double s, const BaseVector & x, BaseVector & y) const override;
  };



}
#endif
  