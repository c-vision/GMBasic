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
    /* Buffer di ID per pixel (SCENE PICK), ADDITIVO e opzionale: NULL = spento,
     * nessuna allocazione e nessuna scrittura, quindi gli stessi byte di sempre.
     * Acceso, ogni pixel che vince il depth test porta l'id dell'oggetto che l'ha
     * scritto (1..256; 0 = fondo/cielo/thing: nessun oggetto mesh). */
    uint32_t* id;     /* w*h, NULL quando il pick e' spento */
} gmb_3d_target_t;

/* Alloca (o riusa) il target. w,h > 0. */
int  gmb_3d_target_alloc(gmb_3d_target_t* t, int w, int h);
void gmb_3d_target_free(gmb_3d_target_t* t);

/* Accende/spegne il buffer di ID del target (SCENE PICK). `on` != 0 alloca (o
 * riusa) w*h word e lo azzera; 0 lo libera e lo mette a NULL. Additivo: colore
 * e profondita' non cambiano, e chi non lo accende non paga ne' memoria ne'
 * scritture. Errori espliciti (target non allocato, memoria). */
int  gmb_3d_target_ids(gmb_3d_target_t* t, int on);

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
    /* P4 (M-FLAGS, M-PALALPHA): oggetto traslucido. Il pixel si miscela con quello
     * gia' scritto: alpha 255 del texel = meta' e meta', alpha < 255 = quella
     * frazione; senza texture il blend e' 50/50. Masked e opachi ignorano l'alpha. */
    int translucent;
    /* SSAA 2x (SCENE AA on): il DRAW va in un target interno doppio, allineato a
     * 1/4 e 3/4 del pixel, e si risolve con una media 2x2 UNA volta per draw (i
     * triangoli che condividono un bordo non si fondono due volte). I campioni non
     * coperti valgono il colore gia' nel pixel: il bordo si fonde col fondo. 0 =
     * un campione al centro e nessun target doppio: gli stessi byte di sempre. */
    int aa;
    /* ID dell'oggetto per il buffer di pick (SCENE PICK): 0 = nessun id. Si
     * scrive solo se il TARGET ha il buffer acceso; altrimenti e' ignorato. */
    unsigned id;
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
    /* Normali per vertice (nv*3) nello spazio dell'OGGETTO, FACOLTATIVE: ruotano
     * con OBJPOS come la geometria. NULL = nessuna: la luce delle
     * lampade non dipende dall'orientamento e il risultato e' quello di P4, byte per
     * byte (le golden restano valide). Presenti = N.L per lampada (M-LIGHT-N).
     * In coda alla struct per non rompere gli inizializzatori posizionali esistenti. */
    const float* nrm;
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

/* --- P4: luce, lampade, fog, cielo, thing (specifica §M-*) ---------------- */

#define GMB_3D_MAX_LAMPS 8           /* M-LIMITS: otto lampade per scena */

/* Lampada (M-LIGHT). Puntiforme quando `dir` e' nulla; con direzione e
 * `half_angle` > 0 e' un cono: pieno sull'asse, zero al bordo. Solo schiarisce,
 * mai colorata. `radius` <= 0 o `amount` = 0 = spenta. */
typedef struct gmb_3d_lamp {
    float x, y, z;
    float radius;
    float dir[3];        /* asse del cono (puntata); 0,0,0 = puntiforme */
    float half_angle;    /* radianti, 0..pi/2; usato solo se dir non e' nulla */
    unsigned char amount;/* 0..255: quanto schiarisce sull'asse */
} gmb_3d_lamp_t;

/* Ambiente di una scena (P4). `fog` = distanza a cui si arriva al buio (0 =
 * spento, M-FOG), `far` = oltre questa distanza i triangoli si saltano (0 =
 * spento), `colormap` = luce quantizzata a 32 livelli (C-SAME-COLORMAP). */
typedef struct gmb_3d_env {
    float fog;
    float far;
    int   colormap;
    int   nlamps;
    gmb_3d_lamp_t lamps[GMB_3D_MAX_LAMPS];
} gmb_3d_env_t;

/* Azzera l'ambiente (fog/far spenti, nessuna lampada, colormap continua). */
void gmb_3d_env_clear(gmb_3d_env_t* env);

/* Luce finale 0..255 in un punto del MONDO (M-LIGHT): `base` (luce dell'oggetto)
 * + somma delle lampade, poi fog sulla distanza di vista `dist`, poi la
 * quantizzazione a 32 livelli se `colormap`. Sola lettura: serve ai MiniTest e
 * al rasterizzatore, che chiamano la stessa funzione (una sola formula). */
unsigned char gmb_3d_light_eval(const gmb_3d_env_t* env, float x, float y, float z,
                                unsigned char base, float dist);

/* Come gmb_3d_light_eval, con la normale del punto (M-LIGHT-N): ogni lampada
 * contribuisce solo se la superficie la guarda (`max(0, N.L)`, L verso la lampada).
 * La normale viene normalizzata qui; lunghezza ~0 = nessuna normale, quindi lo
 * stesso risultato di gmb_3d_light_eval. E' la STESSA formula: le due funzioni
 * chiamano un solo nucleo interno. */
unsigned char gmb_3d_light_eval_n(const gmb_3d_env_t* env, float x, float y, float z,
                                  const float n[3], unsigned char base, float dist);

/* Come gmb_3d_mesh_draw, con l'ambiente: luce per pixel (M-LIGHT), fog (M-FOG)
 * e FAR (i triangoli interamente oltre la distanza si scartano). Con env NULL o
 * spento produce gli STESSI byte di gmb_3d_mesh_draw: nessuna regressione sulle
 * golden di P3. `translucent` nel materiale miscela invece di copiare. */
int gmb_3d_mesh_draw_env(gmb_3d_target_t* t, const float* mvp,
                         const gmb_3d_mesh_t* m, const gmb_3d_material_t* mat,
                         const gmb_3d_env_t* env, gmb_3d_draw_stats_t* stats);

/* Cielo (M-SKY): riempie i pixel NON toccati (depth 0) mappando la picture per
 * angolo orizzontale attorno all'osservatore. `repeat` = quante volte la picture
 * copre i 360 (default 1), `shift` = sfasamento in giri. La riga 0 della texture
 * resta in alto; `fov_deg` e' il campo visivo verticale della camera. */
int gmb_3d_sky_fill(gmb_3d_target_t* t, const unsigned char* rgba, int tw, int th,
                    float yaw_deg, float pitch_deg, float fov_deg,
                    float repeat, float shift);

/* Thing (M-THINGS): billboard verticale con la base in (x,y,z), sempre rivolto
 * all'osservatore, disegnato con depth test. `eye` e' la posizione della camera.
 * La picture e' RGBA8 (riga 0 in alto); alpha 0 = trasparente. */
typedef struct gmb_3d_thing {
    float x, y, z;          /* base del billboard */
    float w, h;             /* dimensioni nel mondo */
    const unsigned char* rgba;
    int tw, th;
    unsigned char light;    /* 0..255 */
} gmb_3d_thing_t;

int gmb_3d_thing_draw(gmb_3d_target_t* t, const float* view, const float* proj,
                      const gmb_3d_thing_t* th, const float eye[3],
                      const gmb_3d_env_t* env, gmb_3d_draw_stats_t* stats);

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
/* SCENE OBJECT r, g, b [, flags [, luce]]: apre un oggetto (0..255, clampato).
 * `flags` (M-FLAGS): 1 masked, 2 traslucido (disegnato per ultimo, da lontano a
 * vicino), 16 due lati; default 0. `luce` 0..255 e' la luce base dell'oggetto,
 * a cui si sommano le lampade (default 255). */
int64_t gmb_scene_object(int64_t r, int64_t g, int64_t b);
int64_t gmb_scene_object_flags(int64_t r, int64_t g, int64_t b, int64_t flags, int64_t light);
int64_t gmb_scene_vertex(double x, double y, double z, double u, double v);
/* VERTEX a 8 argomenti: come sopra, con la normale MONDO del vertice (M-LIGHT-N).
 * In un oggetto le normali ci sono per TUTTI i vertici o per nessuno; mescolarle
 * e' un errore esplicito (GMB_3D_E_ARG). */
int64_t gmb_scene_vertex_n(double x, double y, double z, double u, double v,
                           double nx, double ny, double nz);
int64_t gmb_scene_tri(int64_t a, int64_t b, int64_t c);
int64_t gmb_scene_quad(int64_t a, int64_t b, int64_t c, int64_t d);
int64_t gmb_scene_box(double x0, double y0, double z0,
                      double x1, double y1, double z1);         /* 24 vertici, 12 triangoli */
int64_t gmb_scene_objpos(double x, double y, double z, double yaw_deg);
/* SCENE KEY o, t, x, y, z, yaw, pitch, roll — chiave TRS dell'oggetto `o`
 * (1..nobj, ordine di dichiarazione come SCENE.PICK) al tempo `t` in secondi.
 * Le chiavi di un oggetto si ordinano da sole per tempo; RENDER le campiona al
 * tempo di scena (SCENE TIME) e scrive la trasformazione NELL'oggetto, che da
 * lì il frame legge come se venisse da OBJPOS. Fuori dall'intervallo vince la
 * chiave più vicina (prima/ultima, nessuna estrapolazione); fra due chiavi si
 * interpola linearmente su tutte e sei le componenti, gradi compresi e senza
 * avvolgimento (350 → 10 passa per 180). Tetti: 32 chiavi per oggetto, 65536
 * per scena. Pitch e roll ruotano davvero (ordine Y→X→Z); con loro a zero il
 * modello è quello di sempre, bit per bit. E_ARG = oggetto inesistente,
 * tempo negativo, tetto raggiunto. */
int64_t gmb_scene_key(int64_t o, double t, double x, double y, double z,
                      double yaw, double pitch, double roll);
/* SCENE TIME t — tempo di scena in secondi (>= 0), quello che RENDER usa per
 * campionare le chiavi. Non avanza da sé: lo muove il programma, così lo stesso
 * programma dà sempre gli stessi frame. RESET lo riporta a 0. */
int64_t gmb_scene_time(double t);
int64_t gmb_scene_camera(double x, double y, double z,
                         double yaw_deg, double pitch_deg);
/* SCENE LIGHT dx, dy, dz, livello: luce direzionale per faccia. (dx,dy,dz) punta VERSO
 * la luce (normalizzata qui; nulla = errore), `livello` 0..255 è la luce sulle facce
 * rivolte alla sorgente. Il coseno si quantizza in un intero 0..255 e la miscela
 * ambiente(40)→livello è intera: stessi byte su tutte le gambe. Con LIGHT attiva la
 * luce per faccia sostituisce la tabella fissa delle 6 facce del cubo. */
int64_t gmb_scene_light(double dx, double dy, double dz, int64_t level);
/* SCENE LAMP n, x, y, z, radius, amount [, dx, dy, dz, angle]: lampada 0..7.
 * Senza direzione e' puntiforme; con la direzione (puntata) e `angle` in gradi
 * (semiangolo del cono) e' uno spot: pieno sull'asse, zero al bordo (M-LIGHT).
 * `amount` 0..255; raggio <= 0 o amount 0 spengono la lampada. */
int64_t gmb_scene_lamp(int64_t n, double x, double y, double z, double radius,
                       double amount);
int64_t gmb_scene_lamp_spot(int64_t n, double x, double y, double z, double radius,
                            double amount, double dx, double dy, double dz,
                            double angle_deg);
/* SCENE FOG d: buio alla distanza d (0 = spento). SCENE FAR d: oltre d i
 * triangoli si saltano (0 = spento). SCENE COLORMAP: luce a 32 livelli. */
int64_t gmb_scene_fog(double d);
int64_t gmb_scene_far(double d);
int64_t gmb_scene_colormap(void);
/* SCENE SKY [repeat [, shift]]: cielo per angolo con la texture corrente (M-SKY).
 * La forma §4.2 con l'indice di texture non si applica: SCENE ha UNA texture alla
 * volta, quindi si imposta con TEXTURE e poi si chiama SKY. */
int64_t gmb_scene_sky(double repeat, double shift);
/* SCENE THING n, x, y, z, w, h [, luce]: billboard con la texture corrente
 * (alpha 0 trasparente), base a z, sempre rivolto all'osservatore (M-THINGS). */
int64_t gmb_scene_thing(int64_t n, double x, double y, double z, double w, double h);
int64_t gmb_scene_thing_light(int64_t n, int64_t level);
/* SCENE SMOOTH|ALPHA|TWOSIDED on|off (idea #5): espongono i flag già presenti in
 * `gmb_3d_material_t` del kernel — bilineare, alpha-test, niente back-face cull. `on` != 0
 * accende. I default (1, 1, 0) riproducono il comportamento storico, quindi le scene che
 * non li chiamano danno gli stessi byte. Stato del materiale: sopravvive a RESET. */
int64_t gmb_scene_smooth(int64_t on);
int64_t gmb_scene_alpha(int64_t on);
int64_t gmb_scene_twosided(int64_t on);
/* SCENE AA on|off: antialiasing SSAA 2x, spento di default. Acceso, ogni DRAW di
 * mesh va in un target interno doppio e viene risolto con una media 2x2: i quattro
 * campioni stanno a 1/4 e 3/4 del pixel, quelli fuori dal triangolo valgono il
 * colore gia' nel pixel, quindi il bordo si fonde col fondo. SAVE/PRESENT/
 * SCENE.PIXEL non cambiano: leggono sempre il target logico. Con AA spento nessun
 * target doppio e un campione al centro: gli stessi byte di sempre. Sky e thing
 * non sono antialiasati (nessun bordo geometrico: texture bilineare). Lo stato di
 * scena sopravvive a RESET. */
int64_t gmb_scene_aa(int64_t on);
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
/* Seconda metà di PRESENT per le gambe a DUE immagini (IL/PCode, VM/C): chi ha copiato il
 * frame chiama questa DOPO una copia riuscita, e il frame si chiude come in
 * `gmb_scene_present` (che lo fa da sé). Senza questa chiamata `SCENE.STATS("renders")` non
 * riparte da zero: è la sola parte di PRESENT che non si può dedurre dal target. */
void gmb_scene_present_done(void);
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
/* SCENE PICK on|off: buffer di ID per pixel, ADDITIVO e spento di default.
 * Acceso, RENDER scrive in ogni pixel l'id dell'oggetto che vince il depth test
 * (1..256, ordine di dichiarazione degli OBJECT); fondo, cielo e thing restano 0.
 * Colore, profondita', PIXEL, HASH, SAVE e PRESENT non cambiano. Lo stato
 * sopravvive a RESET, come gli altri flag di scena. */
int64_t gmb_scene_pick_enable(int64_t on);
/* SCENE.PICK(x, y): id dell'oggetto nel pixel — 0 fondo, 1..256 oggetto,
 * -1 senza target o fuori dal target (la stessa sentinella di SCENE.PIXEL),
 * -2 buffer spento con un target che esiste (manca SCENE PICK on). */
int64_t gmb_scene_pick(int64_t x, int64_t y);
/* SCENE.STATS(nome$): una misura della scena (specifica `M-STATS`). Chiavi:
 *   "tris"      triangoli dichiarati            "draw"  disegnati nell'ultimo RENDER
 *   "culled"    scartati nell'ultimo RENDER     "pixels" pixel passati dal depth test
 *   "objs"      oggetti dichiarati              "bytes" memoria RISERVATA dalla geometria
 *   "allocs"    allocazioni del modulo 3D dall'avvio (prova di R4, non si azzera)
 *   "renders"   RENDER nel frame aperto (PRESENT lo chiude, RESET lo azzera)
 *   "renders_max" il massimo di "renders" dopo RESET (R6: > 1 = più RENDER in un giro)
 * −1 = chiave sconosciuta (o NULL): è una lettura, non alza errori di superficie.
 * Il tempo di un frame NON è qui: si misura fuori dal motore (piano §0septies). */
double gmb_scene_stats(const char* name);

/* --- P5: superficie interattiva (specifica §3.3, M-PLAYER/M-COLLIDE/M-FLOOR/M-ZONE) ---
 *
 * Assi SCENE: y = altezza, piano (x, z), fronte a yaw 0 = +z. Il corpo ha i
 * piedi in (x, y, z); CAMERA continua a posizionare l'OCCHIO (P3), quindi le
 * letture X/Y/Z sono l'occhio e `EYE` è la sua altezza sopra i piedi.
 */
/* SCENE PLAYER raggio, altezza, occhio, step (default 14/56/46/24, M-PLAYER). */
int64_t gmb_scene_player(double radius, double height, double eye, double step);
/* SCENE MOVE avanti, destra: relativo a yaw, scivola, sale gradini ≤ step,
 * bloccato da soffitto basso; lascia i bit di HIT nell'ultimo MOVE. */
int64_t gmb_scene_move(double forward, double right);
/* SCENE TURN dyaw [, dpitch]: ruota la vista in gradi (pitch in -89..89). */
int64_t gmb_scene_turn(double d_yaw, double d_pitch);
/* SCENE LIFT dy: sposta il corpo in verticale (piedi e occhio). */
int64_t gmb_scene_lift(double dy);
/* SCENE ZONE id, x1, y1, x2, y2, z1, z2 [, tag]: x/z sul piano, y in altezza.
 * L'ordine di creazione conta (M-ZONE). */
int64_t gmb_scene_zone(int64_t id, double x1, double y1, double x2, double y2,
                       double z1, double z2, const char* tag);
/* SCENE.ZONE(x, y, z): id della prima zona che contiene il punto, -1 se nessuna. */
int64_t gmb_scene_zone_at(double x, double y, double z);
/* SCENE.ZONETAG(id): tag della zona, "" se l'id non esiste. Ritorna un
 * puntatore valido fino alla prossima chiamata: le gambe lo copiano subito
 * (C99: gmb_string_alloc; VM/C: gmb_str_cstr; IL/PCode: Marshal). La superficie
 * resta senza gmb_string_t, così gmb_3d.h non dipende da gmb_runtime.h. */
const char* gmb_scene_zonetag_text(int64_t id);
/* Quote (M-FLOOR): superficie più alta a o sotto y / più bassa a o sopra y,
 * nel punto (x, z); -1000000 quando non c'è (C-SENTINEL). */
double gmb_scene_floor_at(double x, double y, double z);
double gmb_scene_ceil_at(double x, double y, double z);
/* Letture camera e corpo. */
double gmb_scene_x(void);
double gmb_scene_y(void);
double gmb_scene_z(void);
double gmb_scene_eye(void);
double gmb_scene_yaw(void);
double gmb_scene_pitch(void);
double gmb_scene_hit(void);

/* --- P5: raggi (specifica §3.3, M-RAYS) -----------------------------------
 * SHOOT(range) spara lungo la vista: colpisce gli oggetti (SHOTKIND 1) e i thing
 * (SHOTKIND 2); il flag 8 (see-through) lascia passare il colpo, il flag 4 no.
 * Ritorna −1 se non prende nulla, altrimenti l'id: oggetto (come SCENE.PICK, 1..n)
 * o indice del thing (0..255) — SHOTKIND dice quale. USE(reach) ritorna l'oggetto
 * davanti qualunque sia il flag, −1 se niente. SEES(n): 1 vede, 0 non vede,
 * −1 thing inesistente (C-DIVERGE-SEES).
 * Stato dell'ultimo colpo: SHOTKIND 0/1/2, SHOTOBJ (0 = non era un oggetto),
 * SHOTDIST (−1 = nessun colpo), SHOTX/Y/Z (−1000000 = nessun colpo). */
int64_t gmb_scene_shoot(double range);
int64_t gmb_scene_use(double reach);
int64_t gmb_scene_sees(int64_t thing);
double gmb_scene_shotkind(void);
double gmb_scene_shotobj(void);
double gmb_scene_shotdist(void);
double gmb_scene_shotx(void);
double gmb_scene_shoty(void);
double gmb_scene_shotz(void);

/* Load che hanno parcheggiato il task (R11): 0 in T3, +1 per ogni load dentro
 * un task. Osservabile come alloc_count per R4. */
unsigned long gmb_3d_load_park_count(void);

const char* gmb_3d_last_error(void);

/* Chiamata CONTROLLATA delle superfici SCENE: se `rc` != GMB_3D_OK il messaggio
 * canonico di gmb_3d_last_error() diventa un errore BASIC (codice 5, riga
 * `line`) catturabile da ON ERROR/TRY. È la forma che usa l'emitter C99; VM/C fa
 * lo stesso controllo attorno al dispatch SCENE, IL/PCode nei wrapper P/Invoke.
 * Con rc di successo azzera l'errore e ritorna rc. */
int64_t gmb_scene_checked(int64_t rc, int64_t line);

/* Azzera gmb_3d_last_error() prima di una superficie SCENE (usata dal dispatch
 * VM/C per non leggere un messaggio stantio di un comando precedente). */
void gmb_scene_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_3D_H */
