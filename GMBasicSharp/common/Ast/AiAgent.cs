// FILE: Interpreter.Ast.AiAgent.cs

namespace Basic;

#region AI AGENT Statements (fuso nel namespace AI)

[JsonDerivedType(typeof(CreateAgentStmt), nameof(CreateAgentStmt))]
public sealed record CreateAgentStmt(Token AgentName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"CREATE AGENT {AgentName.Lexeme}";

    public override AstNode DeepClone() =>
        new CreateAgentStmt((Token)AgentName.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(DeleteAgentStmt), nameof(DeleteAgentStmt))]
public sealed record DeleteAgentStmt(Token AgentName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"DELETE AGENT {AgentName.Lexeme}";

    public override AstNode DeepClone() =>
        new DeleteAgentStmt((Token)AgentName.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SaveAgentStmt), nameof(SaveAgentStmt))]
public sealed record SaveAgentStmt(Token AgentName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SAVE AGENT {AgentName.Lexeme}";

    public override AstNode DeepClone() =>
        new SaveAgentStmt((Token)AgentName.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(LoadAgentStmt), nameof(LoadAgentStmt))]
public sealed record LoadAgentStmt(Token AgentName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"LOAD AGENT {AgentName.Lexeme}";

    public override AstNode DeepClone() =>
        new LoadAgentStmt((Token)AgentName.DeepClone(), (Token)Token.DeepClone());
}

#endregion
