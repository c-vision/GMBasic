// FILE: Interpreter.Parser.Stmt.Goto.cs
// ReSharper disable InvalidXmlDocComment
namespace Basic;
/// <summary>
///     Contiene i metodi per il parsing delle singole istruzioni, definizioni e clausole del linguaggio BASIC.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Parsa un'istruzione RESUME [NEXT | numero_linea].
    /// Consuma autonomamente il token 'RESUME'.
    /// </summary>
    /// <returns>Un nodo <see cref="ResumeStmt"/>.</returns>
    private ResumeStmt ParseResumeStatement()
    {
        var resumeToken = Consume(TokenKind.Resume, "Internal parser error: Expected 'RESUME'.");
        Token? targetToken = null;
        // RESUME accetta NEXT (ritenta), un numero (riga) o un'etichetta (Identifier),
        // come GOTO: 'RESUME ProssimoTest' riprende dall'etichetta indicata.
        if (Match(TokenKind.Next, TokenKind.Number, TokenKind.Identifier))
        {
            targetToken = Previous();
        }

        return new ResumeStmt(resumeToken, targetToken);
    }
    
    /// <summary>
    /// Parsa un'istruzione GOTO.
    /// Ora consuma autonomamente il token 'GOTO'.
    /// </summary>
    /// <returns>Un nodo <see cref="GotoStmt"/>.</returns>
    private GotoStmt ParseGotoStatement()
    {
        var gotoToken = Consume(TokenKind.Goto, "Internal parser error: Expected 'GOTO'.");
        Log($"[PARSER.ParseGotoStatement] Rilevato GOTO alla riga {gotoToken.Line}:{gotoToken.Column}.");
        var targetToken = Consume(TokenKind.Number, TokenKind.Identifier);
        Log($"[PARSER.ParseStatement]     Destinazione GOTO: {targetToken.Lexeme}.");
        return new GotoStmt(targetToken, gotoToken);
    }

    /// <summary>
    /// Parsa un'istruzione GOSUB.
    /// Ora consuma autonomamente il token 'GOSUB'.
    /// </summary>
    /// <returns>Un nodo <see cref="GosubStmt"/>.</returns>
    private GosubStmt ParseGosubStatement()
    {
        var gosubToken = Consume(TokenKind.Gosub, "Internal parser error: Expected 'GOSUB'.");
        Log($"[PARSER.ParseStatement]   Rilevato GOSUB alla riga {gosubToken.Line}:{gosubToken.Column}.");
        var targetToken = Consume(TokenKind.Number, TokenKind.Identifier);
        Log($"[PARSER.ParseStatement]     Destinazione GOSUB: {targetToken.Lexeme}.");
        return new GosubStmt(targetToken, gosubToken);
    }

    /// Parsa un'istruzione RETURN.
    /// Ora consuma autonomamente il token 'RETURN' e gestisce un valore di ritorno opzionale.
    /// </summary>
    /// <returns>Un nodo <see cref="ReturnStmt"/>.</returns>
    private ReturnStmt ParseReturnStatement()
    {
        var returnToken = Consume(TokenKind.Return, "Internal parser error: Expected 'RETURN'.");
        Log($"[PARSER.ParseReturnStatement] Rilevato RETURN alla riga {returnToken.Line}:{returnToken.Column}.");

        Expr? returnValue = null;
        if (!Check(TokenKind.Newline) && !Check(TokenKind.Colon) && !IsAtEnd())
        {
            returnValue = ParseExpression();
        }

        return new ReturnStmt(returnValue, returnToken);
    }
    
}