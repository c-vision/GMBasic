/*
 * gm_combin.h — combinatoria e teoria dei numeri (da numC).
 *
 * Fattoriali, binomiali, permutazioni/combinazioni (con e senza ripetizione),
 * Catalan, Stirling, primalità, numeri perfetti, gcd/lcm, fattorizzazione,
 * totiente di Eulero.
 */
#ifndef GMMATH_GM_COMBIN_H
#define GMMATH_GM_COMBIN_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- combinatoria ---- */
gm_real gm_comb_factorial(int n);              /* n! */
gm_real gm_comb_binomial(int n, int k);        /* C(n,k) */
gm_real gm_comb_perm(int n, int k);            /* P(n,k) = n!/(n-k)! */
gm_real gm_comb_perm_rep(int n, int k);        /* n^k (disposizioni con rip.) */
gm_real gm_comb_comb_rep(int n, int k);        /* C(n+k-1, k) (comb. con rip.) */
gm_real gm_comb_catalan(int n);                /* numero di Catalan */
gm_real gm_comb_stirling2(int n, int k);       /* Stirling di seconda specie */
gm_real gm_comb_bell(int n);                   /* numero di Bell */

/* ---- teoria dei numeri ---- */
int gm_num_is_prime(long long n);
int gm_num_is_perfect(long long n);
long long gm_num_gcd(long long a, long long b);
long long gm_num_lcm(long long a, long long b);
/* Fattorizzazione: scrive i primi in primes[] e gli esponenti in exps[].
 * Ritorna il numero di fattori (<= maxf). */
int gm_num_factorize(long long n, long long *primes, int *exps, int maxf);
long long gm_num_totient(long long n);          /* φ(n) */
int gm_num_is_square(long long n);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_COMBIN_H */
