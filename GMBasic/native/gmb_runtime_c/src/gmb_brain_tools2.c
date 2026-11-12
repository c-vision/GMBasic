/* strcasestr/strdup con le estensioni POSIX/Darwin */
#define _GNU_SOURCE 1
#define _DARWIN_C_SOURCE 1
/* gmb_brain_tools2.c — Second Brain: grafo (modello/export), DEDUPE/EXTRACT/MERGE/CONVERT,
 * VERIFY --golden, export Obsidian. Tutto in C: nessun toolchain nel mondo C#.
 */
#include "gmb_brain_tools.h"
#include "gmb_brain_tools_internal.h"

#include "gmb_runtime.h"
#include "gmb_graph.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "gmb_compat_win.h"   /* strcasestr (solo su Windows) */
#include <unistd.h>

/* ══════════════════════════════════════════════════════════════════════════
 * Copia RAG (dedupe/extract/merge/convert): chunk+embedding+FTS, MAI re-embedding
 * ══════════════════════════════════════════════════════════════════════════ */

static int rag_schema(const char* db)
{
    /* apre e crea lo schema canonico via l'API del runtime (fonte unica) */
    int64_t h = gmb_rag_open(db);
    if (h < 0) return 0;
    gmb_rag_ensure_collection(db, "wiki", 0, "cosine", NULL);
    gmb_rag_close(h);
    return 1;
}

static long rag_first_collection(sqlite3* db)
{
    return br_sql_scalar(db, "SELECT COALESCE(MIN(id), 0) FROM vec_collections");
}

/* Copia in `dst` i documenti di `src` (filtro LIKE opzionale). `merge` = dedup per
 * source_id; `replace` = l'ultimo vince. Ritorna 1 e i contatori. */
static int copy_rag_docs(const char* src_path, const char* dst_path, const char* source_filter,
                         int merge, int replace, long* out_docs, long* out_chunks, char* err, size_t errcap)
{
    *out_docs = 0;
    *out_chunks = 0;
    {   /* la cartella di destinazione può non esistere (prima apertura) */
        char dir[2400];
        snprintf(dir, sizeof dir, "%s", dst_path);
        char* sl = strrchr(dir, '/');
        if (sl) { *sl = 0; gmb_fs_mkdir_p(dir); }
    }
    if (!rag_schema(dst_path)) { snprintf(err, errcap, "schema del db di destinazione non creabile"); return 0; }
    sqlite3* db = br_sql_open(dst_path, 0);
    if (!db) { snprintf(err, errcap, "db di destinazione non apribile"); return 0; }
    char att[2600];
    snprintf(att, sizeof att, "ATTACH DATABASE '%s' AS src", src_path);
    if (!br_sql_exec(db, att)) { snprintf(err, errcap, "attach del sorgente fallito"); sqlite3_close(db); return 0; }

    long coll = rag_first_collection(db);
    br_sql_exec(db,
        "INSERT OR IGNORE INTO vec_collections(id,name,dimensions,metric,model_version,model_digest,format,created_at) "
        "SELECT id,name,dimensions,metric,model_version,model_digest,COALESCE(format,'fp32'),created_at FROM src.vec_collections;");
    coll = rag_first_collection(db);

    /* elenco documenti del sorgente */
    sqlite3_stmt* st = NULL;
    char q[512];
    if (source_filter && *source_filter)
        snprintf(q, sizeof q, "SELECT id, source_id FROM src.vec_documents WHERE source_id LIKE ?1 ORDER BY id");
    else
        snprintf(q, sizeof q, "SELECT id, source_id FROM src.vec_documents ORDER BY id");
    if (sqlite3_prepare_v2(db, q, -1, &st, NULL) != SQLITE_OK) {
        snprintf(err, errcap, "lettura documenti fallita");
        sqlite3_close(db);
        return 0;
    }
    if (source_filter && *source_filter) {
        char pat[1200];
        snprintf(pat, sizeof pat, "%%%s%%", source_filter);
        sqlite3_bind_text(st, 1, pat, -1, SQLITE_TRANSIENT);
    }
    long ndocs = 0, nchunks = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        long docid = (long)sqlite3_column_int64(st, 0);
        const char* sid = (const char*)sqlite3_column_text(st, 1);
        char source[2048];
        snprintf(source, sizeof source, "%s", sid ? sid : "");
        if (merge) {
            sqlite3_stmt* ex = NULL;
            if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM vec_documents WHERE collection_id=?1 AND source_id=?2", -1, &ex, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(ex, 1, coll);
                sqlite3_bind_text(ex, 2, source, -1, SQLITE_TRANSIENT);
                long have = 0;
                if (sqlite3_step(ex) == SQLITE_ROW) have = (long)sqlite3_column_int64(ex, 0);
                sqlite3_finalize(ex);
                if (have > 0) {
                    if (!replace) continue;
                    sqlite3_stmt* del = NULL;
                    if (sqlite3_prepare_v2(db,
                        "DELETE FROM vec_embeddings WHERE chunk_id IN (SELECT c.id FROM vec_chunks c JOIN vec_documents d ON d.id=c.document_id WHERE d.collection_id=?1 AND d.source_id=?2);", -1, &del, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(del, 1, coll);
                        sqlite3_bind_text(del, 2, source, -1, SQLITE_TRANSIENT);
                        sqlite3_step(del);
                        sqlite3_finalize(del);
                    }
                    if (sqlite3_prepare_v2(db, "DELETE FROM fts_chunks WHERE chunk_id IN (SELECT c.id FROM vec_chunks c JOIN vec_documents d ON d.id=c.document_id WHERE d.collection_id=?1 AND d.source_id=?2);", -1, &del, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(del, 1, coll);
                        sqlite3_bind_text(del, 2, source, -1, SQLITE_TRANSIENT);
                        sqlite3_step(del);
                        sqlite3_finalize(del);
                    }
                    if (sqlite3_prepare_v2(db, "DELETE FROM vec_chunks WHERE document_id IN (SELECT id FROM vec_documents WHERE collection_id=?1 AND source_id=?2);", -1, &del, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(del, 1, coll);
                        sqlite3_bind_text(del, 2, source, -1, SQLITE_TRANSIENT);
                        sqlite3_step(del);
                        sqlite3_finalize(del);
                    }
                    if (sqlite3_prepare_v2(db, "DELETE FROM vec_documents WHERE collection_id=?1 AND source_id=?2;", -1, &del, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(del, 1, coll);
                        sqlite3_bind_text(del, 2, source, -1, SQLITE_TRANSIENT);
                        sqlite3_step(del);
                        sqlite3_finalize(del);
                    }
                }
            }
        }
        /* copia il documento */
        sqlite3_stmt* ins = NULL;
        if (sqlite3_prepare_v2(db,
            "INSERT INTO vec_documents(collection_id,source_id,source_hash,status,tenant_id,access_roles,metadata_json,created_at,updated_at) "
            "SELECT ?1,source_id,source_hash,status,tenant_id,access_roles,metadata_json,created_at,updated_at FROM src.vec_documents WHERE id=?2",
            -1, &ins, NULL) != SQLITE_OK) { snprintf(err, errcap, "insert documento fallito"); break; }
        sqlite3_bind_int64(ins, 1, coll);
        sqlite3_bind_int64(ins, 2, docid);
        int rc = sqlite3_step(ins);
        sqlite3_finalize(ins);
        if (rc != SQLITE_DONE) continue;
        long newdoc = (long)sqlite3_last_insert_rowid(db);
        ndocs++;

        /* chunk + embedding + FTS, uno per uno (id renumbering) */
        sqlite3_stmt* ck = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id FROM src.vec_chunks WHERE document_id=?1 ORDER BY chunk_index", -1, &ck, NULL) != SQLITE_OK) continue;
        sqlite3_bind_int64(ck, 1, docid);
        while (sqlite3_step(ck) == SQLITE_ROW) {
            long oldchunk = (long)sqlite3_column_int64(ck, 0);
            sqlite3_stmt* ci = NULL;
            if (sqlite3_prepare_v2(db,
                "INSERT INTO vec_chunks(document_id,chunk_index,text,token_count,metadata_json) "
                "SELECT ?1,chunk_index,text,token_count,metadata_json FROM src.vec_chunks WHERE id=?2",
                -1, &ci, NULL) != SQLITE_OK) continue;
            sqlite3_bind_int64(ci, 1, newdoc);
            sqlite3_bind_int64(ci, 2, oldchunk);
            rc = sqlite3_step(ci);
            sqlite3_finalize(ci);
            if (rc != SQLITE_DONE) continue;
            long newchunk = (long)sqlite3_last_insert_rowid(db);
            nchunks++;
            sqlite3_stmt* ei = NULL;
            if (sqlite3_prepare_v2(db,
                "INSERT OR REPLACE INTO vec_embeddings(chunk_id,model_version,embedding_created_at,dimensions,embedding) "
                "SELECT ?1,model_version,embedding_created_at,dimensions,embedding FROM src.vec_embeddings WHERE chunk_id=?2",
                -1, &ei, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(ei, 1, newchunk);
                sqlite3_bind_int64(ei, 2, oldchunk);
                sqlite3_step(ei);
                sqlite3_finalize(ei);
            }
            sqlite3_stmt* fi = NULL;
            if (sqlite3_prepare_v2(db,
                "INSERT INTO fts_chunks(text,chunk_id,collection_id) SELECT text,?1,?2 FROM src.vec_chunks WHERE id=?3",
                -1, &fi, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(fi, 1, newchunk);
                sqlite3_bind_int64(fi, 2, coll);
                sqlite3_bind_int64(fi, 3, oldchunk);
                sqlite3_step(fi);
                sqlite3_finalize(fi);
            }
        }
        sqlite3_finalize(ck);
    }
    sqlite3_finalize(st);
    br_sql_exec(db, "DETACH DATABASE src");
    sqlite3_close(db);
    *out_docs = ndocs;
    *out_chunks = nchunks;
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 * DEDUPE
 * ══════════════════════════════════════════════════════════════════════════ */

gmb_brain_result_t gmb_brain_tool_dedupe(const char* name, int apply, int min_chars, int dry_run)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    gmb_brain_rec_t info;
    if (!gmb_brain_reg_info(name, &info)) return br_fail("cervello '%s' non registrato (BRAIN LIST)", name);
    int writes = apply && !dry_run;
    if (min_chars < 1) min_chars = 1;

    sbuf_t out;
    sb_init(&out);
    char p[2048];

    if (gmb_brain_reg_rag_db(name, p, sizeof p) && access(p, R_OK) == 0) {
        sqlite3* db = br_sql_open(p, 0);
        if (!db) return br_fail("db RAG non apribile (%s)", p);
        const char* norm = "lower(trim(replace(replace(text,char(10),' '),char(13),' ')))";
        char q[1024];
        snprintf(q, sizeof q, "SELECT COUNT(*) FROM (SELECT 1 FROM vec_chunks GROUP BY %s HAVING COUNT(*)>1)", norm);
        long groups = br_sql_scalar(db, q);
        long dup = 0;
        snprintf(q, sizeof q, "SELECT COALESCE(SUM(c-1),0) FROM (SELECT COUNT(*) c FROM vec_chunks GROUP BY %s HAVING COUNT(*)>1)", norm);
        dup = br_sql_scalar(db, q);
        char crumbs_q[256];
        snprintf(crumbs_q, sizeof crumbs_q, "SELECT COUNT(*) FROM vec_chunks WHERE length(trim(text)) < %d", min_chars);
        long crumbs = br_sql_scalar(db, crumbs_q);
        char t[512];
        snprintf(t, sizeof t, "RAG: %ld gruppi di chunk duplicati → %ld rimovibili; %ld chunk-crumbs (< %d char)\n",
                 groups, dup, crumbs, min_chars);
        sb_put(&out, t);

        if (writes && (dup > 0 || crumbs > 0)) {
            /* embeddings e FTS dei chunk da eliminare, poi i chunk */
            snprintf(q, sizeof q,
                "DELETE FROM vec_embeddings WHERE chunk_id IN (SELECT id FROM vec_chunks WHERE id NOT IN "
                "(SELECT MIN(id) FROM vec_chunks GROUP BY %s) OR length(trim(text)) < %d);", norm, min_chars);
            br_sql_exec(db, q);
            snprintf(q, sizeof q,
                "DELETE FROM fts_chunks WHERE chunk_id IN (SELECT id FROM vec_chunks WHERE id NOT IN "
                "(SELECT MIN(id) FROM vec_chunks GROUP BY %s) OR length(trim(text)) < %d);", norm, min_chars);
            br_sql_exec(db, q);
            snprintf(q, sizeof q,
                "DELETE FROM vec_chunks WHERE id NOT IN (SELECT MIN(id) FROM vec_chunks GROUP BY %s) OR length(trim(text)) < %d;",
                norm, min_chars);
            br_sql_exec(db, q);
            snprintf(t, sizeof t, "RAG: rimossi %ld chunk (con embedding e righe FTS)\n", dup + crumbs);
            sb_put(&out, t);
        }
        sqlite3_close(db);
    }

    char gdir[1024], gp[2400];
    gmb_paths_graphs_dir(gdir, sizeof gdir);
    if (gmb_brain_reg_graph_json(name, gp, sizeof gp) && access(gp, R_OK) == 0) {
        wgraph_t g;
        if (wg_load(gp, &g)) {
            int dup_edges = 0, self_loops = 0;
            for (int i = 0; i < g.ne; i++) {
                if (!strcmp(g.edges[i].from, g.edges[i].to)) { self_loops++; continue; }
                for (int j = 0; j < i; j++)
                    if (!strcmp(g.edges[j].from, g.edges[i].from) && !strcmp(g.edges[j].to, g.edges[i].to) &&
                        !strcmp(g.edges[j].type, g.edges[i].type)) { dup_edges++; break; }
            }
            char t[256];
            snprintf(t, sizeof t, "GRAPH: %d edge duplicati, %d self-loop\n", dup_edges, self_loops);
            sb_put(&out, t);
            if (writes && (dup_edges > 0 || self_loops > 0)) {
                int removed = 0, k = 0;
                for (int i = 0; i < g.ne; i++) {
                    int drop = 0;
                    if (!strcmp(g.edges[i].from, g.edges[i].to)) drop = 1;
                    for (int j = 0; j < k && !drop; j++)
                        if (!strcmp(g.edges[j].from, g.edges[i].from) && !strcmp(g.edges[j].to, g.edges[i].to) &&
                            !strcmp(g.edges[j].type, g.edges[i].type)) drop = 1;
                    if (drop) { free(g.edges[i].from); free(g.edges[i].to); free(g.edges[i].type); free(g.edges[i].raw); removed++; continue; }
                    g.edges[k++] = g.edges[i];
                }
                g.ne = k;
                char* js = wg_to_json(&g);
                if (js) { br_write_file(gp, js); free(js); }
                br_graph_write_artifacts(name, gdir);
                snprintf(t, sizeof t, "GRAPH: rimossi %d edge (export ricalcolati)\n", removed);
                sb_put(&out, t);
            }
            wg_free(&g);
        }
    }
    if (!out.p || out.n == 0) { free(out.p); return br_fail("cervello '%s': nessun artefatto (né rag né graph)", name); }
    if (!apply) sb_put(&out, "(report: usa --apply per rimuovere)");
    char* text = (char*)malloc(strlen(out.p) + 96);
    if (!text) { free(out.p); return br_fail("memoria esaurita"); }
    sprintf(text, "BRAIN DEDUPE — %s:\n%s", name, out.p);
    free(out.p);
    gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = text;
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * EXTRACT
 * ══════════════════════════════════════════════════════════════════════════ */

gmb_brain_result_t gmb_brain_tool_extract(const char* name, const char* out_name, const char* source_filter,
                                          const char* node_filter, int hops, int dry_run)
{
    if (!name || !*name || !out_name || !*out_name)
        return br_fail("uso: BRAIN EXTRACT <name> --out <nuovo> [--source substr] [--node substr] [--hops N]");
    gmb_brain_rec_t info;
    if (!gmb_brain_reg_info(name, &info)) return br_fail("cervello '%s' non registrato", name);
    if ((!source_filter || !*source_filter) && (!node_filter || !*node_filter))
        return br_fail("serve --source <substr> (RAG) e/o --node <substr> (GRAPH)");
    char avail[64] = "";
    if (gmb_brain_reg_info(out_name, &info) || gmb_brain_reg_available(out_name, avail, sizeof avail))
        return br_fail("il nome '%s' è già in uso", out_name);
    if (hops < 0) hops = 0;

    sbuf_t out;
    sb_init(&out);
    char t[512];

    char ragdir[1024], graphs[1024];
    gmb_paths_rag_dir(ragdir, sizeof ragdir);
    gmb_paths_graphs_dir(graphs, sizeof graphs);

    if (source_filter && *source_filter) {
        char src[2048];
        if (!gmb_brain_reg_rag_db(name, src, sizeof src)) return br_fail("cervello '%s': nessun db RAG da estrarre", name);
        char dst[2400];
        snprintf(dst, sizeof dst, "%s/%s.db", ragdir, out_name);
        if (dry_run) {
            sqlite3* db = br_sql_open(src, 1);
            long n = 0;
            if (db) {
                sqlite3_stmt* st = NULL;
                if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM vec_documents WHERE source_id LIKE ?1", -1, &st, NULL) == SQLITE_OK) {
                    char pat[1200];
                    snprintf(pat, sizeof pat, "%%%s%%", source_filter);
                    sqlite3_bind_text(st, 1, pat, -1, SQLITE_TRANSIENT);
                    if (sqlite3_step(st) == SQLITE_ROW) n = (long)sqlite3_column_int64(st, 0);
                    sqlite3_finalize(st);
                }
                sqlite3_close(db);
            }
            snprintf(t, sizeof t, "[dry-run] RAG: %ld documenti con source_id LIKE '%%%s%%' → %s\n", n, source_filter, dst);
            sb_put(&out, t);
        } else {
            long docs = 0, chunks = 0;
            char err[512] = "";
            if (!copy_rag_docs(src, dst, source_filter, 0, 0, &docs, &chunks, err, sizeof err)) {
                free(out.p);
                return br_fail("estrazione RAG fallita: %s", err);
            }
            snprintf(t, sizeof t, "RAG: %ld documenti, %ld chunk (con embedding) → %s\n", docs, chunks, dst);
            sb_put(&out, t);
        }
    }

    if (node_filter && *node_filter) {
        char src[2400];
        if (!gmb_brain_reg_graph_json(name, src, sizeof src)) return br_fail("cervello '%s': nessun grafo da estrarre", name);
        char dst[2400];
        snprintf(dst, sizeof dst, "%s/%s.json", graphs, out_name);
        if (dry_run) {
            snprintf(t, sizeof t, "[dry-run] GRAPH: nodi con id LIKE '%%%s%%' + %d hop → %s\n", node_filter, hops, dst);
            sb_put(&out, t);
        } else {
            wgraph_t g;
            if (!wg_load(src, &g)) { free(out.p); return br_fail("grafo illeggibile (%s)", src); }
            char** keep = (char**)calloc((size_t)g.nn + 1, sizeof(char*));
            int nk = 0;
            for (int i = 0; i < g.nn; i++)
                if (strcasestr(g.nodes[i].id, node_filter)) keep[nk++] = g.nodes[i].id;
            for (int h = 0; h < hops; h++) {
                int before = nk;
                for (int e = 0; e < g.ne; e++) {
                    int has_from = 0, has_to = 0;
                    for (int k = 0; k < nk; k++) {
                        if (!strcmp(keep[k], g.edges[e].from)) has_from = 1;
                        if (!strcmp(keep[k], g.edges[e].to)) has_to = 1;
                    }
                    if (has_from && !has_to) keep[nk++] = g.edges[e].to;
                    else if (has_to && !has_from) keep[nk++] = g.edges[e].from;
                }
                if (nk == before) break;
            }
            if (nk == 0) { free(keep); wg_free(&g); free(out.p); return br_fail("nessun nodo con id contenente '%s'", node_filter); }
            wgraph_t sub;
            memset(&sub, 0, sizeof sub);
            sub.folders_json = strdup(g.folders_json ? g.folders_json : "{}");
            sub.nodes = (wgnode_t*)calloc((size_t)nk, sizeof(wgnode_t));
            for (int i = 0; i < g.nn; i++) {
                int in = 0;
                for (int k = 0; k < nk; k++) if (!strcmp(keep[k], g.nodes[i].id)) { in = 1; break; }
                if (!in) continue;
                wgnode_t* x = &sub.nodes[sub.nn++];
                x->id = strdup(g.nodes[i].id);
                x->title = strdup(g.nodes[i].title ? g.nodes[i].title : g.nodes[i].id);
                x->raw = strdup(g.nodes[i].raw ? g.nodes[i].raw : "{}");
            }
            sub.edges = (wgedge_t*)calloc((size_t)g.ne + 1, sizeof(wgedge_t));
            for (int e = 0; e < g.ne; e++) {
                int has_from = 0, has_to = 0;
                for (int k = 0; k < nk; k++) {
                    if (!strcmp(keep[k], g.edges[e].from)) has_from = 1;
                    if (!strcmp(keep[k], g.edges[e].to)) has_to = 1;
                }
                if (!has_from || !has_to) continue;
                wgedge_t* x = &sub.edges[sub.ne++];
                x->from = strdup(g.edges[e].from);
                x->to = strdup(g.edges[e].to);
                x->type = strdup(g.edges[e].type ? g.edges[e].type : "link");
                x->raw = strdup(g.edges[e].raw ? g.edges[e].raw : "{}");
            }
            free(keep);
            wg_free(&g);
            gmb_fs_mkdir_p(graphs);
            char* js = wg_to_json(&sub);
            int ok = js && br_write_file(dst, js);
            free(js);
            int nn = sub.nn, ne = sub.ne;
            wg_free(&sub);
            if (!ok) { free(out.p); return br_fail("scrittura del sottografo fallita (%s)", dst); }
            br_graph_write_artifacts(out_name, graphs);
            snprintf(t, sizeof t, "GRAPH: %d nodi, %d edge (%d hop) → %s\n", nn, ne, hops, dst);
            sb_put(&out, t);
        }
    }
    if (!dry_run) {
        const char* mode = (source_filter && *source_filter) && (node_filter && *node_filter) ? "both"
                         : (source_filter && *source_filter) ? "rag" : "graph";
        gmb_brain_reg_register(out_name, info.source[0] ? info.source : NULL, NULL, mode, NULL, NULL, 0, NULL);
        snprintf(t, sizeof t, "registrato: %s (mode %s)\n", out_name, mode);
        sb_put(&out, t);
    }
    char* text = (char*)malloc(strlen(out.p ? out.p : "") + strlen(name) + strlen(out_name) + 96);
    if (!text) { free(out.p); return br_fail("memoria esaurita"); }
    sprintf(text, "BRAIN EXTRACT %s → %s:\n%s", name, out_name, out.p ? out.p : "");
    free(out.p);
    gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = text;
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * MERGE
 * ══════════════════════════════════════════════════════════════════════════ */

gmb_brain_result_t gmb_brain_tool_merge(const char* const* names, int n, const char* out_name,
                                        const char* on_conflict, int dry_run)
{
    if (n < 2) return br_fail("uso: BRAIN MERGE <a> <b> [<c>…] --out <nuovo>");
    if (!out_name || !*out_name) return br_fail("serve --out <nuovo nome>");
    char avail[64] = "";
    gmb_brain_rec_t tmp;
    if (gmb_brain_reg_info(out_name, &tmp) || gmb_brain_reg_available(out_name, avail, sizeof avail))
        return br_fail("il nome '%s' è già in uso", out_name);

    const char* dbs[64];
    const char* graphs[64];
    int ndbs = 0, ngraphs = 0;
    for (int i = 0; i < n; i++) {
        gmb_brain_rec_t ri;
        if (!gmb_brain_reg_info(names[i], &ri)) return br_fail("cervello '%s' non registrato", names[i]);
        char p[2048];
        if (ndbs < 64 && gmb_brain_reg_rag_db(names[i], p, sizeof p)) { dbs[ndbs++] = strdup(p); }
        if (ngraphs < 64 && gmb_brain_reg_graph_json(names[i], p, sizeof p)) { graphs[ngraphs++] = strdup(p); }
    }
    if (ndbs == 0 && ngraphs == 0) return br_fail("nessuno dei cervelli indicati ha artefatti");

    /* precondizione: stesse dimensioni embedding */
    if (ndbs > 1) {
        long dims0 = 0;
        for (int i = 0; i < ndbs; i++) {
            sqlite3* db = br_sql_open(dbs[i], 1);
            if (!db) continue;
            long d = br_sql_scalar(db, "SELECT COALESCE(MAX(dimensions),0) FROM vec_collections");
            sqlite3_close(db);
            if (d <= 0) continue;
            if (dims0 == 0) dims0 = d;
            else if (d != dims0) {
                for (int k = 0; k < ndbs; k++) free((void*)dbs[k]);
                for (int k = 0; k < ngraphs; k++) free((void*)graphs[k]);
                return br_fail("dimensioni embedding diverse fra i cervelli (%ld vs %ld): merge non sicuro", dims0, d);
            }
        }
    }

    sbuf_t out;
    sb_init(&out);
    char t[512];
    char ragdir[1024], gdir[1024];
    gmb_paths_rag_dir(ragdir, sizeof ragdir);
    gmb_paths_graphs_dir(gdir, sizeof gdir);

    char dst_db[2400] = "";
    if (ndbs > 0) {
        snprintf(dst_db, sizeof dst_db, "%s/%s.db", ragdir, out_name);
        if (dry_run) {
            snprintf(t, sizeof t, "[dry-run] RAG: unione di %d db (on-conflict %s) → %s\n", ndbs, on_conflict, dst_db);
            sb_put(&out, t);
        } else {
            unlink(dst_db);
            int replace = on_conflict && !strcasecmp(on_conflict, "replace");
            long docs = 0, chunks = 0;
            char err[512] = "";
            for (int i = 0; i < ndbs; i++) {
                long d = 0, c = 0;
                if (!copy_rag_docs(dbs[i], dst_db, NULL, 1, replace, &d, &c, err, sizeof err)) {
                    for (int k = 0; k < ndbs; k++) free((void*)dbs[k]);
                    for (int k = 0; k < ngraphs; k++) free((void*)graphs[k]);
                    free(out.p);
                    return br_fail("merge RAG fallito: %s", err);
                }
                docs += d;
                chunks += c;
            }
            snprintf(t, sizeof t, "RAG: %ld documenti, %ld chunk → %s\n", docs, chunks, dst_db);
            sb_put(&out, t);
        }
    }
    if (ngraphs > 0) {
        char dst_json[2400];
        snprintf(dst_json, sizeof dst_json, "%s/%s.json", gdir, out_name);
        if (dry_run) {
            snprintf(t, sizeof t, "[dry-run] GRAPH: unione di %d grafi → %s\n", ngraphs, dst_json);
            sb_put(&out, t);
        } else {
            wgraph_t u;
            memset(&u, 0, sizeof u);
            u.folders_json = strdup("{}");
            for (int i = 0; i < ngraphs; i++) {
                wgraph_t g;
                if (!wg_load(graphs[i], &g)) continue;
                for (int k = 0; k < g.nn; k++) {
                    int have = 0;
                    for (int j = 0; j < u.nn; j++) if (!strcmp(u.nodes[j].id, g.nodes[k].id)) { have = 1; break; }
                    if (have) continue;
                    u.nodes = (wgnode_t*)realloc(u.nodes, (size_t)(u.nn + 1) * sizeof(wgnode_t));
                    u.nodes[u.nn].id = strdup(g.nodes[k].id);
                    u.nodes[u.nn].title = strdup(g.nodes[k].title ? g.nodes[k].title : g.nodes[k].id);
                    u.nodes[u.nn].raw = strdup(g.nodes[k].raw ? g.nodes[k].raw : "{}");
                    u.nn++;
                }
                for (int k = 0; k < g.ne; k++) {
                    int dup = 0;
                    for (int j = 0; j < u.ne; j++)
                        if (!strcmp(u.edges[j].from, g.edges[k].from) && !strcmp(u.edges[j].to, g.edges[k].to) &&
                            !strcmp(u.edges[j].type, g.edges[k].type)) { dup = 1; break; }
                    if (dup) continue;
                    u.edges = (wgedge_t*)realloc(u.edges, (size_t)(u.ne + 1) * sizeof(wgedge_t));
                    u.edges[u.ne].from = strdup(g.edges[k].from);
                    u.edges[u.ne].to = strdup(g.edges[k].to);
                    u.edges[u.ne].type = strdup(g.edges[k].type ? g.edges[k].type : "link");
                    u.edges[u.ne].raw = strdup(g.edges[k].raw ? g.edges[k].raw : "{}");
                    u.ne++;
                }
                for (int k = 0; k < g.nd; k++) {
                    int dup = 0;
                    for (int j = 0; j < u.nd; j++) if (!strcmp(u.dangling[j], g.dangling[k])) { dup = 1; break; }
                    if (dup) continue;
                    u.dangling = (char**)realloc(u.dangling, (size_t)(u.nd + 1) * sizeof(char*));
                    u.dangling[u.nd++] = strdup(g.dangling[k]);
                }
                wg_free(&g);
            }
            gmb_fs_mkdir_p(gdir);
            char* js = wg_to_json(&u);
            int ok = js && br_write_file(dst_json, js);
            free(js);
            int nn = u.nn, ne = u.ne;
            wg_free(&u);
            if (!ok) {
                for (int k = 0; k < ndbs; k++) free((void*)dbs[k]);
                for (int k = 0; k < ngraphs; k++) free((void*)graphs[k]);
                free(out.p);
                return br_fail("scrittura del grafo unito fallita (%s)", dst_json);
            }
            br_graph_write_artifacts(out_name, gdir);
            snprintf(t, sizeof t, "GRAPH: %d nodi, %d edge (unione di %d grafi) → %s\n", nn, ne, ngraphs, dst_json);
            sb_put(&out, t);
        }
    }
    for (int k = 0; k < ndbs; k++) free((void*)dbs[k]);
    for (int k = 0; k < ngraphs; k++) free((void*)graphs[k]);

    if (!dry_run) {
        const char* mode = ndbs > 0 && ngraphs > 0 ? "both" : ndbs > 0 ? "rag" : "graph";
        gmb_brain_reg_register(out_name, NULL, NULL, mode, NULL, NULL, 0, NULL);
        snprintf(t, sizeof t, "registrato: %s (mode %s)\n", out_name, mode);
        sb_put(&out, t);
    }
    char* text = (char*)malloc(strlen(out.p ? out.p : "") + strlen(out_name) + 256);
    if (!text) { free(out.p); return br_fail("memoria esaurita"); }
    sprintf(text, "BRAIN MERGE [%d cervelli] → %s:\n%s", n, out_name, out.p ? out.p : "");
    free(out.p);
    gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = text;
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * CONVERT (rag ↔ graph)
 * ══════════════════════════════════════════════════════════════════════════ */

static void normalize_source(const char* in, char* out, size_t cap)
{
    snprintf(out, cap, "%s", in ? in : "");
    for (char* p = out; *p; p++) if (*p == '\\') *p = '/';
    /* "…/AiFinanceWiki/01/x.md" → "AiFinanceWiki/01/x.md" */
    const char* ai = strstr(out, "/Ai");
    if (out[0] == '/' && ai) memmove(out, ai + 1, strlen(ai + 1) + 1);
    while (out[0] == '/') memmove(out, out + 1, strlen(out));
}

gmb_brain_result_t gmb_brain_tool_convert(const char* name, const char* to, const char* out_name,
                                          int in_place, int dry_run, int min_edges)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    if (!to || (strcasecmp(to, "rag") && strcasecmp(to, "graph"))) return br_fail("--to attesi: rag|graph");
    gmb_brain_rec_t info;
    if (!gmb_brain_reg_info(name, &info)) return br_fail("cervello '%s' non registrato (BRAIN LIST)", name);
    char target[256];
    snprintf(target, sizeof target, "%s", (out_name && *out_name) ? out_name : name);
    if (!in_place && (!out_name || !*out_name)) return br_fail("serve --out <nuovo> oppure --in-place");
    if (!in_place) {
        char avail[64] = "";
        gmb_brain_rec_t t2;
        if (gmb_brain_reg_info(target, &t2) || gmb_brain_reg_available(target, avail, sizeof avail))
            return br_fail("il nome '%s' è già in uso (usa --in-place per aggiornare lo stesso cervello)", target);
    }

    char gdir[1024];
    gmb_paths_graphs_dir(gdir, sizeof gdir);

    if (!strcasecmp(to, "graph")) {
        char dbp[2048];
        if (!gmb_brain_reg_rag_db(name, dbp, sizeof dbp)) return br_fail("cervello '%s': nessun db RAG da convertire (usa RAG BUILD)", name);
        sqlite3* db = br_sql_open(dbp, 1);
        if (!db) return br_fail("db RAG non apribile (%s)", dbp);
        /* sorgenti + testi */
        typedef struct { char sid[2048]; char norm[2048]; sbuf_t text; } src_t;
        src_t* srcs = NULL;
        int nsrc = 0;
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(db, "SELECT id, source_id FROM vec_documents ORDER BY id", -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char* sid = (const char*)sqlite3_column_text(st, 1);
                srcs = (src_t*)realloc(srcs, (size_t)(nsrc + 1) * sizeof(src_t));
                memset(&srcs[nsrc], 0, sizeof(src_t));
                snprintf(srcs[nsrc].sid, sizeof srcs[nsrc].sid, "%s", sid ? sid : "");
                normalize_source(srcs[nsrc].sid, srcs[nsrc].norm, sizeof srcs[nsrc].norm);
                sb_init(&srcs[nsrc].text);
                nsrc++;
            }
            sqlite3_finalize(st);
        }
        if (nsrc == 0) { sqlite3_close(db); return br_fail("cervello '%s': nessun documento nel RAG", name); }
        /* testi per documento */
        sqlite3_stmt* ck = NULL;
        if (sqlite3_prepare_v2(db,
            "SELECT d.source_id, c.text FROM vec_chunks c JOIN vec_documents d ON d.id=c.document_id", -1, &ck, NULL) == SQLITE_OK) {
            while (sqlite3_step(ck) == SQLITE_ROW) {
                const char* sid = (const char*)sqlite3_column_text(ck, 0);
                const char* tx = (const char*)sqlite3_column_text(ck, 1);
                if (!sid || !tx) continue;
                for (int i = 0; i < nsrc; i++) if (!strcmp(srcs[i].sid, sid)) { sb_put(&srcs[i].text, tx); sb_put(&srcs[i].text, "\n"); break; }
            }
            sqlite3_finalize(ck);
        }
        sqlite3_close(db);

        wgraph_t g;
        memset(&g, 0, sizeof g);
        g.folders_json = strdup("{}");
        for (int i = 0; i < nsrc; i++) {
            g.nodes = (wgnode_t*)realloc(g.nodes, (size_t)(g.nn + 1) * sizeof(wgnode_t));
            wgnode_t* n = &g.nodes[g.nn++];
            n->id = strdup(srcs[i].norm);
            char* base = strrchr(srcs[i].norm, '/');
            base = base ? base + 1 : srcs[i].norm;
            char title[512];
            snprintf(title, sizeof title, "%s", base);
            char* dot = strrchr(title, '.');
            if (dot) *dot = 0;
            n->title = strdup(title);
            sbuf_t nb; sb_init(&nb);
            sb_put(&nb, "{\"id\": "); sb_esc(&nb, n->id);
            sb_put(&nb, ", \"title\": "); sb_esc(&nb, n->title);
            sb_put(&nb, ", \"frontmatter\": {}, \"community\": 0, \"degree\": 0}");
            n->raw = nb.p;
        }
        /* wikilink [[...]] */
        for (int i = 0; i < nsrc; i++) {
            const char* txt = srcs[i].text.p ? srcs[i].text.p : "";
            const char* p = txt;
            while ((p = strstr(p, "[[")) != NULL) {
                const char* e = strstr(p + 2, "]]");
                if (!e) break;
                char inner[1024];
                size_t l = (size_t)(e - p - 2);
                if (l >= sizeof inner) l = sizeof inner - 1;
                memcpy(inner, p + 2, l);
                inner[l] = 0;
                char* bar = strchr(inner, '|'); if (bar) *bar = 0;
                char* hash = strchr(inner, '#'); if (hash) *hash = 0;
                char* s = inner; while (*s == ' ') s++;
                size_t sl = strlen(s); while (sl && (s[sl - 1] == ' ' || s[sl - 1] == '.')) s[--sl] = 0;
                if (!*s) { p = e + 2; continue; }
                /* risoluzione: path esatto, basename, basename+.md */
                int to_idx = -1;
                for (int k = 0; k < nsrc && to_idx < 0; k++) {
                    if (!strcmp(srcs[k].norm, s)) { to_idx = k; break; }
                    char base[512], basemd[520];
                    const char* bs = strrchr(srcs[k].norm, '/');
                    bs = bs ? bs + 1 : srcs[k].norm;
                    snprintf(base, sizeof base, "%s", bs);
                    snprintf(basemd, sizeof basemd, "%s.md", s);
                    char* d1 = strrchr(base, '.');
                    if (d1) {
                        char base_noext[512];
                        snprintf(base_noext, sizeof base_noext, "%.*s", (int)(d1 - base), base);
                        if (!strcasecmp(base_noext, s)) { to_idx = k; break; }
                    }
                    if (!strcmp(base, basemd)) { to_idx = k; break; }
                }
                if (to_idx < 0) {
                    g.dangling = (char**)realloc(g.dangling, (size_t)(g.nd + 1) * sizeof(char*));
                    char dg[2400];
                    snprintf(dg, sizeof dg, "%s -> %s", srcs[i].norm, s);
                    g.dangling[g.nd++] = strdup(dg);
                } else if (to_idx != i) {
                    int dup = 0;
                    for (int k = 0; k < g.ne; k++)
                        if (!strcmp(g.edges[k].from, srcs[i].norm) && !strcmp(g.edges[k].to, srcs[to_idx].norm)) { dup = 1; break; }
                    if (!dup) {
                        g.edges = (wgedge_t*)realloc(g.edges, (size_t)(g.ne + 1) * sizeof(wgedge_t));
                        wgedge_t* x = &g.edges[g.ne++];
                        x->from = strdup(srcs[i].norm);
                        x->to = strdup(srcs[to_idx].norm);
                        x->type = strdup("link");
                        sbuf_t eb; sb_init(&eb);
                        sb_put(&eb, "{\"from\": "); sb_esc(&eb, x->from);
                        sb_put(&eb, ", \"to\": "); sb_esc(&eb, x->to);
                        sb_put(&eb, ", \"alias\": null, \"type\": \"link\", \"provenance\": \"EXTRACTED\", \"confidence\": 1}");
                        x->raw = eb.p;
                    }
                }
                p = e + 2;
            }
        }
        for (int i = 0; i < nsrc; i++) free(srcs[i].text.p);
        free(srcs);

        char dst[2400];
        snprintf(dst, sizeof dst, "%s/%s.json", gdir, target);
        if (dry_run) {
            int nn = g.nn, ne = g.ne, nd = g.nd;
            wg_free(&g);
            return br_ok("[dry-run] CONVERT rag→graph '%s': %d nodi, %d edge, %d dangling → %s", name, nn, ne, nd, dst);
        }
        if (g.ne < (min_edges > 0 ? min_edges : 0)) {
            int ne = g.ne;
            wg_free(&g);
            return br_fail("conversione rag→graph: solo %d edge trovati (minimo %d): il RAG non contiene "
                           "wikilink risolvibili (usa --min-edges 0 per accettare un grafo senza archi)", ne, min_edges);
        }
        gmb_fs_mkdir_p(gdir);
        char* js = wg_to_json(&g);
        int ok = js && br_write_file(dst, js);
        free(js);
        int nn = g.nn, ne = g.ne, nd = g.nd;
        wg_free(&g);
        if (!ok) return br_fail("scrittura del grafo fallita (%s)", dst);
        br_graph_write_artifacts(target, gdir);
        if (strcmp(target, name) == 0) {
            char avail[64] = "";
            gmb_brain_reg_available(name, avail, sizeof avail);
            gmb_brain_reg_register(name, info.source[0] ? info.source : NULL, info.vault_path[0] ? info.vault_path : NULL,
                                   (!strcmp(avail, "rag,graph")) ? "both" : "graph", NULL, NULL, 0, NULL);
        } else {
            gmb_brain_reg_register(target, info.source[0] ? info.source : NULL, NULL, "graph", NULL, NULL, 0, NULL);
        }
        return br_ok("CONVERT rag→graph: %d nodi, %d edge, %d dangling → %s", nn, ne, nd, dst);
    }

    /* graph → rag */
    char gp[2400];
    if (!gmb_brain_reg_graph_json(name, gp, sizeof gp)) return br_fail("cervello '%s': nessun grafo da convertire (usa GRAPH BUILD)", name);
    wgraph_t g;
    if (!wg_load(gp, &g)) return br_fail("grafo illeggibile (%s)", gp);
    if (g.nn == 0) { wg_free(&g); return br_fail("cervello '%s': grafo senza nodi", name); }
    char ragdir[1024], dbf[2400];
    gmb_paths_rag_dir(ragdir, sizeof ragdir);
    snprintf(dbf, sizeof dbf, "%s/%s.db", ragdir, target);
    if (dry_run) {
        int nn = g.nn;
        wg_free(&g);
        return br_ok("[dry-run] CONVERT graph→rag '%s': %d documenti → %s", name, nn, dbf);
    }
    gmb_fs_mkdir_p(ragdir);
    int indexed = 0, failed = 0;
    for (int i = 0; i < g.nn; i++) {
        sbuf_t doc;
        sb_init(&doc);
        sb_put(&doc, g.nodes[i].title ? g.nodes[i].title : g.nodes[i].id);
        sb_put(&doc, "\ncollegamenti: ");
        int wrote = 0;
        for (int k = 0; k < g.ne && wrote < 40; k++)
            if (!strcmp(g.edges[k].from, g.nodes[i].id)) { if (wrote) sb_put(&doc, ", "); sb_put(&doc, g.edges[k].to); wrote++; }
        sb_put(&doc, "\ncollegato da: ");
        wrote = 0;
        for (int k = 0; k < g.ne && wrote < 40; k++)
            if (!strcmp(g.edges[k].to, g.nodes[i].id)) { if (wrote) sb_put(&doc, ", "); sb_put(&doc, g.edges[k].from); wrote++; }
        int rc = gmb_rag_index_source(dbf, "wiki", g.nodes[i].id, doc.p ? doc.p : "", NULL, 2048, 256);
        if (rc >= 0 && rc != -1) indexed++;
        else failed++;
        free(doc.p);
    }
    wg_free(&g);
    if (indexed == 0)
        return br_fail("conversione graph→rag impossibile: nessun documento indicizzato (manca il provider di embedding?)");
    if (strcmp(target, name) == 0) {
        char avail[64] = "";
        gmb_brain_reg_available(name, avail, sizeof avail);
        gmb_brain_reg_register(name, info.source[0] ? info.source : NULL, info.vault_path[0] ? info.vault_path : NULL,
                               (!strcmp(avail, "rag,graph")) ? "both" : "rag", NULL, NULL, 0, NULL);
    } else {
        gmb_brain_reg_register(target, info.source[0] ? info.source : NULL, NULL, "rag", NULL, NULL, 0, NULL);
    }
    return br_ok("CONVERT graph→rag: %d documenti indicizzati → %s", indexed, dbf);
}

/* ══════════════════════════════════════════════════════════════════════════
 * VERIFY --golden (qualità del retrieval)
 * ══════════════════════════════════════════════════════════════════════════ */

gmb_brain_result_t gmb_brain_tool_verify_golden(const char* name, const char* golden, int topk, double min_p1)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    if (!golden || access(golden, R_OK) != 0) return br_fail("golden set non trovato: %s", golden ? golden : "");
    char db[2048];
    if (!gmb_brain_reg_rag_db(name, db, sizeof db)) return br_fail("cervello '%s': nessun db RAG (VERIFY --golden richiede la modalità rag)", name);
    if (topk <= 0) topk = 5;

    FILE* f = fopen(golden, "r");
    if (!f) return br_fail("golden set non leggibile: %s", golden);
    char line[16384];
    int nq = 0;
    double sp1 = 0, sp3 = 0, srec = 0, smrr = 0;
    sbuf_t out;
    sb_init(&out);
    while (fgets(line, sizeof line, f)) {
        char* t = line;
        while (*t == ' ' || *t == '\t') t++;
        if (!*t || *t == '#' || *t == '\n') continue;
        gmb_val_t row = gmb_json_parse(t);
        if (row.kind != GMB_TYPE_OBJECT) continue;
        gmb_val_t q = gmb_val_dict_get(row, "query");
        gmb_val_t es = gmb_val_dict_get(row, "expect_sources");
        if (q.kind != GMB_TYPE_STRING || !q.v_string.data || !q.v_string.data[0]) continue;
        if (es.kind != GMB_TYPE_ARRAY) continue;
        /* attese */
        char expect[16][512];
        int ne = 0;
        gmb_list_t el = gmb_val_foreach_list(es);
        gmb_int_t en = gmb_list_count(el);
        for (gmb_int_t i = 0; i < en && ne < 16; i++) {
            gmb_val_t e = gmb_list_get(el, i);
            if (e.kind == GMB_TYPE_STRING && e.v_string.data && e.v_string.data[0])
                snprintf(expect[ne++], sizeof expect[0], "%s", e.v_string.data);
        }
        gmb_list_free(&el);
        if (ne == 0) continue;

        char* res = gmb_rag_search(db, "wiki", q.v_string.data, topk, NULL);
        char hits[32][1024];
        int nh = 0;
        if (res) {
            const char* p = res;
            while ((p = strstr(p, "\"sourceId\":\"")) != NULL && nh < 32) {
                p += 12;
                const char* e = strchr(p, '"');
                if (!e) break;
                size_t l = (size_t)(e - p);
                if (l >= sizeof hits[0]) l = sizeof hits[0] - 1;
                memcpy(hits[nh], p, l);
                hits[nh][l] = 0;
                nh++;
                p = e + 1;
            }
            free(res);
        }
        int first = -1;
        for (int i = 0; i < nh && first < 0; i++)
            for (int k = 0; k < ne; k++)
                if (strcasestr(hits[i], expect[k])) { first = i; break; }
        double p1 = first == 0 ? 1.0 : 0.0;
        int top3rel = 0, top3n = nh < 3 ? nh : 3;
        for (int i = 0; i < top3n; i++)
            for (int k = 0; k < ne; k++)
                if (strcasestr(hits[i], expect[k])) { top3rel++; break; }
        double p3 = top3n > 0 ? (double)top3rel / (double)top3n : 0.0;
        int found = 0;
        for (int i = 0; i < nh; i++) {
            int rel = 0;
            for (int k = 0; k < ne; k++) if (strcasestr(hits[i], expect[k])) { rel = 1; break; }
            if (rel) found++;
        }
        double recall = ne > 0 ? (double)found / (double)ne : 0.0;
        double mrr = first >= 0 ? 1.0 / (double)(first + 1) : 0.0;
        sp1 += p1; sp3 += p3; srec += recall; smrr += mrr;
        char tag[16];
        if (first == 0) snprintf(tag, sizeof tag, "ok");
        else if (first > 0) snprintf(tag, sizeof tag, "r%d", first + 1);
        else snprintf(tag, sizeof tag, "MISS");
        char t2[2048];
        snprintf(t2, sizeof t2, "  %-5s P@1=%.0f recall@%d=%.2f MRR=%.2f  «%s»\n", tag, p1, topk, recall, mrr, q.v_string.data);
        sb_put(&out, t2);
        nq++;
    }
    fclose(f);
    if (nq == 0) { free(out.p); return br_fail("golden set vuoto o malformato: %s (attese righe JSON con query + expect_sources)", golden); }

    double p1m = sp1 / nq, p3m = sp3 / nq, rcm = srec / nq, mrrm = smrr / nq;
    char header[512];
    snprintf(header, sizeof header, "BRAIN VERIFY --golden — %s: %d query  ·  P@1 %.0f%% · P@3 %.0f%% · recall@%d %.0f%% · MRR %.2f",
             name, nq, p1m * 100.0, p3m * 100.0, topk, rcm * 100.0, mrrm);
    int ok = !(min_p1 == min_p1) || p1m >= min_p1;   /* min_p1 NaN = nessuna soglia */
    size_t cap = strlen(header) + strlen(out.p ? out.p : "") + 256;
    char* text = (char*)malloc(cap);
    if (!text) { free(out.p); return br_fail("memoria esaurita"); }
    snprintf(text, cap, "%s\n%s", header, out.p ? out.p : "");
    free(out.p);
    gmb_brain_result_t r;
    r.ok = ok;
    r.text = text;
    r.error = ok ? NULL : strdup("P@1 sotto la soglia richiesta");
    return r;
}

