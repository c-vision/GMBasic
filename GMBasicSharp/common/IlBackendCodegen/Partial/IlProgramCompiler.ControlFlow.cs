// FILE: IlBackend/Partial/IlProgramCompiler.ControlFlow.cs
// PARTIAL — dominio CONTROL FLOW (case/try-catch/throw/error/for/next/foreach/
// branch/jump/matrix/vector/ternary).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio CONTROL FLOW.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitCaseTestMatches(CaseTest test, LocalBuilder valueLocal, bool isString)
    {
        switch (test)
        {
            case CaseValue cv:
                _il.Emit(OpCodes.Ldloc, valueLocal);
                EmitExpression(cv.Value);
                EmitComparisonOnStackValues(TokenKind.Equal, isString);
                break;

            case CaseRange cr:
            {
                var failLabel = _il.DefineLabel();
                var doneLabel = _il.DefineLabel();
                _il.Emit(OpCodes.Ldloc, valueLocal);
                EmitExpression(cr.Start);
                EmitComparisonOnStackValues(TokenKind.GreaterEqual, isString);
                _il.Emit(OpCodes.Brfalse, failLabel);
                _il.Emit(OpCodes.Ldloc, valueLocal);
                EmitExpression(cr.End);
                EmitComparisonOnStackValues(TokenKind.LessEqual, isString);
                _il.Emit(OpCodes.Br, doneLabel);
                _il.MarkLabel(failLabel);
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.MarkLabel(doneLabel);
                break;
            }

            case CaseComparison cc:
                _il.Emit(OpCodes.Ldloc, valueLocal);
                EmitExpression(cc.Value);
                EmitComparisonOnStackValues(cc.Operator.Kind, isString);
                break;

            default:
                throw new NotSupportedException($"Backend IL: test CASE '{test.GetType().Name}' non ancora supportato.");
        }
    }

    // ============================================================================
    // TRY/CATCH/FINALLY, THROW, ERROR — vedi TODO Obsidian, Filone B, B7
    // ============================================================================
    //
    // Ogni THROW/ERROR BASIC si incapsula in un UNICO tipo CLR (Basic.BasicException —
    // .NET richiede comunque un vero Exception per `throw`/`catch` nativi, e una CLASS
    // utente come "ValidationError" in ExceptionsCustom.bas non deriva mai da Exception).
    // Un'istanza CLASS lanciata finisce in BasicException.UserExceptionInstance; un codice
    // numerico (THROW 999 / ERROR 999) diventa direttamente l'ErrorCode.
    //
    // TRY/CATCH/FINALLY usa i blocchi IL nativi (BeginExceptionBlock/BeginCatchBlock/
    // BeginFinallyBlock/EndExceptionBlock) — un vantaggio netto di questo backend rispetto
    // al p-code Compiler, che deve reimplementare a mano push/pop di un gestore
    // (OP_PUSH_TRY/OP_POP_TRY). Cattura SEMPRE il singolo tipo BasicException: il dispatch
    // per CatchClause (per tipo di CLASS utente O per il pseudo-tipo ERR, più un eventuale
    // filtro WHEN) avviene TUTTO dentro quell'UNICO blocco catch — mai più `catch` IL con
    // tipi diversi, perché la "specificità" di una CatchClause (es. CATCH ex AS
    // ValidationError) non è il tipo CLR catturato ma il tipo RUNTIME di
    // BasicException.UserExceptionInstance, un dettaglio interno non esprimibile come
    // filtro di tipo CLR diretto.
    //
    // ON ERROR GOTO/RESUME/RESUME NEXT restano esplicitamente NON supportati (cade nel
    // `default` di EmitStatement): un gestore d'errore non strutturato, per-procedura, con
    // ripresa a un punto arbitrario del codice, non ha un mapping naturale su try/catch
    // strutturato — richiederebbe un meccanismo del tutto diverso (installazione di un
    // handler globale + tracciamento del punto di ripresa), fuori scope qui.

    /// <summary>
    /// TRY/CATCH/FINALLY — vedi il commento della sezione sopra per il design generale.
    /// </summary>
    private void EmitTryCatchStatement(TryCatchStmt stmt)
    {
        if (_dispatchedTryEntryLabels.TryGetValue(stmt, out var gosubEntryLabel))
            _il.MarkLabel(gosubEntryLabel);
        var endLabel = _il.BeginExceptionBlock();
        EmitTryGosubContinuationJumps(stmt);

        // Vedi il commento su _activeTryScopes: un GOTO dentro stmt.TryBody il cui
        // target non è tra queste etichette/righe deve uscire dal try con Leave, non Br
        // diretto. Solo il corpo del TRY conta (non CATCH/FINALLY: un salto da lì verso
        // l'esterno del try non attraversa PIÙ questa regione, essendo già fuori dal
        // blocco try quando quel codice gira).
        var labelsInTry = new HashSet<string>();
        var linesInTry = new HashSet<int>();
        foreach (var s in stmt.TryBody)
        {
            if (s is LabelStmt ls) labelsInTry.Add(ls.Label.Lexeme);
            linesInTry.Add(s.Position.Line);
        }
        _activeTryScopes.Push((labelsInTry, linesInTry));
        try
        {
            foreach (var s in stmt.TryBody) EmitStatement(s);
        }
        finally
        {
            _activeTryScopes.Pop();
        }
        _il.Emit(OpCodes.Leave, endLabel);

        // A differenza del pattern per-riga di ON ERROR (dove il dispatch NON compila
        // mai codice utente, solo local/label interni — sicuro da emettere due volte,
        // una per catch), qui il dispatch esegue i corpi VERI delle CatchClauses
        // (EmitStatement su AST scritto dall'utente): compilarli due volte (una per
        // ciascun catch — BasicException "nativa" e la CLR grezza sotto) duplicherebbe
        // qualunque effetto collaterale non idempotente della compilazione (label,
        // GOTO, chiusure...) e produce IL invalido/InvalidProgramException al primo
        // caso non banale. Design: entrambi i catch fanno SOLO Stloc+Leave verso
        // endLabel (nessun dispatch dentro il catch); caughtLocal parte a null (default
        // di un local a riferimento mai assegnato — resta tale sul percorso normale,
        // che non lo tocca) e SUBITO dopo EndExceptionBlock() un unico blocco, fuori da
        // qualunque regione protetta, controlla "caughtLocal != null" e fa il dispatch
        // UNA SOLA VOLTA se serve — corpi delle CatchClauses compilati esattamente una
        // volta indipendentemente da quale dei due catch li ha innescati.
        var caughtLocal = _il.DeclareLocal(_types.BasicExceptionType);

        _il.BeginCatchBlock(_types.BasicExceptionType);
        _il.Emit(OpCodes.Stloc, caughtLocal);
        _il.Emit(OpCodes.Leave, endLabel);

        // Secondo catch: un'eccezione CLR nativa (IndexOutOfRangeException/
        // FileNotFoundException/...) mai avvolta in una BasicException bypasserebbe del
        // tutto TRY/CATCH — vedi AiWiki IL-OnError-Loop-Bug.md, gap array-out-of-range/
        // file mancante. Avvolta con FromClrException usando la posizione del TRY stesso
        // (nessun tracciamento per-statement della riga qui, a differenza del pattern
        // per-riga di ON ERROR — è la miglior posizione disponibile a compile-time).
        _il.BeginCatchBlock(_types.ExceptionType);
        var clrCaughtLocal = _il.DeclareLocal(_types.ExceptionType);
        _il.Emit(OpCodes.Stloc, clrCaughtLocal);
        // Se l'eccezione è GIÀ una BasicException (il catch dedicato sopra dovrebbe averla
        // intercettata, ma l'ordine dei due handler non è garantito su tutti i runtime/JIT:
        // osservato in pratica che questo ramo vince sul precedente, mappando un `ERROR 1000`
        // a ErrorCode.InternalError=200 via FromClrException), usala AS-IS senza rimappare il
        // codice — altrimenti `ex.Number` perde il valore originale.
        var notBasic = _il.DefineLabel();
        _il.Emit(OpCodes.Ldloc, clrCaughtLocal);
        _il.Emit(OpCodes.Isinst, _types.BasicExceptionType);
        _il.Emit(OpCodes.Brfalse, notBasic);
        _il.Emit(OpCodes.Ldloc, clrCaughtLocal);
        _il.Emit(OpCodes.Castclass, _types.BasicExceptionType);
        _il.Emit(OpCodes.Stloc, caughtLocal);
        _il.Emit(OpCodes.Leave, endLabel);
        _il.MarkLabel(notBasic);
        _il.Emit(OpCodes.Ldloc, clrCaughtLocal);
        _il.Emit(OpCodes.Ldc_I4, stmt.Position.FileId);
        _il.Emit(OpCodes.Ldc_I4, stmt.Position.Line);
        _il.Emit(OpCodes.Ldc_I4, stmt.Position.Column);
        _il.Emit(OpCodes.Newobj, _types.SourcePositionConstructor);
        _il.Emit(OpCodes.Call, _types.BasicExceptionFromClrExceptionMethod);
        _il.Emit(OpCodes.Stloc, caughtLocal);
        _il.Emit(OpCodes.Leave, endLabel);

        if (stmt.Finally is not null)
        {
            _il.BeginFinallyBlock();
            foreach (var s in stmt.Finally.Body) EmitStatement(s);
        }

        _il.EndExceptionBlock();

        var noDispatchNeededLabel = _il.DefineLabel();
        _il.Emit(OpCodes.Ldloc, caughtLocal);
        _il.Emit(OpCodes.Brfalse, noDispatchNeededLabel);

        foreach (var catchClause in stmt.CatchClauses)
        {
            var nextClauseLabel = _il.DefineLabel();

            if (catchClause.Variable is not null)
            {
                var variableType = _program.Symbols.GetSymbol(catchClause.Variable.SymbolId).DataType;
                if (variableType is ErrType)
                {
                    EmitBuildErrInstance(caughtLocal, _currentLineLocal, stmt.Position.Line);
                    _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(catchClause.Variable.SymbolId));
                }
                else if (variableType is ClassType classType)
                {
                    // Stloc in un local dedicato invece di Dup+Brfalse: Dup+Brfalse
                    // lascia un valore residuo sullo stack di valutazione sul ramo
                    // "salta a nextClauseLabel" (Brfalse consuma SEMPRE una sola delle
                    // due copie, indipendentemente dal salto) — un vero stack-imbalance
                    // a un punto di merge (Br) raggiunto anche da un ramo WhenCondition
                    // pulito (0 residui) → InvalidProgramException al load. Stloc+Ldloc
                    // (+Ldloc) pulisce lo stack su ENTRAMBI i rami.
                    var classTypeClr = DataTypeMapper.Map(classType, _types, _classTypes);
                    var isMatchLocal = _il.DeclareLocal(classTypeClr);
                    _il.Emit(OpCodes.Ldloc, caughtLocal);
                    _il.Emit(OpCodes.Callvirt, _types.BasicExceptionUserExceptionInstanceGetter);
                    _il.Emit(OpCodes.Isinst, classTypeClr);
                    _il.Emit(OpCodes.Stloc, isMatchLocal);
                    _il.Emit(OpCodes.Ldloc, isMatchLocal);
                    _il.Emit(OpCodes.Brfalse, nextClauseLabel);
                    _il.Emit(OpCodes.Ldloc, isMatchLocal);
                    _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(catchClause.Variable.SymbolId));
                }
                else
                {
                    throw new NotSupportedException($"Backend IL: CATCH con variabile di tipo '{variableType.GetType().Name}' non ancora supportato (solo ERR o una CLASS utente).");
                }
            }

            if (catchClause.WhenCondition is not null)
            {
                EmitExpression(catchClause.WhenCondition);
                _il.Emit(OpCodes.Brfalse, nextClauseLabel);
            }

            foreach (var s in catchClause.Body) EmitStatement(s);
            // Br, non Leave: qui siamo già fuori da qualunque regione protetta (dopo
            // EndExceptionBlock), un Leave non sarebbe necessario né valido.
            _il.Emit(OpCodes.Br, noDispatchNeededLabel);

            _il.MarkLabel(nextClauseLabel);
        }

        // Nessuna CatchClause ha gestito l'eccezione (tipo/WHEN non soddisfatti da
        // nessuna): ripropaga. Ldloc+Throw, non Rethrow — Rethrow non è più disponibile
        // qui (fuori da ogni catch): perde la preservazione dello stack trace originale
        // SOLO in questo caso limite (nessuna clausola ha corrisposto), tradeoff
        // accettato per evitare di ricompilare (quindi duplicare) i corpi delle
        // CatchClauses.
        _il.Emit(OpCodes.Ldloc, caughtLocal);
        _il.Emit(OpCodes.Throw);

        _il.MarkLabel(noDispatchNeededLabel);
    }

    /// <summary>
    /// Costruisce (SENZA lanciarla) una BasicException — nucleo condiviso da THROW/ERROR.
    /// <paramref name="emitErrorCode"/> deve lasciare un int32 sullo stack (il valore
    /// ErrorCode); <paramref name="emitMessage"/>, se non null, deve lasciare una string
    /// (altrimenti viene passato null). Posizione: le 3 componenti di
    /// <paramref name="position"/> sono note a compile-time (dal Token dello statement),
    /// quindi vengono incorporate come costanti — mai calcolate a runtime.
    /// SourceLine/innerException: sempre null in questo backend (nessuna estrazione del
    /// testo sorgente originale né incapsulamento di un'eccezione CLR precedente in questo
    /// MVP).
    /// </summary>
    private void EmitNewBasicException(Action emitErrorCode, SourcePosition position, Action? emitMessage)
    {
        emitErrorCode();
        _il.Emit(OpCodes.Ldc_I4, position.FileId);
        _il.Emit(OpCodes.Ldc_I4, position.Line);
        _il.Emit(OpCodes.Ldc_I4, position.Column);
        _il.Emit(OpCodes.Newobj, _types.SourcePositionConstructor);
        if (emitMessage is not null) emitMessage(); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Newobj, _types.BasicExceptionConstructor);
    }

    /// <summary>
    /// Popola il simbolo globale ERR (StdLib.System.cs, ErrType) con un ErrInstance costruito
    /// dall'eccezione appena catturata — chiamato dal ramo ON ERROR GOTO/RESUME NEXT del catch
    /// per-riga, che a differenza di CATCH ex AS ERR non lega mai l'eccezione a una variabile
    /// propria. Senza questo, ERR.Number/ERR.Description letti dentro l'handler leggono il
    /// local mai inizializzato del simbolo ERR (null) → NullReferenceException. No-op se il
    /// simbolo globale ERR non è nello scope (non dovrebbe mai accadere: registrato da StdLib
    /// per ogni programma). Porting identico da GM-Basic (vedi AiWiki IL-OnError-Loop-Bug.md).
    /// </summary>
    private void EmitCaptureErrorIntoGlobalErr(LocalBuilder caughtExceptionLocal)
    {
        var errSymbolId = _program.Symbols.FindSymbolsByName("ERR").FirstOrDefault(s => s.DataType is ErrType)?.Id;
        if (errSymbolId is null) return;
        EmitStoreVariable(errSymbolId.Value, () => EmitBuildErrInstance(caughtExceptionLocal, _currentLineLocal, 0));
    }

    /// <summary>
    /// Costruisce un Basic.ErrInstance (a cosa si lega CATCH ex AS ERR) dai dettagli della
    /// BasicException catturata — LineNumber/SourceLineText/StackTrace non sono
    /// popolati con precisione in questo MVP (il testo sorgente originale della riga non è
    /// disponibile qui a compile-time in una forma comoda), solo Number/Description, gli
    /// unici due campi effettivamente usati negli esempi di questo dialetto
    /// (ExceptionsTryIfSelect.bas/ExceptionsCustom.bas: sempre e solo ex.Number/ex.Description).
    /// </summary>
    private void EmitBuildErrInstance(LocalBuilder caughtExceptionLocal, LocalBuilder? lineLocal, int fallbackLine)
    {
        _il.Emit(OpCodes.Ldloc, caughtExceptionLocal);
        _il.Emit(OpCodes.Callvirt, _types.BasicExceptionCodeGetter);
        _il.Emit(OpCodes.Ldloc, caughtExceptionLocal);
        _il.Emit(OpCodes.Callvirt, _types.ExceptionMessageGetter);
        // LineNumber: la riga portata dall'eccezione se nota (è la riga del punto che ha
        // fallito, ANCHE in una procedura chiamata: il chiamante non deve riportare il proprio
        // call-site), altrimenti la riga dello statement in esecuzione (local aggiornato da
        // EmitStatement), altrimenti la costante di fallback. Il tutto in IlRuntime.ErrLineOf.
        _il.Emit(OpCodes.Ldloc, caughtExceptionLocal);
        if (lineLocal is not null) _il.Emit(OpCodes.Ldloc, lineLocal);
        else _il.Emit(OpCodes.Ldc_I4, fallbackLine);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ErrLineOf), _types.BasicExceptionType, typeof(int)));
        _il.Emit(OpCodes.Ldstr, string.Empty);
        _il.Emit(OpCodes.Ldstr, string.Empty);
        _il.Emit(OpCodes.Newobj, _types.ErrInstanceConstructor);
    }

    /// <summary>
    /// THROW &lt;espressione&gt;: un'istanza CLASS utente si incapsula in
    /// BasicException.UserExceptionInstance (ErrorCode.UserDefined); un codice numerico
    /// diventa direttamente l'ErrorCode. Ogni altro tipo lancia esplicito — vedi il
    /// commento di sezione sopra per il perché di questo scope.
    /// </summary>
    private void EmitThrowStatement(ThrowStmt stmt)
    {
        var valueType = InferType(stmt.Value);
        var position = stmt.Token.Position;

        if (valueType is ClassType)
        {
            EmitNewBasicException(() => _il.Emit(OpCodes.Ldc_I4, (int)ErrorCode.UserDefined), position, null);
            var exLocal = _il.DeclareLocal(_types.BasicExceptionType);
            _il.Emit(OpCodes.Stloc, exLocal);
            _il.Emit(OpCodes.Ldloc, exLocal);
            EmitExpression(stmt.Value);
            _il.Emit(OpCodes.Callvirt, _types.BasicExceptionUserExceptionInstanceSetter);
            _il.Emit(OpCodes.Ldloc, exLocal);
            _il.Emit(OpCodes.Throw);
            return;
        }

        if (valueType is DoubleType or LongType)
        {
            EmitNewBasicException(() => EmitInt32Arg(stmt.Value), position, null);
            _il.Emit(OpCodes.Throw);
            return;
        }

        throw new NotSupportedException($"Backend IL: THROW di un valore di tipo '{valueType.GetType().Name}' non ancora supportato (solo un'istanza CLASS o un codice di errore numerico Double/Long).");
    }

    /// <summary>ERROR codice[, descrizione]: stesso nucleo di THROW con un codice numerico, con un messaggio opzionale.</summary>
    private void EmitErrorStatement(ErrorStmt stmt)
    {
        var position = stmt.Token.Position;
        Action? emitMessage = stmt.Description is not null ? () => EmitExpression(stmt.Description) : null;
        EmitNewBasicException(() => EmitInt32Arg(stmt.ErrorCode), position, emitMessage);
        // `throw` è valido anche dentro una regione protetta (a differenza di `ret`): prima si
        // faceva Leave verso un punto di throw FUORI dal try, con il risultato che un
        // `ERROR n` dentro un TRY **non era catturabile dal CATCH** di quello stesso TRY
        // (bug: `TRY : ERROR 11 : CATCH e AS ERR : …` terminava il programma su IL, mentre
        // PCode/VM-C/C99 lo catturavano — la divergenza impediva anche di verificare
        // ERR.Number/Description nel CATCH). Il throw diretto lascia al CLR la scelta del
        // catch: TRY/CATCH nativo, altrimenti il dispatch ON ERROR del chunk, altrimenti
        // errore fatale.
        _il.Emit(OpCodes.Throw);
    }

    /// <summary>
    /// FOR: valuta Start/End/Step UNA SOLA VOLTA (semantica BASIC standard — End/Step
    /// non vanno rivalutati ad ogni iterazione, anche se le espressioni originali
    /// referenziano variabili che cambiano durante il ciclo). End e Step finiscono
    /// in locali IL dedicati, associati alla variabile contatore per lexeme in
    /// _forLoopState, cosi' ForCheckAndJumpStmt/NextStmt (più avanti nello stream,
    /// senza SymbolId proprio — solo un Token grezzo) possono ritrovarli.
    /// </summary>
    private void EmitForStatement(ForStmt stmt)
    {
        var counterSymbolId = stmt.Variable.SymbolId;
        // Stepper GPM: FOR sequenziali sullo stesso lexeme (SpritesExamples: tre `FOR I`)
        // hanno SymbolId DIVERSI — ogni EnterLoopStmt registra un simbolo nuovo. Il
        // prologo (PreDeclareForTemps) e ForCheck/Next tengono SOLO il primo. Se Start
        // finisce nel locale del ForStmt corrente, il secondo FOR non azzera il
        // contatore che ForCheck legge (resta End+1) → il corpo non gira mai. In
        // SpritesExamples: 150 LOAD su SprID(0..149), poi SET POSITION saltato → un
        // solo blob a (0,0).
        if (_gpmStepperMode
            && !_parameters.ContainsKey(counterSymbolId)
            && _forLoopState.TryGetValue(stmt.Variable.NameCleaned.Lexeme, out var sharedCounter)
            && sharedCounter.SymbolId != counterSymbolId)
        {
            counterSymbolId = sharedCounter.SymbolId;
        }
        // Contatore FOR ATTIVO: push qui (dopo l'eventuale remap stepper), pop al suo
        // ExitLoopStmt. Guida il reindirizzamento per NOME di EmitLoadVariable: un nome che
        // coincide con un contatore FOR va caricato dal local del contatore SOLO se il FOR è
        // ancora aperto, non per tutta la procedura (bug LoopsBasics TEST 27).
        _activeForCounters.Push(counterSymbolId);
        // Contatore PARAMETRO di procedura (es. FOR i = ... con i parametro): nessun
        // LocalBuilder — percorso Ldarg/Starg (GetOrDeclareLocal lanciava esplicito).
        var counterIsParam = _parameters.ContainsKey(counterSymbolId);
        if (counterIsParam)
        {
            EmitStoreVariable(counterSymbolId, () =>
            {
                EmitExpression(stmt.Start);
                EmitCoerceToCounterType(counterSymbolId);
            });
        }
        else
        {
            var counterLocal = GetOrDeclareLocal(counterSymbolId);
            EmitExpression(stmt.Start);
            EmitCoerceToCounterType(counterSymbolId);
            _il.Emit(OpCodes.Stloc, counterLocal);
        }

        // Stepper mode (§15): end/step sono GIÀ pre-dichiarati dal prologo (PreDeclareForTemps)
        // e vivono nel frame — qui li valorizzo SOLO, senza ridichiararli (altrimenti il
        // ForCheckAndJumpStmt/NextStmt lancerebbe ResolveForLoopState su LocalBuilder diversi).
        if (_gpmStepperMode && _forLoopState.TryGetValue(stmt.Variable.NameCleaned.Lexeme, out var predeclared))
        {
            EmitExpression(stmt.End);
            _il.Emit(OpCodes.Conv_R8);
            _il.Emit(OpCodes.Stloc, predeclared.EndLocal);
            if (stmt.Step is not null) EmitExpression(stmt.Step);
            else _il.Emit(OpCodes.Ldc_R8, 1.0);
            _il.Emit(OpCodes.Conv_R8);
            _il.Emit(OpCodes.Stloc, predeclared.StepLocal);
            return;
        }

        var endLocal = _il.DeclareLocal(_types.Double);
        EmitExpression(stmt.End);
        _il.Emit(OpCodes.Conv_R8);
        _il.Emit(OpCodes.Stloc, endLocal);

        var stepLocal = _il.DeclareLocal(_types.Double);
        if (stmt.Step is not null) EmitExpression(stmt.Step);
        else _il.Emit(OpCodes.Ldc_R8, 1.0);
        _il.Emit(OpCodes.Conv_R8);
        _il.Emit(OpCodes.Stloc, stepLocal);

        _forLoopState[stmt.Variable.NameCleaned.Lexeme] = (stmt.Variable.SymbolId, endLocal, stepLocal);
    }

    /// <summary>
    /// Continua il ciclo se (Step >= 0 AND Counter &lt;= End) OR (Step &lt; 0 AND Counter >= End),
    /// altrimenti salta a ExitLabel — semantica FOR standard che supporta anche STEP negativo.
    /// </summary>
    private void EmitForCheckAndJumpStatement(ForCheckAndJumpStmt stmt)
    {
        var state = ResolveForLoopState(stmt.Variable);
        var counterSymbolId = state.SymbolId;
        var counterIsParam = _parameters.ContainsKey(counterSymbolId);

        var stepNegativeLabel = _il.DefineLabel();
        var checkDoneLabel = _il.DefineLabel();

        // Store exit label for EXIT FOR
        var exitLabel = ResolveNamedLabel(stmt.ExitLabel);
        _loopExitLabels[stmt.Variable.Lexeme] = exitLabel;
        _loopExitLabels[stmt.ExitLabel.Lexeme] = exitLabel;

        _il.Emit(OpCodes.Ldloc, state.StepLocal);
        _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Clt);
        _il.Emit(OpCodes.Brtrue, stepNegativeLabel);

        // Step >= 0: esci se Counter > End.
        if (counterIsParam) EmitLoadCounterAsDouble(counterSymbolId);
        else EmitLoadCounterAsDouble(GetOrDeclareLocal(counterSymbolId));
        _il.Emit(OpCodes.Ldloc, state.EndLocal);
        _il.Emit(OpCodes.Cgt);
        EmitBranch(stmt.ExitLabel, OpCodes.Brtrue);
        _il.Emit(OpCodes.Br, checkDoneLabel);

        // Step < 0: esci se Counter < End.
        _il.MarkLabel(stepNegativeLabel);
        if (counterIsParam) EmitLoadCounterAsDouble(counterSymbolId);
        else EmitLoadCounterAsDouble(GetOrDeclareLocal(counterSymbolId));
        _il.Emit(OpCodes.Ldloc, state.EndLocal);
        _il.Emit(OpCodes.Clt);
        EmitBranch(stmt.ExitLabel, OpCodes.Brtrue);

        _il.MarkLabel(checkDoneLabel);
    }

    private void EmitNextStatement(NextStmt stmt)
    {
        var state = ResolveForLoopState(stmt.Variable);
        var counterSymbolId = state.SymbolId;
        var counterIsParam = _parameters.ContainsKey(counterSymbolId);

        if (counterIsParam)
        {
            EmitStoreVariable(counterSymbolId, () =>
            {
                // valore corrente → double → + step → coerce al tipo dichiarato (Starg).
                EmitLoadVariable(counterSymbolId);
                var ct = NormalizeInferredType(_program.Symbols.GetSymbol(counterSymbolId).DataType);
                var cClr = DataTypeMapper.Map(ct, _types, _classTypes);
                if (cClr == typeof(Int16) || cClr == typeof(Int32) || cClr == typeof(Int64) || cClr == typeof(float))
                    _il.Emit(OpCodes.Conv_R8);
                _il.Emit(OpCodes.Ldloc, state.StepLocal);
                _il.Emit(OpCodes.Add);
                EmitCoerceToCounterType(counterSymbolId);
            });
        }
        else
        {
            var counterLocal = GetOrDeclareLocal(counterSymbolId);
            EmitLoadCounterAsDouble(counterLocal);
            _il.Emit(OpCodes.Ldloc, state.StepLocal);
            _il.Emit(OpCodes.Add);
            EmitStoreDoubleToCounter(counterLocal);
        }
    }

    private void EmitLoadCounterAsDouble(int symbolId)
    {
        EmitLoadVariable(symbolId);
        var t = NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType);
        var clr = DataTypeMapper.Map(t, _types, _classTypes);
        if (clr == typeof(Int16) || clr == typeof(Int32) || clr == typeof(Int64) || clr == typeof(float))
            _il.Emit(OpCodes.Conv_R8);
    }

    private void EmitLoadCounterAsDouble(LocalBuilder counterLocal)
    {
        _il.Emit(OpCodes.Ldloc, counterLocal);
        var localType = counterLocal.LocalType;
        if (localType == typeof(Int16) || localType == typeof(short))
            _il.Emit(OpCodes.Conv_R8);
        else if (localType == typeof(Int32) || localType == typeof(int))
            _il.Emit(OpCodes.Conv_R8);
        else if (localType == typeof(Int64) || localType == typeof(long))
            _il.Emit(OpCodes.Conv_R8);
        else if (localType == typeof(float) || localType == typeof(Single))
            _il.Emit(OpCodes.Conv_R8);
    }

    private void EmitStoreDoubleToCounter(LocalBuilder counterLocal)
    {
        var localType = counterLocal.LocalType;
        if (localType == typeof(Int16) || localType == typeof(short))
            _il.Emit(OpCodes.Conv_I2);
        else if (localType == typeof(Int32) || localType == typeof(int))
            _il.Emit(OpCodes.Conv_I4);
        else if (localType == typeof(Int64) || localType == typeof(long))
            _il.Emit(OpCodes.Conv_I8);
        else if (localType == typeof(float) || localType == typeof(Single))
            _il.Emit(OpCodes.Conv_R4);
        _il.Emit(OpCodes.Stloc, counterLocal);
    }

    private void EmitCoerceToCounterType(int symbolId)
    {
        var type = NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType);
        var clrType = DataTypeMapper.Map(type, _types, _classTypes);
        if (clrType == typeof(Int16)) _il.Emit(OpCodes.Conv_I2);
        else if (clrType == typeof(Int32)) _il.Emit(OpCodes.Conv_I4);
        else if (clrType == typeof(Int64)) _il.Emit(OpCodes.Conv_I8);
        else if (clrType == typeof(float)) _il.Emit(OpCodes.Conv_R4);
        else if (clrType != typeof(double)) _il.Emit(OpCodes.Conv_R8);
    }

    private (int SymbolId, LocalBuilder EndLocal, LocalBuilder StepLocal) ResolveForLoopState(Token variable)
    {
        if (_forLoopState.TryGetValue(variable.Lexeme, out var state)) return state;
        throw new InvalidOperationException(
            $"Backend IL: nessun FOR attivo per la variabile '{variable.Lexeme}' (ForCheckAndJumpStmt/NextStmt senza ForStmt corrispondente).");
    }

    /// <summary>
    /// FOR EACH: copertura attuale SOLO un array locale/globale RANK 1 DIMensionato
    /// direttamente (una <see cref="VariableExpr"/> tracciata in <see cref="_arrays"/> —
    /// non un campo di classe/UDT, non un array multi-dimensionale, non un'espressione
    /// generica come un valore di ritorno di funzione) e UNA sola variabile di iterazione
    /// (l'elemento — il caso a due variabili, chiave/valore su una collezione
    /// dizionario-like, non è modellato in questo backend, che non ha alcuna nozione
    /// nativa di Dictionary come tipo CLR qui, a differenza del p-code Compiler che tratta
    /// ogni collezione come indicizzabile via LBOUND/UBOUND dinamici).
    ///
    /// Il contatore (un vero int32 CIL, mai il tipo scalare dell'elemento) parte dal LOWER
    /// BOUND dell'array — stessa semantica del p-code Compiler (Compiler.Others.cs,
    /// VisitForEachInitStatement, LBOUND non 0) — cosi' un array 1-based/N-based itera
    /// correttamente sui suoi indici reali, esattamente come EmitArrayAccess/
    /// EmitArrayElementAssignment già fanno per l'indicizzazione esplicita.
    /// </summary>
    private void EmitForEachInitStatement(ForEachInitStmt stmt)
    {
        // Matrice con N>=3 variabili (FOR EACH a, b, c IN m(3,3)): la riga viene destrutturata
        // in elementi (vedi EmitForEachAdvanceAndAssignStatement, ramo matrice). Non-matrice
        // con 3+ variabili resta non supportato.

        // Resolve the array: VariableExpr (DIM arr AS Type) or FunctionExpr with 0 args
        // (array reference like `Persone()` in FOR EACH)
        ArrayInfo? arrayInfo = null;
        var collExpr = stmt.CollectionExpr;
        if (collExpr is VariableExpr ve)
            _arrays.TryGetValue(ve.SymbolId, out arrayInfo);
        else if (collExpr is FunctionExpr { Args.Length: 0 } fe)
        {
            // `Numeri()` or `Persone()` — lookup by name
            foreach (var sym in _program.Symbols.FindSymbolsByName(fe.OriginalToken.Lexeme))
                if (_arrays.TryGetValue(sym.Id, out arrayInfo))
                    break;
        }

        // Fast path invariato: array CLR 1D reale, UNA sola variabile di iterazione (accesso
        // diretto Ldelem, niente normalizzazione dinamica — vedi EmitForEachAdvanceAndAssignStatement).
        if (arrayInfo != null && arrayInfo.Rank == 1 && stmt.Variables.Length == 1)
        {
            var iteratorSymbolId = ResolveForEachIteratorSymbolId(stmt.Variables[0], stmt.ScopeId);
            // UpdateSymbolType: il Linker registra la variabile di iterazione con InferredType
            // (→ Double), mai il vero tipo elemento dell'array — vedi il commento esteso su
            // _forEachListState. Un array STRING con la variabile ancora tipata Double produce
            // codegen (PRINT, conversioni, ecc.) errato più avanti nel corpo del loop
            // (InvalidProgramException a runtime, bug pre-esistente anche su questo fast path,
            // dimostrato con `FOR EACH s IN stringArray()`).
            _program.Symbols.UpdateSymbolType(iteratorSymbolId, arrayInfo.ElementDataType);
            var counterLocal = _il.DeclareLocal(_types.Int32);
            _il.Emit(OpCodes.Ldc_I4, arrayInfo.LowerBounds[0]);
            _il.Emit(OpCodes.Stloc, counterLocal);
            _forEachState[stmt.Variables[0].Lexeme] = (arrayInfo, counterLocal, iteratorSymbolId);
            // Un nome di variabile riusato per un loop di forma DIVERSA più avanti nello stesso
            // scope (es. `FOR EACH n IN nums()` poi, più avanti, `FOR EACH n IN RANGE(1,5)`)
            // lascerebbe altrimenti una entry STALE nell'altro dizionario — EmitForEachAdvanceAndAssignStatement
            // controlla _forEachState PRIMA di _forEachListState, quindi la vecchia entry array
            // vincerebbe sempre per nome, con un exit-check basato su bound OBSOLETI (0
            // iterazioni, silenziosamente) invece del vero stato RANGE appena inizializzato —
            // bug scoperto con la sezione "9. RANGE(1,5)" di ForEachComprehensive.bas (la
            // variabile "n" era già stata usata per un FOR EACH su array in sezione 1a).
            _forEachListState.Remove(stmt.Variables[0].Lexeme);
            return;
        }

        // Percorso generico: LIST/DICT/JSON/STRING/RANGE/REVERSE, o un array a DUE variabili
        // (indice+valore, non coperto dal fast path sopra) — la collezione sorgente viene
        // normalizzata UNA volta (object dinamico → List<object?>/List<Tuple<...>>) tramite
        // IlRuntime.ForEachValues/ForEachEntries, poi iterata per indice come un array (vedi
        // _forEachListState).
        var isEntries = stmt.Variables.Length > 1;
        EmitForEachCollectionAsObject(collExpr, arrayInfo);
        _il.Emit(OpCodes.Call, isEntries ? IlForEachEntriesMethod : IlForEachValuesMethod);

        var listLocal = _il.DeclareLocal(isEntries
            ? typeof(List<Tuple<object?, object?>>)
            : typeof(List<object?>));
        _il.Emit(OpCodes.Stloc, listLocal);

        var ctrLocal = _il.DeclareLocal(_types.Int32);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, ctrLocal);

        var iterSymId = ResolveForEachIteratorSymbolId(stmt.Variables[0], stmt.ScopeId);
        int? valueIterSymId = isEntries ? ResolveForEachIteratorSymbolId(stmt.Variables[1], stmt.ScopeId) : null;
        var (keyOrElemType, valueType) = InferForEachElementTypes(collExpr, arrayInfo, isEntries);
        // UpdateSymbolType, non solo un local dichiarato col tipo giusto: InferType(VariableExpr)
        // per ogni USO successivo della variabile (es. l'argomento di PRINT) legge SEMPRE
        // symbol.DataType per decidere quale conversione emettere — su un DICT OF STRING,DOUBLE,
        // `PRINT item` (item=chiave STRING) sceglieva la conversione Double→stringa e chiamava
        // StringHelpers.DoubleToInvariantString su un riferimento string reale: IL invalido,
        // mai un errore a compile-time (InvalidProgramException solo al lancio del programma
        // nativo compilato). UpdateSymbolType corregge la fonte una volta per tutte.
        _program.Symbols.UpdateSymbolType(iterSymId, keyOrElemType);
        if (isEntries) _program.Symbols.UpdateSymbolType(valueIterSymId!.Value, valueType!);
        // Matrice con 3+ variabili (destructuring della riga): ogni variabile è un ELEMENTO
        // (arrayInfo.ElementDataType), NON (indice, valore) come per isEntries generico —
        // UpdateSymbolType altrimenti le tipa Double e il ramo 3+ produce IL invalido
        // (unbox Int16 → stloc su local Double).
        if (arrayInfo is { Rank: > 1 } && stmt.Variables.Length >= 3)
            foreach (var feVar in stmt.Variables)
            {
                var feVarSym = ResolveForEachIteratorSymbolId(feVar, stmt.ScopeId);
                _program.Symbols.UpdateSymbolType(feVarSym, arrayInfo.ElementDataType);
            }
        // Matrice: la variabile che riceve la RIGA (array 1D) a runtime — la [0] con 1 variabile
        // (FOR EACH row IN m), la [1] con 2 variabili (FOR EACH i, row IN m) — va registrata in
        // _arrays (con un local che riceverà la riga a ogni iterazione) altrimenti `row(0)`
        // dentro il loop fallisce con "non DIMensionato". MAI per 3+ variabili (destructuring:
        // le variabili sono ELEMENTI, non array — registrarle in _arrays farebbe Stloc di un
        // Int16 su un local Int16[] → InvalidProgram).
        if (arrayInfo is { Rank: > 1 } && stmt.Variables.Length <= 2)
        {
            // Il symbol della variabile di iterazione (scope loop) può essere DIVERSO dal symbol
            // che il BODY usa (es. `DIM row(1)` + `FOR EACH i, row IN m`: il body row(0) usa il
            // symbol DIM con ArrayType, la registrazione userebbe il symbol loop InferredType →
            // "il simbolo 'row' non è un array" nel body). Preferisci un symbol con lo stesso
            // nome già tipato ArrayType (la DIM) se esiste — è quello che il body risolve.
            var rowVarSymId = isEntries ? (valueIterSymId ?? iterSymId) : iterSymId;
            var elemClrType = DataTypeMapper.Map(NormalizeInferredType(arrayInfo.ElementDataType), _types, _classTypes);
            var rowLocal = _il.DeclareLocal(elemClrType.MakeArrayType());
            _arrays[rowVarSymId] = new ArrayInfo(rowLocal, elemClrType.MakeArrayType(), arrayInfo.ElementDataType, 1, [0]);
            // 2 variabili su matrice: anche la riga (Item2) va assegnata al local array nel ramo
            // isEntries — registriamo il local per l'uso sotto (via _forEachListState.ElementClrType
            // non basta: il ramo isEntries standard assegna Item2 unboxato). Per ora il ramo
            // isEntries a 2 variabili resta con la semantica (indice, riga) — la riga come array
            // è coperta dal ramo !isEntries a 1 variabile e dal ramo 3+.
        }
        var keyOrElemClrType = DataTypeMapper.Map(NormalizeInferredType(keyOrElemType), _types, _classTypes);
        var valueClrType = isEntries ? DataTypeMapper.Map(NormalizeInferredType(valueType!), _types, _classTypes) : null;
        var feElemClr = arrayInfo is { Rank: > 1 } ? DataTypeMapper.Map(NormalizeInferredType(arrayInfo.ElementDataType), _types, _classTypes) : (Type?)null;
        _forEachListState[stmt.Variables[0].Lexeme] = (listLocal, ctrLocal, iterSymId, valueIterSymId, keyOrElemClrType, valueClrType, stmt.Variables.Length, feElemClr);
        // Simmetrico alla pulizia nel fast path array sopra — un nome riusato prima per il
        // percorso generico e poi per un vero array non deve lasciare una entry stale qui.
        // BUG REALE trovato 2026-09-07 ("dict-pairs FAIL" in ForEachJsonEdgeCases.bas, IL):
        // puliva SOLO stmt.Variables[0] (qui "k" in `FOR EACH k, v IN d`) — MAI
        // stmt.Variables[1] ("v"). Se un FOR EACH precedente nella stessa SUB aveva usato
        // "v" come UNICA variabile su un vero array (es. `FOR EACH v IN r`, fast path
        // _forEachState), quella entry stale restava — InferVariableType (TypeSystem.cs)
        // consulta _forEachState per NOME prima del symbol table, quindi ogni uso
        // successivo di "v" dentro il loop generico (qui `vs = vs + v`) veniva letto dal
        // meccanismo SBAGLIATO (array fast path invece di dict-value generico): risultato
        // un double spazzatura (bit denormalizzati, es. 6.6E-314) invece della somma reale.
        // Pulizia estesa a TUTTE le variabili del FOR EACH, non solo la prima.
        foreach (var v in stmt.Variables) _forEachState.Remove(v.Lexeme);
    }

    /// <summary>
    /// Spinge sullo stack la collezione sorgente di un FOR EACH generico come <c>object</c>,
    /// pronta per IlRuntime.ForEachValues/ForEachEntries — con due scorciatoie dedicate per
    /// RANGE/REVERSE (mai variabili tracciate, vanno riconosciute per forma sintattica
    /// dell'espressione, non per SymbolId):
    /// - RANGE(start,end[,step]) chiama direttamente IlRuntime.RangeBuiltin, senza passare da
    ///   un tipo CLR "RANGE" che non esiste.
    /// - REVERSE(x) con x non-stringa chiama IlRuntime.ReverseGeneric (REVERSE su STRING resta
    ///   il percorso dedicato esistente, EmitStringUnaryBuiltinCall, usato per l'uso normale
    ///   come espressione — qui serve solo per l'uso come collezione FOR EACH).
    /// </summary>
    private void EmitForEachCollectionAsObject(Expr collExpr, ArrayInfo? arrayInfo)
    {
        if (arrayInfo != null)
        {
            // Array reale (T[]) — reference type, nessun box necessario.
            _il.Emit(OpCodes.Ldloc, arrayInfo.Local);
            return;
        }
        if (collExpr is BuiltinFunctionCallExpr { Name.Kind: TokenKind.Range } rangeExpr)
        {
            // Il parametro "values" di RANGE (StdLib.Math.cs) è ParamArray: il Linker
            // impacchetta gli argomenti del call site in un unico ArrayLiteralExpr sintetico
            // (Linker.Rewrites.Expressions.cs) — rangeExpr.Args è quindi {ArrayLiteralExpr},
            // non {start, end, [step]} diretti (stesso bug già trovato e corretto lato p-code
            // VM, vedi PCodeProcessor.Builtins.cs case RANGE).
            // Robusto a wrapping ANNIDATO (osservato con RANGE dentro un FOR EACH annidato:
            // {ArrayLiteralExpr{ArrayLiteralExpr{...}}}), non solo a un singolo livello — stesso
            // principio del fix analogo lato p-code VM (unwrap in loop, non un solo if).
            var rangeArgs = rangeExpr.Args;
            while (rangeArgs is [ArrayLiteralExpr rangeArrLit]) rangeArgs = rangeArrLit.Elements;
            EmitDoubleArgument(rangeArgs[0]);
            EmitDoubleArgument(rangeArgs[1]);
            if (rangeArgs.Length >= 3) EmitDoubleArgument(rangeArgs[2]);
            else _il.Emit(OpCodes.Ldc_R8, 1.0);
            _il.Emit(OpCodes.Call, IlRangeBuiltinMethod);
            return;
        }
        if (collExpr is BuiltinFunctionCallExpr { Name.Kind: TokenKind.Reverse, Args.Length: 1 } reverseExpr &&
            InferType(reverseExpr.Args[0]) is not StringType)
        {
            EmitExpression(reverseExpr.Args[0]);
            var argClrType = DataTypeMapper.Map(NormalizeInferredType(InferType(reverseExpr.Args[0])), _types, _classTypes);
            if (argClrType.IsValueType) _il.Emit(OpCodes.Box, argClrType);
            _il.Emit(OpCodes.Call, IlReverseGenericMethod);
            return;
        }
        EmitExpression(collExpr);
        var clrType = DataTypeMapper.Map(NormalizeInferredType(InferType(collExpr)), _types, _classTypes);
        if (clrType.IsValueType) _il.Emit(OpCodes.Box, clrType);
    }

    /// <summary>
    /// Determina il tipo (elemento singolo, o chiave+valore per un ciclo a due variabili) di
    /// una collezione FOR EACH guardando la sua FORMA REALE — mai il DataType (sempre
    /// InferredType) che il Linker assegna alle variabili di iterazione stesse (vedi il
    /// commento su <see cref="_forEachListState"/>). RANGE/REVERSE non hanno un ArrayType/
    /// ListType proprio (sono BuiltinFunctionCallExpr) quindi vanno riconosciuti per forma
    /// sintattica, stessa logica di EmitForEachCollectionAsObject.
    /// </summary>
    private (DataType KeyOrElement, DataType? Value) InferForEachElementTypes(Expr collExpr, ArrayInfo? arrayInfo, bool isEntries)
    {
        if (arrayInfo != null)
        {
            var elemType = arrayInfo.ElementDataType;
            // Matrice (Rank>1) con UNA variabile: il valore di iterazione è la RIGA (array 1D) —
            // la variabile va tipata come ArrayType(elem), non come elemento singolo, altrimenti
            // `row(0)` dentro il loop fallisce con "il simbolo 'row' non è un array".
            if (arrayInfo.Rank > 1 && !isEntries)
                return (new ArrayType(elemType), null);
            // Matrice 2-var (FOR EACH i, row IN m): la VALUE (riga) è un array 1D — tipala come
            // ArrayType(elem), non elemType (altrimenti row(0) nel body fallisce con "il
            // simbolo 'row' non è un array": il body usa il symbol del loop tipato elemType).
            if (arrayInfo.Rank > 1 && isEntries)
                return (new DoubleType(), new ArrayType(elemType));
            return isEntries ? (new DoubleType(), elemType) : (elemType, null);
        }
        if (collExpr is BuiltinFunctionCallExpr { Name.Kind: TokenKind.Range })
            return isEntries ? (new DoubleType(), new DoubleType()) : (new DoubleType(), null);
        if (collExpr is BuiltinFunctionCallExpr { Name.Kind: TokenKind.Reverse, Args.Length: 1 } reverseExpr &&
            InferType(reverseExpr.Args[0]) is not StringType)
            return InferForEachElementTypes(reverseExpr.Args[0], null, isEntries);

        var collType = NormalizeInferredType(InferType(collExpr));
        return collType switch
        {
            ListType lt => isEntries ? (new DoubleType(), lt.ElementType) : (lt.ElementType, null),
            // FOR EACH a UNA variabile su DICT lega alla ENTRY intera (.Key/.Value via campo
            // dinamico), mai al solo valore — vedi il commento esteso in IlRuntime.ForEachValues.
            // JsonType è il "resta object" già usato per JSON: stessa infrastruttura di campo
            // dinamico (EmitFieldAccess/InferFieldAccessType via JsonFieldGet) riusata qui.
            DictType dt => isEntries ? (dt.KeyType, dt.ValueType) : (new Basic.JsonType(), null),
            HashSetType hs => isEntries ? (new DoubleType(), hs.ElementType) : (hs.ElementType, null),
            LinkedListType ll => isEntries ? (new DoubleType(), ll.ElementType) : (ll.ElementType, null),
            StringType => isEntries ? (new DoubleType(), new StringType()) : (new StringType(), null),
            // JSON (e qualunque altro tipo dinamico non riconosciuto): nessuna conversione,
            // resta object — vedi EmitUnboxObjectToType. JsonType è l'unico DataType già
            // mappato a System.Object in DataTypeMapper (AnyType non è mappato affatto).
            _ => isEntries ? (new Basic.JsonType(), new Basic.JsonType()) : (new Basic.JsonType(), null)
        };
    }

    /// <summary>
    /// Controllo di uscita (contatore oltre l'UPPER BOUND reale dell'array — LowerBound +
    /// Length - 1, mai un valore fisso) e assegnazione dell'elemento corrente alla
    /// variabile di iterazione, poi incremento del contatore — stesso schema logico di
    /// EmitForCheckAndJumpStatement + EmitNextStatement per FOR, ma qui in un unico Stmt
    /// (ForEachAdvanceAndAssignStmt combina entrambi i passi, a differenza di FOR che li
    /// tiene separati). L'assegnazione passa da <see cref="EmitStoreVariable(int, Action)"/>
    /// (non l'overload Expr) perché il valore viene da un Ldelem sull'array sorgente, non
    /// da un'espressione utente.
    /// </summary>
    private void EmitForEachAdvanceAndAssignStatement(ForEachAdvanceAndAssignStmt stmt)
    {
        if (_forEachState.TryGetValue(stmt.Variables[0].Lexeme, out var state))
        {
            // Bug corretto (2026-08-31, audit generator expressions): un Ldloc diretto su
            // state.Array.Local assume che l'array viva IN QUEL local — vero per un array
            // locale, MA FALSO per un array STATIC/GLOBALE (es. `DIM nums() = {...}` a
            // livello di modulo): quel local non viene MAI scritto per un array del
            // genere (l'autorità è il bag globale, LoadGlobal/StoreGlobal — vedi
            // ArrayInfo.LoadRef, già usato correttamente da EmitArrayRefFromInfo altrove,
            // es. EmitLoadVariable/EmitArrayElementAssignment) — restava quindi al suo
            // valore di default (0), letto poi come riferimento array via un cast non
            // verificabile a runtime (NullReferenceException: `FOR EACH n IN nums()` con
            // nums globale). EmitArrayRefFromInfo rispetta ArrayInfo.LoadRef quando
            // presente, altrimenti fa lo stesso Ldloc di prima — nessuna regressione per
            // gli array realmente locali.
            _il.Emit(OpCodes.Ldloc, state.CounterLocal);
            EmitArrayRefFromInfo(state.Array);
            _il.Emit(OpCodes.Ldlen);
            _il.Emit(OpCodes.Conv_I4);
            _il.Emit(OpCodes.Ldc_I4, state.Array.LowerBounds[0] - 1);
            _il.Emit(OpCodes.Add); // upperBound = LowerBound - 1 + Length
            _il.Emit(OpCodes.Cgt); // counter > upperBound?
            EmitBranch(stmt.ExitLabel, OpCodes.Brtrue);

            EmitStoreVariable(state.IteratorSymbolId, () =>
            {
                EmitArrayRefFromInfo(state.Array);
                _il.Emit(OpCodes.Ldloc, state.CounterLocal);
                if (state.Array.LowerBounds[0] != 0)
                {
                    _il.Emit(OpCodes.Ldc_I4, state.Array.LowerBounds[0]);
                    _il.Emit(OpCodes.Sub);
                }
                EmitArrayElementLoad(state.Array.ElementDataType);
            });

            _il.Emit(OpCodes.Ldloc, state.CounterLocal);
            _il.Emit(OpCodes.Ldc_I4_1);
            _il.Emit(OpCodes.Add);
            _il.Emit(OpCodes.Stloc, state.CounterLocal);
            return;
        }

        if (_forEachListState.TryGetValue(stmt.Variables[0].Lexeme, out var listState))
        {
            var feVarCount = listState.VarCount;
            var isEntries = listState.ValueIteratorSymbolId.HasValue;

            // Exit check: counter < list.Count ? continua : esci (Clt+Brfalse, equivalente a un
            // Bge diretto che IL non offre come singolo opcode booleano).
            _il.Emit(OpCodes.Ldloc, listState.CounterLocal);
            _il.Emit(OpCodes.Ldloc, listState.ListLocal);
            _il.Emit(OpCodes.Callvirt, isEntries ? ForEachEntriesListCountGetter : ForEachValuesListCountGetter);
            _il.Emit(OpCodes.Clt);
            EmitBranch(stmt.ExitLabel, OpCodes.Brfalse);

            if (feVarCount >= 3)
            {
                // Matrice con 3+ variabili (FOR EACH a, b, c IN m(3,3)): la riga (array 1D) è
                // in Item2 del Tuple (ForEachEntries) — destrutturala in N elementi:
                // a = riga[0], b = riga[1], c = riga[2]. Le variabili sono state tipate come
                // ElementDataType (UpdateSymbolType nell'init) e NON sono in _arrays (la
                // registrazione riga è solo per 1-2 var) — EmitStoreVariable le tratta come
                // local normali. Ogni variabile: carica entryLocal→Item2 (riga)→GetValue(mi)
                // →unbox al tipo elemento.
                var entryLocal = _il.DeclareLocal(typeof(Tuple<object?, object?>));
                _il.Emit(OpCodes.Ldloc, listState.ListLocal);
                _il.Emit(OpCodes.Ldloc, listState.CounterLocal);
                _il.Emit(OpCodes.Callvirt, ForEachEntriesListItemGetter);
                _il.Emit(OpCodes.Stloc, entryLocal);
                var feElemClrType = listState.ElementClrType ?? listState.KeyOrElementClrType;
                for (var mi = 0; mi < feVarCount; mi++)
                {
                    var vSymId = ResolveForEachIteratorSymbolId(stmt.Variables[mi], stmt.ScopeId);
                    EmitStoreVariable(vSymId, () =>
                    {
                        _il.Emit(OpCodes.Ldloc, entryLocal);
                        _il.Emit(OpCodes.Callvirt, TupleItem2Getter);
                        _il.Emit(OpCodes.Ldc_I4, mi);
                        _il.Emit(OpCodes.Call, _types.IlRuntimeArrayElementAtMethod);
                        // Valore null (destructuring parziale, N>colonne): per un target
                        // value-type serve il default (0), non Unbox_Any su null (NRE).
                        if (feElemClrType != typeof(object))
                        {
                            if (feElemClrType.IsValueType)
                            {
                                var notNull = _il.DefineLabel();
                                var done = _il.DefineLabel();
                                _il.Emit(OpCodes.Dup);
                                _il.Emit(OpCodes.Brtrue_S, notNull);
                                _il.Emit(OpCodes.Pop);
                                EmitDefaultValue(_program.Symbols.GetSymbol(vSymId).DataType ?? new DoubleType());
                                _il.Emit(OpCodes.Br_S, done);
                                _il.MarkLabel(notNull);
                                _il.Emit(OpCodes.Unbox_Any, feElemClrType);
                                _il.MarkLabel(done);
                            }
                            else
                                _il.Emit(OpCodes.Castclass, feElemClrType);
                        }
                    });
                }
            }
            else if (!isEntries && _arrays.TryGetValue(listState.IteratorSymbolId, out var feRowInfo) && feRowInfo.Rank == 1 && feRowInfo.Local is { } feRowLocal)
            {
                // Matrice con UNA variabile (FOR EACH row IN m): assegna la RIGA (array 1D,
                // da List<object?> come object) al local array registrato in _arrays.
                // EmitStoreVariable per un symbol in _arrays fa `emitValue() + Stloc local`:
                // qui emitValue pusha la riga e la CASTCLASS al tipo CLR del local array
                // (mai un stloc manuale — sarebbe uno store doppio → IL invalido).
                EmitStoreVariable(listState.IteratorSymbolId, () =>
                {
                    _il.Emit(OpCodes.Ldloc, listState.ListLocal);
                    _il.Emit(OpCodes.Ldloc, listState.CounterLocal);
                    _il.Emit(OpCodes.Callvirt, ForEachValuesListItemGetter);
                    _il.Emit(OpCodes.Castclass, feRowLocal.LocalType);
                });
            }
            else if (!isEntries)
            {
                EmitStoreVariable(listState.IteratorSymbolId, () =>
                {
                    _il.Emit(OpCodes.Ldloc, listState.ListLocal);
                    _il.Emit(OpCodes.Ldloc, listState.CounterLocal);
                    _il.Emit(OpCodes.Callvirt, ForEachValuesListItemGetter);
                    // Castclass/Unbox al tipo REALE della variabile (es. word$ AS STRING su
                    // un Trie boxed: KeyOrElementClrType è object, ma il local è string —
                    // senza castclass, callvirt ToUpperInvariant su un object = IL invalido
                    // con ILOptim). Unbox_Any/castclass sono no-op innocui se coincidono.
                    var iterSymbolType = _program.Symbols.GetSymbol(listState.IteratorSymbolId).DataType;
                    var iterClr = DataTypeMapper.Map(NormalizeInferredType(iterSymbolType), _types, _classTypes);
                    EmitUnboxObjectToType(iterClr);
                });
            }
            else
            {
                // Tuple<object?,object?> è un REFERENCE TYPE: l'Item getter restituisce il
                // riferimento, .Item1/.Item2 si leggono con un Callvirt diretto sul local — stesso
                // identico pattern del ramo a una variabile sopra, nessun local/indirizzo
                // aggiuntivo necessario (a differenza di un value type come KeyValuePair, che
                // avrebbe richiesto Ldloca+Call su un temporaneo per ogni accesso a proprietà).
                var entryLocal = _il.DeclareLocal(typeof(Tuple<object?, object?>));
                _il.Emit(OpCodes.Ldloc, listState.ListLocal);
                _il.Emit(OpCodes.Ldloc, listState.CounterLocal);
                _il.Emit(OpCodes.Callvirt, ForEachEntriesListItemGetter);
                _il.Emit(OpCodes.Stloc, entryLocal);

                EmitStoreVariable(listState.IteratorSymbolId, () =>
                {
                    _il.Emit(OpCodes.Ldloc, entryLocal);
                    _il.Emit(OpCodes.Callvirt, TupleItem1Getter);
                    EmitUnboxObjectToType(listState.KeyOrElementClrType);
                });

                var valueSymbolId = listState.ValueIteratorSymbolId!.Value;
                EmitStoreVariable(valueSymbolId, () =>
                {
                    _il.Emit(OpCodes.Ldloc, entryLocal);
                    _il.Emit(OpCodes.Callvirt, TupleItem2Getter);
                    // Matrice a 2 variabili (FOR EACH i, row IN m): Item2 è la RIGA (array 1D) —
                    // assegnala al local array registrato in _arrays (castclass), non unboxarla.
                    if (_arrays.TryGetValue(valueSymbolId, out var feRow2) && feRow2.Rank == 1 && feRow2.Local is { } feRow2Local)
                        _il.Emit(OpCodes.Castclass, feRow2Local.LocalType);
                    else
                        EmitUnboxObjectToType(listState.ValueClrType!);
                });
            }

            _il.Emit(OpCodes.Ldloc, listState.CounterLocal);
            _il.Emit(OpCodes.Ldc_I4_1);
            _il.Emit(OpCodes.Add);
            _il.Emit(OpCodes.Stloc, listState.CounterLocal);
            return;
        }

        throw new InvalidOperationException(
            $"Backend IL: nessun FOR EACH attivo per la variabile '{stmt.Variables[0].Lexeme}' (ForEachAdvanceAndAssignStmt senza ForEachInitStmt corrispondente).");
    }

    /// <summary>
    /// Converte un <c>object</c> dinamico (già in cima allo stack, tipicamente da
    /// List&lt;object?&gt;/Tuple estratti da IlRuntime.ForEachValues/ForEachEntries) al tipo
    /// CLR concreto della variabile di destinazione — Unbox_Any per i value type (double,
    /// bool, UDT struct...), Castclass per i reference type diversi da object stesso (string,
    /// List&lt;T&gt;...), nessuna conversione se il target è già <c>object</c> (es. una
    /// variabile JSON). Stesso pattern già usato da EmitReceiveStatement per il valore
    /// dinamico di un canale (IslandRuntime.Receive restituisce anch'esso object?).
    /// </summary>
    private void EmitUnboxObjectToType(Type clrType)
    {
        if (clrType != typeof(object) && clrType.IsValueType)
            _il.Emit(OpCodes.Unbox_Any, clrType);
        else if (clrType != typeof(object))
            _il.Emit(OpCodes.Castclass, clrType);
    }

    /// <summary>
    /// Risolve il SymbolId di una variabile di iterazione FOR EACH: il nodo AST porta solo
    /// un Token grezzo (mai un SymbolId proprio, a differenza di un parametro/DIM — vedi
    /// Ast/Loops.cs) perché il Linker la registra per NOME nello scope del loop
    /// (Linker.Binding.cs, case ForEachInitStmt) invece di riscrivere il nodo. Cerca quindi
    /// tra tutti i simboli con quel nome quello il cui ScopeId coincide con quello del loop
    /// (stampato su ogni Stmt dal binder — vedi <c>scopedStmt</c> in Linker.Binding.cs).
    /// </summary>
    private int ResolveForEachIteratorSymbolId(Token variableToken, int scopeId)
    {
        var match = _program.Symbols.FindSymbolsByName(variableToken.Lexeme).FirstOrDefault(s => s.ScopeId == scopeId);
        // Il token può portare il suffisso ($, %, ...) ma il Linker registra il symbol col
        // nome SENZA suffisso (es. item$ → item): fallback sul nome pulito.
        if (match is null && variableToken.Lexeme.Length > 0 && "\u0024%&#!".Contains(variableToken.Lexeme[^1]))
        {
            var cleanName = variableToken.Lexeme[..^1];
            match = _program.Symbols.FindSymbolsByName(cleanName).FirstOrDefault(s => s.ScopeId == scopeId);
        }
        if (match is null)
            throw new NotSupportedException($"Backend IL: impossibile risolvere il simbolo della variabile di iterazione FOR EACH '{variableToken.Lexeme}' (scope {scopeId}).");
        return match.Id;
    }

    private void EmitConditionalJumpStatement(ConditionalJumpStmt stmt)
    {
        EmitConditionBool(stmt.Condition);
        EmitBranch(stmt.InitialTargetLabel, stmt.JumpWhenTrue ? OpCodes.Brtrue : OpCodes.Brfalse);
    }

}
