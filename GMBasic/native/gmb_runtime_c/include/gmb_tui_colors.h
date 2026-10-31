/* ============================================================================
 * gmb_tui_colors.h — Colors (specchio ClipperLib/Controls/Colors.cs):
 * Rgb, BlendMode, Gradient, ColorMath, NamedColors, ColorDetection. Puro.
 * ============================================================================ */
#ifndef GMB_TUI_COLORS_H
#define GMB_TUI_COLORS_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_GRADIENT_MAX_STOPS 16

typedef struct gmb_rgb {
    double r, g, b;
} gmb_rgb_t;

gmb_rgb_t gmb_rgb_from_cellcolor(gmb_cell_color_t c);
gmb_cell_color_t gmb_rgb_to_cellcolor(gmb_rgb_t c);
gmb_rgb_t gmb_rgb_from_u8(uint8_t r, uint8_t g, uint8_t b);
void gmb_rgb_to_u8(gmb_rgb_t c, uint8_t* r, uint8_t* g, uint8_t* b);

enum { GMB_BLEND_RGB = 0, GMB_BLEND_HSL };

typedef struct gmb_gradient {
    double pos[GMB_GRADIENT_MAX_STOPS];
    gmb_rgb_t color[GMB_GRADIENT_MAX_STOPS];
    int stops_count;
    int mode;
} gmb_gradient_t;

void gmb_gradient_from_colors(gmb_gradient_t* g, const gmb_cell_color_t* colors, int n, int mode);
void gmb_gradient_from_stops(gmb_gradient_t* g, const double* pos, const gmb_cell_color_t* colors, int n, int mode);
gmb_cell_color_t gmb_gradient_at(gmb_gradient_t* g, double t);
void gmb_gradient_samples(gmb_gradient_t* g, int n, gmb_cell_color_t* out);
void gmb_gradient_rainbow(gmb_gradient_t* g);
void gmb_gradient_turbo(gmb_gradient_t* g);
void gmb_gradient_viridis(gmb_gradient_t* g);
void gmb_gradient_plasma(gmb_gradient_t* g);
void gmb_gradient_heat(gmb_gradient_t* g);
void gmb_gradient_blues(gmb_gradient_t* g);
void gmb_gradient_grayscale(gmb_gradient_t* g);

/* ---- ColorMath ---- */
void gmb_colormath_rgb_to_hsl(gmb_rgb_t c, double* h, double* s, double* l);
gmb_rgb_t gmb_colormath_hsl_to_rgb(double h, double s, double l);
void gmb_colormath_rgb_to_hsv(gmb_rgb_t c, double* h, double* s, double* v);
gmb_rgb_t gmb_colormath_hsv_to_rgb(double h, double s, double v);
void gmb_colormath_rgb_to_cmyk(gmb_rgb_t c, double* cmyk_c, double* cmyk_m, double* cmyk_y, double* cmyk_k);
gmb_rgb_t gmb_colormath_cmyk_to_rgb(double c, double m, double y, double k);
gmb_rgb_t gmb_colormath_mix(gmb_rgb_t a, gmb_rgb_t b, double t);
gmb_rgb_t gmb_colormath_lighten(gmb_rgb_t c, double amt);
gmb_rgb_t gmb_colormath_darken(gmb_rgb_t c, double amt);
gmb_rgb_t gmb_colormath_saturate(gmb_rgb_t c, double amt);

/* ---- NamedColors ---- */
int gmb_named_colors_get(const char* name, gmb_cell_color_t* out);
int gmb_named_colors_get_rgb(const char* name, gmb_rgb_t* out);

/* ---- ColorDetection ---- */
enum { GMB_COLOR_NONE = 0, GMB_COLOR_ANSI16, GMB_COLOR_ANSI256, GMB_COLOR_TRUECOLOR };

int gmb_color_detection_from_env(const char* term, const char* colorterm, bool no_color, int force_color);
int gmb_color_detection_detect(void);
bool gmb_color_detection_supports_truecolor(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_COLORS_H */
