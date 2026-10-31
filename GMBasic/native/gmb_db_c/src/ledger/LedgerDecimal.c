/* FILE: native/gmb_db_c/src/ledger/LedgerDecimal.c
 * System.Decimal-compatible 96-bit decimal. Journal amounts, not IEEE double.
 */
#include "../../include/gmb_decimal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define GMB_DEC_SIGN  0x80000000u
#define GMB_DEC_SCALE_SHIFT 16
#define GMB_DEC_MAX_SCALE 28

static unsigned __int128 dec_mant(GmbDecimal d)
{
    return ((unsigned __int128)d.hi << 64) | ((unsigned __int128)d.mid << 32) | d.lo;
}

static GmbDecimal dec_pack(unsigned __int128 m, int scale, int neg)
{
    GmbDecimal d;
    const unsigned __int128 max96 = (((unsigned __int128)1) << 96) - 1;
    if (scale < 0) scale = 0;
    while (m > max96 && scale > 0) {
        m /= 10;
        scale--;
    }
    if (m > max96) m = max96;
    if (scale > GMB_DEC_MAX_SCALE) {
        while (scale > GMB_DEC_MAX_SCALE) {
            m /= 10;
            scale--;
        }
    }
    if (m == 0) neg = 0;
    d.lo = (uint32_t)m;
    d.mid = (uint32_t)(m >> 32);
    d.hi = (uint32_t)(m >> 64);
    d.flags = ((uint32_t)scale << GMB_DEC_SCALE_SHIFT) | (neg ? GMB_DEC_SIGN : 0);
    return d;
}

static int dec_scale(GmbDecimal d)
{
    return (int)((d.flags >> GMB_DEC_SCALE_SHIFT) & 0xFFu);
}

static unsigned __int128 dec_pow10(int n)
{
    unsigned __int128 p = 1;
    int i;
    for (i = 0; i < n; i++) p *= 10;
    return p;
}

static void dec_align(unsigned __int128* ma, int* sa, unsigned __int128* mb, int* sb)
{
    if (*sa < *sb) {
        int d = *sb - *sa;
        const unsigned __int128 max96 = (((unsigned __int128)1) << 96) - 1;
        while (d > 0 && *ma <= max96 / 10) {
            *ma *= 10;
            (*sa)++;
            d--;
        }
        if (d > 0) {
            *mb /= dec_pow10(d);
            *sb -= d;
        }
    } else if (*sb < *sa) {
        dec_align(mb, sb, ma, sa);
    }
}

GmbDecimal GmbDecimal_Zero(void)
{
    GmbDecimal d;
    memset(&d, 0, sizeof(d));
    return d;
}

GmbDecimal GmbDecimal_FromI64(int64_t v)
{
    int neg = 0;
    uint64_t u;
    if (v < 0) {
        neg = 1;
        u = (v == INT64_MIN) ? (uint64_t)INT64_MAX + 1u : (uint64_t)(-v);
    } else {
        u = (uint64_t)v;
    }
    return dec_pack(u, 0, neg);
}

GmbDecimal GmbDecimal_FromString(const char* s)
{
    int neg = 0, scale = 0, seenDot = 0, seenDigit = 0;
    unsigned __int128 m = 0;
    const unsigned __int128 max96 = (((unsigned __int128)1) << 96) - 1;
    if (!s) return GmbDecimal_Zero();
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '+' || *s == '-') {
        neg = (*s == '-');
        s++;
    }
    for (; *s; s++) {
        if (*s == '.' && !seenDot) {
            seenDot = 1;
            continue;
        }
        if (*s >= '0' && *s <= '9') {
            seenDigit = 1;
            if (m > max96 / 10) break;
            m = m * 10 + (unsigned)(*s - '0');
            if (seenDot) scale++;
            continue;
        }
        break;
    }
    if (!seenDigit) return GmbDecimal_Zero();
    return dec_pack(m, scale, neg);
}

GmbDecimal GmbDecimal_FromDouble(double v)
{
    char buf[64];
    if (v == 0.0) return GmbDecimal_Zero();
    snprintf(buf, sizeof(buf), "%.15g", v);
    return GmbDecimal_FromString(buf);
}

double GmbDecimal_ToDouble(GmbDecimal d)
{
    char buf[64];
    if (GmbDecimal_ToString(d, buf, (int)sizeof(buf)) != 0) return 0.0;
    return strtod(buf, NULL);
}

int GmbDecimal_ToString(GmbDecimal d, char* out, int cap)
{
    char digits[64];
    unsigned __int128 m;
    int scale, n = 0, i, intLen, pos;
    if (!out || cap < 2) return -1;
    m = dec_mant(d);
    scale = dec_scale(d);
    if (m == 0) {
        snprintf(out, (size_t)cap, "0");
        return 0;
    }
    while (m > 0 && n < (int)sizeof(digits) - 1) {
        digits[n++] = (char)('0' + (int)(m % 10));
        m /= 10;
    }
    pos = 0;
    if (d.flags & GMB_DEC_SIGN) {
        if (pos < cap - 1) out[pos++] = '-';
    }
    intLen = n - scale;
    if (intLen <= 0) {
        if (pos < cap - 1) out[pos++] = '0';
        if (pos < cap - 1) out[pos++] = '.';
        while (intLen < 0 && pos < cap - 1) {
            out[pos++] = '0';
            intLen++;
        }
        for (i = n - 1; i >= 0 && pos < cap - 1; i--)
            out[pos++] = digits[i];
    } else {
        for (i = n - 1; i >= 0 && pos < cap - 1; i--) {
            if (i + 1 == scale && scale > 0) {
                out[pos++] = '.';
                if (pos >= cap - 1) break;
            }
            out[pos++] = digits[i];
        }
        if (scale <= 0) {
            /* integer: already emitted */
        }
    }
    if (scale > 0 && strchr(out, '.')) {
        i = pos - 1;
        while (i > 0 && out[i] == '0') i--;
        if (out[i] == '.') i--;
        pos = i + 1;
    }
    out[pos] = 0;
    return 0;
}

int GmbDecimal_Scale(GmbDecimal d) { return dec_scale(d); }
int GmbDecimal_IsNegative(GmbDecimal d) { return (d.flags & GMB_DEC_SIGN) != 0 && dec_mant(d) != 0; }
int GmbDecimal_IsZero(GmbDecimal d) { return dec_mant(d) == 0; }

GmbDecimal GmbDecimal_Neg(GmbDecimal d)
{
    if (dec_mant(d) == 0) return d;
    d.flags ^= GMB_DEC_SIGN;
    return d;
}

GmbDecimal GmbDecimal_Abs(GmbDecimal d)
{
    d.flags &= ~GMB_DEC_SIGN;
    return d;
}

GmbDecimal GmbDecimal_Add(GmbDecimal a, GmbDecimal b)
{
    unsigned __int128 ma = dec_mant(a), mb = dec_mant(b);
    int sa = dec_scale(a), sb = dec_scale(b);
    int na = GmbDecimal_IsNegative(a), nb = GmbDecimal_IsNegative(b);
    dec_align(&ma, &sa, &mb, &sb);
    if (na == nb)
        return dec_pack(ma + mb, sa, na);
    if (ma >= mb)
        return dec_pack(ma - mb, sa, na);
    return dec_pack(mb - ma, sa, nb);
}

GmbDecimal GmbDecimal_Sub(GmbDecimal a, GmbDecimal b)
{
    return GmbDecimal_Add(a, GmbDecimal_Neg(b));
}

GmbDecimal GmbDecimal_Mul(GmbDecimal a, GmbDecimal b)
{
    unsigned __int128 ma = dec_mant(a), mb = dec_mant(b);
    int scale = dec_scale(a) + dec_scale(b);
    int neg = GmbDecimal_IsNegative(a) != GmbDecimal_IsNegative(b);
    const unsigned __int128 max96 = (((unsigned __int128)1) << 96) - 1;
    unsigned __int128 r;
    if (ma == 0 || mb == 0) return GmbDecimal_Zero();
    r = ma * mb;
    while (r > max96 && scale > 0) {
        r /= 10;
        scale--;
    }
    return dec_pack(r, scale, neg);
}

int GmbDecimal_Cmp(GmbDecimal a, GmbDecimal b)
{
    unsigned __int128 ma = dec_mant(a), mb = dec_mant(b);
    int sa = dec_scale(a), sb = dec_scale(b);
    int na = GmbDecimal_IsNegative(a), nb = GmbDecimal_IsNegative(b);
    if (ma == 0 && mb == 0) return 0;
    if (na != nb) return na ? -1 : 1;
    dec_align(&ma, &sa, &mb, &sb);
    if (ma < mb) return na ? 1 : -1;
    if (ma > mb) return na ? -1 : 1;
    return 0;
}

int GmbDecimal_SelfCheck(void)
{
    char buf[64];
    GmbDecimal a, b, c, s;
    a = GmbDecimal_FromString("0.1");
    b = GmbDecimal_FromString("0.2");
    c = GmbDecimal_FromString("0.3");
    s = GmbDecimal_Add(a, b);
    if (GmbDecimal_Cmp(s, c) != 0) return -1;
    a = GmbDecimal_FromString("100.50");
    b = GmbDecimal_FromString("0.10");
    c = GmbDecimal_FromString("100.60");
    if (GmbDecimal_Cmp(GmbDecimal_Add(a, b), c) != 0) return -2;
    a = GmbDecimal_FromI64(100);
    b = GmbDecimal_FromI64(-100);
    if (!GmbDecimal_IsZero(GmbDecimal_Add(a, b))) return -3;
    if (GmbDecimal_ToString(GmbDecimal_FromString("100.50"), buf, (int)sizeof(buf)) != 0)
        return -4;
    if (strcmp(buf, "100.5") != 0 && strcmp(buf, "100.50") != 0) return -5;
    a = GmbDecimal_FromString("10.00");
    b = GmbDecimal_FromString("2");
    c = GmbDecimal_Mul(a, b);
    if (GmbDecimal_Cmp(c, GmbDecimal_FromI64(20)) != 0) return -6;
    if (GmbDecimal_Cmp(GmbDecimal_FromString("-1.25"), GmbDecimal_FromString("-1.250")) != 0)
        return -7;
    return 0;
}
