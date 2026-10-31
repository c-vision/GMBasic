// FILE: Interpreter.Compiler.Compiler.OnErrors.cs



namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Compila un'istruzione ON ERROR GOTO/GOSUB.
    /// </summary>
    private void VisitOnErrorStatement(OnErrorStmt stmt)
    {
        // Scegli l'opcode corretto in base alla keyword GOTO o GOSUB.
        if (stmt.Keyword != null)
        {
            var opCode = stmt.Keyword.Kind switch
            {
                TokenKind.Goto => OpCode.OP_SET_ERR_HANDLER_GOTO,
                TokenKind.Gosub => OpCode.OP_SET_ERR_HANDLER_GOSUB,
                _ => throw new Exception($"Unsupported ON ERROR keyword: {stmt.Keyword.Lexeme}")
            };

            // Gestione del caso speciale: ON ERROR GOTO 0
            if (stmt.Target != null && stmt.Target.Kind == TokenKind.Number && stmt.Target.Literal is double numberValue && numberValue == 0)
            {
                // GOTO 0 significa "disabilita il gestore di errori".
                // Usiamo l'opcode GOTO e un indirizzo 0.
                EmitByte((byte)OpCode.OP_SET_ERR_HANDLER_GOTO);
                EmitShort(0);
            }
            else
            {
                // Per tutti gli altri target, emettiamo l'opcode scelto con un placeholder da patchare.
                var instructionIp = _chunk.Code.Count;
                var patchOffset = EmitJump((byte)opCode);
                _explicitControlTransfers.Add(new(
                    instructionIp, patchOffset, opCode.ToString(), RequireUnprotectedTarget: true));
            
                if (stmt.Target != null && stmt.Target.Kind == TokenKind.Number && stmt.Target.Literal is double lineValue)
                {
                    var lineNumber = (int)lineValue;
                    // NOTA: Il patching per i numeri di riga va adattato per passare anche l'opcode.
                    // Per ora, lo lasciamo così, ma andrà rivisto se questo sistema non basta.
                    // La logica attuale di patching non distingue tra tipi di salto.
                    _lineNumberJumpsToPatch.Add((patchOffset, lineNumber));
                }
                else
                {
                    _jumpsToPatch.Add((patchOffset, stmt.Target)!);
                }
            }
        }
    }

    /// <summary>
    /// Compila un'istruzione ON ERROR RESUME NEXT.
    /// </summary>
    private void VisitOnErrorResumeStatement(OnErrorResumeStmt stmt)
    {
        // ON ERROR RESUME NEXT: statement che IMPOSTA l'handler resume-next (non salta).
        // Prima emetteva OP_RESUME (identico a RESUME NEXT): con un handler ON ERROR GOTO
        // attivo la VM faceva un salto di resume all'indietro -> loop infinito (hang VM/C).
        EmitByte((byte)OpCode.OP_SET_ERR_HANDLER_RESUME);
    }
    
    private void VisitErrorStatement(ErrorStmt stmt)
    {
        VisitExpression(stmt.ErrorCode);
        if (stmt.Description is not null) VisitExpression(stmt.Description);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.THROW_ERROR); EmitByte((byte)2);
    }
    
    private void VisitResumeStatement(ResumeStmt stmt)
    {
        EmitByte((byte)OpCode.OP_RESUME);

        // --- INIZIO MODIFICA ---
        if (stmt.Target is null)
        {
            // Caso 1: RESUME (senza niente dopo)
            // L'indirizzo speciale 1 indica alla VM di rieseguire l'istruzione.
            EmitShort(1);
        }
        else if (stmt.Target.Kind == TokenKind.Next)
        {
            // Caso 2: RESUME NEXT
            // L'indirizzo speciale 0 indica alla VM di saltare all'istruzione successiva.
            EmitShort(0);
        }
        else
        {
            // Caso 3: RESUME <etichetta> o RESUME <numero_riga>
            // Il target è un'etichetta o un numero di riga, quindi va patchato.
            var patchOffset = _chunk.Code.Count;
            EmitShort(0); // Placeholder per l'indirizzo assoluto
            _explicitControlTransfers.Add(new(
                patchOffset - 1, patchOffset, nameof(OpCode.OP_RESUME), RequireUnprotectedTarget: true));

            if (stmt.Target.Kind == TokenKind.Number && stmt.Target.Literal is double numberValue)
            {
                var lineNumber = (int)numberValue;
                _lineNumberJumpsToPatch.Add((patchOffset, lineNumber));
            }
            else
            {
                // Se non è un numero, deve essere un'etichetta.
                _jumpsToPatch.Add((patchOffset, stmt.Target));
            }
        }
        // --- FINE MODIFICA ---
    }
}
