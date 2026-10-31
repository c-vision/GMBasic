/* native/repl/gmb_lsp.c — Language Server (LSP) per GMBASIC, in C (modalità --lsp).
 * JSON-RPC 2.0 su stdio con Content-Length framing (come richiesto da LSP).
 * Implementati: initialize/initialized/shutdown/exit, textDocument/didOpen, didChange,
 * didClose. publishDiagnostics viene inviato (per ora vuoto) — la diagnostica reale si
 * aggancia a gmb_lexer/gmb_parser quando pronti.
 */
#include "gmb_lsp.h"
#include "gmb_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ── mini JSON-RPC ─────────────────────────────────────────────── */

/* Trova la stringa json: "<key>" e, dopo i ':', ne estrae il valore stringa. */
static int json_get_string(const char* js, const char* key, char* out, int cap)
{
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(js, pat);
    if (!p) return 0;
    p = strchr(p + 1, ':');
    if (!p) return 0;
    p = strchr(p, '"');
    if (!p) return 0;
    p++;
    int n = 0;
    while (*p && *p != '"' && n < cap - 1) out[n++] = *p++;
    out[n] = 0;
    return 1;
}

/* Estrae l'id JSON-RPC (numero o stringa) come token raw, fino a ',' o '}'. */
static void json_get_id(const char* js, char* out, int cap)
{
    const char* p = strstr(js, "\"id\"");
    if (!p) { out[0] = 0; return; }
    p = strchr(p + 1, ':');
    if (!p) { out[0] = 0; return; }
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    int n = 0;
    while (*p && *p != ',' && *p != '}' && n < cap - 1) out[n++] = *p++;
    /* trima whitespace in coda */
    while (n > 0 && isspace((unsigned char)out[n - 1])) n--;
    out[n] = 0;
}

/* ── framing Content-Length ────────────────────────────────────── */

/* Legge una riga "Content-Length: N". Ritorna N o -1 su EOF/errore. */
static long read_content_length(void)
{
    char line[256];
    long len = -1;
    while (fgets(line, sizeof(line), stdin))
    {
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) break; /* fine header */
        if (strncasecmp(line, "Content-Length:", 15) == 0)
            len = atol(line + 15);
    }
    return len;
}

static int read_body(char* buf, int cap, long len)
{
    if (len <= 0 || len >= cap) return 0;
    size_t got = fread(buf, 1, (size_t)len, stdin);
    buf[got] = 0;
    return (long)got == len;
}

static void write_message(const char* body)
{
    printf("Content-Length: %zu\r\n\r\n%s", strlen(body), body);
    fflush(stdout);
}

/* ── risposte JSON-RPC ─────────────────────────────────────────── */

static void send_result(const char* id, const char* result)
{
    char buf[8192];
    snprintf(buf, sizeof(buf), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}", id, result);
    write_message(buf);
}

static void send_error(const char* id, int code, const char* msg)
{
    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
        id, code, msg);
    write_message(buf);
}

/* ── documenti aperti + completion ─────────────────────────────── */
static char g_lsp_doc[65536] = "";   /* testo dell'ultimo documento aperto/cambiato */

/* estrae un int JSON dopo la chiave (prima occorrenza). */
static int json_get_int(const char* js, const char* key, long* out)
{
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(js, pat);
    if (!p) return 0;
    p = strchr(p + 1, ':');
    if (!p) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '-' && (*p < '0' || *p > '9')) return 0;
    *out = atol(p);
    return 1;
}

/* keyword BASIC comuni per completion (subset rappresentativo del linguaggio). */
static const char* const g_lsp_keywords[] = {
    "DIM","PRINT","LET","IF","THEN","ELSE","ELSEIF","END","FOR","NEXT","TO","STEP",
    "WHILE","WEND","DO","LOOP","UNTIL","SELECT","CASE","SUB","FUNCTION","RETURN",
    "GOTO","GOSUB","ON","ERROR","RESUME","EXIT","CONTINUE","AND","OR","NOT","MOD",
    "SQR","ABS","EXP","LOG","SIN","COS","TAN","ATN","INT","FIX","ROUND","RND",
    "STR","VAL","LEN","LEFT","RIGHT","MID","CHR","ASC","INSTR","LOWER","UPPER",
    "TRIM","LTRIM","RTRIM","CINT","CLNG","CDBL","CSNG","CSTR","CBOOL","BYTE",
    "UBOUND","LBOUND","REDIM","TYPE","CLASS","NAMESPACE","IMPORT","PUBLIC","PRIVATE",
    "STATIC","CONST","NEW","USING","TRY","CATCH","THROW","INHERITS","SELF","BASE",
    "AS","ARRAY","LIST","DICT","NEW","IMPORT","NEW",NULL };

/* token (identificatore/parola) alla posizione data nel documento. */
static void lsp_identifier_at(const char* text, long line0, long char0, char* out, int cap)
{
    out[0] = 0;
    long cur_line = 0, cur_col = 0;
    long start = -1, end = -1;
    const char* p = text;
    while (*p)
    {
        if (cur_line == line0 && cur_col == char0) { start = (long)(p - text); break; }
        if (*p == '\n') { cur_line++; cur_col = 0; } else cur_col++;
        p++;
    }
    if (start < 0) return;
    /* allarga a sinistra su identificatore/numero */
    long s = start;
    while (s > 0 && (isalnum((unsigned char)text[s-1]) || text[s-1]=='_' || text[s-1]=='.')) s--;
    /* allarga a destra */
    long e = start;
    const char* q = text + start;
    while (*q && (isalnum((unsigned char)*q) || *q=='_' || *q=='.')) { e++; q++; }
    if (e <= s) return;
    int n = (int)(e - s); if (n > cap - 1) n = cap - 1;
    memcpy(out, text + s, (size_t)n); out[n] = 0;
}

/* ── server ────────────────────────────────────────────────────── */

int gmb_lsp_run(void)
{
    char body[65536];
    long id_num = 1;
    bool shutdown_requested = false;

    for (;;)
    {
        long len = read_content_length();
        if (len < 0) break;
        if (!read_body(body, sizeof(body), len)) continue;

        char method[128] = "";
        char id[64] = "";
        json_get_string(body, "method", method, sizeof(method));
        json_get_id(body, id, sizeof(id));
        if (!*id) snprintf(id, sizeof(id), "null");

        if (strcmp(method, "initialize") == 0)
        {
            /* capabilities minime: completion/hover pronti quando il parser C ci sarà */
            send_result(id,
                "{\"capabilities\":{\"textDocumentSync\":1,"
                "\"completionProvider\":{\"triggerCharacters\":[\".\",\"(\",\",\"]},"
                "\"hoverProvider\":true},\"serverInfo\":{\"name\":\"gmb-lsp\",\"version\":\"0.1\"}}");
        }
        else if (strcmp(method, "initialized") == 0)
        {
            /* niente da rispondere per initialized (notification) */
        }
        else if (strcmp(method, "shutdown") == 0)
        {
            shutdown_requested = true;
            send_result(id, "null");
        }
        else if (strcmp(method, "exit") == 0)
        {
            return shutdown_requested ? 0 : 1;
        }
        else if (strcmp(method, "textDocument/didOpen") == 0 ||
                 strcmp(method, "textDocument/didChange") == 0)
        {
            /* diagnostica reale: tokenizza con gmb_lexer e segnala i token sconosciuti */
            char uri[1024] = "";
            char text[65536] = "";
            json_get_string(body, "uri", uri, sizeof(uri));
            json_get_string(body, "text", text, sizeof(text));
            snprintf(g_lsp_doc, sizeof(g_lsp_doc), "%s", text);   /* conserva per hover/completion */
            gmb_lex_result_t lx = gmb_lexer_scan(text, (long)strlen(text));
            char diag[32768] = "";
            int ndiag = 0;
            for (int i = 0; i < lx.count; i++) {
                if (lx.tokens[i].kind == GMB_TOK_Unknown && lx.tokens[i].length > 0) {
                    char one[512];
                    snprintf(one, sizeof(one),
                        "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                        "\"end\":{\"line\":%d,\"character\":%d}},"
                        "\"severity\":1,\"message\":\"Token sconosciuto: %.*s\"}",
                        ndiag ? "," : "", lx.tokens[i].line - 1, lx.tokens[i].col - 1,
                        lx.tokens[i].line - 1, lx.tokens[i].col - 1 + lx.tokens[i].length,
                        lx.tokens[i].length, lx.tokens[i].lexeme);
                    if (strlen(diag) + strlen(one) < sizeof(diag) - 16) { strcat(diag, one); ndiag++; }
                }
            }
            char notif[65536];
            snprintf(notif, sizeof(notif),
                "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
                "\"params\":{\"uri\":\"%s\",\"diagnostics\":[%s]}}", uri, diag);
            write_message(notif);
            gmb_lex_result_free(&lx);
        }
        else if (strcmp(method, "textDocument/didClose") == 0)
        {
            /* chiude il documento: svuota la diagnostica e il testo conservato */
            char uri[1024] = "";
            json_get_string(body, "uri", uri, sizeof(uri));
            g_lsp_doc[0] = 0;
            char notif[32768];
            snprintf(notif, sizeof(notif),
                "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
                "\"params\":{\"uri\":\"%s\",\"diagnostics\":[]}}", uri);
            write_message(notif);
        }
        else if (strcmp(method, "textDocument/hover") == 0)
        {
            /* hover: riporta l'identificatore/parola sotto il cursore dal documento corrente */
            long hline = 0, hchar = 0;
            json_get_int(body, "line", &hline);
            json_get_int(body, "character", &hchar);
            char word[256] = "";
            lsp_identifier_at(g_lsp_doc, hline, hchar, word, sizeof(word));
            char result[512];
            snprintf(result, sizeof(result),
                "{\"contents\":{\"kind\":\"plaintext\",\"value\":\"%s\"}}", word);
            send_result(id, result);
        }
        else if (strcmp(method, "textDocument/completion") == 0)
        {
            /* completion: suggerisce le keyword BASIC (semplicistico, prefix-insensitive) */
            char comp[16384] = "";
            int nc = 0;
            for (int k = 0; g_lsp_keywords[k] && nc < 200; k++)
            {
                char one[256];
                snprintf(one, sizeof(one),
                    "%s{\"label\":\"%s\",\"kind\":14,\"insertText\":\"%s\"}",
                    nc ? "," : "", g_lsp_keywords[k], g_lsp_keywords[k]);
                if (strlen(comp) + strlen(one) < sizeof(comp) - 32) { strcat(comp, one); nc++; }
            }
            char result[32768];
            snprintf(result, sizeof(result), "{\"isIncomplete\":false,\"items\":[%s]}", comp);
            send_result(id, result);
        }
        else if (*method)
        {
            /* notification senza handler o request sconosciuta: rispondiamo solo alle
             * request (con id). Le notification non hanno id. */
            if (*id) send_error(id, -32601, "Metodo non implementato");
        }
        (void)id_num;
    }
    return 0;
}
