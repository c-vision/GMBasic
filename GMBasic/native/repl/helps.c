/* native/repl/helps.c — speculare a HelpIndex.cs/HelpForm.cs/HelpEntry.cs.
 * Mostra l'help da Helps/<topic>.json.
 */
#include "repl.h"
#include <unistd.h>

static char g_help_buf[65536];

/* JSON mini-parse: legge stringa da una chiave (primo match). */
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

/* Cerca Helps/<topic>.json in: CWD/Helps, REPL/Helps, root repo/Helps. */
static FILE* helps_open(const char* topic, char* path, int cap)
{
    const char* dirs[] = {"shared/helps", "Helps", "REPL/Helps", NULL};
    char cwd[2048];
    getcwd(cwd, sizeof(cwd));
    for (int i = 0; dirs[i]; i++) {
        snprintf(path, cap, "%s/%s/%s.json", cwd, dirs[i], topic ? topic : "grammar");
        FILE* f = fopen(path, "r");
        if (f) return f;
    }
    /* root repo (GMBASIC.sln) + REPL/Helps */
    char cur[2048];
    snprintf(cur, sizeof(cur), "%s", cwd);
    for (;;) {
        char probe[4096];
        snprintf(probe, sizeof(probe), "%s/GMBASIC.sln", cur);
        if (access(probe, F_OK) == 0) {
            snprintf(path, cap, "%s/REPL/Helps/%s.json", cur, topic ? topic : "grammar");
            FILE* f = fopen(path, "r");
            if (f) return f;
            break;
        }
        char* slash = strrchr(cur, '/');
        if (!slash) break;
        *slash = 0;
    }
    return NULL;
}

int rpl_help_show(const char* topic)
{
    char path[1024];
    FILE* f = helps_open(topic, path, sizeof(path));
    if (!f) {
        fprintf(stderr, "?Help '%s' non trovato (Helps/<topic>.json)\n", topic ? topic : "grammar");
        return 1;
    }
    size_t n = fread(g_help_buf, 1, sizeof(g_help_buf) - 1, f);
    g_help_buf[n] = 0; fclose(f);
    const char* name = json_get_string(g_help_buf, "name");
    const char* desc = json_get_string(g_help_buf, "desc");
    if (name) printf("== %s ==\n", name);
    if (desc) { printf("%s\n", desc); return 0; }
    /* JSON ad array di comandi: elenca i nomi (campo "name" di ogni elemento) */
    if (g_help_buf[0] == '[') {
        int n = 0;
        const char* p = g_help_buf;
        while ((p = strstr(p, "\"name\":")) != NULL) {
            p += 6;
            const char* q = strchr(p, '"');
            if (q) { q++; const char* e = strchr(q, '"'); int ln = e ? (int)(e - q) : 0;
                if (ln > 0 && ln < 64) printf("  %.*s\n", ln, q); n++; }
        }
        printf("(%d comandi)\n", n);
        return 0;
    }
    printf("%s\n", g_help_buf);
    return 0;
}
