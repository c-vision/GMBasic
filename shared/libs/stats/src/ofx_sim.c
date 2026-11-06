/* ofx_sim.c — shim FFI per SIMULATION (gs_rng, gs_particle_filter, gs_tpe). */
#include "gs_sim.h"
#include "gs_num.h"
#include "gs_math.h"
#include <math.h>
#include <stdlib.h>

static gs_rng g_ofx_rng;
static int g_ofx_rng_ready = 0;

static void ofx_rng_init(void)
{
    if (!g_ofx_rng_ready) { g_ofx_rng = gs_rng_make(0x9E3779B97F4A7C15ULL); g_ofx_rng_ready = 1; }
}

double ofx_sim_seed(double seed)
{
    unsigned long long s = (unsigned long long)seed;
    g_ofx_rng = gs_rng_make(s ? s : 0x9E3779B97F4A7C15ULL);
    g_ofx_rng_ready = 1;
    return 0.0;
}
double ofx_sim_uniform(void) { ofx_rng_init(); return gs_rng_uniform(&g_ofx_rng); }
double ofx_sim_normal(void) { ofx_rng_init(); return gs_rng_normal(&g_ofx_rng); }
double ofx_sim_exponential(double rate)
{
    ofx_rng_init();
    if (rate <= 0.0) return NAN;
    return -log(1.0 - gs_rng_uniform(&g_ofx_rng)) / rate;
}
double ofx_sim_bernoulli(double p) { ofx_rng_init(); return (double)gs_rng_bernoulli(&g_ofx_rng, p); }
double ofx_sim_binomial(double n_in, double p)
{
    int i, n = (int)n_in;
    double c = 0.0;
    ofx_rng_init();
    if (n < 0) return NAN;
    for (i = 0; i < n; i++) if (gs_rng_bernoulli(&g_ofx_rng, p)) c += 1.0;
    return c;
}

/* out: 2*T valori — out[0..T-1] = mean filtrata, out[T..2T-1] = varianza. */
void ofx_sim_particle_filter(const double* obs, int T, double a, double q, double r,
                             int N, double seed, double* out)
{
    gs_rng rng = gs_rng_make((unsigned long long)seed);
    if (!obs || !out || T < 1 || N < 2) return;
    gs_particle_filter(obs, T, a, q, r, N, &rng, out, out + T);
}

typedef double (*ofx_sim_fv)(const double* x, int64_t n);
typedef struct { ofx_sim_fv f; int d; } ofx_tpe_ctx;

static double ofx_tpe_tramp(const double* x, void* c)
{
    ofx_tpe_ctx* t = (ofx_tpe_ctx*)c;
    return t->f(x, (int64_t)t->d);
}

double ofx_sim_tpe(ofx_sim_fv f, const double* lo, const double* hi, int d,
                   int n_iter, int n_init, double seed, double* out)
{
    ofx_tpe_ctx ctx;
    double* bestx;
    double bestf = 0.0;
    int i;
    if (!f || !lo || !hi || !out || d < 1) return NAN;
    bestx = (double*)malloc((size_t)d * sizeof(double));
    if (!bestx) return NAN;
    ctx.f = f; ctx.d = d;
    if (gs_tpe(ofx_tpe_tramp, &ctx, lo, hi, d, n_iter, n_init, (long)seed, bestx, &bestf) != 0) {
        free(bestx);
        return NAN;
    }
    for (i = 0; i < d; i++) out[i] = bestx[i];
    free(bestx);
    return bestf;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
void ofx_sim_particle_filter__ffi(const double* obs, int64_t obs_ext, int T, double a, double q, double r, int N, double seed, double* out, int64_t out_ext) { ofx_sim_particle_filter(obs, T, a, q, r, N, seed, out); }
double ofx_sim_tpe__ffi(ofx_sim_fv f, const double* lo, int64_t lo_ext, const double* hi, int64_t hi_ext, int d, int n_iter, int n_init, double seed, double* out, int64_t out_ext) { return ofx_sim_tpe(f, lo, hi, d, n_iter, n_init, seed, out); }
