/*
 * MatrixInitMethod.c
 *
 * Generalized interface to routines that initialize a matrix.
 */

#ifdef HAVE_DIRECTIO
#define _GNU_SOURCE
#endif

#include "MatrixInitMethod.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

//

static bool __MatrixInitMethodIsInitialized = false;
static bool __MatrixInitMethodIsInitializing = false;
void __MatrixInitMethodInitialize(void);

//

typedef struct MatrixInitMethod {
    struct MatrixInitMethod     *link;
    bool                        canBeUnregistered;
    const char                  *name;
    size_t                      nameLen;
    MatrixInitMethodCallbacks   callbacks;
} MatrixInitMethod_t;

static MatrixInitMethod_t       *__matrixInitMethods = NULL;

//

MatrixInitMethod_t*
__MatrixInitMethodLookup(
    const char                  *name,
    size_t                      nameLen
)
{
    MatrixInitMethod_t          *mp;

    if ( ! __MatrixInitMethodIsInitialized ) __MatrixInitMethodInitialize();

    mp = __matrixInitMethods;
    if ( nameLen < 1 ) nameLen = strlen(name);

    while ( mp ) {
        if ( nameLen >= mp->nameLen ) {
            if ( strncasecmp(mp->name, name, mp->nameLen) == 0 ) {
                // Leading portion of "name" matches this method; check nameLen + 1
                // to see if it's a full match:
                if ( name[mp->nameLen] == '\0' ) break;
                if ( name[mp->nameLen] == '=' ) break;
            }
        }
        mp = mp->link;
    }
    return mp;
}

//

MatrixInitMethod_t*
__MatrixInitMethodAlloc(
    const char              *name
)
{
    size_t                  mpSize = sizeof(MatrixInitMethod_t);
    size_t                  nameLen = strlen(name);
    MatrixInitMethod_t      *mp = NULL;

    if ( nameLen > 0 ) {
        mp = (MatrixInitMethod_t*)malloc(mpSize + nameLen + 1);
        if ( mp ) {
            mp->canBeUnregistered = true;
            mp->name = (void*)mp + mpSize;
            mp->nameLen = nameLen;
            strncpy((char*)mp->name, name, mp->nameLen + 1);
        }
    }
    return mp;
}

//

bool
__MatrixInitMethodRegister(
    const char                  *name,
    MatrixInitMethodCallbacks   *callbacks,
    bool                        canBeUnregistered
)
{
    MatrixInitMethod_t          *mp = __MatrixInitMethodLookup(name, strlen(name));

    if ( ! mp ) {
        mp = __MatrixInitMethodAlloc(name);
        if ( mp ) {
            mp->canBeUnregistered = canBeUnregistered;
            mp->callbacks = *callbacks;
            mp->link = __matrixInitMethods;
            __matrixInitMethods = mp;
            return true;
        }
    }
    return false;
}

//

bool
MatrixInitMethodRegister(
    const char                  *name,
    MatrixInitMethodCallbacks   *callbacks
)
{
    return __MatrixInitMethodRegister(name, callbacks, false);
}

//

void
MatrixInitMethodUnregister(
    const char                  *name
)
{
    MatrixInitMethod_t          *mp = __matrixInitMethods, *mpLast = NULL;

    while ( mp ) {
        if ( mp->canBeUnregistered && (strcasecmp(mp->name, name) == 0) ) {
            if ( mpLast ) {
                mpLast->link = mp->link;
            } else {
                __matrixInitMethods = mp->link;
            }
            free((void*)mp);
            break;
        }
        mpLast = mp;
        mp = mp->link;
    }
}

//

void
MatrixInitMethodPrintTokenList(
    FILE                *stream
)
{
    MatrixInitMethod_t  *mp;
    const char          *sep = "";

    if ( ! __MatrixInitMethodIsInitialized ) __MatrixInitMethodInitialize();

    mp = __matrixInitMethods;
    fputc('(', stream);
    while ( mp ) {
        fprintf(stream, "%s%s", sep, mp->callbacks.helpToken ? mp->callbacks.helpToken : mp->name);
        mp = mp->link;
        sep = "|";
    }
    fputc(')', stream);
}

//

size_t
MatrixInitMethodCopyTokenList(
    char                    *buffer,
    size_t                  bufferLen
)
{
    MatrixInitMethod_t      *mp;
    const char              *sep = "";
    size_t                  totalLen = 0;

    if ( ! __MatrixInitMethodIsInitialized ) __MatrixInitMethodInitialize();

    mp = __matrixInitMethods;
    while ( mp ) {
        int                 actualLen;

        if ( bufferLen > 0 ) {
            actualLen = snprintf(buffer, bufferLen, "%s%s", sep, mp->callbacks.helpToken ? mp->callbacks.helpToken : mp->name);
        } else {
            actualLen = snprintf(NULL, 0, "%s%s", sep, mp->callbacks.helpToken ? mp->callbacks.helpToken : mp->name);
        }
        buffer += actualLen;
        bufferLen -= actualLen;
        totalLen += actualLen;
        sep = "|";
        mp = mp->link;
    }
    return totalLen;
}

//

const char*
MatrixInitMethodTokenList(void)
{
    static bool     tokenListInited = false;
    static char     *longerThanExpected = NULL;
    static char     expectedLength[128];

    if ( ! tokenListInited ) {
        size_t          actualLen = MatrixInitMethodCopyTokenList(expectedLength, sizeof(expectedLength));

        if ( actualLen >= sizeof(expectedLength) ) {
            longerThanExpected = (char*)malloc(actualLen + 1);
            if ( ! longerThanExpected ) {
                fprintf(stderr, "ERROR:  failed to allocate storage for init method token list\n");
                exit(ENOMEM);
            }
            MatrixInitMethodCopyTokenList(longerThanExpected, actualLen + 1);
        }
        tokenListInited = true;
    }
    if ( longerThanExpected ) return longerThanExpected;
    return (const char*)expectedLength;
}

//
////
//

typedef struct MatrixInitObject {
    unsigned int        refCount;
    MatrixInitMethod_t  *initMethod;
    const void          *context;
} MatrixInitObject;

//

MatrixInitObjectRef
MatrixInitObjectCreate(
    const char          *specification
)
{
    MatrixInitObject    *newObj = NULL;
    MatrixInitMethod_t  *mp = __MatrixInitMethodLookup(specification, strlen(specification));

    if ( mp ) {
        if ( (newObj = (MatrixInitObject*)malloc(sizeof(MatrixInitObject))) ) {
            bool        ok;

            newObj->refCount = 1;
            newObj->initMethod = mp;
            newObj->context = NULL;
            if ( mp->callbacks.alloc ) {
                const char  *args = strchr(specification, '=');
                if ( args ) {
                    ok = mp->callbacks.alloc(args + 1, &newObj->context);
                } else {
                    ok = mp->callbacks.alloc("", &newObj->context);
                }
                if ( ! ok ) {
                    free((void*)newObj);
                    newObj = NULL;
                }
            }
        }
    }
    return (MatrixInitObjectRef)newObj;
}

//

MatrixInitObjectRef
MatrixInitObjectRetain(
    MatrixInitObjectRef initObj
)
{
    initObj->refCount++;
    return initObj;
}

//

void
MatrixInitObjectRelease(
    MatrixInitObjectRef initObj
)
{
    if ( --(initObj->refCount) == 0 ) {
        if ( initObj->initMethod->callbacks.dealloc ) {
            initObj->initMethod->callbacks.dealloc(initObj->context);
        }
        free((void*)initObj);
    }
}

//

const char*
MatrixInitObjectGetName(
    MatrixInitObjectRef initObj
)
{
    return initObj->initMethod->name;
}

//

bool
MatrixInitObjectInit(
    MatrixInitObjectRef initObj,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              *M
)
{
    if ( initObj->initMethod->callbacks.init ) {
        return initObj->initMethod->callbacks.init(initObj->context, timer, nthreads, n, M);
    }
    return false;
}

//
////
//

bool
__MatrixInitMethodNoopInit(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              *M
)
{
    ExecutionTimerStart(timer);
    ExecutionTimerStop(timer);
    return true;
}

MatrixInitMethodCallbacks   __MatrixInitMethodNoop = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .init = __MatrixInitMethodNoopInit
        };

//
////
//

bool
__MatrixInitMethodZeroInit(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              *M
)
{
    f_integer           i, j;

    ExecutionTimerStart(timer);
    memset(M, 0, n * n * sizeof(f_real));
    ExecutionTimerStop(timer);
    return true;
}

MatrixInitMethodCallbacks   __MatrixInitMethodZero = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .init = __MatrixInitMethodZeroInit
        };

//
////
//

bool
__MatrixInitMethodSimpleInit(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              *M
)
{
    f_integer           i, j;

    ExecutionTimerStart(timer);
    for ( i = 0; i < n; i++ )
        for ( j = 0; j < n; j++ )
            M[i * n + j] = i * i + 2 * i * j + j * j;
    ExecutionTimerStop(timer);
    return true;
}

MatrixInitMethodCallbacks   __MatrixInitMethodSimple = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .init = __MatrixInitMethodSimpleInit
        };

//
////
//

#ifdef HAVE_OPENMP

#include <omp.h>

bool
__MatrixInitMethodSimpleOMPInit(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              *M
)
{
    f_integer           i, j;

    omp_set_num_threads(nthreads);
    ExecutionTimerStart(timer);
    #pragma omp parallel private(i,j) shared(M)
    #pragma omp for
    for ( i = 0; i < n; i++ ) {
        for ( j = 0; j < n; j++ ) {
            M[i * n + j] = i * i + 2 * i * j + j * j;
        }
    }
    ExecutionTimerStop(timer);
    omp_set_num_threads(1);
    return true;
}

MatrixInitMethodCallbacks   __MatrixInitMethodSimpleOMP = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .init = __MatrixInitMethodSimpleOMPInit
        };

#endif /* HAVE_OPENMP */

//
////
//

bool
__MatrixInitMethodRandomAlloc(
    const char  *inArgs,
    const void* *outContext
)
{
    unsigned int    randomSeed;

    if ( inArgs ) {
        randomSeed = atol(inArgs);
    }
    srandom(randomSeed);
    return true;
}

//

bool
__MatrixInitMethodRandomInit(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              *M
)
{
    f_integer           i, j;

    ExecutionTimerStart(timer);
    for ( i = 0; i < n; i++ )
        for ( j = 0; j < n; j++ )
            M[i * n + j] = (F_ONE / (f_real)RAND_MAX) * (f_real)random();
    ExecutionTimerStop(timer);
    return true;
}

MatrixInitMethodCallbacks   __MatrixInitMethodRandom = {
            .helpToken = "random{=###}",
            .alloc = __MatrixInitMethodRandomAlloc,
            .dealloc = NULL,
            .init = __MatrixInitMethodRandomInit
        };

//
////
//

typedef struct {
    int         fd;
} MatrixInitMethodFileContext;

//

bool
__MatrixInitMethodFileAlloc(
    const char  *inArgs,
    const void* *outContext
)
{
    int         oflags = 0;
    const char  *args = inArgs;

    while ( 1 ) {
        if ( strncasecmp(args, "sync,", 5) == 0 ) {
            oflags |= O_SYNC;
            args += 5;
        }
        else if ( strncasecmp(args, "noatime,", 8) == 0 ) {
            oflags |= O_NOATIME;
            args += 8;
        }
#ifdef HAVE_DIRECTIO
        else if ( strncasecmp(args, "direct,", 7) == 0 ) {
            oflags |= O_DIRECT;
            args += 7;
        }
#endif /* HAVE_DIRECTIO */
        else break;
    }
    if ( *args ) {
        int         fd;
        char        *colon = strchr(args, ':');

        if ( colon ) args = colon + 1;
        if ( (fd = open(args, oflags)) >= 0 ) {
            MatrixInitMethodFileContext     *context = malloc(sizeof(MatrixInitMethodFileContext));

            if ( context ) {
                context->fd = fd;
                *outContext = context;
                return true;
            }
            close(fd);
        } else {
            fprintf(stderr, "ERROR:  could not open matrix init file %s (flags = 0x%x, errno = %d)\n", args, oflags, errno);
        }
    }
    return false;
}

//

void
__MatrixInitMethodFileDealloc(
    const void *inContext
)
{
    MatrixInitMethodFileContext *CONTEXT = (MatrixInitMethodFileContext*)inContext;

    close(CONTEXT->fd);
    free((void*)inContext);
}

//

bool
__MatrixInitMethodFileInit(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              *M
)
{
    MatrixInitMethodFileContext *CONTEXT = (MatrixInitMethodFileContext*)inContext;

    f_integer           i, j;

    ExecutionTimerStart(timer);
    for ( i = 0; i < n; i++ ) {
        for ( j = 0; j < n; j++ ) {
            ssize_t     actual = read(CONTEXT->fd, &M[i * n + j], sizeof(f_real));

            if ( actual < sizeof(f_real) ) {
                if ( lseek(CONTEXT->fd, 0, SEEK_SET) != 0 ) {
                    fprintf(stderr, "ERROR:  unable to rewind matrix initialization file (errno = %d)\n", errno);
                    return false;
                }
                actual = read(CONTEXT->fd, &M[i * n + j], sizeof(f_real));
            }
            if ( actual != sizeof(f_real) ) {
                fprintf(stderr, "ERROR:  unable to read from matrix initialization file (errno = %d)\n", errno);
                return false;
            }
        }
    }
    ExecutionTimerStop(timer);
    return true;
}

MatrixInitMethodCallbacks   __MatrixInitMethodFile = {
            .helpToken = "file={opt{,..}:}<name>",
            .alloc = __MatrixInitMethodFileAlloc,
            .dealloc = __MatrixInitMethodFileDealloc,
            .init = __MatrixInitMethodFileInit
        };

//
////
//

void
__MatrixInitMethodInitialize(void)
{
    if ( __MatrixInitMethodIsInitializing ) return;

    __MatrixInitMethodIsInitializing = true;

    __MatrixInitMethodRegister("file", &__MatrixInitMethodFile, false);
    __MatrixInitMethodRegister("random", &__MatrixInitMethodRandom, false);
#ifdef HAVE_OPENMP
    __MatrixInitMethodRegister("simple-omp", &__MatrixInitMethodSimpleOMP, false);
#endif /* HAVE_OPENMP */
    __MatrixInitMethodRegister("simple", &__MatrixInitMethodSimple, false);
    __MatrixInitMethodRegister("zero", &__MatrixInitMethodZero, false);
    __MatrixInitMethodRegister("noop", &__MatrixInitMethodNoop, false);

    __MatrixInitMethodIsInitializing = false;
    __MatrixInitMethodIsInitialized = true;
}

//
////
//

#ifdef MATRIXINITMETHOD_UNIT_TEST

int
main()
{
    f_real              M[10000 * 10000];
    f_integer           n = 10000, k;
    MatrixInitObjectRef init = MatrixInitObjectCreate("simple");
    ExecutionTimerRef   timer = ExecutionTimerCreate();

    MatrixInitMethodPrintTokenList(stdout);
    fputc('\n', stdout);

    if ( init ) {
        printf("Doing matrix init, %s style...\n", MatrixInitObjectGetName(init));
        for ( k = 0; k < 100; k ++ )
            MatrixInitObjectInit(init, timer, n, M);
        MatrixInitObjectRelease(init);
        ExecutionTimerSummarizeToStream(timer, "matrix init", stdout);
    }
    return 0;
}

#endif /* MATRIXINITMETHOD_UNIT_TEST */
