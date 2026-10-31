// FILE: Basic.IlBackend.DbFilterHookInit.cs
// Collega RuntimeExpressionEvaluator (common/Databases/, sotto IlBackend nel grafo delle
// dipendenze) alla pipeline unificata Compiler -> P-Code -> PCodeVm interno
// (DbExprPCodeEvaluator) tramite il suo hook di iniezione — stesso pattern già usato per BSP
// (vedi common/IlBackend/PCode/BspDispatch.cs). Databases non può referenziare IlBackend
// direttamente (la dipendenza va nella direzione opposta), quindi il collegamento va fatto qui.

using System.Runtime.CompilerServices;
using Basic.Databases.Rdd;

namespace Basic.IlBackend;

internal static class DbFilterHookInit
{
    /// <summary>Eseguito automaticamente al caricamento di questo assembly — garantisce che
    /// l'hook sia impostato prima che qualunque codice BASIC possa eseguire SET FILTER/SORT TO/
    /// JOIN/COPY TO su ISAM/SPEED (tutti in questo stesso assembly o in uno che lo referenzia).</summary>
#pragma warning disable CA2255 // ModuleInitializer intenzionale: hook impostato all'load dell'assembly
    [ModuleInitializer]
    internal static void Register()
    {
        RuntimeExpressionEvaluator.PCodeEvaluatorFactory = CreateEvaluator;
        Basic.Databases.KeyExpressionPCodeHook.CompileFactory = CreateKeyEvaluator;
    }

    private static RuntimeExpressionEvaluator.ExpressionEvaluator? CreateEvaluator(string expression)
    {
        var compiled = DbExprPCodeEvaluator.TryCompile(expression);
        return compiled is null ? null : (_, ctx) => compiled.EvaluateBool(ctx);
    }

    /// <summary>Usa la cache (GetOrCompileCached): a differenza dei filtri (compilati una volta
    /// per SET FILTER e riusati), il callback KeyExpressionEvaluatorCallback di ISAM viene
    /// invocato per OGNI record con la STESSA stringa espressione chiave — senza cache
    /// ricompilerebbe da zero a ogni record.</summary>
    private static Func<IReadOnlyDictionary<string, object?>?, object?>? CreateKeyEvaluator(string expression)
    {
        var compiled = DbExprPCodeEvaluator.GetOrCompileCached(expression);
        return compiled is null ? null : compiled.Evaluate;
    }
}
