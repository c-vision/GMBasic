// FILE: Ast.Workflow
using System.Collections.Immutable;
using System.Text.Json.Serialization;

namespace Basic;

/// <summary>WORKFLOW name — inizia definizione workflow.</summary>
[JsonDerivedType(typeof(WorkflowDefStmt), nameof(WorkflowDefStmt))]
public sealed record WorkflowDefStmt(Token WorkflowName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new WorkflowDefStmt((Token)WorkflowName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>STEP name, action$ — definisce uno step del workflow corrente.</summary>
[JsonDerivedType(typeof(WorkflowStepStmt), nameof(WorkflowStepStmt))]
public sealed record WorkflowStepStmt(Token StepName, Expr Action, Expr? RetryCount, Expr? TimeoutMs, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new WorkflowStepStmt(
            (Token)StepName.DeepClone(), (Expr)Action.DeepClone(),
            (Expr?)RetryCount?.DeepClone(), (Expr?)TimeoutMs?.DeepClone(),
            (Token)Token.DeepClone());
}

/// <summary>END WORKFLOW — chiude definizione workflow.</summary>
[JsonDerivedType(typeof(WorkflowEndStmt), nameof(WorkflowEndStmt))]
public sealed record WorkflowEndStmt(Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new WorkflowEndStmt((Token)Token.DeepClone());
}

/// <summary>EXECUTE WORKFLOW name — esegue workflow.</summary>
[JsonDerivedType(typeof(WorkflowExecuteStmt), nameof(WorkflowExecuteStmt))]
public sealed record WorkflowExecuteStmt(Token WorkflowName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new WorkflowExecuteStmt((Token)WorkflowName.DeepClone(), (Token)Token.DeepClone());
}
