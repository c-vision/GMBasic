/* gmb_ingest_selfcheck.c — parity test per gmb_ingest_import_file/folder (porting Ingestor C#).
 * Verifica che il runtime C condiviso produca lo stesso comportamento di Ingestor.ImportFile/
 * ImportFolder: testo → copia con frontmatter title, binario → converter registrato (delegate
 * BinaryTextExtractor), chroot guard su path relativi, folder ricorsivo con struttura preservata,
 * errori espliciti (mai stub) per vault/file/converter mancanti.
 * Eseguito come: cmake --build && ./gmb_ingest_selfcheck
 */
#include "gmb_runtime.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("ok: %s\n", msg); } } while (0)

static void ensure_vault(const char* name, const char* root) {
    char path[1024];
    gmb_paths_vaults_db(path, sizeof(path));
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    sqlite3* db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) { if (db) sqlite3_close(db); return; }
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS vaults (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL, root_path TEXT NOT NULL, pattern TEXT NOT NULL);",
        NULL, NULL, NULL);
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT INTO vaults(name,root_path,pattern) VALUES('%s','%s','*.md')"
        " ON CONFLICT(name) DO UPDATE SET root_path='%s',pattern='*.md';", name, root, root);
    sqlite3_exec(db, sql, NULL, NULL, NULL);
    sqlite3_close(db);
}

static void writef(const char* path, const char* content) {
    char dir[1024]; snprintf(dir, sizeof(dir), "%s", path);
    char* sl = strrchr(dir, '/'); if (sl) { *sl = '\0'; mkdir(dir, 0755); }
    FILE* f = fopen(path, "w"); if (f) { fputs(content, f); fclose(f); }
}

/* converter finto per i binari (delegate BinaryTextExtractor): legge un .bin e ritorna md fisso. */
static char* fake_converter(const char* path, const char* fmt) {
    (void)path; (void)fmt;
    return strdup("# Convertito\nmarkdown dal converter");
}

int main(void) {
    const char* vault = "ingv-selfcheck";
    const char* root = "/tmp/gmb_ingest_root";
    const char* src = "/tmp/gmb_ingest_src";
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s %s && mkdir -p %s %s/sub", root, src, root, src);
    system(cmd);
    ensure_vault(vault, root);

    printf("== gmb_ingest selfcheck ==\n");

    /* ── 1. ImportFile: testo .txt → nota .md con frontmatter title (H1) ── */
    writef("/tmp/gmb_ingest_src/nota.txt", "# Il titolo\ncontenuto di prova");
    char* r = gmb_ingest_import_file(vault, "/tmp/gmb_ingest_src/nota.txt", NULL, 1, 1);
    CHECK(r && strcmp(r, "nota.md") == 0, "gmb_ingest_import_file testo (default nota.md)");
    free(r);
    CHECK(gmb_note_exists(vault, "nota.md") == 1, "nota.md esiste nel vault");
    char* t = gmb_note_title(vault, "nota.md");
    /* gmb_note_title ritorna il campo title del frontmatter: "Il titolo" (YAML quotato);
     * la nota è paritaria alla C# (NoteManager scrive title: "Il titolo"). */
    CHECK(t && (strcmp(t, "Il titolo") == 0 || strcmp(t, "\"Il titolo\"") == 0), "title da primo H1 (frontmatter)");
    free(t);
    char* body = gmb_note_get(vault, "nota.md");
    CHECK(body && strstr(body, "contenuto di prova"), "gmb_note_get body");
    free(body);

    /* ── 2. ImportFile con relPath esplicito (sottocartella) ── */
    r = gmb_ingest_import_file(vault, "/tmp/gmb_ingest_src/nota.txt", "sub/rel.md", 1, 1);
    CHECK(r && strcmp(r, "sub/rel.md") == 0, "relPath esplicito preservato");
    free(r);
    CHECK(gmb_note_exists(vault, "sub/rel.md") == 1, "nota in sottocartella");

    /* ── 3. chroot guard: rel con ".." → NULL (mai fuori dalla root) ── */
    r = gmb_ingest_import_file(vault, "/tmp/gmb_ingest_src/nota.txt", "../escape.md", 1, 1);
    CHECK(r == NULL, "chroot guard (..) respinge");

    /* ── 4. file inesistente / vault ignoto → NULL (errore esplicito) ── */
    r = gmb_ingest_import_file(vault, "/tmp/gmb_ingest_src/assente.txt", NULL, 1, 1);
    CHECK(r == NULL, "file inesistente → NULL");
    r = gmb_ingest_import_file("vault-inesistente", "/tmp/gmb_ingest_src/nota.txt", NULL, 1, 1);
    CHECK(r == NULL, "vault ignoto → NULL");

    /* ── 5. binario senza converter → NULL; con converter → nota convertita ── */
    writef("/tmp/gmb_ingest_src/doc.bin", "xxxx");
    r = gmb_ingest_import_file(vault, "/tmp/gmb_ingest_src/doc.bin", NULL, 1, 1);
    CHECK(r == NULL, "binario senza converter → NULL (errore esplicito)");
    gmb_ingest_register_converter(fake_converter);
    r = gmb_ingest_import_file(vault, "/tmp/gmb_ingest_src/doc.bin", NULL, 1, 1);
    CHECK(r && strcmp(r, "doc.md") == 0, "binario con converter → doc.md");
    free(r);
    body = gmb_note_get(vault, "doc.md");
    CHECK(body && strstr(body, "markdown dal converter"), "nota convertita body");
    free(body);
    gmb_ingest_register_converter(NULL);

    /* ── 6. ImportFolder ricorsivo con struttura preservata ──
     * cartella src contiene già: nota.txt, doc.bin (dai test 1/5). Si aggiungono a.txt,
     * sub/b.txt, sub/c.md → 5 file, ma doc.bin è escluso (converter resettato a NULL). */
    writef("/tmp/gmb_ingest_src/a.txt", "# A\ncontenuto a");
    writef("/tmp/gmb_ingest_src/sub/b.txt", "# B\ncontenuto b");
    writef("/tmp/gmb_ingest_src/sub/c.md", "nota c senza frontmatter");   /* già .md, copia */
    long long n = gmb_ingest_import_folder(vault, src, 1, 1, 1);
    CHECK(n == 4, "folder import 4 (nota.txt,a.txt,sub/b.txt,sub/c.md; doc.bin escluso)");

    /* re-import (idempotente per conteggio: sovrascrive le note) → sempre 4 */
    n = gmb_ingest_import_folder(vault, src, 1, 1, 1);
    CHECK(n == 4, "folder re-import = 4 (binario doc.bin ancora escluso)");

    /* ── 7. ImportFolder preserve=0 → piatti nella root ── */
    snprintf(cmd, sizeof(cmd), "rm -rf %s && mkdir -p %s", root, root);
    system(cmd);
    n = gmb_ingest_import_folder(vault, src, 0, 1, 1);
    CHECK(n == 4, "folder import preserve=0 → 4 (nota,a,b,c; niente sub/)");
    /* parità C# ImportFolder: il rel esplicito preserva l'estensione del sorgente (b.txt,
     * non b.md — il default .md vale solo quando rel è null). */
    CHECK(gmb_note_exists(vault, "b.txt") == 1, "preserve=0 appiattisce sub/b.txt → b.txt");

    /* ── 8. cartella mancante / vault ignoto → 0 / -1 ── */
    n = gmb_ingest_import_folder(vault, "/tmp/gmb_ingest_nessuna", 1, 1, 1);
    CHECK(n == 0, "folder mancante → 0");
    n = gmb_ingest_import_folder("vault-inesistente", src, 1, 1, 1);
    CHECK(n == -1, "folder su vault ignoto → -1");

    snprintf(cmd, sizeof(cmd), "rm -rf %s %s", root, src);
    system(cmd);

    printf(failures == 0 ? "ALL PASS\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
