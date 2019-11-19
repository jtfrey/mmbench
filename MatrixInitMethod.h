/*
 * MatrixInitMethod.h
 *
 * Generalized interface to routines that initialize a matrix.
 */

#ifndef __MATRIXINITMETHOD_H__
#define __MATRIXINITMETHOD_H__

#include "FortranInterface.h"
#include "ExecutionTimer.h"

#include <stdio.h>

/*!
 * @typedef MatrixInitMethodAlloc
 *
 * Type of a function that allocates and initializes any state information
 * (outContext) associated with a MatrixInitMethod.  Any additional information
 * that was presented with the method name is passed as inArgs.  E.g. for
 * "file=direct,noatime:/tmp/mats" inArgs would be "direct,noatime:/tmp/mats".
 *
 * The function should return boolean true when successful, false otherwise.
 */
typedef bool (*MatrixInitMethodAlloc)(const char *inArgs, const void* *outContext);
/*!
 * @typedef MatrixInitMethodDealloc
 *
 * Type of a function that finalizes a MatrixInitMethod and disposes of any
 * state (inContext) that was allocated by MatrixInitMethodAlloc().
 */
typedef void (*MatrixInitMethodDealloc)(const void *inContext);
/*!
 * @typedef MatrixInitMethodInit
 *
 * Type of a function that initializes an n-by-n dimensional matrix M.  The
 * function must call ExecutionTimerStart()/ExecutionTimerStop() on the timer
 * argument around the critical code segment(s).  Any method with threaded
 * parallelism should limit parallelism to nthreads.
 *
 * The inContext is state storage allocated by the associated
 * MatrixInitMethodAlloc() function.
 *
 * The function should return boolean true when successful, false otherwise.
 */
typedef bool (*MatrixInitMethodInit)(const void *inContext, ExecutionTimerRef timer, int nthreads, f_integer n, f_real *M);
/*!
 * @typedef MatrixInitMethodCallbacks
 *
 * Data structure containing pointers to the functions that together
 * implement a MatrixInitMethod.
 *
 * @field helpToken An optional C string that describes the format of the
 *          method's argument list (e.g. "file={opt{,..}:}<path>"
 *          for the "file" method).  If NULL, then the name of
 *          the method will be used.
 * @field alloc The function used to allocate and initialize any state
 *          required by the method.  Set to NULL if nothing needs to
 *          be done.
 * @field dealloc The function used to finalize and deallocate any state
 *          required by the method.  Set to NULL if nothing needs to
 *          be done.
 * @field init The function used to initialize an n-by-n matrix
 */
typedef struct {
    const char                  *helpToken;
    MatrixInitMethodAlloc       alloc;
    MatrixInitMethodDealloc     dealloc;
    MatrixInitMethodInit        init;
} MatrixInitMethodCallbacks;

/*!
 * @function MatrixInitMethodRegister
 *
 * Register a MatrixInitMethod with the given name.
 *
 * Returns boolean true if successful.  False is returned if the given
 * name is already registered or a problem occurs while registering the
 * new set of callbacks.
 */
bool MatrixInitMethodRegister(const char *name, MatrixInitMethodCallbacks *callbacks);

/*!
 * @function MatrixInitMethodUnregister
 *
 * Unregister the MatrixInitMethod with the given name.
 */
void MatrixInitMethodUnregister(const char *name);

/*!
 * @function MatrixInitMethodPrintTokenList
 *
 * Print the list of method help tokens (comma-separated) to the given
 * file i/o stream.  No leading or training whitespace is displayed.
 */
void MatrixInitMethodPrintTokenList(FILE *stream);

/*!
 * @function MatrixInitMethodCopyTokenList
 *
 * Write the list of method help tokens (comma-separated) to the given
 * buffer.  The total number of characters written (even if it exceeds
 * bufferLen) is returned to allow callers to dynamically allocate a
 * buffer of appropriate length.
 */
size_t MatrixInitMethodCopyTokenList(char *buffer, size_t bufferLen);

/*!
 * @function MatrixInitMethodTokenList
 *
 * Returns a pointer to a buffer (allocated internally by this API)
 * that contains the token list as a C string.
 */
const char* MatrixInitMethodTokenList(void);

/*!
 * @typedef MatrixInitObjectRef
 *
 * The type of a reference to an instance of a MatrixInitMethod.
 */
typedef struct MatrixInitObject * MatrixInitObjectRef;

/*!
 * @function MatrixInitObjectCreate
 *
 * Create a new MatrixInitMethod instance given the specification.
 */
MatrixInitObjectRef MatrixInitObjectCreate(const char *specification);

/*!
 * @function MatrixInitObjectRetain
 *
 * Return a reference to a copy of initObj.
 */
MatrixInitObjectRef MatrixInitObjectRetain(MatrixInitObjectRef initObj);

/*!
 * @function MatrixInitObjectRelease
 *
 * Decrease the reference count of initObj, deallocating once it reaches
 * zero.
 */
void MatrixInitObjectRelease(MatrixInitObjectRef initObj);

/*!
 * @function MatrixInitObjectGetName
 *
 * Returns the registration name of the MatrixInitMethod associated with
 * initObj.
 */
const char* MatrixInitObjectGetName(MatrixInitObjectRef initObj);

/*!
 * @function MatrixInitObjectInit
 *
 * Initialize the n-by-n dimensional matrix M using the initObj method.
 * Timing data will be collected into timer.
 *
 * Threaded methods should limit themselves to nthreads.
 *
 * Returns boolean true if successful.
 */
bool MatrixInitObjectInit(MatrixInitObjectRef initObj, ExecutionTimerRef timer, int nthreads, f_integer n, f_real *M);

#endif /* __MATRIXINITMETHOD_H__ */
