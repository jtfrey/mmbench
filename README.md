# mmbench

Matrix multiplication benchmarking tool.  The program can be built with various matrix multiplication kernels:

- Baseline Fortran, no compiler optimization
- Smart Fortran (eliminates FP ops for alpha/beta of 0.0/1.0), no compiler optimization
- Smart Fortran, with compiler optimizations
- OpenMP-parallelization of smart Fortran, no compiler optimizations
- OpenMP-parallelization of smart Fortran, with compiler optimizations
- BLAS (sgemm/dgemm)

In this case *compiler optimizations* include loop unrolling, inlining, and host/processor-specific tuning and scheduling.

There are also multiple matrix initialization methods available:

- None
- Zero (memset())
- Simple formula
- Random values
- Binary read from file (options for direct, sync, noatime)

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
| `HAVE_FORTRAN_LOGICAL8` | Off | Use 64-bit logicals in the Fortran subroutines |
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
$ ./mmbench --help
usage:

  ./mmbench [options]

 options:

  -h/--help                            display this information
  -v/--verbose                         increase the amount of information displayed
  -f/--format <format>                 output format for the timing data (default: table)

      <format> = (table|csv|tsv|json|yaml)

  -t/--nthreads <integer>              OpenMP code should use this many threads max; zero
                                       implies that the OpenMP runtime default should be used
                                       (which possibly comes from e.g. OMP_NUM_THREADS)
  -A/--no-align                        do not allocate aligned memory regions
  -B/--align <integer>                 align allocated regions to this byte size
                                       (default: 8)
  -i/--init <init-method>              initialize matrices with this method
                                       (default: noop)

      <init-method> = (noop|zero|simple|simple-omp|random{=###}|file={opt{,..}:}<name>)

  -r/--routines <routine-spec>         augment the list of routines to perform
                                       (default: basic,basic-fortran)

      <routine-spec> = {+|-}(all|basic|basic-fortran|smart-fortran|opt-fortran|basic-fortran-omp|opt-fortran-omp|blas|blas-fortran){,...}


 calculation performed is:

      C = alpha * A . B + beta * C

  -l/--nloop <integer>                 number of times to perform calculation for each
                                       chosen routine; counts greater than 1 will show
                                       averaged timings (default: 4)
  -n/--dimension <integer>             dimension of the matrices (default: 1000)
  -a/--alpha <real>                    alpha value in equation (default: 1)
  -b/--beta <real>                     beta value in equation (default: 0)
```

The simplest test is to just execute `./mmbench` without any flags:

```
$ ./mmbench
Starting test of methods: noop, basic

                   basic       last value         miniumum          maximum          average         variance    std deviation
------------------------ ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
                Walltime         0.904813         0.904813          1.11065         0.956533        0.0105566         0.102745
           User CPU time         0.904708         0.904582          1.11048         0.956268        0.0105703         0.102812
         System CPU time            3e-06                0         0.000572       0.00014375      8.15123e-08      0.000285504
        rusage.ru_maxrss            15420            15276            15420            15384             5184               72
         rusage.ru_nswap                0                0                0                0                0                0
       rusage.ru_inblock                0                0                0                0                0                0
      rusage.ru_outblock                0                0                0                0                0                0


Starting test of methods: noop, basic-fortran

           basic-fortran       last value         miniumum          maximum          average         variance    std deviation
------------------------ ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
                Walltime          7.08075          7.08075          7.08356           7.0825      1.69123e-06       0.00130047
           User CPU time          7.07996          7.07945           7.0826          7.08095      2.20455e-06       0.00148477
         System CPU time                0                0         0.001999         0.000749      9.15335e-07      0.000956732
        rusage.ru_maxrss            15440            15440            15440            15440                0                0
         rusage.ru_nswap                0                0                0                0                0                0
       rusage.ru_inblock                0                0                0                0                0                0
      rusage.ru_outblock                0                0                0                0                0                0


Matrix initialization timing results:

                    noop       last value         miniumum          maximum          average         variance    std deviation
------------------------ ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
                Walltime         9.22e-07         8.91e-07        2.917e-06      1.02125e-06      1.67084e-13       4.0876e-07
           User CPU time                0                0            1e-06      8.33333e-08      7.97101e-14       2.8233e-07
         System CPU time                0                0            1e-06      8.33333e-08      7.97101e-14       2.8233e-07
        rusage.ru_maxrss            15440            11432            15440            14911      1.80697e+06          1344.24
         rusage.ru_nswap                0                0                0                0                0                0
       rusage.ru_inblock                0                0                0                0                0                0
      rusage.ru_outblock                0                0                0                0                0                0

```

To explicitly only test the BLAS variant (which in this example is linked against MKL) using random matrices:

```
$ ./mmbench --init=random --routines==blas
Starting test of methods: random, blas

                    blas       last value         miniumum          maximum          average         variance    std deviation
------------------------ ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
                Walltime       0.00229291       0.00229291         0.455823         0.115742        0.0514023         0.226721
           User CPU time          0.08172         0.078486         0.161008         0.103014       0.00152216        0.0390149
         System CPU time                0                0         0.078607        0.0205448       0.00150116        0.0387448
        rusage.ru_maxrss           168940           168940           168940           168940                0                0
         rusage.ru_nswap                0                0                0                0                0                0
       rusage.ru_inblock                0                0           104224            26056      2.71566e+09            52112
      rusage.ru_outblock                0                0                0                0                0                0


Matrix initialization timing results:

                  random       last value         miniumum          maximum          average         variance    std deviation
------------------------ ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
                Walltime        0.0214437        0.0155221        0.0352457        0.0226773      2.85907e-05       0.00534703
           User CPU time         0.744302         0.012482          1.19908         0.636958          0.15895         0.398686
         System CPU time         0.026802                0         0.068637        0.0292212      0.000408788        0.0202185
        rusage.ru_maxrss           168940            15336           168940           131511      4.58753e+09          67731.3
         rusage.ru_nswap                0                0                0                0                0                0
       rusage.ru_inblock                0                0                0                0                0                0
      rusage.ru_outblock                0                0                0                0                0                0

```

Or to exercise a direct i/o file-based matrix initialization (with smaller matrix dimension):

```
$ dd if=/dev/urandom of=mat bs=8000 count=1000
$ ./mmbench --init=file=direct:mat --routines==opt-fortran
Starting test of methods: file, opt-fortran

             opt-fortran       last value         miniumum          maximum          average         variance    std deviation
------------------------ ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
                Walltime        0.0805436        0.0805433        0.0811797        0.0808243      1.08838e-07      0.000329907
           User CPU time         0.080204         0.080204         0.081175        0.0806755       2.4549e-07      0.000495469
         System CPU time         0.000335                0         0.000335       0.00014425      2.91856e-08      0.000170838
        rusage.ru_maxrss            23608            23188            23608            23476            39456          198.635
         rusage.ru_nswap                0                0                0                0                0                0
       rusage.ru_inblock                0                0                0                0                0                0
      rusage.ru_outblock                0                0                0                0                0                0


Matrix initialization timing results:

                    file       last value         miniumum          maximum          average         variance    std deviation
------------------------ ---------------- ---------------- ---------------- ---------------- ---------------- ----------------
                Walltime         0.189213         0.189129         0.274364         0.196867      0.000595933        0.0244117
           User CPU time         0.015008         0.011788         0.021381        0.0162526      8.68436e-06       0.00294692
         System CPU time         0.174196         0.169903         0.252382         0.180556      0.000516752        0.0227322
        rusage.ru_maxrss            23608            15404            23608          22424.3      6.32978e+06           2515.9
         rusage.ru_nswap                0                0                0                0                0                0
       rusage.ru_inblock                0                0            15632          1302.67      2.03633e+07          4512.57
      rusage.ru_outblock                0                0                0                0                0                0

```
