/* ============================================================================
 * gmb_tui_imagewidget.c — ImageWidget.cs (specchio ClipperLib/Controls).
 * Control: Draw half-block con quantizzazione ANSI-16, HandleClick cella→pixel.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void imagewidget_draw(void* user, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_image_widget_t* w = (gmb_image_widget_t*)user;
    if (!w || w->img_w <= 0 || w->img_h <= 0 || c->width <= 0 || c->height <= 0) return;
    /* Grafica (way 3): i pixel dell'immagine come rettangoli nativi. Stesso controllo. */
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        int pxw = (c->width  * sk->cell_w) / w->img_w; if (pxw < 1) pxw = 1;
        int pxh = (c->height * sk->cell_h) / w->img_h; if (pxh < 1) pxh = 1;
        for (int py = 0; py < w->img_h; py++)
            for (int px = 0; px < w->img_w; px++)
            {
                gmb_cell_color_t col;
                if (!gmb_image_widget_pixel_at(w, px, py, &col)) continue;
                unsigned rgba = 0xFF000000u | ((unsigned)col.r << 16) | ((unsigned)col.g << 8) | col.b;
                gmb_gfx_fill_rounded_rect(sk->origin_x + c->x * sk->cell_w + px * pxw,
                                          sk->origin_y + c->y * sk->cell_h + py * pxh,
                                          pxw, pxh, 0, rgba);
            }
        return;
    }
    for (int cy = 0; cy < c->height; cy++)
    {
        int pyTop = cy * w->img_h / c->height;
        int pyBot = (cy * 2 + 1) * w->img_h / (c->height * 2);
        if (pyBot >= w->img_h) pyBot = w->img_h - 1;
        for (int cx = 0; cx < c->width; cx++)
        {
            int px = cx * w->img_w / c->width;
            gmb_cell_color_t top, bot;
            bool htop = gmb_image_widget_pixel_at(w, px, pyTop, &top);
            bool hbot = gmb_image_widget_pixel_at(w, px, pyBot, &bot);
            if (!htop && !hbot) continue;
            uint32_t ch;
            gmb_cell_color_t fg = { 255, 255, 255 }, bg = { 0, 0, 0 };
            if (htop && hbot)
            {
                ch = 0x2588; /* █ */
                fg = gmb_ansi16[gmb_quantize_to16(top)];
                bg = gmb_ansi16[gmb_quantize_to16(bot)];
            }
            else if (htop)
            {
                ch = 0x2580; /* ▀ */
                fg = gmb_ansi16[gmb_quantize_to16(top)];
            }
            else
            {
                ch = 0x2584; /* ▄ */
                bg = gmb_ansi16[gmb_quantize_to16(bot)];
            }
            int gx = c->x + cx, gy = c->y + cy;
            if (gx < 0 || gy < 0 || gx >= gmb_screen_width(buf) || gy >= gmb_screen_height(buf)) continue;
            gmb_screen_setchar(buf, gx, gy, ch, fg, bg);
        }
    }
}

/* C# ImageWidget.HandleClick: mappa cella→pixel, false fuori dall'immagine. */
static bool imagewidget_click(void* user, gmb_control_t* c, int x, int y)
{
    gmb_image_widget_t* w = (gmb_image_widget_t*)user;
    if (!w || w->img_w <= 0 || w->img_h <= 0 || c->width <= 0 || c->height <= 0) return false;
    int cx = x - c->x, cy = y - c->y;
    if (cx < 0 || cy < 0 || cx >= c->width || cy >= c->height) return false;
    w->last_pixel_x = cx * w->img_w / c->width;
    w->last_pixel_y = cy * w->img_h / c->height;
    return true;
}

void gmb_image_widget_init(gmb_image_widget_t* w)
{
    memset(w, 0, sizeof(*w));
    gmb_control_init(&w->ctrl, "", 0, 0, 0, 0, imagewidget_draw, NULL);
    w->ctrl.user = w;
    w->ctrl.handle_click_fn = imagewidget_click;
    w->last_click_x = -1; w->last_click_y = -1;
    w->last_pixel_x = -1; w->last_pixel_y = -1;
}

void gmb_image_widget_set_bounds(gmb_image_widget_t* w, int x, int y, int width, int height)
{
    w->ctrl.x = x; w->ctrl.y = y; w->ctrl.width = width; w->ctrl.height = height;
}

bool gmb_image_widget_set_pixels(gmb_image_widget_t* w, int wdt, int hgt, const gmb_cell_color_t* px)
{
    if (!w || !px || wdt <= 0 || hgt <= 0 || wdt * hgt > GMB_IW_MAX * GMB_IW_MAX) return false;
    w->img_w = wdt;
    w->img_h = hgt;
    memcpy(w->pixels, px, (size_t)(wdt * hgt) * sizeof(gmb_cell_color_t));
    w->has_pixels = true;
    return true;
}

bool gmb_image_widget_pixel_at(const gmb_image_widget_t* w, int x, int y, gmb_cell_color_t* out)
{
    if (!w->has_pixels || x < 0 || y < 0 || x >= w->img_w || y >= w->img_h) return false;
    if (out) *out = w->pixels[y * w->img_w + x];
    return true;
}

void gmb_image_widget_click(gmb_image_widget_t* w, int x, int y)
{
    w->last_click_x = x;
    w->last_click_y = y;
}

/* ---- 100% parity ImageWidget.cs:186 — RenderProtocol ---- */
const char* gmb_image_widget_render_protocol(const gmb_image_widget_t* w)
{
    if (!w) return "none";
    switch (w->protocol) {
        case 1: return "block";
        case 2: return "half-block";
        case 3: return "braille";
        default: return "none";
    }
}
int gmb_image_widget_last_click_x(const gmb_image_widget_t* w) { return w ? w->last_click_x : -1; }
int gmb_image_widget_last_click_y(const gmb_image_widget_t* w) { return w ? w->last_click_y : -1; }
int gmb_image_widget_last_pixel_x(const gmb_image_widget_t* w) { return w ? w->last_pixel_x : -1; }
int gmb_image_widget_last_pixel_y(const gmb_image_widget_t* w) { return w ? w->last_pixel_y : -1; }
