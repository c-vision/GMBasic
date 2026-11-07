/* gq_derivatives.c — Black-Scholes/Merton, binomiale e greche.
 * Porting C delle formule di CommonLib.Trading.Options (stesse formule: parità). */
#include "gm_quant_derivatives.h"

#include <math.h>
#include <stdlib.h>

static double norm_cdf(double x) { return 0.5 * erfc(-x / sqrt(2.0)); }
static double norm_pdf(double z) { return exp(-0.5 * z * z) / sqrt(2.0 * 3.141592653589793); }

double gq_bs_call(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0) return fmax(0.0, s - k);
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return s * norm_cdf(d1) - k * exp(-r * t) * norm_cdf(d2);
}

double gq_bs_put(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0) return fmax(0.0, k - s);
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return k * exp(-r * t) * norm_cdf(-d2) - s * norm_cdf(-d1);
}

double gq_merton_call(double s, double k, double t, double r, double q, double v)
{
    if (t <= 0.0 || v <= 0.0) return fmax(0.0, s - k);
    double d1 = (log(s / k) + (r - q + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return s * exp(-q * t) * norm_cdf(d1) - k * exp(-r * t) * norm_cdf(d2);
}

double gq_merton_put(double s, double k, double t, double r, double q, double v)
{
    if (t <= 0.0 || v <= 0.0) return fmax(0.0, k - s);
    double d1 = (log(s / k) + (r - q + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return k * exp(-r * t) * norm_cdf(-d2) - s * exp(-q * t) * norm_cdf(-d1);
}

double gq_bs_call_q(double s, double k, double t, double r, double v, double q)
{
    return q == 0.0 ? gq_bs_call(s, k, t, r, v) : gq_merton_call(s, k, t, r, q, v);
}

double gq_bs_put_q(double s, double k, double t, double r, double v, double q)
{
    return q == 0.0 ? gq_bs_put(s, k, t, r, v) : gq_merton_put(s, k, t, r, q, v);
}

double gq_merton_call_surface(double s, double k, double t, double r, double v, double q)
{
    return gq_merton_call(s, k, t, r, q, v);
}

double gq_merton_put_surface(double s, double k, double t, double r, double v, double q)
{
    return gq_merton_put(s, k, t, r, q, v);
}

double gq_binomial_ex(double s, double k, double t, double r, double v, int steps, int is_put, int is_american)
{
    if (steps <= 0) return 0.0;
    double dt = t / steps;
    double u = exp(v * sqrt(dt));
    double d = 1.0 / u;
    double p = (exp(r * dt) - d) / (u - d);
    if (p < 0.0 || p > 1.0) return 0.0;
    double *opt = (double *)malloc(sizeof(double) * (size_t)(steps + 1));
    if (!opt) return 0.0;
    for (int i = 0; i <= steps; i++)
    {
        double asset = s * pow(u, i) * pow(d, steps - i);
        opt[i] = fmax(0.0, is_put ? k - asset : asset - k);
    }
    for (int j = steps - 1; j >= 0; j--)
        for (int i = 0; i <= j; i++)
        {
            double continuation = exp(-r * dt) * (p * opt[i + 1] + (1.0 - p) * opt[i]);
            if (is_american)
            {
                double asset = s * pow(u, i) * pow(d, j - i);
                double exercise = fmax(0.0, is_put ? k - asset : asset - k);
                opt[i] = fmax(continuation, exercise);
            }
            else opt[i] = continuation;
        }
    double res = opt[0];
    free(opt);
    return res;
}

double gq_delta(double s, double k, double t, double r, double v, int is_put)
{
    if (t <= 0.0 || v <= 0.0) return is_put ? (s < k ? -1.0 : 0.0) : (s > k ? 1.0 : 0.0);
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    return is_put ? norm_cdf(d1) - 1.0 : norm_cdf(d1);
}

double gq_gamma(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0 || s <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    return norm_pdf(d1) / (s * v * sqrt(t));
}

double gq_vega(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    return s * norm_pdf(d1) * sqrt(t) * 0.01;
}

double gq_theta(double s, double k, double t, double r, double v, int is_put)
{
    if (t <= 0.0 || v <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double term1 = -(s * norm_pdf(d1) * v) / (2.0 * sqrt(t));
    double theta = is_put
        ? term1 + r * k * exp(-r * t) * norm_cdf(-d2)
        : term1 - r * k * exp(-r * t) * norm_cdf(d2);
    return theta / 365.0;
}

double gq_rho(double s, double k, double t, double r, double v, int is_put)
{
    (void)s; (void)v;
    if (t <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double rho = is_put
        ? -k * t * exp(-r * t) * norm_cdf(-d2)
        : k * t * exp(-r * t) * norm_cdf(d2);
    return rho * 0.01;
}

double gq_vanna(double s, double k, double t, double r, double v)
{
    (void)s;
    if (t <= 0.0 || v <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return -norm_pdf(d1) * d2 / v;
}

double gq_volga(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return s * norm_pdf(d1) * sqrt(t) * d1 * d2 / v;
}

double gq_speed(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0 || s <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double gamma = gq_gamma(s, k, t, r, v);
    return -gamma / s * (d1 / (v * sqrt(t)) + 1.0);
}

double gq_zomma(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double gamma = gq_gamma(s, k, t, r, v);
    return gamma * ((d1 * d2 - 1.0) / v);
}

double gq_charm(double s, double k, double t, double r, double v, int is_put)
{
    if (t <= 0.0 || v <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double charm = -norm_pdf(d1) * (2.0 * r * t - d2 * v * sqrt(t)) / (2.0 * t * v * sqrt(t));
    charm = is_put ? charm + r * exp(-r * t) * norm_cdf(-d2)
                   : charm - r * exp(-r * t) * norm_cdf(d2);
    return charm / 365.0;
}

double gq_color(double s, double k, double t, double r, double v)
{
    if (t <= 0.0 || v <= 0.0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double gamma = gq_gamma(s, k, t, r, v);
    return -gamma / (2.0 * t) * (1.0 + d1 / (v * sqrt(t)));
}

double gq_binomial(double s, double k, double t, double r, double v, int steps, int is_put)
{
    return gq_binomial_ex(s, k, t, r, v, steps, is_put, 0);
}
