// FILE: Interpreter.IlBackendCodegen.Partial.IlProgramCompiler.Scene.cs
// Comandi del dominio SCENE (motore 3D, renderer mesh) sulla gamba IL: ogni verbo è UNA chiamata a
// un metodo di IlRuntime, che inoltra alla facciata gmb_scene_* (CommonLib.Scene3DNative). Nessuna
// copia della scena nel managed: la geometria e il target esistono solo nel runtime C (§0bis).

using System;
using System.Reflection.Emit;
using Basic;
using Compiler;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    private void EmitSceneCommandStatement(SceneCommandStmt stmt)
    {
        var a = stmt.Arguments;

        switch (stmt.Command.Kind)
        {
            case TokenKind.SceneReset:
                RequireSceneArgs(stmt, 0, "SCENE RESET");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneReset)));
                return;

            case TokenKind.SceneView:
                RequireSceneArgs(stmt, 3, "SCENE VIEW w, h, fov");
                EmitInt64Argument(a[0]);
                EmitInt64Argument(a[1]);
                EmitDoubleArgument(a[2]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneView)));
                return;

            case TokenKind.SceneTexture:
                // Due forme, un verbo: numero = scacchiera procedurale, stringa = PNG su disco.
                // La scelta e' nel TIPO dell'espressione (stessa regola del frontend C, che usa
                // etype()): una variabile `p$` o un letterale stringa vanno al file.
                RequireSceneArgs(stmt, 1, "SCENE TEXTURE celle | \"file.png\"");
                if (Parser.IsStringExpression(a[0]))
                {
                    EmitStringArgument(a[0]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneTextureFile)));
                }
                else
                {
                    EmitInt64Argument(a[0]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneTexture)));
                }
                return;

            case TokenKind.SceneObject:
                if (a.Length == 5)
                {
                    for (int i = 0; i < 5; i++) EmitInt64Argument(a[i]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneObjectFlags)));
                    return;
                }
                RequireSceneArgs(stmt, 3, "SCENE OBJECT r, g, b [, flags [, luce]]");
                EmitInt64Argument(a[0]);
                EmitInt64Argument(a[1]);
                EmitInt64Argument(a[2]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneObject)));
                return;

            case TokenKind.SceneVertex:
                if (a.Length == 8)
                {
                    for (int i = 0; i < 8; i++) EmitDoubleArgument(a[i]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneVertexN)));
                    return;
                }
                RequireSceneArgs(stmt, 5, "SCENE VERTEX x, y, z, u, v [, nx, ny, nz]");
                for (int i = 0; i < 5; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneVertex)));
                return;

            case TokenKind.SceneQuad:
                RequireSceneArgs(stmt, 4, "SCENE QUAD a, b, c, d");
                for (int i = 0; i < 4; i++) EmitInt64Argument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneQuad)));
                return;

            case TokenKind.SceneBox:
                RequireSceneArgs(stmt, 6, "SCENE BOX x0, y0, z0, x1, y1, z1");
                for (int i = 0; i < 6; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneBox)));
                return;

            case TokenKind.SceneObjPos:
                RequireSceneArgs(stmt, 4, "SCENE OBJPOS x, y, z, yaw");
                for (int i = 0; i < 4; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneObjPos)));
                return;

            case TokenKind.SceneCamera:
                RequireSceneArgs(stmt, 5, "SCENE CAMERA x, y, z, yaw, pitch");
                for (int i = 0; i < 5; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneCamera)));
                return;

            // Flag di materiale (idea #5): stessa forma per tutti e tre (un argomento numerico
            // 0/1, il parser ha già convertito la parola ON/OFF), cambia solo la facciata C.
            case TokenKind.SceneSmooth:
            case TokenKind.SceneAlpha:
            case TokenKind.SceneTwoSided:
            case TokenKind.SceneAa:
            case TokenKind.ScenePick:
            {
                RequireSceneArgs(stmt, 1, "SCENE SMOOTH|ALPHA|TWOSIDED|AA|PICK on|off");
                EmitInt64Argument(a[0]);
                string method = stmt.Command.Kind switch
                {
                    TokenKind.SceneSmooth => nameof(IlRuntime.SceneSmooth),
                    TokenKind.SceneAlpha => nameof(IlRuntime.SceneAlpha),
                    TokenKind.SceneAa => nameof(IlRuntime.SceneAa),
                    TokenKind.ScenePick => nameof(IlRuntime.ScenePickEnable),
                    _ => nameof(IlRuntime.SceneTwoSided)
                };
                _il.Emit(OpCodes.Call, RuntimeMethod(method));
                return;
            }

            case TokenKind.SceneLight:
                RequireSceneArgs(stmt, 4, "SCENE LIGHT dx, dy, dz, livello");
                for (int i = 0; i < 3; i++) EmitDoubleArgument(a[i]);
                EmitInt64Argument(a[3]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneLight)));
                return;

            // P5: superficie interattiva (specifica §3.3).
            case TokenKind.SceneZone:
                if (a.Length == 8)
                {
                    EmitInt64Argument(a[0]);
                    for (int i = 1; i < 7; i++) EmitDoubleArgument(a[i]);
                    EmitStringArgument(a[7]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneZone)));
                    return;
                }
                RequireSceneArgs(stmt, 7, "SCENE ZONE id, x1, y1, x2, y2, z1, z2");
                EmitInt64Argument(a[0]);
                for (int i = 1; i < 7; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneZone)));
                return;

            case TokenKind.ScenePlayer:
                RequireSceneArgs(stmt, 4, "SCENE PLAYER raggio, altezza, occhio, step");
                for (int i = 0; i < 4; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ScenePlayer)));
                return;

            case TokenKind.SceneMove:
                RequireSceneArgs(stmt, 2, "SCENE MOVE avanti, destra");
                EmitDoubleArgument(a[0]);
                EmitDoubleArgument(a[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneMove)));
                return;

            case TokenKind.SceneTurn:
                if (a.Length < 1 || a.Length > 2)
                    throw new NotSupportedException("Backend IL: SCENE TURN dyaw [, dpitch].");
                EmitDoubleArgument(a[0]);
                if (a.Length == 2) EmitDoubleArgument(a[1]); else _il.Emit(OpCodes.Ldc_R8, 0.0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneTurn)));
                return;

            case TokenKind.SceneLift:
                RequireSceneArgs(stmt, 1, "SCENE LIFT dy");
                EmitDoubleArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneLift)));
                return;

            case TokenKind.ScenePresent:
                RequireSceneArgs(stmt, 0, "SCENE PRESENT non accetta argomenti");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ScenePresent)));
                return;

            // P4: lampade, fog, FAR, cielo, colormap, thing.
            case TokenKind.SceneLamp:
                if (a.Length == 10)
                {
                    EmitInt64Argument(a[0]);
                    for (int i = 1; i < 10; i++) EmitDoubleArgument(a[i]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneLampSpot)));
                    return;
                }
                RequireSceneArgs(stmt, 6, "SCENE LAMP n, x, y, z, raggio, quantità");
                EmitInt64Argument(a[0]);
                for (int i = 1; i < 6; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneLamp)));
                return;

            case TokenKind.SceneFog:
                RequireSceneArgs(stmt, 1, "SCENE FOG distanza");
                EmitDoubleArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneFog)));
                return;

            case TokenKind.SceneFar:
                RequireSceneArgs(stmt, 1, "SCENE FAR distanza");
                EmitDoubleArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneFar)));
                return;

            case TokenKind.SceneSky:
                RequireSceneArgs(stmt, 2, "SCENE SKY repeat, shift");
                EmitDoubleArgument(a[0]);
                EmitDoubleArgument(a[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneSky)));
                return;

            case TokenKind.SceneColormap:
                RequireSceneArgs(stmt, 0, "SCENE COLORMAP non accetta argomenti");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneColorMap)));
                return;

            case TokenKind.SceneThing:
                RequireSceneArgs(stmt, 6, "SCENE THING n, x, y, z, w, h");
                EmitInt64Argument(a[0]);
                for (int i = 1; i < 6; i++) EmitDoubleArgument(a[i]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneThing)));
                return;

            case TokenKind.SceneRender:
                RequireSceneArgs(stmt, 0, "SCENE RENDER");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneRender)));
                return;

            case TokenKind.SceneSave:
                RequireSceneArgs(stmt, 1, "SCENE SAVE path$");
                EmitStringArgument(a[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneSave)));
                return;

            default:
                throw new NotSupportedException($"Backend IL: comando SCENE '{stmt.Command.Kind}' non supportato.");
        }
    }

    // --- letture (funzioni) ----------------------------------------------------------------

    /// <summary>SCENE.VERTS() e simili: nessun argomento, risultato double.</summary>
    private void EmitSceneReadNoArg(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 0)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} non accetta argomenti.");
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    /// <summary>SCENE.FLOORAT/CEILAT/ZONE(x, y, z): tre argomenti double, risultato double.</summary>
    private void EmitSceneReadPoint(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 3 argomenti (x, y, z).");
        for (int i = 0; i < 3; i++) EmitDoubleArgument(expr.Args[i]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    /// <summary>SCENE.ZONETAG(id): un argomento intero, risultato stringa.</summary>
    private void EmitSceneReadZoneTag(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: SCENE.ZONETAG richiede 1 argomento (id).");
        EmitInt64Argument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SceneZoneTag)));
    }

    /// <summary>SCENE.PIXEL(x, y): due argomenti interi, risultato double.</summary>
    private void EmitSceneReadPixel(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 2 argomenti (x, y).");
        EmitInt64Argument(expr.Args[0]);
        EmitInt64Argument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName));
    }

    private static void RequireSceneArgs(SceneCommandStmt stmt, int n, string usage)
    {
        if (stmt.Arguments.Length != n)
            throw new NotSupportedException($"Backend IL: {usage} — ricevuti {stmt.Arguments.Length} argomenti.");
    }
}
