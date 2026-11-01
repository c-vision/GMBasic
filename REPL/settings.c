/* native/repl/settings.c — speculare a SettingsManager.cs/IdeSettings.cs.
 * MERGE: prima system.settings.json (base/default globali), poi app.settings.json
 * (accanto al .bas) che sovrascrive gli stessi settings e ne aggiunge altri se presenti.
 */
#include "repl.h"

static char g_settings_buf[65536];

/* JSON mini-parse: legge stringa da una chiave (primo match), ignora nidificazione. */
static const char* json_get_string(const char* js, const char* key)
{
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(js, pat);
    if (!p) return NULL;
    p = strchr(p + 1, ':');
    if (!p) return NULL;
    p = strchr(p, '"');
    if (!p) return NULL;
    p++;
    static char val[512];
    size_t n = 0;
    while (*p && *p != '"' && n < sizeof(val) - 1) val[n++] = *p++;
    val[n] = 0;
    return val;
}

void rpl_settings_save(const char* project_dir, rpl_settings_t* s)
{
    char path[1024];
    if (project_dir) snprintf(path, sizeof(path), "%s/app.settings.json", project_dir);
    else snprintf(path, sizeof(path), "system.settings.json");
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n");
    fprintf(f, "  \"bell\": %s,\n", s->bell ? "true" : "false");
    fprintf(f, "  \"confirm\": %s,\n", s->confirm ? "true" : "false");
    fprintf(f, "  \"cursor\": %s,\n", s->cursor ? "true" : "false");
    fprintf(f, "  \"century\": %s,\n", s->century ? "true" : "false");
    fprintf(f, "  \"wrap\": %s,\n", s->wrap ? "true" : "false");
    fprintf(f, "  \"escape\": %s,\n", s->escape ? "true" : "false");
    fprintf(f, "  \"dateformat\": \"%s\",\n", s->dateformat);
    fprintf(f, "  \"theme\": \"%s\"\n", s->set_theme);
    fprintf(f, "}\n");
    fclose(f);
}

/* Applica TUTTE le chiavi presenti in `js` su `out` (sovrascrive; i nuovi si aggiungono). */
static void apply_settings(rpl_settings_t* out, const char* js)
{
    if (!js) return;
    char tmp[512];
    const char* v;

    v = json_get_string(js, "fontPath");
    if (v) { snprintf(tmp, sizeof(tmp), "%s", v); out->font_path = strdup(tmp); }

    v = json_get_string(js, "palette");
    if (v) { snprintf(tmp, sizeof(tmp), "%s", v); out->palette_name = strdup(tmp); }

    v = json_get_string(js, "theme");
    if (v) { snprintf(tmp, sizeof(tmp), "%s", v); out->theme = strdup(tmp); }

    v = json_get_string(js, "fontSize");
    if (v) out->font_size = atoi(v);

    /* importFiles: array — se presente qui, sostituisce l'array corrente */
    const char* p = strstr(js, "\"importFiles\"");
    if (p) {
        p = strchr(p, '[');
        if (p) {
            for (int i = 0; i < out->import_count; i++) { free(out->import_files[i]); out->import_files[i] = NULL; }
            int n = 0;
            p = strchr(p, '"');
            while (p && n < 64) {
                const char* e = strchr(p + 1, '"');
                if (!e) break;
                size_t len = (size_t)(e - p - 1);
                out->import_files[n] = (char*)malloc(len + 1);
                memcpy(out->import_files[n], p + 1, len);
                out->import_files[n][len] = 0;
                n++;
                p = strchr(e + 1, '"');
            }
            out->import_count = n;
        }
    }
}

void rpl_settings_load(const char* project_dir, rpl_settings_t* out)
{
    memset(out, 0, sizeof(*out));
    static char app_buf[65536];

    /* 1. system.settings.json — BASE (default globali). */
    FILE* f = fopen("system.settings.json", "r");
    if (f) {
        size_t n = fread(g_settings_buf, 1, sizeof(g_settings_buf) - 1, f);
        g_settings_buf[n] = 0; fclose(f);
        apply_settings(out, g_settings_buf);
    }

    /* 2. app.settings.json (accanto al .bas) — OVERRIDE: sovrascrive gli stessi
     *    settings e ne aggiunge altri se presenti. */
    if (project_dir) {
        char path[1200];
        snprintf(path, sizeof(path), "%s/app.settings.json", project_dir);
        f = fopen(path, "r");
        if (f) {
            size_t n = fread(app_buf, 1, sizeof(app_buf) - 1, f);
            app_buf[n] = 0; fclose(f);
            apply_settings(out, app_buf);
        }
    }
}
