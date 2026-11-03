#ifndef GMB_OPT_H
#define GMB_OPT_H

#include "gmi_runtime.h"

double gmi_opt_newton(double (*fn)(double, void*), double guess, double tol, int max_iter, void* ctx)
;

double gmi_opt_bisection(double (*fn)(double, void*), double lowerBound, double upperBound, double accuracy, int maxIterations, void* ctx)
;

double gmi_opt_brent(double (*fn)(double, void*), double lowerBound, double upperBound, double accuracy, int maxIterations, void* ctx)
;

double gmi_opt_golden(double (*fn)(double, void*), double lo, double hi, double tol, int max_iter, void* ctx)
;

void gmi_opt_nelder_mead(double (*fn)(const double*, int, void*), double* start, int n,
                         double tol, int max_iter, void* ctx, double* out)
;

void gmi_opt_bfgs(double (*fn)(const double*, int, void*), double* start, int n,
                  double tol, int max_iter, void* ctx, double* out)
;

void gmi_opt_cg(double (*fn)(const double*, int, void*), double* start, int n,
                double tol, int max_iter, void* ctx, double* out)
;

void gmi_opt_lbfgs(double (*fn)(const double*, int, void*), double* start, int n,
                   double tol, int memory, int max_iter, void* ctx, double* out)
;

double gmi_opt_secant(double (*fn)(double, void*), double x0, double x1, double tol, int max_iter, void* ctx)
;

#endif /* GMB_OPT_H */
/* Newton non lineare (pyfrbus): */
int gor_newton_damped(double (*F)(const double *x, double *out, void *ctx),
                      double (*J)(const double *x, double *out, void *ctx),
                      void *ctx, double *x, int n, int max_iter, double tol);
int gor_dogleg(double (*F)(const double *x, double *out, void *ctx),
               double (*J)(const double *x, double *out, void *ctx),
               void *ctx, double *x, int n, int max_iter, double tol);
