/* wsp_cap.c — driver (C99, dlopen) per la cattura del residual per layer del motore MLX (WSP W5).
 * Verifica: (1) layer finale == capture_hidden esistente; (2) neutralità: decode greedy identico prima/dopo la
 * cattura e con sessione del chiamante intatta; (3) valori finiti e norme per layer. Opz.: scrive i float32
 * [nlayers][n][hidden] su un file.
 * Uso: wsp_cap <libllm_ai_mlx.dylib> <dir_modello> <id,id,...> <layer,layer,...|all> [out.f32] */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*load_fn)(const char *);
typedef void (*free_fn)(void *);
typedef void *(*cs_fn)(void *);
typedef void (*ds_fn)(void *);
typedef int (*app_fn)(void *, const int32_t *, int);
typedef int (*sgen_fn)(void *, int, int32_t, int32_t *);
typedef int (*gen_fn)(void *, const int32_t *, int, int32_t *, int, int32_t);
typedef int (*caph_fn)(void *, const int32_t *, int, float *, int);
typedef int (*capl_fn)(void *, const int32_t *, int, const int *, int, float *, int);
typedef int (*dim_fn)(void *);

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "uso: %s <dylib> <dir> <ids> <layers|all> [out.f32]\n", argv[0]); return 2; }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    load_fn load = (load_fn)dlsym(lib, "llm_ai_mlx_load_model"); free_fn fr = (free_fn)dlsym(lib, "llm_ai_mlx_free_model");
    cs_fn cs = (cs_fn)dlsym(lib, "llm_ai_mlx_create_session"); ds_fn ds = (ds_fn)dlsym(lib, "llm_ai_mlx_destroy_session");
    app_fn app = (app_fn)dlsym(lib, "llm_ai_mlx_session_append"); sgen_fn sgen = (sgen_fn)dlsym(lib, "llm_ai_mlx_session_generate");
    caph_fn caph = (caph_fn)dlsym(lib, "llm_ai_mlx_session_capture_hidden"); capl_fn capl = (capl_fn)dlsym(lib, "llm_ai_mlx_capture_layers");
    dim_fn edim = (dim_fn)dlsym(lib, "llm_ai_mlx_embed_dim");
    if (!load || !fr || !cs || !ds || !app || !sgen || !caph || !capl || !edim) { fprintf(stderr, "simboli mancanti (dylib senza llm_ai_mlx_capture_layers?)\n"); return 1; }
    void *m = load(argv[2]);
    if (!m) { fprintf(stderr, "load fallita\n"); return 1; }
    int H = edim(m);
    int32_t ids[1024]; int n = 0;
    { char *dup = strdup(argv[3]); for (char *t = strtok(dup, ","); t && n < 1024; t = strtok(NULL, ",")) ids[n++] = atoi(t); free(dup); }
    int layers[64], nl = 0;
    if (!strcmp(argv[4], "all")) { fprintf(stderr, "'all' richiede di conoscere n_layers: passa un elenco\n"); return 2; }
    { char *dup = strdup(argv[4]); for (char *t = strtok(dup, ","); t && nl < 64; t = strtok(NULL, ",")) layers[nl++] = atoi(t); free(dup); }
    size_t per = (size_t)n * (size_t)H;
    float *out = (float *)malloc((size_t)nl * per * sizeof(float));

    /* riferimento di neutralità: decode greedy con sessione del chiamante, SENZA cattura */
    int32_t g0[16], g1[16];
    void *s0 = cs(m); app(s0, ids, n); int ng0 = sgen(s0, 16, -1, g0); ds(s0);
    /* con cattura fra append e generate (la KV del chiamante non deve cambiare) */
    void *s1 = cs(m); app(s1, ids, n);
    int rc = capl(m, ids, n, layers, nl, out, (int)((size_t)nl * per));
    CHECK(rc == n, "capture_layers ha restituito %d (atteso %d)", rc, n);
    int ng1 = sgen(s1, 16, -1, g1); ds(s1);
    CHECK(ng0 == 16 && ng1 == 16 && memcmp(g0, g1, sizeof g0) == 0, "decode diverso con cattura intercalata");
    /* errori attesi */
    CHECK(capl(m, ids, n, layers, nl, out, 1) < 0, "cap troppo piccolo deve fallire");
    int bad = 100000; CHECK(capl(m, ids, n, &bad, 1, out, (int)per) < 0, "layer fuori intervallo deve fallire");
    CHECK(capl(m, ids, 0, layers, nl, out, (int)per) < 0, "n=0 deve fallire");

    int finite = 1;
    for (size_t i = 0; i < (size_t)nl * per; i++) if (!isfinite(out[i])) { finite = 0; break; }
    CHECK(finite, "valori non finiti");
    printf("hidden=%d n=%d nlayers=%d\n", H, n, nl);
    for (int l = 0; l < nl; l++) {
        double s2 = 0; for (size_t i = 0; i < per; i++) s2 += (double)out[l * per + i] * out[l * per + i];
        printf("  layer %2d: norma RMS per elemento %.4f\n", layers[l], sqrt(s2 / (double)per));
    }
    /* coerenza: layer finale == capture_hidden (ultimo layer, tutte le posizioni). Si prova con n_layers-1 se richiesto */
    float *ref = (float *)malloc(per * sizeof(float));
    void *s2 = cs(m); int rh = caph(s2, ids, n, ref, (int)per); ds(s2);
    CHECK(rh == n, "capture_hidden rc=%d", rh);
    int maxl = layers[0]; int mi = 0; for (int l = 1; l < nl; l++) if (layers[l] > maxl) { maxl = layers[l]; mi = l; }
    double md = 0, mr = 0;
    for (size_t i = 0; i < per; i++) { double d = fabs((double)out[mi * per + i] - ref[i]); if (d > md) md = d; if (fabs(ref[i]) > mr) mr = fabs(ref[i]); }
    printf("layer più alto richiesto (%d) vs capture_hidden: max|diff|=%.3g (max|ref|=%.3g)\n", maxl, md, mr);
    if (argc > 5) { FILE *f = fopen(argv[5], "wb"); if (f) { fwrite(out, sizeof(float), (size_t)nl * per, f); fclose(f); } }
    free(out); free(ref); fr(m);
    printf(fails ? "WSP CAP: %d FALLITI\n" : "WSP CAP: OK\n", fails);
    return fails != 0;
}
