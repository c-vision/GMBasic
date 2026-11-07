/* FILE: native/gmb_runtime_c/src/gmb_secondbrain.c
 * Porting condiviso di Basic.AI.Knowledge (VaultManager/LinkManager/Ingestor) — §0bis:
 * folder props, link manuali, sync folder e web scrape come funzioni gmb_* che VM/C e C99
 * chiamano. Stesso file SQLite di C# (<configDir>/vaults.db — gmb_paths), tabelle vaults/vault_folders/
 * note_links. Mai stub: se il vault non esiste nel db, ritorna errore esplicito.
 */
#include "gmb_runtime.h"
#include "gmb_brains.h"   /* gmb_brain_list: elenco dei cervelli su disco (vendor/exe/cache) */
#include "gmb_graph.h"
#include "gmb_commonlib.h"   /* gmb_sha256 (content_hash vault) */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

static void sb_db_path(char* out, size_t cap)
{
    gmb_paths_vaults_db(out, cap);
}

static void sb_migrate(sqlite3* db);   /* forward (definita dopo sb_open) */

static sqlite3* sb_open(void)
{
    char path[1024];
    sb_db_path(path, sizeof(path));
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    sqlite3* db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return NULL; }
    const char* schema =
        "CREATE TABLE IF NOT EXISTS vaults ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT UNIQUE NOT NULL,"
        " root_path TEXT NOT NULL,"
        " pattern TEXT NOT NULL,"
        " index_version INTEGER NOT NULL DEFAULT 1,"
        " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        " indexed_at TEXT);"
        "CREATE TABLE IF NOT EXISTS vault_files ("
        " vault_id INTEGER NOT NULL,"
        " path TEXT NOT NULL,"
        " folder_path TEXT NOT NULL,"
        " content_hash TEXT NOT NULL,"
        " indexed_at TEXT NOT NULL DEFAULT (datetime('now')),"
        " PRIMARY KEY (vault_id, path));"
        "CREATE TABLE IF NOT EXISTS vault_folders ("
        " vault_id INTEGER NOT NULL,"
        " folder_path TEXT NOT NULL,"
        " indexed INTEGER NOT NULL DEFAULT 1,"
        " tags TEXT NOT NULL DEFAULT '',"
        " PRIMARY KEY (vault_id, folder_path));"
        "CREATE TABLE IF NOT EXISTS note_links ("
        " vault_id INTEGER NOT NULL,"
        " from_path TEXT NOT NULL,"
        " to_path TEXT NOT NULL,"
        " alias TEXT,"
        " PRIMARY KEY (vault_id, from_path, to_path));"
        "CREATE TABLE IF NOT EXISTS brain_attach ("
        " name TEXT PRIMARY KEY);"
        "CREATE TABLE IF NOT EXISTS vault_policy ("
        " vault_id INTEGER PRIMARY KEY,"
        " policy TEXT NOT NULL);";
    sqlite3_exec(db, schema, NULL, NULL, NULL);
    /* Migrazione idempotente: un db creato da una versione C precedente (o dal C# con
     * colonne in più) può mancare di vault_files o delle colonne extra di vaults. Il C
     * DEVE creare lo schema completo da solo (§0bis: mondo C indipendente dal C#). */
    sb_migrate(db);
    return db;
}

/* Aggiunge le colonne/tabelle mancanti a un db vaults pre-esistente (idempotente).
 * C# VaultManager crea: vaults(+index_version,created_at,indexed_at) e vault_files. */
static void sb_migrate(sqlite3* db)
{
    if (!db) return;
    char* err = NULL;
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS vault_files ("
        " vault_id INTEGER NOT NULL,"
        " path TEXT NOT NULL,"
        " folder_path TEXT NOT NULL,"
        " content_hash TEXT NOT NULL,"
        " indexed_at TEXT NOT NULL DEFAULT (datetime('now')),"
        " PRIMARY KEY (vault_id, path));",
        NULL, NULL, &err);
    if (err) { sqlite3_free(err); err = NULL; }
    /* colonne extra su vaults (se la tabella esiste già senza) */
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "PRAGMA table_info(vaults);", -1, &st, NULL) == SQLITE_OK)
    {
        int has_ver = 0, has_ca = 0, has_ia = 0;
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* col = (const char*)sqlite3_column_text(st, 1);
            if (!col) continue;
            if (strcmp(col, "index_version") == 0) has_ver = 1;
            else if (strcmp(col, "created_at") == 0) has_ca = 1;
            else if (strcmp(col, "indexed_at") == 0) has_ia = 1;
        }
        sqlite3_finalize(st);
        const char* adds[3] = { NULL, NULL, NULL };
        int n = 0;
        if (!has_ver) adds[n++] = "ALTER TABLE vaults ADD COLUMN index_version INTEGER NOT NULL DEFAULT 1;";
        if (!has_ca)  adds[n++] = "ALTER TABLE vaults ADD COLUMN created_at TEXT NOT NULL DEFAULT (datetime('now'));";
        if (!has_ia)  adds[n++] = "ALTER TABLE vaults ADD COLUMN indexed_at TEXT;";
        for (int i = 0; i < n; i++)
        {
            char* e2 = NULL;
            sqlite3_exec(db, adds[i], NULL, NULL, &e2);
            if (e2) { sqlite3_free(e2); }
        }
    }
}

static long long sb_vault_id(sqlite3* db, const char* name)
{
    sqlite3_stmt* st = NULL;
    long long id = -1;
    if (sqlite3_prepare_v2(db, "SELECT id FROM vaults WHERE name=$1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    return id;
}

static char g_sb_active_vault[128];   /* vault attivo ("" = brain attivo, parità C#) */
static int sb_vault_root(sqlite3* db, const char* name, char* out, size_t cap)
{
    if ((!name || !*name) && g_sb_active_vault[0]) name = g_sb_active_vault;
    if (!name || !*name) return 0;
    sqlite3_stmt* st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(db, "SELECT root_path FROM vaults WHERE name=$1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* r = (const char*)sqlite3_column_text(st, 0);
            if (r) { snprintf(out, cap, "%s", r); ok = 1; }
        }
        sqlite3_finalize(st);
    }
    return ok;
}

/* ── Folder props (AI VAULT ADD/REMOVE/SET FOLDER) ── */

int gmb_vault_folder_add(const char* vault_name, const char* folder, int indexed, const char* tags)
{
    if (!vault_name || !folder) return -1;
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long vid = sb_vault_id(db, vault_name);
    if (vid < 0) { sqlite3_close(db); return -1; }
    /* Guard zip/escape (parità VaultManager.AddFolder): la cartella deve esistere SOTTO la
       root e NON essere la root stessa; si registra il path RELATIVO. Prima il C inseriva
       qualunque valore (es. ".") — divergenza dal C# (che rifiuta "."): RemoveFolder trovava
       una riga che il C# non aveva (bug vaultfolder_domain). */
    char root[1024];
    if (!sb_vault_root(db, vault_name, root, sizeof(root))) { sqlite3_close(db); return -1; }
    const char* f = folder;
    while (*f == '/') f++;
    if (*f == '\0' || strcmp(f, ".") == 0 || strcmp(f, "./") == 0) { sqlite3_close(db); return -1; }
    char rel[1024];
    snprintf(rel, sizeof(rel), "%s", f);
    size_t rl = strlen(rel);
    while (rl > 0 && rel[rl - 1] == '/') rel[--rl] = '\0';
    if (rl == 0 || strcmp(rel, ".") == 0) { sqlite3_close(db); return -1; }
    char full[2048];
    snprintf(full, sizeof(full), "%s/%s", root, rel);
    struct stat stt;
    if (stat(full, &stt) != 0 || !S_ISDIR(stt.st_mode)) { sqlite3_close(db); return -1; }
    int rc = -1;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO vault_folders(vault_id,folder_path,indexed,tags) VALUES($1,$2,$3,$4)"
            " ON CONFLICT(vault_id,folder_path) DO UPDATE SET indexed=$3,tags=$4;",
            -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, rel, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, indexed ? 1 : 0);
        sqlite3_bind_text(st, 4, tags ? tags : "", -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(st) == SQLITE_DONE ? 1 : -1;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return rc;
}

int gmb_vault_folder_remove(const char* vault_name, const char* folder)
{
    if (!vault_name || !folder) return -1;
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long vid = sb_vault_id(db, vault_name);
    if (vid < 0) { sqlite3_close(db); return -1; }
    sqlite3_stmt* st = NULL;
    int n = 0;
    if (sqlite3_prepare_v2(db, "DELETE FROM vault_folders WHERE vault_id=$1 AND folder_path=$2;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, folder, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_DONE) n = (int)sqlite3_changes(db);
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return n;
}

/* AI.VAULTADDFOLDER — parità con l'oracolo (VaultManager.AddFolder -> bool). In questo linguaggio
 * il VERO è -1 (TRUE = -1, vedi il ramo GMB_TYPE_BOOL -> -1.0 nel VM/C). gmb_vault_folder_add
 * ritorna 1/0/-1 (contratto INTERNO asserito dal selfcheck): qui si mappa al valore dell'oracolo.
 * Unica sorgente per i tre codegen (§0bis): prima la VM/C ritornava 1 sul successo mentre
 * IL/PCode ritornavano -1 (divergenza trovata dal sample AiVaultNotesCoverage). */
int gmb_vault_add_folder_bool(const char* vault_name, const char* folder, int indexed, const char* tags) {
    return gmb_vault_folder_add(vault_name, folder, indexed, tags) ? -1 : 0;
}

int gmb_vault_folder_set(const char* vault_name, const char* folder, int* indexed, const char* tags)
{
    /* Parità C# VaultManager.SetFolderProps (2026-09-23, Parita-5-Backend-Residui §2):
     * UPDATE; se nessuna riga → INSERT OR IGNORE (SENZA il controllo esistenza-directory
     * di AddFolder) → 1. Prima il fallback delegava a gmb_vault_folder_add (che pretende
     * la directory sotto root) → -1 mentre IL dava 1, e il successivo REMOVE dava 0
     * mentre IL dava 1 (AiVaultNotesCoverage setprops/rmfolder). Normalizzazione path
     * come il C#: trim '/' + '\\' → '/'. */
    if (!vault_name || !folder) return -1;
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long vid = sb_vault_id(db, vault_name);
    if (vid < 0) { sqlite3_close(db); return -1; }
    char rel[1024];
    {
        const char* f = folder;
        while (*f == '/') f++;
        size_t n = strlen(f);
        if (n >= sizeof(rel)) n = sizeof(rel) - 1;
        size_t j = 0;
        for (size_t i = 0; i < n; i++) rel[j++] = (f[i] == '\\') ? '/' : f[i];
        while (j > 0 && rel[j - 1] == '/') j--;
        rel[j] = '\0';
    }
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db,
            "UPDATE vault_folders SET indexed=COALESCE($3,indexed),tags=COALESCE($4,tags)"
            " WHERE vault_id=$1 AND folder_path=$2;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, rel, -1, SQLITE_TRANSIENT);
        if (indexed) sqlite3_bind_int(st, 3, *indexed ? 1 : 0); else sqlite3_bind_null(st, 3);
        if (tags) sqlite3_bind_text(st, 4, tags, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(st, 4);
        if (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db) > 0) { sqlite3_finalize(st); sqlite3_close(db); return 1; }
        sqlite3_finalize(st);
    }
    int rc = 1;
    if (sqlite3_prepare_v2(db,
            "INSERT OR IGNORE INTO vault_folders(vault_id,folder_path,indexed,tags)"
            " VALUES($1,$2,$3,$4);", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, rel, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, indexed ? (*indexed ? 1 : 0) : 1);
        sqlite3_bind_text(st, 4, tags ? tags : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) != SQLITE_DONE) rc = -1;
        sqlite3_finalize(st);
    }
    else rc = -1;
    sqlite3_close(db);
    return rc;
}
/* AI.VAULTSETFOLDERPROPS(name, folder, indexed$, tags$) — parità VaultManager.SetFolderProps:
 * indexed$ vuoto -> NULL, altrimenti ("0" ? false : true); tags$ vuoto -> NULL.
 * Prima questa conversione era EMESSA INLINE dai codegen (statement-expression con buffer locale,
 * §0bis) e la gamba nativa ignorava del tutto il 4o argomento (tags). Ora è qui, unica sorgente. */
int gmb_vault_folder_set_str(const char* vault_name, const char* folder, const char* indexed, const char* tags) {
    int idx = 0, has_idx = (indexed && *indexed);
    if (has_idx) idx = (strcmp(indexed, "0") != 0);
    return gmb_vault_folder_set(vault_name, folder, has_idx ? &idx : NULL,
                               (tags && *tags) ? tags : NULL);
}


char* gmb_vault_folders_csv(const char* vault_name)
{
    if (!vault_name) return NULL;
    sqlite3* db = sb_open();
    if (!db) return NULL;
    long long vid = sb_vault_id(db, vault_name);
    if (vid < 0) { sqlite3_close(db); return NULL; }
    sqlite3_stmt* st = NULL;
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) { sqlite3_close(db); return NULL; }
    out[0] = '\0';
    if (sqlite3_prepare_v2(db, "SELECT folder_path,indexed,tags FROM vault_folders WHERE vault_id=$1 ORDER BY folder_path;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* f = (const char*)sqlite3_column_text(st, 0);
            long long idx = sqlite3_column_int64(st, 1);
            const char* t = (const char*)sqlite3_column_text(st, 2);
            int need = snprintf(NULL, 0, "%s%s|%lld|%s", len ? "," : "", f ? f : "", idx, t ? t : "");
            if ((size_t)(len + need + 1) > cap) { cap = (len + need + 1) * 2; char* n = realloc(out, cap); if (!n) break; out = n; }
            len += snprintf(out + len, cap - len, "%s%s|%lld|%s", len ? "," : "", f ? f : "", idx, t ? t : "");
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return out;
}

/* ── Link manuali (AI NOTE LINK/UNLINK/SET ALIAS) ── */

static int sb_link_upsert(const char* vault_name, const char* from, const char* to, const char* alias)
{
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long vid = sb_vault_id(db, vault_name);
    if (vid < 0) { sqlite3_close(db); return -1; }
    sqlite3_stmt* st = NULL;
    int rc = -1;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO note_links(vault_id,from_path,to_path,alias) VALUES($1,$2,$3,$4)"
            " ON CONFLICT(vault_id,from_path,to_path) DO UPDATE SET alias=$4;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, from, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, to, -1, SQLITE_TRANSIENT);
        if (alias) sqlite3_bind_text(st, 4, alias, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(st, 4);
        rc = sqlite3_step(st) == SQLITE_DONE ? 1 : -1;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return rc;
}

int gmb_note_link(const char* vault_name, const char* from, const char* to, const char* alias)
{
    if (!vault_name || !from || !to) return -1;
    return sb_link_upsert(vault_name, from, to, alias);
}

int gmb_note_unlink(const char* vault_name, const char* from, const char* to)
{
    if (!vault_name || !from || !to) return -1;
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long vid = sb_vault_id(db, vault_name);
    if (vid < 0) { sqlite3_close(db); return -1; }
    sqlite3_stmt* st = NULL;
    int n = 0;
    if (sqlite3_prepare_v2(db, "DELETE FROM note_links WHERE vault_id=$1 AND from_path=$2 AND to_path=$3;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, from, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, to, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_DONE) n = (int)sqlite3_changes(db);
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return n;
}

int gmb_note_set_alias(const char* vault_name, const char* from, const char* to, const char* alias)
{
    if (!vault_name || !from || !to || !alias) return -1;
    gmb_note_unlink(vault_name, from, to);
    return sb_link_upsert(vault_name, from, to, alias);
}

char* gmb_links_csv(const char* vault_name)
{
    if (!vault_name) return NULL;
    sqlite3* db = sb_open();
    if (!db) return NULL;
    long long vid = sb_vault_id(db, vault_name);
    if (vid < 0) { sqlite3_close(db); return NULL; }
    sqlite3_stmt* st = NULL;
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) { sqlite3_close(db); return NULL; }
    out[0] = '\0';
    if (sqlite3_prepare_v2(db, "SELECT from_path,to_path,alias FROM note_links WHERE vault_id=$1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* f = (const char*)sqlite3_column_text(st, 0);
            const char* t = (const char*)sqlite3_column_text(st, 1);
            const char* a = (const char*)sqlite3_column_text(st, 2);
            int need = snprintf(NULL, 0, "%s%s>%s>%s", len ? "," : "", f ? f : "", t ? t : "", a ? a : "");
            if ((size_t)(len + need + 1) > cap) { cap = (len + need + 1) * 2; char* n = realloc(out, cap); if (!n) break; out = n; }
            len += snprintf(out + len, cap - len, "%s%s>%s>%s", len ? "," : "", f ? f : "", t ? t : "", a ? a : "");
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return out;
}

/* ── Sync folder (bidirectional/push_only, newest_wins) ── */

static void sb_copy_file(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in) return;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", dst);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(out);
    fclose(in);
}

static void sb_push_dir(const char* vault_root, const char* src, const char* rel_dir, int* imp, int* upd, int* conf)
{
    char rootdir[2048];
    if (rel_dir && *rel_dir) snprintf(rootdir, sizeof(rootdir), "%s/%s", vault_root, rel_dir);
    else snprintf(rootdir, sizeof(rootdir), "%s", vault_root);
    DIR* d = opendir(rootdir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)))
    {
        /* skip SOLO . e .. — i dotfile vanno inclusi (parità Directory.GetFiles AllDirectories C#) */
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char rpath[2048], spath[2048];
        if (rel_dir && *rel_dir) snprintf(rpath, sizeof(rpath), "%s/%s", rel_dir, e->d_name);
        else snprintf(rpath, sizeof(rpath), "%s", e->d_name);
        snprintf(spath, sizeof(spath), "%s/%s", rootdir, e->d_name);
        struct stat st;
        if (stat(spath, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { sb_push_dir(vault_root, src, rpath, imp, upd, conf); continue; }
        if (!S_ISREG(st.st_mode)) continue;
        char dst[2048];
        snprintf(dst, sizeof(dst), "%s/%s", src, rpath);
        struct stat dst_st;
        if (stat(dst, &dst_st) != 0)
        {
            sb_copy_file(spath, dst);
            (*imp)++;
        }
        else if (st.st_mtime > dst_st.st_mtime + 1)
        {
            sb_copy_file(spath, dst);
            (*upd)++;
        }
    }
    closedir(d);
}

static void sb_pull_dir(const char* vault_root, const char* src, const char* rel_dir, int* imp, int* upd, int* conf)
{
    char srcdir[2048];
    if (rel_dir && *rel_dir) snprintf(srcdir, sizeof(srcdir), "%s/%s", src, rel_dir);
    else snprintf(srcdir, sizeof(srcdir), "%s", src);
    DIR* d = opendir(srcdir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)))
    {
        /* skip SOLO . e .. — i dotfile vanno inclusi (parità Directory.GetFiles AllDirectories C#) */
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char rpath[2048], spath[2048];
        if (rel_dir && *rel_dir) snprintf(rpath, sizeof(rpath), "%s/%s", rel_dir, e->d_name);
        else snprintf(rpath, sizeof(rpath), "%s", e->d_name);
        snprintf(spath, sizeof(spath), "%s/%s", srcdir, e->d_name);
        struct stat st;
        if (stat(spath, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { sb_pull_dir(vault_root, src, rpath, imp, upd, conf); continue; }
        if (!S_ISREG(st.st_mode)) continue;
        char dst[2048];
        snprintf(dst, sizeof(dst), "%s/%s", vault_root, rpath);
        struct stat dst_st;
        if (stat(dst, &dst_st) != 0)
        {
            sb_copy_file(spath, dst);
            (*imp)++;
        }
        else if (st.st_mtime > dst_st.st_mtime + 1)
        {
            sb_copy_file(spath, dst);
            (*upd)++;
        }
    }
    closedir(d);
}

char* gmb_sync_folder(const char* vault_name, const char* src, const char* direction, const char* conflict)
{
    if (!vault_name || !src) return NULL;
    sqlite3* db = sb_open();
    if (!db) return NULL;
    char root[1024];
    /* Vault sconosciuto: 0/0/0 (parità managed SyncFolder: nessun lavoro, non errore).
     * Fix 2026-09-22 (MiniTest-Oracolo §0.52: ai_syncfolder dava riga vuota su VMC/C99Native). */
    if (!sb_vault_root(db, vault_name, root, sizeof(root))) { sqlite3_close(db); return strdup("0/0/0"); }
    sqlite3_close(db);

    int push = direction && (strcasecmp(direction, "push") == 0 || strcasecmp(direction, "push_only") == 0);
    int imp = 0, upd = 0, conf = 0;
    if (push) sb_push_dir(root, src, NULL, &imp, &upd, &conf);
    else sb_pull_dir(root, src, NULL, &imp, &upd, &conf);

    char out[64];
    snprintf(out, sizeof(out), "%d/%d/%d", imp, upd, conf);
    return strdup(out);
}

/* ── Web scrape (AI INGEST WEB) ── */

static char* sb_strip_html(const char* html)
{
    if (!html) return NULL;
    size_t cap = strlen(html) + 1;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    int in_tag = 0;
    for (const char* p = html; *p && o + 1 < cap; p++)
    {
        if (*p == '<') { in_tag = 1; continue; }
        if (*p == '>') { in_tag = 0; continue; }
        if (!in_tag) out[o++] = *p;
    }
    out[o] = '\0';
    return out;
}

char* gmb_web_scrape(const char* vault_name, const char* url, const char* rel_path)
{
    if (!vault_name || !url) return NULL;
    /* Parità Ingestor.WebScrape: con URL NON assoluta il HttpClientHelper (.NET) ritorna
       "Error: 0 An invalid request URI was provided...". Il C non ha HttpClient: emettiamo lo
       stesso testo del contratto (prima tornava NULL → stringa vuota, divergenza C#/C). */
    if (!strstr(url, "://"))
        return strdup("Error: 0 An invalid request URI was provided. Either the request URI must be an absolute URI or BaseAddress must be set.");
    char* body = gmb_net_http_get(url);
    if (!body) return NULL;
    char* text = sb_strip_html(body);
    free(body);
    if (!text) return NULL;
    /* salva come nota: il path nel vault richiede la root; se rel_path assente → web/<host>.md */
    sqlite3* db = sb_open();
    if (!db) { free(text); return NULL; }
    char root[1024];
    if (!sb_vault_root(db, vault_name, root, sizeof(root))) { sqlite3_close(db); free(text); return NULL; }
    sqlite3_close(db);

    char target[2048];
    char relbuf[1024];
    if (rel_path && *rel_path) { snprintf(target, sizeof(target), "%s/%s", root, rel_path); snprintf(relbuf, sizeof(relbuf), "%s", rel_path); }
    else
    {
        /* web/<host>.md */
        const char* h = strstr(url, "://");
        const char* host = h ? h + 3 : url;
        char hostbuf[256];
        size_t i = 0;
        while (host[i] && host[i] != '/' && host[i] != ':' && i < sizeof(hostbuf) - 1) { hostbuf[i] = host[i]; i++; }
        hostbuf[i] = '\0';
        for (char* c = hostbuf; *c; c++) if (*c == '.') *c = '_';
        snprintf(target, sizeof(target), "%s/web/%s.md", root, hostbuf);
        snprintf(relbuf, sizeof(relbuf), "web/%s.md", hostbuf);
    }
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s", target);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    FILE* f = fopen(target, "wb");
    if (!f) { free(text); return NULL; }
    fprintf(f, "---\ntitle: web\ntype: web\n---\n%s\n", text);
    fclose(f);
    free(text);
    /* parità C# WebScrape: ritorna il path RELATIVO nel vault (non l'assoluto). */
    return strdup(relbuf);
}

/* ── Vault info / Note (porting VaultManager/NoteManager, §0bis) ── */

static long sb_folder_count(sqlite3* db, long long vid) {
    sqlite3_stmt* st = NULL;
    long n = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM vault_files WHERE vault_id=$1;", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, vid);
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    return n;
}

static int sb_resolve(sqlite3* db, const char* name, const char* rel, char* out, size_t cap) {
    if (!sb_vault_root(db, name, out, cap)) return 0;
    size_t rl = strlen(out);
    snprintf(out + rl, cap - rl, "/%s", rel ? rel : "");
    return 1;
}

char* gmb_vault_info_json(const char* name) {
    if (!name) return NULL;
    sqlite3* db = sb_open(); if (!db) return NULL;
    char root[1024] = "", pattern[256] = "";
    long long vid = -1, index_version = 0;
    char indexed_at[64] = "";
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT id,root_path,pattern,index_version,COALESCE(indexed_at,'') FROM vaults WHERE name=$1;",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            vid = sqlite3_column_int64(st, 0);
            snprintf(root, sizeof(root), "%s", (const char*)sqlite3_column_text(st, 1));
            snprintf(pattern, sizeof(pattern), "%s", (const char*)sqlite3_column_text(st, 2));
            index_version = sqlite3_column_int64(st, 3);
            const char* ia = (const char*)sqlite3_column_text(st, 4);
            if (ia) snprintf(indexed_at, sizeof(indexed_at), "%s", ia);
        }
        sqlite3_finalize(st);
    }
    if (vid < 0) { sqlite3_close(db); return NULL; }
    long fc = sb_folder_count(db, vid);               /* COUNT vault_files (fileCount) */
    long dc = 0;                                       /* COUNT DISTINCT folder_path */
    sqlite3_stmt* cs = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(DISTINCT folder_path) FROM vault_files WHERE vault_id=$1;", -1, &cs, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(cs, 1, vid);
        if (sqlite3_step(cs) == SQLITE_ROW) dc = sqlite3_column_int64(cs, 0);
        sqlite3_finalize(cs);
    }
    sqlite3_close(db);
    /* parità VaultManager.VaultInfo C#: {name,rootPath,pattern,indexVersion,indexedAt,
     * fileCount,folderCount} (fileCount = COUNT vault_files; folderCount = COUNT DISTINCT). */
    char ia_json[80];
    if (indexed_at[0]) snprintf(ia_json, sizeof(ia_json), "\"%s\"", indexed_at);
    else snprintf(ia_json, sizeof(ia_json), "null");
    char* out = (char*)malloc(1400);
    if (!out) return NULL;
    snprintf(out, 1400,
        "{\"name\":\"%s\",\"rootPath\":\"%s\",\"pattern\":\"%s\",\"indexVersion\":%lld,"
        "\"indexedAt\":%s,\"fileCount\":%ld,\"folderCount\":%ld}",
        name, root, pattern, index_version, ia_json, fc, dc);
    return out;
}

char* gmb_vault_folders_json(const char* name) {
    if (!name) return NULL;
    sqlite3* db = sb_open(); if (!db) return NULL;
    long long vid = sb_vault_id(db, name);
    if (vid < 0) { sqlite3_close(db); return NULL; }
    sqlite3_stmt* st = NULL;
    size_t cap = 128, len = 0;
    char* out = (char*)malloc(cap); if (!out) { sqlite3_close(db); return NULL; }
    out[0] = '\0';
    if (sqlite3_prepare_v2(db, "SELECT DISTINCT folder_path FROM vault_files WHERE vault_id=$1 ORDER BY folder_path;", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, vid);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* f = (const char*)sqlite3_column_text(st, 0);
            int need = snprintf(NULL, 0, "%s\"%s\"", len ? "," : "", f ? f : "");
            if (len + (size_t)need + 1 > cap) { cap = (len + need + 1) * 2; char* n = realloc(out, cap); if (!n) break; out = n; }
            len += snprintf(out + len, cap - len, "%s\"%s\"", len ? "," : "", f ? f : "");
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    size_t olen = len + 8;
    char* res = (char*)malloc(olen);
    if (!res) { free(out); return NULL; }
    snprintf(res, olen, "[%s]", out);
    free(out);
    return res;
}

long gmb_vault_count(const char* name) {
    if (!name) return -1;
    sqlite3* db = sb_open(); if (!db) return -1;
    long long vid = sb_vault_id(db, name);
    long n = vid < 0 ? -1 : sb_folder_count(db, vid);
    sqlite3_close(db);
    return n;
}

char* gmb_vault_list_json(void) {
    sqlite3* db = sb_open(); if (!db) return NULL;
    sqlite3_stmt* st = NULL;
    size_t cap = 128, len = 0;
    char* out = (char*)malloc(cap); if (!out) { sqlite3_close(db); return NULL; }
    out[0] = '\0';
    if (sqlite3_prepare_v2(db, "SELECT name FROM vaults ORDER BY name;", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* n = (const char*)sqlite3_column_text(st, 0);
            int need = snprintf(NULL, 0, "%s\"%s\"", len ? "," : "", n ? n : "");
            if (len + (size_t)need + 1 > cap) { cap = (len + need + 1) * 2; char* r = realloc(out, cap); if (!r) break; out = r; }
            len += snprintf(out + len, cap - len, "%s\"%s\"", len ? "," : "", n ? n : "");
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    size_t olen = len + 8;
    char* res = (char*)malloc(olen);
    if (!res) { free(out); return NULL; }
    snprintf(res, olen, "[%s]", out);
    free(out);
    return res;
}

/* ── Vault CREATE / INDEX / INDEXFILE / REMOVEFILE (porting VaultManager/VaultIndex C#) ──
 * Il backend C è indipendente dal C# (§0bis): registra/indicizza un vault su SQLite
 * config/vaults.db (stesso file del C#) con content_hash SHA256 hex UPPERCASE (parità
 * Convert.ToHexString del C# — il dedup incrementale cross-runtime deve confrontare hash
 * identici). Schema completo creato da sb_open/sb_migrate. */

/* SHA256 hex UPPERCASE di un file (parità C# VaultManager.ComputeHash: Convert.ToHexString).
 * Ritorna stringa allocata (65 byte) o NULL. Legge il file intero (file .md di note). */
static char* sb_file_hash_upper(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0 || sz > (64L << 20)) { fclose(f); return NULL; }
    rewind(f);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    unsigned char digest[32];
    gmb_sha256((const uint8_t*)buf, rd, digest);
    free(buf);
    char* hex = (char*)malloc(65);
    if (!hex) return NULL;
    static const char* u = "0123456789ABCDEF";
    for (int i = 0; i < 32; i++)
    {
        hex[i * 2] = u[digest[i] >> 4];
        hex[i * 2 + 1] = u[digest[i] & 0x0f];
    }
    hex[64] = '\0';
    return hex;
}

/* Cartella relativa di un file rispetto alla root del vault ("." → "/"). */
static void sb_folder_of(const char* root, const char* full, char* out, size_t cap)
{
    size_t rl = strlen(root);
    const char* rel = full;
    if (strncmp(full, root, rl) == 0 && (full[rl] == '/' || full[rl] == '\0'))
        rel = full + rl + (full[rl] == '/' ? 1 : 0);
    const char* slash = strrchr(rel, '/');
    if (!slash) { snprintf(out, cap, "/"); return; }
    size_t n = (size_t)(slash - rel);
    if (n >= cap) n = cap - 1;
    memcpy(out, rel, n);
    out[n] = '\0';
    if (n == 0) snprintf(out, cap, "/");
}

/* Registra un vault (root+pattern) e lo indicizza subito. Upsert per nome. Ritorna l'id o -1
 * (nome/path vuoti o root inesistente). Parità VaultManager.VaultCreate. */
/* mkdir -p best-effort: crea la directory e i livelli mancanti. */
static void sb_mkdir_p(const char* path)
{
    if (!path || !*path) return;
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* q = tmp + 1; *q; q++)
        if (*q == '/') { *q = '\0'; mkdir(tmp, 0777); *q = '/'; }
    mkdir(tmp, 0777);
}

long long gmb_vault_create(const char* name, const char* root_path, const char* pattern)
{
    if (!name || !*name || !root_path || !*root_path) return -1;
    /* PARITÀ IL — due divergenze trovate dal MiniTest con vault POPOLATO (2026-09-15):
     * 1) il C# fa `Path.GetFullPath(rootPath)` e memorizza il path ASSOLUTO; qui si
     *    memorizzava il path relativo → `sb_resolve` componeva path relativi e le note
     *    non si ritrovavano (sintomo: nodes:[] e NOTEGET vuoto sulla gamba VM/C, mentre IL
     *    funzionava). Ora si rende assoluto rispetto alla CWD del processo.
     * 2) il C# esige che la directory esista (il programma BASIC la crea con MKDIR), ma il
     *    MKDIR del BASIC può risolvere su una base diversa dalla CWD del processo C → CREATE
     *    falliva. Qui la directory (con i livelli intermedi) viene creata dal runtime. */
    char full[2048];
    if (root_path[0] == '/') snprintf(full, sizeof(full), "%s", root_path);
    else {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) snprintf(full, sizeof(full), "%s/%s", cwd, root_path);
        else snprintf(full, sizeof(full), "%s", root_path);
    }
    size_t fl = strlen(full);
    while (fl > 1 && full[fl - 1] == '/') full[--fl] = '\0';
    sb_mkdir_p(full);
    struct stat st;
    if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    const char* pat = (pattern && *pattern) ? pattern : "*.md";
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long id = -1;
    sqlite3_stmt* ins = NULL;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO vaults(name,root_path,pattern) VALUES($1,$2,$3)"
            " ON CONFLICT(name) DO UPDATE SET root_path=$2, pattern=$3;",
            -1, &ins, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(ins, 1, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 2, full, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 3, pat, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(ins);
        sqlite3_finalize(ins);
        if (rc == SQLITE_DONE)
        {
            sqlite3_stmt* sel = NULL;
            if (sqlite3_prepare_v2(db, "SELECT id FROM vaults WHERE name=$1;", -1, &sel, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(sel, 1, name, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(sel) == SQLITE_ROW) id = sqlite3_column_int64(sel, 0);
                sqlite3_finalize(sel);
            }
        }
    }
    if (id < 0) { sqlite3_close(db); return -1; }
    snprintf(g_sb_active_vault, sizeof(g_sb_active_vault), "%s", name);
    /* re-indicizza subito (come VaultCreate C#) */
    gmb_vault_index(name);
    sqlite3_close(db);
    return id;
}

/* Glob match BASIC minimale: '*' e '?' (parità gmb_glob_match in gmb_file.c). */
static int sb_glob_matches(const char* pat, const char* name);

/* Walk ricorsivo di index: inserisce/aggiorna vault_files per ogni file che matcha il pattern
 * e rimuove i file spariti. Ritorna il numero indicizzato. Chiamata con db aperto. */
static long sb_index_walk(sqlite3* db, long long vid, const char* root, const char* pattern,
                          const char* rel, char*** seen, long* seen_n, long* seen_cap)
{
    char dirpath[2048];
    if (rel && *rel) snprintf(dirpath, sizeof(dirpath), "%s/%s", root, rel);
    else snprintf(dirpath, sizeof(dirpath), "%s", root);
    DIR* d = opendir(dirpath);
    if (!d) return 0;
    long count = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL)
    {
        /* skip SOLO . e .. — i dotfile (.github/..., .pre-commit...) VANNO inclusi:
         * parità con Directory.GetFiles(..., AllDirectories) del C# (fix 2026-09-06,
         * benchmark 27.8k: C contava 28059 vs C# 28065 — mancavano i dotfile). */
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", dirpath, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode))
        {
            char sub[2048];
            if (rel && *rel) snprintf(sub, sizeof(sub), "%s/%s", rel, e->d_name);
            else snprintf(sub, sizeof(sub), "%s", e->d_name);
            count += sb_index_walk(db, vid, root, pattern, sub, seen, seen_n, seen_cap);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        if (!sb_glob_matches(pattern, e->d_name)) continue;
        char* hash = sb_file_hash_upper(full);
        if (!hash) continue;
        /* traccia i path visti per lo stale-removal */
        if (*seen_n >= *seen_cap)
        {
            *seen_cap = *seen_cap ? *seen_cap * 2 : 64;
            char** ns = (char**)realloc(*seen, (size_t)(*seen_cap) * sizeof(char*));
            if (!ns) { free(hash); continue; }
            *seen = ns;
        }
        (*seen)[(*seen_n)++] = strdup(full);
        char folder[1024];
        sb_folder_of(root, full, folder, sizeof(folder));
        sqlite3_stmt* ins = NULL;
        if (sqlite3_prepare_v2(db,
                "INSERT INTO vault_files(vault_id,path,folder_path,content_hash) VALUES($1,$2,$3,$4)"
                " ON CONFLICT(vault_id,path) DO UPDATE SET content_hash=$4, indexed_at=datetime('now');",
                -1, &ins, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64(ins, 1, vid);
            sqlite3_bind_text(ins, 2, full, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 3, folder, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 4, hash, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(ins) == SQLITE_DONE) count++;
            sqlite3_finalize(ins);
        }
        free(hash);
    }
    closedir(d);
    return count;
}

/* Glob match BASIC minimale: '*' e '?' (parità gmb_glob_match in gmb_file.c). */
static int sb_glob_matches(const char* pat, const char* name)
{
    while (*pat)
    {
        if (*pat == '*')
        {
            while (*pat == '*') pat++;
            if (!*pat) return 1;
            for (; *name; name++) if (sb_glob_matches(pat, name)) return 1;
            return 0;
        }
        if (!*name) return 0;
        if (*pat != '?' && *pat != *name) return 0;
        pat++; name++;
    }
    return !*name;
}

/* Re-indicizza un vault per nome. Ritorna il numero di file, -1 se vault ignoto.
 * Parità VaultManager.VaultIndex. */
long long gmb_vault_index(const char* name)
{
    if (!name) return -1;
    sqlite3* db = sb_open();
    if (!db) return -1;
    char root[2048] = "", pattern[256] = "";
    long long vid = -1;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id,root_path,pattern FROM vaults WHERE name=$1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW)
        {
            vid = sqlite3_column_int64(st, 0);
            snprintf(root, sizeof(root), "%s", (const char*)sqlite3_column_text(st, 1));
            snprintf(pattern, sizeof(pattern), "%s", (const char*)sqlite3_column_text(st, 2));
        }
        sqlite3_finalize(st);
    }
    if (vid < 0) { sqlite3_close(db); return -1; }
    if (!root[0] || !pattern[0]) { sqlite3_close(db); return -1; }
    char** seen = NULL; long seen_n = 0, seen_cap = 0;
    long count = sb_index_walk(db, vid, root, pattern, NULL, &seen, &seen_n, &seen_cap);
    /* stale removal: path in vault_files non più visti sul disco */
    sqlite3_stmt* sel = NULL;
    char** stale = NULL; long stale_n = 0, stale_cap = 0;
    if (sqlite3_prepare_v2(db, "SELECT path FROM vault_files WHERE vault_id=$1;", -1, &sel, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(sel, 1, vid);
        while (sqlite3_step(sel) == SQLITE_ROW)
        {
            const char* p = (const char*)sqlite3_column_text(sel, 0);
            if (!p) continue;
            int found = 0;
            for (long i = 0; i < seen_n; i++)
                if (seen[i] && strcmp(seen[i], p) == 0) { found = 1; break; }
            if (!found)
            {
                if (stale_n >= stale_cap)
                {
                    stale_cap = stale_cap ? stale_cap * 2 : 16;
                    char** ns = (char**)realloc(stale, (size_t)stale_cap * sizeof(char*));
                    if (!ns) break;
                    stale = ns;
                }
                stale[stale_n++] = strdup(p);
            }
        }
        sqlite3_finalize(sel);
    }
    for (long i = 0; i < stale_n; i++)
    {
        sqlite3_stmt* del = NULL;
        if (sqlite3_prepare_v2(db, "DELETE FROM vault_files WHERE vault_id=$1 AND path=$2;", -1, &del, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64(del, 1, vid);
            sqlite3_bind_text(del, 2, stale[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
        free(stale[i]);
    }
    free(stale);
    for (long i = 0; i < seen_n; i++) free(seen[i]);
    free(seen);
    sqlite3_stmt* upd = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE vaults SET indexed_at=datetime('now'), index_version=index_version+1 WHERE id=$1;", -1, &upd, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(upd, 1, vid);
        sqlite3_step(upd);
        sqlite3_finalize(upd);
    }
    sqlite3_close(db);
    return count;
}

/* Reindicizza UN file del vault (upsert hash). File esistente → 1; sparito → rimuove la voce
 * stale e ritorna 0; vault ignoto → -1. Parità VaultManager.IndexFile. */
long long gmb_vault_index_file(const char* name, const char* file_path)
{
    if (!name || !file_path) return -1;
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long vid = sb_vault_id(db, name);
    if (vid < 0) { sqlite3_close(db); return -1; }
    char full[2048];
    snprintf(full, sizeof(full), "%s", file_path);
    struct stat st;
    if (stat(full, &st) == 0 && S_ISREG(st.st_mode))
    {
        char root[2048] = "";
        sqlite3_stmt* rs = NULL;
        if (sqlite3_prepare_v2(db, "SELECT root_path FROM vaults WHERE id=$1;", -1, &rs, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64(rs, 1, vid);
            if (sqlite3_step(rs) == SQLITE_ROW) snprintf(root, sizeof(root), "%s", (const char*)sqlite3_column_text(rs, 0));
            sqlite3_finalize(rs);
        }
        char* hash = sb_file_hash_upper(full);
        char folder[1024] = "/";
        if (root[0]) sb_folder_of(root, full, folder, sizeof(folder));
        sqlite3_stmt* ins = NULL;
        if (sqlite3_prepare_v2(db,
                "INSERT INTO vault_files(vault_id,path,folder_path,content_hash) VALUES($1,$2,$3,$4)"
                " ON CONFLICT(vault_id,path) DO UPDATE SET content_hash=$4, indexed_at=datetime('now');",
                -1, &ins, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64(ins, 1, vid);
            sqlite3_bind_text(ins, 2, full, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 3, folder, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 4, hash ? hash : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(ins);
            sqlite3_finalize(ins);
        }
        if (hash) free(hash);
        sqlite3_close(db);
        return 1;
    }
    /* file sparito → rimuovi voce stale */
    sqlite3_stmt* del = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM vault_files WHERE vault_id=$1 AND path=$2;", -1, &del, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(del, 1, vid);
        sqlite3_bind_text(del, 2, full, -1, SQLITE_TRANSIENT);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }
    sqlite3_close(db);
    return 0;
}

/* Rimuove la voce di un file dall'indice. Ritorna righe cancellate o -1 se vault ignoto.
 * Parità VaultManager.RemoveFile. */
long long gmb_vault_remove_file(const char* name, const char* file_path)
{
    if (!name || !file_path) return -1;
    sqlite3* db = sb_open();
    if (!db) return -1;
    long long vid = sb_vault_id(db, name);
    if (vid < 0) { sqlite3_close(db); return -1; }
    long long n = 0;
    sqlite3_stmt* del = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM vault_files WHERE vault_id=$1 AND path=$2;", -1, &del, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(del, 1, vid);
        sqlite3_bind_text(del, 2, file_path, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(del) == SQLITE_DONE) n = sqlite3_changes(db);
        sqlite3_finalize(del);
    }
    sqlite3_close(db);
    return n;
}

/* Note: path risolto contro la root del vault (come NoteManager). */

static int sb_write_file(const char* path, const char* content) {
    char dir[1024]; snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/'); if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fputs(content ? content : "", f);
    fclose(f);
    return 1;
}

static int sb_read_file(const char* path, char* out, size_t cap) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
    return 1;
}

static char* sb_note_body(const char* content) {
    /* estrae il body dopo il frontmatter --- ... --- (se presente) */
    if (!content) return NULL;
    if (strncmp(content, "---", 3) == 0) {
        const char* end = strstr(content + 3, "---");
        if (end) { const char* body = end + 3; while (*body == '\n') body++; return strdup(body); }
    }
    return strdup(content);
}

static const char* sb_field(const char* content, const char* key) {
    /* frontmatter: chiave: valore (prima riga che inizia con key) */
    if (!content) return NULL;
    size_t kl = strlen(key);
    const char* p = content;
    if (strncmp(content, "---", 3) != 0) return NULL;
    const char* end = strstr(content + 3, "---");
    if (!end) return NULL;
    char* tmp = strndup(content + 3, (size_t)(end - content - 3));
    char* line = strtok(tmp, "\n");
    while (line) {
        char* colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            if (strcasecmp(line, key) == 0) {
                char* v = colon + 1; while (*v == ' ') v++;
                static char buf[256]; snprintf(buf, sizeof(buf), "%s", v);
                free(tmp); return buf;
            }
        }
        line = strtok(NULL, "\n");
    }
    free(tmp);
    return NULL;
}

int gmb_note_create(const char* name, const char* path, const char* content, const char* tags) {
    sqlite3* db = sb_open(); if (!db) return -1;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return -1; }
    sqlite3_close(db);
    char head[1024] = "";
    /* Frontmatter COME IL (NoteManager.EnsureFrontmatter) — divergenza trovata dal MiniTest
     * ai_linkjson_graph: IL scrive `title: "<nome-senza-estensione>"` (o il primo H1 del body)
     * e i tag come lista DI STRINGHE QUOTATE `tags: ["a", "b"]`; qui si scriveva il nome file
     * CON estensione, senza virgolette, e i tag separati da spazio → `frontmatter` divergente.
     * Il frontmatter si aggiunge SEMPRE (anche senza tag), come IL. */
    {
        const char* c0 = content ? content : "";
        if (!(c0[0] == '-' && c0[1] == '-' && c0[2] == '-'))
        {
            char title[256] = "";
            if (c0[0] == '#' && c0[1] == ' ') {                 /* primo H1 del body */
                size_t k = 0;
                const char* q = c0 + 2;
                while (*q && *q != '\n' && k < sizeof(title) - 1) title[k++] = *q++;
                title[k] = '\0';
            }
            if (!title[0]) {                                     /* fallback: nome file senza ext */
                const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
                snprintf(title, sizeof(title), "%s", base);
                char* dot = strrchr(title, '.'); if (dot) *dot = '\0';
            }
            int o = snprintf(head, sizeof(head), "---\ntitle: \"%s\"\n", title);
            if (tags && *tags)
            {
                char tmp[256]; snprintf(tmp, sizeof(tmp), "%s", tags);
                o += snprintf(head + o, sizeof(head) - (size_t)o, "tags: [");
                char* tok = strtok(tmp, ",");
                int first = 1;
                while (tok)
                {
                    char* t = tok; while (*t == ' ') t++;
                    size_t tl = strlen(t); while (tl && t[tl - 1] == ' ') t[--tl] = '\0';
                    if (o < (int)sizeof(head) - 8)
                        o += snprintf(head + o, sizeof(head) - (size_t)o, "%s\"%s\"", first ? "" : ", ", t);
                    first = 0;
                    tok = strtok(NULL, ",");
                }
                o += snprintf(head + o, sizeof(head) - (size_t)o, "]\n");
            }
            snprintf(head + o, sizeof(head) - (size_t)o, "---\n");
        }
    }
    char full_content[8192];
    if (*head) snprintf(full_content, sizeof(full_content), "%s%s", head, content ? content : "");
    else snprintf(full_content, sizeof(full_content), "%s", content ? content : "");
    return sb_write_file(full, full_content) ? 1 : 0;
}

int gmb_note_update(const char* name, const char* path, const char* content) {
    sqlite3* db = sb_open(); if (!db) return -1;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return -1; }
    sqlite3_close(db);
    /* Parità C# NoteManager.UpdateByPath → RebuildWithFrontmatter: il frontmatter ESISTENTE
       viene preservato e si sostituisce solo il body. Prima si scriveva il contenuto grezzo →
       i tag/title del frontmatter andavano PERSI sulla gamba C (divergenza trovata dal MiniTest
       note_crud_commands: `tags2:` vuoto su VM/C, `alpha,beta` su IL/PCode). */
    char old[65536];
    if (!sb_read_file(full, old, sizeof(old)))
        return sb_write_file(full, content) ? 1 : 0;
    if (strncmp(old, "---", 3) == 0) {
        const char* end = strstr(old + 3, "---");
        if (end) {
            size_t fmLen = (size_t)(end - old) + 3; /* include il `---` di chiusura */
            char newbuf[8192];
            if (fmLen < sizeof(newbuf) - 1) {
                memcpy(newbuf, old, fmLen);
                newbuf[fmLen] = '\0';
                snprintf(newbuf + fmLen, sizeof(newbuf) - fmLen, "\n%s", content ? content : "");
                return sb_write_file(full, newbuf) ? 1 : 0;
            }
        }
    }
    return sb_write_file(full, content) ? 1 : 0;
}

int gmb_note_append(const char* name, const char* path, const char* text) {
    sqlite3* db = sb_open(); if (!db) return -1;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return -1; }
    sqlite3_close(db);
    FILE* f = fopen(full, "ab");
    if (!f) return 0;
    /* Parità C# NoteManager.UpdateAppend: `existing.Content + append` — NESSUN newline aggiunto
     * (il vecchio `fputs("\n")` creava una riga vuota in coda → divergenza di output con IL/PCode
     * e blank-line spurio in `AI.NOTEGET`). */
    fputs(text ? text : "", f);
    fclose(f);
    return 1;
}

int gmb_note_delete(const char* name, const char* path) {
    sqlite3* db = sb_open(); if (!db) return -1;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return -1; }
    sqlite3_close(db);
    return remove(full) == 0 ? 1 : 0;
}

int gmb_note_move(const char* name, const char* old_path, const char* new_path) {
    sqlite3* db = sb_open(); if (!db) return -1;
    char src[2048], dst[2048];
    if (!sb_resolve(db, name, old_path, src, sizeof(src))) { sqlite3_close(db); return -1; }
    if (!sb_resolve(db, name, new_path, dst, sizeof(dst))) { sqlite3_close(db); return -1; }
    sqlite3_close(db);
    char dir[2048]; snprintf(dir, sizeof(dir), "%s", dst);
    char* slash = strrchr(dir, '/'); if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    return rename(src, dst) == 0 ? 1 : 0;
}

char* gmb_note_get(const char* name, const char* path) {
    sqlite3* db = sb_open(); if (!db) return NULL;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return NULL; }
    sqlite3_close(db);
    char* buf = (char*)malloc(65536);
    if (!buf) return NULL;
    if (!sb_read_file(full, buf, 65536)) { free(buf); return NULL; }
    char* body = sb_note_body(buf);
    free(buf);
    return body;
}

char* gmb_note_title(const char* name, const char* path) {
    sqlite3* db = sb_open(); if (!db) return NULL;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return NULL; }
    sqlite3_close(db);
    char buf[65536];
    if (!sb_read_file(full, buf, sizeof(buf))) return NULL;
    /* Parità C# NoteManager.FromText → ExtractTitle(fm.Body) ?? nome-file-senza-ext:
       il titolo è il primo H1 (`# …`) del BODY, altrimenti il nome file senza estensione.
       Il campo `title` del FRONTMATTER NON viene usato da GetTitle (divergenza trovata dal
       sample AiNoteVault: il C ritornava il valore YAML grezzo con le virgolette, `"intro"`,
       mentre IL restituiva `intro`). */
    char* body = sb_note_body(buf);
    if (body) {
        const char* p = body;
        while (*p) {
            const char* eol = strchr(p, '\n');
            size_t ll = eol ? (size_t)(eol - p) : strlen(p);
            size_t i = 0;
            while (i < ll && (p[i] == ' ' || p[i] == '\t' || p[i] == '\r')) i++;
            if (i + 1 < ll && p[i] == '#' && p[i + 1] == ' ') {
                size_t s = i + 2, e = ll;
                while (s < e && (p[s] == ' ' || p[s] == '\t')) s++;
                while (e > s && (p[e - 1] == ' ' || p[e - 1] == '\t' || p[e - 1] == '\r')) e--;
                char* out = strndup(p + s, e - s);
                free(body);
                return out;
            }
            if (!eol) break;
            p = eol + 1;
        }
        free(body);
    }
    const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
    char* out = strdup(base);
    char* dot = strrchr(out, '.'); if (dot) *dot = '\0';
    return out;
}

char* gmb_note_tags(const char* name, const char* path) {
    sqlite3* db = sb_open(); if (!db) return NULL;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return NULL; }
    sqlite3_close(db);
    char buf[65536];
    if (!sb_read_file(full, buf, sizeof(buf))) return NULL;
    const char* t = sb_field(buf, "tags");
    if (!t) return strdup("");
    /* Parità C# FrontmatterParser.ParseFields (lista inline `[a, b]`) + NoteManager.GetTags
       (join ","): split SOLO su ',' e trim di spazi e virgolette su ogni voce. Il vecchio
       approccio (strip di prefissi/suffissi + strtok su ", ") lasciava le virgolette interne
       (`alpha","beta` invece di `alpha,beta`) — divergenza trovata dal sample AiNoteVault. */
    char tmp[2048]; snprintf(tmp, sizeof(tmp), "%s", t);
    char* s = tmp;
    while (*s == ' ' || *s == '\t') s++;
    size_t L = strlen(s);
    if (L >= 2 && s[0] == '[' && s[L - 1] == ']') { s[L - 1] = '\0'; s++; }
    char out[2048]; out[0] = '\0'; int first = 1;
    for (char* tok = strtok(s, ","); tok; tok = strtok(NULL, ",")) {
        char* a = tok;
        while (*a == ' ' || *a == '\t') a++;
        size_t tl = strlen(a);
        while (tl > 0 && (a[tl - 1] == ' ' || a[tl - 1] == '\t' || a[tl - 1] == '"' || a[tl - 1] == '\'')) a[--tl] = '\0';
        while (*a == '"' || *a == '\'') a++;
        if (!*a) continue;
        if (!first) strcat(out, ",");
        strcat(out, a); first = 0;
    }
    return strdup(out);
}

long gmb_note_wordcount(const char* name, const char* path) {
    sqlite3* db = sb_open(); if (!db) return -1;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return -1; }
    sqlite3_close(db);
    char buf[65536];
    if (!sb_read_file(full, buf, sizeof(buf))) return 0;
    /* C# NoteManager.CountWords: solo il BODY (frontmatter escluso). */
    char* body = sb_note_body(buf);
    long w = 0; int in = 0;
    for (const char* p = body; *p; p++) {
        if (*p == ' ' || *p == '\n' || *p == '\t') in = 0;
        else if (!in) { in = 1; w++; }
    }
    free(body);
    return w;
}

int gmb_note_exists(const char* name, const char* path) {
    sqlite3* db = sb_open(); if (!db) return 0;
    char full[2048];
    if (!sb_resolve(db, name, path, full, sizeof(full))) { sqlite3_close(db); return 0; }
    sqlite3_close(db);
    struct stat st;
    return stat(full, &st) == 0 && S_ISREG(st.st_mode) ? 1 : 0;
}

/* ── LLM completion (Enrich/QA/BrainVaultWrite): wrapper sul provider/agente ──
 * Il runtime C non ha un provider LLM nudo: usa gmb_agent_ask (fallback fixture,
 * coerente con IL/PCode senza provider — mai stub silenzioso). */
char* gmb_llm_complete(const char* system, const char* prompt) {
    /* Parità IL (Enricher.NoCompleter / BrainVault.cs:133): senza un provider di
     * completamento ATTIVO il completamento NON deve produrre il testo fixture ma
     * l'errore esplicito. La semantica era già documentata (gmb_docfields.c:8:
     * "provider %CHAT attivo; senza provider o senza testo = errore esplicito").
     * Con un provider registrato (es. kind "fixture") il comportamento resta quello
     * di prima: fallback sul path agente/fixture. */
    if (!(prompt && *prompt)) return strdup("");
    {
        const char* prov = gmb_provider_active("completion");
        /* Un provider di kind "fixture" NON e' un provider LLM reale: stessa semantica degli
         * handler VM/C gmb_bf_secondbrain_c1324/c1331 (parita' IL: Enricher.NoCompleter). */
        if (!prov || !*prov || strcmp(gmb_provider_kind(prov), "fixture") == 0)
            return strdup("Error: nessun provider LLM configurato (AI SET ACTIVE)");
    }
    char* full = (char*)malloc((system ? strlen(system) : 0) + (prompt ? strlen(prompt) : 0) + 3);
    if (!full) return NULL;
    snprintf(full, (system ? strlen(system) : 0) + (prompt ? strlen(prompt) : 0) + 3,
             "%s\n%s", system ? system : "", prompt ? prompt : "");
    char* r = gmb_agent_ask("_llm", full);
    free(full);
    return r;
}

/* ── ENRICH (parità Basic.AI.Knowledge.Enricher.Enrich, porting webapp 2026-09-30) ──
 * I TRE prompt (system, user) vivono qui e non nel layer HTTP: la logica è di dominio
 * (§0bis, una sola sorgente). `mode` è la stringa grezza della rotta; il mapping C# è
 * "SUMMARIZATION" → Summarization, "TAGGING" → Tagging, tutto il resto → EntityExtraction.
 * Truncate: i primi 8000 caratteri + "…" (C# `text[..8000] + "…"`).
 * Testo vuoto → "" ; senza completer → gmb_llm_complete ritorna già l'errore esplicito
 * ("Error: nessun provider LLM configurato (AI SET ACTIVE)"). Ritorno malloc. */
static char* gmb_truncate_utf8(const char* text, size_t maxChars) {
    size_t n = text ? strlen(text) : 0;
    if (n <= maxChars) return strdup(text ? text : "");
    /* taglia a maxChars caratteri, NON byte: gli UTF-8 multibyte non vanno spezzati. */
    size_t cut = 0, chars = 0;
    while (cut < n && chars < maxChars) {
        unsigned char c = (unsigned char)text[cut];
        size_t adv = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        if (cut + adv > n) adv = 1;
        cut += adv;
        chars++;
    }
    char* out = (char*)malloc(cut + 4);
    if (!out) return NULL;
    memcpy(out, text, cut);
    memcpy(out + cut, "\xE2\x80\xA6", 3);  /* "…" */
    out[cut + 3] = '\0';
    return out;
}

char* gmb_enrich(const char* text, const char* mode) {
    if (!text || !*text) return strdup("");
    const char* m = mode ? mode : "";
    const char* system;
    char* user;
    int tagging = 0, entities = 0;
    if (strcasecmp(m, "SUMMARIZATION") == 0) {
        system = "Sei un assistente che sintetizza note di un vault di conoscenza. "
                 "Produci SOLO il riassunto, senza premesse, senza citazioni.";
        tagging = 0; entities = 0;
    } else if (strcasecmp(m, "TAGGING") == 0) {
        system = "Sei un classificatore di note. Ritorna SOLO una lista di tag separati da virgola, "
                 "in minuscolo, senza altri testo.";
        tagging = 1; entities = 0;
    } else {
        system = "Sei un estrattore di entita'. Ritorna SOLO un JSON con chiavi PERSON, ORG, DATE, "
                 "LOC e PRODUCT, ciascuna una lista di stringhe (vuota se assente).";
        tagging = 0; entities = 1;
    }
    char* cut = gmb_truncate_utf8(text, 8000);
    if (!cut) return NULL;
    if (tagging) {
        size_t n = strlen(cut) + 96;
        user = (char*)malloc(n);
        if (user) snprintf(user, n, "Suggerisci 3-8 tag per questa nota (argomenti chiave, concetti, domini):\n\n%s", cut);
    } else if (entities) {
        size_t n = strlen(cut) + 64;
        user = (char*)malloc(n);
        if (user) snprintf(user, n, "Estrai le entita' dal seguente testo:\n\n%s", cut);
    } else {
        size_t n = strlen(cut) + 96;
        user = (char*)malloc(n);
        if (user) snprintf(user, n, "Sintetizza il seguente testo in un riassunto conciso (max ~100 parole):\n\n%s", cut);
    }
    free(cut);
    if (!user) return NULL;
    char* r = gmb_llm_complete(system, user);
    free(user);
    if (!r) return NULL;
    /* .Trim() del C# (spazi bianchi iniziali/finali). */
    char* s = r;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    *e = '\0';
    if (s != r) {
        char* out = strdup(s);
        free(r);
        return out;
    }
    return r;
}

/* ── AI.QAANSWER (parità Enricher.QaAnswer) ─────────────────────────────────────────
 * Ordine dei controlli IDENTICO all'oracolo C#: domanda vuota → brain non disponibile come
 * rag → evidenza insufficiente → errore provider → JSON Success. Il brain è "disponibile come
 * rag" se il registry (config/brains.json) lo registra con mode rag e la sua "source" esiste,
 * oppure (fallback, stessa regola del C# per "brainvault") se esiste <ragDir>/<brain>.db. */
static int sb_brain_rag_db_path(const char* brain, char* out, size_t cap) {
    out[0] = 0;
    if (!brain || !*brain) return 0;
    /* PRIMA la risoluzione dei cervelli (cache ~/.gmbasic/brains, vendor, exe): è dove
     * `AI BRAIN INSTALL` mette il cervello. I path sotto sono i fallback storici. */
    if (gmb_brain_rag_db(brain, out, cap)) return 1;
    char json[1024]; gmb_paths_brains_json(json, sizeof(json));
    FILE* f = fopen(json, "r");
    if (f) {
        char* buf = (char*)malloc(262144);
        if (buf) {
            size_t len = fread(buf, 1, 262143, f); buf[len] = 0;
            char pat[128]; snprintf(pat, sizeof(pat), "\"%s\"", brain);
            for (char* p = strstr(buf, pat); p && !out[0]; p = strstr(p + strlen(pat), pat)) {
                char* end = strchr(p, '}');
                if (!end) end = buf + len;
                if (!strstr(p, "\"rag\"")) continue;
                char* src = strstr(p, "\"source\"");
                if (!src || src > end) continue;
                char* c1 = strchr(src, ':'); if (!c1) continue;
                char* a1 = strchr(c1, '"'); if (!a1 || a1 > end) continue;
                char* b1 = strchr(a1 + 1, '"'); if (!b1 || b1 > end) continue;
                size_t l = (size_t)(b1 - a1 - 1);
                if (l && l < cap - 1) {
                    memcpy(out, a1 + 1, l); out[l] = 0;
                    /* un `source` stantio (il db non è più lì) non deve bloccare i fallback */
                    struct stat st0;
                    if (stat(out, &st0) != 0 || !S_ISREG(st0.st_mode)) out[0] = 0;
                }
            }
            free(buf);
        }
        fclose(f);
    }
    /* NIENTE fallback su <ragDir>/<brain>.db: o il cervello si risolve (cache/vendor/exe/brainsDir)
     * o e' registrato con un `source` CHE ESISTE, altrimenti non e' disponibile e si genera un
     * errore esplicito (decisione utente, 2026-10-31). */
    struct stat st;
    if (!out[0] || stat(out, &st) != 0 || !S_ISREG(st.st_mode)) { out[0] = 0; return 0; }
    return 1;
}

/* Escape JSON minimale (le librerie C non ne espongono uno condiviso). */
static void sb_json_escape(const char* s, char* out, size_t cap) {
    size_t o = 0;
    for (const unsigned char* p = (const unsigned char*)(s ? s : ""); *p && o + 7 < cap; p++) {
        switch (*p) {
            case '"':  out[o++] = '\\'; out[o++] = '"';  break;
            case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
            case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
            case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
            case '\t': out[o++] = '\\'; out[o++] = 't';  break;
            default:
                if (*p < 0x20) { o += (size_t)snprintf(out + o, cap - o, "\\u%04x", *p); }
                else out[o++] = (char)*p;
        }
    }
    out[o] = 0;
}

char* gmb_qa_answer(const char* question, const char* brain, int window) {
    if (!question || !*question) return strdup("{\"status\":\"Error\",\"error\":\"domanda vuota\"}");
    if (window <= 0) window = 3;
    char db[1536];
    if (!sb_brain_rag_db_path(brain, db, sizeof(db))) {
        char m[512];
        snprintf(m, sizeof(m), "{\"status\":\"Error\",\"error\":\"brain '%s' non disponibile come rag\"}", brain ? brain : "");
        return strdup(m);
    }
    char* ans = gmb_rag_ask(db, "wiki", question, window, NULL);
    if (!ans) return strdup("{\"status\":\"Error\",\"error\":\"Error: nessun provider LLM configurato (AI SET ACTIVE)\"}");
    /* Oracolo: senza completer -> errore provider dentro il JSON (Enricher.QaAnswer passo 5). */
    if (strncmp(ans, "Error: nessun provider", 22) == 0 || strncmp(ans, "Error: empty question", 21) == 0) {
        char m[512];
        snprintf(m, sizeof(m), "{\"status\":\"Error\",\"error\":\"%s\"}", ans);
        free(ans);
        return strdup(m);
    }
    if (strncmp(ans, "[insufficient evidence]", 23) == 0) {
        free(ans);
        return strdup("{\"status\":\"InsufficientEvidence\",\"error\":\"nessun chunk rilevante\",\"citations\":[]}");
    }
    size_t cap = strlen(ans) * 2 + strlen(brain ? brain : "") * 2 + strlen(question) * 2 + 256;
    char* esc = (char*)malloc(strlen(ans) * 2 + 16);
    char* qesc = (char*)malloc(strlen(question) * 2 + 16);
    char* out = (char*)malloc(cap);
    if (!esc || !qesc || !out) { free(esc); free(qesc); free(out); free(ans); return NULL; }
    sb_json_escape(ans, esc, strlen(ans) * 2 + 16);
    sb_json_escape(question, qesc, strlen(question) * 2 + 16);
    snprintf(out, cap, "{\"status\":\"Success\",\"brain\":\"%s\",\"question\":\"%s\",\"answer\":\"%s\",\"citations\":[]}",
             brain ? brain : "", qesc, esc);
    free(esc); free(qesc); free(ans);
    return out;
}

/* ── Brain registry (AI.BRAINLIST/INFO): tabella brains su vaults.db ── */

/* Legge config/brains.json (stesso store del C# BrainRegistry) e ritorna i nomi.
 * which=0 -> union(brains, attached) CSV (BrainList); which=1 -> solo attached CSV (BrainUsed). */
static char* gmb_brain_names_from_json(int which) {
    char path[1024]; gmb_paths_brains_json(path, sizeof(path));
    static char names[256][64]; int n = 0;
    FILE* f = fopen(path, "r");
    if (f) {
        char* buf = (char*)malloc(65536);
        if (buf) {
            size_t len = fread(buf, 1, 65535, f); buf[len] = 0;
            char* att = strstr(buf, "\"attached\"");
            if (att) {
                char* lb = strchr(att, '['); char* rb = lb ? strchr(lb, ']') : NULL;
                if (lb && rb) { char* p = lb;
                    while ((p = strchr(p, '"')) && p < rb) { char* e = strchr(p + 1, '"'); if (!e || e > rb) break;
                        size_t l = (size_t)(e - p - 1); if (l < 64 && n < 256) { memcpy(names[n], p + 1, l); names[n][l] = 0; n++; } p = e + 1; } }
            }
            if (!which) { char* p = buf;
                while ((p = strstr(p, "\"name\""))) { char* c = strchr(p, ':'); if (!c) break; char* q = strchr(c, '"'); if (!q) break; char* e = strchr(q + 1, '"'); if (!e) break;
                    size_t l = (size_t)(e - q - 1); if (l < 64 && n < 256) { memcpy(names[n], q + 1, l); names[n][l] = 0; n++; } p = e + 1; } }
            free(buf);
        }
    }
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (names[i][0] && names[j][0] && strcasecmp(names[i], names[j]) == 0) names[j][0] = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (names[i][0] && names[j][0] && strcasecmp(names[i], names[j]) > 0) { char tmp[64]; memcpy(tmp, names[i], 64); memcpy(names[i], names[j], 64); memcpy(names[j], tmp, 64); }
    char* out = (char*)malloc(8192); out[0] = 0; int first = 1;
    for (int i = 0; i < n; i++) if (names[i][0]) { if (!first) strcat(out, ","); strcat(out, names[i]); first = 0; }
    return out;
}
/* Elenco dei cervelli: registry (`<configDir>/brains.json`) **PIÙ** quelli presenti su disco
 * (vendor / exe / brainsDir / cache ~/.gmbasic/brains). Il secondo insieme è indispensabile:
 * `AI BRAIN INSTALL` (e il package manager) registrano il cervello SU DISCO, non nel JSON del
 * C#, quindi senza la scansione un cervello appena installato non comparirebbe in AI.BRAINLIST. */
char* gmb_brain_registry_list(void)
{
    char* json = gmb_brain_names_from_json(0);
    char fs[8192];
    gmb_brain_list(fs, sizeof fs, NULL);           /* "nome@ver;nome2@ver" (risoluzione in C) */
    char names[320][128];
    int n = 0;
    for (char* p = json; p && *p && n < 320; ) {
        char* comma = strchr(p, ',');
        size_t l = comma ? (size_t)(comma - p) : strlen(p);
        if (l > 0 && l < sizeof names[0]) { memcpy(names[n], p, l); names[n][l] = 0; n++; }
        if (!comma) break;
        p = comma + 1;
    }
    for (char* p = fs; *p && n < 320; ) {
        char* semi = strchr(p, ';');
        size_t l = semi ? (size_t)(semi - p) : strlen(p);
        char item[128];
        if (l < sizeof item) {
            memcpy(item, p, l); item[l] = 0;
            char* at = strchr(item, '@');
            if (at) *at = 0;
            int dup = 0;
            for (int k = 0; k < n; k++) if (strcasecmp(names[k], item) == 0) { dup = 1; break; }
            if (!dup && item[0]) snprintf(names[n++], sizeof names[0], "%s", item);
        }
        if (!semi) break;
        p = semi + 1;
    }
    free(json);
    /* ordinamento stabile */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcasecmp(names[i], names[j]) > 0) {
                char tmp[128];
                snprintf(tmp, sizeof tmp, "%s", names[i]);
                snprintf(names[i], sizeof names[0], "%s", names[j]);
                snprintf(names[j], sizeof names[0], "%s", tmp);
            }
    char* out = (char*)malloc(16384);
    if (!out) return NULL;
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        if (i) strcat(out, ",");
        strcat(out, names[i]);
    }
    return out;
}
char* gmb_brain_used_csv_json(void) { return gmb_brain_names_from_json(1); }

char* gmb_brain_registry_info(const char* name) {
    if (!name) return NULL;
    sqlite3* db = sb_open(); if (!db) return NULL;
    char* info = gmb_vault_info_json(name);
    sqlite3_close(db);
    return info; /* riusa VaultInfo: rootPath+fileCount = info brain */
}

/* Parità C# BrainInfo: brain sconosciuto → "{}" (non NULL). Il raw resta NULL per i
 * chiamanti che distinguono l'assenza (repl `BRAIN INFO` → "?cervello non trovato"). */
char* gmb_brain_info_or_empty(const char* name) {
    char* r = gmb_brain_registry_info(name);
    if (r) return r;
    return strdup("{}");
}

/* ── WikiGraph loader (GRAPH query sui brain): JSON GraphExporter → gmb_graph handle ──
 * Il JSON ha {nodes:[{id,title,frontmatter}], edges:[{from,to,alias}], dangling, folderCounts}.
 * Ritorna l'handle del grafo (nodi=id, edge) o 0. Chi lo usa deve gmb_graph_free. */
/* ── GRAPH BUILD: scansiona un vault (.md/.markdown), estrae titolo + wikilink + tags e
 * scrive <outputDir>/<basename>.json (parità GraphStore.Build + GraphExporter.ToJson). ── */
static void sb_esc(const char* in, char* out, size_t cap);
/* Directory dell'ultimo GRAPH BUILD (parità GraphManager.ActiveDir): le funzioni GRAPH.*
 * vi cercano <name>.json PRIMA delle dir di default. */
static char g_graph_active_dir[2048] = "";
/* ActiveGraph (parità GraphManager.ActiveGraph): il RichGraph costruito dall'ultimo
 * GRAPH BUILD, usato dalle op single-brain (Neighbors/Stats/...). */
static int64_t g_active_graph_h = 0;
static char g_active_graph_name[256] = "";
/* Edge del RichGraph attivo in ORDINE DI INSERZIONE (parità GraphManager.Build / g.Edges):
 * servono a DOT/MST export (che iterano g.Edges, non il registry gmb_graph_*). */
typedef struct { char* from; char* to; } sb_gedge_t;
static sb_gedge_t* g_active_edges = NULL;
static int g_active_ne = 0;
static void sb_active_edge_add(const char* from, const char* to) {
    g_active_edges = (sb_gedge_t*)realloc(g_active_edges, (size_t)(g_active_ne + 1) * sizeof(sb_gedge_t));
    g_active_edges[g_active_ne].from = strdup(from ? from : "");
    g_active_edges[g_active_ne].to = strdup(to ? to : "");
    g_active_ne++;
}
static void sb_active_edges_clear(void) {
    for (int i = 0; i < g_active_ne; i++) { free(g_active_edges[i].from); free(g_active_edges[i].to); }
    free(g_active_edges); g_active_edges = NULL; g_active_ne = 0;
}
static double sb_date_days(const char* a, const char* b) {
    int ya = 0, ma = 0, da = 0, yb = 0, mb = 0, db = 0;
    if (sscanf(a, "%d-%d-%d", &ya, &ma, &da) != 3) return -1;
    if (sscanf(b, "%d-%d-%d", &yb, &mb, &db) != 3) return -1;
    struct tm ta; memset(&ta, 0, sizeof(ta)); ta.tm_year = ya - 1900; ta.tm_mon = ma - 1; ta.tm_mday = da;
    struct tm tb; memset(&tb, 0, sizeof(tb)); tb.tm_year = yb - 1900; tb.tm_mon = mb - 1; tb.tm_mday = db;
    double d = difftime(mktime(&ta), mktime(&tb)) / 86400.0;
    return d < 0 ? -d : d;
}
typedef struct { char* rel; char* title; char* folder; char** tags; int ntags; char* date; } sb_vnode_t;

static int sb_is_md_file(const char* name) {
    size_t l = strlen(name);
    if (l > 3 && strcasecmp(name + l - 3, ".md") == 0) return 1;
    if (l > 9 && strcasecmp(name + l - 9, ".markdown") == 0) return 1;
    return 0;
}
static char* sb_title_of(const char* rel) {
    const char* base = strrchr(rel, '/'); base = base ? base + 1 : rel;
    size_t bl = strlen(base);
    if (bl > 3 && strcasecmp(base + bl - 3, ".md") == 0) bl -= 3;
    else if (bl > 9 && strcasecmp(base + bl - 9, ".markdown") == 0) bl -= 9;
    char* t = (char*)malloc(bl + 1); memcpy(t, base, bl); t[bl] = 0;
    return t;
}
static void sb_collect_md(const char* root, const char* rel, sb_vnode_t** out, int* n, int* cap) {
    char full[4096];
    if (rel[0]) snprintf(full, sizeof(full), "%s/%s", root, rel);
    else snprintf(full, sizeof(full), "%s", root);
    DIR* d = opendir(full);
    if (!d) return;
    struct dirent* e;
    /* .NET Directory.GetFiles(AllDirectories): PRIMA i file della dir corrente, POI
     * ricorre nelle sottodirectory (ordine readdir) — l'ordine dei nodi/edge deve
     * combaciare col C# per la parità di GRAPH.QUERYMULTI (neighbor order). */
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (!sb_is_md_file(e->d_name)) continue;
        char childRel[4096], childFull[4096];
        if (rel[0]) snprintf(childRel, sizeof(childRel), "%s/%s", rel, e->d_name);
        else snprintf(childRel, sizeof(childRel), "%s", e->d_name);
        snprintf(childFull, sizeof(childFull), "%s/%s", full, e->d_name);
        struct stat st;
        if (stat(childFull, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (*n >= *cap) { *cap = (*cap ? *cap * 2 : 64); *out = (sb_vnode_t*)realloc(*out, (size_t)*cap * sizeof(sb_vnode_t)); }
        (*out)[*n].rel = strdup(childRel);
        (*out)[*n].title = sb_title_of(childRel);
        (*out)[*n].tags = NULL; (*out)[*n].ntags = 0; (*out)[*n].date = NULL;
        { const char* sl = strrchr(childRel, '/'); if (sl) { size_t dl = (size_t)(sl - childRel); (*out)[*n].folder = (char*)malloc(dl + 1); memcpy((*out)[*n].folder, childRel, dl); (*out)[*n].folder[dl] = 0; } else (*out)[*n].folder = strdup(""); }
        (*n)++;
    }
    rewinddir(d);
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char childRel[4096], childFull[4096];
        if (rel[0]) snprintf(childRel, sizeof(childRel), "%s/%s", rel, e->d_name);
        else snprintf(childRel, sizeof(childRel), "%s", e->d_name);
        snprintf(childFull, sizeof(childFull), "%s/%s", full, e->d_name);
        struct stat st;
        if (stat(childFull, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        sb_collect_md(root, childRel, out, n, cap);
    }
    closedir(d);
}
static int sb_cmp_vnode(const void* a, const void* b) {
    return strcmp(((const sb_vnode_t*)a)->rel, ((const sb_vnode_t*)b)->rel);
}
static void sb_tags_add(char*** out, int* nout, const char* s, size_t len) {
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '"' || s[len-1] == '\'')) len--;
    if (len == 0) return;
    char* tag = (char*)malloc(len + 1); memcpy(tag, s, len); tag[len] = 0;
    *out = (char**)realloc(*out, (size_t)(*nout + 1) * sizeof(char*));
    (*out)[(*nout)++] = tag;
}
static void sb_parse_tags(const char* text, char*** out, int* nout) {
    *out = NULL; *nout = 0;
    if (strncmp(text, "---", 3) != 0) return;
    const char* p = strchr(text, '\n'); if (!p) return;
    const char* fmStart = p + 1;
    const char* fmEnd = strstr(fmStart, "\n---");
    if (!fmEnd) return;
    const char* t = fmStart;
    while (t < fmEnd) {
        const char* lineEnd = (const char*)memchr(t, '\n', (size_t)(fmEnd - t)); if (!lineEnd) lineEnd = fmEnd;
        if (strncmp(t, "tags:", 5) == 0) {
            const char* v = t + 5;
            while (v < lineEnd && (*v == ' ' || *v == '\t')) v++;
            if (v < lineEnd && *v == '[') {
                v++;
                while (v < lineEnd && *v != ']') {
                    while (v < lineEnd && (*v == ' ' || *v == ',' || *v == '"' || *v == '\'')) v++;
                    const char* s = v;
                    while (v < lineEnd && *v != ',' && *v != ']' && *v != '"' && *v != '\'') v++;
                    sb_tags_add(out, nout, s, (size_t)(v - s));
                }
            } else {
                const char* l = lineEnd + 1;
                while (l < fmEnd) {
                    const char* le = (const char*)memchr(l, '\n', (size_t)(fmEnd - l)); if (!le) le = fmEnd;
                    const char* q = l; while (q < le && (*q == ' ' || *q == '\t')) q++;
                    if (q < le && *q == '-') { q++; while (q < le && *q == ' ') q++; sb_tags_add(out, nout, q, (size_t)(le - q)); }
                    else break;
                    l = le + 1;
                }
            }
            return;
        }
        t = lineEnd + 1;
    }
}
static char* sb_parse_date(const char* text) {
    if (strncmp(text, "---", 3) != 0) return NULL;
    const char* p = strchr(text, '\n'); if (!p) return NULL;
    const char* fmStart = p + 1;
    const char* fmEnd = strstr(fmStart, "\n---"); if (!fmEnd) return NULL;
    const char* t = fmStart;
    while (t < fmEnd) {
        const char* lineEnd = (const char*)memchr(t, '\n', (size_t)(fmEnd - t)); if (!lineEnd) lineEnd = fmEnd;
        if (strncmp(t, "date:", 5) == 0) {
            const char* v = t + 5; while (v < lineEnd && (*v == ' ' || *v == '\t')) v++;
            size_t l = (size_t)(lineEnd - v); while (l > 0 && (v[l-1] == ' ' || v[l-1] == '\t' || v[l-1] == '\r')) l--;
            char* d = (char*)malloc(l + 1); memcpy(d, v, l); d[l] = 0; return d;
        }
        t = lineEnd + 1;
    }
    return NULL;
}
static int sb_resolve_link(const char* target, sb_vnode_t* nodes, int nn) {
    char norm[4096]; size_t o = 0;
    for (const char* p = target; *p && o + 1 < sizeof(norm); p++) norm[o++] = (*p == '\\') ? '/' : *p;
    norm[o] = 0;
    for (int i = 0; i < nn; i++) if (strcmp(nodes[i].rel, norm) == 0) return i;
    size_t nl = strlen(norm);
    if (!(nl > 3 && strcasecmp(norm + nl - 3, ".md") == 0)) {
        char wm[4096]; snprintf(wm, sizeof(wm), "%s.md", norm);
        for (int i = 0; i < nn; i++) if (strcmp(nodes[i].rel, wm) == 0) return i;
    }
    const char* b = strrchr(norm, '/'); b = b ? b + 1 : norm;
    char bb[512]; snprintf(bb, sizeof(bb), "%s", b);
    size_t bl = strlen(bb);
    if (bl > 3 && strcasecmp(bb + bl - 3, ".md") == 0) bb[bl - 3] = 0;
    for (int i = 0; i < nn; i++) {
        const char* cb = strrchr(nodes[i].rel, '/'); cb = cb ? cb + 1 : nodes[i].rel;
        char cbb[512]; snprintf(cbb, sizeof(cbb), "%s", cb);
        size_t cl = strlen(cbb);
        if (cl > 3 && strcasecmp(cbb + cl - 3, ".md") == 0) cbb[cl - 3] = 0;
        if (strcasecmp(cbb, bb) == 0) return i;
    }
    return -1;
}

int64_t gmb_graph_build(const char* wikiDir, const char* outputDir) {
    if (!wikiDir || !*wikiDir) return -1;
    struct stat st;
    if (stat(wikiDir, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    sb_vnode_t* nodes = NULL; int nn = 0, cap = 0;
    sb_collect_md(wikiDir, "", &nodes, &nn, &cap);
    /* NIENTE qsort: GraphStore.Build usa Directory.GetFiles (ordine di readdir, non
     * ordinato) — l'ordine dei nodi/edge deve combaciare col C# (neighbor order QUERYMULTI). */
    typedef struct { int from, to; char* alias; } sb_vedge_t;
    sb_vedge_t* edges = NULL; int ne = 0, ecap = 0;
    char** blower = (char**)calloc((size_t)(nn ? nn : 1), sizeof(char*));
    for (int i = 0; i < nn; i++) {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", wikiDir, nodes[i].rel);
        FILE* f = fopen(full, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz < 0) sz = 0; if (sz > (1 << 20) - 1) sz = (1 << 20) - 1;
        char* body = (char*)malloc((size_t)sz + 1);
        size_t rd = fread(body, 1, (size_t)sz, f); body[rd] = 0; fclose(f);
        sb_parse_tags(body, &nodes[i].tags, &nodes[i].ntags);
        nodes[i].date = sb_parse_date(body);
        { size_t bl = strlen(body); char* lw = (char*)malloc(bl + 1); for (size_t k = 0; k < bl; k++) lw[k] = (char)tolower((unsigned char)body[k]); lw[bl] = 0; blower[i] = lw; }
        const char* p = body;
        while ((p = strstr(p, "[[")) != NULL) {
            const char* close = strstr(p + 2, "]]");
            if (!close) break;
            char target[1024]; size_t tl = 0; const char* q = p + 2;
            char alias[512]; size_t al = 0;
            while (q < close && *q != '|' && *q != '#' && tl + 1 < sizeof(target)) target[tl++] = *q++;
            target[tl] = 0;
            if (q < close && *q == '|') { q++; while (q < close && al + 1 < sizeof(alias)) alias[al++] = *q++; alias[al] = 0; }
            int to = sb_resolve_link(target, nodes, nn);
            if (to >= 0 && to != i) {
                if (ne >= ecap) { ecap = ecap ? ecap * 2 : 64; edges = (sb_vedge_t*)realloc(edges, (size_t)ecap * sizeof(sb_vedge_t)); }
                edges[ne].from = i; edges[ne].to = to; edges[ne].alias = al ? strdup(alias) : strdup("");
                ne++;
            }
            p = close + 2;
        }
        free(body);
    }
    /* RichGraph (parità GraphManager.Build): LINK + TAG_SHARE + FOLDER_CHILD +
     * CONTENT_MENTION + TEMPORAL_NEAR → handle attivo per le op single-brain. */
    {
        int64_t gh = gmb_graph_create();
        for (int i = 0; i < nn; i++) gmb_graph_add_node(gh, nodes[i].rel);
        sb_active_edges_clear();
        for (int i = 0; i < ne; i++) { gmb_graph_add_edge(gh, nodes[edges[i].from].rel, nodes[edges[i].to].rel, 1.0); sb_active_edge_add(nodes[edges[i].from].rel, nodes[edges[i].to].rel); }
        for (int i = 0; i < nn; i++) for (int j = i + 1; j < nn; j++) {
            int inter = 0;
            for (int a = 0; a < nodes[i].ntags; a++) { int f = 0; for (int b = 0; b < nodes[j].ntags; b++) if (strcasecmp(nodes[i].tags[a], nodes[j].tags[b]) == 0) { f = 1; break; } if (f) inter++; }
            int uni = nodes[i].ntags + nodes[j].ntags - inter;
            if (uni == 0) continue;
            double jac = (double)inter / uni;
            if (jac >= 0.5) { gmb_graph_add_edge_multi(gh, nodes[i].rel, nodes[j].rel, (double)((long long)(jac * 1000 + 0.5)) / 1000); sb_active_edge_add(nodes[i].rel, nodes[j].rel); }
        }
        for (int i = 0; i < nn; i++) for (int j = i + 1; j < nn; j++) {
            if (!nodes[i].folder[0] || strcmp(nodes[i].folder, nodes[j].folder) != 0) continue;
            int hasLink = 0;
            for (int e = 0; e < ne; e++) if ((edges[e].from == i && edges[e].to == j) || (edges[e].from == j && edges[e].to == i)) { hasLink = 1; break; }
            if (!hasLink) { gmb_graph_add_edge_multi(gh, nodes[i].rel, nodes[j].rel, 0.8); sb_active_edge_add(nodes[i].rel, nodes[j].rel); }
        }
        /* CONTENT_MENTION + TEMPORAL_NEAR: il C# li interleave per nodo a (foreach a in nodes). */
        for (int i = 0; i < nn; i++) {
            for (int j = 0; j < nn; j++) {
                if (i == j || !blower[i]) continue;
                char tl[1024]; size_t x = 0; for (const char* p = nodes[j].title; *p && x < sizeof(tl) - 1; p++) tl[x++] = (char)tolower((unsigned char)*p); tl[x] = 0;
                if (!tl[0] || !strstr(blower[i], tl)) continue;
                int hasLink = 0;
                for (int e = 0; e < ne; e++) if (edges[e].from == i && edges[e].to == j) { hasLink = 1; break; }
                if (!hasLink) { gmb_graph_add_edge_multi(gh, nodes[i].rel, nodes[j].rel, 0.6); sb_active_edge_add(nodes[i].rel, nodes[j].rel); }
            }
            for (int j = 0; j < nn; j++) {
                if (i == j || !nodes[i].date || !nodes[j].date) continue;
                double days = sb_date_days(nodes[i].date, nodes[j].date);
                if (days < 0 || days > 7) continue;
                if (!gmb_graph_has_edge(gh, nodes[i].rel, nodes[j].rel) && !gmb_graph_has_edge(gh, nodes[j].rel, nodes[i].rel)) {
                    double w = (double)((long long)(exp(-days / 7.0) * 1000 + 0.5)) / 1000;
                    gmb_graph_add_edge_multi(gh, nodes[i].rel, nodes[j].rel, w); sb_active_edge_add(nodes[i].rel, nodes[j].rel);
                }
            }
        }
        g_active_graph_h = gh;
        { char nm[1024]; snprintf(nm, sizeof(nm), "%s", wikiDir); size_t l2 = strlen(nm); while (l2 > 0 && nm[l2-1] == '/') nm[--l2] = 0; char* b2 = strrchr(nm, '/'); snprintf(g_active_graph_name, sizeof(g_active_graph_name), "%s", b2 ? b2 + 1 : nm); }
    }
    char name[1024]; snprintf(name, sizeof(name), "%s", wikiDir);
    size_t nl = strlen(name); while (nl > 0 && name[nl-1] == '/') name[--nl] = 0;
    char* base = strrchr(name, '/'); base = base ? base + 1 : name;
    const char* outDir = (outputDir && *outputDir) ? outputDir : ".";
    snprintf(g_graph_active_dir, sizeof(g_graph_active_dir), "%s", outDir);
    char outPath[4096]; snprintf(outPath, sizeof(outPath), "%s/%s.json", outDir, base);
    FILE* wf = fopen(outPath, "wb");
    if (!wf) {
        for (int i = 0; i < nn; i++) { free(nodes[i].rel); free(nodes[i].title); free(nodes[i].folder); free(nodes[i].date); for (int k = 0; k < nodes[i].ntags; k++) free(nodes[i].tags[k]); free(nodes[i].tags); free(blower[i]); }
        free(blower);
        free(nodes); for (int i = 0; i < ne; i++) free(edges[i].alias); free(edges);
        return -1;
    }
    fputs("{\"nodes\":[", wf);
    for (int i = 0; i < nn; i++) {
        char idb[8192], tb[8192]; sb_esc(nodes[i].rel, idb, sizeof(idb)); sb_esc(nodes[i].title, tb, sizeof(tb));
        fprintf(wf, "%s{\"id\":\"%s\",\"title\":\"%s\",\"frontmatter\":{", i ? "," : "", idb, tb);
        if (nodes[i].ntags > 0) {
            fputs("\"tags\":[", wf);
            for (int k = 0; k < nodes[i].ntags; k++) { char tg[1024]; sb_esc(nodes[i].tags[k], tg, sizeof(tg)); fprintf(wf, "%s\"%s\"", k ? "," : "", tg); }
            fputs("]", wf);
        }
        fputs("},\"community\":0,\"degree\":0}", wf);
    }
    fputs("],\"edges\":[", wf);
    for (int i = 0; i < ne; i++) {
        char fb[8192], tb[8192], ab[2048];
        sb_esc(nodes[edges[i].from].rel, fb, sizeof(fb)); sb_esc(nodes[edges[i].to].rel, tb, sizeof(tb)); sb_esc(edges[i].alias, ab, sizeof(ab));
        fprintf(wf, "%s{\"from\":\"%s\",\"to\":\"%s\",\"alias\":\"%s\",\"type\":\"LINK\",\"provenance\":null,\"confidence\":1}", i ? "," : "", fb, tb, ab);
    }
    fputs("],\"dangling\":[],\"folderCounts\":{}}", wf);
    fclose(wf);
    for (int i = 0; i < nn; i++) { free(nodes[i].rel); free(nodes[i].title); free(nodes[i].folder); free(nodes[i].date); for (int k = 0; k < nodes[i].ntags; k++) free(nodes[i].tags[k]); free(nodes[i].tags); free(blower[i]); }
    free(blower);
    free(nodes);
    for (int i = 0; i < ne; i++) free(edges[i].alias);
    free(edges);
    return nn;
}

int64_t gmb_graph_load_wikijson(const char* js) {
    if (!js || !*js) return 0;
    gmb_val_t root = gmb_json_parse(js);
    if (root.kind != GMB_TYPE_OBJECT || !root.v_ptr) return 0;
    gmb_obj_t* robj = (gmb_obj_t*)root.v_ptr;
    if (!robj->data) return 0;
    gmb_dict_t* rd = (gmb_dict_t*)robj->data;
    int64_t h = gmb_graph_create();
    if (!h) return 0;
    /* nodes: [{id,title,...}] */
    gmb_val_t nodesV = gmb_dict_get(*rd, "nodes");
    if (nodesV.kind == GMB_TYPE_OBJECT && nodesV.v_ptr) {
        gmb_list_t* nodes = (gmb_list_t*)((gmb_obj_t*)nodesV.v_ptr)->data;
        if (nodes) for (size_t i = 0; i < nodes->count; i++) {
            gmb_val_t node = nodes->items[i];
            if (node.kind != GMB_TYPE_OBJECT || !node.v_ptr) continue;
            gmb_dict_t* nd = (gmb_dict_t*)((gmb_obj_t*)node.v_ptr)->data;
            gmb_val_t id = nd ? gmb_dict_get(*nd, "id") : gmb_val_nil();
            if (id.kind == GMB_TYPE_STRING && id.v_string.data) {
                char buf[1024];
                snprintf(buf, sizeof(buf), "%.*s", (int)id.v_string.length, id.v_string.data);
                gmb_graph_add_node(h, buf);
            }
        }
    }
    /* edges: [{from,to,alias}] */
    gmb_val_t edgesV = gmb_dict_get(*rd, "edges");
    if (edgesV.kind == GMB_TYPE_OBJECT && edgesV.v_ptr) {
        gmb_list_t* edges = (gmb_list_t*)((gmb_obj_t*)edgesV.v_ptr)->data;
        if (edges) for (size_t i = 0; i < edges->count; i++) {
            gmb_val_t e = edges->items[i];
            if (e.kind != GMB_TYPE_OBJECT || !e.v_ptr) continue;
            gmb_dict_t* ed = (gmb_dict_t*)((gmb_obj_t*)e.v_ptr)->data;
            gmb_val_t f = ed ? gmb_dict_get(*ed, "from") : gmb_val_nil();
            gmb_val_t t = ed ? gmb_dict_get(*ed, "to") : gmb_val_nil();
            if (f.kind == GMB_TYPE_STRING && t.kind == GMB_TYPE_STRING && f.v_string.data && t.v_string.data) {
                char fb[1024], tb[1024];
                snprintf(fb, sizeof(fb), "%.*s", (int)f.v_string.length, f.v_string.data);
                snprintf(tb, sizeof(tb), "%.*s", (int)t.v_string.length, t.v_string.data);
                gmb_graph_add_edge(h, fb, tb, 1.0);
            }
        }
    }
    return h;
}

/* ── Graph brain query (GRAPH.NEIGHBORS/PATH/PAGERANK/...): carica il WikiGraph JSON del
 * brain, esegue l'algoritmo gmb_graph_*, ritorna JSON. Il path è come BrainRegistry:
 * <brains>/<name>/graphs/<name>.json oppure <graphs>/<name>.json. ── */

/* Costruisce un grafo equivalente a `src` (stessi nodi/archi) ma con PESO 1 per arco —
 * parità con ToCommonGraph del C# (AddEdge(...,1)); la semantica max-flow/Louvain dipende
 * solo dagli archi, non dai pesi del builder. */
static int64_t sb_unit_graph(int64_t src) {
    int n = 0; char** ns = gmb_graph_nodes(src, &n);
    int64_t gh = gmb_graph_create();
    if (!gh) { free(ns); return 0; }
    for (int i = 0; i < n; i++) gmb_graph_add_node_consistent(gh, ns[i] ? ns[i] : "");
    int m = 0; gmb_graph_edge_full_t* el = gmb_graph_edge_list(src, &m);
    for (int k = 0; k < m; k++) gmb_graph_add_edge(gh, el[k].from, el[k].to, 1.0);
    free(ns); free(el);
    return gh;
}

static void sb_graph_path(const char* name, char* out, size_t cap) {
    /* Una cartella "attiva" scelta da un comando GRAPH ha la precedenza. */
    if (g_graph_active_dir[0]) {
        snprintf(out, cap, "%s/%s.json", g_graph_active_dir, name);
        if (access(out, F_OK) == 0) return;
    }
    /* Poi la RISOLUZIONE dei cervelli (vendor → exe → brainsDir → cache <nome>/<versione>) e la
     * cartella di BUILD (paths.graphsDir, default ~/.gmbasic/graphs): una sola implementazione,
     * `gmb_brain_graph_json`, condivisa con il resto del runtime. Prima qui si cercava solo il
     * `<brainsDir>` piatto e un cervello INSTALLATO non era interrogabile come grafo. */
    if (gmb_brain_graph_json(name, out, cap)) return;
    out[0] = '\0';
}

static char* sb_json_str_array(char** arr, int n) {
    size_t cap = 64, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        int need = snprintf(NULL, 0, "%s\"%s\"", len ? "," : "", arr[i] ? arr[i] : "");
        if (len + (size_t)need + 1 > cap) { cap = (len + need + 1) * 2; char* r = realloc(out, cap); if (!r) { free(out); return NULL; } out = r; }
        len += snprintf(out + len, cap - len, "%s\"%s\"", len ? "," : "", arr[i] ? arr[i] : "");
    }
    size_t olen = len + 8;
    char* res = (char*)malloc(olen);
    if (!res) { free(out); return NULL; }
    snprintf(res, olen, "[%s]", out);
    free(out);
    return res;
}

typedef struct { char* id; char* title; char* folder; char** tags; int ntags; } sb_gnode_t;
static int sb_load_meta(const char* js, sb_gnode_t** out_n, int* out_nc, char*** out_f, char*** out_t, char*** out_a, int* out_ec);
static void sb_meta_free(sb_gnode_t* n, int nc, char** ef, char** et, char** ea, int ec);
static void sb_esc(const char* in, char* out, size_t cap);

/* GRAPH.QUERYMULTI(query, brains_csv): merge multi-brain (parità IL
 * GraphMultiQuery.QueryMulti — path/title/brains/neighborCount/neighbors). */
char* gmb_graph_query_multi(const char* query, const char* brains_csv)
{
    char* out = (char*)malloc(1 << 20);
    if (!out) return NULL;
    char* csv = strdup(brains_csv ? brains_csv : "");
    char* names[256]; int nbrains = 0;
    if (csv) {
        char* tok = strtok(csv, ",");
        while (tok && nbrains < 256) {
            while (*tok == ' ' || *tok == '\t') tok++;
            char* end = tok + strlen(tok);
            while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
            if (*tok) names[nbrains++] = tok;
            tok = strtok(NULL, ",");
        }
    }
    if (nbrains <= 0) {
        free(csv);
        strcpy(out, "{\"status\":\"InsufficientEvidence\",\"error\":\"nessun brain collegato (AI BRAIN USE)\",\"matches\":[]}");
        return out;
    }
    /* merge nodi/edge da tutti i brain disponibili. */
    sb_gnode_t* mnode = NULL; int mnn = 0;
    char*** mbrains = NULL; int* mnbrains = NULL;
    typedef struct { int from, to; char* alias; char* brain; } sb_medge_t;
    sb_medge_t* medges = NULL; int mne = 0;
    int any = 0;
    for (int i = 0; i < nbrains; i++) {
        char path[2048]; sb_graph_path(names[i], path, sizeof(path));
        if (access(path, F_OK) != 0) continue;
        FILE* f = fopen(path, "rb"); if (!f) continue;
        char* js = (char*)malloc(1 << 20); if (!js) { fclose(f); continue; }
        size_t n = fread(js, 1, (1 << 20) - 1, f); js[n] = '\0'; fclose(f);
        sb_gnode_t* nn = NULL; int nnc = 0; char** ef = NULL; char** et = NULL; char** ea = NULL; int ec = 0;
        if (sb_load_meta(js, &nn, &nnc, &ef, &et, &ea, &ec)) {
            any = 1;
            for (int k = 0; k < nnc; k++) {
                int idx = -1;
                for (int m = 0; m < mnn; m++) if (strcmp(mnode[m].id, nn[k].id) == 0) { idx = m; break; }
                if (idx < 0) {
                    mnode = (sb_gnode_t*)realloc(mnode, (size_t)(mnn + 1) * sizeof(sb_gnode_t));
                    mnode[mnn].id = strdup(nn[k].id); mnode[mnn].title = strdup(nn[k].title);
                    mnode[mnn].folder = NULL; mnode[mnn].tags = NULL; mnode[mnn].ntags = 0;
                    mbrains = (char***)realloc(mbrains, (size_t)(mnn + 1) * sizeof(char**)); mbrains[mnn] = NULL;
                    mnbrains = (int*)realloc(mnbrains, (size_t)(mnn + 1) * sizeof(int)); mnbrains[mnn] = 0;
                    idx = mnn++;
                }
                int found = 0; for (int b = 0; b < mnbrains[idx]; b++) if (strcmp(mbrains[idx][b], names[i]) == 0) { found = 1; break; }
                if (!found) { mbrains[idx] = (char**)realloc(mbrains[idx], (size_t)(mnbrains[idx] + 1) * sizeof(char*)); mbrains[idx][mnbrains[idx]++] = strdup(names[i]); }
            }
            for (int e = 0; e < ec; e++) {
                int fi = -1, ti = -1;
                for (int m = 0; m < mnn; m++) { if (strcmp(mnode[m].id, ef[e]) == 0) fi = m; if (strcmp(mnode[m].id, et[e]) == 0) ti = m; }
                if (fi < 0 || ti < 0) continue;
                medges = (sb_medge_t*)realloc(medges, (size_t)(mne + 1) * sizeof(sb_medge_t));
                medges[mne].from = fi; medges[mne].to = ti; medges[mne].alias = strdup(ea[e]); medges[mne].brain = strdup(names[i]);
                mne++;
            }
        }
        sb_meta_free(nn, nnc, ef, et, ea, ec);
        free(js);
    }
    if (!any || mnn == 0) {
        for (int m = 0; m < mnn; m++) { free(mnode[m].id); free(mnode[m].title); for (int b = 0; b < mnbrains[m]; b++) free(mbrains[m][b]); free(mbrains[m]); }
        free(mnode); free(mbrains); free(mnbrains);
        for (int e = 0; e < mne; e++) { free(medges[e].alias); free(medges[e].brain); }
        free(medges); free(csv);
        strcpy(out, "{\"status\":\"InsufficientEvidence\",\"error\":\"nessun brain graph interrogabile\",\"matches\":[]}");
        return out;
    }
    const char* q = query ? query : "";
    int exact = -1;
    for (int m = 0; m < mnn; m++) if (strcasecmp(mnode[m].id, q) == 0) { exact = m; break; }
    char qe[2048]; sb_esc(q, qe, sizeof(qe));
    size_t o = (size_t)snprintf(out, 1 << 20,
        "{\"status\":\"%s\",\"query\":\"%s\",\"brainCount\":%d,\"totalNodes\":%d,\"matches\":[",
        "Success", qe, nbrains, mnn);
    int nmatch = 0;
    for (int m = 0; m < mnn && nmatch < 50; m++) {
        int match = (exact >= 0) ? (m == exact)
                  : (strcasestr(mnode[m].id, q) != NULL || strcasestr(mnode[m].title, q) != NULL);
        if (!match) continue;
        char idb[2048], tb[2048]; sb_esc(mnode[m].id, idb, sizeof(idb)); sb_esc(mnode[m].title, tb, sizeof(tb));
        o += (size_t)snprintf(out + o, (1 << 20) - o, "%s{\"path\":\"%s\",\"title\":\"%s\",\"brains\":[", nmatch ? "," : "", idb, tb);
        for (int b = 0; b < mnbrains[m]; b++) { char bb[512]; sb_esc(mbrains[m][b], bb, sizeof(bb)); o += (size_t)snprintf(out + o, (1 << 20) - o, "%s\"%s\"", b ? "," : "", bb); }
        /* neighbors: edge out/in su questo nodo */
        int nbc = 0;
        size_t nboff = o;
        o += (size_t)snprintf(out + o, (1 << 20) - o, "],\"neighborCount\":");
        size_t cntPos = o;
        o += (size_t)snprintf(out + o, (1 << 20) - o, "0,\"neighbors\":[");
        int firstNb = 1;
        for (int e = 0; e < mne; e++) {
            if (medges[e].from != m && medges[e].to != m) continue;
            char ab[2048], ob[2048], brb[512], aj[2100];
            sb_esc(medges[e].alias, ab, sizeof(ab)); sb_esc(medges[e].brain, brb, sizeof(brb));
            if (medges[e].alias[0]) snprintf(aj, sizeof(aj), "\"%s\"", ab); else snprintf(aj, sizeof(aj), "null");
            if (medges[e].from == m) {
                sb_esc(mnode[medges[e].to].id, ob, sizeof(ob));
                o += (size_t)snprintf(out + o, (1 << 20) - o, "%s{\"to\":\"%s\",\"alias\":%s,\"direction\":\"out\",\"brain\":\"%s\"}", firstNb ? "" : ",", ob, aj, brb);
            } else {
                sb_esc(mnode[medges[e].from].id, ob, sizeof(ob));
                o += (size_t)snprintf(out + o, (1 << 20) - o, "%s{\"from\":\"%s\",\"alias\":%s,\"direction\":\"in\",\"brain\":\"%s\"}", firstNb ? "" : ",", ob, aj, brb);
            }
            firstNb = 0; nbc++;
            if (nbc >= 20) break;
        }
        o += (size_t)snprintf(out + o, (1 << 20) - o, "]}");
        /* patch neighborCount (cifre) */
        char cntb[16]; int cl = snprintf(cntb, sizeof(cntb), "%d", nbc);
        if (cl > 1) {
            memmove(out + cntPos + cl, out + cntPos + 1, o - (cntPos + 1));
            memcpy(out + cntPos, cntb, (size_t)cl);
            o += (size_t)(cl - 1);
        } else out[cntPos] = cntb[0];
        (void)nboff;
        nmatch++;
        if (exact >= 0) break;
    }
    snprintf(out + o, (1 << 20) - o, "]}");
    for (int m = 0; m < mnn; m++) { free(mnode[m].id); free(mnode[m].title); for (int b = 0; b < mnbrains[m]; b++) free(mbrains[m][b]); free(mbrains[m]); }
    free(mnode); free(mbrains); free(mnbrains);
    for (int e = 0; e < mne; e++) { free(medges[e].alias); free(medges[e].brain); }
    free(medges); free(csv);
    return out;
}

/* ── Export estesi GRAPH.* (DOT/SUBGRAPH/BYTAG/BYFOLDER/MST): i metadati dei nodi
 * (title/tags/folder) NON sono nel registry gmb_graph_* → si ri-legge il JSON del brain
 * (gmb_json_parse), parità GraphExporter.cs / GraphMultiQuery.MinimumSpanningTree. ── */

static char* sb_dup_valstr(gmb_val_t v) {
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) {
        char* s = (char*)malloc((size_t)v.v_string.length + 1);
        if (s) { memcpy(s, v.v_string.data, (size_t)v.v_string.length); s[v.v_string.length] = 0; }
        return s;
    }
    return strdup("");
}
static void sb_ident_sanitize(const char* in, char* out, size_t cap) {
    size_t o = 0;
    for (const char* p = in; p && *p && o + 1 < cap; p++) {
        char ch = *p;
        out[o++] = ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) ? ch : '_';
    }
    out[o] = 0;
}
static void sb_esc(const char* in, char* out, size_t cap) {
    size_t o = 0;
    for (const char* p = in; p && *p && o + 2 < cap; p++) {
        char ch = *p;
        if (ch == '"' || ch == '\\') { out[o++] = '\\'; out[o++] = ch; }
        else if (ch == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (ch == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else out[o++] = ch;
    }
    out[o] = 0;
}

/* Carica nodi (id/title/folder/tags) ed edge (from/to/alias) dal JSON del brain.
 * Ritorna 1 se ok (anche vuoto), 0 se JSON non valido. Il chiamante libera con
 * sb_meta_free. */
static int sb_load_meta(const char* js, sb_gnode_t** out_n, int* out_nc,
                        char*** out_f, char*** out_t, char*** out_a, int* out_ec) {
    *out_n = NULL; *out_nc = 0; *out_f = NULL; *out_t = NULL; *out_a = NULL; *out_ec = 0;
    gmb_val_t root = gmb_json_parse(js);
    if (root.kind != GMB_TYPE_OBJECT || !root.v_ptr) return 0;
    gmb_dict_t* rd = (gmb_dict_t*)((gmb_obj_t*)root.v_ptr)->data;
    if (!rd) return 0;
    gmb_val_t nodesV = gmb_dict_get(*rd, "nodes");
    if (nodesV.kind == GMB_TYPE_OBJECT && nodesV.v_ptr) {
        gmb_list_t* nodes = (gmb_list_t*)((gmb_obj_t*)nodesV.v_ptr)->data;
        if (nodes && nodes->count > 0) {
            sb_gnode_t* arr = (sb_gnode_t*)calloc(nodes->count, sizeof(sb_gnode_t));
            int cnt = 0;
            for (size_t i = 0; arr && i < nodes->count; i++) {
                gmb_val_t node = nodes->items[i];
                if (node.kind != GMB_TYPE_OBJECT || !node.v_ptr) continue;
                gmb_dict_t* nd = (gmb_dict_t*)((gmb_obj_t*)node.v_ptr)->data;
                if (!nd) continue;
                arr[cnt].id = sb_dup_valstr(gmb_dict_get(*nd, "id"));
                arr[cnt].title = sb_dup_valstr(gmb_dict_get(*nd, "title"));
                char* slash = strrchr(arr[cnt].id, '/');
                if (slash) { size_t dl = (size_t)(slash - arr[cnt].id); arr[cnt].folder = (char*)malloc(dl + 1); if (arr[cnt].folder) { memcpy(arr[cnt].folder, arr[cnt].id, dl); arr[cnt].folder[dl] = 0; } }
                else arr[cnt].folder = strdup("");
                gmb_val_t fm = gmb_dict_get(*nd, "frontmatter");
                if (fm.kind == GMB_TYPE_OBJECT && fm.v_ptr) {
                    gmb_dict_t* fd = (gmb_dict_t*)((gmb_obj_t*)fm.v_ptr)->data;
                    gmb_val_t tg = fd ? gmb_dict_get(*fd, "tags") : gmb_val_nil();
                    if (tg.kind == GMB_TYPE_OBJECT && tg.v_ptr) {
                        gmb_list_t* tl = (gmb_list_t*)((gmb_obj_t*)tg.v_ptr)->data;
                        if (tl && tl->count > 0) {
                            arr[cnt].tags = (char**)calloc(tl->count, sizeof(char*));
                            for (size_t k = 0; arr[cnt].tags && k < tl->count; k++)
                                arr[cnt].tags[arr[cnt].ntags++] = sb_dup_valstr(tl->items[k]);
                        }
                    }
                }
                cnt++;
            }
            *out_n = arr; *out_nc = cnt;
        }
    }
    gmb_val_t edgesV = gmb_dict_get(*rd, "edges");
    if (edgesV.kind == GMB_TYPE_OBJECT && edgesV.v_ptr) {
        gmb_list_t* edges = (gmb_list_t*)((gmb_obj_t*)edgesV.v_ptr)->data;
        if (edges && edges->count > 0) {
            char** ef = (char**)calloc(edges->count, sizeof(char*));
            char** et = (char**)calloc(edges->count, sizeof(char*));
            char** ea = (char**)calloc(edges->count, sizeof(char*));
            int cnt = 0;
            for (size_t i = 0; ef && et && ea && i < edges->count; i++) {
                gmb_val_t e = edges->items[i];
                if (e.kind != GMB_TYPE_OBJECT || !e.v_ptr) continue;
                gmb_dict_t* ed = (gmb_dict_t*)((gmb_obj_t*)e.v_ptr)->data;
                if (!ed) continue;
                ef[cnt] = sb_dup_valstr(gmb_dict_get(*ed, "from"));
                et[cnt] = sb_dup_valstr(gmb_dict_get(*ed, "to"));
                ea[cnt] = sb_dup_valstr(gmb_dict_get(*ed, "alias"));
                cnt++;
            }
            *out_f = ef; *out_t = et; *out_a = ea; *out_ec = cnt;
        }
    }
    return 1;
}
static void sb_meta_free(sb_gnode_t* n, int nc, char** ef, char** et, char** ea, int ec) {
    for (int i = 0; i < nc; i++) {
        free(n[i].id); free(n[i].title); free(n[i].folder);
        for (int j = 0; j < n[i].ntags; j++) free(n[i].tags[j]);
        free(n[i].tags);
    }
    free(n);
    for (int i = 0; i < ec; i++) { free(ef[i]); free(et[i]); free(ea[i]); }
    free(ef); free(et); free(ea);
}

/* Append con realloc. */
static void sb_app(char** o, size_t* l, size_t* c, const char* s) {
    size_t n = strlen(s);
    if (*l + n + 1 > *c) { *c = (*l + n + 1) * 2; *o = (char*)realloc(*o, *c); }
    memcpy(*o + *l, s, n); *l += n; (*o)[*l] = 0;
}
static int sb_node_idx(sb_gnode_t* n, int nc, const char* id) {
    for (int i = 0; i < nc; i++) if (strcmp(n[i].id, id) == 0) return i;
    return -1;
}
static int sb_has_tag(sb_gnode_t* n, const char* tag) {
    for (int i = 0; i < n->ntags; i++) if (strcasecmp(n->tags[i], tag) == 0) return 1;
    return 0;
}

/* DOT / SUBGRAPH (mermaid) / BYTAG / BYFOLDER / MST — parità GraphExporter.cs. */
static char* sb_graph_export_op(const char* js, const char* brain, const char* op, const char* arg1, const char* arg2) {
    sb_gnode_t* nodes = NULL; int nn = 0;
    char** ef = NULL; char** et = NULL; char** ea = NULL; int ne = 0;
    if (!sb_load_meta(js, &nodes, &nn, &ef, &et, &ea, &ne))
        return strdup("{\"status\":\"Error\",\"error\":\"graph JSON non valido\"}");
    char* out = NULL;
    size_t cap = 4096, len = 0;
    out = (char*)malloc(cap); if (out) out[0] = '\0';
    if (strcasecmp(op, "DOT") == 0) {
        char idb[512], tb[2048];
        sb_ident_sanitize(brain ? brain : "", idb, sizeof(idb));
        char* hdr = (char*)malloc(64); hdr[0] = 0;
        if (out) {
            char tmp[8192]; snprintf(tmp, sizeof(tmp), "digraph %s {\n    rankdir=LR;\n", idb); sb_app(&out, &len, &cap, tmp);
            for (int i = 0; i < nn; i++) {
                char a[4096], b[4096];
                sb_esc(nodes[i].id, a, sizeof(a)); sb_esc(nodes[i].title, b, sizeof(b));
                snprintf(tmp, sizeof(tmp), "    \"%s\" [label=\"%s\"];\n", a, b); sb_app(&out, &len, &cap, tmp);
            }
            int use_active = (g_active_ne > 0 && strcasecmp(brain ? brain : "", g_active_graph_name) == 0);
            if (use_active) {
                for (int i = 0; i < g_active_ne; i++) {
                    char a[4096], b[4096];
                    sb_esc(g_active_edges[i].from, a, sizeof(a)); sb_esc(g_active_edges[i].to, b, sizeof(b));
                    snprintf(tmp, sizeof(tmp), "    \"%s\" -> \"%s\" [label=\"\"];\n", a, b); sb_app(&out, &len, &cap, tmp);
                }
            } else {
                for (int i = 0; i < ne; i++) {
                    char a[2048], b[2048], c[2048];
                    sb_esc(ef[i], a, sizeof(a)); sb_esc(et[i], b, sizeof(b)); sb_esc(ea[i], c, sizeof(c));
                    snprintf(tmp, sizeof(tmp), "    \"%s\" -> \"%s\" [label=\"%s\"];\n", a, b, c); sb_app(&out, &len, &cap, tmp);
                }
            }
            sb_app(&out, &len, &cap, "}\n");
        }
        free(hdr);
    } else if (strcasecmp(op, "SUBGRAPH") == 0) {
        const char* root = arg1 ? arg1 : "";
        int depth = arg2 ? atoi(arg2) : 1;
        int use_active = (g_active_ne > 0 && strcasecmp(brain ? brain : "", g_active_graph_name) == 0);
        int neff = use_active ? g_active_ne : ne;
        if (out) {
            sb_app(&out, &len, &cap, "graph TD\n");
            int ri = sb_node_idx(nodes, nn, root);
            if (ri >= 0) {
                char sb1[2048], tb[2048], tmp[4096];
                sb_ident_sanitize(root, sb1, sizeof(sb1)); sb_esc(nodes[ri].title, tb, sizeof(tb));
                snprintf(tmp, sizeof(tmp), "    N%s[\"%s\"]\n", sb1, tb); sb_app(&out, &len, &cap, tmp);
                /* BFS sui vicini (grafo orientato: from->to). */
                char** frontier = (char**)calloc(neff + 1, sizeof(char*)); int fn = 0;
                char** visited = (char**)calloc(nn + 1, sizeof(char*)); int vn = 0;
                if (frontier && visited) {
                    frontier[fn++] = strdup(root); visited[vn++] = strdup(root);
                    for (int d = 0; d < depth && fn > 0; d++) {
                        char** next = (char**)calloc(neff + 1, sizeof(char*)); int xn = 0;
                        for (int k = 0; k < fn; k++) {
                            for (int e = 0; e < neff; e++) {
                                const char* efx = use_active ? g_active_edges[e].from : ef[e];
                                const char* etx = use_active ? g_active_edges[e].to : et[e];
                                if (strcmp(efx, frontier[k]) != 0) continue;
                                int seen = 0; for (int v = 0; v < vn; v++) if (strcmp(visited[v], etx) == 0) { seen = 1; break; }
                                if (seen) continue;
                                int ni = sb_node_idx(nodes, nn, etx);
                                char cs[2048], ns[2048], nt[2048];
                                sb_ident_sanitize(frontier[k], cs, sizeof(cs));
                                sb_ident_sanitize(etx, ns, sizeof(ns));
                                sb_esc(ni >= 0 ? nodes[ni].title : etx, nt, sizeof(nt));
                                snprintf(tmp, sizeof(tmp), "    N%s[\"%s\"]\n    N%s --> N%s\n", ns, nt, cs, ns); sb_app(&out, &len, &cap, tmp);
                                visited[vn++] = strdup(etx); next[xn++] = strdup(etx);
                            }
                        }
                        for (int k = 0; k < fn; k++) free(frontier[k]);
                        free(frontier); frontier = next; fn = xn;
                    }
                    for (int k = 0; k < fn; k++) free(frontier[k]); free(frontier);
                    for (int k = 0; k < vn; k++) free(visited[k]); free(visited);
                } else { free(frontier); free(visited); }
            }
        }
    } else if (strcasecmp(op, "BYTAG") == 0 || strcasecmp(op, "BYFOLDER") == 0) {
        int is_tag = strcasecmp(op, "BYTAG") == 0;
        const char* q = arg1 ? arg1 : "";   /* BYTAG: tag; BYFOLDER: folder (2° arg) */
        char qq[2048]; sb_esc(q, qq, sizeof(qq));
        int* match = (int*)calloc(nn + 1, sizeof(int)); int mc = 0;
        char* fl = NULL;
        if (!is_tag) {
            size_t ql = strlen(q);
            fl = (char*)malloc(ql + 1); memcpy(fl, q, ql + 1);
            while (ql > 0 && fl[ql-1] == '/') fl[--ql] = 0;
        }
        for (int i = 0; i < nn; i++) {
            if (is_tag) { if (sb_has_tag(&nodes[i], q)) { match[i] = 1; mc++; } }
            else {
                const char* f = fl ? fl : "";
                if (f[0] == 0 || strcmp(nodes[i].id, f) == 0 || (strncasecmp(nodes[i].id, f, strlen(f)) == 0 && nodes[i].id[strlen(f)] == '/')) { match[i] = 1; mc++; }
            }
        }
        if (out) {
            char tmp[4096];
            /* JSON INDENTATO a 2 spazi (parità `JsonSerializer.Serialize(..., WriteIndented=true)`
             * del C#: GraphExporter.ToJsonByTag/ByFolder). Prima emetteva i newline ma SENZA
             * indentazione → 88 righe di diff su `GraphFunctions.bas` (falso "gap C99Native"). */
            snprintf(tmp, sizeof(tmp), "{\n  \"%s\": \"%s\",\n  \"nodes\": [", is_tag ? "tag" : "folder", qq); sb_app(&out, &len, &cap, tmp);
            int first = 1;
            for (int i = 0; i < nn; i++) if (match[i]) {
                char a[2048], b[2048]; sb_esc(nodes[i].id, a, sizeof(a)); sb_esc(nodes[i].title, b, sizeof(b));
                snprintf(tmp, sizeof(tmp), "%s\n    {\n      \"id\": \"%s\",\n      \"title\": \"%s\"\n    }", first ? "" : ",", a, b); sb_app(&out, &len, &cap, tmp); first = 0;
            }
            sb_app(&out, &len, &cap, "\n  ],\n  \"edges\": [");
            first = 1;
            int use_active = (g_active_ne > 0 && strcasecmp(brain ? brain : "", g_active_graph_name) == 0);
            int neff = use_active ? g_active_ne : ne;
            for (int e = 0; e < neff; e++) {
                const char* efx = use_active ? g_active_edges[e].from : ef[e];
                const char* etx = use_active ? g_active_edges[e].to : et[e];
                int fi = sb_node_idx(nodes, nn, efx); int ti = sb_node_idx(nodes, nn, etx);
                if (fi < 0 || ti < 0 || !match[fi] || !match[ti]) continue;
                char a[2048], b[2048]; sb_esc(efx, a, sizeof(a)); sb_esc(etx, b, sizeof(b));
                snprintf(tmp, sizeof(tmp), "%s\n    {\n      \"from\": \"%s\",\n      \"to\": \"%s\"\n    }", first ? "" : ",", a, b); sb_app(&out, &len, &cap, tmp); first = 0;
            }
            snprintf(tmp, sizeof(tmp), "\n  ],\n  \"count\": %d\n}", mc); sb_app(&out, &len, &cap, tmp);
        }
        free(match); free(fl);
    } else if (strcasecmp(op, "MST") == 0) {
        /* Kruskal: pesi 1.0 (dal JSON) → spanning forest. Usa l'ActiveGraph (g.Edges) se attivo. */
        int* parent = (int*)malloc((nn + 1) * sizeof(int));
        for (int i = 0; i < nn; i++) parent[i] = i;
        int* mf = (int*)calloc(ne + g_active_ne + 1, sizeof(int)); int* mt = (int*)calloc(ne + g_active_ne + 1, sizeof(int)); int mcnt = 0;
        int use_active = (g_active_ne > 0 && strcasecmp(brain ? brain : "", g_active_graph_name) == 0);
        int neff = use_active ? g_active_ne : ne;
        /* AllEdges() del C#: per ogni nodo (ordine) i suoi edge uscenti (ordine), dedup per (from,to). */
        for (int i = 0; i < nn; i++) {
            for (int e = 0; e < neff; e++) {
                const char* f = use_active ? g_active_edges[e].from : ef[e];
                const char* t = use_active ? g_active_edges[e].to : et[e];
                if (strcmp(f, nodes[i].id) != 0) continue;
                int b = sb_node_idx(nodes, nn, t);
                if (b < 0) continue;
                int ra = i, rb = b; while (parent[ra] != ra) ra = parent[ra]; while (parent[rb] != rb) rb = parent[rb];
                if (ra == rb) continue;
                parent[ra] = rb; mf[mcnt] = i; mt[mcnt] = b; mcnt++;
            }
        }
        if (out) {
            char tmp[4096];
            snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"weight\":%d,\"edges\":[", brain ? brain : "", mcnt); sb_app(&out, &len, &cap, tmp);
            int first = 1;
            for (int i = 0; i < mcnt; i++) {
                char a[2048], b[2048]; sb_esc(nodes[mf[i]].id, a, sizeof(a)); sb_esc(nodes[mt[i]].id, b, sizeof(b));
                snprintf(tmp, sizeof(tmp), "%s{\"from\":\"%s\",\"to\":\"%s\",\"weight\":1}", first ? "" : ",", a, b); sb_app(&out, &len, &cap, tmp); first = 0;
            }
            sb_app(&out, &len, &cap, "]}");
        }
        free(parent); free(mf); free(mt);
    } else {
        free(out); out = strdup("{\"status\":\"Error\",\"error\":\"op non supportata\"}");
    }
    sb_meta_free(nodes, nn, ef, et, ea, ne);
    return out;
}

/* ── GRAPH.* op single-brain: formati parità IL GraphMultiQuery.cs ── */
static int sb_cmp_str(const void* a, const void* b) { return strcmp(*(const char* const*)a, *(const char* const*)b); }
static int sb_cmp_pr_desc(const void* a, const void* b) {
    double ra = ((const gmb_graph_pagerank_t*)a)->rank, rb = ((const gmb_graph_pagerank_t*)b)->rank;
    return ra < rb ? 1 : (ra > rb ? -1 : 0);
}
static int sb_cmp_ct_desc(const void* a, const void* b) {
    double ra = ((const gmb_graph_centrality_t*)a)->value, rb = ((const gmb_graph_centrality_t*)b)->value;
    return ra < rb ? 1 : (ra > rb ? -1 : 0);
}
/* Numero come C# Math.Round(x,6) + JsonSerializer (niente zeri finali). */
static void sb_num6(double d, char* out, size_t cap) {
    double r = (double)((long long)(d * 1e6 + (d < 0 ? -0.5 : 0.5))) / 1e6;
    snprintf(out, cap, "%.6f", r);
    char* dot = strchr(out, '.');
    if (dot) { char* e = out + strlen(out) - 1; while (e > dot && *e == '0') *e-- = 0; if (e == dot) *e = 0; }
}
static void sb_app_strarr(char** o, size_t* l, size_t* c, char** arr, int n) {
    sb_app(o, l, c, "[");
    for (int i = 0; i < n; i++) { char x[8192]; sb_esc(arr[i] ? arr[i] : "", x, sizeof(x)); if (i) sb_app(o, l, c, ","); sb_app(o, l, c, "\""); sb_app(o, l, c, x); sb_app(o, l, c, "\""); }
    sb_app(o, l, c, "]");
}
/* Neighbors WikiGraph (parità GraphStore.Neighbors): OUT (ordine inserzione) poi IN, distinct. */
static void sb_wiki_nb(const char* node, char** out, int* n) {
    *n = 0;
    for (int i = 0; i < g_active_ne; i++) if (strcmp(g_active_edges[i].from, node) == 0) { int f = 0; for (int k = 0; k < *n; k++) if (strcmp(out[k], g_active_edges[i].to) == 0) { f = 1; break; } if (!f && *n < 4096) out[(*n)++] = g_active_edges[i].to; }
    for (int i = 0; i < g_active_ne; i++) if (strcmp(g_active_edges[i].to, node) == 0) { int f = 0; for (int k = 0; k < *n; k++) if (strcmp(out[k], g_active_edges[i].from) == 0) { f = 1; break; } if (!f && *n < 4096) out[(*n)++] = g_active_edges[i].from; }
}

/* Tarjan (parità CommonLib.Graph.StronglyConnectedComponents): out-adjacency in ordine
 * di edge (dedup), nodi di partenza in ordine. */
static void sb_scc_dfs(int v, int** adj, int* adjn, int* index, int* low, int* onstack,
                       int* stk, int* sp, int* counter, char** names,
                       char**** result, int* resn, int* rescap) {
    index[v] = low[v] = (*counter)++;
    stk[(*sp)++] = v; onstack[v] = 1;
    for (int k = 0; k < adjn[v]; k++) {
        int w = adj[v][k];
        if (index[w] == -1) { sb_scc_dfs(w, adj, adjn, index, low, onstack, stk, sp, counter, names, result, resn, rescap); if (low[w] < low[v]) low[v] = low[w]; }
        else if (onstack[w]) { if (index[w] < low[v]) low[v] = index[w]; }
    }
    if (low[v] == index[v]) {
        char** scc = (char**)calloc((size_t)(*counter + 1), sizeof(char*)); int cn = 0;
        int w;
        do { w = stk[--(*sp)]; onstack[w] = 0; scc[cn++] = strdup(names[w]); } while (w != v);
        if (*resn >= *rescap) { *rescap = *rescap ? *rescap * 2 : 8; *result = (char***)realloc(*result, (size_t)*rescap * sizeof(char**)); }
        (*result)[(*resn)++] = scc;
    }
}

const char* gmb_graph_active_name(void) { return g_active_graph_name; }
int gmb_graph_rdd_set(const char* backend) {
    if (!backend) return -1;
    if (strcasecmp(backend, "MEMORY") == 0 || strcasecmp(backend, "JSON") == 0 || strcasecmp(backend, "SQLITE") == 0) return 0;
    return -1;
}

char* gmb_graph_brain_query(const char* brain, const char* op, const char* arg1, const char* arg2, int arg3) {
    if (!brain || !op) return NULL;
    char path[2048];
    sb_graph_path(brain, path, sizeof(path));
    if (access(path, F_OK) != 0) {
        if (strcasecmp(op, "ISVALID") == 0 || strcasecmp(op, "HASCYCLE") == 0) return strdup("0");
        /* Parità IL/PCode (GraphMultiQuery.cs): messaggio identico, con le virgolette
         * singole escapate come \u0027 (stile System.Text.Json) → EXPECT dei gen_GRAPH_*.
         * Prima: "graph brain non trovato" (divergenza di messaggio). */
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "{\"status\":\"Error\",\"error\":\"brain \\u0027%s\\u0027 non disponibile come graph\"}",
                 brain);
        return strdup(msg);
    }
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char* js = (char*)malloc(1 << 20);
    if (!js) { fclose(f); return NULL; }
    size_t n = fread(js, 1, (1 << 20) - 1, f);
    js[n] = '\0';
    fclose(f);
    int use_active = (g_active_graph_h && strcasecmp(brain, g_active_graph_name) == 0);
    int64_t h = use_active ? g_active_graph_h : gmb_graph_load_wikijson(js);
    if (!h) { free(js); return strdup("{\"status\":\"Error\",\"error\":\"graph JSON non valido\"}"); }

    char* out = NULL;
    char bb[4096]; sb_esc(brain ? brain : "", bb, sizeof(bb));
    if (strcasecmp(op, "ISVALID") == 0) {
        out = strdup(gmb_graph_node_count(h) > 0 ? "1" : "0");
    } else if (strcasecmp(op, "STATS") == 0) {
        int ncnt = 0; char** ns = gmb_graph_nodes(h, &ncnt);
        long ec = 0; for (int i = 0; i < ncnt; i++) ec += gmb_graph_get_outdegree(h, ns[i]);
        int pl = 0; gmb_graph_pagerank_t* pr = gmb_graph_pagerank(h, 0.85, 100, &pl);
        if (pr && pl > 1) qsort(pr, (size_t)pl, sizeof(gmb_graph_pagerank_t), sb_cmp_pr_desc);
        size_t cap = 1024, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"nodes\":%d,\"edges\":%ld,\"dangling\":0,\"pageRankTop\":[", bb, ncnt, ec); sb_app(&out, &len, &cap, tmp);
        int k5 = pl < 5 ? pl : 5;
        for (int i = 0; i < k5; i++) { char rn[64]; sb_num6(pr[i].rank, rn, sizeof(rn)); char nnb[4096]; sb_esc(pr[i].node ? pr[i].node : "", nnb, sizeof(nnb)); snprintf(tmp, sizeof(tmp), "%s{\"node\":\"%s\",\"rank\":%s}", i ? "," : "", nnb, rn); sb_app(&out, &len, &cap, tmp); }
        sb_app(&out, &len, &cap, "]}");
        free(ns); free(pr);
    } else if (strcasecmp(op, "ADJACENCY") == 0) {
        /* Ponte BRAINS -> GRAPH/FLOW: nodi (ordine ordinale = indice di matrice) + archi.
         * Ordine canonico IDENTICO al mondo C# (GraphMultiQuery.Adjacency): nodi in ordine strcmp,
         * una voce per coppia (i,j) — topologia, peso uniforme —, coppie ordinate per (i,j).
         * Niente pesi nel payload: C# legge il grafo attivo in memoria e il C il JSON esportato,
         * quindi le confidenze degli archi inferiti divergono; il ponte resta sulla topologia. */
        int ncnt = 0; char** ns = gmb_graph_nodes(h, &ncnt);
        for (int i = 1; i < ncnt; i++) {
            char* k = ns[i]; int j = i - 1;
            while (j >= 0 && strcmp(ns[j], k) > 0) { ns[j + 1] = ns[j]; j--; }
            ns[j + 1] = k;
        }
        int el = 0; gmb_graph_edge_full_t* es = gmb_graph_edge_list(h, &el);
        typedef struct { int i, j; } sb_adj_e_t;
        sb_adj_e_t* ag = el > 0 ? (sb_adj_e_t*)calloc((size_t)el, sizeof(sb_adj_e_t)) : NULL;
        int an = 0;
        for (int e = 0; e < el; e++) {
            int a = -1, b = -1;
            for (int i = 0; i < ncnt; i++) {
                if (a < 0 && strcmp(ns[i], es[e].from) == 0) a = i;
                if (b < 0 && strcmp(ns[i], es[e].to) == 0) b = i;
                if (a >= 0 && b >= 0) break;
            }
            if (a < 0 || b < 0 || !ag) continue;
            int found = 0;
            for (int k = 0; k < an; k++) if (ag[k].i == a && ag[k].j == b) { found = 1; break; }
            if (!found) { ag[an].i = a; ag[an].j = b; an++; }
        }
        for (int i = 1; i < an; i++) {
            sb_adj_e_t k = ag[i]; int j = i - 1;
            while (j >= 0 && (ag[j].i > k.i || (ag[j].i == k.i && ag[j].j > k.j))) { ag[j + 1] = ag[j]; j--; }
            ag[j + 1] = k;
        }
        size_t cap = 1024 + (size_t)ncnt * 96 + (size_t)an * 48, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192];
        snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"n\":%d,\"nodes\":[", bb, ncnt);
        sb_app(&out, &len, &cap, tmp);
        for (int i = 0; i < ncnt; i++) {
            char nb[4096]; sb_esc(ns[i] ? ns[i] : "", nb, sizeof(nb));
            snprintf(tmp, sizeof(tmp), "%s\"%s\"", i ? "," : "", nb); sb_app(&out, &len, &cap, tmp);
        }
        sb_app(&out, &len, &cap, "],\"edges\":[");
        for (int k = 0; k < an; k++) {
            snprintf(tmp, sizeof(tmp), "%s[%d,%d]", k ? "," : "", ag[k].i, ag[k].j);
            sb_app(&out, &len, &cap, tmp);
        }
        sb_app(&out, &len, &cap, "]}");
        free(ns); free(es); free(ag);
    } else if (strcasecmp(op, "NEIGHBORS") == 0 && arg1) {
        int l = 0; gmb_graph_edge_t* ns = gmb_graph_neighbors(h, arg1, &l);
        char** arr = (char**)malloc((size_t)(l ? l : 1) * sizeof(char*)); for (int i = 0; i < l; i++) arr[i] = ns[i].node;
        qsort(arr, (size_t)l, sizeof(char*), sb_cmp_str);
        char nb[4096]; sb_esc(arg1, nb, sizeof(nb));
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"node\":\"%s\",\"neighbors\":", bb, nb); sb_app(&out, &len, &cap, tmp);
        sb_app_strarr(&out, &len, &cap, arr, l);
        sb_app(&out, &len, &cap, "}");
        free(arr); free(ns);
    } else if (strcasecmp(op, "PATH") == 0 && arg1 && arg2) {
        int l = 0; char** p = gmb_graph_shortest_path(h, arg1, arg2, &l);
        char fb[4096], tb[4096]; sb_esc(arg1, fb, sizeof(fb)); sb_esc(arg2, tb, sizeof(tb));
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"%s\",\"brain\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"path\":", l > 0 ? "Success" : "InsufficientEvidence", bb, fb, tb); sb_app(&out, &len, &cap, tmp);
        sb_app_strarr(&out, &len, &cap, p, l);
        sb_app(&out, &len, &cap, "}");
        free(p);
    } else if (strcasecmp(op, "PAGERANK") == 0) {
        int l = 0; gmb_graph_pagerank_t* pr = gmb_graph_pagerank(h, 0.85, 100, &l);
        if (pr && l > 1) qsort(pr, (size_t)l, sizeof(gmb_graph_pagerank_t), sb_cmp_pr_desc);
        int kk = arg3 > 0 ? arg3 : ((arg1 && *arg1) ? atoi(arg1) : 10);
        if (kk <= 0) kk = 10;
        int k = kk < l ? kk : l;
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"top\":[", bb); sb_app(&out, &len, &cap, tmp);
        for (int i = 0; i < k; i++) { char rn[64]; sb_num6(pr[i].rank, rn, sizeof(rn)); char nnb[4096]; sb_esc(pr[i].node ? pr[i].node : "", nnb, sizeof(nnb)); snprintf(tmp, sizeof(tmp), "%s{\"node\":\"%s\",\"rank\":%s}", i ? "," : "", nnb, rn); sb_app(&out, &len, &cap, tmp); }
        sb_app(&out, &len, &cap, "]}");
        free(pr);
    } else if (strcasecmp(op, "CONNECTED") == 0) {
        /* C# GraphMultiQuery.Connected: nodi ordinati; BFS su Neighbors (out+in, distinct,
         * ordine inserzione); componenti ordinate per taglia desc. */
        int use_active = (g_active_ne > 0 && strcasecmp(brain ? brain : "", g_active_graph_name) == 0);
        int ncnt = 0; char** ns = gmb_graph_nodes(h, &ncnt);
        char** sorted = (char**)malloc((size_t)(ncnt ? ncnt : 1) * sizeof(char*));
        for (int i = 0; i < ncnt; i++) sorted[i] = strdup(ns[i] ? ns[i] : "");
        qsort(sorted, (size_t)ncnt, sizeof(char*), sb_cmp_str);
        char** visited = (char**)calloc((size_t)(ncnt + 1), sizeof(char*)); int vn = 0;
        char*** comps = (char***)calloc((size_t)(ncnt + 1), sizeof(char**)); int* clens = (int*)calloc((size_t)(ncnt + 1), sizeof(int)); int ncomp = 0;
        for (int i = 0; i < ncnt; i++) {
            int seen = 0; for (int k = 0; k < vn; k++) if (strcmp(visited[k], sorted[i]) == 0) { seen = 1; break; }
            if (seen) continue;
            char** comp = (char**)calloc((size_t)(ncnt + 1), sizeof(char*)); int cn = 0;
            char** q = (char**)calloc((size_t)(ncnt + 1), sizeof(char*)); int qh = 0, qt = 0;
            q[qt++] = sorted[i]; visited[vn++] = strdup(sorted[i]);
            while (qh < qt) {
                char* cur = q[qh++]; comp[cn++] = cur;
                if (use_active) {
                    char* nb[4096]; int nbn = 0; sb_wiki_nb(cur, nb, &nbn);
                    for (int k = 0; k < nbn; k++) { int s2 = 0; for (int v = 0; v < vn; v++) if (strcmp(visited[v], nb[k]) == 0) { s2 = 1; break; } if (!s2) { visited[vn++] = strdup(nb[k]); q[qt++] = nb[k]; } }
                } else {
                    int nl = 0; gmb_graph_edge_t* ne2 = gmb_graph_neighbors(h, cur, &nl);
                    for (int k = 0; k < nl; k++) { const char* to = ne2[k].node; int s2 = 0; for (int v = 0; v < vn; v++) if (strcmp(visited[v], to) == 0) { s2 = 1; break; } if (!s2) { visited[vn++] = strdup(to); q[qt++] = (char*)to; } }
                    free(ne2);
                }
            }
            free(q);
            comps[ncomp] = comp; clens[ncomp] = cn; ncomp++;
        }
        /* ordina le componenti per taglia desc (stable: a parità mantiene l'ordine). */
        for (int i = 0; i < ncomp; i++) for (int j = i + 1; j < ncomp; j++) if (clens[j] > clens[i]) { char** tc = comps[i]; comps[i] = comps[j]; comps[j] = tc; int tl = clens[i]; clens[i] = clens[j]; clens[j] = tl; }
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"componentCount\":%d,\"components\":[", bb, ncomp); sb_app(&out, &len, &cap, tmp);
        for (int i = 0; i < ncomp; i++) { if (i) sb_app(&out, &len, &cap, ","); sb_app_strarr(&out, &len, &cap, comps[i], clens[i]); }
        sb_app(&out, &len, &cap, "]}");
        for (int i = 0; i < ncomp; i++) free(comps[i]);
        free(comps); free(clens);
        for (int i = 0; i < ncnt; i++) free(sorted[i]);
        free(sorted);
        for (int i = 0; i < vn; i++) free(visited[i]);
        free(visited);
        free(ns);
    } else if (strcasecmp(op, "TOPOSORT") == 0) {
        int l = 0; char** p = gmb_graph_toposort(h, &l);
        if (p == NULL) {
            char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Error\",\"brain\":\"%s\",\"error\":\"il grafo contiene un ciclo\"}", bb);
            out = strdup(tmp);
        } else {
            size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
            char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"order\":", bb); sb_app(&out, &len, &cap, tmp);
            sb_app_strarr(&out, &len, &cap, p, l);
            sb_app(&out, &len, &cap, "}");
            free(p);
        }
    } else if (strcasecmp(op, "HUBS") == 0) {
        /* HUBS: nodi più centrali per GRADO (out+in) — parità del tool `graph.hubs` del
         * managed. Prima non esisteva: il tool graph.* sul motore C rispondeva "non
         * disponibile". k = arg3 (default 10). */
        int ncnt = 0; char** ns = gmb_graph_nodes(h, &ncnt);
        typedef struct { const char* node; int deg; } sb_hub_t;
        sb_hub_t* v = ns ? (sb_hub_t*)calloc((size_t)(ncnt ? ncnt : 1), sizeof(sb_hub_t)) : NULL;
        if (v) {
            for (int i = 0; i < ncnt; i++) {
                v[i].node = ns[i];
                v[i].deg = gmb_graph_get_outdegree(h, ns[i]) + gmb_graph_get_indegree(h, ns[i]);
            }
            for (int i = 0; i < ncnt; i++)
                for (int j = i + 1; j < ncnt; j++)
                    if (v[j].deg > v[i].deg || (v[j].deg == v[i].deg && strcmp(v[j].node, v[i].node) < 0)) {
                        sb_hub_t t = v[i]; v[i] = v[j]; v[j] = t;
                    }
            int kk = arg3 > 0 ? arg3 : ((arg1 && *arg1) ? atoi(arg1) : 10);
            if (kk <= 0) kk = 10;
            int k = kk < ncnt ? kk : ncnt;
            size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
            char tmp[8192];
            snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"hubs\":[", bb);
            sb_app(&out, &len, &cap, tmp);
            for (int i = 0; i < k; i++) {
                char nnb[4096]; sb_esc(v[i].node ? v[i].node : "", nnb, sizeof(nnb));
                snprintf(tmp, sizeof(tmp), "%s{\"node\":\"%s\",\"degree\":%d}", i ? "," : "", nnb, v[i].deg);
                sb_app(&out, &len, &cap, tmp);
            }
            sb_app(&out, &len, &cap, "]}");
            free(v);
        }
        free(ns);
    } else if ((strcasecmp(op, "BFS") == 0 || strcasecmp(op, "DFS") == 0) && arg1) {
        int l = 0; char** p = strcasecmp(op, "BFS") == 0 ? gmb_graph_bfs(h, arg1, &l) : gmb_graph_dfs(h, arg1, &l);
        char rb[4096]; sb_esc(arg1, rb, sizeof(rb));
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"root\":\"%s\",\"order\":", bb, rb); sb_app(&out, &len, &cap, tmp);
        sb_app_strarr(&out, &len, &cap, p, l);
        sb_app(&out, &len, &cap, "}");
        free(p);
    } else if (strcasecmp(op, "DIJKSTRA") == 0 && arg1 && arg2) {
        int l = 0; char** p = gmb_graph_dijkstra_path(h, arg1, arg2, &l);
        char fb[4096], tb[4096]; sb_esc(arg1, fb, sizeof(fb)); sb_esc(arg2, tb, sizeof(tb));
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"path\":", bb, fb, tb); sb_app(&out, &len, &cap, tmp);
        sb_app_strarr(&out, &len, &cap, p, l);
        sb_app(&out, &len, &cap, "}");
        free(p);
    } else if (strcasecmp(op, "HASCYCLE") == 0) {
        out = strdup(gmb_graph_has_cycle(h) ? "1" : "0");
    } else if (strcasecmp(op, "ECCENTRICITY") == 0 && arg1) {
        char rn[64]; sb_num6(gmb_graph_eccentricity(h, arg1), rn, sizeof(rn));
        char nb[4096]; sb_esc(arg1, nb, sizeof(nb));
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"node\":\"%s\",\"eccentricity\":%s}", bb, nb, rn);
        out = strdup(tmp);
    } else if (strcasecmp(op, "BIPARTITE") == 0) {
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"bipartite\":%s}", bb, gmb_graph_is_bipartite(h) ? "true" : "false");
        out = strdup(tmp);
    } else if (strcasecmp(op, "BETWEENNESS") == 0) {
        int l = 0; gmb_graph_centrality_t* bt = gmb_graph_betweenness(h, &l);
        if (bt && l > 1) { for (int i = 1; i < l; i++) { gmb_graph_centrality_t key = bt[i]; int j = i - 1; while (j >= 0 && bt[j].value < key.value) { bt[j + 1] = bt[j]; j--; } bt[j + 1] = key; } }
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"values\":[", bb); sb_app(&out, &len, &cap, tmp);
        for (int i = 0; i < l; i++) { char vn[64]; sb_num6(bt[i].value, vn, sizeof(vn)); char nnb[4096]; sb_esc(bt[i].node ? bt[i].node : "", nnb, sizeof(nnb)); snprintf(tmp, sizeof(tmp), "%s{\"node\":\"%s\",\"value\":%s}", i ? "," : "", nnb, vn); sb_app(&out, &len, &cap, tmp); }
        sb_app(&out, &len, &cap, "]}");
        free(bt);
    } else if (strcasecmp(op, "CLUSTERING") == 0 && arg1) {
        char rn[64]; sb_num6(gmb_graph_clustering_coefficient(h, arg1), rn, sizeof(rn));
        char nb[4096]; sb_esc(arg1, nb, sizeof(nb));
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"node\":\"%s\",\"clustering\":%s}", bb, nb, rn);
        out = strdup(tmp);
    } else if (strcasecmp(op, "DIAMETER") == 0) {
        char rn[64]; sb_num6(gmb_graph_diameter(h), rn, sizeof(rn));
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"diameter\":%s}", bb, rn);
        out = strdup(tmp);
    } else if (strcasecmp(op, "SCC") == 0) {
        int use_active = (g_active_ne > 0 && strcasecmp(brain ? brain : "", g_active_graph_name) == 0);
        int ncnt = 0; char** ns = gmb_graph_nodes(h, &ncnt);
        int** adj = (int**)calloc((size_t)(ncnt ? ncnt : 1), sizeof(int*)); int* adjn = (int*)calloc((size_t)(ncnt ? ncnt : 1), sizeof(int));
        if (use_active) {
            for (int e = 0; e < g_active_ne; e++) {
                const char* f = g_active_edges[e].from; const char* t = g_active_edges[e].to;
                int a = -1, b = -1; for (int i = 0; i < ncnt; i++) { if (strcmp(ns[i], f) == 0) a = i; if (strcmp(ns[i], t) == 0) b = i; }
                if (a < 0 || b < 0) continue;
                int dup = 0; for (int k = 0; k < adjn[a]; k++) if (adj[a][k] == b) { dup = 1; break; }
                if (dup) continue;
                adj[a] = (int*)realloc(adj[a], (size_t)(adjn[a] + 1) * sizeof(int)); adj[a][adjn[a]++] = b;
            }
        } else {
            for (int i = 0; i < ncnt; i++) {
                int nl = 0; gmb_graph_edge_t* ne3 = gmb_graph_neighbors(h, ns[i], &nl);
                for (int k = nl - 1; k >= 0; k--) {
                    int b = -1; for (int j = 0; j < ncnt; j++) if (strcmp(ns[j], ne3[k].node) == 0) { b = j; break; }
                    if (b < 0) continue;
                    int dup = 0; for (int k2 = 0; k2 < adjn[i]; k2++) if (adj[i][k2] == b) { dup = 1; break; }
                    if (!dup) { adj[i] = (int*)realloc(adj[i], (size_t)(adjn[i] + 1) * sizeof(int)); adj[i][adjn[i]++] = b; }
                }
                free(ne3);
            }
        }
        int* idx = (int*)malloc((size_t)(ncnt + 1) * sizeof(int)); int* low = (int*)malloc((size_t)(ncnt + 1) * sizeof(int));
        int* onstk = (int*)calloc((size_t)(ncnt + 1), sizeof(int)); int* stk = (int*)malloc((size_t)(ncnt + 1) * sizeof(int));
        for (int i = 0; i < ncnt; i++) idx[i] = -1;
        char*** result = NULL; int resn = 0, rescap = 0, counter = 0, sp = 0;
        for (int i = 0; i < ncnt; i++) if (idx[i] == -1) sb_scc_dfs(i, adj, adjn, idx, low, onstk, stk, &sp, &counter, ns, &result, &resn, &rescap);
        size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
        char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"count\":%d,\"components\":[", bb, resn); sb_app(&out, &len, &cap, tmp);
        for (int i = 0; i < resn; i++) { int ci = 0; while (result[i][ci]) ci++; if (i) sb_app(&out, &len, &cap, ","); sb_app_strarr(&out, &len, &cap, result[i], ci); }
        sb_app(&out, &len, &cap, "]}");
        for (int i = 0; i < resn; i++) { for (int k = 0; result[i][k]; k++) free(result[i][k]); free(result[i]); }
        free(result);
        for (int i = 0; i < ncnt; i++) free(adj[i]);
        free(adj); free(adjn); free(idx); free(low); free(onstk); free(stk);
    } else if (strcasecmp(op, "LOUVAIN") == 0) {
        /* Community detection (Louvain). Grafo = archi del brain ATTIVO/JSON (come le altre op)
         * ma con peso 1 per arco (parità ToCommonGraph del C#: AddEdge(...,1)). */
        int64_t gh = sb_unit_graph(h);
        if (!gh) out = strdup("{\"status\":\"Error\",\"error\":\"graph JSON non valido\"}");
        else {
            int cn = 0; gmb_graph_comp_t* cm = gmb_graph_louvain(gh, 1.0, &cn);
            int maxc = 0; for (int i = 0; i < cn; i++) if (cm[i].component + 1 > maxc) maxc = cm[i].component + 1;
            size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
            char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"count\":%d,\"communities\":[", bb, maxc); sb_app(&out, &len, &cap, tmp);
            for (int i = 0; i < cn; i++) { char nnb[4096]; sb_esc(cm[i].node ? cm[i].node : "", nnb, sizeof(nnb)); snprintf(tmp, sizeof(tmp), "%s{\"node\":\"%s\",\"community\":%d}", i ? "," : "", nnb, cm[i].component); sb_app(&out, &len, &cap, tmp); }
            sb_app(&out, &len, &cap, "]}");
            free(cm); gmb_graph_free(gh);
        }
    } else if (strcasecmp(op, "MAXFLOW") == 0 && arg1 && arg2) {
        /* Flusso massimo (Edmonds-Karp). Ar capacità 1 (peso 1 per arco, parità ToCommonGraph). */
        int64_t gh = sb_unit_graph(h);
        if (!gh) out = strdup("{\"status\":\"Error\",\"error\":\"graph JSON non valido\"}");
        else {
            double fl = gmb_graph_maxflow(gh, arg1, arg2);
            char fb[4096], tb[4096]; sb_esc(arg1, fb, sizeof(fb)); sb_esc(arg2, tb, sizeof(tb));
            char fn[64]; sb_num6(fl, fn, sizeof(fn));
            size_t cap = 512, len = 0; out = (char*)malloc(cap); out[0] = 0;
            char tmp[8192]; snprintf(tmp, sizeof(tmp), "{\"status\":\"Success\",\"brain\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"flow\":%s}", bb, fb, tb, fn);
            sb_app(&out, &len, &cap, tmp);
            gmb_graph_free(gh);
        }
    } else if (strcasecmp(op, "DOT") == 0 || strcasecmp(op, "SUBGRAPH") == 0
               || strcasecmp(op, "BYTAG") == 0 || strcasecmp(op, "BYFOLDER") == 0
               || strcasecmp(op, "MST") == 0) {
        out = sb_graph_export_op(js, brain, op, arg1, arg2);
    } else {
        out = strdup("{\"status\":\"Error\",\"error\":\"op non supportata\"}");
    }
    if (!use_active) gmb_graph_free(h);
    free(js);
    return out;
}

/* ── LINK / FOLDER (Second Brain L3): LinksOut/Backlinks/Dangling/LinkJson/FolderPath.
   Operano su note_links nel vaults.db (da_path/to_path/alias). JSON generato a mano. ── */
static void sb_append_plain(char** o, size_t* l, size_t* c, const char* s);

static void sb_json_escape_append(char** o, size_t* l, size_t* c, const char* s)
{
    if (!s) { sb_append_plain(o, l, c, "null"); return; }
    char buf[16]; (void)buf;
    size_t n = strlen(s);
    if (*l + n * 2 + 3 > *c) { *c = (*l + n * 2 + 3) * 2; *o = (char*)realloc(*o, *c); }
    (*o)[(*l)++] = '"';
    for (size_t i = 0; i < n; i++)
    {
        char ch = s[i];
        if (ch == '"' || ch == '\\') { (*o)[(*l)++] = '\\'; (*o)[(*l)++] = ch; }
        else if (ch == '\n') { (*o)[(*l)++] = '\\'; (*o)[(*l)++] = 'n'; }
        else (*o)[(*l)++] = ch;
    }
    (*o)[(*l)++] = '"';
    (*o)[*l] = '\0';
}
static void sb_append_plain(char** o, size_t* l, size_t* c, const char* s)
{
    size_t n = s ? strlen(s) : 0;
    if (*l + n + 1 > *c) { size_t nc = (*l + n + 1) * 2; if (nc < 64) nc = 64; *o = (char*)realloc(*o, nc); *c = nc; }
    if (s) { memcpy(*o + *l, s, n); *l += n; }
    (*o)[*l] = '\0';
}

char* gmb_note_folder_path(const char* vault, const char* path)
{
    /* PARITÀ IL (divergenza trovata dal MiniTest gen_AI_NoteFolder, 2026-09-15): in C# questa
     * funzione è `NoteManager.FolderPath(vault, path)` ed è PURA sul path — non tocca il DB.
     * Qui invece si esigeva un vault registrato (`sb_vault_id < 0 → NULL`): con un vault
     * inesistente la VM/C/C99Native tornavano "" mentre IL tornava "/" (l'oracolo). Il vault
     * non serve al calcolo: lo si ignora, come fa il C#. */
    (void)vault;
    if (!path) return NULL;
    const char* slash = strrchr(path, '/');
    if (!slash) return strdup("/");   /* C# NoteManager.FolderPath: i>0 ? p[..i] : "/" */
    size_t n = (size_t)(slash - path);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, path, n); out[n] = '\0';
    return out;
}

/* out_links=1 per LINKSOUT (From = path, To = il link), 0 per BACKLINKS (From = chi linka,
 * To = path). Shape JSON = quella di IL LinkRecord.DTO: {"From","To","Alias","Heading",
 * "Resolved"} (PascalCase; Heading sempre null: il C non memorizza le ancore #heading). */
static char* sb_links_by_column(const char* vault, const char* path, const char* col, const char* other, int out_links)
{
    sqlite3* db = sb_open(); if (!db) return NULL;
    long long vid = sb_vault_id(db, vault);
    /* PARITÀ IL (divergenza trovata dai MiniTest gen_AI_LinksOut/LinkJson, 2026-09-15): con un
     * vault NON registrato il C# (`LinkManager.LinksOut`/`All`) serializza una lista VUOTA →
     * `[]`; qui si tornava NULL → il C99Native stampava "" e il minitest falliva. Una query su
     * un vault inesistente è "nessun link", non un errore. */
    if (vid < 0) { sqlite3_close(db); return strdup("[]"); }
    char q[256];
    snprintf(q, sizeof(q), "SELECT %s, alias FROM note_links WHERE vault_id=$1 AND %s=$2 ORDER BY %s;", col, other, col);
    sqlite3_stmt* st = NULL;
    size_t len = 0, cap = 0;
    char* out = NULL;
    sb_append_plain(&out, &len, &cap, "[");
    if (sqlite3_prepare_v2(db, q, -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, path ? path : "", -1, SQLITE_TRANSIENT);
        int first = 1;
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* tgt = (const char*)sqlite3_column_text(st, 0);
            const char* al = (const char*)sqlite3_column_text(st, 1);
            if (!first) sb_append_plain(&out, &len, &cap, ",");
            first = 0;
            /* Shape IL LinkRecord.DTO: {"From","To","Alias","Heading","Resolved"}
             * (era {"to_path":…,"alias":…}: divergenza da IL trovata dal MiniTest
             * ai_linkjson_graph). Heading sempre null: il C non memorizza #heading. */
            {
                const char* from_s = out_links ? (path ? path : "") : (tgt ? tgt : "");
                const char* to_s   = out_links ? (tgt ? tgt : "") : (path ? path : "");
                char fullx[2048];
                int resolved = sb_resolve(db, vault, to_s, fullx, sizeof(fullx)) && access(fullx, R_OK) == 0;
                sb_append_plain(&out, &len, &cap, "{\"From\":");
                sb_json_escape_append(&out, &len, &cap, from_s);
                sb_append_plain(&out, &len, &cap, ",\"To\":");
                sb_json_escape_append(&out, &len, &cap, to_s);
                sb_append_plain(&out, &len, &cap, ",\"Alias\":");
                sb_json_escape_append(&out, &len, &cap, al);
                sb_append_plain(&out, &len, &cap, ",\"Heading\":null,\"Resolved\":");
                sb_append_plain(&out, &len, &cap, resolved ? "true" : "false");
                sb_append_plain(&out, &len, &cap, "}");
            }
        }
        sqlite3_finalize(st);
    }
    sb_append_plain(&out, &len, &cap, "]");
    sqlite3_close(db);
    return out;
}

char* gmb_note_links_out_json(const char* vault, const char* path)
{
    return sb_links_by_column(vault, path, "to_path", "from_path", 1);
}
char* gmb_note_backlinks_json(const char* vault, const char* path)
{
    return sb_links_by_column(vault, path, "from_path", "to_path", 0);
}

char* gmb_links_all_json(const char* vault)
{
    sqlite3* db = sb_open(); if (!db) return NULL;
    long long vid = sb_vault_id(db, vault);
    /* Vedi sb_links_by_column: vault inesistente → lista vuota `[]` (parità IL). */
    if (vid < 0) { sqlite3_close(db); return strdup("[]"); }
    sqlite3_stmt* st = NULL;
    size_t len = 0, cap = 0;
    char* out = NULL;
    sb_append_plain(&out, &len, &cap, "[");
    if (sqlite3_prepare_v2(db, "SELECT from_path,to_path,alias FROM note_links WHERE vault_id=$1 ORDER BY from_path,to_path;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        int first = 1;
        while (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* f = (const char*)sqlite3_column_text(st, 0);
            const char* t = (const char*)sqlite3_column_text(st, 1);
            const char* a = (const char*)sqlite3_column_text(st, 2);
            if (!first) sb_append_plain(&out, &len, &cap, ",");
            first = 0;
            sb_append_plain(&out, &len, &cap, "{\"from\":");
            sb_json_escape_append(&out, &len, &cap, f ? f : "");
            sb_append_plain(&out, &len, &cap, ",\"to\":");
            sb_json_escape_append(&out, &len, &cap, t ? t : "");
            sb_append_plain(&out, &len, &cap, ",\"alias\":");
            sb_json_escape_append(&out, &len, &cap, a);
            sb_append_plain(&out, &len, &cap, "}");
        }
        sqlite3_finalize(st);
    }
    sb_append_plain(&out, &len, &cap, "]");
    sqlite3_close(db);
    return out;
}

long gmb_links_dangling_count(const char* vault)
{
    sqlite3* db = sb_open(); if (!db) return 0;
    long long vid = sb_vault_id(db, vault);
    long dangling = 0;
    if (vid >= 0)
    {
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(db, "SELECT DISTINCT to_path FROM note_links WHERE vault_id=$1;", -1, &st, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int64(st, 1, vid);
            char full[2048];
            while (sqlite3_step(st) == SQLITE_ROW)
            {
                const char* t = (const char*)sqlite3_column_text(st, 0);
                if (!t || !*t) continue;
                if (sb_resolve(db, vault, t, full, sizeof(full)) && access(full, R_OK) != 0) dangling++;
            }
            sqlite3_finalize(st);
        }
    }
    sqlite3_close(db);
    return dangling;
}

/* ── VAULT POLICY / WATCH + BRAIN USE/DETACH (persistenza VM/C su vaults.db).
 * Disponibilità su disco (parità C# BrainRegistry.AvailableModes): rag db
 * (<brains>/<n>/rag/<n>.db o legacy <rag>/<n>.db) o graph json
 * (<brains>/<n>/graphs/<n>.json o legacy <graphs>/<n>.json). */
static int sb_file_is_reg(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
static int sb_brain_available_on_disk(const char* name) {
    if (!name || !*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) return 0;
    /* Prima il RESOLVER (vendor → exe → brainsDir → cache ~/.gmbasic/brains/<nome>/<versione>):
     * un cervello installato da `AI BRAIN INSTALL` vive nella cache versionata e con i soli
     * path fissi qui sotto non veniva riconosciuto ("Use" falliva su un cervello appena
     * installato). */
    {
        char dir[4096];
        if (gmb_brain_lookup(name, NULL, dir, sizeof dir)) return 1;
    }
    char brains[1024], graphs[1024], p[2048];
    gmb_paths_brains_dir(brains, sizeof(brains));
    gmb_paths_graphs_dir(graphs, sizeof(graphs));
    snprintf(p, sizeof(p), "%s/%s/rag/%s.db", brains, name, name);
    if (sb_file_is_reg(p)) return 1;
    snprintf(p, sizeof(p), "%s/%s/graphs/%s.json", brains, name, name);
    if (sb_file_is_reg(p)) return 1;
    snprintf(p, sizeof(p), "%s/%s.json", graphs, name);
    if (sb_file_is_reg(p)) return 1;
    return 0;
}
/* Il C non ha il FileSystemWatcher .NET: la policy AUTO/MANUAL e l'attach dei
   brain sono metadati salvati in tabelle dedicate (non rompono lo schema C#). */
int gmb_vault_watch(const char* name, const char* policy)
{
    if (!name) return -1;
    sqlite3* db = sb_open(); if (!db) return -1;
    const char* pol = (policy && strcasecmp(policy, "AUTO") == 0) ? "AUTO" : "MANUAL";
    /* Parità con l'oracolo (FileWatcher.SetPolicy): la policy si imposta SEMPRE e si ritorna **1**
     * (la C# chiude il watcher e ritorna 1 anche per un vault sconosciuto; lancia solo su
     * argomenti/percorsi invalidi). Il runtime C pretendeva il vault esistente e ritornava -1/0:
     * due divergenze (gen_AI_VaultWatch, EXPECT 1). La persistenza avviene solo per vault noti. */
    long long vid = sb_vault_id(db, name);
    if (vid < 0) { sqlite3_close(db); return 1; }
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "INSERT INTO vault_policy(vault_id,policy) VALUES($1,$2)"
            " ON CONFLICT(vault_id) DO UPDATE SET policy=$2;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        sqlite3_bind_text(st, 2, pol, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return 1;
}
char* gmb_vault_policy(const char* name)
{
    if (!name) return strdup("MANUAL");
    sqlite3* db = sb_open(); if (!db) return strdup("MANUAL");
    long long vid = sb_vault_id(db, name);
    if (vid < 0) { sqlite3_close(db); return strdup("MANUAL"); }   /* vault sconosciuto → politica di default (parità C#/IL) */
    char pol[16] = "MANUAL";
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT policy FROM vault_policy WHERE vault_id=$1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, vid);
        if (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* p = (const char*)sqlite3_column_text(st, 0);
            if (p) snprintf(pol, sizeof(pol), "%s", p);
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return strdup(pol);
}
/* ── Sincronizzazione col registro C# (config/brains.json) ─────────────────────────────
 * MOTIVO (divergenza A1, 2026-09-16): il C# tiene i cervelli in `<config>/brains.json`
 * (`{"attached":[…],"brains":[{"name":…}]}`) e il C in `<config>/vaults.db` (tabella
 * `brain_attach`). Senza sincronizzazione lo STESSO programma BASIC vedeva cervelli diversi
 * per gamba (`AI.BRAINUSED`: airline,trading su IL/PCode vs airline su C). Qui il C:
 *   1) LEGGE `brains.json` (già fatto da gmb_brain_names_from_json);
 *   2) fa l'UNIONE della propria tabella con l'elenco `attached` del JSON;
 *   3) MIRROR su `attached` quando si fa USE/DETACH, così il C# vede le operazioni fatte in C.
 * Il JSON è un file di CONFIGURAZIONE condiviso (non un ponte in-process: nessuna chiamata fra i
 * due mondi — §0bis). L'aggiornamento è testuale e CIRCOSCRITTO all'array `attached`: il resto del
 * file (nomi, mode, checksum, probe, …) resta byte-identico.
 * ───────────────────────────────────────────────────────────────────────────────────── */
static int sb_json_has_name(const char* name);   /* forward: cerca un nome in brains[]/attached */

/* Aggiunge/rimuove un nome nell'array `attached` di brains.json (mirror). Ritorna 1 se il file
 * è stato aggiornato, 0 se non c'era nulla da fare / file assente. Scrittura atomica (tmp+rename). */
static int sb_brains_json_mirror_attach(const char* name, int add)
{
    if (!name || !*name) return 0;
    char path[1024]; gmb_paths_brains_json(path, sizeof(path));
    FILE* f = fopen(path, "r");
    if (!f) return 0;                                  /* nessun registro C#: niente da sincronizzare */
    char* buf = (char*)malloc(262144);
    if (!buf) { fclose(f); return 0; }
    size_t len = fread(buf, 1, 262143, f); buf[len] = 0; fclose(f);

    char* att = strstr(buf, "\"attached\"");
    char* lb = att ? strchr(att, '[') : NULL;
    char* rb = lb ? strchr(lb, ']') : NULL;
    if (!lb || !rb) { free(buf); return 0; }

    /* legge i nomi correnti */
    char cur[256][64]; int n = 0;
    for (char* p = lb; (p = strchr(p, '"')) && p < rb; ) {
        char* e = strchr(p + 1, '"');
        if (!e || e > rb) break;
        size_t l = (size_t)(e - p - 1);
        if (l > 0 && l < 64 && n < 256) { memcpy(cur[n], p + 1, l); cur[n][l] = 0; n++; }
        p = e + 1;
    }
    int found = 0;
    for (int i = 0; i < n; i++) if (strcmp(cur[i], name) == 0) { found = 1; break; }
    if (add && found) { free(buf); return 0; }          /* già presente */
    if (!add && !found) { free(buf); return 0; }        /* già assente */

    /* ricostruisce l'array */
    char arr[8192]; size_t alen = 0;
    arr[0] = 0;
    int wrote = 0;
    for (int i = 0; i < n; i++) {
        if (!add && strcmp(cur[i], name) == 0) continue;
        alen += (size_t)snprintf(arr + alen, sizeof(arr) - alen, "%s\"%s\"", wrote ? ", " : "", cur[i]);
        wrote = 1;
    }
    if (add && !found) {
        alen += (size_t)snprintf(arr + alen, sizeof(arr) - alen, "%s\"%s\"", wrote ? ", " : "", name);
        wrote = 1;
    }

    /* sostituisce il contenuto fra '[' e ']' (esclusi) */
    size_t head = (size_t)(lb - buf) + 1;
    size_t tail = (size_t)(rb - buf);
    char* out = (char*)malloc(head + alen + strlen(buf + tail) + 4);
    if (!out) { free(buf); return 0; }
    memcpy(out, buf, head);
    memcpy(out + head, arr, alen);
    strcpy(out + head + alen, buf + tail);
    free(buf);

    char tmp[1100]; snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE* o = fopen(tmp, "w");
    if (!o) { free(out); return 0; }
    size_t olen = strlen(out);
    size_t w = fwrite(out, 1, olen, o);
    fclose(o); free(out);
    if (w != olen) { remove(tmp); return 0; }
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
    return 1;
}

/* True se il nome è nel registro C# (in `brains[].name` o in `attached`). */
static int sb_json_has_name(const char* name)
{
    if (!name || !*name) return 0;
    char path[1024]; gmb_paths_brains_json(path, sizeof(path));
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char* buf = (char*)malloc(262144);
    if (!buf) { fclose(f); return 0; }
    size_t len = fread(buf, 1, 262143, f); buf[len] = 0; fclose(f);
    char pat[128]; snprintf(pat, sizeof(pat), "\"%s\"", name);
    int found = strstr(buf, pat) != NULL;
    free(buf);
    return found;
}

int gmb_brain_use(const char* names)
{
    if (!names) return 0;
    sqlite3* db = sb_open(); if (!db) return 0;
    int used = 0;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", names);
    for (char* tok = strtok(tmp, ","); tok; tok = strtok(NULL, ","))
    {
        char* t = tok;
        while (*t == ' ') t++;
        size_t tl = strlen(t);
        while (tl > 0 && t[tl - 1] == ' ') t[--tl] = 0;
        if (!*t) continue;
        /* Parità col C# (BrainRegistry.Use): si collega un cervello DISPONIBILE, cioè presente nel
         * vault store C **oppure** nel registro C# (brains.json: un cervello costruito/registrato
         * dal C# — es. `gmb rag build`/`BRAIN CREATE` — deve poter essere collegato anche da C)
         * **oppure** disponibile su disco come file (C#: AvailableModes = rag db o graph json
         * esistenti — es. `rag/trading.db` legacy senza riga di registro: BrainMultiBrain). */
        int known = sb_vault_id(db, t) >= 0 || sb_json_has_name(t) || sb_brain_available_on_disk(t);
        if (!known) continue;
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(db, "INSERT INTO brain_attach(name) VALUES($1)"
                " ON CONFLICT(name) DO NOTHING;", -1, &st, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(st, 1, t, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_DONE) used++;
            sqlite3_finalize(st);
        }
        sb_brains_json_mirror_attach(t, 1);   /* MIRROR: il C# vede il collegamento fatto in C */
    }
    sqlite3_close(db);
    return used;
}
int gmb_brain_detach(const char* name)
{
    if (!name || !*name) return 0;   /* C# BrainRegistry.Detach("") -> false */
    sqlite3* db = sb_open(); if (!db) return 0;
    int rc = 0;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM brain_attach WHERE name=$1;", -1, &st, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(st) == SQLITE_DONE ? 1 : 0;
        sqlite3_finalize(st);
        if (rc) rc = sqlite3_changes(db) > 0 ? 1 : 0;   /* solo se ha rimosso davvero */
    }
    sqlite3_close(db);
    if (rc) sb_brains_json_mirror_attach(name, 0);   /* MIRROR: il C# vede lo scollegamento fatto in C */
    return rc;
}
char* gmb_brain_used_csv(void)
{
    /* UNIONE dei cervelli collegati: tabella C `brain_attach` ∪ `attached` di brains.json (registro
     * C#). È il fix della divergenza A1 (2026-09-16): prima `AI.BRAINUSED` mostrava solo il lato C e
     * lo stesso programma BASIC vedeva cervelli diversi per gamba. Nomi unici, ordinati. */
    char names[256][64]; int n = 0;
    sqlite3* db = sb_open();
    if (db) {
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(db, "SELECT name FROM brain_attach ORDER BY name;", -1, &st, NULL) == SQLITE_OK)
        {
            while (sqlite3_step(st) == SQLITE_ROW)
            {
                const char* nm = (const char*)sqlite3_column_text(st, 0);
                if (nm && *nm && n < 256) { snprintf(names[n], sizeof(names[n]), "%s", nm); n++; }
            }
            sqlite3_finalize(st);
        }
        sqlite3_close(db);
    }
    char path[1024]; gmb_paths_brains_json(path, sizeof(path));
    FILE* f = fopen(path, "r");
    if (f) {
        char* buf = (char*)malloc(262144);
        if (buf) {
            size_t len = fread(buf, 1, 262143, f); buf[len] = 0; fclose(f);
            char* att = strstr(buf, "\"attached\"");
            char* lb = att ? strchr(att, '[') : NULL;
            char* rb = lb ? strchr(lb, ']') : NULL;
            if (lb && rb) {
                for (char* p = lb; (p = strchr(p, '"')) && p < rb; ) {
                    char* e = strchr(p + 1, '"');
                    if (!e || e > rb) break;
                    size_t l = (size_t)(e - p - 1);
                    if (l > 0 && l < 64 && n < 256) { memcpy(names[n], p + 1, l); names[n][l] = 0; n++; }
                    p = e + 1;
                }
            }
            free(buf);
        } else fclose(f);
    }
    /* dedup + ordinamento (parità con gmb_brain_names_from_json) */
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (names[i][0] && names[j][0] && strcasecmp(names[i], names[j]) == 0) names[j][0] = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (names[i][0] && names[j][0] && strcasecmp(names[i], names[j]) > 0) {
            char tmp[64]; memcpy(tmp, names[i], 64); memcpy(names[i], names[j], 64); memcpy(names[j], tmp, 64);
        }
    size_t cap = 64, len = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (!names[i][0]) continue;
        size_t need = strlen(names[i]) + (len ? 1 : 0);
        if (len + need + 1 > cap) { cap = (len + need + 1) * 2; char* r = realloc(out, cap); if (!r) break; out = r; }
        if (len) out[len++] = ',';
        memcpy(out + len, names[i], strlen(names[i])); len += strlen(names[i]);
        out[len] = '\0';
    }
    return out;
}

/* ════════════════════════════════════════════════════════════════════════
 * ── INGEST (AI INGEST FILE/FOLDER, porting Ingestor — §0bis) ──
 * Importa file esterni in un vault come nota .md:
 *   .md/.markdown/.txt → copia del testo (kind "text")
 *   immagine            → OCR via backend (kind "ocr")   — richiede provider %OCR attivo
 *   audio               → trascrizione (kind "audio")    — richiede provider %AUDIO attivo
 *   altri binari        → converter registrato (kind "convert", es. gmb_doc_convert "md")
 * Il vault "" (brain attivo) risolve al primo brain_attach (gmb_brain_use). Mai stub:
 * nessun provider/converter → errore esplicito (ritorno NULL / -1).
 * ════════════════════════════════════════════════════════════════════════ */

static gmb_ingest_converter_fn g_ingest_converter = NULL;

void gmb_ingest_register_converter(gmb_ingest_converter_fn fn) { g_ingest_converter = fn; }

static const char* sb_ext_lower(const char* path)
{
    if (!path) return "";
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path || strchr(dot, '/')) return "";
    static __thread char ext[16];
    size_t i = 0;
    for (const char* p = dot + 1; *p && i < sizeof(ext) - 1; p++) ext[i++] = (char)tolower((unsigned char)*p);
    ext[i] = '\0';
    return ext;
}

static int sb_ext_is(const char* path, const char* a, const char* b, const char* c)
{
    const char* e = sb_ext_lower(path);
    return (a && strcmp(e, a) == 0) || (b && strcmp(e, b) == 0) || (c && strcmp(e, c) == 0);
}

/* Nome del vault risolto. Precedenza (PARITÀ C# NoteManager.VaultRoot — divergenza A2, 2026-09-16):
 *   1. nome esplicito;
 *   2. `g_sb_active_vault` = l'ultimo vault creato nella sessione (equivalente di `VaultManager.LastCreated`
 *      del C#, impostato da AI VAULT CREATE);
 *   3. **l'UNICO** cervello collegato (unione C ∪ brains.json);
 *   4. altrimenti 0 = NESSUN default: il chiamante restituisce un ERRORE ESPLICITO (il C# lancia
 *      "vault ambiguo: N cervelli nello scope …"). Prima qui si sceglieva il PRIMO collegato in ordine
 *      alfabetico: scelta implicita, non deterministica dal punto di vista dell'utente. Ritorna 1 se ok. */
static int sb_ingest_vault_name(sqlite3* db, const char* name, char* out, size_t cap)
{
    if (name && *name) { snprintf(out, cap, "%s", name); return 1; }
    if (g_sb_active_vault[0]) { snprintf(out, cap, "%s", g_sb_active_vault); return 1; }
    /* unico collegato: riusa la stessa lista di AI.BRAINUSED (tabella C ∪ attached di brains.json) */
    char* used = gmb_brain_used_csv();
    if (used) {
        char tmp[512]; snprintf(tmp, sizeof(tmp), "%s", used);
        free(used);
        int n = 0; char* only = NULL;
        for (char* tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
            while (*tok == ' ') tok++;
            if (!*tok) continue;
            n++; only = tok;
        }
        if (n == 1) { snprintf(out, cap, "%s", only); return 1; }
    }
    return 0;   /* nessun default: il chiamante emette l'errore esplicito (mai scelta implicita) */
}

/* Lettura file intero in heap (niente limite fisso): ritorna buffer (free) o NULL. */
static char* sb_read_file_alloc(const char* path, long* out_len)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = (long)rd;
    return buf;
}

/* Prima riga "# titolo" del body (parità NoteManager.ExtractTitle) o NULL. */
static char* sb_extract_h1(const char* content)
{
    if (!content) return NULL;
    const char* p = content;
    while (*p)
    {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char* line = (char*)malloc(len + 1);
        if (!line) return NULL;
        memcpy(line, p, len); line[len] = '\0';
        char* t = line;
        while (*t == ' ' || *t == '\t') t++;
        if (t[0] == '#' && t[1] == ' ')
        {
            char* h = t + 2;
            while (*h == ' ') h++;
            char* out = strdup(h);
            free(line);
            return out;
        }
        free(line);
        if (!nl) break;
        p = nl + 1;
    }
    return NULL;
}

/* Frontmatter come NoteManager.EnsureFrontmatter: se il contenuto inizia con "---"/"+++" non si
 * tocca; altrimenti antepone title (primo H1 o nome file) + tags opzionali. Ritorna heap. */
static char* sb_ingest_frontmatter(const char* content, const char* rel_path, const char* tags)
{
    if (!content) return strdup("");
    if (strncmp(content, "---", 3) == 0 || strncmp(content, "+++", 3) == 0) return strdup(content);
    char* title = sb_extract_h1(content);
    if (!title)
    {
        const char* base = strrchr(rel_path, '/'); base = base ? base + 1 : rel_path;
        const char* dot = strrchr(base, '.');
        size_t bl = dot ? (size_t)(dot - base) : strlen(base);
        title = (char*)malloc(bl + 1);
        if (title) { memcpy(title, base, bl); title[bl] = '\0'; }
    }
    if (!title) return strdup(content);
    size_t need = strlen("---\ntitle: \"\"\n---\n") + strlen(title) + (tags ? strlen(tags) + 32 : 0) + strlen(content) + 16;
    char* out = (char*)malloc(need);
    if (!out) { free(title); return NULL; }
    char* o = out;
    o += sprintf(o, "---\ntitle: \"%s\"\n", title);
    free(title);
    if (tags && *tags)
    {
        /* "a,b" → tags: ["a", "b"] */
        char tmp[256]; snprintf(tmp, sizeof(tmp), "%s", tags);
        o += sprintf(o, "tags: [");
        char* tok = strtok(tmp, ",");
        int first = 1;
        while (tok)
        {
            char* t = tok; while (*t == ' ') t++;
            size_t tl = strlen(t); while (tl && t[tl-1] == ' ') t[--tl] = 0;
            o += sprintf(o, "%s\"%s\"", first ? "" : ", ", t);
            first = 0;
            tok = strtok(NULL, ",");
        }
        o += sprintf(o, "]\n");
    }
    o += sprintf(o, "---\n%s", content);
    return out;
}

/* Verifica che root/rel non esca dalla root (chroot guard, parità NoteManager.ResolvePath).
 * Su rel con "/" iniziale o segmenti ".." → fallisce. Ritorna il path pieno o NULL. */
static char* sb_ingest_target(const char* root, const char* rel)
{
    if (!root || !rel) return NULL;
    const char* r = rel;
    while (*r == '/' || *r == '\\') r++;
    if (!*r) return NULL;
    char* tmp = strdup(r);
    if (!tmp) return NULL;
    for (char* c = tmp; *c; c++) if (*c == '\\') *c = '/';
    /* segmenti ".." → fuori dalla root (validazione su COPIA: strtok muta il buffer) */
    char* copy = strdup(tmp);
    if (!copy) { free(tmp); return NULL; }
    char* seg = strtok(copy, "/");
    int bad = 0;
    while (seg)
    {
        if (strcmp(seg, "..") == 0) { bad = 1; break; }
        seg = strtok(NULL, "/");
    }
    free(copy);
    if (bad) { free(tmp); return NULL; }
    size_t rl = strlen(root);
    size_t al = strlen(tmp);
    char* full = (char*)malloc(rl + al + 2);
    if (!full) { free(tmp); return NULL; }
    snprintf(full, rl + al + 2, "%s/%s", root, tmp);
    free(tmp);
    return full;
}

char* gmb_ingest_import_file(const char* vault_name, const char* source_file, const char* rel_path, int ocr, int transcribe)
{
    if (!source_file) return NULL;
    if (access(source_file, R_OK) != 0) return NULL;          /* file non trovato */
    sqlite3* db = sb_open();
    if (!db) return NULL;
    char name[512];
    if (!sb_ingest_vault_name(db, vault_name, name, sizeof(name))) { sqlite3_close(db); return NULL; }
    char root[1024];
    if (!sb_vault_root(db, name, root, sizeof(root))) { sqlite3_close(db); return NULL; }
    sqlite3_close(db);

    char* content = NULL;
    if (sb_ext_is(source_file, "md", "markdown", "txt"))
    {
        content = sb_read_file_alloc(source_file, NULL);
        if (!content) return NULL;
    }
    else if (sb_ext_is(source_file, "png", "jpg", "jpeg") ||
             sb_ext_is(source_file, "gif", "webp", "bmp") ||
             sb_ext_is(source_file, "jpg", "jpeg", NULL))
    {
        if (!ocr) return NULL;                                 /* OCR disattivato → non supportato */
        if (!gmb_ai_ocr_active()) return NULL;                 /* nessun provider %OCR */
        const char* r = gmb_ai_ocr_read(source_file, "Extract all text from this image.", 0);
        if (!r || !r[0] || r[0] == '[') return NULL;           /* errore/nessun testo */
        content = strdup(r);
    }
    else if (sb_ext_is(source_file, "wav", "mp3", "m4a") ||
             sb_ext_is(source_file, "flac", "ogg", NULL))
    {
        if (!transcribe) return NULL;                          /* trascrizione disattivata */
        if (!gmb_ai_audio_active()) return NULL;               /* nessun provider %AUDIO */
        const char* r = gmb_ai_audio_transcribe(source_file, "", 0);
        if (!r || !r[0] || r[0] == '[') return NULL;
        content = strdup(r);
    }
    else
    {
        /* binario: converter registrato (delegate BinaryTextExtractor C#) */
        if (!g_ingest_converter) return NULL;
        char* c = g_ingest_converter(source_file, "md");
        if (!c || !c[0]) { if (c) free(c); return NULL; }
        content = c;
    }
    if (!content) return NULL;

    /* target: relPath ?? nome base con .md (parità Path.ChangeExtension) */
    char relbuf[2048];
    if (rel_path && *rel_path) snprintf(relbuf, sizeof(relbuf), "%s", rel_path);
    else
    {
        const char* base = strrchr(source_file, '/'); base = base ? base + 1 : source_file;
        char b[1024]; snprintf(b, sizeof(b), "%s", base);
        char* dot = strrchr(b, '.');
        if (dot) *dot = '\0';
        snprintf(relbuf, sizeof(relbuf), "%s.md", b);
    }
    char* full = sb_ingest_target(root, relbuf);
    if (!full) { free(content); return NULL; }                 /* path non valido (chroot) */
    char* text = sb_ingest_frontmatter(content, relbuf, NULL);
    free(content);
    if (!text) { free(full); return NULL; }
    int ok = sb_write_file(full, text);
    free(text);
    free(full);
    if (!ok) return NULL;
    return strdup(relbuf);
}

/* Walk ricorsivo di ImportFolder: rel preservato rispetto a src_folder. */
static long sb_ingest_folder_walk(const char* vault, const char* src_root, const char* rel_dir,
                                  int preserve, int ocr, int transcribe)
{
    char cur[2048];
    if (rel_dir && *rel_dir) snprintf(cur, sizeof(cur), "%s/%s", src_root, rel_dir);
    else snprintf(cur, sizeof(cur), "%s", src_root);
    DIR* d = opendir(cur);
    if (!d) return 0;
    long n = 0;
    struct dirent* e;
    while ((e = readdir(d)))
    {
        /* skip SOLO . e .. — i dotfile vanno inclusi (parità Directory.GetFiles AllDirectories C#) */
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char spath[2048];
        snprintf(spath, sizeof(spath), "%s/%s", cur, e->d_name);
        struct stat st;
        if (stat(spath, &st) != 0) continue;
        if (S_ISDIR(st.st_mode))
        {
            char rel[2048];
            if (rel_dir && *rel_dir) snprintf(rel, sizeof(rel), "%s/%s", rel_dir, e->d_name);
            else snprintf(rel, sizeof(rel), "%s", e->d_name);
            n += sb_ingest_folder_walk(vault, src_root, rel, preserve, ocr, transcribe);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        char rel[2048];
        if (rel_dir && *rel_dir) snprintf(rel, sizeof(rel), "%s/%s", rel_dir, e->d_name);
        else snprintf(rel, sizeof(rel), "%s", e->d_name);
        char* r = preserve
            ? gmb_ingest_import_file(vault, spath, rel, ocr, transcribe)
            : gmb_ingest_import_file(vault, spath, e->d_name, ocr, transcribe);
        if (r) { free(r); n++; }
    }
    closedir(d);
    return n;
}

long long gmb_ingest_import_folder(const char* vault_name, const char* src_folder, int preserve_structure, int ocr, int transcribe)
{
    if (!src_folder) return -1;
    struct stat st;
    if (stat(src_folder, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;   /* cartella mancante */
    sqlite3* db = sb_open();
    if (!db) return -1;
    char name[512];
    if (!sb_ingest_vault_name(db, vault_name, name, sizeof(name))) { sqlite3_close(db); return -1; }
    char root[1024];
    if (!sb_vault_root(db, name, root, sizeof(root))) { sqlite3_close(db); return -1; }
    sqlite3_close(db);
    return sb_ingest_folder_walk(name, src_folder, NULL, preserve_structure ? 1 : 0, ocr, transcribe);
}

/* ── LINKJSON (Second Brain L3): grafo completo come IL LinkManager.ToJson ──────────────
 * Shape ESATTA (oracolo C#): {"nodes":[{"id","title","frontmatter":{…}}],
 *                            "edges":[{"from","to","alias"}],
 *                            "dangling":["<from> → <to>"]}
 * Era il residuo di parità 2026-09-15: la tabella nativa mappava LINKJSON su
 * gmb_links_all_json (array PIATTO di link) → shape sbagliata su C99Native/VM/C.
 * Vault inesistente o senza note → ESATTAMENTE {"nodes":[],"edges":[],"dangling":[]} (IL non
 * è un errore: è "nessun contenuto"). */

/* Tutte le chiavi del frontmatter `--- … ---` come oggetto JSON ("{}" se assente).
 * Il runtime aveva solo sb_field() (UNA chiave): qui si enumerano le chiavi, altrimenti il
 * campo `frontmatter` divergerebbe silenziosamente dall'oracolo IL. YAML-lite, come IL:
 * `[a, b]` → array di stringhe; valore numerico puro → numero; altrimenti stringa. */
/* Rimuove le virgolette doppie di delimitazione YAML: IL scrive `title: "n1"` e i tag
 * `["a", "b"]`, e il parser C# RESTITUISCE il valore senza virgolette. Senza questo
 * strip il frontmatter C conteneva le virgolette (divergenza dal MiniTest). */
static void sb_unquote(char* s)
{
    if (!s) return;
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"')
    {
        memmove(s, s + 1, n - 2);
        s[n - 2] = '\0';
    }
}

static char* sb_frontmatter_json(const char* content)
{
    char* out = NULL; size_t len = 0, cap = 0;
    sb_append_plain(&out, &len, &cap, "{");
    int first = 1;
    if (content && strncmp(content, "---", 3) == 0) {
        const char* end = strstr(content + 3, "\n---");
        if (!end) end = strstr(content + 3, "---");
        if (end) {
            char* tmp = strndup(content + 3, (size_t)(end - content - 3));
            char* save = NULL;
            for (char* line = strtok_r(tmp, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
                char* colon = strchr(line, ':');
                if (!colon) continue;
                *colon = '\0';
                char* k = line; while (*k == ' ' || *k == '\t') k++;
                char* ke = k + strlen(k); while (ke > k && (ke[-1] == ' ' || ke[-1] == '\t')) *--ke = '\0';
                char* v = colon + 1; while (*v == ' ' || *v == '\t') v++;
                char* ve = v + strlen(v); while (ve > v && (ve[-1] == '\r' || ve[-1] == ' ' || ve[-1] == '\t')) *--ve = '\0';
                if (!*k) continue;
                if (!first) sb_append_plain(&out, &len, &cap, ",");
                first = 0;
                sb_json_escape_append(&out, &len, &cap, k);
                sb_append_plain(&out, &len, &cap, ":");
                if (*v == '[') {                       /* lista YAML inline → array di stringhe */
                    sb_append_plain(&out, &len, &cap, "[");
                    int f2 = 1; char* isave = NULL;
                    for (char* it = strtok_r(v + 1, ",", &isave); it; it = strtok_r(NULL, ",", &isave)) {
                        char* s = it; while (*s == ' ') s++;
                        char* e2 = s + strlen(s);
                        while (e2 > s && (e2[-1] == ']' || e2[-1] == ' ' || e2[-1] == '\r')) *--e2 = '\0';
                        if (!*s) continue;
                        sb_unquote(s);
                        if (!f2) sb_append_plain(&out, &len, &cap, ",");
                        f2 = 0;
                        sb_json_escape_append(&out, &len, &cap, s);
                    }
                    sb_append_plain(&out, &len, &cap, "]");
                } else {
                    /* PARITÀ IL: il FrontmatterParser C# restituisce SEMPRE una LISTA di valori
                     * (anche per uno scalare) e il valore è SENZA le virgolette YAML. */
                    sb_unquote(v);
                    sb_append_plain(&out, &len, &cap, "[");
                    sb_json_escape_append(&out, &len, &cap, v);
                    sb_append_plain(&out, &len, &cap, "]");
                }
            }
            free(tmp);
        }
    }
    sb_append_plain(&out, &len, &cap, "}");
    return out ? out : strdup("{}");
}

/* AI.LINKDANGLING come STRINGA: il codegen emitter C# (riga storica TokenKind.LinkDangling in
 * C99CodeEmitter.Builtin.Core.cs) emette `gmb_linkdangling(...)` e tratta il risultato come
 * stringa → la funzione MANCava e il C generato non compilava
 * ("passing 'gmb_double_t' to parameter of incompatible type 'gmb_string_t'", prog.c:182 del
 * MiniTest ai_linkjson_graph). Stesso valore di gmb_links_dangling_count, reso senza
 * decimali: il testo stampato coincide con quello di IL (che stampa il numero). */
char* gmb_linkdangling(const char* vault)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)gmb_links_dangling_count(vault));
    return strdup(buf);
}

/* nodes del grafo: enumerazione del FILESYSTEM come IL EnumerateNotes (non l'indice
 * vault_files: su C l'indice può essere vuoto — l'INDEX del BASIC non popola — e nodes
 * risultava [] mentre le note esistono su disco). Ricorsivo, pattern *.md. */
static int g_graph_nodes_first = 1;
static void sb_graph_nodes_json(char** out, size_t* len, size_t* cap,
                                const char* root, const char* rel, int* first)
{
    char dir[2048];
    if (*rel) snprintf(dir, sizeof(dir), "%s/%s", root, rel);
    else snprintf(dir, sizeof(dir), "%s", root);
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)))
    {
        if (e->d_name[0] == '.') continue;
        char child[2048];
        if (*rel) snprintf(child, sizeof(child), "%s/%s", rel, e->d_name);
        else snprintf(child, sizeof(child), "%s", e->d_name);
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", root, child);
        struct stat stx;
        if (stat(full, &stx) != 0) continue;
        if (S_ISDIR(stx.st_mode)) { sb_graph_nodes_json(out, len, cap, root, child, first); continue; }
        size_t nl = strlen(e->d_name);
        if (nl < 3 || strcasecmp(e->d_name + nl - 3, ".md") != 0) continue;
        static char buf[65536];
        if (!sb_read_file(full, buf, sizeof(buf))) continue;
        if (!*first) sb_append_plain(out, len, cap, ",");
        *first = 0;
        sb_append_plain(out, len, cap, "{\"id\":");
        sb_json_escape_append(out, len, cap, child);
        const char* t = sb_field(buf, "title");
        sb_append_plain(out, len, cap, ",\"title\":");
        if (t) { char tb[512]; snprintf(tb, sizeof(tb), "%s", t); sb_unquote(tb);
                 sb_json_escape_append(out, len, cap, tb); }
        else {
            char base[512]; snprintf(base, sizeof(base), "%s", e->d_name);
            char* dot = strrchr(base, '.'); if (dot) *dot = '\0';
            sb_json_escape_append(out, len, cap, base);
        }
        char* fm = sb_frontmatter_json(buf);
        sb_append_plain(out, len, cap, ",\"frontmatter\":");
        sb_append_plain(out, len, cap, fm ? fm : "{}");
        if (fm) free(fm);
        sb_append_plain(out, len, cap, "}");
    }
    closedir(d);
}

char* gmb_links_json_graph(const char* vault)
{
    sqlite3* db = sb_open(); if (!db) return NULL;
    long long vid = sb_vault_id(db, vault);
    char root[1024] = "";
    char* out = NULL; size_t len = 0, cap = 0;

    /* nodes: le note del vault (id = path relativo, title, frontmatter) — parità
     * LinkManager.ToJson/EnumerateNotes. `vault_files` è l'indice dei file del vault
     * (popolato da NOTE CREATE / ingest): un file presente su disco ma non indicizzato non
     * compare qui, come non compare in NB.LIST. */
    sb_append_plain(&out, &len, &cap, "{\"nodes\":[");
    if (vid >= 0 && sb_vault_root(db, vault, root, sizeof(root)))
        sb_graph_nodes_json(&out, &len, &cap, root, "", &g_graph_nodes_first);

    /* edges (link risolti) + dangling (target non presente su disco), stesso criterio di
     * gmb_links_dangling_count: sb_resolve + access(R_OK) → coerenza fra LINKJSON e
     * LINKDANGLING. Formato dangling identico a IL: "<from> → <to>" (freccia U+2192). */
    sb_append_plain(&out, &len, &cap, "],\"edges\":[");
    if (vid >= 0) {
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT from_path,to_path,COALESCE(alias,'') FROM note_links WHERE vault_id=$1 ORDER BY from_path,to_path;",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, vid);
            int first_e = 1, first_d = 1;
            char* dangle = NULL; size_t dl = 0, dc = 0;
            sb_append_plain(&dangle, &dl, &dc, "[");
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char* from = (const char*)sqlite3_column_text(st, 0);
                const char* to = (const char*)sqlite3_column_text(st, 1);
                const char* alias = (const char*)sqlite3_column_text(st, 2);
                if (!from) from = ""; if (!to) to = ""; if (!alias) alias = "";
                char full[2048];
                int resolved = sb_resolve(db, vault, to, full, sizeof(full)) && access(full, R_OK) == 0;
                if (resolved) {
                    if (!first_e) sb_append_plain(&out, &len, &cap, ",");
                    first_e = 0;
                    sb_append_plain(&out, &len, &cap, "{\"from\":");
                    sb_json_escape_append(&out, &len, &cap, from);
                    sb_append_plain(&out, &len, &cap, ",\"to\":");
                    sb_json_escape_append(&out, &len, &cap, to);
                    sb_append_plain(&out, &len, &cap, ",\"alias\":");
                    sb_json_escape_append(&out, &len, &cap, alias);
                    sb_append_plain(&out, &len, &cap, "}");
                } else {
                    if (!first_d) sb_append_plain(&dangle, &dl, &dc, ",");
                    first_d = 0;
                    /* IL emette UN elemento "<from> \u2192 <to>" con la freccia come ESCAPE
                     * JSON dentro la stessa stringa (il confronto è testuale). Escaping fatto
                     * a mano perché sb_json_escape_append raddoppierebbe il backslash. */
                    {
                        char one[2] = { 0, 0 };
                        sb_append_plain(&dangle, &dl, &dc, "\"");
                        for (const char* q = from; *q; q++) {
                            if (*q == '"' || *q == '\\') sb_append_plain(&dangle, &dl, &dc, "\\");
                            one[0] = *q; sb_append_plain(&dangle, &dl, &dc, one);
                        }
                        sb_append_plain(&dangle, &dl, &dc, " \\u2192 ");
                        for (const char* q = to; *q; q++) {
                            if (*q == '"' || *q == '\\') sb_append_plain(&dangle, &dl, &dc, "\\");
                            one[0] = *q; sb_append_plain(&dangle, &dl, &dc, one);
                        }
                        sb_append_plain(&dangle, &dl, &dc, "\"");
                    }
                }
            }
            sqlite3_finalize(st);
            sb_append_plain(&dangle, &dl, &dc, "]");
            sb_append_plain(&out, &len, &cap, "],\"dangling\":");
            sb_append_plain(&out, &len, &cap, dangle ? dangle : "[]");
            sb_append_plain(&out, &len, &cap, "}");
            free(dangle);
            sqlite3_close(db);
            return out;
        }
    }
    sb_append_plain(&out, &len, &cap, "],\"dangling\":[]}");
    sqlite3_close(db);
    return out;
}
