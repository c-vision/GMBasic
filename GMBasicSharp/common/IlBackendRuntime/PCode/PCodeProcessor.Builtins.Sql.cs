// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;
using CommonLib.Trading;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
            {
    /// <summary>SQL: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> SQLBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.SQLFETCH,
        BuiltInFunction.SQLCLOSERESULTSET,
        BuiltInFunction.SQLEXEC,
        BuiltInFunction.SQLLASTINSERTROWID,
        BuiltInFunction.SQLCHANGES,
        BuiltInFunction.SQLDFSCHEMA,
        BuiltInFunction.SQLCOL,
        BuiltInFunction.SQLTOCSV,
        BuiltInFunction.SQLFROMCSV,
    };

    /// <summary>Sql: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _sqlDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> SQLDispatch
        => _sqlDispatch ??= BuildSqlDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildSqlDispatch() => new()
    {
        [BuiltInFunction.SQLFETCH] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    return Basic.IlBackend.IlRuntime.SqlFetch(
                args.Length > 0 ? SafeDouble(ArgOr(args, 0)) : 0) ? 1.0 : 0.0;
        },
        [BuiltInFunction.SQLCLOSERESULTSET] = (task, args) =>
        {
                Basic.IlBackend.IlRuntime.SqlCloseResultSet(
                args.Length > 0 ? SafeDouble(ArgOr(args, 0)) : 0);
                    return 1.0;
        },
        [BuiltInFunction.SQLEXEC] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica del backend IL, che ora supporta
                // anche i parametri (posizionali/named) — unica implementazione.
            var alias = ArgOr(args, 0)?.ToString() ?? "";
            var sql = ArgOr(args, 1)?.ToString() ?? "";
            var extra = args.Length > 2 ? args.Skip(2).ToArray() : Array.Empty<object?>();
            // Coarse-park (regola GPM): una scrittura che riscrive un file (vtab csv/parquet)
            // va eseguita fuori dal worker; letture e scritture leggere restano sincrone.
            if (Basic.IlBackend.IlRuntime.SqlIsHeavyWrite(alias, sql))
            {
                ParkBlocking(task,
                    () => Basic.IlBackend.IlRuntime.SqlExec(alias, sql, extra),
                    r => task.PushNum(r));
                return ParkedBuiltinResult;
            }
            return Basic.IlBackend.IlRuntime.SqlExec(alias, sql, extra);
        },
        [BuiltInFunction.SQLLASTINSERTROWID] = (task, args) =>
        {
                // Runtime comune (IlRuntime): fallback al driver aperto — unica implementazione.
                    return (double)Basic.IlBackend.IlRuntime.SqlLastInsertRowId(
                ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.SQLCHANGES] = (task, args) =>
        {
                    return (double)Basic.IlBackend.IlRuntime.SqlChanges(
                ArgOr(args, 0)?.ToString() ?? "");
            
        },
        // G10 (2026-10-09): ergonomia DataFrame. L'rs in PCode è un UDT (dict) con `__rsid__`.
        [BuiltInFunction.SQLDFSCHEMA] = (task, args) =>
            Basic.IlBackend.IlRuntime.SqlDfSchema(args.Length > 0 ? SqlRsId(args[0]) : 0),
        [BuiltInFunction.SQLCOL] = (task, args) =>
            Basic.IlBackend.IlRuntime.SqlCol(args.Length > 0 ? SqlRsId(args[0]) : 0, ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.SQLTOCSV] = (task, args) =>
            (double)Basic.IlBackend.IlRuntime.SqlToCsv(args.Length > 0 ? SqlRsId(args[0]) : 0, ArgOr(args, 1)?.ToString() ?? ""),
        [BuiltInFunction.SQLFROMCSV] = (task, args) =>
            (double)Basic.IlBackend.IlRuntime.SqlFromCsv(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? ""),
    };

    /// <summary>Handle rs da un argomento: in PCode l'SQLRESULTSET è un UDT (dict) con `__rsid__`.</summary>
    private static double SqlRsId(object? a)
    {
        if (a is System.Collections.Generic.Dictionary<string, object?> d && d.TryGetValue("__rsid__", out var v) && v is not null)
            return SafeDouble(v);
        return SafeDouble(a);
    }

    private object? ExecuteBuiltInSql(PCodeTask task, BuiltInFunction fn, object?[] args)        => SQLDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSql (fn={fn}).");

            }
