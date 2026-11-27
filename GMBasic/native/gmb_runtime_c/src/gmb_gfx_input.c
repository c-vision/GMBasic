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

/* Touch: stato del dito in attesa di long-press (0 = nessun dito appoggiato). */
static int g_touch_pending;    /* 1 = TOUCH_DOWN accodato, attesa della soglia */
static int g_touch_x, g_touch_y;      /* posizione CORRENTE del gesto (per il long-press) */
static int g_touch_ox, g_touch_oy;    /* origine del gesto: base del confronto slop */
static unsigned int g_touch_ms; /* istante del TOUCH_DOWN */
static int g_touch_hold_ms = GMB_GFX_TOUCH_HOLD_MS;
static int g_touch_longpress_evt; /* 1 = long-press emesso e non ancora letto */

void gmb_gfx_input_reset(void)
{
    g_key_head = g_key_tail = g_key_count = 0;
    g_mouse_head = g_mouse_tail = g_mouse_count = 0;
    memset(g_key_down, 0, sizeof(g_key_down));
    g_mouse_x = g_mouse_y = 0;
    g_mouse_btn_down = 0;
    g_wheel_accum = 0;
    g_touch_pending = 0;
    g_touch_x = g_touch_y = 0;
    g_touch_ox = g_touch_oy = 0;
    g_touch_ms = 0;
    g_touch_longpress_evt = 0;
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
    if (type == GMB_GFX_MOUSE_MOVE || type == GMB_GFX_MOUSE_DOWN || type == GMB_GFX_MOUSE_UP ||
        type == GMB_GFX_TOUCH_DOWN || type == GMB_GFX_TOUCH_UP || type == GMB_GFX_TOUCH_LONGPRESS)
    {
        g_mouse_x = x;
        g_mouse_y = y;
    }
    if (type == GMB_GFX_MOUSE_DOWN || type == GMB_GFX_TOUCH_DOWN) g_mouse_btn_down = 1;
    else if (type == GMB_GFX_MOUSE_UP || type == GMB_GFX_TOUCH_UP || type == GMB_GFX_TOUCH_LONGPRESS) g_mouse_btn_down = 0;
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

/* ---- Touch / long-press ----------------------------------------------------
 * Un dito "appoggiato" e' un TOUCH_DOWN accodato subito (i controlli devono
 * reagire alla pressione come con il mouse). Se resta fermo oltre la soglia,
 * gmb_gfx_input_pump_touch() emette TOUCH_LONGPRESS seguito da TOUCH_UP: il
 * long-press NON lascia il controllo in stato "premuto" (nessun gesto appeso).
 * Un movimento oltre lo slop annulla l'attesa (e' un drag, non un hold). */

void gmb_gfx_input_push_touch(int down, int x, int y, unsigned int now_ms)
{
    if (down)
    {
        g_touch_pending = 1;
        g_touch_x = g_touch_ox = x;
        g_touch_y = g_touch_oy = y;
        g_touch_ms = now_ms;
        gmb_gfx_input_push_mouse(GMB_GFX_TOUCH_DOWN, x, y, 1, 0);
    }
    else
    {
        g_touch_pending = 0; /* dito alzato prima della soglia: nessun long-press */
        gmb_gfx_input_push_mouse(GMB_GFX_TOUCH_UP, x, y, 1, 0);
    }
}

int gmb_gfx_input_touch_move(int x, int y)
{
    /* Movimento del dito: aggiorna la posizione corrente del gesto e accoda un
     * TOUCH_MOVE. NON tocca l'origine usata per lo slop (g_touch_x/y): il
     * confronto origine/posizione lo fa gmb_gfx_input_touch_slop_exceeded(). */
    if (!g_touch_pending) return 0;
    g_touch_x = x;
    g_touch_y = y;
    gmb_gfx_input_push_mouse(GMB_GFX_TOUCH_MOVE, x, y, 0, 0);
    return 1;
}

int gmb_gfx_input_pump_touch(unsigned int now_ms)
{
    if (!g_touch_pending) return 0;
    if (g_touch_hold_ms <= 0) { g_touch_pending = 0; return 0; } /* disabilitato */
    /* Il dito si e' mosso oltre lo slop? Confronto fra ORIGINE del gesto e
     * posizione corrente (aggiornata da gmb_gfx_input_touch_move). Se si e'
     * mosso e' un trascinamento, non un hold: l'attesa si annulla. */
    int dx = g_touch_x - g_touch_ox;
    int dy = g_touch_y - g_touch_oy;
    if (dx > GMB_GFX_TOUCH_HOLD_SLOP || -dx > GMB_GFX_TOUCH_HOLD_SLOP ||
        dy > GMB_GFX_TOUCH_HOLD_SLOP || -dy > GMB_GFX_TOUCH_HOLD_SLOP)
    {
        g_touch_pending = 0; /* trascinamento: niente long-press */
        return 0;
    }
    if (now_ms < g_touch_ms) g_touch_ms = now_ms; /* clock azzerato/avvolto */
    if (now_ms - g_touch_ms < (unsigned int)g_touch_hold_ms) return 0;
    g_touch_pending = 0;
    g_touch_longpress_evt = 1;
    gmb_gfx_input_push_mouse(GMB_GFX_TOUCH_LONGPRESS, g_touch_x, g_touch_y, 1, 0);
    gmb_gfx_input_push_mouse(GMB_GFX_TOUCH_UP, g_touch_x, g_touch_y, 1, 0);
    return 1;
}

void gmb_gfx_input_set_touch_hold_ms(int ms) { g_touch_hold_ms = ms; }
int  gmb_gfx_input_touch_hold_ms(void) { return g_touch_hold_ms; }
int  gmb_gfx_input_touch_down(void) { return g_touch_pending ? 1 : 0; }

int gmb_gfx_input_take_longpress(void) { int v = g_touch_longpress_evt; g_touch_longpress_evt = 0; return v; }

int gmb_gfx_input_touch_to_mouse(int type)
{
    switch (type)
    {
        case GMB_GFX_TOUCH_DOWN: return GMB_GFX_MOUSE_DOWN;
        case GMB_GFX_TOUCH_UP:   return GMB_GFX_MOUSE_UP;
        case GMB_GFX_TOUCH_MOVE: return GMB_GFX_MOUSE_MOVE;
        /* Gia' in pressione dal TOUCH_DOWN: inoltrarlo come secondo evento
         * aprirebbe un falso drag e farebbe perdere il click all'UP. */
        case GMB_GFX_TOUCH_LONGPRESS: return -1;
        default: return type;
    }
}
