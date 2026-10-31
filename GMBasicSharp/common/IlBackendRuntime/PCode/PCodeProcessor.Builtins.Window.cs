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
    /// <summary>WINDOW: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> WINDOWBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.WINDOWX,
        BuiltInFunction.WINDOWY,
        BuiltInFunction.WINDOWWIDTH,
        BuiltInFunction.WINDOWHEIGHT,
        BuiltInFunction.WINDOWDESKTOP,
        BuiltInFunction.WINDOWTHEME,
    };

    /// <summary>Window: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).
    /// Property lazy + Build in metodo d'istanza: le lambda catturano this e chiamano i metodi
    /// d'istanza del processor.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _windowDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> WINDOWDispatch
        => _windowDispatch ??= BuildWindowDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildWindowDispatch() => new()
    {
        [BuiltInFunction.WINDOWX] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.WindowX();
        },
        [BuiltInFunction.WINDOWY] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.WindowY();
        },
        [BuiltInFunction.WINDOWWIDTH] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.WindowWidth();
        },
        [BuiltInFunction.WINDOWHEIGHT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.WindowHeight();
        },
        [BuiltInFunction.WINDOWDESKTOP] = (task, args) =>
        {
                    // Forma FUNZIONE (0 arg) = getter, parità IL (GetCurrentDesktop); la forma
                    // comando `WINDOW DESKTOP n` è un altro statement.
                    return Basic.IlBackend.IlRuntime.GetCurrentDesktop();
        },
        [BuiltInFunction.WINDOWTHEME] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.GetThemeName();
        },
    };

    private object? ExecuteBuiltInWindow(PCodeTask task, BuiltInFunction fn, object?[] args)
        => WINDOWDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInWindow (fn={fn}).");

}
