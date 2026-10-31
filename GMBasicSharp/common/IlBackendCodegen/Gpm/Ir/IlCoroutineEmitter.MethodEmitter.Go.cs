// FILE: IlBackendCodegen/Gpm/Ir/IlCoroutineEmitter.MethodEmitter.Go.cs
// PARTIAL — dominio COROUTINE/GO: GO SELECT / GO FUNCTION / GO PROCEDURE / expr-stmt.
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
        private void EmitGoSelect(IlCoroutineBlock b)
        {
            if (b.AstNode is not GoSelectStmt gs) throw new NotSupportedException("StepV2: Select senza GoSelectStmt.");
            var targets = b.Terminator.Targets;
            var n = gs.Cases.Length;
            var hasDefault = gs.DefaultCase is { Length: > 0 };

            var channelsLocal = _il.DeclareLocal(typeof(ChannelInfo[]));
            var isTimeoutLocal = _il.DeclareLocal(typeof(bool[]));
            var isSendLocal = _il.DeclareLocal(typeof(bool[]));
            var sendValuesLocal = _il.DeclareLocal(typeof(object[]));
            var timeoutsLocal = _il.DeclareLocal(typeof(long[]));
            var ctxLocal = _il.DeclareLocal(typeof(SelectContext));

            // channels[]
            _il.Emit(OpCodes.Ldc_I4, n);
            _il.Emit(OpCodes.Newarr, typeof(ChannelInfo));
            for (var i = 0; i < n; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                if (gs.Cases[i].CommunicationStmt is ReceiveStmt rcv)
                {
                    EmitExprObject(rcv.Channel);
                    _il.Emit(OpCodes.Castclass, typeof(ChannelInfo));
                }
                else if (gs.Cases[i].CommunicationStmt is SendStmt snd)
                {
                    EmitExprObject(snd.Channel);
                    _il.Emit(OpCodes.Castclass, typeof(ChannelInfo));
                }
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Stloc, channelsLocal);

            // isTimeout[]
            _il.Emit(OpCodes.Ldc_I4, n);
            _il.Emit(OpCodes.Newarr, typeof(bool));
            for (var i = 0; i < n; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                _il.Emit(OpCodes.Ldc_I4, gs.Cases[i].CommunicationStmt is TimeoutStmt ? 1 : 0);
                _il.Emit(OpCodes.Stelem_I1);
            }
            _il.Emit(OpCodes.Stloc, isTimeoutLocal);

            // isSend[]
            _il.Emit(OpCodes.Ldc_I4, n);
            _il.Emit(OpCodes.Newarr, typeof(bool));
            for (var i = 0; i < n; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                _il.Emit(OpCodes.Ldc_I4, gs.Cases[i].CommunicationStmt is SendStmt ? 1 : 0);
                _il.Emit(OpCodes.Stelem_I1);
            }
            _il.Emit(OpCodes.Stloc, isSendLocal);

            // sendValues[]
            _il.Emit(OpCodes.Ldc_I4, n);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var i = 0; i < n; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                if (gs.Cases[i].CommunicationStmt is SendStmt sndV)
                {
                    EmitExprObject(sndV.Value);
                }
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Stloc, sendValuesLocal);

            // timeoutsMs[]
            _il.Emit(OpCodes.Ldc_I4, n);
            _il.Emit(OpCodes.Newarr, typeof(long));
            for (var i = 0; i < n; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                if (gs.Cases[i].CommunicationStmt is TimeoutStmt ts)
                {
                    EmitExprDouble(ts.Duration);
                    _il.Emit(OpCodes.Conv_I8);
                }
                else _il.Emit(OpCodes.Ldc_I8, 0L);
                _il.Emit(OpCodes.Stelem_I8);
            }
            _il.Emit(OpCodes.Stloc, timeoutsLocal);

            // ctx: riusa frame.SelectCtx (persistito tra i retry — il wake azzera SleepUntilMs e
            // la deadline del TIMEOUT vive sul ctx) o creane uno nuovo. La creazione avviene in
            // codice MANAGED (IlGpmRuntime.GetOrCreateSelectContext), NON via `newobj` qui: il
            // newobj su un assembly caricato dinamicamente produceva un SelectContext col campo
            // `_channels` corrotto (crash GoSelect — stessa strategia della PCodeVM, che crea il
            // ctx nell'interprete).
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Call, GetOrCreateSelectContextMethod);
            _il.Emit(OpCodes.Stloc, ctxLocal);

            var caseIndexLocal = _il.DeclareLocal(typeof(int));
            var valueLocal = _il.DeclareLocal(typeof(object));
            var closedLocal = _il.DeclareLocal(typeof(bool));

            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Ldloc, channelsLocal);
            _il.Emit(OpCodes.Ldloc, isTimeoutLocal);
            _il.Emit(OpCodes.Ldloc, isSendLocal);
            _il.Emit(OpCodes.Ldloc, sendValuesLocal);
            _il.Emit(OpCodes.Ldloc, timeoutsLocal);
            _il.Emit(OpCodes.Ldloc, ctxLocal);
            _il.Emit(OpCodes.Ldloca, caseIndexLocal);
            _il.Emit(OpCodes.Ldloca, valueLocal);
            _il.Emit(OpCodes.Ldloca, closedLocal);
            _il.Emit(OpCodes.Call, TrySelectSetupMethod);

            var ready = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ready);

            // Non pronto: DEFAULT (se c'è) o park (retry sul select block).
            if (hasDefault)
            {
                // Anche il DEFAULT azzera il ctx (come il WIN): in un ciclo DEFAULT→SLEEP un
                // SEND concorrente può aver rivendicato il ctx (via TryDequeueActiveReceiver →
                // TrySetResult) senza che il select lo consumi (è andato al DEFAULT) → il ctx
                // resta vinto e i select successivi non possono reclamare (deadlock
                // GoRoutinesMultiplexing). Azzerando qui, il prossimo select crea un ctx nuovo.
                EmitLdarg(_frameArg);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Stfld, FrameSelectCtxField);
                EmitContinueTo(targets[targets.Length - 1]);
            }
            else
            {
                EmitSetPc(b.Pc);
                EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
                EmitReturnTransition();
            }

            // Pronto: switch(caseIndex) → label per case; ogni label memorizza il valore (se
            // RECEIVE) e salta al body del case.
            _il.MarkLabel(ready);
            // Un select VINTO non deve inquinare il SUCCESSIVO (es. due GO SELECT in un loop
            // FOR): il SelectContext riusato (frame.SelectCtx) ha già WinningCaseIndex set →
            // il receiver del select successivo non può reclamare il case (deadlock esposto dal
            // run-to-suspend in GoSelectNoSleep). Il ctx serve SOLO per i retry dello stesso
            // select (park → wake); al win lo si azzera → il prossimo select ne crea uno nuovo.
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Stfld, FrameSelectCtxField);
            var caseLabels = new Label[n];
            for (var i = 0; i < n; i++) caseLabels[i] = _il.DefineLabel();
            var invalid = _il.DefineLabel();
            _il.Emit(OpCodes.Ldloc, caseIndexLocal);
            _il.Emit(OpCodes.Switch, caseLabels);
            _il.Emit(OpCodes.Br, invalid);

            for (var i = 0; i < n; i++)
            {
                _il.MarkLabel(caseLabels[i]);
                if (gs.Cases[i].CommunicationStmt is ReceiveStmt winnerRcv && winnerRcv.Destination is VariableTarget wvt)
                {
                    _il.Emit(OpCodes.Ldloc, valueLocal);
                    StoreSlotObject(SlotOf(wvt.SymbolId));
                    if (winnerRcv.OkFlag is VariableTarget okVar)
                    {
                        var isClosed2 = _il.DefineLabel();
                        var done2 = _il.DefineLabel();
                        _il.Emit(OpCodes.Ldloc, valueLocal);
                        _il.Emit(OpCodes.Brfalse, isClosed2);
                        _il.Emit(OpCodes.Ldc_R8, 1.0);
                        _il.Emit(OpCodes.Br, done2);
                        _il.MarkLabel(isClosed2);
                        _il.Emit(OpCodes.Ldc_R8, 0.0);
                        _il.MarkLabel(done2);
                        StoreSlotDouble(SlotOf(okVar.SymbolId));
                    }
                }
                EmitContinueTo(targets[i]);
            }

            _il.MarkLabel(invalid);
            EmitTransition(IlStepKind.CompleteTask, 0, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
        }

        private static readonly ConstructorInfo RefToVarCtor =
            typeof(RefToVar).GetConstructor(new[] { typeof(object?[]), typeof(int) })!;
        private static readonly MethodInfo DerefRefMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.DerefRef))!;
        private static readonly MethodInfo DerefFutureMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.DerefFuture))!;
        private static readonly MethodInfo BuildListMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.BuildList))!;
        private static readonly MethodInfo HttpFetchExprAsyncMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.HttpFetchExprAsync))!;
        private static readonly MethodInfo BlackboardWaitAsyncMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.BlackboardWaitAsync))!;
        private static readonly MethodInfo AgentSwarmAsyncMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.AgentSwarmAsync))!;
        private static readonly MethodInfo HookFireAsyncMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.HookFireAsync))!;
        private static readonly MethodInfo GoalTickAsyncMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GoalTickAsync))!;
        private static readonly MethodInfo JsonFieldGetMethod =
            typeof(Basic.IlBackend.IlRuntime).GetMethod("JsonFieldGet")!;
        private static readonly MethodInfo StoreRefMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.StoreRef))!;
        private static readonly FieldInfo FrameSelectCtxField =
            typeof(IlActivationFrame).GetField(nameof(IlActivationFrame.SelectCtx))!;
        private static readonly MethodInfo GetOrCreateSelectContextMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GetOrCreateSelectContext))!;
        private static readonly MethodInfo TrySelectSetupMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TrySelectSetup))!;

        private void EmitGoFunctionSpawn(Assignment a, GoFunctionExpr gfe)
        {
            // GO FUNCTION Foo(...) INTO x (parità Go go func + future): SpawnGoFunction crea
            // il task + il PCodeFutureValue; x riceve il future (il read farà implicit-await).
            if (a.Target is not VariableTarget vt)
                throw new NotSupportedException("StepV2: GO FUNCTION INTO target non variabile.");
            var calleeDef = _procedureForId(gfe.SymbolId);
            var slot = SlotOf(vt.SymbolId);

            // Callee FLAT (non may-park): esegui Proc_<id> INLINE e salva il RISULTATO —
            // nessun future/v2 task (un future flat non risolverebbe mai: il read DerefFuture
            // leggeva un default; l'errore del callee non arrivava all'ON ERROR/TRY del lettore,
            // FutureErrorFramePropagation). Per i callee may-park resta il future + implicit-await.
            if (_mayPark is not { } mp || !mp.Contains(gfe.SymbolId))
            {
                _il.Emit(OpCodes.Ldc_I4, gfe.SymbolId);
                if (gfe.Args.Length > 0)
                {
                    _il.Emit(OpCodes.Ldc_I4, gfe.Args.Length);
                    _il.Emit(OpCodes.Newarr, typeof(object));
                    for (var i = 0; i < gfe.Args.Length; i++)
                    {
                        _il.Emit(OpCodes.Dup);
                        _il.Emit(OpCodes.Ldc_I4, i);
                        EmitExprObject(gfe.Args[i]);
                        _il.Emit(OpCodes.Stelem_Ref);
                    }
                }
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, CallFlatFunctionMethod);
                StoreSlotObject(slot);
                return;
            }

            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Callvirt, TaskGetScheduler);
            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Ldc_I4, gfe.SymbolId);

            // args object[] (BYREF → RefToVar)
            _il.Emit(OpCodes.Ldc_I4, gfe.Args.Length);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var i = 0; i < gfe.Args.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                bool byRef = calleeDef is { Parameters: { Length: > 0 } pars3 } && i < pars3.Length
                    && pars3[i].Flags.HasFlag(SymbolFlags.IsByRef);
                if (byRef && gfe.Args[i] is VariableExpr bv3)
                {
                    EmitLdarg(_frameArg);
                    _il.Emit(OpCodes.Ldfld, FrameLocalsField);
                    _il.Emit(OpCodes.Ldc_I4, SlotOf(bv3.SymbolId));
                    _il.Emit(OpCodes.Newobj, RefToVarCtor);
                }
                else EmitExprObject(gfe.Args[i]);
                _il.Emit(OpCodes.Stelem_Ref);
            }

            if (gfe.WaitGroup is { } wg) EmitExprObject(wg);
            else _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Call, SpawnGoFunctionMethod);

            StoreSlotObject(slot);
        }

        private static readonly MethodInfo SpawnGoFunctionMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.SpawnGoFunction),
                [typeof(PCodeScheduler), typeof(PCodeTask), typeof(int), typeof(object[]), typeof(PCodeWaitGroup)])!;
        private static readonly MethodInfo CallFlatFunctionMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.CallFlatFunction), [typeof(int), typeof(object[])])!;

        private void EmitGoProcedure(GoProcedureStmt gp)
        {
            if (gp.Call is not FunctionExpr fe)
                throw new NotSupportedException("StepV2: GO SUB con target non FunctionExpr (G2).");
            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Callvirt, TaskGetScheduler);
            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Ldc_I4, fe.SymbolId);
            var n = fe.Args.Length;
            _il.Emit(OpCodes.Ldc_I4, n);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var i = 0; i < n; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                EmitExprObject(fe.Args[i]);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            if (gp.WaitGroup is { } wg)
            {
                EmitExprObject(wg);
                _il.Emit(OpCodes.Castclass, typeof(PCodeWaitGroup));
            }
            else _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Call, SpawnGoSubMethod);
            _il.Emit(OpCodes.Pop);
        }

        private void EmitExpressionStmt(ExpressionStmt es)
        {
            if (es.Expression is FunctionExpr fe)
            {
                // Chiamata a una procedura FLAT (non may-park) da un corpo v2: invoca
                // Proc_<id> con gli argomenti boxed coerced (helper CallFlatSub). Le call a
                // procedure may-park passano dal lowering come terminator Call (suspendable).
                _il.Emit(OpCodes.Ldc_I4, fe.SymbolId);
                if (fe.Args.Length > 0)
                {
                    _il.Emit(OpCodes.Ldc_I4, fe.Args.Length);
                    _il.Emit(OpCodes.Newarr, typeof(object));
                    for (var i = 0; i < fe.Args.Length; i++)
                    {
                        _il.Emit(OpCodes.Dup);
                        _il.Emit(OpCodes.Ldc_I4, i);
                        EmitExprObject(fe.Args[i]);
                        _il.Emit(OpCodes.Stelem_Ref);
                    }
                }
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, CallFlatSubMethod);
                return;
            }
            if (es.Expression is BuiltinFunctionCallExpr bfc)
            {
                // G8-bis: builtin flat VOID (parità PCodeVM) — POOL.PUT / SYNCMAP.STORE / ATOMIC.STORE.
                switch (bfc.Name.Kind)
                {
                    case TokenKind.PoolPut:
                        EmitExprObject(bfc.Args[0]);
                        _il.Emit(OpCodes.Castclass, typeof(PCodePool));
                        EmitExprObject(bfc.Args[1]);
                        _il.Emit(OpCodes.Callvirt, PoolPutMethod);
                        break;
                    case TokenKind.SyncMapStore:
                        EmitExprObject(bfc.Args[0]);
                        _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                        EmitExprObject(bfc.Args[1]);
                        EmitExprObject(bfc.Args[2]);
                        _il.Emit(OpCodes.Callvirt, ConcurrentMapStoreMethod);
                        break;
                    case TokenKind.AtomicStore:
                        EmitExprObject(bfc.Args[0]);
                        _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                        EmitInt64Arg(bfc.Args[1]);
                        _il.Emit(OpCodes.Callvirt, AtomicStoreMethod);
                        break;
                    case TokenKind.ContextCancel:
                        // CONTEXT.CANCEL(ctx) come STATEMENT (parità Go context.Cancel()):
                        // invoca e scarta il risultato (double boxato).
                        EmitExprObject(bfc.Args[0]);
                        _il.Emit(OpCodes.Call, ContextCancelMethod);
                        _il.Emit(OpCodes.Pop);
                        break;
                    default:
                        // Builtin NON-void come statement: emette e scarta il risultato.
                        EmitExprObject(bfc);
                        _il.Emit(OpCodes.Pop);
                        break;
                }
                return;
            }
            throw new NotSupportedException($"StepV2: ExpressionStmt non supportato (G2).");
        }

        // ─── IF ────────────────────────────────────────────────────────────────────────────
}
