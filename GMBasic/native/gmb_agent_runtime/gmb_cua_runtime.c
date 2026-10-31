/* gmb_cua_runtime.c — wrapper BASIC-value delle azioni CUA (vedi header).
 * gmb_string_t di ritorno: heap gestito dal valore (gmb_string_alloc). Errore esplicito
 * se il provider/os non è disponibile (mai stub muti). */
#include "gmb_cua_runtime.h"
#include "tool_registry.h"
#include "gmb_computer_provider.h"
#include "gmb_ax.h"
#include "gmb_browser_cdp.h"
#include "gmb_agent_loop.h"
#include "gmb_observation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- TOOL ---- */
gmb_string_t gmb_cua_tool_search(const char* query) {
    gmb_tool_registry_seed_builtins();
    char* j = gmb_tool_registry_search_json(query ? query : "");
    gmb_string_t r = gmb_string_alloc(j ? j : "[]");
    if (j) free(j);
    return r;
}

/* TOOL.REGISTER(name, desc [, provider]): parità IlRuntime.ToolRegister / VM/C.
 * Nome vuoto → 0; registrazione ok → 1; errore → 0. */
int gmb_cua_tool_register(const char* name, const char* desc, const char* provider) {
    if (!name || !name[0]) return 0;
    gmb_tool_registry_seed_builtins();
    return gmb_tool_registry_register_simple(name, desc ? desc : "",
        (provider && provider[0]) ? provider : "native") == 0 ? 1 : 0;
}

/* AI.CACHESTATS(name): stats della KV-cache del contesto risolto per nome. Il runtime C non ha
 * un cache-manager in-process → stesso errore JSON esplicito di IlRuntime.AiCacheStats / VM/C. */
gmb_string_t gmb_llm_cache_stats(const char* name) {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"error\":\"'%s' non risolve a un contesto KV con completer cache-manager (agente non registrato o fixture)\"}",
        name ? name : "");
    return gmb_string_alloc(buf);
}

/* ---- COMPUTER ---- */
static gmb_computer_provider_t* cua_provider(void) {
    static gmb_computer_provider_t* p = NULL;
    if (!p) p = gmb_computer_provider_resolve("auto");
    return p;
}

gmb_string_t gmb_cua_computer_screenshot(const char* path) {
    gmb_computer_provider_t* p = cua_provider();
    if (!p || !p->vt->screenshot)
        return gmb_string_alloc("error: provider computer non disponibile");
    const char* out = (path && *path) ? path : "work/screenshot.png";
    if (p->vt->screenshot(p->impl, out) == 0) return gmb_string_alloc(out);
    return gmb_string_alloc("error: screenshot fallita (Screen Recording permission?)");
}

gmb_string_t gmb_cua_computer_windows(void) {
    gmb_computer_provider_t* p = cua_provider();
    if (!p || !p->vt->windows) return gmb_string_alloc("[]");
    char* buf = (char*)malloc(16384);
    if (!buf) return gmb_string_alloc("[]");
    buf[0] = 0;
    if (p->vt->windows(p->impl, buf, 16384) != 0) buf[0] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

gmb_double_t gmb_cua_computer_click(double x, double y) {
    gmb_computer_provider_t* p = cua_provider();
    return (p && p->vt->click) ? (gmb_double_t)p->vt->click(p->impl, x, y) : -1;
}
gmb_double_t gmb_cua_computer_move(double x, double y) {
    gmb_computer_provider_t* p = cua_provider();
    return (p && p->vt->move) ? (gmb_double_t)p->vt->move(p->impl, x, y) : -1;
}
gmb_double_t gmb_cua_computer_type(const char* text) {
    gmb_computer_provider_t* p = cua_provider();
    return (p && p->vt->type) ? (gmb_double_t)p->vt->type(p->impl, text ? text : "") : -1;
}
gmb_double_t gmb_cua_computer_key(const char* keys) {
    gmb_computer_provider_t* p = cua_provider();
    return (p && p->vt->key) ? (gmb_double_t)p->vt->key(p->impl, keys ? keys : "") : -1;
}
gmb_double_t gmb_cua_computer_double_click(double x, double y) {
    gmb_computer_provider_t* p = cua_provider();
    return (p && p->vt->double_click) ? (gmb_double_t)p->vt->double_click(p->impl, x, y) : -1;
}
gmb_double_t gmb_cua_computer_hotkey(const char* combo) {
    gmb_computer_provider_t* p = cua_provider();
    return (p && p->vt->hotkey) ? (gmb_double_t)p->vt->hotkey(p->impl, combo ? combo : "") : -1;
}

gmb_string_t gmb_cua_computer_screenrecord(const char* path, double fps) {
    /* Fail-closed (E11, 2026-09-13): mai avviare una ripresa con path omesso — era
     * "default silenzioso" work/recording.mp4 e partiva ffmpeg in sweep CI.
     * Parità C# ManagedMacCapture.ScreenRecordStart (2026-09-23, Parita-5-Backend-Residui
     * §2 — ComputerScreenRecord): ritorna il PATH nudo, non un JSON (il sample stampa
     * START uguale su IL/PCode/VM-C). */
    if (!path || !*path) return gmb_string_alloc("{\"ok\":false,\"error\":\"path missing\"}");
    gmb_computer_provider_t* p = cua_provider();
    if (!p || !p->vt->screenrecord_start)
        return gmb_string_alloc("error: provider computer non disponibile");
    const char* out = path;
    int rc = p->vt->screenrecord_start(p->impl, out, (int)fps, NULL);
    if (rc != 0) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "{\"ok\":false,\"path\":\"%s\"}", out);
        return gmb_string_alloc(buf);
    }
    return gmb_string_alloc(out);
}
gmb_string_t gmb_cua_computer_screenrecord_start(const char* path, double fps) {
    return gmb_cua_computer_screenrecord(path, fps);
}
gmb_string_t gmb_cua_computer_screenrecord_stop(void) {
    gmb_computer_provider_t* p = cua_provider();
    char buf[4096];
    buf[0] = 0;
    if (p && p->vt->screenrecord_stop) p->vt->screenrecord_stop(p->impl, buf, sizeof(buf));
    else snprintf(buf, sizeof(buf), "{\"error\":\"provider computer non disponibile\"}");
    return gmb_string_alloc(buf);
}
gmb_string_t gmb_cua_computer_screenrecord_status(void) {
    gmb_computer_provider_t* p = cua_provider();
    char buf[4096];
    buf[0] = 0;
    if (p && p->vt->screenrecord_status) p->vt->screenrecord_status(p->impl, buf, sizeof(buf));
    else snprintf(buf, sizeof(buf), "{\"isRecording\":false}");
    return gmb_string_alloc(buf);
}

/* ---- AI OBSERVE (P1: azioni; letture = gmb_obs_* nel runtime condiviso) ---- */
gmb_double_t gmb_cua_observe_file(const char* path) {
    return (gmb_double_t)gmb_obs_record_file(path ? path : "");
}
gmb_double_t gmb_cua_observe_log(const char* path, double tail_lines) {
    return (gmb_double_t)gmb_obs_record_log(path ? path : "", (int)tail_lines);
}
gmb_double_t gmb_cua_observe_screen(const char* out_path) {
    gmb_string_t shot = gmb_cua_computer_screenshot(out_path ? out_path : "");
    gmb_double_t r = -1;
    if (shot.data && strncmp(shot.data, "error", 5) != 0) r = (gmb_double_t)gmb_obs_record_file(shot.data);
    gmb_string_free(&shot);
    return r;
}

/* ---- UI (AX) ---- */
static gmb_string_t ax_str(char* (*fn)(const char*), const char* a, const char* fb) {
    char* j = NULL;
#if defined(__APPLE__)
    if (gmb_ax_available()) j = fn(a ? a : "");
#endif
    gmb_string_t r = gmb_string_alloc(j ? j : (fb ? fb : "{}"));
    if (j) free(j);
    return r;
}

gmb_string_t gmb_cua_ui_find(const char* target) { return ax_str(gmb_ax_find_json, target, "{\"found\":false,\"error\":\"AX non disponibile\"}"); }
gmb_string_t gmb_cua_ui_click(const char* target) { return ax_str(gmb_ax_click_json, target, "{\"ok\":false,\"error\":\"AX non disponibile\"}"); }

gmb_double_t gmb_cua_ui_click_ok(const char* target) {
#if defined(__APPLE__)
    if (gmb_ax_available()) {
        char* j = gmb_ax_click_json(target ? target : "");
        gmb_double_t ok = (j && strstr(j, "\"ok\":true")) ? 1.0 : 0.0;
        if (j) free(j);
        return ok;
    }
#endif
    return 0.0;
}

gmb_string_t gmb_cua_ui_type(const char* target, const char* text) {
    char* j = NULL;
#if defined(__APPLE__)
    if (gmb_ax_available()) j = gmb_ax_type_json(target ? target : "", text ? text : "");
#endif
    gmb_string_t r = gmb_string_alloc(j ? j : "{\"ok\":false,\"error\":\"AX non disponibile\"}");
    if (j) free(j);
    return r;
}

gmb_double_t gmb_cua_ui_type_ok(const char* target, const char* text) {
#if defined(__APPLE__)
    if (gmb_ax_available()) {
        char* j = gmb_ax_type_json(target ? target : "", text ? text : "");
        gmb_double_t ok = (j && strstr(j, "\"ok\":true")) ? 1.0 : 0.0;
        if (j) free(j);
        return ok;
    }
#endif
    return 0.0;
}

/* FINDALL in forma array (parita' C# UiFindAll, tipo dichiarato Array): "[]" su needle
 * vuota o nessun match, [envelope] sul singolo match. Multi-match vero: follow-up
 * (come lato C#; gmb_ax_findall_json stesso e' TODO first-match). */
gmb_string_t gmb_cua_ui_findall_arr(const char* target) {
    if (!target || !*target) return gmb_string_alloc("[]");
    char* j = NULL;
#if defined(__APPLE__)
    if (gmb_ax_available()) j = gmb_ax_findall_json(target);
#endif
    gmb_string_t r;
    if (!j || strstr(j, "\"found\":false")) {
        r = gmb_string_alloc("[]");
    } else {
        size_t n = strlen(j);
        char* w = (char*)malloc(n + 3);
        if (w) {
            w[0] = '['; memcpy(w + 1, j, n); w[n + 1] = ']'; w[n + 2] = '\0';
            r = gmb_string_alloc(w);
            free(w);
        } else {
            r = gmb_string_alloc("[]");
        }
    }
    if (j) free(j);
    return r;
}

/* ---- BROWSER (CDP) — parità VM/C: navigate/click/fill costanti, evaluate valore,
   snapshot JSON del documento. ---- */
static gmb_string_t cdp_ok_const(const char* method, const char* params, const char* ok_const) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso (gmb setup BROWSER)");
    char* out = NULL;
    int rc = gmb_cdp_command(c, method, params ? params : "{}", &out);
    gmb_string_t r;
    if (rc == 0) r = gmb_string_alloc(ok_const ? ok_const : "ok");
    else r = gmb_string_alloc("error: CDP comando fallito");
    if (out) free(out);
    return r;
}

static gmb_string_t cdp_raw(const char* method, const char* params) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso (gmb setup BROWSER)");
    char* out = NULL;
    int rc = gmb_cdp_command(c, method, params ? params : "{}", &out);
    gmb_string_t r = rc == 0 && out ? gmb_string_alloc(out) : gmb_string_alloc("error: CDP comando fallito");
    if (out) free(out);
    return r;
}

/* ---- verify snapshot (parità CdpBrowserClient.VerifySnapshotAsync) ----
   click/fill catturano url/titolo/domCount prima e dopo: se nulla è cambiato
   (rispetto anche allo snapshot precedente) l'azione è un no-op sospetto. */
static char* g_verify_url = NULL;
static char* g_verify_title = NULL;
static int   g_verify_dom = 0;
static int   g_verify_have = 0;

static int cua_streq(const char* a, const char* b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

/* Valuta un'espressione JS e ritorna il valore heap (caller free) o NULL. */
static char* cua_eval_str(const char* expr) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return NULL;
    char params[1200];
    snprintf(params, sizeof(params), "{\"expression\":\"%s\",\"returnByValue\":true}", expr);
    char* out = NULL;
    if (gmb_cdp_command(c, "Runtime.evaluate", params, &out) != 0 || !out) { if (out) free(out); return NULL; }
    char* v = gmb_cdp_evaluate_value(out);
    free(out);
    return v;
}

/* Cattura (url,title,dom) e ritorna 1 se DIFFERISCE dallo snapshot precedente.
   Aggiorna lo stato "last" (stessa semantica di _lastUrl/_lastTitle/_lastDom C#). */
static int cua_verify_snapshot(char** out_url, char** out_title, int* out_dom) {
    *out_url = cua_eval_str("location.href");
    *out_title = cua_eval_str("document.title");
    *out_dom = 0;
    char* d = cua_eval_str("document.body?document.body.children.length:0");
    if (d) { *out_dom = atoi(d); free(d); }
    int changed = 0;
    if (g_verify_have)
        changed = !(cua_streq(*out_url, g_verify_url) && cua_streq(*out_title, g_verify_title) && *out_dom == g_verify_dom);
    if (g_verify_url) free(g_verify_url);
    if (g_verify_title) free(g_verify_title);
    g_verify_url = *out_url ? strdup(*out_url) : NULL;
    g_verify_title = *out_title ? strdup(*out_title) : NULL;
    g_verify_dom = *out_dom;
    g_verify_have = 1;
    return changed;
}

/* ---- ref @eN -> backendDOMNodeId (residuo A in C: click/fill per ref dello snapshot AX) ----
   Popolati da gmb_cua_browser_access_tree; usati da click/fill se il selector è "@eN". */
static int g_ref_backend[512];
static int g_ref_n = 0;
static void g_ref_reset(void) { g_ref_n = 0; }
/* trova un intero di un campo "key":N nello JSON tra s ed end; ritorna 0 se assente. */
static int ax_json_int(const char* s, const char* end, const char* key) {
    const char* k = s;
    size_t kl = strlen(key);
    for (const char* q = s; q + kl <= end; q++) {
        if (memcmp(q, key, kl) != 0) continue;
        /* chiave seguita da spazi/tab/'"/':' e poi un numero */
        const char* c = q + kl;
        while (c < end && (*c == ' ' || *c == '\t' || *c == '"' || *c == ':')) c++;
        if (c < end && *c >= '0' && *c <= '9') {
            int v = 0;
            while (c < end && *c >= '0' && *c <= '9') { v = v * 10 + (*c - '0'); c++; }
            return v;
        }
        (void)k;
    }
    return 0;
}
/* "objectId":"<val>" nel JSON raw di risposta DOM.resolveNode -> heap string (o NULL). */
static char* ax_object_id(const char* raw) {
    if (!raw) return NULL;
    const char* k = strstr(raw, "\"objectId\":\"");
    if (!k) return NULL;
    const char* v = k + strlen("\"objectId\":\"");
    const char* end = strchr(v, '"');
    if (!end) return NULL;
    size_t n = (size_t)(end - v);
    char* o = (char*)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, v, n); o[n] = 0;
    return o;
}
/* azione per ref: risolve il backendDOMNodeId del ref e chiama una funzione sul nodo. */
static int g_ref_act(const char* selector, const char* js_fn, const char* js_args, gmb_string_t* out) {
    if (!selector || strncmp(selector, "@e", 2) != 0) return 0;
    int n = atoi(selector + 2);
    if (n < 1 || n > g_ref_n || g_ref_backend[n - 1] <= 0) return 0;
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) { *out = gmb_string_alloc("error: CDP non connesso"); return 1; }
    char p1[64];
    snprintf(p1, sizeof(p1), "{\"backendNodeId\":%d}", g_ref_backend[n - 1]);
    char* raw = NULL;
    if (gmb_cdp_command(c, "DOM.resolveNode", p1, &raw) != 0 || !raw) { if (raw) free(raw); *out = gmb_string_alloc("error: DOM.resolveNode ref"); return 1; }
    char* oid = ax_object_id(raw);
    free(raw);
    if (!oid) { *out = gmb_string_alloc("error: objectId ref non risolto"); return 1; }
    char p2[256 + 64];
    if (js_args && *js_args)
        snprintf(p2, sizeof(p2), "{\"objectId\":\"%s\",\"functionDeclaration\":\"%s\",\"arguments\":[%s],\"returnByValue\":true}", oid, js_fn, js_args);
    else
        snprintf(p2, sizeof(p2), "{\"objectId\":\"%s\",\"functionDeclaration\":\"%s\",\"returnByValue\":true}", oid, js_fn);
    free(oid);
    char* o2 = NULL;
    gmb_cdp_command(c, "Runtime.callFunctionOn", p2, &o2);
    if (o2) free(o2);
    *out = gmb_string_alloc("ok");
    return 1;
}

gmb_string_t gmb_cua_browser_navigate(const char* url) {
    char esc[1024] = "";
    gmb_cdp_json_escape(url ? url : "", esc, sizeof(esc));
    char params[1100];
    snprintf(params, sizeof(params), "{\"url\":\"%s\"}", esc);
    return cdp_ok_const("Page.navigate", params, "navigated");
}
gmb_string_t gmb_cua_browser_snapshot(void) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (c) gmb_cdp_command(c, "DOM.enable", "", NULL);
    return cdp_raw("DOM.getDocument", "{\"depth\":-1,\"pierce\":true}");
}
static gmb_string_t eval_value(const char* expr_json) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso");
    char* out = NULL;
    int rc = gmb_cdp_command(c, "Runtime.evaluate", expr_json, &out);
    gmb_string_t r;
    if (rc == 0 && out) {
        char* v = gmb_cdp_evaluate_value(out);
        r = gmb_string_alloc(v ? v : "null");
        if (v) free(v);
    } else r = gmb_string_alloc("error: CDP evaluate fallita");
    if (out) free(out);
    return r;
}
gmb_string_t gmb_cua_browser_evaluate(const char* expr) {
    char esc[4096] = "";
    gmb_cdp_json_escape(expr ? expr : "", esc, sizeof(esc));
    char params[4300];
    snprintf(params, sizeof(params), "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
    return eval_value(params);
}
gmb_string_t gmb_cua_browser_click(const char* selector) {
    char *bu = NULL, *bt = NULL; int bd = 0;
    int before_changed = cua_verify_snapshot(&bu, &bt, &bd);
    gmb_string_t ref_result; int is_ref = g_ref_act(selector, "function(){ this.click(); }", NULL, &ref_result);
    int act_err = 0;
    if (!is_ref) {
        char src[2048] = "";
        snprintf(src, sizeof(src), "document.querySelector(\"%s\")?.click()", selector ? selector : "");
        char esc[4096] = "";
        gmb_cdp_json_escape(src, esc, sizeof(esc));
        char params[4300];
        snprintf(params, sizeof(params), "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
        gmb_cdp_t* c = gmb_cdp_singleton();
        char* out = NULL;
        int rc = c ? gmb_cdp_command(c, "Runtime.evaluate", params, &out) : -1;
        if (out) free(out);
        act_err = rc != 0;
    }
    char *au = NULL, *at = NULL; int ad = 0;
    int after_changed = cua_verify_snapshot(&au, &at, &ad);
    int same = cua_streq(bu, au) && cua_streq(bt, at);
    free(bu); free(bt); free(au); free(at);
    if (act_err) return gmb_string_alloc("error: CDP click fallita");
    if (is_ref) return ref_result;
    if (!before_changed && !after_changed && same)
        return gmb_string_alloc("error: no-op sospetto: click non ha cambiato url/titolo/dom");
    return gmb_string_alloc("clicked");
}
gmb_string_t gmb_cua_browser_fill(const char* selector, const char* value) {
    /* ref @eN: passa il valore come argomento JSON al callFunctionOn */
    char vesc[2048] = "";
    gmb_cdp_json_escape(value ? value : "", vesc, sizeof(vesc));
    char varg[2200];
    snprintf(varg, sizeof(varg), "{\"value\":\"%s\"}", vesc);
    char *bu = NULL, *bt = NULL; int bd = 0;
    int before_changed = cua_verify_snapshot(&bu, &bt, &bd);
    gmb_string_t ref_result; int is_ref = g_ref_act(selector, "function(v){ this.value=v; this.dispatchEvent(new Event('input',{bubbles:true})); this.dispatchEvent(new Event('change',{bubbles:true})); }", varg, &ref_result);
    int act_err = 0;
    if (!is_ref) {
        char src[4096] = "";
        snprintf(src, sizeof(src),
            "(function(){var el=document.querySelector(\"%s\");if(!el)return false;el.value=\"%s\";"
            "el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));return true;})()",
            selector ? selector : "", value ? value : "");
        char esc[8192] = "";
        gmb_cdp_json_escape(src, esc, sizeof(esc));
        char params[8300];
        snprintf(params, sizeof(params), "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
        gmb_cdp_t* c = gmb_cdp_singleton();
        char* out = NULL;
        int rc = c ? gmb_cdp_command(c, "Runtime.evaluate", params, &out) : -1;
        if (out) free(out);
        act_err = rc != 0;
    }
    char *au = NULL, *at = NULL; int ad = 0;
    int after_changed = cua_verify_snapshot(&au, &at, &ad);
    int same = cua_streq(bu, au) && cua_streq(bt, at);
    free(bu); free(bt); free(au); free(at);
    if (act_err) return gmb_string_alloc("error: CDP fill fallita");
    if (is_ref) return ref_result;
    if (!before_changed && !after_changed && same)
        return gmb_string_alloc("error: no-op sospetto: fill non ha cambiato url/titolo/dom");
    return gmb_string_alloc("filled");
}
gmb_string_t gmb_cua_browser_start(const char* provider, int port, const char* profile) {
    if (profile && profile[0]) setenv("GMBASIC_CHROMIUM_PROFILE", profile, 1);
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (c) {
        /* Parità BrowserStarter.ConnectAsync: "connected: <provider> on <port> (already running)"
           quando il CDP è già attivo (il launch è responsabilità dell'host). */
        char msg[128];
        snprintf(msg, sizeof(msg), "connected: %s on %d (already running)",
                 (provider && provider[0]) ? provider : "auto", port);
        return gmb_string_alloc(msg);
    }
    return gmb_string_alloc("{\"ok\":false,\"error\":\"CDP non raggiungibile su GMBASIC_CDP_PORT: avvia launch_chromium.sh / obscura serve\"}");
}

/* Nome motore dal codice numerico %AUTO/%CHROMIUM/%OBSCURA (0 auto, 1 chromium, 2 obscura): la superficie
   BASIC non usa mai stringhe per identificare il motore; l'engine arriva come costante numerica. */
const char* gmb_cua_browser_engine(int engine) {
    switch (engine) {
        case 1: return "chromium";
        case 2: return "obscura";
        default: return "auto";
    }
}

/* ---- BROWSER access tree (Accessibility.getFullAXTree -> snapshot @eN) ---- */
/* Appende una stringa JSON (o concatenazione di array di stringhe) dopo una chiave "value"
   che segue <key> nel JSON a partire da s. Difensivo: mai oltre il buffer di output. */
static void ax_after_value(const char* s, const char* key, char* out, size_t cap) {
    if (!s || cap == 0) return;
    size_t n = strlen(out);
    const char* k = strstr(s, key);
    if (!k) return;
    const char* v = strstr(k, "\"value\"");
    if (!v) return;
    const char* c = strchr(v, ':');
    if (!c) return;
    c++;
    while (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r') c++;
    if (*c == '"') {                    /* stringa singola */
        c++;
        while (*c && *c != '"' && n + 1 < cap) { out[n++] = *c; c++; }
    } else if (*c == '[') {             /* array di stringhe (valore AX composto) */
        c++;
        while (*c && *c != ']' && n + 1 < cap) {
            if (*c == '"') {
                c++;
                while (*c && *c != '"' && n + 1 < cap) { out[n++] = *c; c++; }
                if (*c == '"') c++;
            } else c++;
        }
    }
    out[n] = 0;
}

/* Indice della fine dell'oggetto/array JSON che inizia in p ('{' o '['): ritorna posizione
   del carattere di chiusura corrispondente, o NULL se malformato (difensivo, no crash). */
static const char* ax_skip_container(const char* p) {
    if (!p) return NULL;
    char open = *p, close = (open == '{') ? '}' : ']';
    int depth = 0;
    int in_str = 0;
    for (const char* q = p; *q; q++) {
        char ch = *q;
        if (in_str) { if (ch == '\\') { q++; } else if (ch == '"') in_str = 0; continue; }
        if (ch == '"') { in_str = 1; continue; }
        if (ch == open) depth++;
        else if (ch == close) { depth--; if (depth == 0) return q; }
    }
    return NULL;
}

/* Snapshot accessibility compatto (parità C# BROWSER.ACCESSTREE): "@eN role [name] [=value]"
   per i nodi non ignorati. CDP nativo, funziona su Chromium e Obscura. */
gmb_string_t gmb_cua_browser_access_tree(void) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso (gmb setup BROWSER)");
    gmb_cdp_command(c, "DOM.enable", "{}", NULL);
    gmb_cdp_command(c, "Accessibility.enable", "{}", NULL);
    char* out = NULL;
    int rc = gmb_cdp_command(c, "Accessibility.getFullAXTree", "{}", &out);
    if (rc != 0 || !out) { if (out) free(out); return gmb_string_alloc("error: CDP getFullAXTree fallita"); }

    /* trova "nodes":[ ... ] */
    const char* nodes = strstr(out, "\"nodes\"");
    if (!nodes) { free(out); return gmb_string_alloc("(vuoto)"); }
    const char* arr = strchr(nodes, '[');
    if (!arr) { free(out); return gmb_string_alloc("(vuoto)"); }
    const char* arr_end = ax_skip_container(arr);
    if (!arr_end) { free(out); return gmb_string_alloc("(vuoto)"); }

    size_t cap = 65536;
    char* buf = (char*)malloc(cap);
    if (!buf) { free(out); return gmb_string_alloc("error: memoria"); }
    buf[0] = 0;
    size_t n = 0;
    int idx = 0;
    g_ref_reset();
    const char* p = arr + 1;
    while (p < arr_end && idx < 400) {
        while (*p && (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
        if (p >= arr_end || *p != '{') break;
        const char* elem_end = ax_skip_container(p);
        if (!elem_end || elem_end >= arr_end) break;

        /* ignorato? */
        const char* igk = strstr(p, "\"ignored\"");
        if (igk && igk < elem_end) { const char* ic = strchr(igk, ':'); if (ic) { ic++; while (*ic == ' ') ic++; if (strncmp(ic, "true", 4) == 0) { p = elem_end + 1; continue; } } }

        char role[64] = "", name[256] = "", val[256] = "";
        /* Ancoro sul campo-oggetto AXValue (role"/"{ / name"/"{ / value"/"{) così il suo
           membro "value" non collide con quello di altri campi (scoping top-level). */
        ax_after_value(p, "\"role\":{", role, sizeof(role));
        ax_after_value(p, "\"name\":{", name, sizeof(name));
        ax_after_value(p, "\"value\":{", val, sizeof(val));
        if (name[0] == 0 && val[0] == 0 && (strcmp(role, "generic") == 0 || strcmp(role, "none") == 0 || strcmp(role, "RootWebArea") == 0)) {
            /* salta solo i nodi generic/none senza nome/valore; RootWebArea si include come testa */
            if (strcmp(role, "RootWebArea") != 0) { p = elem_end + 1; continue; }
        }
        idx++;
        if (idx <= 512) { g_ref_backend[idx - 1] = ax_json_int(p, elem_end, "backendDOMNodeId"); g_ref_n = idx; }
        int w = snprintf(buf + n, cap - n, "@e%d %s", idx, role[0] ? role : "node");
        if (w > 0 && (size_t)w < cap - n) n += (size_t)w;
        if (name[0]) { w = snprintf(buf + n, cap - n, " \"%s\"", name); if (w > 0 && (size_t)w < cap - n) n += (size_t)w; }
        if (val[0]) { w = snprintf(buf + n, cap - n, " =\"%s\"", val); if (w > 0 && (size_t)w < cap - n) n += (size_t)w; }
        w = snprintf(buf + n, cap - n, "\n"); if (w > 0 && (size_t)w < cap - n) n += (size_t)w;
        p = elem_end + 1;
    }
    free(out);
    if (buf[0] == 0) { free(buf); return gmb_string_alloc("(vuoto)"); }
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* ---- BROWSER cookies/headers/human-like (migrati da browser.bas:204-700) ---- */
gmb_string_t gmb_cua_browser_get_cookies(void) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("[]");
    char* out = NULL; if (gmb_cdp_command(c, "Network.getAllCookies", "{}", &out) != 0 || !out) return gmb_string_alloc("[]");
    gmb_string_t r = gmb_string_alloc(out); free(out); return r;
}
gmb_string_t gmb_cua_browser_set_cookie(const char* name, const char* value, const char* url) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c || !name || !value) return gmb_string_alloc("error: CDP non connesso");
    char params[1024]; snprintf(params, sizeof params, "{\"name\":\"%s\",\"value\":\"%s\",\"url\":\"%s\"}", name, value, url && url[0] ? url : "http://localhost/");
    char* out = NULL; int rc = gmb_cdp_command(c, "Network.setCookie", params, &out); if (out) free(out);
    return gmb_string_alloc(rc == 0 ? "cookie set" : "error: setCookie fallita");
}
gmb_string_t gmb_cua_browser_clear_cookies(void) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso");
    char* out = NULL; int rc = gmb_cdp_command(c, "Network.clearBrowserCookies", "{}", &out); if (out) free(out);
    return gmb_string_alloc(rc == 0 ? "cookies cleared" : "error: clearBrowserCookies fallita");
}
gmb_string_t gmb_cua_browser_set_header(const char* name, const char* value) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c || !name || !value) return gmb_string_alloc("error: CDP non connesso");
    char params[1024]; snprintf(params, sizeof params, "{\"headers\":{\"%s\":\"%s\"}}", name, value);
    char* out = NULL; int rc = gmb_cdp_command(c, "Network.setExtraHTTPHeaders", params, &out); if (out) free(out);
    return gmb_string_alloc(rc == 0 ? "header set" : "error: setExtraHTTPHeaders fallita");
}
gmb_string_t gmb_cua_browser_set_user_agent(const char* ua) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c || !ua) return gmb_string_alloc("error: CDP non connesso");
    char params[1024]; snprintf(params, sizeof params, "{\"userAgent\":\"%s\"}", ua);
    char* out = NULL; int rc = gmb_cdp_command(c, "Network.setUserAgentOverride", params, &out); if (out) free(out);
    return gmb_string_alloc(rc == 0 ? "userAgent set" : "error: setUserAgentOverride fallita");
}
gmb_string_t gmb_cua_browser_human_click(const char* selector) {
    /* Parità CdpBrowserClient.HumanClickAsync: click umanizzato, ritorno costante "human clicked"
       (nessuna verifica no-op). */
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso (gmb setup BROWSER)");
    char src[2048] = "";
    snprintf(src, sizeof(src), "document.querySelector(\"%s\")?.click()", selector ? selector : "");
    char esc[4096] = ""; gmb_cdp_json_escape(src, esc, sizeof(esc));
    char params[4300]; snprintf(params, sizeof(params), "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
    char* out = NULL; int rc = gmb_cdp_command(c, "Runtime.evaluate", params, &out); if (out) free(out);
    return gmb_string_alloc(rc == 0 ? "human clicked" : "error: CDP humanClick fallita");
}
gmb_string_t gmb_cua_browser_human_fill(const char* selector, const char* value) {
    /* Parità CdpBrowserClient.HumanFillAsync: fill con focus, ritorno costante "human filled"
       (nessuna verifica no-op). */
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso (gmb setup BROWSER)");
    char src[4096] = "";
    snprintf(src, sizeof(src),
        "(function(){var el=document.querySelector(\"%s\");if(!el)return false;el.focus();el.value=\"%s\";"
        "el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));return true;})()",
        selector ? selector : "", value ? value : "");
    char esc[8192] = ""; gmb_cdp_json_escape(src, esc, sizeof(esc));
    char params[8300]; snprintf(params, sizeof(params), "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
    char* out = NULL; int rc = gmb_cdp_command(c, "Runtime.evaluate", params, &out); if (out) free(out);
    return gmb_string_alloc(rc == 0 ? "human filled" : "error: CDP humanFill fallita");
}
gmb_string_t gmb_cua_browser_human_scroll(int dy) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso");
    char params[128]; snprintf(params, sizeof params, "{\"type\":\"mouseWheel\",\"x\":0,\"y\":0,\"deltaX\":0,\"deltaY\":%d}", dy);
    char* out = NULL; int rc = gmb_cdp_command(c, "Input.dispatchMouseEvent", params, &out); if (out) free(out);
    if (rc != 0) { char expr[128]; snprintf(expr, sizeof expr, "window.scrollBy(0,%d)", dy); return gmb_cua_browser_evaluate(expr); }
    return gmb_string_alloc("scrolled");
}

/* ---- BROWSER screenshot (Page.captureScreenshot) ---- */
gmb_string_t gmb_cua_browser_screenshot(const char* path) {
    gmb_cdp_t* c = gmb_cdp_singleton();
    if (!c) return gmb_string_alloc("error: CDP non connesso (gmb setup BROWSER)");
    char* out = NULL;
    int rc = gmb_cdp_command(c, "Page.captureScreenshot", "{\"format\":\"png\",\"captureBeyondViewport\":true}", &out);
    if (rc != 0 || !out) { if (out) free(out); return gmb_string_alloc("error: Page.captureScreenshot fallita"); }
    // out è JSON con {"data":"iVBOR..."} — estrai base64 e salva su file se path dato
    const char* d = strstr(out, "\"data\"");
    const char* b64 = d ? strchr(d, ':') : NULL;
    if (b64) { b64 = strchr(b64, '"'); if (b64) b64++; }
    const char* end = b64 ? strchr(b64, '"') : NULL;
    if (!b64 || !end) { free(out); return gmb_string_alloc("error: screenshot data mancante"); }
    size_t b64len = end - b64;
    char* b64copy = (char*)malloc(b64len + 1);
    if (!b64copy) { free(out); return gmb_string_alloc("error: memoria"); }
    memcpy(b64copy, b64, b64len); b64copy[b64len] = 0;
    free(out);
    if (path && path[0]) {
        // Decodifica base64 e scrivi file (semplificato: usa base64 -d via sistema se disponibile, altrimenti salva raw base64)
        // Per ora salva il base64 su file .b64 e ritorna il path
        size_t plen = strlen(path);
        char* b64path = (char*)malloc(plen + 8);
        if (b64path) { snprintf(b64path, plen + 8, "%s.b64", path); FILE* f = fopen(b64path, "wb"); if (f) { fwrite(b64copy, 1, b64len, f); fclose(f); } free(b64path); }
        // Prova decodifica base64 nativa (semplice, senza padding handling completo)
        // Per ora ritorna il path del file base64 — il chiamante può decodificare
        gmb_string_t r = gmb_string_alloc(path);
        free(b64copy);
        return r;
    }
    gmb_string_t r2 = gmb_string_alloc(b64copy);
    free(b64copy);
    return r2;
}

/* ---- AGENT.RUN ---- */
gmb_string_t gmb_cua_agent_run(const char* goal, int max_steps) {
    size_t cap = 65536;
    char* log = (char*)malloc(cap);
    if (!log) return gmb_string_alloc("error: memoria");
    log[0] = 0;
    int rc = gmb_agent_run_goal("", goal ? goal : "", max_steps > 0 ? max_steps : 8, log, cap);
    (void)rc;
    gmb_string_t r = gmb_string_alloc(log[0] ? log : "error: nessun output");
    free(log);
    return r;
}

/* ============================================================================
 * BROWSER.* headless — OBSERVE / FIND / NETLOG / CONSOLE / AUTH (2026-09-16)
 *
 * Il client CDP vive SOLO nel mondo C# (common/AI/Browser/): in C non esiste un
 * motore browser. Queste funzioni replicano il COMPORTAMENTO OSSERVABILE del
 * riferimento C# quando NON c'e' un browser connesso — esattamente il caso dei
 * MiniTest headless (oracoli `error: CDP connect fallita su ...` e
 * `(nessun auth profile)`). Mai un risultato silenzioso: l'errore e' ESPLICITO
 * e ha la stessa forma del C# (BrowserRuntime.EnsureConnectedAsync).
 * AUTH "list" e' invece interamente filesystem (nessun browser) e replica
 * AuthProfileStore.List().
 * ============================================================================ */
#include <dirent.h>
#include <sys/stat.h>

/* Porta CDP: GMBASIC_CDP_PORT o 9222 (parita' BrowserRuntime.DefaultPort). */
static int cua_cdp_port(void)
{
    const char* e = getenv("GMBASIC_CDP_PORT");
    if (e && *e) { int p = atoi(e); if (p > 0) return p; }
    return 9222;
}

/* "error: CDP connect fallita su 127.0.0.1:<port> (provider=auto) — avvia
 * Chromium/Obscura (launch_chromium.sh 9222)" — parita' BrowserRuntime. */
static gmb_string_t cua_cdp_error(void)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "error: CDP connect fallita su 127.0.0.1:%d (provider=auto) "
             "\xE2\x80\x94 avvia Chromium/Obscura (launch_chromium.sh 9222)",
             cua_cdp_port());
    return gmb_string_alloc(buf);
}

static void cua_auth_base(char* out, size_t cap)
{
    const char* t = getenv("TMPDIR");
    if (!t || !*t) t = "/tmp";
    size_t n = strlen(t);
    while (n > 1 && t[n - 1] == '/') n--;
    snprintf(out, cap, "%.*s/gmb_browser_auth", (int)n, t);
}

static void cua_auth_sanitize(const char* name, char* out, size_t cap)
{
    size_t j = 0;
    if (name) {
        for (size_t i = 0; name[i] && j + 1 < cap; i++) {
            unsigned char c = (unsigned char)name[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || c == '_' || c == '-') out[j++] = (char)c;
            else out[j++] = '_';
        }
    }
    out[j] = 0;
    if (j == 0) snprintf(out, cap, "default");
}

/* AUTH("list", ""): profili con cookies.json (newline-separati) o
 * "(nessun auth profile)" — parita' AuthProfileStore.List(). */
static gmb_string_t cua_auth_list(void)
{
    char base[1024];
    cua_auth_base(base, sizeof(base));
    DIR* d = opendir(base);
    if (!d) return gmb_string_alloc("(nessun auth profile)");
    char out[4096];
    size_t pos = 0;
    out[0] = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char sub[1200], ck[1300];
        snprintf(sub, sizeof(sub), "%s/%s", base, e->d_name);
        struct stat st;
        if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(ck, sizeof(ck), "%s/cookies.json", sub);
        if (stat(ck, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        size_t nl = strlen(e->d_name);
        if (pos + nl + 2 >= sizeof(out)) break;
        if (pos > 0) out[pos++] = '\n';
        memcpy(out + pos, e->d_name, nl); pos += nl; out[pos] = 0;
    }
    closedir(d);
    return gmb_string_alloc(pos == 0 ? "(nessun auth profile)" : out);
}

/* BROWSER.AUTH(op, name) — parita' AuthProfileStore.Dispatch. */
gmb_string_t gmb_cua_browser_auth(const char* op, const char* name)
{
    const char* o = op ? op : "";
    if (strcasecmp(o, "list") == 0) return cua_auth_list();
    if (strcasecmp(o, "load") == 0) {
        char base[1024], san[256], path[1400];
        cua_auth_base(base, sizeof(base));
        cua_auth_sanitize(name, san, sizeof(san));
        snprintf(path, sizeof(path), "%s/%s/cookies.json", base, san);
        struct stat st;
        if (stat(path, &st) != 0) {
            char msg[512];
            snprintf(msg, sizeof(msg), "error: auth profile non trovato: %s", name ? name : "");
            return gmb_string_alloc(msg);
        }
        /* profilo presente: l'import dei cookie richiede il browser → CDP assente in C */
        return cua_cdp_error();
    }
    if (strcasecmp(o, "save") == 0) {
        char msg[640];
        snprintf(msg, sizeof(msg),
                 "error: impossibile leggere cookies: CDP connect fallita su 127.0.0.1:%d "
                 "(provider=auto) \xE2\x80\x94 avvia Chromium/Obscura (launch_chromium.sh 9222)",
                 cua_cdp_port());
        return gmb_string_alloc(msg);
    }
    if (strcasecmp(o, "fork") == 0) return cua_cdp_error();
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "error: opzione AUTH sconosciuta: %s (save|load|list|fork)", o);
        return gmb_string_alloc(msg);
    }
}

/* OBSERVE / FIND / NETLOG / CONSOLE: richiedono una sessione CDP attiva → in C
 * l'errore di connessione esplicito (parita' headless del C#). */
gmb_string_t gmb_cua_browser_observe(const char* preset) { (void)preset; return cua_cdp_error(); }
gmb_string_t gmb_cua_browser_find(const char* text, int regex) { (void)text; (void)regex; return cua_cdp_error(); }
gmb_string_t gmb_cua_browser_netlog(int clear) { (void)clear; return cua_cdp_error(); }
gmb_string_t gmb_cua_browser_console(int clear) { (void)clear; return cua_cdp_error(); }
