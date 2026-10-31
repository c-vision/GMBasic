// FILE: Interpreter.Parser.Stmt.Sql.cs

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    private static Stmt ParseSqlStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Sql, "Internal parser error: Expected 'SQL' keyword.");
        var commandToken = parser.Peek();

        return ParseSqlSubcommand(parser, expectedNamespace);
    }

    public static SqlOpenStmt ParseSqlOpenStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Open, "Expected 'OPEN'.");
        Expr filePath;
        Token aliasToken;

        // Sintassi unificata (legacy e qualificata fusa): OPEN "file" AS alias
        filePath = parser.ParseExpression();
        if (!parser.Match(TokenKind.As) && !parser.Match(TokenKind.Alias))
            throw CreateError(parser.Peek(), "Expected 'AS' or 'ALIAS' after file path in SQL OPEN.");
        aliasToken = parser.Consume(TokenKind.Identifier, "Expected database alias after AS/ALIAS.");

        return new SqlOpenStmt(filePath, aliasToken, aliasToken);
    }

    public static SqlCloseStmt ParseSqlCloseStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Close, "Expected 'CLOSE'.");
        var dbAlias = ParseSqlSingleArg(parser);
        return new SqlCloseStmt(dbAlias, parser.Previous());
    }

    /// <summary>Dispatcher per SQL CLOSE: distingue SQL CLOSE RESULTSET rs da SQL CLOSE db.</summary>
    private static Stmt ParseSqlCloseDispatcher(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Close, "Expected 'CLOSE'.");
        // SQL CLOSE RESULTSET rs  vs  SQL CLOSE dbAlias
        if (parser.Match(TokenKind.SqlResultSet) || parser.Match(TokenKind.ResultSet))
        {
            Expr rs;
            // Sintassi unificata (legacy e qualificata fusa): CLOSE RESULTSET rs
            rs = parser.ParseExpression();
            return new SqlCloseResultSetStmt(rs, parser.Previous());
        }
        return new SqlCloseStmt(parser.ParseExpression(), parser.Previous());
    }
    internal static Stmt ParseSqlSubcommand(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            TokenKind.Open => ParseSqlOpenStatement(parser, expectedNamespace),
            TokenKind.Close => ParseSqlCloseDispatcher(parser, expectedNamespace),
            TokenKind.SqlExec => ParseSqlExecStatement(parser, expectedNamespace),
            TokenKind.SqlQuery => ParseSqlQueryStatement(parser, expectedNamespace),
            TokenKind.Begin => ParseSqlBeginTransactionStatement(parser, expectedNamespace),
            TokenKind.Commit => ParseSqlCommitStatement(parser, expectedNamespace),
            TokenKind.Rollback => ParseSqlRollbackStatement(parser, expectedNamespace),
            TokenKind.Savepoint => ParseSqlSavepointStatement(parser, expectedNamespace),
            TokenKind.Release => ParseSqlReleaseStatement(parser, expectedNamespace),
            TokenKind.SqlAttach => ParseSqlAttachStatement(parser, expectedNamespace),
            TokenKind.SqlDetach => ParseSqlDetachStatement(parser, expectedNamespace),
            TokenKind.Backup => ParseSqlBackupStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken, $"Comando '{commandToken.Lexeme}' non riconosciuto per il motore SQL.")
        };
    }



    public static SqlExecStmt ParseSqlExecStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.SqlExec, "Expected 'EXEC'.");
        var (dbAlias, sql, parameters) = ParseSqlDbSqlAndParams(parser);
        return new SqlExecStmt(dbAlias, sql, parameters, parser.Previous());
    }

    public static SqlQueryStmt ParseSqlQueryStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.SqlQuery, "Expected 'QUERY'.");
        Expr dbAlias;
        Target resultSet;
        Expr sql;
        ImmutableArray<Expr> parameters;

        // Sintassi unificata (legacy e qualificata fusa): QUERY dbAlias, rs, sql [, params]
        dbAlias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after database alias.");
        resultSet = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after result set variable.");
        sql = parser.ParseExpression();
        parameters = ParseSqlTrailingParamsLegacy(parser);

        return new SqlQueryStmt(dbAlias, resultSet, sql, parameters, parser.Previous());
    }

    public static SqlBeginTransactionStmt ParseSqlBeginTransactionStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: BEGIN TRANSACTION db
        parser.Consume(TokenKind.Begin, "Expected 'BEGIN'.");
        parser.Consume(TokenKind.Transaction, "Expected 'TRANSACTION' after 'BEGIN'.");

        var dbAlias = parser.ParseExpression();
        return new SqlBeginTransactionStmt(dbAlias, parser.Previous());
    }

    public static SqlCommitStmt ParseSqlCommitStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Commit, "Expected 'COMMIT'.");
        var dbAlias = ParseSqlSingleArg(parser);
        return new SqlCommitStmt(dbAlias, parser.Previous());
    }

    public static SqlRollbackStmt ParseSqlRollbackStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Rollback, "Expected 'ROLLBACK'.");
        // SQL ROLLBACK db  |  SQL ROLLBACK [TO] SAVEPOINT db, "name" (rollback parziale).
        if (parser.Match(TokenKind.To))
        {
            parser.Match(TokenKind.Savepoint);
            var rDb = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after database alias in ROLLBACK TO SAVEPOINT.");
            var rName = parser.ParseExpression();
            return new SqlRollbackStmt(rDb, parser.Previous(), rName);
        }
        var dbAlias = ParseSqlSingleArg(parser);
        return new SqlRollbackStmt(dbAlias, parser.Previous());
    }

    public static SqlSavepointStmt ParseSqlSavepointStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Savepoint, "Expected 'SAVEPOINT'.");
        var dbAlias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after database alias in SQL SAVEPOINT.");
        var name = parser.ParseExpression();
        return new SqlSavepointStmt(dbAlias, name, parser.Previous());
    }

    public static SqlReleaseStmt ParseSqlReleaseStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Release, "Expected 'RELEASE'.");
        parser.Match(TokenKind.Savepoint); // "RELEASE SAVEPOINT db, name"
        var dbAlias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after database alias in SQL RELEASE SAVEPOINT.");
        var name = parser.ParseExpression();
        return new SqlReleaseStmt(dbAlias, name, parser.Previous());
    }

    public static SqlAttachStmt ParseSqlAttachStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.SqlAttach, "Expected 'ATTACH'.");
        Expr dbAlias;
        Expr attachPath;
        Token schemaToken;

        // Sintassi unificata (legacy e qualificata fusa): ATTACH dbAlias, path AS schema
        dbAlias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after database alias.");
        attachPath = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' after attach path.");
        schemaToken = ParseSqlSchemaToken(parser);

        return new SqlAttachStmt(dbAlias, attachPath, schemaToken, schemaToken);
    }

    public static SqlDetachStmt ParseSqlDetachStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.SqlDetach, "Expected 'DETACH'.");
        Expr dbAlias;
        Expr schemaName;

        // Sintassi unificata (legacy e qualificata fusa): DETACH dbAlias, schemaName
        dbAlias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after database alias.");
        schemaName = parser.ParseExpression();

        return new SqlDetachStmt(dbAlias, schemaName, parser.Previous());
    }

    public static SqlBackupStmt ParseSqlBackupStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Backup, "Expected 'BACKUP'.");
        Expr dbAlias;
        Expr destination;

        // Sintassi unificata (legacy e qualificata fusa): BACKUP dbAlias TO destination
        dbAlias = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after database alias in SQL BACKUP.");
        destination = parser.ParseExpression();

        return new SqlBackupStmt(dbAlias, destination, parser.Previous());
    }

    private static (Expr DbAlias, Expr Sql, ImmutableArray<Expr> Parameters) ParseSqlDbSqlAndParams(
        Parser parser)
    {
        // Sintassi unificata (legacy e qualificata fusa): EXEC dbAlias, sql [, params]
        var dbAlias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after database alias.");
        var sql = parser.ParseExpression();
        var parameters = ParseSqlTrailingParamsLegacy(parser);
        return (dbAlias, sql, parameters);
    }

    private static Expr ParseSqlSingleArg(Parser parser, bool consumeOpenParen = true)
    {
        // Sintassi unificata (legacy e qualificata fusa): un solo argomento, senza parentesi
        return parser.ParseExpression();
    }

    private static ImmutableArray<Expr> ParseSqlTrailingParamsLegacy(Parser parser)
    {
        if (parser.IsAtEndOfStatement())
            return ImmutableArray<Expr>.Empty;
        var builder = ImmutableArray.CreateBuilder<Expr>();
        while (parser.Match(TokenKind.Comma) && !parser.IsAtEndOfStatement())
            builder.Add(parser.ParseExpression());
        return builder.ToImmutable();
    }

    private static Token ParseSqlSchemaToken(Parser parser)
    {
        if (parser.Check(TokenKind.Identifier))
            return parser.Advance();
        var expr = parser.ParseExpression() as StringExpr
                   ?? throw CreateError(parser.Previous(), "Schema name must be a string or identifier.");
        return new Token(TokenKind.Identifier, expr.Value, null, expr.Position);
    }
}
