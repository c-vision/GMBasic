// FILE: Compiler/Compiler.Pixel.cs
// Compilazione (PCode) dei controlli pixel, CONTROL e FORM_* verso i syscall del bridge.

using System.Collections.Immutable;
using Basic;

namespace Compiler;

public sealed partial class Compiler
{
    private readonly List<(string Name, Target Into, bool IsStr)> _pixelControls = new();
    private int _pixelCounter = 0;

    private static string PxStr(Expr? e) => e is StringExpr se ? (se.Value ?? "") : "";

    /* Nome di riferimento per la composizione (CONTROL ADD): nome-variabile (PARENT c) o stringa. */
    private static string CtrlRefName(List<Expr> a, int i) => i < a.Count ? a[i] switch
    {
        VariableExpr v => v.NameCleaned.Lexeme,
        StringExpr s => s.Value ?? "",
        _ => ""
    } : "";

    private static string PixelSpec(List<PixelClause> cs)
    {
        var parts = new List<string>();
        int Num(PixelClause c) => c.Value is NumberExpr n ? (int)n.Value : 0;
        foreach (var c in cs) switch ((c.Key ?? "").ToUpperInvariant())
        {
            case "NAME": parts.Add($"name=\"{PxStr(c.Value)}\" "); break;
            case "TAG": parts.Add($"tag=\"{PxStr(c.Value)}\" "); break;
            case "TOOLTIP": parts.Add($"tooltip=\"{PxStr(c.Value)}\" "); break;
            case "VISIBLE": parts.Add($"visible={Num(c)} "); break;
            case "ENABLED": parts.Add($"enabled={Num(c)} "); break;
            case "TABINDEX": parts.Add($"tabindex={Num(c)} "); break;
            case "TABSTOP": parts.Add($"tabstop={Num(c)} "); break;
            case "FOCUS": parts.Add("focus "); break;
            case "STYLE": parts.Add(PxStr(c.Value) + " "); break;
        }
        var r = string.Join("", parts).Trim();
        return r.Length > 0 ? r : "";
    }

    private void VisitPixelControlStatement(PixelControlStmt pc)
    {
        var nm = pc.Clauses.FirstOrDefault(c => c.Key.Equals("NAME", StringComparison.OrdinalIgnoreCase)) is { } n && PxStr(n.Value).Length > 0
            ? PxStr(n.Value) : $"px{_pixelCounter++}";
        EmitConstant(pc.Kind.ToUpperInvariant());
        EmitConstant(nm);
        EmitConstant(PixelSpec(pc.Clauses));
        VisitListLiteralExpression(new ListLiteralExpr(pc.Args.ToImmutableArray(), pc.Token));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PIXEL_CTRL); EmitByte((byte)4);
        if (pc.Id != null) { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_LAST_HANDLE); EmitByte((byte)0); AssignVariable(pc.Id, isLet: false); }
        if (pc.Into != null) _pixelControls.Add((nm, pc.Into, false));
    }

    private void VisitControlMethodStatement(ControlMethodStmt cm)
    {
        var a = cm.Args;
        void S(int i) => EmitConstant(i < a.Count ? PxStr(a[i]) : "");
        void N(int i) { if (i < a.Count) VisitExpression(a[i]); else EmitConstant(0.0); }
        switch (cm.ResolveSubKind())
        {
            case TokenKind.Set:
                S(0); S(1); N(2); S(3);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_SET); EmitByte((byte)4);
                break;
            case TokenKind.Get:
                S(0); S(1);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_GET); EmitByte((byte)2);
                if (cm.Into != null) AssignVariable(cm.Into, isLet: false);
                break;
            case TokenKind.ControlGets:
                S(0); S(1);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_GETS); EmitByte((byte)2);
                if (cm.Into != null) AssignVariable(cm.Into, isLet: false);
                break;
            case TokenKind.Move:
                S(0); N(1); N(2); N(3); N(4);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_MOVE); EmitByte((byte)5);
                break;
            case TokenKind.Focus:
                S(0);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_FOCUS); EmitByte((byte)1);
                break;
            case TokenKind.ControlInvalidate:
                S(0);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_INVALIDATE); EmitByte((byte)1);
                break;
            case TokenKind.Refresh:
                S(0);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_REFRESH); EmitByte((byte)1);
                break;
            case TokenKind.ControlZorder:
                S(0); N(1);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_ZORDER); EmitByte((byte)2);
                break;
            case TokenKind.Add:
                /* CONTROL ADD instance AT x,y SIZE w,h [PARENT parent] → registra un UserControl.
                 * Nome controllo = nome-variabile dell'istanza; contenitore = nome-variabile (o
                 * stringa) di PARENT. La composizione annidata usa questi nomi. */
                if (a.Count > 0) VisitExpression(a[0]); else EmitConstant(0.0);
                N(1); N(2); N(3); N(4);
                int _mk = a.FindIndex(e => e is StringExpr se2 && se2.Value == "__UCBIND__");
                int _posEnd = _mk < 0 ? a.Count : _mk;
                EmitConstant(_posEnd > 5 ? CtrlRefName(a, 5) : "");   /* PARENT (contenitore) */
                EmitConstant(CtrlRefName(a, 0));   /* nome del controllo */
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_ADD); EmitByte((byte)7);
                if (cm.Into != null) { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_LAST_HANDLE); EmitByte((byte)0); AssignVariable(cm.Into, isLet: false); }
                /* §11.7-3: triple (evName, metodo, symbolId) dopo il marker → binding handle,ev. */
                for (int bi = _mk + 1; _mk >= 0 && bi + 2 < a.Count; bi += 3)
                {
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_LAST_HANDLE); EmitByte((byte)0);
                    int evIdx = CtrlEvIndex.TryGetValue(PxStr(a[bi]), out var eix) ? eix : 0;
                    EmitConstant((double)evIdx);
                    EmitConstant(PxStr(a[bi + 1]));
                    EmitConstant(a[bi + 2] is NumberExpr ne ? ne.Value : 0.0);
                    VisitExpression(a[0]);
                    EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_BIND_UC_EVENT); EmitByte((byte)5);
                }
                break;
            case TokenKind.Remove:
                if (a.Count > 0) VisitExpression(a[0]); else EmitConstant(0.0);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_REMOVE); EmitByte((byte)1);
                break;
            case TokenKind.Draw:
                /* CONTROL DRAW handle, op, a, b, c, d, str$ → primitiva GraphicsContext. */
                if (a.Count > 0) VisitExpression(a[0]); else EmitConstant(0.0);
                N(1); N(2); N(3); N(4); N(5); S(6);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_DRAW); EmitByte((byte)7);
                break;
        }
    }

    private static readonly System.Collections.Generic.Dictionary<string, int> CtrlEvIndex = new(StringComparer.OrdinalIgnoreCase)
    { {"CLICK",1},{"DBLCLICK",2},{"CHANGE",3},{"GOTFOCUS",4},{"LOSTFOCUS",5},{"MOUSEDOWN",6},{"MOUSEUP",7},
      {"MOUSEMOVE",8},{"KEYDOWN",9},{"KEYPRESS",10},{"KEYUP",11},{"DRAGDROP",12},{"DRAGOVER",13},{"VALIDATE",14},
      {"SCROLL",15},{"DROPDOWN",16},{"UPCLICK",17},{"DOWNCLICK",18},{"NODECLICK",19},{"EXPAND",20},{"COLLAPSE",21},
      {"SELCHANGE",22},{"ROWCOLCHANGE",23},{"WHEN",24} };

    private void VisitFireEventStatement(FireEventStmt fe)
    {
        VisitExpression(fe.Handle);
        VisitExpression(fe.Event);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_FIRE); EmitByte((byte)2);
    }

    private void VisitControlEventStatement(ControlEventStmt ce)
    {
        VisitExpression(ce.Id);
        EmitConstant((double)(CtrlEvIndex.TryGetValue(ce.Event, out var ix) ? ix : 0));
        var kind = (ce.ActionKind ?? "CALL").ToUpperInvariant();
        if (kind == "STOP") { EmitConstant((double)ON_KEY_ACTION_STOP); EmitConstant(0.0); }
        else
        {
            EmitConstant((double)(kind == "GOSUB" ? ON_KEY_ACTION_GOSUB : kind == "GOTO" ? ON_KEY_ACTION_GOTO : ON_KEY_ACTION_CALL));
            var ci = AddConstant(null);
            EmitBytes((byte)OpCode.OP_PUSH_CONST); EmitShort(ci);
            if (ce.TargetToken != null) _constantsToPatch.Add((ci, ce.TargetToken));
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_SET_EVENT_ID); EmitByte((byte)4);
    }

    private void VisitControlEventStatementOld(ControlEventStmt ce)
    {
        VisitExpression(ce.Id);
        EmitConstant(ce.Event.ToUpperInvariant());
        var kind = (ce.ActionKind ?? "CALL").ToUpperInvariant();
        EmitConstant(kind == "STOP" ? "" : (ce.Target ?? ""));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_SET_EVENT_ID); EmitByte((byte)3);
    }

    private void EmitFormReadbacks()
    {
        foreach (var (nm, into, isStr) in _pixelControls)
        {
            EmitConstant(nm);
            if (isStr) { EmitConstant("text"); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_GETS); EmitByte((byte)2); }
            else { EmitConstant("value"); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CTRL_GET); EmitByte((byte)2); }
            AssignVariable(into, isLet: false);
        }
    }

    /// <summary>FORM_FRAME/FORM_CLOSED/FORM_CLOSE (per nome). Ritorna true se gestito.</summary>
    private bool TryVisitFormBuiltin(BuiltinFunctionCallExpr expr)
    {
        var bn = expr.Name.Lexeme?.ToUpperInvariant();
        if (false)
        {
            if (expr.Args.Length > 0) VisitExpression(expr.Args[0]); else EmitConstant(16.0);
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_FRAME); EmitByte((byte)1);
            EmitFormReadbacks();
            EmitConstant(0.0);
            return true;
        }
        if (bn == "WINDOWCLOSED") { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CLOSED); EmitByte((byte)0); return true; }
        if (bn is "CLICKED" or "DOUBLECLICKED" or "CHANGED" or "GOTFOCUSED" or "LOSTFOCUSED")
        {
            if (expr.Args.Length > 0) VisitExpression(expr.Args[0]); else EmitConstant(0.0);
            var sc = bn == "CLICKED" ? SysCall.FORM_CLICKED : bn == "DOUBLECLICKED" ? SysCall.FORM_DBLCLICKED
                   : bn == "CHANGED" ? SysCall.FORM_CHANGED : bn == "GOTFOCUSED" ? SysCall.FORM_GOTFOCUSED : SysCall.FORM_LOSTFOCUSED;
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(sc); EmitByte((byte)1);
            return true;
        }
        if (bn is "CTRLGETPROP" or "CTRLGETPROPS")
        {
            /* §11.1: accessor proprietà controllo (zucchero `istanza.Prop`). */
            if (expr.Args.Length > 0) VisitExpression(expr.Args[0]); else EmitConstant("");
            if (expr.Args.Length > 1) VisitExpression(expr.Args[1]); else EmitConstant("");
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(bn == "CTRLGETPROP" ? SysCall.FORM_CTRL_GET : SysCall.FORM_CTRL_GETS); EmitByte((byte)2);
            return true;
        }
        if (bn == "MOUSECLICKED") { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_CLICKED); EmitByte((byte)0); return true; }
        if (bn == "MOUSEDBLCLICKED") { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_DBLCLICKED); EmitByte((byte)0); return true; }
        if (bn == "MOUSEPRESSED") { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_PRESSED); EmitByte((byte)0); return true; }
        if (bn == "KEYPRESSED") { EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.KEY_PRESSED); EmitByte((byte)0); return true; }
        return false;
    }
}
