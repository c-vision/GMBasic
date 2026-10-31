/* gmb_px_renderer.c — registry (core) dei renderer pixel dei controlli. */
#include "gmb_px_renderer.h"

#define GMB_PX_RENDERER_MAX 128
static gmb_px_draw_fn g_px_renderers[GMB_PX_RENDERER_MAX];

void gmb_px_renderer_set(int control_kind, gmb_px_draw_fn fn) {
    if (control_kind >= 0 && control_kind < GMB_PX_RENDERER_MAX)
        g_px_renderers[control_kind] = fn;
}

gmb_px_draw_fn gmb_px_renderer_get(int control_kind) {
    if (control_kind >= 0 && control_kind < GMB_PX_RENDERER_MAX)
        return g_px_renderers[control_kind];
    return 0;
}
