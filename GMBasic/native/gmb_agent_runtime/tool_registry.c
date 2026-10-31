/* tool_registry.c — implementazione ToolRegistry (vedi header) */
#include "tool_registry.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static gmb_tool_manifest_t g_tools[GMB_TOOL_MAX];
static int g_tool_count = 0;

static int str_case_contains(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return 1;
    size_t nlen = strlen(needle);
    for (const char *p = hay; *p; p++) {
        if (strncasecmp(p, needle, nlen) == 0) return 1;
    }
    return 0;
}

int gmb_tool_registry_register(const gmb_tool_manifest_t *m) {
    if (!m || !m->name[0]) return -1;
    for (int i = 0; i < g_tool_count; i++) {
        if (strcasecmp(g_tools[i].name, m->name) == 0) {
            g_tools[i] = *m;
            return 0;
        }
    }
    if (g_tool_count >= GMB_TOOL_MAX) return -1;
    g_tools[g_tool_count++] = *m;
    return 0;
}

int gmb_tool_registry_unregister(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < g_tool_count; i++) {
        if (strcasecmp(g_tools[i].name, name) == 0) {
            for (int j = i; j < g_tool_count - 1; j++) g_tools[j] = g_tools[j+1];
            g_tool_count--;
            return 0;
        }
    }
    return -1;
}

const gmb_tool_manifest_t *gmb_tool_registry_get(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_tool_count; i++) {
        if (strcasecmp(g_tools[i].name, name) == 0) return &g_tools[i];
    }
    return NULL;
}

int gmb_tool_registry_list(gmb_tool_manifest_t *out, int max) {
    if (!out || max <= 0) return 0;
    int n = g_tool_count < max ? g_tool_count : max;
    for (int i = 0; i < n; i++) out[i] = g_tools[i];
    return n;
}

int gmb_tool_registry_search(const char *query, gmb_tool_manifest_t *out, int max) {
    if (!query || !*query) return gmb_tool_registry_list(out, max);
    int n = 0;
    for (int i = 0; i < g_tool_count && n < max; i++) {
        if (str_case_contains(g_tools[i].name, query) || str_case_contains(g_tools[i].description, query)) {
            out[n++] = g_tools[i];
        }
    }
    return n;
}

int gmb_tool_registry_count(void) { return g_tool_count; }
void gmb_tool_registry_clear(void) { g_tool_count = 0; }

/* Executor esterno del registry (seam C puro) — vedi tool_registry.h. */
static gmb_tool_exec_fn g_tool_exec = NULL;
static void *g_tool_exec_ctx = NULL;
void gmb_tool_registry_set_executor(gmb_tool_exec_fn fn, void *ctx) { g_tool_exec = fn; g_tool_exec_ctx = ctx; }
int gmb_tool_registry_has_executor(void) { return g_tool_exec != NULL ? 1 : 0; }

int gmb_tool_registry_execute(const char *name, const char *args_json, char *out, size_t cap) {
    if (out && cap) out[0] = '\0';
    if (!name || !*name || !gmb_tool_registry_get(name)) return 0;
    if (!g_tool_exec) return -1;
    int rc = g_tool_exec(name, (args_json && *args_json) ? args_json : "{}", out, cap, g_tool_exec_ctx);
    if (rc != 0 && (!out || !out[0]))
        snprintf(out, cap, "error: esecuzione del tool '%s' fallita", name);
    return 1;
}

/* ---- wrapper JSON-friendly per VM/C (BuiltinFunc 1369/1370) ---- */
int gmb_tool_registry_register_simple(const char *name, const char *desc, const char *provider) {
    return gmb_tool_registry_register_simple_risk(name, desc, provider, 0);
}

/* Come register_simple ma con risk esplicito (0=LOW..3=CRITICAL): parità del seed
 * C# (ToolSearchNative) dove files.write/ui.click/ui.type sono "medium". */
int gmb_tool_registry_register_simple_risk(const char *name, const char *desc, const char *provider, int risk) {
    gmb_tool_manifest_t m; memset(&m, 0, sizeof(m));
    if (name) { strncpy(m.name, name, sizeof(m.name)-1); }
    if (desc) { strncpy(m.description, desc, sizeof(m.description)-1); }
    if (provider) { strncpy(m.provider, provider, sizeof(m.provider)-1); }
    else { strncpy(m.provider, "native", sizeof(m.provider)-1); }
    m.risk = risk;
    return gmb_tool_registry_register(&m);
}

/* Escape JSON con la convenzione di System.Text.Json (parità C#): '"' e '\\' con
 * backslash, i caratteri non-ASCII UTF-8 codificati come \uXXXX (surrogate pair per
 * codepoint > 0xFFFF). Senza questo, `ui.find` (descrizione con '→') diverge dal C#. */
static void json_escape_ascii(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)(in ? in : ""); *p && o + 1 < cap; ) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { if (o + 2 >= cap) break; out[o++] = '\\'; out[o++] = (char)c; p++; }
        else if (c < 0x20) { if (o + 6 >= cap) break; o += (size_t)snprintf(out + o, cap - o, "\\u%04x", c); p++; }
        else if (c < 0x80) { out[o++] = (char)c; p++; }
        else {
            unsigned int cp = 0; int len = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; len = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; len = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; len = 3; }
            else { p++; continue; }
            for (int k = 0; k < len; k++) { if ((p[1 + k] & 0xC0) != 0x80) { len = k; break; } cp = (cp << 6) | (p[1 + k] & 0x3Fu); }
            p += 1 + len;
            if (cp > 0xFFFF) {
                cp -= 0x10000;
                unsigned int hi = 0xD800u + (cp >> 10), lo = 0xDC00u + (cp & 0x3FFu);
                if (o + 12 >= cap) break;
                o += (size_t)snprintf(out + o, cap - o, "\\u%04x\\u%04x", hi, lo);
            } else {
                if (o + 6 >= cap) break;
                o += (size_t)snprintf(out + o, cap - o, "\\u%04x", cp);
            }
        }
    }
    out[o] = '\0';
}

char *gmb_tool_registry_search_json(const char *query) {
    gmb_tool_manifest_t out[64];
    int n = gmb_tool_registry_search(query, out, 64);
    size_t cap = 512 + (size_t)n * 640;
    char *buf = (char*)malloc(cap);
    if (!buf) return NULL;
    size_t p = 0;
    p += (size_t)snprintf(buf + p, cap - p, "[");
    static const char* risk_names[] = { "low", "medium", "high", "critical" };
    char en[GMB_TOOL_NAME_MAX * 6 + 4], ed[GMB_TOOL_DESC_MAX * 6 + 4], ep[GMB_TOOL_NAME_MAX * 6 + 4];
    for (int i = 0; i < n; i++) {
        const char* risk = (out[i].risk >= 0 && out[i].risk < 4) ? risk_names[out[i].risk] : "low";
        json_escape_ascii(out[i].name[0] ? out[i].name : "", en, sizeof(en));
        json_escape_ascii(out[i].description[0] ? out[i].description : "", ed, sizeof(ed));
        json_escape_ascii(out[i].provider[0] ? out[i].provider : "native", ep, sizeof(ep));
        p += (size_t)snprintf(buf + p, cap - p,
            "%s{\"name\":\"%s\",\"description\":\"%s\",\"provider\":\"%s\",\"risk\":\"%s\"}",
            i ? "," : "", en, ed, ep, risk);
    }
    p += (size_t)snprintf(buf + p, cap - p, "]");
    return buf;
}
