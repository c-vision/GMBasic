/* ============================================================================
 * gmb_tui_psf.c — font PSF (porting Psf.cs + Fonts.cs).
 * ============================================================================ */
#include "gmb_tui_psf.h"
#include <string.h>

static uint32_t psf_read_u32(const unsigned char* d, int o)
{
    return (uint32_t)(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24));
}

static uint16_t psf_read_u16(const unsigned char* d, int o)
{
    return (uint16_t)(d[o] | (d[o + 1] << 8));
}

int gmb_psf_from_bytes(gmb_psf_font_t* f, const unsigned char* data, int len)
{
    memset(f, 0, sizeof(*f));
    if (!data || len == 0) { f->error = GMB_PSF_ERR_INVALID; return GMB_PSF_ERR_INVALID; }
    f->raw = data;
    f->raw_len = len;
    unsigned char mode = data[0];
    if (mode == 0x36)
    {
        if (len < 4 || data[1] != 0x04) { f->error = GMB_PSF_ERR_INVALID; return GMB_PSF_ERR_INVALID; }
        int cnt;
        switch (data[2])
        {
            case 0: cnt = 256; break;
            case 1: cnt = 512; break;
            case 2: cnt = 256; break;
            case 3: cnt = 512; break;
            default: f->error = GMB_PSF_ERR_INVALID; return GMB_PSF_ERR_INVALID;
        }
        f->count = cnt;
        f->height = data[3];
        f->width = 8;
        f->byte_width = 1;
        f->offset = 4;
        f->has_unicode = (data[2] & 0x01) != 0;
        f->error = GMB_PSF_OK;
        return GMB_PSF_OK;
    }
    if (mode == 0x72)
    {
        if (len < 32 || data[1] != 0xb5 || data[2] != 0x4a || data[3] != 0x86)
        { f->error = GMB_PSF_ERR_INVALID; return GMB_PSF_ERR_INVALID; }
        uint32_t version = psf_read_u32(data, 4);
        if (version != 0) { f->error = GMB_PSF_ERR_INVALID; return GMB_PSF_ERR_INVALID; }
        uint32_t off = psf_read_u32(data, 8);
        if (off != 0x20) { f->error = GMB_PSF_ERR_INVALID; return GMB_PSF_ERR_INVALID; }
        uint32_t flags = psf_read_u32(data, 12);
        f->count = psf_read_u16(data, 16);
        uint32_t h = psf_read_u32(data, 24);
        uint32_t w = psf_read_u32(data, 28);
        f->width = (int)w;
        f->height = (int)h;
        f->byte_width = ((int)w + 7) / 8;
        f->offset = (int)off;
        f->has_unicode = (flags & 0x01) != 0;
        f->error = GMB_PSF_OK;
        return GMB_PSF_OK;
    }
    f->error = GMB_PSF_ERR_INVALID;
    return GMB_PSF_ERR_INVALID;
}

int gmb_psf_get_glyph(const gmb_psf_font_t* f, int index, unsigned char* out, int cap)
{
    if (!f || index < 0 || index >= f->count) return -1;
    int char_byte_len = f->height * f->byte_width;
    if (!out || cap < char_byte_len) return -1;
    int off = f->offset + index * char_byte_len;
    memcpy(out, f->raw + off, (size_t)char_byte_len);
    return char_byte_len;
}

bool gmb_psf_get_pixel(const gmb_psf_font_t* f, int index, int x, int y)
{
    if (!f || index < 0 || index >= f->count) return false;
    if (x < 0 || x >= f->width || y < 0 || y >= f->height) return false;
    int char_byte_len = f->height * f->byte_width;
    int off = f->offset + index * char_byte_len;
    int bit = y * f->byte_width * 8 + x;
    int byte_idx = off + bit / 8;
    int bit_in_byte = 7 - bit % 8;
    return ((f->raw[byte_idx] >> bit_in_byte) & 0x01) != 0;
}

void gmb_psf_render_glyph(const gmb_psf_font_t* f, int index, gmb_screen_buffer_t* buf,
                          int x, int y, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    gmb_cell_color_t fc = fg ? *fg : GMB_CELLCOLOR_WHITE;
    gmb_cell_color_t bc = bg ? *bg : GMB_CELLCOLOR_BLACK;
    for (int gy = 0; gy < f->height; gy++)
        for (int gx = 0; gx < f->width; gx++)
            if (gmb_psf_get_pixel(f, index, gx, gy))
                gmb_screen_setchar(buf, x + gx, y + gy, 0x2588, fc, bc);
}

void gmb_psf_render_text(const gmb_psf_font_t* f, const char* text, gmb_screen_buffer_t* buf,
                         int x, int y, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (!f || !text) return;
    int gw = f->width;
    int sp = 1;
    int i = 0;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++)
    {
        gmb_psf_render_glyph(f, *p, buf, x + i * (gw + sp), y, fg, bg);
        i++;
    }
}

/* ---- Fonts ---- */

static gmb_psf_font_t g_default_font;
static bool g_has_font = false;

void gmb_fonts_set_default(const gmb_psf_font_t* f)
{
    if (f) { g_default_font = *f; g_has_font = true; }
    else g_has_font = false;
}

int gmb_fonts_render_text(const char* text, gmb_screen_buffer_t* buf, int x, int y,
                          const gmb_cell_color_t* fg, const gmb_cell_color_t* bg)
{
    if (!g_has_font) return 0;
    gmb_psf_render_text(&g_default_font, text, buf, x, y, fg, bg);
    return 1;
}
