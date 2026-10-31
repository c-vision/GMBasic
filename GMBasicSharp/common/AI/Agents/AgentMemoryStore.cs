// FILE: Basic.AgentMemoryStore
using Microsoft.Data.Sqlite;
using System.Text.Json;

namespace Basic;

/// <summary>
/// Agent memory store with short-term and long-term storage.
/// Uses SQLite in-memory for persistence.
/// </summary>
public sealed class AgentMemoryStore
{
    private readonly SqliteConnection _connection;

    public AgentMemoryStore(SqliteConnection connection)
    {
        _connection = connection ?? throw new ArgumentNullException(nameof(connection));
        EnsureSchema();
    }

    public static AgentMemoryStore Create(SqliteConnection connection) => new(connection);

    private void EnsureSchema()
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = """
            CREATE TABLE IF NOT EXISTS agent_memory (
                key TEXT PRIMARY KEY,
                value_json TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                updated_at TEXT NOT NULL DEFAULT (datetime('now')),
                is_long_term INTEGER DEFAULT 0
            )
        """;
        cmd.ExecuteNonQuery();
    }

    public bool Store(string key, object? value, bool isLongTerm = false)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        var json = value is not null ? JsonSerializer.Serialize(value) : "null";
        var isLongTermInt = isLongTerm ? 1 : 0;
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = """
            INSERT OR REPLACE INTO agent_memory (key, value_json, updated_at, is_long_term)
            VALUES ($key, $value, datetime('now'), $longTerm)
        """;
        cmd.Parameters.AddWithValue("$key", key);
        cmd.Parameters.AddWithValue("$value", json);
        cmd.Parameters.AddWithValue("$longTerm", isLongTermInt);
        return cmd.ExecuteNonQuery() > 0;
    }

    public T? Load<T>(string key)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "SELECT value_json FROM agent_memory WHERE key = $key";
        cmd.Parameters.AddWithValue("$key", key);
        var result = cmd.ExecuteScalar() as string;
        if (string.IsNullOrEmpty(result)) return default;
        try { return JsonSerializer.Deserialize<T>(result); }
        catch { return default; }
    }

    public bool Delete(string key)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "DELETE FROM agent_memory WHERE key = $key";
        cmd.Parameters.AddWithValue("$key", key);
        return cmd.ExecuteNonQuery() > 0;
    }

    public List<string> List()
    {
        var keys = new List<string>();
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "SELECT key FROM agent_memory ORDER BY key";
        using var reader = cmd.ExecuteReader();
        while (reader.Read()) keys.Add(reader.GetString(0));
        return keys;
    }

    public List<string> Search(string pattern)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(pattern);
        var keys = new List<string>();
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "SELECT key FROM agent_memory WHERE key LIKE $pattern ORDER BY key";
        cmd.Parameters.AddWithValue("$pattern", $"%{pattern}%");
        using var reader = cmd.ExecuteReader();
        while (reader.Read()) keys.Add(reader.GetString(0));
        return keys;
    }

    public int Count() => ScalarInt("SELECT COUNT(*) FROM agent_memory");
    public int CountLongTerm() => ScalarInt("SELECT COUNT(*) FROM agent_memory WHERE is_long_term = 1");

    public void Clear(bool includeLongTerm = false)
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = includeLongTerm
            ? "DELETE FROM agent_memory"
            : "DELETE FROM agent_memory WHERE is_long_term = 0";
        cmd.ExecuteNonQuery();
    }

    private int ScalarInt(string sql)
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = sql;
        return Convert.ToInt32(cmd.ExecuteScalar());
    }
}
