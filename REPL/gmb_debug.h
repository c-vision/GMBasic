/* native/repl/gmb_debug.h — sessione di debug della ReplNative (C).
 * Stato di debug REALE della REPL: breakpoint, watch, modalità step/continue, trace.
 * Il collegamento all'esecuzione (vmc/c99) avviene nei runner; questa è la base di stato
 * e i comandi DEBUG/BREAK/CLEAR/STEP/CONT/WATCH/TRACE la manipolano.
 */
#ifndef GMB_DEBUG_H
#define GMB_DEBUG_H

#include <stdbool.h>

#define GMB_DBG_MAX_BREAKS 64
#define GMB_DBG_MAX_WATCH 32
#define GMB_DBG_MAX_VAR 64

typedef enum { GMB_DBG_OFF, GMB_DBG_STEPPING, GMB_DBG_RUNNING } gmb_dbg_mode_t;

typedef struct gmb_breakpoint {
    int line;            /* 1-based; 0 = slot libero */
    bool enabled;
    unsigned hits;
} gmb_breakpoint_t;

typedef struct gmb_watch {
    char name[GMB_DBG_MAX_VAR];
} gmb_watch_t;

typedef struct gmb_debug_session {
    gmb_dbg_mode_t mode;
    bool trace;                               /* TRACE on/off */
    gmb_breakpoint_t breaks[GMB_DBG_MAX_BREAKS];
    int break_count;
    gmb_watch_t watches[GMB_DBG_MAX_WATCH];
    int watch_count;
    int current_line;                         /* riga corrente (0 = nessuna) */
    bool at_breakpoint;
} gmb_debug_session_t;

void gmb_debug_init(gmb_debug_session_t* s);

/* Breakpoint. Ritornano indice o -1 su errore (set pieno / non valido). */
int gmb_debug_add_break(gmb_debug_session_t* s, int line);
int gmb_debug_clear_break(gmb_debug_session_t* s, int line);
int gmb_debug_clear_all_breaks(gmb_debug_session_t* s);

/* Watch. */
int gmb_debug_add_watch(gmb_debug_session_t* s, const char* var);
int gmb_debug_clear_watch(gmb_debug_session_t* s, const char* var);

bool gmb_debug_has_break(const gmb_debug_session_t* s, int line);

/* Stato esecuzione (usato dai runner vmc/c99). */
void gmb_debug_set_line(gmb_debug_session_t* s, int line);
bool gmb_debug_step_requested(const gmb_debug_session_t* s);

#endif
