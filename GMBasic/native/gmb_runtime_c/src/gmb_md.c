/* gmb_md.c — Markdown AST in C (Fase 5, parity MdSupport C#). Parser block-level →
 * dict/list boxed (gmb_val_t) compatibili BASIC. Usato da C99 (transpiler) e VM/C. */
#include "gmb_md.h"
#include "gmb_strbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* ── accessor boxed ── */
static gmb_dict_t* md_dict_of(gmb_val_t v)
{
    if (v.kind != GMB_TYPE_OBJECT) return NULL;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    return (o && o->data) ? (gmb_dict_t*)o->data : NULL;
}
static gmb_list_t* md_list_of(gmb_val_t v)
{
    if (v.kind != GMB_TYPE_OBJECT) return NULL;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    return (o && o->data) ? (gmb_list_t*)o->data : NULL;
}
static gmb_val_t md_get(gmb_val_t v, const char* key)
{
    gmb_dict_t* d = md_dict_of(v);
    return d ? gmb_dict_get(*d, key) : gmb_val_nil();
}
static const char* md_str(gmb_val_t v) { return v.kind == GMB_TYPE_STRING ? v.v_string.data : ""; }
static double md_num(gmb_val_t v) { return v.kind == GMB_TYPE_DOUBLE ? v.v_double : 0.0; }

static gmb_val_t blk(const char* type)
{
    gmb_dict_t d = gmb_dict_new();
    gmb_val_t v = gmb_make_val_dict_take(&d);
    gmb_val_dict_set(v, "type", gmb_make_val_string(type));
    return v;
}
static gmb_val_t md_strv(const char* s) { return gmb_make_val_string(s ? s : ""); }
static gmb_val_t md_sv(const char* s, size_t n);

/* ── inline: togli bold/italic/code/link → testo semplice ── */
static void inline_to_text(const char* s, gmb_strbuf_t* out)
{
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++)
    {
        char c = s[i];
        if (c == '[')
        {
            const char* close = strchr(s + i + 1, ']');
            if (close && close[1] == '(')
            {
                const char* p = strchr(close + 2, ')');
                if (p) { gmb_strbuf_append(out, s + i + 1, (size_t)(close - s - i - 1)); i = (size_t)(p - s); continue; }
            }
        }
        if (c == '!' && i + 1 < n && s[i + 1] == '[')
        {
            const char* close = strchr(s + i + 2, ']');
            if (close && close[1] == '(')
            {
                const char* p = strchr(close + 2, ')');
                if (p) { gmb_strbuf_append(out, s + i + 2, (size_t)(close - s - i - 2)); i = (size_t)(p - s); continue; }
            }
        }
        if (c == '`')
        {
            const char* close = strchr(s + i + 1, '`');
            if (close) { gmb_strbuf_append(out, s + i + 1, (size_t)(close - s - i - 1)); i = (size_t)(close - s); continue; }
        }
        if (c == '*' || c == '_' || c == '#' || c == '>' || c == '~') continue;
        gmb_strbuf_append(out, &c, 1);
    }
}

/* estrai link [t](u) da raw → aggiungi a lista gmb_list_t* */
static void raw_links(const char* s, gmb_list_t* out)
{
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++)
    {
        if (s[i] != '[') continue;
        const char* rb = strchr(s + i + 1, ']');
        if (!rb || rb[1] != '(') continue;
        const char* rp = strchr(rb + 2, ')');
        if (!rp) continue;
        gmb_val_t l = gmb_make_val_dict(gmb_dict_new());
        gmb_val_dict_set(l, "text", md_sv(s + i + 1, (size_t)(rb - s - i - 1)));
        gmb_val_dict_set(l, "url", md_sv(rb + 2, (size_t)(rp - rb - 2)));
        gmb_list_add(out, l);
        i = (size_t)(rp - s);
    }
}

static gmb_val_t md_sv(const char* s, size_t n)
{
    char* tmp = (char*)malloc(n + 1);
    if (!tmp) return gmb_val_nil();
    memcpy(tmp, s, n); tmp[n] = 0;
    gmb_val_t v = gmb_make_val_string(tmp);
    free(tmp);
    return v;
}

/* trim di una cella tabella (parità MdSupport.ParseTableRow: c.Trim()). */
static char* md_trim(char* s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0;
    return s;
}

static gmb_bool_t is_delim_row(const char* t)
{
    if (!t || t[0] != '|') return 0;
    size_t n = 0;
    while (t[n] && t[n] != '\n') n++;
    if (n < 3) return 0;
    for (size_t i = 1; i + 1 < n; i++)
    {
        char c = t[i];
        if (c != '-' && c != ':' && c != ' ' && c != '|') return 0;
    }
    return 1;
}

static gmb_bool_t is_list_line(const char* t) { return strncmp(t, "- ", 2) == 0 || strncmp(t, "* ", 2) == 0; }
static gmb_bool_t is_num_list(const char* t)
{
    if (t[0] < '0' || t[0] > '9') return 0;
    const char* p = t;
    while (*p && isdigit((unsigned char)*p)) p++;
    return *p == '.' && p[1] == ' ';
}

/* ── PARSE: text → dict {type, blocks:[...]} ── */
gmb_val_t gmb_md_parse(const char* text)
{
    gmb_val_t doc = blk("document");
    gmb_list_t blocks = gmb_list_new();
    const char* p = text ? text : "";
    long guard = 0;
    while (*p && guard++ < 1000000)
    {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char* line = md_sv(p, len).v_string.data; free(line); line = (char*)malloc(len+1); memcpy(line, p, len); line[len]=0;
    const char* t = line;
        while (*t == ' ') t++;
        int h = 0;
        while (t[h] == '#') h++;
        if (h >= 1 && h <= 6 && (t[h] == ' ' || t[h] == 0))
        {
            gmb_val_t b = blk("heading");
            gmb_val_dict_set(b, "level", gmb_make_val_double((double)h));
            gmb_strbuf_t sb = gmb_strbuf_new();
            inline_to_text(t + h + (t[h] == ' ' ? 1 : 0), &sb);
            gmb_val_dict_set(b, "text", md_strv(sb.data));
            gmb_strbuf_free(&sb);
            gmb_list_add(&blocks, b);
            free(line);
            p = nl ? nl + 1 : p + len;
            continue;
        }
        if (strncmp(t, "```", 3) == 0)
        {
            gmb_val_t b = blk("codeblock");
            const char* lang = t + 3;
            while (*lang == ' ') lang++;
            char* lc = (char*)malloc(strlen(lang)+1); strcpy(lc, lang);
            gmb_val_dict_set(b, "language", md_strv(lc)); free(lc);
            gmb_strbuf_t sb = gmb_strbuf_new();
            p = nl ? nl + 1 : p + len;
            while (*p)
            {
                const char* nl2 = strchr(p, '\n');
                size_t len2 = nl2 ? (size_t)(nl2 - p) : strlen(p);
                if (len2 >= 3 && strncmp(p, "```", 3) == 0) { p = nl2 ? nl2 + 1 : p + len2; break; }
                if (sb.length > 0) gmb_strbuf_append(&sb, "\n", 1);
                gmb_strbuf_append(&sb, p, len2);
                p = nl2 ? nl2 + 1 : p + len2;
            }
            gmb_val_dict_set(b, "text", md_strv(sb.data));
            gmb_strbuf_free(&sb);
            gmb_list_add(&blocks, b);
            free(line);
            continue;
        }
        if (t[0] == '|' && nl && is_delim_row(nl + 1))
        {
            gmb_val_t b = blk("table");
            gmb_list_t header = gmb_list_new();
            gmb_list_t rows = gmb_list_new();
            const char* hc = t;
            while (*hc == '|') hc++;
            char hbuf[512]; int hi = 0;
            for (const char* q = hc; *q; q++)
            {
                if (*q == '|' || *q == '\n') { hbuf[hi] = 0; hi = 0; gmb_strbuf_t sb = gmb_strbuf_new(); inline_to_text(md_trim(hbuf), &sb); gmb_list_add(&header, md_strv(sb.data)); gmb_strbuf_free(&sb); if (*q == '\n') break; }
                else if (hi < 511) hbuf[hi++] = *q;
            }
            gmb_val_dict_set(b, "header", gmb_make_val_list_take(&header));
            p = nl ? nl + 1 : p + len;
            nl = strchr(p, '\n');
            p = nl ? nl + 1 : p + strlen(p);
            while (*p)
            {
                const char* rnl = strchr(p, '\n');
                size_t rl = rnl ? (size_t)(rnl - p) : strlen(p);
                char* rl2 = (char*)malloc(rl+1); memcpy(rl2, p, rl); rl2[rl]=0;
                const char* rt = rl2;
                while (*rt == ' ') rt++;
                if (rt[0] != '|') { free(rl2); break; }
                gmb_list_t row = gmb_list_new();
                char cbuf[512]; int ci = 0;
                for (const char* q = rt + 1; *q; q++)
                {
                    if (*q == '|') { cbuf[ci] = 0; ci = 0; gmb_strbuf_t sb = gmb_strbuf_new(); inline_to_text(md_trim(cbuf), &sb); gmb_list_add(&row, md_strv(sb.data)); gmb_strbuf_free(&sb); }
                    else if (ci < 511) cbuf[ci++] = *q;
                }
                /* Ultima cella: la riga può NON finire con '|' (Markdown lo consente). */
                if (ci > 0)
                {
                    cbuf[ci] = 0;
                    gmb_strbuf_t sb = gmb_strbuf_new(); inline_to_text(md_trim(cbuf), &sb);
                    gmb_list_add(&row, md_strv(sb.data)); gmb_strbuf_free(&sb);
                }
                gmb_list_add(&rows, gmb_make_val_list_take(&row));
                free(rl2);
                p = rnl ? rnl + 1 : p + rl;
            }
            gmb_val_dict_set(b, "rows", gmb_make_val_list_take(&rows));
            gmb_list_add(&blocks, b);
            free(line);
            continue;
        }
        if (is_list_line(t) || is_num_list(t))
        {
            gmb_val_t b = blk("list");
            gmb_val_dict_set(b, "ordered", gmb_make_val_double(is_num_list(t) ? 1.0 : 0.0));
            gmb_list_t items = gmb_list_new();
            while (t && *t)
            {
                const char* item = t;
                if (is_list_line(t)) item = t + 2;
                else if (is_num_list(t)) { const char* sp = strchr(t, ' '); item = sp ? sp + 1 : t; }
                gmb_strbuf_t sb = gmb_strbuf_new();
                inline_to_text(item, &sb);
                gmb_list_add(&items, md_strv(sb.data));
                gmb_strbuf_free(&sb);
                free(line); line = NULL;
                p = nl ? nl + 1 : p + len;
                if (!*p) break;
                nl = strchr(p, '\n');
                len = nl ? (size_t)(nl - p) : strlen(p);
                line = (char*)malloc(len+1); memcpy(line, p, len); line[len]=0;
                t = line;
                while (*t == ' ') t++;
                if (!is_list_line(t) && !is_num_list(t)) { free(line); line = NULL; break; }
            }
            if (line) { free(line); line = NULL; }
            gmb_val_dict_set(b, "items", gmb_make_val_list_take(&items));
            gmb_list_add(&blocks, b);
            continue;
        }
        if (t[0] == '>')
        {
            gmb_val_t b = blk("quote");
            gmb_strbuf_t sb = gmb_strbuf_new();
            inline_to_text(t + 1, &sb);
            gmb_val_dict_set(b, "text", md_strv(sb.data));
            gmb_strbuf_free(&sb);
            gmb_list_add(&blocks, b);
            free(line);
            p = nl ? nl + 1 : p + len;
            continue;
        }
        if (strcmp(t, "---") == 0 || strcmp(t, "***") == 0)
        {
            gmb_list_add(&blocks, blk("hr"));
            free(line);
            p = nl ? nl + 1 : p + len;
            continue;
        }
        if (*t == 0) { free(line); p = nl ? nl + 1 : p + len; continue; }
        {
            gmb_val_t b = blk("paragraph");
            gmb_strbuf_t sb = gmb_strbuf_new();
            gmb_list_t links = gmb_list_new();
            while (t && *t && !is_list_line(t) && !is_num_list(t) && t[0] != '#' && strncmp(t, "```", 3) != 0 && t[0] != '|' && t[0] != '>' && strcmp(t, "---") != 0)
            {
                raw_links(t, &links);
                if (sb.length > 0) gmb_strbuf_append(&sb, " ", 1);
                inline_to_text(t, &sb);
                free(line); line = NULL;
                /* Avanza SOLO se la riga successiva CONTINUA il paragrafo. Se è un
                 * terminatore (table/codeblock/lista/heading/quote/hr/EOF) o una riga
                 * vuota, p resta puntato al SUO inizio così l'outer loop la ri-processa
                 * col blocco giusto (prima il terminatore veniva perso). */
                const char* ns = nl ? nl + 1 : p + len;
                const char* nnl = ns ? strchr(ns, '\n') : NULL;
                size_t nlen = nnl ? (size_t)(nnl - ns) : (ns ? strlen(ns) : 0);
                char* nline = (char*)malloc(nlen + 1);
                if (!nline) break;
                if (ns) memcpy(nline, ns, nlen);
                nline[nlen] = 0;
                const char* nt = nline;
                while (*nt == ' ') nt++;
                if (!*nt || is_list_line(nt) || is_num_list(nt) || nt[0] == '#'
                    || strncmp(nt, "```", 3) == 0 || nt[0] == '|' || nt[0] == '>'
                    || strcmp(nt, "---") == 0)
                {
                    free(nline);
                    p = ns; nl = nnl; len = nlen;
                    break;
                }
                p = ns; nl = nnl; len = nlen;
                line = nline;
                t = line;
                while (*t == ' ') t++;
            }
            if (line) { free(line); line = NULL; }
            gmb_val_dict_set(b, "text", md_strv(sb.data));
            gmb_strbuf_free(&sb);
            gmb_val_dict_set(b, "links", gmb_make_val_list_take(&links));
            gmb_list_add(&blocks, b);
            continue;
        }
    }
    gmb_val_dict_set(doc, "blocks", gmb_make_val_list_take(&blocks));
    return doc;
}

/* ── helper: itera i blocchi ── */
static gmb_list_t* md_blocks(gmb_val_t doc)
{
    gmb_val_t bl = md_get(doc, "blocks");
    return md_list_of(bl);
}
static const char* blk_type(gmb_val_t b)
{
    gmb_dict_t* d = md_dict_of(b);
    return d ? md_str(gmb_dict_get(*d, "type")) : "";
}
static gmb_val_t blk_field(gmb_val_t b, const char* key)
{
    gmb_dict_t* d = md_dict_of(b);
    return d ? gmb_dict_get(*d, key) : gmb_val_nil();
}

static gmb_val_t block_list_of_type(gmb_val_t doc, const char* type)
{
    gmb_list_t out = gmb_list_new();
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
            if (strcmp(blk_type(bl->items[i]), type) == 0)
                gmb_list_add(&out, bl->items[i]);
    return gmb_make_val_list_take(&out);
}

gmb_val_t gmb_md_headings(gmb_val_t doc, int level)
{
    gmb_list_t out = gmb_list_new();
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
        {
            gmb_val_t b = bl->items[i];
            if (strcmp(blk_type(b), "heading") == 0)
            {
                double lv = md_num(blk_field(b, "level"));
                if (level <= 0 || (int)lv == level)
                {
                    gmb_val_t h = gmb_make_val_dict(gmb_dict_new());
                    gmb_val_dict_set(h, "level", gmb_make_val_double(lv));
                    gmb_val_dict_set(h, "text", blk_field(b, "text"));
                    gmb_list_add(&out, h);
                }
            }
        }
    return gmb_make_val_list_take(&out);
}

gmb_val_t gmb_md_links(gmb_val_t doc)
{
    gmb_list_t out = gmb_list_new();
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
        {
            gmb_val_t b = bl->items[i];
            gmb_val_t links = blk_field(b, "links");
            gmb_list_t* ll = md_list_of(links);
            if (ll)
                for (size_t j = 0; j < ll->count; j++) gmb_list_add(&out, ll->items[j]);
        }
    return gmb_make_val_list_take(&out);
}

gmb_val_t gmb_md_images(gmb_val_t doc) { return block_list_of_type(doc, "image"); }
gmb_val_t gmb_md_tables(gmb_val_t doc) { return block_list_of_type(doc, "table"); }
gmb_val_t gmb_md_codeblocks(gmb_val_t doc) { return block_list_of_type(doc, "codeblock"); }

gmb_val_t gmb_md_query(gmb_val_t doc, const char* selector)
{
    const char* type = selector;
    int level = 0;
    const char* lb = strchr(selector, '[');
    char tbuf[64]; size_t tl = 0;
    if (lb) { tl = (size_t)(lb - selector); if (tl > 63) tl = 63; memcpy(tbuf, selector, tl); tbuf[tl] = 0; type = tbuf;
             const char* lv = strstr(lb, "level");
             if (lv) level = atoi(strchr(lv, '=') + 1); }
    gmb_list_t out = gmb_list_new();
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
        {
            gmb_val_t b = bl->items[i];
            if (strcmp(blk_type(b), type) != 0) continue;
            if (strcmp(type, "heading") == 0 && level > 0 && (int)md_num(blk_field(b, "level")) != level) continue;
            gmb_list_add(&out, b);
        }
    return gmb_make_val_list_take(&out);
}

static void sb_append_escaped(gmb_strbuf_t* sb, const char* s)
{
    for (const char* q = s; *q; q++)
    {
        if (*q == '&') gmb_strbuf_append(sb, "&amp;", 5);
        else if (*q == '<') gmb_strbuf_append(sb, "&lt;", 4);
        else if (*q == '>') gmb_strbuf_append(sb, "&gt;", 4);
        else if (*q == '"') gmb_strbuf_append(sb, "&quot;", 6);
        else gmb_strbuf_append(sb, q, 1);
    }
}

char* gmb_md_to_html(gmb_val_t doc)
{
    gmb_strbuf_t sb = gmb_strbuf_new();
    gmb_strbuf_append(&sb, "<html><body>\n", 13);
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
        {
            gmb_val_t b = bl->items[i];
            const char* type = blk_type(b);
            if (strcmp(type, "heading") == 0)
            {
                int lv = (int)md_num(blk_field(b, "level"));
                if (lv < 1) lv = 1; if (lv > 6) lv = 6;
                char tag[16]; snprintf(tag, sizeof(tag), "<h%d>", lv);
                gmb_strbuf_append(&sb, tag, strlen(tag));
                sb_append_escaped(&sb, md_str(blk_field(b, "text")));
                snprintf(tag, sizeof(tag), "</h%d>\n", lv);
                gmb_strbuf_append(&sb, tag, strlen(tag));
            }
            else if (strcmp(type, "paragraph") == 0)
            {
                gmb_strbuf_append(&sb, "<p>", 3);
                sb_append_escaped(&sb, md_str(blk_field(b, "text")));
                gmb_strbuf_append(&sb, "</p>\n", 5);
            }
            else if (strcmp(type, "codeblock") == 0)
            {
                const char* lang = md_str(blk_field(b, "language"));
                gmb_strbuf_append(&sb, "<pre><code", 10);
                if (lang && *lang)
                {
                    gmb_strbuf_append(&sb, " class=\"language-", 17);
                    gmb_strbuf_append(&sb, lang, strlen(lang));
                    gmb_strbuf_append(&sb, "\"", 1);
                }
                gmb_strbuf_append(&sb, ">", 1);
                sb_append_escaped(&sb, md_str(blk_field(b, "text")));
                gmb_strbuf_append(&sb, "</code></pre>\n", 14);
            }
            else if (strcmp(type, "table") == 0)
            {
                gmb_strbuf_append(&sb, "<table>\n", 8);
                gmb_list_t* hdr = md_list_of(blk_field(b, "header"));
                if (hdr)
                {
                    gmb_strbuf_append(&sb, "<tr>", 4);
                    for (size_t j = 0; j < hdr->count; j++)
                    {
                        gmb_strbuf_append(&sb, "<th>", 4);
                        sb_append_escaped(&sb, md_str(hdr->items[j]));
                        gmb_strbuf_append(&sb, "</th>", 5);
                    }
                    gmb_strbuf_append(&sb, "</tr>\n", 6);
                }
                gmb_list_t* rows = md_list_of(blk_field(b, "rows"));
                if (rows)
                    for (size_t j = 0; j < rows->count; j++)
                    {
                        gmb_strbuf_append(&sb, "<tr>", 4);
                        gmb_list_t* cells = md_list_of(rows->items[j]);
                        if (cells)
                            for (size_t k = 0; k < cells->count; k++)
                            {
                                gmb_strbuf_append(&sb, "<td>", 4);
                                sb_append_escaped(&sb, md_str(cells->items[k]));
                                gmb_strbuf_append(&sb, "</td>", 5);
                            }
                        gmb_strbuf_append(&sb, "</tr>\n", 6);
                    }
                gmb_strbuf_append(&sb, "</table>\n", 9);
            }
            else if (strcmp(type, "list") == 0)
            {
                gmb_bool_t ordered = md_num(blk_field(b, "ordered")) != 0;
                gmb_strbuf_append(&sb, ordered ? "<ol>\n" : "<ul>\n", ordered ? 5 : 5);
                gmb_list_t* items = md_list_of(blk_field(b, "items"));
                if (items)
                    for (size_t j = 0; j < items->count; j++)
                    {
                        gmb_strbuf_append(&sb, "<li>", 4);
                        sb_append_escaped(&sb, md_str(items->items[j]));
                        gmb_strbuf_append(&sb, "</li>\n", 6);
                    }
                gmb_strbuf_append(&sb, ordered ? "</ol>\n" : "</ul>\n", ordered ? 6 : 6);
            }
            else if (strcmp(type, "quote") == 0)
            {
                gmb_strbuf_append(&sb, "<blockquote>", 12);
                sb_append_escaped(&sb, md_str(blk_field(b, "text")));
                gmb_strbuf_append(&sb, "</blockquote>\n", 14);
            }
            else if (strcmp(type, "hr") == 0) gmb_strbuf_append(&sb, "<hr/>\n", 6);
        }
    gmb_strbuf_append(&sb, "</body></html>", 14);
    gmb_string_t rs = gmb_strbuf_to_string(&sb);
    gmb_strbuf_free(&sb);
    return rs.data ? rs.data : strdup("");
}

char* gmb_md_to_text(gmb_val_t doc)
{
    gmb_strbuf_t sb = gmb_strbuf_new();
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
        {
            gmb_val_t b = bl->items[i];
            const char* type = blk_type(b);
            if (strcmp(type, "paragraph") == 0 || strcmp(type, "heading") == 0 || strcmp(type, "quote") == 0)
            { gmb_strbuf_append(&sb, md_str(blk_field(b, "text")), strlen(md_str(blk_field(b, "text")))); gmb_strbuf_append(&sb, "\n", 1); }
            else if (strcmp(type, "codeblock") == 0)
            { gmb_strbuf_append(&sb, md_str(blk_field(b, "text")), strlen(md_str(blk_field(b, "text")))); gmb_strbuf_append(&sb, "\n", 1); }
            else if (strcmp(type, "list") == 0)
            {
                gmb_list_t* items = md_list_of(blk_field(b, "items"));
                if (items)
                    for (size_t j = 0; j < items->count; j++)
                    { gmb_strbuf_append(&sb, "- ", 2); gmb_strbuf_append(&sb, md_str(items->items[j]), strlen(md_str(items->items[j]))); gmb_strbuf_append(&sb, "\n", 1); }
            }
            else if (strcmp(type, "table") == 0)
            {
                gmb_list_t* rows = md_list_of(blk_field(b, "rows"));
                if (rows)
                    for (size_t j = 0; j < rows->count; j++)
                    {
                        gmb_list_t* cells = md_list_of(rows->items[j]);
                        if (cells)
                            for (size_t k = 0; k < cells->count; k++)
                            {
                                if (k > 0) gmb_strbuf_append(&sb, " | ", 3);
                                gmb_strbuf_append(&sb, md_str(cells->items[k]), strlen(md_str(cells->items[k])));
                            }
                        gmb_strbuf_append(&sb, "\n", 1);
                    }
            }
        }
    gmb_string_t rs = gmb_strbuf_to_string(&sb);
    gmb_strbuf_free(&sb);
    return rs.data ? rs.data : strdup("");
}

char* gmb_md_tree(gmb_val_t doc)
{
    gmb_strbuf_t sb = gmb_strbuf_new();
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
        {
            gmb_val_t b = bl->items[i];
            const char* type = blk_type(b);
            char line[256];
            if (strcmp(type, "heading") == 0)
                snprintf(line, sizeof(line), "#%zu heading level=%g\n", i, md_num(blk_field(b, "level")));
            else if (strcmp(type, "paragraph") == 0)
                snprintf(line, sizeof(line), "#%zu paragraph text=\"%s\"\n", i, md_str(blk_field(b, "text")));
            else if (strcmp(type, "codeblock") == 0)
                snprintf(line, sizeof(line), "#%zu codeblock lang=%s len=%zu\n", i,
                         md_str(blk_field(b, "language")), strlen(md_str(blk_field(b, "text"))));
            else if (strcmp(type, "list") == 0)
            {
                gmb_list_t* items = md_list_of(blk_field(b, "items"));
                snprintf(line, sizeof(line), "#%zu list items=%zu\n", i, items ? items->count : 0);
            }
            else if (strcmp(type, "table") == 0)
            {
                gmb_list_t* rows = md_list_of(blk_field(b, "rows"));
                snprintf(line, sizeof(line), "#%zu table rows=%zu\n", i, rows ? rows->count : 0);
            }
            else
                snprintf(line, sizeof(line), "#%zu %s\n", i, type);
            gmb_strbuf_append(&sb, line, strlen(line));
        }
    gmb_string_t rs = gmb_strbuf_to_string(&sb);
    gmb_strbuf_free(&sb);
    return rs.data ? rs.data : strdup("");
}

char* gmb_md_validate(const char* text)
{
    const char* p = text ? text : "";
    int fences = 0;
    while ((p = strstr(p, "```")) != NULL) { fences++; p += 3; }
    return (fences % 2 != 0) ? strdup("fence non chiusa: triple backtick dispari") : strdup("");
}

char* gmb_md_strip(const char* text) { return gmb_md_to_text(gmb_md_parse(text)); }

char* gmb_md_format(gmb_val_t doc)
{
    gmb_strbuf_t sb = gmb_strbuf_new();
    gmb_list_t* bl = md_blocks(doc);
    if (bl)
        for (size_t i = 0; i < bl->count; i++)
        {
            gmb_val_t b = bl->items[i];
            const char* type = blk_type(b);
            if (strcmp(type, "heading") == 0)
            {
                int lv = (int)md_num(blk_field(b, "level"));
                char hdr[16]; int hd = 0; for (int k = 0; k < lv && hd < 14; k++) hdr[hd++] = '#';
                hdr[hd++] = ' '; hdr[hd] = 0;
                gmb_strbuf_append(&sb, hdr, strlen(hdr));
                gmb_strbuf_append(&sb, md_str(blk_field(b, "text")), strlen(md_str(blk_field(b, "text"))));
                gmb_strbuf_append(&sb, "\n\n", 2);
            }
            else if (strcmp(type, "paragraph") == 0)
            { gmb_strbuf_append(&sb, md_str(blk_field(b, "text")), strlen(md_str(blk_field(b, "text")))); gmb_strbuf_append(&sb, "\n\n", 2); }
            else if (strcmp(type, "codeblock") == 0)
            {
                gmb_strbuf_append(&sb, "```", 3);
                gmb_strbuf_append(&sb, md_str(blk_field(b, "language")), strlen(md_str(blk_field(b, "language"))));
                gmb_strbuf_append(&sb, "\n", 1);
                gmb_strbuf_append(&sb, md_str(blk_field(b, "text")), strlen(md_str(blk_field(b, "text"))));
                gmb_strbuf_append(&sb, "\n```\n\n", 6);
            }
            else if (strcmp(type, "list") == 0)
            {
                gmb_list_t* items = md_list_of(blk_field(b, "items"));
                if (items)
                    for (size_t j = 0; j < items->count; j++)
                    { gmb_strbuf_append(&sb, "- ", 2); gmb_strbuf_append(&sb, md_str(items->items[j]), strlen(md_str(items->items[j]))); gmb_strbuf_append(&sb, "\n", 1); }
                gmb_strbuf_append(&sb, "\n", 1);
            }
            else if (strcmp(type, "table") == 0)
            {
                gmb_list_t* hdr = md_list_of(blk_field(b, "header"));
                if (hdr)
                {
                    gmb_strbuf_append(&sb, "| ", 2);
                    for (size_t j = 0; j < hdr->count; j++)
                    { if (j) gmb_strbuf_append(&sb, " | ", 3); gmb_strbuf_append(&sb, md_str(hdr->items[j]), strlen(md_str(hdr->items[j]))); }
                    gmb_strbuf_append(&sb, " |\n", 3);
                    gmb_strbuf_append(&sb, "| ", 2);
                    for (size_t j = 0; j < hdr->count; j++)
                    { if (j) gmb_strbuf_append(&sb, " | ", 3); gmb_strbuf_append(&sb, "---", 3); }
                    gmb_strbuf_append(&sb, " |\n", 3);
                }
                gmb_list_t* rows = md_list_of(blk_field(b, "rows"));
                if (rows)
                    for (size_t j = 0; j < rows->count; j++)
                    {
                        gmb_list_t* cells = md_list_of(rows->items[j]);
                        if (cells)
                        {
                            gmb_strbuf_append(&sb, "| ", 2);
                            for (size_t k = 0; k < cells->count; k++)
                            { if (k) gmb_strbuf_append(&sb, " | ", 3); gmb_strbuf_append(&sb, md_str(cells->items[k]), strlen(md_str(cells->items[k]))); }
                            gmb_strbuf_append(&sb, " |\n", 3);
                        }
                    }
                gmb_strbuf_append(&sb, "\n", 1);
            }
        }
    gmb_string_t rs = gmb_strbuf_to_string(&sb);
    gmb_strbuf_free(&sb);
    return rs.data ? rs.data : strdup("");
}

/* ── iterazione dei blocchi (per i VIEW: markdown impaginato riga per riga) ──
 * L'AST resta l'unica fonte: queste funzioni espongono solo lettura, così un
 * renderer (gmb_textview) non duplica il parsing. Fuori range → ""/0. */
static gmb_val_t md_block_at(gmb_val_t doc, int index)
{
    gmb_list_t* bl = md_blocks(doc);
    if (!bl || index < 0 || (size_t)index >= bl->count) return gmb_val_nil();
    return bl->items[index];
}

int gmb_md_block_count(gmb_val_t doc)
{
    gmb_list_t* bl = md_blocks(doc);
    return bl ? (int)bl->count : 0;
}

const char* gmb_md_block_type(gmb_val_t doc, int index)
{
    gmb_val_t b = md_block_at(doc, index);
    return b.kind == GMB_TYPE_NIL ? "" : blk_type(b);
}

const char* gmb_md_block_text(gmb_val_t doc, int index)
{
    gmb_val_t b = md_block_at(doc, index);
    return b.kind == GMB_TYPE_NIL ? "" : md_str(blk_field(b, "text"));
}

int gmb_md_block_level(gmb_val_t doc, int index)
{
    gmb_val_t b = md_block_at(doc, index);
    return b.kind == GMB_TYPE_NIL ? 0 : (int)md_num(blk_field(b, "level"));
}

int gmb_md_block_ordered(gmb_val_t doc, int index)
{
    gmb_val_t b = md_block_at(doc, index);
    return b.kind == GMB_TYPE_NIL ? 0 : (int)md_num(blk_field(b, "ordered"));
}

int gmb_md_block_item_count(gmb_val_t doc, int index)
{
    gmb_val_t b = md_block_at(doc, index);
    if (b.kind == GMB_TYPE_NIL) return 0;
    gmb_list_t* items = md_list_of(blk_field(b, "items"));
    return items ? (int)items->count : 0;
}

const char* gmb_md_block_item(gmb_val_t doc, int index, int item)
{
    gmb_val_t b = md_block_at(doc, index);
    if (b.kind == GMB_TYPE_NIL) return "";
    gmb_list_t* items = md_list_of(blk_field(b, "items"));
    if (!items || item < 0 || (size_t)item >= items->count) return "";
    return md_str(items->items[item]);
}

int gmb_md_block_cols(gmb_val_t doc, int index)
{
    gmb_val_t b = md_block_at(doc, index);
    if (b.kind == GMB_TYPE_NIL) return 0;
    gmb_list_t* hdr = md_list_of(blk_field(b, "header"));
    return hdr ? (int)hdr->count : 0;
}

int gmb_md_block_row_count(gmb_val_t doc, int index)
{
    gmb_val_t b = md_block_at(doc, index);
    if (b.kind == GMB_TYPE_NIL) return 0;
    gmb_list_t* rows = md_list_of(blk_field(b, "rows"));
    return rows ? (int)rows->count : 0;
}

/* row < 0 = riga di intestazione; altrimenti riga `row` del corpo. */
const char* gmb_md_block_cell(gmb_val_t doc, int index, int row, int col)
{
    gmb_val_t b = md_block_at(doc, index);
    if (b.kind == GMB_TYPE_NIL || col < 0) return "";
    if (row < 0)
    {
        gmb_list_t* hdr = md_list_of(blk_field(b, "header"));
        if (!hdr || (size_t)col >= hdr->count) return "";
        return md_str(hdr->items[col]);
    }
    gmb_list_t* rows = md_list_of(blk_field(b, "rows"));
    if (!rows || (size_t)row >= rows->count) return "";
    gmb_list_t* cells = md_list_of(rows->items[row]);
    if (!cells || (size_t)col >= cells->count) return "";
    return md_str(cells->items[col]);
}
