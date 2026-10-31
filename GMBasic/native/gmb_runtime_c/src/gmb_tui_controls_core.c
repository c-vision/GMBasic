/* gmb_tui_controls_core.c — factory TESTUALI pure dei controlli (core).
 * Separati da gmb_tui_controls_add.c (UI: frame/pixel). Le init vivono nei file
 * gmb_tui_<ctrl>.c (core); qui solo l'allocazione+registrazione. */
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include <stdlib.h>

gmb_control_t* gmb_tui_add_numericupdown(int x, int y, int w, int mn, int mx, int val)
{ gmb_numeric_updown_t* c = (gmb_numeric_updown_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_numeric_updown_init(c, x, y, w); gmb_numeric_updown_set_range(c, mn, mx); gmb_numeric_updown_set_value(c, val); c->ctrl.ctrl_kind = GMB_CK_NUMERICUPDOWN; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_starrating(int x, int y, int w, int mx, int val)
{ gmb_star_rating_t* c = (gmb_star_rating_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_star_rating_init(c, x, y, w); c->max = mx; c->value = val; c->ctrl.ctrl_kind = GMB_CK_STARRATING; c->ctrl.height = 2; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
gmb_control_t* gmb_tui_add_scrollbar(int x, int y, int len, int mn, int mx, int val)
{ gmb_scrollbar_t* c = (gmb_scrollbar_t*)calloc(1, sizeof(*c)); if (!c) return NULL; gmb_scrollbar_init(c, x, y, len, 0); gmb_scrollbar_set(c, val, mn, mx); c->ctrl.ctrl_kind = GMB_CK_SCROLLBAR; gmb_tui_control_add(&c->ctrl); return &c->ctrl; }
