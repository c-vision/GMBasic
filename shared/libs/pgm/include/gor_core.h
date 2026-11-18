/*
 * gor_core.h — gmbortools: tipi, stato, versione.
 *
 * gmbortools è la libreria C99 di ricerca operativa (combinatoria/CP).
 * Vedi il piano: 
 *
 * C99 puro, sole dipendenze libc/libm. Prefisso API: gor_*.
 */
#ifndef GMBORTOOLS_GOR_CORE_H
#define GMBORTOOLS_GOR_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef double gor_real;
typedef long long gor_int;

#define GMBORTOOLS_VERSION_MAJOR 0
#define GMBORTOOLS_VERSION_MINOR 1
#define GMBORTOOLS_VERSION_PATCH 0

typedef enum gor_status {
    GOR_OK = 0,
    GOR_ERR_ARG = 1,        /* argomento non valido */
    GOR_ERR_NOMEM = 2,      /* allocazione fallita */
    GOR_ERR_INFEASIBLE = 3, /* problema inammissibile */
    GOR_ERR_NOCONV = 4,     /* non convergenza / ciclo negativo */
    GOR_ERR_RANGE = 5,      /* indice/valore fuori range */
    GOR_ERR_CYCLE = 6,      /* ciclo in un grafo che ne vieta */
    GOR_ERR_SINGULAR = 7,   /* sistema singolare */
    GOR_ERR_INTERNAL = 8    /* stato interno incoerente */
} gor_status;

/* Copia ridotta per il pacchetto PGM: servono solo i tipi (gor_real) e lo stato.
 * Le tre funzioni di utilità di gmbortools (gor_version/gor_status_str/gor_near)
 * non sono compilate qui, quindi non vengono dichiarate. */

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_CORE_H */
