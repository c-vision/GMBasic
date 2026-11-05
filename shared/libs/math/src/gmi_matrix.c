/* ============================================================================
 * gmi_matrix.c — Matrice unica (algebra lineare).
 * Un solo registry di handle → doppia-precision matrix. Usata da MATRIX_CMD
 * (comandi grafici 4x4) e LINALG.* (algebra NxN): stessa entità, due front-end.
 * ============================================================================ */
#include "gmi_matrix.h"
#include "gc_gemm.h"
#include "gc_array.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
typedef struct { volatile LONG locked; } gmi_matrix_lock_t;
static void gmi_matrix_lock(gmi_matrix_lock_t* m) { while (_InterlockedExchange(&m->locked, 1)) { ; } }
static void gmi_matrix_unlock(gmi_matrix_lock_t* m) { _InterlockedExchange(&m->locked, 0); }
#else
#include <pthread.h>
#include <unistd.h>
typedef struct { volatile int locked; } gmi_matrix_lock_t;
static void gmi_matrix_lock(gmi_matrix_lock_t* m) { while (__sync_lock_test_and_set(&m->locked, 1)) { ; } }
static void gmi_matrix_unlock(gmi_matrix_lock_t* m) { __sync_lock_release(&m->locked); }
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GMB_MATRIX_MAX 256

typedef struct {
    double* data;       /* row-major */
    int rows, cols;
    int in_use;
} gmi_matrix_t;

static gmi_matrix_t g_mats[GMB_MATRIX_MAX];
static gmi_matrix_lock_t g_mtx;

void gmi_matrix_init(void)
{
    gmi_matrix_lock(&g_mtx);
    memset(g_mats, 0, sizeof(g_mats));
    gmi_matrix_unlock(&g_mtx);
}

void gmi_matrix_shutdown(void)
{
    gmi_matrix_lock(&g_mtx);
    for (int i = 1; i < GMB_MATRIX_MAX; i++)
    {
        if (g_mats[i].in_use)
        {
            free(g_mats[i].data);
            g_mats[i].in_use = 0;
        }
    }
    gmi_matrix_unlock(&g_mtx);
}

static gmi_matrix_t* mget(int64_t h)
{
    if (h <= 0 || h >= GMB_MATRIX_MAX) return NULL;
    return g_mats[h].in_use ? &g_mats[h] : NULL;
}

int64_t gmi_matrix_create(int rows, int cols)
{
    if (rows <= 0 || cols <= 0) return -1;
    gmi_matrix_lock(&g_mtx);
    for (int i = 1; i < GMB_MATRIX_MAX; i++)
    {
        if (!g_mats[i].in_use)
        {
            g_mats[i].data = (double*)calloc((size_t)(rows * cols), sizeof(double));
            if (!g_mats[i].data) { gmi_matrix_unlock(&g_mtx); return -1; }
            g_mats[i].rows = rows;
            g_mats[i].cols = cols;
            g_mats[i].in_use = 1;
            /* identità se quadrata */
            if (rows == cols)
                for (int r = 0; r < rows; r++) g_mats[i].data[r * cols + r] = 1.0;
            gmi_matrix_unlock(&g_mtx);
            return (int64_t)i;
        }
    }
    gmi_matrix_unlock(&g_mtx);
    return -1;
}

int64_t gmi_matrix_create_from(int rows, int cols, const double* data)
{
    int64_t h = gmi_matrix_create(rows, cols);
    if (h <= 0 || !data) return h;
    gmi_matrix_t* m = mget(h);
    if (m) memcpy(m->data, data, (size_t)(rows * cols) * sizeof(double));
    return h;
}

int gmi_matrix_rows(int64_t h) { gmi_matrix_t* m = mget(h); return m ? m->rows : 0; }
int gmi_matrix_cols(int64_t h) { gmi_matrix_t* m = mget(h); return m ? m->cols : 0; }

double gmi_matrix_get(int64_t h, int r, int c)
{
    gmi_matrix_t* m = mget(h);
    if (!m || r < 0 || r >= m->rows || c < 0 || c >= m->cols) return 0;
    return m->data[r * m->cols + c];
}

void gmi_matrix_set(int64_t h, int r, int c, double v)
{
    gmi_matrix_t* m = mget(h);
    if (!m || r < 0 || r >= m->rows || c < 0 || c >= m->cols) return;
    m->data[r * m->cols + c] = v;
}

void gmi_matrix_free(int64_t h)
{
    gmi_matrix_lock(&g_mtx);
    gmi_matrix_t* m = mget(h);
    if (m) { free(m->data); m->in_use = 0; }
    gmi_matrix_unlock(&g_mtx);
}

int gmi_matrix_identity(int64_t h)
{
    gmi_matrix_t* m = mget(h);
    if (!m || m->rows != m->cols) return -1;
    memset(m->data, 0, (size_t)(m->rows * m->cols) * sizeof(double));
    for (int r = 0; r < m->rows; r++) m->data[r * m->cols + r] = 1.0;
    return 0;
}

int64_t gmi_matrix_multiply(int64_t a, int64_t b)
{
    gmi_matrix_t* ma = mget(a);
    gmi_matrix_t* mb = mget(b);
    if (!ma || !mb || ma->cols != mb->rows) return -1;
    int64_t out = gmi_matrix_create(ma->rows, mb->cols);
    gmi_matrix_t* mo = mget(out);
    if (!mo) return -1;
    /* SIMD + blocking (gc_gemm): C = A * B, row-major. */
    gc_gemm(ma->rows, mb->cols, ma->cols, 1.0,
            ma->data, ma->cols, mb->data, mb->cols, 0.0, mo->data, mo->cols);
    return out;
}

int gmi_matrix_multiply_into(int64_t out, int64_t a, int64_t b)
{
    int64_t r = gmi_matrix_multiply(a, b);
    if (r <= 0) return -1;
    gmi_matrix_t* mo = mget(out);
    gmi_matrix_t* mr = mget(r);
    if (!mo || !mr || mo->rows != mr->rows || mo->cols != mr->cols)
    {
        gmi_matrix_free(r);
        return -1;
    }
    memcpy(mo->data, mr->data, (size_t)(mo->rows * mo->cols) * sizeof(double));
    gmi_matrix_free(r);
    return 0;
}

int64_t gmi_matrix_transpose(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) return -1;
    int64_t out = gmi_matrix_create(ma->cols, ma->rows);
    gmi_matrix_t* mo = mget(out);
    if (!mo) return -1;
    for (int r = 0; r < ma->rows; r++)
        for (int c = 0; c < ma->cols; c++)
            mo->data[c * mo->cols + r] = ma->data[r * ma->cols + c];
    return out;
}

/* Gauss-Jordan inverse (in place su una copia) */
int64_t gmi_matrix_inverse(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    double* m = (double*)malloc((size_t)(n * n) * sizeof(double));
    double* inv = (double*)malloc((size_t)(n * n) * sizeof(double));
    if (!m || !inv) { free(m); free(inv); return -1; }
    memcpy(m, ma->data, (size_t)(n * n) * sizeof(double));
    for (int i = 0; i < n * n; i++) inv[i] = 0;
    for (int i = 0; i < n; i++) inv[i * n + i] = 1.0;
    for (int col = 0; col < n; col++)
    {
        /* pivot */
        int pivot = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(m[r * n + col]) > fabs(m[pivot * n + col])) pivot = r;
        if (fabs(m[pivot * n + col]) < 1e-12) { free(m); free(inv); return -1; }
        if (pivot != col)
        {
            for (int c = 0; c < n; c++)
            {
                double t = m[col * n + c]; m[col * n + c] = m[pivot * n + c]; m[pivot * n + c] = t;
                t = inv[col * n + c]; inv[col * n + c] = inv[pivot * n + c]; inv[pivot * n + c] = t;
            }
        }
        double d = m[col * n + col];
        for (int c = 0; c < n; c++)
        {
            m[col * n + c] /= d;
            inv[col * n + c] /= d;
        }
        for (int r = 0; r < n; r++)
        {
            if (r == col) continue;
            double f = m[r * n + col];
            for (int c = 0; c < n; c++)
            {
                m[r * n + c] -= f * m[col * n + c];
                inv[r * n + c] -= f * inv[col * n + c];
            }
        }
    }
    int64_t out = gmi_matrix_create_from(n, n, inv);
    free(m); free(inv);
    return out;
}

double gmi_matrix_determinant(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return 0;
    int n = ma->rows;
    double* m = (double*)malloc((size_t)(n * n) * sizeof(double));
    if (!m) return 0;
    memcpy(m, ma->data, (size_t)(n * n) * sizeof(double));
    double det = 1.0;
    for (int col = 0; col < n; col++)
    {
        int pivot = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(m[r * n + col]) > fabs(m[pivot * n + col])) pivot = r;
        if (fabs(m[pivot * n + col]) < 1e-12) { free(m); return 0; }
        if (pivot != col) { det = -det; for (int c = 0; c < n; c++) { double t = m[col*n+c]; m[col*n+c]=m[pivot*n+c]; m[pivot*n+c]=t; } }
        double d = m[col * n + col];
        det *= d;
        for (int r = col + 1; r < n; r++)
        {
            double f = m[r * n + col] / d;
            for (int c = col; c < n; c++)
                m[r * n + c] -= f * m[col * n + c];
        }
    }
    free(m);
    return det;
}

/* Trasformazioni 4x4 — parità IlRuntime: POST-moltiplicazione M = M * T. */
static void postmul4(gmi_matrix_t* m, const double t[16])
{
    double out[16];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            double acc = 0.0;
            for (int k = 0; k < 4; k++) acc += m->data[r * 4 + k] * t[k * 4 + c];
            out[r * 4 + c] = acc;
        }
    memcpy(m->data, out, sizeof(out));
}

int gmi_matrix_translate(int64_t h, double x, double y, double z)
{
    gmi_matrix_t* m = mget(h);
    if (!m || m->rows != 4 || m->cols != 4) return -1;
    const double t[16] = { 1,0,0,x, 0,1,0,y, 0,0,1,z, 0,0,0,1 };
    postmul4(m, t);
    return 0;
}

int gmi_matrix_rotate_x(int64_t h, double deg)
{
    gmi_matrix_t* m = mget(h);
    if (!m || m->rows != 4 || m->cols != 4) return -1;
    double c = cos(deg * M_PI / 180.0), s = sin(deg * M_PI / 180.0);
    const double t[16] = { 1,0,0,0, 0,c,-s,0, 0,s,c,0, 0,0,0,1 };
    postmul4(m, t);
    return 0;
}

int gmi_matrix_rotate_y(int64_t h, double deg)
{
    gmi_matrix_t* m = mget(h);
    if (!m || m->rows != 4 || m->cols != 4) return -1;
    double c = cos(deg * M_PI / 180.0), s = sin(deg * M_PI / 180.0);
    const double t[16] = { c,0,s,0, 0,1,0,0, -s,0,c,0, 0,0,0,1 };
    postmul4(m, t);
    return 0;
}

int gmi_matrix_rotate_z(int64_t h, double deg)
{
    gmi_matrix_t* m = mget(h);
    if (!m || m->rows != 4 || m->cols != 4) return -1;
    double c = cos(deg * M_PI / 180.0), s = sin(deg * M_PI / 180.0);
    const double t[16] = { c,-s,0,0, s,c,0,0, 0,0,1,0, 0,0,0,1 };
    postmul4(m, t);
    return 0;
}

int gmi_matrix_scale(int64_t h, double x, double y, double z)
{
    gmi_matrix_t* m = mget(h);
    if (!m || m->rows != 4 || m->cols != 4) return -1;
    const double t[16] = { x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1 };
    postmul4(m, t);
    return 0;
}

/* ============================================================================
 * Vettore (un solo tipo, come la matrice)
 * ============================================================================ */
#define GMB_VECTOR_MAX 256
typedef struct { double v[4]; int in_use; } gmi_vector_t;
static gmi_vector_t g_vecs[GMB_VECTOR_MAX];

static gmi_vector_t* vget(int64_t h)
{
    if (h <= 0 || h >= GMB_VECTOR_MAX) return NULL;
    return g_vecs[h].in_use ? &g_vecs[h] : NULL;
}

int64_t gmi_vector_create(double x, double y, double z, double w)
{
    gmi_matrix_lock(&g_mtx);
    for (int i = 1; i < GMB_VECTOR_MAX; i++)
    {
        if (!g_vecs[i].in_use)
        {
            g_vecs[i].v[0] = x; g_vecs[i].v[1] = y; g_vecs[i].v[2] = z; g_vecs[i].v[3] = w;
            g_vecs[i].in_use = 1;
            gmi_matrix_unlock(&g_mtx);
            return (int64_t)i;
        }
    }
    gmi_matrix_unlock(&g_mtx);
    return -1;
}

int gmi_vector_get(int64_t h, int i, double* out)
{
    gmi_vector_t* v = vget(h);
    if (!v || i < 0 || i > 3 || !out) return -1;
    *out = v->v[i];
    return 0;
}

void gmi_vector_set(int64_t h, int i, double val)
{
    gmi_vector_t* v = vget(h);
    if (!v || i < 0 || i > 3) return;
    v->v[i] = val;
}

void gmi_vector_add(int64_t h, int64_t other)
{
    gmi_vector_t* a = vget(h); gmi_vector_t* b = vget(other);
    if (!a || !b) return;
    for (int i = 0; i < 4; i++) a->v[i] += b->v[i];
}

void gmi_vector_sub(int64_t h, int64_t other)
{
    gmi_vector_t* a = vget(h); gmi_vector_t* b = vget(other);
    if (!a || !b) return;
    for (int i = 0; i < 4; i++) a->v[i] -= b->v[i];
}

void gmi_vector_mul(int64_t h, double s)
{
    gmi_vector_t* v = vget(h);
    if (!v) return;
    for (int i = 0; i < 4; i++) v->v[i] *= s;
}

void gmi_vector_div(int64_t h, double s)
{
    gmi_vector_t* v = vget(h);
    if (!v || s == 0) return;
    for (int i = 0; i < 4; i++) v->v[i] /= s;
}

double gmi_vector_norm(int64_t h)
{
    gmi_vector_t* v = vget(h);
    if (!v) return 0;
    double s = 0;
    for (int i = 0; i < 4; i++) s += v->v[i] * v->v[i];
    return sqrt(s);
}

double gmi_vector_dot(int64_t a, int64_t b)
{
    gmi_vector_t* va = vget(a); gmi_vector_t* vb = vget(b);
    if (!va || !vb) return 0;
    double s = 0;
    for (int i = 0; i < 4; i++) s += va->v[i] * vb->v[i];
    return s;
}

double gmi_vector_dot_flat(const double* a, int alen, const double* b, int blen)
{
    if (!a || !b) return 0;
    int n = alen < blen ? alen : blen;
    double s = 0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

double gmi_vector_norm_flat(const double* a, int alen)
{
    if (!a) return 0;
    double s = 0;
    for (int i = 0; i < alen; i++) s += a[i] * a[i];
    return sqrt(s);
}

void gmi_vector_fill_random(int64_t h, double lo, double hi)
{
    gmi_vector_t* v = vget(h);
    if (!v) return;
    for (int i = 0; i < 4; i++)
        v->v[i] = lo + (hi - lo) * ((double)rand() / RAND_MAX);
}

void gmi_vector_wrap(int64_t h, double mn, double mx)
{
    gmi_vector_t* v = vget(h);
    if (!v) return;
    double r = mx - mn;
    for (int i = 0; i < 4; i++)
    {
        if (v->v[i] < mn) v->v[i] += r;
        if (v->v[i] > mx) v->v[i] -= r;
    }
}

void gmi_vector_free(int64_t h)
{
    gmi_matrix_lock(&g_mtx);
    gmi_vector_t* v = vget(h);
    if (v) v->in_use = 0;
    gmi_matrix_unlock(&g_mtx);
}

/* ---- Helper comandi VECTOR (forma handle, parità C99 con IL) ---- */
int gmi_vector_valid(int64_t h) { return vget(h) != NULL ? 1 : 0; }

/* Wrap con semantica IL (IlRuntime.Wrap, basata su resto/fmod — non il ±range singolo
 * di gmi_vector_wrap): usata dai comandi VECTOR WRAP/ADD WRAP su array diretti. */
double gmi_wrap_range(double v, double mn, double mx)
{
    double r = mx - mn;
    if (r <= 0) return v;
    if (v < mn) return mx - fmod(mn - v, r);
    if (v > mx) return mn + fmod(v - mx, r);
    return v;
}

void gmi_vector_add_scalar(int64_t h, double s)
{
    gmi_vector_t* v = vget(h); if (!v) return;
    for (int i = 0; i < 4; i++) v->v[i] += s;
}

void gmi_vector_sub_scalar(int64_t h, double s)
{
    gmi_vector_t* v = vget(h); if (!v) return;
    for (int i = 0; i < 4; i++) v->v[i] -= s;
}

void gmi_vector_mul_vec(int64_t h, int64_t other)
{
    gmi_vector_t* a = vget(h); gmi_vector_t* b = vget(other);
    if (!a || !b) return;
    for (int i = 0; i < 4; i++) a->v[i] *= b->v[i];
}

void gmi_vector_div_vec(int64_t h, int64_t other)
{
    gmi_vector_t* a = vget(h); gmi_vector_t* b = vget(other);
    if (!a || !b) return;
    for (int i = 0; i < 4; i++) a->v[i] /= b->v[i];
}

void gmi_vector_set_all(int64_t h, double val)
{
    gmi_vector_t* v = vget(h); if (!v) return;
    for (int i = 0; i < 4; i++) v->v[i] = val;
}

void gmi_vector_add_wrap_vec(int64_t h, int64_t other, double mn, double mx)
{
    gmi_vector_t* a = vget(h); gmi_vector_t* b = vget(other);
    if (!a || !b || mx - mn <= 0) return;
    for (int i = 0; i < 4; i++) a->v[i] = gmi_wrap_range(a->v[i] + b->v[i], mn, mx);
}

void gmi_vector_add_wrap_scalar(int64_t h, double s, double mn, double mx)
{
    gmi_vector_t* a = vget(h);
    if (!a || mx - mn <= 0) return;
    for (int i = 0; i < 4; i++) a->v[i] = gmi_wrap_range(a->v[i] + s, mn, mx);
}

/* BOUNCE2D 4 argomenti (parità IL VectorBounce2D(pos, vel, width, height)). */
void gmi_vector_bounce2d(int64_t pos, int64_t vel, double width, double height)
{
    gmi_vector_t* p = vget(pos); gmi_vector_t* v = vget(vel);
    (void)height;
    if (!p || !v) return;
    for (int i = 0; i < 4; i++)
    {
        if (p->v[i] < 0 && v->v[i] < 0) v->v[i] = -v->v[i];
        else if (p->v[i] > width && v->v[i] > 0) v->v[i] = -v->v[i];
    }
}

/* BOUNCE2D 8 argomenti (parità IL VectorBounce2D(xPos, yPos, xVel, yVel, ...)). */
void gmi_vector_bounce2d_xy(int64_t xp, int64_t yp, int64_t xv, int64_t yv,
    double xmin, double ymin, double xmax, double ymax)
{
    gmi_vector_t* px = vget(xp); gmi_vector_t* py = vget(yp);
    gmi_vector_t* vx = vget(xv); gmi_vector_t* vy = vget(yv);
    if (!px || !py || !vx || !vy) return;
    for (int i = 0; i < 4; i++)
    {
        if (px->v[i] < xmin && vx->v[i] < 0) vx->v[i] = -vx->v[i];
        else if (px->v[i] > xmax && vx->v[i] > 0) vx->v[i] = -vx->v[i];
        if (py->v[i] < ymin && vy->v[i] < 0) vy->v[i] = -vy->v[i];
        else if (py->v[i] > ymax && vy->v[i] > 0) vy->v[i] = -vy->v[i];
    }
}

/* ============================================================================
 * Decomposizioni (algoritmi numerici reali)
 * ============================================================================ */
#define GMB_DECOMP_MAX 128
typedef struct {
    int type;            /* 0=LU,1=QR,2=SVD,3=EVD,4=Cholesky */
    int64_t a;           /* matrice di base */
    int64_t b;           /* seconda matrice (U, R, S, V, L) */
    int64_t c;           /* terza matrice (V, vettori) */
    double vals[16];     /* autovalori / valori singolari */
    int n;
    int in_use;
} gmi_decomp_t;
static gmi_decomp_t g_decomps[GMB_DECOMP_MAX];

static gmi_decomp_t* dget(int64_t h)
{
    if (h <= 0 || h >= GMB_DECOMP_MAX) return NULL;
    return g_decomps[h].in_use ? &g_decomps[h] : NULL;
}

static int64_t dalloc(int type, int64_t a, int n)
{
    gmi_matrix_lock(&g_mtx);
    for (int i = 1; i < GMB_DECOMP_MAX; i++)
    {
        if (!g_decomps[i].in_use)
        {
            memset(&g_decomps[i], 0, sizeof(gmi_decomp_t));
            g_decomps[i].type = type;
            g_decomps[i].a = a;
            g_decomps[i].n = n;
            g_decomps[i].in_use = 1;
            gmi_matrix_unlock(&g_mtx);
            return (int64_t)i;
        }
    }
    gmi_matrix_unlock(&g_mtx);
    return -1;
}

void gmi_matrix_decomp_free(int64_t h)
{
    gmi_matrix_lock(&g_mtx);
    gmi_decomp_t* d = dget(h);
    if (d) d->in_use = 0;
    gmi_matrix_unlock(&g_mtx);
}

/* ---------- LU (Doolittle con pivot parziale) ---------- */
int64_t gmi_matrix_lu(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t L = gmi_matrix_create(n, n);
    int64_t U = gmi_matrix_create(n, n);
    if (L <= 0 || U <= 0) { if(L>0)gmi_matrix_free(L); if(U>0)gmi_matrix_free(U); return -1; }
    for (int i = 0; i < n; i++) gmi_matrix_set(L, i, i, 1.0);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            gmi_matrix_set(U, i, j, gmi_matrix_get(a, i, j));
    /* pivot parziale + eliminazione */
    for (int k = 0; k < n - 1; k++)
    {
        int p = k;
        for (int i = k + 1; i < n; i++)
            if (fabs(gmi_matrix_get(U, i, k)) > fabs(gmi_matrix_get(U, p, k))) p = i;
        if (p != k)
        {
            for (int j = 0; j < n; j++)
            {
                double t = gmi_matrix_get(U, k, j); gmi_matrix_set(U, k, j, gmi_matrix_get(U, p, j)); gmi_matrix_set(U, p, j, t);
            }
            for (int j = 0; j < k; j++)
            {
                double t = gmi_matrix_get(L, k, j); gmi_matrix_set(L, k, j, gmi_matrix_get(L, p, j)); gmi_matrix_set(L, p, j, t);
            }
        }
        double d = gmi_matrix_get(U, k, k);
        if (fabs(d) < 1e-14) continue;
        for (int i = k + 1; i < n; i++)
        {
            double f = gmi_matrix_get(U, i, k) / d;
            gmi_matrix_set(L, i, k, f);
            for (int j = k; j < n; j++)
                gmi_matrix_set(U, i, j, gmi_matrix_get(U, i, j) - f * gmi_matrix_get(U, k, j));
        }
    }
    int64_t d = dalloc(0, a, n);
    if (d <= 0) { gmi_matrix_free(L); gmi_matrix_free(U); return -1; }
    gmi_decomp_t* dd = dget(d);
    dd->b = L; dd->c = U;
    return d;
}

/* ---------- QR (Householder) ---------- */
int64_t gmi_matrix_qr(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t R = gmi_matrix_create(n, n);
    int64_t Q = gmi_matrix_create(n, n);
    if (R <= 0 || Q <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            gmi_matrix_set(R, i, j, gmi_matrix_get(a, i, j));
    double* H = (double*)calloc((size_t)(n*n), sizeof(double));
    double* A = (double*)calloc((size_t)(n*n), sizeof(double));
    double* Qa = (double*)calloc((size_t)(n*n), sizeof(double));
    if (!H || !A || !Qa) { free(H); free(A); free(Qa); return -1; }
    for (int i = 0; i < n*n; i++) A[i] = gmi_matrix_get(a, i/n, i%n);
    for (int i = 0; i < n; i++) Qa[i*n+i] = 1.0;
    double* tmp = (double*)calloc((size_t)(n*n), sizeof(double));
    if (!tmp) { free(H); free(A); free(Qa); return -1; }
    for (int k = 0; k < n; k++)
    {
        double norm = 0;
        for (int i = k; i < n; i++) norm += A[i*n+k]*A[i*n+k];
        norm = sqrt(norm);
        if (norm < 1e-12) continue;
        double alpha = A[k*n+k] >= 0 ? -norm : norm;
        /* v = x - alpha*e1 */
        double* v = (double*)calloc((size_t)n, sizeof(double));
        v[k] = A[k*n+k] - alpha;
        for (int i = k+1; i < n; i++) v[i] = A[i*n+k];
        double vv = 0;
        for (int i = k; i < n; i++) vv += v[i]*v[i];
        if (vv < 1e-14) { free(v); continue; }
        /* H = I - 2 vv^T/vv */
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                H[i*n+j] = (i==j?1.0:0.0) - 2.0*v[i]*v[j]/vv;
        free(v);
        /* A = H*A */
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
            {
                double s = 0;
                for (int kk = 0; kk < n; kk++) s += H[i*n+kk]*A[kk*n+j];
                tmp[i*n+j] = s;
            }
        memcpy(A, tmp, (size_t)(n*n)*sizeof(double));
        /* Qa = Qa*H */
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
            {
                double s = 0;
                for (int kk = 0; kk < n; kk++) s += Qa[i*n+kk]*H[kk*n+j];
                tmp[i*n+j] = s;
            }
        memcpy(Qa, tmp, (size_t)(n*n)*sizeof(double));
    }
    /* copia Q (Qa), R (A) */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            gmi_matrix_set(Q, i, j, Qa[i*n+j]);
            gmi_matrix_set(R, i, j, A[i*n+j]);
        }
    free(H); free(A); free(Qa); free(tmp);
    int64_t d = dalloc(1, a, n);
    if (d <= 0) return -1;
    gmi_decomp_t* dd = dget(d);
    dd->b = Q; dd->c = R;
    return d;
}

/* ---------- SVD (Jacobi one-sided) ---------- */
int64_t gmi_matrix_svd(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t U = gmi_matrix_create(n, n);
    int64_t S = gmi_matrix_create(n, n);
    int64_t V = gmi_matrix_create(n, n);
    if (U<=0||S<=0||V<=0) return -1;
    /* copia A in U */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            gmi_matrix_set(U, i, j, gmi_matrix_get(a, i, j));
    for (int i = 0; i < n; i++) gmi_matrix_set(V, i, i, 1.0);
    /* Jacobi one-sided sweeps */
    for (int sweep = 0; sweep < 50; sweep++)
    {
        int changed = 0;
        for (int p = 0; p < n-1; p++)
            for (int q = p+1; q < n; q++)
            {
                double app=0, aqq=0, apq=0;
                for (int i = 0; i < n; i++)
                {
                    double up = gmi_matrix_get(U,i,p), uq = gmi_matrix_get(U,i,q);
                    app += up*up; aqq += uq*uq; apq += up*uq;
                }
                if (fabs(apq) < 1e-15) continue;
                double theta = (aqq - app) / (2.0*apq);
                double t = (theta >= 0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(1+theta*theta));
                double c = 1.0/sqrt(1+t*t), s = c*t;
                for (int i = 0; i < n; i++)
                {
                    double up = gmi_matrix_get(U,i,p), uq = gmi_matrix_get(U,i,q);
                    gmi_matrix_set(U,i,p, c*up - s*uq);
                    gmi_matrix_set(U,i,q, s*up + c*uq);
                    double vp = gmi_matrix_get(V,i,p), vq = gmi_matrix_get(V,i,q);
                    gmi_matrix_set(V,i,p, c*vp - s*vq);
                    gmi_matrix_set(V,i,q, s*vp + c*vq);
                }
                changed = 1;
            }
        if (!changed) break;
    }
    /* valori singolari sulla diagonale di S */
    for (int j = 0; j < n; j++)
    {
        double s = 0;
        for (int i = 0; i < n; i++) s += gmi_matrix_get(U,i,j)*gmi_matrix_get(U,i,j);
        gmi_matrix_set(S, j, j, sqrt(s));
    }
    /* ordina i valori singolari in ORDINE DISCENDENTE (parità MathNet/IL):
     * scambia le colonne di S (diagonali), U e V di conseguenza. */
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
        {
            if (gmi_matrix_get(S, j, j) > gmi_matrix_get(S, i, i))
            {
                double t = gmi_matrix_get(S, i, i); gmi_matrix_set(S, i, i, gmi_matrix_get(S, j, j)); gmi_matrix_set(S, j, j, t);
                for (int r = 0; r < n; r++)
                {
                    double u = gmi_matrix_get(U, r, i); gmi_matrix_set(U, r, i, gmi_matrix_get(U, r, j)); gmi_matrix_set(U, r, j, u);
                    double v = gmi_matrix_get(V, r, i); gmi_matrix_set(V, r, i, gmi_matrix_get(V, r, j)); gmi_matrix_set(V, r, j, v);
                }
            }
        }
    int64_t d = dalloc(2, a, n);
    if (d <= 0) return -1;
    gmi_decomp_t* dd = dget(d);
    dd->b = U; dd->c = S; dd->vals[0] = (double)n; /* S in b, V in c */
    /* riusa c come V: scambiamo (c=S, terza=V) — per semplicità: b=U, c=S, V copia */
    dd->b = U; dd->c = S;
    /* salviamo V separatamente */
    dd->a = V;
    return d;
}

/* ---------- EVD (Jacobi per simmetrica) ---------- */
int64_t gmi_matrix_evd(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t V = gmi_matrix_create(n, n);
    int64_t D = gmi_matrix_create(n, n);
    if (V<=0||D<=0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            gmi_matrix_set(V, i, j, gmi_matrix_get(a, i, j));
    for (int i = 0; i < n; i++) gmi_matrix_set(D, i, i, 1.0);
    for (int sweep = 0; sweep < 50; sweep++)
    {
        double off = 0;
        for (int i = 0; i < n; i++)
            for (int j = i+1; j < n; j++)
                off += gmi_matrix_get(V,i,j)*gmi_matrix_get(V,i,j);
        if (off < 1e-20) break;
        for (int p = 0; p < n-1; p++)
            for (int q = p+1; q < n; q++)
            {
                double apq = gmi_matrix_get(V,p,q);
                if (fabs(apq) < 1e-14) continue;
                double app = gmi_matrix_get(V,p,p), aqq = gmi_matrix_get(V,q,q);
                double theta = (aqq - app) / (2.0*apq);
                double t = (theta >= 0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(1+theta*theta));
                double c = 1.0/sqrt(1+t*t), s = c*t;
                for (int i = 0; i < n; i++)
                {
                    double vip = gmi_matrix_get(V,i,p), viq = gmi_matrix_get(V,i,q);
                    gmi_matrix_set(V,i,p, c*vip - s*viq);
                    gmi_matrix_set(V,i,q, s*vip + c*viq);
                    double dip = gmi_matrix_get(D,i,p), diq = gmi_matrix_get(D,i,q);
                    gmi_matrix_set(D,i,p, c*dip - s*diq);
                    gmi_matrix_set(D,i,q, s*dip + c*diq);
                }
                /* passata simmetrica sulle righe (J^T A J): mancante, senza non converge */
                for (int i = 0; i < n; i++)
                {
                    double vpi = gmi_matrix_get(V,p,i), vqi = gmi_matrix_get(V,q,i);
                    gmi_matrix_set(V,p,i, c*vpi - s*vqi);
                    gmi_matrix_set(V,q,i, s*vpi + c*vqi);
                }
            }
    }
    int64_t d = dalloc(3, a, n);
    if (d <= 0) return -1;
    /* ordina gli autovalori in senso ASCENDENTE (parità IL/MathNet): permuta la
     * diagonale di V e le colonne di D (autovettori) di conseguenza. Senza,
     * l'ordine dipende dai dati (Jacobi) e D[0,0] diverge dal gestito. */
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
        {
            if (gmi_matrix_get(V, j, j) < gmi_matrix_get(V, i, i))
            {
                double t = gmi_matrix_get(V, i, i);
                gmi_matrix_set(V, i, i, gmi_matrix_get(V, j, j));
                gmi_matrix_set(V, j, j, t);
                for (int r = 0; r < n; r++)
                {
                    double u = gmi_matrix_get(D, r, i);
                    gmi_matrix_set(D, r, i, gmi_matrix_get(D, r, j));
                    gmi_matrix_set(D, r, j, u);
                }
            }
        }
    gmi_decomp_t* dd = dget(d);
    dd->b = V;    /* autovalori (diagonale) */
    dd->c = D;    /* autovettori */
    return d;
}

/* ---------- Cholesky ---------- */
int64_t gmi_matrix_cholesky(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t L = gmi_matrix_create(n, n);
    if (L <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j <= i; j++)
        {
            double s = gmi_matrix_get(a, i, j);
            for (int k = 0; k < j; k++)
                s -= gmi_matrix_get(L,i,k)*gmi_matrix_get(L,j,k);
            if (i == j) gmi_matrix_set(L, i, j, sqrt(s < 0 ? 0 : s));
            else gmi_matrix_set(L, i, j, s / gmi_matrix_get(L,j,j));
        }
    int64_t d = dalloc(4, a, n);
    if (d <= 0) return -1;
    gmi_decomp_t* dd = dget(d);
    dd->b = L;
    return d;
}

int64_t gmi_matrix_decomp_get_factor(int64_t decomp, int idx)
{
    gmi_decomp_t* d = dget(decomp);
    if (!d) return -1;
    switch (d->type)
    {
        case 0: return idx==0 ? d->b : d->c;   /* LU: L, U */
        case 1: return idx==0 ? d->b : d->c;   /* QR: Q, R */
        case 2: return idx==0 ? d->b : idx==1 ? d->c : d->a; /* SVD: U, S, V */
        case 3: return idx==0 ? d->b : d->c;   /* EVD: autovalori, autovettori */
        case 4: return d->b;                   /* Cholesky: L */
        case 5: return idx==0 ? d->b : d->c;   /* POLAR: U, P */
        case 6: /* EIG: DREAL, DIMAG (N×1), VRREAL, VRIMAG */
            if (idx == 2) return d->b;
            if (idx == 3) return d->c;
            else
            {
                int n = d->n;
                int64_t out = gmi_matrix_create(n, 1);
                if (out <= 0) return -1;
                for (int i = 0; i < n; i++)
                    gmi_matrix_set(out, i, 0, d->vals[idx * n + i]);
                return out;
            }
        case 7: return idx==0 ? d->b : d->c;   /* SCHUR: T, Z */
    }
    return -1;
}

/* Estrazione fattori per NOME (MATRIX.FACTOR(h, "U") ecc.). */
int64_t gmi_matrix_decomp_get_factor_named(int64_t decomp, const char* name)
{
    gmi_decomp_t* d = dget(decomp);
    if (!d || !name) return -1;
    if (d->type == 5)   /* POLAR */
        return strcmp(name, "U") == 0 ? d->b : strcmp(name, "P") == 0 ? d->c : -1;
    if (d->type == 7)   /* SCHUR */
        return strcmp(name, "T") == 0 ? d->b : strcmp(name, "Z") == 0 ? d->c : -1;
    if (d->type == 6)   /* EIG */
    {
        int n = d->n;
        if (strcmp(name, "VRREAL") == 0) return d->b;
        if (strcmp(name, "VRIMAG") == 0) return d->c;
        if (strcmp(name, "DREAL") == 0 || strcmp(name, "DIMAG") == 0)
        {
            int idx = strcmp(name, "DIMAG") == 0 ? 1 : 0;
            int64_t out = gmi_matrix_create(n, 1);
            if (out <= 0) return -1;
            for (int i = 0; i < n; i++)
                gmi_matrix_set(out, i, 0, d->vals[idx * n + i]);
            return out;
        }
        return -1;
    }
    /* fallback per le decomposizioni classiche — dispatch PER TIPO (2026-09-12):
     * lo stesso nome "U" indica matrici diverse (LU: upper in c; SVD: left in b).
     * Storage: LU(0) b=L c=U · QR(1) b=Q c=R · SVD(2) b=U c=S a=V ·
     *          EVD(3) b=autovalori(diag) c=autovettori · Cholesky(4) b=L. */
    if (d->type == 0)   /* LU */
    {
        if (strcmp(name, "L") == 0) return d->b;
        if (strcmp(name, "U") == 0) return d->c;
        return -1;
    }
    if (d->type == 1)   /* QR */
    {
        if (strcmp(name, "Q") == 0) return d->b;
        if (strcmp(name, "R") == 0) return d->c;
        return -1;
    }
    if (d->type == 2)   /* SVD */
    {
        if (strcmp(name, "U") == 0) return d->b;
        if (strcmp(name, "S") == 0) return d->c;
        if (strcmp(name, "V") == 0 || strcmp(name, "VT") == 0) return d->a;
        return -1;
    }
    if (d->type == 3)   /* EVD: b=autovalori, c=autovettori */
    {
        if (strcmp(name, "D") == 0 || strcmp(name, "S") == 0) return d->b;
        if (strcmp(name, "V") == 0) return d->c;
        return -1;
    }
    if (d->type == 4)   /* Cholesky */
    {
        if (strcmp(name, "L") == 0) return d->b;
        return -1;
    }
    /* fallback numerico storico */
    if (strcmp(name, "L") == 0 || strcmp(name, "Q") == 0 || strcmp(name, "U") == 0 || strcmp(name, "D") == 0)
        return d->b;
    if (strcmp(name, "R") == 0 || strcmp(name, "V") == 0) return d->c;
    if (strcmp(name, "S") == 0 || strcmp(name, "VT") == 0) return d->c;
    return -1;
}

/* ============================================================================
 * Funzioni LINALG aggiuntive (port di CommonLib/Numerics/LinAlg.cs).
 * CONVENZIONE DI RITORNO: matrice → handle int64_t (nuova matrice allocata);
 * scalare → double; rank → int. Il vettore risultato di LuSolve/Lstsq è una
 * matrice N×1 (in C non esiste un vettore N-generico: gmi_vector_t è il 4D grafico).
 * ============================================================================ */

/* Rango: eliminazione gaussiana con pivot parziale (funziona anche rettangolare). */
int gmi_matrix_rank(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) return -1;
    int m = ma->rows, n = ma->cols;
    if (m == n)
    {
        /* Quadrata: rank via SVD (robusto come matrix_rank numpy). Il Jacobi one-sided
         * del runtime non defla i valori singolari sotto ~1e-9 relativo al massimo →
         * tol = max(m,n)·S[0]·1e-8, non 1e-15 (che darebbe rank pieno per near-singolari). */
        int64_t d = gmi_matrix_svd(a);
        if (d <= 0) return -1;
        int64_t Sh = gmi_matrix_decomp_get_factor(d, 1);
        double sMax = 0;
        for (int si = 0; si < n; si++) { double sv = gmi_matrix_get(Sh, si, si); if (sv > sMax) sMax = sv; }
        double tol = (double)n * sMax * 1e-8;
        int rank = 0;
        for (int i = 0; i < n; i++)
            if (gmi_matrix_get(Sh, i, i) > tol) rank++;
        gmi_matrix_decomp_free(d);
        return rank;
    }
    /* Rettangolare: rank via SVD del prodotto PIÙ PICCOLO (A·Aᵀ m×m se wide, AᵀA n×n se
     * tall) — come matrix_rank numpy. AᵀA su una wide (n>m) è n×n rank-m e il condizionamento
     * peggiora; l'eliminazione gaussiana amplifica il roundoff (residuo ~1e-9). */
    {
        int64_t At = gmi_matrix_transpose(a);
        int64_t AtA = gmi_matrix_multiply(At, a);   /* n×n */
        int64_t AAt = gmi_matrix_multiply(a, At);   /* m×m */
        gmi_matrix_free(At);
        int64_t small = (n <= m) ? AtA : AAt;
        int64_t big = (n <= m) ? AAt : AtA;
        if (small > 0)
        {
            int rank = gmi_matrix_rank(small);
            gmi_matrix_free(small); gmi_matrix_free(big);
            return rank;
        }
        gmi_matrix_free(small); gmi_matrix_free(big);
    }
    /* fallback: eliminazione gaussiana relativa (se AᵀA non costruibile) */
    double* m2 = (double*)malloc((size_t)(m * n) * sizeof(double));
    if (!m2) return -1;
    memcpy(m2, ma->data, (size_t)(m * n) * sizeof(double));
    double scale = 0;
    for (int i = 0; i < m * n; i++)
        if (fabs(m2[i]) > scale) scale = fabs(m2[i]);
    if (scale < 1e-300) { free(m2); return 0; }
    double tol = (double)(m > n ? m : n) * scale * 1e-12;
    int rank = 0;
    for (int col = 0; col < n && rank < m; col++)
    {
        int pivot = -1;
        double maxv = 0;
        for (int r = rank; r < m; r++)
        {
            double v = fabs(m2[r * n + col]);
            if (v > maxv) { maxv = v; pivot = r; }
        }
        if (maxv < tol) continue;
        if (pivot != rank)
            for (int c = col; c < n; c++)
            { double t = m2[rank * n + c]; m2[rank * n + c] = m2[pivot * n + c]; m2[pivot * n + c] = t; }
        double d = m2[rank * n + col];
        for (int r = rank + 1; r < m; r++)
        {
            double f = m2[r * n + col] / d;
            if (f == 0) continue;
            for (int c = col; c < n; c++) m2[r * n + c] -= f * m2[rank * n + c];
        }
        rank++;
    }
    free(m2);
    return rank;
}

/* Traccia: somma diagonale; solo quadrata (altrimenti NaN). */
double gmi_matrix_trace(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) return 0.0;
    if (ma->rows != ma->cols) return 0.0 / 0.0;
    double s = 0;
    for (int i = 0; i < ma->rows; i++) s += ma->data[i * ma->cols + i];
    return s;
}

/* Norma spettrale (largest singular value = sqrt(λ_max(AᵀA))) via power iteration. */
double gmi_matrix_norm(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) return 0.0;
    int m = ma->rows, n = ma->cols;
    if (n <= 0) return 0.0;
    double* ata = (double*)calloc((size_t)(n * n), sizeof(double));
    double* x = (double*)calloc((size_t)n, sizeof(double));
    double* y = (double*)calloc((size_t)n, sizeof(double));
    if (!ata || !x || !y) { free(ata); free(x); free(y); return 0.0; }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            double s = 0;
            for (int k = 0; k < m; k++) s += ma->data[k * n + i] * ma->data[k * n + j];
            ata[i * n + j] = s;
        }
    for (int i = 0; i < n; i++) x[i] = 1.0;
    double lambda = 0;
    for (int it = 0; it < 300; it++)
    {
        for (int i = 0; i < n; i++)
        {
            double s = 0;
            for (int j = 0; j < n; j++) s += ata[i * n + j] * x[j];
            y[i] = s;
        }
        double norm = 0;
        for (int i = 0; i < n; i++) norm += y[i] * y[i];
        norm = sqrt(norm);
        if (norm < 1e-300) { lambda = 0; break; }
        for (int i = 0; i < n; i++) x[i] = y[i] / norm;
        lambda = 0;
        for (int i = 0; i < n; i++) lambda += x[i] * y[i];
    }
    free(ata); free(x); free(y);
    return sqrt(lambda > 0 ? lambda : 0);
}

/* Pseudoinversa Moore-Penrose. Quadrata INVERTIBILE → A⁻¹ (Gauss-Jordan, esatta).
 * Rettangolare → formula normale adatta alla forma: tall (m>=n) A⁺=(AᵀA)⁻¹Aᵀ,
 * wide (m<n) A⁺=Aᵀ(A·Aᵀ)⁻¹ (AᵀA su una wide è singolare). Ritorna handle o -1. */
int64_t gmi_matrix_pseudo_inverse(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) return -1;
    int m = ma->rows, n = ma->cols;
    if (m == n)
    {
        /* Prima il RANK: se full-rank (rank==n) A⁺ = A⁻¹ (Gauss-Jordan esatta). Se
         * singolare, l'inversa di Gauss-Jordan "riesce" ma dà spazzatura (pivot ~1e-9
         * > 1e-12) → serve il fallback SVD. */
        if (gmi_matrix_rank(a) == n)
        {
            int64_t inv = gmi_matrix_inverse(a);
            if (inv > 0) return inv;
        }
        /* quadrata SINGOLARE: A⁺ = V·S⁺·Uᵀ via SVD. U del Jacobi one-sided NON è
         * normalizzata (le colonne hanno norma = valore singolare) → U_ortho = U/σ. */
        int64_t d = gmi_matrix_svd(a);
        if (d <= 0) return -1;
        gmi_decomp_t* dd = dget(d);
        if (!dd) { return -1; }
        int64_t Uh = dd->b, Sh = dd->c, Vh = dd->a;
        double sMax = 0;
        for (int si = 0; si < n; si++) { double sv = gmi_matrix_get(Sh, si, si); if (sv > sMax) sMax = sv; }
        /* tolleranza coerente col rank (1e-8 relativa): il Jacobi one-sided non defla
         * sotto ~1e-9, quindi un sigma "zero" reale arriva a ~1e-9 → va trattato come 0,
         * altrimenti 1/σ esplode. */
        double tol = (double)n * sMax * 1e-8;
        int64_t Uo = gmi_matrix_create(n, n);
        gmi_matrix_t* mUo = mget(Uo);
        int64_t Sp = gmi_matrix_create(n, n);
        gmi_matrix_t* mSp = mget(Sp);
        if (!mUo || !mSp) { gmi_matrix_decomp_free(d); return -1; }
        memset(mUo->data, 0, (size_t)(n * n) * sizeof(double));
        memset(mSp->data, 0, (size_t)(n * n) * sizeof(double));
        for (int j = 0; j < n; j++)
        {
            double s = gmi_matrix_get(Sh, j, j);
            if (s > tol)
            {
                for (int i = 0; i < n; i++) mUo->data[i * n + j] = gmi_matrix_get(Uh, i, j) / s;
                mSp->data[j * n + j] = 1.0 / s;
            }
        }
        int64_t UoT = gmi_matrix_transpose(Uo);
        int64_t VS = gmi_matrix_multiply(Vh, Sp);
        int64_t out = gmi_matrix_multiply(VS, UoT);
        gmi_matrix_decomp_free(d);
        gmi_matrix_free(Uo); gmi_matrix_free(Sp); gmi_matrix_free(UoT); gmi_matrix_free(VS);
        return out;
    }
    int64_t At = gmi_matrix_transpose(a);
    if (m >= n)
    {
        int64_t AtA = gmi_matrix_multiply(At, a);   /* n×n */
        int64_t inv = gmi_matrix_inverse(AtA);
        if (inv > 0)
        {
            int64_t out = gmi_matrix_multiply(inv, At);   /* n×m */
            gmi_matrix_free(At); gmi_matrix_free(AtA); gmi_matrix_free(inv);
            return out;
        }
        gmi_matrix_free(AtA); gmi_matrix_free(inv);
    }
    else
    {
        int64_t AAt = gmi_matrix_multiply(a, At);   /* m×m (full row rank → invertibile) */
        int64_t inv = gmi_matrix_inverse(AAt);
        if (inv > 0)
        {
            int64_t out = gmi_matrix_multiply(At, inv);   /* n×m */
            gmi_matrix_free(At); gmi_matrix_free(AAt); gmi_matrix_free(inv);
            return out;
        }
        gmi_matrix_free(AAt); gmi_matrix_free(inv);
    }
    gmi_matrix_free(At);
    return -1;
}

/* Spazio nullo di A: risolve il sistema omogeneo A·x = 0 via eliminazione gaussiana
 * in forma ridotta (RREF). Le colonne libere (non-pivot) danno la base del kernel.
 * Ritorna matrice n×k (k colonne base) o n×1 di zeri se il kernel è banale. */
int64_t gmi_matrix_null_space(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) return -1;
    int m = ma->rows, n = ma->cols;
    if (n <= 0 || m <= 0) return -1;
    double* m2 = (double*)malloc((size_t)(m * n) * sizeof(double));
    int* pivotCol = (int*)calloc((size_t)m, sizeof(int));
    if (!m2 || !pivotCol) { free(m2); free(pivotCol); return -1; }
    memcpy(m2, ma->data, (size_t)(m * n) * sizeof(double));
    /* Il RREF con gaussiana AMPLIFICA il roundoff (residuo ~1e-9 su matrici near-singolari)
     * → troverebbe pivot spuri oltre il rango vero. Il rango va preso dal rank SVD
     * (gmi_matrix_rank) e il RREF si FERMA lì: le colonne libere = n - rank. */
    int trueRank = gmi_matrix_rank(a);
    if (trueRank < 0) { free(m2); free(pivotCol); return -1; }
    double scale = 0;
    for (int i = 0; i < m * n; i++)
        if (fabs(m2[i]) > scale) scale = fabs(m2[i]);
    double tol = (double)(m > n ? m : n) * scale * 1e-12;
    int rank = 0;
    for (int col = 0; col < n && rank < trueRank; col++)
    {
        int pivot = -1;
        double maxv = 0;
        for (int r = rank; r < m; r++)
        {
            double v = fabs(m2[r * n + col]);
            if (v > maxv) { maxv = v; pivot = r; }
        }
        if (maxv < tol) continue;
        if (pivot != rank)
            for (int c = 0; c < n; c++)
            { double t = m2[rank * n + c]; m2[rank * n + c] = m2[pivot * n + c]; m2[pivot * n + c] = t; }
        double d = m2[rank * n + col];
        for (int c = 0; c < n; c++) m2[rank * n + c] /= d;
        for (int r = 0; r < m; r++)
        {
            if (r == rank) continue;
            double f = m2[r * n + col];
            if (f == 0) continue;
            for (int c = 0; c < n; c++) m2[r * n + c] -= f * m2[rank * n + c];
        }
        pivotCol[rank] = col;
        rank++;
    }
    /* colonne libere → vettori del kernel */
    int freeCount = 0;
    for (int c = 0; c < n; c++)
    {
        int isPivot = 0;
        for (int r = 0; r < rank; r++) if (pivotCol[r] == c) { isPivot = 1; break; }
        if (!isPivot) freeCount++;
    }
    if (freeCount == 0)
    {
        int64_t out = gmi_matrix_create(n, 1);
        gmi_matrix_t* mo = mget(out);
        if (mo) memset(mo->data, 0, (size_t)n * sizeof(double));
        free(m2); free(pivotCol);
        return out;
    }
    int64_t out = gmi_matrix_create(n, freeCount);
    gmi_matrix_t* mo = mget(out);
    if (!mo) { free(m2); free(pivotCol); return -1; }
    memset(mo->data, 0, (size_t)(n * freeCount) * sizeof(double));
    int k = 0;
    for (int c = 0; c < n; c++)
    {
        int isPivot = 0;
        for (int r = 0; r < rank; r++) if (pivotCol[r] == c) { isPivot = 1; break; }
        if (isPivot) continue;
        mo->data[c * freeCount + k] = 1.0;
        for (int r = 0; r < rank; r++)
            mo->data[pivotCol[r] * freeCount + k] = -m2[r * n + c];
        k++;
    }
    /* Normalizza ogni vettore del kernel a lunghezza unitaria (parità MathNet
     * NullSpace, che ritorna una base ORTONORMALE). */
    for (int j = 0; j < freeCount; j++)
    {
        double nrm = 0;
        for (int i = 0; i < n; i++) { double v = mo->data[i * freeCount + j]; nrm += v * v; }
        nrm = sqrt(nrm);
        if (nrm > 1e-15)
            for (int i = 0; i < n; i++) mo->data[i * freeCount + j] /= nrm;
    }
    free(m2); free(pivotCol);
    return out;
}

/* Risolve A·X = B via Gauss-Jordan su [A|B]. A quadrata. Ritorna handle X (n×p).
 * Il vettore b del C# (Vector<double>) è qui una matrice N×1 (o N×M). */
int64_t gmi_matrix_lu_solve(int64_t a, int64_t b)
{
    gmi_matrix_t* ma = mget(a);
    gmi_matrix_t* mb = mget(b);
    if (!ma || !mb || ma->rows != ma->cols || ma->rows != mb->rows) return -1;
    int n = ma->rows, p = mb->cols;
    double* aug = (double*)calloc((size_t)(n * (n + p)), sizeof(double));
    if (!aug) return -1;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++) aug[i * (n + p) + j] = ma->data[i * n + j];
        for (int j = 0; j < p; j++) aug[i * (n + p) + n + j] = mb->data[i * p + j];
    }
    for (int col = 0; col < n; col++)
    {
        int pivot = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(aug[r * (n + p) + col]) > fabs(aug[pivot * (n + p) + col])) pivot = r;
        if (fabs(aug[pivot * (n + p) + col]) < 1e-12) { free(aug); return -1; }
        if (pivot != col)
            for (int c = 0; c < n + p; c++)
            { double t = aug[col * (n + p) + c]; aug[col * (n + p) + c] = aug[pivot * (n + p) + c]; aug[pivot * (n + p) + c] = t; }
        double d = aug[col * (n + p) + col];
        for (int c = 0; c < n + p; c++) aug[col * (n + p) + c] /= d;
        for (int r = 0; r < n; r++)
        {
            if (r == col) continue;
            double f = aug[r * (n + p) + col];
            if (f == 0) continue;
            for (int c = 0; c < n + p; c++) aug[r * (n + p) + c] -= f * aug[col * (n + p) + c];
        }
    }
    int64_t out = gmi_matrix_create(n, p);
    gmi_matrix_t* mo = mget(out);
    if (!mo) { free(aug); return -1; }
    memset(mo->data, 0, (size_t)(n * p) * sizeof(double));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < p; j++)
            mo->data[i * p + j] = aug[i * (n + p) + n + j];
    free(aug);
    return out;
}

/* Prodotto di Kronecker A⊗B. */
int64_t gmi_matrix_kronecker(int64_t a, int64_t b)
{
    gmi_matrix_t* ma = mget(a);
    gmi_matrix_t* mb = mget(b);
    if (!ma || !mb) return -1;
    int ra = ma->rows, ca = ma->cols, rb = mb->rows, cb = mb->cols;
    int64_t out = gmi_matrix_create(ra * rb, ca * cb);
    gmi_matrix_t* mo = mget(out);
    if (!mo) return -1;
    for (int i = 0; i < ra; i++)
        for (int j = 0; j < ca; j++)
        {
            double av = ma->data[i * ca + j];
            if (av == 0) continue;
            for (int k = 0; k < rb; k++)
                for (int l = 0; l < cb; l++)
                    mo->data[(i * rb + k) * (ca * cb) + (j * cb + l)] = av * mb->data[k * cb + l];
        }
    return out;
}

/* Matrice a blocchi diagonale dai blocchi (handle). */
int64_t gmi_matrix_block_diag(const int64_t* handles, int count)
{
    if (!handles || count <= 0) return -1;
    int totalRows = 0, totalCols = 0;
    for (int k = 0; k < count; k++)
    {
        gmi_matrix_t* m = mget(handles[k]);
        if (!m) return -1;
        totalRows += m->rows; totalCols += m->cols;
    }
    int64_t out = gmi_matrix_create(totalRows, totalCols);
    gmi_matrix_t* mo = mget(out);
    if (!mo) return -1;
    memset(mo->data, 0, (size_t)(totalRows * totalCols) * sizeof(double));
    int ro = 0, co = 0;
    for (int k = 0; k < count; k++)
    {
        gmi_matrix_t* m = mget(handles[k]);
        for (int i = 0; i < m->rows; i++)
            for (int j = 0; j < m->cols; j++)
                mo->data[(ro + i) * totalCols + (co + j)] = m->data[i * m->cols + j];
        ro += m->rows; co += m->cols;
    }
    return out;
}

/* diag(A·B) come colonna N×1 (match C# DiagonalOfProduct). */
int64_t gmi_matrix_diag_of_product(int64_t a, int64_t b)
{
    gmi_matrix_t* ma = mget(a);
    gmi_matrix_t* mb = mget(b);
    if (!ma || !mb || ma->cols != mb->rows) return -1;
    int n = ma->rows;
    int64_t out = gmi_matrix_create(n, 1);
    gmi_matrix_t* mo = mget(out);
    if (!mo) return -1;
    memset(mo->data, 0, (size_t)n * sizeof(double));
    for (int i = 0; i < n; i++)
    {
        double s = 0;
        for (int k = 0; k < ma->cols; k++)
            s += ma->data[i * ma->cols + k] * mb->data[k * mb->cols + i];
        mo->data[i] = s;
    }
    return out;
}

/* ── Metriche di distanza tra vettori (VECTOR namespace, scipy.spatial.distance) ── */
static void gmi_vec2(double a, double b, double* va, double* vb)
{
    gmi_vector_get(a, 0, va); gmi_vector_get(a, 1, va + 1);
    gmi_vector_get(a, 2, va + 2); gmi_vector_get(a, 3, va + 3);
    gmi_vector_get(b, 0, vb); gmi_vector_get(b, 1, vb + 1);
    gmi_vector_get(b, 2, vb + 2); gmi_vector_get(b, 3, vb + 3);
}

double gmi_vector_manhattan(int64_t a, int64_t b)
{
    double va[4], vb[4]; gmi_vec2(a, b, va, vb);
    return fabs(va[0]-vb[0]) + fabs(va[1]-vb[1]) + fabs(va[2]-vb[2]) + fabs(va[3]-vb[3]);
}

double gmi_vector_chebyshev(int64_t a, int64_t b)
{
    double va[4], vb[4]; gmi_vec2(a, b, va, vb);
    double m = 0;
    for (int i = 0; i < 4; i++) { double d = fabs(va[i]-vb[i]); if (d > m) m = d; }
    return m;
}

double gmi_vector_cosine(int64_t a, int64_t b)
{
    double va[4], vb[4]; gmi_vec2(a, b, va, vb);
    double dot = 0, na = 0, nb = 0;
    for (int i = 0; i < 4; i++) { dot += va[i]*vb[i]; na += va[i]*va[i]; nb += vb[i]*vb[i]; }
    double den = sqrt(na) * sqrt(nb);
    return den == 0 ? NAN : 1.0 - dot / den;
}

double gmi_vector_minkowski(int64_t a, int64_t b, double p)
{
    if (p <= 0 || isnan(p)) return NAN;
    double va[4], vb[4]; gmi_vec2(a, b, va, vb);
    double s = 0;
    for (int i = 0; i < 4; i++) s += pow(fabs(va[i]-vb[i]), p);
    return pow(s, 1.0 / p);
}

/* ── Funzioni matriciali (MATRIX namespace, scipy.linalg) ── */
 int64_t gmi_matrix_add_scalar(int64_t a, double s)
{
    int r = gmi_matrix_rows(a), c = gmi_matrix_cols(a);
    int64_t out = gmi_matrix_create(r, c);
    for (int i = 0; i < r; i++) for (int j = 0; j < c; j++) gmi_matrix_set(out, i, j, gmi_matrix_get(a, i, j) + s);
    return out;
}
static int64_t gmi_matrix_mul_scalar(int64_t a, double s)
{
    int r = gmi_matrix_rows(a), c = gmi_matrix_cols(a);
    int64_t out = gmi_matrix_create(r, c);
    for (int i = 0; i < r; i++) for (int j = 0; j < c; j++) gmi_matrix_set(out, i, j, gmi_matrix_get(a, i, j) * s);
    return out;
}

/* Esponenziale di matrice (scipy.linalg.expm): scaling-and-squaring + Taylor. */
int64_t gmi_matrix_expm(int64_t h)
{
    int r = gmi_matrix_rows(h), c = gmi_matrix_cols(h);
    if (r != c || r <= 0) return -1;
    double norm = 0;
    for (int i = 0; i < r; i++)
    {
        double row = 0;
        for (int j = 0; j < c; j++) row += fabs(gmi_matrix_get(h, i, j));
        if (row > norm) norm = row;
    }
    int s = 0;
    if (norm > 0) { s = (int)ceil(log2(norm)) + 1; if (s < 0) s = 0; }
    double p2 = pow(2.0, s);
    int64_t scaled = gmi_matrix_mul_scalar(h, 1.0 / p2);
    int64_t result = gmi_matrix_create(r, c); gmi_matrix_identity(result);
    int64_t term = gmi_matrix_create(r, c); gmi_matrix_identity(term);
    for (int k = 1; k <= 25; k++)
    {
        int64_t tmp = gmi_matrix_multiply(term, scaled);
        int64_t tmps = gmi_matrix_mul_scalar(tmp, 1.0 / k); gmi_matrix_free(tmp);
        gmi_matrix_free(term); term = tmps;
        int64_t sum = gmi_matrix_create(r, c);
        for (int i = 0; i < r; i++) for (int j = 0; j < c; j++)
            gmi_matrix_set(sum, i, j, gmi_matrix_get(result, i, j) + gmi_matrix_get(term, i, j));
        gmi_matrix_free(result); result = sum;
    }
    gmi_matrix_free(term); gmi_matrix_free(scaled);
    for (int i = 0; i < s; i++)
    {
        int64_t sq = gmi_matrix_multiply(result, result);
        gmi_matrix_free(result); result = sq;
    }
    return result;
}

/* Prodotto di Kronecker A⊗B (numpy.kron). */
int64_t gmi_matrix_kron(int64_t a, int64_t b)
{
    int ra = gmi_matrix_rows(a), ca = gmi_matrix_cols(a);
    int rb = gmi_matrix_rows(b), cb = gmi_matrix_cols(b);
    int64_t out = gmi_matrix_create(ra * rb, ca * cb);
    for (int i = 0; i < ra; i++) for (int j = 0; j < ca; j++)
    {
        double av = gmi_matrix_get(a, i, j);
        if (av == 0) continue;
        for (int k = 0; k < rb; k++) for (int l = 0; l < cb; l++)
            gmi_matrix_set(out, i * rb + k, j * cb + l, av * gmi_matrix_get(b, k, l));
    }
    return out;
}

/* ── scipy.linalg: sqrtm/logm (via EVD simmetrica) e hessenberg (Householder) ── */

int64_t gmi_matrix_sqrtm(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t decomp = gmi_matrix_evd(a);
    if (decomp <= 0) return -1;
    int64_t V = gmi_matrix_decomp_get_factor(decomp, 1); /* autovettori (d->c) */
    int64_t D = gmi_matrix_decomp_get_factor(decomp, 0); /* autovalori (d->b, diagonale) */
    int64_t out = gmi_matrix_create(n, n);
    if (V <= 0 || D <= 0 || out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            double s = 0;
            for (int k = 0; k < n; k++)
                s += gmi_matrix_get(V, i, k) * sqrt(gmi_matrix_get(D, k, k)) * gmi_matrix_get(V, j, k);
            gmi_matrix_set(out, i, j, s);
        }
    return out;
}

int64_t gmi_matrix_logm(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t decomp = gmi_matrix_evd(a);
    if (decomp <= 0) return -1;
    int64_t V = gmi_matrix_decomp_get_factor(decomp, 1); /* autovettori (d->c) */
    int64_t D = gmi_matrix_decomp_get_factor(decomp, 0); /* autovalori (d->b, diagonale) */
    int64_t out = gmi_matrix_create(n, n);
    if (V <= 0 || D <= 0 || out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            double s = 0;
            for (int k = 0; k < n; k++)
                s += gmi_matrix_get(V, i, k) * log(gmi_matrix_get(D, k, k)) * gmi_matrix_get(V, j, k);
            gmi_matrix_set(out, i, j, s);
        }
    return out;
}

int64_t gmi_matrix_hessenberg(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t out = gmi_matrix_create(n, n);
    if (out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            gmi_matrix_set(out, i, j, gmi_matrix_get(a, i, j));
    if (n <= 2) return out;
    double* a2 = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    if (!a2) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            a2[i * n + j] = gmi_matrix_get(out, i, j);
    for (int k = 0; k < n - 2; k++)
    {
        int m = n - k - 1;
        double scale = 0;
        for (int i = 0; i < m; i++) scale += fabs(a2[(k + 1 + i) * n + k]);
        if (scale < 1e-300) continue;
        double* u = (double*)malloc(sizeof(double) * (size_t)m);
        if (!u) { free(a2); return -1; }
        for (int i = 0; i < m; i++) u[i] = a2[(k + 1 + i) * n + k] / scale;
        double sum = 0;
        for (int i = 0; i < m; i++) sum += u[i] * u[i];
        double alpha = u[0] >= 0 ? -sqrt(sum) : sqrt(sum);
        u[0] -= alpha;
        double h = 0;
        for (int i = 0; i < m; i++) h += u[i] * u[i];
        if (h < 1e-300) { free(u); continue; }
        h *= 0.5;
        /* A' = (I - u u^T/h) A (I - u u^T/h)  — colonne k..n-1, righe 0..n-1 */
        for (int j = k; j < n; j++)
        {
            double s = 0;
            for (int i = 0; i < m; i++) s += a2[(k + 1 + i) * n + j] * u[i];
            s /= h;
            for (int i = 0; i < m; i++) a2[(k + 1 + i) * n + j] -= s * u[i];
        }
        for (int i = 0; i < n; i++)
        {
            double s = 0;
            for (int j = 0; j < m; j++) s += a2[i * n + (k + 1 + j)] * u[j];
            s /= h;
            for (int j = 0; j < m; j++) a2[i * n + (k + 1 + j)] -= s * u[j];
        }
        free(u);
    }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            gmi_matrix_set(out, i, j, a2[i * n + j]);
    free(a2);
    return out;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Completamento scipy.linalg: funzioni matriciali via EVD, SVD-based,
 * costruttori, solve_triangular, eig generale non simmetrica e real Schur.
 * ════════════════════════════════════════════════════════════════════════════ */

static double gmi_sign_of(double x) { return x == 0 ? 0.0 : (x > 0 ? 1.0 : -1.0); }

static int64_t gmi_matrix_eigenwise(int64_t a, double (*f)(double))
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t decomp = gmi_matrix_evd(a);
    if (decomp <= 0) return -1;
    int64_t V = gmi_matrix_decomp_get_factor(decomp, 1); /* autovettori */
    int64_t D = gmi_matrix_decomp_get_factor(decomp, 0); /* autovalori (diag) */
    int64_t out = gmi_matrix_create(n, n);
    if (V <= 0 || D <= 0 || out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            double s = 0;
            for (int k = 0; k < n; k++)
                s += gmi_matrix_get(V, i, k) * f(gmi_matrix_get(D, k, k)) * gmi_matrix_get(V, j, k);
            gmi_matrix_set(out, i, j, s);
        }
    return out;
}

int64_t gmi_matrix_cosm(int64_t a) { return gmi_matrix_eigenwise(a, cos); }
int64_t gmi_matrix_sinm(int64_t a) { return gmi_matrix_eigenwise(a, sin); }
int64_t gmi_matrix_tanm(int64_t a) { return gmi_matrix_eigenwise(a, tan); }
int64_t gmi_matrix_signm(int64_t a) { return gmi_matrix_eigenwise(a, gmi_sign_of); }

int64_t gmi_matrix_fractional_power(int64_t a, double p)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t decomp = gmi_matrix_evd(a);
    if (decomp <= 0) return -1;
    int64_t V = gmi_matrix_decomp_get_factor(decomp, 1);
    int64_t D = gmi_matrix_decomp_get_factor(decomp, 0);
    int64_t out = gmi_matrix_create(n, n);
    if (V <= 0 || D <= 0 || out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            double s = 0;
            for (int k = 0; k < n; k++)
                s += gmi_matrix_get(V, i, k) * pow(gmi_matrix_get(D, k, k), p) * gmi_matrix_get(V, j, k);
            gmi_matrix_set(out, i, j, s);
        }
    return out;
}

/* ---- SVD-based: orth, subspace_angles, polar, cond, nuclear ---- */

int64_t gmi_matrix_orth(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) return -1;
    int n = ma->rows;
    int64_t d = gmi_matrix_svd(a);
    if (d <= 0) return -1;
    int64_t U = gmi_matrix_decomp_get_factor(d, 0);
    int64_t S = gmi_matrix_decomp_get_factor(d, 1);
    if (U <= 0 || S <= 0) return -1;
    double tol = (double)n * 2.220446049250313e-16 * fabs(gmi_matrix_get(S, 0, 0));
    double* sv = (double*)malloc(sizeof(double) * (size_t)n);
    if (!sv) return -1;
    for (int j = 0; j < n; j++) sv[j] = gmi_matrix_get(S, j, j);
    /* ordina per valore singolare DISCENDENTE (convenzione scipy), riordinando le colonne di U */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (sv[i] < sv[j])
            {
                double ts = sv[i]; sv[i] = sv[j]; sv[j] = ts;
                for (int k = 0; k < n; k++)
                {
                    double tu = gmi_matrix_get(U, k, i);
                    gmi_matrix_set(U, k, i, gmi_matrix_get(U, k, j));
                    gmi_matrix_set(U, k, j, tu);
                }
            }
    int rank = 0;
    for (int j = 0; j < n; j++)
        if (sv[j] > tol) rank++;
    int64_t out = gmi_matrix_create(n, rank);
    if (out <= 0) { free(sv); return -1; }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < rank; j++)
            gmi_matrix_set(out, i, j, gmi_matrix_get(U, i, j) / sv[j]); /* normalizza */
    free(sv);
    return out;
}

double gmi_matrix_cond(int64_t a)
{
    int64_t d = gmi_matrix_svd(a);
    if (d <= 0) return -1;
    int64_t S = gmi_matrix_decomp_get_factor(d, 1);
    if (S <= 0) return -1;
    int n = gmi_matrix_rows(a);
    double smax = gmi_matrix_get(S, 0, 0), smin = gmi_matrix_get(S, 0, 0);
    for (int j = 1; j < n; j++)
    {
        double v = gmi_matrix_get(S, j, j);
        if (v > smax) smax = v;
        if (v < smin) smin = v;
    }
    if (smin < 1e-300) return 1.0 / 0.0;
    return smax / smin;
}

double gmi_matrix_nuclear_norm(int64_t a)
{
    int64_t d = gmi_matrix_svd(a);
    if (d <= 0) return -1;
    int64_t S = gmi_matrix_decomp_get_factor(d, 1);
    if (S <= 0) return -1;
    int n = gmi_matrix_rows(a);
    double sum = 0;
    for (int j = 0; j < n; j++) sum += gmi_matrix_get(S, j, j);
    return sum;
}

int64_t gmi_matrix_polar(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    int64_t d = gmi_matrix_svd(a);
    if (d <= 0) return -1;
    int64_t U = gmi_matrix_decomp_get_factor(d, 0);
    int64_t S = gmi_matrix_decomp_get_factor(d, 1);
    int64_t V = gmi_matrix_decomp_get_factor(d, 2);
    if (U <= 0 || S <= 0 || V <= 0) return -1;
    int64_t Vt = gmi_matrix_transpose(V);
    if (Vt <= 0) return -1;
    /* U dell'SVD C è SCALATO per colonna (U_col·s_j): normalizza con S^{-1} */
    int64_t Sinv = gmi_matrix_create(n, n);
    for (int j = 0; j < n; j++)
        gmi_matrix_set(Sinv, j, j, 1.0 / gmi_matrix_get(S, j, j));
    int64_t Un = gmi_matrix_multiply(U, Sinv);   /* U ortogonale */
    int64_t Uu = gmi_matrix_multiply(Un, Vt);    /* U·V^T */
    int64_t VS = gmi_matrix_multiply(V, S);      /* V·S */
    int64_t P = gmi_matrix_multiply(VS, Vt);     /* V·S·V^T */
    if (Uu <= 0 || VS <= 0 || P <= 0) return -1;
    int64_t dec = dalloc(5, a, n);
    if (dec <= 0) return -1;
    gmi_decomp_t* dd = dget(dec);
    dd->b = Uu;   /* U */
    dd->c = P;    /* P */
    return dec;
}

/* ---- costruttori: toeplitz, hankel, circulant ---- */

int64_t gmi_matrix_toeplitz(const double* c, const double* r, int n, int m)
{
    int64_t out = gmi_matrix_create(n, m);
    if (out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
            gmi_matrix_set(out, i, j, i >= j ? c[i - j] : r[j - i]);
    return out;
}

int64_t gmi_matrix_hankel(const double* c, const double* r, int n, int m)
{
    int64_t out = gmi_matrix_create(n, m);
    if (out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
        {
            int k = i + j;
            gmi_matrix_set(out, i, j, k < n ? c[k] : r[k - n + 1]);
        }
    return out;
}

int64_t gmi_matrix_circulant(const double* c, int n)
{
    int64_t out = gmi_matrix_create(n, n);
    if (out <= 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            gmi_matrix_set(out, i, j, c[((i - j + n) % n)]);
    return out;
}

/* ---- solve_triangular ---- */

int64_t gmi_matrix_solve_triangular(int64_t a, const double* b, int nb, int lower)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols || ma->rows != nb) return -1;
    int n = ma->rows;
    int64_t out = gmi_matrix_create(n, 1); /* vettore colonna N×1 */
    if (out <= 0) return -1;
    double* x = (double*)malloc(sizeof(double) * (size_t)n);
    if (!x) return -1;
    if (lower)
    {
        for (int i = 0; i < n; i++)
        {
            double s = 0;
            for (int j = 0; j < i; j++) s += gmi_matrix_get(a, i, j) * x[j];
            x[i] = (b[i] - s) / gmi_matrix_get(a, i, i);
        }
    }
    else
    {
        for (int i = n - 1; i >= 0; i--)
        {
            double s = 0;
            for (int j = i + 1; j < n; j++) s += gmi_matrix_get(a, i, j) * x[j];
            x[i] = (b[i] - s) / gmi_matrix_get(a, i, i);
        }
    }
    for (int i = 0; i < n; i++) gmi_matrix_set(out, i, 0, x[i]);
    free(x);
    return out;
}

/* ---- EIG generale (non simmetrica) + real Schur: orthes + hqr2 + backsub ---- */

static void gmi_cdiv(double xr, double xi, double yr, double yi, double* cr, double* ci)
{
    double r, d;
    if (fabs(yr) > fabs(yi))
    {
        r = yi / yr;
        d = yr + r * yi;
        *cr = (xr + r * xi) / d;
        *ci = (xi - r * xr) / d;
    }
    else
    {
        r = yr / yi;
        d = yi + r * yr;
        *cr = (r * xr + xi) / d;
        *ci = (r * xi - xr) / d;
    }
}

#define GMB_EPS 2.220446049250313e-16

static int gmi_schur_core(double* h, int n, double* v, double* d, double* e)
{
    int low = 0, high = n - 1;
    double* orts = (double*)malloc(sizeof(double) * (size_t)n);
    if (!orts) return 0;
    /* orthes */
    for (int mm = low + 1; mm <= high - 1; mm++)
    {
        double scale = 0;
        for (int i = mm; i <= high; i++) scale += fabs(h[i * n + (mm - 1)]);
        if (scale != 0.0)
        {
            double hh = 0;
            for (int i = high; i >= mm; i--) { orts[i] = h[i * n + (mm - 1)] / scale; hh += orts[i] * orts[i]; }
            double g = sqrt(hh);
            if (orts[mm] > 0) g = -g;
            hh -= orts[mm] * g;
            orts[mm] -= g;
            for (int j = mm; j < n; j++)
            {
                double f = 0;
                for (int i = high; i >= mm; i--) f += orts[i] * h[i * n + j];
                f = f / hh;
                for (int i = mm; i <= high; i++) h[i * n + j] -= f * orts[i];
            }
            for (int i = 0; i <= high; i++)
            {
                double f = 0;
                for (int j = high; j >= mm; j--) f += orts[j] * h[i * n + j];
                f = f / hh;
                for (int j = mm; j <= high; j++) h[i * n + j] -= f * orts[j];
            }
            orts[mm] = scale * orts[mm];
            h[mm * n + (mm - 1)] = scale * g;
        }
    }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            v[i * n + j] = i == j ? 1.0 : 0.0;
    for (int mm = high - 1; mm >= low + 1; mm--)
    {
        if (h[mm * n + (mm - 1)] != 0.0)
        {
            for (int i = mm + 1; i <= high; i++) orts[i] = h[i * n + (mm - 1)];
            for (int j = mm; j <= high; j++)
            {
                double g = 0;
                for (int i = mm; i <= high; i++) g += orts[i] * v[i * n + j];
                g = (g / orts[mm]) / h[mm * n + (mm - 1)];
                for (int i = mm; i <= high; i++) v[i * n + j] += g * orts[i];
            }
        }
    }
    free(orts);
    /* hqr2 */
    double anorm = 0;
    for (int i = 0; i < n; i++)
        for (int j = (i - 1 > 0 ? i - 1 : 0); j < n; j++)
            anorm += fabs(h[i * n + j]);
    int nn = n - 1;
    double t = 0;
    double s = 0, p = 0, q = 0, r = 0, x = 0, y = 0, w = 0, z = 0;
    int total = 0, ok = 1;
    while (nn >= 0)
    {
        int its = 0;
        int l;
        while (1)
        {
            for (l = nn; l > low; l--)
            {
                s = fabs(h[(l - 1) * n + (l - 1)]) + fabs(h[l * n + l]);
                if (s == 0.0) s = anorm;
                if (fabs(h[l * n + (l - 1)]) <= GMB_EPS * s) { h[l * n + (l - 1)] = 0; break; }
            }
            x = h[nn * n + nn];
            if (l == nn) { d[nn] = x + t; e[nn] = 0; nn--; break; }
            y = h[(nn - 1) * n + (nn - 1)];
            w = h[(nn - 1) * n + nn] * h[nn * n + (nn - 1)];
            if (l == nn - 1)
            {
                p = 0.5 * (y - x);
                q = p * p + w;
                z = sqrt(fabs(q));
                x += t;
                if (q >= 0.0)
                {
                    z = p >= 0 ? p + z : p - z;
                    d[nn - 1] = x + z;
                    d[nn] = d[nn - 1];
                    if (z != 0.0) d[nn] = x - w / z;
                    e[nn - 1] = 0;
                    e[nn] = 0;
                    x = h[nn * n + (nn - 1)];
                    s = fabs(x) + fabs(z);
                    p = x / s;
                    q = z / s;
                    r = sqrt(p * p + q * q);
                    p /= r;
                    q /= r;
                    for (int j = nn - 1; j < n; j++)
                    {
                        z = h[(nn - 1) * n + j];
                        h[(nn - 1) * n + j] = q * z + p * h[nn * n + j];
                        h[nn * n + j] = q * h[nn * n + j] - p * z;
                    }
                    for (int i = 0; i <= nn; i++)
                    {
                        z = h[i * n + (nn - 1)];
                        h[i * n + (nn - 1)] = q * z + p * h[i * n + nn];
                        h[i * n + nn] = q * h[i * n + nn] - p * z;
                    }
                    for (int i = low; i <= high; i++)
                    {
                        z = v[i * n + (nn - 1)];
                        v[i * n + (nn - 1)] = q * z + p * v[i * n + nn];
                        v[i * n + nn] = q * v[i * n + nn] - p * z;
                    }
                }
                else
                {
                    d[nn - 1] = x + p;
                    d[nn] = x + p;
                    e[nn - 1] = z;
                    e[nn] = -z;
                }
                nn -= 2;
                break;
            }
            if (its == 30) { ok = 0; break; }
            if (its == 10 || its == 20)
            {
                t += x;
                for (int i = low; i <= nn; i++) h[i * n + i] -= x;
                s = fabs(h[nn * n + (nn - 1)]) + fabs(h[(nn - 1) * n + (nn - 2)]);
                x = 0.75 * s;
                y = x;
                w = -0.4375 * s * s;
            }
            its++;
            total++;
            if (total > 30 * n) { ok = 0; break; }
            int m = nn - 2;
            for (; m >= l; m--)
            {
                z = h[m * n + m];
                r = x - z;
                s = y - z;
                p = (r * s - w) / h[(m + 1) * n + m] + h[m * n + (m + 1)];
                q = h[(m + 1) * n + (m + 1)] - z - r - s;
                r = h[(m + 2) * n + (m + 1)];
                s = fabs(p) + fabs(q) + fabs(r);
                p /= s;
                q /= s;
                r /= s;
                if (m == l) break;
                if (fabs(h[m * n + (m - 1)]) * (fabs(q) + fabs(r)) <=
                    GMB_EPS * fabs(p) * (fabs(h[(m - 1) * n + (m - 1)]) + fabs(z) + fabs(h[(m + 1) * n + (m + 1)])))
                    break;
            }
            for (int i = m + 2; i <= nn; i++)
            {
                h[i * n + (i - 2)] = 0;
                if (i != m + 2) h[i * n + (i - 3)] = 0;
            }
            for (int k = m; k <= nn - 1; k++)
            {
                int notlast = k != nn - 1;
                if (k != m)
                {
                    p = h[k * n + (k - 1)];
                    q = h[(k + 1) * n + (k - 1)];
                    r = notlast ? h[(k + 2) * n + (k - 1)] : 0;
                    x = fabs(p) + fabs(q) + fabs(r);
                    if (x == 0.0) continue;
                    p /= x;
                    q /= x;
                    r /= x;
                }
                s = sqrt(p * p + q * q + r * r);
                if (p < 0) s = -s;
                if (s != 0.0)
                {
                    if (k != m) h[k * n + (k - 1)] = -s * x;
                    else if (l != m) h[k * n + (k - 1)] = -h[k * n + (k - 1)];
                    p += s;
                    x = p / s;
                    y = q / s;
                    z = r / s;
                    q /= p;
                    r /= p;
                    for (int j = k; j <= nn; j++)
                    {
                        p = h[k * n + j] + q * h[(k + 1) * n + j];
                        if (notlast)
                        {
                            p += r * h[(k + 2) * n + j];
                            h[(k + 2) * n + j] -= p * z;
                        }
                        h[k * n + j] -= p * x;
                        h[(k + 1) * n + j] -= p * y;
                    }
                    for (int i = 0; i <= nn; i++)
                    {
                        p = x * h[i * n + k] + y * h[i * n + (k + 1)];
                        if (notlast)
                        {
                            p += z * h[i * n + (k + 2)];
                            h[i * n + (k + 2)] -= p * r;
                        }
                        h[i * n + k] -= p;
                        h[i * n + (k + 1)] -= p * q;
                    }
                    for (int i = low; i <= high; i++)
                    {
                        p = x * v[i * n + k] + y * v[i * n + (k + 1)];
                        if (notlast)
                        {
                            p += z * v[i * n + (k + 2)];
                            v[i * n + (k + 2)] -= p * r;
                        }
                        v[i * n + k] -= p;
                        v[i * n + (k + 1)] -= p * q;
                    }
                }
            }
        }
        if (!ok) break;
    }
    return ok;
}

static void gmi_backsub(double* t, double* z, const double* d, const double* e, int n,
    double* wre, double* wim)
{
    double anorm = 0;
    for (int i = 0; i < n; i++)
        for (int j = (i - 1 > 0 ? i - 1 : 0); j < n; j++)
            anorm += fabs(t[i * n + j]);
    double zr = 0, rr = 0, sr = 0, ra = 0, sa = 0;
    for (int nn = n - 1; nn >= 0; nn--)
    {
        double p = d[nn], q = e[nn];
        if (q == 0)
        {
            int l = nn;
            t[nn * n + nn] = 1.0;
            for (int i = nn - 1; i >= 0; i--)
            {
                double w = t[i * n + i] - p;
                double r = 0;
                for (int j = l; j <= nn; j++) r += t[i * n + j] * t[j * n + nn];
                if (e[i] < 0) { zr = w; sr = r; }
                else
                {
                    l = i;
                    if (e[i] == 0.0)
                    {
                        t[i * n + nn] = w != 0 ? -r / w : -r / (GMB_EPS * anorm);
                    }
                    else
                    {
                        double x = t[i * n + (i + 1)];
                        double y = t[(i + 1) * n + i];
                        double q2 = (d[i] - p) * (d[i] - p) + e[i] * e[i];
                        double tv = (x * sr - zr * r) / q2;
                        t[i * n + nn] = tv;
                        if (fabs(x) > fabs(zr)) t[(i + 1) * n + nn] = (-r - w * tv) / x;
                        else t[(i + 1) * n + nn] = (-sr - y * tv) / zr;
                    }
                    double sc = fabs(t[i * n + nn]);
                    if ((GMB_EPS * sc) * sc > 1)
                        for (int j = i; j <= nn; j++) t[j * n + nn] /= sc;
                }
            }
        }
        else if (q < 0)
        {
            int l = nn - 1;
            if (fabs(t[nn * n + (nn - 1)]) > fabs(t[(nn - 1) * n + nn]))
            {
                t[(nn - 1) * n + (nn - 1)] = q / t[nn * n + (nn - 1)];
                t[(nn - 1) * n + nn] = -(t[nn * n + nn] - p) / t[nn * n + (nn - 1)];
            }
            else
            {
                gmi_cdiv(0, -t[(nn - 1) * n + nn], t[(nn - 1) * n + (nn - 1)] - p, q,
                    &t[(nn - 1) * n + (nn - 1)], &t[(nn - 1) * n + nn]);
            }
            t[nn * n + (nn - 1)] = 0;
            t[nn * n + nn] = 1.0;
            for (int i = nn - 2; i >= 0; i--)
            {
                ra = 0;
                sa = 0;
                for (int j = l; j <= nn; j++)
                {
                    ra += t[i * n + j] * t[j * n + (nn - 1)];
                    sa += t[i * n + j] * t[j * n + nn];
                }
                double w = t[i * n + i] - p;
                if (e[i] < 0) { zr = w; rr = ra; sr = sa; }
                else
                {
                    l = i;
                    if (e[i] == 0)
                    {
                        gmi_cdiv(-ra, -sa, w, q, &t[i * n + (nn - 1)], &t[i * n + nn]);
                    }
                    else
                    {
                        double x = t[i * n + (i + 1)];
                        double y = t[(i + 1) * n + i];
                        double vr2 = (d[i] - p) * (d[i] - p) + e[i] * e[i] - q * q;
                        double vi2 = (d[i] - p) * 2.0 * q;
                        if (vr2 == 0 && vi2 == 0)
                            vr2 = GMB_EPS * anorm * (fabs(w) + fabs(q) + fabs(x) + fabs(y) + fabs(zr));
                        gmi_cdiv(x * rr - zr * ra + q * sa, x * sr - zr * sa - q * ra, vr2, vi2,
                            &t[i * n + (nn - 1)], &t[i * n + nn]);
                        if (fabs(x) > fabs(zr) + fabs(q))
                        {
                            t[(i + 1) * n + (nn - 1)] = (-ra - w * t[i * n + (nn - 1)] + q * t[i * n + nn]) / x;
                            t[(i + 1) * n + nn] = (-sa - w * t[i * n + nn] - q * t[i * n + (nn - 1)]) / x;
                        }
                        else
                        {
                            gmi_cdiv(-rr - y * t[i * n + (nn - 1)], -sr - y * t[i * n + nn], zr, q,
                                &t[(i + 1) * n + (nn - 1)], &t[(i + 1) * n + nn]);
                        }
                    }
                    double sc = fabs(t[i * n + (nn - 1)]) > fabs(t[i * n + nn])
                        ? fabs(t[i * n + (nn - 1)]) : fabs(t[i * n + nn]);
                    if ((GMB_EPS * sc) * sc > 1)
                        for (int j = i; j <= nn; j++)
                        {
                            t[j * n + (nn - 1)] /= sc;
                            t[j * n + nn] /= sc;
                        }
                }
            }
        }
    }
    /* Riallineamento colonne per coppie complesse, poi W = Z·W. */
    double* revec = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    double* imvec = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    if (!revec || !imvec) { free(revec); free(imvec); return; }
    for (int j = 0; j < n; j++)
        for (int k = 0; k < n; k++)
        {
            if (e[j] == 0) { revec[k * n + j] = t[k * n + j]; imvec[k * n + j] = 0; }
            else if (e[j] < 0) { revec[k * n + j] = t[k * n + (j - 1)]; imvec[k * n + j] = -t[k * n + j]; }
            else { revec[k * n + j] = t[k * n + j]; imvec[k * n + j] = t[k * n + (j + 1)]; }
        }
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
        {
            double rv = 0, iv = 0;
            for (int k = 0; k < n; k++)
            {
                rv += z[i * n + k] * revec[k * n + j];
                iv += z[i * n + k] * imvec[k * n + j];
            }
            wre[i * n + j] = rv;
            wim[i * n + j] = iv;
        }
    free(revec);
    free(imvec);
}

static int gmi_matrix_eig_core(int64_t a, double** dreal, double** dimag, double** vre, double** vim)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    double* h = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    double* v = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    double* d = (double*)malloc(sizeof(double) * (size_t)n);
    double* e = (double*)malloc(sizeof(double) * (size_t)n);
    if (!h || !v || !d || !e) { free(h); free(v); free(d); free(e); return -1; }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            h[i * n + j] = gmi_matrix_get(a, i, j);
    int ok = gmi_schur_core(h, n, v, d, e);
    if (!ok) { free(h); free(v); free(d); free(e); return -1; }
    double* wre = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    double* wim = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    if (!wre || !wim) { free(h); free(v); free(d); free(e); free(wre); free(wim); return -1; }
    gmi_backsub(h, v, d, e, n, wre, wim);
    *dreal = d;
    *dimag = e;
    *vre = wre;
    *vim = wim;
    free(h);
    free(v);
    return 0;
}

int64_t gmi_matrix_eig(int64_t a)
{
    double *dr, *di, *vr, *vi;
    gmi_matrix_t* ma = mget(a);
    if (!ma) return -1;
    int n = ma->rows;
    if (gmi_matrix_eig_core(a, &dr, &di, &vr, &vi) != 0) return -1;
    int64_t dec = dalloc(6, a, n);
    if (dec <= 0) return -1;
    gmi_decomp_t* dd = dget(dec);
    int64_t R = gmi_matrix_create(n, n);
    int64_t I = gmi_matrix_create(n, n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            gmi_matrix_set(R, i, j, vr[i * n + j]);
            gmi_matrix_set(I, i, j, vi[i * n + j]);
        }
    dd->b = R;   /* VRREAL */
    dd->c = I;   /* VRIMAG */
    for (int j = 0; j < n; j++)
    {
        dd->vals[j] = dr[j];
        dd->vals[n + j] = di[j];
    }
    free(dr); free(di); free(vr); free(vi);
    return dec;
}

int64_t gmi_matrix_schur(int64_t a)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma || ma->rows != ma->cols) return -1;
    int n = ma->rows;
    double* h = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    double* v = (double*)malloc(sizeof(double) * (size_t)n * (size_t)n);
    double* d = (double*)malloc(sizeof(double) * (size_t)n);
    double* e = (double*)malloc(sizeof(double) * (size_t)n);
    if (!h || !v || !d || !e) { free(h); free(v); free(d); free(e); return -1; }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            h[i * n + j] = gmi_matrix_get(a, i, j);
    int ok = gmi_schur_core(h, n, v, d, e);
    if (!ok) { free(h); free(v); free(d); free(e); return -1; }
    int64_t dec = dalloc(7, a, n);
    if (dec <= 0) { free(h); free(v); free(d); free(e); return -1; }
    gmi_decomp_t* dd = dget(dec);
    int64_t T = gmi_matrix_create(n, n);
    int64_t Z = gmi_matrix_create(n, n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            gmi_matrix_set(T, i, j, h[i * n + j]);
            gmi_matrix_set(Z, i, j, v[i * n + j]);
        }
    dd->b = T;   /* T */
    dd->c = Z;   /* Z */
    free(h); free(v); free(d); free(e);
    return dec;
}

/* ---- subspace_angles (ritorna il numero di angoli; out allocato) ---- */
int gmi_matrix_subspace_angles(int64_t a, int64_t b, double** out)
{
    int64_t oa = gmi_matrix_orth(a);
    int64_t ob = gmi_matrix_orth(b);
    if (oa <= 0 || ob <= 0) return -1;
    int na = gmi_matrix_rows(oa);
    int ca = gmi_matrix_cols(oa), cb = gmi_matrix_cols(ob);
    if (ca != na) return -1;
    int64_t oat = gmi_matrix_transpose(oa);
    int64_t c = gmi_matrix_multiply(oat, ob);   /* Qa^T · Qb */
    if (c <= 0) return -1;
    int64_t d = gmi_matrix_svd(c);
    if (d <= 0) return -1;
    int64_t S = gmi_matrix_decomp_get_factor(d, 1);
    int len = ca < cb ? ca : cb;
    double* angles = (double*)malloc(sizeof(double) * (size_t)len);
    if (!angles) return -1;
    for (int i = 0; i < len; i++)
    {
        double sv = gmi_matrix_get(S, i, i);
        if (sv > 1.0) sv = 1.0;
        if (sv < -1.0) sv = -1.0;
        angles[i] = acos(sv);
    }
    *out = angles;
    return len;
}

/* ---- varianti _alloc per il backend C99 (ritorno array + lunghezza compagna) ---- */
double* gmi_matrix_subspace_angles_alloc(int64_t a, int64_t b, int64_t* outn)
{
    double* angles = NULL;
    int n = gmi_matrix_subspace_angles(a, b, &angles);
    if (n < 0) { *outn = 0; return NULL; }
    *outn = n;
    return angles;
}

double* gmi_matrix_solve_triangular_alloc(int64_t a, const double* b, int nb, int lower, int64_t* outn)
{
    int64_t sol = gmi_matrix_solve_triangular(a, b, nb, lower);
    if (sol <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)nb);
    if (!out) { *outn = 0; return NULL; }
    for (int i = 0; i < nb; i++) out[i] = gmi_matrix_get(sol, i, 0);
    *outn = nb;
    return out;
}

/* SOLVE(A, b): risolve A·x = b con b array grezzo; ritorna x come array (outn = righe di A). */
double* gmi_matrix_solve_alloc(int64_t a, const double* b, int nb, int64_t* outn)
{
    gmi_matrix_t* ma = mget(a);
    if (!ma) { *outn = 0; return NULL; }
    int n = ma->rows;
    int64_t bh = gmi_matrix_create(nb, 1);
    if (bh <= 0) { *outn = 0; return NULL; }
    for (int i = 0; i < nb; i++) gmi_matrix_set(bh, i, 0, b[i]);
    int64_t sol = gmi_matrix_lu_solve(a, bh);
    gmi_matrix_free(bh);
    if (sol <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { *outn = 0; return NULL; }
    for (int i = 0; i < n; i++) out[i] = gmi_matrix_get(sol, i, 0);
    *outn = n;
    return out;
}

double* gmi_matrix_row_sums_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)rows);
    if (!out) { *outn = 0; return NULL; }
    for (int r = 0; r < rows; r++) {
        double s = 0.0;
        for (int c = 0; c < cols; c++) s += gmi_matrix_get(h, r, c);
        out[r] = s;
    }
    *outn = rows;
    return out;
}

double* gmi_matrix_row_means_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)rows);
    if (!out) { *outn = 0; return NULL; }
    for (int r = 0; r < rows; r++) {
        double s = 0.0;
        for (int c = 0; c < cols; c++) s += gmi_matrix_get(h, r, c);
        out[r] = s / (double)cols;
    }
    *outn = rows;
    return out;
}

double* gmi_matrix_col_means_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)cols);
    if (!out) { *outn = 0; return NULL; }
    for (int c = 0; c < cols; c++) {
        double s = 0.0;
        for (int r = 0; r < rows; r++) s += gmi_matrix_get(h, r, c);
        out[c] = s / (double)rows;
    }
    *outn = cols;
    return out;
}

double* gmi_matrix_col_sums_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)cols);
    if (!out) { *outn = 0; return NULL; }
    for (int c = 0; c < cols; c++) {
        double s = 0.0;
        for (int r = 0; r < rows; r++) s += gmi_matrix_get(h, r, c);
        out[c] = s;
    }
    *outn = cols;
    return out;
}

double* gmi_matrix_row_variances_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)rows);
    if (!out) { *outn = 0; return NULL; }
    for (int r = 0; r < rows; r++) {
        double s = 0.0;
        for (int c = 0; c < cols; c++) s += gmi_matrix_get(h, r, c);
        double mean = s / (double)cols, v = 0.0;
        for (int c = 0; c < cols; c++) { double d = gmi_matrix_get(h, r, c) - mean; v += d * d; }
        out[r] = v / (double)cols;
    }
    *outn = rows;
    return out;
}

double* gmi_matrix_col_variances_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)cols);
    if (!out) { *outn = 0; return NULL; }
    for (int c = 0; c < cols; c++) {
        double s = 0.0;
        for (int r = 0; r < rows; r++) s += gmi_matrix_get(h, r, c);
        double mean = s / (double)rows, v = 0.0;
        for (int r = 0; r < rows; r++) { double d = gmi_matrix_get(h, r, c) - mean; v += d * d; }
        out[c] = v / (double)rows;
    }
    *outn = cols;
    return out;
}

double* gmi_matrix_argmax_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * 2);
    if (!out) { *outn = 0; return NULL; }
    double best = gmi_matrix_get(h, 0, 0); out[0] = 0; out[1] = 0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        double v = gmi_matrix_get(h, r, c);
        if (v > best) { best = v; out[0] = r; out[1] = c; }
    }
    *outn = 2;
    return out;
}

double* gmi_matrix_argmin_alloc(int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * 2);
    if (!out) { *outn = 0; return NULL; }
    double best = gmi_matrix_get(h, 0, 0); out[0] = 0; out[1] = 0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        double v = gmi_matrix_get(h, r, c);
        if (v < best) { best = v; out[0] = r; out[1] = c; }
    }
    *outn = 2;
    return out;
}

double gmi_matrix_frobenius_norm(int64_t h)
{
    gmi_matrix_t* m = mget(h);
    if (!m) return 0.0;
    return gc_array_norm2(m->data, m->rows * m->cols);   /* SIMD */
}

double gmi_matrix_infinity_norm(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    double best = 0.0;
    for (int r = 0; r < rows; r++) {
        double s = 0.0;
        for (int c = 0; c < cols; c++) s += fabs(gmi_matrix_get(h, r, c));
        if (s > best) best = s;
    }
    return best;
}

double gmi_matrix_one_norm(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    double best = 0.0;
    for (int c = 0; c < cols; c++) {
        double s = 0.0;
        for (int r = 0; r < rows; r++) s += fabs(gmi_matrix_get(h, r, c));
        if (s > best) best = s;
    }
    return best;
}

double gmi_matrix_max_value(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    double best = rows > 0 && cols > 0 ? gmi_matrix_get(h, 0, 0) : 0.0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) { double v = gmi_matrix_get(h, r, c); if (v > best) best = v; }
    return best;
}

double gmi_matrix_min_value(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    double best = rows > 0 && cols > 0 ? gmi_matrix_get(h, 0, 0) : 0.0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) { double v = gmi_matrix_get(h, r, c); if (v < best) best = v; }
    return best;
}

int gmi_matrix_is_lower_triangular(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    for (int r = 0; r < rows; r++) for (int c = r + 1; c < cols; c++)
        if (gmi_matrix_get(h, r, c) != 0.0) return 0;
    return 1;
}

int gmi_matrix_is_upper_triangular(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    for (int r = 0; r < rows; r++) for (int c = 0; c < r && c < cols; c++)
        if (gmi_matrix_get(h, r, c) != 0.0) return 0;
    return 1;
}

int gmi_matrix_is_symmetric(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows != cols) return 0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        if (gmi_matrix_get(h, r, c) != gmi_matrix_get(h, c, r)) return 0;
    return 1;
}

int gmi_matrix_is_orthogonal(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows != cols) return 0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        double dot = 0.0;
        for (int k = 0; k < rows; k++) dot += gmi_matrix_get(h, k, r) * gmi_matrix_get(h, k, c);
        double exp = r == c ? 1.0 : 0.0;
        if (fabs(dot - exp) > 1e-9) return 0;
    }
    return 1;
}

int64_t gmi_matrix_clone(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    int64_t n = gmi_matrix_create(rows, cols);
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        gmi_matrix_set(n, r, c, gmi_matrix_get(h, r, c));
    return n;
}

int64_t gmi_matrix_reshape(int64_t h, int new_rows, int new_cols)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows * cols != new_rows * new_cols || new_rows <= 0 || new_cols <= 0) return 0;
    int64_t n = gmi_matrix_create(new_rows, new_cols);
    int idx = 0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        gmi_matrix_set(n, idx / new_cols, idx % new_cols, gmi_matrix_get(h, r, c));
        idx++;
    }
    return n;
}

int64_t gmi_matrix_concat_rows(int64_t a, int64_t b)
{
    int ra = gmi_matrix_rows(a), ca = gmi_matrix_cols(a), cb = gmi_matrix_cols(b);
    if (ca != cb) return 0;
    int64_t n = gmi_matrix_create(ra + gmi_matrix_rows(b), ca);
    for (int r = 0; r < ra; r++) for (int c = 0; c < ca; c++) gmi_matrix_set(n, r, c, gmi_matrix_get(a, r, c));
    for (int r = 0; r < gmi_matrix_rows(b); r++) for (int c = 0; c < cb; c++) gmi_matrix_set(n, ra + r, c, gmi_matrix_get(b, r, c));
    return n;
}

int64_t gmi_matrix_concat_cols(int64_t a, int64_t b)
{
    int ra = gmi_matrix_rows(a), ca = gmi_matrix_cols(a), rb = gmi_matrix_rows(b);
    if (ra != rb) return 0;
    int64_t n = gmi_matrix_create(ra, ca + gmi_matrix_cols(b));
    for (int r = 0; r < ra; r++) for (int c = 0; c < ca; c++) gmi_matrix_set(n, r, c, gmi_matrix_get(a, r, c));
    for (int r = 0; r < rb; r++) for (int c = 0; c < gmi_matrix_cols(b); c++) gmi_matrix_set(n, r, ca + c, gmi_matrix_get(b, r, c));
    return n;
}

bool* gmi_matrix_above_mask_alloc(int64_t h, double threshold, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    bool* out = (bool*)malloc(sizeof(bool) * (size_t)(rows * cols));
    if (!out) { *outn = 0; return NULL; }
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        out[r * cols + c] = gmi_matrix_get(h, r, c) > threshold;
    *outn = rows * cols;
    return out;
}

bool* gmi_matrix_above_matrix_alloc(int64_t h, double threshold, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    bool* out = (bool*)malloc(sizeof(bool) * (size_t)(rows * cols));
    if (!out) { *outn = 0; return NULL; }
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        out[r * cols + c] = gmi_matrix_get(h, r, c) > threshold;
    *outn = rows * cols;
    return out;
}

int64_t gmi_matrix_add_row_vector(int64_t h, const double* v, int vlen)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (vlen != cols) return 0;
    int64_t n = gmi_matrix_clone(h);
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        gmi_matrix_set(n, r, c, gmi_matrix_get(h, r, c) + v[c]);
    return n;
}

int64_t gmi_matrix_add_col_vector(int64_t h, const double* v, int vlen)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (vlen != rows) return 0;
    int64_t n = gmi_matrix_clone(h);
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        gmi_matrix_set(n, r, c, gmi_matrix_get(h, r, c) + v[r]);
    return n;
}

double* gmi_matrix_multiply_vector_alloc(int64_t h, const double* v, int vlen, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (vlen != cols) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)rows);
    if (!out) { *outn = 0; return NULL; }
    for (int r = 0; r < rows; r++) {
        double s = 0.0;
        for (int c = 0; c < cols; c++) s += gmi_matrix_get(h, r, c) * v[c];
        out[r] = s;
    }
    *outn = rows;
    return out;
}

double* gmi_matrix_multiply_left_vector_alloc(const double* v, int vlen, int64_t h, int64_t* outn)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    if (vlen != rows) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)cols);
    if (!out) { *outn = 0; return NULL; }
    for (int c = 0; c < cols; c++) {
        double s = 0.0;
        for (int r = 0; r < rows; r++) s += v[r] * gmi_matrix_get(h, r, c);
        out[c] = s;
    }
    *outn = cols;
    return out;
}

double* gmi_matrix_slice_alloc(int64_t h, int r0, int r1, int c0, int c1, int64_t* outn)
{
    /* Parità IL/PCode (SimdMatrix.Slice, end-ESCLUSIVO documentato): rows = r1-r0. */
    int rows = r1 - r0, cols = c1 - c0;
    if (rows <= 0 || cols <= 0) { *outn = 0; return NULL; }
    double* out = (double*)malloc(sizeof(double) * (size_t)(rows * cols));
    if (!out) { *outn = 0; return NULL; }
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        out[r * cols + c] = gmi_matrix_get(h, r0 + r, c0 + c);
    *outn = rows * cols;
    return out;
}

char* gmi_matrix_as_csv(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    size_t cap = 512, pos = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    for (int r = 0; r < rows; r++) {
        char buf[256];
        int n = snprintf(buf, sizeof(buf), "%.8g", gmi_matrix_get(h, r, 0));
        while (pos + (size_t)n + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
        memcpy(out + pos, buf, (size_t)n); pos += (size_t)n;
        for (int c = 1; c < cols; c++) {
            n = snprintf(buf, sizeof(buf), ",%.8g", gmi_matrix_get(h, r, c));
            while (pos + (size_t)n + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
            memcpy(out + pos, buf, (size_t)n); pos += (size_t)n;
        }
        if (pos + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
        out[pos++] = '\n';
    }
    out[pos] = 0;
    return out;
}

char* gmi_matrix_as_json(int64_t h)
{
    int rows = gmi_matrix_rows(h), cols = gmi_matrix_cols(h);
    size_t cap = 512, pos = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    out[pos++] = '[';   /* parentesi esterna (mancava: output "[1,2],[0,3]]") */
    for (int r = 0; r < rows; r++) {
        char buf[256];
        int n = snprintf(buf, sizeof(buf), "%s%.8g", r == 0 ? "[" : ",[", gmi_matrix_get(h, r, 0));
        while (pos + (size_t)n + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
        memcpy(out + pos, buf, (size_t)n); pos += (size_t)n;
        for (int c = 1; c < cols; c++) {
            n = snprintf(buf, sizeof(buf), ",%.8g", gmi_matrix_get(h, r, c));
            while (pos + (size_t)n + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
            memcpy(out + pos, buf, (size_t)n); pos += (size_t)n;
        }
        if (pos + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
        out[pos++] = ']';
    }
    if (pos + 2 > cap) { cap *= 2; out = (char*)realloc(out, cap); }
    out[pos++] = ']';
    out[pos] = 0;
    return out;
}

static int gmi_matrix_parse_flat(const char* text, double** out, int* rows_out, int* cols_out)
{
    if (!text) return -1;
    int rows = 0, cols = 0, curcols = 0;
    const char* p = text;
    double* vals = NULL;
    int n = 0;
    while (*p) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '[' || *p == ']' || *p == '{' || *p == '}') p++;
        if (!*p) break;
        double v = strtod(p, (char**)&p);
        if (p == text && !*p) break;
        vals = (double*)realloc(vals, sizeof(double) * (size_t)(n + 1));
        vals[n++] = v;
        curcols++;
        while (*p == ' ' || *p == ',' || *p == '\t') { if (*p == ',') { p++; break; } p++; }
        if (*p == '\n' || *p == '\r') { if (cols == 0) cols = curcols; curcols = 0; while (*p == '\n' || *p == '\r') p++; }
    }
    if (cols == 0) cols = curcols;
    if (cols > 0) rows = n / cols;
    *out = vals;
    *rows_out = rows;
    *cols_out = cols;
    return n;
}

int64_t gmi_matrix_load_csv(const char* path)
{
    char* text = NULL;
    /* Risoluzione upward dalla CWD (parità IlRuntime.ResolveMatrixPath): il gate C99
     * esegue col CWD = cartella del sample, parity5 col CWD = root del repo — un path
     * relativo-a-root (es. samples/Fixtures/...) deve risolversi in entrambi i casi. */
    char resolved[4096];
    const char* use = path;
    FILE* probe = (path && *path) ? fopen(path, "r") : NULL;
    if (probe) { fclose(probe); }
    else if (path && *path && path[0] != '/' && path[0] != '\\') {
        char cwd[2048];
        if (getcwd(cwd, sizeof(cwd))) {
            char dir[2048];
            strncpy(dir, cwd, sizeof(dir) - 1); dir[sizeof(dir) - 1] = 0;
            for (;;) {
                snprintf(resolved, sizeof(resolved), "%s/%s", dir, path);
                FILE* f2 = fopen(resolved, "r");
                if (f2) { fclose(f2); use = resolved; break; }
                char* sep = strrchr(dir, '/');
                if (!sep || sep == dir) break;
                *sep = 0;
            }
        }
    }
    FILE* f = (use && *use) ? fopen(use, "r") : NULL;
    if (!f) return 0;
    long len;
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    text = (char*)malloc((size_t)len + 1);
    if (text) { fread(text, 1, (size_t)len, f); text[len] = 0; }
    fclose(f);
    if (!text) return 0;
    double* vals = NULL; int rows = 0, cols = 0;
    gmi_matrix_parse_flat(text, &vals, &rows, &cols);
    free(text);
    if (vals && rows > 0 && cols > 0) {
        int64_t h = gmi_matrix_create(rows, cols);
        for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
            gmi_matrix_set(h, r, c, vals[r * cols + c]);
        free(vals);
        return h;
    }
    if (vals) free(vals);
    return 0;
}

int64_t gmi_matrix_load_json(const char* path)
{
    return gmi_matrix_load_csv(path);
}

int64_t gmi_matrix_slice(int64_t h, int r0, int r1, int c0, int c1)
{
    /* Parità IL/PCode (SimdMatrix.Slice, end-ESCLUSIVO documentato): rows = r1-r0. */
    int rows = r1 - r0, cols = c1 - c0;
    if (rows <= 0 || cols <= 0) return 0;
    int64_t n = gmi_matrix_create(rows, cols);
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++)
        gmi_matrix_set(n, r, c, gmi_matrix_get(h, r0 + r, c0 + c));
    return n;
}
