// FILE: IlProgramCompiler.Gpm.cs
// PARTIAL — dominio GPM/ON-ERROR/GOSUB dispatch (porting da GMBasic, chunk dispatch).
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

/// <summary>Partial del transpiler IL: dominio GPM / ON-ERROR / GOSUB dispatch.</summary>
public sealed partial class IlProgramCompiler
{

    private void InitializeGosubRuntime(ImmutableArray<Stmt> statements)
    {
        var gosubs = statements
            .SelectMany(WalkAst)
            .OfType<GosubStmt>()
            .ToArray();
        if (gosubs.Length == 0) return;

        _gosubReturnStackLocal = _il.DeclareLocal(typeof(Stack<int>));
        _il.Emit(OpCodes.Newobj, GosubStackConstructor);
        _il.Emit(OpCodes.Stloc, _gosubReturnStackLocal);

        _gosubReturnLabels = new Label[gosubs.Length];
        for (var i = 0; i < gosubs.Length; i++)
        {
            var returnLabel = _il.DefineLabel();
            _gosubReturnLabels[i] = returnLabel;
            _gosubSites.Add(gosubs[i], (i, returnLabel));
        }
    }

    /// <summary>
    /// True se il modulo principale usa ON ERROR GOTO/RESUME NEXT o RESUME da qualche
    /// parte (in un IfStmt annidato incluso) — determina se EmitEntryPoint deve passare
    /// alla compilazione "a stati" (EmitEntryPointWithErrorHandling) invece del percorso
    /// lineare semplice. Un programma che non usa mai questa famiglia di istruzioni non
    /// paga alcun costo (compila esattamente come prima).
    /// </summary>
    private static bool UsesOnErrorHandling(BasicModule mainModule) =>
        mainModule.ExecutionOrder.Any(line => mainModule.Statements[line].Any(StatementUsesOnErrorHandling));

    private static IEnumerable<int> EnumerateDeclarationSymbols(ImmutableArray<Stmt> body)
    {
        foreach (var stmt in body)
            if (stmt is DeclarationStmt ds)
                foreach (var d in ds.Declarations)
                    yield return d.SymbolId;
    }

    private static IEnumerable<VariableExpr> EnumerateVariableExprs(ImmutableArray<Stmt> body)
    {
        foreach (var stmt in body)
            foreach (var n in WalkAst(stmt))
                if (n is VariableExpr ve) yield return ve;
    }
    private static IEnumerable<AstNode> WalkAst(AstNode node)
    {
        yield return node;
        foreach (var prop in node.GetType().GetProperties(System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance))
        {
            // Parent è una BACK-REFERENCE al nodo genitore (parser/linker la popolano su
            // FunctionDefinition/UDT/CLASS): visitarla ricorsivamente crea un CICLO
            // (figlio→Parent→figlio...) → StackOverflow (es. TYPE X IMPLEMENTS IFoo con
            // metodi, o TypesGenericType). È una vista, non fa parte dell'albero reale.
            if (prop.Name == "Parent") continue;
            var value = prop.GetValue(node);
            if (value is AstNode child) foreach (var n in WalkAst(child)) yield return n;
            else if (value is System.Collections.IEnumerable en && value is not string)
            {
                // ImmutableArray<T>.Default implementa IEnumerable ma GetEnumerator
                // lancia: alcuni nodi AST lasciano intenzionalmente opzionali queste
                // collezioni. La visita riflessiva deve trattarle come vuote.
                if (value.GetType().GetProperty("IsDefault")?.GetValue(value) is true)
                    continue;
                foreach (var item in en)
                    if (item is AstNode childItem) foreach (var n in WalkAst(childItem)) yield return n;
            }
        }
    }

    private static bool StatementUsesOnErrorHandling(Stmt stmt) => stmt switch
    {
        OnErrorStmt or OnErrorResumeStmt or ResumeStmt => true,
        IfStmt ifs => ifs.IfClauses.Any(c => c.Statements.Any(StatementUsesOnErrorHandling))
                      || (ifs.ElseClause?.Statements.Any(StatementUsesOnErrorHandling) ?? false),
        _ => false
    };

    /// <summary>
    /// Compila l'entry point quando ON ERROR GOTO/RESUME NEXT/RESUME è usato da qualche
    /// parte nel modulo — un modello di esecuzione radicalmente diverso da
    /// <see cref="EmitEntryPoint"/>, necessario perché un errore può essere sollevato
    /// ovunque e deve poter "riprendere" l'esecuzione a un numero di riga arbitrario, cosa
    /// che lo strutturato try/catch/finally nativo di IL non permette direttamente: non si
    /// può saltare (Br) dentro il CORPO di una regione protetta dall'esterno (solo
    /// entrarci dalla sua PRIMA istruzione), e una volta usciti da un try (anche per
    /// un'eccezione) non si può rientrarci con un salto — solo rieseguendo la regione da
    /// capo.
    ///
    /// Soluzione: ogni RIGA diventa il proprio blocco try/catch (mai un singolo try per
    /// l'intera Main — quello impedirebbe di "rientrare" a una riga arbitraria dopo un
    /// errore, dato che l'unico punto di ingresso di un try è la sua prima istruzione).
    /// Un ciclo di dispatch ESTERNO a ogni try (mai annidato in nessuno di essi) confronta
    /// un locale "_state" (la prossima riga da eseguire) con ogni riga nota ed entra nel
    /// blocco try corrispondente saltando alla sua PRIMA istruzione — questo è legale
    /// (un ingresso dall'esterno alla prima istruzione di una regione protetta è permesso,
    /// a differenza di un salto al suo interno). Ogni chunk, completato normalmente o
    /// gestito un errore, aggiorna _state e fa Leave (mai Br) verso il ciclo di dispatch,
    /// che rientra da capo — questo è ciò che rende possibile il "rientro" a una riga
    /// arbitraria: non si rientra MAI nello STESSO blocco try, se ne entra uno NUOVO
    /// (quello della riga di destinazione) dal suo inizio.
    ///
    /// Conseguenza per ogni ALTRO emettitore di salto (GOTO/etichette con nome per
    /// l'appiattimento IF/loop/uscita FOR/uscita FOR EACH/END): un salto diretto (Br) verso
    /// una riga DIVERSA dal chunk corrente attraverserebbe il confine tra due try
    /// SEPARATI — IL non valido. Vedi EmitBranch/EmitSetStateAndLeave, controllati da
    /// <see cref="_onErrorDispatch"/> (non null SOLO durante questo metodo): traducono quei
    /// salti in "imposta lo stato, poi Leave verso il dispatcher" quando serve, altrimenti
    /// si comportano esattamente come nel percorso semplice.
    ///
    /// Nessun errore anche solo POTENZIALMENTE gestito da un handler diventa una vera
    /// eccezione CLR non gestita immediatamente: ogni chunk cattura SEMPRE
    /// Basic.BasicException, e nel proprio catch controlla se un handler è installato
    /// (_errorHandlerTarget) — se non lo è, Rethrow (comportamento identico a un errore
    /// non gestito nel percorso semplice, dove nessun try esiste affatto).
    /// </summary>
    private void EmitEntryPointWithErrorHandling(BasicModule mainModule)
    {
        _localByName.Clear();
        var stateLocal = _il.DeclareLocal(_types.Int32);
        var errorHandlerTargetLocal = _il.DeclareLocal(_types.Int32);
        var errorLineLocal = _il.DeclareLocal(_types.Int32);
        var isGosubLocal = _il.DeclareLocal(_types.Int32);
        // Riga BASIC corrente per ERR.LineNumber (vedi EmitStatement).
        _currentLineLocal = _il.DeclareLocal(_types.Int32);
        var dispatchLoopStart = _il.DefineLabel();
        var doneLabel = _il.DefineLabel();

        // BuildDispatchChunks (porting da GMBasic, vedi IL-OnError-Loop-Bug.md):
        // un chunk per riga NON basta quando una riga contiene un'etichetta A METÀ — il
        // Parser genera etichette sintetiche (FOR_START/FOR_CONTINUE/ecc.) che condividono
        // la riga fisica del FOR/NEXT con statement che NON vanno ri-eseguiti quando il
        // dispatcher "rientra" a quell'etichetta dopo un loop-back (es. ForStmt, l'init del
        // loop) — senza lo split il contatore verrebbe riazzerato a ogni iterazione, loop
        // infinito. chunk.State == numero di riga reale per il PRIMO sotto-segmento di ogni
        // riga (compatibilità GOTO/RESUME per numero), sintetico NEGATIVO per gli altri;
        // chunk.OwnerLine resta SEMPRE il vero numero di riga BASIC.
        var chunks = BuildDispatchChunks(mainModule.ExecutionOrder, ln => mainModule.Statements[ln]);
        var executionOrderStates = chunks.Select(c => c.State).ToImmutableArray();

        InitializeDispatchedGosubRuntime(chunks);

        _il.Emit(OpCodes.Ldc_I4, chunks[0].State);
        _il.Emit(OpCodes.Stloc, stateLocal);
        _il.Emit(OpCodes.Ldc_I4, OnErrorNoHandler);
        _il.Emit(OpCodes.Stloc, errorHandlerTargetLocal);
        _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Stloc, isGosubLocal);

        var chunkLabels = new Dictionary<int, Label>();
        foreach (var chunk in chunks)
        {
            chunkLabels[chunk.State] = _il.DefineLabel();
            foreach (var stmt in chunk.Statements)
                PreScanLabels(stmt, chunk.State);
        }

        _il.MarkLabel(dispatchLoopStart);
        foreach (var chunk in chunks)
        {
            _il.Emit(OpCodes.Ldloc, stateLocal);
            _il.Emit(OpCodes.Ldc_I4, chunk.State);
            _il.Emit(OpCodes.Beq, chunkLabels[chunk.State]);
        }
        _il.Emit(OpCodes.Br, doneLabel);

        for (var i = 0; i < chunks.Count; i++)
        {
            var chunk = chunks[i];
            var nextState = i + 1 < chunks.Count ? chunks[i + 1].State : OnErrorDoneState;

            _il.MarkLabel(chunkLabels[chunk.State]);

            var chunkEndLabel = _il.BeginExceptionBlock();
            _onErrorDispatch = (stateLocal, errorHandlerTargetLocal, errorLineLocal, isGosubLocal, chunkEndLabel, executionOrderStates);
            EmitDispatchedGosubContinuationJumps(chunk.State);

            foreach (var stmt in chunk.Statements)
                EmitStatement(stmt);
            // Uno statement che termina il chunk (GOTO/JUMP/END/STOP/RETURN con dispatch)
            // ha GIÀ emesso un Leave: un secondo Leave qui sarebbe FUORI dal blocco try
            // (il primo ha già lasciato la regione protetta) → IL invalido.
            if (chunk.Statements.Count == 0 || !IsChunkTerminatingStatement(chunk.Statements[^1]))
            {
                _il.Emit(OpCodes.Ldc_I4, nextState);
                _il.Emit(OpCodes.Stloc, stateLocal);
                _il.Emit(OpCodes.Leave, chunkEndLabel);
            }

            // ON ERROR non lega mai l'eccezione a una variabile propria (a differenza di
            // CATCH ex AS ...), ma il codice dell'handler può comunque leggere ERR.Number/
            // ERR.Description — vanno quindi popolati nel simbolo globale ERR (StdLib), non
            // scartati con un semplice Pop (altrimenti NullReferenceException alla prima
            // lettura di ERR.* nell'handler).
            var onErrorCaughtLocal = _il.DeclareLocal(_types.BasicExceptionType);

            // Il dispatch che segue è IDENTICO indipendentemente da quale dei due catch
            // (BasicException "nativa" del linguaggio, o un'eccezione CLR grezza avvolta
            // sotto — vedi il secondo BeginCatchBlock più sotto) ha popolato
            // onErrorCaughtLocal — emesso due volte (funzione locale, non IL condivisa: i
            // due catch sono regioni protette SEPARATE) invece di duplicato a mano. Non
            // compila codice utente (a differenza del dispatch di TRY/CATCH), quindi
            // sicuro da emettere due volte.
            void EmitOnErrorDispatch()
            {
                EmitCaptureErrorIntoGlobalErr(onErrorCaughtLocal);
                // errorLineLocal va impostato QUI, non all'ingresso del chunk: deve
                // riflettere lo STATO del chunk che ha ORIGINATO l'errore (non
                // necessariamente il numero di riga reale — vedi BuildDispatchChunks) e
                // restare STABILE finché RESUME/RESUME NEXT non lo consuma, anche
                // attraverso chunk intermedi (es. l'handler stesso, su righe SUCCESSIVE,
                // che altrimenti lo sovrascriverebbero prima che RESUME lo legga).
                _il.Emit(OpCodes.Ldc_I4, chunk.State);
                _il.Emit(OpCodes.Stloc, errorLineLocal);

                var noHandlerLabel = _il.DefineLabel();
                var resumeNextLabel = _il.DefineLabel();
                var procedureTargetLabel = _il.DefineLabel();
                _il.Emit(OpCodes.Ldloc, errorHandlerTargetLocal);
                _il.Emit(OpCodes.Ldc_I4, OnErrorNoHandler);
                _il.Emit(OpCodes.Beq, noHandlerLabel);
                _il.Emit(OpCodes.Ldloc, errorHandlerTargetLocal);
                _il.Emit(OpCodes.Ldc_I4, OnErrorResumeNextMode);
                _il.Emit(OpCodes.Beq, resumeNextLabel);

                // ON ERROR GOTO <riga>: salta all'handler installato.
                _il.Emit(OpCodes.Ldloc, errorHandlerTargetLocal);
                _il.Emit(OpCodes.Ldc_I4, OnErrorProcedureTargetBase);
                _il.Emit(OpCodes.Ble, procedureTargetLabel);

                _il.Emit(OpCodes.Ldloc, errorHandlerTargetLocal);
                _il.Emit(OpCodes.Stloc, stateLocal);
                _il.Emit(OpCodes.Leave, chunkEndLabel);

                _il.MarkLabel(procedureTargetLabel);
                // ON ERROR GOTO/GOSUB NomeProcedura: dispatch compile-time della sentinella.
                var procedureDoneLabel = _il.DefineLabel();
                foreach (var (sentinel, method) in _onErrorProcedureTargets)
                {
                    _il.Emit(OpCodes.Ldloc, errorHandlerTargetLocal);
                    _il.Emit(OpCodes.Ldc_I4, sentinel);
                    _il.Emit(OpCodes.Bne_Un, procedureDoneLabel);
                    _il.Emit(OpCodes.Call, method);
                    _il.Emit(OpCodes.Br, procedureDoneLabel);
                }
                _il.MarkLabel(procedureDoneLabel);
                _il.Emit(OpCodes.Ldc_I4, nextState);
                _il.Emit(OpCodes.Stloc, stateLocal);
                _il.Emit(OpCodes.Leave, chunkEndLabel);

                _il.MarkLabel(resumeNextLabel);
                // ON ERROR RESUME NEXT: ignora l'errore, continua dal chunk successivo.
                _il.Emit(OpCodes.Ldc_I4, nextState);
                _il.Emit(OpCodes.Stloc, stateLocal);
                _il.Emit(OpCodes.Leave, chunkEndLabel);

                _il.MarkLabel(noHandlerLabel);
                // Nessun gestore installato: comportamento identico a un errore non
                // gestito nel percorso semplice (nessun try lì) — ripropaga. Rethrow
                // valido anche emesso da questa funzione locale condivisa: finisce
                // comunque testualmente dentro la regione catch specifica da cui questa
                // chiamata proviene (BasicException o CLR grezza rispettivamente).
                _il.Emit(OpCodes.Rethrow);
            }

            _il.BeginCatchBlock(_types.BasicExceptionType);
            _il.Emit(OpCodes.Stloc, onErrorCaughtLocal);
            EmitOnErrorDispatch();

            // Secondo catch: un'eccezione CLR nativa (IndexOutOfRangeException/
            // FileNotFoundException/...) mai avvolta in una BasicException — senza
            // questo bypasserebbe del tutto ON ERROR (vedi 
            // IL-OnError-Loop-Bug.md, gap array-out-of-range/file mancante). Avvolta con
            // FromClrException usando OwnerLine (SEMPRE un vero numero di riga BASIC,
            // anche quando chunk.State è sintetico/negativo).
            _il.BeginCatchBlock(_types.ExceptionType);
            var clrCaughtLocal = _il.DeclareLocal(_types.ExceptionType);
            _il.Emit(OpCodes.Stloc, clrCaughtLocal);
            // Se l'eccezione è GIÀ una BasicException (l'ordine dei due catch non è garantito
            // su tutti i runtime/JIT — osservato che questo ramo vince sul precedente), va usata
            // AS-IS: FromClrException la rimapperebbe a InternalError (200) perdendo il codice
            // vero → `ERR.Number` nell'handler ON ERROR stampava 200 anche per `ERROR 11` e per
            // la divisione per zero (ERR 11). Stessa guardia già presente in EmitTryCatchStatement.
            _il.Emit(OpCodes.Ldloc, clrCaughtLocal);
            _il.Emit(OpCodes.Isinst, _types.BasicExceptionType);
            var alreadyBasicLabel = _il.DefineLabel();
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Brtrue, alreadyBasicLabel);
            _il.Emit(OpCodes.Pop);
            _il.Emit(OpCodes.Ldloc, clrCaughtLocal);
            _il.Emit(OpCodes.Ldc_I4_0);
            if (_currentLineLocal is { } clrLineLocal) _il.Emit(OpCodes.Ldloc, clrLineLocal);
            else _il.Emit(OpCodes.Ldc_I4, chunk.OwnerLine);
            _il.Emit(OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Newobj, _types.SourcePositionConstructor);
            _il.Emit(OpCodes.Call, _types.BasicExceptionFromClrExceptionMethod);
            _il.MarkLabel(alreadyBasicLabel);
            _il.Emit(OpCodes.Stloc, onErrorCaughtLocal);
            EmitOnErrorDispatch();

            _il.EndExceptionBlock();
            _onErrorDispatch = null;

            _il.Emit(OpCodes.Br, dispatchLoopStart);
        }

        _il.MarkLabel(doneLabel);
        // Entry top-level + SUB Main con ON ERROR: esegue Main dopo il dispatcher.
        EmitTrailingMainCall();
        // Finalizzazione deterministica dei globali CLASS/UDT con DESTRUCTOR (parità PCode/VMC).
        EmitFinalizeGlobalObjects();
        _il.Emit(OpCodes.Ret);
        _gosubReturnStackLocal = null;
        _gosubResumeSiteLocal = null;
        _dispatchedGosubSites.Clear();
        _dispatchedTryEntryLabels.Clear();
    }

    private void InitializeDispatchedGosubRuntime(IReadOnlyList<DispatchChunk> chunks)
    {
        var nextId = 0;
        foreach (var chunk in chunks)
        {
            foreach (var statement in chunk.Statements)
            {
                WalkAst(statement, node =>
                {
                    if (node is TryCatchStmt tryCatch)
                    {
                        RegisterTryCatchGosubs(tryCatch, chunk.State, ref nextId);
                        return false;
                    }
                    if (node is GosubStmt gosub)
                        _dispatchedGosubSites.Add(gosub,
                            new DispatchedGosubSite(nextId++, chunk.State, _il.DefineLabel(), null));
                    return true;
                });
            }
        }
        if (_dispatchedGosubSites.Count == 0) return;

        _gosubReturnStackLocal = _il.DeclareLocal(typeof(Stack<int>));
        _il.Emit(OpCodes.Newobj, GosubStackConstructor);
        _il.Emit(OpCodes.Stloc, _gosubReturnStackLocal);
        _gosubResumeSiteLocal = _il.DeclareLocal(_types.Int32);
        _il.Emit(OpCodes.Ldc_I4_M1);
        _il.Emit(OpCodes.Stloc, _gosubResumeSiteLocal);
    }

    private void RegisterTryCatchGosubs(TryCatchStmt statement, int ownerState, ref int nextId)
    {
        _dispatchedTryEntryLabels.Add(statement, _il.DefineLabel());
        if (statement.Finally is null)
        {
            foreach (var child in statement.TryBody)
                RegisterGosubsInRegion(child, ownerState, statement, ref nextId);
        }
        foreach (var catchClause in statement.CatchClauses)
        {
            foreach (var child in catchClause.Body)
                RegisterGosubsInRegion(child, ownerState, null, ref nextId);
        }
    }

    private void RegisterGosubsInRegion(object node, int ownerState, TryCatchStmt? protectedTry, ref int nextId)
    {
        var discovered = new List<GosubStmt>();
        WalkAst(node, child =>
        {
            if (child is TryCatchStmt) return false;
            if (child is GosubStmt gosub) discovered.Add(gosub);
            return true;
        });
        foreach (var gosub in discovered)
            _dispatchedGosubSites.Add(gosub,
                new DispatchedGosubSite(nextId++, ownerState, _il.DefineLabel(), protectedTry));
    }

    private void EmitDispatchedGosubContinuationJumps(int ownerState)
    {
        if (_gosubResumeSiteLocal is null) return;
        foreach (var site in _dispatchedGosubSites.Values)
        {
            if (site.OwnerState != ownerState) continue;
            _il.Emit(OpCodes.Ldloc, _gosubResumeSiteLocal);
            _il.Emit(OpCodes.Ldc_I4, site.Id);
            _il.Emit(OpCodes.Beq, site.ProtectedTry is null
                ? site.ReturnLabel
                : _dispatchedTryEntryLabels[site.ProtectedTry]);
        }
    }

    private void EmitTryGosubContinuationJumps(TryCatchStmt statement)
    {
        if (_gosubResumeSiteLocal is null) return;
        foreach (var site in _dispatchedGosubSites.Values)
        {
            if (!ReferenceEquals(site.ProtectedTry, statement)) continue;
            _il.Emit(OpCodes.Ldloc, _gosubResumeSiteLocal);
            _il.Emit(OpCodes.Ldc_I4, site.Id);
            _il.Emit(OpCodes.Beq, site.ReturnLabel);
        }
    }

    /// <summary>
    /// Un chunk try/catch del pattern ON ERROR per-riga: <see cref="DispatchChunk.State"/>
    /// è il valore dispatch che il locale "_state" confronta per rientrarci (== numero di
    /// riga BASIC reale per il PRIMO sotto-segmento di una riga, altrimenti uno stato
    /// sintetico NEGATIVO — vedi <see cref="BuildDispatchChunks"/>); OwnerLine resta SEMPRE
    /// il numero di riga BASIC reale da cui il chunk proviene (mai negativo). Porting
    /// identico da GMBasic (vedi IL-OnError-Loop-Bug.md).
    /// </summary>
    private sealed record DispatchChunk(int State, int OwnerLine, List<Stmt> Statements);

    /// <summary>
    /// Suddivide gli statement di ogni riga in chunk più fini quando una riga contiene
    /// un'etichetta A METÀ — vedi il commento in <see cref="EmitEntryPointWithErrorHandling"/>
    /// e <see cref="DispatchChunk"/>. IL vieta di entrare in una regione protetta se non
    /// dalla sua PRIMA istruzione, quindi ogni etichetta a metà riga diventa l'inizio del
    /// proprio blocco try/catch indipendente. Porting identico da GMBasic.
    /// </summary>
    private static List<DispatchChunk> BuildDispatchChunks(IReadOnlyList<int> executionOrder, Func<int, List<Stmt>> statementsForLine)
    {
        var chunks = new List<DispatchChunk>();
        var syntheticState = -2; // -1 è già OnErrorDoneState/OnErrorNoHandler.
        // Deduplica le RIGHE: se `executionOrder` ripete un numero di riga, emetterne di
        // nuovo gli statement marca due volte le sue Label (utente e sintetiche) →
        // "Label defined multiple times" da ILGenerator.MarkLabel. Il dispatch emette il
        // codice di ogni riga UNA volta e ci salta via state (loop/GOTO): una riga ripetuta
        // in `executionOrder` è duplicazione (es. riga registrata due volte con lambda/DSL),
        // non un secondo punto di esecuzione.
        foreach (var lineNumber in executionOrder.Distinct())
        {
            var segments = SplitAtLabelBoundaries(statementsForLine(lineNumber));
            for (var s = 0; s < segments.Count; s++)
            {
                var state = s == 0 ? lineNumber : syntheticState--;
                chunks.Add(new DispatchChunk(state, lineNumber, segments[s]));
            }
        }
        return chunks;
    }

    /// <summary>
    /// Spezza una lista di statement in segmenti di UNA sola istruzione ciascuno: ogni
    /// statement diventa il proprio chunk/try-catch (vedi BuildDispatchChunks). Prima
    /// raggruppava tutto quanto tra un'etichetta e la successiva in un chunk unico — con
    /// ON ERROR RESUME NEXT attivo, un'eccezione a metà di un blocco senza etichette
    /// faceva risalire lo stato al chunk SUCCESSIVO (l'unico noto), saltando in
    /// silenzio TUTTE le istruzioni restanti del blocco invece di riprendere da quella
    /// subito dopo (bug scoperto dal vivo su DbfRelationJoin.bas: ON ERROR RESUME NEXT +
    /// più KILL senza etichette in mezzo — se una KILL lanciava, l'intero resto di Main()
    /// veniva scartato, exit code 0, nessun errore visibile). Granularità per-istruzione
    /// risolve sia RESUME NEXT (implicito e nella forma esplicita dentro un handler ON
    /// ERROR GOTO) sia il RESUME nudo (rilancia la stessa istruzione, non l'intero
    /// blocco) — RESUME &lt;etichetta&gt; resta invariato: un'etichetta è comunque sempre
    /// la prima (e unica) istruzione del proprio chunk.
    /// </summary>
    private static List<List<Stmt>> SplitAtLabelBoundaries(IReadOnlyList<Stmt> lineStatements)
    {
        var segments = new List<List<Stmt>>(lineStatements.Count);
        foreach (var stmt in lineStatements)
            segments.Add([stmt]);
        return segments;
    }

    /// <summary>Un statement che, con ON ERROR attivo, termina il chunk per-riga emettendo
    /// già un Leave (GOTO/JUMP/END/STOP/RETURN con dispatch): dopo di esso il Leave finale
    /// del chunk sarebbe fuori dal blocco try → IL invalido. Porting identico da GMBasic.</summary>
    private static bool IsChunkTerminatingStatement(Stmt stmt) =>
        stmt is GotoStmt or JumpStmt or EndStmt or StopStmt or ReturnStmt or EndProcedureStmt;

    /// <summary>
    /// DATA non è codice eseguibile: il linker raccoglie i valori per modulo e l'IL
    /// entry point materializza il pool una sola volta prima della prima riga. Il pool
    /// resta nel runtime per tutta Main, quindi READ/RESTORE funzionano anche attraverso
    /// GOTO e il dispatcher ON ERROR.
    /// </summary>
    private void EmitInitializeDataPool(BasicModule mainModule)
    {
        // Il DataStore del linker è la fonte del runtime P-Code e può aggregare più
        // moduli. L'entry point IL però compila il solo MainModule: ricostruire qui il
        // pool dalle sue DATA conserva esattamente l'ordine delle righe eseguibili e non
        // dipende dall'identità/normalizzazione del FilePath usata dal loader.
        var values = mainModule.ExecutionOrder
            .SelectMany(line => mainModule.Statements[line])
            .OfType<DataStmt>()
            .SelectMany(data => data.Values)
            .ToImmutableArray();

        _il.Emit(OpCodes.Ldc_I4, values.Length);
        _il.Emit(OpCodes.Newarr, _types.Object);
        for (var index = 0; index < values.Length; index++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, index);
            EmitDataLiteral(values[index]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, IlRuntimeBeginDataMethod);
    }

    private void EmitDataLiteral(Expr value)
    {
        switch (value)
        {
            case NumberExpr number: _il.Emit(OpCodes.Ldc_R8, number.Value); _il.Emit(OpCodes.Box, _types.Double); break;
            case StringExpr text: _il.Emit(OpCodes.Ldstr, text.Value); break;
            default: throw new NotSupportedException($"Backend IL: valore DATA '{value}' ({value.GetType().Name}) non supportato.");
        }
    }

    /// <summary>Registra all'avvio ogni procedura BASIC (nome → symbolId + nomi parametri)
    /// via IlRuntime.RegisterBasicProcedure. Serve al dispatcher MCP (IlMcpToolDispatcher)
    /// per invocare per nome una procedura registrata come tool (tools/call).</summary>
    private void EmitProcedureRegistrations()
    {
        // Le Ensure girano SEMPRE, anche senza procedure BASIC (il guard del loop sotto
        // non deve saltarle: MCP dispatcher e SQLite servono anche ai programmi piu' semplici).
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.EnsureMcpDispatcher)));
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.EnsureSqlite)));
        // S_eval §ii: assembly del programma per risolvere le SUB/FUNCTION chiamate da
        // EVAL/macro (IlRuntime.InvokeBasicProcedureByName). Chiamato da TUTTE le entry
        // (classic/GPM/top-level): sovrascrive come SetMacroContext (single-program).
        _il.Emit(OpCodes.Call, typeof(System.Reflection.Assembly).GetMethod(nameof(System.Reflection.Assembly.GetExecutingAssembly))!);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SetProcedureAssembly), typeof(System.Reflection.Assembly)));
        if (_procedureDefinitions.Count == 0) return;
        foreach (var (symbolId, definition) in _procedureDefinitions)
        {
            var paramNamesCsv = string.Join(",", definition.Parameters.Select(p => p.OriginalName.Lexeme));
            _il.Emit(OpCodes.Ldstr, definition.OriginalName.Lexeme);
            _il.Emit(OpCodes.Ldc_I4, symbolId);
            _il.Emit(OpCodes.Ldstr, paramNamesCsv);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RegisterBasicProcedure)));
        }
    }

    /// <summary>
    /// ON ERROR GOTO/GOSUB &lt;riga/etichetta&gt; o ON ERROR GOTO 0 (disabilita) — imposta il
    /// locale "handler installato" condiviso per l'intera entry point (vedi
    /// <see cref="_onErrorDispatch"/>). GOTO e GOSUB installano l'handler in modo
    /// IDENTICO (stesso <see cref="_onErrorDispatch"/>.ErrorHandlerTarget, il salto al
    /// verificarsi di un errore non cambia) — l'UNICA differenza è cosa fa un RETURN
    /// (nudo) eseguito dentro il corpo dell'handler: solo per GOSUB deve comportarsi come
    /// RESUME NEXT (vedi EmitReturnStatement e
    /// Interpreter/Tests/Exceptions.cs::SampleOnErrorResume, l'unica fonte di verità
    /// trovata su questa semantica — sia il p-code Compiler/VM sia l'interprete
    /// tree-walker hanno il campo equivalente "ErrorHandlerIsGosub" predisposto ma MAI
    /// realmente cablato a un comportamento diverso per RETURN, quindi non riproducibile
    /// da un riferimento funzionante). <see cref="_onErrorDispatch"/>.IsGosub (un
    /// int 0/1, mai un vero bool per coerenza con lo stile "tutto Int32" già usato per
    /// State/ErrorHandlerTarget) registra quale dei due è stato installato per ultimo. Come
    /// ErrorHandlerTarget, NON viene azzerato da RESUME (vedi EmitResumeStatement) — resta
    /// "sticky", valido anche per un successivo errore gestito dallo STESSO handler, finché
    /// una nuova ON ERROR GOTO/GOSUB non lo sovrascrive.
    /// </summary>
    private void EmitOnErrorStatement(OnErrorStmt stmt)
    {
        if (_onErrorDispatch is not { } dispatch)
            throw new NotSupportedException("Backend IL: ON ERROR usato fuori dall'entry point con gestione errori attiva (bug interno).");

        var isDisable = stmt.Target is { Kind: TokenKind.Number, Literal: double n } && n == 0;
        int target;
        if (isDisable)
        {
            target = OnErrorNoHandler;
        }
        else if (stmt.Target is not null)
        {
            target = ResolveJumpTargetLine(stmt.Target) ?? ResolveOnErrorProcedureTarget(stmt.Target)
                ?? throw new NotSupportedException("Backend IL: target di ON ERROR GOTO/GOSUB non risolvibile a un numero di riga o a una procedura.");
        }
        else
        {
            throw new NotSupportedException("Backend IL: ON ERROR senza GOTO/GOSUB e senza target non supportato.");
        }

        _il.Emit(OpCodes.Ldc_I4, target);
        _il.Emit(OpCodes.Stloc, dispatch.ErrorHandlerTarget);

        _il.Emit(OpCodes.Ldc_I4, stmt.Keyword?.Kind == TokenKind.Gosub ? 1 : 0);
        _il.Emit(OpCodes.Stloc, dispatch.IsGosub);
    }

    /// <summary>ON ERROR RESUME NEXT: imposta la modalità "ignora l'errore, continua dalla riga successiva" — nessun salto a un handler esplicito.</summary>
    private void EmitOnErrorResumeStatement(OnErrorResumeStmt stmt)
    {
        if (_onErrorDispatch is not { } dispatch)
            throw new NotSupportedException("Backend IL: ON ERROR RESUME NEXT usato fuori dall'entry point con gestione errori attiva (bug interno).");
        _il.Emit(OpCodes.Ldc_I4, OnErrorResumeNextMode);
        _il.Emit(OpCodes.Stloc, dispatch.ErrorHandlerTarget);
    }

    /// <summary>
    /// RESUME/RESUME NEXT/RESUME &lt;riga&gt;: usato dentro il codice di un handler ON
    /// ERROR GOTO per riprendere l'esecuzione normale. Come EndStmt/StopStmt, un salto
    /// simile non può mai essere un Br diretto qui (siamo sempre dentro il try del chunk
    /// corrente) — imposta lo stato e fa Leave verso il dispatcher. RESUME (nudo) rilegge
    /// direttamente <see cref="_onErrorDispatch"/>.ErrorLine (la riga che ha originato
    /// l'errore corrente, non necessariamente quella in cui questo RESUME si trova
    /// testualmente); RESUME NEXT deve invece risalire a RUNTIME dalla stessa riga alla
    /// SUCCESSIVA nell'ordine di esecuzione (non nota a compile-time, dato che dipende da
    /// quale riga ha effettivamente fallito) — vedi EmitResumeNextTarget.
    /// </summary>
    private void EmitResumeStatement(ResumeStmt stmt)
    {
        if (_onErrorDispatch is not { } dispatch)
            throw new NotSupportedException("Backend IL: RESUME usato fuori dall'entry point con gestione errori attiva (bug interno).");

        if (stmt.Target is null)
        {
            _il.Emit(OpCodes.Ldloc, dispatch.ErrorLine);
            _il.Emit(OpCodes.Stloc, dispatch.State);
        }
        else if (stmt.Target.Kind == TokenKind.Next)
        {
            EmitResumeNextTarget(dispatch);
        }
        else
        {
            var targetLine = ResolveJumpTargetLine(stmt.Target)
                ?? throw new NotSupportedException($"Backend IL: target di RESUME '{stmt.Target.Lexeme}' non risolvibile a un numero di riga.");
            _il.Emit(OpCodes.Ldc_I4, targetLine);
            _il.Emit(OpCodes.Stloc, dispatch.State);
        }

        // NB: a differenza di OP_RESUME nella VM p-code (che azzera sia ErrorHandlerIp SIA
        // ErrorHandlerIsGosub, disinstallando l'handler), qui RESUME non tocca
        // dispatch.ErrorHandlerTarget/IsGosub — l'handler installato resta armato per un
        // errore SUCCESSIVO (comportamento "sticky" già esistente per ErrorHandlerTarget
        // prima di questo cambiamento, qui replicato per coerenza anche per IsGosub: se lo
        // STESSO handler ON ERROR GOSUB scatta di nuovo, il suo RETURN finale deve
        // continuare a comportarsi come RESUME NEXT, non come un END).
        _il.Emit(OpCodes.Leave, dispatch.LeaveTarget);
    }

    /// <summary>
    /// RESUME NEXT: imposta lo stato alla riga SUCCESSIVA a <see cref="_onErrorDispatch"/>
    /// .ErrorLine nell'ordine di esecuzione — un lookup a RUNTIME (confronto sequenziale
    /// contro ogni riga nota), dato che quale riga ha fallito non è noto a compile-time nel
    /// punto in cui questo RESUME NEXT si trova testualmente (potrebbe essere raggiunto
    /// dopo un errore originato in QUALSIASI riga che punta a questo handler).
    /// </summary>
    private void EmitResumeNextTarget((LocalBuilder State, LocalBuilder ErrorHandlerTarget, LocalBuilder ErrorLine, LocalBuilder IsGosub, Label LeaveTarget, ImmutableArray<int> ExecutionOrder) dispatch)
    {
        var doneLabel = _il.DefineLabel();
        var matchLabels = new Label[dispatch.ExecutionOrder.Length];
        for (var i = 0; i < dispatch.ExecutionOrder.Length; i++)
        {
            matchLabels[i] = _il.DefineLabel();
            _il.Emit(OpCodes.Ldloc, dispatch.ErrorLine);
            _il.Emit(OpCodes.Ldc_I4, dispatch.ExecutionOrder[i]);
            _il.Emit(OpCodes.Beq, matchLabels[i]);
        }
        // Nessuna riga nota combacia con ErrorLine — non dovrebbe accadere (ErrorLine è
        // sempre impostato a una riga valida prima di ogni chunk), ma per sicurezza lascia
        // lo stato invariato (rimarrebbe alla riga corrente, un rischio di loop infinito
        // preferibile a un comportamento indefinito) e prosegue.
        _il.Emit(OpCodes.Br, doneLabel);

        for (var i = 0; i < dispatch.ExecutionOrder.Length; i++)
        {
            _il.MarkLabel(matchLabels[i]);
            var nextLine = i + 1 < dispatch.ExecutionOrder.Length ? dispatch.ExecutionOrder[i + 1] : OnErrorDoneState;
            _il.Emit(OpCodes.Ldc_I4, nextLine);
            _il.Emit(OpCodes.Stloc, dispatch.State);
            _il.Emit(OpCodes.Br, doneLabel);
        }

        _il.MarkLabel(doneLabel);
    }

    /// <summary>
    /// Sintetizza il corpo di Main per il caso "SUB Main con parametri opzionali" (l'altro
    /// ramo valido secondo <see cref="Linker.HasEntryPoint"/>, alternativo al codice
    /// top-level a numero di riga di <see cref="EmitEntryPoint"/>). Il vero SUB Main
    /// utente è già stato compilato come una procedura NORMALE (<paramref
    /// name="mainMethodBuilder"/>, tra <see cref="_procedureMethods"/> — stesso loop di
    /// ogni altra SUB/FUNCTION top-level in CompileToAssembly): questo metodo si limita a
    /// CHIAMARLO, passando il valore di default di ogni parametro (TUTTI opzionali per
    /// costruzione — verificato da Linker.HasEntryPoint prima ancora di raggiungere questo
    /// backend) dato che non esiste alcun call site BASIC reale da cui derivare gli
    /// argomenti: usa <see cref="Parameter.DefaultValue"/> se presente, altrimenti il
    /// default del tipo dichiarato (stesso fallback usato dal Linker per un call site che
    /// omette un argomento opzionale senza DefaultValue esplicito — vedi
    /// Linker.Rewrites.Expressions.cs).
    /// </summary>
    /// <summary>
    /// Emette DIM/GLOBAL/CONST di modulo prima di chiamare SUB Main.
    /// Senza questo, un preambolo GLOBAL faceva scegliere EmitEntryPoint e Main non partiva.
    /// </summary>
    /// <summary>LIBRARY: nessun entry point (il prodotto e' una libreria). Emette il preambolo
    /// di modulo (DIM/CONST globali) e chiude l'entry con ret — nessuna esecuzione automatica.</summary>
    internal void EmitLibraryEntry(BasicModule mainModule)
    {
        EmitModulePreamble(mainModule);
        _il.Emit(OpCodes.Ret);
    }

    private void EmitModulePreamble(BasicModule mainModule)
    {
        var seen = new HashSet<int>();
        foreach (var lineNumber in mainModule.ExecutionOrder)
        {
            if (!seen.Add(lineNumber)) continue;
            foreach (var stmt in mainModule.Statements[lineNumber])
            {
                if (stmt is DeclarationStmt or ConstStmt)
                    EmitStatement(stmt);
            }
        }
    }

    private void EmitEntryPointCallingMain(FunctionDefinition mainProcDef, MethodBuilder mainMethodBuilder)
    {
        EmitProcedureRegistrations();
        foreach (var parameter in mainProcDef.Parameters)
        {
            if (parameter.Flags.HasFlag(SymbolFlags.IsByRef))
                throw new NotSupportedException("Backend IL: SUB Main con un parametro BYREF non supportato (nessun l-value da cui prendere l'indirizzo per l'entry point).");
            if (parameter.DefaultValue is not null)
                EmitExpression(parameter.DefaultValue);
            else
                EmitDefaultValue(parameter.DataType);
        }
        _il.Emit(OpCodes.Call, mainMethodBuilder);
        if (mainProcDef.ReturnType is not VoidType)
        {
            /* FUNCTION Main() AS INTEGER: il valore di ritorno e' l'EXIT CODE del processo
             * (strumenti BASIC/script). Esposto via IlRuntime.ProgramExitCode ai runner. */
            _il.Emit(OpCodes.Conv_R8);
            _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetProgramExitCode))!);
        }
        EmitFinalizeGlobalObjects();
        _il.Emit(OpCodes.Ret);
    }

    /// <summary>Finalizzazione DETERMINISTICA delle variabili GLOBALI di tipo CLASS/UDT con
    /// DESTRUCTOR a fine programma, in ORDINE INVERSO (parità PCode `EmitGlobalDestructorFinalizers`
    /// / VM/C). Legge il global dal bag/loader, chiama `__Destructor`, e per le CLASS
    /// `GC.SuppressFinalize` (anti-doppia-run). No-op se non ci sono finalizer.</summary>
    private void EmitFinalizeGlobalObjects()
    {
        if (_classFinalizers is not { Count: > 0 } && _udtFinalizers is not { Count: > 0 }) return;
        var suppress = typeof(GC).GetMethod(nameof(GC.SuppressFinalize), [typeof(object)])!;
        // Globali in ORDINE INVERSO di registrazione (LIFO, parità PCode/VM-C). Si filtra
        // con IsGlobalSymbol LIVE (l'indice GetGlobalSymbolIds può essere stale: il Linker
        // applica IsGlobal dopo la registrazione, vedi Linker.Binding).
        var syms = _program.Symbols.AllSymbols;
        for (int i = syms.Count - 1; i >= 0; i--)
        {
            var sym = syms[i];
            if (!IsGlobalSymbol(sym.Id)) continue;
            var dt = NormalizeInferredType(sym.DataType);
            if (dt is ClassType ct && _classFinalizers is { } cfs && cfs.TryGetValue(ct.Definition.SymbolId, out var fin))
            {
                // Reference type: carica dal bag (EmitLoadVariable → LoadGlobal + Castclass),
                // null-check, call __Destructor, GC.SuppressFinalize (anti-doppia-run).
                var tmp = _il.DeclareLocal(DataTypeMapper.Map(dt, _types, _classTypes));
                EmitLoadVariable(sym.Id);
                _il.Emit(OpCodes.Stloc, tmp);
                var skip = _il.DefineLabel();
                _il.Emit(OpCodes.Ldloc, tmp);
                _il.Emit(OpCodes.Brfalse, skip);
                _il.Emit(OpCodes.Ldloc, tmp);
                _il.Emit(OpCodes.Call, fin);
                _il.Emit(OpCodes.Ldloc, tmp);
                _il.Emit(OpCodes.Call, suppress);
                _il.MarkLabel(skip);
            }
            else if (dt is UserDefinedType udt && _udtFinalizers is { } ufs && ufs.TryGetValue(udt.SymbolId, out var ufin))
            {
                // Value type: boxed nel bag → indirizzo dentro il box (EmitGlobalUdtAddress)
                // e call al metodo d'istanza `__Destructor` (this = managed pointer).
                EmitGlobalUdtAddress(sym.Id, udt);
                _il.Emit(OpCodes.Call, ufin);
            }
        }
    }

    /// <summary>Finalizza la VECCHIA istanza di un GLOBALE CLASS con DESTRUCTOR prima della
    /// riassegnazione (parità PCode/VM-C/C99: `g = NEW X()` invoca il distruttore del vecchio g).
    /// No-op se il simbolo non è una CLASS con finalizer. Null-check per la prima assegnazione.</summary>
    private void EmitFinalizeReassignedGlobalInstance(int symbolId, Type clr)
    {
        if (_classFinalizers is not { Count: > 0 } cfs) return;
        if (NormalizeInferredType(_program.Symbols.GetSymbol(symbolId).DataType) is not ClassType ct) return;
        if (!cfs.TryGetValue(ct.Definition.SymbolId, out var fin)) return;
        var suppress = typeof(GC).GetMethod(nameof(GC.SuppressFinalize), [typeof(object)])!;
        _il.Emit(OpCodes.Ldc_I4, symbolId);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LoadGlobal), typeof(int)));
        if (clr != typeof(object)) _il.Emit(OpCodes.Castclass, clr);
        var oldLocal = _il.DeclareLocal(clr);
        _il.Emit(OpCodes.Stloc, oldLocal);
        var skip = _il.DefineLabel();
        _il.Emit(OpCodes.Ldloc, oldLocal);
        _il.Emit(OpCodes.Brfalse, skip);
        _il.Emit(OpCodes.Ldloc, oldLocal);
        _il.Emit(OpCodes.Call, fin);
        _il.Emit(OpCodes.Ldloc, oldLocal);
        _il.Emit(OpCodes.Call, suppress);
        _il.MarkLabel(skip);
    }

    /// <summary>Chiama il SUB Main "trailing" (dopo il corpo top-level) se impostato da
    /// Host.Compile.cs. Stessa convenzione di EmitEntryPointCallingMain (argomenti opzionali
    /// con default). No-op se non c'è un Main trailing.</summary>
    private void EmitTrailingMainCall()
    {
        if (_trailingMainCall is not var (def, method) || def is null || method is null) return;
        foreach (var parameter in def.Parameters)
        {
            if (parameter.Flags.HasFlag(SymbolFlags.IsByRef))
                throw new NotSupportedException("Backend IL: SUB Main con un parametro BYREF non supportato (entry top-level + Main).");
            if (parameter.DefaultValue is not null)
                EmitExpression(parameter.DefaultValue);
            else
                EmitDefaultValue(parameter.DataType);
        }
        _il.Emit(OpCodes.Call, method);
        if (def.ReturnType is not VoidType) _il.Emit(OpCodes.Pop);
    }

    /// <summary>
    /// Alloca (senza fissare) un'etichetta IL per ogni LabelStmt. Non ricorre
    /// nei corpi di IfStmt/ecc.: i salti con nome (JumpStmt/ConditionalJumpStmt)
    /// generati dal Parser per l'appiattimento di loop restano tutti a livello
    /// top-level per costruzione, quindi non serve scendere nei blocchi
    /// strutturati che questo backend abbassa da sé in modo ricorsivo (IF/SELECT/ecc.).
    /// <paramref name="ownerLine"/> (solo per l'entry point con ON ERROR attivo — vedi
    /// _namedLabelToLine) registra a quale numero di riga appartiene l'etichetta.
    /// </summary>
    private void PreScanLabels(Stmt stmt, int? ownerLine = null)
    {
        if (stmt is LabelStmt labelStmt)
        {
            _namedLabels[labelStmt.Label.Lexeme] = _il.DefineLabel();
            if (ownerLine is int line) _namedLabelToLine[labelStmt.Label.Lexeme] = line;
        }
        // I blocchi con un Body annidato (USING/IF/SELECT/TRY/WITH/loop) possono contenere
        // LabelStmt SINTETICI generati dal parser (es. ___DO_START_N del DO WHILE dentro
        // un USING — UsingBlock): senza la pre-scansione ricorsiva il label non viene
        // registrato e i jump (ConditionalJumpStmt) falliscono con 'etichetta non trovata'.
        switch (stmt)
        {
            case UsingStmt us:
                foreach (var s in us.Body) PreScanLabels(s, ownerLine);
                break;
            case IfStmt ifs:
                foreach (var clause in ifs.IfClauses)
                    foreach (var s in clause.Statements) PreScanLabels(s, ownerLine);
                if (ifs.ElseClause is not null)
                    foreach (var s in ifs.ElseClause.Statements) PreScanLabels(s, ownerLine);
                break;
            case WithStmt ws:
                foreach (var s in ws.Body) PreScanLabels(s, ownerLine);
                break;
            case TryCatchStmt tc:
                foreach (var s in tc.TryBody) PreScanLabels(s, ownerLine);
                foreach (var c in tc.CatchClauses)
                    foreach (var s in c.Body) PreScanLabels(s, ownerLine);
                if (tc.Finally is not null)
                    foreach (var s in tc.Finally.Body) PreScanLabels(s, ownerLine);
                break;
        }
    }

    /// <summary>
    /// Rileva i symbol dichiarati con tipi DIVERSI da DIM multiple nello stesso corpo
    /// (es. `DIM S AS SINGLE` poi `DIM S$ AS STRING` — stesso slot, tipo ridefinito).
    /// Questi symbol avranno un local IL di tipo object (boxed): GetOrDeclareLocal deve
    /// dichiararli object, le scritture boxano e le letture fanno unbox al tipo dell'uso.
    /// </summary>
    private void PreScanRedefinedTypes(ImmutableArray<Stmt> body)
    {
        var declaredTypes = new Dictionary<int, Type>();
        void ScanStmt(Stmt s)
        {
            if (s is DeclarationStmt declStmt)
                foreach (var d in declStmt.Declarations)
                {
                    // SafeCast (AS?): il risultato è NullableType — può essere NIL a runtime,
                    // serve un local object boxed (bad IS NIL deve funzionare).
                    if (d.InitialValue is SafeCastExpr) _redefinedTypeSymbols.Add(d.SymbolId);
                    if (d.DataType is null) continue;
                    var clrType = DataTypeMapper.Map(d.DataType, _types, _classTypes);
                    if (declaredTypes.TryGetValue(d.SymbolId, out var prev) && prev != clrType)
                        _redefinedTypeSymbols.Add(d.SymbolId);
                    else if (!declaredTypes.ContainsKey(d.SymbolId))
                        declaredTypes[d.SymbolId] = clrType;
                }
        }
        foreach (var stmt in body) ScanStmt(stmt);
    }

    /// <summary>
    /// Compila il corpo di una SUB/FUNCTION utente in un vero metodo CIL statico
    /// (MethodBuilder già dichiarato in CompileToAssembly). A differenza di Main,
    /// <see cref="ProcedureDefinition.Body"/> è una lista piatta di Stmt, non un
    /// dizionario per numero di riga con ExecutionOrder — le SUB/FUNCTION moderne
    /// non supportano GOTO per numero di riga (solo etichette con nome per
    /// l'appiattimento IF/loop), quindi non serve popolare _lineLabels qui, solo
    /// _namedLabels via PreScanLabels.
    /// </summary>
    private void EmitProcedureBody(FunctionDefinition definition)
    {
        // Traccia il file .bas corrente per PROCEDUREFOLDER() import-aware: ogni procedura
        // marca il proprio modulo di appartenenza prima di eseguire il corpo.
        var sourcePath = _program.FileIdToPathMap.GetValueOrDefault(definition.Position.FileId, string.Empty);
        if (!string.IsNullOrEmpty(sourcePath))
        {
            _il.Emit(OpCodes.Ldstr, sourcePath);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SetCurrentSourceFile), typeof(string)));
        }
        // Scope dinamico: apre un frame se la procedura dichiara DYNAMIC (no-op altrimenti).
        EmitDynamicScopeEnter(definition.Body);
        EmitStatementBody(definition.Body);
    }

    /// <summary>
    /// Compila una lista piatta di statement come corpo di un metodo CIL (procedura,
    /// metodo di classe, accessor get/set di una PROPERTY — tutto ciò che non è Main né
    /// un costruttore, che hanno le loro varianti dedicate). Il `ret` finale è difensivo:
    /// il corpo dovrebbe sempre terminare con EndProcedureStmt, che emette già `ret` —
    /// vedi EmitEndProcedureStatement — ma un secondo `ret` irraggiungibile è IL valido.
    /// Se il tipo di ritorno corrente non è void, spinge comunque un valore di default
    /// PRIMA del `ret` difensivo (stessa logica di EmitEndProcedureStatement): necessario
    /// per un GETTER di PROPERTY, il cui corpo potrebbe non contenere mai un
    /// EndProcedureStmt esplicito (sintassi END PROPERTY/END GET, non END FUNCTION) — un
    /// `ret` nudo su un metodo non-void sarebbe IL non valido.
    /// </summary>
}
