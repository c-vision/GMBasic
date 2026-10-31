/* ============================================================================
 * gmb_tui_input.c — Input helper di Clipper (specchio ClipperLib/Inputs).
 * Currency, TextUtil, ConfigLocale, InputManagerNav.
 * ============================================================================ */
#include "gmb_tui_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void gmb_input_separators(bool european, char* out_thousands, char* out_decimal)
{
    if (european) { *out_thousands = '.'; *out_decimal = ','; }
    else          { *out_thousands = ','; *out_decimal = '.'; }
}

double gmb_currency_range_coerce(double value, double min, double max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

double gmb_currency_round_decimals(double value, int decimals)
{
    double f = pow(10.0, decimals);
    return round(value * f) / f;
}

static int currency_add_separators(const char* int_part, char sep, int group, char* out, int out_cap)
{
    if (sep == '\0' || group <= 0) { snprintf(out, out_cap, "%s", int_part); return (int)strlen(out); }
    bool neg = int_part[0] == '-';
    const char* digits = neg ? int_part + 1 : int_part;
    int dl = (int)strlen(digits);
    if (dl <= 0) { snprintf(out, out_cap, "%s", int_part); return (int)strlen(out); }
    /* costruiamo da destra a sinistra in tmp, inserendo sep ogni 'group' cifre */
    char tmp[160];
    int tn = 0;
    int count = 0;
    for (int i = dl - 1; i >= 0; i--)
    {
        if (count > 0 && count % group == 0)
            tmp[tn++] = sep;
        tmp[tn++] = digits[i];
        count++;
    }
    tmp[tn] = '\0';
    /* tmp è invertito → ribaltiamo in out, con eventuale segno */
    int outn = 0;
    if (neg && outn < out_cap - 1) out[outn++] = '-';
    for (int i = tn - 1; i >= 0 && outn < out_cap - 1; i--)
        out[outn++] = tmp[i];
    out[outn] = '\0';
    return outn;
}

int gmb_currency_format(double value, int decimals, char thousands_sep, char decimal_sep,
                        char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    char tho = thousands_sep ? thousands_sep : GMB_CURRENCY_DEFAULT_THOUSANDS;
    char dec = decimal_sep ? decimal_sep : GMB_CURRENCY_DEFAULT_DECIMAL;
    if (decimals < 0) decimals = 0;
    char s[128];
    snprintf(s, sizeof(s), "%.*f", decimals, value);
    char* dot = strchr(s, '.');
    if (!dot)
        return currency_add_separators(s, tho, 3, out, out_cap);
    *dot = '\0';
    char int_sep[128];
    int il = currency_add_separators(s, tho, 3, int_sep, (int)sizeof(int_sep));
    snprintf(out, out_cap, "%s%c%s", int_sep, dec, dot + 1);
    (void)il;
    return (int)strlen(out);
}

int gmb_currency_tryparse(const char* text, double* value, int* decimals, char decimal_sep)
{
    if (!text || !value) return 0;
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
    if (!*text) { *value = 0.0; return 0; }
    char dec = decimal_sep ? decimal_sep : GMB_CURRENCY_DEFAULT_DECIMAL;
    char cleaned[256];
    int n = 0;
    for (const char* p = text; *p && n < (int)sizeof(cleaned) - 1; p++)
    {
        if (*p >= '0' && *p <= '9') cleaned[n++] = *p;
        else if (*p == dec) cleaned[n++] = '.';
        else if (*p == '-') cleaned[n++] = *p;
        /* separatori migliaia e simboli valuta ignorati (lenient) */
    }
    cleaned[n] = '\0';
    if (n == 0) { *value = 0.0; return 0; }
    int decs = decimals ? *decimals : 0;
    if (decimals)
    {
        char* dp = strchr(cleaned, '.');
        if (!dp)
        {
            /* cleaned += "." + zeros(decs) */
            int cl = (int)strlen(cleaned);
            if (cl + decs + 1 < (int)sizeof(cleaned))
            {
                cleaned[cl++] = '.';
                for (int i = 0; i < decs; i++) cleaned[cl++] = '0';
                cleaned[cl] = '\0';
            }
        }
        else
        {
            int frac = (int)strlen(dp + 1);
            if (frac < decs)
            {
                int cl = (int)strlen(cleaned);
                for (int i = 0; i < decs - frac && cl < (int)sizeof(cleaned) - 1; i++)
                    cleaned[cl++] = '0';
                cleaned[cl] = '\0';
            }
            else if (frac > decs)
            {
                int keep = (int)(dp - cleaned) + 1 + decs;
                cleaned[keep] = '\0';
            }
        }
    }
    *value = atof(cleaned);
    return 1;
}

double gmb_currency_adjust_to_space(double value, int max_len, double min_value, double max_value)
{
    double adjusted = gmb_currency_range_coerce(value, min_value, max_value);
    if (adjusted == 0.0 || max_len == 0) return 0.0;
    char s[128];
    snprintf(s, sizeof(s), "%g", adjusted);
    double result = adjusted;
    int len = (int)strlen(s);
    while (len > max_len)
    {
        s[--len] = '\0';
        if (len > 0)
            result = atof(s);
        else
        {
            result = 0.0;
            break;
        }
    }
    return result;
}

int gmb_textutil_allowed_chars(const char* value, const char* allowed, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!value) { out[0] = '\0'; return 0; }
    const char* a = allowed ? allowed : GMB_INPUT_TEXT;
    int n = 0;
    for (const char* p = value; *p && n < out_cap - 1; p++)
        if (strchr(a, *p)) out[n++] = *p;
    out[n] = '\0';
    return n;
}

int gmb_textutil_apply_case(const char* value, bool upper, bool lower, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!value) { out[0] = '\0'; return 0; }
    int n = 0;
    for (const char* p = value; *p && n < out_cap - 1; p++)
    {
        char c = *p;
        if (upper && c >= 'a' && c <= 'z') c = (char)(c - 32);
        else if (lower && c >= 'A' && c <= 'Z') c = (char)(c + 32);
        out[n++] = c;
    }
    out[n] = '\0';
    return n;
}

bool gmb_textutil_is_printable(char c) { return c >= 32 && c < 127; }

int gmb_input_nav_process_key(int key, int* index, int num_of_fields)
{
    if (!index || num_of_fields <= 0) return 0;
    switch (key)
    {
        case GMB_K_BTAB:
        case GMB_K_UP:
            *index = *index == 1 ? num_of_fields : *index - 1;
            return 1;   /* Break */
        case GMB_K_TAB:
        case GMB_K_DOWN:
            *index = *index == num_of_fields ? 1 : *index + 1;
            return 1;
        case GMB_K_ENTER:
            if (*index == num_of_fields) return 2;   /* Exit */
            *index += 1;
            return 1;
        case GMB_K_ESC:
            return 2;
        case GMB_K_HOME:
            *index = 1;
            return 1;
        case GMB_K_END:
            *index = num_of_fields;
            return 1;
        default:
            return 0;   /* Continue */
    }
}

/* ===================== Config ===================== */

static gmb_config_t g_current_config = { false, ",", "." };

void gmb_config_init_default(gmb_config_t* c)
{
    if (!c) return;
    c->debug_mode = false;
    snprintf(c->decimal_separator, sizeof(c->decimal_separator), ",");
    snprintf(c->thousands_separator, sizeof(c->thousands_separator), ".");
}
gmb_config_t* gmb_config_current(void)
{
    return &g_current_config;
}
void gmb_config_make_current(const gmb_config_t* c)
{
    if (!c) return;
    g_current_config.debug_mode = c->debug_mode;
    snprintf(g_current_config.decimal_separator, sizeof(g_current_config.decimal_separator), "%s", c->decimal_separator);
    snprintf(g_current_config.thousands_separator, sizeof(g_current_config.thousands_separator), "%s", c->thousands_separator);
}
