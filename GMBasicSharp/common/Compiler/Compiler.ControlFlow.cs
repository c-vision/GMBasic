// FILE: Interpreter.Compiler.Compiler.ControlFlow.cs



namespace Compiler;

public sealed partial class Compiler
{
    // Mappa per risolvere le etichette: Nome dell'etichetta -> Indirizzo P-Code
    private readonly Dictionary<string, int> _labels = new(StringComparer.OrdinalIgnoreCase);
    // Etichetta (non generata dal compilatore) -> numero di riga: per RESTORE <etichetta> (il
    // target DATA si calcola dalla riga dell'etichetta, come RESTORE <numero>).
    private readonly Dictionary<string, int> _labelLineByLabel = new(StringComparer.OrdinalIgnoreCase);
    // Numero di riga DATA -> indice del primo valore nel data pool (popolato da PopulateDataPool,
    // usato da RESTORE <riga|etichetta> per calcolare la posizione nel pool).
    private readonly Dictionary<int, int> _dataLineToIndex = new();
    // Lista di salti da patchare: Posizione dell'operando del salto -> Nome dell'etichetta target
    private readonly List<(int, Token)> _jumpsToPatch = [];
    /// <summary>Espone i jumps non ancora patchati per la propagazione al genitore.</summary>
    internal IReadOnlyList<(int offset, Token target)> GetJumpsToPatch() => _jumpsToPatch;

    /// <summary>
    /// 2026-09-24 (overload): token di patch per chiamate a procedura con arità della
    /// DEFINIZIONE risolta ("Nome#k") — il patching per solo-nome eseguiva sempre
    /// l'ultimo overload registrato. Senza definizione risolta usa l'arità del call-site.
    /// Le etichette e i nomi procedura non contengono mai '#', quindi nessuna collisione.
    /// Il SymbolId (quando noto) viaggia in <c>Literal</c>: PatchAllJumps preferisce
    /// l'indirizzo per-Simbolo (@sym{id}) — disambigua le omonime in namespace diversi
    /// (il chunk non conserva il namespace nel nome), con fallback a "Nome#k" e al nome nudo.
    /// </summary>
    internal static Token CallPatchToken(Token nameToken, ProcedureDefinition? proc, int argsLength)
    {
        var arity = proc?.Parameters.Length ?? argsLength;
        object? literal = proc is { SymbolId: >= 0 } ? proc.SymbolId : nameToken.Literal;
        return new Token(nameToken.Kind, $"{nameToken.Lexeme}#{arity}", literal, nameToken.Position);
    }

    /// <summary>
    /// Compila un'istruzione IF...THEN...ELSE, gestendo correttamente i salti e
    /// l'analisi di raggiungibilità del codice.
    /// Questa versione è stata riscritta per correggere un bug di "fall-through".
    /// </summary>
    private void VisitIfStatement(IfStmt stmt)
    {
        var endJumps = new List<int>();
        var jumpToNextClause = -1;
        var flowContinues = false; // Traccia se almeno un percorso permette al flusso di continuare dopo l'END IF.

        foreach (var ifClause in stmt.IfClauses)
        {
            if (jumpToNextClause != -1)
            {
                PatchJump(jumpToNextClause);
                _isReachable = true; // L'inizio di una clausola ELSE/ELSEIF è un punto raggiungibile.
            }

            VisitExpression(ifClause.Condition);
            jumpToNextClause = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);

            BeginScope();
            foreach (var thenStmt in ifClause.Statements)
            {
                VisitStatement(thenStmt);
            }
            EndScope();

            if (_isReachable)
            {
                endJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
                flowContinues = true;
            }
        }

        if (jumpToNextClause != -1)
        {
            PatchJump(jumpToNextClause);
            _isReachable = true;
        }

        if (stmt.ElseClause != null)
        {
            BeginScope();
            foreach (var elseStmt in stmt.ElseClause.Statements)
            {
                VisitStatement(elseStmt);
            }
            EndScope();

            if (_isReachable)
            {
                flowContinues = true;
            }
        }
        else
        {
            flowContinues = true;
        }

        PatchJumps(endJumps);
        _isReachable = flowContinues;
    }

    /// <summary>
    /// Compila un blocco SELECT CASE.
    /// Aggiornato per gestire la nuova sintassi con intervalli (TO) e comparazioni (IS).
    /// </summary>
    private void VisitSelectStatement(SelectStmt stmt)
    {
        // L'espressione SELECT viene valutata una sola volta e il suo valore
        // viene salvato in una variabile locale temporanea.
        BeginScope();
        VisitExpression(stmt.Expression);
        var selectLocal = new Token(TokenKind.Identifier, "__select_expr", null, SourcePosition.Unknown);
        AddLocal(selectLocal);
        var selectExprIndex = _locals.Count - 1;
        // Salva il valore dell'espressione in __select_expr
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)selectExprIndex);

        var endJumps = new List<int>();
        var jumpToNextCase = -1;
        var isAnyClauseReachable = false;

        foreach (var caseClause in stmt.CaseClauses)
        {
            if (jumpToNextCase != -1)
            {
                PatchJump(jumpToNextCase);
                _isReachable = true;
            }

            var jumpsToBody = new List<int>();

            foreach (var test in caseClause.Tests)
            {
                // Carica sempre il valore salvato dell'espressione SELECT prima di ogni test.
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)selectExprIndex);

                switch (test)
                {
                    case CaseValue cv:
                        VisitExpression(cv.Value);
                        EmitByte((byte)OpCode.OP_CMP_EQ);
                        jumpsToBody.Add(EmitJump((byte)OpCode.OP_JUMP_IF_TRUE));
                        break;

                    case CaseRange cr:
                        // Logica per: selectValue >= startValue AND selectValue <= endValue
                        // 1. Controlla selectValue >= startValue
                        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)selectExprIndex);
                        VisitExpression(cr.Start);
                        EmitByte((byte)OpCode.OP_CMP_GTE);
                        var jumpIfLess = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);

                        // 2. Controlla selectValue <= endValue (se il primo test è passato)
                        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)selectExprIndex);
                        VisitExpression(cr.End);
                        EmitByte((byte)OpCode.OP_CMP_LTE);
                        PatchJump(jumpIfLess);

                        jumpsToBody.Add(EmitJump((byte)OpCode.OP_JUMP_IF_TRUE));
                        break;

                    case CaseComparison cc:
                        VisitExpression(cc.Value);
                        var opCode = cc.Operator.Kind switch
                        {
                            TokenKind.Equal => OpCode.OP_CMP_EQ,
                            TokenKind.NotEqual => OpCode.OP_CMP_NE,
                            TokenKind.GreaterThan => OpCode.OP_CMP_GT,
                            TokenKind.GreaterEqual => OpCode.OP_CMP_GTE,
                            TokenKind.LessThan => OpCode.OP_CMP_LT,
                            TokenKind.LessEqual => OpCode.OP_CMP_LTE,
                            _ => throw new InvalidOperationException(
                                $"Operatore non supportato in SELECT CASE: {cc.Operator.Kind}")
                        };
                        EmitByte((byte)opCode);
                        jumpsToBody.Add(EmitJump((byte)OpCode.OP_JUMP_IF_TRUE));
                        break;
                }
            }

            jumpToNextCase =
                EmitJump((byte)OpCode.OP_JUMP_FORWARD); // Salta al prossimo CASE se nessun test di questo passa
            PatchJumps(jumpsToBody); // Patcha tutti i test di questo CASE per saltare al corpo

            BeginScope();
            foreach (var caseStmt in caseClause.Statements)
            {
                VisitStatement(caseStmt);
            }

            EndScope();

            if (_isReachable)
            {
                endJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
            }

            isAnyClauseReachable |= _isReachable;
            _isReachable = false;
        }

        if (jumpToNextCase != -1)
        {
            PatchJump(jumpToNextCase);
            _isReachable = true;
        }

        if (stmt.ElseClause is not null)
        {
            BeginScope();
            foreach (var elseStmt in stmt.ElseClause.Statements)
            {
                VisitStatement(elseStmt);
            }

            EndScope();
            isAnyClauseReachable |= _isReachable;
        }
        else
        {
            isAnyClauseReachable = true; // Il path che salta tutti i case è raggiungibile
        }

        PatchJumps(endJumps);
        _isReachable = isAnyClauseReachable;

        EndScope(); // Rimuove la variabile temporanea __select_expr
    }

    /// <summary>
    /// Compila un'istruzione GOTO.
    /// </summary>
    private void VisitGotoStatement(GotoStmt stmt)
    {
        var instructionIp = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_GOTO);
        var patchOffset = _chunk.Code.Count;
        EmitShort(0); // Placeholder per l'indirizzo assoluto
        _explicitControlTransfers.Add(new(instructionIp, patchOffset, nameof(OpCode.OP_GOTO)));

        if (stmt.Target.Kind == TokenKind.Number)
        {
            var lineNumber = (int)(double)(stmt.Target.Literal ?? 0);
            _lineNumberJumpsToPatch.Add((patchOffset, lineNumber));
        }
        else
        {
            _jumpsToPatch.Add((patchOffset, stmt.Target));
        }

        _isReachable = false; // Il codice dopo un GOTO è irraggiungibile.
    }

    /// <summary>
    /// Compila la definizione di un'etichetta.
    /// </summary>
    private void VisitLabelStatement(LabelStmt stmt)
    {
        var labelName = stmt.Label.Lexeme;
        if (_labels.ContainsKey(labelName))
        {
            if (!IsCompilerGeneratedLabel(labelName))
            {
                throw new Exception($"Duplicate label: '{labelName}'.");
            }
        }

        _labels[labelName] = _chunk.Code.Count;
        // Patcha SUBITO i salti in AVANTI pendenti verso questa etichetta (→ posizione corrente).
        // ROBUSTO all'omonimia: `_labels` è un dict per NOME e un nome sintetico può ripetersi nel
        // chunk (es. lo stesso metodo emesso due volte) → risolvere alla FINE legava TUTTI i salti
        // all'ULTIMA occorrenza (bug: il salto d'uscita del WHILE in `Go` puntava all'etichetta di
        // un'altra copia → le scritture dopo il loop si perdevano). Qui ogni salto si lega
        // all'occorrenza IMMEDIATAMENTE precedente, che è quella corretta nel suo flusso.
        for (int i = _jumpsToPatch.Count - 1; i >= 0; i--)
        {
            if (string.Equals(_jumpsToPatch[i].Item2.Lexeme, labelName, StringComparison.OrdinalIgnoreCase))
            {
                var patchAt = _jumpsToPatch[i].Item1;
                // La codifica dipende dall'opcode, come in PatchAllJumps: GOTO/GOSUB/... portano un indirizzo ASSOLUTO, i salti
                // condizionati/FORWARD un offset RELATIVO. Scrivere sempre l'offset relativo (PatchJump) rompeva ogni GOTO in avanti:
                // l'indirizzo finiva vicino a 0 e il programma ripartiva da capo (PCode e VM/C in loop su `GOTO Salto`).
                if (patchAt > 0 && IsAbsoluteAddressOpcode((OpCode)_chunk.Code[patchAt - 1]))
                {
                    var abs = _chunk.Code.Count;
                    if (abs > ushort.MaxValue) throw new Exception("Target address exceeds 65535 bytes.");
                    _chunk.Code[patchAt] = (byte)((abs >> 8) & 0xFF);
                    _chunk.Code[patchAt + 1] = (byte)(abs & 0xFF);
                }
                else
                {
                    PatchJump(patchAt);
                }
                _jumpsToPatch.RemoveAt(i);
            }
        }
        // Riga dell'etichetta per RESTORE <etichetta> (solo etichette utente, non sintetiche).
        if (!IsCompilerGeneratedLabel(labelName))
            _labelLineByLabel[labelName] = stmt.Label.Position.Line;
        _isReachable = true;
    }

    /// <summary>
    /// Compila un'istruzione JumpStmt.
    /// Questa versione è stata aggiornata per ignorare i salti ridondanti
    /// che seguono un'istruzione NEXT in un ciclo FOR ottimizzato.
    /// </summary>
    private void VisitJumpStatement(JumpStmt stmt)
    {
        // Se il flag _ignoreNextJump è attivo (impostato da VisitNextStatement),
        // significa che questo JUMP è ridondante e va saltato.
        if (_ignoreNextJump)
        {
            _ignoreNextJump = false; // Resetta il flag
            return; // Non emettere P-Code per questo salto.
        }

        // Se non è un JUMP da ignorare, procedi con la logica standard.
        if (_labels.TryGetValue(stmt.TargetLabel.Lexeme, out var targetAddress))
        {
            EmitLoop(targetAddress);
        }
        else
        {
            var jumpLocation = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
            _jumpsToPatch.Add((jumpLocation, stmt.TargetLabel));
        }
    }

    /// <summary>
    /// Applica le patch agli indirizzi dei salti che puntano a numeri di riga.
    /// </summary>
    private void PatchAllLineNumberJumps()
    {
        foreach (var (patchOffset, targetLine) in _lineNumberJumpsToPatch)
        {
            if (_lineNumberAddresses.TryGetValue(targetLine, out var targetAddress))
            {
                if (targetAddress > ushort.MaxValue) throw new Exception("Target address exceeds 65535 bytes.");
                _chunk.Code[patchOffset] = (byte)((targetAddress >> 8) & 0xFF);
                _chunk.Code[patchOffset + 1] = (byte)(targetAddress & 0xFF);
            }
            else
            {
                if (_chunk.Code.Count <= patchOffset + 1) continue;
                _chunk.Code[patchOffset] = 0;
                _chunk.Code[patchOffset + 1] = 0;
            }
        }
    }

    /// <summary>
    /// Compila un'istruzione ConditionalJumpStmt.
    /// QUESTA VERSIONE È STATA OTTIMIZZATA PER GESTIRE SALTI CONDIZIONALI ALL'INDIETRO,
    /// tipici dei cicli DO/WHILE, generando P-Code più efficiente.
    /// </summary>
    private void VisitConditionalJumpStatement(ConditionalJumpStmt stmt)
    {
        VisitExpression(stmt.Condition);

        // Se l'etichetta di destinazione è già definita, si tratta di un salto all'indietro (un loop).
        if (_labels.TryGetValue(stmt.TargetLabel.Lexeme, out var targetAddress))
        {
            // Salto all'indietro (es. DO...LOOP WHILE cond, WHILE cond...WEND)
            // La VM non ha opcode come OP_JUMP_IF_TRUE_BACKWARD.
            // La soluzione è invertire la logica e usare un salto in avanti per scavalcare
            // un salto incondizionato all'indietro.
            // "IF cond GOTO indietro"  diventa "IF NOT cond GOTO avanti_poco"

            // Invertiamo la condizione per il salto in avanti che scavalca.
            var invertedOpCode = stmt.JumpWhenTrue ? OpCode.OP_JUMP_IF_FALSE : OpCode.OP_JUMP_IF_TRUE;

            // Salta l'istruzione OP_JUMP_BACKWARD se la condizione invertita è vera.
            var jumpOverBackwardJump = EmitJump((byte)invertedOpCode);

            // Se la condizione non è stata invertita, esegui il salto all'indietro.
            EmitLoop(targetAddress);

            // Patcha il salto che scavalca per atterrare qui, subito dopo il JUMP_BACKWARD.
            PatchJump(jumpOverBackwardJump);
        }
        else
        {
            // Salto in avanti standard (es. IF...THEN)
            var opCode = stmt.JumpWhenTrue ? OpCode.OP_JUMP_IF_TRUE : OpCode.OP_JUMP_IF_FALSE;
            var jumpLocation = EmitJump((byte)opCode);
            _jumpsToPatch.Add((jumpLocation, stmt.TargetLabel));
        }
    }

    /// <summary>
    /// Metodo da chiamare alla fine della compilazione per risolvere tutti i salti
    /// che puntano a etichette o procedure.
    /// </summary>
    /// <summary>Opcode il cui operando a 16 bit e' un indirizzo ASSOLUTO (gli altri salti usano un offset relativo).</summary>
    private static bool IsAbsoluteAddressOpcode(OpCode op) =>
        op is OpCode.OP_GOTO or OpCode.OP_GOSUB or OpCode.OP_CALL or OpCode.OP_CALL_N or OpCode.OP_CALL_CTOR or OpCode.OP_GO or
              OpCode.OP_SET_ERR_HANDLER_GOTO or OpCode.OP_SET_ERR_HANDLER_GOSUB or OpCode.OP_RESUME or OpCode.OP_NEW_GENERATOR_CALL or
              OpCode.OP_GO_FUNCTION;

    private void PatchAllJumps()
    {
        foreach (var (patchOffset, targetToken) in _jumpsToPatch)
        {
            // Determina l'opcode leggendo a ritroso dalla posizione di patching.
            // Un'istruzione di salto ha 1 byte di opcode e 2 byte di operando.
            // 'patchOffset' punta al primo byte dell'operando, quindi l'opcode è a 'patchOffset - 1'.
            var instructionOffset = patchOffset - 1;
            if (instructionOffset < 0) continue; // Sicurezza

            var instruction = (OpCode)_chunk.Code[instructionOffset];

            // Risolve l'indirizzo di destinazione.
            var resolvedAddress = false;
            int targetAddress = 0;
            // 2026-09-24: per-Simbolo (@sym{id}) — disambigua le omonime in namespace diversi
            // (il chunk non conserva il namespace). Fallback a "Nome#k", poi al nome nudo.
            if (targetToken.Literal is int symId
                && _procedureAddresses.TryGetValue($"@sym{symId}", out var symAddr))
            {
                targetAddress = symAddr;
                resolvedAddress = true;
            }
            if (!resolvedAddress
                && !_labels.TryGetValue(targetToken.Lexeme, out targetAddress)
                && !_procedureAddresses.TryGetValue(targetToken.Lexeme, out targetAddress))
            {
                // 2026-09-24 (overload): chiave con arità assente (overload+optional/
                // variadic, siti non convertiti) → fallback al nome nudo, cioè il
                // comportamento precedente (ultimo registrato). Mai un nuovo silenzio.
                var lexeme = targetToken.Lexeme;
                var hashIdx = lexeme.LastIndexOf('#');
                if (hashIdx <= 0 || !_procedureAddresses.TryGetValue(lexeme.Substring(0, hashIdx), out targetAddress))
                {
                    if (instruction == OpCode.OP_SET_ERR_HANDLER_GOTO &&
                        targetToken is { Kind: TokenKind.Number, Literal: double and 0 })
                    {
                        targetAddress = 0;
                    }
                    else
                    {
                        targetAddress = 0;
                    }
                }
            }

            // Controlla se l'opcode richiede un indirizzo assoluto o un offset relativo.
            if (IsAbsoluteAddressOpcode(instruction))
            {
                // Per questi opcode, l'operando è un indirizzo assoluto a 16 bit.
                if (targetAddress > ushort.MaxValue)
                    throw new Exception("Target address exceeds 65535 bytes.");

                _chunk.Code[patchOffset] = (byte)((targetAddress >> 8) & 0xFF);
                _chunk.Code[patchOffset + 1] = (byte)(targetAddress & 0xFF);
            }
            else // Altrimenti, è un salto relativo (OP_JUMP_FORWARD, OP_JUMP_IF_TRUE, etc.)
            {
                // L'offset del salto è calcolato dalla posizione *dopo* l'intera istruzione di salto.
                var jump = targetAddress - (patchOffset + 2);

                if (jump < 0)
                {
                    // --- Gestione automatica del salto all'indietro ---
                    if (instruction == OpCode.OP_JUMP_FORWARD)
                    {
                        _chunk.Code[instructionOffset] = (byte)OpCode.OP_JUMP_BACKWARD;
                        jump = (patchOffset + 2) - targetAddress;
                    }
                    else
                    {
                        throw new Exception(
                            $"Backward jump ({jump}) calculated for a non-JUMP_FORWARD instruction ({instruction}) at offset {instructionOffset}. Target: {targetToken.Lexeme} (@{targetAddress}).");
                    }
                }

                if (jump > ushort.MaxValue)
                    throw new Exception("Code jump offset exceeds 65535 bytes.");

                _chunk.Code[patchOffset] = (byte)((jump >> 8) & 0xFF);
                _chunk.Code[patchOffset + 1] = (byte)(jump & 0xFF);
            }
        }
    }

    private void PatchAbsoluteAddressTargets()
    {
        foreach (var (patchOffset, target) in _absoluteAddressPatches)
        {
            if (!_labels.TryGetValue(target.Lexeme, out var targetAddress) &&
                !_procedureAddresses.TryGetValue(target.Lexeme, out targetAddress))
                targetAddress = 0;
            if (targetAddress > ushort.MaxValue)
                throw new Exception("Target address exceeds 65535 bytes.");
            _chunk.Code[patchOffset] = (byte)((targetAddress >> 8) & 0xFF);
            _chunk.Code[patchOffset + 1] = (byte)(targetAddress & 0xFF);
        }
    }

    /// <summary>
    /// Impedisce a GOTO/GOSUB di entrare in una regione protetta senza averne eseguito il
    /// prologo. Uscire da una regione resta legale ed è gestito dall'unwind della PCodeVM;
    /// un salto interno resta legale solo dentro la stessa sezione TRY, CATCH o FINALLY.
    /// </summary>
    private void ValidateExplicitControlTransfers()
    {
        foreach (var transfer in _explicitControlTransfers)
        {
            if (transfer.TargetOperandIp < 0 || transfer.TargetOperandIp + 1 >= _chunk.Code.Count)
                throw new InvalidOperationException(
                    $"P-Code control transfer truncated at {transfer.InstructionIp}.");

            var targetIp = (_chunk.Code[transfer.TargetOperandIp] << 8) |
                           _chunk.Code[transfer.TargetOperandIp + 1];
            foreach (var (tryIp, region) in _chunk.TryRegions)
            {
                var protectedStartIp = tryIp + 3; // OP_PUSH_TRY + operando U16
                var handlerOffset = (_chunk.Code[tryIp + 1] << 8) | _chunk.Code[tryIp + 2];
                var handlerIp = protectedStartIp + handlerOffset;
                var sourceSection = ProtectedSection(
                    transfer.InstructionIp, protectedStartIp, handlerIp, region);
                var targetSection = ProtectedSection(targetIp, protectedStartIp, handlerIp, region);

                if (targetSection != 0 &&
                    (transfer.RequireUnprotectedTarget || sourceSection != targetSection))
                {
                    throw new InvalidOperationException(
                        $"Illegal {transfer.Operation} from bytecode {transfer.InstructionIp} into a protected " +
                        $"TRY/CATCH/FINALLY region at {targetIp}.");
                }
            }
        }
    }

    private static int ProtectedSection(
        int ip,
        int protectedStartIp,
        int handlerIp,
        TryRegionMetadata region)
    {
        if (ip >= protectedStartIp && ip < handlerIp) return 1;
        if (ip >= handlerIp && ip < region.FinallyIp) return 2;
        if (ip >= region.FinallyIp && ip < region.EndIp) return 3;
        return 0;
    }

    /// <summary>
    /// Metodo helper per patchare una lista di salti allo stesso indirizzo.
    /// </summary>
    private void PatchJumps(List<int> jumps)
    {
        foreach (var jump in jumps)
        {
            PatchJump(jump);
        }

        jumps.Clear();
    }

    /// <summary>
    /// Sostituisce un indirizzo segnaposto con una destinazione calcolata.
    /// </summary>
    private void PatchJump(int offset, int? targetAddress = null)
    {
        var jump = (targetAddress ?? _chunk.Code.Count) - offset - 2;

        if (jump > ushort.MaxValue)
            throw new Exception("Code jump exceeds 65535 bytes.");

        _chunk.Code[offset] = (byte)((jump >> 8) & 0xFF);
        _chunk.Code[offset + 1] = (byte)(jump & 0xFF);
    }

    /// <summary>
    /// Compila un'istruzione ON...GOTO/GOSUB, generando un'istruzione P-Code
    /// che la VM userà per eseguire un salto calcolato.
    /// </summary>
    /// <param name="stmt">Il nodo AST OnJumpStmt da compilare.</param>
    private void VisitOnJumpStatement(OnJumpStmt stmt)
    {
        // 1. Compila l'espressione dell'indice e lascia il suo valore sulla stack.
        // La VM si aspetterà di trovare questo valore (convertito in intero)
        // in cima alla stack quando eseguirà OP_ON_JUMP.
        VisitExpression(stmt.IndexExpression);

        // 2. Emette l'opcode principale per il salto calcolato.
        var instructionIp = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_ON_JUMP);

        // 3. Emette il tipo di salto come primo operando. Questo dice alla VM
        // se deve eseguire un GOTO (salto semplice) o un GOSUB (pushando l'indirizzo di ritorno).
        EmitByte((byte)stmt.Type); // JumpType.Goto o JumpType.Gosub

        // 4. Emette il numero di etichette di destinazione come secondo operando.
        // Questo permette alla VM di sapere quanti indirizzi a 16 bit leggere successivamente.
        EmitByte((byte)stmt.TargetLabels.Length);

        // 5. Itera su ogni etichetta di destinazione ed emette un placeholder a 16 bit
        // per il suo indirizzo. Questi placeholder verranno risolti alla fine della
        // compilazione dalla fase di patching.
        foreach (var target in stmt.TargetLabels)
        {
            var patchOffset = _chunk.Code.Count;
            EmitShort(0); // Placeholder di 2 byte per l'indirizzo assoluto.
            _explicitControlTransfers.Add(new(
                instructionIp,
                patchOffset,
                stmt.Type == JumpType.Gosub ? "OP_ON_JUMP_GOSUB" : "OP_ON_JUMP_GOTO"));

            // Aggiunge il placeholder alla lista di patching appropriata, a seconda
            // che il target sia un numero di riga o un'etichetta nominale.
            if (target.Kind == TokenKind.Number)
            {
                var lineNumber = (int)(double)(target.Literal ?? 0);
                _lineNumberJumpsToPatch.Add((patchOffset, lineNumber));
            }
            else
            {
                _absoluteAddressPatches.Add((patchOffset, target));
            }
        }
    }
}
