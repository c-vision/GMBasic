/* native/repl/help_form.c — speculare a HelpForm.cs: browser help interattivo.
 * Carica tutti i Helps/<topic>.json del repo, elenca gli entry (name/namespace/
 * description), filtro di ricerca, ↑↓/Enter per aprire il dettaglio, ESC per uscire.
 * Riusa lo stesso approccio raw-mode/ANSI di editor.c e menu.c.
 */
#include "repl.h"
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

#define GMB_HELP_MAX_ENTRIES 4096
#define GMB_HELP_MAX_FILES 128

typedef struct {
    char name[96];
    char ns[64];
    char desc[512];
    char detail[2048];   /* dettaglio pre-costruito (syntax/returns/param/seealso) */
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
        char name[96] = "", ns[64] = "", desc[512] = "", syn[1024] = "", ret[64] = "", see[512] = "";
        json_str_in(js, cur, close, "name", name, sizeof(name));
        json_str_in(js, cur, close, "namespace", ns, sizeof(ns));
        json_str_in(js, cur, close, "description", desc, sizeof(desc));
        json_arr_str(js, cur, close, "syntax", syn, sizeof(syn));
        json_str_in(js, cur, close, "returnType", ret, sizeof(ret));
        json_arr_str(js, cur, close, "seeAlso", see, sizeof(see));
        if (name[0]) {
            snprintf(e->name, sizeof(e->name), "%s", name);
            snprintf(e->ns, sizeof(e->ns), "%s", ns[0] ? ns : "-");
            snprintf(e->desc, sizeof(e->desc), "%s", desc);
            snprintf(e->detail, sizeof(e->detail), "%s\n%s\n\n%s%s\n%s%s\n%s%s%s",
                name, ret[0] ? ret : "", desc,
                syn[0] ? "Syntax: " : "", syn,
                see[0] ? "See also: " : "", see,
                ret[0] ? "\nReturns: " : "", ret[0] ? ret : "");
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
    resolve_repo_path(root, sizeof(root), "shared/helps");
    if (access(root, F_OK) != 0) resolve_repo_path(root, sizeof(root), "REPL/Helps");
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

int gmb_help_browse(void)
{
    if (getenv("GMBASIC_DEBUG")) fprintf(stderr, "[help] browse enter\n");
    if (g_nentries == 0) { help_load_all(); if (getenv("GMBASIC_DEBUG")) fprintf(stderr, "[help] loaded %d entries\n", g_nentries); }
    if (g_nentries == 0) { fprintf(stderr, "?help: nessun entry (Helps/*.json non trovati)\n"); return 1; }
    if (h_tty_open() != 0) { fprintf(stderr, "?help: /dev/tty non disponibile\n"); return 1; }
    printf("\033[?25l");

    char filter[64] = "";
    int cur = 0, top = 0;
    int rows = 25;
    bool running = true;
    bool in_detail = false;

    while (running) {
        struct winsize ws;
        if (ioctl(g_tty, TIOCGWINSZ, &ws) == 0) rows = ws.ws_row;
        if (rows < 5) rows = 25;
        int page = rows - 3;
        if (cur < top) top = cur;
        if (cur >= top + page) top = cur - page + 1;
        if (top < 0) top = 0;

        printf("\033[2J\033[H\033[0;44m  GMBASIC Help  |  %d items  |  ESC=Quit Tab/Enter=detail\033[0m\n", g_nentries);
        printf("\033[0;44m  > %s\033[0m\033[K\n", filter);
        if (in_detail) {
            printf("\033[0;90m  %s\033[0m\033[K\n", g_entries[cur].name);
            printf("%s", g_entries[cur].detail);
        } else {
            for (int i = top; i < top + page && i < g_nentries; i++) {
                const char* nm = g_entries[i].name;
                if (filter[0] && !strcasestr(nm, filter) && !strcasestr(g_entries[i].ns, filter)) continue;
                if (i == cur) printf("\033[46m\033[30m> %-28.28s %-16.16s %-40.40s\033[0m\033[K\n", nm, g_entries[i].ns, g_entries[i].desc);
                else printf("  %-28.28s %-16.16s %-40.40s\033[K\n", nm, g_entries[i].ns, g_entries[i].desc);
            }
        }

        int k = h_key();
        if (in_detail) {
            if (k == 27 || k == 13 || k == 9) in_detail = false;
            continue;
        }
        if (k >= 32 && k < 127) {
            size_t l = strlen(filter);
            if (l < sizeof(filter) - 1) { filter[l] = (char)k; filter[l + 1] = 0; cur = 0; top = 0; }
            continue;
        }
        if (k == 7 && filter[0]) { filter[strlen(filter) - 1] = 0; cur = 0; top = 0; continue; }
        switch (k) {
            case 1: if (cur > 0) cur--; break;
            case 2: if (cur < g_nentries - 1) cur++; break;
            case 5: cur = 0; break;
            case 6: cur = g_nentries - 1; break;
            case 11: cur -= page; if (cur < 0) cur = 0; break;
            case 12: cur += page; if (cur > g_nentries - 1) cur = g_nentries - 1; break;
            case 13: case 9: in_detail = true; break;
            case 27: running = false; break;
        }
    }
    printf("\033[2J\033[H\033[?25h\033[0m");
    if (g_tty >= 0) { tcsetattr(g_tty, TCSANOW, &g_orig_tty); close(g_tty); g_tty = -1; }
    return 0;
}
