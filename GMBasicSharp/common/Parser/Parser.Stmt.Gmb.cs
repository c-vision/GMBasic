// FILE: Basic.Parser.Parser.Stmt.Gmb.cs

using System.Collections.Immutable;

namespace Basic;

public partial class Parser
{
    /// <summary>`GMB MODULE REGISTER "slot", "dylib"` | `PRIORITY "slot", pri` | `LIST` |
    /// `SYM "slot", "simbolo"` — registry di moduli esterni (model compiler §4.3). Comando legacy
    /// a spazio: il verbo diventa un BuiltinFunctionCallExpr (ns "GMB", TokenKind.GmbModule) che
    /// ogni backend risolve al proprio runtime (mai un no-op). Termini non quotati = stringhe.</summary>
    public static Stmt ParseGmbStatement(Parser parser, string expectedNamespace)
    {
        _ = expectedNamespace;
        var gmb = parser.Consume(TokenKind.Gmb, "Expected 'GMB'.");
        var verb = parser.Peek();
        bool isModule = verb.Kind == TokenKind.LlmModule
            || (verb.Kind == TokenKind.Identifier && verb.Lexeme.Equals("MODULE", System.StringComparison.OrdinalIgnoreCase));
        if (!isModule) throw CreateError(verb, "Expected 'MODULE' after 'GMB'.");
        parser.Advance();
        var sub = parser.Peek();
        if (sub.Kind is TokenKind.Newline or TokenKind.Eof)
            throw CreateError(sub, "Expected REGISTER/PRIORITY/LIST/SYM after 'GMB MODULE'.");
        string subName = sub.Lexeme;
        parser.Advance();
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(new StringExpr(subName, sub));
        while (!parser.IsAtEndOfStatement() && !parser.Check(TokenKind.Eof))
        {
            if (parser.Match(TokenKind.Comma)) continue;
            var before = parser.Peek().Position;
            args.Add(ParseAiModelArg(parser));
            if (parser.Peek().Position == before) break;
        }
        var nsTok = new Token(TokenKind.Identifier, "GMB", null, gmb.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.GmbModule, "MODULE", null, gmb.Position), args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }
}
