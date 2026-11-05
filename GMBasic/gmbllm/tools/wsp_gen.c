/* wsp_gen.c — driver minimale (C99, dlopen) sul motore MLX di LLMEnv per i gate di W3/W7 del piano WSP:
 * carica una directory modello e genera greedy N token da id di prompt, stampando gli id (uno per riga di output
 * "ids: ..."). Serve a verificare che un checkpoint materializzato/derivato SI CARICHI e GENERI, e a confrontare
 * gli id di due modelli (parità greedy). Non modifica il motore: usa solo l'ABI pubblica esistente
 *   llm_ai_mlx_load_model / llm_ai_mlx_generate / llm_ai_mlx_free_model.
 * Uso:  wsp_gen <libllm_ai_mlx.dylib> <dir_modello> <N> <id,id,id,...> [eos]
 * (mlx.metallib deve stare accanto alla dylib.) */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void *(*load_fn)(const char *);
typedef int (*gen_fn)(void *, const int32_t *, int, int32_t *, int, int32_t);
typedef void (*free_fn)(void *);

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "uso: %s <dylib> <dir_modello> <N> <id,id,...> [eos]\n", argv[0]); return 2; }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    load_fn load = (load_fn)dlsym(lib, "llm_ai_mlx_load_model");
    gen_fn gen = (gen_fn)dlsym(lib, "llm_ai_mlx_generate");
    free_fn fr = (free_fn)dlsym(lib, "llm_ai_mlx_free_model");
    if (!load || !gen || !fr) { fprintf(stderr, "simboli mancanti nella dylib\n"); return 1; }
    int n = atoi(argv[3]);
    int32_t prompt[4096]; int np = 0;
    for (char *t = strtok(argv[4], ","); t && np < 4096; t = strtok(NULL, ",")) prompt[np++] = (int32_t)atoi(t);
    int32_t eos = argc > 5 ? (int32_t)atoi(argv[5]) : -1;
    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
    void *m = load(argv[2]);
    if (!m) { fprintf(stderr, "load_model fallita: %s\n", argv[2]); return 1; }
    clock_gettime(CLOCK_MONOTONIC, &b);
    double tl = (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
    int32_t *out = (int32_t *)calloc((size_t)n + 1, sizeof(int32_t));
    clock_gettime(CLOCK_MONOTONIC, &a);
    int got = gen(m, prompt, np, out, n, eos);
    clock_gettime(CLOCK_MONOTONIC, &b);
    double tg = (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
    if (got < 0) { fprintf(stderr, "generate fallita\n"); return 1; }
    printf("ids:");
    for (int i = 0; i < got; i++) printf(" %d", out[i]);
    printf("\n");
    fprintf(stderr, "caricamento %.1fs, generazione %d token in %.1fs\n", tl, got, tg);
    free(out); fr(m);
    return 0;
}
