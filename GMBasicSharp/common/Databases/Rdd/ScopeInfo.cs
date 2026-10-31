// FILE: Basic.Databases.Rdd.ScopeInfo.cs
// ScopeInfo: controllo ambito operazioni (NEXT n, RECORD n, REST, ALL).
// Stile Clipper/XSharp: DELETE NEXT 5, REPLACE ALL Name WITH 'X', ecc.

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Tipo di scope operazione.</summary>
public enum ScopeType
{
    All,        // Tutti i record
    Next,       // Prossimi N record
    Record,     // Record specifico
    Rest,       // Dal corrente alla fine
    Current     // Solo record corrente (default)
}

/// <summary>Informazioni di scope per un'operazione.</summary>
public sealed class ScopeInfoData
{
    public ScopeType Type { get; set; } = ScopeType.Current;
    public long Count { get; set; }      // Per NEXT e RECORD
    public bool Active => Type != ScopeType.Current;

    public static ScopeInfoData Current => new() { Type = ScopeType.Current };
    public static ScopeInfoData All => new() { Type = ScopeType.All };
    public static ScopeInfoData NextN(long n) => new() { Type = ScopeType.Next, Count = n };
    public static ScopeInfoData RecordN(long n) => new() { Type = ScopeType.Record, Count = n };
    public static ScopeInfoData Rest => new() { Type = ScopeType.Rest };
}

public static partial class CoreDb
{
    /// <summary>Esegue un'operazione con scope su un'area.</summary>
    public static long ScopedOperation(
        DataSession session, int area,
        ScopeInfoData scope,
        Func<IDatabaseDriver, bool> perRecordAction,
        string? forCondition = null,
        string? whileCondition = null)
    {
        var driver = session.GetWorkArea(area)?.Driver ?? session.CurrentDriver;
        if (driver == null || !driver.IsOpen) return 0;

        var forEval = forCondition != null
            ? RuntimeExpressionEvaluator.CreateEvaluator(forCondition) : null;
        var whileEval = whileCondition != null
            ? RuntimeExpressionEvaluator.CreateEvaluator(whileCondition) : null;

        long affected = 0;
        // ScopeType non definisce For/While: l'IL passa 5/6 (come documentato in
        // EmitScopeClauseArgs) e prima cadevano nel default = 1 record, quindi
        // `DELETE FOR Saldo > 200` ne cancellava uno solo (e senza GoTop partiva dalla
        // posizione corrente). For/While operano sull'intero file, come Clipper.
        var scopeCode = (int)scope.Type;
        long remaining = scopeCode switch
        {
            0 => long.MaxValue, // All
            1 => scope.Count,   // Next
            3 => long.MaxValue, // Rest
            5 => long.MaxValue, // For
            6 => long.MaxValue, // While
            _ => 1              // Current / Record
        };

        // RECORD n: posiziona sul record specifico
        if (scope.Type == ScopeType.Record)
        {
            driver.GoTo(scope.Count);
            if (!driver.Eof && perRecordAction(driver))
                affected++;
            return affected;
        }

        // ALL (e FOR/WHILE, che operano sull'intero file): parte da top,
        // come in Clipper/XSharp. NEXT/REST partono dalla posizione corrente.
        if (scope.Type == ScopeType.All || scopeCode is 5 or 6)
            driver.GoTop();

        // NEXT/REST/ALL: itera
        while (remaining > 0 && !driver.Eof)
        {
            // WHILE condition
            if (whileEval != null)
            {
                var record = driver.GetCurrentRecord();
                if (record != null)
                {
                    var ctx = BuildContext(driver, record);
                    if (!whileEval(null, ctx)) break;
                }
            }

            bool include = true;
            if (forEval != null)
            {
                var record = driver.GetCurrentRecord();
                if (record != null)
                {
                    var ctx = BuildContext(driver, record);
                    include = forEval(null, ctx);
                }
            }

            if (include)
            {
                if (perRecordAction(driver))
                    affected++;
            }

            remaining--;
            if (remaining > 0) driver.Skip(1);
        }

        return affected;
    }

    /// <summary>DELETE con scope.</summary>
    public static long ScopedDelete(DataSession s, int area, ScopeInfoData scope,
        string? forCond = null, string? whileCond = null)
        => ScopedOperation(s, area, scope, d => { d.Delete(); return true; }, forCond, whileCond);

    /// <summary>RECALL con scope.</summary>
    public static long ScopedRecall(DataSession s, int area, ScopeInfoData scope,
        string? forCond = null, string? whileCond = null)
        => ScopedOperation(s, area, scope, d => { d.Recall(); return true; }, forCond, whileCond);

    /// <summary>REPLACE (PutCurrentRecord) con scope. `fields` è per NOME campo (non
    /// SymbolId): risolto sul driver del RECORD CORRENTE a ogni iterazione (GetFieldIndex),
    /// stesso pattern di DbfDriver/DatabaseService.Commercial.DbfReplaceByName — RddFieldInfo.
    /// SymbolId è l'indice posizionale per DBF/Speed, non l'ID del Linker che il chiamante
    /// avrebbe altrimenti dovuto passare. Legge il record ESISTENTE prima di scrivere: senza,
    /// PutCurrentRecord con un dizionario parziale avrebbe azzerato gli altri campi.</summary>
    public static long ScopedReplace(DataSession s, int area, ScopeInfoData scope,
        Dictionary<string, object?> fields, string? forCond = null, string? whileCond = null)
        => ScopedOperation(s, area, scope, d =>
        {
            var record = d.GetCurrentRecord() ?? new Dictionary<int, object?>();
            foreach (var kv in fields)
            {
                var idx = d.GetFieldIndex(kv.Key);
                if (idx >= 0) record[d.Fields[idx].SymbolId] = kv.Value;
            }
            d.PutCurrentRecord(record);
            return true;
        }, forCond, whileCond);

    private static Dictionary<string, object?> BuildContext(IDatabaseDriver d, Dictionary<int, object?> record)
    {
        var ctx = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in record)
        {
            var fi = Array.Find(d.Fields, f => f.SymbolId == kv.Key);
            if (fi != null) ctx[fi.Name] = kv.Value;
        }
        return ctx;
    }
}
