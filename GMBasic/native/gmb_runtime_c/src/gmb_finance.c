/* ============================================================================
 * gmb_finance.c — residuo runtime della vecchia superficie FINANCE.
 *
 * La superficie BASIC FINANCE (PMT/FV/PV/NPER/RATE, ammortamenti, NPV/IRR/MIRR/
 * XIRR/XNPV, CUMIPMT/CUMPRINC, DAYS360/YEARFRAC) è migrata alla lib FFI
 * `shared/libs/finance` (libgmbfinance, simboli ofx_fin_*): qui restano SOLO
 * l'errore dell'ultima operazione finanziaria e i suoi accessori, usati dal
 * controllo post-chiamata della VM/C.
 * ============================================================================ */
#include "gmb_finance.h"
#include <stdio.h>

/* Errore dell'ULTIMA funzione finance: "" se ok, altrimenti il messaggio.
 * La vecchia superficie lo impostava sui throw ArgumentException del C#: la lib
 * FFI segnala gli errori con il proprio contratto, questo resta per compatibilità
 * dei chiamanti che interrogano gmb_fin_last_error(). */
static char g_fin_err[160];
const char* gmb_fin_last_error(void) { return g_fin_err; }
void gmb_fin_clear_error(void) { g_fin_err[0] = '\0'; }
void gmb_fin_set_error(const char* msg) { snprintf(g_fin_err, sizeof(g_fin_err), "%s", msg ? msg : ""); }
