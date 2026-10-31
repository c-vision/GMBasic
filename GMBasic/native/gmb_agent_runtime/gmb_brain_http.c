/* gmb_brain_http.c — Adapter Brain reale OpenAI-compatible (vedi header).
 * Riusa il client HTTP del runtime condiviso (gmb_net_http_request) e l'escape/parse
 * JSON del modulo CDP (gmb_cdp_json_escape / gmb_cdp_json_get_str): nessuna duplicazione. */
#include "gmb_brain_http.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include "gmb_network.h"
#include "gmb_browser_cdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gmb_brain_http_s {
    char url[512];
    char model[128];
    long timeout_ms;
};

gmb_brain_http_t* gmb_brain_http_new(const char* url, const char* model) {
    const char* env_url = gmb_opt_env("GMB_BRAIN_URL");
    const char* env_model = gmb_opt_env("GMB_BRAIN_MODEL");
    const char* env_to = gmb_opt_env("GMB_BRAIN_TIMEOUT_MS");
    gmb_brain_http_t* b = (gmb_brain_http_t*)calloc(1, sizeof(*b));
    if (!b) return NULL;
    snprintf(b->url, sizeof(b->url), "%s",
        url ? url : (env_url ? env_url : "http://127.0.0.1:8080/v1/chat/completions"));
    snprintf(b->model, sizeof(b->model), "%s", model ? model : (env_model ? env_model : ""));
    b->timeout_ms = env_to && *env_to ? atol(env_to) : 120000;
    if (b->timeout_ms <= 0) b->timeout_ms = 120000;
    return b;
}

void gmb_brain_http_free(gmb_brain_http_t* b) { free(b); }

int gmb_brain_http_complete(void* ctx, const char* transcript, char* out, size_t cap) {
    if (!ctx || !transcript || !out || cap == 0) return -1;
    gmb_brain_http_t* b = (gmb_brain_http_t*)ctx;
    out[0] = 0;

    /* body JSON: messaggi system+user, content = transcript JSON-escaped */
    size_t tlen = strlen(transcript);
    char* esc = (char*)malloc(tlen * 2 + 32);
    if (!esc) return -1;
    gmb_cdp_json_escape(transcript, esc, tlen * 2 + 32);

    size_t model_extra = b->model[0] ? strlen(b->model) + 32 : 24;
    char* body = (char*)malloc(strlen(esc) + model_extra + 256);
    if (!body) { free(esc); return -1; }
    if (b->model[0])
        snprintf(body, strlen(esc) + model_extra + 256,
            "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"Sei un agente che compie azioni. "
            "Rispondi con una azione JSON o con la risposta finale.\"},{\"role\":\"user\",\"content\":\"%s\"}],"
            "\"temperature\":0.2,\"max_tokens\":600}", b->model, esc);
    else
        snprintf(body, strlen(esc) + model_extra + 256,
            "{\"messages\":[{\"role\":\"system\",\"content\":\"Sei un agente che compie azioni. "
            "Rispondi con una azione JSON o con la risposta finale.\"},{\"role\":\"user\",\"content\":\"%s\"}],"
            "\"temperature\":0.2,\"max_tokens\":600}", esc);
    free(esc);

    const char* hnames[1] = { "Content-Type" };
    const char* hvals[1] = { "application/json" };
    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.method = "POST";
    opts.body = body;
    opts.body_len = strlen(body);
    opts.header_names = hnames;
    opts.header_values = hvals;
    opts.header_count = 1;
    opts.timeout_ms = b->timeout_ms;

    int status = 0;
    char* resp = NULL;
    size_t resp_len = 0;
    char* err = NULL;
    char* hdrs = NULL;
    int rc = gmb_net_http_request(b->url, &opts, &status, &resp, &resp_len, &err, &hdrs);
    free(body);
    if (hdrs) free(hdrs);

    if (rc != 0) {
        snprintf(out, cap, "ERRORE brain (rete): %s", err ? err : "?");
        if (err) free(err);
        if (resp) free(resp);
        return -1;
    }
    if (status != 200 || !resp) {
        snprintf(out, cap, "ERRORE brain (HTTP %d): %.200s", status, resp ? resp : (err ? err : ""));
        if (err) free(err);
        if (resp) free(resp);
        return -1;
    }

    /* estrae choices[0].message.content */
    int got = gmb_cdp_json_get_str(resp, "content", out, cap);
    free(resp);
    if (got != 0) { snprintf(out, cap, "ERRORE brain (content assente)"); return -1; }
    return 0;
}
