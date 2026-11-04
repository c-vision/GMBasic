/* ============================================================================
 * gmb_vm_core.h — Core della VM C (dispatcher opcode su task GPM)
 * File: native/gmb_vm_c/gmb_vm_core.h
 * Fase A2 + A2-GPM-MT: il dispatcher opera su gmb_task_t (stack/frame/ip/globals
 * per-task); gmb_vm_t è il runner (chunk + scheduler + hooks di output).
 * ============================================================================ */

#ifndef GMB_VM_CORE_H
#define GMB_VM_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmb_vm_value.h"
#include "gmb_chunk.h"
#include "gmb_gpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Runner: chunk + scheduler + output hooks ---- */
/* SymbolId dei campi STATIC di CLASS è GLOBALE a tutto il programma (ogni variabile/campo/
 * parametro consuma un ID, non solo i campi STATIC) — un cap di 256 veniva superato già da
 * programmi minuscoli (bug scoperto 2026-09-02, ClassesDestructor.bas: SymbolId 526 per un
 * unico campo STATIC in un file di poche classi). OP_LOAD_STATIC_FIELD/OP_STORE_STATIC_FIELD
 * silenziosamente non facevano nulla oltre il cap (guardia `sid < GMB_STATIC_FIELDS_CAP`) — il
 * campo leggeva sempre NIL. Alzato a 65536 (il formato bytecode usa un operando ushort per il
 * SymbolId, quindi 65536 è il vero limite superiore, mai un compromesso arbitrario). A questa
 * dimensione un array INLINE in gmb_vm_t (~1.6MB) è pericoloso: gmb_vm_t viene dichiarato come
 * variabile LOCALE (stack) in più punti, incluso un worker pthread con stack di default
 * (gmb_vm_bsp.c::bsp_render_worker, pthread_create senza pthread_attr_setstacksize) — stack
 * overflow immediato al render di una pagina BSP. Per questo static_fields/static_fields_init
 * sono ora PUNTATORI, allocati sull'HEAP da gmb_vm_init e liberati da gmb_vm_free (vedi
 * gmb_vm_core.c), non più array a dimensione fissa dentro la struct. */
#define GMB_STATIC_FIELDS_CAP 65536
#define GMB_MAX_BREAKPOINTS 256
/* Semi di globali nominati per pagine BSP (REQ/QUERY/FORM/SESSION). */
#define GMB_VM_MAX_BSP_SEEDS 8
/* Dynamic scope per NOME (FORM/QUERY: keyword riservate, mai una DIM — vedi
 * gmb_vm_set_dynamic_scope sotto). */
#define GMB_VM_MAX_DYNAMIC_SCOPE 8

/* Stato del debugger di riga. Il chunk porta la mappa opcode->riga
 * (chunk->lines): a ogni istruzione il tick controlla se la riga corrente
 * è in breakpoint e, in tal caso, mette in pausa il task (GMB_TS_DEBUG_PAUSED)
 * e ritorna GMB_TICK_DEBUG_PAUSE. Il driver (bridge C#) riprende con
 * gmb_vm_run_debug, che riesegue da dove si è fermato. */
typedef struct {
    uint32_t breakpoints[GMB_MAX_BREAKPOINTS];
    int      breakpoint_count;
    /* 1 = esegue un'istruzione poi si ferma (step-into). Resettato dalla run. */
    int      single_step;
    /* Richiesta asincrona di pausa (SIGUSR1 dall'host interattivo). */
    volatile int pause_requested;
    /* Righe da saltare al resume (non ripausare sulla stessa riga del
     * breakpoint appena lasciato). 0 = nessun skip attivo. */
    uint32_t skip_line;
    /* STEP OVER/OUT (0 = inattivo; altrimenti depth+1, così il memset a 0 è "off").
     * step_over: fermati alla riga successiva con depth <= quella di partenza.
     * step_out:  fermati quando depth < quella di partenza (uscita dal frame). */
    int      step_over_depth;
    int      step_out_depth;
    uint32_t step_over_line;
} gmb_vm_debug_t;

typedef struct {
    const gmb_chunk_t* chunk;
    gmb_scheduler_t* scheduler;
    /* static fields (OP_LOAD/STORE_STATIC_FIELD) — heap-allocati da gmb_vm_init, vedi
     * commento su GMB_STATIC_FIELDS_CAP sopra (mai array inline: gmb_vm_t vive anche su stack
     * di worker thread con stack size di default). */
    gmb_stack_entry_t* static_fields;
    uint8_t* static_fields_init;
    pthread_mutex_t static_lock;
    pthread_mutex_t print_lock;       /* atomicità righe PRINT (PendingPrints) */
    /* Cache dei descrittori UDT/classe materializzati (uno per costante del chunk, condiviso da tutte le
     * istanze: i descrittori sono SOLO LETTI — VTable/FieldNames/...). Prima ogni NEW ne rimaterializzava
     * una copia (~10 stringhe/dict per istanza). La cache tiene 1 riferimento; vedi gmb_vm_desc_cached. */
    pthread_mutex_t desc_lock;
    const void** desc_key;
    struct gmb_obj** desc_obj;
    int desc_n, desc_cap;
    /* Output hook (per ora: stdout via printf) */
    /* Hook di output: il PRIMO parametro è `print_ctx` (contesto opaco del chiamante).
     * Serve a rendere la cattura PER-VM: due render concorrenti non si scrivono più nello
     * stesso buffer statico (prima l'unico modo era serializzare tutto con un mutex). */
    void (*print_str)(void* ctx, const char* s, uint32_t len);
    void (*print_num)(void* ctx, double v);
    void (*print_newline)(void* ctx);
    /* Contesto opaco passato agli hook di output (es. buffer accumulatore
     * del bridge in-process gmb_vm_bridge; NULL per il host CLI). */
    void* print_ctx;
    /* Chiamata a procedura UTENTE non presente nel chunk (frammenti a runtime:
     * EVAL compilato in C99, dove le FUNCTION sono C statiche). NULL = non
     * disponibile → OP_CALL_EXTERNAL produce un errore esplicito. */
    int (*external_call)(void* vm, gmb_task_t* task, const char* name,
                         const gmb_val_t* args, int nargs, gmb_val_t* out);
    /* Debugger di riga (NULL = nessun debug, percorso veloce invariato). */
    gmb_vm_debug_t* debug;
/* Task main del debugger, creato da gmb_vm_debug_init e riusato tra i
 * resume (Continue/Step) — NULL finché non è in esecuzione di debug. */
    struct gmb_task* debug_main;
    /* Task main del programma (creato da gmb_vm_run): gli handler HTTP/WebSocket ne
     * CONDIVIDONO le globali per riferimento, così i SUB handler vedono i global del
     * modulo (es. un LIST OF JSON popolato in Main) invece di un array globali vuoto. */
    struct gmb_task* http_main_task;
    /* Contesto TUI (controlli Clipper, syscall 273–290) — gmb_vm_tui.c, lazy. */
    struct gmb_vm_tui* tui;
    /* V5: pool worker ridimensionabile a runtime (SYSTEM.GOMAXPROCS, parità Go
     * runtime.GOMAXPROCS(n) — specchio SetGoMaxProcs/ConfigureProcessors di PCodeVm).
     * Tutto protetto da worker_lock. I thread lanciati (anche ritirati) restano in
     * worker_threads per la join finale; retire_pending = token di ritiro consumati
     * dai worker in cima al loop (shrink); worker_live = thread non ancora usciti. */
    pthread_mutex_t worker_lock;
    pthread_t* worker_threads;
    int worker_threads_cap;
    int worker_threads_count;
    int worker_target;
    int worker_live;
    int retire_pending;
    /* BSP: semi di globali nominati applicati al task main al momento della
     * creazione (pagine .bsp: REQ/QUERY/FORM/SESSION). Ogni entry possiede un
     * retain (rilasciato all'applicazione o in gmb_vm_free). */
    char* bsp_seed_names[GMB_VM_MAX_BSP_SEEDS];
    gmb_stack_entry_t bsp_seed_values[GMB_VM_MAX_BSP_SEEDS];
    int bsp_seed_count;
    /* Dynamic scope per NOME (OP_LOAD_DYNAMIC_SCOPE, parità PCodeProcessor._dynamicScope
     * C#): a differenza di bsp_seed_* sopra (applicati UNA VOLTA al task main alla
     * creazione, poi mai più consultati), questo bag resta vivo per tutta la vita della
     * VM e viene letto per NOME a ogni esecuzione dell'opcode — usato per FORM/QUERY
     * nelle pagine BSP, keyword riservate che non hanno MAI un vero slot globale (non
     * dichiarabili con DIM, quindi mai in chunk->global_names). Ogni entry possiede un
     * retain (rilasciato in gmb_vm_free). */
    char* dynamic_scope_names[GMB_VM_MAX_DYNAMIC_SCOPE];
    gmb_stack_entry_t dynamic_scope_values[GMB_VM_MAX_DYNAMIC_SCOPE];
    int dynamic_scope_count;
    /* Stato delle aree DB/ISAM (spostato qui da static locali di
     * gmb_vm_exec_db_syscall per permettere lo split dello switch per dominio,
     * task 6.5.2): alias → handle per ISAM e DBF + buffer temporaneo schema.
     * Prima: static locali nella funzione (condivise tra tutti i case). */
    int vm_isam_tmp_sids[64];
    int vm_isam_tmp_count;
    char vm_isam_tmp_schema[2048];
    char vm_isam_alias[32][64];
    int64_t vm_isam_alias_handle[32];
    int vm_isam_alias_count;
    char vm_dbf_alias[32][64];
    int64_t vm_dbf_alias_handle[32];
    int vm_dbf_alias_count;
    /* Cache field-symbolId aree ISAM/SPEED (spostate qui da static locali del core
     * per lo split syscall per dominio, task 6.5.2). ISAM_USE/SPEED_USE le popolano;
     * APPEND/PUT/GET le leggono per i campi "__m<sid>". */
    int64_t vm_isam_sid_handle[8];
    int vm_isam_sids[8][64];
    int vm_isam_sid_count[8];
    int64_t vm_speed_field_handle[8];
    int vm_speed_field_sids[8][64];
    char vm_speed_field_names[8][64][64];
    int vm_speed_field_count[8];
    char vm_speed_field_pk[8][64];
    int exit_code;   /* EXIT CODE del programma (PROGRAM_EXIT syscall / FUNCTION Main) */
} gmb_vm_t;

/* Helper condivise per i dispatcher syscall (non-static: usate dai file handler
 * gmb_syscall_*.c estratti dallo switch monolitico, task 6.5.2). */
int vm_entry_str(gmb_stack_entry_t e, char* buf, int cap);
gmb_obj_string_t* gmb_vm_json_stringify(gmb_obj_t* obj);
gmb_obj_t* md_to_vm(gmb_val_t v);
void team_pop_str(gmb_stack_entry_t e, char* buf, int cap);
void rdd_select_area(const gmb_stack_entry_t* args, int argc);
extern int g_vm_host_argc;
extern const char** g_vm_host_argv;
int vm_isam_sid_slot(gmb_vm_t* vm, int64_t h);
int vm_speed_field_slot(gmb_vm_t* vm, int64_t h);
/* Istanza CLASS/UDT di default (no costruttore), campi per NOME da
 * descriptor["FieldNames"]/["FieldTypes"] — vedi gmb_vm_core.c. Usata da OP_REDIM/
 * OP_REDIM_PRESERVE e da CREATE_ARRAY (gmb_builtin.c) per popolare gli elementi di un
 * array di UDT/CLASS con istanze vere invece di gmb_val_nil(). */
gmb_obj_shared_val_t* vm_make_default_instance(gmb_vm_t* vm, int typeIdx);
void push_num_entry(gmb_task_t* t, double v, uint8_t bt);
void push_str_entry(gmb_task_t* t, const char* s);
void push_ref_entry(gmb_task_t* t, gmb_obj_t* o, uint8_t bt);   /* RETAIN: l'oggetto resta anche al chiamante (riferimento in prestito) */
/* STEAL: lo stack PRENDE il riferimento del chiamante (nessun retain). Per oggetti appena creati (rc 1 del
 * creatore) e per riferimenti posseduti gia' tolti dallo stack/da un'entry: evita il leak di 1 rc per push. */
void push_ref_new(gmb_task_t* t, gmb_obj_t* o, uint8_t bt);
void push_nil(gmb_task_t* t);
/* Rilascio DIFFERITO a inizio istruzione successiva: per riferimenti tolti dallo stack i cui dati (const char*) servono ancora. */
void gmb_task_defer_release(gmb_task_t* t, gmb_obj_t* o);
void push_val_entry(gmb_task_t* t, gmb_val_t v);
int vm_dict_to_csv(gmb_vm_t* vm, gmb_obj_t* ref, const char* fields, char* out, int cap);
int vm_dbf_field_index(const char* fieldsCsv, const char* name);
gmb_val_t vm_dict_get_ci(const gmb_dict_t* d, const char* key);
void vm_val_csv(gmb_val_t v, char* out, int cap);
int vm_csv_split(const char* s, char out[][128], int maxn);
/* Dispatcher SysCall DB commerciali (gmb_syscall_db.c) — lookup per codice nella
 * tabella GMB_IMPL, ritorna 0 se gestito, -1 altrimenti (refactor 6.5.2). */
int gmb_vm_exec_db_syscall(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
int gmb_syscall_fileio(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
int gmb_syscall_rdd(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
void vm_error(gmb_task_t* task, const char* msg);
/* Schema a 3 livelli per un errore BASIC di un builtin: TRY/CATCH -> ON ERROR GOTO/GOSUB
 * -> fatale. Da usare al posto di vm_error quando l'errore è semantico (un builtin fallito). */
void gmb_raise_basic_error(gmb_task_t* task, double err_code, const char* msg);
extern gmb_int_t g_vm_file_handles[64];
extern int g_vm_file_content_types[64];
void vm_pop_args_ordered(gmb_task_t* task, gmb_stack_entry_t* out, int argc);
void vm_pop_str(gmb_task_t* task, char* buf, size_t cap);
/* Condivisione dei globali del task main (wrapper di ws_task_share_main_globals,
 * usato dai domini VM in file separati: gmb_vm_signal.c). */
void gmb_vm_task_share_main_globals(gmb_task_t* t, gmb_vm_t* vm);
/* Helper condivise dei builtin (non-static: usate da gmb_builtin.c, refactor 6.5.2). */
typedef struct {
    double result;
    int is_num;
    gmb_obj_t* result_ref;
    uint8_t result_ref_bt;
    /* basic_type del risultato numerico quando DEVE essere un BOOL (es. LEDGER.VALIDATE →
     * CONVERSIONS.CSTR deve rendere "True" e non "-1", parità IL/C#). 0 = GMB_BT_NUMBER. */
    uint8_t num_bt;
} gmb_builtin_out_t;
void vm_builtin_rstr(char* heap, int* is_num, gmb_obj_t** result_ref, uint8_t* result_ref_bt);
const char* vm_builtin_sarg(const gmb_stack_entry_t* a, int argc, int i);
int vm_json_full_parse(const char* js, gmb_val_t* out);
gmb_obj_t* vm_json_plain_obj(gmb_obj_t* src);   /* istanza UDT/CLASS -> dict "pulito" (senza __class__) */
typedef struct gmb_cpu_job {
    gmb_vm_t* vm;
    gmb_task_t* task;
    int kind;
    double* a; int na;
    double* b; int nb;
    int p1;
    int p2;
} gmb_cpu_job_t;
int vm_try_park_cpu(gmb_vm_t* vm, gmb_task_t* task, gmb_cpu_job_t* j);
/* Park generico per job STRINGA bloccanti (AI.BLACKBOARDWAIT/AI.AGENTSWARM, F1 parita' IL/
 * PCodeVM): stesso schema di gmb_cpu_job_t ma per builtin che ricevono/producono char* invece
 * di double[]. kind: 0=BLACKBOARDWAIT(key,timeout_ms), 1=AGENTSWARM(n,prefix,task_str,cc),
 * 2=AI FINETUNE(model,json), 3=AI LORA APPLY(ckpt,json). I kind 2/3 (B4, 2026-09-13) usano
 * model/json e, in caso di errore del processo nativo, settano l'errore del task prima del
 * re-enqueue (vm_error e' una semplice mutazione di campo). */
typedef struct gmb_str_job {
    gmb_vm_t* vm;
    gmb_task_t* task;
    int kind;
    char* key;       /* BLACKBOARDWAIT: chiave */
    int timeout_ms;  /* BLACKBOARDWAIT: timeout */
    double n;        /* AGENTSWARM: numero agenti */
    char* prefix;    /* AGENTSWARM: prefisso nomi agente */
    char* task_str;  /* AGENTSWARM: prompt/task */
    int cc;          /* AGENTSWARM: max concurrency */
    char* model;     /* FINETUNE/LORA APPLY: path modello (o ckpt prefix) */
    char* json;      /* FINETUNE/LORA APPLY: opzioni JSON */
} gmb_str_job_t;
int vm_try_park_str(gmb_vm_t* vm, gmb_task_t* task, gmb_str_job_t* j);
int vm_try_park_sql_exec(gmb_vm_t* vm, gmb_task_t* task, const char* alias, const char* sql, const char* const* params, int n);
typedef struct { gmb_vm_t* vm; gmb_task_t* task; gmb_obj_closure_t* closure; int last_vec_count; } gmb_vm_closure_ctx_t;
typedef struct { gmb_vm_t* vm; gmb_task_t* task; gmb_obj_closure_t* cl; } gmb_closure_ctx_t;
double gmb_vm_closure_f(double x, void* c);
double gmb_vm_closure_f2(double t, double y, void* c);
double gmb_vm_call_closure_sync(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, double arg);

/* Marker di chiamata SINCRONA: crea il frame con return_ip = -2 sul task `t` (gli
 * `nargs` argomenti devono essere già sullo stack). All'OP_RETURN il valore finisce in
 * t->sync_result/sync_result_ref e sync_active torna 0. Usato dal server MCP
 * (gmb_mcp_exec.c) e dal valutatore di espressioni (vm_expr_call). Ritorna 0 ok. */
int gmb_vm_push_sync_frame(gmb_task_t* t, int nargs);

/* Risolve il nome di una procedura nel chunk della vm nell'indirizzo di ingresso.
 * Ritorna -1 se non trovata. */
int gmb_vm_proc_addr(gmb_vm_t* vm, const char* name);
/* Chiama per NOME una procedura del chunk con UN argomento stringa (sync). */
double gmb_vm_call_proc_sync_str(gmb_vm_t* vm, gmb_task_t* task, const char* name, const char* s);
/* Chiamata sincrona a una procedura per NOME con N argomenti numerici (ritorna double). */
double gmb_vm_call_proc_sync_num(gmb_vm_t* vm, gmb_task_t* task, const char* name, const double* vals, int n);
double gmb_vm_call_closure_sync2(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, double a, double b);
double gmb_vm_call_closure_sync_val(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, gmb_val_t a);
double gmb_vm_call_closure_sync3_val(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, gmb_val_t a, gmb_val_t b, gmb_val_t c);
double gmb_vm_call_closure_sync2_val(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, gmb_val_t a, gmb_val_t b);
double gmb_vm_call_closure_sync_list2_vec(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, const double* p, int np, const double* xs, int nx, double* out);
double gmb_vm_call_closure_sync_ll(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, const double* a, int na, const double* b, int nb);
double gmb_vm_call_closure_sync_list(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, const double* vals, int n);
void gmb_vm_call_closure_sync_entry(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, gmb_val_t a);
void gmb_vm_call_closure_sync2_entry(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure, gmb_val_t a, gmb_val_t b);
void gmb_vm_call_closure_sync_entry0(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* closure);
void gmb_vm_ode_fn(double t, const double* y, int yd, double* dydt, void* c);
int gmb_opt_extract_bounds(const gmb_stack_entry_t* arg, double* lo, double* hi, int max);
/* Prototipi ottimizzatori (gmb_opt_*_ctx, definiti in gmb_vm_core.c — esposti per gmb_builtin.c). */
double gmb_opt_newton_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double guess, double tol, int max_iter);
double gmb_opt_bisect_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double lo, double hi, double tol, int max_iter);
double gmb_opt_brent_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double lo, double hi, double tol, int max_iter);
double gmb_opt_golden_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double lo, double hi, double tol, int max_iter);
double gmb_opt_secant_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double x0, double x1, double tol, int max_iter);
void gmb_opt_nm_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double* start, int n, double tol, int max_iter, double* out);
void gmb_opt_bfgs_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double* start, int n, double tol, int max_iter, double* out);
void gmb_opt_cg_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double* start, int n, double tol, int max_iter, double* out);
void gmb_opt_lbfgs_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double* start, int n, double tol, int max_iter, double* out);
void gmb_opt_broyden_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, double* out);
void gmb_opt_lmfit_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x, const double* y, int nx, const double* p0, int np, double* out);
void gmb_opt_lmfit_vec_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x, const double* y, int nx, const double* p0, int np, double* out);
void gmb_opt_de_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* lo, const double* hi, int n, double* out);
double gmb_opt_brentq_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double a, double b);
double gmb_opt_brenth_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double a, double b);
double gmb_opt_ridder_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double a, double b);
double gmb_opt_toms748_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double a, double b);
double gmb_opt_brentmin_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double a, double b);
double gmb_opt_minscalar_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, double a, double b, const char* method);
void gmb_opt_powell_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, double* out);
void gmb_opt_tnc_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, const double* lo, const double* hi, double* out);
void gmb_opt_cobyla_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, const double* lo, const double* hi, double* out);
void gmb_opt_slsqp_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, const double* lo, const double* hi, double* out);
void gmb_opt_minimize_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, const char* method, const double* lo, const double* hi, double* out);
void gmb_opt_fsolve_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, double* out);
void gmb_opt_lstsq_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, const double* lo, const double* hi, double* out);
void gmb_opt_brute_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* lo, const double* hi, int n, int N, double* out);
void gmb_opt_basin_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* x0, int n, int niter, double* out);
void gmb_opt_dual_ctx(gmb_vm_t* vm, gmb_task_t* task, gmb_obj_closure_t* c, const double* lo, const double* hi, int n, int maxiter, double* out);

/* Helper condivise dei builtin (esposte per gmb_builtin.c, refactor 6.5.2). */
gmb_val_t gmb_vm_arg_list(const gmb_stack_entry_t* a);
void gmb_vm_set_dict_result(gmb_val_t r, int* is_num, gmb_obj_t** result_ref, uint8_t* result_ref_bt);
/* Aggiunge a una lista VM un valore FRESCO (creato dal chiamante, non condiviso): una stringa normale diventa una
 * stringa contata della lista e il buffer originale viene liberato; ogni altro valore e' aggiunto cosi' com'e'. */
void vm_list_add_fresh(gmb_list_t* l, gmb_val_t fresh);
void gmb_vm_set_dict_result_owned(gmb_val_t r, int* is_num, gmb_obj_t** result_ref, uint8_t* result_ref_bt);   /* risultato FRESCO: libera il box */
void gmb_vm_set_result_list(double* r, int n, int* is_num, gmb_obj_t** result_ref);
int gmb_vm_spawn_worker(gmb_vm_t* vm);
double* gmb_vm_groups_to_flat(const gmb_stack_entry_t* a, int* k, int** sizes);
double* gmb_vm_jagged_to_flat(const gmb_stack_entry_t* a, int* nrows, int* ncols);
double* gmb_vm_list_to_dbl(const gmb_stack_entry_t* a, int* cnt);
double* gmb_vm_val_list_to_dbl(const gmb_val_t* v, int* cnt);
double* gmb_vm_flatten_dbl(const gmb_stack_entry_t* a, int* cnt);

int gmb_syscall_vectordb(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
int gmb_syscall_sql(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);

/* Codici di ritorno di gmb_vm_tick/gmb_vm_run_debug oltre a 0 (fine/blocco)
 * e -1 (errore): */
#define GMB_TICK_YIELD         1
#define GMB_TICK_DEBUG_PAUSE   2
#define GMB_TICK_GENERATOR_YIELD 3
/* V2: quantum di istruzioni esaurito — il task è ancora RUNNABLE e va
 * riaccodato (come PCodeTickResult.Preempted). */
#define GMB_TICK_PREEMPTED     4

/* ---- API ---- */
void gmb_vm_init(gmb_vm_t* vm, const gmb_chunk_t* chunk);
void gmb_vm_set_http_vm(gmb_vm_t* vm);
gmb_vm_t* gmb_vm_get_http_vm(void);
void gmb_vm_free(gmb_vm_t* vm);

/* BSP: registra un globale nominato da seminare nel task main alla creazione
 * (prima del primo tick). La vm prende in carico nome (strdup) e valore.
 * Ritorna 0 ok, -1 slot pieni. I semi non presenti tra i globali del chunk
 * sono ignorati all'applicazione. */
int gmb_vm_seed_global(gmb_vm_t* vm, const char* name, gmb_val_t value);

/* Dynamic scope per NOME (FORM/QUERY, keyword riservate non dichiarabili con DIM —
 * risolte a runtime da OP_LOAD_DYNAMIC_SCOPE, MAI da un vero slot globale). A
 * differenza di gmb_vm_seed_global (applicato una volta al task main alla creazione),
 * questo valore resta consultabile per TUTTA la vita della VM. Ritorna 0 ok, -1 slot
 * pieni. Parità con PCodeProcessor.SetDynamicScope/GetDynamicScope (C#). */
int gmb_vm_set_dynamic_scope(gmb_vm_t* vm, const char* name, gmb_val_t value);
gmb_stack_entry_t gmb_vm_get_dynamic_scope(gmb_vm_t* vm, const char* name);

/* BSP: ripristina i block hook del net-watcher sullo scheduler della vm data
 * (dopo una run secondaria che li ha sovrascritti). */
void gmb_vm_restore_net_hooks(gmb_vm_t* vm);

/* Esegue UN tick del task dato. Ritorna:
 *   1  tick finito con OP_YIELD (il task va riaccodato),
 *   0  task finito o bloccato (parcheggiato su struttura d'attesa),
 *  -1  errore (task.state = GMB_TS_FINISHED). */
int gmb_vm_tick(gmb_vm_t* vm, gmb_task_t* task);
/* Callback per OP_CALL_EXTERNAL (procedura utente fuori dal chunk). */
void gmb_vm_set_external_call(gmb_vm_t* vm,
    int (*fn)(void*, gmb_task_t*, const char*, const gmb_val_t*, int, gmb_val_t*));
/* Task corrente del thread (NULL fuori da una task). */
gmb_task_t* gmb_vm_tls_task(void);

/* Chiamata SINCRONA a un indirizzo (frame nuovo, marker sync): gli argomenti sono
 * già sullo stack (argc). Usata dal driver EVAL (gmb_vm_eval.c) e dal prestito C. */
int gmb_vm_call_sync(gmb_task_t* task, int addr, int argc, gmb_val_t* out);
/* Campo del record DBF attivo (per pre/post-carico dei nomi-campo in EVAL). */
int gmb_vm_dbf_field_get(const char* name, gmb_val_t* out);
int gmb_vm_dbf_field_set(const char* name, gmb_val_t in);
/* Riversa l'output catturato di un frammento nello staging della task ospite
 * (ordine interleaving con i PRINT del chiamante). */
void gmb_vm_emit_captured(gmb_vm_t* vm, gmb_task_t* task, const char* s);

/* Esegue fino alla fine (single task, no GPM) — per programmi lineari. */
int gmb_vm_run(gmb_vm_t* vm);

/* Esegue il chunk con lo scheduler multi-core (§16): avvia worker thread,
 * accoda il main task, attende la fine. Ritorna 0 se ok, -1 se errore. */
int gmb_vm_run_mt(gmb_vm_t* vm);

/* Esegue in modalità debug (single task): tick per tick, fermandosi quando la
 * riga corrente è in un breakpoint oppure dopo un'istruzione se single_step.
 * Ritorna GMB_TICK_DEBUG_PAUSE quando il task è in pausa (riga corrente
 * leggibile da vm->chunk->lines[task->ip]); 0 a fine/blocco; -1 su errore.
 * Per riprendere basta richiamarla di nuovo (stato del task preservato). */
int gmb_vm_run_debug(gmb_vm_t* vm);

/* Helper debug (usati dal bridge in-process): gestione breakpoint di riga e
 * stato della pausa. */
void     gmb_vm_debug_clear(gmb_vm_t* vm);
int      gmb_vm_debug_add_breakpoint(gmb_vm_t* vm, uint32_t line);
uint32_t gmb_vm_debug_current_line(gmb_vm_t* vm);
/* Stack e lettura variabili per il debug interattivo (host).
 * gmb_vm_debug_stack riempie le righe dalla più interna alla più esterna e ritorna il
 * numero scritto. gmb_vm_debug_read cerca un globale o un locale del frame corrente e
 * formatta il valore in buf; ritorna 1 se trovato. */
int gmb_vm_debug_stack(gmb_vm_t* vm, uint32_t* lines, int max);
int gmb_vm_debug_depth(gmb_vm_t* vm);   /* frame correnti (STEP OVER/OUT) */
int gmb_vm_debug_read(gmb_vm_t* vm, const char* name, char* buf, size_t cap);

/* Handler syscall / builtin estesi (per opcode concorrenza) */
void gmb_vm_exec_syscall(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
void gmb_vm_exec_builtin(gmb_vm_t* vm, gmb_task_t* task, int func_code, int argc);
void gmb_builtin_dispatch(gmb_vm_t* vm, gmb_task_t* task, int func_code, int argc);

/* Scope dinamico (DYNAMIC): scrittura per NOME fuori dal dispatch opcode — usata dal
 * write-through di @GET/READ (vedi gmb_vm_tui.c). Ritorna 1 se scritto. */
int  gmb_vm_dyn_store_name(gmb_task_t* t, const char* name, gmb_stack_entry_t v);
int  gmb_vm_dyn_kind(gmb_task_t* t, const char* name);
int  gmb_vm_dyn_get(gmb_task_t* t, const char* name, gmb_stack_entry_t* out);

/* Accesso opcode helpers (per-task) */
static inline int gmb_vm_read_u8(gmb_vm_t* vm, gmb_task_t* t)
{
    return t->ip < (int)vm->chunk->code_len ? vm->chunk->code[t->ip++] : 0;
}
static inline int gmb_vm_read_u16(gmb_vm_t* vm, gmb_task_t* t)
{
    int hi = gmb_vm_read_u8(vm, t), lo = gmb_vm_read_u8(vm, t);
    return (hi << 8) | lo;
}

/* Formatting esportati (usati da PRINT) */
void gmb_format_value(char* buf, size_t cap, gmb_stack_entry_t e);
void gmb_format_double(char* buf, size_t cap, double d);
/* Variante COMPATTA per la concatenazione di stringhe (numeri senza padding PRINT) */
void gmb_format_value_compact(char* buf, size_t cap, gmb_stack_entry_t e);

/* NEXT(generatore): riprende un generatore (YIELD) o espressione generatore.
 * Ritorna 0 con il prossimo valore in *out, oppure 1 = esaurito (StopIteration). */
int gmb_vm_next_generator(gmb_vm_t* vm, gmb_obj_generator_t* gen, gmb_stack_entry_t* out);

/* MESSAGE: handle del bus (il NOME della variabile, come per CONNECT/SUBSCRIBE/PUBLISH/PING/
 * CLOSE su VM/C) → id numerico del runtime C gmb_msg_*, oppure -1 se il bus non esiste.
 * Usato dal builtin MESSAGE.DELIVERY (gmb_bf_3815): il builtin deve leggere i contatori del
 * bus giusto senza duplicare la tabella dei bus della VM. */
int64_t gmb_vm_msg_bus_id(const char* handle);

#ifdef __cplusplus
}
#endif

#endif /* GMB_VM_CORE_H */
