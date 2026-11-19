/* gm_gfx_sector.c — settori e portali, rendering software (P2 del piano 3D).
 *
 * Semantica: docs/3D-Specifica-Comportamenti.md §2.2.
 *  - un raggio 2D per colonna, tracciato DENTRO i settori; il parametro del
 *    raggio È la profondità lungo la vista (dir = fronte + destra*t, non
 *    normalizzata): niente correzione fish-eye;
 *  - pavimenti e soffitti per pixel (prospettici, ancorati alle x,y del mondo),
 *    pareti per colonna (1 unità = 1 texel, ancorate alla quota del mondo);
 *  - portale: si disegnano i gradini superiore/inferiore, si restringe la
 *    finestra di righe e il raggio prosegue nel settore dietro (max 64);
 *  - guardare su/giù è uno shear dell'orizzonte, non un tilt.
 *
 * Ogni pixel scritto lascia in `depth` la profondità; i pixel che nessuna
 * geometria copre (mondo non chiuso, W-INV-RAY) restano a 0 e diventano colore
 * di fondo: un buco non deve mai essere nero per caso.
 */
#include "gm_gfx_sector.h"

#include <math.h>
#include <string.h>

#define SEC_EPS   1.0e-6
#define SEC_HUGE  1.0e30
#define SEC_PI    3.14159265358979323846
#define SEC_TAU   (2.0 * SEC_PI)

typedef struct sec_ctx {
    const gm_sector_world *w;
    unsigned int *argb;
    float *depth;
    int wpx, hpx;
    gm_real ex, ey, ez;      /* occhio */
    gm_real fx, fy;          /* fronte (unitaria) */
    gm_real rx, ry;          /* destra (unitaria, ortogonale) */
    gm_real scale;           /* pixel per unità a distanza 1 */
    gm_real hy;              /* orizzonte in pixel (shear del pitch) */
    gm_real fog;
    unsigned int fog_rgb;
} sec_ctx;

static int clamp_i(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int wrap_i(int v, int n)
{
    int m = v % n;
    return (m < 0) ? m + n : m;
}

static int to_i(gm_real v) { return (int)floor(v + 0.5); }

static void pixel(sec_ctx *c, int x, int y, int r, int g, int b, gm_real d)
{
    if (y < 0 || y >= c->hpx) return;
    if (c->fog > 0.0)
    {
        gm_real k = 1.0 - d / c->fog;
        if (k < 0.0) k = 0.0;
        if (k < 1.0)
        {
            int fr = (int)((c->fog_rgb >> 16) & 0xFFu);
            int fg = (int)((c->fog_rgb >> 8) & 0xFFu);
            int fb = (int)(c->fog_rgb & 0xFFu);
            r = to_i(r * k + fr * (1.0 - k));
            g = to_i(g * k + fg * (1.0 - k));
            b = to_i(b * k + fb * (1.0 - k));
        }
    }
    r = clamp_i(r, 0, 255); g = clamp_i(g, 0, 255); b = clamp_i(b, 0, 255);
    c->argb[(size_t)y * (size_t)c->wpx + (size_t)x] =
        ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b;
    c->depth[(size_t)y * (size_t)c->wpx + (size_t)x] = (float)d;
}

static void texel(const gm_tex *t, int u, int v, int *r, int *g, int *b)
{
    const unsigned char *p = t->rgba +
        ((size_t)wrap_i(v, t->h) * (size_t)t->w + (size_t)wrap_i(u, t->w)) * 4u;
    *r = p[0]; *g = p[1]; *b = p[2];
}

/* Riga di schermo di una quota z a profondità lam (shear compreso). */
static int project(const sec_ctx *c, gm_real z, gm_real lam)
{
    return to_i(c->hy - (z - c->ez) * c->scale / lam);
}

/* Pavimento (is_ceil = 0) o soffitto (is_ceil = 1) fra due righe, mappato per
 * pixel: a ogni riga si ricava la profondità dal piano e da lì il punto nel
 * mondo, quindi la texture è ancorata alle x,y del mondo (W-ANCHOR). */
static void fill_plane(sec_ctx *c, int x, gm_real dx, gm_real dy, int y0, int y1,
                       gm_real z, int tex, int is_ceil)
{
    const gm_tex *t = (tex >= 0 && tex < c->w->ntex) ? &c->w->tex[tex] : NULL;
    for (int y = y0; y <= y1; y++)
    {
        /* Sopra l'orizzonte i raggi salgono (soffitto), sotto scendono (pavimento):
         * entrambi i termini cambiano segno, quindi la profondità è SEMPRE positiva
         * e si ricava dal dislivello |z - ez| sul modulo della distanza all'orizzonte. */
        gm_real den = is_ceil ? (c->hy - (y + 0.5)) : ((y + 0.5) - c->hy);
        if (!(den > SEC_EPS)) continue;
        gm_real d = (is_ceil ? (z - c->ez) : (c->ez - z)) * c->scale / den;
        if (!(d > 0.0)) continue;
        int r = 40, g = 44, b = 52;
        if (t) texel(t, (int)floor(c->ex + dx * d), (int)floor(c->ey + dy * d), &r, &g, &b);
        pixel(c, x, y, r, g, b, d);
    }
}

/* Cielo: mappato per angolo attorno all'osservatore (W-SKY), una colonna alla
 * volta; senza texture, un gradiente verticale fisso. */
static void fill_sky(sec_ctx *c, int x, int y0, int y1, gm_real ang)
{
    const gm_tex *t = (c->w->tex_sky >= 0 && c->w->tex_sky < c->w->ntex)
                    ? &c->w->tex[c->w->tex_sky] : NULL;
    int col = (int)floor((ang / SEC_TAU) * (t ? (gm_real)t->w : 1.0));
    for (int y = y0; y <= y1; y++)
    {
        int r = 32, g = 56, b = 96;
        gm_real k = (c->hpx > 1) ? (gm_real)y / (gm_real)(c->hpx - 1) : 0.0;
        r = to_i(r + (90 - r) * k); g = to_i(g + (140 - g) * k); b = to_i(b + (200 - b) * k);
        if (t) texel(t, col, y - to_i(c->hy), &r, &g, &b);
        pixel(c, x, y, r, g, b, SEC_HUGE * 0.5);   /* il cielo non copre nulla */
    }
}

/* Fascia di parete (o gradino di portale) fra due righe, a profondità lam. */
static void fill_wall(sec_ctx *c, int x, gm_real lam, gm_real along,
                      const gm_sector_wall *wall, int y0, int y1)
{
    const gm_tex *t = (wall->tex >= 0 && wall->tex < c->w->ntex) ? &c->w->tex[wall->tex] : NULL;
    int u = (int)floor(along);
    for (int y = y0; y <= y1; y++)
    {
        gm_real z = c->ez + (c->hy - (y + 0.5)) * lam / c->scale;
        int r = 96, g = 96, b = 104;
        if (t) texel(t, u, (int)floor(z), &r, &g, &b);
        pixel(c, x, y, r, g, b, lam);
    }
}

static void column(sec_ctx *c, int x, gm_real dx, gm_real dy, int sec,
                   gm_real entry, int win_top, int win_bot, int left)
{
    const gm_sector *s = &c->w->sectors[sec];

    /* Parete più vicina del settore corrente oltre l'ingresso (convesso). */
    gm_real best = SEC_HUGE;
    int bw = -1;
    gm_real bu = 0.0, blen = 1.0;
    for (int i = 0; i < s->nwalls; i++)
    {
        const gm_sector_wall *w = &c->w->walls[s->walls[i]];
        gm_real ex = w->x1 - w->x0, ey = w->y1 - w->y0;
        gm_real den = dx * ey - dy * ex;
        if (den > -SEC_EPS && den < SEC_EPS) continue;
        gm_real px = w->x0 - c->ex, py = w->y0 - c->ey;
        gm_real lam = (px * ey - py * ex) / den;
        gm_real u = (px * dy - py * dx) / den;
        if (u < 0.0 || u > 1.0) continue;
        if (lam <= entry + SEC_EPS) continue;
        if (lam < best)
        {
            best = lam; bw = s->walls[i]; bu = u;
            blen = sqrt(ex * ex + ey * ey);
        }
    }
    if (bw < 0) return;   /* settore non chiuso: i pixel restano al fondo */

    const gm_sector_wall *w = &c->w->walls[bw];
    int y_ceil = project(c, s->ceil_z, best);
    int y_floor = project(c, s->floor_z, best);

    /* Piani fino alla parete, dentro la finestra ancora aperta. */
    if (s->flags & GM_SECTOR_SKY)
        fill_sky(c, x, win_top, clamp_i(y_ceil - 1, win_top - 1, win_bot), atan2(dy, dx));
    else if (y_ceil > win_top)
        fill_plane(c, x, dx, dy, win_top, clamp_i(y_ceil - 1, win_top, win_bot),
                   s->ceil_z, s->tex_ceil, 1);
    if (y_floor < win_bot)
        fill_plane(c, x, dx, dy, clamp_i(y_floor + 1, win_top, win_bot), win_bot,
                   s->floor_z, s->tex_floor, 0);

    int y_top = project(c, w->top, best);
    int y_bot = project(c, w->bottom, best);
    gm_real along = bu * blen;

    if (w->to < 0 || left <= 0)
    {
        /* Parete solida: la colonna finisce qui. */
        int y0 = clamp_i(y_top, win_top, win_bot), y1 = clamp_i(y_bot, win_top, win_bot);
        if (y1 >= y0) fill_wall(c, x, best, along, w, y0, y1);
        return;
    }

    /* Portale: gradino superiore/inferiore, poi si prosegue nel settore dietro. */
    const gm_sector *n = &c->w->sectors[w->to];
    int y_next_ceil  = project(c, n->ceil_z, best);
    int y_next_floor = project(c, n->floor_z, best);
    if (y_next_ceil > y_top)
        fill_wall(c, x, best, along, w, clamp_i(y_top, win_top, win_bot),
                  clamp_i(y_next_ceil - 1, win_top, win_bot));
    if (y_next_floor < y_bot)
        fill_wall(c, x, best, along, w, clamp_i(y_next_floor + 1, win_top, win_bot),
                  clamp_i(y_bot, win_top, win_bot));

    int nt = clamp_i(y_next_ceil, win_top, win_bot);
    int nb = clamp_i(y_next_floor, win_top, win_bot);
    if (nt > nb) return;   /* apertura fuori dalla finestra: niente da vedere */
    column(c, x, dx, dy, w->to, best, nt, nb, left - 1);
}

gm_status gm_sector_render(const gm_sector_world *w, const gm_sector_camera *cam,
                           unsigned int *argb, float *depth, int w_px, int h_px)
{
    if (!w || !cam || !argb || !depth || w_px <= 0 || h_px <= 0) return GM_ERR_ARG;
    if (!w->sectors || w->nsectors <= 0 || !w->walls || w->ntex < 0) return GM_ERR_ARG;

    sec_ctx c;
    memset(&c, 0, sizeof(c));
    c.w = w; c.argb = argb; c.depth = depth;
    c.wpx = w_px; c.hpx = h_px;
    c.ex = cam->x; c.ey = cam->y; c.ez = cam->z;
    c.fx = cos(cam->angle); c.fy = sin(cam->angle);
    c.rx = c.fy; c.ry = -c.fx;
    c.fog = w->fog; c.fog_rgb = w->fog_rgb;

    gm_real vfov = 2.0 * atan(tan(cam->fov * 0.5) * (gm_real)h_px / (gm_real)w_px);
    c.scale = ((gm_real)h_px * 0.5) / tan(vfov * 0.5);
    c.hy = (gm_real)h_px * 0.5 + tan(cam->pitch) * c.scale;

    /* Camera fuori da ogni settore: nessun raggio parte, il frame resta fondo. */
    int start = gm_sector_at(w, cam->x, cam->y, cam->z);
    if (start < 0) start = gm_sector_at(w, cam->x, cam->y, cam->z + 1.0);
    for (int y = 0; y < h_px; y++)
    {
        memset(depth + (size_t)y * (size_t)w_px, 0, (size_t)w_px * sizeof(float));
        memset(argb + (size_t)y * (size_t)w_px, 0, (size_t)w_px * sizeof(unsigned int));
    }
    if (start < 0) return GM_OK;

    gm_real half = ((gm_real)w_px * 0.5);
    gm_real tanx = tan(cam->fov * 0.5);
    for (int x = 0; x < w_px; x++)
    {
        gm_real t = (((gm_real)x + 0.5) - half) / half * tanx;
        gm_real dx = c.fx + c.rx * t;
        gm_real dy = c.fy + c.ry * t;
        column(&c, x, dx, dy, start, 0.0, 0, h_px - 1, GM_SECTOR_MAX_PORTALS);

        /* Buco (mondo non chiuso): colore di fondo, mai nero per caso. */
        for (int y = 0; y < h_px; y++)
        {
            size_t k = (size_t)y * (size_t)w_px + (size_t)x;
            if (depth[k] != 0.0f) continue;
            argb[k] = w->fog_rgb && w->fog > 0.0 ? (w->fog_rgb & 0xFFFFFFu) : 0x101418u;
        }
    }
    return GM_OK;
}

int gm_sector_at(const gm_sector_world *w, gm_real x, gm_real y, gm_real z)
{
    if (!w || !w->sectors) return -1;
    for (int i = 0; i < w->nsectors; i++)
    {
        const gm_sector *s = &w->sectors[i];
        if (z < s->floor_z || z > s->ceil_z) continue;
        int inside = 1, sign = 0;
        for (int j = 0; j < s->nwalls && inside; j++)
        {
            const gm_sector_wall *wall = &w->walls[s->walls[j]];
            gm_real ex = wall->x1 - wall->x0, ey = wall->y1 - wall->y0;
            gm_real px = x - wall->x0, py = y - wall->y0;
            gm_real cross = ex * py - ey * px;
            if (cross > SEC_EPS || cross < -SEC_EPS)
            {
                int sg = (cross > 0.0) ? 1 : -1;
                if (sign == 0) sign = sg;
                else if (sg != sign) inside = 0;
            }
        }
        if (inside && s->nwalls >= 3) return i;
    }
    return -1;
}
