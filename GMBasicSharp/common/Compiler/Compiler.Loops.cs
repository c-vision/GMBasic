// FILE: Interpreter.Compiler.Compiler.Loops.cs



namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Compila l'istruzione di setup di un ciclo FOR utilizzando l'opcode ottimizzato OP_FOR_PREPARE.
    /// Dopo aver emesso l'opcode, aggiorna il contesto del loop con l'indirizzo di inizio
    /// corretto per il corpo del ciclo.
    /// </summary>
    private void VisitForStatement(ForStmt stmt)
    {
        // Canone shadowing contatore FOR (2026-09-30): il contatore usa SEMPRE uno slot
        // proprio (ombra locale al loop), MA il suo destino a NEXT dipende dall'esterno:
        //  - esiste un binding esterno (DIM locale o GLOBALE) → l'ombra è rimossa a NEXT e
        //    l'esterno riemerge (LoopsForNextOptimized TEST 4: M torna 42);
        //  - NON esiste → il contatore è PROMOSSO a scope esterno a fine ciclo (visibile
        //    dopo il NEXT con l'ultimo valore, come su IL: LoopsForNextLabels I/J/K = 2).
        // Il flag viaggia in _forCounterHadOuter (push qui, pop in VisitExitLoopStatement).
        var hadOuterBinding = ResolveLocal(stmt.Variable.NameCleaned.Lexeme) != -1
            || _globals.ContainsKey(NormalizeIdentifierName(stmt.Variable.NameCleaned.Lexeme));
        _forCounterHadOuter.Push(hadOuterBinding);
        AddLocal(stmt.Variable.NameCleaned);

        var localSlot = ResolveLocal(stmt.Variable.NameCleaned.Lexeme);
        if (localSlot == -1)
        {
            throw new Exception($"Internal Compiler Error: Loop variable '{stmt.Variable.NameSuffixed.Lexeme}' not resolved immediately after declaration.");
        }

        VisitExpression(stmt.Start);
        VisitExpression(stmt.End);
        if (stmt.Step is not null)
        {
            VisitExpression(stmt.Step);
        }
        else
        {
            EmitConstant(1.0);
        }

        EmitByte((byte)OpCode.OP_FOR_PREPARE); EmitShort((ushort)localSlot);
        
        // Emetti JUMP_IF_FALSE per saltare il corpo se i bound sono sbagliati
        // (FOR_PREPARE ha spinto 1.0 se deve eseguire, 0.0 se skip)
        var skipJumpPos = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
        // Salva la posizione del salto per patching — usiamo _jumpsToPatch con sentinel
        _jumpsToPatch.Add((skipJumpPos, new Token(TokenKind.Identifier, "__for_skip", null, stmt.Variable.Position)));

        // Ora che OP_FOR_PREPARE è stato emesso, l'indirizzo corrente è l'inizio
        // effettivo del corpo del loop. Aggiorniamo il contesto che EnterLoopStmt ha pushato.
        if (_forLoopContexts.Count > 0)
        {
            var context = _forLoopContexts.Pop();
            context.loopStartOffset = _chunk.Code.Count; // Imposta l'indirizzo di inizio corretto.
            context.counterSlot = localSlot;             // Slot del contatore del loop (per fusion).
            _forLoopContexts.Push(context);
        }
    }

    /// <summary>
    /// Questo nodo AST viene ignorato. La sua logica è ora completamente
    /// gestita dall'opcode OP_FOR_LOOP, emesso da VisitNextStatement.
    /// </summary>
    private void VisitForCheckAndJumpStatement(ForCheckAndJumpStmt stmt)
    {
        // NO-OP (No Operation). Lasciare questo metodo vuoto è intenzionale.
    }

    /// <summary>
    /// Compila un'istruzione NEXT emettendo l'opcode ottimizzato OP_FOR_LOOP.
    /// Questo opcode unifica l'incremento, il controllo del limite e il salto all'indietro.
    /// Imposta anche un flag per ignorare il `JumpStmt` ridondante che lo segue nell'AST.
    /// </summary>
    private void VisitNextStatement(NextStmt stmt)
    {
        var localSlot = ResolveLocal(stmt.Variable.Lexeme);
        if (localSlot == -1)
            throw new Exception($"Internal Compiler Error: Loop variable '{stmt.Variable.Lexeme}' not found for NEXT.");

        if (_forLoopContexts.Count == 0)
            throw new Exception("Compiler Error: NEXT statement encountered without a corresponding FOR context stack.");

        var (loopStart, _, _) = _forLoopContexts.Peek();
        
        if(loopStart == -1)
            throw new Exception("Internal Compiler Error: FOR loop start offset was not correctly updated.");

        var offset = _chunk.Code.Count + 5 - loopStart;
        if (offset > ushort.MaxValue)
            throw new Exception("Loop body too large for OP_FOR_LOOP.");

        EmitByte((byte)OpCode.OP_FOR_LOOP);
        EmitShort((ushort)localSlot);
        EmitShort(offset);

        // Patcha il JUMP_IF_FALSE di VisitForStatement per saltare qui se il corpo non deve eseguire
        for (int pj = _jumpsToPatch.Count - 1; pj >= 0; pj--)
        {
            if (_jumpsToPatch[pj].Item2.Kind == TokenKind.Identifier && _jumpsToPatch[pj].Item2.Lexeme == "__for_skip")
            {
                var jp = _jumpsToPatch[pj].Item1;
                var jumpOffset = _chunk.Code.Count - (jp + 2);
                if (jumpOffset <= ushort.MaxValue)
                {
                    _chunk.Code[jp] = (byte)((jumpOffset >> 8) & 0xFF);
                    _chunk.Code[jp + 1] = (byte)(jumpOffset & 0xFF);
                }
                _jumpsToPatch.RemoveAt(pj);
                break;
            }
        }
        
        _ignoreNextJump = true;
    }

    /// <summary>
    /// Compila un'istruzione EXIT (es. EXIT FOR, EXIT FUNCTION).
    /// </summary>
    private void VisitExitStatement(ExitStmt stmt)
    {
        if (stmt.TargetLabel is not null)
        {
            var jumpLocation = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
            _jumpsToPatch.Add((jumpLocation, stmt.TargetLabel));
        }
        else
        {
            if (stmt.TargetLoop.Kind == TokenKind.Function)
            {
                // EXIT FUNCTION deve restituire il return-slot (convenzione BASIC
                // `NomeFunzione = ...`), non NIL. Stessa semantica di END FUNCTION.
                var exitRetSlot = ResolveLocal(_currentProcedureName ?? "");
                if (exitRetSlot != -1)
                {
                    EmitByte((byte)OpCode.OP_LOAD_LOCAL);
                    EmitShort((ushort)exitRetSlot);
                }
                else
                {
                    EmitByte((byte)OpCode.OP_PUSH_NIL);
                }
                // BUG-6 residuo (2026-09-03): EXIT SUB/EXIT FUNCTION termina la procedura →
                // finalizza i locali del body PRIMA di uscire (stessa guardia di
                // VisitReturnStatement: skip se la procedura ha GOSUB — il RETURN sarebbe
                // ambiguo — o se la FUNCTION restituisce un tipo oggetto).
                if (_bodyLocalsBoundary >= 0 && !_currentProcHasGosub
                    && _currentFunctionReturnType is not (ClassType or UserDefinedType))
                {
                    EmitFinalizeBodyLocals();
                }
            }

            EmitByte((byte)OpCode.OP_RETURN);
        }

        _isReachable = false;
    }

    /// <summary>
    /// Compila un'istruzione CONTINUE (es. CONTINUE FOR).
    /// </summary>
    private void VisitContinueStatement(ContinueStmt stmt)
    {
        var jumpLocation = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
        _jumpsToPatch.Add((jumpLocation, stmt.TargetLabel));

        _isReachable = false;
    }

    /// <summary>
    /// Compila l'istruzione EnterLoopStmt, che marca l'inizio di un blocco di ciclo.
    /// </summary>
    private void VisitEnterLoopStatement(EnterLoopStmt stmt)
    {
        BeginScope();
        if (stmt.StartToken.Kind == TokenKind.For)
        {
            // Pusha un contesto placeholder. L'indirizzo di inizio corretto (-1)
            // verrà impostato da VisitForStatement dopo aver emesso OP_FOR_PREPARE.
            _forLoopContexts.Push((-1, stmt.ExitLabel, -1));
        }
    }

    /// <summary>
    /// Compila l'istruzione ExitLoopStmt, che marca la fine logica di un blocco di ciclo.
    /// Chiude lo scope aperto da VisitEnterLoopStatement: i locali del corpo (incluso il
    /// contatore FOR, slot fresco da VisitForStatement) muoiono qui — dopo il ciclo i nomi
    /// risolvono di nuovo alle DIM esterne (canone shadowing 2026-09-30). Emette
    /// OP_FINALIZE_SCOPE per gli slot del loop (no-op sui double; indispensabile per UDT).
    /// Posizione sicura: dopo NextStmt (resolve+patch già fatti) e prima della label FOR_EXIT
    /// (EXIT FOR ci atterra DOPO la finalizzazione); lo skip da FOR_PREPARE salta la
    /// finalizzazione di slot mai scritti (benigno).
    /// </summary>
    private void VisitExitLoopStatement(ExitLoopStmt stmt)
    {
        // Pop solo se il ciclo che termina è un FOR (EndToken == Next).
        // ExitLoopStmt è emesso sia per FOR che per DO/WHILE/LOOP.
        // Solo FOR usa _forLoopContexts; DO/WHILE/LOOP no, quindi non devono pop.
        if (_forLoopContexts.Count != 0 && stmt.EndToken.Kind == TokenKind.Next)
        {
            var counterSlot = _forLoopContexts.Peek().counterSlot;
            var hadOuter = _forCounterHadOuter.Count > 0 && _forCounterHadOuter.Pop();
            _forLoopContexts.Pop();
            // Gestione del contatore a fine ciclo (indici degli slot STABILI: non usiamo
            // la rimozione per profondità di EndScope, che è "contigua in coda" e verrebbe
            // bloccata da un contatore interno promosso):
            //  - binding esterno → RINOMINA l'ombra con un sentinella irrisolvibile + Depth 0,
            //    così ResolveLocal torna a trovare la variabile esterna (valore intatto);
            //  - nessun binding esterno → promuovi a Depth 0 mantenendo il nome: il contatore
            //    resta leggibile dopo il NEXT con l'ultimo valore (parità IL).
            if (counterSlot >= 0 && counterSlot < _locals.Count)
            {
                _locals[counterSlot].Depth = 0;
                if (hadOuter)
                    _locals[counterSlot].Name = new Token(TokenKind.Identifier,
                        "__for_removed_" + counterSlot, null, SourcePosition.Unknown);
            }
        }
        EndScope();
    }
}