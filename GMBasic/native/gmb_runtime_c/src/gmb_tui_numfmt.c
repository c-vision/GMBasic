/* ============================================================================
 * gmb_tui_numfmt.c — NumberFormat (porting di ClipperLib/Datatypes
 * NumbersEx.cs, classe NumberFormat). Fedele al C#.
 * ============================================================================ */
#include "gmb_tui_numfmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char* const NUMFMT_PREFIXES[17] =
    { "y","z","a","f","p","n", "\xC2\xB5", "m","", "k","M","G","T","P","E","Z","Y" };

typedef struct numfmt_spec {
    bool zero;
    char fill;
    char align;          /* 0 (default), '<', '>', '=', '^' */
    char sign;           /* default '-'; '+', ' ' */
    bool symbol;         /* '$' o '#' */
    int width;
    bool grouping;
    int precision;
    char fmt_type;       /* 0, 's','%','b','o','O','x','X','f','e','E','d' */
} numfmt_spec_t;

void gmb_numfmt_init(gmb_number_format_t* nf, char decimal_sep, char group_delimiter)
{
    if (!nf) return;
    nf->decimal_sep = decimal_sep ? decimal_sep : '.';
    nf->group_delimiter = group_delimiter ? group_delimiter : ',';
}

/* parse manuale del format-spec:
   ^(?:(.)?([<>=^]))?([+\- ])?([$#])?(0)?(\d+)?(,)?(\.\d+)?([A-Za-z%])?$ */
static void numfmt_parse_pattern(const char* pattern, numfmt_spec_t* spec)
{
    memset(spec, 0, sizeof(*spec));
    spec->fill = ' ';
    spec->sign = '-';
    spec->precision = 6;
    if (!pattern) return;
    int i = 0, pl = (int)strlen(pattern);

    /* [(.)?([<>=^])] : fill (un solo char) seguito da align. Il fill è opzionale
       e riconosciuto SOLO se segue un align. */
    if (i + 1 < pl)
    {
        char c0 = pattern[i], c1 = pattern[i + 1];
        if (c0 == '<' || c0 == '>' || c0 == '=' || c0 == '^')
        {
            spec->fill = ' ';
            spec->align = c0;
            i++;
        }
        else if (c1 == '<' || c1 == '>' || c1 == '=' || c1 == '^')
        {
            spec->fill = c0;
            spec->align = c1;
            i += 2;
        }
    }

    /* ([+\- ]) */
    if (i < pl && (pattern[i] == '+' || pattern[i] == '-' || pattern[i] == ' '))
    {
        spec->sign = pattern[i];
        i++;
    }

    /* ([$#]) */
    if (i < pl && (pattern[i] == '$' || pattern[i] == '#'))
    {
        spec->symbol = true;
        i++;
    }

    /* (0)? */
    if (i < pl && pattern[i] == '0') { spec->zero = true; i++; }

    /* (\d+)? */
    if (i < pl && pattern[i] >= '0' && pattern[i] <= '9')
    {
        spec->width = 0;
        while (i < pl && pattern[i] >= '0' && pattern[i] <= '9')
        {
            spec->width = spec->width * 10 + (pattern[i] - '0');
            i++;
        }
    }

    /* (,)? */
    if (i < pl && pattern[i] == ',') { spec->grouping = true; i++; }

    /* (\.\d+)? */
    if (i < pl && pattern[i] == '.')
    {
        i++;
        spec->precision = 0;
        while (i < pl && pattern[i] >= '0' && pattern[i] <= '9')
        {
            spec->precision = spec->precision * 10 + (pattern[i] - '0');
            i++;
        }
    }

    /* ([A-Za-z%])? */
    if (i < pl && ((pattern[i] >= 'A' && pattern[i] <= 'Z') ||
                   (pattern[i] >= 'a' && pattern[i] <= 'z') || pattern[i] == '%'))
    {
        spec->fmt_type = pattern[i];
        i++;
    }

    if (spec->zero || (spec->fill == '0' && spec->align == '='))
    {
        spec->zero = true;
        spec->fill = '0';
        spec->align = '=';
    }
    if (spec->fmt_type == 'd') spec->precision = 0;
}

/* GetSignificantDigits */
static void numfmt_decompose(const gmb_number_format_t* nf, double value, int significant_digits,
                             char* coeff, int coeff_cap, int* exp)
{
    (void)nf;
    char formatted[64];
    if (significant_digits > 0)
        snprintf(formatted, sizeof(formatted), "%.*E", significant_digits - 1, value);
    else
        snprintf(formatted, sizeof(formatted), "%E", value);
    char* e = strchr(formatted, 'E');
    if (!e) { strncpy(coeff, formatted, (size_t)coeff_cap - 1); coeff[coeff_cap - 1] = '\0'; *exp = 0; return; }
    *exp = atoi(e + 1);
    *e = '\0';
    if (strlen(formatted) == 1) { snprintf(coeff, coeff_cap, "%s", formatted); return; }
    char* dot = strchr(formatted, '.');
    if (dot)
    {
        int in = (int)(dot - formatted);
        int fn = (int)strlen(dot + 1);
        memcpy(coeff, formatted, (size_t)in);
        memcpy(coeff + in, dot + 1, (size_t)fn);
        coeff[in + fn] = '\0';
    }
    else
        snprintf(coeff, coeff_cap, "%s", formatted);
}

static int numfmt_format_si_prefix(const gmb_number_format_t* nf, double value, int precision,
                                   char* out, int cap, int* prefix_exp)
{
    char coefficient[64];
    int exponent;
    numfmt_decompose(nf, value, precision, coefficient, sizeof(coefficient), &exponent);
    int prefixExp = (int)floor((double)exponent / 3);
    if (prefixExp < -8) prefixExp = -8;
    if (prefixExp > 8) prefixExp = 8;
    int i = exponent - prefixExp * 3 + 1;
    int n = (int)strlen(coefficient);
    int o = 0;
    if (i == n) { memcpy(out, coefficient, (size_t)n); o = n; }
    else if (i > n)
    {
        memcpy(out, coefficient, (size_t)n); o = n;
        for (int k = 0; k < i - n && o < cap - 1; k++) out[o++] = '0';
    }
    else if (i > 0)
    {
        memcpy(out, coefficient, (size_t)i); o = i;
        if (o < cap - 1) out[o++] = nf->decimal_sep;
        memcpy(out + o, coefficient + i, (size_t)(n - i)); o += (n - i);
    }
    else
    {
        int zeros = -i;
        char dec_part[64];
        int sub_prec = precision - zeros - 1;
        if (sub_prec < 0) sub_prec = 0;
        int dce;
        numfmt_decompose(nf, value, sub_prec, dec_part, sizeof(dec_part), &dce);
        out[o++] = '0';
        if (o < cap - 1) out[o++] = nf->decimal_sep;
        for (int k = 0; k < zeros && o < cap - 1; k++) out[o++] = '0';
        for (char* p = dec_part; *p && o < cap - 1; p++) out[o++] = *p;
    }
    if (o < cap) out[o] = '\0';
    *prefix_exp = prefixExp;
    return o;
}

static int numfmt_group_value(const gmb_number_format_t* nf, const char* value, int width,
                              char* out, int cap)
{
    int vl = (int)strlen(value);
    char reversed[512];
    int rn = 0;
    for (int i = vl - 1; i >= 0; i--) reversed[rn++] = value[i];
    char grouped[512];
    int gn = 0, count = 0;
    for (int i = 0; i < rn; i++)
    {
        grouped[gn++] = reversed[i];
        count++;
        if (count == 3 && i < rn - 1) { grouped[gn++] = nf->group_delimiter; count = 0; }
    }
    char sb[512];
    int sn = 0;
    for (int i = gn - 1; i >= 0; i--) sb[sn++] = grouped[i];
    sb[sn] = '\0';

    if (width > 0 && sn > width)
    {
        int to_skip = (sb[width - 1] == nf->group_delimiter) ? sn - width - 1 : sn - width;
        sn -= to_skip;
        memcpy(out, sb + to_skip, (size_t)sn);
        out[sn] = '\0';
        return sn;
    }
    memcpy(out, sb, (size_t)sn);
    out[sn] = '\0';
    return sn;
}

static int numfmt_formatted_exp(const gmb_number_format_t* nf, char fmt_type, double value,
                                int precision, bool include_decimal_point, char* out, int cap)
{
    char formatted[128];
    if (fmt_type == 'e')
        snprintf(formatted, sizeof(formatted), "%.*e", precision, value);
    else
        snprintf(formatted, sizeof(formatted), "%.*E", precision, value);
    if (!include_decimal_point || precision != 0)
    {
        /* .NET usa esponente con minimo 3 cifre: e+04 -> e+004 */
        char* e = strchr(formatted, 'e');
        if (!e) e = strchr(formatted, 'E');
        if (e && e[1] && (e[1] == '+' || e[1] == '-') && e[2] >= '0' && e[2] <= '9')
        {
            int digits = 0;
            for (char* p = e + 2; *p >= '0' && *p <= '9'; p++) digits++;
            if (digits < 3)
            {
                int need = 3 - digits;
                int total = (int)strlen(formatted);
                if (total + need < (int)sizeof(formatted) - 1)
                {
                    for (int i = total + need; i > (int)(e - formatted) + 2; i--)
                        formatted[i] = formatted[i - need];
                    for (int i = 0; i < need; i++)
                        formatted[(e - formatted) + 2 + i] = '0';
                }
            }
        }
        snprintf(out, cap, "%s", formatted);
        return (int)strlen(out);
    }
    /* alternate form: force decimal point */
    char* e = strchr(formatted, 'e');
    if (!e) e = strchr(formatted, 'E');
    if (!e) { snprintf(out, cap, "%s", formatted); return (int)strlen(out); }
    int mant_len = (int)(e - formatted);
    if (!memchr(formatted, nf->decimal_sep, (size_t)mant_len))
    {
        memcpy(out, formatted, (size_t)mant_len);
        out[mant_len] = nf->decimal_sep;
        out[mant_len + 1] = '\0';
        int o = mant_len + 1;
        for (char* p = e; *p && o < cap - 1; p++) out[o++] = *p;
        out[o] = '\0';
        return o;
    }
    snprintf(out, cap, "%s", formatted);
    return (int)strlen(out);
}

static void numfmt_to_base(unsigned long long v, int base, bool upper, char* out, int cap)
{
    char tmp[80];
    int n = 0;
    if (v == 0) { snprintf(out, cap, "0"); return; }
    while (v > 0)
    {
        int d = (int)(v % (unsigned)base);
        v /= (unsigned)base;
        tmp[n++] = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + (d - 10));
    }
    int o = 0;
    for (int i = n - 1; i >= 0 && o < cap - 1; i--) out[o++] = tmp[i];
    out[o] = '\0';
}

static char numfmt_sign_prefix(bool is_negative, const numfmt_spec_t* spec)
{
    if (is_negative) return '-';
    if (spec->sign == '+') return '+';
    if (spec->sign == ' ') return ' ';
    return '\0';
}

int gmb_numfmt_format(const gmb_number_format_t* nf, const char* pattern, double value,
                      char* out, int cap)
{
    if (!nf || !pattern || !out || cap <= 0) return 0;
    numfmt_spec_t spec;
    numfmt_parse_pattern(pattern, &spec);
    bool value_is_negative = value < 0;
    double abs = fabs(value);

    char decimal_part[256] = "";
    const char* si_prefix = "";
    char si_prefix_buf[8] = "";
    char unit[2] = "";
    if (spec.fmt_type == '%') unit[0] = '%';

    char si_value[256] = "";
    if (spec.fmt_type == 's')
    {
        int p;
        int si_n = numfmt_format_si_prefix(nf, abs, spec.precision, si_value, sizeof(si_value), &p);
        (void)si_n;
        strcpy(si_prefix_buf, NUMFMT_PREFIXES[8 + p]);
        si_prefix = si_prefix_buf;
    }

    char val[256];
    switch (spec.fmt_type)
    {
        case '%': snprintf(val, sizeof(val), "%.*f", spec.precision, abs * 100.0); break;
        case 'b': numfmt_to_base((unsigned long long)abs, 2, false, val, sizeof(val)); break;
        case 'o': case 'O': numfmt_to_base((unsigned long long)abs, 8, spec.fmt_type == 'O', val, sizeof(val)); break;
        case 'x': numfmt_to_base((unsigned long long)abs, 16, false, val, sizeof(val)); break;
        case 'X': numfmt_to_base((unsigned long long)abs, 16, true, val, sizeof(val)); break;
        case 'f':
            if (spec.symbol)
            {
                snprintf(val, sizeof(val), "%.*f", spec.precision, abs);
                if (spec.precision == 0)
                {
                    int vl = (int)strlen(val);
                    val[vl++] = nf->decimal_sep;
                    val[vl] = '\0';
                }
            }
            else snprintf(val, sizeof(val), "%.*f", spec.precision, abs);
            break;
        case 'e': case 'E':
            numfmt_formatted_exp(nf, spec.fmt_type, abs, spec.precision, spec.symbol, val, sizeof(val));
            break;
        case 's': snprintf(val, sizeof(val), "%s", si_value); break;
        default: snprintf(val, sizeof(val), "%.*f", spec.precision, abs); break;
    }

    if (spec.fmt_type != 'x' && spec.fmt_type != 'X' && value_is_negative
        && fabs(atof(val)) == 0.0 && spec.sign != '+')
    {
        value_is_negative = false;
    }

    char sign_ch = numfmt_sign_prefix(value_is_negative, &spec);
    char sign_prefix[2] = { sign_ch, '\0' };
    const char* leading_part = "";
    char leading_buf[4] = "";
    if (spec.symbol)
    {
        switch (spec.fmt_type)
        {
            case 'b': strcpy(leading_buf, "0b"); break;
            case 'o': strcpy(leading_buf, "0o"); break;
            case 'x': strcpy(leading_buf, "0x"); break;
            case 'O': strcpy(leading_buf, "0O"); break;
            case 'X': strcpy(leading_buf, "0x"); break;
            default: break;
        }
        leading_part = leading_buf;
    }

    /* split integer / decimal parts: primo char non-cifra */
    char int_val[256];
    int dot = -1;
    int vl = (int)strlen(val);
    for (int i = 0; i < vl; i++)
        if (!(val[i] >= '0' && val[i] <= '9')) { dot = i; break; }
    if (dot >= 0)
    {
        snprintf(decimal_part, sizeof(decimal_part), "%s", val + dot);
        val[dot] = '\0';
    }
    snprintf(int_val, sizeof(int_val), "%s", val);

    char prefix[8];
    snprintf(prefix, sizeof(prefix), "%s%s", sign_prefix, leading_part);
    char suffix[512];
    snprintf(suffix, sizeof(suffix), "%s%s%s", decimal_part, si_prefix, unit);

    char grouped_val[512];
    snprintf(grouped_val, sizeof(grouped_val), "%s", int_val);
    if (spec.grouping && !spec.zero)
        numfmt_group_value(nf, int_val, 0, grouped_val, sizeof(grouped_val));

    int plen = (int)strlen(prefix);
    int vlen = (int)strlen(grouped_val);
    int slen = (int)strlen(suffix);
    int length = plen + vlen + slen;
    char padding[128] = "";
    int pn = 0;
    if (length < spec.width)
    {
        int need = spec.width - length;
        for (int i = 0; i < need && pn < (int)sizeof(padding) - 1; i++) padding[pn++] = spec.fill;
        padding[pn] = '\0';
    }

    if (spec.grouping && spec.zero)
    {
        char pre_pad[512];
        snprintf(pre_pad, sizeof(pre_pad), "%s%s%s", plen > 0 ? padding : "", grouped_val, "");
        if (plen > 0)
        {
            /* C#: GroupValue(padding + val, spec.Width - suffix.Length) quando padding non vuoto */
            int gwidth = pn > 0 ? spec.width - slen : 0;
            int pl2 = (int)strlen(prefix);
            char with_pad[600];
            snprintf(with_pad, sizeof(with_pad), "%s%s", padding, grouped_val);
            char gv[600];
            numfmt_group_value(nf, with_pad, gwidth, gv, sizeof(gv));
            snprintf(grouped_val, sizeof(grouped_val), "%s", gv);
            (void)pl2;
        }
        else
        {
            char gv[600];
            numfmt_group_value(nf, grouped_val, 0, gv, sizeof(gv));
            snprintf(grouped_val, sizeof(grouped_val), "%s", gv);
        }
        (void)pre_pad;
        padding[0] = '\0';
    }

    /* assembla in base all'align */
    int o = 0;
    #define APPEND(s) for (const char* _p = (s); *_p && o < cap - 1; _p++) out[o++] = *_p
    switch (spec.align)
    {
        case '<':
            APPEND(prefix); APPEND(grouped_val); APPEND(suffix); APPEND(padding);
            break;
        case '=':
            APPEND(prefix); APPEND(padding); APPEND(grouped_val); APPEND(suffix);
            break;
        case '^':
            { int half = pn / 2;
              for (int i = 0; i < half && o < cap - 1; i++) out[o++] = padding[i];
              APPEND(prefix); APPEND(grouped_val); APPEND(suffix);
              for (int i = half; i < pn && o < cap - 1; i++) out[o++] = padding[i];
              break; }
        default:
            APPEND(padding); APPEND(prefix); APPEND(grouped_val); APPEND(suffix);
            break;
    }
    #undef APPEND
    out[o] = '\0';
    return o;
}
