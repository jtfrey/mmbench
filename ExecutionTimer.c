/*
 * ExecutionTimer.h
 *
 * Pseudo-class that handles timing the execution of code and builds
 * statistics.
 */

#include "ExecutionTimer.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

//

const char* ExecutionTimerMetricNames[] = {
                "Walltime",
                "User CPU time",
                "System CPU time",
                "rusage.ru_maxrss",
                "rusage.ru_nswap",
                "rusage.ru_inblock",
                "rusage.ru_outblock",
                NULL
            };

//

typedef struct ExecutionTimerDatum {
    double          value;
    double          min;
    double          max;
    double          m_i;
    double          s_i;
} ExecutionTimerDatum;

//

void
ExecutionTimerDatumReset(
    ExecutionTimerDatum *d
)
{
    memset(d, 0, sizeof(*d));
}

//

void
ExecutionTimerDatumUpdate(
    ExecutionTimerDatum *d,
    unsigned int        n,
    double              value
)
{
    d->value = value;
    if ( n > 1 ) {
        double          m_prev;

        if ( value > d->max ) d->max = value;
        if ( value < d->min ) d->min = value;

        //
        // Update the running variance accumulators:
        //   ( http://www.johndcook.com/blog/standard_deviation/ )
        //
        m_prev = d->m_i;

        d->m_i += (value - m_prev) / (double)n;
        d->s_i += (value - m_prev) * (value - d->m_i);
    } else {
        d->min = d->max = value;
        d->m_i = value;
    }
}

//

double
ExecutionTimerDatumGetAverage(
    ExecutionTimerDatum *d
)
{
    return d->m_i;
}

//

double
ExecutionTimerDatumGetVariance(
    ExecutionTimerDatum *d,
    unsigned int        n
)
{
    return d->s_i / (double)(n - 1);
}

//

double
ExecutionTimerDatumGetStdDeviation(
    ExecutionTimerDatum *d,
    unsigned int        n
)
{
    return sqrt(ExecutionTimerDatumGetVariance(d, n));
}

//
////
//

typedef struct ExecutionTimer {
    unsigned int            refCount;
    bool                    isStarted;

    struct timespec         startTime, endTime;
    struct rusage           startUsage, endUsage;

    unsigned int            cycleCount;
    ExecutionTimerDatum     metrics[ExecutionTimerMetricEOL];
} ExecutionTimer;

//

void
__ExecutionTimerReset(
    ExecutionTimer  *aTimer
)
{
    int             i;

    aTimer->isStarted = false;
    aTimer->cycleCount = 0;
    for ( i = 0; i < ExecutionTimerMetricEOL; i++ ) ExecutionTimerDatumReset(&aTimer->metrics[i]);
}

//

void
__ExecutionTimerUpdateMetrics(
    ExecutionTimer  *aTimer
)
{
    double          v;

    aTimer->cycleCount++;

    v = (double)(aTimer->endTime.tv_sec - aTimer->startTime.tv_sec);
    v += 1e-9 * (double)(aTimer->endTime.tv_nsec - aTimer->startTime.tv_nsec);
    ExecutionTimerDatumUpdate(&aTimer->metrics[ExecutionTimerMetricWalltime], aTimer->cycleCount, v);

    v = (aTimer->endUsage.ru_utime.tv_sec - aTimer->startUsage.ru_utime.tv_sec);
    v += 1e-6 * (aTimer->endUsage.ru_utime.tv_usec - aTimer->startUsage.ru_utime.tv_usec);
    ExecutionTimerDatumUpdate(&aTimer->metrics[ExecutionTimerMetricUserCPU], aTimer->cycleCount, v);

    v = (aTimer->endUsage.ru_stime.tv_sec - aTimer->startUsage.ru_stime.tv_sec);
    v += 1e-6 * (aTimer->endUsage.ru_stime.tv_usec - aTimer->startUsage.ru_stime.tv_usec);
    ExecutionTimerDatumUpdate(&aTimer->metrics[ExecutionTimerMetricSystemCPU], aTimer->cycleCount, v);

    ExecutionTimerDatumUpdate(&aTimer->metrics[ExecutionTimerMetricMaxRSS], aTimer->cycleCount, (double)aTimer->endUsage.ru_maxrss);

    ExecutionTimerDatumUpdate(&aTimer->metrics[ExecutionTimerMetricNSwaps], aTimer->cycleCount, (double)(aTimer->endUsage.ru_nswap - aTimer->startUsage.ru_nswap));

    ExecutionTimerDatumUpdate(&aTimer->metrics[ExecutionTimerMetricIOBlocksIn], aTimer->cycleCount, (double)(aTimer->endUsage.ru_inblock - aTimer->startUsage.ru_inblock));

    ExecutionTimerDatumUpdate(&aTimer->metrics[ExecutionTimerMetricIOBlocksOut], aTimer->cycleCount, (double)(aTimer->endUsage.ru_oublock - aTimer->startUsage.ru_oublock));
}

//

ExecutionTimerRef
ExecutionTimerCreate(void)
{
    ExecutionTimer  *newTimer = (ExecutionTimer*)malloc(sizeof(ExecutionTimer));

    if ( newTimer ) {
        newTimer->refCount = 1;
        __ExecutionTimerReset(newTimer);
    }
    return (ExecutionTimerRef)newTimer;
}

//

ExecutionTimerRef
ExecutionTimerRetain(
    ExecutionTimerRef   aTimer
)
{
    ExecutionTimer      *TIMER = (ExecutionTimer*)aTimer;

    TIMER->refCount++;
    return aTimer;
}

//

void
ExecutionTimerRelease(
    ExecutionTimerRef   aTimer
)
{
    ExecutionTimer      *TIMER = (ExecutionTimer*)aTimer;

    if ( --(TIMER->refCount) == 0 ) free((void*)aTimer);
}

//

void
ExecutionTimerReset(
    ExecutionTimerRef   aTimer
)
{
    __ExecutionTimerReset((ExecutionTimer*)aTimer);
}

//

bool
ExecutionTimerIsStarted(
    ExecutionTimerRef   aTimer
)
{
    return aTimer->isStarted;
}

//

bool
ExecutionTimerHasStatistics(
    ExecutionTimerRef   aTimer
)
{
    return (aTimer->cycleCount > 1) ? true : false;
}

//

void
ExecutionTimerStart(
    ExecutionTimerRef   aTimer
)
{
    ExecutionTimer      *TIMER = (ExecutionTimer*)aTimer;

    TIMER->isStarted = true;
    clock_gettime(CLOCK_MONOTONIC, &TIMER->startTime);
    getrusage(RUSAGE_SELF, &TIMER->startUsage);
}

//

void
ExecutionTimerStop(
    ExecutionTimerRef   aTimer
)
{
    if ( aTimer->isStarted ) {
        ExecutionTimer      *TIMER = (ExecutionTimer*)aTimer;

        getrusage(RUSAGE_SELF, &TIMER->endUsage);
        clock_gettime(CLOCK_MONOTONIC, &TIMER->endTime);
        TIMER->isStarted = false;

        __ExecutionTimerUpdateMetrics(aTimer);
    }
}

//

unsigned int
ExecutionTimerCycleCount(
    ExecutionTimerRef   aTimer
)
{
    return aTimer->cycleCount;
}

//

double
ExecutionTimerGetValue(
    ExecutionTimerRef       aTimer,
    ExecutionTimerMetric    theMetric,
    ExecutionTimerValue     theValue
)
{
    if ( theMetric < ExecutionTimerMetricEOL ) {
        if ( ExecutionTimerHasStatistics(aTimer) ) {
            switch ( theValue ) {
                case ExecutionTimerValueLastValue:
                    return aTimer->metrics[theMetric].value;
                case ExecutionTimerValueMin:
                    return aTimer->metrics[theMetric].min;
                case ExecutionTimerValueMax:
                    return aTimer->metrics[theMetric].max;
                case ExecutionTimerValueAverage:
                    return ExecutionTimerDatumGetAverage(&aTimer->metrics[theMetric]);
                case ExecutionTimerValueVariance:
                    return ExecutionTimerDatumGetVariance(&aTimer->metrics[theMetric], aTimer->cycleCount);
                case ExecutionTimerValueStdDeviation:
                    return ExecutionTimerDatumGetStdDeviation(&aTimer->metrics[theMetric], aTimer->cycleCount);

                default:
                    break;
            }
        } else if ( aTimer->cycleCount > 0 && theValue == ExecutionTimerValueLastValue ) {
            return aTimer->metrics[theMetric].value;
        }
    }
    return INFINITY;
}

//

const char* __ExecutionTimerOutputFormatStrings[] = {
                "table",
                "csv",
                "tsv",
                "json",
                "yaml",
                NULL
            };

const char*
ExecutionTimerOutputFormats(void)
{
    return "table|csv|tsv|json|yaml";
}

ExecutionTimerOutputFormat
ExecutionTimerOutputFormatParse(
    const char      *s
)
{
    ExecutionTimerOutputFormat  format = ExecutionTimerOutputFormatTable;

    // Brute force:
    if ( !s || ! *s ) return ExecutionTimerOutputFormatTable;
    while ( format < ExecutionTimerOutputFormatMax ) {
        if ( strcasecmp(s, __ExecutionTimerOutputFormatStrings[format]) == 0 ) return format;
        format++;
    }
    return ExecutionTimerOutputFormatInvalid;
}

//

const char*
ExecutionTimerOutputFormatToString(
    ExecutionTimerOutputFormat  format
)
{
    if ( format < ExecutionTimerOutputFormatMax ) return __ExecutionTimerOutputFormatStrings[format];
    return NULL;
}

//

void
ExecutionTimerSummarizeToStream(
    ExecutionTimerRef           aTimer,
    ExecutionTimerOutputFormat  format,
    const char                  *timerName,
    FILE                        *stream
)
{
    if ( ExecutionTimerHasStatistics(aTimer) ) {
        ExecutionTimerMetric    metric;
        const char              *delim = ",", *indent;

        switch ( format ) {
            case ExecutionTimerOutputFormatTable:
                fprintf(stream, "%24.24s %16.16s %16.16s %16.16s %16.16s %16.16s %16.16s\n",
                        timerName ? timerName : "",
                        "last value",
                        "miniumum",
                        "maximum",
                        "average",
                        "variance",
                        "std deviation"
                    );
                fprintf(stream, "%1$.24s %1$.16s %1$.16s %1$.16s %1$.16s %1$.16s %1$.16s\n",
                        "-------------------------------"
                    );
                break;

            case ExecutionTimerOutputFormatTSV:
                delim = "\t";
            case ExecutionTimerOutputFormatCSV:
                fprintf(stream, "\"%2$s\"%1$s\"%3$s\"%1$s\"%4$s\"%1$s\"%5$s\"%1$s\"%6$s\"%1$s\"%7$s\"%1$s\"%8$s\"\n",
                        delim,
                        timerName ? timerName : "",
                        "last value",
                        "miniumum",
                        "maximum",
                        "average",
                        "variance",
                        "std deviation"
                    );
                break;

            case ExecutionTimerOutputFormatJSON:
                fprintf(stream, timerName ? "{\"%s\":{" : "{", timerName);
                delim = "";
                break;

            case ExecutionTimerOutputFormatYAML:
                if ( timerName ) {
                    fprintf(stream, "%s:\n", timerName);
                    indent = "    ";
                } else {
                    indent = "";
                }
                break;

        }
        for ( metric = ExecutionTimerMetricWalltime; metric < ExecutionTimerMetricEOL; metric++ ) {
            switch ( format ) {
                case ExecutionTimerOutputFormatTable:
                    fprintf(stream, "%24s %16lg %16lg %16lg %16lg %16lg %16lg\n",
                        ExecutionTimerMetricNames[metric],
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMin),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMax),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueAverage),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueVariance),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueStdDeviation)
                    );
                    break;

                case ExecutionTimerOutputFormatTSV:
                case ExecutionTimerOutputFormatCSV:
                    fprintf(stream, "\"%2$s\"%1$s%3$lg%1$s%4$lg%1$s%5$lg%1$s%6$lg%1$s%7$lg%1$s%8$lg\n",
                        delim,
                        ExecutionTimerMetricNames[metric],
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMin),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMax),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueAverage),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueVariance),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueStdDeviation)
                    );
                    break;

                case ExecutionTimerOutputFormatJSON:
                    fprintf(stream, "%s\"%s\":{\"last-value\":%lg, \"minimum\":%lg, \"maximum\":%lg, \"average\":%lg, \"variance\":%lg, \"standard-deviation\":%lg}", delim,
                        ExecutionTimerMetricNames[metric],
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMin),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMax),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueAverage),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueVariance),
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueStdDeviation)
                    );
                    delim = ",";
                    break;

                case ExecutionTimerOutputFormatYAML:
                    fprintf(stream, "%s%s:\n", indent, ExecutionTimerMetricNames[metric]);
                    fprintf(stream, "%1$s%1$s%2$s: %3$lg\n", indent, "last-value", ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue));
                    fprintf(stream, "%1$s%1$s%2$s: %3$lg\n", indent, "minimum", ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMin));
                    fprintf(stream, "%1$s%1$s%2$s: %3$lg\n", indent, "maximum", ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueMax));
                    fprintf(stream, "%1$s%1$s%2$s: %3$lg\n", indent, "average", ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueAverage));
                    fprintf(stream, "%1$s%1$s%2$s: %3$lg\n", indent, "variance", ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueVariance));
                    fprintf(stream, "%1$s%1$s%2$s: %3$lg\n", indent, "standard-deviation", ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueStdDeviation));
                    break;
            }
        }
        switch ( format ) {
            default:
                break;

            case ExecutionTimerOutputFormatJSON:
                fprintf(stream, timerName ? "}}" : "}");
                break;
        }
    } else {
        ExecutionTimerMetric    metric;
        const char              *delim = ",", *indent;

        switch ( format ) {
            case ExecutionTimerOutputFormatTable:
                fprintf(stream, "%24.24s %16.16s\n",
                        timerName ? timerName : "",
                        "last value"
                    );
                fprintf(stream, "%1$.24s %1$.16s\n",
                        "-------------------------------"
                    );
                break;

            case ExecutionTimerOutputFormatTSV:
                delim = "\t";
            case ExecutionTimerOutputFormatCSV:
                fprintf(stream, "\"%2$s\"%1$s\"%3$s\"\n",
                        delim,
                        timerName ? timerName : "",
                        "last value"
                    );
                break;

            case ExecutionTimerOutputFormatJSON:
                fprintf(stream, timerName ? "{\"%s\":{" : "{", timerName);
                delim = "";
                break;

            case ExecutionTimerOutputFormatYAML:
                if ( timerName ) {
                    fprintf(stream, "%s:\n", timerName);
                    indent = "    ";
                } else {
                    indent = "";
                }
                break;

        }
        for ( metric = ExecutionTimerMetricWalltime; metric < ExecutionTimerMetricEOL; metric++ ) {
            switch ( format ) {
                case ExecutionTimerOutputFormatTable:
                    fprintf(stream, "%24s %16lg\n",
                        ExecutionTimerMetricNames[metric],
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue)
                    );
                    break;

                case ExecutionTimerOutputFormatTSV:
                case ExecutionTimerOutputFormatCSV:
                    fprintf(stream, "\"%2$s\"%1$s%3$lg\n",
                        delim,
                        ExecutionTimerMetricNames[metric],
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue)
                    );
                    break;

                case ExecutionTimerOutputFormatJSON:
                    fprintf(stream, "%s\"%s\":{\"last-value\":%lg}", delim,
                        ExecutionTimerMetricNames[metric],
                        ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue)
                    );
                    delim = ",";
                    break;

                case ExecutionTimerOutputFormatYAML:
                    fprintf(stream, "%s%s:\n", indent, ExecutionTimerMetricNames[metric]);
                    fprintf(stream, "%1$s%1$s%2$s: %3$lg\n", indent, "last-value", ExecutionTimerGetValue(aTimer, metric, ExecutionTimerValueLastValue));
                    break;
            }
        }
        switch ( format ) {
            default:
                break;

            case ExecutionTimerOutputFormatJSON:
                fprintf(stream, timerName ? "}}" : "}");
                break;
        }
    }
}

//
#ifdef EXECUTIONTIMER_FORTRAN_INTERFACE

#include "FortranInterface.h"

#ifndef EXECUTIONTIMER_FORTRAN_MAX_INSTANCES
#define EXECUTIONTIMER_FORTRAN_MAX_INSTANCES 8
#endif

static bool ExecutionTimerFortranInstancesReady = false;
static ExecutionTimerRef ExecutionTimerFortranInstances[EXECUTIONTIMER_FORTRAN_MAX_INSTANCES];

f_integer
FORTRAN_FN_NAME(executiontimer_create)(void)
{
    f_integer       i;

    if ( ! ExecutionTimerFortranInstancesReady ) {
        memset(&ExecutionTimerFortranInstances[0], 0, sizeof(ExecutionTimerFortranInstances));
        ExecutionTimerFortranInstancesReady = true;
    }

    // Find first free slot:
    i = 0;
    while ( i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES ) {
        if ( ExecutionTimerFortranInstances[i] == NULL ) {
            if ( (ExecutionTimerFortranInstances[i] = ExecutionTimerCreate()) ) {
                return i;
            }
        }
        i++;
    }

    return -1;
}

//

void
FORTRAN_FN_NAME(executiontimer_destroy)(
    f_integer   *timer_id
)
{
    f_integer   i = *timer_id;

    if ( ExecutionTimerFortranInstancesReady && (i >= 0) && (i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES) ) {
        if ( ExecutionTimerFortranInstances[i] ) {
            ExecutionTimerRelease(ExecutionTimerFortranInstances[i]);
            ExecutionTimerFortranInstances[i] = NULL;
        }
    }
    *timer_id = -1;
}

//

void
FORTRAN_FN_NAME(executiontimer_reset)(
    f_integer   *timer_id
)
{
    f_integer   i = *timer_id;

    if ( ExecutionTimerFortranInstancesReady && (i >= 0) && (i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES) ) {
        if ( ExecutionTimerFortranInstances[i] ) {
            ExecutionTimerReset(ExecutionTimerFortranInstances[i]);
        }
    }
}

//

void
FORTRAN_FN_NAME(executiontimer_start)(
    f_integer   *timer_id
)
{
    f_integer   i = *timer_id;

    if ( ExecutionTimerFortranInstancesReady && (i >= 0) && (i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES) ) {
        if ( ExecutionTimerFortranInstances[i] ) {
            ExecutionTimerStart(ExecutionTimerFortranInstances[i]);
        }
    }
}

//

void
FORTRAN_FN_NAME(executiontimer_stop)(
    f_integer   *timer_id
)
{
    f_integer   i = *timer_id;

    if ( ExecutionTimerFortranInstancesReady && (i >= 0) && (i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES) ) {
        if ( ExecutionTimerFortranInstances[i] ) {
            ExecutionTimerStop(ExecutionTimerFortranInstances[i]);
        }
    }
}

//

f_real
FORTRAN_FN_NAME(executiontimer_getvalue)(
    f_integer   *timer_id,
    f_integer   *metric_id,
    f_integer   *value_id
)
{
    f_integer   i = *timer_id;
    double      v = INFINITY;

    if ( ExecutionTimerFortranInstancesReady && (i >= 0) && (i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES) ) {
        if ( ExecutionTimerFortranInstances[i] ) {
            v = ExecutionTimerGetValue(ExecutionTimerFortranInstances[i], *metric_id, *value_id);
        }
    }
    if ( v == INFINITY ) {
#ifdef HAVE_FORTRAN_REAL8
        return HUGE_VAL;
#else
        return HUGE_VALF;
#endif
    }
    return v;
}

//

#ifdef HAVE_FORTRAN_REAL8
    #define EXECUTIONTIMER_MAP_INFINITY(X) (((X) == INFINITY) ? HUGE_VAL : (X))
#else
    #define EXECUTIONTIMER_MAP_INFINITY(X) (((X) == INFINITY) ? HUGE_VALF : (X))
#endif

f_logical
FORTRAN_FN_NAME(executiontimer_getvalues)(
    f_integer   *timer_id,
    f_integer   *metric_id,
    f_real      *lastValue,
    f_real      *minValue,
    f_real      *maxValue,
    f_real      *averageValue,
    f_real      *varianceValue,
    f_real      *stdDeviationValue
)
{
    f_integer   i = *timer_id;
    double      v = INFINITY;

    if ( ExecutionTimerFortranInstancesReady && (i >= 0) && (i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES) ) {
        if ( ExecutionTimerFortranInstances[i] ) {
            v = ExecutionTimerGetValue(ExecutionTimerFortranInstances[i], *metric_id, ExecutionTimerValueLastValue);
            *lastValue = EXECUTIONTIMER_MAP_INFINITY(v);

            v = ExecutionTimerGetValue(ExecutionTimerFortranInstances[i], *metric_id, ExecutionTimerValueMin);
            *minValue = EXECUTIONTIMER_MAP_INFINITY(v);

            v = ExecutionTimerGetValue(ExecutionTimerFortranInstances[i], *metric_id, ExecutionTimerValueMax);
            *maxValue = EXECUTIONTIMER_MAP_INFINITY(v);

            v = ExecutionTimerGetValue(ExecutionTimerFortranInstances[i], *metric_id, ExecutionTimerValueAverage);
            *averageValue = EXECUTIONTIMER_MAP_INFINITY(v);

            v = ExecutionTimerGetValue(ExecutionTimerFortranInstances[i], *metric_id, ExecutionTimerValueVariance);
            *varianceValue = EXECUTIONTIMER_MAP_INFINITY(v);

            v = ExecutionTimerGetValue(ExecutionTimerFortranInstances[i], *metric_id, ExecutionTimerValueStdDeviation);
            *stdDeviationValue = EXECUTIONTIMER_MAP_INFINITY(v);

            return F_TRUE;
        }
    }
    return F_FALSE;
}

//

void
FORTRAN_FN_NAME(executiontimer_summarize)(
    f_integer   *timer_id,
    const char  *name,
    f_integer   nameLen
)
{
    f_integer   i = *timer_id;
    double      v = INFINITY;

    if ( ExecutionTimerFortranInstancesReady && (i >= 0) && (i < EXECUTIONTIMER_FORTRAN_MAX_INSTANCES) ) {
        if ( ExecutionTimerFortranInstances[i] ) {
            char        localName[nameLen + 1];

            memcpy(localName, name, nameLen);
            localName[nameLen] = '\0';

            ExecutionTimerSummarizeToStream(ExecutionTimerFortranInstances[i], ExecutionTimerOutputFormatTable, localName, stdout);
        }
    }
}

#endif

//
#ifdef EXECUTIONTIMER_UNIT_TEST

#include <stdio.h>

#ifndef MATRIX_DIM
#define MATRIX_DIM 4000
#endif

#ifndef LOOP_COUNT
#define LOOP_COUNT 25
#endif

int
main()
{
    double                  *M = NULL;
    int                     i, j, n;
    ExecutionTimerRef       theTimer = ExecutionTimerCreate();

    M = (double*)malloc(MATRIX_DIM * MATRIX_DIM * sizeof(*M));

    for ( n = 0; n < LOOP_COUNT; n++ ) {
        ExecutionTimerStart(theTimer);
        #pragma omp parallel for private(i, j)
        for ( i = 0; i < MATRIX_DIM; i++ ) {
            for ( j = 0; j < MATRIX_DIM; j++ ) {
                M[i*MATRIX_DIM + j] = i * i + 2 * i * j + j * j;
            }
        }
        ExecutionTimerStop(theTimer);
    }
    ExecutionTimerSummarizeToStream(theTimer, NULL, stdout);

    FILE                    *fptr = fopen("/dev/null", "w");

    ExecutionTimerReset(theTimer);
    for ( n = 0; n < LOOP_COUNT; n++ ) {
        ExecutionTimerStart(theTimer);
        fwrite(M, sizeof(*M), MATRIX_DIM * MATRIX_DIM, fptr);
        ExecutionTimerStop(theTimer);
    }
    fclose(fptr);

    printf("\n\n");
    ExecutionTimerSummarizeToStream(theTimer, "file write", stdout);

    ExecutionTimerRelease(theTimer);

#ifdef EXECUTIONTIMER_FORTRAN_INTERFACE
    // Try the Fortran interface:
    f_integer           timer_id = FORTRAN_FN_NAME(executiontimer_create)();
    f_real              lv, minv, maxv, avg, var, stddev;
    f_integer           m,v;

    for ( n = 0; n < LOOP_COUNT; n++ ) {
        FORTRAN_FN_NAME(executiontimer_start)(&timer_id);
        #pragma omp parallel for private(i, j)
        for ( i = 0; i < MATRIX_DIM; i++ ) {
            for ( j = 0; j < MATRIX_DIM; j++ ) {
                M[i*MATRIX_DIM + j] = i * i + 2 * i * j + j * j;
            }
        }
        FORTRAN_FN_NAME(executiontimer_stop)(&timer_id);
    }

    printf("\n\n");
    printf("            %16s %16s %16s %16s %16s %16s\n",
            "Last value",
            "Miniumum",
            "Maximum",
            "Average",
            "Variance",
            "Std Deviation"
        );
    m = ExecutionTimerMetricWalltime;
    v = ExecutionTimerValueLastValue; lv = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueMin; minv = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueMax; maxv = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueAverage; avg = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueVariance; var = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueStdDeviation; stddev = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    printf("Walltime    %16lg %16lg %16lg %16lg %16lg %16lg\n", lv, minv, maxv, avg, var, stddev);

    m = ExecutionTimerMetricUserCPU;
    v = ExecutionTimerValueLastValue; lv = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueMin; minv = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueMax; maxv = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueAverage; avg = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueVariance; var = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    v = ExecutionTimerValueStdDeviation; stddev = FORTRAN_FN_NAME(executiontimer_getvalue)(&timer_id, &m, &v);
    printf("User CPU    %16lg %16lg %16lg %16lg %16lg %16lg\n", lv, minv, maxv, avg, var, stddev);

    m = ExecutionTimerMetricSystemCPU;
    if ( FORTRAN_FN_NAME(executiontimer_getvalues)(&timer_id, &m, &lv, &minv, &maxv, &avg, &var, &stddev) )
        printf("Sys CPU     %16lg %16lg %16lg %16lg %16lg %16lg\n", lv, minv, maxv, avg, var, stddev);

    m = ExecutionTimerMetricMaxRSS;
    if ( FORTRAN_FN_NAME(executiontimer_getvalues)(&timer_id, &m, &lv, &minv, &maxv, &avg, &var, &stddev) )
        printf("MaxRSS      %16lg %16lg %16lg %16lg %16lg %16lg\n", lv, minv, maxv, avg, var, stddev);

    m = ExecutionTimerMetricNSwaps;
    if ( FORTRAN_FN_NAME(executiontimer_getvalues)(&timer_id, &m, &lv, &minv, &maxv, &avg, &var, &stddev) )
        printf("NSwaps      %16lg %16lg %16lg %16lg %16lg %16lg\n", lv, minv, maxv, avg, var, stddev);

    m = ExecutionTimerMetricIOBlocksIn;
    if ( FORTRAN_FN_NAME(executiontimer_getvalues)(&timer_id, &m, &lv, &minv, &maxv, &avg, &var, &stddev) )
        printf("I/O - In    %16lg %16lg %16lg %16lg %16lg %16lg\n", lv, minv, maxv, avg, var, stddev);

    m = ExecutionTimerMetricIOBlocksOut;
    if ( FORTRAN_FN_NAME(executiontimer_getvalues)(&timer_id, &m, &lv, &minv, &maxv, &avg, &var, &stddev) )
        printf("I/O - Out   %16lg %16lg %16lg %16lg %16lg %16lg\n", lv, minv, maxv, avg, var, stddev);

    FORTRAN_FN_NAME(executiontimer_destroy)(&timer_id);
#endif

    return 0;
}

#endif /* EXECUTIONTIMER_UNIT_TEST */
