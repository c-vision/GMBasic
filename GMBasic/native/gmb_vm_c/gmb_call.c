/*
 * gmb_call.c — chiamata FFI tramite libffi (vedi gmb_call.h).
 *
 * Nota sulle piattaforme: su macOS l'header è <ffi/ffi.h> (SDK); su Linux è <ffi.h>
 * (libffi-dev). La libreria si linka con -lffi (CMake: GMB_FFI_LIBRARY).
 */
#include "gmb_call.h"

#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#  include <ffi/ffi.h>
#else
#  include <ffi.h>
#endif

int gmb_call_enabled(void)
{
    static int cached = -1;
    if (cached < 0)
    {
        const char* e = getenv("GMB_FFI_LIBFFI");
        cached = (e && e[0] == '0') ? 0 : 1;
    }
    return cached;
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
