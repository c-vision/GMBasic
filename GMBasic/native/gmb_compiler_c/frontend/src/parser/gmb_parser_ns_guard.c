/* gmb_parser_ns_guard.c — GATE dei NAMESPACE: una funzione di LIBRERIA si chiama NUDA solo se
 * il suo namespace è IMPORTato (o è fra i default del linguaggio).
 *
 * PERCHÉ. Il linker C# (`KnownLibraryNotImported`, Linker.Rewrites.cs) rifiuta un nome nudo di
 * una funzione di libreria il cui namespace non è caricato: "Simbolo non trovato: 'ANGLE2D' è
 * una funzione di libreria in un namespace non importato (VECTOR). Usa la forma qualificata
 * <NS>.X(...) o aggiungi IMPORT <NS>." Il frontend C risolveva invece qualunque nome della
 * tabella `gmb_fn_lookup` senza guardare i namespace: `ANGLE2D(...)` senza `IMPORT VECTOR`
 * compilava su C99Native/VM-C e falliva su IL/PCode — divergenza di RIGORE fra le gambe.
 *
 * REGOLA (identica al C#): il nome nudo è lecito se ALMENO UNO dei namespace che possiedono
 * quel TokenKind in `gmb_ns_lookup` (tabella GENERATA dal registry) è caricato, dove
 * "caricato" = fra i DEFAULT del linguaggio o IMPORTato nel programma. `NS.FUNZ(*...)` (forma
 * qualificata) non passa da qui: il parser la risolve con (ns, nome) e non serve l'import.
 *
 * NB: gli IMPORT sono raccolti con un PRE-SCAN dei token (`gmb_p_collect_imports`, chiamata da
 * gmb_parse) perché una direttiva IMPORT può stare dopo il punto d'uso nello stesso programma.
 */
#include "gmb_parser_internal.h"
#include "gmb_ns_lookup.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Namespace SEMPRE caricati — parità esatta con SourcePipeline.DefaultNamespacesStandard (C#)
 * e con la sezione `defaultNamespaces` di system.settings.json. */
static const char* g_ns_default[] = {
    "SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME",
    "CONVERSIONS", "IO", "CONCURRENCY", "ARRAYS", NULL
};

static int ns_is_loaded(const gmb_parser_t* p, const char* ns)
{
    for (int i = 0; g_ns_default[i]; i++)
        if (strcasecmp(g_ns_default[i], ns) == 0) return 1;
    for (int i = 0; i < p->n_imports; i++)
        if (strcasecmp(p->imports[i], ns) == 0) return 1;
    return 0;
}

/* Contesto del MODULO: un sub-parse di frammento costante (`EVAL("<letterale>")`) punta al
 * padre; IMPORT, nomi dichiarati, procedure utente e flag "snippet" sono quelli del root. */
static const gmb_parser_t* ns_root(const gmb_parser_t* p)
{
    while (p && p->parent) p = p->parent;
    return p;
}

static void ns_add_import(gmb_parser_t* p, char* name, int len)
{
    if (len <= 0) return;
    if (len > 95) len = 95;
    name[len] = 0;
    for (int i = 0; i < p->n_imports; i++)
        if (strcasecmp(p->imports[i], name) == 0) return;      /* già presente */
    if (p->n_imports >= 64) return;
    for (int i = 0; i < len; i++) p->imports[p->n_imports][i] = (char)toupper((unsigned char)name[i]);
    p->imports[p->n_imports][len] = 0;
    p->n_imports++;
}

/* Namespace NON implicito: senza IMPORT non è raggiungibile, né come comando né come lettura
 * qualificata. Oggi l'unico dominio così è WORLD (motore 3D, parità con le gambe gestite, dove
 * `_program.Namespaces` fa lo stesso lavoro in ParseStatement).
 *
 * Unica sede del messaggio e della regola: "caricato" è la STESSA definizione del gate dei nomi
 * nudi (`ns_is_loaded`: default del linguaggio + `p->imports`, riempito dal pre-scan), non una
 * seconda tabella. Gli IMPORT stanno nel root del modulo (un frammento EVAL punta al padre). */
bool gmb_p_require_import(gmb_parser_t* p, const char* ns, int line)
{
    if (!p || !ns) return true;   /* senza parser o senza nome non c'è nulla da verificare */
    const gmb_parser_t* root = ns_root(p);
    if (ns_is_loaded(root, ns)) return true;
    char msg[160];
    snprintf(msg, sizeof msg, "namespace non importato: %s (aggiungi IMPORT %s).", ns, ns);
    gmb_p_set_err(p, msg, line);
    return false;
}

void gmb_p_collect_imports(gmb_parser_t* p)
{
    p->n_imports = 0;
    p->n_user_fns = 0;
    p->n_decl_names = 0;
    p->snippet = 0;
    /* FRAMMENTO EVAL/& (wrapper `SUB|FUNCTION __GMB_EVAL__`): non è un programma, non ha
     * IMPORT → gate sospeso. Riconoscerlo QUI evita di passare un flag da tutti i chiamanti
     * di gmb_parse (snippet, classify, LET-collect in gmb_bytecode/gmb_compiler.c). */
    for (int i = 0; i + 1 < p->n; i++) {
        /* SUB e FUNCTION sono lo stesso token nel lexer C (GMB_KEYWORD_ENTRY("SUB", Function)). */
        if (p->toks[i].kind != GMB_TOK_Function) continue;
        const gmb_token_t* sn = &p->toks[i + 1];
        if (sn->length == 12 && sn->lexeme && strncasecmp(sn->lexeme, "__GMB_EVAL__", 12) == 0) {
            p->snippet = 1;
            return;
        }
    }
    /* Procedure UTENTI dichiarate: `SUB|FUNCTION <nome> [ ( ... ) ]` (anche metodi di classe).
     * Shadowing: `FUNCTION Norm(...)` rende lecito `Norm(x)` senza IMPORT LINALG. */
    for (int i = 0; i + 1 < p->n; i++) {
        if (p->toks[i].kind != GMB_TOK_Function) continue;
        const gmb_token_t* fn = &p->toks[i + 1];
        if (!fn->lexeme || fn->length <= 0 || fn->length >= 64) continue;
        if (fn->kind == GMB_TOK_LeftParen || fn->kind == GMB_TOK_Newline
            || fn->kind == GMB_TOK_Eof) continue;              /* FUNCTION(...) = lambda anonima */
        if (p->n_user_fns >= 512) break;
        int dup = 0;
        for (int k = 0; k < p->n_user_fns; k++)
            if (p->toks[i].length && strlen(p->user_fns[k]) == (size_t)fn->length
                && strncasecmp(p->user_fns[k], fn->lexeme, (size_t)fn->length) == 0) { dup = 1; break; }
        if (dup) continue;
        memcpy(p->user_fns[p->n_user_fns], fn->lexeme, (size_t)fn->length);
        p->user_fns[p->n_user_fns][fn->length] = 0;
        p->n_user_fns++;
        /* PARAMETRI della procedura (primo token di ogni voce, fino alla ')' di chiusura):
         * sono nomi dichiarati, quindi un `param(i)` non è la funzione di libreria omonima. */
        int j = i + 2;
        if (j < p->n && p->toks[j].kind == GMB_TOK_LeftParen) {
            int depth = 1, expect = 1;
            for (j++; j < p->n && depth > 0; j++) {
                const gmb_token_t* q = &p->toks[j];
                if (q->kind == GMB_TOK_Eof) break;
                if (q->kind == GMB_TOK_LeftParen) { depth++; continue; }
                if (q->kind == GMB_TOK_RightParen) { depth--; continue; }
                if (depth != 1) continue;
                if (q->kind == GMB_TOK_Comma) { expect = 1; continue; }
                if (!expect) continue;
                if (!q->lexeme || q->length <= 0 || q->length >= 64) continue;
                char q0 = q->lexeme[0];
                if (!(q0 == '_' || (q0 >= 'A' && q0 <= 'Z') || (q0 >= 'a' && q0 <= 'z'))) continue;
                if (p->n_decl_names < 1024) {
                    memcpy(p->decl_names[p->n_decl_names], q->lexeme, (size_t)q->length);
                    p->decl_names[p->n_decl_names][q->length] = 0;
                    p->n_decl_names++;
                }
                expect = 0;
            }
        }
    }
    /* PARAMETRI di una LAMBDA ANONIMA: `FN(a, b) => …` / `SUB(a, b) { … }` (il token
     * Function/Sub seguito SUBITO da '('). Sono nomi DICHIARATI nello scope della lambda:
     * senza, `z(0)` nel corpo veniva scambiato per la funzione di libreria SCENE.Z e il gate
     * dei namespace faceva fallire `ffi/ffi_callback_vv_return` ("Simbolo non trovato: 'z'…").
     * Stessa raccolta della lista parametri di una procedura nominata, qui per la forma
     * anonima (incluse le voci con parentesi di array: `z() AS DOUBLE`). */
    for (int i = 0; i + 1 < p->n; i++) {
        if (p->toks[i].kind != GMB_TOK_Function) continue;
        if (p->toks[i + 1].kind != GMB_TOK_LeftParen) continue;
        int depth = 0, expect = 1;
        for (int j = i + 1; j < p->n; j++) {
            const gmb_token_t* q = &p->toks[j];
            if (q->kind == GMB_TOK_Eof) break;
            if (q->kind == GMB_TOK_LeftParen) { depth++; continue; }
            if (q->kind == GMB_TOK_RightParen) { depth--; if (depth <= 0) break; continue; }
            if (depth != 1) continue;
            if (q->kind == GMB_TOK_Comma) { expect = 1; continue; }
            if (!expect) continue;
            if (!q->lexeme || q->length <= 0 || q->length >= 64) continue;
            char q0 = q->lexeme[0];
            if (!(q0 == '_' || (q0 >= 'A' && q0 <= 'Z') || (q0 >= 'a' && q0 <= 'z'))) continue;
            if (p->n_decl_names < 1024) {
                memcpy(p->decl_names[p->n_decl_names], q->lexeme, (size_t)q->length);
                p->decl_names[p->n_decl_names][q->length] = 0;
                p->n_decl_names++;
            }
            expect = 0;
        }
    }
    /* PARAMETRI della forma a GRAFFE `{ |a, b| … }`: i nomi stanno fra i due `|`. */
    for (int i = 0; i < p->n; i++) {
        gmb_tokenkind_t bk = p->toks[i].kind;
        int is_bar = (bk == GMB_TOK_Bar)
                  || (bk == GMB_TOK_Unknown && p->toks[i].lexeme && p->toks[i].length == 1
                      && p->toks[i].lexeme[0] == '|');
        if (!is_bar) continue;
        int expect = 1;
        for (int j = i + 1; j < p->n; j++) {
            const gmb_token_t* t = &p->toks[j];
            if (t->kind == GMB_TOK_Eof || t->kind == GMB_TOK_Newline) break;
            int t_bar = (t->kind == GMB_TOK_Bar)
                     || (t->kind == GMB_TOK_Unknown && t->lexeme && t->length == 1
                         && t->lexeme[0] == '|');
            if (t_bar) break;
            if (t->kind == GMB_TOK_Comma) { expect = 1; continue; }
            if (!expect) continue;
            if (!t->lexeme || t->length <= 0 || t->length >= 64) continue;
            char c0 = t->lexeme[0];
            if (!(c0 == '_' || (c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z'))) continue;
            if (p->n_decl_names < 1024) {
                memcpy(p->decl_names[p->n_decl_names], t->lexeme, (size_t)t->length);
                p->decl_names[p->n_decl_names][t->length] = 0;
                p->n_decl_names++;
            }
            expect = 0;
        }
    }
    /* Nomi DICHIARATI da DIM/REDIM/GLOBAL/LOCAL/STATIC (anche MULTIPLI: `DIM a(2), b(2)` e
     * `DIM g(7) AS T, info(0) AS LONG`). Il parser registra in p->declared solo la PRIMA di un
     * DIM multiplo e la tabella ha cap 512: qui si raccolgono tutti, così un accesso a elemento
     * (`info(0)`, `mask(0)`, `files$(i)`) non viene scambiato per la funzione omonima. */
    for (int i = 0; i < p->n; i++) {
        gmb_tokenkind_t k = p->toks[i].kind;
        if (k != GMB_TOK_Dim && k != GMB_TOK_Redim && k != GMB_TOK_Global
            && k != GMB_TOK_Local && k != GMB_TOK_Static) continue;
        int depth = 0, expect = 1;
        for (int j = i + 1; j < p->n; j++) {
            const gmb_token_t* t = &p->toks[j];
            if (t->kind == GMB_TOK_Eof) break;
            if (t->kind == GMB_TOK_Newline && depth == 0) break;
            if (t->kind == GMB_TOK_Colon && depth == 0) break;
            if (t->kind == GMB_TOK_LeftParen) { depth++; continue; }
            if (t->kind == GMB_TOK_RightParen) { if (depth > 0) depth--; continue; }
            if (depth > 0) continue;
            if (t->kind == GMB_TOK_Comma) { expect = 1; continue; }
            if (!expect) continue;
            if (!t->lexeme || t->length <= 0 || t->length >= 64) continue;
            char c0 = t->lexeme[0];
            if (!(c0 == '_' || (c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z'))) continue;
            if (t->kind == GMB_TOK_As) continue;
            if (p->n_decl_names < 1024) {
                memcpy(p->decl_names[p->n_decl_names], t->lexeme, (size_t)t->length);
                p->decl_names[p->n_decl_names][t->length] = 0;
                p->n_decl_names++;
            }
            expect = 0;
        }
    }
    for (int i = 0; i < p->n; i++) {
        if (p->toks[i].kind != GMB_TOK_Import) continue;        char cur[96];
        int cl = 0;
        int depth = 0;     /* dentro la lista selettiva `(a, b)`: ignorata */
        int skip = 0;      /* dopo AS/WITH ALIAS: si ignora fino alla virgola */
        for (int j = i + 1; j < p->n; j++) {
            const gmb_token_t* t = &p->toks[j];
            if (t->kind == GMB_TOK_Eof) break;
            if (t->kind == GMB_TOK_Newline && depth == 0) break;
            if (t->kind == GMB_TOK_Colon && depth == 0) break;
            if (t->kind == GMB_TOK_LeftParen) { depth++; continue; }
            if (t->kind == GMB_TOK_RightParen) { if (depth > 0) depth--; continue; }
            if (depth > 0) continue;
            if (t->kind == GMB_TOK_Comma) { ns_add_import(p, cur, cl); cl = 0; skip = 0; continue; }
            if (t->kind == GMB_TOK_As || t->kind == GMB_TOK_With || t->kind == GMB_TOK_Alias) {
                ns_add_import(p, cur, cl); cl = 0; skip = 1; continue;
            }
            if (skip) continue;
            if (t->kind == GMB_TOK_Dot) { if (cl > 0 && cl < 95) cur[cl++] = '.'; continue; }
            /* `*` di IMPORT A.* non è una keyword del lexer (token Unknown, lexeme "*"). */
            if (t->length == 1 && t->lexeme && t->lexeme[0] == '*') continue;
            if (t->lexeme && t->length > 0) {
                int k = t->length;
                if (cl + k > 95) k = 95 - cl;
                memcpy(cur + cl, t->lexeme, (size_t)k);
                cl += k;
            }
        }
        ns_add_import(p, cur, cl);
    }
}

int gmb_p_check_bare_builtin(gmb_parser_t* p, gmb_tokenkind_t tk, const char* name, int line)
{
    if (tk == GMB_TOK_Unknown || !name || !*name) return 1;
    const gmb_parser_t* root = ns_root(p);
    if (!root) return 1;
    const gmb_parser_t* mod = root;   /* IMPORT/dichiarazioni/snippet del MODULO */
    if (mod->snippet) return 1;      /* frammento EVAL/&: nessun IMPORT possibile (v. sopra) */

    /* Nome che è una KEYWORD del linguaggio (FETCH, LIST, ADD, DETERMINANT…): la sua
     * semantica è del compilatore (espressione/statement), non della procedura di libreria
     * omonima — il C# le risolve come core e non applica la regola dei namespace
     * (verificato: `FETCH("…")` e `DETERMINANT(A)` compilano su IL senza IMPORT).
     * `ANGLE2D`/`DIST2D`, che sono SOLO procedure di libreria, NON sono keyword → restano
     * soggette al gate. */
    if (gmb_keyword_tok(name) != GMB_TOK_Identifier
        && gmb_keyword_tok(name) != 0) return 1;

    /* Procedura UTENTE omonima: shadowing (il C# risolve prima i simboli utente). */
    {
        char bare[64];
        size_t bl = strlen(name);
        if (bl >= sizeof bare) bl = sizeof bare - 1;
        memcpy(bare, name, bl); bare[bl] = 0;
        if (bl > 1 && (bare[bl - 1] == '$' || bare[bl - 1] == '%' || bare[bl - 1] == '!'
                       || bare[bl - 1] == '#')) bare[bl - 1] = 0;
        for (int k = 0; k < p->n_user_fns; k++)
            if (strcasecmp(mod->user_fns[k], bare) == 0) return 1;
        /* Nome DICHIARATO (DIM/param, tutte le forme): accesso a elemento, non una call. */
        for (int k = 0; k < mod->n_decl_names; k++) {
            const char* dn = mod->decl_names[k];
            size_t dl = strlen(dn);
            if (dl > 1 && (dn[dl - 1] == '$' || dn[dl - 1] == '%' || dn[dl - 1] == '!'
                           || dn[dl - 1] == '#')) dl--;
            if (dl == strlen(bare) && strncasecmp(dn, bare, dl) == 0) return 1;
        }
        /* VARIABILE/ARRAY dichiarato (DIM/parametro): `X(i)` è un accesso a elemento, non la
         * funzione di libreria omonima (MOUSE.X/WINDOWS.X…). Il C# arriva qui solo se il nome
         * NON è un simbolo; qui basta la tabella dei nomi dichiarati dal parser. */
        if (gmb_p_is_declared(mod, bare)) return 1;
    }

    /* Suffisso tipo BASIC ($ % ! #): solo il tipo di ritorno, non fa parte del nome. */
    char nm[96];
    size_t l = strlen(name);
    if (l >= sizeof nm) l = sizeof nm - 1;
    memcpy(nm, name, l); nm[l] = 0;
    if (l > 1 && (nm[l - 1] == '$' || nm[l - 1] == '%' || nm[l - 1] == '!'
                  || nm[l - 1] == '#')) nm[l - 1] = 0;

    /* Namespace che REGISTRANO questo nome nella tabella generata dal registry. Si confronta
     * il NOME (non il TokenKind): un TokenKind può essere CONDIVISO da un builtin core e da un
     * namespace di libreria — es. `EOF(1)` è il core (token EofFunc, namespace IO/DBF/ISAM/RDD)
     * mentre la riga LEDGER ha un token diverso; su `EOF` il C# guarda il NOME e vede anche IO
     * (default) → lecito. La chiave di verità resta il registry, mai il lessema a runtime. */
    char owners[8][64];
    int n_owners = 0;
    for (int i = 0; i < GMB_NS_LOOKUP_COUNT && n_owners < 8; i++) {
        const gmb_ns_lookup_row_t* r = &gmb_ns_lookup[i];
        /* Righe che NON sono funzioni: membri/proprietà (tk Identifier) e voci non tipizzate
         * non rientrano nella regola (`WINDOWCLOSED()` è un builtin core per il C#). */
        if (r->tk == GMB_TOK_Identifier || r->tk == GMB_TOK_Unknown) continue;
        char rn[96];
        snprintf(rn, sizeof rn, "%s", r->name ? r->name : "");
        size_t rl = strlen(rn);
        if (rl > 1 && rn[rl - 1] == '$') rn[rl - 1] = 0;
        if (strcasecmp(rn, nm) != 0) continue;
        int dup = 0;
        for (int k = 0; k < n_owners; k++)
            if (strcasecmp(owners[k], r->ns) == 0) { dup = 1; break; }
        if (!dup) snprintf(owners[n_owners++], sizeof owners[0], "%s", r->ns);
    }
    if (n_owners == 0) return 1;                 /* non è una funzione di libreria registrata */
    for (int k = 0; k < n_owners; k++)
        if (ns_is_loaded(mod, owners[k])) return 1;

    char list[256];
    int used = 0;
    list[0] = 0;
    for (int k = 0; k < n_owners; k++)
        used += snprintf(list + used, sizeof list - (size_t)used, "%s%s", k ? ", " : "", owners[k]);
    char msg[640];
    snprintf(msg, sizeof msg,
             "Simbolo non trovato: '%s' è una funzione di libreria in un namespace non importato (%s). "
             "Usa la forma qualificata <NS>.%s(...) o aggiungi IMPORT <NS>.",
             nm, list, nm);
    gmb_p_set_err(p, msg, line);
    return 0;
}
