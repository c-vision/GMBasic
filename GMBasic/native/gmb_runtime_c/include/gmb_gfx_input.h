/* ============================================================================
 * gmb_gfx_input.h — stato input tastiera/mouse del motore grafico unico.
 *
 * La pump SDL (gmb_gfx_present_sdl.c, SOLO main thread) alimenta questo modulo
 * con gli eventi tradotti; il mondo gestito (bridge C#) li drena via P/Invoke:
 *   - tasti  -> coda FIFO di stringhe UTF-8 (INKEY/WAITKEY/GET/ON KEY)
 *   - mouse  -> coda FIFO di eventi (move/down/up/wheel) + stato corrente
 *   - Ctrl+C -> flag consumabile (la decisione "intercettato o abort" e' del
 *               runtime gestito, che conosce ON KEY)
 *
 * Headless-safe: senza SDL le code restano vuote; l'iniezione (test/CI) e'
 * sempre disponibile e non richiede video.
 * ============================================================================ */
#ifndef GMB_GFX_INPUT_H
#define GMB_GFX_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Tipi evento mouse (parita' con i DispatchMouse del runtime gestito). */
#define GMB_GFX_MOUSE_MOVE  1
#define GMB_GFX_MOUSE_DOWN  2
#define GMB_GFX_MOUSE_UP    3
#define GMB_GFX_MOUSE_WHEEL 4

/* Iniezione (test/CI/headless): accoda un tasto UTF-8 non vuoto. */
void gmb_gfx_input_push_key(const char* key);
/* Iniezione mouse: type = GMB_GFX_MOUSE_*; button = numero SDL (1=L,2=M,3=R);
 * wheel = delta (solo WHEEL). Le coordinate sono PIXEL SCHERMO (gia' mappate). */
void gmb_gfx_input_push_mouse(int type, int x, int y, int button, int wheel);

/* Poll tasti (main thread): 1 = letto in out (NUL-terminato), 0 = coda vuota. */
int gmb_gfx_input_poll_key(char* out, int maxlen);
/* Poll eventi mouse (main thread): 1 = letto. wheel vale solo per WHEEL. */
int gmb_gfx_input_poll_mouse(int* type, int* x, int* y, int* button, int* wheel);

/* Ctrl+C premuto dall'ultimo poll (consuma il flag). 1 = premuto. */
int gmb_gfx_input_take_ctrl_c(void);
/* Segnala Ctrl+C (usato dalla pump SDL; esposto per i test). */
void gmb_gfx_input_set_ctrl_c(void);

/* Stato fisico tasto per scancode SDL (0..511). 1 = premuto. */
int  gmb_gfx_input_key_down(int scancode);
/* Aggiorna lo stato fisico (pump SDL / test). */
void gmb_gfx_input_set_key_down(int scancode, int down);

/* Ultima posizione mouse nota (pixel schermo) — per eventi WHEEL senza coord. */
void gmb_gfx_input_get_mouse(int* x, int* y);

/* 1 = un tasto del mouse è premuto (per drag/pan del chart). */
int gmb_gfx_input_mouse_down(void);

/* Delta rotella accumulato dall'ultimo poll (consuma): per zoom del chart. */
int gmb_gfx_input_take_wheel(void);

/* Azzera code/stato (present_stop o nuovo SCREEN). */
void gmb_gfx_input_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GFX_INPUT_H */
