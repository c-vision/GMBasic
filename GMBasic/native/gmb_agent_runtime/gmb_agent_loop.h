/* gmb_agent_loop.h — loop Brain→Azione (CUA §19-20: ACTION→OBSERVE→VERIFY).
 * Il brain (LLM MLX/llama/esterno) è pluggabile: il loop gli dà il transcript e lui
 * risponde con un AgentAction JSON (o la risposta finale). Le azioni sono eseguite dal
 * runtime (gmb_agent_action: browser/UI/computer/tool) e l'osservazione torna al brain.
 * Policy: capability ALLOW/DENY dell'agente applicate a ogni step. */
#ifndef GMB_AGENT_LOOP_H
#define GMB_AGENT_LOOP_H
#include <stddef.h>

/* Brain: dato il transcript (task+storia), produce la prossima risposta in out (cap).
   Ritorna 0 ok, -1 errore/stop. */
typedef int (*gmb_brain_fn)(void* ctx, const char* transcript, char* out, size_t cap);

/* Esegue il loop. goal = task dell'agente; agent = nome (per la capability policy, può
   essere ""). log (logcap) riceve il transcript completo dei passi. Ritorna 0 se terminato
   con risposta finale, 1 se il loop è terminato per max_steps, -1 errore grave. */
int gmb_agent_loop_run(gmb_brain_fn brain, void* ctx, const char* agent, const char* goal,
                       int max_steps, char* log, size_t logcap);

/* ---- Seam brain "GMBasic-native" (dimostrazione con l'embedding GMBasic, NON server
   esterni). L'host/embedding GMBasic (gmb_host_ai_mlx / completer) registra qui la sua
   gmb_brain_fn; gmb_agent_run_goal la usa. Finché non è registrato → errore ESPLICITO. */
int gmb_agent_brain_register(gmb_brain_fn brain, void* ctx);   /* 0 ok */
void gmb_agent_brain_clear(void);
int gmb_agent_brain_is_set(void);

/* §4.10 esposizione agenti (lato C): SEAM verso il compilatore per i tool `code.*` (grafo
 * semantico del codice). `path`+`verb`(SYMBOL|CALLMAP|IMPACT|CONTEXT|FINDSYMBOL|FILEOUTLINE|
 * REPOMAP)+`arg`+`depth` -> stringa canonica in `out` (0 ok / -1). L'host che linka il
 * compilatore registra `gmb_semquery_file`; senza registrazione i tool code.* danno errore
 * ESPLICITO (mai no-op). Il runtime agenti NON linka il compilatore: dipendenza via seam. */
typedef int (*gmb_agent_code_query_fn)(const char* path, const char* verb, const char* arg, int depth, char* out, size_t cap);
void gmb_agent_code_query_set(gmb_agent_code_query_fn fn);

/* AI.AGENTRUNCODE(name, prompt [, rounds]): loop agent con i tool `code.*` (grafo del codice).
 * Parità AgentServer.AskCode (C#). Richiede il seam code.* (gmb_agent_code_query_set) e un brain.
 * Ritorna malloc'd (mai NULL). */
char* gmb_agent_run_code(const char* name, const char* prompt, int rounds);

/* Esegue il loop con il brain GMBasic registrato (se assente → -1 e log esplicito).
   Fallback automatico SOLO su config esplicita GMB_BRAIN_URL (adapter HTTP OpenAI-compat). */
int gmb_agent_run_goal(const char* agent, const char* goal, int max_steps, char* log, size_t logcap);

/* Ask one-shot al brain (testo libero, per CodeGen). GMB_MLX_MODEL_DIR o brain registrato.
   0 ok / -1 con messaggio in out. */
int gmb_brain_ask_sync(const char* prompt, char* out, size_t cap);

/* AI.AGENTRUN(name, goal[, rounds]): loop CUA nativo sull'agente `name` (parità
   AgentServer.AskCua C#: tool_calls JSON, non il loop azione {"action":...}). Brain
   registrato/MLX/HTTP, altrimenti fixture deterministica. Ritorna malloc'd (mai NULL). */
char* gmb_agent_run_named(const char* name, const char* goal, int max_steps);
char* gmb_agent_run_cua(const char* name, const char* goal, int rounds);

/* Tool-loop di AI AGENT ASK (parità AgentToolLoop.AskWithTools C#): prompt con l'elenco
   tool del registry, round brain → parse tool_calls → esecuzione → osservazione, fino a
   testo finale o cap round. Riusa gmb_cua_parse_calls/gmb_cua_execute del loop CUA.
   Ritorna malloc'd (mai NULL), oppure NULL se il brain non è disponibile al primo round
   (il chiamante torna al path single-ask invariato). max_rounds <= 0 → 5. */
char* gmb_agent_ask_with_tools(const char* agent, const char* prompt, int max_rounds);

#endif
