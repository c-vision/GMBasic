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

        /* L'eccezione catturata è sullo stack (vedi OP_PUSH_TRY/OP_THROW: un'istanza CLASS è
         * un Dictionary con "__class__", un ERR/ERROR un Dictionary con Number/Description).
         * La si sposta SUBITO in un locale nascosto: il dispatch per tipo la legge più volte
         * (un test per clausola) senza consumarla. Nome unico per procedura: _locals non
         * viene mai accorciato, quindi il conteggio attuale è un identificatore stabile. */
        var excToken = new Token(TokenKind.Identifier, $"__exc_try_{_locals.Count}", null, stmt.Token.Position);
        AddLocal(excToken, new AnyType());
        int excSlot = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)excSlot);

        foreach (var catchClause in stmt.CatchClauses)
        {
            _isReachable = true;
            BeginScope();

            /* (1) TEST DI TIPO, prima del binding: una clausola `CATCH e AS <Classe>` corrisponde
             * se l'istanza è di quella classe o di una sua sottoclasse (il valore porta il nome
             * della classe CONCRETA: l'elenco è espanso a compile-time). `AS ERR` (ErrType) e
             * `CATCH` senza variabile sono catch-all, come in IL. Prima non c'era alcun test:
             * i corpi venivano emessi in sequenza e vinceva SEMPRE la prima clausola. */
            string? catchTypeName = null;
            bool isErrClause = false;
            if (catchClause.Variable is { } cvDecl && cvDecl.SymbolId >= 0 && _ctx is BasicProgram progCtx)
            {
                var vt = progCtx.Symbols.GetSymbol(cvDecl.SymbolId).DataType;
                if (vt is ClassType cct) catchTypeName = cct.Definition.Name.Lexeme;
                else if (vt is ErrType) isErrClause = true;
            }
            int? failedMatch = catchTypeName is not null ? EmitCatchTypeTest(excSlot, catchTypeName) : null;

            /* (2) BINDING: la variabile della clausola riceve l'eccezione prelevata dal locale. */
            if (catchClause.Variable is not null)
            {
                AddLocal(catchClause.Variable.NameCleaned);
                int exSlot = _locals.Count - 1;
                if (isErrClause)
                {
                    /* `CATCH e AS ERR`: la variabile è l'istanza ERR (Number/Description/LineNumber),
                     * NON il valore lanciato — per un'eccezione utente il valore è il dizionario
                     * della classe, che non ha Number (prima `e.Number` usciva vuoto su PCode/VM-C
                     * mentre IL dava 256). Il runtime aggiorna il globale ERR in OP_THROW/
                     * RaiseBasicError: si carica quello. */
                    if (TryResolveOrRegisterGlobal("ERR", out var errGlobal))
                    {
                        EmitByte((byte)OpCode.OP_LOAD_GLOBAL); EmitShort((ushort)errGlobal);
                    }
                    else
                    {
                        EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)excSlot);
                    }
                }
                else
                {
                    EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)excSlot);
                }
                EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)exSlot);
            }

            /* (3) WHEN (filtro aggiuntivo) + corpo. */
            int? whenFalse = null;
            if (catchClause.WhenCondition is not null)
            {
                VisitExpression(catchClause.WhenCondition);
                whenFalse = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            }

            foreach (var catchStmt in catchClause.Body)
            {
                VisitStatement(catchStmt);
            }

            anyCatchExitReachable |= _isReachable;

            catchExitJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));

            /* (4) Tipo o WHEN non soddisfatti → PROSSIMA clausola (non il FINALLY). */
            int nextClause = _chunk.Code.Count;
            if (whenFalse is { } wf) PatchJump(wf, nextClause);
            if (failedMatch is { } fm) PatchJump(fm, nextClause);

            EndScope();
        }

        /* Nessuna clausola ha corrisposto: RIPROPAGA l'eccezione (parità IL). Con zero CATCH
         * il TRY non aveva handler: si rilancia comunque, come prima (prima però il valore
         * restava sullo stack e veniva raggiunto dal POP_TRY/Finally: qui il rilancio è
         * esplicito e uniforme). */
        if (stmt.CatchClauses.Length > 0)
        {
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)excSlot);
            EmitByte((byte)OpCode.OP_THROW);
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

    /// <summary>
    /// Emette il test di tipo di una clausola `CATCH e AS <Classe>`: `TYPENAME(exc)` confrontato
    /// con il nome della classe <paramref name="target"/> e con TUTTE le classi che la ereditano
    /// (il valore porta il nome della classe CONCRETA; l'elenco è espanso a compile-time, così
    /// `CATCH e AS Base` cattura anche le sottoclassi, come il `isinst` del backend IL).
    /// Ritorna l'offset del salto che scatta quando nessun nome corrisponde: il chiamante lo
    /// patcha all'inizio della clausola SUCCESSIVA (dopo il corpo di questa).
    /// </summary>
    private int EmitCatchTypeTest(int excSlot, string target)
    {
        var names = new List<string>();
        if (_ctx is not null)
        {
            foreach (var cls in _ctx.GetAllClasses())
            {
                ClassDefinition? cur = cls;
                int guard = 0;
                bool match = false;
                while (cur is not null && guard++ < 64)
                {
                    if (NameMatchesClass(cur.Name.Lexeme, target)) { match = true; break; }
                    cur = cur.ResolvedBaseClass;
                }
                if (match) names.Add(cls.Name.Lexeme);
            }
        }
        if (names.Count == 0) names.Add(target);

        var matchJumps = new List<int>();
        foreach (var name in names.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)excSlot);
            EmitByte((byte)OpCode.OP_CALL_BUILTIN); EmitShort((ushort)BuiltInFunction.TYPENAME); EmitByte(1);
            EmitConstant(name);
            EmitByte((byte)OpCode.OP_CMP_EQ);
            var notThis = EmitJump((byte)OpCode.OP_JUMP_IF_FALSE);
            matchJumps.Add(EmitJump((byte)OpCode.OP_JUMP_FORWARD));
            PatchJump(notThis);   /* falso → prossimo candidato */
        }
        var failed = EmitJump((byte)OpCode.OP_JUMP_FORWARD);   /* nessun candidato → prossima clausola */
        foreach (var m in matchJumps) PatchJump(m);           /* corrisponde → corpo della clausola */
        return failed;
    }

    /// <summary>`Ns.Cls` e `Cls` sono lo stesso tipo: confronta anche l'ULTIMO segmento (il nome
    /// dell'istanza è quello dichiarato, eventualmente qualificato).</summary>
    private static bool NameMatchesClass(string a, string b)
    {
        if (a.Equals(b, StringComparison.OrdinalIgnoreCase)) return true;
        static string Last(string s) { var i = s.LastIndexOf('.'); return i >= 0 ? s[(i + 1)..] : s; }
        return Last(a).Equals(Last(b), StringComparison.OrdinalIgnoreCase);
    }
}
