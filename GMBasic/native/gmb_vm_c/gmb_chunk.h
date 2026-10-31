/* ============================================================================
 * gmb_chunk.h — Loader del CompiledChunk serializzato (formato A1)
 * File: native/gmb_vm_c/gmb_chunk.h
 * Deserializza il byte[] prodotto da ChunkSerializer.cs (MAGIC "GMBC", big-endian).
 * ============================================================================ */

#ifndef GMB_CHUNK_H
#define GMB_CHUNK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmb_vm_value.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_CHUNK_MAGIC "GMBC"
#define GMB_CHUNK_VERSION 1

/* ---- Tipi di valore nel constant pool ---- */
enum {
    GMB_CONST_NIL = 0,
    GMB_CONST_DOUBLE = 1,
    GMB_CONST_STRING = 2,
    GMB_CONST_DICT = 3,
    GMB_CONST_LIST = 4,
    GMB_CONST_INT = 5,
    GMB_CONST_DATATYPE = 6,   /* categoria 0=num 1=str 2=other */
};

/* Categoria DataType (specchio del default value della VM C#) */
enum { GMB_DT_NUM = 0, GMB_DT_STR = 1, GMB_DT_OTHER = 2 };

/* ---- Costanti: dict e list (descrittori UDT/class) ---- */
typedef struct gmb_chunk_dict_entry {
    /* chiave: int o stringa */
    int      key_is_int;
    int64_t  key_int;
    char*    key_str;
    /* valore: boxed */
    int      val_type;         /* GMB_CONST_* */
    double   val_double;
    char*    val_str;
    /* dict/list ricorsivi */
    struct gmb_chunk_dict*    val_dict;
    struct gmb_chunk_list*    val_list;
    uint8_t  val_dt_cat;
} gmb_chunk_dict_entry_t;

typedef struct gmb_chunk_dict {
    uint32_t count;
    gmb_chunk_dict_entry_t* entries;
} gmb_chunk_dict_t;

typedef struct gmb_chunk_list {
    uint32_t count;
    gmb_chunk_dict_entry_t* items;  /* riusa la stessa entry boxed */
} gmb_chunk_list_t;

/* ---- Valore boxed generico (costante o DATA) ---- */
typedef struct {
    int type;               /* GMB_CONST_* */
    double d;
    char*  s;
    gmb_chunk_dict_t* dict;
    gmb_chunk_list_t* list;
    uint8_t dt_cat;
} gmb_value_t;

/* ---- Chunk deserializzato ---- */
typedef struct {
    uint8_t* code;
    uint32_t code_len;
    uint32_t* lines;          /* code_len entries (può essere NULL se non richiesto) */
    gmb_value_t* constants;
    uint32_t consts_len;
    gmb_value_t* data;
    uint32_t data_len;
    int32_t entry_point;
    uint32_t global_count;
    /* procedure name -> addr */
    char**   proc_names;
    uint32_t* proc_addrs;
    uint32_t proc_count;
    /* addr -> arity */
    uint32_t* arity_addrs;
    uint32_t* arity_vals;
    uint32_t arity_count;
    /* nomi globali (slot → nome), coda opzionale del GMBC */
    char**   global_names;
    uint32_t global_name_count;
    /* LOCAL COUNT (coda opzionale A1) */
    uint32_t local_count;
    /* TRY regions (unwind sospendibile, coda opzionale A1): indirizzo dell'opcode
     * OP_PUSH_TRY -> (FinallyIp, EndIp, HasCatch). Specchio di
     * CompiledChunk.TryRegions/TryRegionMetadata (Compiler.Exceptions.cs). */
    uint32_t* try_ips;
    uint32_t* try_finally_ips;
    uint32_t* try_end_ips;
    uint8_t*  try_has_catch;
    uint32_t  try_region_count;
    /* nomi locali per procedura (coda opzionale A1, S_eval EVAL/macro): per voce indirizzo
     * di inizio procedura + nomi per slot (NULL = slot anonimo). Specchio di
     * CompiledChunk.ProcedureLocalNames (C#) / ploc_* del compilatore nativo. */
    uint32_t* ploc_addrs;
    char***   ploc_names;
    uint32_t* ploc_counts;
    uint32_t  ploc_count;
    /* pool di stringhe deserializzate (ownership) */
    char** _strings;
    uint32_t _strings_count;
    uint32_t _strings_cap;
} gmb_chunk_t;

/* ---- API ---- */
gmb_chunk_t* gmb_chunk_load(const uint8_t* data, size_t len, bool load_lines);
void         gmb_chunk_free(gmb_chunk_t* chunk);

/* Accesso costanti */
const gmb_value_t* gmb_chunk_const(const gmb_chunk_t* chunk, uint32_t idx);
/* Lookup TRY region per indirizzo opcode OP_PUSH_TRY (ritorna 0 se assente, 1 se trovata). */
int gmb_chunk_try_region(const gmb_chunk_t* chunk, int try_ip, uint32_t* finally_ip, uint32_t* end_ip, int* has_catch);
/* Risolve uno slot locale per nome nella procedura che contiene ip (indirizzo di inizio
 * massimo <= ip, confronto case-insensitive con retry senza suffisso tipo come
 * gmb_expr lookup). Ritorna lo slot o -1 (sconosciuto / fuori da ogni procedura nota). */
int gmb_chunk_local_slot(const gmb_chunk_t* chunk, int ip, const char* name);
/* Valore di default per un campo (da categoria DataType): numerico → 0.0, stringa → "" */
gmb_value_t gmb_field_default(uint8_t dt_cat);

#ifdef __cplusplus
}
#endif

#endif /* GMB_CHUNK_H */
