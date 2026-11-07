/* gq_bonds.c — obbligazioni: prezzo/rendimento, duration, ZCB, BEY.
 * Date in OADate (giorni dal 1899-12-30), come le DateTime di GMBasic. */
#include "gm_quant_bonds.h"

#include <math.h>

/* ── calendario (OADate -> JDN -> Y/M/D) ─────────────────────────────────── */

static int oa_to_jdn(double oa) { return (int)floor(oa) + 2415019; }

static void jdn_to_ymd(int jdn, int *y, int *m, int *d)
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

static int ymd_to_jdn(int y, int m, int d)
{
    int a = (14 - m) / 12;
    int yy = y + 4800 - a;
    int mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

static int add_months_jdn(int jdn, int months)
{
    int y, m, d;
    jdn_to_ymd(jdn, &y, &m, &d);
    int total = (y * 12 + (m - 1)) + months;
    y = total / 12;
    m = total % 12 + 1;
    int last = (m == 2) ? (28 + ((y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 1 : 0))
                        : (30 + ((m + (m / 8)) % 2));
    if (d > last) d = last;
    return ymd_to_jdn(y, m, d);
}

static int next_coupon(int settle_jdn, int mat_jdn, int frequency)
{
    int months = 12 / frequency;
    int coupon = mat_jdn;
    while (coupon > settle_jdn) coupon = add_months_jdn(coupon, -months);
    return add_months_jdn(coupon, months);
}

static int prev_coupon(int next_jdn, int frequency)
{
    return add_months_jdn(next_jdn, -(12 / frequency));
}

static double day_count_360(int j1, int j2, int basis)
{
    int y1, m1, d1, y2, m2, d2;
    jdn_to_ymd(j1, &y1, &m1, &d1);
    jdn_to_ymd(j2, &y2, &m2, &d2);
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

static int valid_freq(int f) { return f == 1 || f == 2 || f == 4; }

/* Modello Trading.Bonds: cedole riferite a nominale 100, rateo (E-DSC)/E. */
static double trd_bond_price(double oa_settle, double oa_mat, double rate, double yld,
                             double redemption, int frequency)
{
    int settle_jdn = oa_to_jdn(oa_settle);
    int mat_jdn = oa_to_jdn(oa_mat);
    int nc = next_coupon(settle_jdn, mat_jdn, frequency);
    int n = 0, coupon = nc;
    while (coupon <= mat_jdn) { n++; coupon = add_months_jdn(coupon, 12 / frequency); }
    double dsc = day_count_360(settle_jdn, nc, 0);
    double e = day_count_360(prev_coupon(nc, frequency), nc, 0);
    if (e == 0.0) e = 1.0;
    double coupon_amount = 100.0 * rate / frequency;
    double discount = 1.0 + yld / frequency;
    double price = redemption / pow(discount, n - 1 + dsc / e);
    for (int i = 1; i <= n; i++)
        price += coupon_amount / pow(discount, i - 1 + dsc / e);
    price -= coupon_amount * ((e - dsc) / e);
    return price;
}

static double trd_bond_yield(double oa_settle, double oa_mat, double rate, double pr,
                             double redemption, int frequency)
{
    double yld = 0.1;
    for (int i = 0; i < 100; i++)
    {
        double p = trd_bond_price(oa_settle, oa_mat, rate, yld, redemption, frequency);
        double dp = (trd_bond_price(oa_settle, oa_mat, rate, yld + 1e-6, redemption, frequency) - p) / 1e-6;
        if (fabs(dp) < 1e-12) break;
        double ny = yld - (p - pr) / dp;
        if (fabs(ny - yld) < 1e-10) return ny;
        yld = ny;
    }
    return yld;
}

/* Modello Numerics.Price (basis 0): periodi frazionari e rateo (1-DSC/E). */
static double num_bond_price(double oa_settle, double oa_mat, double rate, double yld,
                             double redemption, int frequency, int basis)
{
    int settle_jdn = oa_to_jdn(oa_settle);
    int mat_jdn = oa_to_jdn(oa_mat);
    int nc = next_coupon(settle_jdn, mat_jdn, frequency);
    int n = 0, coupon = nc;
    while (coupon <= mat_jdn) { n++; coupon = add_months_jdn(coupon, 12 / frequency); }
    double dsc = day_count_360(settle_jdn, nc, basis);
    double e = day_count_360(prev_coupon(nc, frequency), nc, basis);
    if (e == 0.0) e = 1.0;
    double ai = (rate * redemption / frequency) * (1.0 - dsc / e);
    double price = 0.0;
    for (int k = 1; k <= n; k++)
    {
        double t = (dsc + e * (k - 1)) / e;
        price += (rate * redemption / frequency) / pow(1.0 + yld / frequency, t);
    }
    price += redemption / pow(1.0 + yld / frequency, (dsc + e * n) / e);
    return price - ai;
}

static double num_bond_yield(double oa_settle, double oa_mat, double rate, double price,
                             double redemption, int frequency, int basis)
{
    double lower = 0.0, upper = 1.0, yld = 0.05;
    for (int i = 0; i < 100; i++)
    {
        double test = num_bond_price(oa_settle, oa_mat, rate, yld, redemption, frequency, basis);
        if (fabs(test - price) < 1e-7) return yld;
        if (test > price) lower = yld; else upper = yld;
        yld = (lower + upper) / 2.0;
    }
    return yld;
}

double gq_bond_price(double settle_oa, double maturity_oa, double rate, double yld,
                     double redemption, int frequency, int model)
{
    if (settle_oa >= maturity_oa || !valid_freq(frequency)) return 0.0;
    if (rate < 0.0 || yld < 0.0) return 0.0;
    return model == 1
        ? num_bond_price(settle_oa, maturity_oa, rate, yld, redemption, frequency, 0)
        : trd_bond_price(settle_oa, maturity_oa, rate, yld, redemption, frequency);
}

double gq_bond_yield(double settle_oa, double maturity_oa, double rate, double price,
                     double redemption, int frequency, int model)
{
    if (settle_oa >= maturity_oa || !valid_freq(frequency)) return 0.0;
    return model == 1
        ? num_bond_yield(settle_oa, maturity_oa, rate, price, redemption, frequency, 0)
        : trd_bond_yield(settle_oa, maturity_oa, rate, price, redemption, frequency);
}

double gq_bond_equiv_yield(double face_value, double bond_price, double days_to_maturity)
{
    if (bond_price == 0.0 || days_to_maturity == 0.0) return 0.0;
    return ((face_value - bond_price) / bond_price) * (365.0 / days_to_maturity);
}

double gq_current_yield(double annual_coupons, double bond_price)
{
    if (bond_price == 0.0) return 0.0;
    return annual_coupons / bond_price;
}

double gq_duration(double settle_oa, double maturity_oa, double coupon_rate, double yld,
                   int frequency, int basis, int modified)
{
    (void)basis;
    if (!valid_freq(frequency)) return 0.0;
    int n = (int)((maturity_oa - settle_oa) / 365.0 * frequency + 0.5);
    if (n <= 0) return 0.0;
    double pv = 0.0, weighted = 0.0;
    for (int k = 1; k <= n; k++)
    {
        double cf = coupon_rate / frequency;
        double t = (double)k / frequency;
        double df = pow(1.0 + yld / frequency, k);
        pv += cf / df;
        weighted += t * cf / df;
    }
    pv += 1.0 / pow(1.0 + yld / frequency, n);
    weighted += ((double)n / frequency) * (1.0 / pow(1.0 + yld / frequency, n));
    double dur = weighted / pv;
    return modified ? dur / (1.0 + yld / frequency) : dur;
}

double gq_price_simple(double settle_oa, double maturity_oa, double rate, double yld,
                       double redemption, int frequency)
{
    if (!valid_freq(frequency)) return 0.0;
    int n = (int)floor(((maturity_oa - settle_oa) / 365.0) * frequency + 0.5);
    if (n <= 0) return 0.0;
    double price = 0.0;
    for (int k = 1; k <= n; k++)
        price += (rate * redemption / frequency) / pow(1.0 + yld / frequency, k);
    price += redemption / pow(1.0 + yld / frequency, n);
    return price;
}

double gq_ytm(double coupon_payment, double face_value, double price, double years)
{
    if (years == 0.0) return 0.0;
    double denominator = (face_value + price) / 2.0;
    if (denominator == 0.0) return 0.0;
    double numerator = coupon_payment + ((face_value - price) / years);
    return numerator / denominator;
}

double gq_zcb_value(double face_value, double rate, double time)
{
    if (1.0 + rate == 0.0) return 0.0;
    return face_value / pow(1.0 + rate, time);
}

double gq_zcb_yield(double face_value, double price, double time)
{
    if (time == 0.0 || price == 0.0) return 0.0;
    return pow(face_value / price, 1.0 / time) - 1.0;
}
