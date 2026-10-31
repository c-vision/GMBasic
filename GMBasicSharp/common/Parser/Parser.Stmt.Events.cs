// FILE: Interpreter.Parser.Stmt.Events.cs

namespace Basic;

public sealed partial class Parser
{
    /// <summary>
    /// Parsa l'istruzione RAISE EVENT EventName(args...).
    /// VERSIONE CORRETTA: Usa l'helper ConsumeEventName per accettare
    /// sia identificatori generici che keyword valide come 'Click' come nome dell'evento.
    /// </summary>
    public static Stmt ParseRaiseEventStatement(Parser parser, string? ns)
    {
        var raiseToken = parser.Consume(TokenKind.Raise, "Expected 'RAISE'.");
        parser.Consume(TokenKind.Event, "Expected 'EVENT' after 'RAISE'.");

        // Utilizza l'helper per consumare correttamente il nome dell'evento.
        var eventName = parser.ConsumeEventName(); 
        
        var args = parser.ParseArgumentListInParentheses();

        return new RaiseEventStmt(eventName, args, raiseToken);
    }

    /// <summary>
    /// Parsa l'istruzione ADD HANDLER EventExpr, HandlerExpr.
    /// </summary>
    public static Stmt ParseAddHandlerStatement(Parser parser, string? ns)
    {
        var addToken = parser.Consume(TokenKind.Add, "Expected 'ADD'.");
        parser.Consume(TokenKind.Handler, "Expected 'HANDLER' after 'ADD'.");

        var eventAccess = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after event reference.");
        var handlerExpr = parser.ParseExpression();

        return new AddHandlerStmt(eventAccess, handlerExpr, addToken);
    }

    /// <summary>
    /// Parsa l'istruzione REMOVE HANDLER EventExpr, HandlerExpr.
    /// </summary>
    public static Stmt ParseRemoveHandlerStatement(Parser parser, string? ns)
    {
        var removeToken = parser.Consume(TokenKind.Remove, "Expected 'REMOVE'.");
        parser.Consume(TokenKind.Handler, "Expected 'HANDLER' after 'REMOVE'.");

        var eventAccess = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after event reference.");
        var handlerExpr = parser.ParseExpression();

        return new RemoveHandlerStmt(eventAccess, handlerExpr, removeToken);
    }
}