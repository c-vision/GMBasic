/* gmb_ipc_protocol.h — IPC v1 protocollo tra host GMBasic e processo nativo MLX.
 *
 * R2: messaggi JSON delimitati da newline su stdin/stdout. Sostituisce
 * il report su file (<ckpt>.json) e la formattazione testo per lo stato.
 *
 * Formato richiesta (host → server, una riga JSON per comando):
 *   {"id":<N>,"method":"finetune|apply|status|cancel","params":{...}}
 *
 * Formato risposta (server → host, una o piu' righe JSON):
 *   {"id":<N>,"type":"progress","data":{...}}   (0+)
 *   {"id":<N>,"type":"result","data":{...}}     (finale, successo)
 *   {"id":<N>,"type":"error","data":{"message":"..."}}  (finale, errore)
 *
 * Ogni messaggio e' una riga JSON valida terminata da '\n'.
 * Il server risponde allo stesso id della richiesta.
 * Il client può inviare una nuova richiesta solo dopo aver ricevuto
 * result/error per la precedente (richieste seriali).
 */
#ifndef GMB_IPC_PROTOCOL_H
#define GMB_IPC_PROTOCOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_IPC_MAX_LINE 65536

/* Costruitore di richiesta JSON finetune.
 * Scrive in buf una riga JSON: {"id":1,"method":"finetune","params":{...}}
 * Ritorna 0 ok, -1 se il buffer e' troppo corto. */
int gmb_ipc_build_finetune(const char* model_dir, const char* dataset,
                           const char* holdout, const char* ckpt,
                           int rank, int layer, int steps, int batch,
                           double lr, int seed, const char* frozen,
                           char* buf, size_t cap);

/* Costruitore di richiesta JSON apply.
 * {"id":1,"method":"apply","params":{"ckpt_prefix":"...","model":"...","dataset":"..."}} */
int gmb_ipc_build_apply(const char* ckpt_prefix, const char* model_dir,
                        const char* dataset, char* buf, size_t cap);

/* Costruitore di richiesta JSON status:
 * {"id":1,"method":"status"} */
int gmb_ipc_build_status(char* buf, size_t cap);

/* Costruitore di richiesta JSON cancel:
 * {"id":1,"method":"cancel"} */
int gmb_ipc_build_cancel(char* buf, size_t cap);

/* Estrae un campo JSON dalla risposta (primo object `data`).
 * Helper: cerca "key" in body e copia il valore in out.
 * Gestisce string, number e boolean.
 * Ritorna 0 trovato, -1 non trovato. */
int gmb_ipc_extract(const char* body, const char* key, char* out, size_t cap);

/* Estrae un numero (double) da un campo JSON. */
double gmb_ipc_extract_dbl(const char* body, const char* key, double dflt);

/* Estrae un intero (long) da un campo JSON. */
long gmb_ipc_extract_long(const char* body, const char* key, long dflt);

/* Verifica che body contenga "ok":true (nel data object).
 * Ritorna 1 se ok==true, 0 altrimenti. */
int gmb_ipc_result_ok(const char* body);

/* Determina il tipo del messaggio JSON di risposta:
 *   "result" -> 0
 *   "error"  -> -1
 *   "progress" -> 1
 *   altro    -> -2
 * body e' l'intera riga JSON di risposta. */
int gmb_ipc_response_type(const char* body);

/* Esegue un comando IPC: trova bin via envvar/cands, spawna, scrive
 * request_json su stdin, legge response da stdout.
 * Ritorna exit code del processo (-1 se spawn fallisce).
 * timeout_ms: 0 = nessun timeout.
 * response: buffer di uscita con la risposta JSON completa. */
int gmb_ipc_spawn(const char* bin_name, const char* envvar,
                  const char* request_json,
                  char* response, size_t rcap,
                  int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* GMB_IPC_PROTOCOL_H */