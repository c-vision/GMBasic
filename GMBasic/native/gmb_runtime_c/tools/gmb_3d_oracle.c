/* gmb_3d_oracle.c — P1 del piano 3D: oracolo headless.
 *
 * Pipeline verificata: raster della suite (gm_gfx_raster) -> blit nel TARGET 3D
 * del runtime -> cattura DAL TARGET -> PNG -> confronto con la golden del RID.
 * Non usa MAI gmb_gfx_present_capture(): richiede video reale e main thread e in
 * headless restituisce -1 (nota tecnica §7).
 *
 * Uso:
 *   gmb_3d_oracle --check DIR [--rid RID] [--tol PCT] [--perturb N]
 *   gmb_3d_oracle --bless DIR [--rid RID]
 *   gmb_3d_oracle --selftest                  (guardie R3/R5, null mode, alloc)
 * Il RID predefinito è quello di compilazione (GMB_ORACLE_RID): il chiamante
 * non deve ri-derivarlo con uname.
 *
 * Golden: DIR/<scena>.<rid>.png, una per RID (cc e zcc possono divergere nei
 * float: docs/3D-Specifica-Comportamenti.md §6). Ritorna 0 se nessuna scena
 * differisce (le golden mancanti sono SKIP, mai verdi), 1 su differenza, 2 su uso.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "gmb_3d.h"
#include "gmb_c99gpm.h"              /* R3/R5 con due task veri (scheduler GPM) */
#include "gmb_graphics_internal.h"   /* png_decode */
#include "gm_gfx_raster.h"
#include "gmb_3d_oracle_world.h"      /* P2: scene a settori + prove R11 */

#define W 160
#define H 120

#if defined(__APPLE__) && defined(__aarch64__)
#define GMB_ORACLE_RID "osx-arm64"
#elif defined(__APPLE__)
#define GMB_ORACLE_RID "osx-x64"
#elif defined(__linux__) && defined(__aarch64__)
#define GMB_ORACLE_RID "linux-arm64"
#elif defined(__linux__)
#define GMB_ORACLE_RID "linux-x64"
#elif defined(_WIN64)
#define GMB_ORACLE_RID "win-x64"
#else
#define GMB_ORACLE_RID "unknown"
#endif

typedef int (*scene_fn)(gmb_3d_target_t* t);

/* --- utilità ------------------------------------------------------------- */

static void fill_bg(gm_raster* r, gm_real rr, gm_real gg, gm_real bb)
{
    size_t n = (size_t)r->w * (size_t)r->h;
    for (size_t i = 0; i < n; i++)
    {
        r->color[i * 3 + 0] = rr;
        r->color[i * 3 + 1] = gg;
        r->color[i * 3 + 2] = bb;
    }
}

/* Consegna il raster al target: conversione ARGB (formato delle pagine) e blit. */
static int deliver(gm_raster* r, gmb_3d_target_t* t)
{
    size_t n = (size_t)r->w * (size_t)r->h;
    unsigned int* px = (unsigned int*)malloc(n * sizeof(unsigned int));
    if (!px) return -1;
    if (gm_raster_to_argb(r, px) != GM_OK) { free(px); return -1; }
    int rc = gmb_3d_target_blit_argb(t, (const uint32_t*)px, r->w, r->h);
    free(px);
    return rc == GMB_3D_OK ? 0 : -1;
}

/* Texture di prova, identica per tutti i RID: riempimento deterministico. */
static void make_checker(gm_tex* tex, int alpha_half)
{
    gm_tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            unsigned char r, g, b, a = 255;
            if (((x / 8) + (y / 8)) & 1) { r = 242; g = 190; b = 40; }
            else { r = 26; g = 115; b = 204; }
            if (alpha_half && (x >= 16)) a = 0;                 /* buco: alpha-test */
            if (alpha_half && (y >= 24 && y < 28)) a = 128;     /* mezzo alpha: masked lo ignora */
            gm_tex_set(tex, x, y, r, g, b, a);
        }
    }
}

/* --- scene --------------------------------------------------------------- */

/* 1) triangolo texturizzato con UV corretti in prospettiva (invw variabile). */
static int scene_tri_tex(gmb_3d_target_t* t)
{
    gm_raster r;
    gm_tex tex;
    if (gm_raster_alloc(&r, t->w, t->h) != GM_OK) return -1;
    gm_raster_clear(&r, 1.0e30);
    fill_bg(&r, 0.06, 0.09, 0.16);
    make_checker(&tex, 0);

    gm_real p0[3] = { 12.0, 108.0, 0.35 }, p1[3] = { 148.0, 108.0, 0.35 }, p2[3] = { 80.0, 10.0, 0.20 };
    gm_real u0[2] = { 0.0, 1.0 }, u1[2] = { 1.0, 1.0 }, u2[2] = { 0.5, 0.0 };
    gm_real invw[3] = { 1.0, 1.0, 2.0 };
    gm_raster_triangle_tex(&r, &tex, p0, p1, p2, u0, u1, u2, invw, 0, 0);

    int rc = deliver(&r, t);
    gm_tex_free(&tex);
    gm_raster_free(&r);
    return rc;
}

/* 2) triangolo masked: alpha 0 trasparente, alpha 128 ignorato (masked = sì/no). */
static int scene_tri_mask(gmb_3d_target_t* t)
{
    gm_raster r;
    gm_tex tex;
    if (gm_raster_alloc(&r, t->w, t->h) != GM_OK) return -1;
    gm_raster_clear(&r, 1.0e30);
    fill_bg(&r, 0.70, 0.10, 0.10);
    make_checker(&tex, 1);

    gm_real p0[3] = { 8.0, 112.0, 0.30 }, p1[3] = { 152.0, 112.0, 0.30 }, p2[3] = { 80.0, 4.0, 0.30 };
    gm_real u0[2] = { 0.0, 1.0 }, u1[2] = { 1.0, 1.0 }, u2[2] = { 0.5, 0.0 };
    gm_raster_triangle_tex(&r, &tex, p0, p1, p2, u0, u1, u2, NULL, 0, 1);

    int rc = deliver(&r, t);
    gm_tex_free(&tex);
    gm_raster_free(&r);
    return rc;
}

/* 3) due triangoli sovrapposti con z diverso: vince il vicino (depth buffer). */
static int scene_tri_depth(gmb_3d_target_t* t)
{
    gm_raster r;
    if (gm_raster_alloc(&r, t->w, t->h) != GM_OK) return -1;
    gm_raster_clear(&r, 1.0e30);
    fill_bg(&r, 0.05, 0.05, 0.05);

    gm_real far_p0[3] = { 0.0, 110.0, 0.90 }, far_p1[3] = { 159.0, 110.0, 0.90 }, far_p2[3] = { 80.0, 8.0, 0.90 };
    gm_real far_c[3] = { 0.85, 0.20, 0.20 };
    gm_raster_triangle(&r, far_p0, far_p1, far_p2, far_c, far_c, far_c);

    gm_real near_p0[3] = { 80.0, 110.0, 0.15 }, near_p1[3] = { 159.0, 110.0, 0.15 }, near_p2[3] = { 120.0, 30.0, 0.15 };
    gm_real near_c[3] = { 0.15, 0.80, 0.30 };
    gm_raster_triangle(&r, near_p0, near_p1, near_p2, near_c, near_c, near_c);

    int rc = deliver(&r, t);
    gm_raster_free(&r);
    return rc;
}

/* P1: triangoli. P2: settori (room/door/pitch) e mappa demo, dallo stesso
 * binario: la demo passa da loader, tempo e bridge del runtime. */
#define NSCENES 7
static const char* const SCENE_NAMES[NSCENES] = {
    "tri_tex", "tri_mask", "tri_depth",
    "sector_room", "sector_door", "sector_pitch", "demo"
};
static const scene_fn SCENE_FNS[NSCENES] = {
    scene_tri_tex, scene_tri_mask, scene_tri_depth,
    w_scene_room, w_scene_door, w_scene_pitch, w_scene_demo
};

/* --- golden -------------------------------------------------------------- */

/* Una golden di una scena vuota non e' un oracolo: pretende almeno 3 colori
 * distinti (sfondo + almeno due del disegno). Vale sia al bless sia al confronto. */
static int distinct_colors(const uint32_t* px, size_t n)
{
    uint32_t seen[64];
    int ns = 0;
    for (size_t i = 0; i < n && ns < 64; i++)
    {
        int found = 0;
        for (int k = 0; k < ns; k++) if (seen[k] == px[i]) { found = 1; break; }
        if (!found) seen[ns++] = px[i];
    }
    return ns;
}

static unsigned char* read_file(const char* path, size_t* len)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    unsigned char* buf = (unsigned char*)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

/* Rende la scena i e ne cattura i pixel dal target. */
static int shoot(int i, gmb_3d_target_t* t, uint32_t* shot)
{
    size_t npix = (size_t)t->w * (size_t)t->h;
    if (SCENE_FNS[i](t) != 0) { printf("FAIL %s: render\n", SCENE_NAMES[i]); return -1; }
    if (gmb_3d_target_capture_argb(t, shot, (int)npix) != (int)npix)
    {
        printf("FAIL %s: cattura dal target\n", SCENE_NAMES[i]);
        return -1;
    }
    if (distinct_colors(shot, npix) < 3)
    {
        printf("FAIL %s: fotogramma vuoto (nessun oracolo da confrontare)\n", SCENE_NAMES[i]);
        return -1;
    }
    return 0;
}

static int run_bless(const char* dir, const char* rid, gmb_3d_target_t* t)
{
    int ok = 0;
    char path[1024];
    size_t npix = (size_t)t->w * (size_t)t->h;
    uint32_t* shot = (uint32_t*)malloc(npix * sizeof(uint32_t));
    if (!shot) { printf("FAIL memoria\n"); return 2; }
    for (int i = 0; i < NSCENES; i++)
    {
        if (shoot(i, t, shot) != 0) continue;
        snprintf(path, sizeof(path), "%s/%s.%s.png", dir, SCENE_NAMES[i], rid);
        if (gmb_3d_target_capture_png(t, path) != GMB_3D_OK)
        {
            printf("FAIL %s: %s\n", SCENE_NAMES[i], gmb_3d_last_error());
            continue;
        }
        printf("BLESS %s -> %s\n", SCENE_NAMES[i], path);
        ok++;
    }
    free(shot);
    printf("P1-ORACLE blessed=%d\n", ok);
    return ok == NSCENES ? 0 : 1;
}

/* Prova dell'oracolo: altera N pixel del fotogramma catturato. Serve a dimostrare
 * che un frame con 1 pixel diverso viene RESPINTO (accettazione P1). */
static int g_perturb = 0;

static int run_compare(const char* dir, const char* rid, double tol, gmb_3d_target_t* t)
{
    int ok = 0, diff = 0, skip = 0;
    char path[1024];
    int w = t->w, h = t->h;
    size_t npix = (size_t)w * (size_t)h;
    uint32_t* shot = (uint32_t*)malloc(npix * sizeof(uint32_t));
    if (!shot) { printf("FAIL memoria\n"); return 2; }

    for (int i = 0; i < NSCENES; i++)
    {
        if (shoot(i, t, shot) != 0) { diff++; continue; }
        for (int p = 0; p < g_perturb && (size_t)p < npix; p++) shot[npix / 2 + (size_t)p] ^= 0x00FF00u;
        snprintf(path, sizeof(path), "%s/%s.%s.png", dir, SCENE_NAMES[i], rid);
        size_t len = 0;
        unsigned char* raw = read_file(path, &len);
        if (!raw)
        {
            printf("GOLDEN %s rid=%s SKIP (assente: %s)\n", SCENE_NAMES[i], rid, path);
            skip++;
            continue;
        }
        uint32_t* gp = NULL;
        int gw = 0, gh = 0;
        if (png_decode(raw, len, &gp, &gw, &gh) != 0 || !gp)
        {
            free(raw);
            printf("GOLDEN %s rid=%s SKIP (PNG illeggibile)\n", SCENE_NAMES[i], rid);
            skip++;
            continue;
        }
        free(raw);
        if (gw != w || gh != h)
        {
            printf("GOLDEN %s rid=%s DIFF (dimensioni %dx%d vs %dx%d)\n", SCENE_NAMES[i], rid, gw, gh, w, h);
            free(gp);
            diff++;
            continue;
        }
        long n = 0;
        for (size_t k = 0; k < npix; k++)
            if ((gp[k] & 0xFFFFFFu) != (shot[k] & 0xFFFFFFu)) n++;
        double pct = npix ? 100.0 * (double)n / (double)npix : 0.0;
        int good = (pct <= tol);
        printf("GOLDEN %s rid=%s total=%zu diff=%ld pct=%.3f%% tol=%.2f%% %s\n",
               SCENE_NAMES[i], rid, npix, n, pct, tol, good ? "OK" : "DIFF");
        free(gp);
        if (good) ok++;
        else diff++;
    }
    free(shot);
    printf("P1-ORACLE ok=%d diff=%d skip=%d rid=%s\n", ok, diff, skip, rid);
    return (diff == 0) ? 0 : 1;
}

/* --- R3/R5 con scheduler GPM vero ---------------------------------------- */

/* Senza riferimenti deboli lo scheduler è SEMPRE nel binario: qui le guardie si
 * provano con due task veri, non con due finzioni sullo stesso thread. */
static volatile int g_t_started;      /* il proprietario ha preso il claim */
static volatile int g_t_hold_ms;      /* >0: tiene e poi chiude; <0: sparisce */
static int           g_t_begin_rc = 99;
static gmb_task_t*   g_t_owner;

static void sched_owner(void* arg)
{
    (void)arg;
    g_t_owner = gmb_cur();
    g_t_begin_rc = gmb_3d_render_begin();
    g_t_started = 1;
    if (g_t_hold_ms > 0) gmb_yield_ticks(g_t_hold_ms);
    if (g_t_hold_ms >= 0) gmb_3d_render_end();
    /* hold_ms<0: ritorna SENZA RENDER END (task finito con claim aperto). */
}

static void sched_wait_started(void)
{
    for (int i = 0; i < 5000 && !g_t_started; i++) gmb_yield_ticks(1);
}

static void sched_wait_depth0(void)
{
    for (int i = 0; i < 5000 && gmb_3d_render_depth() != 0; i++) gmb_yield_ticks(1);
}

static int g_sched_ok, g_sched_fail;

#define SCHED_CHECK(cond, what) do { \
    if (cond) { g_sched_ok++; } else { g_sched_fail++; printf("FAIL %s (sched)\n", (what)); } } while (0)

static void sched_main(void* arg)
{
    (void)arg;

    /* (1) proprietario VIVO: il non-proprietario non lo scalza e non lo chiude. */
    g_t_started = 0; g_t_hold_ms = 300; g_t_begin_rc = 99; g_t_owner = NULL;
    gmb_task_spawn(sched_owner, NULL);
    sched_wait_started();
    SCHED_CHECK(g_t_begin_rc == GMB_3D_OK, "il task proprietario apre il renderer");
    SCHED_CHECK(gmb_3d_render_begin() == GMB_3D_E_BUSY, "secondo task: E_BUSY senza attesa");
    gmb_3d_render_end();   /* chiusura da NON proprietario */
    SCHED_CHECK(gmb_3d_render_depth() == 1, "chiusura da non-proprietario: claim vivo intatto");
    sched_wait_depth0();
    SCHED_CHECK(gmb_3d_render_depth() == 0, "il proprietario chiude il proprio claim");
    SCHED_CHECK(gmb_3d_render_begin() == GMB_3D_OK, "renderer riusabile dopo il proprietario");
    gmb_3d_render_end();

    /* (2) proprietario SPARITO (task finito senza RENDER END): la chiusura da
     *     non-proprietario deve LIBERARE lo stato, non lasciarlo occupato. */
    g_t_started = 0; g_t_hold_ms = -1; g_t_begin_rc = 99; g_t_owner = NULL;
    gmb_task_spawn(sched_owner, NULL);
    sched_wait_started();
    for (int i = 0; i < 5000 && g_t_owner && !gmb_task_finished(g_t_owner); i++) gmb_yield_ticks(1);
    SCHED_CHECK(g_t_begin_rc == GMB_3D_OK, "claim orfano: il proprietario lo aveva aperto");
    SCHED_CHECK(g_t_owner != NULL && gmb_task_finished(g_t_owner), "proprietario finito senza RENDER END");
    SCHED_CHECK(gmb_3d_render_depth() == 1, "claim orfano ancora aperto");
    gmb_3d_render_end();   /* chiusura da NON proprietario di un claim orfano */
    SCHED_CHECK(gmb_3d_render_depth() == 0, "chiusura da non-proprietario libera il claim orfano");
    SCHED_CHECK(gmb_3d_render_begin() == GMB_3D_OK, "il renderer NON resta occupato per sempre");
    gmb_3d_render_end();

    /* (3) R11: il LOAD di un mondo dentro un task legge su thread esterno,
     *     parca e resta atomico sul fallimento. */
    w_world_checks(&g_sched_ok, &g_sched_fail);
}

static void run_sched_guard_test(int* ok, int* fail)
{
    gmb_gpm_start(sched_main);
    /* Shutdown: nessun claim residuo e API di nuovo usabile senza scheduler (T3). */
    if (gmb_3d_render_depth() == 0) g_sched_ok++;
    else { g_sched_fail++; printf("FAIL claim residuo dopo lo shutdown GPM (sched)\n"); }
    SCHED_CHECK(gmb_3d_render_begin() == GMB_3D_OK, "T3 dopo lo shutdown: RENDER riusabile");
    gmb_3d_render_end();
    *ok += g_sched_ok;
    *fail += g_sched_fail;
}

#undef SCHED_CHECK

/* --- selftest: guardie e null mode (P1 accettazione) ---------------------- */

static int run_selftest(const char* dir)
{
    int ok = 0, fail = 0;
    w_prepare(dir);   /* documenti di prova del load, scritti fuori dallo scheduler */
#define CHECK(cond, what) do { if (cond) { ok++; } else { fail++; printf("FAIL %s (riga %d)\n", (what), __LINE__); } } while (0)

    gmb_3d_target_t t;
    memset(&t, 0, sizeof(t));
    CHECK(gmb_3d_target_alloc(&t, W, H) == GMB_3D_OK, "alloc target");
    unsigned long a0 = gmb_3d_alloc_count();

    /* R5: ricorsione riconosciuta, nessuna attesa */
    CHECK(gmb_3d_render_begin() == GMB_3D_OK, "render begin");
    CHECK(gmb_3d_render_depth() == 1, "render depth 1");
    CHECK(gmb_3d_render_begin() == GMB_3D_E_RECURSIVE, "ricorsione rifiutata");
    CHECK(strstr(gmb_3d_last_error(), "ricorsiva") != NULL, "errore ricorsivo esplicito");
    gmb_3d_render_end();
    CHECK(gmb_3d_render_depth() == 0, "render depth 0 dopo end");
    CHECK(gmb_3d_render_begin() == GMB_3D_OK, "render riapribile");
    gmb_3d_render_end();
    gmb_3d_render_end();   /* chiusura senza apertura: no-op, nessun crash */
    CHECK(gmb_3d_render_depth() == 0, "end ridondante innocuo");

    /* NULL mode: non disegna, non alloca (T-NULL / R4) */
    gmb_3d_present_mode_set(GMB_3D_MODE_NULL);
    CHECK(gmb_3d_present_mode() == GMB_3D_MODE_NULL, "modo null impostato");
    CHECK(gmb_3d_target_clear(&t, 0x102030, 1.0f) == GMB_3D_OK, "clear");
    uint32_t* white = (uint32_t*)malloc((size_t)W * H * sizeof(uint32_t));
    CHECK(white != NULL, "buffer di prova");
    if (white)
    {
        for (size_t i = 0; i < (size_t)W * H; i++) white[i] = 0xFFFFFF;
        { int all_ok = 1;
          for (int f = 0; f < 100; f++) if (gmb_3d_target_blit_argb(&t, white, W, H) != GMB_3D_OK) all_ok = 0;
          CHECK(all_ok, "blit null mode su 100 frame"); }
        CHECK(t.argb[0] == 0x102030u, "null mode non scrive il target");
        CHECK(gmb_3d_alloc_count() == a0, "null mode non alloca (100 frame)");

        /* modo target: il blit scrive e la cattura riproduce esattamente il target */
        gmb_3d_present_mode_set(GMB_3D_MODE_TARGET);
        CHECK(gmb_3d_target_blit_argb(&t, white, W, H) == GMB_3D_OK, "blit target mode");
        CHECK(t.argb[0] == 0xFFFFFFu, "target mode scrive");
        uint32_t* cap = (uint32_t*)malloc((size_t)W * H * sizeof(uint32_t));
        CHECK(cap != NULL, "buffer cattura");
        if (cap)
        {
            int got = gmb_3d_target_capture_argb(&t, cap, (int)((size_t)W * H));
            CHECK(got == W * H, "cattura tutti i pixel");
            CHECK(memcmp(cap, t.argb, (size_t)W * H * sizeof(uint32_t)) == 0, "cattura identica al target");
            free(cap);
        }
        CHECK(gmb_3d_alloc_count() == a0, "nessuna allocazione per frame");

        /* errori espliciti, mai silenziosi (prima di liberare il buffer) */
        CHECK(gmb_3d_target_blit_argb(&t, white, W + 1, H) == GMB_3D_E_ARG, "blit di dimensioni diverse");
        CHECK(strstr(gmb_3d_last_error(), "dimensioni") != NULL, "errore dimensioni esplicito");
        free(white);
    }
    CHECK(gmb_3d_target_alloc(NULL, W, H) == GMB_3D_E_ARG, "alloc con puntatore NULL");
    CHECK(gmb_3d_target_capture_argb(&t, NULL, 0) == GMB_3D_E_ARG, "cattura senza buffer");

    gmb_3d_target_free(&t);
    CHECK(t.argb == NULL && t.depth == NULL, "free azzera i buffer");

    /* R3/R5 con due task veri: la chiusura da non-proprietario non blocca lo
     * stato né corrompe il claim di un proprietario vivo. */
    run_sched_guard_test(&ok, &fail);

    /* R11 fuori da un task: stessa strada del tool, nessun park. */
    w_world_t3_checks(&ok, &fail);

    /* W-MOVE / W-PLAYER / W-EYE-CLAMP: 10 casi di collisione, sul loader vero. */
    w_collision_checks(&ok, &fail);

    /* T-* e mappa demo: orologio a passo fisso, mover, animazioni, scroll,
     * luce, lampade, traslucenza. */
    w_time_checks(&ok, &fail);

    printf("P1-SELFTEST ok=%d fail=%d\n", ok, fail);
#undef CHECK
    return fail == 0 ? 0 : 1;
}

/* --- main ---------------------------------------------------------------- */

static void usage(void)
{
    printf("uso: gmb_3d_oracle --check DIR [--rid RID] [--tol PCT] [--perturb N]\n"
           "     gmb_3d_oracle --bless DIR [--rid RID]\n"
           "     gmb_3d_oracle --selftest DIR\n"
           "     gmb_3d_oracle --bench [--frames N]\n"
           "  DIR   = cartella delle golden (<scena>.<rid>.png)\n"
           "  --demo MAPPA = mappa demo (.gmbscene): scena `demo`, T-* e bench\n"
           "  --bench = frame time della demo a 640x300 (limite 4 ms)\n");
}

int main(int argc, char** argv)
{
    const char* mode = NULL;
    const char* dir = NULL;
    const char* rid = GMB_ORACLE_RID;
    const char* demo = NULL;
    double tol = 0.0;
    int bench = 0, frames = 120;

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--check") || !strcmp(argv[i], "--bless"))
        {
            mode = argv[i];
            if (i + 1 < argc) dir = argv[++i];
        }
        else if (!strcmp(argv[i], "--selftest"))
        {
            mode = argv[i];
            if (i + 1 < argc) dir = argv[++i];   /* cartella dei documenti di prova */
        }
        else if (!strcmp(argv[i], "--rid") && i + 1 < argc) rid = argv[++i];
        else if (!strcmp(argv[i], "--tol") && i + 1 < argc) tol = atof(argv[++i]);
        else if (!strcmp(argv[i], "--perturb") && i + 1 < argc) g_perturb = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--demo") && i + 1 < argc) demo = argv[++i];
        else if (!strcmp(argv[i], "--bench")) bench = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else { printf("argomento non riconosciuto: %s\n", argv[i]); usage(); return 2; }
    }
    if (demo) w_set_demo_path(demo);
    if (bench) return w_bench_demo(640, 300, frames, 4.0);
    if (!mode) { usage(); return 2; }
    if (!dir) { usage(); return 2; }

    if (!strcmp(mode, "--selftest")) return run_selftest(dir);

    gmb_3d_target_t t;
    memset(&t, 0, sizeof(t));
    if (gmb_3d_target_alloc(&t, W, H) != GMB_3D_OK)
    {
        printf("FAIL target: %s\n", gmb_3d_last_error());
        return 2;
    }
    int rc;
    if (!strcmp(mode, "--bless")) rc = run_bless(dir, rid, &t);
    else                          rc = run_compare(dir, rid, tol, &t);
    gmb_3d_target_free(&t);
    return rc;
}
