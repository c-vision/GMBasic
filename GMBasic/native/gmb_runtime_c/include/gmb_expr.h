/* ============================================================================
 * gmb_expr.h — Valutatore BASIC runtime (& / EVAL) condiviso da VM/C e C99.
 * Replica la semantica C# di IlRuntime.MacroEval / MacroEvalExpr: parse della
 * stringa come espressione o comando, assegnazione top-level `ident = expr`,
 * identificatori via callback env (get/set). Tree-walk su gmb_val_t (v1).
 * ============================================================================ */
#ifndef GMB_EXPR_H
#define GMB_EXPR_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_EXPR_OK          0
#define GMB_EXPR_ERR_PARSE  -1
#define GMB_EXPR_ERR_EVAL   -2
#define GMB_EXPR_ERR_UNDEF  -3

typedef int (*gmb_expr_get_fn)(void* env, const char* name, gmb_val_t* out);
typedef int (*gmb_expr_set_fn)(void* env, const char* name, gmb_val_t in);
/* Funzione utente ospite: 0 = ok (out valorizzato), ≠0 = sconosciuta. */
typedef int (*gmb_expr_call_fn)(void* env, const char* name,
                                const gmb_val_t* args, int nargs, gmb_val_t* out);

int gmb_expr_eval(const char* src, void* env,
                  gmb_expr_get_fn get, gmb_expr_set_fn set,
                  gmb_val_t* result);
int gmb_expr_eval_ex(const char* src, void* env,
                     gmb_expr_get_fn get, gmb_expr_set_fn set,
                     gmb_expr_call_fn call, gmb_val_t* result);
/* Filtro DB / SPEED: `=` resta uguaglianza (non LET). Stringhe `"..."` e `'...'`. */
int gmb_expr_eval_filter(const char* src, void* env,
                         gmb_expr_get_fn get, gmb_expr_set_fn set,
                         gmb_val_t* result);
int gmb_expr_eval_filter_ex(const char* src, void* env,
                            gmb_expr_get_fn get, gmb_expr_set_fn set,
                            gmb_expr_call_fn call,
                            gmb_val_t* result);
int gmb_expr_exec(const char* src, void* env,
                  gmb_expr_get_fn get, gmb_expr_set_fn set);
int gmb_expr_exec_ex(const char* src, void* env,
                     gmb_expr_get_fn get, gmb_expr_set_fn set,
                     gmb_expr_call_fn call);
/* Messaggio dell'ultimo errore del valutatore (TLS), per il contratto C#
 * "MACRO ERROR: <msg>" (VM/C e C99*): valido dopo un rc != GMB_EXPR_OK. */
const char* gmb_expr_last_error(void);
/* Come exec, ma PRINT va in *out_text (malloc, caller free) invece che su stdout.
 * n->op su PRINT: 0 = newline finale, 1 = soppresso (`;`). */
int gmb_expr_exec_capture(const char* src, void* env,
                          gmb_expr_get_fn get, gmb_expr_set_fn set,
                          gmb_expr_call_fn call, char** out_text);

int gmb_expr_dict_get(void* env, const char* name, gmb_val_t* out);
int gmb_expr_dict_set(void* env, const char* name, gmb_val_t in);

/* ---- S_eval: scope transiente per &/EVAL (condiviso VM/C e C99) ----
 * Ogni esecuzione EVAL gira in un nuovo scope figlio (catena verso il parent);
 * DIM/LET/assegnazione-su-ignota creano qui e muoiono a fine eval (mai leak
 * nei globali/host); l'assegnazione implicita cerca scope→parent→backend e
 * scrive in place. Frame opaco, allocato per-esecuzione, nessunaTLS/globali
 * (GPM-safe: vive nello stack di chiamata del task). */
typedef struct gmb_expr_scope gmb_expr_scope_t;
gmb_expr_scope_t* gmb_expr_scope_push(gmb_expr_scope_t* parent);
void gmb_expr_scope_pop(gmb_expr_scope_t* s);
/* Cerca nella catena (corrente → parent): 1 trovato (out valorizzato), 0 no. */
int gmb_expr_scope_get(gmb_expr_scope_t* s, const char* name, gmb_val_t* out);
/* Crea/aggiorna NEL frame corrente (copia la stringa, alias per il resto). */
void gmb_expr_scope_set(gmb_expr_scope_t* s, const char* name, gmb_val_t v);
/* Aggiorna in place dov'è definito nella catena: 1 scritto, 0 assente. */
int gmb_expr_scope_update(gmb_expr_scope_t* s, const char* name, gmb_val_t v);

/* ---- AST pubblico (Fase 8: compile-to-opcode). I valori di tok coincidono
 * con l'enum interno di gmb_expr.c; non riordinare. */
typedef enum {
    GMB_XT_EOF = 0, GMB_XT_NUM, GMB_XT_STR, GMB_XT_ID,
    GMB_XT_PLUS, GMB_XT_MINUS, GMB_XT_STAR, GMB_XT_SLASH, GMB_XT_PERCENT, GMB_XT_CARET,
    GMB_XT_EQ, GMB_XT_NE, GMB_XT_LT, GMB_XT_GT, GMB_XT_LE, GMB_XT_GE,
    GMB_XT_LP, GMB_XT_RP, GMB_XT_COMMA, GMB_XT_COLON,
    GMB_XT_AND, GMB_XT_OR, GMB_XT_NOT, GMB_XT_LET, GMB_XT_PRINT, GMB_XT_AMP,
    GMB_XT_IF, GMB_XT_THEN, GMB_XT_ELSE, GMB_XT_ENDIF,
    /* Fase 9: append-only — non riordinare i valori sopra. */
    GMB_XT_SEMI, GMB_XT_FOR, GMB_XT_TO, GMB_XT_NEXT, GMB_XT_STEP,
    GMB_XT_WHILE, GMB_XT_WEND, GMB_XT_END,
    /* S_eval: append-only. */
    GMB_XT_DIM, GMB_XT_STATIC
} gmb_expr_tok;

typedef enum {
    GMB_XN_LIT = 0, GMB_XN_ID, GMB_XN_UN, GMB_XN_BIN, GMB_XN_CALL,
    GMB_XN_ASSIGN, GMB_XN_PRINT, GMB_XN_SEQ, GMB_XN_IF,
    GMB_XN_FOR, GMB_XN_WHILE,
    /* S_eval: append-only — non riordinare i valori sopra. */
    GMB_XN_DECL
} gmb_expr_nd;

typedef struct gmb_expr_node {
    gmb_expr_nd kind;
    int op;                 /* gmb_expr_tok per UN/BIN */
    gmb_val_t lit;
    char name[72];
    struct gmb_expr_node* a;
    struct gmb_expr_node* b;
    struct gmb_expr_node** args;
    int nargs;
} gmb_expr_node;

typedef struct gmb_expr_prog {
    gmb_expr_node* root;
    char* arena;
    int failed;
    char err[256];
} gmb_expr_prog;

int gmb_expr_parse(const char* src, int as_stmt, int eq_is_assign, gmb_expr_prog* out);
void gmb_expr_prog_free(gmb_expr_prog* p);

#ifdef __cplusplus
}
#endif

#endif /* GMB_EXPR_H */
