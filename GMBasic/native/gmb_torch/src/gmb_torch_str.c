/* ============================================================================
 * gmb_torch_str.c — varianti gmb_string_t della superficie gmb_ml_* (SOLO C99)
 * File: native/gmb_torch/src/gmb_torch_str.c
 *
 * L'emitter C99 esprime le stringhe come gmb_string_t (runtime condiviso): queste
 * varianti adattano i ritorni char e buffer della superficie. NON compilate nella
 * libgmb_torch_loader (dylib standalone senza gmb_runtime) né nella VM: solo nel
 * C99 gate (che linka l'intero gmb_runtime_c).
 * ============================================================================ */

#include "gmb_ml.h"
#include "gmb_torch.h"
#include "gmb_runtime.h"
#include <stdlib.h>

/* Varianti gmb_string_t (per l'emitter C99, che esprime le stringhe come gmb_string_t). */
GMB_ML_API gmb_string_t gmb_ml_device_name_s(void)
{
    char buf[32];
    if (gmb_ml_device_name(gmb_ml_active_device(), buf, (int)sizeof(buf))) return gmb_string_alloc(buf);
    return gmb_string_alloc("cpu");
}
GMB_ML_API gmb_string_t gmb_ml_lib_path_s(void)
{
    char buf[1024];
    if (gmb_ml_lib_path(buf, (int)sizeof(buf))) return gmb_string_alloc(buf);
    return gmb_string_alloc("");
}
GMB_ML_API gmb_string_t gmb_ml_model_state_s(int64_t model)
{
    char buf[512];
    if (gmb_ml_model_state(model, buf, (int)sizeof(buf)) == 0) return gmb_string_alloc(buf);
    return gmb_string_alloc("");
}
/* Varianti gmb_string_t (C99): dtype/device/available → stringa. */
GMB_ML_API gmb_string_t gmb_ml_tensor_dtype_s(int64_t h)
{ char b[16]; if (gmb_ml_tensor_dtype(h, b, (int)sizeof(b)) == 0) return gmb_string_alloc(b); return gmb_string_alloc(""); }
GMB_ML_API gmb_string_t gmb_ml_tensor_device_s(int64_t h)
{ char b[16]; if (gmb_ml_tensor_device(h, b, (int)sizeof(b)) == 0) return gmb_string_alloc(b); return gmb_string_alloc(""); }
GMB_ML_API gmb_string_t gmb_ml_available_s(void)
{ char b[64]; if (gmb_ml_available(b, (int)sizeof(b)) == 0) return gmb_string_alloc(b); return gmb_string_alloc(""); }
/* L6: TEXTDECODE (C99) → gmb_string_t. Buffer 64 KiB come il path C#. */
GMB_ML_API gmb_string_t gmb_ml_text_decode_s(int64_t h)
{ char* b = (char*)malloc(65536); if (!b) return gmb_string_alloc(""); gmb_string_t r; if (gmb_ml_text_decode(h, b, 65536) == 0) r = gmb_string_alloc(b); else r = gmb_string_alloc(""); free(b); return r; }

