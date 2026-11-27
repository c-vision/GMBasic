/* gmb_3d_ray.c — raggi della superficie SCENE (P5, specifica §3.3 `M-RAYS`).
 *
 * `SHOOT(range)`, `USE(reach)` e `SEES(n)` guardano la stessa scena dei comandi:
 *  - gli ostacoli sono gli AABB MONDO degli oggetti (gmb_3d_scene_bounds_all, il
 *    buffer persistente di gmb_3d_scene_buf.c), quindi la vista è coerente con la
 *    collisione (`M-COLLIDE`) e le quote (`M-FLOOR`);
 *  - il flag **8** (see-through) lascia passare colpi e vista (`M-RAYS`); il flag
 *    **4** (passabile) NON lascia passare: è solo collisione, non trasparenza;
 *  - i THING sono cilindri verticali (raggio `w/2`, da `y` a `y+h`) e non fanno
 *    da ostacolo alla vista: un billboard non nasconde.
 *
 * Sono LETTURE (§1): tornano un numero e non alzano errori di superficie. Un
 * argomento non positivo o non finito vale «niente in portata» → `−1`, come una
 * scena vuota: le sentinelle sono `−1` (nessun colpo / assente), `0` (SHOTOBJ
 * senza oggetto, come il fondo di `PICK`) e `−1000000` (coordinata assente,
 * `C-SENTINEL`).
 *
 * Stato: l'ULTIMO colpo (`SHOTKIND`, `SHOTOBJ`, `SHOTDIST`, `SHOTX/Y/Z`) vive qui,
 * una volta sola; `SCENE RESET` lo azzera. Nessuna allocazione: la scansione usa
 * solo il buffer degli AABB già riservato.
 *
 * Assi: y = altezza, piano (x, z), fronte a yaw 0 = +z, pitch positivo verso l'alto
 * (`C-DIVERGE-AXES`, la stessa convenzione di MOVE/TURN).
 */
#include "gmb_3d_internal.h"

#include <math.h>

#define RAY_PI      3.14159265358979
#define RAY_EPS     1.0e-4f
#define RAY_NO_HIT  (-1)              /* C-SENTINEL: nessun colpo / entità assente */
#define RAY_NO_OBJ  0                 /* C-SENTINEL: nessun oggetto, come il fondo di PICK */
#define RAY_NO_COORD (-1000000.0)     /* C-SENTINEL: coordinata assente */
#define RAY_FLAG_SEE_THROUGH 8        /* M-FLAGS: lascia passare colpi e vista */

typedef struct {
    int   kind;    /* 0 niente, 1 oggetto, 2 thing */
    int   obj;     /* id dell'oggetto (0 se il colpo non ne ha preso uno) */
    float t;       /* distanza del colpo, < 0 = nessun colpo */
    float x, y, z; /* punto colpito */
} ray_shot_t;

static ray_shot_t g_shot;

void gmb_3d_ray_reset(void)
{
    g_shot.kind = 0;
    g_shot.obj = RAY_NO_OBJ;
    g_shot.t = -1.0f;
    g_shot.x = g_shot.y = g_shot.z = 0.0f;
}

/* Origine = occhio, direzione = vista (già unitaria: cos²·(sin²+cos²) + sin² = 1). */
static void ray_view(float o[3], float d[3])
{
    float yaw = 0.0f, pitch = 0.0f;
    gmb_3d_scene_camera_get(o, &yaw, &pitch);
    double a = (double)yaw * (RAY_PI / 180.0);
    double p = (double)pitch * (RAY_PI / 180.0);
    d[0] = (float)(sin(a) * cos(p));
    d[1] = (float)sin(p);
    d[2] = (float)(cos(a) * cos(p));
}

/* Intersezione raggio/AABB (slab). Ritorna 1 e la distanza in *tout; un raggio
 * che parte DENTRO il volume colpisce a distanza 0. */
static int ray_aabb(const float o[3], const float d[3], const gmb_3d_aabb_t* b, float tmax, float* tout)
{
    float t0 = 0.0f, t1 = tmax;
    for (int i = 0; i < 3; i++)
    {
        float oi = o[i], di = d[i];
        if (fabsf(di) < 1.0e-9f)
        {
            if (oi < b->min[i] || oi > b->max[i]) return 0;
            continue;
        }
        float a = (b->min[i] - oi) / di;
        float c = (b->max[i] - oi) / di;
        if (a > c) { float t = a; a = c; c = t; }
        if (a > t0) t0 = a;
        if (c < t1) t1 = c;
        if (t0 > t1) return 0;
    }
    *tout = t0;
    return 1;
}

/* Oggetto più vicino lungo il raggio. `skip_see_through` = 1 per SHOOT/SEES (il
 * flag 8 lascia passare), 0 per USE («qualunque siano i flag»).
 * A parità di distanza vince l'id più basso: l'ordine di dichiarazione, come ZONE. */
static int ray_objects(const float o[3], const float d[3], float tmax, int skip_see_through, float* tout)
{
    int nb = 0;
    const gmb_3d_aabb_t* boxes = gmb_3d_scene_bounds_all(&nb);

    int   best_id = RAY_NO_OBJ;
    float best_t  = tmax + RAY_EPS;   /* un colpo esattamente a `tmax` conta */
    for (int i = 0; boxes != NULL && i < nb; i++)
    {
        const gmb_3d_aabb_t* b = &boxes[i];
        if (skip_see_through && (b->flags & RAY_FLAG_SEE_THROUGH)) continue;
        float t;
        if (!ray_aabb(o, d, b, tmax, &t)) continue;
        if (t < best_t) { best_t = t; best_id = b->id; }
    }
    *tout = best_t;
    return best_id;
}

/* Thing più vicino: cilindro verticale (raggio w/2, da y a y+h). La camera è
 * limitata a |pitch| < 89°, quindi la componente orizzontale non è mai nulla e
 * l'equazione di secondo grado non degenera. Ritorna l'indice del thing o -1. */
static int ray_things(const float o[3], const float d[3], float tmax, float* tout)
{
    int n = 0;
    const gmb_3d_scene_thing_rt* things = gmb_3d_scene_things(&n);

    int   best = -1;
    float best_t = tmax + RAY_EPS;
    for (int i = 0; things != NULL && i < n; i++)
    {
        const gmb_3d_scene_thing_rt* th = &things[i];
        if (!th->used) continue;

        float r  = 0.5f * th->w;
        float ox = o[0] - th->x, oz = o[2] - th->z;
        float a  = d[0] * d[0] + d[2] * d[2];
        float b  = 2.0f * (ox * d[0] + oz * d[2]);
        float c  = ox * ox + oz * oz - r * r;
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) continue;

        float sq = sqrtf(disc);
        float t = (-b - sq) / (2.0f * a);
        if (t < 0.0f) t = (-b + sq) / (2.0f * a);
        if (t < 0.0f) t = 0.0f;          /* occhio dentro il cilindro */
        if (t >= best_t) continue;

        float y = o[1] + t * d[1];
        if (y < th->y - RAY_EPS || y > th->y + th->h + RAY_EPS) continue;
        best_t = t;
        best = i;
    }
    *tout = best_t;
    return best;
}

/* SCENE.SHOOT(range): -1 nessun colpo, altrimenti l'id di ciò che è stato preso
 * (oggetto 1..n / indice del thing 0..255): SHOTKIND dice quale dei due.
 * A parità di distanza vince l'oggetto. Aggiorna lo stato dell'ultimo colpo. */
int64_t gmb_scene_shoot(double range)
{
    gmb_3d_ray_reset();
    if (!isfinite(range) || range <= 0.0) return RAY_NO_HIT;

    float o[3], d[3];
    ray_view(o, d);
    const float tmax = (float)range;

    float ot = 0.0f, tt = 0.0f;
    int oid = ray_objects(o, d, tmax, 1, &ot);
    int tid = ray_things(o, d, tmax, &tt);

    float t;
    int64_t id;
    if (oid != RAY_NO_OBJ && (tid < 0 || ot <= tt)) { g_shot.kind = 1; g_shot.obj = oid; id = oid; t = ot; }
    else if (tid >= 0)                              { g_shot.kind = 2;                 id = tid; t = tt; }
    else return RAY_NO_HIT;

    g_shot.t = t;
    g_shot.x = o[0] + d[0] * t;
    g_shot.y = o[1] + d[1] * t;
    g_shot.z = o[2] + d[2] * t;
    return id;
}

/* SCENE.USE(reach): l'oggetto davanti entro `reach`, QUALUNQUE sia il flag (il
 * passabile e il see-through si usano). -1 se niente. Non tocca lo stato
 * dell'ultimo colpo: quello è di SHOOT. */
int64_t gmb_scene_use(double reach)
{
    if (!isfinite(reach) || reach <= 0.0) return RAY_NO_HIT;
    float o[3], d[3], t = 0.0f;
    ray_view(o, d);
    int id = ray_objects(o, d, (float)reach, 0, &t);
    return id == RAY_NO_OBJ ? RAY_NO_HIT : id;
}

/* SCENE.SEES(n): 1 = il thing n ha linea di vista verso l'occhio, 0 = non ce
 * l'ha, -1 = n non è un thing esistente (C-SENTINEL, C-DIVERGE-SEES). Il punto
 * di partenza è al 70% dell'altezza del thing. */
int64_t gmb_scene_sees(int64_t n)
{
    int count = 0;
    const gmb_3d_scene_thing_rt* things = gmb_3d_scene_things(&count);
    if (things == NULL || n < 0 || n >= count || !things[n].used) return -1;

    const gmb_3d_scene_thing_rt* th = &things[n];
    float eye[3], yaw = 0.0f, pitch = 0.0f;
    gmb_3d_scene_camera_get(eye, &yaw, &pitch);

    float o[3] = { th->x, th->y + 0.7f * th->h, th->z };
    float dx = eye[0] - o[0], dy = eye[1] - o[1], dz = eye[2] - o[2];
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < RAY_EPS) return 1;   /* occhio sul thing: niente in mezzo */

    float d[3] = { dx / len, dy / len, dz / len };
    float t = 0.0f;
    return ray_objects(o, d, len - RAY_EPS, 1, &t) == RAY_NO_OBJ ? 1 : 0;
}

/* --- letture dell'ultimo colpo (C-SENTINEL) ------------------------------ */

double gmb_scene_shotkind(void) { return (double)g_shot.kind; }
double gmb_scene_shotobj(void)  { return (double)g_shot.obj; }
double gmb_scene_shotdist(void) { return g_shot.t < 0.0f ? -1.0 : (double)g_shot.t; }
double gmb_scene_shotx(void)    { return g_shot.t < 0.0f ? RAY_NO_COORD : (double)g_shot.x; }
double gmb_scene_shoty(void)    { return g_shot.t < 0.0f ? RAY_NO_COORD : (double)g_shot.y; }
double gmb_scene_shotz(void)    { return g_shot.t < 0.0f ? RAY_NO_COORD : (double)g_shot.z; }
