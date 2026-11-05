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
