// FILE: Interpreter.Compiler.Compiler.Sql.cs

namespace Compiler;

public sealed partial class Compiler
{
    private void VisitSqlOpenStatement(SqlOpenStmt stmt)
    {
        VisitExpression(stmt.FilePath);
        EmitConstant(stmt.OriginalAliasToken.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_OPEN); EmitByte((byte)2);
        EmitByte((byte)OpCode.OP_POP); // discard return value from statement
    }

    private void VisitSqlCloseStatement(SqlCloseStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_CLOSE); EmitByte((byte)1);
    }

    /// <summary>SQL CLOSE RESULTSET rs — riusa il runtime BuiltInFunction.SQLCLOSERESULTSET.</summary>
    private void VisitSqlCloseResultSetStatement(SqlCloseResultSetStmt stmt)
    {
        VisitExpression(stmt.ResultSet);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.SQLCLOSERESULTSET);
        EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_POP); // scarta il valore di ritorno (1.0): è un comando
    }

    private void VisitSqlExecStatement(SqlExecStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        VisitExpression(stmt.Sql);
        EmitConstant((double)stmt.Parameters.Length);
        foreach (var parameter in stmt.Parameters)
            VisitExpression(parameter);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_EXEC); EmitByte((byte)((3 + stmt.Parameters.Length)));
        EmitByte((byte)OpCode.OP_POP); // discard return value from statement
    }

    private void VisitSqlQueryStatement(SqlQueryStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        VisitExpression(stmt.Sql);
        EmitConstant((double)stmt.Parameters.Length);
        foreach (var parameter in stmt.Parameters)
            VisitExpression(parameter);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_QUERY); EmitByte((byte)((3 + stmt.Parameters.Length)));
        AssignVariable(stmt.ResultSet, isLet: false);
    }

    private void VisitSqlBeginTransactionStatement(SqlBeginTransactionStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_BEGIN_TRANSACTION); EmitByte((byte)1);
    }

    private void VisitSqlCommitStatement(SqlCommitStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_COMMIT); EmitByte((byte)1);
    }

    private void VisitSqlRollbackStatement(SqlRollbackStmt stmt)
    {
        if (stmt.SavepointName is not null)
        {
            EmitAliasOrExpression(stmt.DbAlias);
            VisitExpression(stmt.SavepointName);
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_ROLLBACK_TO_SAVEPOINT); EmitByte((byte)2);
            return;
        }
        EmitAliasOrExpression(stmt.DbAlias);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_ROLLBACK); EmitByte((byte)1);
    }

    private void VisitSqlSavepointStatement(SqlSavepointStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_SAVEPOINT); EmitByte((byte)2);
    }

    private void VisitSqlReleaseStatement(SqlReleaseStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_RELEASE_SAVEPOINT); EmitByte((byte)2);
    }

    private void VisitSqlAttachStatement(SqlAttachStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        VisitExpression(stmt.AttachPath);
        EmitConstant(stmt.SchemaToken.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_ATTACH); EmitByte((byte)3);
    }

    private void VisitSqlDetachStatement(SqlDetachStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);
        VisitExpression(stmt.SchemaName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_DETACH); EmitByte((byte)2);
    }

    /// <summary>Emette un alias SQL come costante stringa se è un VariableExpr,
    /// altrimenti compila l'espressione normalmente.</summary>
    private void EmitAliasOrExpression(Expr expr)
    {
        // Push alias as string constant if it's a simple name reference
        if (expr is VariableExpr varExpr)
            EmitConstant(varExpr.NameCleaned.Lexeme);
        else if (expr is QualifiedNameExpr qn && qn.Parts.Length == 1)
            EmitConstant(qn.Parts[0].Lexeme);
        else
            VisitExpression(expr);
    }

    private void VisitSqlBackupStatement(SqlBackupStmt stmt)
    {
        EmitAliasOrExpression(stmt.DbAlias);   // alias = nome (costante stringa), come DETACH/ATTACH: VisitExpression lo leggeva come variabile
        VisitExpression(stmt.DestinationPath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SQL_BACKUP); EmitByte((byte)2);
    }
}
