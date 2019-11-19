/*
 * FortranInterface.h
 *
 * Types and helpers for calling Fortran code from C.
 *
 * Macros that define the API:
 *
 *   HAVE_FORTRAN_INT8      Fortran integer type is 8-byte (64-bit)
 *   HAVE_FORTRAN_LOGICAL8  Fortran logical type is 8-byte (64-bit)
 *   HAVE_FORTRAN_REAL8     Fortran real type is 8-byte (64-bit)
 *
 *   FORTRAN_MANGLING_DBLUNDERSCORE
 *                          Fortran functions/subroutines end with
 *                          two underscore (_) characters
 *   FORTRAN_MANGLING_NONE  Fortran functions/subroutines use no
 *                          leading/trailing underscore (_)
 *                          characters
 *
 * By default (no macros defined) the API uses 32-bit integers, logicals,
 * and reals and a single trailing underscore (_) on function/subroutine
 * names.
 */

#ifndef __FORTRANINTERFACE_H__
#define __FORTRANINTERFACE_H__

#include <stdint.h>

#ifdef HAVE_FORTRAN_INT8
    /*!
     * @typedef f_integer
     *
     * type of a Fortran integer variable
     */
    typedef int64_t f_integer;

    /*!
     * @defined FMT_F_INTEGER
     *
     * printf format string for Fortran integer
     * variables
     */
    #define FMT_F_INTEGER "%ld"
#else
    typedef int32_t f_integer;
    #define FMT_F_INTEGER "%d"
#endif

#ifdef HAVE_FORTRAN_LOGICAL8
    /*!
     * @typedef f_logical
     *
     * type of a Fortran logical variable
     */
    typedef int64_t f_logical;

    /*!
     * @defined FMT_F_LOGICAL
     *
     * printf format string for Fortran logical
     * variables
     */
    #define FMT_F_LOGICAL "%ld"
#else
    typedef int32_t f_logical;
    #define FMT_F_LOGICAL "%d"
#endif

/*!
 * @defined F_TRUE
 *
 * Fortran logical true value
 */
#define F_TRUE (f_logical)1

/*!
 * @defined F_FALSE
 *
 * Fortran logical false value
 */
#define F_FALSE (f_logical)0

#ifdef HAVE_FORTRAN_REAL8
    /*!
     * @typedef f_real
     *
     * type of a Fortran real variable
     */
    typedef double f_real;

    /*!
     * @defined FMT_F_REAL
     *
     * printf format string for Fortran real
     * variables
     */
    #define FMT_F_REAL "%lg"
#else
    typedef float f_real;
    #define FMT_F_REAL "%g"
#endif

/*!
 * @constant F_ONE
 *
 * Fortran real valued at 1
 */
extern f_real F_ONE;

/*!
 * @constant F_ZERO
 *
 * Fortran real valued at 0
 */
extern f_real F_ZERO;

#if defined(FORTRAN_MANGLING_DBLUNDERSCORE)
    #define FORTRAN_FN_NAME(X)     X##__
#elif defined(FORTRAN_MANGLING_NONE)
    #define FORTRAN_FN_NAME(X)     X
#else
    /*!
     * @defined FORTRAN_FN_NAME
     *
     * Preprocessor macro that expands a Fortran function name
     * to the desired mangling scheme (e.g. adding a trailing
     * underscore character)
     */
    #define FORTRAN_FN_NAME(X)     X##_
#endif

#endif /* __FORTRANINTERFACE_H__ */
