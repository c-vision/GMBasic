/*
 * gmb_signal.h — segnali trading (namespace SIGNAL) lato runtime C.
 *
 * Specializzazione TRASPARENTE del messaging: nessun endpoint da specificare, bus locale
 * in-process (in live i segnali arrivano dal signal oracle via i transport remoti di
 * MESSAGE). Modello: sottoscrizione con FILTRO STRUTTURATO, evento con i campi del segnale,
 * handler event-driven chiamato per ogni evento che matcha.
 *
 * FILTRI (AND di quelli specificati; NULL o "" = qualunque):
 *   types      CSV di tipi ammessi          es. "iv_rank_high,iv_rank_low"
 *   asset      uguaglianza (case-insensitive)
 *   market     uguaglianza (case-insensitive)
 *   symbols    CSV di simboli ammessi
 *   timeframe  uguaglianza (case-insensitive) — codice classe (m1..w1, T1, T2, S1)
 *
 * L'handler riceve (id sottoscrizione, tipo, simbolo, valore[, timeframe]). L'arità è
 * decisa dal chiamante (4 parametri = senza timeframe, 5 = con timeframe) e registrata
 * qui: si invoca fn5 se presente, altrimenti fn4. Esattamente una delle due è non-NULL.
 *
 * Consegna SINCRONA sul thread chiamante (come il bus locale di MESSAGE): chi emette è un
 * task GPM, quindi l'handler gira dentro il GPM, mai su un thread esterno.
 */
#ifndef GMB_SIGNAL_H
#define GMB_SIGNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handler a 4 parametri (senza timeframe) / 5 parametri (con timeframe). */
typedef void (*gmb_signal_fn4)(const char *id, const char *type, const char *symbol, double value);
typedef void (*gmb_signal_fn5)(const char *id, const char *type, const char *symbol, double value,
                               const char *timeframe);

/* Registra/aggiorna una sottoscrizione. `id` è l'alias dichiarato con AS "id".
 * Nessun handler finché non arriva gmb_signal_on: un EMIT senza handler non consegna. */
void gmb_signal_subscribe(const char *id, const char *types, const char *asset, const char *market,
                          const char *symbols, const char *timeframe);

/* Aggancia l'handler alla sottoscrizione `id` (esattamente uno fra fn4 e fn5 non-NULL).
 * No-op se l'id non esiste. */
void gmb_signal_on(const char *id, gmb_signal_fn4 fn4, gmb_signal_fn5 fn5);

/* Emette un segnale: consegna a tutte le sottoscrizioni i cui filtri matchano.
 * Sincrono; ritorna il numero di handler invocati. */
int gmb_signal_emit(const char *type, const char *symbol, double value, const char *asset,
                    const char *market, const char *timeframe);

/* Rimuove la sottoscrizione `id`; con id NULL/"" le rimuove tutte. */
void gmb_signal_unsubscribe(const char *id);

/* Keepalive: no-op sul bus locale (parità con MESSAGE PING). */
void gmb_signal_ping(void);

/* Numero di sottoscrizioni attive (diagnostica/test). */
int gmb_signal_count(void);

#ifdef __cplusplus
}
#endif
#endif /* GMB_SIGNAL_H */
