/* gmb_gfx_input_selfcheck.c — verifica del modulo input grafico (headless).
 * Copre: FIFO tasti, overflow, eventi mouse + stato, key-down, Ctrl+C, reset. */
#include "gmb_gfx_input.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
    char key[64];
    int type = 0, x = 0, y = 0, button = 0, wheel = 0;

    gmb_gfx_input_reset();

    /* FIFO tasti: ordine di arrivo preservato, UTF-8 compreso. */
    gmb_gfx_input_push_key("a");
    gmb_gfx_input_push_key("\x1b");
    gmb_gfx_input_push_key("è");
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 1 && strcmp(key, "a") == 0, "primo tasto");
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 1 && strcmp(key, "\x1b") == 0, "secondo tasto");
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 1 && strcmp(key, "è") == 0, "tasto UTF-8");
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 0, "coda vuota");
    gmb_gfx_input_push_key("");
    gmb_gfx_input_push_key(NULL);
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 0, "chiavi vuote ignorate");

    /* Overflow: la coda non supera la capacita' e non corrompe l'ordine. */
    for (int i = 0; i < 300; i++) gmb_gfx_input_push_key("k");
    int n = 0;
    while (gmb_gfx_input_poll_key(key, sizeof(key))) n++;
    CHECK(n == 256, "capacita' coda tasti = 256");

    /* Mouse: move aggiorna la posizione; down/up e wheel in ordine. */
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_MOVE, 40, 30, 0, 0);
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, 40, 30, 1, 0);
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_UP, 41, 31, 1, 0);
    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_WHEEL, 41, 31, 0, 1);
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_MOUSE_MOVE && x == 40 && y == 30, "mouse move");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_MOUSE_DOWN && button == 1, "mouse down (bottone SDL)");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_MOUSE_UP && x == 41 && y == 31, "mouse up");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_MOUSE_WHEEL && wheel == 1, "mouse wheel");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 0, "coda mouse vuota");
    gmb_gfx_input_get_mouse(&x, &y);
    CHECK(x == 41 && y == 31, "ultima posizione mouse");

    /* Stato fisico tasti (scancode). */
    gmb_gfx_input_set_key_down(42, 1);
    CHECK(gmb_gfx_input_key_down(42) == 1, "key down");
    gmb_gfx_input_set_key_down(42, 0);
    CHECK(gmb_gfx_input_key_down(42) == 0, "key up");
    gmb_gfx_input_set_key_down(-1, 1);
    gmb_gfx_input_set_key_down(9999, 1);
    CHECK(gmb_gfx_input_key_down(-1) == 0 && gmb_gfx_input_key_down(9999) == 0, "scancode fuori range");

    /* Ctrl+C: flag consumabile una sola volta. */
    CHECK(gmb_gfx_input_take_ctrl_c() == 0, "ctrl-c assente");
    gmb_gfx_input_set_ctrl_c();
    CHECK(gmb_gfx_input_take_ctrl_c() == 1, "ctrl-c presente");
    CHECK(gmb_gfx_input_take_ctrl_c() == 0, "ctrl-c consumato");

    /* Reset completo. */
    gmb_gfx_input_push_key("z");
    gmb_gfx_input_set_ctrl_c();
    gmb_gfx_input_reset();
    CHECK(gmb_gfx_input_poll_key(key, sizeof(key)) == 0, "reset tasti");
    CHECK(gmb_gfx_input_take_ctrl_c() == 0, "reset ctrl-c");

    /* ---- Touch / long-press ---------------------------------------------- */
    gmb_gfx_input_reset();
    CHECK(gmb_gfx_input_touch_down() == 0, "touch: nessun dito all'avvio");

    /* Tap: down+up prima della soglia -> solo TOUCH_DOWN/TOUCH_UP, niente long-press. */
    gmb_gfx_input_push_touch(1, 10, 20, 1000);
    CHECK(gmb_gfx_input_touch_down() == 1, "touch: dito appoggiato");
    CHECK(gmb_gfx_input_mouse_down() == 1, "touch: down = pressione (drag ok)");
    CHECK(gmb_gfx_input_pump_touch(1200) == 0, "touch: sotto soglia, nessun long-press");
    gmb_gfx_input_push_touch(0, 10, 20, 1250);
    CHECK(gmb_gfx_input_touch_down() == 0, "touch: dito alzato");
    CHECK(gmb_gfx_input_mouse_down() == 0, "touch: up = rilascio");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_TOUCH_DOWN && x == 10 && y == 20 && button == 1, "touch down in coda");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_TOUCH_UP, "touch up in coda");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 0, "tap: nessun long-press");

    /* Hold: dito fermo oltre la soglia -> LONGPRESS + UP (gesto chiuso, non appeso). */
    gmb_gfx_input_push_touch(1, 50, 60, 2000);
    CHECK(gmb_gfx_input_pump_touch(2300) == 0, "hold: ancora sotto soglia");
    CHECK(gmb_gfx_input_pump_touch(2600) == 1, "hold: oltre soglia -> emette");
    CHECK(gmb_gfx_input_touch_down() == 0, "hold: long-press chiude il gesto");
    CHECK(gmb_gfx_input_mouse_down() == 0, "hold: nessun controllo resta premuto");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_TOUCH_DOWN, "hold: down in coda");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_TOUCH_LONGPRESS && x == 50 && y == 60, "hold: long-press in coda");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_TOUCH_UP, "hold: up di chiusura in coda");
    CHECK(gmb_gfx_input_pump_touch(3000) == 0, "hold: long-press emesso una sola volta");

    /* Drag: il dito si sposta oltre lo slop -> nessun long-press. */
    gmb_gfx_input_push_touch(1, 100, 100, 4000);
    CHECK(gmb_gfx_input_touch_move(100 + GMB_GFX_TOUCH_HOLD_SLOP + 5, 100) == 1, "drag: move accodato");
    CHECK(gmb_gfx_input_pump_touch(5000) == 0, "drag: movimento annulla il long-press");
    CHECK(gmb_gfx_input_touch_down() == 0, "drag: niente gesto in attesa di long-press");
    CHECK(gmb_gfx_input_mouse_down() == 1, "drag: la pressione resta (drag in corso)");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_TOUCH_DOWN, "drag: down in coda");
    CHECK(gmb_gfx_input_poll_mouse(&type, &x, &y, &button, &wheel) == 1
          && type == GMB_GFX_TOUCH_MOVE && x == 100 + GMB_GFX_TOUCH_HOLD_SLOP + 5,
          "drag: il movimento e' TOUCH_MOVE (non un mouse-move)");
    gmb_gfx_input_push_touch(0, 200, 100, 5100);
    CHECK(gmb_gfx_input_mouse_down() == 0, "drag: up chiude la pressione");

    /* Movimento DENTRO lo slop: il long-press scatta lo stesso. */
    gmb_gfx_input_push_touch(1, 10, 10, 6000);
    CHECK(gmb_gfx_input_touch_move(12, 11) == 1, "micro-movimento: accodato");
    CHECK(gmb_gfx_input_pump_touch(6600) == 1, "micro-movimento: long-press resta valido");
    CHECK(gmb_gfx_input_touch_move(5, 5) == 0, "move senza dito appoggiato: ignorato");

    /* Soglia configurabile e disattivabile. */
    gmb_gfx_input_set_touch_hold_ms(100);
    CHECK(gmb_gfx_input_touch_hold_ms() == 100, "soglia impostata");
    gmb_gfx_input_push_touch(1, 5, 5, 7000);
    CHECK(gmb_gfx_input_pump_touch(7100) == 1, "soglia 100ms rispettata");
    gmb_gfx_input_set_touch_hold_ms(0);
    gmb_gfx_input_push_touch(1, 5, 5, 8000);
    CHECK(gmb_gfx_input_pump_touch(99999) == 0, "soglia 0 = long-press disabilitato");
    gmb_gfx_input_push_touch(0, 5, 5, 9000);

    /* Clock che va indietro (wrap/azzeramento): non deve emettere subito. */
    gmb_gfx_input_set_touch_hold_ms(GMB_GFX_TOUCH_HOLD_MS);
    gmb_gfx_input_push_touch(1, 7, 7, 5000);
    CHECK(gmb_gfx_input_pump_touch(10) == 0, "clock riavvolto: nessun long-press immediato");
    CHECK(gmb_gfx_input_touch_down() == 1, "clock riavvolto: dito ancora giu'");

    /* Normalizzatore touch->mouse (forma condivisa dai consumatori mouse-only). */
    CHECK(gmb_gfx_input_touch_to_mouse(GMB_GFX_TOUCH_DOWN) == GMB_GFX_MOUSE_DOWN, "norm: TOUCH_DOWN -> MOUSE_DOWN");
    CHECK(gmb_gfx_input_touch_to_mouse(GMB_GFX_TOUCH_UP) == GMB_GFX_MOUSE_UP, "norm: TOUCH_UP -> MOUSE_UP");
    CHECK(gmb_gfx_input_touch_to_mouse(GMB_GFX_TOUCH_MOVE) == GMB_GFX_MOUSE_MOVE, "norm: TOUCH_MOVE -> MOUSE_MOVE");
    CHECK(gmb_gfx_input_touch_to_mouse(GMB_GFX_TOUCH_LONGPRESS) == -1, "norm: LONGPRESS assorbito");
    CHECK(gmb_gfx_input_touch_to_mouse(GMB_GFX_MOUSE_DOWN) == GMB_GFX_MOUSE_DOWN, "norm: mouse invariato");
    CHECK(gmb_gfx_input_touch_to_mouse(GMB_GFX_MOUSE_WHEEL) == GMB_GFX_MOUSE_WHEEL, "norm: wheel invariata");

    /* Long-press osservabile: consumabile una sola volta. */
    gmb_gfx_input_reset();
    CHECK(gmb_gfx_input_take_longpress() == 0, "lp: nessun long-press iniziale");
    gmb_gfx_input_push_touch(1, 3, 3, 1000);
    gmb_gfx_input_pump_touch(1000 + GMB_GFX_TOUCH_HOLD_MS + 1);
    CHECK(gmb_gfx_input_take_longpress() == 1, "lp: long-press osservabile");
    CHECK(gmb_gfx_input_take_longpress() == 0, "lp: evento consumato");
    CHECK(gmb_gfx_input_take_longpress() == 0, "lp: nessun secondo evento");

    /* Reset azzera anche lo stato touch. */
    gmb_gfx_input_reset();
    CHECK(gmb_gfx_input_touch_down() == 0, "reset touch");
    CHECK(gmb_gfx_input_take_longpress() == 0, "reset long-press");

    if (fails == 0) { printf("gmb_gfx_input_selfcheck: OK\n"); return 0; }
    printf("gmb_gfx_input_selfcheck: %d FAIL\n", fails);
    return 1;
}
