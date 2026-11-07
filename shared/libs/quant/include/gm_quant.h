/*
 * gm_quant.h — gmbquant: libreria C99 del dominio quantitativo "StdLib trading".
 *
 * Contenuto (ondata 3): valutazione azionaria, metriche di performance/rischio,
 * volatilità, fondamentali, obbligazioni, derivati, ottimizzazione di portafoglio
 * e backtest. Espone in C le funzioni dei namespace TRADING.* della StdLib, con
 * semantica identica ai backend (canone del runtime condiviso).
 *
 * Convenzioni:
 *  - tutte le funzioni sono PURE (nessuno stato globale, nessuna I/O);
 *  - input non valido (denominatore zero, serie troppo corta, array nullo) -> 0.0
 *    (stesso comportamento del canone C#/runtime: nessuna eccezione dall'FFI);
 *  - `n` è il numero di elementi; le funzioni con `out` ritornano 1 = ok, 0 = input
 *    non valido (out lasciato invariato).
 *
 * Dipendenze: nessuna esterna (solo libm). Monodirezionale: gmbquant -> gmbmaths.
 */
#ifndef GMBQUANT_H
#define GMBQUANT_H

#include <stddef.h>

#include "gm_quant_general.h"
#include "gm_quant_metrics.h"
#include "gm_quant_volatility.h"
#include "gm_quant_fundamentals.h"
#include "gm_quant_bonds.h"
#include "gm_quant_derivatives.h"
#include "gm_quant_portfolio.h"
#include "gm_quant_backtest.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Versione della superficie (incrementata a ogni ondata). */
#define GQ_VERSION_MAJOR 0
#define GQ_VERSION_MINOR 1

#ifdef __cplusplus
}
#endif

#endif /* GMBQUANT_H */
