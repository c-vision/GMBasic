// FILE: IlProgramCompiler.Tray.cs — emissione IL del namespace TRAY (2026-10-02).
// §0quater: file di dominio (nessun nuovo case nel dispatcher centrale oltre l'hook).
// I metodi runtime stanno in IlRuntime.Tray.cs (P/Invoke a gmb_tray via TrayUtils).

using System.Reflection.Emit;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    private static void RequireTrayArgs(BuiltinFunctionCallExpr expr, int n)
    {
        if (expr.Args.Length != n)
            throw new NotSupportedException(
                $"Backend IL: TRAY.{expr.Name.Lexeme.ToUpperInvariant()} richiede {n} argomento/i.");
    }

    /// <summary>True se il TokenKind appartiene al namespace TRAY (hook del dispatcher).</summary>
    private bool TryEmitTrayCall(BuiltinFunctionCallExpr expr)
    {
        switch (expr.Name.Kind)
        {
            case TokenKind.TrayCreate:
                RequireTrayArgs(expr, 2);
                EmitStringArgument(expr.Args[0]);
                EmitStringArgument(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayCreate), [typeof(string), typeof(string)]));
                return true;
            case TokenKind.TrayItem:
                RequireTrayArgs(expr, 2);
                EmitInt64Argument(expr.Args[0]);
                EmitStringArgument(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayItem), [typeof(long), typeof(string)]));
                return true;
            case TokenKind.TraySep:
                RequireTrayArgs(expr, 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TraySep), System.Type.EmptyTypes));
                return true;
            case TokenKind.TrayTooltip:
                RequireTrayArgs(expr, 1);
                EmitStringArgument(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayTooltip), [typeof(string)]));
                return true;
            case TokenKind.TrayPoll:
                RequireTrayArgs(expr, 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayPoll), System.Type.EmptyTypes));
                return true;
            case TokenKind.TrayRemove:
                RequireTrayArgs(expr, 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayRemove), System.Type.EmptyTypes));
                return true;
            case TokenKind.TrayBackend:
                RequireTrayArgs(expr, 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayBackend), System.Type.EmptyTypes));
                return true;
            case TokenKind.TraySelfTest:
                RequireTrayArgs(expr, 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TraySelfTest), System.Type.EmptyTypes));
                return true;
            case TokenKind.TrayAvailable:
                RequireTrayArgs(expr, 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayAvailable), System.Type.EmptyTypes));
                return true;
            case TokenKind.TrayLast:
                RequireTrayArgs(expr, 0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrayLast), System.Type.EmptyTypes));
                return true;
        }
        return false;
    }
}
