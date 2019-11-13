      subroutine mat_mult_smart(n, alpha, A, B, beta, C)
!DIR$ NOOPTIMIZE
!$PRAGMA SUN OPT=0

      implicit none

      integer, intent(in)   :: n
      real, intent(in)      :: A(n,n), B(n,n), alpha, beta
      real, intent(inout)   :: C(n,n)

      integer               :: i, j, k
      real                  :: prodsum

      if ( abs(alpha) <= epsilon(alpha) ) then
          if ( abs(beta) <= epsilon(beta) ) then
              ! alpha = 0, beta = 0
              C = 0.0
          else if ( abs(beta - 1.0) < epsilon(beta) ) then
              ! alpha = 0, beta = 1
              ! =>  C = C
              C = C
          else
              ! alpha = 0, beta != (0,1)
              do j=1,n
                  do i=1,n
                      C(i,j) = beta * C(i,j)
                  end do
              end do
          end if
      else if ( abs(beta) <= epsilon(beta) ) then
          if ( abs(alpha - 1.0) <= epsilon(alpha) ) then
              ! alpha = 1, beta = 0
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + A(i,k) * B(k,j)
                      end do
                      C(i,j) = prodsum
                  end do
              end do
          else
              ! alpha != (0,1), beta = 0
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + alpha * A(i,k) * B(k,j)
                      end do
                      C(i,j) = prodsum
                  end do
              end do
          end if
      else if ( abs(alpha - 1.0) <= epsilon(alpha) ) then
          if ( abs(beta - 1.0) <= epsilon(beta) ) then
              ! alpha = 1, beta = 1
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + A(i,k) * B(k,j)
                      end do
                      C(i,j) = C(i,j) + prodsum
                  end do
              end do
          else
              ! alpha = 1, beta != (0,1)
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + A(i,k) * B(k,j)
                      end do
                      C(i,j) = beta * C(i,j) + prodsum
                  end do
              end do
          end if
      else
          ! alpha != (0,1), beta != (0,1)
          do j=1,n
              do i=1,n
                  prodsum = 0.0
                  do k=1,n
                      prodsum = prodsum + alpha * A(i,k) * B(k,j)
                  end do
                  C(i,j) = beta * C(i,j) + prodsum
              end do
          end do
      end if

      end
