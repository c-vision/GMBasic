/* ============================================================================
 * gmb_tui_text.h — modello testo stilizzato (specchio ClipperLib/Clipper:
 * Text.cs Modifier/Style/Span/Line/Text + Styles.cs). Puro, TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_TEXT_H
#define GMB_TUI_TEXT_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Modifier (specchio Text.cs Modifier, flags) ---- */
#define GMB_MOD_NONE        0x0000u
#define GMB_MOD_BOLD        0x0001u
#define GMB_MOD_DIM         0x0002u
#define GMB_MOD_ITALIC      0x0004u
#define GMB_MOD_UNDERLINED  0x0008u
#define GMB_MOD_SLOWBLINK   0x0010u
#define GMB_MOD_RAPIDBLINK  0x0020u
#define GMB_MOD_REVERSED    0x0040u
#define GMB_MOD_HIDDEN      0x0080u
#define GMB_MOD_CROSSEDOUT  0x0100u

typedef uint16_t gmb_modifier_t;

/* ---- Style (specchio Text.cs Style) ---- */
typedef struct gmb_style {
    bool has_fg;
    gmb_cell_color_t fg;
    bool has_bg;
    gmb_cell_color_t bg;
    gmb_modifier_t add_mod;
    gmb_modifier_t sub_mod;
} gmb_style_t;

gmb_style_t gmb_style_new(void);
gmb_style_t gmb_style_default(void);
gmb_style_t gmb_style_reset(void);
gmb_style_t gmb_style_with_fg(gmb_style_t s, gmb_cell_color_t color);
gmb_style_t gmb_style_with_bg(gmb_style_t s, gmb_cell_color_t color);
gmb_style_t gmb_style_with_add_modifier(gmb_style_t s, gmb_modifier_t m);
gmb_style_t gmb_style_with_remove_modifier(gmb_style_t s, gmb_modifier_t m);
gmb_style_t gmb_style_patch(gmb_style_t s, gmb_style_t other);
/* variante con puntatori (comoda per P/Invoke) */
void gmb_style_patch_ptr(const gmb_style_t* s, const gmb_style_t* other, gmb_style_t* out);
bool gmb_style_eq(gmb_style_t a, gmb_style_t b);

/* ---- Span / Line / Text (specchio Text.cs) ---- */
#define GMB_TEXT_MAX_SPANS   8
#define GMB_TEXT_MAX_LINES   16
#define GMB_TEXT_MAX_CONTENT 256

typedef struct gmb_span {
    char content[GMB_TEXT_MAX_CONTENT];
    gmb_style_t style;
} gmb_span_t;

typedef struct gmb_line {
    gmb_span_t spans[GMB_TEXT_MAX_SPANS];
    int span_count;
    gmb_style_t style;
} gmb_line_t;

typedef struct gmb_text {
    gmb_line_t lines[GMB_TEXT_MAX_LINES];
    int line_count;
    gmb_style_t style;
} gmb_text_t;

void gmb_line_raw(gmb_line_t* out, const char* content);
void gmb_line_styled(gmb_line_t* out, const char* content, gmb_style_t style);
gmb_line_t gmb_line_from_span(gmb_span_t span);
int gmb_line_width(const gmb_line_t* l);
int gmb_line_text(const gmb_line_t* l, char* out, int cap);
int gmb_line_style_at(const gmb_line_t* l, int column, gmb_style_t* out);

void gmb_text_raw(gmb_text_t* out, const char* content);
void gmb_text_styled(gmb_text_t* out, const char* content, gmb_style_t style);
void gmb_text_push(gmb_text_t* t, gmb_line_t line);
void gmb_text_append(gmb_text_t* t, const gmb_text_t* other);
int gmb_text_height(const gmb_text_t* t);
int gmb_text_width(const gmb_text_t* t);

/* ---- Styles (specchio Styles.cs, TTY-free builders) ---- */
int gmb_styles_style_code(const char* style);
int gmb_styles_set_style(const char* s, const char* style, char* out, int cap);
int gmb_styles_bold(const char* s, char* out, int cap);
int gmb_styles_dim(const char* s, char* out, int cap);
int gmb_styles_italic(const char* s, char* out, int cap);
int gmb_styles_underline(const char* s, char* out, int cap);
int gmb_styles_blink(const char* s, char* out, int cap);
int gmb_styles_reverse(const char* s, char* out, int cap);
int gmb_styles_hidden(const char* s, char* out, int cap);
int gmb_styles_set_fg_rgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap);
int gmb_styles_set_bg_rgb(const char* s, uint8_t r, uint8_t g, uint8_t b, char* out, int cap);
int gmb_styles_set_fg256(const char* s, uint8_t color, char* out, int cap);
int gmb_styles_set_bg256(const char* s, uint8_t color, char* out, int cap);
int gmb_styles_set_fg(const char* s, gmb_cell_color_t c, char* out, int cap);
int gmb_styles_set_bg(const char* s, gmb_cell_color_t c, char* out, int cap);
int gmb_styles_set_colors(const char* s, gmb_cell_color_t fg, gmb_cell_color_t bg, char* out, int cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_TEXT_H */
