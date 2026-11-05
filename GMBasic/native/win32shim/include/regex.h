/* regex.h (Windows) — API POSIX `regcomp/regexec/regfree` su PCRE2.
 *
 * Windows non ha <regex.h>. I nostri sorgenti usano solo il sottoinsieme POSIX:
 *   regex_t, regmatch_t, REG_EXTENDED | REG_ICASE, regcomp, regexec(nmatch/match), regfree.
 * Qui si mappa su PCRE2-8 (la stessa libreria che il runtime usa per REGEX.*), così il
 * comportamento delle espressioni è coerente con il resto del linguaggio.
 *
 * Header-only con funzioni `static inline`: serve solo `-lpcre2-8` al link.
 */
#ifndef GMB_WIN32_REGEX_H
#define GMB_WIN32_REGEX_H
#if defined(_WIN32) || defined(_WIN64)
#include "gmb_win32.h"
/* pcre2.h espande i nomi con il suffisso della width: va definita PRIMA dell'include. */
#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#endif
#include <pcre2.h>

typedef struct {
    pcre2_code* re;
} regex_t;

typedef struct {
    long rm_so;
    long rm_eo;
} regmatch_t;

#ifndef REG_EXTENDED
#define REG_EXTENDED 0x0001
#endif
#ifndef REG_ICASE
#define REG_ICASE    0x0002
#endif
#ifndef REG_NOSUB
#define REG_NOSUB    0x0004
#endif
#ifndef REG_NEWLINE
#define REG_NEWLINE  0x0008
#endif
#ifndef REG_NOTBOL
#define REG_NOTBOL   0x0001
#endif

static inline int regcomp(regex_t* r, const char* pattern, int flags)
{
    int opts = 0;
    int errcode = 0;
    PCRE2_SIZE erroff = 0;
    if (!r || !pattern) return 1;
    if (flags & REG_ICASE) opts |= PCRE2_CASELESS;
    if (flags & REG_NEWLINE) opts |= PCRE2_MULTILINE;
    r->re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, opts,
                          &errcode, &erroff, NULL);
    return r->re ? 0 : (errcode ? errcode : 1);
}

static inline int regexec(const regex_t* r, const char* subject, size_t nmatch,
                          regmatch_t* pmatch, int eflags)
{
    pcre2_match_data* md;
    int rc;
    (void)eflags;
    if (!r || !r->re) return 1;
    md = pcre2_match_data_create_from_pattern(r->re, NULL);
    if (!md) return 1;
    rc = pcre2_match(r->re, (PCRE2_SPTR)subject, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    if (rc >= 0 && pmatch) {
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        for (size_t i = 0; i < nmatch && i < (size_t)rc; i++) {
            pmatch[i].rm_so = (long)ov[2 * i];
            pmatch[i].rm_eo = (long)ov[2 * i + 1];
        }
    }
    pcre2_match_data_free(md);
    return rc >= 0 ? 0 : 1;
}

static inline size_t regerror(int errcode, const regex_t* r, char* buf, size_t cap)
{
    (void)r;
    if (buf && cap) snprintf(buf, cap, "regex error %d", errcode);
    return buf ? strlen(buf) : 0;
}

static inline void regfree(regex_t* r)
{
    if (r && r->re) { pcre2_code_free(r->re); r->re = NULL; }
}
#endif /* _WIN32 */
#endif /* GMB_WIN32_REGEX_H */
