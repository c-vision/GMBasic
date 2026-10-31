/* gmb_ai_server.h — gestione del server LLM OpenAI-compatible (gmb_llm_server) come PROCESSO
 * ESTERNO, versione C (mondo C). Analogo C di common/AI/AiServerManager.cs (mondo C#): i due
 * mondi non condividono nulla; la comunicazione e' processo + HTTP (mai bridge in-process).
 * Usato dai backend C (VM/C, C99Native) per implementare il comando `AI SERVER`.
 *
 * Binario: variabile d'ambiente GMBASIC_LLM_SERVER (path assoluto) o il default compilato
 * GMB_AI_SERVER_BIN. Se assente -> errore esplicito (mai stub).
 * API: start avvia il processo e attende readiness (GET /v1/status loaded:true); stop lo
 * ferma; status interroga lo stato live; load/unload gestiscono la residenza
 * (POST /v1/models/load|unload). Funzioni non supportate (piattaforma non unix) -> errore.
 */
#ifndef GMB_AI_SERVER_H
#define GMB_AI_SERVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GMB_AI_SERVER_BIN
#define GMB_AI_SERVER_BIN "gmb_llm_server"
#endif

typedef struct gmb_ai_server_status {
    int running;              /* processo vivo */
    int pid;
    int port;
    int loaded;               /* modello residente */
    int busy;
    char model[128];
    char err[256];
} gmb_ai_server_status_t;

/* Avvia il server come figlio (fork/exec) con readiness (timeout 60s). Ritorna 0 ok e
 * *pid_out = pid; -1 errore (err/errcap valorizzati). max_active>0 => GMB_MAX_ACTIVE. */
int gmb_ai_server_start(const char* model_dir, int port, int max_active,
                        int* pid_out, char* err, size_t errcap);

/* Ferma il processo (SIGTERM). Ritorna 0 ok, -1 se non vivo o kill fallisce. */
int gmb_ai_server_stop(int pid, char* err, size_t errcap);

/* Stato live del server (HTTP GET /v1/status). out->running = processo vivo. */
int gmb_ai_server_status(int pid, int port, gmb_ai_server_status_t* out);

/* Residenza: carica e tiene residente il modello dir (POST /v1/models/load). 0 ok. */
int gmb_ai_server_load(int pid, int port, const char* model_dir, char* err, size_t errcap);

/* Residenza: libera il modello residente (POST /v1/models/unload). 0 ok. */
int gmb_ai_server_unload(int pid, int port, char* err, size_t errcap);

/* ---- strato "verbi" (server corrente + JSON), specchio di IlRuntime.AiServer* ----
 * Gestiscono un server "corrente" di modulo (pid/port salvati all'ultimo start), cosi' i
 * verbi senza argomenti (STATUS/STOP/UNLOAD) e quelli con argomento (START/MODEL) bastano a
 * un backend con una sola chiamata per verbo. OUTPUT: JSON (ok/error) scritto in out.
 * Ritornano 0 (JSON in out) o -1 (errore interno non serializzabile). Mai stub. */
int gmb_ai_server_json_start(const char* model_dir, int port, int max_active, char* out, size_t cap);
int gmb_ai_server_json_stop(char* out, size_t cap);
int gmb_ai_server_json_status(char* out, size_t cap);
int gmb_ai_server_json_load(const char* model_dir, char* out, size_t cap);
int gmb_ai_server_json_unload(char* out, size_t cap);
int gmb_ai_server_current_pid(void);
int gmb_ai_server_current_port(void);

/* Wrapper "statement" (per l'emitter C99/VM): eseguono il verbo con default
 * (porta 8020, nessun argomento extra) senza buffer di output; ritornano 0/-1. */
int gmb_ai_server_stmt_start(const char* model_dir);
int gmb_ai_server_stmt_start_port(const char* model_dir, int port);
int gmb_ai_server_stmt_stop(void);
int gmb_ai_server_stmt_status(void);
int gmb_ai_server_stmt_model(const char* model_dir);
int gmb_ai_server_stmt_unload(void);

#ifdef __cplusplus
}
#endif
#endif
