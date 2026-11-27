/* gmb_3d_scene.c — matrici del renderer e culling per oggetto (P5, M-CULL).
 *
 * Perché un file a parte: gmb_3d_cmd.c è la facciata dei comandi e stava già al
 * limite delle 1200 righe (§0quater). Qui vive la matematica che la facciata usa,
 * una volta sola: matrici column-major e decisione «questo oggetto è fuori dal
 * frustum?». Nessuno stato: tutte le funzioni prendono quello che serve.
 *
 * Il culling per oggetto NON cambia i pixel. Il rasterizzatore
 * (gmb_3d_frame.c) scarta già i triangoli fuori dal frustum, uno per uno, e li
 * conta in `tris_culled`; un oggetto interamente fuori da un piano ha tutti i
 * triangoli fuori, quindi il clip li avrebbe scartati tutti. Qui si prende la
 * stessa decisione una volta sola, con un AABB e un margine, e si salta il
 * lavoro di preparazione: DRAWN, PIXELS e l'hash del frame restano identici.
 *
 * Convenzioni: clip z in [-1,1], w = distanza di vista; modello = traslazione +
 * rotazione su y (la superficie SCENE non ha scala né roll).
 */
#include "gmb_3d_internal.h"

#include <math.h>
#include <string.h>

#define SC_PI 3.14159265358979

/* --- matrici (column-major; clip z in [-1,1], come il kernel) ------------ */

void gmb_3d_m4_mul(float* out, const float* a, const float* b)   /* out = a*b */
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

void gmb_3d_m4_perspective(float* m, float fovy, float aspect, float zn, float zf)
{
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fovy * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = (2.0f * zf * zn) / (zn - zf);
}

void gmb_3d_m4_look_at(float* m, const float eye[3], const float at[3], const float up[3])
{
    float f[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (fl <= 0.0f) fl = 1.0f;
    f[0] /= fl; f[1] /= fl; f[2] /= fl;
    float r[3] = { f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0] };
    float rl = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    if (rl <= 0.0f) rl = 1.0f;
    r[0] /= rl; r[1] /= rl; r[2] /= rl;
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    memset(m, 0, 16 * sizeof(float));
    m[0] = r[0]; m[4] = r[1]; m[8]  = r[2]; m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    m[1] = u[0]; m[5] = u[1]; m[9]  = u[2]; m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
    m[15] = 1.0f;
}

/* Modello di un oggetto: traslazione * rotazione attorno a Y. */
void gmb_3d_m4_model(float* m, float x, float y, float z, float yaw_deg)
{
    float a = yaw_deg * (float)(SC_PI / 180.0);
    float c = cosf(a), s = sinf(a);
    memset(m, 0, 16 * sizeof(float));
    m[0] = c; m[2] = -s;
    m[5] = 1.0f;
    m[8] = s; m[10] = c;
    m[12] = x; m[13] = y; m[14] = z; m[15] = 1.0f;
}

/* TRS completo per le piste KEY: R = Ry(yaw) * Rx(pitch) * Rz(roll), gradi,
 * stessa convenzione column-major di gmb_3d_m4_model. Con pitch = roll = 0 si
 * passa dalla funzione a sola yaw: stessi bit di prima, quindi le golden e il
 * culling delle scene senza chiavi non cambiano. */
void gmb_3d_m4_model_trs(float* m, float x, float y, float z,
                         float yaw_deg, float pitch_deg, float roll_deg)
{
    if (pitch_deg == 0.0f && roll_deg == 0.0f)
    {
        gmb_3d_m4_model(m, x, y, z, yaw_deg);
        return;
    }
    const float k = (float)(SC_PI / 180.0);
    float cy = cosf(yaw_deg * k), sy = sinf(yaw_deg * k);
    float cp = cosf(pitch_deg * k), sp = sinf(pitch_deg * k);
    float cr = cosf(roll_deg * k), sr = sinf(roll_deg * k);

    /* Righe di R = Ry*Rx*Rz (matematica, poi si scrivono le colonne). */
    float r00 = cy * cr + sy * sp * sr, r01 = -cy * sr + sy * sp * cr, r02 = sy * cp;
    float r10 = cp * sr,               r11 = cp * cr,                r12 = -sp;
    float r20 = -sy * cr + cy * sp * sr, r21 = sy * sr + cy * sp * cr, r22 = cy * cp;

    memset(m, 0, 16 * sizeof(float));
    m[0] = r00; m[1] = r10; m[2] = r20;
    m[4] = r01; m[5] = r11; m[6] = r21;
    m[8] = r02; m[9] = r12; m[10] = r22;
    m[12] = x; m[13] = y; m[14] = z; m[15] = 1.0f;
}

/* --- culling per oggetto (P5, M-CULL) ------------------------------------ */

/* I sei piani del frustum in coordinate MONDO, dalle righe della view-projection
 * (column-major). Dentro il frustum significa `a*x + b*y + c*z + d >= 0` su tutti
 * e sei: è la stessa condizione che il clip applica ai singoli vertici. */
void gmb_3d_frustum_planes(const float* vp, float planes[6][4])
{
    for (int i = 0; i < 4; i++)
    {
        planes[0][i] = vp[i * 4 + 3] + vp[i * 4 + 0];   /* sinistra */
        planes[1][i] = vp[i * 4 + 3] - vp[i * 4 + 0];   /* destra   */
        planes[2][i] = vp[i * 4 + 3] + vp[i * 4 + 1];   /* sotto    */
        planes[3][i] = vp[i * 4 + 3] - vp[i * 4 + 1];   /* sopra    */
        planes[4][i] = vp[i * 4 + 3] + vp[i * 4 + 2];   /* vicino   */
        planes[5][i] = vp[i * 4 + 3] - vp[i * 4 + 2];   /* lontano  */
    }
}

/* L'AABB dell'oggetto nello spazio MONDO. Il modello è traslazione + rotazione su
 * y, quindi si ruotano i quattro angoli (x,z) e la y si trasla: il volume resta un
 * AABB e il test sui piani non richiede gli otto angoli. */
static void obj_aabb_world(const float* model, const float bmin[3], const float bmax[3],
                           float c[3], float h[3])
{
    float minx = 0.0f, minz = 0.0f, maxx = 0.0f, maxz = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        float px = (i & 1) ? bmax[0] : bmin[0];
        float pz = (i & 2) ? bmax[2] : bmin[2];
        float wx = model[0] * px + model[8]  * pz + model[12];
        float wz = model[2] * px + model[10] * pz + model[14];
        if (i == 0 || wx < minx) minx = wx;
        if (i == 0 || wx > maxx) maxx = wx;
        if (i == 0 || wz < minz) minz = wz;
        if (i == 0 || wz > maxz) maxz = wz;
    }
    float miny = bmin[1] + model[13];
    float maxy = bmax[1] + model[13];
    c[0] = (minx + maxx) * 0.5f;
    c[1] = (miny + maxy) * 0.5f;
    c[2] = (minz + maxz) * 0.5f;
    h[0] = (maxx - minx) * 0.5f;
    h[1] = (maxy - miny) * 0.5f;
    h[2] = (maxz - minz) * 0.5f;
}

/* 1 = l'oggetto è INTERAMENTE fuori da almeno un piano. Conservativo: il volume è
 * un AABB (più grande della geometria) e c'è un margine, quindi un oggetto che
 * tocca il bordo passa e viene disegnato come prima. */
int gmb_3d_object_outside(const float planes[6][4], const float* model,
                          const float bmin[3], const float bmax[3])
{
    float c[3], h[3];
    obj_aabb_world(model, bmin, bmax, c, h);
    for (int p = 0; p < 6; p++)
    {
        float d = planes[p][0] * c[0] + planes[p][1] * c[1] + planes[p][2] * c[2] + planes[p][3];
        float r = fabsf(planes[p][0]) * h[0] + fabsf(planes[p][1]) * h[1]
                + fabsf(planes[p][2]) * h[2];
        if (d + r < -1.0e-4f) return 1;
    }
    return 0;
}
