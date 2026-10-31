// FILE: Interpreter.Parser.Stmt.Mouse.cs

namespace Basic;

/// <summary>
/// Contiene i metodi di parsing per le istruzioni relative al Mouse.
/// Supporta sia la sintassi legacy (es. MOUSE ON) che qualificata (es. MOUSE.ON()).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi che iniziano con la keyword `MOUSE`.
    /// </summary>
    public static Stmt DispatchMouseStatement(Parser parser, string expectedNamespace)
    {
        // Consuma il token MOUSE (sintassi legacy a spazio).
        var mouseToken = parser.Consume(TokenKind.Mouse, "Expected 'MOUSE'.");

        var subCommand = parser.Peek();

        return ParseMouseSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Parsa MOUSE ON / MOUSE OFF.
    /// </summary>
    public static Stmt ParseMouseConfigStatement(Parser parser, string ns)
    {
        // Il dispatcher ha già verificato il prossimo token
        var stateToken = parser.Advance(); // ON o OFF
        var enabled = stateToken.Kind == TokenKind.On;

        return new MouseConfigStmt(enabled, stateToken);
    }

    /// <summary>
    /// Parsa MOUSE SHOW / MOUSE HIDE.
    /// </summary>
    public static Stmt ParseMouseVisibilityStatement(Parser parser, string ns)
    {
        var stateToken = parser.Advance(); // SHOW o HIDE
        var visible = stateToken.Kind == TokenKind.Show;

        return new MouseVisibilityStmt(visible, stateToken);
    }
    internal static Stmt ParseMouseSubcommand(Parser parser, string expectedNamespace)
    {
        var subCommand = parser.Peek();
        return subCommand.Kind switch
        {
            TokenKind.On or TokenKind.Off => ParseMouseConfigStatement(parser, expectedNamespace),
            TokenKind.Show or TokenKind.Hide => ParseMouseVisibilityStatement(parser, expectedNamespace),
            TokenKind.Set => ParseMouseSetStatement(parser, expectedNamespace),
            TokenKind.Limit => ParseMouseLimitStatement(parser, expectedNamespace),
            TokenKind.Unlimit => ParseMouseLimitStatement(parser, expectedNamespace),
            TokenKind.Cursor => ParseMouseCursorStatement(parser, expectedNamespace),
            TokenKind.Wait => ParseMouseWaitStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommand, $"Sottocomando MOUSE non riconosciuto: '{subCommand.Lexeme}'.")
        };
    }



    /// <summary>
    /// Parsa MOUSE SET x, y.
    /// </summary>
    public static Stmt ParseMouseSetStatement(Parser parser, string ns)
    {
        parser.Consume(TokenKind.Set, "Expected 'SET'.");
        
        Expr x, y;

        // Sintassi unificata (legacy e qualificata fusa): SET x, y
        x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after X.");
        y = parser.ParseExpression();

        return new MouseSetStmt(x, y, parser.Previous());
    }

    /// <summary>
    /// Parsa MOUSE LIMIT x1,y1,x2,y2 oppure MOUSE UNLIMIT.
    /// </summary>
    public static Stmt ParseMouseLimitStatement(Parser parser, string ns)
    {
        // Controlla se è LIMIT o UNLIMIT
        if (parser.Match(TokenKind.Unlimit))
        {
            return new MouseLimitStmt(null, null, null, null, parser.Previous());
        }

        parser.Consume(TokenKind.Limit, "Expected 'LIMIT'.");
        
        Expr x1, y1, x2, y2;

        // Sintassi unificata (legacy e qualificata fusa): LIMIT x1, y1, x2, y2
        x1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        x2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y2 = parser.ParseExpression();

        return new MouseLimitStmt(x1, y1, x2, y2, parser.Previous());
    }

    /// <summary>
    /// Parsa MOUSE CURSOR n.
    /// </summary>
    public static Stmt ParseMouseCursorStatement(Parser parser, string ns)
    {
        parser.Consume(TokenKind.Cursor, "Expected 'CURSOR'.");
        Expr cursorId;

        // Sintassi unificata (legacy e qualificata fusa): CURSOR id
        cursorId = parser.ParseExpression();

        return new MouseCursorStmt(cursorId, parser.Previous());
    }

    /// <summary>
    /// Parsa l'istruzione di attesa del mouse: MOUSE WAIT.
    /// </summary>
    public static Stmt ParseMouseWaitStatement(Parser parser, string ns)
    {
        var waitToken = parser.Consume(TokenKind.Wait, "Expected 'WAIT'.");

        return new MouseWaitStmt(waitToken);
    }
}