/* native/gmb_runtime_c/include/gmb_ast.h — AST del linguaggio in C.
 * Specchia il nucleo di common/Ast/ (Fase C del C-Compiler-Autonomo-Plan).
 * Nodi statement/espressione con allocazione arena semplice (free ricorsivo).
 */
#ifndef GMB_AST_H
#define GMB_AST_H

#include <stdbool.h>
#include <stdint.h>
#include "gmb_tokenkind.h"

typedef struct gmb_expr gmb_expr_t;
typedef struct gmb_stmt gmb_stmt_t;
typedef struct gmb_go_case gmb_go_case_t;

/* ── Espressioni ── */
typedef enum {
    GMB_EXPR_NUM, GMB_EXPR_STR, GMB_EXPR_VAR, GMB_EXPR_BINARY, GMB_EXPR_UNARY, GMB_EXPR_CALL,
    GMB_EXPR_LAMBDA, GMB_EXPR_COMPREHENSION
} gmb_expr_kind_t;

struct gmb_expr {
    gmb_expr_kind_t kind;
    int line;
    double num;
    char* str;
    char* var;                 /* nome variabile */
    char* ns;                  /* namespace della call qualificata (NS.FUNZIONE), NULL se
                                * non qualificata. Il nome canonico resta nudo in var; ns
                                * serve solo a DISAMBIGUARE gli overload (es. GET: POOL/TORCH).
                                * La qualifica non è mai il dispatch primario (regola d'oro). */
    gmb_tokenkind_t ns_tk;     /* chiave canonica del namespace (risolta alla creazione AST:
                                * mai from_name a valle quando disponibile). */
    gmb_tokenkind_t pf_tk;     /* token del PREFISSO quando un membro e' fuso su un risultato
                                * di CALL (call.membro -> 'nome.membro'): es. JSON.Parse.campo
                                * porta pf_tk=Parse. 0 se il nome non deriva da una call. */
    const char* op;            /* operatore binario/unario (lexeme) */
    gmb_expr_t* left;          /* binario sx / unario operando */
    gmb_expr_t* right;         /* binario dx */
    gmb_expr_t** args;         /* argomenti chiamata */
    int nargs;
    /* Invocazione di un VALORE callable (lambda letterale): `{ |x| … }(41)`.
     * Se non NULL è l'espressione del callee; `var` resta NULL. */
    gmb_expr_t* callee;
    gmb_tokenkind_t tk;        /* TokenKind di dispatch (visione 6.6): il builtin/funzione
                                * disambiguato, quando il parser lo risolve per nome.
                                * GMB_TOK_Unknown = non risolto (fallback al nome). */
    char** lambda_params;      /* FN/SUB anonima: parametri nello scope della closure */
    int n_lambda_params;
    uint8_t* lambda_params_str;/* n_lambda_params: 1 = parametro AS STRING */
    uint8_t* lambda_params_arr;/* n_lambda_params: 1 = parametro array (FN(v() AS DOUBLE)) */
    char* lambda_param_cls;    /* inferito (AEVAL/… su array UDT): il parametro 0 è un'istanza
                                * della classe/tipo con questo nome (gmb_val_t) */
    char* lambda_param_cls2;   /* inferito (ASORT comparatore su array UDT): parametro 1 (stesso
                                * tipo dell'elemento). NULL se non applicabile. */
    int lambda_ret_str;        /* FN(...) AS STRING: ritorno stringa */
    int lambda_ret_arr;        /* FN(...) AS ARRAY OF ...: ritorno array (void + out) */
    int lambda_ret_arr_len;    /* numero di residui/equazioni del letterale di ritorno (m) */
    gmb_expr_t* lambda_expr;   /* FN(...) => expr */
    gmb_stmt_t** lambda_body;  /* SUB(...) { ... } / FN a blocco */
    int n_lambda_body;
    int lambda_is_sub;
    /* comprehension `FROM <var> IN <source> [WHERE <where>] [ORDER BY <order> [DESC]] SELECT <sel>`
     * (specchio LinqQueryExpr del C#): itera <source>, filtra <where>, ordina per <order>,
     * proietta <sel> in una LISTA risultato. I campi sono riusati su questo kind. */
    gmb_expr_t* comp_src;
    gmb_expr_t* comp_where;
    gmb_expr_t* comp_order;
    gmb_expr_t* comp_sel;
    char* comp_var;      /* variabile di iterazione */
    int comp_desc;       /* ORDER BY DESC */
    /* comprehension `[ … FOR … ]`/`{ … FOR … }` (lista/dict/set): oltre alla prima clausola
     * (comp_src/comp_var) altre clausole FOR nidificate; IF finale in comp_where; comp_key
     * per dict; comp_ctype 0=list 1=dict 2=set. */
    gmb_expr_t** comp_more_src;   /* sorgenti delle clausole 1..n-1 */
    char** comp_more_var;         /* variabili delle clausole 1..n-1 */
    int n_comp_more;
    gmb_expr_t* comp_key;         /* dict comprehension: espressione chiave */
    int comp_ctype;               /* 0 = lista (o LINQ), 1 = dict, 2 = set */
    int is_list_literal;          /* letterale `[ e1, .., en ]` (QUADRE) invece di `{ ... }`
                                   * (GRAFFE): la guida (Arrays §0.2/§0.3) distingue le due
                                   * forme — fuori da un DIM di array, `[ ]` è un letterale di
                                   * LISTA. Il parser lo marca qui; l'emitter C99 ne deduce la
                                   * rappresentazione (gmb_list_t) quando il target non è un
                                   * array. 0 = graffe/dict/comprehension. */
    /* REWRITE (linker C, pari a Linker.RewriteTarget/nExpression): arricchimento risolto. */
    int  resolved_sym;            /* id del simbolo risolto (-1 = non risolto) */
    char* resolved_type;          /* tipo risolto ("" / NULL = ignoto) */
    /* Accesso SICURO `expr?` (coll[key]? / arr(i)?): su chiave/indice mancante il valore è
     * NIL invece di un errore. Il parser marca l'espressione; l'emitter C99 stampa vuoto
     * per il NaN-sentinella e `??` sceglie il fallback. */
    int is_safe;
};

gmb_expr_t* gmb_expr_num(double v, int line);
gmb_expr_t* gmb_expr_str(const char* s, int line);
gmb_expr_t* gmb_expr_var(const char* name, int line);
gmb_expr_t* gmb_expr_clone(const gmb_expr_t* e);
gmb_expr_t* gmb_expr_binary(const char* op, gmb_expr_t* l, gmb_expr_t* r, int line);
gmb_expr_t* gmb_expr_unary(const char* op, gmb_expr_t* e, int line);
gmb_expr_t* gmb_expr_call(const char* name, gmb_expr_t** args, int n, int line);
gmb_expr_t* gmb_expr_call_typed(const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int n, int line);
gmb_expr_t* gmb_expr_call_ns(const char* name, const char* ns, gmb_expr_t** args, int n, int line);
gmb_expr_t* gmb_expr_lambda(char** params, int nparams, gmb_expr_t* expr,
                            gmb_stmt_t** body, int nbody, int is_sub, int line);
gmb_expr_t* gmb_expr_comp(gmb_expr_t* src, const char* var, gmb_expr_t* where,
                          gmb_expr_t* order, int desc, gmb_expr_t* sel, int line);

/* ── Statement ── */
typedef enum {
    GMB_STMT_PRINT, GMB_STMT_LET, GMB_STMT_ASSIGN, GMB_STMT_IF, GMB_STMT_FOR,
    GMB_STMT_WHILE, GMB_STMT_DO, GMB_STMT_FUNCTION, GMB_STMT_END_FUNCTION,
    GMB_STMT_DIM, GMB_STMT_RETURN, GMB_STMT_EXPR, GMB_STMT_GOTO, GMB_STMT_GOSUB,
    GMB_STMT_NEXT, GMB_STMT_END, GMB_STMT_WEND, GMB_STMT_LOOP, GMB_STMT_EXIT,
    GMB_STMT_CONTINUE,
    GMB_STMT_CALL, GMB_STMT_REM, GMB_STMT_SELECT, GMB_STMT_CASE, GMB_STMT_LABEL,
    GMB_STMT_ON_ERROR, GMB_STMT_ON_KEY, GMB_STMT_ON_MOUSE, GMB_STMT_ON_WINDOW,
    GMB_STMT_TRY, GMB_STMT_CATCH, GMB_STMT_FINALLY, GMB_STMT_ERROR, GMB_STMT_THROW,
    GMB_STMT_DATA, GMB_STMT_READ, GMB_STMT_RESTORE,
    GMB_STMT_REDIM, GMB_STMT_SWAP, GMB_STMT_ERASE, GMB_STMT_STOP, GMB_STMT_RUN, GMB_STMT_REQUIRE, GMB_STMT_YIELD,
    GMB_STMT_AADD, GMB_STMT_AINS, GMB_STMT_ADEL, GMB_STMT_ASHUFFLE, GMB_STMT_ASORT,
    GMB_STMT_AFILL, GMB_STMT_ACOPY,
    GMB_STMT_CLS, GMB_STMT_LOCATE, GMB_STMT_COLOR, GMB_STMT_INKEY, GMB_STMT_INPUT, GMB_STMT_SET, GMB_STMT_SHELL,
    GMB_STMT_WAITKEY, GMB_STMT_GETKEY, GMB_STMT_CAPTION,
    GMB_STMT_SAVESCREEN, GMB_STMT_RESTORESCREEN,
    GMB_STMT_ATFORM, GMB_STMT_SCROLLMODE,
    GMB_STMT_OPEN, GMB_STMT_CLOSE, GMB_STMT_GET, GMB_STMT_PUT, GMB_STMT_LOCK, GMB_STMT_PRINTFILE, GMB_STMT_INPUTFILE, GMB_STMT_LINEINPUT,
    GMB_STMT_SEEK,   /* SEEK n, pos: comando legacy sui numeri di file (pos 1-based) */
    GMB_STMT_CONTROL, GMB_STMT_SEND, GMB_STMT_RECEIVE, GMB_STMT_CHANNEL, GMB_STMT_MUTEX,
    GMB_STMT_GO, GMB_STMT_GOEXIT, GMB_STMT_GOSELECT, GMB_STMT_TIMEOUT, GMB_STMT_WAIT,
    GMB_STMT_DONE, GMB_STMT_SLEEP, GMB_STMT_DEFER, GMB_STMT_COND, GMB_STMT_ONCE,
    GMB_STMT_RAISE_EVENT, GMB_STMT_ADD_HANDLER, GMB_STMT_REMOVE_HANDLER,
    GMB_STMT_WINDOW_THEME, GMB_STMT_WINDOW_DESKTOP, GMB_STMT_WINDOW_FRAME, GMB_STMT_WINDOW_SHADOW,
    GMB_STMT_WINDOW_SELECT, GMB_STMT_WINDOW_DEFINE, GMB_STMT_WINDOW_SETTITLE, GMB_STMT_WINDOW_APPEND,
    GMB_STMT_WINDOW_CONTENT, GMB_STMT_WINDOW_CLEAR, GMB_STMT_WINDOW_SCROLL, GMB_STMT_WINDOW_EDIT,
    GMB_STMT_WINDOW_MOVEDESKTOP, GMB_STMT_WINDOW_ACTION, GMB_STMT_WINDOW_PUMP, GMB_STMT_WINDOW_CLOSE,
    GMB_STMT_MOUSE_CONFIG, GMB_STMT_MOUSE_VISIBILITY, GMB_STMT_MOUSE_SET, GMB_STMT_MOUSE_LIMIT,
    GMB_STMT_MOUSE_CURSOR, GMB_STMT_MOUSE_WAIT,
    GMB_STMT_ZIP, GMB_STMT_UNZIP,
    GMB_STMT_JSON_SAVE, GMB_STMT_JSON_WRITEVALUE, GMB_STMT_JSON_WRITESECTION, GMB_STMT_JSON_OPEN,
    GMB_STMT_MCP, GMB_STMT_TEAM_CREATE, GMB_STMT_TEAM_SAVE, GMB_STMT_TEAM_LOAD,
    GMB_STMT_TEAM_ADDAGENT, GMB_STMT_TEAM_REMOVEAGENT, GMB_STMT_TEAM_RUN,
    GMB_STMT_TEAM_SETROLE, GMB_STMT_TEAM_SETSTRATEGY, GMB_STMT_TEAM_SETCOORDINATOR, GMB_STMT_TEAM_MESSAGE,
    GMB_STMT_TEAM_SETRECURSION, GMB_STMT_TEAM_SETLATENT, GMB_STMT_TEAM_SETLINK,
    GMB_STMT_TEAM_SETTHOUGHTS, GMB_STMT_TEAM_TRAININNER, GMB_STMT_TEAM_TRAINOUTER,
    GMB_STMT_TEAM_SETDAG, GMB_STMT_TEAM_SETCOMPLEXITY, GMB_STMT_TEAM_SETREFLECTION,
    GMB_STMT_TEAM_SETCRITIC, GMB_STMT_TEAM_SETPRUNE, GMB_STMT_TEAM_TRAINTHINKING,
    GMB_STMT_TEAM_TRAINACTORCRITIC, GMB_STMT_TEAM_DISTILLTHINKING,
    GMB_STMT_TEAM_BROADCAST, GMB_STMT_TEAM_REDEFINE, /* L1 broadcast/redefine (append-only: mai rinumerare) */
    GMB_STMT_AGENT_CREATE, GMB_STMT_AGENT_DELETE, GMB_STMT_AGENT_SET, GMB_STMT_AGENT_SAVE, GMB_STMT_AGENT_LOAD,
    GMB_STMT_AGENT_SEND, GMB_STMT_AGENT_RECEIVE, GMB_STMT_AGENT_PAUSE, GMB_STMT_AGENT_RESUME, GMB_STMT_AGENT_STOP,
    GMB_STMT_AGENT_REGISTER_TOOL, GMB_STMT_AGENT_TRACE, GMB_STMT_AGENT_CODE,
    GMB_STMT_AGENT_SERVE, GMB_STMT_AGENT_STOP_SERVER, GMB_STMT_AGENT_ASKSTREAM,
    GMB_STMT_DOMAIN,
    GMB_STMT_NET_CMD,  /* OP_NET_CMD: HTTP SERVER/ROUTE ecc. (sub + argomenti + eventuale code-ref) */
    GMB_STMT_GFX,      /* 2.6(r): grafica (SCREEN/SYNC/POINT/LOAD TEXTURE) — riusa net_sub come
                        * opcode grafico (1=SCREEN, 2=SYNC, 3=POINT, 4=LOAD TEXTURE, 5=LOAD TEXTURE
                        * CELL), net_args/n_netargs come argomenti e net_assign come target INTO/, */
    /* filesystem (FASE 2.2): value = path; RMDIR: cond = flag ricorsivo;
     * RENAMEFILE/COPYFILE: value = sorgente, cond = destinazione;
     * COPYFILE flag=1 = XCOPY (copia ricorsiva directory) */
    GMB_STMT_KILL, GMB_STMT_MKDIR, GMB_STMT_CHDIR, GMB_STMT_RMDIR,
    GMB_STMT_RENAMEFILE, GMB_STMT_COPYFILE,
    GMB_STMT_CLASS,   /* 2.7: CLASS [INHERITS] — campi (cls_field_*) + metodi (cls_methods) */
    GMB_STMT_DELEGATE, /* 2026-09-06: DELEGATE FUNCTION <Nome>(sig) AS <ret> — tipo funzione
                        * nominato (func_name=nome, func_val_arity/str_mask, func_ret_type) */
    GMB_STMT_USING,   /* 2026-09-03: USING risorsa = expr ... END USING — target=risorsa,
                       * value=init, try_body/n_try=corpo (chiama il DESTRUCTOR a END USING) */
    GMB_STMT_AISERVER_START, GMB_STMT_AISERVER_STOP, GMB_STMT_AISERVER_STATUS,
    GMB_STMT_AISERVER_MODEL, GMB_STMT_AISERVER_UNLOAD,
    GMB_STMT_RANDOMIZE,
    GMB_STMT_CONST,
    /* M14 embedded: AI MODEL LOAD/UNLOAD/CACHE/PREFETCH/DEVICE (MoE out-of-core). */
    GMB_STMT_AIMODEL_LOAD, GMB_STMT_AIMODEL_UNLOAD, GMB_STMT_AIMODEL_CACHE,
    GMB_STMT_AIMODEL_PREFETCH, GMB_STMT_AIMODEL_DEVICE, GMB_STMT_AIMODEL_CONVERT,
    GMB_STMT_AIMODEL_PROMOTE, GMB_STMT_AIMODEL_DEMOTE,
    GMB_STMT_AIMODEL_QUANTIZE,
    GMB_STMT_AIMODEL_INSPECT,
    GMB_STMT_SENDKEY,
    /* MEMORYMAP comandi (2026-09-15): MMAPPOKE handle, offset, value;
     * MMAPWRITEBYTES handle, offset, srcArray() [, count] (append-only). */
    GMB_STMT_MMAP_POKE, GMB_STMT_MMAP_WRITEBYTES,
    /* filesystem IO (2026-09-21, parità 4 backend gruppo IO): MOVE src TO dst
     * (value = sorgente, cond = destinazione); WATCH path, mask, SUB
     * (value = path, cond = mask, net_coderef = nome SUB callback);
     * UNWATCH path (value = path). Append-only in coda (mai riordinare). */
    GMB_STMT_MOVEFILE, GMB_STMT_IOWATCH, GMB_STMT_IOUNWATCH,
    GMB_STMT_AIMODEL_SAVE,  /* LLM MODEL SAVE (model compiler) — in coda */
    GMB_STMT_AIMODEL_ABLATE, /* LLM MODEL ABLATE (model surgery MODIFICARE) — in coda */
    GMB_STMT_AIMODEL_REMOVE, /* LLM MODEL REMOVE (model surgery: elimina layer) — in coda */
    GMB_STMT_AIMODEL_DIFF,   /* LLM MODEL DIFF (checkpoint/rollback) — in coda */
    GMB_STMT_AIMODEL_CHECKPOINT, /* LLM MODEL CHECKPOINT (checkpoint-store) — in coda */
    GMB_STMT_AIMODEL_ROLLBACK,   /* LLM MODEL ROLLBACK (checkpoint-store) — in coda */
    GMB_STMT_AIMODEL_ADD,        /* LLM MODEL ADD (AGGIUNGERE: merge moduli) — in coda */
    GMB_STMT_AIMODEL_ADAPTER,    /* LLM MODEL ADAPTER (adapter LoRA .safetensors) — in coda */
    GMB_STMT_AIMODEL_MODULE,     /* LLM MODEL MODULE (modulo dichiarato .gmmod) — in coda */
    GMB_STMT_AIMODEL_MODALITY,   /* LLM MODEL MODALITY (emit .gmmod) — in coda */
    GMB_STMT_AIMODEL_VALIDATE,   /* LLM MODEL VALIDATE — in coda */
    GMB_STMT_AIMODEL_TRACE,      /* LLM MODEL TRACE (K1: trace esperti .moetrace) — in coda */
    /* console (2026-09-21, gruppo Console): CLIP GET/SET/CLEAR — flag 0=GET (target),
     * 1=SET (value), 2=CLEAR. Append-only in coda. */
    GMB_STMT_CLIP,
    /* console (2026-09-21, gruppo Console): CURSOR ON/OFF (value = 1/0 numerico) |
     * CURSOR SHAPE stile$ (cond = espressione stringa). Append-only in coda. */
    GMB_STMT_CURSOR,
    /* console-forzate C* (2026-09-21, gruppo Console, parity PCode): CCLS [colore]
     * (value = colore o NULL), CLOCATE riga, colonna (value/cond), CPALETTE
     * idx, r, g, b (value/cond/file_pos/cond_mutex). Append-only in coda. */
    GMB_STMT_CCLS, GMB_STMT_CLOCATE, GMB_STMT_CPALETTE,
    GMB_STMT_IMPORT,  /* IMPORT <ns>[(members)] [AS alias] — direttiva di namespace (linker/grafo) */
    GMB_STMT_AIPATCH, /* §4.10 write-side: AI PATCH "target","rev" ... END PATCH (corpo in then_body) */
    GMB_STMT_MESSAGE,     /* MESSAGE <verbo> ... (namespace MESSAGE): verbo in mutex_action,
                           * argomenti in net_args; target CONNECT/AS in target_label */
    GMB_STMT_ON_MESSAGE,  /* ON MESSAGE <sub> CALL|GOSUB|STOP <handler>: sub in mutex_action,
                           * handler in target_label, azione in flag (0=call,1=gosub,2=stop) */
    GMB_STMT_ON_CONTROL_EVENT, /* ON <EVENTO> CONTROL <id> <azione>: evento in ce_event, id in on_expr,
                                * target in target_label, azione in flag (0=call,1=gosub,2=stop,3=goto) */
    GMB_STMT_SIGNAL,      /* SIGNAL <verbo> ... (namespace SIGNAL): verbo in mutex_action, argomenti in
                           * net_args. SUBSCRIBE: [types,asset,market,symbols,timeframe,id]; EMIT:
                           * [type,symbol,value,asset,market,timeframe]; UNSUBSCRIBE: [id]; PING: - */
    GMB_STMT_ON_SIGNAL,   /* ON SIGNAL <id> CALL <handler>: id in net_args[0], handler in target_label,
                           * arità dell'handler (4/5) in flag */
} gmb_stmt_kind_t;
typedef struct gmb_window { const char* name; const char* action; const char* style; int flag; } gmb_window_t;

struct gmb_stmt {
    gmb_stmt_kind_t kind;
    int scope_id;   /* REWRITE/link: scope lessicale di dichiarazione (settato dalla symtab, -1 se ignoto) */
    int line;
    int bas_line;   /* 2.8: numero riga BASIC (token LineNumber in testa), 0 se assente */
    int for_next_bas; /* 2.8: numero riga BASIC del NEXT di un FOR (label lbl_N) */
    /* PRINT */
    gmb_expr_t** print_args; int nprint;
    int print_hold;   /* ';' o ',' finale: niente newline (continuazione riga, parity PCodeVM) */
    /* separatore DOPO print_args[i]: 1 se ',' (tab zone 14 colonne, parity oracolo
     * IlRuntime.PrintTabZone/OP_PRINT_TAB_ZONE), 0 se ';' o ultimo argomento (nessun
     * padding). Array parallelo a print_args, stessa dimensione (nprint). */
    int* print_comma;
    /* LET/ASSIGN */
    int is_let;                /* 1 = forma esplicita `LET x = …` (S_eval: dichiara un
                                * locale transiente; l'assegnazione implicita invece
                                * scrive la variabile già esistente del chiamante) */
    char* target; gmb_expr_t* value;
    gmb_expr_t* array_index;   /* ASSIGN a elemento array: v(i) = x */
    gmb_expr_t** array_idx_extra; int n_array_idx_extra;  /* indici dalla 2ª dimensione: v(i, j, ...) = x */
    /* IF/THEN/ELSE */
    gmb_expr_t* cond;
    gmb_stmt_t** then_body; int n_then;
    char* patch_target;   /* AI PATCH: nome procedura target */
    char* patch_rev;      /* AI PATCH: revisione attesa */
    gmb_stmt_t** else_body; int n_else;
    /* FOR/TO/STEP/NEXT */
    char* for_var; gmb_expr_t* for_from; gmb_expr_t* for_to; gmb_expr_t* for_step;
    gmb_stmt_t** for_body; int n_for;
    /* WHILE/WEND */
    gmb_stmt_t** while_body; int n_while;
    /* DO/LOOP/UNTIL */
    gmb_expr_t* do_until; gmb_expr_t* do_while; gmb_stmt_t** do_body; int n_do;
    /* FUNCTION */
    char* func_name; char** func_params; int n_params;
    uint8_t* param_byref;   /* n_params flag: 1 = parametro BYREF */
    uint8_t* param_is_array;/* n_params flag: 1 = parametro array (nome()) */
    uint8_t* param_ndims;   /* n_params: numero dimensioni array param (0/1 = 1D, 2 = 2D...) */
    uint8_t* param_mode;    /* n_params: 0 = IN (default), 1 = OUT, 2 = INOUT (parametri matrice) */
    char** param_types;     /* n_params: nome tipo AS (CHANNEL/WAITGROUP/...) o NULL */    int* param_fixed_lens;  /* n_params: lunghezza STRING * N del parametro (0 = non fissa) */
    uint8_t* param_variadic;/* n_params flag: 1 = PARAMARRAY (raccoglie gli argomenti eccedenti) */
    uint8_t* param_optional;/* n_params flag: 1 = parametro OPTIONAL (default = tipo se manca '=') */
    gmb_expr_t** param_defaults; /* n_params: espressione di default (OPTIONAL ... = espr), NULL se assente/obbligatorio */
    int* param_fn_arity;    /* n_params: parametro `AS FUNCTION(N...)...`: arity della firma (-1 se non function-typed) */
    int* param_fn_str_mask; /* n_params: parametro FUNCTION: bit i = 1 se il parametro i della FIRMA è AS STRING */
    uint8_t* param_fn_ret_str; /* n_params: parametro FUNCTION: 1 = ritorno STRING (altrimenti numerico) */
    /* Callback FFI (2026-09-29): bit i = 1 se il parametro i della FIRMA interna del callback
     * è INTERO (LONG/INTEGER/QUAD/BYTE/...) invece di DOUBLE/SINGLE — serve SOLO al
     * marshalling FFI tipizzato (emit_ffi_wrapper), che deve generare un trampolino con la
     * firma nativa ESATTA gmb_int_t/gmb_double_t per campo (il contratto generico gmb_fn_t
     * usato per i delegate BASIC-a-BASIC passa invece sempre gmb_double_t uniformemente, non
     * basta per una libreria C esterna che si aspetta un long reale in un registro intero).
     * Vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §3bis. */
    int* param_fn_kind_mask;
    /* Callback FFI (2026-11-02): bit i = 1 se il parametro i della FIRMA interna del callback
     * e' un ARRAY (`v() AS DOUBLE`) — forma "vettoriale" della callback FFI. Il nativo
     * riceve (double*, int64_t) e il trampolino ricostruisce l'array BASIC. */
    int* param_fn_arr_mask;
    /* Callback FFI (2026-09-29): 1 se il RITORNO della firma interna del callback è INTERO
     * (stesso motivo di param_fn_kind_mask sopra) — irrilevante se param_fn_has_ret[i]==0
     * (callback SUB, nessun ritorno). */
    uint8_t* param_fn_ret_int;
    /* Callback FFI (2026-11-02): 1 se il RITORNO della firma interna del callback e' un ARRAY
     * (`f(x()) AS ARRAY`) — il nativo lo riceve come parametro `double* out` finale. */
    uint8_t* param_fn_ret_arr;
    /* Callback FFI (2026-09-29): 1 se il parametro FUNCTION aveva una clausola `AS <ritorno>`
     * esplicita (sintassi `AS FUNCTION(...) AS T`, sempre presente per una FUNCTION — mai per
     * una SUB) — l'UNICO modo per distinguere "callback SUB" (nessun ritorno) da "callback
     * FUNCTION AS DOUBLE" (param_fn_ret_str/param_fn_ret_int entrambi 0 in entrambi i casi,
     * altrimenti ambigui). */
    uint8_t* param_fn_has_ret;
    char* func_ret_type;    /* clausola AS di ritorno (STRING/LONG/...) o NULL (numero) */
    int func_ret_ndims;     /* rank esplicito del ritorno: T(,) = 2, T(,,) = 3 (0/1 = 1D) */
    int dim_ndims;          /* DIM v AS MATRIX OF (assi..., T): rank esplicito (0 = non matrice) */
    char* func_ret_tuple_fields; /* AS (nome AS T, ...): CSV dei nomi campo ("lo,hi"; indice vuoto
                                  * = anonimo) o NULL. Permette a DIM x = Fn() di risolvere
                                  * x.nome -> x(indice) nel chiamante (resolve_tuple_field). */
    int func_ret_fixed_len; /* ritorno STRING * N: lunghezza (0 = non fissa) */
    int is_generator;      /* GENERATOR Nome(...) AS <ret> ... END GENERATOR (YIELD nel body) */
    /* FFI (DECLARE ... LIB): wrapper dlopen/dlsym emesso dall'emitter invece del corpo.
     * is_ffi=1 → func_name/func_params/func_ret_type hanno la firma dichiarata. */
    int is_ffi;             /* 1 = statement DECLARE ... LIB ... */
    int ffi_native;         /* 1 = DECLARE NATIVE: ABI esatta, nessuna estensione automatica degli array */
    int is_public;          /* dichiarazione con prefisso PUBLIC (export .gmblib) */
    int is_sub;             /* SUB/FUN (ritorno void) invece di FUNCTION (parità ABI .gmblib) */
    char* ffi_lib;          /* nome libreria (senza virgolette) */
    char* ffi_symbol;       /* simbolo nativo (ALIAS) o NULL (= func_name) */
    char* func_ns;          /* 2026-09-24: namespace della direttiva NAMESPACE attiva alla
                             * definizione (strdup, NULL = globale). Serve al check di
                             * shadowing dei builtin (solo dentro namespace builtin). */
    gmb_stmt_t** func_body; int n_funcbody;
    /* DIM */
    char* dim_var;
    gmb_expr_t* array_size;    /* DIM v(N): dimensione (1D) */
    gmb_expr_t* dim_to_hi;     /* DIM v(lo TO hi): bound superiore (array_size resta lo);
                                * NULL = forma plain. Canone 1-based 2026-09-30. */
    int dim_empty_parens;      /* DIM v(): array dinamico vuoto (0 elementi, cresce con AADD) —
                                * distinto da "nessuna parentesi affatto" (v resta scalare) e
                                * da "DIM v(N)" (array_size valorizzato, N+1 slot già presenti) */
    gmb_expr_t** dim_extra; int n_dim_extra;  /* DIM v(N, M, ...): dimensioni dalla 2ª in poi */
    gmb_expr_t** dim_extra_lo;                /* DIM v(lo TO hi, lo2 TO hi2, ...): lower bound per
                                               * le dimensioni dalla 2ª (NULL = 0). dim_extra[k] è
                                               * sempre il bound SUPERIORE. */
    gmb_expr_t* dim_init;      /* DIM v [AS tipo] = expr: inizializzatore */
    int dim_is_dynamic;        /* DYNAMIC v [AS STRING] [= expr] (scope dinamico, spec
                                * Variables-Scope §2.5): reuse GMB_STMT_DIM — la var NON è un
                                * locale/globale C, vive nel runtime gmb_dscope per NOME
                                * (parità emitter C#). Vedi gmb_c99_emit_dynamic.c. */
    int dim_is_global;         /* `GLOBAL v ...` (anche dentro SUB/FUNCTION): registro a scope
                                * modulo, condiviso da tutte le procedure. */
    int dim_is_local;          /* `LOCAL v ...`: lexical block-scope; se ombreggia una GLOBAL,
                                * la dichiarazione locale ha effetto solo dal punto in poi. */
    char* dim_type;            /* DIM v AS <tipo>: nome tipo (CHANNEL/WAITGROUP/MUTEX/...) */
    char* dim_type_args;       /* DIM v AS Base<A1,A2>: CSV degli argomenti generici ("A1,A2");
                                * NULL se il tipo non è istanziato (parità C# `Pair_INTEGER_STRING_`). */
    int dim_type_tk;            /* DIM v AS <tipo>: TokenKind del tipo (0 = classe/alias) */
    int dim_nullable;           /* DIM v AS <scalare>?: nullable (var = gmb_val_t boxed; NIL ≠ 0) */
    int dim_res_tk;             /* DIM AS <Analisi>Result: token della BASE (KMeans/Pca/...),
                                 * risolto dal parser (0 = non analysis-result) */
    char* dim_elem_class;      /* collections: DIM v AS LIST OF <Classe> / LIST<Classe>: elem UDT */
    int dim_chan_cap;          /* DIM v AS CHANNEL(N): capacita' (-1 = default) */
    int dim_elem_str;          /* DIM v AS CHANNEL OF STRING: 1 = elemento stringa */
    int dim_elem_any;          /* DIM v AS DICT<...,ANY>: valore DINAMICO (gmb_val_t, non double) */
    int dim_fixed_len;         /* DIM v AS STRING * N: lunghezza fissa (0 = non fissa) */
    int func_val_arity;        /* DIM v AS FUNCTION(...) AS <ret>: numero di parametri della firma */
    int func_val_str_mask;     /* DIM v AS FUNCTION(...): bit i = 1 se il parametro i della firma è AS STRING */
    int func_val_ret_is_str;   /* DIM v AS FUNCTION(...) AS STRING: 1 = ritorno stringa (altrimenti numerico) */
    int func_val_ret_is_arr;   /* DIM v AS FUNCTION(...) AS ARRAY: 1 = ritorno array di DOUBLE */
    /* CATENA di ritorni-block (lambda dentro lambda, profondità arbitraria).
     * fn_ret_n = 0 → la procedura ritorna il valore scalare dichiarato (func_ret_type);
     * fn_ret_n = k > 0 → ritorna un code block; fn_ret_arity[i]/fn_ret_mask[i] sono la
     * firma del block al livello i (i = 0 è il block direttamente ritornato) e fn_ret_str
     * è il tipo del valore FINALE (1 = stringa). I campi func_val_* restano la firma del
     * PRIMO block (livello 0), per i consumatori storici. */
    int fn_ret_n;
    int fn_ret_arity[8];
    int fn_ret_mask[8];
    int fn_ret_str;
    /* RETURN / EXIT */
    gmb_expr_t* ret_value;
    char* exit_target;
    /* GOTO/GOSUB */
    char* target_label;
    /* expr statement */
    gmb_expr_t* expr;
    /* SELECT CASE */
    gmb_expr_t** case_tests; int n_case_tests;
    gmb_expr_t** case_hi;    /* parallelo a case_tests: upper bound di CASE lo TO hi (NULL se non range) */
    char** case_cmp;         /* parallelo a case_tests: operatore di CASE IS <op> val ("=","<>",">","<",">=","<="), NULL se non IS */
    gmb_stmt_t** case_body; int n_case_body;
    gmb_stmt_t** select_cases; int n_select_cases;
    bool case_else;
    /* ON statement */
    const char* on_kind;   /* "ERROR_GOTO" / "ERROR_RESUME" / "KEY" / "MOUSE" / "WINDOW" */
    char* ce_event;        /* ON <EVENTO> CONTROL ...: nome evento */
    gmb_expr_t* of_expr;   /* ON WINDOW <ev> OF <handle|nome>: bersaglio */
    gmb_expr_t* on_expr;   /* ON KEY <expr> */
    bool on_goto;          /* ON KEY ... GOTO (true) o GOSUB (false) */
    gmb_expr_t* on_call;   /* ON ERROR <procedureCall> */
    /* TRY/CATCH/FINALLY */
    gmb_stmt_t** try_body; int n_try;
    char* catch_var;
    /* Tipo dichiarato nella clausola (`CATCH e AS <Classe>` / `AS ERR`): NULL = catch-all.
     * Il nome è l'ULTIMO segmento di un tipo qualificato (`Ns.Cls` -> "Cls"), come per NEW.
     * Senza questo campo l'emitter C99 non può fare il dispatch per tipo (prima il tipo
     * veniva semplicemente saltato dal parser). */
    char* catch_type;
    /* Filtro opzionale della clausola (`CATCH e AS ERR WHEN e.Number = 11`): a parità di
     * tipo la condizione decide se la clausola corrisponde. Prima veniva scartato dal
     * parser: C99Native eseguiva sempre la PRIMA clausola (divergenza con IL/PCode/VM-C). */
    gmb_expr_t* catch_when;
    gmb_stmt_t** catch_body; int n_catch;
    gmb_stmt_t** finally_body; int n_finally;
    gmb_expr_t* error_code;
    gmb_expr_t* throw_expr;
    /* DATA/READ/RESTORE */
    gmb_expr_t** data_values; int n_data;
    char** read_vars; int n_read;
    /* SWAP/RUN */
    char* swap_a; char* swap_b;
    char* run_file;
    gmb_expr_t* run_expr;   /* RUN <espressione> (stringa calcolata) */
    char* require_pkg;
    /* array statements */
    char* array_var;
    gmb_expr_t* array_value;
    gmb_expr_t* array_position;
    char* array_var2;      /* ACOPY: destinazione */
    gmb_expr_t* array_start;   /* AFILL/ACOPY: start (0-based) */
    gmb_expr_t* array_count;   /* AFILL/ACOPY: count */
    gmb_expr_t* array_extra;   /* ACOPY: start destinazione */
    /* console */
    gmb_expr_t* row; gmb_expr_t* col;
    gmb_expr_t* fg; gmb_expr_t* bg;
    gmb_expr_t* input_prompt;
    char** set_attr; int n_set_attr;
    char* id_var;   /* ID <var>: variabile che riceve l'handle del controllo */
    /* file I/O */
    char* file_path; const char* file_mode; int file_mode_tk;  /* mode OPEN: token (mai stringa) */
    gmb_expr_t* file_path_expr;   /* OPEN: path come espressione (per l'emitter C) */
    gmb_expr_t* file_num;
    gmb_expr_t* file_pos;         /* SEEK n, pos: posizione (1-based) */
    gmb_expr_t** control_args; int n_control_args;
    int control_into;   /* indice in control_args del primo target INTO (0 = nessuno) */
    char** ev_pairs; int n_ev;   /* ON <EVENT> <sub>: coppie (evento, sub) */
    /* goroutine/concorrenza */
    gmb_expr_t* channel;
    gmb_expr_t* send_value;
    char* receive_dest; gmb_expr_t* ok_flag;
    const char* mutex_action;   /* LOCK/UNLOCK/RLOCK/RUNLOCK */
    int act_tk;                 /* sottocomando di statement come TokenKind P_* (parser
                                 * domain: MCP/verb AI/SPRITE...); 0 = non specificato.
                                 * Tassonomia L/P: mai dispatch per lexeme. */
    gmb_go_case_t* go_cases; int n_go_cases;   /* GO SELECT */
    gmb_stmt_t* go_comm;        /* DEFER: statement interno / case comm */
    char* win_name;             /* WINDOW: nome finestra / target select */
    gmb_tokenkind_t win_tk;     /* dominio/parola in win_name risolta dal parser (es. Sprite/Ai/Sql/Workflow) */
    int flag;                   /* WINDOW DEFINE: 1=Double 2=Shadow 3=Pixel */
    gmb_expr_t* win_coords[4];  /* WINDOW DEFINE: top,left,bottom,right */
    gmb_expr_t* cond_mutex;     /* COND WAIT: secondo argomento */
    gmb_expr_t* go_wait;        /* GO ... IN wg */
    char* go_into;              /* GO FUNCTION ... INTO target */
    gmb_expr_t* proc_name;      /* GO/ONCE: nome procedura */
    gmb_expr_t** go_args; int n_go_args;
    /* NET_CMD (HTTP SERVER/ROUTE) */
    int net_sub;                          /* subopcode OP_NET_CMD */
    gmb_expr_t** net_args; int n_netargs; /* argomenti (entry NULL → PUSH_NIL) */
    char* net_coderef;                    /* procedura callback (code ref) */
    int coderef_pos;                      /* posizione argomento code ref (-1 = nessuno) */
    char* net_assign;                     /* target 1 assegnato dal top-of-stack post NET_CMD */
    char* net_assign2;                    /* target 2 (secondo pop) */
    char* net_assign3;                    /* target 3 (terzo pop) */
    /* Spazio coordinate DICHIARATO (SCREEN ... USE CELLS|PIXELS):
     * 0 = non dichiarato (AUTO), 1 = CELLS, 2 = PIXELS. L'emitter lo mappa su
     * gmb_gfx_set_coord_mode(-1|1|0). 0 e' il default di calloc = AUTO. */
    int coord_mode;
    /* CLASS (2.7): class_name/class_base; campi = nomi+tipi AS; metodi = FUNCTION stmt
     * annidati (func_name "NEW" = costruttore). PROPERTY/OPERATOR/EVENT: saltati dal parser.
     * 2.7(d)p2b: class_is_interface (INTERFACE = classe senza corpo) e cls_implements
     * (clausola IMPLEMENTS If1, If2 — dispatch dinamico su tipo interfaccia). */
    char* class_name; char* class_base;
    char* class_type_params;   /* TYPE/CLASS generico `Pair<T1,T2>`: CSV dei nomi dei type-param
                                * ("T1,T2"); NULL se non generico. Usato per specializzare le
                                * istanze `Pair<INTEGER,STRING>` nell'emitter. */
    int class_is_interface;
    int class_is_udt;
    /* PACKED TYPE (canone SIZEOF): i campi stanno alla somma dei propri byte, senza padding
     * di allineamento — il flag deve giungere alla tabella classi dell'emitter (cls_info). */
    int class_is_packed;
    /* UNION (2026-11-11): tutti i campi a offset 0 (semantica C). L'emitter C99 emette una
     * union ANONIMA (C11) con i tipi C reali: l'aliasing lo fa il compilatore C. */
    int class_is_union;
    /* BITFIELD (`A AS LONG WIDTH 3`, 2026-11-11): larghezza in bit per campo (0 = normale).
     * L'emitter emette veri bitfield C (`int64_t A : 3`) — LSB-first, come le altre gambe. */
    int* cls_field_bits;
    char** cls_implements; int n_cls_implements;
    char** cls_field_names; char** cls_field_types; int n_cls_fields;
    char** cls_static_names; char** cls_static_types; gmb_expr_t** cls_static_inits; int n_cls_statics;
    /* EVENT Nome(...) (2.9): solo il nome (i parametri dichiarati non servono al
     * compilatore — RAISE EVENT passa i propri argomenti a runtime senza verifica di
     * arity) — usato per inizializzare un registro handler (lista) per-istanza al
     * costruttore, vedi cc_class_register/cc_class_member in gmb_compiler.c. */
    char** cls_event_names; int n_cls_events;
    gmb_stmt_t** cls_methods; int n_cls_methods;
    /* IMPORT <ns>[(members)] [AS alias]: direttiva di namespace registrata nell'AST. */
    char* imp_ns;
    char* imp_alias;
    char** imp_members; int n_imp_members, cap_imp_members;
};
typedef struct gmb_go_case { gmb_stmt_t* comm; gmb_stmt_t** body; int n_body; } gmb_go_case_t;

gmb_stmt_t* gmb_stmt_new(gmb_stmt_kind_t kind, int line);
void gmb_stmt_add(gmb_stmt_t*** list, int* n, gmb_stmt_t* s, int* cap);
void gmb_stmt_free(gmb_stmt_t* s);
void gmb_expr_free(gmb_expr_t* e);

#endif
