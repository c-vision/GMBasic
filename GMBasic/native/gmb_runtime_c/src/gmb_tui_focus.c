/* ============================================================================
 * gmb_tui_focus.c — gmb_tui_focus. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== FocusManager ===================== */

void gmb_focus_manager_init(gmb_focus_manager_t* f)
{
    memset(f, 0, sizeof(*f));
    memset(f, 0, sizeof(*f));
    f->current = -1;
}

void gmb_focus_manager_bind_key(gmb_focus_manager_t* f, const char* chord, const char* action)
{
    if (f->bindings_count >= 16 || !chord || !action) return;
    snprintf(f->bindings_chords[f->bindings_count], 32, "%s", chord);
    for (char* p = f->bindings_chords[f->bindings_count]; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    snprintf(f->bindings_actions[f->bindings_count], 32, "%s", action);
    f->bindings_count++;
}

const char* gmb_focus_manager_resolve_key(gmb_focus_manager_t* f, const char* chord)
{
    if (!chord) return NULL;
    char c[32];
    snprintf(c, sizeof(c), "%s", chord);
    for (char* p = c; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    for (int i = 0; i < f->bindings_count; i++)
        if (strcmp(f->bindings_chords[i], c) == 0) return f->bindings_actions[i];
    return NULL;
}

void gmb_focus_manager_register_rect(gmb_focus_manager_t* f, const char* id, gmb_rect_t area)
{
    if (f->entry_count >= GMB_CTRL_MAX_ENTRIES || !id) return;
    gmb_focus_entry_t* e = &f->entries[f->entry_count++];
    memset(e, 0, sizeof(*e));
    snprintf(e->id, sizeof(e->id), "%s", id);
    e->area = area;
    e->enabled = true;
    e->tab_stop = true;
    if (f->current < 0) f->current = 0;
}

void gmb_focus_manager_register_ctrl(gmb_focus_manager_t* f, const char* id, gmb_control_t* ctrl)
{
    if (f->entry_count >= GMB_CTRL_MAX_ENTRIES || !id || !ctrl) return;
    gmb_focus_entry_t* e = &f->entries[f->entry_count++];
    memset(e, 0, sizeof(*e));
    snprintf(e->id, sizeof(e->id), "%s", id);
    e->area = gmb_control_rect(ctrl);
    e->enabled = ctrl->enabled;
    e->tab_stop = ctrl->tab_stop;
    e->tab_index = ctrl->tab_index;
    e->ctrl = ctrl;
    e->has_ctrl = true;
    /* inserisci in ordine di tab_index */
    if (f->entry_count > 1)
    {
        for (int i = f->entry_count - 1; i > 0; i--)
        {
            if (f->entries[i - 1].tab_index <= f->entries[i].tab_index) break;
            gmb_focus_entry_t tmp = f->entries[i];
            f->entries[i] = f->entries[i - 1];
            f->entries[i - 1] = tmp;
        }
    }
    if (f->current < 0) f->current = 0;
}

void gmb_focus_manager_set_enabled(gmb_focus_manager_t* f, const char* id, bool enabled)
{
    for (int i = 0; i < f->entry_count; i++)
        if (strcmp(f->entries[i].id, id) == 0) { f->entries[i].enabled = enabled; return; }
}

void gmb_focus_manager_set_area(gmb_focus_manager_t* f, const char* id, gmb_rect_t area)
{
    for (int i = 0; i < f->entry_count; i++)
        if (strcmp(f->entries[i].id, id) == 0) { f->entries[i].area = area; return; }
}

const char* gmb_focus_manager_current_id(gmb_focus_manager_t* f)
{
    if (f->current >= 0 && f->current < f->entry_count) return f->entries[f->current].id;
    return NULL;
}

bool gmb_focus_manager_set_focus_by_id(gmb_focus_manager_t* f, const char* id)
{
    for (int i = 0; i < f->entry_count; i++)
        if (strcmp(f->entries[i].id, id) == 0 && f->entries[i].enabled)
        {
            f->current = i;
            return true;
        }
    return false;
}

static gmb_rect_t entry_rect(gmb_focus_manager_t* f, gmb_focus_entry_t* e)
{
    if (e->has_ctrl && e->ctrl) return gmb_control_rect(e->ctrl);
    return e->area;
}

bool gmb_focus_manager_set_focus_by_point(gmb_focus_manager_t* f, int x, int y)
{
    for (int i = f->entry_count - 1; i >= 0; i--)
    {
        if (!f->entries[i].enabled) continue;
        gmb_rect_t r = entry_rect(f, &f->entries[i]);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
        {
            f->current = i;
            return true;
        }
    }
    return false;
}

const char* gmb_focus_manager_next(gmb_focus_manager_t* f)
{
    if (f->entry_count == 0) return NULL;
    int idx = f->current >= 0 ? f->current : 0;
    for (int i = 0; i < f->entry_count; i++)
    {
        idx = (idx + 1) % f->entry_count;
        if (f->entries[idx].enabled && f->entries[idx].tab_stop)
        {
            f->current = idx;
            return f->entries[idx].id;
        }
    }
    return NULL;
}

const char* gmb_focus_manager_previous(gmb_focus_manager_t* f)
{
    if (f->entry_count == 0) return NULL;
    int idx = f->current >= 0 ? f->current : 0;
    for (int i = 0; i < f->entry_count; i++)
    {
        idx = (idx - 1 + f->entry_count) % f->entry_count;
        if (f->entries[idx].enabled && f->entries[idx].tab_stop)
        {
            f->current = idx;
            return f->entries[idx].id;
        }
    }
    return NULL;
}

bool gmb_focus_manager_handle_key(gmb_focus_manager_t* f, int key)
{
    if (key == 9) { gmb_focus_manager_next(f); return true; }
    if (key == 15) { gmb_focus_manager_previous(f); return true; }
    return false;
}

void gmb_focus_manager_clear(gmb_focus_manager_t* f)
{
    f->entry_count = 0;
    f->current = -1;
}

/* UiEvent unificato (specchio FocusManager.HandleEvent): Tab/BackTab navigano (e
 * ritornano l'id focalizzato); click/double-click cambiano focus (senza consumare
 * l'evento). Prima la funzione non esisteva sul lato C → parità incompleta. */
const char* gmb_focus_manager_handle_event(gmb_focus_manager_t* f, const gmb_ui_event_t* ev)
{
    if (!f || !ev) return NULL;
    if (ev->type == GMB_UIEVENT_KEY)
    {
        if (ev->key == GMB_K_TAB && (ev->mods & GMB_MODKEY_SHIFT))
        {
            gmb_focus_manager_previous(f);
            return gmb_focus_manager_current_id(f);
        }
        if (ev->key == GMB_K_TAB)
        {
            gmb_focus_manager_next(f);
            return gmb_focus_manager_current_id(f);
        }
        if (ev->key == GMB_K_BTAB)
        {
            gmb_focus_manager_previous(f);
            return gmb_focus_manager_current_id(f);
        }
        return NULL;
    }
    if (ev->type == GMB_UIEVENT_MOUSE)
    {
        if (ev->mouse.kind == GMB_MA_CLICK || ev->mouse.kind == GMB_MA_DOUBLECLICK)
        {
            if (gmb_focus_manager_set_focus_by_point(f, ev->mouse.x, ev->mouse.y))
                return gmb_focus_manager_current_id(f);
        }
        return NULL;
    }
    return NULL;
}
