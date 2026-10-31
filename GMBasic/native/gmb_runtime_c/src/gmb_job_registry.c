/* gmb_job_registry.c — registro job training/apply (R2.4).
 *
 * Ring buffer thread-safe (lock mutex per modifica, ma le letture
 * sono unlocked — ok per uso tipico GPM a singolo worker).
 * L'ID auto-incrementante wrappa a INT_MAX senza collisioni.
 */
#include "gmb_job_registry.h"
#include <string.h>
#include <stdio.h>

/* Controllo thread safety minimo: le scritture vanno in mutua
 * esclusione; le letture possono vedere dati inconsistenti ma
 * in pratica la GPM serializza (un job alla volta). */
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION s_lock;
static int s_lock_init = 0;
static void lock_init(void) { if (!s_lock_init) { InitializeCriticalSection(&s_lock); s_lock_init = 1; } }
#define LOCK() EnterCriticalSection(&s_lock)
#define UNLOCK() LeaveCriticalSection(&s_lock)
#else
#include <pthread.h>
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&s_lock)
#define UNLOCK() pthread_mutex_unlock(&s_lock)
#endif

static gmb_job_record s_records[GMB_JOB_REGISTRY_CAP];
static int s_count = 0;
static int s_next_id = 1;
static int s_last_id = 0;

int gmb_job_registry_add(gmb_job_kind kind, gmb_job_status status,
                         const char* message,
                         double train_loss, double hold_loss,
                         double base_train, double base_hold,
                         long greedy_before, long greedy_after,
                         long demo_changed, const char* ckpt)
{
    LOCK();
    int id = s_next_id++;
    if (s_next_id <= 0) s_next_id = 1; /* wrap */
    int slot = s_count < GMB_JOB_REGISTRY_CAP ? s_count : (s_count % GMB_JOB_REGISTRY_CAP);
    s_records[slot].id = id;
    s_records[slot].kind = kind;
    s_records[slot].status = status;
    s_records[slot].train_loss = train_loss;
    s_records[slot].hold_loss = hold_loss;
    s_records[slot].base_train = base_train;
    s_records[slot].base_hold = base_hold;
    s_records[slot].greedy_before = greedy_before;
    s_records[slot].greedy_after = greedy_after;
    s_records[slot].demo_changed = demo_changed;
    if (ckpt) snprintf(s_records[slot].ckpt, sizeof s_records[slot].ckpt, "%s", ckpt);
    else s_records[slot].ckpt[0] = 0;
    if (message) snprintf(s_records[slot].message, sizeof s_records[slot].message, "%s", message);
    else s_records[slot].message[0] = 0;
    s_count++;
    s_last_id = id;
    UNLOCK();
    return id;
}

int gmb_job_registry_last_id(void)
{
    return s_last_id;
}

const gmb_job_record* gmb_job_registry_get(int id)
{
    if (id <= 0) return NULL;
    for (int i = 0; i < s_count && i < GMB_JOB_REGISTRY_CAP; i++) {
        if (s_records[i].id == id) return &s_records[i];
    }
    return NULL;
}

int gmb_job_registry_count(void)
{
    return s_count < GMB_JOB_REGISTRY_CAP ? s_count : GMB_JOB_REGISTRY_CAP;
}

const gmb_job_record* gmb_job_registry_at(int index)
{
    if (index < 0) return NULL;
    if (index >= gmb_job_registry_count()) return NULL;
    return &s_records[index];
}

void gmb_job_registry_clear(void)
{
    LOCK();
    memset(s_records, 0, sizeof s_records);
    s_count = 0;
    s_last_id = 0;
    UNLOCK();
}

/* Rimuove la run con `id` (shift); ritorna 1 se trovata/rimossa, 0 altrimenti. TRAIN STOP. */
int gmb_job_registry_remove(int id)
{
    if (id <= 0) return 0;
    LOCK();
    int n = s_count < GMB_JOB_REGISTRY_CAP ? s_count : GMB_JOB_REGISTRY_CAP;
    int pos = -1;
    for (int i = 0; i < n; i++) { if (s_records[i].id == id) { pos = i; break; } }
    if (pos < 0) { UNLOCK(); return 0; }
    for (int i = pos; i < n - 1; i++) s_records[i] = s_records[i + 1];
    if (n > 0) memset(&s_records[n - 1], 0, sizeof s_records[0]);
    s_count--;
    if (s_last_id == id) s_last_id = (n - 1 > 0) ? s_records[n - 2].id : 0;
    UNLOCK();
    return 1;
}