/* ============================================================================
 * gmb_finance.c — Formule finanziarie (portate da CommonLib.Numerics/Trading).
 * ============================================================================ */
#include "gmb_finance.h"
#include "gmb_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Errore dell'ultima funzione finance (parità dei throw ArgumentException C#).
 * "" = ok. Le funzioni validate lo azzerano all'inizio e lo impostano su input invalido;
 * VM/C e C99 lo controllano dopo la chiamata e sollevano l'errore. */
static char g_fin_err[160];
const char* gmb_fin_last_error(void) { return g_fin_err; }
void gmb_fin_clear_error(void) { g_fin_err[0] = '\0'; }
void gmb_fin_set_error(const char* msg) { snprintf(g_fin_err, sizeof(g_fin_err), "%s", msg ? msg : ""); }
static void fin_clear_err(void) { g_fin_err[0] = '\0'; }
static void fin_set_err(const char* m) { snprintf(g_fin_err, sizeof(g_fin_err), "%s", m ? m : ""); }

/* CDF normale standard (erfc-based) */
static double gmb_cndf(double x)
{
    return 0.5 * erfc(-x / sqrt(2.0));
}

double gmb_fin_pmt(double rate, double nper, double pv, double fv, int type)
{
    if (nper == 0) return 0;
    if (rate == 0) return -(pv + fv) / nper;
    double pw = pow(1 + rate, nper);
    double pmt = (-pv * pw - fv) / ((1 + rate * type) * (pw - 1) / rate);
    return pmt;
}

double gmb_fin_cumipmt(double rate, double nper, double pv, int start, int end, int type)
{
    fin_clear_err();
    if (nper <= 0 || start < 1 || end > nper || start > end) { fin_set_err("Periodi non validi."); return 0; }
    double total = 0;
    double pmt = gmb_fin_pmt(rate, nper, pv, 0, type);
    double balance = pv;
    for (int per = 1; per <= end; per++)
    {
        double interest = type == 1 ? balance * rate : (balance - pmt) * rate;
        if (per >= start) total += interest;
        balance = type == 1 ? balance + interest - pmt : balance - pmt + interest;
    }
    return total;
}

double gmb_fin_cumprinc(double rate, double nper, double pv, int start, int end, int type)
{
    fin_clear_err();
    if (nper <= 0 || start < 1 || end > nper || start > end) { fin_set_err("Periodi non validi."); return 0; }
    double total = 0;
    double pmt = gmb_fin_pmt(rate, nper, pv, 0, type);
    double balance = pv;
    for (int per = 1; per <= end; per++)
    {
        double interest = type == 1 ? balance * rate : (balance - pmt) * rate;
        double principal = pmt - interest;
        if (per >= start) total += principal;
        balance = type == 1 ? balance + interest - pmt : balance - pmt + interest;
    }
    return total;
}

double gmb_fin_db(double cost, double salvage, double life, double period, int month)
{
    fin_clear_err();
    if (cost <= 0 || life <= 0 || period < 1 || period > life || month < 1 || month > 12 || cost <= salvage)
    { fin_set_err("Argomenti non validi per la funzione DB."); return 0; }
    double rate = 1.0 - pow(salvage / cost, 1.0 / life);
    rate = gmb_round(rate, 3); /* Math.Round ToEven, come C# */
    double totalDep = 0;
    double currentDep = cost * rate * month / 12.0;
    if (period == 1) return currentDep < (cost - salvage) ? currentDep : (cost - salvage);
    totalDep += currentDep;
    for (int i = 2; i < (int)period; i++)
    {
        currentDep = (cost - totalDep) * rate;
        totalDep += currentDep;
    }
    currentDep = (cost - totalDep) * rate;
    double maxDep = cost - salvage - totalDep;
    return currentDep < maxDep ? currentDep : maxDep;
}

double gmb_fin_ddb(double cost, double salvage, double life, double period, double factor)
{
    if (life <= 0 || period < 1) return 0;
    if (factor == 0) factor = 2.0;
    double rate = factor / life;
    double c = cost;
    for (int i = 1; i <= (int)period; i++)
    {
        double d = c * rate;
        if (c - d < salvage) d = c - salvage;
        if (i == (int)period) return d;
        c -= d;
    }
    return 0;
}

double gmb_fin_rate(double nper, double pmt, double pv, double fv, int type, double guess)
{
    /* Newton-Raphson su Pmt */
    if (guess == 0) guess = 0.1;
    double r = guess;
    for (int iter = 0; iter < 100; iter++)
    {
        double pw = pow(1 + r, nper);
        double f = pv * pw + pmt * (1 + r * type) * (pw - 1) / r + fv;
        double fprime = pv * nper * pw / (1 + r) + pmt * (1 + r * type) * (nper * pw / (1 + r) - (pw - 1) / r) / r
                        + pmt * type * (pw - 1) / r;
        if (fprime == 0) break;
        double nr = r - f / fprime;
        if (fabs(nr - r) < 1e-9) return nr;
        r = nr;
    }
    return r;
}

double gmb_fin_bs_call(double s, double k, double t, double r, double v)
{
    if (t <= 0 || v <= 0) return fmax(0.0, s - k);
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return s * gmb_cndf(d1) - k * exp(-r * t) * gmb_cndf(d2);
}

double gmb_fin_bs_put(double s, double k, double t, double r, double v)
{
    if (t <= 0 || v <= 0) return fmax(0.0, k - s);
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return k * exp(-r * t) * gmb_cndf(-d2) - s * gmb_cndf(-d1);
}

double gmb_fin_merton_call(double s, double k, double t, double r, double q, double v)
{
    if (t <= 0 || v <= 0) return fmax(0.0, s - k);
    double d1 = (log(s / k) + (r - q + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return s * exp(-q * t) * gmb_cndf(d1) - k * exp(-r * t) * gmb_cndf(d2);
}

double gmb_fin_merton_put(double s, double k, double t, double r, double q, double v)
{
    if (t <= 0 || v <= 0) return fmax(0.0, k - s);
    double d1 = (log(s / k) + (r - q + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return k * exp(-r * t) * gmb_cndf(-d2) - s * exp(-q * t) * gmb_cndf(-d1);
}

/* ---- Data utilities (OADate = giorni dal 1899-12-30) ---- */
int gmb_oa_to_jdn(double oa)
{
    return (int)floor(oa) + 2415019; /* OADate 0 = 1899-12-30, JD 2415019 */
}

void gmb_jdn_to_ymd(int jdn, int* y, int* m, int* d)
{
    int a = jdn + 32044;
    int b = (4 * a + 3) / 146097;
    int c = a - (146097 * b) / 4;
    int dd = (4 * c + 3) / 1461;
    int e = c - (1461 * dd) / 4;
    int mm = (5 * e + 2) / 153;
    *d = e - (153 * mm + 2) / 5 + 1;
    *m = mm + 3 - 12 * (mm / 10);
    *y = 100 * b + dd - 4800 + mm / 10;
}

int gmb_ymd_to_jdn(int y, int m, int d)
{
    int a = (14 - m) / 12;
    int yy = y + 4800 - a;
    int mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

static int add_months_jdn(int jdn, int months)
{
    int y, m, d;
    gmb_jdn_to_ymd(jdn, &y, &m, &d);
    int total = (y * 12 + (m - 1)) + months;
    y = total / 12;
    m = total % 12 + 1;
    int last = (m == 2) ? (28 + ((y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 1 : 0))
                        : (30 + ((m + (m / 8)) % 2));
    if (d > last) d = last;
    return gmb_ymd_to_jdn(y, m, d);
}

static int get_next_coupon(int settle_jdn, int mat_jdn, int frequency)
{
    int months = 12 / frequency;
    int coupon = mat_jdn;
    while (coupon > settle_jdn)
        coupon = add_months_jdn(coupon, -months);
    return add_months_jdn(coupon, months);
}

static int prev_coupon(int next_jdn, int frequency)
{
    return add_months_jdn(next_jdn, -(12 / frequency));
}

static double day_count_360(int j1, int j2, int basis)
{
    int y1, m1, d1, y2, m2, d2;
    gmb_jdn_to_ymd(j1, &y1, &m1, &d1);
    gmb_jdn_to_ymd(j2, &y2, &m2, &d2);
    if (basis == 4)
    {
        if (d1 == 31) d1 = 30;
        if (d2 == 31) d2 = 30;
        return (y2 - y1) * 360 + (m2 - m1) * 30 + (d2 - d1);
    }
    if (d1 == 31) d1 = 30;
    if (d2 == 31 && d1 == 30) d2 = 30;
    return (y2 - y1) * 360 + (m2 - m1) * 30 + (d2 - d1);
}

double gmb_fin_days360(double oa_start, double oa_end, int basis)
{
    return day_count_360(gmb_oa_to_jdn(oa_start), gmb_oa_to_jdn(oa_end), basis);
}

double gmb_fin_bond_price(double oa_settle, double oa_mat, double rate, double yld, double redemption, int frequency, int basis)
{
    int settle_jdn = gmb_oa_to_jdn(oa_settle);
    int mat_jdn = gmb_oa_to_jdn(oa_mat);
    int nextCoupon = get_next_coupon(settle_jdn, mat_jdn, frequency);
    int N = 0;
    int coupon = nextCoupon;
    while (coupon <= mat_jdn)
    {
        N++;
        coupon = add_months_jdn(coupon, 12 / frequency);
    }
    double DSC = day_count_360(settle_jdn, nextCoupon, basis);
    double E = day_count_360(prev_coupon(nextCoupon, frequency), nextCoupon, basis);
    if (E == 0) E = 1;
    double AI = (rate * redemption / frequency) * (1 - DSC / E);
    double price = 0;
    for (int k = 1; k <= N; k++)
    {
        double t = (DSC + E * (k - 1)) / E;
        price += (rate * redemption / frequency) / pow(1 + yld / frequency, t);
    }
    price += redemption / pow(1 + yld / frequency, (DSC + E * N) / E);
    return price - AI;
}

double gmb_fin_bond_yield(double oa_settle, double oa_mat, double price, double redemption, double rate, int frequency, int basis)
{
    double lower = 0, upper = 1, yld = 0.05, tol = 1e-7;
    for (int i = 0; i < 100; i++)
    {
        double testPrice = gmb_fin_bond_price(oa_settle, oa_mat, rate, yld, redemption, frequency, basis);
        if (fabs(testPrice - price) < tol) return yld;
        if (testPrice > price) lower = yld;
        else upper = yld;
        yld = (lower + upper) / 2;
    }
    return yld;
}

double gmb_fin_duration(double oa_settle, double oa_mat, double coupon, double yld, int frequency, int basis, int modified)
{
    (void)basis;
    /* Parita' CommonLib.Numerics.Duration (modello semplice): n = round(giorni/365*freq)
     * periodi di cedola interi, niente convenzioni day-count/date cedola reali. */
    double days = oa_mat - oa_settle;
    int n = (int)(days / 365.0 * frequency + 0.5);
    if (n <= 0) return 0;
    double pv = 0, weighted = 0;
    for (int k = 1; k <= n; k++)
    {
        double cf = coupon / frequency;
        double t = (double)k / frequency;
        double df = pow(1 + yld / frequency, k);
        pv += cf / df;
        weighted += t * cf / df;
    }
    pv += 1.0 / pow(1 + yld / frequency, n);
    weighted += ((double)n / frequency) * (1.0 / pow(1 + yld / frequency, n));
    double dur = weighted / pv;
    return modified ? dur / (1 + yld / frequency) : dur;
}

/* ---- Funzioni vettoriali (VM estrae liste e delega) ---- */

double gmb_fin_npv(double rate, const double* cf, int n)
{
    double npv = 0;
    for (int i = 0; i < n; i++)
        npv += cf[i] / pow(1 + rate, i + 1);
    return npv;
}

/* MNPV: nel dialetto GMBasic coincide con NPV (parita' IlRuntime.Mnpv:
 * somma cf[i]/(1+rate)^(i+1), primo flusso scontato un periodo). */
double gmb_fin_mnpv(double rate, const double* cf, int n)
{
    double npv = 0;
    for (int i = 0; i < n; i++)
        npv += cf[i] / pow(1 + rate, i + 1);
    return npv;
}

double gmb_fin_irr(const double* cf, int n, double guess)
{
    double g = guess;
    for (int iter = 0; iter < 100; iter++)
    {
        double npv = 0, dnpv = 0;
        for (int i = 0; i < n; i++)
        {
            double d = pow(1 + g, i);
            npv += cf[i] / d;
            dnpv += -i * cf[i] / (d * (1 + g));
        }
        if (fabs(dnpv) < 1e-10) break;
        double ng = g - npv / dnpv;
        if (fabs(ng - g) < 1e-8) return ng;
        g = ng;
    }
    return g;
}

double gmb_fin_mirr(const double* cf, int n, double fr, double rr)
{
    /* Parita' CommonLib.Numerics.Mirr: i flussi POSITIVI vengono capitalizzati in avanti
     * col tasso di reinvestimento (rr) fino a fine periodo; i NEGATIVI scontati all'indietro
     * col tasso di finanziamento (fr). mirr = (FV_pos/(-PV_neg))^(1/(n-1)) - 1. */
    double fvPositive = 0, pvNegative = 0;
    for (int i = 0; i < n; i++)
    {
        if (cf[i] > 0) fvPositive += cf[i] * pow(1 + rr, n - 1 - i);
        else if (cf[i] < 0) pvNegative += cf[i] / pow(1 + fr, i);
    }
    if (fvPositive == 0 || pvNegative == 0) return 0;
    return pow(-fvPositive / pvNegative, 1.0 / (n - 1)) - 1;
}

double gmb_fin_xirr(const double* cf, const double* dates, int n, double guess)
{
    double base = dates[0];
    double g = guess;
    for (int iter = 0; iter < 100; iter++)
    {
        double xnpv = 0, derivative = 0;
        for (int j = 0; j < n; j++)
        {
            double t = (dates[j] - base) / 365.0;
            double df = pow(1 + g, t);
            xnpv += cf[j] / df;
            if (t != 0) derivative -= cf[j] * t * pow(1 + g, t - 1) / (df * df);
        }
        if (fabs(xnpv) < 1e-7) return g;
        if (fabs(derivative) < 1e-12) break;
        double ng = g - xnpv / derivative;
        if (fabs(ng - g) < 1e-7) return ng;
        g = ng;
    }
    return g;
}

double gmb_fin_fv(double r, double n, double pmt, double pv, int type)
{
    if (r == 0) return -pv - pmt * n;
    double pw = pow(1 + r, n);
    return -pv * pw - pmt * (1 + r * type) * (pw - 1) / r;
}

double gmb_fin_fvext(double r, double n, double pmt, double pv, int type)
{
    return gmb_fin_fv(r, n, pmt, pv, type);
}

double gmb_fin_pvext(double r, double n, double pmt, double fv, int type)
{
    if (r == 0) return -(pmt * n + fv);
    double pw = pow(1 + r, n);
    return (-pmt * (1 + r * type) * (1 - 1 / pw) / r) - fv / pw;
}

double gmb_fin_sln(double cost, double salvage, double life)
{
    if (life <= 0) return 0;
    return (cost - salvage) / life;
}

double gmb_fin_syd(double cost, double salvage, double life, double period)
{
    if (life <= 0) return 0;
    return (cost - salvage) * (life - period + 1) * 2 / (life * (life + 1));
}

/* ---- Greche Black-Scholes (CommonLib/Trading/Options.cs) ---- */
static double gmb_ndf(double z) {
    return exp(-0.5 * z * z) / sqrt(2.0 * 3.141592653589793);
}

double gmb_fin_delta(double s, double k, double t, double r, double v, int is_put)
{
    if (t <= 0 || v <= 0) return is_put ? (s < k ? -1.0 : 0.0) : (s > k ? 1.0 : 0.0);
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    return is_put ? gmb_cndf(d1) - 1.0 : gmb_cndf(d1);
}

double gmb_fin_gamma(double s, double k, double t, double r, double v)
{
    if (t <= 0 || v <= 0 || s <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    return gmb_ndf(d1) / (s * v * sqrt(t));
}

double gmb_fin_vega(double s, double k, double t, double r, double v)
{
    if (t <= 0 || v <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    return s * gmb_ndf(d1) * sqrt(t) * 0.01;
}

double gmb_fin_theta(double s, double k, double t, double r, double v, int is_put)
{
    if (t <= 0 || v <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double term1 = -(s * gmb_ndf(d1) * v) / (2.0 * sqrt(t));
    double theta;
    if (is_put) theta = term1 + r * k * exp(-r * t) * gmb_cndf(-d2);
    else theta = term1 - r * k * exp(-r * t) * gmb_cndf(d2);
    return theta / 365.0;
}

double gmb_fin_rho(double s, double k, double t, double r, double v, int is_put)
{
    (void)s; (void)v;
    if (t <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double rho;
    if (is_put) rho = -k * t * exp(-r * t) * gmb_cndf(-d2);
    else rho = k * t * exp(-r * t) * gmb_cndf(d2);
    return rho * 0.01;
}

double gmb_fin_vanna(double s, double k, double t, double r, double v)
{
    (void)s;
    if (t <= 0 || v <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return -gmb_ndf(d1) * d2 / v;
}

double gmb_fin_volga(double s, double k, double t, double r, double v)
{
    if (t <= 0 || v <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    return s * gmb_ndf(d1) * sqrt(t) * d1 * d2 / v;
}

double gmb_fin_speed(double s, double k, double t, double r, double v)
{
    if (t <= 0 || v <= 0 || s <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double gamma = gmb_fin_gamma(s, k, t, r, v);
    return -gamma / s * (d1 / (v * sqrt(t)) + 1.0);
}

double gmb_fin_zomma(double s, double k, double t, double r, double v)
{
    if (t <= 0 || v <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double gamma = gmb_fin_gamma(s, k, t, r, v);
    return gamma * ((d1 * d2 - 1.0) / v);
}

double gmb_fin_charm(double s, double k, double t, double r, double v, int is_put)
{
    if (t <= 0 || v <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double d2 = d1 - v * sqrt(t);
    double charm = -gmb_ndf(d1) * (2.0 * r * t - d2 * v * sqrt(t)) / (2.0 * t * v * sqrt(t));
    if (is_put) charm += r * exp(-r * t) * gmb_cndf(-d2);
    else charm -= r * exp(-r * t) * gmb_cndf(d2);
    return charm / 365.0;
}

double gmb_fin_color(double s, double k, double t, double r, double v)
{
    (void)r;
    if (t <= 0 || v <= 0) return 0.0;
    double d1 = (log(s / k) + (r + 0.5 * v * v) * t) / (v * sqrt(t));
    double gamma = gmb_fin_gamma(s, k, t, r, v);
    return -gamma / (2.0 * t) * (1.0 + d1 / (v * sqrt(t)));
}

double gmb_fin_binomial(double s, double k, double t, double r, double v, int steps, int is_put, int is_american)
{
    fin_clear_err();
    if (steps <= 0) { fin_set_err("Il numero di passi deve essere positivo."); return 0; }
    double dt = t / steps;
    double u = exp(v * sqrt(dt));
    double d = 1.0 / u;
    double p = (exp(r * dt) - d) / (u - d);
    if (p < 0 || p > 1) return 0;

    double* opt = (double*)malloc(sizeof(double) * (steps + 1));
    if (!opt) return 0;
    for (int i = 0; i <= steps; i++)
    {
        double assetPrice = s * pow(u, i) * pow(d, steps - i);
        opt[i] = fmax(0.0, is_put ? k - assetPrice : assetPrice - k);
    }
    for (int j = steps - 1; j >= 0; j--)
    {
        for (int i = 0; i <= j; i++)
        {
            double continuationValue = exp(-r * dt) * (p * opt[i + 1] + (1 - p) * opt[i]);
            if (is_american)
            {
                double assetPrice = s * pow(u, i) * pow(d, j - i);
                double exerciseValue = fmax(0.0, is_put ? k - assetPrice : assetPrice - k);
                opt[i] = fmax(continuationValue, exerciseValue);
            }
            else opt[i] = continuationValue;
        }
    }
    double res = opt[0];
    free(opt);
    return res;
}
double gmb_fin_nper_ext(double rate, double pmt, double pv, double fv, int type) {
    if (rate == 0) {
        if (pmt == 0) return 0;
        return -(fv + pv) / pmt;
    }
    double numerator = pmt * (1 + rate * type) - fv * rate;
    double denominator = pmt * (1 + rate * type) + pv * rate;
    if (denominator == 0 || numerator / denominator <= 0) return 0;
    return log(numerator / denominator) / log(1 + rate);
}
double gmb_fin_price_simple(double oa_settle, double oa_mat, double rate, double yld, double redemption, int frequency) {
    int n = (int)llround(((oa_mat - oa_settle) / 365.0) * frequency);
    if (n <= 0) return 0;
    double price = 0;
    for (int k = 1; k <= n; k++)
        price += (rate * redemption / frequency) / pow(1 + yld / frequency, k);
    price += redemption / pow(1 + yld / frequency, n);
    return price;
}

/* YEARFRAC(start, end, basis): frazione di anno (parita' CommonLib.Numerics.YearFrac).
 * basis: 0=30/360 US, 1=effettivi/(366|365 anno inizio), 2=effettivi/360,
 *        3=effettivi/365, 4=30/360 EU. start/end sono OADate. */
double gmb_fin_yearfrac(double oa_start, double oa_end, int basis)
{
    long days = (long)(oa_end - oa_start);
    if (basis == 1 || basis == 2 || basis == 3)
    {
        int y = gmb_dt_year(oa_start);
        if (basis == 2) return days / 360.0;
        if (basis == 3) return days / 365.0;
        return days / ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 366.0 : 365.0);
    }
    /* basis 0 (US) o 4 (EU): 30/360 */
    int y1 = gmb_dt_year(oa_start), m1 = gmb_dt_month(oa_start), d1 = gmb_dt_day(oa_start);
    int y2 = gmb_dt_year(oa_end),   m2 = gmb_dt_month(oa_end),   d2 = gmb_dt_day(oa_end);
    if (basis == 0)
    {
        if (d1 == 31) d1 = 30;
        if (d2 == 31 && d1 == 30) d2 = 30;
    }
    else
    {
        if (d1 == 31) d1 = 30;
        if (d2 == 31) d2 = 30;
    }
    return ((y2 - y1) * 360 + (m2 - m1) * 30 + (d2 - d1)) / 360.0;
}

/* XNPV(rate, cf, dates): attualizzazione su date non periodiche (parita'
 * CommonLib.Numerics.Xnpv): xnpv = sum cf[i] / (1+rate)^((dates[i]-dates[0])/365). */
double gmb_fin_xnpv(double rate, const double* cf, const double* oa_dates, int n)
{
    double base0 = oa_dates[0];
    double x = 0;
    for (int i = 0; i < n; i++)
    {
        double t = (long)(oa_dates[i] - base0) / 365.0;
        x += cf[i] / pow(1 + rate, t);
    }
    return x;
}
