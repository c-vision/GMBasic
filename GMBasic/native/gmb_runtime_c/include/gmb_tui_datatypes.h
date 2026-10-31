/* ============================================================================
 * gmb_tui_datatypes.h — Locale, Numbers, NumbersEx, Dates, Business, Strings,
 * StringsEx, Macros (specchio ClipperLib/Datatypes). Tutto puro, no OS.
 * ============================================================================ */
#ifndef GMB_TUI_DATATYPES_H
#define GMB_TUI_DATATYPES_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Locale (specchio Locale.cs) ---- */
typedef struct gmb_locale {
    char language[8];
    char country[8];
    char decimal_sep[4];
    char thousands_sep[4];
    char date_format[32];
    char time_format[32];
} gmb_locale_t;

gmb_locale_t gmb_locale_invariant(void);
gmb_locale_t gmb_locale_italian(void);
gmb_locale_t gmb_locale_us(void);

/* ---- Numbers (specchio Numbers.cs) ---- */
int gmb_numbers_format_int(long long v, const gmb_locale_t* loc, char* out, int cap);
int gmb_numbers_format_decimal(double v, int decimals, const gmb_locale_t* loc, char* out, int cap);
int gmb_numbers_format_currency(double v, const gmb_locale_t* loc, char* out, int cap);
int gmb_numbers_format_bytes(long long bytes, char* out, int cap);
/* FormatNumber: PICTURE stile Clipper, es. "99,999.99" con 1234.5 → "  1,234.50". */
int gmb_numbers_format_number(const char* picture, double value, const gmb_locale_t* loc, char* out, int cap);
int gmb_numbers_format_with_separators(double value, int decimals, const gmb_locale_t* loc, char* out, int cap);
/* TryParseNumber: ritorna 1 ok; separatori/decorazioni ignorati lenient. */
int gmb_numbers_try_parse_number(const char* picture, const char* text, double* value,
                                 const gmb_locale_t* loc);

/* ---- NumbersEx (specchio NumbersEx.cs) ---- */
int  gmb_numbersex_first_missing_positive(int* nums, int n);
long long gmb_numbersex_modular_exponent(long long n, long long x, long long p);
long long gmb_numbersex_gcd(long long a, long long b);
/* ModInverse via n^(p-2)%p (__int128 interno). Ritorna 0 se non coprimo. */
long long gmb_numbersex_mod_inverse(long long n, long long p);
/* ParseInteger: ritorna 1 ok. thousands_sep '\0' = default dal locale. */
int gmb_numbersex_parse_integer(const char* value, const gmb_locale_t* loc, char thousands_sep,
                                long long* out);
int gmb_numbersex_parse_currency(const char* value, const gmb_locale_t* loc,
                                 char thousands_sep, char decimal_sep, double* out);

/* ---- Dates (specchio Dates.cs) ---- */
typedef struct gmb_dt {
    int year, month, day, hour, minute, second;
} gmb_dt_t;

int  gmb_dates_format(const gmb_dt_t* dt, const char* format, char* out, int cap);
int  gmb_dates_parse(const char* s, const char* format, gmb_dt_t* out);
int  gmb_dates_days_in_month(int y, int m);
bool gmb_dates_is_leap_year(int y);
/* WeekDay: 0=Sunday ... 6=Saturday (Zeller). */
int  gmb_dates_weekday(int y, int m, int d);
long long gmb_dates_to_unix_seconds(const gmb_dt_t* dt);
gmb_dt_t gmb_dates_from_unix_seconds(long long s);
long long gmb_dates_to_unix_milliseconds(const gmb_dt_t* dt);
gmb_dt_t gmb_dates_from_unix_milliseconds(long long ms);
gmb_dt_t gmb_dates_add_months_safe(const gmb_dt_t* dt, int n);
/* ParseFlexible: prova formati comuni. Ritorna 1 ok. */
int  gmb_dates_parse_flexible(const char* s, gmb_dt_t* out);

/* ---- Business (specchio Business.cs) ---- */
bool gmb_business_is_valid_luhn(const char* code);

/* ---- Strings (specchio Strings.cs) ---- */
int  gmb_strings_repeat(char c, int n, char* out, int cap);
int  gmb_strings_pad_right(const char* s, int width, char pad, char* out, int cap);
int  gmb_strings_pad_left(const char* s, int width, char pad, char* out, int cap);
int  gmb_strings_truncate(const char* s, int max_len, char* out, int cap);
int  gmb_strings_reverse(const char* s, char* out, int cap);
int  gmb_strings_natural_compare(const char* a, const char* b);
int  gmb_strings_substring_safe(const char* s, int start, int len, char* out, int cap);
int  gmb_strings_truncate_middle(const char* s, int max, const char* ellipsis, char* out, int cap);
/* WordWrap: scrive righe separate da '\n'. Ritorna la lunghezza; righe in *lines. */
int  gmb_strings_word_wrap(const char* text, int width, char* out, int cap, int* lines);
bool gmb_strings_is_printable(const char* s);

/* ---- StringsEx (specchio StringsEx.cs) ---- */
bool gmb_stringsex_is_vowel(char c);
bool gmb_stringsex_is_newline(char c);
bool gmb_stringsex_is_whitespace(char c);
bool gmb_stringsex_is_letter(char c);
bool gmb_stringsex_is_ident_start(char c);
bool gmb_stringsex_is_ident_char(char c);
bool gmb_stringsex_is_ascii_printable(char c);
bool gmb_stringsex_is_non_printable(char c);
bool gmb_stringsex_is_valid_escape(const char* s);
bool gmb_stringsex_would_start_an_ident_seq(const char* s);
bool gmb_stringsex_is_start_a_number(const char* s);
bool gmb_stringsex_is_comment_start(const char* s);
bool gmb_stringsex_is_comment_end(const char* s);
/* CountOccurrences: riempie count[256]. Ritorna totali caratteri. */
int  gmb_stringsex_count_occurrences(const char* input, uint32_t count[256]);
/* CowReplace: sostituisce tutte le occorrenze. Ritorna len (0 se nulla). */
int  gmb_stringsex_cow_replace(const char* src, const char* pattern, const char* to,
                               char* out, int cap);
int  gmb_stringsex_cow_replace_n(const char* src, const char* pattern, const char* to,
                                 int count, char* out, int cap);
int  gmb_stringsex_cow_to_ascii_lowercase(const char* src, char* out, int cap);
int  gmb_stringsex_cow_to_ascii_uppercase(const char* src, char* out, int cap);
int  gmb_stringsex_extract_ascii_printables(const char* input, char* out, int cap);

/* ---- Macros (specchio Macros.cs: concat_string!/strcat!) ---- */
int gmb_macros_concat(const char* const* parts, int n, char* out, int cap);

/* ---- Iters (specchio Iters.cs: TakeUntilLastN / FlattenResults) ---- */
/* Copia tutti gli elementi tranne gli ultimi n. Ritorna il numero scritto. */
int gmb_iters_take_until_last_n(const int* src, int src_len, int n, int* out, int cap);
/* FlattenResults: concatena più array (list per list). Ritorna totale scritto. */
int gmb_iters_flatten_results(const int* const* lists, const int* lens, int nlists,
                              int* out, int cap);

/* ---- Regexes (specchio Regexes.cs: pattern comuni; estrazione via regex POSIX) ---- */
#define GMB_REGEX_DATE  "(?i)(?:[0-3]?[0-9](?:st|nd|rd|th)?[ ]+(?:of[ ]+)?(?:jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)[a-z]*|[0-3]?[0-9][-./][0-3]?[0-9][-./][0-9][0-9]?[0-9]?)"
#define GMB_REGEX_TIME  "(?i)[0-9]{1,2}:[0-9]{2}"
#define GMB_REGEX_EMAIL "([A-Za-z0-9!#$%&'*+/=?^_`{|}~.-]+@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?[.])+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)"
#define GMB_REGEX_IPV4  "(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)[.]){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
#define GMB_REGEX_HEXCOLOR "(?:#?([0-9a-fA-F]{6}|[0-9a-fA-F]{3}))"
#define GMB_REGEX_GUID  "[0-9a-fA-F]{8}-?[a-fA-F0-9]{4}-?[a-fA-F0-9]{4}-?[a-fA-F0-9]{4}-?[a-fA-F0-9]{12}"
#define GMB_REGEX_SSN   "(?:[0-9]{3}-[0-9]{2}-[0-9]{4})"
#define GMB_REGEX_MD5   "[0-9a-fA-F]{32}"
#define GMB_REGEX_SHA1  "[0-9a-fA-F]{40}"
#define GMB_REGEX_SHA256 "[0-9a-fA-F]{64}"
#define GMB_REGEX_MAC   "(([a-fA-F0-9]{2}[:-]){5}([a-fA-F0-9]{2}))"
#define GMB_REGEX_ISBN13 "(?:[0-9]-?){12}[0-9xX]"
#define GMB_REGEX_ISBN10 "(?:[0-9]-?){9}[0-9xX]"
#define GMB_REGEX_VISA   "4[0-9]{3}[ -]?[0-9]{4}[ -]?[0-9]{4}[ -]?[0-9]{4}"
#define GMB_REGEX_MASTERCARD "5[1-5][0-9]{2}[ -]?[0-9]{4}[ -]?[0-9]{4}[ -]?[0-9]{4}"
#define GMB_REGEX_CREDITCARD "(?:(?:(?:[0-9]{4}[- ]?){3}[0-9]{4}|[0-9]{15,16}))"

/* Estrae le occorrenze (match interi) del pattern POSIX nel testo.
   Ritorna il numero di match scritti in out (max cap match da max_len char).
   I pattern .NET non compatibili POSIX producono 0 match. */
int gmb_regexes_matches(const char* pattern, const char* text, char out[][128], int cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_DATATYPES_H */
