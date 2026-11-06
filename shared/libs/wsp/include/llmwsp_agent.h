/* llmwsp_agent.h — W9 del piano Weight-Space-Programming: sessione "agente" con strumenti a verbi, budget, trascrizione
 * e REPLAY deterministico.
 *
 * La sessione è il livello C che un agente (AI AGENT di GMBasic, o qualunque LLM esterno) può pilotare con comandi testuali di una
 * riga: una sandbox di DATI. Nessun verbo apre rete, shell o file arbitrari; i verbi non ammessi sono RIFIUTATI e registrati.
 * Verbi (output deterministico, float con %.6g):
 *   lettura:   NORMS <layer> | SPECTRUM <layer> <head> <ov|qk> | COMPOSE <l1> <h1> <l2> <h2> <q|k|v> | JOURNAL | SUMMARY | SHA <tensore>
 *   dati:      DEFINE <nome> <v1> <v2> ...                      (vettori nominati, senza effetti sul modello)
 *   edit:      SCALE <addr> <alpha> | ZERO <addr> | SET <addr> <nome> | ADDDELTA <addr> <nome> | COPY <dst> <src>
 *              PROJECTOUT <addr> <asse_vettori> <flags> <len> <nome>
 *   controllo: CHECKPOINT <nome> | ROLLBACK <nome> | UNDO | REDO
 *   forward:   FORWARD <argomenti liberi>                       (callback del chiamante, conta nel budget dei forward)
 * Budget: max_edits (edit riuscite) e max_forwards; oltre il limite il comando è rifiutato (stato 1) e registrato.
 * Trascrizione JSONL (una riga per comando): seq, cmd, status, hash e testo dell'output, ms. Il REPLAY riesegue i comandi su una
 * sessione NUOVA e verifica che ogni output coincida (gli output di FORWARD si riusano dalla trascrizione, come
 * ScriptedLLM.from_transcript: il forward del motore non è deterministico per costruzione). */
#ifndef LLMWSP_AGENT_H
#define LLMWSP_AGENT_H
#include "llmwsp_overlay.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llmwsp_session llmwsp_session;
typedef int (*llmwsp_forward_fn)(void *ctx, const char *args, char *out, size_t cap);   /* 0 = ok */
typedef struct { int max_edits, max_forwards; } llmwsp_budget;

llmwsp_session *llmwsp_session_new(const llmwsp_store *s, const llmwsp_doc *d, const char *srcdir, llmwsp_budget b,
                                   llmwsp_forward_fn fwd, void *fwd_ctx);
void            llmwsp_session_free(llmwsp_session *x);
llmwsp_overlay *llmwsp_session_overlay(llmwsp_session *x);

/* Apre/chiude la trascrizione JSONL. */
int llmwsp_session_transcript_open(llmwsp_session *x, const char *path);
int llmwsp_session_transcript_close(llmwsp_session *x);

/* Esegue un comando. Ritorna 0 = ok, 1 = rifiutato (verbo non ammesso o budget), -1 = errore di esecuzione.
 * `out` riceve sempre un testo (anche per rifiuti/errori). */
int llmwsp_session_exec(llmwsp_session *x, const char *cmd, char *out, size_t cap);

int llmwsp_session_edits_used(const llmwsp_session *x);
int llmwsp_session_forwards_used(const llmwsp_session *x);

/* REPLAY: riesegue la trascrizione su `x` (sessione nuova, stesso modello/budget). 0 = tutti gli output coincidono;
 * -1 = divergenza (indice 1-based in *mismatch, descrizione in err); -2 = trascrizione illeggibile. */
int llmwsp_replay(llmwsp_session *x, const char *transcript_path, int *mismatch, char *err, size_t ec);

/* run.json: audit della sessione (nessuno strumento esterno, verbi usati, budget, conteggi). */
int llmwsp_session_write_run(const llmwsp_session *x, const char *path);

#ifdef __cplusplus
}
#endif
#endif
