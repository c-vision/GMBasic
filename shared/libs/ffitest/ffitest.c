/* ffitest.c — libreria di test per il marshalling FFI oltre gli 8 argomenti.
 * Funzioni deterministiche, senza dipendenze: ogni test somma/combina i valori. */
#include <stdint.h>

double ft_sum9(double a1, double a2, double a3, double a4, double a5,
               double a6, double a7, double a8, double a9)
{ return a1+a2+a3+a4+a5+a6+a7+a8+a9; }

double ft_sum12(double a1, double a2, double a3, double a4, double a5, double a6,
                double a7, double a8, double a9, double a10, double a11, double a12)
{ return a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12; }

double ft_sum16(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
                double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16)
{ return a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12+a13+a14+a15+a16; }

int64_t ft_isum12(int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6,
                  int64_t a7, int64_t a8, int64_t a9, int64_t a10, int64_t a11, int64_t a12)
{ return a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12; }

/* Firma mista: interi e double alternati, ritorno double. */
double ft_mix12(int64_t a, double b, int64_t c, double d, int64_t e, double f,
                int64_t g, double h, int64_t i, double j, int64_t k, double l)
{ return (double)(a+c+e+g+i+k) + b+d+f+h+j+l; }

/* Firma mista con ritorno intero. */
int64_t ft_mixret(int64_t a, double b, int64_t c, double d)
{ return a + c + (int64_t)(b + d); }

/* Array oltre l'ottavo argomento (scratch/copy-out). */
double ft_arr_tail(const double* v, int n, double a1, double a2, double a3, double a4, double a5,
                   double a6, double a7, double a8, double a9)
{
    double s = a1+a2+a3+a4+a5+a6+a7+a8+a9;
    for (int i = 0; i < n; i++) s += v[i];
    return s;
}

/* Firma mista a 9 argomenti: bit 8 della maschera (ultimo argomento intero). */
double ft_mix9(int64_t a, double b, int64_t c, double d, int64_t e, double f,
               int64_t g, double h, int64_t i)
{ return (double)(a+c+e+g+i) + b+d+f+h; }

/* 20 argomenti double: dimostra che il limite non è più 16. */
double ft_sum20(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
                double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
                double a17, double a18, double a19, double a20)
{ return a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12+a13+a14+a15+a16+a17+a18+a19+a20; }

/* 40 argomenti double: oltre il vecchio tetto delle maschere a 32 bit. */
double ft_sum40(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
                double a9, double a10, double a11, double a12, double a13, double a14, double a15, double a16,
                double a17, double a18, double a19, double a20, double a21, double a22, double a23, double a24,
                double a25, double a26, double a27, double a28, double a29, double a30, double a31, double a32,
                double a33, double a34, double a35, double a36, double a37, double a38, double a39, double a40)
{ return a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12+a13+a14+a15+a16+a17+a18+a19+a20+a21+a22+a23+a24+a25+a26+a27+a28+a29+a30+a31+a32+a33+a34+a35+a36+a37+a38+a39+a40; }
