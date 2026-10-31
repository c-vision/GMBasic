/* ============================================================================
 * gmb_tui_datatypes.c — porting C di ClipperLib/Datatypes.
 * Locale, Numbers, NumbersEx, Dates, Business, Strings, StringsEx, Macros.
 * Tutto puro (no OS), fedele alle sorgenti C#.
 * ============================================================================ */
#include "gmb_tui_datatypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ===================== Locale ===================== */

gmb_locale_t gmb_locale_invariant(void)
{
    gmb_locale_t l;
    memset(&l, 0, sizeof(l));
    strcpy(l.language, "en"); strcpy(l.country, "US");
    strcpy(l.decimal_sep, "."); strcpy(l.thousands_sep, ",");
    strcpy(l.date_format, "yyyy-mm-dd"); strcpy(l.time_format, "HH:MM:ss");
    return l;
}

gmb_locale_t gmb_locale_italian(void)
{
    gmb_locale_t l;
    memset(&l, 0, sizeof(l));
    strcpy(l.language, "it"); strcpy(l.country, "IT");
    strcpy(l.decimal_sep, ","); strcpy(l.thousands_sep, ".");
    strcpy(l.date_format, "dd/mm/yyyy"); strcpy(l.time_format, "HH:MM:ss");
    return l;
}

gmb_locale_t gmb_locale_us(void)
{
    gmb_locale_t l;
    memset(&l, 0, sizeof(l));
    strcpy(l.language, "en"); strcpy(l.country, "US");
    strcpy(l.decimal_sep, "."); strcpy(l.thousands_sep, ",");
    strcpy(l.date_format, "mm/dd/yyyy"); strcpy(l.time_format, "HH:MM:ss");
    return l;
}

/* ===================== Numbers ===================== */

static void numbers_add_thousands(const char* digits, const char* sep, char* out, int cap)
{
    int dl = (int)strlen(digits);
    if (dl <= 3) { snprintf(out, cap, "%s", digits); return; }
    int rem = dl % 3;
    if (rem == 0) rem = 3;
    int n = 0;
    memcpy(out + n, digits, (size_t)rem); n += rem;
    for (int i = rem; i < dl; i += 3)
    {
        size_t sepl = strlen(sep);
        if (n + (int)sepl + 3 >= cap) break;
        memcpy(out + n, sep, sepl); n += (int)sepl;
        memcpy(out + n, digits + i, 3); n += 3;
    }
    out[n] = '\0';
}

static const char* loc_sep(const gmb_locale_t* loc, const char* def)
{
    return (loc && loc->thousands_sep[0]) ? loc->thousands_sep : def;
}

int gmb_numbers_format_int(long long v, const gmb_locale_t* loc, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    const char* sep = loc_sep(loc, ",");
    int neg = v < 0 ? 1 : 0;
    long long abs = v < 0 ? -v : v;
    char digits[64];
    snprintf(digits, sizeof(digits), "%lld", abs);
    char body[128];
    numbers_add_thousands(digits, sep, body, (int)sizeof(body));
    if (neg) snprintf(out, cap, "-%s", body);
    else snprintf(out, cap, "%s", body);
    return (int)strlen(out);
}

static void numbers_extract_fraction(double abs, int decimals, char* out, int cap)
{
    if (decimals <= 0) { out[0] = '\0'; return; }
    double frac = abs - floor(abs);
    double p = pow(10.0, decimals);
    long long scaled = (long long)llround(frac * p);
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", scaled);
    int bl = (int)strlen(buf);
    if (bl < decimals)
    {
        int n = 0;
        for (int i = 0; i < decimals - bl && n < cap - 1; i++) out[n++] = '0';
        memcpy(out + n, buf, (size_t)bl); n += bl;
        out[n] = '\0';
        if (n > decimals) out[decimals] = '\0';
    }
    else
    {
        snprintf(out, cap, "%s", buf);
        out[decimals] = '\0';
    }
}

int gmb_numbers_format_decimal(double v, int decimals, const gmb_locale_t* loc, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    const gmb_locale_t l = loc ? *loc : gmb_locale_invariant();
    const char* tsep = l.thousands_sep[0] ? l.thousands_sep : ",";
    const char* dsep = l.decimal_sep[0] ? l.decimal_sep : ".";
    int neg = v < 0 ? 1 : 0;
    double abs = fabs(v);
    if (decimals < 0) decimals = 0;
    double p = pow(10.0, decimals);
    double scaled = llround(abs * p);
    long long int_part = (long long)floor(scaled / p);
    char intdigits[64];
    snprintf(intdigits, sizeof(intdigits), "%lld", int_part);
    char grouped[128];
    numbers_add_thousands(intdigits, tsep, grouped, (int)sizeof(grouped));
    char frac[128];
    numbers_extract_fraction(abs, decimals, frac, (int)sizeof(frac));
    if (neg) snprintf(out, cap, "-%s%s%s", grouped, dsep, frac);
    else snprintf(out, cap, "%s%s%s", grouped, dsep, frac);
    return (int)strlen(out);
}

int gmb_numbers_format_currency(double v, const gmb_locale_t* loc, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    const gmb_locale_t l = loc ? *loc : gmb_locale_invariant();
    char body[256];
    gmb_numbers_format_decimal(v, 2, &l, body, (int)sizeof(body));
    if (strcmp(l.country, "IT") == 0)
        snprintf(out, cap, "\xE2\x82\xAC%s", body);
    else
        snprintf(out, cap, "$%s", body);
    return (int)strlen(out);
}

int gmb_numbers_format_bytes(long long bytes, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (bytes < 0) bytes = 0;
    static const char* const units[] = { "B", "KB", "MB", "GB", "TB", "PB" };
    double value = (double)bytes;
    int unit = 0;
    int n_units = (int)(sizeof(units) / sizeof(units[0]));
    while (value >= 1024 && unit < n_units - 1) { value /= 1024; unit++; }
    if (unit == 0)
        snprintf(out, cap, "%.0f %s", value, units[unit]);
    else
        snprintf(out, cap, "%.2f %s", value, units[unit]);
    return (int)strlen(out);
}

/* ---- PICTURE numerico Clipper ---- */

enum { GMB_PIC_K_DIGIT0, GMB_PIC_K_DIGIT, GMB_PIC_K_GROUP, GMB_PIC_K_DEC,
       GMB_PIC_K_SIGNMINUS, GMB_PIC_K_SIGNPLUS, GMB_PIC_K_PARENOPEN,
       GMB_PIC_K_PARENCLOSE, GMB_PIC_K_CURRENCY, GMB_PIC_K_FILL, GMB_PIC_K_LIT };

typedef struct gmb_pic_tok {
    int kind;
    char lit;
    int index;      /* slot intero 0 = destra */
    int frac_index; /* slot frazione 0 = dopo virgola */
    bool is_frac;
    bool can_sign;
} gmb_pic_tok_t;

static int numbers_parse_picture(const char* picture, gmb_pic_tok_t* toks, int max_toks,
                                 int* frac_slots, bool* has_int_sign)
{
    int n = 0, mode = 0; /* 0=Integer 1=Fraction */
    *frac_slots = 0;
    *has_int_sign = false;
    for (const char* c = picture; *c; c++)
    {
        if (n >= max_toks) break;
        gmb_pic_tok_t* t = &toks[n];
        memset(t, 0, sizeof(*t));
        switch (*c)
        {
            case '0': t->kind = GMB_PIC_K_DIGIT0; t->is_frac = mode == 1; if (mode == 1) (*frac_slots)++; break;
            case '9': case '#': t->kind = GMB_PIC_K_DIGIT; t->is_frac = mode == 1; if (mode == 1) (*frac_slots)++; break;
            case ',': t->kind = GMB_PIC_K_GROUP; break;
            case '.': mode = 1; t->kind = GMB_PIC_K_DEC; break;
            case '$': t->kind = GMB_PIC_K_CURRENCY; break;
            case '*': t->kind = GMB_PIC_K_FILL; break;
            case '-':
                if (mode == 0) { t->kind = GMB_PIC_K_SIGNMINUS; *has_int_sign = true; }
                else { t->kind = GMB_PIC_K_LIT; t->lit = *c; }
                break;
            case '+':
                if (mode == 0) { t->kind = GMB_PIC_K_SIGNPLUS; *has_int_sign = true; }
                else { t->kind = GMB_PIC_K_LIT; t->lit = *c; }
                break;
            case '(':
                if (mode == 0) { t->kind = GMB_PIC_K_PARENOPEN; *has_int_sign = true; }
                else { t->kind = GMB_PIC_K_LIT; t->lit = *c; }
                break;
            case ')':
                if (mode == 0) { t->kind = GMB_PIC_K_PARENCLOSE; *has_int_sign = true; }
                else { t->kind = GMB_PIC_K_LIT; t->lit = *c; }
                break;
            default: t->kind = GMB_PIC_K_LIT; t->lit = *c; break;
        }
        n++;
    }
    return n;
}

int gmb_numbers_format_number(const char* picture, double value, const gmb_locale_t* loc,
                              char* out, int cap)
{
    if (!out || cap <= 0 || !picture) return 0;
    const gmb_locale_t l = loc ? *loc : gmb_locale_invariant();
    bool negative = value < 0;
    double abs = fabs(value);

    gmb_pic_tok_t toks[96];
    int frac_slots; bool has_int_sign;
    int n = numbers_parse_picture(picture, toks, 96, &frac_slots, &has_int_sign);

    int idx = 0, min_int_sign = 0;
    for (int k = n - 1; k >= 0; k--)
    {
        gmb_pic_tok_t* t = &toks[k];
        if (t->kind == GMB_PIC_K_DIGIT0 || t->kind == GMB_PIC_K_DIGIT)
        {
            if (!t->is_frac)
            {
                t->index = idx; idx++;
                min_int_sign = idx;
                t->can_sign = !has_int_sign;
            }
        }
        else if (t->kind == GMB_PIC_K_GROUP && !t->is_frac)
        {
            t->index = idx;
            t->can_sign = !has_int_sign;
        }
    }
    int frac_idx = 0;
    for (int k = 0; k < n; k++)
    {
        gmb_pic_tok_t* t = &toks[k];
        if (t->is_frac && (t->kind == GMB_PIC_K_DIGIT0 || t->kind == GMB_PIC_K_DIGIT))
            t->frac_index = frac_idx++;
    }
    int len_int = idx;

    /* arrotonda alla precisione frazionaria richiesta (AwayFromZero) */
    double p = pow(10.0, frac_slots);
    double scaled = round(abs * p) / p;
    long long int_part = (long long)floor(scaled);
    char intdigits[64];
    if (int_part == 0) strcpy(intdigits, "0");
    else snprintf(intdigits, sizeof(intdigits), "%lld", int_part);

    char fracdigits[64] = "";
    if (frac_slots > 0)
    {
        double frac = scaled - floor(scaled);
        long long f = (long long)llround(frac * p);
        long long limit = (long long)pow(10.0, frac_slots);
        if (f >= limit)
        {
            int_part++; f = 0;
            if (int_part == 0) strcpy(intdigits, "0");
            else snprintf(intdigits, sizeof(intdigits), "%lld", int_part);
        }
        char fb[64];
        snprintf(fb, sizeof(fb), "%lld", f);
        int fl = (int)strlen(fb);
        int wn = 0;
        for (int i = 0; i < frac_slots - fl; i++) fracdigits[wn++] = '0';
        memcpy(fracdigits + wn, fb, (size_t)fl); wn += fl;
        fracdigits[wn] = '\0';
    }

    char group_ch = l.thousands_sep[0] ? l.thousands_sep[0] : ',';
    char dec_ch = l.decimal_sep[0] ? l.decimal_sep[0] : '.';
    const char* symbol = strcmp(l.country, "IT") == 0 ? "\xE2\x82\xAC" : "$";
    char fill = ' ';

    int o = 0;
    for (int k = 0; k < n; k++)
    {
        gmb_pic_tok_t* t = &toks[k];
        int idl = (int)strlen(intdigits);
        int fdl = (int)strlen(fracdigits);
        switch (t->kind)
        {
            case GMB_PIC_K_DIGIT0:
                if (t->is_frac)
                    { char ch = fdl > t->frac_index ? fracdigits[t->frac_index] : '0'; if (o < cap - 1) out[o++] = ch; }
                else
                    { char ch = idl > t->index ? intdigits[idl - 1 - t->index] : '0'; if (o < cap - 1) out[o++] = ch; }
                break;
            case GMB_PIC_K_DIGIT:
                if (t->is_frac)
                {
                    char ch = fdl > t->frac_index ? fracdigits[t->frac_index] : ' ';
                    if (o < cap - 1) out[o++] = ch;
                }
                else if (idl > t->index)
                    { if (o < cap - 1) out[o++] = intdigits[idl - 1 - t->index]; }
                else if (t->can_sign && (int)(idl > min_int_sign ? idl : min_int_sign) == t->index)
                    { if (o < cap - 1) out[o++] = negative ? '-' : ' '; }
                else
                    { if (o < cap - 1) out[o++] = fill; }
                break;
            case GMB_PIC_K_GROUP:
                if (idl > t->index)
                    { if (o < cap - 1) out[o++] = group_ch; }
                else if (t->can_sign && (int)(idl > min_int_sign ? idl : min_int_sign) == t->index)
                    { if (o < cap - 1) out[o++] = negative ? '-' : ' '; }
                else
                    { if (o < cap - 1) out[o++] = ' '; }
                break;
            case GMB_PIC_K_DEC: if (o < cap - 1) out[o++] = dec_ch; break;
            case GMB_PIC_K_SIGNMINUS: if (o < cap - 1) out[o++] = negative ? '-' : ' '; break;
            case GMB_PIC_K_SIGNPLUS: if (o < cap - 1) out[o++] = negative ? '-' : '+'; break;
            case GMB_PIC_K_PARENOPEN: if (o < cap - 1) out[o++] = negative ? '(' : ' '; break;
            case GMB_PIC_K_PARENCLOSE: if (o < cap - 1) out[o++] = negative ? ')' : ' '; break;
            case GMB_PIC_K_CURRENCY:
                { size_t sl = strlen(symbol); for (size_t i = 0; i < sl && o < cap - 1; i++) out[o++] = symbol[i]; }
                break;
            case GMB_PIC_K_FILL: fill = '*'; break;
            default: if (o < cap - 1) out[o++] = t->lit; break;
        }
    }
    out[o] = '\0';
    (void)len_int;
    return o;
}

int gmb_numbers_format_with_separators(double value, int decimals, const gmb_locale_t* loc,
                                       char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    const gmb_locale_t l = loc ? *loc : gmb_locale_invariant();
    const char* tsep = l.thousands_sep[0] ? l.thousands_sep : ",";
    int neg = value < 0;
    double abs = fabs(value);
    if (decimals < 0) decimals = 0;
    double p = pow(10.0, decimals);
    double scaled = round(abs * p) / p;
    long long int_part = (long long)floor(scaled);
    char intstr[64];
    if (int_part == 0) strcpy(intstr, "0");
    else snprintf(intstr, sizeof(intstr), "%lld", int_part);
    char grouped[128];
    numbers_add_thousands(intstr, tsep, grouped, (int)sizeof(grouped));
    if (decimals <= 0)
    {
        snprintf(out, cap, "%s%s", neg ? "-" : "", grouped);
        return (int)strlen(out);
    }
    double frac = scaled - floor(scaled);
    long long f = (long long)llround(frac * p);
    long long limit = (long long)pow(10.0, decimals);
    if (f >= limit)
    {
        int_part++;
        if (int_part == 0) strcpy(intstr, "0"); else snprintf(intstr, sizeof(intstr), "%lld", int_part);
        numbers_add_thousands(intstr, tsep, grouped, (int)sizeof(grouped));
        f = 0;
    }
    char fstr[64];
    snprintf(fstr, sizeof(fstr), "%lld", f);
    int fl = (int)strlen(fstr);
    char fracstr[64];
    int wn = 0;
    for (int i = 0; i < decimals - fl; i++) fracstr[wn++] = '0';
    memcpy(fracstr + wn, fstr, (size_t)fl); wn += fl;
    fracstr[wn] = '\0';
    char dec_ch = l.decimal_sep[0] ? l.decimal_sep[0] : '.';
    snprintf(out, cap, "%s%s%c%s", neg ? "-" : "", grouped, dec_ch, fracstr);
    return (int)strlen(out);
}

int gmb_numbers_try_parse_number(const char* picture, const char* text, double* value,
                                 const gmb_locale_t* loc)
{
    (void)picture;
    if (!value) return 0;
    const gmb_locale_t l = loc ? *loc : gmb_locale_invariant();
    char dec_ch = l.decimal_sep[0] ? l.decimal_sep[0] : '.';
    char group_ch = l.thousands_sep[0] ? l.thousands_sep[0] : ',';
    char cleaned[512];
    int n = 0;
    bool negative = false;
    for (const char* c = text; *c && n < (int)sizeof(cleaned) - 1; c++)
    {
        if (*c >= '0' && *c <= '9') cleaned[n++] = *c;
        else if (*c == dec_ch) cleaned[n++] = '.';
        else if (*c == '-' || *c == '(') negative = true;
        else if (*c == group_ch || *c == '+' || *c == ')' || *c == '*' || *c == '$') { }
        else { /* ignored decoration */ }
    }
    cleaned[n] = '\0';
    if (n == 0) { *value = 0; return 0; }
    *value = negative ? -atof(cleaned) : atof(cleaned);
    return 1;
}

/* ===================== NumbersEx ===================== */

int gmb_numbersex_first_missing_positive(int* nums, int n)
{
    if (!nums || n <= 0) return 1;
    for (int i = 0; i < n; i++)
    {
        while (nums[i] > 0 && nums[i] <= n && nums[nums[i] - 1] != nums[i])
        {
            int num = nums[i];
            int tmp = nums[num - 1];
            nums[num - 1] = num;
            nums[i] = tmp;
        }
    }
    for (int i = 0; i < n; i++)
        if (nums[i] != i + 1) return i + 1;
    return n + 1;
}

long long gmb_numbersex_modular_exponent(long long n, long long x, long long p)
{
    long long ans = 1;
    if (x <= 0) return 1;
    while (true)
    {
        if (x == 1) return (ans * n) % p;
        if ((x & 1) == 0) { n = (n * n) % p; x >>= 1; continue; }
        else { ans = (ans * n) % p; x -= 1; }
    }
}

long long gmb_numbersex_gcd(long long a, long long b)
{
    if (a == b) return a;
    if (b > a) { long long t = a; a = b; b = t; }
    while (b > 0) { long long t = a; a = b; b = t % b; }
    return a;
}

long long gmb_numbersex_mod_inverse(long long n, long long p)
{
    if (p <= 1 || gmb_numbersex_gcd(n, p) > 1) return 0;
    /* n^(p-2) % p con __int128 per evitare overflow moltiplicativo */
    __int128 an = n, ap = p, ax = p - 2;
    __int128 ans = 1;
    while (ax > 0)
    {
        if (ax & 1) ans = (ans * an) % ap;
        an = (an * an) % ap;
        ax >>= 1;
    }
    return (long long)ans;
}

int gmb_numbersex_parse_integer(const char* value, const gmb_locale_t* loc, char thousands_sep,
                                long long* out)
{
    if (!value || !out) return 0;
    char thous = thousands_sep ? thousands_sep :
        (loc && loc->thousands_sep[0]) ? loc->thousands_sep[0] : ',';
    bool negative = false;
    char digits[256];
    int n = 0;
    bool symbol_found = false, symbol_seen = false;
    int after_symbol = 0;
    const char* input = value;
    while (*input == ' ' || *input == '\t') input++;
    for (const char* c = input; *c && n < (int)sizeof(digits) - 1; c++)
    {
        if (*c == '-' && n == 0) negative = true;
        else if (*c == thous) { symbol_found = true; after_symbol = 0; }
        else if (!(*c >= '0' && *c <= '9')) symbol_seen = true;
        else { after_symbol++; digits[n++] = *c; }
    }
    digits[n] = '\0';
    if (!symbol_seen && (!symbol_found || after_symbol == 0 || after_symbol == 3))
    {
        long long coin = atoll(digits);
        *out = negative ? -coin : coin;
        return 1;
    }
    return 0;
}

int gmb_numbersex_parse_currency(const char* value, const gmb_locale_t* loc,
                                 char thousands_sep, char decimal_sep, double* out)
{
    if (!value || !out) return 0;
    char thous = thousands_sep ? thousands_sep :
        (loc && loc->thousands_sep[0]) ? loc->thousands_sep[0] : ',';
    char decs = decimal_sep ? decimal_sep :
        (loc && loc->decimal_sep[0]) ? loc->decimal_sep[0] : '.';

    int num_of_minus = 0, num_of_decs = 0;
    for (const char* c = value; *c; c++)
    {
        if (*c == '-') num_of_minus++;
        if (*c == decs) num_of_decs++;
    }
    if (num_of_minus > 1 || num_of_decs > 1) return 0;

    bool negative = false, symbol_seen = false;
    char digits[512];
    int n = 0;
    char last_delimiter = '\0';
    int last_streak_len = 0;
    const char* input = value;
    while (*input == ' ' || *input == '\t') input++;

    int dec_pos_rev = -1, ri = 0;
    for (int i = (int)strlen(input) - 1; i >= 0; i--, ri++)
        if (input[i] == decs) { dec_pos_rev = ri; break; }
    int use_decimals = dec_pos_rev >= 0 ? dec_pos_rev : 0;

    for (const char* c = input; *c && n < (int)sizeof(digits) - 1; c++)
    {
        if (*c == '-' && n == 0) negative = true;
        else if (*c == thous || *c == decs) { last_streak_len = 0; last_delimiter = *c; }
        else if (!(*c >= '0' && *c <= '9')) symbol_seen = true;
        else { last_streak_len++; digits[n++] = *c; }
    }
    digits[n] = '\0';
    if (symbol_seen) return 0;
    if (last_delimiter == thous && last_streak_len != 0 && last_streak_len != 3) return 0;

    double coin = atof(digits);
    if (last_delimiter == decs)
    {
        if (last_streak_len < use_decimals)
        {
            int multiply = use_decimals - last_streak_len;
            coin = coin * pow(10, multiply);
        }
        else if (last_streak_len > use_decimals)
        {
            int divide = last_streak_len - use_decimals;
            coin = coin / pow(10, divide);
        }
        coin = coin / pow(10, use_decimals);
    }
    *out = negative ? -coin : coin;
    return 1;
}

/* ===================== Dates ===================== */

enum { GMB_DT_Y4, GMB_DT_Y2, GMB_DT_MONTH, GMB_DT_DAY, GMB_DT_H24, GMB_DT_H12,
       GMB_DT_MIN, GMB_DT_SEC, GMB_DT_AMPM, GMB_DT_LIT };

static int dates_match_tok(const char* format, int* i, int* len)
{
    const char* f = format + *i;
    int fl = (int)strlen(format);
    if (fl - *i >= 4 && strncmp(f, "yyyy", 4) == 0) { *i += 4; *len = 4; return GMB_DT_Y4; }
    if (fl - *i >= 2 && strncmp(f, "yy", 2) == 0) { *i += 2; *len = 2; return GMB_DT_Y2; }
    if (fl - *i >= 2 && strncmp(f, "mm", 2) == 0) { *i += 2; *len = 2; return GMB_DT_MONTH; }
    if (fl - *i >= 2 && strncmp(f, "dd", 2) == 0) { *i += 2; *len = 2; return GMB_DT_DAY; }
    if (fl - *i >= 2 && strncmp(f, "HH", 2) == 0) { *i += 2; *len = 2; return GMB_DT_H24; }
    if (fl - *i >= 2 && strncmp(f, "hh", 2) == 0) { *i += 2; *len = 2; return GMB_DT_H12; }
    if (fl - *i >= 2 && strncmp(f, "MM", 2) == 0) { *i += 2; *len = 2; return GMB_DT_MIN; }
    if (fl - *i >= 2 && strncmp(f, "ss", 2) == 0) { *i += 2; *len = 2; return GMB_DT_SEC; }
    if (fl - *i >= 2 && (strncmp(f, "TT", 2) == 0 || strncmp(f, "tt", 2) == 0)) { *i += 2; *len = 2; return GMB_DT_AMPM; }
    *i += 1; *len = 1; return GMB_DT_LIT;
}

static bool dates_read_int(const char* s, int* si, int max_len, int* value)
{
    int sl = (int)strlen(s);
    int start = *si, end = *si;
    while (end < sl && end - start < max_len && s[end] >= '0' && s[end] <= '9') end++;
    if (end == start) { *value = 0; return false; }
    char buf[16];
    memcpy(buf, s + start, (size_t)(end - start));
    buf[end - start] = '\0';
    *value = atoi(buf);
    *si = end;
    return true;
}

int gmb_dates_format(const gmb_dt_t* dt, const char* format, char* out, int cap)
{
    if (!dt || !format || !out || cap <= 0) return 0;
    int i = 0, o = 0;
    while (i < (int)strlen(format))
    {
        int len;
        int t = dates_match_tok(format, &i, &len);
        char buf[16];
        switch (t)
        {
            case GMB_DT_Y4: snprintf(buf, sizeof(buf), "%04d", dt->year); break;
            case GMB_DT_Y2: snprintf(buf, sizeof(buf), "%02d", dt->year % 100); break;
            case GMB_DT_MONTH: snprintf(buf, sizeof(buf), "%02d", dt->month); break;
            case GMB_DT_DAY: snprintf(buf, sizeof(buf), "%02d", dt->day); break;
            case GMB_DT_H24: snprintf(buf, sizeof(buf), "%02d", dt->hour); break;
            case GMB_DT_H12:
                { int h = dt->hour % 12; if (h == 0) h = 12; snprintf(buf, sizeof(buf), "%02d", h); }
                break;
            case GMB_DT_MIN: snprintf(buf, sizeof(buf), "%02d", dt->minute); break;
            case GMB_DT_SEC: snprintf(buf, sizeof(buf), "%02d", dt->second); break;
            case GMB_DT_AMPM: strcpy(buf, dt->hour < 12 ? "AM" : "PM"); break;
            default: buf[0] = format[i - 1]; buf[1] = '\0'; break;
        }
        for (char* p = buf; *p && o < cap - 1; p++) out[o++] = *p;
    }
    out[o] = '\0';
    return o;
}

int gmb_dates_parse(const char* s, const char* format, gmb_dt_t* out)
{
    if (!s || !format || !out) return 0;
    int si = 0;
    int year = 1, month = 1, day = 1, hour = 0, min = 0, sec = 0;
    bool pm = false;
    int sl = (int)strlen(s), fl = (int)strlen(format);
    int i = 0;
    while (i < fl)
    {
        int len;
        int t = dates_match_tok(format, &i, &len);
        switch (t)
        {
            case GMB_DT_Y4: { int y; if (!dates_read_int(s, &si, 4, &y)) return 0; year = y; break; }
            case GMB_DT_Y2: { int y; if (!dates_read_int(s, &si, 2, &y)) return 0; year = 2000 + y; break; }
            case GMB_DT_MONTH: { int mo; if (!dates_read_int(s, &si, 2, &mo)) return 0; month = mo; break; }
            case GMB_DT_DAY: { int d; if (!dates_read_int(s, &si, 2, &d)) return 0; day = d; break; }
            case GMB_DT_H24: { int h; if (!dates_read_int(s, &si, 2, &h)) return 0; hour = h; break; }
            case GMB_DT_H12: { int h; if (!dates_read_int(s, &si, 2, &h)) return 0; hour = h; break; }
            case GMB_DT_MIN: { int mm; if (!dates_read_int(s, &si, 2, &mm)) return 0; min = mm; break; }
            case GMB_DT_SEC: { int ss; if (!dates_read_int(s, &si, 2, &ss)) return 0; sec = ss; break; }
            case GMB_DT_AMPM:
                if (si + 2 > sl) return 0;
                { char ap[3]; memcpy(ap, s + si, 2); ap[2] = '\0';
                  if (strcmp(ap, "AM") != 0 && strcmp(ap, "PM") != 0 && strcmp(ap, "am") != 0 && strcmp(ap, "pm") != 0) return 0;
                  pm = (ap[0] == 'p' || ap[0] == 'P'); si += 2; }
                break;
            default:
                if (si >= sl || s[si] != format[i - 1]) return 0;
                si++; break;
        }
    }
    if (pm && hour < 12) hour += 12;
    if (month < 1 || month > 12 || day < 1 || day > gmb_dates_days_in_month(year, month)) return 0;
    if (hour > 23 || min > 59 || sec > 59) return 0;
    out->year = year; out->month = month; out->day = day;
    out->hour = hour; out->minute = min; out->second = sec;
    return 1;
}

int gmb_dates_days_in_month(int y, int m)
{
    if (m == 2) return gmb_dates_is_leap_year(y) ? 29 : 28;
    if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
    return 31;
}

bool gmb_dates_is_leap_year(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int gmb_dates_weekday(int y, int m, int d)
{
    if (m < 3) { m += 12; y--; }
    int k = y % 100;
    int j = y / 100;
    int h = (d + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    int dow = (h + 6) % 7;
    return dow;
}

/* giorni da 1970-01-01 (Gregoriano) */
static long long days_from_civil(long long y, int m, int d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(long long z, int* y, int* m, int* d)
{
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    int dd = (int)(doy - (153 * mp + 2) / 5 + 1);
    int mm = (int)(mp + (mp < 10 ? 3 : -9));
    *y = (int)(yy + (mm <= 2));
    *m = mm;
    *d = dd;
}

static long long dt_unix(const gmb_dt_t* dt)
{
    long long days = days_from_civil(dt->year, dt->month, dt->day);
    return days * 86400LL + dt->hour * 3600LL + dt->minute * 60LL + dt->second;
}

static gmb_dt_t dt_from_unix(long long s)
{
    gmb_dt_t dt;
    long long days = s / 86400;
    if (s < 0 && s % 86400 != 0) days -= 1;
    long long rem = s - days * 86400;
    int y, m, d;
    civil_from_days(days, &y, &m, &d);
    dt.year = y; dt.month = m; dt.day = d;
    dt.hour = (int)(rem / 3600); rem %= 3600;
    dt.minute = (int)(rem / 60); rem %= 60;
    dt.second = (int)rem;
    return dt;
}

long long gmb_dates_to_unix_seconds(const gmb_dt_t* dt)
{
    return dt ? dt_unix(dt) : 0;
}

gmb_dt_t gmb_dates_from_unix_seconds(long long s)
{
    return dt_from_unix(s);
}

long long gmb_dates_to_unix_milliseconds(const gmb_dt_t* dt)
{
    return dt_unix(dt) * 1000LL;
}

gmb_dt_t gmb_dates_from_unix_milliseconds(long long ms)
{
    return dt_from_unix(ms / 1000);
}

gmb_dt_t gmb_dates_add_months_safe(const gmb_dt_t* dt, int n)
{
    gmb_dt_t out = *dt;
    int y = dt->year;
    int m = dt->month + n;
    while (m <= 0) { m += 12; y--; }
    while (m > 12) { m -= 12; y++; }
    int max_day = gmb_dates_days_in_month(y, m);
    int day = dt->day < max_day ? dt->day : max_day;
    out.year = y; out.month = m; out.day = day;
    return out;
}

int gmb_dates_parse_flexible(const char* s, gmb_dt_t* out)
{
    if (!s || !out) return 0;
    /* trima */
    const char* start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", start);
    int bl = (int)strlen(buf);
    while (bl > 0 && (buf[bl - 1] == ' ' || buf[bl - 1] == '\t' || buf[bl - 1] == '\n' || buf[bl - 1] == '\r'))
        buf[--bl] = '\0';
    if (bl == 0) return 0;
    static const char* const fmts[] = {
        "yyyy-mm-dd", "yyyy-mm-dd HH:MM:ss", "yyyy-mm-ddTHH:MM:ss",
        "yyyy-mm-ddTHH:MM:ssZ", "dd/mm/yyyy", "dd/mm/yyyy HH:MM:ss",
        "mm/dd/yyyy", "mm/dd/yyyy HH:MM:ss", "yyyy/mm/dd",
        "dd-mm-yyyy", "mm-dd-yyyy", NULL
    };
    for (int i = 0; fmts[i]; i++)
        if (gmb_dates_parse(buf, fmts[i], out)) return 1;
    /* fallback: prova a interpretare come data generica "yyyy-mm-dd HH:MM:ss" tramite parse standard */
    if (gmb_dates_parse(buf, "yyyy-mm-dd HH:MM:ss", out)) return 1;
    if (gmb_dates_parse(buf, "dd/mm/yyyy", out)) return 1;
    return 0;
}

/* ===================== Business ===================== */

bool gmb_business_is_valid_luhn(const char* code)
{
    if (!code || !*code) return false;
    int checksum = 0, i = 0;
    int len = (int)strlen(code);
    for (int p = len - 1; p >= 0; p--)
    {
        char c = code[p];
        if (c == ' ' || c == '\t') continue;
        int d = c - '0';
        if (d < 0 || d > 9) return false;
        if (i % 2 == 1) d *= 2;
        if (d > 9) d -= 9;
        checksum += d;
        i++;
    }
    return i > 1 && checksum % 10 == 0;
}

/* ===================== Strings ===================== */

int gmb_strings_repeat(char c, int n, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (n <= 0) { out[0] = '\0'; return 0; }
    int m = n < cap - 1 ? n : cap - 1;
    memset(out, c, (size_t)m);
    out[m] = '\0';
    return m;
}

int gmb_strings_pad_right(const char* s, int width, char pad, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int sl = (int)strlen(s);
    if (sl >= width) { snprintf(out, cap, "%s", s); return sl; }
    int n = 0;
    for (int i = 0; i < sl && n < cap - 1; i++) out[n++] = s[i];
    int padn = width - sl;
    for (int i = 0; i < padn && n < cap - 1; i++) out[n++] = pad;
    out[n] = '\0';
    return n;
}

int gmb_strings_pad_left(const char* s, int width, char pad, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) s = "";
    int sl = (int)strlen(s);
    if (sl >= width) { snprintf(out, cap, "%s", s); return sl; }
    int padn = width - sl;
    int n = 0;
    for (int i = 0; i < padn && n < cap - 1; i++) out[n++] = pad;
    for (int i = 0; i < sl && n < cap - 1; i++) out[n++] = s[i];
    out[n] = '\0';
    return n;
}

int gmb_strings_truncate(const char* s, int max_len, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) { out[0] = '\0'; return 0; }
    int sl = (int)strlen(s);
    if (sl <= max_len) { snprintf(out, cap, "%s", s); return sl; }
    int m = max_len < cap - 1 ? max_len : cap - 1;
    memcpy(out, s, (size_t)m);
    out[m] = '\0';
    return m;
}

int gmb_strings_reverse(const char* s, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) { out[0] = '\0'; return 0; }
    int sl = (int)strlen(s);
    for (int i = 0; i < sl && i < cap - 1; i++)
        out[i] = s[sl - 1 - i];
    int m = sl < cap - 1 ? sl : cap - 1;
    out[m] = '\0';
    return m;
}

int gmb_strings_natural_compare(const char* a, const char* b)
{
    if (!a) a = "";
    if (!b) b = "";
    int i = 0, j = 0;
    int al = (int)strlen(a), bl = (int)strlen(b);
    while (i < al && j < bl)
    {
        char ca = a[i], cb = b[j];
        if (ca >= '0' && ca <= '9' && cb >= '0' && cb <= '9')
        {
            int si = i, sj = j;
            while (si < al && a[si] >= '0' && a[si] <= '9') si++;
            while (sj < bl && b[sj] >= '0' && b[sj] <= '9') sj++;
            int na_len = si - i, nb_len = sj - j;
            int cmp;
            if (na_len != nb_len) cmp = na_len < nb_len ? -1 : 1;
            else cmp = strncmp(a + i, b + j, (size_t)na_len);
            if (cmp != 0) return cmp;
            i = si; j = sj;
        }
        else
        {
            if (ca != cb) return ca < cb ? -1 : 1;
            i++; j++;
        }
    }
    if (al != bl) return al < bl ? -1 : 1;
    return 0;
}

int gmb_strings_substring_safe(const char* s, int start, int len, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) { out[0] = '\0'; return 0; }
    int sl = (int)strlen(s);
    if (start < 0) start = 0;
    if (start >= sl) { out[0] = '\0'; return 0; }
    if (len < 0) len = 0;
    if (start + len > sl) len = sl - start;
    int m = len < cap - 1 ? len : cap - 1;
    memcpy(out, s + start, (size_t)m);
    out[m] = '\0';
    return m;
}

int gmb_strings_truncate_middle(const char* s, int max, const char* ellipsis, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!s) { out[0] = '\0'; return 0; }
    if (!ellipsis) ellipsis = "...";
    int sl = (int)strlen(s), el = (int)strlen(ellipsis);
    if (sl <= max) { snprintf(out, cap, "%s", s); return sl; }
    if (max <= el) { return gmb_strings_truncate(s, max, out, cap); }
    int keep = max - el;
    int head = keep / 2;
    int tail = keep - head;
    int n = 0;
    for (int i = 0; i < head && n < cap - 1; i++) out[n++] = s[i];
    for (int i = 0; i < el && n < cap - 1; i++) out[n++] = ellipsis[i];
    for (int i = 0; i < tail && n < cap - 1; i++) out[n++] = s[sl - tail + i];
    out[n] = '\0';
    return n;
}

int gmb_strings_word_wrap(const char* text, int width, char* out, int cap, int* lines)
{
    if (!out || cap <= 0) return 0;
    if (width < 1) width = 1;
    if (lines) *lines = 0;
    if (!text || !*text) { out[0] = '\0'; return 0; }
    int o = 0, lines_count = 0;
    char current[4096];
    int cl = 0;
    char word[4096];
    int tl = (int)strlen(text);

    /* helper inline: flush current line */
    #define WRAP_FLUSH() \
        do { \
            for (int _i = 0; _i < cl && o < cap - 1; _i++) out[o++] = current[_i]; \
            if (o < cap - 1) out[o++] = '\n'; \
            cl = 0; lines_count++; \
        } while (0)

    int p = 0;
    while (p < tl)
    {
        /* estrai paragrafo (fino a '\n') */
        int pend = p;
        while (pend < tl && text[pend] != '\n') pend++;
        /* split paragrafo su spazi (come C# Split(' '), con vuote conservate) */
        int wp = p;
        while (wp <= pend)
        {
            int wstart = wp;
            int wend = wp;
            while (wend < pend && text[wend] != ' ') wend++;
            int wl = wend - wstart;
            memcpy(word, text + wstart, (size_t)wl);
            word[wl] = '\0';
            /* gestione word come C# */
            char* w = word;
            while ((int)strlen(w) > width)
            {
                if (cl > 0) { WRAP_FLUSH(); }
                for (int i = 0; i < width && o < cap - 1; i++) out[o++] = w[i];
                if (o < cap - 1) out[o++] = '\n';
                lines_count++;
                w += width;
            }
            int wl2 = (int)strlen(w);
            if (cl == 0)
            {
                memcpy(current, w, (size_t)wl2);
                cl = wl2;
            }
            else if (cl + 1 + wl2 <= width)
            {
                current[cl++] = ' ';
                memcpy(current + cl, w, (size_t)wl2);
                cl += wl2;
            }
            else
            {
                WRAP_FLUSH();
                memcpy(current, w, (size_t)wl2);
                cl = wl2;
            }
            wp = wend + 1;
        }
        /* lines.Add(current) di fine paragrafo */
        WRAP_FLUSH();
        p = pend + 1;
    }

    #undef WRAP_FLUSH
    if (o > 0 && out[o - 1] == '\n') o--;
    out[o] = '\0';
    if (lines) *lines = lines_count;
    return o;
}

bool gmb_strings_is_printable(const char* s)
{
    if (!s) return false;
    for (const char* p = s; *p; p++)
        if (!(*p >= 32 && *p < 127)) return false;
    return true;
}

/* ===================== StringsEx ===================== */

bool gmb_stringsex_is_vowel(char c) { return strchr("aeiouAEIOU", c) != NULL; }
bool gmb_stringsex_is_newline(char c) { return c == '\n'; }
bool gmb_stringsex_is_whitespace(char c) { return c == '\n' || c == '\t' || c == '\f' || c == '\r' || c == ' '; }
bool gmb_stringsex_is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool gmb_stringsex_is_ident_start(char c) { return gmb_stringsex_is_letter(c) || (unsigned char)c > 0x80 || c == '_'; }
bool gmb_stringsex_is_ident_char(char c) { return gmb_stringsex_is_ident_start(c) || (c >= '0' && c <= '9') || c == '-'; }
bool gmb_stringsex_is_ascii_printable(char c)
{
    if ((unsigned char)c > 127) return false;
    int v = (unsigned char)c;
    if (v < 32) return false;
    if (v > 126 && v < 160) return false;
    if (v == 254 || v == 255) return false;
    return true;
}
bool gmb_stringsex_is_non_printable(char c)
{
    int v = (unsigned char)c;
    return (v >= 0 && v <= 8) || v == 11 || (v >= 14 && v <= 31) || v == 127;
}
bool gmb_stringsex_is_valid_escape(const char* s)
{
    if (!s || s[0] != '\\') return false;
    return !gmb_stringsex_is_newline(s[1]);
}
static char char_at(const char* s, int index) { return s && index < (int)strlen(s) ? s[index] : '\0'; }
bool gmb_stringsex_would_start_an_ident_seq(const char* s)
{
    char cp1 = char_at(s, 0), cp2 = char_at(s, 1);
    if (cp1 == '-') return gmb_stringsex_is_ident_start(cp2) || cp2 == '-' || gmb_stringsex_is_valid_escape(s);
    if (gmb_stringsex_is_ident_start(cp1)) return true;
    return gmb_stringsex_is_valid_escape(s);
}
bool gmb_stringsex_is_start_a_number(const char* s)
{
    char cp1 = char_at(s, 0), cp2 = char_at(s, 1), cp3 = char_at(s, 2);
    if (cp1 == '+' || cp1 == '-')
    {
        if (cp2 >= '0' && cp2 <= '9') return true;
        return cp2 == '.' && cp3 >= '0' && cp3 <= '9';
    }
    else if (cp1 == '.') return cp2 >= '0' && cp2 <= '9';
    return cp1 >= '0' && cp1 <= '9';
}
bool gmb_stringsex_is_comment_start(const char* s) { return char_at(s, 0) == '/' && char_at(s, 1) == '*'; }
bool gmb_stringsex_is_comment_end(const char* s) { return char_at(s, 0) == '*' && char_at(s, 1) == '/'; }

int gmb_stringsex_count_occurrences(const char* input, uint32_t count[256])
{
    if (count) memset(count, 0, sizeof(uint32_t) * 256);
    if (!input) return 0;
    int total = 0;
    for (const unsigned char* p = (const unsigned char*)input; *p; p++)
    {
        if (count) count[*p]++;
        total++;
    }
    return total;
}

int gmb_stringsex_cow_replace(const char* src, const char* pattern, const char* to, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!src) { out[0] = '\0'; return 0; }
    if (!pattern || !*pattern || !strstr(src, pattern)) { snprintf(out, cap, "%s", src); return (int)strlen(out); }
    if (!to) to = "";
    int sl = (int)strlen(src), pl = (int)strlen(pattern), tl = (int)strlen(to);
    int o = 0;
    for (int i = 0; i < sl;)
    {
        if (i + pl <= sl && strncmp(src + i, pattern, (size_t)pl) == 0)
        {
            for (int k = 0; k < tl && o < cap - 1; k++) out[o++] = to[k];
            i += pl;
        }
        else
        {
            if (o < cap - 1) out[o++] = src[i];
            i++;
        }
    }
    out[o] = '\0';
    return o;
}

int gmb_stringsex_cow_replace_n(const char* src, const char* pattern, const char* to,
                                int count, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!src) { out[0] = '\0'; return 0; }
    if (!pattern || count <= 0 || !strstr(src, pattern))
    {
        snprintf(out, cap, "%s", src);
        return (int)strlen(out);
    }
    if (!to) to = "";
    int sl = (int)strlen(src), pl = (int)strlen(pattern), tl = (int)strlen(to);
    int done = 0, o = 0;
    for (int i = 0; i < sl;)
    {
        if (done < count && i + pl <= sl && strncmp(src + i, pattern, (size_t)pl) == 0)
        {
            for (int k = 0; k < tl && o < cap - 1; k++) out[o++] = to[k];
            i += pl;
            done++;
        }
        else
        {
            if (o < cap - 1) out[o++] = src[i];
            i++;
        }
    }
    out[o] = '\0';
    return o;
}

static void ascii_lower(char* buf, int len)
{
    for (int i = 0; i < len; i++)
        if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] = (char)(buf[i] + 32);
}

static void ascii_upper(char* buf, int len)
{
    for (int i = 0; i < len; i++)
        if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] = (char)(buf[i] - 32);
}

int gmb_stringsex_cow_to_ascii_lowercase(const char* src, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!src) { out[0] = '\0'; return 0; }
    int sl = (int)strlen(src);
    bool has_upper = false;
    for (int i = 0; i < sl; i++)
        if (src[i] >= 'A' && src[i] <= 'Z') { has_upper = true; break; }
    if (!has_upper) { snprintf(out, cap, "%s", src); return sl; }
    snprintf(out, cap, "%s", src);
    ascii_lower(out, (int)strlen(out));
    return (int)strlen(out);
}

int gmb_stringsex_cow_to_ascii_uppercase(const char* src, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!src) { out[0] = '\0'; return 0; }
    int sl = (int)strlen(src);
    bool has_lower = false;
    for (int i = 0; i < sl; i++)
        if (src[i] >= 'a' && src[i] <= 'z') { has_lower = true; break; }
    if (!has_lower) { snprintf(out, cap, "%s", src); return sl; }
    snprintf(out, cap, "%s", src);
    ascii_upper(out, (int)strlen(out));
    return (int)strlen(out);
}

int gmb_stringsex_extract_ascii_printables(const char* input, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    if (!input) { out[0] = '\0'; return 0; }
    int o = 0;
    for (const char* p = input; *p && o < cap - 1; p++)
        if (gmb_stringsex_is_ascii_printable(*p)) out[o++] = *p;
    out[o] = '\0';
    return o;
}

/* ===================== Iters ===================== */

int gmb_iters_take_until_last_n(const int* src, int src_len, int n, int* out, int cap)
{
    if (!src || !out || cap <= 0) return 0;
    int keep = src_len - n;
    if (keep < 0) keep = 0;
    if (n <= 0) keep = src_len;
    int m = keep < cap ? keep : cap;
    for (int i = 0; i < m; i++) out[i] = src[i];
    return m;
}

int gmb_iters_flatten_results(const int* const* lists, const int* lens, int nlists,
                              int* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int o = 0;
    for (int l = 0; l < nlists && lists && lens; l++)
    {
        if (!lists[l]) continue;
        for (int i = 0; i < lens[l] && o < cap; i++)
            out[o++] = lists[l][i];
    }
    return o;
}

/* ===================== Regexes (pattern POSIX) ===================== */

#include <regex.h>

int gmb_regexes_matches(const char* pattern, const char* text, char out[][128], int cap)
{
    if (!pattern || !text || !out || cap <= 0) return 0;
    /* i pattern .NET hanno (?i) e costrutti non POSIX: rimuovi (?i) e prova REG_EXTENDED|REG_ICASE */
    char pat[512];
    snprintf(pat, sizeof(pat), "%s", pattern);
    char* clean = pat;
    if (strncmp(clean, "(?i)", 4) == 0) clean += 4;
    /* POSIX ERE non supporta (?:...): convertilo in (...) (cattura ok, match intero) */
    char conv[600];
    int ci = 0;
    for (char* p = clean; *p && ci < 599; p++)
    {
        if (p[0] == '(' && p[1] == '?' && p[2] == ':') { conv[ci++] = '('; p += 2; }
        else conv[ci++] = *p;
    }
    conv[ci] = '\0';
    regex_t re;
    int rc = regcomp(&re, conv, REG_EXTENDED | REG_ICASE);
    if (rc != 0) return 0;
    int count = 0;
    regmatch_t m;
    const char* cursor = text;
    while (count < cap && regexec(&re, cursor, 1, &m, 0) == 0)
    {
        if (m.rm_so < 0) break;
        int len = m.rm_eo - m.rm_so;
        if (len > 127) len = 127;
        memcpy(out[count], cursor + m.rm_so, (size_t)len);
        out[count][len] = '\0';
        count++;
        cursor += m.rm_eo;
        if (m.rm_eo == m.rm_so) break;
    }
    regfree(&re);
    return count;
}

/* ===================== Macros ===================== */

int gmb_macros_concat(const char* const* parts, int n, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    int o = 0;
    for (int i = 0; i < n && parts; i++)
    {
        if (!parts[i]) continue;
        for (const char* p = parts[i]; *p && o < cap - 1; p++) out[o++] = *p;
    }
    out[o] = '\0';
    return o;
}

/* ===================== Geometry (SnapZone / SnappedRect) ===================== */

int gmb_geometry_snap_zone(gmb_point_t p, gmb_rect_t screen, int threshold)
{
    if (threshold < 0) threshold = 0;
    if (p.x < screen.x + threshold) return GMB_SNAP_LEFT;
    if (p.x > gmb_rect_right(screen) - threshold) return GMB_SNAP_RIGHT;
    return GMB_SNAP_NONE;
}

int gmb_geometry_snapped_rect(int zone, gmb_rect_t work, gmb_rect_t* out)
{
    if (!out) return 0;
    int half = work.w / 2;
    if (zone == GMB_SNAP_LEFT)
    {
        out->x = work.x; out->y = work.y; out->w = half; out->h = work.h;
    }
    else
    {
        out->x = work.x + half; out->y = work.y; out->w = work.w - half; out->h = work.h;
    }
    return 1;
}
