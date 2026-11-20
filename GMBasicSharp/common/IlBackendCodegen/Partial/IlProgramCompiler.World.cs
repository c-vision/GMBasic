// FILE: Interpreter.IlBackendCodegen.Partial.IlProgramCompiler.World.cs
// Comandi del dominio WORLD (motore 3D) sulla gamba IL: ogni verbo è UNA chiamata a un metodo di
// IlRuntime, che inoltra alla facciata gmb_world_* (CommonLib.World3DNative). Nessuna copia dello
// stato 3D nel managed: il mondo esiste solo nel runtime C (§0bis).

using System;
using System.Reflection.Emit;
using Basic;
using Compiler;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    private void EmitWorldCommandStatement(WorldCommandStmt stmt)
    {
        var a = stmt.Arguments;

        switch (stmt.Command.Kind)
        {
            case TokenKind.Load:
                RequireArgs(stmt, 1, "WORLD LOAD \"mappa\"");
                EmitStringArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldLoad)));
                return;

            case TokenKind.Close:
                RequireArgs(stmt, 0, "WORLD CLOSE");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldClose)));
                return;

            case TokenKind.Tick:
                RequireArgs(stmt, 1, "WORLD TICK secondi");
                EmitDoubleArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldTick)));
                return;

            case TokenKind.SecMove:
                if (a.Length < 3) throw new NotSupportedException("Backend IL: " + "WORLD SECMOVE settore, velocità, pavimento[, soffitto]");
                EmitInt64Argument(a[0]);
                EmitDoubleArgument(a[1]);
                EmitDoubleArgument(a[2]);
                if (a.Length >= 4)
                {
                    EmitDoubleArgument(a[3]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldSecMoveCeil)));
                }
                else
                {
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldSecMove)));
                }
                return;

            case TokenKind.Height:
                RequireArgs(stmt, 1, "WORLD HEIGHT settore");
                EmitInt64Argument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldHeight)));
                return;

            case TokenKind.Anim:
                RequireArgs(stmt, 4, "WORLD ANIM settore, prima, quante, fps");
                EmitInt64Argument(a[0]);
                EmitInt64Argument(a[1]);
                EmitInt64Argument(a[2]);
                EmitDoubleArgument(a[3]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldAnim)));
                return;

            case TokenKind.Scroll:
                RequireArgs(stmt, 4, "WORLD SCROLL settore, what, u/s, v/s");
                EmitInt64Argument(a[0]);
                EmitInt64Argument(a[1]);
                EmitDoubleArgument(a[2]);
                EmitDoubleArgument(a[3]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldScroll)));
                return;

            case TokenKind.LightFx:
                RequireArgs(stmt, 4, "WORLD LIGHTFX settore, modo, amount, rate");
                EmitInt64Argument(a[0]);
                EmitInt64Argument(a[1]);
                EmitDoubleArgument(a[2]);
                EmitDoubleArgument(a[3]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldLightFx)));
                return;

            case TokenKind.Lamp:
                RequireArgs(stmt, 5, "WORLD LAMP x, y, z, raggio, livello");
                EmitDoubleArgument(a[0]);
                EmitDoubleArgument(a[1]);
                EmitDoubleArgument(a[2]);
                EmitDoubleArgument(a[3]);
                EmitDoubleArgument(a[4]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WorldLamp)));
                return;

            default:
                throw new NotSupportedException($"Backend IL: comando WORLD '{stmt.Command.Kind}' non supportato.");
        }
    }

    // --- letture (funzioni) ----------------------------------------------------------------

    /// <summary>WORLD.FLOORZ(settore) e simili: un argomento intero, risultato double.</summary>
    private void EmitWorldReadSector(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 argomento (settore).");
        EmitInt64Argument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    /// <summary>WORLD.TIME(): nessun argomento, risultato double.</summary>
    private void EmitWorldReadNoArg(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 0)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} non accetta argomenti.");
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    private static void RequireArgs(WorldCommandStmt stmt, int n, string usage)
    {
        if (stmt.Arguments.Length != n)
            throw new NotSupportedException($"Backend IL: {usage} — ricevuti {stmt.Arguments.Length} argomenti.");
    }
}
