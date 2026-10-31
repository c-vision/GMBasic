/* native/gmb_runtime_c/include/gmb_rag_internal.h
 * Header interno di gmb_rag.c/gmb_rag_search.c (dominio RAG paritario col C#): funzioni
 * condivise tra il core (schema/chunking/index) e la parte search. NON è un'API pubblica —
 * le funzioni pubbliche gmb_rag_* stanno in gmb_runtime.h. */
#ifndef GMB_RAG_INTERNAL_H
#define GMB_RAG_INTERNAL_H

#include <sqlite3.h>

#define GMB_RAG_MAXDIM 4096

sqlite3* rag_db(int64_t h);
int64_t  rag_handle(const char* db);       /* db = path file o ":memory:" */
int64_t  rag_coll_id(sqlite3* db, const char* name);
int      rag_coll_dims(sqlite3* db, int64_t cid);
int      rag_coll_format(sqlite3* db, int64_t cid);  /* 1 = storage SQ8, 0 = FP32 */
/* P0e (decisione utente 2026-09-12): embedding REALE o errore esplicito, MAI fallback fixture.
 * Ritorna 0 = ok; -1 = nessun modello caricato; -2 = dimensione richiesta != dim del modello.
 * `out` è valido solo se ritorno 0. Messaggi d'errore canonici: rag_embed_err(). */
int      rag_embed(const char* text, int dims, float* out);
const char* rag_embed_err(int rc);         /* messaggio errore per il codice rag_embed */
/* KeywordReranker C# (matches/terms) — implementato in gmb_rag_search.c. */
float    rag_keyword_score(const char* query, const char* doc);
/* Fixture opt-in (2026-09-18): 1 se GMB_FIXTURE_EMBED=1 + provider embedding fixture attivo. */
int      gmb_fixture_embed_wanted(void);
int      gmb_fixture_embed(const char* text, int dims, float* out); /* 0 ok, -1 args */
void     rag_embed_query(const char* q, int dims, float* out);
float    rag_cos(const float* a, const float* b, int n);
extern int g_fts_available;   /* FTS5 attivo? (impostato in gmb_rag_open) */

#endif /* GMB_RAG_INTERNAL_H */
