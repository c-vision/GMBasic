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
