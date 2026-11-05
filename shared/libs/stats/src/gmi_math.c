/* ============================================================================
 * gmi_math.c — builtin matematiche/stringhe (ROUND/RND/CHR).
 * Parte di gmi_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmi_runtime.h"
#include <math.h>
#include "gc_array.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

/* ROUND(value, digits): arrotonda (MidpointRounding.ToEven, come .NET Math.Round). */
 double gmi_round_toeven(double v) {
    double f = floor(v);
    double diff = v - f;
    if (diff > 0.5) return f + 1.0;
    if (diff < 0.5) return f;
    /* esattamente 0.5: va al pari */
    return (fmod(f, 2.0) == 0.0) ? f : f + 1.0;
}
static const unsigned long long gmi_pow10_ull[19] = {
    1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL, 10000000ULL,
    100000000ULL, 1000000000ULL, 10000000000ULL, 100000000000ULL, 1000000000000ULL,
    10000000000000ULL, 100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL,
    100000000000000000ULL, 1000000000000000000ULL };

/* ROUND(value, digits) = Math.Round((decimal)value, digits) (.NET MidpointRounding.ToEven).
 *
 * Il canone .NET opera su decimal: (decimal)value arrotonda il double alle 15 cifre
 * significative (half-even), poi Math.Round tronca/arrotonda alle `digits` decimali con
 * half-even. Una moltiplicazione in double (value*factor) NON replica il canone: su arm64
 * 2.345*100 = 234.50000000000003 supera il mezzo e darebbe 2.35 invece di 2.34. Qui si
 * lavora sulla mantissa decimale esatta del double a 15 cifre significative (via %.14e,
 * locale-independent) e si arrotonda con aritmetica intera half-even. */
gmb_double_t gmi_round(gmb_double_t value, gmb_int_t digits) {
    if (digits < 0) digits = 0;
    if (!isfinite(value) || value == 0.0) return value;
    int neg = value < 0;
    char buf[40];
    snprintf(buf, sizeof buf, "%.14e", neg ? -value : value);
    unsigned long long M = 0;
    int seen = 0, exp10 = 0;
    const char* p = buf;
    if (*p == '+' || *p == '-') p++;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') { M = M * 10ULL + (unsigned long long)(*p - '0'); seen++; }
        else if (*p == 'e' || *p == 'E') { exp10 = atoi(p + 1); break; }
    }
    if (!seen) return value;
    long long e = (long long)exp10 - 14;          /* value = M × 10^e (M a 15 cifre sig) */
    long long sh = e + digits;                    /* value×10^digits = M × 10^sh */
    double res;
    if (sh >= 0) {
        /* gia' intero in scala 10^digits: nessun round, restituisci il numero a 15 sig */
        res = (double)M * pow(10.0, (double)e);
        return neg ? -res : res;
    }
    unsigned long long h = (unsigned long long)(-sh);
    if (h > 18) {
        /* M ≤ 999999999999999 < 10^19: il round di M/10^h e' 0 (2M < 10^h) */
        return neg ? -0.0 : 0.0;
    }
    unsigned long long T = gmi_pow10_ull[h];
    unsigned long long q = M / T;
    unsigned long long r = M % T;
    if (r != 0) {
        if (2 * r > T) q++;                 /* oltre il mezzo → su */
        else if (2 * r == T) { if (q & 1ULL) q++; }  /* mezzo esatto → al pari */
    }
    res = (double)q * pow(10.0, (double)(-digits));
    return neg ? -res : res;
}

/* RND(): pseudo-casuale 0..1 (xorshift64, range [0,1)). RANDOMIZE semina lo stato. */
static unsigned long long g_rnd_state = 0x2545F4914F6CDD1DULL;
void gmi_rnd_seed(uint64_t seed) { g_rnd_state = seed ? seed : 0x2545F4914F6CDD1DULL; }
gmb_double_t gmi_rnd(void) {
    g_rnd_state ^= g_rnd_state << 13;
    g_rnd_state ^= g_rnd_state >> 7;
    g_rnd_state ^= g_rnd_state << 17;
    return (gmb_double_t)(g_rnd_state >> 11) / 9007199254740992.0;
}

/* PI: costante π (builtin PI()/PI). */
gmb_double_t gmi_pi(void) { return 3.14159265358979323846; }

/* MIN/MAX scalari (builtin MIN(a,b,...) / MAX(a,b,...)). */
gmb_double_t gmi_min(gmb_double_t a, gmb_double_t b) { return a < b ? a : b; }
gmb_double_t gmi_max(gmb_double_t a, gmb_double_t b) { return a > b ? a : b; }



/* CHR(code): carattere (code point UTF-8, come char.ConvertFromUtf32). */

/* TIMER(): millisecondi dalla mezzanotte (TimeService.Timer, wrap 24h). */
gmb_int_t gmi_timer(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (gmb_int_t)(GetTickCount64() % 86400000);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t now = ts.tv_sec;
    struct tm* lt = localtime(&now);
    long ms = lt ? (long)(lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec) * 1000 + ts.tv_nsec / 1000000 : 0;
    return (gmb_int_t)(ms % 86400000);
#endif
}

gmb_double_t gmi_int(gmb_double_t v) { return floor(v); }

/* CHOOSE(index, values...) — indice 1-based nella lista valori (ParamArray). Ritorna il valore
 * scelto (gmi_val_t unificato) o gmi_val_nil() se l'indice è fuori range. */
gmb_double_t gmi_floor(gmb_double_t v) { return floor(v); }
gmb_double_t gmi_ceiling(gmb_double_t v) { return ceil(v); }
gmb_double_t gmi_abs(gmb_double_t v) { return fabs(v); }
gmb_double_t gmi_clamp(gmb_double_t v, gmb_double_t lo, gmb_double_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
gmb_double_t gmi_max_arr(const gmb_double_t* a, int n) {
    if (n <= 0 || !a) return 0;
    double m = a[0];
    for (int i = 1; i < n; i++) if (a[i] > m) m = a[i];
    return m;
}
gmb_double_t gmi_min_arr(const gmb_double_t* a, int n) {
    if (n <= 0 || !a) return 0;
    double m = a[0];
    for (int i = 1; i < n; i++) if (a[i] < m) m = a[i];
    return m;
}
gmb_double_t gmi_sum_arr(const gmb_double_t* a, int n) {
    if (n <= 0 || !a) return 0;
    double s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}
gmb_double_t gmi_dist2d(gmb_double_t x1, gmb_double_t y1, gmb_double_t x2, gmb_double_t y2) {
    return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
}
gmb_double_t gmi_dist3d(gmb_double_t x1, gmb_double_t y1, gmb_double_t z1, gmb_double_t x2, gmb_double_t y2, gmb_double_t z2) {
    return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) + (z1-z2)*(z1-z2));
}
gmb_double_t gmi_percentile(const gmb_double_t* a, int n, gmb_double_t p) {
    if (n <= 0 || !a) return 0;
    double* tmp = (double*)malloc(sizeof(double) * n);
    if (!tmp) return 0;
    memcpy(tmp, a, sizeof(double) * n);
    for (int i = 1; i < n; i++) {
        double key = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j] > key) { tmp[j+1] = tmp[j]; j--; }
        tmp[j+1] = key;
    }
    /* PCodeVm PERCENTILE: p è PERCENTUALE [0,100], idx=(p/100)*(n-1) */
    double pos = (n - 1) * (p / 100.0);
    int lo = (int)floor(pos);
    if (lo >= n) lo = n - 1;
    int hi = (int)ceil(pos);
    if (hi >= n) hi = n - 1;
    if (hi < lo) hi = lo;
    double res = tmp[lo];
    if (hi != lo) res = tmp[lo] + (tmp[hi] - tmp[lo]) * (pos - lo);
    free(tmp);
    return res;
}
gmb_int_t gmi_is_numeric(const char* s) {
    if (!s) return 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (!*s) return 0;
    /* Parità IL (decimal.TryParse NumberStyles.Any): NIENTE esadecimale né inf/nan —
     * strtod li accetterebbe ("0x1F"=31, "inf", "nan") mentre IL dice non-numerico. */
    const char* p = s;
    if (*p == '+' || *p == '-') p++;
    if ((p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))) return 0;
    if ((p[0]|32) == 'i' || (p[0]|32) == 'n') {
        char a = (char)(p[0]|32), b = (char)(p[1]|32), c = (char)(p[2]|32);
        if ((a == 'i' && b == 'n' && c == 'f') || (a == 'n' && b == 'a' && c == 'n')) return 0;
    }
    char* end = NULL;
    strtod(s, &end);
    if (!end || end == s) return 0;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    return (*end == '\0') ? 1 : 0;
}

/* STR(value): .NET double.ToString("G") con spazio iniziale per i positivi. */
char* gmi_fmt_g_net(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.15g", v);
    /* converti esponente in E+xx maiuscolo (stile .NET G) */
    char* e = strchr(buf, 'e');
    if (e) {
        *e = 'E';
        if (e[1] != '-' && e[1] != '+') { memmove(e + 2, e + 1, strlen(e + 1) + 1); e[1] = '+'; }
        /* .NET usa almeno 2 cifre esponente */
        if (strlen(e + 2) < 2 && e[1] != '-' && e[1] != '+') { memmove(e + 3, e + 2, strlen(e + 2) + 1); e[2] = '0'; }
    }
    /* Spazio iniziale per i non negativi (parità STR$/CSTR con IL/PCode, semantica classica
     * BASIC: il segno riserva sempre una colonna). L'esclusione precedente per "buf è
     * esattamente '0'" era un bug: STR(0)/CSTR(0) e la concatenazione implicita di uno zero
     * (es. `s = s + STR(i)` in un ciclo FOR i=0 TO n) perdevano lo spazio SOLO per il valore
     * zero, unico caso divergente da IL/PCode (verificato: `STR(0)` dava "0" invece di " 0",
     * mentre STR di qualunque altro non negativo era già corretto). */
    if (v >= 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), " %s", buf);
        strcpy(buf, tmp);
    }
    return strdup(buf);
}

/* VAL(s): estrae il prefisso numerico iniziale e ignora il resto (ConversionUtils.Val). */
gmb_double_t gmi_parse_num(const char* s, int* ok) {
    if (ok) *ok = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;
    /* numero: [+-]? cifre [. cifre] [eE[+-]cifre] */
    const char* p = s;
    if (*p == '+' || *p == '-') p++;
    int digits = 0;
    while (*p >= '0' && *p <= '9') { p++; digits++; }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') { p++; digits++; }
    }
    if (digits == 0 && !(*s == '.' && s[1] >= '0' && s[1] <= '9')) return 0;
    if (*p == 'e' || *p == 'E') {
        const char* q = p + 1;
        if (*q == '+' || *q == '-') q++;
        int ed = 0;
        while (*q >= '0' && *q <= '9') { q++; ed++; }
        if (ed > 0) p = q;
    }
    char tmp[128];
    size_t len = (size_t)(p - s);
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, s, len);
    tmp[len] = 0;
    if (ok) *ok = 1;
    return atof(tmp);
}

gmb_double_t gmi_fix(gmb_double_t v) { return trunc(v); }
gmb_double_t gmi_trunc(gmb_double_t v) { return trunc(v); }
gmb_double_t gmi_sqr(gmb_double_t v) { return sqrt(v); }
gmb_double_t gmi_sgn(gmb_double_t v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
gmb_double_t gmi_cbyte(gmb_double_t v) { long t = (long)v; if (t < 0) t = 0; if (t > 255) t = 255; return (gmb_double_t)(unsigned char)t; }
gmb_double_t gmi_hypot(gmb_double_t a, gmb_double_t b) { return hypot(a, b); }
gmb_double_t gmi_yearfrac(gmb_double_t d1, gmb_double_t d2) {
    /* C# YEARFRAC: (d2-d1).TotalDays / 365.0 — d1/d2 sono OADate in giorni. */
    return fabs(d2 - d1) / 365.0;
}

gmb_double_t gmi_fact(gmb_double_t n) {
    if (n < 0) return 0;
    long long nn = (long long)n;
    double r = 1;
    for (long long i = 2; i <= nn; i++) r *= (double)i;
    return r;
}
gmb_double_t gmi_perm(gmb_double_t n, gmb_double_t k) {
    long long nn = (long long)n, kk = (long long)k;
    if (kk < 0 || kk > nn) return 0;
    double r = 1;
    for (long long i = 0; i < kk; i++) r *= (double)(nn - i);
    return r;
}
gmb_double_t gmi_comb(gmb_double_t n, gmb_double_t k) {
    long long nn = (long long)n, kk = (long long)k;
    if (kk < 0 || kk > nn) return 0;
    if (kk > nn - kk) kk = nn - kk;
    double r = 1;
    for (long long i = 1; i <= kk; i++) r = r * (double)(nn - kk + i) / (double)i;
    return r;
}

gmb_double_t gmi_polarx(gmb_double_t r, gmb_double_t angle_deg) {
    return r * cos(angle_deg * 3.141592653589793 / 180.0);
}
gmb_double_t gmi_polary(gmb_double_t r, gmb_double_t angle_deg) {
    return r * sin(angle_deg * 3.141592653589793 / 180.0);
}
gmb_double_t gmi_angle2d(gmb_double_t x1, gmb_double_t y1, gmb_double_t x2, gmb_double_t y2) {
    return atan2(y2 - y1, x2 - x1) * 180.0 / 3.141592653589793;
}
gmb_double_t gmi_pythagoras(gmb_double_t a, gmb_double_t b) {
    return sqrt(a * a + b * b);
}

static double gmi_next_double(void) {
    return gmi_rnd();
}
/* Sorgente casuale SEPARATA per RANDOMINT/RANDOMDOUBLE/RANDOMNORMAL: il canone C#
 * (IlRuntime → CryptoUtils.CoreRandom*) usa un Random .NET distinto che RANDOMIZE
 * NON semina (RANDOMIZE semina solo g_rnd_state di RND). gmi_rnd/g_rnd_state qui
 * condividerlo farebbe sì che RANDOMIZE influenzi queste funzioni (divergenza
 * silenziosa da IL/PCode). Stato lazy-init non deterministico (tempo + stack addr). */
static unsigned long long g_crnd_state = 0;
static double gmi_next_crnd(void) {
    if (g_crnd_state == 0) {
        g_crnd_state = ((unsigned long long)(uintptr_t)&g_crnd_state << 32)
                     ^ (unsigned long long)(time(NULL) ^ 0x9E3779B97F4A7C15ULL);
        if (g_crnd_state == 0) g_crnd_state = 0x2545F4914F6CDD1DULL;
    }
    g_crnd_state ^= g_crnd_state << 13;
    g_crnd_state ^= g_crnd_state >> 7;
    g_crnd_state ^= g_crnd_state << 17;
    return (double)(g_crnd_state >> 11) / 9007199254740992.0;
}
double gmi_rand_int(double min, double max) {
    int lo = (int)min, hi = (int)max;
    if (hi <= lo) return lo;
    return (double)(lo + (int)(gmi_next_crnd() * (hi - lo)));
}
double gmi_rand_double(double min, double max) {
    return gmi_next_crnd() * (max - min) + min;
}
double gmi_rand_normal(double mean, double stddev) {
    double u1 = 1.0 - gmi_next_crnd();
    double u2 = 1.0 - gmi_next_crnd();
    double z = sqrt(-2.0 * log(u1)) * sin(2.0 * 3.141592653589793 * u2);
    return mean + stddev * z;
}
uint8_t* gmi_rand_bytes(int count) {
    if (count < 0) count = 0;
    uint8_t* out = (uint8_t*)malloc((size_t)(count ? count : 1));
    if (!out) return NULL;
    for (int i = 0; i < count; i++) out[i] = (uint8_t)(gmi_next_double() * 256.0);
    return out;
}
/* RANDOMBYTES(n) → byte[] come gmi_string_t binaria (per C99Native byte-expr/DIM). */
/* RANDOMCRYPTOBYTES(n) → byte[] da sorgente CRITTOGRAFICA (/dev/urandom; fallback PRNG). */

double gmi_correlation(const double* x, const double* y, int n) {
    double mx, my, sxy, sx2, sy2;
    if (n < 2) return 0;
    mx = gc_array_sum(x, n) / n;
    my = gc_array_sum(y, n) / n;
    sxy = gc_array_dot_dev(x, y, n, mx, my);   /* SIMD */
    sx2 = gc_array_sum_sq_dev(x, n, mx);
    sy2 = gc_array_sum_sq_dev(y, n, my);
    return (sx2 > 0 && sy2 > 0) ? sxy / sqrt(sx2 * sy2) : 0.0;
}

static double gmi_median_sorted(double* s, int n) {
    if (n <= 0) return 0;
    if (n % 2 == 0) return (s[n/2-1] + s[n/2]) / 2.0;
    return s[n/2];
}
static void gmi_sort_arr(double* s, int n) {
    for (int i = 1; i < n; i++) { double k = s[i]; int j = i-1; while (j >= 0 && s[j] > k) { s[j+1]=s[j]; j--; } s[j+1]=k; }
}
double gmi_mean(const double* a, int n) {
    if (n <= 0) return 0;
    return gc_array_sum(a, n) / n;   /* SIMD (gc_array) */
}
double gmi_median(const double* a, int n) {
    if (n <= 0) return 0;
    double* s = (double*)malloc(sizeof(double) * n);
    if (!s) return 0;
    memcpy(s, a, sizeof(double) * n);
    gmi_sort_arr(s, n);
    double r = gmi_median_sorted(s, n);
    free(s);
    return r;
}
double gmi_variance(const double* a, int n) {
    if (n < 2) return 0;
    return gc_array_sum_sq_dev(a, n, gmi_mean(a, n)) / (n - 1);   /* SIMD */
}
double gmi_stddev(const double* a, int n) {
    if (n < 2) return 0;
    return sqrt(gmi_variance(a, n));
}
double gmi_skewness(const double* a, int n) {
    if (n < 3) return 0;
    double avg = gmi_mean(a, n);
    double m2 = 0, m3 = 0;
    for (int i = 0; i < n; i++) { double d = a[i]-avg; m2 += d*d; m3 += d*d*d; }
    m2 /= n; m3 /= n;
    double s3 = pow(m2, 1.5);
    if (s3 == 0) return 0;
    return (m3 / s3) * sqrt((double)n * (n - 1)) / (n - 2); /* G1 aggiustata (MathNet/C#) */
}
double gmi_kurtosis(const double* a, int n) {
    if (n < 4) return 0;
    double avg = gmi_mean(a, n);
    double m2 = 0, m4 = 0;
    for (int i = 0; i < n; i++) { double d = a[i]-avg; m2 += d*d; m4 += d*d*d*d; }
    m2 /= n; m4 /= n;
    if (m2 == 0) return 0;
    double g2 = m4 / (m2 * m2);
    return ((double)n - 1) / (((double)n - 2) * ((double)n - 3)) * (((double)n + 1) * g2 - 3.0 * ((double)n - 1)); /* eccesso non distorto (MathNet/C#) */
}
double gmi_mad(const double* a, int n) {
    if (n <= 0) return 0;
    double* s = (double*)malloc(sizeof(double) * n);
    if (!s) return 0;
    memcpy(s, a, sizeof(double) * n);
    gmi_sort_arr(s, n);
    double median = gmi_median_sorted(s, n);
    double* devs = (double*)malloc(sizeof(double) * n);
    if (!devs) { free(s); return 0; }
    for (int i = 0; i < n; i++) devs[i] = fabs(a[i] - median);
    gmi_sort_arr(devs, n);
    double r = gmi_median_sorted(devs, n);
    free(s); free(devs);
    return r;
}

/* ==================== Distribuzioni statistiche (formule standard) ==================== */

/* ── Nucleo scipy.signal (SIGNAL namespace) — double[] ── */
/* NB: lunghezze e outn come gmb_int_t (int64_t): i companion _len del C99 sono gmb_int_t. */
double* gmi_signal_convolve(const double* x, gmb_int_t nx, const double* k, gmb_int_t nk, gmb_int_t* outn)
{
    int n = (int)nx + (int)nk - 1;
    double* r = (double*)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
    if (!r) { *outn = 0; return NULL; }
    *outn = n;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < (int)nk; j++)
        {
            int xi = i - j;
            if (xi >= 0 && xi < (int)nx) r[i] += x[xi] * k[j];
        }
    return r;
}

double* gmi_signal_correlate(const double* x, gmb_int_t nx, const double* k, gmb_int_t nk, gmb_int_t* outn)
{
    int n = (int)nx + (int)nk - 1;
    double* r = (double*)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
    if (!r) { *outn = 0; return NULL; }
    *outn = n;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < (int)nk; j++)
        {
            int xi = i - ((int)nk - 1) + j;
            if (xi >= 0 && xi < (int)nx) r[i] += x[xi] * k[j];
        }
    return r;
}

double* gmi_signal_window(gmb_int_t n, int kind, gmb_int_t* outn)
{
    double* r = (double*)calloc((size_t)(n > 0 ? (size_t)n : 1), sizeof(double));
    if (!r) { *outn = 0; return NULL; }
    *outn = n;
    if (n <= 0) return r;
    if (n == 1) { r[0] = 1.0; return r; }
    double t = 2.0 * M_PI / ((double)n - 1.0);
    for (int i = 0; i < (int)n; i++)
    {
        if (kind == 0) r[i] = 0.5 * (1.0 - cos(t * i));                                    /* hann */
        else if (kind == 1) r[i] = 0.54 - 0.46 * cos(t * i);                                /* hamming */
        else r[i] = 0.42 - 0.5 * cos(t * i) + 0.08 * cos(2.0 * t * i);                       /* blackman */
    }
    return r;
}

/* ── Nuclei scipy.integrate / scipy.interpolate / scipy.fft (2026-08-25) ──
   Le funzioni closure-based (quad/simpson/ode) usano un callback context-aware:
   la VM/C passa un wrapper su gmi_vm_call_closure_sync con (vm, task, closure). */

static const double gmi_k15_nodes[7] = {
    0.9914553711208126, 0.9491079123427585, 0.8648644233597691, 0.7415311855993944,
    0.5860872354676911, 0.4058451513773972, 0.2077849550078985
};
static const double gmi_k15_w[8] = {
    0.0229353220105292, 0.0630920926299786, 0.1047900103222501, 0.1406532597155259,
    0.1690047266392679, 0.1903505780647854, 0.2044329400752989, 0.2094821410847278
};
static const double gmi_k7_w[4] = {
    0.1294849661688697, 0.2797053914892767, 0.3818300505051189, 0.4179591836734694
};

static double gmi_gk15(double (*f)(double, void*), void* ctx, double a, double b, double* err)
{
    double mid = 0.5 * (a + b), half = 0.5 * (b - a);
    double s15 = gmi_k15_w[7] * f(mid, ctx);
    double s7 = gmi_k7_w[3] * f(mid, ctx);
    int gidx[3] = { 1, 3, 5 }, g = 0;
    for (int i = 0; i < 7; i++)
    {
        double dx = half * gmi_k15_nodes[i];
        double f1 = f(mid - dx, ctx), f2 = f(mid + dx, ctx);
        s15 += gmi_k15_w[i] * (f1 + f2);
        if (g < 3 && i == gidx[g]) { s7 += gmi_k7_w[g] * (f1 + f2); g++; }
    }
    double r15 = s15 * half;
    *err = fabs(r15 - s7 * half);
    return r15;
}

static double gmi_gk_adaptive(double (*f)(double, void*), void* ctx, double a, double b, double thresh, int depth, double* err)
{
    double mid = 0.5 * (a + b), e1, e2;
    double L = gmi_gk15(f, ctx, a, mid, &e1);
    double R = gmi_gk15(f, ctx, mid, b, &e2);
    double e = e1 + e2;
    if (depth < 24 && e > thresh)
    {
        double tl, tr;
        double e2v = gmi_gk_adaptive(f, ctx, a, mid, thresh * 0.5, depth + 1, &tl)
                     + gmi_gk_adaptive(f, ctx, mid, b, thresh * 0.5, depth + 1, &tr);
        *err = e2v;
        return tl + tr;
    }
    *err = e;
    return L + R;
}

double gmi_quad_gk(double (*f)(double, void*), void* ctx, double a, double b)
{
    if (a == b) return 0.0;
    double e;
    double s = gmi_gk15(f, ctx, a, b, &e);
    double thresh = 1e-12 * (1.0 + fabs(s));
    return gmi_gk_adaptive(f, ctx, a, b, thresh, 0, &e);
}

double gmi_simpson_ctx(double (*f)(double, void*), void* ctx, double a, double b, int n)
{
    if (n < 2) n = 2;
    if (n % 2) n++;
    double h = (b - a) / n;
    double sum = f(a, ctx) + f(b, ctx);
    for (int i = 1; i < n; i++) sum += (i % 2 == 1 ? 4.0 : 2.0) * f(a + i * h, ctx);
    return sum * h / 3.0;
}

double gm_integrate_trapezoid(const double* y, const double* x, int n)
{
    if (n < 2) return 0.0;
    double s = 0;
    for (int i = 0; i < n - 1; i++) s += 0.5 * (y[i] + y[i + 1]) * (x[i + 1] - x[i]);
    return s;
}

double* gmi_cumtrapezoid(const double* y, const double* x, int n, gmb_int_t* outn)
{
    if (n < 2) { *outn = 0; return NULL; }
    double* r = (double*)malloc(sizeof(double) * (size_t)(n - 1));
    if (!r) { *outn = 0; return NULL; }
    double acc = 0;
    for (int i = 0; i < n - 1; i++)
    {
        acc += 0.5 * (y[i] + y[i + 1]) * (x[i + 1] - x[i]);
        r[i] = acc;
    }
    *outn = n - 1;
    return r;
}




static double* gmi_dense_solve(double* a, int n)
{
    /* a è la matrice n×n seguita dal rhs: risolvi a x = rhs (ultima colonna). */
    double* m = (double*)malloc(sizeof(double) * (size_t)n);
    if (!m) return NULL;
    for (int col = 0; col < n; col++)
    {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(a[r * (n + 1) + col]) > fabs(a[piv * (n + 1) + col])) piv = r;
        if (piv != col)
        {
            for (int j = col; j <= n; j++)
            {
                double t = a[col * (n + 1) + j]; a[col * (n + 1) + j] = a[piv * (n + 1) + j]; a[piv * (n + 1) + j] = t;
            }
        }
        for (int r = col + 1; r < n; r++)
        {
            double f = a[r * (n + 1) + col] / a[col * (n + 1) + col];
            for (int j = col; j <= n; j++) a[r * (n + 1) + j] -= f * a[col * (n + 1) + j];
        }
    }
    for (int r = n - 1; r >= 0; r--)
    {
        double s = a[r * (n + 1) + n];
        for (int j = r + 1; j < n; j++) s -= a[r * (n + 1) + j] * m[j];
        m[r] = s / a[r * (n + 1) + r];
    }
    free(a);
    return m;
}

double gm_interp_cubic_spline(const double* x, const double* y, int n, double tx)
{
    if (n < 2) return y[0];
    if (n == 2) return y[0] + (y[1] - y[0]) * (tx - x[0]) / (x[1] - x[0]);
    double* h = (double*)malloc(sizeof(double) * (size_t)(n - 1));
    if (!h) return y[0];
    for (int i = 0; i < n - 1; i++) h[i] = x[i + 1] - x[i];
    double* a = (double*)calloc((size_t)n * (n + 1), sizeof(double));
    if (!a) { free(h); return y[0]; }
    for (int i = 1; i < n - 1; i++)
    {
        a[i * (n + 1) + (i - 1)] = h[i - 1];
        a[i * (n + 1) + i] = 2.0 * (h[i - 1] + h[i]);
        a[i * (n + 1) + (i + 1)] = h[i];
        double dp = (y[i] - y[i - 1]) / h[i - 1];
        double dc = (y[i + 1] - y[i]) / h[i];
        a[i * (n + 1) + n] = 6.0 * (dc - dp);
    }
    a[0] = -h[1]; a[1] = h[0] + h[1]; a[2] = -h[0];
    a[(n - 1) * (n + 1) + (n - 3)] = -h[n - 2];
    a[(n - 1) * (n + 1) + (n - 2)] = h[n - 2] + h[n - 3];
    a[(n - 1) * (n + 1) + (n - 1)] = -h[n - 3];
    double* m = gmi_dense_solve(a, n);
    if (!m) { free(h); return y[0]; }
    int k = 0;
    if (tx <= x[0]) k = 0;
    else if (tx >= x[n - 1]) k = n - 2;
    else { while (k < n - 2 && tx > x[k + 1]) k++; }
    double hk = h[k], t = tx - x[k], u = x[k + 1] - tx;
    double val = m[k] * u * u * u / (6.0 * hk) + m[k + 1] * t * t * t / (6.0 * hk)
        + (y[k] - m[k] * hk * hk / 6.0) * (u / hk)
        + (y[k + 1] - m[k + 1] * hk * hk / 6.0) * (t / hk);
    free(h); free(m);
    return val;
}

double* gmi_fftfreq(int n, double d, gmb_int_t* outn)
{
    if (n <= 0) { *outn = 0; return NULL; }
    if (d == 0) d = 1.0;
    double* r = (double*)malloc(sizeof(double) * (size_t)n);
    if (!r) { *outn = 0; return NULL; }
    int pos = n - n / 2;
    for (int i = 0; i < pos; i++) r[i] = (double)i / ((double)n * d);
    for (int i = pos; i < n; i++) r[i] = (double)(i - n) / ((double)n * d);
    *outn = n;
    return r;
}

double* gmi_fftshift(const double* arr, int n, gmb_int_t* outn)
{
    if (!arr || n < 2) { *outn = 0; return NULL; }
    double* r = (double*)malloc(sizeof(double) * (size_t)n);
    if (!r) { *outn = 0; return NULL; }
    int shift = n - n / 2;
    for (int i = 0; i < n; i++) r[i] = arr[(i + shift) % n];
    *outn = n;
    return r;
}

double* gmi_ifftshift(const double* arr, int n, gmb_int_t* outn)
{
    if (!arr || n < 2) { *outn = 0; return NULL; }
    double* r = (double*)malloc(sizeof(double) * (size_t)n);
    if (!r) { *outn = 0; return NULL; }
    int shift = n / 2;
    for (int i = 0; i < n; i++) r[i] = arr[(i + shift) % n];
    *outn = n;
    return r;
}

/* scipy.stats.gamma (shape a, scale s) */

/* Chi-squared (distribuzione distinta da Chi): pdf/cdf usate da CHISQUAREDPDF/CDF
 * su tutte le gambe C (VMC/C99/C99Native) — parità scipy/IL. */


/* ── Inverse CDF (ppf) e ISF — parità scipy.stats 1.18 (formule identiche a CommonLib) ── */

/* ==================== Analisi numerica: regressione, interpolazioni, FFT, time series ==================== */

/* LinearRegression OLS → slope, intercept, r2 */
void gmi_analysis_linear_regression(const double* x, const double* y, int n, double* slope, double* intercept, double* r2)
{
    double sx = 0, sy = 0, sxy = 0, sx2 = 0, sy2 = 0;
    for (int i = 0; i < n; i++) { sx += x[i]; sy += y[i]; sxy += x[i]*y[i]; sx2 += x[i]*x[i]; sy2 += y[i]*y[i]; }
    double den = n*sx2 - sx*sx;
    if (den == 0) { *slope = 0; *intercept = (n > 0) ? sy/n : 0; *r2 = 0; return; }
    *slope = (n*sxy - sx*sy) / den;
    *intercept = (sy - (*slope)*sx) / n;
    double d2 = (n*sx2 - sx*sx)*(n*sy2 - sy*sy);
    double r = d2 > 0 ? (n*sxy - sx*sy) / sqrt(d2) : 0;
    *r2 = r * r;
}

static int gmi_interp_find_segment(const double* x, int n, double tx)
{
    if (tx <= x[0]) return 0;
    for (int i = 1; i < n; i++) if (tx < x[i]) return i - 1;
    return n - 2;
}

double gm_interp_linear(const double* x, const double* y, int n, double tx)
{
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    int i = gmi_interp_find_segment(x, n, tx);
    double t = (tx - x[i]) / (x[i+1] - x[i]);
    return y[i] + t * (y[i+1] - y[i]);
}

double gm_interp_step(const double* x, const double* y, int n, double tx)
{
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    for (int i = 1; i < n; i++) if (tx < x[i]) return y[i-1];
    return y[n-1];
}

double gmi_interp_clamp(const double* x, const double* y, int n, double tx, double (*fn)(const double*, const double*, int, double))
{
    if (tx < x[0]) return y[0];
    if (tx > x[n-1]) return y[n-1];
    return fn(x, y, n, tx);
}

double gm_interp_polynomial(const double* x, const double* y, int n, double tx)
{
    double* w = (double*)calloc(n, sizeof(double));
    if (!w) return 0;
    for (int j = 0; j < n; j++)
    {
        w[j] = 1.0;
        for (int k = 0; k < n; k++)
            if (k != j) w[j] /= (x[j] - x[k]);
    }
    (void)0;
    for (int j = 0; j < n; j++)
        if (fabs(tx - x[j]) < 1e-14) { free(w); return y[j]; }
    double num = 0, den = 0;
    for (int j = 0; j < n; j++)
    {
        double t = w[j] / (tx - x[j]);
        num += t * y[j]; den += t;
    }
    free(w);
    return den != 0 ? num / den : 0;
}

double gm_interp_akima(const double* x, const double* y, int n, double tx)
{
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    int i = gmi_interp_find_segment(x, n, tx);
    /* derivate agli estremi (finite differences con damping di Akima) */
    (void)0;
    int m = n - 1;
    double tmp[64];
    for (int j = 0; j < m; j++) tmp[j] = (y[j+1] - y[j]) / (x[j+1] - x[j]);
    /* pendenza in i: media pesata di tmp[i-1], tmp[i] con pesi w1=|tmp[i+1]-tmp[i]|, w2=|tmp[i-1]-tmp[i-2]| */
    double t0 = (i > 0) ? tmp[i-1] : tmp[0];
    double t1 = tmp[i];
    double t2 = (i+1 < m) ? tmp[i+1] : tmp[i];
    double t_1 = (i-1 > 0) ? tmp[i-2] : t0;
    double w1 = fabs(t2 - t1), w2 = fabs(t0 - t_1);
    double dk = (w1 + w2 != 0) ? (w1*t0 + w2*t1) / (w1 + w2) : 0.5*(t0 + t1);
    double dk1 = (i+1 < m) ? ((fabs(tmp[(i+1)+1] - t2) + fabs(t1 - t0)) != 0
                ? (fabs(tmp[i+2] - tmp[i+1])*t1 + fabs(tmp[i+1] - tmp[i])*tmp[i+1]) / (fabs(tmp[i+2]-tmp[i+1]) + fabs(tmp[i+1]-tmp[i]))
                : 0.5*(t1 + (i+1 < m ? tmp[i+1] : t1))) : t1;
    /* Akima da i a i+1: ricostruisce usando due punti precedenti/successivi. Qui usiamo Hermite cubico con pendenze dk,dk1 */
    double h = x[i+1] - x[i], t = (tx - x[i]) / h;
    double y0 = y[i], y1 = y[i+1];
    double a = 2*(y0-y1) + (dk + dk1)*h;
    double b = 3*(y1-y0) - (2*dk + dk1)*h;
    double c = dk*h;
    return a*t*t*t + b*t*t + c*t + y0;
}

/* PCHIP (Fritsch–Carlson) */
double gm_interp_pchip(const double* x, const double* y, int n, double tx)
{
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    int i = gmi_interp_find_segment(x, n, tx);
    double h[64], del[64];
    for (int j = 0; j < n-1; j++) { h[j] = x[j+1] - x[j]; del[j] = (y[j+1] - y[j]) / h[j]; }
    double d[64];
    if (n < 3) { d[0] = del[0]; if (n == 2) d[1] = del[0]; }
    else {
    /* PCHIP (scipy/MathNet canone): derivata al bordo con formula Fritsch-Carlson
     * estesa (non del[0]/del[n-2] nudi): s = ((2h0+h1) d0 - h0 d1)/(h0+h1), poi clamp
     * di monotonia (segno ≠ del0 → 0; overshoot su cambio segno → 3*del0). Senza questa
     * correzione il valore sui segmenti di bordo diverge dal canone IL/PCode (MathNet). */
    {
        double s0 = ((2*h[0] + h[1]) * del[0] - h[0] * del[1]) / (h[0] + h[1]);
        if ((s0 < 0) != (del[0] < 0)) d[0] = 0;
        else if ((del[0] < 0) != (del[1] < 0) && fabs(s0) > fabs(3*del[0])) d[0] = 3*del[0];
        else d[0] = s0;
        int k = n-1;
        double sk = ((2*h[k-1] + h[k-2]) * del[k-1] - h[k-1] * del[k-2]) / (h[k-1] + h[k-2]);
        if ((sk < 0) != (del[k-1] < 0)) d[k] = 0;
        else if ((del[k-1] < 0) != (del[k-2] < 0) && fabs(sk) > fabs(3*del[k-1])) d[k] = 3*del[k-1];
        else d[k] = sk;
    }
    for (int j = 1; j < n-1; j++)
    {
        if (del[j-1] * del[j] <= 0) d[j] = 0;
        else
        {
            double w1 = 2*h[j] + h[j-1], w2 = h[j] + 2*h[j-1];
            d[j] = (w1 + w2) * del[j-1] * del[j] / (w1*del[j] + w2*del[j-1]);
        }
    }
    }
    double hh = h[i], t = (tx - x[i]) / hh;
    double y0 = y[i], y1 = y[i+1], d0 = d[i], d1 = d[i+1];
    double c0 = y0, c1 = d0*hh, c2 = 3*(y1-y0) - (2*d0 + d1)*hh, c3 = 2*(y0-y1) + (d0 + d1)*hh;
    return ((c3*t + c2)*t + c1)*t + c0;
}

/* FFT radix-2 iterativa in-place su array complessi (re, im separati). n deve essere potenza di 2. */
void gmi_fft(double* re, double* im, int n, int inverse)
{
    /* n non potenza di 2 → Bluestein (algoritmo chirp-z), O(n log n), corretto per ogni n.
     * Radix-2 puro qui sotto scrive fuori dai limiti (es. n=6, len=4, ib=7). */
    if ((n & (n - 1)) != 0 && n > 1)
    {
        int m = 1;
        while (m < 2 * n - 1) m <<= 1;
        double* br = (double*)calloc((size_t)m, sizeof(double));
        double* bi = (double*)calloc((size_t)m, sizeof(double));
        double* cr = (double*)calloc((size_t)m, sizeof(double));
        double* ci = (double*)calloc((size_t)m, sizeof(double));
        if (!br || !bi || !cr || !ci) { free(br); free(bi); free(cr); free(ci); return; }
        /* chirp: w[j] = exp(-i*pi*j^2/n) (inverse: +) */
        double wsign = inverse ? 1.0 : -1.0;
        for (int j = 0; j < n; j++)
        {
            double ang = wsign * 3.141592653589793 * ((double)(j * j % (2 * n))) / (double)n;
            double w = cos(ang), w2 = sin(ang);
            br[j] = re[j] * w - im[j] * w2;
            bi[j] = re[j] * w2 + im[j] * w;
            cr[j] = w; ci[j] = -w2;
            if (j > 0) { cr[m - j] = w; ci[m - j] = -w2; }
        }
        gmi_fft(br, bi, m, 0);
        gmi_fft(cr, ci, m, 0);
        for (int k = 0; k < m; k++)
        {
            double tr = br[k] * cr[k] - bi[k] * ci[k];
            bi[k] = br[k] * ci[k] + bi[k] * cr[k];
            br[k] = tr;
        }
        gmi_fft(br, bi, m, 1);
        /* applica a[k]: l'IFFT (gmi_fft inverse) ha GIA' diviso per m (riga 1689) —
         * qui NON si divide di nuovo (prima /m doppio rendeva il Bluestein scalato di 1/m). */
        for (int k = 0; k < n; k++)
        {
            double ang = wsign * 3.141592653589793 * ((double)(k * k % (2 * n))) / (double)n;
            double a = cos(ang), a2 = sin(ang);
            double r = br[k], ii = bi[k];
            re[k] = r * a - ii * a2;
            im[k] = r * a2 + ii * a;
        }
        free(br); free(bi); free(cr); free(ci);
        return;
    }
    /* bit-reversal */
    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        double ang = 2.0 * 3.141592653589793 / len * (inverse ? 1 : -1);
        double wr = cos(ang), wi = sin(ang);
        for (int i = 0; i < n; i += len)
        {
            double cur_r = 1, cur_i = 0;
            for (int k = 0; k < len/2; k++)
            {
                int ia = i + k, ib = i + k + len/2;
                double tr = cur_r * re[ib] - cur_i * im[ib];
                double ti = cur_r * im[ib] + cur_i * re[ib];
                re[ib] = re[ia] - tr; im[ib] = im[ia] - ti;
                re[ia] += tr; im[ia] += ti;
                double nr = cur_r*wr - cur_i*wi;
                cur_i = cur_r*wi + cur_i*wr; cur_r = nr;
            }
        }
    }
    if (inverse) for (int i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
}

/* FFT su segnale reale → matrice n×2 (col0 re, col1 im) per `DIM x() = ANALYSIS.FFT(sig)`.
   Inverse applica 1/n (round-trip ricostruisce il segnale). */
double* gmi_yule_walker(const double* acf, int n, int p, gmb_int_t* out_len)
{
    /* Canone C# (CommonLib.Numerics.YuleWalker, oracolo IL/PCode): l'input è l'ACF;
     * risolve il sistema denso R·c = r con R[i,j] = acf[|i-j|], r[i] = acf[i+1]
     * (eliminazione di Gauss a pivot parziale, come MathNet). NON Levinson-Durbin:
     * quello normalizza acf0 e diverge quando acf[0] è piccolo/zero. */
    if (p == 0) { *out_len = 0; return NULL; }
    if (acf == NULL || n < p + 1) { *out_len = 0; return NULL; }
    double* m = (double*)calloc((size_t)(p * p), sizeof(double));
    double* rhs = (double*)calloc((size_t)p, sizeof(double));
    double* c = (double*)calloc((size_t)p, sizeof(double));
    if (!m || !rhs || !c) { free(m); free(rhs); free(c); *out_len = 0; return NULL; }
    for (int i = 0; i < p; i++) {
        rhs[i] = acf[i + 1];
        for (int j = 0; j < p; j++) {
            int d = i - j; if (d < 0) d = -d;
            m[i * p + j] = acf[d];
        }
    }
    /* forward elimination con pivot parziale */
    for (int col = 0; col < p; col++) {
        int piv = col;
        for (int r0 = col + 1; r0 < p; r0++)
            if (fabs(m[r0 * p + col]) > fabs(m[piv * p + col])) piv = r0;
        if (piv != col) {
            for (int j = col; j < p; j++) { double t = m[col * p + j]; m[col * p + j] = m[piv * p + j]; m[piv * p + j] = t; }
            double t = rhs[col]; rhs[col] = rhs[piv]; rhs[piv] = t;
        }
        double diag = m[col * p + col];
        if (diag == 0.0) { free(m); free(rhs); free(c); *out_len = 0; return NULL; }
        for (int r0 = col + 1; r0 < p; r0++) {
            double f = m[r0 * p + col] / diag;
            if (f == 0.0) continue;
            for (int j = col; j < p; j++) m[r0 * p + j] -= f * m[col * p + j];
            rhs[r0] -= f * rhs[col];
        }
    }
    /* back substitution */
    for (int i = p - 1; i >= 0; i--) {
        double s = rhs[i];
        for (int j = i + 1; j < p; j++) s -= m[i * p + j] * c[j];
        double diag = m[i * p + i];
        if (diag == 0.0) { free(m); free(rhs); free(c); *out_len = 0; return NULL; }
        c[i] = s / diag;
    }
    free(m); free(rhs);
    *out_len = p;
    return c;
}

double* gmi_ma_coefficients(const double* res, int n, int q, gmb_int_t* out_len)
{
    if (n <= 0 || q <= 0 || q >= n) { *out_len = 0; return NULL; }
    double mean = 0;
    for (int i = 0; i < n; i++) mean += res[i];
    mean /= n;
    double var = 0;
    for (int i = 0; i < n; i++) var += (res[i] - mean) * (res[i] - mean);
    var /= n;
    if (var <= 0) { *out_len = 0; return NULL; }
    double* ac = (double*)calloc((size_t)(q + 1), sizeof(double));
    double* out = (double*)calloc((size_t)q, sizeof(double));
    if (!ac || !out) { free(ac); free(out); *out_len = 0; return NULL; }
    for (int l = 0; l <= q; l++)
    {
        double s = 0;
        for (int i = 0; i < n - l; i++) s += (res[i] - mean) * (res[i + l] - mean);
        ac[l] = s / n / var;
    }
    for (int k = 0; k < q; k++) out[k] = ac[k + 1];
    free(ac);
    *out_len = q;
    return out;
}

double gmi_euclidean_distance_squared(const double* a, const double* b, int n)
{
    double s = 0;
    for (int i = 0; i < n; i++) { double d = a[i] - b[i]; s += d * d; }
    return s;
}

gmb_double_t** gmi_fft_complex_matrix(const double* re_in, const double* im_in, int n, int inverse)
{
    if (n <= 0 || !re_in) return NULL;
    gmb_double_t** out = (gmb_double_t**)calloc((size_t)n, sizeof(gmb_double_t*));
    if (!out) return NULL;
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); free(out); return NULL; }
    for (int i = 0; i < n; i++) { re[i] = re_in[i]; im[i] = im_in ? im_in[i] : 0.0; }
    gmi_fft(re, im, n, inverse);
    /* Scala MathNet simmetrica 1/sqrt(N) (canone IL/PCode; gmi_fft usa scipy:
     * forward grezza, inverse 1/n). Forward: *1/sqrt(n); inverse: gmi_fft ha già /n → *sqrt(n). */
    double s = inverse ? sqrt((double)n) : 1.0 / sqrt((double)n);
    for (int i = 0; i < n; i++) { re[i] *= s; im[i] *= s; }
    for (int i = 0; i < n; i++)
    {
        out[i] = (gmb_double_t*)calloc(2, sizeof(gmb_double_t));
        if (out[i]) { out[i][0] = re[i]; out[i][1] = im[i]; }
    }
    free(re); free(im);
    return out;
}

gmb_double_t** gmi_fft_matrix(const double* x, int n, int inverse)
{
    if (n <= 0 || !x) return NULL;
    gmb_double_t** out = (gmb_double_t**)calloc((size_t)n, sizeof(gmb_double_t*));
    if (!out) return NULL;
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); free(out); return NULL; }
    for (int i = 0; i < n; i++) re[i] = x[i];
    gmi_fft(re, im, n, inverse);
    /* Scala MathNet simmetrica 1/sqrt(N) (canone IL/PCode; gmi_fft usa scipy:
     * forward grezza, inverse 1/n). Forward: *1/sqrt(n); inverse: gmi_fft ha già /n → *sqrt(n). */
    double s = inverse ? sqrt((double)n) : 1.0 / sqrt((double)n);
    for (int i = 0; i < n; i++) { re[i] *= s; im[i] *= s; }
    for (int i = 0; i < n; i++)
    {
        out[i] = (gmb_double_t*)calloc(2, sizeof(gmb_double_t));
        if (out[i]) { out[i][0] = re[i]; out[i][1] = im[i]; }
    }
    free(re); free(im);
    return out;
}


/* Holt–Winters additive, ritorna forecast (period valori) + level attuale */
void gmi_holt_winters(const double* y, int n, double alpha, double beta, double gamma, int period,
                      double* fitted_out, int fitted_n, double* mse_out)
{
    /* Canone CommonLib.Numerics.HoltWintersFit (Advanced.cs): smoothing esponenziale
     * stagionale ADDITIVO. fitted = forecast in-sample (zero prima di `period`, dove il
     * modello non ha ancora stagionalita'), mse = errore quadratico medio su [period, n).
     * NOTA: diversa dalla vecchia versione "level/trend/season a coda" — qui seasonal[t]
     * cresce a ogni t (stagionalita' espansa, non ad anello) e il forecast in-sample usa
     * i componenti al passo t-1 con seasonal[t-period]. */
    if (mse_out) *mse_out = 0;
    if (n <= 0) return;
    if (n == 1)
    {
        if (fitted_out && fitted_n >= 1) fitted_out[0] = 0;
        return;
    }
    int sp = period;
    if (sp <= 0) sp = 1;
    double* level = (double*)malloc((size_t)n * sizeof(double));
    double* trend = (double*)malloc((size_t)n * sizeof(double));
    double* seasonal = (double*)malloc((size_t)n * sizeof(double));
    double* fcast = (double*)malloc((size_t)n * sizeof(double));
    if (!level || !trend || !seasonal || !fcast) { free(level); free(trend); free(seasonal); free(fcast); return; }
    level[0] = y[0];
    trend[0] = y[1] - y[0];
    for (int i = 0; i < sp && i < n; i++) seasonal[i] = y[i] - level[0];
    for (int i = 0; i < n; i++) fcast[i] = 0;
    for (int t = sp; t < n; t++)
    {
        double prevLevel = level[t - 1];
        double prevTrend = trend[t - 1];
        double prevSeasonal = seasonal[t - sp];
        level[t] = alpha * (y[t] - prevSeasonal) + (1 - alpha) * (prevLevel + prevTrend);
        trend[t] = beta * (level[t] - prevLevel) + (1 - beta) * prevTrend;
        seasonal[t] = gamma * (y[t] - level[t] - trend[t]) + (1 - gamma) * prevSeasonal;
        fcast[t] = prevLevel + prevTrend + prevSeasonal;
    }
    if (fitted_out && fitted_n >= n)
        for (int i = 0; i < n; i++) fitted_out[i] = fcast[i];
    if (mse_out)
    {
        double acc = 0; int cnt = 0;
        for (int t = sp; t < n; t++) { double d = y[t] - fcast[t]; acc += d * d; cnt++; }
        *mse_out = cnt > 0 ? acc / (double)cnt : 0;
    }
    free(level); free(trend); free(seasonal); free(fcast);
}

/* ADF test (simple: statistic + p-value via approx DF distribution) */
/* p-value ADF: interpolazione lineare fra i punti di ancoraggio MacKinnon (caso
 * "costante, nessun trend") — specchio di CommonLib.Numerics.AdfPValue. */
static int gmi_mat_inv(const double* m, int n, double* out);
static double gmi_adf_pvalue(double tau)
{
    static const double st[8] = { -4.5, -3.43, -2.86, -2.57, -1.6, -0.5, 0.5, 2.0 };
    static const double pv[8] = { 0.0001, 0.01, 0.05, 0.10, 0.30, 0.60, 0.90, 0.999 };
    if (tau <= st[0]) return pv[0];
    if (tau >= st[7]) return pv[7];
    for (int i = 0; i < 7; i++)
    {
        if (tau < st[i] || tau > st[i + 1]) continue;
        double frac = (tau - st[i]) / (st[i + 1] - st[i]);
        return pv[i] + frac * (pv[i + 1] - pv[i]);
    }
    return 0.5;
}

void gmi_adf_test(const double* y, int n, double* statistic, double* pvalue)
{
    /* ADF aumentato (un lag): regressione OLS Δy_t = a + g·y_{t-1} + d·Δy_{t-1} + eps,
     * per t = 2..n-1 (righe = n-2). Canone CommonLib.Numerics.AdfTest: statistic = t di g,
     * seGamma da sigma2 * inv(AtA)[1,1]. */
    if (n < 5) { *statistic = 0; *pvalue = 0.5; return; }
    int rows = n - 2;
    if (rows < 4) { *statistic = 0; *pvalue = 0.5; return; }
    double* dy = (double*)malloc((size_t)(n - 1) * sizeof(double));
    if (!dy) { *statistic = 0; *pvalue = 0.5; return; }
    for (int t = 1; t < n; t++) dy[t - 1] = y[t] - y[t - 1];
    /* AtA (3×3) e Atb (3) delle colonne A_i = [1, y_{t-1}, dy_{t-2}] */
    double ata[9] = {0,0,0,0,0,0,0,0,0};
    double atb[3] = {0,0,0};
    for (int i = 0; i < rows; i++)
    {
        int t = i + 2;
        double a0 = 1.0, a1 = y[t - 1], a2 = dy[t - 2];
        double b = dy[t - 1];
        double cols[3] = { a0, a1, a2 };
        for (int r = 0; r < 3; r++)
        {
            atb[r] += cols[r] * b;
            for (int c = 0; c < 3; c++) ata[r * 3 + c] += cols[r] * cols[c];
        }
    }
    free(dy);
    /* risolvi (AtA) x = Atb con gmi_dense_solve (matrice 3×4: AtA + rhs) */
    double* aug = (double*)malloc(12 * sizeof(double));
    if (!aug) { *statistic = 0; *pvalue = 0.5; return; }
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) aug[r * 4 + c] = ata[r * 3 + c];
    for (int r = 0; r < 3; r++) aug[r * 4 + 3] = atb[r];
    double* coeffs = gmi_dense_solve(aug, 3);
    if (!coeffs) { *statistic = 0; *pvalue = 0.5; return; }
    double gamma = coeffs[1];
    /* rss = Σ(b - A·x)², dof = rows - 3 */
    double rss = 0;
    double* dy2 = (double*)malloc((size_t)(n - 1) * sizeof(double));
    if (dy2)
    {
        for (int t = 1; t < n; t++) dy2[t - 1] = y[t] - y[t - 1];
        for (int i = 0; i < rows; i++)
        {
            int t = i + 2;
            double a0 = 1.0, a1 = y[t - 1], a2 = dy2[t - 2];
            double pred = coeffs[0] * a0 + coeffs[1] * a1 + coeffs[2] * a2;
            double e = dy2[t - 1] - pred;
            rss += e * e;
        }
        free(dy2);
    }
    int dof = rows - 3;
    double sigma2 = dof > 0 ? rss / (double)dof : 0.0;
    /* inv(AtA)[1,1] via gmi_mat_inv (layout 8-per-riga) */
    double inv[64];
    double stat = 0;
    if (gmi_mat_inv(ata, 3, inv))
    {
        double varG = sigma2 * inv[1 * 8 + 1];
        double seGamma = varG > 0 ? sqrt(varG) : 0.0;
        stat = seGamma > 0 ? gamma / seGamma : 0.0;
    }
    *statistic = stat;
    *pvalue = gmi_adf_pvalue(stat);
    free(coeffs);
}

/* K-means 1D */
void gmi_kmeans(const double* data, int n, int k, double* centroids_out)
{
    if (k <= 0) k = 1;
    if (k > n) k = n;
    for (int j = 0; j < k; j++) centroids_out[j] = data[(j * n) / k];
    double* assign = (double*)malloc(n * sizeof(double));
    if (!assign) return;
    for (int iter = 0; iter < 100; iter++)
    {
        for (int i = 0; i < n; i++)
        {
            double best = 1e300; int bi = 0;
            for (int j = 0; j < k; j++) { double d = data[i] - centroids_out[j]; d *= d; if (d < best) { best = d; bi = j; } }
            assign[i] = bi;
        }
        double* sums = (double*)calloc(k, sizeof(double));
        double* cnts = (double*)calloc(k, sizeof(double));
        if (!sums || !cnts) { free(sums); free(cnts); break; }
        for (int i = 0; i < n; i++) { sums[(int)assign[i]] += data[i]; cnts[(int)assign[i]] += 1; }
        int moved = 0;
        for (int j = 0; j < k; j++) if (cnts[j] > 0) { double nc = sums[j] / cnts[j]; if (fabs(nc - centroids_out[j]) > 1e-12) moved = 1; centroids_out[j] = nc; }
        free(sums); free(cnts);
        if (!moved) break;
    }
    free(assign);
}

/* ARIMA(p,d,q) Hannan-Rissanen — canone CommonLib.Numerics.ArimaFit (Analysis.cs):
 * (1) differenzia `d` volte; (2) AR lungo di ordine m per stimare i residui (proxy delle
 * innovazioni MA); (3) regressione OLS finale su p lag AR + q lag residui, con regolarizzazione
 * ridge λ = 1e-8·trace(AtA)/cols (stabilizza AtA quasi singolare). ar_c[0..p), ma_c[0..q),
 * fitted[0..nd) (solo [start, nd) valorizzati, resto 0). */
int gmi_arima_fit(const double* y, int n, int p, int d, int q,
                  double* ar_c, double* ma_c, double* aic, double* fitted)
{
    for (int i = 0; i < p; i++) ar_c[i] = 0;
    for (int i = 0; i < q; i++) ma_c[i] = 0;
    if (aic) *aic = 0;
    if (n <= 0) return -1;
    int nd = n;
    double* diff = (double*)malloc((size_t)n * sizeof(double));
    if (!diff) return -1;
    for (int i = 0; i < n; i++) diff[i] = y[i];
    for (int k = 0; k < d; k++)
    {
        if (nd < 2) { free(diff); return -1; }
        for (int i = 0; i < nd - 1; i++) diff[i] = diff[i + 1] - diff[i];
        nd--;
    }
    if (fitted) for (int i = 0; i < nd; i++) fitted[i] = 0;
    if (nd < p + q + 5) { free(diff); return -1; }

    /* Passo 1: AR lungo per i residui (proxy MA). */
    double* residuals = (double*)calloc((size_t)nd, sizeof(double));
    if (!residuals) { free(diff); return -1; }
    int m = p + q + 2; if (m < 8) m = 8; if (m > nd / 3) m = nd / 3;
    if (m > 0 && nd > m + 1)
    {
        int rn = nd - m;
        /* AtA (m×m) e Atb (m): Am[i, lag] = diff[t-1-lag], bm[i] = diff[t], t = i+m */
        double* ata = (double*)calloc((size_t)m * m, sizeof(double));
        double* atb = (double*)calloc((size_t)m, sizeof(double));
        if (ata && atb)
        {
            for (int i = 0; i < rn; i++)
            {
                int t = i + m;
                for (int lag = 0; lag < m; lag++)
                {
                    double col = diff[t - 1 - lag];
                    atb[lag] += col * diff[t];
                    for (int lag2 = 0; lag2 < m; lag2++) ata[lag * m + lag2] += col * diff[t - 1 - lag2];
                }
            }
            /* solve AtA x = Atb (augmented m×(m+1)) */
            double* aug = (double*)malloc((size_t)m * (m + 1) * sizeof(double));
            if (aug)
            {
                for (int r = 0; r < m; r++)
                {
                    for (int c = 0; c < m; c++) aug[r * (m + 1) + c] = ata[r * m + c];
                    aug[r * (m + 1) + m] = atb[r];
                }
                double* arLong = gmi_dense_solve(aug, m);
                if (arLong)
                {
                    for (int i = 0; i < rn; i++)
                    {
                        int t = i + m;
                        double pred = 0;
                        for (int lag = 0; lag < m; lag++) pred += arLong[lag] * diff[t - 1 - lag];
                        residuals[t] = diff[t] - pred;
                    }
                    free(arLong);
                }
            }
        }
        free(ata); free(atb);
    }

    /* Passo 2: OLS finale p lag AR + q lag residui, con ridge. */
    int start = p; if (q > start) start = q; if (m > start) start = m;
    int rows = nd - start;
    if (rows < p + q + 1) { free(residuals); free(diff); return -1; }
    int cols = p + q;
    double rss = 0;
    if (cols == 0)
    {
        for (int i = start; i < nd; i++) rss += diff[i] * diff[i];
    }
    else
    {
        /* AtA (cols×cols), AtB (cols) */
        double* ata = (double*)calloc((size_t)cols * cols, sizeof(double));
        double* atb = (double*)calloc((size_t)cols, sizeof(double));
        if (!ata || !atb) { free(ata); free(atb); free(residuals); free(diff); return -1; }
        for (int i = 0; i < rows; i++)
        {
            int t = i + start;
            double* rcol = (double*)malloc((size_t)cols * sizeof(double));
            if (!rcol) { free(ata); free(atb); free(residuals); free(diff); return -1; }
            int c = 0;
            for (int lag = 1; lag <= p; lag++) rcol[c++] = diff[t - lag];
            for (int lag = 1; lag <= q; lag++) rcol[c++] = residuals[t - lag];
            for (int r = 0; r < cols; r++)
            {
                atb[r] += rcol[r] * diff[t];
                for (int c2 = 0; c2 < cols; c2++) ata[r * cols + c2] += rcol[r] * rcol[c2];
            }
            free(rcol);
        }
        double trace = 0;
        for (int i = 0; i < cols; i++) trace += ata[i * cols + i];
        double lambda = (cols > 0) ? 1e-8 * (trace / cols) : 0;
        for (int i = 0; i < cols; i++) ata[i * cols + i] += lambda;
            double* aug = (double*)malloc((size_t)cols * (cols + 1) * sizeof(double));
            double* coeffs = NULL;
            if (aug)
            {
                for (int r = 0; r < cols; r++)
                {
                    for (int c = 0; c < cols; c++) aug[r * (cols + 1) + c] = ata[r * cols + c];
                    aug[r * (cols + 1) + cols] = atb[r];
                }
                coeffs = gmi_dense_solve(aug, cols);
            }
            if (coeffs)
            {
                for (int i = 0; i < p; i++) ar_c[i] = coeffs[i];
                for (int i = 0; i < q; i++) ma_c[i] = coeffs[p + i];
                /* rss dei residui del fit finale e fitted[start..nd) */
                for (int i = 0; i < rows; i++)
                {
                    int t = i + start;
                    double pred = 0;
                    int c = 0;
                    for (int lag = 1; lag <= p; lag++) pred += coeffs[c++] * diff[t - lag];
                    for (int lag = 1; lag <= q; lag++) pred += coeffs[c++] * residuals[t - lag];
                    double e = diff[t] - pred;
                    rss += e * e;
                    if (fitted) fitted[t] = pred;
                }
                free(coeffs);
            }
            free(ata); free(atb);
    }
    int k = p + q + 1;
    double sigma2 = rows > 0 ? rss / rows : 0.0;
    if (aic && rows > 0 && sigma2 > 0) *aic = (double)rows * log(sigma2) + 2.0 * k;
    free(residuals);
    free(diff);
    return 0;
}

/* ==================== Trading metrics & portfolio (gruppo Analisi/Portfolio) ==================== */

/* mean campionaria */
static double gmi_stats_mean(const double* x, int n) { if (n<=0) return 0; double s=0; for(int i=0;i<n;i++) s+=x[i]; return s/n; }
/* varianza campionaria (/n-1) */
/* skewness campionaria (MathNet: bias-corrected) */


/* ---- Portfolio optimization (formule chiuse/iterative) ---- */
 void gmi_inv_sym_2x2(const double* m, double* out)
{
    double det = m[0]*m[3] - m[1]*m[2];
    if (fabs(det) < 1e-15) { out[0]=out[1]=out[2]=out[3]=0; return; }
    out[0] = m[3]/det; out[1] = -m[1]/det; out[2] = -m[2]/det; out[3] = m[0]/det;
}
 void gmi_solve_2x2(const double* a, const double* b, double* x)
{
    double det = a[0]*a[3] - a[1]*a[2];
    if (fabs(det) < 1e-15) { x[0]=x[1]=0; return; }
    x[0] = (b[0]*a[3] - a[1]*b[1]) / det;
    x[1] = (a[0]*b[1] - b[0]*a[2]) / det;
}

/* Inversione di matrice generica (Gauss-Jordan), max 8×8. */
static int gmi_mat_inv(const double* m, int n, double* out)
{
    double a[64];
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) a[i*8+j] = m[i*n+j];
    memset(out, 0, 64*sizeof(double));
    for (int i = 0; i < n; i++) out[i*8+i] = 1;
    for (int col = 0; col < n; col++)
    {
        double piv = fabs(a[col*8+col]); int pr = col;
        for (int r = col+1; r < n; r++) if (fabs(a[r*8+col]) > piv) { piv = fabs(a[r*8+col]); pr = r; }
        if (piv < 1e-15) return 0;
        if (pr != col)
        {
            for (int j = 0; j < n; j++) { double t = a[col*8+j]; a[col*8+j] = a[pr*8+j]; a[pr*8+j] = t; t = out[col*8+j]; out[col*8+j] = out[pr*8+j]; out[pr*8+j] = t; }
        }
        double d = a[col*8+col];
        for (int j = 0; j < n; j++) { a[col*8+j] /= d; out[col*8+j] /= d; }
        for (int r = 0; r < n; r++)
        {
            if (r == col) continue;
            double f = a[r*8+col];
            if (f == 0) continue;
            for (int j = 0; j < n; j++) { a[r*8+j] -= f*a[col*8+j]; out[r*8+j] -= f*out[col*8+j]; }
        }
    }
    return 1;
}

/* MINVOLATILITY: w = (Σ⁻¹1)/(1ᵀΣ⁻¹1) — minima varianza globale (formula chiusa). */

/* MAXSHARPE: w ∝ Σ⁻¹(μ − rf·1), normalizzato a somma 1. */

/* RISKPARITY (ERC): iterazione di Spinu. */


/* BLACKLITTERMAN semplificato: posteriori via vista di mercato. */

/* Cholesky solve Ax=b (A SPD), ritorna soluzione o NULL. */
int gmi_cholesky_solve(const double* a, const double* b, int n, double* x)
{
    double L[64][64];
    memset(L, 0, sizeof(L));
    for (int i = 0; i < n; i++)
        for (int j = 0; j <= i; j++)
        {
            double s = a[i*n+j];
            for (int k = 0; k < j; k++) s -= L[i][k]*L[j][k];
            if (i == j)
            {
                if (s <= 0) return 0;
                L[i][j] = sqrt(s);
            }
            else L[i][j] = s / L[j][j];
        }
    double y[64];
    for (int i = 0; i < n; i++)
    {
        double s = b[i];
        for (int k = 0; k < i; k++) s -= L[i][k]*y[k];
        y[i] = s / L[i][i];
    }
    for (int i = n-1; i >= 0; i--)
    {
        double s = y[i];
        for (int k = i+1; k < n; k++) s -= L[k][i]*x[k];
        x[i] = s / L[i][i];
    }
    return 1;
}

/* PCA: matrice m×n → eigenvals[n], eigenvecs[n][n] (power iteration con deflazione). */
void gmi_analysis_pca(const double* data, int m, int n, double* eigenvals, double* eigenvecs)
{
    double means[8] = {0}, centered[64] = {0}, cov[64] = {0};
    for (int j = 0; j < n && j < 8; j++) { for (int i = 0; i < m; i++) means[j] += data[i*n+j]; means[j] /= m; }
    for (int i = 0; i < m; i++) for (int j = 0; j < n && j < 8; j++) centered[i*8+j] = data[i*n+j] - means[j];
    for (int i = 0; i < n && i < 8; i++)
        for (int j = 0; j < n && j < 8; j++)
        {
            double s = 0;
            for (int k = 0; k < m; k++) s += centered[k*8+i] * centered[k*8+j];
            cov[i*8+j] = s / (m > 1 ? m - 1 : 1);
        }
    for (int p = 0; p < n && p < 8; p++)
    {
        double v[8] = {0}; v[p % n] = 1;
        for (int iter = 0; iter < 100; iter++)
        {
            double av[8] = {0};
            for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) av[i] += cov[i*8+j]*v[j];
            double norm = 0; for (int i = 0; i < n; i++) norm += av[i]*av[i];
            norm = sqrt(norm);
            if (norm < 1e-10) break;
            for (int i = 0; i < n; i++) av[i] /= norm;
            double d = 0; for (int i = 0; i < n; i++) { double dd = fabs(av[i]-v[i]); if (dd > d) d = dd; }
            for (int i = 0; i < n; i++) v[i] = av[i];
            if (d < 1e-8) break;
        }
        double ev = 0;
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) ev += v[i]*cov[i*8+j]*v[j];
        eigenvals[p] = ev;
        for (int i = 0; i < n; i++) eigenvecs[i*8+p] = v[i];
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) cov[i*8+j] -= ev * v[i] * v[j];
    }
}

/* ANOVA one-way: gruppi (matrice righe=gruppi), ritorna f,p,ssb,ssw. */

/* ANALYSIS.PCA → Analysis.PcaResult (dict {EigenValues, EigenVectors}) da matrice
 * RETTANGOLARE flat (k osservazioni × n variabili, row-major). Canone IlRuntime.Pca
 * (power iteration, stessa gmi_analysis_pca). */

/* PCA da gmi_val_t lista di liste (righe = osservazioni) — percorso generico. */

/* PCA da matrice JAGGED (righe = osservazioni, tutte di pari lunghezza n). */

/* JARQUE-BERA test: JB = n/6 * (S² + (K-3)²/4) con S=skewness, K=kurtosis campionarie. */
double gmi_stats_jarque_bera(const double* data, int n)
{
    if (n < 4) return 0;
    double m = gmi_stats_mean(data, n);
    double s2 = 0, s3 = 0, s4 = 0;
    for (int i = 0; i < n; i++) { double d = data[i]-m; s2 += d*d; s3 += d*d*d; s4 += d*d*d*d; }
    double var = s2 / n;
    if (var == 0) return 0;
    double skew = (s3 / n) / pow(var, 1.5);
    double kurt = (s4 / n) / (var * var);
    return n / 6.0 * (skew * skew + (kurt - 3) * (kurt - 3) / 4.0);
}

/* Metriche trading aggiuntive (gruppo copertura completa) */
double gmi_tm_min_cvar(const double* r, int n, double conf);
double gmi_tm_min_cdar(const double* r, int n, double conf);
/* PortfolioResult come dict (contratto TP.MAXRETURN/KELLYCRITERION, come la VM/C case 193/189):
   ExpectedReturn = metrica calcolata, Volatility=0, SharpeRatio=0, Weights = lista di n pesi
   (1.0 sull'asset migliore per MAXRETURN, altrimenti 0; k per KELLY). */
/* BACKTEST RUN(returns, strat) → dict {TotalReturn, AnnualizedReturn, MaxDrawdown,
   SharpeRatio, TradesCount, Turnover} (come la VM/C case 624). returns = lista di periodi
   (ogni periodo una lista di rendimenti asset) oppure lista piatta. */
/* TRADING.BACKTESTS.RUN su array 2-D rettangolare (flat rows×cols): rendimento per periodo =
   media degli asset della riga, poi gmi_tm_backtest. Parità IlRuntime.BacktestRun /
   PCodeProcessor case RUN / VM/C gmi_bf_624. */
/* PORTFOLIO OPTIMIZATION → dict PortfolioResult (OPT.MINVOLATILITY/MAXSHARPE/MEANVARIANCE/
   RISKPARITY/EQUALLYWEIGHTED, come VM case 183/184/190/192). cov è JAGGED (gmb_double_t*[n],
   righe irregolari); la funzione la compatta in contiguo n×n e chiama gmi_port_*.
   kind: 0=min_vol, 1=max_sharpe, 2=risk_parity, 3=equally_weighted, 4=mean_variance. */
/* TRADING.PORTFOLIO.EVOLVE(rets, cov, parametriGA) → dict PortfolioResult. L'IL minimizza la
   varianza del portafoglio con un GA (DifferentialEvolution); qui si usa gmi_port_min_vol
   (stesso obiettivo, deterministico) → weights/return/vol/sharpe. cov è JAGGED (gmb_double_t*[n]). */
/* TRADING.PORTFOLIO.BLACKLITTERMAN(marketCaps, cov, riskAversion, tau, P, Q, omega, riskFree)
   → PortfolioResult. Formula completa (parità CommonLib.Trading.Portfolio.BlackLitterman):
   Π = λ Σ w_mkt; posterior = [(τΣ)^-1 + Pᵀ Ω^-1 P]^-1 · [(τΣ)^-1 Π + Pᵀ Ω^-1 Q];
   poi MAXSHARPE(posterior, Σ, rf). cov/P/omega sono JAGGED (gmb_double_t**). */
/* B3 (2026-08-25): rendimenti/volatilità/azioni/obbligazioni mancanti — formule
   allineate a CommonLib.Trading.Metrics/General/Bonds (parità IL/PCodeVM). */
/* MINCVAR: media dei perdite oltre il quantile (CVaR). */
/* MINCDAR: CVaR sul drawdown cumulato. */

/* scipy.fft: rfft (componenti non negative) / irfft (ricostruzione Hermitiana). */
double* gmi_rfft(const double* x, int n, gmb_int_t* outn)
{
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); *outn = 0; return NULL; }
    for (int i = 0; i < n; i++) re[i] = x[i];
    gmi_fft(re, im, n, 0);
    int m = n / 2 + 1;
    double* out = (double*)malloc(sizeof(double) * (size_t)m * 2);
    if (!out) { free(re); free(im); *outn = 0; return NULL; }
    for (int k = 0; k < m; k++) { out[k * 2] = re[k]; out[k * 2 + 1] = im[k]; }
    *outn = m;
    free(re); free(im);
    return out;
}

double* gmi_irfft(const double* re, const double* im, int m, gmb_int_t* outn)
{
    int n = 2 * (m - 1);
    double* cre = (double*)calloc((size_t)n, sizeof(double));
    double* cim = (double*)calloc((size_t)n, sizeof(double));
    if (!cre || !cim) { free(cre); free(cim); *outn = 0; return NULL; }
    for (int k = 0; k < m; k++) { cre[k] = re[k]; cim[k] = im[k]; }
    for (int k = 1; k < m - 1; k++) { cre[n - k] = re[k]; cim[n - k] = -im[k]; }
    gmi_fft(cre, cim, n, 1);
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { free(cre); free(cim); *outn = 0; return NULL; }
    for (int i = 0; i < n; i++) out[i] = cre[i];
    *outn = n;
    free(cre); free(cim);
    return out;
}








/* TRADING.PORTFOLIO.MAD: scarto medio assoluto dalla MEDIANA (parità IL PortfolioMad,
 * VM/C gmi_bf_196: med = data[n/2] su array ordinato). */

/* TRADING.PORTFOLIO.SEMIMAD: solo i valori < target, scarto medio assoluto dalla loro
 * MEDIANA (parità IL PortfolioSemiMad, VM/C gmi_bf_197). target di default 0. */






static void gmi_pca_jacobi(double* a, int n, double* e, double* v)
{
    /* Jacobi: a è la matrice simmetrica (n×n), v = autovettori per colonna, e = autovalori. */
    for (int i = 0; i < n * n; i++) v[i] = 0.0;
    for (int i = 0; i < n; i++) v[i * n + i] = 1.0;
    for (int sweep = 0; sweep < 50; sweep++)
    {
        double off = 0;
        for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) off += a[p * n + q] * a[p * n + q];
        if (off < 1e-24) break;
        for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++)
        {
            if (fabs(a[p * n + q]) < 1e-16) continue;
            double app = a[p * n + p], aqq = a[q * n + q], apq = a[p * n + q];
            double theta = (aqq - app) / (2.0 * apq);
            double t = (theta >= 0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta * theta + 1.0));
            double c = 1.0 / sqrt(t * t + 1.0), s = t * c;
            for (int k = 0; k < n; k++)
            {
                double akp = a[k * n + p], akq = a[k * n + q];
                a[k * n + p] = c * akp - s * akq;
                a[k * n + q] = s * akp + c * akq;
            }
            for (int k = 0; k < n; k++)
            {
                double apk = a[p * n + k], aqk = a[q * n + k];
                a[p * n + k] = c * apk - s * aqk;
                a[q * n + k] = s * apk + c * aqk;
            }
            for (int k = 0; k < n; k++)
            {
                double vkp = v[k * n + p], vkq = v[k * n + q];
                v[k * n + p] = c * vkp - s * vkq;
                v[k * n + q] = s * vkp + c * vkq;
            }
        }
    }
    for (int i = 0; i < n; i++) e[i] = a[i * n + i];
}

gmb_double_t** gmi_pca(const double* data, int rows, int cols, gmb_int_t* out_rows, gmb_int_t* out_cols, double* eigs)
{
    if (!data || rows <= 0 || cols <= 0) { *out_rows = 0; *out_cols = 0; return NULL; }
    double* mean = (double*)calloc((size_t)cols, sizeof(double));
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) mean[c] += data[r * cols + c];
    for (int c = 0; c < cols; c++) mean[c] /= rows;
    double* cov = (double*)calloc((size_t)(cols * cols), sizeof(double));
    for (int i = 0; i < cols; i++) for (int j = 0; j < cols; j++)
    {
        double s = 0;
        for (int r = 0; r < rows; r++) s += (data[r * cols + i] - mean[i]) * (data[r * cols + j] - mean[j]);
        cov[i * cols + j] = s / (rows > 1 ? rows - 1 : 1);
    }
    double* v = (double*)calloc((size_t)(cols * cols), sizeof(double));
    double* e = (double*)calloc((size_t)cols, sizeof(double));
    gmi_pca_jacobi(cov, cols, e, v);
    gmb_double_t** out = (gmb_double_t**)calloc((size_t)cols, sizeof(gmb_double_t*));
    for (int i = 0; i < cols; i++)
    {
        out[i] = (gmb_double_t*)calloc((size_t)cols, sizeof(gmb_double_t));
        for (int j = 0; j < cols; j++) out[i][j] = v[j * cols + i];
    }
    for (int i = 0; i < cols; i++) eigs[i] = e[i];
    *out_rows = cols; *out_cols = cols;
    free(mean); free(cov); free(v); free(e);
    return out;
}





int gmi_cholesky_solve_jagged(const double* const* a, const double* b, int n, double* x)
{
    if (!a || !b || !x || n <= 0) return -1;
    double* flat = (double*)calloc((size_t)(n * n), sizeof(double));
    if (!flat) return -1;
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) flat[i * n + j] = a[i][j];
    int rc = gmi_cholesky_solve(flat, b, n, x);
    free(flat);
    return rc;
}
