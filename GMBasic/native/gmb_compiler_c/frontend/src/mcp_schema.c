/* native/gmb_compiler_c/src/mcp_schema.c
 *
 * Implementazione UNICA dello schema MCP (inputSchema) dai parametri di una
 * procedura BASIC. Usata da entrambi i codegen C (P-Code e C99) per non
 * duplicare la logica (§0bis). Vedi gmb_mcp_schema.h.
 */
#include "gmb_mcp_schema.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- mini buffer heap ---- */
typedef struct { char* s; size_t len, cap; } msb_t;

static int msb_reserve(msb_t* b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap) return 1;
    size_t nc = b->cap ? b->cap : 128;
    while (nc < b->len + extra + 1) nc *= 2;
    char* ns = (char*)realloc(b->s, nc);
    if (!ns) return 0;
    b->s = ns; b->cap = nc;
    return 1;
}
static void msb_put(msb_t* b, const char* s)
{
    if (!s) return;
    size_t n = strlen(s);
    if (!msb_reserve(b, n)) return;
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}
static void msb_putc_(msb_t* b, char c)
{
    if (!msb_reserve(b, 1)) return;
    b->s[b->len++] = c;
    b->s[b->len] = '\0';
}
/* nome come stringa JSON (escaping minimo: quote e backslash) */
static void msb_ident(msb_t* b, const char* s)
{
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\') msb_putc_(b, '\\');
        msb_putc_(b, *s);
    }
}

/* JSON Schema di un tipo BASIC (parità McpSchemaBuilder.MapBasicTypeToJsonSchema). */
static void mcp_schema_type(msb_t* out, const char* btype, int is_array)
{
    if (is_array) {
        msb_put(out, "{\"type\":\"array\",\"items\":{\"type\":");
        msb_put(out, (btype && strstr(btype, "STRING")) ? "\"string\"}" : "\"number\"}");
        msb_put(out, "}");
        return;
    }
    if (!btype || !*btype) { msb_put(out, "{\"type\":\"string\"}"); return; }
    const char* jt;
    if (strstr(btype, "STRING")) jt = "string";
    else if (strstr(btype, "BIT") || strstr(btype, "BOOL")) jt = "boolean";
    else if (strstr(btype, "INTEGER") || strstr(btype, "SHORT") || strstr(btype, "INT")
             || strstr(btype, "LONG") || strstr(btype, "QUAD") || strstr(btype, "BYTE"))
        jt = "integer";
    else if (strstr(btype, "JSON")) jt = "object";
    else jt = "number"; /* DOUBLE/SINGLE/CURRENCY/NUMBER/default numerico */
    msb_put(out, "{\"type\":\"");
    msb_put(out, jt);
    msb_put(out, "\"}");
}

char* gmb_mcp_schema_from_params(char** params, char** types,
                                 uint8_t* is_array, int n_params)
{
    if (n_params <= 0) return strdup("{}");
    msb_t out = {0};
    msb_put(&out, "{\"type\":\"object\",\"properties\":{");
    for (int i = 0; i < n_params; i++) {
        if (i) msb_putc_(&out, ',');
        msb_putc_(&out, '"');
        msb_ident(&out, (params && params[i]) ? params[i] : "");
        msb_put(&out, "\":");
        mcp_schema_type(&out, types ? types[i] : NULL, is_array ? is_array[i] : 0);
    }
    msb_put(&out, "},\"required\":[");
    for (int i = 0; i < n_params; i++) {
        if (i) msb_putc_(&out, ',');
        msb_putc_(&out, '"');
        msb_ident(&out, (params && params[i]) ? params[i] : "");
        msb_putc_(&out, '"');
    }
    msb_put(&out, "]}");
    return out.s ? out.s : strdup("{}");
}

char* gmb_mcp_input_schema_json(const gmb_stmt_t* fs)
{
    if (!fs) return strdup("{}");
    return gmb_mcp_schema_from_params(fs->func_params, fs->param_types,
                                      fs->param_is_array, fs->n_params);
}
