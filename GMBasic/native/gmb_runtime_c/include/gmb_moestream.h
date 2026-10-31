/* gmb_moestream.h — provider AI embedded "moestream" per i backend C (M14 parità 4 backend).
 *
 * I backend C (C99, C99Native, VM/C) NON hanno il provider C#: queste funzioni dlopen-ano
 * `libgmb_ai_moestream` (stessa dylib usata dal C#) e ne chiamano l'API C. È l'unica via per
 * avere gli STESSI comandi/funzioni su tutti i 4 backend.
 *
 * Convenzione: i comandi sono "statement" (ritornano 0 = ok, !=0 = errore; messaggio su stderr);
 * `AI.STREAMSTATS` produce il JSON in un buffer (stringa BASIC via gmb_string_alloc nel codegen). */
#ifndef GMB_MOESTREAM_H
#define GMB_MOESTREAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AI MODEL LOAD nome, path [, RESIDENT] — resident!=0 = tutto in RAM (no swap). */
int gmb_moestream_stmt_load(const char* name, const char* path, int resident);
/* AI MODEL UNLOAD nome */
int gmb_moestream_stmt_unload(const char* name);
/* AI MODEL CACHE nome, MB */
int gmb_moestream_stmt_cache(const char* name, int cache_mb);
/* AI MODEL PREFETCH nome, topn (0 = OFF) */
int gmb_moestream_stmt_prefetch(const char* name, int topn);
/* AI MODEL DEVICE nome, AUTO|CPU|METAL */
int gmb_moestream_stmt_device(const char* name, const char* mode);
/* AI MODEL CONVERT src, dst — converte un checkpoint HF/MLX in `.gmoe` (processo esterno). */
int gmb_moestream_stmt_convert(const char* src, const char* dst);
int gmb_moestream_quantize(const char* src, const char* dst, int bits, int group);
int gmb_moestream_stmt_quantize(const char* src, const char* dst, int bits, int group);
int gmb_moestream_stmt_inspect(const char* dir);
int gmb_moestream_convert(const char* src, const char* dst);
/* AI MODEL PROMOTE nome — ACTIVE (tutti gli esperti in RAM) | DEMOTE nome — WARM (on-demand). */
int gmb_moestream_stmt_promote(const char* name);
int gmb_moestream_stmt_demote(const char* name);
/* AI SET PRIORITY nome, N — priorità di eviction (orchestrazione M21). */
int gmb_moestream_stmt_priority(const char* name, int pri);
/* LLM MODEL TRACE nome, path | OFF — registra il trace esperti `.moetrace` (K1, replay offline). */
int gmb_moestream_stmt_trace(const char* name, const char* path, int on);
/* AI.STREAMSTATS(nome) -> JSON in `out` (cap byte). Ritorna 0 su successo, !=0 su errore. */
int gmb_moestream_streamstats_json(const char* name, char* out, size_t cap);
/* AI REGISTER alias, completion, path.gmoe, "" */
int gmb_moestream_register(const char* alias, const char* path);

/* Stato libreria (diagnostica): 1 se la dylib è caricabile, 0 altrimenti. */
int gmb_moestream_available(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_MOESTREAM_H */
