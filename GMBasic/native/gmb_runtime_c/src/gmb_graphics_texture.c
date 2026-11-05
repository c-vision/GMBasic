/* gmb_graphics_texture.c — texture/PNG/sprite texture (estratto da gmb_graphics.c per evitare monolite) */
#include "gmb_graphics.h"
extern float gmb_gfx_scale_eff(void);
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __has_include
#if __has_include(<zlib.h>)
#define GMB_GFX_HAVE_ZLIB 1
#include <zlib.h>
#endif
#else
#include <zlib.h>
#define GMB_GFX_HAVE_ZLIB 1
#endif

/* ============================== PNG decode ============================== */

static uint32_t read_u32be(const unsigned char* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}

/* F3 multimodale (2026-09-11): non più static — condiviso con gmb_image.c via
 * gmb_graphics_internal.h (unica implementazione del decode PNG, §0bis). */
int png_decode(const unsigned char* data, size_t len, uint32_t** out_px, int* out_w, int* out_h)
{
    *out_px = NULL;
    *out_w = *out_h = 0;
#ifdef GMB_GFX_HAVE_ZLIB
    const unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    if (len < 8 || memcmp(data, sig, 8) != 0) return -1;

    size_t off = 8;
    int width = 0, height = 0, bit_depth = 0, color_type = 0;
    unsigned char palette[256][4];
    int pal_n = 0;
    int have_pal = 0;
    int trns_rgb[6]; int trns_n = 0;
    unsigned char* idat = NULL;
    size_t idat_len = 0, idat_cap = 0;

    while (off + 8 <= len)
    {
        size_t clen = read_u32be(data + off);
        const char* ctype = (const char*)(data + off + 4);
        const unsigned char* cdata = data + off + 8;
        if (off + 8 + clen > len) { free(idat); return -1; }
        if (memcmp(ctype, "IHDR", 4) == 0 && clen >= 13)
        {
            width = (int)read_u32be(cdata);
            height = (int)read_u32be(cdata + 4);
            bit_depth = cdata[8];
            color_type = cdata[9];
        }
        else if (memcmp(ctype, "PLTE", 4) == 0)
        {
            int n = (int)(clen / 3);
            if (n > 256) n = 256;
            for (int i = 0; i < n; i++) { palette[i][0]=cdata[i*3]; palette[i][1]=cdata[i*3+1]; palette[i][2]=cdata[i*3+2]; palette[i][3]=255; }
            pal_n = n; have_pal = 1;
        }
        else if (memcmp(ctype, "tRNS", 4) == 0)
        {
            if (clen > 6) clen = 6;
            trns_n = (int)clen;
            for (int i = 0; i < trns_n; i++) trns_rgb[i] = cdata[i];
            if (have_pal && pal_n > 0)
            {
                int n = trns_n < pal_n ? trns_n : pal_n;
                for (int i = 0; i < n; i++) palette[i][3] = cdata[i];
            }
        }
        else if (memcmp(ctype, "IDAT", 4) == 0)
        {
            if (idat_len + clen > idat_cap)
            {
                size_t ncap = idat_cap ? idat_cap * 2 : clen + 1024;
                unsigned char* nidat = (unsigned char*)realloc(idat, ncap);
                if (!nidat) { free(idat); return -1; }
                idat = nidat; idat_cap = ncap;
            }
            memcpy(idat + idat_len, cdata, clen);
            idat_len += clen;
        }
        else if (memcmp(ctype, "IEND", 4) == 0)
            break;
        off += 12 + clen;
    }

    if (width <= 0 || height <= 0 || bit_depth == 0 || idat_len == 0) { free(idat); return -1; }
    if (bit_depth == 16) bit_depth = 8;

    int channels = (color_type == 2) ? 3 : (color_type == 6) ? 4 : (color_type == 0) ? 1 : (color_type == 4) ? 2 : (color_type == 3) ? 1 : 0;
    if (channels == 0) { free(idat); return -1; }
    /* PNG può avere bit depth 1/2/4 (palette/gray/bilevel): stride in BYTE =
     * ceil(width * bit_depth * channels / 8). Prima si assumeva 8 bit per campione:
     * una palette 4-bit (sprite.png 32x32) veniva letta con stride doppio → colori
     * corrotti. Il filtro PNG lavora sui BYTE con bpp = ceil(bits/8) (min 1). */
    int sample_bits = bit_depth * channels;
    int bpp = (sample_bits + 7) / 8; if (bpp < 1) bpp = 1;
    size_t stride = ((size_t)width * (size_t)sample_bits + 7) / 8;
    /* F3 multimodale (2026-09-11): +1 byte di filtro per riga — prima raw_len ometteva i
     * filter byte e uncompress falliva SEMPRE con Z_BUF_ERROR (decode PNG inutilizzabile). */
    size_t raw_len = (stride + 1) * (size_t)height;

    unsigned char* raw = (unsigned char*)malloc(raw_len);
    if (!raw) { free(idat); return -1; }
    uLongf dest_len = (uLongf)raw_len;
    int rc = uncompress(raw, &dest_len, idat, (uLong)idat_len);
    free(idat);
    if (rc != Z_OK) { free(raw); return -1; }

    uint32_t* px = (uint32_t*)malloc((size_t)width * height * sizeof(uint32_t));
    if (!px) { free(raw); return -1; }

    /* Unfilter per BYTE (standard PNG: bpp = byte per pixel, min 1). */
    for (int y = 0; y < height; y++)
    {
        int filter = raw[(size_t)y * (stride + 1)];
        unsigned char* row = raw + (size_t)y * (stride + 1) + 1;
        const unsigned char* prev = (y == 0) ? NULL : raw + (size_t)(y - 1) * (stride + 1) + 1;
        for (size_t i = 0; i < stride; i++)
        {
            int v = row[i];
            int a = (i == 0) ? 0 : row[i - 1];
            int b = (y == 0) ? 0 : prev[i];
            int cc = (y == 0 || i == 0) ? 0 : prev[i - 1];
            switch (filter)
            {
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += paeth(a, b, cc); break;
                default: break;
            }
            row[i] = (unsigned char)(v & 0xFF);
        }
    }

    /* Estrazione campione a profondità arbitraria (1/2/4/8) per canale singolo. */
    #define PNG_SAMPLE(row, x, depth) \
        ((depth) == 8 ? (unsigned)((row)[(x)]) : \
         (unsigned)(((row)[(((size_t)(x) * (depth)) >> 3)] >> (8 - (depth) - (int)(((size_t)(x) * (depth)) & 7))) & ((1u << (depth)) - 1u)))

    for (int y = 0; y < height; y++)
    {
        const unsigned char* row = raw + (size_t)y * (stride + 1) + 1;
        for (int x = 0; x < width; x++)
        {
            uint32_t r, g, b, a;
            switch (color_type)
            {
                case 0: {
                    unsigned v = PNG_SAMPLE(row, x, bit_depth);
                    if (bit_depth < 8) v = v * 255u / ((1u << bit_depth) - 1u);
                    r = g = b = v; a = 255; break;
                }
                case 2: { const unsigned char* p = row + (size_t)x * 3; r = p[0]; g = p[1]; b = p[2]; a = 255; break; }
                case 3: {
                    unsigned idx = PNG_SAMPLE(row, x, bit_depth);
                    if (idx < (unsigned)pal_n && have_pal) { r = palette[idx][0]; g = palette[idx][1]; b = palette[idx][2]; a = palette[idx][3]; }
                    else { r = g = b = (idx * 255u) / ((1u << bit_depth) - 1u); a = 255; }
                    break;
                }
                case 4: { const unsigned char* p = row + (size_t)x * 2; r = g = b = p[0]; a = p[1]; break; }
                case 6: { const unsigned char* p = row + (size_t)x * 4; r = p[0]; g = p[1]; b = p[2]; a = p[3]; break; }
                default: r = g = b = 0; a = 255; break;
            }
            (void)trns_rgb; (void)trns_n;
            px[(size_t)y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    #undef PNG_SAMPLE
    free(raw);
    *out_px = px;
    *out_w = width;
    *out_h = height;
    return 0;
#else
    (void)data; (void)len; (void)out_px; (void)out_w; (void)out_h;
    return -1;
#endif
}

/* ============================== Texture ============================== */

static int register_image(uint32_t* px, int w, int h, int cell_w, int cell_h)
{
    if (g_gfx.next_tex_id >= GMB_GFX_MAX_TEXTURES) return -1;
    int id = g_gfx.next_tex_id++;
    gmb_gfx_image_t* t = &g_gfx.textures[id];
    t->pixels = px;
    t->w = w;
    t->h = h;
    t->cell_w = cell_w;
    t->cell_h = cell_h;
    t->frames = (cell_w > 0 && cell_h > 0) ? (w / cell_w) * (h / cell_h) : 1;
    t->in_use = 1;
    return id;
}

int gmb_gfx_texture_info(int id, const uint32_t** px, int* w, int* h)
{
    if (id < 0 || id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[id].in_use) return 0;
    gmb_gfx_image_t* t = &g_gfx.textures[id];
    if (!t->pixels) return 0;
    if (px) *px = t->pixels;
    if (w) *w = t->w;
    if (h) *h = t->h;
    return 1;
}

int gmb_gfx_register_rgba(uint32_t* px, int w, int h)
{
    if (!px || w <= 0 || h <= 0) return -1;
    gmb_gfx_init();
    return register_image(px, w, h, 0, 0);
}

int gmb_gfx_restore_screen_png(const char* path)
{
    size_t len = 0;
    unsigned char* data;
    uint32_t* px = NULL;
    int w = 0, h = 0;
    gmb_gfx_page_t* p;
    if (!path) return -1;
    data = read_whole_file(path, &len);
    if (!data) return -1;
    if (png_decode(data, len, &px, &w, &h) != 0 || !px) { free(data); return -1; }
    free(data);
    /* RESTORE SCREEN scrive sulla pagina VISIBILE (parità C# RestoreScreen). */
    { int vi = g_gfx.visible_page;
      if (vi < 0 || vi >= GMB_GFX_MAX_PAGES) vi = g_gfx.active_page;
      p = &g_gfx.pages[vi];
      if (!p->in_use || !p->pixels) { free(px); return -1; } }
    {
        int cw = w < p->w ? w : p->w;
        int ch = h < p->h ? h : p->h;
        for (int y = 0; y < ch; y++)
            memcpy(p->pixels + (size_t)y * p->w, px + (size_t)y * w, (size_t)cw * sizeof(uint32_t));
    }
    free(px);
    return 0;
}

static int load_png_as_texture(const char* path, int cell_w, int cell_h)
{
    size_t len = 0;
    unsigned char* data = read_whole_file(path, &len);
    if (!data) return -1;
    uint32_t* px = NULL;
    int w = 0, h = 0;
    int rc = png_decode(data, len, &px, &w, &h);
    free(data);
    if (rc != 0 || !px) return -1;
    return register_image(px, w, h, cell_w, cell_h);
}

int gmb_gfx_load_texture(const char* path)
{
    gmb_gfx_init();
    if (gmb_gfx_null_mode()) return 1;
    if (!path) return -1;
    return load_png_as_texture(path, 0, 0);
}

int gmb_gfx_load_texture_sheet(const char* path, int cell_w, int cell_h)
{
    gmb_gfx_init();
    if (gmb_gfx_null_mode()) return 1;
    if (!path) return -1;
    if (cell_w <= 0 || cell_h <= 0) return load_png_as_texture(path, 0, 0);

    /* Parità C# LoadTextureSheetInternal: lo spritesheet viene TAGLIATO in una texture
     * 1:1 per cella (row-major), restituendo l'id della PRIMA. L'emitter riempie poi
     * l'array con base+i (e usa texture_frames(base) per il conteggio). Prima si
     * registrava UNA texture con tutto il foglio: la demo spritesheet disegnava
     * l'intero foglio per ogni cella (sprite enormi e sbagliati). */
    size_t len = 0;
    unsigned char* data = read_whole_file(path, &len);
    if (!data) return -1;
    uint32_t* px = NULL; int w = 0, h = 0;
    int rc = png_decode(data, len, &px, &w, &h);
    free(data);
    if (rc != 0 || !px) return -1;

    int cols = w / cell_w, rows = h / cell_h;
    if (cols < 1 || rows < 1) { free(px); return -1; }
    int base = -1, total = 0;
    for (int ry = 0; ry < rows; ry++)
        for (int rx = 0; rx < cols; rx++)
        {
            uint32_t* cell = (uint32_t*)malloc((size_t)cell_w * cell_h * sizeof(uint32_t));
            if (!cell) continue;
            for (int yy = 0; yy < cell_h; yy++)
                memcpy(cell + (size_t)yy * cell_w,
                       px + (size_t)(ry * cell_h + yy) * w + rx * cell_w,
                       (size_t)cell_w * sizeof(uint32_t));
            int id = register_image(cell, cell_w, cell_h, 0, 0);
            if (id < 0) { free(cell); continue; }
            if (base < 0) base = id;
            total++;
        }
    free(px);
    /* frames del PRIMO id = numero di celle: l'emitter lo usa per riempire l'array. */
    if (base >= 0 && total > 0) g_gfx.textures[base].frames = total;
    return base;
}

int gmb_gfx_texture_from_png(const char* path)
{
    return gmb_gfx_load_texture(path);
}

int gmb_gfx_texture_frames(int id)
{
    if (id <= 0 || id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[id].in_use) return 0;
    return g_gfx.textures[id].frames;
}

int gmb_gfx_texture_size(int id, int* w, int* h)
{
    if (id <= 0 || id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[id].in_use) return -1;
    if (w) *w = g_gfx.textures[id].w;
    if (h) *h = g_gfx.textures[id].h;
    return 0;
}

int gmb_gfx_create_solid_texture(int w, int h, uint32_t rgba)
{
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return -1;
    uint32_t* px = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    if (!px) return -1;
    for (int i = 0; i < w * h; i++) px[i] = rgba;
    gmb_gfx_init();
    return register_image(px, w, h, 0, 0);
}

/* Campionamento BILINEARE RGBA (alpha premoltiplicato) del texel continuo (tx,ty).
 * Usato dal percorso RUOTATO: con nearest i bordi/contorni degli sprite risultavano
 * scalettati e "grigi" (parità col filtro lineare GPU del motore C#). */
static uint32_t tex_bilinear_rgba(const uint32_t* px, int w, int h, float tx, float ty)
{
    float fx0 = tx - 0.5f, fy0 = ty - 0.5f;
    int x0 = (int)floorf(fx0), y0 = (int)floorf(fy0);
    float fx = fx0 - (float)x0, fy = fy0 - (float)y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 >= w) x1 = w - 1; if (y1 >= h) y1 = h - 1;
    if (x0 >= w) x0 = w - 1; if (y0 >= h) y0 = h - 1;
    uint32_t p00 = px[(size_t)y0 * w + x0], p10 = px[(size_t)y0 * w + x1];
    uint32_t p01 = px[(size_t)y1 * w + x0], p11 = px[(size_t)y1 * w + x1];
    float a00 = (float)((p00 >> 24) & 0xFF) / 255.0f, a10 = (float)((p10 >> 24) & 0xFF) / 255.0f;
    float a01 = (float)((p01 >> 24) & 0xFF) / 255.0f, a11 = (float)((p11 >> 24) & 0xFF) / 255.0f;
    float w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
    float a = a00 * w00 + a10 * w10 + a01 * w01 + a11 * w11;
    if (a < 0.03f) return 0;
    float r = ((p00 >> 16 & 255) * a00 * w00 + (p10 >> 16 & 255) * a10 * w10
             + (p01 >> 16 & 255) * a01 * w01 + (p11 >> 16 & 255) * a11 * w11) / a;
    float g = ((p00 >> 8 & 255) * a00 * w00 + (p10 >> 8 & 255) * a10 * w10
             + (p01 >> 8 & 255) * a01 * w01 + (p11 >> 8 & 255) * a11 * w11) / a;
    float b = ((p00 & 255) * a00 * w00 + (p10 & 255) * a10 * w10
             + (p01 & 255) * a01 * w01 + (p11 & 255) * a11 * w11) / a;
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    unsigned aa = (unsigned)(a * 255.0f + 0.5f); if (aa > 255) aa = 255;
    return (aa << 24) | ((unsigned)(r + 0.5f) << 16) | ((unsigned)(g + 0.5f) << 8) | (unsigned)(b + 0.5f);
}

void gmb_gfx_draw_texture(int id, int x, int y, double rotation, double scale_x, double scale_y)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    if (id < 0 || id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[id].in_use) return;
    gmb_gfx_image_t* t = &g_gfx.textures[id];
    if (!t->pixels) return;

    /* Scala 0 = sprite INVISIBILE (parità C# DrawTextureInternal: dest w/h = size*0).
     * Prima 0 era interpretato come "scala omessa" → texture intera: gli slot
     * sentinella (0,0, scala 0) di DRAW TEXTURES apparivano come sprite fermi a (0,0). */
    if (scale_x == 0.0 || scale_y == 0.0) return;
    int dw = (int)(t->w * fabs(scale_x));
    int dh = (int)(t->h * fabs(scale_y));
    if (dw <= 0 || dh <= 0) return;
    /* (x,y) è il CENTRO dello sprite (parità C# DrawTextureInternal: sx = x - dw/2,
     * sy = y - dh/2). Prima il runtime C lo trattava come angolo in alto a sinistra:
     * gli sprite risultavano spostati/accavallati ("grandi e sbagliati"). */
    x -= dw / 2;
    y -= dh / 2;
    /* Coordinate/size LOGICHE → NATIVE (renderScale): anche le texture devono scalare,
     * altrimenti su Retina gli sprite restano nel quarto in alto a sinistra. */
    { float s = gmb_gfx_scale_eff();
      if (s != 1.0f) { x = (int)lround((double)x * s); y = (int)lround((double)y * s);
                       dw = (int)lround((double)dw * s); dh = (int)lround((double)dh * s); } }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;

    /* DRIVER VIDEO REALE (Cocoa/X11): lo sprite va composto dalla GPU nel present
     * (SDL_RenderCopyEx), come il motore C# — niente blit CPU sulla pagina, che era
     * il collo di bottiglia (animazioni a scatti). Headless/dummy: percorso software
     * sotto (necessario per SAVE/IMAGEGET deterministici).
     * Con il backend VULKAN attivo il present carica la pagina CPU: gli sprite devono
     * quindi essere composti in software finché non c'è una pipeline sprite Vulkan. */
    { extern int gmb_gfx_present_video_is_real(void);
#ifdef GMB_GFX_VULKAN
      extern int gmb_gfx_vk_active(void);
      int _vk_on = gmb_gfx_vk_active();
#else
      int _vk_on = 0;
#endif
#if defined(GMB_GFX_SDL_PRESENT) || defined(GMB_GFX_VULKAN)
      /* `gmb_gfx_present_push_sprite` esiste solo nei backend di PRESENTAZIONE. In una build
       * headless (container/CI senza SDL) il simbolo non c'è: su macOS il dylib tollera un
       * riferimento indefinito, Linux no ("undefined symbol" al dlopen). Il ramo non può
       * comunque essere preso: senza backend `gmb_gfx_present_video_is_real()` è 0. */
      if (gmb_gfx_present_video_is_real() && !_vk_on)
      {
          gmb_gfx_present_push_sprite(id, x, y, dw, dh, rotation);
          return;
      }
#endif
      }

    /* UPSCALE: bilineare solo per fattori NON interi (es. 1.5x); per ingrandimenti
     * interi (2x, 3x, 5x — sprite pixel-art su pagina Retina) si usa NEAREST: resta
     * nitido, senza sfocatura. */
    int int_up = (t->w > 0 && t->h > 0 && dw % t->w == 0 && dh % t->h == 0);
    if (fabs(rotation) < 0.001 && !int_up && (dw > t->w || dh > t->h))
    {
        for (int yy = 0; yy < dh; yy++)
        {
            float sy = ((float)yy + 0.5f) * (float)t->h / (float)dh - 0.5f;
            int y0 = (int)floorf(sy); float fy = sy - (float)y0;
            int y1 = y0 + 1;
            if (y0 < 0) y0 = 0;
            if (y1 >= t->h) y1 = t->h - 1;
            for (int xx = 0; xx < dw; xx++)
            {
                float sx = ((float)xx + 0.5f) * (float)t->w / (float)dw - 0.5f;
                int x0 = (int)floorf(sx); float fx = sx - (float)x0;
                int x1 = x0 + 1;
                if (x0 < 0) x0 = 0;
                if (x1 >= t->w) x1 = t->w - 1;
                uint32_t p00 = t->pixels[(size_t)y0 * t->w + x0];
                uint32_t p10 = t->pixels[(size_t)y0 * t->w + x1];
                uint32_t p01 = t->pixels[(size_t)y1 * t->w + x0];
                uint32_t p11 = t->pixels[(size_t)y1 * t->w + x1];
                float a00 = (float)((p00 >> 24) & 0xFF) / 255.0f;
                float a10 = (float)((p10 >> 24) & 0xFF) / 255.0f;
                float a01 = (float)((p01 >> 24) & 0xFF) / 255.0f;
                float a11 = (float)((p11 >> 24) & 0xFF) / 255.0f;
                float w00 = (1.0f - fx) * (1.0f - fy), w10 = fx * (1.0f - fy);
                float w01 = (1.0f - fx) * fy,          w11 = fx * fy;
                float a = a00 * w00 + a10 * w10 + a01 * w01 + a11 * w11;
                if (a < 0.03f) continue;   /* trasparente */
                float r = ((float)((p00 >> 16) & 0xFF) * a00 * w00 + (float)((p10 >> 16) & 0xFF) * a10 * w10
                         + (float)((p01 >> 16) & 0xFF) * a01 * w01 + (float)((p11 >> 16) & 0xFF) * a11 * w11) / a;
                float g = ((float)((p00 >> 8) & 0xFF) * a00 * w00 + (float)((p10 >> 8) & 0xFF) * a10 * w10
                         + (float)((p01 >> 8) & 0xFF) * a01 * w01 + (float)((p11 >> 8) & 0xFF) * a11 * w11) / a;
                float b = ((float)(p00 & 0xFF) * a00 * w00 + (float)(p10 & 0xFF) * a10 * w10
                         + (float)(p01 & 0xFF) * a01 * w01 + (float)(p11 & 0xFF) * a11 * w11) / a;
                if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
                if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
                unsigned aa = (unsigned)(a * 255.0f + 0.5f);
                if (aa > 255) aa = 255;
                unsigned rgba = (aa << 24) | ((unsigned)(r + 0.5f) << 16) | ((unsigned)(g + 0.5f) << 8) | (unsigned)(b + 0.5f);
                gmb_gfx_blend_pixel(x + xx, y + yy, rgba, 1.0);
            }
        }
        return;
    }

    if (fabs(rotation) < 0.001) {
        /* Fast path: texture non ruotata, interamente in-bounds, nessun viewport → scrittura
           diretta senza i check per-pixel di put_pixel (benchmark texture pesanti). */
        if (!g_gfx.view_set && x >= 0 && y >= 0 && x + dw <= p->w && y + dh <= p->h)
        {
            size_t row = (size_t)p->w;
            for (int yy = 0; yy < dh; yy++)
            {
                int sy_pix = (dh > 1) ? yy * t->h / dh : 0;
                if (sy_pix >= t->h) sy_pix = t->h - 1;
                uint32_t* dst = p->pixels + (size_t)(y + yy) * row + x;
                const uint32_t* srcrow = t->pixels + (size_t)sy_pix * t->w;
                for (int xx = 0; xx < dw; xx++)
                {
                    int sx_pix = (dw > 1) ? xx * t->w / dw : 0;
                    if (sx_pix >= t->w) sx_pix = t->w - 1;
                    uint32_t pix = srcrow[sx_pix];
                    if (((unsigned char)(pix >> 24)) >= 8)
                        dst[xx] = pix & 0xFFFFFF;
                }
            }
            return;
        }
        for (int yy = 0; yy < dh; yy++)
        {
            int sy_pix = (dh > 1) ? yy * t->h / dh : 0;
            if (sy_pix >= t->h) sy_pix = t->h - 1;
            for (int xx = 0; xx < dw; xx++)
            {
                int sx_pix = (dw > 1) ? xx * t->w / dw : 0;
                if (sx_pix >= t->w) sx_pix = t->w - 1;
                uint32_t pix = t->pixels[(size_t)sy_pix * t->w + sx_pix];
                unsigned char a = (unsigned char)(pix >> 24);
                if (a < 8) continue;
                put_pixel(p, x + xx, y + yy, pix & 0xFFFFFF);
            }
        }
        return;
    }
    double rad = rotation * M_PI / 180.0;
    double cosR = cos(rad), sinR = sin(rad);
    double cx = dw * 0.5, cy = dh * 0.5;
    double hw = dw * 0.5, hh = dh * 0.5;
    int bw = (int)ceil(fabs(dw * cosR) + fabs(dh * sinR));
    int bh = (int)ceil(fabs(dw * sinR) + fabs(dh * cosR));
    if (bw <= 0) bw = dw;
    if (bh <= 0) bh = dh;
    for (int dy = 0; dy < bh; dy++) {
        for (int dx = 0; dx < bw; dx++) {
            double rx = (dx - bw*0.5), ry = (dy - bh*0.5);
            double src_cx =  rx * cosR + ry * sinR;
            double src_cy = -rx * sinR + ry * cosR;
            double sx_f = src_cx + cx;
            double sy_f = src_cy + cy;
            /* Bilineare (come il filtro lineare GPU del motore C#): bordi morbidi,
             * niente alone grigio/scalettato dai texel di contorno. */
            float tx = (float)(sx_f * (double)t->w / (double)dw);
            float ty = (float)(sy_f * (double)t->h / (double)dh);
            if (tx < -0.5f || ty < -0.5f || tx > (float)t->w + 0.5f || ty > (float)t->h + 0.5f) continue;
            uint32_t pix = tex_bilinear_rgba(t->pixels, t->w, t->h, tx, ty);
            if (((unsigned char)(pix >> 24)) < 8) continue;
            int px = x + (int)(hw - bw*0.5) + dx;
            int py = y + (int)(hh - bh*0.5) + dy;
            gmb_gfx_blend_pixel(px, py, pix, 1.0);
        }
    }
}

void gmb_gfx_draw_texture_frame(int id, int frame, int x, int y)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    if (id < 0 || id >= GMB_GFX_MAX_TEXTURES || !g_gfx.textures[id].in_use) return;
    gmb_gfx_image_t* t = &g_gfx.textures[id];
    if (!t->pixels || t->cell_w <= 0 || t->cell_h <= 0) { gmb_gfx_draw_texture(id, x, y, 0, 1, 1); return; }
    int per_row = t->w / t->cell_w;
    if (per_row <= 0) per_row = 1;
    int fx = (frame % per_row) * t->cell_w;
    int fy = (frame / per_row) * t->cell_h;
    float s = gmb_gfx_scale_eff();
    int cw = (int)lround((double)t->cell_w * s);
    int ch = (int)lround((double)t->cell_h * s);
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    gmb_gfx_scale_pt(&x, &y);
    for (int yy = 0; yy < ch; yy++)
    {
        int sy = (ch > 1) ? yy * t->cell_h / ch : 0;
        if (sy >= t->cell_h) sy = t->cell_h - 1;
        for (int xx = 0; xx < cw; xx++)
        {
            int sx = (cw > 1) ? xx * t->cell_w / cw : 0;
            if (sx >= t->cell_w) sx = t->cell_w - 1;
            uint32_t pix = t->pixels[(size_t)(fy + sy) * t->w + (fx + sx)];
            if (((unsigned char)(pix >> 24)) < 8) continue;
            put_pixel(p, x + xx, y + yy, pix & 0xFFFFFF);
        }
    }
}

void gmb_gfx_unload_texture(int id)
{
    if (id <= 0 || id >= GMB_GFX_MAX_TEXTURES) return;
    gmb_gfx_image_t* t = &g_gfx.textures[id];
    if (!t->in_use) return;
    free(t->pixels);
    t->pixels = NULL;
    t->in_use = 0;
}

void gmb_gfx_draw_pixel_buffer(int width, int height, const uint32_t* pixels)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    if (!pixels || width <= 0 || height <= 0) return;
    /* Buffer LOGICO → pagina NATIVA (renderScale): un gradiente 320x240 su Retina
     * deve occupare 640x480 nativi (= 320x240 logici), non un quarto della pagina
     * in alto a sinistra. Nearest (pixel-art/buffer). */
    float s = gmb_gfx_scale_eff();
    int dw = (int)lround((double)width * s);
    int dh = (int)lround((double)height * s);
    if (dw < 1) dw = width;
    if (dh < 1) dh = height;
    for (int y = 0; y < dh && y < p->h; y++)
    {
        int sy = (dh > 1) ? y * height / dh : 0;
        if (sy >= height) sy = height - 1;
        for (int x = 0; x < dw && x < p->w; x++)
        {
            int sx = (dw > 1) ? x * width / dw : 0;
            if (sx >= width) sx = width - 1;
            p->pixels[(size_t)y * p->w + x] = pixels[(size_t)sy * width + sx] & 0xFFFFFF;
        }
    }
}
