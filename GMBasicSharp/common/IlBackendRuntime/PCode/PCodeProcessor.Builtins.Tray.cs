// FILE: PCodeProcessor.Builtins.Tray.cs — namespace TRAY (2026-10-02).
// Dispatch a TABELLA (Dictionary), non switch (§0quater). Delega a IlRuntime.Tray.cs:
// implementazione gestita unica (P/Invoke a gmb_tray), STESSA semantica dei backend C (§0bis).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>TRAY: icona nella barra del SO.</summary>
    private static readonly HashSet<BuiltInFunction> TRAYBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.TRAYCREATE,
        BuiltInFunction.TRAYITEM,
        BuiltInFunction.TRAYSEP,
        BuiltInFunction.TRAYTOOLTIP,
        BuiltInFunction.TRAYPOLL,
        BuiltInFunction.TRAYREMOVE,
        BuiltInFunction.TRAYBACKEND,
        BuiltInFunction.TRAYSELFTEST,
        BuiltInFunction.TRAYAVAILABLE,
        BuiltInFunction.TRAYLAST,
    };

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _trayDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> TRAYDispatch
        => _trayDispatch ??= BuildTrayDispatch();

    private static long TrayLong(object?[] args, int i) => (long)SafeDouble(ArgOr(args, i));
    private static string TrayStr(object?[] args, int i) => ArgOr(args, i)?.ToString() ?? "";

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildTrayDispatch() => new()
    {
        [BuiltInFunction.TRAYCREATE] = (t, a) => IlRuntime.TrayCreate(TrayStr(a, 0), TrayStr(a, 1)),
        [BuiltInFunction.TRAYITEM] = (t, a) => IlRuntime.TrayItem(TrayLong(a, 0), TrayStr(a, 1)),
        [BuiltInFunction.TRAYSEP] = (t, a) => IlRuntime.TraySep(),
        [BuiltInFunction.TRAYTOOLTIP] = (t, a) => IlRuntime.TrayTooltip(TrayStr(a, 0)),
        [BuiltInFunction.TRAYPOLL] = (t, a) => IlRuntime.TrayPoll(),
        [BuiltInFunction.TRAYREMOVE] = (t, a) => IlRuntime.TrayRemove(),
        [BuiltInFunction.TRAYBACKEND] = (t, a) => IlRuntime.TrayBackend(),
        [BuiltInFunction.TRAYSELFTEST] = (t, a) => IlRuntime.TraySelfTest(),
        [BuiltInFunction.TRAYAVAILABLE] = (t, a) => IlRuntime.TrayAvailable(),
        [BuiltInFunction.TRAYLAST] = (t, a) => IlRuntime.TrayLast(),
    };

    private object? ExecuteBuiltInTray(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        if (TRAYDispatch.TryGetValue(fn, out var h)) return h(task, args);
        throw new NotSupportedException($"PCode: builtin TRAY non gestito: {fn}");
    }
}
