/* gmb_docfields.c — F2 multimodale (TODO Multimodal-Runtime): estrazione strutturata
 * documenti → JSON, condivisa da VM/C, C99Native (§0bis).
 *
 * gmb_docfields_json(path): euristiche DETERMINISTICHE (PCRE2, locale IT: date dd/mm/aaaa,
 * decimali con virgola) sul testo di gmb_doc_extract (+ .txt/.md letti diretti):
 * {"number","date","total","vat"} con null per i campi assenti. Offline, nessun modello.
 * gmb_docextract_json(path, schema): stesso testo + prompt vincolato allo schema →
 * gmb_llm_complete (provider %CHAT attivo; senza provider o senza testo = errore esplicito).
 * Errori come {"error":"..."} (mai stub, mai stringhe vuote silenziose).
 * Ritorno in buffer statico (come gmb_ai_ocr_read): i backend copiano, NESSUN free.
 */
#include "gmb_runtime.h"
#include "gmb_doc.h"
#include "gmb_regex_extra.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOCF_MAX_TEXT 12000

static char g_docf_err[512];
static char g_docf_out[16384];

/* Testo del documento: lettura diretta per i testuali (stesso ordine del C#),
 * gmb_doc_extract per i binari (PDF...). Heap in entrambi i casi (free del chiamante). */
static int is_text_ext(const char* path)
{
    size_t n = strlen(path);
    const char* e = n >= 4 ? path + n - 4 : "";
    const char* e5 = n >= 5 ? path + n - 5 : "";
    if (e[0] != '.') return 0;
    if ((e[1] == 't' || e[1] == 'T') && (e[2] == 'x' || e[2] == 'X') && (e[3] == 't' || e[3] == 'T')) return 1;
    if ((e[1] == 'm' || e[1] == 'M') && (e[2] == 'd' || e[2] == 'D')) return 1;
    if ((e[1] == 'c' || e[1] == 'C') && (e[2] == 's' || e[2] == 'S') && (e[3] == 'v' || e[3] == 'V')) return 1;
    if (e5[0] == '.' && (e5[1] == 'l' || e5[1] == 'L') && (e5[2] == 'o' || e5[2] == 'O') && (e5[3] == 'g' || e5[3] == 'G')) return 1;
    return 0;
}

static char* docf_text(const char* path)
{
    size_t n;
    if (!path || !path[0]) return NULL;
    n = strlen(path);
    (void)n;
    if (is_text_ext(path))
    {
        /* .txt/.md/.csv/.log: lettura diretta. */
        FILE* f = fopen(path, "rb");
        char* buf;
        size_t r;
        long sz;
        if (!f) return NULL;
        fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz < 0 || sz > 4 * 1024 * 1024) { fclose(f); return NULL; }
        buf = (char*)malloc((size_t)sz + 1);
        if (!buf) { fclose(f); return NULL; }
        r = fread(buf, 1, (size_t)sz, f);
        fclose(f);
        buf[r] = '\0';
        return buf;
    }
    return gmb_doc_extract(path);
}

static int has_digit(const char* s)
{
    for (; *s; s++) if (isdigit((unsigned char)*s)) return 1;
    return 0;
}

/* Copia con JSON-escape minimale (virgolette, backslash, controlli). */
static void docf_emit_str(char* dst, size_t cap, size_t* at, const char* s)
{
    size_t i = *at;
    if (i + 1 < cap) dst[i++] = '"';
    for (; *s && i + 6 < cap; s++)
    {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { dst[i++] = '\\'; dst[i++] = (char)c; }
        else if (c == '\n') { dst[i++] = '\\'; dst[i++] = 'n'; }
        else if (c == '\r') { dst[i++] = '\\'; dst[i++] = 'r'; }
        else if (c == '\t') { dst[i++] = '\\'; dst[i++] = 't'; }
        else if (c < 0x20) { i += (size_t)snprintf(dst + i, cap - i, "\\u%04x", c); }
        else dst[i++] = (char)c;
    }
    if (i + 1 < cap) dst[i++] = '"';
    dst[i < cap ? i : cap - 1] = '\0';
    *at = i;
}

/* "1.234,56" → "1234.56" (ritorna 1 se numerico), altrimenti copia raw. */
static void docf_norm_total(const char* raw, char* out, size_t cap)
{
    char tmp[128];
    size_t n = 0, i;
    int has_comma = 0;
    for (i = 0; raw[i] && n + 1 < sizeof(tmp); i++)
    {
        if (raw[i] == ',') has_comma = 1;
        if (raw[i] != ' ' && raw[i] != '\'') tmp[n++] = raw[i];
    }
    tmp[n] = '\0';
    if (has_comma)
    {
        /* IT: punti = migliaia, virgola = decimali. */
        char num[128];
        size_t m = 0;
        for (i = 0; tmp[i] && m + 1 < sizeof(num); i++)
        {
            if (tmp[i] == '.') continue;
            num[m++] = (tmp[i] == ',') ? '.' : tmp[i];
        }
        num[m] = '\0';
        snprintf(out, cap, "%s", num);
    }
    else snprintf(out, cap, "%s", tmp);
}

const char* gmb_docfields_json(const char* path)
{
    static const char* P_NUMBER = "(?i)fattura[^0-9\\n]{0,12}([A-Za-z0-9][A-Za-z0-9\\/\\-.]*\\d[A-Za-z0-9\\/\\-.]*)";
    static const char* P_DATE = "(\\d{1,2})[\\/\\.\\-](\\d{1,2})[\\/\\.\\-](\\d{2,4})";
    static const char* P_TOTAL = "(?i)(?:totale(?:\\s+(?:documento|fattura|complessivo|ivato))?|importo\\s+totale)\\s*[:€]?\\s*€?\\s*([0-9][\\d.]*,\\d{2}|\\d[\\d.,]*)";
    static const char* P_VAT = "(?i)(?:p\\.?\\s*iva|partita\\s+iva)[^.0-9]{0,10}(\\d{11})";
    char* text = docf_text(path);
    char number[64] = "", date[16] = "", total[64] = "", vat[16] = "";
    char grp[256];
    size_t at = 0;
    if (!text) { snprintf(g_docf_err, sizeof(g_docf_err), "{\"error\":\"file not found\"}"); return g_docf_err; }
    if (gmb_regex_group(P_NUMBER, text, 1, grp, sizeof(grp)) && has_digit(grp))
        snprintf(number, sizeof(number), "%s", grp);
    {
        char d[16], m[16], y[16];
        if (gmb_regex_group(P_DATE, text, 1, d, sizeof(d)) &&
            gmb_regex_group(P_DATE, text, 2, m, sizeof(m)) &&
            gmb_regex_group(P_DATE, text, 3, y, sizeof(y)))
            snprintf(date, sizeof(date), "%02d/%02d/%s", atoi(d), atoi(m), y);
    }
    if (gmb_regex_group(P_TOTAL, text, 1, grp, sizeof(grp)))
    {
        char norm[64];
        docf_norm_total(grp, norm, sizeof(norm));
        snprintf(total, sizeof(total), "%s", norm);
    }
    if (gmb_regex_group(P_VAT, text, 1, grp, sizeof(grp)))
        snprintf(vat, sizeof(vat), "%s", grp);
    free(text);
    g_docf_out[0] = '\0';
    at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, "{\"number\":");
    if (number[0]) docf_emit_str(g_docf_out, sizeof(g_docf_out), &at, number);
    else at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, "null");
    at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, ",\"date\":");
    if (date[0]) docf_emit_str(g_docf_out, sizeof(g_docf_out), &at, date);
    else at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, "null");
    at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, ",\"total\":");
    if (total[0]) at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, "%s", total);
    else at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, "null");
    at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, ",\"vat\":");
    if (vat[0]) docf_emit_str(g_docf_out, sizeof(g_docf_out), &at, vat);
    else at += (size_t)snprintf(g_docf_out + at, sizeof(g_docf_out) - at, "null");
    snprintf(g_docf_out + at, sizeof(g_docf_out) - at, "}");
    return g_docf_out;
}

const char* gmb_docextract_json(const char* path, const char* schema)
{
    char* text = docf_text(path);
    char* clip;
    char* answer;
    char sys[1024];
    size_t n;
    if (!text) { snprintf(g_docf_err, sizeof(g_docf_err), "{\"error\":\"file not found\"}"); return g_docf_err; }
    if (!text[0]) { free(text); snprintf(g_docf_err, sizeof(g_docf_err), "{\"error\":\"testo non estraibile\"}"); return g_docf_err; }
    n = strlen(text);
    if (n > DOCF_MAX_TEXT) text[DOCF_MAX_TEXT] = '\0';
    snprintf(sys, sizeof(sys),
        "Estrai dal documento i campi dello schema JSON: %s. "
        "Rispondi SOLO con un oggetto JSON valido (niente markdown, niente spiegazioni); "
        "valori assenti = null; numeri con punto decimale.",
        schema && schema[0] ? schema : "{\"text\":\"string\"}");
    answer = gmb_llm_complete(sys, text);
    free(text);
    if (!answer) { snprintf(g_docf_err, sizeof(g_docf_err), "{\"error\":\"nessun provider LLM configurato (AI SET ACTIVE)\"}"); return g_docf_err; }
    /* Parità IlRuntime.AiDocExtract: senza un provider %CHAT REALE (o con provider
     * "fixture") il completamento non produce testo → errore JSON. Prima gmb_llm_complete
     * ritornava il sentinella "Error: nessun provider LLM configurato (AI SET ACTIVE)"
     * (NoCompleter) e il JSON risultava malformato ("Error: ..." vs {"error":...}). */
    if (strcmp(answer, "Error: nessun provider LLM configurato (AI SET ACTIVE)") == 0) {
        free(answer);
        snprintf(g_docf_err, sizeof(g_docf_err), "{\"error\":\"nessun provider LLM configurato (AI SET ACTIVE)\"}");
        return g_docf_err;
    }
    /* Togli eventuali fence ```json ... ```. */
    clip = answer;
    while (*clip == ' ' || *clip == '\n' || *clip == '\r' || *clip == '\t') clip++;
    if (strncmp(clip, "```", 3) == 0)
    {
        char* nl = strchr(clip, '\n');
        char* end;
        clip = nl ? nl + 1 : clip + 3;
        end = strstr(clip, "```");
        if (end) *end = '\0';
    }
    snprintf(g_docf_out, sizeof(g_docf_out), "%s", clip);
    free(answer);
    return g_docf_out;
}
