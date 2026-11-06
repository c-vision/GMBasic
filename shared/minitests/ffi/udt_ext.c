/* udt_ext.c — libreria di test per l'estensione UDT dell'FFI (S0, 2026-11-06).
 * Struct a 3 e 8 campi, kind misto come parametro E come ritorno.
 * Compilata in libudt_ext.dylib accanto ai MiniTest (solo per test). */
#include <stdint.h>

/* 3 campi, kind misto: non è un HFA (c'è un intero) e supera i 16 byte → la ABI la
 * classifica in memoria. È il caso che il vecchio limite "1-2 foglie" rifiutava. */
typedef struct { double a; int64_t b; double c; } Mixed3;

/* 8 campi tutti double: 64 byte, sempre in memoria. */
typedef struct { double f0, f1, f2, f3, f4, f5, f6, f7; } Oct8;

/* 3 campi interi (kind uniforme ma oltre le 2 foglie). */
typedef struct { int64_t a, b, c; } Int3;

Mixed3 mix3_add(Mixed3 x, Mixed3 y)
{
    Mixed3 r;
    r.a = x.a + y.a;
    r.b = x.b + y.b;
    r.c = x.c + y.c;
    return r;
}

double mix3_sum(Mixed3 x)
{
    return x.a + (double)x.b + x.c;
}

Oct8 oct8_inc(Oct8 o)
{
    Oct8 r = o;
    r.f0 += 1; r.f1 += 2; r.f2 += 3; r.f3 += 4;
    r.f4 += 5; r.f5 += 6; r.f6 += 7; r.f7 += 8;
    return r;
}

double oct8_sum(Oct8 o)
{
    return o.f0 + o.f1 + o.f2 + o.f3 + o.f4 + o.f5 + o.f6 + o.f7;
}

int64_t int3_sum(Int3 v)
{
    return v.a + v.b + v.c;
}

Int3 int3_scale(Int3 v, int64_t k)
{
    Int3 r;
    r.a = v.a * k;
    r.b = v.b * k;
    r.c = v.c * k;
    return r;
}
