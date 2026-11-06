/*
 * gn_pbp.h — Probabilistic Backpropagation (Bayesian NN, da HIPS).
 *
 * Da HIPS/Probabilistic-Backpropagation (BSD): propagazione della media e della
 * varianza delle attivazioni attraverso uno strato denso, con i momenti in forma
 * chiusa della ReLU applicata a una gaussiana (Assumed Density Filtering).
 */
#ifndef GMBNEURAL_GN_PBP_H
#define GMBNEURAL_GN_PBP_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Momenti della ReLU su N(m_a, v_a): con a=m_a/sqrt(v_a), p=Phi(a), g=phi(a)/p,
 *   m_z = p*(m_a + sqrt(v_a)*g)
 *   v_z = m_z*(m_a + sqrt(v_a)*g)*(1-p) + p*v_a*(1 - g^2 - g*a). */
void gn_pbp_relu_moments(gn_real m_a, gn_real v_a, gn_real *m_z, gn_real *v_z);

/* Forward PBP di uno strato denso. m_w/v_w (out×in), m_z_prev/v_z_prev (in).
 * m_z/v_z (out). scaling = fattore di scala (es. 1/in). relu != 0 applica i
 * momenti ReLU, altrimenti attivazione lineare. */
gn_status gn_pbp_layer_forward(const gn_real *m_w, const gn_real *v_w,
                               const gn_real *m_z_prev, const gn_real *v_z_prev,
                               int in, int out, gn_real scaling, int relu,
                               gn_real *m_z, gn_real *v_z);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_PBP_H */
