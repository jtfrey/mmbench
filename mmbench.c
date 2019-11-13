/*
 * mmbench.c
 *
 * C driver for the Fortran-based matrix multiplication subroutines.  Performs all
 * memory allocations, matrix initializations, and timing operations.
 *
 */

#ifdef HAVE_DIRECTIO
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <getopt.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_OPENMP
#   include <omp.h>
#endif

//
// Various compile-time constants that act as default values for
// some of the CLI options:
//
#define DEFAULT_MATRIX_DIMENSION    1000
#define DEFAULT_ALPHA               ONE
#define DEFAULT_BETA                ZERO
#define DEFAULT_ALLOC_ALIGNMENT     8
#define DEFAULT_NLOOP               4

//
// C types corresponding to the Fortran types we're using.  Defaults
// to 32-bit integer and real.  The build system forces a default size
// on the Fortran types (e.g. using -fdefault-real-8) and defines the
// corresponding macro (e.g. HAVE_FORTRAN_REAL8).
//
#ifdef HAVE_FORTRAN_INT8
    typedef int64_t f_integer;
#   define FMT_F_INT    "%ld"
#else
    typedef int32_t f_integer;
#   define FMT_F_INT    "%d"
#endif

#ifdef HAVE_FORTRAN_REAL8
    typedef double f_real;
    f_real ONE=1.0;
    f_real ZERO=0.0;
#   define FMT_F_REAL   "%lg"
#else
    typedef float f_real;
    f_real ONE=1.0f;
    f_real ZERO=0.0f;
#   define FMT_F_REAL   "%g"
#endif

//
// The Fortran subroutines:
//
void mat_mult_basic_(f_integer*, f_real*, f_real*, f_real*, f_real*, f_real*, f_integer, f_integer, f_integer, f_integer, f_integer, f_integer);
void mat_mult_smart_(f_integer*, f_real*, f_real*, f_real*, f_real*, f_real*, f_integer, f_integer, f_integer, f_integer, f_integer, f_integer);
void mat_mult_optimized_(f_integer*, f_real*, f_real*, f_real*, f_real*, f_real*, f_integer, f_integer, f_integer, f_integer, f_integer, f_integer);
void mat_mult_openmp_(f_integer*, f_real*, f_real*, f_real*, f_real*, f_real*, f_integer, f_integer, f_integer, f_integer, f_integer, f_integer);
void mat_mult_openmp_optimized_(f_integer*, f_real*, f_real*, f_real*, f_real*, f_real*, f_integer, f_integer, f_integer, f_integer, f_integer, f_integer);
void mat_mult_blas_(f_integer*, f_real*, f_real*, f_real*, f_real*, f_real*, f_integer, f_integer, f_integer, f_integer, f_integer, f_integer);

//
// The different matrix multiplication routines we have available in
// this program.  Includes helper functions to stringify a bit vector
// of selected routines and parse a string that modifies said bit
// vector of selected routines.
//
typedef enum {
    kMultiplyRoutineBasic       = 1 << 0,
    kMultiplyRoutineSmart       = 1 << 1,
    kMultiplyRoutineOpt         = 1 << 2,
    kMultiplyRoutineOpenMPOpt   = 1 << 3,
    kMultiplyRoutineOpenMP      = 1 << 4,
    kMultiplyRoutineBLAS        = 1 << 5,

    kMultiplyRoutineMin         = kMultiplyRoutineBasic,
    kMultiplyRoutineMax         = kMultiplyRoutineBLAS,

    kMultiplyRoutineNone        = 0,

    kMultiplyRoutineAll         = kMultiplyRoutineBasic | kMultiplyRoutineSmart | kMultiplyRoutineOpt | kMultiplyRoutineOpenMP | kMultiplyRoutineOpenMPOpt | kMultiplyRoutineBLAS,

    kMultiplyRoutineDefault     = kMultiplyRoutineBasic | kMultiplyRoutineSmart | kMultiplyRoutineOpt
} MultiplyRoutine_t;

const char* MultiplyRoutine_strs[] = {
                            "basic",
                            "smart",
                            "opt",
                            "openmp_opt",
                            "openmp",
                            "blas",
                            NULL
                        };

const char*
MultiplyRoutine_toString(
    MultiplyRoutine_t   routines
)
{
    static char         routineStr[64];
    int                 i = 0, ridx = 0;
    MultiplyRoutine_t   r = kMultiplyRoutineMin;

    while ( r <= kMultiplyRoutineMax ) {
        if ( (routines & r) != 0 ) {
            i += snprintf(routineStr + i, (sizeof(routineStr) - i), "%s%s", (i > 0 ? "," : ""), MultiplyRoutine_strs[ridx]);
        }
        r <<= 1;
        ridx++;
    }
    return routineStr;
}

//
// The MultiplyRoutine_parseSpec() function understands the following
// dialect:
//
//   - leading "=" sign zeroes the bit vector (prepares for direct assignment of
//     routines)
//   - a "-" prefix on a name unsets that routine in the bit vector
//   - a "+" prefix on a name sets that routine in the bit vector
//   - the name "all" unsets/sets all routines in the bit vector
//
// Multiple values are separated by a comma; whitespace is not allowed.
//
bool
MultiplyRoutine_parseSpec(
    MultiplyRoutine_t   *routines,
    const char          *spec
)
{
    if ( *spec == '=' ) {
        *routines = 0;
        spec++;
    }
    while ( *spec ) {
        while ( *spec == ',' ) spec++;
        if ( *spec) {
            bool        add = true;

            if ( *spec == '-' ) {
                add = false;
                spec++;
            } else if ( *spec == '+' ) {
                spec++;
            }
            if ( *spec ) {
                int         r = kMultiplyRoutineMin, ridx = 0;
                int         slen;

                if ( strncasecmp(spec, "all", 3) == 0 ) {
                    if ( (spec[3] == ',' || spec[3] == '\0') ) {
                        if ( add ) {
                            *routines = kMultiplyRoutineAll;
                        } else {
                            *routines = 0;
                        }
                        spec += 3;
                    } else {
                        break;
                    }
                } else {
                    while ( r <= kMultiplyRoutineMax ) {
                        slen = strlen(MultiplyRoutine_strs[ridx]);
                        if ( strncasecmp(spec, MultiplyRoutine_strs[ridx], slen) == 0 ) break;
                        r <<= 1;
                        ridx++;
                    }
                    if ( (r <= kMultiplyRoutineMax) && (spec[slen] == ',' || spec[slen] == '\0') ) {
                        if ( add ) {
                            *routines |= r;
                        } else {
                            *routines = *routines & (~r);
                        }
                        spec += slen;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    return (*spec) ? false : true;
}

//
// The different matrix initialization methods available in this program.  The
// "from file" method reads one f_real value at a time from the file, and is thus
// affected by the direct i/o setting (when available).  Includes helper functions
// to stringify the selected method and parse a string specifying the method.
//
typedef enum {
    kMatrixInitNoop         = 0,
    kMatrixInitZero,
    kMatrixInitSimple,
    kMatrixInitRandom,
    kMatrixInitFromFile,
    kMatrixInitMax,

    kMatrixInitDefault      = kMatrixInitNoop
} MatrixInit_t;

const char* MatrixInit_strs[] = {
                        "noop",
                        "zero",
                        "simple",
                        "random",
                        NULL
                    };

#ifndef MATRIXINIT_DEFAULT_FILE
#    define MATRIXINIT_DEFAULT_FILE "/dev/urandom"
#endif

const char*
MatrixInit_toString(
    MatrixInit_t    matrixInit,
    const char      *arg
)
{
    static char     outvalue[PATH_MAX+6];

    switch (matrixInit) {
        case kMatrixInitNoop:
        case kMatrixInitZero:
        case kMatrixInitSimple:
        case kMatrixInitRandom:
            return MatrixInit_strs[matrixInit];
        case kMatrixInitFromFile:
            snprintf(outvalue, sizeof(outvalue), "file=%s", arg ? arg : MATRIXINIT_DEFAULT_FILE);
            break;
    }
    return outvalue;
}

bool
MatrixInit_parse(
    MatrixInit_t    *method,
    const char*     *arg,
    const char      *str
)
{
    const char*     *list = MatrixInit_strs;
    int             i = kMatrixInitNoop;

    while ( *list ) {
        if ( strcasecmp(str, *list) == 0 ) {
            *method = i;
            if ( *arg ) free((void*)*arg);
            *arg = NULL;
            return true;
        }
        i++;
        list++;
    }
    // Test for a file=<path> value:
    if ( strncasecmp(str, "file", 4) == 0 ) {
        *method = kMatrixInitFromFile;
        if (*arg) free((void*)*arg);
        if ( *(str + 4) == '=' ) {
            *arg = strdup(str + 5);
        } else {
            *arg = strdup(MATRIXINIT_DEFAULT_FILE);
        }
        return true;
    }
    return false;
}

//
// Initialize a matrix from the given file.  Elements are read one f_real at a time
// from the file; when the end-of-file is reached (or the returned byte count underflows
// the size of f_real) the file is rewound and reused.
//
void
MatrixInit_fromFile(
    int             fd,
    const char      *filename,
    const char      *matrixName,
    f_integer       n,
    f_real          *M
)
{
    f_integer       i, j;

    for ( i = 0; i < n; i ++ ) {
        for ( j = 0; j < n; j++ ) {
            ssize_t     actual = read(fd, &M[i * n + j], sizeof(f_real));

            if ( actual < sizeof(f_real) ) {
                if ( lseek(fd, 0, SEEK_SET) != 0 ) {
                    fprintf(stderr, "ERROR:  unable to rewind matrix initialization file (%s[%d,%d]): %s (errno = %d)\n", matrixName, i, j, filename, errno);
                    exit(errno);
                }
                actual = read(fd, &M[i * n + j], sizeof(f_real));
            }
            if ( actual != sizeof(f_real) ) {
                fprintf(stderr, "ERROR:  unable to read from matrix initialization file (%s[%d,%d]): %s (errno = %d)\n", matrixName, i, j, filename, errno);
                exit(errno);
            }
        }
    }
}


//
// CLI options this program recognizes:
//
struct option cli_opts[] = {
        { "help",           no_argument,        NULL,           'h' },
#ifdef HAVE_DIRECTIO
        { "direct-io",      no_argument,        NULL,           'd' },
#endif
#ifdef HAVE_OPENMP
        { "nthreads",       required_argument,  NULL,           't' },
#endif
        { "no-align",       no_argument,        NULL,           'A' },
        { "align",          required_argument,  NULL,           'S' },
        { "init",           required_argument,  NULL,           'i' },
        { "routines",       required_argument,  NULL,           'r' },
        { "randomseed",     required_argument,  NULL,           's' },
        { "dimension",      required_argument,  NULL,           'n' },
        { "alpha",          required_argument,  NULL,           'a' },
        { "beta",           required_argument,  NULL,           'b' },
        { NULL,             0,                  0,              0   }
    };

const char *cli_optstring = ""
#ifdef HAVE_DIRECTIO
    "d"
#endif
#ifdef HAVE_OPENMP
    "t:"
#endif
    "hAS:i:r:s:n:a:b:";

//
// Writes a program usage help screen and exits.
//
void
usage(
    const char      *exe
)
{
    printf(
        "usage:\n\n"
        "  %s [options]\n\n"
        " options:\n\n"
        "  -h/--help                            display this information\n"
#ifdef HAVE_DIRECTIO
        "  -d/--direct-io                       all files should be opened for direct i/o\n"
#endif
#ifdef HAVE_OPENMP
        "  -t/--nthreads <integer>              OpenMP code should use this many threads max; zero\n"
        "                                       implies that the OpenMP runtime default should be used\n"
        "                                       (which possibly comes from e.g. OMP_NUM_THREADS)\n"
#endif
        "  -s/--randomseed <integer>            seed the PRNG with this starting value\n"
        "  -A/--no-align                        do not allocate aligned memory regions\n"
        "  -B/--align <integer>                 align allocated regions to this byte size\n"
        "                                       (default: "FMT_F_INT")\n"
        "  -i/--init <init-method>              initialize matrices with this method\n"
        "                                       (default: %s)\n\n"
        "      <init-method> = (noop|simple|random|file=<path>)\n\n"
        "  -r/--routines <routine-spec>         augment the list of routines to perform\n"
        "                                       (default: %s)\n\n"
        "      <routine-spec> = {+|-}(all|basic|smart|opt|openmp|openmp_opt|blas){,...}\n\n"
        "\n"
        " calculation performed is:\n\n"
        "      C = alpha * A . B + beta * C\n"
        "\n"
        "  -l/--nloop <integer>                 number of times to perform calculation for each\n"
        "                                       chosen routine; counts greater than 1 will show\n"
        "                                       averaged timings (default: "FMT_F_INT")\n"
        "  -n/--dimension <integer>             dimension of the matrices (default: "FMT_F_INT")\n"
        "  -a/--alpha <real>                    alpha value in equation (default: "FMT_F_REAL")\n"
        "  -b/--beta <real>                     beta value in equation (default: "FMT_F_REAL")\n"
        "\n",
        exe,
        (f_integer)DEFAULT_ALLOC_ALIGNMENT,
        MatrixInit_toString(kMatrixInitDefault, NULL),
        MultiplyRoutine_toString(kMultiplyRoutineDefault),
        (f_integer)DEFAULT_NLOOP,
        (f_integer)DEFAULT_MATRIX_DIMENSION,
        (f_real)DEFAULT_ALPHA,
        (f_real)DEFAULT_BETA
      );
    exit(0);
}

//
// Type that wraps wall and CPU timings.
//
typedef struct {
    double      walltime;
    double      userCpu;
    double      systemCpu;
} timings_t;

//
// Returns a timings_t initialized to zeroes.  Useful for e.g.
//
//      timings_t       T = timings_init();
//
timings_t
timings_init()
{
    timings_t   T = { 0.0, 0.0, 0.0 };
    return T;
}

//
// Add T2 timing values to T1.
//
void
timings_add(
    timings_t   *T1,
    timings_t   *T2
)
{
    T1->walltime += T2->walltime;
    T1->userCpu += T2->userCpu;
    T1->systemCpu += T2->systemCpu;
}

//
// Start/stop the timing clock.  On stop, the timings data structure is filled-in with the
// results.
//
void
timing_clock(
    timings_t   *timings
)
{
    static bool             isStarted = false;
    static struct timespec  startTime, endTime;
    static struct rusage    startUsage, endUsage;
    double                  v;

    if ( isStarted ) {
        getrusage(RUSAGE_SELF, &endUsage);
        clock_gettime(CLOCK_MONOTONIC, &endTime);
        isStarted = false;
        v = (double)(endTime.tv_sec - startTime.tv_sec);
        timings->walltime = v + 1e-9 * (double)(endTime.tv_nsec - startTime.tv_nsec);
        v = (endUsage.ru_utime.tv_sec - startUsage.ru_utime.tv_sec);
        timings->userCpu = v + 1e-6 * (endUsage.ru_utime.tv_usec - startUsage.ru_utime.tv_usec);
        v = (endUsage.ru_stime.tv_sec - startUsage.ru_stime.tv_sec);
        timings->systemCpu = v + 1e-6 * (endUsage.ru_stime.tv_usec - startUsage.ru_stime.tv_usec);
    } else {
        isStarted = true;
        clock_gettime(CLOCK_MONOTONIC, &startTime);
        getrusage(RUSAGE_SELF, &startUsage);
    }
}

//
// Helper macros for starting and stopping the timer.
//
#define TIMING_START() timing_clock(NULL)
#define TIMING_END(T) timing_clock(&(T))

//
// Main program.
//
int
main(
    int                 argc,
    char* const         argv[]
)
{
    const char          *exe = argv[0];
    f_integer           n = DEFAULT_MATRIX_DIMENSION, nloop = DEFAULT_NLOOP;
#ifdef HAVE_OPENMP
    int                 nthreads = 0;
#endif
    f_real              *A=NULL, *B=NULL, *C=NULL, alpha = DEFAULT_ALPHA, beta = DEFAULT_BETA;

    MultiplyRoutine_t   multRoutines = kMultiplyRoutineDefault, r;

    MatrixInit_t        matrixInit = kMatrixInitDefault;
    const char          *matrixInitArg = NULL;

    size_t              allocAlign = DEFAULT_ALLOC_ALIGNMENT;
    bool                shouldAlign = true;

    bool                shouldUseDirectIO = false;

    unsigned int        random_seed;

    int                 optc;

    //
    // Process CLI arguments:
    //
    while ( (optc = getopt_long(argc, argv, cli_optstring, cli_opts, NULL)) != -1 ) {
        switch (optc) {
            case 'h': {
                usage(exe);
                break;
            }

            case 'd': {
                shouldUseDirectIO = true;
                break;
            }

            case 'A': {
                shouldAlign = false;
                break;
            }

            case 'i': {
                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no matrix init specification provided\n");
                    exit(EINVAL);
                }
                if ( ! MatrixInit_parse(&matrixInit, &matrixInitArg, optarg) ) {
                    fprintf(stderr, "ERROR:  invalid matrix init method: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }

            case 'r': {
                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no routine specification provided\n");
                    exit(EINVAL);
                }
                if ( ! MultiplyRoutine_parseSpec(&multRoutines, optarg) ) {
                    fprintf(stderr, "ERROR:  invalid routine specification: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }

            case 'a': {
                char        *end;

                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no value provided for alpha coefficient\n");
                    exit(EINVAL);
                }
                alpha = strtod(optarg, &end);
                if ( end == NULL || end == optarg ) {
                    fprintf(stderr, "ERROR:  invalid alpha coefficient: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }

            case 'b': {
                char        *end;

                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no value provided for beta coefficient\n");
                    exit(EINVAL);
                }
                beta = strtod(optarg, &end);
                if ( end == NULL || end == optarg ) {
                    fprintf(stderr, "ERROR:  invalid beta coefficient: %s\n", optarg);
                    exit(EINVAL);
                }
                break;
            }

            case 's': {
                char        *end;
                long        v;
                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no random seed value specified\n");
                    exit(EINVAL);
                }
                v = strtol(optarg, &end, 0);
                if ( (v < 0 || v > RAND_MAX) || end == NULL || end == optarg ) {
                    fprintf(stderr, "ERROR:  invalid random seed: %s\n", optarg);
                    exit(EINVAL);
                }
                random_seed = v;
                break;
            }

            case 't': {
                char        *end;
                long        v;

#ifdef HAVE_OPENMP
                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no value provided for thread count\n");
                    exit(EINVAL);
                }
                v = strtol(optarg, &end, 0);
                if ( v < 0 || end == NULL || end == optarg ) {
                    fprintf(stderr, "ERROR:  invalid thread count: %s\n", optarg);
                    exit(EINVAL);
                }
                nthreads = v;
#else
                fprintf(stderr, "WARNING:  OpenMP arguments ignored\n");
#endif
                break;
            }

            case 'n': {
                char        *end;
                long        v;
                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no matrix dimension specified\n");
                    exit(EINVAL);
                }
                v = strtol(optarg, &end, 0);
                if ( (v <= 1) || end == NULL || end == optarg ) {
                    fprintf(stderr, "ERROR:  invalid matrix dimension: %s\n", optarg);
                    exit(EINVAL);
                }
                n = v;
                break;
            }

            case 'S': {
                char        *end;
                long        v;
                if ( !optarg || (*optarg == '\0') ) {
                    fprintf(stderr, "ERROR:  no alignment byte count specified\n");
                    exit(EINVAL);
                }
                v = strtol(optarg, &end, 0);
                if ( (v <= 0) || end == NULL || end == optarg ) {
                    fprintf(stderr, "ERROR:  invalid alignment byte count: %s\n", optarg);
                    exit(EINVAL);
                }
                allocAlign = v;
                break;
            }
        }
    }

    //
    // Summarize a few options:
    //
    printf("INFO:  Using matrix initialization method '%s'\n", MatrixInit_toString(matrixInit, matrixInitArg));
    printf("INFO:  Using matrix multiplication routines '%s'\n", MultiplyRoutine_toString(multRoutines));

    //
    // Seed the PRNG:
    //
    srandom(random_seed);

    //
    // Allocate matrices:
    //
    if (shouldAlign) {
        printf("INFO:  Allocating matrices with alignment of %d bytes\n", allocAlign);
        posix_memalign((void**)&A, allocAlign, n * n * sizeof(f_real));
        posix_memalign((void**)&B, allocAlign, n * n * sizeof(f_real));
        posix_memalign((void**)&C, allocAlign, n * n * sizeof(f_real));
    } else {
        size_t      offset;

        A = calloc(n * n + 1, sizeof(f_real));
        B = calloc(n * n + 1, sizeof(f_real));
        C = calloc(n * n + 1, sizeof(f_real));
        // Force onto an unaligned position:
        offset = ((unsigned long long int)A) % sizeof(f_real);
        if ( offset == 0 ) A = (f_real*)((void*)A + 3);
        offset = ((unsigned long long int)A) % sizeof(f_real);
        printf("INFO:  Allocated A matrix with offset alignment %d bytes\n", offset);

        offset = ((unsigned long long int)B) % sizeof(f_real);
        if ( offset == 0 ) B = (f_real*)((void*)B + 3);
        offset = ((unsigned long long int)B) % sizeof(f_real);
        printf("INFO:  Allocated B matrix with offset alignment %d bytes\n", offset);

        offset = ((unsigned long long int)C) % sizeof(f_real);
        if ( offset == 0 ) C = (f_real*)((void*)C + 3);
        offset = ((unsigned long long int)C) % sizeof(f_real);
        printf("INFO:  Allocated C matrix with offset alignment %d bytes\n", offset);
    }
    if ( !A || !B || !C ) {
        fprintf(stderr, "ERROR:  unable to allocate matrices\n");
        exit(1);
    }

    //
    // If no explicit nthreads has been specified, get it from the env:
    //
#ifdef HAVE_OPENMP
    if ( nthreads <= 0 ) nthreads = omp_get_max_threads();
    omp_set_num_threads(1);
#endif

    //
    // Do each selected routine:
    //
    printf("                                                                   %16s | %16s | %16s\n", "walltime ", "user cpu ", "system cpu ");
    r = kMultiplyRoutineMin;
    while ( r <= kMultiplyRoutineMax ) {
        if ( (multRoutines & r) != 0 ) {
            f_integer   loop = 0;
            timings_t   T, T_init_total = timings_init(), T_mult_total = timings_init();

            //
            // Do the routine nloop times:
            //
            while ( loop < nloop ) {
                //
                // Initialize the matrix:
                //
                switch (matrixInit) {
                    case kMatrixInitNoop: {
                        printf("INIT:     Noop matrix initialization:                              "); fflush(stdout);
                        TIMING_START();
                        TIMING_END(T);
                        break;
                    }
                    case kMatrixInitZero: {
                        printf("INIT:     Zero matrix initialization:                              "); fflush(stdout);
                        TIMING_START();
                        bzero(A, n * n * sizeof(f_real));
                        bzero(B, n * n * sizeof(f_real));
                        bzero(C, n * n * sizeof(f_real));
                        TIMING_END(T);
                        break;
                    }
                    case kMatrixInitSimple: {
                        f_integer       i, j;

                        printf("INIT:     Simple matrix initialization:                            "); fflush(stdout);
                        TIMING_START();
                        for ( i = 0; i < n; i ++ ) {
                            for ( j = 0; j < n; j++ ) {
                                A[i * n + j] = (double)(2 + i + j);
                                B[i * n + j] = (double)(i - j);
                                C[i * n + j] = 0.0;
                            }
                        }
                        TIMING_END(T);
                        break;
                    }
                    case kMatrixInitRandom: {
                        f_integer       i, j;
                        long int        rv;

                        printf("INIT:     Random matrix initialization:                            "); fflush(stdout);
                        TIMING_START();
                        for ( i = 0; i < n; i ++ ) {
                            for ( j = 0; j < n; j++ ) {
                                A[i * n + j] = (1.0 / (double)RAND_MAX) * (double)random();
                                B[i * n + j] = (1.0 / (double)RAND_MAX) * (double)random();
                                C[i * n + j] = 0.0;
                            }
                        }
                        TIMING_END(T);
                        break;
                    }
                    case kMatrixInitFromFile: {
                        f_integer       i, j;
                        int             fd, flags = 0;

#ifdef HAVE_DIRECTIO
                        if ( shouldUseDirectIO ) flags |= O_DIRECT | O_SYNC;
#endif

                        printf("INIT:     File-based matrix initialization:                        "); fflush(stdout);
                        TIMING_START();
                        fd = open(matrixInitArg, flags);
                        if ( fd <= 0 ) {
                            fprintf(stderr, "ERROR:  unable to open matrix initialization file: %s (errno = %d)\n", matrixInitArg, errno);
                            exit(errno);
                        }
                        // A matrix:
                        MatrixInit_fromFile(fd, matrixInitArg, "A", n, A);
                        // B matrix:
                        MatrixInit_fromFile(fd, matrixInitArg, "B", n, B);
                        // C matrix:
                        MatrixInit_fromFile(fd, matrixInitArg, "C", n, C);
                        close(fd);
                        TIMING_END(T);
                        break;
                    }
                }
                timings_add(&T_init_total, &T);
                printf("%16.9le | %16.9le | %16.9le\n", T.walltime, T.userCpu, T.systemCpu);

                //
                // Perform the multiplication:
                //
                switch (r) {
                    case kMultiplyRoutineBasic: {
                        printf("START:    Baseline n x n matrix multiply:                          "); fflush(stdout);
                        TIMING_START();
                        mat_mult_basic_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
                        TIMING_END(T);
                        break;
                    }
                    
                    case kMultiplyRoutineSmart: {
                        printf("START:    Smart n x n matrix multiply:                             "); fflush(stdout);
                        TIMING_START();
                        mat_mult_smart_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
                        TIMING_END(T);
                        break;
                    }

                    case kMultiplyRoutineOpt: {
                        printf("START:    Baseline (optimized) n x n matrix multiply:              "); fflush(stdout);
                        TIMING_START();
                        mat_mult_optimized_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
                        TIMING_END(T);
                        break;
                    }

                    case kMultiplyRoutineOpenMP: {
                        char        nthread_str[14];
#ifdef HAVE_OPENMP
                        snprintf(nthread_str, sizeof(nthread_str), "(nthreads=%d)", nthreads);
                        omp_set_num_threads(nthreads);
#else
                        snprintf(nthread_str, sizeof(nthread_str), "(disabled)");
#endif
                        printf("START:    OpenMP %13s n x n matrix multiply:              ", nthread_str); fflush(stdout);
                        TIMING_START();
                        mat_mult_openmp_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
                        TIMING_END(T);
#ifdef HAVE_OPENMP
                        omp_set_num_threads(1);
#endif
                        break;
                    }

                    case kMultiplyRoutineOpenMPOpt: {
                        char        nthread_str[14];
#ifdef HAVE_OPENMP
                        snprintf(nthread_str, sizeof(nthread_str), "(nthreads=%d)", nthreads);
                        omp_set_num_threads(nthreads);
#else
                        snprintf(nthread_str, sizeof(nthread_str), "(disabled)");
#endif
                        printf("START:    OpenMP (optimized) %13s n x n matrix multiply:  ", nthread_str); fflush(stdout);
                        TIMING_START();
                        mat_mult_openmp_optimized_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
                        TIMING_END(T);
#ifdef HAVE_OPENMP
                        omp_set_num_threads(1);
#endif
                        break;
                    }

                    case kMultiplyRoutineBLAS: {
                        printf("START:    BLAS n x n matrix multiply:                              "); fflush(stdout);
                        TIMING_START();
                        mat_mult_blas_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
                        TIMING_END(T);
                        break;
                    }
                }
                printf("%16.9le | %16.9le | %16.9le\n", T.walltime, T.userCpu, T.systemCpu);
                timings_add(&T_mult_total, &T);
                loop++;
            }
            if ( nloop > 1 ) {
                printf("AVG INIT:                                                          %16.9le | %16.9le | %16.9le\n", T_init_total.walltime / (double)nloop, T_init_total.userCpu / (double)nloop, T_init_total.systemCpu / (double)nloop);
                printf("AVG MULT:                                                          %16.9le | %16.9le | %16.9le\n", T_mult_total.walltime / (double)nloop, T_mult_total.userCpu / (double)nloop, T_mult_total.systemCpu / (double)nloop);
            }
            printf("\n");
        }
        r <<= 1;
    }

    return 0;
}
