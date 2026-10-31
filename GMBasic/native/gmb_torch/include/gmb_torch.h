/* ============================================================================
 * gmb_torch.h — API del LOADER (gmb_torch.c), separata dalla superficie host
 * gmb_ml.h. Controllo del caricamento/percorso delle librerie runtime ML.
 * File: native/gmb_torch/include/gmb_torch.h
 *
 * gmb_ml.h = cosa implementa l'host (libtorch); gmb_torch.h = come il runtime C
 * carica/gestisce la lib (scoperta automatica, override percorso, load/unload).
 * Usata da VM/C, C99 e (via P/Invoke) dal bridge gestito.
 * ============================================================================ */

#ifndef GMB_TORCH_H
#define GMB_TORCH_H

#include "gmb_ml.h"
#include "gmb_runtime.h"   /* gmb_string_t per le varianti stringa */

#ifdef __cplusplus
extern "C" {
#endif

/* Override del percorso di ricerca: se impostato, gml_find_lib usa SOLO questa
 * directory (accanto al nome canonico della lib). "" azzera l'override. */
GMB_ML_API int gmb_ml_set_lib_dir(const char* dir);
GMB_ML_API int gmb_ml_set_active_device_name(const char* name);

/* Caricamento esplicito (idempotente). GMB_ML_OK se caricata, GMB_ML_NOTLOADED altrimenti. */
GMB_ML_API int gmb_ml_load_lib(void);

/* Scarica la lib (libera l'handle; le prossime chiamate ricaricano lazy). */
GMB_ML_API void gmb_ml_unload_lib(void);

/* Path risolto della lib (dopo un tentativo di caricamento). 0 se mai risolta. */
GMB_ML_API int gmb_ml_lib_path(char* out, int cap);

/* 1 se la lib è caricata e i simboli risolti. */
GMB_ML_API int gmb_ml_loaded(void);

/* Varianti gmb_string_t (per l'emitter C99). */
GMB_ML_API gmb_string_t gmb_ml_device_name_s(void);
GMB_ML_API gmb_string_t gmb_ml_lib_path_s(void);
GMB_ML_API gmb_string_t gmb_ml_model_state_s(int64_t model);
GMB_ML_API gmb_string_t gmb_ml_tensor_dtype_s(int64_t h);
GMB_ML_API gmb_string_t gmb_ml_tensor_device_s(int64_t h);
GMB_ML_API gmb_string_t gmb_ml_available_s(void);
GMB_ML_API gmb_string_t gmb_ml_text_decode_s(int64_t h);

/* Varianti ALLOC (per l'emitter C99 array-returning): ritornano l'array allocato
 * (caller free) con lunghezza out-param. NULL + *outn=0 se non caricata/invalida. */
GMB_ML_API double* gmb_ml_tensor_toarray_alloc(int64_t h, gmb_int_t* outn);
GMB_ML_API double* gmb_ml_tensor_shape_alloc(int64_t h, gmb_int_t* outn);
GMB_ML_API double* gmb_ml_tensor_topk_alloc(int64_t h, int k, gmb_int_t* outn);
GMB_ML_API double* gmb_ml_tensor_argsort_alloc(int64_t h, gmb_int_t* outn);
GMB_ML_API double* gmb_ml_tensor_onehot_alloc(int idx, int n, gmb_int_t* outn);
GMB_ML_API double* gmb_ml_tensor_clamp_alloc(int64_t h, double lo, double hi, gmb_int_t* outn);
GMB_ML_API double* gmb_ml_tensor_normalize_alloc(int64_t h, gmb_int_t* outn);
GMB_ML_API double* gmb_ml_tensor_standardize_alloc(int64_t h, gmb_int_t* outn);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TORCH_H */
