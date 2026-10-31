/* gmb_matcher.c — Matcher SIMD/DFA per stringhe letterali (implementazione).
 *
 * Design originale: DFA a classi di byte (skip table Boyer-Moore-Horspool) + scansione
 * vettoriale dell'ancora (SSE2 su x86, NEON su ARM), fallback scalare a memchr.
 * Nessuna libreria esterna (solo intrinsics + libc). Sorgente UNICA. */
#include "gmb_matcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif

struct gmb_matcher {
    unsigned char* pat;
    size_t n;
    int icase;
    int word;
    int skip[256];        /* DFA BMH: shift massimo sicuro per classe di byte */
    unsigned char anchor; /* primo byte (fold) */
};

static inline unsigned char gg_fold(unsigned char c, int icase) {
    return icase ? (unsigned char)tolower(c) : c;
}
static inline int gg_isword(unsigned char c) {
    return isalnum(c) || c == '_';
}

gmb_matcher_t* gmb_matcher_new(const char* pattern, int ignore_case, int whole_word) {
    if (!pattern || !*pattern) return NULL;
    size_t n = strlen(pattern);
    gmb_matcher_t* m = (gmb_matcher_t*)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->pat = (unsigned char*)malloc(n);
    if (!m->pat) { free(m); return NULL; }
    for (size_t i = 0; i < n; i++) m->pat[i] = (unsigned char)pattern[i];
    m->n = n; m->icase = ignore_case; m->word = whole_word;
    m->anchor = gg_fold(m->pat[0], ignore_case);
    /* DFA BMH: per ogni byte la distanza dalla sua ultima occorrenza in pat[0..n-2] */
    for (int c = 0; c < 256; c++) m->skip[c] = (int)n;
    for (size_t i = 0; i + 1 < n; i++) m->skip[gg_fold(m->pat[i], ignore_case)] = (int)(n - 1 - i);
    return m;
}

void gmb_matcher_free(gmb_matcher_t* m) {
    if (!m) return;
    free(m->pat);
    free(m);
}

int gmb_matcher_valid(const gmb_matcher_t* m) { return m && m->n > 0; }

/* verifica il candidato a `h` (h[0..n) == pat, con fold); il word-check usa il contesto */
static int gg_verify(const gmb_matcher_t* m, const unsigned char* h, const unsigned char* base, size_t len) {
    for (size_t i = 0; i < m->n; i++)
        if (gg_fold(h[i], m->icase) != gg_fold(m->pat[i], m->icase)) return 0;
    if (m->word) {
        size_t idx = (size_t)(h - base);
        if (idx > 0 && gg_isword(base[idx - 1])) return 0;
        size_t e = idx + m->n;
        if (e < len && gg_isword(base[e])) return 0;
    }
    return 1;
}

/* scansione SIMD dell'ancora in [h, h+scan_len); ritorna il puntatore o NULL */
static const unsigned char* gg_scan_anchor(const gmb_matcher_t* m,
                                           const unsigned char* h, size_t scan_len) {
    unsigned char a = m->anchor;
#if defined(__SSE2__)
    const unsigned char* p = h;
    const unsigned char* vlim = h + (scan_len & ~(size_t)15);
    __m128i va = _mm_set1_epi8((char)a);
    while (p < vlim) {
        __m128i v = _mm_loadu_si128((const __m128i*)p);
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(v, va));
        if (mask) return p + __builtin_ctz((unsigned)mask);
        p += 16;
    }
    const unsigned char* q = (const unsigned char*)memchr(p, a, (size_t)((h + scan_len) - p));
    return q;
#elif defined(__ARM_NEON)
    const unsigned char* p = h;
    const unsigned char* vlim = h + (scan_len & ~(size_t)15);
    uint8x16_t va = vdupq_n_u8(a);
    while (p < vlim) {
        uint8x16_t v = vld1q_u8(p);
        uint8x16_t eq = vceqq_u8(v, va);
        uint8_t tmp[16];
        vst1q_u8(tmp, eq);
        for (int i = 0; i < 16; i++) if (tmp[i]) return p + i;
        p += 16;
    }
    const unsigned char* q = (const unsigned char*)memchr(p, a, (size_t)((h + scan_len) - p));
    return q;
#else
    return (const unsigned char*)memchr(h, a, scan_len);
#endif
}

long gmb_matcher_find(gmb_matcher_t* m, const char* hay, size_t len, long* out_index, long* out_len) {
    if (!m || !hay || m->n == 0 || len < m->n) return -1;
    const unsigned char* base = (const unsigned char*)hay;
    size_t n = m->n;
    size_t last = len - n + 1;   /* numero di posizioni di start valide */
    size_t i = 0;
    while (i < last) {
        /* 1. DFA/ancora: trova la prossima posizione il cui primo byte combacia */
        const unsigned char* cand;
        if (!m->icase) {
            cand = gg_scan_anchor(m, base + i, last - i);
        } else {
            cand = base + i;
            while ((size_t)(cand - base) < last && gg_fold(*cand, 1) != m->anchor) cand++;
            if ((size_t)(cand - base) >= last) cand = NULL;
        }
        if (!cand) return -1;
        i = (size_t)(cand - base);
        /* 2. verifica + word */
        if (gg_verify(m, base + i, base, len)) {
            if (out_index) *out_index = (long)i;
            if (out_len) *out_len = (long)n;
            return (long)i;
        }
        /* 3. near-miss: salta secondo la DFA BMH sull'ultimo byte della finestra */
        int sk = m->skip[gg_fold(base[i + n - 1], m->icase)];
        i += (size_t)(sk < 1 ? 1 : sk);
    }
    return -1;
}

long gmb_matcher_count(gmb_matcher_t* m, const char* hay, size_t len) {
    long count = 0;
    size_t pos = 0;
    while (pos <= len) {
        long idx = -1, ml = 0;
        if (gmb_matcher_find(m, hay + pos, len - pos, &idx, &ml) < 0) break;
        count++;
        pos += (size_t)idx + (ml > 0 ? (size_t)ml : 1);
    }
    return count;
}

int gmb_matcher_selftest(void) {
    int fail = 0;
    /* 1. letterale semplice */
    gmb_matcher_t* m = gmb_matcher_new("needle", 0, 0);
    {
        const char* hay = "a needle in a haystack with another needle";
        long idx = -1, ln = 0;
        long r = gmb_matcher_find(m, hay, strlen(hay), &idx, &ln);
        if (r != 2 || idx != 2 || ln != 6) { fprintf(stderr, "m1 %ld\n", r); fail = 1; }
        if (gmb_matcher_count(m, hay, strlen(hay)) != 2) { fprintf(stderr, "m1c\n"); fail = 1; }
    }
    gmb_matcher_free(m);
    /* 2. ignore-case */
    m = gmb_matcher_new("bar", 1, 0);
    {
        const char* hay = "foo BAR baz Bar";
        long idx = -1;
        if (gmb_matcher_find(m, hay, strlen(hay), &idx, NULL) != 4) { fprintf(stderr, "m2 %ld\n", idx); fail = 1; }
        if (gmb_matcher_count(m, hay, strlen(hay)) != 2) { fprintf(stderr, "m2c\n"); fail = 1; }
    }
    gmb_matcher_free(m);
    /* 3. whole-word */
    m = gmb_matcher_new("cat", 0, 1);
    {
        const char* hay = "concatenate a cat category";
        long idx = -1;
        if (gmb_matcher_find(m, hay, strlen(hay), &idx, NULL) != 14) { fprintf(stderr, "m3 %ld\n", idx); fail = 1; }
        if (gmb_matcher_count(m, hay, strlen(hay)) != 1) { fprintf(stderr, "m3c\n"); fail = 1; }
    }
    gmb_matcher_free(m);
    /* 4. pattern lungo con ancore vicine (stress SIMD 16-byte boundaries) */
    m = gmb_matcher_new("ZZZZZZZZZZZZZZZZZZneedle", 0, 0);
    {
        char hay[128]; memset(hay, 'x', sizeof(hay));
        const char* pat = "ZZZZZZZZZZZZZZZZZZneedle";
        size_t pl = strlen(pat);
        memcpy(hay + 37, pat, pl);
        long idx = -1;
        if (gmb_matcher_find(m, hay, sizeof(hay), &idx, NULL) != 37) { fprintf(stderr, "m4 %ld\n", idx); fail = 1; }
    }
    gmb_matcher_free(m);
    /* 5. non trovato + pattern vuoto */
    m = gmb_matcher_new("absent", 0, 0);
    { if (gmb_matcher_find(m, "abcdef", 6, NULL, NULL) != -1) { fprintf(stderr, "m5\n"); fail = 1; } }
    gmb_matcher_free(m);
    if (gmb_matcher_new("", 0, 0) != NULL) { fprintf(stderr, "m6\n"); fail = 1; }
    return fail;
}
