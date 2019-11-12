      subroutine mat_mult_blas(n, alpha, A, B, beta, C)

      implicit none

      integer, intent(in)   :: n
      real, intent(in)      :: A(n,n), B(n,n), alpha, beta
      real, intent(inout)   :: C(n,n)

#ifdef HAVE_BLAS
#   ifdef HAVE_FORTRAN_REAL8
          call dgemm('N', 'N', n, n, n, alpha, A, n, B, n, beta, C, n)
#   else
          call sgemm('N', 'N', n, n, n, alpha, A, n, B, n, beta, C, n)
#   endif
#else
      write(*,'(a)',advance="no") '<<BLAS variant not implemented>>'
#endif

      end
