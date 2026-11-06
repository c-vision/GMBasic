/* wsp_probe.c — sonde WSP (W5) sul motore MLX via dlopen + scelta del blocco di layer da rimuovere per
 * SIMILARITÀ FRA RAPPRESENTAZIONI (ricetta di Gromov et al., "The Unreasonable Ineffectiveness of the Deeper Layers"):
 * si rimuove il blocco [l, l+n) che minimizza la distanza angolare media fra l'hidden in ingresso (uscita del layer
 * l-1) e quella in uscita (layer l+n-1) sulle posizioni della sequenza di calibrazione. Opzionalmente deriva il
 * modello (W7) senza quel blocco. La distanza è una proxy: la qualità va misurata (PPL/QA), non presunta.
 * Uso:  wsp_probe <libllm_ai_mlx.dylib> <dir_modello> <id,id,...> <n_blocco> [dir_derivato [back]]
 * Compilazione (C99):  cc -Iinclude tools/wsp_probe.c src/llmwsp_{store,doc,linalg,analyze,derive}.c -ldl -lm */
#define _POSIX_C_SOURCE 200809L
#include "llmwsp_derive.h"
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*load_fn)(const char *);
typedef int (*capl_fn)(void *, const int32_t *, int, const int *, int, float *, int);
typedef int (*dim_fn)(void *);

/* distanza angolare media (arccos(cos)/pi) fra le righe corrispondenti di a e b: [n_tok × H] */
static double angular_distance(const float *a, const float *b, size_t n_tok, size_t H) {
    double tot = 0;
    for (size_t t = 0; t < n_tok; t++) {
        double ab = 0, aa = 0, bb = 0;
        for (size_t i = 0; i < H; i++) { double x = a[t * H + i], y = b[t * H + i]; ab += x * y; aa += x * x; bb += y * y; }
        double c = aa > 0 && bb > 0 ? ab / sqrt(aa * bb) : 1;
        if (c > 1) c = 1; if (c < -1) c = -1;
        tot += acos(c) / M_PI;
    }
    return tot / (double)n_tok;
}

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "uso: %s <dylib> <dir_modello> <ids> <n_blocco> [dir_derivato [back]]\n", argv[0]); return 2; }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    load_fn load = (load_fn)dlsym(lib, "llm_ai_mlx_load_model"); capl_fn capl = (capl_fn)dlsym(lib, "llm_ai_mlx_capture_layers"); dim_fn edim = (dim_fn)dlsym(lib, "llm_ai_mlx_embed_dim");
    if (!load || !capl || !edim) { fprintf(stderr, "simboli mancanti\n"); return 1; }
    int32_t ids[1024]; int n = 0;
    { char *d = strdup(argv[3]); for (char *t = strtok(d, ","); t && n < 1024; t = strtok(NULL, ",")) ids[n++] = atoi(t); free(d); }
    int nb = atoi(argv[4]);
    char err[400], p[1100]; llmwsp_store *s; llmwsp_doc *d;
    if (llmwsp_store_open(argv[2], &s, err, sizeof err) < 0) { fprintf(stderr, "store: %s\n", err); return 1; }
    snprintf(p, sizeof p, "%s/config.json", argv[2]);
    if (llmwsp_doc_build(s, p, &d, err, sizeof err) < 0) { fprintf(stderr, "doc: %s\n", err); return 1; }
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    int L = g.layers;
    if (L > 64 || nb < 1 || nb >= L - 1) { fprintf(stderr, "n_blocco o numero di layer non supportati\n"); return 2; }
    void *m = load(argv[2]);
    if (!m) { fprintf(stderr, "load fallita\n"); return 1; }
    int H = edim(m);
    int *lays = (int *)malloc((size_t)L * sizeof(int)); for (int l = 0; l < L; l++) lays[l] = l;
    size_t per = (size_t)n * (size_t)H;
    float *hid = (float *)malloc((size_t)L * per * sizeof(float));
    if (capl(m, ids, n, lays, L, hid, (int)((size_t)L * per)) != n) { fprintf(stderr, "capture_layers fallita\n"); return 1; }
    printf("calibrazione: %d posizioni, %d layer, hidden %d\n", n, L, H);
    /* distanza fra uscita del layer l-1 e uscita del layer l+nb-1; l >= 1; si preserva l'ultimo layer */
    printf("blocco di %d layer: distanza angolare per punto di partenza l (rimuove [l, l+%d))\n", nb, nb);
    int best = -1; double bd = 1e9;
    for (int l = 1; l + nb <= L - 1; l++) {
        double dist = angular_distance(hid + (size_t)(l - 1) * per, hid + (size_t)(l + nb - 1) * per, (size_t)n, (size_t)H);
        printf("  l=%2d  d=%.4f%s\n", l, dist, "");
        if (dist < bd) { bd = dist; best = l; }
    }
    printf("MIGLIORE: rimuovere i layer [%d, %d) (d=%.4f)\n", best, best + nb, bd);
    double dback = angular_distance(hid + (size_t)(L - nb - 2) * per, hid + (size_t)(L - 3) * per, (size_t)n, (size_t)H);
    printf("(riferimento 'a ritroso dal penultimo': blocco [%d, %d) d=%.4f)\n", L - 1 - nb, L - 1, angular_distance(hid + (size_t)(L - 2 - nb) * per, hid + (size_t)(L - 2) * per, (size_t)n, (size_t)H));
    (void)dback;
    if (argc > 6 && !strcmp(argv[6], "back")) { best = L - 1 - nb; printf("DERIVAZIONE: variante a ritroso, rimuove [%d, %d)\n", best, best + nb); }
    if (argc > 5) {
        int *map = (int *)malloc((size_t)L * sizeof(int)); int c = 0;
        for (int l = 0; l < L; l++) if (l < best || l >= best + nb) map[c++] = l;
        llmwsp_derive_ops ops; memset(&ops, 0, sizeof ops); ops.layer_map = map; ops.nl = (size_t)c;
        llmwsp_derive_report rep;
        if (llmwsp_derive(s, d, argv[2], &ops, argv[5], &rep, err, sizeof err) < 0) { fprintf(stderr, "derive: %s\n", err); return 1; }
        printf("derivato in %s: %zu layer, %llu -> %llu byte\n", argv[5], rep.out_layers, (unsigned long long)rep.bytes_before, (unsigned long long)rep.bytes_after);
        free(map);
    }
    free(lays); free(hid); llmwsp_doc_free(d); llmwsp_store_close(s);
    return 0;
}
