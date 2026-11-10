/* ============================================================================
 * gmb_vm_value.h — Valori e stack della VM C (stile Clipper)
 * File: native/gmb_vm_c/gmb_vm_value.h
 * Fedele alla struttura P-CODE C#: StackEntry (Num + Ref + Type + BasicType).
 * ============================================================================ */

#ifndef GMB_VM_VALUE_H
#define GMB_VM_VALUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- BasicType tag (specchio di StackBasicType) ---- */
enum {
    GMB_BT_UNSET  = 0,
    GMB_BT_NUMBER = 1,  /* DOUBLE/INTEGER/LONG/FLOAT */
    GMB_BT_STRING = 2,  /* STRING */
    GMB_BT_BOOL   = 3,  /* TRUE/FALSE */
    GMB_BT_DATE   = 4,  /* DateTime/DateOnly */
    GMB_BT_OBJECT = 5,  /* DICT/LIST/UDT/CLASS */
    GMB_BT_NULL   = 6,  /* NIL/NULL */
    /* CURRENCY: numerico con scala (campo DBF N/F late-binding). Sullo stack/globali è di
     * fatto numerico (num); `scale` (campo dell'entry) preserva le cifre decimali per la
     * resa (PRINT "100.00", CSTR " 100.00"). Non è un DOUBLE puro: senza la scala perde
     * gli zeri finali. */
    GMB_BT_CURRENCY = 7,
};

/* ---- Value: riferimento opaco (stringa/dict/list/canale/...) ----
   NOTA: gmb_obj_t e l'enum GMB_OBJ_* sono definiti in gmb_runtime.h (struct
   unificata, F2: il runtime C deve poter riconoscere i wrapper VM per
   gmb_val_to_json/gmb_json_append_val). */


/* ---- Riferimento a lista/dict del runtime condiviso (gmb_val_t ARRAY/OBJECT).
   La VM li incapsula in un gmb_obj heap così lo stack li gestisce con refcount. ---- */
typedef struct gmb_obj_shared_val {
    struct gmb_obj header;
    gmb_val_t val;       /* GMB_TYPE_ARRAY (list) / GMB_TYPE_OBJECT (dict/class) */
    gmb_list_t* list;    /* cast val.v_ptr quando list */
    gmb_dict_t* dict;    /* cast val.v_ptr quando dict */
    int ndims;           /* 0 = lista semplice; >=1 = array creato da CREATE_ARRAY (DIM A(...)) */
    int is_array;        /* 1 = array (anche dinamico: DIM d() / DIM d() = [...]). Distingue
                          * un array da una LISTA a runtime per TYPENAME → "ARRAY". */
    int dim_upper[8];    /* limite SUPERIORE dichiarato per dimensione — per UBOUND(A, d) */
    int dim_lower[8];    /* limite INFERIORE dichiarato per dimensione — per LBOUND(A, d) (default 0) */
    /* ADR-MATRIX backend C: descrittore a valore (mirror di gmb_matrix_t), popolato per i
     * valori matriciali creati da MATRIX.CREATE e dalle funzioni a valore. La lista flat
     * resta la rappresentazione condivisa con il bytecode e viene tenuta IN SYNC; il
     * descrittore evita di ri-appiattire a ogni operazione MATRIX.* e abilita l'accesso
     * diretto agli elementi. NULL = valore non matriciale (o invalidato da una mutazione
     * della lista: si ricostruisce dal flat). */
    gmb_matrix_t* mat;
} gmb_obj_shared_val_t;

gmb_obj_shared_val_t* gmb_shared_list_new(void);
gmb_obj_shared_val_t* gmb_shared_dict_new(void);

/* Stringa heap. `capacity` è la dimensione allocata di `data` (esclusa la NUL finale
 * aggiunta da gmb_str_append_inplace); può eccedere `length` per via della crescita
 * geometrica dell'append in place -- vedi gmb_str_append_inplace in gmb_vm_value.c. */
typedef struct gmb_obj_string {
    struct gmb_obj header;
    uint32_t length;
    uint32_t capacity;
    char*    data;      /* non-null terminata */
} gmb_obj_string_t;

/* ---- Entry dello stack operandi — specchio di StackEntry C# ---- */
typedef struct {
    double       num;       /* valore numerico (quando type==1) */
    gmb_obj_t*   ref;       /* oggetto (quando type==2) */
    uint8_t      type;      /* 0=vuoto, 1=num, 2=ref */
    uint8_t      basic_type;/* GMB_BT_* */
    uint8_t      scale;     /* GMB_BT_CURRENCY: cifre decimali; 0 per gli altri (mai letto) */
} gmb_stack_entry_t;

/* ---- Closure (funzione anonima/lambda che cattura upvalue) ---- */
typedef struct gmb_obj_closure {
    struct gmb_obj header;
    int address;              /* functionAddress nel chunk */
    int num_upvalues;
    gmb_stack_entry_t* upvalues;  /* valori catturati (copiati) */
} gmb_obj_closure_t;

gmb_obj_closure_t* gmb_closure_new(int address, int num_upvalues);

/* Generatore (GENERATOR PROCEDURE / espressione generatore): incapsula il task
 * dedicato guidato da NEXT() (mai schedulato sul GPM — eseguito sincronamente dal
 * driver). `done` segna la fine dell'iterazione. */
typedef struct gmb_obj_generator {
    struct gmb_obj header;
    struct gmb_task* task;   /* task dedicato (ip=corpo generatore, globals condivise) */
    int done;
    int is_expr;             /* 1 = espressione generatore (lista materializzata) */
    gmb_obj_shared_val_t* expr_list; /* per is_expr: valori prodotti */
    gmb_int_t expr_pos;
} gmb_obj_generator_t;

gmb_obj_generator_t* gmb_generator_new(struct gmb_task* task);

/* ---- Graph (Collections: tipo GRAPH<T>) — incapsula un handle del registry C ---- */
typedef struct gmb_obj_graph {
    struct gmb_obj header;
    int64_t handle;   /* handle nel registry gmb_graph */
} gmb_obj_graph_t;

gmb_obj_graph_t* gmb_graph_obj_new(int64_t handle);

/* ---- Collections generiche (LIST/ALBERI/CODE/...) — incapsula handle + typeCode ---- */
typedef struct gmb_obj_collection {
    struct gmb_obj header;
    int64_t handle;   /* handle nel registry gmb_collections */
    int type_code;    /* typeCode BASIC (9=RedBlack, 10=BITree, 11=Trie, 12=LinkedList, ...) */
} gmb_obj_collection_t;

gmb_obj_collection_t* gmb_collection_obj_new(int64_t handle, int type_code);

/* ---- Costruttori/funzioni oggetto ---- */
gmb_obj_string_t* gmb_str_alloc(const char* data, uint32_t len);
gmb_obj_string_t* gmb_str_cstr(const char* s);
void gmb_str_retain(gmb_obj_string_t* s);
void gmb_str_release(gmb_obj_string_t* s);
uint32_t gmb_str_len(const gmb_obj_string_t* s);
/* Appende `len` byte di `data` a `s` in place (realloc + crescita geometrica della
 * capacità). Il CHIAMANTE deve aver già verificato `s->header.refcount == 1` (nessun
 * altro alias) -- questa funzione non lo ricontrolla, si fida della precondizione. */
void gmb_str_append_inplace(gmb_obj_string_t* s, const char* data, uint32_t len);
void gmb_obj_retain(gmb_obj_t* o);
void gmb_obj_release(gmb_obj_t* o);

/* ---- Stack operandi ---- */
#define GMB_STACK_CAP 4096

typedef struct {
    gmb_stack_entry_t slots[GMB_STACK_CAP];
    int sp;
} gmb_stack_t;

void     gmb_stack_init(gmb_stack_t* s);
void     gmb_stack_push_num(gmb_stack_t* s, double v, uint8_t basic_type);
void     gmb_stack_push_ref(gmb_stack_t* s, gmb_obj_t* o, uint8_t basic_type);
void     gmb_stack_push_nil(gmb_stack_t* s);
gmb_stack_entry_t gmb_stack_pop(gmb_stack_t* s);
gmb_stack_entry_t gmb_stack_pop_release(gmb_stack_t* s);
gmb_stack_entry_t gmb_stack_peek(const gmb_stack_t* s, int from_top);
double   gmb_stack_pop_num(gmb_stack_t* s);
gmb_obj_t* gmb_stack_pop_ref(gmb_stack_t* s);
int      gmb_stack_size(const gmb_stack_t* s);
void     gmb_stack_drop(gmb_stack_t* s, int n);

/* Serializza una lista/dict condivisa in una stringa JSON (retain 1) o NULL.
 * Definita in gmb_vm_value.c (consolidata 2026-09-11: gmb_builtin_json.c rimosso). */
gmb_obj_string_t* gmb_vm_json_stringify(gmb_obj_t* obj);

/* ---- Conversione stack <-> runtime entry ---- */
/* ---- Conversione stack <-> runtime entry (obsoleto, sostituito da gmb_adapter) ---- */

#ifdef __cplusplus
}
#endif

#endif /* GMB_VM_VALUE_H */
