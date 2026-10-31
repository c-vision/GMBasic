// FILE: Basic.Parser.Parser.Stmt.Ai.Distill.cs — AI DISTILL (Stream D: self-distillation).
// Comando legacy (mai parentesi): AI DISTILL BUILD "jsonl" [, {opts}] | AI DISTILL SAMPLE "prompts.jsonl" [, {opts}]
// Il verbo DISTILL lexa su TokenKind.Distill (già usato da TEAM DISTILL: il contesto dopo 'AI'
// disambigua). I verbi BUILD/SAMPLE/EVAL lexano su TokenKind.Build/Sample/Eval. Fase D1+D4:
// BUILD + SAMPLE + EVAL; TRAIN/APPLY/RESTORE sono fasi successive (errore esplicito se richieste ora).

using System.Collections.Immutable;

namespace Basic;

public partial class Parser
{
    private static Stmt ParseAiDistillStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Distill, "Expected 'DISTILL' after 'AI'.");
        var verb = parser.Peek();
        TokenKind? callKind = verb.Kind switch
        {
            TokenKind.Build => TokenKind.AiDistillBuild,
            TokenKind.Sample => TokenKind.AiDistillSample,
            TokenKind.Eval => TokenKind.AiDistillEval,
            TokenKind.AiSubLoraApply => TokenKind.AiDistillApply,
            TokenKind.Restore => TokenKind.AiDistillRestore,
            TokenKind.Train => TokenKind.AiDistillTrain,
            _ => null,
        };
        if (callKind is null)
            throw Parser.CreateError(verb,
                "Atteso BUILD, SAMPLE, EVAL, APPLY, RESTORE o TRAIN dopo 'AI DISTILL'.");
        parser.Advance();   // verbo (BUILD/SAMPLE/EVAL)
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(callKind.Value, "DISTILL" + verb.Lexeme, null, tok.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }
}
