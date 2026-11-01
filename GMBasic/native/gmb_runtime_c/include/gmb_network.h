/* ============================================================================
 * gmb_network.h — Sottosistema di rete (TCP/UDP/HTTP/WebSocket) per la VM/C.
 * Runtime C condiviso (VM + transpiler). Modello: handle long → struttura,
 * come PCodeVM (ExecuteNetCmd). Fase R1: TCP/UDP base.
 *
 * IL RUNTIME È NEUTRO RISPETTO ALLA GPM: il blocco rete passa da hook astratti
 * registrati dal backend (VM/C: PCB interpretati, return 0 dal tick — nessun hook;
 * C99 transpiler: coroutine green-thread — hook block/wake). Il network-watcher
 * condiviso fa poll() sui fd in attesa e sveglia il backend tramite l'hook di wake.
 * ============================================================================ */
#ifndef GMB_NETWORK_H
#define GMB_NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Registry di handle ---- */
/* Apre il registry (init). Chiamato all'avvio della VM. */
void gmb_net_init(void);
void gmb_net_shutdown(void);

/* ---- Hook di blocco astratti (GPM del backend) ----
   Il runtime NON conosce il modello GPM (task coroutine vs PCB). Un backend che vuole
   bloccare cooperativamente su un fd registra:
   - block_fn(ctx, fd, timeout_ms): parcheggia il task corrente del backend su fd
     (C99: state=BLOCKED + gmb_block(); VM/C: non registra, resta non-bloccante).
   - wake_fn(ctx, waiter): riaccoda il task parcheggiato (C99: gmb_sched_enqueue).
   Il waiter è un opacoptrin condiviso: il runtime lo memorizza nel registry fd→waiter
   e lo restituisce a wake_fn senza interpretarlo. */
typedef void (*gmb_net_block_fn)(void* ctx, int fd, int64_t timeout_ms);
typedef void (*gmb_net_wake_fn)(void* ctx, void* waiter);
/* Registra (o azzera, passando NULL/NULL) gli hook di blocco del backend.
   ctx è opaco e passato agli hook. */
void gmb_net_set_block_hooks(gmb_net_block_fn block, gmb_net_wake_fn wake, void* ctx);

/* API per il backend con hook (C99): registra il task corrente (waiter opaco) in
   attesa su fd. Il network-watcher lo sveglia a fd pronto o timeout via wake_fn.
   Ritorna 0 ok / -1 registry pieno. */
int gmb_net_waiter_register(int fd, void* waiter, int64_t timeout_ms);
/* Come gmb_net_waiter_register ma con eventi poll specifici (POLLIN/POLLOUT;
   0 = POLLIN). V7b: WRITE readiness per TCP SEND e connect non-bloccante. */
int gmb_net_waiter_register_ev(int fd, void* waiter, int64_t timeout_ms, short events);
/* V7c (ABA): come register_ev ma memorizza anche il generation token dell'handle
   parcheggiato. Il wake-on-close (net_release) non usa la gen: serve solo al
   backend per il confronto al retry (vedi gmb_net_handle_gen). */
int gmb_net_waiter_register_gen(int fd, void* waiter, int64_t timeout_ms, short events, uint32_t gen);
void gmb_net_waiter_unregister(int fd, void* waiter);

/* V7c: generation token corrente dell'handle (0 = handle chiuso/non valido).
   Monotono per allocazione: un handle chiuso e riusato ha gen diversa, così un
   task parcheggiato sul vecchio handle se ne accorge al retry (anti-ABA). */
uint32_t gmb_net_handle_gen(int64_t handle);

/* Attende finché fd è pronto (read/write) o scade il timeout (ms; <=0 = indefinito).
   - Se il backend ha registrato gli hook: parcheggia il task corrente (via block_fn) e
     ritorna quando il network-watcher lo sveglia (fd pronto o timeout).
   - Senza hook: poll() non-bloccante con timeout (il chiamante VM/C gestisce il "non pronto").
   Ritorna 1 = pronto, 0 = timeout, -1 = errore. */
int gmb_net_wait_fd(int fd, int64_t timeout_ms);

/* Ritorna il file descriptor (socket) di un handle, o -1 se non valido.
   Usato dal dispatcher per attendere l'accettabilità del listener (TCP ACCEPT con timeout). */
int gmb_net_handle_fd(int64_t handle);

/* ---- TCP ---- */
/* Server listener su porta ("" host). Ritorna handle (>0) o -1. */
int64_t gmb_net_tcp_listen(const char* host, int port);
/* Client: connette a host:port. Ritorna handle (>0) o -1. */
int64_t gmb_net_tcp_connect(const char* host, int port);
/* V7b: connect non-bloccante. La risoluzione DNS qui è SINCRONA: il VM/C usa il
   pool asincrono §16 (gmb_net_dns_resolve_start + gmb_net_tcp_connect_addr);
   questo path resta per il backend C99 (green-thread) e come fallback. */
int64_t gmb_net_tcp_connect_start(const char* host, int port, int* pending);
/* Completa il connect non-bloccante. Ritorna 0 = connesso, -1 = fallito. */
int gmb_net_connect_finish(int64_t handle);
/* §16: connect non-bloccante da indirizzo già risolto (sa = sockaddr, sa_len byte).
   Usato dal VM/C dopo la risoluzione DNS asincrona. *pending come connect_start. */
int64_t gmb_net_tcp_connect_addr(const uint8_t* sa, int sa_len, int* pending);
/* ---- §16: pool DNS asincrono ----
   getaddrinfo gira su thread dedicati (fuori dai worker cooperativi). Il job
   segnala il completamento su una pipe: l'fd letto è parcheggiabile via
   gmb_net_waiter_register_* (POLLIN). Id monotono mai riusato (anti-ABA).
   Pool pieno/piattaforma senza pipe → start ritorna -1 (fallback sincrono). */
int64_t gmb_net_dns_resolve_start(const char* host, int port);
int gmb_net_dns_job_fd(int64_t id);
/* Consuma il job completato: 0 = risolto (sockaddr in out_sa, max sa_cap byte),
   -1 = fallito/sconosciuto. Libera lo slot. */
int gmb_net_dns_resolve_finish(int64_t id, uint8_t* out_sa, int sa_cap, int* out_len);
/* Abbandona il job (timeout/task morto): se in volo lo libera il worker. */
void gmb_net_dns_cancel(int64_t id);
/* Accetta una connessione dal listener. Ritorna handle client o -1 (non-blocking). */
int64_t gmb_net_tcp_accept(int64_t listener);
/* Invia dati. Ritorna bytes scritti (>=0), -1 = riprovabile (EAGAIN/WANT),
   -2 = errore hard (V7b). */
int gmb_net_tcp_send(int64_t handle, const uint8_t* data, size_t len);
/* Riceve dati. Ritorna bytes letti, 0 = chiuso, -1 = errore/non-disponibile. */
int gmb_net_tcp_recv(int64_t handle, uint8_t* buf, size_t len);
/* NET.PEEK: come recv ma con MSG_PEEK (non rimuove i dati dal buffer). */
int gmb_net_tcp_peek(int64_t handle, uint8_t* buf, size_t len);
int gmb_net_tcp_available(int64_t handle);
gmb_string_t gmb_net_tcp_peek_str(int64_t handle, int len);
/* Riceve dati attendendo finché disponibili (usa gmb_net_wait_fd + hook backend).
   timeout_ms: 0 = attesa indefinita. Ritorna come gmb_net_tcp_recv. */
int gmb_net_tcp_recv_block(int64_t handle, uint8_t* buf, size_t len, int64_t timeout_ms);
/* Chiude e rimuove l'handle. */
void gmb_net_tcp_close(int64_t handle);

/* ---- UDP ---- */
int64_t gmb_net_udp_open(int port);
int gmb_net_udp_send(int64_t handle, const char* host, int port, const uint8_t* data, size_t len);
int gmb_net_udp_recv(int64_t handle, uint8_t* buf, size_t len, char* from_host, int from_host_cap, int* from_port);
void gmb_net_udp_close(int64_t handle);

/* ---- Introspezione (funzioni NET: CONN/REMOTEIP/REMOTEPORT/LOCALPORT/LOCALHOST/GETHOSTIP) ---- */
/* CONN(handle) → 1 se handle attivo, 0 altrimenti. */
int gmb_net_conn(int64_t handle);
int gmb_net_tcp_available(int64_t handle);
/* REMOTEIP(handle) → IP remoto (buffer scrivibile dal chiamante, cap byte). Ritorna 0 ok. */
int gmb_net_remote_ip(int64_t handle, char* out, int cap);
/* REMOTEPORT(handle) → porta remota (0 se non nota). */
int gmb_net_remote_port(int64_t handle);
/* LOCALPORT(handle) → porta locale (0 se non nota). */
int gmb_net_local_port(int64_t handle);
/* LOCALHOST() → IP locale del primo interfaccia (buffer del chiamante). Ritorna 0 ok. */
int gmb_net_local_host(char* out, int cap);
int gmb_net_resolve_host(const char* host, char* out, int cap);
/* GETHOSTIP(hostname) → IP del nome host (buffer del chiamante). Ritorna 0 ok, -1 risoluzione. */
int gmb_net_get_host_ip(const char* hostname, char* out, int cap);

/* ---- SHUTDOWN / SETOPTION ---- */
/* SHUTDOWN handle, mode ("both"/"send"/"receive" o 0/1/2). Ritorna 0 ok. */
int gmb_net_tcp_shutdown(int64_t handle, const char* mode);
/* SETOPTION handle, name ("TCP_NODELAY"/"SO_REUSEADDR"/"SO_KEEPALIVE"), value ("0"/"1"/"true"/"false"). */
int gmb_net_tcp_setopt(int64_t handle, const char* name, const char* value);

/* ---- SSL/TLS (via OpenSSL libssl) ---- */
/* Inizializza OpenSSL (chiamata da gmb_net_init, idempotente). */
void gmb_ssl_init(void);
/* Client TLS: connette a host:port e completa l'handshake. La validazione del certificato
   è SALTATA (coerente con SslConnect del NetworkService C#: skip per dev/self-signed).
   Ritorna handle SSL (>0) o -1. */
int64_t gmb_ssl_connect(const char* host, int port, int64_t timeout_ms);
/* Listener TLS: apre un socket in ascolto su porta con certificato (PFX/P12 o PEM).
   certPath/certPassword come SSL OPEN del BASIC. Ritorna handle listener (>0) o -1. */
int64_t gmb_ssl_listen(int port, const char* cert_path, const char* cert_password);
/* Accetta una connessione su un listener SSL e completa l'handshake TLS (server).
   Ritorna handle client SSL (>0) o -1 (non-blocking). */
int64_t gmb_ssl_accept(int64_t listener);
/* Invia dati su un handle SSL. Ritorna bytes scritti (>=0), -1 = riprovabile
   (SSL_WANT, vedi gmb_ssl_last_want), -2 = errore hard (V7b). */
int gmb_ssl_send(int64_t handle, const uint8_t* data, size_t len);
/* Riceve dati da un handle SSL. Ritorna bytes letti, 0 = chiuso, -1 = errore/non-pronto. */
int gmb_ssl_recv(int64_t handle, uint8_t* buf, size_t len);
/* Riceve dati attendendo finché disponibili (usa gmb_net_wait_fd + hook backend).
   timeout_ms: 0 = attesa indefinita. */
int gmb_ssl_recv_block(int64_t handle, uint8_t* buf, size_t len, int64_t timeout_ms);
/* V7b: ultimo SSL_WANT (0=nessuno, 1=read, 2=write) per il park con eventi corretti. */
int gmb_ssl_last_want(int64_t handle);
/* Chiude e rimuove un handle SSL (client o listener). */
void gmb_ssl_close(int64_t handle);
/* True se l'handle è un client SSL attivo (per TCP SEND/RECV su canale cifrato). */
int gmb_ssl_is_active(int64_t handle);
/* Fa l'handshake server su un socket già acceptato dal listener SSL (usato dal TC ACCEPT
   su listener SSL). Ritorna handle SSL client (>0) o -1. */
int64_t gmb_ssl_server_handshake(int64_t listener, int64_t client_socket);

/* ---- WebSocket (RFC 6455) ---- */
/* Opcode frame WebSocket. */
enum {
    GMB_WS_CONT = 0x0,
    GMB_WS_TEXT = 0x1,
    GMB_WS_BINARY = 0x2,
    GMB_WS_CLOSE = 0x8,
    GMB_WS_PING = 0x9,
    GMB_WS_PONG = 0xA,
};

/* Client: esegue l'handshake WebSocket verso ws://host:port/path. Ritorna un handle
   WebSocket (>0, riusa il registry) o -1. */
int64_t gmb_ws_connect(const char* host, int port, const char* path, int64_t timeout_ms);
/* Invia un frame di testo (client → masked). Ritorna bytes scritti o -1. */
int gmb_ws_send_text(int64_t handle, const char* text);
/* Invia un frame binario. Ritorna bytes scritti o -1. */
int gmb_ws_send_binary(int64_t handle, const uint8_t* data, size_t len);
/* Invia un frame di close con motivo (reason può essere NULL). Ritorna 0 ok / -1. */
int gmb_ws_send_close(int64_t handle, const char* reason);
/* Riceve UN frame completo (attende finché disponibile). out_opcode = GMB_WS_*.
   out_payload/out_payload_len: payload (malloc, chiamante libera con free).
   Ritorna 0 ok, -1 errore/chiuso. */
int gmb_ws_recv_frame(int64_t handle, int* out_opcode, char** out_payload, size_t* out_payload_len, int64_t timeout_ms);
/* Chiude e rimuove l'handle WebSocket. */
void gmb_ws_close(int64_t handle);
/* Handshake server WebSocket su un client HTTP già accettato (per HTTP ROUTE upgrade).
   client_socket è il fd TCP grezzo. Ritorna 0 ok / -1. Il Sec-WebSocket-Accept viene calcolato
   dal Sec-WebSocket-Key della richiesta (headers grezzi). */
int gmb_ws_server_handshake(int64_t client_socket, const char* request_headers);

/* ---- HTTP client (via libcurl se disponibile) ---- */
/* Fetch HTTP GET. Ritorna body (malloc, chiamante libera con free) o NULL. */
char* gmb_net_http_get(const char* url);

/* Client HTTP semplificato (GET/POST/PUT/DELETE): ritorna body (malloc, chiamante libera
   con free) o NULL su errore di rete. Fonte unica per C99/C99Native/VM-C (§0bis); method
   default "GET". body NULL/vuoto per GET/DELETE. */
char* gmb_net_http_body(const char* method, const char* url, const char* body);

/* Opzioni HTTP per richieste avanzate — mapping 1:1 con CommonLib.Http.HttpOptions (PCodeVM).
   Tutti i campi devono essere settati (default) dal chiamante. */
typedef struct {
    const char* method;              /* "GET"/"POST"/"PUT"/"DELETE"/... (default "GET") */
    const char* body;                /* corpo richiesta (stringa, UTF-8) o NULL */
    size_t body_len;                 /* lunghezza corpo (0 se body NULL) */
    const char** header_names;       /* array header custom (n) */
    const char** header_values;      /* array header custom (n) */
    int header_count;                /* numero header */
    long timeout_ms;                 /* timeout richiesta (default 30000) */
    int use_cookies;                 /* gestione cookie (jar globale) */
    const char* username;            /* Basic auth (se Authorization non esplicito) */
    const char* password;
    const char* bearer_token;        /* Bearer auth (se Authorization non esplicito) */
    const char* proxy_url;           /* proxy HTTP/HTTPS o NULL */
    const char* proxy_username;
    const char* proxy_password;
    const char* client_cert_path;    /* certificato client PFX/P12 o NULL */
    const char* client_cert_password;
    int allow_redirects;             /* follow 3xx (default 1) */
    int max_redirects;               /* default 10 */
    int allow_insecure_ssl;          /* disabilita validazione TLS (default 0) */
    const char* browser_preset;      /* "chrome"/"firefox"/"safari" o NULL */
    const char* pinned_cert_sha256;  /* pin certificato (hex, con/senza separatori) o NULL */
    /* Cookie header "k=v; k2=v2" montata dal COOKIEJAR del chiamante (NULL/vuoto = nessuno).
       Il client lo applica come header "Cookie:" quando nessun header Cookie esplicito è già
       presente in header_names — parità managed (CookieContainer allegato a HttpHandler). */
    const char* cookie_header;
} gmb_net_http_options_t;

/* Esegue una richiesta HTTP/HTTPS completa (mapping 1:1 con HttpClientHelper.PerformRequestAsync).
   out_status: codice di stato (0 = errore di rete, come FetchResult.StatusCode=0).
   out_body/out_body_len: corpo risposta (malloc, chiamante libera con free).
   out_error: messaggio errore (malloc) o NULL se OK.
   out_headers: header di risposta come stringa "Nome: Valore\n..." (malloc) o NULL.
   Ritorna 0 se la richiesta è stata eseguita (anche con status 4xx/5xx), -1 se errore
   irrecuperabile (out_error valorizzato). */
int gmb_net_http_request(const char* url, const gmb_net_http_options_t* opts,
                         int* out_status, char** out_body, size_t* out_body_len,
                         char** out_error, char** out_headers);

/* Scarica un URL su file in STREAMING (libcurl), scrivendo una barra di avanzamento
 * su stderr se show_progress!=0. La barra è disegnata INTERAMENTE dal C (nessuna
 * callback gestita, nessuna C->C#). Ritorna 0 su 2xx, -1 altrimenti (file rimosso). */
int gmb_net_http_download(const char* url, const char* dest_path, int show_progress);

/* Parsa un body multipart/form-data in array di stringhe C (campi name/value +
 * file name/filename/type/data). Ritorna 0 se parsato, -1 se Content-Type non è
 * multipart. Liberare con gmb_net_multipart_free. */
int gmb_net_http_parse_multipart(const char* body, const char* content_type,
                                 char*** out_fn, char*** out_fv, int* out_nf,
                                 char*** out_fln, char*** out_ffn, char*** out_ft, char*** out_fd, int* out_nfl);
void gmb_net_multipart_free(char** names, char** vals, char** files, int n);

/* req.ParseMultipartForm() per il backend C99: parsa il body multipart dal dict req
 * (chiavi Body/__content_type) e ritorna il dict HTTPPOSTEDDATA { FormFields, FileFields }. */
gmb_val_t gmb_http_req_parse_multipart(gmb_val_t req);
/* HTTPREQUEST.ParseForm(): body application/x-www-form-urlencoded → dict (valori stringa,
 * URL-decodificati); NIL se il Content-Type non è form-urlencoded. Sorgente UNICA C (§0bis). */
gmb_val_t gmb_http_req_parse_form(gmb_val_t req);
/* Parser form-urlencoded MODEL-AGNOSTICO (§0bis): body+Content-Type → array di chiavi/valori
 * (stringhe URL-decodificate), che ogni VM converte nel proprio modello oggetti. 0 = ok
 * (anche body vuoto), -1 = Content-Type non form-urlencoded. Liberare con gmb_http_form_free. */
int gmb_http_form_parse(const char* body, const char* ct, char*** keys_out, char*** vals_out, int* n_out);
void gmb_http_form_free(char** keys, char** vals, int n);

/* ---- HTTP server (route verso la VM) ---- */
/* Richiesta HTTP decodificata dal server, passata all'handler. */
typedef struct {
    const char* method;              /* "GET"/"POST"/... */
    const char* path;                /* path senza query (es. "/users/5") */
    const char* query;               /* stringa query (senza '?') o "" */
    const char* body;                /* corpo richiesta (dopo header) o "" */
    const char** header_names;       /* header (header_count) */
    const char** header_values;
    int header_count;
    const char* remote_ip;           /* IP del client o "" */
    char params[16][64];             /* parametri {x} della route (valori) */
    int params_count;
    char param_names[16][64];        /* nomi dei parametri {x} della route */
    int param_names_count;
    int is_websocket_upgrade;        /* 1 se Upgrade: websocket */
    int64_t ws_handle;               /* handle WS accettato (solo se is_websocket_upgrade) */
    /* HTTP USE NEXT — id dello step corrente nella catena middleware (0 = passo finale /
     * nessuna catena). Il wrapper VM/C lo copia nel task (task->http_chain_step) e lo
     * ripassera' come 3° argomento (handle) quando la catena ha piu' di un passo. */
    int64_t chain_step_id;
    /* Streaming (chunked): sink della connessione, opaco, allocato dal server PRIMA del
     * dispatch. Il wrapper dell'handler lo copia nel dict della risposta come
     * "__stream_handle"; da li' res.StartStream/Write/Flush/EndStream parlano col socket.
     * 0 = risposta bufferizzata (comportamento storico). */
    int64_t stream_handle;
} gmb_http_request_t;

/* Callback per l'handler BASIC: riceve la richiesta decodificata, scrive la risposta.
   - out_body: corpo risposta (malloc, chiamante libera con free)
   - out_status: status code (in/out; default 200)
   - out_headers: header risposta come "Nome: Valore\n" (malloc) o NULL
   Ritorna 0 se gestita, -1 se non gestita (il server prova il fallback statico/404).
   Per un upgrade WebSocket, 1 = la VM ha preso in carico la sessione (receive-loop
   avviato: il server NON deve fare echo né chiudere il socket). */
typedef int (*gmb_http_handler_fn)(void* ctx, const gmb_http_request_t* req,
                                   char** out_body, int* out_status, char** out_headers);

/* Handler BINARIO: come sopra, ma dichiara la LUNGHEZZA del corpo. `out_body` non e'
 * NULL-terminato-puo' (i dati possono contenere NUL): il server usa out_body_len.
 * Serve a res.SendFile (gmb_vm_core.c), che serve un file dal disco — uno ZIP ne ha.
 * La firma storica resta quella di sopra perche' il C generato da gmb_c99native chiama
 * gmb_http_server_route: aggiungere un parametro avrebbe rotto quella firma. */
typedef int (*gmb_http_handler_bin_fn)(void* ctx, const gmb_http_request_t* req,
                                       char** out_body, size_t* out_body_len,
                                       int* out_status, char** out_headers);

/* Avvia un server HTTP su porta. Ritorna handle (>0) o -1. */
int64_t gmb_http_server_start(int port);

/* Registra una route con handler BINARIO (vedi gmb_http_handler_bin_fn). Le route
 * registrate qui e quelle registrate con gmb_http_server_route coesistono: ogni route
 * ricorda quale delle due forme ha. */
int gmb_http_server_route_bin(int64_t server, const char* method, const char* path,
                               gmb_http_handler_bin_fn handler, void* ctx);

/* HTTP USE — registra un middleware nella catena che precede le route. method e' "*"
 * nella sintassi legacy; path_prefix e' un PREFISSO (StartsWith, non template), nil/""
 * → "/". Stesse due forme delle route (testuale/binaria). */
int gmb_http_server_use_bin(int64_t server, const char* method, const char* path_prefix,
                            gmb_http_handler_bin_fn handler, void* ctx);
int gmb_http_server_use(int64_t server, const char* method, const char* path_prefix,
                        gmb_http_handler_fn handler, void* ctx);

/* Catena middleware — registry degli step di avanzamento (HTTP USE NEXT).
 * alloc: id >= 1 (0 = registry esausto); advance: marca l'advance dello step;
 * is_advanced/free: lettura e rilascio. gmb_http_use_next(step_id) e' la forma
 * BASIC: step_id>0 = handle esplicito (`HTTP USE NEXT h`), 0/omesso = forma implicita
 * (usa lo step corrente del thread — gmb_http_current_step, impostato dal dispatch). */
int64_t gmb_http_chain_step_alloc(void);
int gmb_http_chain_step_advance(int64_t id);
int gmb_http_chain_step_is_advanced(int64_t id);
void gmb_http_chain_step_free(int64_t id);
int gmb_http_use_next(int64_t step_id);
int64_t gmb_http_current_step(void);
/* Arma/azzera lo step corrente del thread chiamante. Il dispatch C lo imposta sul
 * thread della connessione PRIMA di chiamare l'handler; un backend che esegue il body
 * BASIC su un WORKER GPM (C99Native task-spawn) lo ri-imposta nel thread del
 * task (dal req->chain_step_id) così la forma implicita `HTTP USE NEXT` (senza handle)
 * risolve lo step giusto. 0 = disarmato. */
void gmb_http_set_current_step(int64_t step_id);

/* Serializzazione client-side degli header HTTPOPTIONS nel FETCH (unica sorgente §0bis,
 * vedi l'implementazione in gmb_network.c): enumera TUTTI i capi del dict (non solo i 3
 * noti), mappa ContentType/UserAgent ai nomi wire, dedup case-insensitive. Ritorna il
 * numero di header scritti (≤ max). */
int gmb_http_headers_from_dict(gmb_dict_t* hdrs, const char** names, const char** vals, int max);

/* Avvia un server HTTP su TLS (HTTPS): cert_path/cert_password (PEM o PFX/P12).
   Ritorna handle (>0) o -1. */
int64_t gmb_http_server_start_tls(int port, const char* cert_path, const char* cert_password);

/* HTTP CONFIG cfg: salva il certificato TLS per il prossimo HTTP SERVER. */
void gmb_http_server_set_cert(const char* path, const char* pass);
/* HTTP SERVER: dispatch plain/TLS a seconda di HTTP CONFIG precedente. */
int64_t gmb_http_server_start_tls_or_plain(int port);

/* Query string → val dict (parità IL/PCode/VM: req.Query è un dict di parametri). */
gmb_val_t gmb_http_query_dict(const char* qs);

/* BodyJson canonico dei dict req (parità PCode BuildRequestDict): JSON del body SOLO se
 * Content-Type contiene "application/json" e body non vuoto; altrimenti nil. Unica sorgente
 * per i wrapper VM/C, C99Native (AGENTS §0bis). */
gmb_val_t gmb_http_body_json(const char* content_type, const char* body);

/* COOKIEJAR/sessione — UNICA sorgente C (AGENTS §0bis) per VM/C, C99Native.
 * Il jar è il dict { nome-cookie → valore } dell'oggetto NEW COOKIEJAR.
 * gmb_http_parse_session_id: valore di GM_BASIC_SESSION dall'header Cookie (ritorna
 *   malloc'd o NULL; valore crudo, senza trim — parità del helper managed
 *   ParseSessionIdCookie).
 * gmb_http_jar_cookie_header: "k=v; k2=v2" (malloc'd) dai cookie del jar, NULL se vuoto.
 * gmb_http_jar_capture: memorizza ogni "Set-Cookie: name=value(...)" del blocco header
 *   di risposta nel dict del jar. */
char* gmb_http_parse_session_id(const char* cookie_header);
char* gmb_http_jar_cookie_header(const gmb_dict_t* jar);
void gmb_http_jar_capture(gmb_dict_t* jar, const char* headers_block);

/* HTTPRESPONSE.SetCookie(name, value) — UNICA sorgente C (AGENTS §0bis) per VM/C,
 * C99Native. Appende "Set-Cookie: name=value\n" alla stringa __headers
 * del dict res; l'handler wrapper legge __headers e lo monta su *out_headers, il
 * runtime server (gmb_http_serve_connection) propaga le righe aggiuntive in risposta. */
void gmb_http_response_set_cookie(gmb_val_t res, const char* name, const char* value);
/* Come per SetHeader — una riga arbitraria "Nome: Valore\n" aggiunta a __headers. */
void gmb_http_response_append_header(gmb_val_t res, const char* line);
/* res.SendFile(path [, contentType]): legge il file (BINARIO, anche con NUL — uno ZIP) e lo
 * deposita in Body + "__body_len" del dict risposta (la route BINARIA usa la lunghezza
 * esplicita, non strlen). ContentType esplicito, altrimenti inferito dall'estensione con
 * gmb_http_guess_content_type. §0bis: sorgente UNICA condivisa C99Native/VM-C.
 * null-mode/headless: scrive comunque nel dict. */
void gmb_http_response_send_file(gmb_val_t res, const char* path, const char* content_type);

/* Estensione -> Content-Type (parità bit-exact con PCodeHttpResponse.GuessContentType del
 * reference C#). Sorgente UNICA usata dal helper condiviso e dal VM/C inline. Ritorna sempre
 * una stringa statica (mai NULL). */
const char* gmb_http_guess_content_type(const char* path);

/* Range HTTP (RFC 9110 §14.1): parse di un header "bytes=start-end" per un corpo di `total`
 * byte. Ritorna:
 *    1 = range valido  (*start/*end inclusivi, già clampati dentro [0, total-1]);
 *    0 = nessun range  (header assente, malformato o MULTI-range: si serve il corpo intero, 200);
 *   -1 = NON soddisfacibile (start >= total → il server risponde 416 con "bytes *‍/total").
 * SORGENTE UNICA C (§0bis) per il server nativo (VM/C, C99Native); le gambe managed hanno
 * l'equivalente in CommonLib.Http.HttpRange, verificate dallo stesso MiniTest.
 * Forme supportate: "bytes=0-99", "bytes=100-", "bytes=-50" (ultimi 50 byte). */
int gmb_http_parse_range(const char* header, long long total, long long* start, long long* end);

/* ── Streaming (chunked) di una risposta HTTP ─────────────────────────────────
 * Una risposta può essere inviata PROGRESSIVAMENTE (SSE, pagine lunghe, progressbar)
 * invece che bufferizzata a fine handler. Il server alloca un sink per connessione e lo
 * passa all'handler in req->stream_handle; il wrapper lo deposita nel dict della risposta
 * come "__stream_handle". Da lì:
 *   res.StartStream("text/event-stream") → status + header con Transfer-Encoding: chunked
 *   res.Write(testo)                     → accoda (flush automatico oltre 8 KB)
 *   res.Flush()                          → manda ORA il chunk al client
 *   res.EndStream()                      → flush + chunk terminale, chiude la risposta
 * SORGENTE UNICA C (§0bis): VM/C e C99Native chiamano SOLO queste funzioni. Con lo
 * streaming attivo il server NON invia la risposta bufferizzata (niente Content-Length,
 * niente ETag/gzip/render cache: sono semantiche di una risposta completa).
 * Ritorno: 0 = ok, -1 = nessun sink (risposta non streammabile in questo contesto). */
int gmb_http_response_stream_begin(gmb_val_t res, const char* content_type);
int gmb_http_response_stream_write(gmb_val_t res, const char* text);
int gmb_http_response_stream_write_bytes(gmb_val_t res, const void* data, size_t len);
int gmb_http_response_stream_flush(gmb_val_t res);
int gmb_http_response_stream_end(gmb_val_t res);
/* 1 se lo stream di questa risposta è già iniziato (il server non deve bufferizzare). */
int gmb_http_response_is_streaming(gmb_val_t res);

/* Default NEW HTTPCONFIG: unica sorgente C (AGENTS §0bis) per VM/C (OP_NEW 23) e
 * C99Native. Chiavi = CommonLib.Http.HttpConfig (incluso "DefaultRequestTimeout" come chiave
 * propria, alias sincronizzato solo nell'oggetto C#). seed popola un dict esistente;
 * new crea+popola e ritorna il val boxed. */
void gmb_httpconfig_seed(gmb_dict_t* d);
gmb_val_t gmb_httpconfig_new(void);

/* Chiavi scalari canoniche del dict FETCHRESULT (StatusCode, IsSuccess -1/0, Body, Url,
   ErrorMessage, ContentLength=byte body). UNICA sorgente C per VM/C, C99Native
   (AGENTS §0bis); la chiave "Headers" è aggiunta dal chiamante col dict del PROPRIO mondo
   (shared gmb_obj_shared_val_t per VM/C, dict regolare per gli emitter C99) riempito con
   gmb_http_headers_fill. */
void gmb_http_fetchresult_seed(gmb_dict_t* d, int status, const char* body, size_t body_len,
                               const char* url, const char* err);

/* Riempie il dict (case-preserving {nome → valore}) con gli header di RISPOSTA: righe
   "Nome: Valore\n" (formato di http_header_cb). UNICA implementazione del parsing (§0bis):
   la usano la costruzione shared di VM/C e quella regolare di C99Native, così
   `FetchResult.Headers.Get("k")` è identica sui backend C (bug Headers-Get-Divergenza-Backend).
   d = NULL o headers_raw = NULL → no-op (dict lasciato invariato). */
void gmb_http_headers_fill(gmb_dict_t* d, const char* headers_raw);

/* FETCHRESULT.SaveToFile(path): scrive i byte di "Body" (length-aware, binario-safe).
   0 ok, -1 errore. Usato dalle emissioni member-call di VM/C, C99Native. */
int gmb_http_fetchresult_save(gmb_val_t res, const char* path);

/* Registra una route. method es: "GET"/"POST"/"*". handler/ctx per le chiamate. */
int gmb_http_server_route(int64_t server, const char* method, const char* path,
                          gmb_http_handler_fn handler, void* ctx);

/* Registra una mappatura statica prefix→dir fisica (HTTP STATIC). */
int gmb_http_server_static(int64_t server, const char* prefix, const char* dir);

/* Directory radice delle pagine .bsp (HTTP BSP). Path URL `*.bsp` → gmb_bsp_try_serve. */
int gmb_http_server_bsp(int64_t server, const char* dir);

/* Imposta il timeout richiesta del server (ms). */
int gmb_http_server_set_timeout(int64_t server, int timeout_ms);
/* Security (2026-09-02): header automatici + IP allow/block (CIDR IPv4) — parità HTTP CONFIG IL/PCode. */
int gmb_http_server_set_security(int64_t server, int security_headers, const char* allowed, const char* blocked);
/* CONFIG MaxRequestBodySize (0 = illimitato) → 413 su Content-Length oltre soglia. */
int gmb_http_server_set_max_body(int64_t server, long bytes);
/* CONFIG RateLimitRequestsPerMinute/RateLimitBurstSize — 429 token bucket. */
int gmb_http_server_set_rate_limit(int64_t server, int per_min, int burst);
/* Session store nativo (HTTP.SESSIONCREATE / GMBSESSION) — gmb_network.c. */
char* gmb_session_create(const char* user, const char* role);
int gmb_session_verify(const char* id);
int gmb_session_get(const char* id, char* user_out, size_t user_cap, char* role_out, size_t role_cap);

/* Ferma e chiude il server. */
void gmb_http_server_stop(int64_t server);

#ifdef __cplusplus
}
#endif

#endif /* GMB_NETWORK_H */

gmb_string_t gmb_net_remote_ip_str(int64_t handle);
gmb_string_t gmb_net_local_host_str(void);
gmb_string_t gmb_net_resolve_host_str(const char* host);
