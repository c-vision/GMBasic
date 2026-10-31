// FILE: Interpreter.Parser.Stmt.Workflow.cs

namespace Basic;

public sealed partial class Parser
{
    public static Stmt ParseWorkflowDefStatement(Parser parser, string expectedNamespace)
    {
        // WORKFLOW name — il dispatcher NON consuma il token (chiama il parser col token
        // in input, vedi DispatchRegisteredCommand) → il parser lo consuma da solo.
        parser.Consume(TokenKind.WorkflowStatement, "Expected 'WORKFLOW'.");
        var name = parser.Consume(TokenKind.Identifier, "Expected workflow name after 'WORKFLOW'.");
        return new WorkflowDefStmt(name, name);
    }

    public static Stmt ParseWorkflowStepStatement(Parser parser, string expectedNamespace)
    {
        // STEP name, action [, retry [, timeout]] — il dispatcher NON consuma il token → il parser lo consuma.
        // retry/timeout sono espressioni intere opzionali (0 = default). Syntax: STEP s, "action", 2, 3000
        parser.Consume(TokenKind.Step, "Expected 'STEP'.");
        var stepName = parser.Consume(TokenKind.Identifier, "Expected step name after 'STEP'.");
        parser.Consume(TokenKind.Comma, "Expected ',' after step name.");
        var action = parser.ParseExpression();
        Expr? retry = null;
        Expr? timeout = null;
        if (parser.Check(TokenKind.Comma))
        {
            parser.Advance();
            retry = parser.ParseExpression();
            if (parser.Check(TokenKind.Comma))
            {
                parser.Advance();
                timeout = parser.ParseExpression();
            }
        }
        return new WorkflowStepStmt(stepName, action, retry, timeout, stepName);
    }

    public static Stmt ParseWorkflowExecuteStatement(Parser parser, string expectedNamespace)
    {
        // EXECUTE [WORKFLOW] name — il dispatcher NON consuma EXECUTE → il parser lo consuma.
        parser.Consume(TokenKind.WorkflowExecute, "Expected 'EXECUTE'.");
        if (parser.Check(TokenKind.WorkflowStatement))
            parser.Advance();
        var name = parser.Consume(TokenKind.Identifier, "Expected workflow name after 'EXECUTE'.");
        return new WorkflowExecuteStmt(name, name);
    }
}
