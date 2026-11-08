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

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
            {
    /// <summary>DBF: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> DBFBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.DBFSELECTED,
        BuiltInFunction.DBFFLUSHDURABILITY,
        BuiltInFunction.DBFREPLICATIONSHIP,
        BuiltInFunction.DBFREPLICATIONRECV,
        BuiltInFunction.DBFILTER,
        BuiltInFunction.DBFLOOKUP,
    };

    /// <summary>Dbf: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _dbfDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> DBFDispatch
        => _dbfDispatch ??= BuildDbfDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildDbfDispatch() => new()
    {
        [BuiltInFunction.DBFSELECTED] = (task, args) =>
        {
                // Runtime comune (IlRuntime.DbfSelected): area risolta via DataSession
                // (FindAreaByAlias), come ResolveAreaNum — unica implementazione.
                    return Basic.IlBackend.IlRuntime.DbfSelected(args.Length > 0 ? args[0]?.ToString() : null);
        },
        [BuiltInFunction.DBFFLUSHDURABILITY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    /* Engine di PCODE (DataSession), non i registri di IlRuntime: delegando a
                     * IlRuntime la gamba PCode ritornava 0 (quei registri sono propri di IL).
                     * -1 = successo, come il path syscall (§ dbf_lock_filter.bas). */
                    var fdEngine = ResolveDbfEngine(task, args.Length > 0 ? args[0] : null);
                    if (fdEngine == null) return 0.0;
                    fdEngine.FlushDurability();     /* void */
                    return -1.0;                    /* -1 = successo, come IL e la VM/C */
                // DBF.BEGINGROUPCOMMIT([area]) — differisce fsync su auto-commit.
                // DBF.REPLICATIONSHIP([area,] host, port) → LONG (record inviati).
        },
        [BuiltInFunction.DBFREPLICATIONSHIP] = (task, args) =>
        {
                {
                try
                {
                    var (eng, host, port) = args.Length >= 3
                        ? (ResolveDbfEngine(task, args[0]), args[1]?.ToString() ?? "", (int)SafeDouble(args[2]))
                        : (ResolveDbfEngine(task, null), args[0]?.ToString() ?? "", (int)SafeDouble(args[1]));
                    return eng != null ? (long)eng.ReplicationShipTcp(host, port) : 0L;
                }
                    catch { return 0L; }
                }
                // DBF.REPLICATIONRECV([area,] port) → LONG (record ricevuti).
        },
        [BuiltInFunction.DBFREPLICATIONRECV] = (task, args) =>
        {
                {
                try
                {
                    var (eng2, port2) = args.Length >= 2
                        ? (ResolveDbfEngine(task, args[0]), (int)SafeDouble(args[1]))
                        : (ResolveDbfEngine(task, null), (int)SafeDouble(args[0]));
                    return eng2 != null ? (long)eng2.ReplicationRecvTcp(port2) : 0L;
                }
                    catch { return 0L; }
                }
            
                // ISAM.INFO([file | alias]) → JsonWrapper con header/heap/durabilità.
            
                // ────────────────────────────────────────────────────────────────
                // DBF/ISAM — accessor di work area e campo (BaseDriver/IDatabaseDriver).
                // ────────────────────────────────────────────────────────────────
                // DBF.USED([area]) → -1 se l'area è aperta.
        },
        [BuiltInFunction.DBFILTER] = (task, args) =>
        {
                {
                    /* Prima l'area CORRENTE della sessione (stessa fonte del backend IL, che
                     * funziona): il tracking dell'area del PCode può non essere aggiornato e
                     * ResolveDbfEngine tornava null → DBFILTER() sempre vuoto. */
                    var flSessDrv = _dataSession.GetWorkArea(_dataSession.CurrentArea)?.Driver
                                    as Basic.Databases.Rdd.DbfDriver;
                    if (flSessDrv != null) { try { return flSessDrv.FilterExpression ?? ""; } catch { } }
                    var flDriver = DbfDriverForEngine(ResolveDbfEngine(task, args.Length > 0 ? args[0] : null))
                                   ?? ResolveDbfDriver(task, args.Length > 0 ? args[0] : null);
                    if (flDriver is Basic.Databases.Rdd.BaseDriver flBase)
                    try { return flBase.FilterExpression ?? ""; } catch { }
                    return "";
                }
                // DBF.DBRELATION([area]) → -1 se c'è una relazione attiva.
        },
        [BuiltInFunction.DBFLOOKUP] = (task, args) =>
        {
            // Ordine StdLib: LOOKUP(returnField, searchExpr, searchField [, alias]).
            // Scan del driver di PCODE (dal 4o argomento può arrivare un alias inesistente:
            // si ricade sull'area corrente della sessione, la stessa che DBFILTER usa).
            if (args.Length < 3) return "";
            var luRetField = args[0]?.ToString() ?? "";
            var luSearchVal = args[1];
            var luSearchField = args[2]?.ToString() ?? "";
            var luDriver = _dataSession.GetWorkArea(_dataSession.CurrentArea)?.Driver
                           ?? ResolveDbfDriver(task, args.Length >= 4 ? args[3] : null)
                           ?? ResolveDbfDriver(task, null);
            if (luDriver == null || luRetField.Length == 0 || luSearchField.Length == 0) return "";
            /* L'ENGINE va posizionato esplicitamente: il cursore del driver può restare a EOF
             * (GetFieldValue vuoto, RecNo = ultimo+1) e la scansione non trovava nulla
             * (§ MiniTests/dbf/dbf_lock_filter.bas: LOOKUP vuoto su PCode). */
            if (luDriver is Basic.Databases.Rdd.DbfDriver dbfD && dbfD.Engine is { } engD)
            {
                engD.GoTop();
                dbfD.SyncCursorState();
            }
            else luDriver.GoTop();

            for (var luRec = 0; luRec < luDriver.RecCount; luRec++)
            {
                // I campi DBF sono PADDED ("A2        "): confronto e valore TRIMMATI.
                var luSf = luDriver.GetFieldIndex(luSearchField);
                if (luSf >= 0 && string.Equals((luDriver.GetFieldValue(luSf)?.ToString() ?? "").Trim(),
                                               (luSearchVal?.ToString() ?? "").Trim(), StringComparison.OrdinalIgnoreCase))
                {
                    var luRf = luDriver.GetFieldIndex(luRetField);
                    return luRf >= 0 ? (luDriver.GetFieldValue(luRf)?.ToString() ?? "").Trim() : "";
                }
                luDriver.Skip(1);
            }
            return "";
        },
    };

    private object? ExecuteBuiltInDbf(PCodeTask task, BuiltInFunction fn, object?[] args)
        => DBFDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInDbf (fn={fn}).");

            }
