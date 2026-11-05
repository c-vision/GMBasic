/* llmwsp_mat.h — W3 del piano Weight-Space Programming: Materializer.
 *
 * Scrive un nuovo checkpoint safetensors a partire da uno store e da un insieme di sostituzioni di tensore
 * (overlay minimale: tensore intero, stessa forma/dtype). Gli shard NON toccati vengono clonati
 * (clonefile APFS, altrimenti copia a blocchi); solo lo shard che contiene un tensore modificato viene
 * riscritto, in streaming. Copia anche i file accessori (config, tokenizer, index, ...) e scrive
 * `provenance.json`. Senza modifiche ogni tensore del risultato ha lo stesso sha256 dell'originale. */
#ifndef LLMWSP_MAT_H
#define LLMWSP_MAT_H
#include "llmwsp_store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llmwsp_edit {
    const char *name;       /* tensore da sostituire */
    const void *data;       /* contenuto completo (nbytes esatti) */
    uint64_t    nbytes;
} llmwsp_edit;

typedef struct llmwsp_mat_stats {
    int shards_cloned;      /* shard identici all'originale (clone/copia) */
    int shards_rewritten;   /* shard riscritti perché contengono una modifica */
    int files_copied;       /* file accessori copiati */
    int edited_tensors;
} llmwsp_mat_stats;

/* `srcdir` è la directory da cui `s` è stato aperto. `outdir` non deve esistere o deve essere vuota. */
int llmwsp_materialize(const llmwsp_store *s, const char *srcdir, const llmwsp_edit *edits, size_t nedits,
                       const char *outdir, llmwsp_mat_stats *st, char *err, size_t errcap);

/* Riapre `outdir` e verifica: stessi nomi/dtype/forme; sha256 uguale all'originale per ogni tensore non
 * modificato e uguale al contenuto dell'edit per gli altri; copertura completa. 0 = ok. */
int llmwsp_materialize_verify(const llmwsp_store *s, const llmwsp_edit *edits, size_t nedits,
                              const char *outdir, char *err, size_t errcap);

#ifdef __cplusplus
}
#endif
#endif
