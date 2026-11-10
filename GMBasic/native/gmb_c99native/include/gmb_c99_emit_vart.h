/* gmb_c99_emit_vart.h — DEFINIZIONE UNICA di `var_t` per l'emitter C99 nativo.
 *
 * PERCHÉ ESISTE: la struct era DUPLICATA (62 campi in gmb_c99_emit.c, 43 in
 * gmb_c99_emit_internal.h) e le TU `gmb_c99_emit_*.c`, che includono l'header, la vedevano
 * PICCOLA: qualunque accesso oltre i campi comuni leggeva offset sbagliati e — soprattutto —
 * `vars_t.v[i]` camminava con `sizeof(var_t)` errato (SEGV deterministico con >= 4 locali,
 * trovato dal fix di AI VERIFY). Ora c'è UNA sola definizione, inclusa da entrambi.
 *
 * REGOLA: nessun campo di `var_t` va aggiunto in un .c; si aggiunge QUI.
 */
#ifndef GMB_C99_EMIT_VART_H
#define GMB_C99_EMIT_VART_H

typedef struct { char* name; int type; int is_array; int ndims; int byref; int gkind; int elem_str; int is_bit; /* DIM AS BIT: NOT e AND/OR logici (canone -1/0) */ char elem_class[40]; int captured; int cap_ref;
                 int dyn_len;  /* (b4): lunghezza runtime compagna <ident>_len (array da builtin array-returning) */
                 int loop_new_counter; /* contatore FOR numerico creato dal loop (nessuna DIM
                                       * esterna): a NEXT resta visibile (parità IL). */
                 int has_blo;  /* canone 1-based 2026-09-30: DIM con TO ha emesso `<ident>__blo`
                                * (lower bound) — FOR EACH/LBOUND lo leggono solo se presente */
                 int matrix;   /* (b4): matrice n×m di righe double* (NUMERICAL.Fft/IFFT) */
                 int jagged;   /* (b4): DIM x(n) AS ARRAY OF <tipo> → double** (righe) */
                 int matrix_handle; /* DIM h = MATRIX.CREATE(r,c): handle int64_t (gmb_matrix_*) */
                 int matrix_value;  /* valore MATRIX OF: descrittore gmb_matrix_t (Stage C backend C) */
                 int boxed_val; /* (b4): variabile gmb_val_t (dict-valued, es. MULTIVARIATE.KMeans) */
                  int rgba_obj; /* DIM/param AS RGBA → dict boxed {Red,Green,Blue,Alpha} (gmb_val_t);
                                * membri .Red/.Green/.Blue/.Alpha numerici, COLOR usa il dict. */
                  int sb_builder; /* DIM v AS STRINGBUILDER → gmb_stringbuilder_t* (gmb_sb_*) */
                  int regex_obj; /* DIM v AS REGEX → gmb_val_t oggetto type-name "REGEXWRAPPER"
                                  * (parità IL: TYPENAME(v)=REGEXWRAPPER, non NIL/DOUBLE). */
                 int kvp_entry; /* FOR EACH e IN <dict>: `e` e' un KeyValuePair {Key,Value} boxed.
                                 * 1 = valori numerici, 2 = valori stringa (tipo di e.Value; Key e' sempre stringa). */
                 int nullable;  /* DIM v AS <scalare>?: local gmb_val_t boxed (NIL ≠ 0) */
                 int boxed_md;  /* CONVERT.* Markdown AST boxed (MdParse/doc/member): i membri
                                 * si leggono con gmb_dict_get (raw val), non json_path_text. */
                 int posted_val; /* DIM AS HTTPPOSTEDDATA (esito boxed di ParseMultipartForm):
                                 * gmb_val_t dict; i membri (FormFields/FileFields) sono LISTE —
                                 * lettura grezza gmb_dict_get, mai json_path_text/coercizione. */
                 int elem_boxed;/* LIST gkind i cui elementi sono gmb_val_t boxed (CONVERT
                                 * HEADINGS/LINKS/...): FOR EACH bind la var come gmb_val_t. */
                 int fixed_len; /* 2.8: STRING * N — lunghezza fissa (0 = non fissa) */
                 int arr_byref; /* 2.8: parametro array con REDIM nel body → ctype** + gmb_int_t* (parity _byRefArrayParams oracolo) */
                 char vclass[40]; /* 2.7: variabile istanza di classe (DIM x AS <Classe>) */
                 int func_arity;    /* GK_FUNCVAL: numero di parametri della firma (DIM v AS FUNCTION(...)) */
                 int func_ret_str;  /* GK_FUNCVAL: 1 = la firma dichiara ritorno STRING (altrimenti numerico) */
                 int fn_str_params; /* GK_FUNCVAL: bit i = 1 se il parametro i della firma è AS STRING */
                 /* CATENA di block RITORNATI dal valore funzione in questa variabile (lambda
                  * dentro lambda, profondità arbitraria): fn_ret_n = 0 → il block ritorna un
                  * valore scalare (fn_ret_str dice se stringa); fn_ret_n = k > 0 → ritorna un
                  * block, con firma fn_ret_arity[i]/fn_ret_mask[i] al livello i e valore
                  * finale fn_ret_str. Serve a tipizzare `EVAL(v, args)` / `EVAL(EVAL(…))`. */
                 int fn_ret_n;
                 int fn_ret_arity[8];
                 int fn_ret_mask[8];
                 int fn_ret_str;
                 int cell;          /* 1 = variabile catturata da una closure figlia: vive in una
                                     * CELLA heap condivisa (il corpo vi accede via imap
                                     * `(*__cell_<nome>)`, la closure riceve il puntatore).
                                     * Semantica per-riferimento + lifetime corretto per le
                                     * closure ritornate (parità VM/C/IL). */
                 char gen_id[40];   /* GK_GEN: nome del generatore → tipo gmb_gen_<id>_t */
                 char dim_expr[24]; /* nome della variabile-bound del primo DIM (__dim<id>_0): serve
                                     * a chi deve conoscere la DIMENSIONE a runtime (es. APPENDMANY) */
                 int is_bytes; /* variabile byte-array: conserva i byte grezzi (gmb_string_t binaria) */
                 int implicit_alloc; /* A = {{...}} non dichiarata: matrice 2D implicita da
                                      * allocare al sito di assegnazione (nessun DIM precedente) */
                 int cap_from_param; /* cattura di un PARAMETRO del genitore (mai un DIM locale): il campo
                                       * closure va seminato esplicitamente dal parametro C grezzo al momento
                                       * della creazione, un DIM locale catturato si semina da solo via il
                                       * remap degli identificatori sulla propria istruzione DIM. */
                 int stats_val; /* STATS test d'ipotesi (TTEST/.../DESCRIBE): dict boxed con
                                 * membri SCALARI numerici (Statistic/PValue/...) → lettura
                                 * numerica (gmb_val_to_double), non json_path_text. */
                  int ring_cap;  /* DIM v AS RINGBUFFER<...>: capacità della coda circolare
                                  * (0 = non è un ring buffer). Default 64 (parità
                                  * CircularQueue C#/P-Code). Enqueue sovrascrive il più
                                  * vecchio a buffer pieno; Capacity() ritorna questo valore. */
                  int sql_rs;    /* result set SQL (SQL QUERY alias, rs, ...): handle double;
                                  * rs("col")/rs(i) legge la riga corrente (gmb_sql_current_val). */
                 char* tuple_fields; /* CSV "x,y,z" (indice vuoto = anonimo) se boxed_val viene
                                      * da un letterale tupla NOMINATO (DIM p = (x: 1, y: 2)):
                                      * risolve p.nome -> p(indice) (resolve_tuple_field). NULL
                                      * se non è una tupla nominata. */
                  int dbf_field;    /* 1 = identificatore nudo risolto a CAMPO DBF (late-binding
                                     * Clipper): emesso via imap come gmb_db_dbf_field_val_name. */
                  int is_dim_local; /* 1 = locale dichiarato con DIM esplicito: un nome-campo di
                                    * CLASSE in forma NUDa va reso SELF.<campo> solo se NON è
                                    * un DIM locale né un parametro (che lo ombreggiano). */
                  const void* dim_stmt; /* istruzione DIM che ha registrato la variabile: il
                                    * pre-pass di registrazione può girare più di una volta sulla
                                    * stessa istruzione (idempotente) — la dichiarazione DUPLICATA
                                    * vera è solo quella di un'istruzione DIVERSA. */
                  int late_local;   /* 1 = LOCAL che ombreggia una GLOBAL: dichiarato C solo dal
                                    * punto della LOCAL in poi (lookup_var lo ignora prima). */
                 int elem_byte; /* DIM x(n) AS BYTE: array di BYTE (PUT/GET #n su file BINARY = 1 byte per elemento) */
                 int list_semantics; /* Array nativo che rappresenta una LISTA del linguaggio
                                      * (letterale `[ ... ]` non annidato senza tipo target, o
                                      * generator expression `(x FOR x IN c)`): TYPENAME deve
                                      * dire "LIST`1" come per le LIST (guida Arrays §0.3),
                                      * non "ARRAY". La rappresentazione resta l'array dinamico
                                      * nativo (indicizzazione/LEN/FOR EACH/Add già a parità). */
                 int coll_type_code; /* typeCode BASIC della collezione dichiarata (1=LIST,2=DICT,
                                      * 3=HASHSET,...,17=GRAPH,20=RINGBUFFER,...). Disambigua
                                      * TYPENAME/VALTYPE fra tipi con la stessa rappresentazione C. */
                 int coll_handle; /* 1 = la variabile è un gmb_val_t che PORTA UN HANDLE di
                                   * collezione del runtime condiviso (gmb_coll_*), non una
                                   * gmb_list_t/gmb_dict_t dell'emitter. Serve ai contenitori
                                   * TIPIZZATI con semantica non-lista (HEAP/MAXHEAP/BAG/TRIE):
                                   * heapify su Add, Peek/Extract = radice, BAG multiset. */
               } var_t;

/* `vars_t` dipende da `var_t`: una sola definizione anche di questa. */
typedef struct { var_t* v; int n, cap; } vars_t;

#endif /* GMB_C99_EMIT_VART_H */
