/* gmb_blackboard.c — Blackboard chiave/valore condiviso e concorrente (mondo C).
 *
 * Parita' di comportamento con Basic.Agents.Blackboard (C#): area di scambio strutturata
 * chiave/valore per coordinamento tra goroutine/agenti paralleli. Scrittura last-write-wins;
 * READ legge lo stato corrente ("" se assente); WAIT attende che la chiave abbia un valore
 * (o cambi) fino a timeoutMs; REMOVE/CLEAR/COUNT come il C#. Thread-safe (mutex + cond).
 *
 * NB (pattern corretto GPM, vedi vault Todo AgentBatch-GPM-Park-Based-Rewrite §0bis.10):
 * nelle goroutine il join si fa col WAITGROUP nativo; BLACKBOARDWAIT e' bloccante (timeout)
 * e va usato fuori da un worker GPM. Questo file e' il runtime CONDIVISO: VM/C e i backend
 * C99 chiamano le stesse gmb_blackboard_* (mai duplicazioni inline nei backend).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define GMB_BLACKBOARD_MAX 256

typedef struct {
    char key[128];
    char value[1024];
} gmb_blackboard_entry_t;

static gmb_blackboard_entry_t g_bb_entries[GMB_BLACKBOARD_MAX];
static int g_bb_count = 0;
static pthread_mutex_t g_bb_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_bb_cond = PTHREAD_COND_INITIALIZER;

static int gmb_blackboard_find_locked(const char* key)
{
    for (int i = 0; i < g_bb_count; i++)
        if (strcasecmp(g_bb_entries[i].key, key) == 0) return i;
    return -1;
}

/* Scrive (o aggiorna) la chiave e sveglia i WAIT in attesa. Ritorna 1 = ok, 0 = errore
 * (chiave vuota o blackboard pieno). Parita' Blackboard.Write (C#). */
int gmb_blackboard_write(const char* key, const char* value)
{
    if (!key || !*key) return 0;
    pthread_mutex_lock(&g_bb_lock);
    int idx = gmb_blackboard_find_locked(key);
    if (idx < 0)
    {
        if (g_bb_count >= GMB_BLACKBOARD_MAX) { pthread_mutex_unlock(&g_bb_lock); return 0; }
        idx = g_bb_count++;
        snprintf(g_bb_entries[idx].key, sizeof(g_bb_entries[idx].key), "%s", key);
    }
    snprintf(g_bb_entries[idx].value, sizeof(g_bb_entries[idx].value), "%s", value ? value : "");
    pthread_cond_broadcast(&g_bb_cond);
    pthread_mutex_unlock(&g_bb_lock);
    return 1;
}

/* Legge il valore corrente della chiave (malloc; "" se assente). Parita' Blackboard.Read. */
char* gmb_blackboard_read(const char* key)
{
    char* out = (char*)malloc(1);
    if (!key || !*key) { out[0] = '\0'; return out; }
    pthread_mutex_lock(&g_bb_lock);
    int idx = gmb_blackboard_find_locked(key);
    if (idx >= 0)
    {
        free(out);
        out = strdup(g_bb_entries[idx].value);
    }
    else out[0] = '\0';
    pthread_mutex_unlock(&g_bb_lock);
    return out;
}

/* Attende che la chiave abbia un valore (o cambi), fino a timeout_ms. Ritorna il valore
 * (malloc) o "" se timeout. Parita' Blackboard.Wait (C#): l'attesa e' cond-based e il
 * broadcast di gmb_blackboard_write sveglia. NB: bloccante — non chiamare da un worker GPM
 * (pattern corretto: join col WAITGROUP). */
char* gmb_blackboard_wait(const char* key, int timeout_ms)
{
    char* out = (char*)malloc(1);
    if (!key || !*key) { out[0] = '\0'; return out; }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long ms = timeout_ms > 0 ? timeout_ms : 10000;
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000L; }

    pthread_mutex_lock(&g_bb_lock);
    int idx = gmb_blackboard_find_locked(key);
    if (idx < 0)
    {
        int rc = pthread_cond_timedwait(&g_bb_cond, &g_bb_lock, &ts);
        (void)rc;
        idx = gmb_blackboard_find_locked(key);
    }
    if (idx >= 0) { free(out); out = strdup(g_bb_entries[idx].value); }
    else out[0] = '\0';
    pthread_mutex_unlock(&g_bb_lock);
    return out;
}

/* Rimuove una chiave. 1 = rimossa, 0 = assente. Parita' Blackboard.Remove. */
int gmb_blackboard_remove(const char* key)
{
    if (!key || !*key) return 0;
    pthread_mutex_lock(&g_bb_lock);
    int idx = gmb_blackboard_find_locked(key);
    if (idx < 0) { pthread_mutex_unlock(&g_bb_lock); return 0; }
    if (idx < g_bb_count - 1)
        g_bb_entries[idx] = g_bb_entries[g_bb_count - 1];
    g_bb_count--;
    pthread_cond_broadcast(&g_bb_cond);
    pthread_mutex_unlock(&g_bb_lock);
    return 1;
}

/* Svuota il blackboard. Parita' Blackboard.Clear. */
void gmb_blackboard_clear(void)
{
    pthread_mutex_lock(&g_bb_lock);
    g_bb_count = 0;
    pthread_cond_broadcast(&g_bb_cond);
    pthread_mutex_unlock(&g_bb_lock);
}

/* Numero di chiavi (diagnostica). Parita' Blackboard.Count. */
int gmb_blackboard_count(void)
{
    pthread_mutex_lock(&g_bb_lock);
    int n = g_bb_count;
    pthread_mutex_unlock(&g_bb_lock);
    return n;
}
