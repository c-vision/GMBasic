/* gmb_llm_tok.h — dominio LLM/TOKENIZER del runtime condiviso (VM/C + C99Native).
 *
 * Forme con RITORNO come dato (il comando senza INTO stampa: comportamento legacy invariato).
 * Sorgente unica: queste funzioni delegano a llmmodelio_* (gmbllm) — nessun tokenizer duplicato.
 * Le stringhe tornate sono allocate con malloc: il chiamante (builtin VM/C o codice emesso dal
 * C99Native) ne prende possesso; NULL = errore esplicito (mai stringa vuota silenziosa). */
#ifndef GMB_LLM_TOK_H
#define GMB_LLM_TOK_H

#ifdef __cplusplus
extern "C" {
#endif

/* LLM.TOKENS(model, testo) -> "74,71,78,…" (gli id come dato, non stampati). */
char* gmb_llm_tokens(const char* model, const char* text);

/* LLM.DETOKENIZE(model, "74,71,…") -> testo. */
char* gmb_llm_detokenize(const char* model, const char* ids_csv);

/* LLM.TENSOROUT(sub, a, b, c) -> output del sotto-verbo `LLM TENSOR <sub>` (catturato). */
char* gmb_llm_tensor_out(const char* sub, const char* a, const char* b, const char* c);

/* Famiglie di verbi LLM con output catturabile (stessi ordini del dispatch gmbllm). */
#define GMB_LLM_FAM_TENSOR 0
#define GMB_LLM_FAM_POLICY 1
#define GMB_LLM_FAM_ARCH   2
#define GMB_LLM_FAM_MODULE 3
#define GMB_LLM_FAM_KERNEL 4
#define GMB_LLM_FAM_KV     5

/* Output di qualunque sotto-verbo della famiglia, catturato (nessuna stampa). NULL = errore. */
char* gmb_llm_cmd_out(int family, const char* sub, const char* a, const char* b, const char* c);

/* Se `r` e' NULL alza un errore BASIC catturabile (ON ERROR) con `what`; altrimenti ritorna `r`.
 * Usato dal codice emesso dal C99Native: mai stringa vuota al posto di un errore. */
char* gmb_llm_out_or_raise(char* r, const char* what, long line);

#ifdef __cplusplus
}
#endif
#endif /* GMB_LLM_TOK_H */
