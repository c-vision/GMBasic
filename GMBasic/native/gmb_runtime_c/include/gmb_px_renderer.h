/* gmb_px_renderer.h — registry dei renderer PIXEL dei controlli (Controlli polimorfici).
 * Un controllo ha DUE target: `draw` (celle, TEXT, nel core) e `draw_px_fn` (pixel).
 * I renderer pixel vivono nel target UI e si registrano all'init del modulo grafico;
 * in headless (UI non linkata) il registry è vuoto → nessun renderer pixel (stato reale). */
#ifndef GMB_PX_RENDERER_H
#define GMB_PX_RENDERER_H

struct gmb_control;
typedef void (*gmb_px_draw_fn)(void* user, struct gmb_control* c);

void          gmb_px_renderer_set(int control_kind, gmb_px_draw_fn fn);
gmb_px_draw_fn gmb_px_renderer_get(int control_kind);

/* Implementata nel modulo UI (gmb_graphics_controls_px.c): registra i renderer pixel. */
void gmb_px_renderers_install(void);

#endif /* GMB_PX_RENDERER_H */
