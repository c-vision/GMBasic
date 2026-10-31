/* ============================================================================
 * gmb_tui_fsm.c — Fsm.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gmb_fsm_init(gmb_fsm_t* f, int initial, gmb_fsm_on_event_t handler, void* ctx)
{
    memset(f, 0, sizeof(*f));
    f->current_state = initial;
    f->running = true;
    f->on_event = handler;
    f->ctx = ctx;
}
gmb_fsm_transition_t gmb_fsm_transition_nothing(void)
{
    gmb_fsm_transition_t t = { GMB_TRANSITION_DO_NOTHING, -1, 0 };
    return t;
}
gmb_fsm_transition_t gmb_fsm_transition_change(int new_state)
{
    gmb_fsm_transition_t t = { GMB_TRANSITION_CHANGE_STATE, new_state, 0 };
    return t;
}
gmb_fsm_transition_t gmb_fsm_transition_abort(int reason)
{
    gmb_fsm_transition_t t = { GMB_TRANSITION_ABORT, -1, reason };
    return t;
}
int gmb_fsm_fire(gmb_fsm_t* f, int event_id)
{
    if (!f->running || !f->on_event) return f->current_state;
    gmb_fsm_transition_t t = f->on_event(f->ctx, f->current_state, event_id);
    switch (t.kind)
    {
        case GMB_TRANSITION_CHANGE_STATE:
            f->current_state = t.new_state;
            break;
        case GMB_TRANSITION_ABORT:
            f->running = false;
            break;
        default:
            break;
    }
    return f->current_state;
}
