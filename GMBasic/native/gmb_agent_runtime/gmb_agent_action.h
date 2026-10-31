/* gmb_agent_action.h — Fase 5 (fondamenta): interpreta l'output del Brain (testo con un
 * AgentAction JSON) e lo dispatcha al runtime (ToolRegistry/UI/BROWSER/CDP), producendo
 * un'osservazione. Formati accettati:
 *   {"action":{"tool":"browser.navigate","args":{"url":"https://..."}}}
 *   {"tool_call":{"name":"ui.click","arguments":{"target":"Save"}}}
 * Un file/dominio, <1200 righe. */
#ifndef GMB_AGENT_ACTION_H
#define GMB_AGENT_ACTION_H

#include <stddef.h>

/* Esegue la prima azione strutturata trovata in brain_text. out_obs (cap) riceve
 * un'osservazione testuale/JSON. Ritorna 0 azione eseguita, 1 nessuna azione trovata,
 * -1 errore di dispatch. */
int gmb_agent_action_run(const char* brain_text, char* out_obs, size_t cap);

#endif
