// FILE: Basic.Databases.Rdd.DbfDriver.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Basic.DBF;

namespace Basic.Databases.Rdd;

[DatabaseDriver("DBF", Extensions = new[] { ".dbf" }, Description = "dBASE / FoxPro DBF database")]
public class DbfDriver : BaseDriver
{
    internal DbfEngine? _engine;
    private static readonly IReadOnlyDictionary<IIndexHandle, byte[]> _noKeys =
        new Dictionary<IIndexHandle, byte[]>();

    /// <summary>Accesso al motore DBF per le operazioni avanzate (SortTo/TotalOn/CopyTo/
    /// CloneStructure/ValidateIntegrity).</summary>
    public DbfEngine? Engine => _engine;

    private Basic.DBF.DbfSchemaInfo BuildSchema() =>
        new("R", [.. _fields.Select(f => new Basic.DBF.DbfFieldSchema(f.Name, f.DataType, f.Length))]);

    public void SortTo(string destination, string[] fields, bool[] descending)
    {
        if (_engine == null) return;
        var sortFields = fields.Select((f, i) => (f, i < descending.Length && descending[i])).ToList();
        _engine.SortTo(destination, sortFields, BuildSchema());
    }
    public void CopyTo(string destination, System.Func<System.Collections.Generic.Dictionary<string, object?>, bool>? evaluator)
    {
        if (_engine == null) return;
        _engine.CopyTo(destination, evaluator, BuildSchema());
    }
    public void AppendFrom(string sourcePath, System.Func<System.Collections.Generic.Dictionary<string, object?>, bool>? evaluator)
    {
        if (_engine == null) return;
        _engine.AppendFrom(sourcePath, evaluator);
        SyncCursorState();   // aggiorna _recCount (LASTREC) dopo l'append
    }
    public void CloneStructure(string destination)
    {
        if (_engine == null) return;
        _engine.CloneStructure(destination, BuildSchema());
    }
    public bool Validate()
    {
        if (_engine == null) return true;
        try { return _engine.ValidateIntegrity().IsValid; } catch { return false; }
    }

    public override string DriverName => "DBF";

    public DbfDriver() { }
    public DbfDriver(DbfEngine engine, string alias)
    {
        _engine = engine;
        _alias = alias;
        _fileName = engine.FilePath;
        _isOpen = true;
        EnsureEvaluator();
        SyncCursorState();
    }

    public override void Open(DbOpenInfo info)
    {
        _engine = new DbfEngine(info.FileName, null, false, LockingStrategy.IntraProcess);
        _alias = info.Alias;
        _fileName = info.FileName;
        _isOpen = true;
        EnsureEvaluator();
        SyncCursorState();
    }

    public override void Close() { _engine?.Dispose(); _engine = null; _isOpen = false; }

    // ═══ Navigazione ═══

    public override void GoTop()
    {
        _engine?.GoTop();
        SyncCursorState();
    }

    public override void GoBottom()
    {
        _engine?.GoBottom();
        SyncCursorState();
    }

    public override void GoTo(long n)
    {
        if (_engine == null) return;
        // GO n = posizionamento sul record n (NON una ricerca per chiave: Seek(object) cercava n
        // nell'ordine attivo e, senza indice, finiva a EOF). Stessa semantica del P-Code.
        _engine.GoTop();
        if (n > 1) _engine.Skip((int)(n - 1));
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
        if (_engine == null) return 0;
        _engine.AppendRecord(ToNamedFields(fields), _noKeys);
        SyncCursorState();
        return _recNo;
    }

    public override void Delete()
    {
        _engine?.DeleteCurrent(_noKeys);
        SyncCursorState();
    }

    public override void Recall()
    {
        _engine?.RecallCurrent(_noKeys);
        SyncCursorState();
    }

    public override void Pack() { _engine?.Pack(); SyncCursorState(); }
    public override void Zap() { _engine?.Zap(); SyncCursorState(); }

    public override Dictionary<int, object?>? GetCurrentRecord()
    {
        var dict = _engine?.GetCurrentRecordValues();
        if (dict == null) return null;
        if (_fields.Length == 0) PopulateFields();
        var result = new Dictionary<int, object?>();
        foreach (var f in _fields)
        {
            if (dict.TryGetValue(f.Name, out var v))
                result[f.SymbolId] = v;
        }
        return result;
    }

    public override void PutCurrentRecord(Dictionary<int, object?> fields)
    {
        if (_engine == null) return;
        _engine.PutCurrentRecord(ToNamedFields(fields), _noKeys, _noKeys);
        SyncCursorState();
    }

    /// <summary>DBF UPDATE ON &lt;key&gt; FROM &lt;alias&gt; REPLACE …: per ogni record della
    /// DESTINAZIONE cerca nel SORGENTE il record con lo stesso valore del campo &lt;key&gt; e
    /// applica le sostituzioni. I callback ricevono (contestoDestinazione, contestoSorgente):
    /// i nomi NUDI sono campi della destinazione, `&lt;alias&gt;.&lt;campo&gt;` del sorgente (la
    /// risoluzione dei riferimenti qualificati avviene nel chiamante, che ha l'evaluator).</summary>
    public static void UpdateOn(IDatabaseDriver dest, IDatabaseDriver src, string srcAlias,
        string keyField, string[] fieldNames,
        Func<IReadOnlyDictionary<string, object?>, IReadOnlyDictionary<string, object?>, object?>[] evaluators)
    {
        if (dest is null || src is null || string.IsNullOrEmpty(keyField)) return;
        int kd = dest.GetFieldIndex(keyField), ks = src.GetFieldIndex(keyField);
        if (kd < 0 || ks < 0) return;
        dest.GoTop();
        while (!dest.Eof)
        {
            var drec = dest.GetCurrentRecord();
            if (drec is not null && drec.TryGetValue(kd, out var dkv))
            {
                string dkey = KeyText(dkv);
                var srec = FindByKey(src, ks, dkey);
                if (srec is not null)
                {
                    var dctx = Ctx(dest, drec);
                    var sctx = Ctx(src, srec);
                    var updates = new Dictionary<int, object?>(drec);
                    for (int i = 0; i < fieldNames.Length && i < evaluators.Length; i++)
                    {
                        int idx = dest.GetFieldIndex(fieldNames[i]);
                        if (idx >= 0) updates[idx] = evaluators[i](dctx, sctx);
                    }
                    dest.PutCurrentRecord(updates);
                }
            }
            dest.Skip(1);
        }
    }

    /// <summary>DBF UPDATE (senza FROM): applica le sostituzioni a TUTTI i record della
    /// destinazione, valutandole sul record stesso (i nomi nudi sono campi della destinazione).</summary>
    public static void UpdateAll(IDatabaseDriver drv, string[] fieldNames,
        Func<IReadOnlyDictionary<string, object?>, object?>[] evaluators)
    {
        if (drv is null) return;
        drv.GoTop();
        while (!drv.Eof)
        {
            var rec = drv.GetCurrentRecord();
            if (rec is not null)
            {
                var ctx = Ctx(drv, rec);
                var updates = new Dictionary<int, object?>(rec);
                for (int i = 0; i < fieldNames.Length && i < evaluators.Length; i++)
                {
                    int idx = drv.GetFieldIndex(fieldNames[i]);
                    if (idx >= 0) updates[idx] = evaluators[i](ctx);
                }
                drv.PutCurrentRecord(updates);
            }
            drv.Skip(1);
        }
    }

    /// <summary>Sostituisce i riferimenti `<alias>.<campo>` con il VALORE del record sorgente
    /// (letterale) così l'espressione residua si valuta sul solo contesto della destinazione.</summary>
    public static string SubstituteQualifiedRefs(string expr, string alias,
        IReadOnlyDictionary<string, object?> src)
    {
        foreach (var kv in src)
        {
            var token = alias + "." + kv.Key;
            var lit = kv.Value switch
            {
                null => "NIL",
                bool b => b ? "TRUE" : "FALSE",
                string s => "\"" + s.Replace("\"", "\"\"") + "\"",
                IFormattable f => f.ToString(null, System.Globalization.CultureInfo.InvariantCulture) ?? "0",
                _ => "\"\""
            };
            expr = System.Text.RegularExpressions.Regex.Replace(
                expr, "(?i)" + System.Text.RegularExpressions.Regex.Escape(token) + @"\b", _ => lit);
        }
        return expr;
    }

    private static string KeyText(object? v) => v switch
    {
        null => "",
        string s => s.TrimEnd(),
        IFormattable f => f.ToString(null, System.Globalization.CultureInfo.InvariantCulture) ?? "",
        _ => v.ToString()?.TrimEnd() ?? ""
    };

    private static Dictionary<int, object?>? FindByKey(IDatabaseDriver drv, int keyIdx, string key)
    {
        drv.GoTop();
        while (!drv.Eof)
        {
            var rec = drv.GetCurrentRecord();
            if (rec is not null && rec.TryGetValue(keyIdx, out var v)
                && string.Equals(KeyText(v), key, StringComparison.OrdinalIgnoreCase))
                return rec;
            drv.Skip(1);
        }
        return null;
    }

    /// <summary>Contesto campo→valore per l'evaluator (chiavi = NOMI campo).</summary>
    private static Dictionary<string, object?> Ctx(IDatabaseDriver drv, Dictionary<int, object?> rec)
    {
        var d = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in rec)
            d[drv.Fields.FirstOrDefault(f => f.SymbolId == kv.Key)?.Name ?? kv.Key.ToString()] = kv.Value;
        return d;
    }

    /// <summary>
    /// Riposiziona questo driver (figlio) con SEEK sull'indice attivo, usando il valore
    /// corrente del campo chiave nel padre — stesso contratto di Clipper / CoreDb C.
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
            _engine.Found = false;
            SyncCursorState();
            return false;
        }
        if (parentValue == null)
        {
            _engine.Found = false;
            SyncCursorState();
            return false;
        }
        return Seek(new DbSeekInfo { Key = parentValue });
    }

    /// <summary>
    /// JOIN WITH Clipper: crea <paramref name="destPath"/> con l'unione dei campi
    /// (collisione → suffisso "2") e appende le coppie che passano FOR.
    /// </summary>
    public static void NestedLoopJoin(
        DbfDriver src,
        DbfDriver sec,
        string destPath,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator)
    {
        ArgumentNullException.ThrowIfNull(src);
        ArgumentNullException.ThrowIfNull(sec);
        ArgumentNullException.ThrowIfNull(destPath);
        if (src._engine == null || sec._engine == null) return;

        src.PopulateFields();
        sec.PopulateFields();
        var srcFields = src._engine.GetFields();
        var secFields = sec._engine.GetFields();
        var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var merged = new List<DbfField>(srcFields.Count + secFields.Count);
        foreach (var f in srcFields)
            merged.Add(new DbfField(UniqueDbfName(f.Name, used), f.Type, f.Length, f.DecimalCount, 0));
        var srcCount = merged.Count;
        foreach (var f in secFields)
            merged.Add(new DbfField(UniqueDbfName(f.Name, used), f.Type, f.Length, f.DecimalCount, 0));

        var destDir = Path.GetDirectoryName(Path.GetFullPath(destPath));
        if (!string.IsNullOrEmpty(destDir) && !Directory.Exists(destDir))
            Directory.CreateDirectory(destDir);
        if (File.Exists(destPath)) File.Delete(destPath);

        DbfFile.CreateNew(destPath, src._engine.GetEncoding(), merged);
        using var dest = new DbfEngine(destPath, null, exclusive: true, LockingStrategy.IntraProcess);
        src.GoTop();
        while (!src.Eof)
        {
            sec.GoTop();
            while (!sec.Eof)
            {
                var ctx = MergeFieldContext(src, sec);
                if (evaluator is null || evaluator(ctx))
                {
                    var row = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                    var srcVals = src._engine.GetCurrentRecordValues();
                    var secVals = sec._engine.GetCurrentRecordValues();
                    for (var i = 0; i < srcCount; i++)
                    {
                        var srcName = srcFields[i].Name.Trim();
                        row[merged[i].Name] = srcVals != null && srcVals.TryGetValue(srcName, out var sv) ? sv : null;
                    }
                    for (var i = 0; i < secFields.Count; i++)
                    {
                        var secName = secFields[i].Name.Trim();
                        row[merged[srcCount + i].Name] = secVals != null && secVals.TryGetValue(secName, out var tv) ? tv : null;
                    }
                    dest.AppendRecord(row, _noKeys);
                }
                sec.Skip(1);
            }
            src.Skip(1);
        }
    }

    // ═══ Indici ═══

    public override void CreateIndex(DbOrderInfo info)
    {
        if (_engine == null || string.IsNullOrEmpty(info.Name)) return;
        EnsureEvaluator();
        var keyLen = info.KeyLength > 0 ? info.KeyLength : 50;
        var keyName = System.Text.RegularExpressions.Regex.Replace(info.KeyExpression ?? "", @"#-?\d+", "");
        var enc = _engine.GetEncoding();
        // Espressione chiave: `INDEX ON Nome` (campo singolo) oppure `INDEX ON Citta + Nome`
        // (concatenazione). Prima si cercava un campo col nome ESATTO dell'espressione: non
        // trovato → si usava il PRIMO campo, quindi l'indice a espressione ordinava per ID
        // (il sample ClipperDbfLateBinding §5 risultava in ordine naturale).
        var keyTerms = keyName.Split('+').Select(t => t.Trim()).Where(t => t.Length > 0).ToArray();
        // Termine della chiave: campo nudo oppure funzione comune sulla chiave (STR(campo)).
        static object? CliTerm(System.Collections.Generic.Dictionary<string, object?> row, string term)
        {
            if (row.TryGetValue(term, out var v)) return v;
            var m = System.Text.RegularExpressions.Regex.Match(term,
                @"^(?:[A-Za-z_][\w.]*\.)?(STR|UPPER|LOWER|TRIM)\s*\(\s*([^()]+?)\s*\)$",
                System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            if (m.Success && row.TryGetValue(m.Groups[2].Value.Trim(), out var fv))
            {
                var fn = m.Groups[1].Value.ToUpperInvariant();
                var txt = fn == "STR"
                    ? ((long)Math.Round(Convert.ToDouble(fv, System.Globalization.CultureInfo.InvariantCulture))).ToString(System.Globalization.CultureInfo.InvariantCulture).PadLeft(10)
                    : Convert.ToString(fv, System.Globalization.CultureInfo.InvariantCulture) ?? "";
                if (fn == "UPPER") return txt.ToUpperInvariant();
                if (fn == "LOWER") return txt.ToLowerInvariant();
                if (fn == "TRIM") return txt.Trim();
                return txt;
            }
            return null;
        }
        System.Func<System.Collections.Generic.Dictionary<string, object?>, byte[]?> kp = row =>
        {
            if (row == null) return null;
            object? val;
            if (keyTerms.Length > 1 && keyTerms.All(t => CliTerm(row, t) is not null))
                val = string.Concat(keyTerms.Select(t =>
                    Convert.ToString(CliTerm(row, t), System.Globalization.CultureInfo.InvariantCulture) ?? ""));
            else if (keyTerms.Length == 1 && CliTerm(row, keyTerms[0]) is { } kv)
                val = kv;
            else if (!string.IsNullOrEmpty(keyName) && row.TryGetValue(keyName, out var v))
                val = v;
            else
                val = row.Values.FirstOrDefault();
            return _engine.NormalizeSingleKeyBytes(val, keyLen, enc);
        };
        var token = new Token(TokenKind.Identifier, keyName, null, new SourcePosition(0, 0, 0));
        var expr = new VariableExpr(-1, token, token);
        _engine.IndexOn(info.Name, expr, keyLen, info.Unique, kp);
    }
    public override void OpenIndex(string name, string filePath)
    {
        var handle = _engine?.OpenIndexFile(filePath);
        if (handle != null && !string.IsNullOrEmpty(name))
            _engine?.SetActiveOrder(name);
    }
    public override void CloseIndex(string name) { _engine?.CloseIndexFile(name); }
    public override void SetOrder(string name)
    {
        if (_engine != null && !string.IsNullOrEmpty(name))
            _engine.SetActiveOrder(name);
    }

    public override bool Seek(DbSeekInfo info)
    {
        if (_engine == null || info.Key == null) return false;
        _engine.Seek(info.Key);
        SyncCursorState();
        return _found;
    }

    public override string? CurrentKey() => _engine?.ActiveIndexKeyExpression;
    public override int CurrentKeyNo() => _engine?.ActiveIndexOrder ?? 0;

    // ═══ Lock ═══

    public override bool Lock(long rec) => _engine?.TryLockCurrentRecord() ?? false;
    public override void Unlock(long rec) { _engine?.UnlockRecord(rec); }
    public override bool FileLock() => _engine?.TryLockFile() ?? false;
    public override void FileUnlock() { _engine?.UnlockFile(); }

    // ═══ Filtri (delegati al P-CODE VM via engine.SetFilter) ═══

    /// <summary>Testo del filtro attivo (vuoto se nessuno): è il valore che DBF.DBFILTER()
    /// deve restituire — l'engine espone solo l'oggetto espressione, non il sorgente.</summary>
    public string FilterText => _filterExpression;

    public override void SetFilter(string? expression)
    {
        _filterExpression = expression ?? "";
        base.SetFilter(expression);
    }

    public override void SetDeleted(bool on) { _deleted = on; _engine?.SetDeleted(on); }

    // ═══ Info ═══

    public override object? Info(int infoType, object? param) => infoType switch
    {
        1 => _recNo,          // DBI_RECNO
        2 => _recCount,       // DBI_RECCOUNT
        3 => _fields.Length,  // DBI_FCOUNT
        4 => _alias,          // DBI_ALIAS
        5 => _fileName,       // DBI_FILENAME
        6 => _isOpen,         // DBI_ISOPEN
        _ => null
    };

    // ═══ Helpers ═══

    /// <summary>Aggiorna Eof/Bof/Found/RecNo/RecCount dal motore. Pubblico perché il P-Code
    /// naviga <see cref="DbfEngine"/> direttamente (DBF_SKIP/GO/SEEK), bypassando Skip/GoTop.</summary>
    public void SyncCursorState()
    {
        if (_engine == null) return;
        _eof = _engine.Eof;
        _bof = _engine.Bof;
        _found = _engine.Found;
        _recNo = _engine.GetCurrentRecordNumber();
        _recCount = _engine.GetTotalRecordCount();
        if (_fields.Length == 0) PopulateFields();
    }

    private void EnsureEvaluator()
    {
        if (_engine == null || _engine.Evaluator != null) return;
        var engine = _engine;
        engine.Evaluator = (expr, ctx, _) =>
        {
            if (expr == "__recno__") return (double)engine.GetCurrentRecordNumber();
            if (ctx != null && ctx.TryGetValue(expr, out var val)) return val;
            return expr;
        };
    }

    private void PopulateFields()
    {
        if (_engine == null) return;
        var src = _engine.GetFields();
        var list = new RddFieldInfo[src.Count];
        var offset = 0;
        for (var i = 0; i < src.Count; i++)
        {
            var f = src[i];
            list[i] = new RddFieldInfo(f.Name.Trim(), MapDbfType(f), f.Length, offset, i);
            offset += f.Length;
        }
        _fields = list;
    }

    private Dictionary<string, object?> ToNamedFields(Dictionary<int, object?> fields)
    {
        if (_fields.Length == 0) PopulateFields();
        var named = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in fields)
        {
            var name = NameForSymbol(kv.Key);
            if (name != null) named[name] = kv.Value;
        }
        return named;
    }

    private string? NameForSymbol(int symbolId)
    {
        for (var i = 0; i < _fields.Length; i++)
            if (_fields[i].SymbolId == symbolId) return _fields[i].Name;
        return symbolId >= 0 && symbolId < _fields.Length ? _fields[symbolId].Name : null;
    }

    private static DataType MapDbfType(DbfField f) => f.Type switch
    {
        DbfFieldType.Character => new StringType(f.Length),
        DbfFieldType.Numeric => new DoubleType(),
        DbfFieldType.Float => new SingleType(),
        DbfFieldType.Integer => new LongType(),
        DbfFieldType.Logical => new BitType(),
        DbfFieldType.Date => new DateType(),
        DbfFieldType.DateTime => new DateTimeType(),
        DbfFieldType.Double => new DoubleType(),
        DbfFieldType.Currency => new CurrencyType(),
        _ => new StringType(f.Length)
    };

    private static Dictionary<string, object?> MergeFieldContext(DbfDriver src, DbfDriver sec)
    {
        var ctx = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        var srcVals = src._engine?.GetCurrentRecordValues();
        var secVals = sec._engine?.GetCurrentRecordValues();
        if (srcVals != null)
            foreach (var kv in srcVals) ctx[kv.Key] = kv.Value;
        if (secVals != null)
            foreach (var kv in secVals)
                if (!ctx.ContainsKey(kv.Key)) ctx[kv.Key] = kv.Value;
        // Chiavi alias-qualificate per le condizioni di JOIN con campi omonimi
        // (es. O.IDCLI = C.ID con ID in entrambe le tabelle): il bare resta src-wins
        // per retrocompatibilità, il qualificato disambigua sempre.
        AddQualifiedKeys(ctx, src._alias, srcVals);
        AddQualifiedKeys(ctx, sec._alias, secVals);
        // Sotto-dict per alias: la condizione JIT-compilata accede ad Alias.Campo come
        // FieldAccess su globale Alias + OP_LOAD_FIELD (che legge IDictionary) — senza
        // queste voci il globale Alias vale null e ogni confronto qualificato è falso.
        AddAliasDict(ctx, src._alias, srcVals);
        AddAliasDict(ctx, sec._alias, secVals);
        return ctx;
    }

    private static void AddQualifiedKeys(Dictionary<string, object?> ctx, string? alias,
        Dictionary<string, object?>? vals)
    {
        if (string.IsNullOrWhiteSpace(alias) || vals is null) return;
        foreach (var kv in vals) ctx[$"{alias}.{kv.Key}"] = kv.Value;
    }

    private static void AddAliasDict(Dictionary<string, object?> ctx, string? alias,
        Dictionary<string, object?>? vals)
    {
        if (string.IsNullOrWhiteSpace(alias) || vals is null) return;
        ctx[alias] = new Dictionary<string, object?>(vals, StringComparer.OrdinalIgnoreCase);
    }

    private static string UniqueDbfName(string raw, HashSet<string> used)
    {
        var name = raw.Trim();
        if (name.Length > 10) name = name[..10];
        if (used.Add(name)) return name;
        var stem = name.Length > 9 ? name[..9] : name;
        for (var n = 2; n < 100; n++)
        {
            var candidate = stem + n;
            if (candidate.Length > 10) candidate = candidate[..10];
            if (used.Add(candidate)) return candidate;
        }
        return name;
    }
}
