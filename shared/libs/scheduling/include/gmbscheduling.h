/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbscheduling.h — scheduling e grafi temporali. Self-contained (C99 + libm).
 * Prefisso API: gsc_ (Gmb Scheduling Core). Nessun tipo di dominio di trading. */
#ifndef GMB_SCHEDULING_H
#define GMB_SCHEDULING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef double gsc_real;

typedef enum gsc_status {
    GSC_OK = 0,
    GSC_ERR_ARG = 1,
    GSC_ERR_NOMEM = 2,
    GSC_ERR_RANGE = 3
} gsc_status;

const char *gsc_version(void);
const char *gsc_status_str(gsc_status status);

/* ── Cammini con finestre temporali (arrivo da AirScheduling) ─────────────────
 * Grafo orientato con una finestra [start, end] per nodo (istante di inizio/fine
 * dell'attività). Un arco u->v è percorribile solo se
 *     end[u] + min_connect <= start[v]
 * cioè con il tempo minimo di connessione tra le due attività.
 *
 * Parametri COMPATTI (8, adatti anche al binding FFI):
 *   n         numero di nodi
 *   adj       n*n row-major, 1 = arco u->v
 *   windows   2*n + 1 valori: start[0..n-1], end[0..n-1], min_connect in coda
 *   s, t      nodo iniziale e finale
 *   paths     capacità max_paths * n (ogni riga: fino a n indici di nodo)
 *   lens      capacità max_paths (numero di nodi di ciascun cammino)
 *   max_paths numero massimo di cammini da restituire
 *
 * Ritorna il numero di cammini trovati (0 se nessuno), -1 su argomenti invalidi. */
int gsc_paths_time_window(int n, const int *adj, const double *windows,
                          int s, int t, int *paths, int *lens, int max_paths);

#ifdef __cplusplus
}
#endif

#endif /* GMB_SCHEDULING_H */
