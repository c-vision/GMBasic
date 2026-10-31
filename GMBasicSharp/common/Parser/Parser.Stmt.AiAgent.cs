// FILE: Interpreter.Parser.Stmt.AiAgent.cs

namespace Basic;

public sealed partial class Parser
{
    public static CreateAgentStmt ParseAiAgentCreateStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Create, "Expected 'CREATE'.");
        parser.Consume(TokenKind.Agent, "Expected 'AGENT' after 'CREATE'.");
        var agentName = parser.Consume(TokenKind.Identifier, "Expected agent name after 'CREATE AGENT'.");
        return new CreateAgentStmt(agentName, agentName);
    }

    public static DeleteAgentStmt ParseAiAgentDeleteStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Delete, "Expected 'DELETE'.");
        parser.Consume(TokenKind.Agent, "Expected 'AGENT' after 'DELETE'.");
        var agentName = parser.Consume(TokenKind.Identifier, "Expected agent name after 'DELETE AGENT'.");
        return new DeleteAgentStmt(agentName, agentName);
    }

    public static SaveAgentStmt ParseAiAgentSaveStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Save, "Expected 'SAVE'.");
        parser.Consume(TokenKind.Agent, "Expected 'AGENT' after 'SAVE'.");
        var agentName = parser.Consume(TokenKind.Identifier, "Expected agent name after 'SAVE AGENT'.");
        return new SaveAgentStmt(agentName, agentName);
    }

    public static LoadAgentStmt ParseAiAgentLoadStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Load, "Expected 'LOAD'.");
        parser.Consume(TokenKind.Agent, "Expected 'AGENT' after 'LOAD'.");
        var agentName = parser.Consume(TokenKind.Identifier, "Expected agent name after 'LOAD AGENT'.");
        return new LoadAgentStmt(agentName, agentName);
    }
}
