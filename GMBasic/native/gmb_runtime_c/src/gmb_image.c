/* gmb_image.c — dominio IMMAGINI (F3 multimodale, TODO Multimodal-Runtime).
 *
 * Primitive PNG condivise da VM/C, C99Native (§0bis — unica implementazione):
 * decode via png_decode, encode via save_png_rgb24 (le stesse di texture/screenshot,
 * esposte in gmb_graphics_internal.h). Resize/crop nearest-neighbor puri.
 *
 * SOLO PNG in decode e in encode: altri formati = errore esplicito in gmb_image_error(),
 * mai stub o risultati silenziosi. Ritorno: 1 = ok, 0 = errore (dettaglio via
 * gmb_image_error()). I pixel sono uint32 0xAARRGGBB (stesso layout del decoder).
 */
#include "gmb_runtime.h"
#include "gmb_graphics_internal.h"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Limite anti-bomba su output generati (decode input: comportamento esistente invariato). */
#define GMB_IMAGE_MAX_DIM 16384

static char g_image_err[1024];

const char* gmb_image_error(void)
{
    return g_image_err;
}

static int img_fail(const char* msg)
{
    snprintf(g_image_err, sizeof(g_image_err), "%s", msg ? msg : "[AI Image: errore]");
    return 0;
}

static int has_png_ext(const char* path)
{
    if (!path) return 0;
    size_t n = strlen(path);
    if (n < 4) return 0;
    const char* e = path + n - 4;
    return (e[0] == '.' &&
            (e[1] == 'p' || e[1] == 'P') &&
            (e[2] == 'n' || e[2] == 'N') &&
            (e[3] == 'g' || e[3] == 'G'));
}

/* Carica+decodifica un PNG: 1 = ok (px allocato, w/h), 0 = errore spiegata. */
static int img_load_png(const char* path, uint32_t** px, int* w, int* h)
{
    size_t len = 0;
    unsigned char* data;
    int rc;
    *px = NULL; *w = *h = 0;
    if (!path || !path[0]) return img_fail("[AI Image: file not found]");
    data = read_whole_file(path, &len);
    if (!data) return img_fail("[AI Image: file not found]");
    if (!has_png_ext(path)) { free(data); return img_fail("[AI Image: formato non supportato (solo PNG)]"); }
    rc = png_decode(data, len, px, w, h);
    free(data);
    if (rc != 0 || !*px) return img_fail("[AI Image: decode PNG fallito]");
    return 1;
}

int gmb_image_info(const char* path, int* w, int* h, char* fmt)
{
    uint32_t* px = NULL;
    int iw = 0, ih = 0;
    if (w) *w = 0;
    if (h) *h = 0;
    if (fmt) snprintf(fmt, 16, "?");
    if (!img_load_png(path, &px, &iw, &ih)) return 0;
    free(px);
    if (w) *w = iw;
    if (h) *h = ih;
    if (fmt) snprintf(fmt, 16, "png");
    return 1;
}

/* Forma stringa "w,h,fmt" (o messaggio d'errore): per i backend che ritornano STRING. */
const char* gmb_image_info_str(const char* path)
{
    static char buf[64];
    int w = 0, h = 0;
    char fmt[16] = "?";
    if (!gmb_image_info(path, &w, &h, fmt)) return gmb_image_error();
    snprintf(buf, sizeof(buf), "%d,%d,%s", w, h, fmt);
    return buf;
}

static uint32_t* img_resize_nn(const uint32_t* src, int sw, int sh, int dw, int dh)
{
    uint32_t* dst = (uint32_t*)malloc((size_t)dw * (size_t)dh * sizeof(uint32_t));
    int x, y;
    if (!dst) return NULL;
    for (y = 0; y < dh; y++)
    {
        int sy = (int)((long long)y * sh / dh);
        if (sy >= sh) sy = sh - 1;
        for (x = 0; x < dw; x++)
        {
            int sx = (int)((long long)x * sw / dw);
            if (sx >= sw) sx = sw - 1;
            dst[(size_t)y * dw + x] = src[(size_t)sy * sw + sx];
        }
    }
    return dst;
}

int gmb_image_resize(const char* in, const char* out, int w, int h)
{
    uint32_t* src = NULL;
    uint32_t* dst = NULL;
    int sw = 0, sh = 0, rc;
    if (w <= 0 || h <= 0) return img_fail("[AI Image: dimensioni non valide]");
    if (w > GMB_IMAGE_MAX_DIM || h > GMB_IMAGE_MAX_DIM) return img_fail("[AI Image: dimensioni eccessive]");
    if (!out || !out[0]) return img_fail("[AI Image: percorso output mancante]");
    if (!has_png_ext(out)) return img_fail("[AI Image: output non PNG (solo PNG)]");
    if (!img_load_png(in, &src, &sw, &sh)) return 0;
    dst = img_resize_nn(src, sw, sh, w, h);
    free(src);
    if (!dst) return img_fail("[AI Image: memoria]");
    rc = save_png_rgb24(out, dst, w, h);
    free(dst);
    if (rc != 0) return img_fail("[AI Image: scrittura output fallita]");
    return 1;
}

int gmb_image_crop(const char* in, const char* out, int x, int y, int w, int h)
{
    uint32_t* src = NULL;
    uint32_t* dst = NULL;
    int sw = 0, sh = 0, rc, dx, dy;
    if (w <= 0 || h <= 0) return img_fail("[AI Image: dimensioni non valide]");
    if (w > GMB_IMAGE_MAX_DIM || h > GMB_IMAGE_MAX_DIM) return img_fail("[AI Image: dimensioni eccessive]");
    if (!out || !out[0]) return img_fail("[AI Image: percorso output mancante]");
    if (!has_png_ext(out)) return img_fail("[AI Image: output non PNG (solo PNG)]");
    if (!img_load_png(in, &src, &sw, &sh)) return 0;
    /* Clamp del rettangolo all'immagine; fuori del tutto = errore esplicito. */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= sw || y >= sh || w <= 0 || h <= 0) { free(src); return img_fail("[AI Image: rettangolo fuori immagine]"); }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    dst = (uint32_t*)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!dst) { free(src); return img_fail("[AI Image: memoria]"); }
    for (dy = 0; dy < h; dy++)
        memcpy(dst + (size_t)dy * w, src + (size_t)(y + dy) * sw + x, (size_t)w * sizeof(uint32_t));
    free(src);
    rc = save_png_rgb24(out, dst, w, h);
    free(dst);
    if (rc != 0) return img_fail("[AI Image: scrittura output fallita]");
    return 1;
}

int gmb_image_convert(const char* in, const char* out)
{
    uint32_t* src = NULL;
    int sw = 0, sh = 0, rc;
    if (!out || !out[0]) return img_fail("[AI Image: percorso output mancante]");
    if (!has_png_ext(out)) return img_fail("[AI Image: output non PNG (solo PNG)]");
    /* Normalizza a PNG: decode + encode identici (pixel preservati, layout 0xAARRGGBB). */
    if (!img_load_png(in, &src, &sw, &sh)) return 0;
    rc = save_png_rgb24(out, src, sw, sh);
    free(src);
    if (rc != 0) return img_fail("[AI Image: scrittura output fallita]");
    return 1;
}

/* ── F5 multimodale (2026-09-11): embedding percettivo + compare (modello-free) ──
 * Embedding deterministico 8x8: luminanza (BT.601 intera) al centro di ogni cella,
 * normalizzata a unità. Stessa formula in C# (ImageUtils.PerceptualEmbed) → parità.
 * Serve per near-duplicate detection e per VectorDB multimodale (senza CLIP). */
#define GMB_IMG_GRID 8
#define GMB_IMG_EMBED_N (GMB_IMG_GRID * GMB_IMG_GRID)

static void img_embed_from_px(const uint32_t* px, int w, int h, double* out)
{
    int gx, gy, i;
    double n = 0.0;
    for (gy = 0; gy < GMB_IMG_GRID; gy++)
    {
        int sy = (int)(((double)gy + 0.5) * h / GMB_IMG_GRID);
        if (sy >= h) sy = h - 1;
        if (sy < 0) sy = 0;
        for (gx = 0; gx < GMB_IMG_GRID; gx++)
        {
            int sx = (int)(((double)gx + 0.5) * w / GMB_IMG_GRID);
            uint32_t c;
            int r, g, b;
            if (sx >= w) sx = w - 1;
            if (sx < 0) sx = 0;
            c = px[(size_t)sy * w + sx];
            r = (int)((c >> 16) & 0xFF);
            g = (int)((c >> 8) & 0xFF);
            b = (int)(c & 0xFF);
            out[gy * GMB_IMG_GRID + gx] = (double)((r * 299 + g * 587 + b * 114) / 1000);
        }
    }
    for (i = 0; i < GMB_IMG_EMBED_N; i++) n += out[i] * out[i];
    n = sqrt(n);
    if (n > 1e-12) for (i = 0; i < GMB_IMG_EMBED_N; i++) out[i] /= n;
}

/* AI.IMAGEEMBED(path): lista di 64 double (unitari). Lista VUOTA su errore (input non PNG
 * o mancante): il chiamante verifica count==0 (mai risultato silenzioso su input valido). */
gmb_list_t gmb_image_embed_list(const char* path)
{
    gmb_list_t l = gmb_list_new();
    uint32_t* px = NULL;
    int w = 0, h = 0, i;
    double e[GMB_IMG_EMBED_N];
    if (!img_load_png(path, &px, &w, &h)) return l;
    img_embed_from_px(px, w, h, e);
    free(px);
    for (i = 0; i < GMB_IMG_EMBED_N; i++) gmb_list_add(&l, gmb_make_val_double(e[i]));
    return l;
}

/* AI.IMAGECOMPARE(p1, p2): similarità 0..100 (cosine degli embedding percettivi, unitari).
 * -1 su errore (file mancante/non PNG). */
int gmb_image_compare(const char* p1, const char* p2)
{
    uint32_t *a = NULL, *b = NULL;
    int w1 = 0, h1 = 0, w2 = 0, h2 = 0, i, pct;
    double e1[GMB_IMG_EMBED_N], e2[GMB_IMG_EMBED_N], dot = 0.0;
    if (!img_load_png(p1, &a, &w1, &h1)) return -1;
    if (!img_load_png(p2, &b, &w2, &h2)) { free(a); return -1; }
    img_embed_from_px(a, w1, h1, e1);
    img_embed_from_px(b, w2, h2, e2);
    free(a); free(b);
    for (i = 0; i < GMB_IMG_EMBED_N; i++) dot += e1[i] * e2[i];
    pct = (int)(dot * 100.0 + 0.5);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

/* AI.IMAGEDETECT / AI.IMAGECLASSIFY: delegano al provider vision (stessa via di
 * AI.ANALYZEIMAGE). Nessun provider → errore esplicito (mai stub). Ritorno heap
 * (il chiamante lo copia; gmb_ai_analyze_image alloca). */
const char* gmb_image_detect(const char* path)
{
    return gmb_ai_analyze_image(path,
        "Rileva gli oggetti nell'immagine. Rispondi SOLO con un array JSON di oggetti "
        "{\"label\",\"confidence\",\"x\",\"y\",\"width\",\"height\"}; nessun altro testo.");
}

const char* gmb_image_classify(const char* path, const char* labels)
{
    char prompt[1024];
    snprintf(prompt, sizeof(prompt),
        "Classifica l'immagine in UNA sola etichetta tra: %s. Rispondi SOLO con l'etichetta.",
        labels && labels[0] ? labels : "(nessuna etichetta fornita)");
    return gmb_ai_analyze_image(path, prompt);
}
