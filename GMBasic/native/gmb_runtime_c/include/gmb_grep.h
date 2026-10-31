/* gmb_grep.h — motore di ricerca file "grep" condiviso (sorgente UNICA).
 *
 * Design (adattato, non copiato, da un file pattern searcher moderno):
 *   - camminata ricorsiva delle directory (con profondita' e dotfiles opzionali)
 *   - selezione file per glob include/esclude e per estensione (type)
 *   - match per REGEX (via gmb_regex_* / PCRE2) o stringa fissa
 *   - opzioni: case-insensitive, whole-word, invert, line/column numbers,
 *     only-matching, count, files-with-matches, context before/after
 *   - risultato = lista gmb_val_t di dict {file,line,column,text} (strutturato,
 *     consumabile sia dalla REPL C sia da P/Invoke C# sia dagli emitter).
 *
 * Sorgente unica per VM/C, C99Native e la REPL C; esposta via P/Invoke
 * alla REPL C# (la lib condivisa si ottiene aggiungendo gmb_grep.c a un target SHARED).
 */
#ifndef GMB_GREP_H
#define GMB_GREP_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Esegue la ricerca di `opts` (dict gmb_val_t) su `path` (file o directory).
 * Chiavi di opts: pattern (string), fixed/ignore_case/word/invert/recursive/hidden/
 * line_number/column_number/only_matching/count_only/files_with_matches (bool),
 * max_depth/max_count/context_before/context_after (int), include/exclude/type
 * (string, separati da virgola). Ritorna una gmb_list_t di dict:
 *   { "file": STRING, "line": LONG, "column": LONG, "text": STRING }
 * (per files_with_matches: una entry per file con "text" vuoto e line=0). */
gmb_val_t gmb_grep_search(gmb_val_t opts, const char* path);

/* Numero di HIT totali nella lista ritornata da gmb_grep_search. */
gmb_int_t gmb_grep_count(const gmb_val_t hits);

/* Formatta i risultati come testo "file:line:column:text\n" (rispetta
 * with_line/with_column). Ritorna una gmb_string_t. */
gmb_string_t gmb_grep_format(const gmb_val_t hits, int with_line, int with_column);

/* Helper monoshot: compila una regex usando il runtime condiviso (per i test). */
int gmb_grep_selftest(void);

/* Variante JSON (ABI stabile per P/Invoke C# e REPL C): opts_json = oggetto JSON
 * delle opzioni; ritorna un JSON array di hit (malloc → gmb_grep_free). */
char* gmb_grep_search_json(const char* opts_json, const char* path);
char* gmb_grep_format_json(const char* hits_json, int with_line, int with_column);
void  gmb_grep_free(char* p);

/* ── Superficie di linguaggio GREP.* (default: ricorsivo, con numeri di riga/colonna) ──
 * MATCHES: lista di stringhe "file:line:col:text". FILES: nomi file distinti.
 * COUNT: numero di hit. Le varianti _json ritornano un array JSON di stringhe (malloc,
 * gmb_grep_free) per il P/Invoke C#. */
gmb_list_t gmb_grep_matches(const char* pattern, const char* path);
gmb_int_t  gmb_grep_count_path(const char* pattern, const char* path);
gmb_list_t gmb_grep_files(const char* pattern, const char* path);
char* gmb_grep_matches_json(const char* pattern, const char* path);
char* gmb_grep_files_json(const char* pattern, const char* path);
/* Varianti STRINGA (per la superficie di linguaggio sui backend e per l'uso headless):
 * MATCHES → "file:line:col:text" una riga per hit; FILES → nomi file distinti, uno per riga. */
gmb_string_t gmb_grep_matches_str(const char* pattern, const char* path);
gmb_string_t gmb_grep_files_str(const char* pattern, const char* path);
/* Varianti ARRAY (gmb_string_t* + conteggio in *out_n; free con free()): superficie di
 * linguaggio GREP.MATCHES/FILES. */
gmb_string_t* gmb_grep_matches_arr(const char* pattern, const char* path, gmb_int_t* out_n);
gmb_string_t* gmb_grep_files_arr(const char* pattern, const char* path, gmb_int_t* out_n);
/* Testo (char*, malloc → gmb_grep_free) per P/Invoke diretto (ABI stabile). */
char* gmb_grep_matches_text(const char* pattern, const char* path);
char* gmb_grep_files_text(const char* pattern, const char* path);

/* Hook OPZIONALE per la ricerca in PDF: il consumer che linka gmb_doc registra una funzione
 * che estrae il testo di un PDF (malloc, NULL se non disponibile). Senza registrazione la
 * ricerca in PDF è saltata. Nessuna dipendenza a link-time (§ hook opzionali AGENTS). */
typedef char* (*gmb_grep_pdf_extractor)(const char* path);
void gmb_grep_set_pdf_extractor(gmb_grep_pdf_extractor fn);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GREP_H */
