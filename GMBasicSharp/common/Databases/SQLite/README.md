# SQLite engine (`Databases/SQLite/`)

Fifth embedded database engine for GMBasic — general-purpose SQL via NuGet (`Microsoft.Data.Sqlite` + `SQLitePCLRaw.bundle_e_sqlite3`). No vendored amalgamation.

## C# API (`Basic.SQLite`)

| Type | Role |
|------|------|
| `SqliteRuntime` | One-time native init + capability probe |
| `SqlEngine` | Thread-safe session map: Open, Close, Exec, Query, Fetch, transactions, Attach, Detach, Backup |
| `SqlConnectionHandle` | Per-alias connection + transaction state |
| `SqlResultSet` | Row cursor for FETCH |

Defaults: `PRAGMA foreign_keys=ON`; WAL on file databases (optional).

## BASIC namespace `SQL`

```basic
IMPORT SQL
SQL OPEN "app.db" AS db
SQL EXEC db, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)"
SQL QUERY db, rs, "SELECT * FROM t"
DO WHILE SQL.FETCH(rs)
  PRINT rs("name")
LOOP
SQL CLOSE db
```

Qualified syntax: `SQL.OPEN`, `SQL.EXEC`, `SQL.FETCH`, etc.

## Vector search (`Databases/SQLite/Vector/`)

Brute-force semantic search on `vec_*` tables (Fase 1a–1c). FTS5 hybrid via `HybridSearchByText` (RRF vector + BM25).

```csharp
search.HybridSearchByText(collectionId, "keyword query", topK: 5);
```

```sh
dotnet test Databases.Sqlite.Tests --filter "Category=Fast" -p:RunAnalyzers=false
```


## Native location

Native `e_sqlite3` ships inside the **SQLitePCLRaw.bundle_e_sqlite3** NuGet package (transitive from `Databases.csproj`). At runtime, `SQLitePCLRaw.Batteries_V2.Init()` loads the platform RID-specific native library from the package cache / output directory — not from a manual `Databases/SQLite/` vendor folder.
