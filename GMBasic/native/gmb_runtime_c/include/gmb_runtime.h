/* ============================================================================
 * GMBasicAI C99 Native Runtime Library Header
 * File: native/gmb_runtime_c/include/gmb_runtime.h
 * Description: Core C99 data types, memory management, and I/O primitives
 *              for transpiled BASIC applications.
 * ============================================================================ */

#ifndef GMB_RUNTIME_H
#define GMB_RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include "gmb_llm_tok.h"      /* LLM.TOKENS / LLM.DETOKENIZE / LLM.TENSOROUT (ritorno come dato) */
#include "gmb_ai_server.h"
#include "gmb_finetune.h"
#include "gmb_distill.h"   /* Stream D: AI DISTILL (BUILD) */
#include "gmb_community_db.h"
#include "gmb_matrix.h"
#include "gmb_compress.h"

/* XOR LOGICO BASIC (keyword XOR): -1 se i due valori differiscono per truthiness, 0 altrimenti. */
static inline double gmb_logic_xor(double a, double b)
{
    return (((a != 0.0) != (b != 0.0)) ? -1.0 : 0.0);
}

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------
 * Data Types & Representation
 * ---------------------------------------------------------------------------- */

typedef int64_t gmb_int_t;
typedef double gmb_double_t;
typedef bool gmb_bool_t;

typedef struct {
    char* data;
    size_t length;
    int ref_count;
} gmb_string_t;

/* --- Funzione di prima classe (closure) ---
 * `fn` è un puntatore a funzione generico (il wrapper C emesso dal transpiler con
 * firma per-firma); `ctx` è l'ambiente catturato (upvalues) o NULL. La chiamata
 * passa il ctx come primo argomento implicito. Stessa semantica della ClosureData
 * della PCodeVm (OP_CLOSURE + upvalues) adattata al modello C99 statico. */
typedef struct gmb_fn {
    void* fn;
    void* ctx;
} gmb_fn_t;

typedef enum {
    GMB_TYPE_NIL = 0,
    GMB_TYPE_INT = 1,
    GMB_TYPE_DOUBLE = 2,
    GMB_TYPE_BOOL = 3,
    GMB_TYPE_STRING = 4,
    GMB_TYPE_ARRAY = 5,
    GMB_TYPE_OBJECT = 6,
    /* DATETIME: un valore data/ora (NOW()/TODAY()/DATETIME...). Numericamente e' un OADate
     * (v_double, gestito da gmb_val_to_double/to_int e dai confronti numerici), ma con due
     * rese CANONICHE distinte: PRINT -> "dd/mm/yyyy HH:MM:SS", JSON -> stringa ISO-8601. */
    GMB_TYPE_DATETIME = 7,
    /* CURRENCY: numerico con SCALA (campo N/F di un DBF letto come late-binding, decimal C#).
     * Numericamente è v_double (gmb_val_to_double/to_int, aritmetica, confronti); la resa
     * preserva `scale` cifre decimali: PRINT -> "100.00", CSTR -> " 100.00" (spazio iniziale
     * per >= 0, come il decimal PCode/IL). Senza, un campo nudo diventava STRING "100.00"
     * (CSTR senza spazio) o DOUBLE 100 (senza decimali) — divergenza da IL/PCode. */
    GMB_TYPE_CURRENCY = 8
} gmb_type_kind_t;

/* --- Oggetti (istanze di classi/UDT + heap VM) ---
 * Struttura UNIFICATA usata sia dal runtime transpiler sia dalla VM/C (Fase B
 * dell'unificazione runtime). I campi refcount/kind/release_fn servono alla VM
 * (oggetti heap con garbage collection a refcount); i campi data/type_name/dtor
 * servono al transpiler (istanze di classi/UDT). */
enum {
    GMB_OBJ_STRING = 0,
    GMB_OBJ_CHANNEL = 1,
    GMB_OBJ_WAITGROUP = 2,
    GMB_OBJ_FUTURE = 3,
    GMB_OBJ_DICT = 4,
    GMB_OBJ_LIST = 5,
    GMB_OBJ_CLOSURE = 6,
    GMB_OBJ_GENERATOR = 7,
    GMB_OBJ_GRAPH = 8,
    GMB_OBJ_COLLECTION = 9,
    GMB_OBJ_BYREF = 10,   /* box BYREF: riferimento write-through a uno slot (ByRefBox C#) */
    GMB_OBJ_MUTEX = 11,   /* mutex esclusivo cooperativo (parità Go sync.Mutex / PCodeMutex) */
    GMB_OBJ_RWMUTEX = 12, /* lock lettori/scrittore (parità Go sync.RWMutex / PCodeRwMutex) */
    GMB_OBJ_ONCE = 13,    /* esegui-una-volta (parità Go sync.Once / PCodeOnce) */
    GMB_OBJ_ATOMIC = 14,  /* box int64 atomico (parità Go sync/atomic / PCodeAtomic) */
    GMB_OBJ_POOL = 15,    /* pool di valori riusabili (parità Go sync.Pool / PCodePool) */
    GMB_OBJ_COND = 16,    /* variabile di condizione (parità Go sync.Cond / PCodeCond) */
    GMB_OBJ_CONTEXT = 17, /* cancellazione propagata (parità Go context.Context / PCodeContext) */
    GMB_OBJ_CONCURRENTMAP = 18, /* mappa concorrente (parità Go sync.Map / PCodeConcurrentMap) */
};
typedef struct gmb_obj {
    uint32_t refcount;       /* VM: conteggio riferimenti */
    uint8_t  kind;           /* VM: GMB_OBJ_* (string/dict/list/...) o 0 */
    void     (*release_fn)(struct gmb_obj*); /* VM: distruttore refcount */
    void*    data;           /* transpiler: puntatore alla struct dei campi della classe reale */
    const char* type_name;   /* transpiler: nome C della classe reale (dispatch interfaccia) */
    void     (*dtor)(void*); /* transpiler: distruttore opzionale (riceve data = struct campi) */
} gmb_obj_t;

typedef struct gmb_val {
    gmb_type_kind_t kind;
    union {
        gmb_int_t v_int;
        gmb_double_t v_double;
        gmb_bool_t v_bool;
        gmb_string_t v_string;
        void* v_ptr;
    };
    /* Cifre decimali per la resa di GMB_TYPE_CURRENCY (0 = intero). Letto SOLO quando
     * kind==GMB_TYPE_CURRENCY: gli altri costruttori (gmb_make_val_*, literal, .v_double=)
     * lo lasciano non inizializzato senza effetti (mai letto). gmb_make_val_* non-currency
     * NON lo azzera: la guardia è su kind, non sul valore. */
    int8_t scale;
} gmb_val_t;

gmb_val_t gmb_val_nil(void);
/* Ownership dei contenitori (dict, liste, collezioni): un contenitore POSSIEDE i valori OBJECT
 * "contati", cioe' con release_fn != NULL (oggetti heap della VM: lista/dict condivisi, closure,
 * stringhe VM...). retain = refcount++, release = --refcount e, a 0, release_fn.
 * Gli oggetti "in prestito" (release_fn NULL, refcount 0: istanze/box del transpiler  * C99Native, box runtime gmb_make_val_list/dict) e i valori non-OBJECT NON vengono mai toccati:
 * quei mondi non hanno refcount e condividono gli oggetti fra piu' variabili. */
void gmb_val_retain(gmb_val_t v);
void gmb_val_release(gmb_val_t v);
gmb_bool_t gmb_val_equals(gmb_val_t a, gmb_val_t b);
/* Confronto ORDINATO (-1/0/1): stringa se uno dei due è STRINGA, altrimenti numerico. */
gmb_int_t gmb_val_compare(gmb_val_t a, gmb_val_t b);
gmb_val_t gmb_make_val_int(gmb_int_t i);
gmb_val_t gmb_make_val_double(gmb_double_t d);
/* Numerico con SCALA (cifre decimali): reso con `scale` decimali (PRINT "100.00",
 * CSTR " 100.00"). Usato per il campo N/F di un DBF letto late-binding. */
gmb_val_t gmb_make_val_currency(gmb_double_t d, int scale);
/* OADate in un kind DATETIME: PRINT "dd/mm/yyyy HH:MM:SS", JSON ISO-8601. */
gmb_val_t gmb_make_val_datetime(gmb_double_t oa);
gmb_val_t gmb_make_val_bool(gmb_bool_t b);
gmb_val_t gmb_make_val_string(const char* s);
/* Stringa CONTATA (solo VM/C): buffer con header nascosto {refcount} e marcatore in gmb_string_t.ref_count.
 * Nasce "fluttuante" (rc 0, come i box): gmb_val_retain/release (chiamati da add/set/insert/free/clear/remove
 * dei contenitori) la trattengono/rilasciano e a 1→0 il buffer viene liberato; un release a rc 0 NON libera
 * (nessun underflow). Le stringhe normali (transpiler, runtime) non hanno il marcatore: mai toccate. */
gmb_val_t gmb_make_val_string_counted(const char* s);
void gmb_val_discard(gmb_val_t v);   /* il creatore scarta un temporaneo: libera solo una stringa contata a rc 0 */
gmb_val_t gmb_make_val_string_counted_n(const char* s, size_t n);   /* n byte, NUL-terminata internamente */
/* SQL community: campo della riga corrente del result set (cursore SQL.FETCH) */
gmb_val_t gmb_sql_current_val(int64_t rs, const char* col);
gmb_val_t gmb_sql_current_val_idx(int64_t rs, int col);
/* G10: valore tipizzato di una cella a (row, col) arbitrari (SQL.COL: intera colonna). */
gmb_val_t gmb_sql_val_at(int64_t rs, int row, int col);
gmb_obj_t* gmb_obj_new(void* data);
gmb_obj_t* gmb_obj_new_typed(void* data, const char* type_name);
gmb_val_t gmb_make_val_obj(gmb_obj_t* o);
/* Box di un puntatore grezzo (non-gmb_obj_t) in un val OBJECT: ANY ← valore non-oggetto
 * (es. obj = uno gmb_stringbuilder_t*). */
gmb_val_t gmb_make_val_ptr(void* p);
void* gmb_val_as_ptr(gmb_val_t v);
void* gmb_obj_data(gmb_val_t v);
/* Guardia dispatch metodo su nil: se self non è un oggetto valido → errore BASIC 91
 * catturabile (mai SIGSEGV/nil silenzioso). Vedi gmb_core.c. */
gmb_val_t* gmb_obj_require(gmb_val_t* self, long line);
const char* gmb_obj_type(gmb_val_t v);
gmb_val_t gmb_obj_copy_typed(gmb_val_t v, size_t sz);

/* ── Distruttore istanze transpiler (2026-09-03) ──
 * Il transpiler C (C99Native) emette classi come gmb_obj_t con data → struct dei campi;
 * il SUB DESTRUCTOR BASIC è esposto come funzione gmb_user_<Cls>_DESTRUCTOR(gmb_val_t* self).
 * gmb_obj_set_dtor registra la callback di distruzione (firma void(*)(void*) data, generata
 * dal transpiler) sull'oggetto; gmb_obj_run_dtor la invoca UNA volta quando l'istanza termina
 * la vita (fine scope locale, riassegnazione globale, END USING, fine programma). */
void gmb_obj_set_dtor(gmb_val_t v, void (*dtor)(void*));
void gmb_obj_run_dtor(gmb_val_t v);
gmb_bool_t gmb_obj_has_dtor(gmb_val_t v);

/* --- Liste dinamiche (collezioni LIST) --- */
typedef struct gmb_list {
    gmb_val_t* items;
    size_t count;
    size_t capacity;
} gmb_list_t;

gmb_list_t gmb_list_new(void);
gmb_list_t gmb_list_slice(gmb_list_t src, gmb_int_t start, gmb_int_t end);   /* a[start:end] -> nuova lista */
void gmb_list_free(gmb_list_t* l);
gmb_int_t gmb_list_count(gmb_list_t l);
gmb_val_t gmb_make_val_list(gmb_list_t src);
gmb_val_t gmb_make_val_list_take(gmb_list_t* src);   /* adotta lo storage di *src (nessuna copia), lo lascia vuoto */
gmb_val_t gmb_val_index(gmb_val_t v, gmb_val_t idx);
/* Accesso AMBIGUO `x.membro[idx]` vs `x[idx].membro` (il parser nativo fonde le due forme):
 * risolve a runtime in base alla forma di `base` (dict→columnar, lista→row-based). */
gmb_val_t gmb_val_member_or_index(gmb_val_t base, const char* key, gmb_val_t idx);
gmb_list_t gmb_val_as_list(gmb_val_t v);
gmb_list_t gmb_val_foreach_list(gmb_val_t v);
gmb_val_t gmb_val_foreach_entry(gmb_val_t v, gmb_int_t i);   /* dict -> KeyValuePair {Key,Value}; lista -> elemento i */
gmb_val_t gmb_val_foreach_key(gmb_val_t v, gmb_int_t i);     /* dict -> chiave (stringa); lista -> indice (numerico) */
gmb_val_t gmb_val_foreach_value(gmb_val_t v, gmb_int_t i);   /* dict -> valore; lista -> elemento i */
gmb_val_t gmb_make_val_bytes(const uint8_t* data, size_t len);
uint8_t* gmb_val_as_bytes(gmb_val_t v, size_t* len_out);
void gmb_list_add(gmb_list_t* l, gmb_val_t v);
void gmb_list_add_range(gmb_list_t* l, gmb_list_t src);
gmb_val_t gmb_list_get(gmb_list_t l, gmb_int_t i);
void gmb_list_set(gmb_list_t* l, gmb_int_t i, gmb_val_t v);
int gmb_val_index_set(gmb_val_t v, gmb_val_t key, gmb_val_t nv); /* JSON/collezione boxed: lista (indice) o dict (chiave) */
void gmb_list_insert(gmb_list_t* l, gmb_int_t i, gmb_val_t v);
void gmb_list_remove_at(gmb_list_t* l, gmb_int_t i);
gmb_bool_t gmb_list_contains(gmb_list_t l, gmb_val_t v);
gmb_int_t gmb_list_index_of(gmb_list_t l, gmb_val_t v);
void gmb_list_clear(gmb_list_t* l);
gmb_val_t gmb_list_first(gmb_list_t l);
gmb_val_t gmb_list_last(gmb_list_t l);
void gmb_list_sort(gmb_list_t* l);
void gmb_list_reverse(gmb_list_t* l);
void gmb_list_rotate(gmb_list_t* l, gmb_int_t n);
gmb_list_t gmb_list_reverse_copy(gmb_list_t l);
gmb_list_t gmb_list_sort_copy(gmb_list_t l);
gmb_list_t gmb_asort(gmb_list_t l);
gmb_list_t gmb_list_unique_copy(gmb_list_t l);
gmb_list_t gmb_list_flatten_copy(gmb_list_t l);
gmb_string_t gmb_list_join(gmb_list_t l, const char* sep);
/* Join con ", " fra parentesi quadre (`[a, b]`): parità output array IL/C# (AI.PLUGINSLIST). */
gmb_string_t gmb_list_join_bracketed(gmb_list_t l);
gmb_list_t gmb_list_range(gmb_double_t start, gmb_double_t end, gmb_double_t step);
/* AMAP/AFILTER/AREDUCE/AEVAL con callback gmb_fn_t (funzione di prima classe):
 * iterano la lista e invocano fn(ctx, elemento) per ogni elemento. Il callback ha firma
 * gmb_int_t(*)(void* ctx, gmb_int_t elem) — gli elementi numerici sono estratti a int.
 * AMAP → nuova lista dei risultati; AFILTER → elementi dove fn!=0; AREDUCE → accumulato. */
gmb_list_t gmb_list_map(gmb_list_t l, gmb_fn_t fn);
gmb_list_t gmb_list_filter(gmb_list_t l, gmb_fn_t fn);
gmb_double_t gmb_list_reduce(gmb_list_t l, gmb_fn_t fn, gmb_double_t init);
void gmb_list_foreach(gmb_list_t l, gmb_fn_t fn);
/* Converte un array nativo (double/int) in una lista — per passare array a AMAP/AFILTER. */
gmb_list_t gmb_list_from_doubles(const gmb_double_t* arr, gmb_int_t n);
/* G10: intera colonna `col` come lista di valori tipizzati (SQL.COL, backend C99). */
gmb_list_t gmb_sql_col_list(int64_t rs, const char* col);
/* G10: intera colonna `col` come array C di stringhe (alloc + out-len). */
gmb_string_t* gmb_sql_col_stralloc(int64_t rs, const char* col, gmb_int_t* out_len);
gmb_list_t gmb_list_from_ints(const gmb_int_t* arr, gmb_int_t n);
gmb_list_t gmb_list_from_strings(const gmb_string_t* arr, gmb_int_t n);
/* Converte una lista in un array C nativo (con lunghezza compagna) — per assegnare il
 * risultato di AMAP/AFILTER a un array BASIC (DIM x() = AMAP(...)). */
gmb_int_t* gmb_list_to_ints_array(gmb_list_t l, gmb_int_t* len_out);
gmb_double_t* gmb_list_to_doubles_array(gmb_list_t l, gmb_int_t* len_out);
/* Lista di STRINGHE → array C di gmb_string_t (deep-copy dei buffer). Per le comprehension
 * lista con sorgente/elementi stringa (`DIM up = [ n FOR n IN names ]`). */
gmb_string_t* gmb_list_to_strings_array(gmb_list_t l, gmb_int_t* len_out);
void gmb_list_push(gmb_list_t* l, gmb_val_t v);
gmb_val_t gmb_list_pop(gmb_list_t* l);
void gmb_list_enqueue(gmb_list_t* l, gmb_val_t v);
gmb_val_t gmb_list_dequeue(gmb_list_t* l);
gmb_val_t gmb_list_peek(gmb_list_t l);
gmb_int_t gmb_list_resize(gmb_list_t* l, gmb_int_t newlen, int preserve);

/* --- Dizionari (collezioni DICT, chiave stringa) --- */
typedef struct gmb_pair { char* key; gmb_val_t value; } gmb_pair_t;
typedef struct gmb_dict {
    gmb_pair_t* items;
    size_t count;
    size_t capacity;
} gmb_dict_t;

gmb_dict_t gmb_dict_new(void);
void gmb_dict_free(gmb_dict_t* d);
gmb_int_t gmb_dict_count(gmb_dict_t d);
/* F1 Incongruenze (§0bis): MaxPair/MinPair — chiave della voce con valore max/min,
 * unica implementazione per VM/C, C99Native (gmb_list.c). */
gmb_string_t gmb_dict_max_min_key(const gmb_dict_t* d, int is_max);
gmb_double_t gmb_dict_get_double(gmb_dict_t d, const char* key);
gmb_double_t gmb_dict_get_or_nan(gmb_dict_t d, const char* key);
gmb_double_t gmb_list_get_or_nan(gmb_list_t l, gmb_int_t i);
void gmb_dict_set(gmb_dict_t* d, const char* key, gmb_val_t v);
gmb_val_t gmb_dict_get(gmb_dict_t d, const char* key);
/* Lookup CASE-INSENSITIVE: gli identificatori BASIC sono case-insensitive e i campi DBF
 * arrivano MAIUSCOLI (gmb_db_dbf_fields) mentre il programma li scrive nella forma del TYPE
 * (es. `rec.Nome` vs "NOME") → la lettura per nome falliva (0/""), divergenza da IL/PCode
 * (record var dict con comparatore OrdinalIgnoreCase). */
gmb_val_t gmb_dict_get_ci(gmb_dict_t d, const char* key);
gmb_int_t gmb_dict_contains_key_ci(gmb_dict_t d, const char* key);
/* Come gmb_dict_get ma per lettura STRINGA (es. header): chiave assente → "" (parità col
   Dictionary OrdinalIgnoreCase del managed, che per chiave assente dà stringa vuota, NON
   "<nil>"); valore NON-stringa → la sua resa canonica. Usata dall'accessor `.Headers.Get(...)`
   (bug Headers-Get-Divergenza-Backend). */
gmb_string_t gmb_dict_get_str(gmb_dict_t d, const char* key);
/* Ricostruisce il tipo originale di una chiave dict dalla sua rappresentazione stringa
 * interna: numerica (tutta cifre/decimale) → double, altrimenti stringa. Pubblica (era
 * static in gmb_dict.c) per riuso da VM/C (vm_list_fill_keys, gmb_vm_core.c): senza,
 * .Keys() su un DICT OF DOUBLE,... perdeva il tipo numerico delle chiavi (restavano
 * stringhe), causando padding/formattazione PRINT diversa da PCode. */
gmb_val_t gmb_dict_key_val(const char* key);
gmb_list_t gmb_dict_keys(gmb_dict_t d);
gmb_list_t gmb_dict_keys_sorted(gmb_dict_t d);
/* Come sopra ma con ogni chiave ricostruita al suo tipo REALE (gmb_dict_key_val) invece di
 * sempre-stringa — backend C99Native, bug C99Native-Comprehension-DictParam-ChiaviNumeriche
 * (AiWiki), vedi il commento sopra gmb_dict_keys_typed in gmb_dict.c. */
gmb_list_t gmb_dict_keys_typed(gmb_dict_t d);
gmb_list_t gmb_dict_keys_typed_sorted(gmb_dict_t d);
gmb_list_t gmb_dict_values(gmb_dict_t d);
gmb_list_t gmb_dict_values_sorted(gmb_dict_t d);
gmb_dict_t gmb_dict_copy(gmb_dict_t d);
gmb_list_t gmb_dict_extreme(gmb_dict_t* d, int want_max, int remove);
gmb_list_t gmb_dict_to_kvp(gmb_dict_t d);
gmb_list_t gmb_list_shallow_copy(gmb_list_t l);
gmb_list_t gmb_list_deep_copy(gmb_list_t l);
gmb_double_t gmb_list_starts_with(gmb_list_t l, const char* pref);
gmb_list_t gmb_list_find_all(gmb_list_t l, const char* pref);
gmb_int_t gmb_list_unique_count(gmb_list_t l);
gmb_int_t gmb_list_count_of(gmb_list_t l, gmb_val_t v);
gmb_dict_t gmb_bag_counts(gmb_list_t l);
gmb_double_t gmb_list_sum_range(gmb_list_t l, gmb_int_t from_i, gmb_int_t to_i);
void gmb_list_add_at(gmb_list_t* l, gmb_int_t i, gmb_double_t delta);
gmb_list_t gmb_set_union(gmb_list_t a, gmb_list_t b);
gmb_list_t gmb_set_intersect(gmb_list_t a, gmb_list_t b);
gmb_list_t gmb_set_except(gmb_list_t a, gmb_list_t b);
gmb_bool_t gmb_dict_contains_key(gmb_dict_t d, const char* key);
gmb_bool_t gmb_dict_remove(gmb_dict_t* d, const char* key);
void gmb_dict_clear(gmb_dict_t* d);

/* --- PARSE(JSON) / parser JSON condiviso --- */
gmb_val_t gmb_json_parse(const char* js);
void gmb_json_parse_to_dict(const char* js, gmb_dict_t* out);

/* --- ASR (speech-to-text Qwen3-ASR): entry-point della superficie ASR.TRANSCRIBE.
 * Engine nativo in gmb_asr_*.c; `gmb_asr_cmd` ritorna testo malloc'd (liberare con gmb_asr_free). --- */
char* gmb_asr_cmd(const char* model_dir, const char* wav_path, const char* out_path);
void  gmb_asr_free(char* p);
/* F2 (§0bis): serializzatori JSON grezzi PUBBLICI — unica implementazione della
 * serializzazione list/dict->JSON; VM/C (STRINGIFY) e chiamanti handle-based ci
 * delegano. gmb_val_to_json copre il caso dict già boxato in gmb_obj_t/type_name. */
void gmb_json_append_dict(gmb_dict_t* d, char** buf, size_t* cap, size_t* pos);
void gmb_json_append_list(gmb_list_t* l, char** buf, size_t* cap, size_t* pos);
/* Serializza un gmb_dict_t* GREZZO (non boxato in gmb_obj_t) come oggetto JSON dell'INTERO
 * record — parità PCodeVM SpeedRecordToBytes. Unica implementazione condivisa VM/C + C99
 * (vedi gmb_json_append_val per il caso dict già boxato in gmb_obj_t/type_name). */
gmb_string_t gmb_dict_to_json(gmb_dict_t* d);
gmb_string_t gmb_json_path_text(gmb_val_t root, const char* path);
gmb_string_t gmb_json_path_index_text(gmb_val_t root, const char* member, gmb_int_t idx);
gmb_string_t gmb_json_index_member_text(gmb_val_t root, gmb_int_t idx, const char* member);
gmb_val_t gmb_json_index_member_val(gmb_val_t root, gmb_int_t idx, const char* member);
void gmb_json_path_set(gmb_val_t* root, const char* path, gmb_val_t value);
gmb_val_t gmb_json_load_file(const char* path);
void gmb_json_save_val(gmb_val_t v, const char* path);
/* Encoding order-preserving big-endian di un double per chiavi SPEED ordinabili byte-a-byte
 * (parità PCodeVM SpeedSortableDoubleBytes). `out` deve avere almeno 8 byte. */
void gmb_speed_sortable_double_bytes(gmb_double_t d, uint8_t* out);

/* ----------------------------------------------------------------------------
 * String & Memory Management Primitives
 * ---------------------------------------------------------------------------- */

gmb_string_t gmb_string_alloc(const char* str);
gmb_string_t gmb_string_from_bytes(const char* data, gmb_int_t len);

/* SYSTEM.SYS(proprietà) per i backend C: OS/ARCH/UI/VERSION/RUNTIME/CPUCOUNT/MACHINE/USER/CWD +
 * memoria (MEMAVAILABLE/MEMFREE/MEMRECLAIMABLE/MEMTOTAL/MEMUSED, e …MB). Ritorna un buffer statico
 * (da copiare subito in una stringa BASIC). Parità con IlRuntime.Sys (IL/PCode). */
const char* gmb_sys_info(const char* property);
unsigned long long gmb_mem_available_bytes(void);
unsigned long long gmb_mem_total_bytes(void);
gmb_string_t gmb_string_concat(gmb_string_t s1, gmb_string_t s2);
gmb_string_t gmb_bytes_dump(gmb_string_t b);
void gmb_string_free(gmb_string_t* s);
gmb_string_t gmb_int_to_string(gmb_int_t val);
gmb_string_t gmb_double_to_string(gmb_double_t val);
gmb_string_t gmb_str_basic(gmb_double_t val);   /* STR$/CSTR: spazio iniziale se >= 0 */
gmb_int_t gmb_string_to_int(gmb_string_t s);
gmb_double_t gmb_string_to_double(gmb_string_t s);
gmb_bool_t gmb_string_equals(gmb_string_t a, gmb_string_t b);
int gmb_string_compare(gmb_string_t a, gmb_string_t b);   /* ordinale: <0 / 0 / >0 (strcmp) */
gmb_string_t gmb_string_char_at(gmb_string_t s, gmb_int_t i);

/* ----------------------------------------------------------------------------
 * Standard BASIC I/O & System Primitives
 * ---------------------------------------------------------------------------- */

void gmb_print_string(gmb_string_t s);
void gmb_print_text_typed(const char* s);
void gmb_print_int(gmb_int_t val);
void gmb_print_double(gmb_double_t val);
void gmb_print_bool(gmb_bool_t val);
void gmb_print_val(gmb_val_t v);
void gmb_print_newline(void);
/* Staging di stampa per-statement: un PRINT avvolto in begin/end; su errore (raise) lo stage
 * viene scartato (output parziale non emesso) — parità il/pcode/vmc/c99. */
void gmb_print_stage_begin(void);
void gmb_print_stage_end(int commit);
/* PRINT con separatore ',': avanza alla prossima zona di 14 colonne (parity oracolo
 * IlRuntime.PrintTabZone / VM-C OP_PRINT_TAB_ZONE) — stampa gli spazi di padding in base
 * alla colonna corrente tracciata internamente da gmb_console.c. */
void gmb_print_tab_zone(void);
gmb_string_t gmb_val_to_string(gmb_val_t v);
gmb_string_t gmb_cstr_val(gmb_val_t v);   /* CSTR di un val dinamico: spazio se >= 0, bool True/False */

/* ── STRINGBUILDER (builder mutabile di stringhe) ──
 * Handle opaco con buffer NUL-terminato a crescita AMMORTIZZATA: `Append` ripetuti
 * costano O(1) ammortizzato (raddoppio sotto 1 MiB, crescita lineare sopra). Unica
 * sorgente condivisa (§0bis) per C99Native e VM/C. */
typedef struct gmb_stringbuilder gmb_stringbuilder_t;
gmb_stringbuilder_t* gmb_sb_new(size_t initial_cap);
void                 gmb_sb_free(gmb_stringbuilder_t* sb);
void                 gmb_sb_clear(gmb_stringbuilder_t* sb);
int                  gmb_sb_append(gmb_stringbuilder_t* sb, const char* s);
int                  gmb_sb_append_n(gmb_stringbuilder_t* sb, const char* s, size_t n);
int                  gmb_sb_append_char(gmb_stringbuilder_t* sb, char c);
int                  gmb_sb_append_line(gmb_stringbuilder_t* sb, const char* s);
int                  gmb_sb_append_printf(gmb_stringbuilder_t* sb, const char* fmt, ...);
/* AppendFormat posizionale `{i}`: n valori (gmb_val_t) sostituiti ai segnaposto. */
int                  gmb_sb_append_format_vals(gmb_stringbuilder_t* sb, const char* fmt, int n, const gmb_val_t* vals);
int                  gmb_sb_insert(gmb_stringbuilder_t* sb, size_t pos, const char* s);
int                  gmb_sb_replace(gmb_stringbuilder_t* sb, const char* from, const char* to);
int                  gmb_sb_remove(gmb_stringbuilder_t* sb, size_t pos, size_t n);
int                  gmb_sb_assign(gmb_stringbuilder_t* sb, const char* s);
int                  gmb_sb_append_repeat(gmb_stringbuilder_t* sb, const char* s, size_t n);
long                 gmb_sb_index_of(const gmb_stringbuilder_t* sb, const char* needle);
void                 gmb_sb_reverse(gmb_stringbuilder_t* sb);
void                 gmb_sb_trim(gmb_stringbuilder_t* sb);
const char*          gmb_sb_data(const gmb_stringbuilder_t* sb);
size_t               gmb_sb_length(const gmb_stringbuilder_t* sb);
size_t               gmb_sb_capacity(const gmb_stringbuilder_t* sb);
gmb_string_t         gmb_sb_to_string(const gmb_stringbuilder_t* sb);
gmb_string_t gmb_val_typename(gmb_val_t v);
/* TYPEINFO / TYPEINFO.HAS (reflection MVP, parità IlRuntime.TypeInfo/TypeInfoHas). */
gmb_string_t gmb_typeinfo(gmb_val_t v);
gmb_string_t gmb_typeinfo_val(gmb_val_t v);   /* TYPEINFO semantica IlRuntime.TypeInfo (DICT/LIST OF X) */
gmb_int_t    gmb_typeinfo_has(gmb_val_t v);
const char* gmb_valtype_letter(int kind);
gmb_string_t gmb_val_valtype(gmb_val_t v);
gmb_string_t gmb_val_to_json(gmb_val_t v);
/* Escape JSON e float shortest-round-trip (parità System.Text.Json) — condivisi (§0bis). */
size_t gmb_json_escape(char* o, size_t cap, const char* s);
int gmb_json_float_str(char* out, size_t cap, double v);
/* Testo JSON di un val: se il val è una STRINGA è già testo JSON (contenuto, NON virgolettato);
 * altrimenti serializza (dict/lista/UDT → oggetto/array). Usata da JSON.TOOBJECT. */
gmb_string_t gmb_val_json_text(gmb_val_t v);
/* Display per PRINT (dict → [[k,v],...]; list → [e1,e2,...]) — parità IL/VM-C. */
gmb_string_t gmb_val_to_display(gmb_val_t v);
gmb_string_t gmb_json_double_array(const gmb_double_t* items, gmb_int_t n);
gmb_string_t gmb_json_string_array(const gmb_string_t* items, gmb_int_t n);
gmb_string_t gmb_val_ledger_item(gmb_val_t v);
int gmb_csv_value(char* out, int cap, int* pos, gmb_val_t v);
int gmb_csv_field(const char* csv, int idx, char* out, int cap);

/* ----------------------------------------------------------------------------
 * Error handling (ON ERROR GOTO/RESUME, TRY/CATCH) — error state per-thread.
 * gmb_raise_error fa longjmp all'handler registrato (se attivo), altrimenti termina.
 * Codici BASIC: 11 = Division by zero, 9 = Subscript out of range, ecc.
 * ---------------------------------------------------------------------------- */
int gmb_err_number(void);
const char* gmb_err_description(void);
long gmb_err_line(void);
jmp_buf* gmb_err_set_handler(jmp_buf* buf);
void gmb_raise_error(int number, const char* description, long line);
/* Descrizione canonica di un codice d'errore ("" se sconosciuto). Usata da gmb_raise_error
 * quando la descrizione è vuota e dai backend (VM/C) per popolare ERR.Description. */
const char* gmb_err_default_message(int code);
/* DATA/READ/RESTORE: tabella dati globale con cursore (parità IlRuntime). */
void gmb_data_begin(const gmb_val_t* items, gmb_int_t n);
void gmb_data_restore(gmb_int_t pos);
gmb_double_t gmb_data_read_num(void);
gmb_string_t gmb_data_read_str(void);
double gmb_safe_div(double a, double b, long line);
double gmb_safe_fmod(double a, double b, long line);
double gmb_safe_int_div(double a, double b, long line);
gmb_int_t gmb_safe_index(gmb_int_t idx, gmb_int_t len, long line);
/* RESUME NEXT: il codegen registra con gmb_err_set_resume un jmp_buf per OGNI istruzione
 * che può fallire; RESUME NEXT fa longjmp a quello dell'ULTIMA istruzione fallita,
 * atterrando DOPO di essa (come task->ip della VM). */
void gmb_err_set_resume(jmp_buf* buf);
jmp_buf* gmb_err_resume_jmp(void);
void gmb_err_resume_next(void);
gmb_double_t gmb_val_to_double(gmb_val_t v);
gmb_int_t gmb_val_to_int(gmb_val_t v);
void* gmb_udt_box(const void* src, size_t size, const char* type_name);

/* --- WORKFLOW (definizione/step/execute + STATUS/STEPS) --- */
void gmb_workflow_begin(const char* name);
void gmb_workflow_step(const char* name, const char* action);
void gmb_workflow_step_ex(const char* name, const char* action, int retry, int timeoutMs);
void gmb_workflow_end(void);
void gmb_workflow_execute(const char* name);
gmb_string_t gmb_workflow_list(void);
gmb_string_t gmb_workflow_status(const char* name);
gmb_string_t gmb_workflow_steps(const char* name);
/* Superficie webapp /api/workflows* (porting 2026-09-30) — gli equivalenti C# che
 * mancavano. Tutte ritornano malloc (char*) o 0/1 e riusano il registro sopra.
 *   BEGIN_CHECKED  : 0 se è già aperta una definizione (→ HTTP 409), 1 se aperta;
 *                    un omonimo esistente viene SOSTITUITO (parità Dictionary C#).
 *   EXISTS         : 1 se registrato (in definizione compresa).
 *   ADD_STEP_BY_NAME: 1 ok / 0 se wf assente, step vuoto o wf in costruzione (→ 404).
 *   DELETE         : 1 se rimosso / 0 se assente o in costruzione (→ 404).
 *   EXECUTE_OK     : 1 eseguito / 0 se assente o in costruzione (→ HTTP 500 con ok:false).
 *   *_JSON         : payload gia' serializzato (escape incluso), malloc, o NULL se assente. */
int  gmb_workflow_begin_checked(const char* name);
int  gmb_workflow_exists(const char* name);
int  gmb_workflow_add_step_by_name(const char* wf, const char* step, const char* action, int retry, int timeoutMs);
int  gmb_workflow_delete(const char* name);
int  gmb_workflow_execute_ok(const char* name);
char* gmb_workflow_list_json(void);
char* gmb_workflow_summary_json(const char* name);
char* gmb_workflow_steps_detail_json(const char* name);
char* gmb_wf_log_json(const char* name);

void gmb_cls(void);
/* CLS [colore]: grafica → riempie la pagina col colore; console → sfondo+clear. */
void gmb_cls_color(int color);
void gmb_locate(int row, int col);

/* Modalità screen TUI (parità IL su C99Native): attivata da WINDOW DEFINE; da quel momento
 * PRINT/LOCATE/CLS scrivono nel buffer screen (window-relative) e lo rendono in ANSI. */
void gmb_tui_screen_enable(void);
/* CONSOLE n: modalita' console (0=SDL,1=C64,2=ANSI,3=SYSTEM). */
void gmb_console_select(int n);
int  gmb_console_mode_get(void);
void gmb_console_redraw(void);
/* C64: remap PETSCII e predicato vertical-fill (porting ScreenConfig/Atlas). */
int  gmb_c64_remap(int cp);
int  gmb_c64_is_vfill(int cp);
/* Glifo stirato in verticale a tutta la cella (C64 vertical-fill). */
void gmb_gfx_draw_glyph_vfill(int cp, int x, int y, int cw, int ch, uint32_t color);
int  gmb_tui_screen_active(void);
void gmb_tui_screen_flush(void);

/* CPRINT: forza la stampa su console (stdout), ignorando TUI/gfx. */
void gmb_print_force_console(int on);
void gmb_set_color(int fg, int bg);
/* COLOR <specifica Clipper> ("W+/B"): risolve la coppia fg/bg in VGA16 e applica
 * (usata quando il colore è una STRINGA invece di un indice). */
void gmb_set_color_spec(const char* spec);
void gmb_color_reset(void);
void gmb_sleep_ms(int ms);
/* TICK <fps>: frame limiter + pompa present (thread UI). */
void gmb_tick_pace(double fps);
/* Present del frame dal loop TICK (main-thread-safe). */
void gmb_gfx_present_frame(void);
int64_t gmb_ticks_ms(void);
/* INKEY$ (grafica: legge GMB_TUI_SCRIPT; console: stringa vuota). */
gmb_string_t gmb_console_inkey(void);
gmb_string_t gmb_console_message(const char* value); /* MESSAGE(text?) — getter/setter, non stampa */
void gmb_console_palette_color(int idx, int* r, int* g, int* b); /* VGA16[idx] */
gmb_list_t gmb_console_get_cursor_pos(void);     /* CONSOLE.GETCURSORPOS → [0,0] */
gmb_list_t gmb_console_get_text_metrics(void);   /* CONSOLE.GETTEXTMETRICS → [80,24] */
gmb_list_t gmb_console_get_palette_color(int idx); /* CONSOLE.GETPALETTECOLOR → [r,g,b] */
double gmb_console_alert(const char* msg);   /* CONSOLE.ALERT → 0 (headless, nessun render) */
double gmb_console_confirm(const char* msg); /* CONSOLE.CONFIRM → 0 (headless, nessun render) */
long long gmb_console_menu(const char* title, const char* items_csv); /* MENU modale: indice scelto o -1 (via UNICA controllo C) */
const char* gmb_console_input(const char* prompt);                    /* INPUT modale: testo inserito (via UNICA controllo C) */
long long gmb_console_toggle(const char* title, const char* prompt, int initial);
double gmb_console_slider(const char* title, double mn, double mx, double initial);
long long gmb_console_spinner(const char* title, const char* items_csv);
const char* gmb_console_filedialog(const char* title, const char* pattern, int save);
const char* gmb_console_colorpicker(const char* title, const char* defaultColor);
const char* gmb_console_datepicker(const char* title, const char* defaultDate);
void gmb_console_msgbox(const char* title, const char* message);
/* Clipper @SAY — via unica controlli (Label); coordinate 1-based. @GET/READ: usare gmb_read_*. */
void gmb_at_say(int row, int col, const char* text);
/* ALERT completo (parità AlertBox.cs.Show): pulsanti/align/width/border/colori; ritorna 1..N o 0. */
int gmb_console_alert_ex(const char* msg, const char* b0, const char* b1, const char* b2,
                         int align, int width, int border,
                         int has_fg, int fr, int fg, int fb, int has_bg, int br, int bg, int bb);
/* Tasto premuto con timeout (ms; <=0 = attesa finché c'è input). Legge prima la
 * typeahead scriptata GMB_TUI_SCRIPT (gate CI), poi l'input reale del terminale.
 * Ritorna stringa vuota se scaduto/nessun tasto. */
gmb_string_t gmb_console_readkey(int timeout_ms);
/* Tasto NON bloccante (WAITKEY/GET park-based, VM/C + C99 GPM): decodifica un evento
 * completo (typeahead → script CI → terminale) SOLO se già pronto.
 * Ritorna: 1 = tasto in *out; 0 = nessun tasto ora ma terminale reale (il task GPM
 * parcheggia e riprova); -1 = nessun input definitivo (stdin non-tty/CI) — il chiamante
 * tratta come stringa vuota finale. Mai spin-wait. */
int gmb_console_readkey_nb(gmb_string_t* out);
/* INPUT/LINE INPUT: legge una riga dal terminale (o dalla typeahead scriptata
 * GMB_TUI_SCRIPT), accumulando fino a ENTER. */
gmb_string_t gmb_console_readline(void);
/* CURSOR ON/OFF (ANSI show/hide cursor, portabile). */
void gmb_console_cursor(int on);
/* CONSOLE.CAPTION: titolo finestra terminale (OSC 0; Windows usa SetConsoleTitle). */
void gmb_console_set_title(const char* title);
/* SAVE/RESTORE SCREEN: alternate screen buffer ANSI (?47h / ?47l). */
void gmb_console_save_screen(void);
void gmb_console_restore_screen(void);
/* SAVE/RESTORE SCREEN con nome (parità C# SaveScreenBuffer): snapshot nominati del
 * buffer modale (vuoto se mai creato: headless-safe, mai output). */
void gmb_console_savescreen_named(const char* name);
void gmb_console_restorescreen_named(const char* name);
/* SAVE/RESTORE SCREEN polimorfico (parità C# SaveScreenBuffer): buffer modale (TUI/
 * finestra) → snapshot nominato; altrimenti schermo grafico attivo → PNG su disco
 * (gmb_gfx_save_screen_png); altrimenti console headless → snapshot nominato.
 * Le varianti forzate CSAVESCREEN/CRESTORESCREEN restano su *_named. */
void gmb_console_savescreen_dispatch(const char* name);
void gmb_console_restorescreen_dispatch(const char* name);
/* CURSOR SHAPE: BLOCK/UNDERLINE/BAR (ANSI DECSCUSR, portabile). */
void gmb_console_cursor_shape(const char* shape);
/* CLIPBOARD (CLIP GET/SET/CLEAR, parità 4 backend gruppo Console): su macOS usa
 * pbpaste/pbcopy; altrove GET ritorna "" e SET/CLEAR sono no-op (mai crash).
 * Unica implementazione condivisa per VM/C, C99Native (§0bis). */
gmb_string_t gmb_clipboard_get(void);
void gmb_clipboard_set(const char* text);
void gmb_clipboard_clear(void);
int  gmb_clipboard_get_buf(char* out, int maxlen);
/* SET <attr> <value>: opzioni di sessione Clipper (BELL/CONFIRM/CURSOR/CENTURY/
 * THEME/DATEFORMAT/WRAP/ESCAPE/...). Mantiene lo stato in variabili statiche. */
void gmb_console_set_clipper(int subCode, double value, const char* sval);
/* CONSOLE.SENDKEY "testo": inietta la stringa nella typeahead TUI globale. */
void gmb_console_sendkey(const char* keys);
/* IConsole/SdlConsole (SdlConsole.cs) — primitive mancanti */
void gmb_console_beep(void);
int gmb_console_buffer_width(void);
int gmb_console_buffer_height(void);
void gmb_console_set_cursor_position(int x, int y);
void gmb_console_reset_color(void);
/* IConsole streams (IConsole.cs:74-107) — TextReader/TextWriter come callback */
typedef struct gmb_text_reader {
    int (*read_line)(void* ctx, char* out, int cap);
    int (*read_char)(void* ctx);
    void* ctx;
} gmb_text_reader_t;
typedef struct gmb_text_writer {
    void (*write)(void* ctx, const char* s, int len);
    void (*flush)(void* ctx);
    void* ctx;
} gmb_text_writer_t;
void gmb_console_set_in(gmb_text_reader_t r);
void gmb_console_set_out(gmb_text_writer_t w);
void gmb_console_set_error(gmb_text_writer_t w);
int gmb_console_in_read_line(char* out, int cap);
int gmb_console_in_read_char(void);
int gmb_console_out_write(const char* s);
void gmb_console_out_write_line(const char* s);
void gmb_console_out_flush(void);
int gmb_console_err_write(const char* s);
void gmb_console_err_flush(void);
/* IConsole state (IConsole.cs:111-144) */
void gmb_console_set_buffer_size(int w, int h);
void gmb_console_set_window_size(int w, int h);
void gmb_console_set_window_position(int left, int top);
void gmb_console_set_scroll_region(int top, int bottom);
int gmb_console_is_dirty(void);
void gmb_console_mark_clean(void);
int gmb_console_window_left(void);
int gmb_console_window_top(void);
/* JoyManager (common/CommonGraphics/Consoles/JoyManager.cs:103) — SDL joystick guardato, headless no-op */
void gmb_joy_init(void);
void gmb_joy_refresh(void);
int gmb_joy_count(void);
int gmb_joy_is_present(int id);
long gmb_joy_get_state(int id);
const char* gmb_joy_get_name(int id);
void gmb_joy_dispose(void);
/* ScreenManager (common/CommonGraphics/Screens/Manager.cs:90) — display modes SDL-guarded */
typedef struct gmb_display_mode_info {
    int width, height, refresh_rate;
    char pixel_format[64];
    int display_index;
} gmb_display_mode_info_t;
int gmb_screens_available_modes(gmb_display_mode_info_t* out, int cap);
/* SdlManager (common/CommonGraphics/Consoles/SdlManager.cs:131) — SDL_Init + PollEvents + Quit */
int gmb_sdl_manager_is_initialized(void);
void gmb_sdl_manager_init(void);
int gmb_sdl_manager_poll_events(void);
void gmb_sdl_manager_quit(void);
void gmb_sdl_manager_on_quit(void (*cb)(int, void*), void* ud);
void gmb_sdl_manager_on_keyboard(void (*cb)(int, void*), void* ud);
void gmb_sdl_manager_on_window(void (*cb)(int, void*), void* ud);
void gmb_sdl_manager_on_mouse_button(void (*cb)(int, void*), void* ud);
void gmb_sdl_manager_on_mouse_motion(void (*cb)(int, void*), void* ud);
void gmb_sdl_manager_on_text_input(void (*cb)(int, void*), void* ud);
void gmb_sdl_manager_on_other(void (*cb)(int, void*), void* ud);
void gmb_sdl_manager_remove_quit(void (*cb)(int, void*));
void gmb_sdl_manager_remove_keyboard(void (*cb)(int, void*));

/* ----------------------------------------------------------------------------
 * File system (KILL / MKDIR / RMDIR / FILEEXISTS)
 * ---------------------------------------------------------------------------- */
gmb_bool_t gmb_file_exists(const char* path);
gmb_bool_t gmb_dir_exists(gmb_string_t path);
gmb_int_t gmb_file_kill(gmb_string_t path);
gmb_int_t gmb_file_mkdir(gmb_string_t path);
gmb_int_t gmb_file_rmdir(gmb_string_t path);
gmb_int_t gmb_file_rename(gmb_string_t old_path, gmb_string_t new_path);
gmb_int_t gmb_file_move(gmb_string_t src, gmb_string_t dst);
gmb_int_t gmb_file_eof(gmb_int_t handle);
char* gmb_dir_list(const char* path);
gmb_list_t gmb_fs_dir(gmb_string_t mask);
gmb_list_t gmb_fs_dir_filtered(gmb_string_t mask, gmb_fn_t pred);
gmb_double_t gmb_fs_folder_size(gmb_string_t path);
gmb_string_t gmb_fs_normalize_path(gmb_string_t path);
gmb_string_t gmb_fs_dir_name(gmb_string_t path);
int gmb_fs_chdir(gmb_string_t path);
gmb_int_t gmb_file_rmdir_recursive(gmb_string_t path);
gmb_int_t gmb_fs_copy_directory(gmb_string_t src, gmb_string_t dst, gmb_int_t recursive, gmb_int_t overwrite);
gmb_int_t gmb_fs_copy_directory(gmb_string_t src, gmb_string_t dst, gmb_int_t recursive, gmb_int_t overwrite);
gmb_int_t gmb_file_copy(const char* src, const char* dst);
int gmb_count_lines_filtered(const char* path, gmb_fn_t pred);
double gmb_fs_mtime(const char* path);
double gmb_fs_ctime(const char* path);
double gmb_fs_atime(const char* path);
char* gmb_fs_cwd(void);
int gmb_fs_set_cwd(const char* path);
gmb_int_t gmb_file_open(gmb_string_t path, const char* mode, gmb_int_t* handle_out);   /* onora *handle_out se libero, altrimenti alloca >= 1 */
gmb_int_t gmb_file_open_auto(gmb_string_t path, const char* mode, gmb_int_t* handle_out); /* alloca sempre il primo libero >= 1 (FCREATE/FOPEN) */
/* FREEFILE: primo numero di file libero (1-based), stessa numerazione di gmb_file_open. */
gmb_int_t gmb_file_free_file(void);
gmb_int_t gmb_file_write_string(gmb_int_t handle, gmb_string_t s);
gmb_int_t gmb_file_write_newline(gmb_int_t handle);
gmb_int_t gmb_file_read_line(gmb_int_t handle, gmb_string_t* out);
gmb_int_t gmb_file_read_line_val(gmb_int_t handle, gmb_val_t* out);
gmb_string_t* gmb_file_read_csv_line(gmb_int_t handle, gmb_int_t* count_out);
gmb_val_t gmb_file_read_json(gmb_int_t handle);   /* INPUT # su canale JSON: resto del file -> dict/lista */
/* Campi CSV canonici WRITE # (parità IlRuntime.WriteField/PCode): stringa quotata con
 * "" escape; double -1/0 → TRUE/FALSE. */
gmb_string_t gmb_csv_quote(gmb_string_t s);
gmb_string_t gmb_csv_double(gmb_double_t v);
gmb_int_t gmb_file_close(gmb_int_t handle);
/* FERROR/READALL*: stato errore + liste (§0bis: la logica vive nel runtime condiviso). */
int gmb_file_error(void);
void gmb_file_error_set(int e);
gmb_string_t gmb_file_read_n(gmb_int_t handle, gmb_int_t count);
gmb_string_t gmb_file_read_line_str(gmb_int_t handle);
gmb_list_t gmb_read_all_lines_list(const char* path);
gmb_list_t gmb_read_all_bytes_list(const char* path);
gmb_string_t gmb_read_file_text(const char* path);
gmb_int_t gmb_file_seek(gmb_int_t handle, long offset, int origin);
long gmb_file_tell(gmb_int_t handle);
long gmb_file_length(gmb_int_t handle);
gmb_int_t gmb_file_read_bytes(gmb_int_t handle, void* out, size_t n);
gmb_int_t gmb_file_write_bytes(gmb_int_t handle, const void* src, size_t n);
gmb_int_t gmb_file_write_int16(gmb_int_t handle, short v);
gmb_int_t gmb_file_write_int32(gmb_int_t handle, int v);
gmb_int_t gmb_file_write_int64(gmb_int_t handle, long long v);
gmb_int_t gmb_file_write_float(gmb_int_t handle, float v);
gmb_int_t gmb_file_write_double(gmb_int_t handle, double v);
gmb_int_t gmb_file_write_bool(gmb_int_t handle, gmb_bool_t v);
gmb_int_t gmb_file_write_string_bin(gmb_int_t handle, gmb_string_t s);
/* PUT #n, , value — byte canonici (STRING raw, LIST per-elemento, altro ToString): unica
 * sorgente per il gestore PUT_FILE della VM/C e l'emitter st_put del C99Native. */
gmb_int_t gmb_file_put_val(gmb_int_t handle, gmb_val_t v);
/* RANDOM (OPEN ... FOR RANDOM LEN=n): PUT/GET #n, rec, <UDT> a record fissi, layout canonico SIZEOF
 * (vedi gmb_file.c). desc = un token per campo (t y Y i I f l d c sN); rec<=0 = posizione corrente.
 * Ritorno: 0 ok, -1 handle non RANDOM, -2 layout > LEN. */
gmb_int_t gmb_file_set_random(gmb_int_t handle, gmb_int_t reclen);
gmb_int_t gmb_file_is_random(gmb_int_t handle);
gmb_int_t gmb_file_record_size(const char* desc);
gmb_int_t gmb_file_put_record(gmb_int_t handle, gmb_int_t rec, const char* desc, const gmb_val_t* fields, int nfields);
gmb_int_t gmb_file_get_record(gmb_int_t handle, gmb_int_t rec, const char* desc, gmb_val_t* fields, int nfields);
/* PUT/GET di un array di BYTE su file BINARY (1 byte per elemento; pos>0 = offset pos-1). */
gmb_int_t gmb_file_put_bytes_d(gmb_int_t handle, gmb_int_t pos, const gmb_double_t* a, gmb_int_t n);
gmb_int_t gmb_file_get_bytes_d(gmb_int_t handle, gmb_int_t pos, gmb_double_t* a, gmb_int_t n);
gmb_int_t gmb_file_read_int16(gmb_int_t handle, short* v);
gmb_int_t gmb_file_read_int32(gmb_int_t handle, int* v);
gmb_int_t gmb_file_read_int64(gmb_int_t handle, long long* v);
gmb_int_t gmb_file_read_float(gmb_int_t handle, float* v);
gmb_int_t gmb_file_read_double(gmb_int_t handle, double* v);
gmb_int_t gmb_file_read_bool(gmb_int_t handle, gmb_bool_t* v);
gmb_int_t gmb_file_read_string_bin(gmb_int_t handle, gmb_string_t* out);
gmb_list_t gmb_val_dict_keys(gmb_val_t v);
/* Collezioni: nome canonico (TYPENAME) e lettera Clipper (VALTYPE) dal typeCode BASIC.
 * Implementate in gmb_collections.c; dichiarate qui perché il C generato include questo header. */
const char* gmb_collection_type_name(int type_code);
const char* gmb_collection_valtype_letter(int type_code);
gmb_dict_t gmb_val_as_dict(gmb_val_t v);
gmb_val_t gmb_make_val_dict(gmb_dict_t src);
gmb_val_t gmb_make_val_dict_take(gmb_dict_t* src);   /* adotta lo storage di *src (nessuna copia), lo lascia vuoto */
gmb_val_t gmb_val_tuple(const gmb_val_t* items, gmb_int_t n);
int gmb_val_dict_set(gmb_val_t v, const char* key, gmb_val_t value);
gmb_val_t gmb_val_dict_get(gmb_val_t v, const char* key);
int gmb_val_dict_remove(gmb_val_t v, const char* key);
int gmb_val_dict_clear(gmb_val_t v);
gmb_string_t* gmb_val_dict_keys_array(gmb_val_t v, gmb_int_t* len_out);

/* --- Builtin stringhe (StringUtils C# / builtin BASIC) --- */
/* LEN: numero di CARATTERI (code point Unicode). "é"=1, "─"=1, "😀"=1 (4 byte UTF-8).
 * Prima le gambe C contavano i BYTE e le managed le unità UTF-16: entrambe sbagliate per
 * "caratteri". Vedi Bugs/(solved) - LEN-UTF8-Byte-VS-Char-Gambe-C. */
gmb_int_t gmb_chars_len(const char* s);
/* Numero di caratteri nei primi `maxbytes` byte (per convertire un offset byte in indice). */
gmb_int_t gmb_chars_len_n(const char* s, size_t maxbytes);
/* Offset in BYTE del `chars`-esimo carattere (0..len). Oltre la fine → strlen. */
size_t gmb_chars_to_byte(const char* s, gmb_int_t chars);
/* LEN di una stringa: CARATTERI. */
gmb_int_t gmb_str_t_len(gmb_string_t s);
/* LENB di una stringa: BYTE della codifica UTF-8 (la lunghezza memorizzata). */
gmb_int_t gmb_str_bytes(gmb_string_t s);
gmb_string_t gmb_str_left(gmb_string_t s, gmb_int_t n);
gmb_string_t gmb_str_right(gmb_string_t s, gmb_int_t n);
gmb_string_t gmb_str_mid(gmb_string_t s, gmb_int_t start, gmb_int_t len);
gmb_list_t gmb_str_to_chars(gmb_string_t s);
gmb_string_t gmb_str_reverse(gmb_string_t s);
gmb_int_t gmb_str_instr(gmb_string_t s, gmb_string_t sub, gmb_int_t start);
gmb_string_t gmb_str_repeat(gmb_string_t s, gmb_int_t count, gmb_string_t sep);
gmb_string_t gmb_dup(gmb_int_t n, int ch);
/* DUP con un CARATTERE (stringa): ripete n volte il primo carattere UTF-8 di `s`.
 * gmb_dup/ gmb_dup_cp accettano un CODE POINT numerico (codificato in UTF-8). */
gmb_string_t gmb_dup_str(gmb_int_t n, const char* s);
gmb_string_t gmb_dup_cp(gmb_int_t n, int codepoint);
gmb_string_t gmb_string_padl(gmb_string_t s, gmb_int_t len, gmb_string_t fill);
gmb_string_t gmb_string_padr(gmb_string_t s, gmb_int_t len, gmb_string_t fill);
gmb_string_t gmb_string_padc(gmb_string_t s, gmb_int_t len, gmb_string_t fill);
gmb_string_t gmb_string_adjust_fixed(gmb_string_t s, gmb_int_t len);
gmb_string_t gmb_str_petscii(gmb_string_t s);
gmb_int_t gmb_str_isblank(gmb_string_t s);
gmb_string_t gmb_str_trim(gmb_string_t s);
gmb_string_t gmb_str_ltrim(gmb_string_t s);
gmb_string_t gmb_str_rtrim(gmb_string_t s);
gmb_string_t gmb_str_upper(gmb_string_t s);
gmb_string_t gmb_str_lower(gmb_string_t s);
gmb_bool_t gmb_str_contains(gmb_string_t s, gmb_string_t sub);
gmb_bool_t gmb_str_startswith(gmb_string_t s, gmb_string_t prefix);
gmb_bool_t gmb_str_endswith(gmb_string_t s, gmb_string_t suffix);
gmb_string_t gmb_str_replace(gmb_string_t s, gmb_string_t old, gmb_string_t neu);
gmb_int_t gmb_str_count(gmb_string_t s, gmb_string_t value);
gmb_string_t gmb_str_space(gmb_int_t n);
gmb_string_t gmb_str_tab(gmb_int_t col);
gmb_string_t gmb_str_title(gmb_string_t s);
gmb_string_t gmb_str_pascal(gmb_string_t s);
gmb_string_t gmb_str_camel(gmb_string_t s);
gmb_string_t gmb_str_snake(gmb_string_t s);
gmb_string_t gmb_str_kebab(gmb_string_t s);
gmb_string_t gmb_str_sentence(gmb_string_t s);
gmb_string_t gmb_str_wrap(gmb_string_t text, gmb_int_t width, gmb_int_t alignment);
gmb_int_t gmb_str_search(gmb_string_t haystack, gmb_string_t needle);
gmb_string_t gmb_str_hex(gmb_int_t n);
gmb_string_t gmb_str_oct(gmb_int_t n);
gmb_string_t gmb_str_bin(gmb_int_t n);

/* --- Builtin data/ora (DateUtils/TimerUtils C#) — date in formato OADate (1899-12-30 = 0) --- */
gmb_double_t gmb_dt_datevalue(gmb_string_t s);
gmb_double_t gmb_val_to_oadate(gmb_val_t v);
/* Getter RDD tipizzati (gmb_val_t): numero se la cella è numerica, altrimenti stringa —
 * per l'assegnazione `v = RDD.GETFIELD(...)` senza "double = string" (parità IL/VM-C). */
gmb_val_t gmb_rdd_get_field_val(int idx);
gmb_val_t gmb_rdd_get_field_val_by_name(const char* name);
gmb_val_t gmb_rdd_get_field_str_name(const char* name);
gmb_val_t gmb_rdd_oldval_val(int idx);
gmb_double_t gmb_dt_ctod(gmb_string_t s);
gmb_double_t gmb_dt_stod(gmb_string_t s);
gmb_string_t gmb_dt_dtoc(gmb_double_t oa);
/* DTOC variante RDD → ISO "yyyy-MM-dd" (RDD.DTOC; il DTOC standard e' US "mm-dd-yyyy"). */
gmb_string_t gmb_dt_dtoc_iso(gmb_double_t oa);
gmb_string_t gmb_dt_dtos(gmb_double_t oa);
gmb_string_t gmb_dt_datetime_to_string(gmb_double_t oa);
gmb_string_t gmb_dt_datetime_iso(gmb_double_t oa);   /* JSON: "yyyy-MM-ddTHH:mm:ss" */
gmb_int_t gmb_dt_dow(gmb_double_t oa);
gmb_string_t gmb_dt_cdow(gmb_double_t oa);
gmb_string_t gmb_dt_cmonth(gmb_double_t oa);
gmb_int_t gmb_dt_isoweek(gmb_double_t oa);
gmb_int_t gmb_dt_seconds(void);
gmb_string_t gmb_time_string(void);
gmb_string_t gmb_dt_date_string(void);
gmb_double_t gmb_dt_dayserial(void);

/* --- Data/ora in formato OADate (giorni dal 1899-12-30, frazione = ora) --- */
gmb_double_t gmb_dt_now(void);
gmb_double_t gmb_dt_today(void);
/* RDD: VALTYPE/FLOAT/CURVAL/TABLEUPDATE/TABLEREVERT/BUFFER (definite in gmb_rdd.c). */
gmb_string_t gmb_rdd_valtype_str(void);
gmb_double_t gmb_rdd_float(gmb_double_t v);
gmb_double_t gmb_rdd_curval(gmb_double_t v);
gmb_double_t gmb_rdd_tableupdate(int area);
gmb_double_t gmb_rdd_tablerevert(int area);
gmb_double_t gmb_rdd_buffer(int area, int mode);
gmb_double_t gmb_dt_timeofday(void);
gmb_int_t gmb_dt_isdate(const char* s);
gmb_double_t gmb_dt_eomonth(gmb_double_t oa, int months);
gmb_int_t gmb_dt_weekday(gmb_double_t oa);
gmb_double_t gmb_dt_datediff(const char* interval, gmb_double_t oa1, gmb_double_t oa2);
gmb_double_t gmb_dt_dateadd(const char* interval, gmb_double_t oa, gmb_double_t amount);

/* --- Componenti data/ora (DATESERIAL/YEAR/MONTH/DAY/HOUR/MINUTE/SECOND) --- */
gmb_double_t gmb_dt_dateserial(int y, int m, int d);
gmb_double_t gmb_firstdayofmonth(gmb_double_t oa);
gmb_int_t gmb_dt_year(gmb_double_t oa);
gmb_int_t gmb_dt_month(gmb_double_t oa);
gmb_int_t gmb_dt_day(gmb_double_t oa);
gmb_int_t gmb_dt_hour(gmb_double_t oa);
gmb_int_t gmb_dt_minute(gmb_double_t oa);
gmb_int_t gmb_dt_second(gmb_double_t oa);
gmb_double_t gmb_dt_timeserial(int h, int mi, int s);
gmb_int_t gmb_dt_daysinmonth(gmb_double_t oa);
gmb_int_t gmb_dt_isleapyear(gmb_double_t oa);
gmb_double_t gmb_dt_firstdayofmonth(gmb_double_t oa);
gmb_double_t gmb_dt_datefromunix(gmb_double_t unix_sec);
gmb_double_t gmb_dt_unix(gmb_double_t oa, int have_oa);
gmb_int_t gmb_dt_isowk_year(gmb_double_t oa);
gmb_string_t gmb_dt_timedollar(gmb_double_t oa);

/* --- FORMAT(FormatUtils C#): mask numeriche e token data/ora --- */
gmb_string_t gmb_format(gmb_double_t value, gmb_string_t format);

/* --- Builtin matematiche (Numerics C#) --- */
gmb_double_t gmb_round(gmb_double_t value, gmb_int_t digits);
gmb_double_t gmb_rnd(void);
void gmb_rnd_seed(uint64_t seed); /* RANDOMIZE: semina lo xorshift64 di gmb_rnd */
gmb_double_t gmb_pi(void);
gmb_double_t gmb_min(gmb_double_t a, gmb_double_t b);
gmb_double_t gmb_max(gmb_double_t a, gmb_double_t b);
gmb_string_t gmb_chr(gmb_int_t code);
gmb_int_t gmb_timer(void);
/* TimerUtils (CommonLib/TimerUtils.cs:78) — stopwatch high-res handle-registry, ns CLOCK_MONOTONIC */
int gmb_timer_stopwatch_start(void);
long gmb_timer_stopwatch_stop(int handle);
long gmb_timer_stopwatch_elapsed(int handle);
/* Collections micro (CommonLib/Collections/) — BPlusTree order, SparseSet capacity, ConsistentHash virtual nodes */
int64_t gmb_bptree_create(int order);
int gmb_bptree_add(int64_t h, double key, double value);
int gmb_bptree_get(int64_t h, double key, double* value);
int gmb_bptree_delete(int64_t h, double key);
int gmb_bptree_min(int64_t h, double* key, double* value);
int gmb_bptree_max(int64_t h, double* key, double* value);
void gmb_bptree_clear(int64_t h);
int64_t gmb_bptree_count(int64_t h);
int gmb_bptree_keys(int64_t h, double* out, int cap);
void gmb_bptree_free(int64_t h);
int64_t gmb_sparseset_create(int maxValue, int capacity);
int gmb_sparseset_add(int64_t h, int value);
int gmb_sparseset_contains(int64_t h, int value);
int gmb_sparseset_remove(int64_t h, int value);
void gmb_sparseset_clear(int64_t h);
int gmb_sparseset_count(int64_t h);
int gmb_sparseset_to_array(int64_t h, int* out, int cap);
void gmb_sparseset_free(int64_t h);
int64_t gmb_hashring_create(int replicas);
int gmb_hashring_add_node(int64_t h, const char* node);
int gmb_hashring_remove_node(int64_t h, const char* node);
const char* gmb_hashring_get_node(int64_t h, const char* key);
void gmb_hashring_free(int64_t h);
/* Http BspSessionManager (common/CommonLib/Http/BspSessionManager.cs:103) — cookie session store */
int64_t gmb_bsp_session_create_manager(int timeout_minutes);
int gmb_bsp_session_create(int64_t h, char* id_out, char* cookie_out);
int gmb_bsp_session_get(int64_t h, const char* session_id);
int gmb_bsp_session_get_or_create(int64_t h, const char* cookie_header, char* id_out, char* cookie_out, int* created);
void gmb_bsp_session_destroy(int64_t h, const char* session_id);
int gmb_bsp_session_count(int64_t h);
int gmb_bsp_session_set_data(int64_t h, const char* session_id, const char* key, const char* value);
const char* gmb_bsp_session_get_data(int64_t h, const char* session_id, const char* key);
void gmb_bsp_session_free_manager(int64_t h);
/* AudioUtils (CommonLib/AudioUtils.cs:86) — 5 funzioni math pure, no SDL dep */
double gmb_audio_envelope_adsr(int sample, int attackSamples, int decaySamples,
                               int sustainSamples, int releaseSamples, double sustainLevel);
double gmb_audio_envelope(int sample, int totalSamples, int attackSamples, int releaseSamples);
double gmb_audio_frequency(double baseFreq, int octave);
double gmb_audio_midi_to_frequency(int midiNote);
double gmb_audio_note_duration(int noteLength, int tempo);
/* AudioManager.cs:398/839 — synth PCM puro (testabile) + playback SDL_mixer (GMB_GFX_SDL_MIXER) */
int gmb_audio_synth(int frequency, double duration, int waveform,
                    double attack, double decay, double sustain, double release,
                    double pulse_width, int filter_type, double cutoff, double resonance,
                    double* out, int max_samples);
int gmb_audio_mml_parse(const char* mml, double* out_freq, double* out_dur, int* out_type, int max_cmds);
void gmb_audio_sound(int frequency, double duration, int volume);
void gmb_audio_play_mml(const char* mml);
/* CHOOSE(index, values...) — indice 1-based nella lista valori (ParamArray). Ritorna il valore
 * scelto (gmb_val_t unificato) o gmb_val_nil() se l'indice è fuori range. */
gmb_val_t gmb_choose(gmb_int_t idx, const gmb_val_t* values, gmb_int_t n);

/* --- Builtin matematiche base (INT/FLOOR/CEILING/ABS/CLAMP/MAX/MIN/DIST/PERCENTILE) --- */
gmb_double_t gmb_int(gmb_double_t v);
gmb_double_t gmb_floor(gmb_double_t v);
gmb_double_t gmb_ceiling(gmb_double_t v);
gmb_double_t gmb_abs(gmb_double_t v);
gmb_double_t gmb_clamp(gmb_double_t v, gmb_double_t lo, gmb_double_t hi);
gmb_double_t gmb_max_arr(const gmb_double_t* a, int n);
gmb_double_t gmb_min_arr(const gmb_double_t* a, int n);
gmb_double_t gmb_sum_arr(const gmb_double_t* a, int n);
gmb_double_t gmb_dist2d(gmb_double_t x1, gmb_double_t y1, gmb_double_t x2, gmb_double_t y2);
gmb_double_t gmb_dist3d(gmb_double_t x1, gmb_double_t y1, gmb_double_t z1, gmb_double_t x2, gmb_double_t y2, gmb_double_t z2);
gmb_double_t gmb_percentile(const gmb_double_t* a, int n, gmb_double_t p);
gmb_string_t gmb_num_to_hex(gmb_double_t v);
gmb_string_t gmb_num_to_oct(gmb_double_t v);
gmb_int_t gmb_is_numeric(const char* s);
gmb_string_t gmb_val_str(gmb_double_t v);
gmb_double_t gmb_parse_num(const char* s, int* ok);
gmb_double_t gmb_fix(gmb_double_t v);
gmb_double_t gmb_trunc(gmb_double_t v);
gmb_double_t gmb_sqr(gmb_double_t v);
gmb_double_t gmb_sgn(gmb_double_t v);
gmb_double_t gmb_cbyte(gmb_double_t v);
gmb_double_t gmb_hypot(gmb_double_t a, gmb_double_t b);
gmb_double_t gmb_yearfrac(gmb_double_t d1, gmb_double_t d2);
gmb_double_t gmb_fact(gmb_double_t n);
gmb_double_t gmb_perm(gmb_double_t n, gmb_double_t k);
gmb_double_t gmb_comb(gmb_double_t n, gmb_double_t k);
/* Nucleo scipy.special (SPECIAL namespace) — condiviso VM/C e C99 */
gmb_double_t gmb_gamma(gmb_double_t x);
gmb_double_t gmb_lgamma(gmb_double_t x);
gmb_double_t gmb_beta_fn(gmb_double_t a, gmb_double_t b);
gmb_double_t gmb_erf(gmb_double_t x);
gmb_double_t gmb_erf_c(gmb_double_t x);
gmb_double_t gmb_gamma_p(gmb_double_t a, gmb_double_t x);
gmb_double_t gmb_gammaincc(gmb_double_t a, gmb_double_t x);
gmb_double_t gmb_incbeta(gmb_double_t a, gmb_double_t b, gmb_double_t x);
gmb_double_t gmb_betaln(gmb_double_t a, gmb_double_t b);
gmb_double_t gmb_digamma(gmb_double_t x);
gmb_double_t gmb_erfinv(gmb_double_t x);
gmb_double_t gmb_erfcinv(gmb_double_t x);
gmb_double_t gmb_gammaincinv(gmb_double_t a, gmb_double_t y);
gmb_double_t gmb_gammainccinv(gmb_double_t a, gmb_double_t y);
gmb_double_t gmb_betaincinv(gmb_double_t a, gmb_double_t b, gmb_double_t y);
gmb_double_t gmb_besselj(gmb_double_t v, gmb_double_t x);
gmb_double_t gmb_bessely(gmb_double_t v, gmb_double_t x);
gmb_double_t gmb_besseli(gmb_double_t v, gmb_double_t x);
gmb_double_t gmb_besselk(gmb_double_t v, gmb_double_t x);
/* Nucleo scipy.signal (SIGNAL namespace) — array double */
double* gmb_signal_convolve(const double* x, gmb_int_t nx, const double* k, gmb_int_t nk, gmb_int_t* outn);
double* gmb_signal_correlate(const double* x, gmb_int_t nx, const double* k, gmb_int_t nk, gmb_int_t* outn);
double* gmb_signal_window(gmb_int_t n, int kind, gmb_int_t* outn);
/* --- Nuclei scipy.integrate/interpolate/fft (2026-08-25) --- */
double gmb_quad_gk(double (*f)(double, void*), void* ctx, double a, double b);
double gmb_simpson_ctx(double (*f)(double, void*), void* ctx, double a, double b, int n);
double gmb_trapezoid(const double* y, const double* x, int n);
double* gmb_cumtrapezoid(const double* y, const double* x, int n, gmb_int_t* outn);
double* gmb_solve_ode_ctx(double (*f)(double, double, void*), void* ctx, double y0, double t0, double t1, int steps, gmb_int_t* outn);
double gmb_cubic_spline(const double* x, const double* y, int n, double tx);
double* gmb_fftfreq(int n, double d, gmb_int_t* outn);
double* gmb_fftshift(const double* arr, int n, gmb_int_t* outn);
double* gmb_ifftshift(const double* arr, int n, gmb_int_t* outn);
/* --- Filtri IIR (gmb_filters.c): design → handle; LFILTER/FILTFILT/FREQZ applicano --- */
gmb_int_t gmb_filter_butter(gmb_int_t n, const double* wn, int nwn, const char* btype);
gmb_int_t gmb_filter_cheby1(gmb_int_t n, double rp, const double* wn, int nwn, const char* btype);
gmb_int_t gmb_filter_cheby2(gmb_int_t n, double rs, const double* wn, int nwn, const char* btype);
gmb_int_t gmb_filter_ellip(gmb_int_t n, double rp, double rs, const double* wn, int nwn, const char* btype);
double* gmb_filter_lfilter(gmb_int_t h, const double* x, int nx, gmb_int_t* outn);
double* gmb_filter_filtfilt(gmb_int_t h, const double* x, int nx, gmb_int_t* outn);
double* gmb_filter_freqz(gmb_int_t h, int npts, gmb_int_t* outn);
double* gmb_filter_lowpass(const double* x, int nx, double wn, gmb_int_t n, gmb_int_t* outn);
double* gmb_filter_highpass(const double* x, int nx, double wn, gmb_int_t n, gmb_int_t* outn);
double* gmb_filter_bandpass(const double* x, int nx, double w1, double w2, gmb_int_t n, gmb_int_t* outn);
double* gmb_filter_bandstop(const double* x, int nx, double w1, double w2, gmb_int_t n, gmb_int_t* outn);
/* --- Distribuzioni statistiche (gmb_math.c) --- */
double gmb_dist_normal_pdf(double mean, double sd, double x);
double gmb_dist_normal_cdf(double mean, double sd, double x);
double gmb_dist_chi_pdf(double df, double x);
double gmb_dist_chi_cdf(double df, double x);
double gmb_dist_chi2_pdf(double df, double x);
double gmb_dist_chi2_cdf(double df, double x);
double gmb_dist_beta_pdf(double a, double b, double x);
double gmb_dist_beta_cdf(double a, double b, double x);
double gmb_dist_student_t_pdf(double df, double x);
double gmb_dist_student_t_cdf(double df, double x);
double gmb_dist_weibull_pdf(double shape, double scale, double x);
double gmb_dist_weibull_cdf(double shape, double scale, double x);
double gmb_dist_exp_pdf(double rate, double x);
double gmb_dist_exp_cdf(double rate, double x);
double gmb_dist_bernoulli_pmf(double p, int k);
double gmb_dist_binomial_pmf(double p, int n, int k);
double gmb_dist_poisson_pmf(double lambda, int k);
double gmb_dist_geometric_pmf(double p, int k);
double gmb_dist_negbinom_pmf(double r, double p, int k);
double gmb_dist_hypergeom_pmf(int N, int K, int n, int k);
double gmb_dist_lognormal_pdf(double mu, double sigma, double x);
double gmb_dist_lognormal_cdf(double mu, double sigma, double x);
double gmb_dist_f_pdf(double d1, double d2, double x);
double gmb_dist_f_cdf(double d1, double d2, double x);
double gmb_dist_pareto_pdf(double xm, double alpha, double x);
double gmb_dist_pareto_cdf(double xm, double alpha, double x);
double gmb_dist_logistic_pdf(double mu, double s, double x);
double gmb_dist_logistic_cdf(double mu, double s, double x);
double gmb_dist_triangular_pdf(double a, double b, double c, double x);
double gmb_dist_uniform_pdf(double a, double b, double x);
double gmb_dist_uniform_cdf(double a, double b, double x);
double gmb_dist_skewnormal_pdf(double x, double location, double scale, double shape);
double gmb_dist_normal_ppf(double mean, double sd, double p);
double gmb_dist_normal_isf(double mean, double sd, double p);
double gmb_dist_lognormal_ppf(double mu, double sigma, double p);
double gmb_dist_chi2_ppf(double df, double p);
double gmb_dist_chi2_isf(double df, double p);
double gmb_dist_chi_ppf(double df, double p);
double gmb_dist_beta_ppf(double a, double b, double p);
double gmb_dist_f_ppf(double d1, double d2, double p);
double gmb_dist_f_isf(double d1, double d2, double p);
double gmb_dist_student_t_ppf(double df, double p);
double gmb_dist_student_t_isf(double df, double p);
double gmb_dist_exp_ppf(double rate, double p);
double gmb_dist_exp_isf(double rate, double p);
double gmb_dist_weibull_ppf(double shape, double scale, double p);
double gmb_dist_logistic_ppf(double loc, double scale, double p);
double gmb_dist_uniform_ppf(double a, double b, double p);
double gmb_dist_pareto_ppf(double scale, double shape, double p);
double gmb_dist_triangular_ppf(double a, double b, double c, double p);


gmb_double_t gmb_polarx(gmb_double_t r, gmb_double_t angle_deg);
gmb_double_t gmb_polary(gmb_double_t r, gmb_double_t angle_deg);
gmb_double_t gmb_angle2d(gmb_double_t x1, gmb_double_t y1, gmb_double_t x2, gmb_double_t y2);
gmb_double_t gmb_pythagoras(gmb_double_t a, gmb_double_t b);
double gmb_rand_int(double min, double max);
double gmb_rand_double(double min, double max);
double gmb_rand_normal(double mean, double stddev);
uint8_t* gmb_rand_bytes(int count);
gmb_string_t gmb_rand_bytes_val(int count);
/* RANDOMCRYPTOBYTES(n) → byte[] da sorgente crittografica (/dev/urandom). */
gmb_string_t gmb_rand_crypto_bytes_val(int count);
double gmb_correlation(const double* x, const double* y, int n);
double gmb_mean(const double* a, int n);
double gmb_median(const double* a, int n);
double gmb_variance(const double* a, int n);
double gmb_stddev(const double* a, int n);
double gmb_skewness(const double* a, int n);
double gmb_kurtosis(const double* a, int n);
double gmb_mad(const double* a, int n);
double gmb_opt_newton(double (*fn)(double, void*), double guess, double tol, int max_iter, void* ctx);
double gmb_opt_bisection(double (*fn)(double, void*), double lo, double hi, double tol, int max_iter, void* ctx);
double gmb_opt_brent(double (*fn)(double, void*), double lo, double hi, double tol, int max_iter, void* ctx);
double gmb_opt_golden(double (*fn)(double, void*), double lo, double hi, double tol, int max_iter, void* ctx);
double gmb_opt_secant(double (*fn)(double, void*), double x0, double x1, double tol, int max_iter, void* ctx);
void gmb_opt_nelder_mead(double (*fn)(const double*, int, void*), double* start, int n, double tol, int max_iter, void* ctx, double* out);
void gmb_opt_nelder_mead_c99(gmb_fn_t fn, double* start, int n, double* out);
void gmb_opt_bfgs_c99(gmb_fn_t fn, double* start, int n, double* out);
void gmb_opt_cg(double (*fn)(const double*, int, void*), double* start, int n, double tol, int max_iter, void* ctx, double* out);
void gmb_opt_lbfgs(double (*fn)(const double*, int, void*), double* start, int n, double tol, int memory, int max_iter, void* ctx, double* out);
void gmb_opt_lbfgs_c99(gmb_fn_t fn, double* start, int n, double* out);
void gmb_opt_broyden_c99(gmb_fn_t fn, double* x0, int n, double* out);
int gmb_opt_lin_lstsq(const double* const* a, const double* b, int rows, int cols, double* x);
double* gmb_opt_simplex_c99(const double* c, int n, const double* const* aub, const double* bub, int mub, gmb_int_t* outn);
void gmb_opt_bfgs(double (*fn)(const double*, int, void*), double* start, int n, double tol, int max_iter, void* ctx, double* out);
gmb_string_t gmb_str_join_list(gmb_val_t* items, int count, const char* sep);
gmb_val_t* gmb_str_split_list(const char* s, const char* sep, int* count_out);
gmb_string_t* gmb_str_split_strings(const char* s, const char* sep, int* count_out);
gmb_string_t* gmb_str_tokens_strings(const char* s, const char* delims, int* count_out);

/* ----------------------------------------------------------------------------
 * Event handlers (RAISE EVENT / ADD HANDLER / REMOVE HANDLER)
 * Il registro è una lista concatenata di {nomeEvento, funzione handler}.
 * La chiamata del handler al RAISE è typed dal transpiler: il puntatore a
 * funzione viene castato alla firma dell'EVENT (parametri dichiarati).
 * ---------------------------------------------------------------------------- */
typedef struct gmb_event_handler {
    const char* name;        /* nome evento (costante stringa) */
    void (*fn)(void);        /* handler — cast a firma typed al RAISE */
    struct gmb_event_handler* next;
} gmb_event_handler_t;

void gmb_event_add(const char* name, void (*fn)(void));
void gmb_event_remove(const char* name, void (*fn)(void));
int gmb_event_count(const char* name);
gmb_event_handler_t* gmb_event_first(const char* name);
gmb_event_handler_t* gmb_event_next(gmb_event_handler_t* h, const char* name);

/* ----------------------------------------------------------------------------
 * TimeSeries builtins (TIMESERIES namespace) — restituiscono array allocati
 * (gmb_double_t*) con la lunghezza in out_len. Il chiamante libera con free().
 * ---------------------------------------------------------------------------- */
/* Errore dell'ULTIMA funzione TIMESERIES: "" se ok, altrimenti il messaggio (parità dei
 * throw ArgumentException C#). Il chiamante (VM/C, C99) azzera prima e controlla dopo. */
const char* gmb_ts_last_error(void);
void gmb_ts_clear_error(void);
gmb_double_t* gmb_ts_rollautocorr(gmb_double_t* series, gmb_int_t n, gmb_int_t period, gmb_int_t lag, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolldelta(gmb_double_t* series, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len);
gmb_double_t* gmb_ts_moving_average(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_ema(gmb_double_t* data, gmb_int_t n, double alpha, gmb_int_t* out_len);
gmb_double_t* gmb_ts_returns(gmb_double_t* data, gmb_int_t n, int log_returns, gmb_int_t* out_len);
double gmb_ts_volatility(gmb_double_t* data, gmb_int_t n, int log_returns);
double gmb_ts_autocorrelation(gmb_double_t* data, gmb_int_t n, gmb_int_t lag);
gmb_double_t* gmb_ts_difference(gmb_double_t* data, gmb_int_t n, gmb_int_t order, gmb_int_t* out_len);
gmb_double_t* gmb_ts_deseasonalize(gmb_double_t* data, gmb_int_t n, gmb_int_t season, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_mean(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_stddev(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_zscore(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_cov(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_corr(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_outliers(gmb_double_t* data, gmb_int_t n, double threshold, gmb_int_t* out_len);

/* ----------------------------------------------------------------------------
 * Array dinamici (REDIM / JOIN)
 * ---------------------------------------------------------------------------- */
void gmb_array_resize(void** arr, gmb_int_t* len, gmb_int_t new_len, int elem_size, int preserve);
gmb_string_t gmb_array_join(gmb_string_t* str_arr, gmb_int_t n, const char* sep);
gmb_string_t gmb_array_join_doubles(gmb_double_t* arr, gmb_int_t n, const char* sep);
gmb_string_t gmb_double_array_join(const gmb_double_t* a, gmb_int_t n, const char* sep);
gmb_string_t gmb_string_array_join(const gmb_string_t* a, gmb_int_t n, const char* sep);
gmb_string_t gmb_list_join_doubles(gmb_list_t l, const char* sep);
gmb_string_t gmb_ajoin(gmb_list_t l, const char* sep);
gmb_list_t gmb_range(double a, double b);
gmb_list_t gmb_range_step(double a, double b, double step);
gmb_int_t gmb_array_find_index(void* arr, gmb_int_t n, gmb_fn_t fn, int type);

/* ----------------------------------------------------------------------------
 * Array Clipper su array NATIVI (gmb_double_t* / gmb_string_t* + lunghezza
 * compagna) — parita IL (common/IlBackendRuntime/ArrayHelpers.cs, fonte canonica:
 * 0-based, -1 not-found, nuova copia per AMAP/AFILTER/AREVERSE/ACLONE/AUNIQUE,
 * AREVERSE NON muta l'input). Le funzioni con callback gmb_fn_t usano l'ABI del
 * runtime (gmb_list.c): map/filter/count/scan-predicato -> gmb_double_t(*)(void*,
 * gmb_double_t); reduce -> gmb_double_t(*)(void*, gmb_double_t, gmb_double_t).
 * I risultati array sono malloc'd (ownership al chiamante; runtime a vita breve).
 * ---------------------------------------------------------------------------- */
/* numerici (gmb_double_t*) */
gmb_double_t* gmb_array_map(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n);
gmb_double_t* gmb_array_filter(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n);
/* Varianti BOXED (gmb_val_t* — array di UDT/classe): stessa semantica, ABI lambda
 * gmb_double_t(*)(void*, gmb_val_t). */
gmb_val_t*    gmb_array_filter_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n);
gmb_double_t* gmb_array_map_val_dbl(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n);
/* AMAP su array di UDT con callback UDT->UDT (val->val): preserva l'elemento boxato.
 * ABI lambda: gmb_val_t(*)(void*, gmb_val_t). */
gmb_val_t*    gmb_array_map_val_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n);
/* AREDUCE/ACOUNT su array di UDT (gmb_val_t* boxed): accumulatore numerico + elemento
 * boxed. ABI lambda: reduce gmb_double_t(*)(void*, gmb_double_t, gmb_val_t),
 * count gmb_double_t(*)(void*, gmb_val_t). */
gmb_double_t  gmb_array_reduce_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, int has_init, gmb_double_t init);
gmb_int_t     gmb_array_count_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn);
gmb_int_t     gmb_array_count(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn);
/* ASCAN su array di UDT (gmb_val_t*): indice del primo match, -1 se nessuno. */
gmb_int_t     gmb_array_scan_pred_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t start);
gmb_double_t  gmb_array_reduce(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, int has_init, gmb_double_t init);
gmb_int_t     gmb_array_scan_pred(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t start);
gmb_int_t     gmb_array_scan_pred_str(const gmb_string_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t start);
gmb_int_t     gmb_array_scan_value(const gmb_double_t* a, gmb_int_t n, gmb_double_t v, gmb_int_t start);
gmb_double_t* gmb_array_reverse_copy(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n);
gmb_double_t* gmb_array_unique(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n);
gmb_double_t* gmb_array_clone(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n);
void          gmb_array_sort_inplace(gmb_double_t* a, gmb_int_t n);
/* Sort in-place di un array di stringhe (strcmp). Per ORDER BY di una comprehension lista
 * con elementi stringa (`[ n FOR n IN names ]` ordina i nomi). */
void          gmb_string_array_sort_inplace(gmb_string_t* a, gmb_int_t n);
/* ASORT con comparatore custom (closure gmb_fn_t): vedi gmb_array.c. Semantica: il SEGNO
 * del ritorno determina l'ordine (< 0 = a prima di b); predicato BASIC TRUE(-1) = a prima. */
void          gmb_array_sort_val_cmp(gmb_val_t* a, gmb_int_t n, gmb_fn_t cmp);
void          gmb_array_sort_dbl_cmp(gmb_double_t* a, gmb_int_t n, gmb_fn_t cmp);
gmb_double_t* gmb_array_pad_copy(const gmb_double_t* a, gmb_int_t n, gmb_int_t want, double pad, gmb_int_t* out_n);
gmb_string_t* gmb_array_pad_copy_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t want, const char* pad, gmb_int_t* out_n);
gmb_int_t     gmb_array_binary_search(const gmb_double_t* a, gmb_int_t n, gmb_double_t v);
/* stringhe (gmb_string_t*) — senza callback (deep-copy; confronto gmb_string_equals) */
gmb_string_t* gmb_array_reverse_copy_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t* out_n);
gmb_string_t* gmb_array_unique_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t* out_n);
gmb_string_t* gmb_array_clone_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t* out_n);
gmb_int_t     gmb_array_scan_value_str(const gmb_string_t* a, gmb_int_t n, const char* v, gmb_int_t start);
/* ArrayUtils — Chunk/Zip/Pad/Shuffle (common/CommonLib/ArrayUtils.cs:90-167) */
gmb_list_t gmb_list_chunk(gmb_list_t src, gmb_int_t chunkSize);
gmb_list_t gmb_list_zip_multi(const gmb_list_t* lists, gmb_int_t nLists);
gmb_list_t gmb_list_pad_copy(gmb_list_t src, gmb_int_t newLen, gmb_val_t pad);
void gmb_list_shuffle(gmb_list_t* l, uint64_t seed);
gmb_list_t gmb_array_chunk_doubles(const gmb_double_t* arr, gmb_int_t n, gmb_int_t chunkSize);
gmb_list_t gmb_array_chunk_ints(const gmb_int_t* arr, gmb_int_t n, gmb_int_t chunkSize);
gmb_int_t gmb_list_scan(gmb_list_t l, gmb_fn_t fn);

#ifdef __cplusplus
}
#endif

#endif /* GMB_RUNTIME_H */

#ifdef __cplusplus
extern "C" {   /* coda fuori dal guard storico: forziamo linkage C per i TU C++ */
#endif

/* --- Analisi numerica (gmb_math.c) --- */
void gmb_analysis_linear_regression(const double* x, const double* y, int n, double* slope, double* intercept, double* r2);
double gmb_interp_linear(const double* x, const double* y, int n, double tx);
double gmb_interp_step(const double* x, const double* y, int n, double tx);
double gmb_interp_clamp(const double* x, const double* y, int n, double tx, double (*fn)(const double*, const double*, int, double));
double gmb_interp_polynomial(const double* x, const double* y, int n, double tx);
double gmb_interp_akima(const double* x, const double* y, int n, double tx);
double gmb_interp_pchip(const double* x, const double* y, int n, double tx);
void gmb_fft(double* re, double* im, int n, int inverse);
gmb_double_t** gmb_fft_matrix(const double* x, int n, int inverse);
void gmb_holt_winters(const double* y, int n, double alpha, double beta, double gamma, int period,
                      double* fitted_out, int fitted_n, double* mse_out);
void gmb_adf_test(const double* y, int n, double* statistic, double* pvalue);
void gmb_kmeans(const double* data, int n, int k, double* centroids_out);
/* ARIMA(p,d,q) Hannan-Rissanen (canone CommonLib.Numerics.ArimaFit): ar_c[0..p), ma_c[0..q),
 * *aic, fitted[0..n-d) (serie differenziata). Ritorna 0 ok, -1 serie troppo corta. */
int gmb_arima_fit(const double* y, int n, int p, int d, int q,
                  double* ar_c, double* ma_c, double* aic, double* fitted);

/* --- Trading metrics & portfolio (gmb_math.c) --- */
double gmb_tm_calmar_ratio(const double* r, int n, int ppy);
double gmb_tm_information_ratio(const double* p, const double* b, int n, int ppy);
double gmb_tm_treynor_ratio(const double* r, const double* b, int n, double rf, int ppy);
double gmb_tm_sharpe_ratio(const double* r, int n, double rf, int ppy);
double gmb_tm_returns_skewness(const double* r, int n);
double gmb_tm_annualized_tracking_error(const double* p, const double* b, int n, int ppy);
double gmb_tm_annualized_downside_dev(const double* r, int n, double thr, int ppy);
double gmb_tm_diluted_eps(double ni, double shares, double other);
double gmb_tm_ps_ratio(double price, double sps);
double gmb_tm_pb_ratio(double price, double bv);
double gmb_tm_dividend_payout(double div, double ni);
double gmb_tm_total_stock_return(double p0, double p1, double div);
double gmb_tm_annualized_return(const double* r, int n, int ppy);
double gmb_tm_annualized_volatility(const double* r, int n, int ppy);
double gmb_tm_max_drawdown(const double* r, int n);
double gmb_tm_downside_deviation(const double* r, int n, double thr);
double gmb_tm_value_at_risk(const double* r, int n, double conf);
double gmb_tm_sortino_ratio(const double* r, int n, double rf, int ppy);
double gmb_fin_current_yield(double coupons, double price);
double gmb_fin_ytm(double coupon, double fv, double price, double years);
double gmb_fin_zcb_value(double fv, double rate, double time);
double gmb_fin_zcb_yield(double fv, double price, double time);
double gmb_fin_bvps(double equity, double shares);
double gmb_fin_dps(double dividends, double shares);
double gmb_fin_equity_multiplier(double assets, double equity);
double gmb_fin_nav(double assets, double liabilities, double shares);
double gmb_fin_preferred_value(double dividend, double rate);
double gmb_fin_stock_zero_growth(double dividend, double rate);
double gmb_fin_eps(double netIncome, double shares);
double gmb_fin_diluted_eps(double netIncome, double shares, double instruments);
double gmb_fin_ps_ratio(double price, double salesPerShare);
double gmb_fin_pb_ratio(double price, double bookValue);
double gmb_fin_dividend_payout_ratio(double dividends, double netIncome);
void gmb_port_min_vol(const double* cov, int n, double* weights);
void gmb_port_max_sharpe(const double* rets, const double* cov, int n, double rf, double* weights);
void gmb_port_risk_parity(const double* cov, int n, double* weights);
void gmb_port_equally_weighted(int n, double* weights);
void gmb_port_black_litterman(const double* caps, const double* cov, int n, double riskAversion, double tau,
                              const double* Q, double* weights, double* posterior);
int gmb_cholesky_solve(const double* a, const double* b, int n, double* x);
int gmb_cholesky_solve_jagged(const double* const* a, const double* b, int n, double* x);
/* --- Trading QLNet wrapper (common/CommonLib/Trading/QLNet.cs:256) — math pura, no QLNet C# lib --- */
double gmb_qlnet_portfolio_return(const double* w, const double* mu, int n);
void gmb_qlnet_portfolio_risk(const double* w, const double* cov, int n, double* variance, double* stdev);
double gmb_qlnet_sharpe(double portfolioReturn, double portfolioStDev, double riskFreeRate);
double gmb_qlnet_markowitz_value(const double* w, const double* cov, int n);
double gmb_qlnet_kelly_value(const double* w, const double* cov, const double* mu, int n);
int gmb_qlnet_onesum(const double* w, int n);
int gmb_qlnet_nonneg(const double* w, int n);
int gmb_qlnet_target_return(const double* w, const double* mu, int n, double target);
int gmb_qlnet_boundary(const double* w, const double* lower, const double* upper, int n);

/* --- Analisi multivariata (gmb_math.c) --- */
void gmb_analysis_pca(const double* data, int m, int n, double* eigenvals, double* eigenvecs);
gmb_val_t gmb_pca_result(gmb_val_t v);
gmb_val_t gmb_pca_result_rect(const double* data, int k, int n);
gmb_val_t gmb_pca_result_jagged(const double* const* rows, const int* sizes, int k);
void gmb_analysis_anova(const double* groups, int* groupSizes, int k, double* f, double* p, double* ssb, double* ssw);

/* --- INI helpers (gmb_file.c) --- */
char* gmb_ini_read_value(const char* path, const char* section, const char* key, const char* def);
char** gmb_ini_read_section(const char* path, const char* section, int* count_out);
gmb_val_t gmb_ini_section_to_dict_val(const char* path, const char* section);
gmb_val_t gmb_ini_coerce_val(const char* v);
/* Scrittura INI del file di settings (JSON WRITEVALUE/WRITESECTION, canone
 * CommonLib.FileReadWriteUtils.WriteValue/WriteSection — §0bis runtime condiviso). */
void gmb_ini_write_value(const char* path, const char* section, const char* key, const char* value);
void gmb_ini_write_section(const char* path, const char* section, const gmb_dict_t* values);

double gmb_stats_jarque_bera(const double* data, int n);

/* Statistics - Hypothesis Tests */
gmb_val_t gmb_ttest(gmb_val_t x_val, gmb_val_t y_val);
gmb_val_t gmb_ttestpaired(gmb_val_t x_val, gmb_val_t y_val);
gmb_val_t gmb_ttestonesample(gmb_val_t x_val, gmb_val_t mu_val);
gmb_val_t gmb_anova(gmb_val_t groups_val);
gmb_val_t gmb_anova_result(gmb_val_t groups_val);
gmb_val_t gmb_anova_result_flat(const double* data, const int* sizes, int k);
gmb_val_t gmb_anova_result_jagged(const double* const* rows, const int* sizes, int k);
gmb_val_t gmb_anova_result_rect(const double* data, int k, int ncol);
gmb_val_t gmb_chi2(gmb_val_t observed_val, gmb_val_t expected_val);
gmb_val_t gmb_fisher(gmb_int_t a, gmb_int_t b, gmb_int_t c, gmb_int_t d);
gmb_val_t gmb_ztest(gmb_val_t x_val, gmb_val_t mu_val, gmb_val_t sigma_val);
gmb_val_t gmb_ks(gmb_val_t x_val, gmb_val_t y_val);
gmb_val_t gmb_shapiro(gmb_val_t x_val);
gmb_val_t gmb_normaltest(gmb_val_t x_val);
double gmb_list_sum(gmb_list_t l);
double gmb_list_max(gmb_list_t l);
double gmb_list_min(gmb_list_t l);
gmb_int_t gmb_list_binary_search(gmb_list_t l, double target);

gmb_double_t* gmb_ts_rolling_mad(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_autocorr(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t lag, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_regression(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_decompose_trend(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len);
gmb_double_t* gmb_ts_decompose_seasonal(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len);
gmb_double_t* gmb_ts_decompose_matrix(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len);

double gmb_tm_max_return(const double* r, int n);
double gmb_tm_min_semivariance(const double* r, int n, double thr);
double gmb_tm_kelly_criterion(const double* r, int n);
gmb_val_t gmb_tm_portfolio_result(const double* r, int n, int metric_kind);
gmb_val_t gmb_tm_backtest(const double* r, int n);
/* TRADING.BACKTESTS.RUN su array 2-D rettangolare (flat rows×cols): media per RIGA →
 * metriche backtest (parità IL/PCode/VM-C: rendimenti per periodo = media degli asset). */
gmb_val_t gmb_tm_backtest_matrix(const double* flat, int rows, int cols);
gmb_val_t gmb_tm_portfolio_optimize(gmb_double_t** cov, int n, const double* rets, int rets_n, double rf, int kind);
/* TRADING.PORTFOLIO.EVOLVE(rets, cov[, GA]): min-varianza (proxy deterministico del GA IL) →
 * dict PortfolioResult (ExpectedReturn/Volatility/SharpeRatio/Weights). cov JAGGED. */
gmb_val_t gmb_port_evolve(gmb_double_t** cov, int n, const double* rets, int rets_n, double rf);
/* TRADING.PORTFOLIO.BLACKLITTERMAN(caps, cov, riskAversion, tau, P, Q, omega, rf) → PortfolioResult.
 * cov/P/omega JAGGED; k = numero di view. Formula completa (Π + PᵀΩ⁻¹Q). */
gmb_val_t gmb_blacklitterman(const double* caps, int n, gmb_double_t** cov,
                             double riskAversion, double tau,
                             gmb_double_t** P, int k, const double* Q,
                             gmb_double_t** omega, double rf);
double gmb_tm_risk_premium(double asset, double rf);
double gmb_tm_capital_gains_yield(double p0, double p1);
double gmb_tm_holding_period_return(const double* r, int n);
double gmb_tm_geometric_mean_return(const double* r, int n);
double gmb_tm_growth_rate(double retention, double roe);
double gmb_tm_bid_ask_spread(double bid, double ask);
double gmb_tm_tax_equivalent_yield(double tfy, double rate);
double gmb_tm_min_cvar(const double* r, int n, double conf);
double gmb_tm_min_cdar(const double* r, int n, double conf);

int gmb_agent_create(const char* name);
int gmb_agent_exists(const char* name);
char* gmb_agent_ask_chain(const char* name, const char* prompt, const char* prefix);
char* gmb_eval_validate(const char* result, const char* folder);
int gmb_agent_delete(const char* name);
int gmb_agent_set(const char* name, const char* key, const char* value);
const char* gmb_agent_get(const char* name, const char* key);
char* gmb_agent_ask(const char* name, const char* prompt);
/* Fixture LLM deterministica (parità FixtureLlmCompleter.Complete C#): UNICA sorgente
 * condivisa da gmb_agent_ask e dal loop CUA (AI.AGENTRUN, gmb_agent_loop.c). */
void gmb_fixture_llm_complete(const char* user_prompt, char* out, size_t cap);
/* F6 multimodale (2026-09-11): agent ask con input multimodale (immagine/audio/documento). */
char* gmb_agent_ask_file(const char* name, const char* path, const char* question);

/* gmb_video.c — F7 multimodale (2026-09-11): VIDEO (Y4M nativo; altri formati via ffmpeg).
 * info → "width,height,fps,frames"; frame → PNG del frame `index`. 1 = ok, 0 = errore. */
const char* gmb_video_error(void);
int gmb_video_info(const char* path, char* out, int cap);
const char* gmb_video_info_str(const char* path);
int gmb_video_frame(const char* path, int index, const char* output);
char* gmb_agent_swarm(double n, const char* prefix, const char* task, int max_concurrency); /* malloc, parita' IlRuntime.AiAgentSwarm */
int gmb_agent_list(char out[][64], int max);
const char* gmb_agent_status(const char* name);
int gmb_agent_events(char out[][256], int max);
char* gmb_agent_events_str(void);
/* AI AGENT TRACE <ON|OFF|VERBOSE> (parità AgentEventLog.SetMode): abilita l'echo degli eventi
 * su stderr. Ritorna 1 se la modalità è valida. */
int gmb_agent_trace(const char* mode);
int gmb_agent_save(const char* name, const char* path);
/* Sessione KV dell'agente (AI.PREFILL/DECODE/CACHECLEAR/CACHETRIM/BENCHMARK): i backend C
 * non hanno una sessione KV (AgentServer è C#) → JSON di ERRORE ESPLICITO del contratto IL,
 * mai un risultato silenzioso. malloc, da free(). Parità AgentServer.cs. */
char* gmb_agent_prefill_json(const char* name, const char* role, const char* text);
char* gmb_agent_decode_json(const char* name, int max_tokens);
char* gmb_agent_cache_clear_json(const char* name);
char* gmb_agent_cache_trim_json(const char* name, int drop_tokens);
char* gmb_agent_benchmark_json(const char* name, const char* mode, int iterations, const char* text);
char* gmb_agent_benchresult_json(const char* name);
char* gmb_agent_prefill_stats_json(const char* name);
char* gmb_agent_decode_stats_json(const char* name);
char* gmb_agent_run_graph_json(const char* name, const char* backend_label);
/* §4.10 esposizione agenti: AI.AGENTRUNCODE. Seam verso il loop agenti C `gmb_agent_run_code`
 * (registrato dall'host che linka il runtime agenti: gmb_vm_c, C99Native). */
typedef char* (*gmb_agent_code_run_fn)(const char* name, const char* prompt, int rounds);
void  gmb_agent_run_code_json_set(gmb_agent_code_run_fn fn);
char* gmb_agent_run_code_json(const char* name, const char* prompt, int rounds);
/* Server HTTP agent (AI AGENT SERVE) — core in C, route agent builtin. */
int gmb_agent_http_start(int port);
int gmb_agent_http_stop(void);
int gmb_agent_load(const char* name, const char* path);

/* TEAM (AI TEAM ... / AI.TEAMLIST / AI.TEAMAGENTS) — speculare a gmb_agent.c */
int gmb_team_exists(const char* name);
int gmb_team_create(const char* name);
int gmb_team_delete(const char* name);
int gmb_team_add_agent(const char* team, const char* agent);
int gmb_team_remove_agent(const char* team, const char* agent);
int gmb_team_set_role(const char* team, const char* agent, const char* role);
int gmb_team_set_strategy(const char* team, const char* strategy);
int gmb_team_set_coordinator(const char* team, const char* agent);
int gmb_team_set_recursion(const char* team, int rounds);
int gmb_team_set_latent(const char* team, const char* mode);
int gmb_team_set_link(const char* team, const char* which, const char* cfg);
int gmb_team_set_thoughts(const char* team, int n);
int gmb_team_set_dag(const char* team, int d, int l, int n);
int gmb_team_set_complexity(const char* team, double threshold);
int gmb_team_set_reflection(const char* team, const char* mode);
int gmb_team_set_critic(const char* team, const char* personas, const char* judge);
int gmb_team_set_prune(const char* team, int topk);
int gmb_team_train_thinking(const char* team, const char* dataset);
int gmb_team_train_actor_critic(const char* team, const char* dataset);
int gmb_team_distill_thinking(const char* team, const char* dataset);
int gmb_team_broadcast(const char* team, const char* message);
int gmb_team_redefine(const char* team, const char* definition);
int gmb_team_message(const char* team, const char* msg);
int gmb_team_list(char out[][64], int max);
int gmb_team_agents(const char* team, char out[][64], int max);
char* gmb_team_list_str(void);
char* gmb_team_agents_str(const char* team);
char* gmb_team_run(const char* team, const char* task);
char* gmb_team_run_start(const char* team, const char* task);
char* gmb_team_run_wait(const char* handle);
int gmb_team_on_stream(const char* sub);
double gmb_brainvaultbuild(const char* outputDir);
const char* gmb_brainvaultbuild_error(void);
const char* gmb_team_status(const char* team);
int gmb_team_save(const char* team, const char* path);
int gmb_team_load(const char* team, const char* path);

/* R4/R5 porting (RecursiveMAS 2026-08-28): training + funzioni read */
int gmb_team_train_inner(const char* team, const char* agent, const char* dataset);
int gmb_team_train_outer(const char* team, const char* dataset);
int gmb_team_get_recursion(const char* team);
const char* gmb_team_get_latent(const char* team);
char* gmb_team_train_status_str(const char* team);
char* gmb_team_link_info_str(const char* team);
char* gmb_team_runtime_stats_str(const char* team);

/* --- AI registry (gmb_ai.c) --- */
int gmb_mem_store(const char* key, const char* value, int longTerm);
char* gmb_mem_load(const char* key);   /* malloc (port SQLite :memory:, AgentMemoryStore) */
char* gmb_mem_search(const char* pattern);
int gmb_mem_delete(const char* key);
char* gmb_mem_list(void);
int gmb_provider_register(const char* role, const char* name, const char* kind, const char* config);
int gmb_provider_register_kind(const char* role, const char* name, const char* kind, const char* config);  /* forma funzione: qualunque kind (parità C# Register) */
int gmb_provider_set_active(const char* role, const char* name);
/* Come gmb_provider_set_active ma marca l'ESPLICITEZZA (AI SET ACTIVE dal BASIC): rende
 * legittimo il fixture embedding. Il bootstrap del host usa gmb_provider_set_active. */
int gmb_provider_set_active_explicit(const char* role, const char* name);
/* Nome del provider embedding pinnato dall'ultimo SET ACTIVE (parità C# EmbeddingProviderResolver.Default). */
const char* gmb_provider_embedding_snapshot(void);
const char* gmb_provider_active(const char* role);
/* Path del modello locale MLX dell'provider COMPLETION attivo (`.gguf` o cartella repo MLX),
 * oppure NULL. Auto-wire del brain MLX nei runtime C (parità routing host C#). */
const char* gmb_provider_active_model_path(void);
/* L'embedding provider è stato impostato ESPLICITAMENTE (AI SET ACTIVE %EMBEDDING)?
 * Rende legittimo il fixture embedding (il fixture DEFAULT no) — parità IlRuntime P0. */
int gmb_provider_embedding_explicit(void);
const char* gmb_provider_rotate(const char* role);

/* --- Blackboard condiviso (gmb_blackboard.c) — parita' Basic.Agents.Blackboard (C#) ---
 * Area chiave/valore concorrente (mutex+cond) per coordinamento tra goroutine/agenti.
 * write last-write-wins (1=ok); read "" se assente (malloc); wait fino a timeout_ms
 * (malloc, bloccante: non da un worker GPM — il join corretto e' il WAITGROUP). */
int gmb_blackboard_write(const char* key, const char* value);
char* gmb_blackboard_read(const char* key);
char* gmb_blackboard_wait(const char* key, int timeout_ms);
int gmb_blackboard_remove(const char* key);
void gmb_blackboard_clear(void);
int gmb_blackboard_count(void);

/* --- Categorie di task → provider/modello (gmb_category.c) — Orchestrazione agenti (A1).
 * Parita' Basic.AI.Providers.CategoryRegistry (C#). POLICY: categoria → provider + fallback,
 * routing automatico on/off. set/fallback: 1=ok, 0=err; pick/pick_fallback: malloc ("" se
 * assente); list: JSON deterministico (malloc); clear: test/harness. Thread-safe. */
int gmb_category_set(const char* category, const char* provider);
int gmb_category_fallback(const char* category, const char* provider);
int gmb_category_auto(int flag);
char* gmb_category_pick(const char* category);
char* gmb_category_pick_fallback(const char* category);
int gmb_category_auto_enabled(void);
char* gmb_category_list(void);
void gmb_category_clear(void);

/* --- Task Graph con dipendenze (gmb_taskgraph.c) — Orchestrazione agenti (B1).
 * Parita' Basic.Agents.TaskGraphRegistry (C#). define/depends/assign: 1=ok, 0=err;
 * status/list/run: JSON malloc (run esegue il DAG in ordine topologico, sequenziale
 * GPM-safe, delegando all'agente assegnato via gmb_agent_ask). clear: test/harness. */
int gmb_task_define(const char* id, const char* desc);
int gmb_task_depends(const char* id, const char* dep);
int gmb_task_assign(const char* id, const char* agent);
char* gmb_task_status(const char* id);
/* Task graph — API per il MANAGER loop (AI WORKER MANAGE): prossimo task lavorabile (dipendenze
 * soddisfatte, ordine alfabetico) e mutazione di stato/risultato. Speculari al C#. */
int   gmb_task_next_pending(char* id_out, size_t id_cap, char* desc_out, size_t desc_cap, int* deadlock);
/* Variante con FILTRO (CSV di id ammessi, NULL = tutti): scoping del manager loop su un piano. */
int   gmb_task_next_pending_filtered(const char* only_csv, char* id_out, size_t id_cap,
                                     char* desc_out, size_t desc_cap, int* deadlock);
int   gmb_task_set_status(const char* id, const char* status, const char* result);
char* gmb_task_list(void);
char* gmb_task_run(void);
void gmb_task_clear(void);

/* --- Piani (Planner) con approvazione opzionale (gmb_plan.c) — Orchestrazione agenti (A2).
 * Parita' Basic.Agents.PlanRegistry (C#). define/add/approve: 1=ok, 0=err; status/list: JSON
 * malloc; run: esegue il Task Graph associato SOLO se approvato (JSON malloc). clear: test. */
int gmb_plan_define(const char* name, const char* goal);
int gmb_plan_add(const char* name, const char* task_id);
int gmb_plan_approve(const char* name);
char* gmb_plan_status(const char* name);
/* Piano — API per lo SCOPING del manager loop: CSV dei task del piano e flag di approvazione. */
int   gmb_plan_tasks(const char* name, char* csv_out, size_t cap);
int   gmb_plan_is_approved(const char* name);
char* gmb_plan_list(void);
char* gmb_plan_run(const char* name);
void gmb_plan_clear(void);

/* --- Lifecycle Hooks (gmb_hook.c) — Orchestrazione agenti (A3). Parita'
 * Basic.Agents.HookRegistry (C#). register/unregister: 1=ok, 0=err (evento sconosciuto);
 * list/events/fire: JSON malloc (fire risolve gli handler dell'evento). clear: test. */
int gmb_hook_register(const char* event, const char* handler);
int gmb_hook_unregister(const char* event, const char* handler);
char* gmb_hook_list(void);
char* gmb_hook_events(void);
char* gmb_hook_fire(const char* event, const char* payload);
void gmb_hook_clear(void);

/* --- AI VERIFY ... REQUIRE (gmb_verify.c) — Orchestrazione agenti (B2). Parita'
 * IlRuntime.AiVerify (C#): retry con azione "COMPILE <path>" (compilatore C via weak symbol
 * gmb_compile_bas_file) e REQUIRE valutato con RESULT$ (gmb_expr_eval_ex). Ritorna malloc:
 * risultato accettato, oppure "Error: ...". */
char* gmb_verify_run(const char* action, const char* require, int max_retries);
/* Come gmb_verify_run ma con i LOCALI DEL CHIAMANTE (nome→valore): il REQUIRE li vede
 * (parità C# MacroEvalExprWithLocals — REQUIRE con variabili in scope al sito di AI VERIFY). */
char* gmb_verify_run_ex(const char* action, const char* require, int max_retries, gmb_dict_t* locals);
/* Stessa cosa con i locali in due array paralleli (nomi, valori): usata dai backend che
 * emettono C (C99Native), dove costruire un dict inline nel punto di chiamata è fragile.
 * names[i] NULL/"" viene ignorato; ritorna malloc (liberare con free). */
char* gmb_verify_run_named(const char* action, const char* require, int max_retries,
                           int n_locals, const char* const* names, const gmb_val_t* vals);
/* Valutatore del REQUIRE con RESULT$ = result: 1 soddisfatto, 0 no (condiviso con il loop
 * verificato del worker: AI WORKER LOOP ... REQUIRE ...). */
int   gmb_verify_require(const char* require, const char* result);
/* Come gmb_verify_require ma con i locali del chiamante. */
int   gmb_verify_require_ex(const char* require, const char* result, gmb_dict_t* locals);

/* --- Artifact Store (gmb_artifact.c) — Orchestrazione agenti (A6). Parita'
 * Basic.Agents.ArtifactStore (C#). put/link/remove: 1=ok, 0=err; get/kind: malloc ("" se
 * assente); list: JSON malloc. Il contenuto non è mai duplicato: solo riferimenti. */
int gmb_artifact_put(const char* name, const char* path);
int gmb_artifact_link(const char* name, const char* uri);
char* gmb_artifact_get(const char* name);
char* gmb_artifact_kind(const char* name);
int gmb_artifact_remove(const char* name);
char* gmb_artifact_list(void);
void gmb_artifact_clear(void);

/* --- Resource Manager (gmb_resource.c) — Orchestrazione agenti (A7). Parita'
 * Basic.Agents.ResourceRegistry (C#). set: 1=ok, 0=err; get: malloc ("" se assente);
 * available: 1/0; list: JSON malloc; swarm_size: min(n, max_agents). clear: test. */
int gmb_resource_set(const char* key, const char* value);
char* gmb_resource_get(const char* key);
int gmb_resource_available(const char* key);
char* gmb_resource_list(void);
double gmb_resource_swarm_size(double n);
void gmb_resource_clear(void);

/* --- Handoff (gmb_handoff.c) — Orchestrazione agenti (A9). Parita'
 * Basic.Agents.HandoffRegistry (C#). set/complete: 1=ok, 0=err; status/list: JSON malloc;
 * clear: numero di handoff rimossi. */
int gmb_handoff_set(const char* task_id, const char* to_agent);
int gmb_handoff_complete(const char* task_id, const char* result);
char* gmb_handoff_status(const char* task_id);
char* gmb_handoff_list(void);
int gmb_handoff_clear(void);

/* --- Rules injection gerarchica (gmb_rules.c) — Orchestrazione agenti (A5). Parita'
 * Basic.Agents.RuleRegistry (C#). add/remove: 1=ok, 0=err; for: regole per path (malloc,
 * gerarchico root→leaf); list: JSON malloc; clear: numero scope rimossi. */
int gmb_rules_add(const char* scope, const char* content);
int gmb_rules_remove(const char* scope);
char* gmb_rules_list(void);
char* gmb_rules_for(const char* path);
int gmb_rules_clear(void);

/* --- Goal persistente + idle continuation (gmb_goal.c) — Orchestrazione agenti (A4). Parita'
 * Basic.Agents.GoalRegistry (C#). set/clear: 1=ok, 0=err/nessuno; get: malloc ("" se nessuno);
 * status: JSON malloc; tick: impulso di continuazione (passi se attivo, 0 altrimenti). */
int gmb_goal_set(const char* objective);
int gmb_goal_agent(const char* name);
char* gmb_goal_get(void);
char* gmb_goal_status(void);
double gmb_goal_tick(void);
int gmb_goal_clear(void);

/* --- Session Replay (gmb_replay.c) — Orchestrazione agenti (A8). Parita'
 * Basic.Agents.ReplayRegistry (C#). start/record/stop/clear: 1=ok, 0=err; get/list: JSON malloc. */
int gmb_replay_start(const char* session_id);
int gmb_replay_record(const char* event);
int gmb_replay_stop(void);
char* gmb_replay_get(const char* session_id);
char* gmb_replay_list(void);
int gmb_replay_clear(void);
int gmb_replay_run(const char* session_id, const char* target_id);
/* E1: riesecuzione AUTOMATICA degli eventi (valutatore runtime condiviso) con esiti registrati
 * nella sessione "<sessione>#auto". Ritorna il numero di eventi rieseguiti OK. */
double gmb_replay_auto(const char* session_id);

/* --- Metriche fleet-level (gmb_metrics.c) — Orchestrazione agenti (B6). Parita'
 * Basic.Agents.MetricsRegistry (C#). record: 1=ok, 0=err; get: somma per agente+chiave;
 * list/fleet_stats: JSON malloc; reset: numero agenti rimossi. */
int gmb_metric_record(const char* agent, const char* key, double value);
double gmb_metric_get(const char* agent, const char* key);
char* gmb_metric_list(void);
char* gmb_fleet_stats(void);
int gmb_metric_reset(void);
/* §3.6 PARSEC: metriche TASK standard (tokens/ttft/tool_calls/llm_calls/latency) come JSON malloc. */
char* gmb_task_metrics(const char* task);

/* --- Hashline / hash-anchored edit (gmb_hashline.c) — Orchestrazione agenti (A10). Parita'
 * Basic.Agents.HashlineTool (C#): tag `N#HASH| riga`, edit con verifica hash. */
char* gmb_hashline_tag(const char* content);
char* gmb_hashline_edit(const char* content, const char* line_ref, const char* new_text);
int gmb_hashline_verify(const char* content, const char* line_ref);

/* --- Checkpoint/recovery (gmb_checkpoint.c) — Orchestrazione agenti (B7). Parita'
 * Basic.Agents.CheckpointRegistry (C#). save/clear: 1=ok / numero; resume: malloc ("" se assente);
 * list: JSON malloc. */
int gmb_checkpoint_save(const char* session_id, const char* state);
char* gmb_checkpoint_resume(const char* session_id);
char* gmb_checkpoint_list(void);
int gmb_checkpoint_clear(void);

/* --- Persistenza orchestrazione (gmb_orchestration.c) — SQLite su file (orch_kv).
 * Salva/riprende Artifact/Handoff/Metrics/Checkpoint/Replay. Parita' C# OrchestrationPersistence. */
char* gmb_artifact_export(void);
void gmb_artifact_import(const char* state);
char* gmb_handoff_export(void);
void gmb_handoff_import(const char* state);
char* gmb_metric_export(void);
void gmb_metric_import(const char* state);
char* gmb_checkpoint_export(void);
void gmb_checkpoint_import(const char* state);
char* gmb_replay_export(void);
void gmb_replay_import(const char* state);
char* gmb_category_export(void);
void gmb_category_import(const char* state);
char* gmb_resource_export(void);
void gmb_resource_import(const char* state);
char* gmb_rules_export(void);
void gmb_rules_import(const char* state);
char* gmb_task_export(void);
void gmb_task_import(const char* state);
char* gmb_plan_export(void);
void gmb_plan_import(const char* state);
char* gmb_hook_export(void);
void gmb_hook_import(const char* state);
int gmb_orch_save(const char* path);
int gmb_orch_load(const char* path);
char* gmb_provider_list(const char* role);
const char* gmb_provider_normalize_role(const char* role); /* %CHAT/"embedding"/... → ruolo canonico */
int gmb_provider_unregister(const char* role, const char* name);
int gmb_provider_unregister_name(const char* name);
int gmb_provider_set_weight_name(const char* name, int weight);
char* gmb_provider_test(const char* role, const char* name);
int gmb_provider_set_weight(const char* role, const char* name, int weight);
const char* gmb_provider_weight(const char* role, const char* name);
double gmb_provider_weight_name(const char* name);
int gmb_provider_fail_name(const char* name);
int gmb_provider_reset_failed_name(const char* name);
int gmb_provider_set_status_name(const char* name, const char* status);
const char* gmb_provider_status_name(const char* name);
char* gmb_ai_info_fmt(const char* name);
const char* gmb_provider_kind(const char* name);
 int gmb_provider_run_healthcheck(void);
 int gmb_provider_set_priority(const char* role, const char* name, int pri);
 double gmb_provider_priority_name(const char* name);
 int gmb_provider_set_priority_name(const char* name, int pri);
 int gmb_provider_config(const char* role, const char* name, const char* kind, const char* config);
 int gmb_provider_configure_name(const char* name, const char* key, const char* value);
 const char* gmb_provider_priority(const char* role, const char* name);
 int gmb_ai_priority(const char* name);
 int gmb_ai_set_priority(const char* name, int pri);
 int gmb_provider_register_local(const char* path);
 int gmb_provider_healthcheck_interval(const char* role, const char* name);
 int gmb_provider_set_healthcheck_interval(const char* role, const char* name, int minutes);
 int gmb_provider_start_healthcheck(const char* role, const char* name);
 int gmb_provider_stop_healthcheck(const char* role, const char* name);
int gmb_provider_fail(const char* role, const char* name);
int gmb_provider_reset_failed(const char* role);
int gmb_provider_set_status(const char* role, const char* name, int status);
int gmb_provider_status(const char* role, const char* name);
int gmb_aiv_ensure_collection(const char* db, const char* name, int dims);
int gmb_aiv_ensure_collection_fmt(const char* db, const char* name, int dims, const char* format);  /* format: 'fp32'|'sq8' */
int gmb_aiv_index_source(const char* db, const char* coll, const char* source_id, const char* text);
int gmb_aiv_index_meta(const char* db, const char* coll, const char* source_id, const char* text, const char* metadata);
int gmb_aiv_index_text(const char* db, const char* coll, const char* text);
int gmb_aiv_delete_text(const char* db, const char* coll, const char* source_id);
int gmb_aiv_count(const char* db, const char* coll);
int gmb_aiv_purge_deprecated(const char* db, const char* coll);
char* gmb_aiv_search(const char* db, const char* coll, const char* query, int topK);
char* gmb_aiv_ask(const char* db, const char* coll, const char* question, int topK);
char* gmb_aiv_list_collections(const char* db);
int gmb_aiv_remove_source(const char* db, const char* coll, const char* source_id);

/* gmb_vectordb.c — GenTest (R3): genera testset sintetico da collection. Ritorna malloc-ed
 * JSON con messaggio di esito (path + conteggio + sha256); il chiamante libera. */
char* gmb_vectordb_gentest(const char* db, const char* coll, const char* n_str, const char* types);

/* gmb_rag.c — RAG paritario col C# (SqlVectorSchema/SqlVectorIndexer/SqlVectorSearch/
 * SqlVectorFts), §0bis runtime condiviso: STESSO schema di AiVectorRuntime (vec_documents
 * esteso + vec_chunks + vec_embeddings per chunk + fts_chunks + vec_query_audit), così C# e C
 * condividono gli stessi file rag/<name>.db. VM/C e C99 (via emitter) chiamano queste funzioni. */
int64_t gmb_rag_open(const char* sqlite_path);              /* handle */
int     gmb_rag_close(int64_t h);
int64_t gmb_rag_ensure_collection(const char* db, const char* name, int dims, const char* metric, const char* model_version);
int64_t gmb_rag_ensure_collection_fmt(const char* db, const char* name, int dims, const char* metric, const char* model_version, const char* format);
int     gmb_rag_embedding_available(void);   /* 1 se l'operazione vettoriale può girare (modello o fixture opt-in), 0 = parità C# throw → -1 */
int     gmb_rag_index_source(const char* db, const char* coll, const char* source_id, const char* text,
                             const char* metadata_json, int max_chars, int overlap);  /* 1 indexed|2 updated|0 skipped|-1 */
int     gmb_rag_delete_source(const char* db, const char* coll, const char* source_id);
int     gmb_rag_purge_deprecated(const char* db, const char* coll);   /* n rimossi, -1 errore */
int     gmb_rag_count(const char* db, const char* coll);
char*   gmb_rag_list_collections(const char* db);
char*   gmb_rag_search(const char* db, const char* coll, const char* query, int topk, const char* filter_json);        /* hybrid */
char*   gmb_rag_search_vector(const char* db, const char* coll, const char* query, int topk, const char* filter_json);  /* vector-only */
/* gmb_rag_search_sel: AIVECTOR.SEARCH con knob (reranker OFF/KEYWORD, minEvidence, explain) —
 * fonte unica VM/C + C99 (§0bis); errori espliciti per ONNX/explain/filtri non combinabili. */
char*   gmb_rag_search_sel(const char* db, const char* coll, const char* query, int topk,
                           int hybrid, const char* filter_json, int rerank,
                           double min_evidence, int explain);
/* gmb_rag_context_sel / gmb_rag_ask_sel: RAGCONTEXT/ASK con knob — solo KEYWORD default nel
 * motore C; reranker non-default o minEvidence finita -> errore esplicito. Ritorno malloc. */
char*   gmb_rag_context_sel(const char* db, const char* coll, const char* query, int topk,
                            int rerank, double min_evidence);
char*   gmb_rag_ask_sel(const char* db, const char* coll, const char* question, int topk,
                        int rerank, double min_evidence);
char*   gmb_rag_context(const char* db, const char* coll, const char* query, int topk, const char* filter_json);       /* grounded context */
char*   gmb_rag_ask(const char* db, const char* coll, const char* question, int topk, const char* filter_json);          /* grounded LLM ask */
int     gmb_rag_embedding_is_fixture(void);
int     gmb_rag_selfcheck(void);

/* gmb_rag_eval.c — RAG eval: quartetto retrieval (P@K/R@K/MRR/NDCG, rilevanza binaria,
 * formule IR standard) + giudizi generazione tier-1 (euristiche deterministiche su token
 * ASCII lowercase). §0bis: fonte unica VM/C + C99; il canone gestito è AiVectorRuntime.cs
 * e DEVE restare paritario. ID = CSV (trim, vuoti scartati, case-insensitive).
 * gmb_rageval_selfcheck: 0 = OK (valori handbook esatti per costruzione). */
double gmb_rageval_precision_at(const char* retrieved, const char* ideal, int k);
double gmb_rageval_recall_at(const char* retrieved, const char* ideal, int k);
double gmb_rageval_mrr(const char* retrieved, const char* ideal);
double gmb_rageval_ndcg_at(const char* retrieved, const char* ideal, int k);
double gmb_rageval_faithfulness(const char* context, const char* answer);
double gmb_rageval_relevance(const char* question, const char* answer);
double gmb_rageval_hallucination(const char* context, const char* answer);
double gmb_rageval_completeness(const char* question, const char* answer);
/* L7 Training: BLEU-4 standard non smussato + ROUGE-L (F, beta=1) nella variante GMBasic
 * (token [a-z0-9]+ lowercase, case-insensitive, singolo riferimento — NON sacrebleu). */
double gmb_bleu_score(const char* ref_text, const char* hyp_text);
double gmb_rouge_l(const char* ref_text, const char* hyp_text);
int    gmb_rageval_selfcheck(void);

/* gmb_rageval_advanced.c — R4-R8: diagnosi, Cohen kappa, Wilson CI, calibrazione soglia,
 * valutazione gate/diff, PSI, z-test, Welch t-test, confronto multi-config.
 * Pure math/string, no SQLite/LLM. Tutti i char* ritorni sono mallocati (caller free). */
char*   gmb_rageval_diagnose(const char* eval_json);
char*   gmb_rageval_diagnose_report(void);
double  gmb_rageval_kappa(const char* cases_json);
double  gmb_rageval_wilson_ci_low(const char* cases_json);
double  gmb_rageval_wilson_ci_high(const char* cases_json);
double  gmb_rageval_calibrate_threshold(const char* cases_json);
char*   gmb_rageval_eval_gate(const char* baseline, const char* current);
char*   gmb_rageval_eval_diff(const char* baseline, const char* current);
double  gmb_rageval_psi(const char* baseline, const char* window);
double  gmb_rageval_z_test(const char* variantA, const char* variantB);
/* A/B test su proporzioni: input {"n","success"} → JSON {"z_score","p_value","effect_size"}
 * (contratto guida §8.8; parità con AiVectorRuntime.AbTest). */
char*   gmb_rageval_ab_test(const char* variantA, const char* variantB);
double  gmb_rageval_welch_t(const char* variantA, const char* variantB);
char*   gmb_rageval_compare(const char* configs_json);
int     gmb_rageval_advanced_selfcheck(void);

/* Combined calibrate: JSON {kappa, wilson_ci_low/high, threshold}. */
char*   gmb_rageval_calibrate(const char* cases_json);
/* Baseline capture: salva e ritorna JSON con sample.*/
char*   gmb_rageval_baseline(const char* sample_json);

int gmb_mcp_register_tool(const char* name);
/* Registrazione completa (parità IlRuntime.McpRegisterTool C#): proc/desc/readOnly
 * + inputSchema JSON derivato a compile-time dai parametri della FUNCTION. */
int gmb_mcp_register_tool_full(const char* name, const char* proc, const char* desc, int readonly, const char* schema_json);
/* Registrazione con outputSchema opzionale (parità McpToolDefinition.OutputSchema C#):
 * output_schema_json "" → non emesso in tools/list. Unica implementazione reale, di cui
 * _full è wrapper (§0bis). */
int gmb_mcp_register_tool_ex(const char* name, const char* proc, const char* desc, int readonly,
                             const char* schema_json, const char* output_schema_json);
int gmb_mcp_unregister_tool(const char* name);
char* gmb_mcp_list_tools(void);
int gmb_mcp_register_agent(const char* name);
/* Accessor del registro (usati dal server MCP in gmb_mcp_server.c). */
int gmb_mcp_tool_exists(const char* name);
int gmb_mcp_tool_is_readonly(const char* name);
const char* gmb_mcp_tool_proc(const char* name);
/* inputSchema JSON del tool ("{}" se non dichiarato). Serve all'executor dei mondi C
 * per ricostruire NOME e TIPO degli argomenti nell'ordine dichiarato dalla procedura
 * (il chunk P-Code non porta i tipi dei parametri). Puntatore a buffer interno: NON
 * liberare, valido finché il tool non viene ri-registrato. */
const char* gmb_mcp_tool_schema(const char* name);

/* ---- MCP server (gmb_mcp_server.c): JSON-RPC 2.0 su stdio/HTTP ---- */
/* Hook di esecuzione: invoca la PROCEDURA BASIC `proc_name` con argomenti JSON.
 * Ogni mondo C registra il proprio (VM/C, C99Native) — il runtime condiviso
 * NON conosce l'engine di esecuzione (§0bis). out_text è mallocato (caller free).
 * Ritorna 1 = invocata (anche con errore applicativo, segnalato via *out_is_error), 0 =
 * procedura non trovata/engine assente. */
#ifndef GMB_MCP_EXECUTOR_FN_DEFINED
#define GMB_MCP_EXECUTOR_FN_DEFINED
typedef int (*gmb_mcp_executor_fn)(void* ctx, const char* tool_name, const char* proc_name,
                                   const char* args_json, char** out_text, int* out_is_error);
#endif
void gmb_mcp_set_executor(gmb_mcp_executor_fn fn, void* ctx);
/* Modalità read-only del server (pattern MCP-001): 1 = rifiuta i tool non READONLY.
 * `gmb_mcp_serve` la imposta dal proprio argomento; esposta anche per configurazione
 * esplicita (server avviato altrove / selfcheck). */
void gmb_mcp_set_readonly(int readonly);
/* Processa UNA richiesta JSON-RPC e ritorna la risposta JSON (heap, caller free).
 * Ritorna NULL per le notifiche (nessuna risposta dovuta). */
char* gmb_mcp_handle_json(const char* request_json);
/* Avvia il server MCP. mode = "stdio" (blocca su stdin fino a EOF) | "http" (porta).
 * readOnly=1 → rifiuta i tool non dichiarati READONLY. Ritorna 1 ok, 0 errore. */
int gmb_mcp_serve(const char* mode, int port, int readonly);
/* Ferma il server HTTP (no-op per stdio). */
void gmb_mcp_stop(void);
int gmb_dbf_selected(const char* alias);
const char* gmb_dbf_alias(const char* alias);
int gmb_dbf_used(const char* alias);
int gmb_dbf_lastrec(const char* alias);
int gmb_dbf_eof(void);
int gmb_dbf_bof(void);
int gmb_dbf_found(void);
int gmb_dbf_deleted(const char* alias);

char* gmb_path_app_dir(void);
char* gmb_path_project_dir(void);
char* gmb_path_parent(const char* path);
char* gmb_path_temp_file(void);
char* gmb_path_temp_dir(void);
char* gmb_path_resolve(const char* path);
void gmb_path_set_entry(const char* path);
void gmb_path_set_default(const char* path);
void gmb_path_set_search(const char* joined);
void gmb_path_set_current_file(const char* path);
char* gmb_path_search_paths(void);
char* gmb_path_bin_dir(void);
char* gmb_path_current_file_dir(void);
char* gmb_path_resolve_db(const char* path);
char* gmb_path_find_folder(const char* name, int max_depth);
char* gmb_path_resolve_file(const char* path);
uint8_t* gmb_read_all_bytes(const char* path, size_t* len_out);
char** gmb_read_all_lines(const char* path, int* count_out);
long gmb_count_lines(const char* path);
gmb_int_t gmb_val_length(gmb_val_t v);

/* --- FFI dl condiviso (gmb_ffi_dl.c): dlopen/dlsym usato da C99, VM/C e host IL --- */
void* gmb_ffi_dl_open(const char* path);      /* dlopen + estensioni + match basename */
void* gmb_ffi_dl_sym(void* lib, const char* name);
void  gmb_ffi_dl_preload(const char* abs_path); /* registra una lib di pacchetto risolta */
/* --- scipy.optimize (gmb_optfit.c): curve_fit (LM) e differential_evolution --- */
void gmb_lm_fit(double (*model)(const double*, int, double, void*), void* ctx,
                const double* x, const double* y, int nx,
                const double* p0, int np, int maxiter, double tol, double* out);
void gmb_lm_fit_vec(void (*vmodel)(const double*, int, const double*, int, double*, void*), void* vctx,
                    const double* x, const double* y, int nx, const double* p0, int np, int maxiter, double tol, double* out);
void gmb_de_minimize(double (*fn)(const double*, int, void*), void* ctx,
                     const double* lo, const double* hi, int n,
                     int np, int maxiter, double tol, double* out);
/* --- Trading PsoOptimizer (common/CommonLib/Trading/PsoOptimizer.cs:307) ---
 * returns: matrice [periodi x asset] row-major; objective 0=CVaR 1=CDaR.
 * out: pesi ottimali (somma 1, non negativi). Deterministico (seed fisso). */
void gmb_opt_pso(const double* returns, int nSim, int nAssets, double alpha, int objective,
                 int swarmSize, int maxIter, double* out);
/* --- C99 bridge per i builtin optimize closure-based (gmb_fn_t = lambda transpilata) --- */
double* gmb_opt_de_c99(gmb_fn_t fn, gmb_double_t** bounds, int n, gmb_int_t* outn);
double* gmb_opt_lmfit_c99(gmb_fn_t fn, const double* x, int nx, const double* y, int ny,
                          const double* p0, int np, gmb_int_t* outn);
double* gmb_opt_lmfit_vec_c99(gmb_fn_t fn, const double* xs, const double* ys, int n, const double* p0, int np, gmb_int_t* outn);
/* --- KDTree (gmb_kdtree.c): scipy.spatial.KDTree --- */
gmb_int_t gmb_kdtree_create(const double* flat, int n, int m);
int gmb_kdtree_query(gmb_int_t h, const double* q, int k, int* out_idx);
int gmb_kdtree_query_dist(gmb_int_t h, const double* q, int k, double* out_d);
int gmb_kdtree_query_ball(gmb_int_t h, const double* q, double r, int max_out, int* out_idx);
double* gmb_kdtree_query_indices_alloc(gmb_int_t h, const double* q, int k, gmb_int_t* outn);
double* gmb_kdtree_query_dists_alloc(gmb_int_t h, const double* q, int k, gmb_int_t* outn);
double* gmb_kdtree_query_ball_alloc(gmb_int_t h, const double* q, double r, gmb_int_t* outn);
/* --- scipy.optimize completato (gmb_optfit.c) --- */
double gmb_opt_brentq(double (*fn)(double, void*), void* ctx, double a, double b);
double gmb_opt_brenth(double (*fn)(double, void*), void* ctx, double a, double b);
double gmb_opt_ridder(double (*fn)(double, void*), void* ctx, double a, double b);
double gmb_opt_toms748(double (*fn)(double, void*), void* ctx, double a, double b);
double gmb_opt_brentmin(double (*fn)(double, void*), void* ctx, double lower, double upper);
void gmb_opt_powell(double (*fn)(const double*, int, void*), void* ctx, const double* x0, int n, double* out);
void gmb_opt_tnc(double (*fn)(const double*, int, void*), void* ctx, const double* x0, int n, const double* lo, const double* hi, double* out);
void gmb_opt_cobyla(double (*fn)(const double*, int, void*), void* ctx, const double* x0, int n, const double* lo, const double* hi, double* out);
void gmb_opt_slsqp(double (*fn)(const double*, int, void*), void* ctx, const double* x0, int n, const double* lo, const double* hi, double* out);
void gmb_opt_fsolve(void (*fn)(const double*, int, double*, void*), void* ctx, const double* x0, int n, double* out);
void gmb_opt_broyden(void (*fn)(const double*, int, double*, void*), void* ctx, const double* x0, int n, double* out);
void gmb_opt_least_squares(void (*fn)(const double*, int, double*, void*), void* ctx, const double* x0, int n, int m, const double* lo, const double* hi, double* out);
void gmb_opt_linprog(const double* c, int n, const double** aub, const double* bub, int mub,
                     const double** aeq, const double* beq, int meq,
                     const double* lo, const double* hi, double* out);
void gmb_opt_brute(double (*fn)(const double*, int, void*), void* ctx, const double* lo, const double* hi, int n, int N, double* out);
void gmb_opt_basin_hopping(double (*fn)(const double*, int, void*), void* ctx, const double* x0, int n, int niter, double* out);
void gmb_opt_dual_annealing(double (*fn)(const double*, int, void*), void* ctx, const double* lo, const double* hi, int n, int maxiter, double* out);
/* --- Bridge C99 optimize (957-973) --- */
double gmb_opt_brentq_c99(gmb_fn_t fn, double a, double b);
double gmb_opt_brent_c99(gmb_fn_t fn, double a, double b);
double gmb_quad_gk_c99(gmb_fn_t fn, double a, double b);
double gmb_simpson_c99(gmb_fn_t fn, double a, double b, int n);
double gmb_opt_brenth_c99(gmb_fn_t fn, double a, double b);
double gmb_opt_ridder_c99(gmb_fn_t fn, double a, double b);
double gmb_opt_toms748_c99(gmb_fn_t fn, double a, double b);
double gmb_opt_brentmin_c99(gmb_fn_t fn, double a, double b);
double gmb_opt_bisection_c99(gmb_fn_t fn, double a, double b);
double gmb_opt_secant_c99(gmb_fn_t fn, double a, double b);
double gmb_opt_golden_c99(gmb_fn_t fn, double a, double b);
double* gmb_opt_cg_c99(gmb_fn_t fn, const double* x0, int n, gmb_int_t* outn);
double gmb_opt_minscalar_c99(gmb_fn_t fn, double a, double b, gmb_string_t method);
double* gmb_opt_powell_c99(gmb_fn_t fn, double* x0, int n, gmb_int_t* outn);
double* gmb_opt_tnc_c99(gmb_fn_t fn, double* x0, int n, gmb_double_t** bounds, gmb_int_t* outn);
double* gmb_opt_cobyla_c99(gmb_fn_t fn, double* x0, int n, gmb_double_t** bounds, gmb_int_t* outn);
double* gmb_opt_slsqp_c99(gmb_fn_t fn, double* x0, int n, gmb_double_t** bounds, gmb_int_t* outn);
double* gmb_opt_minimize_c99(gmb_fn_t fn, double* x0, int n, gmb_string_t method, gmb_double_t** bounds, gmb_int_t* outn);
double* gmb_opt_brute_c99(gmb_fn_t fn, gmb_double_t** ranges, int n, int N, gmb_int_t* outn);
double* gmb_opt_basin_c99(gmb_fn_t fn, double* x0, int n, int niter, gmb_int_t* outn);
double* gmb_opt_dual_c99(gmb_fn_t fn, gmb_double_t** bounds, int n, int maxiter, gmb_int_t* outn);
/* --- scipy.stats P0 (gmb_stats.c) --- */
gmb_val_t gmb_pearsonr(gmb_val_t x, gmb_val_t y);
gmb_val_t gmb_spearmanr(gmb_val_t x, gmb_val_t y);
gmb_val_t gmb_linregress(gmb_val_t x, gmb_val_t y);
gmb_val_t gmb_chi2con_flat(const double* t, int nr, int nc);
gmb_val_t gmb_chi2con_jagged(const double** rows, int nr, int nc);
gmb_val_t gmb_pearsonr_flat(const double* x, int nx, const double* y, int ny);
gmb_val_t gmb_spearmanr_flat(const double* x, int nx, const double* y, int ny);
gmb_val_t gmb_linregress_flat(const double* x, int nx, const double* y, int ny);
gmb_val_t gmb_mannwhitney_flat(const double* x, int nx, const double* y, int ny);
gmb_val_t gmb_wilcoxon_flat(const double* x, int nx, const double* y, int ny);
gmb_val_t gmb_skewtest_flat(const double* a, int n);
gmb_val_t gmb_kurtosistest_flat(const double* a, int n);
gmb_val_t gmb_jarquebera_flat(const double* a, int n);
gmb_val_t gmb_describe_flat(const double* a, int n);
gmb_val_t gmb_mannwhitney(gmb_val_t x, gmb_val_t y);
gmb_val_t gmb_wilcoxon(gmb_val_t x, gmb_val_t y);
gmb_val_t gmb_skewtest(gmb_val_t data);
gmb_val_t gmb_kurtosistest(gmb_val_t data);
gmb_val_t gmb_jarquebera(gmb_val_t data);
gmb_val_t gmb_describe(gmb_val_t data);
double gmb_mode(const double* a, int n);
double gmb_moment(const double* a, int n, int order);
double gmb_iqr(const double* a, int n);
/* --- scipy.stats.gamma (gmb_math.c) --- */
double gmb_gamma_pdf(double shape, double scale, double x);
double gmb_gamma_cdf(double shape, double scale, double x);
double gmb_gamma_ppf(double shape, double scale, double p);
double gmb_gamma_isf(double shape, double scale, double p);
/* --- scipy.signal P0 (gmb_filters.c) --- */
double* gmb_firwin(int numtaps, const double* cutoff, int ncut, const char* btype, const char* window);
double* gmb_find_peaks(const double* x, int n, double distance, int* outn);
/* --- scipy.integrate P0 (gmb_ode.c) --- */
double* gmb_solve_ivp_rk45(void (*f)(double, const double*, int, double*, void*), void* ctx,
    const double* y0, int yd, double t0, double t1, double rtol, double atol, int* out_rows);
/* --- scipy.signal P1 (gmb_filters.c) --- */
double* gmb_welch(const double* x, int n, int nperseg, double fs, int* out_rows);
double* gmb_savgol_filter(const double* x, int n, int window, int polyorder, gmb_int_t* outn);
double* gmb_resample(const double* x, int n, int num, gmb_int_t* outn);
double* gmb_decimate(const double* x, int n, int q, gmb_int_t* outn);
/* --- scipy.fft P1 (gmb_math.c) --- */
double* gmb_rfft(const double* x, int n, gmb_int_t* outn);
double* gmb_irfft(const double* re, const double* im, int m, gmb_int_t* outn);
/* --- scipy.interpolate P1 (gmb_interp.c) --- */
double* gmb_splrep(const double* x, const double* y, int n, gmb_int_t* out_len);
double* gmb_splev(const double* spline, int slen, const double* xq, int nq, gmb_int_t* outn);
/* --- scipy.spatial P1 (gmb_spatial.c) --- */
double* gmb_pdist_flat(const double* X, int n, int d, const char* metric, gmb_int_t* outn);
double* gmb_cdist_flat(const double* X, int nx, int dx, const double* Y, int ny, int dy,
    const char* metric, int* out_rows, int* out_cols);
double* gmb_distance_matrix_flat(const double* X, int nx, int dx, const double* Y, int ny, int dy,
    int* out_rows, int* out_cols);
double* gmb_convex_hull_flat(const double* X, int n, int stride, gmb_int_t* outn);
/* --- Bridge C99 P0/P1 --- */
gmb_int_t gmb_welch_c99(const double* x, int n, int nperseg, double fs);
double* gmb_savgol_c99(const double* x, int n, int wl, int po, gmb_int_t* outn);
double* gmb_resample_c99(const double* x, int n, int num, gmb_int_t* outn);
double* gmb_decimate_c99(const double* x, int n, int q, gmb_int_t* outn);
gmb_int_t gmb_rfft_c99(const double* x, int n);
double* gmb_irfft_c99(const double* re, const double* im, int m, gmb_int_t* outn);
double* gmb_firwin_c99(int numtaps, const double* cutoff, int ncut, const char* btype, const char* window, gmb_int_t* outn);
double* gmb_splrep_c99(const double* x, const double* y, int n, gmb_int_t* outn);
double* gmb_splev_c99(const double* spline, int slen, const double* xq, int nq, gmb_int_t* outn);
double* gmb_pdist_c99(gmb_double_t** X, int n, int d, const char* metric, gmb_int_t* outn);
gmb_int_t gmb_cdist_c99(gmb_double_t** X, int nx, int dx, gmb_double_t** Y, int ny, int dy, const char* metric);
gmb_int_t gmb_distance_matrix_c99(gmb_double_t** X, int nx, int dx, gmb_double_t** Y, int ny, int dy);
double* gmb_convex_hull_c99(gmb_double_t** X, int n, gmb_int_t* outn);
double* gmb_find_peaks_alloc(const double* x, int n, double distance, gmb_int_t* outn);
/* --- Bridge C99 vettore→vettore (fsolve/least_squares/solve_ivp) --- */
double gmb_opt_newton_c99(gmb_fn_t fn, double guess);
double* gmb_opt_fsolve_c99(gmb_fn_t fn, double* x0, int n, gmb_int_t* outn);
double* gmb_opt_lstsq_c99(gmb_fn_t fn, double* x0, int n, int m, gmb_double_t** bounds, gmb_int_t* outn);
double* gmb_opt_linprog_c99(const double* c, int n, const double** aub, const double* bub, int mub,
    const double** aeq, const double* beq, int meq, const double** bounds, gmb_int_t* outn);
gmb_int_t gmb_solve_ivp_c99(gmb_fn_t fn, double* y0, int n, double t0, double t1);
gmb_val_t gmb_anova_flat(const double* data, const int* sizes, int k);
gmb_val_t gmb_anova_jagged(const double* const* rows, const int* sizes, int k);
/* Argomenti da riga di comando (salvati da main via gmb_set_command_args). */
void gmb_set_command_args(int argc, char** argv);
int gmb_command_count(void);
const char* gmb_command_arg(int i);
/* TSMINMAX: [min, max] della serie. */
gmb_double_t* gmb_ts_minmax(gmb_double_t* data, gmb_int_t n, gmb_int_t* out_len);
/* Rolling skewness/kurtosis/median/quantile/sharpe (finestra scorrevole). */
gmb_double_t* gmb_ts_rolling_skewness(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_kurtosis(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_median(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_quantile(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double q, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_sharpe(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, gmb_int_t* out_len);
gmb_double_t* gmb_ts_rolling_sortino(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, double target, gmb_int_t* out_len);
/* TSROLLMINMAX: coppie [min,max] per finestra → 2*len. */
gmb_double_t* gmb_ts_rolling_minmax(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
/* TSROLLREGRESSIONSINGLE: beta/alpha per finestra → 2*len. */
gmb_double_t* gmb_ts_rolling_regression_single(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len);
/* PCA: analisi componenti principali (Jacobi su matrice di covarianza centrata).
   data = rows×cols (righe osservazioni), out = matrice cols×cols (autovettori per colonna),
   out_rows = cols, out_cols = cols; eigenvalues in *eigs (cols). */
gmb_double_t** gmb_pca(const double* data, int rows, int cols, gmb_int_t* out_rows, gmb_int_t* out_cols, double* eigs);
gmb_val_t gmb_pca_dict(const double* data, int rows, int cols);
/* TSCOVARIANCEMATRIX: matrice di covarianza k×k di k serie (denominatore n). */
gmb_double_t** gmb_ts_covariance_matrix(const double* const* series, int k, int n);
gmb_double_t** gmb_ts_covariance_obs(const double* const* obs, int n_obs, int n_vars);
/* MOUSE getters (MOUSE.BUTTON/WHEEL/DBLCLICK/PRESENT/MIDDLE): stato mouse TUI corrente
 * (0 in headless). Implementati in gmb_tui_clipper.c, condivisi da VM/C,
 * C99Native (§0bis). */
gmb_double_t gmb_mouse_button(void);
gmb_double_t gmb_mouse_wheel(void);
gmb_double_t gmb_mouse_middle(void);
gmb_double_t gmb_mouse_present(void);
gmb_double_t gmb_mouse_dblclick(void);
/* MOUSE.LEFT/RIGHT/MOVE (booleani canonici -1/0), 2026-09-22: mancavano nel runtime
 * condiviso (VMC non instradava 399/401/402 → sempre 0). */
gmb_double_t gmb_mouse_left(void);
gmb_double_t gmb_mouse_right(void);
gmb_double_t gmb_mouse_move(void);
/* MOUSE.X()/MOUSE.Y() — posizione mouse (pixel) dal sottosistema grafico (0 headless). */
gmb_double_t gmb_mouse_x(void);
gmb_double_t gmb_mouse_y(void);
/* WINDOWS.X/Y/THEME/DESKTOP — stato window manager globale (headless: -1/-1/"Turbo"/1).
 * Implementati in gmb_tui_windowmanager.c, condivisi da VM/C, C99Native. */
gmb_double_t gmb_window_x(void);
gmb_double_t gmb_window_y(void);
gmb_double_t gmb_window_desktop(void);
const char* gmb_window_theme(void);
double gmb_tm_ulcer_index(const double* r, int n);
double* gmb_tm_jarque_bera(const double* r, int n, gmb_int_t* out_len); /* [stat, pval] */
double gmb_tm_expected_return(const double* w, const double* r, int n);
double gmb_tm_is_long_only(const double* w, int n);
double gmb_fin_portfolio_sharpe(double portfolio_return, double portfolio_stdev, double risk_free);
double gmb_tm_mad(const double* r, int n);
double gmb_tm_semimad(const double* r, int n);
/* TRADING.PORTFOLIO.MAD/SEMIMAD: scarto medio assoluto dalla MEDIANA (parità IL
 * PortfolioMad/PortfolioSemiMad, VM/C gmb_bf_196). Diverso da gmb_tm_mad (media). */
double gmb_tm_portfolio_mad(const double* r, int n);
double gmb_tm_portfolio_semimad(const double* r, int n, double target);
double gmb_tm_binomial_option(double s, double k, double t, double r, double v, int steps, int is_call);
double gmb_tm_omega_ratio(const double* r, int n, double threshold);
double gmb_tm_portfolio_var(const double* const* hist, int assets, int periods, const double* w, double conf);
double gmb_tm_portfolio_cvar(const double* const* hist, int assets, int periods, const double* w, double conf);
/* Analisi (dict val-returning, una sorgente per l'emitter C99): costruiscono il dict
   risultato nel runtime, niente statement-expr inline nel transpiler. */
gmb_val_t gmb_adf_test_result(const double* data, int n);
gmb_val_t gmb_holt_winters_result(const double* y, int n, double alpha, double beta, double gamma, int period);
gmb_val_t gmb_arima_result(const double* y, int n, int p, int d, int q);
gmb_val_t gmb_kmeans_result(const double* data, int n, int k);
/* SOLVEODE: ODE scalare dy/dt=f(t,y) con lambda gmb_fn_t → array di steps+1 punti. */
double* gmb_solve_ode_c99(gmb_fn_t fn, double y0, double t0, double t1, int steps, gmb_int_t* outn);
/* JSON path extract: estrae un valore (chiave o chiave[i]) da una stringa JSON.
   Ritorna stringa (scalare) o sottostruttura JSON serializzata; "" se path mancante. */
char* gmb_json_extract(const char* js, const char* path);
gmb_val_t gmb_json_extract_val(const char* js, const char* path);   /* variante TIPIZZATA (PRINT) */
/* Yule-Walker: stima coefficienti AR(p) da autocorrelazione r[0..n-1]. */
double* gmb_yule_walker(const double* r, int n, int p, gmb_int_t* out_len);
/* FFT complessa da vettori re/imm separati → matrice n×2 (col0 re, col1 imm). */
gmb_double_t** gmb_fft_complex_matrix(const double* re, const double* im, int n, int inverse);
/* MA(q): coefficienti media mobile da residui (autocorrelazione normalizzata). */
double* gmb_ma_coefficients(const double* res, int n, int q, gmb_int_t* out_len);
/* Distanza euclidea al quadrato tra due vettori. */
double gmb_euclidean_distance_squared(const double* a, const double* b, int n);

/* bzip2 / brotli (gmb_bzip2.c) + base64 (gmb_encoding.c) — per COMPRESSION.* */
unsigned char* gmb_bzip2_compress(const unsigned char* data, size_t len, size_t* out_len);
unsigned char* gmb_bzip2_decompress(const unsigned char* data, size_t len, size_t* out_len);
unsigned char* gmb_brotli_compress(const unsigned char* data, size_t len, size_t* out_len);
unsigned char* gmb_brotli_decompress(const unsigned char* data, size_t len, size_t* out_len);
char* gmb_base64_encode(const uint8_t* data, size_t len);
/* AI.SHAREENCRYPT/DECRYPT — base64(AES(content, key)), parità ProvenanceManager. */
char* gmb_share_encrypt(const char* content, const char* key);
char* gmb_share_decrypt(const char* cipher_b64, const char* key);
uint8_t* gmb_base64_decode(const char* input, size_t* len_out);
char* gmb_base64url_encode(const uint8_t* data, size_t len);
uint8_t* gmb_base64url_decode(const char* input, size_t* len_out);
char* gmb_html_encode(const char* input);
char* gmb_url_encode(const char* input);
char* gmb_js_encode(const char* input);
char* gmb_jwt_create(const char* claims, const char* secret);
int gmb_jwt_verify(const char* token, const char* secret);
char* gmb_jwt_payload(const char* token);
char* gmb_upload_safe_name(const char* input);
const char* gmb_upload_detect_type(const uint8_t* data, size_t len);
/* Session store nativo (HTTP.SESSIONCREATE / GMBSESSION). */
char* gmb_session_create(const char* user, const char* role);
int gmb_session_verify(const char* id);
int gmb_session_get(const char* id, char* user_out, size_t user_cap, char* role_out, size_t role_cap);
/* Validation core (parità CommonLib.SecurityValidation). */
int gmb_validation_required(const char* s);
int gmb_validation_email(const char* s);
int gmb_validation_url(const char* s);
int gmb_validation_credit_card(const char* s);
int gmb_validation_length(const char* s, int min_len, int max_len);

/* Memory-mapped files (suite MEMORYMAP.* — gmb_file.c). */
int64_t gmb_mmap_open(const char* path);
int64_t gmb_mmap_peek(int64_t h, int64_t offset);
int gmb_mmap_poke(int64_t h, int64_t offset, int value);
int gmb_mmap_write_bytes(int64_t h, int64_t offset, const unsigned char* src, int count);
/* MMAPWRITEBYTES da un array BYTE nativo del mondo C (gmb_int_t*, un elemento per
 * byte, troncato a 8 bit): i due emitter C rappresentano DIM x() AS BYTE come
 * gmb_int_t[], quindi un cast diretto a `unsigned char*` leggerebbe i byte grezzi
 * dell'intero (8 per elemento). Unica implementazione per C99Native. */
int gmb_mmap_write_bytes_i64(int64_t h, int64_t offset, const int64_t* src, int count);
/* Variante per C99Native (array BYTE = gmb_double_t* in quel mondo). */
int gmb_mmap_write_bytes_f64(int64_t h, int64_t offset, const double* src, int count);
int gmb_mmap_read_bytes(int64_t h, int64_t offset, unsigned char* out, int count);
int gmb_mmap_close(int64_t h);
gmb_list_t gmb_mmap_read_bytes_list(int64_t h, int64_t offset, int count);

const char* gmb_ai_get_config(const char* name, const char* key);
int gmb_ai_set_config(const char* name, const char* key, const char* value);
/* Registro opzioni generico condiviso (superficie BASIC `AI SET OPTION`).
 * Una voce = un parametro; i domini la registrano con `gmb_opt_register`. */
int gmb_opt_register(const char* name, const char* desc, const char* def, void (*on_set)(const char*));
int gmb_opt_attach(const char* name, void (*on_set)(const char*));
int gmb_opt_set(const char* name, const char* value);
int gmb_opt_get(const char* name, char* out, int cap);
int gmb_opt_count(void);
int gmb_opt_at(int i, const char** name, const char** desc, char* val, int cap);
char* gmb_opt_get_str(const char* name);
/* Helper di timing condiviso (tempo monotono in millisecondi). */
double gmb_dbg_now_ms(void);
/* Ponte di transizione env→registro: registro se impostato, altrimenti getenv. */
const char* gmb_opt_env(const char* name);
/* Settings condivisi (sezione "engine" di system/app.settings.json, come SettingsManager):
 * globali (dir eseguibile) → override app (dir applicazione). */
int gmb_opts_load(const char* path);
void gmb_opts_load_hierarchy(void);
int gmb_opts_save(const char* path);
int gmb_opt_set_persist(const char* name, const char* value);
char* gmb_ai_info(const char* name);
int gmb_ai_set_default(const char* name);
const char* gmb_ai_get_default(const char* role);
double gmb_ai_get_credits(const char* name);
int gmb_ai_set_credits(const char* name, double credits);
double gmb_provider_credits(const char* name);
int gmb_provider_is_free(const char* name);
double gmb_provider_cost_per_m(const char* name);
int gmb_provider_record_completion(const char* name, double tokens);
int gmb_provider_enforce_credits(const char* name);
char* gmb_provider_v2_info(void);
char* gmb_ai_providers_json(void);   /* AI.PROVIDERS(): contenuto di config/providers.json */
int gmb_provider_v2_save(const char* path);
int gmb_provider_v2_load(const char* path);


int gmb_mcp_server_add(const char* name, const char* kind, const char* source);
int gmb_mcp_load(const char* path);
int gmb_mcp_server_remove(const char* name);
int gmb_mcp_server_set_enabled(const char* name, int enabled);
char* gmb_mcp_server_list(void);
char* gmb_mcp_server_status(const char* name);
char* gmb_mcp_server_marketplace_list(void);
char* gmb_mcp_list_tools_server(const char* server);
char* gmb_mcp_call(const char* server, const char* tool, const char* args);
char* gmb_mcp_prompts(void);
char* gmb_mcp_resources(void);

#include "gmb_network.h"

gmb_val_t gmb_rgb(double r, double g, double b, double a);
int gmb_rgb_to_int(gmb_val_t v);
int gmb_rgb_to_line_color(gmb_val_t v);
int gmb_color_from_val(gmb_val_t v);
char* gmb_ansi_fg(const char* text, int r, int g, int b);
char* gmb_ansi_bg(const char* text, int r, int g, int b);
void gmb_rgb_fill(gmb_dict_t* d, double r, double g, double b, double a);

gmb_list_t gmb_agent_list_names(void);
gmb_string_t gmb_agent_list_str(void);
gmb_list_t gmb_agent_events_names(void);
gmb_list_t gmb_provider_list_names(const char* role);

gmb_string_t gmb_compress_string(const char* algo, gmb_string_t s);
gmb_string_t gmb_decompress_string(const char* algo, gmb_string_t s);

int gmb_type_register(const char* name, const char* fields_json, const char* methods_json);
int gmb_type_register_udt(const char* name, int count);
int gmb_type_udt_field(const char* name, const char* field, long offset, int type);
int gmb_type_udt_field_ex(const char* name, const char* field, long offset, int type, const char* elem);
int gmb_type_udt_lookup(const char* udt, const char* field, long* out_offset, int* out_type);
void gmb_type_udt_read_field(const uint8_t* base, long off, int type, char* out, size_t cap);
int gmb_type_udt_first_field(const char* udt, const char** out_name, long* out_offset, int* out_type);
int gmb_type_udt_field_at(const char* udt, int idx, char* out_name, int namecap, long* out_offset, int* out_type, char* out_elem, int elemcap);
gmb_dict_t* gmb_type_udt_to_dict(const char* udt, const uint8_t* base);
char* gmb_type_properties(const char* name);
char* gmb_type_methods(const char* name);
char* gmb_type_member(const char* name, const char* member, int is_method);

char* gmb_ai_rerank(const char* query, const char* json_results, int top_k);

/* Config AI (porting da IlRuntime AiConfig + ProviderConfig): log, HF, save/load. */
int gmb_ai_set_log_path(const char* path);
char* gmb_ai_analyze_image(const char* path, const char* prompt);
const char* gmb_ai_log_path(void);
int gmb_ai_log_write(const char* file, const char* text, int newline);
int gmb_ai_log_write_line(const char* file, const char* text);
const char* gmb_ai_set_hf_token(const char* value);
const char* gmb_ai_set_hf_repo(const char* value);
const char* gmb_ai_hf_repo(void);
int gmb_ai_save_config(void);
int gmb_ai_load_config(void);
/* AI.SAVE/AI.LOAD registro provider (417/418): parità ProviderServer.Save/Load. */
int gmb_provider_save_registry(const char* path);
int gmb_provider_load_registry(const char* path);

/* AI PATH / AI SKILLS (porting da IlRuntime AiConfig + SkillIndex): registry in-memory. */
int gmb_ai_path_model_add(const char* p);
int gmb_ai_path_model_remove(const char* p);
int gmb_ai_path_skill_add(const char* p);
int gmb_ai_path_skill_remove(const char* p);
int gmb_ai_skill_add(const char* p);
int gmb_ai_skill_remove(const char* name);
int gmb_ai_skill_refresh(void);
int gmb_ai_skill_set_index(const char* p);
const char* gmb_ai_skill_index_path(void);
const char* gmb_ai_skills_csv(void);
/* Array nativo di stringhe per C99Native (DIM x() AS STRING = AI.SKILLSLIST/PATHS): split dei
 * registry in-memory in un array gmb_string (parità IlRuntime AiSkillsList → string[]). */
gmb_string_t* gmb_ai_skills_list(gmb_int_t* out_n);
gmb_string_t* gmb_ai_skills_paths(gmb_int_t* out_n);
/* Forma SCALARE `[a, b]` della lista skill/paths (parità oracolo IL, che stampa l'array fra
 * parentesi quadre con ", "): usata da C99Native per `PRINT AI.SKILLSLIST()`.
 * gmb_ai_skills_csv() è newline-joined e senza parentesi → non adatta a questa forma. */
const char* gmb_ai_skills_list_str(void);
const char* gmb_ai_skills_paths_str(void);

int llmengine_plugin_register(const char* name, const char* kind, const char* config);
int llmengine_plugin_create(const char* name, const char* desc);
int llmengine_plugin_add(const char* path);   /* AI PLUGINS ADD: copia da percorso sorgente (parità C#) */
int llmengine_plugin_set_enabled(const char* name, int enabled);
gmb_list_t llmengine_plugin_list_names(void);
gmb_list_t llmengine_plugin_list_registry(void);   /* AI.PLUGINLIST(): registro in-process (parità C#) */
int llmengine_plugin_registered(const char* name); /* 1 se il plugin è nel registro in-process */
char* llmengine_plugin_info(const char* name);
int gmb_plugins_scan(char names[][64], int max);
/* MatchByTrigger (parità PluginIndex.MatchByTrigger C#): 1 se il prompt matcha il trigger
 * di un plugin noto. Usato da AI AGENT ASK per il tool-loop "solo quando serve". */
int llmengine_plugin_match_trigger(const char* prompt);
char* llmengine_plugin_call(const char* name, const char* arg);
/* Hook di invocazione reale della procedura del plugin (installato dal VM/C, che ha
 * vm+task e risolve il nome con gmb_vm_proc_addr). NULL = risultato dichiarativo. */
typedef char* (*llmengine_plugin_call_fn)(const char* name, const char* arg, void* ctx);
void llmengine_plugin_set_call_hook(llmengine_plugin_call_fn fn, void* ctx);

gmb_list_t gmb_list_from_double_array(const double* arr, gmb_int_t n);
gmb_list_t gmb_list_from_string_array(char** arr, int n);

uint8_t* gmb_sha256_alloc(const uint8_t* data, size_t len, size_t* len_out);
char* gmb_sha256_hex(const uint8_t* data, size_t len);
char* gmb_md5_hex(const uint8_t* data, size_t len);
char* gmb_sha1_hex(const uint8_t* data, size_t len);
char* gmb_sha512_hex(const uint8_t* data, size_t len);
gmb_string_t gmb_digest_raw(gmb_int_t algo, const uint8_t* data, size_t len);
/* HASHFILE/STRHASHFILE: hash di un file (byte[] / hex string). */
gmb_string_t gmb_hashfile_val(const char* path, const char* algo);
gmb_string_t gmb_strhashfile_val(const char* path, const char* algo);
gmb_string_t gmb_bytes_fromhex(gmb_string_t hex);

gmb_list_t gmb_string_split(const char* text, const char* sep);

gmb_double_t* gmb_clone_double_array(const double* arr, gmb_int_t n);
int gmb_all_non_negative(const double* arr, gmb_int_t n);
int gmb_agent_register_tool(const char* name, const char* tool);
int gmb_agent_permission_default(const char* level);
const char* gmb_agent_permission_default_get(void);
int gmb_agent_permission(const char* name, const char* level);
int gmb_agent_tool_policy(const char* name, const char* tool, const char* policy);
/* L1 Queue/ACF — AgentQueueManager (C) */
int gmb_agent_queue_create(const char* agent);
int gmb_agent_queue_send(const char* agent, const char* from, const char* task, const char* input, double payment);
char* gmb_agent_queue_receive(const char* agent);
char* gmb_agent_queue_stats(const char* agent);
gmb_string_t gmb_agent_queue_receive_s(const char* agent);
gmb_string_t gmb_agent_queue_stats_s(const char* agent);
int gmb_agent_queue_clear(const char* agent);
int gmb_agent_queue_pause(const char* agent);
int gmb_agent_queue_resume(const char* agent);
int gmb_agent_queue_stop(const char* agent);
int gmb_agent_queue_is_paused(const char* agent);
int gmb_agent_queue_is_stopped(const char* agent);
int gmb_agent_queue_count(const char* agent);
/* L1 MessageBus — canali */
int gmb_bus_channel_create(const char* channel);
int gmb_bus_channel_publish(const char* channel, const char* message);
int gmb_bus_publish_ex(const char* channel, const char* from, const char* task, const char* input);
int gmb_channel_subscribe(const char* channel, void (*handler)(const char*, const char*, const char*, const char*, void*), void* userdata);
int gmb_channel_unsubscribe(const char* channel, void (*handler)(const char*, const char*, const char*, const char*, void*), void* userdata);
int gmb_channel_subscriber_count(const char* channel);
int gmb_channel_has_subscribers(const char* channel);
void gmb_channel_clear(const char* channel);
/* L1 Fabric */
int gmb_agent_fabric_create(const char* agent_id);
int gmb_agent_send(const char* agent, const char* from, const char* task, const char* input);
char* gmb_agent_receive(const char* agent);
int gmb_agent_stop(const char* agent);
int gmb_agent_pause(const char* agent);
int gmb_agent_resume(const char* agent);
int gmb_agent_fabric_select(const char* const* agents, int n, char* out, int cap, int timeout_ms);
int gmb_agent_insert_trigger_register(const char* db, const char* coll, const char* agent, const char* prompt);
int gmb_agent_insert_trigger_unregister(const char* db, const char* coll);
long gmb_agent_insert_trigger_fired(const char* db, const char* coll);
int gmb_agent_insert_trigger_fire(const char* db, const char* coll, const char* source, const char* text);
void gmb_agent_insert_trigger_clear(void);
int gmb_kg_add_node(const char* node);
int gmb_kg_add_edge(const char* from, const char* to);
char* gmb_kg_query(const char* q);
gmb_string_t gmb_string_take(char* heap);
int gmb_ai_register_ocr_backend(void* load, void* read_image, void* read_audio, void* free_s, void* unload);
int gmb_ai_ocr_load_model(const char* model, const char* mmproj);
const char* gmb_ai_ocr_read(const char* path, const char* prompt, int max_tokens);
const char* gmb_ai_audio_transcribe(const char* path, const char* prompt, int max_tokens);
void gmb_ai_ocr_unload(void);
int gmb_aiv_load_documents(const char* db, const char* coll, const char* path, const char* pattern);
int gmb_ai_register_embed_backend(void* load, void* unload, void* dim, void* text);
int gmb_ai_embed_load(const char* model, int n_gpu_layers);
int gmb_ai_embed_dim(void);
int gmb_ai_embed_text(const char* text, float* out, int cap);
void gmb_ai_embed_unload(void);
double* gmb_aiv_embed(const char* text, int dims, gmb_int_t* outn);

/* gmb_image.c — F3 multimodale (2026-09-11): primitive PNG condivise (1 = ok, 0 = errore
 * via gmb_image_error()). Solo PNG in decode e encode: altri formati = errore esplicito. */
const char* gmb_image_error(void);
int gmb_image_info(const char* path, int* w, int* h, char* fmt);
const char* gmb_image_info_str(const char* path);
int gmb_image_resize(const char* in, const char* out, int w, int h);
int gmb_image_crop(const char* in, const char* out, int x, int y, int w, int h);
int gmb_image_convert(const char* in, const char* out);

/* F5 multimodale (2026-09-11): embedding percettivo (64 double unitari, lista vuota su
 * errore), compare 0..100 (-1 su errore), detect/classify via provider vision. */
gmb_list_t gmb_image_embed_list(const char* path);
int gmb_image_compare(const char* p1, const char* p2);
const char* gmb_image_detect(const char* path);
const char* gmb_image_classify(const char* path, const char* labels);

/* gmb_docfields.c — F2 multimodale (2026-09-11): estrazione strutturata documenti → JSON
 * (buffer statico come gmb_ai_ocr_read: i backend copiano, nessun free). Errori come
 * {"error":"..."}: number/date/total/vat deterministici (offline), schema via LLM %CHAT. */
const char* gmb_docfields_json(const char* path);
const char* gmb_docextract_json(const char* path, const char* schema);

/* gmb_tts.c — F4 multimodale (2026-09-11): TTS via motore di sistema. Scrive un WAV in
 * `output`; 1 = ok, 0 = errore (argomenti vuoti, motore assente). Mai stub. */
int gmb_tts_synthesize(const char* text, const char* output);

/* gmb_brain.c — porting Basic.AI.MemoryBrain (SQLite ~/.gmbasic/brain.db). */
long gmb_brain_add(const char* text, const char* kind, const char* source, int dims);
int gmb_brain_forget(long id);
char* gmb_brain_search(const char* query, int topK, int dims);
/* gmb_brain_search_ex: search con reranker keyword + soglia di evidenza (P1-2b, parità
 * gestito). rerank: 0=OFF (solo ranking base, evidence=-1), 1=KEYWORD (seconda passata:
 * riordina per evidenza keyword; se min_evidence finita scarta le hit sotto soglia e
 * imposta *insufficient=1 quando nessuna resta), 2=%RERANK_ONNX o explain!=0 → *err esplicito
 * e ritorno NULL (mai no-op). Formato riga: "id|kind|score|evidence|text" (text può
 * contenere '|'). Ritorno malloc. */
char* gmb_brain_search_ex(const char* query, int topK, int dims, int rerank,
                          double min_evidence, int explain, char** err, int* insufficient);
/* gmb_ai_sbm_json: output JSON SEARCHMULTI (brain singolo "vm") con reranker/evidence —
 * fonte unica (§0bis) per VM/C e C99. Status Success/Error/InsufficientEvidence. Ritorno malloc. */
char* gmb_ai_sbm_json(const char* query, int topK, int dims, int rerank, double min_evidence, int explain);
/* gmb_ai_sbm_json_multi: SEARCHMULTI multi-brain (parità MultiBrainSearch C#). brains_csv =
 * lista CSV di nomi (o NULL → i collegati); collection = collezione RAG. */
char* gmb_ai_sbm_json_multi(const char* query, int topK, const char* collection,
                            const char* brains_csv, int rerank, double min_evidence, int explain);
/* gmb_ai_sbm_ctx: contesto text per RAGCONTEXTMULTI/ASKMULTI; su errore/insufficiente ritorna
 * il JSON d'errore (prefisso {"status":). Ritorno malloc. */
char* gmb_ai_sbm_ctx(const char* query, int topK, int dims, int rerank, double min_evidence);
long gmb_brain_import_file(const char* path, int dims);
int gmb_brain_add_watch(const char* path);
char* gmb_brain_watched_csv(void);
int gmb_brain_refresh(int dims);

int llmengine_plugin_remove(const char* name);
int llmengine_plugin_refresh(void);
int llmengine_plugin_set_index(const char* name);
char* gmb_ai_skills_prompt(void);
int gmb_ai_skills_marketplace(const char* url, const char* dest);
int gmb_ai_plugins_marketplace(const char* url, const char* dest);
int gmb_mcp_server_refresh(void);
char* gmb_mcp_server_marketplace_get(const char* name);
char* gmb_provider_discover(const char* prefix, const char* capability, const char* base_url, const char* api_key, int solo_free);
char* gmb_process_document(const char* file, const char* prompt);


/* gmb_file.c — FILE_LOCK/UNLOCK (fcntl POSIX) + fd access. */
int gmb_file_get_fd(gmb_int_t handle);
int gmb_file_lock(gmb_int_t handle, long position, long length);
int gmb_file_unlock(gmb_int_t handle, long position, long length);

/* gmb_iowatch.c — IO WATCH (polling mtime, porting FileSystemWatcher). */
int gmb_iowatch_add(const char* path);
int gmb_iowatch_remove(const char* path);
int gmb_iowatch_poll(void);
const char* gmb_iowatch_first_changed(char* buf, size_t cap);
int gmb_iowatch_add_cb(const char* path, const char* callback);
const char* gmb_iowatch_callback(const char* path);
const char* gmb_iowatch_first_changed_cb(char* path_buf, size_t path_cap, char* cb_buf, size_t cb_cap);
int gmb_iowatch_add_fn(const char* path, void (*fn)(void*));
const char* gmb_iowatch_first_changed_fn(char* path_buf, size_t path_cap, void (**fn_out)(void*));
int gmb_iowatch_count(void);

/* gmb_secondbrain.c — porting Basic.AI.Knowledge (VaultManager/LinkManager/Ingestor).
 * Folder props e link manuali su ~/.gmbasic/vaults.db (stesso file di C#); sync via file copy;
 * web scrape via gmb_net_http_get. Fonte unica per VM/C e C99 (§0bis). */
char* gmb_net_http_get(const char* url);
/* client HTTP semplificato GET/POST/PUT/DELETE → body (malloc) o NULL (gmb_network.c). */
char* gmb_net_http_body(const char* method, const char* url, const char* body);
/* provenance (gmb_crypto.c): firma/verifica HMAC-SHA256 — ri-esposta qui per il C99Native. */
char* gmb_provenance_sign(const uint8_t* content, size_t content_len, const char* key, size_t key_len);
int   gmb_provenance_verify(const uint8_t* content, size_t content_len, const char* key, size_t key_len,
                            const char* signature_hex);
uint8_t* gmb_aes_encrypt_password(const uint8_t* data, size_t len, const char* password, size_t* out_len);
uint8_t* gmb_aes_decrypt_password(const uint8_t* enc, size_t len, const char* password, size_t* out_len);
uint8_t* gmb_base64_decode(const char* input, size_t* len_out);
int   gmb_vault_folder_add(const char* vault_name, const char* folder, int indexed, const char* tags);
int   gmb_vault_add_folder_bool(const char* vault_name, const char* folder, int indexed, const char* tags);
int   gmb_vault_folder_remove(const char* vault_name, const char* folder);
int   gmb_vault_folder_set(const char* vault_name, const char* folder, int* indexed, const char* tags);
int   gmb_vault_folder_set_str(const char* vault_name, const char* folder, const char* indexed, const char* tags);
char* gmb_vault_folders_csv(const char* vault_name);
int   gmb_note_link(const char* vault_name, const char* from, const char* to, const char* alias);
int   gmb_note_unlink(const char* vault_name, const char* from, const char* to);
int   gmb_note_set_alias(const char* vault_name, const char* from, const char* to, const char* alias);
char* gmb_links_csv(const char* vault_name);
char* gmb_sync_folder(const char* vault_name, const char* src, const char* direction, const char* conflict);
char* gmb_web_scrape(const char* vault_name, const char* url, const char* rel_path);
/* Vault info/Note (porting NoteManager/VaultManager): operano su <configDir>/vaults.db +
 * file .md nella root del vault. Ritornano JSON (info/folders/list) o stringa (note). */
char* gmb_vault_info_json(const char* vault_name);
char* gmb_vault_folders_json(const char* vault_name);
long  gmb_vault_count(const char* vault_name);
char* gmb_vault_list_json(void);
/* Vault create/index (porting VaultManager/VaultIndex C#, 2026-09-03 — parità C←C#):
 * il runtime C registra/indicizza il vault in autonomia (schema completo, content_hash
 * SHA256 hex UPPERCASE come Convert.ToHexString del C#). */
long long gmb_vault_create(const char* name, const char* root_path, const char* pattern); /* id o -1 */
long long gmb_vault_index(const char* name);                                              /* n file o -1 */
long long gmb_vault_index_file(const char* name, const char* file_path);                  /* 1|0|-1 */
long long gmb_vault_remove_file(const char* name, const char* file_path);                 /* righe o -1 */
int   gmb_note_create(const char* vault_name, const char* path, const char* content, const char* tags);
int   gmb_note_update(const char* vault_name, const char* path, const char* content);
int   gmb_note_append(const char* vault_name, const char* path, const char* text);
int   gmb_note_delete(const char* vault_name, const char* path);
int   gmb_note_move(const char* vault_name, const char* old_path, const char* new_path);
char* gmb_note_get(const char* vault_name, const char* path);
char* gmb_note_title(const char* vault_name, const char* path);
char* gmb_note_tags(const char* vault_name, const char* path);
long  gmb_note_wordcount(const char* vault_name, const char* path);
int   gmb_note_exists(const char* vault_name, const char* path);
char* gmb_note_folder_path(const char* vault, const char* path);
char* gmb_note_links_out_json(const char* vault, const char* path);
char* gmb_note_backlinks_json(const char* vault, const char* path);
char* gmb_links_all_json(const char* vault);
/* LINKJSON: grafo completo {"nodes","edges","dangling"} — parità IL LinkManager.ToJson. */
char* gmb_links_json_graph(const char* vault);
/* AI.LINKDANGLING reso come STRINGA (parità codegen IL). */
char* gmb_linkdangling(const char* vault);
long  gmb_links_dangling_count(const char* vault);
char* gmb_memory_rank(const char* query, int topK);
int   gmb_memory_consolidate(void);
int   gmb_vault_watch(const char* name, const char* policy);
char* gmb_vault_policy(const char* name);
/* DB RAG di un cervello: risoluzione (vendor → exe → brainsDir → cache) poi i fallback
 * storici (<brains>/<nome>/rag/<nome>.db, <ragDir>/<nome>.db). 1 se esiste. */
int   gmb_brain_rag_db(const char* name, char* out, size_t cap);
/* Export JSON del grafo di un cervello: risoluzione (vendor → exe → brainsDir → cache) poi la
 * cartella di BUILD configurata (paths.graphsDir, default ~/.gmbasic/graphs). 1 se esiste. */
int   gmb_brain_graph_json(const char* name, char* out, size_t cap);
int   gmb_brain_use(const char* names);
int   gmb_brain_detach(const char* name);
char* gmb_brain_used_csv(void);
/* AI BRAIN INSTALL nome, sorgente / AI BRAIN UNINSTALL nome (1 = ok, 0 = errore esplicito su
 * stderr). La logica vive in gmb_brains.c: manifest + gmb_fetch, condivisa dai 4 backend. */
gmb_double_t gmb_brain_install(const char* name, const char* source);
gmb_double_t gmb_brain_uninstall(const char* name);
/* ── INGEST (AI INGEST FILE/FOLDER, porting Ingestor — §0bis) ──
 * Importa un file esterno nel vault come nota .md (estensioni testo copiate, immagini OCR,
 * audio trascrizione, binari via converter registrato). Il vault "" risolve al brain attivo
 * (primo brain_attach), altrimenti errore esplicito (mai stub). ImportFile ritorna il path
 * relativo della nota creata (heap, free) o NULL su errore; ImportFolder ritorna il numero di
 * file importati o -1 su errore. Il converter binario è registrato dal chiamante (VM/C → wrapper
 * su gmb_doc_convert, pattern delegate C# BinaryTextExtractor): se nessun converter è registrato
 * e il file è binario, errore esplicito. */
char* gmb_ingest_import_file(const char* vault_name, const char* source_file, const char* rel_path, int ocr, int transcribe);
long long gmb_ingest_import_folder(const char* vault_name, const char* src_folder, int preserve_structure, int ocr, int transcribe);
typedef char* (*gmb_ingest_converter_fn)(const char* path, const char* fmt); /* heap o NULL */
void  gmb_ingest_register_converter(gmb_ingest_converter_fn fn);
/* provider OCR/audio attivi (speculare a ProviderServer.GetActiveName): 1 se il backend è
 * registrato e può OCRizzare/trascrivere, 0 altrimenti (l'ingest fallisce con errore esplicito). */
int   gmb_ai_ocr_active(void);
int   gmb_ai_audio_active(void);
/* LLM completion (Enrich/QA/BrainVaultWrite): wrapper su gmb_agent_ask (fallback fixture). */
char* gmb_llm_complete(const char* system, const char* prompt);
/* ENRICH — parità Basic.AI.Knowledge.Enricher.Enrich (prompt di dominio qui, non nel
 * layer HTTP): mode = "SUMMARIZATION" | "TAGGING" | altro → EntityExtraction. Testo
 * troncato a 8000 caratteri + "…", output trimmato. Senza completer ritorna
 * l'errore esplicito (lo produce gmb_llm_complete). Ritorno malloc. */
char* gmb_enrich(const char* text, const char* mode);

/* AI.QAANSWER — parità Enricher.QaAnswer: (question, brain, window). Ordine dei controlli
 * identico all'oracolo: domanda vuota -> brain non disponibile come rag -> evidenza
 * insufficiente -> errore provider -> JSON Success. */
char* gmb_qa_answer(const char* question, const char* brain, int window);
/* Brain registry (AI.BRAINLIST/INFO): nomi dal db vaults. */
char* gmb_brain_registry_list(void);
char* gmb_brain_used_csv_json(void);
char* gmb_brain_registry_info(const char* name);
char* gmb_brain_info_or_empty(const char* name); /* parità C#: sconosciuto → "{}", mai NULL */
/* WikiGraph loader (GRAPH query sui brain): JSON GraphExporter → gmb_graph handle. */
int64_t gmb_graph_load_wikijson(const char* js);
/* GRAPH BUILD <wikiDir> [, outputDir]: scansiona il vault e scrive <basename>.json. */
int64_t gmb_graph_build(const char* wikiDir, const char* outputDir);
/* Graph brain query: carica il WikiGraph del brain ed esegue op (NEIGHBORS/PATH/PAGERANK/
 * CONNECTED/STATS/SCC/TOPOSORT/BIPARTITE/BETWEENNESS/CLUSTERING/DIAMETER/ISVALID).
 * Ritorna JSON (o "1"/"0" per ISVALID/BIPARTITE). */
char* gmb_graph_brain_query(const char* brain, const char* op, const char* arg1, const char* arg2, int arg3);
const char* gmb_graph_active_name(void);
int gmb_graph_rdd_set(const char* backend);
/* GRAPH.QUERYMULTI(query, brains_csv): merge multi-brain (parità IL GraphMultiQuery.QueryMulti). */
char* gmb_graph_query_multi(const char* query, const char* brains_csv);

/* --- SHELL / process execution (gmb_shell.c) — SYSTEM.SHELL / SHELLOUT / SHELLCODE ---
 * Opt-in: gmb_shell_set_enabled(true) abilita (default: disabilitato). I backend C (VM/C,
 * C99Native) chiamano queste funzioni — MAI inline. Ritorno: exit code o stringa
 * allocata (l'allocazione è gmb_malloc, da liberare col gmb_free del runtime). */
void gmb_shell_set_enabled(int enabled);
int  gmb_shell_is_enabled(void);
int  gmb_shell_exec(const char* command);            /* SHELL: esegue, scarta output, ritorna exit */
char* gmb_shell_out(const char* command);            /* SHELLOUT: esegue, ritorna stdout+stderr */
int  gmb_shell_code(const char* command);            /* SHELLCODE: esegue, ritorna exit code */
int  gmb_shell_run(const char* command);             /* RUN "cmd": esegue, output ereditato, senza opt-in */

/* --- gmb_paths.c — risoluzione percorsi dati GMBASIC (MONDO C, §0bis: C e C# NON
 * condividono codice). Il runtime C scopre la root del repo (GMBASIC.sln risalendo dalla
 * cwd, override GMBASIC_ROOT) e legge la sezione "paths" di system.settings.json
 * (stesso file del C#: <root>/config/system.settings.json → <root>/Resources/
 * system.settings.json → cwd). Default identici al C# (GmbiPaths): le cartelle definitive
 * in root, i componenti runtime scaricati nella CACHE (<cache>/<comp>/<rid>), la cache in /tmp.
 * Ogni valore può essere assoluto o relativo alla root; ~ è espanso. --- */
int   gmb_paths_repo_root(char* out, size_t cap);              /* 1 se trovata root, 0 altrimenti */
int   gmb_paths_resources_dir(char* out, size_t cap);          /* Resources CONDIVISA (font/sprite/manifest): 1 se trovata */
int   gmb_paths_settings_file(char* out, size_t cap);          /* 1 se system.settings.json trovato */
int   gmb_paths_app_settings(char* out, size_t cap);           /* app.settings.json: CWD → accanto all'eseguibile */
/* Settings (system.settings.json + app.settings.json override) — SettingsManager.cs. */
int  gmb_settings_str(const char* file, const char* key, char* out, int cap);
int  gmb_settings_int(const char* file, const char* key, int* out);
void gmb_settings_apply_console_default(void);
int  gmb_settings_console_str(int index, const char* key, char* out, int cap);
int  gmb_settings_console_int(int index, const char* key, int* out);
void  gmb_paths_config_dir(char* out, size_t cap);             /* default <root>/config */
void  gmb_paths_skills_dir(char* out, size_t cap);             /* <root>/../shared/skills se esiste, altrimenti <root>/skills */
void  gmb_paths_plugins_dir(char* out, size_t cap);            /* default <root>/plugins */
void  gmb_paths_logs_dir(char* out, size_t cap);               /* default <root>/ai-logs */
void  gmb_paths_rag_dir(char* out, size_t cap);               /* shared/rag nel monorepo, altrimenti <root>/rag */
void  gmb_paths_graphs_dir(char* out, size_t cap);            /* shared/graphs nel monorepo, altrimenti <root>/graphs */
void  gmb_paths_brains_dir(char* out, size_t cap);             /* default <root>/brains */
void  gmb_paths_brains_exports_dir(char* out, size_t cap);     /* default <root>/brains-exports */
void  gmb_paths_brains_cache_dir(char* out, size_t cap);       /* cache cervelli on-demand: ~/.gmbasic/brains */
int   gmb_paths_brains_manifest(char* out, size_t cap);        /* brains-manifest.json: 1 se esiste */
/* File condiviso del compilatore: `shared/settings/<name>` (seed di provider, manifest). */
int   gmb_paths_shared_settings_file(const char* name, char* out, size_t cap);
void  gmb_paths_libs_dir(char* out, size_t cap);               /* default <root>/build/libs/<rid> */
void  gmb_paths_libs_cache_dir(char* out, size_t cap);          /* cache librerie on-demand: ~/.gmbasic/libs (paths.libsCacheDir) */

/* ── Download ON-DEMAND di UNA libreria (asset per-file) ───────────────────────────────────
 * `libs.txt` (TSV): nome-file \t sha256 \t size \t url. Emesso dal build accanto all'eseguibile
 * e usato da gmb_ffi_dl quando una libreria non si trova in nessun path: la scarica nella cache
 * (~/.gmbasic/libs/<rid>/) con verifica sha256 e poi la carica. Restituisce 1 se il file è
 * disponibile (già in cache o appena scaricato), 0 altrimenti (nessun elenco/URL/rete). */
/* ── gmb_boot: PUNTO UNICO di ricerca/caricamento/bootstrap delle librerie native ────────────
 * Una sola radice (`~/.gmbasic/libs/<rid>/`), un solo risolutore. Il C# non cerca nulla: chiede
 * `gmb_lib_path` e fa `NativeLibrary.Load`. Il C usa `gmb_lib_open`.
 * `gmb_bootstrap` scarica l'indice piatto del RID e i file dello scope, con progress bar su TTY. */
#define GMB_SCOPE_RUNTIME    1
#define GMB_SCOPE_DEV        2
#define GMB_SCOPE_COMPONENTS 4
#define GMB_SCOPE_ALL        8

const char* gmb_rid(void);
int  gmb_cache(char* out, size_t cap);
int  gmb_lib_path(const char* name, char* out, size_t cap);
int  gmb_lib_path_any(const char* name, char* out, size_t cap);   /* nome P/Invoke → file */
int  gmb_lib_open(const char* name);
int  gmb_component(const char* name, char* out, size_t cap);
int  gmb_bootstrap(int scope);
void gmb_progress(const char* label, double frac01);

const char* gmb_libs_rid(void);
int  gmb_libs_ensure_file_from(const char* index_path, const char* fileName, char* out_path, size_t cap);
int  gmb_libs_autoensure(const char* fileName, char* out_path, size_t cap);
int  gmb_libs_ensure_next_to(const char* exe, const char* fileName, char* out_path, size_t cap);
void  gmb_paths_packages_dir(char* out, size_t cap);           /* default: <root>/packages (vedi anche gmb_paths_exe_dir) */
int   gmb_paths_exe_dir(char* out, size_t cap);                /* dir dell'eseguibile: ancora di <exe>/packages */
void  gmb_paths_temp_dir(char* out, size_t cap);               /* default: tmp del SO */
/* File di stato/config (dentro configDir): stessa convenzione del C#. */
void  gmb_paths_vaults_db(char* out, size_t cap);              /* <configDir>/vaults.db */
void  gmb_paths_brain_db(char* out, size_t cap);               /* <configDir>/brain.db */
void  gmb_paths_brains_json(char* out, size_t cap);            /* <configDir>/brains.json */
void  gmb_paths_providers_json(char* out, size_t cap);         /* <configDir>/providers.json */
void  gmb_paths_mcp_json(char* out, size_t cap);               /* <configDir>/mcp.json */
void  gmb_paths_plugins_json(char* out, size_t cap);           /* <configDir>/plugins.json (override archivio plugin SET INDEX) */
void  gmb_paths_skills_index_json(char* out, size_t cap);      /* <configDir>/skills-index.json (override percorso indice SKILLS_PROMPT.md, SET INDEX skills) */
void  gmb_paths_recursivemas_dir(char* out, size_t cap);       /* <configDir>/recursivemas */
/* Componenti runtime scaricati on-demand (libsDir/<componente>/<rid>). */
void  gmb_paths_ml_dir(const char* rid, char* out, size_t cap);          /* libsDir/ml/<rid> */
void  gmb_paths_chromium_dir(const char* rid, char* out, size_t cap);    /* libsDir/chromium/<rid> */
void  gmb_paths_obscura_dir(const char* rid, char* out, size_t cap);     /* libsDir/obscura/<rid> — Obscura headless per agenti (CUA §10) */
/* Marketplace di skills/plugins: CACHE UTENTE (installazione, non staging). */
void  gmb_paths_skills_marketplaces_dir(char* out, size_t cap);  /* ~/.gmbasic/skills-marketplaces */
void  gmb_paths_plugins_marketplaces_dir(char* out, size_t cap); /* ~/.gmbasic/plugins-marketplaces */
/* Cache temporanee (in tempDir). */
void  gmb_paths_kvswap_dir(char* out, size_t cap);               /* temp/gmbasic-kv-swap */
/* Legge una chiave "paths" (es. "ragDir") da system.settings.json → path risolto.
 * Ritorna 1 se la chiave era presente e valorizzata, 0 se assente/null (default). */
int   gmb_paths_get(const char* key, char* out, size_t cap);

/* Cartella ABI di LLMEnv (descrittori dei modelli .gmbarch): da `paths.abiDir` dei settings (app.settings.json per i programmi
 * BASIC); se manca o non e' piu' valida la localizza (prima esecuzione), la salva nei settings e, se LLMEnv non c'e', propone
 * l'installazione. Registrata come risolutore di llmdesc: chiamata quando serve. 1 = trovata. */
int   gmb_abi_dir(char* out, size_t cap);
gmb_string_t gmb_str_padl_str(gmb_string_t s, gmb_int_t length, gmb_string_t pad);
gmb_string_t gmb_str_padr_str(gmb_string_t s, gmb_int_t length, gmb_string_t pad);
gmb_string_t gmb_str_padc_str(gmb_string_t s, gmb_int_t length, gmb_string_t pad);
gmb_string_t gmb_strmd5(gmb_string_t s);
gmb_string_t gmb_strsha256(gmb_string_t s);
/* Eventi mouse/finestra: registrazione handler (parità IL/PCodeVM). */
void gmb_mouse_register_event(const char* event_kind, const char* handler);
void gmb_mouse_unregister_event(const char* event_kind);
void gmb_window_register_event(const char* event_kind, const char* handler);
/* Eventi finestra per HANDLE con callback (function pointer); win=0 = finestra SDL. */
void gmb_window_register_event_of_cb(int win, const char* ev, double (*cb)(void));
void gmb_window_fire(int win, const char* ev);
void gmb_window_reset_events(void);
void gmb_window_unregister_event(const char* event_kind);

/* ── REGEX: val-helper (usati dagli emitter C; impl. in gmb_regex_extra.c, PCRE2) ─────────
 * DICHIARATI qui (non in gmb_regex_extra.h) così il C GENERATO non deve includere pcre2.h. */
gmb_val_t gmb_regex_new_val(const char* pattern);
gmb_val_t gmb_regex_pattern_val(gmb_val_t reg);
int gmb_regex_set_pattern_val(gmb_val_t reg, const char* pattern);gmb_val_t gmb_regex_ismatch_val(gmb_val_t reg, const char* text);
gmb_val_t gmb_regex_match_val(gmb_val_t reg, const char* text);
gmb_val_t gmb_regex_matches_val(gmb_val_t reg, const char* text);
gmb_val_t gmb_regex_replace_val(gmb_val_t reg, const char* text, const char* repl);
gmb_val_t gmb_regex_split_val(gmb_val_t reg, const char* text);
gmb_string_t* gmb_regex_split_strings(gmb_val_t reg, const char* text, gmb_int_t* count);

#ifdef __cplusplus
}
#endif
