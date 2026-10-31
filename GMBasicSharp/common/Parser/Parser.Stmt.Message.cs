// FILE: Interpreter.Parser.Stmt.Message.cs
// Parsing del namespace MESSAGE (sintassi a spazio, come WINDOW):
//   MESSAGE CONNECT "<uri>" [AS alias]
//   MESSAGE SUBSCRIBE <target> TOPIC("a.b") | QUEUE("q") [AS "s"]
//   MESSAGE UNSUBSCRIBE ["s"]
//   MESSAGE PUBLISH <target> TOPIC("a.b") DATA(payload)
//   MESSAGE PING [target]  /  MESSAGE CLOSE [target]
using System;

namespace Basic;

public sealed partial class Parser
{
    private static bool Kw(Parser parser, string kw)
        => parser.Peek().Lexeme.Equals(kw, StringComparison.OrdinalIgnoreCase);

    private static Expr ParseParenExpr(Parser parser)
    {
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        var e = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        return e;
    }

    public static List<Stmt> ParseMessageStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Consume(TokenKind.Message, "Expected 'MESSAGE'.");

        if (Kw(parser, "CONNECT"))
        {
            parser.Advance();
            var uri = parser.ParseExpression();
            Target? handle = parser.Match(TokenKind.As) ? parser.ParseTarget() : null;
            return [new MessageConnectStmt(uri, handle, tok)];
        }
        if (Kw(parser, "SUBSCRIBE"))
        {
            parser.Advance();
            var target = parser.ParseExpression();
            Expr? topic = null, queue = null;
            if (Kw(parser, "TOPIC")) { parser.Advance(); topic = ParseParenExpr(parser); }
            if (Kw(parser, "QUEUE")) { parser.Advance(); queue = ParseParenExpr(parser); }
            Expr? alias = parser.Match(TokenKind.As) ? parser.ParseExpression() : null;
            return [new MessageSubscribeStmt(target, topic, queue, alias, tok)];
        }
        if (Kw(parser, "UNSUBSCRIBE"))
        {
            parser.Advance();
            Expr? sub = parser.IsAtEndOfStatement() ? null : parser.ParseExpression();
            return [new MessageUnsubscribeStmt(sub, tok)];
        }
        if (Kw(parser, "PUBLISH"))
        {
            parser.Advance();
            var target = parser.ParseExpression();
            Expr? topic = null;
            if (Kw(parser, "TOPIC")) { parser.Advance(); topic = ParseParenExpr(parser); }
            Expr data;
            if (Kw(parser, "DATA")) { parser.Advance(); data = ParseParenExpr(parser); }
            else data = parser.ParseExpression();
            return [new MessagePublishStmt(target, topic, data, tok)];
        }
        if (Kw(parser, "PING"))
        {
            parser.Advance();
            Expr? t = parser.IsAtEndOfStatement() ? null : parser.ParseExpression();
            return [new MessagePingStmt(t, tok)];
        }
        if (Kw(parser, "CLOSE"))
        {
            parser.Advance();
            Expr? t = parser.IsAtEndOfStatement() ? null : parser.ParseExpression();
            return [new MessageCloseStmt(t, tok)];
        }
        throw CreateError(parser.Peek(), "Expected CONNECT/SUBSCRIBE/UNSUBSCRIBE/PUBLISH/PING/CLOSE after MESSAGE.");
    }

    /// <summary>ON MESSAGE &lt;sub|bus|ANY&gt; CALL proc | GOSUB label | STOP.</summary>
    private OnMessageStmt ParseOnMessageStatement(Token onToken)
    {
        // id opzionale: assente se il token successivo è già un'azione
        Expr? sub = null;
        if (!Kw(this, "CALL") && !Kw(this, "GOSUB") && !Kw(this, "STOP") && !IsAtEndOfStatement())
            sub = ParseExpression();

        MessageAction action;
        if (Match(TokenKind.Call)) action = new MessageCallAction(Consume(TokenKind.Identifier, TokenKind.Handler));
        else if (Match(TokenKind.Gosub)) action = new MessageGosubAction(Consume(TokenKind.Number, TokenKind.Identifier));
        else if (Match(TokenKind.Stop)) action = new MessageStopAction();
        else throw CreateError(Peek(), "Expected CALL, GOSUB or STOP after ON MESSAGE.");

        return new OnMessageStmt(sub, action, onToken);
    }
}
