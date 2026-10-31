/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_compression.c — speculare a
 * Parser.Stmt.Compression.cs: ZIP src TO dest, UNZIP arch TO cartella. */
#include "gmb_parser_internal.h"
#include <stddef.h>

gmb_stmt_t* gmb_p_parse_compression(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    int kind = 0;
    if (t->kind == GMB_TOK_Zip) { kind = GMB_STMT_ZIP; }
    else if (t->kind == GMB_TOK_Unzip) { kind = GMB_STMT_UNZIP; }
    else return NULL;
    gmb_p_adv(p);
    gmb_stmt_t* s = gmb_stmt_new(kind, line);
    s->channel = gmb_p_parse_expr(p);         /* sorgente/archivio */
    if (!gmb_p_match(p, GMB_TOK_To)) { gmb_p_set_err(p, "Atteso TO in ZIP/UNZIP", line); return s; }
    s->send_value = gmb_p_parse_expr(p);      /* destinazione */
    return s;
}
