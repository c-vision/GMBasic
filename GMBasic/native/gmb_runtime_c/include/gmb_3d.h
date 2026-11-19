/* ============================================================================
 * gmb_3d.h — motore 3D: target di rendering, cattura headless e guardie.
 *
 * P1 del piano 3D (docs/Piano-Motore-3D.md §6). Non c'è ancora un renderer in
 * questo modulo: ci sono il TARGET e la sua cattura, cioè la pipeline che rende
 * verificabile ogni fase successiva.
 *
 * Contratto:
 *  - il fotogramma si legge dal TARGET 3D, mai da gmb_gfx_present_capture():
 *    quella richiede video reale e main thread e in headless restituisce -1
 *    (docs/Nota-Tecnica-Thread-Present-3D.md §7);
 *  - il target contiene word 0x00RRGGBB, lo stesso formato delle pagine 2D:
 *    il passaggio alle pagine è una copia con & 0xFFFFFF, senza conversioni;
 *  - NULL mode: RENDER aggiorna lo stato ma non disegna; il target resta
 *    intatto e non si alloca nulla per frame;
 *  - guardie GPM (nota §3): se lo scheduler è attivo (gmb_c99gpm_active()),
 *    toccare il renderer fuori da un task è un errore; senza scheduler (tool,
 *    oracolo, selfcheck — T3) la stessa API è usabile sincronamente, come
 *    gmb_3d_io farà per il load. I riferimenti a GPM sono FORTI: un binario
 *    che non linka lo scheduler non si costruisce, quindi la guardia non può
 *    diventare inerte per sbaglio.
 *
 * Nessuna variabile d'ambiente: dimensioni e modi passano dagli argomenti.
 * ========================================================================== */
#ifndef GMB_3D_H
#define GMB_3D_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- esiti (negativi = errore, 0 = OK) ---------------------------------- */
enum {
    GMB_3D_OK          = 0,
    GMB_3D_E_ARG       = -1,  /* argomento non valido */
    GMB_3D_E_NOMEM     = -2,  /* allocazione fallita */
    GMB_3D_E_IO        = -3,  /* scrittura/lettura file fallita */
    GMB_3D_E_NO_TASK   = -4,  /* fuori dal task GPM con scheduler attivo */
    GMB_3D_E_BUSY      = -5,  /* il renderer è di un altro task */
    GMB_3D_E_RECURSIVE = -6   /* RENDER dentro RENDER */
};

/* Present: disegna nel target (software) oppure non disegna (null mode). */
typedef enum {
    GMB_3D_MODE_TARGET = 0,
    GMB_3D_MODE_NULL   = 1
} gmb_3d_mode_t;

/* Target di rendering: colore 0x00RRGGBB + profondità. Riusabile: allocare con
 * la stessa dimensione non rialloca (R4: nessuna allocazione per frame). */
typedef struct gmb_3d_target {
    int w, h;
    uint32_t* argb;   /* w*h, 0x00RRGGBB (alpha 0, come le pagine 2D) */
    float* depth;     /* w*h, valori piccoli = vicino */
} gmb_3d_target_t;

/* Alloca (o riusa) il target. w,h > 0. */
int  gmb_3d_target_alloc(gmb_3d_target_t* t, int w, int h);
void gmb_3d_target_free(gmb_3d_target_t* t);

/* Colora tutto il target (argb mascherato a 0x00RRGGBB) e riempie la profondità. */
int  gmb_3d_target_clear(gmb_3d_target_t* t, uint32_t argb, float depth);

/* Copia un fotogramma ARGB (w*h, 0x00RRGGBB) nel target. Serve al ponte con il
 * raster della suite, che produce ARGB con gm_raster_to_argb. Dimensioni
 * identiche: nessuna scalatura implicita. In NULL mode non scrive (ritorna OK). */
int  gmb_3d_target_blit_argb(gmb_3d_target_t* t, const uint32_t* src, int w, int h);

/* Cattura dal TARGET (non dalla finestra): copia fino a cap_px pixel in out
 * (le dimensioni sono t->w × t->h, nessuna scalatura). Ritorna il numero di
 * pixel copiati, negativo su errore. */
int  gmb_3d_target_capture_argb(const gmb_3d_target_t* t, uint32_t* out, int cap_px);

/* Cattura dal TARGET e scrive un PNG RGB24 (formato delle golden del piano). */
int  gmb_3d_target_capture_png(const gmb_3d_target_t* t, const char* path);

/* --- present / guardie di render ---------------------------------------- */
void          gmb_3d_present_mode_set(gmb_3d_mode_t mode);
gmb_3d_mode_t gmb_3d_present_mode(void);

/* Apre/chiude una sessione di render. Errori espliciti (mai attese):
 *  E_NO_TASK    fuori da un task mentre lo scheduler GPM è attivo
 *  E_RECURSIVE  stessa entità che rientra (R5)
 *  E_BUSY       un altro task possiede il renderer (R3: rifiuto, non lock)
 * RENDER END da un non-proprietario non tocca il claim di un proprietario
 * VIVO (R3); se invece il proprietario non può più chiuderlo (task finito
 * senza END, claim nato prima dello scheduler, scheduler spento) libera il
 * claim: il renderer non resta mai occupato per sempre. */
int  gmb_3d_render_begin(void);
void gmb_3d_render_end(void);
int  gmb_3d_render_depth(void);   /* 1 se un claim è aperto, 0 se libero */

/* Buffer allocati dal modulo dall'avvio: un ciclo non deve incrementarlo
 * (equivale a SCENE.STATS("allocs") = 0 per frame). */
unsigned long gmb_3d_alloc_count(void);

const char* gmb_3d_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_3D_H */
