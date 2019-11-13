CMAKE_MINIMUM_REQUIRED (VERSION 3.0)
PROJECT (mmbench VERSION 1.0 LANGUAGES Fortran C)

# Must use GNUInstallDirs to install libraries into correct
# locations on all platforms.
INCLUDE(GNUInstallDirs)

# Add our local modlues to the module path
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

#  Fortran 90 must be supported
IF(NOT CMAKE_Fortran_COMPILER_SUPPORTS_F90)
    MESSAGE(FATAL_ERROR "Fortran compiler does not support F90")
ENDIF(NOT CMAKE_Fortran_COMPILER_SUPPORTS_F90)

# Fortran types:
OPTION(HAVE_FORTRAN_REAL8 "Use 64-bit reals in Fortran subroutines" FALSE)
OPTION(HAVE_FORTRAN_INT8 "Use 64-bit integers in Fortran subroutines" FALSE)

# Setup default Fortran compiler flags for each build type:
INCLUDE(${CMAKE_MODULE_PATH}/SetFortranFlags.cmake)

# Convert the CMAKE_Fortran90_FLAGS to a list:
IF (DEFINED CMAKE_Fortran90_FLAGS AND NOT ${CMAKE_Fortran90_FLAGS} EQUAL "")
    STRING(REPLACE " " ";" CMAKE_Fortran90_FLAGS ${CMAKE_Fortran90_FLAGS})
ENDIF (DEFINED CMAKE_Fortran90_FLAGS AND NOT ${CMAKE_Fortran90_FLAGS} EQUAL "")

# Static builds
OPTION(IS_STATIC_BUILD "Produce static executables/libraries (no shared libraries)" FALSE)
IF (IS_STATIC_BUILD)
    SET(CHEMKIN_LIBS_SHARED FALSE)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    SET(BUILD_SHARED_LIBS OFF)
    SET(CMAKE_EXE_LINKER_FLAGS "-static")
    SET(BLA_STATIC TRUE)
ENDIF (IS_STATIC_BUILD)

# Overrides for optional features:
OPTION(NO_BLAS "Do not use a BLAS sgemm/dgemm" FALSE)
OPTION(NO_OPENMP "Do not use OpenMP parallelism" FALSE)
OPTION(NO_DIRECTIO "Do not use direct i/o" FALSE)

# Locate a BLAS library:
IF (NOT NO_BLAS)
    FIND_PACKAGE(BLAS)
ENDIF (NOT NO_BLAS)

# Figure out the OpenMP flags:
IF (NOT NO_OPENMP)
    FIND_PACKAGE(OpenMP)
ENDIF (NOT NO_OPENMP)

# Check if O_DIRECT is available.
IF (NOT NO_DIRECTIO)
    CHECK_C_SOURCE_COMPILES("
      #include <fcntl.h>
      main() { int i = O_DIRECT; }
      " HAVE_DIRECTIO)
    IF (NOT HAVE_DIRECTIO)
        UNSET(HAVE_DIRECTIO CACHE)
        CHECK_C_SOURCE_COMPILES("
          #define _GNU_SOURCE
          #include <fcntl.h>
          main() { int i = O_DIRECT; }
          " HAVE_DIRECTIO)
    ENDIF (NOT HAVE_DIRECTIO)
ENDIF (NOT NO_DIRECTIO)

#
# Setup the program to build:
#
ADD_EXECUTABLE(mmbench mat_mult_basic.F90 mat_mult_smart.F90 mat_mult_optimized.F90 mat_mult_blas.F90 mat_mult_openmp.F90 mat_mult_openmp_optimized.F90 mmbench.c)
SET_TARGET_PROPERTIES(mmbench PROPERTIES LINKER_LANGUAGE C)
TARGET_COMPILE_OPTIONS(mmbench PUBLIC $<$<COMPILE_LANGUAGE:Fortran>:${CMAKE_Fortran90_FLAGS}>)
SET_SOURCE_FILES_PROPERTIES(mat_mult_basic.F90 mat_mult_smart.F90 mat_mult_blas.F90 mat_mult_openmp.F90 PROPERTIES COMPILE_FLAGS ${CMAKE_Fortran_FLAGS_DEBUG})
SET_SOURCE_FILES_PROPERTIES(mat_mult_optimized.F90 mat_mult_openmp_optimized.F90 PROPERTIES COMPILE_FLAGS ${CMAKE_Fortran_FLAGS_RELEASE})
IF (HAVE_FORTRAN_REAL8)
    TARGET_COMPILE_OPTIONS(mmbench PUBLIC "-DHAVE_FORTRAN_REAL8")
ENDIF (HAVE_FORTRAN_REAL8)
IF (HAVE_FORTRAN_INT8)
    TARGET_COMPILE_OPTIONS(mmbench PUBLIC "-DHAVE_FORTRAN_INT8")
ENDIF (HAVE_FORTRAN_INT8)
IF (BLAS_FOUND)
    TARGET_COMPILE_OPTIONS(mmbench PUBLIC "-DHAVE_BLAS")
    TARGET_LINK_LIBRARIES(mmbench ${BLAS_LDFLAGS} ${BLAS_LIBRARIES})
ENDIF (BLAS_FOUND)
IF (HAVE_DIRECTIO)
    TARGET_COMPILE_OPTIONS(mmbench PUBLIC "-DHAVE_DIRECTIO")
ENDIF (HAVE_DIRECTIO)
IF (OpenMP_FOUND)
    TARGET_COMPILE_OPTIONS(mmbench PUBLIC "-DHAVE_OPENMP")
    TARGET_COMPILE_OPTIONS(mmbench PUBLIC $<$<COMPILE_LANGUAGE:Fortran>:${OpenMP_Fortran_FLAGS}>)
    TARGET_LINK_LIBRARIES(mmbench ${OpenMP_Fortran_FLAGS} ${OpenMP_Fortran_LIBRARIES})
ENDIF (OpenMP_FOUND)

# What does "make install" do?
INSTALL(TARGETS mmbench DESTINATION ${CMAKE_INSTALL_BINDIR})
