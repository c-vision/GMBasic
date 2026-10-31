/* gmb_grep.c — motore grep condiviso (implementazione). Vedi gmb_grep.h.
 *
 * Design adattato (non copiato) da un file pattern searcher moderno: walk ricorsiva,
 * selezione file per glob/estensione, match regex o stringa fissa, line/column,
 * only-matching, count, files-with-matches, context, case-insensitive, whole-word.
 * Sorgente UNICA: lo usano VM/C, C99 e (via P/Invoke su lib condivisa) la REPL C#. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* getline/strtok_r/lstat/ssize_t con -std=c99 */
#endif
#include "gmb_grep.h"
#include "gmb_matcher.h"
#include "gmb_regex_extra.h"
#include "gmb_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>

/* ── opzioni di ricerca ─────────────────────────────────────────────────────────── */
typedef struct {
    const char* pattern;
    int fixed, ignore_case, word, invert, recursive, hidden;
    int line_number, column_number, only_matching, count_only, files_with_matches;
    int max_depth, max_count, ctx_before, ctx_after;
    const char* include;   /* glob separati da virgola, es. "*.c,*.h" */
    const char* exclude;
    const char* type;      /* estensioni separate da virgola, es. "c,cpp" */
} gg_opts;

/* ── lettura opzioni dal dict gmb_val_t ─────────────────────────────────────────── */
static const char* gg_str(gmb_val_t d, const char* k) {
    gmb_val_t v = gmb_dict_get(gmb_val_as_dict(d), k);
    return (v.kind == GMB_TYPE_STRING && v.v_string.data) ? v.v_string.data : NULL;
}
static int gg_bool(gmb_val_t d, const char* k) {
    gmb_val_t v = gmb_dict_get(gmb_val_as_dict(d), k);
    if (v.kind == GMB_TYPE_BOOL) return v.v_bool ? 1 : 0;
    if (v.kind == GMB_TYPE_DOUBLE) return v.v_double != 0.0;
    if (v.kind == GMB_TYPE_INT) return v.v_int != 0;
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return strcmp(v.v_string.data, "0") != 0;
    return 0;
}
static int gg_int(gmb_val_t d, const char* k, int def) {
    gmb_val_t v = gmb_dict_get(gmb_val_as_dict(d), k);
    if (v.kind == GMB_TYPE_INT) return (int)v.v_int;
    if (v.kind == GMB_TYPE_DOUBLE) return (int)v.v_double;
    return def;
}

/* ── glob semplice (*, ?) case-sensitive, con `/` significativo ─────────────────── */
static int gg_glob(const char* pat, const char* name) {
    const char* p = pat; const char* n = name;
    const char* star = NULL; const char* ss = NULL;
    while (*n) {
        if (*p == '*' || *p == '?') {
            if (*p == '?') { p++; n++; continue; }
            star = p++; ss = n; continue;
        }
        if (*p && tolower((unsigned char)*p) == tolower((unsigned char)*n)) { p++; n++; continue; }
        if (star) { p = star + 1; n = ++ss; continue; }
        return 0;
    }
    while (*p == '*') p++;
    return *p == '\0';
}
/* una lista CSV di glob ("*.c,*.h"): match se almeno uno combacia */
static int gg_glob_list(const char* csv, const char* name) {
    if (!csv || !*csv) return 0;
    char buf[1024]; size_t bl = strlen(csv);
    if (bl >= sizeof(buf)) bl = sizeof(buf) - 1;
    memcpy(buf, csv, bl); buf[bl] = 0;
    char* save = NULL;
    for (char* tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;
        if (gg_glob(tok, name)) return 1;
    }
    return 0;
}

/* ── selezione file: include/exclude/type ───────────────────────────────────────── */
static const char* gg_ext(const char* name) {
    const char* dot = strrchr(name, '.');
    return (dot && dot != name) ? dot + 1 : "";
}
static int gg_file_selected(const gg_opts* o, const char* name) {
    if (o->include && *o->include && !gg_glob_list(o->include, name)) return 0;
    if (o->exclude && *o->exclude && gg_glob_list(o->exclude, name)) return 0;
    if (o->type && *o->type) {
        const char* ext = gg_ext(name);
        int ok = 0;
        char buf[256]; size_t bl = strlen(o->type);
        if (bl >= sizeof(buf)) bl = sizeof(buf) - 1;
        memcpy(buf, o->type, bl); buf[bl] = 0;
        char* save = NULL;
        for (char* tok = strtok_r(buf, ",", &save); tok && !ok; tok = strtok_r(NULL, ",", &save)) {
            while (*tok == ' ') tok++;
            if (strcasecmp(tok, ext) == 0) ok = 1;
        }
        if (!ok) return 0;
    }
    return 1;
}

/* ── match di una riga: ritorna l'indice (0-based) del primo match o -1 ─────────── */
static int gg_line_find(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* line, int* out_len) {
    int idx = -1;
    if (o->fixed) {
        long mi = -1, ml = 0;
        if (gmb_matcher_find(mat, line, strlen(line), &mi, &ml) >= 0) { idx = (int)mi; if (out_len) *out_len = (int)ml; }
        else if (out_len) *out_len = 0;
    } else if (re) {
        int len = 0;
        int ok = gmb_regex_match_obj(re, line, NULL, 0, &idx, &len);
        if (!ok) { idx = -1; }
        if (out_len) *out_len = len;
    }
    if (idx >= 0 && o->word) {
        int bad = 0;
        int len = out_len ? *out_len : 0;
        if (idx > 0 && (isalnum((unsigned char)line[idx - 1]) || line[idx - 1] == '_')) bad = 1;
        int end = idx + len;
        if (!bad && line[end] && (isalnum((unsigned char)line[end]) || line[end] == '_')) bad = 1;
        if (bad) idx = -1;
    }
    if (o->invert) idx = (idx < 0) ? 0 : -1;   /* invert: match se NON matcha */
    return idx;
}

/* ── accumulo risultati in una lista gmb_val_t di dict ──────────────────────────── */
static void gg_emit_ctx(gmb_list_t* hits, const char* file, gmb_int_t line, gmb_int_t col, const char* text, gmb_int_t ctx) {
    gmb_dict_t d = gmb_dict_new();
    gmb_dict_set(&d, "file", gmb_make_val_string_counted(file ? file : ""));
    gmb_dict_set(&d, "line", gmb_make_val_int(line));
    gmb_dict_set(&d, "column", gmb_make_val_int(col));
    gmb_dict_set(&d, "text", gmb_make_val_string_counted(text ? text : ""));
    gmb_dict_set(&d, "ctx", gmb_make_val_int(ctx));   /* 0=match 1=context 2=separatore */
    gmb_list_add(hits, gmb_make_val_dict(d));
}
static void gg_emit(gmb_list_t* hits, const char* file, gmb_int_t line, gmb_int_t col, const char* text) {
    gg_emit_ctx(hits, file, line, col, text, 0);
}

/* legge tutto il file in un buffer malloc NUL-terminato (NULL su errore). */
static char* gg_read_all(const char* file, size_t* out_len) {
    FILE* f = fopen(file, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[rd] = 0;
    *out_len = rd;
    return b;
}

/* processa una riga (n = lunghezza senza newline); aggiorna *hits_in_file. Ritorna 1 se
 * la riga ha prodotto match. */
static int gg_scan_line(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat,
                        const char* file, gmb_int_t lineno, char* line, int n, gmb_list_t* hits,
                        int* hits_in_file) {
    int mlen = 0;
    int col = gg_line_find(o, re, mat, line, &mlen);
    if (col < 0) return 0;
    (*hits_in_file)++;                          /* conta ogni match (per max_count e count_only) */
    if (o->files_with_matches) return 1;
    if (o->count_only) return 1;
    if (o->only_matching) {
        int start = (o->invert) ? 0 : col;
        int len = (o->invert) ? n : mlen;
        char saved = line[start + len]; line[start + len] = 0;
        gg_emit(hits, file, lineno, col + 1, line + start);
        line[start + len] = saved;
    } else {
        gg_emit(hits, file, lineno, col + 1, line);
    }
    return 1;
}

/* scansiona un buffer di testo (NUL-terminato a len) riga per riga. */
static int gg_scan_buffer(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                          char* buf, size_t len, gmb_list_t* hits, int* file_matched) {
    int matched = 0, hits_in_file = 0;
    /* ── modalità CONTEXT (-A/-B/-C): due passate (righe + match + finestre) ── */
    if ((o->ctx_before > 0 || o->ctx_after > 0) && !o->count_only && !o->files_with_matches && !o->only_matching) {
        /* indicizza le righe (offset in buf, NUL-terminate a end) */
        size_t* starts = NULL; size_t* ends = NULL; int nlines = 0, cap = 0;
        for (size_t i = 0; i < len; ) {
            size_t j = i; while (j < len && buf[j] != '\n') j++;
            size_t end = j; while (end > i && buf[end - 1] == '\r') end--;
            if (nlines >= cap) { cap = cap ? cap * 2 : 128; starts = realloc(starts, (size_t)cap * sizeof(size_t)); ends = realloc(ends, (size_t)cap * sizeof(size_t)); }
            starts[nlines] = i; ends[nlines] = end; nlines++;
            i = (j < len) ? j + 1 : len;
        }
        int* ism = (int*)calloc((size_t)(nlines > 0 ? nlines : 1), sizeof(int));
        int* mcol = (int*)calloc((size_t)(nlines > 0 ? nlines : 1), sizeof(int));
        for (int k = 0; k < nlines; k++) {
            char saved = buf[ends[k]]; buf[ends[k]] = 0;
            int mlen = 0; int c = gg_line_find(o, re, mat, buf + starts[k], &mlen);
            buf[ends[k]] = saved;
            if (c >= 0) { ism[k] = 1; mcol[k] = c; }
        }
        long last = -2;
        for (int k = 0; k < nlines; k++) {
            if (!ism[k]) continue;
            int lo = k - o->ctx_before; if (lo < 0) lo = 0;
            int hi = k + o->ctx_after; if (hi > nlines - 1) hi = nlines - 1;
            if (o->max_count > 0 && hits_in_file >= o->max_count) break;
            if (last >= 0 && lo > last + 1) gg_emit_ctx(hits, file, 0, 0, "--", 2);
            for (int m = (lo > last + 1 ? lo : (int)(last + 1)); m <= hi; m++) {
                char saved = buf[ends[m]]; buf[ends[m]] = 0;
                if (ism[m]) gg_emit_ctx(hits, file, (gmb_int_t)(m + 1), mcol[m] + 1, buf + starts[m], 0);
                else gg_emit_ctx(hits, file, (gmb_int_t)(m + 1), 0, buf + starts[m], 1);
                buf[ends[m]] = saved;
            }
            last = hi; matched = 1; hits_in_file++;
        }
        free(starts); free(ends); free(ism); free(mcol);
        return matched;
    }
    gmb_int_t lineno = 0;
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && buf[j] != '\n') j++;
        size_t end = j;
        while (end > i && buf[end - 1] == '\r') end--;
        char saved = buf[end]; buf[end] = 0;
        lineno++;
        if (gg_scan_line(o, re, mat, file, lineno, buf + i, (int)(end - i), hits, &hits_in_file)) {
            matched = 1;
            if (o->files_with_matches) { buf[end] = saved; break; }
        }
        buf[end] = saved;
        if (o->max_count > 0 && hits_in_file >= o->max_count) break;
        i = (j < len) ? j + 1 : len;
    }
    if (o->files_with_matches && matched) { gg_emit(hits, file, 0, 0, ""); *file_matched = 1; }
    else if (o->count_only) {
        /* una entry con il conteggio nel campo "line" (text vuoto) */
        gg_emit(hits, file, hits_in_file, 0, "");
        matched = hits_in_file > 0;
    }
    return matched;
}

/* Algoritmo di compressione dedotto dall'estensione (NULL se il file non è compresso). */
static const char* gg_algo_of(const char* name) {
    const char* dot = strrchr(name, '.');
    if (!dot || !dot[1]) return NULL;
    dot++;
    if (!strcasecmp(dot, "gz") || !strcasecmp(dot, "gzip")) return "gzip";
    if (!strcasecmp(dot, "bz2") || !strcasecmp(dot, "bzip2")) return "bzip2";
    if (!strcasecmp(dot, "xz") || !strcasecmp(dot, "lzma")) return "xz";
    if (!strcasecmp(dot, "zst") || !strcasecmp(dot, "zstd")) return "zstd";
    if (!strcasecmp(dot, "lz4")) return "lz4";
    if (!strcasecmp(dot, "br") || !strcasecmp(dot, "brotli")) return "brotli";
    if (!strcasecmp(dot, "z")) return "lzw";   /* .Z compress/LZW (decoder in casa) */
    /* .tar.gz/.tgz/.tar.bz2/... : il tar interno è gestito dopo la decompressione. */
    return NULL;
}

/* sceglie l'algoritmo per un nome che può essere ".tar.gz"/".tgz" ecc. (prova le 2 estensioni). */
static const char* gg_algo_of_multi(const char* name) {
    const char* a = gg_algo_of(name);
    if (a) return a;
    if (!strcasecmp(name + (strlen(name) > 4 ? strlen(name) - 4 : 0), ".tgz")) return "gzip";
    const char* dot = strrchr(name, '.');
    if (dot && dot[1]) {
        /* taglia l'ultima estensione e riprova (es. "x.tar.gz" -> "x.tar") */
        char tmp[4096];
        size_t n = (size_t)(dot - name);
        if (n < sizeof(tmp)) { memcpy(tmp, name, n); tmp[n] = 0;
            const char* d2 = strrchr(tmp, '.');
            if (d2 && d2[1]) {
                if (!strcasecmp(d2 + 1, "tar")) {
                    const char* e = dot + 1;
                    if (!strcasecmp(e, "gz")) return "gzip";
                    if (!strcasecmp(e, "bz2")) return "bzip2";
                    if (!strcasecmp(e, "xz")) return "xz";
                    if (!strcasecmp(e, "zst")) return "zstd";
                    if (!strcasecmp(e, "lz4")) return "lz4";
                    if (!strcasecmp(e, "z")) return "lzw";
                }
            }
        }
    }
    return NULL;
}

/* ── archivio tar (header 512 byte, POSIX ustar) ──────────────────────────────── */
static long gg_tar_oct(const char* p, int n) {
    long v = 0; int i = 0;
    while (i < n && p[i] == ' ') i++;
    for (; i < n && p[i] >= '0' && p[i] <= '7'; i++) v = v * 8 + (p[i] - '0');
    return v;
}

/* ── Documenti office (docx/odt/xlsx/pptx/epub): il testo vive in XML dentro lo ZIP.
 * Estrazione IN-PROCESS (nessuna utility esterna): si tiene il testo FUORI dai tag,
 * `</w:p>`/`</text:p>`/`</p>` → newline, entità base decodificate. ── */
#define GMB_GREP_MAX_ARCHIVE_DEPTH 2

static int gg_is_doc_part(const char* name)
{
    const char* b = strrchr(name, '/'); b = b ? b + 1 : name;
    if (!strcmp(b, "document.xml")) return 1;      /* docx */
    if (!strcmp(b, "content.xml")) return 1;       /* odt */
    if (!strcmp(b, "sharedStrings.xml")) return 1; /* xlsx */
    if (!strcmp(b, "slide.xml")) return 1;         /* pptx (ppt/slides/slideN.xml) */
    { const char* d = strrchr(b, '.');             /* epub chapters */
      if (d && (!strcasecmp(d, ".xhtml") || !strcasecmp(d, ".html") || !strcasecmp(d, ".htm"))) return 1; }
    return 0;
}
/* Estrae il testo leggibile da un frammento XML (malloc; NUL-terminato). */
static char* gg_xml_text(const unsigned char* c, size_t n, size_t* outlen)
{
    char* out = (char*)malloc(n + 1);
    size_t o = 0, i = 0;
    if (!out) return NULL;
    while (i < n)
    {
        if (c[i] == '<')
        {
            size_t j = i + 1;
            while (j < n && c[j] != '>') j++;
            /* newline sui marker di paragrafo/riga */
            if (j < n)
            {
                size_t tl = j - i - 1;
                const char* t = (const char*)c + i + 1;
                if ((tl == 5 && !strncmp(t, "/w:p", 4)) || (tl >= 7 && !strncmp(t, "/text:p", 7))
                    || (tl == 3 && !strncmp(t, "/p", 2)) || !strncmp(t, "w:br", 4)
                    || !strncmp(t, "text:line-break", 15) || !strncmp(t, "w:tab", 5))
                    out[o++] = '\n';
            }
            i = (j < n) ? j + 1 : n;
            continue;
        }
        if (c[i] == '&')
        {
            /* entità base: &amp; &lt; &gt; &quot; &apos; &#NN; */
            if (i + 4 < n && !strncmp((const char*)c + i, "&amp;", 5)) { out[o++] = '&'; i += 5; continue; }
            if (i + 3 < n && !strncmp((const char*)c + i, "&lt;", 4)) { out[o++] = '<'; i += 4; continue; }
            if (i + 3 < n && !strncmp((const char*)c + i, "&gt;", 4)) { out[o++] = '>'; i += 4; continue; }
            if (i + 5 < n && !strncmp((const char*)c + i, "&quot;", 6)) { out[o++] = '"'; i += 6; continue; }
            if (i + 5 < n && !strncmp((const char*)c + i, "&apos;", 6)) { out[o++] = '\''; i += 6; continue; }
            if (i + 2 < n && c[i + 1] == '#')
            {
                size_t j = i + 2; long v = 0; int any = 0, hex = 0;
                if (j < n && (c[j] == 'x' || c[j] == 'X')) { hex = 1; j++; }
                for (; j < n && c[j] != ';'; j++)
                {
                    int d = hex ? ((c[j] >= '0' && c[j] <= '9') ? c[j] - '0' : (c[j] >= 'a' && c[j] <= 'f') ? c[j] - 'a' + 10 : (c[j] >= 'A' && c[j] <= 'F') ? c[j] - 'A' + 10 : -1)
                                : ((c[j] >= '0' && c[j] <= '9') ? c[j] - '0' : -1);
                    if (d < 0) break;
                    v = v * (hex ? 16 : 10) + d; any = 1;
                }
                if (any && j < n && c[j] == ';' && v > 0 && v < 256) { out[o++] = (char)v; i = j + 1; continue; }
            }
        }
        out[o++] = (char)c[i++];
    }
    out[o] = 0;
    if (outlen) *outlen = o;
    return out;
}

/* RTF (testo con control word): estrazione in-process. `\par`/`\line` → newline,
 * `\'hh` → byte, gruppi `{\*...}` (destinazioni ignorate) saltati, `\\ \{ \}` letterali. */
static char* gg_rtf_text(const unsigned char* c, size_t n, size_t* outlen)
{
    char* out = (char*)malloc(n + 1);
    size_t o = 0, i = 0;
    if (!out) return NULL;
    while (i < n)
    {
        if (c[i] == '{' && i + 1 < n && c[i + 1] == '\\' && i + 2 < n && c[i + 2] == '*')
        {
            int depth = 0;
            while (i < n) { if (c[i] == '{') depth++; else if (c[i] == '}' && --depth == 0) { i++; break; } i++; }
            continue;
        }
        if (c[i] == '\\')
        {
            i++;
            if (i >= n) break;
            if (c[i] == '\'' && i + 2 < n)
            {
                int h1 = c[i + 1], h2 = c[i + 2];
                int d1 = (h1 >= '0' && h1 <= '9') ? h1 - '0' : (h1 >= 'a' && h1 <= 'f') ? h1 - 'a' + 10 : (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
                int d2 = (h2 >= '0' && h2 <= '9') ? h2 - '0' : (h2 >= 'a' && h2 <= 'f') ? h2 - 'a' + 10 : (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
                if (d1 >= 0 && d2 >= 0) out[o++] = (char)(d1 * 16 + d2);
                i += 3;
                continue;
            }
            if (c[i] == '\\' || c[i] == '{' || c[i] == '}') { out[o++] = (char)c[i++]; continue; }
            { size_t ws = i; while (i < n && ((c[i] >= 'a' && c[i] <= 'z') || (c[i] >= 'A' && c[i] <= 'Z'))) i++;
              { size_t wl = i - ws;
                int is_par = (wl == 3 && !strncmp((const char*)c + ws, "par", 3)) || (wl == 4 && !strncmp((const char*)c + ws, "line", 4));
                int is_tab = (wl == 3 && !strncmp((const char*)c + ws, "tab", 3));
                if (i < n && c[i] == '-' ) { i++; while (i < n && c[i] >= '0' && c[i] <= '9') i++; }
                if (is_par) out[o++] = '\n'; else if (is_tab) out[o++] = '\t';
                if (i < n && c[i] == ' ') i++;   /* delimitatore */
              }
            }
            continue;
        }
        if (c[i] != '{' && c[i] != '}') out[o++] = (char)c[i];
        i++;
    }
    out[o] = 0;
    if (outlen) *outlen = o;
    return out;
}

/* Dispatch per MAGIC su un blob: ricorsione negli archivi annidati (max depth) e
 * estrazione testo per i documenti office. */
static int gg_scan_blob(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                        char* data, size_t len, gmb_list_t* hits, int* fm, int depth);
static int gg_tar_is(const char* b, size_t len) {
    return len >= 265 && memcmp(b + 257, "ustar", 5) == 0;
}
static int gg_scan_tar(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                       char* buf, size_t len, gmb_list_t* hits, int* fm, int depth) {
    int any = 0; size_t off = 0;
    while (off + 512 <= len) {
        char* h = buf + off;
        if (h[0] == 0) break;   /* blocco zero = fine archivio */
        char name[101]; memcpy(name, h, 100); name[100] = 0;
        long size = gg_tar_oct(h + 124, 12);
        char type = h[156];
        size_t data_off = off + 512;
        if ((type == '0' || type == 0 || type == 'x') && size > 0 && data_off + (size_t)size <= len) {
            char disp[4200];
            snprintf(disp, sizeof(disp), "%s:%s", file, name);
            any |= gg_scan_blob(o, re, mat, disp, buf + data_off, (size_t)size, hits, fm, depth);
        }
        off = data_off + (((size_t)size + 511) / 512) * 512;
    }
    return any;
}

/* ── archivio cpio (newc "070701"/"070702") ──────────────────────────────────── */
static unsigned long gg_cpio_hex(const char* p, int n) {
    unsigned long v = 0;
    for (int i = 0; i < n; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
    }
    return v;
}
static int gg_cpio_is(const char* b, size_t len) {
    return len >= 6 && (memcmp(b, "070701", 6) == 0 || memcmp(b, "070702", 6) == 0);
}
static int gg_scan_cpio(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                        char* buf, size_t len, gmb_list_t* hits, int* fm, int depth) {
    int any = 0; size_t off = 0;
    while (off + 110 <= len) {
        char* h = buf + off;
        if (memcmp(h, "07070", 5) != 0) break;
        unsigned long namesize = gg_cpio_hex(h + 94, 8);
        unsigned long filesize = gg_cpio_hex(h + 54, 8);
        size_t name_off = off + 110;
        if (namesize == 0 || name_off + namesize > len) break;
        if (filesize > 0 && name_off + namesize + filesize <= len) {
            char disp[4200];
            snprintf(disp, sizeof(disp), "%s:%s", file, buf + name_off);
            any |= gg_scan_blob(o, re, mat, disp, buf + name_off + namesize, (size_t)filesize, hits, fm, depth);
        }
        if (strncmp(buf + name_off, "TRAILER!!!", 10) == 0) break;
        size_t data_off = ((name_off + namesize + 3) / 4) * 4;
        off = ((data_off + filesize + 3) / 4) * 4;
    }
    return any;
}

/* ── PDF: estrazione testo via hook REGISTRATO dal consumer (che linka gmb_doc).
 * Nessuna dipendenza a link-time: senza registrazione la ricerca in PDF è saltata. ── */
typedef char* (*gg_pdf_extract_fn)(const char* path);
static gg_pdf_extract_fn g_pdf_extract = NULL;
void gmb_grep_set_pdf_extractor(gg_pdf_extract_fn fn) { g_pdf_extract = fn; }

static int gg_is_pdf(const char* name) {
    const char* dot = strrchr(name, '.');
    return dot && !strcasecmp(dot, ".pdf");
}
static int gg_scan_pdf(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                       gmb_list_t* hits, int* fm) {
    if (!g_pdf_extract) return 0;
    char* all = g_pdf_extract(file);
    if (!all) return 0;
    int r = gg_scan_buffer(o, re, mat, file, all, strlen(all), hits, fm);
    free(all);
    return r;
}

/* ── archivio zip (in-memory, STORE+DEFLATE) ──────────────────────────────────── */
static int gg_zip_is(const char* b, size_t len) {
    if (len < 4) return 0;
    if (b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4) return 1;   /* local file header */
    if (b[0] == 'P' && b[1] == 'K' && b[2] == 5 && b[3] == 6) return 1;   /* zip vuoto (EOCD) */
    return 0;
}
typedef struct { const gg_opts* o; gmb_regex_t* re; gmb_matcher_t* mat; const char* file; gmb_list_t* hits; int* fm; int any; int depth; } gg_zip_ctx;
static int gg_zip_cb(const char* name, const unsigned char* content, size_t clen, void* c) {
    gg_zip_ctx* z = (gg_zip_ctx*)c;
    char disp[4200];
    snprintf(disp, sizeof(disp), "%s:%s", z->file, name);
    if (gg_is_doc_part(name))
    {
        /* docx/odt/xlsx/pptx/epub: testo dagli XML (tag rimossi) */
        size_t tl = 0;
        char* txt = gg_xml_text(content, clen, &tl);
        if (txt) { z->any |= gg_scan_buffer(z->o, z->re, z->mat, disp, txt, tl, z->hits, z->fm); free(txt); }
        else z->any |= gg_scan_blob(z->o, z->re, z->mat, disp, (char*)content, clen, z->hits, z->fm, z->depth + 1);
        return 0;
    }
    z->any |= gg_scan_blob(z->o, z->re, z->mat, disp, (char*)content, clen, z->hits, z->fm, z->depth + 1);
    return 0;
}
static int gg_scan_zip(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                       char* buf, size_t len, gmb_list_t* hits, int* fm, int depth) {
    gg_zip_ctx z = { o, re, mat, file, hits, fm, 0, depth };
    gmb_zip_foreach_mem((const unsigned char*)buf, len, gg_zip_cb, &z);
    return z.any;
}

/* ── archivio 7z via libarchive (dlopen, best-effort): si attiva se libarchive è presente
 * (Resources/libs/<rid>/libarchive.* o di sistema). Niente env, niente dipendenza a build. ── */
typedef struct archive gg_ar_t;
typedef struct archive_entry gg_ar_entry_t;
typedef struct {
    gg_ar_t* (*read_new)(void);
    int (*support_filter_all)(gg_ar_t*);
    int (*support_format_7zip)(gg_ar_t*);
    int (*open_memory)(gg_ar_t*, const void*, size_t);
    int (*next_header)(gg_ar_t*, gg_ar_entry_t**);
    const char* (*entry_pathname)(gg_ar_entry_t*);
    long long (*entry_size)(gg_ar_entry_t*);
    long long (*read_data)(gg_ar_t*, void*, size_t);
    int (*read_free)(gg_ar_t*);
    void* dl; int tried;
} gg_ar_bind;
static gg_ar_bind g_ar;
static int gg_ar_load(void) {
    if (g_ar.tried) return g_ar.dl != NULL;
    g_ar.tried = 1;
    /* libarchive: prima dalla CACHE utente del RID (`~/.gmbasic/libs/<rid>/`), poi i nomi di
     * sistema. Le vecchie voci `Resources/libs/<rid>/…` puntavano a una cartella eliminata. */
    char cached[1200];
    {
        extern void gmb_paths_libs_cache_dir(char* out, size_t cap);
        extern const char* gmb_libs_rid(void);
        char cache[1024];
        gmb_paths_libs_cache_dir(cache, sizeof cache);
#if defined(_WIN32)
        const char* leaf = "archive.dll";
#elif defined(__APPLE__)
        const char* leaf = "libarchive.dylib";
#else
        const char* leaf = "libarchive.so";
#endif
        if (cache[0]) snprintf(cached, sizeof cached, "%s/%s/%s", cache, gmb_libs_rid(), leaf);
        else cached[0] = '\0';
    }
    const char* names[] = {
        cached[0] ? cached : "libarchive.dylib",
        "libarchive.dylib", "/usr/lib/libarchive.dylib",
        "libarchive.so.13", "libarchive.so", "archive.dll", NULL
    };
    for (int i = 0; names[i]; i++) { void* h = dlopen(names[i], RTLD_NOW | RTLD_LOCAL); if (h) { g_ar.dl = h; break; } }
    if (!g_ar.dl) return 0;
#define GG_AR_SYM(f, n) *(void**)(&g_ar.f) = dlsym(g_ar.dl, n)
    GG_AR_SYM(read_new, "archive_read_new");
    GG_AR_SYM(support_filter_all, "archive_read_support_filter_all");
    GG_AR_SYM(support_format_7zip, "archive_read_support_format_7zip");
    GG_AR_SYM(open_memory, "archive_read_open_memory");
    GG_AR_SYM(next_header, "archive_read_next_header");
    GG_AR_SYM(entry_pathname, "archive_entry_pathname");
    GG_AR_SYM(entry_size, "archive_entry_size");
    GG_AR_SYM(read_data, "archive_read_data");
    GG_AR_SYM(read_free, "archive_read_free");
#undef GG_AR_SYM
    return 1;
}
static int gg_7z_is(const char* b, size_t len) {
    return len >= 6 && b[0] == '7' && b[1] == 'z' && b[2] == (char)0xBC && b[3] == (char)0xAF
        && b[4] == 0x27 && b[5] == 0x1C;
}
static int gg_scan_7z(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                      char* buf, size_t len, gmb_list_t* hits, int* fm, int depth) {
    if (!gg_ar_load() || !g_ar.read_new || !g_ar.open_memory) return 0;
    gg_ar_t* a = g_ar.read_new();
    if (!a) return 0;
    g_ar.support_filter_all(a);
    g_ar.support_format_7zip(a);
    if (g_ar.open_memory(a, buf, len) != 0) { g_ar.read_free(a); return 0; }
    int any = 0;
    gg_ar_entry_t* e;
    while (g_ar.next_header(a, &e) == 0) {
        const char* nm = g_ar.entry_pathname(e);
        long long sz = g_ar.entry_size(e);
        if (!nm || sz <= 0) continue;
        char* m = (char*)malloc((size_t)sz + 1);
        if (!m) break;
        long long got = g_ar.read_data(a, m, (size_t)sz);
        if (got > 0) {
            m[got] = 0;
            char disp[4200];
            snprintf(disp, sizeof(disp), "%s:%s", file, nm);
            any |= gg_scan_buffer(o, re, mat, disp, m, (size_t)got, hits, fm);
        }
        free(m);
    }
    g_ar.read_free(a);
    return any;
}

static int gg_scan_blob(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file,
                        char* data, size_t len, gmb_list_t* hits, int* fm, int depth)
{
    if (depth <= GMB_GREP_MAX_ARCHIVE_DEPTH)
    {
        if (gg_tar_is(data, len)) return gg_scan_tar(o, re, mat, file, data, len, hits, fm, depth);
        if (gg_cpio_is(data, len)) return gg_scan_cpio(o, re, mat, file, data, len, hits, fm, depth);
        if (gg_zip_is(data, len)) return gg_scan_zip(o, re, mat, file, data, len, hits, fm, depth);
        if (gg_7z_is(data, len)) return gg_scan_7z(o, re, mat, file, data, len, hits, fm, depth);
    }
    return gg_scan_buffer(o, re, mat, file, data, len, hits, fm);
}
/* scandisce un singolo file: decomprime se compresso, attraversa archivi (tar/cpio/zip/7z) e PDF,
 * altrimenti cerca direttamente. Ritorna 1 se ha prodotto almeno un hit. */
static int gg_scan_file(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* file, gmb_list_t* hits, int* file_matched) {
    if (gg_is_pdf(file)) return gg_scan_pdf(o, re, mat, file, hits, file_matched);
    size_t len = 0;
    char* buf = gg_read_all(file, &len);
    if (!buf) return 0;
    const char* algo = gg_algo_of_multi(file);
    char* data = buf; size_t dlen = len;
    if (algo) {
        size_t dl = 0;
        unsigned char* dec = gmb_decompress_named(algo, (const unsigned char*)buf, len, &dl);
        free(buf);
        if (!dec) return 0;
        data = (char*)dec; dlen = dl;
    }
    int r;
    { const char* dot = strrchr(file, '.');
      if (dot && !strcasecmp(dot, ".rtf")) {
          size_t tl = 0;
          char* txt = gg_rtf_text((const unsigned char*)data, dlen, &tl);
          if (txt) { r = gg_scan_buffer(o, re, mat, file, txt, tl, hits, file_matched); free(txt); }
          else r = gg_scan_buffer(o, re, mat, file, data, dlen, hits, file_matched);
          free(data);
          return r;
      } }
    r = gg_scan_blob(o, re, mat, file, data, dlen, hits, file_matched, 0);
    free(data);
    return r;
}

/* ── walk ricorsiva ─────────────────────────────────────────────────────────────── */
static int gg_walk(const gg_opts* o, gmb_regex_t* re, gmb_matcher_t* mat, const char* dir, int depth, gmb_list_t* hits, int* nfiles) {
    DIR* dp = opendir(dir);
    if (!dp) return 0;
    struct dirent* de;
    int any = 0;
    while ((de = readdir(dp))) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (!o->hidden && de->d_name[0] == '.') continue;
        if (o->exclude && *o->exclude && gg_glob_list(o->exclude, de->d_name)) continue;
        char path[4096];
        int pl = snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        if (pl <= 0 || pl >= (int)sizeof(path)) continue;
        struct stat st;
        if (lstat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (o->recursive && (o->max_depth == 0 || depth < o->max_depth))
                any |= gg_walk(o, re, mat, path, depth + 1, hits, nfiles);
        } else if (S_ISREG(st.st_mode)) {
            if (!gg_file_selected(o, de->d_name)) continue;
            (*nfiles)++;
            int fm = 0;
            any |= gg_scan_file(o, re, mat, path, hits, &fm);
        }
    }
    closedir(dp);
    return any;
}

/* ── API pubblica ───────────────────────────────────────────────────────────────── */
gmb_val_t gmb_grep_search(gmb_val_t opts, const char* path) {
    gg_opts o; memset(&o, 0, sizeof(o));
    o.pattern = gg_str(opts, "pattern");
    if (!o.pattern) o.pattern = "";
    o.fixed = gg_bool(opts, "fixed");
    o.ignore_case = gg_bool(opts, "ignore_case");
    o.word = gg_bool(opts, "word");
    o.invert = gg_bool(opts, "invert");
    o.recursive = gg_bool(opts, "recursive");
    o.hidden = gg_bool(opts, "hidden");
    o.line_number = gg_bool(opts, "line_number");
    o.column_number = gg_bool(opts, "column_number");
    o.only_matching = gg_bool(opts, "only_matching");
    o.count_only = gg_bool(opts, "count_only");
    o.files_with_matches = gg_bool(opts, "files_with_matches");
    o.max_depth = gg_int(opts, "max_depth", 0);
    o.max_count = gg_int(opts, "max_count", 0);
    o.ctx_before = gg_int(opts, "context_before", 0);
    o.ctx_after = gg_int(opts, "context_after", 0);
    o.include = gg_str(opts, "include");
    o.exclude = gg_str(opts, "exclude");
    o.type = gg_str(opts, "type");

    gmb_list_t hits = gmb_list_new();
    gmb_regex_t* re = NULL;
    gmb_matcher_t* mat = NULL;
    if (o.fixed && o.pattern[0]) {
        mat = gmb_matcher_new(o.pattern, o.ignore_case, o.word);
    }
    if (!o.fixed && o.pattern[0]) {
        /* compila una volta (case-insensitive via (?i) inline se richiesto) */
        if (o.ignore_case) {
            size_t pl = strlen(o.pattern);
            char* ic = (char*)malloc(pl + 5);
            if (ic) { memcpy(ic, "(?i)", 4); memcpy(ic + 4, o.pattern, pl + 1); re = gmb_regex_new(ic); free(ic); }
        }
        if (!re) re = gmb_regex_new(o.pattern);
    }

    struct stat st;
    if (path && stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            int nf = 0;
            gg_walk(&o, re, mat, path, 0, &hits, &nf);
        } else if (S_ISREG(st.st_mode)) {
            const char* nm = strrchr(path, '/'); nm = nm ? nm + 1 : path;
            int fm = 0;
            gg_scan_file(&o, re, mat, path, &hits, &fm);
        }
    }
    if (re) gmb_regex_free(re);
    if (mat) gmb_matcher_free(mat);
    return gmb_make_val_list(hits);
}

gmb_int_t gmb_grep_count(const gmb_val_t hits) {
    return gmb_list_count(gmb_val_as_list(hits));
}

gmb_string_t gmb_grep_format(const gmb_val_t hits, int with_line, int with_column) {
    gmb_list_t l = gmb_val_as_list(hits);
    gmb_int_t n = gmb_list_count(l);
    size_t cap = 256, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("");
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_val_t e = gmb_list_get(l, i);
        gmb_dict_t d = gmb_val_as_dict(e);
        const char* file = ""; const char* text = "";
        gmb_val_t fv = gmb_dict_get(d, "file"); if (fv.kind == GMB_TYPE_STRING && fv.v_string.data) file = fv.v_string.data;
        gmb_val_t tv = gmb_dict_get(d, "text"); if (tv.kind == GMB_TYPE_STRING && tv.v_string.data) text = tv.v_string.data;
        gmb_int_t line = gmb_val_to_int(gmb_dict_get(d, "line"));
        gmb_int_t col = gmb_val_to_int(gmb_dict_get(d, "column"));
        gmb_int_t ctx = gmb_val_to_int(gmb_dict_get(d, "ctx"));
        /* separatore fra gruppi di context: "--" */
        if (ctx == 2) {
            while (len + 4 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
            memcpy(buf + len, "--\n", 3); len += 3;
            continue;
        }
        char head[64]; head[0] = 0;
        if (ctx == 1) snprintf(head, sizeof(head), "%lld-", (long long)line);          /* context: file-riga-testo */
        else if (with_line && with_column) snprintf(head, sizeof(head), "%lld:%lld:", (long long)line, (long long)col);
        else if (with_line) snprintf(head, sizeof(head), "%lld:", (long long)line);
        char sep = (ctx == 1) ? '-' : ':';
        size_t hl = strlen(head), fl = strlen(file), tl = strlen(text);
        while (len + hl + fl + 1 + tl + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        memcpy(buf + len, file, fl); len += fl;
        if (hl) { buf[len++] = sep; memcpy(buf + len, head, hl); len += hl; }
        else buf[len++] = sep;
        memcpy(buf + len, text, tl); len += tl;
        buf[len++] = '\n';
    }
    buf[len] = 0;
    gmb_string_t out = gmb_string_alloc(buf);
    free(buf);
    return out;
}

int gmb_grep_selftest(void) {
    /* compila e matcha un pattern su una stringa (nessun I/O) */
    gmb_regex_t* re = gmb_regex_new("[0-9]+");
    if (!re) return 0;
    int idx = -1, len = 0;
    int ok = gmb_regex_match_obj(re, "abc123", NULL, 0, &idx, &len);
    gmb_regex_free(re);
    return ok && idx == 3 && len == 3;
}

/* ── Superficie di linguaggio GREP.* (default: ricorsivo, con numeri di riga/colonna) ── */
static gmb_val_t gg_opts_default(const char* pattern) {
    gmb_dict_t d = gmb_dict_new();
    gmb_dict_set(&d, "pattern", gmb_make_val_string(pattern ? pattern : ""));
    gmb_dict_set(&d, "recursive", gmb_make_val_int(1));
    gmb_dict_set(&d, "line_number", gmb_make_val_int(1));
    gmb_dict_set(&d, "column_number", gmb_make_val_int(1));
    return gmb_make_val_dict(d);
}

/* GREP.MATCHES(pattern, path) → lista di stringhe "file:line:col:text" (una per hit). */
gmb_list_t gmb_grep_matches(const char* pattern, const char* path) {
    gmb_val_t opts = gg_opts_default(pattern);
    gmb_val_t hits = gmb_grep_search(opts, path);
    gmb_list_t l = gmb_val_as_list(hits);
    gmb_int_t n = gmb_list_count(l);
    gmb_list_t out = gmb_list_new();
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_dict_t d = gmb_val_as_dict(gmb_list_get(l, i));
        const char* file = "";
        gmb_val_t fv = gmb_dict_get(d, "file");
        if (fv.kind == GMB_TYPE_STRING && fv.v_string.data) file = fv.v_string.data;
        gmb_val_t tv = gmb_dict_get(d, "text");
        const char* text = (tv.kind == GMB_TYPE_STRING && tv.v_string.data) ? tv.v_string.data : "";
        gmb_int_t line = gmb_val_to_int(gmb_dict_get(d, "line"));
        gmb_int_t col = gmb_val_to_int(gmb_dict_get(d, "column"));
        size_t need = strlen(file) + strlen(text) + 64;
        char* buf = (char*)malloc(need);
        if (!buf) continue;
        snprintf(buf, need, "%s:%lld:%lld:%s", file, (long long)line, (long long)col, text);
        gmb_list_add(&out, gmb_make_val_string(buf));
        free(buf);
    }
    return out;
}

/* GREP.COUNT(pattern, path) → numero di hit. */
gmb_int_t gmb_grep_count_path(const char* pattern, const char* path) {
    gmb_val_t opts = gg_opts_default(pattern);
    gmb_val_t hits = gmb_grep_search(opts, path);
    return gmb_grep_count(hits);
}

/* GREP.MATCHES(pattern, path) → STRING "file:line:col:text\n..." (una riga per hit). */
gmb_string_t gmb_grep_matches_str(const char* pattern, const char* path) {
    gmb_val_t opts = gg_opts_default(pattern);
    gmb_val_t hits = gmb_grep_search(opts, path);
    return gmb_grep_format(hits, 1, 1);
}

/* GREP.MATCHES(pattern, path) → ARRAY di stringhe (gmb_string_t* + *out_n). */
gmb_string_t* gmb_grep_matches_arr(const char* pattern, const char* path, gmb_int_t* out_n) {
    gmb_list_t m = gmb_grep_matches(pattern, path);
    gmb_int_t n = gmb_list_count(m);
    if (out_n) *out_n = n;
    gmb_string_t* out = (gmb_string_t*)calloc((size_t)(n > 0 ? n : 1), sizeof(gmb_string_t));
    if (!out) return NULL;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_val_t v = gmb_list_get(m, i);
        const char* s = (v.kind == GMB_TYPE_STRING && v.v_string.data) ? v.v_string.data : "";
        out[i] = gmb_string_alloc(s);
    }
    return out;
}

/* GREP.FILES(pattern, path) → ARRAY di stringhe (nomi file distinti). */
gmb_string_t* gmb_grep_files_arr(const char* pattern, const char* path, gmb_int_t* out_n) {
    gmb_list_t f = gmb_grep_files(pattern, path);
    gmb_int_t n = gmb_list_count(f);
    if (out_n) *out_n = n;
    gmb_string_t* out = (gmb_string_t*)calloc((size_t)(n > 0 ? n : 1), sizeof(gmb_string_t));
    if (!out) return NULL;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_val_t v = gmb_list_get(f, i);
        const char* s = (v.kind == GMB_TYPE_STRING && v.v_string.data) ? v.v_string.data : "";
        out[i] = gmb_string_alloc(s);
    }
    return out;
}

/* GREP.FILES(pattern, path) → STRING con i nomi file distinti, uno per riga. */
gmb_string_t gmb_grep_files_str(const char* pattern, const char* path) {
    gmb_list_t f = gmb_grep_files(pattern, path);
    gmb_int_t n = gmb_list_count(f);
    size_t cap = 256, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("");
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_val_t v = gmb_list_get(f, i);
        if (v.kind != GMB_TYPE_STRING || !v.v_string.data) continue;
        size_t sl = strlen(v.v_string.data);
        while (len + sl + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        memcpy(buf + len, v.v_string.data, sl); len += sl; buf[len++] = '\n';
    }
    buf[len] = 0;
    gmb_string_t out = gmb_string_alloc(buf);
    free(buf);
    return out;
}

/* GREP.FILES(pattern, path) → lista di nomi file (distinti, ordine di apparizione). */
gmb_list_t gmb_grep_files(const char* pattern, const char* path) {
    gmb_val_t opts = gg_opts_default(pattern);
    gmb_val_t hits = gmb_grep_search(opts, path);
    gmb_list_t l = gmb_val_as_list(hits);
    gmb_int_t n = gmb_list_count(l);
    gmb_list_t out = gmb_list_new();
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_dict_t d = gmb_val_as_dict(gmb_list_get(l, i));
        gmb_val_t fv = gmb_dict_get(d, "file");
        const char* file = (fv.kind == GMB_TYPE_STRING && fv.v_string.data) ? fv.v_string.data : "";
        int seen = 0;
        gmb_int_t m = gmb_list_count(out);
        for (gmb_int_t k = 0; k < m; k++) {
            gmb_val_t ev = gmb_list_get(out, k);
            if (ev.kind == GMB_TYPE_STRING && ev.v_string.data && strcmp(ev.v_string.data, file) == 0) { seen = 1; break; }
        }
        if (!seen) gmb_list_add(&out, gmb_make_val_string(file));
    }
    return out;
}

/* JSON: array di stringhe (per P/Invoke C#). Richiede il parse lato C#. */
static char* gg_json_of_list(gmb_list_t l) {
    gmb_string_t js = gmb_val_to_json(gmb_make_val_list(l));
    const char* s = (js.data && js.data[0]) ? js.data : "[]";
    size_t n = strlen(s);
    char* out = (char*)malloc(n + 1);
    if (out) memcpy(out, s, n + 1);
    return out;
}
char* gmb_grep_matches_json(const char* pattern, const char* path) {
    return gg_json_of_list(gmb_grep_matches(pattern, path));
}
char* gmb_grep_files_json(const char* pattern, const char* path) {
    return gg_json_of_list(gmb_grep_files(pattern, path));
}
/* Testo (char*, malloc → gmb_grep_free) per P/Invoke diretto della superficie GREP.*. */
static char* gg_dup_str(const gmb_string_t s) {
    const char* t = s.data ? s.data : "";
    size_t n = strlen(t);
    char* out = (char*)malloc(n + 1);
    if (out) memcpy(out, t, n + 1);
    return out;
}
char* gmb_grep_matches_text(const char* pattern, const char* path) {
    return gg_dup_str(gmb_grep_matches_str(pattern, path));
}
char* gmb_grep_files_text(const char* pattern, const char* path) {
    return gg_dup_str(gmb_grep_files_str(pattern, path));
}

/* ── Variante JSON (ABI stabile per P/Invoke C# e per la REPL C) ──────────────────
 * opts_json = oggetto JSON delle opzioni (stesse chiavi del dict). Ritorna un JSON
 * array di hit (malloc: liberare con gmb_grep_free). */
char* gmb_grep_search_json(const char* opts_json, const char* path) {
    gmb_val_t opts = gmb_json_parse(opts_json && *opts_json ? opts_json : "{}");
    gmb_val_t hits = gmb_grep_search(opts, path);
    gmb_string_t js = gmb_val_to_json(hits);
    const char* s = (js.data && js.data[0]) ? js.data : "[]";
    size_t n = strlen(s);
    char* out = (char*)malloc(n + 1);
    if (out) { memcpy(out, s, n + 1); }
    return out;
}

/* Libera una stringa ritornata da gmb_grep_search_json. */
void gmb_grep_free(char* p) { free(p); }

/* Formatta un array JSON di hit (ritornato da gmb_grep_search_json) come testo
 * "file:line:column:text\n". Ritorna una stringa malloc (gmb_grep_free). */
char* gmb_grep_format_json(const char* hits_json, int with_line, int with_column) {
    gmb_val_t hits = gmb_json_parse(hits_json && *hits_json ? hits_json : "[]");
    gmb_string_t s = gmb_grep_format(hits, with_line, with_column);
    const char* t = s.data ? s.data : "";
    size_t n = strlen(t);
    char* out = (char*)malloc(n + 1);
    if (out) memcpy(out, t, n + 1);
    return out;
}
