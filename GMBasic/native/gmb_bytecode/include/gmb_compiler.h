/* native/gmb_runtime_c/include/gmb_compiler.h — compilatore P-Code in C.
 * Compila l'AST (gmb_parse) in un CompiledChunk serializzato (formato A1/GMBC,
 * identico a quello che produce ChunkA1Serializer.cs e che legge gmb_chunk_load
 * in native/gmb_vm_c). Percorso 100% C: .bas → lexer/preprocess/macro → parser → linker → chunk.
 */
#ifndef GMB_COMPILER_H
#define GMB_COMPILER_H

#include "gmb_ast.h"
#include "gmb_parser.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opcode P-Code (specchio di Compiler/OpCode.cs — stessi valori della VM/C). */
/* Massimo numero di argomenti FFI (2026-11: era 8, limite della tabella di
 * dispatch; con libffi il limite è il formato a 16 bit). */
#define GMB_FFI_MAX_ARGS 64  /* limite del compilatore = C#/PCode; la VM/C ha headroom a 64 */

enum {
    GMB_OP_PUSH_CONST = 0, GMB_OP_PUSH_NIL = 1, GMB_OP_PUSH_TRUE = 2, GMB_OP_PUSH_FALSE = 3,
    GMB_OP_POP = 4, GMB_OP_DUP = 5,
    GMB_OP_ADD = 6, GMB_OP_SUBTRACT = 7, GMB_OP_MULTIPLY = 8, GMB_OP_DIVIDE = 9,
    GMB_OP_INT_DIVIDE = 10, GMB_OP_POWER = 11, GMB_OP_NEGATE = 12, GMB_OP_NOT = 13, GMB_OP_MOD = 14,
    GMB_OP_CMP_EQ = 18, GMB_OP_CMP_NE = 19, GMB_OP_CMP_GT = 20, GMB_OP_CMP_GTE = 21,
    GMB_OP_CMP_LT = 22, GMB_OP_CMP_LTE = 23,
    GMB_OP_XOR = 212, GMB_OP_BITAND = 220, GMB_OP_BITOR = 221,
    GMB_OP_MK_ARRAY = 45,
    GMB_OP_INIT_ARRAY = 46,   /* riempie un array GIÀ creato (sotto gli elementi sullo stack):
                               * [array][v1..vN] → pop N, pop array, muta, push array. */
    GMB_OP_LOAD_ELEMENT = 38, GMB_OP_STORE_ELEMENT = 40,
    GMB_OP_LOAD_FIELD = 41, GMB_OP_STORE_FIELD = 42,
    GMB_OP_REDIM = 43,
    /* GMB_OP_MK_DICT = 48: già implementato/usato dalla VM/C (costruisce un dict da N
     * coppie chiave/valore sullo stack); qui riusato con count=0 per creare un dict VUOTO
     * — è il record di base di un'istanza UDT (TYPE...END TYPE, campi nudi, copia valore).
     * Vedi GMB_STMT_CLASS/GMB_STMT_DIM in gmb_compiler.c. */
    GMB_OP_MK_DICT = 48,
    GMB_OP_MK_LIST = 49, /* crea una lista (gmb_obj collection) da N elementi sullo stack
                          * (N=0 → lista vuota). VM OP_MK_LIST. Usato dalla comprehension. */
    GMB_OP_DEFINE_GLOBAL = 24, GMB_OP_LOAD_GLOBAL = 25, GMB_OP_STORE_GLOBAL = 26,
    GMB_OP_LOAD_LOCAL = 27, GMB_OP_STORE_LOCAL = 28,
    GMB_OP_JUMP_FORWARD = 51, GMB_OP_JUMP_BACKWARD = 52,
    GMB_OP_JUMP_IF_TRUE = 53, GMB_OP_JUMP_IF_FALSE = 54,
    GMB_OP_CALL_N = 34,
    GMB_OP_CALL = 55,
    GMB_OP_CALL_BUILTIN = 57,
    /* FFI (DECLARE ... LIB): stesso formato/valori della VM/C (OpCode.cs).
     * OP_CALL_FFI (legacy, contratto all-double): [u16 lib][u16 name][u8 argc].
     * OP_CALL_FFI_T (tipizzata, marshalling completo — vedi gmb_vm_core.c): vedi
     * cc_emit_ffi_call in gmb_compiler.c per l'ordine esatto dei byte. */
    GMB_OP_CALL_FFI = 58,
    GMB_OP_CALL_FFI_T = 211,
    GMB_OP_GOTO = 59, GMB_OP_GOSUB = 60,
    GMB_OP_CLEAR_RETURN = 61, GMB_OP_RETURN = 62, GMB_OP_HALT = 63,
    /* Chiamata a procedura UTENTE fuori dal chunk (frammenti a runtime: EVAL in C99).
     * Operandi: u16 const-index del nome, u8 argc. Il risultato è sullo stack. */
    GMB_OP_CALL_EXTERNAL = 65,
    GMB_OP_GO = 66, GMB_OP_SEND = 67, GMB_OP_RECV = 68,
    GMB_OP_WG_WAIT = 69, GMB_OP_WG_DONE = 70, GMB_OP_SLEEP = 71,
    GMB_OP_READ = 75, GMB_OP_RESTORE = 76,
    GMB_OP_THROW = 77,
    GMB_OP_SET_ERR_HANDLER_GOTO = 78, GMB_OP_SET_ERR_HANDLER_GOSUB = 79, GMB_OP_RESUME = 80,
    /* ON ERROR RESUME NEXT (statement che IMPOSTA l'handler resume-next): distinto da
     * RESUME NEXT (statement che SALTA). Prima erano fusi in GMB_OP_RESUME -> la VM non
     * distingueva le due e, con un handler GOTO precedente attivo, faceva un salto stale. */
    GMB_OP_SET_ERR_HANDLER_RESUME = 84,
    GMB_OP_PUSH_TRY = 82, GMB_OP_POP_TRY = 83,
    GMB_OP_FOR_PREPARE = 110, GMB_OP_FOR_LOOP = 111,
    GMB_OP_PRINT_VALUE = 113, GMB_OP_PRINT_LINE = 114, GMB_OP_PRINT_TAB_ZONE = 115,
    GMB_OP_PRINT_NEWLINE = 116,
    GMB_OP_NEW_WAITGROUP = 118, GMB_OP_NEW_CHANNEL = 119,
    GMB_OP_GO_EXIT = 133, GMB_OP_CHANNEL_CLOSE = 135, GMB_OP_RECV_OK = 136,
    GMB_OP_NEW_MUTEX = 137, GMB_OP_LOCK = 138, GMB_OP_UNLOCK = 139,
    GMB_OP_DEFER = 140, GMB_OP_DEFER_RET = 141,
    GMB_OP_NEW_RWMUTEX = 142, GMB_OP_RLOCK = 143, GMB_OP_RUNLOCK = 144,
    GMB_OP_NEW_ONCE = 145, GMB_OP_ONCE_BEGIN = 146, GMB_OP_ONCE_END = 147,
    GMB_OP_NEW_COND = 150, GMB_OP_COND_WAIT = 151, GMB_OP_COND_SIGNAL = 152, GMB_OP_COND_BROADCAST = 153,
    GMB_OP_NEW_CONCURRENTMAP = 154,
    /* GMB_OP_LOAD_ELEMENT_REF = 157: già implementato/usato dalla VM/C (Compiler.UserTypes.cs
     * ::AssignField, "Arr(i).Campo = valore") per caricare un elemento DICT/LIST della
     * lista come RIFERIMENTO condiviso (gmb_adapter_val_to_stack_ref), non come copia —
     * indispensabile perché un successivo STORE_FIELD scriva davvero sull'elemento in
     * lista invece che su un valore effimero. Mancava solo l'emissione lato compilatore
     * nativo (vedi cc_stmt, GMB_STMT_ASSIGN, ramo "arr(i).Campo = valore"). */
    GMB_OP_LOAD_ELEMENT_REF = 157,
    GMB_OP_NEW_ATOMIC = 148, GMB_OP_NEW_POOL = 149,
    GMB_OP_SYSCALL = 86,
    GMB_OP_GO_SELECT_SETUP = 72,
    GMB_OP_RESOLVE_FUTURE = 74, GMB_OP_GO_FUNCTION = 125,
    GMB_OP_MAKE_BYREF = 30, GMB_OP_MAKE_BYREF_GLOBAL = 31,
    GMB_OP_CALL_INDIRECT = 33,
    GMB_OP_CLOSURE = 107, GMB_OP_LOAD_UPVALUE = 108, GMB_OP_STORE_UPVALUE = 109,
    GMB_OP_NET_CMD = 88, GMB_OP_FETCH = 96, GMB_OP_PUSH_CODE_REF = 97,
    GMB_OP_END_VALID = 128,
    /* Bug corretto (2026-08-31, audit macro/EVAL): la VM/C ha già un motore di
     * valutazione macro funzionante (vm_eval_macro, gmb_vm_core.c) su questi due opcode
     * (stessi valori numerici, mai duplicati qui) — mancava solo l'emissione lato
     * compilatore, vedi cc_expr case "_macro_". */
    GMB_OP_EVAL_MACRO_STMT = 105, GMB_OP_EVAL_MACRO_EXPR = 106,
    GMB_OP_CALL_MEMBER = 101,
    /* DESTRUCTOR delle classi: finalizzano (chiamano il DESTRUCTOR) l'istanza classe/UDT
     * nello slot local (SCOPE) o global (GLOBAL) — specchio di Compiler.cs
     * (EmitFinalizeBodyLocals/EmitGlobalDestructorFinalizers) e della VM (gmb_vm_core.c
     * case OP_FINALIZE_*). Bug NativeCompiler-CLASS-Destructor-Finalize-Missing. */
    GMB_OP_FINALIZE_SCOPE = 102,
    GMB_OP_FINALIZE_GLOBAL = 103,
    /* GMB_OP_NEW_CLASS_INSTANCE = 98: già implementato/usato dalla VM/C per i chunk
     * .NET (CLASS con VTable/costruttore) — qui riusato dal compilatore nativo per
     * `NEW ClassName(args)`. Operando: indice nel constant pool di un descrittore
     * GMB_CCONST_DICT (FieldNames/FieldTypes/VTable/Constructors) — vedi
     * cc_class_build_descriptor in gmb_compiler.c. */
    GMB_OP_NEW_CLASS_INSTANCE = 98,
    /* GMB_OP_CALL_CTOR = 56: già implementato/usato dalla VM/C (chiama un costruttore a un
     * indirizzo ASSOLUTO passato come operando — non un indice di constant pool — con lo
     * stack [args..., istanza, argCount]) per BASE.NEW()/setter di proprietà nei chunk
     * .NET. Qui riusato SOLO per BASE.NEW(args) nei costruttori di classi derivate
     * (INHERITS) — vedi cc_class_register/il case "BASE.NEW" in GMB_EXPR_CALL. */
    GMB_OP_CALL_CTOR = 56,
    /* GMB_OP_FE_NEXT = 156: SOLO nativo (mai emesso dal compilatore .NET — quello usa
     * type-inference statica per distinguere FOR EACH su canale da FOR EACH su lista/
     * dict/array, qui non disponibile). Vedi GMB_STMT_FOR/s->flag in gmb_compiler.c
     * e il case nell'interprete in gmb_vm_core.c per il perché: prima di questo opcode,
     * FOR EACH su QUALSIASI collezione non-canale (il caso comune — {1,2,3}, liste,
     * dict) veniva compilato con GMB_OP_RECV_OK, che tratta INCONDIZIONATAMENTE il
     * valore in cima allo stack come un canale — su una lista/dict questo è type
     * confusion runtime (si accede a memoria di un oggetto con layout diverso come se
     * fosse un gmb_channel_t), risultato non deterministico: a volte somma/valori
     * sbagliati (letture spurie), a volte hang genuino (lock/condvar su puntatori
     * spazzatura mai segnalati). GMB_OP_FE_NEXT ispeziona il kind REALE dell'oggetto a
     * runtime e fa il dispatch corretto (canale invariato, lista/dict con
     * indice+conteggio veri).
     */
    GMB_OP_FE_NEXT = 156,
    GMB_OP_FE_MATRIX_D = 159,
    GMB_OP_NEXT_GENERATOR = 73,
    GMB_OP_NEW_GENERATOR_CALL = 124,
    GMB_OP_LIST_DEDUP = 200,
    GMB_OP_DICT_SORTED = 201,
    GMB_OP_FE_DICT = 202,
    GMB_OP_FE_MATRIX_ROW = 203,
    GMB_OP_NEW = 94,
    GMB_OP_JFILE_OPEN = 204,
    GMB_OP_JFILE_WRITE = 205,
    GMB_OP_JFILE_READ = 206,
    GMB_OP_JFILE_CLOSE = 207,
    GMB_OP_JFILE_SAVE = 208,
    GMB_OP_REC_COPY = 209,
    GMB_OP_ATTACH_UDT_DESC = 210
};

/* Tipi di costante (specchio gmb_chunk.h) */
enum {
    GMB_CCONST_NIL = 0, GMB_CCONST_DOUBLE = 1, GMB_CCONST_STRING = 2,
    GMB_CCONST_DICT = 3, GMB_CCONST_LIST = 4,
    GMB_CCONST_INT = 5, GMB_CCONST_DATATYPE = 6
};

/* Nodo di un descrittore composto (dict/list annidato) nel constant pool — usato SOLO
 * per i descrittori di classe (FieldNames/FieldTypes/VTable/Constructors, GMB_CCONST_DICT/
 * GMB_CCONST_LIST): specchio del formato binario che gmb_chunk_load (VM/C) sa già
 * deserializzare per i chunk .NET (rd_dict in gmb_chunk.c), mai emesso finora dal
 * compilatore nativo. Costruito con le entry già note (es. FieldNames) ma con gli
 * indirizzi di VTable/Constructors PATCHATI in-place dopo che i metodi/costruttore sono
 * stati compilati (vedi cc_class_patch_addr in gmb_compiler.c) — nessuna realloc avviene
 * dopo la costruzione iniziale delle entry, quindi i puntatori diretti alle entry restano
 * validi per il patch successivo. */
typedef struct gmb_cconst_node gmb_cconst_node_t;
typedef struct gmb_cconst_entry {
    int key_is_int; int key_int; char* key_str;
    int val_type; double val_d; char* val_s;
    gmb_cconst_node_t* val_node;   /* valore annidato (DICT/LIST) */
} gmb_cconst_entry_t;
struct gmb_cconst_node { gmb_cconst_entry_t* entries; int n, cap; };

/* Chunk compilato in memoria (prima della serializzazione su file). */
typedef struct gmb_cconst { int type; double d; char* s; gmb_cconst_node_t* tree; } gmb_cconst_t;

typedef struct gmb_compiled_chunk {
    uint8_t* code; uint32_t code_len;
    uint32_t* lines;              /* code_len entry */
    /* constant pool */
    gmb_cconst_t* consts;
    uint32_t consts_len;
    /* DATA pool */
    gmb_cconst_t* data;
    uint32_t data_len;
    int32_t entry_point;
    uint32_t global_count;
    /* procedure name -> addr */
    char** proc_names; uint32_t* proc_addrs; uint32_t proc_count;
    /* addr -> arity */
    uint32_t* arity_addrs; uint32_t* arity_vals; uint32_t arity_count;
    /* global names (indice slot -> nome) */
    char** global_names; uint32_t global_name_count;
    uint32_t local_count;
    /* TryRegions (parità C# TryRegions side-table, lette da OP_PUSH_TRY VM/C):
     * tryIp -> finallyIp/endIp/hasCatch */
    uint32_t* try_ips; uint32_t* try_finally_ips; uint32_t* try_end_ips;
    uint8_t* try_has_catch;
    uint32_t try_region_count;
    /* nomi locali per procedura (S_eval EVAL/macro, coda opzionale A1): per voce
     * indirizzo inizio procedura + nomi normalizzati per slot (NULL = anonimo) */
    uint32_t* ploc_addrs; char*** ploc_names; uint32_t* ploc_counts;
    uint32_t ploc_count;
} gmb_compiled_chunk_t;

/* Compila un programma AST in un chunk serializzato (formato A1/GMBC).
 * Ritorna 0 ok (out_data/out_len allocati da malloc), -1 errore (error allocato). */
int gmb_compile(const gmb_program_t* prog, uint8_t** out_data, size_t* out_len, char** error);

/* ── Compilazione SNIPPET A RUNTIME (EVAL / & / filtri): simboli ESTERNI ────────
 * Compila un frammento BASIC contro le tabelle di simboli del chunk OSPITE (VM/C):
 * i nomi già presenti nell'ospite (globali, locali della procedura corrente,
 * procedure) si risolvono sui suoi SLOT/INDIRIZZI, così il codice può essere
 * eseguito NELLA task corrente (stesso frame, stessi globali, CALL alle proc
 * ospiti). È il gemello C di ExternalGlobalProbe del compilatore C#: UN SOLO
 * compilatore, nessun sottoinsieme e nessun interprete parallelo. */
typedef struct {
    const char* const* global_names;   /* slot -> nome (host) */
    int global_count;
    const char* const* local_names;    /* slot -> nome (procedura corrente host) */
    int local_count;
    const char* const* proc_names;     /* procedure host */
    const uint32_t* proc_addrs;        /* indirizzi ASSOLUTI (già basati) */
    const uint32_t* proc_arities;
    int proc_count;
    uint32_t consts_seed;              /* costanti già occupate dal pool host */
    int code_base;                     /* base assoluta del codice nello spazio merged */
} gmb_snippet_ext_t;

typedef struct {
    gmb_compiled_chunk_t chunk;        /* codice con indirizzi già basati su code_base */
    uint32_t entry;                    /* proc wrapper __GMB_EVAL__ (già basata) */
    /* Firma del wrapper: `SUB __GMB_EVAL__(BYREF p0, ...)`. I primi n_param_slots
     * parametri sono locali dell'OSTITE (slot del frame del chiamante, write-through
     * BYREF); i successivi sono nomi ignoti (campi DBF / variabili nuove), passati
     * come celle BYREF pre/post-caricate dal record. */
    char** param_names;
    int* param_slots;                  /* >=0 locale ospite, -1 nome ignoto */
    int n_params;
    /* 1 = il frammento NON è un'espressione valida ma una SEQUENZA DI STATEMENT
     * (`&("PRINT 42")`): compilato col wrapper SUB, eseguito per effetto. */
    int is_stmt;
} gmb_compiled_snippet_t;

/* 0 = ok. `error` (malloc) su errore. `dimmed` va liberato con
 * gmb_compiled_snippet_free. */
int gmb_compile_snippet(const char* src, int as_stmt, const gmb_snippet_ext_t* ext,
                        gmb_compiled_snippet_t* out, char** error);
void gmb_compiled_snippet_free(gmb_compiled_snippet_t* s);
/* Fase 1 CodeGen C: compila un .bas in chunk A1/GMBC. 0 ok (out heap) / non-zero + diag. */
int gmb_compile_bas_file(const char* path, uint8_t** out_data, size_t* out_len, char** diag);
/* REQUIRE (package manager): ritorna un buffer (malloc) col contenuto degli export .inc/.bas
 * dei pacchetti richiesti nel sorgente, oppure NULL. Da prependere al sorgente.
 *
 * Sintassi riconosciuta: `REQUIRE "nome" [VERSION "vincolo"]` (forma canonica) e la forma
 * storica `REQUIRE "nome@vincolo"`. Senza VERSION si prende la versione PIU' RECENTE (SemVer).
 *
 * project_dir: directory del progetto (tipicamente quella del .bas). Se non NULL e contiene un
 * `gmb-package.lock`, il pacchetto viene PINNATO alla versione registrata nel lock: e' il
 * meccanismo di riproducibilita' (stessa versione su ogni backend e su ogni macchina). Se la
 * versione pinnata non e' in cache -> errore esplicito, mai silenzio. Passa NULL per disattivare
 * il lock (nessun comportamento cambia). */
/* Directory di un path, in buffer interno: usata per il project_dir del lock.
 * Se il path non ha '/', ritorna "." (cwd). Thread-local: il driver e' mono-thread. */
const char* proj_dir_of(const char* path);
char* gmb_pkg_collect_requires(const char* main_src, const char* project_dir);
/* Cache dei pacchetti: la cartella `packages/` del progetto (GMBasic/packages). I driver la
 * impostano all.avvio; eliminato il default ~/.gmb/packages. */
void gmb_pkg_set_cache_dir(const char* dir);

/* 1 se l'ULTIMA chiamata a gmb_pkg_collect_requires ha lasciato un pacchetto non risolto.
 * Il chiamante DEVE controllarlo: la funzione ritorna NULL sia quando non c'e' nulla da
 * iniettare sia quando la risoluzione fallisce, e proseguire senza controllare trasformerebbe
 * un REQUIRE non risolto in un fallimento tardivo e fuorviante. */
int gmb_pkg_require_failed(void);

/* Serializza un chunk compilato (senza ristrutturare). Ritorna 0 ok. */
int gmb_compiled_chunk_serialize(const gmb_compiled_chunk_t* c, uint8_t** out_data, size_t* out_len, char** error);

void gmb_compiled_chunk_free(gmb_compiled_chunk_t* c);

#ifdef __cplusplus
}
#endif

#endif /* GMB_COMPILER_H */
