/* native/repl/help_form.c — speculare a HelpForm.cs: browser help interattivo.
 * Carica tutti i Helps/<topic>.json del repo, elenca gli entry (name/namespace/
 * description), raggruppati per namespace; filtro, dettaglio con parametri/ESEMPI, ESC per uscire.
 * Riusa lo stesso approccio raw-mode/ANSI di editor.c e menu.c.
 */
#include "repl.h"
#include "gmb_sdl_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <sys/stat.h>

#define GMB_HELP_MAX_ENTRIES 2048
#define GMB_HELP_MAX_FILES 128

typedef struct {
    char name[96];
    char ns[64];
    char cat[64];
    char desc[512];
    char detail[6144];   /* dettaglio: sintassi, ritorno, parametri, ESEMPI, see also */
} gmb_help_entry_t;

static gmb_help_entry_t g_entries[GMB_HELP_MAX_ENTRIES];
static int g_nentries = 0;

/* ── mini JSON helpers (array boundato) ── */
/* estrae una stringa per chiave DENTRO un dato intervallo [start,end) */
static void json_str_in(const char* js, const char* start, const char* end, const char* key, char* out, int cap)
{
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = start ? start : js;
    while ((p = strstr(p, pat)) && p < end) {
        const char* c = strchr(p + 1, ':');
        if (c && c < end) {
            c = strchr(c, '"');
            if (c && c < end) {
                c++;
                const char* e = strchr(c, '"');
                if (e && e <= end) {
                    int l = (int)(e - c);
                    if (l >= cap) l = cap - 1;
                    memcpy(out, c, (size_t)l); out[l] = 0;
                    return;
                }
            }
        }
        p += strlen(pat);
    }
    out[0] = 0;
}

/* concatena le stringhe di un array "key": [...] fino al ] di chiusura, in out. */
static void json_arr_str(const char* js, const char* start, const char* end, const char* key, char* out, int cap)
{
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(start ? start : js, pat);
    if (!p || p >= end) { out[0] = 0; return; }
    p = strchr(p, '[');
    if (!p || p >= end) { out[0] = 0; return; }
    const char* q = p;
    int depth = 0;
    for (; *q && q < end; q++) { if (*q == '[') depth++; else if (*q == ']') { depth--; if (!depth) break; } }
    const char* arr_end = q;
    out[0] = 0;
    q = p;
    int n = 0;
    while ((q = strstr(q, "\"")) && q < arr_end && n < cap - 1) {
        const char* e = strchr(q + 1, '"');
        if (!e || e > arr_end) break;
        int l = (int)(e - q - 1);
        if (l > 0) {
            if (n + l + 1 < cap - 1) { memcpy(out + n, q + 1, (size_t)l); n += l; out[n++] = ' '; }
        }
        q = e + 1;
    }
    out[n] = 0;
}

/* Range [a,b) dell'array della chiave (per parameters/examples). */
static int json_arr_range(const char* start, const char* end, const char* key,
                          const char** a, const char** b)
{
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(start, pat);
    if (!p || p >= end) return 0;
    p = strchr(p, '[');
    if (!p || p >= end) return 0;
    const char* q = p; int depth = 0;
    for (; *q && q < end; q++) { if (*q == '[') depth++; else if (*q == ']') { depth--; if (!depth) break; } }
    *a = p; *b = q;
    return 1;
}

/* Primo oggetto {...} dopo *from, entro end. Aggiorna *from. 1 = trovato. */
static int json_next_object(const char** from, const char* end,
                            const char** oa, const char** ob)
{
    const char* cur = strchr(*from, '{');
    if (!cur || cur >= end) return 0;
    const char* close = cur + 1; int depth = 1;
    while (*close && depth) { if (*close == '{') depth++; else if (*close == '}') depth--; close++; }
    if (depth || close > end) return 0;
    *oa = cur; *ob = close; *from = close;
    return 1;
}

/* carica un file Helps e popola g_entries dall'array JSON */
static void help_load_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 2 || n > 2 * 1024 * 1024) { fclose(f); return; }
    char* js = (char*)malloc((size_t)n + 1);
    fread(js, 1, (size_t)n, f); js[n] = 0; fclose(f);
    const char* p = strchr(js, '[');
    if (!p) { free(js); return; }
    /* itera gli oggetti {...} dell'array */
    const char* cur = p;
    while ((cur = strchr(cur, '{')) && g_nentries < GMB_HELP_MAX_ENTRIES) {
        const char* close = cur + 1;
        int depth = 1;
        while (*close && depth) { if (*close == '{') depth++; else if (*close == '}') depth--; close++; }
        if (depth) break;
        gmb_help_entry_t* e = &g_entries[g_nentries];
        char name[96] = "", ns[64] = "", cat[64] = "", desc[1024] = "", syn[1024] = "", ret[64] = "", see[512] = "";
        json_str_in(js, cur, close, "name", name, sizeof(name));
        json_str_in(js, cur, close, "namespace", ns, sizeof(ns));
        json_str_in(js, cur, close, "category", cat, sizeof(cat));
        json_str_in(js, cur, close, "description", desc, sizeof(desc));
        json_arr_str(js, cur, close, "syntax", syn, sizeof(syn));
        json_str_in(js, cur, close, "returnType", ret, sizeof(ret));
        json_arr_str(js, cur, close, "seeAlso", see, sizeof(see));
        if (name[0]) {
            snprintf(e->name, sizeof(e->name), "%s", name);
            snprintf(e->ns, sizeof(e->ns), "%s", ns[0] ? ns : "-");
            snprintf(e->cat, sizeof(e->cat), "%s", cat);
            snprintf(e->desc, sizeof(e->desc), "%s", desc);

            /* dettaglio: nome, ns/categoria, descrizione, sintassi, ritorno, parametri, esempi, see also */
            int w = 0;
            char* d = e->detail; int cap = (int)sizeof(e->detail);
            w += snprintf(d + w, cap - w, "%s", name);
            if (ns[0] || cat[0]) w += snprintf(d + w, cap - w, "\n%s%s%s", ns, cat[0] ? "  -  " : "", cat);
            w += snprintf(d + w, cap - w, "\n\n%s", desc);
            if (syn[0]) w += snprintf(d + w, cap - w, "\n\nSintassi:\n  %s", syn);
            if (ret[0]) w += snprintf(d + w, cap - w, "\n\nRitorna: %s", ret);
            /* parametri */
            { const char *pa, *pb;
              if (json_arr_range(cur, close, "parameters", &pa, &pb)) {
                  const char* from = pa + 1;
                  char pname[64], ptype[64], pdesc[256];
                  int any = 0;
                  while (json_next_object(&from, pb, &pa, &pb) ) {
                      json_str_in(js, pa, pb, "name", pname, sizeof(pname));
                      json_str_in(js, pa, pb, "type", ptype, sizeof(ptype));
                      json_str_in(js, pa, pb, "description", pdesc, sizeof(pdesc));
                      if (!pname[0]) continue;
                      if (!any) { w += snprintf(d + w, cap - w, "\n\nParametri:"); any = 1; }
                      w += snprintf(d + w, cap - w, "\n  %s%s%s%s%s", pname,
                                    ptype[0] ? " " : "", ptype,
                                    pdesc[0] ? " - " : "", pdesc);
                  }
              } }
            /* esempi */
            { const char *ea, *eb;
              if (json_arr_range(cur, close, "examples", &ea, &eb)) {
                  const char* from = ea + 1;
                  char edesc[256], ecode[2048], eout[1024];
                  int idx = 0;
                  while (json_next_object(&from, eb, &ea, &eb)) {
                      json_str_in(js, ea, eb, "description", edesc, sizeof(edesc));
                      json_str_in(js, ea, eb, "code", ecode, sizeof(ecode));
                      json_str_in(js, ea, eb, "expectedOutput", eout, sizeof(eout));
                      idx++;
                      w += snprintf(d + w, cap - w, "\n\nEsempio%s%s%s", idx > 1 ? " (2)" : "",
                                    edesc[0] ? " - " : "", edesc);
                      if (ecode[0]) {
                          /* il codice è JSON-escaped: \n nel file qui è già '\\' 'n' */
                          w += snprintf(d + w, cap - w, "\n");
                          for (const char* c = ecode; *c && w < cap - 2; c++) {
                              if (c[0] == '\\' && c[1] == 'n') { d[w++] = '\n'; c++; }
                              else if (c[0] == '\\' && c[1] == 't') { d[w++] = ' '; d[w++] = ' '; c++; }
                              else if (c[0] == '\\' && c[1] == '"') { d[w++] = '"'; c++; }
                              else d[w++] = *c;
                              d[w] = 0;
                          }
                      }
                      if (eout[0]) {
                          w += snprintf(d + w, cap - w, "\n  -> ");
                          for (const char* c = eout; *c && w < cap - 2; c++) {
                              if (c[0] == '\\' && c[1] == 'n') { d[w++] = '\n'; d[w++] = ' '; d[w++] = ' '; c++; }
                              else d[w++] = *c;
                              d[w] = 0;
                          }
                      }
                  }
              } }
            if (see[0]) w += snprintf(d + w, cap - w, "\n\nVedi anche: %s", see);
            if (w >= cap - 1) d[cap - 1] = 0;
            g_nentries++;
        }
        cur = close;
    }
    free(js);
}

/* carica tutti i Helps/<topic>.json da REPL/Helps della root repo */
static void help_load_all(void)
{
    char root[2048];
    /* Help CONDIVISI: <monorepo>/shared/helps (una copia per i due rami). */
    resolve_repo_path(root, sizeof(root), "REPL/helps");
    if (access(root, F_OK) != 0) resolve_repo_path(root, sizeof(root), "shared/helps");
    DIR* d = opendir(root);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        size_t l = strlen(de->d_name);
        if (l < 5 || strcmp(de->d_name + l - 5, ".json") != 0) continue;
        char path[2300];
        snprintf(path, sizeof(path), "%s/%s", root, de->d_name);
        help_load_file(path);
    }
    closedir(d);
}

/* ── raw mode / tasti (stesso approccio di menu.c) ── */
static int g_tty = -1;
static struct termios g_orig_tty;
static int h_tty_open(void)
{
    g_tty = open("/dev/tty", O_RDWR);
    if (g_tty < 0) return -1;
    struct termios raw;
    tcgetattr(g_tty, &g_orig_tty);
    raw = g_orig_tty;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    tcsetattr(g_tty, TCSANOW, &raw);
    return 0;
}
static int h_read(int ms, unsigned char* o)
{
    fd_set f; struct timeval tv;
    FD_ZERO(&f); FD_SET(g_tty, &f);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    if (select(g_tty + 1, &f, NULL, NULL, &tv) <= 0) return -1;
    return read(g_tty, o, 1) == 1 ? 0 : -1;
}
static int h_key(void)
{
    unsigned char c;
    if (read(g_tty, &c, 1) != 1) return -1;
    if (c == 27) {
        unsigned char s[2];
        if (h_read(100, &s[0]) != 0) return 27;
        if (h_read(100, &s[1]) != 0) return 27;
        if (s[0] == '[') {
            switch (s[1]) {
                case 'A': return 1; case 'B': return 2;
                case 'H': return 5; case 'F': return 6;
                case '~': if (h_read(100, &c) == 0) { if (c=='5') return 11; if (c=='6') return 12; if (c=='3') return 8; } return -1;
            }
        }
        return -1;
    }
    if (c == 127 || c == 8) return 7;
    return c;
}

static int help_cmp_ns_name(const void* a, const void* b)
{
    const gmb_help_entry_t* x = (const gmb_help_entry_t*)a;
    const gmb_help_entry_t* y = (const gmb_help_entry_t*)b;
    int c = strcasecmp(x->ns, y->ns);
    if (c) return c;
    return strcasecmp(x->name, y->name);
}

/* Carica (una volta) e ordina per namespace/nome. 1 = ok. */
static int help_ensure_loaded(void)
{
    if (g_nentries == 0) {
        help_load_all();
        qsort(g_entries, (size_t)g_nentries, sizeof(g_entries[0]), help_cmp_ns_name);
        if (getenv("GMBASIC_DEBUG")) fprintf(stderr, "[help] loaded %d entries\n", g_nentries);
    }
    return g_nentries > 0;
}

/* Indice dell'help DENTRO il buffer a celle (consolle a finestra: SDL/C64).
 * Il browser a schermo intero ha bisogno di un terminale; in finestra scriviamo
 * l'elenco raggruppato nel modello e poi presentiamo. */
int gmb_help_print_index(gmb_sdl_console_t* model)
{
    if (!model) return 1;
    if (!help_ensure_loaded()) {
        gmb_sdl_console_write(model, "?help: nessun entry (REPL/helps/*.json non trovati)\n");
        gmb_sdl_console_render(model);
        return 1;
    }
    int W = gmb_screen_width(gmb_sdl_console_screen(model));
    if (W < 20) W = 80;
    /* pagina nuova: senza clear l'elenco scorre via dal buffer */
    gmb_sdl_console_clear_screen(model);
    gmb_sdl_console_set_cursor_position(model, 1, 1);
    gmb_sdl_console_write(model, "\n");
    gmb_sdl_console_write(model, "GMBASIC Help | HELP <namespace> (es. HELP math) per il dettaglio\n\n");
    /* Solo i GRUPPI con il conteggio, UNA riga ciascuno (513 righe non ci stanno). */
    {
        char last_group[64] = "", line[160];
        int count = 0, row = 0;
        for (int i = 0; i <= g_nentries; i++) {
            char group[64] = "";
            if (i < g_nentries) {
                snprintf(group, sizeof(group), "%s", g_entries[i].ns);
                char* dot = strchr(group, '.');
                if (dot) *dot = 0;
                if (!group[0] || strcmp(group, "-") == 0) snprintf(group, sizeof(group), "GENERAL");
            }
            if (i == g_nentries || strcasecmp(group, last_group) != 0) {
                if (last_group[0] && row < 40) {
                    snprintf(line, sizeof(line), "  %-18.18s %3d voci\n", last_group, count);
                    gmb_sdl_console_write(model, line);
                    row++;
                }
                if (i == g_nentries) break;
                snprintf(last_group, sizeof(last_group), "%s", group);
                count = 0;
            }
            count++;
        }
    }
    gmb_sdl_console_write(model, "\n");
    gmb_sdl_console_render(model);
    return 1;
}

int gmb_help_browse(void)
{
    if (!help_ensure_loaded()) { fprintf(stderr, "?help: nessun entry (REPL/helps/*.json non trovati)\n"); return 1; }
    if (h_tty_open() != 0) { fprintf(stderr, "?help: /dev/tty non disponibile\n"); return 1; }
    printf("\033[?25l");

    static int vis[GMB_HELP_MAX_ENTRIES];
    int nvis = 0;
    char filter[64] = "";
    int cur = 0, top = 0, dscroll = 0;
    int rows = 25;
    int running = 1, in_detail = 0;

    /* lista visibile (filtro) */
    #define REBUILD_VIS() do { nvis = 0; for (int i = 0; i < g_nentries; i++) { \
        if (filter[0] && !strcasestr(g_entries[i].name, filter) && !strcasestr(g_entries[i].ns, filter) \
                       && !strcasestr(g_entries[i].cat, filter)) continue; \
        vis[nvis++] = i; } \
        if (cur >= nvis) cur = nvis > 0 ? nvis - 1 : 0; \
        if (cur < 0) cur = 0; top = 0; dscroll = 0; } while (0)
    REBUILD_VIS();

    while (running) {
        struct winsize ws;
        if (ioctl(g_tty, TIOCGWINSZ, &ws) == 0) rows = ws.ws_row;
        if (rows < 8) rows = 25;
        int page = rows - 3;
        if (top < 0) top = 0;
        if (cur < top) top = cur;
        if (cur >= top + page) top = cur - page + 1;

        printf("\033[2J\033[H");
        printf("\033[0;44m  GMBASIC Help  |  %d/%d items  |  %s  |  ESC=Quit Tab/Enter=detail\033[0m\n",
               nvis, g_nentries, in_detail ? "DETAIL" : "LIST");
        printf("\033[0;44m  > %s\033[0m\033[K\n", filter);

        if (in_detail) {
            const gmb_help_entry_t* e = &g_entries[vis[cur]];
            if (getenv("GMBASIC_DEBUG")) fprintf(stderr, "[help] detail %s len=%zu\n", e->name, strlen(e->detail));
            /* dettaglio scrollabile: righe del buffer detail */
            const char* d = e->detail;
            int line = 0, shown = 0;
            for (const char* q = d; *q && shown < page; q++) {
                if (line++ < dscroll) { if (*q == '\n') {} continue; }
                const char* nl = strchr(q, '\n');
                int len = nl ? (int)(nl - q) : (int)strlen(q);
                printf("  %.*s\033[K", len, q);
                if (nl) { printf("\n"); shown++; q = nl; } else break;
            }
            printf("\033[%d;1H\033[0;90m  (Up/Down scroll, ESC/Tab/Enter back)\033[0m", rows);
        } else {
            int lines = 0;
            char last_group[64] = "";
            for (int i = top; i < nvis && lines < page; i++) {
                const gmb_help_entry_t* e = &g_entries[vis[i]];
                /* gruppo = namespace PRINCIPALE (prima del '.'), es. TRADING.METRIC -> TRADING */
                char group[64];
                snprintf(group, sizeof(group), "%s", e->ns);
                char* dot = strchr(group, '.');
                if (dot) *dot = 0;
                if (!group[0] || strcmp(group, "-") == 0) snprintf(group, sizeof(group), "GENERAL");
                /* intestazione di gruppo quando cambia (solo senza filtro) */
                if (!filter[0] && strcasecmp(group, last_group) != 0) {
                    if (lines + 1 >= page) break;
                    printf("\033[0;36m  <%s>\033[0m\033[K\n", group);
                    snprintf(last_group, sizeof(last_group), "%s", group);
                    lines++;
                    if (lines >= page) break;
                }
                if (i == cur)
                    printf("\033[46m\033[30m> %-28.28s %-14.14s %-50.50s\033[0m\033[K\n", e->name, e->cat[0] ? e->cat : e->ns, e->desc);
                else
                    printf("  %-28.28s %-14.14s %-50.50s\033[K\n", e->name, e->cat[0] ? e->cat : e->ns, e->desc);
                lines++;
            }
            printf("\033[%d;1H\033[0;90m  Up/Down move, PgUp/PgDn, type to filter, Enter detail, ESC quit\033[0m", rows);
        }

        fflush(stdout);
        int k = h_key();
        if (in_detail) {
            if (k == 27 || k == 13 || k == 9) { in_detail = 0; dscroll = 0; }
            else if (k == 1) { if (dscroll > 0) dscroll--; }
            else if (k == 2) { dscroll++; }
            else if (k == 11) { dscroll -= page; if (dscroll < 0) dscroll = 0; }
            else if (k == 12) { dscroll += page; }
            continue;
        }
        if (k >= 32 && k < 127) {
            size_t l = strlen(filter);
            if (l < sizeof(filter) - 1) { filter[l] = (char)k; filter[l + 1] = 0; cur = 0; REBUILD_VIS(); }
            continue;
        }
        if (k == 7) { if (filter[0]) filter[strlen(filter) - 1] = 0; cur = 0; REBUILD_VIS(); continue; }
        switch (k) {
            case 1: if (cur > 0) cur--; break;
            case 2: if (cur < nvis - 1) cur++; break;
            case 5: cur = 0; break;
            case 6: cur = nvis > 0 ? nvis - 1 : 0; break;
            case 11: cur -= page; if (cur < 0) cur = 0; break;
            case 12: cur += page; if (cur > nvis - 1) cur = nvis > 0 ? nvis - 1 : 0; break;
            case 13: case 9: if (nvis > 0) { in_detail = 1; dscroll = 0; } break;
            case 27: running = 0; break;
        }
    }
    #undef REBUILD_VIS
    printf("\033[2J\033[H\033[?25h\033[0m");
    if (g_tty >= 0) { tcsetattr(g_tty, TCSANOW, &g_orig_tty); close(g_tty); g_tty = -1; }
    return 0;
}
