/* gmb_doc_pdf.c — Backend PDF via PDFOxide C ABI (dlsym, nessun link a build-time).
 * Carica libpdf_oxide.{dylib,so,dll} da Resources/libs/<os>/ e usa l'API C stabile
 * (pdf_document_open / to_markdown / to_plain_text / to_html / free_string).
 * Tutte le stringhe ritornate sono allocate con malloc (da liberare con free()). */
#include "gmb_doc.h"
#include "gmb_doc_internal.h"
#include "gmb_doc_ocr.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <zlib.h>

/* PDFOxide C ABI (cbindgen) — firme da native/deps/pdf_oxide/include/pdf_oxide_c/pdf_oxide.h. */
typedef struct PdfDocument PdfDocument;

typedef struct FfiSearchResults FfiSearchResults;
typedef struct FfiImageList FfiImageList;
typedef struct FfiRenderedImage FfiRenderedImage;

typedef PdfDocument* (*pdf_open_fn)(const char* path, int32_t* error_code);
typedef void (*pdf_free_fn)(PdfDocument* handle);
typedef int32_t (*pdf_page_count_fn)(PdfDocument* handle, int32_t* error_code);
typedef char* (*pdf_to_markdown_fn)(PdfDocument* handle, int32_t page_index, int32_t* error_code);
typedef char* (*pdf_to_markdown_all_fn)(PdfDocument* handle, int32_t* error_code);
typedef char* (*pdf_to_html_fn)(PdfDocument* handle, int32_t page_index, int32_t* error_code);
typedef char* (*pdf_to_plain_text_fn)(PdfDocument* handle, int32_t page_index, int32_t* error_code);
typedef void (*pdf_get_version_fn)(PdfDocument* handle, uint8_t* major, uint8_t* minor);
typedef void (*free_string_fn)(char* ptr);

/* search */
typedef FfiSearchResults* (*pdf_search_all_fn)(PdfDocument* handle, const char* term, int case_sensitive, int32_t* error_code);
typedef int32_t (*search_result_count_fn)(const FfiSearchResults* r);
typedef char* (*search_result_text_fn)(const FfiSearchResults* r, int32_t index, int32_t* error_code);
typedef int32_t (*search_result_page_fn)(const FfiSearchResults* r, int32_t index, int32_t* error_code);
typedef void (*search_result_free_fn)(FfiSearchResults* r);

/* images */
typedef FfiImageList* (*pdf_get_images_fn)(PdfDocument* handle, int32_t page_index, int32_t* error_code);
typedef int32_t (*image_count_fn)(const FfiImageList* im);
typedef int32_t (*image_dim_fn)(const FfiImageList* im, int32_t index, int32_t* error_code);
typedef char* (*image_format_fn)(const FfiImageList* im, int32_t index, int32_t* error_code);
typedef char* (*image_colorspace_fn)(const FfiImageList* im, int32_t index, int32_t* error_code);
typedef int32_t (*image_bpp_fn)(const FfiImageList* im, int32_t index, int32_t* error_code);
typedef uint8_t* (*image_data_fn)(const FfiImageList* im, int32_t index, int32_t* data_len, int32_t* error_code);
typedef void (*image_list_free_fn)(FfiImageList* im);

/* render */
typedef FfiRenderedImage* (*render_raw_fn)(PdfDocument* doc, int32_t page, int32_t dpi, int32_t* out_w, int32_t* out_h, int32_t* error_code);
typedef int32_t (*rendered_dim_fn)(const FfiRenderedImage* img, int32_t* error_code);
typedef uint8_t* (*rendered_data_fn)(const FfiRenderedImage* img, int32_t* data_len, int32_t* error_code);
typedef void (*rendered_free_fn)(FfiRenderedImage* img);

typedef struct {
    void* dl;
    pdf_open_fn            open;
    pdf_free_fn            free_doc;
    pdf_page_count_fn      page_count;
    pdf_to_markdown_fn     to_markdown;
    pdf_to_markdown_all_fn to_markdown_all;
    pdf_to_html_fn         to_html;
    pdf_to_plain_text_fn   to_plain_text;
    pdf_get_version_fn     get_version;
    free_string_fn         free_string;
    pdf_search_all_fn      search_all;
    search_result_count_fn search_count;
    search_result_text_fn  search_text;
    search_result_page_fn  search_page;
    search_result_free_fn  search_free;
    pdf_get_images_fn      get_images;
    image_count_fn         image_count;
    image_dim_fn           image_width;
    image_dim_fn           image_height;
    image_format_fn        image_format;
    image_colorspace_fn    image_colorspace;
    image_bpp_fn           image_bpp;
    image_data_fn          image_data;
    image_list_free_fn     image_list_free;
    render_raw_fn          render_raw;
    rendered_dim_fn        rendered_w;
    rendered_dim_fn        rendered_h;
    rendered_data_fn       rendered_data;
    rendered_free_fn       rendered_free;
} gmb_pdf_bindings;

static gmb_pdf_bindings g_pdf = { 0 };
static int g_pdf_tried = 0;

/* Cerca Resources/libs/<os>/libpdf_oxide.<ext> risalendo da cwd (fino a 4 livelli). */
static const char* gmb_pdf_libname(void)
{
#if defined(__APPLE__)
#if defined(__aarch64__)
    return "osx-arm64/libpdf_oxide.dylib";
#else
    return "osx-x64/libpdf_oxide.dylib";
#endif
#elif defined(_WIN32)
    return "win-x64/pdf_oxide.dll";
#else
#if defined(__aarch64__)
    return "linux-arm64/libpdf_oxide.so";
#else
    return "linux-x64/libpdf_oxide.so";
#endif
#endif
}

static void* gmb_pdf_open_lib(void)
{
    /* NESSUNA env (Regola Primaria). Scoperta automatica: accanto al binario e risalendo
     * i parent (Resources/libs/<rid>/...). */
    char cwd[2048];
    if (!getcwd(cwd, sizeof(cwd))) return NULL;
    char cand[4096];
    const char* name = gmb_pdf_libname();
    for (int up = 0; up < 4; up++)
    {
        if (up > 0)
        {
            char* slash = strrchr(cwd, '/');
            if (!slash) break;
            *slash = '\0';
        }
        snprintf(cand, sizeof(cand), "%s/Resources/libs/%s", cwd, name);
        void* h = dlopen(cand, RTLD_NOW | RTLD_LOCAL);
        if (h) return h;
    }
    return NULL;
}

static void* gmb_pdf_sym(gmb_pdf_bindings* b, const char* name)
{
    dlerror();
    return dlsym(b->dl, name);
}

static gmb_pdf_bindings* gmb_pdf_load(void)
{
    if (g_pdf_tried) return g_pdf.dl ? &g_pdf : NULL;
    g_pdf_tried = 1;
    g_pdf.dl = gmb_pdf_open_lib();
    if (!g_pdf.dl) return NULL;
    g_pdf.open            = (pdf_open_fn)gmb_pdf_sym(&g_pdf, "pdf_document_open");
    g_pdf.free_doc        = (pdf_free_fn)gmb_pdf_sym(&g_pdf, "pdf_document_free");
    g_pdf.page_count      = (pdf_page_count_fn)gmb_pdf_sym(&g_pdf, "pdf_document_get_page_count");
    g_pdf.to_markdown     = (pdf_to_markdown_fn)gmb_pdf_sym(&g_pdf, "pdf_document_to_markdown");
    g_pdf.to_markdown_all = (pdf_to_markdown_all_fn)gmb_pdf_sym(&g_pdf, "pdf_document_to_markdown_all");
    g_pdf.to_html         = (pdf_to_html_fn)gmb_pdf_sym(&g_pdf, "pdf_document_to_html");
    g_pdf.to_plain_text   = (pdf_to_plain_text_fn)gmb_pdf_sym(&g_pdf, "pdf_document_to_plain_text");
    g_pdf.get_version     = (pdf_get_version_fn)gmb_pdf_sym(&g_pdf, "pdf_document_get_version");
    g_pdf.free_string     = (free_string_fn)gmb_pdf_sym(&g_pdf, "free_string");
    g_pdf.search_all      = (pdf_search_all_fn)gmb_pdf_sym(&g_pdf, "pdf_document_search_all");
    g_pdf.search_count    = (search_result_count_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_search_result_count");
    g_pdf.search_text     = (search_result_text_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_search_result_get_text");
    g_pdf.search_page     = (search_result_page_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_search_result_get_page");
    g_pdf.search_free     = (search_result_free_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_search_result_free");
    g_pdf.get_images      = (pdf_get_images_fn)gmb_pdf_sym(&g_pdf, "pdf_document_get_embedded_images");
    g_pdf.image_count     = (image_count_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_count");
    g_pdf.image_width     = (image_dim_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_get_width");
    g_pdf.image_height    = (image_dim_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_get_height");
    g_pdf.image_format    = (image_format_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_get_format");
    g_pdf.image_colorspace= (image_colorspace_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_get_colorspace");
    g_pdf.image_bpp       = (image_bpp_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_get_bits_per_component");
    g_pdf.image_data      = (image_data_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_get_data");
    g_pdf.image_list_free = (image_list_free_fn)gmb_pdf_sym(&g_pdf, "pdf_oxide_image_list_free");
    g_pdf.render_raw      = (render_raw_fn)gmb_pdf_sym(&g_pdf, "pdf_render_page_raw");
    g_pdf.rendered_w      = (rendered_dim_fn)gmb_pdf_sym(&g_pdf, "pdf_get_rendered_image_width");
    g_pdf.rendered_h      = (rendered_dim_fn)gmb_pdf_sym(&g_pdf, "pdf_get_rendered_image_height");
    g_pdf.rendered_data   = (rendered_data_fn)gmb_pdf_sym(&g_pdf, "pdf_get_rendered_image_data");
    g_pdf.rendered_free   = (rendered_free_fn)gmb_pdf_sym(&g_pdf, "pdf_rendered_image_free");
    if (!g_pdf.open || !g_pdf.free_doc || !g_pdf.free_string)
    {
        dlclose(g_pdf.dl);
        g_pdf.dl = NULL;
        return NULL;
    }
    return &g_pdf;
}

/* Copia il risultato PDFOxide in una stringa malloc (libera l'originale via free_string). */
static char* gmb_pdf_take(char* pdfstr, gmb_pdf_bindings* b)
{
    if (!pdfstr) return NULL;
    char* copy = strdup(pdfstr);
    if (b->free_string) b->free_string(pdfstr);
    return copy;
}

/* ── API pubblica PDF (usate anche dal futuro namespace DOCUMENT/PDF) ── */
void* gmb_doc_pdf_open(const char* path)
{
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !path) return NULL;
    int32_t err = 0;
    return b->open(path, &err);
}

void gmb_doc_pdf_close(void* h)
{
    if (!h) return;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (b && b->free_doc) b->free_doc((PdfDocument*)h);
}

int gmb_doc_pdf_page_count(void* h)
{
    if (!h) return 0;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !b->page_count) return 0;
    int32_t err = 0;
    return (int)b->page_count((PdfDocument*)h, &err);
}

char* gmb_doc_pdf_extract_markdown(void* h, int page)
{
    if (!h) return NULL;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b) return NULL;
    int32_t err = 0;
    if (page < 0 && b->to_markdown_all)
        return gmb_pdf_take(b->to_markdown_all((PdfDocument*)h, &err), b);
    if (!b->to_markdown) return NULL;
    return gmb_pdf_take(b->to_markdown((PdfDocument*)h, page, &err), b);
}

char* gmb_doc_pdf_extract_text(void* h, int page)
{
    if (!h) return NULL;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !b->to_plain_text) return NULL;
    int32_t err = 0;
    /* to_plain_text richiede un indice di pagina valido (come to_html):
     * parità NativeDocPdf.cs — page<0 → 0. Prima passava -1 → stringa vuota. */
    return gmb_pdf_take(b->to_plain_text((PdfDocument*)h, page < 0 ? 0 : page, &err), b);
}

char* gmb_doc_pdf_extract_html(void* h, int page)
{
    if (!h) return NULL;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !b->to_html) return NULL;
    int32_t err = 0;
    /* pdf_document_to_html richiede un indice di pagina valido (a differenza di
     * to_markdown, che accetta -1 = tutto). Parità con NativeDocPdf.cs: page<0 → 0. */
    return gmb_pdf_take(b->to_html((PdfDocument*)h, page < 0 ? 0 : page, &err), b);
}

/* ── Search: ritorna "page|testo" per ogni match (una riga per match) ── */
char* gmb_doc_pdf_search(void* h, const char* pattern)
{
    if (!h || !pattern || !pattern[0]) return NULL;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !b->search_all || !b->search_count) return NULL;
    int32_t err = 0;
    FfiSearchResults* r = b->search_all((PdfDocument*)h, pattern, 0, &err);
    if (!r) return NULL;
    gmb_buf out; buf_init(&out, 512);
    int32_t n = b->search_count(r);
    for (int32_t i = 0; i < n; i++)
    {
        int32_t page = b->search_page(r, i, &err);
        char* txt = b->search_text(r, i, &err);
        char pagebuf[16]; snprintf(pagebuf, sizeof(pagebuf), "%d", (int)page);
        buf_app(&out, "p"); buf_app(&out, pagebuf); buf_app(&out, "|");
        buf_app(&out, txt ? txt : "");
        buf_app(&out, "\n");
        if (txt) b->free_string(txt);
    }
    b->search_free(r);
    return buf_take(&out);
}

/* ── Metadata: "pages=N; version=M.m" ── */
char* gmb_doc_pdf_metadata(void* h)
{
    if (!h) return NULL;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b) return NULL;
    gmb_buf out; buf_init(&out, 64);
    if (b->page_count)
    {
        int32_t err = 0;
        int32_t n = b->page_count((PdfDocument*)h, &err);
        char num[24]; snprintf(num, sizeof(num), "pages=%d", (int)n);
        buf_app(&out, num);
    }
    if (b->get_version)
    {
        uint8_t major = 0, minor = 0;
        b->get_version((PdfDocument*)h, &major, &minor);
        char v[32]; snprintf(v, sizeof(v), "; version=%u.%u", (unsigned)major, (unsigned)minor);
        buf_app(&out, v);
    }
    return buf_take(&out);
}

/* ── Estrai le immagini incorporate della pagina in out_dir; ritorna il numero ── */
int gmb_doc_pdf_extract_images(void* h, int page, const char* out_dir)
{
    if (!h || !out_dir) return 0;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !b->get_images || !b->image_count || !b->image_data) return 0;
    int32_t err = 0;
    FfiImageList* im = b->get_images((PdfDocument*)h, page, &err);
    if (!im) return 0;
    int32_t n = b->image_count(im);
    int written = 0;
    for (int32_t i = 0; i < n; i++)
    {
        int32_t data_len = 0;
        uint8_t* data = b->image_data(im, i, &data_len, &err);
        if (!data || data_len <= 0) continue;
        char* fmt = b->image_format(im, i, &err);
        char path[1024];
        if (fmt && fmt[0]) snprintf(path, sizeof(path), "%s/img_%d_%d.%s", out_dir, page, (int)i, fmt);
        else               snprintf(path, sizeof(path), "%s/img_%d_%d.bin", out_dir, page, (int)i);
        FILE* f = fopen(path, "wb");
        if (f) { fwrite(data, 1, (size_t)data_len, f); fclose(f); written++; }
        if (fmt) b->free_string(fmt);
    }
    b->image_list_free(im);
    return written;
}

/* ── Render pagina → RGBA8888 premultiplied (allocata, free()); ritorna buffer o NULL ── */
uint8_t* gmb_doc_pdf_render_rgba(void* h, int page, int dpi, int* out_w, int* out_h)
{
    if (!h) return NULL;
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !b->render_raw || !b->rendered_data) return NULL;
    int32_t err = 0, w = 0, hgt = 0;
    FfiRenderedImage* img = b->render_raw((PdfDocument*)h, page, dpi, &w, &hgt, &err);
    if (!img) return NULL;
    int32_t data_len = 0;
    uint8_t* src = b->rendered_data(img, &data_len, &err);
    uint8_t* copy = NULL;
    if (src && data_len > 0)
    {
        copy = (uint8_t*)malloc((size_t)data_len);
        if (copy) memcpy(copy, src, (size_t)data_len);
    }
    if (out_w) *out_w = (int)w;
    if (out_h) *out_h = (int)hgt;
    if (b->rendered_free) b->rendered_free(img);
    return copy;
}

/* ── Mini writer PNG (RGBA8, filter 0, zlib deflate, CRC32) ── */
static void png_crc32(uint32_t* crc, const uint8_t* data, size_t len)
{
    uint32_t c = *crc;
    for (size_t i = 0; i < len; i++)
    {
        c ^= data[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    *crc = c;
}
static void png_chunk(FILE* f, const char* type, const uint8_t* data, size_t len)
{
    uint8_t lenb[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len };
    fwrite(lenb, 1, 4, f);
    fwrite(type, 1, 4, f);
    uint32_t crc = 0xFFFFFFFFu;
    png_crc32(&crc, (const uint8_t*)type, 4);
    png_crc32(&crc, data, len);
    crc ^= 0xFFFFFFFFu;
    uint8_t crcb[4] = { (uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc };
    if (data && len) fwrite(data, 1, len, f);
    fwrite(crcb, 1, 4, f);
}

/* Render pagina → file PNG (RGBA8). Ritorna 0 ok, -1 errore. */
int gmb_doc_pdf_render_png(void* h, int page, int dpi, const char* out_path)
{
    int w = 0, ht = 0;
    uint8_t* rgba = gmb_doc_pdf_render_rgba(h, page, dpi, &w, &ht);
    if (!rgba || w <= 0 || ht <= 0) { free(rgba); return -1; }
    FILE* f = fopen(out_path, "wb");
    if (!f) { free(rgba); return -1; }
    const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = { 0 };
    ihdr[0] = (uint8_t)(w >> 24); ihdr[1] = (uint8_t)(w >> 16); ihdr[2] = (uint8_t)(w >> 8); ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)(ht >> 24); ihdr[5] = (uint8_t)(ht >> 16); ihdr[6] = (uint8_t)(ht >> 8); ihdr[7] = (uint8_t)ht;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    png_chunk(f, "IHDR", ihdr, 13);

    /* scanline: filter byte 0 + RGBA */
    size_t row = (size_t)w * 4;
    uint8_t* raw = (uint8_t*)malloc(row * (size_t)ht + (size_t)ht);
    if (raw)
    {
        for (int y = 0; y < ht; y++)
        {
            raw[(size_t)y * (row + 1)] = 0;
            memcpy(raw + (size_t)y * (row + 1) + 1, rgba + (size_t)y * row, row);
        }
        uLongf zlen = compressBound((uLong)(row * (size_t)ht + (size_t)ht));
        uint8_t* z = (uint8_t*)malloc(zlen);
        if (z)
        {
            if (compress2(z, &zlen, raw, (uLong)(row * (size_t)ht + (size_t)ht), 6) == Z_OK)
                png_chunk(f, "IDAT", z, zlen);
            free(z);
        }
        free(raw);
    }
    png_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(rgba);
    return 0;
}

/* ── Convert file PDF in fmt {"md","html","text"} ── */
char* gmb_doc_pdf_convert(const char* path, const char* fmt)
{
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !path) return NULL;
    int32_t err = 0;
    PdfDocument* h = b->open(path, &err);
    if (!h) return NULL;

    gmb_buf out; buf_init(&out, 4096);

    if (b->to_markdown_all && (!fmt || strcasecmp(fmt, "md") == 0 || strcasecmp(fmt, "markdown") == 0))
    {
        char* md = b->to_markdown_all(h, &err);
        if (md) { buf_app(&out, md); b->free_string(md); }
    }
    else
    {
        int32_t n = b->page_count(h, &err);
        if (n < 1) n = 1;
        for (int32_t p = 0; p < n; p++)
        {
            char* part = NULL;
            if (fmt && strcasecmp(fmt, "html") == 0 && b->to_html)
                part = b->to_html(h, p, &err);
            else if (fmt && strcasecmp(fmt, "text") == 0 && b->to_plain_text)
                part = b->to_plain_text(h, p, &err);
            else if (b->to_markdown)
                part = b->to_markdown(h, p, &err);
            if (part)
            {
                buf_app(&out, part);
                buf_app(&out, "\n");
                b->free_string(part);
            }
        }
    }

    b->free_doc(h);
    return buf_take(&out);
}

/* ── EXTRACT ibrido per pagina (parità DocExtract C#): text-layer dove presente,
 *    OCR (Apple Vision, macOS, Livello 1) sulle pagine scansionate. Il fast path
 *    a solo text-layer ritorna to_markdown_all grezzo (parità byte col C#). ── */
static int gmb_doc_pdf_page_has_text(const char* md)
{
    if (!md) return 0;
    /* trim */
    while (*md == ' ' || *md == '\n' || *md == '\r' || *md == '\t') md++;
    if (strlen(md) < 20) return 0;
    return !strstr(md, "OCR REQUIRED");
}

char* gmb_doc_pdf_extract_hybrid(void* h)
{
    gmb_pdf_bindings* b = gmb_pdf_load();
    if (!b || !h) return NULL;
    int32_t err = 0;
    int32_t n = b->page_count(h, &err);
    if (n < 1) n = 1;

    /* fast path: documento a solo text-layer → to_markdown_all (parità col C#). */
    if (b->to_markdown_all)
    {
        char* all = b->to_markdown_all(h, &err);
        if (gmb_doc_pdf_page_has_text(all)) return all;
        if (all) b->free_string(all);
    }

    gmb_buf out; buf_init(&out, 4096);
    const char* td = "/tmp";   /* niente env (Regola Primaria): directory temporanea POSIX */
    for (int32_t p = 0; p < n; p++)
    {
        char* md = b->to_markdown(h, p, &err);
        if (gmb_doc_pdf_page_has_text(md))
        {
            if (out.len > 0) buf_appn(&out, "\n", 1);
            buf_app(&out, md);
            if (md) b->free_string(md);
            continue;
        }
        if (md) b->free_string(md);
        /* pagina scansionata → render 150dpi → OCR.
         * Livello 2: Tesseract (cross-OS, bundle LIBS) su RGBA diretto.
         * Livello 1: Apple Vision (macOS) su PNG. */
        char* ocr = NULL;
        int rw = 0, rh = 0;
        uint8_t* rgba = gmb_doc_pdf_render_rgba(h, p, 150, &rw, &rh);
        if (rgba)
        {
            ocr = gmb_doc_ocr_rgba(rgba, rw, rh);
            free(rgba);
        }
        if (!ocr)
        {
            char tmp[1024];
            snprintf(tmp, sizeof(tmp), "%s/gmb_ocr_%lx_p%d.png", td ? td : "/tmp",
                     (unsigned long)(uintptr_t)h, p);
            if (gmb_doc_pdf_render_png(h, p, 150, tmp) == 0)
            {
                ocr = gmb_doc_ocr_image(tmp);
                remove(tmp);
            }
        }
        if (ocr && ocr[0] && !strstr(ocr, "OCR REQUIRED"))
        {
            if (out.len > 0) buf_appn(&out, "\n", 1);
            buf_app(&out, ocr);
            free(ocr);
            continue;
        }
        if (ocr) free(ocr);
        /* nessun motore / nessun testo → placeholder (comportamento attuale) */
        if (out.len > 0) buf_appn(&out, "\n", 1);
        char ph[64];
        snprintf(ph, sizeof(ph), "> [OCR REQUIRED — page %d]", (int)p + 1);
        buf_app(&out, ph);
    }
    b->free_doc(h);
    return buf_take(&out);
}

/* ── Registry documenti a handle intero (namespace DOCUMENT/PDF) ── */
#define GMB_DOC_MAX_HANDLES 64
typedef struct { void* pdf; int inuse; } gmb_doc_slot;
static gmb_doc_slot g_doc_slots[GMB_DOC_MAX_HANDLES];

int gmb_doc_open(const char* path)
{
    void* h = gmb_doc_pdf_open(path);
    if (!h) return 0;
    for (int i = 0; i < GMB_DOC_MAX_HANDLES; i++)
        if (!g_doc_slots[i].inuse) { g_doc_slots[i].pdf = h; g_doc_slots[i].inuse = 1; return i + 1; }
    gmb_doc_pdf_close(h);
    return 0;
}

void gmb_doc_close(int h)
{
    if (h <= 0 || h > GMB_DOC_MAX_HANDLES) return;
    int i = h - 1;
    if (!g_doc_slots[i].inuse) return;
    gmb_doc_pdf_close(g_doc_slots[i].pdf);
    g_doc_slots[i].inuse = 0;
    g_doc_slots[i].pdf = NULL;
}

static void* gmb_doc_handle(int h)
{
    if (h <= 0 || h > GMB_DOC_MAX_HANDLES) return NULL;
    int i = h - 1;
    return g_doc_slots[i].inuse ? g_doc_slots[i].pdf : NULL;
}

int gmb_doc_page_count(int h) { return gmb_doc_pdf_page_count(gmb_doc_handle(h)); }
char* gmb_doc_extract_markdown(int h, int page) { return gmb_doc_pdf_extract_markdown(gmb_doc_handle(h), page); }
char* gmb_doc_extract_text(int h, int page) { return gmb_doc_pdf_extract_text(gmb_doc_handle(h), page); }
char* gmb_doc_extract_html(int h, int page) { return gmb_doc_pdf_extract_html(gmb_doc_handle(h), page); }
char* gmb_doc_metadata(int h) { return gmb_doc_pdf_metadata(gmb_doc_handle(h)); }
char* gmb_doc_search(int h, const char* pattern) { return gmb_doc_pdf_search(gmb_doc_handle(h), pattern); }
int gmb_doc_extract_images(int h, int page, const char* dir) { return gmb_doc_pdf_extract_images(gmb_doc_handle(h), page, dir); }
int gmb_doc_render_png(int h, int page, int dpi, const char* out) { return gmb_doc_pdf_render_png(gmb_doc_handle(h), page, dpi, out); }
