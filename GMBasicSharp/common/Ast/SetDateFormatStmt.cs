// FILE: Basic.Ast.SetDateFormatStmt

namespace Basic;

/// <summary>SET DATE FORMAT TO "fmt"</summary>
[System.Text.Json.Serialization.JsonDerivedType(typeof(SetDateFormatStmt), nameof(SetDateFormatStmt))]
public sealed record SetDateFormatStmt(Expr Format, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SET DATE FORMAT TO {Format}";
    public override AstNode DeepClone() => new SetDateFormatStmt((Expr)Format.DeepClone(), (Token)Token.DeepClone());
}
