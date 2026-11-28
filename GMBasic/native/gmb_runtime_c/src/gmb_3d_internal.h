/* gmb_3d_internal.h — quanto i moduli 3D si dicono fra loro, mai fuori.
 *
 * gmb_3d.h espone il mondo come handle opaco: chi lo usa non vede né gm_* né
 * la forma dell'array di settori. Qui stanno la definizione, lo stato di tempo
 * per settore (T-*) e il setter dell'errore (l'ultimo errore vive in
 * gmb_3d_frame.c, una sola copia).
 */
#ifndef GMB_3D_INTERNAL_H
#define GMB_3D_INTERNAL_H

#include "gmb_3d.h"
#include "gm_gfx_sector.h"

/* Passo fisso di simulazione (T-FIXED): un'unità sola, uguale per il load e
 * per chi chiama TICK. */
#define GMB_3D_DEFAULT_STEP (1.0 / 60.0)

/* Stato di animazione di un settore: mover (SECMOVE), animazione (ANIM),
 * scroll (SCROLL) e luce dinamica (LIGHTFX). Il documento non lo contiene —
 * lo avvia il programma — quindi vive solo qui e si azzera al load. La luce
 * corrente invece sta nel settore (`light`): una sola copia, letta dal
 * renderer e da LIGHTNOW. */
typedef struct gmb_3d_sector_rt {
    int     moving;                  /* mover attivo */
    gm_real floor_to, ceil_to, speed;
    int     has_ceil;
    int     anim_first, anim_count, anim_ceil;   /* anim_ceil: il settore aveva un soffitto */
    gm_real anim_fps, anim_phase;
    int     scroll_planes, scroll_walls;
    gm_real scroll_u, scroll_v;
    int     fx_mode;                 /* 0 spento, 1 flicker, 2 pulse, 3 strobe */
    gm_real fx_amount, fx_rate, fx_phase, fx_value;
    unsigned int fx_rng;
} gmb_3d_sector_rt;

struct gmb_3d_world {
    gm_sector_world sw;   /* geometria, texture e lampade (`sw.tex`, `sw.ntex`) */
    gm_tex* tex;          /* buffer delle texture, posseduto dal mondo */
    gmb_3d_sector_rt* rt; /* uno per settore, posseduto dal mondo */
    gm_real step;         /* passo fisso (T-FIXED), default GMB_3D_DEFAULT_STEP */
    gm_real accum, time;
    unsigned long ticks;
};

/* Imposta gmb_3d_last_error(). Vuoto = errore azzerato. */
void gmb_3d_set_error(const char* msg);

/* Contatore delle allocazioni del 3D (definito in gmb_3d_frame.c, letto da
 * gmb_3d_alloc_count): serve a DIMOSTRARE che il frame non alloca. */
void gmb_3d_note_alloc(unsigned long n);

/* gmb_3d_scene.c — matrici del renderer e culling per oggetto (P5, M-CULL).
 * La matematica sta UNA volta sola fuori dalla facciata dei comandi, che resta
 * sotto le 1200 righe (§0quater). 
 */
void gmb_3d_m4_mul(float* out, const float* a, const float* b);            /* out = a*b */
void gmb_3d_m4_perspective(float* m, float fovy, float aspect, float zn, float zf);
void gmb_3d_m4_look_at(float* m, const float eye[3], const float at[3], const float up[3]);
void gmb_3d_m4_model(float* m, float x, float y, float z, float yaw_deg);
/* Come sopra, più pitch e roll (gradi, ordine Y→X→Z). Con pitch = roll = 0
 * chiama gmb_3d_m4_model: le scene senza chiavi restano bit-identiche. */
void gmb_3d_m4_model_trs(float* m, float x, float y, float z,
                         float yaw_deg, float pitch_deg, float roll_deg);
/* Sei piani del frustum in coordinate MONDO dalla view-projection (column-major). */
void gmb_3d_frustum_planes(const float* vp, float planes[6][4]);
/* 1 = l'oggetto (AABB nello spazio oggetto + modello) è interamente fuori da almeno
 * un piano: il chiamante conta i suoi triangoli in CULLED e salta il draw. */
int  gmb_3d_object_outside(const float planes[6][4], const float* model,
                           const float bmin[3], const float bmax[3]);

/* Superficie interattiva (gmb_3d_scene_actor.c, P5): AABB MONDO degli oggetti,
 * camera condivisa con la facciata, corpo e zone. Il corpo e le zone vivono nel
 * modulo actor; la camera resta UNA sola, in gmb_3d_cmd.c, e si legge/scrive da
 * qui (nessuna seconda copia). */
typedef struct gmb_3d_aabb {
    float min[3], max[3];   /* y = altezza (convenzione SCENE) */
    int   flags;            /* M-FLAGS: 4 = passabile, 8 = see-through */
    int   id;               /* id dell'oggetto (1..n), come SCENE.PICK: i raggi lo riportano */
} gmb_3d_aabb_t;

/* Thing della scena (M-THINGS): billboard verticale di base (x, y, z), largo w e
 * alto h. Vive in gmb_3d_cmd.c; i raggi (gmb_3d_ray.c) lo leggono per SHOOT/SEES. */
#define GMB_SCENE_MAX_THINGS 256

typedef struct gmb_3d_scene_thing_rt {
    int used;
    float x, y, z, w, h;
    unsigned char light;
} gmb_3d_scene_thing_rt;

/* Array dei thing (capacità GMB_SCENE_MAX_THINGS), sola lettura per i raggi. */
const gmb_3d_scene_thing_rt* gmb_3d_scene_things(int* count);

/* Raggi della superficie SCENE (gmb_3d_ray.c, P5/M-RAYS): azzera l'ultimo colpo.
 * Chiamata da SCENE RESET. */
void gmb_3d_ray_reset(void);

/* Statistiche della scena (gmb_3d_stats.c, P5/M-STATS) — `SCENE.STATS(name$)`.
 * RESET azzera i contatori di frame; RENDER ne conta uno; PRESENT chiude il
 * frame azzerandolo. Le allocazioni NON si azzerano: sono la prova di R4. */
void gmb_3d_stats_reset(void);
void gmb_3d_stats_note_render(void);
void gmb_3d_stats_note_present(void);

/* Piste keyframe TRS (gmb_3d_key.c, M-KEY) — `SCENE KEY` e `SCENE TIME`.
 * Le chiavi sono ordinate per (oggetto, tempo); RENDER le campiona al tempo di
 * scena e scrive la trasformazione negli oggetti. RESET azzera piste e tempo. */
void gmb_3d_key_reset(void);
void gmb_3d_key_sample(void);
int  gmb_3d_key_count(int obj);            /* chiavi dichiarate per un oggetto */
double gmb_scene_time_now(void);           /* tempo di scena corrente */

/* Livello di dettaglio per oggetto (gmb_3d_lod.c, M-LOD) — `SCENE LOD o, dist, mesh`.
 * `gmb_3d_lod_source` restituisce l'indice (0-based) della GEOMETRIA da disegnare
 * per l'oggetto `oi`: lui stesso, oppure la mesh sostitutiva quando l'occhio è
 * oltre la distanza dichiarata. Un livello solo; trasformazione e tinta restano
 * quelle dell'oggetto.
 */
int  gmb_3d_lod_source(int oi, const float eye[3]);

/* Instancing per materiale (gmb_3d_inst.c, M-INST) — `SCENE INSTANCE o, src`:
 * `o` disegna la geometria e il materiale (flag, luce base) di `src`, con la
 * propria trasformazione e tinta. Due indici 0-based e basta:
 * `gmb_3d_inst_geom` = geometria da disegnare (istanza, altrimenti LOD,
 * altrimenti sé stesso); `gmb_3d_inst_mat` = oggetto da cui prendere flag e
 * luce base (la sorgente per un'istanza, altrimenti sé stesso). Il disegno
 * legge i due campi dall'oggetto indicato: nessuna copia del materiale.
 */
int  gmb_3d_inst_geom(int oi, const float eye[3]);
int  gmb_3d_inst_mat(int oi);

/* Skinning a 64 ossa (gmb_3d_skin.c, M-SKIN) — `SCENE BONES`/`POSEBONE`/`WEIGHT`.
 * Un osso è una TRS nello spazio dell'oggetto (posa di default = identità); un
 * vertice porta al massimo DUE influenze, ognuna con il suo osso e il suo peso
 * 0..255 (la fusione normalizza alla somma). `gmb_3d_skin_pos` restituisce le
 * posizioni da disegnare (NULL = geometria dichiarata) nello scratch riservato.
 */
#define GMB_SCENE_MAX_BONES 64
typedef struct gmb_3d_skin {
    unsigned char b0, b1;   /* osso 1..64; GMB_SKIN_NONE = influenza assente */
    unsigned char w0, w1;   /* peso 0..255 */
} gmb_3d_skin_t;
#define GMB_SKIN_NONE 255u
typedef struct gmb_3d_bone { float x, y, z, yaw, pitch, roll; } gmb_3d_bone_t;
void gmb_3d_skin_reset(void);            /* RESET: ossa, pose e pesi */
void gmb_3d_skin_pose_all(void);         /* matrici degli ossi, una volta per RENDER */
const float* gmb_3d_skin_pos(int oi);    /* posizioni da disegnare, NULL = dichiarate */

/* --- buffer della scena a capacità dinamica (gmb_3d_scene_buf.c, P5) -------
 * Il primo blocco di P5 aveva tetti statici (32768 vertici, 16384 triangoli,
 * 512 oggetti) che rendevano irraggiungibile l'accettazione dei 200k triangoli.
 * I buffer partono da quei valori e CRESCONO per raddoppio alla dichiarazione
 * della geometria; il frame legge solo capacità già riservata (nessuna
 * allocazione per frame). I tetti restano come guardia di memoria. */
#define GMB_SCENE_VERTS_INIT 32768
#define GMB_SCENE_TRIS_INIT  16384
#define GMB_SCENE_OBJS_INIT  512
#define GMB_SCENE_VERTS_MAX  (1 << 22)   /* 4.194.304 vertici */
#define GMB_SCENE_TRIS_MAX   (1 << 21)   /* 2.097.152 triangoli */
#define GMB_SCENE_OBJS_MAX   (1 << 16)   /* 65.536 oggetti */

/* Un oggetto della scena: run di vertici/triangoli, trasformazione TRS
 * (traslazione + yaw, più pitch/roll che solo le piste KEY muovono), tinta,
 * flag di materiale e AABB nello spazio oggetto. */
typedef struct gmb_scene_obj {
    int first_vert, nvert, first_tri, ntri;
    float x, y, z, yaw;
    float pitch, roll;     /* M-KEY: 0 = solo yaw (percorso di sempre, bit-identico) */
    int   lod_mesh;        /* M-LOD: 0 = nessun LOD, altrimenti 1..nobj (geometria sostitutiva) */
    float lod_dist;        /* M-LOD: oltre questa distanza si disegna la mesh */
    int   inst_src;        /* M-INST: 0 = nessuna istanza, altrimenti 1..nobj (geometria+materiale) */
    unsigned char r, g, b;
    unsigned char flags;   /* M-FLAGS: 1 masked, 2 traslucido, 4 passabile, 8 see-through, 16 due lati */
    unsigned char light;   /* luce base dell'oggetto 0..255 */
    unsigned char has_nrm; /* 1 = i vertici dichiarano la normale (M-LIGHT-N) */
    int   has_skin;        /* M-SKIN: almeno un vertice ha un peso */
    float bmin[3], bmax[3];
} gmb_scene_obj_t;

/* Stato della geometria: contatori, buffer e capacità. `cur` = oggetto corrente
 * (-1 = nessuno). Gli scratch `order`/`dist` (traslucidi) e `bounds` (AABB
 * mondo) sono dimensionati sulla capacità degli oggetti, quindi il RENDER e le
 * quote non allocano. */
typedef struct gmb_3d_scene_geom {
    int nv, nt, nobj, cur;
    int cap_v, cap_t, cap_o;
    float *pos, *uv, *nrm, *nrm_w;
    float* sk_pos;          /* M-SKIN: posizioni skinnate (scratch), NULL se nessun peso */
    gmb_3d_skin_t* skin;    /* M-SKIN: due influenze per vertice, NULL se nessun peso */
    int* idx;
    unsigned char* light;
    unsigned char* lit_tri;
    gmb_scene_obj_t* obj;
    int* order;
    float* dist;
    gmb_3d_aabb_t* bounds;
} gmb_3d_scene_geom_t;

gmb_3d_scene_geom_t* gmb_3d_scene_geom(void);
void gmb_3d_scene_geom_clear(void);          /* contatori a zero, capacità conservata */
int  gmb_3d_scene_reserve_verts(int need);
/* M-SKIN: i buffer dei pesi nascono al PRIMO WEIGHT (le scene senza skinning non
 * li pagano) e crescono con i vertici; azzerare le prime `nvert` voci le riporta a
 * «nessuna influenza» senza liberare niente. */
int  gmb_3d_scene_skin_touch(void);
void gmb_3d_scene_skin_clear(int nvert);
int  gmb_3d_scene_reserve_tris(int need);
int  gmb_3d_scene_reserve_objs(int need);
/* AABB mondo di tutti gli oggetti con geometria, buffer persistente. */const gmb_3d_aabb_t* gmb_3d_scene_bounds_all(int* count);
/* Memoria RISERVATA dalla geometria (capacità, non uso), in byte: la sa il file
 * che possiede i buffer e i loro passi, così `SCENE.STATS("bytes")` non ripete
 * la formula in un secondo posto. RESET conserva la capacità: il numero non cala. */
double gmb_3d_scene_bytes(void);
void gmb_3d_scene_set_texture(unsigned char* buf, int w, int h);
void gmb_3d_scene_camera_get(float eye[3], float* yaw, float* pitch);
void gmb_3d_scene_camera_set(float x, float y, float z, float yaw, float pitch);
/* P6/M-SND (gmb_3d_audio.c): sorgenti posizionali. RESET le azzera con il resto
 * della scena; RENDER ricalcola gain e pan dalla camera (listener) una volta per
 * frame. La superficie pubblica e' in gmb_3d.h. */
void gmb_3d_sound_reset(void);
void gmb_3d_sound_update(void);
void gmb_3d_actor_reset(void);
void gmb_3d_actor_sync_feet(double eye_y);

/* SCENE PICK (gmb_3d_pick.c). Lo stato della scena resta UNO, in gmb_3d_cmd.c:
 * qui si espone solo il riferimento al suo target e il flag additivo del buffer
 * di id, cosi' VIEW/RENDER possono agganciarlo senza duplicare la scena. */
gmb_3d_target_t* gmb_scene_target_ref(void);
int gmb_scene_pick_active(void);
int gmb_scene_pick_attach(gmb_3d_target_t* t);

#endif /* GMB_3D_INTERNAL_H */
