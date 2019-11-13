      subroutine mat_mult_openmp_optimized(n, alpha, A, B, beta, C)

      implicit none

      integer, intent(in)   :: n
      real, intent(in)      :: A(n,n), B(n,n), alpha, beta
      real, intent(inout)   :: C(n,n)

      integer               :: i, j, k
      real                  :: prodsum

#ifdef HAVE_OPENMP
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
              !$omp parallel shared(A,B,C,alpha,beta,n) private(i,j,k,prodsum)
              !$omp do
              do j=1,n
                  do i=1,n
                      C(i,j) = beta * C(i,j)
                  end do
              end do
              !$omp end do
              !$omp end parallel
          end if
      else if ( abs(beta) <= epsilon(beta) ) then
          if ( abs(alpha - 1.0) <= epsilon(alpha) ) then
              ! alpha = 1, beta = 0
              !$omp parallel shared(A,B,C,alpha,beta,n) private(i,j,k,prodsum)
              !$omp do
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + A(i,k) * B(k,j)
                      end do
                      C(i,j) = prodsum
                  end do
              end do
              !$omp end do
              !$omp end parallel
          else
              ! alpha != (0,1), beta = 0
              !$omp parallel shared(A,B,C,alpha,beta,n) private(i,j,k,prodsum)
              !$omp do
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + alpha * A(i,k) * B(k,j)
                      end do
                      C(i,j) = prodsum
                  end do
              end do
              !$omp end do
              !$omp end parallel
          end if
      else if ( abs(alpha - 1.0) <= epsilon(alpha) ) then
          if ( abs(beta - 1.0) <= epsilon(beta) ) then
              ! alpha = 1, beta = 1
              !$omp parallel shared(A,B,C,alpha,beta,n) private(i,j,k,prodsum)
              !$omp do
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + A(i,k) * B(k,j)
                      end do
                      C(i,j) = C(i,j) + prodsum
                  end do
              end do
              !$omp end do
              !$omp end parallel
          else
              ! alpha = 1, beta != (0,1)
              !$omp parallel shared(A,B,C,alpha,beta,n) private(i,j,k,prodsum)
              !$omp do
              do j=1,n
                  do i=1,n
                      prodsum = 0.0
                      do k=1,n
                          prodsum = prodsum + A(i,k) * B(k,j)
                      end do
                      C(i,j) = beta * C(i,j) + prodsum
                  end do
              end do
              !$omp end do
              !$omp end parallel
          end if
      else
          ! alpha != (0,1), beta != (0,1)
          !$omp parallel shared(A,B,C,alpha,beta,n) private(i,j,k,prodsum)
          !$omp do
          do j=1,n
              do i=1,n
                  prodsum = 0.0
                  do k=1,n
                      prodsum = prodsum + alpha * A(i,k) * B(k,j)
                  end do
                  C(i,j) = beta * C(i,j) + prodsum
              end do
          end do
          !$omp end do
          !$omp end parallel
      end if
#else
      write(*,'(a)',advance="no") '<<OpenMP optimized variant not implemented>>'
#endif

      end
