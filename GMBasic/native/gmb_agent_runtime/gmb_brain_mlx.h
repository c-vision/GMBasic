/* gmb_brain_mlx.h — Brain MLX per il loop C (Fase 5, L203).
 * Collega gmb_agent_loop al motore embedded gmb_host_ai_mlx (dylib) + tokenizer BPE,
 * caricati via dlopen (nessuna dipendenza di link). Prompt in formato chat Qwen/GLM
 * (<|im_start|>...<|im_end|>). Tutto in C, nessun ponte col C#. */
#ifndef GMB_BRAIN_MLX_H
#define GMB_BRAIN_MLX_H
#include "gmb_agent_loop.h"

/* Avvia il brain MLX sul model_dir (es. ~/ai/Models/Qwen3.8-27B-8bit): carica
 * tokenizer.json + modello + sessione. Ritorna 0 ok, -1 errore (messaggio in err, cap). */
int gmb_brain_mlx_start(const char* model_dir, char* err, size_t errcap);

/* gmb_brain_fn (vedi gmb_agent_loop.h). */
int gmb_brain_mlx_complete(void* ctx, const char* transcript, char* out, size_t cap);

void gmb_brain_mlx_stop(void);

/* System prompt risolto dell'agente (parità AgentRecord.ResolveSystemPrompt), usato dal render
 * col chat_template del modello come primo messaggio `system`. Va impostato prima di
 * gmb_brain_ask_sync (regge l'ordine first-call: vale anche se il brain non è ancora avviato).
 * Vuoto/NULL = nessun messaggio system (parità MlxProviders.Complete con system vuoto). */
void gmb_brain_mlx_set_system(const char* sys);
void gmb_brain_mlx_clear_system(void);

/* Limite di token NUOVI per generazione (parità agent.MaxTokens C#). `n <= 0` = default (MAX_NEW).
 * Come il system, regge l'ordine first-call: va impostato prima di gmb_brain_ask_sync. */
void gmb_brain_mlx_set_max_new(int n);

#endif

/* Auto-wire: avvia il brain MLX su model_dir e lo registra come brain del loop.
   Ritorna 0 ok (-1 errore; dettaglio in log, cap). */
int gmb_brain_mlx_auto(const char* model_dir, char* log, size_t logcap);
