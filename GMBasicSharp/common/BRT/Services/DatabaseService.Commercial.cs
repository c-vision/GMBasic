// FILE: Basic.Runtime.Services.DatabaseService.Commercial.cs
// Implementazione COMMERCIALE di IDatabaseService — motori C# nativi in modules/Database
// (Basic.DBF.DbfEngine, Basic.ISAM.Engine, Basic.Databases.Speed.Database,
// Basic.Ledger.LedgerManager). Compilata SOLO in edizione Commercial (vedi
// modules/Database/Database.csproj, esclusa in Community).
//
// Pattern: come la PCodeVM (GM-Basic PCodeProcessor.Syscalls.cs) — DataSession Rdd +
// driver (DbfDriver/IsamDriver/SpeedDriver) incapsulano i motori. Ogni handle long
// restituito da IDatabaseService mappa a un'area del DataSession.

using System.Collections.Concurrent;
using Basic.Databases.Rdd;

namespace Basic.Runtime;

public class CommercialDatabaseService : DatabaseService
{
    static CommercialDatabaseService()
    {
        // I driver commerciali (DBF/ISAM/SPEED) vivono in modules/Database (assembly
        // separato da common/Databases): registrali esplicitamente nel DriverRegistry.
        DriverRegistry.Register("DBF", typeof(Basic.Databases.Rdd.DbfDriver), [".dbf"], "dBASE / FoxPro DBF database");
        DriverRegistry.Register("ISAM", typeof(Basic.Databases.Rdd.IsamDriver), [".isdb"], "ISAM B+Tree database");
        DriverRegistry.Register("SPEED", typeof(Basic.Databases.Rdd.SpeedDriver), [".speed"], "SPEED LSM-tree key-value store");
    }

    private readonly DataSession _session;
    private readonly ConcurrentDictionary<long, int> _handleToArea = new();
    private long _isamHandle = -1;

    public CommercialDatabaseService() : this(new DataSession()) { }

    /// <summary>Condivide la DataSession del runtime host: RDD USE registra i driver nella
    /// STESSA session che il layer RDD di IlBackend legge (Current.SqlDataSession) — senza,
    /// USE e GETFIELD/REPLACE/DELETE vedono due sessioni diverse e i driver non si trovano.</summary>
    public CommercialDatabaseService(DataSession session)
    {
        _session = session;
    }

    private IDatabaseDriver? DriverFor(long handle)
    {
        if (_handleToArea.TryGetValue(handle, out var area))
            return _session.GetWorkArea(area)?.Driver;
        return null;
    }

    private void SyncChildRelations(IDatabaseDriver? moved)
    {
        if (moved == null) return;
        for (var a = 1; a <= DataSession.MaxWorkareas; a++)
        {
            var child = _session.GetWorkArea(a)?.Driver;
            if (child != null && child.HasActiveRelation && ReferenceEquals(child.ParentRelationDriver, moved))
            {
                try { child.SyncRelation(); } catch { }
            }
        }
    }

    private void ClearOutgoingRelations(IDatabaseDriver parent)
    {
        for (var a = 1; a <= DataSession.MaxWorkareas; a++)
        {
            var child = _session.GetWorkArea(a)?.Driver;
            if (child != null && child.HasActiveRelation && ReferenceEquals(child.ParentRelationDriver, parent))
                child.ClearRelation();
        }
    }

    private int AllocateAreaFor(long handle, IDatabaseDriver driver, string alias)
    {
        var area = _session.AllocateArea();
        _session.RegisterDirect(area, driver, alias);
        // Parità con l'ISAM OPEN del PCode (PCodeProcessor.Syscalls.cs, ramo ISAM_USE):
        // il row-buffer RDD risolve gli indici Clipper 1-based in SymbolId SOLO se la
        // work area espone Meta["Udt"]/["SchemaBuffer"]; senza, OLDVAL/TABLEUPDATE sul
        // ISAM usavano chiavi mai lette dal driver (modifiche perse, §ADR-RowBuffer).
        if (driver is Basic.Databases.Rdd.IsamDriver idrv && idrv.UdtDef is { } udt)
        {
            var wa = _session.GetWorkArea(area);
            if (wa != null)
            {
                wa.Meta["Udt"] = udt;
                if (idrv.SchemaBuf is { } sbuf) wa.Meta["SchemaBuffer"] = sbuf;
            }
        }
        _handleToArea[handle] = area;
        return area;
    }

    private IDatabaseDriver OpenDriver(string path, string alias, string driverKind)
    {
        var driver = DriverRegistry.Create(driverKind);
        if (driver is null)
            throw new CommercialEditionRequiredException($"Driver '{driverKind}' non disponibile.");
        driver.Open(new DbOpenInfo { FileName = path, Alias = alias });
        return driver;
    }

    // ================================================================
    // ISAM
    // ================================================================
    public override long IsamOpen(string path)
    {
        var driver = OpenDriver(path, "ISAM", "ISAM");
        var handle = NextHandle;
        AllocateAreaFor(handle, driver, "ISAM");
        return handle;
    }
    public override long IsamOpenSchema(string path, string alias, string schemaJson)
    {
        // Costruisce l'engine ISAM dallo schema (typeName, campi, chiave primaria,
        // indici) serializzato dal dispatcher — pattern PCodeVM ISAM_USE.
        try
        {
            var schemaDoc = System.Text.Json.JsonDocument.Parse(schemaJson);
            var root = schemaDoc.RootElement;
            var typeName = root.GetProperty("TypeName").GetString() ?? "R";
            var fields = new List<Basic.ISAM.Config.IsamFieldSchema>();
            if (root.TryGetProperty("Fields", out var fieldsEl))
            {
                foreach (var f in fieldsEl.EnumerateArray())
                {
                    var fname = f.GetProperty("Name").GetString() ?? "F";
                    var ftype = f.GetProperty("Type").GetString() ?? "STRING";
                    var flen = f.TryGetProperty("Length", out var lenEl) ? lenEl.GetInt32() : 0;
                    fields.Add(new Basic.ISAM.Config.IsamFieldSchema(fname, ParseIsamDataType(ftype), flen));
                }
            }
            // KeyLength degli indici = lunghezza del campo target (per STRING la
            // lunghezza dichiarata; per i numerici un default fisso). Con KeyLength=0
            // SerializeKeyValue produce una chiave vuota (byte[0]) → tutti i record
            // finiscono sotto la chiave '' → falsa violazione di unicità al 2° insert.
            int KeyLengthFor(string name)
            {
                var f = fields.FirstOrDefault(x => x.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
                if (f is null) return 20;
                return f.Length > 0 ? f.Length : 20;
            }
            Basic.ISAM.Config.IndexDefinition pk = new(typeName.Length > 0 && fields.Count > 0 ? fields[0].Name : "ID", fields.Count > 0 ? fields[0].Name : "ID", fields.Count > 0 ? KeyLengthFor(fields[0].Name) : 20, true, false);
            if (root.TryGetProperty("PrimaryKey", out var pkEl) && pkEl.GetString() is { } pkName && fields.Count > 0)
                pk = new Basic.ISAM.Config.IndexDefinition(pkName, pkName, KeyLengthFor(pkName), true, false);
            var secondary = new List<Basic.ISAM.Config.IndexDefinition>();
            if (root.TryGetProperty("Indexes", out var idxEl))
            {
                foreach (var ix in idxEl.EnumerateArray())
                {
                    if (ix.GetString() is { } ixName)
                        secondary.Add(new Basic.ISAM.Config.IndexDefinition(ixName, ixName, KeyLengthFor(ixName), false, false));
                }
            }
            var schema = new Basic.ISAM.Config.IsamSchema(typeName, fields, pk, secondary);
            var udt = Basic.Tools.Isam.IsamOfflineToolkit.BuildMinimalUdt(schema);
            var handle2 = new Basic.ISAM.IsamFileHandle(path, schema, udt, Basic.Databases.LockingStrategy.IntraProcess);
            var engine = new Basic.ISAM.Engine(handle2);
            engine.Evaluator = (expr, ctx, tt) =>
            {
                if (expr == "__recno__") return (double)engine.GetCurrentRecordNumber();
                // Lookup diretto per nome campo PRIMA della pipeline P-Code: un identificatore
                // nudo ("Saldo") compilato produce il load di una variabile non risolta = 0
                // (valore NON nullo) che intercettava il fallback → chiavi indice secondario
                // tutte 0 → ordine di inserimento invece dell'ordine dell'indice (CREATE INDEX
                // su campo non-PK). La pipeline P-Code resta per le espressioni composte
                // (STR(RECNO()), UPPER(Nome), …).
                if (ctx != null && ctx.TryGetValue(expr, out var val)) return val;
                if (Basic.Databases.KeyExpressionPCodeHook.CompileFactory != null)
                {
                    var compiled = Basic.Databases.KeyExpressionPCodeHook.CompileFactory(expr);
                    if (compiled != null)
                    {
                        var result = compiled(ctx);
                        if (result != null) return result;
                    }
                }
                return expr;
            };
            var driver = new Basic.Databases.Rdd.IsamDriver(engine, udt, alias);
            var h = NextHandle;
            AllocateAreaFor(h, driver, alias);
            return h;
        }
        catch
        {
            // Fallback: apri senza schema (driver generico) — i campi restano F0..Fn.
            return IsamOpen(path);
        }
    }
    private static Basic.DataType ParseIsamDataType(string t) => t.ToUpperInvariant() switch
    {
        "STRING" => new Basic.StringType(),
        "LONG" => new Basic.LongType(),
        "INTEGER" => new Basic.IntegerType(),
        "DOUBLE" => new Basic.DoubleType(),
        "CURRENCY" => new Basic.CurrencyType(),
        "BIT" => new Basic.BitType(),
        "BYTE" => new Basic.ByteType(),
        "SBYTE" => new Basic.SByteType(),
        "SINGLE" => new Basic.SingleType(),
        "ULONG" => new Basic.ULongType(),
        "UINTEGER" => new Basic.UIntegerType(),
        "QUAD" => new Basic.QuadType(),
        "UQUAD" => new Basic.UQuadType(),
        "DATE" => new Basic.DateType(),
        "TIME" => new Basic.TimeType(),
        "DATETIME" => new Basic.DateTimeType(),
        "TIMESPAN" => new Basic.TimeSpanType(),
        "JSON" => new Basic.JsonType(),
        _ => new Basic.StringType(),
    };
    public override void IsamClose(long handle)
    {
        if (_handleToArea.TryRemove(handle, out var area)) _session.Unregister(area);
    }
    public override void IsamInsert(long handle, object[] values)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        var fields = new Dictionary<int, object?>();
        for (var i = 0; i < values.Length; i++)
        {
            // Il motore ISAM indica i campi per SymbolId (UdtInstance), non per posizione.
            var sid = i < drv.Fields.Length ? drv.Fields[i].SymbolId : i;
            fields[sid] = values[i];
        }
        drv.Append(fields);
    }
    public override void IsamUpdate(long handle, long recno, object[] values)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.GoTo(recno);
        var fields = new Dictionary<int, object?>();
        for (var i = 0; i < values.Length; i++)
        {
            var sid = i < drv.Fields.Length ? drv.Fields[i].SymbolId : i;
            fields[sid] = values[i];
        }
        drv.PutCurrentRecord(fields);
    }
    public override void IsamPut(long handle, object[] values)
    {
        // ISAM PUT alias [, udtVar] — modifica il record CORRENTE (dopo SEEK/GO), non per
        // recno: come IsamUpdate ma SENZA GoTo. Pattern PCodeVM ISAM_PUT (Engine.Put).
        var drv = DriverFor(handle); if (drv is null) return;
        var fields = new Dictionary<int, object?>();
        for (var i = 0; i < values.Length; i++)
        {
            var sid = i < drv.Fields.Length ? drv.Fields[i].SymbolId : i;
            fields[sid] = values[i];
        }
        drv.PutCurrentRecord(fields);
    }
    public override void IsamDelete(long handle, long recno)
    {
        // Bug ISAM-Comandi-Parita-5-Backend (BUG 2): ISAM DELETE X (nessuno scope esplicito)
        // arriva qui con recno=0 — GoTo(0) lo tratta come "vai a TOP" (IsamDriver.GoTo: n<=1
        // -> GoTop), quindi un DELETE non scopato cancellava SEMPRE il primo record invece del
        // record CORRENTE (es. dopo una SEEK). Stesso guard gia' presente in IsamRead sotto
        // (if (recno > 0) drv.GoTo(recno)) — qui mancava.
        var drv = DriverFor(handle); if (drv is null) return;
        if (recno > 0) drv.GoTo(recno);
        drv.Delete();
    }
    public override object?[] IsamRead(long handle, long recno)
    {
        var drv = DriverFor(handle); if (drv is null) return Array.Empty<object?>();
        if (recno > 0) drv.GoTo(recno);
        var result = new List<object?>();
        var idx = 0;
        while (drv.GetFieldValue(idx) is { } v) { result.Add(v); idx++; }
        return [.. result];
    }
    public override object?[] IsamReadCurrent(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return Array.Empty<object?>();
        var result = new List<object?>();
        var idx = 0;
        while (drv.GetFieldValue(idx) is { } v) { result.Add(v); idx++; }
        return [.. result];
    }
    public override string[] IsamFieldNames(long handle)
    {
        var drv = DriverFor(handle);
        if (drv is null) return [];
        return drv.Fields.Select(f => f.Name).ToArray();
    }
    public override long IsamGetRecordCount(long handle)
    {
        var drv = DriverFor(handle); return drv is null ? 0 : drv.RecCount;
    }
    public override long IsamRecordSize(long handle)
    {
        // RECSIZE([area]) → lunghezza del record in BYTE. L'engine ISAM la conosce
        // (`Engine.RecordLength` = Header.RecordSize): la usiamo come fonte unica.
        // Prima si sommavano `Field.Length`, ma IsamDriver popola RddFieldInfo con
        // Length=0 → RECSIZE ritornava il NUMERO DI CAMPI (5) invece dei byte (~62).
        var drv = DriverFor(handle);
        if (drv is Basic.Databases.Rdd.IsamDriver idrv && idrv.Engine != null)
            return idrv.Engine.RecordLength;
        return drv is null ? 0 : drv.Fields.Sum(f => Math.Max(1, f.Length));
    }
    public override void IsamUse(long handle, string alias, object? type)
    {
        if (_handleToArea.TryGetValue(handle, out var area)) { /* alias già registrato */ }
    }
    // ISAM SELECT <alias>: prima aggiornava SOLO _isamHandle (tracker interno di questo
    // servizio), mai _session.CurrentArea (il tracker di DataSession) — le funzioni RDD
    // generiche RLOCK()/FLOCK()/UNLOCK area-corrente (RddLock/RddRLock/RddFLock in
    // IlRuntime.cs, che leggono session.CurrentArea, non _isamHandle) restavano quindi
    // ancorate all'area dell'ULTIMO OPEN/AllocateAreaFor, non a quella effettivamente
    // selezionata da un ISAM SELECT successivo. Con più aree ISAM aperte (es. Clienti poi
    // Ordini), "ISAM.FLOCK(Clienti)" dopo un "ISAM SELECT Clienti" bloccava ancora Ordini:
    // FLOCK e la successiva "ISAM UNLOCK -1" agivano su DUE DRIVER DIVERSI, lasciando il
    // lock di Clienti orfano per sempre → SynchronizationLockException al Dispose
    // (DbIsamFullTests.bas sez. 11, DbfCloseAll). Ora tiene sincronizzati entrambi.
    public override void IsamSelect(long workArea)
    {
        _isamHandle = workArea;
        if (_handleToArea.TryGetValue(workArea, out var area)) _session.CurrentArea = area;
    }
    public override void IsamGo(long handle, long recno)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        // -2 = sentinella BOTTOM (come SpeedGo); <=0 e diverso da -2 resta TOP.
        if (recno == -2) drv.GoBottom();
        else if (recno <= 0) drv.GoTop();
        else drv.GoTo(recno);
    }
    public override void IsamSkip(long handle, int count)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Skip(count);
    }
    public override void IsamSeek(long handle, object key)
    {
        var drv = DriverFor(handle);
        if (drv is null) return;
        drv.Seek(new DbSeekInfo { Key = key });
    }
    public override void IsamSetOrder(long handle, int indexNum) { }
    public override void IsamSetFilter(long handle, string? filterExpr)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.SetFilter(filterExpr);
    }
    public override void IsamPack(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Pack();
    }
    public override void IsamReindex(long handle)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        try { engine.Reindex(engine.Evaluator); } catch { }
    }
    public override bool IsamFound(long handle)
    {
        var drv = DriverFor(handle); return drv is not null && drv.Found;
    }
    public override bool IsamEof(long handle)
    {
        var drv = DriverFor(handle); return drv is not null && drv.Eof;
    }
    public override bool IsamBof(long handle)
    {
        var drv = DriverFor(handle); return drv is not null && drv.Bof;
    }

    // ── ISAM avanzate (motore Basic.ISAM.Engine) ──

    private Basic.ISAM.Engine? IsamEngineFor(long handle) =>
        DriverFor(handle) is Basic.Databases.Rdd.IsamDriver idrv ? idrv.Engine : null;

    private static Basic.ISAM.Engine.IsamExpressionEvaluator IsamEval(
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) =>
        (_, dict) => evaluator?.Invoke(dict) ?? true;

    public override void IsamRecall(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Recall();
    }
    public override void IsamSetFilterExpr(long handle, Basic.Expr? filterExpr,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        engine.SetFilter(IsamEval(evaluator), filterExpr);
    }
    public override void IsamCreateIndex(long handle, string indexName, string keyExpression, long keyLength)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        // Bug ISAM-RecallCurrent-Dopo-Delete-Offset-Invalido: se lo schema cresce oltre lo slot
        // corrente, EnsureSchemaSlot sposta fisicamente l'area dati (record inclusi) ma il
        // cursore del thread CHIAMANTE (offset assoluto stantio) non viene mai notificato —
        // un accesso successivo senza rinavigare legge byte a caso. Invalidare sempre, non solo
        // nel caso di successo: la relocation puo' essere gia' avvenuta anche se una fase
        // successiva dell'operazione lancia.
        try { engine.CreateIndexAsync(indexName, keyExpression, (int)keyLength, false, engine.Evaluator).Wait(); }
        catch { }
        finally { engine.InvalidateCursor(); }
    }
    public override void IsamDropIndex(long handle, string indexName)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        // Vedi commento in IsamCreateIndex — DropIndexAsync passa anch'esso da
        // EnsureSchemaSlot/EnsureSchemaSlot-equivalente quando lo schema si riduce (SchemaLength
        // non scende mai, ma la scrittura passa dallo stesso percorso di crescita/adattamento).
        try { engine.DropIndexAsync(indexName).Wait(); }
        catch { }
        finally { engine.InvalidateCursor(); }
    }
    public override void IsamRepair(long handle)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        try { engine.Repair(engine.Evaluator); } catch { }
    }
    public override void IsamBackup(long handle, string destPath)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        try { engine.Backup(destPath); } catch { }
    }
    public override bool IsamValidate(long handle)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return true;
        try { return engine.ValidateIntegrity().IsValid; } catch { return false; }
    }
    public override string IsamValidateJson(long handle)
    {
        // ISAM.INTEGRITY(alias) → JSON, pattern PCodeVM SysCall.ISAM_INTEGRITY
        // ({"valid":...,"message":...}); il runtime IL DEVE tornare lo stesso JSON, non un
        // double — assegnato a DIM x AS JSON generava IL invalido (mismatch tipo).
        var engine = IsamEngineFor(handle); if (engine is null) return "{}";
        try
        {
            var (isValid, message) = engine.ValidateIntegrity();
            return "{\"valid\":" + (isValid ? "true" : "false") + ",\"message\":\"" +
                (message ?? "").Replace("\"", "\\\"") + "\"}";
        }
        catch { return "{\"valid\":false,\"message\":\"\"}"; }
    }
    public override void IsamCopyTo(long handle, string destPath, Basic.Expr? condition,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        try { engine.CopyTo(destPath, condition, IsamEval(evaluator), engine.Evaluator); } catch { }
    }
    public override void IsamSortTo(long handle, string destPath, string[] fields, bool[] descending, Basic.Expr? condition,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        try
        {
            var sortFields = fields.Select((f, i) => (f, i < descending.Length && descending[i])).ToList();
            engine.SortTo(destPath, sortFields, condition, IsamEval(evaluator), engine.Evaluator);
        }
        catch { }
    }
    public override void IsamJoin(long handle, long secondaryHandle, string destPath, Basic.Expr condition,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var engine = IsamEngineFor(handle); var secondary = IsamEngineFor(secondaryHandle);
        if (engine is null || secondary is null) return;
        try { engine.JoinWith(secondary, destPath, condition, IsamEval(evaluator), engine.Evaluator); } catch { }
    }
    public override void IsamSetRelation(long handle, long parentHandle, string keyExpression)
    {
        var drv = DriverFor(handle); var parent = DriverFor(parentHandle);
        if (drv is null || parent is null) return;
        try { drv.SetRelation(parent, keyExpression); } catch { }
    }
    public override void IsamLock(long handle, bool isRecordLock)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        try { if (isRecordLock) drv.Lock(drv.RecNo); else drv.FileLock(); } catch { }
    }
    public override void IsamUnlock(long handle, bool isRecordLock)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        try { if (isRecordLock) drv.Unlock(drv.RecNo); else drv.FileUnlock(); } catch { }
    }

    // ================================================================
    // DBF
    // ================================================================
    public override long DbfOpen(string path) => DbfUse(path, [], 0, false, null, null, null);
    public override long DbfUse(string path, object?[] indexPaths, int locking, bool unsafeFlag, string? alias, object? typeId, string? typeName)
    {
        // typeName è overloaded fra due chiamanti: per `DBF USE ... AS Type` è il TIPO BASIC
        // (es. "Row") e NON va mai trattato come nome driver ("Driver 'Row' non trovato");
        // per `RDD USE ... VIA "driver"` (EmitRddUseStatement) è invece il nome del driver
        // esplicito da forzare, finora sempre ignorato — il driver era risolto SOLO per
        // estensione, quindi "RDD USE conn VIA SQL" apriva sempre il driver di default
        // (tipicamente ISAM) invece di SqlDriver, con un file non trovato o dati sbagliati
        // (scoperto verificando RDD_SQL_CRUD.bas: FileNotFoundException su IsamDriver.Open
        // per un connection string SQL). Distingue i due casi verificando se typeName è
        // REALMENTE il nome di un driver registrato — un tipo UDT come "Row" non lo è mai,
        // quindi il comportamento di DBF USE ... AS Type resta invariato.
        var driverKind = (!string.IsNullOrEmpty(typeName) &&
                           DriverRegistry.AvailableDrivers.Contains(typeName, StringComparer.OrdinalIgnoreCase))
            ? typeName
            : DriverRegistry.FindForExtension(path) ?? _session.DefaultDriver;
        var drv = OpenDriver(path, alias ?? driverKind, driverKind);
        var handle = NextHandle;
        AllocateAreaFor(handle, drv, alias ?? driverKind);
        return handle;
    }
    public override object?[] DbfGetRecord(long handle)
    {

        var drv = DriverFor(handle); if (drv is null) return Array.Empty<object?>();
        var result = new List<object?>();
        var idx = 0;
        while (drv.GetFieldValue(idx) is { } v) { result.Add(v); idx++; }
        return [.. result];
    }
    public override System.Collections.Generic.Dictionary<string, object?> DbfGetRecordNamed(long handle)
    {
        var dict = new System.Collections.Generic.Dictionary<string, object?>(System.StringComparer.OrdinalIgnoreCase);
        var drv = DriverFor(handle); if (drv is null) return dict;
        foreach (var f in drv.Fields)
        {
            var fi = drv.GetFieldIndex(f.Name);
            dict[f.Name] = fi >= 0 ? drv.GetFieldValue(fi) : null;
        }
        return dict;
    }
    public override void DbfReplace(long handle, int fieldIndex, object value)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.SetFieldValue(fieldIndex, value);
    }
    /// <summary>REPLACE per NOME campo (vedi IDatabaseService.DbfReplaceByName): risolve
    /// l'indice posizionale dal nome, stesso plumbing GetFieldIndex già usato da
    /// DbfFieldByName per le letture late-binding.</summary>
    public override void DbfReplaceByName(long handle, string fieldName, object? value)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        var idx = drv.GetFieldIndex(fieldName);
        if (idx >= 0) drv.SetFieldValue(idx, value);
    }
    public override void DbfGo(long handle, long recno)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        if (recno <= 0) drv.GoTop(); else drv.GoTo(recno);
        SyncChildRelations(drv);
    }
    public override void DbfGoBottom(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.GoBottom();
        SyncChildRelations(drv);
    }
    public override void DbfSkip(long handle, int count)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Skip(count);
        SyncChildRelations(drv);
    }
    public override bool DbfSeek(long handle, object key)
    {
        var drv = DriverFor(handle); if (drv is null) return false;
        var found = drv.Seek(new DbSeekInfo { Key = key });
        SyncChildRelations(drv);
        return found;
    }
    public override void DbfDelete(long handle, long recno)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.GoTo(recno);
        drv.Delete();
    }
    public override void DbfRecall(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Recall();
    }
    public override void DbfPack(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Pack();
    }
    public override void DbfZap(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Zap();
    }
    public override void DbfAppend(long handle, object[] values)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        var fields = new Dictionary<int, object?>();
        for (var i = 0; i < values.Length; i++) fields[i] = values[i];
        drv.Append(fields);
    }
    /// <summary>`DBF APPEND RECORD {Campo: valore, ...}`: riordina per NOME contro
    /// drv.Fields (stesso fix/motivo di SpeedInsertByName — un DictionaryLiteralExpr non
    /// garantisce l'ordine di schema, e prima non c'era ALCUN percorso per un dict-literal,
    /// EmitObjectArrayArg produceva un object[0] sempre vuoto).</summary>
    public override void DbfAppendByName(long handle, Dictionary<string, object> fields)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        var byIndex = new Dictionary<int, object?>();
        for (var i = 0; i < drv.Fields.Length; i++)
            if (fields.TryGetValue(drv.Fields[i].Name, out var v)) byIndex[drv.Fields[i].SymbolId] = v;
        drv.Append(byIndex);
    }
    /// <summary>Late-binding Clipper (campo nudo, es. `ITEM` dopo `DBF SELECT ORD`): legge
    /// il campo per NOME dal record corrente. Il plumbing (GetFieldIndex/GetFieldValue)
    /// esiste già su IDatabaseDriver/BaseDriver — mancava solo l'esposizione verso il
    /// backend IL (vedi IlRuntime.DbfLateBindField).</summary>
    public override object? DbfFieldByName(long handle, string fieldName)
    {
        var drv = DriverFor(handle); if (drv is null) return null;
        var idx = drv.GetFieldIndex(fieldName);
        return idx < 0 ? null : drv.GetFieldValue(idx);
    }
    public override int DbfFieldIndex(long handle, string fieldName)
    {
        var drv = DriverFor(handle); if (drv is null) return -1;
        return drv.GetFieldIndex(fieldName);
    }
    public override void DbfUpdate(long handle, long recno, object[] values)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.GoTo(recno);
        var fields = new Dictionary<int, object?>();
        for (var i = 0; i < values.Length; i++) fields[i] = values[i];
        drv.PutCurrentRecord(fields);
    }
    public override long DbfGetRecordCount(long handle)
    {
        var drv = DriverFor(handle); return drv is null ? 0 : drv.RecCount;
    }
    public override bool DbfEof(long handle)
    {
        var drv = DriverFor(handle);
        return drv is null || drv.Eof;
    }
    public override bool DbfBof(long handle)
    {
        var drv = DriverFor(handle); return drv is null || drv.Bof;
    }
    public override long DbfRecno(long handle)
    {
        var drv = DriverFor(handle); return drv is null ? 0 : drv.RecNo;
    }
    public override bool DbfFound(long handle)
    {
        var drv = DriverFor(handle); return drv is not null && drv.Found;
    }
    public override bool DbfIsDeleted(long handle)
    {
        // DBF.DELETED() era SEMPRE false: l'engine DbfEngine espone IsCurrentRecordDeleted()
        // pubblicamente (già usato da ConvertFromDbf per saltare i record cancellati), ma
        // questo metodo lo ignorava e restituiva un valore fisso — scoperto verificando
        // DBF DELETE (ALL) seguito da DBF.DELETED(), sempre 0 nonostante il record fosse
        // effettivamente marcato cancellato (visibile da RECALL, che lo ripristinava).
        var engine = DbfDriverFor(handle)?.Engine;
        return engine?.IsCurrentRecordDeleted() ?? false;
    }
    public override void DbfSetDeleted(long handle, bool on)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.SetDeleted(on);
    }
    public override void DbfSetOrder(long handle, int indexOrder)
    {
        var engine = DbfDriverFor(handle)?.Engine;
        if (engine is null) return;
        try { engine.SetActiveOrder(indexOrder); DbfDriverFor(handle)?.SyncCursorState(); } catch { }
    }
    public override void DbfSetOrderByName(long handle, string name)
    {
        var drv = DbfDriverFor(handle);
        var engine = drv?.Engine;
        if (engine is null) return;
        // Il nome dell'indice può differire per estensione/directory: `INDEX ... TO "x.ntx"`
        // registra Name="x.ntx", mentre `SET ORDER TO "x"` passa "x". Prova le varianti
        // (nome, basename, ±estensione index) finché una risolve l'ordine.
        foreach (var cand in DbfOrderNameCandidates(name))
        {
            try { engine.SetActiveOrder(cand); drv.SyncCursorState(); return; } catch { }
        }
    }

    /// <summary>Varianti di nome per risolvere un ordine indice: nome, basename, con/senza
    /// estensione (.ntx/.ndx/.cdx) e senza directory.</summary>
    private static IEnumerable<string> DbfOrderNameCandidates(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) yield break;
        var bn = System.IO.Path.GetFileName(name);
        var noExt = System.IO.Path.GetFileNameWithoutExtension(bn);
        yield return name;
        if (!string.Equals(bn, name, StringComparison.Ordinal)) yield return bn;
        foreach (var baseName in new[] { name, bn, noExt })
            foreach (var ext in new[] { ".ntx", ".ndx", ".cdx" })
                yield return baseName + ext;
        yield return noExt;
    }
    public override void DbfReindex(long handle, long unused)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return;
        try { engine.Reindex(); } catch { }
    }
    public override void DbfIndexOn(long handle, string keyExpr, string file, int unique)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        try
        {
            drv.CreateIndex(new DbOrderInfo
            {
                Name = file,
                KeyExpression = keyExpr,
                KeyLength = 50,
                Unique = unique != 0
            });
        }
        catch { }
    }
    public override void DbfCreateFile(string path, string fieldDefs)
    {
        // CREATE path FIELD_DEFS: crea il file DBF con i campi (formato "Nome=Tipo=Len,...").
        try
        {
            var fields = new List<Basic.DBF.DbfField>();
            foreach (var def in fieldDefs.Split(',', StringSplitOptions.RemoveEmptyEntries))
            {
                var parts = def.Split('=');
                if (parts.Length < 2 || string.IsNullOrWhiteSpace(parts[0])) continue;
                var name = parts[0].Trim();
                var type = parts[1].Trim().ToUpperInvariant();
                var len = parts.Length > 2 && byte.TryParse(parts[2], out var l) ? l : (byte)10;
                var ft = type switch
                {
                    "STRING" or "CHARACTER" => Basic.DBF.DbfFieldType.Character,
                    "INTEGER" => Basic.DBF.DbfFieldType.Integer,
                    "DOUBLE" => Basic.DBF.DbfFieldType.Double,
                    "LONG" or "NUMERIC" => Basic.DBF.DbfFieldType.Numeric,
                    "BIT" or "LOGICAL" => Basic.DBF.DbfFieldType.Logical,
                    "DATE" => Basic.DBF.DbfFieldType.Date,
                    "CURRENCY" => Basic.DBF.DbfFieldType.Currency,
                    _ => Basic.DBF.DbfFieldType.Character
                };
                fields.Add(new Basic.DBF.DbfField(name, ft, len, 0, 0));
            }
            Basic.DBF.DbfFile.CreateNew(path, System.Text.Encoding.UTF8, fields);
        }
        catch { }
    }
    public override void DbfSetFilter(long handle, string? filterExpr)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.SetFilter(filterExpr);
    }
    public override void DbfSetExact(long handle, bool on)
    {
        var engine = DbfDriverFor(handle)?.Engine;
        if (engine is null) return;
        try { engine.SetExact(on); } catch { }
    }
    public override void DbfAppendBlank(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Append(new Dictionary<int, object?>());
    }
    public override long DbfActiveOrder(long handle) =>
        DbfDriverFor(handle)?.Engine?.ActiveIndexOrder ?? 0;
    // `DBF SELECT n` (numero di area diretto, semantica Clipper): prima un no-op — le
    // funzioni RDD generiche RLOCK()/FLOCK() (RddRLock/RddFLock in IlRuntime.cs) leggono
    // session.CurrentArea, mai aggiornato da qui, quindi restavano ancorate all'ultima area
    // aperta (AllocateAreaFor) invece di seguire la SELECT (stesso bug/causa risolto per
    // ISAM in IsamSelect — vedi lì per i dettagli del crash che ha reso visibile il gap).
    public override void DbfSelectWorkArea(long workArea)
    {
        if (workArea >= 1 && workArea <= DataSession.MaxWorkareas) _session.CurrentArea = (int)workArea;
    }
    /// <summary>`DBF SELECT alias`: come DbfSelectWorkArea ma per handle (risolto da
    /// IlRuntime tramite _dbfHandles) invece di numero di area diretto.</summary>
    public override void DbfSelectHandle(long handle)
    {
        if (_handleToArea.TryGetValue(handle, out var area)) _session.CurrentArea = area;
    }
    public override void DbfCloseAll() { _session.CloseAll(); _handleToArea.Clear(); }

    /// <summary>Chiude UNA area: libera anche lo slot nella DataSession, così il successivo
    /// DBF USE riusa il numero d'area più basso (parità Clipper/altri backend:
    /// `DBF.SELECTED()` torna 1 dopo CLOSE+USE). Prima DisposeHandle non liberava l'area e
    /// il nuovo USE prendeva 2.</summary>
    public override void DbfClose(long handle)
    {
        if (_handleToArea.TryRemove(handle, out var area)) _session.Unregister(area);
        base.DbfClose(handle);
    }

    // ── DBF avanzate (motore Basic.DBF.DbfEngine via driver) ──

    private Basic.Databases.Rdd.DbfDriver? DbfDriverFor(long handle) =>
        DriverFor(handle) as Basic.Databases.Rdd.DbfDriver;

    public override bool DbfValidate(long handle)
    {
        var drv = DbfDriverFor(handle); return drv is null || drv.Validate();
    }
    private static readonly System.Collections.Generic.IReadOnlyDictionary<Basic.DBF.IIndexHandle, byte[]> _dbfEmptyKeys = new System.Collections.Generic.Dictionary<Basic.DBF.IIndexHandle, byte[]>();
    private static Basic.DBF.DbfFieldType DbfFieldTypeOf(Basic.DataType dt) => dt switch
    {
        Basic.StringType => Basic.DBF.DbfFieldType.Character,
        Basic.BitType => Basic.DBF.DbfFieldType.Logical,
        Basic.DateType or Basic.DateTimeType => Basic.DBF.DbfFieldType.Date,
        _ => Basic.DBF.DbfFieldType.Numeric,
    };
    public override void DbfList(long handle, string fieldsCsv, string destination, bool toPrint)
        => DbfListCond(handle, fieldsCsv, destination, toPrint, null);

    public override void DbfListCond(long handle, string fieldsCsv, string destination, bool toPrint,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? condition)
    {
        var drv = DbfDriverFor(handle); if (drv is null) return;
        try
        {
            using var w = (toPrint || string.IsNullOrEmpty(destination)) ? null : new System.IO.StreamWriter(destination);
            var fields = (fieldsCsv ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (fields.Length == 0) fields = drv.Fields.Select(f => f.Name.Trim()).ToArray(); // DBF LIST nudo = tutti i campi
            // Condizione FOR: valutata per record sull'INSIEME dei campi (non solo quelli stampati).
            var allNames = condition is null ? [] : drv.Fields.Select(f => f.Name.Trim()).ToArray();
            drv.GoTop();
            while (!drv.Eof)
            {
                if (condition is not null)
                {
                    var vals = new System.Collections.Generic.Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                    foreach (var fn in allNames) vals[fn] = drv.GetFieldValue(drv.GetFieldIndex(fn));
                    if (!condition(vals)) { drv.Skip(1); continue; }
                }
                var parts = fields.Select(f => Convert.ToString(drv.GetFieldValue(drv.GetFieldIndex(f)) ?? "", System.Globalization.CultureInfo.InvariantCulture)).ToArray();
                var line = string.Join(" ", parts);
                if (w != null) w.WriteLine(line); else System.Console.WriteLine(line);
                drv.Skip(1);
            }
            w?.Flush();
        }
        catch { }
    }
    public override void DbfReport(long handle, string formFile, string destination, bool toPrint)
    {
        var drv = DbfDriverFor(handle); if (drv is null) return;
        try
        {
            using var w = (toPrint || string.IsNullOrEmpty(destination)) ? null : new System.IO.StreamWriter(destination);
            var names = drv.Fields.Select(f => f.Name.Trim()).ToArray();
            drv.GoTop();
            while (!drv.Eof)
            {
                var parts = names.Select((n, i) => Convert.ToString(drv.GetFieldValue(i) ?? "", System.Globalization.CultureInfo.InvariantCulture)).ToArray();
                var line = string.Join(" ", parts);
                if (w != null) w.WriteLine(line); else System.Console.WriteLine(line);
                drv.Skip(1);
            }
            w?.Flush();
        }
        catch { }
    }
    public override void DbfAggregate(long handle, string keyField, string destPath, string aggregations)
    {
        var drv = DbfDriverFor(handle); if (drv is null) return;
        try
        {
            var defs = new System.Collections.Generic.List<(string Fn, string Field, string Alias)>();
            foreach (var raw in (aggregations ?? "").Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                var paren = raw.IndexOf('(');
                if (paren < 0) continue;
                var fn = raw[..paren].Trim().ToUpperInvariant();
                var inner = raw[(paren + 1)..];
                var close = inner.IndexOf(')');
                var field = (close >= 0 ? inner[..close] : inner).Trim();
                var alias = $"{fn}_{field}";
                if (close >= 0 && raw[(paren + 1 + close + 1)..].IndexOf("AS", StringComparison.OrdinalIgnoreCase) >= 0)
                    alias = raw[(paren + 1 + close + 1)..].Split("AS", StringSplitOptions.None)[^1].Trim();
                if (fn is "SUM" or "AVG" or "COUNT" or "MIN" or "MAX") defs.Add((fn, field, alias));
            }
            var keyIdx = drv.GetFieldIndex(keyField);
            if (keyIdx < 0) return;
            var kf = drv.Fields[keyIdx];
            var enc = drv is Basic.Databases.Rdd.DbfDriver d0 && d0.Engine is not null
                ? d0.Engine.GetEncoding() : System.Text.Encoding.ASCII;
            var fields = new System.Collections.Generic.List<Basic.DBF.DbfField> {
                new(kf.Name.Trim(), DbfFieldTypeOf(kf.DataType), (byte)System.Math.Clamp(kf.Length, 1, 254), 0, 0) };
            foreach (var d in defs) fields.Add(new Basic.DBF.DbfField(d.Alias, Basic.DBF.DbfFieldType.Numeric, 18, 2, 0));
            Basic.DBF.DbfFile.CreateNew(destPath, enc, fields);
            using var dest = new Basic.DBF.DbfEngine(destPath, null, false, Basic.Databases.LockingStrategy.IntraProcess);
            double[] sums = new double[defs.Count]; long[] counts = new long[defs.Count];
            double[] mins = new double[defs.Count]; double[] maxs = new double[defs.Count];
            string lastKey = ""; bool have = false;
            void FlushRow()
            {
                var row = new System.Collections.Generic.Dictionary<string, object?> { [kf.Name.Trim()] = lastKey.Trim() };
                for (int i = 0; i < defs.Count; i++)
                {
                    double v = defs[i].Fn switch {
                        "SUM" => sums[i], "AVG" => counts[i] > 0 ? sums[i] / counts[i] : 0,
                        "COUNT" => counts[i], "MIN" => mins[i], "MAX" => maxs[i], _ => 0 };
                    row[defs[i].Alias] = v;
                }
                dest.AppendRecord(row, _dbfEmptyKeys);
                System.Array.Clear(sums); System.Array.Clear(counts); System.Array.Clear(mins); System.Array.Clear(maxs);
            }
            drv.GoTop();
            while (!drv.Eof)
            {
                var cur = Convert.ToString(drv.GetFieldValue(keyIdx) ?? "")?.Trim() ?? "";
                if (have && cur != lastKey) FlushRow();
                lastKey = cur; have = true;
                for (int i = 0; i < defs.Count; i++)
                {
                    var idx = drv.GetFieldIndex(defs[i].Field); if (idx < 0) continue;
                    var v = Convert.ToDouble(drv.GetFieldValue(idx) ?? 0.0);
                    if (defs[i].Fn == "SUM") sums[i] += v;
                    else if (defs[i].Fn == "AVG") { sums[i] += v; counts[i]++; }
                    else if (defs[i].Fn == "COUNT") counts[i]++;
                    else if (defs[i].Fn == "MIN") { if (counts[i] == 0 || v < mins[i]) mins[i] = v; counts[i]++; }
                    else if (defs[i].Fn == "MAX") { if (counts[i] == 0 || v > maxs[i]) maxs[i] = v; counts[i]++; }
                }
                drv.Skip(1);
            }
            if (have) FlushRow();
        }
        catch { }
    }
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, long> _dbfViews = new();
    public override void DbfCreateView(string viewName)
    {
        if (!string.IsNullOrEmpty(viewName)) _dbfViews[viewName] = DateTime.UtcNow.Ticks;
    }
    public override void DbfDropView(string viewName)
    {
        if (!string.IsNullOrEmpty(viewName)) _dbfViews.TryRemove(viewName, out _);
    }
    public override void DbfSortTo(long handle, string destPath, string[] fields, bool[] descending)
    {
        var drv = DbfDriverFor(handle); if (drv is null) return;
        try { drv.SortTo(destPath, fields, descending); } catch { }
    }
    public override void DbfTotalOn(long handle, string destPath, Basic.Expr keyExpr, Basic.Expr[] sumFieldExprs,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return;
        try
        {
            var aggregates = new List<Basic.DBF.DbfTotalAggregate>();
            foreach (var fe in sumFieldExprs)
                if (Basic.DBF.DbfTotalExpressions.TryCreate(fe, out var agg)) aggregates.Add(agg);
            System.Func<Dictionary<string, object?>, bool>? cond = evaluator is null ? null : d => evaluator(d);
            engine.TotalOn(destPath, keyExpr, aggregates, null, cond);
        }
        catch { }
    }
    public override void DbfCopyTo(long handle, string destPath,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var drv = DbfDriverFor(handle); if (drv is null) return;
        try { drv.CopyTo(destPath, evaluator is null ? null : d => evaluator(d)); } catch { }
    }
    public override void DbfAppendFrom(long handle, string sourcePath,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var drv = DbfDriverFor(handle); if (drv is null) return;
        try { drv.AppendFrom(sourcePath, evaluator is null ? null : d => evaluator(d)); } catch { }
    }
    public override void DbfCopyStructure(long handle, string destPath)
    {
        var drv = DbfDriverFor(handle); if (drv is null) return;
        try { drv.CloneStructure(destPath); } catch { }
    }
    public override void DbfSetRelation(long handle, long parentHandle, string keyExpression)
    {
        // handle = figlio (chi riceve SetRelation), parentHandle = padre — Clipper / CoreDb.
        if (string.IsNullOrEmpty(keyExpression))
        {
            var parent = DriverFor(handle);
            if (parent is null) return;
            ClearOutgoingRelations(parent);
            return;
        }
        var child = DriverFor(handle);
        var parentDrv = DriverFor(parentHandle);
        if (child is null || parentDrv is null) return;
        try
        {
            child.SetRelation(parentDrv, keyExpression);
            child.SyncRelation();
        }
        catch { }
    }
    public override void DbfJoin(long handle, long secondaryHandle, string destPath,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        var src = DbfDriverFor(handle);
        var sec = DbfDriverFor(secondaryHandle);
        if (src is null || sec is null) return;
        try { Basic.Databases.Rdd.DbfDriver.NestedLoopJoin(src, sec, destPath, evaluator); } catch { }
    }
    public override void DbfUpdate(long handle, string keyField, string[] fieldNames,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        try
        {
            drv.GoTop();
            while (!drv.Eof)
            {
                var rec = drv.GetCurrentRecord();
                if (rec is not null)
                {
                    var ctx = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                    foreach (var kv in rec)
                        ctx[drv.Fields.FirstOrDefault(f => f.SymbolId == kv.Key)?.Name ?? kv.Key.ToString()] = kv.Value;
                    var updates = new Dictionary<int, object?>(rec);
                    for (var i = 0; i < fieldNames.Length && i < valueEvaluators.Length; i++)
                    {
                        var idx = drv.GetFieldIndex(fieldNames[i]);
                        if (idx >= 0) updates[idx] = valueEvaluators[i](ctx);
                    }
                    drv.PutCurrentRecord(updates);
                }
                drv.Skip(1);
            }
        }
        catch { }
    }
    public override void DbfUpdateOn(long destHandle, long srcHandle, string srcAlias, string keyField,
        string[] fieldNames,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>,
                    System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators)
    {
        var dest = DriverFor(destHandle);
        var src = DriverFor(srcHandle);
        if (dest is null || src is null) return;
        try { Basic.Databases.Rdd.DbfDriver.UpdateOn(dest, src, srcAlias, keyField, fieldNames, valueEvaluators); }
        catch { }
    }
    public override void DbfSetExclusive(long handle, bool on) { /* modalità esclusiva: lock del file quando aperto */ }
    public override void DbfSetSchema(long handle, bool on) { /* validazione schema strict in apertura */ }

    // ================================================================
    // SPEED
    // ================================================================
    public override long SpeedOpen(string path)
    {
        var drv = OpenDriver(path, "SPEED", "SPEED");
        var handle = NextHandle;
        AllocateAreaFor(handle, drv, "SPEED");
        return handle;
    }

    /// <summary>
    /// Prima `SPEED USE tableName FROM dbAlias OF Type ALIAS tableAlias INDEXED BY key`
    /// ignorava COMPLETAMENTE `tableName`/lo schema UDT/`INDEXED BY` — il compilatore chiamava
    /// solo `SpeedOpen(dbAlias, tableAlias)`, riaprendo un SECONDO `Database` sullo stesso path
    /// del database già aperto, sotto l'alias della tabella. `_tableId` del driver risultante
    /// restava 0 → `SpeedDriver.Open` sceglieva la prima tabella del manifest (o ne creava una
    /// "KV" di default), MAI la tabella richiesta, e senza schema `BaseDriver.GetFieldValue`
    /// si fermava dopo pochissimi campi — causa radice, trovata isolando un crash SIGSEGV in
    /// SPEED GET (vedi AiWiki Bugs/ILBackend-DbSpeedFullTests-Silent-Hang.md), di un bug
    /// distinto e più a monte di quel crash.
    ///
    /// Ora: risolve il driver del DATABASE già aperto tramite `dbHandle` (stesso `Database`
    /// condiviso, MAI un secondo file aperto), trova-o-crea la tabella per NOME nel suo
    /// manifest con lo schema dedotto dal tipo UDT, e lega un NUOVO `SpeedDriver` (via il
    /// costruttore `SpeedDriver(Database, alias)` + <see cref="SpeedDriver.UseTable"/>) a
    /// quella tabella specifica, registrato sotto un handle proprio per l'alias della tabella.
    /// </summary>
    public override long SpeedOpenSchema(string tableName, long dbHandle, string tableAlias, string schemaJson)
    {
        try
        {
            if (DriverFor(dbHandle) is not SpeedDriver dbDriver || dbDriver.Db is null) return -1;
            var db = dbDriver.Db;

            var schemaDoc = System.Text.Json.JsonDocument.Parse(schemaJson);
            var root = schemaDoc.RootElement;
            var fields = new List<Basic.Databases.Speed.Config.SpeedFieldSchema>();
            if (root.TryGetProperty("Fields", out var fieldsEl))
            {
                foreach (var f in fieldsEl.EnumerateArray())
                {
                    var fname = f.GetProperty("Name").GetString() ?? "F";
                    var ftype = f.GetProperty("Type").GetString() ?? "STRING";
                    var flen = f.TryGetProperty("Length", out var lenEl) ? lenEl.GetInt32() : 0;
                    fields.Add(new Basic.Databases.Speed.Config.SpeedFieldSchema(fname, ParseIsamDataType(ftype), flen));
                }
            }
            var pkName = root.TryGetProperty("PrimaryKey", out var pkEl) ? pkEl.GetString() : null;
            if (string.IsNullOrEmpty(pkName)) pkName = fields.Count > 0 ? fields[0].Name : "ID";
            var pkField = fields.FirstOrDefault(f => f.Name.Equals(pkName, StringComparison.OrdinalIgnoreCase));
            var pkLen = pkField != null && pkField.Length > 0 ? pkField.Length : 32;
            var pk = new Basic.Databases.Speed.Config.SpeedIndexDefinition(
                "PK", pkName, pkField?.DataType ?? new Basic.StringType(), pkLen, true, false);

            // Database.CreateTable è già trova-o-crea con validazione (vedi il suo doc comment):
            // ritorna i metadati esistenti se la tabella c'è già (convalidando lo schema), la
            // crea altrimenti — non serve un TryGetTable separato (Manifest è internal, non
            // accessibile da questo assembly comunque).
            var meta = db.CreateTable(new Basic.Databases.Speed.Config.SpeedSchema(tableName, fields, pk, []));

            var driver = new Basic.Databases.Rdd.SpeedDriver(db, tableAlias);
            driver.UseTable(meta.Id);

            var handle = NextHandle;
            AllocateAreaFor(handle, driver, tableAlias);
            return handle;
        }
        catch { return -1; }
    }
    public override void SpeedInsert(long handle, string key, object[] values)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        var fields = new Dictionary<int, object?>();
        for (var i = 0; i < values.Length; i++) fields[i] = values[i];
        drv.Append(fields);
    }
    /// <summary>`SPEED PUT alias, {Campo: valore, ...}`: riordina per NOME contro lo schema
    /// REALE del driver (drv.Fields, popolato da SpeedDriver.LoadSchema — SymbolId
    /// posizionale, coincide con l'indice) prima di Append. Senza, un dict-literal con
    /// l'ordine dei campi diverso dallo schema (o con l'unico consumer generico
    /// EmitObjectArrayArg che non lo gestiva affatto) produceva un record vuoto/troncato.</summary>
    public override void SpeedInsertByName(long handle, Dictionary<string, object> fields)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        var byIndex = new Dictionary<int, object?>();
        for (var i = 0; i < drv.Fields.Length; i++)
            if (fields.TryGetValue(drv.Fields[i].Name, out var v)) byIndex[drv.Fields[i].SymbolId] = v;
        drv.Append(byIndex);
    }
    public override object?[] SpeedGet(long handle, string key)
    {
        var drv = DriverFor(handle); if (drv is null) return Array.Empty<object?>();
        drv.Seek(new DbSeekInfo { Key = key });
        var result = new List<object?>();
        var idx = 0;
        while (drv.GetFieldValue(idx) is { } v) { result.Add(v); idx++; }
        return [.. result];
    }
    /// <summary>
    /// `SPEED GET alias, record` (nessuna chiave esplicita — legge il record CORRENTE, quello
    /// posizionato da GO/SEEK/SKIP, stessa semantica xBase/Clipper di GET dopo un SEEK separato).
    /// Prima il compilatore passava l'ALIAS della tabella come se fosse la CHIAVE da cercare
    /// (`EmitSpeedKeyArg(SpeedGetStmt)` emetteva `stmt.Alias.NameCleaned.Lexeme`, es. "Clienti"),
    /// quindi <see cref="SpeedGet"/> cercava sempre un record con chiave primaria letterale
    /// "Clienti" — mai trovato, `GetFieldValue(0)` tornava subito null, array vuoto. Questo
    /// metodo NON fa mai un Seek: legge solo lo stato corrente del cursore.
    /// </summary>
    public override object?[] SpeedGetCurrent(long handle)
    {
        var drv = DriverFor(handle); if (drv is null) return Array.Empty<object?>();
        var result = new List<object?>();
        var idx = 0;
        while (drv.GetFieldValue(idx) is { } v) { result.Add(v); idx++; }
        return [.. result];
    }
    public override void SpeedDelete(long handle, string key)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Seek(new DbSeekInfo { Key = key });
        drv.Delete();
    }
    public override void SpeedSeek(long handle, string key)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Seek(new DbSeekInfo { Key = key });
        SyncChildRelations(drv);
    }
    /// <summary>
    /// `SPEED GO alias, TOP|BOTTOM` (unico GO supportato da SPEED, nessun GOTO numerico — vedi
    /// Parser.Stmt.Speed.cs ParseSpeedGoStatement). Prima il compilatore IL non chiamava MAI
    /// questo metodo (`case SpeedGoStmt: EmitDatabaseNoop()`, un no-op incondizionato) — dopo
    /// SPEED PUT il cursore non veniva mai posizionato, quindi SPEED GET (che legge il record
    /// CORRENTE) trovava sempre nulla. -2 è la sentinella per BOTTOM (nessun secondo parametro
    /// disponibile nell'interfaccia esistente, per non toccare la firma condivisa con altri
    /// chiamanti): position&lt;=0 e diverso da -2 resta TOP, invariato.
    /// </summary>
    public override void SpeedGo(long handle, long position)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        if (position == -2) drv.GoBottom();
        else if (position <= 0) drv.GoTop();
        else drv.GoTo(position);
        SyncChildRelations(drv);
    }
    public override void SpeedSkip(long handle, int count)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.Skip(count);
        SyncChildRelations(drv);
    }
    public override bool SpeedEof(long handle)
    {
        var drv = DriverFor(handle); return drv is null || drv.Eof;
    }
    public override void SpeedSetOrder(long handle, string indexName)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.SetOrder(indexName);
    }
    public override void SpeedSetFilter(long handle, string? filterExpr)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        drv.SetFilter(filterExpr);
    }
    /// <summary>`handle` = figlio, `parentHandle` = padre — stesso contratto di
    /// <see cref="DbfSetRelation"/>. `keyExpression` vuota → rimuove la relazione uscente da
    /// `handle` (usato anche come padre in quel caso, vedi IlRuntime.SpeedSetRelation).</summary>
    public override void SpeedSetRelation(long handle, long parentHandle, string keyExpression)
    {
        if (string.IsNullOrEmpty(keyExpression))
        {
            var parent = DriverFor(handle);
            if (parent is null) return;
            ClearOutgoingRelations(parent);
            return;
        }
        var child = DriverFor(handle);
        var parentDrv = DriverFor(parentHandle);
        if (child is null || parentDrv is null) return;
        try
        {
            child.SetRelation(parentDrv, keyExpression);
            child.SyncRelation();
        }
        catch { }
    }
    /// <summary>`SPEED CREATE INDEX indexName ON alias KEY (keyExpression)`: il driver
    /// (SpeedDriver.CreateIndex) delega già a Database.CreateSecondaryIndex — mancava solo
    /// questo collegamento da IDatabaseService.</summary>
    public override void SpeedCreateIndex(long handle, string indexName, string keyExpression)
    {
        var drv = DriverFor(handle); if (drv is null) return;
        try { drv.CreateIndex(new DbOrderInfo { Name = indexName, KeyExpression = keyExpression, KeyLength = 32 }); } catch { }
    }
    public override void SpeedDropIndex(long handle, string indexName)
    {
        if (DriverFor(handle) is not SpeedDriver drv) return;
        try { drv.DropIndex(indexName); } catch { }
    }
    public override Basic.Databases.Speed.Database? SpeedGetDatabaseFor(long handle) =>
        (DriverFor(handle) as SpeedDriver)?.Db;
    public override string? SpeedGetTableNameFor(long handle)
    {
        if (DriverFor(handle) is not SpeedDriver drv || drv.Db is null) return null;
        return drv.Db.Manifest.TryGetTable(drv.TableId, out var meta) ? meta?.Name : null;
    }
    public override bool SpeedCreateViewTable(long dbHandle, string viewName, string schemaJson)
    {
        try
        {
            var db = SpeedGetDatabaseFor(dbHandle);
            if (db is null) return false;
            var schemaDoc = System.Text.Json.JsonDocument.Parse(schemaJson);
            var root = schemaDoc.RootElement;
            var fields = new List<Basic.Databases.Speed.Config.SpeedFieldSchema>();
            if (root.TryGetProperty("Fields", out var fieldsEl))
                foreach (var f in fieldsEl.EnumerateArray())
                {
                    var fname = f.GetProperty("Name").GetString() ?? "F";
                    var ftype = f.GetProperty("Type").GetString() ?? "STRING";
                    var flen = f.TryGetProperty("Length", out var lenEl) ? lenEl.GetInt32() : 0;
                    fields.Add(new Basic.Databases.Speed.Config.SpeedFieldSchema(fname, ParseIsamDataType(ftype), flen));
                }
            var pkName = root.TryGetProperty("PrimaryKey", out var pkEl) ? pkEl.GetString() : null;
            if (string.IsNullOrEmpty(pkName)) pkName = fields.Count > 0 ? fields[0].Name : "ID";
            var pkField = fields.FirstOrDefault(f => f.Name.Equals(pkName, StringComparison.OrdinalIgnoreCase));
            var pkLen = pkField != null && pkField.Length > 0 ? pkField.Length : 32;
            var pk = new Basic.Databases.Speed.Config.SpeedIndexDefinition(
                "PK", pkName, pkField?.DataType ?? new Basic.StringType(), pkLen, true, false);
            db.CreateTable(new Basic.Databases.Speed.Config.SpeedSchema(viewName, fields, pk, []));
            return true;
        }
        catch { return false; }
    }

    // ================================================================
    // Ledger
    // ================================================================
    private readonly ConcurrentDictionary<long, Basic.Ledger.LedgerContext> _ledgerContexts = new();
    private long _ledgerTxId = 1;

    public override long LedgerOpen(string path)
    {
        // PERSISTENZA: all'apertura va CARICATO l'indice (.idx) o, se assente, RICOSTRUITO dal
        // journal (.dat). Senza, l'indice restava vuoto dopo la riapertura → (1) BALANCE/REGISTER/
        // GET vuoti e (2) il nuovo ID transazione ripartiva da max(0)+1 = 1 (bug: l'ID non è
        // stabile). Parità PCode (PCodeProcessor.Syscalls LEDGER_OPEN) — il protocollo LEDGER
        // (guida §1/§7) prevede che la riapertura ricarichi i dati.
        var fullPath = System.IO.Path.GetFullPath(path);
        var ctx = new Basic.Ledger.LedgerContext(fullPath);
        try
        {
            if (System.IO.File.Exists(ctx.IndexFilePath))
                Basic.Ledger.LedgerManager.ReadIndexFromFile(ctx.IndexFilePath, ctx.TransactionIndex);
            else if (System.IO.File.Exists(fullPath))
            {
                Basic.Ledger.LedgerManager.RebuildIndexFromFile(fullPath, ctx.TransactionIndex);
                Basic.Ledger.LedgerManager.SaveIndexToFile(ctx.IndexFilePath, ctx.TransactionIndex);
            }
        }
        catch { }
        var handle = NextHandle;
        _ledgerContexts[handle] = ctx;
        return handle;
    }
    public override void LedgerClose(long handle)
    {
        if (_ledgerContexts.TryRemove(handle, out var ctx))
        {
            Basic.Ledger.LedgerManager.SaveIndexToFile(ctx.IndexFilePath, ctx.TransactionIndex);
        }
        _ledgerCursorIndex.Remove(handle);
    }
    public override long LedgerAppend(long handle, string category, object[] values)
    {
        if (!_ledgerContexts.TryGetValue(handle, out var ctx)) return 0;
        // values = [dateYMD, desc, accountsCsv, amountsCsv].
        var dateStr = values.Length > 0 ? values[0]?.ToString() ?? "" : "";
        var desc = values.Length > 1 ? values[1]?.ToString() ?? "" : "";
        var accountsCsv = values.Length > 2 ? values[2]?.ToString() ?? "" : "";
        var amountsCsv = values.Length > 3 ? values[3]?.ToString() ?? "" : "";
        var accounts = accountsCsv.Split(';', StringSplitOptions.RemoveEmptyEntries);
        var amounts = amountsCsv.Split(';', StringSplitOptions.RemoveEmptyEntries);
        /* La data BASIC arriva come SERIALE numerico (DATEVALUE/"44927"): TryParse falliva →
         * TUTTE le transazioni prendevano la data di OGGI (divergenza § DbLedgerFullTests: il
         * campo Date dei report era sempre oggi). Converte il seriale (OADate) come il resto
         * del runtime (IlRuntime.ToDateTime → DateTime.FromOADate). */
        DateOnly date;
        {
            double oa;
            var v0 = values.Length > 0 ? values[0] : null;
            if (v0 is DateTime dt0) date = DateOnly.FromDateTime(dt0);
            else if (v0 is double d0) date = DateOnly.FromDateTime(DateTime.FromOADate(d0));
            else if (!double.TryParse(dateStr, System.Globalization.NumberStyles.Float,
                                      System.Globalization.CultureInfo.InvariantCulture, out oa) || oa <= 0)
            { if (!DateOnly.TryParse(dateStr, out date)) date = DateOnly.FromDateTime(DateTime.Today); }
            else date = DateOnly.FromDateTime(DateTime.FromOADate(oa));
        }
        var entries = new List<Basic.Ledger.LedgerEntry>();
        var txId = _ledgerTxId++;
        for (var i = 0; i < accounts.Length; i++)
        {
            var amount = i < amounts.Length && decimal.TryParse(amounts[i], System.Globalization.CultureInfo.InvariantCulture, out var a)
                ? a : 0m;
            entries.Add(new Basic.Ledger.LedgerEntry
            {
                TransactionId = txId,
                Date = date,
                Status = ' ',
                Description = desc,
                Account = accounts[i],
                Amount = amount,
                AmountIsExplicit = true,
            });
        }
        return Basic.Ledger.LedgerManager.AddTransaction(ctx, date, desc, entries);
    }
    public override long LedgerGetCount(long handle)
        => _ledgerContexts.TryGetValue(handle, out var ctx) ? ctx.TransactionIndex.Count : 0;

    // ── Cursore LEDGER (pattern DBF/ISAM) ──
    // Il cursore itera le transazioni in ordine ascendente di ID (l'ordine in cui sono
    // state aggiunte). _ledgerCursorIndex è l'indice 0-based nella lista ordinata;
    // -1 = prima della prima (BOF), == count = dopo l'ultima (EOF).
    private readonly Dictionary<long, int> _ledgerCursorIndex = new();

    private static long[] LedgerSortedIds(Basic.Ledger.LedgerContext ctx) =>
        ctx.TransactionIndex.Keys.OrderBy(k => k).ToArray();

    public override void LedgerGoTop(long handle)
    {
        if (!TryLedgerContext(handle, out var ctx)) return;
        var ids = LedgerSortedIds(ctx);
        _ledgerCursorIndex[handle] = ids.Length > 0 ? 0 : -1;
    }
    public override void LedgerGoBottom(long handle)
    {
        if (!TryLedgerContext(handle, out var ctx)) return;
        var ids = LedgerSortedIds(ctx);
        _ledgerCursorIndex[handle] = ids.Length > 0 ? ids.Length - 1 : -1;
    }
    public override void LedgerGo(long handle, long recno)
    {
        // GO <transactionId>: posiziona sulla transazione con quell'ID (FOUND riflette l'esito).
        if (!TryLedgerContext(handle, out var ctx)) return;
        var ids = LedgerSortedIds(ctx);
        var idx = Array.IndexOf(ids, recno);
        if (idx >= 0) _ledgerCursorIndex[handle] = idx;
        else if (!_ledgerCursorIndex.ContainsKey(handle)) _ledgerCursorIndex[handle] = -1;
    }
    public override void LedgerSkip(long handle, int count)
    {
        if (!TryLedgerContext(handle, out var ctx)) return;
        var ids = LedgerSortedIds(ctx);
        var cur = _ledgerCursorIndex.TryGetValue(handle, out var c) ? c : -1;
        _ledgerCursorIndex[handle] = Math.Clamp(cur + count, -1, ids.Length);
    }
    public override bool LedgerBof(long handle) => LedgerCursorValidIndex(handle) is < 0;
    public override bool LedgerEof(long handle)
    {
        if (!TryLedgerContext(handle, out var ctx)) return true;
        var idx = _ledgerCursorIndex.TryGetValue(handle, out var c) ? c : -1;
        return idx < 0 || idx >= ctx.TransactionIndex.Count;
    }
    public override long LedgerRecno(long handle)
    {
        if (!TryLedgerContext(handle, out var ctx)) return 0;
        var idx = _ledgerCursorIndex.TryGetValue(handle, out var c) ? c : -1;
        var ids = LedgerSortedIds(ctx);
        return idx >= 0 && idx < ids.Length ? ids[idx] : 0;
    }
    public override bool LedgerFound(long handle) => LedgerCursorValidIndex(handle) >= 0;

    private int LedgerCursorValidIndex(long handle)
        => _ledgerCursorIndex.TryGetValue(handle, out var c) ? c : -1;

    public override object?[] LedgerRead(long handle, long recno)
    {
        // GET senza ID (cursore): legge la transazione corrente. GET con ID passa da LedgerGet.
        if (!TryLedgerContext(handle, out var ctx)) return [];
        var id = recno > 0 ? recno : LedgerRecno(handle);
        if (id <= 0) return [];
        var entries = Basic.Ledger.LedgerManager.GetTransaction(ctx, id);
        return [.. entries.Select(e => (object?)LedgerEntryJson(e))];
    }

    // ── Ledger — operazioni reali sul motore (Basic.Ledger.LedgerManager) ──

    private bool TryLedgerContext(long handle, out Basic.Ledger.LedgerContext ctx)
    {
        ctx = null!;
        return _ledgerContexts.TryGetValue(handle, out ctx!);
    }
    private static string ToJson(object value) => System.Text.Json.JsonSerializer.Serialize(value);

    public override bool LedgerValidate(long handle)
    {
        if (!TryLedgerContext(handle, out var ctx)) return false;
        try
        {
            // Tutte le transazioni attive devono essere bilanciate.
            foreach (var group in Basic.Ledger.LedgerManager.GetFilteredEntries(ctx, "").GroupBy(e => e.TransactionId))
            {
                if (!Basic.Ledger.LedgerManager.IsTransactionBalanced([.. group])) return false;
            }
            return true;
        }
        catch { return false; }
    }
    public override void LedgerDelete(long handle, long id)
    {
        if (TryLedgerContext(handle, out var ctx))
            Basic.Ledger.LedgerManager.DeleteTransaction(ctx, id);
    }
    public override void LedgerPack(long handle)
    {
        if (TryLedgerContext(handle, out var ctx))
            Basic.Ledger.LedgerManager.PackJournal(ctx);
    }
    public override void LedgerSave(long handle)
    {
        // SAVE: persiste indice e budget (era EmitDatabaseNoop — non salvava nulla).
        if (!TryLedgerContext(handle, out var ctx)) return;
        try
        {
            Basic.Ledger.LedgerManager.SaveIndexToFile(ctx.IndexFilePath, ctx.TransactionIndex);
            if (ctx.BudgetsDirty) Basic.Ledger.LedgerManager.SaveBudgets(ctx.BudgetFilePath, ctx);
            ctx.IsDirty = false;
        }
        catch { }
    }
    public override long LedgerUpdate(long handle, long id, object[] values)
    {
        // EDIT: sostituisce la transazione preservando la semantica del motore
        // (UpdateTransaction = delete + re-add bilanciato). Prima era delete+append
        // con una nuova transazione che perdeva id e categoria. Parità PCodeVM LEDGER_EDIT.
        if (!TryLedgerContext(handle, out var ctx)) return 0;
        try
        {
            var dateStr = values.Length > 0 ? values[0]?.ToString() ?? "" : "";
            var desc = values.Length > 1 ? values[1]?.ToString() ?? "" : "";
            var accountsCsv = values.Length > 2 ? values[2]?.ToString() ?? "" : "";
            var amountsCsv = values.Length > 3 ? values[3]?.ToString() ?? "" : "";
            var accounts = accountsCsv.Split(';', StringSplitOptions.RemoveEmptyEntries);
            var amounts = amountsCsv.Split(';', StringSplitOptions.RemoveEmptyEntries);
            /* Stessa conversione dell'ADD: la data BASIC arriva come DateTime (o seriale). */
            DateOnly date;
            {
                double oa; var v0 = values.Length > 0 ? values[0] : null;
                if (v0 is DateTime dt0) date = DateOnly.FromDateTime(dt0);
                else if (v0 is double d0) date = DateOnly.FromDateTime(DateTime.FromOADate(d0));
                else if (!double.TryParse(dateStr, System.Globalization.NumberStyles.Float,
                                          System.Globalization.CultureInfo.InvariantCulture, out oa) || oa <= 0)
                { if (!DateOnly.TryParse(dateStr, out date)) date = DateOnly.FromDateTime(DateTime.Today); }
                else date = DateOnly.FromDateTime(DateTime.FromOADate(oa));
            }
            var entries = new List<Basic.Ledger.LedgerEntry>();
            for (var i = 0; i < accounts.Length; i++)
            {
                var amount = i < amounts.Length && decimal.TryParse(amounts[i], System.Globalization.CultureInfo.InvariantCulture, out var a)
                    ? a : 0m;
                entries.Add(new Basic.Ledger.LedgerEntry
                {
                    TransactionId = 0,
                    Date = date,
                    Status = ' ',
                    Description = desc,
                    Account = accounts[i],
                    Amount = amount,
                    AmountIsExplicit = true,
                });
            }
            var sum = entries.Sum(e => e.Amount);
            if (sum != 0)
                entries.Add(new Basic.Ledger.LedgerEntry
                {
                    TransactionId = 0,
                    Date = date,
                    Status = ' ',
                    Description = desc,
                    Account = "Equity:Balancing",
                    Amount = -sum,
                    AmountIsExplicit = true,
                });
            return Basic.Ledger.LedgerManager.UpdateTransaction(ctx, id, date, desc, entries);
        }
        catch { return 0; }
    }
    private static object LedgerEntryJson(Basic.Ledger.LedgerEntry e) => new Dictionary<string, object?>()
    {
        ["TransactionId"] = e.TransactionId,
        ["Date"] = e.Date.ToString("yyyy-MM-dd"),
        ["Status"] = e.Status.ToString(),
        ["Description"] = e.Description,
        ["Account"] = e.Account,
        ["Amount"] = (double)e.Amount,
        ["Comment"] = e.Comment,
        ["AmountIsExplicit"] = e.AmountIsExplicit
    };
    public override object?[] LedgerRegister(long handle, string query)
    {
        if (!TryLedgerContext(handle, out var ctx)) return [];
        var entries = Basic.Ledger.LedgerManager.GetFilteredEntries(ctx, query);
        return [.. entries.Select(e => (object?)LedgerEntryJson(e))];
    }
    public override object?[] LedgerBalance(long handle, string query)
    {
        if (!TryLedgerContext(handle, out var ctx)) return [];
        var (labels, amounts) = Basic.Ledger.LedgerManager.GetBalancePlotData(ctx, query);
        return [.. labels.Zip(amounts, (l, a) => (object?)new Dictionary<string, object?> { ["Account"] = l, ["TotalAmount"] = (double)a })];
    }
    public override object?[] LedgerGet(long handle, long id)
    {
        if (!TryLedgerContext(handle, out var ctx)) return [];
        var entries = Basic.Ledger.LedgerManager.GetTransaction(ctx, id);
        return [.. entries.Select(e => (object?)LedgerEntryJson(e))];
    }
    public override string[] LedgerAccounts(long handle, string? root)
    {
        if (!TryLedgerContext(handle, out var ctx)) return [];
        return Basic.Ledger.LedgerManager.GetAccounts(ctx, root);
    }
    public override void LedgerBudget(long handle, string account, decimal amount, string period)
    {
        if (!TryLedgerContext(handle, out var ctx)) return;
        try { Basic.Ledger.LedgerManager.SetBudget(ctx, account, amount, period); } catch { }
    }
    public override object? LedgerVariance(long handle, string account, string period)
    {
        if (!TryLedgerContext(handle, out var ctx)) return 0.0;
        try
        {
            var variance = Basic.Ledger.LedgerManager.GetVariance(ctx, account, period);
            return (double)variance.Variance;
        }
        catch { return 0.0; }
    }
    public override object? LedgerPlot(long handle, string query, string chartType)
    {
        if (!TryLedgerContext(handle, out var ctx)) return null;
        try
        {
            var (labels, amounts) = Basic.Ledger.LedgerManager.GetBalancePlotData(ctx, query);
            return new Dictionary<string, object?>
            {
                ["Labels"] = labels,
                ["Amounts"] = amounts.Select(a => (object)(double)a).ToList()
            };
        }
        catch { return null; }
    }
    public override double LedgerImport(long handle, string csvPath, string mappingRules)
    {
        if (!TryLedgerContext(handle, out var ctx)) return 0;
        try { return Basic.Ledger.LedgerManager.ImportCsv(ctx, csvPath, mappingRules); } catch { return 0; }
    }
    // ── Configurazione (durabilità / replica) ──

    public override void DbfSetDurability(long handle, string mode)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return;
        try { engine.DurabilityMode = Enum.TryParse<Basic.DBF.DbfDurabilityMode>(mode, true, out var m) ? m : Basic.DBF.DbfDurabilityMode.Full; } catch { }
    }
    public override void DbfFlushDurability(long handle)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return;
        try { engine.FlushDurability(); } catch { }
    }
    public override void IsamSetReplication(long handle, string logPath)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return;
        try { engine.SetReplication(logPath); } catch { }
    }
    public override void SpeedSetDurability(long handle, string mode)
    {
        var drv = DriverFor(handle) as Basic.Databases.Rdd.SpeedDriver; var db = drv?.Db;
        if (db is null) return;
        try { db.DurabilityMode = Enum.TryParse<Basic.SPEED.SpeedDurabilityMode>(mode, true, out var m) ? m : Basic.SPEED.SpeedDurabilityMode.Full; } catch { }
    }
    public override void SpeedFlushDurability(long handle)
    {
        var drv = DriverFor(handle) as Basic.Databases.Rdd.SpeedDriver; var db = drv?.Db;
        if (db is null) return;
        try { db.FlushDurability(); } catch { }
    }
    public override void SpeedBeginGroupCommit(long handle)
    {
        var drv = DriverFor(handle) as Basic.Databases.Rdd.SpeedDriver; var db = drv?.Db;
        if (db is null) return;
        try { db.BeginGroupCommit(); } catch { }
    }
    public override void SpeedEndGroupCommit(long handle)
    {
        var drv = DriverFor(handle) as Basic.Databases.Rdd.SpeedDriver; var db = drv?.Db;
        if (db is null) return;
        try { db.EndGroupCommit(); } catch { }
    }
    public override string DbfSetReplication(long handle, string logPath)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return "";
        try { return engine.SetReplication(logPath) ?? ""; } catch { return ""; }
    }
    public override void DbfBeginGroupCommit(long handle)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return;
        try { engine.BeginGroupCommit(); } catch { }
    }
    public override long DbfAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records)
    {
        var drv = DbfDriverFor(handle);
        var engine = drv?.Engine; if (engine is null) return 0;
        try
        {
            engine.AppendManyRecords(records);
            // Il driver tiene una COPIA di RecCount/RecNo/Eof/Bof, riallineata solo da
            // SyncCursorState(): l'append in blocco scrive nel motore ma nessuno la rinfresca,
            // quindi LASTREC() restava al valore di apertura (0) invece del nuovo totale
            // (§ MiniTests/dbf/dbf_replication.bas: PCode 2, IL 0).
            drv!.SyncCursorState();
            return records.Count;
        }
        catch { return 0; }
    }
    public override int DbfFollowReplication(long handle, string replLogPath)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return 0;
        try { return engine.FollowReplication(replLogPath); } catch { return 0; }
    }
    public override int DbfApplyReplication(long handle, string replLogPath)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return 0;
        try { return engine.ApplyReplication(replLogPath); } catch { return 0; }
    }
    public override void DbfEndGroupCommit(long handle)
    {
        var engine = DbfDriverFor(handle)?.Engine; if (engine is null) return;
        try { engine.EndGroupCommit(); } catch { }
    }    public override long IsamAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records)
    {
        var drv = DriverFor(handle) as Basic.Databases.Rdd.IsamDriver;
        var engine = drv?.Engine; var udt = drv?.UdtDef;
        if (engine is null || udt is null) return 0;
        try
        {
            var list = new List<UdtInstance>();
            foreach (var rec in records)
            {
                var fields = new Dictionary<int, object?>();
                foreach (var f in udt.Fields)
                    if (rec.TryGetValue(f.Name.Lexeme, out var v)) fields[f.SymbolId] = v;
                list.Add(new UdtInstance(udt, fields));
            }
            engine.AppendMany(list, engine.Evaluator);
            return records.Count;
        }
        catch { return 0; }
    }
    public override int IsamApplyReplication(long handle, string replLogPath)
    {
        var engine = IsamEngineFor(handle); if (engine is null) return 0;
        try { return engine.ApplyReplication(replLogPath); } catch { return 0; }
    }    public override long SpeedSetMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records)
    {
        var drv = DriverFor(handle) as Basic.Databases.Rdd.SpeedDriver; var db = drv?.Db;
        if (db is null) return 0;
        try
        {
            var entries = new List<KeyValuePair<byte[], byte[]>>();
            foreach (var rec in records)
            {
                var keyVal = rec.Count > 0 ? rec.Values.First()?.ToString() ?? "" : "";
                var json = System.Text.Json.JsonSerializer.Serialize(rec);
                entries.Add(new KeyValuePair<byte[], byte[]>(System.Text.Encoding.UTF8.GetBytes(keyVal), System.Text.Encoding.UTF8.GetBytes(json)));
            }
            db.SetMany(drv!.TableId, entries);
            return records.Count;
        }
        catch { return 0; }
    }
}
