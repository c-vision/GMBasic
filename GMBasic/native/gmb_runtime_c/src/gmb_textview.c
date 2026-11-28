/* ============================================================================
 * gmb_textview.c — vista testuale virtuale (Markdown / diff / lista grande).
 *
 * Il costo del disegno dipende SOLO dalle righe visibili: le righe sono
 * impaginate una volta (all'assegnazione del testo e, per il Markdown, quando
 * cambia la larghezza) e il renderer chiede `gmb_textview_row(i)`.
 *
 * Plain/diff non hanno wrapping: le righe sono i '\n' del testo, quindi la
 * larghezza non le cambia (layout no-op). Markdown usa l'AST canonico di
 * gmb_md (nessun parsing duplicato) e il wrapping UTF-8 di gmb_str_wrap, per
 * avere la stessa resa di WRAP() su tutte le gambe.
 *
 * Memoria: il testo di partenza vive in `src` (la vista lo POSSIEDE, sempre) e
 * le righe in `buf`. In plain/diff le righe sono COPIE delle righe sorgente:
 * così cambiare `mode=` dallo STYLE non perde la sorgente.
 * ============================================================================ */
#include "gmb_textview.h"
#include "gmb_md.h"
#include "gmb_runtime.h"   /* gmb_str_wrap, gmb_string_alloc/free */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct gmb_textview {
    char* src;        /* testo di partenza, sempre posseduto */
    int mode;
    int width;        /* larghezza dell'ultimo impaginato (colonne) */
    char* buf;        /* righe impaginate (copy, mai la sorgente) */
    int buflen, bufcap;
    int* off;         /* inizio di ogni riga in `buf` */
    int* len;         /* lunghezza in byte di ogni riga */
    unsigned char* sty;
    int nrows, cap;
};

/* ── numeri di codepoint UTF-8 (i byte 10xxxxxx non contano) ── */
static int tv_cp(const char* s)
{
    int k = 0;
    for (; s && *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) k++;
    return k;
}

int gmb_textview_parse_mode(const char* name)
{
    if (!name || !*name) return GMB_TV_PLAIN;
    if (!strcasecmp(name, "plain") || !strcasecmp(name, "text") || !strcasecmp(name, "list")
        || !strcasecmp(name, "virtual") || !strcmp(name, "0")) return GMB_TV_PLAIN;
    if (!strcasecmp(name, "diff") || !strcasecmp(name, "patch") || !strcasecmp(name, "unified")) return GMB_TV_DIFF;
    if (!strcasecmp(name, "markdown") || !strcasecmp(name, "md") || !strcasecmp(name, "doc")) return GMB_TV_MARKDOWN;
    return -1;
}

const char* gmb_textview_mode_name(int mode)
{
    switch (mode)
    {
        case GMB_TV_DIFF:     return "diff";
        case GMB_TV_MARKDOWN: return "markdown";
        default:              return "plain";
    }
}

/* ── accumulo righe ─────────────────────────────────────────────────────── */
static int tv_reserve_row(gmb_textview_t* tv)
{
    if (tv->nrows < tv->cap) return 1;
    int nc = tv->cap ? tv->cap * 2 : 64;
    int* no = (int*)realloc(tv->off, (size_t)nc * sizeof(int));
    if (!no) return 0;
    tv->off = no;
    int* nl = (int*)realloc(tv->len, (size_t)nc * sizeof(int));
    if (!nl) return 0;
    tv->len = nl;
    unsigned char* ns = (unsigned char*)realloc(tv->sty, (size_t)nc);
    if (!ns) return 0;
    tv->sty = ns;
    tv->cap = nc;
    return 1;
}

static int tv_buf_need(gmb_textview_t* tv, int extra)
{
    if (tv->buflen + extra <= tv->bufcap) return 1;
    int nc = tv->bufcap ? tv->bufcap : 4096;
    while (nc < tv->buflen + extra) nc *= 2;
    char* nb = (char*)realloc(tv->buf, (size_t)nc);
    if (!nb) return 0;
    tv->buf = nb;
    tv->bufcap = nc;
    return 1;
}

/* Aggiunge una riga di `len` byte (copia), eventualmente preceduta da `pref`. */
static int tv_push_np(gmb_textview_t* tv, const char* pref, const char* text, int len, unsigned char style)
{
    if (len < 0) len = text ? (int)strlen(text) : 0;
    if (!tv_reserve_row(tv)) return 0;
    int ul = pref ? (int)strlen(pref) : 0;
    if (!tv_buf_need(tv, ul + len + 1)) return 0;
    int base = tv->buflen;
    if (ul > 0) memcpy(tv->buf + base, pref, (size_t)ul);
    if (len > 0) memcpy(tv->buf + base + ul, text, (size_t)len);
    tv->buf[base + ul + len] = '\0';
    tv->off[tv->nrows] = base;
    tv->len[tv->nrows] = ul + len;
    tv->sty[tv->nrows] = style;
    tv->buflen = base + ul + len + 1;
    tv->nrows++;
    return 1;
}
static int tv_push_n(gmb_textview_t* tv, const char* text, int len, unsigned char style)
{
    return tv_push_np(tv, NULL, text, len, style);
}
static int tv_push(gmb_textview_t* tv, const char* text, unsigned char style)
{
    return tv_push_n(tv, text, text ? (int)strlen(text) : 0, style);
}

/* Aggiunge `text` impaginato a `width` colonne, con prefisso sulla prima riga e
 * rientro (stesso numero di colonne) sulle continuazioni. */
static int tv_push_wrapped(gmb_textview_t* tv, const char* text, int width,
                           unsigned char style, const char* prefix, const char* cont)
{
    int pl = tv_cp(prefix), cl = tv_cp(cont);
    int w = width - (pl > cl ? pl : cl);
    if (w < 4) w = 4;
    gmb_string_t in = gmb_string_alloc(text ? text : "");
    gmb_string_t out = gmb_str_wrap(in, (gmb_int_t)w, 0);
    gmb_string_free(&in);
    const char* p = out.data ? out.data : "";
    int ok = 1, first = 1;
    while (ok)
    {
        const char* nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        /* gmb_str_wrap pad-right: gli spazi finali non servono, il renderer
         * riempie comunque la cella. Toglierli rende leggibile il readback. */
        int t = len;
        while (t > 0 && p[t - 1] == ' ') t--;
        if (!tv_push_np(tv, first ? prefix : cont, p, t, style)) { ok = 0; break; }
        first = 0;
        if (!nl) break;
        p = nl + 1;
        if (!*p) break;
    }
    gmb_string_free(&out);
    return ok;
}

static void tv_clear_rows(gmb_textview_t* tv)
{
    tv->nrows = 0;
    tv->buflen = 0;
}

/* ── costruzione per modo ───────────────────────────────────────────────── */

/* Plain/diff: ogni '\n' è una riga (copiata in `buf`). */
static int tv_build_lines(gmb_textview_t* tv)
{
    tv_clear_rows(tv);
    const char* s = tv->src;
    if (!s || !*s) return 0;   /* sorgente vuota = nessuna riga (non "una riga vuota") */
    int i = 0;
    while (1)
    {
        int start = i;
        while (s[i] && s[i] != '\n') i++;
        unsigned char st = GMB_TV_S_NORMAL;
        if (tv->mode == GMB_TV_DIFF)
        {
            const char* l = s + start;
            if (!strncmp(l, "+++", 3) || !strncmp(l, "---", 3)) st = GMB_TV_S_META;
            else if (!strncmp(l, "@@", 2)) st = GMB_TV_S_HUNK;
            else if (!strncmp(l, "diff ", 5) || !strncmp(l, "index ", 6)
                     || !strncmp(l, "\\ ", 2) || !strncmp(l, "new file", 8)
                     || !strncmp(l, "deleted file", 12)) st = GMB_TV_S_META;
            else if (l[0] == '+') st = GMB_TV_S_ADD;
            else if (l[0] == '-') st = GMB_TV_S_DEL;
        }
        if (!tv_push_n(tv, s + start, i - start, st)) return tv->nrows;
        if (!s[i]) break;
        i++;
    }
    /* Il '\n' finale di un file è un terminatore, non una riga vuota in più:
     * "a\n" = 1 riga; "a\n\n" = 2 (la riga vuota voluta resta). */
    if (tv->nrows > 1 && tv->len[tv->nrows - 1] == 0) tv->nrows--;
    return tv->nrows;
}

static int tv_find_val(const char* spec, const char* key, char* out, size_t cap)
{
    const char* p = spec;
    size_t kl = strlen(key);
    while (p && *p)
    {
        while (*p == ' ' || *p == ';' || *p == ',') p++;
        if (!strncasecmp(p, key, kl) && p[kl] == '=')
        {
            p += kl + 1;
            size_t n = 0;
            while (*p && *p != ' ' && *p != ';' && n + 1 < cap) out[n++] = *p++;
            out[n] = '\0';
            return 1;
        }
        while (*p && *p != ' ' && *p != ';' && *p != ',') p++;
    }
    return 0;
}
/* Usato dall'applicazione dello STYLE: esposta per non duplicare il parser. */
int gmb_textview_style_mode(const char* spec, int current)
{
    char v[32];
    if (spec && tv_find_val(spec, "mode", v, sizeof v))
    {
        int m = gmb_textview_parse_mode(v);
        return m < 0 ? current : m;
    }
    if (spec && tv_find_val(spec, "virtual", v, sizeof v))
    {
        if (atoi(v) != 0 || !strcasecmp(v, "true") || !strcasecmp(v, "on")) return GMB_TV_PLAIN;
    }
    return current;
}

static int tv_build_markdown(gmb_textview_t* tv)
{
    int width = tv->width;
    /* `usable` è la larghezza del TESTO, già al netto del rientro di 1 colonna che
     * il renderer applica al contenuto della casella (così nessuna riga sborda). */
    int inner = width > 2 ? width - 2 : (width > 0 ? width : 1);
    int usable = inner;
    if (usable < 4) usable = 4;
    tv_clear_rows(tv);
    gmb_val_t doc = gmb_md_parse(tv->src ? tv->src : "");
    int n = gmb_md_block_count(doc);
    for (int i = 0; i < n; i++)
    {
        const char* type = gmb_md_block_type(doc, i);
        if (i > 0) tv_push(tv, "", GMB_TV_S_NORMAL);
        if (!strcmp(type, "heading"))
        {
            int lv = gmb_md_block_level(doc, i);
            unsigned char st = lv <= 1 ? GMB_TV_S_HEAD1 : (lv == 2 ? GMB_TV_S_HEAD2 : GMB_TV_S_HEAD3);
            tv_push_wrapped(tv, gmb_md_block_text(doc, i), usable, st, "", "");
        }
        else if (!strcmp(type, "paragraph") || !strcmp(type, "quote"))
        {
            int quote = !strcmp(type, "quote");
            tv_push_wrapped(tv, gmb_md_block_text(doc, i), usable,
                            quote ? GMB_TV_S_QUOTE : GMB_TV_S_NORMAL,
                            quote ? "│ " : "", quote ? "│ " : "");
        }
        else if (!strcmp(type, "list"))
        {
            int ordered = gmb_md_block_ordered(doc, i);
            int m = gmb_md_block_item_count(doc, i);
            for (int j = 0; j < m; j++)
            {
                char pref[16];
                if (ordered) snprintf(pref, sizeof pref, "%d. ", j + 1);
                else snprintf(pref, sizeof pref, "• ");
                int cl = tv_cp(pref);
                char cont[16];
                int k = 0;
                while (k < cl && k < (int)sizeof cont - 1) { cont[k] = ' '; k++; }
                cont[k] = '\0';
                tv_push_wrapped(tv, gmb_md_block_item(doc, i, j), inner, GMB_TV_S_NORMAL, pref, cont);
            }
        }
        else if (!strcmp(type, "codeblock"))
        {
            const char* code = gmb_md_block_text(doc, i);
            const char* p = code ? code : "";
            while (1)
            {
                const char* nl = strchr(p, '\n');
                int len = nl ? (int)(nl - p) : (int)strlen(p);
                tv_push_n(tv, p, len, GMB_TV_S_CODE);
                if (!nl) break;
                p = nl + 1;
            }
        }
        else if (!strcmp(type, "table"))
        {
            int cols = gmb_md_block_cols(doc, i);
            int rows = gmb_md_block_row_count(doc, i);
            if (cols <= 0) continue;
            /* larghezza colonne = max codepoint, ridotta se non ci sta */
            int cw[32];
            if (cols > 32) cols = 32;
            int total = 0;
            for (int c = 0; c < cols; c++)
            {
                int mw = tv_cp(gmb_md_block_cell(doc, i, -1, c));
                for (int r = 0; r < rows; r++)
                {
                    int w = tv_cp(gmb_md_block_cell(doc, i, r, c));
                    if (w > mw) mw = w;
                }
                if (mw > inner / 2) mw = inner / 2;
                cw[c] = mw;
                total += mw;
            }
            if (total + 3 * (cols - 1) > inner)
            {
                int avail = inner - 3 * (cols - 1);
                if (avail < cols) avail = cols;
                int sum = 0;
                for (int c = 0; c < cols; c++) sum += cw[c];
                if (sum > 0)
                    for (int c = 0; c < cols; c++) cw[c] = (cw[c] * avail) / sum;
            }
            int capn = inner * 4 + 64;
            char* line = (char*)malloc((size_t)capn);
            if (!line) continue;
            for (int r = -1; r < rows; r++)
            {
                int o = 0;
                for (int c = 0; c < cols && o < capn - 8; c++)
                {
                    const char* cell = gmb_md_block_cell(doc, i, r, c);
                    if (c > 0) { memcpy(line + o, " | ", 3); o += 3; }
                    int cnt = 0;
                    for (const char* q = cell; *q && cnt < cw[c] && o < capn - 6; q++)
                    {
                        line[o++] = *q;
                        if (((unsigned char)*q & 0xC0) != 0x80) cnt++;
                    }
                    while (cnt < cw[c] && o < capn - 4) { line[o++] = ' '; cnt++; }
                }
                line[o] = '\0';
                tv_push_n(tv, line, o, r < 0 ? GMB_TV_S_HEAD3 : GMB_TV_S_CODE);
                if (r < 0)
                {
                    int u = 0;
                    for (int c = 0; c < cols && u < capn - 6; c++)
                    {
                        if (c > 0) { line[u++] = '-'; line[u++] = '+'; line[u++] = '-'; }
                        for (int k = 0; k < cw[c] && u < capn - 4; k++) line[u++] = '-';
                    }
                    line[u] = '\0';
                    tv_push_n(tv, line, u, GMB_TV_S_RULE);
                }
            }
            free(line);
        }
        else if (!strcmp(type, "hr"))
        {
            int w = inner > 512 ? 512 : inner;
            char rule[520];
            int o = 0;
            for (int k = 0; k < w; k++) rule[o++] = '-';
            rule[o] = '\0';
            tv_push_n(tv, rule, o, GMB_TV_S_RULE);
        }
        else
        {
            const char* t = gmb_md_block_text(doc, i);
            if (t && *t) tv_push_wrapped(tv, t, usable, GMB_TV_S_NORMAL, "", "");
        }
    }
    return tv->nrows;
}

/* ── API ────────────────────────────────────────────────────────────────── */
int gmb_textview_layout(gmb_textview_t* tv, int width)
{
    if (!tv) return 0;
    if (width <= 0) width = 1;
    if (tv->mode != GMB_TV_MARKDOWN) { tv->width = width; return tv->nrows; }   /* nessun wrapping */
    if (tv->width == width) return tv->nrows;                                   /* già impaginato */
    tv->width = width;
    return tv_build_markdown(tv);
}

gmb_textview_t* gmb_textview_new(const char* src, int mode, int width)
{
    gmb_textview_t* tv = (gmb_textview_t*)calloc(1, sizeof(*tv));
    if (!tv) return NULL;
    tv->mode = mode;
    tv->width = width > 0 ? width : 1;
    tv->src = strdup(src ? src : "");
    if (!tv->src) { free(tv); return NULL; }
    if (tv->mode == GMB_TV_MARKDOWN) tv_build_markdown(tv);
    else tv_build_lines(tv);
    return tv;
}

void gmb_textview_free(gmb_textview_t* tv)
{
    if (!tv) return;
    free(tv->src);
    free(tv->buf);
    free(tv->off);
    free(tv->len);
    free(tv->sty);
    free(tv);
}

int gmb_textview_rows(const gmb_textview_t* tv) { return tv ? tv->nrows : 0; }
int gmb_textview_mode(const gmb_textview_t* tv) { return tv ? tv->mode : GMB_TV_PLAIN; }
int gmb_textview_width(const gmb_textview_t* tv) { return tv ? tv->width : 0; }

const char* gmb_textview_source_text(gmb_textview_t* tv)
{
    return (tv && tv->src) ? tv->src : "";
}

const char* gmb_textview_row(const gmb_textview_t* tv, int idx)
{
    if (!tv || idx < 0 || idx >= tv->nrows || !tv->buf) return "";
    return tv->buf + tv->off[idx];
}

int gmb_textview_row_style(const gmb_textview_t* tv, int idx)
{
    if (!tv || idx < 0 || idx >= tv->nrows) return GMB_TV_S_NORMAL;
    return tv->sty[idx];
}