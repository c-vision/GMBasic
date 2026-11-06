/* layout_ext.c — test FFI del layout C reale degli UDT (S1, 2026-11-06).
 * Struct con campi a 1/2/4/8 byte, padding reale, campo stringa.
 * Compilata da build_testlibs.sh in liblayout_ext.dylib. */
#include <stdint.h>
#include <stddef.h>

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
