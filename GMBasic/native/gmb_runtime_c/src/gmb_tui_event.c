/* ============================================================================
 * gmb_tui_event.c — EventHandler.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== EventHandler ===================== */

void gmb_ctrl_event_handler_init(gmb_ctrl_event_handler_t* h)
{
    memset(h, 0, sizeof(*h));
    gmb_focus_manager_init(&h->focus);
}

void gmb_ctrl_event_handler_register(gmb_ctrl_event_handler_t* h, const char* id, gmb_control_t* ctrl)
{
    if (h->control_count >= GMB_CTRL_MAX_CONTROLS || !id || !ctrl) return;
    snprintf(h->ids[h->control_count], GMB_CTRL_MAX_ID, "%s", id);
    h->controls[h->control_count] = ctrl;
    h->control_count++;
    gmb_focus_manager_register_ctrl(&h->focus, id, ctrl);
}

void gmb_ctrl_event_handler_set_focus_by_id(gmb_ctrl_event_handler_t* h, const char* id)
{
    if (gmb_focus_manager_set_focus_by_id(&h->focus, id))
        for (int i = 0; i < h->control_count; i++)
            h->controls[i]->focused = (strcmp(h->ids[i], id) == 0);
}

gmb_control_t* gmb_ctrl_event_handler_focused(gmb_ctrl_event_handler_t* h)
{
    const char* id = gmb_focus_manager_current_id(&h->focus);
    if (!id) return NULL;
    for (int i = 0; i < h->control_count; i++)
        if (strcmp(h->ids[i], id) == 0) return h->controls[i];
    return NULL;
}


bool gmb_ctrl_event_handler_dispatch(gmb_ctrl_event_handler_t* h, const gmb_ui_event_t* e)
{
    if (!e) return false;
    /* Rettangolo effettivo: se il controllo ha un contenitore (window), le sue coordinate
     * sono relative al content del contenitore → offset per l'hit-test/routing. */
    #define EFF_RECT(cc, rr) do { rr = gmb_control_rect(cc); \
        if ((cc)->container[0]) { extern int gmb_winstmt_content_origin(const char*, int*, int*); \
            extern int gmb_tui_control_content_origin(const char*, int*, int*); \
            int _ox = 0, _oy = 0; if (gmb_winstmt_content_origin((cc)->container, &_ox, &_oy) || \
                gmb_tui_control_content_origin((cc)->container, &_ox, &_oy)) { (rr).x += _ox; (rr).y += _oy; } } } while (0)
    switch (e->type)
    {
        case GMB_UIEVENT_KEY:
        {
            gmb_control_t* fc = gmb_ctrl_event_handler_focused(h);
            if (fc)
            {
                fc->focused = true;
                if (e->key == GMB_K_TAB || e->key == GMB_K_BTAB)
                {
                    gmb_focus_manager_handle_key(&h->focus, e->key);
                    const char* id = gmb_focus_manager_current_id(&h->focus);
                    for (int i = 0; i < h->control_count; i++)
                        h->controls[i]->focused = id ? (strcmp(h->ids[i], id) == 0) : false;
                    return true;
                }
                if (gmb_control_handle_key(fc, e->key)) return true;
            }
            return false;
        }
        case GMB_UIEVENT_MOUSE:
        {
            const gmb_mouse_action_t* a = &e->mouse;
            if (a->kind == GMB_MA_CLICK || a->kind == GMB_MA_DOUBLECLICK)
            {
                if (gmb_focus_manager_set_focus_by_point(&h->focus, a->x, a->y))
                {
                    const char* id = gmb_focus_manager_current_id(&h->focus);
                    for (int i = 0; i < h->control_count; i++)
                        h->controls[i]->focused = id ? (strcmp(h->ids[i], id) == 0) : false;
                }
            }
            gmb_control_t* fc = gmb_ctrl_event_handler_focused(h);
            if (fc && gmb_control_handle_mouse(fc, a)) return true;
            /* topmost (ultimo registrato) al punto */
            for (int i = h->control_count - 1; i >= 0; i--)
            {
                gmb_control_t* c = h->controls[i];
                gmb_rect_t cr; EFF_RECT(c, cr);
                if (c != fc && c->visible && c->enabled && rect_contains(cr, a->x, a->y))
                    if (gmb_control_handle_mouse(c, a)) return true;
            }
            return false;
        }
        default:
            return false;
    }
}

int gmb_ctrl_event_handler_event_to_navigation(const gmb_ui_event_t* e)
{
    if (!e) return 0;
    if (e->type == GMB_UIEVENT_KEY)
    {
        switch (e->key)
        {
            case GMB_K_UP: return 1;
            case GMB_K_DOWN: return 2;
            case GMB_K_TAB: return 2;
            case GMB_K_BTAB: return 1;
            case GMB_K_HOME: return 3;
            case GMB_K_END: return 4;
            case GMB_K_PGUP: return 5;
            case GMB_K_PGDN: return 6;
            case GMB_K_ENTER: return 7;
            case GMB_K_ESC: return 8;
            default: return 0;
        }
    }
    if (e->type == GMB_UIEVENT_MOUSE)
    {
        switch (e->mouse.kind)
        {
            case GMB_MA_CLICK: case GMB_MA_DOUBLECLICK: return 9;
            case GMB_MA_SCROLLUP: return 10;
            case GMB_MA_SCROLLDOWN: return 11;
            default: return 0;
        }
    }
    return 0;
}

bool gmb_ctrl_event_handler_is_quit(const gmb_ui_event_t* e)
{
    if (!e || e->type != GMB_UIEVENT_KEY) return false;
    return e->key == GMB_K_ESC || e->key == 'q' || e->key == 'Q' ||
           (e->key == 'q' && (e->mods & GMB_MODKEY_CONTROL));
}

/* ============================================================================
 * Widget implementations
 * ============================================================================ */

/* ---- Button ---- */
