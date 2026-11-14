/* ============================================================================
 * gmb_gpm.h — GPM multi-core (Piano 1, §16): task, scheduler, canali, waitgroup, future
 * File: native/gmb_vm_c/gmb_gpm.h
 * Fedele al modello P-CODE C# §16: N worker thread (pthread), BlockingCollection,
 * check-and-register atomico sotto lo stesso lock, State=Blocked DENTRO il lock,
 * wasYield come valore di ritorno (mai campo condiviso).
 * ============================================================================ */

#ifndef GMB_GPM_H
#define GMB_GPM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include "gmb_vm_value.h"
#include "gmb_chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Stati task (specchio di PCodeTaskState) ---- */
enum {
    GMB_TS_RUNNABLE = 0,
    GMB_TS_RUNNING = 1,
    GMB_TS_BLOCKED = 2,
    GMB_TS_FINISHED = 3,
    GMB_TS_DEBUG_PAUSED = 4,
};

/* ---- Task (PCB) — specchio di PCodeTask ---- */
/* V1 (piano VMC): lo stack operandi è DINAMICO a crescita geometrica;
 * GMB_TASK_STACK_CAP resta solo la capacità INIZIALE (come PCodeVm: stack
 * iniziale 4096, raddoppio fino al cap configurabile GMBASIC_PCODE_MAX_STACK). */
#define GMB_TASK_STACK_CAP 4096
/* Capacità INIZIALE dei locals (cresce on demand: un programma con centinaia di variabili
 * non deve più perdere gli slot oltre la soglia — vedi gmb_task_ensure_locals). */
#define GMB_LOCALS_CAP 256

typedef struct gmb_task gmb_task_t;
typedef struct gmb_scheduler gmb_scheduler_t;
typedef struct gmb_channel gmb_channel_t;
typedef struct gmb_waitgroup gmb_waitgroup_t;
typedef struct gmb_mutex gmb_mutex_t;
typedef struct gmb_rwlock gmb_rwlock_t;
typedef struct gmb_once gmb_once_t;
typedef struct gmb_atomic gmb_atomic_t;
typedef struct gmb_pool gmb_pool_t;
typedef struct gmb_cond gmb_cond_t;
typedef struct gmb_context gmb_context_t;
typedef struct gmb_concurrentmap gmb_concurrentmap_t;
typedef struct gmb_future gmb_future_t;
typedef struct gmb_select_ctx gmb_select_ctx_t;

/* ---- Entry di attesa canale (V6: specchio ChannelReceiverWaiter/ChannelSenderWaiter) ----
 * Un park parcheggia una entry, non un raw task pointer: per GO SELECT la entry porta il
 * SelectContext condiviso e l'indice del case, così il completamento può rivendicare il
 * winner (CAS) e i loser vengono staccati da tutti i canali (criterio di accettazione 8:
 * nessun raw task pointer dangling nelle code). */
typedef struct gmb_wait_entry {
    gmb_task_t* task;
    gmb_select_ctx_t* sel;   /* NULL per OP_SEND/OP_RECV semplici */
    int case_idx;
} gmb_wait_entry_t;

/* ---- TRY/CATCH/FINALLY sospendibile (specchio TryInfo + PCodeUnwindState PCodeVM) ---- */
#define GMB_TRY_STACK_CAP 64
#define GMB_GOSUB_CAP 64

/* Specchio di PCodeTask.TryInfo: una regione TRY attiva. Refcount perché la stessa
 * voce vive sia in try_stack sia in cleanup_stack (come l'oggetto TryInfo condiviso
 * dai due stack in C#). */
typedef struct gmb_try_entry {
    int refcount;
    int protected_start;     /* ip subito dopo OP_PUSH_TRY */
    int handler_ip;          /* inizio CATCH */
    int stack_pointer;       /* sp al push */
    struct gmb_frame* frame; /* frame al push (IsFrameInChain) */
    int finally_ip;          /* side-table TryRegions (== handler_ip se assente) */
    int end_ip;              /* fine regione (dopo FINALLY) */
    uint8_t has_catch;
} gmb_try_entry_t;

/* Specchio di PCodeUnwindState/PCodeUnwindReason. */
enum {
    GMB_UW_EXCEPTION = 1,
    GMB_UW_RETURN,
    GMB_UW_GOTO,
    GMB_UW_RETURN_GOSUB,
    GMB_UW_CANCEL,
    GMB_UW_GO_EXIT
};

typedef struct gmb_unwind {
    uint8_t reason;          /* GMB_UW_* */
    int origin_ip;
    struct gmb_frame* frame;
    int target_ip;           /* Goto */
    int return_opcode_ip;    /* Return: riesegue OP_RETURN a fine unwind */
    struct gmb_try_entry* target_handler; /* Exception: voce CATCH (ref trattenuta) */
    gmb_stack_entry_t error_value;
    uint8_t has_error_value;
    /* ReturnGosub: continuation da ripristinare */
    int gosub_return_ip;
    int gosub_sp;
    int gosub_try_depth;
    int gosub_cleanup_depth;
} gmb_unwind_t;

/* Continuation GOSUB (specchio PCodeGosubContinuation). */
typedef struct gmb_gosub_cont {
    int return_ip;
    int sp;
    int try_depth;
    int cleanup_depth;
    struct gmb_frame* frame;
} gmb_gosub_cont_t;

#include <stdlib.h>
/* Rilascia una voce TRY refcountata (vive in try_stack e/o cleanup_stack e/o unwind). */
static inline void gmb_try_entry_release(gmb_try_entry_t* e)
{
    if (e && --e->refcount <= 0) free(e);
}

/* ---- Frame di chiamata (specchio di PCodeCallFrame) ---- */
#define GMB_FRAME_LOCALS_CAP 256   /* capacità iniziale dei locals di un frame */
#define GMB_FRAME_STACK_CAP 256
#define GMB_FRAME_DEFER_CAP 64

/* SCOPE DINAMICO (DYNAMIC, spec Variables-Scope §2.5): tabella name→valore di una
 * procedura. La catena dei frame È la catena dello scope dinamico. I valori sono
 * POSSEDUTI (type==2 → ref rilasciato alla free). Vedi OP_DYN_* in gmb_vm_core.c. */
typedef struct {
    char**             names;
    gmb_stack_entry_t* vals;
    int                count;
    int                cap;
} gmb_dyn_scope_t;

/* Stato dei cicli FOR (specchio PCodeCallFrame.ForState, fissato 2026-10-28).
 * Lo stato di un ciclo è del FRAME che lo esegue, non del task: una chiamata annidata
 * riusa gli stessi numeri di slot, quindi con stato indicizzato dal solo slot il
 * FOR_PREPARE del callee sovrascriveva end/step del chiamante e il suo FOR_LOOP
 * lasciava active=0 → il FOR esterno terminava e le iterazioni andavano perse. */
/* Stato dei cicli FOR — MAPPA SPARSA slot → (counter, end, step), come
 * PCodeCallFrame.ForState (Dictionary<int,(double,double,double)> in GpmCore.cs).
 *
 * Prima erano quattro array densi di 64 elementi indicizzati dallo slot del locale: una
 * capacità fissa arbitraria, mentre gli slot di una procedura crescono in fretta perché
 * ogni temp del compilatore (cc_local_add, gmb_compiler.c) ne occupa uno nuovo e non
 * viene mai riusato. Con slot >= 64 il FOR_PREPARE non marcava il ciclo e il FOR_LOOP
 * usciva subito: UNA sola iterazione, in silenzio (caso reale: SUB PcfSave di GlyphSmith,
 * PCF con una proprietà invece di nove).
 *
 * La mappa non ha alcun tetto di slot e paga solo per i cicli DAVVERO aperti nel frame
 * (tipicamente 1-3, ricerca lineare su pochi elementi). Il ciclo è aperto se la voce
 * esiste: alla fine si rimuove, esattamente come il Remove(slot) di PCodeVM. */
typedef struct {
    int    slot;
    double counter;
    double end;
    double step;
} gmb_for_entry_t;

typedef struct {
    gmb_for_entry_t* v;
    int n;
    int cap;
} gmb_for_state_t;

/* Voce del ciclo aperto sullo slot, o NULL se non c'è (ciclo chiuso/mai preparato). */
static inline gmb_for_entry_t* gmb_for_find(gmb_for_state_t* s, int slot)
{
    if (!s) return NULL;
    for (int i = 0; i < s->n; i++)
        if (s->v[i].slot == slot) return &s->v[i];
    return NULL;
}

/* Apre (o riapre) il ciclo sullo slot. NULL solo se manca memoria. */
static inline gmb_for_entry_t* gmb_for_upsert(gmb_for_state_t* s, int slot,
                                              double counter, double end, double step)
{
    if (!s) return NULL;
    gmb_for_entry_t* e = gmb_for_find(s, slot);
    if (!e)
    {
        if (s->n >= s->cap)
        {
            int nc = s->cap ? s->cap * 2 : 4;
            gmb_for_entry_t* nv = (gmb_for_entry_t*)realloc(s->v, (size_t)nc * sizeof(*nv));
            if (!nv) return NULL;
            s->v = nv;
            s->cap = nc;
        }
        e = &s->v[s->n++];
        e->slot = slot;
    }
    e->counter = counter;
    e->end = end;
    e->step = step;
    return e;
}

/* Chiude il ciclo sullo slot (PCode: ForState.Remove(slot)). */
static inline void gmb_for_drop(gmb_for_state_t* s, int slot)
{
    if (!s) return;
    for (int i = 0; i < s->n; i++)
        if (s->v[i].slot == slot)
        {
            s->v[i] = s->v[s->n - 1];
            s->n--;
            return;
        }
}

/* Libera la mappa (frame in uscita, task in distruzione). */
static inline void gmb_for_clear(gmb_for_state_t* s)
{
    if (!s) return;
    free(s->v);
    s->v = NULL;
    s->n = 0;
    s->cap = 0;
}

typedef struct gmb_frame {
    struct gmb_frame* prev;
    int      refcount;   /* 1 = nella frame chain; i box BYREF aggiungono ref */
    int      saved_sp;
    int      return_ip;
    /* Locals DINAMICI (2026-11-05): capacità decisa alla creazione dal local_count della
     * procedura e crescibile on demand. Prima erano array fissi a 256: una SUB con più di
     * ~127 variabili perdeva silenziosamente gli slot oltre il limite (VM/C divergeva). */
    int      nlocals;
    double*  local_nums;
    gmb_obj_t** local_refs;
    uint8_t* local_types;
    uint8_t* local_basic;
    gmb_stack_entry_t frame_stack[GMB_FRAME_STACK_CAP];
    int      frame_sp;
    /* upvalue catturate (per closure annidate); [0..15] */
    gmb_stack_entry_t upvalues[16];
    /* 1 = il frame HA una ref propria su ciascuna upvalue (solo OP_CALL_INDIRECT, dove
     * la closure chiamata può essere l'ultima referenza e viene rilasciata subito). */
    uint8_t upvalues_owned;
    gmb_stack_entry_t return_value;
    /* V4f: classi utente — specchio PCodeCallFrame.ConstructorInstance: nei frame di
     * costruttore (OP_NEW_CLASS_INSTANCE) il RETURN ripusha l'istanza sullo stack. */
    gmb_obj_t* constructor_instance;
    /* V4f-bis: frame creato da OP_CALL_CTOR (BASE.NEW, setter, BASE.metodo()): chiamata
     * DIRETTA per indirizzo, non una nuova istanza. In OP_RETURN usa il check RELATIVO
     * (sp > saved_sp) e non l'assoluto (sp > 0): un costruttore/setter VOID non pusha
     * nulla, ma con lo stack del chiamante non vuoto (es. L.Add(NEW B(...))) il check
     * assoluto poppava il residuo (la LIST) come return spurio. Specchio
     * PCodeCallFrame.IsDirectCall. */
    uint8_t is_direct_call;
    /* V4e: scope ON ERROR per-procedura (specchio EnterProcedureErrorScope/
     * RestoreCallerErrorScope di PCodeProcessor.Helpers): ogni frame è un boundary
     * che salva lo stato error del chiamante e lo ripristina al RETURN. */
    uint8_t is_error_scope_boundary;
    int saved_error_handler_ip;
    uint8_t saved_error_handler_is_gosub;
    int saved_error_mode;
    int saved_error_line_ip;
    int saved_error_resume_next_ip;
    /* V6c DEFER (parità Go defer, specchio PCodeCallFrame.DeferredIps): stack LIFO degli
     * indirizzi dei body registrati da OP_DEFER; eseguiti al RETURN della procedura. */
    int defer_ips[GMB_FRAME_DEFER_CAP];
    int defer_count;
    /* SCOPE DINAMICO (DYNAMIC): dichiarazioni della procedura. Vedi gmb_dyn_scope_t. */
    gmb_dyn_scope_t dyn;
    /* Stato dei cicli FOR aperti in QUESTO frame (muore col frame → il FOR del chiamante
     * non può essere spento dal RETURN del callee). Vedi for_state_of() in gmb_vm_core.c. */
    gmb_for_state_t for_state;
} gmb_frame_t;

/* SCOPE DINAMICO — libera i contenuti di uno scope (nomi + ref posseduti). */
void gmb_dyn_scope_clear(gmb_dyn_scope_t* s);

/* Rilascia un ref al frame (libera quando il conteggio scende a zero). */
void gmb_frame_release(gmb_frame_t* f);

/* Porta i locals del frame/task alla capacità n (>=), preservando i valori; 0 = OOM. */
int gmb_frame_ensure_locals(gmb_frame_t* f, int n);
int gmb_task_ensure_locals(gmb_task_t* t, int n);

/* ---- Box BYREF (specchio ByRefBox C#): riferimento write-through a uno slot
 * local/global. Il frame target è trattenuto con un ref finché il box vive. */
typedef struct gmb_obj_byref {
    struct gmb_obj header;
    struct gmb_task* owner;    /* task proprietario degli array (globals/locals top) */
    gmb_frame_t* frame;        /* NULL = globals (o locals top-level se is_global=0) */
    int slot;
    uint8_t is_global;
    uint8_t is_cell;          /* closure: valore posseduto dal box, non slot indiretto */
    gmb_stack_entry_t cell;
    /* Elemento di un array GMBasic (`Arr(i)` come argomento BYREF, OP_MAKE_BYREF_ELEMENT):
     * un TERZO modo, distinto da slot locale/globale — l'array vive come gmb_list_t dentro
     * un gmb_obj_shared_val_t (GMB_OBJ_LIST), non nelle parallel-array locals/globals, quindi
     * serve un riferimento diretto all'oggetto lista + indice (slot sopra riusato come
     * indice quando is_array_elem è vero). Il box trattiene una ref sull'array (rilasciata in
     * byref_release_fn) per tenerlo vivo per tutta la vita del box, come già fa con `frame`. */
    uint8_t is_array_elem;
    gmb_obj_shared_val_t* arr;
} gmb_obj_byref_t;

gmb_obj_byref_t* gmb_byref_new_local(struct gmb_task* owner, gmb_frame_t* frame, int slot);
gmb_obj_byref_t* gmb_byref_new_global(struct gmb_task* owner, int slot);
/* Cella BYREF con valore PROPRIO (non legata a uno slot): usata dal driver EVAL per
 * passare campi DBF / nomi ignoti al frammento compilato (scritture visibili al
 * chiamante, che le riporta sul record dopo la chiamata). */
gmb_obj_byref_t* gmb_byref_new_cell(struct gmb_task* owner, gmb_stack_entry_t value);
gmb_obj_byref_t* gmb_byref_new_array_elem(struct gmb_task* owner, gmb_obj_shared_val_t* arr, int index);
gmb_stack_entry_t gmb_task_capture_local_byref(struct gmb_task* owner, int slot);
/* Cella write-through per TARGET di form (@ row,col GET <var locale>): a differenza di
 * gmb_task_capture_local_byref imbusta anche le STRINGHE. Le closure NON la usano: per
 * loro una stringa resta l'oggetto condiviso (proxy a un frame poppato = garbage). */
gmb_stack_entry_t gmb_task_capture_local_cell(struct gmb_task* owner, int slot);
gmb_stack_entry_t gmb_task_capture_global_byref(struct gmb_task* owner, int slot);
gmb_stack_entry_t gmb_byref_load(const gmb_obj_byref_t* box);
void gmb_byref_store(gmb_obj_byref_t* box, gmb_stack_entry_t value);

struct gmb_task {
    int id;
    int state;               /* GMB_TS_* */
    int in_queue;
    int executing;
    int wake_pending;
    int force_yield;         /* Gosched esplicito (TaskYield/OP_YIELD): il worker deve riaccodare */
    pthread_mutex_t lock;    /* EnqueueLock per-task */
    int ip;
    /* stack operandi — dinamico (V1): realloc geometrico, mai drop silenzioso.
     * overflow=1 (cap raggiunto o OOM) → il dispatcher abortisce il task con
     * errore deterministico al prossimo dispatch. */
    gmb_stack_entry_t* stack;
    int sp;
    int stack_cap;
    int stack_overflow;
    /* locals (usati quando frame==NULL — GetLocals) — dinamici come i frame */
    int       locals_cap;
    double*   locals_nums;
    gmb_obj_t** locals_refs;
    uint8_t*  locals_types;
    uint8_t*  locals_basic;
    /* frame di chiamata */
    gmb_frame_t* frame;
    /* globals condivisi per riferimento (come goTask.Globals = task.Globals) */
    double*    globals_nums;
    gmb_obj_t** globals_refs;
    uint8_t*   globals_types;
    uint8_t*   globals_basic;
    uint32_t   globals_count;
    int        owns_globals;   /* 1 = questo task possiede i globals e li libera in gmb_task_free */
    /* sleep */
    int64_t sleep_until_ms;
    /* V7a-net (§12): deadline assoluta del syscall di rete parcheggiato (TCP ACCEPT/
     * TCP RECV/UDP RECV). 0 = attesa indefinita. Azzerata al completamento. */
    int64_t net_deadline_ms;
    /* V7b (§12): handle del CONNECT non-bloccante in corso, persistito fra park/retry
     * (0 = nessun connect in volo). Il timeout chiude l'handle e spinge -1. */
    int64_t net_connect_handle;
    /* V7b (§12): byte già inviati del buffer TCP SEND parzialmente scritto, persistiti
     * fra park/retry (POLLOUT). 0 = nuovo send. */
    uint32_t net_send_offset;
    /* V7c (§12, ABA): generation token dell'handle su cui il task è parcheggiato
     * (catturata al park da gmb_net_handle_gen). Al retry, se la gen corrente
     * differisce lo slot è stato chiuso/riusato → la syscall risolve come "chiuso"
     * invece di operare sul socket nuovo. 0 = nessun park attivo. */
    uint32_t net_park_gen;
    /* §16: job DNS asincrono in volo per il TCP CONNECT (0 = nessuno), persistito
     * fra park/retry. Il park attende la pipe del job; il timeout lo cancella
     * (gmb_net_dns_cancel, anche in gmb_task_free se il task muore parcheggiato). */
    int64_t net_dns_job;
    /* HTTP USE NEXT (parità _httpTaskNextHandle del modello managed): id dello step di
     * catena della richiesta che ha spawnato QUESTO task (0 = fuori catena / passo finale).
     * Il wrapper gmb_vm_http_handler_ex lo imposta da req_c->chain_step_id; la forma
     * implicita "HTTP USE NEXT" (senza handle) risolve l'argomento a nil e lo ripesca da
     * qui, perche' il TLS g_http_current_step del server thread NON e' visibile al worker
     * GPM che esegue il middleware. */
    int64_t http_chain_step;
    /* V4: TRY/CATCH/FINALLY sospendibile (specchio TryStack/CleanupStack/PendingUnwind
     * di PCodeTask). Le voci sono refcounted e condivise fra i due stack. */
    gmb_try_entry_t* try_stack[GMB_TRY_STACK_CAP];
    int try_count;
    gmb_try_entry_t* cleanup_stack[GMB_TRY_STACK_CAP];
    int cleanup_count;
    gmb_unwind_t unwind;
    uint8_t unwind_active;
    /* V6c DEFER (specchio PCodeTask.PendingDeferReturn): non-zero mentre OP_RETURN sta
     * eseguendo i body DEFER LIFO della procedura; azzerato dall'ultimo OP_DEFER_RET. */
    uint8_t pending_defer_return;
    /* V6c DEFER task-level (specchio PCodeTask.DeferredIps): per task SENZA frame
     * (top-level/__main e goroutine radice GO SUB), dove non esiste un gmb_frame_t
     * su cui registrare i body. */
    int task_defer_ips[GMB_FRAME_DEFER_CAP];
    int task_defer_count;
    /* V6d ONCE (specchio PCodeTask.PendingOnce): il Once iniziato da OP_ONCE_BEGIN di
     * questo task; completato dal suo OP_ONCE_END. Ref trattenuto dal task. */
    gmb_once_t* pending_once;
    /* V4: cancellazione (specchio CancellationRequested/CancellationUnwindStarted di
     * PCodeTask): il tick loop inizia un unwind Cancel che attraversa i FINALLY. */
    uint8_t cancellation_requested;
    uint8_t cancellation_unwind_started;
    gmb_gosub_cont_t gosub_stack[GMB_GOSUB_CAP];
    int gosub_count;
    /* ip dell'opcode in esecuzione (specchio CurrentInstructionIp): origine degli unwind */
    int current_ip;
    /* ON ERROR GOTO/GOSUB (specchio ErrorHandlerIp/ErrorHandlerIsGosub + IpStack) */
    int error_handler_ip;        /* 0 = nessuno */
    uint8_t error_handler_is_gosub;
    int ip_stack[64];
    int ip_stack_count;
    /* V4e: stato error corrente (specchio ErrorMode/ErrorLineIp/ErrorResumeNextIp) */
    int error_mode;              /* 0 = None, 1 = ResumeNext */
    int error_line_ip;           /* -1 = nessuno (RESUME retry) */
    int error_resume_next_ip;    /* -1 = nessuno (RESUME NEXT) */
    /* future da risolvere (GO FUNCTION) */
    gmb_future_t* future_to_resolve;
    /* waitgroup implicito (GO SUB/FUNCTION ... IN wg): il contatore viene decrementato
     * (gmb_wg_done) quando il task finisce — specchio di PCodeTask.WaitGroup in IlGpm. */
    gmb_waitgroup_t* waitgroup;
    /* Gruppo implicito dei figli lanciati con GO SUB/FUNCTION senza IN (WAIT nuda,
     * bug 2026-10-01): creato al primo figlio, Add(1) per figlio, Done implicito a
     * fine task via go->waitgroup; il ref del genitore è rilasciato in gmb_task_free. */
    gmb_waitgroup_t* implicit_waitgroup;
    char* pending_exception;         /* messaggio o NULL */
    /* Codice/riga/descrizione dell'errore che ha reso il task FATALE (pending_exception).
     * Serve ai driver che devono RILANCIARE l'errore nel contesto di un altro task invece
     * di trasformarlo in StopIteration (generatori, parità C99Native gmb_gen_next). */
    int fatal_err_code;
    long fatal_err_line;
    char* fatal_err_desc;
    /* NEXT() su un generatore il cui corpo è uscito per ERRORE: il codice vero viaggia fino
     * all'handler del chiamante (parità C99Native gmb_gen_next, che rilancia subito). */
    int gen_err_pending;
    int gen_err_code;
    char* gen_err_desc;
    /* DATA pointer (OP_READ/OP_RESTORE): indice nel chunk->data */
    int data_index;
    /* Generatore (GENERATOR PROCEDURE): valore prodotto dall'ultimo YIELD
     * (OP_NEXT_GENERATOR) — letto dal driver NEXT del generatore. */
    gmb_stack_entry_t pending_result;
    int has_pending_result;
    /* ForState del task: usato SOLO quando non c'è un gmb_frame_t (top-level/__main e
     * goroutine radice GO SUB). Con un frame attivo si usa frame->for_state — vedi
     * for_state_of() — perché lo stato di un ciclo FOR appartiene al frame che lo esegue. */
    gmb_for_state_t for_state;
    /* NEXT() su generatore esaurito → StopIteration (ERR.Number=144), gestito da OP_CALL_BUILTIN */
    int stop_iteration;
    /* V6: transazione GO SELECT parcheggiata (specchio PCodeTask.PendingSelect): al wake il
     * case vincente (CAS nel ctx) viene letto alla ri-esecuzione del SETUP; il task rilascia
     * il proprio riferimento quando il SETUP riparte. */
    gmb_select_ctx_t* select_ctx;
    /* TIMEOUT case di GO SELECT (parità Go case <-time.After(d)): il park arma
     * sleep_until_ms con la deadline assoluta e scrive la deadline nello slot; il
     * flag distingue il primo giro (slot = durata, non pronto) dal retry (slot =
     * deadline epoch, pronto se gmb_now_ms() >= slot). */
    uint8_t select_timer_armed;
    /* puntatori a scheduler/vm per il dispatcher */
    gmb_scheduler_t* scheduler;
    void* vm_ctx;                    /* gmb_vm_t* */
    /* stampa bufferizzata per-riga (come PrintLineStaging/PendingPrints) */
    char print_staging[1024];
    int  print_staging_len;
    char print_line[2048];
    int  print_line_len;
    /* chiamata sincrona di closure dai builtin (gmb_vm_call_closure_sync) */
    int sync_active;
    /* Rilasci DIFFERITI: oggetti il cui riferimento e' stato tolto dallo stack ma i cui dati (es. il testo di una
     * stringa-chiave usato come `const char*`) servono fino alla fine dell'istruzione corrente. Svuotati
     * all'inizio dell'istruzione successiva (gmb_vm_tick). */
    struct gmb_obj* defer_rel[8];
    int n_defer_rel;
    int sync_saved_ip;
    double sync_result;
    gmb_obj_t* sync_result_ref;  /* ritorno oggetto (list/dict/string) da closure sync */
    uint8_t sync_result_type;    /* tipo dello stack entry ritornato */
    /* SCOPE DINAMICO (DYNAMIC): fallback quando il task non ha frame di chiamata (corpo
     * top-level). Normalmente le dichiarazioni vivono su gmb_frame_t.dyn lungo la catena
     * dei frame; questo scope base copre i casi senza frame. Lazy (può restare NULL).
     * Liberato da gmb_task_free. Vedi OP_DYN_* in gmb_vm_core.c. */
    gmb_dyn_scope_t* dyn_base;
};
/* Capacità corrente degli slot locali (frame attivo, altrimenti locals top-level). */
static inline int gmb_locals_cap_of(const gmb_task_t* t)
{
    return t->frame ? t->frame->nlocals : t->locals_cap;
}


/* ---- Scheduler — specchio di PCodeScheduler ---- */
struct gmb_scheduler {
    /* coda FIFO condivisa (BlockingCollection) */
    gmb_task_t** queue;
    int queue_cap;
    int queue_head;
    int queue_tail;
    int queue_count;
    pthread_mutex_t qlock;
    pthread_cond_t qcond;
    int shutdown;                     /* CompleteAdding */
    /* tutti i task (per WakeExpiredSleepers) */
    gmb_task_t** all_tasks;
    int all_cap;
    int all_count;
    pthread_mutex_t all_lock;
    int next_id;
    /* V7a (§7.1): completion condition — il main attende su ccond invece di
     * pollare a 1ms; active_count è il numero di task non FINISHED. */
    pthread_mutex_t clock;
    pthread_cond_t ccond;
    int active_count;
    /* F1 (park VM/C per builtin bloccanti tipo AI.BLACKBOARDWAIT/AI.AGENTSWARM):
     * numero di job pthread dedicati in corso (vm_try_park_str/vm_try_park_cpu).
     * Il task e' BLOCKED (non executing, non in coda) mentre il job lavora — senza
     * questo contatore il watchdog gmb_sched_is_stuck (coda vuota + nessun executing)
     * scambia il park per un vero deadlock e cancella il programma in anticipo.
     * Protetto dallo stesso mutex 'clock' (nessun nuovo lock: stesso schema di
     * active_count, incrementato/decrementato in coppia dallo stesso chiamante). */
    int async_job_count;
    /* V7a (§7.1/§11): timer min-heap per gli sleep (sostituisce la scansione
     * di tutti i task). I nodi {deadline, task, id} sono validati al fire:
     * membership in all_tasks (tenuto sotto all_lock da gmb_task_free) + id. */
    pthread_mutex_t tlock;
    pthread_cond_t tcond;
    struct gmb_timer_node* timers;
    int timer_count;
    int timer_cap;
};

/* Nodo del timer min-heap (V7a). */
typedef struct gmb_timer_node {
    int64_t deadline;      /* ms (base gmb_now_ms) */
    gmb_task_t* task;
    int task_id;           /* anti-ABA su riuso indirizzo */
} gmb_timer_node_t;

/* ---- Canale — specchio di ChannelInfo ---- */
struct gmb_channel {
    gmb_obj_t header;         /* kind=GMB_OBJ_CHANNEL, release_fn dedicata */
    pthread_mutex_t lock;
    int closed;                     /* V5: close(ch) Go — impostato atomicamente sotto lock */
    gmb_stack_entry_t* buffer;        /* Queue<object?> — entry completa (num+ref) */
    int buffer_cap;
    int buffer_head;
    int buffer_tail;
    int buffer_count;
    int capacity;                     /* default 10 */
    /* waiting receivers/senders — entry complete (task + eventuale SelectContext/case) */
    gmb_wait_entry_t* recv_entries;
    int recv_cap, recv_head, recv_tail, recv_count;
    gmb_wait_entry_t* send_entries;
    int send_cap, send_head, send_tail, send_count;
};

/* ---- WaitGroup — specchio di PCodeWaitGroup ---- */
struct gmb_waitgroup {
    gmb_obj_t header;         /* kind=GMB_OBJ_WAITGROUP */
    pthread_mutex_t lock;
    int counter;
    gmb_task_t** waiters;
    int wcap, whead, wtail, wcount;
};

/* ---- Mutex — specchio di PCodeMutex (parità Go sync.Mutex) ----
 * LOCK parcheggia il task se il mutex è detenuto; UNLOCK trasferisce la proprietà
 * al primo waiter (handoff) e lo sveglia: al risveglio il task ri-esegue OP_LOCK e
 * gmb_mutex_try_lock lo riconosce come destinatario senza ri-parcheggiarlo. */
struct gmb_mutex {
    gmb_obj_t header;         /* kind=GMB_OBJ_MUTEX */
    pthread_mutex_t lock;
    int held;
    gmb_task_t* owner;
    gmb_task_t** waiters;
    int wcap, wcount;
};

/* ---- RwMutex — specchio di PCodeRwMutex (parità Go sync.RWMutex) ----
 * RLOCK condiviso tra lettori; LOCK (scrittore) esclusivo. WRITER-PREFERENCE:
 * se uno scrittore attende, i nuovi RLOCK si accodano dietro di lui. Handoff come
 * il mutex semplice: il risvegliato ri-esegue OP_LOCK/OP_RLOCK e acquisisce. */
typedef struct gmb_rwlock_waiter { gmb_task_t* task; int is_writer; } gmb_rwlock_waiter_t;
struct gmb_rwlock {
    gmb_obj_t header;         /* kind=GMB_OBJ_RWMUTEX */
    pthread_mutex_t lock;
    int readers;
    int writer_active;
    gmb_task_t* writer_owner;
    int writer_waiting;
    gmb_rwlock_waiter_t* waiters;
    int wcap, wcount;
};

/* ---- Once — specchio di PCodeOnce (parità Go sync.Once) ----
 * TryBegin: NotRun→Running (esegui); Done→salta; Running→parcheggia. Complete
 * marca Done e sveglia TUTTI i waiter (ri-eseguono OP_ONCE_BEGIN → skip). */
#define GMB_ONCE_NOT_RUN 0
#define GMB_ONCE_RUNNING  1
#define GMB_ONCE_DONE     2
struct gmb_once {
    gmb_obj_t header;         /* kind=GMB_OBJ_ONCE */
    pthread_mutex_t lock;
    int state;                /* GMB_ONCE_* */
    gmb_task_t** waiters;
    int wcap, wcount;
};

/* ---- Atomic — specchio di PCodeAtomic (parità Go sync/atomic) ----
 * Box int64 con accesso lock-free via builtin __atomic (seq_cst, come Interlocked). */
struct gmb_atomic {
    gmb_obj_t header;         /* kind=GMB_OBJ_ATOMIC */
    long long value;          /* accesso SOLO via __atomic_* */
};

/* ---- Pool — specchio di PCodePool (parità Go sync.Pool) ----
 * Coda FIFO di entry complete (num+ref); PUT accoda (trattiene il ref), GET sfila
 * la prima (trasferisce l'ownership) o NULL se vuota (Go: Pool senza New → nil). */
struct gmb_pool {
    gmb_obj_t header;         /* kind=GMB_OBJ_POOL */
    pthread_mutex_t lock;
    gmb_stack_entry_t* items;
    int cap, head, count;
};

/* ---- Cond — specchio di PCodeCond (parità Go sync.Cond) ----
 * WAIT rilascia il mutex associato e parcheggia; SIGNAL sveglia UN waiter (FIFO),
 * BROADCAST tutti. Il compiler emette un OP_LOCK subito dopo OP_COND_WAIT: al wake
 * il task riacquisisce il mutex (bloccandosi se tenuto). */
struct gmb_cond {
    gmb_obj_t header;         /* kind=GMB_OBJ_COND */
    pthread_mutex_t lock;
    gmb_task_t** waiters;
    int wcap, wcount;
};

/* ---- Context — specchio di PCodeContext (parità Go context.Context) ----
 * BACKGROUND = radice; WITHCANCEL = figlio registrato nel parent; CANCEL marca e
 * PROPAGA ai figli (un figlio creato dopo la cancel nasce già cancellato); DONE
 * ritorna un canale chiuso alla cancellazione. */
struct gmb_context {
    gmb_obj_t header;         /* kind=GMB_OBJ_CONTEXT */
    pthread_mutex_t lock;
    int cancelled;
    gmb_channel_t* done_channel;      /* lazy; chiuso alla cancel */
    gmb_context_t* parent;            /* NON posseduto (il figlio vive nel parent) */
    gmb_context_t** children;         /* ogni slot tiene UN ref al figlio */
    int ccap, ccount;
};

/* ---- ConcurrentMap — specchio di PCodeConcurrentMap (parità Go sync.Map) ----
 * Mappa K/V thread-safe: ogni accesso sotto il mutex interno. Le chiavi sono entry
 * dello stack (numero o stringa — stesse regole di OP_STORE_ELEMENT); i valori sono
 * entry con retain proprio (STORE/PUT adottano, LOAD ritorna copia con retain). */
typedef struct gmb_cmap_entry { gmb_stack_entry_t key; gmb_stack_entry_t val; } gmb_cmap_entry_t;
struct gmb_concurrentmap {
    gmb_obj_t header;         /* kind=GMB_OBJ_CONCURRENTMAP */
    pthread_mutex_t lock;
    gmb_cmap_entry_t* items;
    int count, cap;
};

/* ---- Future — specchio di PCodeFutureValue ---- */
struct gmb_future {
    gmb_obj_t header;         /* kind=GMB_OBJ_FUTURE */
    pthread_mutex_t lock;
    int is_resolved;
    int is_failed;            /* V4g: specchio FailureException (GO FUNCTION fallita) */
    char fail_msg[256];
    gmb_obj_t* result;      /* valore (stringa/num boxed — vedi note) */
    double result_num;
    int result_is_num;
    gmb_task_t** waiters;
    int wcap, whead, wtail, wcount;
};

/* ---- SelectContext (V6) — specchio di SelectContext in GpmChannels.cs ----
 * Coordina l'attesa di GO SELECT su più canali: un solo case può vincere (CAS sotto
 * lock), il vincitore stacca (detach) tutte le iscrizioni loser dai canali registrati.
 * Refcount: ogni entry parcheggiata tiene un riferimento (rilasciato al pop/detach) più
 * il riferimento del task (rilasciato quando il SETUP riparte). Array dimensionati dal
 * chunk (num_cases u16, parità §20.3 PCodeVM — prima: limite fisso 16). */
struct gmb_select_ctx {
    int refcount;                       /* atomico (built-in __atomic) */
    pthread_mutex_t lock;
    int winner;                         /* -1 = pending, >=0 = case vincente */
    gmb_task_t* task;
    gmb_channel_t** chans;
    int* cases;
    int cap;
    int count;
};

/* ---- API scheduler ---- */
gmb_scheduler_t* gmb_sched_create(void);
void gmb_sched_destroy(gmb_scheduler_t* s);
gmb_task_t* gmb_task_create(gmb_scheduler_t* s, int start_ip);
gmb_task_t* gmb_task_create_unregistered(int start_ip);
void gmb_sched_enqueue(gmb_scheduler_t* s, gmb_task_t* t);   /* Schedule/Wake/Yield */
void gmb_sched_release_executing(gmb_scheduler_t* s, gmb_task_t* t);
gmb_task_t* gmb_sched_find_runnable(gmb_scheduler_t* s);      /* blocca finché non c'è lavoro */
void gmb_sched_signal_shutdown(gmb_scheduler_t* s);
bool gmb_sched_has_active(gmb_scheduler_t* s);
void gmb_sched_wake_expired_sleepers(gmb_scheduler_t* s, int64_t now_ms);
/* V4: cancellazione cooperativa (specchio GpmCore.RequestCancellation/CancelTasks):
 * marca tutti i task non finiti e li sveglia; il tick loop avvia l'unwind Cancel. */
void gmb_sched_request_cancellation(gmb_scheduler_t* s);
int64_t gmb_sched_earliest_sleep(gmb_scheduler_t* s);
void gmb_task_mark_finished(gmb_task_t* t);

/* ---- V7a (§7.1): completion condition + timer min-heap ---- */
/* Chiamato dal worker quando un task passa a FINISHED (decrementa active_count
 * e segnala ccond a zero). */
void gmb_sched_note_task_finished(gmb_scheduler_t* s);
/* Marcatori attività non-task (sessione WS server): hold all'avvio del receive
 * loop, release alla fine — la completion attende anche le sessioni vive. */
void gmb_sched_hold_active(gmb_scheduler_t* s);
void gmb_sched_release_active(gmb_scheduler_t* s);
/* F1: marcatori del job pthread dedicato (vm_try_park_str) — vedi async_job_count
 * sopra. Da chiamare in coppia intorno allo spawn/completion del thread. */
void gmb_sched_async_job_begin(gmb_scheduler_t* s);
void gmb_sched_async_job_end(gmb_scheduler_t* s);
/* Attende active_count==0. timeout_ms<0 = attesa illimitata. Ritorna 1 se tutti
 * i task sono completati, 0 se il timeout scade prima. */
int gmb_sched_wait_all_done_ms(gmb_scheduler_t* s, int64_t timeout_ms);
int gmb_sched_active_count(gmb_scheduler_t* s);
/* Inserisce {t->sleep_until_ms, t} nel min-heap e sveglia il watcher. Chiamare
 * DOPO aver impostato sleep_until_ms/state del task (OP_SLEEP, timeout SELECT). */
void gmb_sched_arm_timer(gmb_scheduler_t* s, gmb_task_t* t);
/* Svuota i nodi scaduti: valida membership/id sotto all_lock e riaccoda i task
 * ancora BLOCKED con deadline coerente. */
void gmb_sched_fire_expired_timers(gmb_scheduler_t* s, int64_t now_ms);
/* Deadline del nodo minimo (-1 se heap vuoto). */
int64_t gmb_sched_next_timer_deadline(gmb_scheduler_t* s);
/* Attesa idle del watcher: blocca su tcond fino a next_deadline (cap 250ms);
 * un arm_timer la interrompe prima. */
void gmb_sched_timer_wait(gmb_scheduler_t* s, int64_t next_deadline);
/* V7a-net: rimuove il task dalla run-queue (se presente) e azzera in_queue.
 * Usato dai runner single-thread (gmb_vm_run/run_debug): lì i wake hook
 * accodano ma la coda non viene mai consumata dai worker. */
void gmb_sched_reclaim_queued(gmb_scheduler_t* s, gmb_task_t* t);
/* Watchdog deadlock (parità Go "fatal error: all goroutines are asleep - deadlock!"):
 * true se la run-queue è vuota E nessun worker sta ticchettando un task in questo
 * istante (nessun t->executing). Il chiamante DEVE combinarlo con "nessun timer armato"
 * e "nessun IO osservato" (gmb_sched_next_timer_deadline/gmb_iowatch_count) — uno stuck
 * momentaneo con un timer/IO pendente non è un deadlock, solo un'attesa legittima. */
int gmb_sched_is_stuck(gmb_scheduler_t* s);

/* ---- API canale ---- */
gmb_channel_t* gmb_channel_create(int capacity);
void gmb_channel_destroy(gmb_channel_t* c);
/* V5 (parità Go close(ch) / ChannelInfo.Close): chiude ATOMICAMENTE e svuota le code
 * dei waiter negli array out (il chiamante li riaccoda allo scheduler): receiver
 * svegliati con ok=false, sender con errore send-su-chiuso. Ritorna 1 se chiuso ora,
 * 0 se era già chiuso (double-close = errore in Go). */
int gmb_channel_close(gmb_channel_t* c, gmb_wait_entry_t** wake_recv, int* n_recv,
                      gmb_wait_entry_t** wake_send, int* n_send);
int gmb_channel_is_closed(gmb_channel_t* c);
/* TrySendOrPark: atomico. Imposta State=Blocked dentro il lock se parcheggia (la entry
 * porta sel/case_idx per GO SELECT, NULL/0 per SEND semplice). Ritorna 1 se inviato
 * (woken_receiver può essere valorizzato), 0 se parcheggiato, -1 se il canale è chiuso
 * (Go: send su chiuso = panic/errore). */
int gmb_channel_send_or_park(gmb_channel_t* c, gmb_obj_t* value, double num, int is_num,
    gmb_task_t* sender, gmb_select_ctx_t* sel, int case_idx, gmb_task_t** woken_receiver);
/* TryReceiveOrPark: atomico. Ritorna true se ricevuto (canale chiuso+buffer vuoto:
 * value=NULL e *closed_out=1 — Go v,ok := <-ch con ok=false), false se parcheggiato. */
bool gmb_channel_recv_or_park(gmb_channel_t* c, gmb_task_t* receiver, gmb_obj_t** value,
    double* num, int* is_num, int* closed_out, gmb_select_ctx_t* sel, int case_idx,
    gmb_task_t** woken_sender);
/* TrySend/TryReceive NON-bloccanti, senza park: per la fase di scan di GO SELECT
 * (prova tutti i canali; il park avviene solo se nessun case è pronto).
 * TrySend su chiuso ritorna false (come C# TrySend). Il *woken restituito è il task del
 * waiter vinto/risvegliato: per una entry di SELECT loser è NULL (claim CAS fallito). */
bool gmb_channel_try_send(gmb_channel_t* c, gmb_obj_t* value, double num, int is_num,
    gmb_task_t** woken_receiver);
bool gmb_channel_try_recv(gmb_channel_t* c, gmb_obj_t** value, double* num, int* is_num,
    gmb_task_t** woken_sender);
void gmb_channel_wake_one_sender(gmb_channel_t* c, gmb_task_t** out_sender);

/* ---- API SelectContext (V6) ---- */
gmb_select_ctx_t* gmb_select_create(gmb_task_t* task, int cap); /* refcount=1 (ref del task) */
void gmb_select_register(gmb_select_ctx_t* s, gmb_channel_t* ch, int case_idx);
int  gmb_select_pending(gmb_select_ctx_t* s);               /* 1 se nessun winner ancora */
/* CAS del winner. Ritorna 1 se questo case ha vinto. Chiama DETACH internamente quando
 * vince (lock dei canali preso DOPO il rilascio del lock del ctx — ordine lock sicuro). */
int  gmb_select_claim(gmb_select_ctx_t* s, int case_idx);
void gmb_select_detach(gmb_select_ctx_t* s);                /* rimuove le proprie entry dai canali */
void gmb_select_retain(gmb_select_ctx_t* s);
void gmb_select_release(gmb_select_ctx_t* s);               /* free a refcount 0 */

/* ---- API waitgroup ---- */
gmb_waitgroup_t* gmb_wg_create(void);
void gmb_wg_destroy(gmb_waitgroup_t* w);
void gmb_wg_add(gmb_waitgroup_t* w, int n);
bool gmb_wg_should_wait(gmb_waitgroup_t* w, gmb_task_t* t);   /* true se parcheggia */
void gmb_wg_done(gmb_waitgroup_t* w, gmb_scheduler_t* s);

/* ---- API mutex (specchio PCodeMutex, parità Go sync.Mutex) ---- */
gmb_mutex_t* gmb_mutex_create(void);
void gmb_mutex_destroy(gmb_mutex_t* m);
bool gmb_mutex_try_lock(gmb_mutex_t* m, gmb_task_t* t);   /* true se acquisito, false = parcheggia */
void gmb_mutex_unlock(gmb_mutex_t* m, gmb_scheduler_t* s);
/* MUTEX.TRYLOCK (Go 1.18, builtin 1207): acquisisce se libero, MAI parcheggia. */
bool gmb_mutex_try_lock_nb(gmb_mutex_t* m);

/* ---- API rwmutex (specchio PCodeRwMutex, parità Go sync.RWMutex) ---- */
gmb_rwlock_t* gmb_rwlock_create(void);
void gmb_rwlock_destroy(gmb_rwlock_t* rw);
bool gmb_rwlock_try_lock(gmb_rwlock_t* rw, gmb_task_t* t);   /* scrittore: true se acquisito */
void gmb_rwlock_unlock(gmb_rwlock_t* rw, gmb_scheduler_t* s);
bool gmb_rwlock_try_rlock(gmb_rwlock_t* rw, gmb_task_t* t);  /* lettore: true se acquisito */
void gmb_rwlock_runlock(gmb_rwlock_t* rw, gmb_scheduler_t* s);
/* MUTEX.TRYLOCK su RWMUTEX (scrittore): solo se nessun lettore/scrittore in attesa. */
bool gmb_rwlock_try_lock_nb(gmb_rwlock_t* rw);

/* ---- API once (specchio PCodeOnce, parità Go sync.Once) ---- */
gmb_once_t* gmb_once_create(void);
void gmb_once_destroy(gmb_once_t* o);
int gmb_once_try_begin(gmb_once_t* o, gmb_task_t* t);  /* GMB_ONCE_RUNNING=esegui, GMB_ONCE_DONE=salta, GMB_ONCE_NOT_RUN=parcheggiato */
void gmb_once_complete(gmb_once_t* o, gmb_scheduler_t* s);

/* ---- API atomic (specchio PCodeAtomic, parità Go sync/atomic) ---- */
gmb_atomic_t* gmb_atomic_create(void);
void gmb_atomic_destroy(gmb_atomic_t* a);
long long gmb_atomic_add(gmb_atomic_t* a, long long delta);    /* → valore PRECEDENTE */
long long gmb_atomic_load(gmb_atomic_t* a);
void gmb_atomic_store(gmb_atomic_t* a, long long v);
int gmb_atomic_cas(gmb_atomic_t* a, long long expected, long long desired); /* 1 se riuscito */
long long gmb_atomic_swap(gmb_atomic_t* a, long long v);      /* → valore PRECEDENTE */

/* ---- API pool (specchio PCodePool, parità Go sync.Pool) ---- */
gmb_pool_t* gmb_pool_create(void);
void gmb_pool_destroy(gmb_pool_t* p);
/* PUT: accoda l'entry PRENDENDO IN CARICO il suo retain (move). */
void gmb_pool_put(gmb_pool_t* p, gmb_stack_entry_t e);
/* GET: sfila la prima entry (move: il chiamante possiede il retain) o {0} se vuoto. */
gmb_stack_entry_t gmb_pool_get(gmb_pool_t* p);

/* ---- API cond (specchio PCodeCond, parità Go sync.Cond) ---- */
gmb_cond_t* gmb_cond_create(void);
void gmb_cond_destroy(gmb_cond_t* c);
/* WAIT: rilascia il mutex (MUTEX o RWMUTEX) e parcheggia il task sulla cond. */
void gmb_cond_wait(gmb_cond_t* c, gmb_obj_t* mutex_obj, gmb_task_t* t, gmb_scheduler_t* s);
void gmb_cond_signal(gmb_cond_t* c, gmb_scheduler_t* s);      /* sveglia UN waiter FIFO */
void gmb_cond_broadcast(gmb_cond_t* c, gmb_scheduler_t* s);   /* sveglia tutti */

/* ---- API context (specchio PCodeContext, parità Go context.Context) ---- */
gmb_context_t* gmb_context_create(gmb_context_t* parent);     /* NULL = BACKGROUND */
void gmb_context_destroy(gmb_context_t* c);
void gmb_context_cancel(gmb_context_t* c, gmb_scheduler_t* s);
int gmb_context_is_cancelled(gmb_context_t* c);
/* DONE: canale chiuso alla cancellazione. RESTITUISCE UN REF (ownership al chiamante);
 * i waiter svegliati dalla close sono negli array out (riaccodare allo scheduler). */
gmb_channel_t* gmb_context_done(gmb_context_t* c, gmb_wait_entry_t** wake_recv, int* n_recv,
                                gmb_wait_entry_t** wake_send, int* n_send);

/* ---- API concurrentmap (specchio PCodeConcurrentMap, parità Go sync.Map) ----
 * STORE adotta i retain di chiave e valore (il chiamante NON li rilascia dopo).
 * LOAD/LOADORSTORE ritornano una copia CON retain (type 0 se assente); DELETE
 * ritorna l'entry rimossa in MOVE (nessun retain extra). */
gmb_concurrentmap_t* gmb_concurrentmap_create(void);
void gmb_concurrentmap_destroy(gmb_concurrentmap_t* m);
void gmb_concurrentmap_store(gmb_concurrentmap_t* m, gmb_stack_entry_t key, gmb_stack_entry_t val);
gmb_stack_entry_t gmb_concurrentmap_load(gmb_concurrentmap_t* m, gmb_stack_entry_t key);
gmb_stack_entry_t gmb_concurrentmap_delete(gmb_concurrentmap_t* m, gmb_stack_entry_t key);
gmb_stack_entry_t gmb_concurrentmap_load_or_store(gmb_concurrentmap_t* m, gmb_stack_entry_t key,
                                                  gmb_stack_entry_t val);

/* ---- API future ---- */
gmb_future_t* gmb_future_create(void);
void gmb_future_destroy(gmb_future_t* f);
bool gmb_future_register_waiter(gmb_future_t* f, gmb_task_t* t);
void gmb_future_resolve(gmb_future_t* f, gmb_obj_t* value, double num, int is_num, gmb_scheduler_t* s);
void gmb_future_fail(gmb_future_t* f, const char* msg, gmb_scheduler_t* s);
int gmb_future_is_resolved(gmb_future_t* f);

/* ---- Utilities task ---- */
/* Cap massimo dello stack operandi (env GMBASIC_PCODE_MAX_STACK, default 1<<20 —
 * stesso ordine di grandezza del cap di PCodeVm per convergenza differenziale). */
int  gmb_task_stack_max(void);
/* Riserva spazio per `extra` entry (crescita geometrica). 0 = overflow/OOM
 * (imposta t->stack_overflow). */
int  gmb_task_stack_reserve(gmb_task_t* t, int extra);
/* Push che PRENDE IN CARICO il retain già esistente di e (move, non copy).
 * Ritorna 0 ok, -1 overflow (il chiamante DEVE rilasciare e). */
int  gmb_task_push_move(gmb_task_t* t, gmb_stack_entry_t e);
/* Riporta sp a `sp` rilasciando i ref scartati (pop con release). */
void gmb_task_truncate(gmb_task_t* t, int sp);
void gmb_task_push_num(gmb_task_t* t, double v, uint8_t bt);
void gmb_task_push_ref(gmb_task_t* t, gmb_obj_t* o, uint8_t bt);
void gmb_task_push_nil(gmb_task_t* t);
gmb_stack_entry_t gmb_task_pop(gmb_task_t* t);
double gmb_task_pop_num(gmb_task_t* t);
void gmb_task_store_local(gmb_task_t* t, int slot, gmb_stack_entry_t e);
gmb_stack_entry_t gmb_task_load_local(gmb_task_t* t, int slot);
void gmb_task_store_global(gmb_task_t* t, int idx, gmb_stack_entry_t e);
gmb_stack_entry_t gmb_task_load_global(gmb_task_t* t, int idx);
void gmb_task_ensure_globals(gmb_task_t* t, uint32_t n);
void gmb_task_free(gmb_task_t* t);

/* clock ms (epoch) */
int64_t gmb_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GPM_H */
