/* gmb_mouse_input.h — parità common/CommonLib/Mouse/MouseInput.cs:180 — flusso autonomo C
 * Stato statico mouse + Enable/Disable ANSI + parser SGR + coda + Limit. No bridge C#.
 */
#ifndef GMB_MOUSE_INPUT_H
#define GMB_MOUSE_INPUT_H

#define GMB_MI_QUEUE 64

typedef struct gmb_mouse_event_raw {
    int x, y, buttons, scroll;
} gmb_mouse_event_raw_t;

int gmb_mouse_input_last_x(void);
int gmb_mouse_input_last_y(void);
int gmb_mouse_input_last_buttons(void);
int gmb_mouse_input_last_scroll_delta(void);
int gmb_mouse_input_enabled(void);
int gmb_mouse_input_has_moved(void);

/* Enable/Disable (MouseInput.cs:16-32): ANSI ?1000h?1003h?1006h / l */
void gmb_mouse_input_enable(void);
void gmb_mouse_input_disable(void);

/* ProcessInput (MouseInput.cs:81-133): parse sequenze SGR ESC[<cb;cx;cyM/m → coda */
void gmb_mouse_input_process(const char* buf, int count);

/* PumpEvents (MouseInput.cs:135-147): coda → LastX/Y/buttons/scroll/HasMoved */
void gmb_mouse_input_pump(void);

/* Show/Hide/SetPosition/Limit/Unlimit/SetCursor (MouseInput.cs:154-179) */
void gmb_mouse_input_show(void);
void gmb_mouse_input_hide(void);
void gmb_mouse_input_set_position(int x, int y);
void gmb_mouse_input_limit(int x1, int y1, int x2, int y2);
void gmb_mouse_input_unlimit(void);
void gmb_mouse_input_set_cursor(int cursor_id);

#endif
