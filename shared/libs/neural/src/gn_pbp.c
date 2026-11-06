/*
 * gn_pbp.c — Probabilistic Backpropagation (da HIPS/Probabilistic-Backpropagation).
 */
#include "gn_pbp.h"
#include <math.h>

static gn_real pnorm(gn_real a)
{
    return 0.5 * erfc(-a / 1.4142135623730951);
}

void gn_pbp_relu_moments(gn_real m_a, gn_real v_a, gn_real *m_z, gn_real *v_z)
{
    gn_real sqrt_aux, a, a2, p, g;
    if (v_a < 1e-300) v_a = 1e-300;
    sqrt_aux = sqrt(v_a);
    a = m_a / sqrt_aux;
    a2 = a * a;
    p = pnorm(a);
    if (a < -30.0)
        g = -a - 1.0 / a + 2.0 / (a * a2);
    else
        g = 0.39894228040143267794 * exp(-0.5 * a2) / (p > 1e-300 ? p : 1e-300);
    if (m_z) *m_z = p * (m_a + sqrt_aux * g);
    if (v_z) *v_z = (*m_z) * (m_a + sqrt_aux * g) * (1.0 - p)
                  + p * v_a * (1.0 - g * g - g * a);
}

gn_status gn_pbp_layer_forward(const gn_real *m_w, const gn_real *v_w,
                               const gn_real *m_z_prev, const gn_real *v_z_prev,
                               int in, int out, gn_real scaling, int relu,
                               gn_real *m_z, gn_real *v_z)
{
    int o, i;
    gn_real sq = sqrt(scaling);
    if (!m_w || !v_w || !m_z_prev || !v_z_prev || !m_z || !v_z || in < 1 || out < 1)
        return GN_ERR_ARG;
    for (o = 0; o < out; o++) {
        gn_real ma = 0.0, va = 0.0;
        const gn_real *mw = m_w + (size_t)o * in;
        const gn_real *vw = v_w + (size_t)o * in;
        for (i = 0; i < in; i++) {
            ma += sq * mw[i] * m_z_prev[i];
            va += scaling * (mw[i] * mw[i] * v_z_prev[i]
                  + vw[i] * m_z_prev[i] * m_z_prev[i]
                  + vw[i] * v_z_prev[i]);
        }
        if (relu) {
            gn_real mzz, vzz;
            gn_pbp_relu_moments(ma, va, &mzz, &vzz);
            m_z[o] = mzz; v_z[o] = vzz;
        } else {
            m_z[o] = ma; v_z[o] = va;
        }
    }
    return GN_OK;
}
