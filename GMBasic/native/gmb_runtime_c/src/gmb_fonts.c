/* gmb_fonts.c — implementazione UNICA del namespace FONTS (TrueType/OpenType).
 *
 * Regola §0bis: la logica che i backend condividono vive qui, non nei backend.
 * stb_truetype e' gia' nel runtime (include/stb_truetype.h + src/stb_truetype_impl.c,
 * unico TU con STB_TRUETYPE_IMPLEMENTATION), quindi qui si usa soltanto l'API.
 *
 * Nessuno stato globale e nessuna cache: ogni chiamata apre il font, lavora e libera.
 * Le strutture restituite sono sempre buffer del CHIAMANTE (adatte al P/Invoke), cosi'
 * IL/PCodeVM possono delegare alla stessa sorgente senza duplicare nulla.
 */
#include "gmb_fonts.h"
#include "gmb_runtime.h"
#include "stb_truetype.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Lettura del file + init di stbtt. Ritorna 0 se pronto. */
static int fonts_open(const char* path, unsigned char** data, stbtt_fontinfo* font) {
    size_t len = 0;
    int offset = 0;
    if (!path || !*path || !data || !font) return -3;
    *data = gmb_read_all_bytes(path, &len);
    if (!*data || len == 0) {
        if (*data) free(*data);
        *data = NULL;
        return -1;
    }
    offset = stbtt_GetFontOffsetForIndex(*data, 0);
    if (offset < 0 || !stbtt_InitFont(font, *data, offset)) {
        free(*data);
        *data = NULL;
        return -2;
    }
    return 0;
}

int gmb_fonts_tt_metrics(const char* path, double pixel_height, int codepoint, double* out8) {
    unsigned char* data = NULL;
    stbtt_fontinfo font;
    float scale = 0.0f;
    int advance = 0, lsb = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int ascent = 0, descent = 0, gap = 0;
    int rc = 0;

    if (!out8) return -3;
    if (!(pixel_height > 0.0)) return -3;
    rc = fonts_open(path, &data, &font);
    if (rc != 0) return rc;

    scale = stbtt_ScaleForPixelHeight(&font, (float)pixel_height);
    if (!(scale > 0.0f)) { free(data); return -2; }

    /* codepoint non coperto: nessun glifo, metriche a zero (mai un errore) */
    if (stbtt_FindGlyphIndex(&font, codepoint) == 0) {
        for (int i = 0; i < 8; i++) out8[i] = 0.0;
        free(data);
        return 0;
    }

    stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);
    stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);

    out8[0] = (double)(x1 - x0);
    out8[1] = (double)(y1 - y0);
    out8[2] = (double)x0;
    out8[3] = (double)y0;
    out8[4] = (double)advance * (double)scale;
    out8[5] = (double)ascent * (double)scale;
    out8[6] = (double)descent * (double)scale;
    out8[7] = (double)gap * (double)scale;

    free(data);
    return 0;
}

int gmb_fonts_tt_render(const char* path, double pixel_height, int codepoint,
                        unsigned char* out, int cap,
                        int* out_w, int* out_h, int* out_xoff, int* out_yoff) {
    unsigned char* data = NULL;
    stbtt_fontinfo font;
    float scale = 0.0f;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, w = 0, h = 0;
    int rc = 0;

    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (out_xoff) *out_xoff = 0;
    if (out_yoff) *out_yoff = 0;
    if (!(pixel_height > 0.0)) return -3;
    if (cap < 0) return -3;

    rc = fonts_open(path, &data, &font);
    if (rc != 0) return rc;

    scale = stbtt_ScaleForPixelHeight(&font, (float)pixel_height);
    if (!(scale > 0.0f)) { free(data); return -2; }

    /* codepoint non coperto: bitmap vuota, non un errore */
    if (stbtt_FindGlyphIndex(&font, codepoint) == 0) { free(data); return 0; }

    stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
    w = x1 - x0;
    h = y1 - y0;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    if (out_xoff) *out_xoff = x0;
    if (out_yoff) *out_yoff = y0;

    if (w == 0 || h == 0) { free(data); return 0; }
    if (!out) { free(data); return -3; }
    if ((long)w * (long)h > (long)cap) { free(data); return -4; }

    memset(out, 0, (size_t)(w * h));
    stbtt_MakeCodepointBitmap(&font, out, w, h, w, scale, scale, codepoint);

    free(data);
    return 0;
}

/* Scansione del cmap: i surrogati non sono caratteri e non vanno esposti. */
static int fonts_is_surrogate(int cp) {
    return cp >= 0xD800 && cp <= 0xDFFF;
}

int gmb_fonts_tt_glyph_count(const char* path) {
    unsigned char* data = NULL;
    stbtt_fontinfo font;
    int n = 0;
    int rc = fonts_open(path, &data, &font);
    if (rc != 0) return -1;
    for (int cp = 0; cp <= 0xFFFF; cp++) {
        if (fonts_is_surrogate(cp)) continue;
        if (stbtt_FindGlyphIndex(&font, cp) != 0) n++;
    }
    free(data);
    return n;
}

/* ---- varianti a lista per i backend C (VM/C e C99Native) -------------------
 * Stessi numeri delle API a buffer: cambia solo l'involucro, cosi' i backend C
 * emettono una chiamata sola (come per IO.READBYTES). */
gmb_list_t gmb_fonts_tt_metrics_list(const char* path, double pixel_height, int codepoint) {
    gmb_list_t l = gmb_list_new();
    double m[8];
    for (int i = 0; i < 8; i++) m[i] = 0.0;
    if (gmb_fonts_tt_metrics(path, pixel_height, codepoint, m) != 0) return l;
    for (int i = 0; i < 8; i++) gmb_list_add(&l, gmb_make_val_double(m[i]));
    return l;
}

gmb_list_t gmb_fonts_tt_render_list(const char* path, double pixel_height, int codepoint) {
    gmb_list_t l = gmb_list_new();
    double m[8];
    int w = 0, h = 0, xo = 0, yo = 0;
    unsigned char* buf = NULL;
    for (int i = 0; i < 8; i++) m[i] = 0.0;
    if (gmb_fonts_tt_metrics(path, pixel_height, codepoint, m) != 0) return l;
    w = (int)(m[0] + 0.5);
    h = (int)(m[1] + 0.5);
    if (w <= 0 || h <= 0) return l;
    buf = (unsigned char*)malloc((size_t)w * (size_t)h);
    if (!buf) return l;
    if (gmb_fonts_tt_render(path, pixel_height, codepoint, buf, w * h, &w, &h, &xo, &yo) == 0) {
        for (int i = 0; i < w * h; i++) gmb_list_add(&l, gmb_make_val_double(buf[i]));
    }
    free(buf);
    return l;
}

gmb_list_t gmb_fonts_tt_glyphs_list(const char* path) {
    gmb_list_t l = gmb_list_new();
    int n = gmb_fonts_tt_glyph_count(path);
    int got = 0;
    int* codes = NULL;
    if (n <= 0) return l;
    codes = (int*)malloc((size_t)n * sizeof(int));
    if (!codes) return l;
    got = gmb_fonts_tt_glyph_codes(path, codes, n);
    if (got > n) got = n;
    for (int i = 0; i < got; i++) gmb_list_add(&l, gmb_make_val_double(codes[i]));
    free(codes);
    return l;
}

int gmb_fonts_tt_glyph_codes(const char* path, int* out, int cap) {
    unsigned char* data = NULL;
    stbtt_fontinfo font;
    int n = 0;
    int rc = fonts_open(path, &data, &font);
    if (rc != 0) return -1;
    if (cap < 0) { free(data); return -3; }
    for (int cp = 0; cp <= 0xFFFF; cp++) {
        if (fonts_is_surrogate(cp)) continue;
        if (stbtt_FindGlyphIndex(&font, cp) == 0) continue;
        if (out && n < cap) out[n] = cp;
        n++;
    }
    free(data);
    return n;
}
