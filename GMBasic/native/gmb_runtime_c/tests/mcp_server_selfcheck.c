/* ============================================================================
 * mcp_server_selfcheck.c — Self-check del MCP server del runtime C condiviso
 * (native/gmb_runtime_c/src/gmb_mcp_server.c).
 *
 * Copre: registro con outputSchema/readOnly, protocollo JSON-RPC (initialize,
 * tools/list, tools/call, resources/list, resources/read, ping), read-only
 * enforcement, errori espliciti (tool ignoto, metodo ignoto, nessun executor),
 * notifiche senza risposta. Nessuna rete: tutto in-process.
 * ============================================================================ */

#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); tests_failed++; } \
        else { tests_passed++; } \
    } while (0)

static int contains(const char* hay, const char* needle)
{
    return hay && needle && strstr(hay, needle) != NULL;
}

/* executor di prova: registra la procedura invocata e ritorna "42". */
static char g_last_proc[128];
static int g_calls = 0;
static int stub_executor(void* ctx, const char* tool_name, const char* proc_name,
                         const char* args_json, char** out_text, int* out_is_error)
{
    (void)ctx; (void)args_json; (void)tool_name;
    g_calls++;
    snprintf(g_last_proc, sizeof(g_last_proc), "%s", proc_name ? proc_name : "");
    *out_text = strdup("42");
    *out_is_error = 0;
    return 1;
}

/* executor assente (simula un mondo C senza engine cablato) */
static void reset_state(void)
{
    gmb_mcp_set_executor(NULL, NULL);
    gmb_mcp_set_readonly(0);
    g_calls = 0;
    g_last_proc[0] = '\0';
}

int main(void)
{
    /* --- registro: outputSchema + readOnly --- */
    CHECK(gmb_mcp_register_tool_ex("gmb_stats", "GmbStats", "Sample statistics by id", 1,
                                   "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}}}",
                                   "{\"type\":\"string\"}") == 1,
          "register_tool_ex (readonly + outputSchema)");
    CHECK(gmb_mcp_register_tool_ex("gmb_mutate", "GmbMutate", "", 0, "{}", "") == 1,
          "register_tool_ex (mutating, senza outputSchema)");
    CHECK(gmb_mcp_tool_exists("gmb_stats") == 1, "tool_exists case-insensitive (esatto)");
    CHECK(gmb_mcp_tool_exists("GMB_STATS") == 1, "tool_exists case-insensitive (upper)");
    CHECK(gmb_mcp_tool_exists("nope") == 0, "tool_exists falso per ignoto");
    CHECK(gmb_mcp_tool_is_readonly("gmb_stats") == 1, "readonly=true per gmb_stats");
    CHECK(gmb_mcp_tool_is_readonly("gmb_mutate") == 0, "readonly=false per gmb_mutate");
    CHECK(strcmp(gmb_mcp_tool_proc("gmb_stats"), "GmbStats") == 0, "proc mappata");

    char* list = gmb_mcp_list_tools();
    CHECK(contains(list, "\"outputSchema\":{\"type\":\"string\"}"), "tools/list contiene outputSchema");
    CHECK(contains(list, "\"readOnly\":true"), "tools/list contiene readOnly:true");
    CHECK(contains(list, "\"description\":\"BASIC tool GmbMutate\""),
          "description default 'BASIC tool <proc>' quando vuota");
    /* l'ordine delle chiavi deve combaciare col C#: inputSchema prima di outputSchema */
    if (list) {
        const char* i1 = strstr(list, "\"inputSchema\"");
        const char* i2 = strstr(list, "\"outputSchema\"");
        CHECK(i1 && i2 && i1 < i2, "ordine inputSchema -> outputSchema (parità C#)");
    }
    free(list);

    /* --- initialize --- */
    reset_state();
    char* r = gmb_mcp_handle_json(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
        "\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"ssc\"}}}");
    CHECK(contains(r, "\"protocolVersion\":\"2024-11-05\""), "initialize ritorna protocolVersion");
    CHECK(contains(r, "\"serverInfo\":{\"name\":\"gmbasic-mcp\""), "initialize ritorna serverInfo");
    CHECK(contains(r, "\"tools\":{\"listChanged\":true}"), "initialize ritorna capabilities.tools");
    free(r);

    /* --- tools/list --- */
    r = gmb_mcp_handle_json("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}");
    CHECK(contains(r, "{\"tools\":["), "tools/list ritorna oggetto con tools");
    CHECK(contains(r, "gmb_stats"), "tools/list contiene il tool registrato");
    free(r);

    /* --- risorse --- */
    r = gmb_mcp_handle_json("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/list\"}");
    CHECK(contains(r, "mcp://tool/gmb_stats"), "resources/list espone mcp://tool/<nome>");
    free(r);
    r = gmb_mcp_handle_json(
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"resources/read\",\"params\":{\"uri\":\"mcp://tool/gmb_stats\"}}");
    CHECK(contains(r, "contents") && contains(r, "GmbStats"), "resources/read risolve la procedura");
    free(r);

    /* --- ping + prompts/list --- */
    r = gmb_mcp_handle_json("{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"ping\"}");
    CHECK(r && strcmp(r, "{\"jsonrpc\":\"2.0\",\"id\":5,\"result\":{}}") == 0, "ping");
    free(r);
    r = gmb_mcp_handle_json("{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"prompts/list\"}");
    CHECK(contains(r, "\"prompts\":[]"), "prompts/list");
    free(r);

    /* --- notifica: nessuna risposta --- */
    r = gmb_mcp_handle_json("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
    CHECK(r == NULL, "notifica -> nessuna risposta");
    free(r);

    /* --- metodo ignoto: errore esplicito --- */
    r = gmb_mcp_handle_json("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"does/not/exist\"}");
    CHECK(contains(r, "\"code\":-32601"), "metodo ignoto -> -32601");
    free(r);

    /* --- tools/call senza executor: errore ESPLICITO (mai stub silenzioso) --- */
    r = gmb_mcp_handle_json(
        "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\",\"params\":{\"name\":\"gmb_stats\",\"arguments\":{\"id\":1}}}");
    CHECK(contains(r, "\"code\":-32603") && contains(r, "No tool executor"),
          "senza executor -> -32603 esplicito");
    free(r);

    /* --- tools/call con executor --- */
    gmb_mcp_set_executor(stub_executor, NULL);
    r = gmb_mcp_handle_json(
        "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/call\",\"params\":{\"name\":\"gmb_stats\",\"arguments\":{\"id\":42}}}");
    /* L'envelope JSON-RPC completo è OBBLIGATORIO: un client reale legge id/result.
     * (Regressione trovata dall'E2E esterno: la risposta usciva nuda.) */
    CHECK(contains(r, "\"jsonrpc\":\"2.0\"") && contains(r, "\"id\":9") && contains(r, "\"result\":"),
          "tools/call ritorna un envelope JSON-RPC completo");
    CHECK(contains(r, "\"text\":\"42\""), "tools/call ritorna il contenuto dell'executor");
    CHECK(contains(r, "\"isError\":false"), "tools/call isError=false");
    CHECK(g_calls == 1 && strcmp(g_last_proc, "GmbStats") == 0, "executor invocato con la PROCEDURA mappata");
    free(r);

    /* --- tools/call tool ignoto --- */
    r = gmb_mcp_handle_json(
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"tools/call\",\"params\":{\"name\":\"nope\",\"arguments\":{}}}");
    CHECK(contains(r, "\"code\":-32602") && contains(r, "Unknown tool"), "tool ignoto -> -32602");
    free(r);

    /* --- tools/call senza name --- */
    r = gmb_mcp_handle_json("{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"tools/call\",\"params\":{\"arguments\":{}}}");
    CHECK(contains(r, "\"code\":-32602") && contains(r, "requires name"), "senza name -> -32602");
    free(r);

    /* --- read-only enforcement: il tool mutante è rifiutato PRIMA del dispatch --- */
    gmb_mcp_set_readonly(1);
    int before = g_calls;
    r = gmb_mcp_handle_json(
        "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"tools/call\",\"params\":{\"name\":\"gmb_mutate\",\"arguments\":{}}}");
    CHECK(contains(r, "Read-only server"), "server read-only rifiuta il tool mutante");
    CHECK(g_calls == before, "read-only: executor NON invocato");
    free(r);
    /* il tool READONLY passa anche in modalità read-only */
    r = gmb_mcp_handle_json(
        "{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":\"tools/call\",\"params\":{\"name\":\"gmb_stats\",\"arguments\":{}}}");
    CHECK(contains(r, "\"isError\":false"), "server read-only accetta il tool READONLY");
    CHECK(g_calls == before + 1, "read-only: executor invocato per il tool dichiarato");
    free(r);
    gmb_mcp_set_readonly(0);

    /* --- mode sconosciuto in gmb_mcp_serve: errore esplicito (nessun default) --- */
    CHECK(gmb_mcp_serve("bogus", 0, 0) == 0, "gmb_mcp_serve con mode sconosciuto -> 0");
    CHECK(gmb_mcp_serve("http", 0, 0) == 0, "gmb_mcp_serve http con porta invalida -> 0");

    printf("mcp_server_selfcheck: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
