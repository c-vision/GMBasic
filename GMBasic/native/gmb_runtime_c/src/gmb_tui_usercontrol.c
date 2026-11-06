/* gmb_tui_usercontrol.c — Controlli utente (UserControl, stile VB6).
 *
 * Host unico gmb_control_t: il metodo BASIC Paint (eseguito sul WORKER GPM) riceve un
 * gmb_gfx_context_t che e' un SINK registratore: le primitive accumulate come draw-list
 * NON chiamano SDL. Il main thread replaya la lista (gmb_gfxctx_replay) durante il present.
 * Cosi' il thread SDL non esegue mai BASIC. Vedi
 * AiWiki/01-Projects/GMBasic/Decisions/ADR-Controlli-Utente-Modello-Classe-VB6.md (§7).
 *
 * Coordinate del contesto = LOCALI al controllo (0..width / 0..height); il replay somma
 * l'origine del controllo sullo schermo. */
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include "gmb_graphics.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- GraphicsContext (sink) ---- */

void gmb_gfxctx_begin(gmb_gfx_context_t* g, int x, int y, int w, int h, unsigned fg, unsigned bg, int focused)
{
    memset(g, 0, sizeof(*g));
    g->x = x; g->y = y; g->width = w; g->height = h;
    g->fg = fg; g->bg = bg; g->cur = fg; g->focused = focused;
}

static gmb_gfx_cmd_t* px_push(gmb_gfx_context_t* g, int op)
{
    if (g->ncmds >= GMB_GFXCTX_MAX_CMDS) return NULL;
    gmb_gfx_cmd_t* c = &g->cmds[g->ncmds++];
    memset(c, 0, sizeof(*c));
    c->op = op; c->color = g->cur;
    return c;
}

void gmb_gfxctx_clear(gmb_gfx_context_t* g) { gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_CLEAR); (void)c; }
void gmb_gfxctx_setcolor(gmb_gfx_context_t* g, unsigned color) { g->cur = color; }

void gmb_gfxctx_line(gmb_gfx_context_t* g, int x1, int y1, int x2, int y2)
{ gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_LINE); if (c) { c->a = x1; c->b = y1; c->c = x2; c->d = y2; } }

void gmb_gfxctx_rect(gmb_gfx_context_t* g, int x, int y, int w, int h)
{ gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_RECT); if (c) { c->a = x; c->b = y; c->c = w; c->d = h; } }

void gmb_gfxctx_fillrect(gmb_gfx_context_t* g, int x, int y, int w, int h)
{ gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_FILLRECT); if (c) { c->a = x; c->b = y; c->c = w; c->d = h; } }

void gmb_gfxctx_circle(gmb_gfx_context_t* g, int cx, int cy, int r)
{ gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_CIRCLE); if (c) { c->a = cx; c->b = cy; c->c = r; } }

void gmb_gfxctx_fillcircle(gmb_gfx_context_t* g, int cx, int cy, int r)
{ gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_FILLCIRCLE); if (c) { c->a = cx; c->b = cy; c->c = r; } }

void gmb_gfxctx_text(gmb_gfx_context_t* g, const char* s, int x, int y)
{ gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_TEXT); if (c) { c->a = x; c->b = y; snprintf(c->text, sizeof(c->text), "%s", s ? s : ""); } }

void gmb_gfxctx_plot(gmb_gfx_context_t* g, int x, int y)
{ gmb_gfx_cmd_t* c = px_push(g, GMB_GCMD_PLOT); if (c) { c->a = x; c->b = y; } }

/* Replay sul main thread: chiama le primitive gmb_gfx_* reali. */
void gmb_gfxctx_replay(const gmb_gfx_context_t* g)
{
    int ox = g->x, oy = g->y;
    for (int i = 0; i < g->ncmds; i++)
    {
        const gmb_gfx_cmd_t* c = &g->cmds[i];
        switch (c->op)
        {
            case GMB_GCMD_CLEAR:      gmb_gfx_fill_rounded_rect(ox, oy, g->width, g->height, 0, g->bg); break;
            case GMB_GCMD_LINE:       gmb_gfx_line(ox + c->a, oy + c->b, ox + c->c, oy + c->d, (int)c->color, ""); break;
            case GMB_GCMD_RECT:       gmb_gfx_stroke_rounded_rect(ox + c->a, oy + c->b, c->c, c->d, 0, c->color); break;
            case GMB_GCMD_FILLRECT:   gmb_gfx_fill_rounded_rect(ox + c->a, oy + c->b, c->c, c->d, 0, c->color); break;
            case GMB_GCMD_CIRCLE:     gmb_gfx_circle(ox + c->a, oy + c->b, c->c, (int)c->color, 0, 0.0, 360.0, 1.0); break;
            case GMB_GCMD_FILLCIRCLE: gmb_gfx_fill_circle(ox + c->a, oy + c->b, c->c * 2, c->color); break;
            case GMB_GCMD_TEXT:       gmb_gfx_draw_text(c->text, ox + c->a, oy + c->b, 0, (int)c->color); break;
            case GMB_GCMD_PLOT:       gmb_gfx_line(ox + c->a, oy + c->b, ox + c->a, oy + c->b, (int)c->color, ""); break;
            default: break;
        }
    }
}

/* ---- GraphicsContext per HANDLE (API per i backend C) ----
 * I backend C compilano `Paint(g AS ANY)` come funzione C, ma i metodi dell'oggetto
 * GraphicsContext non hanno dispatch nativo sui C. Si espone il contesto per HANDLE:
 * Paint riceve un intero; le primitive sono funzioni runtime che prendono l'handle.
 * Il draw-list continua ad accumularsi nel gmb_gfx_context_t (replay sul main thread). */
#define GMB_GFXCTX_TABLE 8
static gmb_gfx_context_t* g_gfxctxs[GMB_GFXCTX_TABLE];
int gmb_gfxctx_open(gmb_gfx_context_t* g)
{ if (!g) return 0; for (int i = 0; i < GMB_GFXCTX_TABLE; i++) if (!g_gfxctxs[i]) { g_gfxctxs[i] = g; return i + 1; } return 0; }
void gmb_gfxctx_close(int h) { if (h > 0 && h <= GMB_GFXCTX_TABLE) g_gfxctxs[h - 1] = NULL; }
static gmb_gfx_context_t* h2g(int h) { return (h > 0 && h <= GMB_GFXCTX_TABLE) ? g_gfxctxs[h - 1] : NULL; }
int  gmb_gfxctx_h_width(int h)  { gmb_gfx_context_t* g = h2g(h); return g ? g->width : 0; }
int  gmb_gfxctx_h_height(int h) { gmb_gfx_context_t* g = h2g(h); return g ? g->height : 0; }
void gmb_gfxctx_h_clear(int h)  { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_clear(g); }
void gmb_gfxctx_h_setcolor(int h, double c) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_setcolor(g, (unsigned)c); }
void gmb_gfxctx_h_line(int h, double x1, double y1, double x2, double y2) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_line(g, (int)x1, (int)y1, (int)x2, (int)y2); }
void gmb_gfxctx_h_rect(int h, double x, double y, double w, double hh) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_rect(g, (int)x, (int)y, (int)w, (int)hh); }
void gmb_gfxctx_h_fillrect(int h, double x, double y, double w, double hh) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_fillrect(g, (int)x, (int)y, (int)w, (int)hh); }
void gmb_gfxctx_h_circle(int h, double cx, double cy, double r) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_circle(g, (int)cx, (int)cy, (int)r); }
void gmb_gfxctx_h_fillcircle(int h, double cx, double cy, double r) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_fillcircle(g, (int)cx, (int)cy, (int)r); }
void gmb_gfxctx_h_text(int h, const char* s, double x, double y) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_text(g, s, (int)x, (int)y); }
void gmb_gfxctx_h_plot(int h, double x, double y) { gmb_gfx_context_t* g = h2g(h); if (g) gmb_gfxctx_plot(g, (int)x, (int)y); }
/* Dispatcher per op (CONTROL DRAW): 0=clear 1=setcolor 2=line 3=rect 4=fillrect 5=circle
 * 6=fillcircle 7=text 8=plot. */
void gmb_gfxctx_h_op(int h, int op, double a, double b, double c, double d, const char* s)
{
    switch (op)
    {
        case 0: gmb_gfxctx_h_clear(h); break;
        case 1: gmb_gfxctx_h_setcolor(h, a); break;
        case 2: gmb_gfxctx_h_line(h, a, b, c, d); break;
        case 3: gmb_gfxctx_h_rect(h, a, b, c, d); break;
        case 4: gmb_gfxctx_h_fillrect(h, a, b, c, d); break;
        case 5: gmb_gfxctx_h_circle(h, a, b, c); break;
        case 6: gmb_gfxctx_h_fillcircle(h, a, b, c); break;
        case 7: gmb_gfxctx_h_text(h, s, a, b); break;
        case 8: gmb_gfxctx_h_plot(h, a, b); break;
    }
}

/* ---- Host UserControl ---- */

static void usercontrol_draw_px(void* u, gmb_control_t* c)
{
    gmb_usercontrol_t* uc = (gmb_usercontrol_t*)u;
    gmb_widget_style_t st = gmb_widget_style_current();
    /* Sink: coordinate LOCALI; origine schermo = rect del controllo. */
    gmb_gfxctx_begin(&uc->ctx, c->x, c->y, c->width, c->height, st.text, st.fill, c->focused);
    if (uc->paint) uc->paint(uc->instance, &uc->ctx);
    gmb_gfxctx_replay(&uc->ctx);
}

/* TEXT: per P0 il controllo utente e' pixel-only (no-op a celle). */
static void usercontrol_draw_txt(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    (void)u; (void)c; (void)buf;
}

void gmb_usercontrol_init(gmb_usercontrol_t* u, int x, int y, int w, int h,
                          void (*paint)(void*, gmb_gfx_context_t*), void* instance,
                          const char* container, const char* name)
{
    memset(u, 0, sizeof(*u));
    memset(u, 0, sizeof(*u));
    gmb_control_init(&u->ctrl, "", x, y, w, h, usercontrol_draw_txt, NULL);
    u->ctrl.user = u;
    u->ctrl.draw_px_fn = usercontrol_draw_px;
    u->instance = instance;
    u->paint = paint;
    snprintf(u->container, sizeof(u->container), "%s", container ? container : "");
    snprintf(u->ctrl.container, sizeof(u->ctrl.container), "%s", container ? container : "");
    if (name && *name) snprintf(u->ctrl.name, sizeof(u->ctrl.name), "%s", name);
}

gmb_control_t* gmb_tui_add_usercontrol(int x, int y, int w, int h,
                                       void (*paint)(void*, gmb_gfx_context_t*), void* instance,
                                       const char* container, const char* name)
{
    gmb_usercontrol_t* u = (gmb_usercontrol_t*)calloc(1, sizeof(*u));
    if (!u) return NULL;
    gmb_usercontrol_init(u, x, y, w, h, paint, instance, container, name);
    u->ctrl.ctrl_kind = GMB_CK_USERCONTROL;
    gmb_tui_control_add(&u->ctrl);
    { extern void gmb_form_set_last_handle(int h); gmb_form_set_last_handle(u->ctrl.handle); }
    return &u->ctrl;
}
