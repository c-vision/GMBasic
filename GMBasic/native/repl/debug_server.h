/* native/repl/debug_server.h — speculare a DebugServer.cs: debugger unificato che
 * incapsula un driver di debug per il backend attivo (PCode/VmC/C99) ed espone l'API
 * usata dall'IDE (IsPaused/CurrentLine/Continue/Step/breakpoint/Watch/Output).
 * In C i driver VmC/C99 si agganceranno alla VM/chunk (in sospeso); qui il wrapper è
 * completo e usa la sessione gmb_debug come stato.
 */
#ifndef GMB_DEBUG_SERVER_H
#define GMB_DEBUG_SERVER_H

#include <stdbool.h>

/* Driver di debug per un backend (vtable). I puntatori a NULL = funzionalità non
 * ancora agganciata al backend (VM/chunk). */
typedef struct gmb_debug_driver {
    bool (*is_paused)(void);
    bool (*is_running)(void);
    int (*current_line)(void);
    void (*continue_exec)(void);
    void (*step_in)(void);
    void (*step_over)(void);
    void (*step_out)(void);
    void (*set_breakpoint)(int line);
    void (*clear_breakpoints)(void);
    const char* (*take_output)(void);
} gmb_debug_driver_t;

typedef struct gmb_debug_server {
    const gmb_debug_driver_t* driver;
    bool running;
} gmb_debug_server_t;

void gmb_debug_server_init(gmb_debug_server_t* s, const gmb_debug_driver_t* driver);
void gmb_debug_server_start(gmb_debug_server_t* s);
bool gmb_debug_server_is_paused(const gmb_debug_server_t* s);
bool gmb_debug_server_is_running(const gmb_debug_server_t* s);
int gmb_debug_server_current_line(const gmb_debug_server_t* s);
void gmb_debug_server_continue_exec(gmb_debug_server_t* s);
void gmb_debug_server_step_in(gmb_debug_server_t* s);
void gmb_debug_server_step_over(gmb_debug_server_t* s);
void gmb_debug_server_step_out(gmb_debug_server_t* s);
void gmb_debug_server_set_breakpoint(gmb_debug_server_t* s, int line);
void gmb_debug_server_clear_breakpoints(gmb_debug_server_t* s);
const char* gmb_debug_server_take_output(const gmb_debug_server_t* s);
void gmb_debug_server_stop(gmb_debug_server_t* s);

#endif
