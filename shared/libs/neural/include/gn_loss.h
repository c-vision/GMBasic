/*
 * gn_loss.h — funzioni di perdita e relativi gradienti rispetto alla predizione.
 */
#ifndef GMBNEURAL_GN_LOSS_H
#define GMBNEURAL_GN_LOSS_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MSE: mean((y-t)^2); grad = 2(y-t)/N. */
gn_real gn_loss_mse(const gn_mat *y, const gn_mat *t, gn_mat *grad);
/* MAE: mean(|y-t|); grad = sign(y-t)/N. */
gn_real gn_loss_mae(const gn_mat *y, const gn_mat *t, gn_mat *grad);
/* Cross-entropy categorica (y = probabilità, t one-hot): -mean(sum t log y);
 * grad = (y-t)/batch. */
gn_real gn_loss_cross_entropy(const gn_mat *y, const gn_mat *t, gn_mat *grad);
/* Binary cross-entropy: -mean(t log y + (1-t) log(1-y)); grad=(y-t)/(N y(1-y)). */
gn_real gn_loss_binary_cross_entropy(const gn_mat *y, const gn_mat *t, gn_mat *grad);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_LOSS_H */
