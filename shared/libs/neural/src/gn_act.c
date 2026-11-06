/* gn_act.c — funzioni di attivazione. */
#include "gn_act.h"

#include <math.h>

gn_real gn_sigmoid(gn_real x) { return 1.0 / (1.0 + exp(-x)); }
gn_real gn_sigmoid_grad(gn_real y) { return y * (1.0 - y); }
gn_real gn_tanh(gn_real x) { return tanh(x); }
gn_real gn_tanh_grad(gn_real y) { return 1.0 - y * y; }
gn_real gn_relu(gn_real x) { return x > 0.0 ? x : 0.0; }
gn_real gn_relu_grad(gn_real y) { return y > 0.0 ? 1.0 : 0.0; }
gn_real gn_leaky_relu(gn_real x) { return x > 0.0 ? x : 0.01 * x; }
gn_real gn_leaky_relu_grad(gn_real y) { return y > 0.0 ? 1.0 : 0.01; }
gn_real gn_elu(gn_real x) { return x > 0.0 ? x : (exp(x) - 1.0); }
gn_real gn_elu_grad(gn_real y) { return y > 0.0 ? 1.0 : (y + 1.0); }
gn_real gn_identity(gn_real x) { return x; }
gn_real gn_identity_grad(gn_real y) { (void)y; return 1.0; }

gn_status gn_softmax(gn_mat *out, const gn_mat *z)
{
    int i, j;
    if (!out || !z || out->rows != z->rows || out->cols != z->cols) return GN_ERR_ARG;
    for (i = 0; i < z->rows; i++) {
        gn_real mx = GN_AT(*z, i, 0), sum = 0.0;
        for (j = 1; j < z->cols; j++) if (GN_AT(*z, i, j) > mx) mx = GN_AT(*z, i, j);
        for (j = 0; j < z->cols; j++) {
            gn_real e = exp(GN_AT(*z, i, j) - mx);
            GN_AT(*out, i, j) = e;
            sum += e;
        }
        for (j = 0; j < z->cols; j++) GN_AT(*out, i, j) /= sum;
    }
    return GN_OK;
}

/* ===================== Attivazioni avanzate (da XNNPACK) ===================== */

gn_real gn_hardsigmoid(gn_real x)
{
    gn_real v = (x + 3.0) / 6.0;
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return v;
}
gn_real gn_hardsigmoid_grad(gn_real x)
{
    return (x > -3.0 && x < 3.0) ? (1.0 / 6.0) : 0.0;
}
gn_real gn_hardswish(gn_real x)
{
    return x * gn_hardsigmoid(x);
}
gn_real gn_hardswish_grad(gn_real x)
{
    if (x <= -3.0) return 0.0;
    if (x >= 3.0) return 1.0;
    return (2.0 * x + 3.0) / 6.0;
}
gn_real gn_silu(gn_real x)
{
    return x * gn_sigmoid(x);
}
gn_real gn_silu_grad(gn_real x)
{
    gn_real s = gn_sigmoid(x);
    return s * (1.0 + x * (1.0 - s));
}
gn_real gn_gelu(gn_real x)
{
    return 0.5 * x * (1.0 + erf(x / 1.4142135623730951));
}
gn_real gn_gelu_grad(gn_real x)
{
    gn_real c = 0.3989422804014327;   /* 1/sqrt(2pi) */
    return 0.5 * (1.0 + erf(x / 1.4142135623730951)) + x * c * exp(-0.5 * x * x);
}
gn_real gn_mish(gn_real x)
{
    gn_real sp = log1p(exp(x < 20.0 ? x : 20.0));   /* softplus stabilizzato */
    return x * tanh(sp);
}
gn_real gn_mish_grad(gn_real x)
{
    gn_real sp = log1p(exp(x < 20.0 ? x : 20.0));
    gn_real th = tanh(sp);
    gn_real s = gn_sigmoid(x);
    return th + x * (1.0 - th * th) * s;
}

gn_real gn_threshold(gn_real x) { return x > 0.0 ? 1.0 : 0.0; }
gn_real gn_threshold_grad(gn_real x) { (void)x; return 0.0; }

/* ===================== Attivazioni darknet (da sowson/darknet) ===================== */

gn_real gn_selu(gn_real x)
{
    return (x >= 0.0) ? 1.0507 * x : 1.0507 * 1.6732 * (exp(x) - 1.0);
}
gn_real gn_selu_grad(gn_real x)
{
    return (x >= 0.0) ? 1.0507 : 1.0507 * 1.6732 * exp(x);
}
gn_real gn_plse(gn_real x)
{
    if (x < -4.0) return 0.01 * (x + 4.0);
    if (x > 4.0) return 0.01 * (x - 4.0) + 1.0;
    return 0.125 * x + 0.5;
}
gn_real gn_plse_grad(gn_real x)
{
    return (x < -4.0 || x > 4.0) ? 0.01 : 0.125;
}
gn_real gn_loggy(gn_real x) { return 2.0 / (1.0 + exp(-x)) - 1.0; }
gn_real gn_loggy_grad(gn_real x)
{
    gn_real sg = 1.0 / (1.0 + exp(-x));   /* sigma(x); loggy = 2*sigma - 1 */
    return 2.0 * sg * (1.0 - sg);
}
gn_real gn_hardtan(gn_real x) { return x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x); }
gn_real gn_hardtan_grad(gn_real x) { return (x > -1.0 && x < 1.0) ? 1.0 : 0.0; }
gn_real gn_lhtan(gn_real x)
{
    if (x < 0.0) return 0.001 * x;
    if (x > 1.0) return 0.001 * (x - 1.0) + 1.0;
    return x;
}
gn_real gn_lhtan_grad(gn_real x) { return (x > 0.0 && x < 1.0) ? 1.0 : 0.001; }
gn_real gn_relie(gn_real x) { return x > 0.0 ? x : 0.01 * x; }
gn_real gn_relie_grad(gn_real x) { return x > 0.0 ? 1.0 : 0.01; }
gn_real gn_ramp(gn_real x) { return x * (x > 0.0 ? 1.0 : 0.0) + 0.1 * x; }
gn_real gn_ramp_grad(gn_real x) { return (x > 0.0 ? 1.0 : 0.0) + 0.1; }
gn_real gn_stair(gn_real x)
{
    int n = (int)floor(x);
    if (n % 2 == 0) return floor(x / 2.0);
    return (x - (gn_real)n) + floor(x / 2.0);
}
gn_real gn_stair_grad(gn_real x) { return (floor(x) == x) ? 0.0 : 1.0; }

gn_status gn_softmax_temp(gn_mat *out, const gn_mat *z, gn_real temperature)
{
    int i, j;
    if (!out || !z || out->rows != z->rows || out->cols != z->cols) return GN_ERR_ARG;
    if (temperature <= 0.0) temperature = 1.0;
    for (i = 0; i < z->rows; i++) {
        gn_real mx = -1e300, sum = 0.0;
        for (j = 0; j < z->cols; j++) { gn_real v = GN_AT(*z, i, j) / temperature; if (v > mx) mx = v; }
        for (j = 0; j < z->cols; j++) {
            gn_real e = exp(GN_AT(*z, i, j) / temperature - mx);
            GN_AT(*out, i, j) = e;
            sum += e;
        }
        for (j = 0; j < z->cols; j++) GN_AT(*out, i, j) /= sum;
    }
    return GN_OK;
}

/* ===================== Attivazioni nnabla (CELU/CRELU/PReLU) ===================== */

gn_real gn_celu(gn_real x, gn_real alpha)
{
    if (alpha <= 0.0) alpha = 1.0;
    if (x > 0.0) return x;
    return alpha * (exp(x / alpha) - 1.0);
}
gn_real gn_celu_grad(gn_real x, gn_real alpha)
{
    if (alpha <= 0.0) alpha = 1.0;
    return x > 0.0 ? 1.0 : exp(x / alpha);
}
gn_real gn_prelu(gn_real x, gn_real alpha)
{
    return x > 0.0 ? x : alpha * x;
}
gn_real gn_prelu_grad(gn_real x, gn_real alpha)
{
    return x > 0.0 ? 1.0 : alpha;
}
void gn_crelu(gn_real *out, const gn_real *in, int n)
{
    int i;
    if (!out || !in || n < 1) return;
    for (i = 0; i < n; i++) {
        out[i] = in[i] > 0.0 ? in[i] : 0.0;
        out[n + i] = in[i] < 0.0 ? -in[i] : 0.0;
    }
}
void gn_crelu_grad(gn_real *grad_in, const gn_real *grad_out, const gn_real *in, int n)
{
    int i;
    if (!grad_in || !grad_out || !in || n < 1) return;
    for (i = 0; i < n; i++) {
        gn_real g = 0.0;
        if (in[i] > 0.0) g += grad_out[i];
        else if (in[i] < 0.0) g -= grad_out[n + i];
        grad_in[i] = g;
    }
}
