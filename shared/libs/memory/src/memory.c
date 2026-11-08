/* ============================================================================
 * memory.c — MEMORY.* (AI memory) per la lib `memory` (shared/libs/memory).
 *
 * KV store su SQLite IN-MEMORY (:memory:), stesso schema/semantica del motore
 * storico (gmb_memory.c / Basic.AgentMemoryStore):
 *   agent_memory(key TEXT PRIMARY KEY, value_json TEXT NOT NULL,
 *                created_at TEXT, updated_at TEXT, is_long_term INTEGER)
 * Store: INSERT OR REPLACE con il valore JSON-encodato come stringa.
 * Load:  decodifica una stringa JSON; valori non-stringa -> "" (parita' Load<string>).
 * List/Search: chiavi ORDER BY key, join "\n"; Search LIKE %pattern% case-insensitive.
 *
 * SQLite e' una libreria di PIATTAFORMA/supporto di GMBasic (macOS: /usr/lib;
 * Linux: gruppo `support` del libs-manifest), non una dipendenza da installare.
 * Nessun hook C->C#: la lib e' autonoma.
 *
 * Simboli esportati con prefisso ofx_mem_.
 * ========================================================================== */
#include "gmbmemory.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3* mem_db = NULL;

static int mem_db_open(void)
{
    if (mem_db) return 0;
    if (sqlite3_open(":memory:", &mem_db) != SQLITE_OK) { mem_db = NULL; return -1; }
    {
        const char* ddl =
            "CREATE TABLE IF NOT EXISTS agent_memory ("
            " key TEXT PRIMARY KEY, value_json TEXT NOT NULL,"
            " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
            " updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
            " is_long_term INTEGER DEFAULT 0)";
        char* err = NULL;
        if (sqlite3_exec(mem_db, ddl, NULL, NULL, &err) != SQLITE_OK) { sqlite3_free(err); return -1; }
    }
    return 0;
}

/* JSON-encoda una stringa (come JsonSerializer.Serialize per il tipo string). */
static char* mem_json_quote(const char* s)
{
    size_t n = 2, k = 0;
    const char* p;
    char* out;
    if (!s) return strdup("null");
    for (p = s; *p; p++) {
        n++;
        if (*p == '"' || *p == '\\') n++;
        else if (*p == '\n' || *p == '\r' || *p == '\t') n++;
    }
    out = (char*)malloc(n + 1);
    if (!out) return NULL;
    out[k++] = '"';
    for (p = s; *p; p++) {
        switch (*p) {
            case '"': out[k++] = '\\'; out[k++] = '"'; break;
            case '\\': out[k++] = '\\'; out[k++] = '\\'; break;
            case '\n': out[k++] = '\\'; out[k++] = 'n'; break;
            case '\r': out[k++] = '\\'; out[k++] = 'r'; break;
            case '\t': out[k++] = '\\'; out[k++] = 't'; break;
            default: out[k++] = *p; break;
        }
    }
    out[k++] = '"';
    out[k] = 0;
    return out;
}

/* Decodifica un valore JSON: stringa -> testo; altro -> "" (Load<string>). */
static char* mem_json_unquote(const char* json)
{
    size_t len, k = 0, i;
    char* out;
    if (!json || json[0] != '"') return strdup("");
    len = strlen(json);
    if (len < 2) return strdup("");
    out = (char*)malloc(len - 1);
    if (!out) return NULL;
    for (i = 1; i < len - 1; i++) {
        char ch = json[i];
        if (ch == '\\' && i + 1 < len - 1) {
            i++;
            switch (json[i]) {
                case 'n': out[k++] = '\n'; break;
                case 'r': out[k++] = '\r'; break;
                case 't': out[k++] = '\t'; break;
                case 'b': out[k++] = '\b'; break;
                case 'f': out[k++] = '\f'; break;
                default: out[k++] = json[i]; break;   /* \" \\ \/ \u... (semplificato) */
            }
        } else {
            out[k++] = ch;
        }
    }
    out[k] = 0;
    return out;
}

static int mem_changes(void)
{
    return mem_db ? sqlite3_changes(mem_db) : 0;
}

int ofx_mem_store(const char* key, const char* value, int long_term)
{
    char* json;
    sqlite3_stmt* st = NULL;
    int rc;
    if (!key || !*key) return 0;
    if (mem_db_open() != 0) return 0;
    json = mem_json_quote(value);
    if (!json) return 0;
    if (sqlite3_prepare_v2(mem_db,
            "INSERT OR REPLACE INTO agent_memory (key, value_json, updated_at, is_long_term)"
            " VALUES (?1, ?2, datetime('now'), ?3)", -1, &st, NULL) != SQLITE_OK)
    { free(json); return 0; }
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, long_term ? 1 : 0);
    rc = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    free(json);
    return rc && mem_changes() > 0;
}

char* ofx_mem_load(const char* key)
{
    sqlite3_stmt* st = NULL;
    char* out = NULL;
    if (!key || mem_db_open() != 0) return strdup("");
    if (sqlite3_prepare_v2(mem_db, "SELECT value_json FROM agent_memory WHERE key = ?1", -1, &st, NULL) != SQLITE_OK)
        return strdup("");
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* j = sqlite3_column_text(st, 0);
        out = mem_json_unquote(j ? (const char*)j : "");
    }
    sqlite3_finalize(st);
    return out ? out : strdup("");
}

int ofx_mem_delete(const char* key)
{
    sqlite3_stmt* st = NULL;
    int rc;
    if (!key || mem_db_open() != 0) return 0;
    if (sqlite3_prepare_v2(mem_db, "DELETE FROM agent_memory WHERE key = ?1", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return rc && mem_changes() > 0;
}

/* SELECT ... ORDER BY key → stringa newline-join delle chiavi. */
static char* mem_select_keys(const char* where_sql, const char* like_param)
{
    sqlite3_stmt* st = NULL;
    const char* sql = where_sql ? where_sql : "SELECT key FROM agent_memory ORDER BY key";
    size_t cap = 256, len = 0;
    char* out;
    if (mem_db_open() != 0) return strdup("");
    if (sqlite3_prepare_v2(mem_db, sql, -1, &st, NULL) != SQLITE_OK) return strdup("");
    if (like_param) sqlite3_bind_text(st, 1, like_param, -1, SQLITE_TRANSIENT);
    out = (char*)malloc(cap);
    if (!out) { sqlite3_finalize(st); return strdup(""); }
    out[0] = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* k = sqlite3_column_text(st, 0);
        const char* key = k ? (const char*)k : "";
        size_t kl = strlen(key);
        if (len + kl + 1 + 1 > cap) {
            size_t nc = cap * 2;
            char* no = (char*)realloc(out, nc);
            if (!no) break;
            out = no; cap = nc;
        }
        if (len) out[len++] = '\n';
        memcpy(out + len, key, kl); len += kl;
        out[len] = 0;
    }
    sqlite3_finalize(st);
    return out;
}

char* ofx_mem_list(void)
{
    return mem_select_keys(NULL, NULL);
}

char* ofx_mem_search(const char* pattern)
{
    char* like;
    char* out;
    size_t pl;
    if (!pattern) return strdup("");
    pl = strlen(pattern);
    like = (char*)malloc(pl + 3);
    if (!like) return strdup("");
    like[0] = '%';
    memcpy(like + 1, pattern, pl);
    like[pl + 1] = '%';
    like[pl + 2] = 0;
    out = mem_select_keys(
        "SELECT key FROM agent_memory WHERE key LIKE ?1 COLLATE NOCASE ORDER BY key", like);
    free(like);
    return out;
}
