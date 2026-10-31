/* gmb_tui_mask.c — mask (estratto da gmb_tui.c per evitare monolite) */
#include "gmb_tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define GMB_MASK_MAX_SLOTS 256

struct gmb_mask {
    char* raw;
    char* template_body;
    gmb_mask_slot_t slots[GMB_MASK_MAX_SLOTS];
    int slot_count;
    bool force_uppercase;
    bool european_numbers;
    bool zero_as_blank;
    int visible_size;
    char fill_char;
    gmb_mask_datatype_t datatype;
};



static bool mask_char_is_digit(char c) { return c >= '0' && c <= '9'; }
static bool mask_char_is_letter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static bool mask_char_is_letter_or_digit(char c)
{
    return mask_char_is_digit(c) || mask_char_is_letter(c);
}
static bool mask_editable(char c)
{
    switch (c)
    {
        case 'A': case 'N': case 'X': case '9': case '#': case '0':
        case 'L': case 'Y': case '!': return true;
        default: return false;
    }
}
static char mask_toupper(char c) { return mask_char_is_letter(c) ? (char)(c - 32) : c; }

bool gmb_mask_validate_char(char c, char sym)
{
    switch (sym)
    {
        case 'A': return mask_char_is_letter(c);
        case 'N': return mask_char_is_letter_or_digit(c);
        case 'X': return true;
        case 'L': return c == 'T' || c == 'F' || c == 't' || c == 'f' || c == '1' || c == '0';
        case 'Y': return c == 'Y' || c == 'N' || c == 'y' || c == 'n';
        case '0': return mask_char_is_digit(c);
        case '9': return mask_char_is_digit(c) || c == '-' || c == '.' || c == ',';
        case '#': return mask_char_is_digit(c) || c == '-' || c == ' ' || c == '.' || c == ',';
        case '!': return mask_char_is_letter_or_digit(c);
        /* MatchChar/MatchDate/MatchDateTime: '.' e ',' come separatori decimali/migliaia */
        case '.': return c == '.' || c == ',';
        case ',': return c == ',' || c == '.';
        default: return false;
    }
}

bool gmb_mask_validate_char_ex(const gmb_mask_t* m, char c, char sym)
{
    if (!m) return gmb_mask_validate_char(c, sym);
    /* Per Float/Integer, '.' e ',' sono separatori decimali/migliaia — validi su 9/# */
    if ((sym == '9' || sym == '#' || sym == '0') && (m->datatype == GMB_MASK_FLOAT || m->datatype == GMB_MASK_INTEGER)) {
        if (c == '.' || c == ',') return true;
    }
    /* MatchDate/MatchDateTime/MatchTime: i separatori / - . : sono simboli validi
     * (C# Masked/Parser.cs:96-115 MatchDate/MatchDateTime symbols=true) */
    if ((m->datatype == GMB_MASK_DATE || m->datatype == GMB_MASK_TIME || m->datatype == GMB_MASK_DATETIME)) {
        if (sym == '/' || sym == '-' || sym == '.' || sym == ':')
            return c == sym || c == '/' || c == '-' || c == '.' || c == ':';
        if (sym == '9' || sym == '#') return mask_char_is_digit(c);
        if (sym == '0') return mask_char_is_digit(c);
    }
    /* Bool case-insensitive già sopra */
    return gmb_mask_validate_char(c, sym);
}

gmb_mask_t* gmb_mask_new(const char* mask)
{
    if (!mask) return NULL;
    gmb_mask_t* m = (gmb_mask_t*)calloc(1, sizeof(gmb_mask_t));
    if (!m) return NULL;
    m->raw = strdup(mask);
    m->fill_char = ' ';
    m->datatype = GMB_MASK_TEXT;

    /* Parsing esteso: "@fn" con @S<size>, @L<char>, e flag @F/@Z/@!/@R/@B etc. */
    const char* p = mask;
    char tmpl[GMB_MASK_MAX_SLOTS];
    int tn = 0;
    while (*p)
    {
        if (*p == '@' && p[1])
        {
            char fn = p[1];
            if (fn == 'S')
            {
                const char* d = p + 2;
                int sz = 0;
                while (mask_char_is_digit(*d)) { sz = sz * 10 + (*d - '0'); d++; }
                m->visible_size = sz;
                p = d;
                continue;
            }
            if (fn == 'L')
            {
                if (p[2]) { m->fill_char = p[2]; p += 3; continue; }
                p += 2;
                continue;
            }
            if (fn == 'F') { m->european_numbers = true; p += 2; continue; }
            if (fn == 'Z') { m->zero_as_blank = true; p += 2; continue; }
            if (fn == '!') m->force_uppercase = true;
            // Altri flag @R/@B/@A/@K/@S etc. ignorati per validazione ma consumati
            p += 2;
            continue;
        }
        if (tn < GMB_MASK_MAX_SLOTS - 1) tmpl[tn++] = *p;
        p++;
    }
    tmpl[tn] = '\0';
    m->template_body = strdup(tmpl);

    /* BuildSlots */
    for (int i = 0; i < tn; i++)
    {
        char c = tmpl[i];
        if (mask_editable(c))
        {
            m->slots[i].is_literal = false;
            m->slots[i].symbol = c;
            m->slots[i].literal = c;
            m->slots[i].force_upper = (c == '!');
        }
        else
        {
            m->slots[i].is_literal = true;
            m->slots[i].symbol = c;
            m->slots[i].literal = c;
        }
    }
    m->slot_count = tn;

    /* DetectDataType */
    if (tn == 0) m->datatype = GMB_MASK_TEXT;
    else if (tn == 1 && (tmpl[0] == 'L' || tmpl[0] == 'Y')) m->datatype = GMB_MASK_BOOL;
    else
    {
        bool all_num = true, has_dot = false;
        bool all_date = true, has_colon = false, has_space = false;
        for (int i = 0; i < tn; i++)
        {
            char c = tmpl[i];
            if (!strchr("09#$*,.() -", c)) all_num = false;
            if (c == '.' || c == ',') has_dot = true;
            if (!strchr("09#/-.: ", c)) all_date = false;
            if (c == ':') has_colon = true;
            if (c == ' ') has_space = true;
        }
        if (all_num)
            m->datatype = has_dot ? GMB_MASK_FLOAT : GMB_MASK_INTEGER;
        else if (all_date)
            m->datatype = has_colon ? (has_space ? GMB_MASK_DATETIME : GMB_MASK_TIME) : GMB_MASK_DATE;
        else
            m->datatype = GMB_MASK_TEXT;
    }
    return m;
}

void gmb_mask_free(gmb_mask_t* m)
{
    if (!m) return;
    free(m->raw);
    free(m->template_body);
    free(m);
}

int gmb_mask_slot_count(const gmb_mask_t* m) { return m ? m->slot_count : 0; }
const gmb_mask_slot_t* gmb_mask_slot(const gmb_mask_t* m, int i)
{ return (m && i >= 0 && i < m->slot_count) ? &m->slots[i] : NULL; }
gmb_mask_datatype_t gmb_mask_datatype(const gmb_mask_t* m) { return m ? m->datatype : GMB_MASK_TEXT; }
bool gmb_mask_force_uppercase(const gmb_mask_t* m) { return m ? m->force_uppercase : false; }
int gmb_mask_visible_size(const gmb_mask_t* m) { return m ? m->visible_size : 0; }
char gmb_mask_fill_char(const gmb_mask_t* m) { return m ? m->fill_char : ' '; }

bool gmb_mask_is_valid_char(const gmb_mask_t* m, int pos, char c)
{
    if (!m || pos < 0 || pos >= m->slot_count) return false;
    if (m->slots[pos].is_literal) return false;
    return gmb_mask_validate_char_ex(m, c, m->slots[pos].symbol);
}

char gmb_mask_transform(const gmb_mask_t* m, int pos, char c)
{
    if (!m || pos < 0 || pos >= m->slot_count) return c;
    const gmb_mask_slot_t* s = &m->slots[pos];
    if ((m->force_uppercase || s->force_upper) && mask_char_is_letter(c))
        return mask_toupper(c);
    return c;
}

int gmb_mask_apply(const gmb_mask_t* m, const char* input, char* out, int out_cap)
{
    if (!m || !out || out_cap <= 0) return 0;
    int si = 0, vi = 0;
    int written = 0;
    const char* in = input ? input : "";
    while (si < m->slot_count)
    {
        const gmb_mask_slot_t* s = &m->slots[si];
        if (s->is_literal)
        {
            if (written + 1 >= out_cap) break;
            out[written++] = s->literal;
            si++;
            continue;
        }
        if (in[vi])
        {
            char c = in[vi];
            if (gmb_mask_validate_char_ex(m, c, s->symbol))
            {
                if (written + 1 >= out_cap) break;
                out[written++] = gmb_mask_transform(m, si, c);
                vi++;
                si++;
                continue;
            }
            vi++;   /* reject char, keep trying */
            continue;
        }
        if (written + 1 >= out_cap) break;
        out[written++] = ' ';
        si++;
    }
    out[written] = '\0';
    return written;
}

int gmb_mask_empty(const gmb_mask_t* m, char* out, int out_cap)
{
    return gmb_mask_apply(m, "", out, out_cap);
}

int gmb_mask_process_key(const gmb_mask_t* m, const char* text, int cursor, char key,
                         char* out, int out_cap, int* out_cursor)
{
    if (!m || !out || out_cap <= 0) return 0;
    int n = m->slot_count;
    const char* t = text ? text : "";

    if (key == '\b')
    {
        int p = cursor - 1;
        while (p >= 0 && m->slots[p].is_literal) p--;
        if (p < 0) { out[0] = '\0'; if (out_cursor) *out_cursor = cursor; return 0; }
        int len = (int)strlen(t);
        char* arr = (char*)malloc((size_t)len + 1);
        if (!arr) { out[0] = '\0'; if (out_cursor) *out_cursor = cursor; return 0; }
        memcpy(arr, t, (size_t)len + 1);
        if (p < len) arr[p] = ' ';
        memcpy(out, arr, (size_t)len + 1);
        free(arr);
        if (out_cursor) *out_cursor = p;
        return len;
    }

    int pos = cursor;
    while (pos < n && m->slots[pos].is_literal) pos++;
    if (pos >= n) { out[0] = '\0'; if (out_cursor) *out_cursor = cursor; return 0; }
    if (!gmb_mask_is_valid_char(m, pos, key))
    { out[0] = '\0'; if (out_cursor) *out_cursor = cursor; return 0; }

    /* text può essere più corto di n (usa Empty come base) */
    int base_len = (int)strlen(t);
    if (base_len < n)
    {
        char empty[GMB_MASK_MAX_SLOTS];
        int el = gmb_mask_apply(m, "", empty, (int)sizeof(empty));
        char* merged = (char*)malloc((size_t)el + 1);
        if (!merged) { out[0] = '\0'; if (out_cursor) *out_cursor = cursor; return 0; }
        memcpy(merged, empty, (size_t)el + 1);
        if (base_len > 0) memcpy(merged, t, (size_t)base_len);
        memcpy(out, merged, (size_t)el + 1);
        free(merged);
        if (pos < el) out[pos] = gmb_mask_transform(m, pos, key);
        int nc = pos + 1;
        while (nc < n && m->slots[nc].is_literal) nc++;
        if (out_cursor) *out_cursor = nc;
        return el;
    }
    else
    {
        int len = (int)strlen(t);
        char* a = (char*)malloc((size_t)len + 1);
        if (!a) { out[0] = '\0'; if (out_cursor) *out_cursor = cursor; return 0; }
        memcpy(a, t, (size_t)len + 1);
        if (pos < len) a[pos] = gmb_mask_transform(m, pos, key);
        memcpy(out, a, (size_t)len + 1);
        free(a);
        int nc = pos + 1;
        while (nc < n && m->slots[nc].is_literal) nc++;
        if (out_cursor) *out_cursor = nc;
        return len;
    }
}
