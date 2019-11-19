/*
 * FortranInterface.c
 *
 * Types and helpers for calling Fortran code from C.
 *
 */

#include "FortranInterface.h"
 
#ifdef HAVE_FORTRAN_REAL8
    f_real F_ONE=1.0;
    f_real F_ZERO=0.0;
#else
    f_real F_ONE=1.0f;
    f_real F_ZERO=0.0f;
#endif
