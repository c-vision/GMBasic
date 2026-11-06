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
    if (!fn || !ret || n < 0) return -1;
    ffi_cif cif;
    ffi_type* local_types[32];
    ffi_type** types = local_types;
    if (n > 32)
    {
        types = (ffi_type**)malloc((size_t)n * sizeof(ffi_type*));
        if (!types) return -1;
    }
    for (int i = 0; i < n; i++)
        types[i] = is_int[i] ? &ffi_type_sint64 : &ffi_type_double;

    ffi_type* rt = ret_is_int ? &ffi_type_sint64 : &ffi_type_double;
    /* ffi_call vuole un void** non-const: i puntatori ai valori non vengono modificati. */
    int rc = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)n, rt, types);
    if (rc == FFI_OK) ffi_call(&cif, FFI_FN(fn), ret, (void**)(size_t)vals);
    if (types != local_types) free(types);
    return rc == FFI_OK ? 0 : -1;
}

int gmb_call_invoke_slots(void* fn, int n, const gmb_call_slot* slots,
                          void* const* vals, int ret_kind,
                          int ret_nfields, const unsigned char* ret_field_is_int,
                          void* ret)
{
    if (!fn || !ret || n < 0) return -1;
    ffi_cif cif;
    ffi_type* local_types[64];
    ffi_type** types = local_types;
    if (n > 64) { types = (ffi_type**)malloc((size_t)n * sizeof(ffi_type*)); if (!types) return -1; }
    /* Ogni slot struct ha bisogno di un ffi_type e di un array di elementi vivi fino a ffi_call. */
    ffi_type* structs = (ffi_type*)calloc((size_t)(n > 0 ? n : 1), sizeof(ffi_type));
    ffi_type*** elems = (ffi_type***)calloc((size_t)(n > 0 ? n : 1), sizeof(ffi_type**));
    int rc = FFI_OK;
    for (int i = 0; i < n && rc == FFI_OK; i++)
    {
        if (slots[i].kind != 2) { types[i] = slots[i].kind == 1 ? &ffi_type_sint64 : &ffi_type_double; continue; }
        int nf = slots[i].nfields;
        if (nf < 1) { rc = FFI_BAD_ARGTYPE; break; }
        elems[i] = (ffi_type**)malloc((size_t)(nf + 1) * sizeof(ffi_type*));
        if (!elems[i]) { rc = FFI_BAD_ARGTYPE; break; }
        for (int f = 0; f < nf; f++)
            elems[i][f] = (slots[i].field_is_int && slots[i].field_is_int[f]) ? &ffi_type_sint64 : &ffi_type_double;
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
                ret_elems[f] = (ret_field_is_int && ret_field_is_int[f]) ? &ffi_type_sint64 : &ffi_type_double;
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
