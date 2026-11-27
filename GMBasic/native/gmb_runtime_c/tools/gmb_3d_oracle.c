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
#include <time.h>          /* P5: banco della scena (tempo mediano per frame) */
#include <math.h>          /* NAN: letture con argomento non finito */

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

/* --- P3: mesh sul target (rasterizzatore ARGB+depth del runtime) ----------
 *
 * Le matrici si costruiscono QUI, non con gm_mat4_*: il kernel si prova contro
 * un produttore indipendente (e gm_gfx_geom non è nel link dell'oracolo).
 * Column-major, clip z in [-1,1] come la convenzione della suite.
 */

#define MESH_PI 3.14159265f

static void m4_mul(float* out, const float* a, const float* b)   /* out = a*b */
{
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
        {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = s;
        }
    memcpy(out, r, sizeof(r));
}

static void m4_perspective(float* m, float fovy, float aspect, float zn, float zf)
{
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fovy * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = (2.0f * zf * zn) / (zn - zf);
}

static void m4_look_at(float* m, const float eye[3], const float at[3], const float up[3])
{
    float f[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    f[0] /= fl; f[1] /= fl; f[2] /= fl;
    float r[3] = { f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0] };
    float rl = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    r[0] /= rl; r[1] /= rl; r[2] /= rl;
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    memset(m, 0, 16 * sizeof(float));
    m[0] = r[0]; m[4] = r[1]; m[8]  = r[2]; m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    m[1] = u[0]; m[5] = u[1]; m[9]  = u[2]; m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
    m[15] = 1.0f;
}

static void mvp_build(float* mvp, const float eye[3], const float at[3],
                      float fovy, float aspect, float zn, float zf)
{
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    float v[16], p[16];
    m4_look_at(v, eye, at, up);
    m4_perspective(p, fovy, aspect, zn, zf);
    m4_mul(mvp, p, v);
}

/* Cubo [-1,1]^3 a 24 vertici (4 per faccia, UV per faccia): 6 facce x 2
 * triangoli. Winding CCW visto da FUORI (il cull del kernel scarta il resto). */
static const float CUBE_POS[24 * 3] = {
    /* +X */  1,-1,-1,  1, 1,-1,  1, 1, 1,  1,-1, 1,
    /* -X */ -1,-1, 1, -1, 1, 1, -1, 1,-1, -1,-1,-1,
    /* +Y */ -1, 1,-1, -1, 1, 1,  1, 1, 1,  1, 1,-1,
    /* -Y */ -1,-1, 1, -1,-1,-1,  1,-1,-1,  1,-1, 1,
    /* +Z */ -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1,
    /* -Z */  1,-1,-1, -1,-1,-1, -1, 1,-1,  1, 1,-1
};
static const float CUBE_UV[24 * 2] = {
    0,1, 1,1, 1,0, 0,0,
    0,1, 1,1, 1,0, 0,0,
    0,1, 1,1, 1,0, 0,0,
    0,1, 1,1, 1,0, 0,0,
    0,1, 1,1, 1,0, 0,0,
    0,1, 1,1, 1,0, 0,0
};
static const int CUBE_IDX[12 * 3] = {
     0, 1, 2,  0, 2, 3,
     4, 5, 6,  4, 6, 7,
     8, 9,10,  8,10,11,
    12,13,14, 12,14,15,
    16,17,18, 16,18,19,
    20,21,22, 20,22,23
};

/* Piano y = -1 su [-S,S]^2, griglia NxN, UV 0..1. Winding con la normale +Y. */
#define PL_N 4
#define PL_S 6.0f
#define PL_V ((PL_N + 1) * (PL_N + 1))
static float pl_pos[PL_V * 3];
static float pl_uv[PL_V * 2];
static int   pl_idx[PL_N * PL_N * 6];

static void build_plane(void)
{
    int i = 0;
    for (int z = 0; z <= PL_N; z++)
        for (int x = 0; x <= PL_N; x++, i++)
        {
            pl_pos[i * 3 + 0] = -PL_S + 2.0f * PL_S * (float)x / (float)PL_N;
            pl_pos[i * 3 + 1] = -1.0f;
            pl_pos[i * 3 + 2] = -PL_S + 2.0f * PL_S * (float)z / (float)PL_N;
            pl_uv[i * 2 + 0] = (float)x / (float)PL_N;
            pl_uv[i * 2 + 1] = (float)z / (float)PL_N;
        }
    int k = 0;
    for (int z = 0; z < PL_N; z++)
        for (int x = 0; x < PL_N; x++)
        {
            int a = z * (PL_N + 1) + x, b = a + 1, c = a + PL_N + 1, d = c + 1;
            pl_idx[k++] = a; pl_idx[k++] = c; pl_idx[k++] = b;   /* normale +Y */
            pl_idx[k++] = b; pl_idx[k++] = c; pl_idx[k++] = d;
        }
}

static void mesh_material(gmb_3d_material_t* mat, const gm_tex* tex, int bilinear)
{
    memset(mat, 0, sizeof(*mat));
    if (tex != NULL)
    {
        mat->rgba = tex->rgba;
        mat->tw = tex->w;
        mat->th = tex->h;
        mat->bilinear = bilinear;
    }
    mat->tint_r = mat->tint_g = mat->tint_b = 255;
    mat->light = 255;
}

/* 4) cubo texturizzato in prospettiva: 3 facce visibili, 3 in back-face. */
static int scene_mesh_cube(gmb_3d_target_t* t)
{
    gm_tex tex;
    make_checker(&tex, 0);
    gmb_3d_target_clear(t, 0x0F1729u, 0.0f);

    const float eye[3] = { 3.2f, 2.6f, 4.0f }, at[3] = { 0.0f, 0.0f, 0.0f };
    float mvp[16];
    mvp_build(mvp, eye, at, 45.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);

    gmb_3d_material_t mat;
    mesh_material(&mat, &tex, 0);
    gmb_3d_mesh_t m;
    m.pos = CUBE_POS; m.uv = CUBE_UV; m.nv = 24; m.idx = CUBE_IDX; m.nt = 12; m.nrm = NULL;
    gmb_3d_draw_stats_t st;
    int rc = gmb_3d_mesh_draw(t, mvp, &m, &mat, &st);
    gm_tex_free(&tex);
    if (rc != GMB_3D_OK) return -1;
    return (st.tris_drawn == 6 && st.tris_culled == 6) ? 0 : -1;
}

/* 5) piano texturizzato + cubo bilineare sopra: due draw, due filtri. */
static int scene_mesh_plane(gmb_3d_target_t* t)
{
    gm_tex tex;
    make_checker(&tex, 0);
    build_plane();
    gmb_3d_target_clear(t, 0x0A0E1Au, 0.0f);

    const float eye[3] = { 5.0f, 3.0f, 7.0f }, at[3] = { 0.0f, -0.6f, 0.0f };
    float mvp[16];
    mvp_build(mvp, eye, at, 50.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);

    gmb_3d_material_t mn, mb;
    mesh_material(&mn, &tex, 0);
    mesh_material(&mb, &tex, 1);
    gmb_3d_mesh_t plane = { pl_pos, pl_uv, PL_V, pl_idx, PL_N * PL_N * 2 };
    gmb_3d_mesh_t cube = { CUBE_POS, CUBE_UV, 24, CUBE_IDX, 12 };
    gmb_3d_draw_stats_t s1, s2;
    int rc = gmb_3d_mesh_draw(t, mvp, &plane, &mn, &s1);
    if (rc == GMB_3D_OK) rc = gmb_3d_mesh_draw(t, mvp, &cube, &mb, &s2);
    gm_tex_free(&tex);
    return (rc == GMB_3D_OK && s1.tris_drawn > 0 && s2.tris_drawn > 0) ? 0 : -1;
}

/* 6) un quad che ATTRAVERSA il near plane: il clip deve tagliarlo, non
 * spifferarlo sullo schermo (vertici a z = -0.05 e z = -2 con znear 0.1). */
static int scene_mesh_clip(gmb_3d_target_t* t)
{
    gmb_3d_target_clear(t, 0x120C1Cu, 0.0f);
    float mvp[16];
    m4_perspective(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);

    const float pos[] = {
        -2.5f, -2.5f, -0.05f,   2.5f, -2.5f, -2.0f,
         2.5f,  2.5f, -2.0f,   -2.5f,  2.5f, -0.05f
    };
    const float uv[] = { 0,1, 1,1, 1,0, 0,0 };
    const int idx[] = { 0,1,2, 0,2,3 };

    gm_tex tex;
    make_checker(&tex, 0);
    gmb_3d_material_t mat;
    mesh_material(&mat, &tex, 0);
    mat.two_sided = 1;
    gmb_3d_mesh_t m = { pos, uv, 4, idx, 2 };
    gmb_3d_draw_stats_t st;
    int rc = gmb_3d_mesh_draw(t, mvp, &m, &mat, &st);
    gm_tex_free(&tex);
    if (rc != GMB_3D_OK) return -1;
    return (st.tris_clipped >= 1 && st.pixels > 0) ? 0 : -1;
}

/* 7) z-buffer: il lontano disegnato DOPO il vicino non lo deve coprire. */
static int scene_mesh_depth(gmb_3d_target_t* t)
{
    gmb_3d_target_clear(t, 0x000000u, 0.0f);
    float mvp[16];
    m4_perspective(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);

    /* Il lontano deve RESTARE visibile attorno al vicino: a z=-4 la mezza altezza
     * visibile è 2.31 (a z=-2 è 1.15), quindi il suo mezzo lato è 6 e non 2. */
    const float near_pos[] = { -1.2f,-1.2f,-2.0f,  1.2f,-1.2f,-2.0f,  1.2f,1.2f,-2.0f, -1.2f,1.2f,-2.0f };
    const float far_pos[]  = { -6.0f,-6.0f,-4.0f,  6.0f,-6.0f,-4.0f,  6.0f,6.0f,-4.0f, -6.0f,6.0f,-4.0f };
    const int idx[] = { 0,1,2, 0,2,3 };

    gmb_3d_material_t mn, mf;
    memset(&mn, 0, sizeof(mn)); mn.tint_r = 214; mn.tint_g = 40;  mn.tint_b = 40;  mn.light = 255;
    memset(&mf, 0, sizeof(mf)); mf.tint_r = 40;  mf.tint_g = 200; mf.tint_b = 70;  mf.light = 255;
    gmb_3d_mesh_t fq = { far_pos, NULL, 4, idx, 2 };
    gmb_3d_mesh_t nq = { near_pos, NULL, 4, idx, 2 };
    gmb_3d_draw_stats_t s1, s2;
    int rc = gmb_3d_mesh_draw(t, mvp, &nq, &mn, &s1);      /* vicino prima */
    if (rc == GMB_3D_OK) rc = gmb_3d_mesh_draw(t, mvp, &fq, &mf, &s2);   /* poi il lontano */
    if (rc != GMB_3D_OK) return -1;
    uint32_t c = t->argb[(size_t)(t->h / 2) * (size_t)t->w + (size_t)(t->w / 2)];
    return (c == (((uint32_t)214 << 16) | ((uint32_t)40 << 8) | 40u)) ? 0 : -1;
}

/* 8) back-face: il quad rivolto all'indietro (rosso) è scartato e lascia
 * vedere quello frontale (blu) e quello giallo accanto. */
static int scene_mesh_cull(gmb_3d_target_t* t)
{
    gmb_3d_target_clear(t, 0x0C0C0Cu, 0.0f);
    float mvp[16];
    m4_perspective(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);

    const float blu[]  = { -2.2f,-1.4f,-2.0f, -0.2f,-1.4f,-2.0f, -0.2f,1.4f,-2.0f, -2.2f,1.4f,-2.0f };
    const float giallo[] = { 0.2f,-1.4f,-2.0f,  2.2f,-1.4f,-2.0f,  2.2f,1.4f,-2.0f,  0.2f,1.4f,-2.0f };
    /* winding INVERTITO: back-face, non deve apparire. Vertici dentro il frustum
     * (a z=-1.4 la mezza altezza visibile è 0.81), così è 1:1 due triangoli e
     * non un fan da clipping. */
    const float rosso[] = { -0.5f,-0.5f,-1.4f, -0.5f,0.5f,-1.4f,  0.5f,0.5f,-1.4f,  0.5f,-0.5f,-1.4f };
    const int idx[] = { 0,1,2, 0,2,3 };

    gmb_3d_material_t mb, mg, mr;
    memset(&mb, 0, sizeof(mb)); mb.tint_r = 60;  mb.tint_g = 90;  mb.tint_b = 220; mb.light = 255;
    memset(&mg, 0, sizeof(mg)); mg.tint_r = 220; mg.tint_g = 200; mg.tint_b = 60;  mg.light = 255;
    memset(&mr, 0, sizeof(mr)); mr.tint_r = 220; mr.tint_g = 30;  mr.tint_b = 30;  mr.light = 255;
    gmb_3d_mesh_t qb = { blu, NULL, 4, idx, 2 };
    gmb_3d_mesh_t qg = { giallo, NULL, 4, idx, 2 };
    gmb_3d_mesh_t qr = { rosso, NULL, 4, idx, 2 };
    gmb_3d_draw_stats_t sb, sg, sr;
    int rc = gmb_3d_mesh_draw(t, mvp, &qb, &mb, &sb);
    if (rc == GMB_3D_OK) rc = gmb_3d_mesh_draw(t, mvp, &qg, &mg, &sg);
    if (rc == GMB_3D_OK) rc = gmb_3d_mesh_draw(t, mvp, &qr, &mr, &sr);
    if (rc != GMB_3D_OK) return -1;
    return (sr.tris_culled == 2 && sr.tris_drawn == 0) ? 0 : -1;
}

/* --- P4: luce, fog, cielo, thing e ordine (specifica M-*) -----------------
 *
 * Le scene usano il kernel direttamente (gmb_3d_mesh_draw_env, sky_fill,
 * thing_draw): sono le stesse funzioni che la superficie SCENE chiama, ma qui
 * la geometria è nota e il fotogramma è l'oracolo.
 */

typedef struct p4_quad {
    float pos[12];
    float uv[8];
    int idx[6];
} p4_quad_t;

/* Quad verticale rivolto alla camera (fronte = CCW visto da -z). */
static void p4_quad_init(p4_quad_t* q, float x0, float y0, float x1, float y1, float z)
{
    static const float uv0[8] = { 0,1, 1,1, 1,0, 0,0 };
    /* Con la camera di p4_camera (forward +z) la destra dello schermo e' -x:
     * l'ordine BL,BR,TR visto dal mondo resta frontale solo se i triangoli sono
     * 0,2,1 e 0,3,2. Senza questo il kernel scarta il quad come back-face. */
    static const int idx0[6] = { 0, 2, 1, 0, 3, 2 };
    q->pos[0] = x0; q->pos[1] = y0; q->pos[2] = z;
    q->pos[3] = x1; q->pos[4] = y0; q->pos[5] = z;
    q->pos[6] = x1; q->pos[7] = y1; q->pos[8] = z;
    q->pos[9] = x0; q->pos[10] = y1; q->pos[11] = z;
    memcpy(q->uv, uv0, sizeof(uv0));
    memcpy(q->idx, idx0, sizeof(idx0));
}

static gmb_3d_mesh_t p4_mesh(const p4_quad_t* q)
{
    gmb_3d_mesh_t m;
    m.pos = q->pos; m.uv = q->uv; m.nv = 4; m.idx = q->idx; m.nt = 2;
    m.nrm = NULL;
    return m;
}

/* Texture tinta unita (per thing e traslucidi). */
static void make_solid(gm_tex* tex, unsigned char r, unsigned char g, unsigned char b)
{
    gm_tex_alloc(tex, 8, 8);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) gm_tex_set(tex, x, y, r, g, b, 255);
}

static void p4_flat_material(gmb_3d_material_t* mat, unsigned char r, unsigned char g,
                             unsigned char b, unsigned char light)
{
    memset(mat, 0, sizeof(*mat));
    mat->tint_r = r; mat->tint_g = g; mat->tint_b = b;
    mat->light = light;
}

static void p4_camera(float mvp[16], float fovy, float aspect)
{
    const float eye[3] = { 0.0f, 0.0f, 0.0f };
    const float at[3]  = { 0.0f, 0.0f, 1.0f };
    mvp_build(mvp, eye, at, fovy, aspect, 0.1f, 100.0f);
}

/* 9) lampada puntiforme: centro chiaro, bordi scuri (falloff lineare). */
static int p4_scene_lamp_point(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x000000u, 0.0f) != GMB_3D_OK) return -1;
    p4_quad_t q;
    p4_quad_init(&q, -3.0f, -3.0f, 3.0f, 3.0f, 4.0f);
    gmb_3d_mesh_t m = p4_mesh(&q);
    gmb_3d_material_t mat;
    p4_flat_material(&mat, 210, 210, 210, 20);
    gmb_3d_env_t env;
    gmb_3d_env_clear(&env);
    env.nlamps = 1;
    env.lamps[0].x = 0.0f; env.lamps[0].y = 0.0f; env.lamps[0].z = 2.5f;
    env.lamps[0].radius = 3.0f;
    env.lamps[0].amount = 235;
    float mvp[16];
    p4_camera(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h);
    gmb_3d_draw_stats_t st;
    return gmb_3d_mesh_draw_env(t, mvp, &m, &mat, &env, &st) == GMB_3D_OK ? 0 : -1;
}

/* 10) spot: il cono illumina solo il quad di sinistra, quello di destra resta buio. */
static int p4_scene_lamp_spot(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x000000u, 0.0f) != GMB_3D_OK) return -1;
    p4_quad_t ql, qr;
    p4_quad_init(&ql, -3.2f, -1.5f, -0.4f, 1.5f, 4.0f);
    p4_quad_init(&qr,  0.4f, -1.5f,  3.2f, 1.5f, 4.0f);
    gmb_3d_mesh_t ml = p4_mesh(&ql), mr = p4_mesh(&qr);
    gmb_3d_material_t mat;
    p4_flat_material(&mat, 210, 210, 210, 15);
    gmb_3d_env_t env;
    gmb_3d_env_clear(&env);
    env.nlamps = 1;
    env.lamps[0].x = -1.8f; env.lamps[0].y = 0.0f; env.lamps[0].z = 2.0f;
    env.lamps[0].radius = 6.0f;
    env.lamps[0].dir[0] = 0.0f; env.lamps[0].dir[1] = 0.0f; env.lamps[0].dir[2] = 1.0f;
    env.lamps[0].half_angle = 25.0f * MESH_PI / 180.0f;
    env.lamps[0].amount = 235;
    float mvp[16];
    p4_camera(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h);
    gmb_3d_draw_stats_t s1, s2;
    if (gmb_3d_mesh_draw_env(t, mvp, &ml, &mat, &env, &s1) != GMB_3D_OK) return -1;
    if (gmb_3d_mesh_draw_env(t, mvp, &mr, &mat, &env, &s2) != GMB_3D_OK) return -1;
    return 0;
}

/* 11) fog: due quad a distanze note, il lontano si scurisce (M-FOG). */
static int p4_scene_fog(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x000000u, 0.0f) != GMB_3D_OK) return -1;
    p4_quad_t qn, qf;
    p4_quad_init(&qn, -3.0f, -2.0f, -1.0f, 2.0f, 3.0f);
    p4_quad_init(&qf,  1.0f, -2.0f,  3.0f, 2.0f, 8.0f);
    gmb_3d_mesh_t mn = p4_mesh(&qn), mf = p4_mesh(&qf);
    gmb_3d_material_t mat;
    p4_flat_material(&mat, 220, 220, 220, 255);
    gmb_3d_env_t env;
    gmb_3d_env_clear(&env);
    env.fog = 9.0f;
    float mvp[16];
    p4_camera(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h);
    gmb_3d_draw_stats_t s1, s2;
    if (gmb_3d_mesh_draw_env(t, mvp, &mn, &mat, &env, &s1) != GMB_3D_OK) return -1;
    if (gmb_3d_mesh_draw_env(t, mvp, &mf, &mat, &env, &s2) != GMB_3D_OK) return -1;
    return 0;
}

/* 12) cielo per angolo: due metà della picture finiscono a sinistra e a destra. */
static int p4_scene_sky(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x000000u, 0.0f) != GMB_3D_OK) return -1;
    gm_tex tex;
    gm_tex_alloc(&tex, 32, 32);
    /* Quattro bande: un fotogramma a due soli colori non è un oracolo (il gate
     * vuole almeno 3 colori distinti), e l'avvolgimento si vede meglio. */
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
        {
            unsigned char r = 200, g = 40, b = 40;
            if (x >= 8 && x < 16)  { r = 220; g = 140; b = 40; }
            else if (x >= 16 && x < 24) { r = 60; g = 180; b = 80; }
            else if (x >= 24)      { r = 30; g = 90; b = 200; }
            /* Metà bassa più scura: la vista copre ~75° (2 bande orizzontali),
             * la variazione verticale porta i colori distinti sopra la soglia e
             * verifica anche la mappatura dell'elevazione. */
            if (y >= 16) { r = (unsigned char)(r / 2); g = (unsigned char)(g / 2); b = (unsigned char)(b / 2); }
            gm_tex_set(&tex, x, y, r, g, b, 255);
        }
    int rc = gmb_3d_sky_fill(t, tex.rgba, tex.w, tex.h, 0.0f, 0.0f, 60.0f, 1.0f, 0.0f);
    gm_tex_free(&tex);
    return rc == GMB_3D_OK ? 0 : -1;
}

/* 13) thing: il vicino copre il lontano (depth test, M-THINGS). */
static int p4_scene_thing(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x000000u, 0.0f) != GMB_3D_OK) return -1;
    const float eye[3] = { 0.0f, 0.0f, 0.0f };
    const float at[3]  = { 0.0f, 0.0f, 1.0f };
    const float up[3]  = { 0.0f, 1.0f, 0.0f };
    float view[16], proj[16];
    m4_look_at(view, eye, at, up);
    m4_perspective(proj, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);

    gm_tex far_tex, near_tex;
    make_solid(&far_tex, 200, 60, 60);
    make_solid(&near_tex, 40, 190, 90);
    gmb_3d_env_t env;
    gmb_3d_env_clear(&env);

    gmb_3d_thing_t th;
    gmb_3d_draw_stats_t st;
    memset(&th, 0, sizeof(th));
    th.x = 0.0f; th.y = -1.0f; th.z = 6.0f; th.w = 2.5f; th.h = 2.5f;
    th.rgba = far_tex.rgba; th.tw = far_tex.w; th.th = far_tex.h; th.light = 255;
    if (gmb_3d_thing_draw(t, view, proj, &th, eye, &env, &st) != GMB_3D_OK) { gm_tex_free(&far_tex); gm_tex_free(&near_tex); return -1; }

    th.z = 3.0f; th.w = 1.6f; th.h = 1.6f;
    th.rgba = near_tex.rgba;
    int rc = gmb_3d_thing_draw(t, view, proj, &th, eye, &env, &st);
    gm_tex_free(&far_tex);
    gm_tex_free(&near_tex);
    return rc == GMB_3D_OK ? 0 : -1;
}

/* 14) ordine e traslucenza: opaco, thing sopra, vetro davanti che fonde (M-ORDER). */
static int p4_scene_order(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x000000u, 0.0f) != GMB_3D_OK) return -1;
    float mvp[16];
    p4_camera(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h);

    p4_quad_t qb;
    p4_quad_init(&qb, -4.0f, -3.0f, 4.0f, 3.0f, 8.0f);
    gmb_3d_mesh_t mb = p4_mesh(&qb);
    gmb_3d_material_t matb;
    p4_flat_material(&matb, 40, 60, 210, 255);
    gmb_3d_draw_stats_t st;
    if (gmb_3d_mesh_draw(t, mvp, &mb, &matb, &st) != GMB_3D_OK) return -1;

    const float eye[3] = { 0.0f, 0.0f, 0.0f };
    const float at[3]  = { 0.0f, 0.0f, 1.0f };
    const float up[3]  = { 0.0f, 1.0f, 0.0f };
    float view[16], proj[16];
    m4_look_at(view, eye, at, up);
    m4_perspective(proj, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);
    gm_tex thing_tex;
    make_solid(&thing_tex, 40, 190, 90);
    gmb_3d_env_t env;
    gmb_3d_env_clear(&env);
    gmb_3d_thing_t th;
    memset(&th, 0, sizeof(th));
    th.x = 0.0f; th.y = -1.5f; th.z = 4.0f; th.w = 2.0f; th.h = 2.0f;
    th.rgba = thing_tex.rgba; th.tw = thing_tex.w; th.th = thing_tex.h; th.light = 255;
    if (gmb_3d_thing_draw(t, view, proj, &th, eye, &env, &st) != GMB_3D_OK) { gm_tex_free(&thing_tex); return -1; }
    gm_tex_free(&thing_tex);

    p4_quad_t qg;
    p4_quad_init(&qg, -3.0f, -2.0f, 3.0f, 2.0f, 2.0f);
    gmb_3d_mesh_t mg = p4_mesh(&qg);
    gmb_3d_material_t matg;
    p4_flat_material(&matg, 210, 40, 40, 255);
    matg.translucent = 1;
    return gmb_3d_mesh_draw(t, mvp, &mg, &matg, &st) == GMB_3D_OK ? 0 : -1;
}

/* 10) normali per vertice (M-LIGHT-N): due quad identici, tinta bianca e luce
 * base 40; il sinistro ha la normale verso la lampada (si accende), il destro
 * quella opposta (resta al solo ambiente). La differenza e' tutta nel N.L. */
static int p4_scene_normals(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x000000u, 0.0f) != GMB_3D_OK) return -1;
    float mvp[16];
    p4_camera(mvp, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h);

    gmb_3d_env_t env;
    gmb_3d_env_clear(&env);
    env.nlamps = 1;
    env.lamps[0].radius = 100.0f;
    env.lamps[0].amount = 255;

    static const float nfront[12] = { 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1 };
    static const float nback[12]  = { 0,0, 1, 0,0, 1, 0,0, 1, 0,0, 1 };

    p4_quad_t ql;
    p4_quad_init(&ql, -2.6f, -1.6f, -0.3f, 1.6f, 2.0f);
    gmb_3d_mesh_t ml = p4_mesh(&ql);
    ml.nrm = nfront;
    gmb_3d_material_t matl;
    p4_flat_material(&matl, 255, 255, 255, 40);
    gmb_3d_draw_stats_t sl;
    if (gmb_3d_mesh_draw_env(t, mvp, &ml, &matl, &env, &sl) != GMB_3D_OK) return -1;

    p4_quad_t qr;
    p4_quad_init(&qr, 0.3f, -1.6f, 2.6f, 1.6f, 2.0f);
    gmb_3d_mesh_t mr = p4_mesh(&qr);
    mr.nrm = nback;
    gmb_3d_material_t matr;
    p4_flat_material(&matr, 255, 255, 255, 40);
    gmb_3d_draw_stats_t sr;
    return gmb_3d_mesh_draw_env(t, mvp, &mr, &matr, &env, &sr) == GMB_3D_OK ? 0 : -1;
}

/* 11) antialiasing SSAA 2x per pixel: stesso quad, bordo quasi verticale vicino
 * al centro. Con AA il pixel di bordo fonde bianco e fondo; l'interno non cambia. */
static int p4_scene_aa(gmb_3d_target_t* t)
{
    if (gmb_3d_target_clear(t, 0x0F1729u, 0.0f) != GMB_3D_OK) return -1;
    float mvp[16];
    /* Camera a -3 e quad a z=0, come `scene_aa.bas`: il bordo cade vicino al
     * centro e la golden ha bianco, fusione e fondo. */
    const float eye[3] = { 0.0f, 0.0f, -3.0f }, at[3] = { 0.0f, 0.0f, 0.0f };
    mvp_build(mvp, eye, at, 60.0f * MESH_PI / 180.0f, (float)t->w / (float)t->h, 0.1f, 100.0f);

    p4_quad_t q;
    p4_quad_init(&q, 0.2f, -2.0f, 4.0f, 2.0f, 0.0f);
    gmb_3d_mesh_t m = p4_mesh(&q);
    gmb_3d_material_t mat;
    p4_flat_material(&mat, 255, 255, 255, 255);
    mat.two_sided = 1;    /* il quad e' specchiato: la faccia utile e' il retro */
    mat.aa = 1;
    gmb_3d_draw_stats_t st;
    return gmb_3d_mesh_draw_env(t, mvp, &m, &mat, NULL, &st) == GMB_3D_OK ? 0 : -1;
}

/* P1: triangoli. P2: settori (room/door/pitch) e mappa demo. P3: mesh sul
 * target ARGB+depth (cubo, piano, clip, z-buffer, back-face). P4: luce, fog,
 * cielo, thing, ordine e normali per vertice (M-LIGHT-N). L'accettazione della
 * fase passa da qui, non da un'impressione.
 */
#define NSCENES 20
static const char* const SCENE_NAMES[NSCENES] = {
    "tri_tex", "tri_mask", "tri_depth",
    "sector_room", "sector_door", "sector_pitch", "demo",
    "mesh_cube", "mesh_plane", "mesh_clip", "mesh_depth", "mesh_cull",
    "lamp_point", "lamp_spot", "fog", "sky", "thing", "order", "normals",
    "aa"
};
static const scene_fn SCENE_FNS[NSCENES] = {
    scene_tri_tex, scene_tri_mask, scene_tri_depth,
    w_scene_room, w_scene_door, w_scene_pitch, w_scene_demo,
    scene_mesh_cube, scene_mesh_plane, scene_mesh_clip, scene_mesh_depth, scene_mesh_cull,
    p4_scene_lamp_point, p4_scene_lamp_spot, p4_scene_fog, p4_scene_sky,
    p4_scene_thing, p4_scene_order, p4_scene_normals, p4_scene_aa
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

/* --- P3: check numerici del rasterizzatore mesh ------------------------- */

static void mesh_checks(int* ok, int* fail)
{
#define MCHECK(cond, what) do { if (cond) { (*ok)++; } else { (*fail)++; printf("FAIL %s (mesh)\n", (what)); } } while (0)

    gmb_3d_target_t t;
    memset(&t, 0, sizeof(t));
    if (gmb_3d_target_alloc(&t, W, H) != GMB_3D_OK) { (*fail)++; printf("FAIL mesh: alloc\n"); return; }
    gmb_3d_present_mode_set(GMB_3D_MODE_TARGET);

    float mvp[16];
    m4_perspective(mvp, 60.0f * MESH_PI / 180.0f, (float)W / (float)H, 0.1f, 100.0f);

    const int idx2[] = { 0, 1, 2, 0, 2, 3 };
    /* 0.9: dentro il frustum a z=-2 (mezza altezza visibile 1.154), così i due
     * triangoli NON vengono tagliati e i contatori sono esatti. */
    const float front[]  = { -0.9f,-0.9f,-2.0f,  0.9f,-0.9f,-2.0f,  0.9f,0.9f,-2.0f, -0.9f,0.9f,-2.0f };
    const float back[]   = { -0.9f,-0.9f,-2.0f, -0.9f,0.9f,-2.0f,  0.9f,0.9f,-2.0f,  0.9f,-0.9f,-2.0f };
    const float behind[] = { -0.9f,-0.9f, 2.0f,  0.9f,-0.9f, 2.0f,  0.9f,0.9f, 2.0f, -0.9f,0.9f, 2.0f };
    const float far_quad[] = { -2.0f,-2.0f,-4.0f, 2.0f,-2.0f,-4.0f, 2.0f,2.0f,-4.0f, -2.0f,2.0f,-4.0f };
    const float cross_near[] = { -2.5f,-2.5f,-0.05f, 2.5f,-2.5f,-2.0f, 2.5f,2.5f,-2.0f, -2.5f,2.5f,-0.05f };

    gmb_3d_material_t mat;
    memset(&mat, 0, sizeof(mat));
    mat.tint_r = 204; mat.tint_g = 34; mat.tint_b = 34; mat.light = 255;
    const uint32_t RED = ((uint32_t)204 << 16) | ((uint32_t)34 << 8) | 34u;
    size_t center = (size_t)(H / 2) * (size_t)W + (size_t)(W / 2);

    gmb_3d_target_clear(&t, 0x000000u, 0.0f);
    gmb_3d_mesh_t fq = { front, NULL, 4, idx2, 2 };
    gmb_3d_draw_stats_t st;
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &fq, &mat, &st) == GMB_3D_OK, "draw ok");
    MCHECK(st.tris_drawn == 2, "2 triangoli disegnati");
    MCHECK(st.pixels > 0, "pixel scritti");
    MCHECK(t.argb[center] == RED, "pixel centrale = tint");

    /* back-face: stesso rettangolo, winding invertito */
    gmb_3d_target_clear(&t, 0x000000u, 0.0f);
    gmb_3d_mesh_t bq = { back, NULL, 4, idx2, 2 };
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &bq, &mat, &st) == GMB_3D_OK, "draw retro ok");
    MCHECK(st.tris_drawn == 0 && st.tris_culled == 2, "back-face scartato");
    MCHECK(t.argb[center] == 0u, "back-face non scrive pixel");
    gmb_3d_material_t two = mat;
    two.two_sided = 1;
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &bq, &two, &st) == GMB_3D_OK && st.tris_drawn == 2,
           "two_sided disegna il retro");

    /* z-buffer: il lontano disegnato dopo il vicino non lo copre, e vale
     * comunque l'ordine inverso (profondità, non ordine di disegno). */
    gmb_3d_material_t green;
    memset(&green, 0, sizeof(green));
    green.tint_g = 200; green.light = 255;
    gmb_3d_mesh_t farq = { far_quad, NULL, 4, idx2, 2 };
    gmb_3d_target_clear(&t, 0x000000u, 0.0f);
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &fq, &mat, &st) == GMB_3D_OK, "near prima");
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &farq, &green, &st) == GMB_3D_OK, "far dopo");
    MCHECK(t.argb[center] == RED, "z-buffer: il lontano non copre il vicino");
    gmb_3d_target_clear(&t, 0x000000u, 0.0f);
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &farq, &green, &st) == GMB_3D_OK, "far prima");
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &fq, &mat, &st) == GMB_3D_OK, "near dopo");
    MCHECK(t.argb[center] == RED, "z-buffer indipendente dall'ordine");

    /* frustum: quad dietro la camera */
    gmb_3d_target_clear(&t, 0x000000u, 0.0f);
    gmb_3d_mesh_t hq = { behind, NULL, 4, idx2, 2 };
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &hq, &mat, &st) == GMB_3D_OK, "draw dietro ok");
    MCHECK(st.tris_drawn == 0 && st.pixels == 0, "dietro la camera: nessun pixel");

    /* clip near: quad che attraversa il piano */
    gmb_3d_target_clear(&t, 0x000000u, 0.0f);
    gmb_3d_mesh_t cq = { cross_near, NULL, 4, idx2, 2 };
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &cq, &two, &st) == GMB_3D_OK, "draw clip ok");
    MCHECK(st.tris_clipped >= 1 && st.tris_drawn >= 1 && st.pixels > 0,
           "near clip: tagliato e disegnato");

    /* UV: texture a 4 quadranti, quattro colori diversi ai quattro campioni */
    {
        gm_tex tex;
        gm_tex_alloc(&tex, 2, 2);
        gm_tex_set(&tex, 0, 0, 255, 0, 0, 255);
        gm_tex_set(&tex, 1, 0, 0, 255, 0, 255);
        gm_tex_set(&tex, 0, 1, 0, 0, 255, 255);
        gm_tex_set(&tex, 1, 1, 255, 255, 255, 255);
        gmb_3d_material_t mt;
        mesh_material(&mt, &tex, 0);
        const float uv4[] = { 0, 0, 1, 0, 1, 1, 0, 1 };
        gmb_3d_mesh_t tq = { far_quad, uv4, 4, idx2, 2 };
        gmb_3d_target_clear(&t, 0x000000u, 0.0f);
        MCHECK(gmb_3d_mesh_draw(&t, mvp, &tq, &mt, &st) == GMB_3D_OK && st.tris_drawn == 2,
               "texture: draw ok");
        uint32_t q0 = t.argb[(size_t)(H / 4) * (size_t)W + (size_t)(W / 4)];
        uint32_t q1 = t.argb[(size_t)(H / 4) * (size_t)W + (size_t)(3 * W / 4)];
        uint32_t q2 = t.argb[(size_t)(3 * H / 4) * (size_t)W + (size_t)(W / 4)];
        uint32_t q3 = t.argb[(size_t)(3 * H / 4) * (size_t)W + (size_t)(3 * W / 4)];
        MCHECK(q0 != q1 && q0 != q2 && q1 != q3, "texture: UV diversi ai quattro angoli");
        gm_tex_free(&tex);
    }

    /* errori espliciti, mai silenziosi */
    MCHECK(gmb_3d_mesh_draw(&t, NULL, &fq, &mat, &st) == GMB_3D_E_ARG, "matrice mancante");
    {
        gm_tex tex;
        gm_tex_alloc(&tex, 2, 2);
        gmb_3d_material_t mt;
        mesh_material(&mt, &tex, 0);
        MCHECK(gmb_3d_mesh_draw(&t, mvp, &fq, &mt, &st) == GMB_3D_E_ARG, "texture senza uv");
        gm_tex_free(&tex);
    }
    {
        const int bad[] = { 0, 1, 99 };
        gmb_3d_mesh_t bm = { front, NULL, 4, bad, 1 };
        MCHECK(gmb_3d_mesh_draw(&t, mvp, &bm, &mat, &st) == GMB_3D_E_ARG, "indice fuori range");
    }

    /* null mode: stato avanza, disegno no */
    gmb_3d_present_mode_set(GMB_3D_MODE_NULL);
    gmb_3d_target_clear(&t, 0x112233u, 0.0f);
    MCHECK(gmb_3d_mesh_draw(&t, mvp, &fq, &mat, &st) == GMB_3D_OK, "null mode ok");
    MCHECK(st.tris_drawn == 0 && st.pixels == 0, "null mode: nessun triangolo");
    MCHECK(t.argb[center] == 0x112233u, "null mode: target intatto");
    gmb_3d_present_mode_set(GMB_3D_MODE_TARGET);

    gmb_3d_target_free(&t);
#undef MCHECK
}

/* --- P4: check numerici di luce/fog/cielo/thing ------------------------- */

static void p4_checks(int* ok, int* fail)
{
#define P4CHECK(cond, what) do { if (cond) { (*ok)++; } else { (*fail)++; printf("FAIL %s (p4)\n", (what)); } } while (0)

    gmb_3d_env_t env;

    /* M-LIGHT: falloff lineare, metà raggio = metà quantità. */
    gmb_3d_env_clear(&env);
    env.nlamps = 1;
    env.lamps[0].radius = 10.0f;
    env.lamps[0].amount = 255;
    P4CHECK(gmb_3d_light_eval(&env, 5.0f, 0.0f, 0.0f, 0, 5.0f) == 128, "lampada: metà raggio = metà luce");
    P4CHECK(gmb_3d_light_eval(&env, 10.0f, 0.0f, 0.0f, 0, 10.0f) == 0, "lampada: al bordo niente luce");

    /* M-LIGHT: cono. Sull'asse pieno; a 60° con semiapertura 30° buio. */
    env.lamps[0].radius = 100.0f;
    env.lamps[0].dir[2] = 1.0f;
    env.lamps[0].half_angle = 30.0f * MESH_PI / 180.0f;
    P4CHECK(gmb_3d_light_eval(&env, 0.0f, 0.0f, 10.0f, 0, 10.0f) == 230, "spot: asse = falloff pieno (230)");
    P4CHECK(gmb_3d_light_eval(&env, 8.660254f, 0.0f, 5.0f, 0, 10.0f) == 0, "spot: fuori cono = buio");

    /* M-LIGHT-N: senza normale il risultato e' IDENTICO (P4 intatto). */
    {
        gmb_3d_env_t le;
        gmb_3d_env_clear(&le);
        le.nlamps = 1;
        le.lamps[0].radius = 10.0f;
        le.lamps[0].amount = 255;
        const float nf[3] = { 0.0f, 0.0f, -1.0f };
        const float nb[3] = { 0.0f, 0.0f,  1.0f };
        const float np[3] = { 1.0f, 0.0f,  0.0f };
        P4CHECK(gmb_3d_light_eval_n(&le, 0.0f, 0.0f, 5.0f, NULL, 0, 5.0f)
                == gmb_3d_light_eval(&le, 0.0f, 0.0f, 5.0f, 0, 5.0f),
                "M-LIGHT-N: senza normale = P4, byte per byte");
        P4CHECK(gmb_3d_light_eval_n(&le, 0.0f, 0.0f, 5.0f, nf, 0, 5.0f) == 128,
                "M-LIGHT-N: faccia verso la luce = falloff pieno (128)");
        P4CHECK(gmb_3d_light_eval_n(&le, 0.0f, 0.0f, 5.0f, nb, 0, 5.0f) == 0,
                "M-LIGHT-N: faccia girata = buio");
        P4CHECK(gmb_3d_light_eval_n(&le, 0.0f, 0.0f, 5.0f, np, 0, 5.0f) == 0,
                "M-LIGHT-N: normale perpendicolare = buio");
    }

    /* M-FOG: buio lineare alla distanza dichiarata. */
    gmb_3d_env_clear(&env);
    env.fog = 100.0f;
    P4CHECK(gmb_3d_light_eval(&env, 0.0f, 0.0f, 0.0f, 255, 50.0f) == 128, "fog: metà distanza = metà luce");
    P4CHECK(gmb_3d_light_eval(&env, 0.0f, 0.0f, 0.0f, 255, 100.0f) == 0, "fog: alla distanza = buio");

    /* C-SAME-COLORMAP: 32 livelli (100 -> q12 -> 98). */
    gmb_3d_env_clear(&env);
    env.colormap = 1;
    P4CHECK(gmb_3d_light_eval(&env, 0.0f, 0.0f, 0.0f, 100, 0.0f) == 98, "colormap: 100 -> 98 su 32 livelli");

    /* M-AA: SSAA 2x per pixel. Stessa geometria e due materiali: con AA spento un
     * campione, acceso quattro. L'area piatta non cambia, il bordo si fonde. */
    {
        gmb_3d_target_t ta, tb;
        memset(&ta, 0, sizeof(ta)); memset(&tb, 0, sizeof(tb));
        if (gmb_3d_target_alloc(&ta, 16, 16) == GMB_3D_OK
            && gmb_3d_target_alloc(&tb, 16, 16) == GMB_3D_OK)
        {
            float amvp[16];
            /* Stessa camera della superficie in `scene_aa.bas` (occhio a -3): i pixel
             * di bordo e area piatta sono quelli del MiniTest. */
            const float aeye[3] = { 0.0f, 0.0f, -3.0f }, aat[3] = { 0.0f, 0.0f, 0.0f };
            mvp_build(amvp, aeye, aat, 60.0f * MESH_PI / 180.0f, 1.0f, 0.1f, 100.0f);
            p4_quad_t q;
            p4_quad_init(&q, 0.2f, -2.0f, 4.0f, 2.0f, 0.0f);
            gmb_3d_mesh_t m = p4_mesh(&q);
            gmb_3d_material_t mat;
            p4_flat_material(&mat, 255, 255, 255, 255);
            mat.two_sided = 1;
            gmb_3d_draw_stats_t st;
            gmb_3d_target_clear(&ta, 0x0F1729u, 0.0f);
            gmb_3d_target_clear(&tb, 0x0F1729u, 0.0f);
            mat.aa = 0;
            int drawn_ok = gmb_3d_mesh_draw_env(&ta, amvp, &m, &mat, NULL, &st) == GMB_3D_OK;
            mat.aa = 1;
            drawn_ok = drawn_ok && gmb_3d_mesh_draw_env(&tb, amvp, &m, &mat, NULL, &st) == GMB_3D_OK;
            P4CHECK(drawn_ok, "AA: due draw ok");
            P4CHECK(ta.argb[(size_t)8 * 16 + 3] == tb.argb[(size_t)8 * 16 + 3],
                    "AA: area piatta identica");
            P4CHECK(ta.argb[(size_t)8 * 16 + 6] != tb.argb[(size_t)8 * 16 + 6],
                    "AA: pixel di bordo diverso");
            P4CHECK(tb.argb[(size_t)8 * 16 + 6] != 0xFFFFFFu
                    && tb.argb[(size_t)8 * 16 + 6] != 0x0F1729u,
                    "AA: il bordo e' una fusione (ne' bianco ne' fondo)");
        }
        else P4CHECK(0, "AA: alloc dei due target");
        gmb_3d_target_free(&ta); gmb_3d_target_free(&tb);
    }

    gmb_3d_target_t t;
    memset(&t, 0, sizeof(t));
    if (gmb_3d_target_alloc(&t, W, H) != GMB_3D_OK) { (*fail)++; printf("FAIL p4: alloc\n"); return; }
    gmb_3d_present_mode_set(GMB_3D_MODE_TARGET);

    float mvp[16];
    p4_camera(mvp, 60.0f * MESH_PI / 180.0f, (float)W / (float)H);
    size_t center = (size_t)(H / 2) * (size_t)W + (size_t)(W / 2);

    /* FAR: oltre la distanza il triangolo si salta. */
    {
        p4_quad_t q;
        p4_quad_init(&q, -3.0f, -3.0f, 3.0f, 3.0f, 8.0f);
        gmb_3d_mesh_t m = p4_mesh(&q);
        gmb_3d_material_t mat;
        p4_flat_material(&mat, 200, 200, 200, 255);
        gmb_3d_env_t fe;
        gmb_3d_env_clear(&fe);
        fe.far = 5.0f;
        gmb_3d_target_clear(&t, 0x000000u, 0.0f);
        gmb_3d_draw_stats_t st;
        P4CHECK(gmb_3d_mesh_draw_env(&t, mvp, &m, &mat, &fe, &st) == GMB_3D_OK, "FAR: draw ok");
        P4CHECK(st.tris_culled == 2 && st.pixels == 0, "FAR: oltre la distanza = niente pixel");
    }

    /* M-PALALPHA: 50/50 esatto. Opaco (100,0,0), vetro (200,200,200). */
    {
        p4_quad_t qo, qt;
        p4_quad_init(&qo, -3.0f, -3.0f, 3.0f, 3.0f, 4.0f);
        p4_quad_init(&qt, -3.0f, -3.0f, 3.0f, 3.0f, 2.0f);
        gmb_3d_mesh_t mo = p4_mesh(&qo), mt = p4_mesh(&qt);
        gmb_3d_material_t mato, matt;
        p4_flat_material(&mato, 100, 0, 0, 255);
        p4_flat_material(&matt, 200, 200, 200, 255);
        matt.translucent = 1;
        gmb_3d_target_clear(&t, 0x000000u, 0.0f);
        gmb_3d_draw_stats_t st;
        P4CHECK(gmb_3d_mesh_draw(&t, mvp, &mo, &mato, &st) == GMB_3D_OK, "translucido: opaco ok");
        P4CHECK(gmb_3d_mesh_draw(&t, mvp, &mt, &matt, &st) == GMB_3D_OK, "translucido: vetro ok");
        P4CHECK(t.argb[center] == (((uint32_t)150 << 16) | ((uint32_t)100 << 8) | 100u),
                "translucido: 50/50 esatto (150,100,100)");
    }

    /* M-SKY: la metà sinistra della picture cade a sinistra, la destra a destra. */
    {
        gm_tex tex;
        gm_tex_alloc(&tex, 32, 32);
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++)
                gm_tex_set(&tex, x, y, x < 16 ? 200 : 30, x < 16 ? 40 : 90, x < 16 ? 40 : 200, 255);
        gmb_3d_target_clear(&t, 0x000000u, 0.0f);
        P4CHECK(gmb_3d_sky_fill(&t, tex.rgba, tex.w, tex.h, 0.0f, 0.0f, 60.0f, 1.0f, 0.0f) == GMB_3D_OK,
                "sky: fill ok");
        P4CHECK(t.argb[(size_t)(H / 2) * (size_t)W + 2u] == (((uint32_t)200 << 16) | ((uint32_t)40 << 8) | 40u),
                "sky: meta' sinistra rossa");
        P4CHECK(t.argb[(size_t)(H / 2) * (size_t)W + (size_t)(W - 3)] == (((uint32_t)30 << 16) | ((uint32_t)90 << 8) | 200u),
                "sky: meta' destra blu");
        gm_tex_free(&tex);
    }

    /* M-THINGS: il thing vicino vince il depth test su quello lontano. */
    {
        const float eye[3] = { 0.0f, 0.0f, 0.0f };
        const float at[3] = { 0.0f, 0.0f, 1.0f };
        const float up[3] = { 0.0f, 1.0f, 0.0f };
        float view[16], proj[16];
        m4_look_at(view, eye, at, up);
        m4_perspective(proj, 60.0f * MESH_PI / 180.0f, (float)W / (float)H, 0.1f, 100.0f);
        gm_tex ft, nt;
        make_solid(&ft, 200, 60, 60);
        make_solid(&nt, 40, 190, 90);
        gmb_3d_env_t e2;
        gmb_3d_env_clear(&e2);
        gmb_3d_thing_t th;
        gmb_3d_draw_stats_t st;
        gmb_3d_target_clear(&t, 0x000000u, 0.0f);
        memset(&th, 0, sizeof(th));
        th.x = 0.0f; th.y = -1.0f; th.z = 6.0f; th.w = 3.0f; th.h = 3.0f;
        th.rgba = ft.rgba; th.tw = ft.w; th.th = ft.h; th.light = 255;
        P4CHECK(gmb_3d_thing_draw(&t, view, proj, &th, eye, &e2, &st) == GMB_3D_OK, "thing: lontano ok");
        th.z = 3.0f; th.w = 1.5f; th.h = 1.5f; th.rgba = nt.rgba;
        P4CHECK(gmb_3d_thing_draw(&t, view, proj, &th, eye, &e2, &st) == GMB_3D_OK, "thing: vicino ok");
        P4CHECK(t.argb[center] == (((uint32_t)40 << 16) | ((uint32_t)190 << 8) | 90u),
                "thing: il vicino copre il lontano");
        gm_tex_free(&ft);
        gm_tex_free(&nt);
    }

    gmb_3d_target_free(&t);
#undef P4CHECK
}

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

    /* P3: mesh sul target (clip/frustum, z-buffer, UV, back-face). */
    mesh_checks(&ok, &fail);

    /* P4: luce, fog, colormap, FAR, traslucenza, cielo, thing. */
    p4_checks(&ok, &fail);

    printf("P1-SELFTEST ok=%d fail=%d\n", ok, fail);
#undef CHECK
    return fail == 0 ? 0 : 1;
}

/* ═══ P5 — culling per oggetto (M-CULL) e banco della scena ═════════════════
 *
 * Due prove, entrambe sulla SUPERFICIE (`gmb_scene_*`), che è ciò che chiamano le
 * quattro gambe: la decisione di culling vive in `gmb_3d_scene.c` e qui si guarda
 * solo il risultato osservabile — DRAWN, CULLED, HASH — non l'interno.
 */

/* Otto box identici: quattro davanti alla camera (z >= 0) e quattro dietro
 * (z < -6). `twosided` è acceso perché CULLED deve contare SOLO ciò che il frustum
 * scarta: con il back-face attivo metà facce di un box chiuso finirebbero lì e il
 * numero non direbbe più niente sul culling per oggetto. */
static int p5_declare(int n, const double* zs)
{
    if (gmb_scene_reset() != GMB_3D_OK) return 0;
    if (gmb_scene_view(320, 180, 60.0) != GMB_3D_OK) return 0;
    if (gmb_scene_twosided(1) != GMB_3D_OK) return 0;
    if (gmb_scene_camera(0.0, 0.0, -6.0, 0.0, 0.0) != GMB_3D_OK) return 0;
    for (int i = 0; i < n; i++)
    {
        if (gmb_scene_object(200, 200, 200) != GMB_3D_OK) return 0;
        if (gmb_scene_box(-0.5, -0.5, -0.5, 0.5, 0.5, 0.5) != GMB_3D_OK) return 0;
        if (gmb_scene_objpos(0.0, 0.0, zs[i], 0.0) != GMB_3D_OK) return 0;
    }
    return 1;
}

static int run_p5_cull(void)
{
    int ok = 0, fail = 0;
    const double both[8] = { 0.0, 3.0, 6.0, 9.0, -12.0, -15.0, -18.0, -21.0 };
    const double front[4] = { 0.0, 3.0, 6.0, 9.0 };
    long drawn, culled = 0;

/* Un check = una riga di stampa solo quando fallisce, come il selftest. */
#define P5_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-CULL fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

    /* 1: ogni triangolo finisce in DRAWN o in CULLED — il culling per oggetto non
     *    fa sparire triangoli dal conteggio. */
    P5_CHECK(p5_declare(8, both), "dichiarazione di %d oggetti", 8);
    drawn = gmb_scene_render() == GMB_3D_OK ? gmb_scene_drawn() : -1;
    culled = gmb_scene_culled();
    P5_CHECK(drawn >= 0, "render fallito: %s", gmb_3d_last_error());
    P5_CHECK(drawn + culled == 96, "drawn+culled=%ld, atteso 96 (8 box x 12)", drawn + culled);

    /* 2: i quattro dietro la camera non si disegnano, i quattro davanti sì. */
    P5_CHECK(drawn == 48 && culled == 48, "davanti/indietro: drawn=%ld culled=%ld (attesi 48/48)",
             drawn, culled);

    /* 3: il frame è IDENTICO a quello di una scena in cui i quattro dietro non
     *    esistono mai: il culling salta lavoro, non cambia un pixel. */
    {
        long h_all = gmb_scene_hash();
        P5_CHECK(p5_declare(4, front), "dichiarazione dei soli 4 davanti");
        drawn = gmb_scene_render() == GMB_3D_OK ? gmb_scene_drawn() : -1;
        P5_CHECK(drawn == 48, "soli 4 davanti: drawn=%ld (atteso 48)", drawn);
        P5_CHECK(gmb_scene_hash() == h_all, "hash diverso: il culling ha cambiato i pixel");
    }

    /* 4: girando la camera i ruoli si invertono; i conteggi restano simmetrici. */
    {
        P5_CHECK(p5_declare(8, both), "dichiarazione di 8 oggetti (camera girata)");
        P5_CHECK(gmb_scene_camera(0.0, 0.0, -6.0, 180.0, 0.0) == GMB_3D_OK, "camera girata");
        drawn = gmb_scene_render() == GMB_3D_OK ? gmb_scene_drawn() : -1;
        culled = gmb_scene_culled();
        P5_CHECK(drawn == 48 && culled == 48, "camera girata: drawn=%ld culled=%ld (attesi 48/48)",
                 drawn, culled);
    }
#undef P5_CHECK

    printf("P5-CULL ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

/* ═══ P5 — superficie interattiva (M-PLAYER/M-COLLIDE/M-FLOOR/M-ZONE) ══════
 *
 * Tutto sulla superficie `gmb_scene_*`, che è ciò che chiamano le quattro
 * gambe: l'oracolo guarda SOLO i valori osservabili (posizione, quote, HIT,
 * zone), mai l'interno del modulo actor.
 *
 * Geometria (y = altezza, piano x/z, fronte a yaw 0 = +z):
 *   pavimento    y −2..0, x/z −50..50
 *   muro         z 20..30, alto 100
 *   gradino      x 10..20, z −10..10, alto 10 (≤ step 24)
 *   passabile    x −20..−10, z −10..10, alto 5, flag 4
 *   soffitto     x −45..−35, z −5..5, y 100..120
 *   mensole      x/z 60..70, tetti 10 e 20 (FLOORAT sovrapposto)
 */
static int p5_interact_scene(void)
{
    if (gmb_scene_reset() != GMB_3D_OK) return 0;
    if (gmb_scene_view(64, 32, 60.0) != GMB_3D_OK) return 0;
    if (gmb_scene_camera(0.0, 46.0, 0.0, 0.0, 0.0) != GMB_3D_OK) return 0;

    if (gmb_scene_object(80, 80, 80) != GMB_3D_OK) return 0;
    if (gmb_scene_box(-50.0, -2.0, -50.0, 50.0, 0.0, 50.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object(120, 120, 120) != GMB_3D_OK) return 0;
    if (gmb_scene_box(-30.0, 0.0, 20.0, 30.0, 100.0, 30.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object(200, 180, 60) != GMB_3D_OK) return 0;
    if (gmb_scene_box(10.0, 0.0, -10.0, 20.0, 10.0, 10.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object_flags(60, 200, 60, 4, 255) != GMB_3D_OK) return 0;
    if (gmb_scene_box(-20.0, 0.0, -10.0, -10.0, 5.0, 10.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object(90, 90, 160) != GMB_3D_OK) return 0;
    if (gmb_scene_box(-45.0, 100.0, -5.0, -35.0, 120.0, 5.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object(180, 140, 180) != GMB_3D_OK) return 0;
    if (gmb_scene_box(60.0, 0.0, 60.0, 70.0, 10.0, 70.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object(180, 140, 180) != GMB_3D_OK) return 0;
    if (gmb_scene_box(60.0, 0.0, 60.0, 70.0, 20.0, 70.0) != GMB_3D_OK) return 0;

    /* M-ZONE: la PRIMA creata vince, anche se una successiva si sovrappone. */
    if (gmb_scene_zone(2, -20.0, -100.0, 20.0, 100.0, -20.0, 20.0, "outer") != GMB_3D_OK) return 0;
    if (gmb_scene_zone(1, -10.0, 0.0, 10.0, 50.0, -10.0, 10.0, "inner") != GMB_3D_OK) return 0;
    if (gmb_scene_zone(3, 30.0, 10.0, 40.0, 20.0, 30.0, 40.0, "shelf") != GMB_3D_OK) return 0;
    return 1;
}

static int run_p5_interact(void)
{
    int ok = 0, fail = 0;
#define P5I_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-INTERACT fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

    P5I_CHECK(p5_interact_scene(), "dichiarazione della scena interattiva");

    /* Letture camera: CAMERA posiziona l'occhio, EYE è l'altezza sui piedi. */
    P5I_CHECK(gmb_scene_x() == 0.0 && gmb_scene_y() == 46.0 && gmb_scene_z() == 0.0,
              "camera x/y/z = %g/%g/%g (attesi 0/46/0)", gmb_scene_x(), gmb_scene_y(), gmb_scene_z());
    P5I_CHECK(gmb_scene_eye() == 46.0 && gmb_scene_yaw() == 0.0 && gmb_scene_pitch() == 0.0,
              "eye/yaw/pitch = %g/%g/%g (attesi 46/0/0)", gmb_scene_eye(), gmb_scene_yaw(), gmb_scene_pitch());
    P5I_CHECK(gmb_scene_hit() == 0.0, "HIT iniziale = %g (atteso 0)", gmb_scene_hit());

    /* M-FLOOR: superficie più alta a o sotto il punto; sentinella fuori piano. */
    P5I_CHECK(gmb_scene_floor_at(0.0, 0.0, 0.0) == 0.0, "FLOORAT sul pavimento = %g", gmb_scene_floor_at(0.0, 0.0, 0.0));
    P5I_CHECK(gmb_scene_floor_at(65.0, 25.0, 65.0) == 20.0, "FLOORAT sopra le mensole = %g (atteso 20)",
              gmb_scene_floor_at(65.0, 25.0, 65.0));
    P5I_CHECK(gmb_scene_floor_at(65.0, 15.0, 65.0) == 10.0, "FLOORAT fra le mensole = %g (atteso 10)",
              gmb_scene_floor_at(65.0, 15.0, 65.0));
    P5I_CHECK(gmb_scene_floor_at(65.0, 5.0, 65.0) == -1000000.0, "FLOORAT sotto le mensole = %g (atteso -1000000)",
              gmb_scene_floor_at(65.0, 5.0, 65.0));
    P5I_CHECK(gmb_scene_ceil_at(-40.0, 0.0, 0.0) == 100.0, "CEILAT sotto il soffitto = %g (atteso 100)",
              gmb_scene_ceil_at(-40.0, 0.0, 0.0));
    P5I_CHECK(gmb_scene_ceil_at(0.0, 0.0, 0.0) == -1000000.0, "CEILAT a cielo aperto = %g", gmb_scene_ceil_at(0.0, 0.0, 0.0));

    /* M-PASSABLE: FLOORAT vede il passabile, il corpo ci passa attraverso. */
    P5I_CHECK(gmb_scene_floor_at(-15.0, 6.0, 0.0) == 5.0, "FLOORAT sul passabile = %g (atteso 5)",
              gmb_scene_floor_at(-15.0, 6.0, 0.0));
    P5I_CHECK(gmb_scene_camera(-15.0, 46.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "camera sul passabile");
    P5I_CHECK(gmb_scene_move(5.0, 0.0) == GMB_3D_OK, "MOVE attraverso il passabile");
    P5I_CHECK(fabs(gmb_scene_z() - 5.0) < 1e-3 && fabs(gmb_scene_y() - 46.0) < 1e-3,
              "passabile: z=%g y=%g (attesi 5/46, nessun gradino)", gmb_scene_z(), gmb_scene_y());
    P5I_CHECK(gmb_scene_hit() == 0.0, "passabile: HIT=%g (atteso 0)", gmb_scene_hit());

    /* M-COLLIDE: scivolamento e stop sul muro. */
    P5I_CHECK(gmb_scene_camera(0.0, 46.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "camera davanti al muro");
    P5I_CHECK(gmb_scene_move(100.0, 0.0) == GMB_3D_OK, "MOVE contro il muro");
    P5I_CHECK(fabs(gmb_scene_z() - 6.0) < 1e-3, "muro: z=%g (atteso 6 = 20 − raggio 14)", gmb_scene_z());
    P5I_CHECK((int)gmb_scene_hit() & 1, "muro: HIT=%g senza bit 1", gmb_scene_hit());
    P5I_CHECK(gmb_scene_move(10.0, 0.0) == GMB_3D_OK, "MOVE addosso al muro");
    P5I_CHECK((int)gmb_scene_hit() & 2, "fermo: HIT=%g senza bit 2", gmb_scene_hit());

    /* Scivolamento: spinta obliqua, la componente lungo il muro sopravvive. */
    P5I_CHECK(gmb_scene_camera(0.0, 46.0, 6.0, 0.0, 0.0) == GMB_3D_OK, "camera a contatto");
    P5I_CHECK(gmb_scene_move(10.0, 10.0) == GMB_3D_OK, "MOVE obliquo sul muro");
    P5I_CHECK(fabs(gmb_scene_x() - 10.0) < 1e-3 && fabs(gmb_scene_z() - 6.0) < 1e-3,
              "scivolamento: x=%g z=%g (attesi 10/6)", gmb_scene_x(), gmb_scene_z());

    /* Gradino ≤ step: si sale (HIT bit 4). */
    P5I_CHECK(gmb_scene_camera(15.0, 46.0, -20.0, 0.0, 0.0) == GMB_3D_OK, "camera davanti al gradino");
    P5I_CHECK(gmb_scene_move(15.0, 0.0) == GMB_3D_OK, "MOVE sul gradino");
    P5I_CHECK(fabs(gmb_scene_y() - 56.0) < 1e-3, "gradino: y=%g (atteso 56 = 10 + occhio 46)", gmb_scene_y());
    P5I_CHECK((int)gmb_scene_hit() & 4, "gradino: HIT=%g senza bit 4", gmb_scene_hit());

    /* M-ZONE: prima creata vince, tolleranza di 2 sotto la base, sentinelle. */
    P5I_CHECK(gmb_scene_zone_at(0.0, 0.0, 0.0) == 2, "ZONE = %lld (atteso 2, la prima creata)",
              (long long)gmb_scene_zone_at(0.0, 0.0, 0.0));
    P5I_CHECK(gmb_scene_zone_at(0.0, 500.0, 0.0) == -1, "ZONE fuori quota = %lld",
              (long long)gmb_scene_zone_at(0.0, 500.0, 0.0));
    P5I_CHECK(gmb_scene_zone_at(35.0, 8.5, 35.0) == 3, "ZONE a 1,5 sotto la base = %lld (atteso 3)",
              (long long)gmb_scene_zone_at(35.0, 8.5, 35.0));
    P5I_CHECK(gmb_scene_zone_at(35.0, 7.5, 35.0) == -1, "ZONE a 2,5 sotto la base = %lld (atteso -1)",
              (long long)gmb_scene_zone_at(35.0, 7.5, 35.0));
    P5I_CHECK(!strcmp(gmb_scene_zonetag_text(2), "outer") && !strcmp(gmb_scene_zonetag_text(1), "inner"),
              "ZONETAG = \"%s\"/\"%s\" (attesi outer/inner)", gmb_scene_zonetag_text(2), gmb_scene_zonetag_text(1));
    P5I_CHECK(gmb_scene_zonetag_text(99)[0] == '\0', "ZONETAG di un id inesistente non è vuoto");

    /* TURN: yaw in gradi, fronte a 90° = +x; LIFT sposta piedi e occhio insieme. */
    P5I_CHECK(gmb_scene_camera(0.0, 46.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "camera per TURN");
    P5I_CHECK(gmb_scene_turn(90.0, -10.0) == GMB_3D_OK, "TURN 90/-10");
    P5I_CHECK(gmb_scene_yaw() == 90.0 && gmb_scene_pitch() == -10.0, "TURN: yaw=%g pitch=%g",
              gmb_scene_yaw(), gmb_scene_pitch());
    P5I_CHECK(gmb_scene_move(5.0, 0.0) == GMB_3D_OK, "MOVE dopo TURN");
    P5I_CHECK(fabs(gmb_scene_x() - 5.0) < 1e-3 && fabs(gmb_scene_z()) < 1e-3,
              "TURN+MOVE: x=%g z=%g (attesi 5/0)", gmb_scene_x(), gmb_scene_z());
    P5I_CHECK(gmb_scene_lift(5.0) == GMB_3D_OK && fabs(gmb_scene_y() - 51.0) < 1e-3,
              "LIFT: y=%g (atteso 51)", gmb_scene_y());
    P5I_CHECK(gmb_scene_eye() == 46.0, "LIFT non cambia EYE (%g)", gmb_scene_eye());

    /* PLAYER: cambia le misure del corpo; l'occhio resta sopra i piedi. */
    P5I_CHECK(gmb_scene_player(10.0, 50.0, 40.0, 20.0) == GMB_3D_OK, "PLAYER 10/50/40/20");
    P5I_CHECK(gmb_scene_eye() == 40.0 && fabs(gmb_scene_y() - 45.0) < 1e-3,
              "PLAYER: eye=%g y=%g (attesi 40/45 = piedi 5 + occhio 40)", gmb_scene_eye(), gmb_scene_y());
    P5I_CHECK(gmb_scene_player(0.0, 50.0, 40.0, 20.0) != GMB_3D_OK && strstr(gmb_3d_last_error(), "raggio"),
              "PLAYER con raggio 0 non è un errore esplicito");
    P5I_CHECK(gmb_scene_turn(0.0, 100.0) != GMB_3D_OK && strstr(gmb_3d_last_error(), "pitch"),
              "TURN oltre 89° non è un errore esplicito");

#undef P5I_CHECK
    printf("P5-INTERACT ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

/* ── P5/M-RAYS: raggi della scena (SHOOT/USE/SEES) ────────────────────────────
 * Scena costruita a mano, con i numeri scelti per essere esatti:
 *  - muro solido a z = 100 (oggetto 1);
 *  - vetro see-through (flag 8) a z = 50 (oggetto 2): SHOOT ci passa, USE no;
 *  - scatola passabile (flag 4) a z = 40 dal secondo punto di vista (oggetto 3):
 *    ai raggi NON è trasparente (`M-RAYS`: passa solo il flag 8);
 *  - thing 7 a z = 20, alto 80: il colpo lo prende prima della scatola;
 *  - muro a z = 10 (oggetto 4) per spegnere la linea di vista del thing. */
static int p5_ray_scene(void)
{
    if (gmb_scene_reset() != GMB_3D_OK) return 0;

    if (gmb_scene_object(200, 200, 200) != GMB_3D_OK) return 0;
    if (gmb_scene_box(-50.0, 0.0, 100.0, 50.0, 100.0, 110.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object_flags(120, 200, 255, 8, 255) != GMB_3D_OK) return 0;
    if (gmb_scene_box(-50.0, 0.0, 50.0, 50.0, 100.0, 55.0) != GMB_3D_OK) return 0;
    if (gmb_scene_object_flags(60, 200, 60, 4, 255) != GMB_3D_OK) return 0;
    if (gmb_scene_box(90.0, 0.0, 40.0, 110.0, 60.0, 50.0) != GMB_3D_OK) return 0;
    /* Il thing arriva DOPO la prova del passabile: è più vicino (z 20) e
     * coprirebbe il colpo (riga più sotto la verifica). */
    return 1;
}

static int run_p5_rays(void)
{
    int ok = 0, fail = 0;
#define P5R_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-RAYS fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

    P5R_CHECK(p5_ray_scene(), "dichiarazione della scena dei raggi");

    /* M-RAYS/see-through: il vetro a z = 50 NON ferma il colpo, il muro a 100 sì. */
    P5R_CHECK(gmb_scene_camera(0.0, 46.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "camera dei raggi");
    P5R_CHECK(gmb_scene_shoot(300.0) == 1, "SHOOT attraverso il vetro = %lld (atteso 1)",
              (long long)gmb_scene_shoot(300.0));
    P5R_CHECK(gmb_scene_shotkind() == 1.0 && gmb_scene_shotobj() == 1.0,
              "SHOTKIND/SHOTOBJ = %g/%g (attesi 1/1)", gmb_scene_shotkind(), gmb_scene_shotobj());
    P5R_CHECK(fabs(gmb_scene_shotdist() - 100.0) < 1e-3, "SHOTDIST = %g (atteso 100)", gmb_scene_shotdist());
    P5R_CHECK(fabs(gmb_scene_shotx()) < 1e-3 && fabs(gmb_scene_shoty() - 46.0) < 1e-3
              && fabs(gmb_scene_shotz() - 100.0) < 1e-3,
              "SHOTX/Y/Z = %g/%g/%g (attesi 0/46/100)", gmb_scene_shotx(), gmb_scene_shoty(), gmb_scene_shotz());

    /* USE: qualunque sia il flag, quindi prende il vetro (2) prima del muro (1). */
    P5R_CHECK(gmb_scene_use(300.0) == 2, "USE davanti = %lld (atteso 2, il vetro)",
              (long long)gmb_scene_use(300.0));
    P5R_CHECK(gmb_scene_use(20.0) == -1, "USE a portata corta = %lld (atteso -1)",
              (long long)gmb_scene_use(20.0));

    /* Flag 4 (passabile): collisione sì, trasparenza no. */
    P5R_CHECK(gmb_scene_camera(100.0, 46.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "camera sulla scatola passabile");
    P5R_CHECK(gmb_scene_shoot(1000.0) == 3, "SHOOT sul passabile = %lld (atteso 3)",
              (long long)gmb_scene_shoot(1000.0));

    /* Thing: cilindro w/2 (fronte a z = 20 − 5 = 15), colpito prima della
     * scatola; SHOTOBJ resta 0 perché non è un oggetto. */
    P5R_CHECK(gmb_scene_thing(7, 100.0, 0.0, 20.0, 10.0, 80.0) == GMB_3D_OK, "thing 7");
    P5R_CHECK(gmb_scene_shoot(100.0) == 7 && gmb_scene_shotkind() == 2.0 && gmb_scene_shotobj() == 0.0,
              "SHOOT sul thing = %lld kind=%g obj=%g (attesi 7/2/0)",
              (long long)gmb_scene_shoot(100.0), gmb_scene_shotkind(), gmb_scene_shotobj());
    P5R_CHECK(fabs(gmb_scene_shotdist() - 15.0) < 0.5, "SHOTDIST sul thing = %g (atteso ~15)",
              gmb_scene_shotdist());

    /* SEES: 1 con la via libera, 0 con il muro in mezzo, -1 se il thing non esiste. */
    P5R_CHECK(gmb_scene_sees(7) == 1, "SEES(7) libero = %lld (atteso 1)", (long long)gmb_scene_sees(7));
    P5R_CHECK(gmb_scene_object(200, 120, 120) == GMB_3D_OK
              && gmb_scene_box(90.0, 0.0, 10.0, 110.0, 100.0, 12.0) == GMB_3D_OK,
              "muro fra il thing e l'occhio");
    P5R_CHECK(gmb_scene_sees(7) == 0, "SEES(7) col muro = %lld (atteso 0)", (long long)gmb_scene_sees(7));
    P5R_CHECK(gmb_scene_sees(99) == -1, "SEES(99) = %lld (atteso -1)", (long long)gmb_scene_sees(99));

    /* Nessun colpo: sentinelle 0/-1/-1000000, non zeri ambigui. */
    P5R_CHECK(gmb_scene_camera(0.0, 46.0, 0.0, 90.0, 0.0) == GMB_3D_OK, "camera verso il vuoto");
    P5R_CHECK(gmb_scene_shoot(50.0) == -1 && gmb_scene_shotkind() == 0.0
              && gmb_scene_shotdist() == -1.0 && gmb_scene_shotx() == -1000000.0,
              "SHOOT a vuoto = %lld kind=%g dist=%g x=%g (attesi -1/0/-1/-1000000)",
              (long long)gmb_scene_shoot(50.0), gmb_scene_shotkind(), gmb_scene_shotdist(), gmb_scene_shotx());

    /* Letture, non comandi: gittata non positiva o non finita = «niente in
     * portata» (−1), mai un errore di superficie né un colpo inventato. */
    P5R_CHECK(gmb_scene_shoot(0.0) == -1 && gmb_scene_shotkind() == 0.0 && gmb_scene_shotdist() == -1.0,
              "SHOOT con gittata 0 non torna «nessun colpo»");
    P5R_CHECK(gmb_scene_use(-1.0) == -1, "USE con portata negativa non torna «nessun colpo»");
    P5R_CHECK(gmb_scene_shoot(NAN) == -1 && gmb_scene_use(NAN) == -1,
              "gittata NaN non torna «nessun colpo»");

#undef P5R_CHECK
    printf("P5-RAYS ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

/* P5 — statistiche della scena (`SCENE.STATS`, specifica M-STATS). Le chiavi
 * danno gli stessi numeri delle letture già esistenti, più i contatori del frame
 * che servono a DIMOSTRARE R4 (allocs) e R6 (renders/renders_max): un RENDER che
 * allocasse, o un frame con più di un RENDER, si vede qui e non a occhio. */
static int run_p5_stats(void)
{
    int ok = 0, fail = 0;
#define P5S_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-STATS fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

    P5S_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET");
    P5S_CHECK(gmb_scene_view(64, 32, 60.0) == GMB_3D_OK, "VIEW");
    P5S_CHECK(gmb_scene_camera(0.0, 46.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "CAMERA");
    /* Box INTERAMENTE dentro il frustum: così draw+culled è esattamente 12 e
     * l'invariante non dipende da come il clip taglia i triangoli di bordo. */
    P5S_CHECK(gmb_scene_object(200, 200, 200) == GMB_3D_OK
              && gmb_scene_box(-10.0, 20.0, 60.0, 10.0, 70.0, 70.0) == GMB_3D_OK,
              "dichiarazione della scena");

    /* Prima di ogni RENDER: la geometria dichiarata si legge, il pass è a zero. */
    P5S_CHECK(gmb_scene_stats("tris") == 12.0 && gmb_scene_stats("objs") == 1.0,
              "tris/objs = %g/%g (attesi 12/1)", gmb_scene_stats("tris"), gmb_scene_stats("objs"));
    P5S_CHECK(gmb_scene_stats("draw") == 0.0 && gmb_scene_stats("culled") == 0.0
              && gmb_scene_stats("renders") == 0.0,
              "draw/culled/renders prima del RENDER = %g/%g/%g (attesi 0/0/0)",
              gmb_scene_stats("draw"), gmb_scene_stats("culled"), gmb_scene_stats("renders"));
    P5S_CHECK(gmb_scene_stats("bytes") > 0.0, "bytes = %g (attesi > 0)", gmb_scene_stats("bytes"));

    /* Chiave ignota o assente: -1, sentinella della specifica (lettura, non errore). */
    P5S_CHECK(gmb_scene_stats("nonesiste") == -1.0 && gmb_scene_stats(NULL) == -1.0,
              "chiave sconosciuta = %g / %g (attesi -1/-1)",
              gmb_scene_stats("nonesiste"), gmb_scene_stats(NULL));
    /* Le chiavi non distinguono maiuscole (BASIC non lo fa). */
    P5S_CHECK(gmb_scene_stats("DRAW") == gmb_scene_stats("draw"),
              "DRAW = %g, draw = %g (attesi uguali)", gmb_scene_stats("DRAW"), gmb_scene_stats("draw"));

    /* Un RENDER: contatori del pass e conteggio per frame. */
    unsigned long a0 = gmb_3d_alloc_count();
    P5S_CHECK(gmb_scene_render() == GMB_3D_OK, "primo RENDER");
    P5S_CHECK(gmb_scene_stats("draw") == (double)gmb_scene_drawn()
              && gmb_scene_stats("culled") == (double)gmb_scene_culled(),
              "draw/culled = %g/%g, letture %lld/%lld (attesi uguali)",
              gmb_scene_stats("draw"), gmb_scene_stats("culled"),
              (long long)gmb_scene_drawn(), (long long)gmb_scene_culled());
    P5S_CHECK(gmb_scene_stats("draw") + gmb_scene_stats("culled") == 12.0,
              "draw+culled = %g (attesi 12: nessun triangolo perso)",
              gmb_scene_stats("draw") + gmb_scene_stats("culled"));
    P5S_CHECK(gmb_scene_stats("pixels") == (double)gmb_scene_pixels() && gmb_scene_stats("pixels") > 0.0,
              "pixels = %g (lettura %lld, attesi uguali e > 0)",
              gmb_scene_stats("pixels"), (long long)gmb_scene_pixels());
    P5S_CHECK(gmb_scene_stats("renders") == 1.0 && gmb_scene_stats("renders_max") == 1.0,
              "renders/renders_max = %g/%g (attesi 1/1)",
              gmb_scene_stats("renders"), gmb_scene_stats("renders_max"));

    /* Secondo RENDER nello stesso frame: R6 si vede (max 2), e il frame non ha
     * allocato nulla (R4). */
    P5S_CHECK(gmb_scene_render() == GMB_3D_OK, "secondo RENDER");
    P5S_CHECK(gmb_scene_stats("renders") == 2.0 && gmb_scene_stats("renders_max") == 2.0,
              "renders/renders_max = %g/%g (attesi 2/2)",
              gmb_scene_stats("renders"), gmb_scene_stats("renders_max"));
    P5S_CHECK(gmb_3d_alloc_count() == a0, "i due RENDER hanno allocato %lu",
              gmb_3d_alloc_count() - a0);
    P5S_CHECK(gmb_scene_stats("allocs") == (double)gmb_3d_alloc_count(),
              "allocs = %g (contatore del modulo %lu)",
              gmb_scene_stats("allocs"), gmb_3d_alloc_count());

    /* RESET: contatori di frame a zero e geometria vuota, memoria riservata
     * CONSERVATA (un RESET non libera: il frame dopo non rialloca). */
    double b0 = gmb_scene_stats("bytes");
    P5S_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET finale");
    P5S_CHECK(gmb_scene_stats("renders") == 0.0 && gmb_scene_stats("renders_max") == 0.0,
              "RESET non azzera renders/renders_max = %g/%g",
              gmb_scene_stats("renders"), gmb_scene_stats("renders_max"));
    P5S_CHECK(gmb_scene_stats("tris") == 0.0 && gmb_scene_stats("objs") == 0.0,
              "RESET non azzera tris/objs = %g/%g",
              gmb_scene_stats("tris"), gmb_scene_stats("objs"));
    P5S_CHECK(gmb_scene_stats("bytes") == b0 && b0 > 0.0,
              "RESET ha liberato la memoria riservata (bytes %g -> %g)", b0, gmb_scene_stats("bytes"));

#undef P5S_CHECK
    printf("P5-STATS ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

/* P5 — keyframe TRS (`SCENE KEY` / `SCENE TIME`, specifica M-KEY). Il test
 * guarda il RISULTATO nel frame (buffer di PICK: quale pixel copre l'oggetto),
 * non lo stato interno: legge solo la facciata C, come le altre gambe. Copre
 * interpolazione, clamp fuori intervallo, pitch/roll applicati davvero, tetto di
 * 32 chiavi per oggetto, errori di argomento, nessuna allocazione per frame e
 * RESET che azzera piste e tempo. */
static int run_p5_key(void)
{
    int ok = 0, fail = 0;
#define P5K_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-KEY fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

    /* Un quad 20×20 davanti alla camera, interamente nel frustum. */
#define P5K_SCENE() do { \
        P5K_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET"); \
        P5K_CHECK(gmb_scene_view(32, 32, 60.0) == GMB_3D_OK, "VIEW"); \
        P5K_CHECK(gmb_scene_camera(0.0, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "CAMERA"); \
        P5K_CHECK(gmb_scene_object(255, 0, 0) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, -10.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, -10.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, 10.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, 10.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK, "dichiarazione del quad"); \
        P5K_CHECK(gmb_scene_pick_enable(1) == GMB_3D_OK, "PICK on"); \
    } while (0)

    P5K_SCENE();
    /* Errori di argomento: oggetto inesistente, tempo negativo, tempo non finito. */
    P5K_CHECK(gmb_scene_key(0, 0.0, 0.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_E_ARG
              && gmb_scene_key(2, 0.0, 0.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_E_ARG,
              "KEY su oggetto 0 o 2 (esiste solo l'1) non rifiutata");
    P5K_CHECK(gmb_scene_key(1, -1.0, 0.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_E_ARG
              && gmb_scene_time(-1.0) == GMB_3D_E_ARG,
              "tempo negativo non rifiutato");
    P5K_CHECK(gmb_3d_last_error()[0] != 0, "errore di KEY senza messaggio");

    /* Tetto dichiarato: 32 chiavi per oggetto, la 33a è un errore esplicito. */
    int keys_ok = 1;
    for (int i = 0; i < 32; i++)
        if (gmb_scene_key(1, (double)i, 0.0, 0.0, 60.0, 0.0, 0.0, 0.0) != GMB_3D_OK) keys_ok = 0;
    P5K_CHECK(keys_ok, "32 chiavi su un oggetto non accettate");
    P5K_CHECK(gmb_scene_key(1, 40.0, 0.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_E_ARG,
              "la 33a chiave non è stata rifiutata");

    /* Interpolazione lineare e clamp, letti dal buffer di PICK (id 1 = quad).
     * Con lo sguardo verso +z le x positive cadono a SINISTRA: il pixel 6 è
     * x ≈ +22, il 25 è x ≈ -22, il 16 è il centro. */
    P5K_SCENE();
    P5K_CHECK(gmb_scene_key(1, 0.0, -20.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_key(1, 1.0, 20.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_OK,
              "due chiavi di posizione");
    P5K_CHECK(gmb_scene_time(0.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pick(25, 16) == 1 && gmb_scene_pick(6, 16) == 0,
              "t=0: pick 25/6 = %lld/%lld (attesi 1/0)",
              (long long)gmb_scene_pick(25, 16), (long long)gmb_scene_pick(6, 16));
    P5K_CHECK(gmb_scene_time(0.5) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pick(16, 16) == 1 && gmb_scene_pick(6, 16) == 0,
              "t=0.5: il centro non è coperto (pick 16 = %lld)", (long long)gmb_scene_pick(16, 16));
    P5K_CHECK(gmb_scene_time(1.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pick(6, 16) == 1 && gmb_scene_pick(25, 16) == 0,
              "t=1: pick 6/25 = %lld/%lld (attesi 1/0)",
              (long long)gmb_scene_pick(6, 16), (long long)gmb_scene_pick(25, 16));
    /* Oltre l'ultima chiave vince l'ultima: nessuna estrapolazione. */
    P5K_CHECK(gmb_scene_time(5.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pick(6, 16) == 1 && gmb_scene_pick(25, 16) == 0,
              "t=5 (clamp): pick 6/25 = %lld/%lld (attesi 1/0)",
              (long long)gmb_scene_pick(6, 16), (long long)gmb_scene_pick(25, 16));

    /* Nessuna allocazione per frame: il campionamento legge solo array riservati. */
    unsigned long a0 = gmb_3d_alloc_count();
    for (int i = 0; i < 50; i++)
    {
        (void)gmb_scene_time((double)i * 0.02);
        (void)gmb_scene_render();
    }
    P5K_CHECK(gmb_3d_alloc_count() == a0, "50 frame con chiavi hanno allocato %lu",
              gmb_3d_alloc_count() - a0);

    /* Pitch e roll arrivano al rasterizzatore: 90° di pitch mette il quad di
     * taglio (nessun triangolo disegnato), 90° di roll su un quad non quadrato
     * cambia il frame. */
    P5K_SCENE();
    P5K_CHECK(gmb_scene_key(1, 0.0, 0.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_key(1, 1.0, 0.0, 0.0, 60.0, 0.0, 90.0, 0.0) == GMB_3D_OK,
              "due chiavi di pitch");
    P5K_CHECK(gmb_scene_time(1.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 0,
              "pitch 90: triangoli disegnati = %lld (attesi 0)", (long long)gmb_scene_drawn());

    /* Quad 20×8 (non quadrato): con 90° di roll l'immagine cambia davvero. */
    P5K_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET roll");
    P5K_CHECK(gmb_scene_view(32, 32, 60.0) == GMB_3D_OK, "VIEW roll");
    P5K_CHECK(gmb_scene_camera(0.0, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "CAMERA roll");
    P5K_CHECK(gmb_scene_object(255, 0, 0) == GMB_3D_OK
              && gmb_scene_vertex(-10.0, -4.0, 0.0, 0.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(10.0, -4.0, 0.0, 1.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(10.0, 4.0, 0.0, 1.0, 0.0) == GMB_3D_OK
              && gmb_scene_vertex(-10.0, 4.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK, "quad 20x8 per il roll");
    P5K_CHECK(gmb_scene_key(1, 0.0, 0.0, 0.0, 60.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_key(1, 1.0, 0.0, 0.0, 60.0, 0.0, 0.0, 90.0) == GMB_3D_OK,
              "due chiavi di roll");
    P5K_CHECK(gmb_scene_time(0.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK, "RENDER roll t=0");
    long long h0 = gmb_scene_hash();
    P5K_CHECK(gmb_scene_time(1.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_hash() != h0,
              "roll 90 non ha cambiato il frame (hash %lld)", (long long)gmb_scene_hash());

    /* RESET: piste e tempo a zero, l'oggetto torna alla trasformazione dichiarata
     * (OBJPOS; senza chiavi niente lo sposta, quindi resta centrato). */
    P5K_SCENE();
    P5K_CHECK(gmb_scene_objpos(0.0, 0.0, 60.0, 0.0) == GMB_3D_OK, "OBJPOS pre-RESET");
    P5K_CHECK(gmb_scene_time(3.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK, "RENDER pre-RESET");
    P5K_SCENE();
    P5K_CHECK(gmb_scene_objpos(0.0, 0.0, 60.0, 0.0) == GMB_3D_OK, "OBJPOS dopo RESET");
    P5K_CHECK(gmb_scene_time(0.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pick(16, 16) == 1,
              "dopo RESET l'oggetto non è più al centro (pick 16 = %lld)",
              (long long)gmb_scene_pick(16, 16));

#undef P5K_SCENE
#undef P5K_CHECK
    printf("P5-KEY ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

static int run_p5_lod(void)
{
    int ok = 0, fail = 0;
#define P5L_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-LOD fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

    /* Oggetto 1: due quad 10×20 affiancati a z=60 (quattro triangoli) — il pixel
     * 16 è il centro, il 12 cade a ~+8,6 unità e sta nel quad di destra. Le
     * geometrie sostitutive stanno a z=400, OLTRE il far plane (300): fuori dal
     * frustum, invisibili per conto loro — l'unico modo di vederle è che il LOD
     * le disegni al posto dell'oggetto 1. Conteggi distinti: 4 triangoli la
     * piena, 2 la mesh, 4 quella della catena (per smascherare un secondo
     * livello applicato). */
#define P5L_SCENE() do { \
        P5L_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET"); \
        P5L_CHECK(gmb_scene_view(32, 32, 60.0) == GMB_3D_OK, "VIEW"); \
        P5L_CHECK(gmb_scene_camera(0.0, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "CAMERA"); \
        P5L_CHECK(gmb_scene_pick_enable(1) == GMB_3D_OK, "PICK on"); \
        P5L_CHECK(gmb_scene_object(255, 0, 0) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, -10.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, -10.0, 0.0, 0.5, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, 10.0, 0.0, 0.5, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, 10.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, -10.0, 0.0, 0.5, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, -10.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, 10.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, 10.0, 0.0, 0.5, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(4, 7, 6, 5) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 60.0, 0.0) == GMB_3D_OK, "oggetto 1 (due quad a z=60)"); \
        P5L_CHECK(gmb_scene_object(0, 255, 0) == GMB_3D_OK \
                  && gmb_scene_vertex(-6.0, -6.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(6.0, -6.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(6.0, 6.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-6.0, 6.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 400.0, 0.0) == GMB_3D_OK, "oggetto 2 (quad piccolo)"); \
        P5L_CHECK(gmb_scene_object(0, 0, 255) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, -10.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, -10.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, 10.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, 10.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK \
                  && gmb_scene_vertex(-6.0, -6.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(6.0, -6.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(6.0, 6.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-6.0, 6.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(4, 7, 6, 5) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 400.0, 0.0) == GMB_3D_OK, "oggetto 3 (due quad)"); \
    } while (0)

    /* Argomenti: indici in 1..nobj, distanza >= 0 e finita. */
    P5L_SCENE();
    P5L_CHECK(gmb_scene_lod(0, 10.0, 2) == GMB_3D_E_ARG
              && gmb_scene_lod(4, 10.0, 2) == GMB_3D_E_ARG,
              "LOD su oggetto 0 o 4 (esistono 1..3) non rifiutato");
    P5L_CHECK(gmb_scene_lod(1, 10.0, 0) == GMB_3D_E_ARG
              && gmb_scene_lod(1, 10.0, 4) == GMB_3D_E_ARG,
              "LOD con mesh 0 o 4 non rifiutato");
    P5L_CHECK(gmb_scene_lod(1, -1.0, 2) == GMB_3D_E_ARG
              && gmb_scene_lod(1, 2.0e9, 2) == GMB_3D_E_ARG,
              "LOD con distanza negativa o fuori scala non rifiutato");
    P5L_CHECK(gmb_3d_last_error()[0] != 0, "errore di LOD senza messaggio");

    /* Senza LOD vale la geometria dichiarata: quattro triangoli, pixel 16 e 12
     * coperti dall'oggetto 1. */
    P5L_SCENE();
    P5L_CHECK(gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 4 && gmb_scene_pick(16, 16) == 1
              && gmb_scene_pick(12, 16) == 1,
              "senza LOD: draw=%lld pick16=%lld pick12=%lld (attesi 4/1/1)",
              (long long)gmb_scene_drawn(), (long long)gmb_scene_pick(16, 16),
              (long long)gmb_scene_pick(12, 16));

    /* Sotto la soglia (distanza 60 < 100) resta la geometria piena. */
    P5L_CHECK(gmb_scene_lod(1, 100.0, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 4,
              "LOD sotto soglia: draw=%lld (atteso 4)", (long long)gmb_scene_drawn());
    /* «Oltre» è stretto: alla distanza esatta (60) vince ancora la piena. */
    P5L_CHECK(gmb_scene_lod(1, 60.0, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 4,
              "soglia esatta: draw=%lld (atteso 4)", (long long)gmb_scene_drawn());

    /* Sopra la soglia si disegna la geometria della mesh, con l'identità di 1:
     * due triangoli, il pixel 16 resta coperto dall'oggetto 1, il 12 no. */
    P5L_CHECK(gmb_scene_lod(1, 30.0, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 2 && gmb_scene_pick(16, 16) == 1
              && gmb_scene_pick(12, 16) == 0,
              "sopra soglia: draw=%lld pick16=%lld pick12=%lld (attesi 2/1/0)",
              (long long)gmb_scene_drawn(), (long long)gmb_scene_pick(16, 16),
              (long long)gmb_scene_pick(12, 16));
    /* dist = 0: sempre la mesh (l'occhio non sta nell'origine dell'oggetto). */
    P5L_CHECK(gmb_scene_lod(1, 0.0, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 2,
              "dist=0: draw=%lld (atteso 2)", (long long)gmb_scene_drawn());
    /* mesh == o: nessuna sostituzione e nessun errore. */
    P5L_CHECK(gmb_scene_lod(1, 100.0, 1) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 4,
              "mesh == o: draw=%lld (atteso 4)", (long long)gmb_scene_drawn());

    /* UN livello: la mesh 2 ha il suo LOD verso 3, ma quando fa da sostituta non
     * lo applica — altrimenti si vedrebbero i due quad di 3 (quattro triangoli). */
    P5L_CHECK(gmb_scene_lod(2, 0.0, 3) == GMB_3D_OK
              && gmb_scene_lod(1, 0.0, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 2,
              "catena di LOD applicata: draw=%lld (atteso 2, un livello solo)",
              (long long)gmb_scene_drawn());

    /* I raggi usano la geometria DICHIARATA di 1: il quad è un piano a z=60. */
    {
        long long hit = (long long)gmb_scene_shoot(300.0);
        double dist = gmb_scene_shotdist();
        P5L_CHECK(hit == 1 && dist == 60.0,
                  "SHOOT con LOD attivo: obj=%lld dist=%g (attesi 1/60)", hit, dist);
    }

    /* Zero allocazioni: il LOD sono due campi nell'oggetto, letti dal frame. */
    {
        unsigned long a0 = gmb_3d_alloc_count();
        P5L_CHECK(gmb_scene_render() == GMB_3D_OK && gmb_3d_alloc_count() == a0,
                  "RENDER con LOD ha allocato: %lu", gmb_3d_alloc_count() - a0);
    }

    /* RESET azzera anche il LOD: la stessa scena, senza SCENE LOD, torna piena. */
    P5L_SCENE();
    P5L_CHECK(gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 4,
              "dopo RESET il LOD è rimasto attivo: draw=%lld (atteso 4)",
              (long long)gmb_scene_drawn());

#undef P5L_SCENE
#undef P5L_CHECK
    printf("P5-LOD ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

static int run_p5_inst(void)
{
    int ok = 0, fail = 0;
#define P5I_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-INST fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

    /* Oggetto 1: due quad 10×20 a z=60 (quattro triangoli), tinta blu. Oggetto 2:
     * un quad piccolo ±6 a z=400 — oltre il far plane (300), invisibile da solo —
     * tinta verde e luce base 128. Oggetto 3 e 4: ospiti SENZA geometria, a z=50
     * e z=40: l'unico modo di vederli è che disegnino la geometria di un altro.
     * La tinta del pixel distingue il materiale: 0x800000 = rosso 255 con luce
     * 128 (della sorgente), 0xFF0000 = luce 255 (dell'oggetto). */
#define P5I_SCENE() do { \
        P5I_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET"); \
        P5I_CHECK(gmb_scene_view(32, 32, 60.0) == GMB_3D_OK, "VIEW"); \
        P5I_CHECK(gmb_scene_camera(0.0, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "CAMERA"); \
        P5I_CHECK(gmb_scene_pick_enable(1) == GMB_3D_OK, "PICK on"); \
        P5I_CHECK(gmb_scene_object(0, 0, 255) == GMB_3D_OK \
                  && gmb_scene_vertex(-15.0, -10.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, -10.0, 0.0, 0.5, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, 10.0, 0.0, 0.5, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-15.0, 10.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, -10.0, 0.0, 0.5, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(15.0, -10.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(15.0, 10.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(0.0, 10.0, 0.0, 0.5, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(4, 7, 6, 5) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 60.0, 0.0) == GMB_3D_OK, "oggetto 1"); \
        P5I_CHECK(gmb_scene_object_flags(0, 255, 0, 0, 128) == GMB_3D_OK \
                  && gmb_scene_vertex(-6.0, -6.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(6.0, -6.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(6.0, 6.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-6.0, 6.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 400.0, 0.0) == GMB_3D_OK, "oggetto 2 (mesh lontana)"); \
        P5I_CHECK(gmb_scene_object(255, 0, 0) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 50.0, 0.0) == GMB_3D_OK, "oggetto 3 (ospite)"); \
        P5I_CHECK(gmb_scene_object(255, 255, 255) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 40.0, 0.0) == GMB_3D_OK, "oggetto 4 (ospite)"); \
        P5I_CHECK(gmb_scene_object(255, 255, 0) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 250.0, 0.0) == GMB_3D_OK, "oggetto 5 (ospite lontano)"); \
        P5I_CHECK(gmb_scene_object(0, 255, 255) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 400.0, 0.0) == GMB_3D_OK, "oggetto 6 (ospite oltre il far)"); \
    } while (0)

    /* Argomenti: oggetto e sorgente in 1..nobj (esistono 1..4). */
    P5I_SCENE();
    P5I_CHECK(gmb_scene_instance(0, 2) == GMB_3D_E_ARG
              && gmb_scene_instance(7, 2) == GMB_3D_E_ARG,
              "INSTANCE su oggetto 0 o 7 (esistono 1..6) non rifiutato");
    P5I_CHECK(gmb_scene_instance(3, 0) == GMB_3D_E_ARG
              && gmb_scene_instance(3, 7) == GMB_3D_E_ARG,
              "INSTANCE con sorgente 0 o 7 non rifiutato");
    P5I_CHECK(gmb_3d_last_error()[0] != 0, "errore di INSTANCE senza messaggio");

    /* Senza istanza gli ospiti non disegnano niente: restano i quattro
     * triangoli dell'oggetto 1, che copre il pixel 16 e il 12. */
    P5I_SCENE();
    P5I_CHECK(gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 4 && gmb_scene_pick(16, 16) == 1
              && gmb_scene_pick(12, 16) == 1,
              "senza istanza: draw=%lld pick16=%lld pick12=%lld (attesi 4/1/1)",
              (long long)gmb_scene_drawn(), (long long)gmb_scene_pick(16, 16),
              (long long)gmb_scene_pick(12, 16));

    /* Istanza: geometria della sorgente (2 triangoli), trasformazione
     * dell'oggetto 3 (z=50, davanti all'1) e identità di 3 in PICK. Il materiale
     * è quello della sorgente: tinta rossa (di 3) con la luce 128 (di 2). */
    P5I_CHECK(gmb_scene_instance(3, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 6 && gmb_scene_pick(16, 16) == 3
              && gmb_scene_pick(10, 16) == 1
              && gmb_scene_pixel(16, 16) == 0x800000
              && gmb_scene_pixel(10, 16) == 0x0000FF,
              "istanza: draw=%lld pick16=%lld pick10=%lld px16=%06llx px10=%06llx (attesi 6/3/1/800000/0000ff)",
              (long long)gmb_scene_drawn(), (long long)gmb_scene_pick(16, 16),
              (long long)gmb_scene_pick(10, 16),
              (long long)(unsigned)gmb_scene_pixel(16, 16),
              (long long)(unsigned)gmb_scene_pixel(10, 16));

    /* L'istanza non ha soglia di distanza: a z=250 (dentro il far plane) la
     * geometria della sorgente si disegna ancora, senza dichiarare distanze. */
    P5I_SCENE();
    P5I_CHECK(gmb_scene_instance(5, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 6,
              "istanza lontana: draw=%lld (atteso 6)", (long long)gmb_scene_drawn());

    /* Il culling guarda la geometria DISEGNATA con il modello dell'oggetto:
     * con l'ospite oltre il far plane (400) l'istanza è scartata come la
     * sorgente, e resta solo l'oggetto 1. */
    P5I_SCENE();
    P5I_CHECK(gmb_scene_instance(6, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 4,
              "istanza oltre il far: draw=%lld (atteso 4)", (long long)gmb_scene_drawn());

    /* UN livello: l'istanza 4 di 3 (a sua volta istanza di 2) disegna la
     * geometria DICHIARATA di 3, che è vuota — non segue la catena. */
    P5I_SCENE();
    P5I_CHECK(gmb_scene_instance(3, 2) == GMB_3D_OK
              && gmb_scene_instance(4, 3) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 6 && gmb_scene_pick(16, 16) == 3,
              "catena di istanze: draw=%lld pick16=%lld (attesi 6/3)",
              (long long)gmb_scene_drawn(), (long long)gmb_scene_pick(16, 16));
    /* La seconda istanza punta alla sorgente vera: due triangoli in più, e
     * l'oggetto 4 (z=40) è davanti a tutti. */
    P5I_CHECK(gmb_scene_instance(4, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 8 && gmb_scene_pick(16, 16) == 4,
              "due istanze: draw=%lld pick16=%lld (attesi 8/4)",
              (long long)gmb_scene_drawn(), (long long)gmb_scene_pick(16, 16));
    /* `src == o` riporta l'oggetto alla propria geometria (ospite vuoto). */
    P5I_CHECK(gmb_scene_instance(4, 4) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 6 && gmb_scene_pick(16, 16) == 3,
              "INSTANCE 4, 4: draw=%lld pick16=%lld (attesi 6/3)",
              (long long)gmb_scene_drawn(), (long long)gmb_scene_pick(16, 16));

    /* INSTANCE e LOD si contendono la stessa scelta: dopo LOD l'oggetto 3
     * disegna i triangoli di 2 ma con il PROPRIO materiale (luce 255, 0xFF0000);
     * l'INSTANCE successiva riporta il materiale della sorgente (0x800000). */
    P5I_SCENE();
    P5I_CHECK(gmb_scene_lod(3, 0.0, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 6 && gmb_scene_pixel(16, 16) == 0xFF0000,
              "LOD dopo INSTANCE: draw=%lld px16=%06llx (attesi 6/ff0000)",
              (long long)gmb_scene_drawn(),
              (long long)(unsigned)gmb_scene_pixel(16, 16));
    P5I_CHECK(gmb_scene_instance(3, 2) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_drawn() == 6 && gmb_scene_pixel(16, 16) == 0x800000,
              "INSTANCE dopo LOD: draw=%lld px16=%06llx (attesi 6/800000)",
              (long long)gmb_scene_drawn(),
              (long long)(unsigned)gmb_scene_pixel(16, 16));

    /* L'oggetto che ospita l'istanza non entra nell'AABB mondo (non ha
     * geometria): il colpo passa e prende l'oggetto 1 a z=60, e sotto l'ospite
     * non c'è nessuna superficie. */
    {
        long long hit = (long long)gmb_scene_shoot(300.0);
        P5I_CHECK(hit == 1 && gmb_scene_shotdist() == 60.0,
                  "SHOOT con istanza: obj=%lld dist=%g (attesi 1/60)", hit, gmb_scene_shotdist());
        P5I_CHECK(gmb_scene_floor_at(0.0, 100.0, 50.0) == -1000000.0,
                  "l'ospite dell'istanza è diventato un ostacolo: FLOORAT=%g",
                  gmb_scene_floor_at(0.0, 100.0, 50.0));
    }

    /* Il materiale viene dalla sorgente anche nei FLAG: un quad con avvolgimento
     * rovescio è scartato con i flag di 1 (nessun due lati) e disegnato con i
     * flag 16 di 3. L'ospite 2 resta lo stesso. */
    P5I_CHECK(gmb_scene_reset() == GMB_3D_OK
              && gmb_scene_view(32, 32, 60.0) == GMB_3D_OK
              && gmb_scene_camera(0.0, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "scena dei flag");
    P5I_CHECK(gmb_scene_object_flags(0, 0, 255, 0, 255) == GMB_3D_OK
              && gmb_scene_vertex(-6.0, -6.0, 0.0, 0.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(6.0, -6.0, 0.0, 1.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(6.0, 6.0, 0.0, 1.0, 0.0) == GMB_3D_OK
              && gmb_scene_vertex(-6.0, 6.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_quad(0, 1, 2, 3) == GMB_3D_OK
              && gmb_scene_objpos(0.0, 0.0, 400.0, 0.0) == GMB_3D_OK, "sorgente a un lato");
    P5I_CHECK(gmb_scene_object(255, 0, 0) == GMB_3D_OK
              && gmb_scene_objpos(0.0, 0.0, 50.0, 0.0) == GMB_3D_OK, "ospite dei flag");
    P5I_CHECK(gmb_scene_object_flags(0, 255, 0, 16, 255) == GMB_3D_OK
              && gmb_scene_vertex(-6.0, -6.0, 0.0, 0.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(6.0, -6.0, 0.0, 1.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(6.0, 6.0, 0.0, 1.0, 0.0) == GMB_3D_OK
              && gmb_scene_vertex(-6.0, 6.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_quad(0, 1, 2, 3) == GMB_3D_OK
              && gmb_scene_objpos(0.0, 0.0, 400.0, 0.0) == GMB_3D_OK, "sorgente a due lati");
    P5I_CHECK(gmb_scene_instance(2, 1) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 0,
              "flag della sorgente ignorati (due lati spento): draw=%lld",
              (long long)gmb_scene_drawn());
    P5I_CHECK(gmb_scene_instance(2, 3) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 2,
              "flag due lati della sorgente non applicati: draw=%lld",
              (long long)gmb_scene_drawn());

    /* Zero allocazioni: l'istanza è un intero nell'oggetto, letto dal frame. */
    {
        unsigned long a0 = gmb_3d_alloc_count();
        P5I_CHECK(gmb_scene_render() == GMB_3D_OK && gmb_3d_alloc_count() == a0,
                  "RENDER con istanze ha allocato: %lu", gmb_3d_alloc_count() - a0);
    }

    /* RESET azzera anche le istanze. */
    P5I_SCENE();
    P5I_CHECK(gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 4,
              "dopo RESET l'istanza è rimasta attiva: draw=%lld (atteso 4)",
              (long long)gmb_scene_drawn());

#undef P5I_SCENE
#undef P5I_CHECK
    printf("P5-INST ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

/* P5/M-SKIN — skinning a 64 ossa. Un quad ±10 a z=60 in un 64×64 con fov 60:
 * mezzo lato verticale = 60·tan30° = 34,64 unità, quindi 1 unità = 0,924 px e il
 * quad copre i pixel 23..41. Una posa +10 in x lo sposta di 9,2 px (32..50),
 * +20 di 18,5 (41..59). Il fondo è 0x0f1729. */
/* I quattro vertici del quad (oggetto 1) allo stesso osso/peso: una posa muove
 * la figura intera, non un angolo solo. */
static int p5s_quad_weight(int bone, double w)
{
    for (long long v = 0; v < 4; v++)
        if (gmb_scene_weight(1, v, bone, w) != GMB_3D_OK) return 0;
    return 1;
}

static int run_p5_skin(void)
{
    int ok = 0, fail = 0;
#define P5S_CHECK(cond, ...) do { if (cond) ok++; else { fail++; printf("P5-SKIN fail: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define P5S_BG 0x0f1729

#define P5S_SCENE() do { \
        P5S_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET"); \
        P5S_CHECK(gmb_scene_view(64, 64, 60.0) == GMB_3D_OK, "VIEW"); \
        P5S_CHECK(gmb_scene_camera(0.0, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK, "CAMERA"); \
        P5S_CHECK(gmb_scene_pick_enable(1) == GMB_3D_OK, "PICK on"); \
        P5S_CHECK(gmb_scene_object(255, 255, 255) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, -10.0, 0.0, 0.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, -10.0, 0.0, 1.0, 1.0) == GMB_3D_OK \
                  && gmb_scene_vertex(10.0, 10.0, 0.0, 1.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_vertex(-10.0, 10.0, 0.0, 0.0, 0.0) == GMB_3D_OK \
                  && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK \
                  && gmb_scene_objpos(0.0, 0.0, 60.0, 0.0) == GMB_3D_OK, "oggetto 1 (quad)"); \
    } while (0)

    /* Riferimento: geometria dichiarata. */
    P5S_SCENE();
    P5S_CHECK(gmb_scene_render() == GMB_3D_OK && gmb_scene_drawn() == 2
              && gmb_scene_pixel(32, 32) == 0xFFFFFF && gmb_scene_pick(32, 32) == 1
              && gmb_scene_pixel(44, 32) == P5S_BG,
              "base: draw=%lld px32=%06llx pick=%lld px44=%06llx",
              (long long)gmb_scene_drawn(), (long long)(unsigned)gmb_scene_pixel(32, 32),
              (long long)gmb_scene_pick(32, 32), (long long)(unsigned)gmb_scene_pixel(44, 32));
    long long base_hash = (long long)gmb_scene_hash();

    /* Le ossa senza pesi non cambiano un byte: il percorso skinnato non esiste
     * finché WEIGHT non lega un vertice (M-SKIN-ALLOC). */
    P5S_CHECK(gmb_scene_bones(2) == GMB_3D_OK, "BONES 2");
    P5S_CHECK(gmb_scene_render() == GMB_3D_OK && (long long)gmb_scene_hash() == base_hash,
              "BONES senza pesi ha cambiato il frame");
    P5S_CHECK(gmb_scene_bones(65) == GMB_3D_E_ARG && gmb_scene_bones(-1) == GMB_3D_E_ARG,
              "BONES fuori 0..64 non rifiutato");
    P5S_CHECK(gmb_scene_bones(2) == GMB_3D_OK, "BONES 2 (ripristino)");

    /* Argomenti: osso 1..BONES, oggetto 1..nobj, vertice 0..nvert-1. */
    P5S_CHECK(gmb_scene_posebone(0, 0, 0, 0, 0) == GMB_3D_E_ARG
              && gmb_scene_posebone(3, 0, 0, 0, 0) == GMB_3D_E_ARG,
              "POSEBONE con osso 0 o 3 (esistono 1..2) non rifiutato");
    P5S_CHECK(gmb_scene_weight(0, 0, 1, 1.0) == GMB_3D_E_ARG
              && gmb_scene_weight(2, 0, 1, 1.0) == GMB_3D_E_ARG,
              "WEIGHT con oggetto 0 o 2 (esiste 1) non rifiutato");
    P5S_CHECK(gmb_scene_weight(1, -1, 1, 1.0) == GMB_3D_E_ARG
              && gmb_scene_weight(1, 4, 1, 1.0) == GMB_3D_E_ARG,
              "WEIGHT con vertice -1 o 4 (esistono 0..3) non rifiutato");
    P5S_CHECK(gmb_scene_weight(1, 0, 0, 1.0) == GMB_3D_E_ARG
              && gmb_scene_weight(1, 0, 3, 1.0) == GMB_3D_E_ARG,
              "WEIGHT con osso 0 o 3 non rifiutato");
    P5S_CHECK(gmb_3d_last_error()[0] != 0, "errore di WEIGHT senza messaggio");

    /* Peso 0 e posa identità: il frame resta quello di sempre, bit per bit. */
    P5S_CHECK(p5s_quad_weight(1, 0.0)
              && gmb_scene_render() == GMB_3D_OK && (long long)gmb_scene_hash() == base_hash,
              "peso 0 ha mosso il vertice");
    P5S_CHECK(p5s_quad_weight(1, 1.0)
              && gmb_scene_render() == GMB_3D_OK && (long long)gmb_scene_hash() == base_hash
              && gmb_scene_drawn() == 2,
              "posa identità diversa dalla geometria dichiarata");

    /* Posa +10 in x: il quad si sposta di 9,2 px. La camera guarda +z con up +y,
     * quindi +x va a SINISTRA sullo schermo (copre 14..32). */
    P5S_CHECK(gmb_scene_posebone(1, 10.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pixel(24, 32) == 0xFFFFFF && gmb_scene_pick(24, 32) == 1
              && gmb_scene_pixel(45, 32) == P5S_BG,
              "posa +10: px24=%06llx px45=%06llx",
              (long long)(unsigned)gmb_scene_pixel(24, 32),
              (long long)(unsigned)gmb_scene_pixel(45, 32));

    /* Tornare alla posa identità riporta il frame di riferimento. */
    P5S_CHECK(gmb_scene_posebone(1, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && (long long)gmb_scene_hash() == base_hash,
              "POSEBONE identità non torna al frame base");

    /* Due influenze 1:1 fra l'identità e +20 → punto medio +10. */
    P5S_CHECK(gmb_scene_bones(2) == GMB_3D_OK, "BONES 2 per il blend");
    P5S_CHECK(p5s_quad_weight(1, 1.0) && p5s_quad_weight(2, 1.0),
              "due influenze sugli stessi vertici");
    P5S_CHECK(gmb_scene_posebone(2, 20.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pixel(24, 32) == 0xFFFFFF && gmb_scene_pixel(45, 32) == P5S_BG,
              "blend 1:1: px24=%06llx px45=%06llx (atteso il punto medio +10)",
              (long long)(unsigned)gmb_scene_pixel(24, 32),
              (long long)(unsigned)gmb_scene_pixel(45, 32));
    /* Un peso fuori 0..1 si clampa: 3 vale 1, quindi il blend resta 1:1. */
    P5S_CHECK(p5s_quad_weight(1, 3.0)
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pixel(24, 32) == 0xFFFFFF && gmb_scene_pixel(45, 32) == P5S_BG,
              "peso 3 non clampato a 1: px24=%06llx px45=%06llx",
              (long long)(unsigned)gmb_scene_pixel(24, 32),
              (long long)(unsigned)gmb_scene_pixel(45, 32));

    /* Una sola influenza con peso 0,25 vale la matrice intera (normalizzazione). */
    P5S_SCENE();
    P5S_CHECK(gmb_scene_bones(2) == GMB_3D_OK, "BONES 2 (peso singolo)");
    P5S_CHECK(gmb_scene_posebone(2, 20.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && p5s_quad_weight(2, 0.25)
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pixel(10, 32) == 0xFFFFFF && gmb_scene_pixel(45, 32) == P5S_BG,
              "peso singolo 0,25: px10=%06llx px45=%06llx (atteso +20 pieno)",
              (long long)(unsigned)gmb_scene_pixel(10, 32),
              (long long)(unsigned)gmb_scene_pixel(45, 32));

    /* Terza influenza: errore esplicito, non una caduta silenziosa. */
    P5S_CHECK(gmb_scene_bones(3) == GMB_3D_OK
              && gmb_scene_weight(1, 0, 1, 1.0) == GMB_3D_OK
              && gmb_scene_weight(1, 0, 2, 1.0) == GMB_3D_OK
              && gmb_scene_weight(1, 0, 3, 1.0) == GMB_3D_E_ARG,
              "terza influenza non rifiutata");

    /* La posa NON sposta il mondo: AABB, raggi e quote restano i dichiarati. */
    P5S_SCENE();
    P5S_CHECK(gmb_scene_bones(1) == GMB_3D_OK
              && gmb_scene_posebone(1, 20.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && p5s_quad_weight(1, 1.0)
              && gmb_scene_render() == GMB_3D_OK, "scena della posa estrema");
    {
        long long hit = (long long)gmb_scene_shoot(300.0);
        P5S_CHECK(hit == 1 && gmb_scene_shotdist() == 60.0,
                  "SHOOT con skin: obj=%lld dist=%g (attesi 1/60)", hit, gmb_scene_shotdist());
        P5S_CHECK(gmb_scene_floor_at(0.0, 100.0, 60.0) == 10.0,
                  "FLOORAT con skin: %g (atteso 10, AABB dichiarato)",
                  gmb_scene_floor_at(0.0, 100.0, 60.0));
    }

    /* Zero allocazioni per frame, anche con pesi e pose. */
    {
        unsigned long a0 = gmb_3d_alloc_count();
        P5S_CHECK(gmb_scene_render() == GMB_3D_OK && gmb_3d_alloc_count() == a0,
                  "RENDER con skin ha allocato: %lu", gmb_3d_alloc_count() - a0);
    }

    /* ---- P5/M-SKIN-KEY: piste di chiavi degli ossi (`SCENE ANIM`) ---------- */
    P5S_SCENE();
    P5S_CHECK(gmb_scene_bones(1) == GMB_3D_OK && p5s_quad_weight(1, 1.0),
              "scena delle chiavi");
    P5S_CHECK(gmb_scene_anim(0, 0.0, 0, 0, 0, 0) == GMB_3D_E_ARG
              && gmb_scene_anim(2, 0.0, 0, 0, 0, 0) == GMB_3D_E_ARG,
              "ANIM con osso 0 o 2 (esiste 1) non rifiutato");
    P5S_CHECK(gmb_scene_anim(1, -1.0, 0, 0, 0, 0) == GMB_3D_E_ARG,
              "ANIM con tempo negativo non rifiutato");
    /* Identità a 0 s, +20 a 1 s: a metà tempo la posa è +10 (come la posa
     * data a mano, perché la chiave SCRIVE la posa dell'osso). */
    P5S_CHECK(gmb_scene_anim(1, 0.0, 0, 0, 0, 0) == GMB_3D_OK
              && gmb_scene_anim_trs(1, 1.0, 20, 0, 0, 0, 0, 0) == GMB_3D_OK,
              "dichiarazione delle due chiavi (forma corta e TRS)");
    P5S_CHECK(gmb_scene_time(0.5) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pixel(24, 32) == 0xFFFFFF && gmb_scene_pixel(45, 32) == P5S_BG,
              "chiavi a meta' tempo: px24=%06llx px45=%06llx (atteso +10)",
              (long long)(unsigned)gmb_scene_pixel(24, 32),
              (long long)(unsigned)gmb_scene_pixel(45, 32));
    /* Clamp: prima della prima chiave vale la prima, dopo l'ultima l'ultima. */
    P5S_CHECK(gmb_scene_time(0.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && (long long)gmb_scene_hash() == base_hash,
              "la chiave a 0 s non e' l'identita'");
    P5S_CHECK(gmb_scene_time(2.0) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pixel(10, 32) == 0xFFFFFF && gmb_scene_pixel(45, 32) == P5S_BG,
              "clamp dopo l'ultima chiave: px10=%06llx px45=%06llx (atteso +20)",
              (long long)(unsigned)gmb_scene_pixel(10, 32),
              (long long)(unsigned)gmb_scene_pixel(45, 32));
    /* La pista vince sul POSEBONE: stesso stato, campionato per ultimo. */
    P5S_CHECK(gmb_scene_posebone(1, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK
              && gmb_scene_pixel(10, 32) == 0xFFFFFF && gmb_scene_pixel(32, 32) == P5S_BG,
              "POSEBONE ha battuto la pista dell'osso");
    /* Una chiave al tempo dell'ultima la sostituisce: la forma lunga è la stessa. */
    P5S_CHECK(gmb_scene_anim_trs(1, 2.0, 0, 0, 0, 0, 0, 0) == GMB_3D_OK
              && gmb_scene_render() == GMB_3D_OK && (long long)gmb_scene_hash() == base_hash,
              "chiave TRS a 2 s non riporta la posa all'identita'");
    /* Tetto dichiarato: 32 chiavi per osso, la trentatreesima è un errore.
     * Da schema pulito: BONES svuota le tre chiavi appena dichiarate (e i pesi). */
    {
        int ok32 = (gmb_scene_bones(1) == GMB_3D_OK);
        for (int i = 0; i < 32; i++)
            if (gmb_scene_anim(1, 10.0 + (double)i, 0, 0, 0, 0) != GMB_3D_OK) ok32 = 0;
        P5S_CHECK(ok32 && gmb_scene_anim(1, 99.0, 0, 0, 0, 0) == GMB_3D_E_ARG,
                  "il tetto di 32 chiavi per osso non e' applicato");
    }

    /* BONES azzera i pesi (e le piste): cambiare scheletro non lascia appeso
     * niente di riferito alle ossa precedenti. */
    P5S_CHECK(gmb_scene_bones(1) == GMB_3D_OK && gmb_scene_render() == GMB_3D_OK
              && (long long)gmb_scene_hash() == base_hash,
              "BONES non ha azzerato i pesi");
    /* RESET azzera ossa, pose e pesi: la stessa scena ridichiarata senza comandi
     * di skin torna al frame di riferimento (se un peso fosse rimasto, il quad
     * del vertice 0 sarebbe ancora spostato). */
    P5S_CHECK(gmb_scene_reset() == GMB_3D_OK, "RESET");
    P5S_CHECK(gmb_scene_view(64, 64, 60.0) == GMB_3D_OK
              && gmb_scene_camera(0.0, 0.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_pick_enable(1) == GMB_3D_OK
              && gmb_scene_object(255, 255, 255) == GMB_3D_OK
              && gmb_scene_vertex(-10.0, -10.0, 0.0, 0.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(10.0, -10.0, 0.0, 1.0, 1.0) == GMB_3D_OK
              && gmb_scene_vertex(10.0, 10.0, 0.0, 1.0, 0.0) == GMB_3D_OK
              && gmb_scene_vertex(-10.0, 10.0, 0.0, 0.0, 0.0) == GMB_3D_OK
              && gmb_scene_quad(0, 3, 2, 1) == GMB_3D_OK
              && gmb_scene_objpos(0.0, 0.0, 60.0, 0.0) == GMB_3D_OK,
              "scena ridichiarata dopo RESET");
    P5S_CHECK(gmb_scene_render() == GMB_3D_OK && (long long)gmb_scene_hash() == base_hash,
              "dopo RESET lo skin e' rimasto attivo");

#undef P5S_SCENE
#undef P5S_CHECK
#undef P5S_BG
    printf("P5-SKIN ok=%d fail=%d\n", ok, fail);
    return fail == 0 ? 0 : 1;
}

static int cmp_double(const void* a, const void* b)
{
    double x = *(const double*)a, y = *(const double*)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/* Banco della scena (P5, accettazione «N oggetti a >= 30 fps»): N box in griglia
 * davanti alla camera, FRAMES render misurati a orologio monotono. Il numero che
 * conta è la MEDIANA, non una stima: la macchina è condivisa (piano §0septies). */
static int p5_scene_bench(int n, int w, int h, int frames, int grid)
{
    if (n <= 0 || n > 65536 || w <= 0 || h <= 0 || frames < 1 || frames > 100000)
    {
        printf("BENCH-SCENE uso: --objects 1..65536 | --tris 1..786432, --size WxH, --frames N\n");
        return 2;
    }
    if (gmb_scene_reset() != GMB_3D_OK || gmb_scene_view(w, h, 60.0) != GMB_3D_OK)
    {
        printf("BENCH-SCENE errore: %s\n", gmb_3d_last_error());
        return 2;
    }

    /* Due disposizioni, per misurare due cose diverse:
     *  - `--objects N` (griglia 25 colonne davanti alla camera): il banco storico
     *    del primo blocco di P5, 12 triangoli per oggetto, numeri confrontabili;
     *  - `--tris N` (griglia QUADRATA larga tutta la camera): serve a riempire il
     *    quadro con ~N triangoli senza finire fuori dal frustum, che è il modo
     *    onesto di misurare l'accettazione dei 200k (con 25 colonne e 16k oggetti
     *    si uscirebbe dal campo visivo e si misurerebbe il culling, non il raster). */
    const int cols = grid ? (int)ceil(sqrt((double)n)) : 25;
    const int rows = (n + cols - 1) / cols;
    for (int i = 0; i < n; i++)
    {
        double x, y, z;
        if (grid)
        {
            x = (double)(i % cols) - (cols - 1) * 0.5;
            y = (double)(i / cols) - (rows - 1) * 0.5;
            z = 0.0;
        }
        else
        {
            x = ((double)(i % cols) - (cols - 1) * 0.5) * 1.2;
            y = ((double)(i / cols) - (rows - 1) * 0.5) * 1.2 + 4.0;
            z = 2.0 + (double)(i % 7) * 0.5;
        }
        if (gmb_scene_object(180, 190, 210) != GMB_3D_OK
            || gmb_scene_box(-0.45, -0.45, -0.45, 0.45, 0.45, 0.45) != GMB_3D_OK
            || gmb_scene_objpos(x, y, z, grid ? 0.0 : 25.0) != GMB_3D_OK)
        {
            printf("BENCH-SCENE errore: %s\n", gmb_3d_last_error());
            return 2;
        }
    }
    if (grid ? (gmb_scene_camera(0.0, 0.0, -120.0, 0.0, 0.0) != GMB_3D_OK)
             : (gmb_scene_camera(0.0, 4.0, -24.0, 0.0, -6.0) != GMB_3D_OK))
    {
        printf("BENCH-SCENE errore: %s\n", gmb_3d_last_error());
        return 2;
    }

    /* Nessuna allocazione per frame: si misura col contatore, non a occhio. La
     * geometria (buffer dinamici) è già riservata: un RENDER che allocasse — un
     * buffer nuovo, un realloc — fa fallire il banco. */
    unsigned long alloc0 = gmb_3d_alloc_count();
    for (int i = 0; i < 5; i++)                    /* warm-up: cache e target caldi */
        if (gmb_scene_render() != GMB_3D_OK) { printf("BENCH-SCENE render: %s\n", gmb_3d_last_error()); return 2; }

    double* ms = (double*)malloc((size_t)frames * sizeof(double));
    if (ms == NULL) return 2;
    for (int i = 0; i < frames; i++)
    {
        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        if (gmb_scene_render() != GMB_3D_OK) { free(ms); printf("BENCH-SCENE render: %s\n", gmb_3d_last_error()); return 2; }
        clock_gettime(CLOCK_MONOTONIC, &b);
        ms[i] = (double)(b.tv_sec - a.tv_sec) * 1000.0 + (double)(b.tv_nsec - a.tv_nsec) / 1e6;
    }

    unsigned long alloc_n = gmb_3d_alloc_count() - alloc0;
    if (alloc_n != 0)
    {
        printf("BENCH-SCENE allocazioni nel frame: %lu (attese 0)\n", alloc_n);
        free(ms);
        return 1;
    }

    double sum = 0.0;
    for (int i = 0; i < frames; i++) sum += ms[i];
    qsort(ms, (size_t)frames, sizeof(double), cmp_double);
    double med = ms[frames / 2];
    printf("BENCH-SCENE objects=%d verts=%ld tris=%ld drawn=%ld culled=%ld w=%d h=%d frames=%d "
           "allocs=%lu min=%.3f med=%.3f mean=%.3f\n",
           n, gmb_scene_verts(), gmb_scene_tris(), gmb_scene_drawn(), gmb_scene_culled(),
           w, h, frames, alloc_n, ms[0], med, sum / (double)frames);
    free(ms);
    return 0;
}

/* Banco della DICHIARAZIONE di chiavi (M-KEY, metodo §0septies): `--objects N`
 * oggetti con `--keys K` chiavi ciascuno, dichiarate in ordine di oggetto e poi
 * di tempo — il caso normale di una timeline. Il numero da guardare è il costo
 * PER CHIAVE: se cresce con le chiavi già presenti, la dichiarazione è
 * quadratica. La scena si ricostruisce a ogni giro FUORI dal cronometro: si
 * misura solo il ciclo di SCENE KEY. Nessuna semantica nuova, solo misure. */
static int p5_key_bench(int nobj, int nkeys, int rounds)
{
    if (nobj <= 0 || nobj > 4096 || nkeys <= 0 || nkeys > 32 || rounds < 1 || rounds > 100
        || (long)nobj * nkeys > 65536)
    {
        printf("BENCH-KEY uso: --objects 1..4096, --keys 1..32 (objects*keys <= 65536), --rounds 1..100\n");
        return 2;
    }
    double* ms = (double*)malloc((size_t)rounds * sizeof(double));
    if (ms == NULL) return 2;

    for (int r = 0; r < rounds; r++)
    {
        /* Scena e oggetti fuori dal cronometro: RESET azzera anche le piste. */
        if (gmb_scene_reset() != GMB_3D_OK || gmb_scene_view(320, 180, 60.0) != GMB_3D_OK)
        { free(ms); printf("BENCH-KEY scena: %s\n", gmb_3d_last_error()); return 2; }
        for (int i = 0; i < nobj; i++)
        {
            double x = ((double)(i % 25) - 12.0) * 1.2;
            double y = ((double)(i / 25) - 4.0) * 1.2;
            if (gmb_scene_object(180, 190, 210) != GMB_3D_OK
                || gmb_scene_box(-0.45, -0.45, -0.45, 0.45, 0.45, 0.45) != GMB_3D_OK
                || gmb_scene_objpos(x, y, 4.0, 0.0) != GMB_3D_OK)
            { free(ms); printf("BENCH-KEY scena: %s\n", gmb_3d_last_error()); return 2; }
        }

        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < nobj; i++)
        {
            for (int k = 0; k < nkeys; k++)
            {
                if (gmb_scene_key(i + 1, (double)k * 0.1, 0.0, 1.0, 0.0, 5.0, 0.0, 0.0) != GMB_3D_OK)
                { free(ms); printf("BENCH-KEY chiave: %s\n", gmb_3d_last_error()); return 2; }
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        ms[r] = (double)(b.tv_sec - a.tv_sec) * 1000.0 + (double)(b.tv_nsec - a.tv_nsec) / 1e6;
    }

    double sum = 0.0;
    for (int r = 0; r < rounds; r++) sum += ms[r];
    qsort(ms, (size_t)rounds, sizeof(double), cmp_double);
    double med = ms[rounds / 2];
    double keys = (double)nobj * (double)nkeys;
    printf("BENCH-KEY objects=%d keys=%d total=%.0f rounds=%d min=%.3f med=%.3f mean=%.3f "
           "us_per_key=%.4f\n",
           nobj, nkeys, keys, rounds, ms[0], med, sum / (double)rounds,
           med * 1000.0 / keys);
    free(ms);
    return 0;
}

/* --- main ---------------------------------------------------------------- */

static void usage(void)
{
    printf("uso: gmb_3d_oracle --check DIR [--rid RID] [--tol PCT] [--perturb N]\n"
           "     gmb_3d_oracle --bless DIR [--rid RID]\n"
           "     gmb_3d_oracle --selftest DIR\n"
           "     gmb_3d_oracle --bench [--frames N]\n"
           "     gmb_3d_oracle --cullcheck\n"
           "     gmb_3d_oracle --interactcheck\n"
           "     gmb_3d_oracle --raycheck\n"
           "     gmb_3d_oracle --statscheck\n"
           "     gmb_3d_oracle --keycheck\n"
           "     gmb_3d_oracle --lodcheck\n"
           "     gmb_3d_oracle --bench-scene [--objects N | --tris N] [--size WxH] [--frames N]\n"
           "     gmb_3d_oracle --bench-key [--objects N] [--keys K] [--rounds R]\n"
           "  DIR   = cartella delle golden (<scena>.<rid>.png)\n"
           "  --demo MAPPA = mappa demo (.gmbscene): scena `demo`, T-* e bench\n"
           "  --bench = frame time della demo a 640x300 (limite 4 ms)\n"
           "  --cullcheck = culling per oggetto sulla superficie SCENE (P5)\n"
           "  --interactcheck = corpo, MOVE, quote e zone della superficie SCENE (P5)\n"
           "  --raycheck = SHOOT/USE/SEES e stato dell'ultimo colpo (P5, M-RAYS)\n"
           "  --statscheck = SCENE.STATS: contatori del frame e memoria riservata (P5)\n"
           "  --keycheck = SCENE KEY/TIME: piste TRS, clamp, tetto, pitch/roll (P5, M-KEY)\n"
           "  --lodcheck = SCENE LOD: soglie, mesh sostitutiva, identità, raggi (P5, M-LOD)\n"
           "  --instcheck = SCENE INSTANCE: geometria e materiale condivisi, identità (P5, M-INST)\n"
           "  --skincheck = SCENE BONES/POSEBONE/WEIGHT/ANIM: ossa, pesi, fusione, chiavi (P5, M-SKIN)\n"
           "  --bench-scene = N oggetti (default 500) a --size (default 640x360)\n"
           "  --tris N = N triangoli dichiarati (12 per box) in griglia frontale\n"
           "  --bench-key = tempo di dichiarazione di N oggetti x K chiavi (--keys, default 32)\n");
}

int main(int argc, char** argv)
{
    const char* mode = NULL;
    const char* dir = NULL;
    const char* rid = GMB_ORACLE_RID;
    const char* demo = NULL;
    double tol = 0.0;
    int bench = 0, frames = 120;
    int cullcheck = 0, interactcheck = 0, raycheck = 0, statscheck = 0, keycheck = 0, lodcheck = 0, instcheck = 0, skincheck = 0, bench_scene = 0, objects = 500, bw = 640, bh = 360;
    int bench_key = 0, keys = 32, rounds = 5;
    int tris_target = 0;   /* --tris N: 0 = griglia storica a oggetti */

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
        else if (!strcmp(argv[i], "--cullcheck")) cullcheck = 1;
        else if (!strcmp(argv[i], "--interactcheck")) interactcheck = 1;
        else if (!strcmp(argv[i], "--raycheck")) raycheck = 1;
        else if (!strcmp(argv[i], "--statscheck")) statscheck = 1;
        else if (!strcmp(argv[i], "--keycheck")) keycheck = 1;
        else if (!strcmp(argv[i], "--lodcheck")) lodcheck = 1;
        else if (!strcmp(argv[i], "--instcheck")) instcheck = 1;
        else if (!strcmp(argv[i], "--skincheck")) skincheck = 1;
        else if (!strcmp(argv[i], "--bench-scene")) bench_scene = 1;
        else if (!strcmp(argv[i], "--bench-key")) bench_key = 1;
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rounds") && i + 1 < argc) rounds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--objects") && i + 1 < argc) objects = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tris") && i + 1 < argc) tris_target = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc)
        {
            if (sscanf(argv[++i], "%dx%d", &bw, &bh) != 2) { printf("--size WxH\n"); return 2; }
        }
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else { printf("argomento non riconosciuto: %s\n", argv[i]); usage(); return 2; }
    }
    if (demo) w_set_demo_path(demo);
    if (cullcheck) return run_p5_cull();
    if (interactcheck) return run_p5_interact();
    if (raycheck) return run_p5_rays();
    if (statscheck) return run_p5_stats();
    if (keycheck) return run_p5_key();
    if (lodcheck) return run_p5_lod();
    if (instcheck) return run_p5_inst();
    if (skincheck) return run_p5_skin();
    if (bench_key) return p5_key_bench(objects, keys, rounds);
    if (bench_scene)
    {
        /* --tris N: 12 triangoli per box, griglia quadrata (vedi p5_scene_bench). */
        if (tris_target > 0)
        {
            objects = (tris_target + 11) / 12;
            return p5_scene_bench(objects, bw, bh, frames, 1);
        }
        return p5_scene_bench(objects, bw, bh, frames, 0);
    }
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
