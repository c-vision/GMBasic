/* ============================================================================
 * gmb_tui_psf.h — font PSF (specchio ClipperLib/Clipper Psf.cs + Fonts.cs).
 * Lettore PSF1/PSF2 e rendering glyph come blocchi '█'. Puro, TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_PSF_H
#define GMB_TUI_PSF_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GMB_PSF_OK = 0, GMB_PSF_ERR_UNKNOWN, GMB_PSF_ERR_NOT_FOUND, GMB_PSF_ERR_IO, GMB_PSF_ERR_INVALID
};

typedef struct gmb_psf_font {
    const unsigned char* raw;
    int raw_len;
    int offset;
    int count;
    int width;
    int height;
    int byte_width;
    bool has_unicode;
    int error;
} gmb_psf_font_t;

int  gmb_psf_from_bytes(gmb_psf_font_t* f, const unsigned char* data, int len);
int  gmb_psf_get_glyph(const gmb_psf_font_t* f, int index, unsigned char* out, int cap);
bool gmb_psf_get_pixel(const gmb_psf_font_t* f, int index, int x, int y);
void gmb_psf_render_glyph(const gmb_psf_font_t* f, int index, gmb_screen_buffer_t* buf,
                          int x, int y, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);
void gmb_psf_render_text(const gmb_psf_font_t* f, const char* text, gmb_screen_buffer_t* buf,
                         int x, int y, const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);

/* ---- Fonts (specchio Fonts.cs) ---- */
void gmb_fonts_set_default(const gmb_psf_font_t* f);
int  gmb_fonts_render_text(const char* text, gmb_screen_buffer_t* buf, int x, int y,
                           const gmb_cell_color_t* fg, const gmb_cell_color_t* bg);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_PSF_H */
