// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Program.cs
// PARTIAL — dominio PROGRAM: EmitEntryPoint, EmitStatementBody,
// EmitProcedureBodyWithErrorHandling, AppendModuleErrorHandlerChunks.
// Migrato da IlProgramCompiler.cs (2026-08-30 — split 6.4.2).

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

/// <summary>Partial del transpiler IL: entry-point e corpi procedure.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitEntryPoint(BasicModule mainModule)
    {
        _localByName.Clear();
        // Riga BASIC corrente per ERR.LineNumber / posizione delle eccezioni (vedi EmitStatement).
        _currentLineLocal = _il.DeclareLocal(_types.Int32);
        EmitInitializeDataPool(mainModule);
        EmitProcedureRegistrations();
        // Pre-scan dei tipi ridichiarati (DIM S AS SINGLE poi DIM S$ AS STRING) anche per il
        // Main top-level — EmitStatementBody (procedure) lo fa, EmitEntryPoint no.
        var topLevelStmts = mainModule.ExecutionOrder
            .SelectMany(ln => mainModule.Statements[ln]).ToImmutableArray();
        PreScanRedefinedTypes(topLevelStmts);
        if (UsesOnErrorHandling(mainModule))
        {
            EmitEntryPointWithErrorHandling(mainModule);
            return;
        }

        InitializeGosubRuntime(topLevelStmts);

        foreach (var lineNumber in mainModule.ExecutionOrder)
        {
            _lineLabels[lineNumber] = _il.DefineLabel();
            foreach (var stmt in mainModule.Statements[lineNumber])
                // ownerLine = il numero di riga (chiave di ExecutionOrder, es. esplicita 130 o
                // sintetica 1_000_000+N): serve a _namedLabelToLine per GOTO/ON ERROR/RESTORE
                // con target a etichetta (prima popolato solo nel path ON ERROR).
                PreScanLabels(stmt, lineNumber);
        }

        // TRY trasparente attorno al corpo del Main: il verifier del JIT di .NET 10
        // fallisce la SUB con ON ERROR (InvalidProgram — "Unknown result type" per i
        // TypeRef BasicException dei suoi catch) quando il Main TOP-LEVEL lineare la
        // chiama (es. HttpFetchAdvanced: GO SUB ClientTest). Il TRY/CATCH nativo del
        // source risolve il problema (la CALL dentro un try); i catch qui NON fanno
        // rethrow (swallow) perché il rethrow nel try trasparente riproduce il verifier
        // failure — la semantica resta: il Main non gestisce errori e le SUB gestiscono
        // i propri; un'eccezione non gestita viene inghiottita invece di crashare.
        var needsOnErrorProcedures = _procedureDefinitions.Values.Any(d => d.Body.Any(StatementUsesOnErrorHandling));
        var mainTryEndLabel = default(System.Reflection.Emit.Label);
        var mainLabels = new HashSet<string>();
        var mainLines = new HashSet<int>();
        if (needsOnErrorProcedures)
        {
            mainTryEndLabel = _il.BeginExceptionBlock();
            foreach (var s in topLevelStmts)
            {
                if (s is LabelStmt ls) mainLabels.Add(ls.Label.Lexeme);
                mainLines.Add(s.Position.Line);
            }
            _activeTryScopes.Push((mainLabels, mainLines));
        }
        _missingLineExitLabel = _il.DefineLabel();

        // ExecutionOrder può contenere DUPLICATI (prodotto da FlattenStatementsByLine che
        // fonde/inserisce righe sintetiche): marcare lo stesso label due volte è un
        // InvalidProgramException ("Label defined multiple times", GraphicsFullGfxTest) e
        // ri-emettere la riga duplicherebbe l'esecuzione — skip della seconda occorrenza.
        var seenEntryLines = new HashSet<int>();
        foreach (var lineNumber in mainModule.ExecutionOrder)
        {
            if (!seenEntryLines.Add(lineNumber)) continue;
            _il.MarkLabel(_lineLabels[lineNumber]);
            foreach (var stmt in mainModule.Statements[lineNumber])
                EmitStatement(stmt);
        }

        if (needsOnErrorProcedures)
        {
            _activeTryScopes.Pop();
            var tCaughtLocal = _il.DeclareLocal(_types.BasicExceptionType);
            _il.Emit(OpCodes.Leave, mainTryEndLabel);
            _il.BeginCatchBlock(_types.BasicExceptionType);
            _il.Emit(OpCodes.Stloc, tCaughtLocal);
            _il.Emit(OpCodes.Leave, mainTryEndLabel);
            _il.BeginCatchBlock(_types.ExceptionType);
            _il.Emit(OpCodes.Pop);
            _il.Emit(OpCodes.Leave, mainTryEndLabel);
            _il.EndExceptionBlock();
        }

        // Entry top-level + SUB Main: esegue Main DOPO il corpo top-level (se impostato).
        EmitTrailingMainCall();

        // Finalizzazione deterministica dei globali CLASS/UDT con DESTRUCTOR (parità PCode/VMC).
        EmitFinalizeGlobalObjects();

        if (_missingLineExitLabel is { } missingExitLabel) _il.MarkLabel(missingExitLabel);
        _il.Emit(OpCodes.Ret);
        _gosubReturnStackLocal = null;
        _gosubSites.Clear();
        _gosubReturnLabels = [];
        _missingLineExitLabel = null;
    }

    private void EmitStatementBody(ImmutableArray<Stmt> body)
    {
        _localByName.Clear();
        // Riga BASIC corrente per ERR.LineNumber / posizione delle eccezioni (vedi EmitStatement).
        _currentLineLocal = _il.DeclareLocal(_types.Int32);
        // GOSUB dentro una PROCEDURA (SUB/FUNCTION): registra i siti di continuazione
        // runtime (lo stesso pattern dell'entry point) — necessario anche per i corpi
        // v2 (dead code) che contengono GOSUB verso target con FINALLY sospendibile.
        InitializeGosubRuntime(body);
        // ON ERROR dentro una PROCEDURA (SUB/FUNCTION — es. HttpFetch SUB Main): il pattern
        // a chunk genera IL che il verifier/JIT rifiuta ANCHE con pochi chunk ('Expected O,
        // but got Unknown' per i locali BasicException — diagnosticato 2026-08-14, anche con
        // 3 chunk). SKIP esplicito (il runner lo marca) — MAI IL rotto che diventerebbe FAIL.
        // NOTA: le procedure TOP-LEVEL con ON ERROR passano da EmitProcedureBodyWithErrorHandling
        // (collegato al loop delle procedure — il dispatch a chunk ora funziona); questo check
        // intercetta SOLO i corpi non-top-level (metodi di classe/altri) con ON ERROR.
        if (body.Any(StatementUsesOnErrorHandling))
            throw new NotSupportedException(
                "Backend IL: ON ERROR in un metodo non-top-level (classe) non ancora supportato — il dispatch a chunk è collegato solo a SUB/FUNCTION top-level.");
        foreach (var stmt in body) PreScanLabels(stmt);
        PreScanRedefinedTypes(body);
        _missingLineExitLabel = _il.DefineLabel();
        foreach (var stmt in body) EmitStatement(stmt);
        // Il ret difensivo serve SOLO se il corpo non termina con un EndProcedureStmt
        // esplicito (che emette già il proprio ret — vedi EmitEndProcedureStatement):
        // un DOPPIO ret finale (il ret dell'END SUB + il ret difensivo) rendeva ambiguo
        // per il verifier il target dei brtrue dell'ultimo PRINT ("found ref 'string'"
        // sul primo dei due ret — AgentTeam InvalidProgram).
        var endsWithEndProcedure = body.Length > 0 && body[^1] is EndProcedureStmt;
        if (!endsWithEndProcedure)
        {
            if (_missingLineExitLabel is { } missingExitLabel) _il.MarkLabel(missingExitLabel);
            EmitDefaultReturnThenDefersThenRet();
        }
        else if (_missingLineExitLabel is { } missingExitLabel2)
        {
            _il.MarkLabel(missingExitLabel2);
            EmitDefaultReturnThenDefersThenRet();
        }
        _missingLineExitLabel = null;

        // Punto di throw esterno alle regioni protette per gli ERROR statement emessi con
        // Leave (EmitErrorStatement dentro un TRY). Marcato SOLO se qualche ERROR li ha usati.
        if (_errorThrowExitPending)
        {
            _il.MarkLabel(_errorThrowExitLabel!.Value);
            _il.Emit(OpCodes.Ldloc, _errorThrowTemp!);
            _il.Emit(OpCodes.Throw);
            _errorThrowExitPending = false;
        }
        _errorThrowExitLabel = null;
        _errorThrowTemp = null;

        // Punto di RETURN esterno alle regioni protette per i RETURN dentro TRY/FINALLY, E
        // per i RETURN con DEFER pendenti fuori da qualunque TRY (EmitReturnStatement — la
        // condizione che porta qui è stata allargata per il caso DEFER). Il valore è già
        // nel local tipizzato (stack pulito): i DEFER, se presenti, eseguono LIFO PRIMA del
        // ret reale (parità Go/PCode/VM-C/C99Native).
        if (_returnExitPending)
        {
            _il.MarkLabel(_returnExitLabel!.Value);
            EmitDeferredBodiesIfAny();
            if (_returnExitValue is not null) _il.Emit(OpCodes.Ldloc, _returnExitValue);
            EmitDynamicScopeExit();
            _il.Emit(OpCodes.Ret);
            _returnExitPending = false;
        }
        _returnExitLabel = null;
        _returnExitValue = null;
    }

    /// <summary>
    /// ON ERROR in una PROCEDURA (corpo piatto, etichette con nome — mai numeri di riga):
    /// stesso dispatch a chunk dell'entry point (EmitEntryPointWithErrorHandling), ma i
    /// chunk vengono costruiti dal corpo (SplitAtLabelBoundaries) e le ETICHETTE vengono
    /// mappate al chunk.State (non a un numero di riga). Porting del modello P-Code
    /// (OP_SET_ERR_HANDLER_GOTO: salto diretto all'handler impostato).
    /// </summary>
    private void EmitProcedureBodyWithErrorHandling(ImmutableArray<Stmt> body)
    {
        _localByName.Clear();
        // Scope dinamico: apre un frame se la procedura dichiara DYNAMIC (no-op altrimenti).
        EmitDynamicScopeEnter(body);
        var stateLocal = _il.DeclareLocal(_types.Int32);
        var errorHandlerTargetLocal = _il.DeclareLocal(_types.Int32);
        var errorLineLocal = _il.DeclareLocal(_types.Int32);
        var isGosubLocal = _il.DeclareLocal(_types.Int32);
        // Riga BASIC corrente per ERR.LineNumber: in questo percorso i chunk hanno pseudo-riga 0
        // (stati sintetici), quindi la riga va tracciata per statement da EmitStatement.
        _currentLineLocal = _il.DeclareLocal(_types.Int32);
        var dispatchLoopStart = _il.DefineLabel();
        var doneLabel = _il.DefineLabel();
        // Uscita dei `RETURN valore` emessi dentro i try dei chunk (EmitReturnStatement):
        // il valore viaggia in un local tipizzato, il ret vero è emesso a fine metodo.
        _onErrorReturnExitLabel = _il.DefineLabel();
        _onErrorReturnValue = _currentReturnType is not null and not VoidType
            ? _il.DeclareLocal(DataTypeMapper.Map(_currentReturnType, _types, _classTypes))
            : null;

        // Chunk dal corpo piatto: pseudo-riga 0 + split alle etichette (ogni etichetta
        // non-prima apre un chunk con state sintetico NEGATIVO). Le etichette con nome
        // vengono mappate al chunk.State per GOTO/ON ERROR (ResolveJumpTargetLine).
        var chunks = BuildDispatchChunks([0], _ => body.ToList());
        var executionOrderStates = chunks.Select(c => c.State).ToImmutableArray();
        foreach (var chunk in chunks)
            foreach (var stmt in chunk.Statements)
                if (stmt is LabelStmt lbl)
                    _namedLabelToLine[lbl.Label.Lexeme] = chunk.State;
        // ON ERROR GOTO/GOSUB <label> verso un handler a livello MODULO (pattern comune:
        // `GosubHandler:` tra le SUB): la label NON è nel body della procedura — pre-scan
        // anche delle label top-level del modulo (stessa mappa dell'entry point). Le righe
        // top-level del modulo sono SINTETICHE (1_000_000+N): il target va mappato allo
        // stato del chunk (riga sintetica), non alla riga sorgente reale — il dispatch flat
        // salta SOLO a chunk presenti nel proprio dispatch.
        if (_program?.MainModule is { } mainMod)
            foreach (var (syntheticLine, lineStatements) in mainMod.Statements)
                foreach (var stmt in lineStatements)
                    if (stmt is LabelStmt moduleLbl)
                        _namedLabelToLine.TryAdd(moduleLbl.Label.Lexeme, syntheticLine);

        // Handler ON ERROR a livello MODULO referenziati da questa procedura: i loro
        // statement sono inlinali come chunk aggiuntivi nel dispatch della proc (stato =
        // riga reale del label, raggiungibile da _namedLabelToLine). Così anche una proc
        // FLAT (senza stepper v2) recupera da un errore verso un handler del modulo —
        // parità PCodeVM (lì l'handler è un chunk globale raggiungibile da qualsiasi proc).
        if (_program?.MainModule is { } mainModuleRef)
            AppendModuleErrorHandlerChunks(body, chunks, mainModuleRef);

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

            foreach (var stmt in chunk.Statements)
                EmitStatement(stmt);
            if (chunk.Statements.Count == 0 || !IsChunkTerminatingStatement(chunk.Statements[^1]))
            {
                _il.Emit(OpCodes.Ldc_I4, nextState);
                _il.Emit(OpCodes.Stloc, stateLocal);
                _il.Emit(OpCodes.Leave, chunkEndLabel);
            }

            var onErrorCaughtLocal = _il.DeclareLocal(_types.BasicExceptionType);

            void EmitOnErrorDispatch()
            {
                EmitCaptureErrorIntoGlobalErr(onErrorCaughtLocal);
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

                // ON ERROR GOTO/GOSUB NomeProcedura: target sentinella negativo sotto la
                // base → invoca la procedura corrispondente (dopo aver aggiornato lo stato
                // alla riga successiva, così RETURN nudo o la fine dell'handler riprendono
                // il flusso — stesso effetto del salto a una riga del gestore).
                _il.Emit(OpCodes.Ldloc, errorHandlerTargetLocal);
                _il.Emit(OpCodes.Ldc_I4, OnErrorProcedureTargetBase);
                _il.Emit(OpCodes.Ble, procedureTargetLabel);

                _il.Emit(OpCodes.Ldloc, errorHandlerTargetLocal);
                _il.Emit(OpCodes.Stloc, stateLocal);
                _il.Emit(OpCodes.Leave, chunkEndLabel);

                _il.MarkLabel(procedureTargetLabel);
                // Dispatch compile-time: la sentinella runtime (installata da
                // EmitOnErrorStatement) identifica QUALE procedura invocare. Catena di
                // confronti verso il metodo registrato; poi si riprende da nextState.
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
                _il.Emit(OpCodes.Ldc_I4, nextState);
                _il.Emit(OpCodes.Stloc, stateLocal);
                _il.Emit(OpCodes.Leave, chunkEndLabel);

                _il.MarkLabel(noHandlerLabel);
                _il.Emit(OpCodes.Rethrow);
            }

            _il.BeginCatchBlock(_types.BasicExceptionType);
            _il.Emit(OpCodes.Stloc, onErrorCaughtLocal);
            EmitOnErrorDispatch();

            _il.BeginCatchBlock(_types.ExceptionType);
            var clrCaughtLocal = _il.DeclareLocal(_types.ExceptionType);
            _il.Emit(OpCodes.Stloc, clrCaughtLocal);
            // Una BasicException già tipizzata va usata AS-IS (FromClrException la rimapperebbe a
            // InternalError 200, perdendo il codice vero: `ERR.Number` = 11). Stessa guardia di
            // EmitTryCatchStatement / del percorso dell'entry point.
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
        if (_currentReturnType is not null and not VoidType)
        {
            // END FUNCTION / EXIT FUNCTION dentro il dispatch: il vero `ret` è qui (fuori da
            // ogni try), quindi il valore va letto ORA dal return-slot (convenzione BASIC
            // `NomeFunzione = …`). Prima si emetteva EmitDefaultValue: `ReadText = s :
            // END FUNCTION` restituiva "" — bug reale (lettura di ogni file vuota).
            if (_currentReturnSymbolId >= 0) EmitLoadVariable(_currentReturnSymbolId);
            else EmitDefaultValue(_currentReturnType);
        }
        EmitDynamicScopeExit();
        _il.Emit(OpCodes.Ret);

        // Punto di uscita di un `RETURN valore` emesso dentro un try del dispatch (fuori da
        // ogni regione protetta): il valore è già nel local tipizzato.
        if (_onErrorReturnExitLabel is { } procedureReturnExit)
        {
            _il.MarkLabel(procedureReturnExit);
            if (_onErrorReturnValue is { } procedureReturnValue) _il.Emit(OpCodes.Ldloc, procedureReturnValue);
            EmitDynamicScopeExit();
            _il.Emit(OpCodes.Ret);
        }
        _onErrorReturnExitLabel = null;
        _onErrorReturnValue = null;
    }

    /// <summary>Inlina gli handler ON ERROR a livello MODULO referenziati dal body della
    /// procedura flat corrente come chunk aggiuntivi del suo dispatch. Il dispatch a chunk
    /// è LOCALE alla proc (stati sintetici/righe del proprio body): un target ON ERROR verso
    /// una label del modulo (mappata alla sua riga reale in _namedLabelToLine) non trova mai
    /// il chunk → l'errore veniva inghiottito in silenzio. Qui gli statement della regione
    /// handler del modulo (dalla label fino alla fine / al label successivo) diventano chunk
    /// con stato = riga reale del label (primo statement) + stati sintetici unici al di sotto
    /// del minimo della proc (niente collisioni). Il RESUME NEXT dell'handler riprende alla
    /// riga successiva all'errore NELLA proc (lookup runtime su executionOrderStates).</summary>
    private void AppendModuleErrorHandlerChunks(
        ImmutableArray<Stmt> body, List<DispatchChunk> chunks, BasicModule mainMod)
    {
        var targets = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var stmt in body)
            if (stmt is OnErrorStmt oe && oe.Target is { Kind: TokenKind.Identifier } t)
                targets.Add(t.Lexeme);
        if (targets.Count == 0) return;

        var existingStates = new HashSet<int>(chunks.Select(c => c.State));
        var nextSynthetic = chunks.Min(c => c.State) - 1;
        var moduleLines = mainMod.ExecutionOrder;
        List<Stmt> StatementsFor(int ln) =>
            mainMod.Statements.TryGetValue(ln, out var s) ? s : [];

        foreach (var labelName in targets)
        {
            if (!_namedLabelToLine.TryGetValue(labelName, out _)) continue;
            // Le righe TOP-LEVEL del modulo sono SINTETICHE (1_000_000+N): il valore in
            // _namedLabelToLine è la riga sorgente REALE del label → non confrontabile con
            // moduleLines. Trova la riga sintetica che contiene il LabelStmt con questo nome.
            var labelIdx = -1;
            for (var i = 0; i < moduleLines.Length; i++)
            {
                if (!mainMod.Statements.TryGetValue(moduleLines[i], out var lineStatements)) continue;
                if (lineStatements.Any(s => s is LabelStmt ls && string.Equals(ls.Label.Lexeme, labelName, StringComparison.OrdinalIgnoreCase)))
                { labelIdx = i; break; }
            }
            if (labelIdx < 0) continue;
            var labelLine = moduleLines[labelIdx];
            if (existingStates.Contains(labelLine)) continue;

            var regionLines = new List<int>();
            for (var i = labelIdx; i < moduleLines.Length; i++)
            {
                var ln = moduleLines[i];
                if (i > labelIdx && StatementsFor(ln).Any(s => s is LabelStmt)) break;
                regionLines.Add(ln);
            }
            foreach (var ln in regionLines)
            {
                var segments = SplitAtLabelBoundaries(StatementsFor(ln));
                for (var s = 0; s < segments.Count; s++)
                {
                    var state = s == 0 && ln == labelLine ? labelLine : nextSynthetic--;
                    chunks.Add(new DispatchChunk(state, ln, segments[s]));
                    existingStates.Add(state);
                }
            }
        }
}
}
