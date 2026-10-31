/* ============================================================================
 * gmb_tui_mousestmt.c — modello MOUSE headless (vedi include/gmb_tui_mousestmt.h).
 * Stato nel ClipperState globale; nessun SDL.
 * ============================================================================ */
#include "gmb_tui_mousestmt.h"
#include "gmb_tui_clipper.h"
#include <stdio.h>

static int g_mouse_cursor_id = 0;
static int g_mouse_limited = 0;
static int g_mouse_x1 = 0, g_mouse_y1 = 0, g_mouse_x2 = 0, g_mouse_y2 = 0;
static int g_mouse_wait_count = 0;

/* id cursore → nome (specchio SDL_SystemCursor usato dal backend C#) */
static const char* mouse_cursor_name(int id)
{
    static const char* names[] = {
        "arrow", "ibeam", "wait", "cross", "uparrow",
        "sizeNWSE", "sizeNESW", "sizeWE", "sizeNS", "sizeAll", "no", "hand"
    };
    if (id >= 0 && id < (int)(sizeof(names) / sizeof(names[0]))) return names[id];
    return "arrow";
}

void gmb_mousestmt_enable(int on)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (on) gmb_clipper_mouse_enable(); else gmb_clipper_mouse_disable();
    if (st) st->mouse_present = on != 0;
}

void gmb_mousestmt_show(int visible)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (st) st->mouse_cursor_visible = visible != 0;
}

void gmb_mousestmt_setpos(int x, int y)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (!st) return;
    if (g_mouse_limited) {
        if (x < g_mouse_x1) x = g_mouse_x1;
        if (x > g_mouse_x2) x = g_mouse_x2;
        if (y < g_mouse_y1) y = g_mouse_y1;
        if (y > g_mouse_y2) y = g_mouse_y2;
    }
    st->mouse_col = x;
    st->mouse_row = y;
}

void gmb_mousestmt_cursor(int id)
{
    g_mouse_cursor_id = id;
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (st) snprintf(st->mouse_cursor_shape, sizeof(st->mouse_cursor_shape), "%s", mouse_cursor_name(id));
}

void gmb_mousestmt_limit(int x1, int y1, int x2, int y2)
{
    if (x1 < 0 && y1 < 0 && x2 < 0 && y2 < 0) {
        g_mouse_limited = 0;
        g_mouse_x1 = g_mouse_y1 = g_mouse_x2 = g_mouse_y2 = 0;
        return;
    }
    g_mouse_limited = 1;
    g_mouse_x1 = x1; g_mouse_y1 = y1; g_mouse_x2 = x2; g_mouse_y2 = y2;
}

void gmb_mousestmt_wait(void)
{
    g_mouse_wait_count++;
}

int gmb_mousestmt_enabled(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    return st ? (st->mouse_present ? 1 : 0) : 0;
}

int gmb_mousestmt_visible(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    return st ? (st->mouse_cursor_visible ? 1 : 0) : 0;
}

int gmb_mousestmt_x(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    return st ? st->mouse_col : 0;
}

int gmb_mousestmt_y(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    return st ? st->mouse_row : 0;
}

int gmb_mousestmt_cursor_id(void)   { return g_mouse_cursor_id; }
int gmb_mousestmt_limited(void)     { return g_mouse_limited; }
int gmb_mousestmt_limit_x1(void)    { return g_mouse_x1; }
int gmb_mousestmt_limit_y1(void)    { return g_mouse_y1; }
int gmb_mousestmt_limit_x2(void)    { return g_mouse_x2; }
int gmb_mousestmt_limit_y2(void)    { return g_mouse_y2; }
int gmb_mousestmt_wait_count(void)  { return g_mouse_wait_count; }

void gmb_mousestmt_reset(void)
{
    g_mouse_cursor_id = 0;
    g_mouse_limited = 0;
    g_mouse_x1 = g_mouse_y1 = g_mouse_x2 = g_mouse_y2 = 0;
    g_mouse_wait_count = 0;
}
