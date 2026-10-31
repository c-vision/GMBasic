// FILE: Basic.SQLite.TestSql — RDD test helper, sostituisce SqlEngine nei test xUnit.

using Basic.Databases.Rdd;

namespace Basic.SQLite;

public static class TestSql
{
    private static readonly DataSession _ds = new();

    private static SqlDriver Driver(string alias)
    {
        var area = _ds.FindAreaByAlias(alias);
        if (area == 0) throw new InvalidOperationException($"'{alias}' not open.");
        return (SqlDriver)_ds.GetWorkArea(area)!.Driver!;
    }

    public static void Open(string path, string alias, bool walMode = true)
    {
        // Close solo se lo stesso alias è già aperto
        if (_ds.FindAreaByAlias(alias) != 0)
            Close(alias);
        var driver = new SqlDriver();
        driver.Open(new DbOpenInfo { FileName = $"Data Source={path}", Alias = alias, Driver = "SQLite" });
        _ds.RegisterDirect(_ds.AllocateArea(), driver, alias);
        // PRAGMA obbligatori dopo open
        Exec(alias, "PRAGMA busy_timeout = 5000");
        if (walMode)
            Exec(alias, "PRAGMA journal_mode = WAL");
    }

    public static void Close(string alias) { var d = Driver(alias); d.Close(); _ds.Unregister(_ds.FindAreaByAlias(alias)); }
    public static void CloseAll() => _ds.CloseAll();
    public static bool IsOpen(string alias) => _ds.FindAreaByAlias(alias) != 0;
    public static int Exec(string alias, string sql, IReadOnlyList<object?>? p = null, IReadOnlyDictionary<string, object?>? n = null) => (int)Driver(alias).ExecRaw(sql, p, n);
    public static int Query(string alias, string sql, IReadOnlyList<object?>? p = null, IReadOnlyDictionary<string, object?>? n = null) => Driver(alias).QueryRaw(sql, p, n);
    public static bool Fetch(int rs) => FindDriver()?.Fetch(rs) ?? false;
    public static IReadOnlyDictionary<string, object?>? GetCurrentRow(int rs) => FindDriver()?.GetCurrentRow(rs);
    public static void CloseResultSet(int rs) => FindDriver()?.CloseResultSet(rs);
    public static void Attach(string alias, string path, string schema) => Driver(alias).Attach(path, schema);
    public static void Detach(string alias, string schema) => Driver(alias).Detach(schema);
    public static void BeginTransaction(string alias) => Driver(alias).BeginTransaction();
    public static void Commit(string alias) => Driver(alias).Commit();
    public static void Rollback(string alias) => Driver(alias).Rollback();
    public static long LastInsertRowId(string alias) => Driver(alias).LastInsertRowId();
    public static long Changes(string alias) => Driver(alias).Changes();
    public static void Backup(string alias, string path) => Driver(alias).Backup(path);
    public static void WithConnection(string alias, Action<Microsoft.Data.Sqlite.SqliteConnection> action)
    {
        Driver(alias).WithConnection(c => { if (c is Microsoft.Data.Sqlite.SqliteConnection sc) action(sc); });
    }

    private static SqlDriver? FindDriver()
    {
        for (int i = 1; i <= DataSession.MaxWorkareas; i++)
            if (_ds.GetWorkArea(i)?.Driver is SqlDriver sd) return sd;
        return null;
    }
}
