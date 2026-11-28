/* FILE: native/gmb_vm_c/tests/park_wait_selfcheck.c
 * Verifica DETERMINISTICA dell'attesa su evento del task (§0ter) usata dai THREAD del runtime
 * che devono consegnare una risposta al client: attesa dell'handler HTTP/WS (gmb_vm_core.c) e
 * attesa del tool MCP (gmb_mcp_exec.c). Prima erano cicli di nanosleep (poll 500µs / 2ms):
 * gmb_sched_wait_task_event deve invece dormire sulla condizione del task e svegliarsi
 * SULL'EVENTO, rispettando la deadline.
 * Copre:
 *   (a) FINISHED segnalato da un altro thread → risveglio, ok=1, ben prima della deadline;
 *   (b) SYNC_CLOSED (frame sync del tool) segnalato da un altro thread → risveglio, ok=1;
 *   (c) nessun evento → timeout, ok=0, deadline rispettata;
 *   (d) evento già avvenuto prima dell'attesa → ritorno immediato.
 * I thread di prova scrivono stato sotto t->lock, come i veri scrittori (worker_loop e il
 * ritorno del frame sync), poi chiamano gmb_task_signal_event. */
#include "gmb_gpm.h"

#include <pthread.h>
#include <stdio.h>
#include <time.h>

static int g_fail = 0;
static void check(int cond, const char* what)
{
    if (!cond) { printf("FAIL: %s\n", what); g_fail++; }
}

static int64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

typedef struct { gmb_task_t* t; int what; int delay_ms; } sig_arg_t;

/* "Scrittore": a 50 ms mette il predicato a vero sotto il lock, poi segnala. */
static void* signaller(void* p)
{
    sig_arg_t* a = (sig_arg_t*)p;
    struct timespec ts;
    ts.tv_sec = a->delay_ms / 1000;
    ts.tv_nsec = (long)(a->delay_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
    pthread_mutex_lock(&a->t->lock);
    if (a->what == GMB_TASK_WAIT_FINISHED) a->t->state = GMB_TS_FINISHED;
    else a->t->sync_active = 0;
    pthread_mutex_unlock(&a->t->lock);
    gmb_task_signal_event(a->t);
    return NULL;
}

static void wait_and_time(gmb_task_t* t, int what, int64_t timeout_ms, int* out_ok, int64_t* out_elapsed)
{
    int64_t t0 = now_ms();
    *out_ok = gmb_sched_wait_task_event(t, what, timeout_ms);
    *out_elapsed = now_ms() - t0;
}

int main(void)
{
    gmb_scheduler_t* s = gmb_sched_create();
    check(s != NULL, "scheduler creato");
    if (!s) return 1;

    /* (a) FINISHED segnalato da un altro thread. */
    {
        gmb_task_t* t = gmb_task_create(s, 0);
        pthread_t th;
        sig_arg_t a = { t, GMB_TASK_WAIT_FINISHED, 50 };
        int ok = 0;
        int64_t el = 0;
        check(t != NULL, "task creato (a)");
        pthread_create(&th, NULL, signaller, &a);
        wait_and_time(t, GMB_TASK_WAIT_FINISHED, 5000, &ok, &el);
        pthread_join(th, NULL);
        check(ok == 1, "FINISHED: risveglio sull'evento");
        check(el >= 40, "FINISHED: non e' tornata prima dell'evento");
        check(el < 2000, "FINISHED: non ha aspettato la deadline");
    }

    /* (b) SYNC_CLOSED: il tool MCP ha chiuso il frame sync. */
    {
        gmb_task_t* t = gmb_task_create(s, 0);
        pthread_t th;
        sig_arg_t a = { t, GMB_TASK_WAIT_SYNC_CLOSED, 50 };
        int ok = 0;
        int64_t el = 0;
        check(t != NULL, "task creato (b)");
        pthread_mutex_lock(&t->lock);
        t->sync_active = 1;
        pthread_mutex_unlock(&t->lock);
        pthread_create(&th, NULL, signaller, &a);
        wait_and_time(t, GMB_TASK_WAIT_SYNC_CLOSED, 5000, &ok, &el);
        pthread_join(th, NULL);
        check(ok == 1, "SYNC_CLOSED: risveglio sull'evento");
        check(el < 2000, "SYNC_CLOSED: non ha aspettato la deadline");
    }

    /* (c) Nessun evento: timeout con deadline rispettata (nessun rischio di attesa infinita). */
    {
        gmb_task_t* t = gmb_task_create(s, 0);
        int ok = 1;
        int64_t el = 0;
        check(t != NULL, "task creato (c)");
        wait_and_time(t, GMB_TASK_WAIT_FINISHED, 120, &ok, &el);
        check(ok == 0, "timeout: nessun evento -> 0");
        check(el >= 100, "timeout: ha atteso la deadline");
        check(el < 2000, "timeout: non si e' incantata");
    }

    /* (d) Evento gia' avvenuto: nessuna attesa. */
    {
        gmb_task_t* t = gmb_task_create(s, 0);
        int ok = 0;
        int64_t el = 0;
        check(t != NULL, "task creato (d)");
        pthread_mutex_lock(&t->lock);
        t->state = GMB_TS_FINISHED;
        pthread_mutex_unlock(&t->lock);
        wait_and_time(t, GMB_TASK_WAIT_FINISHED, 5000, &ok, &el);
        check(ok == 1, "evento gia' avvenuto: ok");
        check(el < 100, "evento gia' avvenuto: ritorno immediato");
    }

    gmb_sched_destroy(s);
    if (g_fail) printf("FALLITI: %d\n", g_fail);
    else printf("OK: attesa su evento del task (4 blocchi)\n");
    return g_fail ? 1 : 0;
}
