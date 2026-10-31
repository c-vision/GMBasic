/* ============================================================================
 * gmb_tui_fuzzy.c — gmb_tui_fuzzy. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Fuzzy ===================== */

enum
{
    GMB_FUZZY_BONUS_MATCH = 50000,
    GMB_FUZZY_BONUS_EXACT = 1000,
    GMB_FUZZY_BONUS_START = 10,
    GMB_FUZZY_BONUS_START_WORD = 5,
    GMB_FUZZY_BONUS_CAND_LEN = -1,
    GMB_FUZZY_BONUS_MATCH_LEN = -10,
    GMB_FUZZY_BONUS_HOLES = -30,
    GMB_FUZZY_BONUS_SINGLED = -15
};

static bool gmb_fuzzy_is_word_sep(char c) { return c == '_' || c == ' ' || c == '-'; }

static char gmb_fuzzy_norm_char(unsigned char c)
{
    switch (c)
    {
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return 'a';
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: return 'e';
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'i';
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: return 'o';
        case 0xD9: case 0xDA: case 0xDB: case 0xDC: return 'u';
        default:
            if (c >= 'A' && c <= 'Z') return (char)(c + 32);
            if (c >= 0xE0 && c <= 0xE5) return 'a';
            if (c >= 0xE8 && c <= 0xEB) return 'e';
            if (c >= 0xEC && c <= 0xEF) return 'i';
            if (c >= 0xF2 && c <= 0xF6) return 'o';
            if (c >= 0xF9 && c <= 0xFC) return 'u';
            return (char)c;
    }
}

static int gmb_fuzzy_max_holes(int len)
{
    switch (len)
    {
        case 1: return 0;
        case 2: return 1;
        case 3: return 2;
        case 4: return 2;
        case 5: return 3;
        case 6: return 3;
        default: return len * 4 / 7;
    }
}

static int gmb_fuzzy_normalize(const char* s, char* out, int cap)
{
    int n = 0;
    for (const char* p = s; *p && n < cap - 1; p++)
    {
        char c = gmb_fuzzy_norm_char((unsigned char)*p);
        if (c != 0) out[n++] = c;
    }
    out[n] = 0;
    return n;
}

gmb_fuzzy_result_t gmb_fuzzy_score(const char* query, const char* candidate)
{
    gmb_fuzzy_result_t res = { 0, 0 };
    char q[256], cand[1024];
    int qn = gmb_fuzzy_normalize(query, q, 256);
    int cn = gmb_fuzzy_normalize(candidate, cand, 1024);
    if (qn == 0) { res.score = GMB_FUZZY_BONUS_MATCH; res.matched = 1; return res; }
    if (cn < qn) return res;

    int pos[256];
    int best_score = 0;
    int n = cn + 1 - qn;
    for (int start = 0; start < n; start++)
    {
        if (cand[start] != q[0]) continue;
        int cand_idx = start, pat_idx = 0;
        while (pat_idx < qn && cand_idx < cn)
        {
            if (cand[cand_idx] == q[pat_idx]) { pos[pat_idx] = cand_idx; pat_idx++; }
            cand_idx++;
        }
        if (pat_idx != qn) continue;

        int score = GMB_FUZZY_BONUS_MATCH + GMB_FUZZY_BONUS_CAND_LEN * cn;
        int nb_holes = 0;
        for (int i = 1; i < qn; i++)
            if (pos[i] > 1 + pos[i - 1]) nb_holes++;
        if (nb_holes > gmb_fuzzy_max_holes(qn)) continue;

        score += GMB_FUZZY_BONUS_HOLES * nb_holes;
        int match_len = 1 + cand_idx - pos[0];
        score += match_len * GMB_FUZZY_BONUS_MATCH_LEN;

        int perfect = 0;
        if (pos[0] == 0)
        {
            score += GMB_FUZZY_BONUS_START + GMB_FUZZY_BONUS_START_WORD;
            if (cn == qn) { score += GMB_FUZZY_BONUS_EXACT; perfect = 1; }
        }
        else if (gmb_fuzzy_is_word_sep(cand[pos[0] - 1]))
        {
            score += GMB_FUZZY_BONUS_START_WORD;
            if (cn - pos[0] == qn) perfect = 1;
        }

        if (score > best_score)
        {
            best_score = score;
            res.matched = 1;
            if (perfect) break;
        }
    }

    res.score = res.matched ? best_score : 0;
    return res;
}

bool gmb_fuzzy_matches(const char* query, const char* candidate)
{
    return gmb_fuzzy_score(query, candidate).matched;
}

int gmb_fuzzy_filter(const char* query, const char* const* items, int count, int* indexes, int max_results)
{
    char q[256];
    gmb_fuzzy_normalize(query, q, 256);
    if (q[0] == 0)
    {
        int n = count < max_results ? count : max_results;
        for (int i = 0; i < n; i++) indexes[i] = i;
        return n;
    }
    int n = 0;
    for (int i = 0; i < count && n < max_results; i++)
    {
        gmb_fuzzy_result_t r = gmb_fuzzy_score(query, items[i]);
        if (r.matched) indexes[n++] = i;
    }
    return n;
}
