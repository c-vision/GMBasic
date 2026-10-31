// FILE: Basic.Connectors.ConnectorDatabase
// Database connector F1 — Postgres/SQLite via API comune (connect/query/execute/transaction/schema/tables/describe) — SYSTEM.txt §11

using System.Data.Common;
using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorDatabase
{
    public static string Dispatch(string connectorName, string tool, string argsJson)
    {
        var args = TryParseArgs(argsJson);
        var kind = connectorName.ToLowerInvariant();
        // Normalizza kind: postgres/sqlite/mysql/sqlserver/mariadb/duckdb → provider
        return tool.ToLowerInvariant() switch
        {
            "connect" => Connect(kind, args),
            "query" => Query(kind, args),
            "execute" => Execute(kind, args),
            "transaction" => Transaction(kind, args),
            "schema" => Schema(kind, args),
            "tables" => Tables(kind, args),
            "describe" => Describe(kind, args),
            _ => throw new ArgumentException($"DB tool '{tool}' unknown (connect/query/execute/transaction/schema/tables/describe)")
        };
    }

    private static string Connect(string kind, Dictionary<string, string> args)
    {
        var connStr = GetArg(args, "conn_string") ?? GetArg(args, "connectionString") ?? GetArg(args, "path") ?? GetArg(args, "url") ?? "";
        if (string.IsNullOrWhiteSpace(connStr))
            connStr = ConnectorRegistry.Instance.GetSecret(kind, "conn_string") ?? ConnectorRegistry.Instance.GetSecret(kind, "path") ?? "";
        if (string.IsNullOrWhiteSpace(connStr) && kind == "sqlite")
            connStr = "Data Source=:memory:";
        if (string.IsNullOrWhiteSpace(connStr))
            throw new ArgumentException($"DB connect: conn_string/path required for {kind} (usa CONNECTOR SECRET SET {kind}, conn_string, \"...\")");

        // Verifica connessione (apre e chiude)
        using var conn = CreateConnection(kind, connStr);
        conn.Open();
        var state = conn.State.ToString();
        return JsonSerializer.Serialize(new { kind, connectionString = MaskConnStr(connStr), state, status = "connected" }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Query(string kind, Dictionary<string, string> args)
    {
        var sql = GetArg(args, "sql") ?? GetArg(args, "query") ?? throw new ArgumentException("sql required");
        var connStr = ResolveConnStr(kind, args);
        using var conn = CreateConnection(kind, connStr);
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = sql;
        using var reader = cmd.ExecuteReader();
        var rows = new List<Dictionary<string, object?>>();
        while (reader.Read())
        {
            var row = new Dictionary<string, object?>();
            for (int i = 0; i < reader.FieldCount; i++)
                row[reader.GetName(i)] = reader.IsDBNull(i) ? null : reader.GetValue(i);
            rows.Add(row);
        }
        return JsonSerializer.Serialize(new { kind, sql, rows, count = rows.Count }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Execute(string kind, Dictionary<string, string> args)
    {
        var sql = GetArg(args, "sql") ?? throw new ArgumentException("sql required");
        var connStr = ResolveConnStr(kind, args);
        using var conn = CreateConnection(kind, connStr);
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = sql;
        var affected = cmd.ExecuteNonQuery();
        return JsonSerializer.Serialize(new { kind, sql, affected, status = "ok" }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Transaction(string kind, Dictionary<string, string> args)
    {
        var sqlsJson = GetArg(args, "sqls") ?? GetArg(args, "queries") ?? "[]";
        List<string> sqls;
        try { sqls = JsonSerializer.Deserialize<List<string>>(sqlsJson) ?? []; } catch { sqls = [sqlsJson]; }
        var connStr = ResolveConnStr(kind, args);
        using var conn = CreateConnection(kind, connStr);
        conn.Open();
        using var tx = conn.BeginTransaction();
        int total = 0;
        try
        {
            foreach (var sql in sqls)
            {
                using var cmd = conn.CreateCommand();
                cmd.Transaction = tx;
                cmd.CommandText = sql;
                total += cmd.ExecuteNonQuery();
            }
            tx.Commit();
        }
        catch (Exception ex) { tx.Rollback(); throw new InvalidOperationException($"Transaction failed: {ex.Message}", ex); }
        return JsonSerializer.Serialize(new { kind, count = sqls.Count, affected = total, status = "committed" }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Schema(string kind, Dictionary<string, string> args)
    {
        var db = GetArg(args, "db") ?? GetArg(args, "database") ?? "";
        var connStr = ResolveConnStr(kind, args);
        using var conn = CreateConnection(kind, connStr);
        conn.Open();
        // Lista tabelle come schema
        var tables = new List<string>();
        using (var cmd = conn.CreateCommand())
        {
            if (kind == "sqlite")
                cmd.CommandText = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";
            else if (kind == "postgres")
                cmd.CommandText = "SELECT tablename FROM pg_tables WHERE schemaname='public' ORDER BY tablename";
            else
                cmd.CommandText = "SELECT name FROM sqlite_master WHERE type='table' LIMIT 0";
            try
            {
                using var r = cmd.ExecuteReader();
                while (r.Read()) tables.Add(r.GetString(0));
            }
            catch { }
        }
        return JsonSerializer.Serialize(new { kind, database = db, tables, count = tables.Count }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Tables(string kind, Dictionary<string, string> args) => Schema(kind, args);

    private static string Describe(string kind, Dictionary<string, string> args)
    {
        var table = GetArg(args, "table") ?? throw new ArgumentException("table required");
        var connStr = ResolveConnStr(kind, args);
        using var conn = CreateConnection(kind, connStr);
        conn.Open();
        using var cmd = conn.CreateCommand();
        if (kind == "sqlite")
            cmd.CommandText = $"SELECT sql FROM sqlite_master WHERE type='table' AND name='{table.Replace("'", "''")}'";
        else
            cmd.CommandText = $"SELECT column_name, data_type FROM information_schema.columns WHERE table_name='{table.Replace("'", "''")}' ORDER BY ordinal_position";
        var cols = new List<Dictionary<string, object?>>();
        using var r = cmd.ExecuteReader();
        while (r.Read())
        {
            var row = new Dictionary<string, object?>();
            for (int i = 0; i < r.FieldCount; i++) row[r.GetName(i)] = r.IsDBNull(i) ? null : r.GetValue(i);
            cols.Add(row);
        }
        return JsonSerializer.Serialize(new { kind, table, columns = cols }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string ResolveConnStr(string kind, Dictionary<string, string> args)
    {
        var cs = GetArg(args, "conn_string") ?? GetArg(args, "connectionString") ?? GetArg(args, "path") ?? "";
        if (!string.IsNullOrWhiteSpace(cs))
        {
            cs = ConvertPostgresUrlIfNeeded(cs);
            if (kind == "sqlite" && !cs.Contains("="))
            {
                // Assicura che la directory esista per file path (es. work/demo.db → samples/.../work)
                try
                {
                    var dir = Path.GetDirectoryName(cs);
                    if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
                    {
                        var baseDir = CommonLib.EntrySource.Dir;
                        var fullDir = Path.IsPathRooted(dir) ? dir : Path.Combine(baseDir, dir);
                        if (!Directory.Exists(fullDir)) Directory.CreateDirectory(fullDir);
                    }
                }
                catch { }
                cs = $"Data Source={cs}";
            }
            return cs;
        }
        cs = ConnectorRegistry.Instance.GetSecret(kind, "conn_string") ?? ConnectorRegistry.Instance.GetSecret(kind, "path") ?? "";
        if (!string.IsNullOrWhiteSpace(cs))
        {
            cs = ConvertPostgresUrlIfNeeded(cs);
            if (kind == "sqlite" && !cs.Contains("=")) cs = $"Data Source={cs}";
            return cs;
        }
        if (kind == "sqlite") return "Data Source=:memory:";
        throw new ArgumentException($"DB {kind}: conn_string/path required (CONNECTOR SECRET SET {kind}, conn_string, \"...\")");
    }

    private static DbConnection CreateConnection(string kind, string connStr)
    {
        if (kind == "postgres" || connStr.Contains("Host=") || connStr.Contains("Username=") || connStr.StartsWith("postgresql://", StringComparison.OrdinalIgnoreCase))
        {
            // Npgsql è ora referenziato direttamente in AI.csproj
            return new Npgsql.NpgsqlConnection(ConvertPostgresUrlIfNeeded(connStr));
        }
        if (kind is "mysql" or "mariadb")
        {
            return new MySqlConnector.MySqlConnection(connStr);
        }
        // Default: SQLite (Microsoft.Data.Sqlite è sempre referenziato da AI.csproj)
        return new Microsoft.Data.Sqlite.SqliteConnection(connStr);
    }

    private static string ConvertPostgresUrlIfNeeded(string cs)
    {
        if (!cs.StartsWith("postgresql://", StringComparison.OrdinalIgnoreCase) && !cs.StartsWith("postgres://", StringComparison.OrdinalIgnoreCase))
            return cs;
        try
        {
            var uri = new Uri(cs);
            var user = Uri.UnescapeDataString(uri.UserInfo.Split(':')[0]);
            var pass = uri.UserInfo.Contains(':') ? Uri.UnescapeDataString(uri.UserInfo.Split(':', 2)[1]) : "";
            var host = uri.Host;
            var port = uri.Port > 0 ? uri.Port : 5432;
            var db = uri.AbsolutePath.Trim('/').Split('?')[0];
            if (string.IsNullOrWhiteSpace(db)) db = "postgres";
            var parts = new List<string> { $"Host={host}", $"Port={port}", $"Username={user}", $"Database={db}" };
            if (!string.IsNullOrEmpty(pass)) parts.Add($"Password={pass}");
            // Query string params (sslmode etc.)
            if (!string.IsNullOrWhiteSpace(uri.Query))
            {
                var q = uri.Query.TrimStart('?').Split('&', StringSplitOptions.RemoveEmptyEntries);
                foreach (var kv in q)
                {
                    var p = kv.Split('=', 2);
                    if (p.Length == 2 && p[0].Equals("sslmode", StringComparison.OrdinalIgnoreCase))
                        parts.Add($"SslMode={p[1]}");
                }
            }
            return string.Join(";", parts);
        }
        catch { return cs; }
    }

    private static string MaskConnStr(string cs)
    {
        if (cs.Contains("Password=")) return System.Text.RegularExpressions.Regex.Replace(cs, "Password=[^;]+", "Password=***");
        if (cs.Contains("pwd=")) return System.Text.RegularExpressions.Regex.Replace(cs, "pwd=[^;]+", "pwd=***", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        return cs;
    }

    private static Dictionary<string, string> TryParseArgs(string json)
    {
        var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrWhiteSpace(json)) return d;
        try
        {
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.ValueKind != JsonValueKind.Object) return d;
            foreach (var p in doc.RootElement.EnumerateObject())
                d[p.Name] = p.Value.ValueKind == JsonValueKind.String ? p.Value.GetString() ?? "" : p.Value.GetRawText().Trim('"');
        }
        catch { }
        return d;
    }
    private static string? GetArg(Dictionary<string, string> d, string k) => d.TryGetValue(k, out var v) ? v : null;
}
