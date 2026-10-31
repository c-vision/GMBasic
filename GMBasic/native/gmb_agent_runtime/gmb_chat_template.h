/* gmb_chat_template.h — risoluzione AUTOMATICA del chat_template del modello nel mondo C,
 * in parita' con common/AI/Providers/ChatTranscripts.ResolveFromDir (C#) e con il motore
 * Jinja nativo native/llmjinja. Indipendente da C# (nessun bridge/P-Invoke).
 *
 * Dato `model_dir`:
 *  - cartella: legge `chat_template` da `tokenizer_config.json` (stringa, oppure prima stringa
 *    di una lista, oppure lista di oggetti con campo `template`) e, se assente, il file
 *    `chat_template.jinja`; legge `bos_token`/`eos_token` (stringa o oggetto con `content`)
 *    da `tokenizer_config.json` poi `config.json`;
 *  - file `.gguf`: legge la KV `tokenizer.chat_template` dal GGUF (header-only gmb_gguf.h).
 *
 * Se un template esiste, lo rende con le variabili
 *   { messages:[{system,<system>} (solo se non vuoto), {user,<transcript>}],
 *     add_generation_prompt:true, bos_token, eos_token }
 * via llmjinja_render (parità MlxProviders.Complete + ChatTranscripts.ResolveFromDir C#).
 * In caso di assenza/errore di I/O o di render ritorna -1 con il motivo in `err`, cosi' il
 * chiamante degrada al prompt scritto a mano (DeepSeek/ChatML). Fail-loud: nessun output
 * approssimato, nessuno stub.
 */
#ifndef GMB_CHAT_TEMPLATE_H
#define GMB_CHAT_TEMPLATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ritorna 0 e scrive il prompt in `out` (NUL-terminato, capacita' `cap`) se il modello ha un
 * chat_template renderizzabile; -1 altrimenti (fallback), con motivo in `err`.
 * `system` può essere NULL/vuoto (nessun messaggio system, come C#). */
int gmb_chat_template_build(const char* model_dir, const char* system, const char* transcript,
                            char* out, size_t cap, char* err, size_t errcap);

/* Stringa `key` (es. "eos_token"/"bos_token") da tokenizer_config.json/config.json della cartella
 * MLX (stringa o oggetto `content`). Ritorna malloc'd (il chiamante fa free()) o NULL. */
char* gmb_chat_template_read_token(const char* model_dir, const char* key);

#ifdef __cplusplus
}
#endif
#endif /* GMB_CHAT_TEMPLATE_H */
