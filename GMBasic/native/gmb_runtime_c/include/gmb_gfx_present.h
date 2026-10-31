/* ============================================================================
 * gmb_gfx_present.h — Presentazione SDL opzionale del framebuffer gmb_graphics.
 *
 * gmb_graphics.c (SCREEN/CLS/PSET/...) e' puro software, senza dipendenza SDL:
 * disegna in un framebuffer in memoria (g_gfx.pages[]) ma non lo mostra mai a
 * schermo. Questo modulo e' il pezzo mancante: crea una vera finestra SDL e vi
 * copia il framebuffer, quando SDL2 e' disponibile (compilato solo se CMake lo
 * trova, vedi gmb_runtime_c/CMakeLists.txt — GMB_GFX_SDL_PRESENT).
 *
 * Vincolo di thread: SDL/Cocoa impone che TUTTE le chiamate a questo modulo
 * (start/tick/stop) avvengano sul vero thread main del processo — mai da un
 * task worker della VM GPM (crash nativo garantito su macOS altrimenti, stessa
 * causa gia' riscontrata e risolta lato backend IL C#, vedi
 * IlRuntime.RunGpmDriverWithGraphicsPump). Il chiamante (gmb_vm_bridge_run) deve
 * eseguire la VM su un thread worker dedicato e pompare questo modulo in loop
 * sul thread chiamante originale.
 * ============================================================================ */
#ifndef GMB_GFX_PRESENT_H
#define GMB_GFX_PRESENT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Inizializza SDL_INIT_VIDEO (idempotente). Non crea ancora la finestra: la
 * finestra si crea/ridimensiona pigramente al primo gmb_gfx_present_tick() con
 * uno schermo grafico attivo (gmb_gfx_visible_pixels() non-NULL). */
void gmb_gfx_present_start(void);

/* Da chiamare ripetutamente in loop sul thread main (es. ogni ~16ms) mentre la
 * VM gira su un thread worker: pompa gli eventi SDL e, se lo schermo grafico e'
 * attivo, ricopia il framebuffer corrente sulla finestra. No-op sicuro se lo
 * schermo non e' (ancora) attivo. */
void gmb_gfx_present_tick(void);

/* L'utente ha chiuso la finestra (SDL_QUIT / click sulla X) — il chiamante deve
 * interrompere l'attesa della VM (la VM stessa continua finche' non termina). */
int gmb_gfx_present_should_quit(void);
void gmb_gfx_present_close_window(void);
void gmb_gfx_present_request_quit(void);

/* Distrugge finestra/renderer/texture e SDL_QuitSubSystem(SDL_INIT_VIDEO) se
 * questo modulo aveva inizializzato SDL. */
void gmb_gfx_present_stop(void);
void gmb_gfx_window_set_visible(int on);  /* SDL main-thread: show/hide finestra */

/* 1 se il thread corrente è quello che ha chiamato present_start (il "main"
 * thread richiesto da SDL/Cocoa). Senza start registrato ritorna 1. */
int gmb_gfx_present_on_main_thread(void);

/* SOLO PER TEST: processa un singolo evento SDL come farebbe gmb_gfx_present_tick
 * (stessa traduzione verso gmb_gfx_input). Evita la coda eventi SDL quando il
 * driver (es. dummy con sdl2-compat) non la supporta in modo affidabile. */
void gmb_gfx_present_process_event(const void* sdl_event);

/* ── Clipboard di sistema (SDL) ── */
void gmb_gfx_clipboard_set(const char* text);
/* Copia il testo negli appunti in out (NUL-terminato); ritorna la lunghezza. */
int  gmb_gfx_clipboard_get(char* out, int maxlen);
void gmb_gfx_clipboard_clear(void);

/* ── Cursore / puntatore (main thread, come il resto del modulo) ── */
/* Mostra/nasconde il cursore di sistema (SDL_ShowCursor). */
void gmb_gfx_present_show_cursor(int visible);
/* Sposta il cursore alla coordinata SCHERMO (pixel del framebuffer), clampata al
 * limite attivo; mappa alla finestra e chiama SDL_WarpMouseInWindow. */
void gmb_gfx_present_warp_mouse(int x, int y);
/* Imposta il cursore di sistema (id = SDL_SystemCursor: 0=arrow, 2=hand, 5=ibeam...). */
void gmb_gfx_present_set_cursor(int cursor_id);
/* Limita il movimento del mouse al rettangolo SCHERMO (0,0,0,0 = reset). */
void gmb_gfx_present_mouse_limit(int x1, int y1, int x2, int y2);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GFX_PRESENT_H */
