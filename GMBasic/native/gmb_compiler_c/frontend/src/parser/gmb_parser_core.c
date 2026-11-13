/* native/gmb_runtime_c/src/parser/gmb_parser_core.c — speculare a Parser.cs: loop del programma. */
#include "gmb_parser.h"
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Vero se in una lista di statement (a livello di MODULO) esiste una `LET x = ...`: la
 * specifica (Variables-Scope §3.1) la vieta fuori dalle procedure. Si scende nei blocchi
 * (IF/FOR/WHILE/DO/SELECT/TRY) ma NON nei corpi delle procedure (`func_body`), dove la LET è
 * lecita. Usato da `gmb_parse` dopo il parsing. */
static int gmb_p_top_level_let(gmb_stmt_t** list, int n) {
    if (!list) return 0;
    for (int i = 0; i < n; i++) {
        gmb_stmt_t* s = list[i];
        if (!s) continue;
        if (s->kind == GMB_STMT_ASSIGN && s->is_let) return 1;
        if (gmb_p_top_level_let(s->then_body, s->n_then)) return 1;
        if (gmb_p_top_level_let(s->else_body, s->n_else)) return 1;
        if (gmb_p_top_level_let(s->for_body, s->n_for)) return 1;
        if (gmb_p_top_level_let(s->while_body, s->n_while)) return 1;
        if (gmb_p_top_level_let(s->do_body, s->n_do)) return 1;
        if (gmb_p_top_level_let(s->case_body, s->n_case_body)) return 1;
        if (gmb_p_top_level_let(s->try_body, s->n_try)) return 1;
        if (gmb_p_top_level_let(s->catch_body, s->n_catch)) return 1;
        if (gmb_p_top_level_let(s->finally_body, s->n_finally)) return 1;
    }
    return 0;
}

gmb_program_t gmb_parse(const gmb_lex_result_t* lx, char** error)
{
    gmb_program_t prog = {0};
    gmb_parser_t p = {0};
    p.toks = lx->tokens;
    p.n = lx->count;
    p.error = NULL;
    /* Pre-scan modalita' grafica: se il programma contiene `SCREEN n` con n != 0,
     * i controlli keyword (LISTBOX/BUTTON/STATUSBAR/...) si emettono in versione PIXEL
     * per TUTTO il programma, indipendentemente dall'ordine di definizione delle SUB
     * (GuiBuildForm puo' precedere la SUB che esegue SCREEN). SCREEN 0 => TUI. */
    for (int i = 0; i + 1 < p.n; i++) {
        if (p.toks[i].kind != GMB_TOK_Screen) continue;
        int j = i + 1;
        while (j < p.n && p.toks[j].kind == GMB_TOK_Newline) j++;
        if (j < p.n && p.toks[j].kind == GMB_TOK_Number) {
            char buf[64]; int ln = p.toks[j].length; if (ln > 63) ln = 63;
            memcpy(buf, p.toks[j].lexeme, (size_t)ln); buf[ln] = 0;
            if (strtod(buf, NULL) != 0.0) { p.gui_mode = 1; break; }
        }
    }
    /* GATE dei NAMESPACE: raccoglie gli IMPORT del programma (pre-scan dei token, come il
     * pre-scan SCREEN sopra: una direttiva può stare dopo il punto d'uso) — serve a
     * `gmb_p_check_bare_builtin` per rifiutare un nome nudo di libreria non importato. */
    gmb_p_collect_imports(&p);

    while (!gmb_p_at_end(&p)) {
        gmb_p_skip_eol(&p);
        if (gmb_p_at_end(&p)) break;
        /* Bug corretto (2026-08-31): gmb_p_skip_eol (chiamata qui sopra) può aver già
         * accodato uno o più statement REM per numeri di riga "scartati" (righe
         * SOLO commento/vuote, es. "100 REM Routine..." seguita da "110 PRINT ...")
         * — vedi gmb_p_queue_bas_rem in gmb_parser_utils.c. Questi PRECEDONO
         * logicamente lo statement principale che stiamo per parsare (il loro numero
         * di riga viene PRIMA), quindi vanno drenati in prog.stmts ORA, PRIMA di
         * chiamare gmb_p_parse_stmt — non insieme alla coda "post" più sotto
         * (pensata per statement multipli generati DURANTE il parsing dello
         * statement principale, es. `DIM a(2), b(2)`, che invece lo SEGUONO
         * logicamente). Confondere le due code dava un ordine di programma
         * invertito (bug: FunctionsMain.bas, "100 REM..."/"110 PRINT..." finiva
         * compilato come [PRINT(110), REM(100)] invece di [REM(100), PRINT(110)] —
         * l'indirizzo registrato per "100" finiva quindi DOPO il PRINT invece che
         * prima, e "GOSUB 100" saltava a un punto che saltava silenziosamente il
         * corpo vero della subroutine). */
        for (int i = 0; i < p.n_queued; i++) {
            if (prog.n >= prog.cap) { prog.cap = prog.cap ? prog.cap * 2 : 16; prog.stmts = (gmb_stmt_t**)realloc(prog.stmts, (size_t)prog.cap * sizeof(gmb_stmt_t*)); }
            prog.stmts[prog.n++] = p.queued[i];
        }
        p.n_queued = 0;
        int bas = p.bas_fresh ? p.last_bas_line : 0;   /* 2.8: numero riga BASIC della riga logica */
        bool ml = false;
        gmb_stmt_t* s = gmb_p_parse_stmt(&p, &ml);
        if (s) {
            if (prog.n >= prog.cap) { prog.cap = prog.cap ? prog.cap * 2 : 16; prog.stmts = (gmb_stmt_t**)realloc(prog.stmts, (size_t)prog.cap * sizeof(gmb_stmt_t*)); }
            prog.stmts[prog.n++] = s;
            if (s->kind != GMB_STMT_REM) p.saw_stmt = 1;   /* LIBRARY valida solo prima di questo */
        }
        /* statement multipli prodotti da una sola riga (es. DIM a(2), b(2)) */
        for (int i = 0; i < p.n_queued; i++) {
            if (bas > 0 && p.queued[i] && !p.queued[i]->bas_line) p.queued[i]->bas_line = bas;
            if (prog.n >= prog.cap) { prog.cap = prog.cap ? prog.cap * 2 : 16; prog.stmts = (gmb_stmt_t**)realloc(prog.stmts, (size_t)prog.cap * sizeof(gmb_stmt_t*)); }
            prog.stmts[prog.n++] = p.queued[i];
            if (p.queued[i] && p.queued[i]->kind != GMB_STMT_REM) p.saw_stmt = 1;
        }
        p.n_queued = 0;
        if (p.error) break;
        if (!ml) {
            /* ':' separa statement sulla stessa riga */
            if (gmb_p_check(&p, GMB_TOK_Colon)) { gmb_p_adv(&p); continue; }
            gmb_p_skip_eol(&p);
        }
    }
    memcpy(prog.library_name, p.lib_name, sizeof(prog.library_name));
    prog.option_strict = p.option_strict;
    /* `LET` a livello di MODULO = errore di sintassi (specifica Variables-Scope §3.1): con
     * `p.error` già valorizzato non si sovrascrive l'errore originale. Si scende nei blocchi
     * (IF/FOR/...) ma NON nei corpi delle procedure (func_body). */
    if (!p.error && gmb_p_top_level_let(prog.stmts, prog.n))
    {
        /* `p.error` è LIBERATO dal chiamante (contratto di gmb_parse: `*error` è una stringa
         * allocata, vedi `gmb_p_set_err`). Un buffer `static` qui faceva `free()` di memoria
         * non allocata → abort del driver ("pointer being freed was not allocated") su ogni
         * programma con `LET` a livello di modulo (constructs/let_scope_modulo). */
        p.error = strdup("LET è valido solo dentro una procedura (SUB/FUNCTION/GENERATOR): "
                         "a livello di modulo usare un'assegnazione (x = ...) oppure DIM/GLOBAL.");
    }
    if (error) *error = p.error;
    return prog;
}

void gmb_program_free(gmb_program_t* prog)
{
    for (int i = 0; i < prog->n; i++) gmb_stmt_free(prog->stmts[i]);
    free(prog->stmts);
    prog->stmts = NULL; prog->n = prog->cap = 0;
}

