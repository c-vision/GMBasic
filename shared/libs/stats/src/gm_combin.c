/* gm_combin.c — combinatoria e teoria dei numeri (da numC). */
#include "gm_combin.h"

#include <math.h>
#include <stdlib.h>

gm_real gm_comb_factorial(int n)
{
    gm_real f = 1.0;
    int i;
    if (n < 0) return 0.0;
    for (i = 2; i <= n; i++) f *= (gm_real)i;
    return f;
}

gm_real gm_comb_binomial(int n, int k)
{
    gm_real r = 1.0;
    int i;
    if (k < 0 || k > n || n < 0) return 0.0;
    if (k > n - k) k = n - k;
    for (i = 0; i < k; i++) r = r * (gm_real)(n - i) / (gm_real)(i + 1);
    return r;
}

gm_real gm_comb_perm(int n, int k)
{
    gm_real r = 1.0;
    int i;
    if (k < 0 || k > n || n < 0) return 0.0;
    for (i = 0; i < k; i++) r *= (gm_real)(n - i);
    return r;
}

gm_real gm_comb_perm_rep(int n, int k)
{
    gm_real r = 1.0;
    int i;
    if (n < 0 || k < 0) return 0.0;
    for (i = 0; i < k; i++) r *= (gm_real)n;
    return r;
}

gm_real gm_comb_comb_rep(int n, int k)
{
    if (n <= 0 || k < 0) return 0.0;
    return gm_comb_binomial(n + k - 1, k);
}

gm_real gm_comb_catalan(int n)
{
    if (n < 0) return 0.0;
    return gm_comb_binomial(2 * n, n) / (gm_real)(n + 1);
}

gm_real gm_comb_stirling2(int n, int k)
{
    /* S(n,k) = S(n-1,k-1) + k S(n-1,k) */
    gm_real *prev, *cur, res;
    int i, j;
    if (n < 0 || k < 0 || k > n) return 0.0;
    prev = (gm_real *)calloc((size_t)(k + 1), sizeof(gm_real));
    cur = (gm_real *)calloc((size_t)(k + 1), sizeof(gm_real));
    if (!prev || !cur) { free(prev); free(cur); return 0.0; }
    prev[0] = 1.0;
    for (i = 1; i <= n; i++) {
        for (j = 0; j <= k; j++) cur[j] = (j > 0 ? prev[j - 1] : 0.0) + (gm_real)j * prev[j];
        { gm_real *t = prev; prev = cur; cur = t; }
    }
    res = prev[k];
    free(prev); free(cur);
    return res;
}

gm_real gm_comb_bell(int n)
{
    gm_real s = 0.0;
    int k;
    if (n < 0) return 0.0;
    for (k = 0; k <= n; k++) s += gm_comb_stirling2(n, k);
    return s;
}

/* ---- teoria dei numeri ---- */

int gm_num_is_prime(long long n)
{
    long long i;
    if (n < 2) return 0;
    if (n < 4) return 1;
    if (n % 2 == 0) return 0;
    for (i = 3; i * i <= n; i += 2) if (n % i == 0) return 0;
    return 1;
}

int gm_num_is_perfect(long long n)
{
    long long i, sum = 1;
    if (n <= 1) return 0;
    for (i = 2; i * i <= n; i++) if (n % i == 0) { sum += i; if (i != n / i) sum += n / i; }
    return sum == n;
}

long long gm_num_gcd(long long a, long long b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { long long t = a % b; a = b; b = t; }
    return a;
}

long long gm_num_lcm(long long a, long long b)
{
    long long g;
    if (a == 0 || b == 0) return 0;
    g = gm_num_gcd(a, b);
    return (a / g) * b;
}

int gm_num_factorize(long long n, long long *primes, int *exps, int maxf)
{
    int nf = 0;
    long long p;
    if (n < 2 || primes == NULL || exps == NULL || maxf <= 0) return 0;
    for (p = 2; p * p <= n && nf < maxf; p++) {
        if (n % p == 0) {
            int e = 0;
            while (n % p == 0) { n /= p; e++; }
            primes[nf] = p; exps[nf] = e; nf++;
        }
    }
    if (n > 1 && nf < maxf) { primes[nf] = n; exps[nf] = 1; nf++; }
    return nf;
}

long long gm_num_totient(long long n)
{
    long long result, p;
    if (n <= 0) return 0;
    if (n == 1) return 1;
    result = n;
    for (p = 2; p * p <= n; p++) {
        if (n % p == 0) {
            while (n % p == 0) n /= p;
            result -= result / p;
        }
    }
    if (n > 1) result -= result / n;
    return result;
}

int gm_num_is_square(long long n)
{
    long long r;
    if (n < 0) return 0;
    r = (long long)(sqrt((double)n) + 0.5);
    return (r * r == n) ? 1 : 0;
}
