/* ============================================================================
 * gmb_c99gpm.h — GPM C99 come libreria del runtime condiviso (piano C99Native,
 * FASE 2.6a). API pubblica del prologo goroutine: coroutine stackful, canali,
 * waitgroup, select beacon, sync primitives Go (mutex/rwmutex/once/cond/pool/
 * atomic/syncmap/context/future). Implementazione: src/gmb_c99gpm.c (porting
 * 1:1 del prologo di C99CodeEmitter.cs).
 * ========================================================================== */
#ifndef GMB_C99GPM_H
#define GMB_C99GPM_H

#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include "gmb_runtime.h"   /* gmb_val_t, gmb_string_t */

#ifdef __cplusplus
extern "C" {
#endif

/* --- Portability layer (specchio dell'emissione prologo, solo typedef) --- */
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    typedef SRWLOCK gmb_mutex_t;
    typedef CONDITION_VARIABLE gmb_cond_t;
    typedef HANDLE gmb_thread_t;
    typedef LPVOID gmb_ctx_t;             /* Fiber (coroutine Win32) */
    typedef struct { volatile LONG locked; } gmb_lock_t;
    #define GMB_THREAD_CREATE(t,fn,arg) ((*(t) = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)(fn),(arg),0,NULL)) != NULL)
    #define GMB_THREAD_JOIN(t)   WaitForSingleObject((t),INFINITE)
    #define GMB_THREAD_DETACH(t) CloseHandle(*(t))
#else
    #include <pthread.h>
    typedef pthread_mutex_t gmb_mutex_t;
    typedef pthread_cond_t gmb_cond_t;
    typedef pthread_t gmb_thread_t;
    #if defined(__aarch64__)
    typedef struct { void* sp; void* pc;
        unsigned long x19,x20,x21,x22,x23,x24,x25,x26,x27,x28,x29;
        double d8,d9,d10,d11,d12,d13,d14,d15;
    } gmb_ctx_t;
    #elif defined(__x86_64__)
    typedef struct { void* sp; void* pc;
        unsigned long rbx,rbp,r12,r13,r14,r15;
    } gmb_ctx_t;
    #else
    #include <ucontext.h>
    typedef ucontext_t gmb_ctx_t;
    #endif
    typedef struct { volatile int locked; } gmb_lock_t;
    #define GMB_THREAD_CREATE(t,fn,arg) (pthread_create((t),NULL,(fn),(arg)) == 0)
    #define GMB_THREAD_JOIN(t)   pthread_join((t),NULL)
    #define GMB_THREAD_DETACH(t) pthread_detach(*(t))
#endif

/* Macro mutex/cond usate anche dal codice emesso (C99CodeEmitter.Rete.cs). */
#if defined(_WIN32) || defined(_WIN64)
    #define GMB_MUTEX_INIT(m)    InitializeSRWLock(m)
    #define GMB_MUTEX_LOCK(m)    AcquireSRWLockExclusive(m)
    #define GMB_MUTEX_UNLOCK(m)  ReleaseSRWLockExclusive(m)
    #define GMB_COND_INIT(c)     InitializeConditionVariable(c)
    #define GMB_COND_SIGNAL(c)   WakeConditionVariable(c)
    #define GMB_COND_BROADCAST(c) WakeAllConditionVariable(c)
    #define GMB_COND_WAIT(c,m)   SleepConditionVariableSRW((c),(m),INFINITE,0)
#else
    #define GMB_MUTEX_INIT(m)    pthread_mutex_init((m),NULL)
    #define GMB_MUTEX_LOCK(m)    pthread_mutex_lock(m)
    #define GMB_MUTEX_UNLOCK(m)  pthread_mutex_unlock(m)
    #define GMB_COND_INIT(c)     pthread_cond_init((c),NULL)
    #define GMB_COND_SIGNAL(c)   pthread_cond_signal(c)
    #define GMB_COND_BROADCAST(c) pthread_cond_broadcast(c)
    #define GMB_COND_WAIT(c,m)   pthread_cond_wait((c),(m))
#endif

/* --- Tipi GPM (struct interne opache salvo gmb_task, usata dal codice emesso) --- */
typedef struct gmb_chan gmb_chan_t;
typedef struct gmb_wg gmb_wg_t;
typedef struct gmb_future gmb_future_t;
typedef struct gmb_gmutex gmb_gmutex_t;
typedef struct gmb_grwmutex gmb_grwmutex_t;
typedef struct gmb_gonce gmb_gonce_t;
typedef struct gmb_atomic gmb_atomic_t;
typedef struct gmb_pool gmb_pool_t;
typedef struct gmb_syncmap gmb_syncmap_t;
typedef struct gmb_gcond gmb_gcond_t;
typedef struct gmb_gcontext gmb_gcontext_t;
typedef struct gmb_task gmb_task_t;
typedef struct gmb_sel_state gmb_sel_state_t;

/* SELECT (protocollo beacon): il codice emesso dal transpiler dereferenzia i
   campi (winner/winner_val/winner_ok) — definizione completa, non opaca. */
struct gmb_sel_state { volatile int settled; volatile int winner; volatile int winner_ok; gmb_val_t winner_val; volatile int refcnt; };

/* Future (GO FUNCTION ... INTO): il wrapper FETCH emesso dal transpiler
   dereferenzia ->value — definizione completa, non opaca. */
struct gmb_future { gmb_lock_t lock; int state; gmb_val_t value; int err_num; char* err_desc; gmb_task_t* waiters; };

#define GMB_TS_RUNNABLE 0
#define GMB_TS_RUNNING  1
#define GMB_TS_BLOCKED  2
#define GMB_TS_FINISHED 3

/* GO EXIT: catena dei FINALLY attivi del task (parita' runtime.Goexit). */
typedef struct gmb_unwind_frame {
    jmp_buf buf;
    struct gmb_unwind_frame* prev;
} gmb_unwind_frame_t;

struct gmb_task {
    gmb_ctx_t ctx;
    void (*entry_fn)(void*);
    void* entry_arg;
    int state;
    int in_queue;
    int executing;
    int wake_pending;
    int was_yield;
    int sync_granted;
    volatile int xfer_done;
    gmb_unwind_frame_t* unwind_top;
    int unwinding;
    int returning;
    int ret_cont;
    jmp_buf* err_jmp;
    jmp_buf* err_resume;
    gmb_wg_t* wg;
    /* Gruppo implicito dei figli senza IN (WAIT nuda): creato al primo figlio
     * (bug 2026-10-01). Vive quanto il task (mai liberato prima, come i wg). */
    gmb_wg_t* implicit_wg;
    long long sleep_until_ms;
    gmb_task_t* next;
    gmb_task_t* all_next;
    gmb_lock_t lock;
    char* stack;
    size_t stack_size;
    void* stack_map;
    size_t stack_map_len;
};

/* --- Scheduler / avvio --- */
void gmb_gpm_start(void (*main_entry)(void*));
/* 1 se lo scheduler GPM è attivo (tra gmb_gpm_start e lo shutdown). */
int  gmb_c99gpm_active(void);
int gmb_gomaxprocs(int n);
int gmb_ncpu(void);                      /* NUMCPU builtin (def. per-platform in gmb_c99gpm.c) */
int gmb_c99_sched_active_count(void);
int gmb_c99_sched_has_active(void);
int64_t gmb_c99_now_ms(void);

/* --- Goroutine --- */
void gmb_task_spawn(void (*fn)(void*), void* arg);
void gmb_yield_ticks(long long ms);
void gmb_yield(void);
void gmb_task_exit(void);
void gmb_goexit(void);
void gmb_unwind_continue(void);
gmb_task_t* gmb_cur(void);
void gmb_block(void);

/* --- Canali --- */
gmb_chan_t* gmb_chan_new(int cap);
void gmb_chan_send(gmb_chan_t* c, gmb_val_t v);
gmb_val_t gmb_chan_recv(gmb_chan_t* c);
void gmb_chan_recv_ok(gmb_chan_t* c, gmb_val_t* out, int* ok_out);
int gmb_chan_tryrecv(gmb_chan_t* c, gmb_val_t* out, int* ok_out);
int gmb_chan_trysend(gmb_chan_t* c, gmb_val_t v);
void gmb_chan_close(gmb_chan_t* c);
int gmb_chan_send_or_park(gmb_chan_t* c, gmb_val_t v, gmb_task_t** woken);
int gmb_chan_recv_or_park(gmb_chan_t* c, gmb_val_t* out, int* ok_out, gmb_task_t** woken);

/* --- SELECT (protocollo beacon) --- */
gmb_sel_state_t* gmb_select_park(gmb_chan_t** chans, int* dir, gmb_val_t* send_vals, int n, long long deadline_ms);
void gmb_sel_deref(gmb_sel_state_t* s);

/* --- WaitGroup --- */
gmb_wg_t* gmb_wg_new(void);
void gmb_c99_wg_add(gmb_wg_t* w, int n);
void gmb_c99_wg_done(gmb_wg_t* w);
void gmb_wg_wait(gmb_wg_t* w);
/* Gruppo implicito dei figli lanciati con GO SUB/FUNCTION senza IN (WAIT nuda,
 * bug 2026-10-01): come i waitgroup espliciti ma senza handle BASIC. Il gruppo
 * vive quanto il task genitore (mai liberato prima, come gli espliciti). */
gmb_wg_t* gmb_implicit_wg_get(void);   /* del task corrente; crea se assente */
void gmb_implicit_wait(void);          /* wait sul gruppo implicito (no-op se assente) */

/* --- Context --- */
gmb_gcontext_t* gmb_ctx_create(gmb_gcontext_t* parent);
gmb_gcontext_t* gmb_ctx_background(void);
gmb_gcontext_t* gmb_ctx_withcancel(gmb_gcontext_t* p);
gmb_chan_t* gmb_ctx_done(gmb_gcontext_t* c);
void gmb_ctx_cancel(gmb_gcontext_t* c);
int gmb_ctx_cancelled(gmb_gcontext_t* c);

/* --- Sync primitives (parita' Go sync/atomic) --- */
gmb_gmutex_t* gmb_gmutex_new(void);
void gmb_gmutex_lock(gmb_gmutex_t* m);
void gmb_gmutex_unlock(gmb_gmutex_t* m);
int gmb_gmutex_trylock(gmb_gmutex_t* m);
gmb_grwmutex_t* gmb_grwmutex_new(void);
void gmb_grwmutex_rlock(gmb_grwmutex_t* m);
void gmb_grwmutex_runlock(gmb_grwmutex_t* m);
void gmb_grwmutex_lock(gmb_grwmutex_t* m);
void gmb_grwmutex_unlock(gmb_grwmutex_t* m);
int gmb_grwmutex_trylock(gmb_grwmutex_t* m);
gmb_gonce_t* gmb_gonce_new(void);
void gmb_gonce_run(gmb_gonce_t* o, void (*fn)(void));
gmb_gcond_t* gmb_gcond_new(void);
void gmb_gcond_wait(gmb_gcond_t* c, void* mu, int kind);
void gmb_gcond_signal(gmb_gcond_t* c);
void gmb_gcond_broadcast(gmb_gcond_t* c);
gmb_atomic_t* gmb_atomic_new(void);
void gmb_c99_atomic_store(gmb_atomic_t* a, long long v);
long long gmb_atomic_fetch(gmb_atomic_t* a);
long long gmb_atomic_inc(gmb_atomic_t* a, long long d);
long long gmb_c99_atomic_swap(gmb_atomic_t* a, long long newv);
int gmb_c99_atomic_cas(gmb_atomic_t* a, long long oldv, long long newv);
gmb_pool_t* gmb_pool_new(void);
void gmb_c99_pool_put(gmb_pool_t* p, gmb_val_t v);
gmb_val_t gmb_c99_pool_get(gmb_pool_t* p);
gmb_syncmap_t* gmb_syncmap_new(void);
void gmb_syncmap_store(gmb_syncmap_t* m, const char* k, gmb_val_t v);
gmb_val_t gmb_syncmap_load(gmb_syncmap_t* m, const char* k);
gmb_val_t gmb_syncmap_delete(gmb_syncmap_t* m, const char* k);
gmb_val_t gmb_syncmap_load_or_store(gmb_syncmap_t* m, const char* k, gmb_val_t v);

/* --- Future (GO FUNCTION ... INTO) --- */
gmb_future_t* gmb_future_new(void);
void gmb_c99_future_resolve(gmb_future_t* f, gmb_val_t v);
void gmb_c99_future_fail(gmb_future_t* f, int err_num, const char* err_desc);
void gmb_future_cancel(gmb_future_t* f);
void gmb_future_await(gmb_future_t* f);
void gmb_future_check_error(gmb_future_t* f);

/* --- Job in volo su thread OS non-GPM (FETCH e affini) ---
 * Un future risolto da un pthread detached (non-GPM) lascia la GPM "cieca": coda vuota,
 * nessuno in esecuzione, nessun timer/iowatch → il watchdog anti-deadlock dichiarerebbe
 * un deadlock falso. Questi tre marcatori lo tengono fermofinché il job è in volo.
 * Uso canonico (identico al coarse-park SQL e al GPM VM/C): il thread chiama
 * gmb_c99gpm_extjob_begin() per PRIMA cosa e gmb_c99gpm_extjob_end() dopo la resolve. */
void gmb_c99gpm_extjob_begin(void);
void gmb_c99gpm_extjob_end(void);
int  gmb_c99gpm_extjob_count(void);

/* --- Helper valori --- */
gmb_string_t gmb_val_str_or_empty(gmb_val_t v);

/* WAITKEY/GET_KEY/INKEY park-based per coroutine GPM: poll non bloccante
 * (gmb_console_readkey_nb) + gmb_yield_ticks(20) quando il tasto non è pronto —
 * il worker NON si blocca (le altre goroutine proseguono anche a GOMAXPROCS=1).
 * timeout_ms<=0 = attesa indefinita; >0 = deadline. Ritorna il tasto (sempre
 * allocato; "" se timeout/nessun terminale). Mai spin-wait. */
gmb_string_t gmb_key_wait_park(int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* GMB_C99GPM_H */
