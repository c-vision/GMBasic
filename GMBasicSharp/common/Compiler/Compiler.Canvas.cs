// FILE: Compiler/Compiler.Canvas.cs
// Compilazione del comando CANVAS (canvas offscreen) verso il runtime C grafico.
// ABI del syscall CANVAS_OP: op(string), a0..a3(double), path(string) = 7 argomenti.
namespace Compiler;

public sealed partial class Compiler
{
    private void VisitCanvasStatement(CanvasStmt cs)
    {
        var a = cs.Args;
        EmitConstant(cs.Sub);
        for (int i = 0; i < 4; i++)
        {
            Expr? e = cs.Sub switch
            {
                // SAVE handle, "path": l'handle è il primo argomento numerico, il path è stringa.
                "SAVE" => (i == 0 && a.Length > 0) ? a[0] : null,
                // LOAD "path": nessun argomento numerico.
                "LOAD" => null,
                _ => i < a.Length ? a[i] : null
            };
            if (e != null) VisitExpression(e);
            else EmitConstant(0.0);
        }
        Expr? path = cs.Sub switch
        {
            "SAVE" => a.Length > 1 ? a[1] : null,
            "LOAD" => a.Length > 0 ? a[0] : null,
            _ => null
        };
        // Il path può essere un letterale o una VARIABILE stringa: va valutato, non
        // ridotto a "" (bug trovato dal MiniTest: P$ arrivava vuoto a PCode/VM-C).
        if (path != null) VisitExpression(path); else EmitConstant("");
        EmitByte((byte)OpCode.OP_SYSCALL);
        EmitSysCall(SysCall.CANVAS_OP);
        EmitByte((byte)7);
        if (cs.Into != null) AssignVariable(cs.Into, isLet: false);
    }
}
