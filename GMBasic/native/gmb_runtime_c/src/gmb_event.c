/* ============================================================================
 * gmb_event.c — registri event handler (RAISE EVENT / ADD HANDLER / REMOVE).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"

static gmb_event_handler_t* g_event_handlers = NULL;

void gmb_event_add(const char* name, void (*fn)(void)) {
    if (!name || !fn) return;
    gmb_event_handler_t* h = (gmb_event_handler_t*)malloc(sizeof(gmb_event_handler_t));
    if (!h) return;
    h->name = name;
    h->fn = fn;
    h->next = g_event_handlers;
    g_event_handlers = h;
}

void gmb_event_remove(const char* name, void (*fn)(void)) {
    gmb_event_handler_t** pp = &g_event_handlers;
    while (*pp) {
        gmb_event_handler_t* cur = *pp;
        if (strcmp(cur->name, name) == 0 && cur->fn == fn) {
            *pp = cur->next;
            free(cur);
            return;
        }
        pp = &cur->next;
    }
}

int gmb_event_count(const char* name) {
    int n = 0;
    for (gmb_event_handler_t* h = g_event_handlers; h; h = h->next)
        if (strcmp(h->name, name) == 0) n++;
    return n;
}

gmb_event_handler_t* gmb_event_first(const char* name) {
    for (gmb_event_handler_t* h = g_event_handlers; h; h = h->next)
        if (strcmp(h->name, name) == 0) return h;
    return NULL;
}

gmb_event_handler_t* gmb_event_next(gmb_event_handler_t* h, const char* name) {
    for (h = h->next; h; h = h->next)
        if (strcmp(h->name, name) == 0) return h;
    return NULL;
}
