/* ============================================================================
 * gmb_ml.h — superficie C stabile dell'host ML (PyTorch/libtorch) per GMBasic
 * File: native/gmb_torch/include/gmb_ml.h
 *
 * ABI stabile tra:
 *   - il runtime C (gmb_torch.c, dlopen da resources/libs/<rid>/) — VM/C e C99
 *   - il bridge gestito C# (MlNativeEngine, P/Invoke) — ILBackend e PCodeVM
 *   - l'host C++ (gmb_host_ml / libgmb_torch) che implementa la superficie con libtorch
 *
 * Convenzioni (come gmb_db.h): handle opachi int64_t, errori come codice,
 * gmb_ml_last_error() per il messaggio. Niente eccezioni, niente C++ nel C.
 * Device: 0=CPU, 1=CUDA, 2=ROCm, 3=Metal.
 * ============================================================================ */

#ifndef GMB_ML_H
#define GMB_ML_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(GMB_ML_BUILD_SHARED)
#  define GMB_ML_API __declspec(dllexport)
#else
#  define GMB_ML_API
#endif

#define GMB_ML_OK         0
#define GMB_ML_ERR       -1
#define GMB_ML_NOTLOADED -2   /* host lib non caricata (torch non disponibile) */
#define GMB_ML_NOOP      -3   /* op non implementata dall'host */

#define GMB_ML_DEV_CPU    0
#define GMB_ML_DEV_CUDA   1
#define GMB_ML_DEV_ROCM   2
#define GMB_ML_DEV_METAL  3
#define GMB_ML_DEV_AUTO   4   /* solo per TENSOR SET ACTIVE DEVICE "auto" */

/* ── device ──────────────────────────────────────────────────────────────── */

/* Inizializza il device (context creato, driver caricato). -1 se non disponibile. */
GMB_ML_API int  gmb_ml_init(int device);
/* Il device è usabile? (probe: context + almeno 1 device enumerato per GPU). */
GMB_ML_API int  gmb_ml_device_available(int device);
/* Nome umano del device ("Metal", "CUDA 12.x", "CPU"), 0 se sconosciuto. */
GMB_ML_API int  gmb_ml_device_name(int device, char* out, int cap);
/* Device attivo corrente (0..3). */
GMB_ML_API int  gmb_ml_active_device(void);
/* Cambia il device attivo a runtime (i tensor già creati restano sul vecchio). */
GMB_ML_API int  gmb_ml_set_active_device(int device);
GMB_ML_API int  gmb_ml_set_active_device_name(const char* name); /* "auto"|"cpu"|"cuda"|"rocm"|"mlx" */
/* Ultimo errore come stringa statica (thread-local). */
GMB_ML_API const char* gmb_ml_last_error(void);

/* ── tensor (handle opachi int64_t) ───────────────────────────────────────── */

GMB_ML_API int64_t gmb_ml_tensor_create(int rows, int cols, int device);
GMB_ML_API int64_t gmb_ml_tensor_from(int rows, int cols, const double* data, int device);
GMB_ML_API int64_t gmb_ml_tensor_zeros(int rows, int cols, int device);
GMB_ML_API int64_t gmb_ml_tensor_ones(int rows, int cols, int device);
GMB_ML_API int64_t gmb_ml_tensor_eye(int n, int device);
GMB_ML_API int64_t gmb_ml_tensor_randn(int rows, int cols, int device);
GMB_ML_API void    gmb_ml_tensor_free(int64_t h);
GMB_ML_API int     gmb_ml_tensor_toarray(int64_t h, double* out, int max);
GMB_ML_API int     gmb_ml_tensor_rows(int64_t h);
GMB_ML_API int     gmb_ml_tensor_cols(int64_t h);
GMB_ML_API double  gmb_ml_tensor_get(int64_t h, int r, int c);
GMB_ML_API int     gmb_ml_tensor_set(int64_t h, int r, int c, double v);

GMB_ML_API int64_t gmb_ml_tensor_add(int64_t a, int64_t b);
GMB_ML_API int64_t gmb_ml_tensor_sub(int64_t a, int64_t b);
GMB_ML_API int64_t gmb_ml_tensor_mul(int64_t a, int64_t b);
GMB_ML_API int64_t gmb_ml_tensor_div(int64_t a, int64_t b);
GMB_ML_API int64_t gmb_ml_tensor_matmul(int64_t a, int64_t b);
GMB_ML_API int64_t gmb_ml_tensor_relu(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_sigmoid(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_tanh(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_softmax(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_exp(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_log(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_sqrt(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_pow(int64_t a, double p);
GMB_ML_API int64_t gmb_ml_tensor_abs(int64_t a);
GMB_ML_API double  gmb_ml_tensor_sum(int64_t a);
GMB_ML_API double  gmb_ml_tensor_mean(int64_t a);
GMB_ML_API double  gmb_ml_tensor_max(int64_t a);
GMB_ML_API double  gmb_ml_tensor_min(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_reshape(int64_t a, int rows, int cols);
GMB_ML_API int64_t gmb_ml_tensor_transpose(int64_t a);
GMB_ML_API int     gmb_ml_tensor_allclose(int64_t a, int64_t b, double tol);
GMB_ML_API int64_t gmb_ml_tensor_gelu(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_permute(int64_t a, int d0, int d1);
GMB_ML_API int64_t gmb_ml_tensor_slice(int64_t a, int dim, int start, int end);
GMB_ML_API int64_t gmb_ml_tensor_concat(int64_t a, int64_t b);
GMB_ML_API int64_t gmb_ml_tensor_stack(int64_t a, int64_t b);
GMB_ML_API int64_t gmb_ml_tensor_split(int64_t a, int index, int n);
/* Bonus (piano ML §3.6, da libtorch): topk/argsort/onehot/clamp/normalize/standardize
 * ritornano un NUOVO handle; similarity ritorna uno scalare. */
GMB_ML_API int64_t gmb_ml_tensor_topk(int64_t a, int k);
GMB_ML_API int64_t gmb_ml_tensor_argsort(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_argmax(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_onehot(int idx, int n);
GMB_ML_API int64_t gmb_ml_tensor_clamp(int64_t a, double lo, double hi);
GMB_ML_API int64_t gmb_ml_tensor_normalize(int64_t a);
GMB_ML_API int64_t gmb_ml_tensor_standardize(int64_t a);
GMB_ML_API double  gmb_ml_similarity(int64_t a, int64_t b, const char* metric);
/* accesso info / model extras */
GMB_ML_API int gmb_ml_tensor_dtype(int64_t h, char* out, int cap);
GMB_ML_API int gmb_ml_tensor_device(int64_t h, char* out, int cap);
GMB_ML_API int gmb_ml_available(char* out, int cap);            /* lista device disponibili */
GMB_ML_API int gmb_ml_model_load(int64_t h, const char* path);
GMB_ML_API int gmb_ml_model_save(int64_t h, const char* path);
GMB_ML_API int gmb_ml_model_reset(int64_t h);
GMB_ML_API int64_t gmb_ml_model_input(int64_t h);
GMB_ML_API int64_t gmb_ml_model_output(int64_t h);
GMB_ML_API double gmb_ml_loss(int64_t y, int64_t yhat, const char* name);
GMB_ML_API double gmb_ml_accuracy(int64_t y, int64_t yhat);

/* ── model / training ─────────────────────────────────────────────────────── */

/* arch: "linear"|"mlp"|"lstm"|"cnn"; cfg_json: architettura (hidden, attivazioni...). */
GMB_ML_API int64_t gmb_ml_model_create(const char* arch, const char* cfg_json);
GMB_ML_API void    gmb_ml_model_free(int64_t h);
GMB_ML_API int     gmb_ml_model_load(int64_t h, const char* path);   /* torchscript .pt */
GMB_ML_API int     gmb_ml_model_save(int64_t h, const char* path);
/* L9: export con formato (flat = merge+save). Formato ignoto -> errore. */
GMB_ML_API int     gmb_ml_model_export(int64_t h, const char* path, const char* format);
/* Un passo di discesa; opt: "adam"|"sgd"|"adagrad"|"rmsprop"; loss: "mse"|"crossentropy"|"binary". */
GMB_ML_API int     gmb_ml_model_train_step(int64_t model, int64_t x, int64_t y,
                                           const char* opt, const char* loss, double lr);
/* L2: variante con opts JSON (scheduler|warmup_steps|decay_steps|accum_steps|clip_norm|
 * resume_ckpt); NULL = default (const, nessun warmup/accum/clip/resume). */
GMB_ML_API int     gmb_ml_model_train_step_ex(int64_t model, int64_t x, int64_t y,
                                              const char* opt, const char* loss, double lr,
                                              const char* optsJson);
/* L5 DPO/ORPO: un passo preference su id token. mr<=0 = ORPO (senza ref). optsJson:
 * beta (def 0.1), opt, lr + chiavi scheduler L2. Ritorna loss (>=0) o -1.0 in errore. */
GMB_ML_API double  gmb_ml_model_dpo_step(int64_t ms, int64_t mr, int64_t x, int64_t c, int64_t r,
                                         const char* optsJson);
/* L6 pipeline dati testuale TOY (char-level, NON BPE): ogni byte = id 0..255.
 * encode -> tensore [1,n]; decode -> stringa; dataset_load_text -> [n_sample, cutoff_len]. */
GMB_ML_API int64_t gmb_ml_text_encode(const char* text);
GMB_ML_API int     gmb_ml_text_decode(int64_t h, char* out, int cap);
GMB_ML_API int64_t gmb_ml_dataset_load_text(const char* path, const char* optsJson);
/* L11: lettore imatrix. layer_energy -> out[l] (media energie, ritorna n layer);
 * rank_map -> out[l] normalizzato tra min_rank..max_rank. -1 in errore. */
GMB_ML_API int     gmb_ml_imatrix_layer_energy(const char* path, double* out, int cap);
GMB_ML_API int     gmb_ml_imatrix_rank_map(const char* path, int min_rank, int max_rank, int* out, int cap);
/* Varianti che ritornano direttamente il tensore (per l'emitter C99). */
GMB_ML_API int64_t gmb_ml_imatrix_load_t(const char* path);
GMB_ML_API int64_t gmb_ml_imatrix_ranks_t(const char* path, int min_rank, int max_rank);
GMB_ML_API double  gmb_ml_model_loss(int64_t model, int64_t x, int64_t y);
GMB_ML_API int64_t gmb_ml_model_forward(int64_t model, int64_t input);
GMB_ML_API int64_t gmb_ml_model_parameters(int64_t model);   /* conteggio parametri */
GMB_ML_API int     gmb_ml_model_state(int64_t model, char* out, int cap); /* JSON */

/* NCM (Next-Concept Modeling, arXiv 2610.14162): generazione di testo non condizionata con checkpoint HazyResearch/ncm
 * (LM1B small). device: GMB_ML_DEV_*. Ritorna un handle (>0) o 0 (gmb_ml_last_error). */
GMB_ML_API int64_t gmb_ml_ncm_load(const char* generator_pt, const char* tokenizer_pt, const char* vocab_txt, int device);
GMB_ML_API void    gmb_ml_ncm_free(int64_t h);
/* Genera n testi (seed >= 0 fissa il generatore casuale) e li scrive in out separati da '\n'. Ritorna i byte scritti; -1 su errore (handle non valido, n <= 0, buffer troppo piccolo: mai troncato in silenzio). */
GMB_ML_API int     gmb_ml_ncm_generate(int64_t h, int n, int64_t seed, char* out, int cap);
/* Come gmb_ml_ncm_generate ma ritorna i testi (separati da '\n') come stringa C valida fino alla chiamata successiva sullo stesso thread:
 * forma adatta all'FFI di GMBasic (namespace NCM, shared/include/ncm.inc). "" su errore (gmb_ml_last_error). Nessun limite di lunghezza. */
GMB_ML_API const char* gmb_ml_ncm_text(int64_t h, int n, int64_t seed);
/* Modelli condizionati (checkpoint XSum): config.yaml del riferimento + BPE GPT-2 (vocab.json, merges.txt). Ritorna handle > 0 o 0.
 * gmb_ml_ncm_complete: testo generato dal prompt (riassunto), tagliato al primo EOS; cfg_scale < 0 = valore del config. "" su errore.
 * gmb_ml_rouge: F-measure in [0,1] fra riferimento e ipotesi; kind 1 = ROUGE-1, 2 = ROUGE-2, 3 = ROUGE-L (-1 se kind non valido). */
GMB_ML_API int64_t gmb_ml_ncm_load_cond(const char* generator_pt, const char* tokenizer_pt, const char* config_yaml, const char* vocab_json, const char* merges_txt, int device);
GMB_ML_API const char* gmb_ml_ncm_complete(int64_t h, const char* prompt, int64_t seed, double cfg_scale);
GMB_ML_API double gmb_ml_rouge(const char* reference, const char* hypothesis, int kind);

#ifdef __cplusplus
}
#endif

#endif /* GMB_ML_H */
