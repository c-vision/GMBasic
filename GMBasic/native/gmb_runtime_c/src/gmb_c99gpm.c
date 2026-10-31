/* gmb_c99gpm.c — GPM C99 come libreria del runtime condiviso (FASE 2.6a del
   piano C99Native). Porting 1:1 del prologo emesso da C99CodeEmitter.cs:
   worker pthread/fiber, coroutine stackful, canali retry-based, select beacon,
   sync primitives Go.
   GENERATO: non modificare a mano — rigenerare con tools/gmb_c99gpm_gen.py.
   Rispetto al testo originale sono soppresse solo le dichiarazioni duplicate
   con include/gmb_c99gpm.h (typedef di piattaforma, forward typedef, GMB_TS_*,
   struct gmb_task, gmb_unwind_frame) e tolto `static` solo all'API pubblica. */
#define _XOPEN_SOURCE 600
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE 1
#endif
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "gmb_c99gpm.h"
#include "gmb_tray.h"
#include "gmb_ui_gfx.h"
#include "gmb_iconsole.h"
/* ===== Portability layer (POSIX vs Windows) ===== */
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    typedef LPVOID gmb_ctx_t;            /* Fiber (coroutine Win32) */
    static void gmb_lock(gmb_lock_t* m) { while (_InterlockedExchange(&m->locked,1)) { ; } }
    static void gmb_unlock(gmb_lock_t* m) { _InterlockedExchange(&m->locked,0); }
    int64_t gmb_c99_now_ms(void) { return (int64_t)GetTickCount64(); }
    /* gmb_sleep_ms: fornita dal runtime (gmb_console.c, cross-platform) */
    int gmb_ncpu(void) { return (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS); }
#else
    #include <pthread.h>
    #include <ucontext.h>
    #include <time.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <signal.h>
    /* Context switch in ASSEMBLY (stile Boost.Context/libco/gogo Go) —
       nessuna dipendenza da ucontext (deprecato su macOS). Salva SP +
       registri callee-saved e salta, come il gogo di Go. Fallback ucontext
       solo per arch esotiche non supportate. */
#if defined(__aarch64__)
#elif defined(__x86_64__)
#else
    #include <ucontext.h>
#endif
#if defined(__aarch64__)
    __attribute__((naked)) static void gmb_ctx_switch(gmb_ctx_t* from, gmb_ctx_t* to) {
        __asm__ __volatile__ (
            "stp x19, x20, [sp, #-16]!\n"
            "stp x21, x22, [sp, #-16]!\n"
            "stp x23, x24, [sp, #-16]!\n"
            "stp x25, x26, [sp, #-16]!\n"
            "stp x27, x28, [sp, #-16]!\n"
            "stp d8, d9, [sp, #-16]!\n"
            "stp d10, d11, [sp, #-16]!\n"
            "stp d12, d13, [sp, #-16]!\n"
            "stp d14, d15, [sp, #-16]!\n"
            "stp x29, x30, [sp, #-16]!\n"
            "mov x2, sp\n"
            "str x2, [x0]\n"
            "str x30, [x0, #8]\n"
            "ldr x2, [x1]\n"
            "mov sp, x2\n"
            "ldp x29, x30, [sp], #16\n"
            "ldp d14, d15, [sp], #16\n"
            "ldp d12, d13, [sp], #16\n"
            "ldp d10, d11, [sp], #16\n"
            "ldp d8, d9, [sp], #16\n"
            "ldp x27, x28, [sp], #16\n"
            "ldp x25, x26, [sp], #16\n"
            "ldp x23, x24, [sp], #16\n"
            "ldp x21, x22, [sp], #16\n"
            "ldp x19, x20, [sp], #16\n"
            "ldr x30, [x1, #8]\n"
            "br x30\n"
            : : : "memory");
    }
    static void gmb_ctx_make(gmb_ctx_t* ctx, void (*fn)(void), char* stack, size_t sz) {
        unsigned long* sp = (unsigned long*)(stack + sz);
        sp -= 20;
        sp[0]=0; sp[1]=(unsigned long)fn;   /* x29, x30=resume pc */
        sp[2]=0; sp[3]=0; sp[4]=0; sp[5]=0; sp[6]=0; sp[7]=0; sp[8]=0; sp[9]=0;
        sp[10]=0; sp[11]=0; sp[12]=0; sp[13]=0; sp[14]=0; sp[15]=0; sp[16]=0; sp[17]=0; sp[18]=0; sp[19]=0;
        ctx->sp = sp; ctx->pc = (void*)fn;
    }
#elif defined(__x86_64__)
    __attribute__((naked)) static void gmb_ctx_switch(gmb_ctx_t* from, gmb_ctx_t* to) {
        __asm__ __volatile__ (
            ".intel_syntax noprefix\n"
            "push rbp\n"
            "push rbx\n"
            "push r12\n"
            "push r13\n"
            "push r14\n"
            "push r15\n"
            "mov [rdi], rsp\n"
            "mov rax, [rsp+48]\n"
            "mov [rdi+8], rax\n"
            "mov rsp, [rsi]\n"
            "mov rax, [rsi+8]\n"
            "pop r15\n"
            "pop r14\n"
            "pop r13\n"
            "pop r12\n"
            "pop rbx\n"
            "pop rbp\n"
            "jmp rax\n"
            ".att_syntax prefix\n"
            : : : "memory");
    }
    static void gmb_ctx_make(gmb_ctx_t* ctx, void (*fn)(void), char* stack, size_t sz) {
        void** sp = (void**)(stack + sz);
        sp -= 7;
        sp[0]=0; sp[1]=0; sp[2]=0; sp[3]=0; sp[4]=0; sp[5]=0;
        sp[6]=(void*)fn;   /* return address = trampoline */
        ctx->sp = sp; ctx->pc = (void*)fn;
    }
#else
    static void gmb_ctx_make(gmb_ctx_t* ctx, void (*fn)(void), char* stack, size_t sz) {
        getcontext(ctx); ctx->uc_stack.ss_sp = stack;
        ctx->uc_stack.ss_size = sz; ctx->uc_link = NULL;
        makecontext(ctx, fn, 0);
    }
    static void gmb_ctx_switch(gmb_ctx_t* from, gmb_ctx_t* to) { swapcontext(from, to); }
#endif
    static void gmb_lock(gmb_lock_t* m) { while (__sync_lock_test_and_set(&m->locked,1)) { ; } }
    static void gmb_unlock(gmb_lock_t* m) { __sync_lock_release(&m->locked); }
    int64_t gmb_c99_now_ms(void) {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
    /* gmb_sleep_ms: fornita dal runtime (gmb_console.c, cross-platform) */
    /* _XOPEN_SOURCE 600 nasconde _SC_NPROCESSORS_ONLN su macOS/Linux:
       definizione esplicita del valore numerico (Darwin 58, Linux 84). */
#ifndef _SC_NPROCESSORS_ONLN
#if defined(__APPLE__)
    #define _SC_NPROCESSORS_ONLN 58
#elif defined(__linux__)
    #define _SC_NPROCESSORS_ONLN 84
#endif
#endif
    int gmb_ncpu(void) { int n = 0;
#ifdef _SC_NPROCESSORS_ONLN
        n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
        return n > 0 ? n : 1; }
#endif

/* --- GPM §16: forward decls --- */
typedef struct gmb_scheduler gmb_scheduler_t;


/* GO EXIT (parità runtime.Goexit): catena dei FINALLY attivi del task.
   Ogni TRY/FINALLY registra un frame; gmb_goexit fa longjmp al più interno,
   il FINALLY esegue (sospendibile) e gmb_unwind_continue passa all'esterno. */


struct gmb_scheduler {
    gmb_task_t** queue; int qcap, qhead, qtail, qcount;
    gmb_mutex_t qlock; gmb_cond_t qcond;
    int shutdown;
    gmb_task_t** all; int all_cap, all_count;
    gmb_mutex_t all_lock;
};
static gmb_scheduler_t g_sched;
static volatile int g_gpm_active = 0;   /* 1 tra gmb_gpm_start e lo shutdown: scheduler GPM attivo */
int gmb_c99gpm_active(void) { return g_gpm_active; }
#if defined(_WIN32) || defined(_WIN64)
static __thread LPVOID g_worker_fiber;   /* fiber del worker (resume point) */
#else
static __thread gmb_ctx_t g_worker_ctx;
#endif
static __thread gmb_task_t* g_current;
/* Rilettura TLS FRESCA di g_current: il compilatore cachea la variabile
   __thread in un registro che swapcontext ripristina col valore STALE/NULL
   dopo il resume (macOS TLS+ucontext) → tutte le funzioni coroutine usano
   una variabile locale inizializzata da gmb_cur(), mai il globale diretto. */
__attribute__((noinline)) gmb_task_t* gmb_cur(void) { return g_current; }

#if defined(_WIN32) || defined(_WIN64)
static void __stdcall gmb_task_trampoline(void* _p);
#else
static void gmb_task_trampoline(void);
#endif

/* --- Enqueue (replica gmb_sched_enqueue) --- */
static void gmb_sched_enqueue(gmb_task_t* t) {
    gmb_lock(&t->lock);
    if (t->state == GMB_TS_FINISHED) { gmb_unlock(&t->lock); return; }
    if (t->executing) { t->wake_pending = 1; gmb_unlock(&t->lock); return; }
    if (t->in_queue) { gmb_unlock(&t->lock); return; }
    t->in_queue = 1;
    gmb_unlock(&t->lock);
    t->state = GMB_TS_RUNNABLE;
    GMB_MUTEX_LOCK(&g_sched.qlock);
    if (g_sched.qcount >= g_sched.qcap) {
        int ncap = g_sched.qcap ? g_sched.qcap * 2 : 64;
        gmb_task_t** nq = (gmb_task_t**)malloc(ncap * sizeof(gmb_task_t*));
        int n = 0; for (int i = 0; i < g_sched.qcount; i++) nq[n++] = g_sched.queue[(g_sched.qhead + i) % g_sched.qcap];
        free(g_sched.queue); g_sched.queue = nq; g_sched.qcap = ncap; g_sched.qhead = 0; g_sched.qtail = n;
    }
    if (!g_sched.shutdown) {
        g_sched.queue[g_sched.qtail] = t;
        g_sched.qtail = (g_sched.qtail + 1) % g_sched.qcap;
        g_sched.qcount++;
        GMB_COND_SIGNAL(&g_sched.qcond);
    }
    GMB_MUTEX_UNLOCK(&g_sched.qlock);
}

/* --- FASE 2: stack configurabile + guard page (2026-08-18) --- */
/* Dimensione stack per task: GMBASIC_GOROUTINE_STACK (byte) o default 4MB.
   Con overcommit il costo reale è solo per le pagine toccate (non 4MB fisici
   per goroutine): l'allocazione mmap è lazy. */
static size_t gmb_stack_default_size(void) {
    const char* env = getenv("GMBASIC_GOROUTINE_STACK");
    if (env && *env) {
        long long v = atoll(env);
        if (v >= (1 << 16)) return (size_t)v;   /* min 64KB */
    }
    return (size_t)(4 << 20);   /* default 4MB */
}
#if defined(_WIN32) || defined(_WIN64)
/* Windows: lo stack è gestito dalla Fiber (CreateFiber prende la dimensione);
   niente guard page manuale — l'OS protegge lo stack della fiber. */
#else
/* Alloca lo stack con GUARD PAGE POSIX: mmap stack+1 pagina, la pagina in cima
   (indirizzi più bassi, verso cui cresce SP) è PROT_NONE. Un overflow tocca la
   guard page → SIGSEGV rilevabile (handler sotto) invece di corruzione muta.
   Layout: [GUARD PROT_NONE | stack RW (size byte)] — stack cresce in basso. */
static void gmb_stack_alloc(size_t size, char** stack_out, void** map_out, size_t* map_len_out) {
    long pg = sysconf(_SC_PAGESIZE);
    if (pg <= 0) pg = 4096;
    size_t map_len = size + (size_t)pg;
/* su macOS con _XOPEN_SOURCE 600 né MAP_ANON né MAP_ANONYMOUS sono esposti:
   MAP_ANON è costante di piattaforma (0x1000 su Darwin). Linux espone
   MAP_ANONYMOUS. Copertura cross-platform senza _DARWIN_C_SOURCE (che
   altera il layout di ucontext_t e rompe g_worker_ctx nell'assembly). */
#ifndef MAP_ANONYMOUS
    #ifndef MAP_ANON
        #define MAP_ANON 0x1000
    #endif
    #define MAP_ANONYMOUS MAP_ANON
#endif
    char* base = (char*)mmap(NULL, map_len, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) { *stack_out = NULL; *map_out = NULL; *map_len_out = 0; return; }
    if (mprotect(base, (size_t)pg, PROT_NONE) != 0) { munmap(base, map_len);
        *stack_out = NULL; *map_out = NULL; *map_len_out = 0; return; }
    *stack_out = base + pg;      /* zona stack usabile */
    *map_out = base;             /* per munmap a fine task */
    *map_len_out = map_len;
}
/* Rilevatore overflow: SIGSEGV nella guard page → errore chiaro, non corruzione.
   Solo la task corrente può toccare la propria guard page; l'handler valuta la
   guard della task corrrente (g_current) e, se l'indirizzo del fault è dentro,
   stampa e termina. Altrimenti ripristina il comportamento default. */
static void gmb_stack_fault_handler(int sig, siginfo_t* si, void* uctx) {
    (void)sig; (void)uctx;
    gmb_task_t* g = gmb_cur();
    /* Il fault può cadere ESATTAMENTE sul confine guard/stack (SP scende fino al
       primo byte PROT_NONE): il range copre l'intero mapping dello stack
       (guard + zona usabile) — l'unica parte non accessibile è la guard, quindi
       un SIGSEGV qui è per definizione overflow della coroutine. */
    if (g && g->stack_map && si->si_addr >= g->stack_map
        && (char*)si->si_addr <= (char*)g->stack_map + g->stack_map_len) {
        fprintf(stderr, "[GMBASIC] STACK OVERFLOW coroutine: aumentare GMBASIC_GOROUTINE_STACK"
            " (attuale %zu byte)\n", g->stack_size);
        _Exit(2);
    }
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}
static void gmb_stack_install_handler(void) {
    struct sigaction sa = { 0 };
    sa.sa_sigaction = gmb_stack_fault_handler;
    sa.sa_flags = SA_SIGINFO;
    /* su macOS una pagina PROT_NONE genera SIGBUS, non SIGSEGV — serve entrambi */
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}
#endif

void gmb_task_spawn(void (*fn)(void*), void* arg) {
    gmb_task_t* t = (gmb_task_t*)calloc(1, sizeof(gmb_task_t));
    size_t sz = gmb_stack_default_size();
    t->stack_size = sz;
    t->entry_fn = fn; t->entry_arg = arg; t->state = GMB_TS_RUNNABLE;
#if defined(_WIN32) || defined(_WIN64)
    t->stack = NULL; t->stack_map = NULL;
    t->ctx = CreateFiber((SIZE_T)sz, (LPFIBER_START_ROUTINE)gmb_task_trampoline, NULL);
#else
    gmb_stack_alloc(sz, &t->stack, &t->stack_map, &t->stack_map_len);
    gmb_ctx_make(&t->ctx, gmb_task_trampoline, t->stack, sz);
#endif
    GMB_MUTEX_LOCK(&g_sched.all_lock);
    if (g_sched.all_count >= g_sched.all_cap) {
        int ncap = g_sched.all_cap ? g_sched.all_cap * 2 : 64;
        g_sched.all = (gmb_task_t**)realloc(g_sched.all, ncap * sizeof(gmb_task_t*));
        g_sched.all_cap = ncap;
    }
    g_sched.all[g_sched.all_count++] = t;
    GMB_MUTEX_UNLOCK(&g_sched.all_lock);
    gmb_sched_enqueue(t);
}
#if defined(_WIN32) || defined(_WIN64)
void __stdcall gmb_task_trampoline(void* _p) { gmb_task_t* g_current = gmb_cur();
    g_current->entry_fn(g_current->entry_arg);
    gmb_lock(&g_current->lock);
    g_current->state = GMB_TS_FINISHED;
    gmb_unlock(&g_current->lock);
    SwitchToFiber(g_worker_fiber);
}
#else
void gmb_task_trampoline(void) { gmb_task_t* g_current = gmb_cur();
    g_current->entry_fn(g_current->entry_arg);
    gmb_lock(&g_current->lock);
    g_current->state = GMB_TS_FINISHED;
    gmb_unlock(&g_current->lock);
    gmb_ctx_switch(&g_current->ctx, &g_worker_ctx);
}
#endif

void gmb_block(void) { gmb_task_t* g_current = gmb_cur();
    /* Salva la catena handler error TLS nel task: al resume il worker può essere
       DIVERSO (task migration) e il __thread state non segue il task. */
    g_current->err_jmp = gmb_err_set_handler(NULL);
    g_current->err_resume = gmb_err_resume_jmp();
#if defined(_WIN32) || defined(_WIN64)
    SwitchToFiber(g_worker_fiber);
#else
    gmb_ctx_switch(&g_current->ctx, &g_worker_ctx);
#endif
    gmb_err_set_handler(g_current->err_jmp);
    gmb_err_set_resume(g_current->err_resume);
}
void gmb_yield(void) { gmb_task_t* g_current = gmb_cur(); g_current->was_yield = 1;
    g_current->err_jmp = gmb_err_set_handler(NULL);
    g_current->err_resume = gmb_err_resume_jmp();
#if defined(_WIN32) || defined(_WIN64)
    SwitchToFiber(g_worker_fiber);
#else
    gmb_ctx_switch(&g_current->ctx, &g_worker_ctx);
#endif
    gmb_err_set_handler(g_current->err_jmp);
    gmb_err_set_resume(g_current->err_resume);
}
void gmb_yield_ticks(long long ms) { gmb_task_t* g_current = gmb_cur();
    if (ms <= 0) return;   /* §10: SLEEP<=0 prosegue senza bloccare */
    gmb_lock(&g_current->lock);
    g_current->state = GMB_TS_BLOCKED; g_current->sleep_until_ms = gmb_c99_now_ms() + ms;
    gmb_unlock(&g_current->lock);
    gmb_block();
}

int gmb_c99_sched_has_active(void) {
    int active = 0;
    GMB_MUTEX_LOCK(&g_sched.all_lock);
    for (int i = 0; i < g_sched.all_count; i++) if (g_sched.all[i]->state != GMB_TS_FINISHED) { active = 1; break; }
    GMB_MUTEX_UNLOCK(&g_sched.all_lock);
    return active;
}
static gmb_task_t* gmb_sched_find_runnable(void) {
    GMB_MUTEX_LOCK(&g_sched.qlock);
    while (g_sched.qcount == 0 && !g_sched.shutdown)
        GMB_COND_WAIT(&g_sched.qcond, &g_sched.qlock);
    if (g_sched.qcount == 0) { GMB_MUTEX_UNLOCK(&g_sched.qlock); return NULL; }
    gmb_task_t* t = g_sched.queue[g_sched.qhead];
    g_sched.qhead = (g_sched.qhead + 1) % g_sched.qcap;
    g_sched.qcount--;
    GMB_MUTEX_UNLOCK(&g_sched.qlock);
    gmb_lock(&t->lock); t->in_queue = 0; t->executing = 1; gmb_unlock(&t->lock);
    return t;
}
static void gmb_sched_release_executing(gmb_task_t* t) {
    int rewake;
    gmb_lock(&t->lock);
    t->executing = 0; rewake = t->wake_pending; t->wake_pending = 0;
    gmb_unlock(&t->lock);
    if (rewake) gmb_sched_enqueue(t);
}

static int64_t gmb_sched_earliest_sleep(void) {
    int64_t earliest = -1;
    GMB_MUTEX_LOCK(&g_sched.all_lock);
    for (int i = 0; i < g_sched.all_count; i++) {
        gmb_task_t* t = g_sched.all[i];
        gmb_lock(&t->lock);
        if (t->state == GMB_TS_BLOCKED && t->sleep_until_ms > 0 && (earliest < 0 || t->sleep_until_ms < earliest)) earliest = t->sleep_until_ms;
        gmb_unlock(&t->lock);
    }
    GMB_MUTEX_UNLOCK(&g_sched.all_lock);
    return earliest;
}
/* Coarse-park SQL (op pesanti): hook registrato dal backend C99 (sempre attivo,
 * parità con IL/PCode/VM-C). Esegue le scritture che riscrivono file (vtab csv/parquet) fuori dal worker.
 * Il contatore sopprime il watchdog mentre un job e' in volo. */
static volatile int g_sql_async_jobs = 0;
/* Job in volo su THREAD OS non-GPM (FETCH e affini: l'emitter C99/C99Native crea un
 * pthread detached che chiama gmb_c99_future_resolve da fuori dalla GPM). Il watchdog NON vede
 * quei thread: durante l'attesa la coda e' vuota, nessun task e' in esecuzione, nessun
 * timer e nessun iowatch — condizioni che il watchdog scambia per un deadlock, uccidendo
 * il processo mentre l'attesa e' legittima. Il contatore lo tiene fermo.
 * Precedenti canonici: coarse-park SQL in QUESTO file (g_sql_async_jobs) e
 * gmb_sched_async_job_begin/end (async_job_count) del GPM VM/C.
 *Nota: il begin va Called dentro il thread (non prima della create): se pthread_create
 * fallisce il contatore non viene toccato e l'await resta l'unico difetto (pre-esistente). */
static volatile int g_ext_thread_jobs = 0;
void gmb_c99gpm_extjob_begin(void) { __sync_fetch_and_add(&g_ext_thread_jobs, 1); }
void gmb_c99gpm_extjob_end(void)   { __sync_fetch_and_sub(&g_ext_thread_jobs, 1); }
int  gmb_c99gpm_extjob_count(void) { return g_ext_thread_jobs; }

#if defined(_WIN32) || defined(_WIN64)
/* Su Windows la parte ASYNC del modulo SQL (pthread detached) resta nel ramo POSIX, ma la
 * DEFINIZIONE dell'interfaccia deve esistere comunque: la referenziano i consumer
 * (selfcheck SQL, C99Native). Zero = nessun modulo SQL installato, che è lo stato di default. */
#include "gmb_sql_iface.h"
gmb_sql_iface_t gmb_sql_iface;
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#include "gmb_rdd_db.h"
#include "gmb_sql_iface.h"

/* Definizione nel CORE (zero = nessun modulo SQL): il modulo SQL la riempie al load. */
gmb_sql_iface_t gmb_sql_iface;
typedef struct { gmb_task_t* task; char* alias; char* sql; char** params; int n; int rc; } c99_sql_job_t;
static void* c99_sql_job(void* a) {
    c99_sql_job_t* j = (c99_sql_job_t*)a;
    j->rc = gmb_sql_iface.exec_params_raw ? gmb_sql_iface.exec_params_raw(j->alias, j->sql, (const char* const*)j->params, j->n) : -1;
    __sync_fetch_and_sub(&g_sql_async_jobs, 1);
    gmb_sched_enqueue(j->task);
    return NULL;
}
static int c99_sql_park(const char* alias, const char* sql, const char* const* params, int n) {
    gmb_task_t* self = gmb_cur();
    c99_sql_job_t* j = (c99_sql_job_t*)calloc(1, sizeof(c99_sql_job_t));
    if (!j) return gmb_sql_iface.exec_params_raw ? gmb_sql_iface.exec_params_raw(alias, sql, params, n) : -1;
    j->task = self; j->alias = strdup(alias ? alias : ""); j->sql = strdup(sql ? sql : "");
    if (n > 0 && params) { j->params = (char**)calloc((size_t)n, sizeof(char*)); for (int i = 0; i < n; i++) j->params[i] = strdup(params[i] ? params[i] : ""); j->n = n; }
    __sync_fetch_and_add(&g_sql_async_jobs, 1);
    pthread_t th;
    if (pthread_create(&th, NULL, c99_sql_job, j) != 0) {
        __sync_fetch_and_sub(&g_sql_async_jobs, 1);
        free(j->alias); free(j->sql); if (j->params) { for (int i = 0; i < j->n; i++) free(j->params[i]); free(j->params); } free(j);
        return gmb_sql_iface.exec_params_raw ? gmb_sql_iface.exec_params_raw(alias, sql, params, n) : -1;
    }
    pthread_detach(th);
    self->state = GMB_TS_BLOCKED;
    gmb_block();
    int rc = j->rc;
    free(j->alias); free(j->sql); if (j->params) { for (int i = 0; i < j->n; i++) free(j->params[i]); free(j->params); } free(j);
    return rc;
}
#endif
/* Watchdog deadlock (parità Go "fatal error: all goroutines are asleep - deadlock!"):
 * true se la run-queue è vuota E nessun worker sta ticchettando un task in questo
 * istante. Il chiamante DEVE combinarlo con "nessun timer armato" e "nessun IO
 * osservato" — uno stuck momentaneo con un timer/IO pendente non è un deadlock. */
static int gmb_sched_is_stuck(void) {
    GMB_MUTEX_LOCK(&g_sched.qlock);
    int qc = g_sched.qcount;
    GMB_MUTEX_UNLOCK(&g_sched.qlock);
    if (qc > 0) return 0;
    int any_executing = 0;
    GMB_MUTEX_LOCK(&g_sched.all_lock);
    for (int i = 0; i < g_sched.all_count && !any_executing; i++) {
        gmb_task_t* t = g_sched.all[i];
        gmb_lock(&t->lock);
        if (t->state != GMB_TS_FINISHED && t->executing) any_executing = 1;
        gmb_unlock(&t->lock);
    }
    GMB_MUTEX_UNLOCK(&g_sched.all_lock);
    return !any_executing;
}
static void gmb_sched_wake_expired_sleepers(int64_t now_ms) {
    gmb_task_t** snap = (gmb_task_t**)malloc((size_t)(g_sched.all_count ? g_sched.all_count : 1) * sizeof(gmb_task_t*));
    int n = 0;
    GMB_MUTEX_LOCK(&g_sched.all_lock);
    for (int i = 0; i < g_sched.all_count; i++) if (g_sched.all[i]->state != GMB_TS_FINISHED) snap[n++] = g_sched.all[i];
    GMB_MUTEX_UNLOCK(&g_sched.all_lock);
    for (int i = 0; i < n; i++) {
        gmb_task_t* t = snap[i]; int exp = 0;
        gmb_lock(&t->lock);
        if (t->state == GMB_TS_BLOCKED && t->sleep_until_ms > 0 && t->sleep_until_ms <= now_ms) { t->sleep_until_ms = 0; exp = 1; }
        gmb_unlock(&t->lock);
        if (exp) gmb_sched_enqueue(t);
    }
    free(snap);
}

/* --- Worker loop (replica worker_loop di gmb_vm_core.c) --- */
static void* gmb_worker(void* arg) {
#if defined(_WIN32) || defined(_WIN64)
    g_worker_fiber = ConvertThreadToFiber(NULL);
#endif
    for (;;) {
        gmb_task_t* t = gmb_sched_find_runnable();
        if (!t) break;
        t->state = GMB_TS_RUNNING;
        g_current = t;
#if defined(_WIN32) || defined(_WIN64)
        SwitchToFiber(t->ctx);
#else
        gmb_ctx_switch(&g_worker_ctx, &t->ctx);
#endif
        g_current = NULL;
        if (t->state != GMB_TS_FINISHED && t->was_yield) {
            t->was_yield = 0;
            gmb_sched_enqueue(t);
        }
        gmb_sched_release_executing(t);
    }
    return NULL;
}
static void* gmb_sleep_watcher(void* arg) {
    (void)arg;
    /* Watchdog deadlock (parità Go): stessa condizione confermata per 2 giri
     * consecutivi (~2 * wait_ms, tipicamente ≤500ms) prima di dichiarare deadlock,
     * per escludere una race transitoria (un worker sta per fare un enqueue). */
    int stuck_streak = 0;
    while (gmb_c99_sched_has_active()) {
        gmb_sched_wake_expired_sleepers(gmb_c99_now_ms());
        /* IO WATCH: poll dei file osservati → spawn della callback SUB come task GPM.
         * gmb_iowatch_first_changed_fn ri-arma lo snapshot (re-arm). Fast-path O(1):
         * se nessuna path e' osservata, zero lavoro. */
        if (gmb_iowatch_count() > 0)
        {
            char iwp[1024]; void (*iwfn)(void*) = NULL;
            while (gmb_iowatch_first_changed_fn(iwp, sizeof(iwp), &iwfn)[0] != '\0')
            {
                if (!iwfn) continue;
                gmb_task_spawn(iwfn, NULL);
            }
        }
        int64_t next = gmb_sched_earliest_sleep();
        if (next < 0 && gmb_iowatch_count() == 0 && g_sql_async_jobs == 0
            && g_ext_thread_jobs == 0 && gmb_sched_is_stuck()) {
            if (++stuck_streak >= 2) {
                fprintf(stderr, "fatal error: all goroutines are asleep - deadlock!\n"
                                 "[GMBASIC] nessun task puo' piu' fare progressi (nessun task runnable/in "
                                 "esecuzione, nessun timer o I/O in attesa) -- probabile SEND senza un "
                                 "RECEIVE corrispondente, o un WAIT su un WAITGROUP mai completato.\n");
                fflush(stderr);
                _Exit(2);
            }
        } else stuck_streak = 0;
        int wait_ms = next < 0 ? 50 : (next - gmb_c99_now_ms() > 250 ? 250 : (int)(next - gmb_c99_now_ms()));
        if (wait_ms < 1) wait_ms = 1;
        gmb_sleep_ms(wait_ms);
    }
    return NULL;
}

/* --- FASE 5: SYSTEM.NUMGOROUTINE / SYSTEM.GOMAXPROCS (parità Go runtime) ---
   NUMGOROUTINE = task non FINISHED in g_sched.all. GOMAXPROCS(n) ritorna il
   valore precedente e cresce SOLO aggiungendo worker detached (mai rimossi,
   parità Go: ridurre GOMAXPROCS non uccide thread). */
static int g_gomaxprocs = 0;
static gmb_thread_t g_extra_workers[256];
static int g_extra_workers_n = 0;
static gmb_mutex_t g_gmp_lock;
int gmb_c99_sched_active_count(void) {
    int n = 0;
    GMB_MUTEX_LOCK(&g_sched.all_lock);
    for (int i = 0; i < g_sched.all_count; i++) if (g_sched.all[i]->state != GMB_TS_FINISHED) n++;
    GMB_MUTEX_UNLOCK(&g_sched.all_lock);
    return n;
}
int gmb_gomaxprocs(int n) {
    GMB_MUTEX_LOCK(&g_gmp_lock);
    int prev = g_gomaxprocs;
    if (n > 0) {   /* n<=0 = sola query (parità Go runtime.GOMAXPROCS(0)) */
        if (n > g_gomaxprocs) {
            int add = n - g_gomaxprocs;
            for (int i = 0; i < add && g_extra_workers_n < 256; i++)
                GMB_THREAD_CREATE(&g_extra_workers[g_extra_workers_n++], gmb_worker, NULL);
        }
        g_gomaxprocs = n;
    }
    GMB_MUTEX_UNLOCK(&g_gmp_lock);
    return prev;
}

void gmb_gpm_start(void (*main_entry)(void*)) {
    g_gpm_active = 1;
#if !defined(_WIN32) && !defined(_WIN64)
    gmb_stack_install_handler();
#endif
    GMB_MUTEX_INIT(&g_sched.qlock);
    GMB_COND_INIT(&g_sched.qcond);
    GMB_MUTEX_INIT(&g_sched.all_lock);
    GMB_MUTEX_INIT(&g_gmp_lock);
    g_sched.qcap = 64; g_sched.queue = (gmb_task_t**)calloc(64, sizeof(gmb_task_t*));
    g_sched.all_cap = 64; g_sched.all = (gmb_task_t**)calloc(64, sizeof(gmb_task_t*));
#if !defined(_WIN32) && !defined(_WIN64)
    /* Coarse-park SQL sempre attivo (parità con IL/PCode/VM-C: nessun flag, nessuna asimmetria). */
    if (gmb_sql_iface.set_park_hook) gmb_sql_iface.set_park_hook(c99_sql_park);
#endif
    gmb_task_spawn(main_entry, NULL);
    const char* env = getenv("GMBASIC_GOMAXPROCS");
    int nw = env ? atoi(env) : 0;
    if (nw <= 0) nw = gmb_ncpu();
    if (nw <= 0) nw = 4;
    if (nw > 8) nw = 8;
    g_gomaxprocs = nw;
    gmb_thread_t workers[8]; gmb_thread_t watcher;
    for (int i = 0; i < nw; i++) GMB_THREAD_CREATE(&workers[i], gmb_worker, NULL);
    GMB_THREAD_CREATE(&watcher, gmb_sleep_watcher, NULL);
    /* TRAY + present grafico: il main thread pompa coda tray e finestra SDL
     * (pattern SDL: UI sul main thread). La VM gira sui worker GPM. */
    { extern void gmb_tui_present_gfx(void);
      extern int gmb_gfx_screen_on(void);   /* usata anche fuori da GMB_GFX_SDL_PRESENT (riga debug) */
#ifdef GMB_GFX_SDL_PRESENT
      extern void gmb_gfx_present_start(void); extern void gmb_gfx_present_stop(void);
      /* NON aprire la finestra SDL se lo screen grafico non è attivo (niente SCREEN):
       * altrimenti ogni app (es. tray-only) mostra una finestra vuota all'avvio.
       * Il present_start avviene lazy via gmb_tui_present_gfx quando SCREEN è chiamato. */
      if (gmb_gfx_screen_on()) gmb_gfx_present_start();
#endif
      { extern int gmb_gfx_present_on_main_thread(void); static int _li=0; int _main = gmb_gfx_present_on_main_thread();
        while (gmb_c99_sched_has_active()) {
          if(_li<5 && getenv("GMB_GFX_DBG")){ _li++; fprintf(stderr,"[loop] it=%d main=%d active=%d on=%d\n", _li, _main, gmb_c99_sched_has_active(), gmb_gfx_screen_on()); }
          GMB_UI_TRAY_POLL(); gmb_tui_present_gfx(); gmb_iconsole_present_if_active(); gmb_sleep_ms(1); } }
#ifdef GMB_GFX_SDL_PRESENT
      gmb_gfx_present_stop();
#endif
    }
    GMB_MUTEX_LOCK(&g_sched.qlock); g_sched.shutdown = 1;
    GMB_COND_BROADCAST(&g_sched.qcond); GMB_MUTEX_UNLOCK(&g_sched.qlock);
    for (int i = 0; i < nw; i++) GMB_THREAD_JOIN(workers[i]);
    GMB_THREAD_JOIN(watcher);
#if !defined(_WIN32) && !defined(_WIN64)
    for (int i = 0; i < g_sched.all_count; i++) {
        if (g_sched.all[i]->stack_map) munmap(g_sched.all[i]->stack_map, g_sched.all[i]->stack_map_len);
        free(g_sched.all[i]);
    }
#endif
}

struct gmb_chan {
    gmb_val_t* buf; int count, cap, head, tail;   /* cap 0 = unbuffered (Go make(chan T)) */
    struct gmb_waiter* sw; int sw_cap, sw_head, sw_tail, sw_count;
    struct gmb_waiter* rw; int rw_cap, rw_head, rw_tail, rw_count;
    int closed;   /* CHANNEL CLOSE (parità Go close(ch)) */
    gmb_lock_t lock;
};
gmb_chan_t* gmb_chan_new(int cap) {
    gmb_chan_t* c = (gmb_chan_t*)calloc(1, sizeof(gmb_chan_t));
    /* Parità Go: capacity 0 = UNBUFFERED vero (rendezvous diretto sender→receiver,
       nessuno slot in transito). Il vecchio hack cap-1 lasciava al sender 1 valore
       di anticipo → deriva di fase fra le parti e deadlock multi-worker
       (bench_concurrency: tutti i worker parcheggiati in SEND col Main in SEND). */
    c->cap = cap > 0 ? cap : 0;
    c->buf = (gmb_val_t*)calloc((size_t)(c->cap > 0 ? c->cap : 1), sizeof(gmb_val_t));
    return c;
}
/* GO SELECT: stato condiviso fra le registrazioni (beacon) di UN park. settled=1
   appena un canale claima il wake (CAS) o il task riparte dopo il park: le entry
   rimaste sugli altri canali diventano stale e vengono scartate lazy al pop
   (parità Go: i sudog perdenti sono rimossi; qui rimozione differita). refcnt =
   registrazioni vive: ogni pop fa deref, l'ultimo libera. I beacon NON portano
   dati (has_val=0, out==NULL): il trasferimento passa SEMPRE da un waiter reale
   (sender parcheggiato col valore / receiver coi puntatori out-ok) e avviene nella
   ri-valutazione del select svegliato — mai handoff esterno verso un beacon. */
typedef struct gmb_waiter { gmb_task_t* t; gmb_val_t val; int has_val; gmb_val_t* out; int* ok; gmb_sel_state_t* sel; } gmb_waiter_t;
void gmb_sel_deref(gmb_sel_state_t* s) { if (s && __sync_sub_and_fetch(&s->refcnt, 1) == 0) free(s); }
static int gmb_sel_claim(gmb_sel_state_t* s, int wi) { if (!s) return 1;
    if (!__sync_bool_compare_and_swap(&s->settled, 0, 1)) return 0;
    s->winner = wi; s->winner_ok = 1; return 1; }   /* wi=-1: wake con ri-valutazione */
static void gmb_wpush(gmb_waiter_t** a, int* cap, int* head, int* tail, int* count, gmb_task_t* t, gmb_val_t v, int has_val, gmb_val_t* out, int* ok, gmb_sel_state_t* sel);
static gmb_task_t* gmb_wpop(gmb_waiter_t** a, int* cap, int* head, int* tail, int* count, gmb_waiter_t* out_e);
static __attribute__((unused)) void gmb_chan_sel_wake(gmb_chan_t* c, gmb_task_t** woken) {   /* sotto c->lock */
    gmb_waiter_t** qa; int *qcap, *qhead, *qtail, *qcount;
    if (c->rw_count > 0) { qa = &c->rw; qcap = &c->rw_cap; qhead = &c->rw_head; qtail = &c->rw_tail; qcount = &c->rw_count; }
    else { qa = &c->sw; qcap = &c->sw_cap; qhead = &c->sw_head; qtail = &c->sw_tail; qcount = &c->sw_count; }
    while (*qcount > 0) {
        gmb_waiter_t* h = &(*qa)[*qhead];
        if (!h->sel) return;   /* testa reale: nessun beacon da svegliare */
        gmb_waiter_t e; gmb_wpop(qa, qcap, qhead, qtail, qcount, &e);
        if (gmb_sel_claim(e.sel, -1)) { *woken = e.t; gmb_sel_deref(e.sel); return; }
        gmb_sel_deref(e.sel);   /* stale: scarta e continua */
    }
}
static void gmb_wpush(gmb_waiter_t** a, int* cap, int* head, int* tail, int* count, gmb_task_t* t, gmb_val_t v, int has_val, gmb_val_t* out, int* ok, gmb_sel_state_t* sel) {
    if (*count >= *cap) {
        int nc = *cap ? *cap * 2 : 8;
        gmb_waiter_t* na = (gmb_waiter_t*)malloc(nc * sizeof(gmb_waiter_t));
        for (int i = 0; i < *count; i++) na[i] = (*a)[(*head + i) % *cap];
        free(*a); *a = na; *cap = nc; *head = 0; *tail = *count;
    }
    gmb_waiter_t* e = &(*a)[*tail]; e->t = t; e->val = v; e->has_val = has_val; e->out = out; e->ok = ok; e->sel = sel;
    *tail = (*tail + 1) % *cap; (*count)++;
}
static gmb_task_t* gmb_wpop(gmb_waiter_t** a, int* cap, int* head, int* tail, int* count, gmb_waiter_t* out_e) {
    if (*count == 0) return NULL;
    gmb_waiter_t* e = &(*a)[*head]; *head = (*head + 1) % *cap; (*count)--;
    if (out_e) *out_e = *e;
    return e->t;
}
static void gmb_cpush(gmb_task_t*** a, int* cap, int* head, int* tail, int* count, gmb_task_t* t) {
    if (*count >= *cap) {
        int nc = *cap ? *cap * 2 : 8;
        gmb_task_t** na = (gmb_task_t**)malloc(nc * sizeof(gmb_task_t*));
        int nn = 0; for (int i = 0; i < *count; i++) na[nn++] = (*a)[(*head + i) % *cap];
        free(*a); *a = na; *cap = nc; *head = 0; *tail = nn;
    }
    (*a)[*tail] = t; *tail = (*tail + 1) % *cap; (*count)++;
}
static gmb_task_t* gmb_cpop(gmb_task_t*** a, int* cap, int* head, int* tail, int* count) {
    if (*count == 0) return NULL;
    gmb_task_t* t = (*a)[*head]; *head = (*head + 1) % *cap; (*count)--; return t;
}
int gmb_chan_send_or_park(gmb_chan_t* c, gmb_val_t v, gmb_task_t** woken) { gmb_task_t* g_current = gmb_cur();
    *woken = NULL;
    gmb_lock(&c->lock);
    if (c->closed) { gmb_unlock(&c->lock); return -1; }   /* Go: send su chiuso = panic */
    while (c->rw_count > 0) {
        gmb_waiter_t* h = &c->rw[c->rw_head];
        if (h->sel) {   /* beacon GO SELECT: dati visibili PRIMA del wake, poi ri-valuta */
            gmb_waiter_t e; gmb_wpop(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, &e);
            int win = gmb_sel_claim(e.sel, -1); gmb_sel_deref(e.sel);
            if (!win) continue;   /* stale: prossimo waiter */
            if (c->cap > 0 && c->count < c->cap) {
                c->buf[c->tail] = v; c->tail = (c->tail + 1) % c->cap; c->count++;
                *woken = e.t; gmb_unlock(&c->lock); return 1;
            }
            g_current->state = GMB_TS_BLOCKED; g_current->sleep_until_ms = 0;
            gmb_wpush(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, g_current, v, 1, NULL, NULL, NULL);
            *woken = e.t; gmb_unlock(&c->lock); return 0;   /* valore in transito su sw */
        }
        if (h->out) {   /* receiver reale: handoff diretto (Go sendq→recvq) */
            gmb_waiter_t e; gmb_wpop(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, &e);
            *e.out = v; if (e.ok) *e.ok = 1; e.t->xfer_done = 1; *woken = e.t;
            gmb_unlock(&c->lock);
            return 1;
        }
        gmb_wpop(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, NULL);   /* anomalo: scarta */
    }
    if (c->cap > 0 && c->count < c->cap) {
        c->buf[c->tail] = v; c->tail = (c->tail + 1) % c->cap; c->count++;
        gmb_unlock(&c->lock);
        return 1;
    }
    g_current->state = GMB_TS_BLOCKED; g_current->sleep_until_ms = 0;
    gmb_wpush(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, g_current, v, 1, NULL, NULL, NULL);
    gmb_unlock(&c->lock);
    return 0;
}
void gmb_chan_send(gmb_chan_t* c, gmb_val_t v) { gmb_task_t* g_self = gmb_cur();
    if (!c) { for (;;) { g_self->state = GMB_TS_BLOCKED; gmb_block(); } }   /* nil: blocco perenne (Go) */
    for (;;) {
        gmb_task_t* w = NULL;
        int r = gmb_chan_send_or_park(c, v, &w);
        if (r < 0) { fprintf(stderr, "[GMBASIC] SEND su canale chiuso (parita Go: panic send on closed channel)\n"); _Exit(2); }
        if (r) { if (w) gmb_sched_enqueue(w); return; }
        if (w) gmb_sched_enqueue(w);   /* park post-claim beacon: la controparte va svegliata */
        gmb_block();
        if (g_self->xfer_done) { g_self->xfer_done = 0; return; }   /* handoff: valore già preso */
    }
}
/* recv con flag ok (parità Go v, ok := <-ch): ritorna 1 se conclusa (controllare
   *ok_out: 0 = canale chiuso e svuotato, valore zero), 0 se parcheggiato. */
int gmb_chan_recv_or_park(gmb_chan_t* c, gmb_val_t* out, int* ok_out, gmb_task_t** woken) { gmb_task_t* g_current = gmb_cur();
    *woken = NULL; *ok_out = 1;
    gmb_lock(&c->lock);
    if (c->count > 0) {
        *out = c->buf[c->head]; c->head = (c->head + 1) % c->cap; c->count--;
        while (c->sw_count > 0) {   /* Go: refill dal sender in testa (FIFO) */
            gmb_waiter_t* h = &c->sw[c->sw_head];
            if (h->has_val) {   /* sender reale: entra nel buffer, handoff completato */
                gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
                c->buf[c->tail] = e.val; c->tail = (c->tail + 1) % c->cap; c->count++; e.t->xfer_done = 1; *woken = e.t;
                break;
            }
            if (h->sel) {   /* beacon: slot libero → wake (trysend nel buffer), o scarta se stale */
                gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
                int win = gmb_sel_claim(e.sel, -1); gmb_sel_deref(e.sel);
                if (win) { *woken = e.t; break; }
                continue;
            }
            gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, NULL);
        }
        gmb_unlock(&c->lock);
        return 1;
    }
    if (c->closed) {   /* chiuso e svuotato: zero + ok=0, mai blocco (Go) */
        memset(out, 0, sizeof(*out)); *ok_out = 0;
        gmb_unlock(&c->lock);
        return 1;
    }
    while (c->sw_count > 0) {
        gmb_waiter_t* h = &c->sw[c->sw_head];
        if (h->has_val) {   /* sender reale: presa diretta del valore in transito */
            gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
            *out = e.val; e.t->xfer_done = 1; *woken = e.t;
            gmb_unlock(&c->lock);
            return 1;
        }
        if (h->sel) {   /* beacon: il receiver parcheggia PRIMA (out reali visibili),
                          poi claim+wake → la ri-valutazione fa trysend sull'handoff. */
            gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
            int win = gmb_sel_claim(e.sel, -1); gmb_sel_deref(e.sel);
            if (!win) continue;   /* stale */
            g_current->state = GMB_TS_BLOCKED; g_current->sleep_until_ms = 0;
            gmb_val_t __zv; memset(&__zv, 0, sizeof(__zv));
            gmb_wpush(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, g_current, __zv, 0, out, ok_out, NULL);
            *woken = e.t; gmb_unlock(&c->lock); return 0;
        }
        gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, NULL);
    }
    g_current->state = GMB_TS_BLOCKED; g_current->sleep_until_ms = 0;
    gmb_val_t __zv; memset(&__zv, 0, sizeof(__zv));
    gmb_wpush(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, g_current, __zv, 0, out, ok_out, NULL);
    gmb_unlock(&c->lock);
    return 0;
}
gmb_val_t gmb_chan_recv(gmb_chan_t* c) {
    gmb_val_t tmp; int ok = 1; gmb_task_t* g = gmb_cur();
    if (!c) { for (;;) { g->state = GMB_TS_BLOCKED; gmb_block(); } }   /* nil */
    for (;;) {
        gmb_task_t* w = NULL;
        if (gmb_chan_recv_or_park(c, &tmp, &ok, &w)) { if (w) gmb_sched_enqueue(w); return tmp; }
        if (w) gmb_sched_enqueue(w);   /* park post-claim beacon: la controparte va svegliata */
        gmb_block();
        if (g->xfer_done) { g->xfer_done = 0; return tmp; }   /* handoff: valore già in tmp */
    }
}
void gmb_chan_recv_ok(gmb_chan_t* c, gmb_val_t* out, int* ok_out) {
    gmb_task_t* g = gmb_cur();
    if (!c) { for (;;) { g->state = GMB_TS_BLOCKED; gmb_block(); } }   /* nil */
    for (;;) {
        gmb_task_t* w = NULL;
        if (gmb_chan_recv_or_park(c, out, ok_out, &w)) { if (w) gmb_sched_enqueue(w); return; }
        if (w) gmb_sched_enqueue(w);   /* park post-claim beacon: la controparte va svegliata */
        gmb_block();
        if (g->xfer_done) { g->xfer_done = 0; return; }   /* handoff: out/ok già scritti */
    }
}
/* tryrecv NON bloccante (per GO SELECT): 1 = pronto (ok_out dice chiuso/aperto), 0 = vuoto. */
int gmb_chan_tryrecv(gmb_chan_t* c, gmb_val_t* out, int* ok_out) {
    if (!c) return 0;   /* nil: mai pronto (Go) */
    gmb_task_t* w = NULL; int ready = 0;
    *ok_out = 1;
    gmb_lock(&c->lock);
    if (c->count > 0) {
        *out = c->buf[c->head]; c->head = (c->head + 1) % c->cap; c->count--; ready = 1;
        while (c->sw_count > 0) {
            gmb_waiter_t* h = &c->sw[c->sw_head];
            if (h->has_val) {
                gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
                c->buf[c->tail] = e.val; c->tail = (c->tail + 1) % c->cap; c->count++; e.t->xfer_done = 1; w = e.t;
                break;
            }
            if (h->sel) {
                gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
                int win = gmb_sel_claim(e.sel, -1); gmb_sel_deref(e.sel);
                if (win) { w = e.t; break; }
                continue;
            }
            gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, NULL);
        }
    } else if (c->closed) { memset(out, 0, sizeof(*out)); *ok_out = 0; ready = 1; }
    else while (c->sw_count > 0) {
        gmb_waiter_t* h = &c->sw[c->sw_head];
        if (h->has_val) {
            gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
            *out = e.val; e.t->xfer_done = 1; w = e.t; ready = 1;
            break;
        }
        if (h->sel) break;   /* beacon: non pronto in try (il sender deve parcheggiare) */
        gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, NULL);
    }
    gmb_unlock(&c->lock);
    if (ready && w) gmb_sched_enqueue(w);
    return ready;
}
int gmb_chan_trysend(gmb_chan_t* c, gmb_val_t v) {
    if (!c) return 0;   /* nil: mai pronto (Go) */
    gmb_task_t* w = NULL; int ok = 0;
    gmb_lock(&c->lock);
    if (!c->closed) {
        while (c->rw_count > 0) {
            gmb_waiter_t* h = &c->rw[c->rw_head];
            if (h->sel) {   /* beacon select-receiver: senza parcheggio del sender non c'è
                              consegna non bloccante → case NON pronto (niente promesse). */
                break;
            }
            if (h->out) {
                gmb_waiter_t e; gmb_wpop(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, &e);
                *e.out = v; if (e.ok) *e.ok = 1; e.t->xfer_done = 1; w = e.t; ok = 1;
                break;
            }
            gmb_wpop(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, NULL);
        }
        if (!ok && c->cap > 0 && c->count < c->cap) {
            c->buf[c->tail] = v; c->tail = (c->tail + 1) % c->cap; c->count++; ok = 1;
        }
    }
    gmb_unlock(&c->lock);
    if (ok && w) gmb_sched_enqueue(w);
    return ok;
}
/* CHANNEL CLOSE (parità Go close(ch)): idempotente sul doppio close (il VM/C lo
   segnala come errore; qui tollerato). Sveglia TUTTI i waiter: i receiver vedranno
   zero+ok=0, i sender parcheggiati al retry trovano closed → errore send-su-chiuso. */
void gmb_chan_close(gmb_chan_t* c) {
    if (!c) return;
    gmb_task_t* wake = NULL;
    gmb_lock(&c->lock);
    if (c->closed) { gmb_unlock(&c->lock); return; }
    c->closed = 1;
    gmb_waiter_t e;
    while (gmb_wpop(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, &e) != NULL) { if (e.sel) gmb_sel_deref(e.sel); e.t->next = wake; wake = e.t; }
    while (gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e) != NULL) { if (e.sel) gmb_sel_deref(e.sel); e.t->next = wake; wake = e.t; }
    gmb_unlock(&c->lock);
    for (; wake; ) { gmb_task_t* nx = wake->next; wake->next = NULL; gmb_sched_enqueue(wake); wake = nx; }
}

struct gmb_wg { int count; gmb_task_t* waiters; gmb_lock_t lock; };
gmb_wg_t* gmb_wg_new(void) { return (gmb_wg_t*)calloc(1, sizeof(gmb_wg_t)); }
void gmb_c99_wg_add(gmb_wg_t* w, int n) { gmb_lock(&w->lock); w->count += n; gmb_unlock(&w->lock); }
void gmb_c99_wg_done(gmb_wg_t* w) {
    gmb_task_t* wakers = NULL;
    gmb_lock(&w->lock);
    w->count--;
    if (w->count <= 0) { wakers = w->waiters; w->waiters = NULL; }
    gmb_unlock(&w->lock);
    for (; wakers; ) { gmb_task_t* nx = wakers->next; gmb_sched_enqueue(wakers); wakers = nx; }
}
void gmb_wg_wait(gmb_wg_t* w) { gmb_task_t* g_current = gmb_cur();
    for (;;) {
        gmb_lock(&w->lock);
        if (w->count <= 0) { gmb_unlock(&w->lock); return; }
        g_current->state = GMB_TS_BLOCKED; g_current->sleep_until_ms = 0;
        g_current->wg = w; g_current->next = w->waiters; w->waiters = g_current;
        gmb_unlock(&w->lock);
        gmb_block();
    }
}

/* WAIT nuda (bug 2026-10-01): gruppo implicito dei figli lanciati con GO
 * SUB/FUNCTION senza IN. Senza figli mai lanciati il gruppo non esiste e la
 * wait è no-op (come prima). I figli con IN restano del gruppo esplicito. */
gmb_wg_t* gmb_implicit_wg_get(void) {
    gmb_task_t* g_current = gmb_cur();
    if (!g_current) return NULL;
    if (!g_current->implicit_wg) g_current->implicit_wg = gmb_wg_new();
    return g_current->implicit_wg;
}
void gmb_implicit_wait(void) {
    gmb_task_t* g_current = gmb_cur();
    if (g_current && g_current->implicit_wg) gmb_wg_wait(g_current->implicit_wg);
}

/* CONTEXT (parità Go context.Context): albero di contesti cancellabili; DONE è un
   canale (cap 0) chiuso alla cancellazione, propagata ai figli ricorsivamente. */
struct gmb_gcontext { gmb_lock_t lock; int cancelled; gmb_chan_t* done;
    struct gmb_gcontext** kids; int nkids, capkids; };
gmb_gcontext_t* gmb_ctx_create(gmb_gcontext_t* parent) {
    gmb_gcontext_t* c = (gmb_gcontext_t*)calloc(1, sizeof(gmb_gcontext_t));
    c->done = gmb_chan_new(0);
    if (parent) {
        gmb_lock(&parent->lock);
        if (parent->cancelled) { c->cancelled = 1; gmb_unlock(&parent->lock); gmb_chan_close(c->done); }
        else {
            if (parent->nkids == parent->capkids) {
                parent->capkids = parent->capkids ? parent->capkids * 2 : 4;
                parent->kids = (gmb_gcontext_t**)realloc(parent->kids, parent->capkids * sizeof(gmb_gcontext_t*));
            }
            parent->kids[parent->nkids++] = c;
            gmb_unlock(&parent->lock);
        }
    }
    return c;
}
gmb_gcontext_t* gmb_ctx_background(void) { return gmb_ctx_create(NULL); }
gmb_gcontext_t* gmb_ctx_withcancel(gmb_gcontext_t* p) { return gmb_ctx_create(p); }
void gmb_ctx_cancel(gmb_gcontext_t* c) {
    if (!c) return;
    gmb_lock(&c->lock);
    if (c->cancelled) { gmb_unlock(&c->lock); return; }
    c->cancelled = 1;
    int n = c->nkids; gmb_gcontext_t** kids = c->kids;
    gmb_unlock(&c->lock);
    gmb_chan_close(c->done);
    for (int i = 0; i < n; i++) gmb_ctx_cancel(kids[i]);
}
gmb_chan_t* gmb_ctx_done(gmb_gcontext_t* c) { return c ? c->done : NULL; }
int gmb_ctx_cancelled(gmb_gcontext_t* c) { if (!c) return 0; gmb_lock(&c->lock); int v = c->cancelled; gmb_unlock(&c->lock); return v; }

/* --- MUTEX (parità Go sync.Mutex / PCodeMutex): lock cooperativo sul task (mai sul
   thread OS). UNLOCK con HANDOFF: il waiter svegliato riceve direttamente il lock
   (held resta 1) — niente starvation da inserimento tra unlock e re-lock. */
struct gmb_gmutex { gmb_lock_t lock; int held; gmb_task_t** waiters; int wcap; int whead; int wtail; int wcount; };
gmb_gmutex_t* gmb_gmutex_new(void) { return (gmb_gmutex_t*)calloc(1, sizeof(gmb_gmutex_t)); }
void gmb_gmutex_lock(gmb_gmutex_t* m) { gmb_task_t* g = gmb_cur();
    for (;;) {
        if (g->sync_granted) { g->sync_granted = 0; return; }  /* handoff da unlock */
        gmb_lock(&m->lock);
        if (!m->held) { m->held = 1; gmb_unlock(&m->lock); return; }
        g->state = GMB_TS_BLOCKED; g->sleep_until_ms = 0;
        gmb_cpush(&m->waiters, &m->wcap, &m->whead, &m->wtail, &m->wcount, g);
        gmb_unlock(&m->lock);
        gmb_block();
    }
}
void gmb_gmutex_unlock(gmb_gmutex_t* m) {
    gmb_lock(&m->lock);
    gmb_task_t* w = gmb_cpop(&m->waiters, &m->wcap, &m->whead, &m->wtail, &m->wcount);
    if (!w) m->held = 0;   /* nessun waiter: rilascia (handoff altrimenti) */
    gmb_unlock(&m->lock);
    if (w) { w->next = NULL; w->sync_granted = 1; gmb_sched_enqueue(w); }
}

/* --- RWMUTEX (parità Go sync.RWMutex / PCodeRwMutex): lettori condivisi + scrittore
   esclusivo, precedenza agli scrittori in attesa (anti-starvation, come Go). */
struct gmb_grwmutex { gmb_lock_t lock; int readers; int writer;
    gmb_task_t** rwait; int rwc; int rwh; int rwt; int rn;
    gmb_task_t** wwait; int wwc; int wwh; int wwt; int wn; };
gmb_grwmutex_t* gmb_grwmutex_new(void) { return (gmb_grwmutex_t*)calloc(1, sizeof(gmb_grwmutex_t)); }
void gmb_grwmutex_rlock(gmb_grwmutex_t* m) { gmb_task_t* g = gmb_cur();
    for (;;) {
        if (g->sync_granted) { g->sync_granted = 0; return; }  /* già contato dall'unlock */
        gmb_lock(&m->lock);
        if (!m->writer && m->wn == 0) { m->readers++; gmb_unlock(&m->lock); return; }
        g->state = GMB_TS_BLOCKED; g->sleep_until_ms = 0;
        gmb_cpush(&m->rwait, &m->rwc, &m->rwh, &m->rwt, &m->rn, g);
        gmb_unlock(&m->lock);
        gmb_block();
    }
}
void gmb_grwmutex_runlock(gmb_grwmutex_t* m) {
    gmb_task_t* handoff = NULL;
    gmb_lock(&m->lock);
    m->readers--;
    if (m->readers == 0 && m->wn > 0) {  /* handoff a UN scrittore */
        m->writer = 1;
        handoff = gmb_cpop(&m->wwait, &m->wwc, &m->wwh, &m->wwt, &m->wn);
    }
    gmb_unlock(&m->lock);
    if (handoff) { handoff->next = NULL; handoff->sync_granted = 1; gmb_sched_enqueue(handoff); }
}
void gmb_grwmutex_lock(gmb_grwmutex_t* m) { gmb_task_t* g = gmb_cur();
    for (;;) {
        if (g->sync_granted) { g->sync_granted = 0; return; }  /* handoff da unlock/runlock */
        gmb_lock(&m->lock);
        if (!m->writer && m->readers == 0) { m->writer = 1; gmb_unlock(&m->lock); return; }
        g->state = GMB_TS_BLOCKED; g->sleep_until_ms = 0;
        gmb_cpush(&m->wwait, &m->wwc, &m->wwh, &m->wwt, &m->wn, g);
        gmb_unlock(&m->lock);
        gmb_block();
    }
}
void gmb_grwmutex_unlock(gmb_grwmutex_t* m) {
    gmb_task_t* wake = NULL;
    gmb_lock(&m->lock);
    m->writer = 0;
    if (m->wn > 0) {  /* handoff a UN scrittore in attesa */
        m->writer = 1;
        wake = gmb_cpop(&m->wwait, &m->wwc, &m->wwh, &m->wwt, &m->wn);
        if (wake) wake->sync_granted = 1;
    } else {          /* tutti i lettori in attesa procedono insieme */
        gmb_task_t* w; gmb_task_t* head = NULL;
        while ((w = gmb_cpop(&m->rwait, &m->rwc, &m->rwh, &m->rwt, &m->rn)) != NULL) {
            m->readers++; w->sync_granted = 1; w->next = head; head = w;
        }
        wake = head;
    }
    gmb_unlock(&m->lock);
    for (; wake; ) { gmb_task_t* nx = wake->next; wake->next = NULL; gmb_sched_enqueue(wake); wake = nx; }
}

/* --- ONCE (parità Go sync.Once): il primo chiamante esegue fn, gli altri parcheggiano
   e proseguono al completamento senza ri-eseguire (Do once). */
struct gmb_gonce { gmb_lock_t lock; int state; gmb_task_t* waiters; };  /* state: 0=todo 1=running 2=done */
gmb_gonce_t* gmb_gonce_new(void) { return (gmb_gonce_t*)calloc(1, sizeof(gmb_gonce_t)); }
void gmb_gonce_run(gmb_gonce_t* o, void (*fn)(void)) { gmb_task_t* g = gmb_cur();
    gmb_lock(&o->lock);
    if (o->state == 2) { gmb_unlock(&o->lock); return; }
    if (o->state == 1) {
        for (;;) {
            g->state = GMB_TS_BLOCKED; g->sleep_until_ms = 0;
            g->next = o->waiters; o->waiters = g;
            gmb_unlock(&o->lock);
            gmb_block();
            gmb_lock(&o->lock);
            if (o->state == 2) { gmb_unlock(&o->lock); return; }
        }
    }
    o->state = 1;
    gmb_unlock(&o->lock);
    fn();
    gmb_task_t* w;
    gmb_lock(&o->lock);
    o->state = 2; w = o->waiters; o->waiters = NULL;
    gmb_unlock(&o->lock);
    for (; w; ) { gmb_task_t* nx = w->next; w->next = NULL; gmb_sched_enqueue(w); w = nx; }
}

/* --- TRYLOCK (parità Go 1.18 mu.TryLock): acquisizione NON bloccante. --- */
int gmb_gmutex_trylock(gmb_gmutex_t* m) {
    gmb_lock(&m->lock);
    if (!m->held) { m->held = 1; gmb_unlock(&m->lock); return 1; }
    gmb_unlock(&m->lock); return 0;
}
int gmb_grwmutex_trylock(gmb_grwmutex_t* m) {  /* scrittore: solo se nessuno attivo/attivo in attesa */
    gmb_lock(&m->lock);
    if (!m->writer && m->readers == 0 && m->wn == 0 && m->rn == 0) { m->writer = 1; gmb_unlock(&m->lock); return 1; }
    gmb_unlock(&m->lock); return 0;
}

/* --- ATOMIC (parità Go sync/atomic: AddInt64/Load/Store/CAS/Swap su box LONG) --- */
struct gmb_atomic { long long v; gmb_lock_t lock; };
gmb_atomic_t* gmb_atomic_new(void) { return (gmb_atomic_t*)calloc(1, sizeof(gmb_atomic_t)); }
 long long gmb_atomic_inc(gmb_atomic_t* a, long long d) {
     gmb_lock(&a->lock); long long old = a->v; a->v += d; gmb_unlock(&a->lock); return old + d;
 }
long long gmb_atomic_fetch(gmb_atomic_t* a) {
    gmb_lock(&a->lock); long long v = a->v; gmb_unlock(&a->lock); return v;
}
void gmb_c99_atomic_store(gmb_atomic_t* a, long long v) {
    gmb_lock(&a->lock); a->v = v; gmb_unlock(&a->lock);
}
int gmb_c99_atomic_cas(gmb_atomic_t* a, long long oldv, long long newv) {
    gmb_lock(&a->lock); int ok = (a->v == oldv); if (ok) a->v = newv; gmb_unlock(&a->lock); return ok;
}
long long gmb_c99_atomic_swap(gmb_atomic_t* a, long long newv) {
    gmb_lock(&a->lock); long long old = a->v; a->v = newv; gmb_unlock(&a->lock); return old;
}

/* --- POOL (parità Go sync.Pool: Get/Put; GET a vuoto → valore zero) --- */
struct gmb_pool { gmb_val_t* items; int cap; int count; gmb_lock_t lock; };
gmb_pool_t* gmb_pool_new(void) { return (gmb_pool_t*)calloc(1, sizeof(gmb_pool_t)); }
void gmb_c99_pool_put(gmb_pool_t* p, gmb_val_t v) {
    gmb_lock(&p->lock);
    if (p->count == p->cap) { p->cap = p->cap ? p->cap * 2 : 8; p->items = (gmb_val_t*)realloc(p->items, p->cap * sizeof(gmb_val_t)); }
    p->items[p->count++] = v;
    gmb_unlock(&p->lock);
}
gmb_val_t gmb_c99_pool_get(gmb_pool_t* p) {
    gmb_lock(&p->lock);
    gmb_val_t v = p->count > 0 ? p->items[--p->count] : gmb_val_nil();
    gmb_unlock(&p->lock);
    return v;
}

/* --- SYNCMAP (parità Go sync.Map su dict + lock: Store/Load/Delete/LoadOrStore) --- */
struct gmb_syncmap { gmb_dict_t dict; gmb_lock_t lock; };
gmb_syncmap_t* gmb_syncmap_new(void) {
    gmb_syncmap_t* m = (gmb_syncmap_t*)calloc(1, sizeof(gmb_syncmap_t)); m->dict = gmb_dict_new(); return m;
}
void gmb_syncmap_store(gmb_syncmap_t* m, const char* k, gmb_val_t v) {
    gmb_lock(&m->lock); gmb_dict_set(&m->dict, k, v); gmb_unlock(&m->lock);
}
gmb_val_t gmb_syncmap_load(gmb_syncmap_t* m, const char* k) {
    gmb_lock(&m->lock); gmb_val_t v = gmb_dict_get(m->dict, k); gmb_unlock(&m->lock); return v;
}
gmb_val_t gmb_syncmap_delete(gmb_syncmap_t* m, const char* k) {
    gmb_lock(&m->lock);
    gmb_val_t v = gmb_dict_get(m->dict, k);
    gmb_dict_remove(&m->dict, k);
    gmb_unlock(&m->lock);
    return v;
}
gmb_val_t gmb_syncmap_load_or_store(gmb_syncmap_t* m, const char* k, gmb_val_t v) {
    gmb_lock(&m->lock);
    gmb_val_t cur = gmb_dict_get(m->dict, k);
    if (cur.kind == GMB_TYPE_NIL) { gmb_dict_set(&m->dict, k, v); cur = v; }
    gmb_unlock(&m->lock);
    return cur;
}
/* val → stringa con NIL → "" (parità IlRuntime.CollectionToString: null → "");
   usato da POOL.GET vuoto e SYNCMAP.LOAD/DELETE/LOADORSTORE su chiave assente.
   Ritorna gmb_string_t: IsStringExpression riconosce questi builtin. */
gmb_string_t gmb_val_str_or_empty(gmb_val_t v) {
    if (v.kind == GMB_TYPE_NIL) return gmb_string_alloc("");
    return gmb_val_to_string(v);
}

/* --- COND (parità Go sync.Cond): WAIT registra il waiter PRIMA di rilasciare il
   mutex (nessun wake perso: SIGNAL poppa sotto lo stesso lock), poi parcheggia;
   al wake ri-acquisisce il mutex (MUTEX o RWMUTEX, kind 0/1) e torna al chiamante. */
struct gmb_gcond { gmb_lock_t lock; gmb_task_t** waiters; int ccap, chead, ctail, ccount; };
gmb_gcond_t* gmb_gcond_new(void) { return (gmb_gcond_t*)calloc(1, sizeof(gmb_gcond_t)); }
void gmb_gcond_wait(gmb_gcond_t* c, void* mu, int kind) {
    gmb_task_t* g = gmb_cur();
    gmb_lock(&c->lock);
    g->state = GMB_TS_BLOCKED; g->sleep_until_ms = 0;
    gmb_cpush(&c->waiters, &c->ccap, &c->chead, &c->ctail, &c->ccount, g);
    gmb_unlock(&c->lock);
    if (kind == 1) gmb_grwmutex_unlock((gmb_grwmutex_t*)mu); else gmb_gmutex_unlock((gmb_gmutex_t*)mu);
    gmb_block();
    if (kind == 1) gmb_grwmutex_lock((gmb_grwmutex_t*)mu); else gmb_gmutex_lock((gmb_gmutex_t*)mu);
}
void gmb_gcond_signal(gmb_gcond_t* c) {
    gmb_lock(&c->lock);
    gmb_task_t* w = gmb_cpop(&c->waiters, &c->ccap, &c->chead, &c->ctail, &c->ccount);
    gmb_unlock(&c->lock);
    if (w) { w->next = NULL; gmb_sched_enqueue(w); }
}
void gmb_gcond_broadcast(gmb_gcond_t* c) {
    gmb_lock(&c->lock);
    gmb_task_t* wake = NULL; gmb_task_t* w;
    while ((w = gmb_cpop(&c->waiters, &c->ccap, &c->chead, &c->ctail, &c->ccount)) != NULL) { w->next = wake; wake = w; }
    gmb_unlock(&c->lock);
    for (; wake; ) { gmb_task_t* nx = wake->next; wake->next = NULL; gmb_sched_enqueue(wake); wake = nx; }
}

/* --- FUTURE (§11) 4 stati (parità PCodeVM/IL): pending=0, resolved=1, error=2,
   cancelled=3. Implicit await: leggere un future non risolto BLOCCA; un future in
   ERRORE rilancia l'errore nel contesto dell'awaiter (ON ERROR/TRY del chiamante). */
gmb_future_t* gmb_future_new(void) { return (gmb_future_t*)calloc(1, sizeof(gmb_future_t)); }
void gmb_c99_future_resolve(gmb_future_t* f, gmb_val_t v) {
    gmb_task_t* w = NULL;
    gmb_lock(&f->lock);
    if (f->state == 0) { f->state = 1; f->value = v; w = f->waiters; f->waiters = NULL; }
    gmb_unlock(&f->lock);
    for (; w; ) { gmb_task_t* nx = w->next; gmb_sched_enqueue(w); w = nx; }
}
void gmb_c99_future_fail(gmb_future_t* f, int err_num, const char* err_desc) {
    gmb_task_t* w = NULL;
    gmb_lock(&f->lock);
    if (f->state == 0) { f->state = 2; f->err_num = err_num;
        f->err_desc = err_desc && *err_desc ? strdup(err_desc) : strdup("");
        w = f->waiters; f->waiters = NULL; }
    gmb_unlock(&f->lock);
    for (; w; ) { gmb_task_t* nx = w->next; gmb_sched_enqueue(w); w = nx; }
}
void gmb_future_cancel(gmb_future_t* f) {
    gmb_task_t* w = NULL;
    gmb_lock(&f->lock);
    if (f->state == 0) { f->state = 3; w = f->waiters; f->waiters = NULL; }
    gmb_unlock(&f->lock);
    for (; w; ) { gmb_task_t* nx = w->next; gmb_sched_enqueue(w); w = nx; }
}
void gmb_future_await(gmb_future_t* f) {
    for (;;) {
        gmb_lock(&f->lock);
        if (f->state != 0) { gmb_unlock(&f->lock); return; }
        gmb_task_t* g = gmb_cur();
        g->state = GMB_TS_BLOCKED; g->sleep_until_ms = 0; g->next = f->waiters; f->waiters = g;
        gmb_unlock(&f->lock);
        gmb_block();
    }
}
/* Future in errore → rilancia nel contesto dell'awaiter (longjmp all'handler ON
   ERROR/TRY attivo nel task che attende, parità PCodeVM frame propagation). */
void gmb_future_check_error(gmb_future_t* f) {
    if (f->state == 2) gmb_raise_error(f->err_num, f->err_desc ? f->err_desc : "", 0);
}

/* --- GO EXIT (§11.4): termina il task immediatamente (State=FINISHED) --- */
void gmb_task_exit(void) {
    gmb_task_t* g = gmb_cur();
    gmb_lock(&g->lock); g->state = GMB_TS_FINISHED; gmb_unlock(&g->lock);
#if defined(_WIN32) || defined(_WIN64)
    SwitchToFiber(g_worker_fiber);
#else
    gmb_ctx_switch(&g->ctx, &g_worker_ctx);
#endif
}

/* GO EXIT: segna l'unwind e longjmp al FINALLY più interno; senza FINALLY attivi
   termina subito. Il flag resta nel task: ogni FINALLY, dopo il body, chiama
   gmb_unwind_continue → FINALLY esterno e così via fino all'uscita del task. */
void gmb_goexit(void) {
    gmb_task_t* g = gmb_cur();
    if (!g) exit(0);
    g->unwinding = 1;
    if (g->unwind_top) longjmp(g->unwind_top->buf, 1);
    gmb_task_exit();
}
void gmb_unwind_continue(void) {
    gmb_task_t* g = gmb_cur();
    if (g && g->unwind_top) { longjmp(g->unwind_top->buf, 1); return; }
    gmb_task_exit();
}

gmb_sel_state_t* gmb_select_park(gmb_chan_t** chans, int* dir, gmb_val_t* send_vals, int n, long long deadline_ms) {
    gmb_task_t* g_current = gmb_cur();
    /* Registra il task su TUTTI i canali non-nil come beacon legato a sel (stato
       condiviso, CAS sul settled): il primo canale che claima sveglia il task, le
       entry residue diventano stale e vengono scartate lazy al pop (loser cleanup).
       NON lockare g_sched.qlock: l'ordine canale→qlock (di send/recv→enqueue)
       invertito qui produrrebbe deadlock; l'enqueue avviene DOPO l'unlock.
       deadline_ms > 0 = CASE TIMEOUT: lo sleep watcher sveglia il task alla scadenza.
       FINAL POLL dopo la registrazione: chiude la finestra di race fra lo sweep
       dell'emettitore (tryrecv/trysend) e la registrazione — se nel mezzo una
       controparte ha parcheggiato col valore/con out, il transfer avviene qui
       sotto lock e il case è servito direttamente (sel->winner/winner_val). */
    gmb_sel_state_t* sel = (gmb_sel_state_t*)malloc(sizeof(gmb_sel_state_t));
    sel->settled = 0; sel->winner = -1; sel->winner_ok = 1; sel->refcnt = 0;
    memset(&sel->winner_val, 0, sizeof(sel->winner_val));
    int registered = 0;
    for (int i = 0; i < n; i++) {
        if (!chans[i]) continue;   /* canale nil: mai pronto, non registra (Go) */
        gmb_lock(&chans[i]->lock);
        g_current->state = GMB_TS_BLOCKED;
        gmb_val_t __zv; memset(&__zv, 0, sizeof(__zv));
        if (dir[i] == 1)
            gmb_wpush(&chans[i]->sw, &chans[i]->sw_cap, &chans[i]->sw_head, &chans[i]->sw_tail, &chans[i]->sw_count, g_current, __zv, 0, NULL, NULL, sel);
        else
            gmb_wpush(&chans[i]->rw, &chans[i]->rw_cap, &chans[i]->rw_head, &chans[i]->rw_tail, &chans[i]->rw_count, g_current, __zv, 0, NULL, NULL, sel);
        sel->refcnt++;   /* sotto chans[i]->lock: visibile ai pop nei lock successivi */
        gmb_unlock(&chans[i]->lock);
        registered++;
    }
    sel->refcnt++;   /* ref del chiamante: farà UNA gmb_sel_deref (dispatch o retry) */
    g_current->state = GMB_TS_BLOCKED;
    g_current->sleep_until_ms = 0;
    if (deadline_ms > 0) g_current->sleep_until_ms = deadline_ms;
    if (registered == 0 && deadline_ms <= 0)
        for (;;) gmb_block();   /* tutti nil, nessun timeout: blocco perenne (Go deadlock) */
    gmb_task_t* w = NULL;
    for (int i = 0; i < n; i++) {
        gmb_chan_t* c = chans[i];
        if (!c) continue;
        gmb_lock(&c->lock);
        int won = 0;
        if (dir[i] == 0) {   /* CASE RECEIVE */
            if (c->count > 0) {
                if (gmb_sel_claim(sel, i)) {
                    sel->winner_val = c->buf[c->head]; c->head = (c->head + 1) % c->cap; c->count--;
                    while (c->sw_count > 0) {   /* refill dal sender in testa (FIFO) */
                        gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
                        if (e.sel) { if (gmb_sel_claim(e.sel, -1)) w = e.t; gmb_sel_deref(e.sel); if (w) break; continue; }
                        if (e.has_val) { c->buf[c->tail] = e.val; c->tail = (c->tail + 1) % c->cap; c->count++; e.t->xfer_done = 1; w = e.t; break; }
                    }
                    won = 1;
                }
            } else if (c->closed) {
                if (gmb_sel_claim(sel, i)) { sel->winner_ok = 0; won = 1; }
            } else if (c->sw_count > 0 && c->sw[c->sw_head].has_val) {
                if (gmb_sel_claim(sel, i)) {
                    gmb_waiter_t e; gmb_wpop(&c->sw, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, &e);
                    sel->winner_val = e.val;
                    if (e.sel) { if (gmb_sel_claim(e.sel, -1)) w = e.t; gmb_sel_deref(e.sel); }
                    else { e.t->xfer_done = 1; w = e.t; }
                    won = 1;
                }
            }
        } else {   /* CASE SEND */
            if (!c->closed) {
                if (c->rw_count > 0 && c->rw[c->rw_head].out) {
                    if (gmb_sel_claim(sel, i)) {
                        gmb_waiter_t e; gmb_wpop(&c->rw, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count, &e);
                        *e.out = send_vals[i]; if (e.ok) *e.ok = 1;
                        if (e.sel) { if (gmb_sel_claim(e.sel, -1)) w = e.t; gmb_sel_deref(e.sel); }
                        else { e.t->xfer_done = 1; w = e.t; }
                        won = 1;
                    }
                } else if (c->cap > 0 && c->count < c->cap) {
                    if (gmb_sel_claim(sel, i)) {
                        c->buf[c->tail] = send_vals[i]; c->tail = (c->tail + 1) % c->cap; c->count++;
                        won = 1;
                    }
                }
            }
        }
        gmb_unlock(&c->lock);
        if (won) { if (w) gmb_sched_enqueue(w); return sel; }   /* case servito: niente block */
    }
    gmb_block();
    sel->settled = 1;   /* da qui ogni entry residua è stale (wake già avvenuto) */
    return sel;
}

/* ── WAITKEY/INKEY park-based per coroutine GPM (residuo C3-C99, 2026-09-09) ──────
 * L'emitter C# in modalità GPM (programConcurrency) emetteva gmb_console_readkey(0)
 * INLINE nella coroutine → bloccava l'INTERO worker pthread (con GOMAXPROCS=1 congela
 * tutte le altre goroutine; con pool multi-worker ne occupa uno per sempre). Queste
 * funzioni cedono il worker tra i tentativi: poll NON bloccante (gmb_console_readkey_nb,
 * runtime condiviso) + gmb_yield_ticks(20) quando il tasto non è ancora pronto.
 * gmb_yield_ticks parcheggia la coroutine (context switch) e il sleep_watcher la
 * risveglia dopo 20ms → le altre goroutine proseguono. timeout_ms<=0 = attesa indefinita
 * (WAITKEY/GET_KEY); timeout_ms>0 = attesa limitata (INKEY k$, ms — deadline). Ritorna
 * il tasto come stringa (sempre allocata; "" se timeout/nessun terminale). Mai spin-wait. */
extern int gmb_gfx_screen_on(void);
extern int gmb_gfx_input_poll_key(char* out, int maxlen);

gmb_string_t gmb_key_wait_park(int timeout_ms)
{
    int64_t deadline_ms = 0;
    if (timeout_ms > 0) deadline_ms = gmb_c99_now_ms() + timeout_ms;
    for (;;)
    {
        /* SORGENTI agnostiche dal backend: si provano TUTTE, in ordine, ad ogni giro.
         * (grafica: coda finestra; test/TUI: typeahead scriptata; altrimenti tty). */
        char kb[256];
        if (gmb_gfx_input_poll_key(kb, (int)sizeof(kb))) return gmb_string_alloc(kb);
        { extern int gmb_tui_global_typeahead_read(void);
          int tk = gmb_tui_global_typeahead_read();
          if (tk >= 0) {
              char b[3] = {0,0,0};
              switch (tk) {
                  case 27: b[0] = 0x1b; break;
                  case 13: b[0] = '\r'; break;
                  case 9:  b[0] = '\t'; break;
                  case 127: b[0] = 0x7f; break;
                  case 19: b[0] = 0x1c; break;
                  case 4:  b[0] = 0x1d; break;
                  case 5:  b[0] = 0x1e; break;
                  case 24: b[0] = 0x1f; break;
                  default: b[0] = (char)(tk & 0xff); break;
              }
              return gmb_string_alloc(b);
          } }
        { extern int isatty(int);
          if (isatty(0)) {   /* console solo con un vero terminale (headless: nessun ESC fantasma) */
              gmb_string_t out = { 0, 0, 0 };
              int r = gmb_console_readkey_nb(&out);
              if (r > 0) return out;
              gmb_string_free(&out);
          } }
        if (timeout_ms > 0 && gmb_c99_now_ms() >= deadline_ms) return gmb_string_alloc("");
        if (gmb_gfx_screen_on()) { struct timespec ts = { 0, 1000000 }; nanosleep(&ts, 0); }
        else gmb_yield_ticks(20);
    }
}


