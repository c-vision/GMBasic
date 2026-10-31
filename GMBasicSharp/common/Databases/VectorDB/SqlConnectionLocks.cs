// FILE: Basic.SQLite.Vector.SqlConnectionLocks
using System.Runtime.CompilerServices;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite.Vector;

/// <summary>Per-connection lock shared by SqlVectorIndexer and SqlVectorSearch. A single
/// SqliteConnection's internal command list is not thread-safe for concurrent
/// CreateCommand/Dispose across threads, so any two objects wrapping the same connection
/// must serialize on the same lock instance rather than each holding its own private lock
/// (which would still let a reader and a writer collide on the connection).</summary>
internal static class SqlConnectionLocks
{
    private static readonly ConditionalWeakTable<SqliteConnection, object> Locks = new();

    public static object For(SqliteConnection connection) =>
        Locks.GetValue(connection, static _ => new object());
}
