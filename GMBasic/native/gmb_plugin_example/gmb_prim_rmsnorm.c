/* gmb_prim_rmsnorm.c — PLUGIN PRIMITIVA d'esempio: RMSNorm (elementwise, reference IO op 1).
 * Dimostra l'ABI delle primitive (llmengine_plugin.h): un'operazione d'architettura = una dylib, che
 * l'engine carica a runtime e guida per-fase (create/forward/destroy). Nessuna ricompilazione. */
#include "llmengine_plugin.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct { float eps; } RmsState;

static void* p_create(const char* params_json)
{
    RmsState* st = (RmsState*)calloc(1, sizeof(RmsState));
    if (st == NULL) return NULL;
    st->eps = 1e-5f;
    if (params_json != NULL) { const char* e = strstr(params_json, "eps"); if (e) { const char* c = strchr(e, ':'); if (c) st->eps = (float)atof(c + 1); } }
    return st;
}
static void p_destroy(void* h) { free(h); }

/* forward: out[i] = in[i] / rms(in) * w[i] (w = aux; se assente, w = 1). */
static int p_forward(void* h, void* io)
{
    RmsState* st = (RmsState*)h;
    LlmEnginePluginPrimIO* p = (LlmEnginePluginPrimIO*)io;
    if (st == NULL || p == NULL || p->op != LLMENGINE_PLUGIN_PRIM_OP_ELEMENTWISE) return -1;
    if (p->in == NULL || p->out == NULL || p->n == 0) return -1;
    if (p->aux != NULL && p->aux_n != 0 && p->aux_n < p->n) return -1;
    double ss = 0; for (size_t i = 0; i < p->n; i++) ss += (double)p->in[i] * p->in[i];
    float r = 1.0f / sqrtf((float)(ss / (double)p->n) + st->eps);
    for (size_t i = 0; i < p->n; i++) p->out[i] = p->in[i] * r * (p->aux ? p->aux[i] : 1.0f);
    return 0;
}

static const LlmEnginePluginPrim g_prim = { "rmsnorm", "norm", LLMENGINE_PLUGIN_ABI_VERSION, p_create, p_destroy, p_forward };
const LlmEnginePluginPrim* gmb_prim_describe(void) { return &g_prim; }
/* Nome canonico atteso da LLMEnv (llmengine_plugin.c). */
const LlmEnginePluginPrim* llmengine_plugin_prim_describe(void) { return &g_prim; }
