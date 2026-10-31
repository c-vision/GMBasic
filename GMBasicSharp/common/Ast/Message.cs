// FILE: Interpreter.Ast.Message.cs
// Messaging generico (namespace MESSAGE) + evento ON MESSAGE.
// Handler dedicato (MessageAction), NON il modello OnKeyAction di KEY/WINDOW.
using System.Collections.Immutable;

namespace Basic;

/// <summary>MESSAGE CONNECT "&lt;uri&gt;" [AS alias].</summary>
[JsonDerivedType(typeof(MessageConnectStmt), nameof(MessageConnectStmt))]
public sealed record MessageConnectStmt(Expr Uri, Target? Handle, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"MESSAGE CONNECT {Uri}" + (Handle is null ? "" : " AS (target)");
    public override AstNode DeepClone() => new MessageConnectStmt((Expr)Uri.DeepClone(), (Target?)Handle?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>MESSAGE SUBSCRIBE &lt;target&gt; TOPIC("a.b") | QUEUE("q") [AS "s"].</summary>
[JsonDerivedType(typeof(MessageSubscribeStmt), nameof(MessageSubscribeStmt))]
public sealed record MessageSubscribeStmt(Expr Target, Expr? Topic, Expr? Queue, Expr? Alias, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"MESSAGE SUBSCRIBE {Target}" + (Topic is null ? "" : $" TOPIC({Topic})") + (Queue is null ? "" : $" QUEUE({Queue})");
    public override AstNode DeepClone() => new MessageSubscribeStmt((Expr)Target.DeepClone(), (Expr?)Topic?.DeepClone(), (Expr?)Queue?.DeepClone(), (Expr?)Alias?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>MESSAGE UNSUBSCRIBE ["s"]  (senza argomento = tutte).</summary>
[JsonDerivedType(typeof(MessageUnsubscribeStmt), nameof(MessageUnsubscribeStmt))]
public sealed record MessageUnsubscribeStmt(Expr? Sub, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "MESSAGE UNSUBSCRIBE" + (Sub is null ? "" : $" {Sub}");
    public override AstNode DeepClone() => new MessageUnsubscribeStmt((Expr?)Sub?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>MESSAGE PUBLISH &lt;target&gt; TOPIC("a.b") DATA(payload).</summary>
[JsonDerivedType(typeof(MessagePublishStmt), nameof(MessagePublishStmt))]
public sealed record MessagePublishStmt(Expr Target, Expr? Topic, Expr Data, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"MESSAGE PUBLISH {Target} TOPIC({Topic}) DATA({Data})";
    public override AstNode DeepClone() => new MessagePublishStmt((Expr)Target.DeepClone(), (Expr?)Topic?.DeepClone(), (Expr)Data.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>MESSAGE PING [target].</summary>
[JsonDerivedType(typeof(MessagePingStmt), nameof(MessagePingStmt))]
public sealed record MessagePingStmt(Expr? Target, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "MESSAGE PING" + (Target is null ? "" : $" {Target}");
    public override AstNode DeepClone() => new MessagePingStmt((Expr?)Target?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>MESSAGE CLOSE [target].</summary>
[JsonDerivedType(typeof(MessageCloseStmt), nameof(MessageCloseStmt))]
public sealed record MessageCloseStmt(Expr? Target, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => "MESSAGE CLOSE" + (Target is null ? "" : $" {Target}");
    public override AstNode DeepClone() => new MessageCloseStmt((Expr?)Target?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Azione di un handler ON MESSAGE (dedicata, non OnKeyAction).</summary>
public abstract record MessageAction
{
    public abstract MessageAction DeepClone();
}
public sealed record MessageCallAction(Token Handler) : MessageAction
{
    public override MessageAction DeepClone() => new MessageCallAction((Token)Handler.DeepClone());
}
public sealed record MessageGosubAction(Token TargetLabel) : MessageAction
{
    public override MessageAction DeepClone() => new MessageGosubAction((Token)TargetLabel.DeepClone());
}
public sealed record MessageStopAction : MessageAction
{
    public override MessageAction DeepClone() => this with { };
}

/// <summary>ON MESSAGE &lt;sub|bus|ANY&gt; CALL|GOSUB|STOP.</summary>
[JsonDerivedType(typeof(OnMessageStmt), nameof(OnMessageStmt))]
public sealed record OnMessageStmt(Expr? Sub, MessageAction Action, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"ON MESSAGE {Sub?.ToString() ?? "ANY"} {Action.GetType().Name}";
    public override AstNode DeepClone() => new OnMessageStmt((Expr?)Sub?.DeepClone(), Action.DeepClone(), (Token)Token.DeepClone());
}
