// FILE: Interpreter.Compiler.World.cs
// Comandi del dominio WORLD (motore 3D) verso la bytecode PCode: argomenti sullo stack e UNA
// syscall per verbo. Il caso è eseguito da PCodeProcessor.Syscalls (gamba PCode) e dalla tabella
// syscall di gmb_vm_core.c (gamba VM/C): entrambe inoltrano alla facciata gmb_world_* del runtime
// C, che è la sola proprietaria dello stato 3D.

namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>Syscall del verbo WORLD. TokenKind come chiave, mai il lessema (§12).</summary>
    private static SysCall WorldSysCall(TokenKind kind) => kind switch
    {
        TokenKind.Load => SysCall.WORLD_LOAD,
        TokenKind.Close => SysCall.WORLD_CLOSE,
        TokenKind.Tick => SysCall.WORLD_TICK,
        TokenKind.SecMove => SysCall.WORLD_SECMOVE,
        TokenKind.Height => SysCall.WORLD_HEIGHT,
        TokenKind.Anim => SysCall.WORLD_ANIM,
        TokenKind.Scroll => SysCall.WORLD_SCROLL,
        TokenKind.LightFx => SysCall.WORLD_LIGHTFX,
        TokenKind.Lamp => SysCall.WORLD_LAMP,
        _ => throw new NotImplementedException($"WORLD: verbo '{kind}' non supportato dalla PCodeVM.")
    };

    /// <summary>
    /// Compila un comando WORLD: valuta gli argomenti (che restano sullo stack nell'ordine della
    /// sorgente) e chiama la syscall del verbo. I comandi non hanno valore di ritorno (§1): il
    /// codice di ritorno della facciata C non è leggibile dal BASIC.
    /// <para>
    /// SECMOVE ha DUE syscall (3 o 4 argomenti) perché l'esecutore non riceve l'arità: la forma
    /// con il soffitto deve essere distinguibile senza contare i valori sullo stack.
    /// </para>
    /// </summary>
    private void VisitWorldCommandStatement(WorldCommandStmt stmt)
    {
        var call = stmt.Command.Kind == TokenKind.SecMove && stmt.Arguments.Length >= 4
            ? SysCall.WORLD_SECMOVE_CEIL
            : WorldSysCall(stmt.Command.Kind);

        foreach (var arg in stmt.Arguments)
            VisitExpression(arg);

        EmitByte((byte)OpCode.OP_SYSCALL);
        EmitSysCall(call);
        EmitByte((byte)stmt.Arguments.Length);
    }
}
