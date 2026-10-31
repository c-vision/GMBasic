// FILE: Basic.Parser.Parser.Stmt.Ai.Finetune.cs — AI FINETUNE / AI LORA APPLY
// (superficie MLX LoRA — vedi Piani/(proposal) - MLX-Finetune-Superficie-BASIC.md).

using System.Collections.Immutable;

namespace Basic;

public partial class Parser
{
    /// <summary>
    /// Legacy (comando, mai parentesi): AI FINETUNE "dirModelo" [, { ... }]
    /// Avvia il fine-tune LoRA del modello MLX locale. Il dict di opzioni è
    /// opzionale; passato a AI.FINETUNE(model, opts).
    /// </summary>
    private static Stmt ParseAiFinetuneStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubFinetune, "Expected 'FINETUNE' after 'AI'.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        var first = parser.ParseExpression();
        if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
            first = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        args.Add(first);
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiFinetune, "FINETUNE", null, tok.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>
    /// Legacy (comando, mai parentesi): AI LORA APPLY "ckptPrefisso" [, { ... }]
    /// Verifica il deploy di un adapter addestrato (ri-carica A/B dal checkpoint in
    /// una sessione nativa e confronta la generazione OFF vs ON su un prompt dal
    /// dataset). Il dict di opzioni { "model":..., "dataset":... } è opzionale.
    /// </summary>
    private static Stmt ParseAiLoraStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubLora, "Expected 'LORA' after 'AI'.");
        parser.Consume(TokenKind.AiSubLoraApply, "Expected 'APPLY' after 'AI LORA'.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        var first = parser.ParseExpression();
        if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
            first = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        args.Add(first);
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiLoraApply, "LORAAPPLY", null, tok.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }
}
