/* ============================================================================
 * gmb_settings.c — Settings globali + app (porting SettingsManager.cs).
 * system.settings.json (default) + app.settings.json (override per progetto).
 * Mini-parse JSON: primo match di una chiave (sufficiente per consoleConfiguration).
 * ============================================================================ */
#include "gmb_runtime.h"
#include "gmb_graphics.h"
#include "gmb_ui_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_sbuf[262144];

static char* settings_read(const char* path)
{
    if (!path || !*path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    size_t n = fread(g_sbuf, 1, sizeof(g_sbuf) - 1, f);
    fclose(f); g_sbuf[n] = 0;
    return g_sbuf;
}

int gmb_settings_str(const char* file, const char* key, char* out, int cap)
{
    char* js = settings_read(file);
    if (!js) return 0;
    char pat[128]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(js, pat);
    if (!p) return 0;
    p = strchr(p + 1, ':'); if (!p) return 0;
    p = strchr(p, '"'); if (!p) return 0;
    p++;
    int o = 0; while (*p && *p != '"' && o < cap - 1) out[o++] = *p++;
    out[o] = 0;
    return 1;
}

int gmb_settings_int(const char* file, const char* key, int* out)
{
    char* js = settings_read(file);
    if (!js) return 0;
    char pat[128]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(js, pat);
    if (!p) return 0;
    p = strchr(p + 1, ':'); if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return 0;
    *out = atoi(p);
    return 1;
}

/* Applica consoleConfiguration[0] (system, poi app override): charGrid (buffer/console)
 * oppure logicalWidth/Height; coerente con gmb_screen_config_apply. */
void gmb_settings_apply_console_default(void)
{
    int idx = 0;
    const char* env = getenv("GMB_CONSOLE"); if (env && *env) idx = atoi(env);
    int gw = 0, gh = 0, lw = 0, lh = 0;
    gmb_settings_console_int(idx, "charGridWidth", &gw);
    gmb_settings_console_int(idx, "charGridHeight", &gh);
    gmb_settings_console_int(idx, "logicalWidth", &lw);
    gmb_settings_console_int(idx, "logicalHeight", &lh);
    if (gw > 0 && gh > 0)
    {
        gmb_console_set_buffer_size(gw, gh);
        gmb_console_set_window_size(gw, gh);
    }
    else if (lw > 0 && lh > 0)
    {
        GMB_GFX_SCREEN(-1, lw, lh, 0);
    }
}

/* ===== Parser depth-aware (array/oggetti/stringhe) per consoleConfiguration[index] ===== */

/* Ritorna il puntatore al valore di `key` a livello top-level dell'oggetto [s,e). */
static const char* obj_value(const char* s, const char* e, const char* key)
{
    char pat[128]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    size_t kl = strlen(pat);
    int depth = 0;
    const char* p = s;
    while (p < e)
    {
        if (*p == '"')
        {
            if (depth == 0 && (size_t)(e - p) >= kl && strncmp(p, pat, kl) == 0)
            {
                const char* q = p + kl;
                while (q < e && (*q == ' ' || *q == '\t')) q++;
                if (q < e && *q == ':') return q + 1;
            }
            p++; while (p < e && *p && *p != '"') { if (*p == '\\' && p + 1 < e) p++; p++; }
            p++;
            continue;
        }
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') depth--;
        p++;
    }
    return NULL;
}

/* Ritorna [start,end) dell'elemento `index` dell'array il cui primo carattere non-bianco
 * è in (q, e). Elementi = oggetti/valori separati da ',' a livello top-level dell'array. */
static int array_elem(const char* q, const char* e, int index, const char** out_s, const char** out_e)
{
    while (q < e && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r' || *q == ',')) q++;
    int depth = 0, idx = 0;
    const char* start = q;
    for (const char* p = q; p < e; )
    {
        char ch = *p;
        if (ch == '"')
        {
            p++; while (p < e && *p && *p != '"') { if (*p == '\\' && p + 1 < e) p++; p++; }
            p++;
            continue;
        }
        if (ch == '{' || ch == '[') depth++;
        else if (ch == '}' || ch == ']')
        {
            depth--;
            if (depth == 0 && *p == '}')
            {
                if (idx == index) { *out_s = start; *out_e = p + 1; return 1; }
                idx++;
                p++;
                while (p < e && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
                start = p;
                continue;
            }
            if (depth == 0 && *p == ']') return 0;
        }
        p++;
    }
    return 0;
}

/* Trova l'oggetto richiesto in un file: consoleConfiguration[index]. */
static int console_obj(const char* file, int index, const char** os, const char** oe)
{
    char* js = settings_read(file);
    if (!js) return 0;
    const char* end = js + strlen(js);
    const char* v = obj_value(js, end, "consoleConfiguration");
    if (!v) return 0;
    while (v < end && *v != '[') v++;
    if (v >= end) return 0;
    return array_elem(v + 1, end, index, os, oe);
}

/* Campo stringa/int di consoleConfiguration[index]: system, poi OVERRIDE app. */
int gmb_settings_console_str(int index, const char* key, char* out, int cap)
{
    extern int gmb_paths_settings_file(char* out, size_t cap);
    char sp[1024]; int have = gmb_paths_settings_file(sp, sizeof(sp));
    const char *s, *e;
    if (have && console_obj(sp, index, &s, &e))
    {
        const char* val = obj_value(s, e, key);
        if (val) { while (*val == ' ' || *val == '\t') val++; if (*val == '"') { val++; int o=0; while (*val && *val != '"' && o<cap-1) out[o++]=*val++; out[o]=0; return 1; } }
    }
    if (console_obj("app.settings.json", index, &s, &e))
    {
        const char* val = obj_value(s, e, key);
        if (val) { while (*val == ' ' || *val == '\t') val++; if (*val == '"') { val++; int o=0; while (*val && *val != '"' && o<cap-1) out[o++]=*val++; out[o]=0; return 1; } }
    }
    return 0;
}
int gmb_settings_console_int(int index, const char* key, int* out)
{
    extern int gmb_paths_settings_file(char* out, size_t cap);
    extern int gmb_paths_app_settings(char* out, size_t cap);
    char sp[1024]; int have = gmb_paths_settings_file(sp, sizeof(sp));
    const char *s, *e;
    const char* files[2]; int nf = 0;
    static char ap[2600]; int have_app = gmb_paths_app_settings(ap, sizeof ap);
    if (have) files[nf++] = sp;
    /* l'override dell'app può stare accanto all'ESEGUIBILE (compilato distribuito) */
    files[nf++] = have_app ? ap : "app.settings.json";
    for (int i = nf - 1; i >= 0; i--)   /* app override */
    {
        if (!files[i] || !files[i][0]) continue;
        if (console_obj(files[i], index, &s, &e))
        {
            const char* val = obj_value(s, e, key);
            if (val) { while (*val == ' ' || *val == '\t') val++; if (*val >= '0' && *val <= '9') { *out = atoi(val); return 1; } }
        }
    }
    return 0;
}
