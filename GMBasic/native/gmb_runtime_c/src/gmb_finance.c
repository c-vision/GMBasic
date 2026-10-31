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

/* ---- Trading: azioni, CAPM, obbligazioni (CommonLib/Trading) ---- */

double gmb_trd_stock_const_growth(double nextDividend, double rate, double growth)
{
    fin_clear_err();
    double denominator = rate - growth;
    if (denominator <= 0) { fin_set_err("Il tasso di rendimento richiesto deve essere maggiore del tasso di crescita."); return 0; }
    return nextDividend / denominator;
}

double gmb_trd_capm(double riskFreeRate, double beta, double marketReturn)
{
    return riskFreeRate + beta * (marketReturn - riskFreeRate);
}

double gmb_trd_bond_equiv_yield(double faceValue, double bondPrice, double daysToMaturity)
{
    fin_clear_err();
    if (bondPrice == 0) { fin_set_err("Il prezzo dell'obbligazione non può essere zero."); return 0; }
    if (daysToMaturity == 0) { fin_set_err("I giorni a scadenza non possono essere zero."); return 0; }
    return ((faceValue - bondPrice) / bondPrice) * (365.0 / daysToMaturity);
}

double gmb_trd_current_yield(double annualCoupons, double bondPrice)
{
    fin_clear_err();
    if (bondPrice == 0) { fin_set_err("Il prezzo dell'obbligazione non può essere zero."); return 0; }
    return annualCoupons / bondPrice;
}

double gmb_trd_ytm(double couponPayment, double faceValue, double price, double years)
{
    fin_clear_err();
    if (years == 0) { fin_set_err("Gli anni a scadenza non possono essere zero."); return 0; }
    double numerator = couponPayment + ((faceValue - price) / years);
    double denominator = (faceValue + price) / 2.0;
    if (denominator == 0) return 0;
    return numerator / denominator;
}

double gmb_trd_zc_bond_value(double faceValue, double rate, double time)
{
    return faceValue / pow(1.0 + rate, time);
}

double gmb_trd_zc_bond_yield(double faceValue, double price, double time)
{
    fin_clear_err();
    if (time == 0) { fin_set_err("Il tempo a scadenza non può essere zero."); return 0; }
    if (price == 0) { fin_set_err("Il prezzo attuale non può essere zero."); return 0; }
    return pow(faceValue / price, 1.0 / time) - 1.0;
}

/* BONDPRICE/BONDYIELD (Trading.Bonds): modello a cedole, coupon riferito a nominale 100. */
double gmb_trd_bond_price(double oa_settle, double oa_mat, double rate, double yld, double redemption, int frequency)
{
    int settle_jdn = gmb_oa_to_jdn(oa_settle);
    int mat_jdn = gmb_oa_to_jdn(oa_mat);
    int nextCoupon = get_next_coupon(settle_jdn, mat_jdn, frequency);
    int N = 0;
    int coupon = nextCoupon;
    while (coupon <= mat_jdn) { N++; coupon = add_months_jdn(coupon, 12 / frequency); }
    double DSC = day_count_360(settle_jdn, nextCoupon, 0);
    double E = day_count_360(prev_coupon(nextCoupon, frequency), nextCoupon, 0);
    if (E == 0) E = 1;
    double couponAmount = 100.0 * rate / frequency;
    double discount = 1 + yld / frequency;
    double price = redemption / pow(discount, N - 1 + DSC / E);
    for (int i = 1; i <= N; i++)
        price += couponAmount / pow(discount, i - 1 + DSC / E);
    /* accrued: giorni dall'ultima cedola al settlement = (E-DSC), NON DSC (parita' GetCouponDateInfo a/e) */
    price -= couponAmount * ((E - DSC) / E);
    return price;
}

double gmb_trd_bond_yield(double oa_settle, double oa_mat, double rate, double pr, double redemption, int frequency)
{
    double yld = 0.1;
    for (int i = 0; i < 100; i++)
    {
        double p = gmb_trd_bond_price(oa_settle, oa_mat, rate, yld, redemption, frequency);
        double dp = (gmb_trd_bond_price(oa_settle, oa_mat, rate, yld + 1e-6, redemption, frequency) - p) / 1e-6;
        if (fabs(dp) < 1e-12) break;
        double ny = yld - (p - pr) / dp;
        if (fabs(ny - yld) < 1e-10) return ny;
        yld = ny;
    }
    return yld;
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

/* ---- Trading: metriche azionarie (CommonLib/Trading/General.cs, Metrics.cs) ---- */
double gmb_trd_dps(double dividends, double numShares) {
    fin_clear_err();
    if (numShares == 0) { fin_set_err("Il numero di azioni non può essere zero."); return 0; }
    return dividends / numShares;
}
double gmb_trd_eps(double netIncome, double avgShares) {
    fin_clear_err();
    if (avgShares == 0) { fin_set_err("Il numero di azioni medie non può essere zero."); return 0; }
    return netIncome / avgShares;
}
double gmb_trd_nav(double fundAssets, double fundLiabilities, double outstandingShares) {
    fin_clear_err();
    if (outstandingShares == 0) { fin_set_err("Il numero di azioni in circolazione non può essere zero."); return 0; }
    return (fundAssets - fundLiabilities) / outstandingShares;
}
/* Kurtosi (eccesso): E[(x-μ)^4]/σ^4 - 3. */
double gmb_trd_returns_kurtosis(const double* returns, int n) {
    if (!returns || n < 4) return 0.0;
    double mean = 0.0;
    for (int i = 0; i < n; i++) mean += returns[i];
    mean /= n;
    double var = 0.0, m4 = 0.0;
    for (int i = 0; i < n; i++) {
        double d = returns[i] - mean;
        var += d * d;
        m4 += d * d * d * d;
    }
    var /= n; m4 /= n;
    if (var < 1e-15) return 0.0;
    return m4 / (var * var) - 3.0;
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

double gmb_trd_equity_multiplier(double totalAssets, double equity) {
    fin_clear_err();
    if (equity == 0) { fin_set_err("Il patrimonio netto non può essere zero."); return 0; }
    return totalAssets / equity;
}

/* ---- Metriche di performance: canone CommonLib.Trading.Metrics (frazione, NON ×100).
 *   NOTA 2026-09-07: queste funzioni (usate SOLO dal backend VM/C) erano legacy in scala
 *   PERCENTUALE (×100) e con formule semplificate (drawdown a somma semplice, sharpe/sortino
 *   con ppy fisso 252 e rf ignorato). Riscritte per allinearsi al canone C# (Metrics.cs):
 *   TotalReturn/AnnualizedReturn/MaxDrawdown in FRAZIONE (0.05 = 5%), MaxDrawdown con
 *   capitalizzazione composta, Sharpe/Sortino su rendimenti in eccesso (r-rf) con sample std e
 *   il periodsPerYear (ppy) passato. Bug: (open) - VMC-Metriche-Rendimenti-x100. ---- */
double gmb_trd_total_return(const double* r, int n) {
    if (n <= 0) return 0;
    double t = 1.0;
    for (int i = 0; i < n; i++) t *= (1 + r[i]);
    return t - 1;
}
double gmb_trd_annualized_return(const double* r, int n, int ppy) {
    if (n <= 0 || ppy <= 0) return 0;
    double t = 1.0;
    for (int i = 0; i < n; i++) t *= (1 + r[i]);
    return pow(t, (double)ppy / n) - 1;
}
double gmb_trd_sharpe(const double* r, int n, double rf, int ppy) {
    if (n < 2 || ppy <= 0) return 0;
    double avg = 0; for (int i = 0; i < n; i++) avg += (r[i] - rf); avg /= n;
    double s = 0; for (int i = 0; i < n; i++) { double e = r[i] - rf; s += (e - avg) * (e - avg); }
    double std = sqrt(s / (n - 1));
    return std > 0 ? sqrt((double)ppy) * avg / std : 0;
}
double gmb_trd_sortino(const double* r, int n, double rf, int ppy) {
    if (n < 2 || ppy <= 0) return 0;
    double avg = 0; for (int i = 0; i < n; i++) avg += (r[i] - rf); avg /= n;
    /* DownsideDeviation(excess, 0): sqrt(media dei quadrati degli excess < 0) */
    double sum = 0; int cnt = 0;
    for (int i = 0; i < n; i++) { double e = r[i] - rf; if (e < 0) { sum += e * e; cnt++; } }
    if (cnt == 0) return 0;
    double dd = sqrt(sum / cnt);
    return dd == 0 ? 0 : sqrt((double)ppy) * avg / dd;
}
double gmb_trd_max_drawdown(const double* r, int n) {
    if (n <= 0) return 0;
    double cum = 1 + r[0], peak = cum, mdd = 0;
    for (int i = 1; i < n; i++) {
        cum *= (1 + r[i]);
        if (cum > peak) peak = cum;
        double dd = (cum - peak) / peak;
        if (dd < mdd) mdd = dd;
    }
    return mdd;
}
double gmb_trd_beta(const double* r, const double* b, int n) {
    if (n < 2) return 0;
    double avgA = 0, avgB = 0;
    for (int i = 0; i < n; i++) { avgA += r[i]; avgB += b[i]; }
    avgA /= n; avgB /= n;
    double cov = 0, varB = 0;
    for (int i = 0; i < n; i++) { cov += (r[i]-avgA)*(b[i]-avgB); varB += (b[i]-avgB)*(b[i]-avgB); }
    return varB > 0 ? cov / varB : 0;
}
double gmb_trd_alpha(const double* r, const double* b, int n, double rf, int ppy) {
    if (n < 2 || ppy <= 0) return 0;
    double beta = gmb_trd_beta(r, b, n);
    /* canone Metrics.Alpha (Jensen, annualizzato):
     * alpha = AR(asset) - (annualizedRiskFree + beta*(AR(bench) - annualizedRiskFree))
     * con annualizedRiskFree = (1+rf)^ppy - 1 (rf per periodo). */
    double ar = gmb_trd_annualized_return(r, n, ppy);
    double arb = gmb_trd_annualized_return(b, n, ppy);
    double arf = pow(1 + rf, (double)ppy) - 1;
    return ar - (arf + beta * (arb - arf));
}
/* GAP CHIUSI 2026-08-29 — TRADING.METRICS PERatio/DividendYield (semplici). */
double gmb_trd_pe_ratio(double price, double eps) {
    if (eps == 0) return 0;
    return price / eps;
}
double gmb_trd_dividend_yield(double dps, double price) {
    if (price == 0) return 0;
    return (dps / price) * 100.0;
}
double gmb_trd_var(const double* r, int n, double conf) {
    if (n <= 0) return 0;
    double* s = (double*)malloc(sizeof(double) * n);
    if (!s) return 0;
    memcpy(s, r, sizeof(double) * n);
    for (int i = 1; i < n; i++) { double k = s[i]; int j = i-1; while (j >= 0 && s[j] > k) { s[j+1]=s[j]; j--; } s[j+1]=k; }
    int idx = (int)((1 - conf) * n);
    if (idx < 0) idx = 0; if (idx >= n) idx = n - 1;
    double res = -s[idx] * 100;
    free(s);
    return res;
}
double gmb_trd_cvar(const double* r, int n, double conf) {
    if (n <= 0) return 0;
    double* s = (double*)malloc(sizeof(double) * n);
    if (!s) return 0;
    memcpy(s, r, sizeof(double) * n);
    for (int i = 1; i < n; i++) { double k = s[i]; int j = i-1; while (j >= 0 && s[j] > k) { s[j+1]=s[j]; j--; } s[j+1]=k; }
    int cutoff = (int)((1 - conf) * n);
    if (cutoff <= 0) cutoff = 1;
    double sum = 0;
    for (int i = 0; i < cutoff; i++) sum += s[i];
    free(s);
    /* CVaR = media delle perdite oltre il VaR (segno negativo), come il canone C#
     * CommonLib.Trading.Metrics.ConditionalValueAtRisk. Non valore assoluto. */
    return sum / cutoff;
}
double gmb_trd_portfolio_var_jagged(double** rows, int rows_n, int cols, double conf) {
    if (!rows || rows_n <= 0 || cols <= 0) return 0;
    int n = rows_n * cols;
    double* flat = (double*)malloc(sizeof(double) * (size_t)n);
    if (!flat) return 0;
    int k = 0;
    for (int r = 0; r < rows_n; r++)
        for (int c = 0; c < cols; c++) flat[k++] = rows[r] ? rows[r][c] : 0;
    double res = gmb_trd_var(flat, n, conf);
    free(flat);
    return res;
}
double gmb_trd_portfolio_cvar_jagged(double** rows, int rows_n, int cols, double conf) {
    if (!rows || rows_n <= 0 || cols <= 0) return 0;
    int n = rows_n * cols;
    double* flat = (double*)malloc(sizeof(double) * (size_t)n);
    if (!flat) return 0;
    int k = 0;
    for (int r = 0; r < rows_n; r++)
        for (int c = 0; c < cols; c++) flat[k++] = rows[r] ? rows[r][c] : 0;
    /* CVaR assoluto (parità IL CommonLib.Numerics.ConditionalVaR / VM/C gmb_bf_194). */
    double res = fabs(gmb_trd_cvar(flat, n, conf));
    free(flat);
    return res;
}
double gmb_trd_expected_return(const double* w, const double* r, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) sum += w[i] * r[i];
    return sum;
}
double gmb_trd_downside_dev(const double* r, int n, double threshold) {
    if (n <= 0) return 0;
    double ds = 0; int dc = 0;
    for (int i = 0; i < n; i++) if (r[i] < threshold) { double d = r[i] - threshold; ds += d*d; dc++; }
    /* canone Metrics.DownsideDeviation: sqrt(media dei quadrati) = sqrt(sum/cnt), FRAZIONE */
    return dc > 0 ? sqrt(ds / dc) : 0;
}
double gmb_trd_ann_vol(const double* r, int n, int ppy) {
    if (n < 2 || ppy <= 0) return 0;
    double avg = 0; for (int i = 0; i < n; i++) avg += r[i]; avg /= n;
    double s = 0; for (int i = 0; i < n; i++) s += (r[i]-avg)*(r[i]-avg);
    /* canone: sqrt(ppy) * sample-stddev, FRAZIONE (nessun ×100) */
    return sqrt((s / (n - 1)) * ppy);
}
double gmb_trd_omega(const double* r, int n, double threshold) {
    if (n <= 0) return 0;
    double up = 0, dn = 0;
    for (int i = 0; i < n; i++) { double d = r[i] - threshold; if (d > 0) up += d; else dn -= d; }
    return dn == 0 ? (double)INFINITY : up / dn;
}
double gmb_trd_ulcer(const double* r, int n) {
    /* canone Metrics.UlcerIndex: sqrt(media dei drawdown%^2) su cumulativa COMPOSTA dal picco.
     * dd% = (cum_i - peak)/peak * 100 → indice in scala percentuale (es. 2.5). */
    if (n <= 0) return 0;
    double cum = 1 + r[0], peak = cum, sum = 0;
    for (int i = 1; i < n; i++) {
        cum *= (1 + r[i]);
        if (cum > peak) peak = cum;
        double dd = (cum - peak) / peak * 100.0;
        sum += dd * dd;
    }
    return sqrt(sum / n);
}
double gmb_trd_portfolio_stddev(double w1, double s1, double w2, double s2, double corr) {
    return sqrt(w1*w1*s1*s1 + w2*w2*s2*s2 + 2*w1*w2*s1*s2*corr);
}

double gmb_trd_book_value(double equity, double shares) { fin_clear_err(); if (shares == 0) { fin_set_err("Il numero di azioni non può essere zero."); return 0; } return equity / shares; }
double gmb_trd_stock_zero_growth(double dividend, double rate) { fin_clear_err(); if (rate == 0) { fin_set_err("Il tasso di rendimento richiesto non può essere zero."); return 0; } return dividend / rate; }
double gmb_trd_preferred_stock_value(double dividend, double rate) { fin_clear_err(); if (rate == 0) { fin_set_err("Il tasso di sconto non può essere zero."); return 0; } return dividend / rate; }

int gmb_trd_is_long_only(const double* w, int n) {
    for (int i = 0; i < n; i++) if (w[i] < 0) return 0;
    return 1;
}

double gmb_trd_portfolio_sharpe(const double* w, const double* r, int n) {
    if (n < 2) return 0;
    double pRet = 0;
    for (int i = 0; i < n; i++) pRet += w[i] * r[i];
    double avg = 0; for (int i = 0; i < n; i++) avg += r[i]; avg /= n;
    double s = 0; for (int i = 0; i < n; i++) s += (r[i]-avg)*(r[i]-avg);
    double std = sqrt(s / (n - 1));
    return std > 0 ? (pRet / std) * sqrt(252.0) : 0;
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
