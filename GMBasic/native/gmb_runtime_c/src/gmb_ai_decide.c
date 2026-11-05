/* gmb_ai_decide.c — decision layer (JEV/SystemOne) per le gambe C: contratto + Mock/Rules.
 * Mirror di GMBasicSharp/common/AI/Decision. Il core di inferenza è in LLMEnv (ABI v8
 * CAP_DECIDE); qui non si tocca il motore. */
#include "gmb_ai_decide.h"
#include "gmb_decision_clm.h"
#include "gmb_runtime.h"
#include "gmb_network.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* 24 slot canonici: le etichette sono STABILI (cambiarle invalida bias/T allenati). */
const gmb_decision_option_t gmb_decision_slots_noul[2] = {
    { "false", "false", -1, 0.0f, 1.0f },
    { "true",  "true",  -1, 0.0f, 1.0f },
};
const gmb_decision_option_t gmb_decision_slots_score[6] = {
    { "0", "0", -1, 0.0f, 1.0f }, { "1", "1", -1, 0.0f, 1.0f },
    { "2", "2", -1, 0.0f, 1.0f }, { "3", "3", -1, 0.0f, 1.0f },
    { "4", "4", -1, 0.0f, 1.0f }, { "5", "5", -1, 0.0f, 1.0f },
};
const gmb_decision_option_t gmb_decision_slots_choice[16] = {
    { "A", "A", -1, 0.0f, 1.0f }, { "B", "B", -1, 0.0f, 1.0f },
    { "C", "C", -1, 0.0f, 1.0f }, { "D", "D", -1, 0.0f, 1.0f },
    { "E", "E", -1, 0.0f, 1.0f }, { "F", "F", -1, 0.0f, 1.0f },
    { "G", "G", -1, 0.0f, 1.0f }, { "H", "H", -1, 0.0f, 1.0f },
    { "I", "I", -1, 0.0f, 1.0f }, { "J", "J", -1, 0.0f, 1.0f },
    { "K", "K", -1, 0.0f, 1.0f }, { "L", "L", -1, 0.0f, 1.0f },
    { "M", "M", -1, 0.0f, 1.0f }, { "N", "N", -1, 0.0f, 1.0f },
    { "O", "O", -1, 0.0f, 1.0f }, { "P", "P", -1, 0.0f, 1.0f },
};

struct gmb_decision_provider {
    int is_rules;              /* 0 = mock, 1 = rules */
    char* mock_option_id;      /* mock: NULL = prima opzione */
    double mock_probability;
};

gmb_decision_provider_t* gmb_decision_provider_mock(const char* option_id, double probability) {
    if (probability < 0.0 || probability > 1.0) return NULL;
    gmb_decision_provider_t* p = (gmb_decision_provider_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->is_rules = 0;
    p->mock_probability = probability;
    if (option_id && option_id[0]) {
        size_t n = strlen(option_id);
        p->mock_option_id = (char*)malloc(n + 1);
        if (!p->mock_option_id) { free(p); return NULL; }
        memcpy(p->mock_option_id, option_id, n + 1);
    }
    return p;
}

gmb_decision_provider_t* gmb_decision_provider_rules(void) {
    gmb_decision_provider_t* p = (gmb_decision_provider_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->is_rules = 1;
    return p;
}

void gmb_decision_provider_free(gmb_decision_provider_t* provider) {
    if (!provider) return;
    free(provider->mock_option_id);
    free(provider);
}

static int ieq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static int icontains(const char* haystack, const char* needle) {
    if (!haystack || !needle || !*needle) return 0;
    size_t nl = strlen(needle);
    for (const char* p = haystack; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == nl) return 1;
    }
    return 0;
}

static const gmb_decision_option_t* find_option(const gmb_decision_option_t* options,
                                                int n_options, const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < n_options; i++)
        if (ieq(options[i].id, id)) return &options[i];
    return NULL;
}

static int validate_domain(gmb_decision_kind_t kind, const gmb_decision_option_t* options,
                           int n_options) {
    if (!options || n_options < 1 || n_options > GMB_DECISION_MAX_OPTIONS) return -1;
    if (kind == GMB_DECISION_NOUL && n_options != 2) return -1;
    for (int i = 0; i < n_options; i++) {
        if (!options[i].id || !options[i].id[0]) return -1;
        for (int j = i + 1; j < n_options; j++)
            if (ieq(options[i].id, options[j].id)) return -1;
    }
    return 0;
}

static void fill_result(gmb_decision_result_t* out, gmb_decision_kind_t kind,
                        const gmb_decision_option_t* option, double probability) {
    memset(out, 0, sizeof(*out));
    out->kind = kind;
    out->option_id = option->id;
    out->value = option->label ? option->label : option->id;
    out->probability = probability;
    out->has_confidence = 0;
    out->has_calibrated = 0;
}

int gmb_decision_evaluate(const gmb_decision_provider_t* provider, const char* state,
                          gmb_decision_kind_t kind,
                          const gmb_decision_option_t* options, int n_options,
                          gmb_decision_result_t* out) {
    if (!provider || !out) return -1;
    if (validate_domain(kind, options, n_options) != 0) return -1;

    if (!provider->is_rules) {
        const gmb_decision_option_t* option =
            provider->mock_option_id ? find_option(options, n_options, provider->mock_option_id)
                                     : &options[0];
        if (!option) return -1;
        fill_result(out, kind, option, provider->mock_probability);
        return 1;
    }

    /* Rules: NOUL riconosce yes/true/sì vs no/false; CHOICE matcha la label/slot nello stato;
     * SCORE mappa keyword di severità; altrimenti prima opzione 0.5. */
    const char* s = state ? state : "";
    if (kind == GMB_DECISION_NOUL) {
        /* UTF-8: tolower non fa case-folding sulle lettere accentate, quindi "sì" va
         * confrontato anche nelle varianti maiuscole/miste (SÌ/Sì/sÌ). */
        if (icontains(s, "sì") || icontains(s, "Sì") || icontains(s, "sÌ") || icontains(s, "SÌ") ||
            icontains(s, "si") || icontains(s, "yes") || icontains(s, "true")) {
            const gmb_decision_option_t* option = find_option(options, n_options, "true");
            if (option) { fill_result(out, kind, option, 1.0); return 1; }
        }
        if (icontains(s, "no") || icontains(s, "false")) {
            const gmb_decision_option_t* option = find_option(options, n_options, "false");
            if (option) { fill_result(out, kind, option, 1.0); return 1; }
        }
    }
    if (kind == GMB_DECISION_CHOICE) {
        for (int i = 0; i < n_options; i++) {
            const char* label = (options[i].label && options[i].label[0])
                                ? options[i].label : options[i].id;
            /* Solo label "vera" (len > 1): gli slot canonici A..P sono lettere singole e
             * matcherebbero quasi qualunque stato (es. "technical" contiene 'a'). */
            if (label && strlen(label) > 1 && icontains(s, label)) {
                fill_result(out, kind, &options[i], 1.0);
                return 1;
            }
        }
    }
    if (kind == GMB_DECISION_SCORE) {
        int max_score = 0;
        for (int i = 0; i < n_options; i++) {
            int v = atoi(options[i].id ? options[i].id : "");
            if (v > max_score) max_score = v;
        }
        static const struct { const char* key; const char* slot5; const char* slot10; } map[] = {
            { "critical", "5", "10" }, { "critico", "5", "10" },
            { "high", "4", "8" }, { "alto", "4", "8" },
            { "medium", "3", "5" }, { "medio", "3", "5" },
            { "low", "2", "3" }, { "basso", "2", "3" },
            { "none", "0", "0" }, { "nessuno", "0", "0" },
        };
        for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
            if (!icontains(s, map[i].key)) continue;
            const gmb_decision_option_t* option =
                find_option(options, n_options, max_score >= 10 ? map[i].slot10 : map[i].slot5);
            if (option) { fill_result(out, kind, option, 1.0); return 1; }
        }
    }

    fill_result(out, kind, &options[0], 0.5);
    return 1;
}

static void json_escape(const char* src, char* dst, size_t cap) {
    size_t o = 0;
    if (cap == 0) return;
    for (const char* p = src ? src : ""; *p && o + 2 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { dst[o++] = '\\'; dst[o++] = (char)c; }
        else if (c == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; }
        else if (c == '\r') { dst[o++] = '\\'; dst[o++] = 'r'; }
        else if (c == '\t') { dst[o++] = '\\'; dst[o++] = 't'; }
        else if (c < 0x20) { continue; }
        else dst[o++] = (char)c;
    }
    dst[o] = 0;
}

int gmb_decision_result_json(const gmb_decision_result_t* result, char* out, size_t cap) {
    if (!result || !out || cap == 0) return -1;
    const char* kind = result->kind == GMB_DECISION_NOUL ? "NOUL"
                     : result->kind == GMB_DECISION_SCORE ? "SCORE" : "CHOICE";
    char option[256], value[512], conf[48], cal[48];
    json_escape(result->option_id, option, sizeof(option));
    json_escape(result->value, value, sizeof(value));
    if (result->has_confidence)
        snprintf(conf, sizeof(conf), "%.6f", result->confidence);
    else
        snprintf(conf, sizeof(conf), "null");
    if (result->has_calibrated)
        snprintf(cal, sizeof(cal), "%.6f", result->calibrated);
    else
        snprintf(cal, sizeof(cal), "null");
    int n = snprintf(out, cap,
        "{\"kind\":\"%s\",\"option\":\"%s\",\"value\":\"%s\",\"probability\":%.6f,"
        "\"confidence\":%s,\"calibrated\":%s}",
        kind, option, value, result->probability, conf, cal);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}

/* Ponte per la dylib AI (vedi gmb_ai_decide.h): il riferimento forte a queste funzioni del
 * core garantisce che i loro oggetti (`gmb_ai.o`, `gmb_paths.o`) entrino nel link anche in un
 * programma che usa SOLO `AI DECIDE ... "local"`. */
const char* gmb_ai_decide_active_model_path(void)
{
    return gmb_provider_active_model_path();
}

/* Il decisore in corso e' un provider `%DECIDE` di kind `verbalizer`? Il provider locale applica la
 * config di quel provider (decide/decideTemplate/decideCalibration) SOLO in questo caso: una chiamata
 * `USING "local"` usa sempre la config del modello completion, anche se un verbalizer e' attivo. */
static __thread int g_decide_profile_on;
int gmb_ai_decide_profile_active(void)
{
    return g_decide_profile_on;
}

/* ── Provider REMOTO TypeSafe (JEV): POST JSON, risposta con la decisione già calcolata ── */
static int dc_typesafe_run(const char* state, const char* kind,
                           const gmb_decision_option_t* options, int n_options,
                           gmb_decision_result_t* out, char* err, size_t errcap) {
    const char* cfg = gmb_provider_active_config("decide");
    if (!cfg || !*cfg) {
        if (err && errcap) snprintf(err, errcap, "typesafe: nessun provider %%DECIDE attivo con config baseUrl");
        return -1;
    }
    char* base = gmb_json_extract(cfg, "baseUrl");
    if (!base || !*base) {
        free(base);
        if (err && errcap) snprintf(err, errcap, "typesafe: baseUrl mancante nel config del provider %%DECIDE");
        return -1;
    }
    char* api = gmb_json_extract(cfg, "apiKey");
    char* model = gmb_json_extract(cfg, "model");
    char* timeout_s = gmb_json_extract(cfg, "timeoutMs");
    /* Al remoto si manda il kind CANONICO (NOUL|CHOICE|SCORE), non "CHOICE:labels"/"SCORE:0:10". */
    const char* kind_wire = (kind && strncasecmp(kind, "CHOICE", 6) == 0) ? "CHOICE"
                          : (kind && strncasecmp(kind, "SCORE", 5) == 0) ? "SCORE" : "NOUL";

    size_t cap = strlen(state ? state : "") + strlen(kind ? kind : "") + 1024;
    for (int i = 0; i < n_options; i++) {
        cap += strlen(options[i].id ? options[i].id : "") + strlen(options[i].label ? options[i].label : "") + 96;
    }
    char* req = (char*)malloc(cap);
    if (!req) { free(base); free(api); free(model); free(timeout_s); return -1; }
    size_t off = 0;
    #define DC_APP(...) do { int _n = snprintf(req + off, cap - off, __VA_ARGS__);         if (_n < 0 || (size_t)_n >= cap - off) { off = cap; } else off += (size_t)_n; } while (0)
    DC_APP("{\"state\":\"");
    if (off < cap) { json_escape(state ? state : "", req + off, cap - off); off += strlen(req + off); }
    DC_APP("\",\"kind\":\"");
    if (off < cap) { json_escape(kind_wire, req + off, cap - off); off += strlen(req + off); }
    DC_APP("\",\"options\":[");
    for (int i = 0; i < n_options && off < cap; i++) {
        if (i) DC_APP(",");
        DC_APP("{\"id\":\"");
        if (off < cap) { json_escape(options[i].id ? options[i].id : "", req + off, cap - off); off += strlen(req + off); }
        DC_APP("\",\"label\":\"");
        if (off < cap) { json_escape(options[i].label ? options[i].label : "", req + off, cap - off); off += strlen(req + off); }
        DC_APP("\"}");
    }
    DC_APP("]");
    if (model && *model) {
        DC_APP(",\"model\":\"");
        if (off < cap) { json_escape(model, req + off, cap - off); off += strlen(req + off); }
        DC_APP("\"");
    }
    DC_APP("}");
    if (off >= cap) {
        free(req); free(base); free(api); free(model); free(timeout_s);
        if (err && errcap) snprintf(err, errcap, "typesafe: richiesta troppo grande");
        return -1;
    }

    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof opts);
    opts.method = "POST";
    opts.body = req;
    opts.body_len = strlen(req);
    opts.timeout_ms = 15000;
    opts.allow_redirects = 1;
    opts.max_redirects = 5;
    if (api && *api) opts.bearer_token = api;
    if (timeout_s && *timeout_s) {
        long t = strtol(timeout_s, NULL, 10);
        if (t > 0) opts.timeout_ms = t;
    }
    int status = 0;
    char* body = NULL; size_t blen = 0; char* neterr = NULL; char* hdrs = NULL;
    int rc = gmb_net_http_request(base, &opts, &status, &body, &blen, &neterr, &hdrs);
    free(req); free(base); free(api); free(model); free(timeout_s);
    if (hdrs) free(hdrs);
    if (rc != 0) {
        if (err && errcap) snprintf(err, errcap, "typesafe: HTTP fallito: %s", neterr ? neterr : "errore di rete");
        free(body); free(neterr);
        return -1;
    }
    free(neterr);
    if (status < 200 || status >= 300) {
        if (err && errcap) snprintf(err, errcap, "typesafe: HTTP %d", status);
        free(body);
        return -1;
    }
    if (!body || blen == 0) {
        free(body);
        if (err && errcap) snprintf(err, errcap, "typesafe: risposta vuota");
        return -1;
    }
    char* js = (char*)malloc(blen + 1);
    if (!js) { free(body); return -1; }
    memcpy(js, body, blen); js[blen] = 0; free(body);

    char* opt_text = gmb_json_extract(js, "option");
    char* prob_text = gmb_json_extract(js, "probability");
    char* conf_text = gmb_json_extract(js, "confidence");
    char* cal_text = gmb_json_extract(js, "calibrated");
    char* rkind = gmb_json_extract(js, "kind");
    if (!opt_text || !*opt_text || !prob_text || !*prob_text) {
        if (err && errcap) snprintf(err, errcap, "typesafe: risposta senza option/probability");
        free(js); free(opt_text); free(prob_text); free(conf_text); free(cal_text); free(rkind);
        return -1;
    }
    double prob = atof(prob_text);
    if (!(prob >= 0.0 && prob <= 1.0)) {
        if (err && errcap) snprintf(err, errcap, "typesafe: probability fuori [0,1]");
        free(js); free(opt_text); free(prob_text); free(conf_text); free(cal_text); free(rkind);
        return -1;
    }
    const gmb_decision_option_t* chosen = NULL;
    for (int i = 0; i < n_options; i++) {
        if (options[i].id && ieq(options[i].id, opt_text)) { chosen = &options[i]; break; }
    }
    if (!chosen) {
        for (int i = 0; i < n_options; i++) {
            if (options[i].label && ieq(options[i].label, opt_text)) { chosen = &options[i]; break; }
        }
    }
    if (!chosen) {
        if (err && errcap) snprintf(err, errcap, "typesafe: option '%s' non è nel dominio", opt_text);
        free(js); free(opt_text); free(prob_text); free(conf_text); free(cal_text); free(rkind);
        return -1;
    }
    if (rkind && *rkind) {
        int match = ieq(rkind, kind_wire);
        if (!match) {
            if (err && errcap) snprintf(err, errcap, "typesafe: kind '%s' incoerente", rkind);
            free(js); free(opt_text); free(prob_text); free(conf_text); free(cal_text); free(rkind);
            return -1;
        }
    }
    memset(out, 0, sizeof(*out));
    out->kind = (gmb_decision_kind_t)((kind && ieq(kind, "NOUL")) ? 0 : (kind && strncasecmp(kind, "SCORE", 5) == 0) ? 2 : 1);
    out->option_id = chosen->id;
    out->value = chosen->label ? chosen->label : chosen->id;
    out->probability = prob;
    if (conf_text && *conf_text && strcmp(conf_text, "null") != 0) { out->confidence = atof(conf_text); out->has_confidence = 1; }
    if (cal_text && *cal_text && strcmp(cal_text, "null") != 0) { out->calibrated = atof(cal_text); out->has_calibrated = 1; }
    free(js); free(opt_text); free(prob_text); free(conf_text); free(cal_text); free(rkind);
    return 1;
}

/* ── Provider `clm` (Contrastive Language Models): stesso contratto SystemOne ─────────────────
 * Config del provider %DECIDE (JSON): `baseUrl` (server `clm-serve`; se non contiene
 * `/v1/systemone` viene aggiunto), `apiKey`, `timeoutMs`, `instructions` (testo della domanda).
 * Senza `baseUrl` ma con `head` il percorso è LOCALE: encoder MLX last-token (`encoder`, altrimenti
 * il modello attivo) + head in C. Ogni errore è ESPLICITO, mai un fallback silenzioso.
 * `cfg` è passata ESPLICITAMENTE: il C# (P/Invoke) tiene il proprio registry dei provider. */
static int dc_sb_add(char** buf, size_t* cap, size_t* off, const char* s, size_t len);
typedef struct { char *base, *api, *timeout, *instr, *head, *enc; } clm_cfg_t;

static void clm_cfg_free(clm_cfg_t* c)
{
    free(c->base); free(c->api); free(c->timeout); free(c->instr); free(c->head); free(c->enc);
    memset(c, 0, sizeof *c);
}

/* 0 = ok (remoto se base, altrimenti locale), -1 = errore in err. */
static int clm_cfg_load(const char* cfg, clm_cfg_t* c, char* err, size_t errcap)
{
    memset(c, 0, sizeof *c);
    if (!cfg || !*cfg) {
        if (err && errcap) snprintf(err, errcap, "clm: nessun provider %%DECIDE attivo con config (baseUrl)");
        return -1;
    }
    c->base = gmb_json_extract(cfg, "baseUrl");
    c->api = gmb_json_extract(cfg, "apiKey");
    c->timeout = gmb_json_extract(cfg, "timeoutMs");
    c->instr = gmb_json_extract(cfg, "instructions");
    c->head = gmb_json_extract(cfg, "head");
    c->enc = gmb_json_extract(cfg, "encoder");
    if ((!c->base || !*c->base) && (!c->head || !*c->head)) {
        clm_cfg_free(c);
        if (err && errcap) snprintf(err, errcap, "clm: baseUrl mancante nel config del provider %%DECIDE");
        return -1;
    }
    return 0;
}

static __thread gmb_clm_stats_t g_clm_stats;
static __thread int g_clm_stats_valid;

static double dc_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

/* POST del corpo `req` a <baseUrl>/v1/systemone → `*js` (heap, NUL-terminato). 0 ok / -1. */
static int clm_http_post(const clm_cfg_t* c, const char* req, char** js, double* ms_net, char* err, size_t errcap)
{
    char url[2048];
    size_t bl = strlen(c->base);
    snprintf(url, sizeof url, "%s%s", c->base,
             strstr(c->base, "/v1/systemone") ? "" : (c->base[bl - 1] == '/' ? "v1/systemone" : "/v1/systemone"));
    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof opts);
    opts.method = "POST";
    opts.body = req;
    opts.body_len = strlen(req);
    opts.timeout_ms = 15000;
    opts.allow_redirects = 1;
    opts.max_redirects = 5;
    if (c->api && *c->api) opts.bearer_token = c->api;
    if (c->timeout && *c->timeout) { long t = strtol(c->timeout, NULL, 10); if (t > 0) opts.timeout_ms = t; }
    int status = 0;
    char* body = NULL; size_t blen = 0; char* neterr = NULL; char* hdrs = NULL;
    double t0 = dc_now_ms();
    int rc = gmb_net_http_request(url, &opts, &status, &body, &blen, &neterr, &hdrs);
    *ms_net = dc_now_ms() - t0;
    free(hdrs);
    if (rc != 0) {
        if (err && errcap) snprintf(err, errcap, "clm: HTTP fallito: %s", neterr ? neterr : "errore di rete");
        free(body); free(neterr);
        return -1;
    }
    free(neterr);
    if (status < 200 || status >= 300) {
        if (err && errcap) snprintf(err, errcap, "clm: HTTP %d", status);
        free(body);
        return -1;
    }
    *js = (char*)malloc(blen + 1);
    if (!*js) { free(body); return -1; }
    if (body && blen) memcpy(*js, body, blen);
    (*js)[blen] = 0;
    free(body);
    return 0;
}

static void clm_stats_remote(double ms_total, double ms_net, double margin)
{
    memset(&g_clm_stats, 0, sizeof g_clm_stats);
    g_clm_stats.local = 0; g_clm_stats.ms_total = ms_total; g_clm_stats.ms_net = ms_net; g_clm_stats.margin = margin;
    g_clm_stats_valid = 1;
}

static int dc_clm_run_cfg(const char* cfg, const char* state, const char* kind,
                          const gmb_decision_option_t* options, int n_options,
                          gmb_decision_kind_t dkind, gmb_decision_result_t* out,
                          char* err, size_t errcap)
{
    clm_cfg_t c;
    if (clm_cfg_load(cfg, &c, err, errcap) != 0) return -1;
    double t0 = dc_now_ms();
    int rc = -1;
    if (c.base && *c.base) {
        const char* kind_wire = (kind && strncasecmp(kind, "CHOICE", 6) == 0) ? "CHOICE"
                              : (kind && strncasecmp(kind, "SCORE", 5) == 0) ? "SCORE" : "NOUL";
        char* req = gmb_clm_build_request(state, kind_wire, options, n_options, c.instr);
        char* js = NULL; double ms_net = 0;
        if (req && clm_http_post(&c, req, &js, &ms_net, err, errcap) == 0) {
            rc = gmb_clm_parse_response(js, dkind, options, n_options, out, err, errcap);
            if (rc == 1) {
                float tmp[GMB_DECISION_MAX_OPTIONS]; (void)tmp;
                clm_stats_remote(dc_now_ms() - t0, ms_net, out->confidence);
            }
        }
        free(req); free(js);
    } else {
        /* LOCALE: encoder Qwen3-8B (config "encoder", altrimenti il modello attivo) + head. */
        const char* enc_path = (c.enc && *c.enc) ? c.enc : gmb_ai_decide_active_model_path();
        char** texts = gmb_clm_candidate_texts(dkind, options, n_options, c.instr);
        float pr[GMB_DECISION_MAX_OPTIONS];
        gmb_clm_stats_t st;
        if (texts && n_options <= GMB_DECISION_MAX_OPTIONS &&
            gmb_ai_decide_clm_local_scores(state, c.instr, (const char* const*)texts, n_options,
                                           enc_path, c.head, pr, &st, err, errcap) == 0) {
            int top = 0;
            for (int i = 1; i < n_options; i++) if (pr[i] > pr[top]) top = i;
            memset(out, 0, sizeof *out);
            out->kind = dkind;
            out->option_id = options[top].id;
            out->value = (options[top].label && *options[top].label) ? options[top].label : options[top].id;
            out->probability = pr[top];
            out->confidence = gmb_clm_confidence(pr, n_options); out->has_confidence = 1;
            g_clm_stats = st; g_clm_stats_valid = 1;
            rc = 1;
        }
        gmb_clm_free_texts(texts, n_options);
    }
    clm_cfg_free(&c);
    return rc == 1 ? 1 : -1;
}

/* RANK: best-of-N su candidati liberi (kind "RANK:c1|c2|…"). probs[n] = distribuzione sui candidati. */
static int dc_clm_rank_cfg(const char* cfg, const char* state, const char* const* cands, int n,
                           float* probs, char* err, size_t errcap)
{
    clm_cfg_t c;
    if (clm_cfg_load(cfg, &c, err, errcap) != 0) return -1;
    double t0 = dc_now_ms();
    int rc = -1;
    if (c.base && *c.base) {
        char* req = gmb_clm_build_rank_request(state, c.instr, cands, n);
        char* js = NULL; double ms_net = 0;
        if (req && clm_http_post(&c, req, &js, &ms_net, err, errcap) == 0 &&
            gmb_clm_parse_rank_response(js, n, probs, err, errcap) == 0) {
            float top = -1, second = -1;
            for (int i = 0; i < n; i++) { if (probs[i] > top) { second = top; top = probs[i]; } else if (probs[i] > second) second = probs[i]; }
            clm_stats_remote(dc_now_ms() - t0, ms_net, n > 1 ? (double)(top - second) : 0.0);
            rc = 0;
        }
        free(req); free(js);
    } else {
        const char* enc_path = (c.enc && *c.enc) ? c.enc : gmb_ai_decide_active_model_path();
        gmb_clm_stats_t st;
        if (gmb_ai_decide_clm_local_scores(state, c.instr, cands, n, enc_path, c.head, probs, &st, err, errcap) == 0) {
            g_clm_stats = st; g_clm_stats_valid = 1;
            rc = 0;
        }
    }
    clm_cfg_free(&c);
    return rc;
}

#define DC_RANK_MAX 256

/* "RANK:c1|c2|…" → {"kind":"RANK","ranked":[{"rank":1,"candidate":"…","probability":p},…],"confidence":c}.
 * Errori: {"error":"…"} (mai un risultato inventato). */
static char* dc_rank_run_json(const char* state, const char* kind, const char* cfg)
{
    char err[GMB_DECISION_ERR_CAP] = "";
    char* copy = strdup(kind + 5);
    const char* cands[DC_RANK_MAX]; int n = 0;
    float* probs = NULL; char* out = NULL;
    if (!copy) return NULL;
    for (char* p = copy; p && *p && n < DC_RANK_MAX; ) {
        char* sep = strchr(p, '|');
        if (sep) *sep = 0;
        while (*p == ' ' || *p == '\t') p++;
        char* e = p + strlen(p);
        while (e > p && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        if (*p) cands[n++] = p;
        p = sep ? sep + 1 : NULL;
    }
    if (n < 2) {
        snprintf(err, sizeof err, "clm: RANK richiede almeno 2 candidati");
    } else if (!(probs = (float*)malloc((size_t)n * sizeof(float)))) {
        snprintf(err, sizeof err, "clm: memoria esaurita");
    } else if (dc_clm_rank_cfg(cfg, state ? state : "", cands, n, probs, err, sizeof err) == 0) {
        int* idx = (int*)malloc((size_t)n * sizeof(int));
        if (idx) {
            for (int i = 0; i < n; i++) idx[i] = i;
            for (int i = 1; i < n; i++) {                      /* insertion sort stabile, prob decrescente */
                int v = idx[i], j = i - 1;
                while (j >= 0 && probs[idx[j]] < probs[v]) { idx[j + 1] = idx[j]; j--; }
                idx[j + 1] = v;
            }
            size_t cap = 0, off = 0;
            char hd[64];
            int hn = snprintf(hd, sizeof hd, "{\"kind\":\"RANK\",\"ranked\":[");
            dc_sb_add(&out, &cap, &off, hd, (size_t)hn);
            for (int r = 0; r < n; r++) {
                int i = idx[r];
                size_t el = strlen(cands[i]) * 6 + 8;
                char* esc = (char*)malloc(el);
                if (!esc) break;
                json_escape(cands[i], esc, el);
                char item[64];
                int in = snprintf(item, sizeof item, "%s{\"rank\":%d,\"candidate\":\"", r ? "," : "", r + 1);
                dc_sb_add(&out, &cap, &off, item, (size_t)in);
                dc_sb_add(&out, &cap, &off, esc, strlen(esc));
                in = snprintf(item, sizeof item, "\",\"probability\":%.6f}", (double)probs[i]);
                dc_sb_add(&out, &cap, &off, item, (size_t)in);
                free(esc);
            }
            char tl[64];
            int tn = snprintf(tl, sizeof tl, "],\"confidence\":%.6f}", gmb_clm_confidence(probs, n));
            dc_sb_add(&out, &cap, &off, tl, (size_t)tn);
            free(idx);
        }
    }
    free(probs); free(copy);
    if (out) return out;
    char esc[GMB_DECISION_ERR_CAP + 64], buf[GMB_DECISION_ERR_CAP + 96];
    json_escape(err[0] ? err : "clm: errore", esc, sizeof esc);
    snprintf(buf, sizeof buf, "{\"error\":\"%s\"}", esc);
    return strdup(buf);
}

/* Misure dell'ULTIMA decisione clm su questo thread (VERIFY): NULL se non ce n'è stata una. */
void gmb_ai_decide_clm_stats_reset(void) { g_clm_stats_valid = 0; }
char* gmb_ai_decide_clm_stats_json(void)
{
    if (!g_clm_stats_valid) return NULL;
    char buf[320];
    int n = snprintf(buf, sizeof buf,
        "{\"mode\":\"%s\",\"ms\":%.3f,\"encoderMs\":%.3f,\"headMs\":%.3f,\"netMs\":%.3f,\"embeddings\":%d,\"margin\":%.6f,\"cache\":\"off\"}",
        g_clm_stats.local ? "local" : "remote", g_clm_stats.ms_total, g_clm_stats.ms_encoder,
        g_clm_stats.ms_head, g_clm_stats.ms_net, g_clm_stats.embeddings, g_clm_stats.margin);
    return n < 0 ? NULL : strdup(buf);
}

/* ── P6 — adapter REMOTO (OpenAI-compatibile) ─────────────────────────────────────────────
 * Usa il provider completion attivo quando ha `baseUrl` (endpoint remoto): POST
 * `<baseUrl>[/chat/completions]` con i messaggi system/user, legge
 * `choices[0].message.content` e riconosce l'opzione. Ritorna:
 *   1 = risultato, 0 = nessun baseUrl (usa il provider locale), -1 = errore esplicito. */
static int dc_adapter_remote_run(const char* state, const char* kind,
                                 const gmb_decision_option_t* options, int n_options,
                                 gmb_decision_result_t* out, char* err, size_t errcap)
{
    const char* cfg = gmb_provider_active_config("completion");
    if (!cfg || !*cfg) return 0;
    char* base = gmb_json_extract(cfg, "baseUrl");
    if (!base || !*base) { free(base); return 0; }
    char* api = gmb_json_extract(cfg, "apiKey");
    char* model = gmb_json_extract(cfg, "model");
    char* timeout_ms = gmb_json_extract(cfg, "timeoutMs");
    if (!api) api = strdup("");
    if (!model) model = strdup("");
    if (!timeout_ms) timeout_ms = strdup("");

    char url[2048];
    size_t bl = strlen(base);
    if (strstr(base, "/chat/completions"))
        snprintf(url, sizeof url, "%s", base);
    else
        snprintf(url, sizeof url, "%s%s/chat/completions", base, bl && base[bl - 1] == '/' ? "" : "");
    free(base);

    char labels[4096]; labels[0] = 0;
    for (int i = 0; i < n_options; i++) {
        const char* lab = (options[i].label && *options[i].label) ? options[i].label : options[i].id;
        size_t used = strlen(labels);
        if (used >= sizeof labels) break;
        snprintf(labels + used, sizeof labels - used, "%s%s", i ? ", " : "", lab ? lab : "");
    }
    size_t need = strlen(state ? state : "") + strlen(labels) + strlen(model) + 2048;
    char* req = (char*)malloc(need);
    if (!req) { free(api); free(model); free(timeout_ms); return -1; }
    size_t off = 0;
    #define DC_AD_APP(...) do { int _n = snprintf(req + off, need - off, __VA_ARGS__); \
        if (_n < 0 || (size_t)_n >= need - off) { off = need; } else off += (size_t)_n; } while (0)
    DC_AD_APP("{\"model\":\"");
    if (off < need) { json_escape(model, req + off, need - off); off += strlen(req + off); }
    DC_AD_APP("\",\"temperature\":0,\"max_tokens\":48,\"messages\":[{\"role\":\"system\",\"content\":\"");
    if (off < need) {
        json_escape("You are a decision engine. Choose exactly one allowed option.", req + off, need - off);
        off += strlen(req + off);
    }
    DC_AD_APP("\"},{\"role\":\"user\",\"content\":\"State:\\n");
    if (off < need) { json_escape(state ? state : "", req + off, need - off); off += strlen(req + off); }
    DC_AD_APP("\\nAllowed options: ");
    if (off < need) { json_escape(labels, req + off, need - off); off += strlen(req + off); }
    DC_AD_APP("\\nAnswer with exactly one option and nothing else.\"}]}");
    if (off >= need) {
        free(req); free(api); free(model); free(timeout_ms);
        if (err && errcap) snprintf(err, errcap, "adapter remoto: richiesta troppo grande");
        return -1;
    }

    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof opts);
    opts.method = "POST";
    opts.body = req;
    opts.body_len = strlen(req);
    opts.timeout_ms = 30000;
    if (api && *api) opts.bearer_token = api;
    if (timeout_ms && *timeout_ms) {
        long t = strtol(timeout_ms, NULL, 10);
        if (t > 0) opts.timeout_ms = t;
    }
    int status = 0;
    char* body = NULL; size_t blen = 0; char* neterr = NULL; char* hdrs = NULL;
    int rc = gmb_net_http_request(url, &opts, &status, &body, &blen, &neterr, &hdrs);
    free(req); free(api); free(model); free(timeout_ms);
    if (hdrs) free(hdrs);
    if (rc != 0) {
        if (err && errcap) snprintf(err, errcap, "adapter remoto: HTTP fallito: %s", neterr ? neterr : "rete");
        free(body); free(neterr);
        return -1;
    }
    free(neterr);
    if (status < 200 || status >= 300) {
        if (err && errcap) snprintf(err, errcap, "adapter remoto: HTTP %d", status);
        free(body);
        return -1;
    }
    if (!body || blen == 0) {
        free(body);
        if (err && errcap) snprintf(err, errcap, "adapter remoto: risposta vuota");
        return -1;
    }
    char* js = (char*)malloc(blen + 1);
    if (!js) { free(body); return -1; }
    memcpy(js, body, blen); js[blen] = 0; free(body);
    char* content = gmb_json_extract(js, "choices[0].message.content");
    if (!content || !*content) { free(content); content = gmb_json_extract(js, "content"); }
    free(js);
    if (!content || !*content) {
        free(content);
        if (err && errcap) snprintf(err, errcap, "adapter remoto: risposta senza content");
        return -1;
    }
    int best = gmb_decision_match_text(options, n_options, content);
    if (best < 0) {
        char detail[200];
        size_t n = strlen(content);
        if (n > 180) n = 180;
        memcpy(detail, content, n); detail[n] = 0;
        if (err && errcap) snprintf(err, errcap,
            "adapter remoto: la risposta non corrisponde a un'opzione ('%s')", detail);
        free(content);
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->kind = (kind && ieq(kind, "NOUL")) ? GMB_DECISION_NOUL
             : (kind && strncasecmp(kind, "SCORE", 5) == 0) ? GMB_DECISION_SCORE
             : GMB_DECISION_CHOICE;
    out->option_id = options[best].id;
    out->value = (options[best].label && *options[best].label) ? options[best].label : options[best].id;
    out->probability = 1.0f;
    free(content);
    return 1;
}

/* ── Match deterministico di una risposta generata (provider adapter) ───────────────────── */
static void dc_match_trim(const char* in, char* out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!in) return;
    const char* p = in;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '"' || *p == '\'' ||
           *p == '(' || *p == '[' || *p == '{') p++;
    const char* e = p + strlen(p);
    while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r' ||
                     e[-1] == '"' || e[-1] == '\'' || e[-1] == '.' || e[-1] == '!' ||
                     e[-1] == '?' || e[-1] == ':' || e[-1] == ';' || e[-1] == ')' ||
                     e[-1] == ']' || e[-1] == '}')) e--;
    size_t n = (size_t)(e - p);
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n);
    out[n] = 0;
}

static int dc_match_alnum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int dc_match_word_contains(const char* haystack, const char* needle) {
    if (!needle || !*needle) return 0;
    size_t nl = strlen(needle);
    for (const char* p = haystack; *p; p++) {
        if (strncasecmp(p, needle, nl) != 0) continue;
        int left_ok = (p == haystack) || !dc_match_alnum(p[-1]);
        int right_ok = !dc_match_alnum(p[nl]);
        if (left_ok && right_ok) return 1;
    }
    return 0;
}

int gmb_decision_match_text(const gmb_decision_option_t* options, int n, const char* text) {
    if (!options || n < 1 || !text) return -1;
    char t[512];
    dc_match_trim(text, t, sizeof t);
    if (!t[0]) return -1;
    for (int i = 0; i < n; i++)
        if ((options[i].id && strcasecmp(t, options[i].id) == 0) ||
            (options[i].label && strcasecmp(t, options[i].label) == 0))
            return i;
    char* end = NULL;
    double num = strtod(t, &end);
    if (end && end != t) {
        while (*end == ' ' || *end == '\t') end++;
        if (*end == 0) {
            for (int i = 0; i < n; i++) {
                const char* lab = (options[i].label && *options[i].label) ? options[i].label : options[i].id;
                char* e2 = NULL;
                if (!lab) continue;
                double v = strtod(lab, &e2);
                if (e2 && e2 != lab && *e2 == 0 && fabs(v - num) < 1e-9) return i;
            }
        }
    }
    int best = -1;
    size_t best_len = 0;
    for (int i = 0; i < n; i++) {
        const char* lab = (options[i].label && *options[i].label) ? options[i].label : options[i].id;
        if (!lab) continue;
        if (dc_match_word_contains(t, lab) || (options[i].id && dc_match_word_contains(t, options[i].id))) {
            size_t l = strlen(lab);
            if (l > best_len) { best = i; best_len = l; }
        }
    }
    if (best >= 0) return best;
    char first = t[0];
    for (int i = 0; i < n; i++)
        if (options[i].id && options[i].id[1] == 0 &&
            toupper((unsigned char)options[i].id[0]) == toupper((unsigned char)first))
            return i;
    return -1;
}

/* Risolve il dominio del kind in una struttura STABILE (label/custom inclusi). Una sola
 * grammatica per dispatcher, group e provider locale; `d` deve vivere quanto il dominio. */
int gmb_decision_resolve_domain(const char* kind, gmb_decision_domain_t* d) {
    if (!kind || !d) return -1;
    d->kind = GMB_DECISION_CHOICE;
    d->options = NULL;
    d->n_options = 0;
    d->labelbuf[0] = 0;
    for (int i = 0; i < 11; i++) d->score_labels[i][0] = 0;
    memset(d->custom, 0, sizeof d->custom);

    /* CHOICE:<label1>|<label2>|... — dominio libero mappato agli slot stabili A..P. */
    if (strncasecmp(kind, "CHOICE:", 7) == 0) {
        size_t len = strlen(kind + 7);
        if (len == 0 || len >= sizeof(d->labelbuf)) return -1;
        memcpy(d->labelbuf, kind + 7, len + 1);
        char* p = d->labelbuf;
        int n = 0;
        while (*p && n < GMB_DECISION_MAX_OPTIONS) {
            char* sep = strchr(p, '|');
            if (sep) *sep = 0;
            while (*p == ' ' || *p == '\t') p++;
            char* end = p + strlen(p);
            while (end > p && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
            if (*p) {
                d->custom[n].id = gmb_decision_slots_choice[n].id;
                d->custom[n].label = p;
                d->custom[n].token_id = -1;
                d->custom[n].bias = 0.0f;
                d->custom[n].temperature = 1.0f;
                n++;
            }
            if (!sep) break;
            p = sep + 1;
        }
        if (n == 0) return -1;
        d->options = d->custom;
        d->n_options = n;
        d->kind = GMB_DECISION_CHOICE;
        return 0;
    }
    if (ieq(kind, "NOUL")) {
        d->kind = GMB_DECISION_NOUL;
        d->options = gmb_decision_slots_noul;
        d->n_options = 2;
        return 0;
    }
    if (strncasecmp(kind, "SCORE:", 6) == 0) {
        /* SCORE:0:10 — rubrica linguistica (11 livelli); SCORE:0:5 = canonica. */
        int lo = -1, hi = -1;
        if (sscanf(kind + 6, "%d:%d", &lo, &hi) != 2 || lo != 0 || (hi != 5 && hi != 10)) return -1;
        for (int i = 0; i <= hi; i++) {
            snprintf(d->score_labels[i], sizeof d->score_labels[i], "%d", i);
            d->custom[i].id = d->score_labels[i];
            d->custom[i].label = d->score_labels[i];
            d->custom[i].token_id = -1;
            d->custom[i].bias = 0.0f;
            d->custom[i].temperature = 1.0f;
        }
        d->kind = GMB_DECISION_SCORE;
        d->options = d->custom;
        d->n_options = hi + 1;
        return 0;
    }
    if (ieq(kind, "SCORE")) {
        d->kind = GMB_DECISION_SCORE;
        d->options = gmb_decision_slots_score;
        d->n_options = 6;
        return 0;
    }
    if (ieq(kind, "CHOICE")) {
        d->kind = GMB_DECISION_CHOICE;
        d->options = gmb_decision_slots_choice;
        d->n_options = 16;
        return 0;
    }
    return -1;
}

static char* dc_run_json_inner(const char* state, const char* kind, const char* provider_name) {
    if (!kind) return NULL;
    /* Provider esplicito assente → ruolo %DECIDE attivo, se registrato/attivato. */
    const char* effective_provider = provider_name;
    if (!effective_provider || !*effective_provider) {
        const char* decide_kind = gmb_provider_active_kind("decide");
        if (decide_kind && *decide_kind) effective_provider = decide_kind;
    }
    g_decide_profile_on = effective_provider && ieq(effective_provider, "verbalizer");
    /* RANK ("RANK:c1|c2|…"): best-of-N su candidati liberi, solo col provider clm. */
    if (strncasecmp(kind, "RANK:", 5) == 0) {
        if (effective_provider && ieq(effective_provider, "clm"))
            return dc_rank_run_json(state ? state : "", kind, gmb_provider_active_config("decide"));
        return strdup("{\"error\":\"RANK richiede il provider clm\"}");
    }
    gmb_decision_domain_t dom;
    if (gmb_decision_resolve_domain(kind, &dom) != 0) return NULL;
    const gmb_decision_option_t* options = dom.options;
    int n_options = dom.n_options;
    gmb_decision_kind_t k = dom.kind;

    /* Provider REMOTO TypeSafe (JEV). */
    if (effective_provider && ieq(effective_provider, "typesafe")) {
        gmb_decision_result_t ts;
        char terr[GMB_DECISION_ERR_CAP] = "";
        if (dc_typesafe_run(state ? state : "", kind, options, n_options, &ts, terr, sizeof terr) != 1) {
            char esc[GMB_DECISION_ERR_CAP + 64];
            json_escape(terr[0] ? terr : "typesafe: errore", esc, sizeof esc);
            char ebuf[GMB_DECISION_ERR_CAP + 96];
            int en = snprintf(ebuf, sizeof ebuf, "{\"error\":\"%s\"}", esc);
            if (en < 0) return NULL;
            char* eout = (char*)malloc((size_t)en + 1);
            if (!eout) return NULL;
            memcpy(eout, ebuf, (size_t)en + 1);
            return eout;
        }
        char tbuf[1024];
        if (gmb_decision_result_json(&ts, tbuf, sizeof tbuf) < 0) return NULL;
        size_t tn = strlen(tbuf);
        char* tout = (char*)malloc(tn + 1);
        if (!tout) return NULL;
        memcpy(tout, tbuf, tn + 1);
        return tout;
    }

    /* Provider `clm` (contratto SystemOne, wire /v1/systemone). Errori JSON espliciti. */
    if (effective_provider && ieq(effective_provider, "clm")) {
        gmb_decision_result_t cr;
        char cerr[GMB_DECISION_ERR_CAP] = "";
        if (dc_clm_run_cfg(gmb_provider_active_config("decide"), state ? state : "", kind, options, n_options, k, &cr, cerr, sizeof cerr) != 1) {
            char esc[GMB_DECISION_ERR_CAP + 64];
            json_escape(cerr[0] ? cerr : "clm: errore", esc, sizeof esc);
            char ebuf[GMB_DECISION_ERR_CAP + 96];
            int en = snprintf(ebuf, sizeof ebuf, "{\"error\":\"%s\"}", esc);
            if (en < 0) return NULL;
            char* eout = (char*)malloc((size_t)en + 1);
            if (!eout) return NULL;
            memcpy(eout, ebuf, (size_t)en + 1);
            return eout;
        }
        char cbuf[1024];
        if (gmb_decision_result_json(&cr, cbuf, sizeof cbuf) < 0) return NULL;
        size_t cn = strlen(cbuf);
        char* cout = (char*)malloc(cn + 1);
        if (!cout) return NULL;
        memcpy(cout, cbuf, cn + 1);
        return cout;
    }

    /* P6 — provider ADAPTER (generativo): il modello locale GENERA la risposta e il testo
     * viene riconosciuto come opzione. Esplicito: nessun match = errore JSON, mai un fallback. */
    if (effective_provider && (ieq(effective_provider, "adapter") || ieq(effective_provider, "llm"))) {
        gmb_decision_result_t adapter_result;
        char aerr[GMB_DECISION_ERR_CAP] = "";
        int ar = dc_adapter_remote_run(state ? state : "", kind, options, n_options,
                                       &adapter_result, aerr, sizeof aerr);
        if (ar == 0)
            ar = gmb_ai_decide_local_adapter_run(state ? state : "", kind, options, n_options,
                                                &adapter_result, aerr, sizeof aerr);
        if (ar != 1) {
            char esc[GMB_DECISION_ERR_CAP + 64];
            json_escape(aerr[0] ? aerr : "adapter non disponibile", esc, sizeof esc);
            char ebuf[GMB_DECISION_ERR_CAP + 96];
            int en = snprintf(ebuf, sizeof ebuf, "{\"error\":\"adapter: %s\"}", esc);
            if (en < 0) return NULL;
            char* eout = (char*)malloc((size_t)en + 1);
            if (!eout) return NULL;
            memcpy(eout, ebuf, (size_t)en + 1);
            return eout;
        }
        char abuffer[1024];
        if (gmb_decision_result_json(&adapter_result, abuffer, sizeof abuffer) < 0) return NULL;
        size_t an = strlen(abuffer);
        char* aout = (char*)malloc(an + 1);
        if (!aout) return NULL;
        memcpy(aout, abuffer, an + 1);
        return aout;
    }

    /* Provider locale MLX: l'inferenza vive nella dylib AI (`gmb_ai_decide_local_run`),
     * che usa il motore LLMEnv (CAP_DECIDE), il tokenizer e il chat_template reale. Un
     * fallimento è un errore JSON ESPLICITO: mai un fallback silenzioso a rules. */
    if (effective_provider && (ieq(effective_provider, "local") || ieq(effective_provider, "verbalizer"))) {
        gmb_decision_result_t local_result;
        char lerr[GMB_DECISION_ERR_CAP] = "";
        if (gmb_ai_decide_local_run(state ? state : "", kind, options, n_options,
                                    &local_result, lerr, sizeof lerr) != 1) {
            char esc[GMB_DECISION_ERR_CAP + 64];
            json_escape(lerr[0] ? lerr : "local provider non disponibile", esc, sizeof esc);
            char ebuf[GMB_DECISION_ERR_CAP + 96];
            int en = snprintf(ebuf, sizeof ebuf, "{\"error\":\"local: %s\"}", esc);
            if (en < 0) return NULL;
            char* eout = (char*)malloc((size_t)en + 1);
            if (!eout) return NULL;
            memcpy(eout, ebuf, (size_t)en + 1);
            return eout;
        }
        char lbuffer[1024];
        if (gmb_decision_result_json(&local_result, lbuffer, sizeof lbuffer) < 0) return NULL;
        size_t ln = strlen(lbuffer);
        char* lout = (char*)malloc(ln + 1);
        if (!lout) return NULL;
        memcpy(lout, lbuffer, ln + 1);
        return lout;
    }
    gmb_decision_provider_t* provider = NULL;
    if (!effective_provider || !effective_provider[0] || ieq(effective_provider, "rules"))
        provider = gmb_decision_provider_rules();
    else if (ieq(effective_provider, "mock"))
        provider = gmb_decision_provider_mock(NULL, 1.0);
    if (!provider) {
        char esc[256];
        json_escape(effective_provider ? effective_provider : "", esc, sizeof esc);
        char ebuf[512];
        int en = snprintf(ebuf, sizeof ebuf,
                          "{\"error\":\"provider '%s' non valido (rules/mock/local)\"}", esc);
        if (en < 0) return NULL;
        char* eout = (char*)malloc((size_t)en + 1);
        if (!eout) return NULL;
        memcpy(eout, ebuf, (size_t)en + 1);
        return eout;
    }
    gmb_decision_result_t result;
    int rc = gmb_decision_evaluate(provider, state ? state : "", k, options, n_options, &result);
    gmb_decision_provider_free(provider);
    if (rc != 1) return NULL;

    char buffer[1024];
    if (gmb_decision_result_json(&result, buffer, sizeof(buffer)) < 0) return NULL;
    size_t n = strlen(buffer);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, buffer, n + 1);
    return out;
}

/* Provider `%DECIDE` registrato con nome proprio (`AI REGISTER "jev", %DECIDE, "verbalizer", {...}`):
 * `AI DECIDE ... USING "jev"` ne usa il kind e, per i kind con profilo (verbalizer), il provider
 * locale legge decide/decideTemplate/decideCalibration dalla sua config: l'alias viene reso attivo
 * per la durata della chiamata e il precedente ripristinato. */
typedef struct { char prev[64]; int swapped; } dc_alias_t;

static int dc_is_builtin_kind(const char* p) {
    return ieq(p, "rules") || ieq(p, "mock") || ieq(p, "local") || ieq(p, "typesafe")
        || ieq(p, "adapter") || ieq(p, "llm") || ieq(p, "verbalizer") || ieq(p, "clm");
}

/* Ritorna il provider effettivo da passare alla logica interna (kind dell'alias, o l'input). */
static const char* dc_alias_enter(const char* provider, dc_alias_t* a) {
    a->prev[0] = 0; a->swapped = 0;
    if (!provider || !*provider || dc_is_builtin_kind(provider)) return provider;
    const char* role = gmb_provider_role_name(provider);
    if (!role || strcmp(role, "decide") != 0) return provider;
    const char* cur = gmb_provider_active("decide");
    snprintf(a->prev, sizeof a->prev, "%s", cur ? cur : "");
    if (gmb_provider_set_active("decide", provider)) a->swapped = 1;
    const char* k = gmb_provider_kind(provider);
    return (k && *k) ? k : provider;
}

static void dc_alias_leave(dc_alias_t* a) {
    if (a->swapped && a->prev[0]) gmb_provider_set_active("decide", a->prev);
}

char* gmb_ai_decide_run_json(const char* state, const char* kind, const char* provider_name) {
    dc_alias_t al;
    const char* eff = dc_alias_enter(provider_name, &al);
    char* r = dc_run_json_inner(state, kind, eff);
    g_decide_profile_on = 0;
    dc_alias_leave(&al);
    return r;
}

/* Catena esplicita: il primo provider che non risponde `{"error":…}` vince. */
static int dc_chain_is_error(const char* r) {
    return r && strncmp(r, "{\"error\"", 8) == 0;
}

char* gmb_ai_decide_run_json_chain(const char* state, const char* kind,
                                   const char* provider, const char* fallback_csv) {
    const char* parts[16];
    char* owned[16] = { 0 };
    int n = 0;
    if (provider && *provider) parts[n++] = provider;
    if (fallback_csv && *fallback_csv) {
        char* copy = strdup(fallback_csv);
        if (!copy) return NULL;
        char* p = copy;
        while (*p && n < 16) {
            char* sep = strchr(p, '|');
            if (sep) *sep = 0;
            while (*p == ' ' || *p == '\t') p++;
            char* e = p + strlen(p);
            while (e > p && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
            if (*p) { owned[n] = strdup(p); parts[n] = owned[n]; n++; }
            if (!sep) break;
            p = sep + 1;
        }
        free(copy);
    }
    if (n == 0) parts[n++] = "rules";

    char errbuf[2048]; size_t used = 0; errbuf[0] = 0;
    for (int i = 0; i < n; i++) {
        char* r = gmb_ai_decide_run_json(state, kind, parts[i]);
        if (r && !dc_chain_is_error(r)) {
            for (int k = 0; k < n; k++) free(owned[k]);
            return r;
        }
        const char* txt = r ? r : "errore";
        size_t tl = strlen(txt);
        const char* body = txt; size_t bl = tl;
        if (r && tl >= 12 && strncmp(txt, "{\"error\":\"", 10) == 0
            && txt[tl - 2] == '"' && txt[tl - 1] == '}') {
            body = txt + 10; bl = tl - 12;
        }
        if (used + bl + strlen(parts[i]) + 8 < sizeof errbuf) {
            if (used) { memcpy(errbuf + used, " | ", 3); used += 3; }
            used += (size_t)snprintf(errbuf + used, sizeof errbuf - used,
                                     "%s: %.*s", parts[i], (int)bl, body);
        }
        free(r);
    }
    for (int k = 0; k < n; k++) free(owned[k]);

    char esc[2300];
    json_escape(errbuf[0] ? errbuf : "nessun provider", esc, sizeof esc);
    char out[2400];
    int len = snprintf(out, sizeof out, "{\"error\":\"tutti i provider falliti: %s\"}", esc);
    if (len < 0) return NULL;
    char* res = (char*)malloc((size_t)len + 1);
    if (!res) return NULL;
    memcpy(res, out, (size_t)len + 1);
    return res;
}

/* ── Batch (desugare della forma AI DECIDE … END DECIDE): {nome:"<json risultato>"} ──
 * `names_json`/`kinds_json` sono array JSON di stringhe (le label CHOICE possono contenere
 * '|', quindi NON si usa un CSV). v1: valuta le domande in sequenza; il provider locale può
 * condividere il prefisso in una versione successiva. */
static int dc_sb_reserve(char** buf, size_t* cap, size_t need) {
    if (*cap >= need) return 1;
    size_t nc = *cap ? *cap : 512;
    while (nc < need) nc *= 2;
    char* nb = (char*)realloc(*buf, nc);
    if (!nb) return 0;
    *buf = nb; *cap = nc; return 1;
}

static int dc_sb_add(char** buf, size_t* cap, size_t* off, const char* s, size_t len) {
    if (!dc_sb_reserve(buf, cap, *off + len + 1)) return 0;
    memcpy(*buf + *off, s, len);
    *off += len; (*buf)[*off] = 0;
    return 1;
}

static char* dc_group_build_inner(const char* state, const char* names_json, const char* kinds_json,
                            const char* provider, const char* fallback) {
    gmb_val_t nv = gmb_json_parse(names_json ? names_json : "[]");
    gmb_val_t kv = gmb_json_parse(kinds_json ? kinds_json : "[]");
    gmb_list_t nl = gmb_val_as_list(nv);
    gmb_list_t kl = gmb_val_as_list(kv);
    int n = (int)gmb_list_count(nl);
    if (n < 1 || (int)gmb_list_count(kl) != n) {
        const char* e = "{\"error\":\"AiDecideGroup: names/kinds disallineati\"}";
        return strdup(e);
    }

    gmb_string_t* names = (gmb_string_t*)calloc((size_t)n, sizeof(gmb_string_t));
    gmb_string_t* kinds = (gmb_string_t*)calloc((size_t)n, sizeof(gmb_string_t));
    if (!names || !kinds) { free(names); free(kinds); return NULL; }
    for (int i = 0; i < n; i++) {
        names[i] = gmb_val_to_string(gmb_list_get(nl, i));
        kinds[i] = gmb_val_to_string(gmb_list_get(kl, i));
    }

    /* FUSIONE C: con provider locale e nessuna catena di fallback, risolve TUTTI i domini e
     * delega a `gmb_ai_decide_local_run_group`, che condivide il prefill del prefisso comune
     * (stessa semantica del percorso managed). Se la dylib AI non espone l'entry point, si
     * ricade sul loop sequenziale qui sotto (stessa semantica, nessun errore mascherato). */
    const char* eff = provider;
    if (!eff || !*eff) {
        const char* dk = gmb_provider_active_kind("decide");
        if (dk && *dk) eff = dk;
    }
    g_decide_profile_on = eff && ieq(eff, "verbalizer");
    const char* group_off = getenv("GMB_DECIDE_GROUP_OFF");
    int allow_group = !(group_off && *group_off && strcmp(group_off, "0") != 0);
    if (allow_group && (!fallback || !*fallback) && eff && (ieq(eff, "local") || ieq(eff, "verbalizer")) && n <= 32) {
        gmb_decision_domain_t* doms = (gmb_decision_domain_t*)calloc((size_t)n, sizeof(*doms));
        gmb_decision_result_t* res = (gmb_decision_result_t*)calloc((size_t)n, sizeof(*res));
        int okall = doms && res;
        for (int i = 0; i < n && okall; i++)
            if (gmb_decision_resolve_domain(kinds[i].data ? kinds[i].data : "", &doms[i]) != 0)
                okall = 0;
        if (okall) {
            char gerr[GMB_DECISION_ERR_CAP] = "";
            if (gmb_ai_decide_local_run_group(state ? state : "", doms, n, res,
                                              gerr, sizeof gerr) == 1) {
                char* out = NULL; size_t cap = 0, off = 0;
                int good = dc_sb_add(&out, &cap, &off, "{", 1);
                for (int i = 0; i < n && good; i++) {
                    char buf[1024];
                    if (gmb_decision_result_json(&res[i], buf, sizeof buf) < 0) { good = 0; break; }
                    char esc[2048];
                    json_escape(names[i].data ? names[i].data : "", esc, sizeof esc);
                    good = (i == 0 || dc_sb_add(&out, &cap, &off, ",", 1))
                        && dc_sb_add(&out, &cap, &off, "\"", 1)
                        && dc_sb_add(&out, &cap, &off, esc, strlen(esc))
                        && dc_sb_add(&out, &cap, &off, "\":\"", 3);
                    if (good) {
                        json_escape(buf, esc, sizeof esc);
                        good = dc_sb_add(&out, &cap, &off, esc, strlen(esc))
                            && dc_sb_add(&out, &cap, &off, "\"", 1);
                    }
                }
                if (good && dc_sb_add(&out, &cap, &off, "}", 1)) {
                    free(doms); free(res);
                    for (int i = 0; i < n; i++) { gmb_string_free(&names[i]); gmb_string_free(&kinds[i]); }
                    free(names); free(kinds);
                    return out;
                }
                free(out);
            }
        }
        free(doms); free(res);
    }

    /* Percorso sequenziale (provider non locale, fallback esplicito o group non disponibile). */
    char* out = NULL; size_t cap = 0, off = 0;
    if (!dc_sb_add(&out, &cap, &off, "{", 1)) { free(names); free(kinds); return NULL; }
    for (int i = 0; i < n; i++) {
        char esc[2048];
        if (i > 0 && !dc_sb_add(&out, &cap, &off, ",", 1)) { free(out); return NULL; }
        if (!dc_sb_add(&out, &cap, &off, "\"", 1)) { free(out); return NULL; }
        json_escape(names[i].data ? names[i].data : "", esc, sizeof esc);
        if (!dc_sb_add(&out, &cap, &off, esc, strlen(esc)) ||
            !dc_sb_add(&out, &cap, &off, "\":\"", 3)) { free(out); return NULL; }
        char* r = (fallback && *fallback)
            ? gmb_ai_decide_run_json_chain(state ? state : "", kinds[i].data ? kinds[i].data : "",
                                           provider ? provider : "", fallback)
            : gmb_ai_decide_run_json(state ? state : "", kinds[i].data ? kinds[i].data : "",
                                     provider ? provider : "");
        if (!r) r = strdup("{\"error\":\"decisione fallita\"}");
        json_escape(r, esc, sizeof esc);
        if (!dc_sb_add(&out, &cap, &off, esc, strlen(esc)) ||
            !dc_sb_add(&out, &cap, &off, "\"", 1)) { free(r); free(out); return NULL; }
        free(r);
    }
    if (!dc_sb_add(&out, &cap, &off, "}", 1)) { free(out); return NULL; }
    for (int i = 0; i < n; i++) { gmb_string_free(&names[i]); gmb_string_free(&kinds[i]); }
    free(names); free(kinds);
    return out;
}

static char* dc_group_build(const char* state, const char* names_json, const char* kinds_json,
                            const char* provider, const char* fallback) {
    dc_alias_t al;
    const char* eff = dc_alias_enter(provider, &al);
    char* r = dc_group_build_inner(state, names_json, kinds_json, eff, fallback);
    g_decide_profile_on = 0;
    dc_alias_leave(&al);
    return r;
}

/* Il desugar del batch emette UNA chiamata per ogni destinazione (EXTRACT dal gruppo): senza
 * cache il gruppo verrebbe rieseguito N volte — con un provider generativo (adapter/LLM) i
 * target vedrebbero generazioni DIVERSE, oltre al costo N×. Cache thread-local dell'ultima
 * richiesta identica (stessa forma del provider managed). */
static __thread char* g_group_key;
static __thread char* g_group_result;

static char* dc_group_cache_get(const char* key) {
    if (g_group_key && g_group_result && strcmp(g_group_key, key) == 0)
        return strdup(g_group_result);
    return NULL;
}
static void dc_group_cache_put(const char* key, const char* result) {
    free(g_group_key); g_group_key = strdup(key ? key : "");
    free(g_group_result); g_group_result = strdup(result ? result : "");
}

char* gmb_ai_decide_group_json(const char* state, const char* names_json, const char* kinds_json,
                               const char* provider, const char* fallback) {
    const char* s = state ? state : "";
    const char* nj = names_json ? names_json : "";
    const char* kj = kinds_json ? kinds_json : "";
    const char* pr = provider ? provider : "";
    const char* fb = fallback ? fallback : "";
    size_t klen = strlen(s) + strlen(nj) + strlen(kj) + strlen(pr) + strlen(fb) + 8;
    char* key = (char*)malloc(klen);
    if (!key) return dc_group_build(state, names_json, kinds_json, provider, fallback);
    snprintf(key, klen, "%s\x1f%s\x1f%s\x1f%s\x1f%s", s, nj, kj, pr, fb);
    char* cached = dc_group_cache_get(key);
    if (cached) { free(key); return cached; }
    char* result = dc_group_build(state, names_json, kinds_json, provider, fallback);
    if (result) dc_group_cache_put(key, result);
    free(key);
    return result;
}

/* ── Diagnostica `AI DECIDE VERIFY`: stessa richiesta su più provider, report accordo/drift ── */
char* gmb_ai_decide_verify_json(const char* state, const char* kind, const char* providers_csv) {
    const char* csv = (providers_csv && *providers_csv) ? providers_csv : "rules|mock";
    char* copy = strdup(csv);
    if (!copy) return NULL;
    char* out = NULL; size_t cap = 0, off = 0;
    if (!dc_sb_add(&out, &cap, &off, "{\"ok\":", 6)) { free(copy); return NULL; }

    char* opts[16]; double probs[16]; int nopts = 0, nprobs = 0, ok = 1, first = 1;
    char* results = NULL; size_t rcap = 0, roff = 0;
    if (!dc_sb_add(&results, &rcap, &roff, "[", 1)) { free(copy); free(out); return NULL; }
    char* p = copy;
    while (*p) {
        char* sep = strchr(p, '|');
        if (sep) *sep = 0;
        while (*p == ' ' || *p == '\t') p++;
        char* e = p + strlen(p);
        while (e > p && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        if (*p) {
            char escp[256]; json_escape(p, escp, sizeof escp);
            gmb_ai_decide_clm_stats_reset();
            char* r = gmb_ai_decide_run_json(state ? state : "", kind ? kind : "", p);
            char* cstats = gmb_ai_decide_clm_stats_json();      /* solo se il provider era clm */
            if (!first && !dc_sb_add(&results, &rcap, &roff, ",", 1)) { free(r); break; }
            first = 0;
            char head[512];
            int hn = snprintf(head, sizeof head, "{\"provider\":\"%s\",\"result\":", escp);
            if (hn < 0 || !dc_sb_add(&results, &rcap, &roff, head, (size_t)hn) ||
                !dc_sb_add(&results, &rcap, &roff, r ? r : "null", strlen(r ? r : "null")) ||
                (cstats && (!dc_sb_add(&results, &rcap, &roff, ",\"clm\":", 7) ||
                           !dc_sb_add(&results, &rcap, &roff, cstats, strlen(cstats)))) ||
                !dc_sb_add(&results, &rcap, &roff, "}", 1)) { free(r); free(cstats); break; }
            free(cstats);
            if (r) {
                char* opt = gmb_json_extract(r, "option");
                char* prob = gmb_json_extract(r, "probability");
                if (opt && *opt && prob && *prob) {
                    if (nopts < 16) opts[nopts++] = strdup(opt);
                    if (nprobs < 16) probs[nprobs++] = atof(prob);
                } else ok = 0;
                free(opt); free(prob);
            } else ok = 0;
            free(r);
        }
        if (!sep) break;
        p = sep + 1;
    }
    free(copy);
    if (!dc_sb_add(&results, &rcap, &roff, "]", 1)) { free(out); free(results); return NULL; }

    int distinct = 0;
    for (int i = 0; i < nopts; i++) {
        int seen = 0;
        for (int j = 0; j < i; j++) if (ieq(opts[i], opts[j])) { seen = 1; break; }
        if (!seen) distinct++;
    }
    int agreement = distinct <= 1;
    int drift = nopts == 0 ? 0 : distinct - 1;
    double spread = 0.0;
    if (nprobs > 0) {
        double mn = probs[0], mx = probs[0];
        for (int i = 1; i < nprobs; i++) { if (probs[i] < mn) mn = probs[i]; if (probs[i] > mx) mx = probs[i]; }
        spread = mx - mn;
    }
    for (int i = 0; i < nopts; i++) free(opts[i]);

    char tail[512];
    int tn = snprintf(tail, sizeof tail,
        ",\"agreement\":%s,\"argmaxDrift\":%d,\"probabilitySpread\":%.15g,\"results\":%s}",
        agreement ? "true" : "false", drift, spread, results);
    free(results);
    if (tn < 0 || !dc_sb_add(&out, &cap, &off, ok ? "true" : "false", ok ? 4 : 5) ||
        !dc_sb_add(&out, &cap, &off, tail, (size_t)tn)) { free(out); return NULL; }
    return out;
}

/* Entry per P/Invoke (IL/PCode → libgmb_clm): esegue `AI DECIDE` col provider `clm` con la config
 * passata ESPLICITAMENTE (JSON: baseUrl/apiKey/timeoutMs/instructions oppure head/encoder).
 * Ritorna lo stesso JSON del runtime C (`{"kind":…}` oppure `{"error":"…"}`), in heap (free con
 * gmb_ai_decide_free_json). NULL solo su memoria esaurita. */
char* gmb_ai_decide_clm_run_json(const char* state, const char* kind, const char* config_json)
{
    if (kind && strncasecmp(kind, "RANK:", 5) == 0) return dc_rank_run_json(state ? state : "", kind, config_json);
    char err[GMB_DECISION_ERR_CAP] = "";
    gmb_decision_domain_t dom;
    gmb_decision_result_t r;
    char buf[1024];
    if (!kind || gmb_decision_resolve_domain(kind, &dom) != 0) {
        snprintf(err, sizeof err, "clm: kind non valido");
    } else if (dc_clm_run_cfg(config_json, state ? state : "", kind, dom.options, dom.n_options,
                              dom.kind, &r, err, sizeof err) == 1) {
        if (gmb_decision_result_json(&r, buf, sizeof buf) < 0) return NULL;
        return strdup(buf);
    }
    char esc[GMB_DECISION_ERR_CAP + 64];
    json_escape(err[0] ? err : "clm: errore", esc, sizeof esc);
    int n = snprintf(buf, sizeof buf, "{\"error\":\"%s\"}", esc);
    return n < 0 ? NULL : strdup(buf);
}

void gmb_ai_decide_free_json(char* p) { free(p); }
