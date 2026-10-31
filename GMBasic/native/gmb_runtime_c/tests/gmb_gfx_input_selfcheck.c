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

    if (fails == 0) { printf("gmb_gfx_input_selfcheck: OK\n"); return 0; }
    printf("gmb_gfx_input_selfcheck: %d FAIL\n", fails);
    return 1;
}
