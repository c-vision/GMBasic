/* gmb_semver.c — vedi gmb_semver.h. Replica di `Parser.PackageManager.SemVer`,
 * `ParseRange`, `SemVerPre` e `Satisfies` del lato C#: stessa grammatica, stesse regole
 * di prerelease, stesso esito. Qualsiasi modifica deve toccare ENTRAMBI i lati.
 */
#include "gmb_semver.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- SemVerPre.IsValid: identificatori alfanumerici, numerici senza zeri iniziali ---- */

static int pre_is_ident(const char* id, size_t len)
{
    size_t i;
    int all_digits = 1;
    if (len == 0) return 0;
    if (len > GMB_SEMVER_PRE_MAX - 1) return 0;
    for (i = 0; i < len; i++) {
        char c = id[i];
        if (!(isalnum((unsigned char)c) || c == '-')) return 0;
        if (!isdigit((unsigned char)c)) all_digits = 0;
    }
    /* C#: se l'identificatore e' TUTTO cifre, niente zeri iniziali ("01" non valido).
     * "0a" NON e' tutto cifre: resta valido (il test e' sul primo char solo SULLA forma
     * numerica — inverterlo renderebbe "0a" invalido e divergerebbe). */
    if (all_digits && len > 1 && id[0] == '0') return 0;
    return 1;
}

static int pre_is_valid(const char* pre)
{
    const char* p = pre;
    if (pre == NULL || *pre == 0) return 0;
    while (1) {
        const char* dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (!pre_is_ident(p, len)) return 0;
        if (!dot) return 1;
        p = dot + 1;
    }
}

/* ---- SemVerPre.Compare: numerici < alfanumerici, release > prerelease ---- */

static int ident_is_number(const char* s, size_t len, long* out)
{
    size_t i;
    long v = 0;
    for (i = 0; i < len; i++) if (!isdigit((unsigned char)s[i])) return 0;
    if (len == 0 || len > 9) return 0;          /* oltre 9 cifre: trattalo come non-numero */
    for (i = 0; i < len; i++) v = v * 10 + (s[i] - '0');
    *out = v;
    return 1;
}

static int pre_cmp(const char* a, const char* b)
{
    const char *pa = a, *pb = b;
    if (a == NULL && b == NULL) return 0;
    if (a == NULL) return 1;    /* release > prerelease */
    if (b == NULL) return -1;
    if (*a == 0 && *b == 0) return 0;
    while (1) {
        const char *da, *db;
        size_t la, lb;
        int na, nb;
        long va = 0, vb = 0;
        da = strchr(pa, '.');
        db = strchr(pb, '.');
        la = da ? (size_t)(da - pa) : strlen(pa);
        lb = db ? (size_t)(db - pb) : strlen(pb);
        if (la == 0 && lb == 0) return 0;
        na = ident_is_number(pa, la, &va);
        nb = ident_is_number(pb, lb, &vb);
        if (na && nb) {
            if (va != vb) return va < vb ? -1 : 1;
        } else if (na) {
            return -1;   /* numerici < alfanumerici */
        } else if (nb) {
            return 1;
        } else {
            size_t m = la < lb ? la : lb;
            int c = m ? memcmp(pa, pb, m) : 0;
            if (c != 0) return c < 0 ? -1 : 1;
            if (la != lb) return la < lb ? -1 : 1;
        }
        if (!da && !db) return 0;
        if (!da) return -1;   /* meno identificatori -> minore */
        if (!db) return 1;
        pa = da + 1;
        pb = db + 1;
    }
}

/* ---- Parse ---- */

static const char* skip_v(const char* s)
{
    while (*s == ' ' || *s == '\t') s++;
    if (*s == 'v' || *s == 'V') s++;
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

int gmb_semver_parse(const char* version, gmb_semver_t* out)
{
    char buf[128];
    const char *s, *plus, *dash, *parts[3];
    size_t plen[3];
    int i;
    if (version == NULL || out == NULL) return 0;
    s = skip_v(version);
    /* trim finale */
    {
        size_t n = strlen(s);
        while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) n--;
        if (n == 0 || n >= sizeof buf) return 0;
        memcpy(buf, s, n);
        buf[n] = 0;
    }
    s = buf;
    memset(out, 0, sizeof(*out));
    plus = strchr(s, '+');
    if (plus) {
        size_t n = strlen(plus + 1);
        if (n >= sizeof out->build) n = sizeof out->build - 1;
        memcpy(out->build, plus + 1, n);
        out->build[n] = 0;
        ((char*)s)[plus - buf] = 0;
    }
    dash = strchr(s, '-');
    if (dash) {
        size_t n = strlen(dash + 1);
        if (n >= sizeof out->prerelease) n = sizeof out->prerelease - 1;
        memcpy(out->prerelease, dash + 1, n);
        out->prerelease[n] = 0;
        if (!pre_is_valid(out->prerelease)) return 0;
        ((char*)s)[dash - buf] = 0;
    }
    parts[0] = s;
    for (i = 0; i < 2; i++) {
        char* dot = strchr(parts[i], '.');
        if (!dot) return 0;
        *dot = 0;
        parts[i + 1] = dot + 1;
    }
    for (i = 0; i < 3; i++) {
        char* end = NULL;
        long val;
        if (parts[i][0] == 0) return 0;
        val = strtol(parts[i], &end, 10);
        if (end == parts[i] || *end != 0) return 0;
        if (i == 0) out->major = (int)val;
        else if (i == 1) out->minor = (int)val;
        else out->patch = (int)val;
        plen[i] = 0;
    }
    (void)plen;
    return 1;
}

int gmb_semver_is_prerelease(const gmb_semver_t* v)
{
    return (v != NULL && v->prerelease[0] != 0) ? 1 : 0;
}

int gmb_semver_cmp(const gmb_semver_t* a, const gmb_semver_t* b)
{
    if (a->major != b->major) return a->major < b->major ? -1 : 1;
    if (a->minor != b->minor) return a->minor < b->minor ? -1 : 1;
    if (a->patch != b->patch) return a->patch < b->patch ? -1 : 1;
    return pre_cmp(a->prerelease[0] ? a->prerelease : NULL,
                   b->prerelease[0] ? b->prerelease : NULL);
}

/* ---- ParseRange: 1 o 2 componenti vengono paddati a 3 (identico al C#) ---- */

static int parse_range(const char* version, gmb_semver_t* out)
{
    char tmp[128];
    const char *s = skip_v(version);
    size_t n;
    int dots = 0;
    size_t i;
    if (s == NULL || *s == 0) return 0;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    if (n == 0 || n >= sizeof tmp) return 0;
    memcpy(tmp, s, n);
    tmp[n] = 0;
    for (i = 0; i < n; i++) if (tmp[i] == '.') dots++;
    /* componenti = dots+1: il C# padda a 3 quando sono 1 o 2 ("1" -> "1.0.0", "1.2" -> "1.2.0") */
    {
        int comps = (int)dots + 1;
        if (comps == 1 || comps == 2) {
            size_t k = strlen(tmp);
            int need = 3 - comps;
            while (need-- > 0 && k + 2 < sizeof tmp) { tmp[k++] = '.'; tmp[k++] = '0'; }
            tmp[k] = 0;
        }
    }
    return gmb_semver_parse(tmp, out);
}

/* ---- Satisfies ---- */

static char* rtrim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    return s;
}

static int satisfies_one(const gmb_semver_t* v, const char* constraint);

static int satisfies_one(const gmb_semver_t* v, const char* raw)
{
    char c[160];
    const char *or;
    gmb_semver_t req;
    size_t n;

    snprintf(c, sizeof c, "%s", raw ? raw : "");
    rtrim(c);
    /* trim iniziale */
    { char* p = c; while (*p == ' ' || *p == '\t') p++; if (p != c) memmove(c, p, strlen(p) + 1); }
    if (c[0] == 0) return 1;
    if (!strcmp(c, "*") || !strcasecmp(c, "latest")) return 1;

    /* OR: "a || b" -> almeno uno soddisfatto (verificato PRIMA di ogni altro ramo,
     * come nel C#). */
    or = strstr(c, "||");
    if (or) {
        size_t left = (size_t)(or - c);
        char l[160];
        if (left >= sizeof l) left = sizeof l - 1;
        memcpy(l, c, left);
        l[left] = 0;
        rtrim(l);
        if (satisfies_one(v, l)) return 1;
        return satisfies_one(v, or + 2);
    }

    /* wildcard "1.*" (solo major) / "1.2.*" (major+minor) — mai prerelease.
     * Il C# conta i componenti del prefisso: 1 -> major, 2 -> major+minor, altro -> false. */
    n = strlen(c);
    if (n >= 2 && c[n - 2] == '.' && c[n - 1] == '*') {
        char pre[64];
        int ncomp = 1, maj = 0, min = 0;
        char *end = NULL, *dot;
        size_t pl = n - 2, i2;
        if (pl == 0 || pl >= sizeof pre) return 0;
        memcpy(pre, c, pl);
        pre[pl] = 0;
        rtrim(pre);
        for (i2 = 0; i2 < pl; i2++) if (pre[i2] == '.') ncomp++;
        if (gmb_semver_is_prerelease(v)) return 0;
        maj = (int)strtol(pre, &end, 10);
        if (end == pre || (end && *end != 0 && ncomp == 1)) return 0;
        if (ncomp == 1) return v->major == maj;
        if (ncomp == 2) {
            dot = strchr(pre, '.');
            if (!dot) return 0;
            min = (int)strtol(dot + 1, &end, 10);
            if (end == dot + 1 || (end && *end != 0)) return 0;
            return v->major == maj && v->minor == min;
        }
        return 0;
    }

    if (c[0] == '^') {
        if (!parse_range(c + 1, &req)) return 0;
        if (gmb_semver_is_prerelease(&req)) {
            if (gmb_semver_cmp(v, &req) == 0) return 1;
            return v->major == req.major && v->minor == req.minor &&
                   v->patch == req.patch && !strcmp(v->prerelease, req.prerelease);
        }
        if (gmb_semver_is_prerelease(v)) return 0;
        return v->major == req.major && gmb_semver_cmp(v, &req) >= 0;
    }
    if (c[0] == '~') {
        if (!parse_range(c + 1, &req)) return 0;
        if (gmb_semver_is_prerelease(&req)) return gmb_semver_cmp(v, &req) == 0;
        if (gmb_semver_is_prerelease(v)) return 0;
        return v->major == req.major && v->minor == req.minor && gmb_semver_cmp(v, &req) >= 0;
    }
    if (!strncmp(c, ">=", 2)) { if (!parse_range(c + 2, &req)) return 0; return gmb_semver_cmp(v, &req) >= 0; }
    if (c[0] == '>')          { if (!parse_range(c + 1, &req)) return 0; return gmb_semver_cmp(v, &req) >  0; }
    if (!strncmp(c, "<=", 2)) { if (!parse_range(c + 2, &req)) return 0; return gmb_semver_cmp(v, &req) <= 0; }
    if (c[0] == '<')          { if (!parse_range(c + 1, &req)) return 0; return gmb_semver_cmp(v, &req) <  0; }

    /* esatta */
    if (!gmb_semver_parse(c, &req)) return 0;
    return gmb_semver_cmp(v, &req) == 0 ? 1 : 0;
}

int gmb_semver_satisfies(const gmb_semver_t* v, const char* constraint)
{
    if (v == NULL) return 0;
    if (constraint == NULL) return 0;
    return satisfies_one(v, constraint);
}