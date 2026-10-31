
// FILE: Basic.Ast.SetThemeStmt

namespace Basic;

/// <summary>SET THEME TO "name"</summary>
[System.Text.Json.Serialization.JsonDerivedType(typeof(SetThemeStmt), nameof(SetThemeStmt))]
public sealed record SetThemeStmt(Expr ThemeName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SET THEME TO {ThemeName}";
    public override AstNode DeepClone() => new SetThemeStmt((Expr)ThemeName.DeepClone(), (Token)Token.DeepClone());
}
