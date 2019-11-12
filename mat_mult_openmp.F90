      subroutine mat_mult_openmp(n, alpha, A, B, beta, C)

      implicit none

      integer, intent(in)   :: n
      real, intent(in)      :: A(n,n), B(n,n), alpha, beta
      real, intent(inout)   :: C(n,n)

      integer               :: i, j, k
      real                  :: prodsum

#ifdef HAVE_OPENMP
      !$omp parallel shared(A,B,C,alpha,beta,n) private(i,j,k,prodsum)
      !$omp do
      do i=1,n
          do j=1,n
              prodsum = 0.0
              do k=1,n
                  prodsum = prodsum + alpha * A(i,k) * B(k,j)
              end do
              C(i,j) = beta * C(i,j) + prodsum
          end do
      end do
      !$omp end do
      !$omp end parallel
#else
      write(*,'(a)',advance="no") '<<OpenMP variant not implemented>>'
#endif

      end
