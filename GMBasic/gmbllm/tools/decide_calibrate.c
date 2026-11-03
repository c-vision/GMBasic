/* decide_calibrate.c — tool PUL: fit della calibrazione JEV (T5) da un file di campioni.
 *
 * Uso: gmbllm_decide_calibrate <samples.txt> <scoring_head.json>
 *
 * Formato (una domanda per riga; '#' commento, righe vuote ignorate):
 *   <kind> <n_options> <label> <logprob_0> ... <logprob_n-1>
 * kind: 0=NOUL, 1=CHOICE, 2=SCORE. label = indice dell'opzione corretta (0..n-1).
 * Le logprob sono quelle GREZZE delle opzioni (bias escluso), come le legge il motore.
 *
 * Scrive `scoring_head.json` con head_T/T_kind + blocco `calibration` (metriche) e stampa
 * un riepilogo. Nessuna dipendenza MLX: è il confine PUL che produce l'artefatto.
 */
#include "llm_decide_calib.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLES 200000

static int parse_line(char* line, llm_decide_sample_t* out) {
    char* p = line;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (!*p || *p == '#') return 0;   /* vuota/commento */
    char* end = NULL;
    long kind = strtol(p, &end, 10);
    if (end == p) return -1;
    p = end;
    long n = strtol(p, &end, 10);
    if (end == p || n < 1 || n > LLM_DECIDE_MAX_OPTIONS) return -1;
    p = end;
    long label = strtol(p, &end, 10);
    if (end == p || label < 0 || label >= n) return -1;
    p = end;
    out->kind = (int)kind;
    out->n_options = (int)n;
    out->label = (int)label;
    for (int i = 0; i < out->n_options; i++) {
        double v = strtod(p, &end);
        if (end == p) return -1;
        out->logprob[i] = (float)v;
        p = end;
    }
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return *p ? -1 : 1;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "uso: %s <samples.txt> <scoring_head.json>\n", argv[0]);
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "ERRORE: input non leggibile: %s\n", argv[1]); return 2; }

    llm_decide_sample_t* samples = (llm_decide_sample_t*)calloc(MAX_SAMPLES, sizeof(*samples));
    if (!samples) { fclose(f); fprintf(stderr, "ERRORE: memoria\n"); return 2; }

    int n = 0, line_no = 0, bad = 0;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        line_no++;
        int rc = parse_line(line, &samples[n]);
        if (rc < 0) { fprintf(stderr, "ERRORE: riga %d non valida\n", line_no); bad = 1; break; }
        if (rc == 1) { n++; if (n >= MAX_SAMPLES) break; }
    }
    fclose(f);
    if (bad) { free(samples); return 1; }
    if (n == 0) { fprintf(stderr, "ERRORE: nessun campione\n"); free(samples); return 1; }

    llm_decide_calib_result_t r;
    if (llm_decide_calib_fit(samples, n, &r, argv[2]) != 0) {
        fprintf(stderr, "ERRORE: fit fallito (input invalido)\n");
        free(samples);
        return 1;
    }
    free(samples);

    printf("DECIDE-CALIBRATE n=%d head_T=%.6f T_kind=[noul %.6f choice %.6f score %.6f] identity=%d cv=%d\n",
           r.n_questions, r.head_T, r.T_kind[0], r.T_kind[1], r.T_kind[2], r.identity, r.cv_folds);
    printf("  accuracy=%.4f brier %.6f -> %.6f  ece %.6f -> %.6f (oof brier %.6f -> %.6f)\n",
           r.accuracy, r.brier_before, r.brier_after, r.ece_before, r.ece_after,
           r.brier_oof_before, r.brier_oof_after);
    printf("  scritto: %s\n", argv[2]);
    return 0;
}
