/* action_resolver.c — implementazione ActionResolver.
 * Su macOS il ramo AX è REALE: i wrapper *json (usati da VM/C UI.* e da gmb_agent_action)
 * cercano nell'albero AX e premono/digitano davvero (gmb_ax_mac). Altrove falliscono
 * al resolve-only. */
#include "action_resolver.h"
#include "gmb_computer_provider.h"
#include "gmb_ax.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

const char *gmb_resolve_level_name(gmb_resolve_level_t level) {
    switch (level) {
        case GMB_RESOLVE_API: return "API";
        case GMB_RESOLVE_AX: return "AX";
        case GMB_RESOLVE_DOM: return "DOM";
        case GMB_RESOLVE_CUA: return "CUA";
        default: return "UNKNOWN";
    }
}

int gmb_action_resolver_click(const char *target, gmb_resolved_action_t *out) {
    if (!target || !out) return -1;
    memset(out, 0, sizeof(*out));
    strncpy(out->target, target, sizeof(out->target)-1);
    // Euristica: se target sembra un selettore CSS (#id, .class) → DOM
    if (target[0] == '#' || target[0] == '.') {
        out->level = GMB_RESOLVE_DOM;
        out->provider = "BrowserProvider";
        out->action = "click";
        return 0;
    }
    // Se AX disponibile (macOS) → AX
#if defined(__APPLE__)
    out->level = GMB_RESOLVE_AX;
    out->provider = "MacProvider";
    out->action = "click";
    return 0;
#endif
    // Fallback CUA
    out->level = GMB_RESOLVE_CUA;
    out->provider = "ComputerProvider";
    out->action = "click";
    out->x = 100; out->y = 100; // TODO: Vision → coordinate reali
    return 0;
}

int gmb_action_resolver_type(const char *target, const char *text, gmb_resolved_action_t *out) {
    if (!target || !text || !out) return -1;
    memset(out, 0, sizeof(*out));
    strncpy(out->target, target, sizeof(out->target)-1);
    out->level = GMB_RESOLVE_AX;
    out->provider = "MacProvider";
    out->action = "type";
    return 0;
}

/* ---- wrapper JSON-friendly (VM/C builtin 1371/1372/1374) ----
 * macOS: delega al ramo AX reale (trova/preme/digita). Altrove: resolve-only. */
char *gmb_action_resolver_find(const char *target) {
    if (!target) target = "";
#if defined(__APPLE__)
    if (gmb_ax_available()) return gmb_ax_find_json(target);
#endif
    gmb_resolved_action_t r;
    if (gmb_action_resolver_click(target, &r) != 0) {
        return strdup("{\"found\":false}");
    }
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"found\":true,\"target\":\"%s\",\"level\":\"%s\",\"provider\":\"%s\"}",
        r.target, gmb_resolve_level_name(r.level), r.provider ? r.provider : "");
    return strdup(buf);
}
char *gmb_action_resolver_findall(const char *target) {
    if (!target) target = "";
#if defined(__APPLE__)
    if (gmb_ax_available()) return gmb_ax_findall_json(target);
#endif
    return strdup("[]");
}
char *gmb_action_resolver_click_json(const char *target) {
    if (!target) target = "";
#if defined(__APPLE__)
    if (gmb_ax_available()) return gmb_ax_click_json(target);
#endif
    gmb_resolved_action_t r;
    if (gmb_action_resolver_click(target, &r) != 0) return strdup("{\"ok\":false}");
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"level\":\"%s\",\"provider\":\"%s\",\"x\":%.0f,\"y\":%.0f}",
        gmb_resolve_level_name(r.level), r.provider ? r.provider : "", r.x, r.y);
    return strdup(buf);
}
char *gmb_action_resolver_type_json(const char *target, const char *text) {
    if (!target) target = ""; if (!text) text = "";
#if defined(__APPLE__)
    if (gmb_ax_available()) return gmb_ax_type_json(target, text);
#endif
    gmb_resolved_action_t r;
    if (gmb_action_resolver_type(target, text, &r) != 0) return strdup("{\"ok\":false}");
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"level\":\"%s\",\"provider\":\"%s\"}",
        gmb_resolve_level_name(r.level), r.provider ? r.provider : "");
    return strdup(buf);
}
