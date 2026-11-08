// FILE: IlBackendCodegen/Gpm/Ir/IlCoroutineEmitter.MethodEmitter.Park.cs
// PARTIAL — dominio COROUTINE/PARK+GO+CONTROL: park network/inkey/tcp, GO select/function/
// procedure, statement coroutine. Migrato da IlCoroutineEmitter.MethodEmitter.cs
// (2026-08-30 — split, file <= 1200 righe).

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
        private void EmitWaitKeyPark(int pc, int resumePc, Target? variable)
        {
            var keyLocal = _il.DeclareLocal(typeof(string));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Ldloca, keyLocal);
            _il.Emit(OpCodes.Call, TryWaitKeyParkMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(pc);
            EmitTransition(IlStepKind.Park, pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            if (variable is VariableTarget vt)
            {
                _il.Emit(OpCodes.Ldloc, keyLocal);
                StoreSlotObject(SlotOf(vt.SymbolId));
            }
            EmitContinueTo(resumePc);
        }

        /// <summary>READ (@GET … READ) come PARK per-tasto (parità col modello C gmb_read_run):
        /// per ogni campo @GET — render + attesa di un tasto (IlGpmRuntime.ReadFieldStep →
        /// WaitKeyStep, park cooperativo). WHEN .F. salta il campo; VALID .F. rifà il campo. Lo
        /// stato (campo corrente + buffer) vive nel frame e sopravvive al park.</summary>
        private void EmitReadPark(IlCoroutineBlock b)
        {
            var fields = b.ReadFields;
            /* Blocchi con WHEN o VALID: sequenziale via helper (un campo per volta):
             * il predicato vede i valori già confermati dei campi precedenti e il
             * ri-prompt VALID riguarda SOLO il campo rifiutato (il loop nativo non ha
             * un evaluator BASIC, quindi il predicato si valuta qui). */
            if (fields is not null && fields.Exists(x => x.When is not null || x.Valid is not null))
            {
                foreach (var f in fields)
                {
                    if (f.Variable is not VariableTarget vt) continue;
                    var isStr = _program.Symbols.GetSymbol(vt.SymbolId).DataType is StringType;
                    var againField = _il.DefineLabel();
                    _il.MarkLabel(againField);
                    EmitExprDouble(f.Row); _il.Emit(OpCodes.Conv_R8);
                    EmitExprDouble(f.Col); _il.Emit(OpCodes.Conv_R8);
                    if (f.ControlWidth is not null) { EmitExprDouble(f.ControlWidth); _il.Emit(OpCodes.Conv_R8); }
                    else _il.Emit(OpCodes.Ldc_R8, 10.0);
                    LoadSlotObject(SlotOf(vt.SymbolId));
                    _il.Emit(OpCodes.Ldstr, f.ControlType ?? "");
                    EmitExprDoubleOrZero(f.MinValue ?? f.RangeLo);
                    EmitExprDoubleOrZero(f.MaxValue ?? f.RangeHi);
                    EmitExprDoubleOrZero(f.StepValue);
                    if (f.When is not null)
                    {
                        EmitExprDouble(f.When);
                        _il.Emit(OpCodes.Conv_I4);
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Ceq);            /* 1 = predicato falso → skip */
                    }
                    else _il.Emit(OpCodes.Ldc_I4_0);
                    _il.Emit(OpCodes.Call, ReadSequentialFieldMethod);
                    if (isStr) StoreSlotObject(SlotOf(vt.SymbolId));
                    else { _il.Emit(OpCodes.Call, IlRuntimeToDoubleMethod); StoreSlotDouble(SlotOf(vt.SymbolId)); }
                    if (f.Valid is not null)
                    {
                        EmitExprDouble(f.Valid);
                        _il.Emit(OpCodes.Conv_I4);
                        _il.Emit(OpCodes.Ldc_I4_0);
                        _il.Emit(OpCodes.Ceq);
                        _il.Emit(OpCodes.Brtrue, againField);
                    }
                }
                EmitContinueTo(b.Terminator.ResumePc);
                return;
            }
            var again = _il.DefineLabel();
            _il.MarkLabel(again);
            // READ NATIVO (una sola implementazione): ri-prompt VALID = si azzera il contesto
            // e si riaggiungono i campi (il testo rifiutato torna come valore iniziale della
            // variabile). Senza reset i campi si duplicherebbero a ogni ri-prompt.
            _il.Emit(OpCodes.Call, ReadNativeResetMethod);
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Call, ReadBeginMethod);

            if (fields is not null)
            {
                for (var i = 0; i < fields.Count; i++)
                {
                    var f = fields[i];
                    if (f.Variable is not VariableTarget vt) continue;
                    var isStr = _program.Symbols.GetSymbol(vt.SymbolId).DataType is StringType;

                    EmitExprDouble(f.Row); _il.Emit(OpCodes.Conv_R8);
                    EmitExprDouble(f.Col); _il.Emit(OpCodes.Conv_R8);
                    if (f.ControlWidth is not null) { EmitExprDouble(f.ControlWidth); _il.Emit(OpCodes.Conv_R8); }
                    else _il.Emit(OpCodes.Ldc_R8, 10.0);
                    LoadSlotObject(SlotOf(vt.SymbolId));
                    _il.Emit(OpCodes.Call, EditInitialMethod);
                    _il.Emit(OpCodes.Ldstr, f.ControlType ?? "");
                    _il.Emit(OpCodes.Ldstr, "");   // PICTURE
                    _il.Emit(OpCodes.Ldstr, "");   // ITEMS
                    /* Min/Max/Step/Range sono OPZIONALI: con la clausola assente l'AST ha null
                     * e EmitExprDouble(null) andava in NRE, facendo abortire l'emissione StepV2
                     * a metà e lasciando label non marcate ("Label N has not been marked"). */
                    EmitExprDoubleOrZero(f.MinValue ?? f.RangeLo);
                    EmitExprDoubleOrZero(f.MaxValue ?? f.RangeHi);
                    EmitExprDoubleOrZero(f.StepValue);
                    _il.Emit(OpCodes.Call, ReadNativeAddMethod);
                    _il.Emit(OpCodes.Pop);
                    if (f.Message is not null) { EmitExprObject(f.Message); _il.Emit(OpCodes.Call, ReadNativeSetMessageMethod); }
                    if (f.Error is not null) { EmitExprObject(f.Error); _il.Emit(OpCodes.Call, ReadNativeSetErrorMethod); }
                }
            }

            _il.Emit(OpCodes.Call, ReadNativeRunMethod);
            _il.Emit(OpCodes.Pop);

            if (fields is not null)
            {
                for (var i = 0; i < fields.Count; i++)
                {
                    var f = fields[i];
                    if (f.Variable is not VariableTarget vt) continue;
                    var isStr = _program.Symbols.GetSymbol(vt.SymbolId).DataType is StringType;
                    _il.Emit(OpCodes.Ldc_I4, i);
                    _il.Emit(OpCodes.Call, ReadNativeValueMethod);
                    if (isStr) StoreSlotObject(SlotOf(vt.SymbolId));
                    else { _il.Emit(OpCodes.Call, IlRuntimeToDoubleMethod); StoreSlotDouble(SlotOf(vt.SymbolId)); }
                }
            }

            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Call, ReadEndMethod);
            /* VALID (Clipper): il predicato legge la variabile già assegnata; se falso si
             * ripropone il campo ripartendo dall'inizio del blocco (che azzera il contesto
             * READ e riaggiunge i campi), come gmb_read_valid_ok. */
            if (fields is not null)
                for (var i = 0; i < fields.Count; i++)
                {
                    var f = fields[i];
                    if (f.Valid is null || f.Variable is not VariableTarget) continue;
                    var okLbl = _il.DefineLabel();
                    EmitConditionBoolV2(f.Valid);
                    _il.Emit(OpCodes.Brtrue, okLbl);
                    _il.Emit(OpCodes.Br, again);
                    _il.MarkLabel(okLbl);
                }
            EmitContinueTo(b.Terminator.ResumePc);
        }

        /// <summary>INKEY senza timeout (G5): poll non bloccante, mai park — se c'è un tasto in
        /// coda lo consuma, altrimenti "" (parità flat TryInkeyPoll).</summary>
        private void EmitExprDoubleOrZero(Expr? e)
        {
            if (e is not null) { EmitExprDouble(e); _il.Emit(OpCodes.Conv_R8); }
            else _il.Emit(OpCodes.Ldc_R8, 0.0);
        }

        private void EmitInkeyPoll(Target variable)        {
            var keyLocal = _il.DeclareLocal(typeof(string));
            EmitLdarg(_taskArg);
            _il.Emit(OpCodes.Ldloca, keyLocal);
            _il.Emit(OpCodes.Call, TryInkeyPollMethod);
            _il.Emit(OpCodes.Pop);
            if (variable is VariableTarget vt)
            {
                _il.Emit(OpCodes.Ldloc, keyLocal);
                StoreSlotObject(SlotOf(vt.SymbolId));
            }
        }

        /// <summary>G5 rete: park di rete unificato (IlNetworkAwait su BeginWait/PCodeWaitRegistration,
        /// parità PCodeVM). Emette gli argomenti (dal lambda), chiama lo Step helper; su false
        /// parcheggia (retry stesso block), su true salva il risultato (handle) nel target.</summary>
        private void EmitNetworkPark(int pc, int resumePc, MethodInfo stepMethod, Action emitArgs, Target? handleTarget)
        {
            var valueLocal = _il.DeclareLocal(typeof(object));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            emitArgs();
            _il.Emit(OpCodes.Ldloca, valueLocal);
            _il.Emit(OpCodes.Call, stepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(pc);
            EmitTransition(IlStepKind.Park, pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            if (handleTarget is VariableTarget hvt)
            {
                _il.Emit(OpCodes.Ldloc, valueLocal);
                StoreSlotObject(SlotOf(hvt.SymbolId));
            }
            EmitContinueTo(resumePc);
        }

        /// <summary>TCP RECV (G5): park (TcpRecvStep) → buffer + bytesReceived (byte[] o stringa).</summary>
        private void EmitTcpRecvPark(IlCoroutineBlock b, TcpRecvStmt recv)
        {
            var valueLocal = _il.DeclareLocal(typeof(object));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitInt64Arg(recv.ClientHandle);
            _il.Emit(OpCodes.Ldloca, valueLocal);
            _il.Emit(OpCodes.Call, TcpRecvStepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            if (recv.Buffer is VariableTarget bt)
            {
                _il.Emit(OpCodes.Ldloc, valueLocal);
                if (_program is not null
                    && _program.Symbols.GetSymbol(bt.SymbolId).DataType is StringType)
                    _il.Emit(OpCodes.Call, NetworkDataToStringMethod);
                StoreSlotObject(SlotOf(bt.SymbolId));
            }
            if (recv.BytesReceived is VariableTarget nrt)
            {
                _il.Emit(OpCodes.Ldloc, valueLocal);
                _il.Emit(OpCodes.Call, BytesLengthMethod);
                StoreSlotDouble(SlotOf(nrt.SymbolId));
            }
            EmitContinueTo(b.Terminator.ResumePc);
        }

        /// <summary>UDP RECV (G5): park (UdpRecvStep) → object[]{data, host, port} nei target.</summary>
        private void EmitUdpRecvPark(IlCoroutineBlock b, UdpRecvStmt recv)
        {
            var valueLocal = _il.DeclareLocal(typeof(object));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            EmitInt64Arg(recv.Handle);
            _il.Emit(OpCodes.Ldloca, valueLocal);
            _il.Emit(OpCodes.Call, UdpRecvStepMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(b.Pc);
            EmitTransition(IlStepKind.Park, b.Pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitArrayGetStore(valueLocal, 0, recv.Buffer);
            EmitArrayGetStore(valueLocal, 1, recv.RemoteIp);
            EmitArrayGetStore(valueLocal, 2, recv.RemotePort);
            EmitContinueTo(b.Terminator.ResumePc);
        }

        private void EmitArrayGetStore(LocalBuilder valueLocal, int index, Target target)
        {
            if (target is not VariableTarget vt) return;
            _il.Emit(OpCodes.Ldloc, valueLocal);
            _il.Emit(OpCodes.Ldc_I4, index);
            _il.Emit(OpCodes.Call, ArrayGetMethod);
            StoreSlotObject(SlotOf(vt.SymbolId));
        }

        /// <summary>Argomento INT64 tipizzato (handle di rete LONG).</summary>
        private void EmitInt64Arg(Expr e)
        {
            EmitExprObject(e);
            _il.Emit(OpCodes.Call, AsDoubleMethod);
            _il.Emit(OpCodes.Conv_I8);
        }

        /// <summary>Park grafico VOID (SYNC/SCREEN): chiama Try*Park(task, args), park/retry.</summary>
        private void EmitGraphicsVoidPark(int pc, int resumePc, MethodInfo parkMethod, Action emitArgs)
        {
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            emitArgs();
            _il.Emit(OpCodes.Call, parkMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(pc);
            EmitTransition(IlStepKind.Park, pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            EmitContinueTo(resumePc);
        }

        /// <summary>Park grafico con risultato INT (POINT/LOAD TEXTURE/SPRITE LOAD): il valore
        /// (id/colore) va alla variabile target (o viene scartato).</summary>
        private void EmitGraphicsIntPark(int pc, int resumePc, MethodInfo parkMethod, Action emitArgs, int? targetSymbolId)
        {
            var intLocal = _il.DeclareLocal(typeof(int));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            emitArgs();
            _il.Emit(OpCodes.Ldloca, intLocal);
            _il.Emit(OpCodes.Call, parkMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(pc);
            EmitTransition(IlStepKind.Park, pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            if (targetSymbolId is int tid)
            {
                _il.Emit(OpCodes.Ldloc, intLocal);
                _il.Emit(OpCodes.Conv_R8);
                StoreSlotDouble(SlotOf(tid));
            }
            EmitContinueTo(resumePc);
        }

        /// <summary>Park grafico con risultato OBJECT (LOAD TEXTURE CELL → int[], SPRITE
        /// GETPIXELS → array): il risultato va nello slot del target.</summary>
        private void EmitGraphicsObjPark(int pc, int resumePc, MethodInfo parkMethod, Action emitArgs, int? targetSymbolId)
        {
            var objLocal = _il.DeclareLocal(typeof(object));
            EmitLdarg(_taskArg);
            EmitLdarg(_frameArg);
            emitArgs();
            _il.Emit(OpCodes.Ldloca, objLocal);
            _il.Emit(OpCodes.Call, parkMethod);
            var ok = _il.DefineLabel();
            _il.Emit(OpCodes.Brtrue, ok);
            EmitSetPc(pc);
            EmitTransition(IlStepKind.Park, pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
            _il.MarkLabel(ok);
            if (targetSymbolId is int tid2)
            {
                _il.Emit(OpCodes.Ldloc, objLocal);
                StoreSlotObject(SlotOf(tid2));
            }
            EmitContinueTo(resumePc);
        }

        /// <summary>Argomento STRING tipizzato (gli helper di rete vogliono string).</summary>
        private void EmitStringArg(Expr e)
        {
            EmitExprObject(e);
            _il.Emit(OpCodes.Call, AsStringMethod);
        }

        /// <summary>Argomento INT32 tipizzato (gli helper di rete vogliono int).</summary>
        private void EmitInt32Arg(Expr e)
        {
            EmitExprObject(e);
            _il.Emit(OpCodes.Call, AsDoubleMethod);
            _il.Emit(OpCodes.Conv_I4);
        }

        private void EmitAssignment(Assignment a)
        {
            if (a.Target is VariableTarget vt)
            {
                var slot = SlotOf(vt.SymbolId);
                EmitExprObject(a.Value);
                StoreSlotObject(slot);
                return;
            }
            // G4 closure: scrittura di un campo della closure (__context.cattura = valore).
            if (a.Target is FieldTarget ft && ft.Structure is VariableTarget fvt)
            {
                var fieldB = ResolveFieldBuilder(ft);
                // Istanze del context: local nel chiamante (ldloc) o `this` nel metodo di closure.
                if (_slotOf.TryGetValue(fvt.SymbolId, out var ctxSlot)) LoadSlotObject(ctxSlot);
                else if (_isClosureMethod) _il.Emit(OpCodes.Ldarg_0);
                else throw new NotSupportedException($"StepV2: istanza '{fvt.NameCleaned.Lexeme}' senza slot (G4).");
                EmitExprObject(a.Value);
                EmitCoerceFieldValue(fieldB);
                _il.Emit(OpCodes.Stfld, fieldB);
                return;
            }
            throw new NotSupportedException($"StepV2: target non variabile non supportato (G2).");
        }

        /// <summary>Converte il valore (boxed double/stringa) sul top dello stack al tipo CLR
        /// del campo della closure prima dello Stfld (i locali v2 sono object[] → i numeri sono
        /// double boxati, i campi LONG sono Int64).</summary>
        private void EmitCoerceFieldValue(FieldBuilder fieldB)
        {
            var ft = fieldB.FieldType;
            if (ft == typeof(double))
            {
                _il.Emit(OpCodes.Unbox_Any, typeof(double));
            }
            else if (ft == typeof(long))
            {
                _il.Emit(OpCodes.Unbox_Any, typeof(double));
                _il.Emit(OpCodes.Conv_I8);
            }
            else if (ft == typeof(int))
            {
                _il.Emit(OpCodes.Unbox_Any, typeof(double));
                _il.Emit(OpCodes.Conv_I4);
            }
            else if (ft == typeof(short))
            {
                _il.Emit(OpCodes.Unbox_Any, typeof(double));
                _il.Emit(OpCodes.Conv_I2);
            }
            else if (ft == typeof(bool))
            {
                _il.Emit(OpCodes.Unbox_Any, typeof(double));
                _il.Emit(OpCodes.Conv_I4);
            }
            else if (ft == typeof(string))
            {
                _il.Emit(OpCodes.Castclass, typeof(string));
            }
            else if (ft != typeof(object))
            {
                throw new NotSupportedException($"StepV2: campo closure di tipo CLR '{ft}' non supportato (G4).");
            }
        }

        private void EmitErrorStatement(ErrorStmt er)
        {
            // ERROR n → transizione Throw (il driver la gestisce: ON ERROR/TRY unwind). MAI
            // un throw grezzo: dentro una protected region (CIL try/catch del wrapInCilCatch
            // per i blocchi FINALLY/TRY) sarebbe IL invalido ("Label N has not been marked"
            // al write dell'assembly).
            _il.Emit(OpCodes.Ldc_I4, (int)IlStepKind.Throw);
            _il.Emit(OpCodes.Ldc_I4, 0); // resumePc
            _il.Emit(OpCodes.Ldc_I4, 0); // targetProcedure
            _il.Emit(OpCodes.Ldnull);    // arguments
            _il.Emit(OpCodes.Ldc_I4, (int)IlReturnDestination.Discard);
            _il.Emit(OpCodes.Ldnull);    // value
            EmitExprDouble(er.ErrorCode);
            _il.Emit(OpCodes.Conv_I4);
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Ldc_I4, 0);
            _il.Emit(OpCodes.Newobj, SourcePositionCtor);
            if (er.Description is { } desc) EmitExprObject(desc);
            else _il.Emit(OpCodes.Ldnull);
            _il.Emit(OpCodes.Ldnull); // SourceLine
            _il.Emit(OpCodes.Ldnull); // innerException
            _il.Emit(OpCodes.Newobj, BasicExceptionCtor);
            _il.Emit(OpCodes.Call, StepExFactoryMethod);
            EmitReturnTransition();
        }

        private void EmitThrowStatement(ThrowStmt stmt)
        {
            // THROW n → transizione Throw (stesso modello di ERROR): MAI throw grezzo dentro
            // una protected region (IL invalido). Valore numerico → BasicException col codice;
            // istanza CLASS → BasicException con UserExceptionInstance.
            _il.Emit(OpCodes.Ldc_I4, (int)IlStepKind.Throw);
            _il.Emit(OpCodes.Ldc_I4, 0); // resumePc
            _il.Emit(OpCodes.Ldc_I4, 0); // targetProcedure
            _il.Emit(OpCodes.Ldnull);    // arguments
            _il.Emit(OpCodes.Ldc_I4, (int)IlReturnDestination.Discard);
            _il.Emit(OpCodes.Ldnull);    // value
            if (stmt.Value is NewExpr)
            {
                // THROW new Class() → istanza utente (UserExceptionInstance).
                _il.Emit(OpCodes.Ldc_I4, (int)Basic.ErrorCode.UserDefined);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Newobj, SourcePositionCtor);
                _il.Emit(OpCodes.Ldnull); // message
                _il.Emit(OpCodes.Ldnull); // SourceLine
                _il.Emit(OpCodes.Ldnull); // innerException
                _il.Emit(OpCodes.Newobj, BasicExceptionCtor);
                _il.Emit(OpCodes.Call, BasicExceptionUserExceptionInstanceSetter);
                EmitExprObject(stmt.Value);
            }
            else
            {
                EmitExprDouble(stmt.Value);
                _il.Emit(OpCodes.Conv_I4);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Newobj, SourcePositionCtor);
                _il.Emit(OpCodes.Ldnull); // message
                _il.Emit(OpCodes.Ldnull); // SourceLine
                _il.Emit(OpCodes.Ldnull); // innerException
                _il.Emit(OpCodes.Newobj, BasicExceptionCtor);
            }
            _il.Emit(OpCodes.Call, StepExFactoryMethod);
            EmitReturnTransition();
        }


}
