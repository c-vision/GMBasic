/* gmb_doc.c — Conversione documentale in C (parity ConversionLib).
 * Dipende da gmb_runtime_c: gmb_zip_extract (compress), gmb_net_http_get (network),
 * gmb_file (lettura file), gmb_string (helper). */
#include "gmb_doc.h"
#include "gmb_doc_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "gmb_runtime.h"
#include "gmb_compress.h"
#include "gmb_network.h"
#include <zlib.h>

/* ── lettura file ── */
static char* gmb_doc_read_file(const char* path, size_t* outlen)
{
    FILE* f = fopen(path, "rb");
    long sz; char* data;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    data = (char*)malloc((size_t)sz + 1);
    if (!data) { fclose(f); return NULL; }
    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    data[rd] = 0;
    if (outlen) *outlen = rd;
    return data;
}

static char* gmb_doc_read_buffer(const unsigned char* data, size_t len)
{
    char* r = (char*)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, data, len); r[len] = 0;
    return r;
}

/* ── strip HTML: estrae testo (optionally headings -> markdown) ── */
static void gmb_doc_html_to_text(gmb_buf* out, const char* html)
{
    int in_tag = 0;
    for (const char* p = html; *p; p++) {
        if (*p == '<') { in_tag = 1; continue; }
        if (in_tag) {
            if (*p == '>') in_tag = 0;
            continue;
        }
        if (*p == '&') {
            if (strncmp(p, "&amp;", 5) == 0) { buf_app(out, "&"); p += 4; }
            else if (strncmp(p, "&lt;", 4) == 0) { buf_app(out, "<"); p += 3; }
            else if (strncmp(p, "&gt;", 4) == 0) { buf_app(out, ">"); p += 3; }
            else if (strncmp(p, "&quot;", 6) == 0) { buf_app(out, "\""); p += 5; }
            else if (strncmp(p, "&#39;", 5) == 0) { buf_app(out, "'"); p += 4; }
            else if (strncmp(p, "&nbsp;", 6) == 0) { buf_ch(out, ' '); p += 5; }
            else buf_ch(out, '&');
            continue;
        }
        buf_ch(out, *p);
    }
}

/* ── strip RTF: rimuove i control words, mantiene il testo ── */
static void gmb_doc_rtf_to_text(gmb_buf* out, const char* rtf)
{
    for (const char* p = rtf; *p; p++) {
        if (*p == '\\') {
            p++;
            if (*p == '\\' || *p == '{' || *p == '}') { buf_ch(out, *p); continue; }
            while (*p && !isspace((unsigned char)*p) && *p != ';') p++;
            if (*p == ';') continue;
            continue;
        }
        if (*p == '{' || *p == '}') continue;
        buf_ch(out, *p);
    }
}

/* ── estrazione testo da XML (tag noti): <w:t>, <t>, testo tra > e < ── */
static void gmb_doc_xml_text(gmb_buf* out, const char* xml, const char* tag)
{
    size_t tl = strlen(tag);
    const char* p = xml;
    (void)tag;
    while ((p = strstr(p, "<")) != NULL) {
        const char* gt = strchr(p, '>');
        if (!gt) break;
        if (gt - p >= (long)tl + 1 && strncasecmp(p + 1, tag, tl) == 0 && (p[1+tl] == '>' || isspace((unsigned char)p[1+tl]))) {
            const char* t = gt + 1;
            const char* et = strstr(t, "</");
            if (!et) break;
            buf_appn(out, t, (size_t)(et - t));
            p = et;
        } else {
            p = gt;
        }
    }
}

/* ── CSV → markdown table ── */
static void gmb_doc_csv_to_md(gmb_buf* out, const char* csv)
{
    gmb_buf row; buf_init(&row, 256);
    const char* p = csv;
    int first = 1;
    /* righe: separa per \n; celle per , */
    gmb_buf cells[1]; (void)cells;
    char cell[512]; int ci = 0;
    while (*p) {
        if (*p == '\n' || *p == '\r') {
            if (*p == '\r' && p[1] == '\n') p++;
            if (ci > 0) { cell[ci] = 0; if (!first) buf_app(out, " | "); buf_app(out, cell); buf_app(out, " |"); ci = 0; }
            if (ci == 0 && first) { /* intestazione */ }
            if (first) { buf_app(out, "\n|---|---|"); first = 0; }
            else buf_app(out, "\n");
            p++;
            continue;
        }
        if (*p == ',') {
            cell[ci] = 0; ci = 0;
            buf_app(out, "| ");
            buf_app(out, cell);
            buf_ch(out, ' ');
            p++;
            continue;
        }
        if (ci < (int)sizeof(cell) - 1) cell[ci++] = *p;
        p++;
    }
    if (ci > 0) { cell[ci] = 0; buf_app(out, "| "); buf_app(out, cell); buf_app(out, " |"); }
    (void)row;
}

/* ── lettore entry ZIP diretto (no estrazione su disco, crea le sottodir).
 * Legge l'entry dal central directory e la decomprime (STORED/DEFLATE via zlib). ── */
static char* gmb_doc_zip_read_entry(const char* archive, const char* entry, size_t* outlen)
{
    FILE* z = fopen(archive, "rb");
    unsigned char eocd[22];
    if (!z) return NULL;
    fseek(z, -22, SEEK_END);
    if (fread(eocd, 1, 22, z) != 22) { fclose(z); return NULL; }
    unsigned long cd_off = (unsigned long)eocd[16] | ((unsigned long)eocd[17]<<8) | ((unsigned long)eocd[18]<<16) | ((unsigned long)eocd[19]<<24);
    unsigned short nents = (unsigned short)(eocd[10] | (eocd[11]<<8));
    fseek(z, cd_off, SEEK_SET);
    for (unsigned short e = 0; e < nents; e++) {
        unsigned char cd[46];
        if (fread(cd, 1, 46, z) != 46) break;
        unsigned short nlen = (unsigned short)(cd[28] | (cd[29]<<8));
        unsigned long lfh_off = (unsigned long)cd[42] | ((unsigned long)cd[43]<<8) | ((unsigned long)cd[44]<<16) | ((unsigned long)cd[45]<<24);
        unsigned long csize = (unsigned long)cd[20] | ((unsigned long)cd[21]<<8) | ((unsigned long)cd[22]<<16) | ((unsigned long)cd[23]<<24);
        unsigned long usize  = (unsigned long)cd[24] | ((unsigned long)cd[25]<<8) | ((unsigned long)cd[26]<<16) | ((unsigned long)cd[27]<<24);
        unsigned short meth  = (unsigned short)(cd[10] | (cd[11]<<8));
        unsigned short elen = (unsigned short)(cd[30] | (cd[31]<<8));
        unsigned short clen = (unsigned short)(cd[32] | (cd[33]<<8));
        char name[512];
        if (nlen >= sizeof(name)) nlen = (unsigned short)(sizeof(name) - 1);
        fread(name, 1, nlen, z); name[nlen] = 0;
        /* salta extra field + comment del central directory: altrimenti l'entry
         * successiva viene letta in posizione sbagliata (zip reali con extra fields,
         * es. creati da Python/Excel, fallivano dopo la prima entry). */
        if (elen) fseek(z, elen, SEEK_CUR);
        if (clen) fseek(z, clen, SEEK_CUR);
        if (strcmp(name, entry) == 0) {
            /* va all'LFH: salta nome+extra, poi i dati */
            unsigned char lfh[30];
            fseek(z, lfh_off, SEEK_SET);
            if (fread(lfh, 1, 30, z) != 30) break;
            unsigned short enlen = (unsigned short)(lfh[26] | (lfh[27]<<8));
            unsigned short exlen = (unsigned short)(lfh[28] | (lfh[29]<<8));
            fseek(z, lfh_off + 30 + enlen + exlen, SEEK_SET);
            unsigned char* comp = (unsigned char*)malloc(csize ? csize : 1);
            if (!comp) break;
            size_t rd = fread(comp, 1, csize, z);
            fclose(z);
            if (meth == 0) { /* STORED */
                char* r = (char*)malloc(rd + 1);
                if (!r) { free(comp); return NULL; }
                memcpy(r, comp, rd); r[rd] = 0;
                if (outlen) *outlen = rd;
                free(comp);
                return r;
            }
            if (meth == 8) { /* DEFLATE */
                uLongf dl = usize ? usize : csize * 4 + 64;
                unsigned char* out = (unsigned char*)malloc(dl);
                if (!out) { free(comp); return NULL; }
                if (uncompress(out, &dl, comp, (uLong)rd) != Z_OK) {
                    /* retry con buffer più grande */
                    dl = csize * 8 + 1024;
                    unsigned char* out2 = (unsigned char*)realloc(out, dl);
                    if (!out2) { free(out); free(comp); return NULL; }
                    out = out2;
                    if (uncompress(out, &dl, comp, (uLong)rd) != Z_OK) { free(out); free(comp); return NULL; }
                }
                char* r = (char*)malloc(dl + 1);
                if (!r) { free(out); free(comp); return NULL; }
                memcpy(r, out, dl); r[dl] = 0;
                if (outlen) *outlen = dl;
                free(out); free(comp);
                return r;
            }
            free(comp);
            return NULL;
        }
    }
    fclose(z);
    return NULL;
}

static char* gmb_doc_zip_xml(const char* path, const char* inner, const char* tag)
{
    size_t len = 0;
    char* data = gmb_doc_zip_read_entry(path, inner, &len);
    if (!data) { /* fallback basename (zip piatti) */
        const char* base = strrchr(inner, '/');
        data = gmb_doc_zip_read_entry(path, base ? base + 1 : inner, &len);
    }
    gmb_buf out; buf_init(&out, 1024);
    if (data) { gmb_doc_xml_text(&out, data, tag); free(data); }
    return buf_take(&out);
}

/* ── XLSX → markdown tables: sharedStrings + worksheets (celle per riferimento) ── */
typedef struct { char** items; int n, cap; } gmb_strarr;
static void gmb_strarr_add(gmb_strarr* a, const char* s)
{
    if (a->n >= a->cap) { a->cap = a->cap ? a->cap * 2 : 16; a->items = (char**)realloc(a->items, sizeof(char*) * (size_t)a->cap); }
    a->items[a->n++] = strdup(s ? s : "");
}

/* estrae tutti i <si>...</si> → array di stringhe (concatena i <t> dentro ogni si) */
static void gmb_xlsx_shared(gmb_strarr* out, const char* xml)
{
    const char* p = xml;
    while ((p = strstr(p, "<si")) != NULL) {
        const char* siEnd = strstr(p, "</si>");
        if (!siEnd) break;
        gmb_buf cell; buf_init(&cell, 64);
        const char* q = p;
        while ((q = strstr(q, "<t")) != NULL && q < siEnd) {
            const char* gt = strchr(q, '>');
            const char* et = strstr(q, "</t>");
            if (!gt || !et || et > siEnd) break;
            buf_appn(&cell, gt + 1, (size_t)(et - gt - 1));
            q = et + 4;
        }
        gmb_strarr_add(out, cell.p);
        buf_free(&cell);
        p = siEnd + 5;
    }
}

/* estrae <v>..</v> dentro un cella */
static void gmb_xlsx_cell_value(gmb_buf* out, const char* c, const char* tattr)
{
    if (tattr && strncasecmp(tattr, "inlineStr", 9) == 0) {
        const char* it = strstr(c, "<t");
        const char* gt = it ? strchr(it, '>') : NULL;
        const char* et = it ? strstr(it, "</t>") : NULL;
        if (gt && et) buf_appn(out, gt + 1, (size_t)(et - gt - 1));
        return;
    }
    const char* v = strstr(c, "<v>");
    if (v) {
        const char* ev = strstr(v + 3, "</v>");
        if (ev) buf_appn(out, v + 3, (size_t)(ev - v - 3));
    }
}

/* colonna (A=1..Z, AA..) dal riferimento cella 'r' */
static int gmb_xlsx_col(const char* r)
{
    int col = 0;
    while (*r && ((*r >= 'A' && *r <= 'Z') || (*r >= 'a' && *r <= 'z'))) {
        col = col * 26 + (toupper((unsigned char)*r) - 'A' + 1);
        r++;
    }
    return col;
}

static void gmb_xlsx_sheet_to_md(gmb_buf* out, const char* xml, const gmb_strarr* shared, const char* sheetname)
{
    if (sheetname && sheetname[0]) { buf_app(out, "## "); buf_app(out, sheetname); buf_app(out, "\n\n"); }
    const char* p = xml;
    int firstRow = 1;
    while ((p = strstr(p, "<row")) != NULL) {
        const char* gt = strchr(p, '>');
        const char* rowEnd = strstr(p, "</row>");
        if (!gt || !rowEnd) break;
        gmb_buf row; buf_init(&row, 256);
        int prevCol = 0;
        const char* c = gt + 1;
        while (c < rowEnd && (c = strstr(c, "<c ")) != NULL && c < rowEnd) {
            const char* cgt = strchr(c, '>');
            if (!cgt) break;
            /* attributi r e t */
            char rattr[16] = "", tattr[16] = "";
            const char* rr = strstr(c, " r=\"");
            if (rr && rr < cgt) { const char* e = strchr(rr + 4, '"'); if (e) { size_t n = (size_t)(e - rr - 4); if (n < sizeof(rattr)) { memcpy(rattr, rr + 4, n); rattr[n] = 0; } } }
            const char* tt = strstr(c, " t=\"");
            if (tt && tt < cgt) { const char* e = strchr(tt + 4, '"'); if (e) { size_t n = (size_t)(e - tt - 4); if (n < sizeof(tattr)) { memcpy(tattr, tt + 4, n); tattr[n] = 0; } } }
            int col = rattr[0] ? gmb_xlsx_col(rattr) : prevCol + 1;
            while (prevCol + 1 < col) { buf_app(&row, " | "); prevCol++; }
            gmb_buf val; buf_init(&val, 32);
            if (strncmp(tattr, "s", 1) == 0) {
                gmb_buf v; buf_init(&v, 16);
                gmb_xlsx_cell_value(&v, c, NULL);
                int idx = atoi(v.p);
                buf_free(&v);
                if (idx >= 0 && idx < shared->n) buf_app(&val, shared->items[idx]);
            } else {
                gmb_xlsx_cell_value(&val, c, tattr);
            }
            buf_app(&row, " | "); buf_app(&row, val.p);
            buf_free(&val);
            prevCol = col;
            c = cgt + 1;
        }
        buf_app(out, "|"); buf_app(out, row.p); buf_app(out, " |\n");
        if (firstRow) { buf_app(out, "|---|---|\n"); firstRow = 0; }
        buf_free(&row);
        p = rowEnd + 6;
    }
    buf_app(out, "\n");
}

static void gmb_doc_xlsx_to_md(gmb_buf* out, const char* path)
{
    size_t len = 0;
    char* sharedXml = gmb_doc_zip_read_entry(path, "xl/sharedStrings.xml", &len);
    gmb_strarr shared = { 0 };
    if (sharedXml) { gmb_xlsx_shared(&shared, sharedXml); free(sharedXml); }
    for (int s = 1; s <= 16; s++) {
        char inner[128]; snprintf(inner, sizeof(inner), "xl/worksheets/sheet%d.xml", s);
        char* sheet = gmb_doc_zip_read_entry(path, inner, &len);
        if (!sheet) { snprintf(inner, sizeof(inner), "worksheets/sheet%d.xml", s); sheet = gmb_doc_zip_read_entry(path, inner, &len); }
        if (!sheet) break;
        char name[64]; snprintf(name, sizeof(name), "Sheet %d", s);
        gmb_xlsx_sheet_to_md(out, sheet, &shared, name);
        free(sheet);
    }
    for (int i = 0; i < shared.n; i++) free(shared.items[i]);
    free(shared.items);
}

/* ── PPTX → markdown: tutti i slide (slide1..N), testo <a:t> per slide ── */
static void gmb_doc_pptx_to_md(gmb_buf* out, const char* path)
{
    size_t len = 0;
    for (int s = 1; s <= 64; s++)
    {
        char inner[128]; snprintf(inner, sizeof(inner), "ppt/slides/slide%d.xml", s);
        char* slide = gmb_doc_zip_read_entry(path, inner, &len);
        if (!slide) { snprintf(inner, sizeof(inner), "slides/slide%d.xml", s); slide = gmb_doc_zip_read_entry(path, inner, &len); }
        if (!slide) break;
        char head[32]; snprintf(head, sizeof(head), "\n## Slide %d\n", s);
        buf_app(out, head);
        gmb_doc_xml_text(out, slide, "a:t");
        free(slide);
    }
}

/* ── EPUB → markdown: titolo + contenuto XHTML (OPF → manifest → spine) ── */
static void gmb_doc_epub_to_md(gmb_buf* out, const char* path)
{
    size_t len = 0;
    char* title = gmb_doc_zip_read_entry(path, "OEBPS/content.opf", &len);
    if (!title) title = gmb_doc_zip_read_entry(path, "META-INF/content.xml", &len);
    gmb_buf t; buf_init(&t, 128);
    if (title) { gmb_doc_xml_text(&t, title, "dc:title"); free(title); }
    if (t.p && t.p[0]) { buf_app(out, "# "); buf_app(out, t.p); buf_app(out, "\n\n"); }
    buf_free(&t);

    /* primi N file XHTML del contenuto (spine) — titolo del primo href */
    char href[512] = "";
    size_t hl = 0;
    char* opf = gmb_doc_zip_read_entry(path, "OEBPS/content.opf", &hl);
    if (!opf) opf = gmb_doc_zip_read_entry(path, "content.opf", &hl);
    if (opf)
    {
        const char* m = strstr(opf, "<spine");
        const char* itemref = m ? strstr(m, "idref=") : NULL;
        if (itemref) { const char* q1 = strchr(itemref, '"'); const char* q2 = q1 ? strchr(q1 + 1, '"') : NULL; if (q1 && q2) { char idref[64]; size_t il = (size_t)(q2 - q1 - 1); if (il < sizeof(idref)) { memcpy(idref, q1 + 1, il); idref[il] = 0; const char* it = strstr(opf, "id=\""); while (it && strncmp(it + 4, idref, il) != 0) it = strstr(it + 1, "id=\""); if (it) { const char* h = strstr(it, "href=\""); if (h) { const char* e = strchr(h + 6, '"'); if (e) { size_t n = (size_t)(e - h - 6); if (n < sizeof(href)) { memcpy(href, h + 6, n); href[n] = 0; } } } } } } }
        free(opf);
    }
    if (href[0])
    {
        char inner[512]; snprintf(inner, sizeof(inner), "OEBPS/%s", href);
        char* xhtml = gmb_doc_zip_read_entry(path, inner, &len);
        if (!xhtml) xhtml = gmb_doc_zip_read_entry(path, href, &len);
        if (xhtml) { gmb_doc_html_to_text(out, xhtml); free(xhtml); }
    }
}

/* ── convert core: input file → markdown ── */
static char* gmb_doc_to_md(const char* path)
{
    const char* ext = gmb_doc_ext(path);
    if (strcasecmp(ext, "pdf") == 0)
        return gmb_doc_pdf_convert(path, "md");
    size_t len = 0;
    char* data = gmb_doc_read_file(path, &len);
    if (!data) return NULL;
    gmb_buf out; buf_init(&out, 1024);
    char* result = NULL;

    if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "md") == 0) {
        buf_app(&out, data);
    } else if (strcasecmp(ext, "csv") == 0) {
        gmb_doc_csv_to_md(&out, data);
    } else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        gmb_doc_html_to_text(&out, data);
    } else if (strcasecmp(ext, "rtf") == 0) {
        gmb_doc_rtf_to_text(&out, data);
    } else if (strcasecmp(ext, "docx") == 0) {
        char* t = gmb_doc_zip_xml(path, "word/document.xml", "w:t");
        if (t) { buf_app(&out, t); free(t); }
    } else if (strcasecmp(ext, "odt") == 0) {
        char* t = gmb_doc_zip_xml(path, "content.xml", "text:p");
        if (t) { buf_app(&out, t); free(t); }
    } else if (strcasecmp(ext, "xlsx") == 0) {
        gmb_doc_xlsx_to_md(&out, path);
    } else if (strcasecmp(ext, "pptx") == 0) {
        gmb_doc_pptx_to_md(&out, path);
    } else if (strcasecmp(ext, "epub") == 0) {
        gmb_doc_epub_to_md(&out, path);
    } else {
        buf_app(&out, data); /* fallback: testo grezzo */
    }
    free(data);
    result = buf_take(&out);
    return result;
}

/* ── API pubbliche ── */
char* gmb_doc_convert(const char* path, const char* fmt)
{
    if (!path) return NULL;
    if (strcasecmp(gmb_doc_ext(path), "pdf") == 0)
        return gmb_doc_pdf_convert(path, fmt);
    char* md = gmb_doc_to_md(path);
    if (!md) return NULL;
    if (!fmt || strcasecmp(fmt, "md") == 0 || strcasecmp(fmt, "markdown") == 0) return md;
    if (strcasecmp(fmt, "text") == 0 || strcasecmp(fmt, "txt") == 0) return md;
    if (strcasecmp(fmt, "html") == 0) {
        gmb_buf b; buf_init(&b, 512);
        buf_app(&b, "<html><body>\n");
        buf_app(&b, md);
        buf_app(&b, "\n</body></html>");
        free(md);
        return buf_take(&b);
    }
    return md;
}

char* gmb_doc_convert_buffer(const unsigned char* data, size_t len, const char* ext, const char* fmt)
{
    if (!data || !ext) return NULL;
    gmb_buf out; buf_init(&out, 1024);
    const char* src = (const char*)data;
    /* i formati testuali parsano un buffer direttamente; i container ZIP (docx/odt/
     * epub/xlsx/pptx) leggono da file → non supportati da buffer (NULL). */
    if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "md") == 0 || strcasecmp(ext, "log") == 0) {
        buf_appn(&out, src, len);
    } else if (strcasecmp(ext, "csv") == 0) {
        char* s = (char*)malloc(len + 1);
        if (!s) { buf_free(&out); return NULL; }
        memcpy(s, src, len); s[len] = 0;
        gmb_doc_csv_to_md(&out, s);
        free(s);
    } else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        char* s = (char*)malloc(len + 1);
        if (!s) { buf_free(&out); return NULL; }
        memcpy(s, src, len); s[len] = 0;
        gmb_doc_html_to_text(&out, s);
        free(s);
    } else if (strcasecmp(ext, "rtf") == 0) {
        char* s = (char*)malloc(len + 1);
        if (!s) { buf_free(&out); return NULL; }
        memcpy(s, src, len); s[len] = 0;
        gmb_doc_rtf_to_text(&out, s);
        free(s);
    } else {
        buf_free(&out);
        return NULL; /* zip/pdf: richiedono file */
    }
    if (fmt && strcasecmp(fmt, "html") == 0) {
        gmb_buf b; buf_init(&b, 512);
        buf_app(&b, "<html><body>\n"); buf_app(&b, out.p); buf_app(&b, "\n</body></html>");
        buf_free(&out);
        return buf_take(&b);
    }
    return buf_take(&out);
}

char* gmb_doc_batch(const char* dir, const char* fmt, int recursive)
{
    DIR* d = opendir(dir);
    struct dirent* e;
    gmb_buf out; buf_init(&out, 2048);
    if (!d) return NULL;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (recursive && e->d_type == DT_DIR) {
            char sub[2048]; snprintf(sub, sizeof(sub), "%s/%s", dir, e->d_name);
            char* r = gmb_doc_batch(sub, fmt, recursive);
            if (r) { buf_app(&out, r); free(r); }
            continue;
        }
        if (e->d_type == DT_DIR) continue;
        char fp[2048]; snprintf(fp, sizeof(fp), "%s/%s", dir, e->d_name);
        buf_app(&out, "--- FILE: "); buf_app(&out, e->d_name); buf_app(&out, " ---\n");
        char* c = gmb_doc_convert(fp, fmt);
        if (c) { buf_app(&out, c); buf_app(&out, "\n\n"); free(c); }
    }
    closedir(d);
    return buf_take(&out);
}

char* gmb_doc_web(const char* url, const char* fmt)
{
    char* html = gmb_net_http_get(url);
    /* Fetch fallita: errore ESPLICITO e DETERMINISTICO, stesso marcatore del C#
     * DocumentConverter.ConvertUrl (il messaggio curl non è riproducibile su HttpClient).
     * Prima ritornava NULL -> stringa vuota (silent-wrong). */
    if (!html) return strdup("?Error: web fetch failed");
    gmb_buf out; buf_init(&out, 1024);
    gmb_doc_html_to_text(&out, html);
    free(html);
    if (fmt && strcasecmp(fmt, "html") == 0) {
        gmb_buf b; buf_init(&b, 512);
        buf_app(&b, "<html><body>\n"); buf_app(&b, out.p); buf_app(&b, "\n</body></html>");
        buf_free(&out);
        return buf_take(&b);
    }
    return buf_take(&out);
}

void gmb_doc_formats(char* out, int cap)
{
    if (out && cap > 0)
        snprintf(out, (size_t)cap, "txt,csv,md,html,rtf,docx,epub,odt,xlsx,pptx,pdf,url");
}

char* gmb_doc_formats_s(void)
{
    return strdup("txt,csv,md,html,rtf,docx,epub,odt,xlsx,pptx,pdf,url");
}

int gmb_doc_selfcheck(void)
{
    return 0;
}

/* ── DOCUMENT.EXTRACT: text-layer markdown di un documento; per i PDF fa il routing
 *    ibrido per pagina (text-layer via PDFOxide + OCR Apple Vision sulle pagine
 *    scansionate — parità DocExtract C#). Gli altri formati via gmb_doc_convert. ── */
char* gmb_doc_extract(const char* path)
{
    if (!path) return NULL;
    if (strcasecmp(gmb_doc_ext(path), "pdf") == 0)
    {
        void* h = gmb_doc_pdf_open(path);
        if (!h) return NULL;
        return gmb_doc_pdf_extract_hybrid(h);
    }
    return gmb_doc_convert(path, "md");
}

/* ── Comandi CONVERT legacy: EXTRACT TO / RENDER TO (scrivono su file) ── */
int gmb_doc_extract_to(const char* path, const char* dest)
{
    char* md = gmb_doc_extract(path);
    if (!md) return -1;
    FILE* f = dest ? fopen(dest, "w") : NULL;
    int rc = 0;
    if (f) { fputs(md, f); fclose(f); rc = 0; } else rc = -1;
    free(md);
    return rc;
}

int gmb_doc_render_to(const char* path, const char* dest)
{
    int h = gmb_doc_open(path);
    if (!h) return -1;
    int rc = gmb_doc_render_png(h, 0, 150, dest);
    gmb_doc_close(h);
    return rc;
}

/* CONVERT.EXTRACTENGINE: classifica il documento (PDFOXIDE/OCR/HYBRID) senza fare OCR. */
char* gmb_doc_extract_engine(const char* path)
{
    int h = gmb_doc_open(path);
    if (!h) return NULL;
    int pages = gmb_doc_page_count(h);
    if (pages < 1) pages = 1;
    int text = 0, ocr = 0;
    for (int p = 0; p < pages; p++)
    {
        char* md = gmb_doc_extract_markdown(h, p);
        int has = md && strlen(md) >= 20 && !strstr(md, "OCR REQUIRED");
        if (has) text++; else ocr++;
        free(md);
    }
    gmb_doc_close(h);
    return strdup(ocr == 0 ? "PDFOXIDE" : text == 0 ? "OCR" : "HYBRID");
}
