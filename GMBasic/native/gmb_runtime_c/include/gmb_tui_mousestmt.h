/* ============================================================================
 * gmb_tui_mousestmt.h — modello MOUSE per gli statement (headless, specchio
 * dei syscall MOUSE_* del backend C#: MOUSE_ENABLE/SHOW/SETPOS/CURSOR/LIMIT/WAIT).
 *
 * Stato nel ClipperState globale (mouse_present, mouse_cursor_visible,
 * mouse_row/col, mouse_cursor_shape) + limiti in questo modulo. Nessun SDL:
 * headless-safe come il resto del runtime C.
 * ============================================================================ */
#ifndef GMB_TUI_MOUSESTMT_H
#define GMB_TUI_MOUSESTMT_H

#ifdef __cplusplus
extern "C" {
#endif

void gmb_mousestmt_enable(int on);          /* MOUSE ON|OFF */
void gmb_mousestmt_show(int visible);       /* MOUSE SHOW|HIDE */
void gmb_mousestmt_setpos(int x, int y);    /* MOUSE SET x, y */
void gmb_mousestmt_cursor(int id);          /* MOUSE CURSOR id */
void gmb_mousestmt_limit(int x1, int y1, int x2, int y2); /* x1<0 → UNLIMIT */
void gmb_mousestmt_wait(void);              /* MOUSE WAIT (sync point headless) */

/* introspezione (minitest) */
int gmb_mousestmt_enabled(void);
int gmb_mousestmt_visible(void);
int gmb_mousestmt_x(void);
int gmb_mousestmt_y(void);
int gmb_mousestmt_cursor_id(void);
int gmb_mousestmt_limited(void);
int gmb_mousestmt_limit_x1(void);
int gmb_mousestmt_limit_y1(void);
int gmb_mousestmt_limit_x2(void);
int gmb_mousestmt_limit_y2(void);
int gmb_mousestmt_wait_count(void);
void gmb_mousestmt_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_MOUSESTMT_H */
