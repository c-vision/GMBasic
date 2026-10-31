/* action_resolver.h — ActionResolver (CUA §9, §21, §44)
 * Risolve UI.CLICK("Save") sul provider migliore: Native API → AX/UIA/AT-SPI → DOM → CUA pixel.
 * Un file/dominio, <1200 righe. */
#ifndef GMB_ACTION_RESOLVER_H
#define GMB_ACTION_RESOLVER_H

typedef enum {
    GMB_RESOLVE_API = 0,      // API nativa (es. files.save, browser API)
    GMB_RESOLVE_AX = 1,       // Accessibility (AX/UIA/AT-SPI)
    GMB_RESOLVE_DOM = 2,      // DOM/CDP (browser)
    GMB_RESOLVE_CUA = 3       // Computer Use pixel (fallback universale)
} gmb_resolve_level_t;

typedef struct {
    gmb_resolve_level_t level;
    const char *provider; // es. "BrowserProvider", "MacProvider"
    const char *action;   // es. "click", "type"
    double x, y;          // coordinate se CUA
    char target[128];     // target semantico es. "Save"
} gmb_resolved_action_t;

// Risolve un'azione semantica sul livello più informativo disponibile.
// Ritorna 0 se risolta, -1 se nessun provider disponibile.
int gmb_action_resolver_click(const char *target, gmb_resolved_action_t *out);
int gmb_action_resolver_type(const char *target, const char *text, gmb_resolved_action_t *out);
const char *gmb_resolve_level_name(gmb_resolve_level_t level);

/* wrapper JSON-friendly (per VM/C builtin 1371/1372/1374) */
char *gmb_action_resolver_find(const char *target); /* heap, free() */
char *gmb_action_resolver_findall(const char *target);
char *gmb_action_resolver_click_json(const char *target);
char *gmb_action_resolver_type_json(const char *target, const char *text);

#endif
