/* gmb_semver.c — SemVer 2.0 + range matching (riferimento C del Package Manager).
 * Parità 1:1 con Parser.PackageManager.SemVer (C#): vedi selfcheck per i casi. */
#include "gmb_semver.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

static void copy_trunc(char* dst, size_t cap, const char* src)
{
    if (!dst || cap == 0) return;
    snprintf(dst, cap, "%s", src ? src : "");
}

static int parse_uint(const char* s, int* out)
{
    if (!s || !*s) return 0;
    long v = 0;
    for (const char* p = s; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        v = v * 10 + (*p - '0');
        if (v > 2000000000L) return 0;
    }
    *out = (int)v;
    return 1;
}

static int is_all_digits(const char* s)
{
    if (!s || !*s) return 0;
    for (const char* p = s; *p; p++) if (*p < '0' || *p > '9') return 0;
    return 1;
}

/* Ident prerelease valido (SemVer 2.0): all-digits senza leading zero (se len>1), oppure [0-9A-Za-z-]+. */
static int ident_valid(const char* id)
{
    if (!id || !*id) return 0;
    if (is_all_digits(id)) return strlen(id) == 1 || id[0] != '0';
    for (const char* p = id; *p; p++) {
        char c = *p;
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-'))
            return 0;
    }
    return 1;
}

static int pre_valid(const char* pre)
{
    char buf[128];
    copy_trunc(buf, sizeof(buf), pre);
    char* save = NULL;
    int any = 0;
    for (char* t = strtok_r(buf, ".", &save); t; t = strtok_r(NULL, ".", &save)) {
        if (!ident_valid(t)) return 0;
        any = 1;
    }
    return any;
}

int gmb_semver_parse(const char* s, gmb_semver_t* out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!s) return 0;
    const char* p = s;
    while (*p == 'v' || *p == ' ') p++;
    char buf[512];
    copy_trunc(buf, sizeof(buf), p);
    size_t bl = strlen(buf);
    while (bl > 0 && buf[bl - 1] == ' ') buf[--bl] = 0;

    char* plus = strchr(buf, '+');
    if (plus) { copy_trunc(out->build, sizeof(out->build), plus + 1); *plus = 0; }
    char* dash = strchr(buf, '-');
    if (dash) { copy_trunc(out->prerelease, sizeof(out->prerelease), dash + 1); *dash = 0; }

    char* parts[4]; int n = 0;
    char* save = NULL;
    for (char* t = strtok_r(buf, ".", &save); t && n < 4; t = strtok_r(NULL, ".", &save)) parts[n++] = t;
    if (n != 3) return 0;
    if (!parse_uint(parts[0], &out->major)) return 0;
    if (!parse_uint(parts[1], &out->minor)) return 0;
    if (!parse_uint(parts[2], &out->patch)) return 0;
    if (dash) { if (!pre_valid(out->prerelease)) return 0; out->has_pre = 1; }
    return 1;
}

int gmb_semver_parse_range(const char* s, gmb_semver_t* out)
{
    if (!s || !*s) { if (out) memset(out, 0, sizeof(*out)); return 0; }
    const char* p = s;
    while (*p == 'v' || *p == ' ') p++;
    char trimmed[512];
    copy_trunc(trimmed, sizeof(trimmed), p);
    size_t tl = strlen(trimmed);
    while (tl > 0 && trimmed[tl - 1] == ' ') trimmed[--tl] = 0;
    int dots = 0;
    for (size_t i = 0; i < tl; i++) if (trimmed[i] == '.') dots++;
    int n = dots + 1;
    if (n == 1 || n == 2) {
        char padded[512];
        copy_trunc(padded, sizeof(padded), trimmed);
        for (int i = n; i < 3; i++) strncat(padded, ".0", sizeof(padded) - strlen(padded) - 1);
        return gmb_semver_parse(padded, out);
    }
    return gmb_semver_parse(s, out);
}

/* Precedenza prerelease: release > prerelease; confronto per identificatori. */
static int pre_cmp(const char* a, int a_has, const char* b, int b_has)
{
    if (!a_has && !b_has) return 0;
    if (!a_has) return 1;
    if (!b_has) return -1;
    char ab[128], bb[128];
    copy_trunc(ab, sizeof(ab), a);
    copy_trunc(bb, sizeof(bb), b);
    char* sa = NULL; char* sb = NULL;
    char* ai = strtok_r(ab, ".", &sa);
    char* bi = strtok_r(bb, ".", &sb);
    while (ai || bi) {
        if (!ai) return -1;
        if (!bi) return 1;
        int an = is_all_digits(ai), bn = is_all_digits(bi);
        if (an && bn) { long av = atol(ai), bv = atol(bi); if (av != bv) return av < bv ? -1 : 1; }
        else if (an) return -1;
        else if (bn) return 1;
        else { int c = strcmp(ai, bi); if (c) return c < 0 ? -1 : 1; }
        ai = strtok_r(NULL, ".", &sa);
        bi = strtok_r(NULL, ".", &sb);
    }
    return 0;
}

int gmb_semver_cmp(const gmb_semver_t* a, const gmb_semver_t* b)
{
    if (a->major != b->major) return a->major < b->major ? -1 : 1;
    if (a->minor != b->minor) return a->minor < b->minor ? -1 : 1;
    if (a->patch != b->patch) return a->patch < b->patch ? -1 : 1;
    return pre_cmp(a->prerelease, a->has_pre, b->prerelease, b->has_pre);
}

/* Uguaglianza "esatta" che INCLUDE il build metadata. NON va usata per il matching dei range:
 * SemVer 2.0 §10 dice che il build è ignorato nel confronto di precedenza, e quindi anche nel
 * soddisfacimento di un vincolo ("1.2.3+b5" soddisfa "1.2.3" — comportamento npm). Usare questa
 * funzione dentro satisfies() faceva divergere il resolver dal compilatore C (che, come npm,
 * ignora il build): `REQUIRE "x" VERSION "1.2.3"` non trovava la versione pubblicata 1.2.3+b5.
 * Per il matching si usa la PRECEDENZA (gmb_semver_cmp == 0). */
int gmb_semver_eq(const gmb_semver_t* a, const gmb_semver_t* b)
{
    return gmb_semver_cmp(a, b) == 0 && strcmp(a->build, b->build) == 0 && a->has_pre == b->has_pre;
}

int gmb_semver_format(const gmb_semver_t* v, char* out, size_t cap)
{
    if (!out || cap == 0 || !v) return 0;
    if (v->has_pre) snprintf(out, cap, "%d.%d.%d-%s", v->major, v->minor, v->patch, v->prerelease);
    else snprintf(out, cap, "%d.%d.%d", v->major, v->minor, v->patch);
    return 1;
}

static int starts(const char* s, const char* pre)
{
    return strncmp(s, pre, strlen(pre)) == 0;
}

static int satisfies_inner(const gmb_semver_t* v, const char* constraint)
{
    if (!constraint) return 0;
    char c[512];
    copy_trunc(c, sizeof(c), constraint);
    char* s = c;
    while (*s == ' ' || *s == '\t') s++;
    size_t l = strlen(s);
    while (l > 0 && (s[l - 1] == ' ' || s[l - 1] == '\t')) s[--l] = 0;

    if (strcmp(s, "*") == 0 || strcmp(s, "") == 0 || strcmp(s, "latest") == 0) return 1;

    if (strstr(s, "||")) {
        char buf[512];
        copy_trunc(buf, sizeof(buf), s);
        char* save = NULL;
        for (char* t = strtok_r(buf, "|", &save); t; t = strtok_r(NULL, "|", &save)) {
            char* tt = t;
            while (*tt == ' ') tt++;
            size_t tl = strlen(tt);
            while (tl > 0 && tt[tl - 1] == ' ') tt[--tl] = 0;
            if (*tt && satisfies_inner(v, tt)) return 1;
        }
        return 0;
    }

    if (s[0] == '^') {
        gmb_semver_t req;
        if (!gmb_semver_parse_range(s + 1, &req)) return 0;
        if (req.has_pre)
            return gmb_semver_cmp(v, &req) == 0;   /* build IGNORATO (SemVer 2.0 §10) */
        if (v->has_pre) return 0;
        return v->major == req.major && gmb_semver_cmp(v, &req) >= 0;
    }
    if (s[0] == '~') {
        gmb_semver_t req;
        if (!gmb_semver_parse_range(s + 1, &req)) return 0;
        if (req.has_pre) return gmb_semver_cmp(v, &req) == 0;   /* build IGNORATO */
        if (v->has_pre) return 0;
        return v->major == req.major && v->minor == req.minor && gmb_semver_cmp(v, &req) >= 0;
    }
    size_t sl = strlen(s);
    if (sl >= 2 && strcmp(s + sl - 2, ".*") == 0) {
        char prefix[512];
        copy_trunc(prefix, sizeof(prefix), s);
        prefix[sl - 2] = 0;
        size_t pl = strlen(prefix);
        while (pl > 0 && prefix[pl - 1] == '.') prefix[--pl] = 0;
        if (v->has_pre) return 0;
        char* dot = strchr(prefix, '.');
        if (!dot) { int mj; return parse_uint(prefix, &mj) ? v->major == mj : 0; }
        *dot = 0;
        int mj, mn;
        return (parse_uint(prefix, &mj) && parse_uint(dot + 1, &mn)) ? (v->major == mj && v->minor == mn) : 0;
    }

    if (starts(s, ">=")) { gmb_semver_t r; return gmb_semver_parse_range(s + 2, &r) && gmb_semver_cmp(v, &r) >= 0; }
    if (starts(s, "<=")) { gmb_semver_t r; return gmb_semver_parse_range(s + 2, &r) && gmb_semver_cmp(v, &r) <= 0; }
    if (starts(s, ">"))  { gmb_semver_t r; return gmb_semver_parse_range(s + 1, &r) && gmb_semver_cmp(v, &r) > 0; }
    if (starts(s, "<"))  { gmb_semver_t r; return gmb_semver_parse_range(s + 1, &r) && gmb_semver_cmp(v, &r) < 0; }

    gmb_semver_t ex;
    if (!gmb_semver_parse(s, &ex)) return 0;
    /* PRECEDENZA, non uguaglianza esatta: "1.2.3+b5" soddisfa "1.2.3" (build ignorato,
     * SemVer 2.0 §10 / npm). Era l'unica divergenza reale col gmb_semver del frontend C. */
    return gmb_semver_cmp(v, &ex) == 0;
}

int gmb_semver_satisfies(const gmb_semver_t* v, const char* range)
{
    return satisfies_inner(v, range);
}

int gmb_semver_find_best(int n_constraints, const char* const* constraints,
                         int n_versions, const char* const* versions,
                         char* out, size_t cap)
{
    int best = -1;
    gmb_semver_t bestv;
    memset(&bestv, 0, sizeof(bestv));
    for (int i = 0; i < n_versions; i++) {
        gmb_semver_t v;
        if (!gmb_semver_parse(versions[i], &v)) continue;
        int ok = 1;
        for (int k = 0; k < n_constraints; k++)
            if (!gmb_semver_satisfies(&v, constraints[k])) { ok = 0; break; }
        if (!ok) continue;
        if (best < 0 || gmb_semver_cmp(&v, &bestv) > 0) { best = i; bestv = v; }
    }
    if (best < 0) { if (cap) out[0] = 0; return 0; }
    copy_trunc(out, cap, versions[best]);
    return 1;
}
