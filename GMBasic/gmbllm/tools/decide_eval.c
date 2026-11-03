/* decide_eval.c — tool PUL: valutazione DETERMINISTICA di un dataset di decisioni (T7).
 * Uso: gmbllm_decide_eval <samples.txt> [head_T T_noul T_choice T_score]
 * Formato righe: <kind> <n_options> <label> <logprob_0> ... <logprob_n-1>
 * Stampa accuracy/Brier/NLL/ECE complessive e per kind. Nessuna dipendenza MLX. */
#include "llm_decide_calib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLES 200000

static int parse_line(char* line, llm_decide_sample_t* out) {
    char* p = line;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (!*p || *p == '#') return 0;
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
    out->kind = (int)kind; out->n_options = (int)n; out->label = (int)label;
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
    if (argc < 2) { fprintf(stderr, "uso: %s <samples.txt> [head_T T_noul T_choice T_score]\n", argv[0]); return 2; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "ERRORE: input non leggibile: %s\n", argv[1]); return 2; }
    llm_decide_sample_t* samples = (llm_decide_sample_t*)calloc(MAX_SAMPLES, sizeof(*samples));
    if (!samples) { fclose(f); return 2; }
    int n = 0, line_no = 0, bad = 0;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        line_no++;
        int rc = parse_line(line, &samples[n]);
        if (rc < 0) { fprintf(stderr, "ERRORE: riga %d non valida\n", line_no); bad = 1; break; }
        if (rc == 1) { n++; if (n >= MAX_SAMPLES) break; }
    }
    fclose(f);
    if (bad || n == 0) { free(samples); return 1; }

    float head_T = 1.0f, T_kind[3] = { 1.0f, 1.0f, 1.0f };
    if (argc >= 3) head_T = (float)atof(argv[2]);
    if (argc >= 6) { T_kind[0] = (float)atof(argv[3]); T_kind[1] = (float)atof(argv[4]); T_kind[2] = (float)atof(argv[5]); }

    llm_decide_metrics_t all, per[3];
    if (llm_decide_eval(samples, n, head_T, T_kind, &all, per) != 0) {
        fprintf(stderr, "ERRORE: dataset invalido\n"); free(samples); return 1;
    }
    free(samples);
    printf("DECIDE-EVAL n=%d accuracy=%.6f brier=%.6f nll=%.6f ece=%.6f\n",
           all.n, all.accuracy, all.brier, all.nll, all.ece);
    static const char* const names[3] = { "noul", "choice", "score" };
    for (int k = 0; k < 3; k++)
        printf("  %-6s n=%d accuracy=%.6f brier=%.6f nll=%.6f ece=%.6f\n",
               names[k], per[k].n, per[k].accuracy, per[k].brier, per[k].nll, per[k].ece);
    return 0;
}
