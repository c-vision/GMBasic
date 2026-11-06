/* llmwsp_verify.h — W8 del piano Weight-Space-Programming: verifica, gate di qualità e catena di provenienza.
 *
 *  - VERIFY: riapre un checkpoint, controlla (opz.) il WSP Document (coerenza col config), la copertura, la FINITEZZA dei
 *    valori (NaN/Inf nei tensori F32/BF16/F16) e, se è dato uno store di base, forma/dtype uguali e quali tensori sono
 *    cambiati (confronto sha256), segnalando quelli cambiati che NON erano ammessi.
 *  - GATE: soglie dichiarate dal chiamante su valori misurati altrove (KL, PPL, accordo...). Il verdetto è una funzione
 *    pura dei numeri: niente soglie implicite.
 *  - PROVENIENZA: hash di modello (sha256 sui sha256 per tensore, in ordine di nome) e catena base → ops → output scritta in
 *    JSON, con il testo della ricetta e i gate. Nessun dato esterno. */
#ifndef LLMWSP_VERIFY_H
#define LLMWSP_VERIFY_H
#include "llmwsp_doc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llmwsp_verify_opts {
    int          check_doc;          /* costruisce il WSP Document da <dir>/config.json e lo controlla (solo famiglie supportate) */
    int          check_finite;       /* scansione NaN/Inf */
    const llmwsp_store *base;        /* opzionale: confronto con la base */
    const char **allowed_changed;    /* nomi dei tensori che PUÒ essere cambiati rispetto alla base (se base != NULL) */
    size_t       n_allowed;
} llmwsp_verify_opts;

typedef struct llmwsp_verify_report {
    size_t  tensors;
    int     doc_violations;
    int     coverage_violations;
    size_t  nonfinite_tensors;
    int64_t nonfinite_values;
    size_t  shape_mismatch;          /* forma/dtype/tensori mancanti rispetto alla base */
    size_t  changed;                 /* tensori diversi dalla base */
    size_t  unexpected_changed;      /* cambiati e non ammessi */
    char    first_error[300];
    int     ok;                      /* 1 se nessuna violazione */
} llmwsp_verify_report;

int llmwsp_verify_dir(const char *dir, const llmwsp_verify_opts *o, llmwsp_verify_report *r);

typedef struct llmwsp_gate {
    const char *name;
    double      value;               /* misurato */
    double      threshold;           /* dichiarato */
    int         le;                  /* 1: passa se value <= threshold; 0: passa se value >= threshold */
} llmwsp_gate;

/* Valuta i gate; json_out (malloc, opzionale) riceve il rapporto. Ritorna il numero di gate FALLITI (valori non finiti = fallito). */
int llmwsp_gates_eval(const llmwsp_gate *g, size_t n, char **json_out);

/* hash del modello: sha256 della sequenza "<nome>\0<sha256 tensore>\n" in ordine di nome */
int llmwsp_model_hash(const llmwsp_store *s, char hex[65]);

/* Scrive `path` (JSON): catena base -> ops -> output. recipe_text e gates_json possono essere NULL. */
int llmwsp_provenance_write(const char *path, const llmwsp_store *base, const char *recipe_text, const llmwsp_store *out,
                            const char *gates_json, char *err, size_t ec);

#ifdef __cplusplus
}
#endif
#endif
