// FILE: Interpreter.Compiler.Compiler.Exceptions.cs



namespace Compiler;

public sealed partial class Compiler
{
    private void VisitTryCatchStatement(TryCatchStmt stmt)
    {
        var tryOpcodeAddress = _chunk.Code.Count;
        var jumpToCatchHandlers = EmitJump((byte)OpCode.OP_PUSH_TRY);

        BeginScope();
        foreach (var tryStmt in stmt.TryBody)
        {
            VisitStatement(tryStmt);
        }

        var tryExitReachable = _isReachable;

        EndScope();

        EmitByte((byte)OpCode.OP_POP_TRY);

        var jumpToFinally = EmitJump((byte)OpCode.OP_JUMP_FORWARD);

        PatchJump(jumpToCatchHandlers);

        var catchExitJumps = new List<int>();
        var anyCatchExitReachable = false;

        foreach (var catchClause in stmt.CatchClauses)
        {
            _isReachable = true;
            BeginScope();

            if (catchClause.Variable is not null)
            {
                // OP_PUSH_TRY (per THROW/ERROR non gestiti da nessun try precedente)
                // e OP_THROW/SysCall.THROW_ERROR (vedi PCodeVm/PCodeProcessor.cs) lasciano
                // SEMPRE un singolo valore sullo stack quando saltano qui — l'eccezione
                // catturata (un'istanza CLASS come Dictionary con "__class__", o per
                // ERROR/CATCH ex AS ERR un Dictionary con "Number"/"Description"). AddLocal
                // riserva solo lo SLOT: senza questo STORE_LOCAL il valore restava sullo
                // stack (mai consumato) e la variabile "ex" leggeva zero/vuoto invece
                // dell'eccezione reale — bug scoperto testando CATCH ex AS <CLASS utente>
                // (i campi risultavano sempre vuoti).
                AddLocal(catchClause.Variable.NameCleaned);
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)(_locals.Count - 1));
            }

            if (catchClause.WhenCondition is not null)
            {
                VisitExpression(catchClause.WhenCondition);

                var jumpToNextCatch = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);

                foreach (var catchStmt in catchClause.Body)
                {
                    VisitStatement(catchStmt);
                }

                anyCatchExitReachable |= _isReachable;

                catchExitJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
                PatchJump(jumpToNextCatch);
            }
            else
            {
                if (catchClause.Variable is null)
                {
                    EmitByte((byte)OpCode.OP_POP);
                }

                foreach (var catchStmt in catchClause.Body)
                {
                    VisitStatement(catchStmt);
                }

                anyCatchExitReachable |= _isReachable;

                catchExitJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
            }

            EndScope();
        }

        // L'indirizzo corrente è l'inizio del blocco FINALLY (o la fine se non c'è).
        var finallyAddress = _chunk.Code.Count;
        PatchJump(jumpToFinally, finallyAddress);

        if (stmt.Finally is not null)
        {
            _isReachable = true;
            BeginScope();
            foreach (var finallyStmt in stmt.Finally.Body)
            {
                VisitStatement(finallyStmt);
            }

            EndScope();
        }

        var finallyCompletesNormally = stmt.Finally is null || _isReachable;

        // Patcha tutti i salti che escono dai blocchi CATCH per puntare
        // all'inizio del blocco FINALLY.
        foreach (var exitJump in catchExitJumps)
        {
            PatchJump(exitJump, finallyAddress);
        }

        _chunk.TryRegions[tryOpcodeAddress] = new TryRegionMetadata(
            finallyAddress,
            _chunk.Code.Count,
            stmt.CatchClauses.Length > 0);
        _isReachable = finallyCompletesNormally && (tryExitReachable || anyCatchExitReachable);
    }
}
