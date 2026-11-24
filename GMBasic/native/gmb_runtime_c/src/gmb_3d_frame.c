/* gmb_3d_frame.c — target 3D, cattura headless e guardie di render (P1).
 *
 * Perché qui e non nella suite: lo STATO e il ponte verso la pagina/present
 * stanno nel runtime (piano §3.1.3 — la dylib FFI e la copia statica sono due
 * istanze). La matematica e la rasterizzazione restano in gm_gfx_* (suite).
 *
 * Il modulo NON include header SDL (R2) e non alloca per frame (R4).
 * Le funzioni GPM sono riferite FORTI (nessun __attribute__((weak))): se lo
 * scheduler non è nel link, il modulo NON si linka. Con i riferimenti deboli
 * la guardia di R1/R3/R5 diventava silenziosamente inerte — un binario senza
 * GPM accettava RENDER come se nessuno stesse renderizzando, senza dirlo. Chi
 * gira senza scheduler (oracolo, tool, selfcheck — T3) linka comunque il
 * runtime, che contiene gmb_c99gpm.c: la guardia c'è sempre.
 */
#include "gmb_3d_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "gmb_c99gpm.h"
/* Campionamento delle texture (wrap + nearest/bilineare): UNA sola sede, la
 * suite. Il ciclo del framebuffer invece sta qui perché il target del runtime è
 * ARGB+depth, non il framebuffer RGB float di gm_raster (§0bis: si condivide il
 * campionatore e la matematica, non si duplica la stessa funzione). */
#include "gm_gfx_raster.h"
/* Scrittore PNG del runtime (gmb_graphics_screenshot.c): stessa implementazione
 * dello screenshot 2D, nessuna seconda copia (§0bis). */
extern int save_png_rgb24(const char* path, const uint32_t* px, int w, int h);

/* --- stato del modulo ---------------------------------------------------- */

static volatile int g_present_mode = GMB_3D_MODE_TARGET;
static unsigned long g_alloc_count = 0;

/* Proprietà del renderer in UN SOLO word (CAS): 0 = libero, &g_no_task = claim
 * aperto senza scheduler (T3: tool, oracolo, selfcheck), altrimenti il task GPM
 * proprietario. Un solo word rende acquisizione, chiusura e recupero atomici:
 * non esiste una finestra in cui il claim è libero ma un proprietario lo crede
 * suo (era il caso di busy + owner separati). */
static gmb_task_t g_no_task;                      /* sentinella T3, mai dereferenziata */
static volatile uintptr_t g_render_claim = 0;

static char g_last_error[192];

static void set_err(const char* msg)
{
    g_last_error[0] = '\0';
    if (msg && *msg) snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
}

void gmb_3d_set_error(const char* msg)
{
    set_err(msg);
}

static gmb_task_t* claim_get(void)
{
    return (gmb_task_t*)g_render_claim;
}

static int claim_take(gmb_task_t* expected, gmb_task_t* want)
{
    return __sync_bool_compare_and_swap(&g_render_claim, (uintptr_t)expected, (uintptr_t)want);
}

/* Identità del chiamante: il task GPM; senza task (T3) la sentinella, così due
 * entità diverse non si scambiano per proprietarie. */
static gmb_task_t* claim_of(gmb_task_t* me)
{
    return (me != NULL) ? me : &g_no_task;
}

/* Un claim è orfano quando nessuno può più chiuderlo: aperto senza task (T3),
 * nato prima dello scheduler, oppure lasciato da un task FINITO senza RENDER
 * END. I task non vengono liberati finché lo scheduler è attivo (gmb_c99gpm.c),
 * quindi `claim` è dereferenziabile solo in quel caso: prima si controlla
 * gmb_c99gpm_active(). I chiamanti non passano mai NULL (word 0 = libero). */
static int claim_orphan(gmb_task_t* claim)
{
    if (claim == &g_no_task) return 1;
    if (!gmb_c99gpm_active()) return 1;
    return gmb_task_finished(claim);
}

/* --- target -------------------------------------------------------------- */

int gmb_3d_target_alloc(gmb_3d_target_t* t, int w, int h)
{
    if (t == NULL || w <= 0 || h <= 0) { set_err("target: dimensioni non valide"); return GMB_3D_E_ARG; }
    if (t->argb != NULL && t->depth != NULL && t->w == w && t->h == h) return GMB_3D_OK;  /* riuso */

    gmb_3d_target_free(t);

    size_t n = (size_t)w * (size_t)h;
    uint32_t* argb = (uint32_t*)calloc(n, sizeof(uint32_t));
    float* depth = (float*)malloc(n * sizeof(float));
    if (argb == NULL || depth == NULL)
    {
        free(argb);
        free(depth);
        set_err("target: memoria esaurita");
        return GMB_3D_E_NOMEM;
    }
    t->w = w;
    t->h = h;
    t->argb = argb;
    t->depth = depth;
    g_alloc_count += 2;   /* i due buffer del target */
    set_err("");
    return GMB_3D_OK;
}

void gmb_3d_target_free(gmb_3d_target_t* t)
{
    if (t == NULL) return;
    free(t->argb);
    free(t->depth);
    t->argb = NULL;
    t->depth = NULL;
    t->w = 0;
    t->h = 0;
}

int gmb_3d_target_clear(gmb_3d_target_t* t, uint32_t argb, float depth)
{
    if (t == NULL || t->argb == NULL || t->depth == NULL) { set_err("target: non allocato"); return GMB_3D_E_ARG; }
    size_t n = (size_t)t->w * (size_t)t->h;
    uint32_t c = argb & 0xFFFFFFu;
    for (size_t i = 0; i < n; i++)
    {
        t->argb[i] = c;
        t->depth[i] = depth;
    }
    return GMB_3D_OK;
}

int gmb_3d_target_blit_argb(gmb_3d_target_t* t, const uint32_t* src, int w, int h)
{
    if (t == NULL || t->argb == NULL || src == NULL || w <= 0 || h <= 0)
    {
        set_err("target: blit con argomenti non validi");
        return GMB_3D_E_ARG;
    }
    if (w != t->w || h != t->h)
    {
        set_err("target: blit di dimensioni diverse (nessuna scalatura implicita)");
        return GMB_3D_E_ARG;
    }
    /* NULL mode: RENDER aggiorna lo stato ma non disegna (R8, T-NULL). */
    if (g_present_mode == GMB_3D_MODE_NULL) return GMB_3D_OK;

    size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; i++) t->argb[i] = src[i] & 0xFFFFFFu;
    return GMB_3D_OK;
}

int gmb_3d_target_capture_argb(const gmb_3d_target_t* t, uint32_t* out, int cap_px)
{
    if (t == NULL || t->argb == NULL || out == NULL || cap_px <= 0)
    {
        set_err("cattura: argomenti non validi");
        return GMB_3D_E_ARG;
    }
    size_t n = (size_t)t->w * (size_t)t->h;
    if ((size_t)cap_px < n) n = (size_t)cap_px;
    for (size_t i = 0; i < n; i++) out[i] = t->argb[i] & 0xFFFFFFu;
    return (int)n;
}

int gmb_3d_target_capture_png(const gmb_3d_target_t* t, const char* path)
{
    if (t == NULL || t->argb == NULL || path == NULL || *path == '\0')
    {
        set_err("cattura PNG: argomenti non validi");
        return GMB_3D_E_ARG;
    }
    if (save_png_rgb24(path, t->argb, t->w, t->h) != 0)
    {
        set_err("cattura PNG: scrittura fallita");
        return GMB_3D_E_IO;
    }
    return GMB_3D_OK;
}

/* --- present e guardie --------------------------------------------------- */

void gmb_3d_present_mode_set(gmb_3d_mode_t mode)
{
    g_present_mode = (mode == GMB_3D_MODE_NULL) ? GMB_3D_MODE_NULL : GMB_3D_MODE_TARGET;
}

gmb_3d_mode_t gmb_3d_present_mode(void)
{
    return (g_present_mode == GMB_3D_MODE_NULL) ? GMB_3D_MODE_NULL : GMB_3D_MODE_TARGET;
}

int gmb_3d_render_begin(void)
{
    gmb_task_t* me = gmb_cur();

    if (gmb_c99gpm_active() && me == NULL)
    {
        set_err("RENDER fuori dal task GPM");
        return GMB_3D_E_NO_TASK;
    }

    /* Pretesa atomica: nessuna attesa, mai un lock (R3/R4). */
    gmb_task_t* mine = claim_of(me);
    if (claim_take(NULL, mine))
    {
        set_err("");
        return GMB_3D_OK;
    }
    gmb_task_t* held = claim_get();
    if (held == mine)
    {
        set_err("RENDER: chiamata ricorsiva");
        return GMB_3D_E_RECURSIVE;
    }
    /* Recupero di un claim che nessuno può più chiudere. Il CAS è sullo STESSO
     * word appena letto: se nel frattempo è comparso un proprietario vivo, il
     * recupero fallisce e si resta su E_BUSY. */
    if (claim_orphan(held) && claim_take(held, mine))
    {
        set_err("");
        return GMB_3D_OK;
    }
    set_err("RENDER: un altro task sta renderizzando");
    return GMB_3D_E_BUSY;
}

void gmb_3d_render_end(void)
{
    gmb_task_t* held = claim_get();
    if (held == NULL) return;              /* chiusura senza apertura: no-op */

    gmb_task_t* mine = claim_of(gmb_cur());
    if (held == mine)
    {
        if (claim_take(held, NULL)) set_err("");
        return;
    }
    if (!claim_orphan(held))
    {
        /* Il proprietario è vivo: il claim resta suo (R3). Non è uno stallo —
         * lo chiuderà lui; se sparisce, il prossimo END/BEGIN lo recupera. */
        set_err("RENDER: chiusura da un task che non possiede il renderer");
        return;
    }
    /* Nessuno può più chiuderlo (task finito senza RENDER END, claim nato prima
     * dello scheduler, scheduler spento): liberarlo qui evita che il renderer
     * resti occupato per sempre. */
    if (claim_take(held, NULL))
        set_err("RENDER: claim orfano liberato da un non-proprietario");
}

int gmb_3d_render_depth(void)
{
    return (claim_get() != NULL) ? 1 : 0;
}

unsigned long gmb_3d_alloc_count(void)
{
    return g_alloc_count;
}

const char* gmb_3d_last_error(void)
{
    return g_last_error;
}

/* ═══ P3 — rasterizzatore mesh sul target ARGB+depth ══════════════════════
 *
 * Il ciclo caldo è quello deciso in specifica §11: test depth → campiona texel
 * → salta alpha 0 → texel x tint x luce (interi) → scrivi la word. Niente
 * palette, niente tabelle: il target È il framebuffer, in 0x00RRGGBB.
 *
 * Il clip è in CLIP SPACE contro i sei piani del frustum (|x|,|y|,|z| <= w),
 * non contro un near/far passato a parte: i piani sono quelli della matrice,
 * quindi non possono divergere da lei. L'UV si interpola corretto in
 * prospettiva (u/w, v/w, 1/w) e la profondità scritta è la distanza di vista —
 * la STESSA convenzione di gm_sector_render (0 = niente, vince il più grande),
 * così i due renderer condividono il buffer senza conversioni.
 *
 * Il campionamento delle texture NON è reimplementato qui: si usa gm_tex_sample
 * (wrap + nearest/bilineare della suite), unica sede di quella logica.
 */

typedef struct gmb_3d_vclip { float x, y, z, w, u, v; } gmb_3d_vclip;

#define GMB3D_CLIP_MAX 12

static float gmb3d_plane_d(const gmb_3d_vclip* v, int plane)
{
    switch (plane)
    {
        case 0:  return v->w + v->x;   /* x >= -w */
        case 1:  return v->w - v->x;   /* x <=  w */
        case 2:  return v->w + v->y;   /* y >= -w */
        case 3:  return v->w - v->y;   /* y <=  w */
        case 4:  return v->w + v->z;   /* near: z >= -w (clip z in [-1,1]) */
        default: return v->w - v->z;   /* far:  z <=  w */
    }
}

/* Sutherland-Hodgman su un piano del frustum: interpola anche u,v (lineari in
 * clip space), così il taglio non introduce cuciture nelle coordinate. */
static int gmb3d_clip_plane(const gmb_3d_vclip* in, int n, int plane, gmb_3d_vclip* out)
{
    int on = 0;
    for (int i = 0; i < n; i++)
    {
        const gmb_3d_vclip* a = &in[i];
        const gmb_3d_vclip* b = &in[(i + 1) % n];
        float da = gmb3d_plane_d(a, plane);
        float db = gmb3d_plane_d(b, plane);
        int ina = da >= 0.0f;
        int inb = db >= 0.0f;
        if (ina) out[on++] = *a;
        if (ina != inb)
        {
            float k = da / (da - db);
            gmb_3d_vclip m;
            m.x = a->x + k * (b->x - a->x);
            m.y = a->y + k * (b->y - a->y);
            m.z = a->z + k * (b->z - a->z);
            m.w = a->w + k * (b->w - a->w);
            m.u = a->u + k * (b->u - a->u);
            m.v = a->v + k * (b->v - a->v);
            out[on++] = m;
        }
    }
    return on;
}

static float gmb3d_edge(float ax, float ay, float bx, float by, float px, float py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static unsigned gmb3d_clamp255(int v)
{
    return (v < 0) ? 0u : (v > 255 ? 255u : (unsigned)v);
}

/* Un triangolo già in screen space (y verso il basso), con 1/w e UV per vertice.
 * Winding: una faccia frontale è CCW vista da fuori, e con y verso il basso la sua
 * area calcolata da gmb3d_edge resta POSITIVA (verificato dai check dell'oracolo:
 * «pixel centrale = tint» sul quad frontale e «back-face non scrive pixel» su
 * quello invertito). Il cull lo decide il chiamante sul segno dell'area. */
static void gmb3d_tri(gmb_3d_target_t* t, const gmb_3d_material_t* mat,
                      const float s[3][2], const float iw[3], const float uv[3][2],
                      gmb_3d_draw_stats_t* st)
{
    float area = gmb3d_edge(s[0][0], s[0][1], s[1][0], s[1][1], s[2][0], s[2][1]);
    if (area == 0.0f) return;

    int minx = (int)floorf(fminf(fminf(s[0][0], s[1][0]), s[2][0]));
    int maxx = (int)ceilf (fmaxf(fmaxf(s[0][0], s[1][0]), s[2][0]));
    int miny = (int)floorf(fminf(fminf(s[0][1], s[1][1]), s[2][1]));
    int maxy = (int)ceilf (fmaxf(fmaxf(s[0][1], s[1][1]), s[2][1]));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > t->w - 1) maxx = t->w - 1;
    if (maxy > t->h - 1) maxy = t->h - 1;
    if (minx > maxx || miny > maxy) return;

    gm_tex tx;
    tx.w = mat->tw;
    tx.h = mat->th;
    tx.rgba = (unsigned char*)mat->rgba;   /* sola lettura: il campionatore non scrive */

    for (int y = miny; y <= maxy; y++)
    {
        float py = (float)y + 0.5f;
        for (int x = minx; x <= maxx; x++)
        {
            float px = (float)x + 0.5f;
            float b0 = gmb3d_edge(s[1][0], s[1][1], s[2][0], s[2][1], px, py) / area;
            float b1 = gmb3d_edge(s[2][0], s[2][1], s[0][0], s[0][1], px, py) / area;
            float b2 = gmb3d_edge(s[0][0], s[0][1], s[1][0], s[1][1], px, py) / area;
            if (b0 < 0.0f || b1 < 0.0f || b2 < 0.0f) continue;

            float iws = b0 * iw[0] + b1 * iw[1] + b2 * iw[2];
            if (iws <= 0.0f) continue;
            float d = 1.0f / iws;                       /* distanza di vista */
            size_t idx = (size_t)y * (size_t)t->w + (size_t)x;
            /* Depth = distanza (convenzione di gm_sector_render): 0 = niente
             * disegnato, altrimenti vince la distanza PIU' PICCOLA (più vicino).
             * Così un pass di mesh dopo i settori testa contro i loro valori
             * senza conversioni, e l'ordine di disegno non conta. */
            if (t->depth[idx] != 0.0f && d >= t->depth[idx]) continue;

            int cr, cg, cb;
            if (mat->rgba != NULL)
            {
                float u = (b0 * uv[0][0] * iw[0] + b1 * uv[1][0] * iw[1] + b2 * uv[2][0] * iw[2]) / iws;
                float v = (b0 * uv[0][1] * iw[0] + b1 * uv[1][1] * iw[1] + b2 * uv[2][1] * iw[2]) / iws;
                gm_real rgb[3];
                gm_real a = 1.0;
                gm_tex_sample(&tx, (gm_real)u, (gm_real)v, mat->bilinear, rgb, &a);
                if (mat->masked && a <= 0.0) continue;  /* alpha-test */
                cr = (int)((gmb3d_clamp255((int)(rgb[0] * 255.0 + 0.5)) * mat->tint_r) / 255u);
                cg = (int)((gmb3d_clamp255((int)(rgb[1] * 255.0 + 0.5)) * mat->tint_g) / 255u);
                cb = (int)((gmb3d_clamp255((int)(rgb[2] * 255.0 + 0.5)) * mat->tint_b) / 255u);
            }
            else
            {
                cr = mat->tint_r;
                cg = mat->tint_g;
                cb = mat->tint_b;
            }
            if (mat->light != 255)
            {
                cr = (cr * mat->light) / 255;
                cg = (cg * mat->light) / 255;
                cb = (cb * mat->light) / 255;
            }
            t->argb[idx] = ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb;
            t->depth[idx] = d;
            st->pixels++;
        }
    }
}

int gmb_3d_mesh_draw(gmb_3d_target_t* t, const float* mvp,
                     const gmb_3d_mesh_t* m, const gmb_3d_material_t* mat,
                     gmb_3d_draw_stats_t* stats)
{
    gmb_3d_draw_stats_t local;
    if (stats == NULL) stats = &local;
    memset(stats, 0, sizeof(*stats));

    if (t == NULL || t->argb == NULL || t->depth == NULL || t->w <= 0 || t->h <= 0)
    {
        set_err("mesh: target non allocato");
        return GMB_3D_E_ARG;
    }
    if (mvp == NULL || m == NULL || mat == NULL)
    {
        set_err("mesh: matrice, mesh o materiale mancanti");
        return GMB_3D_E_ARG;
    }
    if (m->pos == NULL || m->idx == NULL || m->nv <= 0 || m->nt <= 0)
    {
        set_err("mesh: geometria non valida");
        return GMB_3D_E_ARG;
    }
    if (mat->rgba != NULL && (mat->tw <= 0 || mat->th <= 0))
    {
        set_err("mesh: texture senza dimensioni");
        return GMB_3D_E_ARG;
    }
    if (mat->rgba != NULL && m->uv == NULL)
    {
        set_err("mesh: texture senza coordinate uv");
        return GMB_3D_E_ARG;
    }

    /* NULL mode: lo stato avanza, il disegno no (R8, T-NULL). */
    if (g_present_mode == GMB_3D_MODE_NULL)
    {
        set_err("");
        return GMB_3D_OK;
    }

    const int has_uv = (m->uv != NULL);

    for (int i = 0; i < m->nt; i++)
    {
        int ia = m->idx[i * 3 + 0];
        int ib = m->idx[i * 3 + 1];
        int ic = m->idx[i * 3 + 2];
        if (ia < 0 || ia >= m->nv || ib < 0 || ib >= m->nv || ic < 0 || ic >= m->nv)
        {
            set_err("mesh: indice fuori range");
            return GMB_3D_E_ARG;
        }

        gmb_3d_vclip poly[GMB3D_CLIP_MAX], tmp[GMB3D_CLIP_MAX];
        const int ii[3] = { ia, ib, ic };
        for (int k = 0; k < 3; k++)
        {
            const float* p = m->pos + (size_t)ii[k] * 3;
            gmb_3d_vclip* o = &poly[k];
            o->x = mvp[0] * p[0] + mvp[4] * p[1] + mvp[8]  * p[2] + mvp[12];
            o->y = mvp[1] * p[0] + mvp[5] * p[1] + mvp[9]  * p[2] + mvp[13];
            o->z = mvp[2] * p[0] + mvp[6] * p[1] + mvp[10] * p[2] + mvp[14];
            o->w = mvp[3] * p[0] + mvp[7] * p[1] + mvp[11] * p[2] + mvp[15];
            o->u = has_uv ? m->uv[(size_t)ii[k] * 2 + 0] : 0.0f;
            o->v = has_uv ? m->uv[(size_t)ii[k] * 2 + 1] : 0.0f;
        }

        int n = 3;
        for (int plane = 0; plane < 6 && n >= 3; plane++)
        {
            n = gmb3d_clip_plane(poly, n, plane, tmp);
            for (int k = 0; k < n; k++) poly[k] = tmp[k];
        }
        if (n < 3) { stats->tris_culled++; continue; }   /* fuori dal frustum */
        if (n > 3) stats->tris_clipped++;

        float sx[GMB3D_CLIP_MAX], sy[GMB3D_CLIP_MAX];
        float iw[GMB3D_CLIP_MAX], uv[GMB3D_CLIP_MAX][2];
        for (int k = 0; k < n; k++)
        {
            float invw = 1.0f / poly[k].w;
            sx[k] = (poly[k].x * invw * 0.5f + 0.5f) * (float)(t->w - 1);
            sy[k] = (0.5f - poly[k].y * invw * 0.5f) * (float)(t->h - 1);
            iw[k] = invw;
            uv[k][0] = poly[k].u;
            uv[k][1] = poly[k].v;
        }

        for (int k = 1; k + 1 < n; k++)
        {
            float s[3][2] = { { sx[0], sy[0] }, { sx[k], sy[k] }, { sx[k + 1], sy[k + 1] } };
            float tuv[3][2] = { { uv[0][0], uv[0][1] }, { uv[k][0], uv[k][1] }, { uv[k + 1][0], uv[k + 1][1] } };
            float tiw[3] = { iw[0], iw[k], iw[k + 1] };
            float a2 = gmb3d_edge(s[0][0], s[0][1], s[1][0], s[1][1], s[2][0], s[2][1]);
            if (a2 == 0.0f) continue;
            if (!mat->two_sided && a2 < 0.0f) { stats->tris_culled++; continue; }  /* back-face */
            gmb3d_tri(t, mat, s, tiw, tuv, stats);
            stats->tris_drawn++;
        }
    }

    set_err("");
    return GMB_3D_OK;
}
