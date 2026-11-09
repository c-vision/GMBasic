/* ============================================================================
 * gmb_finance.h — residuo runtime della vecchia superficie FINANCE.
 *
 * La superficie FINANCE è migrata alla lib FFI `shared/libs/finance`
 * (libgmbfinance, ofx_fin_*). Qui restano solo gli accessori dell'errore
 * dell'ultima operazione, usati dal controllo post-chiamata della VM/C.
 * ============================================================================ */
#ifndef GMB_FINANCE_H
#define GMB_FINANCE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Errore dell'ULTIMA funzione finance: "" se ok, altrimenti il messaggio. */
const char* gmb_fin_last_error(void);
void gmb_fin_clear_error(void);
void gmb_fin_set_error(const char* msg);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FINANCE_H */
