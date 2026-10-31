/* gmb_dbg_time.c — helper di TIMING condiviso (§0bis, gmb_runtime_c).
 *
 * Unico punto per misurare il tempo monotono (ms). Evita lo scaffolding sparso
 * (`clock_gettime` + variabili locali) e rende la strumentazione una riga sola
 * (facile da iniettare e rimuovere). Nessuna env, nessun flag. */
#include "gmb_runtime.h"
#include <time.h>

double gmb_dbg_now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}
