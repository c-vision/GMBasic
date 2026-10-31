// FILE: Basic.Databases.Rdd.IsamDriver.cs
using System.Collections.Generic;
using System.Linq;
using Basic.ISAM;

namespace Basic.Databases.Rdd;

[DatabaseDriver("ISAM", Extensions = new[] { ".isdb" }, Description = "ISAM B+Tree database")]
public class IsamDriver : BaseDriver
{
    internal Engine? _engine;
    internal UserDefinedType? _udt;
    private SchemaBuffer? _schemaBuffer;

    /// <summary>Accesso al motore ISAM per le operazioni avanzate (CopyTo/SortTo/Join/
    /// Repair/Backup/Validate/CreateIndex/DropIndex/SetFilter).</summary>
    public Engine? Engine => _engine;

    /// <summary>Definizione del tipo UDT associato all'area (per APPENDMANY/Bulk).</summary>
    public UserDefinedType? UdtDef => _udt;


    public override string DriverName => "ISAM";

    /// <summary>Buffer a offset fissi per accesso diretto ai campi (3-5x più veloce).</summary>
    public SchemaBuffer? SchemaBuf
    {
        get
        {
            if (_schemaBuffer == null && _udt != null)
                _schemaBuffer = SchemaBuffer.FromUdt(_udt);
            return _schemaBuffer;
        }
    }

    public IsamDriver() { }
    public IsamDriver(Engine engine, UserDefinedType udt, string alias)
    {
        _engine = engine;
        _udt = udt;
        _alias = alias;
        _fileName = engine.FilePath;
        _isOpen = true;
        if (_udt != null) _schemaBuffer = SchemaBuffer.FromUdt(_udt);
        // BaseDriver.Fields/GetFieldIndex/GetFieldValue (usati da IsamDriver.SyncRelation per
        // risolvere il campo chiave sul driver padre/figlio) leggono _fields — mai popolato
        // prima d'ora, quindi restava sempre vuoto e GetFieldIndex/ResolveRelationChildField
        // fallivano sempre (SET RELATION silenziosamente inefficace anche a driver corretto).
        _fields = _udt != null
            ? [.. _udt.Fields.Select(tf => new RddFieldInfo(tf.Name.Lexeme, tf.DataType, 0, 0, tf.SymbolId))]
            : [];
        SyncCursorState();
    }

    public override void Open(DbOpenInfo info)
    {
        // Legge lo schema DAL FILE (blocco ISCH embedded) — NIENTE più no-op:
        // RDD USE "file.isdb" (auto-detect ISAM) ora funziona, senza schema dal codice.
        if (!File.Exists(info.FileName))
            throw new System.IO.FileNotFoundException($"File ISAM non trovato: {info.FileName}");
        var snapshot = Basic.Tools.Isam.IsamOfflineToolkit.ReadSnapshot(info.FileName);
        var udt = Basic.Tools.Isam.IsamOfflineToolkit.BuildMinimalUdt(snapshot.Schema);
        var engine = Basic.Tools.Isam.IsamOfflineToolkit.OpenEngine(info.FileName, null);
        // Evaluator chiavi: pipeline Compiler -> P-Code -> PCodeVm (stessa del path
        // schema-rich in CommercialDatabaseService.IsamOpenSchema), con fallback al
        // lookup diretto per nome campo.
        engine.Evaluator = (expr, ctx, tt) =>
        {
            if (expr == "__recno__") return (double)engine.GetCurrentRecordNumber();
            // Lookup diretto per nome campo PRIMA della compilazione P-Code: un identificatore
            // nudo ("Saldo") compilato produce il load di una variabile non risolta = 0 (valore
            // NON nullo), che intercettava il fallback → le chiavi dell'indice secondario erano
            // tutte 0 → l'indice restituiva l'ordine di inserimento invece del proprio ordine
            // (CREATE INDEX su un campo diverso dalla PK). Stesso ordine del PCode.
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
        _engine = engine;
        _udt = udt;
        _alias = info.Alias;
        _fileName = info.FileName;
        _isOpen = true;
        _fields = udt != null
            ? [.. udt.Fields.Select(tf => new RddFieldInfo(tf.Name.Lexeme, tf.DataType, 0, 0, tf.SymbolId))]
            : [];
        if (udt != null) _schemaBuffer = SchemaBuffer.FromUdt(udt);
        SyncCursorState();
    }
    public override void Close() { _engine?.Dispose(); _engine = null; _isOpen = false; }

    // ═══ Navigazione ═══

    public override void GoTop()
    {
        _engine?.Go(Config.CursorMoveOperation.Top);
        SyncCursorState();
    }

    public override void GoBottom()
    {
        _engine?.Go(Config.CursorMoveOperation.Bottom);
        SyncCursorState();
    }

    public override void GoTo(long n)
    {
        // ISAM non ha un indirizzamento per recno: posiziona sul TOP e scorre n-1 record.
        if (_engine == null || n <= 1) { _engine?.Go(Config.CursorMoveOperation.Top); SyncCursorState(); return; }
        _engine.Go(Config.CursorMoveOperation.Top);
        for (long i = 1; i < n; i++)
        {
            _engine.Skip(1);
            if (_engine.IsEof()) break;
        }
        SyncCursorState();
    }

    public override void Skip(int n)
    {
        _engine?.Skip(n);
        SyncCursorState();
    }

    public override void SkipRaw(int n)
    {
        _engine?.Skip(n);
        SyncCursorState();
    }

    // ═══ CRUD ═══

    public override long Append(Dictionary<int, object?> fields)
    {
        if (_engine == null || _udt == null) return 0;
        _engine.Append(new UdtInstance(_udt, new Dictionary<int, object?>(fields)), _engine.Evaluator);
        SyncCursorState();
        return _recNo;
    }

    public override void Delete()
    {
        _engine?.DeleteCurrent(_engine?.Evaluator);
        SyncCursorState();
    }

    public override void Recall()
    {
        _engine?.RecallCurrent(_engine?.Evaluator);
        SyncCursorState();
    }

    public override void Pack()
    {
        _engine?.Pack(_engine?.Evaluator);
        SyncCursorState();
    }

    public override void Zap()
    {
        // Cancella tutti i record: scorre da TOP eliminando finché non raggiunge EOF.
        if (_engine == null) return;
        _engine.Go(Config.CursorMoveOperation.Top);
        while (!_engine.IsEof())
        {
            _engine.DeleteCurrent(_engine.Evaluator);
            _engine.Skip(1);
        }
        SyncCursorState();
    }

    public override Dictionary<int, object?>? GetCurrentRecord()
    {
        var fields = _engine?.GetCurrentRecord()?.Fields;
        if (fields != null && SchemaBuf != null)
            SchemaBuf.FromDictionary(fields);
        return fields;
    }
    public override void PutCurrentRecord(Dictionary<int, object?> fields)
    {
        if (_engine == null || _udt == null) return;
        _engine.Put(new UdtInstance(_udt, new Dictionary<int, object?>(fields)), _engine.Evaluator);
        SyncCursorState();
    }

    // ═══ Indici ═══

    public override void CreateIndex(DbOrderInfo info)
    {
        if (_engine == null || string.IsNullOrEmpty(info.Name)) return;
        _engine.CreateIndexAsync(info.Name, info.KeyExpression, info.KeyLength > 0 ? info.KeyLength : 20,
                info.Unique, _engine.Evaluator)
            .GetAwaiter().GetResult();
        SyncCursorState();
    }
    public override void OpenIndex(string name, string filePath)
    {
        // ISAM: gli indici secondari sono EMBEDDED nello schema del file (nessun file indice
        // esterno da aprire) — attiva l'ordine per nome se esiste tra gli indici dello schema.
        if (_engine != null && !string.IsNullOrEmpty(name))
            _engine.SetActiveOrder(name);
    }
    public override void CloseIndex(string name)
    {
        if (_engine == null || string.IsNullOrEmpty(name)) return;
        try { _engine.DropIndexAsync(name).GetAwaiter().GetResult(); } catch { }
        SyncCursorState();
    }
    public override void SetOrder(string name)
    {
        // Commuta l'indice ATTIVO dell'engine (primary o secondary per nome). Prima era un
        // no-op → l'ordine restava primario e ogni Seek usava la PK, rendendo impossibili
        // SET ORDER e SEEK su un indice secondario (parità IL/VM-C/C99).
        _engine?.SetActiveOrder(name ?? "");
    }

    public override bool Seek(DbSeekInfo info)
    {
        if (_engine == null || info.Key == null) return false;
        // La chiave va SERIALIZZATA come nell'insert (padding a KeyLength), altrimenti
        // "T2" (6 byte) non coincide con "T2        " (10 byte) nell'indice → NOT FOUND.
        // L'indice attivo può essere SECONDARIO (SET ORDER): usa la sua definizione, non la PK.
        var activeName = _engine.ActiveIndexName();
        var def = _engine.Schema.PrimaryKey;
        if (!string.IsNullOrEmpty(activeName))
        {
            foreach (var sk in _engine.Schema.SecondaryKeys)
                if (string.Equals(sk.Name, activeName, StringComparison.OrdinalIgnoreCase)) { def = sk; break; }
        }
        var key = Basic.ISAM.SchemaManager.SerializeKeyValue(info.Key, def);
        _engine.Seek(key);
        SyncCursorState();
        return _found;
    }

    public override string? CurrentKey() => null;
    public override int CurrentKeyNo() => (int)(_engine?.GetCurrentRecordNumber() ?? 0);

    // ═══ Relazioni master-detail (SET RELATION) ═══

    /// <summary>
    /// Riposiziona questo driver (figlio) sul primo record il cui campo di relazione corrisponde
    /// al valore corrente del campo chiave nel driver padre.
    ///
    /// Nota architetturale: SET RELATION ON &lt;parent&gt; TO &lt;keyExpr&gt; INTO &lt;child&gt; (sintassi
    /// xBase classica) non specifica il campo del FIGLIO da confrontare — si affida all'indice
    /// attivo del figlio per interpretare il valore cercato. In questo motore ISAM, SetOrder e'
    /// un no-op (l'unico indice utilizzabile per Seek e' sempre la chiave primaria) e non esiste
    /// un meccanismo di indice secondario attivabile: non c'e' quindi modo di sapere, dalla sola
    /// sintassi, quale campo del figlio confrontare. Risolto per euristica: si prova prima il nome
    /// del campo chiave stesso (caso in cui padre e figlio condividono il nome), poi la convenzione
    /// "ID_" + alias del padre (es. parent "Clienti" -> "ID_Clienti", o "ID_Cliente" con la comune
    /// singolarizzazione italiana "i" finale -> "e"). Se nessun campo del figlio corrisponde, la
    /// relazione non trova nulla (comportamento sicuro, non un crash).
    ///
    /// Il posizionamento e' uno scan lineare dall'inizio (non un vero Seek su indice, per la
    /// ragione sopra) — accettabile per la dimensione tipica delle relazioni master-detail nei
    /// test, non pensato per grandi tabelle figlio.
    /// </summary>
    public override bool SyncRelation()
    {
        if (_parentRelation == null || string.IsNullOrEmpty(_relationKeyExpression) || _engine == null)
            return _found;

        object? parentValue;
        var parentFieldIndex = _parentRelation.GetFieldIndex(_relationKeyExpression);
        if (parentFieldIndex >= 0)
        {
            parentValue = _parentRelation.GetFieldValue(parentFieldIndex);
        }
        else if (Basic.Databases.KeyExpressionPCodeHook.CompileFactory?.Invoke(_relationKeyExpression) is { } compiled)
        {
            // _relationKeyExpression non è un semplice nome di campo (es. `TO STR(ID)+Suffisso`)
            // — valutata tramite la pipeline P-Code unificata (stesso hook già usato per le
            // chiavi SEEK/INDEX ON) contro il record CORRENTE del padre, invece di fallire
            // silenziosamente come prima.
            parentValue = compiled(BuildFieldContext(_parentRelation));
        }
        else
        {
            _engine.SetFoundFlag(false);
            SyncCursorState();
            return false;
        }
        if (parentValue == null)
        {
            _engine.SetFoundFlag(false);
            SyncCursorState();
            return false;
        }

        var childFieldName = ResolveRelationChildField(_parentRelation.Alias);
        if (childFieldName == null)
        {
            _engine.SetFoundFlag(false);
            SyncCursorState();
            return false;
        }

        var buf = SchemaBuf;
        _engine.Go(Config.CursorMoveOperation.Top);
        while (!_engine.IsEof())
        {
            var rec = _engine.GetCurrentRecord();
            if (rec != null && buf != null)
            {
                buf.FromDictionary(rec.Fields);
                if (RelationValuesEqual(buf.ReadValue(childFieldName), parentValue))
                {
                    _engine.SetFoundFlag(true);
                    SyncCursorState();
                    return true;
                }
            }
            _engine.Skip(1);
        }
        _engine.SetFoundFlag(false);
        SyncCursorState();
        return false;
    }

    private string? ResolveRelationChildField(string? parentAlias)
    {
        if (_fields.Length == 0 || string.IsNullOrEmpty(_relationKeyExpression)) return null;
        var candidates = new List<string> { _relationKeyExpression! };
        if (!string.IsNullOrEmpty(parentAlias))
        {
            candidates.Add("ID_" + parentAlias);
            if (parentAlias!.EndsWith("i", System.StringComparison.OrdinalIgnoreCase) && parentAlias.Length > 1)
                candidates.Add("ID_" + parentAlias[..^1] + "e"); // euristica plurale italiano: Clienti -> Cliente
            if (parentAlias.EndsWith("s", System.StringComparison.OrdinalIgnoreCase) && parentAlias.Length > 1)
                candidates.Add("ID_" + parentAlias[..^1]); // euristica plurale inglese
        }
        foreach (var candidate in candidates)
            foreach (var f in _fields)
                if (string.Equals(f.Name, candidate, System.StringComparison.OrdinalIgnoreCase))
                    return f.Name;
        return null;
    }

    private static bool RelationValuesEqual(object? a, object? b)
    {
        if (a == null || b == null) return false;
        if (a is double || b is double || a is long || b is long || a is int || b is int)
        {
            try { return System.Convert.ToDouble(a) == System.Convert.ToDouble(b); }
            catch { /* fallthrough to string compare */ }
        }
        return string.Equals(a.ToString(), b.ToString(), System.StringComparison.OrdinalIgnoreCase);
    }

    // ═══ Lock ═══

    public override bool Lock(long rec) { _engine?.LockCurrentRecord(); return true; }
    // UNLOCK (record): semantica Clipper reale — rilascia OGNI lock record tenuto da
    // quest'area, non solo quello sotto il cursore ORA (che potrebbe essersi spostato da
    // quando RLOCK fu acquisito — vedi Engine.ReleaseAllRecordLocks per i dettagli del bug
    // che questo fix risolve: un RLOCK+SKIP+UNLOCK lasciava il lock originale orfano per
    // sempre, causando SynchronizationLockException al Dispose).
    public override void Unlock(long rec) { _engine?.ReleaseAllRecordLocks(); }
    public override bool FileLock() => _engine?.TryLockFile() ?? false;
    public override void FileUnlock() { if (_engine != null) _engine.UnlockFile(); }

    // ═══ Filtri (delegati al P-CODE VM via engine.SetFilter) ═══

    public override void SetFilter(string? expression)
    {
        _filterExpression = expression ?? "";
        base.SetFilter(expression);
    }

    // ═══ Info ═══

    public override object? Info(int infoType, object? param) => infoType switch
    {
        1 => _recNo,
        2 => _recCount,
        3 => _udt != null ? _udt.Fields.Length : 0,
        4 => _alias,
        5 => _fileName,
        6 => _isOpen,
        _ => null
    };

    // ═══ Helpers ═══

    /// <summary>Aggiorna Eof/Bof/Found/RecNo/RecCount leggendo lo stato corrente di _engine.
    /// Pubblico perché il P-Code VM naviga l'Engine direttamente (ISAM_RECORD/ISAM_SKIP/
    /// ISAM_SEEK bypassano GoTop/Skip/Seek di questo driver per performance — vedi
    /// PCodeProcessor.Syscalls.cs), quindi deve richiamare questo metodo esplicitamente dopo
    /// ogni navigazione diretta o Eof/Bof/Found/RecNo restano fermi allo stato dell'apertura.</summary>
    public void SyncCursorState()
    {
        if (_engine == null) return;
        _eof = _engine.IsEof();
        _bof = _engine.IsBof();
        _found = _engine.WasFound();
        _recNo = _engine.GetCurrentRecordNumber();
        _recCount = _engine.GetTotalRecordCount();
    }
}
