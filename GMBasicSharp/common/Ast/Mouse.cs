// FILE: Interpreter.Ast.Mouse.cs

namespace Basic;

/// <summary>
/// Rappresenta l'istruzione MOUSE ON o MOUSE OFF.
/// </summary>
[JsonDerivedType(typeof(MouseConfigStmt), nameof(MouseConfigStmt))]
public sealed record MouseConfigStmt(bool Enabled, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"MOUSE {(Enabled ? "ON" : "OFF")}";

    public override AstNode DeepClone()
    {
        return new MouseConfigStmt(Enabled, (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione MOUSE SHOW o MOUSE HIDE.
/// </summary>
[JsonDerivedType(typeof(MouseVisibilityStmt), nameof(MouseVisibilityStmt))]
public sealed record MouseVisibilityStmt(bool Visible, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"MOUSE {(Visible ? "SHOW" : "HIDE")}";

    public override AstNode DeepClone()
    {
        return new MouseVisibilityStmt(Visible, (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione MOUSE SET x, y per posizionare il cursore.
/// </summary>
[JsonDerivedType(typeof(MouseSetStmt), nameof(MouseSetStmt))]
public sealed record MouseSetStmt(Expr X, Expr Y, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"MOUSE SET {X}, {Y}";

    public override AstNode DeepClone()
    {
        return new MouseSetStmt((Expr)X.DeepClone(), (Expr)Y.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione MOUSE LIMIT x1, y1, x2, y2 per restringere l'area del mouse,
/// oppure MOUSE UNLIMIT per rimuovere le restrizioni (in questo caso le espressioni sono null).
/// </summary>
[JsonDerivedType(typeof(MouseLimitStmt), nameof(MouseLimitStmt))]
public sealed record MouseLimitStmt(Expr? X1, Expr? Y1, Expr? X2, Expr? Y2, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => X1 != null 
        ? $"MOUSE LIMIT {X1}, {Y1}, {X2}, {Y2}" 
        : "MOUSE UNLIMIT";

    public override AstNode DeepClone()
    {
        return new MouseLimitStmt(
            (Expr?)X1?.DeepClone(), 
            (Expr?)Y1?.DeepClone(), 
            (Expr?)X2?.DeepClone(), 
            (Expr?)Y2?.DeepClone(), 
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione MOUSE CURSOR n per cambiare l'aspetto del cursore.
/// </summary>
[JsonDerivedType(typeof(MouseCursorStmt), nameof(MouseCursorStmt))]
public sealed record MouseCursorStmt(Expr CursorId, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"MOUSE CURSOR {CursorId}";

    public override AstNode DeepClone()
    {
        return new MouseCursorStmt((Expr)CursorId.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione WAIT MOUSE che sospende l'esecuzione fino a un evento mouse.
/// </summary>
[JsonDerivedType(typeof(MouseWaitStmt), nameof(MouseWaitStmt))]
public sealed record MouseWaitStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "WAIT MOUSE";

    public override AstNode DeepClone()
    {
        return new MouseWaitStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Tipi di eventi mouse supportati.
/// </summary>
public enum MouseEventKind
{
    Click,
    DblClick,
    Down,
    Up,
    Move,
    Scroll
}

/// <summary>
/// Rappresenta l'istruzione ON MOUSE evento azione.
/// Esempio: ON MOUSE CLICK GOSUB MyHandler
/// </summary>
[JsonDerivedType(typeof(OnMouseStmt), nameof(OnMouseStmt))]
public sealed record OnMouseStmt(MouseEventKind EventKind, OnKeyAction Action, Token Token) 
    : Stmt(Token.Position)
{
    public override string ToString() => $"ON MOUSE {EventKind.ToString().ToUpper()} {Action}";

    public override AstNode DeepClone()
    {
        return new OnMouseStmt(EventKind, Action.DeepClone(), (Token)Token.DeepClone());
    }
}

