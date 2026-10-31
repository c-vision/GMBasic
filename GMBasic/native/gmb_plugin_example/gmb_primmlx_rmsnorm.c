/* gmb_primmlx_rmsnorm.c — PLUGIN PRIMITIVA MLX-aware d'esempio: RMSNorm nel forward del motore.
 * Lavora sui HANDLE MLX OPACHI (mlx_array*) — ZERO copie (nessuna regressione di velocità).
 * I simboli MLX si risolvono a runtime dall'host (l'engine li ha caricati). */
#include "llmengine_plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <mlx/c/mlx.h>

typedef struct { float eps; } St;
static void* p_create(const char* params_json)
{
    St* s = (St*)calloc(1, sizeof(St)); if (!s) return NULL; s->eps = 1e-5f;
    if (params_json) { const char* e = strstr(params_json, "eps"); if (e) { const char* c = strchr(e, ':'); if (c) s->eps = (float)atof(c + 1); } }
    return s;
}
static void p_destroy(void* h) { free(h); }
static int p_forward(void* h, LlmEnginePluginPrimMLXIO* io)
{
    (void)h;
    if (io == NULL || io->op != LLMENGINE_PLUGIN_PRIMMLX_OP_NORM) return -1;
    mlx_array* x   = (mlx_array*)io->x;
    mlx_array* w   = (mlx_array*)io->w;
    mlx_array* out = (mlx_array*)io->out;
    mlx_stream* st = (mlx_stream*)io->stream;
    if (x == NULL || w == NULL || out == NULL || st == NULL) return -1;
    return mlx_fast_rms_norm(out, *x, *w, io->eps, *st) == 0 ? 0 : -1;
}
static const LlmEnginePluginPrimMLX g_prim = { "rmsnorm", "norm", LLMENGINE_PLUGIN_ABI_VERSION, p_create, p_destroy, p_forward };
const LlmEnginePluginPrimMLX* gmb_prim_mlx_describe(void) { return &g_prim; }
/* Nome canonico atteso da LLMEnv (llmengine_plugin.c). */
const LlmEnginePluginPrimMLX* llmengine_plugin_prim_mlx_describe(void) { return &g_prim; }
