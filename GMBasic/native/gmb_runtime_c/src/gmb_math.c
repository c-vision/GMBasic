/* ============================================================================
 * gmb_math.c — builtin matematiche/stringhe (ROUND/RND/CHR).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include "gmb_finance.h"
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
static const unsigned long long gmb_pow10_ull[19] = {
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
gmb_double_t gmb_round(gmb_double_t value, gmb_int_t digits) {
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
    unsigned long long T = gmb_pow10_ull[h];
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
void gmb_rnd_seed(uint64_t seed) { g_rnd_state = seed ? seed : 0x2545F4914F6CDD1DULL; }
gmb_double_t gmb_rnd(void) {
    g_rnd_state ^= g_rnd_state << 13;
    g_rnd_state ^= g_rnd_state >> 7;
    g_rnd_state ^= g_rnd_state << 17;
    return (gmb_double_t)(g_rnd_state >> 11) / 9007199254740992.0;
}

/* PI: costante π (builtin PI()/PI). */
gmb_double_t gmb_pi(void) { return 3.14159265358979323846; }

/* MIN/MAX scalari (builtin MIN(a,b,...) / MAX(a,b,...)). */
gmb_double_t gmb_min(gmb_double_t a, gmb_double_t b) { return a < b ? a : b; }
gmb_double_t gmb_max(gmb_double_t a, gmb_double_t b) { return a > b ? a : b; }



/* CHR(code): carattere (code point UTF-8, come char.ConvertFromUtf32). */
gmb_string_t gmb_chr(gmb_int_t code) {
    char buf[8];
    unsigned int cp = (unsigned int)code;
    if (cp < 0x80) {
        buf[0] = (char)cp; buf[1] = 0;
    } else if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        buf[2] = 0;
    } else if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        buf[3] = 0;
    } else {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        buf[4] = 0;
    }
    return gmb_string_alloc(buf);
}

/* TIMER(): millisecondi dalla mezzanotte (TimeService.Timer, wrap 24h). */
gmb_int_t gmb_timer(void) {
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

gmb_double_t gmb_int(gmb_double_t v) { return floor(v); }

/* CHOOSE(index, values...) — indice 1-based nella lista valori (ParamArray). Ritorna il valore
 * scelto (gmb_val_t unificato) o gmb_val_nil() se l'indice è fuori range. */
gmb_val_t gmb_choose(gmb_int_t idx, const gmb_val_t* values, gmb_int_t n)
{
    gmb_int_t i = idx - 1;
    if (values && i >= 0 && i < n)
        return values[i];
    return gmb_val_nil();
}
gmb_double_t gmb_floor(gmb_double_t v) { return floor(v); }
gmb_double_t gmb_ceiling(gmb_double_t v) { return ceil(v); }
gmb_double_t gmb_abs(gmb_double_t v) { return fabs(v); }
gmb_double_t gmb_clamp(gmb_double_t v, gmb_double_t lo, gmb_double_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
gmb_double_t gmb_max_arr(const gmb_double_t* a, int n) {
    if (n <= 0 || !a) return 0;
    double m = a[0];
    for (int i = 1; i < n; i++) if (a[i] > m) m = a[i];
    return m;
}
gmb_double_t gmb_min_arr(const gmb_double_t* a, int n) {
    if (n <= 0 || !a) return 0;
    double m = a[0];
    for (int i = 1; i < n; i++) if (a[i] < m) m = a[i];
    return m;
}
gmb_double_t gmb_sum_arr(const gmb_double_t* a, int n) {
    if (n <= 0 || !a) return 0;
    double s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}
gmb_double_t gmb_dist2d(gmb_double_t x1, gmb_double_t y1, gmb_double_t x2, gmb_double_t y2) {
    return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
}
gmb_double_t gmb_dist3d(gmb_double_t x1, gmb_double_t y1, gmb_double_t z1, gmb_double_t x2, gmb_double_t y2, gmb_double_t z2) {
    return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) + (z1-z2)*(z1-z2));
}
gmb_double_t gmb_percentile(const gmb_double_t* a, int n, gmb_double_t p) {
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
    /* Canone IL: IlRuntime.Percentile(data, p/100) → CommonLib round(fraction*100) = round(p)
     * → MathNet Statistics.Percentile(data, k) col metodo NEAREST-RANK:
     * idx = ceil(k/100 * N) - 1 (clamp [0,N-1]). NON interpolazione lineare: per
     * STATS.PERCENTILE(dati, 0.90) l'IL dà dati[0] (k=1), la vecchia lineare dava 1.081. */
    int k = (int)floor(p + 0.5);          /* round(p) */
    if (k < 0) k = 0; else if (k > 100) k = 100;
    int idx = (int)ceil(k / 100.0 * n) - 1;
    if (idx < 0) idx = 0; else if (idx >= n) idx = n - 1;
    double res = tmp[idx];
    free(tmp);
    return res;
}
gmb_string_t gmb_num_to_hex(gmb_double_t v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llX", (unsigned long long)(long long)v);
    return gmb_string_alloc(buf);
}
gmb_string_t gmb_num_to_oct(gmb_double_t v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llo", (unsigned long long)(long long)v);
    return gmb_string_alloc(buf);
}
gmb_int_t gmb_is_numeric(const char* s) {
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
static char* gmb_fmt_g_net(double v) {
    char buf[64];
    if (v == 0.0) v = 0.0;   /* -0.0 -> "0" (parita con .NET/sample) */
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
gmb_string_t gmb_val_str(gmb_double_t v) {
    char* s = gmb_fmt_g_net(v);
    gmb_string_t r = gmb_string_alloc(s ? s : "");
    free(s);
    return r;
}

/* VAL(s): estrae il prefisso numerico iniziale e ignora il resto (ConversionUtils.Val). */
gmb_double_t gmb_parse_num(const char* s, int* ok) {
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

gmb_double_t gmb_fix(gmb_double_t v) { return trunc(v); }
gmb_double_t gmb_trunc(gmb_double_t v) { return trunc(v); }
gmb_double_t gmb_sqr(gmb_double_t v) { return sqrt(v); }
gmb_double_t gmb_sgn(gmb_double_t v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
gmb_double_t gmb_cbyte(gmb_double_t v) { long t = (long)v; if (t < 0) t = 0; if (t > 255) t = 255; return (gmb_double_t)(unsigned char)t; }
gmb_double_t gmb_hypot(gmb_double_t a, gmb_double_t b) { return hypot(a, b); }
gmb_double_t gmb_yearfrac(gmb_double_t d1, gmb_double_t d2) {
    /* C# YEARFRAC: (d2-d1).TotalDays / 365.0 — d1/d2 sono OADate in giorni. */
    return fabs(d2 - d1) / 365.0;
}

gmb_double_t gmb_fact(gmb_double_t n) {
    if (n < 0) return 0;
    long long nn = (long long)n;
    double r = 1;
    for (long long i = 2; i <= nn; i++) r *= (double)i;
    return r;
}
gmb_double_t gmb_perm(gmb_double_t n, gmb_double_t k) {
    long long nn = (long long)n, kk = (long long)k;
    if (kk < 0 || kk > nn) return 0;
    double r = 1;
    for (long long i = 0; i < kk; i++) r *= (double)(nn - i);
    return r;
}
gmb_double_t gmb_comb(gmb_double_t n, gmb_double_t k) {
    long long nn = (long long)n, kk = (long long)k;
    if (kk < 0 || kk > nn) return 0;
    if (kk > nn - kk) kk = nn - kk;
    double r = 1;
    for (long long i = 1; i <= kk; i++) r = r * (double)(nn - kk + i) / (double)i;
    return r;
}

gmb_double_t gmb_polarx(gmb_double_t r, gmb_double_t angle_deg) {
    return r * cos(angle_deg * 3.141592653589793 / 180.0);
}
gmb_double_t gmb_polary(gmb_double_t r, gmb_double_t angle_deg) {
    return r * sin(angle_deg * 3.141592653589793 / 180.0);
}
gmb_double_t gmb_angle2d(gmb_double_t x1, gmb_double_t y1, gmb_double_t x2, gmb_double_t y2) {
    return atan2(y2 - y1, x2 - x1) * 180.0 / 3.141592653589793;
}
gmb_double_t gmb_pythagoras(gmb_double_t a, gmb_double_t b) {
    return sqrt(a * a + b * b);
}

static double gmb_next_double(void) {
    return gmb_rnd();
}
/* Sorgente casuale SEPARATA per RANDOMINT/RANDOMDOUBLE/RANDOMNORMAL: il canone C#
 * (IlRuntime → CryptoUtils.CoreRandom*) usa un Random .NET distinto che RANDOMIZE
 * NON semina (RANDOMIZE semina solo g_rnd_state di RND). gmb_rnd/g_rnd_state qui
 * condividerlo farebbe sì che RANDOMIZE influenzi queste funzioni (divergenza
 * silenziosa da IL/PCode). Stato lazy-init non deterministico (tempo + stack addr). */
static unsigned long long g_crnd_state = 0;
static double gmb_next_crnd(void) {
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
double gmb_rand_int(double min, double max) {
    int lo = (int)min, hi = (int)max;
    if (hi <= lo) return lo;
    return (double)(lo + (int)(gmb_next_crnd() * (hi - lo)));
}
double gmb_rand_double(double min, double max) {
    return gmb_next_crnd() * (max - min) + min;
}
double gmb_rand_normal(double mean, double stddev) {
    double u1 = 1.0 - gmb_next_crnd();
    double u2 = 1.0 - gmb_next_crnd();
    double z = sqrt(-2.0 * log(u1)) * sin(2.0 * 3.141592653589793 * u2);
    return mean + stddev * z;
}
uint8_t* gmb_rand_bytes(int count) {
    if (count < 0) count = 0;
    uint8_t* out = (uint8_t*)malloc((size_t)(count ? count : 1));
    if (!out) return NULL;
    for (int i = 0; i < count; i++) out[i] = (uint8_t)(gmb_next_double() * 256.0);
    return out;
}
/* RANDOMBYTES(n) → byte[] come gmb_string_t binaria (per C99Native byte-expr/DIM). */
gmb_string_t gmb_rand_bytes_val(int count) {
    gmb_string_t r;
    if (count < 0) count = 0;
    r.length = (uint32_t)count;
    r.data = (char*)malloc((size_t)(count ? count : 1));
    if (!r.data) { r.data = (char*)calloc(1, 1); r.length = 0; r.ref_count = 1; return r; }
    for (int i = 0; i < count; i++) r.data[i] = (char)(gmb_next_double() * 256.0);
    r.ref_count = 1;
    return r;
}
/* RANDOMCRYPTOBYTES(n) → byte[] da sorgente CRITTOGRAFICA (/dev/urandom; fallback PRNG). */
gmb_string_t gmb_rand_crypto_bytes_val(int count) {
    gmb_string_t r;
    if (count < 0) count = 0;
    r.length = (uint32_t)count;
    r.ref_count = 1;
    r.data = (char*)malloc((size_t)(count ? count : 1));
    if (!r.data) { r.data = (char*)calloc(1, 1); r.length = 0; return r; }
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t got = fread(r.data, 1, (size_t)count, f);
        fclose(f);
        if (got == (size_t)count) return r;
    }
    for (int i = 0; i < count; i++) r.data[i] = (char)(gmb_next_double() * 256.0);
    return r;
}

double gmb_range_arr(const double* a, int n) {
    if (!a || n <= 0) return 0;
    double lo = a[0], hi = a[0];
    for (int i = 1; i < n; i++) { if (a[i] < lo) lo = a[i]; if (a[i] > hi) hi = a[i]; }
    return hi - lo;
}

double gmb_covariance(const double* x, const double* y, int n) {
    if (n < 2) return 0;
    double mx = 0, my = 0;
    for (int i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;
    double sxy = 0;
    for (int i = 0; i < n; i++) sxy += (x[i] - mx) * (y[i] - my);
    return sxy / (double)(n - 1);
}

double gmb_correlation(const double* x, const double* y, int n) {
    if (n < 2) return 0;
    double mx = 0, my = 0;
    for (int i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;
    double sxy = 0, sx2 = 0, sy2 = 0;
    for (int i = 0; i < n; i++) {
        double dx = x[i] - mx, dy = y[i] - my;
        sxy += dx * dy; sx2 += dx * dx; sy2 += dy * dy;
    }
    return (sx2 > 0 && sy2 > 0) ? sxy / sqrt(sx2 * sy2) : 0.0;
}

static double gmb_median_sorted(double* s, int n) {
    if (n <= 0) return 0;
    if (n % 2 == 0) return (s[n/2-1] + s[n/2]) / 2.0;
    return s[n/2];
}
static void gmb_sort_arr(double* s, int n) {
    for (int i = 1; i < n; i++) { double k = s[i]; int j = i-1; while (j >= 0 && s[j] > k) { s[j+1]=s[j]; j--; } s[j+1]=k; }
}
double gmb_mean(const double* a, int n) {
    if (n <= 0) return 0;
    double s = 0; for (int i = 0; i < n; i++) s += a[i];
    return s / n;
}
double gmb_median(const double* a, int n) {
    if (n <= 0) return 0;
    double* s = (double*)malloc(sizeof(double) * n);
    if (!s) return 0;
    memcpy(s, a, sizeof(double) * n);
    gmb_sort_arr(s, n);
    double r = gmb_median_sorted(s, n);
    free(s);
    return r;
}
double gmb_variance(const double* a, int n) {
    if (n < 2) return 0;
    double m = gmb_mean(a, n), s = 0;
    for (int i = 0; i < n; i++) { double d = a[i] - m; s += d*d; }
    return s / (n - 1);
}
double gmb_stddev(const double* a, int n) {
    if (n < 2) return 0;
    return sqrt(gmb_variance(a, n));
}
double gmb_skewness(const double* a, int n) {
    if (n < 3) return 0;
    double avg = gmb_mean(a, n);
    double m2 = 0, m3 = 0;
    for (int i = 0; i < n; i++) { double d = a[i]-avg; m2 += d*d; m3 += d*d*d; }
    m2 /= n; m3 /= n;
    double s3 = pow(m2, 1.5);
    if (s3 == 0) return 0;
    return (m3 / s3) * sqrt((double)n * (n - 1)) / (n - 2); /* G1 aggiustata (MathNet/C#) */
}
double gmb_kurtosis(const double* a, int n) {
    if (n < 4) return 0;
    double avg = gmb_mean(a, n);
    double m2 = 0, m4 = 0;
    for (int i = 0; i < n; i++) { double d = a[i]-avg; m2 += d*d; m4 += d*d*d*d; }
    m2 /= n; m4 /= n;
    if (m2 == 0) return 0;
    double g2 = m4 / (m2 * m2);
    return ((double)n - 1) / (((double)n - 2) * ((double)n - 3)) * (((double)n + 1) * g2 - 3.0 * ((double)n - 1)); /* eccesso non distorto (MathNet/C#) */
}
double gmb_mad(const double* a, int n) {
    if (n <= 0) return 0;
    double* s = (double*)malloc(sizeof(double) * n);
    if (!s) return 0;
    memcpy(s, a, sizeof(double) * n);
    gmb_sort_arr(s, n);
    double median = gmb_median_sorted(s, n);
    double* devs = (double*)malloc(sizeof(double) * n);
    if (!devs) { free(s); return 0; }
    for (int i = 0; i < n; i++) devs[i] = fabs(a[i] - median);
    gmb_sort_arr(devs, n);
    double r = gmb_median_sorted(devs, n);
    free(s); free(devs);
    return r;
}/* Nelder-Mead (simplex): minimizza fn(v[0..n-1]) partendo da start. Ritorna il punto minimo. */
/* ── F1: nucleo MathNet.Optimization (port fedele per parità col canone IL) ──
 * NumericalGradientObjective (forward difference H=1e-8), WeakWolfeLineSearch,
 * MinimizerBase.ExitCriteriaSatisfied. Riusato da BFGS/LBFGS/Powell/CG. */
typedef struct {
    double (*f)(const double*, int, void*);
    void* ctx;
    int n;
    double* point;
    double value;
    double* grad;
} mn_obj_t;
/* ==================== Distribuzioni statistiche (formule standard) ==================== */

/* ── MathNet.Numerics 5.0.0 GammaLn/Gamma (Lanczos, Pugh 2004) — port fedele per
 * parità bit-esatta col canone C# (SpecialFunctions.GammaLn/Gamma). Sostituisce
 * lgamma()/tgamma() di libm, che differivano di ~1e-13. ── */
#define GMB_MT_GAMMA_N 10
#define GMB_MT_GAMMA_R 10.900511
static const double g_mt_gammadk[11] = {
    2.48574089138753565546e-5, 1.05142378581721974210, -3.45687097222016235469,
    4.51227709466894823700, -2.98285225323576655721, 1.05639711577126713077,
    -1.95428773191645869583e-1, 1.70970543404441224307e-2, -5.71926117404305781283e-4,
    4.63399473359905636708e-6, -2.71994908488607703910e-9
};
#define GMB_MT_LNPI 1.1447298858494001741434273513530587116472948129153
#define GMB_MT_LOGTWOSQRTEOPI 0.6207822376352452223455184457816472122518527279025978
#define GMB_MT_TWOSQRTEOPI 1.8603827342052657173362492472666631120594218414085755
#define GMB_MT_E 2.7182818284590452353602874713526624977572470937000

static double gmb_mathnet_gammaln(double z)
{
    if (z < 0.5) {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (i - z);
        return GMB_MT_LNPI - log(sin(M_PI * z)) - log(s)
             - GMB_MT_LOGTWOSQRTEOPI - ((0.5 - z) * log((0.5 - z + GMB_MT_GAMMA_R) / GMB_MT_E));
    } else {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (z + i - 1.0);
        return log(s) + GMB_MT_LOGTWOSQRTEOPI + ((z - 0.5) * log((z - 0.5 + GMB_MT_GAMMA_R) / GMB_MT_E));
    }
}

static double gmb_mathnet_gamma(double z)
{
    if (z < 0.5) {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (i - z);
        return M_PI / (sin(M_PI * z) * s * GMB_MT_TWOSQRTEOPI
                       * pow((0.5 - z + GMB_MT_GAMMA_R) / GMB_MT_E, 0.5 - z));
    } else {
        double s = g_mt_gammadk[0];
        for (int i = 1; i <= GMB_MT_GAMMA_N; i++) s += g_mt_gammadk[i] / (z + i - 1.0);
        return s * GMB_MT_TWOSQRTEOPI * pow((z - 0.5 + GMB_MT_GAMMA_R) / GMB_MT_E, z - 0.5);
    }
}

/* Parità con CommonLib.Numerics.SpecialGamma/SpecialLGamma (wrapper NaN/Inf). */
double gmb_lgamma(double x) { return (isnan(x) || x <= 0) ? NAN : gmb_mathnet_gammaln(x); }
double gmb_beta_fn(double a, double b)
{
    if (isnan(a) || isnan(b) || a <= 0 || b <= 0) return NAN;
    return exp(gmb_mathnet_gammaln(a) + gmb_mathnet_gammaln(b) - gmb_mathnet_gammaln(a + b));
}

/* Regolarized incomplete beta I_x(a,b). */
double gmb_incbeta(double a, double b, double x)
{
    /* I_x(a,b) — port fedele di MathNet BetaRegularized (continued fraction,
     * eps=2.22e-16, fpmin=DBL_MIN). Sostituisce gmb_betacf (eps 3e-14). */
    if (a < 0 || b < 0) return NAN;
    if (x < 0 || x > 1) return NAN;
    double bt = (x == 0.0 || x == 1.0) ? 0.0
        : exp(gmb_mathnet_gammaln(a + b) - gmb_mathnet_gammaln(a) - gmb_mathnet_gammaln(b)
              + (a * log(x)) + (b * log(1.0 - x)));
    int sym = x >= (a + 1.0) / (a + b + 2.0);
    const double eps = 2.220446049250313e-16;
    const double fpmin = 2.2250738585072014e-308;
    if (sym) { x = 1.0 - x; double t = a; a = b; b = t; }
    double qab = a + b, qap = a + 1.0, qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - (qab * x / qap);
    if (fabs(d) < fpmin) d = fpmin;
    d = 1.0 / d;
    double h = d;
    for (int m = 1, m2 = 2; m <= 50000; m++, m2 += 2) {
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + (aa * d);
        if (fabs(d) < fpmin) d = fpmin;
        c = 1.0 + (aa / c);
        if (fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + (aa * d);
        if (fabs(d) < fpmin) d = fpmin;
        c = 1.0 + (aa / c);
        if (fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        double del = d * c;
        h *= del;
        if (fabs(del - 1.0) <= eps) break;
    }
    return sym ? 1.0 - (bt * h / a) : bt * h / a;
}

double gmb_erf_c(double x) { return erfc(x); }
static double gmb_normal_cdf(double z) { return 0.5 * erfc(-z / sqrt(2.0)); }

double gmb_gamma_p(double a, double x)
{
    /* regularized lower incomplete gamma P(a,x) via series/continued fraction */
    if (x < 0 || a <= 0) return 0;
    if (x == 0) return 0;
    double lg = gmb_mathnet_gammaln(a);  /* log Γ(a) — canone MathNet (non lgamma C: 1 ULP) */
    if (x < a + 1)
    {
        double ap = a, sum = 1.0 / a, del = sum;
        for (int n = 1; n <= 300; n++) { ap += 1; del *= x / ap; sum += del; if (fabs(del) < fabs(sum) * 1e-14) break; }
        return sum * exp(-x + a * log(x) - lg);
    }
    else
    {
        double b = x + 1 - a, c = 1e30, d = 1.0 / b, h = d;
        for (int i = 1; i <= 300; i++)
        {
            double an = -i * (i - a);
            b += 2;
            d = an * d + b; if (fabs(d) < 1e-30) d = 1e-30;
            c = b + an / c; if (fabs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            double del = d * c;
            h *= del;
            if (fabs(del - 1) < 1e-14) break;
        }
        return 1 - exp(-x + a * log(x) - lg) * h;
    }
}

/* ── Nucleo scipy.special (SPECIAL namespace) — condiviso VM/C e C99 ── */

/* gamma completa Γ(x) (x>0); tgamma è la libreria C (precisione macchina). */
double gmb_gamma(double x)
{
    if (isnan(x)) return NAN;
    if (isinf(x)) return x > 0 ? INFINITY : NAN;
    return gmb_mathnet_gamma(x);
}

/* erf(x) = 1 − erfc(x) */
double gmb_erf(double x) { return 1.0 - erfc(x); }

/* gammaincc(a,x) = Q(a,x) = 1 − P(a,x) */
double gmb_gammaincc(double a, double x)
{
    if (a <= 0 || x < 0) return NAN;
    return 1.0 - gmb_gamma_p(a, x);
}

/* betaln(a,b) = log B(a,b) */
double gmb_betaln(double a, double b)
{
    if (a <= 0 || b <= 0) return NAN;
    return gmb_mathnet_gammaln(a) + gmb_mathnet_gammaln(b) - gmb_mathnet_gammaln(a + b);
}

/* digamma ψ(x) — port fedele di MathNet DiGamma (AS 103, Bernardo 1976 / Minka).
 * Sostituisce la ricorrenza con soglia 8 (differiva di ~3e-13). Wrapper C#:
 * NaN o x<=0 → NaN. */
double gmb_digamma(double x)
{
    if (isnan(x) || x <= 0) return NAN;
    const double c = 12.0;
    const double d1 = -0.57721566490153286;
    const double d2 = 1.6449340668482264365;
    const double s = 1e-6;
    const double s3 = 1.0 / 12.0, s4 = 1.0 / 120.0, s5 = 1.0 / 252.0, s6 = 1.0 / 240.0, s7 = 1.0 / 132.0;
    if (x <= s) return d1 - (1.0 / x) + (d2 * x);
    double result = 0;
    while (x < c) { result -= 1.0 / x; x += 1.0; }
    double r = 1.0 / x;
    result += log(x) - (0.5 * r);
    r *= r;
    result -= r * (s3 - (r * (s4 - (r * (s5 - (r * (s6 - (r * s7))))))));
    return result;
}

/* erfinv(x): Newton su erf. */
double gmb_erfinv(double x)
{
    if (x < -1 || x > 1) return NAN;
    if (x == 1) return INFINITY;
    if (x == -1) return -INFINITY;
    double y = x * 0.8862269254527580; /* sqrt(pi)/2 * x come stima */
    for (int i = 0; i < 40; i++)
    {
        double e = gmb_erf(y) - x;
        if (fabs(e) < 1e-15) break;
        y -= e * 0.8862269254527580 * exp(y * y);
    }
    return y;
}

/* erfcinv(x): Newton su erfc. */
double gmb_erfcinv(double x)
{
    if (x < 0 || x > 2) return NAN;
    if (x == 0) return INFINITY;
    if (x == 2) return -INFINITY;
    double y = 1.0 - x / 2.0;
    for (int i = 0; i < 40; i++)
    {
        double e = erfc(y) - x;
        if (fabs(e) < 1e-15) break;
        y += e * 0.8862269254527580 * exp(y * y);
    }
    return y;
}

/* gammaincinv(a,y): x tale che P(a,x)=y — bisection con upper crescente. */
double gmb_gammaincinv(double a, double y)
{
    if (a <= 0 || y < 0 || y > 1) return NAN;
    if (y == 0) return 0;
    if (y == 1) return INFINITY;
    double lo = 0, hi = 1;
    while (gmb_gamma_p(a, hi) < y) hi *= 2;
    for (int i = 0; i < 60; i++)
    {
        double mid = 0.5 * (lo + hi);
        if (gmb_gamma_p(a, mid) < y) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

/* gammainccinv(a,y): x tale che Q(a,x)=y (Q=1−P). */
double gmb_gammainccinv(double a, double y)
{
    if (a <= 0 || y < 0 || y > 1) return NAN;
    if (y == 0) return INFINITY;
    if (y == 1) return 0;
    return gmb_gammaincinv(a, 1.0 - y);
}

/* betaincinv(a,b,y): x tale che I_x(a,b)=y — bisection su [0,1]. */
double gmb_betaincinv(double a, double b, double y)
{
    if (a <= 0 || b <= 0 || y < 0 || y > 1) return NAN;
    if (y == 0) return 0;
    if (y == 1) return 1;
    double lo = 0, hi = 1;
    for (int i = 0; i < 60; i++)
    {
        double mid = 0.5 * (lo + hi);
        if (gmb_incbeta(a, b, mid) < y) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

/* ── Bessel (scipy.special jv/yv/iv/kv) — serie + formule con digamma ── */
#define GMB_EULER_GAMMA 0.5772156649015329

static double gmb_bessel_series(double v, double x, double sign)
{
    /* Σ sign^m (x/2)^(2m+v) / (m! Γ(m+v+1)) — Jv (sign=-1), Iv (sign=+1) */
    double half = 0.5 * x;
    double term = pow(half, v) / tgamma(v + 1);
    double sum = term;
    for (int m = 1; m <= 60; m++)
    {
        term *= (sign * half * half) / ((double)m * (v + m));
        sum += term;
        if (fabs(term) < fabs(sum) * 1e-15) break;
    }
    return sum;
}

double gmb_besselj(double v, double x)
{
    if (x < 0) return gmb_besselj(v, -x) * (fmod(v, 2.0) == 0 ? 1.0 : -1.0);
    return gmb_bessel_series(v, x, -1.0);
}

double gmb_besseli(double v, double x)
{
    if (x < 0) return gmb_bessel_series(v, -x, 1.0) * (fmod(v, 2.0) == 0 ? 1.0 : -1.0);
    return gmb_bessel_series(v, x, 1.0);
}

/* ── Bessel Y/K: AMOS (D. E. Amos) via f2c — bit-esatto con MathNet 5.0.0.
 * Sostituisce la serie + limite d=1e-4 (errore ~1e-9). Sorgenti in src/amos/. ── */
extern int zbesy_(double*, double*, double*, int*, int*, double*, double*, int*, double*, double*, int*);
extern int zbesk_(double*, double*, double*, int*, int*, double*, double*, int*, int*);

double gmb_bessely(double v, double x)
{
    if (isnan(v) || isnan(x) || x < 0.0) return NAN;
    double zr = x, zi = 0.0;
    int kode = 1, n = 1, nz = 0, ierr = 0;
    double cyr = 0.0, cyi = 0.0, cwrkr = 0.0, cwrki = 0.0;
    zbesy_(&zr, &zi, &v, &kode, &n, &cyr, &cyi, &nz, &cwrkr, &cwrki, &ierr);
    return cyr;
}

double gmb_besselk(double v, double x)
{
    if (isnan(v) || isnan(x) || x < 0.0) return NAN;
    if (x == 0.0) return INFINITY;
    if (x > 710.0 * (1.0 + fabs(v))) return 0.0; /* underflow (parità MathNet) */
    double zr = x, zi = 0.0;
    int kode = 1, n = 1, nz = 0, ierr = 0;
    double cyr = 0.0, cyi = 0.0;
    zbesk_(&zr, &zi, &v, &kode, &n, &cyr, &cyi, &nz, &ierr);
    return cyr;
}

/* ── Nucleo scipy.signal (SIGNAL namespace) — double[] ── */
/* NB: lunghezze e outn come gmb_int_t (int64_t): i companion _len del C99 sono gmb_int_t. */
double* gmb_signal_convolve(const double* x, gmb_int_t nx, const double* k, gmb_int_t nk, gmb_int_t* outn)
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

double* gmb_signal_correlate(const double* x, gmb_int_t nx, const double* k, gmb_int_t nk, gmb_int_t* outn)
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

double* gmb_signal_window(gmb_int_t n, int kind, gmb_int_t* outn)
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
   la VM/C passa un wrapper su gmb_vm_call_closure_sync con (vm, task, closure). */

static const double gmb_k15_nodes[7] = {
    0.9914553711208126, 0.9491079123427585, 0.8648644233597691, 0.7415311855993944,
    0.5860872354676911, 0.4058451513773972, 0.2077849550078985
};
static const double gmb_k15_w[8] = {
    0.0229353220105292, 0.0630920926299786, 0.1047900103222501, 0.1406532597155259,
    0.1690047266392679, 0.1903505780647854, 0.2044329400752989, 0.2094821410847278
};
static const double gmb_k7_w[4] = {
    0.1294849661688697, 0.2797053914892767, 0.3818300505051189, 0.4179591836734694
};

static double gmb_gk15(double (*f)(double, void*), void* ctx, double a, double b, double* err)
{
    double mid = 0.5 * (a + b), half = 0.5 * (b - a);
    double s15 = gmb_k15_w[7] * f(mid, ctx);
    double s7 = gmb_k7_w[3] * f(mid, ctx);
    int gidx[3] = { 1, 3, 5 }, g = 0;
    for (int i = 0; i < 7; i++)
    {
        double dx = half * gmb_k15_nodes[i];
        double f1 = f(mid - dx, ctx), f2 = f(mid + dx, ctx);
        s15 += gmb_k15_w[i] * (f1 + f2);
        if (g < 3 && i == gidx[g]) { s7 += gmb_k7_w[g] * (f1 + f2); g++; }
    }
    double r15 = s15 * half;
    *err = fabs(r15 - s7 * half);
    return r15;
}

static double gmb_gk_adaptive(double (*f)(double, void*), void* ctx, double a, double b, double thresh, int depth, double* err)
{
    double mid = 0.5 * (a + b), e1, e2;
    double L = gmb_gk15(f, ctx, a, mid, &e1);
    double R = gmb_gk15(f, ctx, mid, b, &e2);
    double e = e1 + e2;
    if (depth < 24 && e > thresh)
    {
        double tl, tr;
        double e2v = gmb_gk_adaptive(f, ctx, a, mid, thresh * 0.5, depth + 1, &tl)
                     + gmb_gk_adaptive(f, ctx, mid, b, thresh * 0.5, depth + 1, &tr);
        *err = e2v;
        return tl + tr;
    }
    *err = e;
    return L + R;
}

double gmb_quad_gk(double (*f)(double, void*), void* ctx, double a, double b)
{
    if (a == b) return 0.0;
    double e;
    double s = gmb_gk15(f, ctx, a, b, &e);
    double thresh = 1e-12 * (1.0 + fabs(s));
    return gmb_gk_adaptive(f, ctx, a, b, thresh, 0, &e);
}

double gmb_simpson_ctx(double (*f)(double, void*), void* ctx, double a, double b, int n)
{
    if (n < 2) n = 2;
    if (n % 2) n++;
    double h = (b - a) / n;
    double sum = f(a, ctx) + f(b, ctx);
    for (int i = 1; i < n; i++) sum += (i % 2 == 1 ? 4.0 : 2.0) * f(a + i * h, ctx);
    return sum * h / 3.0;
}

double gmb_trapezoid(const double* y, const double* x, int n)
{
    if (n < 2) return 0.0;
    double s = 0;
    for (int i = 0; i < n - 1; i++) s += 0.5 * (y[i] + y[i + 1]) * (x[i + 1] - x[i]);
    return s;
}

double* gmb_cumtrapezoid(const double* y, const double* x, int n, gmb_int_t* outn)
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

static double gmb_solve_ode_tramp(double t, double y, void* ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t*)ctx;
    return ((double(*)(void*, double, double))fn.fn)(fn.ctx, t, y);
}

double* gmb_solve_ode_c99(gmb_fn_t fn, double y0, double t0, double t1, int steps, gmb_int_t* outn)
{
    return gmb_solve_ode_ctx(gmb_solve_ode_tramp, &fn, y0, t0, t1, steps, outn);
}

double* gmb_solve_ode_ctx(double (*f)(double, double, void*), void* ctx, double y0, double t0, double t1, int steps, gmb_int_t* outn)
{
    if (steps < 1) steps = 1;
    double h = (t1 - t0) / steps;
    double* ys = (double*)malloc(sizeof(double) * (size_t)(steps + 1));
    if (!ys) { *outn = 0; return NULL; }
    double t = t0, y = y0;
    ys[0] = y0;
    for (int i = 0; i < steps; i++)
    {
        double k1 = f(t, y, ctx);
        double k2 = f(t + 0.5 * h, y + 0.5 * h * k1, ctx);
        double k3 = f(t + 0.5 * h, y + 0.5 * h * k2, ctx);
        double k4 = f(t + h, y + h * k3, ctx);
        y += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        t += h;
        ys[i + 1] = y;
    }
    *outn = steps + 1;
    return ys;
}

static double* gmb_dense_solve(double* a, int n)
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

double gmb_cubic_spline(const double* x, const double* y, int n, double tx)
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
    double* m = gmb_dense_solve(a, n);
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

double* gmb_fftfreq(int n, double d, gmb_int_t* outn)
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

double* gmb_fftshift(const double* arr, int n, gmb_int_t* outn)
{
    if (!arr || n < 2) { *outn = 0; return NULL; }
    double* r = (double*)malloc(sizeof(double) * (size_t)n);
    if (!r) { *outn = 0; return NULL; }
    int shift = n - n / 2;
    for (int i = 0; i < n; i++) r[i] = arr[(i + shift) % n];
    *outn = n;
    return r;
}

double* gmb_ifftshift(const double* arr, int n, gmb_int_t* outn)
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
double gmb_gamma_pdf(double shape, double scale, double x)
{
    if (x < 0 || shape <= 0 || scale <= 0) return 0;
    return pow(x / scale, shape - 1.0) * exp(-x / scale) / (scale * gmb_gamma(shape));
}
double gmb_gamma_cdf(double shape, double scale, double x)
{
    if (x < 0) return 0;
    return gmb_gamma_p(shape, x / scale);
}

double gmb_gamma_sf(double shape, double scale, double x)
{
    return 1.0 - gmb_gamma_cdf(shape, scale, x);
}
double gmb_gamma_ppf(double shape, double scale, double p)
{
    if (p <= 0) return 0;
    if (p >= 1) return INFINITY;
    return gmb_gammaincinv(shape, p) * scale;
}
double gmb_gamma_isf(double shape, double scale, double p)
{
    return gmb_gamma_ppf(shape, scale, 1.0 - p);
}

double gmb_dist_normal_pdf(double mean, double sd, double x) {
    if (sd <= 0) return 0;
    double z = (x - mean) / sd;
    return exp(-0.5 * z * z) / (sd * sqrt(2 * 3.141592653589793));
}
double gmb_dist_normal_cdf(double mean, double sd, double x) {
    if (sd <= 0) return 0;
    return gmb_normal_cdf((x - mean) / sd);
}
double gmb_dist_chi_pdf(double df, double x) {
    if (df <= 0 || x < 0) return 0;
    /* Chi (non chi-squared): 2^(1-k/2) x^(k-1) e^(-x^2/2) / Γ(k/2) — parità scipy/IL. */
    return pow(2.0, 1.0 - df / 2.0) * pow(x, df - 1.0) * exp(-x * x / 2.0) / tgamma(df / 2.0);
}
double gmb_dist_chi_cdf(double df, double x) {
    if (df <= 0) return 0;
    /* Chi CDF = P(k/2, x^2/2) — prima era la formula chi-squared P(k/2, x/2). */
    return x < 0 ? 0 : gmb_gamma_p(df / 2.0, x * x / 2.0);
}
/* Chi-squared (distribuzione distinta da Chi): pdf/cdf usate da CHISQUAREDPDF/CDF
 * su tutte le gambe C (VMC/C99/C99Native) — parità scipy/IL. */
double gmb_dist_chi2_pdf(double df, double x) {
    if (df <= 0 || x < 0) return 0;
    return pow(2.0, -df / 2.0) * pow(x, df / 2.0 - 1.0) * exp(-x / 2.0) / tgamma(df / 2.0);
}
double gmb_dist_chi2_cdf(double df, double x) {
    if (df <= 0) return 0;
    return x < 0 ? 0 : gmb_gamma_p(df / 2.0, x / 2.0);
}
double gmb_dist_beta_pdf(double a, double b, double x) {
    if (a <= 0 || b <= 0) return NAN;
    if (x < 0 || x > 1) return 0;
    return pow(x, a-1) * pow(1-x, b-1) / gmb_beta_fn(a, b);
}
double gmb_dist_beta_cdf(double a, double b, double x) {
    if (a <= 0 || b <= 0) return NAN;
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    return gmb_incbeta(a, b, x);
}
double gmb_dist_student_t_pdf(double df, double x) {
    if (df <= 0) return 0;
    return tgamma((df+1)/2) / (sqrt(df * 3.141592653589793) * tgamma(df/2)) * pow(1 + x*x/df, -(df+1)/2);
}
double gmb_dist_student_t_cdf(double df, double x) {
    if (df <= 0) return 0;
    double z = df / (df + x*x);
    double ib = gmb_incbeta(df/2, 0.5, z);
    return x >= 0 ? 1 - 0.5*ib : 0.5*ib;
}
double gmb_dist_weibull_pdf(double shape, double scale, double x) {
    if (shape <= 0 || scale <= 0) return NAN;
    if (x < 0) return 0;
    return (shape/scale) * pow(x/scale, shape-1) * exp(-pow(x/scale, shape));
}
double gmb_dist_weibull_cdf(double shape, double scale, double x) {
    if (shape <= 0 || scale <= 0) return NAN;
    return x < 0 ? 0 : 1 - exp(-pow(x/scale, shape));
}
double gmb_dist_exp_pdf(double rate, double x) {
    if (rate <= 0) return 0;
    return x < 0 ? 0 : rate * exp(-rate * x);
}
double gmb_dist_exp_cdf(double rate, double x) {
    if (rate <= 0) return 0;
    return x < 0 ? 0 : 1 - exp(-rate * x);
}
double gmb_dist_bernoulli_pmf(double p, int k) {
    if (p < 0 || p > 1) return 0;
    return k == 0 ? 1 - p : k == 1 ? p : 0;
}
double gmb_dist_binomial_pmf(double p, int n, int k) {
    if (p < 0 || p > 1 || n < 0 || k < 0 || k > n) return 0;
    return gmb_comb(n, k) * pow(p, k) * pow(1 - p, n - k);
}
double gmb_dist_poisson_pmf(double lambda, int k) {
    if (lambda <= 0 || k < 0) return 0;
    return exp(-lambda) * pow(lambda, k) / tgamma(k + 1);
}
double gmb_dist_geometric_pmf(double p, int k) {
    if (p < 0 || p > 1 || k <= 0) return 0;
    return p * pow(1 - p, k - 1);
}
double gmb_dist_negbinom_pmf(double r, double p, int k) {
    if (r <= 0 || p < 0 || p > 1 || k < 0) return 0;
    return gmb_comb(r + k - 1, k) * pow(p, r) * pow(1 - p, k);
}
double gmb_dist_hypergeom_pmf(int N, int K, int n, int k) {
    if (k < 0 || k > n || k > K || n - k > N - K) return 0;
    return gmb_comb(K, k) * gmb_comb(N - K, n - k) / gmb_comb(N, n);
}

double gmb_dist_lognormal_pdf(double mu, double sigma, double x) {
    if (sigma <= 0 || x <= 0) return 0;
    double z = (log(x) - mu) / sigma;
    return exp(-0.5 * z * z) / (x * sigma * sqrt(2 * 3.141592653589793));
}
double gmb_dist_lognormal_cdf(double mu, double sigma, double x) {
    if (sigma <= 0 || x <= 0) return 0;
    return gmb_normal_cdf((log(x) - mu) / sigma);
}
double gmb_dist_f_pdf(double d1, double d2, double x) {
    if (d1 <= 0 || d2 <= 0) return NAN;
    if (x < 0) return 0;
    if (x == 0 && d1 < 2) return 1.0 / 0.0;
    return exp(lgamma((d1+d2)/2) - lgamma(d1/2) - lgamma(d2/2)
               + (d1/2)*log(d1) + (d2/2)*log(d2) + (d1/2-1)*log(x)
               - ((d1+d2)/2)*log(d2 + d1*x));
}
double gmb_dist_f_cdf(double d1, double d2, double x) {
    if (d1 <= 0 || d2 <= 0) return NAN;
    if (x < 0) return 0;
    double y = d1 * x / (d1 * x + d2);
    return gmb_incbeta(d1/2, d2/2, y);
}
double gmb_dist_pareto_pdf(double xm, double alpha, double x) {
    if (alpha <= 0 || xm <= 0 || x < xm) return 0;
    return alpha * pow(xm, alpha) / pow(x, alpha + 1);
}
double gmb_dist_pareto_cdf(double xm, double alpha, double x) {
    if (alpha <= 0 || xm <= 0 || x < xm) return 0;
    return 1 - pow(xm / x, alpha);
}
double gmb_dist_logistic_pdf(double mu, double s, double x) {
    if (s <= 0) return 0;
    double z = (x - mu) / s;
    double e = exp(-z);
    return e / (s * (1 + e) * (1 + e));
}
double gmb_dist_logistic_cdf(double mu, double s, double x) {
    if (s <= 0) return 0;
    return 1 / (1 + exp(-(x - mu) / s));
}
double gmb_dist_triangular_pdf(double a, double b, double c, double x) {
    /* parametrizzazione invalida (max<=min o mode fuori [min,max]) → NaN, come il
     * MathNet/CommonLib C# (Triangular.PDF lancia, l'IL restituisce NaN). */
    if (b <= a || c < a || c > b) return NAN;
    if (x < a || x > b) return 0;
    if (x <= c) return (x == a) ? 0 : 2 * (x - a) / ((b - a) * (c - a));
    return 2 * (b - x) / ((b - a) * (b - c));
}

double gmb_dist_triangular_cdf(double a, double b, double c, double x) {
    if (b <= a || c < a || c > b) return NAN;
    if (x <= a) return 0;
    if (x >= b) return 1;
    if (x <= c) return ((x - a) * (x - a)) / ((b - a) * (c - a));
    return 1.0 - ((b - x) * (b - x)) / ((b - a) * (b - c));
}
double gmb_dist_uniform_pdf(double a, double b, double x) {
    if (b <= a) return 0;
    return (x >= a && x <= b) ? 1.0 / (b - a) : 0;
}
double gmb_dist_uniform_cdf(double a, double b, double x) {
    if (b <= a) return 0;
    if (x <= a) return 0;
    if (x >= b) return 1;
    return (x - a) / (b - a);
}
double gmb_dist_skewnormal_pdf(double x, double location, double scale, double shape) {
    if (scale <= 0) return 0;
    double z = (x - location) / scale;
    double phi = exp(-0.5 * z * z) / sqrt(2 * 3.141592653589793);
    return (2.0 / scale) * phi * gmb_normal_cdf(shape * z);
}

/* ── Inverse CDF (ppf) e ISF — parità scipy.stats 1.18 (formule identiche a CommonLib) ── */
double gmb_dist_normal_ppf(double mean, double sd, double p)
    { return mean + sd * sqrt(2.0) * gmb_erfinv(2.0 * p - 1.0); }
double gmb_dist_normal_isf(double mean, double sd, double p)
    { return gmb_dist_normal_ppf(mean, sd, 1.0 - p); }
double gmb_dist_lognormal_ppf(double mu, double sigma, double p)
    { return exp(gmb_dist_normal_ppf(mu, sigma, p)); }
double gmb_dist_chi2_ppf(double df, double p)
    { return 2.0 * gmb_gammaincinv(df / 2.0, p); }
double gmb_dist_chi2_isf(double df, double p)
    { return 2.0 * gmb_gammaincinv(df / 2.0, 1.0 - p); }
double gmb_dist_chi_ppf(double df, double p)
    { return sqrt(gmb_dist_chi2_ppf(df, p)); }
double gmb_dist_beta_ppf(double a, double b, double p)
    { return gmb_betaincinv(a, b, p); }
double gmb_dist_f_ppf(double d1, double d2, double p)
{
    double z = gmb_betaincinv(d1 / 2.0, d2 / 2.0, p);
    return d2 * z / (d1 * (1.0 - z));
}
double gmb_dist_f_isf(double d1, double d2, double p)
    { return gmb_dist_f_ppf(d1, d2, 1.0 - p); }
double gmb_dist_student_t_ppf(double df, double p)
{
    if (p == 0.5) return 0.0;
    double sign = p > 0.5 ? 1.0 : -1.0;
    return sign * sqrt(gmb_dist_f_ppf(1.0, df, fabs(2.0 * p - 1.0)));
}
double gmb_dist_student_t_isf(double df, double p)
    { return gmb_dist_student_t_ppf(df, 1.0 - p); }
double gmb_dist_exp_ppf(double rate, double p)
    { return -log(1.0 - p) / rate; }
double gmb_dist_exp_isf(double rate, double p)
    { return -log(p) / rate; }
double gmb_dist_weibull_ppf(double shape, double scale, double p)
    { return scale * pow(-log(1.0 - p), 1.0 / shape); }
double gmb_dist_logistic_ppf(double loc, double scale, double p)
    { return loc + scale * log(p / (1.0 - p)); }
double gmb_dist_uniform_ppf(double a, double b, double p)
    { return a + p * (b - a); }
double gmb_dist_pareto_ppf(double scale, double shape, double p)
    { return scale / pow(1.0 - p, 1.0 / shape); }
double gmb_dist_triangular_ppf(double a, double b, double c, double p)
{
    /* a=min, b=max, c=mode */
    double cstd = (c - a) / (b - a);
    double xstd = (p < cstd) ? sqrt(cstd * p) : 1.0 - sqrt((1.0 - cstd) * (1.0 - p));
    return a + (b - a) * xstd;
}

double gmb_dist_weibull_isf(double shape, double scale, double p) { return gmb_dist_weibull_ppf(shape, scale, 1.0 - p); }
double gmb_dist_uniform_isf(double a, double b, double p) { return gmb_dist_uniform_ppf(a, b, 1.0 - p); }
double gmb_dist_pareto_isf(double scale, double shape, double p) { return gmb_dist_pareto_ppf(scale, shape, 1.0 - p); }
double gmb_dist_beta_isf(double a, double b, double p) { return gmb_dist_beta_ppf(a, b, 1.0 - p); }
double gmb_dist_chi_isf(double k, double p) { return gmb_dist_chi_ppf(k, 1.0 - p); }
double gmb_dist_logistic_isf(double loc, double scale, double p) { return gmb_dist_logistic_ppf(loc, scale, 1.0 - p); }
double gmb_dist_lognormal_isf(double mu, double sigma, double p) { return gmb_dist_lognormal_ppf(mu, sigma, 1.0 - p); }
double gmb_dist_triangular_isf(double a, double b, double c, double p) { return gmb_dist_triangular_ppf(a, b, c, 1.0 - p); }

/* ==================== Analisi numerica: regressione, interpolazioni, FFT, time series ==================== */

/* LinearRegression OLS → slope, intercept, r2 */
void gmb_analysis_linear_regression(const double* x, const double* y, int n, double* slope, double* intercept, double* r2)
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

static int gmb_interp_find_segment(const double* x, int n, double tx)
{
    if (tx <= x[0]) return 0;
    for (int i = 1; i < n; i++) if (tx < x[i]) return i - 1;
    return n - 2;
}

double gmb_interp_linear(const double* x, const double* y, int n, double tx)
{
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    int i = gmb_interp_find_segment(x, n, tx);
    double t = (tx - x[i]) / (x[i+1] - x[i]);
    return y[i] + t * (y[i+1] - y[i]);
}

double gmb_interp_step(const double* x, const double* y, int n, double tx)
{
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    for (int i = 1; i < n; i++) if (tx < x[i]) return y[i-1];
    return y[n-1];
}

double gmb_interp_clamp(const double* x, const double* y, int n, double tx, double (*fn)(const double*, const double*, int, double))
{
    if (tx < x[0]) return y[0];
    if (tx > x[n-1]) return y[n-1];
    return fn(x, y, n, tx);
}

double gmb_interp_polynomial(const double* x, const double* y, int n, double tx)
{
    double* w = (double*)calloc(n, sizeof(double));
    if (!w) return 0;
    for (int j = 0; j < n; j++)
    {
        w[j] = 1.0;
        for (int k = 0; k < n; k++)
            if (k != j) w[j] /= (x[j] - x[k]);
    }
    double res = 0;
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

double gmb_interp_akima(const double* x, const double* y, int n, double tx)
{
    if (n < 2) return y[0];
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    /* Slopes delle secanti d[i] = (y[i+1]-y[i])/(x[i+1]-x[i]). */
    double* d = (double*)malloc((size_t)(n > 0 ? n : 1) * sizeof(double));
    double* t = (double*)malloc((size_t)(n > 0 ? n : 1) * sizeof(double));
    if (!d || !t) { free(d); free(t); return y[0]; }
    int m = n - 1;
    for (int j = 0; j < m; j++) d[j] = (y[j+1] - y[j]) / (x[j+1] - x[j]);
    /* Derivate ai nodi — canone ESATTO MathNet CubicSpline.InterpolateAkimaSorted (v5):
     * t[0]=d0; t[1]=(d0+d1)/2; t[i]=(w1*d[i-1]+w2*d[i])/(w1+w2) con w1=|d[i+1]-d[i]|,
     * w2=|d[i-1]-d[i-2]| per i=2..n-3; t[n-2]=(d[n-3]+d[n-2])/2; t[n-1]=d[n-2].
     * (La vecchia versione "estrapolava" male gli estremi con guardie di indice errate →
     * valori diversi da MathNet su IL/PCode, es. Sample AnalysisTests Spline 5.175 vs 5.1625.) */
    if (n == 2) { t[0] = t[1] = d[0]; }
    else if (n == 3) { t[0] = d[0]; t[1] = 0.5*(d[0]+d[1]); t[2] = d[1]; }
    else {
        t[0] = d[0];
        t[1] = 0.5*(d[0] + d[1]);
        for (int i = 2; i <= n - 3; i++) {
            double w1 = fabs(d[i+1] - d[i]);
            double w2 = fabs(d[i-1] - d[i-2]);
            t[i] = (w1 + w2 != 0.0) ? (w1*d[i-1] + w2*d[i]) / (w1 + w2) : 0.5*(d[i-1] + d[i]);
        }
        t[n-2] = 0.5*(d[n-3] + d[n-2]);
        t[n-1] = d[n-2];
    }
    int i = gmb_interp_find_segment(x, n, tx);
    double h = x[i+1] - x[i], u = (tx - x[i]) / h;
    double y0 = y[i], y1 = y[i+1], ta = t[i], tb = t[i+1];
    double a = 2*(y0-y1) + (ta + tb)*h;
    double b = 3*(y1-y0) - (2*ta + tb)*h;
    double c = ta*h;
    double r = a*u*u*u + b*u*u + c*u + y0;
    free(d); free(t);
    return r;
}

/* PCHIP (Fritsch–Carlson) */
double gmb_interp_pchip(const double* x, const double* y, int n, double tx)
{
    if (tx <= x[0]) return y[0];
    if (tx >= x[n-1]) return y[n-1];
    int i = gmb_interp_find_segment(x, n, tx);
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
void gmb_fft(double* re, double* im, int n, int inverse)
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
        gmb_fft(br, bi, m, 0);
        gmb_fft(cr, ci, m, 0);
        for (int k = 0; k < m; k++)
        {
            double tr = br[k] * cr[k] - bi[k] * ci[k];
            bi[k] = br[k] * ci[k] + bi[k] * cr[k];
            br[k] = tr;
        }
        gmb_fft(br, bi, m, 1);
        /* applica a[k]: l'IFFT (gmb_fft inverse) ha GIA' diviso per m (riga 1689) —
         * qui NON si divide di nuovo (prima /m doppio rendeva il Bluestein scalato di 1/m). */
        for (int k = 0; k < n; k++)
        {
            double ang = wsign * 3.141592653589793 * ((double)(k * k % (2 * n))) / (double)n;
            double a = cos(ang), a2 = sin(ang);
            double r = br[k], ii = bi[k];
            re[k] = r * a - ii * a2;
            im[k] = r * a2 + ii * a;
        }
        /* Normalizzazione INVERSE 1/n: il ramo radix-2 la applica (sotto), il Bluestein la
         * ometteva → gmb_fft(inverse=1) su n non potenza di 2 tornava il valore GREZZO scalato
         * di n (es. NUMERICAL.Ifft di un segnale a 9 elementi: round-trip 1.0 → 9.0). Il /m
         * interno è la normalizzazione della CONVOLUZIONE (necessaria e corretta in entrambe le
         * direzioni); la 1/n è la normalizzazione della DFT inversa e va aggiunta QUI. */
        if (inverse) for (int k = 0; k < n; k++) { re[k] /= n; im[k] /= n; }
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

/* FFT su segnale reale → matrice n×2 (col0 re, col1 im) per `DIM x() = NUMERICAL.Fft(sig)`.
   Inverse applica 1/n (round-trip ricostruisce il segnale). */
double* gmb_yule_walker(const double* acf, int n, int p, gmb_int_t* out_len)
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

double* gmb_ma_coefficients(const double* res, int n, int q, gmb_int_t* out_len)
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

double gmb_euclidean_distance_squared(const double* a, const double* b, int n)
{
    double s = 0;
    for (int i = 0; i < n; i++) { double d = a[i] - b[i]; s += d * d; }
    return s;
}

gmb_double_t** gmb_fft_complex_matrix(const double* re_in, const double* im_in, int n, int inverse)
{
    if (n <= 0 || !re_in) return NULL;
    gmb_double_t** out = (gmb_double_t**)calloc((size_t)n, sizeof(gmb_double_t*));
    if (!out) return NULL;
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); free(out); return NULL; }
    for (int i = 0; i < n; i++) { re[i] = re_in[i]; im[i] = im_in ? im_in[i] : 0.0; }
    gmb_fft(re, im, n, inverse);
    /* Scala MathNet simmetrica 1/sqrt(N) (canone IL/PCode; gmb_fft usa scipy:
     * forward grezza, inverse 1/n). Forward: *1/sqrt(n); inverse: gmb_fft ha già /n → *sqrt(n). */
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

gmb_double_t** gmb_fft_matrix(const double* x, int n, int inverse)
{
    if (n <= 0 || !x) return NULL;
    gmb_double_t** out = (gmb_double_t**)calloc((size_t)n, sizeof(gmb_double_t*));
    if (!out) return NULL;
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); free(out); return NULL; }
    for (int i = 0; i < n; i++) re[i] = x[i];
    gmb_fft(re, im, n, inverse);
    /* Scala MathNet simmetrica 1/sqrt(N) (canone IL/PCode; gmb_fft usa scipy:
     * forward grezza, inverse 1/n). Forward: *1/sqrt(n); inverse: gmb_fft ha già /n → *sqrt(n). */
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
void gmb_holt_winters(const double* y, int n, double alpha, double beta, double gamma, int period,
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
static int gmb_mat_inv(const double* m, int n, double* out);
static double gmb_adf_pvalue(double tau)
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

void gmb_adf_test(const double* y, int n, double* statistic, double* pvalue)
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
    /* risolvi (AtA) x = Atb con gmb_dense_solve (matrice 3×4: AtA + rhs) */
    double* aug = (double*)malloc(12 * sizeof(double));
    if (!aug) { *statistic = 0; *pvalue = 0.5; return; }
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) aug[r * 4 + c] = ata[r * 3 + c];
    for (int r = 0; r < 3; r++) aug[r * 4 + 3] = atb[r];
    double* coeffs = gmb_dense_solve(aug, 3);
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
    /* inv(AtA)[1,1] via gmb_mat_inv (layout n×n) */
    double inv[9];
    double stat = 0;
    if (gmb_mat_inv(ata, 3, inv))
    {
        double varG = sigma2 * inv[1 * 3 + 1];
        double seGamma = varG > 0 ? sqrt(varG) : 0.0;
        stat = seGamma > 0 ? gamma / seGamma : 0.0;
    }
    *statistic = stat;
    *pvalue = gmb_adf_pvalue(stat);
    free(coeffs);
}

/* K-means 1D */
void gmb_kmeans(const double* data, int n, int k, double* centroids_out)
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
int gmb_arima_fit(const double* y, int n, int p, int d, int q,
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
            double* aug = (double*)malloc((size_t)m * (m + 1) * sizeof(double));
            if (aug)
            {
                for (int r = 0; r < m; r++)
                {
                    for (int c = 0; c < m; c++) aug[r * (m + 1) + c] = ata[r * m + c];
                    aug[r * (m + 1) + m] = atb[r];
                }
                double* arLong = gmb_dense_solve(aug, m);
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
                coeffs = gmb_dense_solve(aug, cols);
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
static double gmb_stats_mean(const double* x, int n) { if (n<=0) return 0; double s=0; for(int i=0;i<n;i++) s+=x[i]; return s/n; }

/* Inversione di matrice generica (Gauss-Jordan), dimensione arbitraria (n×n
   row-major, buffer allocato dinamicamente). 1 = ok, 0 = singolare/memoria. */
static int gmb_mat_inv(const double* m, int n, double* out)
{
    double *a;
    int i, j, col, r;
    if (n <= 0) return 0;
    a = (double*)malloc((size_t)(n * n) * sizeof(double));
    if (!a) return 0;
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) a[i*n+j] = m[i*n+j];
    memset(out, 0, (size_t)(n * n) * sizeof(double));
    for (i = 0; i < n; i++) out[i*n+i] = 1;
    for (col = 0; col < n; col++)
    {
        double piv = fabs(a[col*n+col]); int pr = col;
        for (r = col+1; r < n; r++) if (fabs(a[r*n+col]) > piv) { piv = fabs(a[r*n+col]); pr = r; }
        if (piv < 1e-15) { free(a); return 0; }
        if (pr != col)
        {
            for (j = 0; j < n; j++) { double t = a[col*n+j]; a[col*n+j] = a[pr*n+j]; a[pr*n+j] = t; t = out[col*n+j]; out[col*n+j] = out[pr*n+j]; out[pr*n+j] = t; }
        }
        double d = a[col*n+col];
        for (j = 0; j < n; j++) { a[col*n+j] /= d; out[col*n+j] /= d; }
        for (r = 0; r < n; r++)
        {
            if (r == col) continue;
            double f = a[r*n+col];
            if (f == 0) continue;
            for (j = 0; j < n; j++) { a[r*n+j] -= f*a[col*n+j]; out[r*n+j] -= f*out[col*n+j]; }
        }
    }
    free(a);
    return 1;
}

/* Cholesky solve Ax=b (A SPD), ritorna soluzione o NULL. */
int gmb_cholesky_solve(const double* a, const double* b, int n, double* x)
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
void gmb_analysis_pca(const double* data, int m, int n, double* eigenvals, double* eigenvecs)
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
void gmb_analysis_anova(const double* groups, int* groupSizes, int k, double* f, double* p, double* ssb, double* ssw)
{
    double grandMean = 0; int N = 0;
    const double* g = groups;
    for (int gi = 0; gi < k; gi++) { for (int i = 0; i < groupSizes[gi]; i++) grandMean += g[i]; g += groupSizes[gi]; N += groupSizes[gi]; }
    if (N == 0 || k < 2) { *f = 0; *p = 1; *ssb = 0; *ssw = 0; return; }
    grandMean /= N;
    double ssbV = 0, sswV = 0;
    g = groups;
    for (int gi = 0; gi < k; gi++)
    {
        double gm = 0;
        for (int i = 0; i < groupSizes[gi]; i++) gm += g[i];
        gm /= groupSizes[gi];
        ssbV += groupSizes[gi] * pow(gm - grandMean, 2);
        for (int i = 0; i < groupSizes[gi]; i++) sswV += pow(g[i] - gm, 2);
        g += groupSizes[gi];
    }
    double msb = ssbV / (k - 1);
    double msw = N > k ? sswV / (N - k) : 1;
    double fv = msw != 0 ? msb / msw : 0;
    /* gmb_dist_f_cdf(d1=dfBetween=k-1, d2=dfWithin=N-k, x=fv) — prima gli argomenti erano
     * scambiati (fv, k-1, N-k): CDF valutata con d1=fv, d2=dfBetween, x=dfWithin → p-value
     * errato (es. 0.299 vs canone 0.0709 per F(1,6,4.8)). */
    double pv = fv > 0 ? 1.0 - gmb_dist_f_cdf((double)(k - 1), (double)(N - k), fv) : 1.0;
    *f = fv; *p = pv; *ssb = ssbV; *ssw = sswV;
}

/* MULTIVARIATE.Pca → MULTIVARIATE.PcaResult (dict {EigenValues, EigenVectors}) da matrice
 * RETTANGOLARE flat (k osservazioni × n variabili, row-major). Canone IlRuntime.Pca
 * (power iteration, stessa gmb_analysis_pca). */
gmb_val_t gmb_pca_result_rect(const double* data, int k, int n)
{
    gmb_dict_t d = gmb_dict_new();
    if (!data || k <= 0 || n <= 0 || n > 8)
    {
        gmb_dict_set(&d, "EigenValues", gmb_make_val_list(gmb_list_new()));
        gmb_dict_set(&d, "EigenVectors", gmb_make_val_list(gmb_list_new()));
        return gmb_make_val_dict(d);
    }
    double ev[8] = {0}, evec[64] = {0};
    gmb_analysis_pca(data, k, n, ev, evec);
    gmb_list_t evl = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&evl, gmb_make_val_double(ev[i]));
    gmb_dict_set(&d, "EigenValues", gmb_make_val_list_take(&evl));
    gmb_list_t evcl = gmb_list_new();
    for (int i = 0; i < n; i++)
    {
        gmb_list_t row = gmb_list_new();
        for (int j = 0; j < n; j++) gmb_list_add(&row, gmb_make_val_double(evec[i * 8 + j]));
        gmb_list_add(&evcl, gmb_make_val_list_take(&row));
    }
    gmb_dict_set(&d, "EigenVectors", gmb_make_val_list_take(&evcl));
    return gmb_make_val_dict_take(&d);
}

/* PCA da gmb_val_t lista di liste (righe = osservazioni) — percorso generico. */
gmb_val_t gmb_pca_result(gmb_val_t v)
{
    if (v.kind != GMB_TYPE_ARRAY && v.kind != GMB_TYPE_OBJECT) { gmb_val_t e = {0}; return e; }
    gmb_list_t rows = gmb_val_as_list(v);
    int k = (int)gmb_list_count(rows);
    if (k <= 0) { gmb_val_t e = {0}; return e; }
    gmb_list_t r0 = gmb_val_as_list(gmb_list_get(rows, 0));
    int n = (int)gmb_list_count(r0);
    if (n <= 0 || n > 8) { gmb_val_t e = {0}; return e; }
    double* flat = (double*)malloc((size_t)(k * n) * sizeof(double));
    if (!flat) { gmb_val_t e = {0}; return e; }
    for (int i = 0; i < k; i++)
    {
        gmb_list_t row = gmb_val_as_list(gmb_list_get(rows, i));
        for (int j = 0; j < n; j++) flat[i * n + j] = gmb_val_to_double(gmb_list_get(row, j));
    }
    gmb_val_t r = gmb_pca_result_rect(flat, k, n);
    free(flat);
    return r;
}

/* PCA da matrice JAGGED (righe = osservazioni, tutte di pari lunghezza n). */
gmb_val_t gmb_pca_result_jagged(const double* const* rows, const int* sizes, int k)
{
    if (!rows || !sizes || k <= 0) { gmb_val_t e = {0}; return e; }
    int n = sizes[0];
    if (n <= 0 || n > 8) { gmb_val_t e = {0}; return e; }
    for (int i = 1; i < k; i++) if (sizes[i] != n) { gmb_val_t e = {0}; return e; }
    double* flat = (double*)malloc((size_t)(k * n) * sizeof(double));
    if (!flat) { gmb_val_t e = {0}; return e; }
    for (int i = 0; i < k; i++)
        for (int j = 0; j < n; j++) flat[i * n + j] = rows[i][j];
    gmb_val_t r = gmb_pca_result_rect(flat, k, n);
    free(flat);
    return r;
}

/* JARQUE-BERA test: JB = n/6 * (S² + (K-3)²/4) con S=skewness, K=kurtosis campionarie. */
double gmb_stats_jarque_bera(const double* data, int n)
{
    if (n < 4) return 0;
    double m = gmb_stats_mean(data, n);
    double s2 = 0, s3 = 0, s4 = 0;
    for (int i = 0; i < n; i++) { double d = data[i]-m; s2 += d*d; s3 += d*d*d; s4 += d*d*d*d; }
    double var = s2 / n;
    if (var == 0) return 0;
    double skew = (s3 / n) / pow(var, 1.5);
    double kurt = (s4 / n) / (var * var);
    return n / 6.0 * (skew * skew + (kurt - 3) * (kurt - 3) / 4.0);
}
double gmb_tm_min_semivariance(const double* r, int n, double thr) {
    double sum=0; int cnt=0;
    for(int i=0;i<n;i++) if(r[i]<thr){ double d=thr-r[i]; sum+=d*d; cnt++; }
    return cnt>0 ? sum/cnt : 0;
}
/* BACKTEST RUN(returns, strat) → dict {TotalReturn, AnnualizedReturn, MaxDrawdown,
   SharpeRatio, TradesCount, Turnover} (come la VM/C case 624). returns = lista di periodi
   (ogni periodo una lista di rendimenti asset) oppure lista piatta. */
/* TRADING.BACKTESTS.RUN su array 2-D rettangolare (flat rows×cols): rendimento per periodo =
   media degli asset della riga, poi gmb_tm_backtest. Parità IlRuntime.BacktestRun /
   PCodeProcessor case RUN / VM/C gmb_bf_624. */
/* PORTFOLIO OPTIMIZATION → dict PortfolioResult (OPT.MINVOLATILITY/MAXSHARPE/MEANVARIANCE/
   RISKPARITY/EQUALLYWEIGHTED, come VM case 183/184/190/192). cov è JAGGED (gmb_double_t*[n],
   righe irregolari); la funzione la compatta in contiguo n×n e chiama gmb_port_*.
   kind: 0=min_vol, 1=max_sharpe, 2=risk_parity, 3=equally_weighted, 4=mean_variance. */
/* TRADING.PORTFOLIO.EVOLVE(rets, cov, parametriGA) → dict PortfolioResult. L'IL minimizza la
   varianza del portafoglio con un GA (DifferentialEvolution); qui si usa gmb_port_min_vol
   (stesso obiettivo, deterministico) → weights/return/vol/sharpe. cov è JAGGED (gmb_double_t*[n]). */
/* TRADING.PORTFOLIO.BLACKLITTERMAN(marketCaps, cov, riskAversion, tau, P, Q, omega, riskFree)
   → PortfolioResult. Formula completa (parità CommonLib.Trading.Portfolio.BlackLitterman):
   Π = λ Σ w_mkt; posterior = [(τΣ)^-1 + Pᵀ Ω^-1 P]^-1 · [(τΣ)^-1 Π + Pᵀ Ω^-1 Q];
   poi MAXSHARPE(posterior, Σ, rf). cov/P/omega sono JAGGED (gmb_double_t**). */
/* B3 (2026-08-25): rendimenti/volatilità/azioni/obbligazioni mancanti — formule
   allineate a CommonLib.Trading.Metrics/General/Bonds (parità IL/PCodeVM). */
double gmb_fin_current_yield(double coupons, double price) { return price==0?0:coupons/price; }
double gmb_fin_ytm(double coupon, double fv, double price, double years) { double den=(fv+price)/2; if(years==0||den==0) return 0; return (coupon+(fv-price)/years)/den; }
double gmb_fin_zcb_value(double fv, double rate, double time) { double base=1+rate; if(base==0) return 0; return fv/pow(base, time); }
double gmb_fin_zcb_yield(double fv, double price, double time) { if(time==0||price==0||fv<=0||price<0) return 0; return pow(fv/price, 1.0/time)-1; }
double gmb_fin_bvps(double equity, double shares) { return shares==0?0:equity/shares; }
double gmb_fin_dps(double dividends, double shares) { return shares==0?0:dividends/shares; }
double gmb_fin_equity_multiplier(double assets, double equity) { return equity==0?0:assets/equity; }
double gmb_fin_nav(double assets, double liabilities, double shares) { return shares==0?0:(assets-liabilities)/shares; }
double gmb_fin_preferred_value(double dividend, double rate) { return rate==0?0:dividend/rate; }
double gmb_fin_stock_zero_growth(double dividend, double rate) { return rate==0?0:dividend/rate; }
double gmb_fin_eps(double netIncome, double shares) { return shares==0?0:netIncome/shares; }
double gmb_fin_diluted_eps(double netIncome, double shares, double instruments) { double den=shares+instruments; return den==0?0:netIncome/den; }
double gmb_fin_ps_ratio(double price, double salesPerShare) { return salesPerShare==0?0:price/salesPerShare; }
double gmb_fin_pb_ratio(double price, double bookValue) { return bookValue==0?0:price/bookValue; }
double gmb_fin_dividend_payout_ratio(double dividends, double netIncome) { return netIncome==0?0:dividends/netIncome; }

/* scipy.fft: rfft (componenti non negative) / irfft (ricostruzione Hermitiana). */
double* gmb_rfft(const double* x, int n, gmb_int_t* outn)
{
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); *outn = 0; return NULL; }
    for (int i = 0; i < n; i++) re[i] = x[i];
    gmb_fft(re, im, n, 0);
    int m = n / 2 + 1;
    double* out = (double*)malloc(sizeof(double) * (size_t)m * 2);
    if (!out) { free(re); free(im); *outn = 0; return NULL; }
    for (int k = 0; k < m; k++) { out[k * 2] = re[k]; out[k * 2 + 1] = im[k]; }
    *outn = m;
    free(re); free(im);
    return out;
}

double* gmb_irfft(const double* re, const double* im, int m, gmb_int_t* outn)
{
    int n = 2 * (m - 1);
    double* cre = (double*)calloc((size_t)n, sizeof(double));
    double* cim = (double*)calloc((size_t)n, sizeof(double));
    if (!cre || !cim) { free(cre); free(cim); *outn = 0; return NULL; }
    for (int k = 0; k < m; k++) { cre[k] = re[k]; cim[k] = im[k]; }
    for (int k = 1; k < m - 1; k++) { cre[n - k] = re[k]; cim[n - k] = -im[k]; }
    gmb_fft(cre, cim, n, 1);
    double* out = (double*)malloc(sizeof(double) * (size_t)n);
    if (!out) { free(cre); free(cim); *outn = 0; return NULL; }
    for (int i = 0; i < n; i++) out[i] = cre[i];
    *outn = n;
    free(cre); free(cim);
    return out;
}

double gmb_fin_portfolio_sharpe(double portfolio_return, double portfolio_stdev, double risk_free)
{
    if (portfolio_stdev == 0) return 0;
    return (portfolio_return - risk_free) / portfolio_stdev;
}

static void gmb_pca_jacobi(double* a, int n, double* e, double* v)
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

gmb_double_t** gmb_pca(const double* data, int rows, int cols, gmb_int_t* out_rows, gmb_int_t* out_cols, double* eigs)
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
    gmb_pca_jacobi(cov, cols, e, v);
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

gmb_val_t gmb_adf_test_result(const double* data, int n)
{
    gmb_dict_t d = gmb_dict_new();
    double s = 0, p = 0;
    gmb_adf_test(data, n, &s, &p);
    gmb_dict_set(&d, "Statistic", gmb_make_val_double(s));
    gmb_dict_set(&d, "PValue", gmb_make_val_double(p));
    return gmb_make_val_dict_take(&d);
}

gmb_val_t gmb_holt_winters_result(const double* y, int n, double alpha, double beta, double gamma, int period)
{
    gmb_dict_t d = gmb_dict_new();
    double* fit = (double*)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
    double mse = 0;
    if (fit) { gmb_holt_winters(y, n, alpha, beta, gamma, period, fit, n, &mse); }
    gmb_list_t fl = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&fl, gmb_make_val_double(fit ? fit[i] : 0.0));
    if (fit) free(fit);
    gmb_dict_set(&d, "Forecast", gmb_make_val_list_take(&fl));
    gmb_dict_set(&d, "Mse", gmb_make_val_double(mse));
    return gmb_make_val_dict_take(&d);
}

gmb_val_t gmb_arima_result(const double* y, int n, int p, int d, int q)
{
    gmb_dict_t out = gmb_dict_new();
    if (p < 0) p = 0; if (q < 0) q = 0; if (d < 0) d = 0;
    double* ac = (double*)calloc((size_t)(p > 0 ? p : 1), sizeof(double));
    double* ma = (double*)calloc((size_t)(q > 0 ? q : 1), sizeof(double));
    double aic = 0;
    int nd = n;
    for (int k = 0; k < d; k++) nd = nd > 1 ? nd - 1 : nd;
    double* fit = (double*)calloc((size_t)(nd > 0 ? nd : 1), sizeof(double));
    if (ac && ma && fit) gmb_arima_fit(y, n, p, d, q, ac, ma, &aic, fit);
    gmb_list_t al = gmb_list_new();
    for (int i = 0; i < p; i++) gmb_list_add(&al, gmb_make_val_double(ac ? ac[i] : 0.0));
    gmb_dict_set(&out, "ArCoeffs", gmb_make_val_list_take(&al));
    gmb_list_t ml = gmb_list_new();
    for (int i = 0; i < q; i++) gmb_list_add(&ml, gmb_make_val_double(ma ? ma[i] : 0.0));
    gmb_dict_set(&out, "MaCoeffs", gmb_make_val_list_take(&ml));
    gmb_dict_set(&out, "AIC", gmb_make_val_double(aic));
    gmb_list_t fl = gmb_list_new();
    for (int i = 0; i < nd; i++) gmb_list_add(&fl, gmb_make_val_double(fit ? fit[i] : 0.0));
    gmb_dict_set(&out, "Fitted", gmb_make_val_list_take(&fl));
    free(ac); free(ma); free(fit);
    return gmb_make_val_dict_take(&out);
}

gmb_val_t gmb_kmeans_result(const double* data, int n, int k)
{
    gmb_dict_t d = gmb_dict_new();
    double c[64] = {0};
    gmb_kmeans(data, n, k, c);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < k; i++) gmb_list_add(&l, gmb_make_val_double(c[i]));
    gmb_dict_set(&d, "Centroids", gmb_make_val_list_take(&l));
    return gmb_make_val_dict_take(&d);
}

int gmb_cholesky_solve_jagged(const double* const* a, const double* b, int n, double* x)
{
    if (!a || !b || !x || n <= 0) return -1;
    double* flat = (double*)calloc((size_t)(n * n), sizeof(double));
    if (!flat) return -1;
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) flat[i * n + j] = a[i][j];
    int rc = gmb_cholesky_solve(flat, b, n, x);
    free(flat);
    return rc;
}

/* Adattatori C99 per NUMERICAL.Integrate/Simpson (chiusure BASIC, gmb_fn_t).
 * Vivevano in gmb_optfit.c, rimosso con l'optimizer: senza queste due funzioni
 * il C generato da C99Native non linka (undefined gmb_quad_gk_c99/simpson_c99). */
static double gmb_c99_fn_scalar(double x, void *ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t *)ctx;
    return ((double (*)(void *, double))fn.fn)(fn.ctx, x);
}

double gmb_quad_gk_c99(gmb_fn_t fn, double a, double b)
{
    return gmb_quad_gk(gmb_c99_fn_scalar, &fn, a, b);
}

double gmb_simpson_c99(gmb_fn_t fn, double a, double b, int n)
{
    return gmb_simpson_ctx(gmb_c99_fn_scalar, &fn, a, b, n);
}

/* Noncentral chi-square: Poisson mixture of central chi-square (df+2j). */
double gmb_dist_nc_chi2_pdf(double df, double nc, double x)
{
    if (df <= 0 || nc < 0 || x < 0) return NAN;
    double half = nc * 0.5, w = exp(-half), sum = 0;
    for (int j = 0; j < 300; j++) {
        sum += w * gmb_dist_chi2_pdf(df + 2.0 * j, x);
        w *= half / (j + 1.0);
    }
    return sum;
}
double gmb_dist_nc_chi2_cdf(double df, double nc, double x)
{
    if (df <= 0 || nc < 0) return NAN;
    if (x <= 0) return 0;
    double half = nc * 0.5, w = exp(-half), sum = 0;
    for (int j = 0; j < 300; j++) {
        sum += w * gmb_dist_chi2_cdf(df + 2.0 * j, x);
        w *= half / (j + 1.0);
    }
    return sum > 1 ? 1 : sum;
}
double gmb_dist_nc_chi2_sf(double df, double nc, double x) { return 1.0 - gmb_dist_nc_chi2_cdf(df, nc, x); }

/* Noncentral beta: Poisson mixture of central beta(a+j,b). */
double gmb_dist_nc_beta_pdf(double a, double b, double nc, double x)
{
    if (a <= 0 || b <= 0 || nc < 0) return NAN;
    double half = nc * 0.5, w = exp(-half), sum = 0;
    for (int j = 0; j < 300; j++) { sum += w * gmb_dist_beta_pdf(a + j, b, x); w *= half / (j + 1.0); }
    return sum;
}
double gmb_dist_nc_beta_cdf(double a, double b, double nc, double x)
{
    if (a <= 0 || b <= 0 || nc < 0) return NAN;
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    double half = nc * 0.5, w = exp(-half), sum = 0;
    for (int j = 0; j < 300; j++) { sum += w * gmb_dist_beta_cdf(a + j, b, x); w *= half / (j + 1.0); }
    return sum > 1 ? 1 : sum;
}
double gmb_dist_nc_beta_sf(double a, double b, double nc, double x) { return 1.0 - gmb_dist_nc_beta_cdf(a, b, nc, x); }
/* Noncentral gamma (scale=1): Poisson mixture of central gamma(shape+j,1). */
double gmb_dist_nc_gamma_pdf(double shape, double nc, double x)
{
    if (shape <= 0 || nc < 0) return NAN;
    double half = nc * 0.5, w = exp(-half), sum = 0;
    for (int j = 0; j < 300; j++) { sum += w * gmb_gamma_pdf(shape + j, 1.0, x); w *= half / (j + 1.0); }
    return sum;
}
double gmb_dist_nc_gamma_cdf(double shape, double nc, double x)
{
    if (shape <= 0 || nc < 0) return NAN;
    if (x <= 0) return 0;
    double half = nc * 0.5, w = exp(-half), sum = 0;
    for (int j = 0; j < 300; j++) { sum += w * gmb_gamma_cdf(shape + j, 1.0, x); w *= half / (j + 1.0); }
    return sum > 1 ? 1 : sum;
}
double gmb_dist_nc_gamma_sf(double shape, double nc, double x) { return 1.0 - gmb_dist_nc_gamma_cdf(shape, nc, x); }

/* Noncentral F (via noncentral beta). Y = d1 X/(d1 X + d2) ~ ncbeta(d1/2,d2/2,nc). */
double gmb_dist_nc_f_pdf(double d1, double d2, double nc, double x)
{
    if (d1 <= 0 || d2 <= 0 || nc < 0) return NAN;
    if (x <= 0) return 0;
    double y = d1 * x / (d1 * x + d2);
    double dydx = d1 * d2 / ((d1 * x + d2) * (d1 * x + d2));
    double half = nc * 0.5, w = exp(-half), sum = 0;
    for (int j = 0; j < 400; j++) {
        sum += w * gmb_dist_beta_pdf(d1 * 0.5 + j, d2 * 0.5, y);
        if (j > half && w < 1e-18) break;
        w *= half / (j + 1.0);
    }
    return sum * dydx;
}
double gmb_dist_nc_f_cdf(double d1, double d2, double nc, double x)
{
    if (d1 <= 0 || d2 <= 0 || nc < 0) return NAN;
    if (x <= 0) return 0;
    double y = d1 * x / (d1 * x + d2);
    return gmb_dist_nc_beta_cdf(d1 * 0.5, d2 * 0.5, nc, y);
}
double gmb_dist_nc_f_sf(double d1, double d2, double nc, double x) { return 1.0 - gmb_dist_nc_f_cdf(d1, d2, nc, x); }

/* Noncentral t (Lenth 1989 series for F~, x >= 0). */
static double gmb_nc_t_ftilde(double df, double mu, double x)
{
    double phi = 0.5 * erfc(mu / sqrt(2.0));
    double half = 0.5 * mu * mu;
    double y = (x * x) / (x * x + df);
    double powr = exp(-half);
    double sum = 0.0;
    for (int j = 0; j < 400; j++) {
        double pj = powr / exp(gmb_lgamma((double)j + 1.0));
        double qj = mu / (sqrt(2.0) * exp(gmb_lgamma((double)j + 1.5))) * powr;
        sum += pj * gmb_incbeta((double)j + 0.5, df / 2.0, y)
             + qj * gmb_incbeta((double)j + 1.0, df / 2.0, y);
        if ((double)j > half && fabs(pj) + fabs(qj) < 1e-18) break;
        powr *= half;
    }
    return phi + 0.5 * sum;
}
double gmb_dist_nc_t_cdf(double df, double nc, double x)
{
    if (df <= 0) return NAN;
    if (nc == 0) return gmb_dist_student_t_cdf(df, x);
    if (x < 0) return 1.0 - gmb_nc_t_ftilde(df, -nc, -x);
    return gmb_nc_t_ftilde(df, nc, x);
}
double gmb_dist_nc_t_pdf(double df, double nc, double x)
{
    if (df <= 0) return NAN;
    if (nc == 0) return gmb_dist_student_t_pdf(df, x);
    if (x == 0) return exp(gmb_lgamma(0.5 * (df + 1.0)) - gmb_lgamma(0.5 * df) - 0.5 * nc * nc)
                    / sqrt(3.141592653589793 * df);
    return (df / x) * (gmb_dist_nc_t_cdf(df + 2.0, nc, x * sqrt(1.0 + 2.0 / df))
                       - gmb_dist_nc_t_cdf(df, nc, x));
}
double gmb_dist_nc_t_sf(double df, double nc, double x) { return 1.0 - gmb_dist_nc_t_cdf(df, nc, x); }

/* Zero-truncated Poisson (ESL #166). */
static double gmb_ztp_denom(double lambda)
{
    if (lambda < 1e-6) return lambda * (1.0 - lambda * 0.5);
    return -expm1(-lambda);
}
double gmb_dist_ztpoisson_pmf(double lambda, int k)
{
    if (lambda <= 0) return NAN;
    if (k < 1) return 0;
    return exp((double)k * log(lambda) - lgamma((double)k + 1.0) - lambda - log(gmb_ztp_denom(lambda)));
}
double gmb_dist_ztpoisson_cdf(double lambda, int k)
{
    if (lambda <= 0) return NAN;
    if (k < 1) return 0;
    double denom = gmb_ztp_denom(lambda);
    double p = lambda * exp(-lambda) / denom, s = p;
    for (int j = 2; j <= k; j++) {
        p *= lambda / (double)j;
        s += p;
        if (p < 1e-15 * s) break;
    }
    return s > 1 ? 1 : s;
}

/* --- Distribuzioni multivariate + copule archimedeane (MULTIVARIATE) --- */
static int gmb_mv_prepare(int d, const double* x, const double* loc, const double* cov,
                          double* half_logdet, double* q)
{
    double L[64][64], y[64];
    if (d <= 0 || d > 64 || !x || !loc || !cov) return 0;
    memset(L, 0, sizeof(L));
    for (int i = 0; i < d; i++)
        for (int j = 0; j <= i; j++) {
            double s = cov[i * d + j];
            for (int k = 0; k < j; k++) s -= L[i][k] * L[j][k];
            if (i == j) { if (s <= 0) return 0; L[i][j] = sqrt(s); }
            else L[i][j] = s / L[j][j];
        }
    *half_logdet = 0;
    for (int i = 0; i < d; i++) *half_logdet += log(L[i][i]);
    for (int i = 0; i < d; i++) {
        double s = x[i] - loc[i];
        for (int j = 0; j < i; j++) s -= L[i][j] * y[j];
        y[i] = s / L[i][i];
    }
    *q = 0;
    for (int i = 0; i < d; i++) *q += y[i] * y[i];
    return 1;
}
double gmb_mv_gauss_logpdf(int d, const double* x, const double* loc, const double* cov)
{
    double hl, q;
    if (!gmb_mv_prepare(d, x, loc, cov, &hl, &q)) return NAN;
    return -0.5 * q - hl - 0.5 * d * log(2.0 * 3.141592653589793);
}
double gmb_mv_gauss_pdf(int d, const double* x, const double* loc, const double* cov)
{
    return exp(gmb_mv_gauss_logpdf(d, x, loc, cov));
}
double gmb_mv_student_pdf(int d, const double* x, const double* loc, const double* cov, double nu)
{
    double hl, q, lognorm;
    if (nu <= 0) return NAN;
    if (isinf(nu)) return gmb_mv_gauss_pdf(d, x, loc, cov);
    if (!gmb_mv_prepare(d, x, loc, cov, &hl, &q)) return NAN;
    lognorm = lgamma(0.5 * (nu + d)) - lgamma(0.5 * nu) - 0.5 * d * log(nu * 3.141592653589793);
    return exp(lognorm - hl - 0.5 * (nu + d) * log1p(q / nu));
}
double gmb_copula_clayton_cdf(double u, double v, double theta)
{
    if (theta <= 0 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return NAN;
    double x = pow(u, -theta) + pow(v, -theta) - 1.0;
    return x > 0 ? pow(x, -1.0 / theta) : 0.0;
}
double gmb_copula_gumbel_cdf(double u, double v, double theta)
{
    if (theta < 1 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return NAN;
    double a = pow(-log(u), theta), b = pow(-log(v), theta);
    return exp(-pow(a + b, 1.0 / theta));
}
double gmb_copula_frank_cdf(double u, double v, double theta)
{
    if (theta <= 0 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return NAN;
    double a = exp(-theta * u), b = exp(-theta * v), e = exp(-theta);
    return -log(1.0 + (a - 1.0) * (b - 1.0) / (e - 1.0)) / theta;
}

/* --- Inverse dei non centrali: bisezione sulla CDF (parita' CommonLib) --- */
typedef double (*gmb_nc_cdf_fn)(const double* par, double x);
static double gmb_nc_cdf_chi2(const double* par, double x) { return gmb_dist_nc_chi2_cdf(par[0], par[1], x); }
static double gmb_nc_cdf_beta(const double* par, double x) { return gmb_dist_nc_beta_cdf(par[0], par[1], par[2], x); }
static double gmb_nc_cdf_gamma(const double* par, double x) { return gmb_dist_nc_gamma_cdf(par[0], par[1], x); }
static double gmb_nc_cdf_f(const double* par, double x) { return gmb_dist_nc_f_cdf(par[0], par[1], par[2], x); }
static double gmb_nc_cdf_t(const double* par, double x) { return gmb_dist_nc_t_cdf(par[0], par[1], x); }

static double gmb_nc_bisect(gmb_nc_cdf_fn f, const double* par, double p, double lo, double hi)
{
    if (!(p > 0.0 && p < 1.0)) return NAN;
    for (int i = 0; i < 200; i++) {
        double mid = 0.5 * (lo + hi);
        if (f(par, mid) < p) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}
static double gmb_nc_ppf_pos(gmb_nc_cdf_fn f, const double* par, double p)
{
    double lo = 0.0, hi = 1.0;
    if (!(p > 0.0 && p < 1.0)) return NAN;
    while (f(par, hi) < p && hi < 1e12) hi *= 2.0;
    return gmb_nc_bisect(f, par, p, lo, hi);
}
static double gmb_nc_ppf_t(gmb_nc_cdf_fn f, const double* par, double p)
{
    double lo = -1.0, hi = 1.0;
    if (!(p > 0.0 && p < 1.0)) return NAN;
    while (f(par, lo) > p && lo > -1e9) lo *= 2.0;
    while (f(par, hi) < p && hi < 1e9) hi *= 2.0;
    return gmb_nc_bisect(f, par, p, lo, hi);
}
double gmb_dist_nc_chi2_ppf(double df, double nc, double p)
{ double par[2] = { df, nc }; return gmb_nc_ppf_pos(gmb_nc_cdf_chi2, par, p); }
double gmb_dist_nc_chi2_isf(double df, double nc, double p) { return gmb_dist_nc_chi2_ppf(df, nc, 1.0 - p); }
double gmb_dist_nc_beta_ppf(double a, double b, double nc, double p)
{ double par[3] = { a, b, nc }; return gmb_nc_bisect(gmb_nc_cdf_beta, par, p, 0.0, 1.0); }
double gmb_dist_nc_beta_isf(double a, double b, double nc, double p) { return gmb_dist_nc_beta_ppf(a, b, nc, 1.0 - p); }
double gmb_dist_nc_gamma_ppf(double shape, double nc, double p)
{ double par[2] = { shape, nc }; return gmb_nc_ppf_pos(gmb_nc_cdf_gamma, par, p); }
double gmb_dist_nc_gamma_isf(double shape, double nc, double p) { return gmb_dist_nc_gamma_ppf(shape, nc, 1.0 - p); }
double gmb_dist_nc_f_ppf(double d1, double d2, double nc, double p)
{ double par[3] = { d1, d2, nc }; return gmb_nc_ppf_pos(gmb_nc_cdf_f, par, p); }
double gmb_dist_nc_f_isf(double d1, double d2, double nc, double p) { return gmb_dist_nc_f_ppf(d1, d2, nc, 1.0 - p); }
double gmb_dist_nc_t_ppf(double df, double nc, double p)
{ double par[2] = { df, nc }; return gmb_nc_ppf_t(gmb_nc_cdf_t, par, p); }
double gmb_dist_nc_t_isf(double df, double nc, double p) { return gmb_dist_nc_t_ppf(df, nc, 1.0 - p); }

/* --- gm_special2: funzioni speciali avanzate (port fedele) --- */
static double gmb_sp2_lgam(double x)
{
    static const double c[9] = {
        0.99999999999980993, 676.5203681218851, -1259.1392167224028,
        771.32342877765313, -176.61502916214059, 12.507343278686905,
        -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7
    };
    double a, t; int i;
    if (x < 0.5) return log(3.14159265358979323846 / sin(3.14159265358979323846 * x)) - gmb_sp2_lgam(1.0 - x);
    x -= 1.0;
    a = c[0];
    t = x + 7.5;
    for (i = 1; i < 9; i++) a += c[i] / (x + (double)i);
    return 0.5 * log(2.0 * 3.14159265358979323846) + (x + 0.5) * log(t) - t + log(a);
}
static double gmb_sp2_gam(double x)
{
    if (x == floor(x) && x <= 0.0) return NAN;
    if (x < 0.5) return 3.14159265358979323846 / (sin(3.14159265358979323846 * x) * gmb_sp2_gam(1.0 - x));
    return exp(gmb_sp2_lgam(x));
}
double gmb_special_hypergeom_2f1(double a, double b, double c, double x)
{
    double s = 1.0, term = 1.0;
    int n;
    if (c == 0.0 || (c == floor(c) && c <= 0.0)) return NAN;
    if (x < -1.0) {
        double y = x / (x - 1.0);
        return pow(1.0 - x, -a) * gmb_special_hypergeom_2f1(a, c - b, c, y);
    }
    if (x < 0.0 || x < 1.0) {
        for (n = 0; n < 10000; n++) {
            term *= (a + n) * (b + n) / ((c + n) * (n + 1.0)) * x;
            s += term;
            if (fabs(term) < 1e-16 * fabs(s)) break;
        }
        return s;
    }
    if (x == 1.0) return gmb_sp2_gam(c) * gmb_sp2_gam(c - a - b) / (gmb_sp2_gam(c - a) * gmb_sp2_gam(c - b));
    return NAN;
}
double gmb_special_hypergeom_1f1(double a, double b, double x)
{
    double s, term, u;
    int n;
    if (b == 0.0 || (b == floor(b) && b <= 0.0)) return NAN;
    if (a <= 0.0 && a == floor(a)) {
        s = 1.0; term = 1.0;
        for (n = 0; n <= (int)(-a); n++) {
            term *= (a + n) / ((b + n) * (n + 1.0)) * x;
            s += term;
            if (!(fabs(s) < 1e300)) return NAN;
        }
        return s;
    }
    if (x > 15.0) {
        double lv, sign = (gmb_sp2_gam(a) < 0.0) ? -1.0 : 1.0;
        int k;
        if (fabs(a) > 60.0 || fabs(b) > 60.0 || x > 700.0) lv = 0.0;
        else lv = lgamma(b) - lgamma(a) + x + (a - b) * log(fabs(x));
        if (lv > 700.0) return sign * 1e308;
        if (lv < -700.0) return 0.0;
        s = 1.0; term = 1.0;
        for (k = 0; k < 12; k++) {
            term *= (b - a + k) * (1.0 - a + k) / (k + 1.0) / x;
            s += term;
        }
        return sign * exp(lv) * s;
    }
    if (x < -20.0) {
        double lv, sign = ((gmb_sp2_gam(b) < 0.0) ? -1.0 : 1.0) * ((gmb_sp2_gam(b - a) < 0.0) ? -1.0 : 1.0);
        double best;
        int cap;
        u = -x;
        if (fabs(a) > 60.0 || fabs(b - a) > 60.0) lv = 0.0;
        else lv = lgamma(b) - lgamma(b - a) - a * log(u);
        if (lv > 700.0) return sign * 1e308;
        if (lv < -700.0) return 0.0;
        cap = (int)u + 8;
        if (cap < 16) cap = 16;
        if (cap > 300) cap = 300;
        s = 1.0; term = 1.0; best = 1.0;
        {
            double minterm = 1.0;
            for (n = 1; n <= cap; n++) {
                term *= (a + (n - 1)) * (a - b + n) / n * (-1.0 / u);
                s += term;
                if (fabs(term) < minterm) { minterm = fabs(term); best = s; }
            }
        }
        return sign * exp(lv) * best;
    }
    s = 1.0; term = 1.0;
    for (n = 0; n < 100000; n++) {
        term *= (a + n) / ((b + n) * (n + 1.0)) * x;
        s += term;
        if (!(fabs(s) < 1e300)) return NAN;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}
static const double gmb_sp2_bern[] = {
    1.0 / 6.0, -1.0 / 30.0, 1.0 / 42.0, -1.0 / 30.0, 5.0 / 66.0,
    -691.0 / 2730.0, 7.0 / 6.0, -3617.0 / 510.0, 43867.0 / 798.0, -174611.0 / 330.0
};
static double gmb_sp2_zeta_em(double s, double a)
{
    int N = 20, M = 10, k, j;
    double sum = 0.0, Np;
    for (k = 0; k < N; k++) sum += pow(k + a, -s);
    Np = N + a;
    sum += pow(Np, 1.0 - s) / (s - 1.0) + 0.5 * pow(Np, -s);
    for (j = 1; j <= M; j++) {
        double fact = 1.0, term, rising = 1.0;
        int m2 = 2 * j;
        for (k = 1; k <= m2; k++) fact *= (double)k;
        for (k = 0; k < m2 - 1; k++) rising *= (s + k);
        term = gmb_sp2_bern[j - 1] / fact * rising * pow(Np, -s - m2 + 1.0);
        sum += term;
    }
    return sum;
}
double gmb_special_zeta(double s)
{
    if (s == 1.0) return INFINITY;
    if (s > 1.0) return gmb_sp2_zeta_em(s, 1.0);
    return pow(2.0, s) * pow(3.14159265358979323846, s - 1.0) * sin(3.14159265358979323846 * s / 2.0) * gmb_sp2_gam(1.0 - s) * gmb_special_zeta(1.0 - s);
}
double gmb_special_zeta_hurwitz(double s, double a)
{
    if (a <= 0.0 && a == floor(a)) return INFINITY;
    return gmb_sp2_zeta_em(s, a);
}
double gmb_special_polygamma(double n_in, double x)
{
    double r = 0.0;
    int i, n = (int)n_in;
    if (n < 0 || x <= 0.0) return NAN;
    if (n == 0) {
        double res = 0.0;
        while (x < 6.0) { res -= 1.0 / x; x += 1.0; }
        {
            double inv = 1.0 / x, inv2 = inv * inv, p = inv2;
            res += log(x) - 0.5 * inv;
            for (i = 1; i <= 10; i++) { res -= gmb_sp2_bern[i - 1] / (2 * i) * p; p *= inv2; }
        }
        return res;
    }
    {
        double fact = 1.0, factn1 = 1.0, res;
        int sign = (n % 2 == 1) ? 1 : -1;
        for (i = 1; i <= n; i++) fact *= (double)i;
        for (i = 1; i <= n - 1; i++) factn1 *= (double)i;
        while (x < 10.0) { r += (double)sign * fact / pow(x, (double)(n + 1)); x += 1.0; }
        res = factn1 / pow(x, (double)n) + 0.5 * fact / pow(x, (double)(n + 1));
        for (i = 1; i <= 10; i++) {
            int m, k2 = 2 * i;
            double ratio = 1.0;
            for (m = k2 + 1; m <= k2 + n - 1; m++) ratio *= (double)m;
            res += gmb_sp2_bern[i - 1] * ratio / pow(x, (double)(k2 + n));
        }
        r += ((n % 2 == 1) ? 1.0 : -1.0) * res;
    }
    return r;
}
double gmb_special_lambert_w(double x)
{
    double w;
    int i;
    if (x < -1.0 / 2.71828182845904523536) return NAN;
    if (x == 0.0) return 0.0;
    if (x == -1.0 / 2.71828182845904523536) return -1.0;
    if (x > 1.0) w = log(x) - log(log(x) + 1e-300);
    else w = x;
    for (i = 0; i < 100; i++) {
        double ew = exp(w), f = w * ew - x;
        double wp1 = w + 1.0;
        double d = ew * wp1 - (w + 2.0) * f / (2.0 * wp1);
        double dw = f / d;
        w -= dw;
        if (fabs(dw) < 1e-16 * (1.0 + fabs(w))) break;
    }
    return w;
}
double gmb_special_mittag_leffler(double alpha, double beta, double x)
{
    double s = 0.0;
    int k;
    for (k = 0; k < 2000; k++) {
        double term = pow(x, (double)k) / gmb_sp2_gam(alpha * k + beta);
        s += term;
        if (k > 0 && fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}
double gmb_special_struve_h(double v, double x)
{
    double s = 0.0, h = x / 2.0;
    int k;
    for (k = 0; k < 500; k++) {
        double term = pow(h, 2.0 * k + v + 1.0) / (gmb_sp2_gam(k + 1.5) * gmb_sp2_gam(k + v + 1.5));
        if (k % 2) term = -term;
        s += term;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}
double gmb_special_kelvin_ber(double x)
{
    double s = 0.0, h = x / 2.0;
    int k;
    for (k = 0; k < 200; k++) {
        double term = pow(h, 4.0 * k) / (gmb_sp2_gam(2.0 * k + 1.0) * gmb_sp2_gam(2.0 * k + 1.0));
        if (k % 2) term = -term;
        s += term;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}
double gmb_special_kelvin_bei(double x)
{
    double s = 0.0, h = x / 2.0;
    int k;
    for (k = 0; k < 200; k++) {
        double term = pow(h, 4.0 * k + 2.0) / (gmb_sp2_gam(2.0 * k + 2.0) * gmb_sp2_gam(2.0 * k + 2.0));
        if (k % 2) term = -term;
        s += term;
        if (fabs(term) < 1e-16 * fabs(s)) break;
    }
    return s;
}
double gmb_special_parabolic_cylinder(double a, double x)
{
    double nu = -a - 0.5;
    double t1 = sqrt(3.14159265358979323846) / gmb_sp2_gam(0.5 * (1.0 - nu)) * gmb_special_hypergeom_1f1(-0.5 * nu, 0.5, 0.5 * x * x);
    double t2 = sqrt(2.0 * 3.14159265358979323846) * x / gmb_sp2_gam(-0.5 * nu) * gmb_special_hypergeom_1f1(0.5 * (1.0 - nu), 1.5, 0.5 * x * x);
    return pow(2.0, 0.5 * nu) * exp(-0.25 * x * x) * (t1 - t2);
}

/* --- Random Matrix Theory (MULTIVARIATE) --- */
static void gmb_rmt_jacobi(int n, double* A, double* eval, double* evec)
{
    int i, j, k, sweep;
    memset(evec, 0, (size_t)n * n * sizeof(double));
    for (i = 0; i < n; i++) evec[i * n + i] = 1.0;
    for (sweep = 0; sweep < 100; sweep++) {
        double off = 0.0, diag = 0.0;
        for (i = 0; i < n; i++) {
            diag += A[i*n+i] * A[i*n+i];
            for (j = i + 1; j < n; j++) off += A[i*n+j] * A[i*n+j];
        }
        if (off <= 1e-30 * (diag + 1e-300)) break;
        for (i = 0; i < n; i++)
            for (j = i + 1; j < n; j++) {
                double apq = A[i*n+j];
                if (fabs(apq) < 1e-300) continue;
                double app = A[i*n+i], aqq = A[j*n+j];
                double tau = (aqq - app) / (2.0 * apq);
                double t = (tau >= 0 ? 1.0 : -1.0) / (fabs(tau) + sqrt(1.0 + tau * tau));
                double c = 1.0 / sqrt(1.0 + t * t), sn = t * c;
                for (k = 0; k < n; k++) {
                    double aik = A[i*n+k], ajk = A[j*n+k];
                    A[i*n+k] = c * aik - sn * ajk;
                    A[j*n+k] = sn * aik + c * ajk;
                }
                for (k = 0; k < n; k++) {
                    double aki = A[k*n+i], akj = A[k*n+j];
                    A[k*n+i] = c * aki - sn * akj;
                    A[k*n+j] = sn * aki + c * akj;
                }
                for (k = 0; k < n; k++) {
                    double vik = evec[k*n+i], vjk = evec[k*n+j];
                    evec[k*n+i] = c * vik - sn * vjk;
                    evec[k*n+j] = sn * vik + c * vjk;
                }
                A[i*n+j] = 0.0; A[j*n+i] = 0.0;
            }
    }
    for (i = 0; i < n; i++) eval[i] = A[i*n+i];
}
double gmb_mv_rmt_lambda_plus(int n, int T, double sigma2)
{
    if (n < 1 || T < 1 || sigma2 < 0.0) return NAN;
    double r = sqrt((double)n / T);
    return sigma2 * (1.0 + r) * (1.0 + r);
}
double gmb_mv_rmt_lambda_minus(int n, int T, double sigma2)
{
    if (n < 1 || T < 1 || sigma2 < 0.0) return NAN;
    double r = sqrt((double)n / T);
    return sigma2 * (1.0 - r) * (1.0 - r);
}
double gmb_mv_rmt_coherence(const double* v, int n)
{
    double mx = 0.0;
    int i;
    if (!v || n < 1) return NAN;
    for (i = 0; i < n; i++) { double a = fabs(v[i]); if (a > mx) mx = a; }
    return (double)n * mx * mx;
}
double gmb_mv_rmt_spectral_norm(const double* A, int n)
{
    double *W, *eval, *evec, mx = 0.0;
    int i;
    if (!A || n < 1) return NAN;
    W = (double*)malloc((size_t)n * n * sizeof(double));
    eval = (double*)malloc((size_t)n * sizeof(double));
    evec = (double*)malloc((size_t)n * n * sizeof(double));
    if (!W || !eval || !evec) { free(W); free(eval); free(evec); return NAN; }
    memcpy(W, A, (size_t)n * n * sizeof(double));
    gmb_rmt_jacobi(n, W, eval, evec);
    for (i = 0; i < n; i++) { double a = fabs(eval[i]); if (a > mx) mx = a; }
    free(W); free(eval); free(evec);
    return mx;
}
double gmb_mv_rmt_davis_kahan(double e_norm, double gap)
{
    if (gap <= 0.0) return NAN;
    return fabs(e_norm) / gap;
}
double gmb_mv_rmt_principal_angle(const double* u, const double* v, int n)
{
    double nu = 0.0, nv = 0.0, dot = 0.0, c;
    int i;
    if (!u || !v || n < 1) return NAN;
    for (i = 0; i < n; i++) { nu += u[i]*u[i]; nv += v[i]*v[i]; dot += u[i]*v[i]; }
    if (nu <= 0.0 || nv <= 0.0) return NAN;
    c = fabs(dot) / sqrt(nu * nv);
    if (c > 1.0) c = 1.0;
    return acos(c);
}
double gmb_mv_rmt_tightness(double observed, double bound)
{
    if (bound <= 0.0) return 0.0;
    return fabs(observed) / bound;
}

/* --- Gaussian Process regression (MULTIVARIATE), port di gs_gp_regression --- */
static int gmb_mv_solve(int n, double* A, double* b, double* x)
{
    int i, j, k, piv;
    for (i = 0; i < n; i++) {
        piv = i;
        for (j = i + 1; j < n; j++) if (fabs(A[j*n+i]) > fabs(A[piv*n+i])) piv = j;
        if (fabs(A[piv*n+i]) < 1e-300) return 0;
        if (piv != i) {
            for (k = 0; k < n; k++) { double t = A[i*n+k]; A[i*n+k] = A[piv*n+k]; A[piv*n+k] = t; }
            { double t = b[i]; b[i] = b[piv]; b[piv] = t; }
        }
        for (j = i + 1; j < n; j++) {
            double f = A[j*n+i] / A[i*n+i];
            for (k = i; k < n; k++) A[j*n+k] -= f * A[i*n+k];
            b[j] -= f * b[i];
        }
    }
    for (i = n - 1; i >= 0; i--) {
        double s = b[i];
        for (j = i + 1; j < n; j++) s -= A[i*n+j] * x[j];
        x[i] = s / A[i*n+i];
    }
    return 1;
}
static gmb_val_t gmb_mv_nan_dict(void)
{
    gmb_dict_t d = gmb_dict_new();
    gmb_dict_set(&d, "Mean", gmb_make_val_double(NAN));
    gmb_dict_set(&d, "Variance", gmb_make_val_double(NAN));
    return gmb_make_val_dict_take(&d);
}
static int gmb_mv_gp_fill(const double* x, const double* y, int n,
                          const double* xs, int m, double l2, double noise,
                          double* mean, double* var)
{
    double *K, *alpha, *kx, *v, *A, *sol;
    int i, j, q;
    if (!x || !y || !xs || !mean || !var || n < 1 || m < 1 || l2 <= 0.0) return 0;
    K = (double*)malloc((size_t)n * n * sizeof(double));
    alpha = (double*)malloc((size_t)n * sizeof(double));
    kx = (double*)malloc((size_t)n * sizeof(double));
    v = (double*)malloc((size_t)n * sizeof(double));
    A = (double*)malloc((size_t)n * n * sizeof(double));
    sol = (double*)malloc((size_t)n * sizeof(double));
    if (!K || !alpha || !kx || !v || !A || !sol) {
        free(K); free(alpha); free(kx); free(v); free(A); free(sol);
        return 0;
    }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            double dd = x[i] - x[j];
            K[i*n+j] = exp(-0.5 * dd * dd / l2) + (i == j ? noise : 0.0);
        }
    for (i = 0; i < n; i++) alpha[i] = y[i];
    memcpy(A, K, (size_t)n * n * sizeof(double));
    if (!gmb_mv_solve(n, A, alpha, v)) {
        free(K); free(alpha); free(kx); free(v); free(A); free(sol);
        return 0;
    }
    memcpy(alpha, v, (size_t)n * sizeof(double));
    for (q = 0; q < m; q++) {
        double mu = 0.0, kv;
        for (i = 0; i < n; i++) { double dd = xs[q] - x[i]; kx[i] = exp(-0.5 * dd * dd / l2); mu += kx[i] * alpha[i]; }
        for (i = 0; i < n; i++) v[i] = kx[i];
        memcpy(A, K, (size_t)n * n * sizeof(double));
        if (!gmb_mv_solve(n, A, v, sol)) { mean[q] = mu; var[q] = noise; continue; }
        kv = 1.0 + noise;
        for (i = 0; i < n; i++) kv -= kx[i] * sol[i];
        mean[q] = mu;
        var[q] = kv > 0.0 ? kv : 0.0;
    }
    free(K); free(alpha); free(kx); free(v); free(A); free(sol);
    return 1;
}
gmb_val_t gmb_mv_gp_regress(const double* x, const double* y, int n,
                            const double* xs, int m, double l2, double noise)
{
    double* mean;
    double* var;
    gmb_dict_t d;
    if (!x || !y || !xs || n < 1 || m < 1 || l2 <= 0.0) return gmb_mv_nan_dict();
    mean = (double*)malloc((size_t)m * sizeof(double));
    var = (double*)malloc((size_t)m * sizeof(double));
    if (!mean || !var) { free(mean); free(var); return gmb_mv_nan_dict(); }
    if (!gmb_mv_gp_fill(x, y, n, xs, m, l2, noise, mean, var)) {
        free(mean); free(var);
        return gmb_mv_nan_dict();
    }
    d = gmb_dict_new();
    gmb_dict_set(&d, "Mean", gmb_make_val_list(gmb_list_from_doubles(mean, m)));
    gmb_dict_set(&d, "Variance", gmb_make_val_list(gmb_list_from_doubles(var, m)));
    free(mean); free(var);
    return gmb_make_val_dict_take(&d);
}

/* --- Bayesian optimization + gambler's ruin + causal inference (gs_math.c) --- */
static double gmb_bo_phi(double z) { return 0.3989422804014327 * exp(-0.5 * z * z); }
static double gmb_bo_Phi(double z) { return gmb_dist_normal_cdf(0.0, 1.0, z); }
double gmb_mv_bo_ei(double mu, double sd, double best, double xi)
{
    double d, z;
    if (sd <= 1e-300) { d = mu - best - xi; return d > 0.0 ? d : 0.0; }
    d = mu - best - xi;
    z = d / sd;
    return d * gmb_bo_Phi(z) + sd * gmb_bo_phi(z);
}
double gmb_mv_bo_pi(double mu, double sd, double best, double xi)
{
    if (sd <= 1e-300) return (mu - best - xi > 0.0) ? 1.0 : 0.0;
    return gmb_bo_Phi((mu - best - xi) / sd);
}
double gmb_mv_bo_ucb(double mu, double sd, double kappa) { return mu + kappa * sd; }
gmb_val_t gmb_mv_bo_next(const double* x, const double* y, int n,
                         const double* cand, int m, double l2, double noise, double kappa)
{
    double* mean;
    double* var;
    double bv = -1e300;
    int i, best = 0;
    gmb_dict_t d;
    if (!x || !y || !cand || n < 2 || m < 1) return gmb_mv_nan_dict();
    mean = (double*)malloc((size_t)m * sizeof(double));
    var = (double*)malloc((size_t)m * sizeof(double));
    if (!mean || !var) { free(mean); free(var); return gmb_mv_nan_dict(); }
    if (!gmb_mv_gp_fill(x, y, n, cand, m, l2, noise, mean, var)) {
        free(mean); free(var);
        return gmb_mv_nan_dict();
    }
    for (i = 0; i < m; i++) {
        double u = mean[i] + kappa * sqrt(var[i] > 0.0 ? var[i] : 0.0);
        if (u > bv) { bv = u; best = i; }
    }
    d = gmb_dict_new();
    gmb_dict_set(&d, "Index", gmb_make_val_double(best));
    gmb_dict_set(&d, "Value", gmb_make_val_double(bv));
    free(mean); free(var);
    return gmb_make_val_dict_take(&d);
}
double gmb_mv_gambler_prob(double i, double N, double p)
{
    double q, r;
    if (N <= 0.0 || i < 0.0 || i > N || p < 0.0 || p > 1.0) return NAN;
    q = 1.0 - p;
    if (i == 0.0) return 1.0;
    if (i == N) return 0.0;
    if (fabs(p - 0.5) < 1e-12) return 1.0 - i / N;
    r = q / p;
    return (pow(r, i) - pow(r, N)) / (1.0 - pow(r, N));
}
double gmb_mv_gambler_duration(double i, double N, double p)
{
    double q, r;
    if (N <= 0.0 || i < 0.0 || i > N || p < 0.0 || p > 1.0) return NAN;
    q = 1.0 - p;
    if (fabs(p - 0.5) < 1e-12) return i * (N - i);
    r = q / p;
    return i / (q - p) - (N / (q - p)) * (1.0 - pow(r, i)) / (1.0 - pow(r, N));
}
double gmb_mv_ate_ipw(const double* t, const double* y, const double* ps, int n)
{
    int i;
    double s1 = 0.0, s0 = 0.0;
    if (!t || !y || !ps || n < 1) return NAN;
    for (i = 0; i < n; i++) {
        double e = ps[i] < 1e-6 ? 1e-6 : (ps[i] > 1.0 - 1e-6 ? 1.0 - 1e-6 : ps[i]);
        s1 += t[i] * y[i] / e;
        s0 += (1.0 - t[i]) * y[i] / (1.0 - e);
    }
    return s1 / n - s0 / n;
}
double gmb_mv_ate_matching(const double* t, const double* y, const double* ps, int n)
{
    int i, j, nt = 0, nc = 0;
    double att = 0.0, atc = 0.0;
    if (!t || !y || !ps || n < 2) return NAN;
    for (i = 0; i < n; i++) {
        int best = -1;
        double bd = 1e300;
        for (j = 0; j < n; j++) {
            if (t[j] == t[i]) continue;
            { double dd = fabs(ps[i] - ps[j]); if (dd < bd) { bd = dd; best = j; } }
        }
        if (best < 0) continue;
        if (t[i] > 0.5) { att += y[i] - y[best]; nt++; }
        else { atc += y[best] - y[i]; nc++; }
    }
    att = nt > 0 ? att / nt : 0.0;
    atc = nc > 0 ? atc / nc : 0.0;
    if (nt > 0 && nc > 0) return 0.5 * (att + atc);
    return nt > 0 ? att : atc;
}

/* --- SVM multiclasse (perceptron, port di gs_svm_multiclass) --- */
void gmb_mv_svm_fit(const double* X, const double* y, int n, int d, int k,
                    double C, int max_iter, double* W)
{
    double eta = C / n;
    int iter, i, j, c, changed = 1;
    for (i = 0; i < k * d; i++) W[i] = 0.0;
    for (iter = 0; iter < max_iter && changed; iter++) {
        changed = 0;
        for (i = 0; i < n; i++) {
            const double* xi = X + (size_t)i * d;
            int yi = (int)y[i];
            for (c = 0; c < k; c++) {
                int lbl = (c == yi) ? 1 : -1;
                double margin = 0.0;
                for (j = 0; j < d; j++) margin += W[c*d+j] * xi[j];
                if (lbl * margin < 1.0) {
                    for (j = 0; j < d; j++) W[c*d+j] += eta * lbl * xi[j];
                    changed = 1;
                }
            }
        }
    }
}
int gmb_mv_svm_predict(const double* W, const double* x, int d, int k)
{
    int c, best = 0, j;
    double bs = -1e300;
    for (c = 0; c < k; c++) {
        double s = 0.0;
        for (j = 0; j < d; j++) s += W[c*d+j] * x[j];
        if (s > bs) { bs = s; best = c; }
    }
    return best;
}
gmb_val_t gmb_mv_svm_fit_dict(const double* X, const double* y, int n, int d, int k,
                              double C, int max_iter)
{
    double* W;
    gmb_dict_t dict;
    gmb_val_t wv;
    if (!X || !y || n < 1 || d < 1 || k < 2 || C <= 0.0 || max_iter < 1) return gmb_mv_nan_dict();
    W = (double*)calloc((size_t)k * d, sizeof(double));
    if (!W) return gmb_mv_nan_dict();
    gmb_mv_svm_fit(X, y, n, d, k, C, max_iter, W);
    wv = gmb_make_val_list(gmb_list_from_doubles(W, k * d));
    free(W);
    dict = gmb_dict_new();
    gmb_dict_set(&dict, "W", wv);
    gmb_dict_set(&dict, "Classes", gmb_make_val_double(k));
    gmb_dict_set(&dict, "Features", gmb_make_val_double(d));
    return gmb_make_val_dict_take(&dict);
}

/* --- GMM EM 1D (port di gs_gmm_em_1d) --- */
gmb_val_t gmb_mv_gmm_em_1d(const double* x, int n, int k, int iters)
{
    double *mu, *var, *w, *gam, gmin, gmax, gmean = 0.0, gvar = 0.0;
    int i, j, it;
    gmb_dict_t d;
    gmb_val_t mv, vv, wv;
    if (!x || n < 2 || k < 1) return gmb_mv_nan_dict();
    mu = (double*)malloc((size_t)k * sizeof(double));
    var = (double*)malloc((size_t)k * sizeof(double));
    w = (double*)malloc((size_t)k * sizeof(double));
    gam = (double*)malloc((size_t)n * k * sizeof(double));
    if (!mu || !var || !w || !gam) { free(mu); free(var); free(w); free(gam); return gmb_mv_nan_dict(); }
    gmin = gmax = x[0];
    for (i = 0; i < n; i++) { gmean += x[i]; if (x[i] < gmin) gmin = x[i]; if (x[i] > gmax) gmax = x[i]; }
    gmean /= n;
    for (i = 0; i < n; i++) gvar += (x[i]-gmean)*(x[i]-gmean);
    gvar /= n;
    if (gvar <= 0.0) gvar = 1e-6;
    for (j = 0; j < k; j++) { mu[j] = gmin + (gmax-gmin)*(j+0.5)/k; var[j] = gvar; w[j] = 1.0/k; }
    for (it = 0; it < iters; it++) {
        for (i = 0; i < n; i++) {
            double tot = 0.0;
            for (j = 0; j < k; j++) {
                double dd = x[i]-mu[j];
                double g = w[j]/sqrt(2.0*3.141592653589793*var[j]) * exp(-0.5*dd*dd/var[j]);
                gam[i*k+j] = g; tot += g;
            }
            if (tot <= 0.0) tot = 1e-300;
            for (j = 0; j < k; j++) gam[i*k+j] /= tot;
        }
        for (j = 0; j < k; j++) {
            double nk = 0, mk = 0, vk = 0;
            for (i = 0; i < n; i++) nk += gam[i*k+j];
            if (nk < 1e-12) { w[j] = 1e-12; continue; }
            for (i = 0; i < n; i++) mk += gam[i*k+j]*x[i];
            mk /= nk;
            for (i = 0; i < n; i++) { double dd = x[i]-mk; vk += gam[i*k+j]*dd*dd; }
            vk /= nk;
            mu[j] = mk; var[j] = vk > 1e-12 ? vk : 1e-12; w[j] = nk/n;
        }
    }
    mv = gmb_make_val_list(gmb_list_from_doubles(mu, k));
    vv = gmb_make_val_list(gmb_list_from_doubles(var, k));
    wv = gmb_make_val_list(gmb_list_from_doubles(w, k));
    free(mu); free(var); free(w); free(gam);
    d = gmb_dict_new();
    gmb_dict_set(&d, "Mu", mv);
    gmb_dict_set(&d, "Variance", vv);
    gmb_dict_set(&d, "Weight", wv);
    return gmb_make_val_dict_take(&d);
}

/* --- FITTING: curve fitting (port di gs_fit.c) --- */
double gmb_fit_cheb_eval(double x, const double* coef, int m)
{
    double b1 = 0.0, b2 = 0.0;
    int j;
    for (j = m - 1; j >= 1; j--) {
        double b0 = 2.0 * x * b1 - b2 + coef[j];
        b2 = b1; b1 = b0;
    }
    return x * b1 - b2 + coef[0];
}
double gmb_fit_pade_eval(double x, const double* a, int na, const double* b, int nb)
{
    double num = 0.0, den = 0.0, xp = 1.0;
    int i;
    for (i = 0; i < na; i++) { num += a[i] * xp; xp *= x; }
    xp = 1.0;
    for (i = 0; i <= nb; i++) { den += b[i] * xp; xp *= x; }
    return den != 0.0 ? num / den : 0.0;
}
double gmb_fit_poly_eval(double x, const double* coef, int m)
{
    double s = 0.0;
    for (int i = m; i >= 0; i--) s = s * x + coef[i];
    return s;
}
double gmb_fit_poly_ls(const double* x, const double* y, int n, int m, double* coef)
{
    int i, j, k, p = m + 1;
    double* N;
    double* r;
    double ssq = 0.0;
    if (!x || !y || !coef || n < 1 || m < 0 || p > 32) return 0.0;
    N = (double*)calloc((size_t)p * p, sizeof(double));
    r = (double*)calloc((size_t)p, sizeof(double));
    if (!N || !r) { free(N); free(r); return 0.0; }
    for (i = 0; i < n; i++) {
        for (j = 0; j < p; j++) {
            double xj = pow(x[i], (double)j);
            r[j] += xj * y[i];
            for (k = 0; k < p; k++) N[j*p+k] += xj * pow(x[i], (double)k);
        }
    }
    if (!gmb_mv_solve(p, N, r, coef)) { free(N); free(r); return 0.0; }
    for (i = 0; i < n; i++) {
        double e = y[i] - gmb_fit_poly_eval(x[i], coef, m);
        ssq += e * e;
    }
    free(N); free(r);
    return ssq;
}
void gmb_fit_rsquared(const double* data, int n, double* r2, double* angle)
{
    int i, k = 0;
    double Sx = 0, Sy = 0, Sxy = 0, Sx2 = 0, Sy2 = 0;
    if (r2) *r2 = 0;
    if (angle) *angle = 0;
    if (!data || n <= 2) return;
    for (i = 0; i < n; i++) {
        double v = data[i];
        if (v == 1.7976931348623157e308 || !isfinite(v)) continue;
        { double x = i + 1; Sx += x; Sy += v; Sxy += x * v; Sx2 += x * x; Sy2 += v * v; k++; }
    }
    if (k <= 2) return;
    {
        double nn = k;
        double Sx22 = Sx * Sx / nn, Sy22 = Sy * Sy / nn, SxSy = Sx * Sy / nn;
        double div = (Sx2 - Sx22) * (Sy2 - Sy22);
        if (fabs(div) < 2.220446049250313e-16) return;
        if (r2) *r2 = (Sxy - SxSy) * (Sxy - SxSy) / div;
        if (angle) *angle = (Sxy - SxSy) / (Sx2 - Sx22);
    }
}
double gmb_fit_rsquared_test(const double* data, int n)
{
    double r2 = 0, angle = 0;
    double weight;
    gmb_fit_rsquared(data, n, &r2, &angle);
    weight = 1.0 - 1.0 / sqrt((double)n + 1.0);
    if (angle < 0) return -fabs(r2) * weight;
    return r2 * weight;
}
static gmb_val_t gmb_fit_nan_dict(void)
{
    gmb_dict_t d = gmb_dict_new();
    gmb_dict_set(&d, "Coef", gmb_make_val_double(NAN));
    gmb_dict_set(&d, "Ssq", gmb_make_val_double(NAN));
    return gmb_make_val_dict_take(&d);
}
gmb_val_t gmb_fit_polyfit_dict(const double* x, const double* y, int n, int m)
{
    double* coef;
    double ssq;
    gmb_dict_t d;
    gmb_val_t cv;
    if (!x || !y || n < 1 || m < 0 || m > 31) return gmb_fit_nan_dict();
    coef = (double*)calloc((size_t)(m + 1), sizeof(double));
    if (!coef) return gmb_fit_nan_dict();
    ssq = gmb_fit_poly_ls(x, y, n, m, coef);
    cv = gmb_make_val_list(gmb_list_from_doubles(coef, m + 1));
    free(coef);
    d = gmb_dict_new();
    gmb_dict_set(&d, "Coef", cv);
    gmb_dict_set(&d, "Ssq", gmb_make_val_double(ssq));
    return gmb_make_val_dict_take(&d);
}
gmb_val_t gmb_fit_rsquared_dict(const double* data, int n)
{
    double r2 = 0, angle = 0;
    gmb_dict_t d = gmb_dict_new();
    gmb_fit_rsquared(data, n, &r2, &angle);
    gmb_dict_set(&d, "R2", gmb_make_val_double(r2));
    gmb_dict_set(&d, "Angle", gmb_make_val_double(angle));
    return gmb_make_val_dict_take(&d);
}

/* --- SIMULATION: RNG deterministico (SplitMix64) condiviso fra le 4 gambe --- */
#include <stdint.h>
static uint64_t g_sim_state = 0x9E3779B97F4A7C15ULL;
void gmb_sim_seed(uint64_t seed) { g_sim_state = seed ? seed : 0x9E3779B97F4A7C15ULL; }
static uint64_t gmb_sim_next_u64(void)
{
    g_sim_state += 0x9E3779B97F4A7C15ULL;
    uint64_t z = g_sim_state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static double gmb_sim_next_double(void)
{
    return (double)(gmb_sim_next_u64() >> 11) * (1.0 / 9007199254740992.0);
}
double gmb_sim_uniform(void) { return gmb_sim_next_double(); }
double gmb_sim_normal(void)
{
    double u, v, s;
    do {
        u = 2.0 * gmb_sim_next_double() - 1.0;
        v = 2.0 * gmb_sim_next_double() - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);
    return u * sqrt(-2.0 * log(s) / s);
}
double gmb_sim_exponential(double rate)
{
    if (rate <= 0.0) return NAN;
    return -log(1.0 - gmb_sim_next_double()) / rate;
}
double gmb_sim_bernoulli(double p)
{
    return gmb_sim_next_double() < p ? 1.0 : 0.0;
}
double gmb_sim_binomial(double n_in, double p)
{
    int i, n = (int)n_in;
    double c = 0.0;
    if (n < 0) return NAN;
    for (i = 0; i < n; i++) if (gmb_sim_next_double() < p) c += 1.0;
    return c;
}

/* --- Particle filter bootstrap (x_t=a*x_{t-1}+N(0,q); y_t=x_t+N(0,r)) --- */
gmb_val_t gmb_sim_particle_filter(const double* obs, int T, double a, double q, double r,
                                  int N, double seed)
{
    double *part, *w, *cdf, *mean, *var, m, v;
    gmb_dict_t d;
    int t, i;
    gmb_sim_seed((unsigned long long)seed);
    if (!obs || T < 1 || N < 2) return gmb_mv_nan_dict();
    part = (double*)malloc((size_t)N * sizeof(double));
    w = (double*)malloc((size_t)N * sizeof(double));
    cdf = (double*)malloc((size_t)N * sizeof(double));
    mean = (double*)malloc((size_t)T * sizeof(double));
    var = (double*)malloc((size_t)T * sizeof(double));
    if (!part || !w || !cdf || !mean || !var) {
        free(part); free(w); free(cdf); free(mean); free(var);
        return gmb_mv_nan_dict();
    }
    for (i = 0; i < N; i++) part[i] = obs[0] + sqrt(r) * gmb_sim_normal();
    for (t = 0; t < T; t++) {
        double wsum = 0.0;
        for (i = 0; i < N; i++) {
            if (t > 0) part[i] = a * part[i] + sqrt(q) * gmb_sim_normal();
            { double dd = obs[t] - part[i]; w[i] = exp(-0.5 * dd * dd / r); }
            wsum += w[i];
        }
        if (wsum <= 0.0) wsum = 1e-300;
        for (i = 0; i < N; i++) w[i] /= wsum;
        m = 0.0; for (i = 0; i < N; i++) m += w[i] * part[i];
        v = 0.0; for (i = 0; i < N; i++) { double dd = part[i] - m; v += w[i] * dd * dd; }
        mean[t] = m; var[t] = v;
        {
            double acc = 0.0;
            for (i = 0; i < N; i++) { acc += w[i]; cdf[i] = acc; }
            if (acc > 0.0) for (i = 0; i < N; i++) cdf[i] /= acc;
        }
        {
            double* np = (double*)malloc((size_t)N * sizeof(double));
            if (np) {
                for (i = 0; i < N; i++) {
                    double u = gmb_sim_uniform();
                    int lo = 0, hi = N - 1, mid;
                    while (lo < hi) { mid = (lo + hi) / 2; if (cdf[mid] < u) lo = mid + 1; else hi = mid; }
                    np[i] = part[lo];
                }
                memcpy(part, np, (size_t)N * sizeof(double));
                free(np);
            }
        }
    }
    d = gmb_dict_new();
    gmb_dict_set(&d, "Mean", gmb_make_val_list(gmb_list_from_doubles(mean, T)));
    gmb_dict_set(&d, "Variance", gmb_make_val_list(gmb_list_from_doubles(var, T)));
    free(part); free(w); free(cdf); free(mean); free(var);
    return gmb_make_val_dict_take(&d);
}

/* --- Gradient boosting (port di gs_boost.c) con modello serializzato --- */
typedef struct gmb_bnode {
    int feat; double thr, leaf;
    struct gmb_bnode *l, *r;
} gmb_bnode;

static double gmb_b_mean(const double* y, const int* idx, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < n; i++) s += y[idx[i]];
    return n > 0 ? s / (double)n : 0.0;
}
static int gmb_b_best_split(const double* X, const double* y, const int* idx, int n, int p,
                            int* bf, double* bt, double* bgain)
{
    double s = 0, ss = 0, sse_p, best = -1, bt_ = 0;
    int bf_ = -1, f, i, j;
    for (i = 0; i < n; i++) { double v = y[idx[i]]; s += v; ss += v * v; }
    sse_p = ss - s * s / n;
    for (f = 0; f < p; f++) {
        for (i = 0; i < n; i++) {
            double thr = X[(size_t)idx[i] * p + f];
            double sl = 0, sll = 0, sr = 0, srr = 0;
            int nl = 0, nr = 0;
            for (j = 0; j < n; j++) {
                double v = X[(size_t)idx[j] * p + f], yy = y[idx[j]];
                if (v <= thr) { sl += yy; sll += yy * yy; nl++; }
                else { sr += yy; srr += yy * yy; nr++; }
            }
            if (nl < 1 || nr < 1) continue;
            {
                double sse = (sll - sl * sl / nl) + (srr - sr * sr / nr);
                double gain = sse_p - sse;
                if (gain > best) { best = gain; bf_ = f; bt_ = thr; }
            }
        }
    }
    *bf = bf_; *bt = bt_; *bgain = best;
    return bf_ >= 0;
}
static void gmb_b_partition(const double* X, const int* idx, int n, int p, int f, double t,
                            int* li, int* nl, int* ri, int* nr)
{
    int j;
    *nl = 0; *nr = 0;
    for (j = 0; j < n; j++) {
        if (X[(size_t)idx[j] * p + f] <= t) li[(*nl)++] = idx[j];
        else ri[(*nr)++] = idx[j];
    }
}
static gmb_bnode* gmb_b_node(const double* y, const int* idx, int n)
{
    gmb_bnode* nd = (gmb_bnode*)calloc(1, sizeof(gmb_bnode));
    nd->feat = -1; nd->leaf = gmb_b_mean(y, idx, n);
    return nd;
}
static gmb_bnode* gmb_b_depthwise(const double* X, const double* y, const int* idx, int n, int p,
                                  int depth, int maxd, int minleaf)
{
    gmb_bnode* nd = gmb_b_node(y, idx, n);
    int bf, nl, nr; double bt, g;
    int *li, *ri;
    if (depth >= maxd || n < 2 * minleaf) return nd;
    if (!gmb_b_best_split(X, y, idx, n, p, &bf, &bt, &g) || g <= 0.0) return nd;
    li = (int*)malloc((size_t)n * sizeof(int));
    ri = (int*)malloc((size_t)n * sizeof(int));
    gmb_b_partition(X, idx, n, p, bf, bt, li, &nl, ri, &nr);
    nd->feat = bf; nd->thr = bt;
    nd->l = gmb_b_depthwise(X, y, li, nl, p, depth + 1, maxd, minleaf);
    nd->r = gmb_b_depthwise(X, y, ri, nr, p, depth + 1, maxd, minleaf);
    free(li); free(ri);
    return nd;
}
typedef struct { gmb_bnode* nd; int* idx; int n; int depth; } gmb_bleaf;
static gmb_bnode* gmb_b_leafwise(const double* X, const double* y, const int* idx0, int n0, int p,
                                 int maxd, int minleaf)
{
    gmb_bnode* root = gmb_b_node(y, idx0, n0);
    int cap = 1 << (maxd + 1), nl = 1, max_nodes = (1 << maxd) - 1, nodes = 1;
    gmb_bleaf* lv = (gmb_bleaf*)malloc((size_t)cap * sizeof(gmb_bleaf));
    int* a = (int*)malloc((size_t)n0 * sizeof(int));
    int i;
    for (i = 0; i < n0; i++) a[i] = idx0[i];
    lv[0].nd = root; lv[0].idx = a; lv[0].n = n0; lv[0].depth = 0;
    while (nodes < max_nodes) {
        int best = -1, bbf = -1, k;
        double bbt = 0, bg = 0;
        for (k = 0; k < nl; k++) {
            int bf; double bt, g;
            if (lv[k].depth >= maxd || lv[k].n < 2 * minleaf) continue;
            if (gmb_b_best_split(X, y, lv[k].idx, lv[k].n, p, &bf, &bt, &g) && g > bg) { bg = g; best = k; bbf = bf; bbt = bt; }
        }
        if (best < 0 || bg <= 0) break;
        {
            gmb_bleaf sp = lv[best];
            int *li = (int*)malloc((size_t)sp.n * sizeof(int));
            int *ri = (int*)malloc((size_t)sp.n * sizeof(int));
            int nli, nri;
            gmb_b_partition(X, sp.idx, sp.n, p, bbf, bbt, li, &nli, ri, &nri);
            sp.nd->feat = bbf; sp.nd->thr = bbt;
            sp.nd->l = gmb_b_node(y, li, nli);
            sp.nd->r = gmb_b_node(y, ri, nri);
            lv[best].nd = sp.nd->l; lv[best].idx = li; lv[best].n = nli; lv[best].depth = sp.depth + 1;
            lv[nl].nd = sp.nd->r; lv[nl].idx = ri; lv[nl].n = nri; lv[nl].depth = sp.depth + 1; nl++;
            free(sp.idx);
            nodes++;
        }
    }
    for (i = 0; i < nl; i++) free(lv[i].idx);
    free(lv);
    return root;
}
static gmb_bnode* gmb_b_symmetric(const double* X, const double* y, const int* idx0, int n0, int p,
                                  int maxd, int minleaf)
{
    gmb_bnode* root = gmb_b_node(y, idx0, n0);
    int cap = 1 << (maxd + 1), nl = 1, depth, i;
    gmb_bleaf* lv = (gmb_bleaf*)malloc((size_t)cap * sizeof(gmb_bleaf));
    int* a = (int*)malloc((size_t)n0 * sizeof(int));
    for (i = 0; i < n0; i++) a[i] = idx0[i];
    lv[0].nd = root; lv[0].idx = a; lv[0].n = n0; lv[0].depth = 0;
    for (depth = 0; depth < maxd; depth++) {
        int tot = 0, c = 0, k, bf, ok;
        double bt, g;
        int* comb;
        gmb_bleaf* nxt;
        int nn = 0;
        for (k = 0; k < nl; k++) tot += lv[k].n;
        if (tot == 0) break;
        comb = (int*)malloc((size_t)tot * sizeof(int));
        for (k = 0; k < nl; k++) for (i = 0; i < lv[k].n; i++) comb[c++] = lv[k].idx[i];
        ok = (tot >= 2 * minleaf) && gmb_b_best_split(X, y, comb, tot, p, &bf, &bt, &g) && g > 0;
        free(comb);
        if (!ok) break;
        nxt = (gmb_bleaf*)malloc((size_t)cap * sizeof(gmb_bleaf));
        for (k = 0; k < nl; k++) {
            gmb_bleaf sp = lv[k];
            int *li = (int*)malloc((size_t)sp.n * sizeof(int));
            int *ri = (int*)malloc((size_t)sp.n * sizeof(int));
            int nli, nri;
            gmb_b_partition(X, sp.idx, sp.n, p, bf, bt, li, &nli, ri, &nri);
            sp.nd->feat = bf; sp.nd->thr = bt;
            sp.nd->l = gmb_b_node(y, li, nli);
            sp.nd->r = gmb_b_node(y, ri, nri);
            nxt[nn].nd = sp.nd->l; nxt[nn].idx = li; nxt[nn].n = nli; nxt[nn].depth = depth + 1; nn++;
            nxt[nn].nd = sp.nd->r; nxt[nn].idx = ri; nxt[nn].n = nri; nxt[nn].depth = depth + 1; nn++;
            free(sp.idx);
        }
        free(lv); lv = nxt; nl = nn;
    }
    for (i = 0; i < nl; i++) free(lv[i].idx);
    free(lv);
    return root;
}
static gmb_bnode* gmb_b_fit_tree(int engine, const double* X, const double* y, const int* idx,
                                 int n, int p, int maxd, int minleaf)
{
    if (engine == 1) return gmb_b_leafwise(X, y, idx, n, p, maxd, minleaf);
    if (engine == 2) return gmb_b_symmetric(X, y, idx, n, p, maxd, minleaf);
    return gmb_b_depthwise(X, y, idx, n, p, 0, maxd, minleaf);
}
static double gmb_b_tree_predict(const gmb_bnode* n, const double* x)
{
    while (n->feat >= 0) n = (x[n->feat] <= n->thr) ? n->l : n->r;
    return n->leaf;
}
static void gmb_b_tree_free(gmb_bnode* n) { if (!n) return; gmb_b_tree_free(n->l); gmb_b_tree_free(n->r); free(n); }
static int gmb_b_count(const gmb_bnode* n) { return n ? 1 + gmb_b_count(n->l) + gmb_b_count(n->r) : 0; }
static int gmb_b_serialize(const gmb_bnode* n, double* out, int base, int* next)
{
    int me = (*next)++;
    out[(size_t)base + (size_t)me * 5 + 0] = n->feat;
    out[(size_t)base + (size_t)me * 5 + 1] = n->thr;
    out[(size_t)base + (size_t)me * 5 + 2] = n->leaf;
    if (n->feat >= 0) {
        int l = gmb_b_serialize(n->l, out, base, next);
        int r = gmb_b_serialize(n->r, out, base, next);
        out[(size_t)base + (size_t)me * 5 + 3] = l;
        out[(size_t)base + (size_t)me * 5 + 4] = r;
    } else {
        out[(size_t)base + (size_t)me * 5 + 3] = -1;
        out[(size_t)base + (size_t)me * 5 + 4] = -1;
    }
    return me;
}
double* gmb_mv_boost_fit_alloc(const double* X, int n, int p, const double* y,
                               int engine, int n_est, double lr, int max_depth, int* outn)
{
    double *f, *r, *model;
    int* idx;
    gmb_bnode** trees;
    int t, i, n_trees = 0, pos;
    if (outn) *outn = 0;
    if (!X || !y || n <= 1 || p <= 0 || engine < 0 || engine > 2 || n_est <= 0 || max_depth < 1 || max_depth > 20) return NULL;
    if (!(lr > 0.0 && lr <= 1.0)) lr = 0.1;
    f = (double*)malloc((size_t)n * sizeof(double));
    r = (double*)malloc((size_t)n * sizeof(double));
    idx = (int*)malloc((size_t)n * sizeof(int));
    trees = (gmb_bnode**)malloc((size_t)n_est * sizeof(gmb_bnode*));
    if (!f || !r || !idx || !trees) { free(f); free(r); free(idx); free(trees); return NULL; }
    {
        double s = 0;
        for (i = 0; i < n; i++) s += y[i];
        s /= n;
        for (i = 0; i < n; i++) { f[i] = s; idx[i] = i; }
        for (t = 0; t < n_est; t++) {
            gmb_bnode* tree;
            for (i = 0; i < n; i++) r[i] = y[i] - f[i];
            tree = gmb_b_fit_tree(engine, X, r, idx, n, p, max_depth, 1);
            if (!tree) break;
            trees[n_trees++] = tree;
            for (i = 0; i < n; i++) f[i] += lr * gmb_b_tree_predict(tree, X + (size_t)i * p);
        }
    }
    {
        int total = 4;
        for (t = 0; t < n_trees; t++) total += 1 + gmb_b_count(trees[t]) * 5;
        model = (double*)malloc((size_t)total * sizeof(double));
        if (!model) {
            for (t = 0; t < n_trees; t++) gmb_b_tree_free(trees[t]);
            free(trees); free(f); free(r); free(idx);
            return NULL;
        }
        model[0] = p; model[1] = n_trees; model[2] = lr;
        {
            double s = 0;
            for (i = 0; i < n; i++) s += y[i];
            model[3] = s / n;
        }
        pos = 4;
        for (t = 0; t < n_trees; t++) {
            int cnt = gmb_b_count(trees[t]);
            int next = 0;
            model[pos] = cnt;
            gmb_b_serialize(trees[t], model, pos + 1, &next);
            pos += 1 + cnt * 5;
        }
        *outn = total;
    }
    for (t = 0; t < n_trees; t++) gmb_b_tree_free(trees[t]);
    free(trees); free(f); free(r); free(idx);
    return model;
}
double gmb_mv_boost_predict(const double* model, int mlen, const double* x)
{
    int nf, nt, t, pos, node;
    double v, lr;
    if (!model || mlen < 4 || !x) return NAN;
    nf = (int)model[0]; nt = (int)model[1]; lr = model[2]; v = model[3];
    (void)nf;
    pos = 4;
    for (t = 0; t < nt; t++) {
        int cnt = (int)model[pos];
        int base = pos + 1;
        node = 0;
        while (base + node * 5 + 4 < mlen) {
            int feat = (int)model[base + node * 5 + 0];
            if (feat < 0) { v += lr * model[base + node * 5 + 2]; break; }
            if (x[feat] <= model[base + node * 5 + 1]) node = (int)model[base + node * 5 + 3];
            else node = (int)model[base + node * 5 + 4];
        }
        pos += 1 + cnt * 5;
    }
    return v;
}
double gmb_mv_boost_score(const double* model, int mlen, const double* X, int n, int p, const double* y)
{
    double ym = 0, sst = 0, sse = 0;
    int i;
    if (!model || !X || !y || n <= 0 || p <= 0) return NAN;
    for (i = 0; i < n; i++) ym += y[i];
    ym /= n;
    for (i = 0; i < n; i++) {
        double pr = gmb_mv_boost_predict(model, mlen, X + (size_t)i * p);
        sst += (y[i] - ym) * (y[i] - ym);
        sse += (y[i] - pr) * (y[i] - pr);
    }
    return sst > 0 ? 1.0 - sse / sst : 0.0;
}

/* --- TPE (Tree-structured Parzen Estimator), port di gs_tpe --- */
static int gmb_sim_int(int n)
{
    int v;
    if (n <= 1) return 0;
    v = (int)(gmb_sim_next_double() * n);
    if (v < 0) v = 0;
    if (v >= n) v = n - 1;
    return v;
}
typedef double (*gmb_tpe_fn_t)(const double* x, void* ctx);
static double gmb_tpe_logpdf_dim(double x, double mu, double sd)
{
    double d = x - mu;
    return -0.5 * d * d / (sd * sd) - log(sd);
}
double* gmb_sim_tpe_ctx(gmb_tpe_fn_t f, void* ctx, const double* lo, const double* hi, int d,
                        int n_iter, int n_init, double seed, gmb_int_t* outn)
{
    static const double LNORM = 0.91893853320467274178;
    double *X, *Y, *sigma, *cand, *bestx, *out;
    int *ord;
    int m, count = 0, i, k, it, best = 0;
    if (outn) *outn = 0;
    if (!f || !lo || !hi || !outn || d < 1 || n_init < 2 || n_iter < 1) return NULL;
    m = n_init + n_iter;
    X = (double*)malloc((size_t)m * d * sizeof(double));
    Y = (double*)malloc((size_t)m * sizeof(double));
    sigma = (double*)malloc((size_t)d * sizeof(double));
    cand = (double*)malloc((size_t)d * sizeof(double));
    bestx = (double*)malloc((size_t)d * sizeof(double));
    ord = (int*)malloc((size_t)m * sizeof(int));
    if (!X || !Y || !sigma || !cand || !bestx || !ord) {
        free(X); free(Y); free(sigma); free(cand); free(bestx); free(ord);
        return NULL;
    }
    gmb_sim_seed((unsigned long long)seed);
    for (count = 0; count < n_init; count++) {
        for (k = 0; k < d; k++) X[(size_t)count * d + k] = lo[k] + (hi[k] - lo[k]) * gmb_sim_uniform();
        Y[count] = f(&X[(size_t)count * d], ctx);
    }
    for (k = 0; k < d; k++) sigma[k] = 0.2 * (hi[k] - lo[k]);
    for (it = 0; it < n_iter; it++) {
        int n_good, g, c, nb;
        double best_ratio = -1e300;
        for (i = 0; i < count; i++) ord[i] = i;
        for (i = 0; i < count; i++)
            for (g = i + 1; g < count; g++)
                if (Y[ord[g]] < Y[ord[i]]) { int t = ord[i]; ord[i] = ord[g]; ord[g] = t; }
        n_good = (int)(0.25 * count);
        if (n_good < 1) n_good = 1;
        if (n_good >= count) n_good = count - 1;
        nb = count - n_good;
        for (c = 0; c < 24; c++) {
            int src = ord[gmb_sim_int(n_good)];
            double lg = 0.0, gb = 0.0, ratio;
            for (k = 0; k < d; k++) {
                double xc = X[(size_t)src * d + k] + sigma[k] * gmb_sim_normal();
                if (xc < lo[k]) xc = lo[k];
                if (xc > hi[k]) xc = hi[k];
                cand[k] = xc;
            }
            {
                double acc = 0.0;
                for (g = 0; g < n_good; g++) {
                    double lp = -LNORM;
                    int id = ord[g];
                    for (k = 0; k < d; k++) lp += gmb_tpe_logpdf_dim(cand[k], X[(size_t)id * d + k], sigma[k]);
                    acc += exp(lp);
                }
                lg = log(acc / n_good + 1e-300);
            }
            if (nb > 0) {
                double acc = 0.0;
                for (g = n_good; g < count; g++) {
                    double lp = -LNORM;
                    int id = ord[g];
                    for (k = 0; k < d; k++) lp += gmb_tpe_logpdf_dim(cand[k], X[(size_t)id * d + k], sigma[k]);
                    acc += exp(lp);
                }
                gb = log(acc / nb + 1e-300);
            } else gb = -1e300;
            ratio = lg - gb;
            if (ratio > best_ratio) { best_ratio = ratio; for (k = 0; k < d; k++) bestx[k] = cand[k]; }
        }
        for (k = 0; k < d; k++) X[(size_t)count * d + k] = bestx[k];
        Y[count] = f(bestx, ctx);
        count++;
    }
    best = 0;
    for (i = 1; i < count; i++) if (Y[i] < Y[best]) best = i;
    out = (double*)malloc((size_t)(d + 1) * sizeof(double));
    if (out) {
        for (k = 0; k < d; k++) out[k] = X[(size_t)best * d + k];
        out[d] = Y[best];
        *outn = d + 1;
    }
    free(X); free(Y); free(sigma); free(cand); free(bestx); free(ord);
    return out;
}
typedef struct { gmb_fn_t fn; int d; } gmb_tpe_c99_ctx_t;
static double gmb_tpe_c99_tramp(const double* x, void* ctx)
{
    gmb_tpe_c99_ctx_t* c = (gmb_tpe_c99_ctx_t*)ctx;
    gmb_int_t prev = gmb_lambda_bound(0);
    double r;
    gmb_lambda_bound_set(0, (gmb_int_t)c->d - 1);
    r = ((double(*)(void*, double*))c->fn.fn)(c->fn.ctx, (double*)x);
    gmb_lambda_bound_set(0, prev);
    return r;
}
double* gmb_sim_tpe_c99(gmb_fn_t fn, const double* lo, const double* hi, int d,
                        int n_iter, int n_init, double seed, gmb_int_t* outn)
{
    gmb_tpe_c99_ctx_t c;
    c.fn = fn;
    c.d = d;
    return gmb_sim_tpe_ctx(gmb_tpe_c99_tramp, &c, lo, hi, d, n_iter, n_init, seed, outn);
}

/* --- VECTOR: operazioni su handle (gm_vec) --- */
int64_t gmb_vector_op(int64_t ah, int64_t bh, double s, double t, int op)
{
    double a[4] = {0,0,0,0}, b[4] = {0,0,0,0}, r[4];
    int i;
    if (!gmb_vector_valid(ah)) return -1;
    for (i = 0; i < 4; i++) gmb_vector_get(ah, i, &a[i]);
    if (bh >= 0 && gmb_vector_valid(bh)) for (i = 0; i < 4; i++) gmb_vector_get(bh, i, &b[i]);
    for (i = 0; i < 4; i++) {
        double x = a[i], y = b[i];
        switch (op) {
        case 0: r[i] = x + y; break;
        case 1: r[i] = x - y; break;
        case 2: r[i] = x * y; break;
        case 3: r[i] = y != 0.0 ? x / y : 0.0; break;
        case 4: r[i] = x * s; break;
        case 6: r[i] = x + (y - x) * t; break;
        case 8: r[i] = x < y ? x : y; break;
        case 9: r[i] = x > y ? x : y; break;
        case 10: r[i] = fabs(x); break;
        default: r[i] = x; break;
        }
    }
    if (op == 5) {
        double n = sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2] + a[3]*a[3]);
        if (n > 0.0) for (i = 0; i < 4; i++) r[i] = a[i] / n;
        else for (i = 0; i < 4; i++) r[i] = a[i];
    }
    if (op == 7) {
        r[0] = a[1]*b[2] - a[2]*b[1];
        r[1] = a[2]*b[0] - a[0]*b[2];
        r[2] = a[0]*b[1] - a[1]*b[0];
        r[3] = 0.0;
    }
    return gmb_vector_create(r[0], r[1], r[2], r[3]);
}
double gmb_vector_angle_h(int64_t ah, int64_t bh)
{
    double a[4] = {0,0,0,0}, b[4] = {0,0,0,0}, dot = 0, na = 0, nb = 0, c;
    int i;
    if (!gmb_vector_valid(ah) || !gmb_vector_valid(bh)) return NAN;
    for (i = 0; i < 4; i++) { gmb_vector_get(ah, i, &a[i]); gmb_vector_get(bh, i, &b[i]); }
    for (i = 0; i < 4; i++) { dot += a[i]*b[i]; na += a[i]*a[i]; nb += b[i]*b[i]; }
    if (na <= 0.0 || nb <= 0.0) return NAN;
    c = fabs(dot) / sqrt(na * nb);
    if (c > 1.0) c = 1.0;
    return acos(c);
}
double gmb_vector_distance_h(int64_t ah, int64_t bh)
{
    double a[4] = {0,0,0,0}, b[4] = {0,0,0,0}, s = 0;
    int i;
    if (!gmb_vector_valid(ah) || !gmb_vector_valid(bh)) return NAN;
    for (i = 0; i < 4; i++) { gmb_vector_get(ah, i, &a[i]); gmb_vector_get(bh, i, &b[i]); }
    for (i = 0; i < 4; i++) { double d = a[i] - b[i]; s += d * d; }
    return sqrt(s);
}
