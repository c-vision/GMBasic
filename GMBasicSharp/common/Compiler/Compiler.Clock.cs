// FILE: Interpreter.Compiler.Clock.cs
// Comandi del dominio CLOCK (tempo a passo fisso) verso la bytecode PCode: argomenti sullo
// stack e UNA syscall per verbo. Il caso è eseguito da PCodeProcessor.Syscalls (gamba PCode) e
// dalla tabella syscall di gmb_vm_core.c (gamba VM/C): entrambe inoltrano alla facciata
// gmb_clock_frame_* del runtime C, la sola proprietaria dell'orologio.

namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>Syscall del verbo CLOCK. TokenKind come chiave, mai il lessema (§12).</summary>
    private static SysCall ClockSysCall(TokenKind kind) => kind switch
    {
        TokenKind.Pause => SysCall.CLOCK_PAUSE,
        TokenKind.Resume => SysCall.CLOCK_RESUME,
        TokenKind.Scale => SysCall.CLOCK_SCALE,
        TokenKind.Fps => SysCall.CLOCK_FPS,
        _ => throw new NotImplementedException($"CLOCK: verbo '{kind}' non supportato dalla PCodeVM.")
    };

    /// <summary>
    /// Compila un comando CLOCK: valuta gli argomenti (che restano sullo stack nell'ordine della
    /// sorgente) e chiama la syscall del verbo. I comandi non hanno valore di ritorno (§1).
    /// </summary>
    private void VisitClockCommandStatement(ClockCommandStmt stmt)
    {
        foreach (var arg in stmt.Arguments)
            VisitExpression(arg);

        EmitByte((byte)OpCode.OP_SYSCALL);
        EmitSysCall(ClockSysCall(stmt.Command.Kind));
        EmitByte((byte)stmt.Arguments.Length);
    }
}
