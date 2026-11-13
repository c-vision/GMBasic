/* native/gmb_runtime_c/src/parser/gmb_parser_internal.h — stato del parser condiviso tra i
 * moduli gmb_parser_*.c (speculari ai file Parser.*.cs). */
#ifndef GMB_PARSER_INTERNAL_H
#define GMB_PARSER_INTERNAL_H

#include "gmb_ast.h"
#include "gmb_lexer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stddef.h>

typedef struct gmb_parser {
    const gmb_token_t* toks;
    int n;
    int pending_for_ends;   /* FOR annidati chiusi dalla clausola NEXT multipla (es. NEXT J, I) */
    /* Stack dei loop in parsing (kinds GMB_TOK_For/While/Do): l'EXIT/CONTINUE nudo dentro
     * un loop eredita il kind più interno (canone 2026-09-30: esce dal loop, non dalla SUB).
     * Push/pop nel dispatcher (parse_for/while/do), mai nei rami early-return interni. */
    int loop_stack[32];
    int n_loop_stack;
    int last_bas_line;      /* 2.8: ultimo numero riga BASIC (LineNumber) consumato da skip_eol */
    int bas_fresh;          /* 2.8: last_bas_line appartiene alla riga logica corrente (non a una precedente) */
    int pos;
    char* error;
    /* coda statement extra prodotti da un singolo parse (es. DIM a(2), b(2) → due DIM);
     * drenata da gmb_p_parse_block subito dopo lo statement principale */
    gmb_stmt_t** queued; int n_queued; int cap_queued;
    /* alias di namespace registrati da IMPORT <NS> AS <X> (parser monofile: il
     * qualificato <X>.FUNC deve essere risolto come builtin, regola d'oro) */
    int import_aliases_n;
    char import_aliases[16][32];
    char import_aliases_ns[16][96];   /* ns FISICO (es. "TRADING.PORTFOLIO") per l'alias i */
    /* Nomi DICHIARATI (DIM/param) — per la risoluzione nome-vs-variabile negli argomenti
     * dei comandi AI: un identificatore nudo dichiarato resta una VARIABILE, non il
     * letterale-nome (parità Parser._declaredVariableNames, bug P4). */
    char declared[512][64];
    int n_declared;
    /* Costanti stringa note a compile-time (DIM <name> AS STRING = "literal", mai
     * riassegnata): usate per espandere &(<name>)/EVAL(<name>) che producono code block
     * senza compilazione a runtime (i transpiler C non hanno un compilatore BASIC).
     * Svuotata a fine FUNCTION/SUB (scope di procedura) e su assegnazione del nome. */
    char cstr_names[64][64];
    char* cstr_vals[64];
    int n_cstr;
    /* 2026-09-24: namespace della direttiva NAMESPACE attiva ("" = globale). Timbrato
     * sulle definizioni FUNCTION/DECLARE (func_ns) per il check di shadowing dei
     * builtin — parità Parser._currentNamespace (C#). */
    char cur_ns[96];
    /* Elenco dei namespace UTENTE dichiarati (direttiva NAMESPACE ...): gmb_p_strip_ns lo
     * consulta oltre ai LangNamespace della StdLib, così `NS.MEMBRO` è riconosciuto come
     * qualificato anche per namespace DEFINITI NEL PROGRAMMA (es. package protobuf). */
    int ns_decls_n;
    char ns_decls[64][96];
    /* LIBRARY "nome": nome della libreria condivisa ("" = eseguibile). */
    char lib_name[96];
    /* 1 = e' stato parsato almeno uno statement NON-REM (comando/codice). La direttiva
     * LIBRARY e' valida solo finche' resta 0 (prima di ogni comando; i commenti non contano). */
    int saw_stmt;
    /* Modalita' grafica: i controlli keyword (LISTBOX/SPINNER/…) emettono la versione PIXEL
     * (nome rinominato in "PIX<NAME>" a parse-time). */
    int gui_mode;
    /* OPTION STRICT ON|OFF: direttiva di modulo (parità linker C#). Con ON l'emitter rifiuta
     * ogni nome non dichiarato. */
    int option_strict;
    /* GATE dei NAMESPACE (parità `KnownLibraryNotImported` del linker C#): namespace
     * IMPORTati nel programma (normalizzati in MAIUSCOLO, anche dotted). Una funzione di
     * LIBRERIA si chiama nuda SOLO se il suo namespace è qui o è fra i default del
     * linguaggio: `RMDIR`-style i comandi non c'entrano, questo vale per le funzioni. */
    char imports[64][96];
    int n_imports;
    /* Procedure DICHIARATE dal programma (FUNCTION/SUB, anche metodi di classe): una
     * funzione UTENTE con lo stesso nome di un builtin di libreria lo SHADOWA (il C# lo
     * risolve a link-time), quindi il gate dei namespace non deve scattare. Raccolte dallo
     * stesso pre-scan degli IMPORT (copre anche le dichiarazioni successive al punto d'uso). */
    char user_fns[512][64];
    int n_user_fns;
    /* Nomi DICHIARATI (DIM/REDIM/GLOBAL/LOCAL/STATIC, anche nelle forme MULTIPLE
     * `DIM a(2), b(2)` e `DIM g(7) AS T, info(0) AS LONG`): il parser registra in
     * `declared` solo la PRIMA di un DIM multiplo, e la tabella ha cap 512 — qui il
     * pre-scan li raccoglie tutti, così `info(0)`/`mask(0)` (accessi a elemento) non
     * vengono scambiati per la funzione di libreria omonima. */
    char decl_names[1024][64];
    int n_decl_names;
    /* 1 = il sorgente è un FRAMMENTO EVAL/& (wrapper `SUB|FUNCTION __GMB_EVAL__`, vedi
     * gmb_bytecode/snip_wrap_text): un frammento non è un programma e non può contenere
     * IMPORT → il gate dei namespace è sospeso (il valutatore C# della macro risolve i
     * builtin per nome). Rilevato dal pre-scan di gmb_p_collect_imports. */
    int snippet;
    /* PADRE quando questo parser è il SUB-PARSE di un frammento COSTANTE (`EVAL("...")`/
     * `&("...")` di un letterale, gmb_p_try_expand_const_macro): il testo è parte dello
     * STESSO modulo, quindi il gate dei namespace deve leggere il contesto del padre
     * (IMPORT, nomi dichiarati, procedure utente) e non vedere un programma vuoto. */
    const struct gmb_parser* parent;
} gmb_parser_t;

typedef struct {
    gmb_stmt_t** items;
    int n, cap;
} gmb_stmt_list_t;

/* ── dichiarazioni (DIM/param) — parità Parser._declaredVariableNames ── */
void gmb_p_declare_name(gmb_parser_t* p, const char* name);
void gmb_p_cstr_set(gmb_parser_t* p, const char* name, const char* value);
/* Fold di un'espressione COSTANTE in stringa (letterale, variabile con costante nota,
 * concatenazione `+`): 1 se il valore è noto — abilita l'espansione compile-time di
 * `&(<name>)` anche per stringhe costruite da parti costanti (`DIM s = pre + "..."`). */
int gmb_p_const_str_fold(const gmb_parser_t* p, const gmb_expr_t* e, char* out, size_t cap);
void gmb_p_cstr_clear(gmb_parser_t* p, const char* name);
const char* gmb_p_cstr_get(const gmb_parser_t* p, const char* name);
void gmb_p_cstr_reset(gmb_parser_t* p);
bool gmb_p_is_declared(const gmb_parser_t* p, const char* name);
bool gmb_p_is_declared_tok(const gmb_parser_t* p, const gmb_token_t* t);

/* ── utils (Parser.Utils.cs) ── */
const gmb_token_t* gmb_p_peek(gmb_parser_t* p, int off);
/* Campo DIMensionato di un TYPE: "arr(4) AS LONG" ha l'AS DOPO le parensi, quindi il guard
 * (peek(1) == AS) del corpo TYPE non lo vede e il campo va perduto. Bilancia le parensi a
 * partire da peek(1) e dice se, chiusa la dimensione, c'è AS sulla stessa riga. */
int gmb_p_udt_field_dims_ahead(gmb_parser_t* p);
/* 2026-09-24: cattura il nome del namespace della direttiva NAMESPACE corrente (anche
 * dotted, es. TRADING.PORTFOLIO) in p->cur_ns; riga vuota = reset a "" (globale). */
void gmb_p_capture_namespace(gmb_parser_t* p);
/* ── GATE dei NAMESPACE (parità `KnownLibraryNotImported`, Linker.Rewrites.cs) ──
 * `gmb_p_collect_imports` pre-scansiona i token e registra in p->imports i namespace
 * IMPORTati (anche dotted, normalizzati in maiuscolo; `IMPORT A, B`, `A (membri)`,
 * `A AS x` e `A.*` sono gestiti). `gmb_p_check_bare_builtin` verifica una chiamata NUDa
 * già risolta a TokenKind: se il nome appartiene a uno o più namespace di libreria e
 * NESSUNO è caricato (import esplicito o default del linguaggio) → errore esplicito, come
 * il linker C#. Ritorna 1 se la chiamata è lecita, 0 se ha settato p->error. */
void gmb_p_collect_imports(gmb_parser_t* p);
int  gmb_p_check_bare_builtin(gmb_parser_t* p, gmb_tokenkind_t tk, const char* name, int line);
const gmb_token_t* gmb_p_cur(gmb_parser_t* p);
bool gmb_p_at_end(gmb_parser_t* p);
void gmb_p_adv(gmb_parser_t* p);
bool gmb_p_check(gmb_parser_t* p, gmb_tokenkind_t k);
bool gmb_p_match(gmb_parser_t* p, gmb_tokenkind_t k);
void gmb_p_set_err(gmb_parser_t* p, const char* m, int line);
bool gmb_p_tok_is_keyword(const gmb_token_t* t, const char* kw);
/* Soft keyword (nome non nella keyword-table del lexer, es. BUTTON/CONTROL/GETS/PUMP):
 * match su lexeme+length. Non usare per le keyword hard (quelle hanno TokenKind → gmb_p_tok_is_keyword). */
bool gmb_p_tok_is_soft_keyword(const gmb_token_t* t, const char* kw);
/* Regola d'oro §5 (dispatch per TokenKind, MAI per lexeme): nome di tipo STRING / numerico. */
static inline bool gmb_p_tok_is_string_type_name(const gmb_token_t* t) {
    return t && t->kind == GMB_TOK_StringType;
}
static inline bool gmb_p_tok_is_scalar_type_name(const gmb_token_t* t) {
    if (!t) return false;
    switch (t->kind) {
        case GMB_TOK_Long: case GMB_TOK_Integer: case GMB_TOK_Quad: case GMB_TOK_Byte:
        case GMB_TOK_SByte: case GMB_TOK_UQuad: case GMB_TOK_ULong: case GMB_TOK_UInteger:
            return true;
        default:
            return false;
    }
}
void gmb_p_skip_eol(gmb_parser_t* p);
void gmb_p_extract(char* out, int cap, const gmb_token_t* t);
char* gmb_p_tok_strdup(const gmb_token_t* t);
double gmb_p_num_literal(const char* b);   /* 2.8: letterale numerico BASIC (0x/0b/0o/&H/suffissi) */
bool gmb_p_at_end_compound(gmb_parser_t* p, gmb_tokenkind_t second);
/* coda statement multipli prodotti da un singolo statement sorgente (es. DIM multiplo) */
void gmb_p_queue_stmt(gmb_parser_t* p, gmb_stmt_t* s);
void gmb_p_drain_queued(gmb_parser_t* p, gmb_stmt_list_t* out);
gmb_stmt_list_t gmb_p_stmt_list_new(void);
void gmb_p_stmt_list_add(gmb_stmt_list_t* L, gmb_stmt_t* s);
void gmb_p_stmt_list_free(gmb_stmt_list_t* L);

/* ── espressioni (Parser.Expressions.cs, .Literals.cs, .Complex.cs) ── */
gmb_expr_t* gmb_p_parse_expr(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_primary(gmb_parser_t* p);

/* ── statement (Parser.Statements.cs, Parser.Stmt.*.cs) ── */
gmb_stmt_t* gmb_p_parse_stmt(gmb_parser_t* p, bool* multiline);
gmb_stmt_t* gmb_p_parse_using(gmb_parser_t* p, int line, bool* multiline);   /* 2026-09-03 */
gmb_stmt_t* gmb_p_parse_aipatch(gmb_parser_t* p, int line);   /* §4.10 write-side */
void gmb_p_parse_block(gmb_parser_t* p, gmb_stmt_list_t* out, gmb_tokenkind_t stop_kw,
                       const char* stop_name, int start_line);
/* Parser.Stmt.Loops.cs */
gmb_stmt_t* gmb_p_parse_for(gmb_parser_t* p, int line, bool* multiline);
gmb_stmt_t* gmb_p_parse_while(gmb_parser_t* p, int line, bool* multiline);
gmb_stmt_t* gmb_p_parse_do(gmb_parser_t* p, int line, bool* multiline);
gmb_stmt_t* gmb_p_parse_tick(gmb_parser_t* p, int line, bool* multiline);
/* Parser.Stmt.Branches.cs */
gmb_stmt_t* gmb_p_parse_if(gmb_parser_t* p, int line, bool* multiline);
gmb_stmt_t* gmb_p_parse_select_case(gmb_parser_t* p, int line, bool* multiline);
/* Parser.Stmt.Goto.cs */
gmb_stmt_t* gmb_p_parse_goto(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_gosub(gmb_parser_t* p, int line);
/* Parser.Stmt.On.cs */
gmb_stmt_t* gmb_p_parse_on(gmb_parser_t* p, int line, bool* multiline);
/* Parser.Stmt.Exceptions.cs */
gmb_stmt_t* gmb_p_parse_try_catch(gmb_parser_t* p, int line, bool* multiline);
gmb_stmt_t* gmb_p_parse_error(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_throw(gmb_parser_t* p, int line);
/* Parser.Stmt.DataRead.cs */
gmb_stmt_t* gmb_p_parse_data(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_read(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_restore(gmb_parser_t* p, int line);
/* Parser.Stmt.Commands.cs */
gmb_stmt_t* gmb_p_parse_redim(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_erase(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_swap(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_stop(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_run(gmb_parser_t* p, int line);
/* PROCESS <file> [PROMPT <istruzioni>] (E3): statement-espressione sul builtin PROCESS. */
gmb_stmt_t* gmb_p_parse_process(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_require(gmb_parser_t* p, int line);
/* EMBEDFILE "file" AS nome — direttiva compile-time: il contenuto diventa un letterale
 * stringa binario (DIM + assegnazione in coda). Vedi gmb_parser_stmt_embed.c. */
gmb_stmt_t* gmb_p_parse_embedfile(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_yield(gmb_parser_t* p, int line);
/* Parser.Stmt.Arrays.cs */
gmb_stmt_t* gmb_p_parse_aadd(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_ains(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_adel(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_ashuffle(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_asort(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_afill(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_acopy(gmb_parser_t* p, int line);
/* Parser.Stmt.Console.cs */
gmb_stmt_t* gmb_p_parse_cls(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_locate(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_color(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_inkey(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_getkey(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_savescreen(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_restorescreen(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_atform(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_scrollmode(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_caption(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_input(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_lineinput_console(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_set(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_sendkey(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_clip(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_shell(gmb_parser_t* p, int line);
/* Parser.Stmt.Files.cs */
gmb_stmt_t* gmb_p_parse_open(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_fread_into(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_close(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_seek(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_printfile(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_putfile(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_getfile(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_lockfile(gmb_parser_t* p, int line, int unlock);
gmb_stmt_t* gmb_p_parse_inputfile(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_lineinput(gmb_parser_t* p, int line);
/* Parser.Stmt.Controls.cs (widget TUI generici) */
gmb_stmt_t* gmb_p_parse_control(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_control_method(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_fire(gmb_parser_t* p, int line);
/* BUTTON x, y, w, "label" ON CLICK <sub> — bottone non-modale con callback GPM. */
gmb_stmt_t* gmb_p_parse_button(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_panel(gmb_parser_t* p, int line);
/* Parser.Stmt.GoRoutines.cs (GO/SEND/RECEIVE/CHANNEL/MUTEX/COND/ONCE/...) */
gmb_stmt_t* gmb_p_parse_goroutine(gmb_parser_t* p, int line);
/* Parser.Stmt.Events.cs (RAISE EVENT / ADD HANDLER / REMOVE HANDLER) */
gmb_stmt_t* gmb_p_parse_events(gmb_parser_t* p, int line);
/* Parser.Stmt.Window.cs (WINDOW THEME/DEFINE/SELECT/... e azioni) */
gmb_stmt_t* gmb_p_parse_window(gmb_parser_t* p, int line);
/* Parser.Stmt.Mouse.cs (MOUSE ON/OFF/SHOW/HIDE/SET/LIMIT/CURSOR/WAIT) */
gmb_stmt_t* gmb_p_parse_mouse(gmb_parser_t* p, int line);
/* Parser.Stmt.Compression.cs (ZIP/UNZIP) */
gmb_stmt_t* gmb_p_parse_compression(gmb_parser_t* p, int line);
/* Parser.Stmt.Json.cs (JSON SAVE/WRITEVALUE/WRITESECTION/OPEN) */
gmb_stmt_t* gmb_p_parse_json(gmb_parser_t* p, int line);
/* Parser.Stmt.Team.cs (TEAM CREATE/ADD/REMOVE/RUN/SET) */
gmb_stmt_t* gmb_p_parse_team(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_message(gmb_parser_t* p, int line);
int gmb_p_is_message_verb(const gmb_token_t* t);
/* Parser.Stmt.Signal.c (SIGNAL SUBSCRIBE/EMIT/UNSUBSCRIBE/PING, ON SIGNAL) */
gmb_stmt_t* gmb_p_parse_signal(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_on_signal(gmb_parser_t* p, int line);
/* Parser.Stmt.AiAgent.cs (AI AGENT CREATE/DELETE/SET) */
gmb_stmt_t* gmb_p_parse_agent(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_ai_server(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_ai_model(gmb_parser_t* p, int line);
/* Parser.Stmt.Mcp.cs (AI MCP SERVER/SERVE/STOP/REGISTER/UNREGISTER) */
gmb_stmt_t* gmb_p_parse_mcp(gmb_parser_t* p, int line);
/* Parser.Stmt.Convert.c (CONVBAT <dir>[,<fmt>][,<rec>] INTO <target>) */
gmb_stmt_t* gmb_p_parse_convbat(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_web2md(gmb_parser_t* p, int line);
/* comandi di dominio generici (Dbf/Isam/Sql/Ledger/Rdd/Speed/Matrix/Vector/Torch/Ai/Http/WebSocket/Workflow) */
bool gmb_p_is_domain_ns(gmb_tokenkind_t k);
gmb_stmt_t* gmb_p_parse_domain_stmt(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_domain_stmt_ov(gmb_parser_t* p, int line, const char* ov_name, gmb_tokenkind_t ov_tk);

/* CLASS...END CLASS / TYPE...END TYPE (2.7/2.9) — gmb_parser_stmt_class.c */
gmb_stmt_t* gmb_p_parse_class(gmb_parser_t* p, int line, int is_udt, int is_packed);
gmb_stmt_t* gmb_p_parse_operator_method(gmb_parser_t* p, int line);

/* NET (TCP/UDP/SSL/WS/HTTP) + GFX — gmb_parser_stmt_net.c */
gmb_stmt_t* gmb_p_parse_net_cmd(gmb_parser_t* p, int line);
gmb_stmt_t* gmb_p_parse_gfx(gmb_parser_t* p, int line);
/* SENDKEY (parser della console: definita in gmb_parser_stmt_console.c, usata dal
 * dispatcher in gmb_parser_statements.c). Mancava la dichiarazione → il compilatore C
 * non compilava (implicit declaration vietata, ISO C99). */
gmb_stmt_t* gmb_p_parse_sendkey(gmb_parser_t* p, int line);

/* statement di consumo — gmb_parser_statements.c */
void gmb_p_consume_statement(gmb_parser_t* p);
gmb_stmt_t* gmb_p_parse_assign_or_call(gmb_parser_t* p, const char* target, int line);
gmb_stmt_t* gmb_p_make_incdec(gmb_expr_t* rd, int plus, int line);
gmb_expr_t* gmb_p_parse_unary(gmb_parser_t* p);

#endif
