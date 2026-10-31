/* gmb_orchestration.c — Persistenza su disco dello stato di orchestrazione (mondo C).
 *
 * Salva/riprende lo stato dei registri di orchestrazione (Artifact, Handoff, Metrics,
 * Checkpoint, Replay) in un database SQLite su file (tabella KV `orch_kv`). Il file è condiviso
 * con il runtime C# (stesso schema/format). Runtime CONDIVISO (AGENTS §0bis).
 * Nota: il VectorDB/RAG è orientato a embedding+chunking, non adatto a stato KV → SQLite.
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sqlite3.h>

#define GMB_ORCH_NSCOPES 11
static const char* g_orch_scopes[GMB_ORCH_NSCOPES] = {
    "artifact", "handoff", "metrics", "checkpoint", "replay",
    "category", "resource", "rules", "task", "plan", "hook"
};

static char* gmb_orch_export_one(int i)
{
    switch (i)
    {
        case 0: return gmb_artifact_export();
        case 1: return gmb_handoff_export();
        case 2: return gmb_metric_export();
        case 3: return gmb_checkpoint_export();
        case 4: return gmb_replay_export();
        case 5: return gmb_category_export();
        case 6: return gmb_resource_export();
        case 7: return gmb_rules_export();
        case 8: return gmb_task_export();
        case 9: return gmb_plan_export();
        case 10: return gmb_hook_export();
        default: return NULL;
    }
}

static void gmb_orch_import_one(int i, const char* state)
{
    switch (i)
    {
        case 0: gmb_artifact_import(state); break;
        case 1: gmb_handoff_import(state); break;
        case 2: gmb_metric_import(state); break;
        case 3: gmb_checkpoint_import(state); break;
        case 4: gmb_replay_import(state); break;
        case 5: gmb_category_import(state); break;
        case 6: gmb_resource_import(state); break;
        case 7: gmb_rules_import(state); break;
        case 8: gmb_task_import(state); break;
        case 9: gmb_plan_import(state); break;
        case 10: gmb_hook_import(state); break;
        default: break;
    }
}

static void gmb_orch_clear_all(void)
{
    gmb_artifact_clear();
    gmb_handoff_clear();
    gmb_metric_reset();
    gmb_checkpoint_clear();
    gmb_replay_clear();
    gmb_category_clear();
    gmb_resource_clear();
    gmb_rules_clear();
    gmb_task_clear();
    gmb_plan_clear();
    gmb_hook_clear();
}

/* Salva lo stato corrente in `path` (SQLite). Ritorna il numero di scope salvati (0 = errore). */
int gmb_orch_save(const char* path)
{
    const char* p = (path && *path) ? path : NULL;
    char def[1024];
    if (!p)
    {
        char cfg[1024];
        gmb_paths_config_dir(cfg, sizeof(cfg));
        snprintf(def, sizeof(def), "%s/orchestration.db", cfg);
        p = def;
    }
    /* Crea la cartella padre se manca (parità C# OrchestrationPersistence.Save). */
    {
        const char* slash = strrchr(p, '/');
        if (slash && slash != p)
        {
            char dir[1024];
            size_t dl = (size_t)(slash - p);
            if (dl < sizeof(dir)) { memcpy(dir, p, dl); dir[dl] = '\0'; mkdir(dir, 0755); }
        }
    }
    sqlite3* db = NULL;
    if (sqlite3_open(p, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return 0; }
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS orch_kv(scope TEXT PRIMARY KEY, state TEXT NOT NULL)", 0, 0, 0);
    int n = 0;
    for (int i = 0; i < GMB_ORCH_NSCOPES; i++)
    {
        char* st = gmb_orch_export_one(i);
        sqlite3_stmt* stmt = NULL;
        if (sqlite3_prepare_v2(db, "INSERT INTO orch_kv(scope,state) VALUES(?,?) ON CONFLICT(scope) DO UPDATE SET state=excluded.state", -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, g_orch_scopes[i], -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, st ? st : "", -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_DONE) n++;
            sqlite3_finalize(stmt);
        }
        free(st);
    }
    sqlite3_close(db);
    return n;
}

/* Riprende lo stato da `path` (i registri sono SOSTITUITI). Ritorna gli scope ripristinati. */
int gmb_orch_load(const char* path)
{
    const char* p = (path && *path) ? path : NULL;
    char def[1024];
    if (!p)
    {
        char cfg[1024];
        gmb_paths_config_dir(cfg, sizeof(cfg));
        snprintf(def, sizeof(def), "%s/orchestration.db", cfg);
        p = def;
    }
    sqlite3* db = NULL;
    if (sqlite3_open(p, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return 0; }
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS orch_kv(scope TEXT PRIMARY KEY, state TEXT NOT NULL)", 0, 0, 0);
    gmb_orch_clear_all();
    int n = 0;
    for (int i = 0; i < GMB_ORCH_NSCOPES; i++)
    {
        sqlite3_stmt* stmt = NULL;
        if (sqlite3_prepare_v2(db, "SELECT state FROM orch_kv WHERE scope=?", -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, g_orch_scopes[i], -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char* v = (const char*)sqlite3_column_text(stmt, 0);
                if (v) { gmb_orch_import_one(i, v); n++; }
            }
            sqlite3_finalize(stmt);
        }
    }
    sqlite3_close(db);
    return n;
}
