/* ============================================================================
 * gmb_tui_input.h — Input helper di Clipper (specchio ClipperLib/Inputs).
 * Currency, TextUtil, ConfigChars/ConfigLocale, InputManagerNav.
 * ============================================================================ */
#ifndef GMB_TUI_INPUT_H
#define GMB_TUI_INPUT_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- ConfigChars / ConfigLocale (specchio Globals.cs) ---- */
#define GMB_INPUT_DIGITS        "0123456789"
#define GMB_INPUT_UPPER_ALPHA   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define GMB_INPUT_LOWER_ALPHA   "abcdefghijklmnopqrstuvwxyz"
#define GMB_INPUT_ALPHA         "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define GMB_INPUT_ACCENTED      "\xC3\xA0\xC3\xA1\xC3\xA8\xC3\xA9\xC3\xAC\xC3\xAD\xC3\xB2\xC3\xB3\xC3\xB9\xC3\xBA"
#define GMB_INPUT_PRINTABLES    " !#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\xC2\xB6"
#define GMB_INPUT_TEXT          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\xC2\xB6"

typedef struct gmb_input_locale {
    char decimal_separator;
    char thousands_separator;
    char bool_yes;
    char bool_no;
} gmb_input_locale_t;

/* Separators(european): european → ('.', ','); standard → (',', '.') */
void gmb_input_separators(bool european, char* out_thousands, char* out_decimal);

/* ---- Currency (specchio Currency.cs) ---- */
#define GMB_CURRENCY_DEFAULT_THOUSANDS '.'
#define GMB_CURRENCY_DEFAULT_DECIMAL ','

double gmb_currency_range_coerce(double value, double min, double max);
double gmb_currency_round_decimals(double value, int decimals);
/* Format(value, decimals, thousandsSep(0=default), decimalSep(0=default)).
   Scrive in out, ritorna len. */
int gmb_currency_format(double value, int decimals, char thousands_sep, char decimal_sep,
                        char* out, int out_cap);
/* TryParse: testo → double (separatore decimale leniente). Ritorna 1 ok. */
int gmb_currency_tryparse(const char* text, double* value, int* decimals,
                          char decimal_sep);
/* AdjustToSpace: coerce + tronca la stampa a maxLen char. */
double gmb_currency_adjust_to_space(double value, int max_len, double min_value, double max_value);

/* ---- TextUtil (specchio TextUtil.cs) ---- */
int gmb_textutil_allowed_chars(const char* value, const char* allowed, char* out, int out_cap);
int gmb_textutil_apply_case(const char* value, bool upper, bool lower, char* out, int out_cap);
bool gmb_textutil_is_printable(char c);

/* ---- InputManagerNav.ProcessKey (specchio Manager.cs) ----
   KeyAction: 0=Continue, 1=Break, 2=Exit. Ritorna l'azione. index 1-based. */
int gmb_input_nav_process_key(int key, int* index, int num_of_fields);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_INPUT_H */

/* ---- Config (specchio Inputs/Config.cs) ---- */
typedef struct gmb_config {
    bool debug_mode;
    char decimal_separator[8];
    char thousands_separator[8];
} gmb_config_t;
void gmb_config_init_default(gmb_config_t* c);
gmb_config_t* gmb_config_current(void);
void gmb_config_make_current(const gmb_config_t* c);
