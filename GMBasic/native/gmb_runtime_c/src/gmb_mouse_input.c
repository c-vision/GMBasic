/* gmb_mouse_input.c — parità common/CommonLib/Mouse/MouseInput.cs:180 — flusso autonomo C
 * Stato statico mouse + Enable/Disable ANSI + parser SGR ESC[<cb;cx;cyM/m + coda + Limit.
 * Senza thread (modello poll): il chiamante legge stdin/typeahead e chiama process/pump.
 */
#include "gmb_mouse_input.h"
#include "gmb_runtime.h"
#include <string.h>
#include <stdio.h>

typedef struct gmb_mouse_state {
    int last_x, last_y, last_buttons, last_scroll_delta;
    int enabled, has_moved;
    int has_limit, lx1, ly1, lx2, ly2;
    gmb_mouse_event_raw_t queue[GMB_MI_QUEUE];
    int q_head, q_tail, q_count;
} gmb_mouse_state_t;

static gmb_mouse_state_t g_mi;

int gmb_mouse_input_last_x(void) { return g_mi.last_x; }
int gmb_mouse_input_last_y(void) { return g_mi.last_y; }
int gmb_mouse_input_last_buttons(void) { return g_mi.last_buttons; }
int gmb_mouse_input_last_scroll_delta(void) { return g_mi.last_scroll_delta; }
int gmb_mouse_input_enabled(void) { return g_mi.enabled; }
int gmb_mouse_input_has_moved(void) { return g_mi.has_moved; }

static void mi_enqueue(int x, int y, int buttons, int scroll)
{
    if (g_mi.q_count >= GMB_MI_QUEUE) return;
    g_mi.queue[g_mi.q_tail].x = x;
    g_mi.queue[g_mi.q_tail].y = y;
    g_mi.queue[g_mi.q_tail].buttons = buttons;
    g_mi.queue[g_mi.q_tail].scroll = scroll;
    g_mi.q_tail = (g_mi.q_tail + 1) % GMB_MI_QUEUE;
    g_mi.q_count++;
}

void gmb_mouse_input_enable(void)
{
    if (g_mi.enabled) return;
    gmb_console_out_write("\x1b[?1000h\x1b[?1003h\x1b[?1006h");
    gmb_console_out_flush();
    g_mi.enabled = 1;
}

void gmb_mouse_input_disable(void)
{
    if (!g_mi.enabled) return;
    gmb_console_out_write("\x1b[?1000l\x1b[?1003l\x1b[?1006l");
    gmb_console_out_flush();
    g_mi.enabled = 0;
}

/* ProcessInput (MouseInput.cs:81-133): cerca ESC[<cb;cx;cyM|m */
void gmb_mouse_input_process(const char* buf, int count)
{
    if (!buf || count <= 0) return;
    for (int i = 0; i < count; i++) {
        if (buf[i] == 0x1b && i + 3 < count && buf[i + 1] == '[' && buf[i + 2] == '<') {
            int j = i + 3;
            int parts[3] = {0, 0, 0};
            int np = 0;
            int current = 0;
            while (j < count && buf[j] != 'M' && buf[j] != 'm' && np < 3) {
                if (buf[j] == ';') { parts[np++] = current; current = 0; }
                else if (buf[j] >= '0' && buf[j] <= '9') current = current * 10 + (buf[j] - '0');
                else break;
                j++;
            }
            if (j < count && (buf[j] == 'M' || buf[j] == 'm') && np + 1 >= 3) {
                parts[np] = current;
                if (np < 3) parts[2] = current;
                int cb = parts[0], cx = parts[1], cy = parts[2];
                int release = buf[j] == 'm';
                int buttons = 0, scroll = 0;
                if ((cb & 0x40) != 0) scroll = ((cb & 1) == 0) ? 1 : -1;
                else if (release) buttons = 0;
                else {
                    if ((cb & 1) != 0) buttons |= 1;
                    if ((cb & 2) != 0) buttons |= 2;
                    if ((cb & 4) != 0) buttons |= 4;
                }
                mi_enqueue(cx, cy, buttons, scroll);
                i = j;
            }
        }
    }
}

void gmb_mouse_input_pump(void)
{
    while (g_mi.q_count > 0) {
        gmb_mouse_event_raw_t evt = g_mi.queue[g_mi.q_head];
        g_mi.q_head = (g_mi.q_head + 1) % GMB_MI_QUEUE;
        g_mi.q_count--;
        g_mi.last_x = evt.x;
        g_mi.last_y = evt.y;
        g_mi.has_moved = 1;
        if (evt.buttons != 0 || evt.scroll != 0)
            g_mi.last_buttons = evt.buttons;
        if (evt.scroll != 0)
            g_mi.last_scroll_delta = evt.scroll;
    }
}

void gmb_mouse_input_show(void) { gmb_console_out_write("\x1b[?25h"); gmb_console_out_flush(); }
void gmb_mouse_input_hide(void) { gmb_console_out_write("\x1b[?25l"); gmb_console_out_flush(); }

void gmb_mouse_input_set_position(int x, int y)
{
    if (g_mi.has_limit) {
        if (x < g_mi.lx1) x = g_mi.lx1;
        if (x > g_mi.lx2) x = g_mi.lx2;
        if (y < g_mi.ly1) y = g_mi.ly1;
        if (y > g_mi.ly2) y = g_mi.ly2;
    }
    g_mi.last_x = x;
    g_mi.last_y = y;
}

void gmb_mouse_input_limit(int x1, int y1, int x2, int y2)
{
    g_mi.has_limit = 1;
    g_mi.lx1 = x1 < x2 ? x1 : x2;
    g_mi.ly1 = y1 < y2 ? y1 : y2;
    g_mi.lx2 = x1 < x2 ? x2 : x1;
    g_mi.ly2 = y1 < y2 ? y2 : y1;
}

void gmb_mouse_input_unlimit(void) { g_mi.has_limit = 0; }

void gmb_mouse_input_set_cursor(int cursor_id)
{
    /* 0=default, 1=linea, 2=barra (DECSCUSR, MouseInput.cs:174-179) */
    if (cursor_id == 1) gmb_console_out_write("\x1b[4 q");
    else if (cursor_id == 2) gmb_console_out_write("\x1b[6 q");
    else gmb_console_out_write("\x1b[0 q");
    gmb_console_out_flush();
}
