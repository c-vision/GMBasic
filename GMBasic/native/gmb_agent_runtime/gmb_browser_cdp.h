/* gmb_browser_cdp.h — CDP client nativo C (Chrome DevTools Protocol) per la VM/C.
 * Compatibile con Chromium (page-target ws) e Obscura (flat-session su browser ws,
 * Target.createTarget + attachToTarget flatten). Un file/dominio, <1200 righe. */
#ifndef GMB_BROWSER_CDP_H
#define GMB_BROWSER_CDP_H

#include <stddef.h>

typedef struct gmb_cdp_s gmb_cdp_t;

/* Apre il CDP verso il browser locale su <port> (127.0.0.1 poi [::1]).
   Ritorna un contesto o NULL se nessun endpoint CDP raggiungibile. */
gmb_cdp_t* gmb_cdp_open(int port);

/* Invia un comando CDP (method, params_json) e attende la risposta con id matching.
   - params_json == "" o NULL → "{}"
   - Ritorna 0 successo, -1 errore. Su successo *out = heap JSON del campo "result"
     (chiamante free()). Su errore CDP/trasporto *out = NULL (e messaggio in c->last_err). */
int gmb_cdp_command(gmb_cdp_t* c, const char* method, const char* params_json, char** out);

const char* gmb_cdp_last_error(gmb_cdp_t* c);

void gmb_cdp_close(gmb_cdp_t* c);

/* JSON-escape di una stringa (per interpolare in un params JSON). Ritorna la lunghezza
   scritta in out (escluso il terminatore). Logica condivisa (§0bis): vive nel runtime. */
size_t gmb_cdp_json_escape(const char* in, char* out, size_t cap);

/* Estrae il valore interno di un risultato Runtime.evaluate (CDP: {"result":{"result":{...,"value":...}}}).
   Parità con il client C# (che ritorna solo il valore): ritorna heap plain-text (stringhe
   unescaped), "null" se assente. Chiamante free(). */
char* gmb_cdp_evaluate_value(const char* evaluate_result_json);

/* Singleton di processo: ctx CDP sulla porta GMBASIC_CDP_PORT (default 9222), aperto lazy.
   Condiviso da builtin VM/C e agent_action. */
gmb_cdp_t* gmb_cdp_singleton(void);

/* Getter JSON esportati (scanner unico nel modulo): campo stringa (0 ok / -1) e presenza. */
int gmb_cdp_json_get_str(const char* js, const char* key, char* out, size_t cap);
int gmb_cdp_json_has(const char* js, const char* key);

#endif
