/* ============================================================================
 * gmb_tui_input_bridge.c — Routing input grafico → controlli (piano TUI-GUI).
 * Drena gmb_gfx_input (tasti UTF-8 + eventi mouse in pixel) e li consegna al
 * controllo focus / all'EventHandler, convertendo pixel→cella. Stesso thread
 * del pump SDL. C-first: nessuna logica nel managed.
 * gmb_gfx_input è compilato solo con SDL presente (GMB_GFX_SDL_PRESENT);
 * senza SDL il pump è un no-op (headless).
 * ============================================================================ */
#include "gmb_tui_widgets.h"

#ifdef GMB_GFX_SDL_PRESENT

#include "gmb_gfx_input.h"
#include <string.h>

/* Mappa un tasto UTF-8 (primo codice) nei codici gmb_tui (GMB_K_*). */
static int gmb_tui_key_from_utf8(const char* k)
{
    unsigned char c = (unsigned char)k[0];
    switch (c)
    {
        case 0x1b: return GMB_K_ESC;
        case '\r': case '\n': return GMB_K_ENTER;
        case '\b': return GMB_K_BS;
        case '\t': return GMB_K_TAB;
        default: break;
    }
    return (int)c;
}

static void gmb_tui_mouse_kind(int type, int button, int wheel, gmb_mouse_action_t* ma)
{
    switch (type)
    {
        case GMB_GFX_MOUSE_DOWN:
            ma->kind = button == 3 ? GMB_MA_RIGHTCLICK : (button == 2 ? GMB_MA_MIDDLECLICK : GMB_MA_CLICK);
            break;
        case GMB_GFX_MOUSE_UP: ma->kind = GMB_MA_RELEASE; break;
        case GMB_GFX_MOUSE_WHEEL: ma->kind = wheel > 0 ? GMB_MA_SCROLLUP : GMB_MA_SCROLLDOWN; break;
        default: ma->kind = GMB_MA_MOVE; break;
    }
}

static int gmb_tui_pump_events(gmb_control_t* c, gmb_ctrl_event_handler_t* h, int cell_w, int cell_h)
{
    int n = 0;
    char k[32];
    while (gmb_gfx_input_poll_key(k, (int)sizeof(k)))
    {
        int code = gmb_tui_key_from_utf8(k);
        if (h)
        {
            gmb_ui_event_t ev; memset(&ev, 0, sizeof(ev));
            ev.type = GMB_UIEVENT_KEY; ev.key = code;
            if (gmb_ctrl_event_handler_dispatch(h, &ev)) n++;
        }
        else if (c && gmb_control_handle_key(c, code)) n++;
    }
    int type, x, y, button, wheel;
    while (gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel))
    {
        gmb_mouse_action_t ma; memset(&ma, 0, sizeof(ma));
        gmb_tui_mouse_kind(type, button, wheel, &ma);
        ma.x = cell_w > 0 ? x / cell_w : x;
        ma.y = cell_h > 0 ? y / cell_h : y;
        if (h)
        {
            gmb_ui_event_t ev; memset(&ev, 0, sizeof(ev));
            ev.type = GMB_UIEVENT_MOUSE; ev.mouse = ma;
            if (gmb_ctrl_event_handler_dispatch(h, &ev)) n++;
        }
        else if (c && gmb_control_handle_mouse(c, &ma)) n++;
    }
    return n;
}

int gmb_tui_pump_control(gmb_control_t* c, int cell_w, int cell_h)
{
    return gmb_tui_pump_events(c, NULL, cell_w, cell_h);
}

int gmb_tui_pump_event_handler(gmb_ctrl_event_handler_t* h, int cell_w, int cell_h)
{
    return gmb_tui_pump_events(NULL, h, cell_w, cell_h);
}

#else /* !GMB_GFX_SDL_PRESENT — headless: nessun input grafico */

int gmb_tui_pump_control(gmb_control_t* c, int cell_w, int cell_h)
{ (void)c; (void)cell_w; (void)cell_h; return 0; }
int gmb_tui_pump_event_handler(gmb_ctrl_event_handler_t* h, int cell_w, int cell_h)
{ (void)h; (void)cell_w; (void)cell_h; return 0; }

#endif
