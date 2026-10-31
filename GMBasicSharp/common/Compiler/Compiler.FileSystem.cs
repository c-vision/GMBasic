// FILE: Interpreter.Compiler.Compiler.FileSystem.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per le istruzioni che interagiscono con il filesystem
/// (manipolazione di file e directory).
/// </summary>
public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione MKDIR.
    /// </summary>
    private void VisitMkDirStatement(MkDirStmt stmt)
    {
        VisitExpression(stmt.DirectoryPath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MKDIR); EmitByte((byte)1);
    }
    
    /// <summary>
    /// Compila un'istruzione RMDIR.
    /// </summary>
    private void VisitRmDirStatement(RmDirStmt stmt)
    {
        VisitExpression(stmt.DirectoryPath);
        if (stmt.RecursiveExpr is not null) VisitExpression(stmt.RecursiveExpr);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RMDIR); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione CHDIR.
    /// </summary>
    private void VisitChDirStatement(ChDirStmt stmt)
    {
        VisitExpression(stmt.DirectoryPath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHDIR); EmitByte((byte)1);
    }
    
    /// <summary>
    /// Compila un'istruzione COPY.
    /// </summary>
    private void VisitCopyStatement(CopyStmt stmt)
    {
        VisitExpression(stmt.SourceFile);
        VisitExpression(stmt.DestinationFile);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.COPY_FILE); EmitByte((byte)2);
    }
    
    /// <summary>
    /// Compila un'istruzione MOVE.
    /// </summary>
    private void VisitMoveStatement(MoveStmt stmt)
    {
        VisitExpression(stmt.SourceFile);
        VisitExpression(stmt.DestinationFile);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOVE_FILE); EmitByte((byte)2);
    }
    
    /// <summary>
    /// Compila un'istruzione XCOPY.
    /// </summary>
    private void VisitXCopyStatement(XCopyStmt stmt)
    {
        VisitExpression(stmt.SourceDirectory);
        VisitExpression(stmt.DestinationDirectory);
        if (stmt.RecursiveExpr is not null) VisitExpression(stmt.RecursiveExpr);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.OverwriteExpr is not null) VisitExpression(stmt.OverwriteExpr);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.XCOPY_DIR); EmitByte((byte)4);
    }

    /// <summary>
    /// Compila un'istruzione KILL.
    /// </summary>
    private void VisitKillStatement(KillStmt stmt)
    {
        VisitExpression(stmt.FileName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.KILL_FILE); EmitByte((byte)1);
    }

    /// <summary>
    /// Compila un'istruzione RENAME.
    /// </summary>
    private void VisitRenameStatement(RenameStmt stmt)
    {
        VisitExpression(stmt.OldFileName);
        VisitExpression(stmt.NewFileName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RENAME_FILE); EmitByte((byte)2);
    }
}