/* gmb_regex_extra.h — parità CommonLib/Text/RegexWrapper.cs:103 via PCRE2 (isolato)
 * Non tocca gmb_tui_datatypes.c:1230 (POSIX). Include <pcre2.h> internamente.
 */
#ifndef GMB_REGEX_EXTRA_H
#define GMB_REGEX_EXTRA_H

#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#endif
#include <pcre2.h>

int gmb_regex_is_match(const char* pattern, const char* text);
int gmb_regex_match(const char* pattern, const char* text, char* out_value, int out_cap,
                    int* index_out, int* length_out);
int gmb_regex_matches_values(const char* pattern, const char* text, char out[][256], int cap);
int gmb_regex_group(const char* pattern, const char* text, int group_index, char* out, int out_cap);
int gmb_regex_replace(const char* pattern, const char* text, const char* replacement, char* out, int out_cap);
int gmb_regex_split(const char* pattern, const char* text, char out[][256], int cap);

/* ── API per-oggetto: pattern compilato UNA volta e riusato (handle opaco) ────────────────
 * Un valore REGEX BASIC conserva gmb_regex_t*; ogni metodo (IsMatch/Match/Matches/Replace/
 * Split) riusa il pattern compilato, senza ricompilare ad ogni chiamata. */
typedef struct gmb_regex gmb_regex_t;

gmb_regex_t* gmb_regex_new(const char* pattern);
void         gmb_regex_free(gmb_regex_t* re);
int          gmb_regex_set_pattern(gmb_regex_t* re, const char* pattern);  /* r.Pattern = "..." */
const char*  gmb_regex_pattern(const gmb_regex_t* re);                      /* r.Pattern */
int          gmb_regex_is_match_obj(const gmb_regex_t* re, const char* text);
int          gmb_regex_match_obj(const gmb_regex_t* re, const char* text, char* out_value, int out_cap,
                                 int* index_out, int* length_out);
int          gmb_regex_matches_full(const gmb_regex_t* re, const char* text,
                                    char out[][256], int* index_out, int* length_out, int cap);
int          gmb_regex_group_obj(const gmb_regex_t* re, const char* text, int group_index, char* out, int out_cap);
int          gmb_regex_replace_obj(const gmb_regex_t* re, const char* text, const char* replacement, char* out, int out_cap);
int          gmb_regex_split_obj(const gmb_regex_t* re, const char* text, char out[][256], int cap);

#endif
