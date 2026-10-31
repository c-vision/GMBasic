/* gmb_screen_internal.h — struttura PRIVATA del buffer di schermo a celle.
 *
 * Vive fuori da gmb_tui.c perché il buffer è anche in `libgmb_screen.dylib` (console testuale
 * senza SDL). Chi accede ai campi direttamente (TUI) include questo header.
 */
#ifndef GMB_SCREEN_INTERNAL_H
#define GMB_SCREEN_INTERNAL_H
#include "gmb_tui.h"

struct gmb_screen_buffer {
    int width, height;
    gmb_screen_cell_t* cells;   /* [height*width] */
    gmb_screen_sink_t* sink;    /* se != NULL: le scritture vanno al backend */
};

#endif
