// FILE: Interpreter.Compiler.Compiler.Rdd.cs

// ReSharper disable UseUtf8StringLiteral



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per le istruzioni RDD (Record Description
/// Driver) — strato generalizzato sopra i driver DBF/ISAM/SPEED/SQL.
/// Separato da Compiler.Dbf.cs (RDD è un dominio proprio, non DBF).
/// </summary>
public sealed partial class Compiler
{

    private void VisitRddSetDefaultStatement(RddSetDefaultStmt stmt)
    {
        VisitExpression(stmt.DriverName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_SET_DEFAULT); EmitByte((byte)1);
    }

    private void VisitRddUseStatement(RddUseStmt stmt)
    {
        // Arg 1: file name
        VisitExpression(stmt.FileName);
        // Arg 2: driver name
        VisitExpression(stmt.Driver);
        // Arg 3: alias (or NIL)
        if (stmt.AliasToken != null)
            EmitConstant(stmt.AliasToken!.Lexeme);
        else
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_USE); EmitByte((byte)3);
        EmitByte((byte)OpCode.OP_POP); // discard area number
    }

    private void VisitRddLockStatement(RddLockStmt stmt)
    {
        // Push area (or NIL for current)
        if (stmt.AreaToken != null)
            EmitConstant(stmt.AreaToken.Lexeme);
        else
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        SysCall syscall = stmt.Kind switch
        {
            RddLockKind.RLock => SysCall.RDD_RLOCK,
            RddLockKind.FLock => SysCall.RDD_FLOCK,
            RddLockKind.Unlock => SysCall.RDD_UNLOCK,
            _ => SysCall.RDD_RLOCK
        };
        // GM-Basic: OP_SYSCALL usa EmitSysCall (U16 big-endian), NON un byte singolo —
        // EmitBytes((byte)syscall, 1) produceva [OP_SYSCALL][syscall][argCount] (3 byte)
        // ma il dispatcher legge [hi][lo][argCount] (4 byte): l'IP si disallineava e il
        // flusso dopo lo statement (es. i PRINT legacy in RDD_Legacy.bas) veniva saltato.
        EmitByte((byte)OpCode.OP_SYSCALL);
        EmitSysCall(syscall);
        EmitByte(1);
    }

    private void VisitRddSetDeletedStatement(RddSetDeletedStmt stmt)
    {
        // Allineato alla forma funzione RDD.SETDELETED(area, on): pusha [area, on]
        // (area prima), la VM popa on prima (in cima) — vedi case SysCall.RDD_SETDELETED.
        EmitConstant(0.0); // area 0 = corrente
        EmitConstant((double)(stmt.On ? 1 : 0));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_SETDELETED); EmitByte((byte)2);
    }

    private void VisitRddSetFieldStatement(RddSetFieldStmt stmt)
    {
        VisitExpression(stmt.Area);
        VisitExpression(stmt.Field);
        VisitExpression(stmt.Value);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_SET_FIELD); EmitByte((byte)3);
    }

    private void VisitRddDeleteStatement(RddDeleteStmt stmt)
    {
        var scopeArgCount = CompileScopeClause(stmt.Scope);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_DELETE_SCOPED); EmitByte((byte)(scopeArgCount));
    }

    private void VisitRddRecallStatement(RddRecallStmt stmt)
    {
        var scopeArgCount = CompileScopeClause(stmt.Scope);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_RECALL_SCOPED); EmitByte((byte)(scopeArgCount));
    }

    private void VisitRddReplaceStatement(RddReplaceStmt stmt)
    {
        // Spinge i replacement (fieldName, value) come coppie, poi count, poi lo SCOPE
        // (in CIMA — pattern PopScopeClause/DBF_REPLACE): le syscall consumano dalla cima.
        foreach (var r in stmt.Replacements)
        {
            // FieldTarget (campo risolto via CurrentUdtScopeId): .ToString() emetteva la
            // rappresentazione debug del record invece del nome campo, stesso bug/causa
            // già risolto per DBF REPLACE (vedi Compiler.Dbf.cs.VisitDbfReplaceStatement).
            if (r.Target is VariableTarget vt)
                EmitConstant(vt.NameCleaned.Lexeme);
            else if (r.Target is FieldTarget ft)
                EmitConstant(ft.OriginalToken.Lexeme);
            else
                EmitConstant(r.Target?.ToString() ?? "");
            VisitExpression(r.Value);
        }
        EmitConstant((double)stmt.Replacements.Length);
        var scopeArgCount = CompileScopeClause(stmt.Scope);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_REPLACE_SCOPED);
        EmitByte((byte)(scopeArgCount + stmt.Replacements.Length * 2 + 1));
    }

    private void VisitRddSetReprocessStatement(RddSetReprocessStmt stmt)
    {
        VisitExpression(stmt.Ms);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_SETREPROCESS); EmitByte((byte)1);
    }

    private void VisitRddSetOrderStatement(RddSetOrderStmt stmt)
    {
        VisitExpression(stmt.Area);
        VisitExpression(stmt.Order);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_SETORDER); EmitByte((byte)2);
    }

    private void VisitRddSetLockTimeoutStatement(RddSetLockTimeoutStmt stmt)
    {
        VisitExpression(stmt.Ms);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_SETLOCKTIMEOUT); EmitByte((byte)1);
    }

    private void VisitRddOnEventStatement(RddOnEventStmt stmt)
    {
        VisitExpression(stmt.EventName);
        if (stmt.Callback != null) VisitExpression(stmt.Callback);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_ONEVENT); EmitByte((byte)2);
    }

    private void VisitRddOffEventStatement(RddOffEventStmt stmt)
    {
        VisitExpression(stmt.EventName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_OFFEVENT); EmitByte((byte)1);
    }

    private void VisitRddCreateIndexStatement(RddCreateIndexStmt stmt)
    {
        VisitExpression(stmt.Area);
        VisitExpression(stmt.Name);
        VisitExpression(stmt.Key);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_CREATEINDEX); EmitByte((byte)3);
    }

    private void VisitRddCloseIndexStatement(RddCloseIndexStmt stmt)
    {
        VisitExpression(stmt.Area);
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_CLOSEINDEX); EmitByte((byte)2);
    }

    private void VisitRddCopyToStatement(RddCopyToStmt stmt)
    {
        VisitExpression(stmt.Area);
        VisitExpression(stmt.FileName);
        if (stmt.OutDriver != null) VisitExpression(stmt.OutDriver);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_COPYTO); EmitByte((byte)3);
    }

    private void VisitRddSortToStatement(RddSortToStmt stmt)
    {
        VisitExpression(stmt.Area);
        VisitExpression(stmt.FileName);
        if (stmt.Fields != null) VisitExpression(stmt.Fields);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_SORTTO); EmitByte((byte)3);
    }

    private void VisitRddTotalOnStatement(RddTotalOnStmt stmt)
    {
        VisitExpression(stmt.Area);
        VisitExpression(stmt.Key);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RDD_TOTALON); EmitByte((byte)2);
}

}
