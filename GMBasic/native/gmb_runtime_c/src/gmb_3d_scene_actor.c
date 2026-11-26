/* gmb_3d_scene_actor.c — superficie interattiva di SCENE (P5).
 *
 * Specifica: docs/3D-Specifica-Comportamenti.md §3.3 (`M-PLAYER`, `M-COLLIDE`,
 * `M-FLOOR`, `M-ZONE`, `M-PASSABLE`).
 *
 * PERCHÉ UN FILE A PARTE. `gmb_3d_cmd.c` è la facciata dei comandi e sta sotto le
 * 1200 righe (§0quater): qui vive la semantica del corpo, una volta sola, e la
 * facciata la chiama. Nessuna copia dello stato: il corpo e le zone sono di
 * QUESTO modulo, la camera resta di `gmb_3d_cmd.c` e si legge/scrive con i due
 * accessori interni `gmb_3d_scene_camera_get/set` (nessuna seconda camera).
 *
 * ASSI. La superficie SCENE usa **y come altezza** (P3/P4, campioni, golden):
 * il piano è (x, z), il fronte a yaw 0 è +z, la destra è +x. Le zone e le quote
 * seguono la stessa convenzione: `ZONE id, x1, y1, x2, y2, z1, z2` ha x1/x2 e
 * z1/z2 sul piano e y1/y2 in altezza (divergenza dichiarata `C-DIVERGE-AXES`
 * rispetto al testo del riferimento, che usa z in su).
 *
 * COLLISIONE. Il corpo è un cilindro verticale (raggio, altezza) con i piedi in
 * (x, y, z). Gli ostacoli sono gli AABB MONDO degli oggetti della scena:
 *  - un oggetto con flag **4** (passabile) non spinge, non fa soffitto e non
 *    regge il passo; `FLOORAT`/`CEILAT` continuano a vederlo (`M-PASSABLE`);
 *  - un oggetto il cui tetto sta entro `step` dai piedi è un GRADINO: non è un
 *    muro, si sale (HIT bit 4 sul cambio di quota);
 *  - gli altri spingono: il centro viene riportato a un raggio dal rettangolo,
 *    quindi la componente obliqua sopravvive e il corpo scivola (`M-COLLIDE`,
 *    stessa semantica di `W-MOVE`);
 *  - un soffitto più basso dell'altezza del corpo annulla il sotto-passo.
 * HIT: bit 1 una parete ha spinto, bit 2 non ci si è mossi, bit 4 la quota è
 * cambiata (nel mondo a settori è il cambio di settore).
 *
 * NESSUNA ALLOCAZIONE: gli AABB mondo arrivano dal buffer persistente della
 * scena (gmb_3d_scene_bounds_all, capacità = oggetti riservati) e i sotto-passi
 * sono al massimo 32, quindi né MOVE né FLOORAT/CEILAT allocano.
 */
#include "gmb_3d_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define ACT_PI          3.14159265358979
#define ACT_EPS         1.0e-5
#define ACT_MAX_SUBSTEPS 32
#define ACT_PUSH_PASSES 4
#define ACT_MAX_ZONES   128
#define ACT_TAG_MAX     32
#define ACT_ZONE_SLACK  2.0     /* M-ZONE: il punto vale da 2 unità sotto la base */
#define ACT_NO_PLANE    (-1000000.0)   /* C-SENTINEL: quota assente */

typedef struct {
    int   used;
    int   id;
    float x0, y0, z0;   /* minimi normalizzati (y = altezza) */
    float x1, y1, z1;   /* massimi normalizzati */
    char  tag[ACT_TAG_MAX];
} act_zone_t;

static act_zone_t g_zone[ACT_MAX_ZONES];
static int        g_nzone;

typedef struct {
    double feet_y;
    double radius, height, eye, step;
    int    hit;
} act_body_t;

static act_body_t g_body;

static void actor_defaults(void)
{
    g_body.radius = 14.0;    /* M-PLAYER */
    g_body.height = 56.0;
    g_body.eye    = 46.0;
    g_body.step   = 24.0;
    g_body.feet_y = 1.5 - g_body.eye;   /* camera di default (0, 1.5, 6) */
    g_body.hit    = 0;
}

void gmb_3d_actor_reset(void)
{
    memset(g_zone, 0, sizeof(g_zone));
    g_nzone = 0;
    actor_defaults();
}

/* La camera di cmd.c è l'occhio; i piedi stanno `eye` sotto. CAMERA (P3)
 * continua a posizionare l'occhio: le letture X/Y/Z restano quelle di sempre e
 * le golden non cambiano. */
void gmb_3d_actor_sync_feet(double eye_y)
{
    g_body.feet_y = eye_y - g_body.eye;
}

/* --- corpo: comandi ------------------------------------------------------ */

int64_t gmb_scene_player(double radius, double height, double eye, double step)
{
    if (!isfinite(radius) || !isfinite(height) || !isfinite(eye) || !isfinite(step))
    {
        gmb_3d_set_error("SCENE PLAYER: valori non finiti");
        return GMB_3D_E_ARG;
    }
    if (radius <= 0.0 || height <= 0.0 || eye < 0.0 || step < 0.0)
    {
        gmb_3d_set_error("SCENE PLAYER: raggio e altezza positivi, occhio e step non negativi");
        return GMB_3D_E_ARG;
    }
    if (eye > height)
    {
        gmb_3d_set_error("SCENE PLAYER: l'occhio non può stare sopra la testa");
        return GMB_3D_E_ARG;
    }

    float eye_pos[3], yaw, pitch;
    gmb_3d_scene_camera_get(eye_pos, &yaw, &pitch);
    g_body.radius = radius;
    g_body.height = height;
    g_body.eye    = eye;
    g_body.step   = step;
    /* I piedi restano dove sono: cambiare l'occhio alza la testa, non sposta il
     * corpo. La camera segue (piedi + occhio). */
    gmb_3d_scene_camera_set(eye_pos[0], (float)(g_body.feet_y + g_body.eye), eye_pos[2], yaw, pitch);
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_turn(double d_yaw, double d_pitch)
{
    if (!isfinite(d_yaw) || !isfinite(d_pitch))
    {
        gmb_3d_set_error("SCENE TURN: angoli non finiti");
        return GMB_3D_E_ARG;
    }

    float eye[3], yaw, pitch;
    gmb_3d_scene_camera_get(eye, &yaw, &pitch);
    double ny = (double)yaw + d_yaw;
    double np = (double)pitch + d_pitch;
    if (np <= -89.0 || np >= 89.0)
    {
        gmb_3d_set_error("SCENE TURN: pitch tra -89 e 89 gradi");
        return GMB_3D_E_ARG;
    }
    gmb_3d_scene_camera_set(eye[0], eye[1], eye[2], (float)ny, (float)np);
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_lift(double dy)
{
    if (!isfinite(dy))
    {
        gmb_3d_set_error("SCENE LIFT: quota non finita");
        return GMB_3D_E_ARG;
    }

    float eye[3], yaw, pitch;
    gmb_3d_scene_camera_get(eye, &yaw, &pitch);
    g_body.feet_y += dy;
    gmb_3d_scene_camera_set(eye[0], (float)(g_body.feet_y + g_body.eye), eye[2], yaw, pitch);
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

/* --- collisione ---------------------------------------------------------- */

/* Un oggetto è un OSTACOLO VERTICALE per un corpo con i piedi a `feet` quando
 * attraversa la fascia (piedi + step, piedi + altezza). Sotto lo step è un
 * gradino, sopra la testa non tocca. */
static int act_blocks(const gmb_3d_aabb_t* b, double feet, double height, double step)
{
    return b->max[1] > feet + step + ACT_EPS && b->min[1] < feet + height - ACT_EPS;
}

/* Punto del rettangolo (x, z) più vicino a (px, pz). */
static void act_closest_xz(const gmb_3d_aabb_t* b, double px, double pz, double* qx, double* qz)
{
    *qx = px < b->min[0] ? b->min[0] : (px > b->max[0] ? b->max[0] : px);
    *qz = pz < b->min[2] ? b->min[2] : (pz > b->max[2] ? b->max[2] : pz);
}

/* Spinge il centro fuori dal rettangolo espanso di `radius`. Ritorna 1 se ha
 * spostato. Il centro DENTRO il rettangolo esce dal lato più corto: è la stessa
 * correzione di posizione di W-MOVE, e da lì nasce lo scivolamento. */
static int act_push_out(const gmb_3d_aabb_t* b, double radius, double* x, double* z)
{
    double qx, qz;
    act_closest_xz(b, *x, *z, &qx, &qz);
    double dx = *x - qx, dz = *z - qz;
    double d = sqrt(dx * dx + dz * dz);
    if (d >= radius - ACT_EPS) return 0;

    if (d > ACT_EPS)
    {
        *x = qx + dx / d * radius;
        *z = qz + dz / d * radius;
        return 1;
    }

    /* Centro esattamente dentro: la via più corta per uscire dal rettangolo
     * allargato. */
    double left   = *x - (b->min[0] - radius);
    double right  = (b->max[0] + radius) - *x;
    double back   = *z - (b->min[2] - radius);
    double front  = (b->max[2] + radius) - *z;
    double best = left;
    int side = 0;
    if (right < best) { best = right; side = 1; }
    if (back < best)  { best = back;  side = 2; }
    if (front < best) { best = front; side = 3; }
    if (side == 0) *x = b->min[0] - radius;
    else if (side == 1) *x = b->max[0] + radius;
    else if (side == 2) *z = b->min[2] - radius;
    else *z = b->max[2] + radius;
    return 1;
}

/* Tetto più basso sopra i piedi nel punto (x, z), escludendo i passabili. */
static double act_ceiling_at(const gmb_3d_aabb_t* boxes, int n, double x, double z, double feet)
{
    double best = ACT_NO_PLANE;
    for (int i = 0; i < n; i++)
    {
        const gmb_3d_aabb_t* b = &boxes[i];
        if (b->flags & 4) continue;
        if (x < b->min[0] - ACT_EPS || x > b->max[0] + ACT_EPS) continue;
        if (z < b->min[2] - ACT_EPS || z > b->max[2] + ACT_EPS) continue;
        /* Solo ciò che sta SOPRA i piedi è un soffitto: un oggetto che parte
         * alla quota dei piedi è un pavimento o un muro, non un tetto. */
        if (b->min[1] <= feet + ACT_EPS) continue;
        if (best == ACT_NO_PLANE || b->min[1] < best) best = b->min[1];
    }
    return best;
}

/* Pavimento d'appoggio: il tetto più alto entro `step` dai piedi, escludendo i
 * passabili (M-PASSABLE: non reggono il passo). ACT_NO_PLANE se non c'è. */
static double act_support_at(const gmb_3d_aabb_t* boxes, int n, double x, double z,
                             double feet, double step)
{
    double best = ACT_NO_PLANE;
    for (int i = 0; i < n; i++)
    {
        const gmb_3d_aabb_t* b = &boxes[i];
        if (b->flags & 4) continue;
        if (x < b->min[0] - ACT_EPS || x > b->max[0] + ACT_EPS) continue;
        if (z < b->min[2] - ACT_EPS || z > b->max[2] + ACT_EPS) continue;
        if (b->max[1] > feet + step + ACT_EPS) continue;
        if (b->max[1] < feet - step - ACT_EPS) continue;
        if (best == ACT_NO_PLANE || b->max[1] > best) best = b->max[1];
    }
    return best;
}

int64_t gmb_scene_move(double forward, double right)
{
    if (!isfinite(forward) || !isfinite(right))
    {
        gmb_3d_set_error("SCENE MOVE: valori non finiti");
        return GMB_3D_E_ARG;
    }

    /* AABB mondo in un buffer persistente (capacità = oggetti riservati): con la
     * geometria dinamica di P5 gli oggetti possono essere migliaia, quindi non
     * si usa più uno scratch fisso di 512. */
    int nb = 0;
    const gmb_3d_aabb_t* boxes = gmb_3d_scene_bounds_all(&nb);
    if (boxes == NULL) nb = 0;

    float eye[3], yaw, pitch;
    gmb_3d_scene_camera_get(eye, &yaw, &pitch);

    double a = (double)yaw * (ACT_PI / 180.0);
    double dx = forward * sin(a) + right * cos(a);
    double dz = forward * cos(a) - right * sin(a);

    double x = eye[0], z = eye[2], y = g_body.feet_y;
    const double x0 = x, z0 = z;
    int hit = 0;

    double len = sqrt(dx * dx + dz * dz);
    int nsub = 1;
    if (len > ACT_EPS)
    {
        nsub = (int)ceil(len / (g_body.radius * 0.5));
        if (nsub < 1) nsub = 1;
        if (nsub > ACT_MAX_SUBSTEPS) nsub = ACT_MAX_SUBSTEPS;
    }
    const double sx = nsub > 1 ? dx / (double)nsub : dx;
    const double sz = nsub > 1 ? dz / (double)nsub : dz;

    for (int si = 0; si < nsub && len > ACT_EPS; si++)
    {
        double px = x, pz = z;
        x += sx;
        z += sz;

        for (int pass = 0; pass < ACT_PUSH_PASSES; pass++)
        {
            int moved = 0;
            for (int i = 0; i < nb; i++)
            {
                const gmb_3d_aabb_t* b = &boxes[i];
                if (b->flags & 4) continue;
                if (!act_blocks(b, y, g_body.height, g_body.step)) continue;
                if (act_push_out(b, g_body.radius, &x, &z)) { moved = 1; hit |= GMB_3D_HIT_WALL; }
            }
            if (!moved) break;
        }

        double ceil = act_ceiling_at(boxes, nb, x, z, y);
        if (ceil != ACT_NO_PLANE && ceil - y < g_body.height - ACT_EPS)
        {
            x = px;
            z = pz;
            hit |= GMB_3D_HIT_WALL;
            continue;
        }

        double support = act_support_at(boxes, nb, x, z, y, g_body.step);
        if (support != ACT_NO_PLANE)
        {
            if (fabs(support - y) > ACT_EPS) hit |= GMB_3D_HIT_SECTOR;
            y = support;
        }
    }

    if (len > ACT_EPS && fabs(x - x0) + fabs(z - z0) < ACT_EPS) hit |= GMB_3D_HIT_STUCK;

    g_body.feet_y = y;
    g_body.hit = hit;
    gmb_3d_scene_camera_set((float)x, (float)(y + g_body.eye), (float)z, yaw, pitch);
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

/* --- quote (`M-FLOOR`) ---------------------------------------------------- */

double gmb_scene_floor_at(double x, double y, double z)
{
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) return ACT_NO_PLANE;

    /* AABB mondo in un buffer persistente (capacità = oggetti riservati): con la
     * geometria dinamica di P5 gli oggetti possono essere migliaia, quindi non
     * si usa più uno scratch fisso di 512. */
    int nb = 0;
    const gmb_3d_aabb_t* boxes = gmb_3d_scene_bounds_all(&nb);
    if (boxes == NULL) nb = 0;
    double best = ACT_NO_PLANE;
    for (int i = 0; i < nb; i++)
    {
        const gmb_3d_aabb_t* b = &boxes[i];
        if (x < b->min[0] - ACT_EPS || x > b->max[0] + ACT_EPS) continue;
        if (z < b->min[2] - ACT_EPS || z > b->max[2] + ACT_EPS) continue;
        if (b->max[1] > y + ACT_EPS) continue;
        if (best == ACT_NO_PLANE || b->max[1] > best) best = b->max[1];
    }
    return best;
}

double gmb_scene_ceil_at(double x, double y, double z)
{
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) return ACT_NO_PLANE;

    /* AABB mondo in un buffer persistente (capacità = oggetti riservati): con la
     * geometria dinamica di P5 gli oggetti possono essere migliaia, quindi non
     * si usa più uno scratch fisso di 512. */
    int nb = 0;
    const gmb_3d_aabb_t* boxes = gmb_3d_scene_bounds_all(&nb);
    if (boxes == NULL) nb = 0;
    double best = ACT_NO_PLANE;
    for (int i = 0; i < nb; i++)
    {
        const gmb_3d_aabb_t* b = &boxes[i];
        if (x < b->min[0] - ACT_EPS || x > b->max[0] + ACT_EPS) continue;
        if (z < b->min[2] - ACT_EPS || z > b->max[2] + ACT_EPS) continue;
        if (b->min[1] < y - ACT_EPS) continue;
        if (best == ACT_NO_PLANE || b->min[1] < best) best = b->min[1];
    }
    return best;
}

/* --- zone (`M-ZONE`) ------------------------------------------------------ */

int64_t gmb_scene_zone(int64_t id, double x1, double y1, double x2, double y2,
                       double z1, double z2, const char* tag)
{
    if (!isfinite(x1) || !isfinite(y1) || !isfinite(x2) || !isfinite(y2)
        || !isfinite(z1) || !isfinite(z2))
    {
        gmb_3d_set_error("SCENE ZONE: coordinate non finite");
        return GMB_3D_E_ARG;
    }
    if (g_nzone >= ACT_MAX_ZONES)
    {
        gmb_3d_set_error("SCENE ZONE: massimo 128 zone per scena");
        return GMB_3D_E_ARG;
    }

    act_zone_t* zn = &g_zone[g_nzone++];
    memset(zn, 0, sizeof(*zn));
    zn->used = 1;
    zn->id = (int)id;
    zn->x0 = (float)(x1 < x2 ? x1 : x2);
    zn->x1 = (float)(x1 < x2 ? x2 : x1);
    zn->y0 = (float)(y1 < y2 ? y1 : y2);
    zn->y1 = (float)(y1 < y2 ? y2 : y1);
    zn->z0 = (float)(z1 < z2 ? z1 : z2);
    zn->z1 = (float)(z1 < z2 ? z2 : z1);
    if (tag != NULL)
        snprintf(zn->tag, sizeof(zn->tag), "%s", tag);
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

/* La PRIMA zona creata che contiene il punto: l'ordine di creazione è parte
 * dell'API (M-ZONE), quindi non si ordina nulla. */
static const act_zone_t* act_zone_at(double x, double y, double z)
{
    for (int i = 0; i < g_nzone; i++)
    {
        const act_zone_t* zn = &g_zone[i];
        if (!zn->used) continue;
        if (x < zn->x0 - ACT_EPS || x > zn->x1 + ACT_EPS) continue;
        if (z < zn->z0 - ACT_EPS || z > zn->z1 + ACT_EPS) continue;
        if (y < zn->y0 - ACT_ZONE_SLACK || y > zn->y1 + ACT_EPS) continue;
        return zn;
    }
    return NULL;
}

int64_t gmb_scene_zone_at(double x, double y, double z)
{
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) return -1;
    const act_zone_t* zn = act_zone_at(x, y, z);
    return zn ? (int64_t)zn->id : -1;
}

const char* gmb_scene_zonetag_text(int64_t id)
{
    for (int i = 0; i < g_nzone; i++)
        if (g_zone[i].used && g_zone[i].id == (int)id) return g_zone[i].tag;
    return "";
}

/* Letture camera/corpo: la camera è di gmb_3d_cmd.c (una sola), il corpo e
 * l'ultimo HIT sono di questo modulo. */
static double cam_scalar(int which)
{
    float e[3], yaw = 0.0f, pitch = 0.0f;
    gmb_3d_scene_camera_get(e, &yaw, &pitch);
    switch (which)
    {
        case 0: return e[0];
        case 1: return e[1];
        case 2: return e[2];
        case 3: return yaw;
        default: return pitch;
    }
}

double gmb_scene_x(void) { return cam_scalar(0); }
double gmb_scene_y(void) { return cam_scalar(1); }
double gmb_scene_z(void) { return cam_scalar(2); }
double gmb_scene_eye(void) { return g_body.eye; }
double gmb_scene_yaw(void) { return cam_scalar(3); }
double gmb_scene_pitch(void) { return cam_scalar(4); }
double gmb_scene_hit(void) { return (double)g_body.hit; }
