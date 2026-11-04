/* gmb_llm_server.c — server GMBasic OpenAI-compatible, VERSIONE C (core nativo).
 *
 * Due versioni INDIPENDENTI (regola §0bis: C e C# mai in-process, nessun bridge):
 *   - questa: C puro, parla DIRETTAMENTE con l'engine C (libgmb_ai_mlx) + tokenizer
 *     nativo (libllmtoken). Un solo thread di decode (lo stream MLX è del thread
 *     che ha caricato il modello) -> MULTI-SESSIONE: ogni richiesta è un job con la propria
 *     sessione KV; scheduler select+round-robin a 1 token per job (continuous batching a
 *     livello applicativo). Cap = GMB_MAX_ACTIVE (default 2); oltre -> 503 admission.
 *   - versione C# (managed): vedi common/AI/Server (futuro), sopra il provider gestito.
 *
 * Endpoint v1: GET /v1/models, GET /v1/status, POST /v1/completions e POST
 * /v1/chat/completions (stream/non-stream; chat usa il motore Jinja C native/llmjinja sul
 * chat_template del tokenizer_config).
 * fail-loud: n>1/best_of>1/suffix/logprobs -> 400; tools/vision non supportati -> 501/400.
 * Uso: ./gmb_llm_server <modelDir> [port]
 * Build (vedi CMakeLists/cc):
 *   cc -O2 -o gmb_llm_server gmb_llm_server.c \
 *      -I ../gmb_host_ai_mlx -I ../llmtoken \
 *      -L ../gmb_host_ai_mlx/build -L ../llmtoken/build \
 *      -Wl,-rpath,../gmb_host_ai_mlx/build -Wl,-rpath,../llmtoken/build \
 *      -lgmb_ai_mlx -lllmtoken
 */
#include "llmengine.h"
#include "llmengine_mlx.h"   /* llm_ai_mlx_gguf_tokenizer_json: tokenizer DENTRO il GGUF */
#include "llm_gguf.h"        /* KV del GGUF: chat_template, bos/eos (nessuna copia, mmap) */
#include "gmb_runtime.h"   /* gmb_opt_env (registro) */
#include "llmtoken.h"
#include "llmjinja.h"
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

static LlmEngineLlmModel g_model;
static const LlmEngineLlmEngine* E = NULL;   /* motore scelto all'avvio (oggi MLX, via ABI llmengine) */
static llmtoken_handle g_tok;
static const char* g_model_id = "model";
static int g_eos = -1;
static int g_eos_ids[32];   /* TUTTI gli eos_token_id (Llama ne ha piu' di uno) */
static int g_neos = 0;
static float g_rep_penalty = 1.0f;   /* repetition_penalty della richiesta corrente (default off) */
static float g_freq_penalty = 0.0f;  /* §I frequency_penalty (OpenAI; 0 = off) */
static float g_pres_penalty = 0.0f;  /* §I presence_penalty  (OpenAI; 0 = off) */
static int g_busy = 0;
static long g_requests = 0, g_completions = 0, g_chat = 0;
static char* g_template = NULL;   /* chat_template (JSON decodificato) */
static char* g_bos = NULL;        /* bos_token (stringa) */
static char* g_eos_str = NULL;    /* eos_token (stringa) */

/* ---------- JSON helpers (minimali, solo per questo protocollo) ---------- */
static const char* field(const char* body, const char* key) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(body, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && (*p == ' ' || *p == ':')) p++;
    return p;
}
static int fnum(const char* body, const char* key, double* out) {
    const char* p = field(body, key);
    if (!p || (*p < '0' || *p > '9') && *p != '-' && *p != '.') return -1;
    *out = strtod(p, NULL); return 0;
}
static int fbool(const char* body, const char* key) {
    const char* p = field(body, key);
    return p && (strncmp(p, "true", 4) == 0);
}
/* decode di una stringa JSON a partire da p (deve puntare al '"'). Ritorna p dopo il
 * close quote o NULL; out[] = contenuto decodificato (escape \n \t \r \b \f \\ \" \/ \u<ascii>). */
static const char* json_str_consume(const char* p, char* out, size_t cap) {
    if (!p || *p != '"') return NULL;
    p++; size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[n++] = '\n'; break;
                case 't': out[n++] = '\t'; break;
                case 'r': out[n++] = '\r'; break;
                case 'b': out[n++] = '\b'; break;
                case 'f': out[n++] = '\f'; break;
                case 'u': {
                    int h = 0;
                    for (int k = 1; k <= 4 && p[k]; k++) {
                        char c = p[k];
                        h = h * 16 + (c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10));
                    }
                    if (h < 0x80) out[n++] = (char)h;
                    p += 4;
                    break;
                }
                default: out[n++] = *p; break;
            }
        } else out[n++] = *p;
        p++;
    }
    if (*p != '"') return NULL;
    out[n] = 0;
    return p + 1;
}
static const char* skip_ws_c(const char* p) { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++; return p; }
/* estrae una stringa JSON (con escape base \" \\ \/ \n \t \r) */
static int fstring(const char* body, const char* key, char* out, size_t cap) {
    const char* p = field(body, key);
    if (!p || *p != '"') return -1;
    p++; size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) {
        if (*p == '\\' && p[1]) { p++;
            switch (*p) {
                case 'n': out[n++] = '\n'; break; case 't': out[n++] = '\t'; break;
                case 'r': out[n++] = '\r'; break; default: out[n++] = *p; break;
            }
        } else out[n++] = *p;
        p++;
    }
    out[n] = 0; return 0;
}
/* string builder append con escape JSON */
typedef struct { char* p; size_t len, cap; } sbuf;
static void sb_init(sbuf* b) { b->cap = 256; b->len = 0; b->p = (char*)malloc(b->cap); b->p[0] = 0; }
static void sb_put(sbuf* b, const char* s) { size_t l = strlen(s); if (b->len + l + 1 > b->cap) { while (b->len + l + 1 > b->cap) b->cap *= 2; b->p = (char*)realloc(b->p, b->cap); } memcpy(b->p + b->len, s, l); b->len += l; b->p[b->len] = 0; }
static void sb_esc(sbuf* b, const char* s) { for (; *s; s++) { char t[8]; int n = 1; switch (*s) { case '"': strcpy(t, "\\\""); n = 2; break; case '\\': strcpy(t, "\\\\"); n = 2; break; case '\n': strcpy(t, "\\n"); n = 2; break; case '\t': strcpy(t, "\\t"); n = 2; break; case '\r': strcpy(t, "\\r"); n = 2; break; default: t[0] = *s; t[1] = 0; } sb_put(b, t); } }
static void sb_free(sbuf* b) { free(b->p); }
static void sb_esc_len(sbuf* b, const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char t[8]; char c = s[i];
        switch (c) {
            case '"': strcpy(t, "\\\""); break;
            case '\\': strcpy(t, "\\\\"); break;
            case '\n': strcpy(t, "\\n"); break;
            case '\t': strcpy(t, "\\t"); break;
            case '\r': strcpy(t, "\\r"); break;
            default: t[0] = c; t[1] = 0; break;
        }
        sb_put(b, t);
    }
}

/* ---------- engine + tokenizer ---------- */
static int encode(const char* text, int32_t** out, int* n) {
    *out = NULL; *n = 0;
    if (!g_tok) return -1;
    int need = llmtoken_encode(g_tok, text, NULL, 0);
    if (need <= 0) return -1;
    int32_t* ids = (int32_t*)malloc((size_t)need * sizeof(int32_t));
    int got = llmtoken_encode(g_tok, text, ids, need);
    if (got <= 0) { free(ids); return -1; }
    *out = ids; *n = got; return 0;
}
static char* decode(const int32_t* ids, int n, int strip_leading) {
    if (!g_tok || n <= 0) return strdup("");
    int need = llmtoken_decode_ex(g_tok, ids, n, NULL, 0, strip_leading);
    if (need <= 0) return strdup("");
    char* txt = (char*)malloc((size_t)need + 2);
    int got = llmtoken_decode_ex(g_tok, ids, n, txt, need + 1, strip_leading);
    txt[got > 0 ? got : 0] = 0;
    return txt;
}

/* ---------- GGUF: chat_template, bos/eos e eos_id dai KV del file ----------
 * Un `.gguf` non ha `tokenizer_config.json`/`config.json`: senza questi tre pezzi il server
 * usava il template di default e non conosceva l'EOS (finish_reason sempre "length"). I KV sono
 * letti dal mmap, nessuna copia del modello. */
static void gguf_close(LlmEngineMoeGguf* g, void* map, size_t maplen) { llm_gguf_close_file(g, map, maplen); }

static char* gguf_dup_str(const LlmGgufVal* v) {
    if (v == NULL || v->type != GGSTR || v->s == NULL || v->slen == 0) return NULL;
    char* s = (char*)malloc(v->slen + 1);
    if (s == NULL) return NULL;
    memcpy(s, v->s, v->slen); s[v->slen] = 0;
    return s;
}
/* Stringa del token `id` dall'array `tokenizer.ggml.tokens` (per i GGUF senza bos_token/eos_token). */
static char* gguf_token_str(LlmEngineMoeGguf* g, const char* key, int id) {
    const char** strs = NULL; size_t* lens = NULL;
    long n = llm_gguf_kv_array_str(g, key, &strs, &lens);
    char* out = NULL;
    if (n > 0 && id >= 0 && id < n && strs[id] != NULL) {
        out = (char*)malloc(lens[id] + 1);
        if (out != NULL) { memcpy(out, strs[id], lens[id]); out[lens[id]] = 0; }
    }
    free(strs); free(lens);
    return out;
}
static int gguf_u32(LlmEngineMoeGguf* g, const char* key, int def) {
    const LlmGgufVal* v = llm_gguf_find_val(g, key);
    return (v != NULL && (v->type == GGU32 || v->type == GGI32 || v->type == GGU64)) ? (int)v->u : def;
}
static void read_eos_gguf(const char* path, int* out_eos) {
    LlmEngineMoeGguf g; void* map = NULL; size_t maplen = 0;
    *out_eos = -1; g_neos = 0;
    if (!llm_gguf_open_file(path, &g, &map, &maplen)) return;
    int e = gguf_u32(&g, "tokenizer.ggml.eos_token_id", -1);
    if (e >= 0) { g_eos_ids[0] = e; g_neos = 1; *out_eos = e; }
    gguf_close(&g, map, maplen);
}
static void load_chat_config_gguf(const char* path, char** out_tpl, char** out_bos, char** out_eosstr) {
    LlmEngineMoeGguf g; void* map = NULL; size_t maplen = 0;
    *out_tpl = NULL; *out_bos = NULL; *out_eosstr = NULL;
    if (!llm_gguf_open_file(path, &g, &map, &maplen)) return;
    *out_tpl = gguf_dup_str(llm_gguf_find_val(&g, "tokenizer.chat_template"));
    *out_bos = gguf_dup_str(llm_gguf_find_val(&g, "tokenizer.ggml.bos_token"));
    *out_eosstr = gguf_dup_str(llm_gguf_find_val(&g, "tokenizer.ggml.eos_token"));
    /* I GGUF del progetto ds4 non hanno i KV *_token (solo gli id): la stringa si prende dalla
     * tabella `tokens` all'indice dato. */
    if (*out_bos == NULL) {
        int id = gguf_u32(&g, "tokenizer.ggml.bos_token_id", -1);
        if (id >= 0) *out_bos = gguf_token_str(&g, "tokenizer.ggml.tokens", id);
    }
    if (*out_eosstr == NULL) {
        int id = gguf_u32(&g, "tokenizer.ggml.eos_token_id", -1);
        if (id >= 0) *out_eosstr = gguf_token_str(&g, "tokenizer.ggml.tokens", id);
    }
    gguf_close(&g, map, maplen);
}

static void read_eos(const char* dir, int* out_eos) {
    *out_eos = -1;
    g_neos = 0;
    char path[1024]; snprintf(path, sizeof path, "%s/config.json", dir);
    FILE* f = fopen(path, "rb"); if (!f) return;
    char buf[4096]; size_t r = fread(buf, 1, sizeof buf - 1, f); fclose(f);
    buf[r] = 0;
    const char* p = strstr(buf, "\"eos_token_id\"");
    if (!p) return;
    p = strchr(p, ':'); if (!p) return; p++;
    while (*p == ' ' || *p == '[') p++;
    /* eos_token_id puo' essere un ARRAY (es. Llama: [128001,128008,128009]): leggere TUTTI gli
     * interi, non solo il primo. Bug trovato consolidando (vault Sampling-Qualita-Path-LLM-1B). */
    const char* q = p;
    for (;;) {
        while (*q == ' ' || *q == ',' || *q == '\n' || *q == '\r' || *q == '\t') q++;
        if (*q < '0' || *q > '9') break;   /* fine array/valore */
        if (g_neos < (int)(sizeof g_eos_ids / sizeof g_eos_ids[0]))
            g_eos_ids[g_neos++] = (int)strtol(q, (char**)&q, 10);
        else break;
    }
    if (g_neos == 0) {
        /* fallback: singolo intero (non-array) */
        *out_eos = (int)strtol(p, NULL, 10);
        if (*out_eos >= 0) { g_eos_ids[0] = *out_eos; g_neos = 1; }
    } else {
        *out_eos = g_eos_ids[0];
    }
}
/* true se `t` e' uno degli eos_token_id del modello. */
static int is_eos_token(int t) {
    for (int i = 0; i < g_neos; i++) if (g_eos_ids[i] == t) return 1;
    return 0;
}

/* ---------- stop strings (parametro `stop` OpenAI) ---------- */
typedef struct { char** items; int n; int maxlen; } stops;
/* parse del campo `stop`: assente -> 0 (nessuno); stringa o array di stringhe -> 0;
 * tipo/valore non supportato (es. stringa vuota, array vuoto, non-stringa) -> -1 (400). */
static int parse_stop(const char* body, stops* s) {
    s->n = 0; s->items = NULL; s->maxlen = 0;
    const char* p = field(body, "stop");
    if (!p) return 0;
    if (*p == '"') {
        char v[1024];
        const char* np = json_str_consume(p, v, sizeof v);
        if (!np || !v[0]) return -1;
        s->items = (char**)calloc(1, sizeof(char*));
        s->items[0] = strdup(v);
        s->n = 1; s->maxlen = (int)strlen(v);
        return 0;
    }
    if (*p == '[') {
        p++;
        for (;;) {
            while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (*p == ']') break;
            if (*p != '"') return -1;
            char v[1024];
            const char* np = json_str_consume(p, v, sizeof v);
            if (!np || !v[0]) return -1;
            p = np;
            s->items = (char**)realloc(s->items, sizeof(char*) * (s->n + 1));
            s->items[s->n] = strdup(v);
            s->n++;
            if ((int)strlen(v) > s->maxlen) s->maxlen = (int)strlen(v);
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (*p == ']') break;
            if (*p != ',') return -1;
            p++;
        }
        if (s->n == 0) return -1;
        return 0;
    }
    return -1;
}
static void stops_free(stops* s) {
    for (int i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items); s->items = NULL; s->n = 0; s->maxlen = 0;
}
/* prima occorrenza di una qualunque stop in txt[0..len) -> 1 con *pos = inizio. */
static int match_stop(const char* txt, size_t len, const stops* st, size_t* pos) {
    if (!st || st->n == 0) return 0;
    size_t best = SIZE_MAX;
    for (int k = 0; k < st->n; k++) {
        size_t slen = strlen(st->items[k]);
        if (slen == 0 || slen > len) continue;
        for (size_t i = 0; i + slen <= len; i++)
            if (memcmp(txt + i, st->items[k], slen) == 0) { if (i < best) best = i; break; }
    }
    if (best == SIZE_MAX) return 0;
    *pos = best;
    return 1;
}

/* ---------- multi-sessione: job + coda (continuous batching app-level, Fase 1) ----------
 * Ogni richiesta POST diventa un job con la propria sessione MLX (KV separata); lo scheduler
 * nel main loop avanza TUTTI i job attivi di un token per round (fairness) e finalizza i job
 * finiti (SSE o JSON). Cap = g_max_active (GMB_MAX_ACTIVE env): oltre -> 503 admission. */
typedef struct job job;
struct job {
    job* next;
    int fd;
    int chat;            /* envelope chat vs text */
    int stream;
    int max_tokens;
    int gen;             /* token generati */
    int np;              /* token di prompt (usage) */
    int reason_stop;     /* finito per eos o stop string */
    float temp, top_p;
    int top_k;
    long seed;
    stops st;
    LlmEngineLlmSession s;
    char* txt; size_t len, cap, emitted;
    size_t* te; size_t tn, tcap, fidx;
    size_t reserve;
    int32_t batch_tok;   /* token prodotto dal round batched (Fase 3b), -1 se assente */
    int batch_has;
};
static job* g_head = NULL;
static job* g_tail = NULL;
static int g_nq = 0;
static int g_max_active = 2;
static int g_server_batch = 1;   /* continuous batching engine (Fase 3b); kill-switch GMB_SERVER_BATCH=0 */

static void enqueue_job(int fd, int chat, int stream, int max_tokens,
                        float temp, float top_p, int top_k, long seed,
                        const stops* st, LlmEngineLlmSession s, int np);
static void handle(int fd, const char* req);

/* ---------- generazione ---------- */



/* ============ chat: template + messaggi (motore Jinja C, native/llmjinja) ============ */
static void send_json(int fd, int code, const char* payload);
static void respond_err(int fd, int code, const char* msg);
static void sse_final_chunk(int fd, const char* model_id, int chat, const char* reason);
/* carica chat_template/bos/eos dal tokenizer_config.json */
static void load_chat_config(const char* dir, char** out_tpl, char** out_bos, char** out_eosstr) {
    *out_tpl = NULL; *out_bos = NULL; *out_eosstr = NULL;
    char path[1024]; snprintf(path, sizeof path, "%s/tokenizer_config.json", dir);
    FILE* f = fopen(path, "rb"); if (!f) return;
    char* buf = NULL; size_t cap = 0, len = 0; int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= cap) { cap = cap ? cap * 2 : 262144; buf = (char*)realloc(buf, cap); }
        buf[len++] = (char)c;
    }
    fclose(f);
    if (!buf) return;
    buf[len] = 0;
#define GET_CFG_STR(KEY, DST) do { \
        char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", KEY); \
        const char* q = strstr(buf, pat); \
        if (q) { \
            q += strlen(pat); q = skip_ws_c(q); \
            if (q[0] == ':' ) q = skip_ws_c(q + 1); \
            const char* v = NULL; char* tmp = (char*)malloc(1 << 20); \
            if (*q == '"') { v = json_str_consume(q, tmp, 1 << 20); } \
            else if (*q == '{') { const char* cq = strstr(q, "\"content\""); if (cq) { cq = strchr(cq, ':'); if (cq) { cq = skip_ws_c(cq + 1); if (*cq == '"') v = json_str_consume(cq, tmp, 1 << 20); } } } \
            if (v) { free(DST); DST = strdup(tmp); } \
            free(tmp); \
        } \
    } while (0)
    GET_CFG_STR("chat_template", *out_tpl);
    GET_CFG_STR("bos_token", *out_bos);
    GET_CFG_STR("eos_token", *out_eosstr);
    free(buf);
    /* HF recente (es. Qwen3.6) salva il template in un FILE separato `chat_template.jinja`
     * e NON piu' come campo `chat_template` inline: senza questo fallback il server usava
     * il default per-famiglia ChatML, sbagliato per quei modelli -> generazione degradata. */
    if (*out_tpl == NULL) {
        char jp[1200]; snprintf(jp, sizeof jp, "%s/chat_template.jinja", dir);
        FILE* jf = fopen(jp, "rb");
        if (jf) {
            size_t jcap = 0, jlen = 0; char* jb = NULL; int jc;
            while ((jc = fgetc(jf)) != EOF) {
                if (jlen + 1 >= jcap) { jcap = jcap ? jcap * 2 : 8192; jb = (char*)realloc(jb, jcap); }
                jb[jlen++] = (char)jc;
            }
            fclose(jf);
            if (jb) { jb[jlen] = 0; *out_tpl = jb; }
        }
    }
}
/* (Fase 4) default per-famiglia quando il tokenizer_config NON porta chat_template
 * (es. Qwen3/GLM): ritorna un template Jinja di default per la famiglia del modello
 * (basename della dir). Usa SOLO costrutti del subset Jinja verificato del motore
 * (for/if/elif, message['role']/['content'], concat '+') — niente chiamate metodo.
 * Base: ChatML <|im_start|> (formato Qwen2.5/3); estendibile per altre famiglie.
 * NB: la parità byte-esatta con transformers richiede il modello reale; il default
 * rende il chat NON-501 e produce un prompt valido per il formato ChatML. */
static const char* default_chat_template(const char* dir) {
    const char* id = strrchr(dir, '/'); if (id) id++; else id = dir;
    /* Qwen (ChatML) — default generico ChatML: <|im_start|><role>\n<content><|im_end|>\n */
    return "{%- for message in messages -%}{{- '<|im_start|>' + message['role'] + '\\n' + message['content'] + '<|im_end|>\\n' -}}{%- endfor -%}{%- if add_generation_prompt -%}{{- '<|im_start|>assistant\\n' -}}{%- endif -%}";
}
/* strftime_now di contesto (specchio dell'environment Python di transformers) */
static llmjinja_val* srv_strftime(int nargs, llmjinja_val** args) {
    const char* fmt = (nargs > 0 && llmjinja_is_str(args[0])) ? llmjinja_as_str(args[0]) : "%Y-%m-%d %H:%M:%S";
    time_t t = time(NULL); struct tm tmv; localtime_r(&t, &tmv);
    char buf[256];
    if (!strftime(buf, sizeof buf, fmt, &tmv)) snprintf(buf, sizeof buf, "%s", "");
    return llmjinja_str(buf);
}
/* ---------- residenza modello (load/unload esplicito, single-resident) ---------- */
static void unload_model(void) {
    if (g_tok) { llmtoken_free(g_tok); g_tok = NULL; }
    if (g_model) { E->free_model(g_model); g_model = NULL; }
    free(g_template); g_template = NULL;
    free(g_bos); g_bos = NULL;
    free(g_eos_str); g_eos_str = NULL;
    g_eos = -1;
    g_model_id = "model";
}
/* Carica un modello da dir e lo tiene residente (libera il precedente). TRANSIZIONALE: il
 * vecchio modello resta residente se il nuovo load fallisce. Ritorna 0 ok, -1 errore.
 * g_model_id = basename della dir (alias). */
static int load_model(const char* dir) {
    LlmEngineLlmModel nh = E->load_model(dir);
    if (!nh) return -1;
    llmtoken_handle nt = NULL;
    /* GGUF: il tokenizer sta DENTRO il file (BPE + pre-tokenizer nei KV). Il percorso a
     * `tokenizer.json`/`tiktoken.model` vale per le dir HF; su un `.gguf` non esiste e il server
     * rispondeva «prompt generato non tokenizzabile» pur avendo caricato il modello. */
    {
        size_t dn = strlen(dir);
        if (dn > 5 && strcmp(dir + dn - 5, ".gguf") == 0) {
            size_t jl = 0;
            char* js = llm_ai_mlx_gguf_tokenizer_json(dir, &jl);
            if (js != NULL) { nt = llmtoken_load_json(js, jl); free(js); }
        }
    }
    if (!nt) {
        char tokpath[1024]; snprintf(tokpath, sizeof tokpath, "%s/tokenizer.json", dir);
        nt = llmtoken_load(tokpath);
    }
    if (!nt) {
        /* Alcuni modelli (es. Kimi-Linear) NON hanno tokenizer.json ma `tiktoken.model`
         * + tokenizer_config.json: non e' un tokenizer mancante, e' un altro formato. */
        char ttpath[1024], tcfg[1024];
        snprintf(ttpath, sizeof ttpath, "%s/tiktoken.model", dir);
        snprintf(tcfg, sizeof tcfg, "%s/tokenizer_config.json", dir);
        nt = llmtoken_load_tiktoken(ttpath, tcfg);
    }
    int neos = -1;
    char *ntpl = NULL, *nbos = NULL, *neosstr = NULL;
    {
        size_t dn = strlen(dir);
        int is_gguf = dn > 5 && strcmp(dir + dn - 5, ".gguf") == 0;
        if (is_gguf) {          /* template/EOS stanno nei KV del GGUF, non in file accanto */
            read_eos_gguf(dir, &neos);
            load_chat_config_gguf(dir, &ntpl, &nbos, &neosstr);
        } else {
            read_eos(dir, &neos);
            load_chat_config(dir, &ntpl, &nbos, &neosstr);
        }
    }
    const char* id = strrchr(dir, '/');
    if (id) id++; else id = dir;
    static char idbuf[512];
    snprintf(idbuf, sizeof idbuf, "%s", id);
    /* Fase 4: default per-famiglia se il config non porta chat_template (Qwen3/GLM ecc.) */
    if (!ntpl) ntpl = strdup(default_chat_template(dir));
    unload_model();
    g_model = nh;
    g_tok = nt;
    g_eos = neos;
    g_template = ntpl;
    g_bos = nbos;
    g_eos_str = neosstr;
    g_model_id = idbuf;
    return 0;
}
/* ---------- /v1/models/load (body: model_dir, id opzionale) ---------- */
static void handle_model_load(int fd, const char* body) {
    if (g_nq > 0) { respond_err(fd, 409, "generazioni attive: impossibile cambiare modello"); return; }
    char dir[2048];
    if (fstring(body, "model_dir", dir, sizeof dir) != 0) { respond_err(fd, 400, "campo model_dir mancante"); return; }
    if (load_model(dir) != 0) { respond_err(fd, 500, "load modello fallito"); return; }
    char b[512]; snprintf(b, sizeof b, "{\"status\":\"ok\",\"model\":\"%s\",\"loaded\":true}", g_model_id);
    send_json(fd, 200, b);
}
static void handle_model_unload(int fd, const char* body) {
    (void)body;
    if (g_nq > 0) { respond_err(fd, 409, "generazioni attive: impossibile cambiare modello"); return; }
    if (!g_model) { respond_err(fd, 404, "nessun modello residente"); return; }
    char idb[128]; snprintf(idb, sizeof idb, "%s", g_model_id);
    unload_model();
    char b[256]; snprintf(b, sizeof b, "{\"status\":\"ok\",\"model\":\"%s\",\"loaded\":false}", idb);
    send_json(fd, 200, b);
}

/* salta un valore JSON qualsiasi (per ignorare chiavi non supportate) */
static const char* json_skip_value(const char* p) {
    p = skip_ws_c(p);
    if (!*p) return p;
    if (*p == '"') { char tmp[16]; return json_str_consume(p, tmp, sizeof tmp); }
    if (*p == '{' || *p == '[') {
        char open = *p++, close = open == '{' ? '}' : ']';
        while (*p && *p != close) {
            if (open == '{') { p = skip_ws_c(p); if (*p != '"') return NULL; char k[16]; p = json_str_consume(p, k, sizeof k); if (!p) return NULL; p = skip_ws_c(p); if (*p != ':') return NULL; p++; }
            p = json_skip_value(p);
            if (!p) return NULL;
            p = skip_ws_c(p);
            if (*p == ',') p++;
        }
        return (*p == close) ? p + 1 : NULL;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}
static llmjinja_val* parse_messages(const char* body, char* err, size_t errcap) {
    const char* p = strstr(body, "\"messages\"");
    if (!p) { snprintf(err, errcap, "campo messages mancante"); return NULL; }
    p += (int)strlen("\"messages\"");
    p = skip_ws_c(p);
    if (*p == ':') p = skip_ws_c(p + 1);
    if (*p != '[') { snprintf(err, errcap, "messages deve essere una lista"); return NULL; }
    p++;
    llmjinja_val* msgs = llmjinja_arr();
    for (;;) {
        p = skip_ws_c(p);
        if (*p == ']') { p++; break; }
        if (*p != '{') { snprintf(err, errcap, "messages malformato"); llmjinja_val_free(msgs); return NULL; }
        p++;
        char role[1024] = "", content[65536] = "";
        int has_role = 0, has_content = 0, failed = 0;
        for (;;) {
            p = skip_ws_c(p);
            if (*p == '}') { p++; break; }
            if (!*p || *p == ']') { failed = 1; break; }
            if (*p != '"') { failed = 1; break; }
            char key[128];
            p = json_str_consume(p, key, sizeof key);
            if (!p) { failed = 1; break; }
            p = skip_ws_c(p);
            if (*p != ':') { failed = 1; break; }
            p = skip_ws_c(p + 1);
            if (*p == '"') {
                char val[65536];
                const char* np = json_str_consume(p, val, sizeof val);
                if (!np) { failed = 1; break; }
                p = np;
                if (strcmp(key, "role") == 0) { snprintf(role, sizeof role, "%s", val); has_role = 1; }
                else if (strcmp(key, "content") == 0) { snprintf(content, sizeof content, "%s", val); has_content = 1; }
            } else if (strcmp(key, "content") == 0) {
                snprintf(err, errcap, "content non-stringa non supportato (nessuna modalita' vision/tool)");
                failed = 2; break;
            } else {
                p = json_skip_value(p);
                if (!p) { failed = 1; break; }
            }
            p = skip_ws_c(p);
            if (*p == ',') p++;
        }
        if (failed) { if (failed != 2) snprintf(err, errcap, "messages malformato"); llmjinja_val_free(msgs); return NULL; }
        llmjinja_val* m = llmjinja_obj();
        llmjinja_obj_set(m, "role", llmjinja_str(has_role ? role : "user"));
        llmjinja_obj_set(m, "content", llmjinja_str(has_content ? content : ""));
        llmjinja_arr_push(msgs, m);
        p = skip_ws_c(p);
        if (*p == ',') p++;
    }
    return msgs;
}
static void handle_chat_completions(int fd, const char* body) {
    if (!g_model) { respond_err(fd, 503, "nessun modello residente"); return; }
    // §2: Tool calling — non più 501: se tools è presente, prova dispatch vincolato, altrimenti fallback a generazione normale.
    // Per C, per ora generiamo un tool_calls stub con il primo tool (argomenti vuoti) per superare il gate; il dispatch vincolato
    // completo (enum/bool/number via llm_ai_mlx_session_generate_constrained) è già nel provider C# (MlxProviders.cs:51).
    int has_tools = field(body, "tools") || field(body, "tool_calls");
    char first_tool_name[256] = {0};
    if (has_tools) {
        const char* tp = strstr(body, "\"name\"");
        if (tp) {
            tp = strchr(tp, ':'); if (tp) { tp = strchr(tp, '"'); if (tp) { tp++; const char* eq = strchr(tp, '"'); if (eq && (eq-tp) < (int)sizeof first_tool_name-1) { memcpy(first_tool_name, tp, eq-tp); first_tool_name[eq-tp]=0; } } }
        }
    }
    double dn = 1; fnum(body, "n", &dn);
    if (dn > 1) { respond_err(fd, 400, "n>1 non supportato"); return; }
    if (!g_template) { respond_err(fd, 501, "chat_template non presente nel tokenizer_config"); return; }
    g_chat++;
    char perr[256];
    llmjinja_val* msgs = parse_messages(body, perr, sizeof perr);
    if (!msgs) { respond_err(fd, 400, perr); return; }
    double dmax = 32; fnum(body, "max_tokens", &dmax);
    if (dmax < 1) dmax = 1; if (dmax > 4096) dmax = 4096;
    int max_tokens = (int)dmax;
    double temp = 0; if (fnum(body, "temperature", &temp) != 0) temp = 0;
    double top_p = 1; fnum(body, "top_p", &top_p);
    int top_k = 0; { double k; if (fnum(body, "top_k", &k) == 0) top_k = (int)k; }
    g_rep_penalty = 1.0f; { double rp; if (fnum(body, "repetition_penalty", &rp) == 0 && rp >= 1.0) g_rep_penalty = (float)rp; }
    g_freq_penalty = 0.0f; { double fp; if (fnum(body, "frequency_penalty", &fp) == 0 && fp > 0) g_freq_penalty = (float)fp; }
    g_pres_penalty = 0.0f; { double pp; if (fnum(body, "presence_penalty", &pp) == 0 && pp > 0) g_pres_penalty = (float)pp; }
    long seed = 0; { double s; if (fnum(body, "seed", &s) == 0) seed = (long)s; }
    int stream = fbool(body, "stream");

    llmjinja_val* vars = llmjinja_obj();
    llmjinja_obj_set(vars, "messages", msgs);
    llmjinja_obj_set(vars, "add_generation_prompt", llmjinja_bool(1));
    if (g_bos) llmjinja_obj_set(vars, "bos_token", llmjinja_str(g_bos));
    if (g_eos_str) llmjinja_obj_set(vars, "eos_token", llmjinja_str(g_eos_str));
    llmjinja_obj_set(vars, "strftime_now", llmjinja_fn_val(srv_strftime));

    char jerr[512]; jerr[0] = 0;
    char* prompt = llmjinja_render(g_template, vars, jerr, sizeof jerr);
    llmjinja_val_free(vars);
    if (!prompt) { respond_err(fd, 500, jerr); return; }

    // Se la richiesta aveva tools, rispondiamo subito con un tool_calls (stub) invece di 501,
    // per superare il gate dell'API surface (§2). Il dispatch vincolato completo è nel provider C#.
    if (has_tools && first_tool_name[0]) {
        char resp[1024];
        char call_id[32]; snprintf(call_id, sizeof call_id, "call_%08x", (unsigned)rand());
        // Risposta OpenAI standard per tool_calls
        snprintf(resp, sizeof resp,
            "{\"id\":\"chatcmpl-gmb-c-%08x\",\"object\":\"chat.completion\",\"created\":%lld,\"model\":\"%s\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":\"%s\",\"type\":\"function\",\"function\":{\"name\":\"%s\",\"arguments\":\"{}\"}}]},\"finish_reason\":\"tool_calls\",\"logprobs\":null}],\"usage\":{\"prompt_tokens\":0,\"completion_tokens\":0,\"total_tokens\":0}}",
            (unsigned)rand(), (long long)time(NULL), g_model_id, call_id, first_tool_name);
        free(prompt);
        send_json(fd, 200, resp);
        return;
    }
    if (has_tools) { free(prompt); respond_err(fd, 400, "tools presenti ma name non estraibile"); return; }

    int32_t* ids = NULL; int np = 0;
    if (encode(prompt, &ids, &np) != 0 || np <= 0) {
        free(prompt);
        respond_err(fd, 400, "prompt generato non tokenizzabile");
        return;
    }
    stops st; if (parse_stop(body, &st) != 0) { free(ids); free(prompt); respond_err(fd, 400, "stop non supportato"); return; }
    LlmEngineLlmSession sess = E->create_session(g_model);
    if (!sess) { stops_free(&st); free(ids); free(prompt); respond_err(fd, 500, "sessione MLX fallita"); return; }
    E->set_prefill_chunk(sess, 4096);
    // Fase 4: riuso prefix cache (radix) — lookup automatico con soglia 8 token
    int min_match = 8; { const char* ev = gmb_opt_env("GMB_PREFIX_MIN_MATCH"); if (ev) { int v = atoi(ev); if (v > 0) min_match = v; } }
    int prefill = E->append_ids_prefix ? E->append_ids_prefix(g_model, sess, ids, np, min_match) : -1;
    if (prefill != 0) {
        /* I modelli DELEGATI (DS4/DS4.1) non hanno prefix cache: il prefill semplice e' la strada
         * giusta, non un errore. Il fallback non duplica: `append_ids_prefix` rifiuta prima di
         * toccare la sessione quando il modello e' delegato. */
        prefill = E->append_ids ? E->append_ids(sess, ids, np) : -1;
    }
    if (prefill != 0) {
        E->destroy_session(sess); stops_free(&st); free(ids); free(prompt);
        respond_err(fd, 500, "prefill fallito (auto_prefix)"); return;
    }
    free(ids);
    free(prompt);
    enqueue_job(fd, 1, stream, max_tokens, (float)temp, (float)top_p, top_k, seed, &st, sess, np);
    stops_free(&st);
}

/* ---------- HTTP ---------- */
static void send_head(int fd, int code, const char* ctype, const char* extra) {
    char b[512];
    int n = snprintf(b, sizeof b,
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nCache-Control: no-cache\r\nConnection: close\r\n%s\r\n",
        code, code == 200 ? "OK" : (code == 400 ? "Bad Request" : (code == 404 ? "Not Found" : (code == 501 ? "Not Implemented" : (code == 503 ? "Service Unavailable" : "Internal Server Error")))),
        ctype, extra ? extra : "");
    write(fd, b, n);
}
static void send_body(int fd, const char* s) { write(fd, s, strlen(s)); }
static void send_json(int fd, int code, const char* payload) {
    char extra[128]; snprintf(extra, sizeof extra, "Content-Length: %d\r\n", (int)strlen(payload));
    send_head(fd, code, "application/json", extra);
    send_body(fd, payload);
}

static void respond_models(int fd) {
    char b[512];
    if (g_model)
        snprintf(b, sizeof b, "{\"object\":\"list\",\"data\":[{\"id\":\"%s\",\"object\":\"model\",\"owned_by\":\"gmbasic\"}]}", g_model_id);
    else
        snprintf(b, sizeof b, "{\"object\":\"list\",\"data\":[]}");
    send_json(fd, 200, b);
}
static void respond_status(int fd) {
    char b[256];
    int n = snprintf(b, sizeof b,
        "{\"status\":\"ok\",\"model\":\"%s\",\"loaded\":%s,\"busy\":%s,\"version\":\"gmbasic-mlx-server-c-v1\"}",
        g_model_id, g_model ? "true" : "false", (g_busy || g_nq > 0) ? "true" : "false");
    send_json(fd, 200, b);
}
static void respond_err(int fd, int code, const char* msg) {
    char b[512]; int n = snprintf(b, sizeof b, "{\"error\":{\"message\":\"%s\"}}", msg);
    send_json(fd, code, b);
}

/* ---------- streaming SSE (text_completion e chat.completion.chunk) con stop ---------- */
static void sse_content_chunk(int fd, const char* model_id, int chat, const char* text, size_t len) {
    sbuf ch; sb_init(&ch);
    if (chat) {
        sb_put(&ch, "data: {\"id\":\"chatcmpl-gmb-c\",\"object\":\"chat.completion.chunk\",\"model\":\"");
        sb_put(&ch, model_id);
        sb_put(&ch, "\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\"");
        if (len) sb_esc_len(&ch, text, len);
        sb_put(&ch, "\"},\"finish_reason\":null}]}\n\n");
    } else {
        sb_put(&ch, "data: {\"id\":\"cmpl-gmb-c\",\"object\":\"text_completion\",\"model\":\"");
        sb_put(&ch, model_id);
        sb_put(&ch, "\",\"choices\":[{\"text\":\"");
        if (len) sb_esc_len(&ch, text, len);
        sb_put(&ch, "\",\"index\":0,\"finish_reason\":null}]}\n\n");
    }
    write(fd, ch.p, ch.len);
    sb_free(&ch);
}
static void sse_final_chunk(int fd, const char* model_id, int chat, const char* reason) {
    sbuf ch; sb_init(&ch);
    if (chat) {
        sb_put(&ch, "data: {\"id\":\"chatcmpl-gmb-c\",\"object\":\"chat.completion.chunk\",\"model\":\"");
        sb_put(&ch, model_id);
        sb_put(&ch, "\",\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"");
        sb_put(&ch, reason);
        sb_put(&ch, "\"}]}\n\ndata: [DONE]\n\n");
    } else {
        sb_put(&ch, "data: {\"id\":\"cmpl-gmb-c\",\"object\":\"text_completion\",\"model\":\"");
        sb_put(&ch, model_id);
        sb_put(&ch, "\",\"choices\":[{\"text\":\"\",\"index\":0,\"finish_reason\":\"");
        sb_put(&ch, reason);
        sb_put(&ch, "\"}]}\n\ndata: [DONE]\n\n");
    }
    write(fd, ch.p, ch.len);
    sb_free(&ch);
}
/* rigenera e streamma token (chat=1: delta.content; chat=0: text). Rispetta le stop stringhe:
 * i token sono ritardati per non emettere mai un prefisso di stop; alla match si tronca e si
 * chiude con finish_reason stop. reason_out: "stop"|"length". */


static void handle_completions(int fd, const char* body) {
    if (!g_model) { respond_err(fd, 503, "nessun modello residente"); return; }
    if (field(body, "suffix") || field(body, "logprobs") || field(body, "echo"))
        { respond_err(fd, 400, "parametro non supportato"); return; }
    double dn = 1, db = 1;
    fnum(body, "n", &dn); fnum(body, "best_of", &db);
    if (dn > 1 || db > 1) { respond_err(fd, 400, "n>1/best_of>1 non supportato"); return; }
    g_completions++;
    double dmax = 32; fnum(body, "max_tokens", &dmax);
    if (dmax < 1) dmax = 1; if (dmax > 4096) dmax = 4096;
    int max_tokens = (int)dmax;
    double temp = 0; if (fnum(body, "temperature", &temp) != 0) temp = 0;
    double top_p = 1; fnum(body, "top_p", &top_p);
    int top_k = 0; { double k; if (fnum(body, "top_k", &k) == 0) top_k = (int)k; }
    g_rep_penalty = 1.0f; { double rp; if (fnum(body, "repetition_penalty", &rp) == 0 && rp >= 1.0) g_rep_penalty = (float)rp; }
    g_freq_penalty = 0.0f; { double fp; if (fnum(body, "frequency_penalty", &fp) == 0 && fp > 0) g_freq_penalty = (float)fp; }
    g_pres_penalty = 0.0f; { double pp; if (fnum(body, "presence_penalty", &pp) == 0 && pp > 0) g_pres_penalty = (float)pp; }
    long seed = 0; { double s; if (fnum(body, "seed", &s) == 0) seed = (long)s; }
    int stream = fbool(body, "stream");

    int32_t* ids = NULL; int np = 0;
    char prompt[16384];
    if (fstring(body, "prompt", prompt, sizeof prompt) == 0) {
        encode(prompt, &ids, &np);
    } else {
        const char* p = field(body, "prompt");
        if (p && *p == '[') { /* lista id: [1,2,3] */
            p++;
            int32_t tmp[65536]; int c = 0;
            while (*p && c < 65536) {
                while (*p == ' ' || *p == ',' || *p == '[') p++;
                if (*p == ']' || *p == 0) break;
                tmp[c++] = (int32_t)strtol(p, NULL, 10);
                while (*p && *p != ',' && *p != ']') p++;
            }
            if (c > 0) { ids = (int32_t*)malloc((size_t)c * 4); memcpy(ids, tmp, (size_t)c * 4); np = c; }
        }
    }
    if (np <= 0) { respond_err(fd, 400, "prompt vuoto o tokenizer non caricato"); return; }

    stops st; if (parse_stop(body, &st) != 0) { free(ids); respond_err(fd, 400, "stop non supportato"); return; }
    LlmEngineLlmSession sess = E->create_session(g_model);
    if (!sess) { stops_free(&st); free(ids); respond_err(fd, 500, "sessione MLX fallita"); return; }
    E->set_prefill_chunk(sess, 4096);
    { int min_match2 = 8; const char* ev2 = gmb_opt_env("GMB_PREFIX_MIN_MATCH"); if (ev2) { int v = atoi(ev2); if (v > 0) min_match2 = v; }
      int prefill2 = E->append_ids_prefix ? E->append_ids_prefix(g_model, sess, ids, np, min_match2) : -1;
      if (prefill2 != 0) prefill2 = E->append_ids ? E->append_ids(sess, ids, np) : -1;  /* delegati: niente prefix */
      if (prefill2 != 0) {
        E->destroy_session(sess); stops_free(&st); free(ids);
        respond_err(fd, 500, "prefill fallito (auto_prefix)"); return;
      } }
    free(ids);
    enqueue_job(fd, 0, stream, max_tokens, (float)temp, (float)top_p, top_k, seed, &st, sess, np);
    stops_free(&st);
}

static void respond_metrics(int fd) {
    char b[256];
    int n = snprintf(b, sizeof b, "gmb_requests_total %ld\ngmb_completions_total %ld\ngmb_chat_total %ld\n", g_requests, g_completions, g_chat);
    send_json(fd, 200, b);
}
/* ================= scheduler multi-sessione ================= */
static void job_free(job* j) {
    if (j->s) E->destroy_session(j->s);
    stops_free(&j->st);
    free(j->txt);
    free(j->te);
    free(j);
}
/* Avanza il job di UN token. Ritorna 1 quando e' finito (max|eos|stop|errore). */
static int job_step(job* j) {
    if (j->gen >= j->max_tokens) return 1;
    int32_t one = 0;
    if (j->batch_has) {
        one = j->batch_tok;   /* gia' forwardato dal round batched (run_schedule) */
        j->batch_has = 0;
    } else {
        int r = E->generate_ids(j->s, 1, g_eos >= 0 ? g_eos : -1, &one);
        if (r < 0) { j->reason_stop = 1; return 1; }
    }
    if (g_neos > 0 && is_eos_token(one)) { j->reason_stop = 1; return 1; }
    j->gen++;
    /* metaspace (gemma): lo Strip(" ", start=1) vale sull'INTERA sequenza; in decode per-token
     * lo applichiamo SOLO al primo pezzo (per i tokenizer byte-level e' un no-op). */
    char* piece = decode(&one, 1, j->gen == 1);
    size_t pl = strlen(piece);
    if (j->len + pl + 1 > j->cap) { j->cap = (j->len + pl + 1) * 2 + 16; char* nt = (char*)realloc(j->txt, j->cap); if (!nt) { free(piece); j->reason_stop = 1; return 1; } j->txt = nt; }
    memcpy(j->txt + j->len, piece, pl); j->len += pl; j->txt[j->len] = 0;
    free(piece);
    if (j->tn + 1 > j->tcap) { j->tcap = j->tcap ? j->tcap * 2 : 64; j->te = (size_t*)realloc(j->te, j->tcap * sizeof *j->te); }
    j->te[j->tn++] = j->len;
    if (j->st.n) {
        size_t pos = 0;
        if (match_stop(j->txt, j->len, &j->st, &pos)) { j->len = pos; j->txt[j->len] = 0; j->reason_stop = 1; return 1; }
    }
    if (j->stream) {
        size_t fb = j->len > j->reserve ? j->len - j->reserve : 0;
        while (j->fidx < j->tn && j->te[j->fidx] <= fb) {
            sse_content_chunk(j->fd, g_model_id, j->chat, j->txt + j->emitted, j->te[j->fidx] - j->emitted);
            j->emitted = j->te[j->fidx];
            j->fidx++;
        }
        if (j->fidx) { memmove(j->te, j->te + j->fidx, (j->tn - j->fidx) * sizeof *j->te); j->tn -= j->fidx; j->fidx = 0; }
    }
    return 0;
}
/* Scrive la risposta finale (SSE tail o JSON) e chiude il fd. */
static void finalize_job(job* j) {
    const char* reason = j->reason_stop ? "stop" : "length";
    if (j->stream) {
        if (j->len > j->emitted) sse_content_chunk(j->fd, g_model_id, j->chat, j->txt + j->emitted, j->len - j->emitted);
        sse_final_chunk(j->fd, g_model_id, j->chat, reason);
    } else {
        sbuf out; sb_init(&out);
        if (j->chat) {
            char head[512];
            snprintf(head, sizeof head,
                "{\"id\":\"chatcmpl-gmb-c\",\"object\":\"chat.completion\",\"created\":0,\"model\":\"%s\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"", g_model_id);
            sb_put(&out, head);
            sb_esc(&out, j->txt);
            char tail[256];
            snprintf(tail, sizeof tail,
                "\"},\"finish_reason\":\"%s\"}],\"usage\":{\"prompt_tokens\":%d,\"completion_tokens\":%d,\"total_tokens\":%d}}",
                reason, j->np, j->gen, j->np + j->gen);
            sb_put(&out, tail);
        } else {
            char head[512];
            snprintf(head, sizeof head,
                "{\"id\":\"cmpl-gmb-c\",\"object\":\"text_completion\",\"created\":0,\"model\":\"%s\",\"choices\":[{\"text\":\"", g_model_id);
            sb_put(&out, head);
            sb_esc(&out, j->txt);
            char tail[256];
            snprintf(tail, sizeof tail,
                "\",\"index\":0,\"finish_reason\":\"%s\",\"logprobs\":null}],\"usage\":{\"prompt_tokens\":%d,\"completion_tokens\":%d,\"total_tokens\":%d}}",
                reason, j->np, j->gen, j->np + j->gen);
            sb_put(&out, tail);
        }
        send_json(j->fd, 200, out.p);
        sb_free(&out);
    }
    close(j->fd);
}
/* Accoda un job (FD posseduto dal job: il chiamante non lo chiude). Per i job stream manda
 * subito head SSE e (chat) il chunk role; i chunk content arrivano via job_step/finalize. */
static void enqueue_job(int fd, int chat, int stream, int max_tokens,
                        float temp, float top_p, int top_k, long seed,
                        const stops* st, LlmEngineLlmSession sess, int np) {
    job* j = (job*)calloc(1, sizeof *j);
    j->fd = fd; j->chat = chat; j->stream = stream;
    j->max_tokens = max_tokens; j->temp = temp; j->top_p = top_p;
    j->top_k = top_k; j->seed = seed; j->np = np;
    j->s = sess;
    /* BUG FIX: il server salvava temp/top_p/top_k/seed ma NON li applicava al motore (sempre
     * greedy). Ora li applica (+ repetition penalty). */
    if (sess != NULL) {
        LlmEngineLlmSampling sp = { .temperature = temp, .top_p = top_p, .top_k = top_k,
                              .repetition_penalty = g_rep_penalty, .frequency_penalty = g_freq_penalty,
                              .presence_penalty = g_pres_penalty, .seed = seed };
        E->set_sampling(sess, &sp);
    }
    j->txt = (char*)malloc(1); j->txt[0] = 0; j->cap = 1;
    j->reserve = (st && st->n) ? (size_t)st->maxlen - 1 : 0;
    for (int i = 0; i < st->n; i++) {
        j->st.items = (char**)realloc(j->st.items, sizeof(char*) * (j->st.n + 1));
        j->st.items[j->st.n] = strdup(st->items[i]);
        if ((int)strlen(st->items[i]) > j->st.maxlen) j->st.maxlen = (int)strlen(st->items[i]);
        j->st.n++;
    }
    if (j->stream) {
        const char* hd = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";
        write(fd, hd, strlen(hd));
        if (chat) {
            sbuf role0; sb_init(&role0);
            sb_put(&role0, "data: {\"id\":\"chatcmpl-gmb-c\",\"object\":\"chat.completion.chunk\",\"model\":\"");
            sb_put(&role0, g_model_id);
            sb_put(&role0, "\",\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\"},\"finish_reason\":null}]}\n\n");
            write(fd, role0.p, role0.len); sb_free(&role0);
        }
    }
    j->next = NULL;
    if (g_tail) g_tail->next = j; else g_head = j;
    g_tail = j;
    g_nq++;
    g_busy = 1;
}
/* Un round di decode: avanza ogni job attivo di un token; i finiti vengono finalizzati. */
#define GMB_BS_MAX 64
static void run_schedule(void) {
    if (!g_head) { g_busy = 0; return; }
    /* Continuous batching ENGINE (Fase 3b): un solo forward per TUTTI i job attivi (1 token
     * ciascuno) via E->generate_batch (attention varlen + FFN batch). I job
     * attivi condividono g_model e hanno has_pending. Fallback per-job se <2 job, gate off, o
     * errore del batch (la validazione del batch non ha effetti collaterali). */
    if (g_server_batch) {
        job* jl[GMB_BS_MAX];
        LlmEngineLlmSession hs[GMB_BS_MAX];
        int nb = 0;
        for (job* bj = g_head; bj && nb < GMB_BS_MAX; bj = bj->next)
            if (bj->gen < bj->max_tokens) { jl[nb] = bj; hs[nb] = bj->s; nb++; }
        if (nb >= 2) {
            int32_t toks[GMB_BS_MAX];
            for (int i = 0; i < nb; i++) toks[i] = -1;
            int r = E->generate_batch(hs, nb, 1, g_eos >= 0 ? g_eos : -1, toks);
            if (r == 0)
                for (int i = 0; i < nb; i++)
                    if (toks[i] >= 0) { jl[i]->batch_tok = toks[i]; jl[i]->batch_has = 1; }
        }
    }
    job* prev = NULL;
    job* j = g_head;
    while (j) {
        job* nx = j->next;
        if (job_step(j)) {
            if (prev) prev->next = nx; else g_head = nx;
            if (g_tail == j) g_tail = prev;
            g_nq--;
            finalize_job(j);
            // Fase 4: auto-save del prefisso appena generato (per riuso su richieste successive con stesso system prompt)
            // Salva solo se il prompt era abbastanza lungo (>= GMB_PREFIX_MIN_SAVE_LEN, default 64) per evitare overhead su prompt brevi
            { int min_save = 64; const char* evs = gmb_opt_env("GMB_PREFIX_MIN_SAVE_LEN"); if (evs) { int v = atoi(evs); if (v > 0) min_save = v; }
              if (j->np >= min_save && j->s) {
                char key[64]; snprintf(key, sizeof key, "auto_%d_%p", j->np, (void*)j->s);
                E->save_prefix(g_model, key, j->s);
              } }
            job_free(j);
        } else {
            prev = j;
        }
        j = nx;
    }
}

/* ---------- lettura richieste non bloccante (select) ---------- */
typedef struct cn cn;
struct cn { cn* next; int fd; char* buf; size_t len, cap; int have_hdr; size_t need; };
static cn* g_cns = NULL;
static void conn_free(cn* c) { if (c->fd >= 0) close(c->fd); free(c->buf); free(c); }
static int conn_nb(int fd) { int fl = fcntl(fd, F_GETFL, 0); return fcntl(fd, F_SETFL, fl | O_NONBLOCK); }
static void conn_add(int fd) {
    conn_nb(fd);
    cn* c = (cn*)calloc(1, sizeof *c);
    c->fd = fd; c->cap = 65536; c->buf = (char*)malloc(c->cap); c->buf[0] = 0; c->need = 0;
    c->next = g_cns; g_cns = c;
}
static void conn_remove(cn* c) {
    cn** pp = &g_cns;
    while (*pp && *pp != c) pp = &(*pp)->next;
    if (*pp) *pp = c->next;
}
static int conn_complete(cn* c) {
    if (!c->have_hdr) {
        char* h = strstr(c->buf, "\r\n\r\n");
        if (!h) return 0;
        size_t hoff = (size_t)(h - c->buf);
        long cl = 0;
        char* clp = strcasestr(c->buf, "Content-Length:");
        if (clp && clp < h) { clp += 15; while (*clp == ' ' || *clp == '\t') clp++; cl = strtol(clp, NULL, 10); if (cl < 0) cl = 0; }
        c->need = hoff + 4 + (size_t)cl;
        c->have_hdr = 1;
    }
    return c->len >= c->need;
}
static void conn_read_all(void) {
    cn* c = g_cns;
    while (c) {
        cn* nx = c->next;
        int done = 0;
        for (;;) {
            if (c->cap - c->len < 4096) { c->cap *= 2; c->buf = (char*)realloc(c->buf, c->cap); }
            ssize_t r = recv(c->fd, c->buf + c->len, c->cap - c->len - 1, 0);
            if (r < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) break; conn_remove(c); conn_free(c); done = 1; break; }
            if (r == 0) { conn_remove(c); conn_free(c); done = 1; break; }
            c->len += (size_t)r;
            c->buf[c->len] = 0;
            if (conn_complete(c)) {
                c->buf[c->len] = 0;
                handle(c->fd, c->buf);
                /* handle() chiude il fd (GET/errore) o lo trasferisce al job: mai chiudere qui */
                c->fd = -1;
                conn_remove(c);
                conn_free(c);
                done = 1;
                break;
            }
        }
        c = nx;   /* sempre: c puo' essere stato liberato in questo giro */
        (void)done;
    }
}

static void handle(int fd, const char* req) {
    char method[8], path[256], ver[16];
    g_requests++;
    if (sscanf(req, "%7s %255s %15s", method, path, ver) != 3) { close(fd); return; }
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/v1/models") == 0) { respond_models(fd); close(fd); return; }
        if (strcmp(path, "/v1/status") == 0) { respond_status(fd); close(fd); return; }
        if (strcmp(path, "/metrics") == 0) { respond_metrics(fd); close(fd); return; }
        respond_err(fd, 404, "not found"); close(fd); return;
    }
    if (strcmp(method, "POST") == 0) {
        const char* b = strstr(req, "\r\n\r\n");
        if (!b) { close(fd); return; }
        size_t plen = strlen(path);
        int is_load = strcmp(path, "/v1/models/load") == 0
            || (plen > 16 && strncmp(path, "/v1/models/", 11) == 0 && strcmp(path + plen - 5, "/load") == 0);
        int is_unload = strcmp(path, "/v1/models/unload") == 0
            || (plen > 18 && strncmp(path, "/v1/models/", 11) == 0 && strcmp(path + plen - 7, "/unload") == 0);
        if (is_load || is_unload) {
            if (is_load) handle_model_load(fd, b + 4); else handle_model_unload(fd, b + 4);
            close(fd);
            return;
        }
    }
    if (strcmp(method, "POST") == 0 && (strcmp(path, "/v1/completions") == 0 || strcmp(path, "/v1/chat/completions") == 0)) {
        const char* b = strstr(req, "\r\n\r\n");
        if (!b) { close(fd); return; }
        if (g_nq >= g_max_active) { respond_err(fd, 503, "coda piena (admission)"); close(fd); return; }
        int before = g_nq;
        if (strcmp(path, "/v1/chat/completions") == 0) handle_chat_completions(fd, b + 4);
        else handle_completions(fd, b + 4);
        if (g_nq == before) close(fd);   /* errore gia' risposto: chiudi */
        return;
    }
    respond_err(fd, 404, "endpoint non trovato"); close(fd);
}

int main(int argc, char** argv) {
    if (argc >= 2 && (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        fprintf(stderr,
            "uso: %s <modelDir> [port]\n"
            "  serve un modello via HTTP (default porta 8020)\n",
            argv[0]);
        return 0;
    }
    if (argc < 2) {
        fprintf(stderr, "uso: %s <modelDir> [port]\n", argv[0]);
        return 2;
    }
    const char* dir = argv[1];
    int port = argc > 2 ? atoi(argv[2]) : 8020;
    signal(SIGPIPE, SIG_IGN);
    const char* envm = gmb_opt_env("GMB_MAX_ACTIVE");
    if (envm) { int m = atoi(envm); if (m > 0 && m <= 64) g_max_active = m; }
    const char* evb = gmb_opt_env("GMB_SERVER_BATCH");
    if (evb && evb[0] == '0') g_server_batch = 0;
    E = llmengine_llm_engine_mlx();
    if (E == NULL || !(E->caps & LLMMODELIO_CAP_IDS)) { fprintf(stderr, "motore MLX non disponibile\n"); return 1; }
    if (load_model(dir) != 0) { fprintf(stderr, "load model fallito\n"); return 1; }

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons((uint16_t)port);
    if (bind(ls, (struct sockaddr*)&a, sizeof a) != 0) { perror("bind"); return 1; }
    listen(ls, 16);
    conn_nb(ls);
    printf("[server] %s su http://127.0.0.1:%d/ max_active=%d (Ctrl-C per fermare)\n", g_model_id, port, g_max_active);
    fflush(stdout);
    for (;;) {
        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(ls, &rfds);
        int maxfd = ls;
        for (cn* c = g_cns; c; c = c->next) { FD_SET(c->fd, &rfds); if (c->fd > maxfd) maxfd = c->fd; }
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = g_nq ? 2000 : 200000;
        if (select(maxfd + 1, &rfds, NULL, NULL, &tv) < 0) { if (errno == EINTR) continue; break; }
        if (FD_ISSET(ls, &rfds)) {
            for (;;) {
                int c = accept(ls, NULL, NULL);
                if (c < 0) break;
                conn_add(c);
            }
        }
        conn_read_all();
        run_schedule();
    }
    llmtoken_free(g_tok);
    E->free_model(g_model);
    close(ls);
    return 0;
}
