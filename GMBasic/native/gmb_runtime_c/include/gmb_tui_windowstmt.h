/* ============================================================================
 * gmb_tui_windowstmt.h — modello finestre per gli statement WINDOW
 * (specchio del comportamento IL/PCode del backend C#).
 *
 * Registry nome→id sul WindowManager statico (gmb_window_manager_static),
 * contenuto per finestra, flag edit/shadow/frame/desktop/tema e render sul
 * buffer TUI globale (gmb_tui_global_state()->buffer) quando disponibile.
 *
 * Usato dall'emitter C99Native (st_window in gmb_c99_emit.c) e verificabile
 * dai minitest (gmb_winstmt_count/line_count/line).
 * ============================================================================ */
#ifndef GMB_TUI_WINDOWSTMT_H
#define GMB_TUI_WINDOWSTMT_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* flag di DEFINE (bit) */
enum {
    GMB_WINSTMT_DOUBLE  = 1 << 0,
    GMB_WINSTMT_SHADOW  = 1 << 1,
    GMB_WINSTMT_PIXEL   = 1 << 2,
    GMB_WINSTMT_ROUNDED = 1 << 3,
    GMB_WINSTMT_NOBUTTONS = 1 << 4   /* esclude ESPLICITAMENTE i pulsanti (default: presenti) */
};

/* WINDOW DEFINE nome FROM t,l TO b,r [TITLE ...] [DOUBLE|SINGLE|ROUNDED] [SHADOW] [PIXEL] */
int  gmb_winstmt_define(const char* name, const char* title,
                        int top, int left, int bottom, int right, int flags);
/* WINDOW nome SELECT */
void gmb_winstmt_select(const char* name);
/* WINDOW nome SET TITLE / WINDOW SET TITLE (name="") */
void gmb_winstmt_set_title(const char* name, const char* title);
/* WINDOW nome PRINT <riga> */
void gmb_winstmt_append(const char* name, const char* text);
/* WINDOW nome TEXT <contenuto> */
void gmb_winstmt_content(const char* name, const char* text);
/* WINDOW nome CLEAR */
void gmb_winstmt_clear(const char* name);
/* WINDOW nome SCROLL <n> */
void gmb_winstmt_scroll(const char* name, int lines);
/* WINDOW nome EDIT ON|OFF */
void gmb_winstmt_edit(const char* name, int on);
/* WINDOW nome DESKTOP <n> */
void gmb_winstmt_move_desktop(const char* name, int desktop);
/* WINDOW nome FRAME DOUBLE|SINGLE|ROUNDED */
void gmb_winstmt_frame(const char* name, int border);
/* WINDOW nome SHADOW ON|OFF */
void gmb_winstmt_shadow(const char* name, int on);
/* WINDOW nome ACTIVATE|DEACTIVATE|RELEASE|CLOSE|MINIMIZE|RESTORE */
void gmb_winstmt_action(const char* name, const char* action);
/* WINDOW THEME <n> (globale) */
void gmb_winstmt_show(int on);   /* WINDOW SHOW/HIDE: mostra/nasconde la finestra gfx */
void gmb_winstmt_theme(int theme);
/* WINDOW THEME "Nome" (es. "Turbo"): tema per NOME (il nome è la chiave; il tema numerico
 * resta per WINDOW THEME <n>). */
void gmb_winstmt_theme_name(const char* name);
/* WINDOW DESKTOP <n> (globale) */
void gmb_winstmt_desktop(int desktop);
/* WINDOW FRAME DOUBLE|SINGLE|ROUNDED (globale) */
void gmb_winstmt_frame_all(int border);
/* WINDOW SHADOW ON|OFF (globale) */
void gmb_winstmt_shadow_all(int on);

/* introspezione (minitest) */
int  gmb_winstmt_count(void);
int  gmb_winstmt_id(const char* name);
int  gmb_winstmt_line_count(const char* name);
const char* gmb_winstmt_line(const char* name, int i);
int  gmb_winstmt_theme_get(void);
int  gmb_winstmt_desktop_get(void);
int  gmb_winstmt_edit_get(const char* name);
int  gmb_winstmt_shadow_get(const char* name);
int  gmb_winstmt_frame_get(const char* name);
int  gmb_winstmt_is_minimized(const char* name);
/* render esplicito (di norma automatico a ogni operazione) */
void gmb_winstmt_render(const char* name);
void gmb_winstmt_render_all(void);
/* render esplicito (di norma automatico a ogni operazione) */
void gmb_winstmt_render(const char* name);
void gmb_winstmt_render_all(void);
/* Render nativo PIXEL sul page gfx (frame rounded + titolo + contenuto). */
void gmb_winstmt_render_pixel(const char* name, int origin_x, int origin_y, int cell_w, int cell_h);
/* Ripristina (de-minimizza) la finestra con dato id (chip della status bar). */
void gmb_winstmt_restore_by_id(int id);
/* Compositor pixel di TUTTE le finestre (porting GfxWindowManager): chrome nativo. */
void gmb_winstmt_compose_all_pixel(void);
/* Gestione mouse sulle finestre (chiudi/min/max/drag). Main thread. */
void gmb_winstmt_handle_mouse_events(void);
/* Handler mouse in coord pixel-logiche (cw/ch = cella). Usato anche dalla SdlConsole. */
void gmb_winstmt_handle_mouse_px(int lx, int ly, int type, int button, int cw, int ch, int workW, int workH);
/* Iniezione eventi mouse da GMB_MOUSE_SCRIPT per test autonomi. */
void gmb_winstmt_mouse_script_pump(void);
/* reset (test) */
void gmb_winstmt_reset(void);

/* ==== API finestre PIXEL per il client C# (stato nel runtime C) ====
 * Coordinate in PIXEL LOGICI, layout identico al compositor (title 24, padding 6,
 * pulsanti a destra, bordo resize 4). Prima queste capacità esistevano solo nella
 * classe C# GfxWindowManager. */
void gmb_winstmt_pixel_client_rect(const char* name, int* out4);
int  gmb_winstmt_pixel_control_at(const char* name, int x, int y);   /* 1=close 2=min 3=max */
int  gmb_winstmt_pixel_resize_edge_at(const char* name, int x, int y); /* N=1 S=2 E=4 W=8 */
int  gmb_winstmt_pixel_taskbar_chip_at(int x, int y);                 /* id o -1 */
void gmb_winstmt_set_colors(const char* name, unsigned int body, unsigned int fg, unsigned int title);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_WINDOWSTMT_H */
