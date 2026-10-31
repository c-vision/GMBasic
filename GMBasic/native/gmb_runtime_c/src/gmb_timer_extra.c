/* gmb_timer_extra.c — parità CommonLib/TimerUtils.cs:78 (stopwatch handle-registry high-res)
 * Aggiunge StopwatchStart/Stop/Elapsed in nanosecondi (CLOCK_MONOTONIC).
 * Isolato: non tocca gmb_math.c:404 (gmb_timer sec coarse) né gmb_sleep_ms.
 */
#include "gmb_runtime.h"
#include <time.h>
#include <string.h>

#define GMB_TIMER_MAX 256

typedef struct {
    int in_use;
    int handle;
    struct timespec start;
} gmb_stopwatch_t;

static gmb_stopwatch_t g_stopwatches[GMB_TIMER_MAX];
static int g_next_timer_handle = 1;

static struct timespec g_timer_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

static long g_timer_elapsed_ns(const gmb_stopwatch_t* sw)
{
    struct timespec now = g_timer_now();
    long long ns = (long long)(now.tv_sec - sw->start.tv_sec) * 1000000000LL
                 + (long long)(now.tv_nsec - sw->start.tv_nsec);
    if (ns < 0) ns = 0;
    return (long)ns;
}

int gmb_timer_stopwatch_start(void)
{
    int handle = g_next_timer_handle++;
    for (int i = 0; i < GMB_TIMER_MAX; i++) {
        if (!g_stopwatches[i].in_use) {
            g_stopwatches[i].in_use = 1;
            g_stopwatches[i].handle = handle;
            g_stopwatches[i].start = g_timer_now();
            return handle;
        }
    }
    return -1;
}

static gmb_stopwatch_t* g_timer_find(int handle)
{
    for (int i = 0; i < GMB_TIMER_MAX; i++)
        if (g_stopwatches[i].in_use && g_stopwatches[i].handle == handle)
            return &g_stopwatches[i];
    return NULL;
}

long gmb_timer_stopwatch_stop(int handle)
{
    gmb_stopwatch_t* sw = g_timer_find(handle);
    if (!sw) return 0;
    long ns = g_timer_elapsed_ns(sw);
    sw->in_use = 0;
    return ns;
}

long gmb_timer_stopwatch_elapsed(int handle)
{
    gmb_stopwatch_t* sw = g_timer_find(handle);
    if (!sw) return 0;
    return g_timer_elapsed_ns(sw);
}
