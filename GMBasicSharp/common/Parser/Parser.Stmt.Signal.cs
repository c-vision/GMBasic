// FILE: Interpreter.Parser.Stmt.Signal.cs
// Namespace SIGNAL (sintassi a spazio, come MESSAGE):
//   SIGNAL SUBSCRIBE [TYPES("a","b")] [ASSET("..")] [MARKET("..")] [SYMBOLS("a","b")]
//                    [TIMEFRAME("h1")] AS "id"
//   SIGNAL EMIT "tipo", "simbolo", valore [, ASSET("..")] [, MARKET("..")] [, TIMEFRAME("..")]
//   SIGNAL UNSUBSCRIBE ["id"]   /   SIGNAL PING
//   ON SIGNAL "id" CALL|GOSUB|STOP <handler>
// TYPES/SYMBOLS accettano una lista di stringhe COSTANTI, piegata in un CSV a parse-time
// (stessa regola della leg C): un filtro costruito a runtime non avrebbe parità fra i backend.
using System;
using System.Collections.Generic;
using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    /// <summary>Parser del comando SIGNAL (registrato come comando per NOME, come CONTROL).</summary>
    public static Stmt ParseSignalStatement(Parser parser, string? expectedNamespace)
    {
        var tok = parser.Peek();
        parser.Advance(); // SIGNAL

        if (!parser.Peek().Lexeme.Equals("SUBSCRIBE", StringComparison.OrdinalIgnoreCase) &&
            !parser.Peek().Lexeme.Equals("EMIT", StringComparison.OrdinalIgnoreCase) &&
            !parser.Peek().Lexeme.Equals("UNSUBSCRIBE", StringComparison.OrdinalIgnoreCase) &&
            !parser.Peek().Lexeme.Equals("PING", StringComparison.OrdinalIgnoreCase))
            throw CreateError(parser.Peek(), "Expected SUBSCRIBE/EMIT/UNSUBSCRIBE/PING after SIGNAL.");

        var verb = parser.Peek().Lexeme.ToUpperInvariant();
        parser.Advance();

        if (verb == "SUBSCRIBE")
        {
            Expr? types = null, asset = null, market = null, symbols = null, timeframe = null;
            if (Kw(parser, "TYPES")) { parser.Advance(); types = ParseSignalClause(parser); }
            if (Kw(parser, "ASSET")) { parser.Advance(); asset = ParseSignalClause(parser); }
            if (Kw(parser, "MARKET")) { parser.Advance(); market = ParseSignalClause(parser); }
            if (Kw(parser, "SYMBOLS")) { parser.Advance(); symbols = ParseSignalClause(parser); }
            if (Kw(parser, "TIMEFRAME")) { parser.Advance(); timeframe = ParseSignalClause(parser); }
            if (!parser.Match(TokenKind.As))
                throw CreateError(parser.Peek(), "Expected AS \"id\" after SIGNAL SUBSCRIBE filters.");
            var id = parser.ParseExpression();
            return new SignalSubscribeStmt(id, types, asset, market, symbols, timeframe, tok);
        }

        if (verb == "EMIT")
        {
            var type = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after the signal type.");
            var symbol = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after the symbol.");
            var value = parser.ParseExpression();
            parser.Match(TokenKind.Comma);
            Expr? asset = null, market = null, timeframe = null;
            if (Kw(parser, "ASSET")) { parser.Advance(); asset = ParseSignalClause(parser); }
            if (Kw(parser, "MARKET")) { parser.Advance(); market = ParseSignalClause(parser); }
            if (Kw(parser, "TIMEFRAME")) { parser.Advance(); timeframe = ParseSignalClause(parser); }
            return new SignalEmitStmt(type, symbol, value, asset, market, timeframe, tok);
        }

        if (verb == "UNSUBSCRIBE")
        {
            Expr? id = parser.IsAtEndOfStatement() ? null : parser.ParseExpression();
            return new SignalUnsubscribeStmt(id, tok);
        }

        return new SignalPingStmt(tok);
    }

    /// <summary>
    /// Clausola di filtro: <c>(...)</c> = lista di stringhe costanti piegata in CSV
    /// ("a,b"); un solo argomento non costante resta un'espressione (CSV dinamico).
    /// </summary>
    private static Expr ParseSignalClause(Parser parser)
    {
        if (!parser.Check(TokenKind.LeftParen)) return parser.ParseExpression();
        var open = parser.Peek();
        parser.Advance();

        var parts = new List<string>();
        int count = 0;
        while (!parser.Check(TokenKind.RightParen) && !parser.IsAtEndOfStatement())
        {
            var arg = parser.ParseExpression();
            if (arg is not StringExpr se)
            {
                if (count == 0 && parser.Check(TokenKind.RightParen))
                {
                    // Unico argomento non costante: CSV dinamico (valore dell'espressione).
                    parser.Advance();
                    return arg;
                }
                throw CreateError(parser.Peek(), "SIGNAL: TYPES/SYMBOLS accept only constant strings.");
            }
            parts.Add(se.Value);
            count++;
            if (!parser.Match(TokenKind.Comma)) break;
        }
        parser.Consume(TokenKind.RightParen, "Expected ')' after the signal filter list.");
        return new StringExpr(string.Join(",", parts), open);
    }

    /// <summary>ON SIGNAL "id" CALL|GOSUB|STOP &lt;handler&gt;.</summary>
    private OnSignalStmt ParseOnSignalStatement(Token onToken)
    {
        Expr? sub = null;
        if (!Kw(this, "CALL") && !Kw(this, "GOSUB") && !Kw(this, "STOP") && !IsAtEndOfStatement())
            sub = ParseExpression();

        MessageAction action;
        if (Match(TokenKind.Call)) action = new MessageCallAction(Consume(TokenKind.Identifier, TokenKind.Handler));
        else if (Match(TokenKind.Gosub)) action = new MessageGosubAction(Consume(TokenKind.Number, TokenKind.Identifier));
        else if (Match(TokenKind.Stop)) action = new MessageStopAction();
        else throw CreateError(Peek(), "Expected CALL, GOSUB or STOP after ON SIGNAL.");

        return new OnSignalStmt(sub, action, onToken);
    }
}
