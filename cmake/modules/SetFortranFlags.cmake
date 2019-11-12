######################################################
# Determine and set the Fortran compiler flags we want
######################################################

####################################################################
# Make sure that the default build type is RELEASE if not specified.
####################################################################

# Make sure the build type is uppercase
STRING(TOUPPER "${CMAKE_BUILD_TYPE}" BT)

IF(BT STREQUAL "RELEASE")
    SET(CMAKE_BUILD_TYPE RELEASE CACHE STRING
      "Choose the type of build, options are DEBUG, RELEASE, or TESTING."
      FORCE)
ELSEIF(BT STREQUAL "DEBUG")
    SET (CMAKE_BUILD_TYPE DEBUG CACHE STRING
      "Choose the type of build, options are DEBUG, RELEASE, or TESTING."
      FORCE)
ELSEIF(BT STREQUAL "TESTING")
    SET (CMAKE_BUILD_TYPE TESTING CACHE STRING
      "Choose the type of build, options are DEBUG, RELEASE, or TESTING."
      FORCE)
ENDIF(BT STREQUAL "RELEASE")

#########################################################
# If the compiler flags have already been set, return now
#########################################################

IF(CMAKE_Fortran_FLAGS_RELEASE AND CMAKE_Fortran_FLAGS_TESTING AND CMAKE_Fortran_FLAGS_DEBUG)
    RETURN ()
ENDIF(CMAKE_Fortran_FLAGS_RELEASE AND CMAKE_Fortran_FLAGS_TESTING AND CMAKE_Fortran_FLAGS_DEBUG)

########################################################################
# Set the appropriate flags for this compiler for each build type.
#######################################################################

# There is some bug where -march=native doesn't work on Mac
IF(APPLE)
    SET(GNUNATIVE "-mtune=native")
ELSE()
    SET(GNUNATIVE "-march=native")
ENDIF()

IF ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")
	#
	# GNU compiler:
	#
	SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -I.")
	IF(HAVE_FORTRAN_REAL8)
	    SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fdefault-real-8")
	ENDIF (HAVE_FORTRAN_REAL8)
	IF(HAVE_FORTRAN_INT8)
	    SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fdefault-integer-8")
	ENDIF (HAVE_FORTRAN_INT8)
	SET(CMAKE_Fortran90_FLAGS "${CMAKE_Fortran90_FLAGS} -ffree-form -ffree-line-length-none")

	## DEBUG flags:
	SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} -O0 -Wall -fbacktrace -fcheck=bounds")

	## TESTING flags:
	SET(CMAKE_Fortran_FLAGS_TESTING "${CMAKE_Fortran_FLAGS_TESTING} -O2")

	## RELEASE flags:
	SET(CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -O3 ${GNUNATIVE}")
ELSEIF ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "Intel")
	#
	# Intel compiler:
	#
	IF (WIN32)
		SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} /QI.")
		SET(CMAKE_Fortran90_FLAGS "${CMAKE_Fortran90_FLAGS} /Qnofixed")

		## DEBUG flags:
		SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} /Od /warn:all /traceback /check:bounds")

		## TESTING flags:
		SET(CMAKE_Fortran_FLAGS_TESTING "${CMAKE_Fortran_FLAGS_TESTING} /O2")

		## RELEASE flags:
		SET(CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} /O3 /QxHost")
	ELSE (WIN32)
		SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -I.")
        IF (HAVE_FORTRAN_REAL8)
            SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -r8")
        ENDIF (HAVE_FORTRAN_REAL8)
        IF (HAVE_FORTRAN_INT8)
            SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -i8")
        ENDIF (HAVE_FORTRAN_INT8)
		SET(CMAKE_Fortran90_FLAGS "${CMAKE_Fortran90_FLAGS} -nofixed")

		## DEBUG flags:
		SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} -O0 -warn all -traceback -check bounds")

		## TESTING flags:
		SET(CMAKE_Fortran_FLAGS_TESTING "${CMAKE_Fortran_FLAGS_TESTING} -O2")

		## RELEASE flags:
		SET(CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -O3 -xHost")
	ENDIF (WIN32)
ELSEIF ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "PGI")
	#
	# Portland compiler:
	#
	SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -I.")
    IF (HAVE_FORTRAN_REAL8)
        SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -r8")
    ELSE (HAVE_FORTRAN_REAL8)
        SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -r4")
    ENDIF (HAVE_FORTRAN_REAL8)
    IF (HAVE_FORTRAN_INT8)
        SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -i8")
    ELSE (HAVE_FORTRAN_INT8)
        SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -i4")
    ENDIF (HAVE_FORTRAN_INT8)
	SET(CMAKE_Fortran90_FLAGS "${CMAKE_Fortran90_FLAGS} -Mfree")

	## DEBUG flags:
	SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} -O0 -traceback -Mbounds")

	## TESTING flags:
	SET(CMAKE_Fortran_FLAGS_TESTING "${CMAKE_Fortran_FLAGS_TESTING} -O2")

	## RELEASE flags:
	SET(CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -O3 -ta=host")
ELSE ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")
	MESSAGE("Unknown compiler")
ENDIF ("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU")

STRING(STRIP "${CMAKE_Fortran_FLAGS}" TMP_VALUE)
SET(CMAKE_Fortran_FLAGS "${TMP_VALUE}" CACHE STRING "Set the CMAKE_Fortran_FLAGS flags" FORCE)
UNSET(TMP_VALUE)

STRING(STRIP "${CMAKE_Fortran_FLAGS_RELEASE}" TMP_VALUE)
SET(CMAKE_Fortran_FLAGS_RELEASE "${TMP_VALUE}" CACHE STRING "Set the CMAKE_Fortran_FLAGS_RELEASE flags" FORCE)
UNSET(TMP_VALUE)

STRING(STRIP "${CMAKE_Fortran_FLAGS_DEBUG}" TMP_VALUE)
SET(CMAKE_Fortran_FLAGS_DEBUG "${TMP_VALUE}" CACHE STRING "Set the CMAKE_Fortran_FLAGS_DEBUG flags" FORCE)
UNSET(TMP_VALUE)

STRING(STRIP "${CMAKE_Fortran_FLAGS_TESTING}" TMP_VALUE)
SET(CMAKE_Fortran_FLAGS_TESTING "${TMP_VALUE}" CACHE STRING "Set the CMAKE_Fortran_FLAGS_TESTING flags" FORCE)
UNSET(TMP_VALUE)

STRING(STRIP "${CMAKE_Fortran90_FLAGS}" TMP_VALUE)
SET(CMAKE_Fortran90_FLAGS "${TMP_VALUE}" CACHE STRING "Set the CMAKE_Fortran90_FLAGS flags" FORCE)
UNSET(TMP_VALUE)
