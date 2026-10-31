/* gmb_model_io.h — Model I/O IN-PROCESS (namespace LLM): convert / quantize / inspect SENZA
 * subprocess. Sorgente UNICA di logica: queste funzioni richiamano le implementazioni C già
 * esistenti (gmb_model_quantize_ex, llmdesc/.gmbarch) — le stesse usate dalla
 * dylib `gmb_ai_modelio` per il P/Invoke dal C#. Sostituiscono i `system("gmb_model_…")` di
 * gmb_embedded.c (AGENTS: niente subprocess per delegare logica C richiamabile). */
#ifndef GMB_MODEL_IO_H
#define GMB_MODEL_IO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Converte un checkpoint HF/MLX in container `GGUF` (full-grid, dense, ctx 4096).
 * 0 = ok, -1 = errore. */
int gmb_model_io_convert(const char* model_dir, const char* out_path);

/* Quantizza un modello HF/MLX (F32/F16/BF16) in MLX affine RTN. `bits` 2..8; `group` <=0 -> 64;
 * `imatrix_path` può essere NULL. 0 = ok, -1 = errore. */
int gmb_model_io_quantize(const char* src_dir, const char* out_dir, int bits, int group,
                          const char* imatrix_path, int expert_bits);

/* Discovery: scrive il report in `out` (cap byte) e ritorna la lunghezza (>0) o -1. */
int gmb_model_io_inspect(const char* dir, char* out, int cap);

/* Discovery: come sopra ma STAMPA il report su stdout (per il comando `LLM MODEL INSPECT`).
 * 0 = ok, -1 = errore. */
int gmb_model_io_inspect_print(const char* dir);

#ifdef __cplusplus
}
#endif

#endif /* GMB_MODEL_IO_H */
