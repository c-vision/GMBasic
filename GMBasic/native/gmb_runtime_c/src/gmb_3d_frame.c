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

int gmb_3d_target_ids(gmb_3d_target_t* t, int on)
{
    if (t == NULL || t->argb == NULL || t->depth == NULL || t->w <= 0 || t->h <= 0)
    {
        set_err("target: non allocato");
        return GMB_3D_E_ARG;
    }
    if (!on)
    {
        free(t->id);
        t->id = NULL;
        return GMB_3D_OK;
    }
    size_t n = (size_t)t->w * (size_t)t->h;
    uint32_t* ids = (uint32_t*)realloc(t->id, n * sizeof(uint32_t));
    if (ids == NULL)
    {
        set_err("target: memoria esaurita per il buffer di id");
        return GMB_3D_E_NOMEM;
    }
    t->id = ids;
    for (size_t i = 0; i < n; i++) ids[i] = 0;
    return GMB_3D_OK;
}

void gmb_3d_target_free(gmb_3d_target_t* t)
{
    if (t == NULL) return;
    free(t->argb);
    free(t->depth);
    free(t->id);
    t->argb = NULL;
    t->depth = NULL;
    t->id = NULL;
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
        if (t->id != NULL) t->id[i] = 0;   /* fondo: nessun oggetto (SCENE PICK) */
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

/* wx,wy,wz = posizione MONDO del vertice: si interpolano come u,v e servono alla
 * luce per pixel (lampade e fog dipendono dal punto, non dal triangolo).
 * nx,ny,nz = normale MONDO del vertice, opzionale (M-LIGHT-N): resta 0 quando la
 * mesh non ne dichiara; in quel caso non entra in nessun calcolo. */
typedef struct gmb_3d_vclip { float x, y, z, w, u, v, wx, wy, wz, nx, ny, nz; } gmb_3d_vclip;

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
            m.wx = a->wx + k * (b->wx - a->wx);
            m.wy = a->wy + k * (b->wy - a->wy);
            m.wz = a->wz + k * (b->wz - a->wz);
            m.nx = a->nx + k * (b->nx - a->nx);
            m.ny = a->ny + k * (b->ny - a->ny);
            m.nz = a->nz + k * (b->nz - a->nz);
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
                      const gmb_3d_env_t* env,
                      const float s[3][2], const float iw[3], const float uv[3][2],
                      const float wp[3][3], const float nm[3][3], int has_nm,
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
            float tex_a = 1.0f;                 /* alpha del texel (M-PALALPHA) */
            if (mat->rgba != NULL)
            {
                float u = (b0 * uv[0][0] * iw[0] + b1 * uv[1][0] * iw[1] + b2 * uv[2][0] * iw[2]) / iws;
                float v = (b0 * uv[0][1] * iw[0] + b1 * uv[1][1] * iw[1] + b2 * uv[2][1] * iw[2]) / iws;
                gm_real rgb[3];
                gm_real a = 1.0;
                gm_tex_sample(&tx, (gm_real)u, (gm_real)v, mat->bilinear, rgb, &a);
                if (mat->masked && a <= 0.0) continue;  /* alpha-test */
                tex_a = (float)a;
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
            /* Luce per pixel: base dell'oggetto + lampade, poi fog (M-LIGHT/M-FOG).
             * Senza ambiente la formula e' quella di P3, byte per byte. */
            int lv = mat->light;
            if (env != NULL)
            {
                float wx = (b0 * wp[0][0] * iw[0] + b1 * wp[1][0] * iw[1] + b2 * wp[2][0] * iw[2]) / iws;
                float wy = (b0 * wp[0][1] * iw[0] + b1 * wp[1][1] * iw[1] + b2 * wp[2][1] * iw[2]) / iws;
                float wz = (b0 * wp[0][2] * iw[0] + b1 * wp[1][2] * iw[1] + b2 * wp[2][2] * iw[2]) / iws;
                if (has_nm)   /* M-LIGHT-N: normale interpolata, poi normalizzata nel nucleo */
                {
                    float n[3];
                    n[0] = (b0 * nm[0][0] * iw[0] + b1 * nm[1][0] * iw[1] + b2 * nm[2][0] * iw[2]) / iws;
                    n[1] = (b0 * nm[0][1] * iw[0] + b1 * nm[1][1] * iw[1] + b2 * nm[2][1] * iw[2]) / iws;
                    n[2] = (b0 * nm[0][2] * iw[0] + b1 * nm[1][2] * iw[1] + b2 * nm[2][2] * iw[2]) / iws;
                    lv = (int)gmb_3d_light_eval_n(env, wx, wy, wz, n, mat->light, d);
                }
                else
                    lv = (int)gmb_3d_light_eval(env, wx, wy, wz, mat->light, d);
            }
            if (lv != 255)
            {
                cr = (cr * lv) / 255;
                cg = (cg * lv) / 255;
                cb = (cb * lv) / 255;
            }
            /* Traslucido (M-PALALPHA): alpha 255 = meta' e meta', alpha < 255 = quella
             * frazione, alpha 0 = niente. Masked e opachi non arrivano qui col blend. */
            if (mat->translucent)
            {
                float f = (mat->rgba != NULL) ? (tex_a >= 1.0f ? 0.5f : tex_a) : 0.5f;
                if (f <= 0.0f) continue;
                uint32_t old = t->argb[idx];
                int orr = (int)((old >> 16) & 0xFFu), og = (int)((old >> 8) & 0xFFu), ob = (int)(old & 0xFFu);
                cr = (int)((float)cr * f + (float)orr * (1.0f - f) + 0.5f);
                cg = (int)((float)cg * f + (float)og * (1.0f - f) + 0.5f);
                cb = (int)((float)cb * f + (float)ob * (1.0f - f) + 0.5f);
            }
            t->argb[idx] = ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb;
            t->depth[idx] = d;
            /* SCENE PICK: il pixel che vince il depth test porta l'id dell'oggetto.
             * Buffer NULL (spento) = nessuna scrittura: byte di sempre. */
            if (t->id != NULL) t->id[idx] = mat->id;
            st->pixels++;
        }
    }
}

/* --- SSAA 2x (materiale.aa) ----------------------------------------------
 * Il DISEGNO di una mesh va in un target interno doppio, poi si risolve con la
 * media 2x2: la fusione avviene UNA volta per draw, quindi i triangoli che
 * condividono un bordo non si fondono due volte (il difetto del SSAA per
 * triangolo). Il target e' riusato (nessuna allocazione per frame quando la
 * dimensione non cambia) e la mappatura schermo dei campioni e' 2*(W-1), cosi'
 * i quattro campioni cadono a 1/4 e 3/4 del pixel logico. */
static gmb_3d_target_t g_ss_target;

static void gmb3d_ss_resolve(gmb_3d_target_t* dst, const gmb_3d_target_t* src)
{
    for (int y = 0; y < dst->h; y++)
        for (int x = 0; x < dst->w; x++)
        {
            size_t di = (size_t)y * (size_t)dst->w + (size_t)x;
            unsigned r = 0, g = 0, b = 0;
            float dmin = 0.0f;
            uint32_t pick = 0;
            for (int k = 0; k < 4; k++)
            {
                size_t si = (size_t)(y * 2 + (k >> 1)) * (size_t)src->w + (size_t)(x * 2 + (k & 1));
                uint32_t p = src->argb[si];
                r += (p >> 16) & 0xFFu; g += (p >> 8) & 0xFFu; b += p & 0xFFu;
                float sd = src->depth[si];
                if (sd != 0.0f && (dmin == 0.0f || sd < dmin))
                {
                    dmin = sd;
                    if (src->id != NULL) pick = src->id[si];
                }
            }
            dst->argb[di] = (((r + 2u) / 4u) << 16) | (((g + 2u) / 4u) << 8) | ((b + 2u) / 4u);
            dst->depth[di] = dmin;
            /* L'id e' quello del campione PIU' VICINO (occlusione corretta); con
             * il buffer spento dst->id e' NULL e non si scrive nulla. */
            if (dst->id != NULL) dst->id[di] = pick;
        }
}

int gmb_3d_mesh_draw(gmb_3d_target_t* t, const float* mvp,
                     const gmb_3d_mesh_t* m, const gmb_3d_material_t* mat,
                     gmb_3d_draw_stats_t* stats)
{
    return gmb_3d_mesh_draw_env(t, mvp, m, mat, NULL, stats);
}

int gmb_3d_mesh_draw_env(gmb_3d_target_t* t, const float* mvp,
                         const gmb_3d_mesh_t* m, const gmb_3d_material_t* mat,
                         const gmb_3d_env_t* env, gmb_3d_draw_stats_t* stats)
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
    const int has_nm = (m->nrm != NULL);   /* M-LIGHT-N: assente = percorso P4 identico */

    /* SSAA 2x: si disegna nel target doppio, inizializzato col colore e con la
     * profondita' gia' presenti (il pass di settori resta rispettato). */
    gmb_3d_target_t* dt = t;
    const int ss = (mat->aa != 0);
    if (ss)
    {
        if (gmb_3d_target_alloc(&g_ss_target, t->w * 2, t->h * 2) != GMB_3D_OK)
        {
            set_err("mesh: target SSAA non allocabile");
            return GMB_3D_E_NOMEM;
        }
        /* Il pick segue il disegno: stessi campioni del colore, poi il resolve
         * prende l'id del campione piu' vicino. Con il buffer spento non si
         * alloca e non si scrive niente. */
        if (t->id != NULL && gmb_3d_target_ids(&g_ss_target, 1) != GMB_3D_OK)
        {
            set_err("mesh: buffer di id SSAA non allocabile");
            return GMB_3D_E_NOMEM;
        }
        for (int y = 0; y < g_ss_target.h; y++)
            for (int x = 0; x < g_ss_target.w; x++)
            {
                size_t si = (size_t)y * (size_t)g_ss_target.w + (size_t)x;
                size_t di = (size_t)(y >> 1) * (size_t)t->w + (size_t)(x >> 1);
                g_ss_target.argb[si] = t->argb[di];
                g_ss_target.depth[si] = t->depth[di];
                if (t->id != NULL) g_ss_target.id[si] = t->id[di];
            }
        dt = &g_ss_target;
    }

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
            o->wx = p[0]; o->wy = p[1]; o->wz = p[2];
            o->nx = has_nm ? m->nrm[(size_t)ii[k] * 3 + 0] : 0.0f;
            o->ny = has_nm ? m->nrm[(size_t)ii[k] * 3 + 1] : 0.0f;
            o->nz = has_nm ? m->nrm[(size_t)ii[k] * 3 + 2] : 0.0f;
        }

        int n = 3;
        for (int plane = 0; plane < 6 && n >= 3; plane++)
        {
            n = gmb3d_clip_plane(poly, n, plane, tmp);
            for (int k = 0; k < n; k++) poly[k] = tmp[k];
        }
        if (n < 3) { stats->tris_culled++; continue; }   /* fuori dal frustum */
        if (n > 3) stats->tris_clipped++;

        /* FAR (M-FOG): oltre la distanza dichiarata il triangolo si salta. La
         * distanza e' il w medio (distanza di vista; il clip non altera il piano). */
        if (env != NULL && env->far > 0.0f)
        {
            float wsum = 0.0f;
            for (int k = 0; k < n; k++) wsum += poly[k].w;
            if (wsum / (float)n > env->far) { stats->tris_culled++; continue; }
        }

        float sx[GMB3D_CLIP_MAX], sy[GMB3D_CLIP_MAX];
        float iw[GMB3D_CLIP_MAX], uv[GMB3D_CLIP_MAX][2], wp[GMB3D_CLIP_MAX][3];
        float nm[GMB3D_CLIP_MAX][3];
        for (int k = 0; k < n; k++)
        {
            float invw = 1.0f / poly[k].w;
            /* SSAA: 2*(W-1) mette i quattro campioni a 1/4 e 3/4 del pixel logico. */
            float sw = ss ? (float)(2 * (t->w - 1)) : (float)(dt->w - 1);
            float sh = ss ? (float)(2 * (t->h - 1)) : (float)(dt->h - 1);
            sx[k] = (poly[k].x * invw * 0.5f + 0.5f) * sw;
            sy[k] = (0.5f - poly[k].y * invw * 0.5f) * sh;
            iw[k] = invw;
            uv[k][0] = poly[k].u;
            uv[k][1] = poly[k].v;
            wp[k][0] = poly[k].wx;
            wp[k][1] = poly[k].wy;
            wp[k][2] = poly[k].wz;
            nm[k][0] = poly[k].nx;
            nm[k][1] = poly[k].ny;
            nm[k][2] = poly[k].nz;
        }

        for (int k = 1; k + 1 < n; k++)
        {
            float s[3][2] = { { sx[0], sy[0] }, { sx[k], sy[k] }, { sx[k + 1], sy[k + 1] } };
            float tuv[3][2] = { { uv[0][0], uv[0][1] }, { uv[k][0], uv[k][1] }, { uv[k + 1][0], uv[k + 1][1] } };
            float tiw[3] = { iw[0], iw[k], iw[k + 1] };
            float twp[3][3] = { { wp[0][0], wp[0][1], wp[0][2] },
                                { wp[k][0], wp[k][1], wp[k][2] },
                                { wp[k + 1][0], wp[k + 1][1], wp[k + 1][2] } };
            float tnm[3][3] = { { nm[0][0], nm[0][1], nm[0][2] },
                                { nm[k][0], nm[k][1], nm[k][2] },
                                { nm[k + 1][0], nm[k + 1][1], nm[k + 1][2] } };
            float a2 = gmb3d_edge(s[0][0], s[0][1], s[1][0], s[1][1], s[2][0], s[2][1]);
            if (a2 == 0.0f) continue;
            if (!mat->two_sided && a2 < 0.0f) { stats->tris_culled++; continue; }  /* back-face */
            gmb3d_tri(dt, mat, env, s, tiw, tuv, twp, tnm, has_nm, stats);
            stats->tris_drawn++;
        }
    }

    if (ss) gmb3d_ss_resolve(t, dt);   /* un solo resolve per DRAW: niente doppie fusioni */

    set_err("");
    return GMB_3D_OK;
}

/* ═══ P4 — luce, fog, cielo, thing (specifica M-LIGHT/M-FOG/M-SKY/M-THINGS) ══
 *
 * La luce si valuta nel PUNTO del mondo interpolato per pixel (non sul centro del
 * triangolo): una lampada vicina a una faccia grande deve schiarire solo la parte
 * vicina. `gmb_3d_light_eval` e' l'unica formula — la usano il rasterizzatore e i
 * MiniTest, cosi' un numero atteso non dipende da una seconda implementazione.
 */

void gmb_3d_env_clear(gmb_3d_env_t* env)
{
    if (env != NULL) memset(env, 0, sizeof(*env));
}

/* Nucleo UNICO di M-LIGHT/M-LIGHT-N. `n` NULL = nessun orientamento: e' il
 * percorso di P4, con le stesse operazioni di prima (niente normalizzazione,
 * niente prodotto scalare), quindi gli stessi byte. Con `n`, ogni lampada conta
 * solo su una superficie che la guarda: f *= max(0, N.L), L verso la lampada. */
static unsigned char gmb3d_light_core(const gmb_3d_env_t* env, float x, float y, float z,
                                      const float n[3], unsigned char base, float dist)
{
    int lv = (int)base;
    if (env == NULL) return (unsigned char)lv;

    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    int have_n = 0;
    if (n != NULL)
    {
        float nl2 = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
        if (nl2 > 1e-12f)
        {
            float inv = 1.0f / sqrtf(nl2);
            nx = n[0] * inv; ny = n[1] * inv; nz = n[2] * inv;
            have_n = 1;
        }
    }

    int nl = env->nlamps;
    if (nl > GMB_3D_MAX_LAMPS) nl = GMB_3D_MAX_LAMPS;
    for (int i = 0; i < nl; i++)
    {
        const gmb_3d_lamp_t* L = &env->lamps[i];
        if (!(L->radius > 0.0f) || L->amount == 0) continue;
        float dx = x - L->x, dy = y - L->y, dz = z - L->z;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= L->radius * L->radius) continue;     /* fuori dal raggio */
        float d = sqrtf(d2);
        float f = 1.0f - d / L->radius;                /* falloff lineare */
        if (f < 0.0f) f = 0.0f;
        if (have_n)
        {
            /* L (verso la lampada) = -(dx,dy,dz)/d: N.L > 0 solo se la faccia la vede. */
            float ndl = -(dx * nx + dy * ny + dz * nz) / d;
            if (ndl <= 0.0f) continue;
            f *= ndl;
        }

        float dl = sqrtf(L->dir[0] * L->dir[0] + L->dir[1] * L->dir[1] + L->dir[2] * L->dir[2]);
        if (dl > 1e-6f && L->half_angle > 0.0f)         /* cono spot */
        {
            float cosT = (dx * L->dir[0] + dy * L->dir[1] + dz * L->dir[2]) / (d > 1e-6f ? d * dl : 1e-6f);
            float ch = cosf(L->half_angle);
            float s = (1.0f - ch > 1e-6f) ? (cosT - ch) / (1.0f - ch)
                                          : (cosT >= 1.0f ? 1.0f : 0.0f);
            if (s < 0.0f) s = 0.0f;
            if (s > 1.0f) s = 1.0f;
            f *= s;
        }
        lv += (int)((float)L->amount * f + 0.5f);
    }
    if (lv > 255) lv = 255;

    if (env->fog > 0.0f)                              /* buio alla distanza fog */
    {
        float k = 1.0f - dist / env->fog;
        if (k < 0.0f) k = 0.0f;
        if (k > 1.0f) k = 1.0f;
        lv = (int)((float)lv * k + 0.5f);
    }
    if (env->colormap)                                /* 32 livelli (C-SAME-COLORMAP) */
    {
        int q = (lv * 31 + 127) / 255;
        if (q < 0) q = 0;
        if (q > 31) q = 31;
        lv = q * 255 / 31;
    }
    return (unsigned char)lv;
}

unsigned char gmb_3d_light_eval(const gmb_3d_env_t* env, float x, float y, float z,
                                unsigned char base, float dist)
{
    return gmb3d_light_core(env, x, y, z, NULL, base, dist);
}

unsigned char gmb_3d_light_eval_n(const gmb_3d_env_t* env, float x, float y, float z,
                                  const float n[3], unsigned char base, float dist)
{
    return gmb3d_light_core(env, x, y, z, n, base, dist);
}

/* Cielo per angolo: ogni pixel non coperto (depth 0) prende il texel della
 * direzione che gli corrisponde. L'angolo orizzontale si misura dallo yaw della
 * camera, quello verticale dal pitch e dalla riga; `repeat`/`shift` fanno
 * scorrere la picture attorno ai 360 (M-SKY). Non scrive la profondita': il cielo
 * e' dietro tutto e non deve mai vincere un depth test. */
int gmb_3d_sky_fill(gmb_3d_target_t* t, const unsigned char* rgba, int tw, int th,
                    float yaw_deg, float pitch_deg, float fov_deg,
                    float repeat, float shift)
{
    if (t == NULL || t->argb == NULL || t->depth == NULL || t->w <= 0 || t->h <= 0)
    {
        set_err("sky: target non allocato");
        return GMB_3D_E_ARG;
    }
    if (rgba == NULL || tw <= 0 || th <= 0)
    {
        set_err("sky: texture mancante");
        return GMB_3D_E_ARG;
    }
    if (!(fov_deg > 1.0f && fov_deg < 179.0f))
    {
        set_err("sky: fov fuori range");
        return GMB_3D_E_ARG;
    }
    if (!(repeat > 0.0f)) repeat = 1.0f;
    if (g_present_mode == GMB_3D_MODE_NULL)
    {
        set_err("");
        return GMB_3D_OK;
    }

    const float pi = 3.14159265358979f;
    float fov = fov_deg * (pi / 180.0f);
    float tf = tanf(fov * 0.5f);
    float aspect = (float)t->w / (float)t->h;
    float yaw = yaw_deg * (pi / 180.0f);
    float pitch = pitch_deg * (pi / 180.0f);

    for (int y = 0; y < t->h; y++)
    {
        float v = ((float)y + 0.5f) / (float)t->h * 2.0f - 1.0f;
        float elev = pitch - atanf(v * tf);
        float sv = 0.5f - elev / pi;
        int ty = (int)(sv * (float)th);
        if (ty < 0) ty = 0;
        if (ty >= th) ty = th - 1;
        for (int x = 0; x < t->w; x++)
        {
            size_t idx = (size_t)y * (size_t)t->w + (size_t)x;
            if (t->depth[idx] != 0.0f) continue;       /* gia' disegnato */
            float u = ((float)x + 0.5f) / (float)t->w * 2.0f - 1.0f;
            /* Con forward +z la destra dello schermo e' -x: l'angolo cresce verso
             * sinistra, quindi il segno e' NEGATO (lo blocca il check dell'oracolo:
             * la meta' sinistra della picture cade a sinistra). */
            float ang = yaw - atanf(u * tf * aspect);
            float su = (ang / (2.0f * pi)) * repeat + shift;
            su = su - floorf(su);
            int tx = (int)(su * (float)tw);
            if (tx < 0) tx = 0;
            if (tx >= tw) tx = tw - 1;
            const unsigned char* p = rgba + ((size_t)ty * (size_t)tw + (size_t)tx) * 4u;
            t->argb[idx] = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
        }
    }
    set_err("");
    return GMB_3D_OK;
}

static void gmb3d_m4_mul(float* out, const float* a, const float* b)   /* out = a*b */
{
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) sum += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = sum;
        }
    memcpy(out, r, sizeof(r));
}

/* Thing: billboard verticale con la base in (x,y,z), y = altezza (la convenzione
 * della superficie SCENE), sempre rivolto all'osservatore. Il quad si costruisce
 * nello spazio mondo e passa da gmb_3d_mesh_draw_env: clip, depth, luce e fog sono
 * gli stessi di ogni altra mesh, non una seconda pipeline (M-THINGS).
 * two_sided: l'orientamento del billboard non deve mai scartarlo. */
int gmb_3d_thing_draw(gmb_3d_target_t* t, const float* view, const float* proj,
                      const gmb_3d_thing_t* th, const float eye[3],
                      const gmb_3d_env_t* env, gmb_3d_draw_stats_t* stats)
{
    gmb_3d_draw_stats_t local;
    if (stats == NULL) stats = &local;
    memset(stats, 0, sizeof(*stats));

    if (t == NULL || t->argb == NULL || view == NULL || proj == NULL || th == NULL || eye == NULL)
    {
        set_err("thing: argomenti mancanti");
        return GMB_3D_E_ARG;
    }
    if (!(th->w > 0.0f) || !(th->h > 0.0f) || th->rgba == NULL || th->tw <= 0 || th->th <= 0)
    {
        set_err("thing: dimensioni o texture non valide");
        return GMB_3D_E_ARG;
    }

    float fx = eye[0] - th->x, fz = eye[2] - th->z;
    float fl = sqrtf(fx * fx + fz * fz);
    float rx = 1.0f, rz = 0.0f;
    if (fl > 1e-6f) { rx = fz / fl; rz = -fx / fl; }   /* right = up x forward */

    float hw = th->w * 0.5f;
    const float pos[12] = {
        th->x - rx * hw, th->y,          th->z - rz * hw,   /* base sinistra */
        th->x + rx * hw, th->y,          th->z + rz * hw,   /* base destra   */
        th->x + rx * hw, th->y + th->h,  th->z + rz * hw,   /* alto destra   */
        th->x - rx * hw, th->y + th->h,  th->z - rz * hw    /* alto sinistra */
    };
    const float uv[8] = { 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
    const int idx[6] = { 0, 1, 2, 0, 2, 3 };

    gmb_3d_mesh_t m;
    m.pos = pos; m.uv = uv; m.nv = 4; m.idx = idx; m.nt = 2; m.nrm = NULL;

    gmb_3d_material_t mat;
    memset(&mat, 0, sizeof(mat));
    mat.rgba = th->rgba;
    mat.tw = th->tw;
    mat.th = th->th;
    mat.tint_r = mat.tint_g = mat.tint_b = 255;
    mat.light = th->light;
    mat.bilinear = 1;
    mat.masked = 1;          /* alpha 0 = trasparente (C-INDEX0) */
    mat.two_sided = 1;

    float mvp[16];
    gmb3d_m4_mul(mvp, proj, view);
    return gmb_3d_mesh_draw_env(t, mvp, &m, &mat, env, stats);
}
