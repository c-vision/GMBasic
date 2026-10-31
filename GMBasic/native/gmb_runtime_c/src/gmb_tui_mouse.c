/* ============================================================================
 * gmb_tui_mouse.c — layer mouse (porting ClipperLib/Clipper Mouse.cs).
 * TTY-free: parsing di sequenze SGR/X10 e classificazione in azioni.
 * ============================================================================ */
#include "gmb_tui_mouse.h"
#include "gmb_tui_clipper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double mouse_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int gmb_mouse_action_inkey(const gmb_mouse_action_t* a)
{
    return GMB_MOUSE_BASE - a->kind;
}

int gmb_mouse_action_from_inkey(int inkey, gmb_clipper_state_t* st, gmb_mouse_action_t* out)
{
    int raw = GMB_MOUSE_BASE - inkey;
    if (raw >= 0 && raw < GMB_MA_COUNT)
    {
        out->kind = raw;
        out->x = st->mouse_x;
        out->y = st->mouse_y;
        out->mods = GMB_MODKEY_NONE;
        if (st->mouse_shift[0] || st->mouse_shift[1] || st->mouse_shift[2]) out->mods |= GMB_MODKEY_SHIFT;
        if (st->mouse_ctrl[0] || st->mouse_ctrl[1] || st->mouse_ctrl[2]) out->mods |= GMB_MODKEY_CONTROL;
        return 1;
    }
    return 0;
}

int gmb_mouse_action_from_inkey_stub(int inkey, gmb_mouse_action_t* out)
{
    int raw = GMB_MOUSE_BASE - inkey;
    if (raw >= 0 && raw < GMB_MA_COUNT)
    {
        out->kind = raw;
        out->x = 0;
        out->y = 0;
        out->mods = GMB_MODKEY_NONE;
        return 1;
    }
    return 0;
}

static int mouse_ansi_button(int b)
{
    switch (b)
    {
        case 0: return GMB_MB_LEFT;
        case 1: return GMB_MB_MIDDLE;
        case 2: return GMB_MB_RIGHT;
        default: return GMB_MB_ANY;
    }
}

int gmb_mouse_from_encoded(int cb, int x, int y, bool released, gmb_mouse_event_t* out)
{
    gmb_key_modifiers_t mods = GMB_MODKEY_NONE;
    if ((cb & 0x04) != 0) mods |= GMB_MODKEY_SHIFT;
    if ((cb & 0x08) != 0) mods |= GMB_MODKEY_ALT;
    if ((cb & 0x10) != 0) mods |= GMB_MODKEY_CONTROL;

    int code = cb & ~0x1C;
    int btn = GMB_MB_LEFT;
    bool drag = false;
    int scroll = GMB_SCROLL_NONE;

    if (code >= 64 && code <= 67)
    {
        switch (code)
        {
            case 64: scroll = GMB_SCROLL_UP; break;
            case 65: scroll = GMB_SCROLL_DOWN; break;
            case 66: scroll = GMB_SCROLL_LEFT; break;
            default: scroll = GMB_SCROLL_RIGHT; break;
        }
    }
    else if ((cb & 0x20) != 0)
    {
        drag = true;
        btn = mouse_ansi_button(cb & 0x03);
    }
    else
    {
        btn = mouse_ansi_button(cb & 0x03);
    }

    out->button = btn;
    out->x = x - 1;
    out->y = y - 1;
    out->mods = mods;
    out->released = released;
    out->drag = drag;
    out->scroll = scroll;
    return 1;
}

int gmb_mouse_decode_sequence(const char* seq, gmb_mouse_event_t* out)
{
    if (!seq || !*seq || strlen(seq) < 3) return 0;
    int sl = (int)strlen(seq);

    if (sl >= 3 && seq[0] == '\x1b' && seq[1] == '[' && seq[2] == '<')
    {
        int end = -1;
        for (int i = 3; i < sl; i++)
            if (seq[i] == 'M' || seq[i] == 'm') { end = i; break; }
        if (end < 0) return 0;
        char body[32];
        int bn = end - 3;
        if (bn >= 31) bn = 31;
        memcpy(body, seq + 3, (size_t)bn);
        body[bn] = '\0';
        int cb = 0, x = 0, y = 0;
        if (sscanf(body, "%d;%d;%d", &cb, &x, &y) != 3) return 0;
        bool released = seq[end] == 'm';
        return gmb_mouse_from_encoded(cb, x, y, released, out);
    }

    if (sl >= 3 && seq[0] == '\x1b' && seq[1] == '[' && seq[2] == 'M')
    {
        if (sl < 6) return 0;
        int cb = (unsigned char)seq[3] - 32;
        int x = (unsigned char)seq[4] - 32;
        int y = (unsigned char)seq[5] - 32;
        return gmb_mouse_from_encoded(cb, x, y, false, out);
    }

    return 0;
}

/* doppio/triplo click: 400ms / 2 celle */
#define GMB_DOUBLE_MS  400.0
#define GMB_DOUBLE_DIST 2

static double g_last_click = -1e18;
static int g_last_click_x, g_last_click_y;
static double g_last_double = -1e18;
static int g_last_double_x, g_last_double_y;

int gmb_mouse_process_event(const gmb_mouse_event_t* e, gmb_mouse_action_t* out)
{
    out->kind = GMB_MA_NONE;
    out->x = e->x;
    out->y = e->y;
    out->mods = e->mods;

    if (e->scroll != GMB_SCROLL_NONE)
    {
        switch (e->scroll)
        {
            case GMB_SCROLL_UP: out->kind = GMB_MA_SCROLLUP; break;
            case GMB_SCROLL_DOWN: out->kind = GMB_MA_SCROLLDOWN; break;
            case GMB_SCROLL_LEFT: out->kind = GMB_MA_SCROLLLEFT; break;
            default: out->kind = GMB_MA_SCROLLRIGHT; break;
        }
        return 1;
    }

    if (e->drag)
    {
        out->kind = (e->button == GMB_MB_ANY) ? GMB_MA_MOVE : GMB_MA_DRAG;
        return 1;
    }
    if (e->released)
    {
        out->kind = GMB_MA_RELEASE;
        return 1;
    }

    switch (e->button)
    {
        case GMB_MB_RIGHT:
            out->kind = GMB_MA_RIGHTCLICK;
            return 1;
        case GMB_MB_MIDDLE:
            out->kind = GMB_MA_MIDDLECLICK;
            return 1;
        default:
            break;
    }

    double now = mouse_now_ms();
    if (now - g_last_double <= GMB_DOUBLE_MS &&
        e->x - g_last_double_x <= GMB_DOUBLE_DIST && g_last_double_x - e->x <= GMB_DOUBLE_DIST &&
        e->y - g_last_double_y <= GMB_DOUBLE_DIST && g_last_double_y - e->y <= GMB_DOUBLE_DIST)
    {
        g_last_double = -1e18;
        out->kind = GMB_MA_TRIPLECLICK;
        return 1;
    }

    if (now - g_last_click <= GMB_DOUBLE_MS &&
        e->x - g_last_click_x <= GMB_DOUBLE_DIST && g_last_click_x - e->x <= GMB_DOUBLE_DIST &&
        e->y - g_last_click_y <= GMB_DOUBLE_DIST && g_last_click_y - e->y <= GMB_DOUBLE_DIST)
    {
        g_last_click = -1e18;
        g_last_double = now;
        g_last_double_x = e->x;
        g_last_double_y = e->y;
        out->kind = GMB_MA_DOUBLECLICK;
        return 1;
    }

    g_last_click = now;
    g_last_click_x = e->x;
    g_last_click_y = e->y;
    out->kind = GMB_MA_CLICK;
    return 1;
}

void gmb_mouse_reset_click_state(void)
{
    g_last_click = -1e18;
    g_last_double = -1e18;
}

int gmb_mouse_decode_and_process(const char* seq, gmb_mouse_action_t* out)
{
    gmb_mouse_event_t ev;
    if (!gmb_mouse_decode_sequence(seq, &ev)) return 0;
    return gmb_mouse_process_event(&ev, out);
}
