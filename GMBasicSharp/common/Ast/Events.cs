// FILE: Interpreter.Ast.Events.cs

namespace Basic;

/// <summary>
/// Rappresenta l'istruzione RAISE EVENT EventName(args...).
/// Usata all'interno di una classe per scatenare un evento.
/// </summary>
[JsonDerivedType(typeof(RaiseEventStmt), nameof(RaiseEventStmt))]
public sealed record RaiseEventStmt(Token EventName, ImmutableArray<Expr> Arguments, Token Token) 
    : Stmt(Token.Position)
{
    public override string ToString() => $"RAISE EVENT {EventName.Lexeme}({string.Join(", ", Arguments)})";

    public override AstNode DeepClone()
    {
        return new RaiseEventStmt(
            (Token)EventName.DeepClone(),
            [.. Arguments.Select(a => (Expr)a.DeepClone())],
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Classe base per le operazioni sui gestori di eventi (ADD/REMOVE HANDLER).
/// </summary>
[JsonDerivedType(typeof(AddHandlerStmt), nameof(AddHandlerStmt))]
[JsonDerivedType(typeof(RemoveHandlerStmt), nameof(RemoveHandlerStmt))]
public abstract record EventHandlerStmt(Expr EventAccess, Expr HandlerExpr, Token Token) 
    : Stmt(Token.Position);

/// <summary>
/// Rappresenta l'istruzione ADD HANDLER obj.Event, MyHandler.
/// </summary>
public sealed record AddHandlerStmt(Expr EventAccess, Expr HandlerExpr, Token Token) 
    : EventHandlerStmt(EventAccess, HandlerExpr, Token)
{
    public override string ToString() => $"ADD HANDLER {EventAccess}, {HandlerExpr}";

    public override AstNode DeepClone()
    {
        return new AddHandlerStmt((Expr)EventAccess.DeepClone(), (Expr)HandlerExpr.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione REMOVE HANDLER obj.Event, MyHandler.
/// </summary>
public sealed record RemoveHandlerStmt(Expr EventAccess, Expr HandlerExpr, Token Token) 
    : EventHandlerStmt(EventAccess, HandlerExpr, Token)
{
    public override string ToString() => $"REMOVE HANDLER {EventAccess}, {HandlerExpr}";

    public override AstNode DeepClone()
    {
        return new RemoveHandlerStmt((Expr)EventAccess.DeepClone(), (Expr)HandlerExpr.DeepClone(), (Token)Token.DeepClone());
    }
}