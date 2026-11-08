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
    /// <summary>MOUSE: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> MOUSEBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.MOUSEX,
        BuiltInFunction.MOUSEY,
        BuiltInFunction.MOUSEBUTTON,
        BuiltInFunction.MOUSEWHEEL,
        BuiltInFunction.MOUSECLICK,
        BuiltInFunction.MOUSEDBLCLICK,
        BuiltInFunction.MOUSEMOVE,
        BuiltInFunction.MOUSEPRESENT,
        BuiltInFunction.MOUSELEFT,
        BuiltInFunction.MOUSERIGHT,
        BuiltInFunction.MOUSEMIDDLE,
    };

    /// <summary>Mouse: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _mouseDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> MOUSEDispatch
        => _mouseDispatch ??= BuildMouseDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildMouseDispatch() => new()
    {
        [BuiltInFunction.MOUSEX] = (task, args) =>
        {
                // Runtime comune (IlRuntime): polimorfico GUI pixel / TUI celle con
                // sottoscrizione eventi SDL — unica implementazione.
                return Basic.IlBackend.IlRuntime.MouseX();
        },
        [BuiltInFunction.MOUSEY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MouseY();
        },
        [BuiltInFunction.MOUSEBUTTON] = (task, args) =>
        {
                return args.Length > 0
                ? Basic.IlBackend.IlRuntime.MouseButton(SafeDouble(ArgOr(args, 0)))
                : Basic.IlBackend.IlRuntime.MouseButton();
        },
        [BuiltInFunction.MOUSEWHEEL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MouseWheel();
        },
        [BuiltInFunction.MOUSECLICK] = (task, args) =>
        {
                if (args.Length >= 4)
                return Basic.IlBackend.IlRuntime.MouseClick(
                SafeDouble(ArgOr(args, 0)), SafeDouble(ArgOr(args, 1)),
                SafeDouble(ArgOr(args, 2)), SafeDouble(ArgOr(args, 3)));
                return Basic.IlBackend.IlRuntime.MouseClick();
        },
        [BuiltInFunction.MOUSEDBLCLICK] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MouseDblClick();
        },
        [BuiltInFunction.MOUSEMOVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MouseMove();
        },
        [BuiltInFunction.MOUSEPRESENT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MousePresent();
        },
        [BuiltInFunction.MOUSELEFT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MouseLeft();
        },
        [BuiltInFunction.MOUSERIGHT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MouseRight();
        },
        [BuiltInFunction.MOUSEMIDDLE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.MouseMiddle();
        },
    };

    private object? ExecuteBuiltInMouse(PCodeTask task, BuiltInFunction fn, object?[] args)
        => MOUSEDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInMouse (fn={fn}).");

}
