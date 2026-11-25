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
    m.pos = CUBE_POS; m.uv = CUBE_UV; m.nv = 24; m.idx = CUBE_IDX; m.nt = 12;
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

/* P1: triangoli. P2: settori (room/door/pitch) e mappa demo. P3: mesh sul
 * target ARGB+depth (cubo, piano, clip, z-buffer, back-face). P4: luce, fog,
 * cielo, thing e ordine. L'accettazione della fase passa da qui, non da
 * un'impressione.
 */
#define NSCENES 18
static const char* const SCENE_NAMES[NSCENES] = {
    "tri_tex", "tri_mask", "tri_depth",
    "sector_room", "sector_door", "sector_pitch", "demo",
    "mesh_cube", "mesh_plane", "mesh_clip", "mesh_depth", "mesh_cull",
    "lamp_point", "lamp_spot", "fog", "sky", "thing", "order"
};
static const scene_fn SCENE_FNS[NSCENES] = {
    scene_tri_tex, scene_tri_mask, scene_tri_depth,
    w_scene_room, w_scene_door, w_scene_pitch, w_scene_demo,
    scene_mesh_cube, scene_mesh_plane, scene_mesh_clip, scene_mesh_depth, scene_mesh_cull,
    p4_scene_lamp_point, p4_scene_lamp_spot, p4_scene_fog, p4_scene_sky,
    p4_scene_thing, p4_scene_order
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

    /* M-FOG: buio lineare alla distanza dichiarata. */
    gmb_3d_env_clear(&env);
    env.fog = 100.0f;
    P4CHECK(gmb_3d_light_eval(&env, 0.0f, 0.0f, 0.0f, 255, 50.0f) == 128, "fog: metà distanza = metà luce");
    P4CHECK(gmb_3d_light_eval(&env, 0.0f, 0.0f, 0.0f, 255, 100.0f) == 0, "fog: alla distanza = buio");

    /* C-SAME-COLORMAP: 32 livelli (100 -> q12 -> 98). */
    gmb_3d_env_clear(&env);
    env.colormap = 1;
    P4CHECK(gmb_3d_light_eval(&env, 0.0f, 0.0f, 0.0f, 100, 0.0f) == 98, "colormap: 100 -> 98 su 32 livelli");

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
