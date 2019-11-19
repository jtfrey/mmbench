/*
 * mmbench.c
 *
 * C driver for the matrix multiplication test suite.  Performs all
 * memory allocations, matrix initializations, and timing operations.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <getopt.h>
#include <errno.h>

#ifdef HAVE_OPENMP
#   include <omp.h>
#endif

#include "FortranInterface.h"
#include "MatrixInitMethod.h"
#include "MatrixMultiplyMethod.h"

//
// Various compile-time constants that act as default values for
// some of the CLI options:
//
#define DEFAULT_INIT_METHOD         "noop"
#define DEFAULT_MULTIPLY_METHODS    "basic,basic-fortran"
#define DEFAULT_OUTPUT_FORMAT       "table"
#define DEFAULT_MATRIX_DIMENSION    1000
#define DEFAULT_ALPHA               F_ONE
#define DEFAULT_BETA                F_ZERO
#define DEFAULT_ALLOC_ALIGNMENT     8
#define DEFAULT_NLOOP               4

//
// CLI options this program recognizes:
//
struct option cli_opts[] = {
        { "help",           no_argument,        NULL,           'h' },
        { "verbose",        no_argument,        NULL,           'v' },
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
        { "format",         required_argument,  NULL,           'f' },
        { NULL,             0,                  0,              0   }
    };

const char *cli_optstring = ""
#ifdef HAVE_OPENMP
    "t:"
#endif
    "hvAS:i:r:s:n:a:b:f:";

//
// Make verbosity a global:
//
unsigned int        verbosity = 0;

#define DEBUG(F, ...) if (verbosity >= 3) fprintf(stderr, "DEBUG(%s:%d)  " F "\n", __FILE__, __LINE__, ##__VA_ARGS__ )
#define INFO(F, ...) if (verbosity >= 2) fprintf(stderr, "INFO(%s:%d)  " F "\n", __FILE__, __LINE__, ##__VA_ARGS__ )
#define WARN(F, ...) if (verbosity >= 1) fprintf(stderr, "WARNING(%s:%d)  " F "\n", __FILE__, __LINE__, ##__VA_ARGS__ )
#define ERROR(F, ...) fprintf(stderr, "ERROR(%s:%d)  " F "\n", __FILE__, __LINE__, ##__VA_ARGS__ )

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
        "  -v/--verbose                         increase the amount of information displayed\n"
        "  -f/--format <format>                 output format for the timing data (default: %s)\n\n"
        "      <format> = (%s)\n\n"
#ifdef HAVE_OPENMP
        "  -t/--nthreads <integer>              OpenMP code should use this many threads max; zero\n"
        "                                       implies that the OpenMP runtime default should be used\n"
        "                                       (which possibly comes from e.g. OMP_NUM_THREADS)\n"
#endif
        "  -A/--no-align                        do not allocate aligned memory regions\n"
        "  -B/--align <integer>                 align allocated regions to this byte size\n"
        "                                       (default: "FMT_F_INTEGER")\n"
        "  -i/--init <init-method>              initialize matrices with this method\n"
        "                                       (default: %s)\n\n"
        "      <init-method> = (%s)\n\n"
        "  -r/--routines <routine-spec>         augment the list of routines to perform\n"
        "                                       (default: %s)\n\n"
        "      <routine-spec> = {+|-}(all|%s){,...}\n\n"
        "\n"
        " calculation performed is:\n\n"
        "      C = alpha * A . B + beta * C\n"
        "\n"
        "  -l/--nloop <integer>                 number of times to perform calculation for each\n"
        "                                       chosen routine; counts greater than 1 will show\n"
        "                                       averaged timings (default: "FMT_F_INTEGER")\n"
        "  -n/--dimension <integer>             dimension of the matrices (default: "FMT_F_INTEGER")\n"
        "  -a/--alpha <real>                    alpha value in equation (default: "FMT_F_REAL")\n"
        "  -b/--beta <real>                     beta value in equation (default: "FMT_F_REAL")\n"
        "\n",
        exe,
        DEFAULT_OUTPUT_FORMAT,
        ExecutionTimerOutputFormats(),
        (f_integer)DEFAULT_ALLOC_ALIGNMENT,
        DEFAULT_INIT_METHOD,
        MatrixInitMethodTokenList(),
        DEFAULT_MULTIPLY_METHODS,
        MatrixMultiplyMethodTokenList(),
        (f_integer)DEFAULT_NLOOP,
        (f_integer)DEFAULT_MATRIX_DIMENSION,
        (f_real)DEFAULT_ALPHA,
        (f_real)DEFAULT_BETA
      );
    exit(0);
}

//
// Multiply method list
//
typedef struct MultiplyMethodList {
    struct MultiplyMethodList   *links[2];
    size_t                      methodStrLen;
    char                        methodStr[];
} MultiplyMethodList;

MultiplyMethodList*
__MultiplyMethodListAlloc(
    const char      *name,
    size_t          nameLen
)
{
    MultiplyMethodList  *node = malloc(sizeof(MultiplyMethodList) + nameLen + 1);

    if ( node ) {
        node->links[0] = node->links[1] = NULL;
        strncpy(node->methodStr, name, nameLen);
        node->methodStr[nameLen] = '\0';
        node->methodStrLen = nameLen;
    }
    return node;
}

void
__MultiplyMethodListDealloc(
    MultiplyMethodList  *methodList
)
{
    while ( methodList ) {
        MultiplyMethodList  *next = methodList->links[0];
        free((void*)methodList);
        methodList = next;
    }
}

MultiplyMethodList*
__MultiplyMethodListFind(
    MultiplyMethodList  *methodList,
    const char          *name,
    size_t              nameLen
)
{
    while ( methodList ) {
        if ( (methodList->methodStrLen == nameLen) && (strncasecmp(methodList->methodStr, name, nameLen) == 0) ) return methodList;
        methodList = methodList->links[0];
    }
    return NULL;
}

void
MultiplyMethodListPrint(
    MultiplyMethodList  *methodList
)
{
    const char          *sep = "";

    while ( methodList ) {
        printf("%s%s", sep, methodList->methodStr);
        methodList = methodList->links[0];
        sep = ",";
    }
}

const char*
MultiplyMethodListGetString(
    MultiplyMethodList  *methodList
)
{
    static size_t       bufferLen = 0;
    static char         *buffer = NULL;
    static char         preallocBuffer[256];
    char                *s, *ss, *sep = "";
    size_t              actualLen = 0, slen;
    MultiplyMethodList  *list = methodList;

    while ( list ) {
        actualLen += ((actualLen != 0) ? 1 : 0) + list->methodStrLen;
        list = list->links[0];
    }
    if ( actualLen < sizeof(preallocBuffer) ) {
        s = preallocBuffer;
        slen = sizeof(preallocBuffer);
    } else if ( actualLen < bufferLen ) {
        s = buffer;
        slen = bufferLen;
    } else if ( buffer ) {
        buffer = realloc(buffer, actualLen + 1);
        if ( ! buffer ) {
            ERROR("failed to resize method list buffer");
            exit(ENOMEM);
        }
        s = buffer;
        bufferLen = slen = actualLen + 1;
    }
    list = methodList;
    ss = s;
    while ( list ) {
        int     n = snprintf(ss, slen, "%s%s", sep, list->methodStr);
        ss += n;
        slen -= n;
        sep = ",";
        list = list->links[0];
    }
    return (const char*)s;
}

MultiplyMethodList*
MultiplyMethodListIter(
    MultiplyMethodList  *iterMethodList,
    const char*         *methodStr,
    size_t              *methodStrLen
)
{
    if ( iterMethodList ) {
        *methodStr = iterMethodList->methodStr;
        *methodStrLen = iterMethodList->methodStrLen;
        iterMethodList = iterMethodList->links[0];
    }
    return iterMethodList;
}

void
MultiplyMethodListParse(
    MultiplyMethodList  **methodList,
    const char          *nameList
)
{
    MultiplyMethodList  *list = *methodList;

    // Leading '=' means to drop all methods selected thus far:
    if ( *nameList == '=' ) {
        __MultiplyMethodListDealloc(list);
        list = NULL;
        nameList++;
    }
    while ( *nameList ) {
        bool            shouldRemove = false;
        const char      *endPtr;

        if ( *nameList == '-' ) {
            nameList++;
            shouldRemove = true;
        } else if ( *nameList == '+' ) {
            nameList++;
        }
        // Find end-of-string or next comma:
        endPtr = strchr(nameList, ',');
        if ( ! endPtr ) endPtr = nameList + strlen(nameList);

        // All?
        if ( ((endPtr - nameList) == 3) && (strncasecmp(nameList, "all", 3) == 0) ) {
            // Scrub the list:
            __MultiplyMethodListDealloc(list);
            list = NULL;

            // If it was add, then put 'em all back:
            if ( ! shouldRemove ) {
                size_t      allMethodsLen = strlen(MatrixMultiplyMethodTokenList()) + 1;
                char        allMethods[allMethodsLen], *p;

                MatrixMultiplyMethodCopyTokenList(allMethods, allMethodsLen);
                p = allMethods;
                while ( *p ) {
                    if ( *p == '|' ) *p = ',';
                    p++;
                }
                MultiplyMethodListParse(&list, allMethods);
            }
        } else {
            //
            // Check for this method in the list:
            //
            MultiplyMethodList*     foundNode = __MultiplyMethodListFind(list, nameList, (endPtr - nameList));

            if ( foundNode ) {
                if ( shouldRemove ) {
                    // Is it the list head?
                    if ( foundNode == list ) list = foundNode->links[0];
                    // Forward link?
                    if ( foundNode->links[0] ) {
                        foundNode->links[0]->links[1] = foundNode->links[1];
                    }
                    // Back link?
                    if ( foundNode->links[1] ) {
                        foundNode->links[1]->links[0] = foundNode->links[0];
                    }
                    free((void*)foundNode);
                }
            } else if ( ! shouldRemove ) {
                foundNode = __MultiplyMethodListAlloc(nameList, (endPtr - nameList));
                if ( foundNode ) {
                    MultiplyMethodList  *lastNode = list;

                    while ( lastNode && lastNode->links[0] ) lastNode = lastNode->links[0];
                    if ( lastNode ) {
                        lastNode->links[0] = foundNode;
                        foundNode->links[1] = lastNode;
                    } else {
                        list = foundNode;
                    }
                } else {
                    ERROR("unable to allocate memory while parsing method list");
                    exit(ENOMEM);
                }
            }
        }
        nameList = endPtr;
        if ( *nameList == ',' ) nameList++;
    }
    *methodList = list;
}

void
MultiplyMethodListDestroy(
    MultiplyMethodList  **list
)
{
    __MultiplyMethodListDealloc(*list);
    *list = NULL;
}

//
// Main program.
//
int
main(
    int                 argc,
    char* const         argv[]
)
{
    const char                  *exe = argv[0];
    f_integer                   n = DEFAULT_MATRIX_DIMENSION, nloop = DEFAULT_NLOOP, loop;
#ifdef HAVE_OPENMP
    int                         nthreads = 0;
#endif
    f_real                      *A=NULL, *B=NULL, *C=NULL, alpha = DEFAULT_ALPHA, beta = DEFAULT_BETA;
    const char                  *initMethod = DEFAULT_INIT_METHOD;
    MatrixInitObjectRef         matrixInitMethod = NULL;
    MultiplyMethodList          *multiplyMethods = NULL, *iterMultiplyMethods;
    size_t                      allocAlign = DEFAULT_ALLOC_ALIGNMENT;
    bool                        shouldAlign = true;
    ExecutionTimerRef           matInitTimer = ExecutionTimerCreate();
    ExecutionTimerRef           matMulTimer = ExecutionTimerCreate();
    ExecutionTimerOutputFormat  timerOutputFormat;

    int                         optc;

    MultiplyMethodListParse(&multiplyMethods, DEFAULT_MULTIPLY_METHODS);
    timerOutputFormat = ExecutionTimerOutputFormatParse(DEFAULT_OUTPUT_FORMAT);

    //
    // Process CLI arguments:
    //
    while ( (optc = getopt_long(argc, argv, cli_optstring, cli_opts, NULL)) != -1 ) {
        switch (optc) {
            case 'h': {
                usage(exe);
                break;
            }

            case 'v': {
                verbosity++;
                break;
            }

            case 'A': {
                shouldAlign = false;
                break;
            }

            case 'i': {
                if ( !optarg || (*optarg == '\0') ) {
                    ERROR("ERROR:  no matrix init specification provided");
                    exit(EINVAL);
                }
                initMethod = optarg;
                break;
            }

            case 'r': {
                if ( optarg && *optarg ) MultiplyMethodListParse(&multiplyMethods, optarg);
                break;
            }

            case 'a': {
                char        *end;

                if ( !optarg || (*optarg == '\0') ) {
                    ERROR("no value provided for alpha coefficient");
                    exit(EINVAL);
                }
                alpha = strtod(optarg, &end);
                if ( end == NULL || end == optarg ) {
                    ERROR("invalid alpha coefficient: %s", optarg);
                    exit(EINVAL);
                }
                break;
            }

            case 'b': {
                char        *end;

                if ( !optarg || (*optarg == '\0') ) {
                    ERROR("no value provided for beta coefficient");
                    exit(EINVAL);
                }
                beta = strtod(optarg, &end);
                if ( end == NULL || end == optarg ) {
                    ERROR("invalid beta coefficient: %s", optarg);
                    exit(EINVAL);
                }
                break;
            }

            case 't': {
                char        *end;
                long        v;

#ifdef HAVE_OPENMP
                if ( !optarg || (*optarg == '\0') ) {
                    ERROR("no value provided for thread count");
                    exit(EINVAL);
                }
                v = strtol(optarg, &end, 0);
                if ( v < 0 || end == NULL || end == optarg ) {
                    ERROR("invalid thread count: %s", optarg);
                    exit(EINVAL);
                }
                nthreads = v;
#else
                WARN("OpenMP arguments ignored");
#endif
                break;
            }

            case 'n': {
                char        *end;
                long        v;
                if ( !optarg || (*optarg == '\0') ) {
                    ERROR("no matrix dimension specified");
                    exit(EINVAL);
                }
                v = strtol(optarg, &end, 0);
                if ( (v <= 1) || end == NULL || end == optarg ) {
                    ERROR("invalid matrix dimension: %s", optarg);
                    exit(EINVAL);
                }
                n = v;
                break;
            }

            case 'S': {
                char        *end;
                long        v;
                if ( !optarg || (*optarg == '\0') ) {
                    ERROR("no alignment byte count specified");
                    exit(EINVAL);
                }
                v = strtol(optarg, &end, 0);
                if ( (v <= 0) || end == NULL || end == optarg ) {
                    ERROR("invalid alignment byte count: %s", optarg);
                    exit(EINVAL);
                }
                allocAlign = v;
                break;
            }

            case 'f': {
                ExecutionTimerOutputFormat  newFormat = ExecutionTimerOutputFormatParse(optarg);

                if ( newFormat == ExecutionTimerOutputFormatInvalid ) {
                    WARN("invalid timer output format specified: %s", optarg);
                } else {
                    timerOutputFormat = newFormat;
                }
                break;
            }
        }
    }

    INFO("Initialization method requested: %s", initMethod);
    matrixInitMethod = MatrixInitObjectCreate(initMethod);
    if ( ! matrixInitMethod ) {
        ERROR("unable to allocate matrix initializer: %s", initMethod);
        exit(EINVAL);
    }
    INFO("Multiplication methods requested: %s", MultiplyMethodListGetString(multiplyMethods));
    INFO("Matrix dimension: " FMT_F_INTEGER, n);
    INFO("Number of loop iterations per method: " FMT_F_INTEGER, nloop);
    INFO("Timing output format: %s", ExecutionTimerOutputFormatToString(timerOutputFormat));

    //
    // Allocate matrices:
    //
    if (shouldAlign) {
        INFO("Allocating matrices with alignment of %d bytes", allocAlign);
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
        INFO("Allocated A matrix with offset alignment %d bytes", offset);

        offset = ((unsigned long long int)B) % sizeof(f_real);
        if ( offset == 0 ) B = (f_real*)((void*)B + 3);
        offset = ((unsigned long long int)B) % sizeof(f_real);
        INFO("Allocated B matrix with offset alignment %d bytes", offset);

        offset = ((unsigned long long int)C) % sizeof(f_real);
        if ( offset == 0 ) C = (f_real*)((void*)C + 3);
        offset = ((unsigned long long int)C) % sizeof(f_real);
        INFO("Allocated C matrix with offset alignment %d bytes", offset);
    }
    if ( !A || !B || !C ) {
        ERROR("unable to allocate matrices");
        exit(1);
    }

    //
    // If no explicit nthreads has been specified, get it from the env:
    //
#ifdef HAVE_OPENMP
    if ( nthreads <= 0 ) nthreads = omp_get_max_threads();
    omp_set_num_threads(1);
    INFO("Threaded routines will use %d thread(s)", nthreads);
#endif

    //
    // Loop over the list of methods:
    //
    iterMultiplyMethods = multiplyMethods;
    while ( iterMultiplyMethods ) {
        const char              *methodStr;
        size_t                  methodStrLen;
        MatrixMultiplyObjectRef multMethod;

        iterMultiplyMethods = MultiplyMethodListIter(iterMultiplyMethods, &methodStr, &methodStrLen);
        if ( (multMethod = MatrixMultiplyObjectCreate(methodStr)) ) {
            printf("Starting test of methods: %s, %s\n\n", MatrixInitObjectGetName(matrixInitMethod), MatrixMultiplyObjectGetName(multMethod));
            ExecutionTimerReset(matMulTimer);
            for ( loop = 0; loop < nloop; loop++ ) {
                if ( ! MatrixInitObjectInit(matrixInitMethod, matInitTimer, nthreads, n, A) ||
                     ! MatrixInitObjectInit(matrixInitMethod, matInitTimer, nthreads, n, B) ||
                     ! MatrixInitObjectInit(matrixInitMethod, matInitTimer, nthreads, n, C)
                ) {
                    ERROR("failure in iteration %ld of %s init method", (long)loop, initMethod);
                    exit(1);
                }
                if ( ! MatrixMultiplyObjectMultiply(multMethod, matMulTimer, nthreads, n, alpha, A, B, beta, C) ) {
                    ERROR("failure in iteration %ld of %s multiplication method", (long)loop, methodStr);
                    exit(1);
                }
            }
            ExecutionTimerSummarizeToStream(matMulTimer, timerOutputFormat, MatrixMultiplyObjectGetName(multMethod), stdout);
            MatrixMultiplyObjectRelease(multMethod);
            printf("\n\n");
        } else {
            ERROR("no such multiplication method: %s", methodStr);
            exit(EINVAL);
        }
    }

    printf("Matrix initialization timing results:\n\n");
    ExecutionTimerSummarizeToStream(matInitTimer, timerOutputFormat, MatrixInitObjectGetName(matrixInitMethod), stdout);
    printf("\n\n");

    MultiplyMethodListDestroy(&multiplyMethods);
    MatrixInitObjectRelease(matrixInitMethod);

    return 0;
}
