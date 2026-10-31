/* gmb_brain_http.h — Adapter Brain reale (OpenAI-compatible) per il loop.
 * Collega gmb_agent_loop a un server LLM locale via HTTP POST
 * (POST /v1/chat/completions): mlx_lm.server, llama.cpp server, vLLM, ...
 * Config: url/model via argomenti o env GMB_BRAIN_URL / GMB_BRAIN_MODEL /
 * GMB_BRAIN_TIMEOUT_MS. Firma gmb_brain_complete ≡ gmb_brain_fn. */
#ifndef GMB_BRAIN_HTTP_H
#define GMB_BRAIN_HTTP_H
#include "gmb_agent_loop.h"

typedef struct gmb_brain_http_s gmb_brain_http_t;

/* url/model possono essere NULL → env (GMB_BRAIN_URL default
   http://127.0.0.1:8080/v1/chat/completions, GMB_BRAIN_MODEL default ""). */
gmb_brain_http_t* gmb_brain_http_new(const char* url, const char* model);
void gmb_brain_http_free(gmb_brain_http_t* b);

/* gmb_brain_fn: chiama il server, ritorna choices[0].message.content in out. */
int gmb_brain_http_complete(void* ctx, const char* transcript, char* out, size_t cap);

#endif
