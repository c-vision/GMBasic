// FILE: Interpreter.Ast.Sql.cs

using System.Collections.Immutable;
using System.Text;

namespace Basic;

#region SQL Statements

[JsonDerivedType(typeof(SqlOpenStmt), nameof(SqlOpenStmt))]
public sealed record SqlOpenStmt(
    Expr FilePath,
    Token OriginalAliasToken,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL OPEN {FilePath} AS {OriginalAliasToken.Lexeme}";

    public override AstNode DeepClone() =>
        new SqlOpenStmt((Expr)FilePath.DeepClone(), (Token)OriginalAliasToken.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlCloseStmt), nameof(SqlCloseStmt))]
public sealed record SqlCloseStmt(Expr DbAlias, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL CLOSE {DbAlias}";

    public override AstNode DeepClone() =>
        new SqlCloseStmt((Expr)DbAlias.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlCloseResultSetStmt), nameof(SqlCloseResultSetStmt))]
public sealed record SqlCloseResultSetStmt(Expr ResultSet, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL CLOSE RESULTSET {ResultSet}";

    public override AstNode DeepClone() =>
        new SqlCloseResultSetStmt((Expr)ResultSet.DeepClone(), (Token)Token.DeepClone());
}


[JsonDerivedType(typeof(SqlExecStmt), nameof(SqlExecStmt))]
public sealed record SqlExecStmt(
    Expr DbAlias,
    Expr Sql,
    ImmutableArray<Expr> Parameters,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString()
    {
        var sb = new StringBuilder($"SQL EXEC {DbAlias}, {Sql}");
        foreach (var p in Parameters) sb.Append(", ").Append(p);
        return sb.ToString();
    }

    public override AstNode DeepClone() =>
        new SqlExecStmt(
            (Expr)DbAlias.DeepClone(),
            (Expr)Sql.DeepClone(),
            Parameters.Select(p => (Expr)p.DeepClone()).ToImmutableArray(),
            (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlQueryStmt), nameof(SqlQueryStmt))]
public sealed record SqlQueryStmt(
    Expr DbAlias,
    Target ResultSet,
    Expr Sql,
    ImmutableArray<Expr> Parameters,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL QUERY {DbAlias}, {ResultSet}, {Sql}";

    public override AstNode DeepClone() =>
        new SqlQueryStmt(
            (Expr)DbAlias.DeepClone(),
            (Target)ResultSet.DeepClone(),
            (Expr)Sql.DeepClone(),
            Parameters.Select(p => (Expr)p.DeepClone()).ToImmutableArray(),
            (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlBeginTransactionStmt), nameof(SqlBeginTransactionStmt))]
public sealed record SqlBeginTransactionStmt(Expr DbAlias, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL BEGIN TRANSACTION {DbAlias}";

    public override AstNode DeepClone() =>
        new SqlBeginTransactionStmt((Expr)DbAlias.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlCommitStmt), nameof(SqlCommitStmt))]
public sealed record SqlCommitStmt(Expr DbAlias, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL COMMIT {DbAlias}";

    public override AstNode DeepClone() =>
        new SqlCommitStmt((Expr)DbAlias.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlRollbackStmt), nameof(SqlRollbackStmt))]
public sealed record SqlRollbackStmt(Expr DbAlias, Token Token, Expr? SavepointName = null) : Stmt(Token.Position)
{
    public override string ToString() => SavepointName is null
        ? $"SQL ROLLBACK {DbAlias}"
        : $"SQL ROLLBACK TO SAVEPOINT {DbAlias}, {SavepointName}";

    public override AstNode DeepClone() =>
        new SqlRollbackStmt((Expr)DbAlias.DeepClone(), (Token)Token.DeepClone(),
            (Expr?)SavepointName?.DeepClone());
}

[JsonDerivedType(typeof(SqlSavepointStmt), nameof(SqlSavepointStmt))]
public sealed record SqlSavepointStmt(Expr DbAlias, Expr Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL SAVEPOINT {DbAlias}, {Name}";

    public override AstNode DeepClone() =>
        new SqlSavepointStmt((Expr)DbAlias.DeepClone(), (Expr)Name.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlReleaseStmt), nameof(SqlReleaseStmt))]
public sealed record SqlReleaseStmt(Expr DbAlias, Expr Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL RELEASE SAVEPOINT {DbAlias}, {Name}";

    public override AstNode DeepClone() =>
        new SqlReleaseStmt((Expr)DbAlias.DeepClone(), (Expr)Name.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlAttachStmt), nameof(SqlAttachStmt))]
public sealed record SqlAttachStmt(
    Expr DbAlias,
    Expr AttachPath,
    Token SchemaToken,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() =>
        $"SQL ATTACH {DbAlias}, {AttachPath} AS {SchemaToken.Lexeme}";

    public override AstNode DeepClone() =>
        new SqlAttachStmt(
            (Expr)DbAlias.DeepClone(),
            (Expr)AttachPath.DeepClone(),
            (Token)SchemaToken.DeepClone(),
            (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlDetachStmt), nameof(SqlDetachStmt))]
public sealed record SqlDetachStmt(Expr DbAlias, Expr SchemaName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL DETACH {DbAlias}, {SchemaName}";

    public override AstNode DeepClone() =>
        new SqlDetachStmt(
            (Expr)DbAlias.DeepClone(),
            (Expr)SchemaName.DeepClone(),
            (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(SqlBackupStmt), nameof(SqlBackupStmt))]
public sealed record SqlBackupStmt(Expr DbAlias, Expr DestinationPath, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SQL BACKUP {DbAlias} TO {DestinationPath}";

    public override AstNode DeepClone() =>
        new SqlBackupStmt(
            (Expr)DbAlias.DeepClone(),
            (Expr)DestinationPath.DeepClone(),
            (Token)Token.DeepClone());
}

#endregion
