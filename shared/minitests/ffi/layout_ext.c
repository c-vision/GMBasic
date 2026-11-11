/* layout_ext.c — test FFI del layout C reale degli UDT (S1, 2026-11-06).
 * Struct con campi a 1/2/4/8 byte, padding reale, campo stringa.
 * Compilata da build_testlibs.sh in liblayout_ext.dylib. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* a@0 (4), padding 4, b@8 (8), c@16 (1), padding 7 → size 24 */
typedef struct { int32_t a; double b; uint8_t c; } Mixed;

/* x@0 (4), y@4 (2), padding 2, z@8 (8) → size 16 */
typedef struct { float x; int16_t y; int64_t z; } Fiz;

/* s@0 (8), n@8 (4), padding 4 → size 16 */
typedef struct { char *s; int64_t n; } Named;

int32_t mixed_size(void)   { return (int32_t)sizeof(Mixed); }
int32_t mixed_off_b(void)  { return (int32_t)offsetof(Mixed, b); }
int32_t mixed_off_c(void)  { return (int32_t)offsetof(Mixed, c); }
int32_t fiz_size(void)     { return (int32_t)sizeof(Fiz); }
int32_t named_size(void)   { return (int32_t)sizeof(Named); }

double mixed_sum(Mixed m)  { return (double)m.a + m.b + (double)m.c; }

Mixed mixed_bump(Mixed m)
{
    m.a += 1;
    m.b += 0.5;
    m.c += 1;
    return m;
}

double fiz_sum(Fiz f)      { return f.x + (double)f.y + (double)f.z; }

Fiz fiz_bump(Fiz f)
{
    f.x = (float)(f.x + 1.0f);
    f.y = (int16_t)(f.y + 2);
    f.z += 3;
    return f;
}

int64_t named_len(Named v) { return v.n + (v.s ? 1 : 0); }

Named named_make(const char *s, int64_t n)
{
    Named r;
    r.s = (char *)s;
    r.n = n;
    return r;
}

/* 12 campi (2026-11-06, S1): verifica il conteggio foglie u16 e il layout a >8 campi. */
typedef struct { int32_t f0,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11; } Big12;

int32_t big12_size(void) { return (int32_t)sizeof(Big12); }

double big12_sum(Big12 b)
{
    return (double)(b.f0+b.f1+b.f2+b.f3+b.f4+b.f5+b.f6+b.f7+b.f8+b.f9+b.f10+b.f11);
}

Big12 big12_inc(Big12 b)
{
    b.f0++; b.f1++; b.f2++; b.f3++; b.f4++; b.f5++;
    b.f6++; b.f7++; b.f8++; b.f9++; b.f10++; b.f11++;
    return b;
}

/* Campo STRING (2026-11-06, S1): char* → il nativo non trattiene il puntatore. */
typedef struct { char *s; int32_t n; } Named2;

int32_t named2_size(void) { return (int32_t)sizeof(Named2); }
int32_t named2_len(Named2 v) { return (int32_t)(v.s ? (int32_t)strlen(v.s) : 0) + v.n; }
Named2 named2_make(const char *s, int32_t n) { Named2 r; r.s = (char *)s; r.n = n; return r; }

/* Campi senza segno (2026-11-06): uint16/uint32/uint64. */
typedef struct { uint16_t a; uint32_t b; uint64_t c; } Wide;

int32_t wide_size(void) { return (int32_t)sizeof(Wide); }
double wide_sum(Wide w) { return (double)w.a + (double)w.b + (double)w.c; }
Wide wide_bump(Wide w) { w.a += 1; w.b += 2; w.c += 3; return w; }

/* Array di UDT (S3, 2026-11-06): n struct contigue (layout C). */
typedef struct { int32_t x; double y; } Pt2;

double pts_sum(Pt2 *a, int64_t n)
{
    double s = 0;
    for (int64_t i = 0; i < n; i++) s += (double)a[i].x + a[i].y;
    return s;
}

void pts_scale(Pt2 *a, int64_t n, double k)
{
    for (int64_t i = 0; i < n; i++) { a[i].x = (int32_t)(a[i].x * k); a[i].y *= k; }
}

/* Campo ARRAY dentro uno UDT (S3b, 2026-11-06): A(4) → 5 elementi int32 contigui. */
typedef struct { int32_t a[5]; double b; } Arr4;

int32_t arr4_size(void) { return (int32_t)sizeof(Arr4); }
double arr4_sum(Arr4 v)
{
    double s = v.b;
    for (int i = 0; i < 5; i++) s += (double)v.a[i];
    return s;
}
Arr4 arr4_bump(Arr4 v)
{
    for (int i = 0; i < 5; i++) v.a[i] += 1;
    v.b += 0.5;
    return v;
}

/* UDT con DUE campi ARRAY (S3b, 2026-11-06): A(4) → 5 int32, D(2) → 3 double. */
typedef struct { int32_t a[5]; double d[3]; double b; } ArrMix;

int32_t arrmix_size(void) { return (int32_t)sizeof(ArrMix); }
double arrmix_sum(ArrMix v)
{
    double s = v.b;
    for (int i = 0; i < 5; i++) s += (double)v.a[i];
    for (int i = 0; i < 3; i++) s += v.d[i];
    return s;
}
ArrMix arrmix_bump(ArrMix v)
{
    for (int i = 0; i < 5; i++) v.a[i] += 1;
    for (int i = 0; i < 3; i++) v.d[i] += 0.5;
    v.b += 10;
    return v;
}

/* Campi UNSIGNED di larghezza dichiarata (UINTEGER=uint16, ULONG=uint32, UQUAD=uint64). */
typedef struct { uint16_t u16; uint32_t u32; uint64_t u64; } UUns;

int32_t uuns_size(void) { return (int32_t)sizeof(UUns); }
double uuns_sum(UUns v) { return (double)v.u16 + (double)v.u32 + (double)v.u64; }

/* Callback con arità 6 (S4): oltre il vecchio limite 0-4. */
double cb6_eval(double (*f)(double, double, double, double, double, double),
                double a, double b, double c, double d, double e, double g)
{
    return f(a, b, c, d, e, g);
}

/* Callback arità 8 e callback intero arità 6 (S4). */
double cb8_eval(double (*f)(double, double, double, double, double, double, double, double),
                double a0, double a1, double a2, double a3, double a4, double a5, double a6, double a7)
{
    return f(a0, a1, a2, a3, a4, a5, a6, a7);
}
int64_t cb6_eval_i(int64_t (*f)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t),
                   int64_t a0, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5)
{
    return f(a0, a1, a2, a3, a4, a5);
}

/* Combinazione completa (S1+S3b+S0): struct annidata + campo ARRAY + larghezze miste. */
typedef struct { double x; double y; } PtD;
typedef struct { PtD p; int32_t k; double z; double a[3]; int64_t n; } Blob;

int32_t blob_size(void) { return (int32_t)sizeof(Blob); }
double blob_sum(Blob b)
{
    return b.p.x + b.p.y + (double)b.k + b.z + b.a[0] + b.a[1] + b.a[2] + (double)b.n;
}
Blob blob_bump(Blob b)
{
    b.p.x += 1; b.p.y += 2; b.k += 3; b.z += 4;
    for (int i = 0; i < 3; i++) b.a[i] += 5;
    b.n += 6;
    return b;
}

/* Array di UDT che a loro volta hanno campi ARRAY (S3 + S3b). */
double arr4_block_sum(Arr4* v, int64_t n)
{
    double s = 0;
    for (int64_t i = 0; i < n; i++) { s += v[i].b; for (int k = 0; k < 5; k++) s += (double)v[i].a[k]; }
    return s;
}
void arr4_block_bump(Arr4* v, int64_t n)
{
    for (int64_t i = 0; i < n; i++) { for (int k = 0; k < 5; k++) v[i].a[k] += 1; v[i].b += 1; }
}

/* Riempie un array di UDT con campi array (il BASIC non può scrivere v(i).A(k) su 3 gambe). */
void arr4_fill(Arr4* v, int64_t n)
{
    for (int64_t i = 0; i < n; i++) {
        for (int k = 0; k < 5; k++) v[i].a[k] = 0;
        v[i].a[0] = (int32_t)(1 + i);
        v[i].a[4] = (int32_t)(5 + i);
        v[i].b = 1;
    }
}

/* 16 campi con larghezze miste (matrice F5: struct >8 campi). */
typedef struct {
    int16_t a; int32_t b; double c; float d; int64_t e; uint8_t f; double g; int32_t h;
    double i; int16_t j; double k; int32_t l; double m; int64_t n; double o; float p;
} Wide16;

int32_t wide16_size(void) { return (int32_t)sizeof(Wide16); }
double wide16_sum(Wide16 w)
{
    return (double)w.a + (double)w.b + w.c + (double)w.d + (double)w.e + (double)w.f
         + w.g + (double)w.h + w.i + (double)w.j + w.k + (double)w.l + w.m
         + (double)w.n + w.o + (double)w.p;
}

/* UDT con campo STRING (char*) + campo ARRAY: marshalling misto, parametro e ritorno. */
typedef struct { char* name; int32_t v[3]; } NamedVec;

int32_t nv_size(void) { return (int32_t)sizeof(NamedVec); }
double nv_sum(NamedVec n)
{
    return (double)n.v[0] + (double)n.v[1] + (double)n.v[2]
         + (n.name && n.name[0] == 'x' ? 100.0 : 0.0);
}
NamedVec nv_make(void)
{
    static char b[] = "xyz";
    NamedVec r; r.name = b; r.v[0] = 1; r.v[1] = 2; r.v[2] = 3;
    return r;
}

/* UDT come argomento in POSIZIONE > 8 (matrice F5: i limiti `i < 8` dell'emitter). */
double udt_at9(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8, Arr4 v)
{
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + v.b + (double)v.a[0];
}

/* Parametri ARRAY e STRING in POSIZIONE >= 9 (stessa classe di bug dell'UDT in coda). */
double arr_at9(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
               double* buf, int64_t n)
{
    double s = a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
    for (int64_t i = 0; i < n; i++) s += buf[i];
    return s;
}
double str_at9(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8,
               const char* s)
{
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + (s && s[0] == 'x' ? 100.0 : 0.0);
}

/* 64 campi (cap dichiarato del layout UDT, F5 2026-11-07): il test più largo era 16.
 * 64 int32 = 256 byte; `wide64_sum` = somma dei valori 1..64 = 2080. */
typedef struct {
    int32_t f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15;
    int32_t f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31;
    int32_t f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47;
    int32_t f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63;
} Wide64;

int32_t wide64_size(void) { return (int32_t)sizeof(Wide64); }
double wide64_sum(Wide64 w)
{
    return (double)(w.f0 + w.f1 + w.f2 + w.f3 + w.f4 + w.f5 + w.f6 + w.f7
                  + w.f8 + w.f9 + w.f10 + w.f11 + w.f12 + w.f13 + w.f14 + w.f15
                  + w.f16 + w.f17 + w.f18 + w.f19 + w.f20 + w.f21 + w.f22 + w.f23
                  + w.f24 + w.f25 + w.f26 + w.f27 + w.f28 + w.f29 + w.f30 + w.f31
                  + w.f32 + w.f33 + w.f34 + w.f35 + w.f36 + w.f37 + w.f38 + w.f39
                  + w.f40 + w.f41 + w.f42 + w.f43 + w.f44 + w.f45 + w.f46 + w.f47
                  + w.f48 + w.f49 + w.f50 + w.f51 + w.f52 + w.f53 + w.f54 + w.f55
                  + w.f56 + w.f57 + w.f58 + w.f59 + w.f60 + w.f61 + w.f62 + w.f63);
}
Wide64 wide64_inc(Wide64 w)
{
    w.f0++; w.f1++; w.f2++; w.f3++; w.f4++; w.f5++; w.f6++; w.f7++;
    w.f8++; w.f9++; w.f10++; w.f11++; w.f12++; w.f13++; w.f14++; w.f15++;
    w.f16++; w.f17++; w.f18++; w.f19++; w.f20++; w.f21++; w.f22++; w.f23++;
    w.f24++; w.f25++; w.f26++; w.f27++; w.f28++; w.f29++; w.f30++; w.f31++;
    w.f32++; w.f33++; w.f34++; w.f35++; w.f36++; w.f37++; w.f38++; w.f39++;
    w.f40++; w.f41++; w.f42++; w.f43++; w.f44++; w.f45++; w.f46++; w.f47++;
    w.f48++; w.f49++; w.f50++; w.f51++; w.f52++; w.f53++; w.f54++; w.f55++;
    w.f56++; w.f57++; w.f58++; w.f59++; w.f60++; w.f61++; w.f62++; w.f63++;
    return w;
}

/* BYREF UDT (struct*): in/out di una struct per puntatore. */
typedef struct { double x; double y; } ByrefPt;

double byref_pt_sum(const ByrefPt* p) { return p->x + p->y; }
void byref_pt_scale(ByrefPt* p, double k) { p->x *= k; p->y *= k; }
void byref_pt_set(ByrefPt* p, double x, double y) { p->x = x; p->y = y; }

/* ── Callback multipli (2026-11-11) ────────────────────────────────────────────
 * Due/Tre callback nella STESSA firma e con la STESSA forma (kind, arità): dal 2026-11-11
 * non c'è più un pool a 2 trampolini per forma sui callback scalari (ffi_callback_two.bas,
 * ffi_callback_three.bas). */
double cb_apply2(double (*f)(double), double (*g)(double), double x)
{
    return f(x) + g(x);
}

double cb_apply3(double (*f)(double), double (*g)(double), double (*h)(double), double x)
{
    return f(x) + g(x) + h(x);
}

/* ── Array di STRING (char**), SOLO INPUT (2026-11-11) ────────────────────────
 * `a() AS STRING` passa al nativo un `const char**` di n stringhe UTF-8, valide
 * SOLO durante la chiamata (il nativo deve copiarle se vuole trattenerle). */
int64_t strs_total(const char** a, int64_t n)
{
    int64_t t = 0;
    for (int64_t i = 0; i < n; i++) t += a[i] ? (int64_t)strlen(a[i]) : 0;
    return t;
}

/* ── Callback con firma interna MISTA e arità > 8 (2026-11-11) ─────────────────
 * `int64_t (*)(int64_t, double)` e `double (*)(double ×9)`: il trampolino deve avere
 * ESATTAMENTE questa firma (kind per slot), non una firma omogenea. */
double cb_mixed(double (*f)(int64_t, double), int64_t n, double d)
{
    return f(n, d);
}

double cb_apply9(double (*f)(double, double, double, double, double, double, double, double, double), double x)
{
    return f(x, x, x, x, x, x, x, x, x);
}

/* Mixed callback DOUBLE/LONG/DOUBLE → DOUBLE (ordine dei registri FP/int alternato). */
double cb_mixed_q(double (*f)(double, int64_t, double), int64_t n, double d)
{
    return f(d, n, d);
}
