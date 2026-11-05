/* llmwsp_overlay.h — W6 del piano Weight-Space Programming: overlay copy-on-write, edit, journal, undo/redo,
 * diff, ricetta riproducibile (RECIPE/REPLAY) e patch sparsa a slice.
 *
 * La base (Weight Store) resta IMMUTABILE: ogni edit agisce su una copia del tensore in memoria, nello stesso
 * dtype dell'originale (F32/BF16/F16). Invarianti verificate prima di ogni commit: forma e dtype preservati, valori
 * finiti; altrimenti l'edit è rifiutata e NULLA cambia. Ogni edit produce un report (frazione di pesi toccati,
 * variazione relativa di norma, massimo scarto). Gli indirizzi sono quelli del WSP Document
 * ("layers.3.attn.o.heads.5") oppure "@<asse>:<da>:<a>:<nome tensore>" per una slice arbitraria.
 *
 * POLITICA: le primitive (SET/SCALE/PROJECTOUT/...) sono generali. Il piano non include ricette né dataset per
 * rimuovere protezioni di sicurezza; ogni uso resta nel journal. RANK1/NULLSPACE e il gate KL richiedono
 * attivazioni/forward (W5) e NON sono in questo modulo. */
#ifndef LLMWSP_OVERLAY_H
#define LLMWSP_OVERLAY_H
#include "llmwsp_doc.h"
#include "llmwsp_mat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llmwsp_overlay llmwsp_overlay;

typedef struct llmwsp_ov_report {
    int64_t touched;       /* elementi la cui rappresentazione è cambiata */
    int64_t total;         /* elementi della slice */
    double  rel_norm;      /* ||dopo - prima|| / ||prima|| (0 se prima è nullo) */
    double  max_abs;       /* massimo scarto assoluto */
} llmwsp_ov_report;

llmwsp_overlay *llmwsp_ov_new(const llmwsp_store *s, const llmwsp_doc *d, const char *srcdir);
void            llmwsp_ov_free(llmwsp_overlay *o);

/* conversioni usate dall'overlay (esposte per i test): dtype F32/BF16/F16, arrotondamento al più vicino pari */
int llmwsp_decode_f64(const char *dtype, const void *raw, size_t n, double *out);
int llmwsp_encode_f64(const char *dtype, const double *in, size_t n, void *raw);

/* ---- edit (tutte: 0 = ok, <0 = rifiutata e nulla modificato; err descrive il motivo) ---- */
int llmwsp_ov_scale(llmwsp_overlay *o, const char *addr, double alpha, llmwsp_ov_report *r, char *err, size_t ec);
int llmwsp_ov_zero(llmwsp_overlay *o, const char *addr, llmwsp_ov_report *r, char *err, size_t ec);
int llmwsp_ov_set(llmwsp_overlay *o, const char *addr, const double *v, size_t n, llmwsp_ov_report *r, char *err, size_t ec);
int llmwsp_ov_adddelta(llmwsp_overlay *o, const char *addr, const double *v, size_t n, llmwsp_ov_report *r, char *err, size_t ec);
int llmwsp_ov_copy(llmwsp_overlay *o, const char *dst, const char *src, llmwsp_ov_report *r, char *err, size_t ec);

/* PROJECTOUT: toglie dalla slice le componenti lungo `ndirs` direzioni (ognuna di `len` elementi, row-major).
 * vec_axis: l'asse della slice che contiene i VETTORI da proiettare (0: le colonne sono vettori di lunghezza =
 * righe, es. o_proj/down_proj con asse residuo sulle righe; 1: le righe sono vettori, es. embed, q/k/v/gate/up). */
enum { LLMWSP_PROJ_NORMPRESERVING = 1,   /* ogni vettore ritorna alla norma originale */
       LLMWSP_PROJ_HARMLESS = 2 };       /* ndirs==2: dirs[0]=d, dirs[1]=h innocua; si proietta d ortogonalizzata a h */
int llmwsp_ov_projectout(llmwsp_overlay *o, const char *addr, const double *dirs, size_t ndirs, size_t len,
                         int vec_axis, int flags, llmwsp_ov_report *r, char *err, size_t ec);

/* ---- journal ---- */
size_t llmwsp_ov_journal_len(const llmwsp_overlay *o);   /* edit totali registrate */
size_t llmwsp_ov_journal_pos(const llmwsp_overlay *o);   /* edit attualmente applicate */
int    llmwsp_ov_undo(llmwsp_overlay *o);                /* 0 = ok, -1 = niente da annullare */
int    llmwsp_ov_redo(llmwsp_overlay *o);

/* sha256 del contenuto CORRENTE di un tensore (overlay se modificato, altrimenti base). */
int llmwsp_ov_tensor_sha256(const llmwsp_overlay *o, const char *tensor, char hex[65]);

typedef struct llmwsp_ov_diffent {
    const char *tensor;
    int64_t     changed;          /* elementi diversi dalla base */
    int64_t     total;
    int64_t     r0, r1, c0, c1;   /* bounding box delle differenze */
    double      rel_norm;
} llmwsp_ov_diffent;
/* Diff a livello slice rispetto alla base. Riempie fino a cap voci; *n = numero di tensori modificati. */
int llmwsp_ov_diff(const llmwsp_overlay *o, llmwsp_ov_diffent *out, size_t cap, size_t *n);

/* ---- scrittura ---- */
int llmwsp_ov_materialize(const llmwsp_overlay *o, const char *outdir, llmwsp_mat_stats *st, char *err, size_t ec);

/* RECIPE: scrive in `dir` (da creare) `recipe.gmbderive` + `blob_N.f64`; REPLAY la riesegue su un overlay nuovo. */
int llmwsp_ov_recipe(const llmwsp_overlay *o, const char *dir, char *err, size_t ec);
int llmwsp_ov_replay(llmwsp_overlay *o, const char *dir, char *err, size_t ec);

/* Patch sparsa a slice (solo le slice davvero cambiate, la più stretta fra righe/colonne per tensore). */
int llmwsp_ov_export_patch(const llmwsp_overlay *o, const char *path, size_t *bytes, char *err, size_t ec);
int llmwsp_ov_apply_patch(llmwsp_overlay *o, const char *path, char *err, size_t ec);

#ifdef __cplusplus
}
#endif
#endif
