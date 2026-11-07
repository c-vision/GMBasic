/*
 * gmb_call.c — chiamata FFI tramite libffi (vedi gmb_call.h).
 *
 * Nota sulle piattaforme: su macOS l'header è <ffi/ffi.h> (SDK); su Linux è <ffi.h>
 * (libffi-dev). La libreria si linka con -lffi (CMake: GMB_FFI_LIBRARY).
 */
#include "gmb_call.h"

#include <stdlib.h>
#include <string.h>

#if defined(GMB_FFI_VENDORED)
#  include <ffi.h>          /* copia vendorizzata: shared/libs/ffi/out/include */
#elif defined(__APPLE__)
#  include <ffi/ffi.h>      /* SDK macOS */
#else
#  include <ffi.h>          /* libffi-dev */
#endif

/* Cache delle `ffi_cif` (2026-11-07, banco FFI). `ffi_prep_cif` costa ~7 ns per
 * chiamata e, con la cache, il solo `ffi_call` resta ~16 ns. Chiave: (n, maschera
 * int64 per posizione, kind del ritorno) — copre tutte le firme omogenee/miste scalari.
 * La tabella è fissa (64 firme distinte) e le `ffi_type*` restano vive quanto la voce:
 * libffi le referenzia per tutta la vita del cif. VM/C è single-thread per VM. */
#define GMB_CIF_CACHE_MAX 64
typedef struct {
    int used;
    int n;
    int ret_is_int;
    uint64_t int_mask;
    ffi_cif cif;
    ffi_type** types;   /* array allocato, posseduto dalla voce */
} gmb_cif_entry_t;

static gmb_cif_entry_t g_cif_cache[GMB_CIF_CACHE_MAX];
static int g_cif_next = 0;

static gmb_cif_entry_t* gmb_cif_cache_get(int n, uint64_t mask, int ret_is_int)
{
    for (int i = 0; i < GMB_CIF_CACHE_MAX; i++)
    {
        gmb_cif_entry_t* e = &g_cif_cache[i];
        if (e->used && e->n == n && e->ret_is_int == ret_is_int && e->int_mask == mask)
            return e;
    }
    gmb_cif_entry_t* e = &g_cif_cache[g_cif_next];
    g_cif_next = (g_cif_next + 1) % GMB_CIF_CACHE_MAX;
    free(e->types);
    e->types = NULL;
    e->used = 0;
    e->n = n;
    e->ret_is_int = ret_is_int;
    e->int_mask = mask;
    if (n > 0)
    {
        e->types = (ffi_type**)malloc((size_t)n * sizeof(ffi_type*));
        if (!e->types) return NULL;
        for (int i = 0; i < n; i++)
            e->types[i] = (mask & (1ull << i)) ? &ffi_type_sint64 : &ffi_type_double;
    }
    ffi_type* rt = ret_is_int ? &ffi_type_sint64 : &ffi_type_double;
    if (ffi_prep_cif(&e->cif, FFI_DEFAULT_ABI, (unsigned)n, rt, e->types) != FFI_OK)
    {
        free(e->types); e->types = NULL;
        return NULL;
    }
    e->used = 1;
    return e;
}

int gmb_call_enabled(void)
{
    /* Sempre attivo (2026-11-06): l'interruttore via env `GMB_FFI_LIBFFI=0` violava la regola
     * del monorepo (nessuna env nuova per governare il comportamento). Il ripiego sul dispatch
     * storico resta automatico quando `ffi_prep_cif` fallisce, senza bisogno di un flag. */
    return 1;
}

int gmb_call_invoke(void* fn, int n, const unsigned char* is_int,
                    void* const* vals, int ret_is_int, void* ret)
{
    if (!fn || !ret || n < 0 || n > 64) return -1;
    uint64_t mask = 0;
    for (int i = 0; i < n; i++)
        if (is_int[i]) mask |= (1ull << i);

    gmb_cif_entry_t* e = gmb_cif_cache_get(n, mask, ret_is_int);
    if (!e) return -1;
    /* ffi_call vuole un void** non-const: i puntatori ai valori non vengono modificati. */
    ffi_call(&e->cif, FFI_FN(fn), ret, (void**)(size_t)vals);
    return 0;
}

/* Mappa un codice tipo del descrittore FFI (condiviso con il bytecode e il C#) su un
 * ffi_type di libffi. 0=double 1=int64 2=int8 3=uint8 4=int16 5=uint16 6=int32 7=uint32
 * 8=float32 9=pointer 10=uint64. */
static ffi_type* ffi_type_of_code(unsigned char code)
{
    switch (code)
    {
        case 0:  return &ffi_type_double;
        case 1:  return &ffi_type_sint64;
        case 2:  return &ffi_type_sint8;
        case 3:  return &ffi_type_uint8;
        case 4:  return &ffi_type_sint16;
        case 5:  return &ffi_type_uint16;
        case 6:  return &ffi_type_sint32;
        case 7:  return &ffi_type_uint32;
        case 8:  return &ffi_type_float;
        case 9:  return &ffi_type_pointer;
        case 10: return &ffi_type_uint64;
        default: return &ffi_type_double;
    }
}

int gmb_call_invoke_slots(void* fn, int n, const gmb_call_slot* slots,
                          void* const* vals, int ret_kind,
                          int ret_nfields, const unsigned char* ret_field_type,
                          const int* ret_field_off, int ret_struct_size,
                          void* ret)
{
    (void)ret_field_off;   /* gli offset servono al chiamante per riempire/leggere il buffer;
                            * libffi ricostruisce la stessa classificazione ABI dai tipi. */
    (void)ret_struct_size;
    if (!fn || !ret || n < 0) return -1;
    ffi_cif cif;
    ffi_type* local_types[64];
    ffi_type** types = local_types;
    if (n > 64) { types = (ffi_type**)malloc((size_t)n * sizeof(ffi_type*)); if (!types) return -1; }
    ffi_type* structs = (ffi_type*)calloc((size_t)(n > 0 ? n : 1), sizeof(ffi_type));
    ffi_type*** elems = (ffi_type***)calloc((size_t)(n > 0 ? n : 1), sizeof(ffi_type**));
    if (!structs || !elems)
    {
        free(structs); free(elems);
        if (types != local_types) free(types);
        return -1;
    }
    int rc = FFI_OK;
    for (int i = 0; i < n && rc == FFI_OK; i++)
    {
        if (slots[i].kind != 2) { types[i] = slots[i].kind == 1 ? &ffi_type_sint64 : &ffi_type_double; continue; }
        int nf = slots[i].nfields;
        if (nf < 1 || !slots[i].field_type) { rc = FFI_BAD_ARGTYPE; break; }
        elems[i] = (ffi_type**)malloc((size_t)(nf + 1) * sizeof(ffi_type*));
        if (!elems[i]) { rc = FFI_BAD_ARGTYPE; break; }
        for (int f = 0; f < nf; f++)
            elems[i][f] = ffi_type_of_code(slots[i].field_type[f]);
        elems[i][nf] = NULL;
        structs[i].size = 0; structs[i].alignment = 0;
        structs[i].type = FFI_TYPE_STRUCT; structs[i].elements = elems[i];
        types[i] = &structs[i];
    }
    ffi_type* rt;
    ffi_type ret_struct; ffi_type** ret_elems = NULL;
    if (rc != FFI_OK) { rt = &ffi_type_void; }
    else if (ret_kind == 2 && ret_nfields > 0)
    {
        ret_elems = (ffi_type**)malloc((size_t)(ret_nfields + 1) * sizeof(ffi_type*));
        if (!ret_elems) rc = FFI_BAD_ARGTYPE;
        else
        {
            for (int f = 0; f < ret_nfields; f++)
                ret_elems[f] = ffi_type_of_code(ret_field_type ? ret_field_type[f] : 0);
            ret_elems[ret_nfields] = NULL;
            ret_struct.size = 0; ret_struct.alignment = 0;
            ret_struct.type = FFI_TYPE_STRUCT; ret_struct.elements = ret_elems;
            rt = &ret_struct;
        }
    }
    else rt = ret_kind == 1 ? &ffi_type_sint64 : &ffi_type_double;

    if (rc == FFI_OK)
    {
        rc = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)n, rt, types);
        if (rc == FFI_OK) ffi_call(&cif, FFI_FN(fn), ret, (void**)(size_t)vals);
    }
    for (int i = 0; i < n; i++) free(elems[i]);
    free(elems); free(structs);
    free(ret_elems);
    if (types != local_types) free(types);
    return rc == FFI_OK ? 0 : -1;
}
