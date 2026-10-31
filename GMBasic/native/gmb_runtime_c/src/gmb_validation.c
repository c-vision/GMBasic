/* FILE: gmb_validation.c — core validazione condiviso (Security D, 2026-09-02)
 * Parità con CommonLib.SecurityValidation (regole semplici/deterministiche). */

#include "gmb_runtime.h"
#include <ctype.h>
#include <string.h>

int gmb_validation_required(const char* s) {
    if (!s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    return *s != '\0';
}

static int ci_prefix(const char* s, const char* p) {
    /* case-insensitive: s inizia con p? */
    for (; *p; s++, p++) {
        char a = *s, b = *p;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

int gmb_validation_email(const char* s) {
    if (!s || !*s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    /* niente spazi interni */
    for (const char* p = s; *p; p++) if (isspace((unsigned char)*p)) return 0;
    const char* at = strchr(s, '@');
    if (!at || at == s || at[1] == '\0') return 0;
    const char* domain = at + 1;
    const char* dot = strchr(domain, '.');
    if (!dot || dot == domain || dot[1] == '\0') return 0;
    return 1;
}

int gmb_validation_url(const char* s) {
    if (!s || !*s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    for (const char* p = s; *p; p++) if (isspace((unsigned char)*p)) return 0;
    if (!(ci_prefix(s, "http://") || ci_prefix(s, "https://"))) return 0;
    return strchr(s, '.') != NULL;
}

int gmb_validation_credit_card(const char* s) {
    if (!s || !*s) return 0;
    int n = 0;
    for (const char* p = s; *p; p++) {
        if (isdigit((unsigned char)*p)) n++;
        else if (*p != ' ' && *p != '-') return 0;
    }
    if (n < 12 || n > 19) return 0;
    int sum = 0, alt = 0;
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
        char c = s[i];
        if (!isdigit((unsigned char)c)) continue;
        int d = c - '0';
        if (alt) { d *= 2; if (d > 9) d -= 9; }
        sum += d;
        alt = !alt;
    }
    return sum % 10 == 0;
}

int gmb_validation_length(const char* s, int min_len, int max_len) {
    int len = s ? (int)strlen(s) : 0;
    if (len < min_len) return 0;
    if (max_len >= 0 && len > max_len) return 0;
    return 1;
}
