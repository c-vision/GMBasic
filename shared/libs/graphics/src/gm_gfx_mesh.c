/* gm_gfx_mesh.c — generazione di mesh (da c3dlas meshgen). */
#include "gm_gfx_mesh.h"

#include <math.h>
#include <stdlib.h>

void gm_mesh_free(gm_mesh *m)
{
    if (m == NULL) return;
    free(m->pos); free(m->tri);
    m->pos = NULL; m->tri = NULL; m->nv = m->nt = 0;
}

gm_mesh gm_mesh_uv_sphere(int slices, int stacks, gm_real radius)
{
    gm_mesh m;
    int i, j, ti = 0;
    m.nv = (slices + 1) * (stacks + 1);
    m.pos = (gm_vec3 *)malloc((size_t)m.nv * sizeof(gm_vec3));
    m.nt = slices * stacks * 2;
    m.tri = (int *)malloc((size_t)m.nt * 3 * sizeof(int));
    if (!m.pos || !m.tri) { gm_mesh_free(&m); return m; }
    for (i = 0; i <= stacks; i++) {
        gm_real phi = 3.14159265358979323846 * (gm_real)i / (gm_real)stacks;
        for (j = 0; j <= slices; j++) {
            gm_real theta = 2.0 * 3.14159265358979323846 * (gm_real)j / (gm_real)slices;
            m.pos[i * (slices + 1) + j] = gm_vec3_make(
                radius * sin(phi) * cos(theta),
                radius * cos(phi),
                radius * sin(phi) * sin(theta));
        }
    }
    for (i = 0; i < stacks; i++)
        for (j = 0; j < slices; j++) {
            int a = i * (slices + 1) + j, b = a + slices + 1;
            m.tri[ti++] = a; m.tri[ti++] = b; m.tri[ti++] = a + 1;
            m.tri[ti++] = a + 1; m.tri[ti++] = b; m.tri[ti++] = b + 1;
        }
    return m;
}

gm_mesh gm_mesh_box(gm_real sx, gm_real sy, gm_real sz)
{
    gm_mesh m;
    gm_real hx = sx / 2.0, hy = sy / 2.0, hz = sz / 2.0;
    static const int fidx[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {3,2,6,7}, {0,3,7,4}, {1,2,6,5}
    };
    int f, k, ti = 0;
    m.nv = 8;
    m.pos = (gm_vec3 *)malloc(8 * sizeof(gm_vec3));
    m.nt = 12;
    m.tri = (int *)malloc(12 * 3 * sizeof(int));
    if (!m.pos || !m.tri) { gm_mesh_free(&m); return m; }
    m.pos[0] = gm_vec3_make(-hx,-hy,-hz); m.pos[1] = gm_vec3_make( hx,-hy,-hz);
    m.pos[2] = gm_vec3_make( hx, hy,-hz); m.pos[3] = gm_vec3_make(-hx, hy,-hz);
    m.pos[4] = gm_vec3_make(-hx,-hy, hz); m.pos[5] = gm_vec3_make( hx,-hy, hz);
    m.pos[6] = gm_vec3_make( hx, hy, hz); m.pos[7] = gm_vec3_make(-hx, hy, hz);
    for (f = 0; f < 6; f++) {
        m.tri[ti++] = fidx[f][0]; m.tri[ti++] = fidx[f][1]; m.tri[ti++] = fidx[f][2];
        m.tri[ti++] = fidx[f][0]; m.tri[ti++] = fidx[f][2]; m.tri[ti++] = fidx[f][3];
    }
    (void)k;
    return m;
}

gm_mesh gm_mesh_plane(int nx, int ny, gm_real sx, gm_real sy)
{
    gm_mesh m;
    int i, j, ti = 0;
    m.nv = (nx + 1) * (ny + 1);
    m.pos = (gm_vec3 *)malloc((size_t)m.nv * sizeof(gm_vec3));
    m.nt = nx * ny * 2;
    m.tri = (int *)malloc((size_t)m.nt * 3 * sizeof(int));
    if (!m.pos || !m.tri) { gm_mesh_free(&m); return m; }
    for (i = 0; i <= ny; i++)
        for (j = 0; j <= nx; j++)
            m.pos[i * (nx + 1) + j] = gm_vec3_make(sx * ((gm_real)j / (gm_real)nx - 0.5), 0.0,
                                                   sy * ((gm_real)i / (gm_real)ny - 0.5));
    for (i = 0; i < ny; i++)
        for (j = 0; j < nx; j++) {
            int a = i * (nx + 1) + j, b = a + nx + 1;
            m.tri[ti++] = a; m.tri[ti++] = b; m.tri[ti++] = a + 1;
            m.tri[ti++] = a + 1; m.tri[ti++] = b; m.tri[ti++] = b + 1;
        }
    return m;
}

gm_mesh gm_mesh_cylinder(int slices, gm_real radius, gm_real height)
{
    gm_mesh m;
    int i, j, ti = 0;
    gm_real hy = height / 2.0;
    /* (slices+1)*2 vertici laterali + 2 centri */
    m.nv = (slices + 1) * 2 + 2;
    m.pos = (gm_vec3 *)malloc((size_t)m.nv * sizeof(gm_vec3));
    m.nt = slices * 4; /* 2 per lato + 1 per tappo x2 */
    m.tri = (int *)malloc((size_t)m.nt * 3 * sizeof(int));
    if (!m.pos || !m.tri) { gm_mesh_free(&m); return m; }
    for (i = 0; i <= slices; i++) {
        gm_real th = 2.0 * 3.14159265358979323846 * (gm_real)i / (gm_real)slices;
        gm_real x = radius * cos(th), z = radius * sin(th);
        m.pos[i] = gm_vec3_make(x, -hy, z);
        m.pos[i + slices + 1] = gm_vec3_make(x, hy, z);
    }
    m.pos[(slices + 1) * 2] = gm_vec3_make(0.0, -hy, 0.0);
    m.pos[(slices + 1) * 2 + 1] = gm_vec3_make(0.0, hy, 0.0);
    for (i = 0; i < slices; i++) {
        int a = i, b = i + 1, c = i + slices + 1, d = i + slices + 2;
        int bot = (slices + 1) * 2, top = (slices + 1) * 2 + 1;
        m.tri[ti++] = a; m.tri[ti++] = c; m.tri[ti++] = b;
        m.tri[ti++] = b; m.tri[ti++] = c; m.tri[ti++] = d;
        m.tri[ti++] = bot; m.tri[ti++] = b; m.tri[ti++] = a;
        m.tri[ti++] = top; m.tri[ti++] = c; m.tri[ti++] = d;
    }
    (void)j;
    return m;
}

/* ═══ Attributi opzionali: normali + UV (P0 del piano 3D, additive) ════════════
 * Nessuna funzione qui cambia il comportamento dei generatori sopra: chi vuole
 * solo posizioni+indici continua a chiamare gm_mesh_box/plane/uv_sphere/cylinder
 * e ottiene esattamente gli stessi numeri di prima. */

gm_mesh_attrs gm_mesh_attrs_none(void)
{
    gm_mesh_attrs a;
    a.nrm = NULL; a.uv = NULL;
    return a;
}

void gm_mesh_attrs_free(gm_mesh_attrs *a)
{
    if (a == NULL) return;
    free(a->nrm); free(a->uv);
    a->nrm = NULL; a->uv = NULL;
}

int gm_mesh_attrs_valid(const gm_mesh_attrs *a, int nv)
{
    return a != NULL && a->nrm != NULL && a->uv != NULL && nv > 0;
}

static int attrs_alloc(gm_mesh_attrs *a, int nv)
{
    a->nrm = NULL; a->uv = NULL;
    if (nv <= 0) return 0;
    a->nrm = (gm_vec3 *)malloc((size_t)nv * sizeof(gm_vec3));
    a->uv = (gm_real *)malloc((size_t)nv * 2 * sizeof(gm_real));
    if (a->nrm == NULL || a->uv == NULL) { gm_mesh_attrs_free(a); return 0; }
    return 1;
}

/* normalizza, con fallback neutro su vettore nullo (mesh degenere: non si
 * produce NaN, che poi avvelenerebbe il rasterizzatore). */
static gm_vec3 safe_normal(gm_vec3 v)
{
    gm_real len = gm_vec3_length(v);
    if (len <= 1e-12) return gm_vec3_make(0.0, 1.0, 0.0);
    return gm_vec3_scale(v, 1.0 / len);
}

gm_status gm_mesh_normals(const gm_mesh *m, gm_vec3 *nrm)
{
    int t, i;
    if (m == NULL || nrm == NULL) return GM_ERR_ARG;
    if (m->pos == NULL || m->tri == NULL || m->nv <= 0 || m->nt <= 0) return GM_ERR_ARG;
    for (i = 0; i < m->nv; i++) nrm[i] = gm_vec3_make(0.0, 0.0, 0.0);
    for (t = 0; t < m->nt; t++) {
        int a = m->tri[t * 3], b = m->tri[t * 3 + 1], c = m->tri[t * 3 + 2];
        gm_vec3 n;
        if (a < 0 || b < 0 || c < 0 || a >= m->nv || b >= m->nv || c >= m->nv) continue;
        /* non normalizzata: il modulo è 2*area, quindi pesa da sé le facce grandi */
        n = gm_vec3_cross(gm_vec3_sub(m->pos[b], m->pos[a]),
                          gm_vec3_sub(m->pos[c], m->pos[a]));
        nrm[a] = gm_vec3_add(nrm[a], n);
        nrm[b] = gm_vec3_add(nrm[b], n);
        nrm[c] = gm_vec3_add(nrm[c], n);
    }
    for (i = 0; i < m->nv; i++) nrm[i] = safe_normal(nrm[i]);
    return GM_OK;
}

/* Box con 24 vertici (4 per faccia): serve per avere normali PIATTE e UV intere
 * su ogni faccia; il box da 8 vertici resta com'è e non si tocca. */
gm_status gm_mesh_box_uv(gm_real sx, gm_real sy, gm_real sz, gm_mesh *m, gm_mesh_attrs *a)
{
    /* per faccia: normale (n) e asse u (t); v = n x t. */
    static const gm_real FACES[6][6] = {
        { 1, 0, 0,   0, 0,-1 },
        {-1, 0, 0,   0, 0, 1 },
        { 0, 1, 0,   1, 0, 0 },
        { 0,-1, 0,   1, 0, 0 },
        { 0, 0, 1,   1, 0, 0 },
        { 0, 0,-1,  -1, 0, 0 }
    };
    gm_real hx = sx / 2.0, hy = sy / 2.0, hz = sz / 2.0;
    int f, k, ti = 0;
    if (m == NULL || a == NULL || sx <= 0.0 || sy <= 0.0 || sz <= 0.0) return GM_ERR_ARG;
    m->nv = 24;
    m->pos = (gm_vec3 *)malloc(24 * sizeof(gm_vec3));
    m->nt = 12;
    m->tri = (int *)malloc(12 * 3 * sizeof(int));
    if (m->pos == NULL || m->tri == NULL || !attrs_alloc(a, 24)) {
        gm_mesh_free(m);
        return GM_ERR_NO_MEM;
    }
    for (f = 0; f < 6; f++) {
        gm_vec3 n = gm_vec3_make(FACES[f][0], FACES[f][1], FACES[f][2]);
        gm_vec3 t = gm_vec3_make(FACES[f][3], FACES[f][4], FACES[f][5]);
        gm_vec3 b = gm_vec3_cross(n, t);
        gm_real hn = (n.x != 0.0) ? hx : (n.y != 0.0 ? hy : hz);
        gm_real hu = (t.x != 0.0) ? hx : hz;
        gm_real hv = (b.x != 0.0) ? hx : (b.y != 0.0 ? hy : hz);
        int base = f * 4;
        for (k = 0; k < 4; k++) {
            gm_real su = (k == 1 || k == 2) ? 1.0 : -1.0;
            gm_real sv = (k >= 2) ? 1.0 : -1.0;
            int idx = base + k;
            m->pos[idx] = gm_vec3_add(gm_vec3_scale(n, hn),
                             gm_vec3_add(gm_vec3_scale(t, hu * su),
                                         gm_vec3_scale(b, hv * sv)));
            a->nrm[idx] = n;
            a->uv[idx * 2] = (su > 0.0) ? 1.0 : 0.0;
            a->uv[idx * 2 + 1] = (sv > 0.0) ? 1.0 : 0.0;
        }
        m->tri[ti++] = base;     m->tri[ti++] = base + 1; m->tri[ti++] = base + 2;
        m->tri[ti++] = base;     m->tri[ti++] = base + 2; m->tri[ti++] = base + 3;
    }
    return GM_OK;
}

gm_status gm_mesh_plane_uv(int nx, int ny, gm_real sx, gm_real sy, gm_mesh *m, gm_mesh_attrs *a)
{
    int i, j;
    if (m == NULL || a == NULL || nx < 1 || ny < 1) return GM_ERR_ARG;
    *m = gm_mesh_plane(nx, ny, sx, sy);
    if (m->nv <= 0 || m->pos == NULL) return GM_ERR_NO_MEM;
    if (!attrs_alloc(a, m->nv)) { gm_mesh_free(m); return GM_ERR_NO_MEM; }
    for (i = 0; i <= ny; i++)
        for (j = 0; j <= nx; j++) {
            int idx = i * (nx + 1) + j;
            a->nrm[idx] = gm_vec3_make(0.0, 1.0, 0.0);
            a->uv[idx * 2] = (gm_real)j / (gm_real)nx;
            a->uv[idx * 2 + 1] = (gm_real)i / (gm_real)ny;
        }
    return GM_OK;
}

gm_status gm_mesh_sphere_uv(int slices, int stacks, gm_real radius, gm_mesh *m, gm_mesh_attrs *a)
{
    int i, j, idx;
    if (m == NULL || a == NULL || slices < 3 || stacks < 2 || radius <= 0.0) return GM_ERR_ARG;
    /* stessa topologia della sfera esistente: nessuna divergenza fra le due */
    *m = gm_mesh_uv_sphere(slices, stacks, radius);
    if (m->nv <= 0 || m->pos == NULL) return GM_ERR_NO_MEM;
    if (!attrs_alloc(a, m->nv)) { gm_mesh_free(m); return GM_ERR_NO_MEM; }
    for (i = 0; i <= stacks; i++)
        for (j = 0; j <= slices; j++) {
            idx = i * (slices + 1) + j;
            a->nrm[idx] = safe_normal(m->pos[idx]);
            a->uv[idx * 2] = (gm_real)j / (gm_real)slices;
            a->uv[idx * 2 + 1] = (gm_real)i / (gm_real)stacks;
        }
    return GM_OK;
}

gm_status gm_mesh_cylinder_uv(int slices, gm_real radius, gm_real height,
                              gm_mesh *m, gm_mesh_attrs *a)
{
    int i, rim, bot, top;
    if (m == NULL || a == NULL || slices < 3 || radius <= 0.0 || height <= 0.0) return GM_ERR_ARG;
    *m = gm_mesh_cylinder(slices, radius, height);
    if (m->nv <= 0 || m->pos == NULL) return GM_ERR_NO_MEM;
    if (!attrs_alloc(a, m->nv)) { gm_mesh_free(m); return GM_ERR_NO_MEM; }
    rim = (slices + 1) * 2;
    bot = rim;          /* centro della base bassa */
    top = rim + 1;      /* centro della base alta */
    for (i = 0; i <= slices; i++) {
        gm_real th = 2.0 * 3.14159265358979323846 * (gm_real)i / (gm_real)slices;
        gm_vec3 n = gm_vec3_make(cos(th), 0.0, sin(th));
        gm_real u = (gm_real)i / (gm_real)slices;
        a->nrm[i] = n;            a->uv[i * 2] = u;            a->uv[i * 2 + 1] = 0.0;
        a->nrm[i + slices + 1] = n; a->uv[(i + slices + 1) * 2] = u;
        a->uv[(i + slices + 1) * 2 + 1] = 1.0;
    }
    a->nrm[bot] = gm_vec3_make(0.0, -1.0, 0.0);
    a->nrm[top] = gm_vec3_make(0.0, 1.0, 0.0);
    a->uv[bot * 2] = a->uv[top * 2] = 0.5;
    a->uv[bot * 2 + 1] = a->uv[top * 2 + 1] = 0.5;
    return GM_OK;
}
