// FILE: Interpreter.Compiler.Compiler.Others.cs

using System.Diagnostics.CodeAnalysis;


namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Mappa centralizzata per tutte le funzioni statiche (namespaced).
    /// Chiave: (Namespace, NomeFunzione), Valore: (Indice Namespace, Indice Funzione)
    /// </summary>
    private static readonly Dictionary<(string, string), (byte, byte)> StaticCallMap =
        new(new TupleStringComparer())
        {
            // Namespace 0: JSON
            { ("JSON", "PARSE"), (0, 1) },
            { ("JSON", "STRINGIFY"), (0, 2) },
            { ("JSON", "LOAD"), (0, 3) },

            // Namespace 1: KEY
            { ("KEY", "MAKE"), (1, 0) }
        };

    /// <summary>
    /// Comparer personalizzato privato per dizionari con chiavi di tipo (string, string)
    /// che esegue un confronto case-insensitive.
    /// </summary>
    private class TupleStringComparer : IEqualityComparer<(string, string)>
    {
        public bool Equals((string, string) x, (string, string) y)
        {
            return x.Item1.Equals(y.Item1, StringComparison.OrdinalIgnoreCase) &&
                   x.Item2.Equals(y.Item2, StringComparison.OrdinalIgnoreCase);
        }

        public int GetHashCode([DisallowNull] (string, string) obj)
        {
            return StringComparer.OrdinalIgnoreCase.GetHashCode(obj.Item1) ^
                   StringComparer.OrdinalIgnoreCase.GetHashCode(obj.Item2);
        }
    }

    /* SubCallStmt rimosso dall'AST — mantenere commentato
    private void VisitSubCallStatement(SubCallStmt stmt) { }
    */

    private void VisitSwapStatement(SwapStmt stmt)
    {
        // Per SWAP, passiamo gli indici delle variabili (locali o globali)
        // al runtime per eseguire lo scambio.
        var leftIsLocal = ResolveLocal(GetTargetToken(stmt.Left).Lexeme) != -1;
        var rightIsLocal = ResolveLocal(GetTargetToken(stmt.Right).Lexeme) != -1;

        EmitConstant(leftIsLocal); // Pusha un booleano
        EmitConstant((double)ResolveVariableIndex(stmt.Left));
        EmitConstant(rightIsLocal); // Pusha un booleano
        EmitConstant((double)ResolveVariableIndex(stmt.Right));

        // NOTA: La syscall SWAP ora si aspetta 4 argomenti
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SWAP); EmitByte((byte)4);
    }

    private void VisitEndStatement(EndStmt stmt)
    {
        EmitByte((byte)OpCode.OP_HALT);
        _isReachable = false; // Il codice dopo un END è irraggiungibile.
    }

    private void VisitRandomizeStatement(RandomizeStmt stmt)
    {
        byte argCount;
        if (stmt.Seed is not null)
        {
            VisitExpression(stmt.Seed);
            argCount = 1;
        }
        else
        {
            // La VM deve sapere che non ci sono argomenti espliciti,
            // ma deve comunque aspettarsi un valore sulla stack (NIL) per coerenza.
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            argCount = 1;
        }

        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RANDOMIZE); EmitByte((byte)(argCount));
    }

    /// <summary>
    /// Compila SYSTEM.SHELL "comando" (o legacy SHELL) come OP_CALL_BUILTIN SHELL.
    /// Il runtime (PCodeProcessor.Builtins SHELL) esegue con ParkBlocking (GPM §0ter).
    /// </summary>
    private void VisitSystemShellStatement(SystemShellStmt stmt)
    {
        VisitExpression(stmt.Command);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.SHELL);
        EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_POP); // scarta il valore di ritorno (double)
    }

    private void VisitSleepStatement(SleepStmt stmt)
    {
        VisitExpression(stmt.Duration);
        EmitByte((byte)OpCode.OP_SLEEP);
    }

    private void VisitLockStatement(MutexLockStmt stmt)
    {
        VisitExpression(stmt.Mutex);
        EmitByte((byte)OpCode.OP_LOCK);
    }

    private void VisitUnlockStatement(MutexUnlockStmt stmt)
    {
        VisitExpression(stmt.Mutex);
        EmitByte((byte)OpCode.OP_UNLOCK);
    }

    private void VisitCondWaitStatement(CondWaitStmt stmt)
    {
        // Parità Go c.Wait(): pusha (cond, mutex), OP_COND_WAIT rilascia il mutex e parcheggia;
        // il wake riparte sull'OP_LOCK successivo che riacquisisce il mutex (bloccandosi se tenuto).
        VisitExpression(stmt.Cond);
        VisitExpression(stmt.Mutex);
        EmitByte((byte)OpCode.OP_COND_WAIT);
        VisitExpression(stmt.Mutex);
        EmitByte((byte)OpCode.OP_LOCK);
    }

    private void VisitCondSignalStatement(CondSignalStmt stmt)
    {
        VisitExpression(stmt.Cond);
        EmitByte((byte)OpCode.OP_COND_SIGNAL);
    }

    private void VisitCondBroadcastStatement(CondBroadcastStmt stmt)
    {
        VisitExpression(stmt.Cond);
        EmitByte((byte)OpCode.OP_COND_BROADCAST);
    }

    private void VisitRLockStatement(MutexRLockStmt stmt)
    {
        VisitExpression(stmt.Mutex);
        EmitByte((byte)OpCode.OP_RLOCK);
    }

    private void VisitRUnlockStatement(MutexRUnlockStmt stmt)
    {
        VisitExpression(stmt.Mutex);
        EmitByte((byte)OpCode.OP_RUNLOCK);
    }

    private void VisitOnceStatement(OnceStmt stmt)
    {
        // Parità Go sync.Once.Do: il primo chiamante esegue la procedura, i chiamanti
        // concorrenti attendono il completamento e poi proseguono (senza ri-eseguirla).
        // Layout:
        //   [push once]
        //   [OP_ONCE_BEGIN] [skip_hi] [skip_lo]   ; NotRun→Running (prosegui) / Done→skip / Running→park
        //   [OP_CALL_N proc]                       ; esegue la procedura (solo il primo, arity 0)
        //   [OP_ONCE_END]                          ; segna Done e sveglia i waiter
        //   skip:                                  ; tutti proseguono da qui
        VisitExpression(stmt.Once);
        var beginPos = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_ONCE_BEGIN);
        EmitByte(0); // placeholder skip hi
        EmitByte(0); // placeholder skip lo
        // Chiamata diretta per NOME (arity 0): stesso patching differito di OP_CALL_N, così
        // la procedura può essere definita in qualsiasi punto del modulo.
        var callLoc = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_CALL_N);
        EmitByte(0xFF); // addr hi placeholder
        EmitByte(0xFF); // addr lo placeholder
        EmitByte(0);    // 0 argomenti (sync.Once.Do esegue f() senza argomenti)
        // 2026-09-24 (overload): patch con arità 0 (ONCE chiama sempre f()).
        _jumpsToPatch.Add((callLoc + 1, CallPatchToken(stmt.ProcedureName, null, 0)));
        EmitByte((byte)OpCode.OP_ONCE_END);
        var skipAddr = _chunk.Code.Count;
        // Patch lo skip dentro OP_ONCE_BEGIN.
        _chunk.Code[beginPos + 1] = (byte)(skipAddr >> 8);
        _chunk.Code[beginPos + 2] = (byte)skipAddr;
    }

    private void VisitDeferStatement(DeferStmt stmt)
    {
        // Parità Go defer: registra il body da eseguire a fine procedura (LIFO). Layout:
        //   [OP_DEFER] [addr_hi] [addr_lo]        ; registra l'indirizzo del body
        //   [OP_JUMP_FORWARD] [offset]             ; salta il body nel flusso normale
        //   [body DEFER]                           ; eseguito a fine procedura
        //   [OP_DEFER_RET]                         ; torna all'uscita della procedura
        var deferOpcodePos = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_DEFER);
        EmitByte(0); // placeholder addr hi
        EmitByte(0); // placeholder addr lo
        var skip = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
        var bodyAddr = _chunk.Code.Count;
        VisitStatement(stmt.Body);
        EmitByte((byte)OpCode.OP_DEFER_RET);
        // Patch l'indirizzo del body dentro OP_DEFER.
        _chunk.Code[deferOpcodePos + 1] = (byte)(bodyAddr >> 8);
        _chunk.Code[deferOpcodePos + 2] = (byte)bodyAddr;
        PatchJump(skip);
    }

    // ========================================================================
    // --- JSON Commands (implementati come SysCalls) ---
    // ========================================================================

    /// <summary>
    /// Compila un'istruzione JSON.WriteValue.
    /// </summary>
    private void VisitJsonWriteValueStatement(JsonWriteValueStmt stmt)
    {
        VisitExpression(stmt.Key);
        VisitExpression(stmt.Value);
        if (stmt.Section is not null) VisitExpression(stmt.Section);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.JSON_WRITE_VALUE); EmitByte((byte)3);
    }

    /// <summary>
    /// Compila un'istruzione JSON.WriteSection.
    /// </summary>
    private void VisitJsonWriteSectionStatement(JsonWriteSectionStmt stmt)
    {
        VisitExpression(stmt.Source);
        if (stmt.Section is not null) VisitExpression(stmt.Section);
        else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.JSON_WRITE_SECTION); EmitByte((byte)2);
    }

    /// <summary>
    /// Compila un'istruzione STOP.
    /// Emette un opcode OP_HALT per terminare l'esecuzione.
    /// </summary>
    private void VisitStopStatement(StopStmt stmt)
    {
        EmitByte((byte)OpCode.OP_HALT);
    }
    
    /// <summary>
    /// Compila un'istruzione RUN.
    /// Emette una syscall per eseguire un comando esterno.
    /// </summary>
    private void VisitRunStatement(RunStmt stmt)
    {
        VisitExpression(stmt.Command); // Compila l'espressione del comando
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RUN); EmitByte((byte)1);
    }
    
    /// <summary>
    /// Compila un'istruzione macro (&...).
    /// </summary>
    private void VisitMacroStatement(MacroStmt stmt)
    {
        // 1. Compila l'espressione che produce la stringa di codice.
        VisitExpression(stmt.Expression);
        // 2. Emette l'opcode che istruisce la VM a compilare ed eseguire la stringa.
        EmitByte((byte)OpCode.OP_EVAL_MACRO_STMT);
        // S_eval: cattura le locali vive al sito (incluse quelle di blocco IF/FOR).
        RegisterMacroSiteLocalNames();
    }

    private void VisitWithStatement(WithStmt stmt)
    {
        // Valuta l'espressione WITH e la salva in un globale temporaneo
        VisitExpression(stmt.Expression);
        var withGlobalIdx = DefineGlobal("__with_obj");
        EmitByte((byte)OpCode.OP_STORE_GLOBAL);
        EmitShort(withGlobalIdx);

        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs).
        PreScanWholeBodyNativeFusions(stmt.Body);

        foreach (var s in stmt.Body)
        {
            VisitStatement(s);
        }
    }

    private void VisitMatchStatement(MatchStmt stmt)
    {
        // 1. Valuta l'espressione da matchare, salvala in __match_expr
        BeginScope();
        VisitExpression(stmt.Expression);
        var matchLocal = new Token(TokenKind.Identifier, "__match_expr", null, SourcePosition.Unknown);
        AddLocal(matchLocal);
        int matchIdx = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)matchIdx);
        
        var endJumps = new List<int>();
        int? skipToNextJump = null;
        
        foreach (var mc in stmt.Cases)
        {
            // Salta al prossimo CASE se il precedente non ha matchato
            if (skipToNextJump.HasValue)
            {
                PatchJump(skipToNextJump.Value);
                _isReachable = true;
            }
            
            bool isWildcard = mc.WildcardToken != null && mc.WildcardToken.Kind != TokenKind.Empty;
            bool isDefault = mc.ValueTest == null && mc.RangeTest == null && 
                             mc.ComparisonTest == null && mc.NullPattern == null &&
                             mc.TypePattern == null && mc.ValuePattern == null && !isWildcard;
            
            // Carica il valore da matchare (se non è default/wildcard)
            if (!isDefault && !isWildcard)
            {
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)matchIdx);
            }
            
            bool matched = false;
            
            // 2. Testa il pattern
            if (isWildcard || isDefault)
            {
                // CASE _ o DEFAULT — matcha sempre
                matched = true;
            }
            else if (mc.NullPattern != null)
            {
                // CASE IS NOTHING
                EmitByte((byte)OpCode.OP_PUSH_NIL);
                EmitByte((byte)OpCode.OP_CMP_EQ);
                skipToNextJump = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
                matched = true;
            }
            else if (mc.ValueTest != null)
            {
                // CASE value (semplice)
                VisitExpression(mc.ValueTest.Value);
                EmitByte((byte)OpCode.OP_CMP_EQ);
                skipToNextJump = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
                matched = true;
            }
            else if (mc.ValuePattern != null)
            {
                // CASE value AS var
                EmitByte((byte)OpCode.OP_POP);
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)matchIdx);
                VisitExpression(mc.ValuePattern.PatternValue);
                EmitByte((byte)OpCode.OP_CMP_EQ);
                skipToNextJump = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
                matched = true;
            }
            else if (mc.RangeTest != null)
            {
                // CASE start TO end
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)matchIdx);
                VisitExpression(mc.RangeTest.Start);
                EmitByte((byte)OpCode.OP_CMP_GTE);
                var jumpIfLess = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)matchIdx);
                VisitExpression(mc.RangeTest.End);
                EmitByte((byte)OpCode.OP_CMP_LTE);
                PatchJump(jumpIfLess);
                skipToNextJump = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
                matched = true;
            }
            else if (mc.ComparisonTest != null)
            {
                // CASE IS > val
                VisitExpression(mc.ComparisonTest.Value);
                var cmpOp = mc.ComparisonTest.Operator.Kind switch
                {
                    TokenKind.Equal => OpCode.OP_CMP_EQ,
                    TokenKind.NotEqual => OpCode.OP_CMP_NE,
                    TokenKind.GreaterThan => OpCode.OP_CMP_GT,
                    TokenKind.GreaterEqual => OpCode.OP_CMP_GTE,
                    TokenKind.LessThan => OpCode.OP_CMP_LT,
                    TokenKind.LessEqual => OpCode.OP_CMP_LTE,
                    _ => OpCode.OP_CMP_EQ
                };
                EmitByte((byte)cmpOp);
                skipToNextJump = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
                matched = true;
            }
            else if (mc.TypePattern != null)
            {
                EmitByte((byte)OpCode.OP_POP);
                matched = true;
            }
            
            if (matched)
            {
                // 3. Se c'è una variabile di binding, assegna il valore
                var bindingVar = mc.ValuePattern?.VariableName ?? mc.TypePattern?.VariableName;
                if (bindingVar != null)
                {
                    EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)matchIdx);
                    AssignVariable(new VariableTarget(-1, bindingVar, bindingVar), isLet: false);
                }
                
                // 4. Esegui il corpo
                foreach (var s in mc.Statements)
                {
                    VisitStatement(s);
                }
                
                // 5. Salta alla fine del MATCH (no fall-through)
                endJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
            }
            else
            {
                // Nessun pattern matchato — pulisci la stack
                EmitByte((byte)OpCode.OP_POP);
            }
        }
        
        // Patcha i salti alla fine
        foreach (var j in endJumps)
            PatchJump(j);
        
        EndScope();
    }

    private void VisitForEachStatement(ForEachStmt stmt)
    {
        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs):
        // un FOR annidato dentro il corpo di un FOR EACH va comunque pre-scansionato.
        PreScanWholeBodyNativeFusions(stmt.Body);
        foreach (var bodyStmt in stmt.Body)
            VisitStatement(bodyStmt);
        _feSlots.TryPop(out _);
        var exitPos = _chunk.Code.Count;
        EmitByte((byte)OpCode.OP_POP);
        foreach (var bodyStmt in stmt.Body)
            if (bodyStmt is ForEachAdvanceAndAssignStmt fea)
                _labels[fea.ExitLabel.Lexeme] = exitPos;
    }

    private void VisitForEachInitStatement(ForEachInitStmt stmt)
    {
        foreach (var v in stmt.Variables)
            AddLocal(v, new InferredType());
        VisitExpression(stmt.CollectionExpr);
        // Dynamic temp slots for counter and array ref (avoids nested loop corruption)
        var ctrSlot = _locals.Count;
        AddLocal(new Token(TokenKind.Identifier, "__ctr", null, stmt.CollectionExpr.Position));
        var arrSlot = _locals.Count;
        AddLocal(new Token(TokenKind.Identifier, "__arr", null, stmt.CollectionExpr.Position));
        // Distinzione dict-like calcolata QUI (una volta) e propagata a
        // VisitForEachAdvanceAndAssignStatement. Determinato qui a compile-time (una volta sola)
        // invece che ad ogni iterazione: senza questa distinzione un FOR EACH idx, v IN arr()
        // tentava SEMPRE l'estrazione .Key/.Value (via OP_LOAD_FIELD) sull'elemento posizionale
        // grezzo dell'array — silenziosamente null su un valore non-IDictionary (bug scoperto
        // con ForEachComprehensive.bas, sezione "Indice + valore su array").
        var collType = CompilerInferExpressionType(stmt.CollectionExpr);
        var isChannel = collType is ChannelType;
        var isDictLike = collType is DictType or Basic.JsonType;
        if (isChannel)
        {
            // FOR EACH v IN ch (parità Go for v := range ch): il loop riceve dal canale finché
            // chiuso. Il canale (già sullo stack da VisitExpression) va salvato in arrSlot;
            // ctrSlot è riusato come slot del flag OK. Niente contatore/LBOUND/UBOUND.
            EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)arrSlot);
            _feSlots.Push((arrSlot, ctrSlot, false, false, true, stmt.Variables.Length));
            return;
        }
        // Matrice multi-rank: il valore di iterazione è la RIGA (array 1D) — con N variabili
        // scalari viene destrutturata in elementi (FOR EACH a, b IN m(2,2) → a=m(i,0), b=m(i,1)).
        // Matrice = ArrayType annidato (ElementType è a sua volta ArrayType): DIM m(2,2) AS INTEGER
        // risolve il tipo dell'espressione a ArrayType(ArrayType(Integer)) nel linker.
        var isMatrix = collType is ArrayType { ElementType: ArrayType };
        _feSlots.Push((arrSlot, ctrSlot, isDictLike, isMatrix, false, stmt.Variables.Length));
        EmitByte((byte)OpCode.OP_DUP);
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)arrSlot);
        // Inizializza contatore a LBOUND(arr) invece di 0 — supporta array 1-based
        EmitByte((byte)OpCode.OP_DUP);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.LBOUND);
        EmitByte((byte)(1));
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)ctrSlot);
        // La collezione originale pushata da VisitExpression sopra resta sullo stack, ormai
        // inutile: da qui in poi il loop legge SEMPRE arrSlot (mai più questo valore diretto,
        // vedi VisitForEachAdvanceAndAssignStatement). Senza questo POP resta un riferimento
        // PERMANENTE sullo stack VM dopo ogni FOR EACH — ForEachStmt/VisitForEachStatement
        // (che avrebbe un OP_POP simmetrico) è dead code: il Parser non costruisce mai quel
        // wrapper per "FOR EACH ... NEXT" (produce sempre la sequenza piatta EnterLoopStmt/
        // ForEachInitStmt/.../ExitLoopStmt, senza pop compensativo). Bug scoperto così: un
        // secondo DIM x AS DICT = {...} dopo un FOR EACH su un ALTRO dict trovava quel
        // riferimento leaked in cima allo stack e lo scambiava per l'istanza-da-popolare che
        // OP_MK_DICT usa per DIM x AS REDBLACK/TREE/AVL = {...} — il nuovo dict finiva per
        // essere un ALIAS del vecchio (stesso oggetto, entrambe le variabili con le entry
        // sommate). Riproducibile con due DICT diversi in una SUB, secondo FOR EACH dopo il
        // primo: vedi Executor/Samples/Loops/ForEachComprehensive.bas, sezioni 4b-4d.
        EmitByte((byte)OpCode.OP_POP);
    }

    private void VisitForEachAdvanceAndAssignStatement(ForEachAdvanceAndAssignStmt stmt)
    {
        if (!_feSlots.TryPeek(out var fe))
            throw new Exception("FOR EACH slot stack underflow");
        var (arrSlot, ctrSlot, isDictLike, isMatrix, isChannel, feVarCount) = fe;
        if (isChannel)
        {
            // FOR EACH v IN ch (parità Go for v := range ch): RECV_OK pusha (valore, flag).
            // ctrSlot = flag OK: 0 quando il canale è chiuso e svuotato → EXIT.
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)arrSlot);
            EmitByte((byte)OpCode.OP_RECV_OK);
            EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)ctrSlot); // flag
            var chVarName = stmt.Variables[0].Lexeme;
            var chLocalIdx = ResolveLocal(chVarName);
            if (chLocalIdx != -1) { EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)chLocalIdx); } else EmitByte((byte)OpCode.OP_POP);
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)ctrSlot);
            EmitConstant(0.0);
            EmitByte((byte)OpCode.OP_CMP_EQ);
            var chExitJump = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
            _jumpsToPatch.Add((chExitJump, stmt.ExitLabel));
            return;
        }
        // Exit check: if counter > UBOUND(arr)
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)ctrSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)arrSlot);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.UBOUND);
        EmitByte((byte)(1));
        EmitByte((byte)OpCode.OP_CMP_GT);
        var exitJump = EmitJump((byte)OpCode.OP_JUMP_IF_TRUE);
        _jumpsToPatch.Add((exitJump, stmt.ExitLabel));

        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)arrSlot);
        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)ctrSlot);
        // OP_LOAD_ELEMENT_POSITIONAL (non OP_LOAD_ELEMENT): ctrSlot è un contatore di iterazione,
        // non una chiave reale — su un IDictionary deve leggere la N-esima entry in ordine di
        // enumerazione, non cercare la chiave letterale N. Vedi il commento sull'opcode in
        // OpCode.cs. Porting da GMBasicAI (commit 701da5e).
        EmitByte((byte)OpCode.OP_LOAD_ELEMENT_POSITIONAL);
        EmitByte((byte)(1));

        var vName = stmt.Variables[0].Lexeme;
        var localIdx = ResolveLocal(vName);
        if (localIdx != -1)
        {
            // Matrice con N>=3 variabili (FOR EACH a, b, c IN m(3,3)): l'elemento di
            // iterazione è la RIGA (array 1D) — OP_FE_DESTRUCT la destruttura a runtime in
            // N elementi (a = riga[0], b = riga[1], c = riga[2]). Con 1-2 variabili la
            // semantica standard (indice, valore/riga) resta invariata.
            if (stmt.Variables.Length >= 3 && !isDictLike)
            {
                EmitByte((byte)OpCode.OP_FE_DESTRUCT);
                EmitByte((byte)stmt.Variables.Length);
            }
            if (stmt.Variables.Length > 1 && isDictLike)
            {
                EmitByte((byte)OpCode.OP_DUP);
                // Extract Key → store to variable[0]
                var fnKeyIdx = AddConstant("Key");
                EmitByte((byte)OpCode.OP_LOAD_FIELD);
                EmitShort(fnKeyIdx);
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIdx);
                // Extract Value from remaining copy → store to variable[1]
                var v1Name = stmt.Variables[1].Lexeme;
                var v1Idx = ResolveLocal(v1Name);
                if (v1Idx >= 0)
                {
                    var fnValIdx = AddConstant("Value");
                    EmitByte((byte)OpCode.OP_LOAD_FIELD);
                    EmitShort(fnValIdx);
                    EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)v1Idx);
                }
            }
            else if (stmt.Variables.Length >= 3)
            {
                // Matrice destrutturata (FOR EACH a, b, c IN m): OP_FE_DESTRUCT ha pushato
                // gli elementi della riga con il PRIMO in cima. Assegna ogni variabile dal pop.
                for (var mi = 0; mi < stmt.Variables.Length; mi++)
                {
                    var vTok = stmt.Variables[mi];
                    var vSlot = ResolveLocal(vTok.Lexeme);
                    if (vSlot >= 0) { EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)vSlot); } else EmitByte((byte)OpCode.OP_POP);
                }
            }
            else if (stmt.Variables.Length > 1)
            {
                // Array/LIST: variabile[1] = valore (l'elemento già in cima allo stack da
                // OP_LOAD_ELEMENT_POSITIONAL sopra), variabile[0] = indice (ctrSlot).
                var v1Name = stmt.Variables[1].Lexeme;
                var v1Idx = ResolveLocal(v1Name);
                if (v1Idx >= 0) { EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)v1Idx); } else
                    EmitByte((byte)OpCode.OP_POP);
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)ctrSlot);
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIdx);
            }
            else
            {
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)localIdx);
            }
        }

        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)ctrSlot);
        EmitConstant(1.0);
        EmitByte((byte)OpCode.OP_ADD);
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)ctrSlot);
    }

    private void VisitYieldStatement(YieldStmt stmt)
    {
        VisitExpression(stmt.ValueToYield);
        EmitByte((byte)OpCode.OP_NEXT_GENERATOR);
    }

    private void VisitJsonSaveStatement(JsonSaveStmt stmt)
    {
        VisitExpression(stmt.Source);
        VisitExpression(stmt.FilePath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.JSON_SAVE); EmitByte((byte)2);
    }

    private void VisitRethrowStatement(RethrowStmt stmt)
    {
        EmitByte((byte)OpCode.OP_THROW);
    }

    private void VisitTupleAssignmentStatement(TupleAssignmentStmt stmt)
    {
        VisitExpression(stmt.TupleExpression);
        // BUG FIX (twin-port GMBasicAI): prima ogni variabile caricava SEMPRE l'elemento 0
        // (EmitConstant(0.0) fisso) — `(a, b) = (10, 20)` dava a=10, b=10 e lo swap
        // `(a, b) = (b, a)` restava rotto senza essere mai catturato (TypesTuple.bas non
        // aveva golden). Ora ogni variabile prende l'elemento i-esimo.
        for (var i = 0; i < stmt.VariableNames.Length; i++)
        {
            var varName = stmt.VariableNames[i];
            EmitByte((byte)OpCode.OP_DUP);
            EmitConstant((double)i);
            EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, 1);
            AssignVariable(new VariableTarget(-1,
                new Token(TokenKind.Identifier, varName.Lexeme, null, SourcePosition.Unknown),
                varName), false);
        }
        EmitByte((byte)OpCode.OP_POP);
    }

    private void VisitTupleDestructuringStatement(TupleDestructuringStmt stmt)
    {
        VisitExpression(stmt.TupleExpression);
        for (var i = 0; i < stmt.VariableNames.Length; i++)
        {
            var varName = stmt.VariableNames[i];
            EmitByte((byte)OpCode.OP_DUP);
            EmitConstant((double)i);
            EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, 1);
            AssignVariable(new VariableTarget(-1,
                new Token(TokenKind.Identifier, varName.Lexeme, null, SourcePosition.Unknown),
                varName), false);
        }
        EmitByte((byte)OpCode.OP_POP);
    }

    /// <summary>S_eval §ii: cerca un nome nella tabella host-procs (confronto
    /// case-insensitive come _procedureAddresses). Preferisce l'overload con arità
    /// esatta, altrimenti il primo omonimo (tolleranza come le chiamate normali —
    /// il runtime valida). Ritorna l'indice per OP_CALL_HOST.</summary>
    private bool FindExternalHostProc(string name, int argc, out int index)
    {
        index = -1;
        var table = ExternalHostProcs;
        if (table == null || string.IsNullOrEmpty(name)) return false;
        for (var i = 0; i < table.Count; i++)
            if (table[i].Arity == argc && string.Equals(table[i].Name, name, StringComparison.OrdinalIgnoreCase)) { index = i; return true; }
        for (var i = 0; i < table.Count; i++)
            if (string.Equals(table[i].Name, name, StringComparison.OrdinalIgnoreCase)) { index = i; return true; }
        return false;
    }

    private void VisitUnresolvedCallExpression(UnresolvedCallExpr expr)
    {
        // Chiamata a un GENERATOR PROCEDURE (`Count(3)`) che il Linker non ha risolto a un
        // FunctionExpr — arriva qui invece che in VisitFunctionExpression. Stessa intercettazione
        // (vedi lì per il commento completo): non deve eseguire il corpo, deve restituire un
        // handle sospeso via OP_NEW_GENERATOR_CALL.
        if (_procedureMap.TryGetValue((expr.Name.Lexeme, expr.Arguments.Length), out var uProc) &&
            uProc is GeneratorDefinition uGenDef)
        {
            foreach (var arg in expr.Arguments) VisitExpression(arg);
            var genCallLoc = EmitJump((byte)OpCode.OP_NEW_GENERATOR_CALL);
            _jumpsToPatch.Add((genCallLoc, uGenDef.Name));
            EmitByte((byte)expr.Arguments.Length);
            return;
        }

        foreach (var arg in expr.Arguments)
        {
            VisitExpression(arg);
        }
        // Cerca l'indirizzo della funzione tra le procedure compilate
        if (_procedureAddresses.TryGetValue(expr.Name.Lexeme, out var addr))
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
            EmitByte((byte)OpCode.OP_CALL);
            EmitByte((byte)(addr & 0xFF));
            EmitByte((byte)((addr >> 8) & 0xFF));
        }
        else if (FindExternalHostProc(expr.Name.Lexeme, expr.Arguments.Length, out var hostIdx))
        {
            // S_eval §ii: procedura dell'HOST (programma ospite della macro) — l'esecuzione
            // avviene per invocazione sincrona nel runtime ospite (PCodeVm sync-pump o
            // registro CLR su IL), mai con indirizzi del chunk macro. L'argc viaggia come
            // secondo operando (come OP_CALL_N): il runtime valida.
            if (expr.Arguments.Length > 255)
                throw new InvalidOperationException($"Macro chiama '{expr.Name.Lexeme}' con {expr.Arguments.Length} argomenti (max 255).");
            EmitByte((byte)OpCode.OP_CALL_HOST);
            EmitShort((ushort)hostIdx);
            EmitByte((byte)expr.Arguments.Length);
        }
        else
        {
            // MAI un no-op silenzioso: una chiamata qualificata (o semplice) a una funzione
            // inesistente è un ERRORE, non va compilata come OP_PUSH_NIL (prima CONSOLE.
            // SETWINDOWTITLE(...) girava "senza fare nulla" e mascherava il problema).
            // Diagnostico DISTINTO per esito: "simbolo non trovato" vs "simbolo ambiguo"
            // (vs namespace non importato / conflitto utente-builtin).
            throw new InvalidOperationException(UnresolvedCallDiagnostics.Build(expr));
        }
    }
}