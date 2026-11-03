/* ============================================================================
 * gmb_bsp_compile.c — traduzione .bsp (HTML + <% %>) → BASIC.
 * Estratto da gmb_bsp.c per essere linkato ANCHE dal driver dei chunk
 * (gmb_chunk_driver): così il compilatore BSP in C è COMPLETO — la pagina
 * compilata viene eseguita dalla VM/C (A1), non dal valutatore di espressioni.
 * Nessuna dipendenza dal runtime: solo libc.
 * ============================================================================ */
#include "gmb_bsp.h"
#include "gmb_bsp_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int gmb_bsp_buf_grow(gmb_bsp_buf* b, size_t extra)
{
    if (b->n + extra + 1 <= b->cap) return 1;
    size_t nc = b->cap ? b->cap * 2 : 256;
    while (nc < b->n + extra + 1) nc *= 2;
    char* p = (char*)realloc(b->p, nc);
    if (!p) return 0;
    b->p = p;
    b->cap = nc;
    return 1;
}

void gmb_bsp_buf_app(gmb_bsp_buf* b, const char* s)
{
    if (!b || !s) return;
    size_t k = strlen(s);
    if (!gmb_bsp_buf_grow(b, k)) return;
    memcpy(b->p + b->n, s, k);
    b->n += k;
    b->p[b->n] = 0;
}

void gmb_bsp_buf_napp(gmb_bsp_buf* b, const char* s, size_t k)
{
    if (!b || !s || !k) return;
    if (!gmb_bsp_buf_grow(b, k)) return;
    memcpy(b->p + b->n, s, k);
    b->n += k;
    b->p[b->n] = 0;
}

void gmb_bsp_buf_ch(gmb_bsp_buf* b, char ch)
{
    if (!gmb_bsp_buf_grow(b, 1)) return;
    b->p[b->n++] = ch;
    b->p[b->n] = 0;
}

static void emit_html(gmb_bsp_buf* b, const char* s, size_t n)
{
    if (!s || !n) return;
    /* OGNI riga sorgente diventa `PRINT "..."` SENZA `;`: il newline va preservato.
     * Con `;` le righe venivano concatenate: dentro <script> i commenti `//` si
     * mangiavano il resto del codice (SyntaxError "Unexpected end of input") e i
     * <pre> perdevano le righe. Anche le righe vuote emettono il loro newline. */
    const char* line = s;
    const char* end = s + n;
    while (line < end) {
        const char* nl = (const char*)memchr(line, '\n', (size_t)(end - line));
        const char* le = nl ? nl : end;
        size_t ln = (size_t)(le - line);
        if (ln && line[ln - 1] == '\r') ln--;
        gmb_bsp_buf_app(b, "PRINT \"");
        for (size_t i = 0; i < ln; i++) {
            if (line[i] == '"') gmb_bsp_buf_app(b, "\"\"");
            else gmb_bsp_buf_ch(b, line[i]);
        }
        /* `;` solo sull'ULTIMO segmento senza newline (es. HTML prima di <%= %>):
         * il testo deve restare attaccato. Con newline: PRINT senza `;` → riga
         * preservata (commenti // dentro <script>, <pre>, ecc.). */
        gmb_bsp_buf_app(b, nl ? "\"\n" : "\";\n");
        line = nl ? nl + 1 : end;
    }
}

/* Le pagine BSP contengono commenti BASIC `' ...` dentro i blocchi <% %>. Il motore
 * di espressioni C (gmb_expr) usa `'` come delimitatore di STRINGA (idioma
 * Clipper/SQL), quindi un commento verrebbe letto come stringa non chiusa →
 * "expected expression". Qui si tolgono i commenti riga rispettando le stringhe
 * "..." (un apostrofo DENTRO una stringa resta). Il testo HTML resta intatto. */
static void buf_app_code(gmb_bsp_buf* b, const char* s, size_t n)
{
    size_t i = 0;
    int in_str = 0;
    while (i < n) {
        char ch = s[i];
        if (in_str) {
            gmb_bsp_buf_napp(b, &ch, 1);
            if (ch == '"') {
                if (i + 1 < n && s[i + 1] == '"') { gmb_bsp_buf_napp(b, &s[i + 1], 1); i += 2; continue; }
                in_str = 0;
            }
            i++;
            continue;
        }
        if (ch == '"') { in_str = 1; gmb_bsp_buf_napp(b, &ch, 1); i++; continue; }
        if (ch == '\'') {                       /* commento fino a fine riga */
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        gmb_bsp_buf_napp(b, &ch, 1);
        i++;
    }
}


static const char* bsp_trim(const char* s, size_t* n)
{
    while (*n && isspace((unsigned char)*s)) { s++; (*n)--; }
    while (*n && isspace((unsigned char)s[*n - 1])) (*n)--;
    return s;
}

/* full=0 → BASIC per gmb_expr (niente IMPORT: il valutatore non ha namespace).
 * full=1 → BASIC per il COMPILATORE completo (VM/C): emette le direttive IMPORT
 *          dichiarate dalla pagina, così `DASHBOARD.*`, `AI.*`, … si risolvono. */
static int bsp_compile_impl(const char* src, char** out_bas, int full)
{
    if (!out_bas) return GMB_EXPR_ERR_PARSE;
    *out_bas = NULL;
    if (!src) src = "";
    gmb_bsp_buf b;
    memset(&b, 0, sizeof(b));
    if (full) {
        /* Gli IMPORT vanno PRIMA di ogni statement (il linker li risolve a livello di
         * modulo): si raccolgono da TUTTE le direttive e si emettono in testa, poi il
         * corpo. Prima un `IMPORT` dopo un PRINT non veniva risolto (EmptyRowHtml). */
        gmb_bsp_buf_app(&b, "IMPORT HTTP\nIMPORT STRINGS\n");
        const char* d = src;
        while ((d = strstr(d, "<%@")) != NULL) {
            d += 3;
            const char* e = strstr(d, "%>");
            const char* end = e ? e : d + strlen(d);
            const char* q = d;
            while (q < end && isspace((unsigned char)*q)) q++;
            if (end - q >= 6 && !strncasecmp(q, "IMPORT", 6)) {
                q += 6;
                while (q < end) {
                    while (q < end && (isspace((unsigned char)*q) || *q == ',')) q++;
                    const char* ns = q;
                    while (q < end && (isalnum((unsigned char)*q) || *q == '_' || *q == '.')) q++;
                    if (q > ns) {
                        gmb_bsp_buf_app(&b, "IMPORT ");
                        gmb_bsp_buf_napp(&b, ns, (size_t)(q - ns));
                        gmb_bsp_buf_app(&b, "\n");
                    } else if (q < end && *q != ',') q++;
                }
            }
            d = e ? e + 2 : end;
        }
    }
    const char* p = src;
    while (*p) {
        const char* tag = strstr(p, "<%");
        if (!tag) {
            emit_html(&b, p, strlen(p));
            break;
        }
        if (tag > p) emit_html(&b, p, (size_t)(tag - p));
        p = tag + 2;
        if (*p == '@') {
            p++;
            const char* e = strstr(p, "%>");
            const char* end = e ? e : p + strlen(p);
            /* direttiva <%@ ... %>: solo consumata (gli IMPORT sono gia' in testa). */
            p = e ? e + 2 : end;
            continue;
        }
        if (p[0] == '-' && p[1] == '-') {
            const char* e = strstr(p, "--%>");
            p = e ? e + 4 : p + strlen(p);
            continue;
        }
        if (*p == '=') {
            p++;
            const char* e = strstr(p, "%>");
            size_t n = e ? (size_t)(e - p) : strlen(p);
            const char* expr = bsp_trim(p, &n);
            gmb_bsp_buf_app(&b, "PRINT ");
            if (n) gmb_bsp_buf_napp(&b, expr, n);
            gmb_bsp_buf_app(&b, ";\n");
            p = e ? e + 2 : p + n;
            continue;
        }
        const char* e = strstr(p, "%>");
        size_t n = e ? (size_t)(e - p) : strlen(p);
        if (n) {
            buf_app_code(&b, p, n);
            if (p[n - 1] != '\n') gmb_bsp_buf_app(&b, "\n");
        }
        p = e ? e + 2 : p + n;
    }
    if (!b.p) {
        b.p = (char*)calloc(1, 1);
        if (!b.p) return GMB_EXPR_ERR_PARSE;
    }
    *out_bas = b.p;
    return GMB_EXPR_OK;
}

int gmb_bsp_compile(const char* src, char** out_bas)
{
    return bsp_compile_impl(src, out_bas, 0);
}

int gmb_bsp_compile_full(const char* src, char** out_bas)
{
    return bsp_compile_impl(src, out_bas, 1);
}
