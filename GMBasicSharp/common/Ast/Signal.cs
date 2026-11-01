// FILE: Interpreter.Ast.Signal.cs
// Segnali (namespace SIGNAL): sottoscrizione con filtro strutturato, emissione, evento
// ON SIGNAL. L'azione dell'handler riusa MessageAction (CALL/GOSUB/STOP): stesso modello
// del messaging, di cui SIGNAL è una specializzazione trasparente.
using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// SIGNAL SUBSCRIBE [TYPES(..)] [ASSET(..)] [MARKET(..)] [SYMBOLS(..)] [TIMEFRAME(..)] AS "id".
/// Ogni filtro è un'espressione stringa (CSV per TYPES/SYMBOLS); null = nessun filtro.
/// </summary>
[JsonDerivedType(typeof(SignalSubscribeStmt), nameof(SignalSubscribeStmt))]
public sealed record SignalSubscribeStmt(Expr Id, Expr? Types, Expr? Asset, Expr? Market,
    Expr? Symbols, Expr? Timeframe, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SIGNAL SUBSCRIBE AS {Id}";
    public override AstNode DeepClone() => new SignalSubscribeStmt((Expr)Id.DeepClone(),
        (Expr?)Types?.DeepClone(), (Expr?)Asset?.DeepClone(), (Expr?)Market?.DeepClone(),
        (Expr?)Symbols?.DeepClone(), (Expr?)Timeframe?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>SIGNAL EMIT tipo, simbolo, valore [ASSET(..)] [MARKET(..)] [TIMEFRAME(..)].</summary>
[JsonDerivedType(typeof(SignalEmitStmt), nameof(SignalEmitStmt))]
public sealed record SignalEmitStmt(Expr Type, Expr Symbol, Expr Value, Expr? Asset,
    Expr? Market, Expr? Timeframe, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SIGNAL EMIT {Type}, {Symbol}, {Value}";
    public override AstNode DeepClone() => new SignalEmitStmt((Expr)Type.DeepClone(),
        (Expr)Symbol.DeepClone(), (Expr)Value.DeepClone(), (Expr?)Asset?.DeepClone(),
        (Expr?)Market?.DeepClone(), (Expr?)Timeframe?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>SIGNAL UNSUBSCRIBE ["id"] (senza argomento = tutte).</summary>
[JsonDerivedType(typeof(SignalUnsubscribeStmt), nameof(SignalUnsubscribeStmt))]
public sealed record SignalUnsubscribeStmt(Expr? Id, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "SIGNAL UNSUBSCRIBE" + (Id is null ? "" : $" {Id}");
    public override AstNode DeepClone() => new SignalUnsubscribeStmt((Expr?)Id?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>SIGNAL PING — keepalive (no-op sul bus locale).</summary>
[JsonDerivedType(typeof(SignalPingStmt), nameof(SignalPingStmt))]
public sealed record SignalPingStmt(Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "SIGNAL PING";
    public override AstNode DeepClone() => new SignalPingStmt((Token)Token.DeepClone());
}

/// <summary>ON SIGNAL "id" CALL|GOSUB|STOP &lt;handler&gt;.</summary>
[JsonDerivedType(typeof(OnSignalStmt), nameof(OnSignalStmt))]
public sealed record OnSignalStmt(Expr? Sub, MessageAction Action, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"ON SIGNAL {Sub?.ToString() ?? "*"} {Action.GetType().Name}";
    public override AstNode DeepClone() => new OnSignalStmt((Expr?)Sub?.DeepClone(), Action.DeepClone(), (Token)Token.DeepClone());
}
