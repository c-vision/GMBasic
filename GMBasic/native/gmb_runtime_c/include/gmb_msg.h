/*
 * gmb_msg.h — messaging generico (namespace MESSAGE) lato runtime C.
 *
 * Bus IN-PROCESS (transport "local"): connetti, sottoscrivi (per alias), pubblica su topic,
 * handler registrato come puntatore a funzione C (la SUB generata). Fan-out sincrono sul
 * topic coincidente. I transport remoti (tcp/amqp/...) si innestano sullo stesso shim.
 *
 * CONFERMA DI CONSEGNA (solo transport tcp). Il broker assegna un id a ogni messaggio e lo
 * ri-consegna finché il consumatore non conferma (at-least-once, con retry-after configurato
 * lato broker):
 *   - ricezione: dopo il dispatch il client manda ACK per il messaggio consumato, oppure NACK
 *     se nessun handler registrato lo ha preso in carico;
 *   - pubblicazione: gmb_msg_publish attende la conferma del broker (attesa limitata a
 *     GMB_MSG_ACK_WAIT_MS), così al ritorno le MSG sono già state scritte ai sottoscrittori.
 * Sul bus locale (e con broker legacy che non manda id) non cambia nulla: nessun ACK.
 *
 * BUS INESISTENTE ≠ BUS LOCALE. `find_bus() == NULL` significa "handle non valido" (bus mai
 * creato o slot esaurito): SUBSCRIBE non registra nulla e PUBLISH è no-op. La consegna
 * in-process avviene SOLO su un bus locale valido, e ogni bus ha la PROPRIA identità (una
 * CONNECT tcp fallita ripiega su un bus locale con id proprio): due CONNECT fallite non si
 * vedono tra loro. Prima ogni fallimento restituiva -1 e la PUBLISH su -1 consegnava le
 * subscription dell'altra CONNECT fallita (falso positivo di rete).
 */
#ifndef GMB_MSG_H
#define GMB_MSG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handler: (id sottoscrizione, topic, payload). */
typedef void (*gmb_msg_handler_t)(const char *sub, const char *topic, const char *data);

int64_t gmb_msg_connect(const char *uri);
/* -1 se il bus non esiste (handle non valido): nessuna subscription registrata. */
int64_t gmb_msg_subscribe(int64_t bus, const char *topic, const char *alias);
void    gmb_msg_unsubscribe(const char *sub);
void    gmb_msg_publish(int64_t bus, const char *topic, const char *data);
/* No-op se l'handle non è valido: la consegna locale avviene solo sulle subscription del bus. */
/* 1 se il bus esiste ed è un tcp realmente connesso (0 per bus locale o CONNECT fallita). */
int     gmb_msg_bus_is_tcp(int64_t bus);

void    gmb_msg_ping(int64_t bus);
void    gmb_msg_close(int64_t bus);
void    gmb_msg_reg_handler(const char *sub, gmb_msg_handler_t fn);
/*
 * CONFERMA DI CONSEGNA (at-least-once) sul trasporto tcp:
 *   - ogni MSG porta un id assegnato dal broker ("MSG <seq> <topic> <data>");
 *   - dopo il dispatch gmb_msg_pump() manda "ACK <seq>" se almeno un handler ha
 *     consumato il messaggio, altrimenti "NACK <seq>" (messaggio orfano);
 *   - il broker ri-consegna finché non riceve la conferma (retry-after) e dopo N
 *     tentativi lo abbandona: la policy resta del broker, il client non tiene coda.
 *   - la PUB non attende la conferma del broker: chi deve osservare l'effetto
 *     attende una condizione propria (attendere nella PUBLISH richiederebbe di
 *     pompare dentro la scrittura, rientrando nell'handler del VM).
 */
/* Consegna i messaggi in arrivo dal broker (trasporto tcp): chiamare periodicamente.
 * Ritorna il numero di messaggi consegnati. No-op sul bus locale. Manda anche le conferme
 * (ACK/NACK) dei messaggi con id, e consuma gli ACK di conferma delle nostre PUB. */
int     gmb_msg_pump(int64_t bus);
/* Avvia una RECEIVE goroutine GPM (park-based) per un bus tcp: poll non bloccante +
 * gmb_yield_ticks; dispatcha gli handler. No-op per bus locali. Idempotente. */
void    gmb_msg_start_recv(int64_t bus);
/* Pompa TUTTI i bus tcp attivi (non bloccante): da chiamare nelle attese (SLEEP). */
void    gmb_msg_pump_all(void);

/*
 * CONTATORI DI CONSEGNA — MESSAGE.DELIVERY(bus) -> "sent=S recv=R ack=A nack=N dup=D last=L"
 * Per bus, monotoni, azzerati alla CONNECT. Semantica (identica alle gambe managed):
 *   sent  PUB scritte verso il broker (solo trasporto tcp: il bus locale non conferma)
 *   recv  MSG ricevute dal broker (con o senza id)
 *   ack   ACK inviati (almeno un handler ha preso in carico il messaggio)
 *   nack  NACK inviati (messaggio orfano: il broker lo ri-consegna)
 *   dup   MSG con id già visto, cioè RI-CONSEGNE del broker (dup>0 && nack>0 = retry osservato)
 *   last  massimo id visto (0 se nessuno)
 * Ritorna 1 se il bus esiste, 0 altrimenti (in tal caso out contiene tutti zeri).
 * Non è thread-safe (buffer del chiamante) e non blocca. Il trasporto "local" risponde con
 * sent/ack/nack/dup = 0: la consegna in-process è sincrona, non c'è conferma da contare.
 */
int     gmb_msg_delivery_stats(int64_t bus, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* GMB_MSG_H */
