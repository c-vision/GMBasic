/* gmb_chart_gmb.h — Draw entry point del motore autonomo `gmb_chart` per l'host GMBasic.
 * Due adapter di rendering: pixel (gmb_gfx_*) e celle (gmb_screen_buffer_t). */
#ifndef GMB_CHART_GMB_H
#define GMB_CHART_GMB_H

#include "gmb_chart.h"
#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Rendering pixel (gmb_gfx_*): usato dal path GRAPHICS / dal namespace CHART. */
void gmb_chart_draw_gfx(gmb_chart_t* ch, int ox, int oy, int w, int h);

/* Rendering a celle su buffer TUI: usato dai controlli in TEXT (es. CANDLESTICK). */
void gmb_chart_draw_cells(gmb_chart_t* ch, gmb_screen_buffer_t* buf, int ox, int oy, int w, int h);

#ifdef __cplusplus
}
#endif
#endif /* GMB_CHART_GMB_H */
