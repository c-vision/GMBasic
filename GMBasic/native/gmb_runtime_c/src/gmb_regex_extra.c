/* gmb_regex_extra.c — parità CommonLib/Text/RegexWrapper.cs:103 via PCRE2 (libreria C esterna)
 * Aggiunge IsMatch/Match/Matches/Replace/Split/Group con sintassi .NET-ish (inline (?i) ecc).
 * Isolato: NON tocca gmb_tui_datatypes.c:1230 (gmb_regexes_matches POSIX, resta invariato).
 * Link: find_library(pcre2-8) in CMakeLists.
 */
#include "gmb_regex_extra.h"
#include "gmb_runtime.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pcre2.h>

static pcre2_code* g_re_compile(const char* pattern, pcre2_match_data** md_out)
{
    int errcode;
    PCRE2_SIZE erroffset;
    pcre2_code* re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0,
                                   &errcode, &erroffset, NULL);
    if (!re) return NULL;
    *md_out = pcre2_match_data_create_from_pattern(re, NULL);
    if (!*md_out) { pcre2_code_free(re); return NULL; }
    return re;
}

int gmb_regex_is_match(const char* pattern, const char* text)
{
    if (!pattern || !text) return 0;
    pcre2_match_data* md = NULL;
    pcre2_code* re = g_re_compile(pattern, &md);
    if (!re) return 0;
    int rc = pcre2_match(re, (PCRE2_SPTR)text, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return rc >= 0;
}

int gmb_regex_match(const char* pattern, const char* text, char* out_value, int out_cap,
                    int* index_out, int* length_out)
{
    if (!pattern || !text) return 0;
    pcre2_match_data* md = NULL;
    pcre2_code* re = g_re_compile(pattern, &md);
    if (!re) return 0;
    int rc = pcre2_match(re, (PCRE2_SPTR)text, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    if (rc < 0) { pcre2_match_data_free(md); pcre2_code_free(re); return 0; }
    PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
    if (index_out) *index_out = (int)ov[0];
    if (length_out) *length_out = (int)(ov[1] - ov[0]);
    if (out_value && out_cap > 0) {
        int len = (int)(ov[1] - ov[0]);
        if (len > out_cap - 1) len = out_cap - 1;
        memcpy(out_value, text + ov[0], (size_t)len);
        out_value[len] = '\0';
    }
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return 1;
}

int gmb_regex_matches_values(const char* pattern, const char* text, char out[][256], int cap)
{
    if (!pattern || !text || !out || cap <= 0) return 0;
    pcre2_match_data* md = NULL;
    pcre2_code* re = g_re_compile(pattern, &md);
    if (!re) return 0;
    int count = 0;
    PCRE2_SIZE start = 0;
    PCRE2_SIZE slen = strlen(text);
    while (count < cap) {
        int rc = pcre2_match(re, (PCRE2_SPTR)text, slen, start, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        if (ov[1] > slen) break;
        int len = (int)(ov[1] - ov[0]);
        if (len > 255) len = 255;
        memcpy(out[count], text + ov[0], (size_t)len);
        out[count][len] = '\0';
        count++;
        if (ov[1] == ov[0]) { /* match vuoto: .NET avanza di un char */
            if (ov[1] >= slen) break;
            start = ov[1] + 1;
        } else {
            start = ov[1];
        }
    }
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return count;
}

int gmb_regex_group(const char* pattern, const char* text, int group_index, char* out, int out_cap)
{
    if (!pattern || !text || !out || out_cap <= 0 || group_index < 0) return 0;
    pcre2_match_data* md = NULL;
    pcre2_code* re = g_re_compile(pattern, &md);
    if (!re) return 0;
    PCRE2_SIZE slen = strlen(text);
    int rc = pcre2_match(re, (PCRE2_SPTR)text, slen, 0, 0, md, NULL);
    if (rc < 0) { pcre2_match_data_free(md); pcre2_code_free(re); return 0; }
    PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
    int ok = 0;
    /* pcre2_get_ovector_count = numero di gruppi+1 */
    PCRE2_SIZE ncap = pcre2_get_ovector_count(md);
    /* F2 multimodale (2026-09-11): off-by-one — ovector_count = n.coppie (gruppo 0
     * incluso): il gruppo g è valido se g < ncap. Prima (g+1) < ncap scartava il gruppo
     * 0 sempre e l'ultimo gruppo di ogni pattern (es. unico gruppo di cattura). */
    if ((PCRE2_SIZE)group_index < ncap && ov[group_index * 2] != PCRE2_UNSET) {
        int len = (int)(ov[group_index * 2 + 1] - ov[group_index * 2]);
        if (len > out_cap - 1) len = out_cap - 1;
        memcpy(out, text + ov[group_index * 2], (size_t)len);
        out[len] = '\0';
        ok = 1;
    } else {
        out[0] = '\0';
        ok = 0; /* .NET: stringa vuota se non presente */
        if (rc > 0) out[0] = '\0';
    }
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return ok;
}

/* Espansione stile .NET: $& intero match, $1..$9 gruppi, $$ letterale $ */
static void g_re_append_expansion(char* out, int* o, int cap,
                                  const char* repl, PCRE2_SIZE* ov, int ncap,
                                  const char* text, PCRE2_SIZE match_so, PCRE2_SIZE match_eo)
{
    for (const char* p = repl; *p && *o < cap - 1; p++) {
        if (*p == '$' && p[1]) {
            if (p[1] == '$') { out[(*o)++] = '$'; p++; continue; }
            if (p[1] == '&') { for (PCRE2_SIZE k = match_so; k < match_eo && *o < cap - 1; k++) out[(*o)++] = text[k]; p++; continue; }
            if (p[1] >= '0' && p[1] <= '9') {
                int g = p[1] - '0';
                if (g < ncap && ov[g * 2] != PCRE2_UNSET) {
                    for (PCRE2_SIZE k = ov[g * 2]; k < ov[g * 2 + 1] && *o < cap - 1; k++) out[(*o)++] = text[k];
                }
                p++;
                continue;
            }
        }
        out[(*o)++] = *p;
    }
}

int gmb_regex_replace(const char* pattern, const char* text, const char* replacement, char* out, int out_cap)
{
    if (!pattern || !text || !replacement || !out || out_cap <= 0) return 0;
    pcre2_match_data* md = NULL;
    pcre2_code* re = g_re_compile(pattern, &md);
    if (!re) return 0;
    PCRE2_SIZE slen = strlen(text);
    int o = 0;
    PCRE2_SIZE start = 0;
    while (start < slen || o == 0) {
        int rc = pcre2_match(re, (PCRE2_SPTR)text, slen, start, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        int ncap = (int)pcre2_get_ovector_count(md);
        if (ov[1] > slen) break;
        /* copia testo prima del match */
        for (PCRE2_SIZE k = start; k < ov[0] && o < out_cap - 1; k++) out[o++] = text[k];
        g_re_append_expansion(out, &o, out_cap, replacement, ov, ncap, text, ov[0], ov[1]);
        if (ov[1] == ov[0]) {
            /* match vuoto: .NET sostituisce e avanza di un char */
            if (ov[1] >= slen) { start = ov[1] + 1; if (start > slen) start = slen; break; }
            start = ov[1] + 1;
        } else {
            start = ov[1];
        }
    }
    /* copia resto */
    for (PCRE2_SIZE k = start; k < slen && o < out_cap - 1; k++) out[o++] = text[k];
    out[o] = '\0';
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return o;
}

int gmb_regex_split(const char* pattern, const char* text, char out[][256], int cap)
{
    if (!pattern || !text || !out || cap <= 0) return 0;
    pcre2_match_data* md = NULL;
    pcre2_code* re = g_re_compile(pattern, &md);
    if (!re) return 0;
    PCRE2_SIZE slen = strlen(text);
    int count = 0;
    PCRE2_SIZE start = 0;
    PCRE2_SIZE cur = 0;
    while (count < cap) {
        int rc = pcre2_match(re, (PCRE2_SPTR)text, slen, start, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        if (ov[1] > slen) break;
        /* pezzo tra cur e ov[0] */
        int len = (int)(ov[0] - cur);
        if (len > 255) len = 255;
        memcpy(out[count], text + cur, (size_t)len);
        out[count][len] = '\0';
        count++;
        if (count >= cap) break;
        /* .NET Split: se c'è un gruppo catturante include il suo valore */
        int ncap = (int)pcre2_get_ovector_count(md);
        if (ncap > 1 && ov[2] != PCRE2_UNSET) {
            int glen = (int)(ov[3] - ov[2]);
            if (glen > 255) glen = 255;
            memcpy(out[count], text + ov[2], (size_t)glen);
            out[count][glen] = '\0';
            count++;
            if (count >= cap) break;
        }
        if (ov[1] == ov[0]) {
            if (ov[1] >= slen) { cur = ov[1]; start = slen + 1; }
            else { cur = ov[1]; start = ov[1] + 1; }
        } else {
            cur = ov[1];
            start = ov[1];
        }
    }
    /* ultimo pezzo */
    if (count < cap) {
        int len = (int)(slen - cur);
        if (len > 255) len = 255;
        memcpy(out[count], text + cur, (size_t)len);
        out[count][len] = '\0';
        count++;
    }
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return count;
}

/* ══════════════════════════════════════════════════════════════════════════════════════════
 * API PER-OGGETTO con PATTERN COMPILATO RIUSABILE
 * ──────────────────────────────────────────────────────────────────────────────────────────
 * Il pattern viene compilato UNA volta alla creazione (o al Pattern=) e riusato per N
 * match/IsMatch/Replace/Split: nessuna ricompilazione per chiamata. L'handle è opaco
 * (gmb_regex_t*) e vive nel valore REGEX della variabile BASIC (lifetime del caso d'uso).
 * ══════════════════════════════════════════════════════════════════════════════════════════ */

struct gmb_regex {
    char*       pattern;   /* copia del pattern sorgente (getter r.Pattern) */
    pcre2_code* code;      /* pattern compilato (NULL se non valido) */
};
static pcre2_code* g_re_compile_code(const char* pattern)
{
    int errcode;
    PCRE2_SIZE erroffset;
    return pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0,
                         &errcode, &erroffset, NULL);
}

gmb_regex_t* gmb_regex_new(const char* pattern)
{
    gmb_regex_t* re = (gmb_regex_t*)calloc(1, sizeof(gmb_regex_t));
    if (!re) return NULL;
    re->pattern = pattern ? strdup(pattern) : strdup("");
    re->code = re->pattern ? g_re_compile_code(re->pattern) : NULL;
    return re;
}

void gmb_regex_free(gmb_regex_t* re)
{
    if (!re) return;
    if (re->code) pcre2_code_free(re->code);
    free(re->pattern);
    free(re);
}

/* Imposta/ricompila il pattern (r.Pattern = "..."). Handle stabile in-place. */
int gmb_regex_set_pattern(gmb_regex_t* re, const char* pattern)
{
    if (!re) return 0;
    if (re->code) { pcre2_code_free(re->code); re->code = NULL; }
    free(re->pattern);
    re->pattern = pattern ? strdup(pattern) : strdup("");
    if (re->pattern) re->code = g_re_compile_code(re->pattern);
    return re->code != NULL;
}

const char* gmb_regex_pattern(const gmb_regex_t* re)
{
    return (re && re->pattern) ? re->pattern : "";
}

int gmb_regex_is_match_obj(const gmb_regex_t* re, const char* text)
{
    if (!re || !re->code || !text) return 0;
    pcre2_match_data* md = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!md) return 0;
    int rc = pcre2_match(re->code, (PCRE2_SPTR)text, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    pcre2_match_data_free(md);
    return rc >= 0;
}

/* Primo match: Value/Index/Length (success 0/1). out_value può essere NULL. */
int gmb_regex_match_obj(const gmb_regex_t* re, const char* text, char* out_value, int out_cap,
                        int* index_out, int* length_out)
{
    if (!re || !re->code || !text) return 0;
    pcre2_match_data* md = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!md) return 0;
    int rc = pcre2_match(re->code, (PCRE2_SPTR)text, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    if (rc < 0) { pcre2_match_data_free(md); return 0; }
    PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
    if (index_out) *index_out = (int)ov[0];
    if (length_out) *length_out = (int)(ov[1] - ov[0]);
    if (out_value && out_cap > 0) {
        int len = (int)(ov[1] - ov[0]);
        if (len > out_cap - 1) len = out_cap - 1;
        memcpy(out_value, text + ov[0], (size_t)len);
        out_value[len] = '\0';
    }
    pcre2_match_data_free(md);
    return 1;
}

/* Tutti i match in UNA passata: valori + (index,length) per ogni occorrenza.
 * out_* hanno cap slot; ritorna il numero di match (<= cap). */
int gmb_regex_matches_full(const gmb_regex_t* re, const char* text,
                           char out[][256], int* index_out, int* length_out, int cap)
{
    if (!re || !re->code || !text || cap <= 0) return 0;
    pcre2_match_data* md = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!md) return 0;
    PCRE2_SIZE slen = strlen(text);
    PCRE2_SIZE start = 0;
    int count = 0;
    while (count < cap) {
        int rc = pcre2_match(re->code, (PCRE2_SPTR)text, slen, start, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        if (ov[1] > slen) break;
        int len = (int)(ov[1] - ov[0]);
        if (len > 255) len = 255;
        if (out) { memcpy(out[count], text + ov[0], (size_t)len); out[count][len] = '\0'; }
        if (index_out) index_out[count] = (int)ov[0];
        if (length_out) length_out[count] = (int)(ov[1] - ov[0]);
        count++;
        if (ov[1] == ov[0]) { if (ov[1] >= slen) break; start = ov[1] + 1; }
        else start = ov[1];
    }
    pcre2_match_data_free(md);
    return count;
}

int gmb_regex_group_obj(const gmb_regex_t* re, const char* text, int group_index, char* out, int out_cap)
{
    if (!re || !re->code || !text || !out || out_cap <= 0 || group_index < 0) return 0;
    pcre2_match_data* md = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!md) return 0;
    int rc = pcre2_match(re->code, (PCRE2_SPTR)text, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    if (rc < 0) { pcre2_match_data_free(md); out[0] = '\0'; return 0; }
    PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
    int ok = 0;
    PCRE2_SIZE ncap = pcre2_get_ovector_count(md);
    if ((PCRE2_SIZE)group_index < ncap && ov[group_index * 2] != PCRE2_UNSET) {
        int len = (int)(ov[group_index * 2 + 1] - ov[group_index * 2]);
        if (len > out_cap - 1) len = out_cap - 1;
        memcpy(out, text + ov[group_index * 2], (size_t)len);
        out[len] = '\0';
        ok = 1;
    } else out[0] = '\0';
    pcre2_match_data_free(md);
    return ok;
}

int gmb_regex_replace_obj(const gmb_regex_t* re, const char* text, const char* replacement,
                          char* out, int out_cap)
{
    if (!re || !re->code || !text || !replacement || !out || out_cap <= 0) return 0;
    pcre2_match_data* md = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!md) return 0;
    PCRE2_SIZE slen = strlen(text);
    int o = 0;
    PCRE2_SIZE start = 0;
    while (start < slen || o == 0) {
        int rc = pcre2_match(re->code, (PCRE2_SPTR)text, slen, start, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        int ncap = (int)pcre2_get_ovector_count(md);
        if (ov[1] > slen) break;
        for (PCRE2_SIZE k = start; k < ov[0] && o < out_cap - 1; k++) out[o++] = text[k];
        g_re_append_expansion(out, &o, out_cap, replacement, ov, ncap, text, ov[0], ov[1]);
        if (ov[1] == ov[0]) {
            if (ov[1] >= slen) { start = ov[1] + 1; if (start > slen) start = slen; break; }
            start = ov[1] + 1;
        } else start = ov[1];
    }
    for (PCRE2_SIZE k = start; k < slen && o < out_cap - 1; k++) out[o++] = text[k];
    out[o] = '\0';
    pcre2_match_data_free(md);
    return o;
}

int gmb_regex_split_obj(const gmb_regex_t* re, const char* text, char out[][256], int cap)
{
    if (!re || !re->code || !text || !out || cap <= 0) return 0;
    pcre2_match_data* md = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!md) return 0;
    PCRE2_SIZE slen = strlen(text);
    int count = 0;
    PCRE2_SIZE start = 0;
    PCRE2_SIZE cur = 0;
    while (count < cap) {
        int rc = pcre2_match(re->code, (PCRE2_SPTR)text, slen, start, 0, md, NULL);
        if (rc < 0) break;
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        if (ov[1] > slen) break;
        int len = (int)(ov[0] - cur);
        if (len > 255) len = 255;
        memcpy(out[count], text + cur, (size_t)len);
        out[count][len] = '\0';
        count++;
        if (count >= cap) break;
        int ncap = (int)pcre2_get_ovector_count(md);
        if (ncap > 1 && ov[2] != PCRE2_UNSET) {
            int glen = (int)(ov[3] - ov[2]);
            if (glen > 255) glen = 255;
            memcpy(out[count], text + ov[2], (size_t)glen);
            out[count][glen] = '\0';
            count++;
            if (count >= cap) break;
        }
        if (ov[1] == ov[0]) {
            if (ov[1] >= slen) { cur = ov[1]; start = slen + 1; }
            else { cur = ov[1]; start = ov[1] + 1; }
        } else { cur = ov[1]; start = ov[1]; }
    }
    if (count < cap) {
        int len = (int)(slen - cur);
        if (len > 255) len = 255;
        memcpy(out[count], text + cur, (size_t)len);
        out[count][len] = '\0';
        count++;
    }
    pcre2_match_data_free(md);
    return count;
}

/* ══════════════════════════════════════════════════════════════════════════════════════════
 * VAL-HELPER per gli emitter: costruiscono/leggono i VALORI BASIC (dict/list) dei metodi
 * REGEX, così l'emitter C emette una sola chiamata per metodo.
 * ══════════════════════════════════════════════════════════════════════════════════════════ */

/* Crea il valore REGEX (object opaco type-name "REGEXWRAPPER") con handle compilato. */
gmb_val_t gmb_regex_new_val(const char* pattern)
{
    return gmb_make_val_obj(gmb_obj_new_typed(gmb_regex_new(pattern), "REGEXWRAPPER"));
}

/* Estrae l'handle dal valore REGEX; NULL se non è un REGEXWRAPPER. */
gmb_regex_t* gmb_regex_from_val(gmb_val_t v)
{
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return NULL;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o->type_name || strcmp(o->type_name, "REGEXWRAPPER") != 0) return NULL;
    return (gmb_regex_t*)o->data;
}

/* r.Pattern = "...": imposta/ricompila il pattern nell'handle. */
int gmb_regex_set_pattern_val(gmb_val_t reg, const char* pattern)
{
    return gmb_regex_set_pattern(gmb_regex_from_val(reg), pattern);
}

/* r.Pattern (get): stringa del pattern (mai NULL → "" se non è un REGEX). */
gmb_val_t gmb_regex_pattern_val(gmb_val_t reg)
{
    return gmb_make_val_string_counted(gmb_regex_pattern(gmb_regex_from_val(reg)));
}

/* r.IsMatch(s) → BIT (0/-1 come i bool BASIC). */
gmb_val_t gmb_regex_ismatch_val(gmb_val_t reg, const char* text)
{
    return gmb_make_val_bool(gmb_regex_is_match_obj(gmb_regex_from_val(reg), text) ? -1 : 0);
}

/* r.Match(s) → dict {Value,Index,Length,Success} (MatchObject). */
gmb_val_t gmb_regex_match_val(gmb_val_t reg, const char* text)
{
    gmb_dict_t d = gmb_dict_new();
    char val[256]; int idx = 0, len = 0;
    int ok = gmb_regex_match_obj(gmb_regex_from_val(reg), text, val, (int)sizeof(val), &idx, &len);
    gmb_dict_set(&d, "Value", gmb_make_val_string_counted(ok ? val : ""));
    gmb_dict_set(&d, "Index", gmb_make_val_int(ok ? idx : 0));
    gmb_dict_set(&d, "Length", gmb_make_val_int(ok ? len : 0));
    gmb_dict_set(&d, "Success", gmb_make_val_bool(ok ? -1 : 0));
    return gmb_make_val_dict(d);
}

/* r.Matches(s) → lista di dict MatchObject. */
gmb_val_t gmb_regex_matches_val(gmb_val_t reg, const char* text)
{
    gmb_list_t l = gmb_list_new();
    static char vals[512][256];
    static int idxs[512], lens[512];
    int n = gmb_regex_matches_full(gmb_regex_from_val(reg), text, vals, idxs, lens, 512);
    for (int i = 0; i < n; i++) {
        gmb_dict_t d = gmb_dict_new();
        gmb_dict_set(&d, "Value", gmb_make_val_string_counted(vals[i]));
        gmb_dict_set(&d, "Index", gmb_make_val_int(idxs[i]));
        gmb_dict_set(&d, "Length", gmb_make_val_int(lens[i]));
        gmb_dict_set(&d, "Success", gmb_make_val_bool(-1));
        gmb_list_add(&l, gmb_make_val_dict(d));
    }
    return gmb_make_val_list(l);
}

/* r.Replace(s, repl) → stringa. */
gmb_val_t gmb_regex_replace_val(gmb_val_t reg, const char* text, const char* repl)
{
    static char buf[1 << 20];
    gmb_regex_replace_obj(gmb_regex_from_val(reg), text, repl, buf, (int)sizeof(buf));
    return gmb_make_val_string_counted(buf);
}

/* r.Split(s) → lista di stringhe. */
gmb_val_t gmb_regex_split_val(gmb_val_t reg, const char* text)
{
    int cap = 512;
    char (*parts)[256] = (char(*)[256])malloc((size_t)cap * 256);
    gmb_list_t l = gmb_list_new();
    if (!parts) return gmb_make_val_list(l);
    int n = gmb_regex_split_obj(gmb_regex_from_val(reg), text, parts, cap);
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_string_counted(parts[i]));
    free(parts);
    return gmb_make_val_list(l);
}

/* r.Split(s) → array nativo di stringhe (gmb_string_t*) + count.
 * Per `DIM x$() = r.Split(s)`: JOIN/iterazione su un vero array di stringhe. */
gmb_string_t* gmb_regex_split_strings(gmb_val_t reg, const char* text, gmb_int_t* count)
{
    int cap = 512;
    char (*parts)[256] = (char(*)[256])malloc((size_t)cap * 256);
    if (!parts) { if (count) *count = 0; return NULL; }
    int n = gmb_regex_split_obj(gmb_regex_from_val(reg), text, parts, cap);
    gmb_string_t* out = (gmb_string_t*)calloc((size_t)(n > 0 ? n : 1), sizeof(gmb_string_t));
    for (int i = 0; i < n && out; i++) out[i] = gmb_string_alloc(parts[i]);
    free(parts);
    if (count) *count = n;
    return out;
}
