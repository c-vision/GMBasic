/* native/gmb_runtime_c/src/gmb_memory.c — MEMORY.* (AI memory), PORT ESATTO del motore C#
 * Basic.AgentMemoryStore: SQLite IN-MEMORY (:memory:) con lo STESSO schema e la stessa
 * semantica, così VM/C, C99Native usano lo stesso motore dei backend C# (IL/PCode).
 *   Schema: agent_memory(key TEXT PRIMARY KEY, value_json TEXT NOT NULL, created_at TEXT,
 *           updated_at TEXT, is_long_term INTEGER DEFAULT 0)
 *   Store: INSERT OR REPLACE; il valore e' serializzato in JSON (string -> "...") come il C#.
 *   Load:  deserializza il JSON (string -> testo); JSON non-string -> "" (parita' Load<string>).
 *   List:  chiavi ORDER BY key, join "\n".  Search: chiavi LIKE %pattern% (case-insensitive
 *          ASCII come SQLite), ORDER BY key, join "\n".
 * Volatile di processo (come il C# :memory:), a scelta dell'utente. */
#include "gmb_runtime.h"
#include <sqlite3.h>
#include "gmb_state_hook.h"   /* hook opzionale di stato (client managed) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3* g_mem_db = NULL;

static int gmb_mem_db_open(void)
{
    if (g_mem_db) return 0;
    if (sqlite3_open(":memory:", &g_mem_db) != SQLITE_OK) { g_mem_db = NULL; return -1; }
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS agent_memory ("
        " key TEXT PRIMARY KEY, value_json TEXT NOT NULL,"
        " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        " updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        " is_long_term INTEGER DEFAULT 0)";
    char* err = NULL;
    if (sqlite3_exec(g_mem_db, ddl, NULL, NULL, &err) != SQLITE_OK) { sqlite3_free(err); return -1; }
    return 0;
}

/* JSON-encoda una stringa BASIC (come JsonSerializer.Serialize per il tipo string) */
static char* gmb_mem_json_quote(const char* s)
{
    if (!s) return strdup("null");
    size_t n = 2;
    for (const char* p = s; *p; p++) {
        n++;
        if (*p == '"' || *p == '\\') n++;
        else if (*p == '\n' || *p == '\r' || *p == '\t') n++;
    }
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = '"';
    for (const char* p = s; *p; p++) {
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

/* Decodifica un valore JSON: se e' una stringa -> testo decodificato (malloc);
 * altrimenti (numero/bool/null/oggetto) -> "" come Load<string> sul C#. */
static char* gmb_mem_json_unquote(const char* json)
{
    if (!json || json[0] != '"') return strdup("");
    size_t len = strlen(json);
    if (len < 2) return strdup("");
    char* out = (char*)malloc(len - 1);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 1; i < len - 1; i++) {
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

static int gmb_mem_changes(void)
{
    return g_mem_db ? sqlite3_changes(g_mem_db) : 0;
}

int gmb_mem_store(const char* key, const char* value, int longTerm)
{
    { char lt[8]; snprintf(lt, sizeof lt, "%d", longTerm ? 1 : 0);
      int hr = gmb_state_hook_mutate("memory.store", key, value, lt); if (hr >= 0) return hr; }
    if (!key || !*key) return 0;
    if (gmb_mem_db_open() != 0) return 0;
    char* json = gmb_mem_json_quote(value);
    if (!json) return 0;
    sqlite3_stmt* st = NULL;
    const char* sql = "INSERT OR REPLACE INTO agent_memory (key, value_json, updated_at, is_long_term)"
                      " VALUES (?1, ?2, datetime('now'), ?3)";
    if (sqlite3_prepare_v2(g_mem_db, sql, -1, &st, NULL) != SQLITE_OK) { free(json); return 0; }
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, longTerm ? 1 : 0);
    int rc = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    free(json);
    return rc && gmb_mem_changes() > 0;
}

char* gmb_mem_load(const char* key)
{
    if (!key || gmb_mem_db_open() != 0) return strdup("");
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(g_mem_db, "SELECT value_json FROM agent_memory WHERE key = ?1", -1, &st, NULL) != SQLITE_OK)
        return strdup("");
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    char* out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* j = sqlite3_column_text(st, 0);
        out = gmb_mem_json_unquote(j ? (const char*)j : "");
    }
    sqlite3_finalize(st);
    return out ? out : strdup("");
}

int gmb_mem_delete(const char* key)
{
    { int hr = gmb_state_hook_mutate("memory.delete", key, NULL, NULL); if (hr >= 0) return hr; }
    if (!key || gmb_mem_db_open() != 0) return 0;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(g_mem_db, "DELETE FROM agent_memory WHERE key = ?1", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return rc && gmb_mem_changes() > 0;
}

/* SELECT ... ORDER BY key → stringa newline-join delle chiavi */
static char* gmb_mem_select_keys(const char* where_sql, const char* like_param)
{
    if (gmb_mem_db_open() != 0) return strdup("");
    sqlite3_stmt* st = NULL;
    const char* sql = where_sql ? where_sql : "SELECT key FROM agent_memory ORDER BY key";
    if (sqlite3_prepare_v2(g_mem_db, sql, -1, &st, NULL) != SQLITE_OK) return strdup("");
    if (like_param) sqlite3_bind_text(st, 1, like_param, -1, SQLITE_TRANSIENT);
    size_t cap = 256, len = 0;
    char* out = (char*)malloc(cap);
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

char* gmb_mem_list(void)
{
    return gmb_mem_select_keys(NULL, NULL);
}

char* gmb_mem_search(const char* pattern)
{
    if (!pattern) return strdup("");
    size_t pl = strlen(pattern);
    char* like = (char*)malloc(pl + 3);
    if (!like) return strdup("");
    like[0] = '%';
    memcpy(like + 1, pattern, pl);
    like[pl + 1] = '%';
    like[pl + 2] = 0;
    char* out = gmb_mem_select_keys(
        "SELECT key FROM agent_memory WHERE key LIKE ?1 COLLATE NOCASE ORDER BY key", like);
    free(like);
    return out;
}
