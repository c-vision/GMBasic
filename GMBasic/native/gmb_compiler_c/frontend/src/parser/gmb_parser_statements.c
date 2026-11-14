/* native/gmb_runtime_c/src/parser/gmb_parser_statements.c — speculare a Parser.Statements.cs */
#include "gmb_parser_internal.h"
#include "gmb_fn_lookup.h"
#include <stdlib.h>
#include <strings.h>
#include <string.h>

/* MATRIX OF (I1, …, In, V) — 2026-11-09. `p` punta al token MATRIX.
 * Consuma OF ( … ) e ritorna il rank (numero di tipi-asse). `value_tok` riceve
 * l'ultimo tipo (tipo valore). 0 se non è una MATRIX OF valida. */
static int gmb_parse_matrix_of(gmb_parser_t* p, const gmb_token_t** value_tok)
{
    if (value_tok) *value_tok = NULL;
    gmb_p_adv(p); /* MATRIX */
    if (!gmb_p_match(p, GMB_TOK_Of)) return 0;
    if (!gmb_p_match(p, GMB_TOK_LeftParen)) return 0;
    int rank = 0;
    const gmb_token_t* last = NULL;
    int depth = 0;
    while (!gmb_p_at_end(p)) {
        const gmb_token_t* t = gmb_p_cur(p);
        if (t->kind == GMB_TOK_RightParen && depth == 0) { gmb_p_adv(p); break; }
        if (t->kind == GMB_TOK_LeftParen) depth++;
        else if (t->kind == GMB_TOK_RightParen) depth--;
        else if (t->kind == GMB_TOK_Comma && depth == 0) rank++;
        if (t->kind != GMB_TOK_Comma && t->kind != GMB_TOK_Newline) last = t;
        gmb_p_adv(p);
    }
    if (!last) return 0;
    if (value_tok) *value_tok = last;
    return rank; /* numero di assi (= virgole) */
}

/* `ARRAY OF T` (mature type system, 2026-11-09): il tipo dichiarato è l'ELEMENTO T,
 * non un array annidato. `p` punta al token ARRAY; consuma ARRAY [OF <tipo>] e lascia
 * il cursore sul tipo elemento. Ritorna il token dell'elemento (NULL se assente/DOUBLE
 * implicito). Usato da DIM e parametri: rank/estensioni vengono dalle parentesi del
 * nome, mai da un tipo ARRAY OF ARRAY (che non esiste più). */
static const gmb_token_t* gmb_consume_array_of(gmb_parser_t* p)
{
    if (!p || !gmb_p_cur(p)) return NULL;
    const gmb_token_t* t = gmb_p_cur(p);
    if (!(t->kind == GMB_TOK_Array || gmb_p_tok_is_keyword(t, "Array"))) return NULL;
    gmb_p_adv(p); /* ARRAY */
    if (gmb_p_check(p, GMB_TOK_Of)) {
        gmb_p_adv(p); /* OF */
        const gmb_token_t* el = gmb_p_cur(p);
        if (el && el->lexeme) return el;
    }
    return NULL; /* ARRAY senza OF = ARRAY OF DOUBLE implicito */
}

static int gm_parser_prim_len(const char* w, int len) {
    static const char* pr[] = { "STRING","INTEGER","INT","LONG","DOUBLE","SINGLE","FLOAT","BOOLEAN","BOOL","BYTE","ANY", NULL };
    if (!w) return 1;
    for (int i = 0; pr[i]; i++)
        if ((int)strlen(pr[i]) == len && strncasecmp(w, pr[i], (size_t)len) == 0) return 1;
    return 0;
}
/* tipi COLLECTION riconosciuti per TokenKind (dk0 = at->kind del tipo in DIM) */
static bool dim_coll_kind(gmb_tokenkind_t k) {
    switch (k) {
        case GMB_TOK_List: case GMB_TOK_Dict: case GMB_TOK_Queue: case GMB_TOK_Stack:
        case GMB_TOK_HashSet: case GMB_TOK_Sorted: case GMB_TOK_LinkedList:
        case GMB_TOK_DoublyLinkedList: case GMB_TOK_Circular: case GMB_TOK_Tree:
        case GMB_TOK_AVLTree: case GMB_TOK_RedBlack: case GMB_TOK_BPlusTree:
        case GMB_TOK_Trie: case GMB_TOK_Heap: case GMB_TOK_MaxHeap: case GMB_TOK_Bag:
        case GMB_TOK_BITree: case GMB_TOK_HashRing: case GMB_TOK_RingBuffer:
        case GMB_TOK_SparseSet: case GMB_TOK_Cache: case GMB_TOK_Graph:
        case GMB_TOK_Timeline:
            return true;
        default: return false;
    }
}
/* map: tipo arg voluto = 2 (chiave + valore) */
static bool dim_map_kind(gmb_tokenkind_t k) {
    return k == GMB_TOK_Dict || k == GMB_TOK_Sorted || k == GMB_TOK_Cache || k == GMB_TOK_Tree
        || k == GMB_TOK_AVLTree || k == GMB_TOK_RedBlack || k == GMB_TOK_BPlusTree;
}

/* Arg comando GRAPH: stringa letterale o identificatore nudo → stringa (parità
 * Parser.ParseGraphNameArg). Un identificatore con suffisso ($/tipo) resta variabile. */
static gmb_expr_t* gmb_p_graph_arg(gmb_parser_t* p, int line) {
    const gmb_token_t* tk = gmb_p_cur(p);
    if (tk && tk->kind == GMB_TOK_Identifier && tk->length > 0 && tk->lexeme[tk->length - 1] != '$'
        && !gmb_p_is_declared_tok(p, tk)) {
        char* nm = (char*)malloc((size_t)tk->length + 1);
        memcpy(nm, tk->lexeme, (size_t)tk->length); nm[tk->length] = 0;
        gmb_p_adv(p);
        gmb_expr_t* e = gmb_expr_str(nm, line);
        free(nm);
        return e;
    }
    /* PAROLA-CHIAVE usata come VALORE di modalità: `GRAPH RDD MEMORY` / `GRAPH RDD JSON`.
     * MEMORY e JSON sono keyword del lexer (GMB_TOK_Memory/GMB_TOK_Json), quindi non
     * cadevano nel ramo Identifier e `gmb_p_parse_expr` le trattava come VARIABILI →
     * il C99Native emetteva `static gmb_double_t MEMORY = 0;` e
     * `gmb_graph_rdd_set(gmb_double_to_string(MEMORY).data)` = modalità "0" invece di
     * "MEMORY"/"JSON" (bug reale, non solo warning). SQLITE funzionava perché non è keyword.
     * Qui il lexeme della keyword viene preso come stringa, come per gli identificatori. */
    if (tk && tk->kind == GMB_TOK_Memory) { gmb_p_adv(p); return gmb_expr_str("MEMORY", line); }
    if (tk && tk->kind == GMB_TOK_Json) { gmb_p_adv(p); return gmb_expr_str("JSON", line); }
    return gmb_p_parse_expr(p);
}

/* CONST <nome> [AS tipo] = <expr> — dichiarazione costante utente: il compilatore C
 * la registra in prepass e risolve i riferimenti (nessun bytecode a runtime). */
static gmb_stmt_t* gmb_p_parse_const(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CONST, line);
    const gmb_token_t* nm = gmb_p_cur(p);
    if (nm && nm->kind != GMB_TOK_Newline && nm->kind != GMB_TOK_Eof) { s->target = gmb_p_tok_strdup(nm); gmb_p_adv(p); }
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
        gmb_p_adv(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline
            && gmb_p_cur(p)->kind != GMB_TOK_Equal) gmb_p_adv(p);
    }
    if (gmb_p_match(p, GMB_TOK_Equal)) s->value = gmb_p_parse_expr(p);
    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
    return s;
}
#include <string.h>
#include <ctype.h>
#include <stdio.h>
static const gmb_tokenkind_t gmb_tk_dim_sync_toks[] = { GMB_TOK_Channel, GMB_TOK_Waitgroup, GMB_TOK_Mutex, GMB_TOK_RwMutex, GMB_TOK_Once, GMB_TOK_Cond, GMB_TOK_Atomic, GMB_TOK_Pool, GMB_TOK_SyncMap, GMB_TOK_ConcurrentMap, GMB_TOK_Context, GMB_TOK_HttpRequest, GMB_TOK_HttpResponse, GMB_TOK_WebSocket };
static int gmb_tk_dim_sync_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_tk_dim_sync_toks) / sizeof(gmb_tk_dim_sync_toks[0]); i++)
        if (gmb_tk_dim_sync_toks[i] == k) return 1;
    return 0;
}
static const gmb_tokenkind_t gmb_tk_rem_vari_toks[] = { GMB_TOK_Load, GMB_TOK_Replace, GMB_TOK_Use, GMB_TOK_Tcp, GMB_TOK_Ssl, GMB_TOK_Udp, GMB_TOK_Delete, GMB_TOK_SetOption, GMB_TOK_Recall, GMB_TOK_Shutdown, GMB_TOK_Tilemap, GMB_TOK_TilemapLoad, GMB_TOK_TilemapSet, GMB_TOK_TilemapOffset, GMB_TOK_TilemapScroll, GMB_TOK_TilemapDraw, GMB_TOK_Declare, GMB_TOK_Lib, GMB_TOK_Step, GMB_TOK_Write, GMB_TOK_WorkflowExecute, GMB_TOK_ConvDoc }; /* 2026-10-02: LINE/FONT/DRAW/CIRCLE/PSET/PRESET/PAINT/SCREENSET/PCOPY/PALETTE/VIEW/VIEWPORT ora reali (parita' emitter C#) */ /* 2026-09-21: usciti come reali — Watch/Unwatch, Cursor, CCLS/CLOCATE/CSAVESCREEN/CRESTORESCREEN/CPALETTE; i display puri restano errore esplicito */ /* 2026-10-08: GMB_TOK_Put RIMOSSO dalla lista — PUT # è ora supportato (parser gmb_p_parse_putfile + emitter st_put; sample HttpMultiPart, parità 4 backend) */
static int gmb_tk_rem_vari_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_tk_rem_vari_toks) / sizeof(gmb_tk_rem_vari_toks[0]); i++)
        if (gmb_tk_rem_vari_toks[i] == k) return 1;
    return 0;
}
gmb_stmt_t* gmb_p_parse_stmt(gmb_parser_t* p, bool* multiline);

/* consuma uno statement fino a fine riga, saltando le continuazioni '_' */
void gmb_p_consume_statement(gmb_parser_t* p)
{
    while (!gmb_p_at_end(p)) {
        const gmb_token_t* tt = gmb_p_cur(p);
        if (tt->kind == GMB_TOK_Newline) { gmb_p_adv(p); break; }
        if (tt->kind == GMB_TOK_Identifier && tt->length == 1 && tt->lexeme[0] == '_' && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Newline) {
            gmb_p_adv(p); gmb_p_adv(p);
            if (gmb_p_check(p, GMB_TOK_LineNumber)) gmb_p_adv(p);
            continue;   /* riga continuata */
        }
        gmb_p_adv(p);
    }
}

gmb_stmt_t* gmb_p_parse_print(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_PRINT, line);
    int cap = 0;
    /* Bug corretto (2026-08-31, audit lambda/closure): un PRINT con separatore finale
     * (`PRINT x; " ";`) dentro il corpo a blocco di una lambda `SUB(...) { ... }` deve
     * fermarsi anche su '}' (RightBrace), non solo su newline/colon/EOF — altrimenti,
     * dopo il ';' finale, il loop sotto tenta di parsare UN'ALTRA espressione a partire da
     * '}' (non un inizio di espressione valido), corrompendo il cursore del parser: il
     * '}' che avrebbe dovuto chiudere la lambda viene "mangiato" dentro il PRINT, e il
     * parser del blocco lambda (che aspetta quel '}') non lo trova mai — l'errore
     * risultante ("blocco non terminato") appare molte righe/token dopo, alla fine
     * dell'intero file, perché il parser continua a consumare token oltre il vero
     * confine del blocco (riprodotto isolando `AEVAL arr, SUB(x) { PRINT x; " "; }`:
     * stesso identico sintomo su VMC e C99, che condividono questo parser). */
    if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_check(p, GMB_TOK_Colon)
        || gmb_p_check(p, GMB_TOK_RightBrace) || gmb_p_check(p, GMB_TOK_Else)
        || gmb_p_check(p, GMB_TOK_ElseIf) || gmb_p_at_end(p)) return s;
    for (;;) {
        if (s->nprint >= cap) {
            cap = cap ? cap * 2 : 4;
            s->print_args = (gmb_expr_t**)realloc(s->print_args, (size_t)cap * sizeof(gmb_expr_t*));
            s->print_comma = (int*)realloc(s->print_comma, (size_t)cap * sizeof(int));
        }
        int idx = s->nprint;
        s->print_args[s->nprint++] = gmb_p_parse_expr(p);
        s->print_comma[idx] = 0;
        /* separatori: ',' (tab zone 14 colonne) e ';' (no spazio) — entrambi continuano la
         * lista; solo ',' richiede il padding (parity oracolo IlRuntime.PrintTabZone/
         * OP_PRINT_TAB_ZONE) — registrato su print_comma[idx], letto dall'emitter C99Native. */
        if (gmb_p_match(p, GMB_TOK_Comma)) s->print_comma[idx] = 1;
        else if (!gmb_p_match(p, GMB_TOK_Semicolon)) break;
        if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_check(p, GMB_TOK_Colon)
            || gmb_p_check(p, GMB_TOK_RightBrace) || gmb_p_check(p, GMB_TOK_Else)
            || gmb_p_check(p, GMB_TOK_ElseIf) || gmb_p_at_end(p)) {
            /* separatore finale: niente newline (parity PCodeVM), MA se era ',' il padding
             * di zona va comunque emesso (parity oracolo: la virgola diventa SEMPRE un
             * OP_PRINT_TAB_ZONE, anche in coda — è quella che allinea il PRIMO argomento
             * della PROSSIMA PRINT sulla stessa riga, es. `PRINT x,` dentro un ciclo). Solo
             * ';' finale non aggiunge nulla: print_comma[idx] resta quello già impostato
             * sopra (1 se ',', 0 se ';'). */
            s->print_hold = 1;
            break;
        }
    }
    return s;
}

void gmb_p_parse_block(gmb_parser_t* p, gmb_stmt_list_t* out, gmb_tokenkind_t stop_kw,
                        const char* stop_name, int start_line)
{
    for (;;) {
        gmb_p_skip_eol(p);
        if (gmb_p_at_end(p)) { gmb_p_set_err(p, "blocco non terminato", start_line); return; }
    const gmb_token_t* t = gmb_p_cur(p);

        /* NEXT J, I multiplo: il FOR corrente e' stato chiuso da un NEXT annidato */
        if (stop_kw == GMB_TOK_Next && p->pending_for_ends > 0) { p->pending_for_ends--; return; }
        /* stop: nuova keyword di chiusura (incl. END <word>) o END (terminatore universale) */
        if (t->kind == stop_kw || (stop_name && gmb_p_tok_is_keyword(t, stop_name))) { return; }
        if (stop_kw == GMB_TOK_Else && t->kind == GMB_TOK_ElseIf) return;   /* ELSEIF in un blocco IF */
        if (t->kind == GMB_TOK_End && gmb_p_peek(p, 1)) {
            /* END <keyword> = terminatore composto SOLO se combacia col blocco corrente;
             * END nudo = statement STOP (non termina il blocco) */
            gmb_tokenkind_t nk = gmb_p_peek(p, 1)->kind;
            bool end_matches = false;
            if (stop_kw == GMB_TOK_EndIf && nk == GMB_TOK_If) end_matches = true;
            else if (stop_kw == GMB_TOK_Else && nk == GMB_TOK_If) end_matches = true;
            else if (stop_kw == GMB_TOK_EndFunction && nk == GMB_TOK_Function) end_matches = true;
            else if (stop_kw == GMB_TOK_Case && nk == GMB_TOK_Select) end_matches = true;
            else if (stop_kw == GMB_TOK_Catch && nk == GMB_TOK_Try) end_matches = true;
            else if (stop_kw == GMB_TOK_EndTry && nk == GMB_TOK_Try) end_matches = true;
            else if (stop_kw == GMB_TOK_EndGet && nk == GMB_TOK_Get) end_matches = true;
            else if (stop_kw == GMB_TOK_EndSet && nk == GMB_TOK_Set) end_matches = true;
            else if (stop_kw == GMB_TOK_EndProperty && (nk == GMB_TOK_Property || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Property"))) end_matches = true;
            else if (stop_kw == GMB_TOK_EndUsing && (nk == GMB_TOK_Using || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Using"))) end_matches = true;
            else if (stop_kw == GMB_TOK_EndGenerator && (nk == GMB_TOK_Generator || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Generator"))) end_matches = true;
            else if (stop_kw == GMB_TOK_EndTick && nk == GMB_TOK_Tick) end_matches = true;   /* END TICK */
            else if (stop_kw == GMB_TOK_End && (nk == GMB_TOK_AiPatch || gmb_p_tok_is_soft_keyword(gmb_p_peek(p, 1), "PATCH"))) end_matches = true;   /* §4.10 END PATCH */
            else if ((stop_kw == GMB_TOK_Wend || stop_kw == GMB_TOK_Loop)
                     && (nk == GMB_TOK_Newline || nk == GMB_TOK_Colon || nk == GMB_TOK_Eof)) end_matches = true;  /* WHILE/DO ... END */
            if (end_matches) return;
        }
        if (t->kind == GMB_TOK_Finally || gmb_p_tok_is_keyword(t, "Finally")) return;
        if (stop_kw == GMB_TOK_EndIf && gmb_p_at_end_compound(p, GMB_TOK_If)) { return; }
        if (stop_kw == GMB_TOK_EndFunction && gmb_p_at_end_compound(p, GMB_TOK_Function)) { return; }
        bool ml = false;
        gmb_stmt_t* s = gmb_p_parse_stmt(p, &ml);
        if (s) gmb_p_stmt_list_add(out, s);
        gmb_p_drain_queued(p, out);
        if (p->error) return;
        if (!ml) {
            /* ':' separa statement sulla stessa riga (es. PRINT ... : GOTO 150) */
            if (gmb_p_check(p, GMB_TOK_Colon)) { gmb_p_adv(p); continue; }
            gmb_p_skip_eol(p);
        }
    }
}

static gmb_stmt_t* gmb_p_parse_stmt_impl(gmb_parser_t* p, bool* multiline);

/* Legge la firma `(p1 AS T1, p2 AS T2, ...)` di un tipo funzione (DIM v AS FUNCTION(...) /
 * parametro `x AS FUNCTION(...)`): ritorna l'ARIETÀ e setta in *strmask i bit dei parametri
 * AS STRING. Consuma fino alla ')' INCLUSa (se presente). *intmask (2026-09-29, Callback FFI
 * — vedi AiWiki Bugs/(open) - FFI-Estensioni-Possibili-BYREF-Struct-Callback.md §3bis), se
 * non NULL, riceve i bit dei parametri di tipo INTERO (LONG/INTEGER/QUAD/BYTE/SBYTE/UQUAD/
 * ULONG/UINTEGER) invece di DOUBLE/SINGLE — serve SOLO al marshalling FFI tipizzato (la firma
 * nativa del trampolino deve avere ESATTAMENTE gmb_int_t/gmb_double_t per campo, non il
 * contratto generico "sempre gmb_double_t" di gmb_fn_t usato altrove per i delegate BASIC-a-
 * BASIC). */
static int gmb_p_fn_sig(gmb_parser_t* p, int* strmask, int* intmask, int* arrmask) {
    int seg = 0, mask = 0, imask = 0, amask = 0;
    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
        if (!gmb_p_check(p, GMB_TOK_RightParen)) {
            int depth = 0;   /* parentesi annidate: `v() AS DOUBLE` */
            for (;;) {
                int seg_str = 0, seg_int = 0, seg_arr = 0;
                while (!gmb_p_at_end(p)) {
                    const gmb_token_t* tk = gmb_p_cur(p);
                    if (depth == 0 && (gmb_p_check(p, GMB_TOK_Comma) || gmb_p_check(p, GMB_TOK_RightParen))) break;
                    if (tk && tk->kind == GMB_TOK_LeftParen) {
                        if (gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_RightParen) seg_arr = 1;
                        depth++; gmb_p_adv(p); continue;
                    }
                    if (tk && tk->kind == GMB_TOK_RightParen) { if (depth > 0) depth--; gmb_p_adv(p); continue; }
                    if (tk && tk->kind == GMB_TOK_StringType) seg_str = 1;
                    if (tk && (tk->kind == GMB_TOK_Long || tk->kind == GMB_TOK_Integer
                        || tk->kind == GMB_TOK_Quad || tk->kind == GMB_TOK_Byte
                        || tk->kind == GMB_TOK_SByte || tk->kind == GMB_TOK_UQuad
                        || tk->kind == GMB_TOK_ULong || tk->kind == GMB_TOK_UInteger))
                        seg_int = 1;
                    gmb_p_adv(p);
                }
                if (seg_str) mask |= (1 << seg);
                if (seg_int) imask |= (1 << seg);
                if (seg_arr) amask |= (1 << seg);
                seg++;
                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
            }
        }
        gmb_p_match(p, GMB_TOK_RightParen);
    }
    if (strmask) *strmask = mask;
    if (intmask) *intmask = imask;
    if (arrmask) *arrmask = amask;
    return seg;
}

gmb_stmt_t* gmb_p_parse_stmt(gmb_parser_t* p, bool* multiline) {
    int bas = p->bas_fresh ? p->last_bas_line : 0;
    p->bas_fresh = 0;
    gmb_stmt_t* s = gmb_p_parse_stmt_impl(p, multiline);
    if (s && bas > 0 && !s->bas_line) s->bas_line = bas;
    /* DIM: registra il nome dichiarato (risoluzione nome-vs-variabile negli argomenti
     * dei comandi AI, parità Parser._declaredVariableNames, bug P4). */
    if (s && s->kind == GMB_STMT_DIM && s->dim_var) gmb_p_declare_name(p, s->dim_var);
    /* Costante stringa nota (DIM <name> AS STRING = "literal", scalare): abilita
     * l'espansione compile-time di &(<name>)/EVAL(<name>) (bug macro-variabile-block).
     * Vale anche per CONCATENAZIONI di costanti (`DIM s = pre + "x| ... }"`): il fold
     * rende la macro espandibile a compile-time come il letterale. */
    if (s && s->kind == GMB_STMT_DIM && s->dim_var && s->dim_init && !s->array_size) {
        if (s->dim_init->kind == GMB_EXPR_STR)
            gmb_p_cstr_set(p, s->dim_var, s->dim_init->str ? s->dim_init->str : "");
        else {
            char folded[1024];
            if (gmb_p_const_str_fold(p, s->dim_init, folded, sizeof(folded)))
                gmb_p_cstr_set(p, s->dim_var, folded);
        }
    }
    /* Assegnazione: flow-sensitive — se il RHS è una stringa COSTANTE (letterale o
     * concatenazione di costanti) il nome resta una costante di compilazione nota;
     * altrimenti non è più costante. Copre `DIM s AS STRING` + `s = "..."` + `&(s)`. */
    if (s && (s->kind == GMB_STMT_LET || s->kind == GMB_STMT_ASSIGN) && s->target) {
        char folded[1024];
        if (gmb_p_const_str_fold(p, s->value, folded, sizeof(folded)))
            gmb_p_cstr_set(p, s->target, folded);
        else
            gmb_p_cstr_clear(p, s->target);
    }
    /* Fine FUNCTION/SUB: le costanti locali muoiono con lo scope di procedura. */
    if (s && s->kind == GMB_STMT_FUNCTION) gmb_p_cstr_reset(p);
    return s;
}

/* ── AI DECIDE batch (JEV/SystemOne): parse + desugar in N ASSIGN sequenziali ─────────────
 * Forma canonica: AI DECIDE a fine riga, una domanda per riga (`nome AS kind`), poi
 * FROM <stato>, USING <provider> opzionale, END DECIDE. Stessa semantica della forma
 * singola (il builtin DECIDE viene emesso una volta per domanda); la fusione prefill/KV
 * (DECISION_GROUP) è l'ottimizzazione successiva. */
static int dc_tok_is(const gmb_token_t* t, const char* kw) {
    return t && gmb_p_tok_is_soft_keyword(t, kw);
}

/* Legge un kind di domanda e ritorna la stringa del builtin (heap) o NULL su errore. */
static char* dc_parse_batch_kind(gmb_parser_t* p, int line) {
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->kind != GMB_TOK_Identifier) {
        gmb_p_set_err(p, "AI DECIDE batch: atteso NOUL/SCORE/CHOICE dopo AS", line);
        return NULL;
    }
    if (dc_tok_is(t, "NOUL")) { gmb_p_adv(p); return strdup("NOUL"); }
    if (dc_tok_is(t, "SCORE")) {
        gmb_p_adv(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Number) {
            const gmb_token_t* lo = gmb_p_cur(p); gmb_p_adv(p);
            if (!(gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_To)) {
                gmb_p_set_err(p, "SCORE: atteso TO dopo il minimo", line); return NULL;
            }
            gmb_p_adv(p);
            const gmb_token_t* hi = gmb_p_cur(p);
            if (!hi || hi->kind != GMB_TOK_Number) {
                gmb_p_set_err(p, "SCORE: atteso il massimo dopo TO", line); return NULL;
            }
            gmb_p_adv(p);
            double lv = atof(lo->lexeme), hv = atof(hi->lexeme);
            if (lv != 0.0 || (hv != 5.0 && hv != 10.0)) {
                gmb_p_set_err(p, "AI DECIDE batch: SCORE supporta le rubriche 0 TO 5 e 0 TO 10", line);
                return NULL;
            }
            return strdup(hv == 10.0 ? "SCORE:0:10" : "SCORE");
        }
        return strdup("SCORE");
    }
    if (dc_tok_is(t, "CHOICE")) {
        gmb_p_adv(p);
        char buf[1024]; size_t bl = 0;
        bl += (size_t)snprintf(buf + bl, sizeof buf - bl, "CHOICE");
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_LeftParen) {
            gmb_p_adv(p);
            int first = 1;
            if (!(gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_RightParen)) {
                for (;;) {
                    gmb_expr_t* e = gmb_p_parse_expr(p);
                    if (!e || e->kind != GMB_EXPR_STR || !e->str) {
                        gmb_p_set_err(p, "CHOICE: attese etichette stringa", line); return NULL;
                    }
                    size_t el = strlen(e->str);
                    if (bl + el + 2 >= sizeof buf) {
                        gmb_p_set_err(p, "CHOICE: etichette troppo lunghe", line); return NULL;
                    }
                    buf[bl++] = first ? ':' : '|';
                    memcpy(buf + bl, e->str, el); bl += el; buf[bl] = 0;
                    first = 0;
                    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) { gmb_p_adv(p); continue; }
                    break;
                }
            }
            if (!(gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_RightParen)) {
                gmb_p_set_err(p, "CHOICE: attesa ')'", line); return NULL;
            }
            gmb_p_adv(p);
        }
        return strdup(buf);
    }
    gmb_p_set_err(p, "AI DECIDE batch: kind non valido (NOUL/SCORE/CHOICE)", line);
    return NULL;
}

/* Clausola FALLBACK "p1", "p2", … (nomi stringa) → CSV `p1|p2` heap. *present=0 se assente. */
static char* dc_parse_fallback_csv(gmb_parser_t* p, int line, int* present) {
    *present = 0;
    if (!dc_tok_is(gmb_p_cur(p), "FALLBACK")) return NULL;
    *present = 1;
    gmb_p_adv(p);
    char buf[1024]; size_t bl = 0; buf[0] = 0; int first = 1;
    for (;;) {
        gmb_expr_t* e = gmb_p_parse_expr(p);
        if (!e || e->kind != GMB_EXPR_STR || !e->str) {
            gmb_p_set_err(p, "FALLBACK: attesi nomi provider stringa", line);
            return NULL;
        }
        size_t el = strlen(e->str);
        if (bl + el + (first ? 0 : 1) + 1 >= sizeof buf) {
            gmb_p_set_err(p, "FALLBACK: lista troppo lunga", line);
            return NULL;
        }
        if (!first) buf[bl++] = '|';
        memcpy(buf + bl, e->str, el); bl += el; buf[bl] = 0; first = 0;
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) { gmb_p_adv(p); continue; }
        break;
    }
    return strdup(buf);
}

/* Array JSON di stringhe (le label CHOICE possono contenere '|': niente CSV). */
static char* dc_json_array(char** items, int n) {
    size_t cap = 64, off = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    #define DC_ARR_NEED(k) do { if (off + (k) + 1 > cap) { while (off + (k) + 1 > cap) cap *= 2;         char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; } } while (0)
    DC_ARR_NEED(1); out[off++] = '[';
    for (int i = 0; i < n; i++) {
        if (i) { DC_ARR_NEED(1); out[off++] = ','; }
        DC_ARR_NEED(1); out[off++] = '"';
        for (const char* p = items[i] ? items[i] : ""; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { DC_ARR_NEED(2); out[off++] = '\\'; out[off++] = (char)c; }
            else if (c == '\n') { DC_ARR_NEED(2); out[off++] = '\\'; out[off++] = 'n'; }
            else if (c == '\r') { DC_ARR_NEED(2); out[off++] = '\\'; out[off++] = 'r'; }
            else if (c == '\t') { DC_ARR_NEED(2); out[off++] = '\\'; out[off++] = 't'; }
            else if (c >= 0x20) { DC_ARR_NEED(1); out[off++] = (char)c; }
        }
        DC_ARR_NEED(1); out[off++] = '"';
    }
    DC_ARR_NEED(1); out[off++] = ']';
    out[off] = 0;
    #undef DC_ARR_NEED
    return out;
}

#define DC_BATCH_MAX 16

static gmb_stmt_t* dc_parse_batch(gmb_parser_t* p, int line) {
    char* names[DC_BATCH_MAX];
    char* kinds[DC_BATCH_MAX];
    int n = 0;
    gmb_p_skip_eol(p);
    while (!(gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_From)) {
        const gmb_token_t* nt = gmb_p_cur(p);
        if (!nt || nt->kind == GMB_TOK_Eof || nt->kind == GMB_TOK_End) {
            gmb_p_set_err(p, "AI DECIDE batch: manca FROM <stato> / END DECIDE", line); return NULL;
        }
        if (n >= DC_BATCH_MAX) {
            gmb_p_set_err(p, "AI DECIDE batch: massimo 16 domande", line); return NULL;
        }
        /* Nome PERMISSIVO (parità ParseTarget C#): anche keyword come ROUTE/PRIORITY sono
         * nomi validi; l'AS al token successivo disambigua. */
        const gmb_token_t* nx = gmb_p_peek(p, 1);
        if (!(nx && (nx->kind == GMB_TOK_As || dc_tok_is(nx, "AS")))) {
            gmb_p_set_err(p, "AI DECIDE batch: atteso <nome> AS <kind>", line); return NULL;
        }
        char* nm = gmb_p_tok_strdup(nt);
        gmb_p_adv(p);   /* nome */
        gmb_p_adv(p);   /* AS */
        char* kd = dc_parse_batch_kind(p, line);
        if (!kd) { free(nm); return NULL; }
        names[n] = nm; kinds[n] = kd; n++;
        gmb_p_skip_eol(p);
    }
    if (n == 0) { gmb_p_set_err(p, "AI DECIDE batch: nessuna domanda", line); return NULL; }
    gmb_p_adv(p);   /* FROM */
    gmb_expr_t* state = gmb_p_parse_expr(p);
    if (!state) { gmb_p_set_err(p, "AI DECIDE batch: stato mancante", line); return NULL; }
    gmb_p_skip_eol(p);
    gmb_expr_t* provider = NULL;
    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Using) {
        gmb_p_adv(p);
        provider = gmb_p_parse_expr(p);
        if (!provider) { gmb_p_set_err(p, "AI DECIDE batch: provider mancante dopo USING", line); return NULL; }
        gmb_p_skip_eol(p);
    }
    int fb_present = 0;
    char* fb_csv = dc_parse_fallback_csv(p, line, &fb_present);
    if (fb_present && !fb_csv) return NULL;
    gmb_expr_t* fb_expr = NULL;
    if (fb_present) {
        if (!provider) {
            char* sep = strchr(fb_csv, '|');
            if (sep) *sep = 0;
            provider = gmb_expr_str(fb_csv, line);
            if (sep) { char* rest = strdup(sep + 1); free(fb_csv); fb_csv = rest; }
            else { free(fb_csv); fb_csv = NULL; }
        }
        if (fb_csv && *fb_csv) fb_expr = gmb_expr_str(fb_csv, line);
        free(fb_csv);
        gmb_p_skip_eol(p);
    }
    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Into) {
        gmb_p_set_err(p, "AI DECIDE batch: INTO non supportato (i nomi delle domande sono le destinazioni)", line);
        return NULL;
    }
    if (!(gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_End)) {
        gmb_p_set_err(p, "AI DECIDE batch: atteso END DECIDE", line); return NULL;
    }
    gmb_p_adv(p);
    if (!dc_tok_is(gmb_p_cur(p), "DECIDE")) {
        gmb_p_set_err(p, "AI DECIDE batch: atteso DECIDE dopo END", line); return NULL;
    }
    gmb_p_adv(p);

    /* Una sola entry point per l'intero batch; ogni destinazione estrae la propria
     * stringa dal risultato (JSON.EXTRACT) — in C l'assegnazione a DECISION auto-parsa. */
    char* names_json = dc_json_array(names, n);
    char* kinds_json = dc_json_array(kinds, n);
    if (!names_json || !kinds_json) {
        free(names_json); free(kinds_json);
        gmb_p_set_err(p, "AI DECIDE batch: memoria", line);
        return NULL;
    }
    gmb_expr_t** gargs = (gmb_expr_t**)malloc(5 * sizeof(gmb_expr_t*));
    gargs[0] = state;
    gargs[1] = gmb_expr_str(names_json, line);
    gargs[2] = gmb_expr_str(kinds_json, line);
    gargs[3] = provider ? provider : gmb_expr_str("", line);
    gargs[4] = fb_expr ? fb_expr : gmb_expr_str("", line);
    free(names_json); free(kinds_json);
    gmb_expr_t* group = gmb_expr_call_typed("DECIDE", GMB_TOK_AiDecide, gargs, 5, line);

    gmb_stmt_t* first = NULL;
    for (int i = 0; i < n; i++) {
        gmb_expr_t** eargs = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        eargs[0] = (i == 0) ? group : gmb_expr_clone(group);
        eargs[1] = gmb_expr_str(names[i], line);
        gmb_expr_t* extract = gmb_expr_call_typed("EXTRACT", GMB_TOK_Extract, eargs, 2, line);
        gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
        a->target = names[i];
        a->value = extract;
        if (!first) first = a; else gmb_p_queue_stmt(p, a);
    }
    return first;
}

static gmb_stmt_t* gmb_p_parse_stmt_impl(gmb_parser_t* p, bool* multiline)
{
    *multiline = false;
    gmb_p_skip_eol(p);
    const gmb_token_t* t = gmb_p_cur(p);
    while (t && t->kind == GMB_TOK_LineNumber) { gmb_p_adv(p); gmb_p_skip_eol(p); t = gmb_p_cur(p); }
    if (!t) return NULL;
    /* PUBLIC/PRIVATE come prefisso generico di statement (es. "PUBLIC CONST X = 1",
     * "PUBLIC FUNCTION ..." dentro NAMESPACE): salta il prefisso e parsa il resto. */
    if ((t->kind == GMB_TOK_Public || t->kind == GMB_TOK_Private) && gmb_p_peek(p, 1)) {
        int is_pub = (t->kind == GMB_TOK_Public);
        gmb_p_adv(p);
        gmb_stmt_t* ps = gmb_p_parse_stmt(p, multiline);
        if (is_pub && ps && ps->kind == GMB_STMT_FUNCTION) ps->is_public = 1;
        return ps;
    }
    int line = t->line;
    if (t->kind == GMB_TOK_Newline || t->kind == GMB_TOK_Colon || t->kind == GMB_TOK_Eof) {
        /* solo etichetta di riga (es. 'IF a<b THEN 999' o riga vuota) — no-op */
        if (t->kind == GMB_TOK_Colon) gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    if (t->kind == GMB_TOK_Number) {
        /* riferimento a numero di riga come statement (es. IF a<b THEN 220) — label */
        gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }

    /* `{ |x| … }(args)` come STATEMENT: invocazione diretta di un literal lambda.
     * Il parser delle espressioni sa costruire la CALL con callee (gmb_p_parse_power);
     * qui si ammette un'espressione che INIZIA con `{` (es. `{ |d| n = n + d }(1)`). */
    if (t->kind == GMB_TOK_LeftBrace) {
        gmb_expr_t* lx = gmb_p_parse_expr(p);
        if (lx) {
            gmb_stmt_t* ls = gmb_stmt_new(GMB_STMT_EXPR, line);
            ls->expr = lx;
            return ls;
        }
    }

    if (t->kind == GMB_TOK_Print && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Hash) { gmb_p_adv(p); return gmb_p_parse_printfile(p, line); }
    if (t->kind == GMB_TOK_Input && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Hash) { gmb_p_adv(p); return gmb_p_parse_inputfile(p, line); }
    if (t->kind == GMB_TOK_LineInput && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Hash) { gmb_p_adv(p); return gmb_p_parse_lineinput(p, line); }
    if (t->kind == GMB_TOK_Line && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Input
        && gmb_p_peek(p, 2) && gmb_p_peek(p, 2)->kind == GMB_TOK_Hash) { gmb_p_adv(p); gmb_p_adv(p); return gmb_p_parse_lineinput(p, line); }
    /* LINE INPUT console (senza #): sintassi a due parole, una variabile, prompt opzionale —
     * speculare a Parser.Stmt.Console.cs ParseLineInputStatement. */
    if (t->kind == GMB_TOK_Line && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Input) {
        gmb_p_adv(p); gmb_p_adv(p); return gmb_p_parse_lineinput_console(p, line);
    }
    if (gmb_p_tok_is_soft_keyword(t, "BUTTON"))
        return gmb_p_parse_button(p, line);
    /* WINDOW … (THEME/DESKTOP/FRAME/SHADOW/DEFINE/SET TITLE/azioni): parser dedicato
     * già presente e speculare al C# (Parser.Stmt.Window.cs); mancava solo il dispatch,
     * quindi su VM/C `WINDOW THEME "x"` dava "CONTROL non riconosciuto". */
    if (gmb_p_tok_is_soft_keyword(t, "WINDOW"))
        return gmb_p_parse_window(p, line);
    if (gmb_p_tok_is_soft_keyword(t, "PANEL"))
        return gmb_p_parse_panel(p, line);
    /* Controlli PIXEL (widget grafici) non-keyword: CHECK/RADIO/TRACK/DROPDOWN/PROGRESS
     * x, y, ..., INTO var (valore in double* del programma). */
    /* CONTROL <SUB> ... : metodi a runtime per nome (CONTROL SET/GET/GETS/MOVE/FOCUS/ZORDER). */
    if (gmb_p_tok_is_soft_keyword(t, "CONTROL"))
        return gmb_p_parse_control_method(p, line);
    /* FIRE EVENT <handle>, "<EVENTO>" — comando a due token (iniezione evento controllo). */
    if (gmb_p_tok_is_soft_keyword(t, "FIRE"))
        return gmb_p_parse_fire(p, line);
    /* CONTROLS CHART (keyword CHART non seguito da un sotto-verbo): controllo a celle.
     * CHART+PLOT/SHOW/SAVE/CLEAR/CLOSE/FINANCIAL/SET/GET/GETS/DRAW resta comando (ramo gfx);
     * altrimenti (coordinate) è il controllo `CONTROLS CHART x,y,w,h[,"csv"]`. */
    if (t->kind == GMB_TOK_Chart) {
        const gmb_token_t* cc = gmb_p_peek(p, 1);
        bool cc_sub = cc && (cc->kind == GMB_TOK_Plot || cc->kind == GMB_TOK_Show || cc->kind == GMB_TOK_Save ||
                             cc->kind == GMB_TOK_Clear || cc->kind == GMB_TOK_Close || cc->kind == GMB_TOK_Financial ||
                             cc->kind == GMB_TOK_Set || cc->kind == GMB_TOK_Get || cc->kind == GMB_TOK_Draw ||
                             gmb_p_tok_is_soft_keyword(cc, "GETS"));
        if (!cc_sub) return gmb_p_parse_control(p, line);
    }
    /* CANVAS: con un sottocomando è il comando offscreen (ramo gfx); con coordinate è il
     * controllo `CONTROLS CANVAS x,y,w,h [NAME …]` — stessa disambiguazione di CHART. */
    if (t->kind == GMB_TOK_Canvas) {
        const gmb_token_t* cc = gmb_p_peek(p, 1);
        bool cc_sub = cc && (gmb_p_tok_is_soft_keyword(cc, "NEW") || gmb_p_tok_is_soft_keyword(cc, "USE") ||
                             gmb_p_tok_is_soft_keyword(cc, "END") || gmb_p_tok_is_soft_keyword(cc, "CLEAR") ||
                             gmb_p_tok_is_soft_keyword(cc, "SET") || gmb_p_tok_is_soft_keyword(cc, "GET") ||
                             gmb_p_tok_is_soft_keyword(cc, "BLIT") || gmb_p_tok_is_soft_keyword(cc, "SAVE") ||
                             gmb_p_tok_is_soft_keyword(cc, "LOAD") || gmb_p_tok_is_soft_keyword(cc, "FREE") ||
                             gmb_p_tok_is_soft_keyword(cc, "WIDTH") || gmb_p_tok_is_soft_keyword(cc, "HEIGHT"));
        if (!cc_sub) return gmb_p_parse_control(p, line);
    }
    if (t->kind == GMB_TOK_Identifier && (
            gmb_p_tok_is_soft_keyword(t, "CHECK") || gmb_p_tok_is_soft_keyword(t, "RADIO") || gmb_p_tok_is_soft_keyword(t, "TRACK") || gmb_p_tok_is_soft_keyword(t, "IMAGE") ||
            gmb_p_tok_is_soft_keyword(t, "CANVAS") || gmb_p_tok_is_soft_keyword(t, "TILING") ||
            gmb_p_tok_is_soft_keyword(t, "DROPDOWN") || gmb_p_tok_is_soft_keyword(t, "PROGRESS") || gmb_p_tok_is_soft_keyword(t, "RICHTEXT") ||
            gmb_p_tok_is_soft_keyword(t, "TEXTBOX") || gmb_p_tok_is_soft_keyword(t, "HALFBLOCK") ||
            gmb_p_tok_is_soft_keyword(t, "IMAGEBUTTON") || gmb_p_tok_is_soft_keyword(t, "CANDLESTICK") ||
            gmb_p_tok_is_soft_keyword(t, "FUNCTIONKEYBAR") || gmb_p_tok_is_soft_keyword(t, "PANEL")))
        return gmb_p_parse_control(p, line);
    if (t->kind == GMB_TOK_CheckBox || t->kind == GMB_TOK_ListBox || t->kind == GMB_TOK_ComboBox
        || t->kind == GMB_TOK_Menu || t->kind == GMB_TOK_RadioGroup || t->kind == GMB_TOK_Toggle
        || t->kind == GMB_TOK_Qr
        || t->kind == GMB_TOK_Slider || t->kind == GMB_TOK_NumericUpDown || t->kind == GMB_TOK_StarRating || t->kind == GMB_TOK_ScrollBar || t->kind == GMB_TOK_ScrollView         || t->kind == GMB_TOK_LabelControl         || t->kind == GMB_TOK_DataList         || t->kind == GMB_TOK_PropertyGrid         || t->kind == GMB_TOK_LogView         || t->kind == GMB_TOK_HexView         || t->kind == GMB_TOK_BigText         || t->kind == GMB_TOK_CodeArea         || t->kind == GMB_TOK_ReorderListBox         || t->kind == GMB_TOK_AlertBox         || t->kind == GMB_TOK_ConfirmationBox || t->kind == GMB_TOK_FileExplorer || t->kind == GMB_TOK_Dashboard || t->kind == GMB_TOK_Tab || t->kind == GMB_TOK_Grid
        || t->kind == GMB_TOK_Splitter || t->kind == GMB_TOK_ContextMenu
        || t->kind == GMB_TOK_TreeView || t->kind == GMB_TOK_TBrowse || t->kind == GMB_TOK_DatePicker
        || t->kind == GMB_TOK_ColorPicker || t->kind == GMB_TOK_FileDialog || t->kind == GMB_TOK_PrinterDialog
        || t->kind == GMB_TOK_StatusBar || t->kind == GMB_TOK_Spinner) {
        /* Famiglia UNICA: nessun prefisso PIX. Il controllo gmb_control_t rende celle
         * (TEXT) o pixel ricco (GRAPHICS) in base alla finestra. */
        return gmb_p_parse_control(p, line);
    }
    { gmb_stmt_t* g = gmb_p_parse_goroutine(p, line); if (g) return g; }
    if (t->kind == GMB_TOK_Mouse) { gmb_stmt_t* ms = gmb_p_parse_mouse(p, line); if (ms) return ms; }
    if (t->kind == GMB_TOK_Zip || t->kind == GMB_TOK_Unzip) { gmb_stmt_t* cz = gmb_p_parse_compression(p, line); if (cz) return cz; }
    /* JSON come COMANDO solo se NON è un assegnamento: `Json = NEW ...` (ritorno della FUNCTION
     * omonima) deve cadere nel ramo keyword-come-variabile sotto, non in gmb_p_parse_json
     * ("Comando non riconosciuto dopo 'JSON'"). */
    if (t->kind == GMB_TOK_Json && !(gmb_p_peek(p, 1) && (gmb_p_peek(p, 1)->kind == GMB_TOK_Equal
            || gmb_p_peek(p, 1)->kind == GMB_TOK_InlineAssign))) {
        gmb_stmt_t* js = gmb_p_parse_json(p, line); if (js) return js;
    }
    /* keyword usata come variabile/membro (es. data(i,0) = ..., done = TRUE, Message = ...) */
    if (t->kind >= GMB_TOK_Define) {
        bool kw_eq = gmb_p_peek(p, 1) && (gmb_p_peek(p, 1)->kind == GMB_TOK_Equal || gmb_p_peek(p, 1)->kind == GMB_TOK_InlineAssign);
        bool kw_arr = false;
        if (gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen) {
            int depth = 0;
            for (int i = 1; ; i++) {
                const gmb_token_t* tt = gmb_p_peek(p, i);
                if (!tt || tt->kind == GMB_TOK_Newline || tt->kind == GMB_TOK_Eof) break;
                if (tt->kind == GMB_TOK_LeftParen) depth++;
                else if (tt->kind == GMB_TOK_RightParen) { if (depth == 0) { if (gmb_p_peek(p, i + 1) && gmb_p_peek(p, i + 1)->kind == GMB_TOK_Equal) kw_arr = true; break; } depth--; }
            }
        }
        bool kw_bracket = gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftBracket;
        /* CHIAMATA NUDA di una procedura con nome keyword: `step()` (SUB step() definita).
         * Parità col frontend C# permissivo (commit 28bb165e9) e con `foo()` per gli
         * identificatori: keyword seguita da `( … )` NON seguita da `=` → call. Senza
         * questo, `step()` cadeva nel catch-all "comando non ancora supportato".
         * `WHILE(x)`/`IF(x)`/ecc. non arrivano qui (dispatchati prima dai loro parser). */
        bool kw_call = false;
        /* WHITELIST: solo le keyword "di clausola" (STEP/UNTIL/TO/THEN/AS/OF/BY/IN/WHEN),
         * che NON iniziano uno statement e che quindi possono essere nomi di procedura
         * chiamati nudi (`SUB step()` + `step()`, parità col C# permissivo). Le keyword
         * che iniziano uno statement e accettano `(` (PRINT (…), IF (…), WHILE (…),
         * SWITCH/MATCH (…)) NON vanno intercettate: vince il loro dispatcher. */
        bool kw_call_ok = (t->kind == GMB_TOK_Step || t->kind == GMB_TOK_Until
                           || t->kind == GMB_TOK_To || t->kind == GMB_TOK_Then
                           || t->kind == GMB_TOK_As || t->kind == GMB_TOK_Of
                           || t->kind == GMB_TOK_By || t->kind == GMB_TOK_In
                           || t->kind == GMB_TOK_When);
        if (kw_call_ok && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen) {
            int cdepth = 0;
            for (int i = 1; ; i++) {
                const gmb_token_t* tt = gmb_p_peek(p, i);
                if (!tt || tt->kind == GMB_TOK_Newline || tt->kind == GMB_TOK_Eof) break;
                if (tt->kind == GMB_TOK_LeftParen) cdepth++;
                else if (tt->kind == GMB_TOK_RightParen) {
                    cdepth--;
                    if (cdepth == 0) {   /* ')' che chiude la '(' iniziale */
                        const gmb_token_t* nx = gmb_p_peek(p, i + 1);
                        if (!(nx && (nx->kind == GMB_TOK_Equal || nx->kind == GMB_TOK_InlineAssign)))
                            kw_call = true;
                        break;
                    }
                }
            }
        }
        bool kw_member = gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Dot
            && !(t->kind == GMB_TOK_If || t->kind == GMB_TOK_While || t->kind == GMB_TOK_For
                 || t->kind == GMB_TOK_Do || t->kind == GMB_TOK_Select || t->kind == GMB_TOK_Switch
                 || t->kind == GMB_TOK_On || t->kind == GMB_TOK_Match || t->kind == GMB_TOK_When);
        /* BUG REALE trovato 2026-09-07 (SUB qualificata da namespace come statement, es.
         * `Calcoli.MostraRisultato(...)`, non eseguita): questo blocco gestisce "keyword
         * usata come identificatore" (t->kind >= GMB_TOK_Define copre PERO' anche
         * GMB_TOK_Identifier, che ha valore maggiore di GMB_TOK_Define nell'enum — quindi
         * TROVA ogni identificatore, non solo le keyword). kw_member intercettava quindi
         * ANCHE `NomeAlias.Membro` per un alias di namespace importato (IMPORT NS WITH
         * ALIAS / IMPORT NS AS Alias), dispatchando a gmb_p_parse_assign_or_call che tratta
         * "NomeAlias" come un normale identificatore/variabile — mai una SUB qualificata —
         * e la chiamata diventava un no-op silenzioso. Il ramo corretto per questo caso
         * esiste più sotto (~riga 257, controlla p->import_aliases) ma non veniva MAI
         * raggiunto perché questo blocco ritornava prima. Fix: se il lessema del token
         * corrente è un alias di namespace registrato, kw_member non scatta qui — il
         * controllo scende al ramo dedicato sotto, che already gestisce correttamente
         * l'espressione-statement qualificata. */
        if (kw_member && t->length < 32) {
            char nmk[32]; int mlk = t->length < 31 ? t->length : 31;
            memcpy(nmk, t->lexeme, (size_t)mlk); nmk[mlk] = 0;
            for (int ai = 0; ai < p->import_aliases_n; ai++)
                if (strcasecmp(p->import_aliases[ai], nmk) == 0) { kw_member = false; break; }
        }
        if (kw_eq || kw_arr || kw_bracket || kw_member || kw_call) {
            const gmb_token_t* tg = t;
            char* nm = (char*)malloc((size_t)tg->length + 1); memcpy(nm, tg->lexeme, (size_t)tg->length); nm[tg->length] = 0;
            gmb_p_adv(p);
            gmb_stmt_t* st = gmb_p_parse_assign_or_call(p, nm, line);
            free(nm);
            return st;
        }
    }

    if (t->kind == GMB_TOK_Message && gmb_p_is_message_verb(gmb_p_peek(p, 1))) {
        gmb_stmt_t* ms = gmb_p_parse_message(p, line); if (ms) return ms;
    }
    /* SIGNAL SUBSCRIBE/EMIT/UNSUBSCRIBE/PING — namespace SIGNAL (comando per nome:
     * SIGNAL non è una keyword del linguaggio, come CONTROL/WINDOW). */
    if (gmb_p_tok_is_soft_keyword(t, "SIGNAL")) {
        gmb_stmt_t* ss = gmb_p_parse_signal(p, line); if (ss) return ss;
    }
    if (t->kind == GMB_TOK_TeamCreate || t->kind == GMB_TOK_Message) { gmb_stmt_t* tm = gmb_p_parse_team(p, line); if (tm) return tm; }
    if (t->kind == GMB_TOK_Llm && gmb_p_peek(p, 1) && (gmb_p_peek(p, 1)->kind == GMB_TOK_AiPatch || (gmb_p_tok_is_soft_keyword(gmb_p_peek(p, 1), "PATCH")))) { gmb_stmt_t* ap = gmb_p_parse_aipatch(p, line); if (ap) return ap; }
    if (t->kind == GMB_TOK_Ai && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_TeamCreate) { gmb_p_adv(p); gmb_stmt_t* tm = gmb_p_parse_team(p, line); if (tm) return tm; }
    if (t->kind == GMB_TOK_Ai && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Mcp) { gmb_p_adv(p); gmb_stmt_t* mc = gmb_p_parse_mcp(p, line); if (mc) return mc; }
    /* AI AGENT CREATE/DELETE/SET (parser agent dedicato — non statement di dominio generico) */
    if (t->kind == GMB_TOK_Ai && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Agent) {
        gmb_p_adv(p);   /* AI */
        gmb_p_adv(p);   /* AGENT */
        gmb_stmt_t* ag = gmb_p_parse_agent(p, line);
        if (ag) return ag;
    }
    /* AI SERVER START/STOP/STATUS/MODEL/UNLOAD (parser dedicato; runtime condiviso
     * gmb_ai_server_*: gestione del server LLM come processo esterno) */
    if ((t->kind == GMB_TOK_Ai || t->kind == GMB_TOK_Llm) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Server) {
        gmb_p_adv(p);   /* AI | LLM */
        gmb_p_adv(p);   /* SERVER */
        gmb_stmt_t* as = gmb_p_parse_ai_server(p, line);
        if (as) return as;
    }
    /* AI MODEL LOAD/UNLOAD/CACHE/PREFETCH/DEVICE (M14 embedded, provider MoE out-of-core) */
    /* [HARD MOVE AI->LLM 2026-09-19] `AI MODEL …` non è più valido: solo `LLM MODEL …`. */
    if (t->kind == GMB_TOK_Llm && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_AiSubModel) {
        gmb_p_adv(p);   /* LLM */
        gmb_p_adv(p);   /* MODEL */
        gmb_stmt_t* am = gmb_p_parse_ai_model(p, line);
        if (am) return am;
    }
    /* TRAIN LORA MERGE — alias di LLM MODEL ADAPTER (stmt AIMODEL), runtime llmmodelio_model.
     * SAVE/LOAD NON passano di qui: sono il comando TRAIN LORA SAVE|LOAD (§4: adapter RAW
     * <-> .safetensors), gestito come domain-stmt (chunk/emit -> builtin 2312/2313). */
    if (t->kind == GMB_TOK_Train && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_AiSubLora
        && gmb_p_peek(p, 2)
        && gmb_p_peek(p, 2)->kind == GMB_TOK_LlmMerge) {
        gmb_p_adv(p);   /* TRAIN */
        gmb_p_adv(p);   /* LORA */
        gmb_stmt_t* lm = gmb_p_parse_ai_model(p, line);
        if (lm) return lm;
    }
    /* CONVBAT <dir>[, <fmt>][, <rec>] INTO <target> — forma comando di CONVERT.BATCH
     * (parser in gmb_parser_stmt_convert.c, parità Parser.ParseConvertBatchStatement C#). */
    if (t->kind == GMB_TOK_CvtBatch) { gmb_stmt_t* cb = gmb_p_parse_convbat(p, line); if (cb) return cb; }
    if (t->kind == GMB_TOK_WebToMd) { gmb_stmt_t* wm = gmb_p_parse_web2md(p, line); if (wm) return wm; }
    /* AI VERIFY <attempt> REQUIRE <require> [INTO <target>] — verifier/retry (B2).
     * Statement = GMB_STMT_CALL con call GMB_TOK_AiVerify (2 argomenti); runtime condiviso
     * gmb_verify_run (COMPILE via compilatore C, REQUIRE via valutatore runtime). */
    if (t->kind == GMB_TOK_Ai && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Verify) {
        gmb_p_adv(p);   /* AI */
        gmb_p_adv(p);   /* VERIFY */
        gmb_expr_t* v_attempt = gmb_p_parse_expr(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Require) gmb_p_adv(p);
        gmb_expr_t* v_require = gmb_p_parse_expr(p);
        /* gmb_expr_call_typed salva il PUNTATORE args senza copiare → array su heap. */
        gmb_expr_t** v_args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        v_args[0] = v_attempt; v_args[1] = v_require;
        gmb_expr_t* v_call = gmb_expr_call_typed("VERIFY", GMB_TOK_AiVerify, v_args, 2, line);
        /* [INTO <target>] → assegnamento del risultato (parità C# LetStmt). */
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Into) {
            gmb_p_adv(p);
            const gmb_token_t* v_tgt = gmb_p_cur(p);
            if (v_tgt && v_tgt->length > 0) {
                char* v_name = (char*)malloc((size_t)v_tgt->length + 1);
                memcpy(v_name, v_tgt->lexeme, (size_t)v_tgt->length); v_name[v_tgt->length] = 0;
                gmb_p_adv(p);
                gmb_stmt_t* v_a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                v_a->target = v_name;
                v_a->value = v_call;
                return v_a;
            }
        }
        gmb_stmt_t* v_s = gmb_stmt_new(GMB_STMT_CALL, line);
        v_s->expr = v_call;
        return v_s;
    }
    /* BRAINS BUILD/UPDATE <dir>[, <out>] | BRAINS FIND PATH <from> TO <to> | BRAINS RDD <backend>
     * — comandi legacy dei brain (ex GRAPH, 2026-11-04; parità Parser.Stmt.Graph.cs):
     * GMB_STMT_CALL con GMB_TOK_Graph*. */
    if (t->kind == GMB_TOK_Brains && gmb_p_peek(p, 1)) {
        gmb_tokenkind_t gv = gmb_p_peek(p, 1)->kind;
        if (gv == GMB_TOK_Build || gv == GMB_TOK_Update) {
            gmb_p_adv(p); gmb_p_adv(p);
            gmb_expr_t* g0 = gmb_p_graph_arg(p, line);
            gmb_expr_t* g1 = NULL;
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) { gmb_p_adv(p); g1 = gmb_p_graph_arg(p, line); }
            gmb_expr_t** ga = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*)); ga[0] = g0; ga[1] = g1;
            gmb_expr_t* gc = gmb_expr_call_typed("BUILD", gv == GMB_TOK_Build ? GMB_TOK_GraphBuild : GMB_TOK_GraphUpdate, ga, g1 ? 2 : 1, line);
            gmb_stmt_t* gs = gmb_stmt_new(GMB_STMT_CALL, line); gs->expr = gc; return gs;
        }
        if (gv == GMB_TOK_Find) {
            gmb_p_adv(p); gmb_p_adv(p);
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Path) gmb_p_adv(p);
            gmb_expr_t* gfrom = gmb_p_graph_arg(p, line);
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_To) gmb_p_adv(p);
            gmb_expr_t* gto = gmb_p_graph_arg(p, line);
            gmb_expr_t** ga = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*)); ga[0] = gfrom; ga[1] = gto;
            gmb_expr_t* gc = gmb_expr_call_typed("FINDPATH", GMB_TOK_GraphFindPath, ga, 2, line);
            gmb_stmt_t* gs = gmb_stmt_new(GMB_STMT_CALL, line); gs->expr = gc; return gs;
        }
        if (gv == GMB_TOK_Rdd) {
            gmb_p_adv(p); gmb_p_adv(p);
            gmb_expr_t* gb = gmb_p_graph_arg(p, line);
            gmb_expr_t** ga = (gmb_expr_t**)malloc(1 * sizeof(gmb_expr_t*)); ga[0] = gb;
            gmb_expr_t* gc = gmb_expr_call_typed("RDD", GMB_TOK_GraphRdd, ga, 1, line);
            gmb_stmt_t* gs = gmb_stmt_new(GMB_STMT_CALL, line); gs->expr = gc; return gs;
        }
    }
    /* AI DECIDE <state>, "NOUL|SCORE|CHOICE" [INTO <target>] — decision layer JEV/SystemOne
     * (v0: builtin GMB_TOK_AiDecide → gmb_ai_decide_run_json, provider Rules, JSON).
     * DECIDE è una soft keyword (Identifier), non una keyword lessicale. */
    if (t->kind == GMB_TOK_Ai && gmb_p_peek(p, 1) && gmb_p_tok_is_soft_keyword(gmb_p_peek(p, 1), "DECIDE")) {
        gmb_p_adv(p);   /* AI */
        gmb_p_adv(p);   /* DECIDE */
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Verify
                             || gmb_p_tok_is_keyword(gmb_p_cur(p), "VERIFY"))) {
            gmb_p_adv(p);
            gmb_expr_t* vs = gmb_p_parse_expr(p);
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) gmb_p_adv(p);
            gmb_expr_t* vk = gmb_p_parse_expr(p);
            gmb_expr_t* vp = NULL;
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) { gmb_p_adv(p); vp = gmb_p_parse_expr(p); }
            gmb_expr_t** va = (gmb_expr_t**)malloc(6 * sizeof(gmb_expr_t*));
            va[0] = vs; va[1] = vk; va[2] = vp ? vp : gmb_expr_str("rules|mock", line);
            va[3] = gmb_expr_str("", line); va[4] = gmb_expr_str("", line); va[5] = gmb_expr_str("", line);
            gmb_expr_t* vc = gmb_expr_call_typed("DECIDE", GMB_TOK_AiDecide, va, 6, line);
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Into) {
                gmb_p_adv(p);
                const gmb_token_t* vt = gmb_p_cur(p);
                if (vt && vt->length > 0) {
                    char* vn = (char*)malloc((size_t)vt->length + 1);
                    memcpy(vn, vt->lexeme, (size_t)vt->length); vn[vt->length] = 0;
                    gmb_p_adv(p);
                    gmb_stmt_t* va2 = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                    va2->target = vn; va2->value = vc;
                    return va2;
                }
            }
            gmb_stmt_t* vstmt = gmb_stmt_new(GMB_STMT_CALL, line);
            vstmt->expr = vc;
            return vstmt;
        }
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Newline || gmb_p_cur(p)->kind == GMB_TOK_Colon))
            return dc_parse_batch(p, line);
        gmb_expr_t* dc_state = gmb_p_parse_expr(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) gmb_p_adv(p);
        gmb_expr_t* dc_kind = gmb_p_parse_expr(p);
        /* [ , "<provider>" ] — selezione esplicita (rules/mock in v0). Forma a comma:
         * il parser delle espressioni non ha un delimitatore soft-keyword. */
        gmb_expr_t* dc_provider = NULL;
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) {
            gmb_p_adv(p);
            dc_provider = gmb_p_parse_expr(p);
        } else if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Using) {
            gmb_p_adv(p);   /* forma canonica: USING "local" */
            dc_provider = gmb_p_parse_expr(p);
        }
        /* [ FALLBACK "p1","p2"… ]: senza provider il primo nome è il primario. */
        int dc_fb_present = 0;
        char* dc_fb_csv = dc_parse_fallback_csv(p, line, &dc_fb_present);
        if (dc_fb_present && !dc_fb_csv) return NULL;
        gmb_expr_t* dc_fb_expr = NULL;
        if (dc_fb_present) {
            if (!dc_provider) {
                char* sep = strchr(dc_fb_csv, '|');
                if (sep) *sep = 0;
                dc_provider = gmb_expr_str(dc_fb_csv, line);
                if (sep) { char* rest = strdup(sep + 1); free(dc_fb_csv); dc_fb_csv = rest; }
                else { free(dc_fb_csv); dc_fb_csv = NULL; }
            }
            if (dc_fb_csv && *dc_fb_csv) dc_fb_expr = gmb_expr_str(dc_fb_csv, line);
            free(dc_fb_csv);
        }
        /* gmb_expr_call_typed salva il PUNTATORE args senza copiare → array su HEAP
         * (un array di stack diventerebbe dangling e l'emitter leggerebbe NULL/0). */
        int dc_n = 2 + (dc_provider ? 1 : 0) + (dc_fb_expr ? 1 : 0);
        gmb_expr_t** dc_args = (gmb_expr_t**)malloc((size_t)dc_n * sizeof(gmb_expr_t*));
        dc_args[0] = dc_state; dc_args[1] = dc_kind;
        if (dc_provider) dc_args[2] = dc_provider;
        if (dc_fb_expr) dc_args[dc_n - 1] = dc_fb_expr;
        gmb_expr_t* dc_call = gmb_expr_call_typed("DECIDE", GMB_TOK_AiDecide, dc_args, dc_n, line);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Into) {
            gmb_p_adv(p);
            const gmb_token_t* dc_tgt = gmb_p_cur(p);
            if (dc_tgt && dc_tgt->length > 0) {
                char* dc_tn = (char*)malloc((size_t)dc_tgt->length + 1);
                memcpy(dc_tn, dc_tgt->lexeme, (size_t)dc_tgt->length); dc_tn[dc_tgt->length] = 0;
                gmb_p_adv(p);
                gmb_stmt_t* dc_a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                dc_a->target = dc_tn;
                dc_a->value = dc_call;
                return dc_a;
            }
        }
        gmb_stmt_t* dc_s = gmb_stmt_new(GMB_STMT_CALL, line);
        dc_s->expr = dc_call;
        return dc_s;
    }

    /* AI ASKCHAIN <name>, <prompt>, <prefix> [INTO <target>] — anello di ragionamento
     * concatenato via Blackboard (T1.2). Statement = GMB_STMT_CALL con call GMB_TOK_AiAskChain;
     * runtime condiviso gmb_agent_ask_chain. */
    if (t->kind == GMB_TOK_Ai && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_AiAskChain) {
        gmb_p_adv(p);   /* AI */
        gmb_p_adv(p);   /* ASKCHAIN */
        gmb_expr_t* ac_name = gmb_p_parse_expr(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) gmb_p_adv(p);
        gmb_expr_t* ac_prompt = gmb_p_parse_expr(p);
        gmb_expr_t* ac_prefix = NULL;
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) { gmb_p_adv(p); ac_prefix = gmb_p_parse_expr(p); }
        int ac_n = ac_prefix ? 3 : 2;
        gmb_expr_t** ac_args = (gmb_expr_t**)malloc((size_t)ac_n * sizeof(gmb_expr_t*));
        ac_args[0] = ac_name; ac_args[1] = ac_prompt;
        if (ac_prefix) ac_args[2] = ac_prefix;
        gmb_expr_t* ac_call = gmb_expr_call_typed("ASKCHAIN", GMB_TOK_AiAskChain, ac_args, ac_n, line);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Into) {
            gmb_p_adv(p);
            const gmb_token_t* ac_tgt = gmb_p_cur(p);
            if (ac_tgt && ac_tgt->length > 0) {
                char* ac_tn = (char*)malloc((size_t)ac_tgt->length + 1);
                memcpy(ac_tn, ac_tgt->lexeme, (size_t)ac_tgt->length); ac_tn[ac_tgt->length] = 0;
                gmb_p_adv(p);
                gmb_stmt_t* ac_a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                ac_a->target = ac_tn;
                ac_a->value = ac_call;
                return ac_a;
            }
        }
        gmb_stmt_t* ac_s = gmb_stmt_new(GMB_STMT_CALL, line);
        ac_s->expr = ac_call;
        return ac_s;
    }
    /* AI EVAL VALIDATE <result> AGAINST [VAULT] <folder> [INTO <target>] — UAT via vault (T4.1).
     * Statement = GMB_STMT_CALL con call GMB_TOK_AiEvalValidate; runtime gmb_eval_validate. */
    if ((t->kind == GMB_TOK_Ai || t->kind == GMB_TOK_Train) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Eval
        && gmb_p_peek(p, 2) && gmb_p_peek(p, 2)->kind == GMB_TOK_Validate) {
        gmb_p_adv(p);   /* AI | TRAIN */
        gmb_p_adv(p);   /* EVAL */
        gmb_p_adv(p);   /* VALIDATE */
        gmb_expr_t* ev_res = gmb_p_parse_expr(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Identifier) gmb_p_adv(p);   /* AGAINST */
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Vault) gmb_p_adv(p);        /* [VAULT] */
        gmb_expr_t* ev_folder = gmb_p_parse_expr(p);
        gmb_expr_t** ev_args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        ev_args[0] = ev_res; ev_args[1] = ev_folder;
        gmb_expr_t* ev_call = gmb_expr_call_typed("EVALVALIDATE", GMB_TOK_AiEvalValidate, ev_args, 2, line);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Into) {
            gmb_p_adv(p);
            const gmb_token_t* ev_tgt = gmb_p_cur(p);
            if (ev_tgt && ev_tgt->length > 0) {
                char* ev_tn = (char*)malloc((size_t)ev_tgt->length + 1);
                memcpy(ev_tn, ev_tgt->lexeme, (size_t)ev_tgt->length); ev_tn[ev_tgt->length] = 0;
                gmb_p_adv(p);
                gmb_stmt_t* ev_a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                ev_a->target = ev_tn;
                ev_a->value = ev_call;
                return ev_a;
            }
        }
        gmb_stmt_t* ev_s = gmb_stmt_new(GMB_STMT_CALL, line);
        ev_s->expr = ev_call;
        return ev_s;
    }
    /* HTTP: TUTTI i verbi vanno all'handler inline (ROUTE/SERVER + CONFIG/STATIC/USE no-op).
     * Prima http_cmd copriva solo ROUTE/SERVER → CONFIG/STATIC cadevano nel domain-parser
     * → "statement di dominio 'HTTP' non supportato" (CanvasDemo/WebAgentDashboard). */
    bool http_cmd = (t->kind == GMB_TOK_Http);
    bool ws_cmd = (t->kind == GMB_TOK_WebSocket && gmb_p_peek(p, 1)
        && gmb_p_peek(p, 1)->kind == GMB_TOK_Connect);
    if (t->kind == GMB_TOK_Tcp || t->kind == GMB_TOK_Udp || t->kind == GMB_TOK_Ssl || ws_cmd
        || t->kind == GMB_TOK_Shutdown || t->kind == GMB_TOK_SetOption || t->kind == GMB_TOK_Fetch)
        return gmb_p_parse_net_cmd(p, line);
    /* Domain-statement SOLO per la forma a spazio (AI SET KEY ...). Se il namespace di dominio
     * è seguito da DOT (AI.REGISTER(...)) è una chiamata qualificata (dot-form) → statement-
     * espressione (parity C# ExpressionStmt), NON un domain-statement. */
    bool ai_next_dot = t && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Dot;
    /* Un token che è anche keyword di dominio (es. `sprite`) usato come NOME di variabile
     * (seguito da '(' / '=' / '[') NON è un domain-statement: `sprite(0) = 4` (variabile
     * array chiamata sprite). Prima → "SPRITE (: azione non supportata"
     * (WebAgentCanvasImageDemo, `DIM sprite(18) AS LONG`). */
    bool ns_var_use = t && gmb_p_peek(p, 1)
        && (gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen || gmb_p_peek(p, 1)->kind == GMB_TOK_Equal
            || gmb_p_peek(p, 1)->kind == GMB_TOK_LeftBracket);
    /* [HARD MOVE TRAIN 2026-10-02] training/produzione-pesi → namespace TRAIN:
     * `AI DISTILL` e `LLM FINETUNE|LORA|QUALITY` non sono più validi. */
    if (t->kind == GMB_TOK_Ai && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Distill) {
        gmb_p_set_err(p, "AI DISTILL non piu' valido: il comando e' nel namespace TRAIN (TRAIN DISTILL).", line);
        return NULL;
    }
    if (t->kind == GMB_TOK_Llm && gmb_p_peek(p, 1)
        && (gmb_p_peek(p, 1)->kind == GMB_TOK_AiSubFinetune || gmb_p_peek(p, 1)->kind == GMB_TOK_AiSubLora
            || gmb_p_peek(p, 1)->kind == GMB_TOK_LlmQuality)) {
        gmb_p_set_err(p, "LLM FINETUNE/LORA/QUALITY non piu' validi: spostati nel namespace TRAIN.", line);
        return NULL;
    }
    if (gmb_p_is_domain_ns(t->kind) && !http_cmd && !ws_cmd && !ai_next_dot && !ns_var_use) { gmb_stmt_t* dm = gmb_p_parse_domain_stmt(p, line); if (dm) return dm; }
    /* BARE COMMAND FALLBACK: comandi RDD usati SENZA prefisso `RDD` (es. `USE ":memory:" VIA
     * "ARRAY" ALIAS T1`), quando RDD è importato/attivo (parity Parser bare-command fallback). */
    if ((t->kind == GMB_TOK_Use || t->kind == GMB_TOK_Replace || t->kind == GMB_TOK_Delete || t->kind == GMB_TOK_Recall) && p) {
        int rdd = 0;
        for (int ii = 0; ii < p->import_aliases_n; ii++) {
            const char* rn = p->import_aliases_ns[ii][0] ? p->import_aliases_ns[ii] : p->import_aliases[ii];
            if (strcasecmp(rn, "RDD") == 0) { rdd = 1; break; }
        }
        if (rdd) return gmb_p_parse_domain_stmt_ov(p, line, "RDD", GMB_TOK_Rdd);
    }
    /* `CONNECTOR <verbo> …` — alias LEGACY senza `AI` (parità Parser.Stmt.Connector.ParsenStatement).
     * CONNECTOR/CONNECTORS sono keyword del lexer C (come in Lexer.Keywords.cs, TokenKind.Connector)
     * → il riconoscimento è per TokenKind, mai per lessema (§12). Si sintetizza il dominio
     * AI CONNECTOR: win_name=AI, mutex_action=CONNECTOR, go_args=[verbo,…]. */
    if (t->kind == GMB_TOK_Connector
        && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof
        && gmb_p_peek(p, 1)->kind != GMB_TOK_Equal && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftParen
        && gmb_p_peek(p, 1)->kind != GMB_TOK_Dot) {
        gmb_p_adv(p);   /* CONNECTOR */
        gmb_stmt_t* cs = gmb_stmt_new(GMB_STMT_DOMAIN, line);
        cs->win_name = strdup("AI");
        cs->win_tk = GMB_TOK_Ai;
        cs->mutex_action = strdup("CONNECTOR");
        int ccap = 0, first = 1;
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) {
            if (gmb_p_check(p, GMB_TOK_Comma)) { gmb_p_adv(p); continue; }
            int tp = p->pos;
            gmb_expr_t* e = gmb_p_parse_expr(p);
            if (!e) {
                const gmb_token_t* kt2 = gmb_p_cur(p);
                if (kt2 && kt2->lexeme && kt2->length > 0) { e = gmb_expr_var(gmb_p_tok_strdup(kt2), line); gmb_p_adv(p); }
            }
            if (e) { if (cs->n_go_args >= ccap) { ccap = ccap ? ccap * 2 : 8; cs->go_args = (gmb_expr_t**)realloc(cs->go_args, (size_t)ccap * sizeof(gmb_expr_t*)); } cs->go_args[cs->n_go_args++] = e; }
            if (p->pos == tp) gmb_p_adv(p);
            first = 0; (void)first;
        }
        return cs;
    }
    { gmb_stmt_t* ev = gmb_p_parse_events(p, line); if (ev) return ev; }
    /* statement qualificato da namespace IMPORTATO (es. Calcoli.MostraRisultato(...)):
     * il parser statement non strippa l'ns (a differenza dell'espressione) → lo trattiamo
     * come statement-espressione, che risolve l'ns e la funzione nuda. */
    if (t && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Dot
        && t->kind != GMB_TOK_Newline && t->kind != GMB_TOK_Eof && t->length < 64)
    {
        char nml[64]; int ml2 = t->length < 63 ? t->length : 63;
        memcpy(nml, t->lexeme, (size_t)ml2); nml[ml2] = 0;
        int is_alias = 0;
        for (int ai = 0; ai < p->import_aliases_n; ai++)
            if (strcasecmp(p->import_aliases[ai], nml) == 0) { is_alias = 1; break; }
        if (is_alias)
        {
            gmb_stmt_t* se = gmb_stmt_new(GMB_STMT_EXPR, line);
            se->expr = gmb_p_parse_expr(p);
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
            return se;
        }
    }
    if (t->kind == GMB_TOK_DoEvents) {
        /* DOEVENTS (VB6): un frame di eventi NON bloccante (WINDOW PUMP 0). Stesso AST. */
        gmb_p_adv(p);
        gmb_stmt_t* sp = gmb_stmt_new(GMB_STMT_WINDOW_PUMP, line);
        sp->channel = gmb_expr_num(0.0, line);
        return sp;
    }
    if (t->kind == GMB_TOK_Window) {
        /* WINDOW grafico [SCREEN] (...) vs WINDOW TUI (nome/DEFINE/THEME/...):
         * il primo va al parser gfx (viewport, sub 18), il secondo a gmb_p_parse_window. */
        const gmb_token_t* nx = gmb_p_peek(p, 1);
        bool gfx_form = !nx || nx->kind == GMB_TOK_Screen || nx->kind == GMB_TOK_LeftParen
                     || nx->kind == GMB_TOK_Newline || nx->kind == GMB_TOK_Colon;
        if (gfx_form) return gmb_p_parse_gfx(p, line);
        return gmb_p_parse_window(p, line);
    }
    if (t->kind == GMB_TOK_Print) { gmb_p_adv(p); return gmb_p_parse_print(p, line); }
    if (t->kind == GMB_TOK_QuestionMark) { gmb_p_adv(p); return gmb_p_parse_print(p, line); }
    /* 2.7: CLASS (anche PUBLIC/PRIVATE/ABSTRACT CLASS) — parsing reale */
    {
        const gmb_token_t* c1 = t; int cadv = 0;
        if ((t->kind == GMB_TOK_Public || t->kind == GMB_TOK_Private) && gmb_p_peek(p, 1)) { c1 = gmb_p_peek(p, 1); cadv = 1; }
        if (c1 && c1->kind == GMB_TOK_Abstract && gmb_p_peek(p, cadv + 1)) { c1 = gmb_p_peek(p, cadv + 1); cadv++; }
        if (c1 && c1->kind == GMB_TOK_Class) { for (int i = 0; i < cadv; i++) gmb_p_adv(p); return gmb_p_parse_class(p, line, 0, 0); }
    }
    /* UNION (2026-11-11): tipo a campi SOVRAPPOSTI (semantica C). Stesso corpo di un TYPE
     * (solo campi), chiuso da END UNION (o END TYPE). La semantica di aliasing è realizzata
     * dal runtime (PCodeVM/VM-C: buffer di byte + refresh dei fratelli) e — su C99Native —
     * da una `union` anonima C11 con i tipi C reali: in entrambi i casi l'utente scrive un
     * campo e ne legge un altro. */
    if (t->kind == GMB_TOK_UnionType) {
        gmb_stmt_t* u = gmb_p_parse_class(p, line, 1, 0);
        if (u) u->class_is_union = 1;
        return u;
    }
    /* dichiarazioni a blocco: TYPE/PACKED TYPE/ENUM/INTERFACE ... END <word> — nessun codice */
    if (t->kind == GMB_TOK_Type || t->kind == GMB_TOK_Enum || t->kind == GMB_TOK_Interface
        || ((t->kind == GMB_TOK_Public || t->kind == GMB_TOK_Private) && gmb_p_peek(p, 1) && (gmb_p_peek(p, 1)->kind == GMB_TOK_Enum || gmb_p_peek(p, 1)->kind == GMB_TOK_Interface || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Type")))
        || (t->kind == GMB_TOK_Packed && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Type)) {
        /* PACKED TYPE: il flag NON si scarta — è il layout dei campi (somma senza padding),
         * che serve integro all'emitter per il canone di SIZEOF (vedi TypeLayout.cs). */
        int packed_decl = 0;
        if (t->kind == GMB_TOK_Public || t->kind == GMB_TOK_Private) gmb_p_adv(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Packed) { gmb_p_adv(p); packed_decl = 1; }
        /* 2.7(d)p2b: TYPE → UDT value-type: parsato come classe con flag is_udt
         * (campi nudi + SUB NEW/PROPERTY/OPERATOR; assegnamento = copia valore). */
        if (gmb_p_check(p, GMB_TOK_Type) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Type")) {
            return gmb_p_parse_class(p, line, 1, packed_decl);
        }
        /* 2.9: INTERFACE → classe marcata is_interface, con le FIRME dei metodi
         * dichiarati (SUB/FUNCTION Nome(...) [AS Tipo], una riga per membro, senza
         * corpo) catturate in cls_methods (func_body vuoto — un'interfaccia non ha
         * mai un'implementazione propria, solo il NOME/arity serve al compilatore
         * per riconoscere `obj.Metodo()` su una variabile dichiarata AS <Interfaccia>
         * — il dispatch reale avviene comunque sul VTable dell'ISTANZA concreta a
         * runtime, mai su questo descrittore). Prima (fino a questa sessione) il
         * corpo veniva scartato token-per-token, rendendo IMPLEMENTS/INTERFACE
         * inutilizzabile per il dispatch (nessuna firma mai in AST). */
        if (gmb_p_check(p, GMB_TOK_Interface)) {
            gmb_p_adv(p);   /* INTERFACE */
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CLASS, line);
            s->class_is_interface = 1;
            const gmb_token_t* nm = gmb_p_cur(p);
            if (nm && !gmb_p_at_end(p) && nm->kind != GMB_TOK_Newline) { s->class_name = gmb_p_tok_strdup(nm); gmb_p_adv(p); }
            else s->class_name = strdup("_iface_");
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);   /* generico <T> ecc. */
            if (gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
            int icap = 0;
            for (;;) {   /* firme membro fino a END INTERFACE / END */
                gmb_p_skip_eol(p);
                while (!gmb_p_at_end(p) && gmb_p_cur(p)->kind == GMB_TOK_LineNumber) gmb_p_adv(p);
                if (gmb_p_at_end(p)) break;
                const gmb_token_t* ct = gmb_p_cur(p);
                if (ct->kind == GMB_TOK_EndInterface || gmb_p_tok_is_keyword(ct, "EndInterface")) { gmb_p_adv(p); break; }
                if (ct->kind == GMB_TOK_End) {
                    gmb_p_adv(p);
                    const gmb_token_t* nw = gmb_p_cur(p);
                    if (nw && (nw->kind == GMB_TOK_Interface || gmb_p_tok_is_keyword(nw, "Interface"))) { gmb_p_adv(p); break; }
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                    if (gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                    continue;
                }
                if (ct->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(ct, "Sub") || gmb_p_tok_is_keyword(ct, "Function")) {
                    gmb_p_adv(p);   /* SUB/FUNCTION */
                    gmb_stmt_t* mm = gmb_stmt_new(GMB_STMT_FUNCTION, ct->line);
                    const gmb_token_t* mn = gmb_p_cur(p);
                    if (mn && mn->kind != GMB_TOK_Newline) { mm->func_name = gmb_p_tok_strdup(mn); gmb_p_adv(p); }
                    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                        int mpcap = 0;
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
                            if (gmb_p_check(p, GMB_TOK_ByRef) || gmb_p_check(p, GMB_TOK_ByVal)
                                || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byref") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Byval")
                                || gmb_p_tok_is_keyword(gmb_p_cur(p), "Optional")) gmb_p_adv(p);
                            const gmb_token_t* prm = gmb_p_cur(p);
                            if (!prm || prm->kind == GMB_TOK_RightParen || prm->kind == GMB_TOK_Newline) break;
                            if (mm->n_params >= mpcap) {
                                mpcap = mpcap ? mpcap * 2 : 4;
                                mm->func_params = (char**)realloc(mm->func_params, (size_t)mpcap * sizeof(char*));
                                mm->param_byref = (uint8_t*)realloc(mm->param_byref, (size_t)mpcap);
                                mm->param_is_array = (uint8_t*)realloc(mm->param_is_array, (size_t)mpcap); mm->param_ndims = (uint8_t*)realloc(mm->param_ndims, (size_t)mpcap);
                                mm->param_types = (char**)realloc(mm->param_types, (size_t)mpcap * sizeof(char*));
                                for (int z = mm->n_params; z < mpcap; z++) { mm->param_types[z] = NULL; mm->param_is_array[z] = 0; mm->param_ndims[z] = 0; }
                            }
                            mm->func_params[mm->n_params] = gmb_p_tok_strdup(prm);
                            mm->param_byref[mm->n_params] = 0;
                            mm->param_is_array[mm->n_params] = 0; mm->param_ndims[mm->n_params] = 0;
                            mm->param_types[mm->n_params] = NULL;
                            mm->n_params++;
                            gmb_p_adv(p);
                            if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                                gmb_p_adv(p);
                                const gmb_token_t* ty = gmb_p_cur(p);
                                if (ty && ty->kind != GMB_TOK_RightParen && ty->kind != GMB_TOK_Newline) { mm->param_types[mm->n_params - 1] = gmb_p_tok_strdup(ty); gmb_p_adv(p); }
                            }
                            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                        }
                        gmb_p_match(p, GMB_TOK_RightParen);
                    }
                    if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As))) {
                        gmb_p_adv(p);
                        const gmb_token_t* ty = gmb_p_cur(p);
                        if (ty && ty->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) { mm->func_ret_type = gmb_p_tok_strdup(ty); gmb_p_adv(p); }
                    }
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);   /* residui riga (solo firma, niente corpo) */
                    if (s->n_cls_methods >= icap) { icap = icap ? icap * 2 : 8; s->cls_methods = (gmb_stmt_t**)realloc(s->cls_methods, (size_t)icap * sizeof(gmb_stmt_t*)); }
                    s->cls_methods[s->n_cls_methods++] = mm;
                    continue;
                }
                gmb_p_adv(p);
            }
            return s;
        }
        /* ENUM ... END ENUM: membri → costanti (GMB_STMT_CONST), valore implicito = +1
         * dall'ultimo (parità C#). Prima il blocco era SKIPPATO token-per-token →
         * `StatoOrdine.Nuovo` non esisteva (0) sul C99Native
         * (minitests/constructs/types_enum.bas). */
        if (gmb_p_check(p, GMB_TOK_Enum) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Enum")) {
            gmb_p_adv(p);
            char enum_prefix[160] = "";
            const gmb_token_t* enm = gmb_p_cur(p);
            if (enm && enm->kind == GMB_TOK_Identifier) {   /* nome ENUM: prefisso dei membri */
                snprintf(enum_prefix, sizeof(enum_prefix), "%.*s.", (int)enm->length, enm->lexeme);
                gmb_p_adv(p);
            }
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) { gmb_p_adv(p); if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline) gmb_p_adv(p); }
            if (gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
            double lastv = 0;
            int has_last_num = 0;
            gmb_stmt_t* prev_cst = NULL;
            for (;;) {
                gmb_p_skip_eol(p);
                if (gmb_p_at_end(p)) break;
                const gmb_token_t* mt = gmb_p_cur(p);
                if (mt->kind == GMB_TOK_EndEnum || gmb_p_tok_is_keyword(mt, "EndEnum")) { gmb_p_adv(p); break; }
                if (mt->kind == GMB_TOK_End) {
                    gmb_p_adv(p);
                    const gmb_token_t* nw = gmb_p_cur(p);
                    if (nw && (nw->kind == GMB_TOK_Enum || gmb_p_tok_is_keyword(nw, "Enum"))) { gmb_p_adv(p); break; }
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                    if (gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                    continue;
                }
                if (mt->kind == GMB_TOK_Newline) { gmb_p_adv(p); continue; }
                gmb_stmt_t* cst = gmb_stmt_new(GMB_STMT_CONST, mt->line);
                if (enum_prefix[0]) {
                    size_t pl = strlen(enum_prefix);
                    char* full = (char*)malloc(pl + mt->length + 1);
                    if (full) { memcpy(full, enum_prefix, pl); memcpy(full + pl, mt->lexeme, mt->length); full[pl + mt->length] = 0; }
                    cst->target = full;
                } else
                    cst->target = gmb_p_tok_strdup(mt);
                gmb_p_adv(p);
                if (gmb_p_match(p, GMB_TOK_Equal)) {
                    cst->value = gmb_p_parse_expr(p);
                    if (cst->value && cst->value->kind == GMB_EXPR_NUM) { lastv = cst->value->num; has_last_num = 1; prev_cst = NULL; }
                    else { has_last_num = 0; prev_cst = cst; }
                } else {
                    if (has_last_num) { cst->value = gmb_expr_num(lastv + 1.0, mt->line); lastv += 1.0; }
                    else if (prev_cst && prev_cst->value) {
                        cst->value = gmb_expr_binary("+", prev_cst->value, gmb_expr_num(1.0, mt->line), mt->line);
                        prev_cst->value = NULL;   /* proprietà trasferita al nuovo nodo (no double-free) */
                        prev_cst = cst;
                    } else cst->value = gmb_expr_num(0.0, mt->line);
                }
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                gmb_p_queue_stmt(p, cst);
            }
            return gmb_stmt_new(GMB_STMT_REM, line);
        }
        gmb_tokenkind_t stop = (gmb_p_check(p, GMB_TOK_Enum)) ? GMB_TOK_EndEnum : GMB_TOK_EndType;
        const char* sname = (gmb_p_check(p, GMB_TOK_Enum)) ? "EndEnum" : (gmb_p_check(p, GMB_TOK_Interface)) ? "EndInterface" : "EndType";
        gmb_p_adv(p);
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);   /* nome in testa (anche generico Box<T>) */
        gmb_stmt_t* ds = gmb_stmt_new(GMB_STMT_REM, line);
        /* skip righe di dichiarazione campo fino a END <word> / END */
        for (;;) {
            if (gmb_p_at_end(p)) break;
            const gmb_token_t* ct = gmb_p_cur(p);
            if (ct->kind == stop || (sname && gmb_p_tok_is_keyword(ct, sname))) { gmb_p_adv(p); break; }
            if (ct->kind == GMB_TOK_End) {
                gmb_p_adv(p);
                const gmb_token_t* nw = gmb_p_cur(p);
                int is_term = nw && (nw->kind == GMB_TOK_Type || nw->kind == GMB_TOK_Enum
                    || nw->kind == GMB_TOK_Interface || nw->kind == GMB_TOK_Class
                    || gmb_p_tok_is_keyword(nw, "Type")
                    || gmb_p_tok_is_keyword(nw, "Enum") || gmb_p_tok_is_keyword(nw, "Interface")
                    || gmb_p_tok_is_keyword(nw, "Class"));
                if (is_term) { gmb_p_adv(p); break; }
                /* END FUNCTION/SUB/OPERATOR/PROPERTY interno a una classe: salta la riga e continua */
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                if (gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
                continue;
            }
            if (ct->kind == GMB_TOK_Newline) { gmb_p_adv(p); continue; }
            gmb_p_adv(p);
        }
        return ds;
    }
    /* PUBLIC/PRIVATE/STATIC … OPERATOR … END OPERATOR a livello MODULO (non dentro una
     * classe): metodo sintetico OP_<simbolo>/OP_CTYPE (parity Parser.Stmt.Classes). */
    if (t->kind == GMB_TOK_Public || t->kind == GMB_TOK_Private || t->kind == GMB_TOK_Static) {
        int j = 1;
        while (gmb_p_peek(p, j) && (gmb_p_peek(p, j)->kind == GMB_TOK_Public || gmb_p_peek(p, j)->kind == GMB_TOK_Private
               || gmb_p_peek(p, j)->kind == GMB_TOK_Static || gmb_p_peek(p, j)->kind == GMB_TOK_Abstract)) j++;
        if (gmb_p_peek(p, j) && gmb_p_tok_is_keyword(gmb_p_peek(p, j), "Operator")) {
            for (int k = 0; k < j + 1; k++) gmb_p_adv(p);   /* modifiers + OPERATOR */
            return gmb_p_parse_operator_method(p, line);
        }
    }
    /* membri di classe: PUBLIC/PRIVATE/STATIC FUNCTION|SUB|OPERATOR|... → skip (dichiarazione) */
    if ((t->kind == GMB_TOK_Public || t->kind == GMB_TOK_Private || t->kind == GMB_TOK_Static) && gmb_p_peek(p, 1)
        && gmb_p_peek(p, 1)->kind == GMB_TOK_Function) {
        /* PUBLIC/PRIVATE/STATIC SUB/FUNCTION — lascia decidere al gestore dichiarazioni */
        gmb_p_adv(p);
        t = gmb_p_cur(p);
    } else if (t->kind == GMB_TOK_Public && gmb_p_peek(p, 1)
        && gmb_p_peek(p, 1)->kind != GMB_TOK_Enum && gmb_p_peek(p, 1)->kind != GMB_TOK_Class && gmb_p_peek(p, 1)->kind != GMB_TOK_Abstract
        && !gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Type") && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline) {
        /* `PUBLIC <non-funzione/tipo>` a livello MODULO: dichiarazione → skip.
         * NB: PRIVATE/STATIC (e LOCAL) con un nome-variabile NON vanno saltati: sono
         * DICHIARAZIONI DI VARIABILE (anche persistenti, dentro una SUB) gestite dal ramo
         * LOCAL/STATIC/PRIVATE sotto. Prima venivano inghiottite qui come REM → variabile
         * silenziosamente NON registrata (bug: `STATIC Accum(2) AS DOUBLE` → "Accum non
         * definita"; `PRIVATE x`). */
        gmb_p_adv(p);
        if (t->kind == GMB_TOK_Abstract) gmb_p_adv(p);
        gmb_p_consume_statement(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    /* WRITE #n, <expr>, ... — come PRINT # ma con separatori virgola (FASE 2.2) */
    if (t->kind == GMB_TOK_Write && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Hash) {
        gmb_p_adv(p);
        gmb_stmt_t* w = gmb_p_parse_printfile(p, line);
        w->flag = 1;
        return w;
    }
    /* RENAME <old> TO <new> | COPY <src> TO <dst> | XCOPY <src> TO <dst>[, rec[, ovr]]
     * (FASE 2.2; flag XCOPY 2026-09-21: rec→file_pos, ovr→cond_mutex, default 0) */
    if (t->kind == GMB_TOK_Rename || t->kind == GMB_TOK_Copy || t->kind == GMB_TOK_XCopy) {
        gmb_stmt_kind_t k = t->kind == GMB_TOK_Rename ? GMB_STMT_RENAMEFILE : GMB_STMT_COPYFILE;
        gmb_stmt_t* fs = gmb_stmt_new(k, line);
        if (t->kind == GMB_TOK_XCopy) fs->flag = 1;
        gmb_p_adv(p);
        fs->value = gmb_p_parse_expr(p);
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) gmb_p_adv(p);
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) gmb_p_adv(p);
        else gmb_p_match(p, GMB_TOK_Comma);
        fs->cond = gmb_p_parse_expr(p);
        if (fs->flag == 1 && gmb_p_match(p, GMB_TOK_Comma)) {
            fs->file_pos = gmb_p_parse_expr(p);   /* recursive: conservato (era scartato) */
            if (gmb_p_match(p, GMB_TOK_Comma))
                fs->cond_mutex = gmb_p_parse_expr(p);   /* overwrite: conservato */
        }
        return fs;
    }
    /* MOVE <src> TO <dst> (2026-09-21, parità 4 backend gruppo IO): sposta file
     * (parity File.Move C#) via gmb_file_move. Nota: AI NOTE MOVE è un altro dominio
     * (domain-statement), qui solo il MOVE filesystem top-level. */
    if (t->kind == GMB_TOK_Move) {
        gmb_stmt_t* mv = gmb_stmt_new(GMB_STMT_MOVEFILE, line);
        gmb_p_adv(p);
        mv->value = gmb_p_parse_expr(p);
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) gmb_p_adv(p);
        else gmb_p_match(p, GMB_TOK_Comma);
        mv->cond = gmb_p_parse_expr(p);
        return mv;
    }
    /* WATCH path, mask, CallbackSUB | UNWATCH path (2026-09-21, parità 4 backend
     * gruppo IO): filesystem watcher con callback SUB (parity IoWatch C#).
     * value = path, cond = mask (ignorata dal runtime C: tutti gli eventi),
     * net_coderef = nome SUB (deve esistere, 0 parametri). */
    if (t->kind == GMB_TOK_Watch) {
        gmb_stmt_t* w = gmb_stmt_new(GMB_STMT_IOWATCH, line);
        gmb_p_adv(p);
        w->value = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) w->cond = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) {
            const gmb_token_t* cb = gmb_p_cur(p);
            /* nome SUB: Identifier o keyword usata come nome (stesso criterio di GO,
             * goroutines.c:190 — i callback si chiamano spesso come verbi). */
            if (cb && (cb->kind == GMB_TOK_Identifier || cb->kind == GMB_TOK_Unknown
                       || cb->kind >= GMB_TOK_Define)) {
                w->net_coderef = gmb_p_tok_strdup(cb);
                gmb_p_adv(p);
            }
        }
        return w;
    }
    if (t->kind == GMB_TOK_Unwatch) {
        gmb_stmt_t* w = gmb_stmt_new(GMB_STMT_IOUNWATCH, line);
        gmb_p_adv(p);
        w->value = gmb_p_parse_expr(p);
        return w;
    }
    /* 2.6(r): statement grafici — SCREEN/SYNC/POINT/LOAD TEXTURE (GMB_STMT_GFX);
     * CPRINT = PRINT su console host (parity capture output PCodeVM headless) */
    if (t->kind == GMB_TOK_Load && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Texture)
        return gmb_p_parse_gfx(p, line);
    {
        const gmb_token_t* nx = gmb_p_peek(p, 1);
        bool gfx_cmd = (t->kind == GMB_TOK_Screen || t->kind == GMB_TOK_TextMode || t->kind == GMB_TOK_Sync || t->kind == GMB_TOK_Point ||
                        t->kind == GMB_TOK_ScreenSet || t->kind == GMB_TOK_PSet || t->kind == GMB_TOK_Preset ||
                        t->kind == GMB_TOK_Circle || t->kind == GMB_TOK_Paint || t->kind == GMB_TOK_Font ||
                        t->kind == GMB_TOK_PCopy || t->kind == GMB_TOK_Palette || t->kind == GMB_TOK_View ||
                        t->kind == GMB_TOK_Viewport);
        /* LINE non e' LINE INPUT; DRAW non e' DRAW TEXTURES */
        if (t->kind == GMB_TOK_Line && nx && nx->kind != GMB_TOK_Input) gfx_cmd = true;
        if (t->kind == GMB_TOK_Draw && nx && nx->kind != GMB_TOK_DrawTextures) gfx_cmd = true;
        if (t->kind == GMB_TOK_Unload && nx && (nx->kind == GMB_TOK_Texture || nx->kind == GMB_TOK_Font)) gfx_cmd = true;
        if (t->kind == GMB_TOK_BSave || t->kind == GMB_TOK_BLoad) gfx_cmd = true;
        if (t->kind == GMB_TOK_ImageGet || t->kind == GMB_TOK_ImagePut || t->kind == GMB_TOK_ImagePuts) gfx_cmd = true;
        if (t->kind == GMB_TOK_Polygon) gfx_cmd = true;
        if (t->kind == GMB_TOK_Chart && nx && (nx->kind == GMB_TOK_Plot || nx->kind == GMB_TOK_Show ||
            nx->kind == GMB_TOK_Save || nx->kind == GMB_TOK_Clear || nx->kind == GMB_TOK_Close ||
            nx->kind == GMB_TOK_Financial || nx->kind == GMB_TOK_Set || nx->kind == GMB_TOK_Get ||
            nx->kind == GMB_TOK_Draw ||
            gmb_p_tok_is_soft_keyword(nx, "GETS"))) gfx_cmd = true;
        if (t->kind == GMB_TOK_Canvas && nx && (gmb_p_tok_is_soft_keyword(nx, "NEW") ||
            gmb_p_tok_is_soft_keyword(nx, "USE") || gmb_p_tok_is_soft_keyword(nx, "END") ||
            gmb_p_tok_is_soft_keyword(nx, "CLEAR") || gmb_p_tok_is_soft_keyword(nx, "SET") ||
            gmb_p_tok_is_soft_keyword(nx, "GET") || gmb_p_tok_is_soft_keyword(nx, "BLIT") ||
            gmb_p_tok_is_soft_keyword(nx, "SAVE") || gmb_p_tok_is_soft_keyword(nx, "LOAD") ||
            gmb_p_tok_is_soft_keyword(nx, "FREE") || gmb_p_tok_is_soft_keyword(nx, "WIDTH") ||
            gmb_p_tok_is_soft_keyword(nx, "HEIGHT"))) gfx_cmd = true;
        if (t->kind == GMB_TOK_PixelBuffer) gfx_cmd = true;
        /* `PIXEL BUFFER w, h, buf` (forma a spazio di PIXELBUFFER: PIXEL + BUFFER).
         * BUFFER è keyword del lexer (GMB_TOK_Buffer, come TokenKind.Buffer in C#):
         * il dispatch è per TokenKind, non per lessema. */
        if (t->kind == GMB_TOK_Pixel && nx && nx->kind == GMB_TOK_Buffer) gfx_cmd = true;
        if (t->kind == GMB_TOK_Spritedef) gfx_cmd = true;
        if (t->kind == GMB_TOK_Tilemap && nx && (nx->kind == GMB_TOK_Load || nx->kind == GMB_TOK_Set ||
            nx->kind == GMB_TOK_Offset || nx->kind == GMB_TOK_Scroll || nx->kind == GMB_TOK_Draw)) gfx_cmd = true;
        if (gfx_cmd) {
            gmb_stmt_t* gx = gmb_p_parse_gfx(p, line);
            if (gx) return gx;
        }
    }
    if (t->kind == GMB_TOK_CPrint) { gmb_p_adv(p); gmb_stmt_t* cs = gmb_p_parse_print(p, line); if (cs) cs->flag = 1; return cs; }
    /* DELEGATE FUNCTION <Nome>(sig) AS <ret> — tipo funzione nominato (2026-09-06) */
    if (t->kind == GMB_TOK_Delegate) {
        gmb_p_adv(p);
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Function") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Sub")) gmb_p_adv(p);
        gmb_stmt_t* dl = gmb_stmt_new(GMB_STMT_DELEGATE, line);
        const gmb_token_t* dn = gmb_p_cur(p);
        if (dn && dn->lexeme && dn->kind != GMB_TOK_Newline) { dl->func_name = gmb_p_tok_strdup(dn); gmb_p_adv(p); }
        dl->func_val_arity = gmb_p_fn_sig(p, &dl->func_val_str_mask, NULL, NULL);
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
            gmb_p_adv(p);
            const gmb_token_t* rt = gmb_p_cur(p);
            if (rt && rt->lexeme && rt->kind != GMB_TOK_Newline) { dl->func_ret_type = gmb_p_tok_strdup(rt); gmb_p_adv(p); }
        }
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
        return dl;
    }
    /* DECLARE [PUBLIC|PRIVATE] FUNCTION|SUB <nome> LIB "<lib>" [ALIAS "<sym>"] ([params]) [AS <ret>]
     * — FFI: statement FUNCTION sintetico con n_funcbody=0 + dati lib/symbol; l'emitter
     * genera il wrapper dlopen/dlsym (parity emitter C# EmitFfiWrapper). */
    if (t->kind == GMB_TOK_Declare) {
        gmb_p_adv(p);
        /* visibilità: consumata e ignorata (il codice C generato è statico) */
        if (gmb_p_check(p, GMB_TOK_Public) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Private")) gmb_p_adv(p);
        /* DECLARE NATIVE FUNCTION|SUB: modificatore contestuale (non riservato): ABI esatta, niente estensioni array. */
        int dl_native = 0;
        if (gmb_p_tok_is_soft_keyword(gmb_p_cur(p), "native")) { dl_native = 1; gmb_p_adv(p); }
        /* SUB/FN/FUNCTION lexano come GMB_TOK_Function: SUB si distingue dal lessema. */
        const gmb_token_t* dk = gmb_p_cur(p);
        int dl_is_sub = gmb_p_tok_is_soft_keyword(dk, "sub");
        if (!gmb_p_tok_is_keyword(dk, "Function") && !gmb_p_tok_is_keyword(dk, "Sub"))
            { gmb_p_set_err(p, "DECLARE: atteso FUNCTION o SUB", line); return NULL; }
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_FUNCTION, line);
        s->is_ffi = 1;
        s->ffi_native = dl_native;
        s->is_sub = dl_is_sub;
        /* 2026-09-24: namespace di definizione (check shadowing builtin in emissione). */
        if (p->cur_ns[0]) { s->func_ns = strdup(p->cur_ns); }
        const gmb_token_t* fn = gmb_p_cur(p);
        if (fn && fn->lexeme) {
            s->func_name = (char*)malloc((size_t)fn->length + 1);
            memcpy(s->func_name, fn->lexeme, (size_t)fn->length); s->func_name[fn->length] = 0;
            gmb_p_adv(p);
        }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Lib") || gmb_p_check(p, GMB_TOK_Lib)) {
            gmb_p_adv(p);
            const gmb_token_t* lb = gmb_p_cur(p);
            if (lb && lb->lexeme && lb->length >= 2) {
                s->ffi_lib = (char*)malloc((size_t)lb->length - 1);
                memcpy(s->ffi_lib, lb->lexeme + 1, (size_t)lb->length - 2);
                s->ffi_lib[lb->length - 2] = 0;
                gmb_p_adv(p);
            }
        }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Alias") || gmb_p_check(p, GMB_TOK_Alias)) {
            gmb_p_adv(p);
            const gmb_token_t* al = gmb_p_cur(p);
            if (al && al->lexeme && al->length >= 2) {
                s->ffi_symbol = (char*)malloc((size_t)al->length - 1);
                memcpy(s->ffi_symbol, al->lexeme + 1, (size_t)al->length - 2);
                s->ffi_symbol[al->length - 2] = 0;
                gmb_p_adv(p);
            }
        }
        /* parametri (nome [AS tipo], ...) */
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            int cap = 0;
            if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                for (;;) {
                    int is_byref = 0;
                    int param_mode = 0;
                    /* ADR-MATRIX §3: IN/OUT/INOUT anche nei DECLARE NATIVE (parità col
                     * parser C#): OUT/INOUT valgono solo seguite da un NOME. */
                    {
                        const gmb_token_t* mc = gmb_p_cur(p);
                        if (gmb_p_tok_is_keyword(mc, "In")) {
                            /* `IN` può essere anche il NOME del parametro (`in() AS DOUBLE`):
                             * è un modificatore solo se seguito da un ALTRO nome. Prima veniva
                             * consumato sempre, quindi `DECLARE ... (in() AS DOUBLE)` e
                             * `SUB S(in() AS BYTE)` davano «statement non riconosciuto»
                             * (graphicsffi/gfx2d_*). */
                            const gmb_token_t* nx = gmb_p_peek(p, 1);
                            if (nx && nx->kind != GMB_TOK_As && nx->kind != GMB_TOK_Comma
                                && nx->kind != GMB_TOK_RightParen && nx->kind != GMB_TOK_LeftParen
                                && nx->kind != GMB_TOK_Equal) {
                                param_mode = 0;
                                gmb_p_adv(p);
                            }
                        } else if (mc && mc->kind == GMB_TOK_Identifier && mc->lexeme
                                   && (strncasecmp(mc->lexeme, "OUT", 3) == 0
                                       || strncasecmp(mc->lexeme, "INOUT", 5) == 0)) {
                            const gmb_token_t* nx = gmb_p_peek(p, 1);
                            if (nx && nx->kind != GMB_TOK_As && nx->kind != GMB_TOK_Comma
                                && nx->kind != GMB_TOK_RightParen && nx->kind != GMB_TOK_LeftParen
                                && nx->kind != GMB_TOK_Equal) {
                                param_mode = (strncasecmp(mc->lexeme, "INOUT", 5) == 0) ? 2 : 1;
                                gmb_p_adv(p);
                            }
                        }
                    }
                    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Byref") || gmb_p_check(p, GMB_TOK_ByRef)) { is_byref = 1; gmb_p_adv(p); }
                    else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Byval")) gmb_p_adv(p);
                    const gmb_token_t* prm = gmb_p_cur(p);
                    if (prm && prm->lexeme) {
                        if (s->n_params >= cap) {
                            cap = cap ? cap * 2 : 4;
                            s->func_params = (char**)realloc(s->func_params, (size_t)cap * sizeof(char*));
                            s->param_byref = (uint8_t*)realloc(s->param_byref, (size_t)cap);
                            s->param_is_array = (uint8_t*)realloc(s->param_is_array, (size_t)cap); s->param_ndims = (uint8_t*)realloc(s->param_ndims, (size_t)cap); s->param_mode = (uint8_t*)realloc(s->param_mode, (size_t)cap);
                            s->param_types = (char**)realloc(s->param_types, (size_t)cap * sizeof(char*));
                        }
                        char* b = (char*)malloc((size_t)prm->length + 1);
                        memcpy(b, prm->lexeme, (size_t)prm->length); b[prm->length] = 0;
                        s->func_params[s->n_params] = b;
                        s->param_byref[s->n_params] = (uint8_t)is_byref;
                        s->param_is_array[s->n_params] = 0; s->param_ndims[s->n_params] = 0; s->param_mode[s->n_params] = (uint8_t)param_mode;
                        s->param_types[s->n_params] = NULL;
                        s->n_params++;
                    }
                    gmb_p_adv(p);
                    /* Array BYREF (2026-09-28): `name()` — solo forma vuota (nessun
                     * ridimensionamento qui, e' un parametro FFI, non una DIM). Vedi
                     * emit_ffi_wrapper/gmb_c99_emit.c e AiWiki Bugs/(open) -
                     * FFI-Marshalling-Limitato-No-BYREF-Array-Struct.md punto (1). */
                    if (gmb_p_check(p, GMB_TOK_LeftParen)) {
                        gmb_p_adv(p);
                        gmb_p_match(p, GMB_TOK_RightParen);
                        if (s->n_params > 0) {
                            s->param_is_array[s->n_params - 1] = 1;
                            s->param_byref[s->n_params - 1] = 1;
                        }
                    }
                    int matrix_handled = 0;
                    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                        gmb_p_adv(p);
                        const gmb_token_t* ty = gmb_p_cur(p);
                        /* `AS ARRAY OF T`: tipo ELEMENTO T (rank dagli assi/`()`). */
                        if (ty && (ty->kind == GMB_TOK_Array || gmb_p_tok_is_keyword(ty, "Array"))) {
                            ty = gmb_consume_array_of(p);
                            if (s->n_params > 0) {
                                s->param_is_array[s->n_params - 1] = 1;
                                if (!s->param_ndims[s->n_params - 1]) s->param_ndims[s->n_params - 1] = 1;
                            }
                        }
                        if (ty && ty->kind == GMB_TOK_Matrix && s->n_params > 0) {
                            /* DECLARE ... (X AS MATRIX OF (I1, …, In, V), ...): il parametro
                             * FFI riceve il puntatore flat (row-major) e il tipo valore V. */
                            const gmb_token_t* vt = NULL;
                            int axes = gmb_parse_matrix_of(p, &vt);
                            if (axes >= 1 && vt) {
                                s->param_is_array[s->n_params - 1] = 1;
                                s->param_ndims[s->n_params - 1] = (uint8_t)axes;
                                if (!s->param_types[s->n_params - 1]) {
                                    char* tb = (char*)malloc((size_t)vt->length + 1);
                                    memcpy(tb, vt->lexeme, (size_t)vt->length); tb[vt->length] = 0;
                                    s->param_types[s->n_params - 1] = tb;
                                }
                                matrix_handled = 1;
                            }
                        }
                        if (ty && ty->lexeme && s->n_params > 0 && !s->param_types[s->n_params - 1]) {
                            char* tb = (char*)malloc((size_t)ty->length + 1);
                            memcpy(tb, ty->lexeme, (size_t)ty->length); tb[ty->length] = 0;
                            s->param_types[s->n_params - 1] = tb;
                        }
                        /* Callback FFI (2026-09-29): `AS FUNCTION(...) AS T`/`AS SUB(...)` come
                         * parametro di una DECLARE ... LIB — vedi AiWiki Bugs/(open) - FFI-
                         * Estensioni-Possibili-BYREF-Struct-Callback.md §3bis. Stessa logica del
                         * parser generico (parametro FUNCTION di una FUNCTION/SUB utente, sopra
                         * in questo stesso file), duplicata qui perché DECLARE ... LIB ha un
                         * parser dei parametri COMPLETAMENTE separato (più semplice: niente
                         * OPTIONAL/PARAMARRAY/default). */
                        if (ty && ty->lexeme && s->n_params > 0
                            && (gmb_p_tok_is_keyword(gmb_p_peek(p, 0), "Function")
                                || ty->kind == GMB_TOK_Function)) {
                            gmb_p_adv(p);   /* FUNCTION/SUB */
                            int fn_sig_mask = 0, fn_kind_mask = 0;
                            int fn_arr_mask = 0;
                            int fn_arity = gmb_p_fn_sig(p, &fn_sig_mask, &fn_kind_mask, &fn_arr_mask);
                            /* Allocazione a dimensione FISSA 16 (non s->n_params, che al
                             * momento di QUESTA prima allocazione riflette solo QUANTI
                             * parametri sono stati letti FINORA — se il parametro FUNCTION non
                             * e' il PRIMO della lista, o se altri lo seguono, un'allocazione
                             * dimensionata sul valore corrente sarebbe troppo piccola per gli
                             * indici successivi → scrittura fuori dai limiti che corrompe
                             * memoria adiacente, osservato realmente: un secondo parametro
                             * scalare letto come garbage "is function-typed". 16 e' un margine
                             * di sicurezza oltre il limite di 8 argomenti FFI già imposto a
                             * valle in emit_ffi_wrapper). */
                            if (!s->param_fn_arity) {
                                s->param_fn_arity = (int*)calloc(16, sizeof(int));
                                s->param_fn_str_mask = (int*)calloc(16, sizeof(int));
                                s->param_fn_kind_mask = (int*)calloc(16, sizeof(int));
                                s->param_fn_arr_mask = (int*)calloc(16, sizeof(int));
                                for (int z = 0; z < 16; z++) s->param_fn_arity[z] = -1;
                            }
                            if (s->n_params <= 16) {
                                s->param_fn_arity[s->n_params - 1] = fn_arity;
                                s->param_fn_str_mask[s->n_params - 1] = fn_sig_mask;
                                s->param_fn_kind_mask[s->n_params - 1] = fn_kind_mask;
                                s->param_fn_arr_mask[s->n_params - 1] = fn_arr_mask;
                            }
                            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                                if (!s->param_fn_has_ret) s->param_fn_has_ret = (uint8_t*)calloc(16, sizeof(uint8_t));
                                if (s->n_params <= 16) s->param_fn_has_ret[s->n_params - 1] = 1;
                                gmb_p_adv(p);
                                const gmb_token_t* rtf = gmb_p_cur(p);
                                if (gmb_p_tok_is_string_type_name(rtf)) {
                                    if (!s->param_fn_ret_str) s->param_fn_ret_str = (uint8_t*)calloc(16, sizeof(uint8_t));
                                    if (s->n_params <= 16) s->param_fn_ret_str[s->n_params - 1] = 1;
                                }
                                if (gmb_p_tok_is_scalar_type_name(rtf)) {
                                    if (!s->param_fn_ret_int) s->param_fn_ret_int = (uint8_t*)calloc(16, sizeof(uint8_t));
                                    if (s->n_params <= 16) s->param_fn_ret_int[s->n_params - 1] = 1;
                                }
                                if (rtf && (rtf->kind == GMB_TOK_Array || gmb_p_tok_is_keyword(rtf, "Array"))) {
                                    if (!s->param_fn_ret_arr) s->param_fn_ret_arr = (uint8_t*)calloc(16, sizeof(uint8_t));
                                    if (s->n_params <= 16) s->param_fn_ret_arr[s->n_params - 1] = 1;
                                }
                                if (rtf) gmb_p_adv(p);
                            }
                        } else if (!matrix_handled) {
                            gmb_p_adv(p);
                        }
                    }
                    if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                }
            }
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
            gmb_p_adv(p);
            const gmb_token_t* rt = gmb_p_cur(p);
            if (rt && rt->lexeme) {
                s->func_ret_type = (char*)malloc((size_t)rt->length + 1);
                memcpy(s->func_ret_type, rt->lexeme, (size_t)rt->length); s->func_ret_type[rt->length] = 0;
                int ret_matrix = 0;
                if (rt->kind == GMB_TOK_Matrix) {
                    const gmb_token_t* vt = NULL;
                    int axes = gmb_parse_matrix_of(p, &vt);
                    if (axes >= 1 && vt) {
                        s->func_ret_ndims = axes;
                        char* rb = (char*)malloc((size_t)vt->length + 1);
                        memcpy(rb, vt->lexeme, (size_t)vt->length); rb[vt->length] = 0;
                        free(s->func_ret_type);
                        s->func_ret_type = rb;
                        ret_matrix = 1;
                    }
                }
                /* AS FUNCTION(params) AS <ret>: cattura la FIRMA del block restituito
                 * (arity/mask/ret) — serve a `DIM b = Mk()` (firma inferita): senza,
                 * GK_FUNCVAL restava arity 0 → `EVAL(b, 5)` "numero argomenti errato". */
                if (gmb_keyword_tok(s->func_ret_type) == GMB_TOK_Function) {
                    gmb_p_adv(p);
                    if (gmb_p_check(p, GMB_TOK_LeftParen))
                        s->func_val_arity = gmb_p_fn_sig(p, &s->func_val_str_mask, NULL, NULL);
                    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                        gmb_p_adv(p);
                        const gmb_token_t* rr = gmb_p_cur(p);
                        if (gmb_p_tok_is_string_type_name(rr))
                            s->func_val_ret_is_str = 1;
                        if (rr && gmb_keyword_tok(rr) == GMB_TOK_Array) s->func_val_ret_is_arr = 1;
                    }
                } else if (!ret_matrix) {
                    gmb_p_adv(p);
                }
            }
        }
        gmb_p_skip_eol(p);
        return s;
    }
    /* comandi vari: RANDOMIZE/.../SCROLL/FONT/USE/TCP/SSL/UDP/OPTION — nessun bytecode */
        if (t->kind == GMB_TOK_Const) { gmb_p_adv(p); return gmb_p_parse_const(p, line); }
    /* MEMORYMAP comandi (2026-09-15): prima cadevano nel catch-all REM (no-op silenzioso
     * su C99Native). MMAPPOKE handle, offset, value; MMAPWRITEBYTES handle, offset,
     * srcArray() [, count]. file_num=handle, file_pos=offset, print_args[0]=value/src,
     * print_args[1]=count. */
    if (t->kind == GMB_TOK_MemoryMapPoke || t->kind == GMB_TOK_MemoryMapWriteBytes) {
        bool is_poke = t->kind == GMB_TOK_MemoryMapPoke;
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(is_poke ? GMB_STMT_MMAP_POKE : GMB_STMT_MMAP_WRITEBYTES, line);
        s->file_num = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) s->file_pos = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) {
            s->print_args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
            s->print_args[0] = gmb_p_parse_expr(p);
            s->nprint = 1;
            if (!is_poke && gmb_p_match(p, GMB_TOK_Comma)) { s->print_args[1] = gmb_p_parse_expr(p); s->nprint = 2; }
        }
        return s;
    }
    /* OPTION [STRICT ON|OFF | ...]: direttiva di modulo. `OPTION STRICT ON` chiede che ogni
     * nome usato sia dichiarato (nessuna variabile implicita): il flag viaggia fino all'emitter,
     * che rifiuta i nomi non risolti con lo stesso messaggio del linker C#. Le altre forme
     * restano no-op. Prima la riga veniva solo saltata (STRICT ignorato → su questa gamba un
     * programma "strict" compilava e stampava 0 dove le altre gambe davano errore). */
    if (t->kind == GMB_TOK_Option) {
        gmb_p_adv(p);
        /* STRICT ON|OFF */
        int is_strict_tok = 0;
        const gmb_token_t* nt = gmb_p_cur(p);
        if (nt && (nt->kind == GMB_TOK_Strict
                   || gmb_p_tok_is_keyword(nt, "Strict"))) {
            is_strict_tok = 1;
            gmb_p_adv(p);
            const gmb_token_t* on = gmb_p_cur(p);
            if (on && (on->kind == GMB_TOK_On || gmb_p_tok_is_keyword(on, "On"))) {
                p->option_strict = 1;
                gmb_p_adv(p);
            } else if (on && (on->kind == GMB_TOK_Off || gmb_p_tok_is_keyword(on, "Off"))) {
                p->option_strict = 0;
                gmb_p_adv(p);
            }
        }
        (void)is_strict_tok;
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    /* AUDIO: SOUND/PLAY/FILTER ... — no-op headless (parità emitter C# SoundStmt/PlayStmt/
     * SetFilterStmt: "runtime audio esterno non portato"). Salta la riga. Prima erano in
     * rem_vari → "comando 'X' non ancora supportato" (7 sample Audio). */
    if (t->kind == GMB_TOK_Sound || t->kind == GMB_TOK_Play || t->kind == GMB_TOK_Filter || t->kind == GMB_TOK_Scroll) {
        gmb_p_adv(p);
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
if (gmb_tk_rem_vari_in(t->kind)) {
        /* Questi 53 token NON sono dispatchati prima di questo punto: cadevano nel ramo
         * eat che li mangiava come REM (codice silenziosamente perso). Errore esplicito,
         * mai rimozione silenziosa (decisione utente 2026-09-17, anti-silent). */
        /* nome_word: primo token della riga (il comando) come testo, senza il resto. */
        char wbuf[64];
        int wl = t->length < (int)sizeof(wbuf) - 1 ? t->length : (int)sizeof(wbuf) - 1;
        memcpy(wbuf, t->lexeme, wl); wbuf[wl] = 0;
        char msg[160];
        snprintf(msg, sizeof(msg), "comando '%s' non ancora supportato dal compilatore nativo", wbuf);
        gmb_p_set_err(p, msg, line);
        return NULL;
    }
    if (t->kind == GMB_TOK_Match) {
        /* MATCH <expr> ... CASE <pattern> => body ... (Rust-style) */
        gmb_p_adv(p);
        gmb_expr_t* me = gmb_p_parse_expr(p);
        if (me) gmb_expr_free(me);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_REM, line);
        for (;;) {
            gmb_p_skip_eol(p);
            if (gmb_p_at_end(p)) break;
            const gmb_token_t* tt = gmb_p_cur(p);
            if (!(tt->kind == GMB_TOK_Case || gmb_p_tok_is_keyword(tt, "Case"))) break;
            gmb_p_adv(p);
            /* pattern fino a '=>' o fine riga */
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)
                   && !gmb_p_check(p, GMB_TOK_FatArrow) && !gmb_p_check(p, GMB_TOK_Arrow)) gmb_p_adv(p);
            gmb_p_match(p, GMB_TOK_FatArrow); gmb_p_match(p, GMB_TOK_Arrow);
            /* body fino al prossimo CASE o al terminatore del blocco esterno */
            for (;;) {
                gmb_p_skip_eol(p);
                if (gmb_p_at_end(p)) break;
                if (gmb_p_check(p, GMB_TOK_Case) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Case")) break;
                if (gmb_p_check(p, GMB_TOK_End) && gmb_p_peek(p, 1)) break;
                bool ml2 = false;
                gmb_stmt_t* st = gmb_p_parse_stmt(p, &ml2);
                if (!st) { if (p->error) break; gmb_p_adv(p); continue; }
                gmb_stmt_free(st);
            }
        }
        return s;
    }
    /* direttiva #REGION/#ENDREGION */
    if (t->kind == GMB_TOK_Hash) {
        gmb_p_consume_statement(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    /* comandi vari: RANDOMIZE/WAITKEY/STATUSBAR/KEYGEN/BOX/AFILL/TOGGLE/SCREEN/DYNAMIC/OPERATOR/attribute — nessun bytecode */
    if (t->kind == GMB_TOK_AFill) { gmb_p_adv(p); return gmb_p_parse_afill(p, line); }
    if (t->kind == GMB_TOK_ACopy) { gmb_p_adv(p); return gmb_p_parse_acopy(p, line); }
    /* BOX/DYNAMIC nudi: NON supportati dal compilatore nativo (box drawing e dynamic
     * scoping vivono solo nel mondo C#) — errore esplicito, mai REM silenzioso. */
    if (t->kind == GMB_TOK_Box) {
        /* BOX x1,y1,x2,y2 [SINGLE|DOUBLE|...]: cornice text-mode. L'emitter C# la emette
         * come NO-OP (C99CodeEmitter.cs:5852 "WINDOW BoxStmt: no-op (non implementato)") →
         * parità: salta la riga. Prima il parser C dava errore esplicito. */
        gmb_p_adv(p);
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    if (t->kind == GMB_TOK_Dynamic) {
        /* DYNAMIC v [AS STRING] [= expr] — scope dinamico (spec Variables-Scope §2.5).
         * Riusa il nodo GMB_STMT_DIM con dim_is_dynamic=1: il nome NON è un locale/globale C,
         * vive nel runtime gmb_dscope per NOME (parità emitter C#). Vedi gmb_c99_emit_dynamic.c. */
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DIM, line);
        s->dim_is_dynamic = 1;
        const gmb_token_t* v = gmb_p_cur(p);
        /* Accetta anche token "keyword" come nome (xBase è permissivo: es. `record` è un
         * TokenKind ma è un nome legittimo). Prima restava dim_var NULL → crash. */
        if (v && v->lexeme && v->length > 0 && v->kind != GMB_TOK_Newline && v->kind != GMB_TOK_Colon
            && v->kind != GMB_TOK_Comma && v->kind != GMB_TOK_Equal) {
            s->dim_var = (char*)malloc((size_t)v->length + 1);
            memcpy(s->dim_var, v->lexeme, (size_t)v->length);
            s->dim_var[v->length] = 0;
            gmb_p_adv(p);
        }
        if (gmb_p_check(p, GMB_TOK_As) || gmb_p_tok_is_keyword(gmb_p_cur(p), "As")) {
            gmb_p_adv(p);
            const gmb_token_t* at = gmb_p_cur(p);
            if (at && at->lexeme) {
                char tb[32]; size_t tl = (size_t)at->length < 31 ? (size_t)at->length : 31;
                memcpy(tb, at->lexeme, tl); tb[tl] = 0;
                s->dim_type = (char*)malloc(tl + 1); memcpy(s->dim_type, tb, tl + 1);
                s->dim_type_tk = (at->kind == GMB_TOK_Identifier || at->kind == GMB_TOK_Unknown) ? 0 : (int)at->kind;
                gmb_p_adv(p);
            }
        }
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Equal)
               && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        if (gmb_p_match(p, GMB_TOK_Equal)) s->dim_init = gmb_p_parse_expr(p);
        return s;
    }
    if ((t->kind == GMB_TOK_LessThan && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof)) {
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p); gmb_p_match(p, GMB_TOK_GreaterThan);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    /* RANDOMIZE [seed]: semina il PRNG (gmb_rnd_seed) — seed opzionale */
    if (t->kind == GMB_TOK_Randomize) {
        int has_seed = !gmb_p_at_end(p) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline
            && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof;
        gmb_p_adv(p);
        gmb_stmt_t* rz = gmb_stmt_new(GMB_STMT_RANDOMIZE, line);
        if (has_seed) rz->value = gmb_p_parse_expr(p);
        return rz;
    }
    /* WAITKEY: attende un tasto (parity PCodeVM/IL — NON è no-op). Gli altri
     * token del gruppo legacy (STATUSBAR/KEYGEN/KEYCLOSE/KEYIMPORT/KEYEXPORT) restano REM. */
    if (t->kind == GMB_TOK_WaitKey) {
        gmb_p_adv(p);
        gmb_stmt_t* wk = gmb_stmt_new(GMB_STMT_WAITKEY, line);
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
        return wk;
    }
    /* SCROLLMODE mode: regione scroll (parity PCode CONSOLE_SCROLLMODE) — NON REM. */
    if (t->kind == GMB_TOK_ScrollMode) { gmb_p_adv(p); return gmb_p_parse_scrollmode(p, line); }
    if (t->kind == GMB_TOK_StatusBar) {
        gmb_p_consume_statement(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    /* SAVE SCREEN <nome>: snapshot nominato (parity SaveScreenStmt C#) — NON REM:
     * prima mangiato in silenzio, ora implementato (runtime gmb_console_savescreen_named). */
    if (t->kind == GMB_TOK_Save && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Screen) {
        gmb_p_adv(p); gmb_p_adv(p);
        return gmb_p_parse_savescreen(p, line);
    }
    /* Clipper TUI: @ r, c SAY expr / @ r, c GET var [TYPE ...] — nodo dedicato perché
     * l'emitter fallisce ESPLICITO (form engine solo C#); mai REM silenzioso. */
    if (t->kind == GMB_TOK_AtSign) {
        gmb_p_adv(p);
        return gmb_p_parse_atform(p, line);
    }
    /* comandi filesystem: KILL/MKDIR/CHDIR/RMDIR <path>[, TRUE] (FASE 2.2).
     * ERASE e' alias di KILL (parita' C#: StdLib.Io.cs TokenKind.Erase →
     * ParseKillStatement) — NON l'ERASE-array di QBASIC. */
    if (t->kind == GMB_TOK_Kill || t->kind == GMB_TOK_Erase || t->kind == GMB_TOK_MkDir || t->kind == GMB_TOK_ChDir || t->kind == GMB_TOK_RmDir) {
        gmb_stmt_kind_t k = (t->kind == GMB_TOK_Kill || t->kind == GMB_TOK_Erase) ? GMB_STMT_KILL
                          : t->kind == GMB_TOK_MkDir ? GMB_STMT_MKDIR
                          : t->kind == GMB_TOK_ChDir ? GMB_STMT_CHDIR : GMB_STMT_RMDIR;
        gmb_p_adv(p);
        gmb_stmt_t* fs = gmb_stmt_new(k, line);
        fs->value = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) fs->cond = gmb_p_parse_expr(p);
        return fs;
    }
    /* direttive di modulo: IMPORT [NS [(...)] [AS X]|.*] e REQUIRE — consumate e ignorate
     * (sono direttive di importazione/namespace, non generano codice) */
    if (t->kind == GMB_TOK_Import || t->kind == GMB_TOK_Require) {
        gmb_p_adv(p);
        int depth = 0, had_as = 0;
        char first_ns[64]; int first_ns_len = 0;
        /* ns FISICO accumulato (segmenti + '.') fino a '(' (selettore) o AS — serve per
         * registrare alias→ns reale (es. IMPORT TRADING.PORTFOLIO AS TP). */
        char collected[96]; int cl = 0;
        char ns_at_as[96]; ns_at_as[0] = 0;
        char mems[16][48]; int n_mem = 0; int mem_len[16];
        while (!gmb_p_at_end(p)) {
            const gmb_token_t* tt = gmb_p_cur(p);
            if (tt->kind == GMB_TOK_Newline && depth == 0) break;
            if (first_ns_len == 0 && tt->lexeme && tt->length > 0
                && tt->lexeme[0] != '(' && tt->lexeme[0] != '.' && tt->lexeme[0] != '*'
                && tt->length < 63) {
                memcpy(first_ns, tt->lexeme, (size_t)tt->length); first_ns[tt->length] = 0;
                first_ns_len = (int)tt->length;
            }
            if (tt->kind == GMB_TOK_As && depth == 0 && p->import_aliases_n < 16) {
                /* IMPORT <NS> AS <ALIAS>: registra l'alias per il qualificato <ALIAS>.FUNC */
                had_as = 1;
                if (cl > 0 && cl < 95) { memcpy(ns_at_as, collected, (size_t)cl); ns_at_as[cl] = 0; }
                gmb_p_adv(p);
                const gmb_token_t* al = gmb_p_cur(p);
                if (al && al->kind != GMB_TOK_Newline && al->kind != GMB_TOK_Eof && al->length < 32) {
                    memcpy(p->import_aliases[p->import_aliases_n], al->lexeme, (size_t)al->length);
                    p->import_aliases[p->import_aliases_n][al->length] = 0;
                    if (cl > 0 && cl < 95) { memcpy(p->import_aliases_ns[p->import_aliases_n], collected, (size_t)cl); p->import_aliases_ns[p->import_aliases_n][cl] = 0; }
                    else { p->import_aliases_ns[p->import_aliases_n][0] = 0; }
                    p->import_aliases_n++;
                }
                continue;
            }
            if (tt->kind == GMB_TOK_LeftParen) { depth++; }
            else if (tt->kind == GMB_TOK_RightParen) { depth--; }
            else if (depth == 1 && n_mem < 16 && tt->kind != GMB_TOK_Comma && tt->lexeme
                     && tt->length > 0 && tt->length < 47) {
                if (tt->kind != GMB_TOK_As && tt->kind != GMB_TOK_Alias) {
                    memcpy(mems[n_mem], tt->lexeme, (size_t)tt->length);
                    mem_len[n_mem] = (int)tt->length; n_mem++;
                }
            }
            else if (depth == 0 && cl < 94 && tt->lexeme && tt->length > 0) {
                char c0 = tt->lexeme[0];
                if (c0 == '.') { if (cl > 0) collected[cl++] = '.'; }
                else if (c0 != '*' && c0 != ',' && c0 != '(' && tt->kind != GMB_TOK_As
                         && tt->kind != GMB_TOK_Import && tt->kind != GMB_TOK_Require
                         && tt->kind != GMB_TOK_With && tt->kind != GMB_TOK_Alias) {
                    int k = (int)tt->length; if (cl + k > 94) k = 94 - cl;
                    memcpy(collected + cl, tt->lexeme, (size_t)k); cl += k; collected[cl] = 0;
                }
            }
            gmb_p_adv(p);
        }
        /* IMPORT <NS> senza AS: il namespace stesso diventa un qualificatore valido
         * (es. Calcoli.Somma / Geometria.Punto) — senza alias il parser non lo riconosceva. */
        if (!had_as && first_ns_len > 0 && p->import_aliases_n < 16) {
            int dup = 0;
            for (int ai = 0; ai < p->import_aliases_n; ai++)
                if (strcasecmp(p->import_aliases[ai], first_ns) == 0) { dup = 1; break; }
            if (!dup) {
                memcpy(p->import_aliases[p->import_aliases_n], first_ns, (size_t)first_ns_len);
                p->import_aliases[p->import_aliases_n][first_ns_len] = 0;
                p->import_aliases_ns[p->import_aliases_n][0] = 0; /* alias==ns, risolto da gmb_ns_names */
                p->import_aliases_n++;
            }
        }
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_IMPORT, t->line);
        s->imp_ns = strdup(ns_at_as[0] ? ns_at_as : (collected[0] ? collected : (first_ns_len ? first_ns : "")));
        if (had_as && p->import_aliases_n > 0)
            s->imp_alias = strdup(p->import_aliases[p->import_aliases_n - 1]);
        if (n_mem > 0) {
            s->imp_members = (char**)calloc((size_t)n_mem, sizeof(char*));
            for (int mi = 0; mi < n_mem; mi++) { s->imp_members[mi] = strndup(mems[mi], (size_t)mem_len[mi]); s->n_imp_members++; }
        }
        return s;
    }
    /* NAMESPACE <nome>: scope di un modulo (.inc). Consumato come no-op a livello di
     * statement: il C99Native è monofile e registra le classi col nome nudo — il prefisso
     * qualificato (NS.Cls) viene risolto al nome nudo in fase di tipo (DIM/NEW). La
     * direttiva termina a fine file (nessun END NAMESPACE nella convenzione .inc). */
    if (t->kind == GMB_TOK_Namespace) {
        gmb_p_adv(p);
        /* 2026-09-24: traccia il namespace corrente (parità _currentNamespace C#) per lo
         * stamp sulle definizioni FUNCTION/DECLARE (check shadowing builtin). Cattura il
         * nome anche dotted (TRADING.PORTFOLIO); riga vuota = reset a globale.
         * Logica in gmb_parser_utils.c (file piccolo, non fa crescere questo). */
        gmb_p_capture_namespace(p);
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)
               && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_REM, t->line);
        return s;
    }
    /* LIBRARY "nome" (o `LIBRARY nome`): direttiva di modulo -> produce una libreria
     * condivisa (.gmblib), non un eseguibile. Consumata come REM; il nome va in p->lib_name
     * e da li' in gmb_program_t.library_name (letto dall'emitter C). */
    if (t->kind == GMB_TOK_Library) {
        /* LIBRARY e' una direttiva di PROGRAMMA: valida SOLO come primo comando (i commenti
         * possono precederla). Vietata dopo IMPORT/REQUIRE o qualsiasi altro statement. */
        if (p->saw_stmt || p->lib_name[0]) {
            gmb_p_set_err(p, "LIBRARY deve essere il primo comando del programma (prima di IMPORT/REQUIRE e di ogni altro comando; i commenti possono precederlo)", t->line);
            return NULL;
        }
        gmb_p_adv(p);
        const gmb_token_t* nt = gmb_p_cur(p);
        if (nt && nt->lexeme && nt->length > 0
            && nt->kind != GMB_TOK_Newline && nt->kind != GMB_TOK_Colon) {
            const char* src = nt->lexeme;
            size_t L = (size_t)nt->length;
            if (L >= 2 && src[0] == '"' && src[L-1] == '"') { src++; L -= 2; }
            if (L > sizeof(p->lib_name) - 1) L = sizeof(p->lib_name) - 1;
            memcpy(p->lib_name, src, L); p->lib_name[L] = 0;
            gmb_p_adv(p);
        }
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)
               && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_REM, t->line);
    }
    if (t->kind == GMB_TOK_Rem) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_REM, line);
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
        return s;
    }
    if (t->kind == GMB_TOK_If) { gmb_p_adv(p); return gmb_p_parse_if(p, line, multiline); }
    if (t->kind == GMB_TOK_Using) { gmb_p_adv(p); return gmb_p_parse_using(p, line, multiline); }
    if (t->kind == GMB_TOK_Select) { gmb_p_adv(p); return gmb_p_parse_select_case(p, line, multiline); }
    if (t->kind == GMB_TOK_On) { gmb_p_adv(p); return gmb_p_parse_on(p, line, multiline); }
    if (t->kind == GMB_TOK_Try) { gmb_p_adv(p); return gmb_p_parse_try_catch(p, line, multiline); }
    if (t->kind == GMB_TOK_Error) { gmb_p_adv(p); return gmb_p_parse_error(p, line); }
    if (t->kind == GMB_TOK_Resume) {
        /* RESUME | RESUME NEXT | RESUME <label> (statement standalone dentro un
           handler ON ERROR; parità C# VisitResumeStatement: modo 1/0/addr) */
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ON_ERROR, line);
        s->on_kind = "ERROR_RESUME";
        s->flag = 1;   /* default: RESUME (retry, modo 1) */
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Next") || gmb_p_check(p, GMB_TOK_Next)) { gmb_p_adv(p); s->flag = 0; }   /* RESUME NEXT (modo 0) */
        else if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Eof && gmb_p_cur(p)->kind != GMB_TOK_Colon) {
            s->flag = 2;   /* RESUME <label>: indirizzo patchato */
            s->target_label = (char*)malloc((size_t)gmb_p_cur(p)->length + 1);
            memcpy(s->target_label, gmb_p_cur(p)->lexeme, (size_t)gmb_p_cur(p)->length);
            s->target_label[gmb_p_cur(p)->length] = 0;
            gmb_p_adv(p);
        }
        return s;
    }
    if (t->kind == GMB_TOK_Throw) { gmb_p_adv(p); return gmb_p_parse_throw(p, line); }
    if (t->kind == GMB_TOK_Data && !(gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen)) { gmb_p_adv(p); return gmb_p_parse_data(p, line); }
    if (t->kind == GMB_TOK_Read) { gmb_p_adv(p); return gmb_p_parse_read(p, line); }
    if (t->kind == GMB_TOK_Restore) {
        /* RESTORE SCREEN <nome>: snapshot nominato (parity RestoreScreenStmt) — NON REM.
         * Resta il DATA RESTORE per la forma senza Screen. */
        if (gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Screen) {
            gmb_p_adv(p); gmb_p_adv(p);
            return gmb_p_parse_restorescreen(p, line);
        }
        gmb_p_adv(p); return gmb_p_parse_restore(p, line);
    }
    if (t->kind == GMB_TOK_Redim) { gmb_p_adv(p); return gmb_p_parse_redim(p, line); }
    /* ERASE: gestito sopra nel ramo filesystem come alias di KILL (GMB_STMT_KILL).
     * gmb_p_parse_erase (ERASE-array QBASIC) non e' piu' raggiungibile: resta solo
     * come codice morto da rimuovere in una sessione dedicata. */
    if (t->kind == GMB_TOK_Swap) { gmb_p_adv(p); return gmb_p_parse_swap(p, line); }
    if (t->kind == GMB_TOK_Stop) { gmb_p_adv(p); return gmb_p_parse_stop(p, line); }
    /* RUN [<file>] — RUN non è (ancora) una keyword del lexer C: arriva come Identifier,
     * quindi si accetta anche per grafia. Il guard su `=` mantiene l'uso come variabile
     * (`run = 1`), già presente in alcuni MiniTest TUI. */
    if ((t->kind == GMB_TOK_Run || gmb_p_tok_is_soft_keyword(t, "RUN"))
        && !(gmb_p_peek(p, 1) && (gmb_p_peek(p, 1)->kind == GMB_TOK_Equal
                                  || gmb_p_peek(p, 1)->kind == GMB_TOK_InlineAssign))) {
        gmb_p_adv(p); return gmb_p_parse_run(p, line);
    }
    if (t->kind == GMB_TOK_Require) { gmb_p_adv(p); return gmb_p_parse_require(p, line); }
    /* EMBEDFILE "file" AS nome — direttiva compile-time (equivalente di @embedFile).
     * SOFT keyword: si attiva solo se la riga comincia con la parola E il token successivo
     * è un letterale stringa, così `embedfile` resta un nome di variabile utilizzabile. */
    if (gmb_p_tok_is_soft_keyword(t, "EMBEDFILE")
        && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_StringLiteral) {
        gmb_p_adv(p);
        return gmb_p_parse_embedfile(p, line);
    }
    /* ERRDEFER <stmt> — cleanup solo sul percorso d'errore (parità `errdefer` di Zig).
     * SOFT keyword: si attiva solo se la riga comincia con la parola e il token successivo
     * NON è un operatore di assegnazione (così `errdefer` resta un nome utilizzabile). */
    if (gmb_p_tok_is_soft_keyword(t, "ERRDEFER")
        && !(gmb_p_peek(p, 1) && (gmb_p_peek(p, 1)->kind == GMB_TOK_Equal
                                  || gmb_p_peek(p, 1)->kind == GMB_TOK_InlineAssign))) {
        gmb_p_adv(p);
        return gmb_p_parse_errdefer(p, line);
    }
    if (t->kind == GMB_TOK_Process) { gmb_p_adv(p); return gmb_p_parse_process(p, line); }
    if (t->kind == GMB_TOK_Yield) { gmb_p_adv(p); return gmb_p_parse_yield(p, line); }
    if (t->kind == GMB_TOK_AAdd) { gmb_p_adv(p); return gmb_p_parse_aadd(p, line); }
    if (t->kind == GMB_TOK_AIns) { gmb_p_adv(p); return gmb_p_parse_ains(p, line); }
    if (t->kind == GMB_TOK_ADel) { gmb_p_adv(p); return gmb_p_parse_adel(p, line); }
    if (t->kind == GMB_TOK_AShuffle) { gmb_p_adv(p); return gmb_p_parse_ashuffle(p, line); }
    if (t->kind == GMB_TOK_ASort) { gmb_p_adv(p); return gmb_p_parse_asort(p, line); }
    if (t->kind == GMB_TOK_AEval) {
        /* AEVAL <array>, <callback> come STATEMENT (non assegnato a una variabile):
         * prima "arr"/"blk" venivano parsati SOLO per consumare correttamente i token,
         * poi LIBERATI subito e la funzione ritornava un GMB_STMT_REM (no-op) — AEVAL
         * come statement non chiamava mai davvero il builtin, qualunque fosse la sintassi
         * del callback (SUB block, FN lambda, codeblock Clipper). Fix: costruisce la
         * stessa chiamata AEVAL(arr, callback) che funziona già in forma-espressione e la
         * scarta come GMB_STMT_EXPR (cc_expr + OP_POP, già generico — stesso schema già
         * usato per "&expr"/CALL come statement poco sotto). */
        gmb_p_adv(p);
        gmb_expr_t* arr = gmb_p_parse_expr(p);
        gmb_expr_t* blk = NULL;
        if (gmb_p_match(p, GMB_TOK_Comma)) blk = gmb_p_parse_expr(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_EXPR, line);
        if (arr && blk) {
            gmb_expr_t** args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
            args[0] = arr; args[1] = blk;
            s->expr = gmb_expr_call("AEVAL", args, 2, line);
        } else {
            if (arr) gmb_expr_free(arr);
            if (blk) gmb_expr_free(blk);
        }
        /* 2026-09-24: NON consumare fino a fine riga — un `: <stmt>` che segue il blocco
         * callback (`AEVAL arr, SUB(x){ ... } : PRINT`) è uno STATEMENT separato e va
         * lasciato al parser del blocco (prima veniva mangiato: la `: PRINT` di newline
         * dopo AEVAL/AFILTER/AREDUCE spariva → righe fuse nella stessa uscita, divergenza
         * vs IL/PCode/VM-C). Si ferma anche al ':' come PRINT/altri statement. */
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)
               && !gmb_p_at_end(p)) gmb_p_adv(p);
        return s;
    }
    if (t->kind == GMB_TOK_Cls) { gmb_p_adv(p); return gmb_p_parse_cls(p, line); }
    if (t->kind == GMB_TOK_Locate) { gmb_p_adv(p); return gmb_p_parse_locate(p, line); }
    if (t->kind == GMB_TOK_Color) { gmb_p_adv(p); return gmb_p_parse_color(p, line); }
    if (t->kind == GMB_TOK_Inkey) { gmb_p_adv(p); return gmb_p_parse_inkey(p, line); }
    /* GET tastiera nudo (`GET var$` → GMB_STMT_GETKEY, parity C# DispatchGetStatement che
     * di default va a CONSOLE.GET). `GET #` (IO.GET) escluso: il C non ha IO.GET, resta
     * al comportamento precedente (errore); DBF/ISAM/SPEED usano forme qualificate. */
    if (t->kind == GMB_TOK_Get && (!gmb_p_peek(p, 1) || gmb_p_peek(p, 1)->kind != GMB_TOK_Hash)) { gmb_p_adv(p); return gmb_p_parse_getkey(p, line); }
    if (t->kind == GMB_TOK_Caption) { gmb_p_adv(p); return gmb_p_parse_caption(p, line); }
    if (t->kind == GMB_TOK_Input) { gmb_p_adv(p); return gmb_p_parse_input(p, line); }
    if (t->kind == GMB_TOK_Set) { gmb_p_adv(p); return gmb_p_parse_set(p, line); }
    if (t->kind == GMB_TOK_SendKey) { gmb_p_adv(p); return gmb_p_parse_sendkey(p, line); }
    /* CLIP GET dest | CLIP SET expr | CLIP CLEAR (2026-09-21, gruppo Console). */
    if (t->kind == GMB_TOK_Clip) { gmb_p_adv(p); return gmb_p_parse_clip(p, line); }
    /* CCLS [colore] (2026-09-21, gruppo Console): clear + background opzionale. */
    if (t->kind == GMB_TOK_CCLS) {
        gmb_stmt_t* cs = gmb_stmt_new(GMB_STMT_CCLS, line);
        gmb_p_adv(p);
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p))
            cs->value = gmb_p_parse_expr(p);
        return cs;
    }
    /* CLOCATE riga, colonna (2026-09-21, gruppo Console, parity CLOCATE C#). */
    if (t->kind == GMB_TOK_CLocate) {
        gmb_stmt_t* cs = gmb_stmt_new(GMB_STMT_CLOCATE, line);
        gmb_p_adv(p);
        cs->value = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) cs->cond = gmb_p_parse_expr(p);
        return cs;
    }
    /* CSAVESCREEN nome | CRESTORESCREEN nome (2026-09-21): riusano gli snapshot
     * nominati SAVE/RESTORE SCREEN (stessi GMB_STMT_* + on_expr). */
    if (t->kind == GMB_TOK_CSaveScreen) {
        gmb_p_adv(p);
        return gmb_p_parse_savescreen(p, line);
    }
    if (t->kind == GMB_TOK_CRestoreScreen) {
        gmb_p_adv(p);
        return gmb_p_parse_restorescreen(p, line);
    }
    /* CPALETTE idx, r, g, b (2026-09-21, gruppo Console): palette entry numerica
     * (parity PCode PALETTE_SET; le sottoforme USE/DEFINE/... restano C#-only). */
    if (t->kind == GMB_TOK_CPalette) {
        gmb_stmt_t* cs = gmb_stmt_new(GMB_STMT_CPALETTE, line);
        gmb_p_adv(p);
        cs->value = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) cs->cond = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) cs->file_pos = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) cs->cond_mutex = gmb_p_parse_expr(p);
        return cs;
    }
    /* CURSOR ON|OFF | CURSOR SHAPE stile$ (2026-09-21, gruppo Console, parity C#
     * ParseCursorStatement). Nota: MOUSE CURSOR ha primo token MOUSE (altro ramo).
     * ON/OFF possono lessare come keyword o come Identifier: si confronta il lexeme
     * (slice non NUL-terminata → strncasecmp con length, mai strcmp). */
    if (t->kind == GMB_TOK_Cursor) {
        gmb_stmt_t* cs = gmb_stmt_new(GMB_STMT_CURSOR, line);
        gmb_p_adv(p);
        const gmb_token_t* ck = gmb_p_cur(p);
        if (ck && (ck->kind == GMB_TOK_Shape || gmb_p_tok_is_keyword(ck, "Shape"))) {
            gmb_p_adv(p);
            cs->cond = gmb_p_parse_expr(p);
        } else if (ck && (ck->kind == GMB_TOK_On || ck->kind == GMB_TOK_Off)) {
            int on = (ck->kind == GMB_TOK_On) ? 1 : 0;
            gmb_expr_t* ne = (gmb_expr_t*)calloc(1, sizeof(gmb_expr_t));
            ne->kind = GMB_EXPR_NUM; ne->line = line; ne->num = (double)on;
            cs->value = ne;
            gmb_p_adv(p);
        } else {
            cs->value = gmb_p_parse_expr(p);
        }
        return cs;
    }
    if (t->kind == GMB_TOK_Shell) { gmb_p_adv(p); return gmb_p_parse_shell(p, line); }
    if (t->kind == GMB_TOK_Open) { gmb_p_adv(p); return gmb_p_parse_open(p, line); }
    if (t->kind == GMB_TOK_Close) { gmb_p_adv(p); return gmb_p_parse_close(p, line); }
    if (t->kind == GMB_TOK_Seek) { gmb_p_adv(p); return gmb_p_parse_seek(p, line); }
    /* PUT #n, [record], source — comando IO file (speculare a emitter C# PutFileStmt;
     * sample HttpMultiPart salva i byte caricati; prima errore esplicito "non ancora
     * supportato"). GET # resta NON supportato qui (nessun emitter: mai drop silenzioso). */
    if (t->kind == GMB_TOK_Put) { gmb_p_adv(p); return gmb_p_parse_putfile(p, line); }
    /* GET #n, [record], dest — lettura di un record RANDOM nei campi di un UDT (speculare a PUT #). */
    if (t->kind == GMB_TOK_Get) { gmb_p_adv(p); return gmb_p_parse_getfile(p, line); }
    if (t->kind == GMB_TOK_Lock) { gmb_p_adv(p); return gmb_p_parse_lockfile(p, line, 0); }
    if (t->kind == GMB_TOK_Unlock) { gmb_p_adv(p); return gmb_p_parse_lockfile(p, line, 1); }
    /* FREAD come COMANDO (B): solo se NON e' la forma funzione FREAD(...) — quella la
     * consuma il parser di espressioni. */
    if (t->kind == GMB_TOK_FRead && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftParen)
    { gmb_p_adv(p); return gmb_p_parse_fread_into(p, line); }
    /* Loop-stack per EXIT nudo (canone 2026-09-30): push prima del parse del loop,
     * pop dopo — singolo punto, nessun early-return interno da bilanciare. */
    if (t->kind == GMB_TOK_For) { gmb_p_adv(p); if (p->n_loop_stack < 32) p->loop_stack[p->n_loop_stack++] = GMB_TOK_For; gmb_stmt_t* lr = gmb_p_parse_for(p, line, multiline); if (p->n_loop_stack > 0) p->n_loop_stack--; return lr; }
    if (t->kind == GMB_TOK_While) { gmb_p_adv(p); if (p->n_loop_stack < 32) p->loop_stack[p->n_loop_stack++] = GMB_TOK_While; gmb_stmt_t* lr = gmb_p_parse_while(p, line, multiline); if (p->n_loop_stack > 0) p->n_loop_stack--; return lr; }
    if (t->kind == GMB_TOK_Do) { gmb_p_adv(p); if (p->n_loop_stack < 32) p->loop_stack[p->n_loop_stack++] = GMB_TOK_Do; gmb_stmt_t* lr = gmb_p_parse_do(p, line, multiline); if (p->n_loop_stack > 0) p->n_loop_stack--; return lr; }
    if (t->kind == GMB_TOK_Tick) { gmb_p_adv(p); if (p->n_loop_stack < 32) p->loop_stack[p->n_loop_stack++] = GMB_TOK_Do; gmb_stmt_t* lr = gmb_p_parse_tick(p, line, multiline); if (p->n_loop_stack > 0) p->n_loop_stack--; return lr; }
    if (t->kind == GMB_TOK_Async && gmb_p_peek(p, 1) && (gmb_p_peek(p, 1)->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Sub"))) {
        gmb_p_adv(p);   /* ASYNC */
        t = gmb_p_cur(p);
    }
    if (t->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(t, "Sub")
        || t->kind == GMB_TOK_Generator) {
        /* SUB/FN/FUNCTION lexano come GMB_TOK_Function: SUB si distingue dal lessema */
        int kw_is_sub = gmb_p_tok_is_soft_keyword(t, "sub");
        int is_gen = (t->kind == GMB_TOK_Generator);
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_FUNCTION, line);
        s->is_generator = is_gen;
        s->is_sub = kw_is_sub;
        /* 2026-09-24: namespace di definizione (check shadowing builtin in emissione). */
        if (p->cur_ns[0]) { s->func_ns = strdup(p->cur_ns); }
        const gmb_token_t* fn = gmb_p_cur(p);
        if (fn) { s->func_name = (char*)malloc((size_t)fn->length + 1); memcpy(s->func_name, fn->lexeme, (size_t)fn->length); s->func_name[fn->length] = 0; gmb_p_adv(p); }
        /* generico: Nome<T> — i '<' '>' lexano come operatori di confronto */
        if (gmb_p_check(p, GMB_TOK_LessThan)) { while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p); gmb_p_match(p, GMB_TOK_GreaterThan); }
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            int cap = 0;
            if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                for (;;) {
                    /* BYVAL/BYREF/OPTIONAL/PARAMARRAY PRIMA del nome — BYREF/PARAMARRAY vanno
                     * registrati per il codegen. Bug corretto (2026-08-31, audit PARAMARRAY/
                     * OPTIONAL): PARAMARRAY veniva solo consumato (gmb_p_adv) senza mai
                     * marcare il parametro — trattato come uno scalare qualunque, con ≥1
                     * argomenti reali il valore finiva in un local scalare invece che in un
                     * array (bug: FunctionsParams.bas, "indexing a numeric scalar is not
                     * allowed"). */
                    int is_byref = 0, is_variadic = 0, is_optional = 0;
                    /* ADR-MATRIX §3: IN/OUT/INOUT in posizione parametro. OUT/INOUT non
                     * sono keyword globali: valgono solo seguite da un NOME (non AS/,/)/=). */
                    int param_mode = 0;
                    {
                        const gmb_token_t* mc = gmb_p_cur(p);
                        if (gmb_p_tok_is_keyword(mc, "In")) {
                            /* `IN` può essere anche il NOME del parametro (`in() AS DOUBLE`):
                             * è un modificatore solo se seguito da un ALTRO nome. Prima veniva
                             * consumato sempre, quindi `DECLARE ... (in() AS DOUBLE)` e
                             * `SUB S(in() AS BYTE)` davano «statement non riconosciuto»
                             * (graphicsffi/gfx2d_*). */
                            const gmb_token_t* nx = gmb_p_peek(p, 1);
                            if (nx && nx->kind != GMB_TOK_As && nx->kind != GMB_TOK_Comma
                                && nx->kind != GMB_TOK_RightParen && nx->kind != GMB_TOK_LeftParen
                                && nx->kind != GMB_TOK_Equal) {
                                param_mode = 0;
                                gmb_p_adv(p);
                            }
                        } else if (mc && mc->kind == GMB_TOK_Identifier && mc->lexeme
                                   && (strncasecmp(mc->lexeme, "OUT", 3) == 0
                                       || strncasecmp(mc->lexeme, "INOUT", 5) == 0)) {
                            const gmb_token_t* nx = gmb_p_peek(p, 1);
                            if (nx && nx->kind != GMB_TOK_As && nx->kind != GMB_TOK_Comma
                                && nx->kind != GMB_TOK_RightParen && nx->kind != GMB_TOK_LeftParen
                                && nx->kind != GMB_TOK_Equal) {
                                param_mode = (strncasecmp(mc->lexeme, "INOUT", 5) == 0) ? 2 : 1;
                                gmb_p_adv(p);
                            }
                        }
                    }
                    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Byref") || gmb_p_check(p, GMB_TOK_ByRef)) { is_byref = 1; gmb_p_adv(p); }
                    else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Paramarray")) { is_variadic = 1; gmb_p_adv(p); }
                    else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Optional")) { is_optional = 1; gmb_p_adv(p); }
                    else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Byval")) gmb_p_adv(p);
                    const gmb_token_t* prm = gmb_p_cur(p);
                    if (prm) {
                        if (s->n_params >= cap) { cap = cap ? cap * 2 : 4; s->func_params = (char**)realloc(s->func_params, (size_t)cap * sizeof(char*)); s->param_byref = (uint8_t*)realloc(s->param_byref, (size_t)cap); s->param_is_array = (uint8_t*)realloc(s->param_is_array, (size_t)cap); s->param_ndims = (uint8_t*)realloc(s->param_ndims, (size_t)cap); s->param_mode = (uint8_t*)realloc(s->param_mode, (size_t)cap); s->param_types = (char**)realloc(s->param_types, (size_t)cap * sizeof(char*)); s->param_fixed_lens = (int*)realloc(s->param_fixed_lens, (size_t)cap * sizeof(int)); s->param_variadic = (uint8_t*)realloc(s->param_variadic, (size_t)cap); s->param_optional = (uint8_t*)realloc(s->param_optional, (size_t)cap); s->param_defaults = (gmb_expr_t**)realloc(s->param_defaults, (size_t)cap * sizeof(gmb_expr_t*)); s->param_fn_arity = (int*)realloc(s->param_fn_arity, (size_t)cap * sizeof(int)); s->param_fn_str_mask = (int*)realloc(s->param_fn_str_mask, (size_t)cap * sizeof(int)); s->param_fn_ret_str = (uint8_t*)realloc(s->param_fn_ret_str, (size_t)cap); for (int z = s->n_params; z < cap; z++) { s->param_types[z] = NULL; s->param_is_array[z] = 0; s->param_ndims[z] = 0; s->param_mode[z] = 0; s->param_fixed_lens[z] = 0; s->param_variadic[z] = 0; s->param_optional[z] = 0; s->param_defaults[z] = NULL; s->param_fn_arity[z] = -1; s->param_fn_str_mask[z] = 0; s->param_fn_ret_str[z] = 0; } }
                        char* b = (char*)malloc((size_t)prm->length + 1); memcpy(b, prm->lexeme, (size_t)prm->length); b[prm->length] = 0;
                        s->func_params[s->n_params] = b;
                        s->param_byref[s->n_params] = (uint8_t)is_byref;
                        s->param_is_array[s->n_params] = 0; s->param_ndims[s->n_params] = 0;
                        s->param_mode[s->n_params] = (uint8_t)param_mode;
                        s->param_types[s->n_params] = NULL;
                        s->param_fixed_lens[s->n_params] = 0;
                        s->param_variadic[s->n_params] = (uint8_t)is_variadic;
                        s->param_optional[s->n_params] = (uint8_t)is_optional;
                        s->param_defaults[s->n_params] = NULL;
                        s->n_params++;
                    }
                    gmb_p_adv(p);
                    /* parametro: nome [()] [(,)] [(,,)]... [AS tipo] — il numero di
                     * dimensioni e' dato dalle virgole (rank-n); `()` = rank 1. */
                    while (gmb_p_check(p, GMB_TOK_LeftParen)) {
                        gmb_p_adv(p);
                        int _rank = 1;
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
                            if (gmb_p_check(p, GMB_TOK_Comma)) _rank++;
                            gmb_p_adv(p);
                        }
                        gmb_p_match(p, GMB_TOK_RightParen);
                        if (s->n_params > 0) { s->param_is_array[s->n_params - 1] = 1; s->param_ndims[s->n_params - 1] += (uint8_t)_rank; }
                    }
                    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                        gmb_p_adv(p);
                        /* cattura il nome del tipo (primo token) per il codegen C99 (GPM) */
                        const gmb_token_t* ty = gmb_p_cur(p);
                        /* `AS ARRAY OF T`: tipo ELEMENTO T (rank dagli assi/`()`); niente
                         * array annidati (ARRAY OF ARRAY non esiste più). */
                        if (ty && (ty->kind == GMB_TOK_Array || gmb_p_tok_is_keyword(ty, "Array"))) {
                            ty = gmb_consume_array_of(p);
                            if (s->n_params > 0) {
                                s->param_is_array[s->n_params - 1] = 1;
                                if (!s->param_ndims[s->n_params - 1]) s->param_ndims[s->n_params - 1] = 1;
                            }
                        }
                        if (ty && ty->kind == GMB_TOK_Matrix) {
                            /* `a AS MATRIX OF (I1, …, In, V)`: rank dagli assi, tipo valore V. */
                            const gmb_token_t* vt = NULL;
                            int axes = gmb_parse_matrix_of(p, &vt);
                            if (axes >= 2 && vt && s->n_params > 0) {
                                s->param_is_array[s->n_params - 1] = 1;
                                s->param_ndims[s->n_params - 1] = (uint8_t)axes;
                                if (!s->param_types[s->n_params - 1]) {
                                    char* tb = (char*)malloc((size_t)vt->length + 1);
                                    memcpy(tb, vt->lexeme, (size_t)vt->length); tb[vt->length] = 0;
                                    s->param_types[s->n_params - 1] = tb;
                                }
                            }
                        }
                        if (ty && ty->lexeme && s->n_params > 0 && !s->param_types[s->n_params - 1]) {
                            char* tb = (char*)malloc((size_t)ty->length + 1); memcpy(tb, ty->lexeme, (size_t)ty->length); tb[ty->length] = 0;
                            /* Tipo QUALIFICATO (`AS Ns.Classe`, es. `v AS Mezzi.Veicolo`): concatena
                             * i segmenti dopo il punto PEEKando (senza consumarli — li salta poi il
                             * loop delle clausole-tipo qui sotto). Il parametro di tipo classe diventa
                             * vclass nell'emitter → i suoi metodi (`v.GetMarca()`) risolvono la catena
                             * di ereditarietà (prima param_types = "Mezzi" → cls_find falliva). */
                            for (int po = 1;; po += 2) {
                                const gmb_token_t* dot = gmb_p_peek(p, po);
                                const gmb_token_t* seg = gmb_p_peek(p, po + 1);
                                if (!dot || dot->kind != GMB_TOK_Dot) break;
                                if (!seg || !seg->lexeme || seg->kind == GMB_TOK_Newline
                                    || seg->kind == GMB_TOK_Eof || seg->kind == GMB_TOK_LeftParen) break;
                                char* nb = (char*)realloc(tb, strlen(tb) + 1 + (size_t)seg->length + 1);
                                if (!nb) break;
                                tb = nb;
                                strcat(tb, ".");
                                strncat(tb, seg->lexeme, (size_t)seg->length);
                            }
                            s->param_types[s->n_params - 1] = tb;
                        }
                        /* 2026-09-03: parametro `AS FUNCTION(N,...) AS <ret>` (predicato di ordine
                         * superiore, es. FunctionsLambda.bas `predicate AS FUNCTION(DOUBLE) AS
                         * BIT`): cattura arity (i tipi della firma sono sempre numerici in questo
                         * emitter) e se il ritorno è STRING — stessa logica del DIM v AS FUNCTION. */
                        if (ty && ty->lexeme && s->n_params > 0
                            && (gmb_p_tok_is_keyword(gmb_p_peek(p, 0), "Function")
                                || ty->kind == GMB_TOK_Function)) {
                            gmb_p_adv(p);   /* FUNCTION/SUB */
                            int fn_sig_mask = 0, fn_kind_mask = 0;
                            int fn_arr_mask = 0;
                            int fn_arity = gmb_p_fn_sig(p, &fn_sig_mask, &fn_kind_mask, &fn_arr_mask);
                            if (!s->param_fn_arity) {
                                s->param_fn_arity = (int*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(int));
                                s->param_fn_str_mask = (int*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(int));
                                for (int z = 0; z < s->n_params; z++) s->param_fn_arity[z] = -1;
                            }
                            if (!s->param_fn_kind_mask)
                                s->param_fn_kind_mask = (int*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(int));
                            if (!s->param_fn_arr_mask)
                                s->param_fn_arr_mask = (int*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(int));
                            s->param_fn_arity[s->n_params - 1] = fn_arity;
                            s->param_fn_str_mask[s->n_params - 1] = fn_sig_mask;
                            s->param_fn_kind_mask[s->n_params - 1] = fn_kind_mask;
                            s->param_fn_arr_mask[s->n_params - 1] = fn_arr_mask;
                            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                                if (!s->param_fn_has_ret) s->param_fn_has_ret = (uint8_t*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(uint8_t));
                                s->param_fn_has_ret[s->n_params - 1] = 1;
                                gmb_p_adv(p);
                                const gmb_token_t* rtf = gmb_p_cur(p);
                                if (gmb_p_tok_is_string_type_name(rtf)) {
                                    if (!s->param_fn_ret_str) s->param_fn_ret_str = (uint8_t*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(uint8_t));
                                    s->param_fn_ret_str[s->n_params - 1] = 1;
                                }
                                if (gmb_p_tok_is_scalar_type_name(rtf)) {
                                    if (!s->param_fn_ret_int) s->param_fn_ret_int = (uint8_t*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(uint8_t));
                                    s->param_fn_ret_int[s->n_params - 1] = 1;
                                }
                                if (rtf && (rtf->kind == GMB_TOK_Array || gmb_p_tok_is_keyword(rtf, "Array"))) {
                                    if (!s->param_fn_ret_arr) s->param_fn_ret_arr = (uint8_t*)calloc((size_t)(s->n_params > 0 ? s->n_params : 1), sizeof(uint8_t));
                                    s->param_fn_ret_arr[s->n_params - 1] = 1;
                                }
                                if (rtf) gmb_p_adv(p);
                            }
                        }
                        for (;;) {
                            /* Bug corretto (2026-08-31, audit OPTIONAL): '=' (inizio del
                             * valore di default, es. "AS STRING = \"Sig.\"") non era tra i
                             * terminatori di questo loop di skip della clausola tipo — veniva
                             * quindi "mangiato" qui dentro come un token qualunque (insieme al
                             * valore di default stesso), e il controllo dedicato al default
                             * più sotto (gmb_p_match(GMB_TOK_Equal)) non vedeva mai un '='
                             * da consumare: il default restava sempre NULL. */
                            if (gmb_p_at_end(p) || gmb_p_check(p, GMB_TOK_Newline)
                                || gmb_p_check(p, GMB_TOK_Comma) || gmb_p_check(p, GMB_TOK_RightParen)
                                || gmb_p_check(p, GMB_TOK_Equal)) break;
                            if (gmb_p_check(p, GMB_TOK_Multiply) && gmb_p_peek(p, 1)
                                && gmb_p_peek(p, 1)->kind == GMB_TOK_Number && s->n_params > 0) {
                                /* STRING * N: lunghezza fissa del parametro */
                                gmb_p_adv(p);
                                char* nb = gmb_p_tok_strdup(gmb_p_cur(p));
                                s->param_fixed_lens[s->n_params - 1] = atoi(nb);
                                free(nb);
                                gmb_p_adv(p);
                                continue;
                            }
                            if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                                int d = 1;
                                while (!gmb_p_at_end(p) && d > 0) {
                                    if (gmb_p_match(p, GMB_TOK_LeftParen)) d++;
                                    else if (gmb_p_match(p, GMB_TOK_RightParen)) d--;
                                    else if (gmb_p_at_end(p)) break;
                                    else gmb_p_adv(p);
                                }
                                continue;
                            }
                            if (gmb_p_match(p, GMB_TOK_LessThan)) {
                                /* tipo generico: DICT<STRING, INTEGER> — salta fino a '>' */
                                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p);
                                gmb_p_match(p, GMB_TOK_GreaterThan);
                                continue;
                            }
                            if (gmb_p_check(p, GMB_TOK_Of)) {
                                /* AS DICT OF K, V (forma "legacy" con OF — vedi anche DIM AS
                                 * DICT OF K,V più sotto in questo file, reg_dim, che gestisce
                                 * correttamente lo stesso caso): un tipo mappa richiede DUE nomi
                                 * di tipo separati da virgola — quella virgola non è un
                                 * separatore di PARAMETRO. Senza questo caso speciale il loop
                                 * terminava alla prima virgola (condizione di uscita in cima al
                                 * for), lasciando "DOUBLE" (il tipo valore) come token
                                 * successivo: il chiamante lo interpretava come un SECONDO
                                 * parametro — "numero argomenti errato per T: attesi 2, trovati
                                 * 1" (bug C99Native-Comprehension-DictParam-ChiaviNumeriche,
                                 * AiWiki). Consuma esattamente i token-tipo attesi (1 per una
                                 * collezione semplice, 2 per una mappa/dict — dim_map_kind,
                                 * stessa tabella di reg_dim), virgola fra loro inclusa. */
                                gmb_p_adv(p);   /* OF */
                                int want = dim_map_kind(ty ? ty->kind : GMB_TOK_Unknown) ? 2 : 1;
                                int seen = 0;
                                while (!gmb_p_at_end(p) && seen < want) {
                                    const gmb_token_t* otk = gmb_p_cur(p);
                                    if (!otk || otk->kind == GMB_TOK_Newline || otk->kind == GMB_TOK_RightParen || otk->kind == GMB_TOK_Equal) break;
                                    if (otk->kind == GMB_TOK_Comma) { gmb_p_adv(p); continue; }
                                    if (otk->lexeme && isalpha((unsigned char)otk->lexeme[0])) { seen++; gmb_p_adv(p); continue; }
                                    break;
                                }
                                continue;
                            }
                            gmb_p_adv(p);
                        }
                    }
                    if (gmb_p_match(p, GMB_TOK_Equal)) {
                        /* valore di default: OPTIONAL Char AS STRING = "-". Bug corretto
                         * (2026-08-31, audit OPTIONAL): l'espressione veniva parsata e poi
                         * SUBITO scartata (gmb_expr_free) — mai salvata da nessuna parte.
                         * Un argomento omesso non riceveva quindi mai il default dichiarato
                         * (bug: FunctionsCall.bas, "Ciao, Rossi" invece di "Ciao, Sig. Rossi";
                         * FunctionsRecursionAndByRef.bas, TestOpzionali con valori
                         * completamente disallineati). */
                        gmb_expr_t* defv = gmb_p_parse_expr(p);
                        if (s->n_params > 0) s->param_defaults[s->n_params - 1] = defv;
                        else if (defv) gmb_expr_free(defv);
                    }
                    if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                }
            }
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        /* clausola di ritorno: AS <tipo> [* N] o AS (t1, t2) — consuma fino a fine riga;
         * il PRIMO token del tipo è catturato per il codegen C99 (ritorno STRING/numero) */
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
            gmb_p_adv(p);
            const gmb_token_t* rt = gmb_p_cur(p);
            if (rt && rt->kind != GMB_TOK_Newline && rt->lexeme && !s->func_ret_type) {
                char* rb = (char*)malloc((size_t)rt->length + 1);
                memcpy(rb, rt->lexeme, (size_t)rt->length); rb[rt->length] = 0;
                s->func_ret_type = rb;
            }
            if (rt && rt->kind == GMB_TOK_Matrix) {
                /* `AS MATRIX OF (I1, …, In, V)`: rank dagli assi, tipo valore V. */
                const gmb_token_t* vt = NULL;
                int axes = gmb_parse_matrix_of(p, &vt);
                if (axes >= 2 && vt) {
                    s->func_ret_ndims = axes;
                    char* rb = (char*)malloc((size_t)vt->length + 1);
                    memcpy(rb, vt->lexeme, (size_t)vt->length); rb[vt->length] = 0;
                    free(s->func_ret_type);
                    s->func_ret_type = rb;
                }
            }
            /* AS FUNCTION(params) AS <ret>: cattura la FIRMA del block restituito
             * (arity/mask/ret) — serve a `DIM b = Mk()` (firma inferita): senza,
             * GK_FUNCVAL restava arity 0 → `EVAL(b, 5)` "numero argomenti errato". */
            if (rt && rt->kind == GMB_TOK_Function) {
                gmb_p_adv(p);
                if (gmb_p_check(p, GMB_TOK_LeftParen))
                    s->func_val_arity = gmb_p_fn_sig(p, &s->func_val_str_mask, NULL, NULL);
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                    gmb_p_adv(p);
                    const gmb_token_t* rr = gmb_p_cur(p);
                    if (gmb_p_tok_is_string_type_name(rr))
                        s->func_val_ret_is_str = 1;
                    if (rr && gmb_keyword_tok(rr) == GMB_TOK_Array) s->func_val_ret_is_arr = 1;
                }
            }
            /* AS (nome AS T, ...): cattura i nomi campo del ritorno in un CSV (stesso
             * formato del literal tuple, ",y," con vuoto = anonimo) così che il CHIAMANTE
             * (DIM x = Fn(), reg_dim/prop_dim) possa risolvere x.nome -> x(indice)
             * (resolve_tuple_field). Un campo ha nome se il token è un Identifier seguito
             * da AS; altrimenti è anonimo (segnaposto vuoto). */
            if (rt && rt->kind == GMB_TOK_LeftParen && !s->func_ret_tuple_fields) {
                char csv[512]; size_t cl = 0; int any = 0, d = 1, first = 1;
                for (int i = 1; ; i++) {
                    const gmb_token_t* tk = gmb_p_peek(p, i);
                    if (!tk || tk->kind == GMB_TOK_Newline) break;
                    if (tk->kind == GMB_TOK_LeftParen) { d++; continue; }
                    if (tk->kind == GMB_TOK_RightParen) { d--; if (d <= 0) break; continue; }
                    if (d != 1) continue;
                    const gmb_token_t* prev = gmb_p_peek(p, i - 1);
                    if (prev && (prev->kind == GMB_TOK_LeftParen || prev->kind == GMB_TOK_Comma)) {
                        if (!first && cl + 1 < sizeof(csv)) csv[cl++] = ',';
                        first = 0;
                        const gmb_token_t* nx = gmb_p_peek(p, i + 1);
                        if (tk->kind == GMB_TOK_Identifier && nx
                            && (nx->kind == GMB_TOK_As || gmb_p_tok_is_keyword(nx, "As"))) {
                            size_t nl = (size_t)tk->length;
                            if (cl + nl < sizeof(csv)) { memcpy(csv + cl, tk->lexeme, nl); cl += nl; any = 1; }
                        }
                    }
                }
                if (any) { csv[cl] = 0; s->func_ret_tuple_fields = strdup(csv); }
            }
            int depth = 0;
            while (!gmb_p_at_end(p)) {
                const gmb_token_t* tt = gmb_p_cur(p);
                if (tt->kind == GMB_TOK_Newline && depth == 0) break;
                if (tt->kind == GMB_TOK_FatArrow || tt->kind == GMB_TOK_Arrow) break;   /* 2.7(d): corpo => */
                /* rank esplicito del ritorno: T(,), T(,,), ... (solo virgole dentro le parentesi) */
                if (tt->kind == GMB_TOK_LeftParen && depth == 0 && s->func_ret_ndims == 0) {
                    int _rk = 1, _k = 1;
                    while (gmb_p_peek(p, _k) && gmb_p_peek(p, _k)->kind == GMB_TOK_Comma) { _rk++; _k++; }
                    if (_rk > 1 && gmb_p_peek(p, _k) && gmb_p_peek(p, _k)->kind == GMB_TOK_RightParen) {
                        s->func_ret_ndims = _rk;
                        for (int _z = 0; _z <= _k; _z++) gmb_p_adv(p);
                        continue;
                    }
                }
                if (tt->kind == GMB_TOK_Multiply && gmb_p_peek(p, 1)
                    && gmb_p_peek(p, 1)->kind == GMB_TOK_Number) {
                    /* STRING * N: lunghezza fissa del ritorno */
                    gmb_p_adv(p);
                    char* nb = gmb_p_tok_strdup(gmb_p_cur(p));
                    s->func_ret_fixed_len = atoi(nb);
                    free(nb);
                    gmb_p_adv(p);
                    continue;
                }
                if (tt->kind == GMB_TOK_LeftParen) depth++;
                else if (tt->kind == GMB_TOK_RightParen) depth--;
                gmb_p_adv(p);
            }
        }
        /* 2.7(d): corpo a espressione — FUNCTION ... => expr (= RETURN expr),
         * SUB ... => <istruzione singola> (parity PCodeVM) */
        if (gmb_p_check(p, GMB_TOK_FatArrow) || gmb_p_check(p, GMB_TOK_Arrow)) {
            gmb_p_adv(p);
            gmb_stmt_list_t body = gmb_p_stmt_list_new();
            int is_sub = kw_is_sub;
            if (is_sub) {
                bool ml2 = false;
                gmb_stmt_t* one = gmb_p_parse_stmt(p, &ml2);
                if (one) gmb_stmt_add(&body.items, &body.n, one, &body.cap);
            } else {
                gmb_stmt_t* r = gmb_stmt_new(GMB_STMT_RETURN, line);
                r->ret_value = gmb_p_parse_expr(p);
                gmb_stmt_add(&body.items, &body.n, r, &body.cap);
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
            (void)gmb_p_wrap_errdefers(&body, line);
            s->func_body = body.items; s->n_funcbody = body.n;
            return s;
        }
        *multiline = true;
        gmb_stmt_list_t body = gmb_p_stmt_list_new();
        if (is_gen)
            gmb_p_parse_block(p, &body, GMB_TOK_EndGenerator, "EndGenerator", line);
        else
            gmb_p_parse_block(p, &body, GMB_TOK_EndFunction, "EndFunction", line);
        if (is_gen) {
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndGenerator
                                 || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndGenerator"))) gmb_p_adv(p);
            else if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_End) {
                gmb_p_adv(p);
                if (gmb_p_cur(p)) gmb_p_adv(p);   /* GENERATOR / GEN */
            }
        } else {
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndFunction || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndFunction"))) gmb_p_adv(p);
            else if (gmb_p_at_end_compound(p, GMB_TOK_Function)) { gmb_p_adv(p); gmb_p_adv(p); }
        }
        /* ERRDEFER dentro un GENERATOR: il desugaring produce un TRY/CATCH, che il corpo di un
         * generatore non può contenere (l'emitter C lo rifiuta come "statement non supportato").
         * Errore ESPLICITO in parsing, con la ragione. */
        if (is_gen) {
            if (!p->error && gmb_p_has_stray_errdefer(body.items, body.n))
                p->error = strdup("ERRDEFER non è supportato dentro un GENERATOR: il cleanup sul "
                                  "percorso d'errore richiede un TRY/CATCH, che il corpo di un "
                                  "generatore non può contenere.");
        } else {
            (void)gmb_p_wrap_errdefers(&body, line);
        }
        s->func_body = body.items; s->n_funcbody = body.n;
        return s;
    }
    if (t->kind == GMB_TOK_Await) {
        /* AWAIT <expr>: parità C# (AwaitExpr compila come il target) — l'attesa è
           implicita nel modello VM; valutiamo comunque l'espressione per gli effetti. */
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_EXPR, line);
        s->expr = gmb_p_parse_expr(p);
        if (!s->expr) { gmb_stmt_free(s); return gmb_stmt_new(GMB_STMT_REM, line); }
        return s;
    }
    if (t->kind == GMB_TOK_Http) {
        /* HTTP ROUTE method, path, Handler() / HTTP SERVER [porta|STOP] (parità
           Compiler.Network.cs → OP_NET_CMD sub HTTP_ROUTE=15 / HTTP_SERVER=14) */
        gmb_p_adv(p);
        if (gmb_p_check(p, GMB_TOK_Route)) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
            s->net_sub = 15;
            s->coderef_pos = 2;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*));
            s->n_netargs = 2;
            s->net_args[0] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            gmb_p_skip_eol(p);
            s->net_args[1] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            gmb_p_skip_eol(p);
            const gmb_token_t* cb = gmb_p_cur(p);
            if (cb && cb->kind != GMB_TOK_Newline && cb->kind != GMB_TOK_Eof) {
                s->net_coderef = gmb_p_tok_strdup(cb);
                gmb_p_adv(p);
                if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                    gmb_p_match(p, GMB_TOK_RightParen);
                }
            }
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return s;
        }
        if (gmb_p_check(p, GMB_TOK_Server)) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
            s->net_sub = 14;
            s->coderef_pos = -1;
            s->net_args = (gmb_expr_t**)calloc(5, sizeof(gmb_expr_t*));
            s->n_netargs = 5;
            if (gmb_p_check(p, GMB_TOK_Stop)) {
                gmb_p_adv(p);
                s->net_args[0] = gmb_expr_call("_true_", NULL, 0, line);
            } else {
                s->net_args[0] = gmb_expr_call("_false_", NULL, 0, line);
                if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Eof)
                    s->net_args[1] = gmb_p_parse_expr(p);
            }
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return s;
        }
        if (gmb_p_check(p, GMB_TOK_Config)) {
            /* HTTP CONFIG cfg → salva CertificatePath/CertificatePassword per il prossimo
             * HTTP SERVER (net_sub=16; parita' emitter C# Rete.cs HttpConfigStmt, dove prima
             * era un no-op e il server HTTPS partiva plain → client https hang). */
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
            s->net_sub = 16;
            s->coderef_pos = -1;
            s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
            s->n_netargs = 1;
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Eof)
                s->net_args[0] = gmb_p_parse_expr(p);
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return s;
        }
        /* HTTP USE [path,] handler() / HTTP USE NEXT [handle] — parità OP_NET_CMD
         * della VM/C (sub 16/17) e di emitter C# (Rete.cs HttpUseStmt/HttpUseNextStmt).
         * Il no-op precedente lasciava i sample HttpRoutes/Mvc/ con la catena
         * middleware vuota → auth/advanced/async middleware mai invocati. */
        if (gmb_p_check(p, GMB_TOK_Use)) {
            gmb_p_adv(p);
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Next") || gmb_p_check(p, GMB_TOK_Next)) {
                gmb_p_adv(p);
                gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
                s->net_sub = 17;                 /* HTTP_USE_NEXT (emitter: net_sub==17) */
                s->coderef_pos = -1;
                s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
                s->n_netargs = 1;
                if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Eof)
                    s->net_args[0] = gmb_p_parse_expr(p);
                while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
                return s;
            }
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
            s->net_sub = 18;                    /* HTTP_USE (emitter: net_sub==18) */
            s->coderef_pos = 1;
            s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
            s->n_netargs = 1;
            /* path opzionale: se il token dopo il current è già LeftParen → handler()
             * immediato (nessun path), altrimenti parse path + virgola. */
            if (gmb_p_cur(p) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftParen) {
                s->net_args[0] = gmb_p_parse_expr(p);
                gmb_p_match(p, GMB_TOK_Comma);
                gmb_p_skip_eol(p);
            }
            const gmb_token_t* cb = gmb_p_cur(p);
            if (cb && cb->kind != GMB_TOK_Newline && cb->kind != GMB_TOK_Eof) {
                s->net_coderef = gmb_p_tok_strdup(cb);
                gmb_p_adv(p);
                if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                    gmb_p_match(p, GMB_TOK_RightParen);
                }
            }
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return s;
        }
        /* HTTP STATIC prefix, dir — file statici serviti dal server.
         * Prima era un no-op del parser: su C99Native `HTTP STATIC` non registrava NULLA e
         * ogni richiesta a un file statico finiva in 404 (lo stesso valeva per Range/206).
         * net_sub = 19 (emitter: st_net_case 19 → gmb_http_server_static). */
        if (gmb_p_check(p, GMB_TOK_Static)) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_NET_CMD, line);
            s->net_sub = 19;
            s->coderef_pos = -1;
            s->net_args = (gmb_expr_t**)calloc(2, sizeof(gmb_expr_t*));
            s->n_netargs = 2;
            /* Alcune forme hanno "HTTP STATIC" già consumato e il token corrente è la prefix;
             * altre passano dal verbo STATIC come keyword di dichiarazione: si accetta sia
             * `HTTP STATIC "/s", "./dir"` sia la variante senza virgola. */
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Comma) gmb_p_adv(p);
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Eof)
                s->net_args[0] = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_Comma);
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Eof)
                s->net_args[1] = gmb_p_parse_expr(p);
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return s;
        }
        /* HTTP BSP/altro: parity no-op, consuma la riga */
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }
    if (t->kind == GMB_TOK_Local || t->kind == GMB_TOK_Static || t->kind == GMB_TOK_Private) {
        gmb_stmt_t* ls = gmb_stmt_new(GMB_STMT_DIM, line);
        ls->dim_is_local = (t->kind == GMB_TOK_Local);
        ls->dim_is_static = (t->kind == GMB_TOK_Static);
        gmb_p_adv(p);
        const gmb_token_t* lv = gmb_p_cur(p);
        if (lv) { ls->dim_var = (char*)malloc((size_t)lv->length + 1); memcpy(ls->dim_var, lv->lexeme, (size_t)lv->length); ls->dim_var[lv->length] = 0; gmb_p_adv(p); }
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            if (!gmb_p_check(p, GMB_TOK_RightParen)) ls->array_size = gmb_p_parse_expr(p);
            /* Canone 1-based 2026-09-30: DIM v(lo TO hi) — upper separato (array_size resta lo). */
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); ls->dim_to_hi = gmb_p_parse_expr(p); }
            while (gmb_p_match(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_at_end(p)) {
                gmb_expr_t* d = gmb_p_parse_expr(p);
                gmb_expr_t* d_lo = NULL;
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); d_lo = d; d = gmb_p_parse_expr(p); }
                ls->dim_extra = (gmb_expr_t**)realloc(ls->dim_extra, (size_t)(ls->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                ls->dim_extra_lo = (gmb_expr_t**)realloc(ls->dim_extra_lo, (size_t)(ls->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                ls->dim_extra[ls->n_dim_extra] = d;
                ls->dim_extra_lo[ls->n_dim_extra] = d_lo;
                ls->n_dim_extra++;
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        /* AS <tipo> [* N] [= expr] — allineato al branch DIM (bug 2026-09-06:
         * `LOCAL x AS INTEGER = expr` scartava la clausola AS e l'inizializzatore,
         * valore sempre 0 — vedi report NativeVMC-Callable… nota closure by-value). */
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
            const gmb_token_t* at = gmb_p_peek(p, 1);
            if (at && at->lexeme) {
                char tb2[32]; size_t tl2 = (size_t)at->length < 31 ? (size_t)at->length : 31;
                memcpy(tb2, at->lexeme, tl2); tb2[tl2] = 0;
                if (at->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(at, "Sub")
                    || !strcasecmp(tb2, "FUNCTION") || !strcasecmp(tb2, "SUB")) {
                    /* LOCAL v AS FUNCTION(...) AS <ret> — variabile funzione */
                    gmb_p_adv(p); gmb_p_adv(p);
                    ls->dim_type = (char*)malloc(9); memcpy(ls->dim_type, "FUNCTION", 9);
                    int arity2 = 0;
                    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                        if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                            arity2 = 1;
                            for (;;) {
                                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                                arity2++;
                            }
                        }
                        gmb_p_match(p, GMB_TOK_RightParen);
                    }
                    ls->func_val_arity = arity2;
                    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                        gmb_p_adv(p);
                        const gmb_token_t* rt = gmb_p_cur(p);
                        if (gmb_p_tok_is_string_type_name(rt)) ls->func_val_ret_is_str = 1;
                        if (rt && gmb_keyword_tok(rt) == GMB_TOK_Array) ls->func_val_ret_is_arr = 1;
                        if (rt) gmb_p_adv(p);
                    }
                } else {
                    gmb_p_adv(p);   /* AS */
                    gmb_p_adv(p);   /* nome tipo */
                    ls->dim_type = (char*)malloc(tl2 + 1); memcpy(ls->dim_type, tb2, tl2 + 1);
                    if (!strcasecmp(tb2, "STRING") && gmb_p_check(p, GMB_TOK_Multiply)
                        && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Number) {
                        gmb_p_adv(p);
                        char* nb2 = gmb_p_tok_strdup(gmb_p_cur(p));
                        ls->dim_fixed_len = atoi(nb2);
                        free(nb2);
                        gmb_p_adv(p);
                    }
                }
            } else { gmb_p_adv(p); }
        }
        if (gmb_p_match(p, GMB_TOK_Equal)) {
            ls->dim_init = gmb_p_parse_expr(p);
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return ls;
        }
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
        return ls;
    }
    if (t->kind == GMB_TOK_Global) {
        gmb_p_adv(p);
        gmb_stmt_t* gs = gmb_stmt_new(GMB_STMT_DIM, line);
        gs->dim_is_global = 1;
        const gmb_token_t* gv = gmb_p_cur(p);
        if (gv) { gs->dim_var = (char*)malloc((size_t)gv->length + 1); memcpy(gs->dim_var, gv->lexeme, (size_t)gv->length); gs->dim_var[gv->length] = 0; gmb_p_adv(p); }
        if (gmb_p_match(p, GMB_TOK_LeftParen)) { if (!gmb_p_check(p, GMB_TOK_RightParen)) gs->array_size = gmb_p_parse_expr(p);
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); gs->dim_to_hi = gmb_p_parse_expr(p); }
            while (gmb_p_match(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_at_end(p)) {
                gmb_expr_t* d = gmb_p_parse_expr(p);
                gmb_expr_t* d_lo = NULL;
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); d_lo = d; d = gmb_p_parse_expr(p); }
                gs->dim_extra = (gmb_expr_t**)realloc(gs->dim_extra, (size_t)(gs->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                gs->dim_extra_lo = (gmb_expr_t**)realloc(gs->dim_extra_lo, (size_t)(gs->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                gs->dim_extra[gs->n_dim_extra] = d;
                gs->dim_extra_lo[gs->n_dim_extra] = d_lo;
                gs->n_dim_extra++;
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p); gmb_p_match(p, GMB_TOK_RightParen); }
        /* `GLOBAL x AS STRING` — la clausola AS va catturata (prima era ignorata: il tipo
         * stringa arrivava solo dal punto fisso sugli assegnamenti, DOPO il prepass lambda →
         * `lambda_expr_returns_str` vedeva la globale numerica e `{ |s| RETURN s + gSuffix }`
         * restava con param double, divergenza col C#). */
        if (gmb_p_check(p, GMB_TOK_As) || gmb_p_tok_is_keyword(gmb_p_cur(p), "As")) {
            const gmb_token_t* at = gmb_p_peek(p, 1);
            if (at && at->lexeme && (at->lexeme[0] == '_' || isalpha((unsigned char)at->lexeme[0]))) {
                size_t tl = (size_t)at->length < 31 ? (size_t)at->length : 31;
                gmb_p_adv(p);   /* AS */
                gmb_p_adv(p);   /* nome tipo */
                gs->dim_type = (char*)malloc(tl + 1); memcpy(gs->dim_type, at->lexeme, tl); gs->dim_type[tl] = 0;
                gs->dim_type_tk = (at->kind == GMB_TOK_Identifier || at->kind == GMB_TOK_Unknown) ? 0 : (int)at->kind;
                /* GLOBAL x AS <collezione><...> / OF ...: cattura il tipo ELEMENTO/VALORE
                 * (identico al ramo DIM) — senza, `GLOBAL Users AS DICT<LONG, JSON>` perdeva
                 * il valore JSON e l'emitter lo trattava come dict scalare (double). */
                if (dim_coll_kind((gmb_tokenkind_t)gs->dim_type_tk) && gmb_p_check(p, GMB_TOK_LessThan)) {
                    gmb_p_adv(p);   /* '<' */
                    int ntypes = 0;
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan) && !gmb_p_check(p, GMB_TOK_Newline)) {
                        const gmb_token_t* gtk = gmb_p_cur(p);
                        if (gtk && gtk->lexeme && isalpha((unsigned char)gtk->lexeme[0])) {
                            ntypes++;
                            int want = dim_map_kind((gmb_tokenkind_t)gs->dim_type_tk) ? 2 : 1;
                            if (ntypes == want) {
                                if (gtk->kind == GMB_TOK_StringType)
                                    gs->dim_elem_str = 1;
                                else if (!gm_parser_prim_len(gtk->lexeme, gtk->length) && !gs->dim_elem_class) {
                                    char ec[64]; int el = gtk->length < 63 ? gtk->length : 63;
                                    memcpy(ec, gtk->lexeme, (size_t)el); ec[el] = 0;
                                    gs->dim_elem_class = strdup(ec);
                                }
                            }
                        }
                        gmb_p_adv(p);
                    }
                    gmb_p_match(p, GMB_TOK_GreaterThan);
                }
            }
        }
        /* GLOBAL x [AS tipo] = expr — l'inizializzatore va catturato (prima era scartato:
         * `GLOBAL NextId AS LONG = 1` restava 0). */
        if (gmb_p_match(p, GMB_TOK_Equal)) gs->dim_init = gmb_p_parse_expr(p);
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
        return gs;
    }
    /* DIM (a, b, c) = expr — destrutturazione di tupla in variabili FRESCHE (distinta dallo
     * statement di assegnazione tuple "(a,b)=(b,a)" più sotto, che opera su variabili GIA'
     * dichiarate e non parte da DIM). Riconosciuta SOLO quando '(' segue DIM immediatamente
     * (nessun nome prima): "DIM x(5)" resta il ramo array esistente sotto. Parità con
     * TupleDestructuringStmt (common/Parser/Parser.Declarations.cs, 
     * C99CodeEmitter.cs) — RHS letterale `(e1, e2, ...)`: un DIM fresco per variabile, valore
     * diretto. RHS generico (variabile/chiamata gmb_val_t, es. `DIM (a,b,c) = t`): valutato
     * UNA VOLTA in un temporaneo __tdN boxed_val, poi un DIM per variabile che estrae per
     * indice (gmb_val_index), stesso schema emitter C#. Chiude Gap "Destrutturazione" di
     * Bugs/ (nota "tuple/parita transpiler/native") (prima: non riconosciuto affatto,
     * '(' veniva letto come se fosse il NOME della variabile DIM). */
    /* VAR è dichiarazione con tipo inferito: stessa semantica di DIM per il parser C
     * (il tipo si inferisce a codegen). Prima GMB_TOK_Var non era gestito → "statement
     * non riconosciuto" su `VAR x = [ ... ]` (ClipperAggregate/Dynamic). */
    if ((t->kind == GMB_TOK_Dim || t->kind == GMB_TOK_Var) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen) {
        int save_pos = p->pos;
        gmb_p_adv(p); /* DIM */
        gmb_p_adv(p); /* ( */
        char** dnames = NULL; int ndnames = 0, capdnames = 0;
        int dok = 1;
        for (;;) {
            const gmb_token_t* nt = gmb_p_cur(p);
            if (!nt || nt->kind != GMB_TOK_Identifier) { dok = 0; break; }
            if (ndnames >= capdnames) { capdnames = capdnames ? capdnames * 2 : 4; dnames = (char**)realloc(dnames, sizeof(char*) * (size_t)capdnames); }
            dnames[ndnames++] = gmb_p_tok_strdup(nt);
            gmb_p_adv(p);
            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        }
        if (dok && ndnames >= 1 && gmb_p_match(p, GMB_TOK_RightParen) && gmb_p_match(p, GMB_TOK_Equal)) {
            gmb_expr_t* rhs = gmb_p_parse_expr(p);
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) gmb_p_adv(p);
            static int g_dim_destr_seq = 0;
            int dseq = g_dim_destr_seq++;
            gmb_stmt_t* dhead = NULL;
            if (rhs && rhs->kind == GMB_EXPR_CALL && rhs->var && strcmp(rhs->var, "_tuple_lit_") == 0
                && rhs->nargs == ndnames) {
                for (int i = 0; i < ndnames; i++) {
                    gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
                    d->dim_var = strdup(dnames[i]);
                    d->dim_init = rhs->args[i]; rhs->args[i] = NULL; /* ownership trasferita */
                    if (!dhead) dhead = d; else gmb_p_queue_stmt(p, d);
                }
                gmb_expr_free(rhs); /* args già NULLed: libera solo il guscio */
            } else if (rhs) {
                char tdb[48]; snprintf(tdb, sizeof(tdb), "__td%d", dseq);
                gmb_stmt_t* td = gmb_stmt_new(GMB_STMT_DIM, line);
                td->dim_var = strdup(tdb);
                td->dim_init = rhs;
                dhead = td;
                for (int i = 0; i < ndnames; i++) {
                    gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
                    d->dim_var = strdup(dnames[i]);
                    /* target DINAMICO (gmb_val_t): l'elemento può essere di qualsiasi tipo
                     * (stringa/numerico/tupla) — sentinella letta da reg_dim. */
                    d->dim_type = strdup("__VAL__");
                    gmb_expr_t** ia = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*));
                    ia[0] = gmb_expr_num((double)i, line);
                    d->dim_init = gmb_expr_call(tdb, ia, 1, line);
                    gmb_p_queue_stmt(p, d);
                }
            }
            for (int i = 0; i < ndnames; i++) free(dnames[i]);
            free(dnames);
            if (dhead) return dhead;
            return gmb_stmt_new(GMB_STMT_REM, line);
        }
        /* non era una destrutturazione riconoscibile: ripristina e prosegui col DIM normale */
        for (int i = 0; i < ndnames; i++) free(dnames[i]);
        free(dnames);
        p->pos = save_pos;
    }
    if (t->kind == GMB_TOK_Dim || t->kind == GMB_TOK_Var) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DIM, line);
        const gmb_token_t* v = gmb_p_cur(p);
        if (v) { s->dim_var = (char*)malloc((size_t)v->length + 1); memcpy(s->dim_var, v->lexeme, (size_t)v->length); s->dim_var[v->length] = 0; gmb_p_adv(p); }
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            /* Bug corretto (2026-09-02): "DIM x()" (parentesi vuote, array dinamico stile
             * Clipper) lasciava array_size a NULL — IDENTICO a "DIM x" senza parentesi
             * affatto — quindi cadeva nel percorso scalare generico (init 0.0) invece che
             * creare una lista vuota vera: un successivo AADD non aveva nessun array reale
             * su cui operare. Il flag dim_empty_parens distingue i due casi. */
            if (gmb_p_check(p, GMB_TOK_RightParen)) s->dim_empty_parens = 1;
            else s->array_size = gmb_p_parse_expr(p);
            /* Canone 1-based 2026-09-30: DIM v(lo TO hi) — upper separato (array_size resta lo). */
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); s->dim_to_hi = gmb_p_parse_expr(p); }
            for (;;) {
                if (!gmb_p_check(p, GMB_TOK_Comma) || gmb_p_check(p, GMB_TOK_RightParen) || gmb_p_at_end(p)) break;
                /* virgola separatore dimensioni SOLO se non seguita da 'nome(' (forma multipla) */
                const gmb_token_t* n1 = gmb_p_peek(p, 1);
                const gmb_token_t* n2 = gmb_p_peek(p, 2);
                if (n1 && n1->kind == GMB_TOK_Identifier && n2 && n2->kind == GMB_TOK_LeftParen) break;
                gmb_p_adv(p);
                gmb_expr_t* d = gmb_p_parse_expr(p);
                gmb_expr_t* d_lo = NULL;
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); d_lo = d; d = gmb_p_parse_expr(p); }
                s->dim_extra = (gmb_expr_t**)realloc(s->dim_extra, (size_t)(s->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                s->dim_extra_lo = (gmb_expr_t**)realloc(s->dim_extra_lo, (size_t)(s->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                s->dim_extra[s->n_dim_extra] = d;
                s->dim_extra_lo[s->n_dim_extra] = d_lo;
                s->n_dim_extra++;
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        /* forma multipla: DIM a(2), b(2), c(2) → DIM aggiuntivi in coda (drenati dal blocco) */
        while (gmb_p_check(p, GMB_TOK_Comma) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Identifier
               && gmb_p_peek(p, 2) && gmb_p_peek(p, 2)->kind == GMB_TOK_LeftParen) {
            gmb_p_adv(p);   /* ',' */
            gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
            const gmb_token_t* dv = gmb_p_cur(p);
            if (dv) { d->dim_var = (char*)malloc((size_t)dv->length + 1); memcpy(d->dim_var, dv->lexeme, (size_t)dv->length); d->dim_var[dv->length] = 0; gmb_p_adv(p); }
            gmb_p_adv(p);   /* '(' */
            if (gmb_p_check(p, GMB_TOK_RightParen)) d->dim_empty_parens = 1;
            else d->array_size = gmb_p_parse_expr(p);
            /* Canone 1-based 2026-09-30: DIM v(lo TO hi). */
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); d->dim_to_hi = gmb_p_parse_expr(p); }
            for (;;) {
                if (!gmb_p_check(p, GMB_TOK_Comma) || gmb_p_check(p, GMB_TOK_RightParen) || gmb_p_at_end(p)) break;
                const gmb_token_t* n1 = gmb_p_peek(p, 1);
                const gmb_token_t* n2 = gmb_p_peek(p, 2);
                if (n1 && n1->kind == GMB_TOK_Identifier && n2 && n2->kind == GMB_TOK_LeftParen) break;
                gmb_p_adv(p);
                gmb_expr_t* dd = gmb_p_parse_expr(p);
                gmb_expr_t* dd_lo = NULL;
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); dd_lo = dd; dd = gmb_p_parse_expr(p); }
                d->dim_extra = (gmb_expr_t**)realloc(d->dim_extra, (size_t)(d->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                d->dim_extra_lo = (gmb_expr_t**)realloc(d->dim_extra_lo, (size_t)(d->n_dim_extra + 1) * sizeof(gmb_expr_t*));
                d->dim_extra[d->n_dim_extra] = dd;
                d->dim_extra_lo[d->n_dim_extra] = dd_lo;
                d->n_dim_extra++;
            }
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
            gmb_p_match(p, GMB_TOK_RightParen);
            gmb_p_queue_stmt(p, d);
        }
        /* forma multipla SCALARE senza inizializzatore: DIM I, J, K [, L] → un DIM per
         * ciascuna variabile successiva, in coda. Prima questi nomi venivano SCARTATI dal
         * drain "skip to newline" sotto (solo la prima variabile era dichiarata): i
         * contatori FOR omonimi risultavano "creati dal loop" invece che DIM esterni
         * (LoopsForNextLabelsLineNumbers: J/K non ripristinati dopo il ciclo). */
        while (gmb_p_check(p, GMB_TOK_Comma) && gmb_p_peek(p, 1)
               && gmb_p_peek(p, 1)->kind == GMB_TOK_Identifier
               && !(gmb_p_peek(p, 2) && gmb_p_peek(p, 2)->kind == GMB_TOK_LeftParen)) {
            gmb_p_adv(p);   /* ',' */
            gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
            const gmb_token_t* dv = gmb_p_cur(p);
            if (dv) { d->dim_var = (char*)malloc((size_t)dv->length + 1); memcpy(d->dim_var, dv->lexeme, (size_t)dv->length); d->dim_var[dv->length] = 0; gmb_p_adv(p); }
            /* `AS tipo` per la variabile MULTIPLA scalare (es. `DIM i AS INTEGER, r AS STRING`):
             * prima l'AS era ignorato → `r` restava numerico (bug DIM-multivar-stringa). */
            if (gmb_p_check(p, GMB_TOK_As) || gmb_p_tok_is_keyword(gmb_p_cur(p), "As")) {
                const gmb_token_t* at = gmb_p_peek(p, 1);
                if (at && at->lexeme) {
                    char tb[32]; size_t tl = (size_t)at->length < 31 ? (size_t)at->length : 31;
                    memcpy(tb, at->lexeme, tl); tb[tl] = 0;
                    /* Qualunque PAROLA come tipo (INTEGER/STRING/LONG/.../nome classe), come
                     * nel ramo scalare del 1° var — non gmb_tk_dim_sync_in (STRING/INTEGER non
                     * sono in quella lista). */
                    if (at->lexeme[0] == '_' || isalpha((unsigned char)at->lexeme[0])) {
                        gmb_p_adv(p);   /* AS */
                        gmb_p_adv(p);   /* nome tipo */
                        d->dim_type = (char*)malloc(tl + 1); memcpy(d->dim_type, tb, tl + 1);
                        d->dim_type_tk = (at->kind == GMB_TOK_Identifier || at->kind == GMB_TOK_Unknown) ? 0 : (int)at->kind;
                    }
                }
            }
            if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                if (gmb_p_check(p, GMB_TOK_RightParen)) d->dim_empty_parens = 1;
                else d->array_size = gmb_p_parse_expr(p);
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); d->dim_to_hi = gmb_p_parse_expr(p); }
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                gmb_p_match(p, GMB_TOK_RightParen);
            }
            if (gmb_p_match(p, GMB_TOK_Equal)) d->dim_init = gmb_p_parse_expr(p);
            gmb_p_queue_stmt(p, d);
        }
        /* inizializzatore: DIM x = <expr> (può essere un lambda FN a blocco fino a END FN) */
        if (gmb_p_match(p, GMB_TOK_Equal)) {
            s->dim_init = gmb_p_parse_expr(p);
            /* forma multipla SCALARE con inizializzatore: DIM a = 1, b = 2, c = 3 (bug: "b = 2,
             * c = 3" veniva scartato in blocco dal drain "skip to newline/colon" sotto — a era
             * l'unica variabile davvero dichiarata, b/c restavano non dichiarate, lette poi come
             * globali fallback a 0). Un DIM aggiuntivo per variabile, in coda (stesso schema del
             * ramo array "DIM a(2), b(2)" sopra, gmb_p_queue_stmt) — ogni variabile successiva
             * può avere il proprio inizializzatore (', nome = expr'), essere un array
             * (', nome(dim)' [= init]), o restare senza (', nome' — default scalare 0). */
            while (gmb_p_check(p, GMB_TOK_Comma) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Identifier) {
                gmb_p_adv(p);   /* ',' */
                gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
                const gmb_token_t* dv = gmb_p_cur(p);
                if (dv) { d->dim_var = (char*)malloc((size_t)dv->length + 1); memcpy(d->dim_var, dv->lexeme, (size_t)dv->length); d->dim_var[dv->length] = 0; gmb_p_adv(p); }
                if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                    if (gmb_p_check(p, GMB_TOK_RightParen)) d->dim_empty_parens = 1;
                    else d->array_size = gmb_p_parse_expr(p);
                    /* Canone 1-based 2026-09-30: DIM v(lo TO hi). */
                    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); d->dim_to_hi = gmb_p_parse_expr(p); }
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                    gmb_p_match(p, GMB_TOK_RightParen);
                }
                if (gmb_p_match(p, GMB_TOK_Equal)) d->dim_init = gmb_p_parse_expr(p);
                gmb_p_queue_stmt(p, d);
            }
            while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) gmb_p_adv(p);
            return s;
        }
        /* AS <tipo sync GPM> (CHANNEL/WAITGROUP/MUTEX/RWMUTEX/ONCE/COND/ATOMIC/POOL/SYNCMAP/CONTEXT):
           cattura per il codegen C99 */
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
            const gmb_token_t* at = gmb_p_peek(p, 1);
            if (at && (at->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(at, "Sub"))) {
                /* DIM v AS FUNCTION(...) AS <ret> / DIM v AS SUB(...): variabile funzione
                 * di prima classe (closure restituita da un'altra FUNCTION e chiamata più
                 * tardi — vedi FunctionsAdvanced.bas). Cattura solo l'ARIETÀ (numero di
                 * parametri) e se il ritorno è STRING: i parametri della FIRMA sono sempre
                 * numerici in questo emitter (stesso limite di tutto il resto del backend
                 * C99 nativo, dove ogni valore scalare non-stringa è gmb_double_t) — un
                 * parametro STRING nella firma non è supportato (rischio accettato, nessun
                 * sample esistente lo esercita). */
                gmb_p_adv(p);   /* AS */
                gmb_p_adv(p);   /* FUNCTION/SUB */
                s->dim_type = (char*)malloc(9); memcpy(s->dim_type, "FUNCTION", 9);
                s->dim_type_tk = GMB_TOK_Function;
                int arity = gmb_p_fn_sig(p, &s->func_val_str_mask, NULL, NULL);
                s->func_val_arity = arity;
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                    gmb_p_adv(p);
                    const gmb_token_t* rt = gmb_p_cur(p);
                    if (gmb_p_tok_is_string_type_name(rt))
                        s->func_val_ret_is_str = 1;
                    if (rt && gmb_keyword_tok(rt) == GMB_TOK_Array) s->func_val_ret_is_arr = 1;
                    if (rt) gmb_p_adv(p);
                }
                if (gmb_p_match(p, GMB_TOK_Equal)) {
                    /* `DIM saluta AS FUNCTION(...) AS STRING = FN(...) ... END FN`
                     * (FunctionsLambda.bas): un letterale lambda multi-riga come
                     * inizializzatore — va delegato a gmb_p_parse_expr come QUALUNQUE altro
                     * `DIM x = <expr>` (stesso schema poco sopra in questa stessa funzione),
                     * MAI un semplice skip-to-newline/colon: il corpo del lambda continua su
                     * più righe fino a un terminatore che solo il parser di espressioni sa
                     * riconoscere — uno skip cieco disallineava il parser (bug verificato dal
                     * vivo: "blocco non terminato" molte righe dopo). La compilazione EFFETTIVA
                     * del valore lambda-come-gmb_fn_t resta un gap noto e separato lato
                     * emitter (nessun caso GMB_EXPR_LAMBDA in emit_expr/etype) — qui serve
                     * solo non corrompere il parsing del resto del file.
                     */
                    s->dim_init = gmb_p_parse_expr(p);
                }
                while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) gmb_p_adv(p);
                return s;
            }
            if (at && at->lexeme) {
                char tb[32]; size_t tl = (size_t)at->length < 31 ? (size_t)at->length : 31;
                memcpy(tb, at->lexeme, tl); tb[tl] = 0;
                gmb_tokenkind_t dk0 = (at->kind == GMB_TOK_Identifier || at->kind == GMB_TOK_Unknown)
                                      ? (gmb_tokenkind_t)0 : at->kind;
                if (gmb_tk_dim_sync_in(dk0)) {
                    gmb_p_adv(p);   /* AS */
                    gmb_p_adv(p);   /* nome tipo */
                    s->dim_type = (char*)malloc(tl + 1); memcpy(s->dim_type, tb, tl + 1);
                    s->dim_type_tk = (int)dk0;
                    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                        const gmb_token_t* cn = gmb_p_cur(p);
                        if (cn && cn->kind == GMB_TOK_Number) { s->dim_chan_cap = atoi(gmb_p_tok_strdup(cn)); gmb_p_adv(p); }
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                        gmb_p_match(p, GMB_TOK_RightParen);
                    }
                    if (gmb_p_check(p, GMB_TOK_Of)) {
                        gmb_p_adv(p);
                        const gmb_token_t* el = gmb_p_cur(p);
                        if (el && el->kind == GMB_TOK_StringType) s->dim_elem_str = 1;
                        if (el) gmb_p_adv(p);
                    }
                    /* capacità DOPO la clausola OF: `CHANNEL OF INTEGER (3)` (sintassi canonica) */
                    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                        const gmb_token_t* cn = gmb_p_cur(p);
                        if (cn && cn->kind == GMB_TOK_Number) { s->dim_chan_cap = atoi(gmb_p_tok_strdup(cn)); gmb_p_adv(p); }
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                        gmb_p_match(p, GMB_TOK_RightParen);
                    }
                    /* NON fare skip-to-newline+return: lascia che il DRAIN finale gestisca
                     * `, var [AS tipo]` (es. `DIM i AS INTEGER, r AS STRING` → r dichiarato). */
                    goto dim_trailing;
                }
                /* AS <tipo scalare o classe> (STRING/LONG/.../nome classe, anche una KEYWORD
                   usata come nome di classe es. `BOX` → GMB_TOK_Box): accetta qualunque
                   PAROLA (lexeme che inizia per lettera/underscore) come tipo. */
                if (at->lexeme && (at->lexeme[0] == '_' || isalpha((unsigned char)at->lexeme[0]))) {
                    gmb_p_adv(p);   /* AS */
                    /* `AS MATRIX OF (assi…, T)`: DIM di una matrice rank-n (double* + __bK).
                     * Il cursore resta DOPO la `)` (gmb_parse_matrix_of consuma tutto). */
                    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Matrix) {
                        const gmb_token_t* vt = NULL;
                        int axes = gmb_parse_matrix_of(p, &vt);
                        if (axes >= 2 && vt && vt->lexeme) {
                            size_t eln = (size_t)vt->length < 31 ? (size_t)vt->length : 31;
                            memcpy(tb, vt->lexeme, eln); tb[eln] = 0;
                            dk0 = (vt->kind == GMB_TOK_Identifier || vt->kind == GMB_TOK_Unknown) ? (gmb_tokenkind_t)0 : vt->kind;
                            s->dim_ndims = axes;
                        }
                        goto dim_type_done;
                    }
                    /* `AS ARRAY OF T`: il tipo dichiarato è l'ELEMENTO T; il rank viene
                     * dagli assi del DIM (mai un array annidato: ARRAY OF ARRAY non esiste). */
                    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Array || gmb_p_tok_is_keyword(gmb_p_cur(p), "Array"))) {
                        const gmb_token_t* el = gmb_consume_array_of(p);
                        if (el && el->lexeme) {
                            size_t eln = (size_t)el->length < 31 ? (size_t)el->length : 31;
                            memcpy(tb, el->lexeme, eln); tb[eln] = 0;
                            dk0 = (el->kind == GMB_TOK_Identifier || el->kind == GMB_TOK_Unknown) ? (gmb_tokenkind_t)0 : el->kind;
                        } else {
                            tb[0] = 0; dk0 = GMB_TOK_Double;
                        }
                    }
                    gmb_p_adv(p);   /* nome tipo */
dim_type_done:
                    /* Tipo QUALIFICATO `AS NS.Cls` (es. AGENTS.AgentHandle): il C99Native è
                     * monofile e registra le classi col nome nudo (NAMESPACE è no-op) → si
                     * usa l'ULTIMO segmento dopo il punto come dim_type. Bug 2026-09-03: il
                     * '.' non veniva consumato e il DIM generava statement spuri. */
                    if (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1)
                        && gmb_p_peek(p, 1)->lexeme
                        && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline
                        && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof
                        && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftParen) {
                        /* tipo qualificato NS.Cls (anche multi-segmento `A.B.C`): il segmento
                         * può NON essere un GMB_TOK_Identifier (nomi-classe tokenizzati come
                         * keyword); si consuma comunque e si salva l'ULTIMO segmento (nome nudo
                         * della classe, come registrata: il NAMESPACE è no-op nel C99Native). */
                        const gmb_token_t* sub = NULL;
                        while (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1)
                               && gmb_p_peek(p, 1)->lexeme
                               && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline
                               && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof
                               && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftParen) {
                            gmb_p_adv(p);   /* '.' */
                            sub = gmb_p_cur(p);
                            gmb_p_adv(p);   /* segmento */
                        }
                        if (sub) {
                            free(s->dim_type);
                            s->dim_type = (char*)malloc((size_t)sub->length + 1);
                            memcpy(s->dim_type, sub->lexeme, (size_t)sub->length);
                            s->dim_type[sub->length] = 0;
                            s->dim_type_tk = 0;   /* classe qualificata NS.Cls */
                        }
                    } else {
                        s->dim_type = (char*)malloc(tl + 1); memcpy(s->dim_type, tb, tl + 1);
                        s->dim_type_tk = (int)dk0;
                    }
                    /* tipo GENERICO `Base<A1,A2>` (es. `Pair<INTEGER,STRING>`): cattura gli
                     * argomenti (CSV) per la specializzazione (`Pair_INTEGER_STRING_`); consuma
                     * il `<...>` che altrimenti il drain salterebbe (parità C#). */
                    if (s->dim_type_tk == 0 && gmb_p_check(p, GMB_TOK_LessThan)) {
                        gmb_p_adv(p);   /* '<' */
                        char ta[128]; size_t tal = 0; ta[0] = 0; int gdepth = 1;
                        while (!gmb_p_at_end(p) && gdepth > 0) {
                            const gmb_token_t* tt = gmb_p_cur(p);
                            if (gmb_p_check(p, GMB_TOK_LessThan)) gdepth++;
                            else if (gmb_p_check(p, GMB_TOK_GreaterThan)) { gdepth--; if (gdepth == 0) break; }
                            if (tt && tt->lexeme && tt->length
                                && tt->kind != GMB_TOK_Comma && tt->kind != GMB_TOK_Newline) {
                                if (tal && tal < sizeof ta - 2) ta[tal++] = ',';
                                size_t cl = (size_t)tt->length; if (tal + cl >= sizeof ta) cl = sizeof ta - tal - 1;
                                memcpy(ta + tal, tt->lexeme, cl); tal += cl; ta[tal] = 0;
                            }
                            gmb_p_adv(p);
                        }
                        gmb_p_match(p, GMB_TOK_GreaterThan);
                        if (tal) s->dim_type_args = strdup(ta);
                    }
                    /* `AS <scalare>?` (nullable): l'emitter la rappresenta come gmb_val_t boxed
                     * (NIL → gmb_val_nil()), così TYPEINFO/TYPEINFO.HAS sono parità 4 backend. */
                    if (gmb_p_check(p, GMB_TOK_QuestionMark)) { gmb_p_adv(p); s->dim_nullable = 1; }
                    /* ANALYSIS result-UDT (KMeansResult/PcaResult/AnovaResult/...): porta il
                     * token della BASE (<Base>Result) risolto qui nel parser — mai from_name a
                     * valle nell'emitter. Mappa dedicata: i tipi <Base>Result dell'ANALYSIS non
                     * sono procedure StdLib (non stanno nel CSV), quindi gmb_fn_tok non basta;
                     * Regression è l'anomalia (token interno P_Regression). */
                    {
                        static const struct { const char* base; gmb_tokenkind_t tk; } an_res[] = {
                            { "KMeans", GMB_TOK_KMeans },
                            { "Pca", GMB_TOK_Pca },
                            { "Anova", GMB_TOK_Anova },
                            { "Arima", GMB_TOK_Arima },
                            { "HoltWinters", GMB_TOK_HoltWinters },
                            { "AdfTest", GMB_TOK_AdfTest },
                            { "Regression", GMB_TOK_P_Regression },
                        };
                        const char* rr = s->dim_type ? strstr(s->dim_type, "Result") : NULL;
                        if (rr && rr[6] == '\0' && rr != s->dim_type) {
                            size_t bl2 = (size_t)(rr - s->dim_type);
                            if (bl2 < 64) {
                                char bb[64]; memcpy(bb, s->dim_type, bl2); bb[bl2] = 0;
                                s->dim_res_tk = (int)GMB_TOK_Unknown;
                                for (size_t ai = 0; ai < sizeof(an_res) / sizeof(an_res[0]); ai++)
                                    if (strcasecmp(bb, an_res[ai].base) == 0) {
                                        s->dim_res_tk = (int)an_res[ai].tk;
                                        break;
                                    }
                                if (s->dim_res_tk == (int)GMB_TOK_Unknown)
                                    s->dim_res_tk = (int)gmb_fn_tok(bb);
                            }
                        }
                    }
                    /* STRING * N: lunghezza fissa */
                    if (dk0 == GMB_TOK_StringType && gmb_p_check(p, GMB_TOK_Multiply)
                        && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Number) {
                        gmb_p_adv(p);   /* '*' */
                        char* nb = gmb_p_tok_strdup(gmb_p_cur(p));
                        s->dim_fixed_len = atoi(nb);
                        free(nb);
                        gmb_p_adv(p);   /* numero */
                    }
                    /* COLLECTIONS: AS LIST/QUEUE/STACK/HASHSET<STRING|...> e
                     * AS DICT/SORTED<...> — consuma i type argument generici e cattura il tipo
                     * ELEMENTO: i single-type (LIST/QUEUE/STACK/HASHSET) → 1° type arg,
                     * i map (DICT/SORTED) → SECONDO (il primo è la chiave). dim_elem_str=1 se STRING. */
                    if (dim_coll_kind(dk0)
                        && gmb_p_check(p, GMB_TOK_LessThan)) {
                        gmb_p_adv(p);   /* '<' */
                        int ntypes = 0;
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)
                               && !gmb_p_check(p, GMB_TOK_Newline)) {
                            const gmb_token_t* gtk = gmb_p_cur(p);
                            if (gtk && gtk->lexeme && isalpha((unsigned char)gtk->lexeme[0])) {
                                ntypes++;
                                int want = dim_map_kind(dk0) ? 2 : 1;
                                if (ntypes == want) {
                                    if (gtk->kind == GMB_TOK_StringType)
                                        s->dim_elem_str = 1;
                                    else if (gtk->kind == GMB_TOK_Any)
                                        s->dim_elem_any = 1;   /* valore dinamico: gmb_val_t */
                                    else if (!gm_parser_prim_len(gtk->lexeme, gtk->length) && !s->dim_elem_class) {
                                        /* want==1: elem UDT di LIST<...>; want==2: tipo VALORE di
                                         * una mappa (DICT<...,JSON>/<...,UDT>) — un valore non
                                         * primitivo va memorizzato come gmb_val_t (percorso
                                         * "dict di val"), non come double (bug .Values().ToJson(),
                                         * Users[k]=v, v=Users[k] sui DICT<*,JSON>). */
                                        char ec[64]; int el = gtk->length < 63 ? gtk->length : 63;
                                        memcpy(ec, gtk->lexeme, (size_t)el); ec[el] = 0;
                                        s->dim_elem_class = strdup(ec);
                                    }
                                }
                            }
                            gmb_p_adv(p);
                        }
                        gmb_p_match(p, GMB_TOK_GreaterThan);
                    }
                    /* COLLECTIONS (legacy OF): AS LIST OF STRING / AS DICT OF STRING, DOUBLE … */
                    if (dim_coll_kind(dk0)
                        && gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Of) {
                        gmb_p_adv(p);
                        int ntypes = 0;
                        for (;;) {
                            if (gmb_p_at_end(p)) break;
                            if (gmb_p_check(p, GMB_TOK_Equal)) break;
                            const gmb_token_t* otk = gmb_p_cur(p);
                            if (!otk) break;
                            if (otk->lexeme && isalpha((unsigned char)otk->lexeme[0])) {
                                ntypes++;
                                int want = dim_map_kind(dk0) ? 2 : 1;
                                if (ntypes == want) {
                                    if (otk->kind == GMB_TOK_StringType)
                                        s->dim_elem_str = 1;
                                    else if (otk->kind == GMB_TOK_Any)
                                        s->dim_elem_any = 1;   /* valore dinamico: gmb_val_t */
                                    else if (!gm_parser_prim_len(otk->lexeme, otk->length) && !s->dim_elem_class) {
                                        /* see ramo generico sopra: want==2 = valore mappa non
                                         * primitivo → percorso "dict di val" (gmb_val_t). */
                                        char ec2[64]; int el2 = otk->length < 63 ? otk->length : 63;
                                        memcpy(ec2, otk->lexeme, (size_t)el2); ec2[el2] = 0;
                                        s->dim_elem_class = strdup(ec2);
                                    }
                                }
                                gmb_p_adv(p);
                            } else if (gmb_p_check(p, GMB_TOK_Comma)) {
                                gmb_p_adv(p);
                            } else break;
                        }
                    }
                }
            }
        }
        /* consuma il resto: AS <tipo> [, var [AS tipo]] ... (parità Parser.Declarations) */
        dim_trailing: ;
        int cap = 0;
        while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) {
            /* `, var [AS tipo] [= init]` dopo il TIPO della prima variabile (es.
             * `DIM i AS INTEGER, r AS STRING`, o `DIM d AS DICT<...>, altro`): DICHIARA la
             * variabile aggiuntiva (prima il drain la SCARTAVA → r restava numerico). */
            /* Nome-like (anche KEYWORD usato come nome: `Scale`, `X`, `To`... — es.
             * `DIM Rot#(N), Scale#(N)` dove SCALE è un TokenKind grafico): accetta qualunque token
             * che INIZI come un identificatore, esclusi i soli token STRUTTURALI della DIM
             * (As/To/Then/=). Prima richiedeva GMB_TOK_Identifier → il 2° nome keyword NON veniva
             * dichiarato (l'array `Scale#` restava sconosciuto → "non è un array"). */
            const gmb_token_t* nxt_dv = gmb_p_peek(p, 1);
            int dv_name_like = nxt_dv && nxt_dv->lexeme && nxt_dv->length > 0
                && (nxt_dv->lexeme[0] == '_' || isalpha((unsigned char)nxt_dv->lexeme[0]))
                && nxt_dv->kind != GMB_TOK_As && nxt_dv->kind != GMB_TOK_To
                && nxt_dv->kind != GMB_TOK_Then && nxt_dv->kind != GMB_TOK_Equal;
            if (gmb_p_check(p, GMB_TOK_Comma) && dv_name_like) {
                gmb_p_adv(p);   /* ',' */
                gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
                const gmb_token_t* dv = gmb_p_cur(p);
                if (dv) { d->dim_var = (char*)malloc((size_t)dv->length + 1); memcpy(d->dim_var, dv->lexeme, (size_t)dv->length); d->dim_var[dv->length] = 0; gmb_p_adv(p); }
                if (gmb_p_check(p, GMB_TOK_As) || gmb_p_tok_is_keyword(gmb_p_cur(p), "As")) {
                    const gmb_token_t* at = gmb_p_peek(p, 1);
                    if (at && at->lexeme) {
                        char tb[32]; size_t tl = (size_t)at->length < 31 ? (size_t)at->length : 31;
                        memcpy(tb, at->lexeme, tl); tb[tl] = 0;
                        if (at->lexeme[0] == '_' || isalpha((unsigned char)at->lexeme[0])) {
                            gmb_p_adv(p);   /* AS */
                            gmb_p_adv(p);   /* nome tipo */
                            d->dim_type = (char*)malloc(tl + 1); memcpy(d->dim_type, tb, tl + 1);
                            d->dim_type_tk = (at->kind == GMB_TOK_Identifier || at->kind == GMB_TOK_Unknown) ? 0 : (int)at->kind;
                        }
                    }
                }
                if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                    if (gmb_p_check(p, GMB_TOK_RightParen)) d->dim_empty_parens = 1;
                    else d->array_size = gmb_p_parse_expr(p);
                    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
                    gmb_p_match(p, GMB_TOK_RightParen);
                }
                /* `DIM n AS LONG, b$() AS STRING`: qui il tipo si cercava SOLO prima delle
                 * parentesi. Con la forma `nome() AS STRING` il flag dim_empty_parens non
                 * veniva impostato e `AS STRING` restava non consumato → la variabile non era
                 * un array dinamico di stringhe (`b$ = SPLIT(...)` falliva con "SPLIT richiede
                 * un target DIM ...() AS STRING dinamico"; `__len`/UBOUND assenti). */
                if (!d->dim_type && (gmb_p_check(p, GMB_TOK_As) || gmb_p_tok_is_keyword(gmb_p_cur(p), "As"))) {
                    const gmb_token_t* at2 = gmb_p_peek(p, 1);
                    if (at2 && at2->lexeme && (at2->lexeme[0] == '_' || isalpha((unsigned char)at2->lexeme[0]))) {
                        char tb2[32]; size_t tl2 = (size_t)at2->length < 31 ? (size_t)at2->length : 31;
                        memcpy(tb2, at2->lexeme, tl2); tb2[tl2] = 0;
                        gmb_p_adv(p);   /* AS */
                        gmb_p_adv(p);   /* nome tipo */
                        d->dim_type = (char*)malloc(tl2 + 1); memcpy(d->dim_type, tb2, tl2 + 1);
                        d->dim_type_tk = (at2->kind == GMB_TOK_Identifier || at2->kind == GMB_TOK_Unknown) ? 0 : (int)at2->kind;
                    }
                }
                if (gmb_p_match(p, GMB_TOK_Equal)) d->dim_init = gmb_p_parse_expr(p);
                gmb_p_queue_stmt(p, d);
                continue;
            }
            if (gmb_p_match(p, GMB_TOK_Comma)) continue;
            if (gmb_p_match(p, GMB_TOK_Equal)) {
                /* inizializzatore dopo il tipo: DIM D AS DICT<...> = { ... } */
                s->dim_init = gmb_p_parse_expr(p);
                while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p)) gmb_p_adv(p);
                return s;
            }
            const gmb_token_t* x = gmb_p_cur(p);
            if (!x) break;
            gmb_expr_t* e;
            if (x->kind == GMB_TOK_Number) { char* nx = gmb_p_tok_strdup(x); e = gmb_expr_num(gmb_p_num_literal(nx), line); free(nx); }
            else e = (x->kind == GMB_TOK_StringLiteral ? gmb_expr_str(gmb_p_tok_strdup(x), line)
                          : gmb_expr_var(gmb_p_tok_strdup(x), line));
            if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
            s->go_args[s->n_go_args++] = e;
            gmb_p_adv(p);
        }
        return s;
    }
    if (t->kind == GMB_TOK_Return) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_RETURN, line);
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
            /* RETURN (a, b): prima veniva RILEVATA (lookahead virgola top-level) e poi
             * SCARTATA di proposito (scansione a profondità fino a ')' senza costruire
             * nulla — s->ret_value restava NULL, la funzione ritornava sempre il default
             * "vuoto"). Il letterale tupla ORA lo costruisce gmb_p_parse_expr stesso
             * (case GMB_TOK_LeftParen in gmb_parser_expressions.c → CALL "_tuple_lit_",
             * fix tuple/parita transpiler-native): nessuna gestione speciale qui,
             * basta lasciar fare al parser di espressioni generico, come per qualunque
             * altro RETURN. */
            s->ret_value = gmb_p_parse_expr(p);
            /* ritorno multiplo LEGACY senza parentesi (RETURN a, b, c — raro, comportamento
             * preesistente invariato): consuma ", expr..." fino a fine riga, gli extra
             * restano scartati (limite noto, non introdotto qui). RETURN (a, b) con
             * parentesi non passa più da qui: e' già consumato per intero dal ramo sopra. */
            while (gmb_p_match(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
                gmb_expr_t* extra = gmb_p_parse_expr(p);
                if (extra) gmb_expr_free(extra);
            }
        }
        return s;
    }
    /* member access come statement: obj.metodo(args) / obj.prop — parse come espressione */
    if ((t->kind == GMB_TOK_Identifier || t->kind >= GMB_TOK_Define) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Dot) {
        gmb_stmt_t* es = gmb_stmt_new(GMB_STMT_EXPR, line);
        es->expr = gmb_p_parse_expr(p);
        return es;
    }
    /* label: token Label oppure Identifier seguito da ':' */
    if (t->kind == GMB_TOK_Label || (t->kind == GMB_TOK_Identifier && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Colon)) {
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_LABEL, line);
        const gmb_token_t* lbl = t;
        if (t->kind == GMB_TOK_Label) {
            /* forma 'LABEL <nome>' — il nome è il token successivo */
            gmb_p_adv(p);
            const gmb_token_t* n = gmb_p_cur(p);
            lbl = n;
            if (n) { gmb_p_adv(p); }
        } else {
            /* forma 'Nome:' — adv dopo il nome */
            gmb_p_adv(p);
        }
        if (lbl) {
            s->target_label = (char*)malloc((size_t)lbl->length + 1);
            memcpy(s->target_label, lbl->lexeme, (size_t)lbl->length);
            s->target_label[lbl->length] = 0;
        }
        if (gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);   /* ':' */
        return s;
    }
    if (t->kind == GMB_TOK_Goto) { gmb_p_adv(p); return gmb_p_parse_goto(p, line); }
    if (t->kind == GMB_TOK_Gosub) { gmb_p_adv(p); return gmb_p_parse_gosub(p, line); }
    if (t->kind == GMB_TOK_End) {
        /* END WORKFLOW → chiusura definizione workflow (domain WORKFLOW → gmb_workflow_end). */
        const gmb_token_t* en = gmb_p_peek(p, 1);
        if (en && (gmb_p_tok_is_keyword(en, "Workflow") || en->kind == GMB_TOK_WorkflowStatement)) {
            gmb_p_adv(p); gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DOMAIN, line);
            s->win_name = strdup("WORKFLOW");
            return s;
        }
        /* `END IF` / `END SELECT` arrivati QUI sono ORFANI (i parser di blocco consumano il proprio END prima di tornare): senza
         * questo, `END` diventava lo statement 'fine programma' e `IF` apriva un IF senza THEN ("atteso THEN"), fuorviante. Tipico
         * caso: la forma MISTA `IF c THEN stmt ELSE` + blocco + `END IF`. Stessa diagnostica del parser C#. */
        if (en && en->line == t->line && (en->kind == GMB_TOK_If || gmb_p_tok_is_keyword(en, "If"))) {
            gmb_p_set_err(p, "END IF without a matching IF (a single-line 'IF c THEN stmt [ELSE stmt]' has no END IF; "
                             "if you mixed it with a multi-line block, use the block form 'IF c THEN' / 'ELSE' / 'END IF')", line);
            gmb_p_adv(p); gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_END, line);
        }
        if (en && en->line == t->line && (en->kind == GMB_TOK_Select || gmb_p_tok_is_keyword(en, "Select"))) {
            gmb_p_set_err(p, "END SELECT without a matching SELECT CASE", line);
            gmb_p_adv(p); gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_END, line);
        }
        gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_END, line);
    }
    if (t->kind == GMB_TOK_Exit) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_EXIT, line);
        const gmb_token_t* tg = gmb_p_cur(p);
        if (tg && tg->kind != GMB_TOK_Newline && tg->kind != GMB_TOK_Eof && tg->kind != GMB_TOK_Colon) {
            /* EXIT TICK → break del TICK (reso come DO WHILE 1): target canonico "DO". */
            const char* tn = (tg->kind == GMB_TOK_Tick) ? "DO" : NULL;
            if (tn) { s->exit_target = (char*)malloc(strlen(tn) + 1); memcpy(s->exit_target, tn, strlen(tn) + 1); }
            else { s->exit_target = (char*)malloc((size_t)tg->length + 1); memcpy(s->exit_target, tg->lexeme, (size_t)tg->length); s->exit_target[tg->length] = 0; }
            gmb_p_adv(p);
        }
        else if (p->n_loop_stack > 0) {
            /* EXIT nudo dentro un loop: eredita il kind più interno → l'emitter fa break
             * (canone 2026-09-30; prima usciva dalla SUB con return). Fuori dai loop resta
             * senza target → return (invariato). */
            int lk = p->loop_stack[p->n_loop_stack - 1];
            const char* ln = (lk == GMB_TOK_While) ? "WHILE" : (lk == GMB_TOK_Do) ? "DO" : "FOR";
            s->exit_target = (char*)malloc(strlen(ln) + 1); memcpy(s->exit_target, ln, strlen(ln) + 1);
        }
        return s;
    }
    if (t->kind == GMB_TOK_Continue) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CONTINUE, line);
        const gmb_token_t* tg = gmb_p_cur(p);
        if (tg && tg->kind != GMB_TOK_Newline && tg->kind != GMB_TOK_Eof && tg->kind != GMB_TOK_Colon) { s->exit_target = (char*)malloc((size_t)tg->length + 1); memcpy(s->exit_target, tg->lexeme, (size_t)tg->length); s->exit_target[tg->length] = 0; gmb_p_adv(p); }
        return s;
    }
    /* ++t / --t (prefix) come STATEMENT → `t = t ± 1` (qualunque target: variabile, campo
     * nudo/qualificato, elemento di array). */
    if (t->kind == GMB_TOK_PlusPlus || t->kind == GMB_TOK_MinusMinus) {
        int plus = t->kind == GMB_TOK_PlusPlus;
        gmb_p_adv(p);
        gmb_expr_t* op = gmb_p_parse_unary(p);
        if (!op) { gmb_p_set_err(p, "++/--: operando mancante", line); return NULL; }
        return gmb_p_make_incdec(op, plus, line);
    }
    if (t->kind == GMB_TOK_Identifier || t->kind == GMB_TOK_Let) {
        bool is_let = t->kind == GMB_TOK_Let;
        if (is_let) gmb_p_adv(p);
        const gmb_token_t* target = gmb_p_cur(p);
        if (!target) { gmb_p_set_err(p, "identificatore atteso", line); return NULL; }
        char* name = (char*)malloc((size_t)target->length + 1); memcpy(name, target->lexeme, (size_t)target->length); name[target->length] = 0;
        if (!is_let && strcasecmp(name, "CALL") == 0) {
            gmb_p_adv(p);
            const gmb_token_t* fn = gmb_p_cur(p);
            if (fn) { free(name); name = (char*)malloc((size_t)fn->length + 1); memcpy(name, fn->lexeme, (size_t)fn->length); name[fn->length] = 0; gmb_p_adv(p); }
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CALL, line);
            s->expr = gmb_p_parse_primary(p);
            if (s->expr && s->expr->kind != GMB_EXPR_CALL) { gmb_expr_free(s->expr); s->expr = gmb_expr_call(name, NULL, 0, line); }
            free(name);
            return s;
        }
        gmb_p_adv(p);
        gmb_stmt_t* st = gmb_p_parse_assign_or_call(p, name, line);
        if (st && is_let) st->is_let = 1;
        free(name);
        return st;
    }
    /* assegnazione tuple: (a, b) = (b, a) — statement che inizia con '('.
     * Prima: le variabili LHS oltre la prima venivano scartate (solo `first`) e il RHS
     * veniva scansionato e SCARTATO senza costruire nulla, sostituito da un sentinella
     * "_tuple_" senza dati -> lo swap non scriveva mai i valori reali (bug
     * tuple/parita transpiler-native). Ora: raccoglie TUTTI i nomi LHS, parsa il RHS
     * col parser di espressioni generico (che per `(e1, e2, ...)` costruisce CALL
     * "_tuple_lit_", fix parallelo in gmb_parser_expressions.c), e desugar in
     * temporanee — come l'equivalente emitter C# (TupleAssignmentStmt): valuta il tuple
     * destro in temporanee PRIMA di scrivere le variabili (per lo scambio a,b = b,a) —
     * un DIM per elemento poi un'ASSIGN per variabile, accodati con gmb_p_queue_stmt
     * (stesso schema già in uso per "DIM a(2), b(2)"). Solo il RHS LETTERALE
     * `(e1, e2, ...)` con lo STESSO numero di elementi del LHS è gestito (come
     * TupleAssignmentStmt in emitter C#): un RHS non letterale o con arità diversa resta
     * un gap noto (fuori scope, nessun sample bersaglio lo usa) — statement no-op. */
    if (t->kind == GMB_TOK_LeftParen) {
        gmb_p_adv(p); /* '(' */
        char** names = NULL; int nnames = 0, capnames = 0;
        for (;;) {
            const gmb_token_t* nt = gmb_p_cur(p);
            if (!nt || nt->kind != GMB_TOK_Identifier) break;
            if (nnames >= capnames) { capnames = capnames ? capnames * 2 : 4; names = (char**)realloc(names, sizeof(char*) * (size_t)capnames); }
            names[nnames++] = gmb_p_tok_strdup(nt);
            gmb_p_adv(p);
            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        }
        gmb_p_match(p, GMB_TOK_RightParen);
        gmb_stmt_t* result = gmb_stmt_new(GMB_STMT_REM, line);
        if (nnames >= 1 && gmb_p_match(p, GMB_TOK_Equal)) {
            gmb_expr_t* rhs = gmb_p_parse_expr(p);
            if (rhs && rhs->kind == GMB_EXPR_CALL && rhs->var && strcmp(rhs->var, "_tuple_lit_") == 0
                && rhs->nargs == nnames) {
                static int g_tuple_asgn_seq = 0;
                int seq = g_tuple_asgn_seq++;
                gmb_stmt_t* head = NULL;
                for (int i = 0; i < nnames; i++) {
                    gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
                    char tb[48]; snprintf(tb, sizeof(tb), "__ta%d_%d", seq, i);
                    d->dim_var = strdup(tb);
                    d->dim_init = rhs->args[i]; rhs->args[i] = NULL; /* ownership trasferita */
                    if (!head) head = d; else gmb_p_queue_stmt(p, d);
                }
                for (int i = 0; i < nnames; i++) {
                    gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                    a->target = strdup(names[i]);
                    char tb[48]; snprintf(tb, sizeof(tb), "__ta%d_%d", seq, i);
                    a->value = gmb_expr_var(tb, line);
                    gmb_p_queue_stmt(p, a);
                }
                gmb_expr_free(rhs); /* args già NULLed/trasferiti: libera solo il guscio */
                result = head;
            } else if (rhs) {
                gmb_expr_free(rhs);
            }
        }
        for (int i = 0; i < nnames; i++) free(names[i]);
        free(names);
        return result;
    }
    /* keyword seguita da '(' → assign_or_call: copre sia chiamata come statement
       (es. KEYIMPORT(...)) sia assegnazione a elemento array quando la variabile ha
       il nome di una keyword (es. data(0) = 1) */
    if (t->kind != GMB_TOK_Ampersand && t->kind != GMB_TOK_Call && t->kind != GMB_TOK_Eval
        && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen) {
        gmb_p_adv(p);
        char* kname = gmb_p_tok_strdup(t);
        gmb_stmt_t* st = gmb_p_parse_assign_or_call(p, kname, line);
        free(kname);
        return st;
    }
    /* &(...) / &expr — macro eval come statement. Bug corretto (2026-08-31, audit
     * macro/EVAL): questo ramo consumava '&' QUI e poi chiamava gmb_p_parse_expr sul
     * resto — che quindi non vedeva mai il '&' (già consumato), perdendo la semantica
     * macro (il testo veniva valutato come una NORMALE espressione, mai come codice a
     * runtime). Fix: NON consumare '&' qui — lascialo al parser di espressioni
     * (gmb_p_parse_primary), che lo riconosce e produce il nodo sentinella "_macro_"
     * (stesso schema di "_array_"/"_switch_"). GMB_STMT_EXPR (cc_expr + OP_POP, già
     * generico) scarta correttamente il risultato che OP_EVAL_MACRO_EXPR ripusha —
     * nessun opcode/ramo statement dedicato necessario. */
    if (t->kind == GMB_TOK_Ampersand || t->kind == GMB_TOK_Eval) {
        /* `&(cmd)` e `EVAL(...)` come statement: il parser di espressioni riconosce
         * entrambi e produce il nodo sentinella `_macro_` (prima EVAL cadeva nel ramo
         * call generico → "funzione 'EVAL' non definita"). */
        gmb_stmt_t* ev = gmb_stmt_new(GMB_STMT_EXPR, line);
        ev->expr = gmb_p_parse_expr(p);
        return ev;
    }
    if (t->kind == GMB_TOK_Call) {
        gmb_p_adv(p);
        gmb_stmt_t* cs = gmb_stmt_new(GMB_STMT_EXPR, line);
        cs->expr = gmb_p_parse_expr(p);
        return cs;
    }
    /* keyword usata come variabile seguita da '=' (es. ok = ..., err = ...):
     * nessun handler specifico l'ha riconosciuta → è un'assegnazione */
    if (gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Equal) {
        gmb_p_adv(p);
        gmb_stmt_t* st = gmb_stmt_new(GMB_STMT_ASSIGN, line);
        st->target = gmb_p_tok_strdup(t);
        if (!gmb_p_match(p, GMB_TOK_Equal)) { gmb_p_set_err(p, "atteso =", line); return st; }
        st->value = gmb_p_parse_expr(p);
        return st;
    }
    gmb_p_set_err(p, "statement non riconosciuto", line);
    return NULL;
}
