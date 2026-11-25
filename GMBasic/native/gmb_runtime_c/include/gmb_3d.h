/* ============================================================================
 * gmb_3d.h — motore 3D: target di rendering, cattura headless e guardie.
 *
 * P1 del piano 3D (docs/Piano-Motore-3D.md §6) e i mondi a settori di P2. Il
 * TARGET e la sua cattura sono la pipeline che rende verificabile ogni fase;
 * il renderer vero sta nella suite (gm_gfx_sector), qui c'è solo il ponte.
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

/* --- mesh sul target (P3, piano §6) -------------------------------------- */

/* Materiale di un draw. La texture è RGBA8 con la riga 0 in ALTO (convenzione
 * delle immagini, la stessa di gm_tex): il campionamento passa da gm_tex_sample,
 * unica sede del wrap e del filtro. Senza texture (rgba = NULL) il colore del
 * triangolo è `tint`. `light` è il livello 0..255 (255 = pieno): in P3 arriva dal
 * chiamante, in P4 dalle lampade. L'RGB finale è texel x tint x light in
 * aritmetica INTERA (clamp 255): stessi byte su tutti i RID. */
typedef struct gmb_3d_material {
    const unsigned char* rgba;             /* tw*th*4, riga 0 in alto; NULL = piatto */
    int tw, th;
    unsigned char tint_r, tint_g, tint_b;  /* 255 = nessun tint */
    unsigned char light;                   /* 255 = luce piena */
    int bilinear;                          /* 1 = bilineare, 0 = nearest */
    int masked;                            /* 1 = alpha 0 scartato (alpha-test) */
    int two_sided;                         /* 1 = niente back-face cull */
} gmb_3d_material_t;

/* Mesh indicizzata, vertici in coordinate MONDO: pos nv*3, uv nv*2 (NULL quando
 * il materiale non ha texture), idx nt*3. La mesh è del chiamante: qui non si
 * copia nulla e non si alloca nulla per frame (R4). */
typedef struct gmb_3d_mesh {
    const float* pos;
    const float* uv;
    int nv;
    const int* idx;
    int nt;
} gmb_3d_mesh_t;

/* Contatori del pass: `tris_drawn` è ciò che SCENE.DRAWN espone, `tris_culled`
 * conta i triangoli fuori dal frustum o rivolti altrove, `tris_clipped` quelli
 * che il clip ha dovuto tagliare, `pixels` i pixel passati dal depth test. Il
 * costo di un ciclo si legge da qui, non da un cronometro sotto contesa
 * (piano §0septies). */
typedef struct gmb_3d_draw_stats {
    unsigned long tris_drawn;
    unsigned long tris_culled;
    unsigned long tris_clipped;
    unsigned long pixels;
} gmb_3d_draw_stats_t;

/* Rasterizza la mesh nel target (0x00RRGGBB + depth).
 *
 * `mvp` = model-view-projection 4x4 column-major, la stessa convenzione di
 * gm_mat4_perspective/gm_mat4_look_at (clip z in [-1,1], w = distanza di vista).
 * Il clip è fatto in CLIP SPACE contro i sei piani del frustum (|x| <= w,
 * |y| <= w, |z| <= w): near e far non si passano a parte perché stanno già nella
 * matrice, quindi non possono andare fuori sincrono con lei. L'UV è corretto in
 * prospettiva (si interpolano u/w, v/w, 1/w) e la profondità scritta è la
 * DISTANZA DI VISTA con la convenzione di gm_sector_render: 0 = niente
 * disegnato, altrimenti vince la distanza PIU' PICCOLA (più vicino). Così un
 * pass di mesh dopo un pass di settori testa contro la stessa profondità senza
 * conversioni e l'ordine di disegno non conta.
 *
 * Winding: una faccia FRONTALE è CCW vista da fuori (come i generatori di
 * gm_gfx_mesh); con two_sided = 0 le facce inverse sono scartate e contate in
 * tris_culled.
 *
 * In null mode non disegna e azzera i contatori (R8, T-NULL). Errori espliciti,
 * mai un risultato silenzioso: target non allocato, indici fuori range, texture
 * senza UV. */
int gmb_3d_mesh_draw(gmb_3d_target_t* t, const float* mvp,
                     const gmb_3d_mesh_t* m, const gmb_3d_material_t* mat,
                     gmb_3d_draw_stats_t* stats);

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

/* --- tempo e animazioni (P2, specifica §2.4) ----------------------------- */

/* TICK: orologio a PASSO FISSO (T-FIXED). `seconds` si accoda e il mondo
 * avanza a passi di `step` (default 1/60); un solo TICK non accoda più di 10 s,
 * l'eccesso si scarta, così un hitch non fa spirale. Tempo e tick sono simulati:
 * stesso seed e stessi input danno la stessa sequenza di frame (T-CLOCK). */
int           gmb_3d_tick(gmb_3d_world_t* w, double seconds);

double        gmb_3d_time(const gmb_3d_world_t* w);
unsigned long gmb_3d_ticks(const gmb_3d_world_t* w);

/* SECMOVE: porta il pavimento — e con la variante `_ceil` anche il soffitto —
 * alla quota data, a `speed` unità/secondo. */
int gmb_3d_secmove(gmb_3d_world_t* w, int sector, float speed, float floor_z);
int gmb_3d_secmove_ceil(gmb_3d_world_t* w, int sector, float speed,
                        float floor_z, float ceil_z);

/* MOVING: -1 mentre il settore si muove, 0 quando è fermo (T-SECMOVE). */
int gmb_3d_moving(const gmb_3d_world_t* w, int sector);

/* Quote attuali del settore, in SOLA LETTURA (non tocca il mover). */
int gmb_3d_sector_z(const gmb_3d_world_t* w, int sector, float* floor_z, float* ceil_z);

/* HEIGHT: come `sector_z`, e in più ferma il mover (T-SECMOVE). */
int gmb_3d_sector_height(gmb_3d_world_t* w, int sector, float* floor_z, float* ceil_z);

/* ANIM: cicla le picture del settore su `first…first+count−1` a `fps`. */
int gmb_3d_anim(gmb_3d_world_t* w, int sector, int first, int count, float fps);

/* SCROLL: `what` 0 = piani (pavimento e soffitto), 1 = pareti del settore;
 * rate in unità/secondo, integrate dal passo fisso. */
int gmb_3d_scroll(gmb_3d_world_t* w, int sector, int what, float u_per_s, float v_per_s);

/* LIGHTFX: 1 flicker casuale fra `luce−amount` e `luce`, 2 pulse sinusoidale
 * ±amount, 3 strobe a mezzo periodo; 0 spegne l'effetto. `amount` 0..1, `rate`
 * in Hz. Il valore vive in LIGHTNOW. */
int   gmb_3d_lightfx(gmb_3d_world_t* w, int sector, int mode, float amount, float rate);
float gmb_3d_light_now(const gmb_3d_world_t* w, int sector);

/* LAMP: lampada puntiforme che schiarisce con falloff lineare fino a `radius`
 * (W-LIGHT: mai colorata, niente ombre). Fino a GM_SECTOR_MAX_LAMPS per mondo. */
int gmb_3d_lamp(gmb_3d_world_t* w, float x, float y, float z, float radius, float level);

/* --- comandi BASIC del dominio WORLD (specifica §2.4) -------------------- */

/* Il BASIC non maneggia handle: c'è UN mondo corrente per processo, come gli
 * altri domini a stato globale. Ogni funzione è solo la faccia C di una
 * `gmb_3d_*` (nessuna logica qui, §0bis): le usano gli emitter dei comandi.
 *
 * Comandi: WORLD LOAD | CLOSE | TICK, SECMOVE, HEIGHT, ANIM, SCROLL, LIGHTFX,
 *          LAMP. Letture: WORLD.MOVING | LIGHTNOW | TIME | FLOORZ | CEILZ
 *          (funzioni: leggono e basta — non fermano un mover: quello è HEIGHT). */
int    gmb_world_load(const char* path);     /* WORLD LOAD: 0 = OK, <0 errore */
void   gmb_world_free(void);                 /* WORLD CLOSE */
int    gmb_world_tick(double seconds);       /* WORLD TICK */
int    gmb_world_secmove(int sector, double speed, double floor_z);
int    gmb_world_secmove_ceil(int sector, double speed, double floor_z, double ceil_z);
int    gmb_world_height(int sector);         /* WORLD HEIGHT: ferma il mover */
int    gmb_world_anim(int sector, int first, int count, double fps);
int    gmb_world_scroll(int sector, int what, double u_per_s, double v_per_s);
int    gmb_world_lightfx(int sector, int mode, double amount, double rate);
int    gmb_world_lamp(double x, double y, double z, double radius, double level);
double gmb_world_moving(int sector);         /* -1 in moto, 0 fermo */
double gmb_world_lightnow(int sector);
double gmb_world_time(void);
double gmb_world_floor(int sector);
double gmb_world_ceil(int sector);

/* --- SCENE: superficie del renderer mesh (P3) ---------------------------
 *
 * Un solo stato di scena per processo, come WORLD, e nessun handle da
 * maneggiare. Non è una seconda implementazione: la geometria si accumula in
 * gmb_3d_cmd.c e il disegno è `gmb_3d_mesh_draw`, la stessa del kernel. È la
 * faccia unica dei comandi `SCENE` (piano §4.2) e delle `DECLARE NATIVE ...
 * LIB "gmb_3d"`: una implementazione, due modi di chiamarla.
 *
 * Convenzioni: y è l'altezza; i vertici si dichiarano in senso antiorario VISTO
 * DA FUORI (il winding del kernel); la camera ha yaw in gradi (0 = sguardo verso
 * +z) e pitch in gradi (negativo = verso il basso); `fov` è il campo visivo
 * VERTICALE in gradi. Tipo `int64_t`/`double`/`const char*`: è l'ABI di
 * `DECLARE NATIVE` (LONG, DOUBLE, STRING), non un tipo nuovo.
 */
int64_t gmb_scene_reset(void);                       /* azzera geometria e camera */
int64_t gmb_scene_view(int64_t w, int64_t h, double fov_deg);   /* target + fov */
int64_t gmb_scene_checker(int64_t cells);            /* texture a scacchi in scala di grigi */
/* SCENE TEXTURE "file.png": texture da un PNG su disco (RGBA8, riga 0 in alto, la
 * convenzione di gm_tex). Usa `png_decode`, l'unico decoder del runtime: il file si
 * legge una volta e la texture si sostituisce SOLO se il decode riesce (le due forme
 * di SCENE TEXTURE restano atomiche: su errore vale quella precedente). Limite 4096
 * per lato; alpha 0 = trasparente, dal materiale la texture è masked. */
int64_t gmb_scene_texture_file(const char* path);
int64_t gmb_scene_object(int64_t r, int64_t g, int64_t b);      /* apre un oggetto (0..255, clampato) */
int64_t gmb_scene_vertex(double x, double y, double z, double u, double v);
int64_t gmb_scene_tri(int64_t a, int64_t b, int64_t c);
int64_t gmb_scene_quad(int64_t a, int64_t b, int64_t c, int64_t d);
int64_t gmb_scene_box(double x0, double y0, double z0,
                      double x1, double y1, double z1);         /* 24 vertici, 12 triangoli */
int64_t gmb_scene_objpos(double x, double y, double z, double yaw_deg);
int64_t gmb_scene_camera(double x, double y, double z,
                         double yaw_deg, double pitch_deg);
/* SCENE LIGHT dx, dy, dz, livello: luce direzionale per faccia. (dx,dy,dz) punta VERSO
 * la luce (normalizzata qui; nulla = errore), `livello` 0..255 è la luce sulle facce
 * rivolte alla sorgente. Il coseno si quantizza in un intero 0..255 e la miscela
 * ambiente(40)→livello è intera: stessi byte su tutte le gambe. Con LIGHT attiva la
 * luce per faccia sostituisce la tabella fissa delle 6 facce del cubo. */
int64_t gmb_scene_light(double dx, double dy, double dz, int64_t level);
/* SCENE SMOOTH|ALPHA|TWOSIDED on|off (idea #5): espongono i flag già presenti in
 * `gmb_3d_material_t` del kernel — bilineare, alpha-test, niente back-face cull. `on` != 0
 * accende. I default (1, 1, 0) riproducono il comportamento storico, quindi le scene che
 * non li chiamano danno gli stessi byte. Stato del materiale: sopravvive a RESET. */
int64_t gmb_scene_smooth(int64_t on);
int64_t gmb_scene_alpha(int64_t on);
int64_t gmb_scene_twosided(int64_t on);
/* SCENE PRESENT: porta il TARGET 3D nella pagina 2D corrente (canvas attivo, altrimenti
 * pagina dello schermo), copia 1:1 dall'angolo in alto a sinistra senza scalatura. È
 * l'unico ponte verso la finestra: il present vero (SDL) resta al pump sul MAIN thread,
 * qui si scrive solo memoria. GPM: con lo scheduler attivo vale solo dentro un task,
 * altrimenti E_NO_TASK; senza scheduler (tool/oracolo) è usabile sincronamente.
 * Errori: nessun frame (E_ARG), nessuno schermo 2D attivo (E_ARG), nessuna pagina (E_ARG). */
int64_t gmb_scene_present(void);
/* Guardia GPM di PRESENT, separata dalla copia: serve alle gambe in cui il modulo 3D e il
 * modulo grafico sono DUE immagini diverse (IL/PCode: dylib `gmb_3d` e `gmb_gfx`; VM/C):
 * lì il frame si prende con `gmb_scene_frame` e la copia la fa `gmb_gfx_present_argb` nel
 * modulo che possiede la pagina. OK = si può presentare, E_NO_TASK = fuori dal task GPM,
 * E_ARG = nessun frame. */
int64_t gmb_scene_present_check(void);
/* Frame corrente del target 3D (0x00RRGGBB, riga 0 in alto) e sue dimensioni. NULL quando
 * non c'è ancora un target (manca VIEW/RENDER). Sola lettura: nessuna copia, nessun lock. */
const uint32_t* gmb_scene_frame(int* w, int* h);
int64_t gmb_scene_render(void);                      /* rasterizza nel target */
int64_t gmb_scene_save(const char* path);            /* PNG del frame corrente */
int64_t gmb_scene_verts(void);                       /* geometria dichiarata */
int64_t gmb_scene_tris(void);
int64_t gmb_scene_drawn(void);                       /* triangoli rasterizzati */
int64_t gmb_scene_culled(void);
int64_t gmb_scene_pixels(void);
int64_t gmb_scene_hash(void);                        /* FNV-1a del frame (0..2^32-1) */
int64_t gmb_scene_pixel(int64_t x, int64_t y);       /* 0x00RRGGBB, -1 fuori target */

/* Load che hanno parcheggiato il task (R11): 0 in T3, +1 per ogni load dentro
 * un task. Osservabile come alloc_count per R4. */
unsigned long gmb_3d_load_park_count(void);

const char* gmb_3d_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_3D_H */
