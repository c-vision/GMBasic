// FILE: Basic.Databases.Rdd.SqlDriver.cs
// SqlDriver — RDD SQL generico (XSharp SQL RDD).
// Supporta SQL Server (Microsoft.Data.SqlClient) e SQLite (Microsoft.Data.Sqlite).
// PostgreSQL/MySQL/Oracle richiedono pacchetti NuGet aggiuntivi.
// Carica risultati in DataTable per navigazione bidirezionale.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Data;
using System.Data.Common;
using System.Linq;
using Microsoft.Data.Sqlite;

namespace Basic.Databases.Rdd;

[DatabaseDriver("SQL", Extensions = new string[0], Description = "SQL database driver (SQL Server, SQLite)")]
public class SqlDriver : BaseDriver
{
    private DbConnection? _connection;
    private DbProviderFactory? _factory;
    // Nome FullName della factory attiva (es. "Npgsql.NpgsqlFactory") — il driver NON
    // referenzia i tipi ADO concreti (Npgsql/SqlClient/...) per il dispatch: decide la
    // famiglia dal NOME factory (stringa), come LastInsertRowIdSql. Questo è il decoupling
    // che abilita il taglio per-provider (NativeAOT §9.7): il core parla solo DbFactory +
    // nome; i driver esterni diventano lazy senza che SqlDriver debba referenziarli.
    private string _factoryName = "";
    private bool _isNpgsql;
    private DbTransaction? _activeTransaction;
    private DataTable? _data;
    private int _currentRow = -1;
    private string _connectionString = "";
    private string _tableName = "";
    private string _whereClause = "";
    private string _orderBy = "";
    internal SchemaBuffer? _schemaBuf;
    private readonly Dictionary<string, int> _colNameToIdx = new(StringComparer.OrdinalIgnoreCase);

    // Ultima riga modificata (portabile: catturata dal ritorno di ExecuteNonQuery, valida su
    // QUALSIASI provider — niente funzione changes() specifica SQLite).
    private long _lastAffectedRows;

    // Coarse-park: nomi delle virtual table file-backed (csv/parquet) create su questa
    // connessione. Una scrittura su di esse riscrive un file intero -> va eseguita fuori dal
    // worker GPM (ParkBlocking), non sincrona. Mirror del classificatore C in gmb_sql.c.
    private readonly HashSet<string> _fileVtabs = new(StringComparer.OrdinalIgnoreCase);

    // Raw SQL result set registry (materializzato: SqlResultSet, niente DataReader vivi)
    private static readonly ConcurrentDictionary<int, SqlResultSet> _resultSets = new();
    private static int _nextResultSetId;
    private static readonly object _rsLock = new();

    public override string DriverName => "SQL";

    public SqlDriver() { }

    public override void Open(DbOpenInfo info)
    {
        _connectionString = info.FileName;   // connection string
        _tableName = info.Alias;             // nome tabella
        _whereClause = "";
        _orderBy = "";

        try
        {
            // Auto-detect del provider dalla connection string (per TIPO factory).
            // Percorso FILE puro (senza keyword) → SQLite, wrappato in "Data Source=".
            if (!info.FileName.Contains('='))
                _connectionString = $"Data Source={info.FileName}";
            _factory = DetectFactory(_connectionString);
            _factoryName = _factory?.GetType().FullName ?? "";
            _isNpgsql = _factoryName.Contains("Npgsql", StringComparison.OrdinalIgnoreCase);

            _connection = _factory.CreateConnection();
            _connection!.ConnectionString = _connectionString;
            _connection.Open();
            // UDF temporali (TIMEBUCKET/FIRST_TS/LAST_TS, parità C gmb_sql.c): solo su SQLite
            // (altri provider — Npgsql ecc. — hanno le proprie funzioni e niente CreateFunction).
            if (_connection is Microsoft.Data.Sqlite.SqliteConnection sc)
            {
                Basic.SQLite.SqlTimeUdf.Register(sc);
                Basic.SQLite.SqlStatsUdf.Register(sc);
                Basic.SQLite.SqlCsvExtension.TryLoad(sc);
            }
            _isOpen = true;
            LoadTableData();
        }
        catch (Exception ex)
        {
            System.Console.Error.WriteLine($"SQL OPEN ERROR: {ex.Message}");
            _isOpen = false;
        }
    }

    /// <summary>Carica una factory per NOME (type + assembly) con probe robusto — mai ref statico
    /// al driver → escludibile da illink/NativeAOT (il driver si distribuisce accanto al binario
    /// solo se serve). Ordine: (1) assembly già caricati, (2) Type.GetType nel contesto corrente,
    /// (3) dll nella BaseDirectory (fortemente nominati: Assembly.Load per nome senza token fallisce).
    /// I factory ADO sono SINGLETON (XxxFactory.Instance, senza ctor publico): si legge la property
    /// statica "Instance", con fallback Activator per i provider che espongono un ctor.</summary>
    private static DbProviderFactory? LoadProviderFactory(string typeFullName, string asm)
    {
        Type? t = null;
        foreach (var a in System.AppDomain.CurrentDomain.GetAssemblies())
            if (a.GetName().Name?.Equals(asm, StringComparison.OrdinalIgnoreCase) == true) { t = a.GetType(typeFullName, false); if (t != null) break; }
        t ??= Type.GetType($"{typeFullName}, {asm}", throwOnError: false);
        if (t is null)
        {
            try
            {
                var path = System.IO.Path.Combine(AppContext.BaseDirectory, asm + ".dll");
                if (System.IO.File.Exists(path))
                {
                    var a2 = System.Reflection.Assembly.LoadFrom(path);
                    t = a2.GetType(typeFullName, throwOnError: false);
                }
            }
            catch { t = null; }
        }
        if (t is null) return null;
        var instanceProp = t.GetProperty("Instance", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
        if (instanceProp?.GetValue(null) is DbProviderFactory single) return single;
        var instanceField = t.GetField("Instance", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
        if (instanceField?.GetValue(null) is DbProviderFactory singleField) return singleField;
        try { return (DbProviderFactory)Activator.CreateInstance(t)!; } catch { return null; }
    }

    /// <summary>Risolve il DbProviderFactory dalla connection string (per keyword) — LAZY
    /// (NativeAOT §9.7): i driver RDBMS esterni (Npgsql/Oracle/MySql/Firebird/SqlClient) sono
    /// caricati per NOME assembly via reflection, MAI referenziati staticamente → Databases
    /// (core) non li trascina nel binario. Sqlite resta STATICO (core, ~1MB, usato da tutto).
    /// Se l'assembly driver non è raggiungibile a runtime → errore chiaro (fallback = Sqlite
    /// non corretto: il keyword-set non matcherebbe; meglio fallire con messaggio esplicito).
    /// MySQL/MariaDB: Uid= e Port=3306/3307 disambiguano da Oracle/SqlClient.</summary>
    internal static DbProviderFactory DetectFactory(string connectionString)
    {
        // Percorso FILE puro (senza keyword di connection string): è SQLite.
        if (!connectionString.Contains('='))
            return Microsoft.Data.Sqlite.SqliteFactory.Instance;
        // Mappa keyword → factory (type + assembly, lazy). L'ordine segue la specificità:
        // TNS/Oracle prima di User Id; Uid=/3306 prima di Host= (MySQL vs PG).
        (string Needle, string Type, string Asm)[] map =
        {
            ("TNS", "Oracle.ManagedDataAccess.Client.OracleClientFactory", "Oracle.ManagedDataAccess"),
            ("Oracle", "Oracle.ManagedDataAccess.Client.OracleClientFactory", "Oracle.ManagedDataAccess"),
            ("Dialect=", "FirebirdSql.Data.FirebirdClient.FirebirdClientFactory", "FirebirdSql.Data.FirebirdClient"),
            ("ServerType=", "FirebirdSql.Data.FirebirdClient.FirebirdClientFactory", "FirebirdSql.Data.FirebirdClient"),
            ("Port=3306", "MySqlConnector.MySqlConnectorFactory", "MySqlConnector"),
            ("Port=3307", "MySqlConnector.MySqlConnectorFactory", "MySqlConnector"),
            ("Uid=", "MySqlConnector.MySqlConnectorFactory", "MySqlConnector"),
            ("Allow User Variables", "MySqlConnector.MySqlConnectorFactory", "MySqlConnector"),
            ("Host=", "Npgsql.NpgsqlFactory", "Npgsql"),
            ("Host ", "Npgsql.NpgsqlFactory", "Npgsql"),
            ("Server=", "Microsoft.Data.SqlClient.SqlClientFactory", "Microsoft.Data.SqlClient"),
            ("Server ", "Microsoft.Data.SqlClient.SqlClientFactory", "Microsoft.Data.SqlClient"),
        };
        foreach (var (needle, typeFull, asm) in map)
        {
            if (!connectionString.Contains(needle, StringComparison.OrdinalIgnoreCase)) continue;
            var f = LoadProviderFactory(typeFull, asm);
            if (f is not null) return f;
            throw new InvalidOperationException(
                $"SQL OPEN: provider '{typeFull}' non trovato (assembly '{asm}'). Il driver per questa " +
                $"connection string non è incluso nel binario (NativeAOT lazy — distribuisci la dll del driver accanto all'eseguibile).");
        }
        if (connectionString.Contains("User Id=", StringComparison.OrdinalIgnoreCase)
            && connectionString.Contains("Data Source=", StringComparison.OrdinalIgnoreCase))
        {
            var of = LoadProviderFactory("Oracle.ManagedDataAccess.Client.OracleClientFactory", "Oracle.ManagedDataAccess");
            if (of is not null) return of;
        }
        if (connectionString.Contains("Data Source=", StringComparison.OrdinalIgnoreCase)
            && !connectionString.Contains("Server=", StringComparison.OrdinalIgnoreCase)
            && !connectionString.Contains("Host=", StringComparison.OrdinalIgnoreCase))
            return Microsoft.Data.Sqlite.SqliteFactory.Instance;
        throw new InvalidOperationException("SQL OPEN: connection string non riconosciuta (nessun provider driver disponibile).");
    }

    public override void Close()
    {
        _activeTransaction?.Dispose(); _activeTransaction = null;
        _data?.Dispose(); _data = null;
        _connection?.Close(); _connection?.Dispose(); _connection = null;
        _isOpen = false;
    }

    /// <summary>Crea un comando sulla connessione, agganciato alla transazione ADO.NET attiva
    /// (Npgsql/SqlClient): senza cmd.Transaction Npgsql lascia lo stato in attesa e le
    /// operazioni escono dalla transazione.</summary>
    private DbCommand CreateCommand(string? sql = null)
    {
        var cmd = _connection!.CreateCommand();
        if (_activeTransaction != null) cmd.Transaction = _activeTransaction;
        if (sql != null) cmd.CommandText = sql;
        return cmd;
    }

    /// <summary>Comando con SQL normalizzato (placeholder deterministici). Il Prepare()
    /// avviene DOPO il bind dei parametri (vedi ExecRaw/QueryRaw): preparare prima lascia
    /// una prepared statement con 0 parametri → Npgsql rifiuta il bind.</summary>
    private DbCommand GetPreparedCommand(string sql)
    {
        return CreateCommand(NormalizeSql(sql));
    }

    /// <summary>Best-effort Prepare() dopo il bind dei parametri (riuso piani: la cache
    /// server-side di Npgsql/SQLite è keyed su testo+parametri).</summary>
    private static void PrepareIfNeeded(DbCommand cmd)
    {
        try { cmd.Prepare(); } catch { /* provider senza Prepare(): usa non preparato */ }
    }

    /// <summary>Riscrive i placeholder posizionali in modo deterministico per il provider:
    /// $1..$N per Npgsql, $p0..$pN-1 per gli altri (SQLite/MySQL/SQL Server).</summary>
    private string NormalizeSql(string sql)
    {
        if (!sql.Contains('?')) return sql;
        var isNpgsql = _isNpgsql;
        var sb = new System.Text.StringBuilder(sql.Length + 8);
        int idx = 0;
        foreach (var ch in sql)
        {
            if (ch == '?') { idx++; sb.Append(isNpgsql ? $"${idx}" : $"$p{idx - 1}"); }
            else sb.Append(ch);
        }
        return sb.ToString();
    }

    // ═══ Navigazione ═══

    public override void GoTop() { _currentRow = HasRows ? 0 : -1; SyncState(); }
    public override void GoBottom() { _currentRow = HasRows ? _data!.Rows.Count - 1 : -1; SyncState(); }
    public override void GoTo(long n) { if (n >= 1 && n <= (_data?.Rows.Count ?? 0)) _currentRow = (int)n - 1; SyncState(); }
    
    public override void Skip(int n)
    {
        _currentRow += n;
        if (_data != null && _currentRow >= _data.Rows.Count) _currentRow = _data.Rows.Count;
        SyncState();
    }
    public override void SkipRaw(int n) => Skip(n);

    private bool HasRows => _data != null && _data.Rows.Count > 0;

    // ═══ CRUD ═══

    public override long Append(Dictionary<int, object?> fields)
    {
        if (_connection == null || string.IsNullOrEmpty(_tableName)) return 0;
        try
        {
            var cols = new List<string>();
            var vals = new List<object?>();
            foreach (var kv in fields)
            {
                var fi = _fields.FirstOrDefault(f => f.SymbolId == kv.Key);
                if (fi != null) { cols.Add(fi.Name); vals.Add(kv.Value ?? DBNull.Value); }
            }
            if (cols.Count == 0) return 0;
            var placeholders = string.Join(",", cols.Select(_ => "?"));
            var sql = $"INSERT INTO {_tableName} ({string.Join(",", cols)}) VALUES ({placeholders})";
            ExecuteNonQuery(sql, vals);
            LoadTableData();
            return _recNo;
        }
        catch { return 0; }
    }

    public override void Delete()
    {
        var pk = GetPrimaryKeyValue();
        if (pk == null) return;
        var savedRow = _currentRow;
        ExecuteNonQuery($"DELETE FROM {_tableName} WHERE {pk.Value.col} = ?", [pk.Value.val]);
        LoadTableData();
        // LoadTableData riazzera SEMPRE _currentRow a 0: dopo aver cancellato la riga alla
        // posizione fisica N, tutte le righe successive si spostano su di una posizione
        // (N+1 diventa N). ScopedOperation (RDD DELETE ALL/FOR) chiama SEMPRE Skip(1) subito
        // dopo questa azione (per avanzare al record successivo): posizionarsi qui già sulla
        // riga "successiva" (savedRow) produrrebbe un doppio avanzamento che la salterebbe
        // di nuovo. savedRow-1 lascia che sia lo Skip(1) esterno a compiere l'unico
        // avanzamento reale, atterrando correttamente sulla riga shiftata in savedRow (prima
        // versione di questo fix: usava savedRow direttamente, ma il doppio avanzamento con
        // Skip(1) esterno faceva comunque saltare una riga su due — verificato dal vivo).
        _currentRow = savedRow - 1;
        GetCurrentRecord();
        SyncState();
    }

    public override void Recall()
    {
        // SQL: DELETE è fisico (nessun flag di cancellato come xBase) → nessun undelete.
        throw new NotSupportedException("SQL: DELETE è fisico, RECALL non è applicabile.");
    }

    public override void Pack()
    {
        _data?.AcceptChanges();
        SyncState();
    }

    public override void Zap()
    {
        ExecuteNonQuery($"DELETE FROM {_tableName}", Array.Empty<object?>());
        _data?.Clear(); _currentRow = -1;
        SyncState();
    }

    public override Dictionary<int, object?>? GetCurrentRecord()
    {
        if (_data == null || _currentRow < 0 || _currentRow >= _data.Rows.Count) return null;
        var row = _data.Rows[_currentRow];
        var result = new Dictionary<int, object?>();
        for (int i = 0; i < _fields.Length && i < _data.Columns.Count; i++)
            result[_fields[i].SymbolId] = row[i] == DBNull.Value ? null : row[i];
        if (_schemaBuf != null)
            _schemaBuf.FromDictionary(result);
        return result;
    }

    public override void PutCurrentRecord(Dictionary<int, object?> fields)
    {
        var pk = GetPrimaryKeyValue();
        if (pk == null) return;
        var sets = new List<string>();
        var vals = new List<object?>();
        foreach (var kv in fields)
        {
            var fi = _fields.FirstOrDefault(f => f.SymbolId == kv.Key);
            if (fi != null && fi.Name != pk.Value.col) { sets.Add($"{fi.Name} = ?"); vals.Add(kv.Value ?? DBNull.Value); }
        }
        if (sets.Count == 0) return;
        var savedRow = _currentRow;
        vals.Add(pk.Value.val);
        ExecuteNonQuery($"UPDATE {_tableName} SET {string.Join(",", sets)} WHERE {pk.Value.col} = ?", vals);
        LoadTableData();
        // LoadTableData riazzera SEMPRE _currentRow a 0 — l'UPDATE non rimuove righe (stesso
        // conteggio), quindi restare sullo stesso indice fisico è corretto (stesso motivo di
        // Delete() sopra: ScopedOperation fa Skip(1) dopo ogni azione assumendo il cursore
        // fermo sul record appena processato, non un reload che lo riportava sempre a 0 —
        // "RDD REPLACE ... ALL" avrebbe processato solo metà dei record).
        _currentRow = Math.Min(savedRow, (_data?.Rows.Count ?? 1) - 1);
        GetCurrentRecord();
        SyncState();
    }

    // ═══ Ordini / Seek ═══

    public override void CreateIndex(DbOrderInfo info)
    {
        if (_connection != null && !string.IsNullOrEmpty(info.Name))
        {
            var unique = info.Unique ? "UNIQUE " : "";
            ExecuteNonQuery($"CREATE {unique}INDEX IF NOT EXISTS {info.Name} ON {_tableName} ({info.KeyExpression})", Array.Empty<object?>());
        }
    }
    public override void OpenIndex(string name, string filePath)
    {
        // SQL: gli indici vivono NEL database (CREATE INDEX) — non esiste un file indice
        // esterno da aprire. Attiva l'ordine per nome (SET ORDER → ORDER BY), come gli
        // indici embedded di ISAM/SPEED.
        SetOrder(name);
    }
    public override void CloseIndex(string name)
    {
        if (_connection != null && !string.IsNullOrEmpty(name))
            ExecuteNonQuery($"DROP INDEX IF EXISTS {name}", Array.Empty<object?>());
    }
    public override void SetOrder(string name) { _orderBy = name; LoadTableData(); }

    public override bool Seek(DbSeekInfo info)
    {
        if (info.Key == null || _data == null) return false;
        for (int i = 0; i < _data.Rows.Count; i++)
            for (int c = 0; c < _data.Columns.Count; c++)
                if (Equals(_data.Rows[i][c], info.Key))
                { _currentRow = i; _found = true; SyncState(); return true; }
        _found = false; return false;
    }

    public override string? CurrentKey() => null;
    public override int CurrentKeyNo() => _currentRow;

    // ═══ Lock ═══
    // SQL: il controllo di concorrenza è del MOTORE (transazioni/isolamento) — i lock del
    // driver sono advisory (Lock/FileLock restituiscono true). Unlock/FileUnlock sono
    // deliberatamente no-op, documentati (niente lock a livello di driver).

    public override bool Lock(long rec) => true;
    public override void Unlock(long rec) { /* advisory — il motore gestisce la concorrenza */ }
    public override bool FileLock() => true;
    public override void FileUnlock() { /* advisory — il motore gestisce la concorrenza */ }

    // ═══ Filtri ═══

    public override void SetFilter(string? expression)
    {
        _whereClause = expression ?? "";
        LoadTableData();
    }

    // ═══ Info ═══

    public override object? Info(int infoType, object? param) => infoType switch
    {
        1 => _recNo, 2 => _recCount, 3 => _fields.Length, 4 => _alias,
        5 => _connectionString, 6 => _isOpen, _ => null
    };

    // ═══ Helpers ═══

    private void LoadTableData()
    {
        if (_connection == null || string.IsNullOrEmpty(_tableName)) return;
        try
        {
            var sql = $"SELECT * FROM {_tableName}";
            if (!string.IsNullOrEmpty(_whereClause)) sql += $" WHERE {_whereClause}";
            if (!string.IsNullOrEmpty(_orderBy)) sql += $" ORDER BY {_orderBy}";

            using var cmd = CreateCommand(sql);
            using var reader = cmd.ExecuteReader();
            _data?.Dispose();
            _data = new DataTable();
            _data.Load(reader);

            _colNameToIdx.Clear();
            var fieldList = new List<RddFieldInfo>();
            for (int i = 0; i < _data.Columns.Count; i++)
            {
                _colNameToIdx[_data.Columns[i].ColumnName] = i;
                fieldList.Add(new RddFieldInfo(_data.Columns[i].ColumnName, new DoubleType(), _data.Columns[i].MaxLength, i, i));
            }
            _fields = fieldList.ToArray();
            // Build SchemaBuffer con offset calcolati
            var foffsets = new FieldOffsetInfo[fieldList.Count];
            int off = 0;
            for (int i = 0; i < fieldList.Count; i++)
            {
                var fi = fieldList[i];
                foffsets[i] = new FieldOffsetInfo(fi.Name, off, Math.Max(4, fi.Length), fi.SymbolId, typeof(object));
                off += Math.Max(4, fi.Length);
            }
            _schemaBuf = new SchemaBuffer(foffsets);
            // Auto-naviga al primo record per popolare lo SchemaBuffer
            if (_data.Rows.Count > 0) { _currentRow = 0; GetCurrentRecord(); }
            SyncState();
        }
        catch (Exception ex) { System.Console.Error.WriteLine($"SQL LoadTableData: {ex.Message}"); }
    }

    private (string col, object? val)? GetPrimaryKeyValue()
    {
        if (_data == null || _currentRow < 0 || _currentRow >= _data.Rows.Count || _data.Columns.Count == 0) return null;
        var col = _data.Columns[0].ColumnName;
        for (int i = 0; i < _data.Columns.Count; i++)
            if (_data.Columns[i].ColumnName.Equals("ID", StringComparison.OrdinalIgnoreCase))
            { col = _data.Columns[i].ColumnName; break; }
        return (col, _data.Rows[_currentRow][col]);
    }

    private void ExecuteNonQuery(string sql, IReadOnlyList<object?> parameters)
    {
        if (_connection == null) return;
        try
        {
            // Stesso path normalizzato/prepared di ExecRaw: i "?" vengono riscritti in
            // $p0.. (SQLite) o $1..$N (Npgsql) con parametri tipizzati — prima qui si usava
            // "?" + parametri senza nome → su PostgreSQL il binding falliva (null in PK).
            using var cmd = GetPreparedCommand(sql);
            BindParameters(cmd, parameters, null);
            PrepareIfNeeded(cmd);
            _lastAffectedRows = cmd.ExecuteNonQuery();
        }
        catch { }
    }

    private void SyncState()
    {
        _eof = !HasRows || _currentRow >= _data!.Rows.Count;
        _bof = _currentRow < 0;
        _found = _currentRow >= 0 && _currentRow < (_data?.Rows.Count ?? 0);
        _recNo = _currentRow + 1;
        _recCount = _data?.Rows.Count ?? 0;
    }

    // ═══════════════════════════════════════════════════════════
    // SQL Raw — implementazione completa (SQLite + SQL Server)
    // ═══════════════════════════════════════════════════════════

    public override long ExecRaw(string sql, IReadOnlyList<object?>? positional, IReadOnlyDictionary<string, object?>? named)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        using var cmd = GetPreparedCommand(sql);
        BindParameters(cmd, positional, named);
        PrepareIfNeeded(cmd);
        var affected = cmd.ExecuteNonQuery();
        _lastAffectedRows = affected;
        TrackFileVtab(sql);
        return affected;
    }

    // ── Coarse-park: discriminante "scrittura PESANTE su vtab file-backed" (mirror C) ──
    private static bool CiEq(string a, string b) => string.Equals(a, b, StringComparison.OrdinalIgnoreCase);
    private static bool CiFind(string hay, string needle) => hay.IndexOf(needle, StringComparison.OrdinalIgnoreCase) >= 0;
    private static int NextToken(string s, ref int i, out string tok)
    {
        tok = "";
        int n = s.Length;
        while (i < n)
        {
            char ch = s[i];
            if (char.IsWhiteSpace(ch)) { i++; continue; }
            if (ch == '-' && i + 1 < n && s[i + 1] == '-') { while (i < n && s[i] != '\n') i++; continue; }
            if (ch == '/' && i + 1 < n && s[i + 1] == '*') { i += 2; while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) i++; i = System.Math.Min(n, i + 2); continue; }
            break;
        }
        if (i >= n) return 0;
        char q = s[i];
        if (q == '"' || q == '`' || q == '[')
        {
            char close = q == '[' ? ']' : q; i++;
            int st = i; while (i < n && s[i] != close) i++;
            tok = s.Substring(st, i - st); if (i < n) i++;
            return 1;
        }
        int start = i;
        while (i < n && (char.IsLetterOrDigit(s[i]) || s[i] == '_' || s[i] == '.' || s[i] == '$')) i++;
        tok = s.Substring(start, i - start);
        return tok.Length > 0 ? 1 : 0;
    }
    private void TrackFileVtab(string sql)
    {
        if (string.IsNullOrEmpty(sql)) return;
        int i = 0; var toks = new List<string>();
        for (int k = 0; k < 6 && NextToken(sql, ref i, out var t) == 1; k++) toks.Add(t);
        if (toks.Count >= 4 && CiEq(toks[0], "create") && CiEq(toks[1], "virtual") && CiEq(toks[2], "table")
            && (CiFind(sql, "using csv") || CiFind(sql, "using parquet")))
            _fileVtabs.Add(toks[3]);
    }
    private bool IsFileVtab(string name) => !string.IsNullOrEmpty(name) && _fileVtabs.Contains(name);
    /// <summary>true = scrittura pesante (riscrittura file csv/parquet) da eseguire fuori dal worker GPM.</summary>
    public bool IsHeavyWrite(string sql)
    {
        if (string.IsNullOrEmpty(sql)) return false;
        int i = 0; var toks = new List<string>();
        for (int k = 0; k < 6 && NextToken(sql, ref i, out var t) == 1; k++) toks.Add(t);
        if (toks.Count == 0) return false;
        if (CiEq(toks[0], "create"))
        {
            var isFileVtabCreate = toks.Count >= 4 && CiEq(toks[1], "virtual") && CiEq(toks[2], "table")
                && (CiFind(sql, "using csv") || CiFind(sql, "using parquet"));
            // Registra il nome QUI, in classificazione (sincrona, sul thread che esegue lo
            // statement in ordine), non solo in TrackFileVtab (chiamata da ExecRaw DOPO che
            // l'operazione — eseguita su un thread park in background, ParkBlocking — è
            // davvero finita). Il coarse-park CREATE+INSERT/UPDATE/DELETE successivi sono
            // statement SEPARATI con park SEPARATI: senza questa registrazione anticipata,
            // lo statement successivo poteva essere classificato PRIMA che il park del CREATE
            // avesse avuto modo di marcare "t" come file-vtab, perdendo il coarse-park (bug
            // AiWiki SQL-Parquet-CoarsePark-Race-Cross-Test — write non parcheggiata, corsa
            // con la CleanupWorkDir del file successivo nello stesso processo xUnit).
            if (isFileVtabCreate) _fileVtabs.Add(toks[3]);
            return isFileVtabCreate;
        }
        if ((CiEq(toks[0], "insert") || CiEq(toks[0], "replace")) && toks.Count >= 3 && CiEq(toks[1], "into")) return IsFileVtab(toks[2]);
        if (CiEq(toks[0], "update") && toks.Count >= 2) return IsFileVtab(toks[1]);
        if (CiEq(toks[0], "delete") && toks.Count >= 3 && CiEq(toks[1], "from")) return IsFileVtab(toks[2]);
        if ((CiEq(toks[0], "drop") || CiEq(toks[0], "alter")) && toks.Count >= 3 && CiEq(toks[1], "table")) return IsFileVtab(toks[2]);
        return false;
    }

    public override int QueryRaw(string sql, IReadOnlyList<object?>? positional, IReadOnlyDictionary<string, object?>? named)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        // MATERIALIZZA tutte le righe a QUERY time (portabile, indipendente dal DataReader):
        // abilita rs(i) per indice, rs.COUNT e un cursore ri-percorribile.
        var cmd = GetPreparedCommand(sql);
        BindParameters(cmd, positional, named);
        PrepareIfNeeded(cmd);
        using var reader = cmd.ExecuteReader();
        var columns = new List<string>();
        for (int i = 0; i < reader.FieldCount; i++) columns.Add(reader.GetName(i));
        var rows = new List<IReadOnlyDictionary<string, object?>>();
        while (reader.Read())
        {
            var row = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
            for (int i = 0; i < reader.FieldCount; i++)
                row[columns[i]] = reader.IsDBNull(i) ? null : reader.GetValue(i);
            rows.Add(row);
        }
        var resultSet = new SqlResultSet(columns, rows);
        int id;
        lock (_rsLock) { id = ++_nextResultSetId; }
        _resultSets[id] = resultSet;
        return id;
    }

    public override bool Fetch(int resultSetId)
    {
        if (!_resultSets.TryGetValue(resultSetId, out var rs))
            throw new InvalidOperationException($"Result set {resultSetId} is not open.");
        return rs.Fetch();
    }

    public override IReadOnlyDictionary<string, object?>? GetCurrentRow(int resultSetId)
    {
        if (!_resultSets.TryGetValue(resultSetId, out var rs))
            throw new InvalidOperationException($"Result set {resultSetId} is not open.");
        return rs.CurrentRow;
    }

    public override void CloseResultSet(int resultSetId)
    {
        _resultSets.TryRemove(resultSetId, out _);
    }

    /// <summary>Valore della colonna per INDICE nella riga corrente.</summary>
    public object? GetCurrentFieldByIndex(int resultSetId, int columnIndex)
    {
        if (!_resultSets.TryGetValue(resultSetId, out var rs))
            throw new InvalidOperationException($"Result set {resultSetId} is not open.");
        return rs.CurrentField(columnIndex);
    }

    /// <summary>Numero di righe del result set materializzato.</summary>
    public int ResultSetCount(int resultSetId)
    {
        if (!_resultSets.TryGetValue(resultSetId, out var rs))
            throw new InvalidOperationException($"Result set {resultSetId} is not open.");
        return rs.Count;
    }

    /// <summary>Numero di colonne del result set.</summary>
    public int ResultSetColumnCount(int resultSetId)
    {
        if (!_resultSets.TryGetValue(resultSetId, out var rs))
            throw new InvalidOperationException($"Result set {resultSetId} is not open.");
        return rs.ColumnCount;
    }

    /// <summary>Nomi delle colonne del result set (DESCRIBE/COL).</summary>
    public IReadOnlyList<string> ResultSetColumns(int resultSetId)
    {
        if (!_resultSets.TryGetValue(resultSetId, out var rs))
            throw new InvalidOperationException($"Result set {resultSetId} is not open.");
        return rs.Columns;
    }

    /// <summary>Valore (row,col) del result set senza muovere il cursore (COL/TOCSV).</summary>
    public object? ResultSetValue(int resultSetId, int row, int col)
    {
        if (!_resultSets.TryGetValue(resultSetId, out var rs))
            throw new InvalidOperationException($"Result set {resultSetId} is not open.");
        return rs.ValueAt(row, col);
    }

    /// <summary>SQL.FROMCSV: carica un CSV (header = nomi colonne) in una tabella. G10 (2026-10-09).</summary>
    public long LoadCsv(string table, string path)
    {
        try
        {
            var lines = System.IO.File.ReadAllLines(path);
            if (lines.Length == 0) return -1;
            var header = SqlCsvSplit(lines[0]);
            var cols = string.Join(",", header.Select(h => "\"" + h.Replace("\"", "\"\"") + "\" TEXT"));
            ExecuteNonQuery($"CREATE TABLE IF NOT EXISTS \"{table}\" ({cols})", Array.Empty<object?>());
            var ins = $"INSERT INTO \"{table}\" VALUES ({string.Join(",", header.Select((_, i) => "?"))})";
            for (int r = 1; r < lines.Length; r++)
                ExecuteNonQuery(ins, SqlCsvSplit(lines[r]).Cast<object?>().ToArray());
            return 0;
        }
        catch { return -1; }
    }

    private static string[] SqlCsvSplit(string line)
    {
        var outp = new List<string>(); var sb = new System.Text.StringBuilder(); bool q = false;
        for (int i = 0; i < line.Length; i++)
        {
            char ch = line[i];
            if (q) { if (ch == '"') { if (i + 1 < line.Length && line[i + 1] == '"') { sb.Append('"'); i++; } else q = false; } else sb.Append(ch); }
            else if (ch == '"') q = true;
            else if (ch == ',') { outp.Add(sb.ToString()); sb.Clear(); }
            else sb.Append(ch);
        }
        outp.Add(sb.ToString());
        return outp.ToArray();
    }

    public override void Attach(string path, string? schema)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        if (string.IsNullOrWhiteSpace(path)) throw new ArgumentException("Path required for ATTACH.");
        var schemaName = schema ?? "attached";
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = $"ATTACH DATABASE @path AS \"{schemaName.Replace("\"", "\"\"")}\"";
        var p = cmd.CreateParameter();
        p.ParameterName = "@path";
        p.Value = path;
        cmd.Parameters.Add(p);
        cmd.ExecuteNonQuery();
    }

    public override void Detach(string? schema)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        var schemaName = schema ?? "attached";
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = $"DETACH DATABASE \"{schemaName.Replace("\"", "\"\"")}\"";
        cmd.ExecuteNonQuery();
    }

    public override void BeginTransaction()
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        // Npgsql/SqlClient: transazione REALE via API ADO.NET (il testo raw "BEGIN TRANSACTION"
        // non è il modo corretto: Npgsql lascia lo stato in attesa e il ROLLBACK non annulla).
        // Deciso dal NOME factory (stringa), non dal tipo concreto — §9.7 (decoupling ADO).
        if (_isNpgsql || _factoryName.Contains("SqlClient", StringComparison.OrdinalIgnoreCase))
        {
            _activeTransaction = _connection.BeginTransaction();
            return;
        }
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "BEGIN TRANSACTION";
        cmd.ExecuteNonQuery();
    }

    public override void Commit()
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        if (_activeTransaction != null)
        {
            _activeTransaction.Commit();
            _activeTransaction.Dispose();
            _activeTransaction = null;
            return;
        }
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "COMMIT";
        cmd.ExecuteNonQuery();
    }

    public override void Rollback()
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        if (_activeTransaction != null)
        {
            _activeTransaction.Rollback();
            _activeTransaction.Dispose();
            _activeTransaction = null;
            return;
        }
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "ROLLBACK";
        cmd.ExecuteNonQuery();
    }

    // ═══ Savepoint (raw SQL — SQLite/PG/MySQL supportano SAVEPOINT) ═══

    private static string SafeSavepointName(string name)
        => string.IsNullOrWhiteSpace(name) ? "sp" : name.Replace("\"", "\"\"");

    public void Savepoint(string name)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = $"SAVEPOINT \"{SafeSavepointName(name)}\"";
        cmd.ExecuteNonQuery();
    }

    public void ReleaseSavepoint(string name)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = $"RELEASE SAVEPOINT \"{SafeSavepointName(name)}\"";
        cmd.ExecuteNonQuery();
    }

    public void RollbackToSavepoint(string name)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = $"ROLLBACK TO SAVEPOINT \"{SafeSavepointName(name)}\"";
        cmd.ExecuteNonQuery();
    }

    public override long LastInsertRowId()
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        var sql = LastInsertRowIdSql(_factory?.GetType().FullName);
        if (sql.Length == 0) return 0; // Oracle/Firebird: nessun rowid di sessione generico
        try
        {
            using var cmd = CreateCommand(sql);
            return Convert.ToInt64(cmd.ExecuteScalar()!, System.Globalization.CultureInfo.InvariantCulture);
        }
        catch { return 0; } // es. PG lastval() prima di qualsiasi nextval in sessione
    }

    /// <summary>SQL dell'ultimo rowid per NOME factory (stringa) — testabile SENZA driver/DB
    /// installati. "Ultimo rowid inserito" è per-connettore/sessione: ogni famiglia ha il suo.</summary>
    internal static string LastInsertRowIdSql(string? factoryTypeName)
    {
        var name = factoryTypeName ?? "";
        if (name.Contains("Sqlite", StringComparison.OrdinalIgnoreCase)) return "SELECT last_insert_rowid()";
        if (name.Contains("Npgsql", StringComparison.OrdinalIgnoreCase)) return "SELECT lastval()";
        // MySqlConnector (MySQL + MariaDB) e MySql.Data (Oracle) condividono "MySql".
        if (name.Contains("MySql", StringComparison.OrdinalIgnoreCase)
            || name.Contains("MariaDb", StringComparison.OrdinalIgnoreCase)) return "SELECT LAST_INSERT_ID()";
        if (name.Contains("SqlClient", StringComparison.OrdinalIgnoreCase)
            || name.Contains("SqlConnection", StringComparison.OrdinalIgnoreCase)) return "SELECT SCOPE_IDENTITY()";
        return ""; // Oracle/Firebird richiedono RETURNING / GEN_ID(seq): non generico
    }

    public override long Changes()
    {
        // Portabile: il conteggio è catturato dal RITORNO di ExecuteNonQuery al momento
        // dell'esecuzione (ExecRaw/ExecuteNonQuery) — funziona su QUALSIASI provider, senza
        // la funzione changes() specifica di SQLite.
        return _lastAffectedRows;
    }

    /// <summary>Runs an action on the underlying connection (for VectorDB use).</summary>
    internal void WithConnection(Action<DbConnection> action)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        action(_connection);
    }

    /// <summary>SQLite backup to destination file.</summary>
    public void Backup(string destinationPath)
    {
        if (_connection == null) throw new InvalidOperationException("Database not open.");
        if (_connection is SqliteConnection sqliteConn)
        {
            using var dest = new SqliteConnection($"Data Source={destinationPath}");
            dest.Open();
            sqliteConn.BackupDatabase(dest);
        }
        else throw new NotSupportedException("BACKUP is only supported for SQLite.");
    }

    /// <summary>Typed parameter: imposta DbType dal valore .NET (evita conversioni
    /// ambigue del provider e permette un binding esatto).</summary>
    private static void SetParameterType(DbParameter p, object? value)
    {
        switch (value)
        {
            case null: break; // valore default del provider
            case byte[]: p.DbType = DbType.Binary; break;
            case bool: p.DbType = DbType.Boolean; break;
            case DateTime: p.DbType = DbType.DateTime; break;
            case int or long or short or byte or sbyte: p.DbType = DbType.Int64; break;
            case uint or ulong or ushort: p.DbType = DbType.UInt64; break;
            case double or float or decimal: p.DbType = DbType.Double; break;
            default: p.DbType = DbType.String; break;
        }
    }

    private void BindParameters(DbCommand cmd, IReadOnlyList<object?>? positional, IReadOnlyDictionary<string, object?>? named)
    {
        // Il testo è GIÀ normalizzato (NormalizeSql in GetPreparedCommand): i placeholder
        // sono $1..$N (Npgsql) o $p0..$pN-1 (altri) — nessun re-scan di '?' qui.
        var isNpgsql = _isNpgsql;
        if (named is not null)
        {
            foreach (var (key, value) in named)
            {
                var name = key.StartsWith('@') || key.StartsWith('$') ? key : "@" + key;
                var p = cmd.CreateParameter();
                p.ParameterName = name;
                p.Value = value ?? DBNull.Value;
                SetParameterType(p, value);
                cmd.Parameters.Add(p);
            }
        }

        if (positional is null || positional.Count == 0) return;
        for (var i = 0; i < positional.Count; i++)
        {
            // Npgsql: parametri posizionali SENZA nome (bound per ordine ai $1..$N).
            var p = cmd.CreateParameter();
            if (!isNpgsql) p.ParameterName = $"$p{i}";
            p.Value = positional[i] ?? DBNull.Value;
            SetParameterType(p, positional[i]);
            cmd.Parameters.Add(p);
        }
    }
}
