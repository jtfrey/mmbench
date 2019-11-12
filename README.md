# mmbench

Matrix multiplication benchmarking tool.  The program can be built with various matrix multiplication kernels:

- Baseline Fortran, no compiler optimization
- Baseline Fortran, with compiler optimizations
- OpenMP-parallelization, no compiler optimizations
- OpenMP-parallelization, with compiler optimizations
- BLAS (sgemm/dgemm)

In this case *compiler optimizations* include loop unrolling, inlining, and host/processor-specific tuning and scheduling.

## Building

The CMake build system can be used to configure the build of the program.  From the cloned source directory:

```
$ mkdir build
$ cd build
$ FC=gfortran CC=gcc cmake ..
  :
$ make
```

The Intel and Portland compilers can also be used to build the program (e.g. using "FC=ifort CC=icc" or "FC=pgf90 CC=pgcc").

There are several options that control the features available in the program:

| Option | Default | Description |
| --- | --- | --- |
| `HAVE_FORTRAN_REAL8` | Off | Use 64-bit (double-precision) reals in the Fortran subroutines |
| `HAVE_FORTRAN_INT8` | Off | Use 64-bit integers in the Fortran subroutines |
| `IS_STATIC_BUILD` | Off | Build a static executable (no shared libraries) |
| `NO_BLAS` | Off | Do not search for a BLAS library at all, and do not enable the BLAS variant routine |
| `NO_OPENMP` | Off | Do not determine how to enable OpenMP for the compiler, and do not enable the OpenMP variant routine |
| `NO_DIRECTIO` | Off | Do not determine how to enable `O_DIRECT` or allow direct i/o by the program |
| `CMAKE_INSTALL_PREFIX` | /usr/local | Base path for installation of built components |

For example, to build with double-precision floating point:

```
$ FC=gfortran CC=gcc cmake -DHAVE_FORTRAN_REAL8=On ..
  :
$ make
```

