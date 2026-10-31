/* gmb_graphics_screenshot.c — screenshot PNG (estratto da gmb_graphics.c per evitare monolite) */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#ifdef __has_include
#if __has_include(<zlib.h>)
#define GMB_GFX_HAVE_ZLIB 1
#include <zlib.h>
#endif
#else
#include <zlib.h>
#define GMB_GFX_HAVE_ZLIB 1
#endif

/* ============================ Screenshot PNG ============================ */

static void write_u32be(unsigned char* p, uint32_t v) { p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16); p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v; }

static uint32_t png_crc_update(uint32_t c, const unsigned char* data, size_t len)
{
    static uint32_t table[256];
    static int init = 0;
    if (!init)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = (v & 1) ? (0xEDB88320u ^ (v >> 1)) : (v >> 1);
            table[i] = v;
        }
        init = 1;
    }
    for (size_t i = 0; i < len; i++) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c;
}

static void write_png_chunk(FILE* f, const char* type, const unsigned char* data, uint32_t clen)
{
    unsigned char hdr[8];
    write_u32be(hdr, clen);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    fwrite(data, 1, clen, f);
    uint32_t c = png_crc_update(0xFFFFFFFFu, (const unsigned char*)type, 4);
    if (clen > 0) c = png_crc_update(c, data, clen);
    uint32_t crc = c ^ 0xFFFFFFFFu;
    unsigned char crcb[4];
    write_u32be(crcb, crc);
    fwrite(crcb, 1, 4, f);
}

/* F3 multimodale (2026-09-11): non più static — condiviso con gmb_image.c (§0bis). */
int save_png_rgb24(const char* path, const uint32_t* px, int w, int h)
{
#ifdef GMB_GFX_HAVE_ZLIB
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig, 1, 8, f);

    unsigned char ihdr[13];
    write_u32be(ihdr, (uint32_t)w);
    write_u32be(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    write_png_chunk(f, "IHDR", ihdr, 13);

    /* raw scanlines con filtro 0 */
    size_t stride = (size_t)w * 3 + 1;
    unsigned char* raw = (unsigned char*)malloc(stride * (size_t)h);
    if (!raw) { fclose(f); return -1; }
    for (int y = 0; y < h; y++)
    {
        raw[(size_t)y * stride] = 0;
        for (int x = 0; x < w; x++)
        {
            uint32_t c = px[(size_t)y * w + x] & 0xFFFFFF;
            raw[(size_t)y * stride + 1 + (size_t)x * 3 + 0] = (unsigned char)(c >> 16);
            raw[(size_t)y * stride + 1 + (size_t)x * 3 + 1] = (unsigned char)(c >> 8);
            raw[(size_t)y * stride + 1 + (size_t)x * 3 + 2] = (unsigned char)c;
        }
    }
    uLongf clen = compressBound((uLong)(stride * (size_t)h));
    unsigned char* comp = (unsigned char*)malloc(clen);
    if (!comp) { free(raw); fclose(f); return -1; }
    if (compress2(comp, &clen, raw, (uLong)(stride * (size_t)h), Z_BEST_SPEED) != Z_OK) { free(comp); free(raw); fclose(f); return -1; }
    write_png_chunk(f, "IDAT", comp, (uint32_t)clen);
    free(comp);
    free(raw);
    write_png_chunk(f, "IEND", NULL, 0);
    fclose(f);
    return 0;
#else
    (void)path; (void)px; (void)w; (void)h;
    return -1;
#endif
}

/* Default HEADLESS della cattura offscreen (presente in gmb_gfx_present_sdl.c quando
 * SDL è linkato; debole per non rompere gli harness che compilano solo il sottoinsieme
 * runtime senza present SDL). */
#if defined(__GNUC__)
__attribute__((weak))
#endif
int gmb_gfx_present_capture(uint32_t* out, int w, int h)
{
    (void)out; (void)w; (void)h;
    return -1;
}

int gmb_gfx_save_screen_png(const char* path)
{
    /* SAVE SCREEN cattura la pagina VISIBILE (parità C# SaveScreen), non quella
     * attiva di disegno: dopo `SCREENSET 0` con attiva=1 il salvataggio deve
     * mostrare la pagina 0. */
    int vi = g_gfx.visible_page;
    if (vi < 0 || vi >= GMB_GFX_MAX_PAGES) vi = g_gfx.active_page;
    gmb_gfx_page_t* p = &g_gfx.pages[vi];
    if (!p->in_use || !p->pixels) return -1;
    /* Finestra reale: gli sprite sono composti dalla GPU e non sono nella pagina.
     * Rendi offscreen pagina+sprite e salva quel buffer (parità con ciò che si vede). */
    if (path)
    {
        size_t n = (size_t)p->w * (size_t)p->h;
        uint32_t* cap = (uint32_t*)malloc(n * sizeof(uint32_t));
        if (cap)
        {
            if (gmb_gfx_present_capture(cap, p->w, p->h) == 0)
            {
                int r = save_png_rgb24(path, cap, p->w, p->h);
                free(cap);
                return r;
            }
            free(cap);
        }
    }
    return save_png_rgb24(path, p->pixels, p->w, p->h);
}
