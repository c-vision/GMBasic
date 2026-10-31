namespace Basic;

/// <summary>SET legacy Clipper (WRAP/DELIMITERS/ESCAPE/TYPEAHEAD/INTENSITY/EPOCH/DECIMALS/
/// FIXED/PRINTER/DEVICE/MESSAGE) — attributo + valore (ON/OFF o espressione).</summary>
public sealed record SetClipperStmt(string AttributeName, Expr Value, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SET {AttributeName} TO {Value}";
    public override AstNode DeepClone() => new SetClipperStmt(AttributeName, (Expr)Value.DeepClone(), (Token)Token.DeepClone());
}
