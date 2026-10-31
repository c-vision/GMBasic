/* gmb_sb_selfcheck.c — parity test per gmb_secondbrain.c (folder props, link manuali, sync).
 * Verifica che il runtime C condiviso produca gli stessi risultati di C# (VaultManager/
 * LinkManager/Ingestor): add/remove/set folder, link/unlink/set_alias, sync "imp/upd/conf".
 * Eseguito come: cmake --build && ./gmb_sb_selfcheck
 */
#include "gmb_runtime.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("ok: %s\n", msg); } } while (0)

/* Crea/aggiorna il vault di test nel db (stesso path di gmb_secondbrain: gmb_paths_vaults_db). */
static void ensure_vault(const char* name) {
    char path[1024];
    gmb_paths_vaults_db(path, sizeof(path));
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    sqlite3* db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return; }
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS vaults (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL, root_path TEXT NOT NULL, pattern TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS vault_folders (vault_id INTEGER NOT NULL, folder_path TEXT NOT NULL, indexed INTEGER NOT NULL DEFAULT 1, tags TEXT NOT NULL DEFAULT '', PRIMARY KEY (vault_id, folder_path));"
        "CREATE TABLE IF NOT EXISTS note_links (vault_id INTEGER NOT NULL, from_path TEXT NOT NULL, to_path TEXT NOT NULL, alias TEXT, PRIMARY KEY (vault_id, from_path, to_path));",
        NULL, NULL, NULL);
    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO vaults(name,root_path,pattern) VALUES('%s','/tmp','*.md')"
        " ON CONFLICT(name) DO UPDATE SET root_path='/tmp',pattern='*.md';", name);
    sqlite3_exec(db, sql, NULL, NULL, NULL);
    sqlite3_close(db);
}

int main(void) {
    const char* vault = "sbv-selfcheck";
    ensure_vault(vault);

    printf("== gmb_secondbrain selfcheck ==\n");

    /* folder props */
    int a = gmb_vault_folder_add(vault, "sub", 1, "tag1,tag2");
    CHECK(a == 1, "gmb_vault_folder_add");
    char* csv = gmb_vault_folders_csv(vault);
    CHECK(csv && strstr(csv, "sub|1|tag1,tag2"), "gmb_vault_folders_csv");
    free(csv);
    int off = 0;
    int s = gmb_vault_folder_set(vault, "sub", &off, "archive");
    CHECK(s == 1, "gmb_vault_folder_set");
    csv = gmb_vault_folders_csv(vault);
    CHECK(csv && strstr(csv, "sub|0|archive"), "gmb_vault_folder_set persisted");
    free(csv);
    int r = gmb_vault_folder_remove(vault, "sub");
    CHECK(r == 1, "gmb_vault_folder_remove");

    /* link manuali */
    int l = gmb_note_link(vault, "a.md", "b.md", "alias1");
    CHECK(l == 1, "gmb_note_link");
    int sa = gmb_note_set_alias(vault, "a.md", "b.md", "x");
    CHECK(sa == 1, "gmb_note_set_alias");
    char* lcsv = gmb_links_csv(vault);
    CHECK(lcsv && strstr(lcsv, "a.md>b.md>x"), "gmb_links_csv alias");
    free(lcsv);
    int u = gmb_note_unlink(vault, "a.md", "b.md");
    CHECK(u == 1, "gmb_note_unlink");

    /* sync su un vault inesistente → NULL (errore esplicito, non stub) */
    char* syn = gmb_sync_folder("vault-non-esiste", "/tmp/sb_sync", "bidirectional", "newest_wins");
    CHECK(syn == NULL, "gmb_sync_folder errore su vault ignoto");

    /* vault info / note CRUD (porting VaultManager/NoteManager) */
    char* info = gmb_vault_info_json(vault);
    CHECK(info && strstr(info, "\"fileCount\":0"), "gmb_vault_info_json");
    free(info);
    char* list = gmb_vault_list_json();
    CHECK(list && strstr(list, "sbv-selfcheck"), "gmb_vault_list_json");
    free(list);

    /* note: usa una root temporanea del vault (re-indirizzata al test) */
    int nc = gmb_note_create(vault, "n1.md", "contenuto nota uno", "alpha,beta");
    CHECK(nc == 1, "gmb_note_create");
    CHECK(gmb_note_exists(vault, "n1.md") == 1, "gmb_note_exists");
    char* body = gmb_note_get(vault, "n1.md");
    CHECK(body && strstr(body, "contenuto nota uno"), "gmb_note_get body");
    free(body);
    char* tags = gmb_note_tags(vault, "n1.md");
    CHECK(tags && strstr(tags, "alpha"), "gmb_note_tags");
    free(tags);
    char* title = gmb_note_title(vault, "n1.md");
    CHECK(title && *title, "gmb_note_title");
    free(title);
    CHECK(gmb_note_wordcount(vault, "n1.md") >= 3, "gmb_note_wordcount");
    CHECK(gmb_note_append(vault, "n1.md", "altra riga") == 1, "gmb_note_append");
    CHECK(gmb_note_update(vault, "n1.md", "contenuto aggiornato") == 1, "gmb_note_update");
    body = gmb_note_get(vault, "n1.md");
    CHECK(body && strstr(body, "aggiornato"), "gmb_note_update persisted");
    free(body);
    CHECK(gmb_note_move(vault, "n1.md", "n2.md") == 1, "gmb_note_move");
    CHECK(gmb_note_exists(vault, "n2.md") == 1, "gmb_note_move target");
    CHECK(gmb_note_delete(vault, "n2.md") == 1, "gmb_note_delete");

    /* ── Vault create/index/index_file/remove_file (parità VaultManager C#, 2026-09-03) ── */
    {
        /* root temporanea con 3 .md (uno in una sottocartella) e 1 .txt (escluso dal pattern) */
        const char* root = "/tmp/sb_vault_index_root";
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf %s && mkdir -p %s/sub", root, root);
        system(cmd);
        FILE* f1 = fopen("/tmp/sb_vault_index_root/a.md", "w"); if (f1) { fputs("nota a", f1); fclose(f1); }
        FILE* f2 = fopen("/tmp/sb_vault_index_root/b.md", "w"); if (f2) { fputs("nota bbbbbbb", f2); fclose(f2); }
        FILE* f3 = fopen("/tmp/sb_vault_index_root/sub/c.md", "w"); if (f3) { fputs("nota cccc", f3); fclose(f3); }
        FILE* ft = fopen("/tmp/sb_vault_index_root/ignored.txt", "w"); if (ft) { fputs("no md", ft); fclose(ft); }

        /* create: registra + indicizza subito → id >= 0 e 3 file (i .md, non il .txt) */
        long long cid = gmb_vault_create("sbv-index", root, "*.md");
        CHECK(cid >= 0, "gmb_vault_create id");
        CHECK(gmb_vault_count("sbv-index") == 3, "gmb_vault_count dopo create (3 .md, .txt escluso)");
        char* lfold = gmb_vault_folders_json("sbv-index");
        CHECK(lfold && strstr(lfold, "sub"), "gmb_vault_folders_json include subfolder");
        free(lfold);

        /* index esplicito (idempotente, upsert) → ancora 3 */
        CHECK(gmb_vault_index("sbv-index") == 3, "gmb_vault_index (upsert)");

        /* index_file: nuovo file → 1, contatore sale a 4 */
        FILE* f4 = fopen("/tmp/sb_vault_index_root/d.md", "w"); if (f4) { fputs("nota d", f4); fclose(f4); }
        CHECK(gmb_vault_index_file("sbv-index", "/tmp/sb_vault_index_root/d.md") == 1, "gmb_vault_index_file nuovo");
        CHECK(gmb_vault_count("sbv-index") == 4, "gmb_vault_count dopo index_file");

        /* remove_file → scende a 3 (rimuove la voce, il file fisico resta) */
        CHECK(gmb_vault_remove_file("sbv-index", "/tmp/sb_vault_index_root/d.md") == 1, "gmb_vault_remove_file");
        CHECK(gmb_vault_count("sbv-index") == 3, "gmb_vault_count dopo remove_file");
        /* index_file su file SPARITO (cancellato dal disco) → 0 e rimuove la voce stale */
        remove("/tmp/sb_vault_index_root/d.md");
        CHECK(gmb_vault_index_file("sbv-index", "/tmp/sb_vault_index_root/d.md") == 0, "gmb_vault_index_file stale");
        CHECK(gmb_vault_count("sbv-index") == 3, "gmb_vault_count dopo stale");
        CHECK(gmb_vault_index("sbv-index") == 3, "gmb_vault_index finale");
        CHECK(gmb_vault_count("sbv-index") == 3, "gmb_vault_count finale 3");

        /* vault ignoto → -1 */
        CHECK(gmb_vault_create("", "/tmp", NULL) == -1, "gmb_vault_create nome vuoto");
        CHECK(gmb_vault_index("vault-inesistente") == -1, "gmb_vault_index vault ignoto");
        CHECK(gmb_vault_index_file("vault-inesistente", "/tmp/x.md") == -1, "gmb_vault_index_file vault ignoto");
        CHECK(gmb_vault_remove_file("vault-inesistente", "/tmp/x.md") == -1, "gmb_vault_remove_file vault ignoto");

        /* pulizia */
        snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
        system(cmd);
    }

    printf(failures == 0 ? "ALL PASS\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
