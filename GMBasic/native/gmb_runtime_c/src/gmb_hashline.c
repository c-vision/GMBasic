/* gmb_hashline.c — Hash-anchored edit (Hashline) — Orchestrazione agenti (A10), mondo C.
 *
 * Ogni riga è taggata `N#HASH| contenuto` (hash FNV-1a 32-bit, 4 cifre esadecimali maiuscole):
 * l'edit riferisce il tag ed è rifiutato se il contenuto è cambiato (hash mismatch). Parita'
 * con Basic.Agents.HashlineTool (C#): stesso formato tag, stesso hash. Runtime CONDIVISO (§0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

static unsigned int gmb_hashline_hash(const char* line)
{
    unsigned int h = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)(line ? line : ""); *p; p++)
    {
        h ^= (unsigned int)*p;
        h *= 16777619u;
    }
    return h;
}

/* Copia una riga trimmando il '\r' finale (in un buffer fornito). */
static void gmb_hashline_copy_line(const char* s, size_t len, char* out, size_t cap)
{
    while (len > 0 && (s[len - 1] == '\r')) len--;
    if (len >= cap) len = cap - 1;
    memcpy(out, s, len);
    out[len] = '\0';
}

char* gmb_hashline_tag(const char* content)
{
    const char* c = content ? content : "";
    size_t cap = strlen(c) + 64;
    /* ogni riga aggiunge fino a 8 char di tag */
    size_t nl = 0; for (const char* p = c; *p; p++) if (*p == '\n') nl++;
    cap += (nl + 1) * 8;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t off = 0;
    int n = 1;
    const char* line = c;
    for (;;)
    {
        const char* nlpos = strchr(line, '\n');
        size_t len = nlpos ? (size_t)(nlpos - line) : strlen(line);
        char buf[4096];
        gmb_hashline_copy_line(line, len, buf, sizeof(buf));
        off += (size_t)snprintf(out + off, cap - off, "%d#%04X| %s", n, gmb_hashline_hash(buf), buf);
        if (!nlpos) break;
        off += (size_t)snprintf(out + off, cap - off, "\n");
        line = nlpos + 1;
        n++;
    }
    return out;
}

/* Parsa "N#HASH" (o "N"). Ritorna N; *want riceve l'hash (fino a 16 char) o "". */
static int gmb_hashline_parse_ref(const char* ref, char* want, size_t wcap)
{
    want[0] = '\0';
    if (!ref) return 0;
    const char* s = ref;
    while (*s == ' ' || *s == '\t') s++;
    char numbuf[32]; size_t ni = 0;
    while (*s >= '0' && *s <= '9' && ni < sizeof(numbuf) - 1) numbuf[ni++] = *s++;
    numbuf[ni] = '\0';
    if (*s == '#')
    {
        s++;
        size_t wi = 0;
        while (*s && *s != ' ' && *s != '\t' && *s != '|' && wi < wcap - 1) want[wi++] = *s++;
        want[wi] = '\0';
    }
    return ni > 0 ? atoi(numbuf) : 0;
}

char* gmb_hashline_edit(const char* content, const char* ref, const char* new_text)
{
    const char* c = content ? content : "";
    /* Conta le righe. */
    int total = 1; for (const char* p = c; *p; p++) if (*p == '\n') total++;
    char want[16];
    int n = gmb_hashline_parse_ref(ref, want, sizeof(want));
    if (n < 1 || n > total)
    {
        char* e = (char*)malloc(96);
        if (e) snprintf(e, 96, "Error: riga %d fuori range (1..%d)", n, total);
        return e;
    }
    /* Verifica hash. */
    if (want[0])
    {
        const char* line = c; int idx = 1;
        while (idx < n) { const char* nl = strchr(line, '\n'); if (!nl) break; line = nl + 1; idx++; }
        const char* nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        char buf[4096]; gmb_hashline_copy_line(line, len, buf, sizeof(buf));
        char have[16]; snprintf(have, sizeof(have), "%04X", gmb_hashline_hash(buf));
        if (strcasecmp(have, want) != 0)
        {
            char* e = (char*)malloc(128);
            if (e) snprintf(e, 128, "Error: hash mismatch riga %d (atteso %s, trovato %s)", n, want, have);
            return e;
        }
    }
    /* Ricostruisci sostituendo la riga n. */
    size_t cap = strlen(c) + strlen(new_text ? new_text : "") + 16;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t off = 0;
    const char* line = c; int idx = 1;
    for (;;)
    {
        const char* nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        if (idx == n)
        {
            const char* nt = new_text ? new_text : "";
            size_t nl2 = strlen(nt);
            memcpy(out + off, nt, nl2); off += nl2;
        }
        else
        {
            char buf[4096]; gmb_hashline_copy_line(line, len, buf, sizeof(buf));
            size_t bl = strlen(buf);
            memcpy(out + off, buf, bl); off += bl;
        }
        if (!nl) break;
        out[off++] = '\n';
        line = nl + 1;
        idx++;
    }
    out[off] = '\0';
    return out;
}

int gmb_hashline_verify(const char* content, const char* ref)
{
    const char* c = content ? content : "";
    int total = 1; for (const char* p = c; *p; p++) if (*p == '\n') total++;
    char want[16];
    int n = gmb_hashline_parse_ref(ref, want, sizeof(want));
    if (n < 1 || n > total) return 0;
    if (!want[0]) return 1;
    const char* line = c; int idx = 1;
    while (idx < n) { const char* nl = strchr(line, '\n'); if (!nl) break; line = nl + 1; idx++; }
    const char* nl = strchr(line, '\n');
    size_t len = nl ? (size_t)(nl - line) : strlen(line);
    char buf[4096]; gmb_hashline_copy_line(line, len, buf, sizeof(buf));
    char have[16]; snprintf(have, sizeof(have), "%04X", gmb_hashline_hash(buf));
    return strcasecmp(have, want) == 0 ? 1 : 0;
}
