// FILE: Interpreter.IlBackendCodegen.Partial.IlProgramCompiler.Clock.cs
// Comandi del dominio CLOCK (tempo a passo fisso) sulla gamba IL: ogni verbo è UNA chiamata a
// un metodo di IlRuntime, che inoltra alla facciata gmb_clock_frame_* (CommonLib.ClockNative).
// Nessuna copia dell'orologio nel managed: il tempo esiste solo nel runtime C (§0bis).

using System;
using System.Reflection.Emit;
using Basic;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    private void EmitClockCommandStatement(ClockCommandStmt stmt)
    {
        var a = stmt.Arguments;

        switch (stmt.Command.Kind)
        {
            case TokenKind.Pause:
                RequireClockArgs(stmt, 0, "CLOCK PAUSE");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ClockPause)));
                return;

            case TokenKind.Resume:
                RequireClockArgs(stmt, 0, "CLOCK RESUME");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ClockResume)));
                return;

            case TokenKind.Scale:
                RequireClockArgs(stmt, 1, "CLOCK SCALE secondi");
                EmitDoubleArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ClockScale)));
                return;

            case TokenKind.Fps:
                RequireClockArgs(stmt, 1, "CLOCK FPS frequenza");
                EmitDoubleArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ClockFps)));
                return;

            default:
                throw new NotSupportedException($"Backend IL: comando CLOCK '{stmt.Command.Kind}' non supportato.");
        }
    }

    private static void RequireClockArgs(ClockCommandStmt stmt, int n, string usage)
    {
        if (stmt.Arguments.Length != n)
            throw new NotSupportedException($"Backend IL: {usage} — ricevuti {stmt.Arguments.Length} argomenti.");
    }
}
