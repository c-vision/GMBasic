/* ============================================================================
 * gmb_torch.c — runtime ML separato dal runtime C (GMBasic)
 * File: native/gmb_torch/src/gmb_torch.c
 *
 * Integrazione PyTorch/libtorch per i backend NATIVI C (VM/C e C99):
 *   - risolve la libreria host da resources/libs/<rid>/ (per sistema operativo),
 *   - la carica con dlopen/LoadLibrary (lazy, mai link statico),
 *   - implementa la superficie pubblica gmb_ml.h delegando all'host caricato.
 *
 * Il bridge gestito (ILBackend/PCodeVM) NON passa da qui: usa lo stesso gmb_ml.h
 * via P/Invoke (MlNativeEngine), vedi piano ML §1.1.
 *
 * Device auto-detect: GMB_ML_DEVICE env (cpu|cuda|rocm|mlx|auto) → altrimenti
 * probe host (Metal su Apple, poi CUDA, ROCm, fallback CPU). Idempotente: mai
 * errori hard se torch non è presente — tutte le funzioni ritornano GMB_ML_NOTLOADED.
 * ============================================================================ */

#include "gmb_ml.h"
#include "gmb_torch.h"
#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

/* ── costanti / stati ─────────────────────────────────────────────────────── */

#define GMB_TORCH_MAX_ERR 256

static int g_ml_loaded = 0;        /* host caricato e simboli risolti */
static int g_ml_tried = 0;         /* tentativo di caricamento già eseguito */
static int g_ml_active_dev = GMB_ML_DEV_CPU;
static char g_ml_err[GMB_TORCH_MAX_ERR] = "";
static void* g_ml_handle = NULL;   /* handle dlopen della lib host */
static char g_ml_path[1024] = "";  /* path risolto (ultimo caricamento) */
static char g_ml_override_dir[1024] = ""; /* TENSOR SET LIB PATH */

static void gml_set_err(const char* fmt, const char* a, const char* b);

/* ── vtable host (simboli risolti da gmb_ml.h) ────────────────────────────── */

typedef struct {
    /* device */
    int (*init)(int);
    int (*device_available)(int);
    int (*device_name)(int, char*, int);
    int (*active_device)(void);
    int (*set_active_device)(int);
    /* tensor */
    int64_t (*tensor_create)(int, int, int);
    int64_t (*tensor_from)(int, int, const double*, int);
    int64_t (*text_encode)(const char*);
    int     (*text_decode)(int64_t, char*, int);
    int64_t (*dataset_load_text)(const char*, const char*);
    /* NCM (Next-Concept Modeling): generazione testo, vedi gmb_host_ml.cpp */
    int64_t     (*ncm_load)(const char*, const char*, const char*, int);
    void        (*ncm_free)(int64_t);
    int         (*ncm_generate)(int64_t, int, int64_t, char*, int);
    const char* (*ncm_text)(int64_t, int, int64_t);
    const char* (*host_last_error)(void);
    int (*imatrix_layer_energy)(const char*, double*, int);
    int (*imatrix_rank_map)(const char*, int, int, int*, int);
    int64_t (*tensor_zeros)(int, int, int);
    int64_t (*tensor_ones)(int, int, int);
    int64_t (*tensor_eye)(int, int);
    int64_t (*tensor_randn)(int, int, int);
    void (*tensor_free)(int64_t);
    int (*tensor_toarray)(int64_t, double*, int);
    int (*tensor_rows)(int64_t);
    int (*tensor_cols)(int64_t);
    double (*tensor_get)(int64_t, int, int);
    int (*tensor_set)(int64_t, int, int, double);
    int64_t (*tensor_add)(int64_t, int64_t);
    int64_t (*tensor_sub)(int64_t, int64_t);
    int64_t (*tensor_mul)(int64_t, int64_t);
    int64_t (*tensor_div)(int64_t, int64_t);
    int64_t (*tensor_matmul)(int64_t, int64_t);
    int64_t (*tensor_relu)(int64_t);
    int64_t (*tensor_sigmoid)(int64_t);
    int64_t (*tensor_tanh)(int64_t);
    int64_t (*tensor_softmax)(int64_t);
    int64_t (*tensor_exp)(int64_t);
    int64_t (*tensor_log)(int64_t);
    int64_t (*tensor_sqrt)(int64_t);
    int64_t (*tensor_pow)(int64_t, double);
    int64_t (*tensor_abs)(int64_t);
    double (*tensor_sum)(int64_t);
    double (*tensor_mean)(int64_t);
    double (*tensor_max)(int64_t);
    double (*tensor_min)(int64_t);
    int64_t (*tensor_reshape)(int64_t, int, int);
    int64_t (*tensor_transpose)(int64_t);
    int (*tensor_allclose)(int64_t, int64_t, double);
    int64_t (*tensor_gelu)(int64_t);
    int64_t (*tensor_permute)(int64_t, int, int);
    int64_t (*tensor_slice)(int64_t, int, int, int);
    int64_t (*tensor_concat)(int64_t, int64_t);
    int64_t (*tensor_stack)(int64_t, int64_t);
    int64_t (*tensor_split)(int64_t, int, int);
    int64_t (*tensor_topk)(int64_t, int);
    int64_t (*tensor_argsort)(int64_t);
    int64_t (*tensor_argmax)(int64_t);
    int64_t (*tensor_onehot)(int, int);
    int64_t (*tensor_clamp)(int64_t, double, double);
    int64_t (*tensor_normalize)(int64_t);
    int64_t (*tensor_standardize)(int64_t);
    double (*similarity)(int64_t, int64_t, const char*);
    int (*tensor_dtype)(int64_t, char*, int);
    int (*tensor_device)(int64_t, char*, int);
    int (*available)(char*, int);
    int (*model_load)(int64_t, const char*);
    int (*model_save)(int64_t, const char*);
    int (*model_reset)(int64_t);
    int64_t (*model_input)(int64_t);
    int64_t (*model_output)(int64_t);
    double (*loss)(int64_t, int64_t, const char*);
    double (*accuracy)(int64_t, int64_t);
    /* model / training */
    int64_t (*model_create)(const char*, const char*);
    void (*model_free)(int64_t);
    int (*model_train_step)(int64_t, int64_t, int64_t, const char*, const char*, double);
    int (*model_train_step_ex)(int64_t, int64_t, int64_t, const char*, const char*, double, const char*);
    double (*model_dpo_step)(int64_t, int64_t, int64_t, int64_t, int64_t, const char*);
    int (*model_export)(int64_t, const char*, const char*);    double (*model_loss)(int64_t, int64_t, int64_t);
    int64_t (*model_forward)(int64_t, int64_t);
    int64_t (*model_parameters)(int64_t);
    int (*model_state)(int64_t, char*, int);
} gmb_ml_vtable_t;

static gmb_ml_vtable_t g_vt;

/* ── dlopen portabile ─────────────────────────────────────────────────────── */

#if defined(_WIN32)
static void* gml_dlopen(const char* path) { return (void*)LoadLibraryA(path); }
static void* gml_dlsym(void* h, const char* name) { return (void*)GetProcAddress((HMODULE)h, name); }
static void  gml_dlclose(void* h) { if (h) FreeLibrary((HMODULE)h); }
static const char* gml_dlerror(void) { return "LoadLibrary failed"; }
#else
static void* gml_dlopen(const char* path) { return dlopen(path, RTLD_NOW | RTLD_GLOBAL); }
static void* gml_dlsym(void* h, const char* name) { return dlsym(h, name); }
static void  gml_dlclose(void* h) { if (h) dlclose(h); }
static const char* gml_dlerror(void) { return dlerror(); }
#endif

static int gml_file_exists(const char* path);

/* ── rid (platforma + architettura) — coincide con resources/libs/<rid> ───── */

static const char* gml_rid(void)
{
#if defined(_WIN32)
#  if defined(_M_ARM64)
    return "win-arm64";
#  else
    return "win-x64";
#  endif
#else
    const char* os = "unknown";
    const char* arch = "unknown";
    FILE* p = popen("uname -s 2>/dev/null", "r");
    char buf[64] = "";
    if (p) { if (fgets(buf, sizeof(buf), p)) os = buf; pclose(p); }
    if (strstr(buf, "Darwin") != NULL) os = "osx";
    else if (strstr(buf, "Linux") != NULL) os = "linux";
    p = popen("uname -m 2>/dev/null", "r");
    buf[0] = '\0';
    if (p) { if (fgets(buf, sizeof(buf), p)) arch = buf; pclose(p); }
    if (strstr(buf, "arm64") != NULL || strstr(buf, "aarch64") != NULL) arch = "arm64";
    else if (strstr(buf, "x86_64") != NULL || strstr(buf, "amd64") != NULL) arch = "x64";
    /* rid come le cartelle di resources/libs: osx-arm64, osx-x64, linux-... */
    return os[0] == 'o' ? (arch[0] == 'a' ? "osx-arm64" : "osx-x64")
         : os[0] == 'l' ? (arch[0] == 'a' ? "linux-arm64" : "linux-x64")
         : "unknown";
#endif
}

static const char* gml_lib_basename(void)
{
#if defined(_WIN32)
    return "gmb_torch.dll";
#elif defined(__APPLE__)
    return "libgmb_torch.dylib";
#else
    return "libgmb_torch.so";
#endif
}

/* Path candidati della lib host (ordine): component cache <root>/libs-runtime/ml/<rid>/
 * (gmb_paths_ml_dir — dove ML SETUP/`gmb ml setup`/SETUP TORCH scarica; default,
 * configurabile in system.settings.json sezione "paths"), poi GMB_TORCH_PATH env, poi
 * cwd/Resources/libs, poi parenti. */
static int gml_find_lib(char* out, int cap)
{
    const char* rid = gml_rid();
    /* override esplicito (TENSOR SET LIB PATH): SOLO questa directory. */
    if (g_ml_override_dir[0])
    {
        snprintf(out, (size_t)cap, "%s/%s", g_ml_override_dir, gml_lib_basename());
        return 1;
    }
    {
        char mlc[1024];
        gmb_paths_ml_dir(rid, mlc, sizeof(mlc));
        snprintf(out, (size_t)cap, "%s/%s", mlc, gml_lib_basename());
        if (gml_file_exists(out)) return 1;
    }
    const char* env = gmb_opt_env("GMB_TORCH_PATH");
    if (env && *env)
    {
        snprintf(out, (size_t)cap, "%s/%s", env, gml_lib_basename());
        return 1;
    }
    const char* rels[] = {
        "resources/libs/%s/%s",
        "../resources/libs/%s/%s",
        "../../resources/libs/%s/%s",
        "libs/%s/%s",
    };
    for (size_t i = 0; i < sizeof(rels) / sizeof(rels[0]); i++)
    {
        snprintf(out, (size_t)cap, rels[i], rid, gml_lib_basename());
        if (gml_file_exists(out)) return 1;
    }
    return 0;
}

/* esistenza file (senza caricarlo: niente side-effect dlopen nel probe). */
static int gml_file_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* ── caricamento lazy ─────────────────────────────────────────────────────── */

/* memcpy per void* → funzione: legale sia in C sia in C++ (dlsym ritorna void*). */
#define GML_RESOLVE(fn, name) do { void* _s = gml_dlsym(h, name); memcpy(&g_vt.fn, &_s, sizeof(_s)); } while (0)

static int gml_load(void)
{
    if (g_ml_tried) return g_ml_loaded ? GMB_ML_OK : GMB_ML_NOTLOADED;
    g_ml_tried = 1;
    char path[1024];
    if (!gml_find_lib(path, (int)sizeof(path)))
    {
        gml_set_err("host ML non trovato (torch non disponibile): %s", gml_lib_basename(), NULL);
        return GMB_ML_NOTLOADED;
    }
    void* h = gml_dlopen(path);
    if (!h)
    {
        gml_set_err("host ML: %s", gml_dlerror(), NULL);
        return GMB_ML_NOTLOADED;
    }
    snprintf(g_ml_path, sizeof(g_ml_path), "%s", path);
    memset(&g_vt, 0, sizeof(g_vt));
    GML_RESOLVE(init, "gmb_ml_init");
    GML_RESOLVE(device_available, "gmb_ml_device_available");
    GML_RESOLVE(device_name, "gmb_ml_device_name");
    GML_RESOLVE(active_device, "gmb_ml_active_device");
    GML_RESOLVE(set_active_device, "gmb_ml_set_active_device");
    GML_RESOLVE(tensor_create, "gmb_ml_tensor_create");
    GML_RESOLVE(tensor_from, "gmb_ml_tensor_from");
    GML_RESOLVE(text_encode, "gmb_ml_text_encode");
    GML_RESOLVE(text_decode, "gmb_ml_text_decode");
    GML_RESOLVE(dataset_load_text, "gmb_ml_dataset_load_text");
    GML_RESOLVE(ncm_load, "gmb_ml_ncm_load");
    GML_RESOLVE(ncm_free, "gmb_ml_ncm_free");
    GML_RESOLVE(ncm_generate, "gmb_ml_ncm_generate");
    GML_RESOLVE(ncm_text, "gmb_ml_ncm_text");
    GML_RESOLVE(host_last_error, "gmb_ml_last_error");
    GML_RESOLVE(imatrix_layer_energy, "gmb_ml_imatrix_layer_energy");
    GML_RESOLVE(imatrix_rank_map, "gmb_ml_imatrix_rank_map");
    GML_RESOLVE(tensor_zeros, "gmb_ml_tensor_zeros");
    GML_RESOLVE(tensor_ones, "gmb_ml_tensor_ones");
    GML_RESOLVE(tensor_eye, "gmb_ml_tensor_eye");
    GML_RESOLVE(tensor_randn, "gmb_ml_tensor_randn");
    GML_RESOLVE(tensor_free, "gmb_ml_tensor_free");
    GML_RESOLVE(tensor_toarray, "gmb_ml_tensor_toarray");
    GML_RESOLVE(tensor_rows, "gmb_ml_tensor_rows");
    GML_RESOLVE(tensor_cols, "gmb_ml_tensor_cols");
    GML_RESOLVE(tensor_get, "gmb_ml_tensor_get");
    GML_RESOLVE(tensor_set, "gmb_ml_tensor_set");
    GML_RESOLVE(tensor_add, "gmb_ml_tensor_add");
    GML_RESOLVE(tensor_sub, "gmb_ml_tensor_sub");
    GML_RESOLVE(tensor_mul, "gmb_ml_tensor_mul");
    GML_RESOLVE(tensor_div, "gmb_ml_tensor_div");
    GML_RESOLVE(tensor_matmul, "gmb_ml_tensor_matmul");
    GML_RESOLVE(tensor_relu, "gmb_ml_tensor_relu");
    GML_RESOLVE(tensor_sigmoid, "gmb_ml_tensor_sigmoid");
    GML_RESOLVE(tensor_tanh, "gmb_ml_tensor_tanh");
    GML_RESOLVE(tensor_softmax, "gmb_ml_tensor_softmax");
    GML_RESOLVE(tensor_exp, "gmb_ml_tensor_exp");
    GML_RESOLVE(tensor_log, "gmb_ml_tensor_log");
    GML_RESOLVE(tensor_sqrt, "gmb_ml_tensor_sqrt");
    GML_RESOLVE(tensor_pow, "gmb_ml_tensor_pow");
    GML_RESOLVE(tensor_abs, "gmb_ml_tensor_abs");
    GML_RESOLVE(tensor_sum, "gmb_ml_tensor_sum");
    GML_RESOLVE(tensor_mean, "gmb_ml_tensor_mean");
    GML_RESOLVE(tensor_max, "gmb_ml_tensor_max");
    GML_RESOLVE(tensor_min, "gmb_ml_tensor_min");
    GML_RESOLVE(tensor_reshape, "gmb_ml_tensor_reshape");
    GML_RESOLVE(tensor_transpose, "gmb_ml_tensor_transpose");
    GML_RESOLVE(tensor_allclose, "gmb_ml_tensor_allclose");
    GML_RESOLVE(tensor_gelu, "gmb_ml_tensor_gelu");
    GML_RESOLVE(tensor_permute, "gmb_ml_tensor_permute");
    GML_RESOLVE(tensor_slice, "gmb_ml_tensor_slice");
    GML_RESOLVE(tensor_concat, "gmb_ml_tensor_concat");
    GML_RESOLVE(tensor_stack, "gmb_ml_tensor_stack");
    GML_RESOLVE(tensor_split, "gmb_ml_tensor_split");
    GML_RESOLVE(tensor_topk, "gmb_ml_tensor_topk");
    GML_RESOLVE(tensor_argsort, "gmb_ml_tensor_argsort");
    GML_RESOLVE(tensor_argmax, "gmb_ml_tensor_argmax");
    GML_RESOLVE(tensor_onehot, "gmb_ml_tensor_onehot");
    GML_RESOLVE(tensor_clamp, "gmb_ml_tensor_clamp");
    GML_RESOLVE(tensor_normalize, "gmb_ml_tensor_normalize");
    GML_RESOLVE(tensor_standardize, "gmb_ml_tensor_standardize");
    GML_RESOLVE(similarity, "gmb_ml_similarity");
    GML_RESOLVE(tensor_dtype, "gmb_ml_tensor_dtype");
    GML_RESOLVE(tensor_device, "gmb_ml_tensor_device");
    GML_RESOLVE(available, "gmb_ml_available");
    GML_RESOLVE(model_load, "gmb_ml_model_load");
    GML_RESOLVE(model_save, "gmb_ml_model_save");
    GML_RESOLVE(model_reset, "gmb_ml_model_reset");
    GML_RESOLVE(model_input, "gmb_ml_model_input");
    GML_RESOLVE(model_output, "gmb_ml_model_output");
    GML_RESOLVE(loss, "gmb_ml_loss");
    GML_RESOLVE(accuracy, "gmb_ml_accuracy");
    GML_RESOLVE(model_create, "gmb_ml_model_create");
    GML_RESOLVE(model_free, "gmb_ml_model_free");
    GML_RESOLVE(model_train_step, "gmb_ml_model_train_step");
    GML_RESOLVE(model_train_step_ex, "gmb_ml_model_train_step_ex");
    GML_RESOLVE(model_dpo_step, "gmb_ml_model_dpo_step");
    GML_RESOLVE(model_export, "gmb_ml_model_export");
    GML_RESOLVE(model_loss, "gmb_ml_model_loss");
    GML_RESOLVE(model_forward, "gmb_ml_model_forward");
    GML_RESOLVE(model_parameters, "gmb_ml_model_parameters");
    GML_RESOLVE(model_state, "gmb_ml_model_state");
    if (!g_vt.init || !g_vt.tensor_create)
    {
        gml_dlclose(h);
        gml_set_err("host ML: superficie incompleta (simboli gmb_ml_* mancanti)", NULL, NULL);
        return GMB_ML_NOTLOADED;
    }
    g_ml_handle = h;
    g_ml_loaded = 1;
    /* device attivo: env override, poi probe host (auto). */
    const char* envDev = gmb_opt_env("GMB_ML_DEVICE");
    int dev = GMB_ML_DEV_CPU;
    if (envDev)
    {
        if (strcmp(envDev, "cuda") == 0) dev = GMB_ML_DEV_CUDA;
        else if (strcmp(envDev, "rocm") == 0) dev = GMB_ML_DEV_ROCM;
        else if (strcmp(envDev, "mlx") == 0 || strcmp(envDev, "metal") == 0) dev = GMB_ML_DEV_METAL;
    }
    else
    {
        if (g_vt.device_available && g_vt.device_available(GMB_ML_DEV_METAL)) dev = GMB_ML_DEV_METAL;
        else if (g_vt.device_available && g_vt.device_available(GMB_ML_DEV_CUDA)) dev = GMB_ML_DEV_CUDA;
        else if (g_vt.device_available && g_vt.device_available(GMB_ML_DEV_ROCM)) dev = GMB_ML_DEV_ROCM;
    }
    g_ml_active_dev = dev;
    return GMB_ML_OK;
}

static void gml_set_err(const char* fmt, const char* a, const char* b)
{
    snprintf(g_ml_err, sizeof(g_ml_err), fmt, a ? a : "", b ? b : "");
}

/* ── dispatch device ──────────────────────────────────────────────────────── */

GMB_ML_API int gmb_ml_init(int device)
{
    int rc = gml_load();
    if (rc != GMB_ML_OK) return rc;
    int r = g_vt.init ? g_vt.init(device) : GMB_ML_ERR;
    if (r == GMB_ML_OK) g_ml_active_dev = device;
    return r;
}

GMB_ML_API int gmb_ml_device_available(int device)
{
    if (device == GMB_ML_DEV_CPU) return 1;   /* CPU sempre disponibile */
    int rc = gml_load();
    if (rc != GMB_ML_OK) return 0;
    return g_vt.device_available ? g_vt.device_available(device) : 0;
}

GMB_ML_API int gmb_ml_device_name(int device, char* out, int cap)
{
    if (out && cap > 0) out[0] = '\0';
    switch (device)
    {
        case GMB_ML_DEV_CPU:   if (out && cap > 0) snprintf(out, (size_t)cap, "CPU"); return 1;
        case GMB_ML_DEV_CUDA:  if (out && cap > 0) snprintf(out, (size_t)cap, "CUDA"); return 1;
        case GMB_ML_DEV_ROCM:  if (out && cap > 0) snprintf(out, (size_t)cap, "ROCm"); return 1;
        case GMB_ML_DEV_METAL: if (out && cap > 0) snprintf(out, (size_t)cap, "Metal"); return 1;
        default: return 0;
    }
}

GMB_ML_API int gmb_ml_active_device(void)
{
    gml_load();
    if (g_vt.active_device) return g_vt.active_device();
    return g_ml_active_dev;
}

GMB_ML_API int gmb_ml_set_active_device(int device)
{
    if (device < GMB_ML_DEV_CPU || device > GMB_ML_DEV_METAL) return GMB_ML_ERR;
    int rc = gml_load();
    if (rc != GMB_ML_OK) { g_ml_active_dev = device; return GMB_ML_OK; }
    g_ml_active_dev = device;
    return g_vt.set_active_device ? g_vt.set_active_device(device) : GMB_ML_OK;
}

/* Imposta il device attivo dal nome ("auto"|"cpu"|"cuda"|"rocm"|"mlx"|"metal").
 * Ritorna GMB_ML_OK(0) oppure GMB_ML_ERR(-1) per nome sconosciuto/device non disponibile. */
GMB_ML_API int gmb_ml_set_active_device_name(const char* name)
{
    if (!name || !*name) return GMB_ML_ERR;
    int dev = GMB_ML_DEV_CPU;
    if (strcmp(name, "cpu") == 0) dev = GMB_ML_DEV_CPU;
    else if (strcmp(name, "cuda") == 0) dev = GMB_ML_DEV_CUDA;
    else if (strcmp(name, "rocm") == 0) dev = GMB_ML_DEV_ROCM;
    else if (strcmp(name, "mlx") == 0 || strcmp(name, "metal") == 0) dev = GMB_ML_DEV_METAL;
    else if (strcmp(name, "auto") != 0) return GMB_ML_ERR;
    if (strcmp(name, "auto") == 0)
    {
        int rc = gml_load();
        if (rc != GMB_ML_OK) return rc;
        if (g_vt.device_available && g_vt.device_available(GMB_ML_DEV_METAL)) dev = GMB_ML_DEV_METAL;
        else if (g_vt.device_available && g_vt.device_available(GMB_ML_DEV_CUDA)) dev = GMB_ML_DEV_CUDA;
        else if (g_vt.device_available && g_vt.device_available(GMB_ML_DEV_ROCM)) dev = GMB_ML_DEV_ROCM;
        else dev = GMB_ML_DEV_CPU;
    }
    return gmb_ml_set_active_device(dev);
}

GMB_ML_API const char* gmb_ml_last_error(void) { return g_ml_err; }

/* ── loader controls (gmb_torch.h) ─────────────────────────────────────────── */

GMB_ML_API int gmb_ml_set_lib_dir(const char* dir)
{
    if (dir && *dir)
        snprintf(g_ml_override_dir, sizeof(g_ml_override_dir), "%s", dir);
    else
        g_ml_override_dir[0] = '\0';
    /* il prossimo accesso riprova con la nuova directory */
    g_ml_tried = 0;
    g_ml_loaded = 0;
    return GMB_ML_OK;
}

GMB_ML_API int gmb_ml_load_lib(void)
{
    int rc = gml_load();
    return rc;
}

GMB_ML_API void gmb_ml_unload_lib(void)
{
    if (g_ml_handle) { gml_dlclose(g_ml_handle); g_ml_handle = NULL; }
    g_ml_loaded = 0;
    g_ml_tried = 0;
    g_ml_path[0] = '\0';
    memset(&g_vt, 0, sizeof(g_vt));
}

GMB_ML_API int gmb_ml_lib_path(char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (g_ml_path[0]) { snprintf(out, (size_t)cap, "%s", g_ml_path); return 1; }
    /* risolvi senza caricare (solo percorso) */
    char p[1024];
    if (gml_find_lib(p, (int)sizeof(p))) { snprintf(out, (size_t)cap, "%s", p); return 1; }
    out[0] = '\0';
    return 0;
}

GMB_ML_API int gmb_ml_loaded(void) { return g_ml_loaded; }

/* ── dispatch tensor (delega all'host; NOTLOADED se torch assente) ────────── */

#define GML_HANDLE(name, ...) \
    do { if (gml_load() != GMB_ML_OK) return GMB_ML_NOTLOADED; return g_vt.name ? g_vt.name(__VA_ARGS__) : GMB_ML_ERR; } while (0)
#define GML_HANDLE_H(name, ...) \
    do { if (gml_load() != GMB_ML_OK) return -1; return g_vt.name ? g_vt.name(__VA_ARGS__) : -1; } while (0)
#define GML_HANDLE_D(name, ...) \
    do { if (gml_load() != GMB_ML_OK) return 0.0; return g_vt.name ? g_vt.name(__VA_ARGS__) : 0.0; } while (0)
#define GML_HANDLE_V(name, ...) \
    do { if (gml_load() != GMB_ML_OK) return; return g_vt.name ? (void)g_vt.name(__VA_ARGS__) : (void)0; } while (0)

GMB_ML_API int64_t gmb_ml_tensor_create(int r, int c, int d) { GML_HANDLE_H(tensor_create, r, c, d); }
GMB_ML_API int64_t gmb_ml_tensor_from(int r, int c, const double* data, int d) { GML_HANDLE_H(tensor_from, r, c, data, d); }
/* L6: pipeline dati testuale toy. Senior: se il simbolo manca nell'host (vecchio), errore pulito. */
GMB_ML_API int64_t gmb_ml_text_encode(const char* text)
{ if (gml_load() != GMB_ML_OK || !g_vt.text_encode) { gml_set_err("%s", "text_encode: host senza supporto (aggiorna libgmb_torch)", NULL); return -1; } return g_vt.text_encode(text); }
GMB_ML_API int gmb_ml_text_decode(int64_t h, char* out, int cap)
{ if (gml_load() != GMB_ML_OK || !g_vt.text_decode) return GMB_ML_ERR; return g_vt.text_decode(h, out, cap); }
GMB_ML_API int64_t gmb_ml_dataset_load_text(const char* path, const char* opts)
{ if (gml_load() != GMB_ML_OK || !g_vt.dataset_load_text) { gml_set_err("%s", "dataset_load_text: host senza supporto (aggiorna libgmb_torch)", NULL); return -1; } return g_vt.dataset_load_text(path, opts); }
/* Le funzioni NCM sono raggiunte SOLO via FFI (LIB "gmb_torch" risolto sul processo stesso in VM/C): senza `used` il linker
 * (-dead_strip) le elimina dall'eseguibile, perche' nessun altro codice le chiama. */
#if defined(__GNUC__) || defined(__clang__)
#  define GML_KEEP __attribute__((used))
#else
#  define GML_KEEP
#endif
/* NCM: l'errore dell'host (messaggio dell'eccezione C++) viene copiato nel last_error del loader. */
static void gml_copy_host_err(void)
{ if (g_vt.host_last_error) { const char* m = g_vt.host_last_error(); if (m && *m) snprintf(g_ml_err, sizeof(g_ml_err), "%s", m); } }
GML_KEEP GMB_ML_API int64_t gmb_ml_ncm_load(const char* gen, const char* tok, const char* vocab, int device)
{ if (gml_load() != GMB_ML_OK || !g_vt.ncm_load) { gml_set_err("%s", "ncm_load: host senza supporto (aggiorna libgmb_torch)", NULL); return 0; }
  int64_t h = g_vt.ncm_load(gen, tok, vocab, device); if (!h) gml_copy_host_err(); return h; }
GML_KEEP GMB_ML_API void gmb_ml_ncm_free(int64_t h)
{ if (gml_load() == GMB_ML_OK && g_vt.ncm_free) g_vt.ncm_free(h); }
GML_KEEP GMB_ML_API int gmb_ml_ncm_generate(int64_t h, int n, int64_t seed, char* out, int cap)
{ if (gml_load() != GMB_ML_OK || !g_vt.ncm_generate) { gml_set_err("%s", "ncm_generate: host senza supporto (aggiorna libgmb_torch)", NULL); return -1; }
  int r = g_vt.ncm_generate(h, n, seed, out, cap); if (r < 0) gml_copy_host_err(); return r; }
GML_KEEP GMB_ML_API const char* gmb_ml_ncm_text(int64_t h, int n, int64_t seed)
{ if (gml_load() != GMB_ML_OK || !g_vt.ncm_text) { gml_set_err("%s", "ncm_text: host senza supporto (aggiorna libgmb_torch)", NULL); return ""; }
  const char* r = g_vt.ncm_text(h, n, seed); if (!r || !*r) gml_copy_host_err(); return r ? r : ""; }
GMB_ML_API int gmb_ml_imatrix_layer_energy(const char* path, double* out, int cap)
{ if (gml_load() != GMB_ML_OK || !g_vt.imatrix_layer_energy) return GMB_ML_ERR; return g_vt.imatrix_layer_energy(path, out, cap); }
GMB_ML_API int gmb_ml_imatrix_rank_map(const char* path, int lo, int hi, int* out, int cap)
{ if (gml_load() != GMB_ML_OK || !g_vt.imatrix_rank_map) return GMB_ML_ERR; return g_vt.imatrix_rank_map(path, lo, hi, out, cap); }
GMB_ML_API int64_t gmb_ml_imatrix_load_t(const char* path)
{ double b[4096]; int n = gmb_ml_imatrix_layer_energy(path, b, 4096); if (n <= 0) return -1; return gmb_ml_tensor_from(n, 1, b, gmb_ml_active_device()); }
GMB_ML_API int64_t gmb_ml_imatrix_ranks_t(const char* path, int lo, int hi)
{ int ri[4096]; int n = gmb_ml_imatrix_rank_map(path, lo, hi, ri, 4096); if (n <= 0) return -1; double b[4096]; for (int i = 0; i < n; i++) b[i] = (double)ri[i]; return gmb_ml_tensor_from(n, 1, b, gmb_ml_active_device()); }
GMB_ML_API int64_t gmb_ml_tensor_zeros(int r, int c, int d) { GML_HANDLE_H(tensor_zeros, r, c, d); }
GMB_ML_API int64_t gmb_ml_tensor_ones(int r, int c, int d) { GML_HANDLE_H(tensor_ones, r, c, d); }
GMB_ML_API int64_t gmb_ml_tensor_eye(int n, int d) { GML_HANDLE_H(tensor_eye, n, d); }
GMB_ML_API int64_t gmb_ml_tensor_randn(int r, int c, int d) { GML_HANDLE_H(tensor_randn, r, c, d); }
GMB_ML_API void    gmb_ml_tensor_free(int64_t h) { GML_HANDLE_V(tensor_free, h); }
GMB_ML_API int     gmb_ml_tensor_toarray(int64_t h, double* o, int m) { GML_HANDLE(tensor_toarray, h, o, m); }
GMB_ML_API int     gmb_ml_tensor_rows(int64_t h) { GML_HANDLE(tensor_rows, h); }
GMB_ML_API int     gmb_ml_tensor_cols(int64_t h) { GML_HANDLE(tensor_cols, h); }
GMB_ML_API double  gmb_ml_tensor_get(int64_t h, int r, int c) { GML_HANDLE_D(tensor_get, h, r, c); }
GMB_ML_API int     gmb_ml_tensor_set(int64_t h, int r, int c, double v) { GML_HANDLE(tensor_set, h, r, c, v); }
GMB_ML_API int64_t gmb_ml_tensor_add(int64_t a, int64_t b) { GML_HANDLE_H(tensor_add, a, b); }
GMB_ML_API int64_t gmb_ml_tensor_sub(int64_t a, int64_t b) { GML_HANDLE_H(tensor_sub, a, b); }
GMB_ML_API int64_t gmb_ml_tensor_mul(int64_t a, int64_t b) { GML_HANDLE_H(tensor_mul, a, b); }
GMB_ML_API int64_t gmb_ml_tensor_div(int64_t a, int64_t b) { GML_HANDLE_H(tensor_div, a, b); }
GMB_ML_API int64_t gmb_ml_tensor_matmul(int64_t a, int64_t b) { GML_HANDLE_H(tensor_matmul, a, b); }
GMB_ML_API int64_t gmb_ml_tensor_relu(int64_t a) { GML_HANDLE_H(tensor_relu, a); }
GMB_ML_API int64_t gmb_ml_tensor_sigmoid(int64_t a) { GML_HANDLE_H(tensor_sigmoid, a); }
GMB_ML_API int64_t gmb_ml_tensor_tanh(int64_t a) { GML_HANDLE_H(tensor_tanh, a); }
GMB_ML_API int64_t gmb_ml_tensor_softmax(int64_t a) { GML_HANDLE_H(tensor_softmax, a); }
GMB_ML_API int64_t gmb_ml_tensor_exp(int64_t a) { GML_HANDLE_H(tensor_exp, a); }
GMB_ML_API int64_t gmb_ml_tensor_log(int64_t a) { GML_HANDLE_H(tensor_log, a); }
GMB_ML_API int64_t gmb_ml_tensor_sqrt(int64_t a) { GML_HANDLE_H(tensor_sqrt, a); }
GMB_ML_API int64_t gmb_ml_tensor_pow(int64_t a, double p) { GML_HANDLE_H(tensor_pow, a, p); }
GMB_ML_API int64_t gmb_ml_tensor_abs(int64_t a) { GML_HANDLE_H(tensor_abs, a); }
GMB_ML_API double  gmb_ml_tensor_sum(int64_t a) { GML_HANDLE_D(tensor_sum, a); }
GMB_ML_API double  gmb_ml_tensor_mean(int64_t a) { GML_HANDLE_D(tensor_mean, a); }
GMB_ML_API double  gmb_ml_tensor_max(int64_t a) { GML_HANDLE_D(tensor_max, a); }
GMB_ML_API double  gmb_ml_tensor_min(int64_t a) { GML_HANDLE_D(tensor_min, a); }
GMB_ML_API int64_t gmb_ml_tensor_reshape(int64_t a, int r, int c) { GML_HANDLE_H(tensor_reshape, a, r, c); }
GMB_ML_API int64_t gmb_ml_tensor_transpose(int64_t a) { GML_HANDLE_H(tensor_transpose, a); }
GMB_ML_API int     gmb_ml_tensor_allclose(int64_t a, int64_t b, double tol) { GML_HANDLE(tensor_allclose, a, b, tol); }
/* Varianti ALLOC (per l'emitter C99: l'array e' allocato e la lunghezza e' out-param). */
double* gmb_ml_tensor_toarray_alloc(int64_t h, gmb_int_t* outn)
{
    int r = gmb_ml_tensor_rows(h), c = gmb_ml_tensor_cols(h);
    *outn = 0;
    if (r <= 0 || c <= 0) return NULL;
    int n = r * c;
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) return NULL;
    int got = gmb_ml_tensor_toarray(h, out, n);
    if (got >= 0) { *outn = got; return out; }
    free(out);
    return NULL;
}
double* gmb_ml_tensor_shape_alloc(int64_t h, gmb_int_t* outn)
{
    int r = gmb_ml_tensor_rows(h), c = gmb_ml_tensor_cols(h);
    *outn = 0;
    if (r < 0 || c < 0) return NULL;
    double* out = (double*)malloc(sizeof(double) * 2);
    if (!out) return NULL;
    out[0] = r; out[1] = c; *outn = 2;
    return out;
}
GMB_ML_API int64_t gmb_ml_tensor_gelu(int64_t a) { GML_HANDLE_H(tensor_gelu, a); }
GMB_ML_API int64_t gmb_ml_tensor_permute(int64_t a, int d0, int d1) { GML_HANDLE_H(tensor_permute, a, d0, d1); }
GMB_ML_API int64_t gmb_ml_tensor_slice(int64_t a, int dim, int start, int end) { GML_HANDLE_H(tensor_slice, a, dim, start, end); }
GMB_ML_API int64_t gmb_ml_tensor_concat(int64_t a, int64_t b) { GML_HANDLE_H(tensor_concat, a, b); }
GMB_ML_API int64_t gmb_ml_tensor_stack(int64_t a, int64_t b) { GML_HANDLE_H(tensor_stack, a, b); }
GMB_ML_API int64_t gmb_ml_tensor_split(int64_t a, int index, int n) { GML_HANDLE_H(tensor_split, a, index, n); }
GMB_ML_API int64_t gmb_ml_tensor_topk(int64_t a, int k) { GML_HANDLE_H(tensor_topk, a, k); }
GMB_ML_API int64_t gmb_ml_tensor_argsort(int64_t a) { GML_HANDLE_H(tensor_argsort, a); }
GMB_ML_API int64_t gmb_ml_tensor_argmax(int64_t a) { GML_HANDLE_H(tensor_argmax, a); }
GMB_ML_API int64_t gmb_ml_tensor_onehot(int idx, int n) { GML_HANDLE_H(tensor_onehot, idx, n); }
GMB_ML_API int64_t gmb_ml_tensor_clamp(int64_t a, double lo, double hi) { GML_HANDLE_H(tensor_clamp, a, lo, hi); }
GMB_ML_API int64_t gmb_ml_tensor_normalize(int64_t a) { GML_HANDLE_H(tensor_normalize, a); }
GMB_ML_API int64_t gmb_ml_tensor_standardize(int64_t a) { GML_HANDLE_H(tensor_standardize, a); }
GMB_ML_API double gmb_ml_similarity(int64_t a, int64_t b, const char* metric) { GML_HANDLE_D(similarity, a, b, metric); }
GMB_ML_API int gmb_ml_tensor_dtype(int64_t h, char* out, int cap) { GML_HANDLE(tensor_dtype, h, out, cap); }
GMB_ML_API int gmb_ml_tensor_device(int64_t h, char* out, int cap) { GML_HANDLE(tensor_device, h, out, cap); }
GMB_ML_API int gmb_ml_available(char* out, int cap) { GML_HANDLE(available, out, cap); }
GMB_ML_API int gmb_ml_model_reset(int64_t h) { GML_HANDLE(model_reset, h); }
GMB_ML_API int64_t gmb_ml_model_input(int64_t h) { GML_HANDLE_H(model_input, h); }
GMB_ML_API int64_t gmb_ml_model_output(int64_t h) { GML_HANDLE_H(model_output, h); }
GMB_ML_API double gmb_ml_loss(int64_t y, int64_t yhat, const char* name) { GML_HANDLE_D(loss, y, yhat, name); }
GMB_ML_API double gmb_ml_accuracy(int64_t y, int64_t yhat) { GML_HANDLE_D(accuracy, y, yhat); }
/* Varianti ALLOC (array-returning per C99/PCode): applicano l'op e fanno toarray. */
double* gmb_ml_tensor_topk_alloc(int64_t h, int k, gmb_int_t* outn)
{ int64_t r = gmb_ml_tensor_topk(h, k); double* out = gmb_ml_tensor_toarray_alloc(r, outn); gmb_ml_tensor_free(r); return out; }
double* gmb_ml_tensor_argsort_alloc(int64_t h, gmb_int_t* outn)
{ int64_t r = gmb_ml_tensor_argsort(h); double* out = gmb_ml_tensor_toarray_alloc(r, outn); gmb_ml_tensor_free(r); return out; }
double* gmb_ml_tensor_onehot_alloc(int idx, int n, gmb_int_t* outn)
{ int64_t r = gmb_ml_tensor_onehot(idx, n); double* out = gmb_ml_tensor_toarray_alloc(r, outn); gmb_ml_tensor_free(r); return out; }
double* gmb_ml_tensor_clamp_alloc(int64_t h, double lo, double hi, gmb_int_t* outn)
{ int64_t r = gmb_ml_tensor_clamp(h, lo, hi); double* out = gmb_ml_tensor_toarray_alloc(r, outn); gmb_ml_tensor_free(r); return out; }
double* gmb_ml_tensor_normalize_alloc(int64_t h, gmb_int_t* outn)
{ int64_t r = gmb_ml_tensor_normalize(h); double* out = gmb_ml_tensor_toarray_alloc(r, outn); gmb_ml_tensor_free(r); return out; }
double* gmb_ml_tensor_standardize_alloc(int64_t h, gmb_int_t* outn)
{ int64_t r = gmb_ml_tensor_standardize(h); double* out = gmb_ml_tensor_toarray_alloc(r, outn); gmb_ml_tensor_free(r); return out; }

/* ── dispatch model / training ────────────────────────────────────────────── */

GMB_ML_API int64_t gmb_ml_model_create(const char* arch, const char* cfg) { GML_HANDLE_H(model_create, arch, cfg); }
GMB_ML_API void    gmb_ml_model_free(int64_t h) { GML_HANDLE_V(model_free, h); }
GMB_ML_API int     gmb_ml_model_load(int64_t h, const char* p) { GML_HANDLE(model_load, h, p); }
GMB_ML_API int     gmb_ml_model_save(int64_t h, const char* p) { GML_HANDLE(model_save, h, p); }
GMB_ML_API int     gmb_ml_model_train_step(int64_t m, int64_t x, int64_t y, const char* o, const char* l, double lr)
{ GML_HANDLE(model_train_step, m, x, y, o, l, lr); }
/* L2: variante con opts JSON (puo' mancare nella lib host vecchia: fallback a train_step base). */
GMB_ML_API int     gmb_ml_model_train_step_ex(int64_t m, int64_t x, int64_t y, const char* o, const char* l, double lr, const char* opts)
{ if (!g_vt.model_train_step_ex) return gmb_ml_model_train_step(m, x, y, o, l, lr); GML_HANDLE(model_train_step_ex, m, x, y, o, l, lr, opts); }
GMB_ML_API int gmb_ml_model_export(int64_t h, const char* p, const char* fmt)
{ if (gml_load() != GMB_ML_OK || !g_vt.model_export) return GMB_ML_ERR; return g_vt.model_export(h, p, fmt); }
GMB_ML_API double  gmb_ml_model_loss(int64_t m, int64_t x, int64_t y) { GML_HANDLE_D(model_loss, m, x, y); }
/* L5: DPO/ORPO (mr<=0 = ORPO). Senza simbolo nell'host (vecchio) -> -1.0. */
GMB_ML_API double  gmb_ml_model_dpo_step(int64_t ms, int64_t mr, int64_t x, int64_t c, int64_t r, const char* opts)
{ if (gml_load() != GMB_ML_OK || !g_vt.model_dpo_step) return -1.0; return g_vt.model_dpo_step(ms, mr, x, c, r, opts); }
GMB_ML_API int64_t gmb_ml_model_forward(int64_t m, int64_t i) { GML_HANDLE_H(model_forward, m, i); }
GMB_ML_API int64_t gmb_ml_model_parameters(int64_t m) { GML_HANDLE_H(model_parameters, m); }
GMB_ML_API int     gmb_ml_model_state(int64_t m, char* o, int cap) { GML_HANDLE(model_state, m, o, cap); }
