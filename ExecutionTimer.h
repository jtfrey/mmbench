/*
 * ExecutionTimer.h
 *
 * Pseudo-class that handles timing the execution of code and builds
 * statistics.
 */

#ifndef __EXECUTIONTIMER_H__
#define __EXECUTIONTIMER_H__

#include <stdio.h>
#include <stdbool.h>

/*!
 * @typedef ExecutionTimerRef
 *
 * Type of a reference to an ExecutionTimer pseudo-object.
 */
typedef struct ExecutionTimer * ExecutionTimerRef;

/*!
 * @function ExecutionTimerCreate
 *
 * Allocate and initialize a new ExecutionTimer object.
 */
ExecutionTimerRef ExecutionTimerCreate(void);

/*!
 * @function ExecutionTimerRetain
 *
 * Make a reference copy of aTimer.
 */
ExecutionTimerRef ExecutionTimerRetain(ExecutionTimerRef aTimer);

/*!
 * @function ExecutionTimerRelease
 *
 * Release a reference to an ExecutionTimer object.  When the object's
 * reference count reaches zero, it is deallocated.
 */
void ExecutionTimerRelease(ExecutionTimerRef aTimer);

/*!
 * @function ExecutionTimerReset
 *
 * Reset an ExecutionTimer object.  The cycle count, values, and statistics
 * are zeroed.
 */
void ExecutionTimerReset(ExecutionTimerRef aTimer);

/*!
 * @function ExecutionTimerIsStarted
 *
 * Returns boolean true if the ExecutionTimerStart() function has been called
 * on aTimer and a matching ExecutionTimerStop() has not yet been called.
 */
bool ExecutionTimerIsStarted(ExecutionTimerRef aTimer);

/*!
 * @function ExecutionTimerHasStatistics
 *
 * Returns boolean true if aTimer has been start-stopped at least two times
 * already.  At least two readings implies that statistics can be calculated.
 */
bool ExecutionTimerHasStatistics(ExecutionTimerRef aTimer);

/*!
 * @function ExecutionTimerStart
 *
 * Note current time and resource usage levels, mark aTimer as having been
 * started.
 *
 * Additional calls to ExecutionTimerStart() without calling ExecutionTimerStop()
 * simply restart the current timing cycle and do NOT accumulate statistics.
 */
void ExecutionTimerStart(ExecutionTimerRef aTimer);

/*!
 * @function ExecutionTimerStop
 *
 * Note current time and resource usage levels, mark aTimer as having been
 * stopped and calculate any statistics.
 */
void ExecutionTimerStop(ExecutionTimerRef aTimer);

/*!
 * @function ExecutionTimerCycleCount
 *
 * Returns the number of start-stop cycles aTimer has traversed since its
 * creation (or last ExecutionTimerReset()).
 */
unsigned int ExecutionTimerCycleCount(ExecutionTimerRef aTimer);

/*!
 * @enum ExecutionTimerMetric
 *
 * The various metrics that are maintained by an ExecutionTimer object.
 */
enum {
    ExecutionTimerMetricWalltime = 0,
    ExecutionTimerMetricUserCPU,
    ExecutionTimerMetricSystemCPU,
    ExecutionTimerMetricMaxRSS,
    ExecutionTimerMetricNSwaps,
    ExecutionTimerMetricIOBlocksIn,
    ExecutionTimerMetricIOBlocksOut,
    //
    ExecutionTimerMetricEOL
};

/*!
 * @typedef ExecutionTimerMetric
 *
 * Type used in conjunction with the ExecutionTimerMetric enumeration.
 */
typedef unsigned int ExecutionTimerMetric;

/*!
 * @enum ExecutionTimerValue
 *
 * The various values that are maintained by an ExecutionTimer object for
 * each metric.
 */
enum {
    ExecutionTimerValueLastValue = 0,
    ExecutionTimerValueMin,
    ExecutionTimerValueMax,
    ExecutionTimerValueAverage,
    ExecutionTimerValueVariance,
    ExecutionTimerValueStdDeviation,
    //
    ExecutionTimerValueEOL
};

/*!
 * @typedef ExecutionTimerValue
 *
 * Type used in conjunction with the ExecutionTimerValue enumeration.
 */
typedef unsigned int ExecutionTimerValue;

/*!
 * @function ExecutionTimerGetValue
 *
 * Return the given value of the given metric for aTimer.  Returns the
 * constant INFINITY for undefined values.
 */
double ExecutionTimerGetValue(ExecutionTimerRef aTimer, ExecutionTimerMetric theMetric, ExecutionTimerValue theValue);

/*!
 * @enum ExecutionTimerOutputFormat
 *
 * The various output formats to which the timing data can be output.
 */
enum {
    ExecutionTimerOutputFormatTable = 0,
    ExecutionTimerOutputFormatCSV,
    ExecutionTimerOutputFormatTSV,
    ExecutionTimerOutputFormatJSON,
    ExecutionTimerOutputFormatYAML,
    //
    ExecutionTimerOutputFormatMax,
    ExecutionTimerOutputFormatInvalid
};

/*!
 * @typedef ExecutionTimerOutputFormat
 *
 * Type used in conjunction with the ExecutionTimerOutputFormat enumeration.
 */
typedef unsigned int ExecutionTimerOutputFormat;

/*!
 * @function ExecutionTimerOutputFormats
 *
 * Returns a C string constant comprising the list of valid formats as a
 * vertical bar-separated string.
 */
const char* ExecutionTimerOutputFormats(void);

/*!
 * @function ExecutionTimerOutputFormatParse
 *
 * Returns the output format associated with a string.
 */
ExecutionTimerOutputFormat ExecutionTimerOutputFormatParse(const char *s);

/*!
 * @function ExecutionTimerOutputFormatToString
 *
 * Return a string constant that is the unparsed from of format.
 */
const char* ExecutionTimerOutputFormatToString(ExecutionTimerOutputFormat format);

/*!
 * @function ExecutionTimerSummarizeToStream
 *
 * Write a summary of the timer's value and statistics to the given file stream.
 */
void ExecutionTimerSummarizeToStream(ExecutionTimerRef aTimer, ExecutionTimerOutputFormat format, const char *timerName, FILE *stream);


/*

The Fortran Interface
=====================

If the ExecutionTimer.c source is compiled with the EXECUTIONTIMER_FORTRAN_INTERFACE
macro defined, a set of Fortran subroutines/functions will be included that provide
an interface to this pseudo class.

There are a fixed number of timers available to the Fortran interface.  This is because a
static, fixed-dimension array of ExecutionTimerRef values is used to map the pointers to
Fortran integers.  The EXECUTIONTIMER_FORTRAN_MAX_INSTANCES macro controls the size of the
table and defaults to 8.


Allocating a Timer
------------------

Fortran code can allocate a new ExecutionTimer object using the ExecutionTimer_Create()
function:

```
implicit none
integer :: ExecutionTimer_Create, timerId

timerId = ExecutionTimer_Create()
```


Starting/Stopping a Timer
-------------------------

Starting and stopping the timer are accomplished as follows:

```
Call ExecutionTimer_Start(timerId)
!
! Do stuff...
!
Call ExecutionTimer_Stop(timerId)
```


Retrieving Data
---------------

Values are retrieved from a timer using:

* `ExecutionTimer_GetValue()`: a function that returns a single value of the specified metric
* `ExecutionTimer_GetValues()`: a function that returns all values of the specified metric en masse

Metrics are the data points analyzed by a timer:

| Metric Id | Description                          |
| --------- | ------------------------------------ |
| 0         | Walltime                             |
| 1         | User CPU time                        |
| 2         | System CPU time                      |
| 3         | Maximum Resident Set Size (RSS)      |
| 4         | Number of pages moved in/out of swap |
| 5         | Number of blocks read from disk      |
| 6         | Number of blocks written to disk     |

Values are the statistics maintained for each metric:

| Value Id | Description        |
| -------- | ------------------ |
| 0        | Last value         |
| 1        | Minimum value      |
| 2        | Maximum value      |
| 3        | Average value      |
| 4        | Variance           |
| 5        | Standard deviation |

The average walltime can thus be fetched as:

```
real :: ExecutionTimer_GetValue, v

v = ExecutionTimer_GetValue(timerId, 0, 3)
```

All statistical values for user CPU can be fetched en masse as:

```
real :: lv, minv, maxv, avg, var, stddev
logical :: ExecutionTimer_GetValues

if ( ExecutionTimer_GetValues(timerId, 1, lv, minv, maxv, avg, var, stddev) ) then
    write(*,*) lv, minv, maxv, avg, var, stddev
end if
```


Summarizing a Timer to stdout
-----------------------------

The timer can be summarized to stdout as follows:

```
Call ExecutionTimer_Summarize(timerId)
```


Resetting a Timer
-----------------

The timer can be cleared of all statistics and values as follows:

```
Call ExecutionTimer_Reset(timerId)
```


Deallocating a Timer
--------------------

An allocated timer should be deallocated when it is no longer being used:

```
Call ExecutionTimer_Destroy(timerId)
```

*/

#endif /* __EXECUTIONTIMER_H__ */
