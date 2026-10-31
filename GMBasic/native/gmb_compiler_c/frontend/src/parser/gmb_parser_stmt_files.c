/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_files.c — speculare a
 * Parser.Stmt.Files.cs: OPEN, CLOSE, PRINT#, INPUT#, LINEINPUT#, GET, PUT. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

static char* read_name(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    char* s = (char*)malloc((size_t)t->length + 1);
    memcpy(s, t->lexeme, (size_t)t->length); s[t->length] = 0;
    gmb_p_adv(p);
    return s;
}

/* OPEN "file" [FOR INPUT|OUTPUT|APPEND|RANDOM|BINARY] [ACCESS ...] AS [#]n */
gmb_stmt_t* gmb_p_parse_open(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_OPEN, line);
    const gmb_token_t* f = gmb_p_cur(p);
    /* Il percorso è un'ESPRESSIONE e il nome della variabile può coincidere con una KEYWORD
     * (es. `path`, che nel lexer è GMB_TOK_Path): prima il controllo accettava solo
     * StringLiteral/Identifier, quindi `OPEN path FOR WRITE AS #1` restava senza file_path_expr
     * e l'emitter moriva con "OPEN: manca il path del file" (trovato da MiniTests/io/io_misc.bas,
     * gamba C99Native; il mondo C# gestisce già questo caso). */
    if (f && f->kind != GMB_TOK_Newline) {
        if (f->kind == GMB_TOK_StringLiteral) {
            s->file_path = (char*)malloc((size_t)f->length + 1);
            memcpy(s->file_path, f->lexeme, (size_t)f->length); s->file_path[f->length] = 0;
        }
        s->file_path_expr = gmb_p_parse_expr(p);
    }
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "For") || gmb_p_check(p, GMB_TOK_For)) {
        gmb_p_adv(p);
        const gmb_token_t* m = gmb_p_cur(p);
        if (m) {
            s->file_mode = gmb_p_tok_strdup(m); s->file_mode_tk = (int)m->kind;
            gmb_p_adv(p);
        }
    }
    /* AS #n */
    while (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) {
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
            gmb_p_adv(p);
            gmb_p_match(p, GMB_TOK_Hash); /* #n */
            s->file_num = gmb_p_parse_expr(p);
            /* Clausole dopo AS #n (parità Parser.Stmt.Files.cs): LEN = n (lunghezza record di un file
             * RANDOM) → file_pos; le altre (ACCESS, LOCK, ENCODING, TYPE JSON) restano ignorate. */
            while (gmb_p_cur(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
                if (gmb_p_check(p, GMB_TOK_Len)) {
                    gmb_p_adv(p);
                    gmb_p_match(p, GMB_TOK_Equal);
                    s->file_pos = gmb_p_parse_expr(p);
                } else break;
            }
            break;
        }
        gmb_p_adv(p);
    }
    return s;
}

/* CLOSE [#n, ...] */
gmb_stmt_t* gmb_p_parse_close(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CLOSE, line);
    if (gmb_p_match(p, GMB_TOK_Hash) || gmb_p_cur(p)) {
        if (s->file_num == NULL && gmb_p_cur(p) && !gmb_p_check(p, GMB_TOK_Newline)) {
            if (gmb_p_check(p, GMB_TOK_Hash)) gmb_p_adv(p);
            s->file_num = gmb_p_parse_expr(p);
        }
    }
    return s;
}

/* SEEK n, pos — comando legacy: numero di file (`OPEN ... AS #n`) e posizione 1-based. */
gmb_stmt_t* gmb_p_parse_seek(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SEEK, line);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) s->file_num = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_Newline)) s->file_pos = gmb_p_parse_expr(p);
    return s;
}

/* PRINT #n, <expr>, ... */
gmb_stmt_t* gmb_p_parse_printfile(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_PRINTFILE, line);
    if (gmb_p_match(p, GMB_TOK_Hash)) {
        s->file_num = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
    }
    int cap = 0;
    if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) return s;
    for (;;) {
        if (s->nprint >= cap) { cap = cap ? cap * 2 : 4; s->print_args = (gmb_expr_t**)realloc(s->print_args, (size_t)cap * sizeof(gmb_expr_t*)); }
        s->print_args[s->nprint++] = gmb_p_parse_expr(p);
        /* `;` accettato come `,` (parità gmb_p_parse_print): senza, `PRINT #n, a; b` lasciava
         * `; b` non consumato → "statement non riconosciuto (riga N)". */
        if (!gmb_p_match(p, GMB_TOK_Comma) && !gmb_p_match(p, GMB_TOK_Semicolon)) break;
        if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) break;
    }
    return s;
}

/* PUT [#]n, [record], source — speculare a Parser.Stmt.Files.cs::ParseIoPutStatement:
 * il '#' è opzionale, il numero di record è opzionale (forma `PUT #f, , data` = record
 * assente, scrittura sequenziale). */
gmb_stmt_t* gmb_p_parse_putfile(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_PUT, line);
    gmb_p_match(p, GMB_TOK_Hash);
    s->file_num = gmb_p_parse_expr(p);
    gmb_p_match(p, GMB_TOK_Comma);
    if (!gmb_p_check(p, GMB_TOK_Comma)) s->file_pos = gmb_p_parse_expr(p);
    gmb_p_match(p, GMB_TOK_Comma);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) s->send_value = gmb_p_parse_expr(p);
    return s;
}

/* GET [#]n, [record], destinazione — speculare a Parser.Stmt.Files.cs::ParseIoGetStatement.
 * Per un file RANDOM la destinazione è tipicamente un UDT (record letto nei suoi campi). Il '#'
 * è opzionale ma il dispatch (gmb_parser_statements.c) instrada qui solo `GET #`: `GET var$`
 * resta GETKEY. file_num=handle, file_pos=record (opz.), target=nome della variabile. */
gmb_stmt_t* gmb_p_parse_getfile(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_GET, line);
    gmb_p_match(p, GMB_TOK_Hash);
    s->file_num = gmb_p_parse_expr(p);
    gmb_p_match(p, GMB_TOK_Comma);
    if (!gmb_p_check(p, GMB_TOK_Comma)) s->file_pos = gmb_p_parse_expr(p);
    gmb_p_match(p, GMB_TOK_Comma);
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) s->target = read_name(p);
    return s;
}

/* LOCK #n [, record] / UNLOCK #n [, record] — parità Parser.Stmt.Files.cs::ParseLockStatement
 * (gmb_file_lock/unlock). flag: 0=LOCK, 1=UNLOCK. file_num=handle, file_pos=record (opz.). */
gmb_stmt_t* gmb_p_parse_lockfile(gmb_parser_t* p, int line, int unlock)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_LOCK, line);
    s->flag = unlock;
    gmb_p_match(p, GMB_TOK_Hash);
    s->file_num = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Comma)) {
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Colon))
            s->file_pos = gmb_p_parse_expr(p);
    }
    return s;
}

/* INPUT #n, <var>, ... */
gmb_stmt_t* gmb_p_parse_inputfile(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_INPUTFILE, line);
    if (gmb_p_match(p, GMB_TOK_Hash)) {
        s->file_num = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
    }
    int cap = 0;
    for (;;) {
        const gmb_token_t* v = gmb_p_cur(p);
        if (v && v->kind != GMB_TOK_Newline && v->kind != GMB_TOK_Eof && v->kind != GMB_TOK_Colon && v->kind != GMB_TOK_Comma) {
            if (s->n_read >= cap) { cap = cap ? cap * 2 : 4; s->read_vars = (char**)realloc(s->read_vars, (size_t)cap * sizeof(char*)); }
            s->read_vars[s->n_read] = (char*)malloc((size_t)v->length + 1);
            memcpy(s->read_vars[s->n_read], v->lexeme, (size_t)v->length); s->read_vars[s->n_read][v->length] = 0;
            s->n_read++;
            gmb_p_adv(p);
        }
        if (!gmb_p_match(p, GMB_TOK_Comma)) break;
    }
    return s;
}

/* LINEINPUT #n, <var> */
gmb_stmt_t* gmb_p_parse_lineinput(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_LINEINPUT, line);
    if (gmb_p_match(p, GMB_TOK_Hash)) {
        s->file_num = gmb_p_parse_expr(p);
        gmb_p_match(p, GMB_TOK_Comma);
    }
    const gmb_token_t* v = gmb_p_cur(p);
    if (v && v->kind == GMB_TOK_Identifier) {
        s->read_vars = (char**)malloc(sizeof(char*));
        s->read_vars[0] = (char*)malloc((size_t)v->length + 1);
        memcpy(s->read_vars[0], v->lexeme, (size_t)v->length); s->read_vars[0][v->length] = 0;
        s->n_read = 1;
        gmb_p_adv(p);
    }
    return s;
}

/* FREAD handle [, count] INTO buffer — COMANDO (B): legge e SCRIVE nel buffer.
 * Desugara come il C# (Parser.Stmt.Files.cs): assegnazione `buffer = FREAD(handle[, count])`.
 * La forma FUNZIONE FREAD(handle [, count]) ritorna i dati e NON scrive (AGENTS §1/§3).
 * La call e' sintetizzata per NOME: nel mondo C il dispatch e' per nome (compilatore separato). */
gmb_stmt_t* gmb_p_parse_fread_into(gmb_parser_t* p, int line)
{
    /* args su HEAP: l'AST trattiene il puntatore oltre il ritorno (uno stack array
     * provocherebbe un use-after-return → abort del driver). */
    gmb_expr_t** args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
    int n = 0;
    (void)gmb_p_match(p, GMB_TOK_Hash);            /* '#' opzionale (legacy FREAD #1 ...) */
    args[n++] = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Comma) && n < 2) args[n++] = gmb_p_parse_expr(p);
    if (gmb_p_check(p, GMB_TOK_Into) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Into")) gmb_p_adv(p);
    const gmb_token_t* v = gmb_p_cur(p);
    if (!v || (v->kind != GMB_TOK_Identifier && v->kind != GMB_TOK_Var)) return NULL;
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ASSIGN, line);
    s->target = (char*)malloc((size_t)v->length + 1);
    memcpy(s->target, v->lexeme, (size_t)v->length); s->target[v->length] = 0;
    gmb_p_adv(p);
    s->value = gmb_expr_call("FREAD", args, n, line);
    return s;
}
