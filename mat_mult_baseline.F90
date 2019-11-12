      subroutine mat_mult_baseline(n, alpha, A, B, beta, C)
!DIR$ NOOPTIMIZE
!$PRAGMA SUN OPT=0

      implicit none

      integer, intent(in)   :: n
      real, intent(in)      :: A(n,n), B(n,n), alpha, beta
      real, intent(inout)   :: C(n,n)

      integer               :: i, j, k
      real                  :: prodsum

      do i=1,n
          do j=1,n
              prodsum = 0.0
              do k=1,n
                  prodsum = prodsum + alpha * A(i,k) * B(k,j)
              end do
              C(i,j) = beta * C(i,j) + prodsum
          end do
      end do

      end
