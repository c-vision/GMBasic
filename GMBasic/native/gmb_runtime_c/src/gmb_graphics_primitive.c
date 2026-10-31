/* gmb_graphics_primitive.c — primitive (estratto da gmb_graphics.c per evitare monolite) */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
extern float g_gfx_scale;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void line_core(int x1, int y1, int x2, int y2, int color, const char* style)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    gmb_gfx_scale_pt(&x1, &y1);
    gmb_gfx_scale_pt(&x2, &y2);
    uint32_t c = resolve_color(color < 0 ? g_gfx.fg : color);

    int fill_box = 0;
    if (style)
    {
        if (strstr(style, "BF")) fill_box = 2;
        else if (strstr(style, "B")) fill_box = 1;
    }
    if (fill_box)
    {
        int xa = x1 < x2 ? x1 : x2, xb = x1 < x2 ? x2 : x1;
        int ya = y1 < y2 ? y1 : y2, yb = y1 < y2 ? y2 : y1;
        if (fill_box == 2)
        {
            for (int y = ya; y <= yb; y++)
                for (int x = xa; x <= xb; x++)
                    put_pixel(p, x, y, c);
        }
        else
        {
            /* "B" = rettangolo con BORDI (4 lati), non riempito. */
            for (int x = xa; x <= xb; x++) { put_pixel(p, x, ya, c); put_pixel(p, x, yb, c); }
            for (int y = ya; y <= yb; y++) { put_pixel(p, xa, y, c); put_pixel(p, xb, y, c); }
        }
        return;
    }

    int dx = abs(x2 - x1), dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
        put_pixel(p, x1, y1, c);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
    g_gfx.cursor_x = x2;
    g_gfx.cursor_y = y2;
}

void gmb_gfx_line(int x1, int y1, int x2, int y2, int color, const char* style)
{
    if (x1 < 0) x1 = g_gfx.cursor_x;
    if (y1 < 0) y1 = g_gfx.cursor_y;
    gmb_gfx_map(&x1, &y1);
    gmb_gfx_map(&x2, &y2);
    line_core(x1, y1, x2, y2, color, style);
}

/* LINE con coordinate FRAZIONARIE (WINDOW): la mappatura world→viewport è fatta in
 * double, poi si arrotonda. Con PSET/LINE interi la curva y=sin(x) si schiacciava
 * (sin ∈ [-1,1] → int 0) — GraphicsWindows fase 2/3. */
void gmb_gfx_line_f(double x1, double y1, double x2, double y2, int color, const char* style)
{
    /* LINE -(...): start omesso = punto corrente (stesso sentinella -1 della variante int). */
    if (x1 < 0) x1 = g_gfx.cursor_x;
    if (y1 < 0) y1 = g_gfx.cursor_y;
    gmb_gfx_map_d(&x1, &y1);
    gmb_gfx_map_d(&x2, &y2);
    line_core((int)lround(x1), (int)lround(y1), (int)lround(x2), (int)lround(y2), color, style);
}

/* LINE relativa con coordinate frazionarie: da cursore → (x2,y2) mappato. */
void gmb_gfx_line_rel_f(double x2, double y2, int color, const char* style)
{
    gmb_gfx_map_d(&x2, &y2);
    line_core(g_gfx.cursor_x, g_gfx.cursor_y,
              (int)lround(x2), (int)lround(y2), color, style);
}

void gmb_gfx_circle(int x, int y, int radius, int color, int fill, double start, double end, double aspect)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    gmb_gfx_map(&x, &y);
    gmb_gfx_scale_pt(&x, &y);
    if (g_gfx_scale > 0.0f && g_gfx_scale != 1.0f) radius = (int)lround((double)radius * g_gfx_scale);
    uint32_t c = resolve_color(color < 0 ? g_gfx.fg : color);
    double asp = (aspect > 0.001) ? aspect : 1.0;
    if (radius <= 0) { put_pixel(p, x, y, c); return; }

    /* Unità: RADIANTI (come il motore C#: CircleInternal default 0..2π).
     * Cerchio intero se start==end==0 oppure se lo span è 2π (o multiplo). */
    int arc_only = 0;
    {
        double span = end - start;
        int full = (fabs(start) < 1e-9 && fabs(end) < 1e-9);
        if (!full && fabs(fabs(span) - 2*M_PI) < 1e-6) full = 1;
        if (!full && span != 0.0)
        {
            double turns = span / (2*M_PI);
            if (fabs(turns - (double)(long)(turns + (turns >= 0 ? 0.5 : -0.5))) < 1e-6) full = 1;
        }
        arc_only = !full;
    }
    double sa = start, ea = end;
    if (arc_only) {
        while (sa < 0) sa += 2*M_PI;
        while (ea < 0) ea += 2*M_PI;
        while (sa >= 2*M_PI) sa -= 2*M_PI;
        while (ea >= 2*M_PI) ea -= 2*M_PI;
    }
    int r2 = radius * radius;
    if (fill)
    {
        for (int yy = -radius; yy <= radius; yy++)
        {
            int half = (int)sqrt((double)(r2 - yy * yy));
            int yoff = (int)(yy / asp);
            for (int xx = -half; xx <= half; xx++) {
                if (arc_only) {
                    double ang = atan2((double)yy / asp, (double)xx);
                    if (ang < 0) ang += 2*M_PI;
                    int inside = (sa <= ea) ? (ang >= sa && ang <= ea) : (ang >= sa || ang <= ea);
                    if (!inside) continue;
                }
                put_pixel(p, x + xx, y + yoff, c);
            }
        }
    }
    else
    {
        for (int yy = -radius; yy <= radius; yy++)
        {
            int half = (int)sqrt((double)(r2 - yy * yy));
            int yoff = (int)(yy / asp);
            int pts[2][2] = {{half, yoff}, {-half, yoff}};
            for (int k = 0; k < 2; k++) {
                int xx = pts[k][0], yo = pts[k][1];
                if (arc_only) {
                    double ang = atan2((double)yo, (double)xx);
                    if (ang < 0) ang += 2*M_PI;
                    int inside = (sa <= ea) ? (ang >= sa && ang <= ea) : (ang >= sa || ang <= ea);
                    if (!inside) continue;
                }
                put_pixel(p, x + xx, y + yo, c);
            }
        }
        for (int xx = -radius; xx <= radius; xx++)
        {
            int half = (int)sqrt((double)(r2 - xx * xx));
            int pts[2][2] = {{xx, (int)(half / asp)}, {xx, (int)(-half / asp)}};
            for (int k = 0; k < 2; k++) {
                int xo = pts[k][0], yy2 = pts[k][1];
                if (arc_only) {
                    double ang = atan2((double)yy2, (double)xo);
                    if (ang < 0) ang += 2*M_PI;
                    int inside = (sa <= ea) ? (ang >= sa && ang <= ea) : (ang >= sa || ang <= ea);
                    if (!inside) continue;
                }
                put_pixel(p, x + xo, y + yy2, c);
            }
        }
    }
}

void gmb_gfx_paint(int x, int y, int fill_color, int border_color)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    gmb_gfx_map(&x, &y);
    gmb_gfx_scale_pt(&x, &y);
    if (x < 0 || y < 0 || x >= p->w || y >= p->h) return;
    uint32_t fill = resolve_color(fill_color < 0 ? g_gfx.fg : fill_color);
    (void)border_color;   /* il flood si ferma su QUALSIASI colore diverso dal seed:
                           * il contorno del cerchio è un muro, indipendentemente dal
                           * colore di bordo (prima border=bg=nero → il seed nero
                           * interno al cerchio veniva considerato "bordo" → PAINT
                           * non riempiva nulla, bug cerchio pieno). */
    uint32_t target = p->pixels[(size_t)y * p->w + x];
    if (target == fill) return;

    size_t n = (size_t)p->w * p->h;
    /* Ogni pixel può essere accodato fino a 4 volte (4-connettività): cap 4n, con
     * guardia per non sforare (prima n poteva traboccare su riempimenti grandi). */
    size_t cap = n * 4;
    uint32_t* stack = (uint32_t*)malloc(cap * sizeof(uint32_t));
    if (!stack) return;
    size_t sp = 0;
    stack[sp++] = (uint32_t)((size_t)y * p->w + x);
    while (sp > 0)
    {
        size_t idx = stack[--sp];
        if (p->pixels[idx] != target) continue;
        p->pixels[idx] = fill;
        int px = (int)(idx % p->w), py = (int)(idx / p->w);
        if (px > 0 && sp < cap) stack[sp++] = idx - 1;
        if (px < p->w - 1 && sp < cap) stack[sp++] = idx + 1;
        if (py > 0 && sp < cap) stack[sp++] = idx - (size_t)p->w;
        if (py < p->h - 1 && sp < cap) stack[sp++] = idx + (size_t)p->w;
    }
    free(stack);
}

/* DRAW "turtle": M<dx,dy>|M<dx>,<dy>, U/D/L/R[<n>], E/F/G/H[<n>], C<n>, P<fill>,<border>, B (pen up). */
void gmb_gfx_draw(const char* command)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    if (!command) return;
    int x = g_gfx.cursor_x, y = g_gfx.cursor_y;
    int color = g_gfx.fg;
    int pen_down = 1;
    const char* s = command;

    while (*s)
    {
        if (isspace((unsigned char)*s)) { s++; continue; }
        char cmd = (char)toupper((unsigned char)*s); s++;
        long n1 = 0, n2 = 0;
        int have1 = 0, have2 = 0;
        while (isdigit((unsigned char)*s) || *s == ' ') { if (isdigit((unsigned char)*s)) { n1 = n1 * 10 + (*s - '0'); have1 = 1; } s++; }
        if (*s == ',') { s++; while (isspace((unsigned char)*s)) s++; while (isdigit((unsigned char)*s)) { n2 = n2 * 10 + (*s - '0'); have2 = 1; s++; } }
        (void)have2;
        if (!have1 && cmd != 'B' && cmd != 'C' && cmd != 'P') n1 = 1;

        switch (cmd)
        {
            case 'M':
                if (have1 && have2) { x = (int)n1; y = (int)n2; }
                else if (have1) { x += (int)n1; }
                else if (*s == '+' || *s == '-') { int sign = (*s == '-') ? -1 : 1; s++; long dx = 0, dy = 0; int isd = 0; while (isdigit((unsigned char)*s)) { dx = dx * 10 + (*s - '0'); s++; isd = 1; } if (isd) dx *= sign; if (*s == ',') { s++; int sg2 = 1; if (*s == '-') { sg2 = -1; s++; } while (isdigit((unsigned char)*s)) { dy = dy * 10 + (*s - '0'); s++; } dy *= sg2; } if (pen_down) gmb_gfx_line(x, y, x + (int)dx, y + (int)dy, color, NULL); x += (int)dx; y += (int)dy; }
                break;
            case 'U': if (pen_down) gmb_gfx_line(x, y, x, y - (int)n1, color, NULL); y -= (int)n1; break;
            case 'D': if (pen_down) gmb_gfx_line(x, y, x, y + (int)n1, color, NULL); y += (int)n1; break;
            case 'L': if (pen_down) gmb_gfx_line(x, y, x - (int)n1, y, color, NULL); x -= (int)n1; break;
            case 'R': if (pen_down) gmb_gfx_line(x, y, x + (int)n1, y, color, NULL); x += (int)n1; break;
            case 'E': if (pen_down) gmb_gfx_line(x, y, x + (int)n1, y - (int)n1, color, NULL); x += (int)n1; y -= (int)n1; break;
            case 'F': if (pen_down) gmb_gfx_line(x, y, x + (int)n1, y + (int)n1, color, NULL); x += (int)n1; y += (int)n1; break;
            case 'G': if (pen_down) gmb_gfx_line(x, y, x - (int)n1, y + (int)n1, color, NULL); x -= (int)n1; y += (int)n1; break;
            case 'H': if (pen_down) gmb_gfx_line(x, y, x - (int)n1, y - (int)n1, color, NULL); x -= (int)n1; y -= (int)n1; break;
            case 'B': pen_down = 0; break;
            case 'N': pen_down = 1; break;
            case 'C': color = (int)n1; break;
            case 'P': gmb_gfx_paint(x, y, (int)n1, have2 ? (int)n2 : -1); break;
            default: break;
        }
    }
    g_gfx.cursor_x = x;
    g_gfx.cursor_y = y;
}

void gmb_gfx_polygon(const float* points, int count, int color)
{
    gmb_gfx_polygon_fill(points, count, color);
}

/* Testo spostato in gmb_graphics_text.c per evitare monolite */

/* Texture spostata in gmb_graphics_texture.c per evitare monolite */

/* Sprite spostato in gmb_graphics_sprite.c per evitare monolite */

/* ============================ Viewport / coordinate ============================ */

void gmb_gfx_view(int x1, int y1, int x2, int y2)
{
    gmb_gfx_init();
    if (x2 <= x1 || y2 <= y1)
    {
        /* VIEW (reset) → schermo intero. */
        g_gfx.view_set = 0;
        g_gfx.vx1 = 0; g_gfx.vy1 = 0; g_gfx.vx2 = g_gfx.width - 1; g_gfx.vy2 = g_gfx.height - 1;
        return;
    }
    g_gfx.view_set = 1;
    g_gfx.vx1 = x1; g_gfx.vy1 = y1; g_gfx.vx2 = x2; g_gfx.vy2 = y2;
}

void gmb_gfx_window(int x1, int y1, int x2, int y2)
{
    gmb_gfx_init();
    if (x2 <= x1 && y2 <= y1)
    {
        g_gfx.window_set = 0;
        return;
    }
    g_gfx.window_set = 1;
    g_gfx.wx1 = x1; g_gfx.wy1 = y1; g_gfx.wx2 = x2; g_gfx.wy2 = y2;
}

/* VIEW/WINDOW con limiti FRAZIONARI (es. VIEWPORT SCREEN (-4,-1.5)-(4,1.5)):
 * l'emitter C99Native passava (int) e -1.5/1.5 diventavano -1/1 (scala errata). */
void gmb_gfx_view_d(double x1, double y1, double x2, double y2)
{
    gmb_gfx_view((int)lround(x1), (int)lround(y1), (int)lround(x2), (int)lround(y2));
}

void gmb_gfx_window_d(double x1, double y1, double x2, double y2)
{
    gmb_gfx_init();
    if (x2 <= x1 && y2 <= y1)
    {
        g_gfx.window_set = 0;
        return;
    }
    g_gfx.window_set = 1;
    g_gfx.wx1 = x1; g_gfx.wy1 = y1; g_gfx.wx2 = x2; g_gfx.wy2 = y2;
}

/* VIEW color_sfondo [, bordo]: riempie il viewport e/o ne disegna il bordo
 * (parità C# ViewInternal). Usato da `VIEW (..)-(..), , 15` (GraphicsWindows). */
void gmb_gfx_view_style(int bg, int border)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || !g_gfx.view_set) return;
    float s = g_gfx_scale > 0.0f ? g_gfx_scale : 1.0f;
    int x1 = (int)lround((double)g_gfx.vx1 * s);
    int y1 = (int)lround((double)g_gfx.vy1 * s);
    int x2 = (int)lround((double)(g_gfx.vx2 + 1) * s) - 1;
    int y2 = (int)lround((double)(g_gfx.vy2 + 1) * s) - 1;
    int th = (int)lround((double)s); if (th < 1) th = 1;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= p->w) x2 = p->w - 1;
    if (y2 >= p->h) y2 = p->h - 1;
    if (x2 < x1 || y2 < y1) return;
    if (bg >= 0)
    {
        uint32_t c = resolve_color(bg);
        for (int y = y1; y <= y2; y++)
            for (int x = x1; x <= x2; x++)
                p->pixels[(size_t)y * p->w + x] = c;
    }
    if (border >= 0)
    {
        uint32_t c = resolve_color(border);
        for (int t = 0; t < th; t++)
        {
            int yy1 = y1 + t, yy2 = y2 - t;
            if (yy1 > yy2) break;
            for (int x = x1; x <= x2; x++) { p->pixels[(size_t)yy1 * p->w + x] = c; p->pixels[(size_t)yy2 * p->w + x] = c; }
        }
        for (int t = 0; t < th; t++)
        {
            int xx1 = x1 + t, xx2 = x2 - t;
            if (xx1 > xx2) break;
            for (int y = y1; y <= y2; y++) { p->pixels[(size_t)y * p->w + xx1] = c; p->pixels[(size_t)y * p->w + xx2] = c; }
        }
    }
}

/* ============================ Poligono pieno ============================ */

static int poly_intersection(const float* pts, int n, int y, double* xs, int* m)
{
    int k = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
    {
        int yi = (int)pts[i * 2 + 1], yj = (int)pts[j * 2 + 1];
        if ((yi <= y && yj > y) || (yj <= y && y > yi))
        {
            double xi = pts[i * 2], xj = pts[j * 2];
            xs[k++] = xi + (double)(y - yi) / (double)(yj - yi) * (xj - xi);
        }
    }
    /* ordina le intersezioni */
    for (int a = 0; a < k; a++)
        for (int b = a + 1; b < k; b++)
            if (xs[b] < xs[a]) { double t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
    *m = k;
    return k;
}

void gmb_gfx_polygon_fill(const float* points, int count, int color)
{
    gmb_gfx_page_t* p = active_page();
    if (!p) return;
    if (!points || count < 3) return;
    uint32_t c = resolve_color(color < 0 ? g_gfx.fg : color);
    int miny = (int)points[1], maxy = (int)points[1];
    for (int i = 1; i < count; i++)
    {
        int yy = (int)points[i * 2 + 1];
        if (yy < miny) miny = yy;
        if (yy > maxy) maxy = yy;
    }
    double xs[64];
    for (int y = miny; y <= maxy; y++)
    {
        int m = 0;
        if (poly_intersection(points, count, y, xs, &m) < 2) continue;
        for (int i = 0; i + 1 < m; i += 2)
        {
            int xa = (int)xs[i], xb = (int)xs[i + 1];
            for (int x = xa; x <= xb; x++) put_pixel(p, x, y, c);
        }
    }
}

/* ============================ Estensioni bridge ============================ */

void gmb_gfx_clear_rect(int x1, int y1, int x2, int y2, int color)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){x1=(int)lround((x1)*_S);y1=(int)lround((y1)*_S);x2=(int)lround((x2)*_S);y2=(int)lround((y2)*_S);} }
    gmb_gfx_page_t* p = active_page();
    uint32_t c;
    if (!p) return;
    c = resolve_color(color < 0 ? g_gfx.fg : color);
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= p->w) x2 = p->w - 1;
    if (y2 >= p->h) y2 = p->h - 1;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            p->pixels[(size_t)y * p->w + x] = c;
}

void gmb_gfx_tab_zone(void)
{
    int cw = gmb_gfx_text_width("M");
    int zone = 14;
    if (cw <= 0) cw = 8;
    g_gfx.cursor_x = ((g_gfx.cursor_x / (cw * zone)) + 1) * cw * zone;
}

/* ===================== Chrome pixel nativo (GfxSurface) =====================
 * Porting di CommonGraphics/Graphics/GfxWindowCanvas.cs (sc=1: nessuno scaling
 * nel motore C). rgba = 0xAARRGGBB; pixel pagina = 0x00RRGGBB. */
static void gmb_gfx_fill_rgb(gmb_gfx_page_t* p, int x1, int y1, int x2, int y2, unsigned int rgba)
{
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= p->w) x2 = p->w - 1;
    if (y2 >= p->h) y2 = p->h - 1;
    unsigned a = (rgba >> 24) & 0xFFu;
    if (a < 255u)   /* ALPHA: blend (prima veniva ignorata -> ombre nere piene) */
    {
        for (int y = y1; y <= y2; y++)
            for (int x = x1; x <= x2; x++)
                gmb_gfx_blend_pixel(x, y, rgba, 1.0);
        return;
    }
    unsigned int rgb = rgba & 0xFFFFFFu;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++) p->pixels[(size_t)y * p->w + x] = rgb;
}

void gmb_gfx_blend_pixel(int x, int y, unsigned int rgba, double coverage)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || x < 0 || y < 0 || x >= p->w || y >= p->h) return;
    double shape = ((rgba >> 24) & 0xFF) / 255.0;
    double eff = coverage * shape;
    if (eff <= 0.0) return;
    size_t idx = (size_t)y * p->w + x;
    if (eff >= 1.0) { p->pixels[idx] = rgba & 0xFFFFFFu; return; }
    unsigned int bg = p->pixels[idx];
    int bgR = (bg >> 16) & 0xFF, bgG = (bg >> 8) & 0xFF, bgB = bg & 0xFF;
    int fgR = (rgba >> 16) & 0xFF, fgG = (rgba >> 8) & 0xFF, fgB = rgba & 0xFF;
    int oR = (int)lround(fgR * eff + bgR * (1 - eff));
    int oG = (int)lround(fgG * eff + bgG * (1 - eff));
    int oB = (int)lround(fgB * eff + bgB * (1 - eff));
    if (oR < 0) oR = 0; if (oR > 255) oR = 255;
    if (oG < 0) oG = 0; if (oG > 255) oG = 255;
    if (oB < 0) oB = 0; if (oB > 255) oB = 255;
    p->pixels[idx] = ((unsigned)oR << 16) | ((unsigned)oG << 8) | (unsigned)oB;
}

void gmb_gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){x=(int)lround((x)*_S);y=(int)lround((y)*_S);w=(int)lround((w)*_S);h=(int)lround((h)*_S);radius=(int)lround((radius)*_S);} }
    gmb_gfx_page_t* p = active_page();
    if (!p || w <= 0 || h <= 0) return;
    if (radius <= 0) { gmb_gfx_fill_rgb(p, x, y, x + w - 1, y + h - 1, rgba); return; }
    int r = radius;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 1) r = 1;
    gmb_gfx_fill_rgb(p, x, y + r, x + w - 1, y + h - r - 1, rgba);
    gmb_gfx_fill_rgb(p, x + r, y, x + w - r - 1, y + h - 1, rgba);
    int cx[4] = { x + r, x + w - r - 1, x + r, x + w - r - 1 };
    int cy[4] = { y + r, y + r, y + h - r - 1, y + h - r - 1 };
    int bx[4] = { x, x + w - r, x, x + w - r };
    int by[4] = { y, y, y + h - r, y + h - r };
    for (int k = 0; k < 4; k++)
        for (int dy = 0; dy < r; dy++)
            for (int dx = 0; dx < r; dx++)
            {
                int px = bx[k] + dx, py = by[k] + dy;
                double ddx = px + 0.5 - cx[k], ddy = py + 0.5 - cy[k];
                double dist = sqrt(ddx * ddx + ddy * ddy);
                double cov = r + 0.5 - dist;
                if (cov <= 0) continue;
                if (cov > 1) cov = 1;
                gmb_gfx_blend_pixel(px, py, rgba, cov);
            }
}


/* Riempimento a gradiente VERTICALE dentro un rettangolo arrotondato (accenti sfumati:
 * neon/glass). Alpha rispettata (blend). */
void gmb_gfx_fill_rounded_gradient(int x, int y, int w, int h, int radius, unsigned int c1, unsigned int c2)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){x=(int)lround((x)*_S);y=(int)lround((y)*_S);w=(int)lround((w)*_S);h=(int)lround((h)*_S);radius=(int)lround((radius)*_S);} }
    gmb_gfx_page_t* p = active_page();
    if (!p || w <= 0 || h <= 0) return;
    int r = radius; if (r > w / 2) r = w / 2; if (r > h / 2) r = h / 2; if (r < 0) r = 0;
    int r1 = (int)(c1 >> 16) & 0xFF, g1 = (int)(c1 >> 8) & 0xFF, b1 = (int)c1 & 0xFF;
    int r2 = (int)(c2 >> 16) & 0xFF, g2 = (int)(c2 >> 8) & 0xFF, b2 = (int)c2 & 0xFF;
    unsigned a1 = (c1 >> 24) & 0xFFu, a2 = (c2 >> 24) & 0xFFu;
    for (int i = 0; i < h; i++)
    {
        int inset = 0;
        if (r > 0 && i < r)          { double d = (double)r - (i + 0.5); inset = r - (int)lround(sqrt((double)r * r - d * d)); }
        else if (r > 0 && i >= h - r) { double d = (double)r - (h - i - 0.5); inset = r - (int)lround(sqrt((double)r * r - d * d)); }
        if (inset < 0) inset = 0;
        double t = (h > 1) ? (double)i / (double)(h - 1) : 0.0;
        int rr = (int)lround(r1 + (r2 - r1) * t);
        int gg = (int)lround(g1 + (g2 - g1) * t);
        int bb = (int)lround(b1 + (b2 - b1) * t);
        unsigned aa = (unsigned)lround(a1 + ((double)a2 - (double)a1) * t);
        unsigned col = ((aa & 0xFFu) << 24) | ((unsigned)rr << 16) | ((unsigned)gg << 8) | (unsigned)bb;
        gmb_gfx_fill_rgb(p, x + inset, y + i, x + w - 1 - inset, y + i, col);
    }
}

void gmb_gfx_stroke_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){x=(int)lround((x)*_S);y=(int)lround((y)*_S);w=(int)lround((w)*_S);h=(int)lround((h)*_S);radius=(int)lround((radius)*_S);} }
    gmb_gfx_page_t* p = active_page();
    if (!p || w <= 0 || h <= 0) return;
    if (radius <= 0)
    {
        gmb_gfx_fill_rgb(p, x, y, x + w - 1, y, rgba);
        gmb_gfx_fill_rgb(p, x, y + h - 1, x + w - 1, y + h - 1, rgba);
        gmb_gfx_fill_rgb(p, x, y, x, y + h - 1, rgba);
        gmb_gfx_fill_rgb(p, x + w - 1, y, x + w - 1, y + h - 1, rgba);
        return;
    }
    int r = radius; if (r > w / 2) r = w / 2; if (r > h / 2) r = h / 2; if (r < 1) r = 1;
    gmb_gfx_fill_rgb(p, x + r, y, x + w - r - 1, y, rgba);
    gmb_gfx_fill_rgb(p, x + r, y + h - 1, x + w - r - 1, y + h - 1, rgba);
    gmb_gfx_fill_rgb(p, x, y + r, x, y + h - r - 1, rgba);
    gmb_gfx_fill_rgb(p, x + w - 1, y + r, x + w - 1, y + h - r - 1, rgba);
    int cx[4] = { x + r, x + w - r - 1, x + r, x + w - r - 1 };
    int cy[4] = { y + r, y + r, y + h - r - 1, y + h - r - 1 };
    int bx[4] = { x, x + w - r, x, x + w - r };
    int by[4] = { y, y, y + h - r, y + h - r };
    for (int k = 0; k < 4; k++)
        for (int dy = 0; dy < r; dy++)
            for (int dx = 0; dx < r; dx++)
            {
                int px = bx[k] + dx, py = by[k] + dy;
                double ddx = px + 0.5 - cx[k], ddy = py + 0.5 - cy[k];
                double dist = sqrt(ddx * ddx + ddy * ddy);
                double cov = 1.0 - fabs(dist - (r - 0.5));
                if (cov <= 0) continue;
                if (cov > 1) cov = 1;
                gmb_gfx_blend_pixel(px, py, rgba, cov);
            }
}

static unsigned int gmb_gfx_blend_rgb(unsigned int a, unsigned int b, double t)
{
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    int r = (int)lround(ar + (br - ar) * t);
    int g = (int)lround(ag + (bg - ag) * t);
    int bl = (int)lround(ab + (bb - ab) * t);
    return ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)bl;
}

void gmb_gfx_gradient_rect(int x, int y, int w, int h, unsigned int cstart, unsigned int cend, int vertical)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){x=(int)lround((x)*_S);y=(int)lround((y)*_S);w=(int)lround((w)*_S);h=(int)lround((h)*_S);} }
    gmb_gfx_page_t* p = active_page();
    if (!p || w <= 0 || h <= 0) return;
    int steps = vertical ? h : w;
    if (steps <= 0) return;
    for (int i = 0; i <= steps; i++)
    {
        double t = (double)i / steps;
        unsigned int c = gmb_gfx_blend_rgb(cstart, cend, t);
        if (vertical)
        {
            int yy = y + i;
            if (yy < 0 || yy >= p->h) continue;
            for (int fx = 0; fx < w; fx++) { int xx = x + fx; if (xx >= 0 && xx < p->w) p->pixels[(size_t)yy * p->w + xx] = c; }
        }
        else
        {
            int xx = x + i;
            if (xx < 0 || xx >= p->w) continue;
            for (int fy = 0; fy < h; fy++) { int yy = y + fy; if (yy >= 0 && yy < p->h) p->pixels[(size_t)yy * p->w + xx] = c; }
        }
    }
}


/* Ombra con contorno ARROTONDATO (SDF): niente alone quadrato dietro le card. */
void gmb_gfx_drop_shadow_rounded(int x, int y, int w, int h, int offset,
                                 unsigned int shadow_rgba, int corner, int blur)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){x=(int)lround((x)*_S);y=(int)lround((y)*_S);w=(int)lround((w)*_S);h=(int)lround((h)*_S);offset=(int)lround((offset)*_S);corner=(int)lround((corner)*_S);blur=(int)lround((blur)*_S);} }
    gmb_gfx_page_t* p = active_page();
    if (!p || w <= 0 || h <= 0) return;
    if (blur < 1) blur = 1;
    double cx = x + offset + w / 2.0, cy = y + offset + h / 2.0;
    double hx = w / 2.0, hy = h / 2.0;
    double R = corner; if (R > hx) R = hx; if (R > hy) R = hy; if (R < 0) R = 0;
    unsigned int baseA = (shadow_rgba >> 24) & 0xFF;
    unsigned int rgb = shadow_rgba & 0xFFFFFFu;
    int x0 = (int)(cx - hx - blur), x1 = (int)(cx + hx + blur) + 1;
    int y0 = (int)(cy - hy - blur), y1 = (int)(cy + hy + blur) + 1;
    for (int yy = y0; yy < y1; yy++) {
        if (yy < 0 || yy >= p->h) continue;
        for (int xx = x0; xx < x1; xx++) {
            if (xx < 0 || xx >= p->w) continue;
            double px = xx + 0.5 - cx, py = yy + 0.5 - cy;
            double qx = (px < 0 ? -px : px) - (hx - R);
            double qy = (py < 0 ? -py : py) - (hy - R);
            double mx = qx > 0 ? qx : 0, my = qy > 0 ? qy : 0;
            double d = sqrt(mx*mx + my*my) + (qx > qy ? qx : qy) - R;
            if (d <= 0) continue;                      /* sotto la card */
            double t = 1.0 - d / (double)blur;
            if (t <= 0) continue;
            unsigned int a = (unsigned int)(baseA * t);
            gmb_gfx_blend_pixel(xx, yy, rgb | (a << 24), 1.0);
        }
    }
}

void gmb_gfx_drop_shadow(int x, int y, int w, int h, int offset, unsigned int shadow_rgba, int radius)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){x=(int)lround((x)*_S);y=(int)lround((y)*_S);w=(int)lround((w)*_S);h=(int)lround((h)*_S);offset=(int)lround((offset)*_S);radius=(int)lround((radius)*_S);} }
    gmb_gfx_page_t* p = active_page();
    if (!p || w <= 0 || h <= 0) return;
    if (offset < 1) offset = 1;
    if (radius < 2) radius = 2;
    int x0 = x + offset, y0 = y + offset;
    unsigned int baseA = (shadow_rgba >> 24) & 0xFF;
    unsigned int rgb = shadow_rgba & 0xFFFFFFu;
    for (int yy = y0 - radius; yy < y0 + h + radius; yy++)
    {
        if (yy < 0 || yy >= p->h) continue;
        int dy = yy < y0 ? y0 - yy : (yy >= y0 + h ? yy - (y0 + h - 1) : 0);
        for (int xx = x0 - radius; xx < x0 + w + radius; xx++)
        {
            if (xx < 0 || xx >= p->w) continue;
            int dx = xx < x0 ? x0 - xx : (xx >= x0 + w ? xx - (x0 + w - 1) : 0);
            double d = sqrt((double)dx * dx + (double)dy * dy);
            if (d >= radius) continue;
            double t = 1 - d / radius;
            int a = (int)lround(baseA * (t * t * (3 - 2 * t)));
            if (a <= 0) continue;
            gmb_gfx_blend_pixel(xx, yy, rgb | ((unsigned)a << 24), 1.0);
        }
    }
}

/* Cerchio pieno anti-aliasing (porting GfxFilledCircle). cx,cy=centro, d=diametro, rgba=0xAARRGGBB. */
void gmb_gfx_fill_circle(int cx, int cy, int d, unsigned int rgba)
{
    { float _S=g_gfx_scale; if(_S!=1.0f){cx=(int)lround((cx)*_S);cy=(int)lround((cy)*_S);d=(int)lround((d)*_S);} }
    gmb_gfx_page_t* p = active_page();
    if (!p || d <= 0) return;
    double rN = d / 2.0;
    int x0 = (int)floor(cx - rN - 1), x1 = (int)ceil(cx + rN + 1);
    int y0 = (int)floor(cy - rN - 1), y1 = (int)ceil(cy + rN + 1);
    for (int py = y0; py <= y1; py++)
        for (int px = x0; px <= x1; px++)
        {
            double ddx = px + 0.5 - cx, ddy = py + 0.5 - cy;
            double dist = sqrt(ddx * ddx + ddy * ddy);
            double cov = rN + 0.5 - dist;
            if (cov <= 0) continue;
            if (cov > 1) cov = 1;
            gmb_gfx_blend_pixel(px, py, rgba, cov);
        }
}

/* COLLIDEBATCH(xs, ys, w, h): AABB broad+narrow su array di posizioni (parità
 * GraphicsManager.CollideBatch C#). Ritorna un buffer STATICO riusato
 * [i0,j0,i1,j1,...] di coppie di indici; *out_len = numero di elementi.
 * Le posizioni "fuori schermo" (>=1e5, sentinella dei morti) sono ignorate. */
double* gmb_gfx_collide_batch(const double* xs, int nx, const double* ys, int ny,
                              double w, double h, int* out_len)
{
    static double* buf = NULL;
    static int cap = 0;
    int k = 0;
    int n = nx < ny ? nx : ny;
    double halfW = (w > 0 ? w : 0) / 2.0, halfH = (h > 0 ? h : 0) / 2.0;
    if (out_len) *out_len = 0;
    if (!xs || !ys || n < 2) return buf;
    for (int i = 0; i < n; i++)
    {
        double ax = xs[i], ay = ys[i];
        if (!(ax > -1e5 && ax < 1e5 && ay > -1e5 && ay < 1e5)) continue;
        for (int j = i + 1; j < n; j++)
        {
            double bx = xs[j], by = ys[j];
            if (!(bx > -1e5 && bx < 1e5 && by > -1e5 && by < 1e5)) continue;
            if (ax + halfW >= bx - halfW && ax - halfW <= bx + halfW &&
                ay + halfH >= by - halfH && ay - halfH <= by + halfH)
            {
                if (k + 2 > cap) {
                    int nc = cap ? cap * 2 : 256;
                    double* nb = (double*)realloc(buf, (size_t)nc * sizeof(double));
                    if (!nb) { if (out_len) *out_len = k; return buf; }
                    buf = nb; cap = nc;
                }
                buf[k++] = (double)i;
                buf[k++] = (double)j;
            }
        }
    }
    if (out_len) *out_len = k;
    return buf;
}
