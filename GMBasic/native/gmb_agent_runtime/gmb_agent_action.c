/* gmb_agent_action.c — implementazione AgentAction runner (Fase 5 fondamenta).
 * Riusa i componenti già testati del runtime (gmb_agent_runtime): ToolRegistry,
 * ActionResolver (UI), CDP browser. Nessuna logica duplicata (§0bis). */
#include "gmb_agent_action.h"
#include "gmb_browser_cdp.h"
#include "tool_registry.h"
#include "action_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gmb_agent_action_run(const char* brain_text, char* out_obs, size_t cap) {
    if (!out_obs || cap == 0) return -1;
    out_obs[0] = 0;
    if (!brain_text || !*brain_text) return 1;

    /* 1) isola l'oggetto azione: {"action":{...}} oppure {"tool_call":{...}} */
    const char* body = brain_text;
    int has_action = gmb_cdp_json_has(body, "action");
    int has_call = gmb_cdp_json_has(body, "tool_call");
    if (!has_action && !has_call) return 1;

    /* 2) estrae tool/name e args/arguments */
    char tool[128] = "";
    char args[2048] = "";
    if (has_action) {
        if (gmb_cdp_json_get_str(body, "tool", tool, sizeof(tool)) != 0)
            gmb_cdp_json_get_str(body, "name", tool, sizeof(tool));
        gmb_cdp_json_get_str(body, "args", args, sizeof(args));
    } else {
        if (gmb_cdp_json_get_str(body, "name", tool, sizeof(tool)) != 0)
            gmb_cdp_json_get_str(body, "tool", tool, sizeof(tool));
        gmb_cdp_json_get_str(body, "arguments", args, sizeof(args));
    }
    if (!tool[0]) { snprintf(out_obs, cap, "{\"obs\":\"action senza tool\"}"); return -1; }

    /* 3) dispatch sul runtime */
    if (strcmp(tool, "tool.search") == 0 || strcmp(tool, "TOOL.SEARCH") == 0) {
        char q[256] = "";
        gmb_cdp_json_get_str(args, "query", q, sizeof(q));
        char* j = gmb_tool_registry_search_json(q);
        snprintf(out_obs, cap, "{\"obs\":%s}", j ? j : "[]");
        if (j) free(j);
        return 0;
    }
    if (strcmp(tool, "ui.find") == 0 || strcmp(tool, "UI.FIND") == 0) {
        char target[256] = "";
        gmb_cdp_json_get_str(args, "target", target, sizeof(target));
        char* j = gmb_action_resolver_find(target);
        snprintf(out_obs, cap, "{\"obs\":%s}", j ? j : "{\"found\":false}");
        if (j) free(j);
        return 0;
    }
    if (strncmp(tool, "browser.", 8) == 0 || strncmp(tool, "browser_", 8) == 0) {
        gmb_cdp_t* cdp = gmb_cdp_singleton();
        if (!cdp) { snprintf(out_obs, cap, "{\"obs\":\"browser non connesso\"}"); return -1; }
        const char* verb = tool + 8;
        char* out = NULL;
        char body[2048];
        int rc = -1;
        if (strncmp(verb, "navigate", 8) == 0) {
            char url[512] = "";
            gmb_cdp_json_get_str(args, "url", url, sizeof(url));
            char esc[1024] = "";
            gmb_cdp_json_escape(url, esc, sizeof(esc));
            snprintf(body, sizeof(body), "{\"url\":\"%s\"}", esc);
            rc = gmb_cdp_command(cdp, "Page.navigate", body, &out);
            snprintf(out_obs, cap, rc == 0 ? "{\"obs\":\"navigated\"}" : "{\"obs\":\"%s\"}", rc == 0 ? "ok" : (gmb_cdp_last_error(cdp) ? gmb_cdp_last_error(cdp) : "err"));
        } else if (strncmp(verb, "evaluate", 8) == 0) {
            char expr[1024] = "";
            gmb_cdp_json_get_str(args, "expression", expr, sizeof(expr));
            char esc[2048] = "";
            gmb_cdp_json_escape(expr, esc, sizeof(esc));
            snprintf(body, sizeof(body), "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
            rc = gmb_cdp_command(cdp, "Runtime.evaluate", body, &out);
            char* val = rc == 0 && out ? gmb_cdp_evaluate_value(out) : NULL;
            snprintf(out_obs, cap, "{\"obs\":\"%s\"}", val ? val : (gmb_cdp_last_error(cdp) ? gmb_cdp_last_error(cdp) : "err"));
            if (val) free(val);
        } else if (strncmp(verb, "snapshot", 8) == 0) {
            gmb_cdp_command(cdp, "DOM.enable", "", NULL);
            rc = gmb_cdp_command(cdp, "DOM.getDocument", "{\"depth\":-1,\"pierce\":true}", &out);
            snprintf(out_obs, cap, "{\"obs\":\"%s\"}", rc == 0 && out ? out : "no dom");
        } else {
            snprintf(out_obs, cap, "{\"obs\":\"tool browser non supportato: %s\"}", verb);
            if (out) free(out);
            return -1;
        }
        if (out) free(out);
        return rc == 0 ? 0 : -1;
    }
    snprintf(out_obs, cap, "{\"obs\":\"tool sconosciuto: %s\"}", tool);
    return -1;
}
