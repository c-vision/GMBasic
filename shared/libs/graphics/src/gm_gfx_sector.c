/* gm_gfx_sector.c — settori e portali, rendering software (P2 del piano 3D).
 *
 * Semantica: docs/3D-Specifica-Comportamenti.md §2.2.
 *  - un raggio 2D per colonna, tracciato DENTRO i settori; il parametro del
 *    raggio È la profondità lungo la vista (dir = fronte + destra*t, non
 *    normalizzata): niente correzione fish-eye;
 *  - pavimenti e soffitti per pixel (prospettici, ancorati alle x,y del mondo),
 *    pareti per colonna (1 unità = 1 texel, ancorate alla quota del mondo);
 *  - portale: la finestra di righe si restringe all'apertura, la parete resta
 *    solida sopra e sotto di essa, e il raggio prosegue nel settore dietro
 *    (max 64); una parete con flag 16 si disegna in MEZZO a ciò che sta dietro
 *    (W-TRANS: media per canale);
 *  - guardare su/giù è uno shear dell'orizzonte, non un tilt;
 *  - luce = moltiplicatore del settore (LIGHTFX) più le lampade puntiformi con
 *    falloff lineare (W-LIGHT); senza lampade e a luce piena non costa nulla.
 *
 * Ogni pixel scritto lascia in `depth` la profondità; i pixel che nessuna
 * geometria copre (mondo non chiuso, W-INV-RAY) restano a 0 e tengono il colore
 * di fondo scritto all'inizio: un buco non deve mai essere nero per caso.
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

/* Finestra di righe: [*y0,*y1] ristretta a [lo,hi]; falso se non resta nulla. */
static int clip_span(int *y0, int *y1, int lo, int hi)
{
    if (*y0 < lo) *y0 = lo;
    if (*y1 > hi) *y1 = hi;
    return *y0 <= *y1;
}

/* Luce del punto: moltiplicatore del settore (0 = buio, 1 = piena) più le
 * lampade (W-LIGHT: solo schiariscono, falloff lineare). Salta tutto quando non
 * c'è nulla da fare. */
static void shade(const sec_ctx *c, gm_real base, gm_real x, gm_real y, gm_real z,
                  int *r, int *g, int *b)
{
    gm_real k = base;
    for (int i = 0; i < c->w->nlamps; i++)
    {
        const gm_sector_lamp *l = &c->w->lamps[i];
        gm_real dx = x - l->x, dy = y - l->y, dz = z - l->z;
        gm_real d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= l->radius * l->radius) continue;
        k += l->level * (1.0 - sqrt(d2) / l->radius);
    }
    if (k == 1.0) return;
    *r = clamp_i((int)(*r * k), 0, 255);
    *g = clamp_i((int)(*g * k), 0, 255);
    *b = clamp_i((int)(*b * k), 0, 255);
}

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
    /* r,g,b arrivano già in 0..255: texel e shade limitano, il fog è convesso. */
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

/* Pavimento (is_floor) o soffitto del settore, per pixel: a ogni riga si ricava
 * la profondità dal piano e da lì il punto nel mondo, quindi la texture è
 * ancorata alle x,y del mondo (W-ANCHOR) più l'offset di SCROLL. Una riga che
 * guarda il piano dal lato sbagliato (sopra l'orizzonte per un pavimento) non
 * lo vede. */
static void fill_plane(sec_ctx *c, int x, gm_real dx, gm_real dy, int y0, int y1,
                       const gm_sector *s, int is_floor)
{
    gm_real z = is_floor ? s->floor_z : s->ceil_z;
    int tex = is_floor ? s->tex_floor : s->tex_ceil;
    const gm_tex *t = (tex >= 0 && tex < c->w->ntex) ? &c->w->tex[tex] : NULL;
    gm_real dz = z - c->ez;
    for (int y = y0; y <= y1; y++)
    {
        gm_real den = ((gm_real)y + 0.5) - c->hy;
        if (dz * den >= 0.0) continue;
        gm_real d = fabs(dz * c->scale / den);
        gm_real wx = c->ex + dx * d, wy = c->ey + dy * d;
        int r = 40, g = 44, b = 52;
        if (t)
            texel(t, (int)floor(wx + s->scroll_x), (int)floor(wy + s->scroll_y), &r, &g, &b);
        shade(c, s->light, wx, wy, z, &r, &g, &b);
        pixel(c, x, y, r, g, b, d);
    }
}

/* Cielo: mappato per angolo attorno all'osservatore (W-SKY), una colonna alla
 * volta; senza texture, un gradiente verticale fisso. */
static void fill_sky(sec_ctx *c, int x, int y0, int y1, gm_real ang)
{
    const gm_tex *t = (c->w->tex_sky >= 0 && c->w->tex_sky < c->w->ntex)
                    ? &c->w->tex[c->w->tex_sky] : NULL;
    for (int y = y0; y <= y1; y++)
    {
        int r, g, b;
        if (t)
            texel(t, (int)floor((ang / SEC_TAU) * (gm_real)t->w), y - to_i(c->hy), &r, &g, &b);
        else
        {
            gm_real k = (c->hpx > 1) ? (gm_real)y / (gm_real)(c->hpx - 1) : 0.0;
            r = to_i(32.0 + (90.0 - 32.0) * k);
            g = to_i(56.0 + (140.0 - 56.0) * k);
            b = to_i(96.0 + (200.0 - 96.0) * k);
        }
        pixel(c, x, y, r, g, b, SEC_HUGE);   /* il cielo non copre nulla */
    }
}

/* Parete colpita dal raggio in questa colonna: quel che serve alle fasce,
 * calcolato una volta. `top`/`bot` sono la finestra di righe ancora aperta a
 * questo livello di ricorsione. */
typedef struct sec_hit {
    const gm_sector      *s;
    const gm_sector_wall *w;
    gm_real lam;      /* profondità lungo la vista */
    gm_real along;    /* coordinata u sulla parete */
    gm_real x, y;     /* punto nel mondo, per la luce */
    int     col;      /* colonna di schermo */
    int     top, bot;
} sec_hit;

/* Fascia di parete (o frontone del gradino) fra due righe, ritagliata nella
 * finestra del livello. Con `glass` (flag 16) il colore si media per canale con
 * ciò che il raggio ha già disegnato dietro (W-TRANS: (a+b)>>1). */
static void fill_wall(sec_ctx *c, const sec_hit *h, int y0, int y1, int glass)
{
    if (!clip_span(&y0, &y1, h->top, h->bot)) return;
    const gm_sector_wall *wall = h->w;
    const gm_tex *t = (wall->tex >= 0 && wall->tex < c->w->ntex) ? &c->w->tex[wall->tex] : NULL;
    int u = (int)floor(h->along + wall->scroll_u);
    for (int y = y0; y <= y1; y++)
    {
        gm_real z = c->ez + (c->hy - (y + 0.5)) * h->lam / c->scale;
        int r = 96, g = 96, b = 104;
        if (t) texel(t, u, (int)floor(z + wall->scroll_v), &r, &g, &b);
        shade(c, h->s->light, h->x, h->y, z, &r, &g, &b);
        if (!glass)
        {
            pixel(c, h->col, y, r, g, b, h->lam);
            continue;
        }
        size_t k = (size_t)y * (size_t)c->wpx + (size_t)h->col;
        unsigned int back = c->argb[k];
        c->argb[k] = ((unsigned int)((r + (int)((back >> 16) & 0xFFu)) >> 1) << 16) |
                     ((unsigned int)((g + (int)((back >> 8) & 0xFFu)) >> 1) << 8) |
                      (unsigned int)((b + (int)(back & 0xFFu)) >> 1);
        c->depth[k] = (float)h->lam;
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

    sec_hit h;
    h.s = s;
    h.w = &c->w->walls[bw];
    h.lam = best;
    h.along = bu * blen;
    h.x = c->ex + dx * best;
    h.y = c->ey + dy * best;
    h.col = x;
    h.top = win_top;
    h.bot = win_bot;

    int y_ceil  = project(c, s->ceil_z, best);
    int y_floor = project(c, s->floor_z, best);
    int y_top   = project(c, h.w->top, best);
    int y_bot   = project(c, h.w->bottom, best);

    /* Piani fino alla parete, dentro la finestra ancora aperta. */
    int c0 = win_top, c1 = y_ceil - 1;
    if (clip_span(&c0, &c1, win_top, win_bot))
    {
        if (s->flags & GM_SECTOR_SKY) fill_sky(c, x, c0, c1, atan2(dy, dx));
        else fill_plane(c, x, dx, dy, c0, c1, s, 0);
    }
    int f0 = y_floor + 1, f1 = win_bot;
    if (clip_span(&f0, &f1, win_top, win_bot))
        fill_plane(c, x, dx, dy, f0, f1, s, 1);

    if (h.w->to < 0 || left <= 0)
    {
        fill_wall(c, &h, y_top, y_bot, 0);   /* parete solida: la colonna finisce qui */
        return;
    }

    /* Portale: fuori dall'apertura la parete resta solida — fascia sopra il
     * varco fino al soffitto di questo settore, fascia sotto fino al suo
     * pavimento. Dentro il varco si prosegue nel settore dietro; se il
     * pavimento dietro è più alto, la banda fino all'apertura è il frontone
     * del gradino. */
    int nb = y_bot;
    int y_next_floor = project(c, c->w->sectors[h.w->to].floor_z, best);
    if (y_next_floor < nb)
    {
        fill_wall(c, &h, y_next_floor + 1, nb, 0);
        nb = y_next_floor;
    }
    fill_wall(c, &h, y_ceil, y_top - 1, 0);
    fill_wall(c, &h, y_bot + 1, y_floor, 0);

    int nt = y_top;
    if (!clip_span(&nt, &nb, win_top, win_bot)) return;   /* apertura fuori finestra */
    column(c, x, dx, dy, h.w->to, best, nt, nb, left - 1);

    /* Vetro (W-TRANS): disegnato DOPO il settore dietro, in mezzo ai due. */
    if ((h.w->flags & GM_SECTOR_W_TRANS) && left > 1)
        fill_wall(c, &h, nt, nb, 1);
}

gm_status gm_sector_render(const gm_sector_world *w, const gm_sector_camera *cam,
                           unsigned int *argb, float *depth, int w_px, int h_px)
{
    if (!w || !cam || !argb || !depth || w_px <= 0 || h_px <= 0) return GM_ERR_ARG;
    if (!w->sectors || w->nsectors <= 0 || !w->walls || w->ntex < 0) return GM_ERR_ARG;
    if (w->nlamps < 0 || w->nlamps > GM_SECTOR_MAX_LAMPS) return GM_ERR_ARG;

    /* Fondo: dove nessuna geometria scrive (mondo non chiuso, W-INV-RAY) resta
     * questo colore, mai nero per caso. */
    unsigned int bg = (w->fog > 0.0 && w->fog_rgb) ? (w->fog_rgb & 0xFFFFFFu) : 0x101418u;
    size_t npix = (size_t)w_px * (size_t)h_px;
    for (size_t i = 0; i < npix; i++)
    {
        argb[i] = bg;
        depth[i] = 0.0f;
    }

    /* Camera fuori da ogni settore: nessun raggio parte (W-CAMERA). */
    int start = gm_sector_at(w, cam->x, cam->y, cam->z);
    if (start < 0) return GM_OK;

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

    gm_real half = ((gm_real)w_px * 0.5);
    gm_real tanx = tan(cam->fov * 0.5);
    for (int x = 0; x < w_px; x++)
    {
        gm_real t = (((gm_real)x + 0.5) - half) / half * tanx;
        gm_real dx = c.fx + c.rx * t;
        gm_real dy = c.fy + c.ry * t;
        column(&c, x, dx, dy, start, 0.0, 0, h_px - 1, GM_SECTOR_MAX_PORTALS);
    }
    return GM_OK;
}

int gm_sector_contains(const gm_sector_world *w, int sec, gm_real x, gm_real y)
{
    if (!w || !w->sectors || !w->walls || sec < 0 || sec >= w->nsectors) return 0;
    const gm_sector *s = &w->sectors[sec];
    if (s->nwalls < 3) return 0;
    int sign = 0;
    for (int i = 0; i < s->nwalls; i++)
    {
        const gm_sector_wall *wall = &w->walls[s->walls[i]];
        gm_real ex = wall->x1 - wall->x0, ey = wall->y1 - wall->y0;
        gm_real cross = ex * (y - wall->y0) - ey * (x - wall->x0);
        if (cross > SEC_EPS || cross < -SEC_EPS)
        {
            int sg = (cross > 0.0) ? 1 : -1;
            if (sign == 0) sign = sg;
            else if (sg != sign) return 0;
        }
    }
    return 1;
}

int gm_sector_at(const gm_sector_world *w, gm_real x, gm_real y, gm_real z)
{
    if (!w || !w->sectors) return -1;
    for (int i = 0; i < w->nsectors; i++)
    {
        const gm_sector *s = &w->sectors[i];
        if (z < s->floor_z || z > s->ceil_z) continue;
        if (gm_sector_contains(w, i, x, y)) return i;
    }
    return -1;
}
