/* ============================================================================
 * gmb_tui_colors.c — Colors (porting Colors.cs). Puro.
 * ============================================================================ */
#include "gmb_tui_colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===================== Rgb ===================== */

gmb_rgb_t gmb_rgb_from_cellcolor(gmb_cell_color_t c)
{
    gmb_rgb_t r;
    if (c.is_default) { r.r = 0; r.g = 0; r.b = 0; return r; }
    r.r = c.r / 255.0;
    r.g = c.g / 255.0;
    r.b = c.b / 255.0;
    return r;
}

static uint8_t clamp255(double v)
{
    double clamped = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
    if (clamped <= 0.0) return 0;
    if (clamped >= 1.0) return 255;
    return (uint8_t)(clamped * 255.0 + 0.5);
}

gmb_cell_color_t gmb_rgb_to_cellcolor(gmb_rgb_t c)
{
    gmb_cell_color_t out;
    out.r = clamp255(c.r);
    out.g = clamp255(c.g);
    out.b = clamp255(c.b);
    out.is_default = false;
    return out;
}

gmb_rgb_t gmb_rgb_from_u8(uint8_t r, uint8_t g, uint8_t b)
{
    gmb_rgb_t out = { r / 255.0, g / 255.0, b / 255.0 };
    return out;
}

void gmb_rgb_to_u8(gmb_rgb_t c, uint8_t* r, uint8_t* g, uint8_t* b)
{
    if (r) *r = clamp255(c.r);
    if (g) *g = clamp255(c.g);
    if (b) *b = clamp255(c.b);
}

/* ===================== Gradient ===================== */

static void gradient_set_u8_stops(gmb_gradient_t* g, const double* pos, const uint8_t* rgb,
                                  int n, int mode)
{
    g->stops_count = n > GMB_GRADIENT_MAX_STOPS ? GMB_GRADIENT_MAX_STOPS : n;
    g->mode = mode;
    for (int i = 0; i < g->stops_count; i++)
    {
        g->pos[i] = pos[i];
        g->color[i] = gmb_rgb_from_u8(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    }
}

void gmb_gradient_from_colors(gmb_gradient_t* g, const gmb_cell_color_t* colors, int n, int mode)
{
    g->stops_count = 0;
    g->mode = mode;
    if (!colors || n <= 0) return;
    g->stops_count = n > GMB_GRADIENT_MAX_STOPS ? GMB_GRADIENT_MAX_STOPS : n;
    double nn = (double)n;
    for (int i = 0; i < g->stops_count; i++)
    {
        g->pos[i] = nn > 1 ? (double)i / (nn - 1.0) : 0.0;
        g->color[i] = gmb_rgb_from_cellcolor(colors[i]);
    }
}

void gmb_gradient_from_stops(gmb_gradient_t* g, const double* pos, const gmb_cell_color_t* colors, int n, int mode)
{
    g->stops_count = 0;
    g->mode = mode;
    if (!pos || !colors || n <= 0) return;
    g->stops_count = n > GMB_GRADIENT_MAX_STOPS ? GMB_GRADIENT_MAX_STOPS : n;
    for (int i = 0; i < g->stops_count; i++)
    {
        g->pos[i] = pos[i];
        g->color[i] = gmb_rgb_from_cellcolor(colors[i]);
    }
}

static gmb_rgb_t colormath_mix(gmb_rgb_t a, gmb_rgb_t b, double t);
static void colormath_rgb_to_hsl(gmb_rgb_t c, double* h, double* s, double* l);
static gmb_rgb_t colormath_hsl_to_rgb(double h, double s, double l);

static double shortest_hue_delta(double a, double b)
{
    double d = fmod(b - a, 360.0);
    if (d < 0) d += 360.0;
    return d > 180.0 ? d - 360.0 : d;
}

static gmb_rgb_t gradient_interpolate_hsl(gmb_rgb_t a, gmb_rgb_t b, double t)
{
    double ha, sa, la, hb, sb, lb;
    colormath_rgb_to_hsl(a, &ha, &sa, &la);
    colormath_rgb_to_hsl(b, &hb, &sb, &lb);
    double dh = shortest_hue_delta(ha, hb);
    return colormath_hsl_to_rgb(ha + dh * t, sa + (sb - sa) * t, la + (lb - la) * t);
}

gmb_cell_color_t gmb_gradient_at(gmb_gradient_t* g, double t)
{
    if (g->stops_count == 0) return GMB_CELLCOLOR_DEFAULT;
    double first_pos = g->pos[0];
    double last_pos = g->pos[g->stops_count - 1];
    if (t <= first_pos) return gmb_rgb_to_cellcolor(g->color[0]);
    if (t >= last_pos) return gmb_rgb_to_cellcolor(g->color[g->stops_count - 1]);
    int lo = 0, hi = g->stops_count;
    while (lo < hi)
    {
        int mid = (lo + hi) / 2;
        if (g->pos[mid] < t) lo = mid + 1;
        else hi = mid;
    }
    double pa = g->pos[lo - 1], pb = g->pos[lo];
    gmb_rgb_t ca = g->color[lo - 1], cb = g->color[lo];
    double local = fabs(pb - pa) < 1e-12 ? 0.0 : (t - pa) / (pb - pa);
    gmb_rgb_t result = (g->mode == GMB_BLEND_HSL)
        ? gradient_interpolate_hsl(ca, cb, local)
        : colormath_mix(ca, cb, local);
    return gmb_rgb_to_cellcolor(result);
}

void gmb_gradient_samples(gmb_gradient_t* g, int n, gmb_cell_color_t* out)
{
    if (n <= 0 || !out) return;
    for (int i = 0; i < n; i++)
        out[i] = gmb_gradient_at(g, n > 1 ? (double)i / (n - 1.0) : 0.0);
}

void gmb_gradient_rainbow(gmb_gradient_t* g)
{
    static const double pos[] = { 0.0, 0.166, 0.333, 0.5, 0.666, 0.833, 1.0 };
    static const uint8_t rgb[] = {
        255,0,0, 255,255,0, 0,255,0, 0,255,255, 0,0,255, 255,0,255, 255,0,0
    };
    gradient_set_u8_stops(g, pos, rgb, 7, GMB_BLEND_HSL);
}

void gmb_gradient_turbo(gmb_gradient_t* g)
{
    static const double pos[] = { 0.0, 0.13, 0.25, 0.38, 0.5, 0.62, 0.75, 0.88, 1.0 };
    static const uint8_t rgb[] = {
        48,18,59, 70,107,227, 40,174,229, 53,224,153, 147,232,58, 221,203,54, 249,133,38, 209,54,21, 122,4,3
    };
    gradient_set_u8_stops(g, pos, rgb, 9, GMB_BLEND_RGB);
}

void gmb_gradient_viridis(gmb_gradient_t* g)
{
    static const double pos[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
    static const uint8_t rgb[] = { 68,1,84, 59,82,139, 33,145,140, 94,201,98, 253,231,37 };
    gradient_set_u8_stops(g, pos, rgb, 5, GMB_BLEND_RGB);
}

void gmb_gradient_plasma(gmb_gradient_t* g)
{
    static const double pos[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
    static const uint8_t rgb[] = { 13,8,135, 126,3,168, 204,71,120, 248,149,64, 240,249,33 };
    gradient_set_u8_stops(g, pos, rgb, 5, GMB_BLEND_RGB);
}

void gmb_gradient_heat(gmb_gradient_t* g)
{
    static const double pos[] = { 0.0, 0.33, 0.66, 1.0 };
    static const uint8_t rgb[] = { 0,0,0, 178,34,34, 255,165,0, 255,255,255 };
    gradient_set_u8_stops(g, pos, rgb, 4, GMB_BLEND_RGB);
}

void gmb_gradient_blues(gmb_gradient_t* g)
{
    static const double pos[] = { 0.0, 0.5, 1.0 };
    static const uint8_t rgb[] = { 8,29,88, 43,131,186, 199,233,180 };
    gradient_set_u8_stops(g, pos, rgb, 3, GMB_BLEND_RGB);
}

void gmb_gradient_grayscale(gmb_gradient_t* g)
{
    static const double pos[] = { 0.0, 1.0 };
    static const uint8_t rgb[] = { 0,0,0, 255,255,255 };
    gradient_set_u8_stops(g, pos, rgb, 2, GMB_BLEND_RGB);
}

/* ===================== ColorMath ===================== */

static void colormath_rgb_to_hsl(gmb_rgb_t c, double* h, double* s, double* l)
{
    double min = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
    double max = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
    double chroma = max - min;
    double ll = (max + min) / 2.0;
    if (chroma < 1e-12) { *h = 0.0; *s = 0.0; *l = ll; return; }
    double hh;
    if (fabs(max - c.r) < 1e-12) hh = fmod((c.g - c.b) / chroma, 6.0);
    else if (fabs(max - c.g) < 1e-12) hh = (c.b - c.r) / chroma + 2.0;
    else hh = (c.r - c.g) / chroma + 4.0;
    hh *= 60.0;
    if (hh < 0.0) hh += 360.0;
    double ss = ll < 1.0 ? chroma / (1.0 - fabs(2.0 * ll - 1.0)) : 0.0;
    *h = hh;
    *s = ss;
    *l = ll;
}

static gmb_rgb_t colormath_hsl_to_rgb(double h, double s, double l)
{
    h = fmod(h, 360.0);
    if (h < 0) h += 360.0;
    double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = l - c / 2.0;
    double r, g, b;
    if (h < 60.0) { r = c; g = x; b = 0.0; }
    else if (h < 120.0) { r = x; g = c; b = 0.0; }
    else if (h < 180.0) { r = 0.0; g = c; b = x; }
    else if (h < 240.0) { r = 0.0; g = x; b = c; }
    else if (h < 300.0) { r = x; g = 0.0; b = c; }
    else { r = c; g = 0.0; b = x; }
    gmb_rgb_t out = { r + m, g + m, b + m };
    return out;
}

void gmb_colormath_rgb_to_hsl(gmb_rgb_t c, double* h, double* s, double* l) { colormath_rgb_to_hsl(c, h, s, l); }
gmb_rgb_t gmb_colormath_hsl_to_rgb(double h, double s, double l) { return colormath_hsl_to_rgb(h, s, l); }

void gmb_colormath_rgb_to_hsv(gmb_rgb_t c, double* h, double* s, double* v)
{
    double min = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
    double max = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
    double delta = max - min;
    double vv = max;
    double ss = max > 1e-6 ? delta / max : 0.0;
    double hh = 0.0;
    if (delta > 1e-12)
    {
        if (fabs(max - c.r) < 1e-12) hh = (c.g - c.b) / delta;
        else if (fabs(max - c.g) < 1e-12) hh = 2.0 + (c.b - c.r) / delta;
        else hh = 4.0 + (c.r - c.g) / delta;
        hh *= 60.0;
    }
    if (hh < 0.0) hh += 360.0;
    *h = hh;
    *s = ss;
    *v = vv;
}

gmb_rgb_t gmb_colormath_hsv_to_rgb(double h, double s, double v)
{
    h = fmod(h, 360.0);
    if (h < 0) h += 360.0;
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    double r, g, b;
    if (h < 60.0) { r = c; g = x; b = 0.0; }
    else if (h < 120.0) { r = x; g = c; b = 0.0; }
    else if (h < 180.0) { r = 0.0; g = c; b = x; }
    else if (h < 240.0) { r = 0.0; g = x; b = c; }
    else if (h < 300.0) { r = x; g = 0.0; b = c; }
    else { r = c; g = 0.0; b = x; }
    gmb_rgb_t out = { r + m, g + m, b + m };
    return out;
}

void gmb_colormath_rgb_to_cmyk(gmb_rgb_t c, double* cc, double* cm, double* cy, double* ck)
{
    double k = 1.0 - (c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b));
    if (k >= 1.0) { *cc = 0; *cm = 0; *cy = 0; *ck = 1.0; return; }
    *cc = (1.0 - c.r - k) / (1.0 - k);
    *cm = (1.0 - c.g - k) / (1.0 - k);
    *cy = (1.0 - c.b - k) / (1.0 - k);
    *ck = k;
}

gmb_rgb_t gmb_colormath_cmyk_to_rgb(double c, double m, double y, double k)
{
    gmb_rgb_t out = { (1.0 - c) * (1.0 - k), (1.0 - m) * (1.0 - k), (1.0 - y) * (1.0 - k) };
    return out;
}

static gmb_rgb_t colormath_mix(gmb_rgb_t a, gmb_rgb_t b, double t)
{
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    gmb_rgb_t out = { a.r + t * (b.r - a.r), a.g + t * (b.g - a.g), a.b + t * (b.b - a.b) };
    return out;
}

gmb_rgb_t gmb_colormath_mix(gmb_rgb_t a, gmb_rgb_t b, double t) { return colormath_mix(a, b, t); }

gmb_rgb_t gmb_colormath_lighten(gmb_rgb_t c, double amt)
{
    double h, s, l;
    colormath_rgb_to_hsl(c, &h, &s, &l);
    double nl = l + amt;
    if (nl < 0.0) nl = 0.0;
    if (nl > 1.0) nl = 1.0;
    return colormath_hsl_to_rgb(h, s, nl);
}

gmb_rgb_t gmb_colormath_darken(gmb_rgb_t c, double amt) { return gmb_colormath_lighten(c, -amt); }

gmb_rgb_t gmb_colormath_saturate(gmb_rgb_t c, double amt)
{
    double h, s, l;
    colormath_rgb_to_hsl(c, &h, &s, &l);
    double ns = s + amt;
    if (ns < 0.0) ns = 0.0;
    if (ns > 1.0) ns = 1.0;
    return colormath_hsl_to_rgb(h, ns, l);
}

/* ===================== NamedColors ===================== */

typedef struct named_color { const char* name; uint8_t r, g, b; } named_color_t;

static const named_color_t NAMED_COLORS[] = {
    {"aliceblue",240,248,255},{"antiquewhite",250,235,215},{"aqua",0,255,255},
    {"aquamarine",127,255,212},{"azure",240,255,255},{"beige",245,245,220},
    {"bisque",255,228,196},{"black",0,0,0},{"blanchedalmond",255,235,205},
    {"blue",0,0,255},{"blueviolet",138,43,226},{"brown",165,42,42},
    {"burlywood",222,184,135},{"cadetblue",95,158,160},{"chartreuse",127,255,0},
    {"chocolate",210,105,30},{"coral",255,127,80},{"cornflowerblue",100,149,237},
    {"cornsilk",255,248,220},{"crimson",220,20,60},{"cyan",0,255,255},
    {"darkblue",0,0,139},{"darkcyan",0,139,139},{"darkgoldenrod",184,134,11},
    {"darkgray",169,169,169},{"darkgreen",0,100,0},{"darkgrey",169,169,169},
    {"darkkhaki",189,183,107},{"darkmagenta",139,0,139},{"darkolivegreen",85,107,47},
    {"darkorange",255,140,0},{"darkorchid",153,50,204},{"darkred",139,0,0},
    {"darksalmon",233,150,122},{"darkseagreen",143,188,143},{"darkslateblue",72,61,139},
    {"darkslategray",47,79,79},{"darkslategrey",47,79,79},{"darkturquoise",0,206,209},
    {"darkviolet",148,0,211},{"deeppink",255,20,147},{"deepskyblue",0,191,255},
    {"dimgray",105,105,105},{"dimgrey",105,105,105},{"dodgerblue",30,144,255},
    {"firebrick",178,34,34},{"floralwhite",255,250,240},{"forestgreen",34,139,34},
    {"fuchsia",255,0,255},{"gainsboro",220,220,220},{"ghostwhite",248,248,255},
    {"gold",255,215,0},{"goldenrod",218,165,32},{"gray",128,128,128},
    {"green",0,128,0},{"greenyellow",173,255,47},{"grey",128,128,128},
    {"honeydew",240,255,240},{"hotpink",255,105,180},{"indianred",205,92,92},
    {"indigo",75,0,130},{"ivory",255,255,240},{"khaki",240,230,140},
    {"lavender",230,230,250},{"lavenderblush",255,240,245},{"lawngreen",124,252,0},
    {"lemonchiffon",255,250,205},{"lightblue",173,216,230},{"lightcoral",240,128,128},
    {"lightcyan",224,255,255},{"lightgoldenrodyellow",250,250,210},{"lightgray",211,211,211},
    {"lightgreen",144,238,144},{"lightgrey",211,211,211},{"lightpink",255,182,193},
    {"lightsalmon",255,160,122},{"lightseagreen",32,178,170},{"lightskyblue",135,206,250},
    {"lightslategray",119,136,153},{"lightslategrey",119,136,153},{"lightsteelblue",176,196,222},
    {"lightyellow",255,255,224},{"lime",0,255,0},{"limegreen",50,205,50},
    {"linen",250,240,230},{"magenta",255,0,255},{"maroon",128,0,0},
    {"mediumaquamarine",102,205,170},{"mediumblue",0,0,205},{"mediumorchid",186,85,211},
    {"mediumpurple",147,112,219},{"mediumseagreen",60,179,113},{"mediumslateblue",123,104,238},
    {"mediumspringgreen",0,250,154},{"mediumturquoise",72,209,204},{"mediumvioletred",199,21,133},
    {"midnightblue",25,25,112},{"mintcream",245,255,250},{"mistyrose",255,228,225},
    {"moccasin",255,228,181},{"navajowhite",255,222,173},{"navy",0,0,128},
    {"oldlace",253,245,230},{"olive",128,128,0},{"olivedrab",107,142,35},
    {"orange",255,165,0},{"orangered",255,69,0},{"orchid",218,112,214},
    {"palegoldenrod",238,232,170},{"palegreen",152,251,152},{"paleturquoise",175,238,238},
    {"palevioletred",219,112,147},{"papayawhip",255,239,213},{"peachpuff",255,218,185},
    {"peru",205,133,63},{"pink",255,192,203},{"plum",221,160,221},
    {"powderblue",176,224,230},{"purple",128,0,128},{"rebeccapurple",102,51,153},
    {"red",255,0,0},{"rosybrown",188,143,143},{"royalblue",65,105,225},
    {"saddlebrown",139,69,19},{"salmon",250,128,114},{"sandybrown",244,164,96},
    {"seagreen",46,139,87},{"seashell",255,245,238},{"sienna",160,82,45},
    {"silver",192,192,192},{"skyblue",135,206,235},{"slateblue",106,90,205},
    {"slategray",112,128,144},{"slategrey",112,128,144},{"snow",255,250,250},
    {"springgreen",0,255,127},{"steelblue",70,130,180},{"tan",210,180,140},
    {"teal",0,128,128},{"thistle",216,191,216},{"tomato",255,99,71},
    {"turquoise",64,224,208},{"violet",238,130,238},{"wheat",245,222,179},
    {"white",255,255,255},{"whitesmoke",245,245,245},{"yellow",255,255,0},
    {"yellowgreen",154,205,50}, {NULL,0,0,0}
};

int gmb_named_colors_get(const char* name, gmb_cell_color_t* out)
{
    if (!name || !out) return 0;
    char n[64];
    snprintf(n, sizeof(n), "%s", name);
    for (char* p = n; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    for (int i = 0; NAMED_COLORS[i].name; i++)
    {
        if (strcmp(n, NAMED_COLORS[i].name) == 0)
        {
            out->r = NAMED_COLORS[i].r;
            out->g = NAMED_COLORS[i].g;
            out->b = NAMED_COLORS[i].b;
            out->is_default = false;
            return 1;
        }
    }
    return 0;
}

int gmb_named_colors_get_rgb(const char* name, gmb_rgb_t* out)
{
    gmb_cell_color_t c;
    if (!gmb_named_colors_get(name, &c)) return 0;
    if (out) *out = gmb_rgb_from_cellcolor(c);
    return 1;
}

/* ===================== ColorDetection ===================== */

int gmb_color_detection_from_env(const char* term, const char* colorterm, bool no_color, int force_color)
{
    if (no_color && force_color == 0) return GMB_COLOR_NONE;
    if (force_color >= 3) return GMB_COLOR_TRUECOLOR;
    if (colorterm)
    {
        if (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0)
            return GMB_COLOR_TRUECOLOR;
    }
    if (term)
    {
        int tl = (int)strlen(term);
        if ((tl >= 6 && strcmp(term + tl - 6, "direct") == 0) || (tl >= 9 && strcmp(term + tl - 9, "truecolor") == 0))
            return GMB_COLOR_TRUECOLOR;
        if (strstr(term, "256color") || colorterm)
            return GMB_COLOR_ANSI256;
        if (strstr(term, "color"))
            return GMB_COLOR_ANSI16;
    }
    if (colorterm) return GMB_COLOR_ANSI256;
    if (force_color > 0) return GMB_COLOR_ANSI16;
    return GMB_COLOR_NONE;
}

int gmb_color_detection_detect(void)
{
    const char* term = getenv("TERM");
    const char* colorterm = getenv("COLORTERM");
    const char* nc = getenv("NO_COLOR");
    bool no_color = nc && nc[0] != '\0';
    int force_color = 0;
    const char* fc = getenv("FORCE_COLOR");
    if (fc) force_color = atoi(fc);
    return gmb_color_detection_from_env(term, colorterm, no_color, force_color);
}

bool gmb_color_detection_supports_truecolor(void)
{
    return gmb_color_detection_detect() == GMB_COLOR_TRUECOLOR;
}
