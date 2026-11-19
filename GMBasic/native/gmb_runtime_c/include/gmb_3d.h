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
    GMB_3D_E_RECURSIVE = -6,  /* RENDER dentro RENDER */
    GMB_3D_E_SCHEMA    = -7,  /* documento fuori schema gmb-3d v1 */
    GMB_3D_E_INVARIANT = -8   /* invariante W-INV-* violata (load fallito) */
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

/* --- mondi a settori (P2, specifica §2) ---------------------------------- */

/* Handle OPACO: lo stato del mondo vive nel runtime, la matematica nelle
 * funzioni gm_sector_* della suite (piano §3.1.3). */
typedef struct gmb_3d_world gmb_3d_world_t;

typedef struct gmb_3d_camera {
    float x, y, z;          /* occhio */
    float angle, pitch;     /* radianti: angolo di vista e shear dell'orizzonte */
    float fov;              /* radianti, orizzontale */
    float far;
} gmb_3d_camera_t;

/* Carica un mondo a settori da un documento gmb-3d v1 (`kind:"sectors"`).
 * R11: dentro un task il file si legge su un thread esterno e il task PARCA;
 * fuori da un task (tool, oracolo — T3) la lettura è sincrona. Parsing e
 * validazione girano sempre nel chiamante; il load è ATOMICO: su errore il
 * mondo precedente resta valido e *out non viene toccato. */
int  gmb_3d_world_load(const char* path, gmb_3d_world_t** out);
void gmb_3d_world_free(gmb_3d_world_t* w);

/* Rende il mondo nel target. In null mode non disegna (T-NULL). */
int  gmb_3d_world_render(const gmb_3d_world_t* w, const gmb_3d_camera_t* cam,
                         gmb_3d_target_t* t);

/* --- corpo e movimento (P2, specifica §2.3) ------------------------------ */

/* Bit di HIT (W-MOVE): 1 una parete ha spinto, 2 non ci si è mossi affatto,
 * 4 cambio di settore. Accorpabili nello stesso MOVE. */
enum {
    GMB_3D_HIT_WALL   = 1,
    GMB_3D_HIT_STUCK  = 2,
    GMB_3D_HIT_SECTOR = 4
};

/* Corpo del giocatore: cilindro verticale con i piedi in (x,y,z). I default
 * sono quelli di W-PLAYER (raggio 14, altezza 56, occhio 46, step 24);
 * `angle`/`pitch` sono la vista, `sector` il settore corrente (-1 = da
 * collocare). `hit` è il bitmask dell'ultimo MOVE. */
typedef struct gmb_3d_player {
    float x, y, z;          /* piedi nel mondo */
    float angle, pitch;     /* radianti: 0 = +x */
    float radius, height;   /* cilindro di collisione */
    float eye, step;        /* occhio dal pavimento, gradino massimo */
    float fov, far;         /* vista usata da gmb_3d_player_view */
    int   sector;           /* settore corrente, -1 = nessuno */
    int   hit;              /* GMB_3D_HIT_* dell'ultimo MOVE */
} gmb_3d_player_t;

/* Default W-PLAYER; azzera `hit` e mette `sector` a -1. */
void gmb_3d_player_defaults(gmb_3d_player_t* p);

/* Colloca i piedi in (x,y) sul pavimento del settore che contiene il punto
 * (quello con il pavimento più alto, se i settori si sovrappongono). Fuori da
 * ogni settore è un errore esplicito (W-CAMERA), non una posizione di ripiego. */
int  gmb_3d_player_place(const gmb_3d_world_t* w, gmb_3d_player_t* p, float x, float y);

/* MOVE relativo alla vista: `forward` lungo il fronte, `right` verso la destra
 * di chi guarda. Scivola lungo le pareti, sale gradini fino a `step`, entra nei
 * portali in cui il corpo sta, e lascia in p->hit i bit W-MOVE. */
int  gmb_3d_player_move(const gmb_3d_world_t* w, gmb_3d_player_t* p,
                        float forward, float right);

/* Camera dell'occhio del giocatore: z = piedi + eye, limitato a 2 unità sotto
 * il soffitto del settore (W-EYE-CLAMP). */
int  gmb_3d_player_view(const gmb_3d_world_t* w, const gmb_3d_player_t* p,
                        gmb_3d_camera_t* cam);

/* Load che hanno parcheggiato il task (R11): 0 in T3, +1 per ogni load dentro
 * un task. Osservabile come alloc_count per R4. */
unsigned long gmb_3d_load_park_count(void);

const char* gmb_3d_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_3D_H */
