/* ============================================================================
 * gmb_gfx_input.c — implementazione di gmb_gfx_input.h.
 *
 * Code FIFO a capacita' fissa, lock-free (scrittore = main thread SDL, lettore =
 * main thread del drain gestito: stesso thread per contratto). In headless le
 * code restano vuote e l'iniezione funziona comunque (test/CI).
 * ============================================================================ */
#include "gmb_gfx_input.h"

#include <string.h>

#define GMB_INPUT_KEY_CAP     256
#define GMB_INPUT_KEY_MAXLEN  64
#define GMB_INPUT_MOUSE_CAP   256
#define GMB_INPUT_SCANCODES   512

static char g_keys[GMB_INPUT_KEY_CAP][GMB_INPUT_KEY_MAXLEN];
static int  g_key_head; /* prossimo da leggere */
static int  g_key_tail; /* prossimo da scrivere */
static int  g_key_count;

typedef struct { int type, x, y, button, wheel; } gmb_input_mouse_ev_t;
static gmb_input_mouse_ev_t g_mouse[GMB_INPUT_MOUSE_CAP];
static int g_mouse_head, g_mouse_tail, g_mouse_count;

static unsigned char g_key_down[GMB_INPUT_SCANCODES];
static int g_mouse_x, g_mouse_y;
static int g_mouse_btn_down;   /* 1 = un tasto mouse è premuto (drag) */
static int g_wheel_accum;      /* delta rotella accumulato (letto dal chart) */
static int g_ctrl_c;

void gmb_gfx_input_reset(void)
{
    g_key_head = g_key_tail = g_key_count = 0;
    g_mouse_head = g_mouse_tail = g_mouse_count = 0;
    memset(g_key_down, 0, sizeof(g_key_down));
    g_mouse_x = g_mouse_y = 0;
    g_ctrl_c = 0;
}

void gmb_gfx_input_push_key(const char* key)
{
    if (!key || key[0] == '\0') return;
    if (g_key_count >= GMB_INPUT_KEY_CAP) return; /* pieno: si scarta il nuovo */
    char* slot = g_keys[g_key_tail];
    strncpy(slot, key, GMB_INPUT_KEY_MAXLEN - 1);
    slot[GMB_INPUT_KEY_MAXLEN - 1] = '\0';
    g_key_tail = (g_key_tail + 1) % GMB_INPUT_KEY_CAP;
    g_key_count++;
}

int gmb_gfx_input_poll_key(char* out, int maxlen)
{
    if (!out || maxlen <= 0 || g_key_count <= 0) return 0;
    strncpy(out, g_keys[g_key_head], (size_t)maxlen - 1);
    out[maxlen - 1] = '\0';
    g_key_head = (g_key_head + 1) % GMB_INPUT_KEY_CAP;
    g_key_count--;
    return 1;
}

void gmb_gfx_input_push_mouse(int type, int x, int y, int button, int wheel)
{
    if (type == GMB_GFX_MOUSE_MOVE || type == GMB_GFX_MOUSE_DOWN || type == GMB_GFX_MOUSE_UP)
    {
        g_mouse_x = x;
        g_mouse_y = y;
    }
    if (type == GMB_GFX_MOUSE_DOWN) g_mouse_btn_down = 1;
    else if (type == GMB_GFX_MOUSE_UP) g_mouse_btn_down = 0;
    if (type == GMB_GFX_MOUSE_WHEEL) g_wheel_accum += wheel;
    if (g_mouse_count >= GMB_INPUT_MOUSE_CAP) return; /* pieno: si scarta il nuovo */
    gmb_input_mouse_ev_t* ev = &g_mouse[g_mouse_tail];
    ev->type = type; ev->x = x; ev->y = y; ev->button = button; ev->wheel = wheel;
    g_mouse_tail = (g_mouse_tail + 1) % GMB_INPUT_MOUSE_CAP;
    g_mouse_count++;
}

int gmb_gfx_input_poll_mouse(int* type, int* x, int* y, int* button, int* wheel)
{
    if (g_mouse_count <= 0) return 0;
    gmb_input_mouse_ev_t* ev = &g_mouse[g_mouse_head];
    if (type) *type = ev->type;
    if (x) *x = ev->x;
    if (y) *y = ev->y;
    if (button) *button = ev->button;
    if (wheel) *wheel = ev->wheel;
    g_mouse_head = (g_mouse_head + 1) % GMB_INPUT_MOUSE_CAP;
    g_mouse_count--;
    return 1;
}

int gmb_gfx_input_take_ctrl_c(void)
{
    int v = g_ctrl_c;
    g_ctrl_c = 0;
    return v;
}

void gmb_gfx_input_set_ctrl_c(void) { g_ctrl_c = 1; }

int gmb_gfx_input_key_down(int scancode)
{
    if (scancode < 0 || scancode >= GMB_INPUT_SCANCODES) return 0;
    return g_key_down[scancode] ? 1 : 0;
}

void gmb_gfx_input_set_key_down(int scancode, int down)
{
    if (scancode < 0 || scancode >= GMB_INPUT_SCANCODES) return;
    g_key_down[scancode] = down ? 1 : 0;
}

void gmb_gfx_input_get_mouse(int* x, int* y)
{
    if (x) *x = g_mouse_x;
    if (y) *y = g_mouse_y;
}

/* MOUSE.X()/MOUSE.Y(): posizione mouse in PIXEL (parità IlRuntime.MouseX/MouseY; 0 headless). */
double gmb_mouse_x(void) { return (double)g_mouse_x; }
double gmb_mouse_y(void) { return (double)g_mouse_y; }

/* 1 = un tasto del mouse è premuto (per drag/pan del chart). */
int gmb_gfx_input_mouse_down(void) { return g_mouse_btn_down; }

/* Delta rotella accumulato dall'ultimo poll (consuma). */
int gmb_gfx_input_take_wheel(void) { int w = g_wheel_accum; g_wheel_accum = 0; return w; }
