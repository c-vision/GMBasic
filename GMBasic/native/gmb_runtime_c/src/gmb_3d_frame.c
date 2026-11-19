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
#include "gmb_3d.h"

#include <stdio.h>
#include <stdlib.h>

#include "gmb_c99gpm.h"
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
