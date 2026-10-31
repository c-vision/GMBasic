// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Statements.Concurrency.cs
// PARTIAL — dominio STATEMENTS/CONCURRENCY: sleep/send/receive/mutex/defer/once/cond/
// channel/go-select/wait/done. Migrato da IlProgramCompiler.Statements.cs
// (2026-08-30 — split, file <= 1200 righe).

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

/// <summary>Partial del transpiler IL: statement concorrenza/GPM.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitSleepStatement(SleepStmt stmt)
    {
        EmitInt32Arg(stmt.Duration);
        _il.Emit(OpCodes.Conv_R8); // Int32 → Double per IslandRuntime.Sleep(double)
        _il.Emit(OpCodes.Call, IslandRuntimeSleepMethod);
    }

    /// <summary>Risolve il <see cref="ChannelType"/> (con Direzione) di un'espressione canale, per
    /// il check direzionale G8-bis (parità Go `chan<- T` / `<-chan T`). Solo per
    /// variabili/parametri/globali risolvibili staticamente; altrimenti null (nessun check).</summary>
    private ChannelType? ResolveChannelDirectionType(Expr channelExpr)
    {
        if (channelExpr is VariableExpr ve)
            return NormalizeInferredType(_program.Symbols.GetSymbol(ve.SymbolId).DataType) as ChannelType;
        return null;
    }

    private void EmitSendStatement(SendStmt stmt)
    {
        // Parità Go chan direzionali (G8-bis): SEND su un canale ReceiveOnly ('<-chan T') è un
        // errore di compilazione — come il PCode Compiler (Compiler.GoRoutines.cs).
        if (ResolveChannelDirectionType(stmt.Channel) is { Direction: ChannelDirection.ReceiveOnly } chRecvDir)
            throw new NotSupportedException(
                $"Backend IL: SEND su un canale ReceiveOnly ('<-chan T'): non si può inviare su '{chRecvDir}'.");
        EmitExpression(stmt.Channel); // ChannelInfo sullo stack
        EmitExpression(stmt.Value);   // value sullo stack (value type → box)
        var valueType = InferType(stmt.Value);
        var valueClrType = DataTypeMapper.Map(NormalizeInferredType(valueType), _types, _classTypes);
        if (valueClrType.IsValueType)
            _il.Emit(OpCodes.Box, valueClrType);
        _il.Emit(OpCodes.Call, IslandRuntimeSendInlineMethod);
    }

    private void EmitReceiveStatement(ReceiveStmt stmt)
    {
        // Parità Go chan direzionali (G8-bis): RECEIVE su un canale SendOnly ('chan<- T') è un
        // errore di compilazione — come il PCode Compiler (Compiler.GoRoutines.cs).
        if (ResolveChannelDirectionType(stmt.Channel) is { Direction: ChannelDirection.SendOnly } chSendDir)
            throw new NotSupportedException(
                $"Backend IL: RECEIVE su un canale SendOnly ('chan<- T'): non si può ricevere da '{chSendDir}'.");
        EmitExpression(stmt.Channel);
        _il.Emit(OpCodes.Call, IslandRuntimeReceiveInlineMethod);
        // IslandRuntime.Receive restituisce object? — serve unbox/cast al tipo target
        if (stmt.Destination is VariableTarget vt)
        {
            var targetDataType = NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType);
            var clrType = DataTypeMapper.Map(targetDataType, _types, _classTypes);
            if (IlConversion.IsNumeric(targetDataType))
            {
                // Il valore ricevuto è boxato come object (Double da un letterale SEND 42):
                // Unbox_Any(Int16) lancerebbe InvalidCastException (Double ≠ Int16). Uso
                // IlRuntime.ToDouble(object) + conv — stesso pattern di EmitCoerceToTarget.
                _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, clrType);
            }
            else if (clrType != typeof(object) && clrType.IsValueType)
            {
                _il.Emit(OpCodes.Unbox_Any, clrType);
            }
            else if (clrType != typeof(object))
            {
                _il.Emit(OpCodes.Castclass, clrType);
            }
            // Store: value already on stack
            _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        }
        else
        {
            _il.Emit(OpCodes.Pop);
        }
    }

    private void EmitMutexLockStatement(MutexLockStmt stmt)
    {
        // LOCK mu (parità Go mu.Lock()): nel percorso lineare (senza scheduler) acquisisce
        // senza attesa (un lock conteso in lineare è un errore: non può parcheggiare).
        // RWMUTEX: `MUTEX LOCK rw` con rw RWMUTEX usa PCodeRwMutex.TryLock (scrittore).
        var lockClr = NormalizeInferredType(InferType(stmt.Mutex)) is RwMutexType ? typeof(PCodeRwMutex) : typeof(PCodeMutex);
        EmitExpression(stmt.Mutex);
        _il.Emit(OpCodes.Castclass, lockClr);
        _il.Emit(OpCodes.Ldnull); // task: il path lineare non ha scheduler
        _il.Emit(OpCodes.Callvirt, lockClr == typeof(PCodeRwMutex) ? PCodeRwMutexTryLockMethod : PCodeMutexTryLockMethod);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitMutexUnlockStatement(MutexUnlockStmt stmt)
    {
        // UNLOCK mu (parità Go mu.Unlock()): rilascia e sveglia al più un waiter. RWMUTEX:
        // `MUTEX UNLOCK rw` con rw RWMUTEX usa PCodeRwMutex.Unlock (scrittore).
        var unlockClr = NormalizeInferredType(InferType(stmt.Mutex)) is RwMutexType ? typeof(PCodeRwMutex) : typeof(PCodeMutex);
        EmitExpression(stmt.Mutex);
        _il.Emit(OpCodes.Castclass, unlockClr);
        _il.Emit(OpCodes.Ldnull); // scheduler
        _il.Emit(OpCodes.Callvirt, unlockClr == typeof(PCodeRwMutex) ? PCodeRwMutexUnlockMethod : PCodeMutexUnlockMethod);
    }

    private void EmitMutexRLockStatement(MutexRLockStmt stmt)
    {
        // RWMUTEX RLOCK (parità Go rw.RLock()): lettura condivisa — nel percorso lineare
        // (senza scheduler) acquisisce senza attesa (un lock conteso in lineare non può
        // parcheggiare).
        EmitExpression(stmt.Mutex);
        _il.Emit(OpCodes.Castclass, typeof(PCodeRwMutex));
        _il.Emit(OpCodes.Ldnull); // task: il path lineare non ha scheduler
        _il.Emit(OpCodes.Callvirt, PCodeRwMutexTryRLockMethod);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitMutexRUnlockStatement(MutexRUnlockStmt stmt)
    {
        // RWMUTEX RUNLOCK (parità Go rw.RUnlock()): rilascia un read lock.
        EmitExpression(stmt.Mutex);
        _il.Emit(OpCodes.Castclass, typeof(PCodeRwMutex));
        _il.Emit(OpCodes.Ldnull); // scheduler
        _il.Emit(OpCodes.Callvirt, PCodeRwMutexRUnlockMethod);
    }

    private void EmitDeferStatement(DeferStmt stmt)
    {
        // DEFER nel path LINEARE (Proc_<id>, per procedure NON may-park o comunque chiamate
        // dirette non-goroutine — il driver GPM v2 continua a gestire i propri DEFER via
        // stepper, EmitDeferPush/DeferDone nel lowering, percorso separato e non toccato
        // qui). Parità Go: registra il body, eseguito LIFO a ogni uscita della
        // procedura (EmitReturnStatement + coda di EmitStatementBody). _currentReturnType
        // è null SOLO per il corpo top-level del modulo (Main): DEFER lì resta non
        // supportato (nessun punto di funzione/procedura a cui agganciare l'esecuzione
        // finale) — mai stato un caso d'uso reale sugli altri 4 backend.
        if (_currentReturnType is null)
        {
            _il.Emit(OpCodes.Ldstr, "DEFER a livello top-level (fuori da SUB/FUNCTION) non è supportato.");
            _il.Emit(OpCodes.Newobj, NotSupportedExceptionCtor);
            _il.Emit(OpCodes.Throw);
            return;
        }
        // ON ERROR usa una strategia di compilazione del tutto diversa (dispatch a chunk,
        // EmitProcedureBodyWithErrorHandling): EmitEndProcedureStatement, in quel percorso,
        // esce con Leave verso il dispatcher SENZA passare dal punto di uscita comune che
        // esegue i DEFER — combinarli silenziosamente eseguirebbe la registrazione ma MAI
        // il corpo differito. Fallisce esplicito invece (mai un'esecuzione silenziosamente
        // persa), stessa politica del path top-level sopra.
        if (_onErrorDispatch is not null)
        {
            _il.Emit(OpCodes.Ldstr, "DEFER insieme a ON ERROR nella stessa procedura non è ancora supportato sul path lineare.");
            _il.Emit(OpCodes.Newobj, NotSupportedExceptionCtor);
            _il.Emit(OpCodes.Throw);
            return;
        }
        _currentDefers.Add(stmt.Body);
    }

    /// <summary>Esegue in ordine LIFO i body DEFER registrati nella procedura corrente
    /// (parità Go: l'ultimo DEFER registrato è il primo eseguito). No-op se non ce ne sono.
    /// Chiamata da ogni punto di uscita reale del metodo (RETURN esterno a TRY/FINALLY via
    /// _returnExitLabel, e il fallthrough naturale a fine corpo in EmitStatementBody).</summary>
    private void EmitDeferredBodiesIfAny()
    {
        for (var i = _currentDefers.Count - 1; i >= 0; i--)
            EmitStatement(_currentDefers[i]);
    }

    /// <summary>Uscita "ret difensivo" del fallthrough senza END SUB/FUNCTION esplicito
    /// (EmitStatementBody): valore di default (se FUNCTION) → DEFER LIFO (se presenti) →
    /// ret. Il valore va salvato in un local PRIMA di eseguire i DEFER — uno statement
    /// deferred (es. un IF) potrebbe generare branch/merge il cui bilanciamento dello stack
    /// il verifier CLR richiede coerente; lasciare un valore "in sospeso" sullo stack
    /// mentre si emette IL arbitrario del corpo DEFER è la stessa classe di bug del
    /// ReturnFromTry (vedi EmitReturnStatement) — via il local invece, lo stack resta
    /// pulito (profondità 0) per tutta l'emissione dei body deferred.</summary>
    private void EmitDefaultReturnThenDefersThenRet()
    {
        if (_currentReturnType is not null and not VoidType)
        {
            EmitDefaultValue(_currentReturnType);
            if (_currentDefers.Count > 0)
            {
                var clr = DataTypeMapper.Map(_currentReturnType, _types, _classTypes);
                _returnExitValue ??= _il.DeclareLocal(clr);
                _il.Emit(OpCodes.Stloc, _returnExitValue);
                EmitDeferredBodiesIfAny();
                _il.Emit(OpCodes.Ldloc, _returnExitValue);
            }
        }
        else
        {
            EmitDeferredBodiesIfAny();
        }
        EmitDynamicScopeExit();
        _il.Emit(OpCodes.Ret);
    }

    private void EmitOnceStatement(OnceStmt stmt)
    {
        // ONCE RUN o, DoInit — path LINEARE (senza scheduler, fallback della Proc_<id>): la
        // goroutine gira via StepV2 (EmitOnce nel lowering v2); qui basta COMPILARE e, se il
        // Once non è già Done, eseguire la procedura flat + Complete (approssimazione lineare).
        if (stmt.ProcedureName.Lexeme.Length == 0)
            throw new NotSupportedException("Backend IL: ONCE RUN con nome procedura non valido.");
        var doInitDef = _procedureDefinitions.Values
            .FirstOrDefault(d => d.OriginalName.Lexeme.Replace("_", "")
                .Equals(stmt.ProcedureName.Lexeme.Replace("_", ""), StringComparison.OrdinalIgnoreCase));
        if (doInitDef == null)
            throw new NotSupportedException($"Backend IL: ONCE RUN procedura '{stmt.ProcedureName.Lexeme}' non trovata.");
        var doInitId = doInitDef.SymbolId;

        EmitExpression(stmt.Once);
        _il.Emit(OpCodes.Castclass, typeof(PCodeOnce));
        _il.Emit(OpCodes.Callvirt, PCodeOnceIsDoneGetter);
        var doneLabel = _il.DefineLabel();
        _il.Emit(OpCodes.Brtrue, doneLabel);
        // Esegue la procedura flat (arity 0) + Complete.
        EmitExpression(stmt.Once);
        _il.Emit(OpCodes.Castclass, typeof(PCodeOnce));
        _il.Emit(OpCodes.Ldc_I4, doInitId);
        _il.Emit(OpCodes.Call, IlGpmRuntimeRunOnceBodyMethod);
        _il.MarkLabel(doneLabel);
    }

    private void EmitCondSignalStatement(CondSignalStmt stmt)
    {
        // COND SIGNAL c (parità Go c.Signal()): sveglia al più UN waiter — flat.
        EmitExpression(stmt.Cond);
        _il.Emit(OpCodes.Castclass, typeof(PCodeCond));
        _il.Emit(OpCodes.Ldnull); // scheduler: il path lineare non ha scheduler
        _il.Emit(OpCodes.Callvirt, PCodeCondSignalMethod);
    }

    private void EmitCondBroadcastStatement(CondBroadcastStmt stmt)
    {
        // COND BROADCAST c (parità Go c.Broadcast()): sveglia TUTTI i waiter — flat.
        EmitExpression(stmt.Cond);
        _il.Emit(OpCodes.Castclass, typeof(PCodeCond));
        _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Callvirt, PCodeCondBroadcastMethod);
    }

    private void EmitCondWaitStatement(CondWaitStmt stmt)
    {
        // COND WAIT c, mu nel path LINEARE: non c'è scheduler → non supportato (è un park
        // GPM). Il body standard di una procedura may-park viene emesso ma mai eseguito
        // (il driver v2 usa lo stepper StepV2_<id>); se un ipotetico run lineare lo
        // raggiungesse, l'errore è esplicito.
        EmitExpression(stmt.Mutex);
        _il.Emit(OpCodes.Castclass, typeof(PCodeMutex));
        _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Callvirt, PCodeMutexUnlockMethod);
        _il.Emit(OpCodes.Ldstr, "COND WAIT richiede la GPM v2 (path lineare non supportato).");
        _il.Emit(OpCodes.Newobj, NotSupportedExceptionCtor);
        _il.Emit(OpCodes.Throw);
    }

    private static readonly System.Reflection.MethodInfo PCodeMutexTryLockMethod =
        typeof(PCodeMutex).GetMethod(nameof(PCodeMutex.TryLock))!;
    private static readonly System.Reflection.MethodInfo PCodeMutexUnlockMethod =
        typeof(PCodeMutex).GetMethod(nameof(PCodeMutex.Unlock))!;
    private static readonly System.Reflection.MethodInfo PCodeRwMutexTryRLockMethod =
        typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.TryRLock))!;
    private static readonly System.Reflection.MethodInfo PCodeRwMutexRUnlockMethod =
        typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.RUnlock))!;
    private static readonly System.Reflection.MethodInfo PCodeRwMutexTryLockMethod =
        typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.TryLock))!;
    private static readonly System.Reflection.MethodInfo PCodeRwMutexUnlockMethod =
        typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.Unlock))!;
    private static readonly System.Reflection.PropertyInfo PCodeOnceIsDoneProperty =
        typeof(PCodeOnce).GetProperty(nameof(PCodeOnce.IsDone))!;
    private static readonly System.Reflection.MethodInfo PCodeOnceIsDoneGetter =
        PCodeOnceIsDoneProperty.GetGetMethod()!;
    private static readonly System.Reflection.MethodInfo IlGpmRuntimeRunOnceBodyMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.RunOnceBody))!;
    private static readonly System.Reflection.MethodInfo PCodeCondSignalMethod =
        typeof(PCodeCond).GetMethod(nameof(PCodeCond.Signal))!;
    private static readonly System.Reflection.MethodInfo PCodeCondBroadcastMethod =
        typeof(PCodeCond).GetMethod(nameof(PCodeCond.Broadcast))!;
    private static readonly System.Reflection.ConstructorInfo NotSupportedExceptionCtor =
        typeof(NotSupportedException).GetConstructor([typeof(string)])!;

    private void EmitCloseChannelStatement(CloseChannelStmt stmt)
    {
        // CHANNEL CLOSE ch (parità Go close(ch)): chiude atomicamente; double-close → errore.
        EmitExpression(stmt.Channel);
        _il.Emit(OpCodes.Call, IlGpmRuntimeCloseChannelMethod);
    }

    private static readonly System.Reflection.MethodInfo IlGpmRuntimeCloseChannelMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.CloseChannel))!;

    private void EmitGoSelectStatement(GoSelectStmt stmt)
    {
        // GO SELECT in IL sincrono (niente scheduler): tenta ogni case in ORDINE; il primo
        // la cui comunicazione è PRONTA (RECEIVE → TryReceive, SEND → TrySend) esegue il
        // suo body; se nessuno è pronto e c'è DEFAULT, esegue quello. Non c'è blocco.
        var doneLabel = _il.DefineLabel();

        foreach (var caseClause in stmt.Cases)
        {
            var nextCaseLabel = _il.DefineLabel();
            switch (caseClause.CommunicationStmt)
            {
                case ReceiveStmt recv:
                {
                    // object tmp; channel.TryReceive(out tmp) → bool; false → case successivo.
                    var tmp = _il.DeclareLocal(_types.Object);
                    EmitExpression(recv.Channel);
                    _il.Emit(OpCodes.Ldloca, tmp);
                    var tryReceive = typeof(ChannelInfo).GetMethod("TryReceive")!;
                    _il.Emit(OpCodes.Call, tryReceive);
                    _il.Emit(OpCodes.Brfalse, nextCaseLabel);
                    // Il valore ricevuto (boxed in tmp) va nella destination se presente.
                    if (recv.Destination is VariableTarget vt)
                    {
                        _il.Emit(OpCodes.Ldloc, tmp);
                        var targetDataType = NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType);
                        var clr = DataTypeMapper.Map(targetDataType, _types, _classTypes);
                        if (IlConversion.IsNumeric(targetDataType))
                        {
                            // Boxed Double (da SEND 42) → ToDouble + conv (mai Unbox_Any:
                            // lancerebbe InvalidCastException su Double≠Int16).
                            _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                            IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, clr);
                        }
                        else if (clr != typeof(object))
                        {
                            if (clr.IsValueType) _il.Emit(OpCodes.Unbox_Any, clr);
                            else _il.Emit(OpCodes.Castclass, clr);
                        }
                        _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
                    }
                    break;
                }
                case SendStmt send:
                {
                    // channel.TrySend(value) → bool; false → case successivo.
                    EmitExpression(send.Channel);
                    EmitExpression(send.Value);
                    var vClr = DataTypeMapper.Map(NormalizeInferredType(InferType(send.Value)), _types, _classTypes);
                    if (vClr.IsValueType) _il.Emit(OpCodes.Box, vClr);
                    var trySend = typeof(ChannelInfo).GetMethod("TrySend", [typeof(object)])!;
                    _il.Emit(OpCodes.Call, trySend);
                    _il.Emit(OpCodes.Brfalse, nextCaseLabel);
                    break;
                }
                case TimeoutStmt:
                    // Path lineare SINCRONO (nessuno scheduler): il TIMEOUT non può mai
                    // scattare — case mai pronto → passa al successivo (il Main v2 gira via
                    // RunV2Entry/StepV2, che gestisce il timeout reale; qui il Main lineare
                    // deve solo COMPILARE).
                    _il.Emit(OpCodes.Br, nextCaseLabel);
                    break;
                default:
                    throw new NotSupportedException("Backend IL: GO SELECT con comunicazione non SEND/RECEIVE non supportata.");
            }
            foreach (var bodyStmt in caseClause.Body) EmitStatement(bodyStmt);
            _il.Emit(OpCodes.Br, doneLabel);
            _il.MarkLabel(nextCaseLabel);
        }

        if (stmt.DefaultCase is { } defaultCase)
            foreach (var bodyStmt in defaultCase) EmitStatement(bodyStmt);

        _il.MarkLabel(doneLabel);
    }

    private void EmitWaitStatement(WaitStmt stmt)
    {
        // In IL il GO SUB è INLINE (sincrono): i task sono già completati al WAIT, e non
        // esiste un task corrente (IslandRuntime.CurrentTask lancia). Il WaitGroup è valutato
        // per preservare side-effect di espressioni, poi scartato: WAIT diventa un no-op
        // sicuro (NetSslConcurrency.bas, fix 2026-08-11).
        EmitExpression(stmt.WaitGroup);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitDoneStatement(DoneStmt stmt)
    {
        // Come WAIT: no-op sicuro (DoneAndWake richiederebbe CurrentTask.Scheduler).
        EmitExpression(stmt.WaitGroup);
        _il.Emit(OpCodes.Pop);
    }

    /// <summary>Comprensione di lista [ x*2 FOR x IN arr IF cond ] — genera il loop annidato
    /// in IL (List<object> + for + condition + Add), stessa semantica della VM
    /// (VisitListComprehensionExpression: MK_LIST + AADD).</summary>
}
