// FILE: Interpreter.Compiler.Compiler.Files.cs



namespace Compiler;

/// <summary>
    /// Contiene i metodi di visita per tutte le istruzioni relative a I/O su file di dati
    /// (sequenziali, random, binari).
    /// </summary>
public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione OPEN. Usa l'opcode prefisso OP_FILE_CMD
    /// perché OPEN è un comando complesso con molte varianti.
    /// </summary>
    private void VisitOpenStatement(OpenStmt stmt)
    {
        VisitExpression(stmt.FileName);
        EmitConstant((double)stmt.Mode);
        EmitConstant((double)stmt.LockMode);
        VisitExpression(stmt.FileNumber);
        if (stmt.RecordLength is not null) VisitExpression(stmt.RecordLength); else EmitByte((byte)OpCode.OP_PUSH_NIL);

        var typeCode = ContentTypeCode.Default;
        if (stmt.FileContentType is JsonType) typeCode = ContentTypeCode.Json;
        EmitConstant((double)typeCode);

        EmitConstant(0.0); // Numero di chiavi (non supportato per file standard)
        EmitConstant((double)stmt.IndexPaths.Length); // Numero di indici

        EmitConstant(0.0); // UNSAFE = false

        var totalArgs = 9; // FileName, Mode, LockMode, FileNumber, RecordLength, ContentTypeCode, KeyCount, IndexCount, Unsafe

        EmitBytes((byte)OpCode.OP_FILE_CMD, (byte)FileSubOpCode.OPEN, (byte)totalArgs);
    }
    
    /// <summary>
    /// Compila un'istruzione CLOSE. Usa OP_SYSCALL perché è un comando semplice
    /// senza varianti complesse.
    /// </summary>
    private void VisitCloseStatement(CloseStmt stmt)
    {
        if (stmt.IsIsam)
        {
            // ISAM CLOSE [alias, ...]: chiude davvero l'area (prima compilato come CLOSE #n: l'alias
            // veniva valutato come numero di file e non succedeva nulla). Una syscall per alias; senza
            // argomenti un solo nil = area ISAM corrente.
            if (stmt.FileNumbers.IsEmpty)
            {
                EmitByte((byte)OpCode.OP_PUSH_NIL);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_CLOSE); EmitByte((byte)1);
                return;
            }
            foreach (var area in stmt.FileNumbers)
            {
                EmitFileNumber(area);
                EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ISAM_CLOSE); EmitByte((byte)1);
            }
            return;
        }
        if (stmt.FileNumbers.IsEmpty)
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL); 
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CLOSE_FILE); EmitByte((byte)1);
        }
        else
        {
            foreach(var fileNum in stmt.FileNumbers)
            {
                VisitExpression(fileNum);
            }
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CLOSE_FILE); EmitByte((byte)(stmt.FileNumbers.Length));
        }
    }
    
    /// <summary>
    /// Compila un'istruzione PRINT #.
    /// </summary>
    private void VisitPrintFileStatement(PrintFileStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        foreach (var arg in stmt.Arguments)
        {
            if (arg.Expression is not null) VisitExpression(arg.Expression);
            else EmitConstant(arg.Separator ?? string.Empty);
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PRINT_FILE); EmitByte((byte)((stmt.Arguments.Length + 1)));
    }
    
    /// <summary>
    /// Compila un'istruzione WRITE #.
    /// </summary>
    private void VisitWriteFileStatement(WriteFileStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        foreach (var arg in stmt.Arguments)
        {
            VisitExpression(arg);
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WRITE_FILE); EmitByte((byte)((stmt.Arguments.Length + 1)));
    }
    
    /// <summary>
    /// Compila un'istruzione INPUT #.
    /// </summary>
    private void VisitInputFileStatement(InputFileStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        // HINT di tipo per ogni target (1 = numerico, 0 = stringa): il runtime divide la
        // riga nei campi CSV e converte il campo i-esimo secondo il tipo del target
        // (TRUE/FALSE → -1/0). Senza hint, un campo "101" finiva come STRINGA nello slot
        // numerico (INPUT # multi-var divergente fra le 4 gambe).
        foreach (var target in stmt.Variables)
        {
            var numeric = true;
            if (target is VariableTarget vt)
            {
                var t = InferExpressionTypeFromTarget(vt);
                numeric = t is not StringType and not InferredType and not AnyType;
            }
            EmitConstant(numeric ? 1.0 : 0.0);
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.INPUT_FILE); EmitByte((byte)(stmt.Variables.Length + 1));

        for (var i = stmt.Variables.Length - 1; i >= 0; i--)
        {
            AssignVariable(stmt.Variables[i], isLet: false);
        }
    }
    
    /// <summary>
    /// Compila un'istruzione LINE INPUT #.
    /// </summary>
    private void VisitLineInputFileStatement(LineInputFileStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.LINE_INPUT_FILE); EmitByte((byte)1);
        AssignVariable(stmt.Variable, isLet: false);
    }
    
    /// <summary>
    /// Compila un'istruzione GET #.
    /// </summary>
    private void VisitGetFileStatement(GetFileStmt stmt)
    {
        EmitFileNumber(stmt.FileNumber);
        if (stmt.RecordNumber is not null) VisitExpression(stmt.RecordNumber);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        // GET su UDT (file RANDOM): descrittore dei campi + istanza corrente della variabile
        // destinazione, che il syscall riempie IN PLACE per nome campo (percorso legacy invariato
        // per qualunque altro target).
        if (!stmt.IsIsam && stmt.Destination is VariableTarget gvt
            && InferExpressionTypeFromTarget(gvt) is UserDefinedType gUdt
            && TryRandomRecordDescriptor(gUdt, out var gDesc))
        {
            EmitConstant(gDesc);
            VisitExpression(new VariableExpr(gvt.SymbolId, gvt.NameSuffixed, gvt.NameCleaned));
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.GET_FILE); EmitByte((byte)4);
            AssignVariable(stmt.Destination, isLet: false);
            return;
        }
        // GET #n, pos, <array di BYTE>: descriptor "@y" + array corrente → il syscall lo riempie in place.
        if (!stmt.IsIsam && stmt.Destination is VariableTarget bvt
            && InferExpressionTypeFromTarget(bvt) is ArrayType { ElementType: ByteType })
        {
            EmitConstant("@y");
            VisitExpression(new VariableExpr(bvt.SymbolId, bvt.NameSuffixed, bvt.NameCleaned));
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.GET_FILE); EmitByte((byte)4);
            AssignVariable(stmt.Destination, isLet: false);
            return;
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.GET_FILE); EmitByte((byte)2);
        AssignVariable(stmt.Destination, isLet: false);
    }

    /// <summary>Descrittore RANDOM dei campi di uno UDT ("nome:tok;…", vedi TypeLayout.RandomRecordToken),
    /// false se l'UDT ha campi non ammessi nel record (PUT/GET restano sul percorso legacy).</summary>
    private bool TryRandomRecordDescriptor(UserDefinedType udt, out string descriptor)
        => TypeLayout.TryRandomRecordDescriptor(udt,
            u => _ctx?.GetAllUserDefinedTypes().FirstOrDefault(x =>
                x.Name.Lexeme.Equals(u.Name.Lexeme, StringComparison.OrdinalIgnoreCase)),
            out descriptor);
    
    /// <summary>
    /// Compila un'istruzione PUT #.
    /// </summary>
    private void VisitPutFileStatement(PutFileStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        if (stmt.RecordNumber is not null) VisitExpression(stmt.RecordNumber);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        VisitExpression(stmt.Source);
        // PUT di un array di BYTE: descriptor "@y" (un byte grezzo per elemento).
        if (CompilerInferExpressionType(stmt.Source) is ArrayType { ElementType: ByteType })
        {
            EmitConstant("@y");
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PUT_FILE); EmitByte((byte)4);
            return;
        }
        // PUT di un UDT: il syscall riceve anche il descrittore dei campi (file RANDOM → record a layout canonico).
        if (CompilerInferExpressionType(stmt.Source) is UserDefinedType pUdt && TryRandomRecordDescriptor(pUdt, out var pDesc))
        {
            EmitConstant(pDesc);
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PUT_FILE); EmitByte((byte)4);
            return;
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PUT_FILE); EmitByte((byte)3);
    }
    
    /// <summary>
    /// Compila un'istruzione SEEK #.
    /// </summary>
    private void VisitSeekStatement(SeekStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        VisitExpression(stmt.PositionExpr);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SEEK_FILE); EmitByte((byte)2);
    }
}