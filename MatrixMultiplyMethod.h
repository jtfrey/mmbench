/*
 * MatrixMultiplyMethod.h
 *
 * Generalized interface to routines that multiply two square matrices.
 */

#ifndef __MATRIXMULTIPLYMETHOD_H__
#define __MATRIXMULTIPLYMETHOD_H__

#include "FortranInterface.h"
#include "ExecutionTimer.h"

#include <stdio.h>

/*!
 * @typedef MatrixMultiplyMethodAlloc
 *
 * Type of a function that allocates and initializes any state information
 * (outContext) associated with a MatrixMultiplyMethod.
 *
 * The function should return boolean true when successful, false otherwise.
 */
typedef bool (*MatrixMultiplyMethodAlloc)(const char *inArgs, const void* *outContext);
/*!
 * @typedef MatrixMultiplyMethodDealloc
 *
 * Type of a function that finalizes a MatrixMultiplyMethod and disposes of any
 * state (inContext) that was allocated by MatrixMultiplyMethodAlloc().
 */
typedef void (*MatrixMultiplyMethodDealloc)(const void *inContext);
/*!
 * @typedef MatrixMultiplyMethodMultiply
 *
 * Type of a function that multiplies two n-by-n dimensional matrices A and B, storing
 * the product in C according to:
 *
 *     alpha * A . B + beta * C => C
 *
 * The function must call ExecutionTimerStart()/ExecutionTimerStop()
 * on the timer argument around the critical code segment(s).  Any method with threaded
 * parallelism should limit parallelism to nthreads.
 *
 * The inContext is state storage allocated by the associated
 * MatrixMultiplyMethodAlloc() function.
 *
 * The function should return boolean true when successful, false otherwise.
 */
typedef bool (*MatrixMultiplyMethodMultiply)(const void *inContext, ExecutionTimerRef timer, int nthreads, f_integer n, f_real alpha, f_real *A, f_real *B, f_real beta, f_real *C);
/*!
 * @typedef MatrixMultiplyMethodCallbacks
 *
 * Data structure containing pointers to the functions that together
 * implement a MatrixMultiplyMethod.
 *
 * @field helpToken An optional C string that describes the method.  If NULL,
 *          then the name of the method will be used.
 * @field alloc The function used to allocate and initialize any state
 *          required by the method.  Set to NULL if nothing needs to
 *          be done.
 * @field dealloc The function used to finalize and deallocate any state
 *          required by the method.  Set to NULL if nothing needs to
 *          be done.
 * @field init The function used to multiply two n-by-n matrices
 */
typedef struct {
    const char                      *helpToken;
    MatrixMultiplyMethodAlloc       alloc;
    MatrixMultiplyMethodDealloc     dealloc;
    MatrixMultiplyMethodMultiply    multiply;
} MatrixMultiplyMethodCallbacks;

/*!
 * @function MatrixMultiplyMethodRegister
 *
 * Register a MatrixMultiplyMethod with the given name.
 *
 * Returns boolean true if successful.  False is returned if the given
 * name is already registered or a problem occurs while registering the
 * new set of callbacks.
 */
bool MatrixMultiplyMethodRegister(const char *name, MatrixMultiplyMethodCallbacks *callbacks);

/*!
 * @function MatrixMultiplyMethodUnregister
 *
 * Unregister the MatrixMultiplyMethod with the given name.
 */
void MatrixMultiplyMethodUnregister(const char *name);

/*!
 * @function MatrixMultiplyMethodPrintTokenList
 *
 * Print the list of method help tokens (comma-separated) to the given
 * file i/o stream.  No leading or training whitespace is displayed.
 */
void MatrixMultiplyMethodPrintTokenList(FILE *stream);

/*!
 * @function MatrixMultiplyMethodCopyTokenList
 *
 * Write the list of method help tokens (comma-separated) to the given
 * buffer.  The total number of characters written (even if it exceeds
 * bufferLen) is returned to allow callers to dynamically allocate a
 * buffer of appropriate length.
 */
size_t MatrixMultiplyMethodCopyTokenList(char *buffer, size_t bufferLen);

/*!
 * @function MatrixMultiplyMethodTokenList
 *
 * Returns a pointer to a buffer (allocated internally by this API)
 * that contains the token list as a C string.
 */
const char* MatrixMultiplyMethodTokenList(void);

/*!
 * @typedef MatrixMultiplyObjectRef
 *
 * The type of a reference to an instance of a MatrixMultiplyMethod.
 */
typedef struct MatrixMultiplyObject * MatrixMultiplyObjectRef;

/*!
 * @function MatrixMultiplyObjectCreate
 *
 * Create a new MatrixMultiplyMethod instance given the specification.
 */
MatrixMultiplyObjectRef MatrixMultiplyObjectCreate(const char *specification);

/*!
 * @function MatrixMultiplyObjectRetain
 *
 * Return a reference to a copy of matMulObj.
 */
MatrixMultiplyObjectRef MatrixMultiplyObjectRetain(MatrixMultiplyObjectRef matMulObj);

/*!
 * @function MatrixMultiplyObjectRelease
 *
 * Decrease the reference count of matMulObj, deallocating once it reaches
 * zero.
 */
void MatrixMultiplyObjectRelease(MatrixMultiplyObjectRef matMulObj);

/*!
 * @function MatrixMultiplyObjectGetName
 *
 * Returns the registration name of the MatrixMultiplyMethod associated with
 * initObj.
 */
const char* MatrixMultiplyObjectGetName(MatrixMultiplyObjectRef matMulObj);

/*!
 * @function MatrixMultiplyObjectMultiply
 *
 * Multiply using the matMulObj method the two n-by-n dimensional matrices A and B,
 * placing the product in C according to
 *
 *     alpha * A . B + beta * C => C
 *
 * Timing data will be collected into timer.
 *
 * Threaded methods should limit themselves to nthreads.
 *
 * Returns boolean true if successful.
 */
bool MatrixMultiplyObjectMultiply(MatrixMultiplyObjectRef matMulObj, ExecutionTimerRef timer, int nthreads, f_integer n, f_real alpha, f_real *A, f_real *B, f_real beta, f_real *C);

#endif /* __MATRIXMULTIPLYMETHOD_H__ */
