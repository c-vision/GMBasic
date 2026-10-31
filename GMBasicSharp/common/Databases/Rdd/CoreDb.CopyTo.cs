// FILE: Basic.Databases.Rdd.CoreDb.CopyTo.cs
// COPY TO: esporta record da un'area verso un altro driver.
// Universale cross-driver: ISAM→DBF, DBF→CSV, SPEED→ISAM, ecc.

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

public static partial class CoreDb
{
    /// <summary>
    /// COPY TO: esporta record dall'area source verso un file con un driver.
    /// </summary>
    /// <param name="session">DataSession.</param>
    /// <param name="sourceArea">Area origine.</param>
    /// <param name="targetFile">File destinazione.</param>
    /// <param name="targetDriver">Driver destinazione (es. "DBF", "CSV", "ISAM").</param>
    /// <param name="fields">Campi da esportare (null = tutti).</param>
    /// <param name="forCondition">Filtro FOR opzionale.</param>
    /// <param name="whileCondition">Condizione WHILE opzionale.</param>
    /// <returns>Numero record copiati.</returns>
    public static long CopyTo(
        DataSession session,
        int sourceArea,
        string targetFile,
        string targetDriver,
        string[]? fields = null,
        string? forCondition = null,
        string? whileCondition = null)
    {
        var sourceWa = session.GetWorkArea(sourceArea);
        var source = sourceWa?.Driver;
        if (source == null || !source.IsOpen) return -1;

        // Crea driver destinazione
        var target = DriverRegistry.Create(targetDriver);
        if (target == null) return -1;

        try
        {
            // COPY TO crea/ricrea il target: un file esistente non va letto+appeso.
            if (System.IO.File.Exists(targetFile)) System.IO.File.Delete(targetFile);
            target.Open(new DbOpenInfo { FileName = targetFile, Alias = "_copyto", Driver = targetDriver });
            // Il target testo nuovo non ha header/campi: propaga lo schema del source.
            if (source.Fields.Length > 0) target.SetFields(source.Fields);
            if (!target.IsOpen) return -1;

            var eval = forCondition != null
                ? RuntimeExpressionEvaluator.CreateEvaluator(forCondition) : null;
            var whileEval = whileCondition != null
                ? RuntimeExpressionEvaluator.CreateEvaluator(whileCondition) : null;

            long copied = 0;
            source.GoTop();

            while (!source.Eof)
            {
                var record = source.GetCurrentRecord();
                if (record != null)
                {
                    // Filtra campi
                    Dictionary<int, object?> outRecord;
                    if (fields != null && fields.Length > 0)
                    {
                        outRecord = new Dictionary<int, object?>();
                        foreach (var f in fields)
                        {
                            var idx = source.GetFieldIndex(f);
                            if (idx >= 0 && record.TryGetValue(idx, out var val))
                                outRecord[idx] = val;
                        }
                    }
                    else
                    {
                        outRecord = record;
                    }

                    // FOR condition
                    bool include = true;
                    if (eval != null)
                    {
                        var ctx = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                        foreach (var kv in record)
                        {
                            var fi = Array.Find(source.Fields, f => f.SymbolId == kv.Key);
                            if (fi != null) ctx[fi.Name] = kv.Value;
                        }
                        include = eval(null, ctx);
                    }

                    if (include)
                    {
                        target.Append(outRecord);
                        copied++;
                    }
                }

                // WHILE condition
                if (whileEval != null)
                {
                    var ctx = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                    if (record != null)
                        foreach (var kv in record)
                        {
                            var fi = Array.Find(source.Fields, f => f.SymbolId == kv.Key);
                            if (fi != null) ctx[fi.Name] = kv.Value;
                        }
                    if (!whileEval(null, ctx)) break;
                }

                source.Skip(1);
            }

            return copied;
        }
        finally
        {
            target.Close();
        }
    }

    /// <summary>
    /// IMPORT FROM: importa record da un file (driver sorgente, es. "CSV") nell'area di
    /// destinazione già aperta — percorso INVERSO di <see cref="CopyTo"/> (file → area,
    /// invece di area → file). Non crea/tronca la destinazione: fa APPEND dei record letti,
    /// stessa convenzione IMPORT FROM di Clipper/xBase.
    /// </summary>
    /// <returns>Numero di record importati, o -1 se l'area destinazione o il file sorgente
    /// non sono validi.</returns>
    public static long CopyFrom(DataSession session, int targetArea, string sourceFile, string sourceDriver)
    {
        var target = session.GetWorkArea(targetArea)?.Driver;
        if (target == null || !target.IsOpen) return -1;
        if (!System.IO.File.Exists(sourceFile)) return -1;

        var source = DriverRegistry.Create(sourceDriver);
        if (source == null) return -1;

        try
        {
            source.Open(new DbOpenInfo { FileName = sourceFile, Alias = "_copyfrom", Driver = sourceDriver });
            if (!source.IsOpen) return -1;

            long imported = 0;
            source.GoTop();
            while (!source.Eof)
            {
                var record = source.GetCurrentRecord();
                if (record != null)
                {
                    // Rimappa per NOME campo (i SymbolId del driver sorgente testuale, es. CSV
                    // "colonna N", non corrispondono a quelli della destinazione).
                    var outRecord = new Dictionary<int, object?>();
                    foreach (var srcField in source.Fields)
                    {
                        if (!record.TryGetValue(srcField.SymbolId, out var value)) continue;
                        // GetFieldIndex ritorna la posizione nell'array Fields del target, NON
                        // il suo SymbolId (vedi BaseDriver.GetFieldIndex) — Append si aspetta
                        // un dizionario keyed per SymbolId (stessa convenzione di
                        // GetFieldValue/tutti i driver). Con la posizione grezza come chiave,
                        // ogni TryGetValue lato driver (es. ISAM Engine.Append) falliva sempre
                        // e il record veniva inserito vuoto.
                        var dstIdx = target.GetFieldIndex(srcField.Name);
                        if (dstIdx >= 0) outRecord[target.Fields[dstIdx].SymbolId] = value;
                    }
                    target.Append(outRecord);
                    imported++;
                }
                source.Skip(1);
            }
            return imported;
        }
        finally { source.Close(); }
    }

    /// <summary>SORT TO: ordina e copia verso un nuovo file.</summary>
    public static long SortTo(
        DataSession session,
        int sourceArea,
        string targetFile,
        string targetDriver,
        string sortField,
        bool descending = false)
    {
        var source = session.GetWorkArea(sourceArea)?.Driver;
        if (source == null || !source.IsOpen) return -1;

        // Raccogli tutti i record
        var allRecords = new List<Dictionary<int, object?>>();
        source.GoTop();
        while (!source.Eof)
        {
            var rec = source.GetCurrentRecord();
            if (rec != null) allRecords.Add(rec);
            source.Skip(1);
        }

        // Ordina
        var fieldIdx = source.GetFieldIndex(sortField);
        allRecords.Sort((a, b) =>
        {
            var va = fieldIdx >= 0 && a.TryGetValue(fieldIdx, out var av) ? av : null;
            var vb = fieldIdx >= 0 && b.TryGetValue(fieldIdx, out var bv) ? bv : null;
            int cmp = Comparer<object>.Default.Compare(va, vb);
            return descending ? -cmp : cmp;
        });

        // Scrivi destinazione
        var target = DriverRegistry.Create(targetDriver);
        if (target == null) return -1;
        try
        {
            if (System.IO.File.Exists(targetFile)) System.IO.File.Delete(targetFile);
            target.Open(new DbOpenInfo { FileName = targetFile, Alias = "_sortto", Driver = targetDriver });
            if (source.Fields.Length > 0) target.SetFields(source.Fields);
            foreach (var rec in allRecords) target.Append(rec);
            return allRecords.Count;
        }
        finally { target.Close(); }
    }

    /// <summary>TOTAL ON: raggruppa per campo e somma campi numerici.</summary>
    public static long TotalOn(
        DataSession session,
        int sourceArea,
        string targetFile,
        string targetDriver,
        string groupField,
        string[] sumFields)
    {
        var source = session.GetWorkArea(sourceArea)?.Driver;
        if (source == null || !source.IsOpen) return -1;

        var groups = new Dictionary<object, Dictionary<int, double>>();
        var groupFieldIdx = source.GetFieldIndex(groupField);

        source.GoTop();
        while (!source.Eof)
        {
            var rec = source.GetCurrentRecord();
            if (rec != null)
            {
                var key = groupFieldIdx >= 0 && rec.TryGetValue(groupFieldIdx, out var kv) ? kv : null;
                if (key == null) key = DBNull.Value;

                if (!groups.TryGetValue(key, out var sums))
                    groups[key] = sums = new Dictionary<int, double>();

                foreach (var sf in sumFields)
                {
                    var sfIdx = source.GetFieldIndex(sf);
                    if (sfIdx >= 0 && rec.TryGetValue(sfIdx, out var sv) && sv != null)
                    {
                        if (!sums.TryGetValue(sfIdx, out var cur)) cur = 0;
                        sums[sfIdx] = cur + Convert.ToDouble(sv);
                    }
                }
            }
            source.Skip(1);
        }

        var target = DriverRegistry.Create(targetDriver);
        if (target == null) return -1;
        try
        {
            if (System.IO.File.Exists(targetFile)) System.IO.File.Delete(targetFile);
            target.Open(new DbOpenInfo { FileName = targetFile, Alias = "_totalon", Driver = targetDriver });
            if (source.Fields.Length > 0) target.SetFields(source.Fields);
            foreach (var (key, sums) in groups)
            {
                var outRec = new Dictionary<int, object?> { [groupFieldIdx] = key };
                foreach (var (sfIdx, total) in sums)
                    outRec[sfIdx] = total;
                target.Append(outRec);
            }
            return groups.Count;
        }
        finally { target.Close(); }
    }
}
