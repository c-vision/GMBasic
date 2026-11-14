/* gmb_parser_stmt_errdefer.c — `ERRDEFER <stmt>` (parità con `errdefer` di Zig).
 *
 *   SUB F()
 *     ERRDEFER CLOSE #1              ' eseguito SOLO se F esce per un errore
 *     ...
 *   END SUB
 *
 * SEMANTICA. Il cleanup corre quando la PROCEDURA esce perché un errore la attraversa:
 *   - errore catturato da un TRY/CATCH *dentro* la procedura → la procedura NON esce → il
 *     cleanup NON corre (come `errdefer` di Zig, che è legato all'uscita dal blocco);
 *   - errore che sfugge alla procedura (o RETURN/END con errore attivo) → cleanup e RILANCIO.
 * L'ordine di esecuzione è LIFO, come per DEFER.
 *
 * IMPLEMENTAZIONE (zero lavoro nei backend). Il parser NON emette nulla: al termine della
 * procedura il corpo viene avvolto in
 *
 *   TRY
 *     <corpo senza i marcatori ERRDEFER>
 *   CATCH __gmb_ed_e AS ERR
 *     <cleanup, in ordine inverso>
 *     ERROR __gmb_ed_e.Number, __gmb_ed_e.Description      ' rilancia lo STESSO errore
 *   END TRY
 *
 * cioè due costrutti che esistono già su tutte e 4 le gambe (TRY/CATCH, `AS ERR`, `ERROR`).
 * Il marcatore `GMB_STMT_ERRDEFER` è quindi solo un segnaposto di parsing: se ne arriva uno
 * all'emissione significa che è stato usato FUORI da una procedura, e il parser lo rifiuta.
 */
#include "gmb_parser_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* `ERRDEFER <stmt>` — il marcatore porta lo statement in `go_comm` (come DEFER). */
gmb_stmt_t* gmb_p_parse_errdefer(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ERRDEFER, line);
    bool ml = false;
    s->go_comm = gmb_p_parse_stmt(p, &ml);
    return s;
}

/* Espressione `var.Campo` (membro di un valore boxed): il parser rappresenta il member access
 * come VAR con nome QUALIFICATO (`e.Number`) — è quella forma che l'emitter risolve con il
 * binding della clausola CATCH (`catch_names`), non una CALL. */
static gmb_expr_t* member_expr(const char* var, const char* field, int line)
{
    size_t n = strlen(var) + strlen(field) + 2;
    char* name = (char*)malloc(n);
    if (!name) return NULL;
    snprintf(name, n, "%s.%s", var, field);
    gmb_expr_t* e = gmb_expr_var(name, line);
    free(name);
    return e;
}

/* Estrae i marcatori ERRDEFER da `list` — RICORSIVAMENTE nei corpi annidati (IF/FOR/WHILE/
 * DO/TRY/CATCH/FINALLY/SELECT) — togliendoli dai corpi e accodando il loro `go_comm` in `out`
 * in ordine di dichiarazione. Ritorna il numero di statement rimasti in `list`.
 *
 * Perché ricorsivo: `ERRDEFER` dentro un IF è legittimo (il cleanup è della PROCEDURA, come
 * in C# dove il marcatore finisce nello scope della procedura a qualunque profondità). Prima
 * si guardava solo il livello superiore: il marcatore annidato restava nel corpo e l'emitter
 * falliva con "statement non ancora supportato (kind=222)". */
static int extract_in_list(gmb_stmt_t** list, int n, gmb_stmt_t*** out, int* out_n, int* out_cap)
{
    if (!list || n <= 0) return n;
    int k = 0;
    for (int i = 0; i < n; i++) {
        gmb_stmt_t* s = list[i];
        if (!s) continue;
        if (s->kind == GMB_STMT_ERRDEFER) {
            if (s->go_comm) {
                if (*out_n >= *out_cap) {
                    *out_cap = *out_cap ? *out_cap * 2 : 8;
                    *out = (gmb_stmt_t**)realloc(*out, (size_t)(*out_cap) * sizeof(gmb_stmt_t*));
                }
                (*out)[(*out_n)++] = s->go_comm;
            }
            continue;   /* il marcatore non resta nel corpo */
        }
        s->n_then    = extract_in_list(s->then_body,    s->n_then,    out, out_n, out_cap);
        s->n_else    = extract_in_list(s->else_body,    s->n_else,    out, out_n, out_cap);
        s->n_for     = extract_in_list(s->for_body,     s->n_for,     out, out_n, out_cap);
        s->n_while   = extract_in_list(s->while_body,   s->n_while,   out, out_n, out_cap);
        s->n_do      = extract_in_list(s->do_body,      s->n_do,      out, out_n, out_cap);
        s->n_try     = extract_in_list(s->try_body,     s->n_try,     out, out_n, out_cap);
        s->n_catch   = extract_in_list(s->catch_body,   s->n_catch,   out, out_n, out_cap);
        s->n_finally = extract_in_list(s->finally_body, s->n_finally, out, out_n, out_cap);
        for (int c = 0; c < s->n_select_cases; c++)
            if (s->select_cases[c])
                s->select_cases[c]->n_case_body =
                    extract_in_list(s->select_cases[c]->case_body,
                                    s->select_cases[c]->n_case_body, out, out_n, out_cap);
        list[k++] = s;
    }
    return k;
}

/* Avvolge `body` nel TRY/CATCH dei cleanup se contiene almeno un ERRDEFER.
 * Ritorna il numero di ERRDEFER trovati (0 = corpo invariato). */
int gmb_p_wrap_errdefers(gmb_stmt_list_t* body, int line)
{
    if (!body || body->n <= 0) return 0;

    gmb_stmt_t** defers = NULL;
    int n_def = 0, cap_def = 0;
    body->n = extract_in_list(body->items, body->n, &defers, &n_def, &cap_def);
    if (n_def == 0) { free(defers); return 0; }

    /* Nome del binding UNIVOCO per procedura (contatore statico): con lo stesso nome in due
     * procedure un frontend/linker che risolve i nomi globalmente può far collidere il
     * secondo binding (sul frontend C# succedeva: "la base è un valore costante"). */
    static int s_ed_uid = 0;
    char ev[32];
    snprintf(ev, sizeof ev, "__gmb_ed_e_%d", ++s_ed_uid);

    /* ── clausola CATCH: cleanup (LIFO) + rilancio dell'errore ── */
    gmb_stmt_t* c = gmb_stmt_new(GMB_STMT_CATCH, line);
    c->catch_var = strdup(ev);
    c->catch_type = strdup("ERR");
    int cap = n_def + 1;
    c->catch_body = (gmb_stmt_t**)malloc((size_t)cap * sizeof(gmb_stmt_t*));
    c->n_catch = 0;
    /* LIFO: l'ultimo ERRDEFER dichiarato corre per primo. */
    for (int i = n_def - 1; i >= 0; i--) c->catch_body[c->n_catch++] = defers[i];
    free(defers);
    gmb_stmt_t* re = gmb_stmt_new(GMB_STMT_ERROR, line);
    re->error_code = member_expr(ev, "Number", line);
    re->throw_expr = member_expr(ev, "Description", line);
    c->catch_body[c->n_catch++] = re;

    /* ── TRY: il corpo originale (ormai senza marcatori) ── */
    gmb_stmt_t* try_s = gmb_stmt_new(GMB_STMT_TRY, line);
    try_s->try_body = (gmb_stmt_t**)malloc((size_t)(body->n + 1) * sizeof(gmb_stmt_t*));
    try_s->n_try = 0;
    for (int i = 0; i < body->n; i++)
        if (body->items[i]) try_s->try_body[try_s->n_try++] = body->items[i];
    try_s->catch_body = (gmb_stmt_t**)malloc(sizeof(gmb_stmt_t*));
    try_s->catch_body[0] = c;
    try_s->n_catch = 1;

    /* il corpo della procedura diventa il solo TRY */
    body->items[0] = try_s;
    body->n = 1;
    return n_def;
}

/* `ERRDEFER` fuori da una procedura: errore esplicito (il marcatore non ha nessuno che lo
 * trasformi e i backend non lo conoscono). */
int gmb_p_has_stray_errdefer(gmb_stmt_t** list, int n)
{
    for (int i = 0; i < n; i++) {
        gmb_stmt_t* s = list ? list[i] : NULL;
        if (!s) continue;
        if (s->kind == GMB_STMT_ERRDEFER) return 1;
        if (gmb_p_has_stray_errdefer(s->then_body, s->n_then)) return 1;
        if (gmb_p_has_stray_errdefer(s->else_body, s->n_else)) return 1;
        if (gmb_p_has_stray_errdefer(s->for_body, s->n_for)) return 1;
        if (gmb_p_has_stray_errdefer(s->while_body, s->n_while)) return 1;
        if (gmb_p_has_stray_errdefer(s->do_body, s->n_do)) return 1;
        if (gmb_p_has_stray_errdefer(s->try_body, s->n_try)) return 1;
        if (gmb_p_has_stray_errdefer(s->catch_body, s->n_catch)) return 1;
        if (gmb_p_has_stray_errdefer(s->finally_body, s->n_finally)) return 1;
    }
    return 0;
}
