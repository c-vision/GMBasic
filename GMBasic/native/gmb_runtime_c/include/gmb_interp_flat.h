/* gmb_interp_flat.h — PONTE delle INTERPOLAZIONI per i backend MANAGED (P/Invoke C#→C).
 *
 * PERCHÉ. `CommonLib/Numerics/Interpolation.cs` calcolava in C# (MathNet + barycentric + sistema
 * not-a-knot) le stesse interpolazioni che `gmb_math.c` implementa per VM/C e C99Native: due
 * sorgenti. Qui il managed chiama i kernel del runtime (come `gmb_matrix_flat`).
 *
 * CONTRATTO. Solo inoltro: `n` è il numero di punti (x e y della stessa lunghezza). */
#ifndef GMB_INTERP_FLAT_H
#define GMB_INTERP_FLAT_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

double gmb_ipf_linear(const double* x, const double* y, int n, double tx);
double gmb_ipf_step(const double* x, const double* y, int n, double tx);
double gmb_ipf_polynomial(const double* x, const double* y, int n, double tx);
double gmb_ipf_akima(const double* x, const double* y, int n, double tx);
double gmb_ipf_pchip(const double* x, const double* y, int n, double tx);
double gmb_ipf_cubic_spline(const double* x, const double* y, int n, double tx);

#ifdef __cplusplus
}
#endif

#endif /* GMB_INTERP_FLAT_H */
