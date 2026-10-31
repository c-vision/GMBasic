// FILE: Basic.Databases.Rdd.CoreDb.AppendFrom.cs
// APPEND FROM: copia record tra work area tramite IDatabaseDriver.

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

public static partial class CoreDb
{
    /// <summary>
    /// APPEND FROM: copia tutti i record dall'area source all'area target.
    /// Supporta trasferimento cross-driver (es. DBF → ISAM, CSV → DBF).
    /// </summary>
    /// <param name="session">DataSession condiviso.</param>
    /// <param name="targetArea">Area di destinazione.</param>
    /// <param name="sourceArea">Area di origine.</param>
    /// <param name="fields">Campi da copiare (null = tutti).</param>
    /// <param name="forCondition">Espressione filtro opzionale (es. "Balance > 0").</param>
    /// <param name="whileCondition">Continua finché true.</param>
    /// <returns>Numero record copiati, -1 se errore.</returns>
    public static long AppendFrom(
        DataSession session,
        int targetArea,
        int sourceArea,
        string[]? fields = null,
        string? forCondition = null,
        string? whileCondition = null)
    {
        var targetWa = session.GetWorkArea(targetArea);
        var sourceWa = session.GetWorkArea(sourceArea);
        var target = targetWa?.Driver;
        var source = sourceWa?.Driver;

        if (target == null || source == null || !target.IsOpen || !source.IsOpen)
            return -1;

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
                // Filtra campi se specificati
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

                // Valuta condizione FOR
                bool include = true;
                if (eval != null)
                {
                    // Costruisci contesto string-key per l'evaluator
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

            // Condizione WHILE
            if (whileEval != null)
            {
                var ctx = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                foreach (var kv in record ?? new Dictionary<int, object?>())
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

    /// <summary>Sovraccarico semplificato: copia tutto da source a target.</summary>
    public static long AppendFrom(DataSession session, int targetArea, int sourceArea)
        => AppendFrom(session, targetArea, sourceArea, null, null, null);
}
