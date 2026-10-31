/* ============================================================================
 * gmb_strings.c — builtin stringhe (StringUtils C# / builtin BASIC).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * Tutte le funzioni usano gmb_string_t (struct {data,length,ref_count}).
 * ============================================================================ */

#include "gmb_runtime.h"
#include "gmb_tui.h"
#include "gmb_commonlib.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── CARATTERI (code point Unicode) vs BYTE ─────────────────────────────────
 * LEN conta i CARATTERI (code point): "─" = 1, "é" = 1, "😀" = 1 (4 byte in UTF-8).
 * LENB conta i BYTE della codifica UTF-8 (l'unica cosa che serve per buffer/file/limiti).
 * Prima le gambe C contavano i byte anche per LEN (LEN("┌─┐")=9) e le managed le unità UTF-16
 * (LEN("😀")=2): entrambe erano "il carattere sbagliato". Vedi
 * Bugs/(solved) - LEN-UTF8-Byte-VS-Char-Gambe-C. */

/* numero di CARATTERI nei primi `maxbytes` byte (SIZE_MAX = fino al NUL) */
gmb_int_t gmb_chars_len_n(const char* s, size_t maxbytes) {
    gmb_int_t chars = 0;
    size_t i = 0;
    if (!s) return 0;
    while (s[i]) {
        unsigned char b = (unsigned char)s[i];
        int nb;
        if (b < 0x80) nb = 1;
        else if ((b & 0xE0) == 0xC0) nb = 2;
        else if ((b & 0xF0) == 0xE0) nb = 3;
        else if ((b & 0xF8) == 0xF0) nb = 4;
        else nb = 1;                    /* byte non valido: conta 1, avanza 1 */
        if (i + (size_t)nb > maxbytes) break;
        i += (size_t)nb;
        chars += 1;                     /* 1 CARATTERE, indipendentemente dai byte */
    }
    return chars;
}

gmb_int_t gmb_chars_len(const char* s) {
    return gmb_chars_len_n(s, (size_t)-1);
}

/* offset in BYTE del `chars`-esimo carattere (0..len). Oltre la fine → strlen. */
size_t gmb_chars_to_byte(const char* s, gmb_int_t chars) {
    size_t i = 0;
    gmb_int_t c = 0;
    if (!s || chars <= 0) return 0;
    while (s[i] && c < chars) {
        unsigned char b = (unsigned char)s[i];
        int nb;
        if (b < 0x80) nb = 1;
        else if ((b & 0xE0) == 0xC0) nb = 2;
        else if ((b & 0xF0) == 0xE0) nb = 3;
        else if ((b & 0xF8) == 0xF0) nb = 4;
        else nb = 1;
        i += (size_t)nb;
        c += 1;
    }
    return i;
}

/* LEN di una stringa: CARATTERI (code point). */
gmb_int_t gmb_str_t_len(gmb_string_t s) {
    return gmb_chars_len(s.data);
}

/* LENB di una stringa: BYTE UTF-8 (la lunghezza memorizzata). */
gmb_int_t gmb_str_bytes(gmb_string_t s) {
    return s.data ? (gmb_int_t)s.length : 0;
}

/* LEFT(s, n): primi n caratteri (unità UTF-16). */
gmb_string_t gmb_str_left(gmb_string_t s, gmb_int_t n) {
    size_t bytes;
    if (!s.data) return gmb_string_alloc("");
    if (n < 0) n = 0;
    bytes = gmb_chars_to_byte(s.data, n);
    if (bytes > s.length) bytes = s.length;
    {
        char* buf = (char*)malloc(bytes + 1);
        if (!buf) return gmb_string_alloc("");
        memcpy(buf, s.data, bytes);
        buf[bytes] = 0;
        {
            gmb_string_t r = gmb_string_alloc(buf);
            free(buf);
            return r;
        }
    }
}

/* RIGHT(s, n): ultimi n caratteri (unità UTF-16). */
gmb_string_t gmb_str_right(gmb_string_t s, gmb_int_t n) {
    gmb_int_t units;
    size_t off;
    if (!s.data) return gmb_string_alloc("");
    if (n < 0) n = 0;
    units = gmb_chars_len(s.data);
    if (n > units) n = units;
    off = gmb_chars_to_byte(s.data, units - n);
    return gmb_str_left(gmb_string_alloc(s.data + off), n);
}

/* MID(s, start, len): sottostringa 1-based, indici in unità UTF-16. len<=0 → stringa vuota. */
gmb_string_t gmb_str_mid(gmb_string_t s, gmb_int_t start, gmb_int_t len) {
    gmb_int_t units;
    size_t off, off2, bytes;
    if (!s.data || start < 1) return gmb_string_alloc("");
    units = gmb_chars_len(s.data);
    if (start - 1 > units) return gmb_string_alloc("");
    if (len <= 0) return gmb_string_alloc("");
    off = gmb_chars_to_byte(s.data, start - 1);
    off2 = gmb_chars_to_byte(s.data, start - 1 + len);
    if (off2 > s.length) off2 = s.length;
    if (off2 < off) off2 = off;
    bytes = off2 - off;
    {
        char* buf = (char*)malloc(bytes + 1);
        if (!buf) return gmb_string_alloc("");
        memcpy(buf, s.data + off, bytes);
        buf[bytes] = 0;
        {
            gmb_string_t r = gmb_string_alloc(buf);
            free(buf);
            return r;
        }
    }
}

/* INSTR(s, sub, start): indice 1-based di sub in s (0 se non trovato).
   case-insensitive (OrdinalIgnoreCase), start oltre fine → 0. */
static const char* gmb_strstr_ci(const char* hay, const char* needle) {
    if (!hay || !needle) return NULL;
    size_t hl = strlen(hay), nl = strlen(needle);
    if (nl == 0) return hay;
    if (nl > hl) return NULL;
    for (size_t i = 0; i + nl <= hl; i++) {
        size_t j = 0;
        while (j < nl && tolower((unsigned char)hay[i+j]) == tolower((unsigned char)needle[j])) j++;
        if (j == nl) return hay + i;
    }
    return NULL;
}
gmb_int_t gmb_str_instr(gmb_string_t s, gmb_string_t sub, gmb_int_t start) {
    size_t off;
    const char* found;
    gmb_int_t units;
    if (!s.data || !sub.data || start < 1) return 0;
    units = gmb_chars_len(s.data);
    if (start - 1 > units) return 0;
    /* `start` è un indice in UNITÀ UTF-16 (come C#): si converte in offset di byte e si
     * riconverte in unità la posizione trovata (prima erano tutti byte). */
    off = gmb_chars_to_byte(s.data, start - 1);
    found = gmb_strstr_ci(s.data + off, sub.data);
    if (!found) return 0;
    return gmb_chars_len_n(s.data, (size_t)(found - s.data)) + 1;
}

/* REPEAT(s, count[, sep]): ripete s count volte, separate da sep. */
gmb_string_t gmb_str_repeat(gmb_string_t s, gmb_int_t count, gmb_string_t sep) {
    if (count <= 0 || !s.data) return gmb_string_alloc("");
    const char* sepstr = sep.data ? sep.data : "";
    size_t seplen = sep.data ? sep.length : 0;
    size_t total = (size_t)count * s.length + (size_t)(count - 1) * seplen;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    size_t off = 0;
    for (gmb_int_t i = 0; i < count; i++) {
        if (i > 0 && seplen > 0) { memcpy(buf + off, sepstr, seplen); off += seplen; }
        memcpy(buf + off, s.data, s.length); off += s.length;
    }
    buf[off] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* DUP(n, ch): ripete n volte il carattere (parity IlRuntime.Dup — il 2° argomento può
 * essere una stringa di cui si usa il primo char, o un codice numerico). */
/* Numero di byte del primo carattere UTF-8 di `s` (1 se vuoto/ASCII). */
static int utf8_first_len(const char* s) {
    unsigned char b;
    if (!s || !*s) return 0;
    b = (unsigned char)s[0];
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;
}

/* DUP(n, <stringa>): ripete n volte il PRIMO CARATTERE della stringa.
 * Prima si ripeteva un solo BYTE: con "─" (3 byte UTF-8) usciva una sequenza non valida
 * (box disallineati e file illeggibile sulla gamba C99Native). */
gmb_string_t gmb_dup_str(gmb_int_t n, const char* s) {
    int fl;
    char* buf;
    gmb_int_t i;
    gmb_string_t r;
    fl = utf8_first_len(s);
    if (fl == 0) return gmb_string_alloc("");
    if (n < 0) n = 0;
    buf = (char*)malloc((size_t)(n * fl) + 1);
    if (!buf) return gmb_string_alloc("");
    for (i = 0; i < n; i++) memcpy(buf + (size_t)i * (size_t)fl, s, (size_t)fl);
    buf[(size_t)n * (size_t)fl] = 0;
    r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* DUP(n, <codice numerico>): ripete n volte il code point (codificato in UTF-8). */
gmb_string_t gmb_dup_cp(gmb_int_t n, int cp) {
    char enc[4];
    int fl = 0;
    char* buf;
    gmb_int_t i;
    gmb_string_t r;
    if (cp < 0) cp = 0;
    if (cp > 0x10FFFF) cp = '?';
    if (cp < 0x80) { enc[0] = (char)cp; fl = 1; }
    else if (cp < 0x800) { enc[0] = (char)(0xC0 | (cp >> 6)); enc[1] = (char)(0x80 | (cp & 0x3F)); fl = 2; }
    else if (cp < 0x10000) { enc[0] = (char)(0xE0 | (cp >> 12)); enc[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); enc[2] = (char)(0x80 | (cp & 0x3F)); fl = 3; }
    else { enc[0] = (char)(0xF0 | (cp >> 18)); enc[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); enc[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); enc[3] = (char)(0x80 | (cp & 0x3F)); fl = 4; }
    if (n < 0) n = 0;
    buf = (char*)malloc((size_t)(n * fl) + 1);
    if (!buf) return gmb_string_alloc("");
    for (i = 0; i < n; i++) memcpy(buf + (size_t)i * (size_t)fl, enc, (size_t)fl);
    buf[(size_t)n * (size_t)fl] = 0;
    r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

gmb_string_t gmb_dup(gmb_int_t n, int ch) {
    return gmb_dup_cp(n, ch);
}

/* PADC/PADL/PADR(s, len[, fill]): padding a lunghezza len col primo char di fill.
   Semantica allineata a StringHelpers/CommonLib.StringUtils (IL): PADL/PADR non
   troncano se s è più lunga; PADC tronca a len. */
static char gmb_pad_fill(gmb_string_t fill) { return (fill.data && fill.length > 0) ? fill.data[0] : ' '; }
gmb_string_t gmb_string_padl(gmb_string_t s, gmb_int_t len, gmb_string_t fill) {
    if (!s.data) return gmb_string_alloc("");
    if ((gmb_int_t)s.length >= len) return gmb_string_alloc(s.data);
    char f = gmb_pad_fill(fill);
    size_t total = (size_t)len, sl = s.length, p = total - sl;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    for (size_t i = 0; i < p; i++) buf[i] = f;
    memcpy(buf + p, s.data, sl); buf[total] = 0;
    gmb_string_t r = gmb_string_alloc(buf); free(buf); return r;
}
gmb_string_t gmb_string_padr(gmb_string_t s, gmb_int_t len, gmb_string_t fill) {
    if (!s.data) return gmb_string_alloc("");
    if ((gmb_int_t)s.length >= len) return gmb_string_alloc(s.data);
    char f = gmb_pad_fill(fill);
    size_t total = (size_t)len, sl = s.length, p = total - sl;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    memcpy(buf, s.data, sl);
    for (size_t i = 0; i < p; i++) buf[sl + i] = f;
    buf[total] = 0;
    gmb_string_t r = gmb_string_alloc(buf); free(buf); return r;
}
gmb_string_t gmb_string_padc(gmb_string_t s, gmb_int_t len, gmb_string_t fill) {
    if (!s.data) return gmb_string_alloc("");
    char f = gmb_pad_fill(fill);
    size_t sl = s.length;
    if (sl >= (size_t)len) { char* t = (char*)malloc((size_t)len + 1); if(!t) return gmb_string_alloc(""); memcpy(t, s.data, (size_t)len); t[(size_t)len]=0; gmb_string_t r = gmb_string_alloc(t); free(t); return r; }
    size_t total = (size_t)len, padAmount = total - sl, pl = padAmount / 2, pr = padAmount - pl;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    for (size_t i = 0; i < pl; i++) buf[i] = f;
    memcpy(buf + pl, s.data, sl);
    for (size_t i = 0; i < pr; i++) buf[pl + sl + i] = f;
    buf[total] = 0;
    gmb_string_t r = gmb_string_alloc(buf); free(buf); return r;
}

/* STRING * N (parità IlRuntime.AdjustFixedString / OP_ADJUST_STRING PCodeVM):
   padding a destra con spazi se più corta, troncamento se più lunga. */
gmb_string_t gmb_string_adjust_fixed(gmb_string_t s, gmb_int_t len) {
    if (len <= 0) return s.data ? s : gmb_string_alloc("");
    if (!s.data) s = gmb_string_alloc("");
    if (s.length > (size_t)len) {
        char* t = (char*)malloc((size_t)len + 1);
        if (!t) return gmb_string_alloc("");
        memcpy(t, s.data, (size_t)len); t[(size_t)len] = 0;
        gmb_string_t r = gmb_string_alloc(t); free(t); return r;
    }
    if (s.length < (size_t)len) {
        size_t total = (size_t)len, p = total - s.length;
        char* buf = (char*)malloc(total + 1);
        if (!buf) return gmb_string_alloc("");
        memcpy(buf, s.data, s.length);
        for (size_t i = 0; i < p; i++) buf[s.length + i] = ' ';
        buf[total] = 0;
        gmb_string_t r = gmb_string_alloc(buf); free(buf); return r;
    }
    return gmb_string_alloc(s.data);
}

/* PETSCII(s): \r → \n, stampabili ASCII invariati, resto → '?'. Parità IlRuntime.Petscii. */
gmb_string_t gmb_str_petscii(gmb_string_t s) {
    if (!s.data) return gmb_string_alloc("");
    char* buf = (char*)malloc(s.length + 1);
    if (!buf) return gmb_string_alloc("");
    for (size_t i = 0; i < s.length; i++) {
        char c = s.data[i];
        buf[i] = (c == 0x0D) ? '\n' : ((c >= 0x20 && c <= 0x7E) ? c : '?');
    }
    buf[s.length] = 0;
    gmb_string_t r = gmb_string_alloc(buf); free(buf); return r;
}

/* gmb_str_isblank: 1 se vuota o solo whitespace (parità string.IsNullOrWhiteSpace). */
gmb_int_t gmb_str_isblank(gmb_string_t s) {
    if (!s.data) return 1;
    for (size_t i = 0; i < s.length; i++) {
        char c = s.data[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') return 0;
    }
    return 1;
}

/* TRIM/LTRIM/RTRIM: taglia spazi (e whitespace). */
gmb_string_t gmb_str_trim(gmb_string_t s) {
    if (!s.data) return gmb_string_alloc("");
    const char* b = s.data;
    while (*b && isspace((unsigned char)*b)) b++;
    const char* e = s.data + s.length;
    while (e > b && isspace((unsigned char)e[-1])) e--;
    char* buf = (char*)malloc((size_t)(e - b) + 1);
    if (!buf) return gmb_string_alloc("");
    size_t n = (size_t)(e - b);
    memcpy(buf, b, n); buf[n] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

gmb_string_t gmb_str_ltrim(gmb_string_t s) {
    if (!s.data) return gmb_string_alloc("");
    const char* b = s.data;
    while (*b && isspace((unsigned char)*b)) b++;
    return gmb_string_alloc(b);
}

gmb_string_t gmb_str_rtrim(gmb_string_t s) {
    if (!s.data) return gmb_string_alloc("");
    const char* e = s.data + s.length;
    while (e > s.data && isspace((unsigned char)e[-1])) e--;
    char* buf = (char*)malloc((size_t)(e - s.data) + 1);
    if (!buf) return gmb_string_alloc("");
    size_t n = (size_t)(e - s.data);
    memcpy(buf, s.data, n); buf[n] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

static gmb_string_t gmb_str_case(gmb_string_t s, int to_upper) {
    if (!s.data) return gmb_string_alloc("");
    char* buf = (char*)malloc(s.length + 1);
    if (!buf) return gmb_string_alloc("");
    for (size_t i = 0; i < s.length; i++) {
        unsigned char c = (unsigned char)s.data[i];
        buf[i] = to_upper ? (char)toupper(c) : (char)tolower(c);
    }
    buf[s.length] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

gmb_string_t gmb_str_upper(gmb_string_t s) { return gmb_str_case(s, 1); }
gmb_string_t gmb_str_lower(gmb_string_t s) { return gmb_str_case(s, 0); }

gmb_bool_t gmb_str_contains(gmb_string_t s, gmb_string_t sub) {
    return s.data && sub.data && strstr(s.data, sub.data) != NULL;
}

gmb_bool_t gmb_str_startswith(gmb_string_t s, gmb_string_t prefix) {
    if (!s.data || !prefix.data) return 0;
    size_t pl = prefix.length;
    if (s.length < pl) return 0;
    for (size_t i = 0; i < pl; i++)
        if (tolower((unsigned char)s.data[i]) != tolower((unsigned char)prefix.data[i])) return 0;
    return 1;
}

gmb_bool_t gmb_str_endswith(gmb_string_t s, gmb_string_t suffix) {
    if (!s.data || !suffix.data) return 0;
    size_t sl = suffix.length;
    if (s.length < sl) return 0;
    for (size_t i = 0; i < sl; i++)
        if (tolower((unsigned char)s.data[s.length - sl + i]) != tolower((unsigned char)suffix.data[i])) return 0;
    return 1;
}

/* REPLACE(s, old, new): sostituisce tutte le occorrenze. */
gmb_string_t gmb_str_replace(gmb_string_t s, gmb_string_t old, gmb_string_t neu) {
    if (!s.data) return gmb_string_alloc("");
    const char* neuptr = neu.data ? neu.data : "";
    size_t neulen = neu.data ? neu.length : 0;
    if (!old.data || old.length == 0) {
        /* C# string.Replace("", neu): inserisce neu in ogni posizione (incluso inizio/fine). */
        size_t total = s.length + neulen * (s.length + 1);
        char* buf = (char*)malloc(total + 1);
        if (!buf) return gmb_string_alloc("");
        char* w = buf;
        for (size_t i = 0; i <= s.length; i++) {
            memcpy(w, neuptr, neulen); w += neulen;
            if (i < s.length) *w++ = s.data[i];
        }
        *w = 0;
        gmb_string_t r = gmb_string_alloc(buf);
        free(buf);
        return r;
    }
    size_t count = 0;
    const char* p = s.data;
    while ((p = strstr(p, old.data)) != NULL) { count++; p += old.length; }
    size_t total = s.length + count * (neulen > old.length ? neulen - old.length : 0);
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    char* w = buf;
    p = s.data;
    const char* end = s.data + s.length;
    while (p < end) {
        const char* hit = strstr(p, old.data);
        if (!hit) { size_t rem = (size_t)(end - p); memcpy(w, p, rem); w += rem; break; }
        size_t pre = (size_t)(hit - p);
        memcpy(w, p, pre); w += pre;
        memcpy(w, neuptr, neulen); w += neulen;
        p = hit + old.length;
    }
    *w = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* COUNT(s, value): quante occorrenze di value in s. */
gmb_int_t gmb_str_count(gmb_string_t s, gmb_string_t value) {
    if (!s.data || !value.data || value.length == 0) return 0;
    gmb_int_t n = 0;
    const char* p = s.data;
    while ((p = strstr(p, value.data)) != NULL) { n++; p += value.length; }
    return n;
}

/* SPACE(n): n spazi. */
gmb_string_t gmb_str_space(gmb_int_t n) {
    if (n <= 0) return gmb_string_alloc("");
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) return gmb_string_alloc("");
    memset(buf, ' ', (size_t)n);
    buf[n] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* TAB(col): spazi fino alla colonna col (col 1 = inizio, tab ogni 8). */
gmb_string_t gmb_str_tab(gmb_int_t col) {
    /* TAB C#: ritorna "\t" (un carattere tab), ignora la colonna. */
    (void)col;
    return gmb_string_alloc("\t");
}

/* TITLE(s): prima lettera di ogni parola maiuscola, resto minuscolo. */
gmb_string_t gmb_str_title(gmb_string_t s) {
    if (!s.data) return gmb_string_alloc("");
    char* buf = (char*)malloc(s.length + 1);
    if (!buf) return gmb_string_alloc("");
    int newword = 1;
    for (size_t i = 0; i < s.length; i++) {
        unsigned char c = (unsigned char)s.data[i];
        if (isspace(c)) { buf[i] = (char)c; newword = 1; }
        else { buf[i] = newword ? (char)toupper(c) : (char)tolower(c); newword = 0; }
    }
    buf[s.length] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* WRAP(text, width, alignment): avvolge il testo alle parole (semplice). */
gmb_string_t gmb_str_wrap(gmb_string_t text, gmb_int_t width, gmb_int_t alignment) {
    /* Word-wrap: parita' CommonLib.StringUtils.Wrap — righe per PAROLE (spazio),
     * spezza solo le parole piu' lunghe di width; separatore righe "\n". */
    (void)alignment;
    if (width <= 0 || !text.data) return gmb_string_alloc("");
    size_t cap = text.length * 2 + (size_t)width + 16;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("");
    size_t o = 0;
    size_t line_len = 0;
    size_t i = 0;
    while (i < text.length) {
        while (i < text.length && text.data[i] == ' ') i++;
        if (i >= text.length) break;
        size_t wstart = i;
        while (i < text.length && text.data[i] != ' ') i++;
        size_t wlen = i - wstart;
        if (wlen > (size_t)width) {
            size_t p = 0;
            while (wlen - p >= (size_t)width) {
                if (line_len > 0) { buf[o++] = '\n'; line_len = 0; }
                memcpy(buf + o, text.data + wstart + p, (size_t)width); o += (size_t)width; p += (size_t)width;
                buf[o++] = '\n';
            }
            if (wlen - p > 0) {
                if (line_len > 0) { buf[o++] = '\n'; line_len = 0; }
                memcpy(buf + o, text.data + wstart + p, wlen - p); o += wlen - p;
                line_len = wlen - p;
            }
            continue;
        }
        size_t need = line_len > 0 ? 1 + wlen : wlen;
        if (line_len > 0 && line_len + need > (size_t)width) {
            buf[o++] = '\n'; line_len = 0; need = wlen;
        }
        if (line_len > 0) buf[o++] = ' ';
        memcpy(buf + o, text.data + wstart, wlen); o += wlen;
        line_len += need;
    }
    buf[o] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* SEARCH(haystack, needle): booleano (1 trovato / 0 no), case-insensitive,
 * come C# StringHelpers.Search (Contains OrdinalIgnoreCase). */
gmb_int_t gmb_str_search(gmb_string_t haystack, gmb_string_t needle) {
    if (!haystack.data || !needle.data) return 0;
    size_t hl = haystack.length, nl = needle.length;
    if (nl == 0) return 1;
    if (nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; i++) {
        int ok = 1;
        for (size_t j = 0; j < nl; j++) {
            unsigned char h = (unsigned char)haystack.data[i + j];
            unsigned char n = (unsigned char)needle.data[j];
            if (h >= 'A' && h <= 'Z') h = (unsigned char)(h + 32);
            if (n >= 'A' && n <= 'Z') n = (unsigned char)(n + 32);
            if (h != n) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

/* HEX(n): esadecimale maiuscolo. OCT(n): ottale. BIN(n): binario. */
gmb_string_t gmb_str_hex(gmb_int_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llX", (unsigned long long)n);
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_str_oct(gmb_int_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llo", (unsigned long long)n);
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_str_bin(gmb_int_t n) {
    unsigned long long v = (unsigned long long)n;
    if (v == 0) return gmb_string_alloc("0");
    char buf[80];
    char* p = buf + sizeof(buf) - 1;
    *p = 0;
    while (v > 0) { *--p = (char)('0' + (v & 1)); v >>= 1; }
    return gmb_string_alloc(p);
}

/* SPLIT(s, sep): lista di stringhe. Ritorna array malloc'd di gmb_val_t. */
gmb_val_t* gmb_str_split_list(const char* s, const char* sep, int* count_out) {
    *count_out = 0;
    if (!s) return NULL;
    if (!sep || !*sep) {
        /* senza separatore: caratteri individuali */
        int n = (int)strlen(s);
        if (n == 0) return NULL;
        gmb_val_t* out = (gmb_val_t*)malloc(sizeof(gmb_val_t) * n);
        if (!out) return NULL;
        for (int i = 0; i < n; i++) {
            char tmp[2] = { s[i], 0 };
            out[i] = gmb_make_val_string(tmp);
        }
        *count_out = n;
        return out;
    }
    size_t sl = strlen(sep);
    int cap = 8, n = 0;
    gmb_val_t* out = (gmb_val_t*)malloc(sizeof(gmb_val_t) * cap);
    if (!out) return NULL;
    const char* p = s;
    for (;;) {
        const char* next = strstr(p, sep);
        size_t len = next ? (size_t)(next - p) : strlen(p);
        char* buf = (char*)malloc(len + 1);
        if (!buf) break;
        memcpy(buf, p, len); buf[len] = 0;
        if (n >= cap) {
            cap *= 2;
            gmb_val_t* nb = (gmb_val_t*)realloc(out, sizeof(gmb_val_t) * cap);
            if (!nb) { free(buf); break; }
            out = nb;
        }
        out[n++] = gmb_make_val_string(buf);
        free(buf);
        if (!next) break;
        p = next + sl;
    }
    *count_out = n;
    return out;
}

/* Split in array di gmb_string_t (per `DIM x$() = regex.Split(...)`): come
   gmb_str_split_list ma restituisce gmb_string_t* (stringhe native). */
gmb_string_t* gmb_str_split_strings(const char* s, const char* sep, int* count_out) {
    gmb_val_t* vals = gmb_str_split_list(s, sep, count_out);
    if (!vals || *count_out <= 0) return NULL;
    int n = *count_out;
    gmb_string_t* out = (gmb_string_t*)malloc(sizeof(gmb_string_t) * n);
    if (!out) return NULL;
    for (int i = 0; i < n; i++) out[i] = gmb_val_to_string(vals[i]);
    free(vals);
    return out;
}

/* ARRAYS.ATOKENS(s, delims): tokenizza su QUALSIASI carattere di delims e scarta i token
 * vuoti (parity IL/PCode/VM: .NET Split(char[], RemoveEmptyEntries) / strtok). */
gmb_string_t* gmb_str_tokens_strings(const char* s, const char* delims, int* count_out) {
    *count_out = 0;
    if (!s) return NULL;
    const char* dl = (delims && *delims) ? delims : " ";
    size_t sl = strlen(s);
    size_t cap = 8, n = 0;
    gmb_string_t* out = (gmb_string_t*)malloc(cap * sizeof(gmb_string_t));
    if (!out) return NULL;
    size_t i = 0;
    while (i < sl) {
        while (i < sl && strchr(dl, s[i])) i++;
        size_t start = i;
        while (i < sl && !strchr(dl, s[i])) i++;
        if (i > start) {
            if (n == cap) { cap *= 2; gmb_string_t* no = (gmb_string_t*)realloc(out, cap * sizeof(gmb_string_t)); if (!no) { free(out); return NULL; } out = no; }
            size_t tlen = i - start;
            char* tmp = (char*)malloc(tlen + 1);
            if (!tmp) { free(out); return NULL; }
            memcpy(tmp, s + start, tlen); tmp[tlen] = 0;
            out[n++] = gmb_string_alloc(tmp);
            free(tmp);
        }
    }
    *count_out = (int)n;
    return out;
}

/* JOIN(list di stringhe, sep): concatena con separatore. items è array gmb_val_t. */
gmb_string_t gmb_str_join_list(gmb_val_t* items, int count, const char* sep) {
    if (!items || count <= 0) return gmb_string_alloc("");
    size_t total = 0;
    for (int i = 0; i < count; i++) {
        if (items[i].kind == GMB_TYPE_STRING && items[i].v_string.data)
            total += items[i].v_string.length;
        else if (items[i].kind == GMB_TYPE_INT) total += 24;
        else if (items[i].kind == GMB_TYPE_DOUBLE) total += 32;
        else total += 8;
    }
    size_t sep_len = sep ? strlen(sep) : 0;
    total += sep_len * (size_t)(count > 0 ? count - 1 : 0) + 1;
    char* buf = (char*)malloc(total);
    if (!buf) return gmb_string_alloc("");
    size_t o = 0;
    for (int i = 0; i < count; i++) {
        if (i > 0 && sep_len) { memcpy(buf + o, sep, sep_len); o += sep_len; }
        if (items[i].kind == GMB_TYPE_STRING && items[i].v_string.data) {
            memcpy(buf + o, items[i].v_string.data, items[i].v_string.length);
            o += items[i].v_string.length;
        } else if (items[i].kind == GMB_TYPE_INT) {
            int n = snprintf(buf + o, total - o, "%lld", (long long)items[i].v_int);
            if (n > 0) o += (size_t)n;
        } else if (items[i].kind == GMB_TYPE_DOUBLE) {
            int n = snprintf(buf + o, total - o, "%g", items[i].v_double);
            if (n > 0) o += (size_t)n;
        }
    }
    buf[o] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* PASCAL(s): ToTitleCase senza spazi. */
gmb_string_t gmb_str_pascal(gmb_string_t s) {
    gmb_string_t t = gmb_str_title(s);
    if (!t.data) return t;
    char* buf = (char*)malloc(t.length + 1);
    if (!buf) { free(t.data); return gmb_string_alloc(""); }
    size_t o = 0;
    for (size_t i = 0; i < t.length; i++) {
        if (t.data[i] != ' ') buf[o++] = t.data[i];
    }
    buf[o] = 0;
    free(t.data);
    return gmb_string_alloc(buf);
}

/* CAMEL(s): come pascal ma prima lettera minuscola. */
gmb_string_t gmb_str_camel(gmb_string_t s) {
    gmb_string_t p = gmb_str_pascal(s);
    if (!p.data || p.length == 0) { if (p.data) free(p.data); return gmb_string_alloc(""); }
    char* buf = (char*)malloc(p.length + 1);
    if (!buf) { free(p.data); return gmb_string_alloc(""); }
    strcpy(buf, p.data);
    buf[0] = (char)tolower((unsigned char)buf[0]);
    free(p.data);
    return gmb_string_alloc(buf);
}

/* SNAKE(s)/KEBAB(s): minuscolo, non-alnum → sep, trim sep. */
static gmb_string_t gmb_str_sepcase(gmb_string_t s, char sep) {
    /* Parita' StringHelpers.ToSeparated (snake/kebab): per ogni char i>0, se e' MAIUSCOLO
     * e il precedente non e' gia' il separatore -> sep + minuscola; altrimenti minuscola.
     * Gli spazi/separatori esistenti NON vengono convertiti (restano). */
    if (!s.data) return gmb_string_alloc("");
    char* buf = (char*)malloc(s.length * 2 + 2);
    if (!buf) return gmb_string_alloc("");
    size_t o = 0;
    for (size_t i = 0; i < s.length; i++) {
        unsigned char c = (unsigned char)s.data[i];
        if (i > 0 && c >= 'A' && c <= 'Z' && (unsigned char)s.data[i - 1] != (unsigned char)sep) {
            buf[o++] = sep;
            buf[o++] = (char)tolower(c);
        } else {
            buf[o++] = (char)tolower(c);
        }
    }
    buf[o] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}
gmb_string_t gmb_str_snake(gmb_string_t s) { return gmb_str_sepcase(s, '_'); }
gmb_string_t gmb_str_kebab(gmb_string_t s) { return gmb_str_sepcase(s, '-'); }

/* SENTENCE(s): prima lettera maiuscola, resto minuscolo. */
gmb_string_t gmb_str_sentence(gmb_string_t s) {
    if (!s.data || s.length == 0) return gmb_string_alloc("");
    char* buf = (char*)malloc(s.length + 1);
    if (!buf) return gmb_string_alloc("");
    buf[0] = (char)toupper((unsigned char)s.data[0]);
    for (size_t i = 1; i < s.length; i++)
        buf[i] = (char)tolower((unsigned char)s.data[i]);
    buf[s.length] = 0;
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* FOR EACH su stringa: lista dei singoli caratteri (gmb_val_t stringhe). */
gmb_list_t gmb_str_to_chars(gmb_string_t s) {
    gmb_list_t out = gmb_list_new();
    for (size_t i = 0; i < s.length; i++) {
        char c[2] = { s.data[i], '\0' };
        gmb_list_add(&out, gmb_make_val_string(c));
    }
    return out;
}

/* REVERSE(s): inverte l'ordine dei caratteri di una stringa. */
gmb_string_t gmb_str_reverse(gmb_string_t s) {
    gmb_string_t r = gmb_string_alloc("");
    r.length = s.length;
    r.data = (char*)realloc(r.data, s.length + 1);
    if (r.data) {
        for (size_t i = 0; i < s.length; i++) r.data[i] = s.data[s.length - 1 - i];
        r.data[s.length] = '\0';
    }
    return r;
}

/* PADL/PADR/PADC: wrapper che ritorna gmb_string_t; delega alle versioni buffer
 * di gmb_tui.c (gmb_str_padl/padr/padc). char = primo char del pad (spazio se vuoto). */
static char gmb_str_pad_first(gmb_string_t pad) {
    return (pad.data && pad.length > 0) ? pad.data[0] : ' ';
}
gmb_string_t gmb_str_padl_str(gmb_string_t s, gmb_int_t length, gmb_string_t pad) {
    int cap = (int)s.length + (length > 0 ? (int)length : 0) + 4;
    char* buf = (char*)malloc((size_t)cap);
    if (!buf) return gmb_string_alloc("");
    gmb_str_padl(s.data ? s.data : "", (int)length, gmb_str_pad_first(pad), buf, cap);
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}
gmb_string_t gmb_str_padr_str(gmb_string_t s, gmb_int_t length, gmb_string_t pad) {
    int cap = (int)s.length + (length > 0 ? (int)length : 0) + 4;
    char* buf = (char*)malloc((size_t)cap);
    if (!buf) return gmb_string_alloc("");
    gmb_str_padr(s.data ? s.data : "", (int)length, gmb_str_pad_first(pad), buf, cap);
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}
gmb_string_t gmb_str_padc_str(gmb_string_t s, gmb_int_t length, gmb_string_t pad) {
    int cap = (int)s.length + (length > 0 ? (int)length : 0) + 4;
    char* buf = (char*)malloc((size_t)cap);
    if (!buf) return gmb_string_alloc("");
    gmb_str_padc(s.data ? s.data : "", (int)length, gmb_str_pad_first(pad), buf, cap);
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* STRMD5 / STRSHA256: hash di una stringa come hex minuscolo (gmb_md5_hex/gmb_sha256_hex). */
gmb_string_t gmb_strmd5(gmb_string_t s) {
    char* h = gmb_md5_hex((const uint8_t*)(s.data ? s.data : ""), s.length);
    gmb_string_t r = gmb_string_alloc(h ? h : "");
    if (h) free(h);
    return r;
}
gmb_string_t gmb_strsha256(gmb_string_t s) {
    char* h = gmb_sha256_hex((const uint8_t*)(s.data ? s.data : ""), s.length);
    gmb_string_t r = gmb_string_alloc(h ? h : "");
    if (h) free(h);
    return r;
}
