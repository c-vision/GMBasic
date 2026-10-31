/* gmb_pkg_glob.c — match glob (riferimento C del Package Manager). */
#include "gmb_pkg_glob.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static int eqc(char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); }

/* Glob con backtracking: '*' un segmento, '**' qualsiasi, '?' un char non-'/'. Case-insensitive. */
static int glob_match(const char* p, const char* s)
{
    while (*p) {
        if (p[0] == '*') {
            int dbl = (p[1] == '*');
            const char* pp = p + (dbl ? 2 : 1);
            if (!*pp) return 1;
            for (const char* t = s;; t++) {
                if (glob_match(pp, t)) return 1;
                if (!*t) break;
                if (!dbl && *t == '/') break; /* '*' singolo non attraversa '/' */
            }
            return 0;
        }
        if (p[0] == '?') {
            if (!*s || *s == '/') return 0;
            p++; s++;
            continue;
        }
        if (!eqc(*p, *s)) return 0;
        p++; s++;
    }
    return *s == 0;
}

static void norm(const char* in, char* out, size_t cap)
{
    size_t j = 0;
    for (const char* p = (in ? in : ""); *p && j + 1 < cap; p++) out[j++] = (*p == '\\') ? '/' : *p;
    out[j] = 0;
    /* trim leading/trailing '/' */
    size_t l = strlen(out);
    while (l > 0 && out[l - 1] == '/') out[--l] = 0;
    if (out[0] == '/') memmove(out, out + 1, strlen(out));
}

static int starts_ci(const char* s, const char* pre)
{
    return strncasecmp(s, pre, strlen(pre)) == 0;
}

int gmb_pkg_glob_match(const char* pattern, const char* relPath)
{
    char pb[1024], rb[4096];
    norm(pattern, pb, sizeof(pb));
    norm(relPath, rb, sizeof(rb));
    if (!pb[0]) return 0;
    if (strcasecmp(pb, rb) == 0) return 1;                      /* r == p */
    size_t pl = strlen(pb);
    if (strlen(rb) > pl && starts_ci(rb, pb) && rb[pl] == '/') return 1; /* r inizia con p + '/' */
    if (!strchr(pb, '/')) {                                     /* pattern senza '/': matcha il basename */
        const char* slash = strrchr(rb, '/');
        if (glob_match(pb, slash ? slash + 1 : rb)) return 1;
    }
    return glob_match(pb, rb);
}
