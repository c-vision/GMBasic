/* gmb_decision_head.c — testa di scoring condivisa JEV/SystemOne (T3). Vedi header. */
#include "gmb_decision_head.h"
#include "gmb_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define HEAD_JSON_CAP (256 * 1024)

static const char* const HEAD_SLOTS[GMB_DECISION_HEAD_SLOTS] = {
    "false", "true",
    "0", "1", "2", "3", "4", "5",
    "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P",
};

void gmb_decision_head_init(gmb_decision_head_t* head) {
    if (!head) return;
    memset(head, 0, sizeof(*head));
    head->head_T = 1.0f;
    head->T_kind[0] = head->T_kind[1] = head->T_kind[2] = 1.0f;
    for (int i = 0; i < GMB_DECISION_HEAD_SLOTS; i++) head->token_id[i] = -1;
    head->calibrated = 0;
}

static int head_load_path(const char* path, gmb_decision_head_t* head);

int gmb_decision_head_load(const char* model_dir, gmb_decision_head_t* head) {
    if (!head) return -1;
    if (!model_dir || !*model_dir) { gmb_decision_head_init(head); return 0; }
    char path[1300];
    snprintf(path, sizeof path, "%s/scoring_head.json", model_dir);
    return head_load_path(path, head);
}

/* Artefatto di calibrazione a PATH ESPLICITO (`decideCalibration` nella config del provider):
 * 1 = caricato, 0 = assente, -1 = presente ma illeggibile. */
int gmb_decision_head_load_file(const char* path, gmb_decision_head_t* head) {
    if (!head) return -1;
    if (!path || !*path) { gmb_decision_head_init(head); return 0; }
    return head_load_path(path, head);
}

int gmb_decision_head_slot(const gmb_decision_option_t* option) {
    if (!option) return -1;
    for (int i = 0; i < GMB_DECISION_HEAD_SLOTS; i++) {
        if (option->id && strcasecmp(option->id, HEAD_SLOTS[i]) == 0) return i;
    }
    for (int i = 0; i < GMB_DECISION_HEAD_SLOTS; i++) {
        if (option->label && strcasecmp(option->label, HEAD_SLOTS[i]) == 0) return i;
    }
    return -1;
}

/* Numero da un nodo JSON testuale estratto ("" / "null" / "false" = assente). */
static int head_number(const char* js, const char* path, float* out) {
    char* v = gmb_json_extract(js, path);
    if (!v) return 0;
    int ok = 0;
    if (v[0] && strcmp(v, "null") != 0 && strcmp(v, "false") != 0) {
        *out = (float)atof(v);
        ok = 1;
    }
    free(v);
    return ok;
}

/* Carica l'artefatto da un PATH ESPLICITO (usato anche da `decideCalibration`). */
static int head_load_path(const char* path, gmb_decision_head_t* head) {
    gmb_decision_head_init(head);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;

    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    char* js = (char*)malloc(HEAD_JSON_CAP);
    if (!js) { fclose(f); return -1; }
    size_t n = fread(js, 1, HEAD_JSON_CAP - 1, f);
    js[n] = 0;
    fclose(f);

    /* Deve essere un oggetto JSON: un file troncato/corrotto è un errore, mai identità silenziosa. */
    const char* p = js;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    int valid = (*p == '{' && n > 0);
    if (valid) {
        const char* e = js + n;
        while (e > js && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;
        valid = (e > js && e[-1] == '}');
    }
    if (!valid) { free(js); return -1; }

    float v;
    if (head_number(js, "head_T", &v) && v > 0.0f) head->head_T = v;

    /* T_kind: oggetto {noul,choice,score} oppure array [noul,choice,score]. */
    float t0;
    char probe[32];
    snprintf(probe, sizeof probe, "T_kind[0]");
    if (head_number(js, probe, &t0)) {
        for (int k = 0; k < 3; k++) {
            snprintf(probe, sizeof probe, "T_kind[%d]", k);
            if (head_number(js, probe, &v) && v > 0.0f) head->T_kind[k] = v;
        }
    } else {
        static const char* const kinds[3] = { "noul", "choice", "score" };
        for (int k = 0; k < 3; k++) {
            char tp[64];
            snprintf(tp, sizeof tp, "T_kind.%s", kinds[k]);
            if (head_number(js, tp, &v) && v > 0.0f) head->T_kind[k] = v;
        }
    }

    /* bias: array posizionale (ordine canonico) oppure oggetto per etichetta. */
    float b0;
    int bias_array = head_number(js, "bias[0]", &b0);
    for (int i = 0; i < GMB_DECISION_HEAD_SLOTS; i++) {
        char bp[64];
        if (bias_array) snprintf(bp, sizeof bp, "bias[%d]", i);
        else snprintf(bp, sizeof bp, "bias.%s", HEAD_SLOTS[i]);
        if (head_number(js, bp, &v)) head->bias[i] = v;
    }

    /* verbalizer_ids: token id esatti per slot. */
    for (int i = 0; i < GMB_DECISION_HEAD_SLOTS; i++) {
        char tp[64];
        snprintf(tp, sizeof tp, "verbalizer_ids.%s", HEAD_SLOTS[i]);
        char* id = gmb_json_extract(js, tp);
        if (id) {
            if (id[0] && strcmp(id, "null") != 0) {
                int v = atoi(id);
                if (v >= 0) head->token_id[i] = v;
            }
            free(id);
        }
    }

    char* cal = gmb_json_extract(js, "calibrated");
    head->calibrated = !(cal && strcasecmp(cal, "false") == 0);
    free(cal);
    free(js);
    return 1;
}

int gmb_decision_head_apply(const gmb_decision_head_t* head, gmb_decision_kind_t kind,
                            const float* logprob, const gmb_decision_option_t* options, int n,
                            float* out_prob, int* out_argmax, double* out_confidence) {
    if (!head || !logprob || !options || n < 1 || !out_prob || !out_argmax) return -1;
    int ki = (int)kind;
    if (ki < 0 || ki > 2) return -1;
    float tk = head->T_kind[ki];
    if (!(tk > 0.0f)) tk = 1.0f;
    float ht = head->head_T;
    if (!(ht > 0.0f)) ht = 1.0f;
    float denom = ht * tk;

    float* z = (float*)malloc((size_t)n * sizeof(float));
    double* ex = (double*)malloc((size_t)n * sizeof(double));
    if (!z || !ex) { free(z); free(ex); return -1; }

    float maxz = -INFINITY;
    for (int i = 0; i < n; i++) {
        int slot = gmb_decision_head_slot(&options[i]);
        float bias = slot >= 0 ? head->bias[slot] : 0.0f;
        z[i] = (logprob[i] + bias) / denom;
        if (z[i] > maxz) maxz = z[i];
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++) { ex[i] = exp((double)(z[i] - maxz)); sum += ex[i]; }
    if (!(sum > 0.0)) { free(z); free(ex); return -1; }

    int best = 0;
    for (int i = 0; i < n; i++) {
        out_prob[i] = (float)(ex[i] / sum);
        if (out_prob[i] > out_prob[best]) best = i;
    }
    *out_argmax = best;

    if (out_confidence) {
        double rest = 0.0;
        for (int i = 0; i < n; i++) if (i != best) rest += out_prob[i];
        double mean_rest = n > 1 ? rest / (n - 1) : 0.0;
        double conf = (double)out_prob[best] - mean_rest;
        if (conf < 0.0) conf = 0.0;
        if (conf > 1.0) conf = 1.0;
        *out_confidence = conf;
    }
    free(z);
    free(ex);
    return 0;
}
