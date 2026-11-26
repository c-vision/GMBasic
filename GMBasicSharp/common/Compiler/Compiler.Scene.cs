// FILE: Interpreter.Compiler.Scene.cs
// Comandi del dominio SCENE (motore 3D, renderer mesh) verso la bytecode PCode: argomenti sullo
// stack e UNA syscall per verbo. Il caso è eseguito da PCodeProcessor.Syscalls (gamba PCode) e
// dalla tabella syscall di gmb_vm_core.c (gamba VM/C): entrambe inoltrano alla facciata
// gmb_scene_* del runtime C, che è la sola proprietaria della scena (§0bis).

namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>Syscall del verbo SCENE. TokenKind come chiave, mai il lessema (§12).</summary>
    private static SysCall SceneSysCall(TokenKind kind, int argc) => kind switch
    {
        TokenKind.SceneReset => SysCall.SCENE_RESET,
        TokenKind.SceneView => SysCall.SCENE_VIEW,
        TokenKind.SceneTexture => SysCall.SCENE_TEXTURE,
        TokenKind.SceneObject => argc == 5 ? SysCall.SCENE_OBJECT_FLAGS : SysCall.SCENE_OBJECT,
        TokenKind.SceneVertex => argc == 8 ? SysCall.SCENE_VERTEX_N : SysCall.SCENE_VERTEX,
        TokenKind.SceneQuad => SysCall.SCENE_QUAD,
        TokenKind.SceneBox => SysCall.SCENE_BOX,
        TokenKind.SceneObjPos => SysCall.SCENE_OBJPOS,
        TokenKind.SceneCamera => SysCall.SCENE_CAMERA,
        TokenKind.SceneRender => SysCall.SCENE_RENDER,
        TokenKind.SceneSave => SysCall.SCENE_SAVE,
        TokenKind.SceneLight => SysCall.SCENE_LIGHT,
        TokenKind.SceneSmooth => SysCall.SCENE_SMOOTH,
        TokenKind.SceneAlpha => SysCall.SCENE_ALPHA,
        TokenKind.SceneTwoSided => SysCall.SCENE_TWOSIDED,
        TokenKind.ScenePresent => SysCall.SCENE_PRESENT,
        // P4: stesso verbo, due forme distinte per numero di argomenti.
        TokenKind.SceneLamp => argc == 10 ? SysCall.SCENE_LAMP_SPOT : SysCall.SCENE_LAMP,
        TokenKind.SceneFog => SysCall.SCENE_FOG,
        TokenKind.SceneFar => SysCall.SCENE_FAR,
        TokenKind.SceneSky => SysCall.SCENE_SKY,
        TokenKind.SceneColormap => SysCall.SCENE_COLORMAP,
        TokenKind.SceneThing => SysCall.SCENE_THING,
        TokenKind.SceneAa => SysCall.SCENE_AA,
        TokenKind.ScenePick => SysCall.SCENE_PICK,
        // P5: superficie interattiva.
        TokenKind.SceneZone => SysCall.SCENE_ZONE,
        TokenKind.ScenePlayer => SysCall.SCENE_PLAYER,
        TokenKind.SceneMove => SysCall.SCENE_MOVE,
        TokenKind.SceneTurn => SysCall.SCENE_TURN,
        TokenKind.SceneLift => SysCall.SCENE_LIFT,
        _ => throw new NotImplementedException($"SCENE: verbo '{kind}' non supportato dalla PCodeVM.")
    };

    /// <summary>
    /// Compila un comando SCENE: valuta gli argomenti (che restano sullo stack nell'ordine della
    /// sorgente) e chiama la syscall del verbo. I comandi non hanno valore di ritorno (§1): il
    /// codice di ritorno della facciata C non è leggibile dal BASIC.
    /// </summary>
    private void VisitSceneCommandStatement(SceneCommandStmt stmt)
    {
        foreach (var arg in stmt.Arguments)
            VisitExpression(arg);

        EmitByte((byte)OpCode.OP_SYSCALL);
        EmitSysCall(SceneSysCall(stmt.Command.Kind, stmt.Arguments.Length));
        EmitByte((byte)stmt.Arguments.Length);
    }
}
