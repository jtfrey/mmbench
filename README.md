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

## Execution

```
$ ./mm.x --help
usage:

  ./mm.x [options]

 options:

  -h/--help                            display this information
  -d/--direct-io                       all files should be opened for direct i/o
  -t/--nthreads <integer>              OpenMP code should use this many threads max; zero
                                       implies that the OpenMP runtime default should be used
                                       (which possibly comes from e.g. OMP_NUM_THREADS)
  -s/--randomseed <integer>            seed the PRNG with this starting value
  -A/--no-align                        do not allocate aligned memory regions
  -B/--align <integer>                 align allocated regions to this byte size
                                       (default: 8)
  -i/--init <init-method>              initialize matrices with this method
                                       (default: noop)

      <init-method> = (noop|simple|random|file=<path>)

  -r/--routines <routine-spec>         augment the list of routines to perform
                                       (default: basic,opt)

      <routine-spec> = {+|-}(all|basic|opt|openmp|openmp_opt|blas){,...}


 calculation performed is:

      C = alpha * A . B + beta * C

  -l/--nloop <integer>                 number of times to perform calculation for each
                                       chosen routine; counts greater than 1 will show
                                       averaged timings (default: 4)
  -n/--dimension <integer>             dimension of the matrices (default: 1000)
  -a/--alpha <real>                    alpha value in equation (default: 1)
  -b/--beta <real>                     beta value in equation (default: 0)

```

The simplest test is to just execute `./mm.x` without any flags:

```
$ ./mm.x
INFO:  Using matrix initialization method 'noop'
INFO:  Using matrix multiplication routines 'basic,opt'
INFO:  Allocating matrices with alignment of 8 bytes
                                                                          walltime  |        user cpu  |      system cpu 
INIT:     Noop matrix initialization:                               8.024300000e-05 |  4.310000000e-04 |  8.840000000e-04
START:    Baseline n x n matrix multiply:                           5.141303350e+00 |  6.287292000e+00 |  3.084725000e+00
INIT:     Noop matrix initialization:                               2.963000000e-06 |  1.000000000e-06 |  1.000000000e-06
START:    Baseline n x n matrix multiply:                           4.883241605e+00 |  4.882736000e+00 |  0.000000000e+00
INIT:     Noop matrix initialization:                               2.884000000e-06 |  2.000000000e-06 |  0.000000000e+00
START:    Baseline n x n matrix multiply:                           4.888432435e+00 |  4.887408000e+00 |  5.200000000e-04
INIT:     Noop matrix initialization:                               2.912000000e-06 |  2.000000000e-06 |  1.000000000e-06
START:    Baseline n x n matrix multiply:                           4.882876768e+00 |  4.882377000e+00 |  0.000000000e+00
AVG INIT:                                                           2.225050000e-05 |  1.090000000e-04 |  2.215000000e-04
AVG MULT:                                                           4.948963540e+00 |  5.234953250e+00 |  7.713112500e-01

INIT:     Noop matrix initialization:                               2.961000000e-06 |  2.000000000e-06 |  0.000000000e+00
START:    Baseline (optimized) n x n matrix multiply:               9.125678750e-01 |  9.124730000e-01 |  0.000000000e+00
INIT:     Noop matrix initialization:                               2.826000000e-06 |  2.000000000e-06 |  0.000000000e+00
START:    Baseline (optimized) n x n matrix multiply:               9.129683200e-01 |  9.128680000e-01 |  0.000000000e+00
INIT:     Noop matrix initialization:                               2.844000000e-06 |  2.000000000e-06 |  0.000000000e+00
START:    Baseline (optimized) n x n matrix multiply:               9.126583860e-01 |  9.125630000e-01 |  0.000000000e+00
INIT:     Noop matrix initialization:                               2.823000000e-06 |  2.000000000e-06 |  0.000000000e+00
START:    Baseline (optimized) n x n matrix multiply:               9.122169010e-01 |  9.121220000e-01 |  0.000000000e+00
AVG INIT:                                                           2.863500000e-06 |  2.000000000e-06 |  0.000000000e+00
AVG MULT:                                                           9.126028705e-01 |  9.125065000e-01 |  0.000000000e+00
```

To explicitly only test the BLAS variant (which in this example is linked against the Portland default BLAS):

```
$ ./mm.x --routines==blas
INFO:  Using matrix initialization method 'noop'
INFO:  Using matrix multiplication routines 'blas'
INFO:  Allocating matrices with alignment of 8 bytes
                                                                          walltime  |        user cpu  |      system cpu 
INIT:     Noop matrix initialization:                               8.031400000e-05 |  5.000000000e-04 |  8.950000000e-04
START:    BLAS n x n matrix multiply:                               5.172554300e-02 |  5.447710000e-01 |  1.315743000e+00
INIT:     Noop matrix initialization:                               6.323800000e-05 |  3.460000000e-04 |  8.180000000e-04
START:    BLAS n x n matrix multiply:                               3.954175800e-02 |  3.974420000e-01 |  1.024535000e+00
INIT:     Noop matrix initialization:                               5.596000000e-05 |  2.900000000e-04 |  7.120000000e-04
START:    BLAS n x n matrix multiply:                               3.639290700e-02 |  3.713540000e-01 |  8.211290000e-01
INIT:     Noop matrix initialization:                               4.267000000e-06 |  1.000000000e-06 |  2.000000000e-06
START:    BLAS n x n matrix multiply:                               3.629492400e-02 |  3.606800000e-02 |  2.210000000e-04
AVG INIT:                                                           5.094475000e-05 |  2.842500000e-04 |  6.067500000e-04
AVG MULT:                                                           4.098878300e-02 |  3.374087500e-01 |  7.904070000e-01
```

Or to exercise a direct i/o file-based matrix initialization (with smaller matrix dimension):

```
$ ./mm.x --init=file=mat --routines==opt -n100 --direct-io
INFO:  Using matrix initialization method 'file=mat'
INFO:  Using matrix multiplication routines 'opt'
INFO:  Allocating matrices with alignment of 8 bytes
                                                                          walltime  |        user cpu  |      system cpu 
INIT:     File-based matrix initialization:                         3.229985637e+00 |  1.245620000e+00 |  3.481999000e+00
START:    Baseline (optimized) n x n matrix multiply:               7.919956000e-03 |  7.915000000e-03 |  0.000000000e+00
INIT:     File-based matrix initialization:                         3.202796434e+00 |  2.042400000e-02 |  2.968670000e-01
START:    Baseline (optimized) n x n matrix multiply:               8.526381000e-03 |  8.521000000e-03 |  0.000000000e+00
INIT:     File-based matrix initialization:                         3.034241127e+00 |  2.403800000e-02 |  3.069780000e-01
START:    Baseline (optimized) n x n matrix multiply:               8.381360000e-03 |  8.178000000e-03 |  1.920000000e-04
INIT:     File-based matrix initialization:                         2.995645767e+00 |  1.868500000e-02 |  3.060000000e-01
START:    Baseline (optimized) n x n matrix multiply:               8.563715000e-03 |  8.559000000e-03 |  0.000000000e+00
AVG INIT:                                                           3.115667241e+00 |  3.271917500e-01 |  1.097961000e+00
AVG MULT:                                                           8.347853000e-03 |  8.293250000e-03 |  4.800000000e-05
```
