/* ============================================================================
 * gmb_state_hook.h — hook OPZIONALE per lo stato di agenti/team/workflow/memoria.
 *
 * PERCHÉ. Il runtime C ha un proprio registro di agenti/team/workflow/memoria
 * (gmb_agent.c, gmb_team.c, gmb_workflow.c, gmb_memory.c): quando la VM/C gira da
 * sola (C99Native, gmb_vm_host) quello è l'unico stato che esiste. Quando invece la
 * VM/C è OSPITATA da un processo managed (motore BSP in C chiamato dal C# via
 * P/Invoke), lo stato "vero" vive nel runtime managed: senza un ponte la stessa
 * pagina BSP resa dai due motori mostrerebbe agenti/team/workflow/memoria diversi.
 *
 * COME. Il client C# registra una funzione; la VM/C la interroga prima del proprio
 * registro e usa la risposta solo se c'è. Se nessuno la registra (C99Native, VM/C
 * standalone) ogni getter/mutatore cade sul registro C: comportamento INVARIATO.
 *
 * REGOLE (AGENTS §Regola primaria: gli hook C→C# sono ammessi con 3 condizioni)
 *   1. NULL-safe: senza registrazione la VM/C funziona identica (`g_state_fn == NULL`).
 *   2. GPM: l'hook è chiamato sul thread della richiesta e NON deve eseguire codice
 *      BASIC — solo operazioni sul runtime ospite.
 *   3. Licenza: il runtime C non linka il managed. Il puntatore è OPACO (`void* ctx`)
 *      e la funzione è fornita dal client; nessuna dipendenza di build.
 *
 * ABI: `int fn(void* ctx, const char* surface, const char* a, const char* b,
 *             const char* c, char* out, int cap)`
 *   ritorna >= 0 = lunghezza in byte della risposta UTF-8 (scritta in `out` fino a
 *             `cap`; se > cap il chiamante rialloca e richiama);
 *   ritorna < 0 = "non gestita" → il runtime C usa il proprio registro.
 * ============================================================================ */
#ifndef GMB_STATE_HOOK_H
#define GMB_STATE_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*gmb_state_query_fn)(void* ctx, const char* surface,
                                  const char* a, const char* b, const char* c,
                                  char* out, int cap);

/* Registra (o rimuove, con fn = NULL) l'hook. Da chiamare una volta all'avvio
 * dell'host; non è protetto da lock (l'hook è impostato prima delle richieste). */
void gmb_state_hook_set(gmb_state_query_fn fn, void* ctx);

/* True se un hook è registrato. */
int gmb_state_hook_active(void);

/* Interroga l'hook per una LETTURA. Ritorna un buffer malloc (caller free) o NULL se
 * l'hook non è registrato / la superficie non è gestita. Superfici (una risposta per
 * riga quando è una lista):
 *   "agent.list"                 → nomi, \n
 *   "agent.get"      (nome, chiave) → valore
 *   "agent.status"   (nome)         → stato
 *   "agent.events"                 → eventi, \n
 *   "team.list"                    → nomi, \n
 *   "team.agents"    (team)        → nomi/ruoli come stringa del runtime
 *   "workflow.list"                → nomi, \n
 *   "workflow.status"(nome)        → stato
 *   "workflow.steps" (nome)        → passi
 *   "memory.list"                  → chiavi, \n
 *   "memory.load"    (chiave)      → valore
 *   "memory.search"  (pattern)     → risultati
 */
char* gmb_state_hook_query(const char* surface, const char* a, const char* b);

/* Variante a TRE argomenti (es. "agent.askfile"(nome,path,domanda)). */
char* gmb_state_hook_query3(const char* surface, const char* a, const char* b, const char* c);

/* Interroga l'hook per una MUTAZIONE. Ritorna 1 = riuscita, 0 = rifiutata,
 * -1 = non gestita (nessun hook / superficie non gestita → il runtime C esegue la
 * propria). Operazioni:
 *   "agent.create"(nome) · "agent.delete"(nome) · "agent.set"(nome,chiave,valore)
 *   "team.create"(team) · "team.delete"(team) · "team.add"(team,agente)
 *   "team.remove"(team,agente) · "team.set_role"(team,agente,ruolo)
 *   "team.set_strategy"(team,strategia) · "team.set_coordinator"(team,agente)
 *   "workflow.begin"(nome) · "workflow.step"(nome,azione,retry|timeout)
 *   "workflow.end"() · "workflow.execute"(nome) · "workflow.delete"(nome)
 *   "memory.store"(chiave,valore,longTerm) · "memory.delete"(chiave)
 */
int gmb_state_hook_mutate(const char* op, const char* a, const char* b, const char* c);

/* Canale d'errore: se il client managed risponde con il marcatore `\x01` + messaggio, la
 * chiamata ritorna "non gestita" e il messaggio resta qui (thread-local) perché il VM possa
 * sollevare lo STESSO errore BASIC della gamba managed (schema ON ERROR/TRY della pagina).
 * `gmb_state_hook_take_error` ritorna il messaggio (malloc, caller free) e azzera il canale. */
char* gmb_state_hook_take_error(void);
void  gmb_state_hook_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_STATE_HOOK_H */
