/* gmb_c99_emit_ai_sb.c — emitter C99 del Second Brain (namespace AI/GRAPH), §0quater.
 * UN handler per il dominio (emit_bf_ai_sb) organizzato in sottosezioni per sotto-dominio.
 * Il parser C99Native consegna il nome NUDO (es. ISVALID) + ns (GRAPH) + tk (Identifier):
 * il dispatch usa bi_is(name, ...) — MAI il lexeme come chiave (regola d'oro: il nome è la
 * chiave canonica risolta dal parser). Ogni builtin emette la chiamata al runtime C condiviso
 * gmb_* (gmb_secondbrain.c, gmb_graph.c, gmb_brain.c, gmb_crypto.c): §0bis, UNA implementazione.
 */
#include "gmb_c99_emit_internal.h"
#include <string.h>

/* ── helper: emissione argomento stringa → `.data` nudo ── */
static void emit_s(ctx_t* c, gmb_expr_t** args, int i) {
    emit_str_operand(c, args[i]);
}

static void emit_call_str(ctx_t* c, const char* fn, gmb_expr_t** args, int from, int count) {
    sb_printf(&c->out, "gmb_string_alloc(%s(", fn);
    for (int i = 0; i < count; i++) {
        if (i) sb_put(&c->out, ", ");
        emit_s(c, args, from + i);
        sb_put(&c->out, ".data");
    }
    sb_put(&c->out, "))");
}

static void emit_call_num(ctx_t* c, const char* fn, gmb_expr_t** args, int from, int count) {
    sb_printf(&c->out, "(gmb_double_t)%s(", fn);
    for (int i = 0; i < count; i++) {
        if (i) sb_put(&c->out, ", ");
        emit_s(c, args, from + i);
        sb_put(&c->out, ".data");
    }
    sb_put(&c->out, ")");
}


/* ── TABELLE DI LOOKUP per sotto-dominio (niente catene if/switch).
 * gb_row: o emissione generica str/num con conteggio, o handler dedicato `sp`. */
typedef int (*gb_sp)(ctx_t*, gmb_expr_t**, int);
typedef struct { gmb_tokenkind_t tk; int is_str; int min_a; int cap; const char* fn; gb_sp sp; } gb_row;
static int gb_run(ctx_t* c, const gb_row* e, gmb_expr_t** args, int nargs) {
    if (e->sp) return e->sp(c, args, nargs);
    if (nargs < e->min_a) return 0;
    int cnt = e->cap ? (nargs > e->cap ? e->cap : nargs) : e->min_a;
    if (e->is_str) emit_call_str(c, e->fn, args, 0, cnt);
    else emit_call_num(c, e->fn, args, 0, cnt);
    return 1;
}
static int gb_lookup(const gb_row* tbl, size_t n, ctx_t* c, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    for (size_t i = 0; i < n; i++)
        if (tbl[i].tk == tk) return gb_run(c, &tbl[i], args, nargs);
    return 0;
}
#define GB_LEN(a) (sizeof(a)/sizeof((a)[0]))

/* ── VAULT (L1) ── */
static int gb_vault_add(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 2) return 0;
    sb_printf(&c->out, "(gmb_double_t)gmb_vault_add_folder_bool(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, "); emit_s(c, args, 1); sb_put(&c->out, ".data, ");
    if (nargs >= 3) sb_printf(&c->out, "strcmp("), emit_s(c, args, 2), sb_put(&c->out, ".data, \"0\") != 0");
    else sb_put(&c->out, "1");
    sb_put(&c->out, ", NULL)");
    return 1;
}
static int gb_vault_setprops(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 3) return 0;
    /* §0bis: una sola implementazione (gmb_vault_folder_set_str nel runtime condiviso). Prima:
     * statement-expression inline + il 4o argomento (tags) IGNORATO. */
    sb_put(&c->out, "(gmb_double_t)gmb_vault_folder_set_str(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    emit_s(c, args, 1); sb_put(&c->out, ".data, ");
    emit_s(c, args, 2); sb_put(&c->out, ".data, ");
    if (nargs >= 4) { emit_s(c, args, 3); sb_put(&c->out, ".data)"); } else sb_put(&c->out, "NULL)");
    return 1;
}
static const gb_row GB_VAULT[] = {
    { GMB_TOK_AiVaultInfo,      1, 1, 1, "gmb_vault_info_json",     NULL },
    { GMB_TOK_AiVaultFolders,   1, 1, 1, "gmb_vault_folders_json",  NULL },
    { GMB_TOK_AiVaultCount,     0, 1, 1, "gmb_vault_count",         NULL },
    { GMB_TOK_AiVaultList,      1, 0, 0, "gmb_vault_list_json",     NULL },
    { GMB_TOK_AiVaultPolicy,    1, 1, 1, "gmb_vault_folders_json",  NULL },
    { GMB_TOK_AiVaultAddFolder, 0, 0, 0, NULL,                      gb_vault_add },
    { GMB_TOK_AiVaultRemoveFolder,0,2,2,"gmb_vault_folder_remove",  NULL },
    { GMB_TOK_AiVaultSetFolderProps,0,0,0,NULL,                     gb_vault_setprops },
};
static int emit_sb_vault(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_VAULT, GB_LEN(GB_VAULT), c, tk, args, nargs);
}

/* ── NOTE (L2) ── */
static const gb_row GB_NOTE[] = {
    { GMB_TOK_NoteGet,       1, 2, 2, "gmb_note_get",        NULL },
    { GMB_TOK_NoteTitle,     1, 2, 2, "gmb_note_title",      NULL },
    { GMB_TOK_NoteTags,      1, 2, 2, "gmb_note_tags",       NULL },
    { GMB_TOK_NoteWordCount, 0, 2, 2, "gmb_note_wordcount",  NULL },
    { GMB_TOK_NoteExists,    0, 2, 2, "gmb_note_exists",     NULL },
    { GMB_TOK_NoteFolder,    1, 2, 2, "gmb_note_folder_path",NULL },
};
static int emit_sb_note(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_NOTE, GB_LEN(GB_NOTE), c, tk, args, nargs);
}

/* ── LINK (L3) ── */
static const gb_row GB_LINK[] = {
    { GMB_TOK_AiNoteLink,      0, 3, 4, "gmb_note_link",    NULL },
    { GMB_TOK_AiNoteUnlink,    0, 3, 3, "gmb_note_unlink",  NULL },
    { GMB_TOK_AiNoteSetAlias,  0, 4, 4, "gmb_note_set_alias",NULL },
    { GMB_TOK_LinksOut,        2, 2, 2, "gmb_note_links_out_json", NULL },
    { GMB_TOK_Backlinks,       2, 2, 2, "gmb_note_backlinks_json", NULL },
    { GMB_TOK_LinkDangling,    0, 1, 1, "gmb_links_dangling_count", NULL },
    { GMB_TOK_LinkJson,        1, 1, 1, "gmb_links_json_graph", NULL },
};
static int emit_sb_link(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_LINK, GB_LEN(GB_LINK), c, tk, args, nargs);
}

/* ── INGEST / SYNC (L6) ── */
/* INGEST FILE/FOLDER: il parser/command NON porta il vault (args[0]=source, args[1]=rel/
 * preserve) — il runtime risolve "" = brain attivo (parità PCode/VM/C §0bis). Emette la
 * chiamata reale gmb_ingest_import_file/folder; converter binario non registrato → errore
 * esplicito a runtime (mai stub). */
static int gb_ingest_file(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 1) return 0;
    sb_printf(&c->out, "(gmb_double_t)(gmb_ingest_import_file(\"\", ");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    if (nargs >= 2) { emit_s(c, args, 1); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_put(&c->out, ", 1, 1) != NULL)");
    return 1;
}
static int gb_ingest_folder(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 1) return 0;
    sb_printf(&c->out, "(gmb_double_t)gmb_ingest_import_folder(\"\", ");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    if (nargs >= 2) { emit_expr(c, args[1]); sb_put(&c->out, " != 0"); } else sb_put(&c->out, "1");
    sb_put(&c->out, ", 1, 1)");
    return 1;
}
/* AiSyncFolder / AiIngestWeb: gli opzionali (direction/conflict, relPath) vanno paddati
 * con NULL come fa l'emitter C# (parità VM/C, che passa NULL per gli args mancanti).
 * La riga generica emetteva solo gli args presenti → `cc: too few arguments`
 * (AiVaultNotesCoverage su C99Native: gmb_sync_folder/2 vs 4, gmb_web_scrape/2 vs 3). */
static int gb_sync_folder(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 2) return 0;
    sb_printf(&c->out, "gmb_string_alloc(gmb_sync_folder(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    emit_s(c, args, 1); sb_put(&c->out, ".data, ");
    if (nargs >= 3) { emit_s(c, args, 2); sb_put(&c->out, ".data, "); } else sb_put(&c->out, "NULL, ");
    if (nargs >= 4) { emit_s(c, args, 3); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_put(&c->out, "))");
    return 1;
}
static int gb_ingest_web(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 2) return 0;
    sb_printf(&c->out, "gmb_string_alloc(gmb_web_scrape(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    emit_s(c, args, 1); sb_put(&c->out, ".data, ");
    if (nargs >= 3) { emit_s(c, args, 2); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_put(&c->out, "))");
    return 1;
}
static const gb_row GB_INGEST[] = {
    { GMB_TOK_AiSyncFolder,   0, 0, 0, NULL, gb_sync_folder },
    { GMB_TOK_AiIngestWeb,    0, 0, 0, NULL, gb_ingest_web },
    { GMB_TOK_IngestFile,   0, 0, 0, NULL, gb_ingest_file },
    { GMB_TOK_IngestFolder, 0, 0, 0, NULL, gb_ingest_folder },
};
static int emit_sb_ingest(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_INGEST, GB_LEN(GB_INGEST), c, tk, args, nargs);
}

/* ── MEMORY (§3) ── */
static int gb_mem_ask(ctx_t* c, gmb_expr_t** args, int nargs) {
    /* AI.MEMORYASK(query[, topK]) → ricerca nel brain (gmb_brain_search, dims=0 = usa le dim
     * memorizzate). Parità emitter C# (TokenKind.MemoryAsk). */
    if (nargs < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(gmb_brain_search(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    if (nargs >= 2) { emit_expr(c, args[1]); } else sb_put(&c->out, "5");
    sb_put(&c->out, ", 0))");
    return 1;
}
static int gb_mem_rank(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(gmb_brain_search(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    if (nargs >= 2) { emit_expr(c, args[1]); } else sb_put(&c->out, "5");
    sb_put(&c->out, ", 384))");
    return 1;
}
static int gb_mem_consolidate(ctx_t* c, gmb_expr_t** args, int nargs) {
    // §0bis: niente stub. Prima emetteva la COSTANTE "1" (finto-risultato); ora si chiama
    // la funzione reale del runtime condiviso (ritorna 1 in caso di successo).
    (void)args; (void)nargs;
    sb_put(&c->out, "(gmb_double_t)gmb_memory_consolidate()");
    return 1;
}
static const gb_row GB_MEMORY[] = {
    { GMB_TOK_MemoryAsk,        0, 0, 0, NULL, gb_mem_ask },
    { GMB_TOK_MemoryRank,       0, 0, 0, NULL, gb_mem_rank },
    { GMB_TOK_MemoryConsolidate,0, 0, 0, NULL, gb_mem_consolidate },
};
static int emit_sb_memory(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_MEMORY, GB_LEN(GB_MEMORY), c, tk, args, nargs);
}

/* ── GRAPH (L5) via gmb_graph_brain_query ── */
static void emit_graph_call(ctx_t* c, const char* op, gmb_expr_t** args, int nargs) {
    sb_printf(&c->out, "gmb_string_alloc(gmb_graph_brain_query(");
    if (nargs >= 1) { emit_s(c, args, 0); sb_put(&c->out, ".data"); } else sb_put(&c->out, "\"\"");
    sb_printf(&c->out, ", \"%s\", ", op);
    if (nargs >= 2) { emit_s(c, args, 1); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_printf(&c->out, ", ");
    if (nargs >= 3) { emit_s(c, args, 2); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_printf(&c->out, ", ");
    if (nargs >= 4) { emit_expr(c, args[3]); } else sb_put(&c->out, "0");
    sb_put(&c->out, "))");
}

/* HASCYCLE/ISVALID: il runtime ritorna "0"/"1" (stringa) ma la superficie è BitType →
 * numero. Statement-expression con free (prima: gmb_print_double(gmb_string_alloc(...))
 * → type mismatch, cc error). */
static void emit_graph_call_num(ctx_t* c, const char* op, gmb_expr_t** args, int nargs) {
    sb_put(&c->out, "({ char* _gn = gmb_graph_brain_query(");
    if (nargs >= 1) { emit_s(c, args, 0); sb_put(&c->out, ".data"); } else sb_put(&c->out, "\"\"");
    sb_printf(&c->out, ", \"%s\", ", op);
    if (nargs >= 2) { emit_s(c, args, 1); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_printf(&c->out, ", ");
    if (nargs >= 3) { emit_s(c, args, 2); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_put(&c->out, ", 0); double _gd = _gn ? atof(_gn) : 0; if (_gn) free(_gn); _gd; })");
}

/* GRAPH: LOOKUP TABLE {tokenkind -> op} — nessuna catena if/switch. * Il parser produce per GRAPH.X il tokenkind QUALIFICATO GMB_TOK_GraphX (1784-1805),
 * NON l'op nuda (GMB_TOK_GraphDiameter ecc.) — fix 2026-09-07: prima la tabella usava le op
 * nude, che il parser non produce mai → nessun GRAPH.* veniva emesso nel C99Native.
 * Solo le op che il runtime C gmb_graph_brain_query supporta (QUERYMULTI/MST/DOT/
 * SUBGRAPH/BYTAG/BYFOLDER sono C#-only e NON vanno emesse nel C). */
typedef struct { gmb_tokenkind_t tk; const char* op; int min_a; } gb_graph_op;
static const gb_graph_op GB_GRAPH_OPS[] = {
    { GMB_TOK_GraphNeighbors,   "NEIGHBORS",  2 },
    { GMB_TOK_GraphPath,        "PATH",       3 },
    { GMB_TOK_GraphStats,       "STATS",      1 },
    { GMB_TOK_GraphIsValid,     "ISVALID",    1 },
    { GMB_TOK_BrainsAdjacency,  "ADJACENCY",  1 },
    { GMB_TOK_GraphDot,         "DOT",        1 },
    { GMB_TOK_GraphByTag,       "BYTAG",      2 },
    { GMB_TOK_GraphByFolder,    "BYFOLDER",   2 },
};
static int emit_sb_graph(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    /* QUERYMULTI(query, brains_csv): firma DIVERSA (gmb_graph_query_multi), non
     * gmb_graph_brain_query(brain, op, ...). */
    if (tk == GMB_TOK_GraphQueryMulti) {
        if (nargs < 1) return 0;
        sb_put(&c->out, "gmb_string_alloc(gmb_graph_query_multi(");
        emit_s(c, args, 0); sb_put(&c->out, ".data, ");
        if (nargs >= 2) { emit_s(c, args, 1); sb_put(&c->out, ".data"); } else sb_put(&c->out, "\"\"");
        sb_put(&c->out, "))");
        return 1;
    }
    if (tk == GMB_TOK_GraphHasCycle || tk == GMB_TOK_GraphIsValid) {
        if (nargs < 1) return 0;
        emit_graph_call_num(c, tk == GMB_TOK_GraphHasCycle ? "HASCYCLE" : "ISVALID", args, nargs);
        return 1;
    }
    if (tk == GMB_TOK_GraphBuild || tk == GMB_TOK_GraphUpdate) {
        if (nargs < 1) return 0;
        /* GRAPH BUILD/UPDATE sono COMANDI (statement): niente cast a (gmb_double_t),
         * che in posizione-statement produce `-Wunused-value` e inquina lo stdout
         * catturato dall'harness (falso DIFF di parity4). Chiamata nuda = nessun warning. */
        sb_put(&c->out, "gmb_graph_build(");
        emit_s(c, args, 0); sb_put(&c->out, ".data, ");
        if (nargs >= 2) { emit_s(c, args, 1); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
        sb_put(&c->out, ")");
        return 1;
    }
    if (tk == GMB_TOK_GraphRdd) {
        if (nargs < 1) return 0;
        sb_put(&c->out, "gmb_graph_rdd_set(");   /* comando: vedi nota sopra (niente cast) */
        emit_s(c, args, 0); sb_put(&c->out, ".data)");
        return 1;
    }
    if (tk == GMB_TOK_GraphFindPath) {
        if (nargs < 2) return 0;
        sb_put(&c->out, "gmb_string_alloc(gmb_graph_brain_query(gmb_graph_active_name(), \"PATH\", ");
        emit_s(c, args, 0); sb_put(&c->out, ".data, ");
        emit_s(c, args, 1); sb_put(&c->out, ".data, 0))");
        return 1;
    }
    for (size_t i = 0; i < sizeof(GB_GRAPH_OPS) / sizeof(GB_GRAPH_OPS[0]); i++) {
        if (GB_GRAPH_OPS[i].tk == tk) {
            if (nargs < GB_GRAPH_OPS[i].min_a) return 0;
            emit_graph_call(c, GB_GRAPH_OPS[i].op, args, nargs);
            return 1;
        }
    }
    return 0;
}

/* ── BRAIN (L6) ── */
static const gb_row GB_BRAIN[] = {
    { GMB_TOK_AiBrainList, 1, 0, 0, "gmb_brain_registry_list", NULL },
    { GMB_TOK_AiBrainUsed, 1, 0, 0, "gmb_brain_used_csv", NULL },   /* unione C ∪ brains.json (sincronizzato) */
    { GMB_TOK_AiBrainInfo, 1, 1, 1, "gmb_brain_info_or_empty", NULL },
};
static int emit_sb_brain(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_BRAIN, GB_LEN(GB_BRAIN), c, tk, args, nargs);
}

/* ── PROVENANCE (§2) ── */
static int gb_prov_sign(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 2) return 0;
    sb_printf(&c->out, "gmb_string_alloc(gmb_provenance_sign((const uint8_t*)");
    emit_s(c, args, 0); sb_printf(&c->out, ".data, (size_t)"); emit_s(c, args, 0); sb_printf(&c->out, ".length, ");
    emit_s(c, args, 1); sb_printf(&c->out, ".data, (size_t)"); emit_s(c, args, 1); sb_printf(&c->out, ".length))");
    return 1;
}
static int gb_prov_verify(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 3) return 0;
    sb_printf(&c->out, "(gmb_double_t)gmb_provenance_verify((const uint8_t*)");
    emit_s(c, args, 0); sb_printf(&c->out, ".data, (size_t)"); emit_s(c, args, 0); sb_printf(&c->out, ".length, ");
    emit_s(c, args, 1); sb_printf(&c->out, ".data, (size_t)"); emit_s(c, args, 1); sb_printf(&c->out, ".length, ");
    emit_s(c, args, 2); sb_printf(&c->out, ".data)");
    return 1;
}
static int gb_prov_encrypt(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 2) return 0;
    /* §0bis: una sola implementazione nel runtime condiviso (prima: statement-expression inline
     * con buffer locali, che la gamba nativa non riusciva nemmeno a compilare). */
    sb_put(&c->out, "gmb_string_alloc(gmb_share_encrypt(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    emit_s(c, args, 1); sb_put(&c->out, ".data))");
    return 1;
}
static int gb_prov_decrypt(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_share_decrypt(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    emit_s(c, args, 1); sb_put(&c->out, ".data))");
    return 1;
}
static const gb_row GB_PROVENANCE[] = {
    { GMB_TOK_SignNote,      0, 0, 0, NULL, gb_prov_sign },
    { GMB_TOK_VerifyNote,    0, 0, 0, NULL, gb_prov_verify },
    { GMB_TOK_ShareEncrypt,  0, 0, 0, NULL, gb_prov_encrypt },
    { GMB_TOK_ShareDecrypt,  0, 0, 0, NULL, gb_prov_decrypt },
};
static int emit_sb_provenance(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_PROVENANCE, GB_LEN(GB_PROVENANCE), c, tk, args, nargs);
}

/* ── MULTI-BRAIN (P1-2b): AI.SEARCHMULTI/RAGCONTEXTMULTI/ASKMULTI.
 * I tre builtin girano sul brain condiviso gmb_brain (brain singolo "vm", come VM/C).
 * Reranker: 0=%RERANK_OFF, 1=%RERANK_KEYWORD (default); minEvidence omessa o negativa = off;
 * explain su SEARCHMULTI -> il runtime risponde con errore esplicito (mai no-op).
 * Fonte unica delle semantiche: gmb_brain_search_ex/gmb_ai_sbm_json/gmb_ai_sbm_ctx. */
/* TRUE/FALSE arrivano dal parser C99Native come pseudo-call `_true_()`/`_false_()`:
 * per un argomento numerico serve la costante BASIC (vero = -1). */
static void gb_emit_num(ctx_t* c, gmb_expr_t* a, const char* dflt) {
    if (!a) { sb_put(&c->out, dflt); return; }
    if (a->var && strcasecmp(a->var, "_true_") == 0) { sb_put(&c->out, "-1"); return; }
    if (a->var && strcasecmp(a->var, "_false_") == 0) { sb_put(&c->out, "0"); return; }
    emit_expr(c, a);
}
static int gb_multi_search(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 1) return 0;
    /* SEARCHMULTI multi-brain (parità MultiBrainSearch C#): passa collection (args[2]) e
     * brains (args[3]) a gmb_ai_sbm_json_multi; NULL/nil/mancante → NULL (usa i collegati). */
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_sbm_json_multi(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    if (nargs >= 2) { sb_put(&c->out, "(int)("); gb_emit_num(c, args[1], "5"); sb_put(&c->out, ")"); } else sb_put(&c->out, "5");
    sb_put(&c->out, ", ");
    /* collection args[2], default "wiki" */
    if (nargs >= 3 && args[2] && !(args[2]->kind == GMB_EXPR_VAR && args[2]->var && strcasecmp(args[2]->var, "NULL") == 0))
    { emit_s(c, args, 2); sb_put(&c->out, ".data"); } else sb_put(&c->out, "\"wiki\"");
    sb_put(&c->out, ", ");
    /* brains args[3], default NULL */
    if (nargs >= 4 && args[3] && !(args[3]->kind == GMB_EXPR_VAR && args[3]->var && strcasecmp(args[3]->var, "NULL") == 0))
    { emit_s(c, args, 3); sb_put(&c->out, ".data"); } else sb_put(&c->out, "NULL");
    sb_put(&c->out, ", ");
    if (nargs >= 5) { sb_put(&c->out, "(int)("); gb_emit_num(c, args[4], "1"); sb_put(&c->out, ")"); } else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    if (nargs >= 6) { gb_emit_num(c, args[5], "-1.0"); } else sb_put(&c->out, "-1.0");
    sb_put(&c->out, ", ");
    if (nargs >= 7) { sb_put(&c->out, "(int)("); gb_emit_num(c, args[6], "0"); sb_put(&c->out, ")"); } else sb_put(&c->out, "0");
    sb_put(&c->out, "))");
    return 1;
}
static int gb_multi_ctx(ctx_t* c, gmb_expr_t** args, int nargs, int rk_idx, int ev_idx) {
    sb_put(&c->out, "gmb_string_alloc(gmb_ai_sbm_ctx(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    if (nargs >= 2) { sb_put(&c->out, "(int)("); gb_emit_num(c, args[1], "5"); sb_put(&c->out, ")"); } else sb_put(&c->out, "5");
    sb_put(&c->out, ", 384, ");
    if (nargs > rk_idx) { sb_put(&c->out, "(int)("); gb_emit_num(c, args[rk_idx], "1"); sb_put(&c->out, ")"); } else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    if (nargs > ev_idx) { gb_emit_num(c, args[ev_idx], "-1.0"); } else sb_put(&c->out, "-1.0");
    sb_put(&c->out, "))");
    return 1;
}
static int gb_multi_rag(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 1) return 0;
    return gb_multi_ctx(c, args, nargs, 2, 3);
}
static int gb_multi_ask(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 1) return 0;
    /* contesto reale o errore/insufficiente JSON; solo se contesto reale chiama l'LLM. */
    sb_put(&c->out, "({ char* _c = gmb_ai_sbm_ctx(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    if (nargs >= 2) { sb_put(&c->out, "(int)("); emit_expr(c, args[1]); sb_put(&c->out, ")"); } else sb_put(&c->out, "5");
    sb_put(&c->out, ", 384, ");
    if (nargs > 2) { sb_put(&c->out, "(int)("); emit_expr(c, args[2]); sb_put(&c->out, ")"); } else sb_put(&c->out, "1");
    sb_put(&c->out, ", ");
    if (nargs > 3) { emit_expr(c, args[3]); } else sb_put(&c->out, "-1.0");
    /* "[insufficient evidence]" è già la RISPOSTA canonica (nessun documento rilevante:
     * parità RagAskPipeline del lato managed, che non chiama l'LLM). Prima si costruiva il
     * prompt e si chiamava il completa → senza provider usciva "Error: nessun provider LLM
     * configurato (AI SET ACTIVE)" invece del messaggio deterministico (ai/ai_askmulti). */
    sb_put(&c->out, "); char* _r; if (_c && (strncmp(_c, \"{\\\"status\\\":\", 10) == 0 || strncmp(_c, \"[insufficient evidence]\", 23) == 0)) { _r = _c; } else { char _p[8192]; snprintf(_p, sizeof(_p), \"Domanda: %s\\nContesto:\\n%s\", ");
    emit_s(c, args, 0); sb_put(&c->out, ".data, _c ? _c : \"\"); if (_c) free(_c); _r = gmb_llm_complete(\"Rispondi alla domanda usando SOLO il contesto fornito.\", _p); } gmb_string_t _s = gmb_string_alloc(_r); if (_r) free(_r); _s; })");
    return 1;
}
static const gb_row GB_MULTI[] = {
    { GMB_TOK_AiSearchMulti,    1, 0, 0, NULL, gb_multi_search },
    { GMB_TOK_AiRagContextMulti,1, 0, 0, NULL, gb_multi_rag },
    { GMB_TOK_AiAskMulti,       1, 0, 0, NULL, gb_multi_ask },
};
static int emit_sb_multi(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_MULTI, GB_LEN(GB_MULTI), c, tk, args, nargs);
}

/* ── ENRICH/QA/BrainVault (§2) via gmb_llm_complete ── */
static int gb_enr_tags(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 1) return 0;
    sb_printf(&c->out, "gmb_string_alloc(gmb_llm_complete(\"Suggerisci tag\", ");
    emit_s(c, args, 0); sb_put(&c->out, ".data))");
    return 1;
}
static int gb_ask_chain(ctx_t* c, gmb_expr_t** args, int nargs) {
    if (nargs < 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_ask_chain(");
    emit_s(c, args, 0); sb_put(&c->out, ".data, ");
    emit_s(c, args, 1); sb_put(&c->out, ".data, ");
    if (nargs >= 3) { emit_s(c, args, 2); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "\"chain\"");
    sb_put(&c->out, "))");
    return 1;
}
/* AI.QAANSWER: handler dedicato — il conteggio generico (is_str, cap 2) perdeva il terzo
 * argomento (window) e non poteva esprimere la logica dell'oracolo: si delega al runtime
 * condiviso gmb_qa_answer (che implementa l'ordine dei controlli di Enricher.QaAnswer). */
static int sb_qa_answer(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 2) return 0;
    sb_put(&c->out, "gmb_string_alloc(gmb_qa_answer(");
    emit_s(c, a, 0); sb_put(&c->out, ".data, ");
    emit_s(c, a, 1); sb_put(&c->out, ".data, ");
    if (n >= 3) { sb_put(&c->out, "(int)("); emit_expr(c, a[2]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "3");
    sb_put(&c->out, "))");
    return 1;
}
static int gb_brainvault_query(ctx_t* c, gmb_expr_t** args, int nargs) {
    (void)args; (void)nargs;
    /* AI.BRAINVAULTQUERY richiede un provider embedding attivo (parità BrainVault.Query C#):
     * errore ESPLICITO, mai silenzioso. L'IL/PCode sollevano l'eccezione e l'harness stampa
     * "ERROR: <msg>"; qui si emette lo stesso testo (il raise C terminerebbe il processo,
     * divergendo dall'exit 0 delle altre gambe). Prima era mappato su gmb_llm_complete (vuoto). */
    sb_put(&c->out, "gmb_string_alloc(\"ERROR: Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)\")");
    return 1;
}
static const gb_row GB_ENRICH[] = {
    { GMB_TOK_Enrich,          1, 2, 2, "gmb_llm_complete", NULL },
    { GMB_TOK_SuggestTags,     0, 0, 0, NULL, gb_enr_tags },
    { GMB_TOK_QaAnswer,        0, 2, 3, NULL, sb_qa_answer },
    { GMB_TOK_BrainVaultQuery, 0, 1, 2, NULL, gb_brainvault_query },
    { GMB_TOK_BrainVaultWrite, 1, 1, 2, "gmb_llm_complete", NULL },
    { GMB_TOK_AiAskChain,      0, 0, 0, NULL, gb_ask_chain },
};
static int emit_sb_enrich(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_ENRICH, GB_LEN(GB_ENRICH), c, tk, args, nargs);
}

/* ── KNOWLEDGE GRAPH (AI.KGQUERY / KGADDNODE / KGADDEDGE) ─────────────────────────────
 * §0bis: nessuna logica qui, si chiamano le funzioni del runtime condiviso
 * (gmb_kg_query / gmb_kg_add_node / gmb_kg_add_edge). Prima non esisteva ALCUNA emissione per
 * questa famiglia: l'emitter cadeva su "simbolo non trovato: AI.KGQUERY non definita". */
static const gb_row GB_KG[] = {
    { GMB_TOK_AiKgQuery,   1, 1, 1, "gmb_kg_query",    NULL },   /* char* -> stringa */
    { GMB_TOK_AiKgAddNode, 0, 1, 1, "gmb_kg_add_node", NULL },   /* int   -> numero  */
    { GMB_TOK_AiKgAddEdge, 0, 2, 2, "gmb_kg_add_edge", NULL },
};
static int emit_sb_kg(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_KG, GB_LEN(GB_KG), c, tk, args, nargs);
}

/* ── AGENT (LIST/PAUSE/RESUME/STOP/SEND/RECEIVE/QUEUE*) ───────────────────────────────
 * §0bis: solo chiamate al runtime condiviso (gmb_agent_*). Prima non esisteva ALCUNA
 * emissione per questa famiglia: l'emitter cadeva su "simbolo non trovato". */
/* AI.AGENTLIST: `gmb_agent_list_str()` ritorna GIÀ un gmb_string_t (non un char*): non va
 * avvolto in gmb_string_alloc (che vuole un char*) — emissione diretta. */
static int sb_agent_list(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    /* AI.AGENTLIST() → string[] (C#). gmb_agent_list_names() ritorna la gmb_list_t di
     * stringhe (parità emitter C#). Prima emetteva gmb_agent_list_str() (stringa CSV) →
     * `DIM names() = AI.AGENTLIST()` diventava una stringa scalare e UBOUND(names)/names(i)
     * fallivano (AgentOperatingCenter). */
    sb_put(&c->out, "gmb_agent_list_names()");
    return 1;
}
/* AI.AGENTSEND(to, task, input [, payment]) → runtime gmb_agent_queue_send(to, "user", task,
 * input, payment). Il runtime `gmb_agent_send` vuole `from` esplicito (4 arg) e il BASIC no:
 * si usa la forma queue_* come il IL (From="user"). Prima il generico emetteva 3
 * arg → "too few arguments to function gmb_agent_send". */
static int sb_agent_send(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 3) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_agent_queue_send(");
    emit_s(c, a, 0); sb_put(&c->out, ".data, gmb_string_alloc(\"user\").data, ");
    emit_s(c, a, 1); sb_put(&c->out, ".data, ");
    emit_s(c, a, 2); sb_put(&c->out, ".data, ");
    if (n >= 4) { sb_put(&c->out, "(gmb_double_t)("); emit_expr(c, a[3]); sb_put(&c->out, ")"); }
    else sb_put(&c->out, "0");
    sb_put(&c->out, ")");
    return 1;
}

/* AI AGENT PAUSE/RESUME/STOP/QUEUE CLEAR: booleano canonico -1/0 (parità
 * IlRuntime.AiAgent*), il runtime C ritorna 1/0 → conversione nel ternario. */
static int sb_agent_bool_emit(ctx_t* c, gmb_expr_t** args, int nargs, const char* fn) {
    sb_printf(&c->out, "((gmb_double_t)%s(", fn);
    if (nargs >= 1) { emit_s(c, args, 0); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\").data");
    sb_put(&c->out, ") ? -1.0 : 0.0)");
    return 1;
}
static int sb_agent_pause(ctx_t* c, gmb_expr_t** a, int n) { return sb_agent_bool_emit(c, a, n, "gmb_agent_pause"); }
static int sb_agent_resume(ctx_t* c, gmb_expr_t** a, int n) { return sb_agent_bool_emit(c, a, n, "gmb_agent_resume"); }
static int sb_agent_stop(ctx_t* c, gmb_expr_t** a, int n) { return sb_agent_bool_emit(c, a, n, "gmb_agent_stop"); }
static int sb_agent_qclear(ctx_t* c, gmb_expr_t** a, int n) { return sb_agent_bool_emit(c, a, n, "gmb_agent_queue_clear"); }

static const gb_row GB_AGENT[] = {
    { GMB_TOK_AiAgentList,	0, 0, 0, NULL, sb_agent_list },         /* stringa (gmb_string_t) */
    { GMB_TOK_AiAgentPause,	0, 1, 1, NULL, sb_agent_pause }, /* numero -1/0 (canonico) */
    { GMB_TOK_AiAgentResume,	0, 1, 1, NULL, sb_agent_resume }, /* numero -1/0 */
    { GMB_TOK_AiAgentStop,	0, 1, 1, NULL, sb_agent_stop }, /* numero -1/0 */
    { GMB_TOK_AiAgentSend,	0, 3, 4, NULL, sb_agent_send }, /* numero: (to, task, input [, payment]) */
    { GMB_TOK_AiAgentReceive,	1, 1, 1, "gmb_agent_receive", NULL }, /* stringa */
    { GMB_TOK_AiAgentQueueClear,	0, 1, 1, NULL, sb_agent_qclear }, /* numero -1/0 */
    { GMB_TOK_AiAgentQueueStats,	1, 1, 1, "gmb_agent_queue_stats", NULL }, /* stringa */
};

static int emit_sb_agent(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_AGENT, GB_LEN(GB_AGENT), c, tk, args, nargs);
}

/* ── BRAIN / SKILLS / VAULT-WATCH / HF (righe semplici) ───────────────────────────────
 * §0bis: solo chiamate al runtime condiviso. Prima non esisteva alcuna emissione. */
static int sb_brainvaultbuild(ctx_t* c, gmb_expr_t** a, int n) {
    /* Oracolo C# (IlRuntime.BrainVaultBuild): dir vuota → -1; senza provider embedding
     * attivo → eccezione esplicita. Il runtime C torna -2 in quel caso: qui si solleva
     * l'errore col testo esatto dell'oracolo (mai stub silenzioso, §0bis). */
    if (n < 1) return 0;
    sb_put(&c->out, "({ double _bvb = gmb_brainvaultbuild(");
    emit_s(c, a, 0); sb_put(&c->out, ".data); ");
    sb_put(&c->out, "_bvb == -2 ? (gmb_raise_error(0, gmb_brainvaultbuild_error(), 0L), -1.0) : _bvb; })");
    return 1;
}
/* AI.CHANNELSUBSCRIBE/UNSUBSCRIBE come FUNZIONE: parity managed (no-op). In BASIC il
 * "handler" è un nome agente e il routing a callback non è esposto — comportamento identico
 * a IL/PCode (`AiChannelSubscribe => 1.0`), VM/C (crea il canale, ritorna 1).
 * Prima il FUNCTION form dava "simbolo non trovato" sul C99Native. */
static int sb_channel_sub(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "1.0"); return 1; }
    sb_put(&c->out, "(gmb_bus_channel_create(");
    emit_s(c, a, 0); sb_put(&c->out, ".data), 1.0)");
    return 1;
}
/* AI.AGENTRUNGRAPH/RAG: firma canonica (name, prompt [, rounds]) come la dichiarazione
 * StdLib; il runtime condiviso ritorna l'errore esplicito quando il knowledge graph non è
 * disponibile su questo host (agente non registrato → testo oracolo). */
static int sb_agent_run_graph(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    /* Chiamata DIRETTA al loop C del grafo (gmb_agent_loop.c): su C99Native il motore è C e non
     * esiste alcun backend managed da preferire. `uses_agent_loop` emette l'include di
     * gmb_agent_loop.h nel C generato. */
    c->uses_agent_loop = 1;
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_run_graph(");
    emit_s(c, a, 0); sb_put(&c->out, ".data, ");
    if (n >= 2) { sb_put(&c->out, "("); emit_s(c, a, 1); sb_put(&c->out, ").data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 3) { sb_put(&c->out, "(int)("); gb_emit_num(c, a[2], "5"); sb_put(&c->out, ")"); } else sb_put(&c->out, "5");
    sb_put(&c->out, "))");
    return 1;
}
/* §4.10: AI.AGENTRUNCODE name, prompt [, rounds] — loop agentico sul grafo del CODICE (tool
 * code.*). Il loop reale (runtime agenti C) e la query semantica C sono agganciati al runtime
 * condiviso via seam. Emette il wrapper uniforme `gmb_agent_run_code_json(name, prompt, rounds)`. */
static int sb_agent_run_code(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) { sb_put(&c->out, "gmb_string_alloc(\"\")"); return 1; }
    /* Loop C diretto (tool code.*): il motore principale è C, non c'è un backend managed da
     * preferire su questa gamba. */
    c->uses_agent_loop = 1;
    sb_put(&c->out, "gmb_string_alloc(gmb_agent_run_code(");
    emit_s(c, a, 0); sb_put(&c->out, ".data, ");
    if (n >= 2) { sb_put(&c->out, "("); emit_s(c, a, 1); sb_put(&c->out, ").data, "); } else sb_put(&c->out, "\"\", ");
    if (n >= 3) { sb_put(&c->out, "(int)("); gb_emit_num(c, a[2], "5"); sb_put(&c->out, ")"); } else sb_put(&c->out, "5");
    sb_put(&c->out, "))");
    return 1;
}
/* Gap DICHIARATO (feature con runtime/engine non portato su C99 headless): errore ESPLICITO
 * (§0bis, mai "simbolo non trovato" né stub silenzioso). Il gate il runner
 * C99Native riconoscono il testo "non disponibile nel backend C99 headless" come GAP (SKIP),
 * non come regressione. Vale per AGENTRUNGRAPH/RAG e gli stats engine (DECODE/PREFILL/BENCH/CACHE). */
static int sb_gap_notavail(ctx_t* c, gmb_expr_t** a, int n) {
    (void)a; (void)n;
    sb_put(&c->out, "(gmb_raise_error(5, \"builtin non disponibile nel backend C99 headless (runtime/engine non portato)\", 0L), gmb_string_alloc(\"\"))");
    return 1;
}
/* AI.BRAININSTALL(nome[, sorgente]) — sorgente assente = installazione dal manifest. */
static int sb_brain_install(ctx_t* c, gmb_expr_t** a, int n) {
    if (n < 1) return 0;
    sb_put(&c->out, "(gmb_double_t)gmb_brain_install(");
    emit_s(c, a, 0); sb_put(&c->out, ".data, ");
    if (n >= 2) { emit_s(c, a, 1); sb_put(&c->out, ".data)"); }
    else sb_put(&c->out, "\"\")");
    return 1;
}
static const gb_row GB_AIEXTRA[] = {
    { GMB_TOK_AiHfRepo,       1, 0, 0, "gmb_ai_hf_repo",      NULL },  /* char* -> stringa */
    { GMB_TOK_AiSkillsPrompt, 1, 0, 0, "gmb_ai_skills_prompt",NULL },  /* char* -> stringa */
    /* Forma SCALARE `PRINT AI.SKILLSLIST()/AI.SKILLSPATHS()`: il runtime formatta `[a, b]`
       (parità oracolo IL). Prima esisteva solo il percorso array-assignment (DIM x() AS STRING). */
    { GMB_TOK_AiSkillsList,   1, 0, 0, "gmb_ai_skills_list_str", NULL },
    { GMB_TOK_AiSkillsPaths,  1, 0, 0, "gmb_ai_skills_paths_str",NULL },
    { GMB_TOK_AiBrainUse,     0, 1, 1, "gmb_brain_use",       NULL },
    { GMB_TOK_AiBrainDetach,  0, 1, 1, "gmb_brain_detach",    NULL },
    /* Il cervello è un DATO: install da manifest/URL e rimozione vivono nel runtime condiviso
     * (gmb_brains.c) → VM/C e C99Native fanno la stessa cosa di IL/PCodeVM, non un errore. */
    { GMB_TOK_AiBrainInstall,   0, 1, 2, NULL,                 sb_brain_install },
    { GMB_TOK_AiBrainUninstall, 0, 1, 1, "gmb_brain_uninstall", NULL },
    { GMB_TOK_AiVaultWatch,   0, 1, 2, "gmb_vault_watch",     NULL },
    { GMB_TOK_BrainVaultBuild,0, 0, 0, NULL, sb_brainvaultbuild },
    { GMB_TOK_AiChannelCreate,      0, 1, 1, "gmb_bus_channel_create", NULL },
    { GMB_TOK_AiChannelPublish,     0, 2, 2, "gmb_bus_channel_publish", NULL },
    { GMB_TOK_AiChannelSubscribe,   0, 1, 2, NULL, sb_channel_sub },
    { GMB_TOK_AiChannelUnsubscribe, 0, 1, 2, NULL, sb_channel_sub },
    { GMB_TOK_AgentRunGraph,        1, 2, 3, NULL, sb_agent_run_graph },
    { GMB_TOK_AgentRunRag,          1, 2, 3, NULL, sb_agent_run_graph },
    { GMB_TOK_AgentRunCode,         1, 3, 3, NULL, sb_agent_run_code },
    { GMB_TOK_AiDecodeStats,        1, 1, 1, "gmb_agent_decode_stats_json", NULL },
    { GMB_TOK_AiPrefillStats,       1, 1, 1, "gmb_agent_prefill_stats_json", NULL },
    { GMB_TOK_AiBenchResult,        1, 1, 1, "gmb_agent_benchresult_json", NULL },
    { GMB_TOK_AiCacheStats,         1, 1, 1, NULL, sb_gap_notavail },
};
static int emit_sb_aiextra(ctx_t* c, const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs) {
    return gb_lookup(GB_AIEXTRA, GB_LEN(GB_AIEXTRA), c, tk, args, nargs);
}

typedef int (*sb_sub_fn)(ctx_t*, const char*, gmb_tokenkind_t, gmb_expr_t**, int);
static const sb_sub_fn SB_SUBS[] = {
    emit_sb_vault, emit_sb_note, emit_sb_link, emit_sb_ingest,
    emit_sb_memory, emit_sb_provenance, emit_sb_graph, emit_sb_brain, emit_sb_enrich,
    emit_sb_multi, emit_sb_kg, emit_sb_agent, emit_sb_aiextra,
};
int emit_bf_ai_sb(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)ns;
    for (size_t i = 0; i < sizeof(SB_SUBS) / sizeof(SB_SUBS[0]); i++)
        if (SB_SUBS[i](c, name, tk, args, nargs)) return 1;
    return 0;
}
