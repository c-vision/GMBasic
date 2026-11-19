/* gmb_3d_move.c — corpo, MOVE con scivolamento/gradini/portali e HIT (P2).
 *
 * Semantica: docs/3D-Specifica-Comportamenti.md §2.3 (W-MOVE, W-PLAYER,
 * W-EYE-CLAMP). Il confine di §8.2 è rispettato: la geometria (settori,
 * pareti, portali) viene dalla suite (gm_gfx_sector), qui stanno il corpo, lo
 * stato e la semantica del comando.
 *
 *  - corpo = cilindro verticale: (x,y) sono i piedi, `radius` e `height` il
 *    volume; niente fisica generale, solo il volume contro le pareti. Quale
 *    settore contiene un punto lo dice la suite (gm_sector_contains): qui non
 *    c'è una seconda copia della geometria;
 *  - MOVE è relativo alla vista: angolo 0 = +x, 90° = +y, la destra di chi
 *    guarda è (sin a, -cos a) — la stessa convenzione del renderer;
 *  - una parete che non si può attraversare SPINGE (HIT bit 1): il centro
 *    viene riportato a un raggio dal segmento, quindi la componente obliqua
 *    sopravvive e il corpo scivola. Non si calcolano normali di velocità:
 *    si corregge la posizione, e lo scivolamento è la conseguenza;
 *  - un portale lascia passare se il corpo ci sta — apertura alta almeno
 *    `height`, larga almeno 2×raggio — e se il dislivello col pavimento dietro
 *    non supera `step` (HIT bit 4 sul cambio di settore). Scendere non è
 *    limitato: il gradino vale per la salita;
 *  - il movimento si spezza in sotto-passi ≤ raggio/2 (max 32): una parete
 *    sottile non si attraversa per tunneling anche a velocità alte;
 *  - HIT bit 2 quando la posizione finale coincide con quella iniziale.
 *
 * Nessuna allocazione, nessun accesso a file, nessuno stato globale: il
 * mondo è di sola lettura e il giocatore è del chiamante.
 */
#include "gmb_3d_internal.h"

#include <math.h>
#include <string.h>

#define MV_EPS          1.0e-6
#define MV_MAX_SUBSTEPS 32
#define MV_PUSH_PASSES  4      /* pareti consecutive in un angolo */

/* Direzione verso l'interno del settore a partire da un punto sul suo bordo:
 * media degli estremi delle pareti (il settore è convesso, quindi la media sta
 * dentro), senza dipendere dal verso di elenco. Serve solo quando il centro
 * del corpo cade esattamente sulla parete. */
static int inward_dir(const gm_sector_world* w, const gm_sector* s,
                      gm_real x, gm_real y, gm_real* nx, gm_real* ny)
{
    if (!s || s->nwalls <= 0) return 0;
    gm_real sx = 0.0, sy = 0.0;
    for (int i = 0; i < s->nwalls; i++)
    {
        const gm_sector_wall* wl = &w->walls[s->walls[i]];
        sx += wl->x0;
        sy += wl->y0;
    }
    sx = sx / (gm_real)s->nwalls - x;
    sy = sy / (gm_real)s->nwalls - y;
    gm_real l = sqrt(sx * sx + sy * sy);
    if (!(l > MV_EPS)) return 0;
    *nx = sx / l;
    *ny = sy / l;
    return 1;
}

/* Un portale è attraversabile dal corpo che sta a quota `z`? Se sì scrive il
 * pavimento del settore dietro in *next_z. */
static int portal_passable(const gm_sector_world* w, const gm_sector_wall* wl,
                           gm_real z, gm_real radius, gm_real height, gm_real step,
                           gm_real* next_z)
{
    if (wl->to < 0 || wl->to >= w->nsectors) return 0;
    if (wl->flags & GM_SECTOR_W_MOVE_BLOCK) return 0;   /* finestra, ringhiera (W-FLAGS) */
    const gm_sector* n = &w->sectors[wl->to];

    gm_real ex = wl->x1 - wl->x0, ey = wl->y1 - wl->y0;
    gm_real len = sqrt(ex * ex + ey * ey);
    if (len < 2.0 * radius - MV_EPS) return 0;              /* apertura più stretta del corpo */
    if (n->floor_z > z + step + MV_EPS) return 0;           /* gradino troppo alto */
    if (n->floor_z + height > wl->top + MV_EPS) return 0;   /* apertura più bassa del corpo */
    if (n->floor_z < wl->bottom - MV_EPS) return 0;         /* il varco parte sopra il pavimento dietro */
    *next_z = n->floor_z;
    return 1;
}

/* Distanza del punto dal segmento; *qx,*qy = punto più vicino. */
static gm_real closest_on_wall(const gm_sector_wall* wl, gm_real px, gm_real py,
                               gm_real* qx, gm_real* qy)
{
    gm_real ex = wl->x1 - wl->x0, ey = wl->y1 - wl->y0;
    gm_real len2 = ex * ex + ey * ey;
    gm_real t = (len2 > MV_EPS) ? ((px - wl->x0) * ex + (py - wl->y0) * ey) / len2 : 0.0;
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;
    *qx = wl->x0 + ex * t;
    *qy = wl->y0 + ey * t;
    gm_real dx = px - *qx, dy = py - *qy;
    return sqrt(dx * dx + dy * dy);
}

/* Riporta il centro fuori dalle pareti che il corpo non può attraversare.
 * Ritorna 1 se almeno una parete ha spinto. */
static int push_out(const gm_sector_world* w, const gm_sector* s, gm_real z,
                    gm_real radius, gm_real height, gm_real step,
                    gm_real* x, gm_real* y)
{
    int pushed = 0;
    for (int pass = 0; pass < MV_PUSH_PASSES; pass++)
    {
        int moved = 0;
        for (int i = 0; i < s->nwalls; i++)
        {
            const gm_sector_wall* wl = &w->walls[s->walls[i]];
            gm_real nz;
            if (portal_passable(w, wl, z, radius, height, step, &nz)) continue;

            gm_real qx, qy;
            gm_real d = closest_on_wall(wl, *x, *y, &qx, &qy);
            if (d >= radius - MV_EPS) continue;

            /* La spinta riporta il centro DAL lato in cui si trova già: se è
             * dentro il settore lo tiene dentro, se è fuori non lo tira
             * attraverso il muro (l'uscita la decide substep). */
            gm_real nx, ny;
            if (d > MV_EPS) { nx = (*x - qx) / d; ny = (*y - qy) / d; }
            /* centro esattamente sulla parete: si va verso l'interno */
            else if (!inward_dir(w, s, qx, qy, &nx, &ny)) continue;

            *x = qx + nx * radius;
            *y = qy + ny * radius;
            moved = 1;
            pushed = 1;
        }
        if (!moved) break;
    }
    return pushed;
}

/* Un sotto-passo di (dx,dy). Ritorna i bit HIT del passo; *blocked = 1 se il
 * passo è stato annullato (nessun portale per uscire dal settore). */
static int substep(const gm_sector_world* w, gm_real* x, gm_real* y, gm_real* z,
                   int* sec, gm_real radius, gm_real height, gm_real step,
                   gm_real dx, gm_real dy, int* blocked)
{
    gm_real px = *x, py = *y;
    const gm_sector* s = &w->sectors[*sec];

    *x += dx;
    *y += dy;
    int pushed = push_out(w, s, *z, radius, height, step, x, y);
    *blocked = 0;

    if (gm_sector_contains(w, *sec, *x, *y))
        return pushed ? GMB_3D_HIT_WALL : 0;

    /* Uscito dal settore: può farlo solo attraverso un portale del settore
     * corrente, e solo se il corpo ci sta (portal_passable). */
    for (int i = 0; i < s->nwalls; i++)
    {
        const gm_sector_wall* wl = &w->walls[s->walls[i]];
        gm_real nz;
        if (!portal_passable(w, wl, *z, radius, height, step, &nz)) continue;
        if (!gm_sector_contains(w, wl->to, *x, *y)) continue;

        *sec = wl->to;
        *z = nz;
        return GMB_3D_HIT_SECTOR | (pushed ? GMB_3D_HIT_WALL : 0);
    }

    /* Nessun portale: il passo si annulla. Una parete non si attraversa. */
    *x = px;
    *y = py;
    *blocked = 1;
    return GMB_3D_HIT_WALL;
}

void gmb_3d_player_defaults(gmb_3d_player_t* p)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->radius = 14.0f;               /* W-PLAYER */
    p->height = 56.0f;
    p->eye    = 46.0f;
    p->step   = 24.0f;
    p->fov    = 1.5707963f;
    p->far    = 4096.0f;
    p->sector = -1;
}

int gmb_3d_player_place(const gmb_3d_world_t* w, gmb_3d_player_t* p, float x, float y)
{
    if (!w || !p)
    {
        gmb_3d_set_error("player place: mondo o giocatore mancanti");
        return GMB_3D_E_ARG;
    }

    int best = -1;
    for (int i = 0; i < w->sw.nsectors; i++)
    {
        if (!gm_sector_contains(&w->sw, i, (gm_real)x, (gm_real)y)) continue;
        if (best < 0 || w->sw.sectors[i].floor_z > w->sw.sectors[best].floor_z) best = i;
    }
    if (best < 0)
    {
        gmb_3d_set_error("player place: posizione fuori da ogni settore");
        return GMB_3D_E_ARG;
    }

    p->x = x;
    p->y = y;
    p->z = (float)w->sw.sectors[best].floor_z;
    p->sector = best;
    p->hit = 0;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_player_move(const gmb_3d_world_t* w, gmb_3d_player_t* p,
                       float forward, float right)
{
    if (!w || !p)
    {
        gmb_3d_set_error("player move: mondo o giocatore mancanti");
        return GMB_3D_E_ARG;
    }
    if (p->sector < 0 || p->sector >= w->sw.nsectors)
    {
        if (gmb_3d_player_place(w, p, p->x, p->y) != GMB_3D_OK) return GMB_3D_E_ARG;
    }

    gm_real radius = p->radius > 0.0f ? (gm_real)p->radius : 14.0;
    gm_real height = p->height > 0.0f ? (gm_real)p->height : 56.0;
    gm_real step   = p->step   > 0.0f ? (gm_real)p->step   : 24.0;

    /* Relativo alla vista: fronte (cos,sin), destra (sin,-cos). */
    gm_real ang = (gm_real)p->angle;
    gm_real dx = (gm_real)forward * cos(ang) + (gm_real)right * sin(ang);
    gm_real dy = (gm_real)forward * sin(ang) - (gm_real)right * cos(ang);

    gm_real x = (gm_real)p->x, y = (gm_real)p->y, z = (gm_real)p->z;
    gm_real x0 = x, y0 = y;
    int sec = p->sector;
    int hit = 0;

    gm_real len = sqrt(dx * dx + dy * dy);
    if (len > MV_EPS)
    {
        int n = (int)ceil(len / (radius * 0.5));
        if (n < 1) n = 1;
        if (n > MV_MAX_SUBSTEPS) n = MV_MAX_SUBSTEPS;
        gm_real sx = dx / (gm_real)n, sy = dy / (gm_real)n;

        for (int i = 0; i < n; i++)
        {
            int blocked = 0;
            hit |= substep(&w->sw, &x, &y, &z, &sec, radius, height, step,
                           sx, sy, &blocked);
            if (blocked) break;   /* parete piena: insistere non serve */
        }
    }

    p->sector = sec;
    p->x = (float)x;
    p->y = (float)y;
    p->z = (float)z;

    /* Bit 2: richiesta di moto ma posizione finale identica a quella iniziale. */
    if (len > MV_EPS && fabs(x - x0) + fabs(y - y0) < MV_EPS) hit |= GMB_3D_HIT_STUCK;
    p->hit = hit;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_player_view(const gmb_3d_world_t* w, const gmb_3d_player_t* p,
                       gmb_3d_camera_t* cam)
{
    if (!w || !p || !cam)
    {
        gmb_3d_set_error("player view: mondo, giocatore o camera mancanti");
        return GMB_3D_E_ARG;
    }
    if (p->sector < 0 || p->sector >= w->sw.nsectors)
    {
        gmb_3d_set_error("player view: settore del giocatore non valido");
        return GMB_3D_E_ARG;
    }

    const gm_sector* s = &w->sw.sectors[p->sector];
    gm_real eye = p->eye > 0.0f ? (gm_real)p->eye : 46.0;
    gm_real z = (gm_real)p->z + eye;

    /* W-EYE-CLAMP: l'occhio resta 2 unità sotto il soffitto del settore. */
    gm_real top = s->ceil_z - 2.0;
    if (z > top) z = top;

    cam->x = p->x;
    cam->y = p->y;
    cam->z = (float)z;
    cam->angle = p->angle;
    cam->pitch = p->pitch;
    cam->fov = p->fov > 0.0f ? p->fov : 1.5707963f;
    cam->far = p->far > 0.0f ? p->far : 4096.0f;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}
