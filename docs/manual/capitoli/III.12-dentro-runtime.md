# III.12 — Dentro il runtime

## Cosa ottieni da questo capitolo

Cosa succede fra il tuo `.bas` e il processo che stampa. Non serve saperlo per scrivere un
programma, serve per capire un errore strano, un output che arriva in ordine diverso, un
`SLEEP` che non blocca tutto. È il capitolo più tecnico del volume, ed è l'unico che parla di
come è fatto l'esecutore invece di come si usa.

## 1. Quattro backend, un solo linguaggio

| Backend | Come esegue | Comando |
|---|---|---|
| **IL** | compila in un eseguibile nativo (AOT) | `gmb file.bas -n` |
| **PCodeVM** | bytecode in-process | `--pcode`, `test_ilrun --engine=pcode` |
| **VM/C** | `.chunk` A1 eseguito da `gmb_vm_host` (processo esterno) | `gmb file.bas` |
| **C99Native** | `.bas` → C → `cc` → eseguibile | `tools/sh/gmb_c99native_run.sh` |

Il programma è lo stesso; cambia il percorso. `tools/sh/parity4` esegue lo stesso file su tutti
e quattro e confronta l'output normalizzato: è il modo con cui il progetto si assicura che le
quattro strade portino allo stesso risultato.

## 2. Da sorgente a chunk: A1/GMBC

Il percorso classico non passa l'albero sintattico alla VM. Traduce il programma in un **chunk
A1**: un file binario con il magic `GMBC`, la versione, i simboli, le costanti e le istruzioni in
**big-endian**. Lo produce il serializzatore C# e lo legge `gmb_chunk.c` della VM/C.

```
file.bas → lexer → parser → linker → chunk A1 (.prg/.chunk) → gmb_vm_host → output
```

Il chunk è il formato di scambio: `gmb file.prg` e `gmb file.chunk` saltano la compilazione e
chiamano direttamente l'host. Un `.pasm` è la stessa cosa in forma leggibile (disassemblaggio),
utile per capire cosa fa davvero il compilatore.

## 3. Il percorso C: un file sorgente, un compilatore di sistema

C99Native non produce bytecode: produce **un unico file C** (`prog.c`) che include il runtime
condiviso (`gmb_runtime_c`) e viene compilato con `cc`. Il programma finito è un eseguibile che
non ha bisogno né di .NET né della VM.

Questa è la ragione per cui il libro usa questo backend per gli esempi: quel che stampa viene da
un binario normale, compilato da un compilatore normale.

```
$ bash tools/sh/gmb_c99native_run.sh ../shared/samples/Native/HelloNative.bas hn1 ""
Hello from Native AOT!
3 + 4 =  7
str: basic native string
iter  1
iter  2
iter  3
NATIVE OK
```

## 4. La GPM: chi esegue il codice BASIC

Il modello è quello delle goroutine: **N worker** (thread di sistema) eseguono i **task**. Un
task è una coroutine con stack proprio (1 MB) che **commuta solo ai punti di blocco BASIC**.

| Elemento | Ruolo |
|---|---|
| coda FIFO dei runnable | scheduling **round-robin deterministico** |
| stato del task | `0` runnable, `1` running, `2` blocked, `3` finished |
| `gmb_cur()` | corrente del worker, in TLS |
| commutatore di contesto | assembly su POSIX, **fiber** su Windows |

Le tre guardie sotto il lock del task (`executing`, `in_queue`, `wake_pending`) impediscono i
tre errori classici di uno scheduler: accodare un task finito, accodare un task mentre sta
girando, accodarlo due volte. Il terzo caso è il più insidioso: chi sveglia un task mentre
gira non lo accoda, alza `wake_pending`, e sarà `release_executing` a rimetterlo in coda.

## 5. Perché un worker non si blocca mai

Un worker non può fermarsi ad aspettare: se lo facesse, gli altri task su quel worker
resterebbero fermi. Quindi ogni attesa è un **park**: il task si toglie dalla coda, l'operazione
gira fuori (pool, thread di rete, timer), il task viene riaccodato quando il risultato è pronto.

| Cosa blocca | Come si sblocca |
|---|---|
| `SEND` su canale pieno | un `RECEIVE` libera un posto |
| `RECEIVE` su canale vuoto | un `SEND` mette un valore |
| `WAIT` su gruppo | l'ultimo `DONE` |
| `SLEEP ms` | il timer |
| `GO FUNCTION … INTO` | il valore della future |
| `FETCH`, SQL pesante, I/O | il completamento dell'operazione |

I canali sono **retry-based**: il valore passa sempre dal buffer circolare (capacità
predefinita 10); le code di attesa servono solo a sapere **chi** svegliare, e chi si sveglia
**riesegue** l'operazione e ritrova la condizione aggiornata. È il motivo per cui non ci sono
risvegli persi.

## 6. Il confine che non si attraversa

Un handler BASIC non viene mai invocato sul thread che ha letto l'evento. Il thread esterno
salva lo stato, e il dispatch avviene come **task GPM** con contesto esplicito. Questa non è
una preferenza di stile: senza, un handler può partire con lo stato sbagliato e il programma si
blocca in modo intermittente — un caso reale del 2026, risolto proprio così.

## 7. Cosa osservi tu

| Segnale | Significato |
|---|---|
| `exit=0` | il programma è arrivato in fondo |
| output in ordine diverso | due task scrivono: l'ordine dipende dallo scheduler |
| `SLEEP` non ferma gli altri task | è il park: dorme solo quel task |
| un canale con capacità 0 | `SEND` e `RECEIVE` devono incontrarsi, altrimenti deadlock |

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| deadlock con `SEND` | nessuno riceve, buffer pieno | aggiungi un `RECEIVE` o capacita il canale |
| deadlock con `WAIT` | un figlio non fa `DONE` | controlla ogni uscita del figlio, `EXIT` compreso |
| il programma finisce prima dei task | manca la `WAIT` | attendi i figli prima di uscire |
| `SLEEP` usato come sincronizzazione | è un'attesa, non una barriera | usa `WAIT` o un canale |

## 9. Dove andare adesso

- **III.13** — GPM dal lato di chi scrive: canali, mutex, select.
- **III.14** — errori e diagnostica.
- **III.16** — chiamare codice C dal BASIC, che è l'altro modo di parlare col runtime.













> **Nota (2026-11-05):** `SPECIAL` non è più un namespace builtin. È esposto via FFI
> da `libgmbmath` (`shared/libs/math`, sorgenti inclusi) tramite
> `shared/include/special.inc`. Le chiamate `SPECIAL.*` restano identiche.


<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### C99-GPM-Technical-Manual

*Fonte: `C99-GPM-Technical-Manual`*


## 1. Che cos'è la GPM C99

## 2. Il portability layer (POSIX assembly vs Windows fibers)

/* ===== Portability layer (POSIX vs Windows) ===== */
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    typedef SRWLOCK gmb_mutex_t;          /* scheduler lock */
    typedef CONDITION_VARIABLE gmb_cond_t;
    typedef HANDLE gmb_thread_t;
    typedef LPVOID gmb_ctx_t;             /* Fiber (coroutine Win32) */
    #define GMB_MUTEX_INIT(m)    InitializeSRWLock(m)
    #define GMB_MUTEX_LOCK(m)    AcquireSRWLockExclusive(m)
    #define GMB_MUTEX_UNLOCK(m)  ReleaseSRWLockExclusive(m)
    #define GMB_COND_INIT(c)     InitializeConditionVariable(c)
    #define GMB_COND_SIGNAL(c)   WakeConditionVariable(c)
    #define GMB_COND_BROADCAST(c) WakeAllConditionVariable(c)
    #define GMB_COND_WAIT(c,m)   SleepConditionVariableSRW((c),(m),INFINITE,0)
    #define GMB_THREAD_CREATE(t,fn,arg) ((*(t) = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)(fn),(arg),0,NULL)) != NULL)
    #define GMB_THREAD_JOIN(t)   WaitForSingleObject((t),INFINITE)


## 3. Il context switch in assembly (POSIX)

### 3.1 `gmb_ctx_t`

#if defined(__aarch64__)
typedef struct {
    void* sp; void* pc;
    unsigned long x19,x20,x21,x22,x23,x24,x25,x26,x27,x28,x29;
    double d8,d9,d10,d11,d12,d13,d14,d15;    /* SIMD callee-saved */
} gmb_ctx_t;
#elif defined(__x86_64__)
typedef struct {
    void* sp; void* pc;
    unsigned long rbx,rbp,r12,r13,r14,r15;
} gmb_ctx_t;
#else
#include <ucontext.h>
typedef ucontext_t gmb_ctx_t;                /* FALLBACK arch esotiche */
#endif


### 3.2 `gmb_ctx_switch` (CODICE ESATTO)

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


### 3.3 `gmb_ctx_make` (stack iniziale — CODICE ESATTO)

/* aarch64 */
static void gmb_ctx_make(gmb_ctx_t* ctx, void (*fn)(void), char* stack, size_t sz) {
    unsigned long* sp = (unsigned long*)(stack + sz);
    sp -= 20;                                    /* 10 coppie */
    sp[0]=0; sp[1]=(unsigned long)fn;            /* x29, x30(=resume pc = trampoline) */
    sp[2]=0; sp[3]=0; sp[4]=0; sp[5]=0; sp[6]=0; sp[7]=0; sp[8]=0; sp[9]=0;   /* d14..d8 */
    sp[10]=0; sp[11]=0; sp[12]=0; sp[13]=0; sp[14]=0; sp[15]=0; sp[16]=0; sp[17]=0;
    sp[18]=0; sp[19]=0;                          /* x27..x19 */
    ctx->sp = sp;
    ctx->pc = (void*)fn;
}

/* x86_64 */
static void gmb_ctx_make(gmb_ctx_t* ctx, void (*fn)(void), char* stack, size_t sz) {
    void** sp = (void**)(stack + sz);
    sp -= 7;                                     /* 6 regs + return address */


### 3.4 Windows (Fibers)

/* creazione task */
t->ctx = CreateFiber((SIZE_T)(1 << 20), (LPFIBER_START_ROUTINE)gmb_task_trampoline, NULL);
/* worker all'avvio */
g_worker_fiber = ConvertThreadToFiber(NULL);
/* worker → task */
SwitchToFiber(t->ctx);
/* task → worker (park) */
SwitchToFiber(g_worker_fiber);


## 4. Il TLS: `gmb_cur()` noinline (FIX CRITICO)

/* REGOLA D'ORO: leggere g_current SEMPRE via funzione noinline. */
__attribute__((noinline)) static gmb_task_t* gmb_cur(void) { return g_current; }


gmb_task_t* g_current = gmb_cur();   /* variabile locale — mai il globale diretto */


## 5. Strutture dati

### 5.1 Il task (goroutine)

struct gmb_task {
    gmb_ctx_t ctx;                 /* contesto coroutine (assembly/fiber) */
    void (*entry_fn)(void*);
    void* entry_arg;
    int state;                     /* GMB_TS_RUNNABLE/RUNNING/BLOCKED/FINISHED */
    int in_queue;                  /* già in coda ready? */
    int executing;                 /* in-flight su un worker (guardia anti-doppio) */
    int wake_pending;              /* wake avvenuto mentre eseguiva → riaccoda */
    int was_yield;                 /* TaskYield esplicito → HandlePostTick riaccoda */
    gmb_chan_t* chan;              /* canale su cui è parcheggiato */
    gmb_val_t* out;                /* RECEIVE: destinazione */
    gmb_val_t send_val;            /* SEND parcheggiato: valore */
    int is_send;                   /* 1=SEND 0=RECEIVE */
    gmb_wg_t* wg;                  /* waitgroup su cui è parcheggiato */
    long long sleep_until_ms;      /* 0 = non dorme */
    gmb_task_t* next;              /* coda d'attesa (wg) */


### 5.2 Lo scheduler

struct gmb_scheduler {
    gmb_task_t** queue; int qcap, qhead, qtail, qcount;   /* coda FIFO */
    gmb_mutex_t qlock; gmb_cond_t qcond;                  /* Take bloccante */
    int shutdown;
    gmb_task_t** all; int all_cap, all_count;             /* lista globale */
    gmb_mutex_t all_lock;
};
static gmb_scheduler_t g_sched;
/* per-worker: */
static __thread gmb_ctx_t g_worker_ctx;      /* POSIX (assembly) */
static __thread LPVOID g_worker_fiber;       /* Windows */
static __thread gmb_task_t* g_current;


### 5.3 Canale (retry-based, cap default 10)

struct gmb_chan {
    gmb_val_t* buf; int count, cap, head, tail;        /* buffer circolare */
    gmb_task_t** send_wait; int sw_cap, sw_head, sw_tail, sw_count;  /* mittenti bloccati */
    gmb_task_t** recv_wait; int rw_cap, rw_head, rw_tail, rw_count;  /* riceventi bloccati */
    gmb_lock_t lock;
};


### 5.4 WaitGroup, Future, Select

struct gmb_wg { int count; gmb_task_t* waiters; gmb_lock_t lock; };
struct gmb_future { gmb_lock_t lock; int resolved; gmb_val_t value; gmb_task_t* waiters; };
/* GO SELECT: registrazione multi-canale + park; al risveglio RI-VALUTA tutti i case (§ 8). */


## 6. Il scheduler (core)

### 6.1 Enqueue (coda FIFO, guardia anti-doppio)

static void gmb_sched_enqueue(gmb_task_t* t) {
    gmb_lock(&t->lock);
    if (t->state == GMB_TS_FINISHED) { gmb_unlock(&t->lock); return; }
    if (t->executing) { t->wake_pending = 1; gmb_unlock(&t->lock); return; }
    if (t->in_queue) { gmb_unlock(&t->lock); return; }
    t->in_queue = 1; t->state = GMB_TS_RUNNABLE;
    gmb_unlock(&t->lock);
    GMB_MUTEX_LOCK(&g_sched.qlock);
    if (g_sched.qcount >= g_sched.qcap) { /* cresce la coda */ }
    if (!g_sched.shutdown) {
        g_sched.queue[g_sched.qtail] = t;
        g_sched.qtail = (g_sched.qtail + 1) % g_sched.qcap;
        g_sched.qcount++;
        GMB_COND_SIGNAL(&g_sched.qcond);
    }
    GMB_MUTEX_UNLOCK(&g_sched.qlock);


### 6.2 FindRunnable / ReleaseExecuting

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


### 6.3 Worker loop (replica fedele del `worker_loop` §16)

static void* gmb_worker(void* arg) {
#if defined(_WIN32) || defined(_WIN64)
    g_worker_fiber = ConvertThreadToFiber(NULL);
#endif
    for (;;) {
        gmb_task_t* t = gmb_sched_find_runnable();
        if (!t) break;                          /* shutdown */
        t->state = GMB_TS_RUNNING;
        g_current = t;
#if defined(_WIN32) || defined(_WIN64)
        SwitchToFiber(t->ctx);
#else
        gmb_ctx_switch(&g_worker_ctx, &t->ctx);
#endif
        g_current = NULL;
        /* HandlePostTick */


### 6.4 Sleep-watcher (SLEEP wall-clock)

static void* gmb_sleep_watcher(void* arg) {
    while (gmb_sched_has_active()) {
        gmb_sched_wake_expired_sleepers(gmb_now_ms());
        int64_t next = gmb_sched_earliest_sleep();
        int wait_ms = next < 0 ? 50 : (next - gmb_now_ms() > 250 ? 250 : (int)(next - gmb_now_ms()));
        if (wait_ms < 1) wait_ms = 1;
        gmb_sleep_ms(wait_ms);
    }
    return NULL;
}


### 6.5 Driver (`gmb_gpm_start`)

static void gmb_gpm_start(void (*main_entry)(void*)) {
    GMB_MUTEX_INIT(&g_sched.qlock); GMB_COND_INIT(&g_sched.qcond); GMB_MUTEX_INIT(&g_sched.all_lock);
    g_sched.qcap = 64; g_sched.queue = (gmb_task_t**)calloc(64, sizeof(gmb_task_t*));
    g_sched.all_cap = 64; g_sched.all = (gmb_task_t**)calloc(64, sizeof(gmb_task_t*));
    gmb_task_spawn(main_entry, NULL);
    int nw = env ? atoi(getenv("GMBASIC_GOMAXPROCS")) : 0;
    if (nw <= 0) nw = gmb_ncpu();
    if (nw <= 0) nw = 4;
    if (nw > 8) nw = 8;
    gmb_thread_t workers[8]; gmb_thread_t watcher;
    for (int i = 0; i < nw; i++) GMB_THREAD_CREATE(&workers[i], gmb_worker, NULL);
    GMB_THREAD_CREATE(&watcher, gmb_sleep_watcher, NULL);
    while (gmb_sched_has_active()) gmb_sleep_ms(1);    /* driver polla has_active */
    GMB_MUTEX_LOCK(&g_sched.qlock); g_sched.shutdown = 1;
    GMB_COND_BROADCAST(&g_sched.qcond); GMB_MUTEX_UNLOCK(&g_sched.qlock);
    for (int i = 0; i < nw; i++) GMB_THREAD_JOIN(workers[i]);


## 7. Le primitive di blocco

static void gmb_block(void) { gmb_task_t* g_current = gmb_cur();
#if defined(_WIN32) || defined(_WIN64)
    SwitchToFiber(g_worker_fiber);
#else
    gmb_ctx_switch(&g_current->ctx, &g_worker_ctx);
#endif
}
static void gmb_yield(void) { gmb_task_t* g_current = gmb_cur(); g_current->was_yield = 1;
#if defined(_WIN32) || defined(_WIN64)
    SwitchToFiber(g_worker_fiber);
#else
    gmb_ctx_switch(&g_current->ctx, &g_worker_ctx);
#endif
}
void gmb_yield_ticks(long long ms) { gmb_task_t* g_current = gmb_cur();
    if (ms <= 0) return;   /* §10: SLEEP<=0 prosegue senza bloccare */


### 7.1 Canali (retry-based)

int gmb_chan_send_or_park(gmb_chan_t* c, gmb_val_t v, gmb_task_t** woken) {
    gmb_task_t* g_current = gmb_cur();
    *woken = NULL;
    gmb_lock(&c->lock);
    if (c->count < c->cap) {
        c->buf[c->tail] = v; c->tail = (c->tail + 1) % c->cap; c->count++;
        *woken = gmb_cpop(&c->recv_wait, &c->rw_cap, &c->rw_head, &c->rw_tail, &c->rw_count);
        gmb_unlock(&c->lock);
        return 1;                                  /* inviato */
    }
    g_current->state = GMB_TS_BLOCKED;             /* dentro il lock (§16.3) */
    g_current->chan = c; g_current->send_val = v; g_current->is_send = 1;
    gmb_cpush(&c->send_wait, &c->sw_cap, &c->sw_head, &c->sw_tail, &c->sw_count, g_current);
    gmb_unlock(&c->lock);
    return 0;                                      /* parcheggiato */
}


### 7.2 WaitGroup

void gmb_wg_add(gmb_wg_t* w, int n) { gmb_lock(&w->lock); w->count += n; gmb_unlock(&w->lock); }
void gmb_wg_done(gmb_wg_t* w) {
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
        g_current->state = GMB_TS_BLOCKED;         /* dentro il lock */
        g_current->wg = w; g_current->next = w->waiters; w->waiters = g_current;
        gmb_unlock(&w->lock);


### 7.3 Future (implicit await §11)

void gmb_future_resolve(gmb_future_t* f, gmb_val_t v) {
    gmb_task_t* w = NULL;
    gmb_lock(&f->lock);
    if (!f->resolved) { f->resolved = 1; f->value = v; w = f->waiters; f->waiters = NULL; }
    gmb_unlock(&f->lock);
    for (; w; ) { gmb_task_t* nx = w->next; gmb_sched_enqueue(w); w = nx; }
}
void gmb_future_await(gmb_future_t* f) {
    for (;;) {
        gmb_lock(&f->lock);
        if (f->resolved) { gmb_unlock(&f->lock); return; }
        gmb_task_t* g = gmb_cur();
        g->state = GMB_TS_BLOCKED; g->next = f->waiters; f->waiters = g;
        gmb_unlock(&f->lock);
        gmb_block();
    }


### 7.4 GO EXIT (§11.4)

static void gmb_task_exit(void) {
    gmb_task_t* g = gmb_cur();
    gmb_lock(&g->lock); g->state = GMB_TS_FINISHED; gmb_unlock(&g->lock);
#if defined(_WIN32) || defined(_WIN64)
    SwitchToFiber(g_worker_fiber);
#else
    gmb_ctx_switch(&g->ctx, &g_worker_ctx);
#endif
}


### 7.5 Trampoline

#if defined(_WIN32) || defined(_WIN64)
static void __stdcall gmb_task_trampoline(void* _p) { gmb_task_t* g_current = gmb_cur();
#else
static void gmb_task_trampoline(void) { gmb_task_t* g_current = gmb_cur();
#endif
    g_current->entry_fn(g_current->entry_arg);
    gmb_lock(&g_current->lock); g_current->state = GMB_TS_FINISHED; gmb_unlock(&g_current->lock);
#if defined(_WIN32) || defined(_WIN64)
    SwitchToFiber(g_worker_fiber);
#else
    gmb_ctx_switch(&g_current->ctx, &g_worker_ctx);
#endif
}


## 8. GO SELECT (§9)

void gmb_select_park(gmb_chan_t** chans, int* dir, gmb_val_t* send_vals, int n) {
    GMB_MUTEX_LOCK(&g_sched.qlock);   /* serializza la registrazione multi-canale */
    for (int i = 0; i < n; i++) {
        gmb_lock(&chans[i]->lock);
        g_current->state = GMB_TS_BLOCKED;
        if (dir[i] == 1) gmb_cpush(&chans[i]->send_wait, ...);
        else             gmb_cpush(&chans[i]->recv_wait, ...);
        gmb_unlock(&chans[i]->lock);
    }
    GMB_MUTEX_UNLOCK(&g_sched.qlock);
    gmb_block();
}


/* forma generata dal transpiler per `GO SELECT ... END SELECT` */
__goselect_N_retry:
    { gmb_val_t v0 = gmb_val_nil();
      if (gmb_chan_tryrecv(C1, &v0)) { target = v0.v_int; ...body...; goto __goselect_N_done; } }
    { gmb_val_t v1 = gmb_val_nil();
      if (gmb_chan_tryrecv(C2, &v1)) { ...body...; goto __goselect_N_done; } }
    /* DEFAULT (se presente): polling non bloccante */
    ...
    /* nessun case pronto e nessun DEFAULT → parcheggia su tutti i canali, poi ri-valuta */
    { gmb_chan_t* chans[...] = {C1, C2}; int dir[...] = {0, 0}; gmb_val_t svals[...];
      gmb_select_park(chans, dir, svals, n); goto __goselect_N_retry; }
__goselect_N_done:;


## 9. Spawn dei task

### 9.1 `gmb_task_spawn`

void gmb_task_spawn(void (*fn)(void*), void* arg) {
    gmb_task_t* t = (gmb_task_t*)calloc(1, sizeof(gmb_task_t));
    t->stack = (char*)malloc(1 << 20);                       /* 1MB */
    t->entry_fn = fn; t->entry_arg = arg; t->state = GMB_TS_RUNNABLE;
#if defined(_WIN32) || defined(_WIN64)
    t->ctx = CreateFiber((SIZE_T)(1 << 20), (LPFIBER_START_ROUTINE)gmb_task_trampoline, NULL);
#else
    gmb_ctx_make(&t->ctx, gmb_task_trampoline, t->stack, (size_t)(1 << 20));
#endif
    GMB_MUTEX_LOCK(&g_sched.all_lock);  /* registra in all[] */  GMB_MUTEX_UNLOCK(&g_sched.all_lock);
    gmb_sched_enqueue(t);
}


### 9.2 GO SUB (`EmitGoProcedure`)

typedef struct gmb_go_P_0_args { T0 a0; T1 a1; } gmb_go_P_0_args;
static void gmb_go_P_0(void* p) {
    gmb_go_P_0_args* a = (gmb_go_P_0_args*)p;
    gmb_user_P(a->a0, a->a1);        /* §7.1: DONE è ESPLICITO nel body — il wrapper NON auto-decrementa */
    free(a);
}
/* call-site GO SUB P(C, WG) IN WG */
if (WG) gmb_wg_add(WG, 1);           /* §7.1: Add(1) al lancio */
gmb_task_spawn(gmb_go_P_0, __go);


### 9.3 GO FUNCTION INTO (`EmitGoFunction` + future §11)

gmb_future_t* TotaleFinale_fut = gmb_future_new();   /* dichiarato FUORI dal blocco spawn */
{ /* spawn (scope per __gof) */
  args* __gof = calloc(...); __gof->a0=...; __gof->out = &TotaleFinale; __gof->fut = TotaleFinale_fut;
  gmb_task_spawn(gmb_gofn_Manager_0, __gof);
}
/* wrapper */
static void gmb_gofn_Manager_0(void* p) {
    args* a = p;
    *a->out = gmb_user_Manager(a->a0, a->a1);
    gmb_future_resolve(a->fut, gmb_make_val_int(*a->out));
    free(a);
}
/* OGNI lettura di TotaleFinale diventa implicit await (§11): */
TotaleFinale_fut ? (gmb_future_await(TotaleFinale_fut), TotaleFinale) : TotaleFinale


## 10. Mappatura statement BASIC → C (emessa dal transpiler)
| BASIC | C generato |
|---|---|
| `GO SUB P(a) [IN wg]` | wrapper + `gmb_wg_add(wg,1)` + `gmb_task_spawn(...)` |
| `GO FUNCTION F(a) INTO v` | future + wrapper che risolve + implicit await sulle letture di `v` |
| `SEND x TO c` | `gmb_chan_send(c, gmb_make_val_int/double/string(x))` |
| `RECEIVE v FROM c` | `v = gmb_chan_recv(c).v_int/.v_double/.v_string` (target implicito dichiarato al volo) |
| `WAIT wg` | `gmb_wg_wait(wg)` (se variabile `gmb_wg_t*`) |
| `WAIT <ms>` / `SLEEP <ms>` | `gmb_yield_ticks((long long)(ms))` |
| `DONE wg` | `gmb_wg_done(wg)` |
| `GO SELECT ...` | loop retry + tryrecv/trysend + `gmb_select_park` + DEFAULT |
| `GO EXIT` | `gmb_task_exit()` |
| `TaskYield()` | `gmb_yield()` (via `was_yield`) |
| `DIM c AS CHANNEL OF T` | `gmb_chan_t* c = gmb_chan_new(10);` |
| `DIM wg AS WAITGROUP` | `gmb_wg_t* wg = gmb_wg_new();` |
| `AWAIT expr` | valutazione sincrona (`EmitExpression(expr.Target)`) |

## 11. Integrazione nel transpiler (C# `C99CodeEmitter`)

## 11bis. Il doppio percorso e il flag di compilazione

### Perché due percorsi
| Percorso | Quando | Costo |
|---|---|---|
| **Lineare** | `programConcurrency == false` → il `main` C chiama `gmb_user_Main()` direttamente | veloce: nessuno scheduler, nessun worker thread, zero overhead coroutine |
| **GPM** | `programConcurrency == true` → `gmb_gpm_start(entry)` (scheduler + worker + coroutine) | overhead del prologo pthread + scheduling |

### Il bug di detection che ha motivato il default `Always`

### Le tre modalità (`C99GpmMode`)
| Modalità | Semantica |
|---|---|
| `Always` (**default**) | `programConcurrency = true` sempre. **Rete di sicurezza**: anche se la detection manca qualcosa, la GPM gira comunque → mai crash. Costo: scheduler anche sui programmi puramente CPU. |
| `Auto` | `programConcurrency = UsesConcurrency(program)` — il rilevamento statico decide. |
| `Linear` | Preferisce il percorso veloce ma è una **preferenza, non un override cieco**: se `UsesConcurrency` è vero, forzare il lineare sarebbe semanticamente errato → warning `GPM:Linear: il programma usa concorrenza, ripiego su GPM (Auto).` e ripiego su GPM. |

### Dove si imposta il flag

## 12. Esempio completo (BASIC → C essenziale)

SUB Produttore(C AS CHANNEL OF INTEGER, WG AS WAITGROUP)
    FOR i = 1 TO 3
        SLEEP 100
        SEND i TO C
    NEXT i
    DONE WG
END SUB
SUB Main()
    DIM C AS CHANNEL OF INTEGER
    DIM WG AS WAITGROUP
    GO SUB Produttore(C, WG) IN WG
    FOR j = 1 TO 3
        DIM v AS INTEGER
        RECEIVE v FROM C
        PRINT "ricevuto "; v
    NEXT j


## 13. Limiti noti e FASE 2

## 14. Verifica

### Executor-Architecture

*Fonte: `Executor-Architecture`*


## File

Executor/                 # orchestrazione e compilazione C# (`Orchestrator.Compile`)
REPL/                     # CLI, REPL, impostazioni e dispatch IL/PCode gestita/VM-C/C99


## Serializzazione chunk A1 (D2)

## InternalsVisibleTo

<InternalsVisibleTo Include="Executor" />


## CLI / Riga di comando

gmb                              → REPL interattiva
gmb <file.bas>                   → compila + esegui via VM/C (chunk A1), poi esci
gmb <file.bas> -n                → compila + esegui IL nativo, poi esci
gmb <file.bas> -repl             → esegui, poi rimani in REPL
gmb <file.bas> -n -repl          → esegui IL, poi rimani in REPL
gmb <file.prg>                   → carica + esegui .prg con la VM/C, poi esci
gmb <file.chunk>                 → esegue un chunk A1 con gmb_vm_host (VM/C)


## Ctrl+C / BREAK

## Flusso COMPILE/RUN/EXEC

### COMPILE

COMPILE <file>           → .prg (chunk A1) via Compiler + PrgFormat.Write
COMPILE <file> -dump     → .prg + .pasm (P-Code disassembly)
COMPILE <file> -n        → eseguibile IL (AOT) in bin/ (default)
COMPILE <file> -n -local → eseguibile IL nella stessa cartella del .bas
COMPILE <file> -n -full  → ⚠️ legacy (D3): bin/ con runtime .NET (self-contained) NON è più lo standard; IL = AOT nativo
COMPILE <file> -n -r <rid> → target RID


### RUN

RUN                      → esegue programma REPL via VM/C (chunk A1)
RUN -n                   → compila + esegue IL nativo
RUN <file>               → compila + esegue file via VM/C
RUN <file> -n            → compila + esegue file via IL


### EXEC

EXEC <file>              → esegue .prg senza ricompilare
EXEC <file> -n           → esegue eseguibile nativo senza ricompilare


## REPL Commands (45 totali)
| Categoria | Comandi |
|---|---|
| **Programma** | LIST, NEW, AUTO, RENUM, DELETE/DEL |
| **File I/O** | SAVE, LOAD, SAMPLE |
| **Compilazione** | COMPILE, RUN, EXEC, LOADPRG, PRGINFO |
| **Analisi** | AST, DASM, IL |
| **File system** | LS, CD, WHERE/WH, RN/RENAME, DL/DELF, CP/COPY, DP/DUPLICATE, BL/BACKUP |
| **Deploy** | DEPLOY/DEP |
| **Utilità** | FLATTEN (stub), CLS, HELP, QUIT/EXIT |

### PaginatedMenu

## DEPLOY

DEPLOY <file> [-r <runtime>]


## Test di regressione AppHost

dotnet run --project test_apphost/


## Esecuzione

## Differenze da Interpreter (deprecato)
| Interpreter (legacy) | Executor (unificato) |
|---|---|
| Executor AST (`Gpm.*.cs`, ~40 file) | Eliminato (D1) |
| `Executor.ExecutionSlice`, `CallFrame` AST | Eliminato |
| Console SDL2 (StandardConsole) | Solo `System.Console` (front-end) |
| `_runtime`, `_scheduler` GPM | VM/C GPM cooperativo | 
| REPL come partial class di `Executor` | `ReplEngine` standalone |
| `Environment.Exit()` su QUIT | Flag `_quitting` |
| Banner SDL2 centrato | Banner ASCII semplice |

## Riferimenti

### GM-Basic-BRT-Reference-Card

*Fonte: `GM-Basic-BRT-Reference-Card`*


## IRuntimeHost — Mappa servizi

_runtime.Console.xxx()      ConsoleService/HeadlessConsoleService
_runtime.Keyboard.xxx()     KeyboardService/HeadlessKeyboardService
_runtime.Clipboard.xxx()    ClipboardService
_runtime.File.xxx()         FileService
_runtime.Time.xxx()         TimeService
_runtime.Crypto.xxx()       CryptoService
_runtime.Memory.xxx()       MemoryService
_runtime.Ffi.xxx()          FfiService
_runtime.Network.xxx()      NetworkService
_runtime.Http.xxx()         HttpService
_runtime.Database.xxx()     DatabaseService (BRT/CommonLib — ISAM/DBF/SPEED/Ledger)
                            /NullDatabaseService
_runtime.Graphics.xxx()     CommonGraphics (SDL2 — community)
_runtime.Sandbox            null (futuro)


## DatabaseService — Engine mapping

_runtime.Database.DbfUse(path, idx, lock, ...) → new DbfEngine(...)
_runtime.Database.DbfSkip(h, count)            → engine.Skip(count)
_runtime.Database.DbfSeek(h, key)              → engine.Seek(key); return engine.Found
_runtime.Database.DbfGetRecord(h)              → engine.GetCurrentRecordValues()?.Values
_runtime.Database.DbfDelete(h, _)              → engine.DeleteCurrent(indexKeys)
_runtime.Database.DbfRecall(h)                 → engine.RecallCurrent(indexKeys)
_runtime.Database.DbfPack(h)                   → engine.Pack()
_runtime.Database.DbfZap(h)                    → engine.Zap()
_runtime.Database.DbfAppendBlank(h)            → engine.AppendBlankRecord(indexKeys)
_runtime.Database.DbfAppend(h, values)         → engine.AppendRecord(dict, indexKeys)
_runtime.Database.DbfReplace(h, idx, val)      → engine.PutCurrentRecord(vals, oldKeys, newKeys)
_runtime.Database.DbfSetDeleted(h, on)         → engine.SetDeleted(on)
_runtime.Database.DbfSetExact(h, on)           → engine.SetExact(on)
_runtime.Database.DbfCloseAll()                → engine.Dispose() per tutti
_runtime.Database.DbfReindex(h, _)             → engine.Reindex()
_runtime.Database.DbfGo(h, recno)              → engine.GoTop(); engine.Skip(recno-1)


## P-CODE VM — Opcode dispatch (comportamento legacy di riferimento)

PCodeProcessor.ExecuteTask(task, maxInstr)
│
├── 102/104 opcode (GMBasicAI)
│    └── ogni opcode → Pop/Push dallo stack + eventuale chiamata _runtime.*
│
├── OP_SYSCALL → ExecuteSyscall(task, code, argCount)
│    └── 52/52 SysCall (GMBasicAI) / 182/182 SysCall (GM-Basic)
│         └── _runtime.Database.Xxx() / _runtime.Graphics.Xxx() / _runtime.File.Xxx() / ...
│
└── OP_CALL_BUILTIN → ExecuteBuiltinFunction(funcId, args)
     └── 26 built-in functions (ABS, SGN, SIN, COS, LEN, CHR, VAL, ...)


## PCodeTask — Stato (legacy, riferimento)

PCodeTask
├── StackEntry[4096] Stack      ← operand stack (unificato)
│    └── struct { double Num; object? Ref; byte Type }
├── int Ip                       ← instruction pointer
├── int Sp                       ← stack pointer
├── PCodeCallFrame? Frame         ← frame corrente
├── object?[]? Globals            ← variabili globali
├── object?[] _locals             ← variabili locali
├── Dictionary<int, (double,double,double)> ForState  ← FOR loop
├── Dictionary<int, long> FileHandles ← handle file
├── Stack<TryInfo> TryStack       ← TRY/CATCH
├── int? ErrorHandlerIp           ← ON ERROR
└── PCodeTaskState State          ← Runnable/Finished/Suspended


## PrgFormat — .prg binary (formato legacy)

Magic:  0x50434F44  ("PCOD" in ASCII)
Version: 1

Layout:
  [Magic:4][Version:2][NameLen:4][Name:N]
  [CodeCount:4][Code:CodeCount]
  [ConstCount:4][Constants:var]
  [LineCount:4][Lines:LineCount*4]

PrgChunk:
  byte[] Code
  object?[] Constants
  int[] Lines
  string? SourceName


## CLI

gmb [file] [flags]

Flags:
  --pcode     Forza la VM/C (P-Code) anche per .bas
  --ast       Forza AST Executor (anche per .prg)
  -noconsole  Headless (nessuna finestra)
  -c / -s     Console SDL / System (unificato — grafica community)
  -l / -p / -e / -o / -ll   Verbose flags
  -DSYM=val   Define simbolo
  --run cmd   Esegui comando REPL

REPL commands (P-Code / VM/C):
  COMPILE <file.bas>  → produce file.prg (chunk A1, GMBC)
  LOADPRG <file.prg>  → carica .prg
  PRGINFO [file.prg]  → info .prg
  RUN                 → esegue con la VM/C se caricato .prg


### PRG-Format

*Fonte: `PRG-Format`*


## Magic Number

## Struttura
| Offset | Dimensione | Campo | Descrizione |
|--------|-----------|-------|-------------|
| 0 | 4 | Magic | `0x50434F44` |
| 4 | 2 | Version | 1 |
| 6 | 4 | NameLen | Lunghezza nome sorgente (byte) |
| 10 | N | Name | Nome file sorgente (UTF-8) |
| 10+N | 4 | CodeCount | Numero opcode |
| 14+N | CodeCount | Code | Bytecode p-code |
| | 4 | ConstCount | Numero costanti |
| | var | Constants | Pool costanti (tipizzate) |
| | 4 | LineCount | Numero righe sorgente |
| | 4*N | Lines | Mappa righe → opcode |

## Codifica Costanti
| Type | Dimensione | Formato |
|------|-----------|---------|
| 0 | 0 | null |
| 1 | 8 | double (IEEE 754 LE) |
| 2 | 4+N | string: 4 byte length + N byte UTF-8 |

## REPL Commands

COMPILE <file.bas>
  Compila .bas in .prg. Salva accanto al sorgente.
  Output: "Compiled: test.bas -> test.prg (52 opcodes, 7 const)"

LOADPRG <file.prg>
  Carica un .prg per l'esecuzione. Dopo LOADPRG, RUN usa la VM/C.
  Output: "Loaded: test.prg (52 opcodes)"

PRGINFO [file.prg]
  Mostra info su un .prg senza eseguirlo.
  Senza argomento, mostra info sul .prg caricato.
  Output: "PRG Info: test.bas (52 opcodes, 1024 bytes, valid: True)"


## Esempio

# REPL
> COMPILE "test.bas"
Compiled: test.bas -> test.prg (52 opcodes, 7 const)

> PRGINFO "test.prg"
PRG Info: test.bas (52 opcodes, 1024 bytes, valid: True)

> LOADPRG "test.prg"
Loaded: test.prg (52 opcodes)

> RUN
P-CODE VM: execution completed.


## CLI

# Esecuzione diretta .prg
gmb test.prg

# Forza la VM/C (P-Code) per .bas
gmb test.bas --pcode


## File Chiave
| File | Descrizione |
|------|-------------|
| `GMBasicSharp/common/BRT/ChunkA1Serializer.cs` | Serializzazione chunk A1 (GMBC) — formato attuale nel repo unificato |
| `GMBasicSharp/common/BRT/PrgFormat.cs` | Serializzazione/deserializzazione formato .prg legacy (PCOD) |
| `GMBasicSharp/REPL/ReplEngine.cs` | Comandi REPL COMPILE, LOADPRG, PRGINFO |
| `GMBasic/native/gmb_vm_c/` | VM/C: legge il chunk A1 e lo esegue |

### VM-C-GPM-Technical-Manual

*Fonte: `VM-C-GPM-Technical-Manual`*


## 1. Modello concettuale

GOMAXPROCS × worker (M) — thread OS
  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ worker 1 │  │ worker 2 │  │ worker N │   ← eseguono gmb_vm_tick(task)
  └────┬─────┘  └────┬─────┘  └────┬─────┘
       │             │             │
       └─────────────┼─────────────┘
                     ▼
          coda FIFO condivisa (BlockingCollection)
          gmb_scheduler_t.queue
                     │
                     ▼
          gmb_task_t (PCB): ip, stack[4096], sp, frame, globals
          — commutano SOLO ai punti di blocco (return 0 dal tick)


### 1.1 I tre ruoli
| Entità | Ruolo | È un thread OS? |
|---|---|---|
| `gmb_vm_t` | Runner (chunk + scheduler + hook di output) | No |
| `gmb_scheduler_t` | "Kernel": coda FIFO, `all_tasks`, sleep-watcher | No |
| `gmb_worker` (`worker_loop`) | Esegue tick di task diversi | **SÌ** (N thread) |
| `gmb_task_t` (G) | PCB della goroutine: tutto lo stato per riprendere | No (dati) |
| `gmb_channel_t` / `gmb_waitgroup_t` / `gmb_future_t` | Strutture di attesa | No (dati + lock) |

### 1.2 Regole fondanti (§16)

## 2. Strutture dati (gmb_gpm.h)

### 2.1 Stati task

enum {
    GMB_TS_RUNNABLE = 0,   /* in coda, pronto */
    GMB_TS_RUNNING  = 1,   /* in esecuzione su un worker */
    GMB_TS_BLOCKED  = 2,   /* parcheggiato su una struttura di attesa */
    GMB_TS_FINISHED = 3,   /* terminato */
    GMB_TS_DEBUG_PAUSED = 4,
};


### 2.2 `gmb_task_t` — il PCB

struct gmb_task {
    int id;
    int state;               /* GMB_TS_* */
    int in_queue;            /* anti-doppio-enqueue */
    int executing;           /* sta girando su un worker */
    int wake_pending;        /* enqueue durante l'esecuzione → riaccoda al release */
    int force_yield;         /* Gosched esplicito (TaskYield): worker riaccoda */
    pthread_mutex_t lock;    /* EnqueueLock per-task */
    int ip;                  /* Instruction Pointer di QUESTO task */
    gmb_stack_entry_t stack[GMB_TASK_STACK_CAP];  /* stack operandi (4096 slot) */
    int sp;
    /* locals (usati quando frame==NULL — GetLocals) */
    double     locals_nums[GMB_LOCALS_CAP];
    gmb_obj_t* locals_refs[GMB_LOCALS_CAP];
    uint8_t    locals_types[GMB_LOCALS_CAP];
    uint8_t    locals_basic[GMB_LOCALS_CAP];


### 2.3 `gmb_scheduler_t` — il kernel

struct gmb_scheduler {
    gmb_task_t** queue;      /* coda FIFO condivisa (BlockingCollection) */
    int queue_cap, queue_head, queue_tail, queue_count;
    pthread_mutex_t qlock;
    pthread_cond_t  qcond;
    int shutdown;            /* CompleteAdding */
    gmb_task_t** all_tasks;  /* tutti i task mai creati (per sleep-watcher) */
    int all_cap, all_count;
    pthread_mutex_t all_lock;
    int next_id;
};


### 2.4 `gmb_channel_t` — canale

struct gmb_channel {
    gmb_obj_t header;         /* kind=GMB_OBJ_CHANNEL */
    pthread_mutex_t lock;
    gmb_stack_entry_t* buffer;   /* buffer circolare di entry complete (num+ref) */
    int buffer_cap, buffer_head, buffer_tail, buffer_count;
    int capacity;                /* default 10 */
    gmb_task_t** recv_tasks;     /* receiver parcheggiati */
    int recv_cap, recv_head, recv_tail, recv_count;
    gmb_task_t** send_tasks;     /* sender parcheggiati */
    int send_cap, send_head, send_tail, send_count;
};


### 2.5 `gmb_waitgroup_t` e `gmb_future_t`

struct gmb_waitgroup {
    gmb_obj_t header;
    pthread_mutex_t lock;
    int counter;
    gmb_task_t** waiters;      /* coda di waiter (più di uno!) */
    int wcap, whead, wtail, wcount;
};

struct gmb_future {
    gmb_obj_t header;
    pthread_mutex_t lock;
    int is_resolved;
    gmb_obj_t* result;      /* valore boxed (stringa) o NULL */
    double result_num;      /* valore numerico */
    int result_is_num;
    gmb_task_t** waiters;


## 3. Scheduler (gmb_gpm.c)

### 3.1 `gmb_sched_enqueue` — Schedule/Wake/Yield (un'unica funzione)

void gmb_sched_enqueue(gmb_scheduler_t* s, gmb_task_t* t)
{
    pthread_mutex_lock(&t->lock);
    if (t->state == GMB_TS_FINISHED) { pthread_mutex_unlock(&t->lock); return; }
    if (t->executing) { t->wake_pending = 1; pthread_mutex_unlock(&t->lock); return; }
    if (t->in_queue)  { pthread_mutex_unlock(&t->lock); return; }
    t->in_queue = 1;
    pthread_mutex_unlock(&t->lock);
    t->state = GMB_TS_RUNNABLE;
    pthread_mutex_lock(&s->qlock);
    if (!s->shutdown) queue_push(s, t);
    pthread_cond_signal(&s->qcond);
    pthread_mutex_unlock(&s->qlock);
}


### 3.2 `gmb_sched_find_runnable` — blocco nativo del worker

gmb_task_t* gmb_sched_find_runnable(gmb_scheduler_t* s)
{
    pthread_mutex_lock(&s->qlock);
    while (s->queue_count == 0 && !s->shutdown)
        pthread_cond_wait(&s->qcond, &s->qlock);
    if (s->queue_count == 0) { pthread_mutex_unlock(&s->qlock); return NULL; }
    gmb_task_t* t = queue_pop(s);
    pthread_mutex_unlock(&s->qlock);
    if (t) {
        pthread_mutex_lock(&t->lock);
        t->in_queue = 0;
        t->executing = 1;
        pthread_mutex_unlock(&t->lock);
    }
    return t;
}


### 3.3 `gmb_sched_release_executing`

void gmb_sched_release_executing(gmb_scheduler_t* s, gmb_task_t* t)
{
    int rewake;
    pthread_mutex_lock(&t->lock);
    t->executing = 0;
    rewake = t->wake_pending;
    t->wake_pending = 0;
    pthread_mutex_unlock(&t->lock);
    if (rewake) gmb_sched_enqueue(s, t);
}


### 3.4 Sleep cooperativo

void gmb_sched_wake_expired_sleepers(gmb_scheduler_t* s, int64_t now_ms)
{
    /* snapshot di all_tasks (non-FINISHED), poi per ognuno:
       se BLOCKED && sleep_until_ms > 0 && sleep_until_ms <= now_ms
       → azzera sleep_until_ms e gmb_sched_enqueue */
}

int64_t gmb_sched_earliest_sleep(gmb_scheduler_t* s)
{
    /* min sleep_until_ms tra i task BLOCKED, o -1 se nessuno */
}


## 4. Il tick: `gmb_vm_tick` (gmb_vm_core.c)

### 4.1 Contratto di ritorno
| Ritorno | Significato |
|---|---|
| `1` | Tick finito con YIELD (`OP_YIELD`) o Gosched (`force_yield`) → worker riaccoda |
| `0` | Task finito (`FINISHED`) o **bloccato** (parcheggiato su una struttura d'attesa) |
| `-1` | Errore (`vm_error`, `pending_exception` impostata) |

### 4.2 Il loop

int gmb_vm_tick(gmb_vm_t* vm, gmb_task_t* task)
{
    while (1)
    {
        if (task->ip < 0 || task->ip >= (int)vm->chunk->code_len)
        { task->state = GMB_TS_FINISHED; return 0; }
        int op = vm->chunk->code[task->ip++];
        switch (op)
        {
            /* ... opcode che possono bloccarsi: */
            /*   return 0;  → parcheggiato (ip già riavvolto, operandi già ripushati) */
            /*   return 1;  → yield/gosched */
            case OP_YIELD:
                return 1;
            default:
            { vm_error(task, "opcode non implementato"); return -1; }


### 4.3 Granularità (allineata a Go)

## 5. Punti di blocco e contratto di ripresa

### 5.1 `OP_SEND` (retry-based)

case OP_SEND:
{
    if (task->sp < 2) break;
    gmb_stack_entry_t ce = gmb_task_pop(task);   /* canale in cima */
    gmb_stack_entry_t ve = gmb_task_pop(task);   /* valore */
    if (ce.type == 2 && ce.ref && ce.basic_type == GMB_BT_OBJECT)
    {
        gmb_channel_t* ch = (gmb_channel_t*)ce.ref;
        /* prepara retry PRIMA della visibilità */
        gmb_task_push_ref(task, (gmb_obj_t*)ve.ref, ve.basic_type);
        gmb_task_push_ref(task, (gmb_obj_t*)ce.ref, GMB_BT_OBJECT);
        task->ip--;
        gmb_task_t* woken = NULL;
        int is_num = ve.type == 1;
        if (gmb_channel_send_or_park(ch, (gmb_obj_t*)ve.ref, ve.num, is_num, task, &woken))
        {


### 5.2 `OP_RECV` (retry-based)

case OP_RECV:
{
    if (task->sp <= 0) break;
    gmb_stack_entry_t ce = gmb_task_pop(task);
    if (ce.type == 2 && ce.ref && ce.basic_type == GMB_BT_OBJECT)
    {
        gmb_channel_t* ch = (gmb_channel_t*)ce.ref;
        gmb_task_push_ref(task, (gmb_obj_t*)ce.ref, GMB_BT_OBJECT);  /* canale ripushato */
        task->ip--;
        gmb_obj_t* value = NULL; double num = 0; int is_num = 0;
        gmb_task_t* woken = NULL;
        if (gmb_channel_recv_or_park(ch, task, &value, &num, &is_num, &woken))
        {
            task->sp -= 1;
            task->ip++;
            if (woken) gmb_sched_enqueue(vm->scheduler, woken);


### 5.3 `OP_WG_WAIT` (NON riavvolge — riprende dall'istruzione successiva)

case OP_WG_WAIT:
{
    gmb_stack_entry_t wg = task->sp > 0 ? gmb_task_pop(task) : (gmb_stack_entry_t){ 0 };
    if (wg.type == 2 && wg.ref && wg.basic_type == GMB_BT_OBJECT)
    {
        gmb_waitgroup_t* w = (gmb_waitgroup_t*)wg.ref;
        if (gmb_wg_should_wait(w, task)) return 0;   /* parcheggiato, NESSUN rewind */
    }
    break;
}


### 5.4 `OP_SLEEP`

case OP_SLEEP:
{
    if (task->sp <= 0) break;
    double ms = gmb_task_pop_num(task);
    if (ms <= 0) break;          /* §10: SLEEP<=0 prosegue senza bloccare */
    task->sleep_until_ms = gmb_now_ms() + (int64_t)ms;
    task->state = GMB_TS_BLOCKED;
    return 0;
}


### 5.5 `OP_GO_SELECT_SETUP` (completo: RECV + SEND + DEFAULT + park)

case OP_GO_SELECT_SETUP:
{
    int startIp = task->ip - 1;
    int num_cases = gmb_vm_read_u8(vm, task);
    int num_sends = gmb_vm_read_u8(vm, task);
    int def_off = gmb_vm_read_u16(vm, task);
    int case_addrs[16];
    for (int i = 0; i < num_cases && i < 16; i++) case_addrs[i] = gmb_vm_read_u16(vm, task);
    /* Stack: [send_val_0..send_val_{S-1}] [channel_0..channel_{C-1}] */
    int baseSp = task->sp - num_cases - num_sends;
    if (baseSp < 0) { vm_error(task, "GO SELECT: stack invalido"); return -1; }

    /* 1. Scan RECV (indice baseSp + num_sends + j) via gmb_channel_try_recv */
    /* 2. Scan SEND (valore baseSp + j, canale baseSp + num_sends + j) via gmb_channel_try_send */
    /* 3. DEFAULT: se def_off != 0xFFFF → esegui subito (nessun park) */
    /* 4. Park atomico su TUTTI i canali (or-park), retry al risveglio (startIp) */


## 6. Canali: `send_or_park` / `recv_or_park` (check-and-register atomico)

bool gmb_channel_send_or_park(gmb_channel_t* c, gmb_obj_t* value, double num, int is_num,
    gmb_task_t* sender, gmb_task_t** woken_receiver)
{
    pthread_mutex_lock(&c->lock);
    if (c->buffer_count < c->capacity)
    {
        /* buffer con spazio: accoda entry completa */
        gmb_stack_entry_t e = { 0 };
        if (is_num) { e.num = num; e.type = 1; e.basic_type = GMB_BT_NUMBER; }
        else if (value) { e.ref = value; e.type = 2; e.basic_type = GMB_BT_STRING; }
        else { e.type = 2; e.basic_type = GMB_BT_NULL; }
        c->buffer[(c->buffer_head + c->buffer_count) % c->buffer_cap] = e;
        c->buffer_count++;
        *woken_receiver = cq_pop(&c->recv_tasks, ...);
        pthread_mutex_unlock(&c->lock);
        return true;


## 7. WaitGroup e Future

### 7.1 `gmb_wg_done` sveglia TUTTI i waiter

void gmb_wg_done(gmb_waitgroup_t* w, gmb_scheduler_t* s)
{
    gmb_task_t** to_wake = NULL; int n = 0;
    pthread_mutex_lock(&w->lock);
    w->counter--;
    if (w->counter <= 0) {
        to_wake = malloc(...);
        for (int i = 0; i < w->wcount; i++) to_wake[n++] = w->waiters[i];
        w->wcount = 0;
    }
    pthread_mutex_unlock(&w->lock);
    for (int i = 0; i < n; i++) gmb_sched_enqueue(s, to_wake[i]);
    free(to_wake);
}


### 7.2 Future: risoluzione idempotente

void gmb_future_resolve(gmb_future_t* f, gmb_obj_t* value, double num, int is_num, gmb_scheduler_t* s)
{
    gmb_task_t** to_wake = NULL; int n = 0;
    pthread_mutex_lock(&f->lock);
    if (f->is_resolved) { pthread_mutex_unlock(&f->lock); return; }  /* idempotente */
    f->is_resolved = 1; f->result = value; f->result_num = num; f->result_is_num = is_num;
    if (f->wcount > 0) { to_wake = malloc(...); for (...) to_wake[n++] = f->waiters[i]; f->wcount = 0; }
    pthread_mutex_unlock(&f->lock);
    for (int i = 0; i < n; i++) gmb_sched_enqueue(s, to_wake[i]);
    free(to_wake);
}


## 8. Implicit await (§11): la lettura di un future non risolto BLOCCA

/* OP_LOAD_LOCAL — rewind -2: opcode(1) + slot(1) */
case OP_LOAD_LOCAL:
{
    int slot = gmb_vm_read_u8(vm, task);
    gmb_stack_entry_t e = gmb_task_load_local(task, slot);
    if (e.type == 2 && e.ref && e.basic_type == GMB_BT_OBJECT &&
        ((gmb_obj_t*)e.ref)->kind == GMB_OBJ_FUTURE)
    {
        gmb_future_t* fut = (gmb_future_t*)e.ref;
        if (!gmb_future_is_resolved(fut))
        {
            task->ip -= 2;
            if (gmb_future_register_waiter(fut, task)) return 0;   /* blocca */
            task->ip += 2;
        }
        /* risolto: pusha il valore */


## 9. Gosched esplicito: TaskYield() = `force_yield`

case 676: /* YIELD — 0 arg = TaskYield (Gosched), 6-7 arg = bond yield */
    if (argc == 0) { task->force_yield = 1; return; }
    result = gmb_fin_bond_yield(...);
    break;


/* OP_CALL_BUILTIN — dopo l'esecuzione del builtin: */
if (task->force_yield) return 1;   /* TaskYield: termina il tick subito */


/* worker_loop — HandlePostTick: */
else if (was_yield || task->force_yield) {
    task->force_yield = 0;
    task->state = GMB_TS_RUNNABLE;
    gmb_sched_enqueue(vm->scheduler, task);
}


## 10. Worker loop e run multi-thread

### 10.1 `worker_loop` — il cuore dell'esecuzione

static void* worker_loop(void* arg)
{
    gmb_vm_t* vm = (gmb_vm_t*)arg;
    while (1)
    {
        gmb_task_t* task = gmb_sched_find_runnable(vm->scheduler);
        if (!task) break; /* shutdown */
        task->state = GMB_TS_RUNNING;
        int was_yield = gmb_vm_tick(vm, task);   /* esito come VALORE LOCALE (fix §16.4) */
        /* HandlePostTick */
        if (task->state == GMB_TS_FINISHED)
        {
            if (task->future_to_resolve && task->pending_exception == NULL)
            {
                gmb_stack_entry_t top = { 0 };
                if (task->sp > 0) top = task->stack[task->sp - 1];


### 10.2 `gmb_vm_run_mt` — avvio e shutdown

int gmb_vm_run_mt(gmb_vm_t* vm)
{
    int worker_count = 4;
    const char* env = getenv("GMBASIC_GOMAXPROCS");
    if (env) { int n = atoi(env); if (n > 0) worker_count = n; }

    gmb_task_t* main = gmb_task_create(vm->scheduler, ...);
    main->vm_ctx = vm;
    main->owns_globals = 1;   /* il main possiede i globals; i GO li condividono */
    gmb_task_ensure_globals(main, vm->chunk->global_count);
    gmb_sched_enqueue(vm->scheduler, main);

    pthread_t workers[32];
    pthread_t watcher;
    int nw = worker_count < 32 ? worker_count : 32;
    for (int i = 0; i < nw; i++) pthread_create(&workers[i], NULL, worker_loop, vm);


### 10.3 `sleep_watcher_loop`

static void* sleep_watcher_loop(void* arg)
{
    gmb_vm_t* vm = (gmb_vm_t*)arg;
    while (gmb_sched_has_active(vm->scheduler))
    {
        gmb_sched_wake_expired_sleepers(vm->scheduler, gmb_now_ms());
        int64_t next = gmb_sched_earliest_sleep(vm->scheduler);
        int wait_ms = next < 0 ? 50 : (next - gmb_now_ms() > 250 ? 250 : (int)(next - gmb_now_ms()));
        if (wait_ms < 1) wait_ms = 1;
        nanosleep(&(struct timespec){ wait_ms / 1000, (wait_ms % 1000) * 1000000 }, NULL);
    }
    return NULL;
}


## 11. Lancio goroutine: `OP_GO` e `OP_GO_FUNCTION`

### 11.1 Convenzione di passaggio argomenti

### 11.2 `OP_GO` (fire-and-forget)

case OP_GO:
{
    int addr = gmb_vm_read_u16(vm, task);
    if (vm->scheduler) LaunchGoTask(vm->scheduler, task, addr, NULL);
    break;
}


### 11.3 `OP_GO_FUNCTION` (con future)

case OP_GO_FUNCTION:
{
    int addr = gmb_vm_read_u16(vm, task);
    if (vm->scheduler)
    {
        gmb_task_t* go = gmb_task_create(vm->scheduler, addr);
        go->vm_ctx = vm;
        /* condividi globals */
        ...
        gmb_future_t* fut = gmb_future_create();
        go->future_to_resolve = fut;
        gmb_task_push_ref(task, (gmb_obj_t*)fut, GMB_BT_OBJECT);  /* DEVE pushare UN valore */
        gmb_sched_enqueue(vm->scheduler, go);
    }
    else push_nil(task);
    break;


## 12. Print bufferizzato per-riga

## 13. Esempio completo minimale (uso della GPM)

SUB Produttore(C AS CHANNEL OF INTEGER, WG AS WAITGROUP)
  DIM I AS INTEGER
  FOR I = 1 TO 5
    SEND I TO C
  NEXT I
  DONE WG
END SUB

SUB Main()
  DIM C AS CHANNEL OF INTEGER
  DIM WG AS WAITGROUP
  GO SUB Produttore(C, WG) IN WG

  DIM V AS INTEGER
  DIM I AS INTEGER
  FOR I = 1 TO 5


## 14. Compilare ed eseguire

# Build della VM/C (libreria statica + host CLI)
cmake -S GMBasic/native/gmb_vm_c -B GMBasic/native/gmb_vm_c/build_debug
cmake --build GMBasic/native/gmb_vm_c/build_debug

# Eseguire un chunk A1 (prodotto dal serializzatore C#)
./native/gmb_vm_c/build_debug/gmb_vm_host file.chunk --mt

# Determinismo (single-thread)
GMBASIC_GOMAXPROCS=1 ./native/gmb_vm_c/build_debug/gmb_vm_host file.chunk --mt


## 15. Verifica e gate

## 16. Errori da NON ripetere (sintesi)

### VM-C-Opcode-Spec

*Fonte: `VM-C-Opcode-Spec`*


## Stato di copertura (riassunto)
| Componente | Definiti | Implementati | Mancanti | Comportamento mancanti |
|---|---|---|---|---|
| Opcode | 132 | **110** | 22 | Errore esplicito `"opcode N non implementato"` |
| Syscall | 328 | **2** | 326 | **No-op SILENZIOSI** (peggio di errore) |
| Builtin | 688 | 364 | ~324 | Errore esplicito / no-op |

## 1. Opcode

### 1.1 Implementati (110)

### Stack Operations
| Opcode | Scopo |
|---|---|
| `OP_PUSH_CONST` | Definisce il set di istruzioni (OpCode) per la Virtual Machine BASIC. |
| `OP_PUSH_NIL` | Spinge sulla stack il valore speciale NIL (non inizializzato). |
| `OP_PUSH_TRUE` | Spinge sulla stack il valore booleano TRUE. |
| `OP_PUSH_FALSE` | Spinge sulla stack il valore booleano FALSE. |
| `OP_POP` | Rimuove e scarta il valore in cima alla stack. |
| `OP_DUP` | Duplica il valore in cima alla stack. |

### Arithmetic & Logical Operations
| Opcode | Scopo |
|---|---|
| `OP_ADD` | Preleva due valori dalla stack, li somma e spinge il risultato. |
| `OP_SUBTRACT` | Preleva due valori dalla stack, li sottrae e spinge il risultato. |
| `OP_MULTIPLY` | Preleva due valori dalla stack, li moltiplica e spinge il risultato. |
| `OP_DIVIDE` | Preleva due valori dalla stack, li divide e spinge il risultato. |
| `OP_INT_DIVIDE` | Preleva due valori dalla stack, esegue la divisione intera e spinge il risultato. |
| `OP_POWER` | Preleva due valori, eleva il primo alla potenza del secondo e spinge il risultato. |
| `OP_NEGATE` | Preleva un valore dalla stack, ne inverte il segno e spinge il risultato. |
| `OP_NOT` | Preleva un valore dalla stack, ne esegue la negazione logica e spinge il risultato. |
| `OP_MOD` | Preleva due valori dalla stack, calcola il resto della divisione e spinge il risultato. |
| `OP_AND` | Esegue l'operazione logica/bitwise AND su due valori. |
| `OP_OR` | Esegue l'operazione logica/bitwise OR su due valori. |

### Comparison Operations
| Opcode | Scopo |
|---|---|
| `OP_CMP_EQ` | Preleva due valori dalla stack, li confronta per uguaglianza e spinge il risultato booleano. |
| `OP_CMP_NE` | Preleva due valori dalla stack, li confronta per disuguaglianza e spinge il risultato booleano. |
| `OP_CMP_GT` | Preleva due valori dalla stack, confronta se il primo è maggiore del secondo e spinge il risultato booleano. |
| `OP_CMP_GTE` | Preleva due valori dalla stack, confronta se il primo è maggiore o uguale al secondo e spinge il risultato booleano. |
| `OP_CMP_LT` | Preleva due valori dalla stack, confronta se il primo è minore del secondo e spinge il risultato booleano. |
| `OP_CMP_LTE` | Preleva due valori dalla stack, confronta se il primo è minore o uguale al secondo e spinge il risultato booleano. |

### Variable & Data Structure Operations
| Opcode | Scopo |
|---|---|
| `OP_DEFINE_GLOBAL` | Definisce una nuova variabile globale. L'operando è l'indice della variabile. Preleva il valore iniziale dalla stack. |
| `OP_LOAD_GLOBAL` | Legge il valore di una variabile globale e lo spinge sulla stack. L'operando è l'indice della variabile. |
| `OP_STORE_GLOBAL` | Preleva un valore dalla stack e lo assegna a una variabile globale. L'operando è l'indice della variabile. |
| `OP_LOAD_LOCAL` | Legge il valore di una variabile locale dallo stack frame corrente e lo spinge sulla stack. L'operando è l'indice (slot) della variabile. |
| `OP_STORE_LOCAL` | Preleva un valore dalla stack e lo assegna a una variabile locale. L'operando è l'indice (slot) della variabile. |
| `OP_ADJUST_STRING` |  |
| `OP_MAKE_BYREF` |  |
| `OP_MAKE_BYREF_GLOBAL` |  |
| `OP_CALL_INDIRECT` |  |
| `OP_CALL_N` |  |
| `OP_PUSH_REF_LOCAL` | Trova una variabile locale, crea un oggetto riferimento ad essa e lo spinge sulla stack. Usato per passare argomenti BYREF. |
| `OP_PUSH_REF_GLOBAL` | Trova una variabile globale, crea un oggetto riferimento ad essa e lo spinge sulla stack. Usato per passare argomenti BYREF. |
| `OP_LOAD_ELEMENT` | Preleva un array/oggetto e N indici dalla stack, legge l'elemento e spinge il suo valore sulla stack. L'operando (byte) è il numero di indici N. |
| `OP_ARRAY_LEN` | Restituisce la lunghezza di un array in cima allo stack. |
| `OP_STORE_ELEMENT` | Preleva un valore, un array/oggetto e N indici dalla stack, e salva il valore nell'elemento specificato. L'operando (byte) è il numero di indici N. |
| `OP_LOAD_FIELD` | Preleva una struttura (UDT) dalla stack, legge un campo e spinge il suo valore sulla stack. L'operando (short) è l'indice del nome del campo nella constant pool. |
| `OP_STORE_FIELD` | Preleva un valore e una struttura (UDT) dalla stack, e salva il valore nel campo specificato. L'operando (short) è l'indice del nome del campo. |
| `OP_REDIM` | Ridimensiona un array. Operandi: isLocal (byte), varIndex (short), #dimensioni (byte). Le nuove dimensioni sono sulla stack. |
| `OP_REDIM_PRESERVE` | Ridimensiona un array conservando i dati esistenti. La sua struttura di operandi è identica a OP_REDIM. |
| `OP_MK_ARRAY` | Crea un nuovo array. Preleva N valori dalla stack (dove N è l'operando short) e li usa per popolare un nuovo array. |
| `OP_INIT_ARRAY` | Inizializza un array già esistente con N valori. |
| `OP_MK_DICT` | Crea un nuovo dizionario. Preleva N coppie chiave-valore dalla stack (dove N è l'operando short) e le usa per popolare il dizionario. |
| `OP_MK_LIST` | Crea una nuova lista. Preleva N valori dalla stack (dove N è l'operando short) e li usa per popolare la lista. |

### Control Flow & Procedures
| Opcode | Scopo |
|---|---|
| `OP_JUMP_FORWARD` | Esegue un salto incondizionato in avanti. |
| `OP_JUMP_BACKWARD` | Esegue un salto incondizionato all'indietro (per i loop). |
| `OP_JUMP_IF_TRUE` | Preleva un valore dalla stack. Se è vero, esegue un salto in avanti. |
| `OP_JUMP_IF_FALSE` | Preleva un valore dalla stack. Se è falso, esegue un salto in avanti. |
| `OP_CALL` | Chiama una procedura/funzione a un indirizzo assoluto. Il numero di argomenti è sulla stack. |
| `OP_CALL_CTOR` | Chiamata a costruttore (BASE.NEW). Stack: [arg1, ..., argN, SELF, num_args]. |
| `OP_CALL_BUILTIN` | Chiama una funzione predefinita del runtime. |
| `OP_GOTO` | Esegue un salto incondizionato a un indirizzo assoluto. |
| `OP_GOSUB` | Esegue un GOSUB a un indirizzo assoluto, pushando l'indirizzo di ritorno. |
| `OP_CLEAR_RETURN` | Resetta il ReturnValue del frame corrente a null. |
| `OP_RETURN` | Termina l'esecuzione di una funzione/sub e ritorna al chiamante. |
| `OP_HALT` | Interrompe immediatamente l'esecuzione del programma. |
| `OP_ENTER_LOOP` | Inizia un nuovo contesto di loop. |
| `OP_EXIT_LOOP` | Termina il contesto di loop corrente. |

### Concurrency / GoRoutines & Generators
| Opcode | Scopo |
|---|---|
| `OP_GO` | Crea una nuova GoRoutine e la prepara per l'esecuzione. |
| `OP_SEND` | Invia un valore a un canale. Il valore e il canale sono sulla stack. |
| `OP_RECV` | Riceve un valore da un canale. Il canale è sulla stack. |
| `OP_WG_WAIT` | Attende che un WaitGroup arrivi a zero. Il WaitGroup è sulla stack. |
| `OP_WG_DONE` | Decrementa il contatore di un WaitGroup. Il WaitGroup è sulla stack. |
| `OP_SLEEP` | Interrompe l'esecuzione della GoRoutine corrente per un numero di millisecondi. Il valore è sulla stack. |
| `OP_GO_SELECT_SETUP` | Esegue il setup per un'istruzione GO SELECT. |

### Error Handling & Events
| Opcode | Scopo |
|---|---|
| `OP_THROW` | Lancia un'eccezione con un codice e un messaggio (sulla stack). |
| `OP_SET_ERR_HANDLER_GOTO` | Imposta l'indirizzo del gestore di errori `ON ERROR GOTO`. |
| `OP_SET_ERR_HANDLER_GOSUB` | Imposta l'indirizzo del gestore di errori `ON ERROR GOSUB`. |
| `OP_RESUME` | Riprende l'esecuzione dopo un errore gestito. |
| `OP_PUSH_TRY` | Installa un gestore di eccezioni `TRY...CATCH`. Spinge un frame di eccezione sulla call stack. |
| `OP_POP_TRY` | Rimuove il gestore di eccezioni `TRY...CATCH` corrente dalla call stack. |
| `OP_ENTER_FINALLY` | Segnala l'inizio di un blocco FINALLY. |
| `OP_EXIT_FINALLY` | Segnala la fine di un blocco FINALLY e propaga eventuali eccezioni. |

### Opcodes a Prefisso (Polimorfici)
| Opcode | Scopo |
|---|---|
| `OP_SYSCALL` | Esegue una chiamata a una funzione di sistema atomica. |
| `OP_FILE_CMD` | Esegue un comando complesso di I/O su file. |
| `OP_NET_CMD` | Esegue un comando di rete complesso. |
| `OP_CLIP_CMD` | Esegue un comando complesso per gli appunti. |
| `OP_MATRIX_CMD` | Esegue un comando complesso per le matrici di trasformazione. |
| `OP_VECTOR_CMD` | Esegue un comando vettoriale complesso in batch. |
| `OP_NEW` | Istanzia un nuovo oggetto di un tipo specifico (es. HTTPOPTIONS). |
| `OP_NEW_UDT_INSTANCE` | Alloca e inizializza una nuova istanza di un UDT. L'operando (ushort) è un indice al descrittore del tipo. |
| `OP_FETCH` | Esegue la funzione speciale FETCH. L'operando (byte) indica la variante (sincrona/asincrona). |

### Opcodes per Classi & Oggetti
| Opcode | Scopo |
|---|---|
| `OP_PUSH_CODE_REF` | Spinge sulla stack un riferimento a un blocco di codice P-Code compilato. |
| `OP_NEW_CLASS_INSTANCE` | Alloca una nuova istanza di una classe e la spinge sulla stack. |
| `OP_LOAD_MEMBER` | Carica il valore di un campo/proprietà da un'istanza (SELF) sulla stack. |
| `OP_STORE_MEMBER` | Preleva un valore dalla stack e lo assegna a un campo/proprietà di un'istanza (SELF). |
| `OP_CALL_MEMBER` | Chiama un metodo o operatore su un'istanza (SELF). L'operando (short) è un indice nella constant pool al nome del metodo. |
| `OP_FINALIZE_SCOPE` | Inizia un blocco di finalizzazione dello scope. Usato per i distruttori. |
| `OP_FINALIZE_GLOBAL` | Esegue il distruttore (SUB DESTRUCTOR) di una variabile globale, dato l'indice |
| `OP_ON_JUMP` | Esegue un salto calcolato basato su un indice. Usato per ON...GOTO/GOSUB. |

### Opcodes per Procedure Locali e Closure
| Opcode | Scopo |
|---|---|
| `OP_CLOSURE` | Crea una closure per una funzione locale. L'operando (ushort) è un indice |
| `OP_LOAD_UPVALUE` | Carica il valore di una variabile catturata (upvalue) dalla closure corrente. |
| `OP_STORE_UPVALUE` | Assegna un valore a una variabile catturata (upvalue) nella closure corrente. |

### Opcodes per Cicli Ottimizzati
| Opcode | Scopo |
|---|---|
| `OP_FOR_PREPARE` | Prepara un ciclo FOR. Preleva start, end, e step dalla stack e li memorizza |
| `OP_FOR_LOOP` | Esegue un'iterazione di un ciclo FOR. Incrementa il contatore, lo confronta |

### Opcodes per PRINT Ottimizzato
| Opcode | Scopo |
|---|---|
| `OP_PRINT_VALUE` | Preleva un valore dalla stack e lo stampa sulla console/schermo attivo |
| `OP_PRINT_LINE` | Preleva un valore dalla stack, lo stampa, e aggiunge un newline. |
| `OP_PRINT_TAB_ZONE` | Sposta il cursore alla successiva "print zone" (tabulazione). |
| `OP_PRINT_NEWLINE` | Stampa un carattere di newline. Corrisponde a `PRINT` senza argomenti. |

### Opcodes per Cooperativo Scheduling
| Opcode | Scopo |
|---|---|
| `OP_YIELD` | Yield point volontario. Il task cede il controllo allo scheduler. |

### Opcodes per Concorrenza (WaitGroup)
| Opcode | Scopo |
|---|---|
| `OP_NEW_WAITGROUP` | Crea un nuovo WaitGroupInfo (contatore) e lo spinge sulla stack. |
| `OP_NEW_CHANNEL` | Crea un nuovo canale (ChannelInfo) e lo spinge sulla stack. |
| `OP_CLONE_UDT` | Copia per valore un'istanza UDT sulla cima dello stack: sostituisce il dizionario con |
| `OP_LOAD_STATIC_FIELD` | Legge il valore corrente di un campo STATIC di CLASS (condiviso da tutte le istanze, |
| `OP_STORE_STATIC_FIELD` | Scrive il valore in cima alla stack in un campo STATIC di CLASS (condiviso da tutte le |
| `OP_GO_FUNCTION` | Lancia una <c>GO FUNCTION</c> (a differenza di OP_GO/GO SUB, il chiamante vuole il |
| `OP_LOAD_ELEMENT_POSITIONAL` | Variante di OP_LOAD_ELEMENT usata ESCLUSIVAMENTE da FOR EACH per avanzare su un |
| `OP_IS_ARRAY` | Pusha 1.0 se il valore in cima allo stack è un Array (senza consumarlo), altrimenti 0.0. |

### 1.2 Non implementati (22) — errore esplicito
| Opcode | Scopo (dal compilatore) | Dominio |
|---|---|---|
| `OP_AUDIO_CMD` | Esegue un comando audio complesso. | — |
| `OP_BREAKPOINT` |  | — |
| `OP_CALL_FFI` | Chiama una funzione esterna da una libreria dinamica (FFI). | — |
| `OP_END_VALID` | Termina una subroutine di predicato VALID (generata dal compilatore per | — |
| `OP_EVAL_MACRO_EXPR` | Valuta una macro usata in un'espressione. Preleva una stringa dalla stack, | — |
| `OP_EVAL_MACRO_STMT` | Valuta una macro usata come istruzione. Preleva una stringa dalla stack, | — |
| `OP_FE_DESTRUCT` | Destruttura il valore di iterazione di un FOR EACH su MATRICE in N elementi. | — |
| `OP_FOR_EACH_ADVANCE` | Avanza di un elemento in un ciclo FOR EACH. Incrementa il contatore in | — |
| `OP_IS_TYPE` |  | — |
| `OP_LIST_ADD` | Aggiunge un elemento a una lista. Preleva il valore e poi la lista dalla stack. Lascia la lista sulla stack. | — |
| `OP_NEW_DICT` | Crea un nuovo dizionario vuoto. | — |
| `OP_NEW_GENERATOR_CALL` | Chiama un GENERATOR PROCEDURE (<c>GENERATOR Nome(...) AS T ... END GENERATOR</c>) SENZA | — |
| `OP_NEW_GENERATOR_EXPR` | Costruisce un oggetto generatore lazy da un'espressione generatore | — |
| `OP_NEXT_GENERATOR` | Avanza un'istanza di generatore e pusha il valore. | — |
| `OP_ON_KEY` | Registra un gestore di eventi per un tasto. Richiede 3 valori sulla stack: KeyCode, ActionType, TargetAddress. | — |
| `OP_ON_MOUSE` |  | — |
| `OP_READ` | Legge il prossimo valore dalla tabella DATA e lo lascia sulla stack. | — |
| `OP_RESOLVE_FUTURE` | Attende il completamento di un FutureValue e pusha il risultato. | — |
| `OP_RESTORE` | Resetta il "data pointer" della VM all'inizio della tabella DATA. | — |
| `OP_SPRITE_CMD` | Esegue un comando complesso per gli sprite. | — |
| `OP_STORE_RETURN` |  | — |
| `OP_XOR` | Esegue l'operazione logica/bitwise XOR su due valori. | — |

## 2. Syscall

### 2.1 Implementate (2 di 328)
| Syscall | Codice | Scopo |
|---|---|---|
| `KEY_GEN` | 250 | Genera una coppia di chiavi asimmetriche (ECDSA/RSA via OpenSSL) → push [pub, priv] |
| `KEY_CLOSE` | 265 | Rilascia un handle di chiave |

### 2.2 NON implementate (326) — ⚠️ NO-OP SILENZIOSI

## 3. Builtin (funzioni)

## 4. Esecuzione
| Percorso | Dove | Stato |
|---|---|---|
| REPL `RUN` | `ReplEngine` → `VmCBridge.Run` (in-process) | ✅ |
| IDE Run/Immediate | `GMBasicIde` → `VmCBridge`/`DebugServer` | ✅ (debug per riga: breakpoint/step) |
| `vmtest` | `tools/runners/GMBASIC.Conformance` | ✅ in-process |
| Host CLI standalone | `GMBasic/native/gmb_vm_c/build/gmb_vm_host` | ✅ (strumento autonomo, non più spawnato da REPL) |
| C99 transpiler | `GMBasic/native/gmb_c99native/src` → `cc` + `gmb_runtime_c` | ✅ (D4 Commercial) |

### Debug di riga (2026-08-17)

## 5. Gap prioritari (per il lavoro futuro)

## Relazioni

<!-- END SURFACE -->


<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **BRAINS.ADJACENCY(x1)** — `PRINT BRAINS.ADJACENCY("graph-vault")` (shared/minitests/graph/adjacency.bas:13)
- **BRAINS.BYFOLDER(x1, x2)** — `r = BRAINS.BYFOLDER("graph-vault", "strategies")` (shared/minitests/graph/graph_all.bas:42)
- **BRAINS.BYTAG(x1, x2)** — `r = BRAINS.BYTAG("graph-vault", "strategy")` (shared/minitests/graph/graph_all.bas:40)
- **BRAINS.DOT(x1)** — `r = BRAINS.DOT("graph-vault")` (shared/minitests/graph/graph_all.bas:38)
- **BRAINS.ISVALID(x1)** — `r = BRAINS.ISVALID("graph-vault")` (shared/minitests/graph/graph_all.bas:44)
- **BRAINS.NEIGHBORS(x1, x2)** — `r = BRAINS.NEIGHBORS("graph-vault", "index.md")` (shared/minitests/graph/graph_all.bas:32)
- **BRAINS.PATH(x1, x2, x3)** — `r = BRAINS.PATH("graph-vault", "index.md", "trading/momentum.md")` (shared/minitests/graph/graph_all.bas:34)
- **BRAINS.QUERYMULTI(x1)** — `DIM g1$ = BRAINS.QUERYMULTI("BlackScholes", "finance")` (shared/samples/graph/FinanceBrainGraph.bas:33)
- **BRAINS.STATS(x1)** — `r = BRAINS.STATS("graph-vault")` (shared/minitests/graph/graph_all.bas:36)
- **ENV.ENV(x1)** — nessun uso nel corpus
- **ENV.HAS(x1)** — `PRINT names(i); " present="; ENV.HAS(names(i))` (shared/samples/System/EnvDemo.bas:20)
- **FONTS.TTGLYPHS(x1)** — `g = FONTS.TTGLYPHS("/tmp/gmb_fonts_non_esiste.ttf")` (shared/minitests/fonts/tt_functions.bas:18)
- **FONTS.TTMETRICS(x1, x2, x3)** — `m = FONTS.TTMETRICS("/tmp/gmb_fonts_non_esiste.ttf", 16, 65)` (shared/minitests/fonts/tt_functions.bas:10)
- **FONTS.TTRENDER(x1, x2, x3)** — `b = FONTS.TTRENDER("/tmp/gmb_fonts_non_esiste.ttf", 16, 65)` (shared/minitests/fonts/tt_functions.bas:14)
- **LINALG.CHOLESKY(x1)** — `DIM ch = LINALG.CHOLESKY(a)` (shared/samples/Matrix/MatrixDecompositions.bas:30)
- **LINALG.COND(x1)** — `PRINT "cond     ="; LINALG.COND(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:37)
- **LINALG.COSM(x1)** — `c = LINALG.COSM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:11)
- **LINALG.DETERMINANT(x1)** — `PRINT "det(expm(I)) ="; LINALG.DETERMINANT(e)` (shared/samples/Math/MatrixFuncSciPyParity.bas:9)
- **LINALG.EIG(x1)** — `ed = LINALG.EIG(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:62)
- **LINALG.EVD(x1)** — `DIM evd = LINALG.EVD(a)` (shared/samples/Matrix/MatrixDecompositions.bas:36)
- **LINALG.EXPM(x1)** — `e = LINALG.EXPM(h)` (shared/samples/Math/MatrixFuncSciPyParity.bas:8)
- **LINALG.FACTOR(x1, x2)** — `pu = LINALG.FACTOR(pol, "U")` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:33)
- **LINALG.FRACTIONALPOWER(x1, x2)** — `fp = LINALG.FRACTIONALPOWER(h, 0.5)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:19)
- **LINALG.FROBENIUSNORM(x1)** — `PRINT "FROBENIUS="; LINALG.FROBENIUSNORM(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:25)
- **LINALG.HESSENBERG(x1)** — `hh = LINALG.HESSENBERG(h3)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:23)
- **LINALG.INFINITYNORM(x1)** — `PRINT "INFINITYNORM="; LINALG.INFINITYNORM(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:27)
- **LINALG.INVERSE(x1)** — `DIM inv = LINALG.INVERSE(a)` (shared/samples/Matrix/MatrixDecompositions.bas:20)
- **LINALG.LOGM(x1)** — `l = LINALG.LOGM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:14)
- **LINALG.LU(x1)** — `DIM lu = LINALG.LU(a)` (shared/samples/Matrix/MatrixDecompositions.bas:39)
- **LINALG.NORM(x1)** — `PRINT "norm="; MATH.ROUND(LINALG.NORM(a), 6)` (shared/samples/Matrix/MatrixDecompositions.bas:17)
- **LINALG.NUCLEARNORM(x1)** — `PRINT "nuc      ="; LINALG.NUCLEARNORM(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:38)
- **LINALG.NULLSPACE(x1)** — `DIM ns = LINALG.NULLSPACE(sg)` (shared/samples/Matrix/MatrixDecompositions.bas:89)
- **LINALG.ONENORM(x1)** — `PRINT "ONENORM="; LINALG.ONENORM(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:26)
- **LINALG.ORTH(x1)** — `o = LINALG.ORTH(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:27)
- **LINALG.PINVERSE(x1)** — `p = LINALG.PINVERSE(h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:32)
- **LINALG.POLAR(x1)** — `pol = LINALG.POLAR(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:32)
- **LINALG.QR(x1)** — `DIM qr = LINALG.QR(a)` (shared/samples/Matrix/MatrixDecompositions.bas:42)
- **LINALG.RANK(x1)** — `PRINT "rank="; LINALG.RANK(a)` (shared/samples/Matrix/MatrixDecompositions.bas:16)
- **LINALG.SCHUR(x1)** — `sd = LINALG.SCHUR(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:69)
- **LINALG.SIGNM(x1)** — `sg = LINALG.SIGNM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:17)
- **LINALG.SINM(x1)** — `s = LINALG.SINM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:13)
- **LINALG.SOLVE(x1, x2)** — `DIM xs() = LINALG.SOLVE(a, b)` (shared/samples/Matrix/MatrixDecompositions.bas:97)
- **LINALG.SOLVETRIANGULAR(x1, x2, x3)** — `DIM vb() = LINALG.SOLVETRIANGULAR(L, b, 1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:58)
- **LINALG.SQRTM(x1)** — `s = LINALG.SQRTM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:10)
- **LINALG.SUBSPACEANGLES(x1, x2)** — `DIM ang() = LINALG.SUBSPACEANGLES(h, g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:29)
- **LINALG.SVD(x1)** — `DIM svd = LINALG.SVD(a)` (shared/samples/Matrix/MatrixDecompositions.bas:33)
- **LINALG.TANM(x1)** — `tn = LINALG.TANM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:15)
- **NUMERICAL.CUBICSPLINE(x1, x2, x3)** — `PRINT xn; "  "; NUMERICAL.InterpolateLinear(x, y, xn); "  "; NUMERICAL.CubicSpline(x, y, xn); "  "; …` (shared/samples/Analysis/AnalysisInterpReal.bas:13)
- **NUMERICAL.CUMTRAPEZOID(x1, x2)** — `DIM ct() = NUMERICAL.CumTrapezoid(yy, xx)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:14)
- **NUMERICAL.DIFFERENTIATE(x1, x2)** — `DIM Derivata = NUMERICAL.Differentiate(FN(x) => x*x, 2)` (shared/samples/Analysis/AnalysisTests.bas:132)
- **NUMERICAL.FFT(x1)** — `DIM spettro = NUMERICAL.Fft(segnale)` (shared/samples/Analysis/ControlloQualita.bas:60)
- **NUMERICAL.FFTCOMPLEX(x1, x2)** — `DIM f() = NUMERICAL.FftComplex(x, imz)` (shared/minitests/analysis/fftc.bas:4)
- **NUMERICAL.FFTFREQ(x1, x2)** — `DIM f6() = NUMERICAL.FftFreq(6, 1)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:22)
- **NUMERICAL.FFTSHIFT(x1)** — `DIM fs() = NUMERICAL.FftShift(a6)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:25)
- **NUMERICAL.IFFT(x1)** — `DIM ricostruito = NUMERICAL.Ifft(spettro)` (shared/samples/Analysis/ControlloQualita.bas:81)
- **NUMERICAL.IFFTCOMPLEX(x1, x2)** — `DIM r() = NUMERICAL.IfftComplex(x, imz)` (shared/minitests/analysis/fftc.bas:6)
- **NUMERICAL.IFFTSHIFT(x1)** — `DIM ifs() = NUMERICAL.IfftShift(a6)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:27)
- **NUMERICAL.INTEGRATE(x1, x2, x3)** — `DIM Integrale = NUMERICAL.Integrate(FN(x) => x*x, 0, 1)` (shared/samples/Analysis/AnalysisTests.bas:128)
- **NUMERICAL.INTERPOLATECLAMP(x1, x2, x3)** — `DIM y_clamp = NUMERICAL.InterpolateClamp(DatiX, DatiY, x_new)` (shared/samples/Analysis/AnalysisTests.bas:118)
- **NUMERICAL.INTERPOLATEGENERIC(x1, x2, x3, x4)** — `DIM y_gen = NUMERICAL.InterpolateGeneric("linear", DatiX, DatiY, x_new)` (shared/samples/Analysis/AnalysisTests.bas:119)
- **NUMERICAL.INTERPOLATELINEAR(x1, x2, x3)** — `PRINT xn; "  "; NUMERICAL.InterpolateLinear(x, y, xn); "  "; NUMERICAL.CubicSpline(x, y, xn); "  "; …` (shared/samples/Analysis/AnalysisInterpReal.bas:13)
- **NUMERICAL.INTERPOLATEPCHIP(x1, x2, x3)** — `PRINT xn; "  "; NUMERICAL.InterpolateLinear(x, y, xn); "  "; NUMERICAL.CubicSpline(x, y, xn); "  "; …` (shared/samples/Analysis/AnalysisInterpReal.bas:13)
- **NUMERICAL.INTERPOLATEPOLYNOMIAL(x1, x2, x3)** — `PRINT xn; "  "; NUMERICAL.InterpolateLinear(x, y, xn); "  "; NUMERICAL.CubicSpline(x, y, xn); "  "; …` (shared/samples/Analysis/AnalysisInterpReal.bas:13)
- **NUMERICAL.INTERPOLATESPLINE(x1, x2, x3)** — `PRINT "spline@2.5: "; NUMERICAL.InterpolateSpline(x, y, 2.5)` (shared/samples/Analysis/AnalysisInterpReal.bas:15)
- **NUMERICAL.INTERPOLATESTEP(x1, x2, x3)** — `PRINT xn; "  "; NUMERICAL.InterpolateLinear(x, y, xn); "  "; NUMERICAL.CubicSpline(x, y, xn); "  "; …` (shared/samples/Analysis/AnalysisInterpReal.bas:13)
- **NUMERICAL.IRFFT(x1, x2)** — `DIM ir() = NUMERICAL.Irfft(re, im)` (shared/samples/Analysis/AnalysisParityP0.bas:20)
- **NUMERICAL.RFFT(x1)** — `DIM rf = NUMERICAL.Rfft(x)` (shared/samples/Analysis/AnalysisParityP0.bas:9)
- **NUMERICAL.SIMPSON(x1, x2, x3, x4)** — `PRINT "simpson     ="; NUMERICAL.Simpson(FN(x) => x * x, 0, 1, 256)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:12)
- **NUMERICAL.SOLVEIVP(x1, x2, x3, x4)** — `DIM npts = NUMERICAL.SolveIvp({|t, y| RETURN -1}, y0, 0.0, 1.0)` (shared/samples/Analysis/AnalysisParityP0.bas:14)
- **NUMERICAL.SOLVEODE(x1, x2, x3, x4, x5)** — `DIM ys() = NUMERICAL.SolveOde({ |t, y| RETURN -y }, 1.0, 0.0, 1.0, 10)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:17)
- **NUMERICAL.SPLEV(x1, x2)** — `DIM sv() = NUMERICAL.Splev(sp, xq)` (shared/samples/Analysis/AnalysisParityP0.bas:29)
- **NUMERICAL.SPLREP(x1, x2)** — `DIM sp = NUMERICAL.Splrep(xs, ys)` (shared/samples/Analysis/AnalysisParityP0.bas:26)
- **NUMERICAL.TRAPEZOID(x1, x2)** — `PRINT "trapezoid   ="; NUMERICAL.Trapezoid(yy, xx)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:13)
- **REGRESSION.REGRESS(x1, x2)** — `RegRes = REGRESSION.Regress(DatiX, DatiY)` (shared/samples/Analysis/AnalysisTests.bas:47)
- **SYSTEM.CHOOSE(x1)** — `PRINT SYSTEM.CHOOSE(1, 5, 6, 7)` (shared/minitests/system/choose.bas:9)
- **SYSTEM.COMMAND(x1)** — `PRINT "COMMAND(1) len: "; LEN(SYSTEM.COMMAND(1))` (shared/samples/System/SystemFullCoverage.bas:18)
- **SYSTEM.COMMANDCOUNT()** — `IF SYSTEM.COMMANDCOUNT() >= 0 THEN PRINT "system.count ok"` (shared/samples/Commands/DirectCastCommandProbe.bas:28)
- **SYSTEM.CTYPE(x1, x2)** — `PRINT "=== Coercizione SYSTEM.CTYPE ==="` (shared/samples/System/SystemUdtIntrospection.bas:28)
- **SYSTEM.COUNT(x1, x2)** — `IF SYSTEM.COMMANDCOUNT() >= 0 THEN PRINT "system.count ok"` (shared/samples/Commands/DirectCastCommandProbe.bas:28)
- **SYSTEM.EMPTY(x1)** — `PRINT "EMPTY(nonInit):   "; SYSTEM.EMPTY(nonInit)` (shared/samples/System/SystemLowLevel.bas:92)
- **SYSTEM.GOMAXPROCS()** — `SYSTEM.GOMAXPROCS(4)` (shared/samples/Concurrency/PCodeGpmGoMaxProcsResizeStress.bas:30)
- **SYSTEM.IIF(x1, x2, x3)** — `DIM res$ = SYSTEM.IIF(val > 5, "Maggiore (IIF)", "Minore (IIF)")` (shared/samples/System/SystemLowLevel.bas:80)
- **SYSTEM.LBOUND(x1)** — `PRINT "lbound="; SYSTEM.LBOUND(a)` (shared/minitests/system/runtime.bas:6)
- **SYSTEM.NUMCPU()** — `nc = SYSTEM.NUMCPU()` (shared/samples/Concurrency/GpmV2NumGoroutine.bas:15)
- **SYSTEM.NUMGOROUTINE()** — `ng = SYSTEM.NUMGOROUTINE()` (shared/samples/Concurrency/GpmV2NumGoroutine.bas:14)
- **SYSTEM.NEXT(x1)** — `total = total + NEXT(it)` (shared/samples/System/SystemNextGenerator.bas:15)
- **SYSTEM.SHELLCODE(x1)** — `gitCode = SYSTEM.SHELLCODE(gitCheck)` (shared/samples/PackageRegistry/PackageServer.bas:1201)
- **SYSTEM.SHELLOUT(x1)** — `gitOut = SYSTEM.SHELLOUT("git ls-remote --tags " + repoUrl + " | grep -c '" + tagR + "'")` (shared/samples/PackageRegistry/PackageServer.bas:1205)
- **SYSTEM.SIZEOF(x1)** — `PRINT "SizeOf(INTEGER): "; SYSTEM.SIZEOF(i); " (Atteso 2)"` (shared/samples/System/SystemLowLevel.bas:51)
- **SYSTEM.STRPTR(x1)** — `PRINT "StrPtr(s):       "; CONVERSIONS.HEX(SYSTEM.STRPTR(s))` (shared/samples/System/SystemLowLevel.bas:62)
- **SYSTEM.STRTI()** — `PRINT "System Time Str:  "; SYSTEM.STRTI()` (shared/samples/System/SystemLowLevel.bas:111)
- **SYSTEM.SYS(x1)** — `PRINT "OS System:        "; SYSTEM.SYS("OS")` (shared/samples/System/SystemLowLevel.bas:100)
- **SYSTEM.TAB(x1)** — `IF LEN(SYSTEM.TAB(4)) >= 0 THEN PRINT "tab-ok"` (shared/minitests/system/runtime.bas:8)
- **SYSTEM.TI()** — `DIM t1 = SYSTEM.TI()` (shared/samples/System/SystemLowLevel.bas:105)
- **SYSTEM.TYPENAME(x1)** — `PRINT "TYPENAME:   "; SYSTEM.TYPENAME(p)` (shared/samples/System/SystemUdtIntrospection.bas:23)
- **SYSTEM.UBOUND(x1)** — `PRINT "DIM: ", SYSTEM.UBOUND(e) + 1        ' 64` (shared/samples/Ai/AiImageEmbed.bas:17)
- **SYSTEM.UDTALIGNED(x1)** — `PRINT "UDTALIGNED: "; SYSTEM.UDTALIGNED(p)` (shared/samples/System/SystemUdtIntrospection.bas:25)
- **SYSTEM.UDTPACKED(x1)** — `PRINT "UDTPACKED:  "; SYSTEM.UDTPACKED(p)` (shared/samples/System/SystemUdtIntrospection.bas:24)
- **SYSTEM.VALTYPE(x1)** — `PRINT "valtype="; SYSTEM.VALTYPE(Amount)` (shared/samples/Clipper/ClipperBareCurrencyPrint.bas:47)
- **SYSTEM.VARPTR(x1)** — `PRINT "VarPtr(i):       "; CONVERSIONS.HEX(SYSTEM.VARPTR(i))` (shared/samples/System/SystemLowLevel.bas:59)
- **TYPEINFO.HAS(x1)** — `PRINT "HAS(42):    "; TYPEINFO.HAS(42)` (shared/samples/System/TypeInfoIntrospect.bas:9)
- **TYPEINFO.METHODINFO(x1)** — `PRINT "METHODINFO: "; TYPEINFO.METHODINFO("Person")` (shared/samples/System/TypeInfoIntrospect.bas:12)
- **TYPEINFO.PROPERTYINFO(x1)** — `PRINT "PROPERTYINFO: "; TYPEINFO.PROPERTYINFO("Person")` (shared/samples/System/TypeInfoIntrospect.bas:13)
- **TYPEINFO.TYPEINFO(x1)** — `PRINT "TSDECOMPOSE type: "; TYPEINFO(dec)` (shared/samples/TimeSeries/TimeSeriesFullCoverage.bas:39)
<!-- END DOMAIN TABLE -->
