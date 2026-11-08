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
    /// <summary>SPEED: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> SPEEDBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.SPEEDBEGINGROUPCOMMIT,
        BuiltInFunction.SPEEDENDGROUPCOMMIT,
        BuiltInFunction.SPEEDFLUSHDURABILITY,
        BuiltInFunction.SPEEDINFO,
        BuiltInFunction.SPEEDSETDURABILITY,
        BuiltInFunction.SPEEDVALIDATE,
    };

    /// <summary>Speed: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _speedDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> SPEEDDispatch
        => _speedDispatch ??= BuildSpeedDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildSpeedDispatch() => new()
    {
        [BuiltInFunction.SPEEDBEGINGROUPCOMMIT] = (task, args) =>
        {
                {
                    var dbfBgc = ResolveDbfEngine(task, args.Length > 0 ? args[0] : null);
                    if (dbfBgc != null) try { dbfBgc.BeginGroupCommit(); } catch { }
                    return true;
                }
                // DBF.ENDGROUPCOMMIT([area]) — chiude group-commit, sync se Full.
        },
        [BuiltInFunction.SPEEDENDGROUPCOMMIT] = (task, args) =>
        {
                {
                    var dbfEgc = ResolveDbfEngine(task, args.Length > 0 ? args[0] : null);
                    if (dbfEgc != null) try { dbfEgc.EndGroupCommit(); } catch { }
                    return true;
                }
        },
        [BuiltInFunction.SPEEDSETDURABILITY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): ritorna il mode risultante (string), come il
                // canone StdLib (StringType). Firme: (mode) o (dbAlias, mode).
                    if (args.Length >= 2)
                    return Basic.IlBackend.IlRuntime.SpeedSetDurability(
                args[1]?.ToString() ?? "", args[0]?.ToString() ?? "");
                    return Basic.IlBackend.IlRuntime.SpeedSetDurability(
                args.Length >= 1 ? args[0]?.ToString() ?? "" : "", null);
                // SPEED.FLUSHDURABILITY([dbAlias]) — forza sync su disco.
        },
        [BuiltInFunction.SPEEDFLUSHDURABILITY] = (task, args) =>
        {
                // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    return Basic.IlBackend.IlRuntime.SpeedFlushDurability(
                args.Length > 0 ? args[0]?.ToString() : null) != 0;
                // SPEED.INFO([dbAlias]) → riepilogo manifest/tabelle (stringa).
        },
        [BuiltInFunction.SPEEDINFO] = (task, args) =>
        {
                {
                    var sInfo = ResolveSpeedDatabase(task, args.Length > 0 ? args[0] : null);
                    if (sInfo == null) return "";
                    var sb = new System.Text.StringBuilder();
                sb.AppendLine($"durability_mode: {sInfo.DurabilityMode}");
                sb.AppendLine($"group_commit: {sInfo.IsGroupCommitActive}");
                    foreach (var table in sInfo.Manifest.GetAllTables())
                {
                    var sstCount = table.SstFilesByLevel.Values.Sum(files => files.Count);
                sb.AppendLine($"  {table.Name} (id={table.Id}) sst_files={sstCount} secondary_indexes={table.Schema.SecondaryKeys.Count}");
                }
                    return sb.ToString().TrimEnd();
                }
                // SPEED.VALIDATE([dbAlias][, full]) → BIT integrità (quick o full).
        },
        [BuiltInFunction.SPEEDVALIDATE] = (task, args) =>
        {
                {
                try
                {
                    var fullFlag = args.Length >= 2 && args[1] is bool sfb && sfb;
                    var sVal = ResolveSpeedDatabase(task, args.Length >= 1 && !(args[0] is bool) ? args[0] : null);
                    if (sVal == null) return 0.0;
                    // Convenzione Bit canonica -1 vero / 0 falso (prima 1.0: divergeva
                    // da IL/C99/VM-C e da TYPENAME/confronti).
                    return fullFlag ? (sVal.ValidateIntegrityFull().IsValid ? -1.0 : 0.0)
                : (sVal.ValidateIntegrity().IsValid ? -1.0 : 0.0);
                }
                    catch { return 0.0; }
                }
        },
    };

    private object? ExecuteBuiltInSpeed(PCodeTask task, BuiltInFunction fn, object?[] args)
        => SPEEDDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInSpeed (fn={fn}).");

            }
