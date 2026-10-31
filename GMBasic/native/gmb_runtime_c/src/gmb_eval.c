/* gmb_eval.c — T4.1: UAT automatica via vault (oracolo a TERMINI, nessun LLM).
 * Parità con Basic.AI.Eval.VaultValidator (C#): valida un RISULTATO contro le requirement-line
 * dei doc .md/.txt di una cartella; verdetto JSON {"ok","passed","failed","coverage"}.
 * Runtime CONDIVISO (AGENTS §0bis). */
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define EV_MAX_TERMS 64
#define EV_MAX_REQS  256

typedef struct { char text[1024]; char terms[EV_MAX_TERMS][64]; int nterms; } ev_req_t;

static const char* EV_STOP[] = {
    "il","lo","la","le","gli","un","una","uno","e","ed","di","da","con","per","che","deve",
    "dovra","essere","del","della","dei","non","ma","se","o","in","su","tra","fra","nel","nella",
    "the","a","an","and","of","to","for","with","shall","must","should","will","be","is","are",
    "this","that","from","on","at","by", NULL
};

static int ev_stopword(const char* w)
{
    for (int i = 0; EV_STOP[i]; i++) if (strcmp(w, EV_STOP[i]) == 0) return 1;
    return 0;
}

static int ev_term_char(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80;
}

/* Termini significativi: parole >=4 char (a-z/0-9/accentate), minuscole, no stopword, distinct. */
static int ev_terms(const char* text, char out[EV_MAX_TERMS][64])
{
    int n = 0;
    if (!text) return 0;
    const char* p = text;
    while (*p)
    {
        if (ev_term_char((unsigned char)tolower((unsigned char)*p)))
        {
            char w[64]; int wi = 0;
            while (*p && ev_term_char((unsigned char)tolower((unsigned char)*p)) && wi < 63)
            {
                unsigned char c = (unsigned char)*p;
                w[wi++] = (c < 0x80) ? (char)tolower(c) : (char)c;
                p++;
            }
            w[wi] = '\0';
            if (wi >= 4 && !ev_stopword(w))
            {
                int dup = 0;
                for (int i = 0; i < n; i++) if (strcmp(out[i], w) == 0) { dup = 1; break; }
                if (!dup && n < EV_MAX_TERMS) { snprintf(out[n], 64, "%s", w); n++; }
            }
        }
        else p++;
    }
    return n;
}

/* True se la riga è una requirement-line (bullet o MUST/SHOULD/REQUIREMENT). */
static int ev_req_line(const char* line, char* out, size_t cap)
{
    const char* s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-' || *s == '*') { const char* r = s + 1; while (*r == ' ' || *r == '\t') r++; snprintf(out, cap, "%s", r); return 1; }
    if ((unsigned char)s[0] == 0xE2 && (unsigned char)s[1] == 0x80 && (unsigned char)s[2] == 0xA2) { const char* r = s + 3; while (*r == ' ' || *r == '\t') r++; snprintf(out, cap, "%s", r); return 1; }
    if (isdigit((unsigned char)*s))
    {
        const char* r = s; while (isdigit((unsigned char)*r)) r++;
        if (*r == '.' || *r == ')') { r++; while (*r == ' ' || *r == '\t') r++; snprintf(out, cap, "%s", r); return 1; }
    }
    /* MUST/SHOULD/REQUIREMENT [spazi] ':' */
    for (const char* p = line; *p; p++)
    {
        if ((strncasecmp(p, "MUST", 4) == 0 || strncasecmp(p, "SHOULD", 6) == 0 || strncasecmp(p, "REQUIREMENT", 11) == 0))
        {
            const char* q = p;
            while (*q && *q != ':' && *q != '\n') { if (!isspace((unsigned char)*q)) { if (*q != ':' && *q != 0) { /* salta lettere della parola */ } } q++; }
            /* cerca ':' dopo la parola chiave */
            q = p;
            while (*q && *q != ':') q++;
            if (*q == ':') { snprintf(out, cap, "%s", line); return 1; }
        }
    }
    return 0;
}

static int ev_ends(const char* f, const char* ext)
{
    size_t lf = strlen(f), le = strlen(ext);
    return lf >= le && strcasecmp(f + lf - le, ext) == 0;
}

/* Raccoglie le requirement-line dai .md/.txt ricorsivamente. */
static int ev_collect(const char* dir, ev_req_t* reqs, int n, int cap)
{
    DIR* d = opendir(dir);
    if (!d) return n;
    struct dirent* e;
    while ((e = readdir(d)) != NULL)
    {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { n = ev_collect(path, reqs, n, cap); continue; }
        if (!ev_ends(e->d_name, ".md") && !ev_ends(e->d_name, ".txt")) continue;
        FILE* fp = fopen(path, "r");
        if (!fp) continue;
        char line[2048];
        while (fgets(line, sizeof(line), fp))
        {
            size_t ll = strlen(line);
            while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r')) line[--ll] = '\0';
            char cand[1024];
            if (!ev_req_line(line, cand, sizeof(cand))) continue;
            if (n >= cap) continue;
            char terms[EV_MAX_TERMS][64];
            int nt = ev_terms(cand, terms);
            if (nt == 0) continue;
            snprintf(reqs[n].text, sizeof(reqs[n].text), "%s", line);
            for (int i = 0; i < nt; i++) snprintf(reqs[n].terms[i], 64, "%s", terms[i]);
            reqs[n].nterms = nt;
            n++;
        }
        fclose(fp);
    }
    closedir(d);
    return n;
}

static void ev_lower(const char* s, char* out, size_t cap)
{
    size_t i = 0;
    for (; s && *s && i + 1 < cap; s++) out[i++] = (char)tolower((unsigned char)*s);
    out[i] = '\0';
}

static void ev_json_str(const char* s, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = s ? s : ""; *p && o + 2 < cap; p++)
    {
        if (*p == '"' || *p == '\\') { out[o++] = '\\'; out[o++] = *p; }
        else out[o++] = *p;
    }
    out[o] = '\0';
}

/* "0.###" invariant: intero se senza decimali, altrimenti max 3 cifre senza zeri finali. */
static void ev_fmt(double v, char* out, size_t cap)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%.3f", v);
    size_t l = strlen(tmp);
    while (l > 0 && tmp[l-1] == '0') tmp[--l] = '\0';
    if (l > 0 && tmp[l-1] == '.') tmp[--l] = '\0';
    snprintf(out, cap, "%s", tmp);
}

char* gmb_eval_validate(const char* result, const char* folder)
{
    ev_req_t* reqs = (ev_req_t*)calloc(EV_MAX_REQS, sizeof(ev_req_t));
    if (!reqs) return NULL;
    int nr = 0;
    if (folder && *folder) nr = ev_collect(folder, reqs, 0, EV_MAX_REQS);
    char lres[65536];
    ev_lower(result, lres, sizeof(lres));
    int passed = 0, failed = 0, covered = 0, totalterms = 0;
    char (*plist)[1024] = calloc(EV_MAX_REQS, 1024);
    char (*flist)[1024] = calloc(EV_MAX_REQS, 1024);
    for (int i = 0; i < nr; i++)
    {
        int hits = 0;
        for (int t = 0; t < reqs[i].nterms; t++)
            if (strstr(lres, reqs[i].terms[t])) hits++;
        double ratio = reqs[i].nterms == 0 ? 1.0 : (double)hits / reqs[i].nterms;
        covered += hits;
        totalterms += reqs[i].nterms;
        if (ratio >= 0.5) snprintf(plist[passed++], 1024, "%s", reqs[i].text);
        else snprintf(flist[failed++], 1024, "%s", reqs[i].text);
    }
    int ok = (failed == 0 && nr > 0) ? 1 : 0;
    double coverage = totalterms == 0 ? 1.0 : (double)covered / totalterms;
    size_t cap = 65536;
    char* out = (char*)malloc(cap);
    if (!out) { free(reqs); free(plist); free(flist); return NULL; }
    size_t off = (size_t)snprintf(out, cap, "{\"ok\":%s,\"passed\":[", ok ? "true" : "false");
    for (int i = 0; i < passed; i++) { char e[2048]; ev_json_str(plist[i], e, sizeof(e)); off += (size_t)snprintf(out+off, cap-off, "%s\"%s\"", i?",":"", e); }
    off += (size_t)snprintf(out+off, cap-off, "],\"failed\":[");
    for (int i = 0; i < failed; i++) { char e[2048]; ev_json_str(flist[i], e, sizeof(e)); off += (size_t)snprintf(out+off, cap-off, "%s\"%s\"", i?",":"", e); }
    char cov[64]; ev_fmt(coverage, cov, sizeof(cov));
    snprintf(out+off, cap-off, "],\"coverage\":%s}", cov);
    free(reqs); free(plist); free(flist);
    return out;
}
