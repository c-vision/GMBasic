// FILE: IlBackendCodegen/Gpm/Ir/IlCoroutineEmitter.MethodEmitter.Control.cs
// PARTIAL — dominio COROUTINE/CONTROL: if/select/for/expr-object/field-access/binary.
// Migrato da IlCoroutineEmitter.MethodEmitter.Park.cs (2026-08-30 — split).

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Reflection;
using System.Reflection.Emit;
using Basic.IlBackend.Gpm;
using Basic.IlBackend.Gpm.Ir;
using Basic.Runtime;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

internal sealed partial class MethodEmitter
{
        private void EmitIf(IlCoroutineBlock b, IfStmt ifs)
        {
            var targets = b.Terminator.Targets;
            for (var i = 0; i < ifs.IfClauses.Length; i++)
            {
                EmitConditionBoolV2(ifs.IfClauses[i].Condition);
                var next = _il.DefineLabel();
                _il.Emit(OpCodes.Brfalse, next);
                EmitContinueTo(targets[i]);
                _il.MarkLabel(next);
            }
            // else / successore
            EmitContinueTo(targets[targets.Length - 1]);
        }

        /// <summary>Condizione di IF/CondJump nel subset v2: lascia un BOOL int32 (0/1) pulito.
        /// Le condizioni numeriche/logiche (confronti, AND/OR, variabili) passano da
        /// EmitExprDouble + `!= 0` — MAI box + IsTruthy: `box System.Double` produce un tipo
        /// statico "boxed" che il verifier/JIT rifiuta quando passato a IsTruthy(object) in
        /// condizioni composte (AND annidati → InvalidProgram, PCodeGpmAtomic).
        /// `!= 0` (non `> 0`): in BASIC vero = -1, quindi `NOT flag` (= -1) deve essere vero —
        /// con `>0` risultava falso (bug DoWhileThreadFlag: DO WHILE NOT flag non iterava).</summary>
        private void EmitConditionBoolV2(Expr cond)
        {
            if (cond is BinaryExpr or UnaryExpr or VariableExpr or NumberExpr or BooleanExpr or GroupingExpr)
            {
                EmitExprDouble(cond);
                _il.Emit(OpCodes.Ldc_R8, 0.0);
                _il.Emit(OpCodes.Ceq);
                _il.Emit(OpCodes.Ldc_I4_0);
                _il.Emit(OpCodes.Ceq); // 1 se != 0, altrimenti 0
                return;
            }
            EmitExprObject(cond);
            _il.Emit(OpCodes.Call, IsTruthyMethod);
        }

        // ─── SELECT ────────────────────────────────────────────────────────────────────────

        private void EmitSelect(IlCoroutineBlock b, SelectStmt sel)
        {
            var targets = b.Terminator.Targets;
            var clauses = sel.CaseClauses;

            // Un label per il body di ogni CASE: il match salta lì; il non-match cade al
            // test successivo; nessun match → else (targets[ultimo]).
            var bodyLabels = new Label[clauses.Length];
            for (var i = 0; i < clauses.Length; i++) bodyLabels[i] = _il.DefineLabel();

            for (var i = 0; i < clauses.Length; i++)
            {
                foreach (var test in clauses[i].Tests)
                {
                    switch (test)
                    {
                        case CaseValue cv:
                            EmitExprObject(sel.Expression);
                            _il.Emit(OpCodes.Call, AsDoubleMethod);
                            EmitExprObject(cv.Value);
                            _il.Emit(OpCodes.Call, AsDoubleMethod);
                            _il.Emit(OpCodes.Beq, bodyLabels[i]);
                            break;
                        case CaseRange cr:
                            // match se (test >= start && test <= end): due branch al body.
                            EmitExprObject(sel.Expression);
                            _il.Emit(OpCodes.Call, AsDoubleMethod);
                            EmitExprObject(cr.Start);
                            _il.Emit(OpCodes.Call, AsDoubleMethod);
                            _il.Emit(OpCodes.Bge, bodyLabels[i]);
                            EmitExprObject(sel.Expression);
                            _il.Emit(OpCodes.Call, AsDoubleMethod);
                            EmitExprObject(cr.End);
                            _il.Emit(OpCodes.Call, AsDoubleMethod);
                            _il.Emit(OpCodes.Ble, bodyLabels[i]);
                            break;
                        default:
                            throw new NotSupportedException("StepV2: CaseTest non supportato (G2).");
                    }
                }
            }

            // Nessun CASE ha matchato → else / successore.
            EmitContinueTo(targets[targets.Length - 1]);

            // Body dei CASE.
            for (var i = 0; i < clauses.Length; i++)
            {
                _il.MarkLabel(bodyLabels[i]);
                EmitContinueTo(targets[i]);
            }
        }

        // ─── FOR check ─────────────────────────────────────────────────────────────────────

        private void EmitForCheck(IlCoroutineBlock b, ForStmt forStmt)
        {
            var varSlot = SlotOf(forStmt.Variable.SymbolId);
            var stepPositive = StepIsPositive(forStmt);
            // cond = step >= 0 ? I <= End : I >= End
            LoadSlotDouble(varSlot);
            EmitExprDouble(forStmt.End);
            var body = _il.DefineLabel();
            var exit = _il.DefineLabel();
            _il.Emit(stepPositive ? OpCodes.Ble : OpCodes.Bge, body);
            EmitContinueTo(b.Terminator.Targets[1]); // uscita
            _il.MarkLabel(body);
            EmitContinueTo(b.Terminator.Targets[0]); // corpo
        }

        private bool StepIsPositive(ForStmt forStmt)
        {
            if (forStmt.Step is not { } step) return true;
            if (step is NumberExpr num) return num.Value >= 0;
            return true;
        }

        // ─── ConditionalJump (WHILE label form) ────────────────────────────────────────────

        private void EmitCondJump(IlCoroutineBlock b, ConditionalJumpStmt cj)
        {
            EmitConditionBoolV2(cj.Condition);
            var jump = _il.DefineLabel();
            // JumpWhenTrue ? brtrue : brfalse verso il target[0]
            _il.Emit(cj.JumpWhenTrue ? OpCodes.Brtrue : OpCodes.Brfalse, jump);
            EmitContinueTo(b.Terminator.Targets[1]); // successore
            _il.MarkLabel(jump);
            EmitContinueTo(b.Terminator.Targets[0]); // etichetta (pc risolto dal lowering)
        }

        // ─── Call suspendable ──────────────────────────────────────────────────────────────

        private void EmitCall(IlCoroutineBlock b)
        {
            var t = b.Terminator;
            var unwrapped = b.AstNode switch
            {
                ExpressionStmt { Expression: FunctionExpr f1 } => f1,
                ExpressionStmt { Expression: BuiltinFunctionCallExpr bfc1 } => bfc1,
                LetStmt { Assignments: { Length: 1 } } lt => UnwrapConversions(lt.Assignments[0].Value),
                DeclarationStmt { Declarations: { Length: 1 } } ds => UnwrapConversions(ds.Declarations[0].InitialValue!),
                _ => (Expr?)null
            };
            ImmutableArray<Expr> callArgs = unwrapped switch
            {
                FunctionExpr f => f.Args,
                BuiltinFunctionCallExpr bfcA => bfcA.Args,
                _ => ImmutableArray<Expr>.Empty
            };
            var targetProc = t.TargetProc;
            var calleeDef = _procedureForId(targetProc);

            // StepCall(resumePc, targetProc, args[], dest, targetLocalCount, retSlot, ctx)
            _il.Emit(OpCodes.Ldc_I4, t.ResumePc);
            _il.Emit(OpCodes.Ldc_I4, targetProc);

            // Argomenti: BYREF → RefToVar(frame.Locals, slot) (il callee scrive nel caller);
            // altrimenti il valore inline → object[].
            _il.Emit(OpCodes.Ldc_I4, callArgs.Length);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var i = 0; i < callArgs.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                bool byRef = calleeDef is { Parameters: { Length: > 0 } pars2 } && i < pars2.Length
                    && pars2[i].Flags.HasFlag(SymbolFlags.IsByRef);
                if (byRef && callArgs[i] is VariableExpr bv)
                {
                    // new RefToVar(frame.Locals, slot)
                    EmitLdarg(_frameArg);
                    _il.Emit(OpCodes.Ldfld, FrameLocalsField);
                    _il.Emit(OpCodes.Ldc_I4, SlotOf(bv.SymbolId));
                    _il.Emit(OpCodes.Newobj, RefToVarCtor);
                }
                else EmitExprObject(callArgs[i]);
                _il.Emit(OpCodes.Stelem_Ref);
            }

            // Destinazione del RETURN: Discard per le SUB void; Local (slot del target) per le
            // FUNCTION. ReturnSlot nel terminator = SymbolId del target → slot del frame.
            var dest = t.ReturnDest;
            var retSlot = t.ReturnSlot >= 0 ? SlotOf(t.ReturnSlot) : -1;
            _il.Emit(OpCodes.Ldc_I4, (int)dest);
            _il.Emit(OpCodes.Ldc_I4, callArgs.Length); // targetLocalCount
            _il.Emit(OpCodes.Ldc_I4, retSlot);
            // G4 closure: la call a una procedura locale con cattura porta l'istanza __context
            // (un local del CHIAMANTE, dichiarato dal Linker) come ClosureContext della
            // transizione — il driver lo salva nel frame del callee e lo lega allo stepper.
            if (_closureClassOf is not null && _closureClassOf.TryGetValue(targetProc, out var closureClassId))
            {
                var ctxSlot = FindClosureContextSlot(closureClassId);
                LoadSlotObject(ctxSlot);
            }
            else _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Call, StepCallFactoryMethod);
            EmitReturnTransition();
        }

        /// <summary>Cerca nel corpo corrente il local `__context` dichiarato dal Linker per una
        /// classe di closure (DIM __context AS __ClosureCtx_... = NEW ...): il SymbolId del DIM
        /// identifica lo slot del frame. Stesso ruolo di FindClosureContextLocal del flat.</summary>
        private int FindClosureContextSlot(int closureClassId)
        {
            foreach (var s in _def.Body)
            {
                if (s is not DeclarationStmt d) continue;
                foreach (var decl in d.Declarations)
                {
                    if (decl.DataType is ClassType ct && ct.Definition.SymbolId == closureClassId)
                        return SlotOf(decl.SymbolId);
                }
            }
            throw new NotSupportedException(
                $"StepV2: call a closure '{_def.OriginalName.Lexeme}' senza local __context (classe {closureClassId}).");
        }

        // ─── Park ──────────────────────────────────────────────────────────────────────────

        private void EmitPark(IlCoroutineBlock b)
        {
            // Implicit-await PARCHEGGIANTE del future (GO FUNCTION INTO / FETCH): attende che
            // il future risolva (TryReadFuturePark) — parità Go (v, ok := <-future). Il valore
            // risolto viene scritto NELLO SLOT; al wake il blocco rientra e il read lo trova.
            if (b.FutureSlot is { } fs)
            {
                EmitLdarg(_taskArg);
                EmitLdarg(_frameArg);
                _il.Emit(OpCodes.Ldc_I4, fs);
                _il.Emit(OpCodes.Call, TryReadFutureParkMethod);
                var ok = _il.DefineLabel();
                _il.Emit(OpCodes.Brtrue, ok);
                EmitSetPc(b.Pc);
                EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
                EmitReturnTransition();
                _il.MarkLabel(ok);
                EmitContinueTo(b.Terminator.ResumePc);
                return;
            }
            if (b.Aux is SqlExecFnPark sef) { EmitSqlExecFnParkValue(b, sef); return; }
            var stmt = b.AstNode as Stmt;
            switch (stmt)
            {
                case SleepStmt sleep:
                    EmitSleep(b, sleep);
                    break;
                case CondWaitStmt cw:
                    EmitCondWait(b, cw);
                    break;
                case ForEachAdvanceAndAssignStmt fea:
                    EmitForEachChannel(b, fea);
                    break;
                case SendStmt send:
                    EmitSend(b, send);
                    break;
                case ReceiveStmt recv:
                    EmitReceive(b, recv);
                    break;
                case WaitStmt wait:
                    EmitWait(b, wait);
                    break;
                case WaitKeyStmt:
                    EmitWaitKeyPark(b.Pc, b.Terminator.ResumePc, null);
                    break;
                case GetKeyboardStmt gkPark:
                    EmitWaitKeyPark(b.Pc, b.Terminator.ResumePc, gkPark.Variable);
                    break;
                case ClipperReadStmt:
                    EmitReadPark(b);
                    break;
                case TcpConnectStmt tcpP:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, TcpConnectStepMethod,
                        () => { EmitStringArg(tcpP.Host); EmitInt32Arg(tcpP.Port); }, tcpP.Handle);
                    break;
                case SslOpenStmt sslP:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, SslOpenStepMethod,
                        () =>
                        {
                            EmitInt32Arg(sslP.Port);
                            EmitStringArg(sslP.CertificatePath);
                            if (sslP.CertificatePassword is { } cpp) EmitExprObject(cpp);
                            else _il.Emit(OpCodes.Ldnull);
                        }, sslP.Handle);
                    break;
                case SslConnectStmt sslC3:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, SslConnectStepMethod,
                        () =>
                        {
                            EmitStringArg(sslC3.Host);
                            // Porta omessa → 443 di default (parità col flat
                            // EmitSslConnectStatement e gli altri backend; 0 = SocketException).
                            if (sslC3.Port is { } sp3) EmitInt32Arg(sp3);
                            else _il.Emit(OpCodes.Ldc_I4, 443);
                            _il.Emit(OpCodes.Ldnull);
                        }, sslC3.Handle);
                    break;
                case WebSocketConnectStmt wsP:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, WsConnectStepMethod,
                        () =>
                        {
                            EmitStringArg(wsP.Host);
                            EmitInt32Arg(wsP.Port);
                            if (wsP.Path is { } wpp) EmitStringArg(wpp);
                            else _il.Emit(OpCodes.Ldnull);
                            if (wsP.Timeout is { } wtp) EmitExprObject(wtp);
                            else _il.Emit(OpCodes.Ldnull);
                        }, wsP.Handle);
                    break;
                case TcpSendStmt tcpSp:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, TcpSendStepMethod,
                        () => { EmitInt64Arg(tcpSp.ClientHandle); EmitExprObject(tcpSp.Data); },
                        null);
                    break;
                case TcpRecvStmt tcpRp:
                    EmitTcpRecvPark(b, tcpRp);
                    break;
                case TcpAcceptStmt tcpAp:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, TcpAcceptStepMethod,
                        () => EmitInt64Arg(tcpAp.ServerHandle), tcpAp.ClientHandle);
                    break;
                case UdpRecvStmt udpRp:
                    EmitUdpRecvPark(b, udpRp);
                    break;
                case SyncStmt:
                    EmitGraphicsVoidPark(b.Pc, b.Terminator.ResumePc, TrySyncParkMethod, () => { });
                    break;
                case ScreenStmt scrP:
                    EmitGraphicsVoidPark(b.Pc, b.Terminator.ResumePc, TryScreenParkMethod, () =>
                    {
                        EmitInt32Arg(scrP.Mode);
                        if (scrP.ActivePage is { } ap) EmitInt32Arg(ap); else _il.Emit(OpCodes.Ldc_I4, 0);
                        if (scrP.VisiblePage is { } vp) EmitInt32Arg(vp); else _il.Emit(OpCodes.Ldc_I4, 0);
                        _il.Emit(OpCodes.Ldc_I4, 0);
                    });
                    break;
                case PointStmt ptP:
                    EmitGraphicsIntPark(b.Pc, b.Terminator.ResumePc, TryPointParkMethod,
                        () => { EmitInt32Arg(ptP.X); EmitInt32Arg(ptP.Y); },
                        ptP.Color is VariableExpr cve ? cve.SymbolId : (int?)null);
                    break;
                case LoadTextureStmt ltP when ltP.CellWidth is null && ltP.CellHeight is null:
                    EmitGraphicsIntPark(b.Pc, b.Terminator.ResumePc, TryLoadTextureParkMethod,
                        () => EmitStringArg(ltP.FileName),
                        ltP.Destination is VariableTarget lvt ? lvt.SymbolId : (int?)null);
                    break;
                case SpriteCommandStmt spP when spP.Command.Kind == TokenKind.SpriteLoad:
                    EmitGraphicsIntPark(b.Pc, b.Terminator.ResumePc, TrySpriteLoadParkMethod,
                        () =>
                        {
                            if (spP.Arguments.Length >= 1) EmitStringArg(spP.Arguments[0]);
                            else _il.Emit(OpCodes.Ldstr, "");
                        },
                        spP.Arguments.Length >= 2 && spP.Arguments[1] is VariableExpr sve2 ? sve2.SymbolId : (int?)null);
                    break;
                case LoadTextureStmt ltSheet when ltSheet.CellWidth is not null && ltSheet.CellHeight is not null:
                    // LOAD TEXTURE ... CELL(w,h): risultato object (int[]) → Destination.
                    EmitGraphicsObjPark(b.Pc, b.Terminator.ResumePc, TryLoadTextureSheetParkMethod,
                        () =>
                        {
                            EmitStringArg(ltSheet.FileName);
                            EmitInt32Arg(ltSheet.CellWidth);
                            EmitInt32Arg(ltSheet.CellHeight);
                        },
                        ltSheet.Destination is VariableTarget shvt ? shvt.SymbolId : (int?)null);
                    break;
                case SpriteCommandStmt spGp when spGp.Command.Kind == TokenKind.SpriteGetPixels:
                    // SPRITE GETPIXELS spriteId, destVar: risultato object (array) → Arguments[1].
                    EmitGraphicsObjPark(b.Pc, b.Terminator.ResumePc, TrySpriteGetPixelsParkMethod,
                        () =>
                        {
                            if (spGp.Arguments.Length >= 1) EmitInt32Arg(spGp.Arguments[0]);
                            else _il.Emit(OpCodes.Ldc_I4, 0);
                        },
                        spGp.Arguments.Length >= 2 && spGp.Arguments[1] is VariableExpr gve ? gve.SymbolId : (int?)null);
                    break;
                case MutexLockStmt mu:
                    EmitMutexLock(b, mu);
                    break;
                case MutexRLockStmt mrl:
                    EmitMutexRLock(b, mrl);
                    break;
                case OnceStmt onceS:
                    EmitOnce(b, onceS);
                    break;
                case SqlExecStmt seSql:
                    EmitSqlExecPark(b, seSql);
                    break;
                default:
                    throw new NotSupportedException($"StepV2: park non supportato per {stmt?.GetType().Name ?? "null"} (G2).");
            }
        }

        /// <summary>Alias SQL in espressione: un NOME è la STRINGA del nome (parità
        /// EmitAliasOrExpression), altrimenti l'espressione normale.</summary>
        private void EmitSqlAliasStringLocal(Expr e)
        {
            if (e is VariableExpr ve) { _il.Emit(OpCodes.Ldstr, ve.NameCleaned.Lexeme); return; }
            if (e is QualifiedNameExpr qn && qn.Parts.Length == 1) { _il.Emit(OpCodes.Ldstr, qn.Parts[0].Lexeme); return; }
            EmitStringArg(e);
        }

        /// <summary>Park SQL.EXEC forma FUNZIONE (con valore): TrySqlExecFnPark(task, frame,
        /// alias, sql, params, out changes); al ritorno scrive changes nello slot target.</summary>
        private void EmitSqlExecFnParkValue(IlCoroutineBlock b, SqlExecFnPark sef)
        {
            var local = _il.DeclareLocal(typeof(double));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitSqlAliasStringLocal(sef.Call.Args[0]);
            EmitStringArg(sef.Call.Args[1]);
            var extra = sef.Call.Args.Length - 2;
            _il.Emit(OpCodes.Ldc_I4, extra);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var i = 0; i < extra; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                EmitExprObject(sef.Call.Args[i + 2]);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Ldloca, local);
            _il.Emit(OpCodes.Call, TrySqlExecFnParkMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            if (sef.TargetSlot >= 0) { _il.Emit(OpCodes.Ldloc, local); StoreSlotDouble(SlotOf(sef.TargetSlot)); }
            EmitContinueTo(b.Terminator.ResumePc);
        }

        /// <summary>SQL EXEC (statement) con coarse-park: se heavy, TrySqlExecPark lo esegue
        /// fuori dal worker e parcheggia; altrimenti prosegue subito.</summary>
        private void EmitSqlExecPark(IlCoroutineBlock b, SqlExecStmt se)
        {
            EmitGraphicsVoidPark(b.Pc, b.Terminator.ResumePc, TrySqlExecParkMethod, () =>
            {
                // Alias come STRINGA del nome (parità flat EmitSqlAliasCore + EmitSqlExecFnPark):
                // un NOME non è una variabile e non ha slot (SymbolId -1 → LoadSlotObject
                // rompeva l'emissione; bug 2026-10-01, todo Coarse-Park).
                EmitSqlAliasStringLocal(se.DbAlias);
                EmitExprObject(se.Sql);
                _il.Emit(OpCodes.Ldc_I4, se.Parameters.Length);
                _il.Emit(OpCodes.Newarr, typeof(object));
                for (var i = 0; i < se.Parameters.Length; i++)
                {
                    _il.Emit(OpCodes.Dup);
                    _il.Emit(OpCodes.Ldc_I4, i);
                    EmitExprObject(se.Parameters[i]);
                    _il.Emit(OpCodes.Stelem_Ref);
                }
            });
        }

        private void EmitSleep(IlCoroutineBlock b, SleepStmt sleep)
        {
            // G5: timer await unificato (piano §9, generation-safe). SleepStep ritorna true se
            // si continua subito (durata ≤ 0, o wake già consumato), false se parcheggiato.
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitExprDouble(sleep.Duration);
            _il.Emit(OpCodes.Call, SleepStepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            // parcheggiato: retry sullo stesso block (il wake rientra qui → TryConsume).
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitContinueTo(b.Terminator.ResumePc);
        }

        private void EmitCondWait(IlCoroutineBlock b, CondWaitStmt cw)
        {
            // G8-bis: COND WAIT c, mu (parità Go sync.Cond.Wait) — fase 1 rilascia il mutex e
            // parcheggia sul cond; fase 2 (wake dal SIGNAL) riacquisisce il mutex (handoff).
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitExprObject(cw.Cond);
            _il.Emit(OpCodes.Castclass, typeof(PCodeCond));
            EmitExprObject(cw.Mutex);
            _il.Emit(OpCodes.Castclass, typeof(PCodeMutex));
            _il.Emit(OpCodes.Call, CondWaitStepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitContinueTo(b.Terminator.ResumePc);
        }

        private void EmitForEachChannel(IlCoroutineBlock b, ForEachAdvanceAndAssignStmt fea)
        {
            // FOR EACH v IN ch (parità Go for v := range ch): RECEIVE con flag OK. Se ok → il
            // valore va alla variabile del loop e si continua col corpo; se !ok (canale chiuso
            // e svuotato) → EXIT. Il canale è nello slot registrato dal ForEachInitStmt.
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            LoadRawSlotObject(_channelForEachSlot);
            _il.Emit(OpCodes.Castclass, typeof(ChannelInfo));
            _feValueLocal ??= _il.DeclareLocal(typeof(object));
            _feOkLocal ??= _il.DeclareLocal(typeof(bool));
            _il.Emit(OpCodes.Ldloca, _feValueLocal);
            _il.Emit(OpCodes.Ldloca, _feOkLocal);
            _il.Emit(OpCodes.Call, ForEachChannelStepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            // Parcheggiato: retry sullo stesso block (il wake rientra qui → TryConsume).
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            // ok flag → continua col corpo; !ok → EXIT (Targets[0] = pc dell'etichetta di exit).
            var exit = _il.DefineLabel();
            _il.Emit(OpCodes.Ldloc, _feOkLocal!);
            _il.Emit(OpCodes.Brfalse, exit);
            var vt = fea.Variables[0];
            var varSlot = _slotOfName.TryGetValue(vt.Lexeme, out var vs) ? vs : throw new InvalidOperationException($"StepV2: variabile del FOR EACH '{vt.Lexeme}' senza slot.");
            _il.Emit(OpCodes.Ldloc, _feValueLocal!);
            StoreSlotObject(varSlot);
            EmitContinueTo(b.Terminator.ResumePc);
            _il.MarkLabel(exit);
            EmitContinueTo(b.Terminator.Targets.Length > 0 ? b.Terminator.Targets[0] : b.Terminator.ResumePc);
        }

        private void EmitSend(IlCoroutineBlock b, SendStmt send)
        {
            // G8-bis: parità Go chan direzionali — SEND su un canale ReceiveOnly ('<-chan T') è
            // un errore di compilazione (come il PCode Compiler). Check statico nel lowering v2.
            if (_program is not null && send.Channel is VariableExpr chVarSend && chVarSend.SymbolId >= 0
                && _program.Symbols.GetSymbol(chVarSend.SymbolId).DataType is ChannelType chtSend
                && chtSend.Direction == ChannelDirection.ReceiveOnly)
                throw new NotSupportedException($"StepV2: SEND su un canale ReceiveOnly ('<-chan T'): non si può inviare su '{send.Channel}'.");
            // G5: send await unificato (IlChannelSendAwait, generation-safe).
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitExprObject(send.Channel);
            _il.Emit(OpCodes.Castclass, typeof(ChannelInfo));
            EmitExprObject(send.Value);
            _il.Emit(OpCodes.Call, SendStepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            // parcheggiato: retry sullo stesso block (ri-send sul wake)
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitContinueTo(b.Terminator.ResumePc);
        }

        private void EmitReceive(IlCoroutineBlock b, ReceiveStmt recv)
        {
            // G8-bis: parità Go chan direzionali — RECEIVE su un canale SendOnly ('chan<- T') è
            // un errore di compilazione (come il PCode Compiler). Check statico nel lowering v2.
            if (_program is not null && recv.Channel is VariableExpr chVarRecv && chVarRecv.SymbolId >= 0
                && _program.Symbols.GetSymbol(chVarRecv.SymbolId).DataType is ChannelType chtRecv
                && chtRecv.Direction == ChannelDirection.SendOnly)
                throw new NotSupportedException($"StepV2: RECEIVE su un canale SendOnly ('chan<- T'): non si può ricevere da '{recv.Channel}'.");
            // G5: receive await unificato (IlChannelReceiveAwait, generation-safe).
            var valueLocal = _il.DeclareLocal(typeof(object));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitExprObject(recv.Channel);
            _il.Emit(OpCodes.Castclass, typeof(ChannelInfo));
            _il.Emit(OpCodes.Ldloca, valueLocal);
            _il.Emit(OpCodes.Call, ReceiveStepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            if (recv.Destination is VariableTarget vt)
            {
                _il.Emit(OpCodes.Ldloc, valueLocal);
                StoreSlotObject(SlotOf(vt.SymbolId));
            }
            // OK flag (parità Go `v, ok := <-ch`): 1 se il valore è stato ricevuto, 0 se il
            // canale è chiuso (receive su chiuso vuoto ritorna true con value = null).
            if (recv.OkFlag is VariableTarget okVar)
            {
                var isClosed = _il.DefineLabel();
                var done = _il.DefineLabel();
                _il.Emit(OpCodes.Ldloc, valueLocal);
                _il.Emit(OpCodes.Brfalse, isClosed);
                _il.Emit(OpCodes.Ldc_R8, 1.0);
                _il.Emit(OpCodes.Br, done);
                _il.MarkLabel(isClosed);
                _il.Emit(OpCodes.Ldc_R8, 0.0);
                _il.MarkLabel(done);
                StoreSlotDouble(SlotOf(okVar.SymbolId));
            }
            EmitContinueTo(b.Terminator.ResumePc);
        }

        private void EmitWait(IlCoroutineBlock b, WaitStmt wait)
        {
            // G5: waitgroup await unificato (IlWaitGroupAwait, generation-safe).
            // WaitStep ritorna true = pronto (continua), false = parcheggiato.
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitExprObject(wait.WaitGroup);
            _il.Emit(OpCodes.Call, WaitStepMethod); // WaitStep no-ops per WAIT <numero> (non-waitgroup)
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitContinueTo(b.Terminator.ResumePc);
        }

        /// <summary>True se l'espressione è un RWMUTEX (il tipo del simbolo è RwMutexType) —
        /// LOCK/UNLOCK dispatchano su PCodeRwMutex invece di PCodeMutex.</summary>
        private bool IsRwMutexExpr(Expr e)
            => _program is not null && e is VariableExpr v && v.SymbolId >= 0
                && _program.Symbols.GetSymbol(v.SymbolId).DataType is RwMutexType;

        private void EmitMutexLock(IlCoroutineBlock b, MutexLockStmt mu)
        {
            // MUTEX LOCK mu (parità Go mu.Lock()): se il mutex è detenuto il task parcheggia;
            // al wake la proprietà è passata per HANDOFF (Unlock ri-assegna il lock al waiter) —
            // quindi il resume va OLTRE il lock (ResumePc), NON un retry (che fallirebbe perché
            // il mutex è già ri-assegnato al task svegliato). Se libero, acquisisce e continua.
            // RWMUTEX: `MUTEX LOCK rw` con rw RWMUTEX usa PCodeRwMutex.TryLock (scrittore).
            var mutexClr = IsRwMutexExpr(mu.Mutex) ? typeof(PCodeRwMutex) : typeof(PCodeMutex);
            var tryLock = mutexClr == typeof(PCodeRwMutex) ? RwMutexTryLockMethod : MutexTryLockMethod;
            EmitExprObject(mu.Mutex);
            _il.Emit(OpCodes.Castclass, mutexClr);
            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Callvirt, tryLock);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Terminator.ResumePc); // handoff: al wake si prosegue senza ri-ritentare
            EmitTransition(IlStepKind.Park, b.Terminator.ResumePc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitContinueTo(b.Terminator.ResumePc);
        }

        private void EmitMutexRLock(IlCoroutineBlock b, MutexRLockStmt mrl)
        {
            // RWMUTEX RLOCK (parità Go rw.RLock()): lettura condivisa — se uno scrittore ha il
            // lock il task parcheggia (handoff); altrimenti acquisisce e continua.
            EmitExprObject(mrl.Mutex);
            _il.Emit(OpCodes.Castclass, typeof(PCodeRwMutex));
            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Callvirt, RwMutexTryRLockMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Terminator.ResumePc); // handoff: al wake si prosegue senza ri-ritentare
            EmitTransition(IlStepKind.Park, b.Terminator.ResumePc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitContinueTo(b.Terminator.ResumePc);
        }

        /// <summary>ONCE RUN o, DoInit (parità Go sync.Once.Do): il primo chiamante esegue la
        /// procedura flat e marca il Once (RunOnceBody = DoInit + Complete); i chiamanti
        /// concorrenti parcheggiano (IlOnceAwait) e al wake saltano (il Once è Done).</summary>
        private void EmitOnce(IlCoroutineBlock b, OnceStmt once)
        {
            if (_nameToId is null || !_nameToId.TryGetValue(once.ProcedureName.Lexeme, out var doInitId))
                throw new NotSupportedException($"StepV2: ONCE RUN procedura '{once.ProcedureName.Lexeme}' non trovata.");
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitExprObject(once.Once);
            _il.Emit(OpCodes.Castclass, typeof(PCodeOnce));
            _il.Emit(OpCodes.Call, OnceStepMethod); // OnceStepResult (int)

            var run = _il.DefineLabel();
            var park = _il.DefineLabel();
            var done = _il.DefineLabel();
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, (int)IlGpmRuntime.OnceStepResult.Run);
            _il.Emit(OpCodes.Beq, run);
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, (int)IlGpmRuntime.OnceStepResult.Parked);
            _il.Emit(OpCodes.Beq, park);
            _il.Emit(OpCodes.Pop);
            // Skip: già eseguito → continua.
            EmitContinueTo(b.Terminator.ResumePc);
            _il.Emit(OpCodes.Br, done);

            // Park: un altro chiamante sta eseguendo DoInit → parcheggia (retry sul block).
            _il.MarkLabel(park);
            _il.Emit(OpCodes.Pop);
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.Emit(OpCodes.Br, done);

            // Run: questo chiamante esegue DoInit (flat) + Complete.
            _il.MarkLabel(run);
            _il.Emit(OpCodes.Pop);
            EmitExprObject(once.Once);
            _il.Emit(OpCodes.Castclass, typeof(PCodeOnce));
            _il.Emit(OpCodes.Ldc_I4, doInitId);
            _il.Emit(OpCodes.Call, RunOnceBodyMethod);
            EmitContinueTo(b.Terminator.ResumePc);

            _il.MarkLabel(done);
        }

        // ─── Accesso a locali/spill e espressioni ─────────────────────────────────────────
        private static readonly System.Reflection.MethodInfo ErrInstanceNumberGetterMethod =
            typeof(Basic.ErrInstance).GetProperty(nameof(Basic.ErrInstance.Number))!.GetGetMethod()!;
        private static readonly System.Reflection.MethodInfo ErrInstanceLineNumberGetterMethod =
            typeof(Basic.ErrInstance).GetProperty(nameof(Basic.ErrInstance.LineNumber))!.GetGetMethod()!;
        private static readonly System.Reflection.MethodInfo ErrInstanceDescriptionGetterMethod =
            typeof(Basic.ErrInstance).GetProperty(nameof(Basic.ErrInstance.Description))!.GetGetMethod()!;
        private static readonly System.Reflection.MethodInfo ErrInstanceSourceLineTextGetterMethod =
            typeof(Basic.ErrInstance).GetProperty(nameof(Basic.ErrInstance.SourceLineText))!.GetGetMethod()!;

        private int SlotOf(int symbolId)
            => _slotOf.TryGetValue(symbolId, out var s) ? s : throw new InvalidOperationException($"StepV2: variabile senza slot (sym {symbolId}).");

        private void LoadRawSlotObject(int slot)
        {
            if (_globalSlots.Contains(slot))
            {
                // GLOBAL: bag condiviso di processo (IlRuntime.ProgramGlobals), condiviso tra i task.
                _il.Emit(OpCodes.Ldc_I4, _symbolForSlot[slot]);
                _il.Emit(OpCodes.Call, IlRuntimeLoadGlobalMethod);
                return;
            }
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Ldfld, FrameLocalsField);
            _il.Emit(OpCodes.Ldc_I4, slot);
            _il.Emit(OpCodes.Ldelem_Ref);
        }

        private void LoadSlotObject(int slot)
        {
            LoadRawSlotObject(slot);
            if (_byRefSlots.Contains(slot))
                _il.Emit(OpCodes.Call, DerefRefMethod);
            if (_futureSlots.Contains(slot))
                _il.Emit(OpCodes.Call, DerefFutureMethod);
        }

        private LocalBuilder StoreTemp() => _storeTemp ??= _il.DeclareLocal(typeof(object));

        private void StoreSlotObject(int slot)
        {
            if (_byRefSlots.Contains(slot))
            {
                // BYREF: il valore corrente dello slot è una RefToVar → scrivi ATTRAVERSO la
                // cella (il chiamante vede la modifica). stind.ref vuole [addr, value].
                _il.Emit(OpCodes.Stloc, StoreTemp());
                LoadRawSlotObject(slot);              // [cell]
                _il.Emit(OpCodes.Ldloc, StoreTemp()); // [cell, value]
                _il.Emit(OpCodes.Call, StoreRefMethod);
                return;
            }
            if (_globalSlots.Contains(slot))
            {
                // GLOBAL: bag condiviso di processo (IlRuntime.ProgramGlobals) — condiviso tra i task.
                _il.Emit(OpCodes.Stloc, StoreTemp());
                _il.Emit(OpCodes.Ldc_I4, _symbolForSlot[slot]);
                _il.Emit(OpCodes.Ldloc, StoreTemp());
                _il.Emit(OpCodes.Call, IlRuntimeStoreGlobalMethod);
                return;
            }
            // stind.ref vuole [addr, value] (value in cima): il valore è già sullo stack,
            // lo si mette in un temp, si spinge l'indirizzo, poi il valore.
            _il.Emit(OpCodes.Stloc, StoreTemp());
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Ldfld, FrameLocalsField);
            _il.Emit(OpCodes.Ldc_I4, slot);
            _il.Emit(OpCodes.Ldelema, typeof(object));
            _il.Emit(OpCodes.Ldloc, StoreTemp());
            _il.Emit(OpCodes.Stind_Ref);
        }

        private void LoadSlotDouble(int slot)
        {
            LoadSlotObject(slot);
            _il.Emit(OpCodes.Call, AsDoubleMethod);
        }

        private void StoreSlotDouble(int slot)
        {
            // box del valore double, poi store come object.
            _il.Emit(OpCodes.Box, typeof(double));
            StoreSlotObject(slot);
        }

        private void EmitExprObject(Expr e)
        {
            switch (e)
            {
                case NumberExpr num:
                    _il.Emit(OpCodes.Ldc_R8, num.Value);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case StringExpr str:
                    _il.Emit(OpCodes.Ldstr, str.Value);
                    break;
                case BooleanExpr bl:
                    _il.Emit(OpCodes.Ldc_I4, bl.Value ? 1 : 0);
                    _il.Emit(OpCodes.Box, typeof(bool));
                    break;
                case ListLiteralExpr ll:
                    // Literal LIST → array di elementi boxed + BuildList (List<object>).
                    _il.Emit(OpCodes.Ldc_I4, ll.Elements.Length);
                    _il.Emit(OpCodes.Newarr, typeof(object));
                    for (var li = 0; li < ll.Elements.Length; li++)
                    {
                        _il.Emit(OpCodes.Dup);
                        _il.Emit(OpCodes.Ldc_I4, li);
                        EmitExprObject(ll.Elements[li]);
                        _il.Emit(OpCodes.Stelem_Ref);
                    }
                    _il.Emit(OpCodes.Call, BuildListMethod);
                    break;
                case VariableExpr v:
                    LoadSlotObject(SlotOf(v.SymbolId));
                    break;
                // Elemento di array in contesto OBJECT (es. `PRINT px(1)` in un metodo con
                // park: SPRITE GETPIXELS marca la procedura come may-park → StepV2). Prima
                // mancava e l'eccezione "ArrayAccessExpr non supportata (G2)" veniva
                // inghiottita, lasciando label definite e mai marcate: il metodo non
                // compilava ("Label 2 has not been marked"). Stessa semantica di
                // EmitExprDouble per gli array numerici, con il box del tipo elemento.
                case ArrayAccessExpr aa:
                {
                    var elem = ArrayElementType(aa.SymbolId);
                    LoadSlotObject(SlotOf(aa.SymbolId));
                    EmitExprDouble(aa.Indices[0]);
                    _il.Emit(OpCodes.Conv_I4);
                    EmitLoadArrayElementObject(elem);
                    break;
                }
                case IndexedAccessExpr ia:
                {
                    var elem = ia.Base is VariableExpr iav ? ArrayElementType(iav.SymbolId) : null;
                    EmitExprObject(ia.Base);
                    EmitExprDouble(ia.Indices[0]);
                    _il.Emit(OpCodes.Conv_I4);
                    EmitLoadArrayElementObject(elem);
                    break;
                }
                case GroupingExpr g:
                    EmitExprObject(g.Expression);
                    break;
                case UnaryExpr u:
                    EmitExprDouble(u);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BinaryExpr bin when bin.Op.Kind == TokenKind.Plus && IsStringConcat(bin):
                    // Concatena stringhe (parità flat: "+" su stringhe → String.Concat).
                    EmitExprObject(bin.Left);
                    EmitExprObject(bin.Right);
                    _il.Emit(OpCodes.Call, StringConcatMethod);
                    break;
                case BinaryExpr bin:
                    EmitBinaryDouble(bin);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Str:
                    // STR(n) (parità flat): conversione numero → stringa.
                    EmitExprDouble(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, StringHelpersStrMethod);
                    break;
                // G8-bis: handle di rete in contesto object — LOCALPORT/PORT numerici (box),
                // REMOTE stringa (nessun box).
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.LocalPort:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, LocalPortMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.RemotePort:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, PortMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.RemoteIp:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, RemoteMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.GoMaxProcs:
                    // SYSTEM.GOMAXPROCS in contesto object: box del risultato.
                    if (bfc.Args.Length > 0) EmitExprDouble(bfc.Args[0]);
                    else _il.Emit(OpCodes.Ldc_R8, 0.0);
                    _il.Emit(OpCodes.Call, GoMaxProcsMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                // G8-bis: NET.LOCALHOST() (string), NET.GETHOSTIP$ (string), LEN (numerico),
                // ABS (numerico) — veloci, non park (parità flat Builtins.cs :88/:145/:147/:282).
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Host:
                    _il.Emit(OpCodes.Call, IlRuntimeHostMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.GetHostIp:
                    EmitStringArg(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeGetHostIpMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Len:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeLenMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Abs:
                    EmitExprDouble(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeAbsMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Conn:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeConnMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Recv:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeNetRecvMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Peek && bfc.Args.Length == 2:
                    // NET.PEEK(handle, n) — stringa peek non consumante (parità flat EmitPeekCall).
                    EmitInt64Arg(bfc.Args[0]);
                    EmitInt32Arg(bfc.Args[1]);
                    _il.Emit(OpCodes.Call, IlRuntimeTcpPeekMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.ContextBackground:
                    _il.Emit(OpCodes.Call, ContextBackgroundMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.ContextWithCancel:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, ContextWithCancelMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.ContextCancel:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, ContextCancelMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.ContextDone:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, ContextDoneMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.ContextCancelled:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, ContextCancelledMethod);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                case BuiltinFunctionCallExpr bfc when IsNumericConversion(bfc.Name.Lexeme):
                    EmitExprDouble(bfc.Args[0]);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                // G8-bis: builtin NUMERICI (ATOMIC*/NUMGOROUTINE/NUMCPU) in contesto object
                // (EmitAssignment/EmitExprObject) → EmitExprDouble + box.
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind is
                    TokenKind.AtomicSwap or TokenKind.AtomicFetchValue or TokenKind.AtomicIncrement
                    or TokenKind.AtomicCompareSwap or TokenKind.NumGoroutine or TokenKind.NumCpu:
                    EmitExprDouble(bfc);
                    _il.Emit(OpCodes.Box, typeof(double));
                    break;
                // G8-bis: POOL/SYNCMAP builtin flat (parità PCodeVM) — chiamata d'istanza.
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.PoolGet:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodePool));
                    _il.Emit(OpCodes.Callvirt, PoolGetMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.SyncMapLoad:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                    EmitExprObject(bfc.Args[1]);
                    _il.Emit(OpCodes.Callvirt, ConcurrentMapLoadMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.SyncMapDelete:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                    EmitExprObject(bfc.Args[1]);
                    _il.Emit(OpCodes.Callvirt, ConcurrentMapDeleteMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.SyncMapLoadOrStore:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                    EmitExprObject(bfc.Args[1]);
                    EmitExprObject(bfc.Args[2]);
                    _il.Emit(OpCodes.Callvirt, ConcurrentMapLoadOrStoreMethod);
                    break;
                case HttpFetchExpr hf:
                    // G5: FETCH espressione → HttpFetchExprAsync(url, opts) ritorna il future;
                    // la lettura della variabile target fa DerefFuture (implicit-await lite).
                    EmitExprObject(hf.Url);
                    if (hf.Options is not null) EmitExprObject(hf.Options);
                    else _il.Emit(OpCodes.Ldnull);
                    _il.Emit(OpCodes.Call, HttpFetchExprAsyncMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.BlackboardWait:
                    // Strada C (F1): BLACKBOARDWAIT in goroutine → BlackboardWaitAsync(key, timeout)
                    // ritorna un future (l'attesa gira sul blocking pool); il read del target parka.
                    EmitStringArg(bfc.Args[0]);
                    if (bfc.Args.Length > 1) EmitExprDouble(bfc.Args[1]);
                    else _il.Emit(OpCodes.Ldc_R8, 10000.0);
                    _il.Emit(OpCodes.Call, BlackboardWaitAsyncMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.AgentSwarm:
                    // Strada C (F1): AGENTSWARM in goroutine → AgentSwarmAsync(n, prefix, task, cc).
                    EmitExprDouble(bfc.Args[0]);
                    EmitStringArg(bfc.Args[1]);
                    EmitStringArg(bfc.Args[2]);
                    if (bfc.Args.Length > 3) EmitExprDouble(bfc.Args[3]);
                    else _il.Emit(OpCodes.Ldc_R8, 8.0);
                    _il.Emit(OpCodes.Call, AgentSwarmAsyncMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.AiHookFire:
                    // Aggancio GPM: HOOKFIRE in goroutine → HookFireAsync(event, payload); il
                    // dispatch degli handler-agente gira sul blocking pool, il read del target parka.
                    EmitStringArg(bfc.Args[0]);
                    if (bfc.Args.Length > 1) EmitStringArg(bfc.Args[1]);
                    else _il.Emit(OpCodes.Ldstr, "");
                    _il.Emit(OpCodes.Call, HookFireAsyncMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.AiGoalTick:
                    // Aggancio GPM: GOALTICK in goroutine → GoalTickAsync().
                    _il.Emit(OpCodes.Call, GoalTickAsyncMethod);
                    break;
                case FieldAccessExpr fa when fa.Structure is VariableExpr errObj && IsErrVariable(errObj):
                    EmitErrFieldAccess(fa.Field.Lexeme, asObject: true);
                    break;
                case FieldAccessExpr fa:
                    if (TryEmitDynamicFieldAccess(fa)) break;
                    EmitFieldAccessInstance(fa);
                    var faFieldB = ResolveFieldBuilder(fa);
                    _il.Emit(OpCodes.Ldfld, faFieldB);
                    // i campi sono tipizzati (Int64 per LONG): box del tipo CLR del campo.
                    if (faFieldB.FieldType != typeof(object))
                        _il.Emit(OpCodes.Box, faFieldB.FieldType);
                    break;
                case NewExpr ne when ne.ParsedType is ClassType ct:
                    if (ne.ConstructorArgs is { Length: > 0 } || ne.InitialValues is { Length: > 0 })
                        throw new NotSupportedException("StepV2: NEW con args/inizializzatore non supportato (G4).");
                    if (_classConstructors is null || !_classConstructors.TryGetValue(ct.Definition.SymbolId, out var ctors))
                        throw new NotSupportedException($"StepV2: NEW della classe '{ct.Definition.OriginalName.Lexeme}' senza costruttori (G4).");
                    var ctor = ctors.FirstOrDefault(c => c.Parameters.Length == 0)?.Builder
                        ?? throw new NotSupportedException($"StepV2: NEW della classe '{ct.Definition.OriginalName.Lexeme}' senza ctor parameterless (G4).");
                    _il.Emit(OpCodes.Newobj, ctor);
                    break;
                default:
                    throw new NotSupportedException($"StepV2: espressione '{e.GetType().Name}' non supportata (G2).");
            }
        }

        /// <summary>Tipo dell'ELEMENTO di un array fisso: il simbolo può essere tipizzato
        /// ArrayType oppure direttamente col tipo elemento.</summary>
        private Basic.DataType? ArrayElementType(int symbolId)
        {
            if (_program is null) return null;
            var dt = _program.Symbols.GetSymbol(symbolId).DataType;
            return dt is ArrayType at ? at.ElementType : dt;
        }

        /// <summary>Carica l'elemento puntato da (array, indice) sullo stack come object,
        /// con il box del tipo elemento (parità flat/ArrayAccessExpr).</summary>
        private void EmitLoadArrayElementObject(Basic.DataType? elem)
        {
            if (elem is LongType)
            {
                _il.Emit(OpCodes.Ldelem_I8);
                _il.Emit(OpCodes.Box, typeof(long));
            }
            else if (elem is DoubleType)
            {
                _il.Emit(OpCodes.Ldelem_R8);
                _il.Emit(OpCodes.Box, typeof(double));
            }
            else if (elem is StringType)
            {
                _il.Emit(OpCodes.Ldelem_Ref);
            }
            else
            {
                _il.Emit(OpCodes.Ldelem_I4);
                _il.Emit(OpCodes.Box, typeof(int));
            }
        }

        /// <summary>Spinge l'istanza di un FieldAccessExpr della closure (__context.cattura):
        /// local nel chiamante (ldloc) o `this` dentro il metodo di closure.</summary>
        private void EmitFieldAccessInstance(FieldAccessExpr fa)
        {
            if (fa.Structure is not VariableExpr sve)
                throw new NotSupportedException("StepV2: FieldAccessExpr su struttura non-variabile non supportato (G4).");
            if (IsErrVariable(sve))
            {
                EmitLoadErrInstance();
                return;
            }
            if (_slotOf.TryGetValue(sve.SymbolId, out var slot)) LoadSlotObject(slot);
            else if (_isClosureMethod) _il.Emit(OpCodes.Ldarg_0);
            else throw new NotSupportedException($"StepV2: istanza '{sve.NameCleaned?.Lexeme ?? sve.NameSuffixed?.Lexeme}' senza slot (G4).");
        }

        /// <summary>ERR (pseudo-globale, ErrType) NON ha slot nel frame: vive nel bag globale
        /// runtime (stesso percorso delle globali, IlRuntime.LoadGlobal).</summary>
        private bool IsErrVariable(VariableExpr v)
            => _program is not null && _program.Symbols.GetSymbol(v.SymbolId).DataType is ErrType;

        private void EmitLoadErrInstance()
        {
            var errSym = _program!.Symbols.FindSymbolsByName("ERR").FirstOrDefault(s => s.DataType is ErrType)?.Id
                ?? throw new NotSupportedException("StepV2: simbolo ERR non trovato (StdLib).");
            _il.Emit(OpCodes.Ldc_I4, errSym);
            _il.Emit(OpCodes.Call, IlRuntimeLoadGlobalMethod);
            _il.Emit(OpCodes.Castclass, typeof(Basic.ErrInstance));
        }

        /// <summary>ERR.Number/LineNumber/Description/SourceLineText nel subset v2 — parità flat
        /// (getter tipizzati del record ErrInstance; mai JsonFieldGet).</summary>
        private void EmitErrFieldAccess(string field, bool asObject)
        {
            EmitLoadErrInstance();
            var isNumber = field.Equals("Number", StringComparison.OrdinalIgnoreCase);
            var isLine = field.Equals("LineNumber", StringComparison.OrdinalIgnoreCase);
            var isDesc = field.Equals("Description", StringComparison.OrdinalIgnoreCase);
            var isSrc = field.Equals("SourceLineText", StringComparison.OrdinalIgnoreCase);
            if (!isNumber && !isLine && !isDesc && !isSrc)
                throw new NotSupportedException($"StepV2: campo '{field}' non supportato su ERR (solo Number/Description/LineNumber/SourceLineText).");
            _il.Emit(OpCodes.Call, isNumber ? ErrInstanceNumberGetterMethod
                : isLine ? ErrInstanceLineNumberGetterMethod
                : isDesc ? ErrInstanceDescriptionGetterMethod
                : ErrInstanceSourceLineTextGetterMethod);
            if (isNumber || isLine)
            {
                _il.Emit(OpCodes.Conv_R8);
                if (asObject) _il.Emit(OpCodes.Box, typeof(double));
            }
        }

        /// <summary>G5: campo DINAMICO su tipi di risultato/opzioni (FetchResult.StatusCode,
        /// HttpOptions, JSON...) — stesso percorso JsonFieldGet del flat (campo per NOME via
        /// runtime, FieldSymbolId è sempre -1 su questi tipi). Ritorna true se gestito.</summary>
        private bool TryEmitDynamicFieldAccess(FieldAccessExpr fa)
        {
            if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                System.Console.Error.WriteLine($"[DYNFIELD] {fa.Field.Lexeme} struct={fa.Structure?.GetType().Name} program={_program is not null} symbol={(fa.Structure is VariableExpr sve0 ? sve0.SymbolId : -1)}");
            if (_program is null || fa.Structure is not VariableExpr sve) return false;
            var dt = _program.Symbols.GetSymbol(sve.SymbolId).DataType;
            if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                System.Console.Error.WriteLine($"[DYNFIELD] {fa.Field.Lexeme} sym={sve.SymbolId} dt={dt?.GetType().Name}");
            var dynamic = dt is FetchResultType or HttpRequestType or HttpResponseType
                or HttpOptionsType or HttpHeadersType or HttpConfigType or JsonType or AnyType
                or WebSocketType
                // Collezioni native (es. Recovered.Count su un LIST literal — reflection via
                // JsonFieldGet, come i FetchResult). Include le collezioni tipizzate.
                or ListType or LinkedListType or CircularLinkedListType or StackType or QueueType
                or HashSetType or BagType or CacheType or TrieType or TreeType or AVLTreeType or RedBlackTreeType
                or BPlusTreeType or BinaryHeapType or HashRingType or SparseSetType
                or BinaryIndexedTreeType or TimelineType or GraphType;
            if (!dynamic) return false;
            EmitFieldAccessInstance(fa);
            _il.Emit(OpCodes.Ldstr, fa.Field.Lexeme);
            _il.Emit(OpCodes.Call, JsonFieldGetMethod);
            return true;
        }

        private FieldBuilder ResolveFieldBuilder(FieldAccessExpr fa)
        {
            if (_classFields is not null && fa.FieldSymbolId >= 0 && _classFields.TryGetValue(fa.FieldSymbolId, out var fb)) return fb;
            // Il linker NON risolve il FieldSymbolId dei campi della closure (id = -1): risolvi
            // per NOME sulla classe dell'istanza (stesso schema di ResolveFieldTargetSymbolId).
            if (_program is not null && _classFields is not null && fa.Structure is VariableExpr sve
                && _program.Symbols.GetSymbol(sve.SymbolId).DataType is ClassType ct)
            {
                foreach (var m in ct.Definition.Members)
                    if (m is FieldMember fm
                        && fm.Declaration.NameCleaned.Lexeme.Equals(fa.Field.Lexeme, StringComparison.OrdinalIgnoreCase)
                        && _classFields.TryGetValue(fm.Declaration.SymbolId, out var fbByName))
                        return fbByName;
            }
            throw new NotSupportedException($"StepV2: campo '{fa.Field.Lexeme}' della closure non trovato (G4).");
        }

        private FieldBuilder ResolveFieldBuilder(FieldTarget ft)
        {
            if (_classFields is not null && ft.FieldSymbolId >= 0 && _classFields.TryGetValue(ft.FieldSymbolId, out var fb)) return fb;
            if (_program is not null && _classFields is not null && ft.Structure is VariableTarget fvt
                && _program.Symbols.GetSymbol(fvt.SymbolId).DataType is ClassType fct)
            {
                foreach (var m in fct.Definition.Members)
                    if (m is FieldMember fm
                        && fm.Declaration.NameCleaned.Lexeme.Equals(ft.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase)
                        && _classFields.TryGetValue(fm.Declaration.SymbolId, out var fbByName))
                        return fbByName;
            }
            throw new NotSupportedException($"StepV2: campo '{ft.OriginalToken.Lexeme}' della closure non trovato (G4).");
        }

        /// <summary>Valuta un'espressione producendo un double sullo stack CIL.</summary>
        private void EmitExprDouble(Expr e)
        {
            switch (e)
            {
                case NumberExpr num:
                    _il.Emit(OpCodes.Ldc_R8, num.Value);
                    break;
                case VariableExpr v:
                    LoadSlotDouble(SlotOf(v.SymbolId));
                    break;
                case IndexedAccessExpr ia:
                    // px(1) su array fisso 1D numerico (int[]/long[]/double[]): elemento → double.
                    EmitExprObject(ia.Base);
                    EmitExprDouble(ia.Indices[0]);
                    _il.Emit(OpCodes.Conv_I4);
                    if (ia.Base is VariableExpr iaBase && _program is not null
                        && _program.Symbols.GetSymbol(iaBase.SymbolId).DataType is LongType)
                    {
                        _il.Emit(OpCodes.Ldelem_I8);
                        _il.Emit(OpCodes.Conv_R8);
                    }
                    else
                    {
                        _il.Emit(OpCodes.Ldelem_I4);
                        _il.Emit(OpCodes.Conv_R8);
                    }
                    break;
                case ArrayAccessExpr aa:
                    // px(1) su array fisso 1D (variante ArrayAccessExpr del parser): elemento → double.
                    LoadSlotObject(SlotOf(aa.SymbolId));
                    EmitExprDouble(aa.Indices[0]);
                    _il.Emit(OpCodes.Conv_I4);
                    if (_program is not null
                        && _program.Symbols.GetSymbol(aa.SymbolId).DataType is LongType)
                    {
                        _il.Emit(OpCodes.Ldelem_I8);
                        _il.Emit(OpCodes.Conv_R8);
                    }
                    else
                    {
                        _il.Emit(OpCodes.Ldelem_I4);
                        _il.Emit(OpCodes.Conv_R8);
                    }
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.UBound:
                    // UBOUND(px) su array fisso: lunghezza (ldlen) → double.
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Ldlen);
                    _il.Emit(OpCodes.Conv_I4);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                case GroupingExpr g:
                    EmitExprDouble(g.Expression);
                    break;
                case UnaryExpr u:
                    // NOT è type-directed (parità flat EmitUnaryExpression): su BIT è
                    // logico (0→-1, altrimenti 0), sui numerici è complemento bitwise
                    // (~5 = -6). Prima QUALSIASI unary faceva `Neg` → `NOT flag` dava
                    // `-flag` (0→0, 1→-1): bug del percorso GPM/stepper (DoWhileThreadFlag).
                    if (u.Op.Kind == TokenKind.Plus) { EmitExprDouble(u.Right); break; }
                    if (u.Op.Kind == TokenKind.Not)
                    {
                        bool isBit = u.Right is VariableExpr nv
                            && _program.Symbols.GetSymbol(nv.SymbolId).DataType is BitType;
                        EmitExprDouble(u.Right);
                        if (isBit)
                        {
                            _il.Emit(OpCodes.Ldc_R8, 0.0);
                            _il.Emit(OpCodes.Ceq);
                            _il.Emit(OpCodes.Neg);
                            _il.Emit(OpCodes.Conv_R8);
                        }
                        else
                        {
                            _il.Emit(OpCodes.Conv_I8);
                            _il.Emit(OpCodes.Not);
                            _il.Emit(OpCodes.Conv_R8);
                        }
                        break;
                    }
                    EmitExprDouble(u.Right);
                    _il.Emit(OpCodes.Neg);
                    break;
                case FieldAccessExpr fa when fa.Structure is VariableExpr errNum && IsErrVariable(errNum):
                    EmitErrFieldAccess(fa.Field.Lexeme, asObject: false);
                    break;
                case FieldAccessExpr fa:
                    if (TryEmitDynamicFieldAccess(fa))
                    {
                        // Campo dinamico (FetchResult.StatusCode, Ex.Number, JSON...) boxed:
                        // converti a double (il confronto/uso numerico passa da qui).
                        _il.Emit(OpCodes.Call, AsDoubleMethod);
                        break;
                    }
                    EmitFieldAccessInstance(fa);
                    var faFld = ResolveFieldBuilder(fa);
                    _il.Emit(OpCodes.Ldfld, faFld);
                    if (faFld.FieldType != typeof(double))
                        _il.Emit(OpCodes.Conv_R8);
                    break;
                case BinaryExpr bin:
                    EmitBinaryDouble(bin);
                    break;
                case BuiltinFunctionCallExpr bfc when IsNumericConversion(bfc.Name.Lexeme):
                    EmitExprDouble(bfc.Args[0]);
                    break;
                // G8-bis: LOCALPORT/PORT (handle di rete, ritornano la porta come numero).
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.LocalPort:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, LocalPortMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.GoMaxProcs:
                    // SYSTEM.GOMAXPROCS(n): legge il P count (n ignorato in v2 inline).
                    if (bfc.Args.Length > 0) EmitExprDouble(bfc.Args[0]);
                    else _il.Emit(OpCodes.Ldc_R8, 0.0);
                    _il.Emit(OpCodes.Call, GoMaxProcsMethod);
                    break;
                // G8-bis: ABS/LEN in contesto numerico (parità flat: Abs(double)→double,
                // Len(object)→long → conv).
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Abs:
                    EmitExprDouble(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeAbsMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Len:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeLenMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Conn:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeConnMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Recv:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, IlRuntimeNetRecvMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.RemotePort:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, PortMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.ContextCancelled:
                    // CONTEXT.CANCELLED(ctx) in contesto numerico → double (0/1).
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, ContextCancelledMethod);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.ContextCancel:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Call, ContextCancelMethod);
                    break;
                // G8-bis: ATOMIC*/SYSTEM.NUMGOROUTINE/NUMCPU (parità PCodeVM).
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.AtomicSwap:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                    EmitInt64Arg(bfc.Args[1]);
                    _il.Emit(OpCodes.Callvirt, AtomicSwapMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.AtomicFetchValue:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                    _il.Emit(OpCodes.Callvirt, AtomicLoadMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.AtomicIncrement:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                    EmitInt64Arg(bfc.Args[1]);
                    _il.Emit(OpCodes.Callvirt, AtomicAddMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.AtomicCompareSwap:
                    EmitExprObject(bfc.Args[0]);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                    EmitInt64Arg(bfc.Args[1]);
                    EmitInt64Arg(bfc.Args[2]);
                    _il.Emit(OpCodes.Callvirt, AtomicCompareSwapMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.NumGoroutine:
                    EmitLdarg(_taskArg);
                    _il.Emit(OpCodes.Callvirt, SchedulerGetMethod);
                    _il.Emit(OpCodes.Callvirt, SchedulerTaskCountMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.NumCpu:
                    _il.Emit(OpCodes.Call, IlRuntimeEnvironmentProcessorCountMethod);
                    _il.Emit(OpCodes.Conv_R8);
                    break;
                default:
                    throw new NotSupportedException($"StepV2: espressione double '{e.GetType().Name}' non supportata (G2).");
            }
        }

        /// <summary>True se un `+` è concat di STRINGHE (parità flat): un operando letterale
        /// stringa, una variabile StringType, o un builtin che ritorna stringa (STR).</summary>
        private static bool IsStringOperand(Expr e, BasicProgram? program)
        {
            switch (e)
            {
                case StringExpr: return true;
                case VariableExpr v when program is not null:
                    return program.Symbols.GetSymbol(v.SymbolId).DataType is StringType;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Str: return true;
                case BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Stringify: return true;
                default: return false;
            }
        }

        private bool IsStringConcat(BinaryExpr bin)
            => IsStringOperand(bin.Left, _program) || IsStringOperand(bin.Right, _program);

        private void EmitBinaryDouble(BinaryExpr bin)
        {
            var isComparison = bin.Op.Kind is TokenKind.LessThan or TokenKind.LessEqual
                or TokenKind.GreaterThan or TokenKind.GreaterEqual
                or TokenKind.ExactlyEqual or TokenKind.NotEqual or TokenKind.Equal;
            if (isComparison)
            {
                // Confronto STRINGHE (es. IF resp$ = "" — parità flat String.Equals): il path
                // numerico sotto farebbe EmitExprDouble(StringExpr) → NotSupported a metà
                // emissione → StepV2 PARZIALE → "Label not marked" al Save (lezione 7f2ac888).
                if (IsStringOperand(bin.Left, _program) || IsStringOperand(bin.Right, _program))
                {
                    EmitExprObject(bin.Left);
                    EmitExprObject(bin.Right);
                    _il.Emit(OpCodes.Call, StringEqualsMethod); // bool
                    if (bin.Op.Kind == TokenKind.NotEqual)
                    {
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Ceq); // NOT equal
                    }
                    var strLt = _il.DefineLabel();
                    var strDone = _il.DefineLabel();
                    _il.Emit(OpCodes.Brtrue, strLt);
                    _il.Emit(OpCodes.Ldc_R8, 0.0);
                    _il.Emit(OpCodes.Br, strDone);
                    _il.MarkLabel(strLt);
                    // true canonico BASIC = -1 (non 1): il path FLAT ottiene lo stesso con
                    // OpCodes.Neg dopo il confronto (IlProgramCompiler.BuiltinsMath.cs:742).
                    // Con 1.0 i programmi GPM che stampano un confronto (x = a > b) davano 1
                    // su IL e -1 su P-Code/VM/C: divergenza di parita' silenziosa.
                    _il.Emit(OpCodes.Ldc_R8, -1.0);
                    _il.MarkLabel(strDone);
                    return;
                }
                EmitExprDouble(bin.Left);
                EmitExprDouble(bin.Right);
                var lt = _il.DefineLabel();
                var done = _il.DefineLabel();
                switch (bin.Op.Kind)
                {
                    case TokenKind.LessThan:
                        _il.Emit(OpCodes.Blt, lt); break;
                    case TokenKind.LessEqual:
                        _il.Emit(OpCodes.Ble, lt); break;
                    case TokenKind.GreaterThan:
                        _il.Emit(OpCodes.Bgt, lt); break;
                    case TokenKind.GreaterEqual:
                        _il.Emit(OpCodes.Bge, lt); break;
                    case TokenKind.ExactlyEqual or TokenKind.Equal:
                        _il.Emit(OpCodes.Beq, lt); break;
                    case TokenKind.NotEqual:
                        _il.Emit(OpCodes.Bne_Un, lt); break;
                }
                _il.Emit(OpCodes.Ldc_R8, 0.0);
                _il.Emit(OpCodes.Br, done);
                _il.MarkLabel(lt);
                // vedi ilcommento sul ramo stringa: true canonico = -1, non 1.
                _il.Emit(OpCodes.Ldc_R8, -1.0);
                _il.MarkLabel(done);
                return;
            }
            EmitExprDouble(bin.Left);
            EmitExprDouble(bin.Right);
            switch (bin.Op.Kind)
            {
                case TokenKind.Plus: _il.Emit(OpCodes.Add); break;
                case TokenKind.Minus: _il.Emit(OpCodes.Sub); break;
                case TokenKind.Multiply: _il.Emit(OpCodes.Mul); break;
                // Divisione/MOD: helper runtime con ERR 11 su divisore 0 — stessa semantica
                // del percorso lineare (IlRuntime.SafeDivide/SafeMod) e delle altre 3 gambe.
                case TokenKind.Divide:
                    { _il.Emit(OpCodes.Ldc_I4, bin.Op.Position.Line > 0 ? bin.Op.Position.Line : 0); _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.SafeDivide), new[] { typeof(double), typeof(double), typeof(int) })!); }
                    break;
                case TokenKind.Mod:
                    { _il.Emit(OpCodes.Ldc_I4, bin.Op.Position.Line > 0 ? bin.Op.Position.Line : 0); _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.SafeMod), new[] { typeof(double), typeof(double), typeof(int) })!); }
                    break;
                case TokenKind.IntegerDivide:
                    { _il.Emit(OpCodes.Ldc_I4, bin.Op.Position.Line > 0 ? bin.Op.Position.Line : 0); _il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.SafeIntDivide), new[] { typeof(double), typeof(double), typeof(int) })!); }
                    break;
                case TokenKind.And: _il.Emit(OpCodes.Mul); break; // AND logico su doppi: approssimato
                case TokenKind.Or: _il.Emit(OpCodes.Add); break;
                default: throw new NotSupportedException($"StepV2: operatore '{bin.Op.Lexeme}' non supportato (G2).");
            }
        }
}
