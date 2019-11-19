/*
 * MatrixMultiplyMethod.c
 *
 * Generalized interface to routines that multiply two square matrices.
 */

#include "MatrixMultiplyMethod.h"

#include <errno.h>

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

static bool __MatrixMultiplyMethodIsInitialized = false;
static bool __MatrixMultiplyMethodIsInitializing = false;
void __MatrixMultiplyMethodInitialize(void);

//

typedef struct MatrixMultiplyMethod {
    struct MatrixMultiplyMethod     *link;

    bool                            canBeUnregistered;
    const char                      *name;
    size_t                          nameLen;
    MatrixMultiplyMethodCallbacks   callbacks;
} MatrixMultiplyMethod_t;

static MatrixMultiplyMethod_t       *__MatrixMultiplyMethods = NULL;

//

MatrixMultiplyMethod_t*
__MatrixMultiplyMethodLookup(
    const char                  *name,
    size_t                      nameLen
)
{
    MatrixMultiplyMethod_t      *mp;

    if ( ! __MatrixMultiplyMethodIsInitialized ) __MatrixMultiplyMethodInitialize();

    mp = __MatrixMultiplyMethods;
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

MatrixMultiplyMethod_t*
__MatrixMultiplyMethodAlloc(
    const char              *name
)
{
    size_t                  mpSize = sizeof(MatrixMultiplyMethod_t);
    size_t                  nameLen = strlen(name);
    MatrixMultiplyMethod_t  *mp = NULL;

    if ( nameLen > 0 ) {
        mp = (MatrixMultiplyMethod_t*)malloc(mpSize + nameLen + 1);
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
__MatrixMultiplyMethodRegister(
    const char                      *name,
    MatrixMultiplyMethodCallbacks   *callbacks,
    bool                            canBeUnregistered
)
{
    MatrixMultiplyMethod_t          *mp = __MatrixMultiplyMethodLookup(name, strlen(name));

    if ( ! mp ) {
        mp = __MatrixMultiplyMethodAlloc(name);
        if ( mp ) {
            mp->callbacks = *callbacks;
            mp->canBeUnregistered = canBeUnregistered;
            mp->link = __MatrixMultiplyMethods;
            __MatrixMultiplyMethods = mp;
            return true;
        }
    }
    return false;
}

//

bool
MatrixMultiplyMethodRegister(
    const char                      *name,
    MatrixMultiplyMethodCallbacks   *callbacks
)
{
    return __MatrixMultiplyMethodRegister(name, callbacks, true);
}

//

void
MatrixMultiplyMethodUnregister(
    const char                  *name
)
{
    MatrixMultiplyMethod_t          *mp = __MatrixMultiplyMethods, *mpLast = NULL;

    while ( mp ) {
        if ( mp->canBeUnregistered && (strcasecmp(mp->name, name) == 0) ) {
            if ( mpLast ) {
                mpLast->link = mp->link;
            } else {
                __MatrixMultiplyMethods = mp->link;
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
MatrixMultiplyMethodPrintTokenList(
    FILE                    *stream
)
{
    MatrixMultiplyMethod_t  *mp;
    const char              *sep = "";

    if ( ! __MatrixMultiplyMethodIsInitialized ) __MatrixMultiplyMethodInitialize();

    mp = __MatrixMultiplyMethods;
    while ( mp ) {
        fprintf(stream, "%s%s", sep, mp->callbacks.helpToken ? mp->callbacks.helpToken : mp->name);
        mp = mp->link;
        sep = "|";
    }
}

//

size_t
MatrixMultiplyMethodCopyTokenList(
    char                    *buffer,
    size_t                  bufferLen
)
{
    MatrixMultiplyMethod_t  *mp;
    const char              *sep = "";
    size_t                  totalLen = 0;

    if ( ! __MatrixMultiplyMethodIsInitialized ) __MatrixMultiplyMethodInitialize();

    mp = __MatrixMultiplyMethods;

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
MatrixMultiplyMethodTokenList(void)
{
    static bool     tokenListInited = false;
    static char     *longerThanExpected = NULL;
    static char     expectedLength[256];

    if ( ! tokenListInited ) {
        size_t          actualLen = MatrixMultiplyMethodCopyTokenList(expectedLength, sizeof(expectedLength));

        if ( actualLen >= sizeof(expectedLength) ) {
            longerThanExpected = (char*)malloc(actualLen + 1);
            if ( ! longerThanExpected ) {
                fprintf(stderr, "ERROR:  failed to allocate storage for multiply method token list\n");
                exit(ENOMEM);
            }
            MatrixMultiplyMethodCopyTokenList(longerThanExpected, actualLen + 1);
        }
        tokenListInited = true;
    }
    if ( longerThanExpected ) return longerThanExpected;
    return (const char*)expectedLength;
}

//
////
//

typedef struct MatrixMultiplyObject {
    unsigned int            refCount;
    MatrixMultiplyMethod_t  *matMulMethod;
    const void              *context;
} MatrixMultiplyObject;

//

MatrixMultiplyObjectRef
MatrixMultiplyObjectCreate(
    const char          *specification
)
{
    MatrixMultiplyObject    *newObj = NULL;
    MatrixMultiplyMethod_t  *mp = __MatrixMultiplyMethodLookup(specification, strlen(specification));

    if ( mp ) {
        if ( (newObj = (MatrixMultiplyObject*)malloc(sizeof(MatrixMultiplyObject))) ) {
            bool        ok;

            newObj->refCount = 1;
            newObj->matMulMethod = mp;
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
    return (MatrixMultiplyObjectRef)newObj;
}

//

MatrixMultiplyObjectRef
MatrixMultiplyObjectRetain(
    MatrixMultiplyObjectRef matMulObj
)
{
    matMulObj->refCount++;
    return matMulObj;
}

//

void
MatrixMultiplyObjectRelease(
    MatrixMultiplyObjectRef matMulObj
)
{
    if ( --(matMulObj->refCount) == 0 ) {
        if ( matMulObj->matMulMethod->callbacks.dealloc ) {
            matMulObj->matMulMethod->callbacks.dealloc(matMulObj->context);
        }
        free((void*)matMulObj);
    }
}

//

const char*
MatrixMultiplyObjectGetName(
    MatrixMultiplyObjectRef matMulObj
)
{
    return matMulObj->matMulMethod->name;
}

//

bool
MatrixMultiplyObjectMultiply(
    MatrixMultiplyObjectRef matMulObj,
    ExecutionTimerRef       timer,
    int                     nthreads,
    f_integer               n,
    f_real                  alpha,
    f_real                  *A,
    f_real                  *B,
    f_real                  beta,
    f_real                  *C
)
{
    if ( matMulObj->matMulMethod->callbacks.multiply ) {
        return matMulObj->matMulMethod->callbacks.multiply(matMulObj->context, timer, nthreads, n, alpha, A, B, beta, C);
    }
    return false;
}

//
////
//

bool
__MatrixMultiplyMethodBasicMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
    f_integer           i, j, k;
    f_real              s;

    ExecutionTimerStart(timer);
    for ( i = 0; i < n; i++ ) {
        for ( j = 0; j < n; j ++ ) {
            s = F_ZERO;
            for ( k = 0; k < n; k++ ) s += A[i * n + k] * B[k * n + j];
            C[i * n + j] = s;
        }
    }
    ExecutionTimerStop(timer);
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodBasic = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodBasicMultiply
        };

//
////
//

bool
__MatrixMultiplyMethodBasicFortranMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
    ExecutionTimerStart(timer);
    mat_mult_basic_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
    ExecutionTimerStop(timer);
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodBasicFortran = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodBasicFortranMultiply
        };

//
////
//

bool
__MatrixMultiplyMethodOptFortranMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
    ExecutionTimerStart(timer);
    mat_mult_optimized_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
    ExecutionTimerStop(timer);
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodOptFortran = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodOptFortranMultiply
        };

//
////
//

bool
__MatrixMultiplyMethodSmartFortranMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
    ExecutionTimerStart(timer);
    mat_mult_smart_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
    ExecutionTimerStop(timer);
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodSmartFortran = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodSmartFortranMultiply
        };

//
////
//

bool
__MatrixMultiplyMethodBasicFortranOMPMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
#ifdef HAVE_OPENMP
    omp_set_num_threads(nthreads);
#endif /* HAVE_OPENMP */
    ExecutionTimerStart(timer);
    mat_mult_openmp_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
    ExecutionTimerStop(timer);
#ifdef HAVE_OPENMP
    omp_set_num_threads(1);
#endif /* HAVE_OPENMP */
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodBasicFortranOMP = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodBasicFortranOMPMultiply
        };

//
////
//

bool
__MatrixMultiplyMethodOptFortranOMPMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
#ifdef HAVE_OPENMP
    omp_set_num_threads(nthreads);
#endif /* HAVE_OPENMP */
    ExecutionTimerStart(timer);
    mat_mult_openmp_optimized_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
    ExecutionTimerStop(timer);
#ifdef HAVE_OPENMP
    omp_set_num_threads(1);
#endif /* HAVE_OPENMP */
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodOptFortranOMP = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodOptFortranOMPMultiply
        };

//
////
//

bool
__MatrixMultiplyMethodBLASFortranMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
#ifdef HAVE_OPENMP
    omp_set_num_threads(nthreads);
#endif /* HAVE_OPENMP */
    ExecutionTimerStart(timer);
    mat_mult_blas_(&n, &alpha, A, B, &beta, C, n, n, n, n, n, n);
    ExecutionTimerStop(timer);
#ifdef HAVE_OPENMP
    omp_set_num_threads(1);
#endif /* HAVE_OPENMP */
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodBLASFortran = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodBLASFortranMultiply
        };

//
////
//

bool
__MatrixMultiplyMethodBLASMultiply(
    const void          *inContext,
    ExecutionTimerRef   timer,
    int                 nthreads,
    f_integer           n,
    f_real              alpha,
    f_real              *A,
    f_real              *B,
    f_real              beta,
    f_real              *C
)
{
#ifdef HAVE_BLAS
#ifdef HAVE_OPENMP
    omp_set_num_threads(nthreads);
#endif /* HAVE_OPENMP */
    ExecutionTimerStart(timer);
#ifdef HAVE_FORTRAN_REAL8
    dgemm_("N", "N", &n, &n, &n, &alpha, A, &n, B, &n, &beta, C, &n, 1, 1, n, n, n, n, n, n);
#else
    sgemm_("N", "N", &n, &n, &n, &alpha, A, &n, B, &n, &beta, C, &n, 1, 1, n, n, n, n, n, n);
#endif /* HAVE_FORTRAN_REAL8 */
    ExecutionTimerStop(timer);
#ifdef HAVE_OPENMP
    omp_set_num_threads(1);
#endif /* HAVE_OPENMP */
#else /* HAVE_BLAS */
    printf("<<BLAS variant not implemented>>");
#endif /* HAVE_BLAS */
    return true;
}

MatrixMultiplyMethodCallbacks   __MatrixMultiplyMethodBLAS = {
            .helpToken = NULL,
            .alloc = NULL,
            .dealloc = NULL,
            .multiply = __MatrixMultiplyMethodBLASMultiply
        };

//
////
//

void
__MatrixMultiplyMethodInitialize(void)
{
    if ( __MatrixMultiplyMethodIsInitializing ) return;

    __MatrixMultiplyMethodIsInitializing = true;

    __MatrixMultiplyMethodRegister("blas-fortran", &__MatrixMultiplyMethodBLASFortran, false);
    __MatrixMultiplyMethodRegister("blas", &__MatrixMultiplyMethodBLAS, false);
    __MatrixMultiplyMethodRegister("opt-fortran-omp", &__MatrixMultiplyMethodOptFortranOMP, false);
    __MatrixMultiplyMethodRegister("basic-fortran-omp", &__MatrixMultiplyMethodBasicFortranOMP, false);
    __MatrixMultiplyMethodRegister("opt-fortran", &__MatrixMultiplyMethodOptFortran, false);
    __MatrixMultiplyMethodRegister("smart-fortran", &__MatrixMultiplyMethodSmartFortran, false);
    __MatrixMultiplyMethodRegister("basic-fortran", &__MatrixMultiplyMethodBasicFortran, false);
    __MatrixMultiplyMethodRegister("basic", &__MatrixMultiplyMethodBasic, false);

    __MatrixMultiplyMethodIsInitializing = false;
    __MatrixMultiplyMethodIsInitialized = true;
}

//
////
//

#ifdef MATRIXMULTIPLYMETHOD_UNIT_TEST

int
main()
{
    f_real                  A[1000 * 1000], B[1000 * 1000], C[1000 * 1000];
    f_integer               n = 1000, k;
    MatrixMultiplyObjectRef matmul = MatrixMultiplyObjectCreate("smart-fortran");
    ExecutionTimerRef       timer = ExecutionTimerCreate();

    MatrixMultiplyMethodPrintTokenList(stdout);
    fputc('\n', stdout);

    if ( matmul ) {
        printf("Doing matrix multiply, %s style...\n", MatrixMultiplyObjectGetName(matmul));
        for ( k = 0; k < 10; k ++ )
            MatrixMultiplyObjectMultiply(matmul, timer, n, F_ONE, A, B, F_ZERO, C);
        MatrixMultiplyObjectRelease(matmul);
        ExecutionTimerSummarizeToStream(timer, "matrix multiply", stdout);
    }
    return 0;
}

#endif /* MATRIXMULTIPLYMETHOD_UNIT_TEST */
