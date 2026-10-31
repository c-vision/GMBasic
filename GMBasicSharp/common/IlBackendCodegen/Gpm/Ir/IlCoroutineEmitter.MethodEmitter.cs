// FILE: IlBackendCodegen/Gpm/Ir/IlCoroutineEmitter.MethodEmitter.cs
// PARTIAL — dominio COROUTINE/METHODEMITTER: emette lo StepV2 per procedura (dispatch
// su frame.Pc + label per block). Migrato da IlCoroutineEmitter.cs (2026-08-30 — split,
// file <= 1200 righe). Classe estratta da annidata a internal (stesso namespace).

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
    private static bool IsNumericConversion(string name)
        => name.ToUpperInvariant() is "CLNG" or "CINT" or "CDBL" or "CSNG" or "VAL";

    /// <summary>Scava le conversioni numeriche fino all'espressione interna (per trovare una
    /// FunctionExpr may-park avvolta in CLNG ecc.).</summary>
    private static Expr UnwrapConversions(Expr e)
    {
        while (e is BuiltinFunctionCallExpr { Args: { Length: 1 } } bf && IsNumericConversion(bf.Name.Lexeme))
            e = bf.Args[0];
        return e;
    }

    /// <summary>`DIM x AS &lt;collezione avanzata&gt;` (senza NEW, GPM/IR): crea l'istanza CommonLib
    /// (stesse classi di EmitNewAdvancedCollection del path lineare) e la lascia sullo stack.
    /// L'emitter IR gestiva solo Channel/WaitGroup/Mutex/... → le collezioni restavano null
    /// (NullReferenceException su ENQUEUE, Capacity 0 su programmi top-level — bug IL
    /// RINGBUFFER/GRAPH/BAG di CollectionsGraphMissing).</summary>
    private void EmitNewAdvancedCollectionIr(DataType dt)
    {
        switch (dt)
        {
            case SparseSetType:
                _il.Emit(OpCodes.Ldc_I4, 1_000_000); _il.Emit(OpCodes.Ldc_I4, 64);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.SparseSet).GetConstructor([typeof(int), typeof(int)])!);
                return;
            case RingBufferType:
                _il.Emit(OpCodes.Ldc_I4, 64);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.CircularQueue<object>).GetConstructor([typeof(int)])!);
                return;
            case HashRingType:
                _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Newarr, typeof(object)); _il.Emit(OpCodes.Ldc_I4_1);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.ConsistentHash<object>).GetConstructor([typeof(IEnumerable<object>), typeof(int)])!);
                return;
            case BPlusTreeType:
                _il.Emit(OpCodes.Ldc_I4_3);
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.BPlusTree<string, object?>).GetConstructor([typeof(int)])!);
                return;
            case TrieType:
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.Trie).GetConstructor(Type.EmptyTypes)!);
                return;
            case GraphType:
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.Graph<object>).GetConstructor(Type.EmptyTypes)!);
                return;
            case BinaryIndexedTreeType:
                _il.Emit(OpCodes.Ldc_I4_0); _il.Emit(OpCodes.Newarr, typeof(double));
                _il.Emit(OpCodes.Newobj, typeof(CommonLib.Collections.BinaryIndexedTree<double>).GetConstructor([typeof(double[])])!);
                return;
            case BagType:
                _il.Emit(OpCodes.Newobj, typeof(Dictionary<object, long>).GetConstructor(Type.EmptyTypes)!);
                return;
            default:
                // Tree/AVL/RedBlack/Timeline/BinaryHeap/Circular/Doubly → SortedDictionary
                // (come EmitNewAdvancedCollection). Cache è un caso a parte (ctor nullable).
                _il.Emit(OpCodes.Newobj, typeof(SortedDictionary<object, object?>).GetConstructor(Type.EmptyTypes)!);
                return;
        }
    }


        private static readonly MethodInfo StepFactoryMethod =
            typeof(IlStepTransitionFactory).GetMethod(nameof(IlStepTransitionFactory.Step))!;
        private static readonly MethodInfo StepExFactoryMethod =
            typeof(IlStepTransitionFactory).GetMethod(nameof(IlStepTransitionFactory.StepEx))!;
        private static readonly MethodInfo BasicExceptionUserExceptionInstanceSetter =
            typeof(Basic.BasicException).GetProperty(nameof(Basic.BasicException.UserExceptionInstance))!.GetSetMethod()!;
        private static readonly MethodInfo StepCallFactoryMethod =
            typeof(IlStepTransitionFactory).GetMethod(nameof(IlStepTransitionFactory.StepCall))!;
        private static readonly MethodInfo EnterTryRegionMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.EnterTryRegion))!;
        private static readonly MethodInfo ExitTryRegionMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ExitTryRegion))!;
        private static readonly MethodInfo CloseChannelMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.CloseChannel))!;
        private static readonly MethodInfo SetOnErrorMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.SetOnError))!;
        private static readonly MethodInfo PushDeferredMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.PushDeferred))!;
        private static readonly MethodInfo CallFlatSubMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.CallFlatSub), [typeof(int), typeof(object[])])!;
        private static readonly MethodInfo TryReadFutureParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TryReadFuturePark))!;
        private static readonly MethodInfo GraphicsFontMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsFont))!;
        private static readonly MethodInfo GraphicsColorMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsColor))!;
        private static readonly MethodInfo GraphicsLocateMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsLocate))!;
        private static readonly MethodInfo GraphicsLineMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsLine))!;
        private static readonly MethodInfo GraphicsCircleMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsCircle))!;
        private static readonly MethodInfo GraphicsPsetMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsPset))!;
        private static readonly MethodInfo GraphicsPresetMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsPreset))!;
        private static readonly MethodInfo GraphicsDrawMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsDraw))!;
        private static readonly MethodInfo GraphicsSaveScreenMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsSaveScreen))!;
        private static readonly MethodInfo GraphicsRestoreScreenMethod = typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.GraphicsRestoreScreen))!;
        private static readonly MethodInfo BindCatchVariableMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.BindCatchVariable))!;        private static readonly MethodInfo TrySendParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TrySendPark))!;
        private static readonly MethodInfo TryReceiveParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TryReceivePark))!;
        private static readonly MethodInfo SendStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.SendStep))!;
        private static readonly MethodInfo ReceiveStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReceiveStep))!;
        private static readonly MethodInfo WaitStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.WaitStep))!;
        private static readonly MethodInfo CondWaitStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.CondWaitStep))!;
        private static readonly MethodInfo ForEachChannelStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ForEachChannelStep))!;
        private static readonly MethodInfo AsDoubleMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.AsDouble))!;
        private static readonly MethodInfo AsStringMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.AsString))!;
        private static readonly MethodInfo IsTruthyMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.IsTruthy))!;
        private static readonly MethodInfo SpawnGoSubMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.SpawnGoSub),
                [typeof(PCodeScheduler), typeof(PCodeTask), typeof(int), typeof(object[]), typeof(PCodeWaitGroup)])!;
        private static readonly MethodInfo ShouldWaitMethod =
            typeof(PCodeWaitGroup).GetMethod(nameof(PCodeWaitGroup.ShouldWait))!;
        private static readonly MethodInfo DoneAndWakeMethod =
            typeof(PCodeWaitGroup).GetMethod(nameof(PCodeWaitGroup.DoneAndWake))!;
        private static readonly MethodInfo WaitGroupDoneMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.WaitGroupDone))!;
        private static readonly MethodInfo PrintValueMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.PrintValue))!;
        private static readonly MethodInfo PrintNewLineMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.PrintNewLine))!;
        // Stage per-statement (canone parità flat IL/PCode/VM-C/C99): abbraccia ogni PRINT/CPRINT
        // come fa IlProgramCompiler.EmitPrintStatement; il parziale si SCARTA su errore.
        private static readonly MethodInfo PrintStageBeginMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrintStageBegin))!;
        private static readonly MethodInfo PrintStageEndMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrintStageEnd), [typeof(bool)])!;

        private static readonly ConstructorInfo ChannelInfoCtor =
            typeof(ChannelInfo).GetConstructor(new[] { typeof(int) })!;
        private static readonly ConstructorInfo WaitGroupCtor =
            typeof(PCodeWaitGroup).GetConstructor(Type.EmptyTypes)!;
        private static readonly ConstructorInfo MutexCtor =
            typeof(PCodeMutex).GetConstructor(Type.EmptyTypes)!;
        private static readonly ConstructorInfo RwMutexCtor =
            typeof(PCodeRwMutex).GetConstructor(Type.EmptyTypes)!;
        private static readonly ConstructorInfo OnceCtor =
            typeof(PCodeOnce).GetConstructor(Type.EmptyTypes)!;
        private static readonly MethodInfo MutexTryLockMethod =
            typeof(PCodeMutex).GetMethod(nameof(PCodeMutex.TryLock))!;
        private static readonly MethodInfo RwMutexTryLockMethod =
            typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.TryLock))!;
        private static readonly MethodInfo RwMutexTryRLockMethod =
            typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.TryRLock))!;
        private static readonly MethodInfo RwMutexRUnlockMethod =
            typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.RUnlock))!;
        private static readonly MethodInfo RwMutexUnlockMethod =
            typeof(PCodeRwMutex).GetMethod(nameof(PCodeRwMutex.Unlock))!;
        private static readonly MethodInfo OnceStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.OnceStep))!;
        private static readonly MethodInfo RunOnceBodyMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.RunOnceBody))!;
        private static readonly MethodInfo MutexUnlockMethod =
            typeof(PCodeMutex).GetMethod(nameof(PCodeMutex.Unlock))!;
        private static readonly ConstructorInfo CondCtor =
            typeof(PCodeCond).GetConstructor(Type.EmptyTypes)!;
        private static readonly MethodInfo CondSignalMethod =
            typeof(PCodeCond).GetMethod(nameof(PCodeCond.Signal))!;
        private static readonly MethodInfo CondBroadcastMethod =
            typeof(PCodeCond).GetMethod(nameof(PCodeCond.Broadcast))!;
        private static readonly ConstructorInfo PoolCtor =
            typeof(PCodePool).GetConstructor(Type.EmptyTypes)!;
        private static readonly ConstructorInfo ConcurrentMapCtor =
            typeof(PCodeConcurrentMap).GetConstructor(Type.EmptyTypes)!;
        private static readonly ConstructorInfo AtomicCtor =
            typeof(PCodeAtomic).GetConstructor(Type.EmptyTypes)!;
        private static readonly MethodInfo PoolGetMethod =
            typeof(PCodePool).GetMethod(nameof(PCodePool.Get))!;
        private static readonly MethodInfo PoolPutMethod =
            typeof(PCodePool).GetMethod(nameof(PCodePool.Put))!;
        private static readonly MethodInfo ConcurrentMapStoreMethod =
            typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.Store))!;
        private static readonly MethodInfo ConcurrentMapLoadMethod =
            typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.Load))!;
        private static readonly MethodInfo ConcurrentMapDeleteMethod =
            typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.Delete))!;
        private static readonly MethodInfo ConcurrentMapLoadOrStoreMethod =
            typeof(PCodeConcurrentMap).GetMethod(nameof(PCodeConcurrentMap.LoadOrStore))!;
        private static readonly MethodInfo AtomicSwapMethod =
            typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Swap))!;
        private static readonly MethodInfo AtomicStoreMethod =
            typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Store))!;
        private static readonly MethodInfo AtomicLoadMethod =
            typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Load))!;
        private static readonly MethodInfo AtomicAddMethod =
            typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.Add))!;
        private static readonly MethodInfo AtomicCompareSwapMethod =
            typeof(PCodeAtomic).GetMethod(nameof(PCodeAtomic.CompareSwap))!;
        private static readonly MethodInfo SchedulerTaskCountMethod =
            typeof(PCodeScheduler).GetProperty(nameof(PCodeScheduler.TaskCount))!.GetMethod!;
        private static readonly MethodInfo SchedulerGetMethod =
            typeof(PCodeTask).GetProperty(nameof(PCodeTask.Scheduler))!.GetMethod!;
        private static readonly MethodInfo IlRuntimeEnvironmentProcessorCountMethod =
            typeof(Environment).GetProperty(nameof(Environment.ProcessorCount))!.GetMethod!;
        private static readonly MethodInfo IlRuntimeLoadGlobalMethod =
            typeof(Basic.IlBackend.IlRuntime).GetMethod(nameof(Basic.IlBackend.IlRuntime.LoadGlobal))!;
        private static readonly MethodInfo StringHelpersStrMethod =
            typeof(Basic.IlBackend.StringHelpers).GetMethod(nameof(Basic.IlBackend.StringHelpers.Str))!;
        private static readonly MethodInfo StringConcatMethod =
            typeof(string).GetMethod(nameof(string.Concat), [typeof(object), typeof(object)])!;
        private static readonly MethodInfo StringEqualsMethod =
            typeof(string).GetMethod(nameof(string.Equals), [typeof(string), typeof(string)])!;
        private static readonly MethodInfo IlRuntimeStoreGlobalMethod =
            typeof(Basic.IlBackend.IlRuntime).GetMethod(nameof(Basic.IlBackend.IlRuntime.StoreGlobal))!;

        private static readonly ConstructorInfo SourcePositionCtor =
            typeof(SourcePosition).GetConstructor(new[] { typeof(int), typeof(int), typeof(int) })!;
        private static readonly ConstructorInfo BasicExceptionCtor =
            typeof(BasicException).GetConstructor(new[] {
                typeof(ErrorCode), typeof(SourcePosition), typeof(string), typeof(SourceLine), typeof(Exception) })!;

        private static readonly MethodInfo EnsureFrameLocalsMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.EnsureFrameLocals))!;
        private static readonly FieldInfo FramePcField =
            typeof(IlActivationFrame).GetField("Pc")!;
        private static readonly FieldInfo FrameLocalsField =
            typeof(IlActivationFrame).GetField("Locals")!;
        private LocalBuilder? _storeTemp;
        private static readonly MethodInfo TaskGetScheduler =
            typeof(PCodeTask).GetMethod("get_Scheduler")!;
        private static readonly MethodInfo SleepGet =
            typeof(PCodeTask).GetMethod("get_SleepUntilMs")!;
        private static readonly MethodInfo SleepSet =
            typeof(PCodeTask).GetMethod("set_SleepUntilMs")!;
        private static readonly MethodInfo SleepStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.SleepStep))!;
        private static readonly MethodInfo TryWaitKeyParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.WaitKeyStep))!;
        private static readonly MethodInfo ReadNativeAddMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadNativeAdd))!;
        private static readonly MethodInfo ReadNativeRunMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadNativeRun))!;
        private static readonly MethodInfo ReadNativeValueMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadNativeValue))!;
        private static readonly MethodInfo ReadNativeSetMessageMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadNativeSetMessage))!;
        private static readonly MethodInfo ReadNativeSetErrorMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadNativeSetError))!;
        private static readonly MethodInfo ReadSequentialFieldMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadSequentialField))!;
        private static readonly MethodInfo ReadNativeResumeMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadNativeResume))!;

        private static readonly MethodInfo ReadBeginMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadBegin))!;
        private static readonly MethodInfo ReadEndMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadEnd))!;
        private static readonly MethodInfo ReadFieldIndexMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadFieldIndex))!;
        private static readonly MethodInfo ReadSetFieldIndexMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadSetFieldIndex))!;
        private static readonly MethodInfo ReadFieldValueMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadFieldValue))!;
        private static readonly MethodInfo ReadResetFieldMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadResetField))!;
        private static readonly MethodInfo ReadFieldStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ReadFieldStep))!;
        private static readonly MethodInfo EditInitialMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.EditInitial))!;
        private static readonly MethodInfo IlRuntimeToDoubleMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.ToDouble), new[] { typeof(object) })!;
        private static readonly MethodInfo TryInkeyPollMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TryInkeyPoll))!;
        private static readonly MethodInfo TcpConnectStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TcpConnectStep))!;
        private static readonly MethodInfo SslOpenStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.SslOpenStep))!;
        private static readonly MethodInfo SslConnectStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.SslConnectStep))!;
        private static readonly MethodInfo WsConnectStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.WsConnectStep))!;
        private static readonly MethodInfo TcpSendStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TcpSendStep))!;
        private static readonly MethodInfo TcpRecvStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TcpRecvStep))!;
        private static readonly MethodInfo TcpAcceptStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TcpAcceptStep))!;
        private static readonly MethodInfo TrySyncParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TrySyncPark))!;
        private static readonly MethodInfo TrySqlExecParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TrySqlExecPark))!;
        private static readonly MethodInfo TrySqlExecFnParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TrySqlExecFnPark))!;
        private static readonly MethodInfo TryScreenParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TryScreenPark))!;
        private static readonly MethodInfo TryPointParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TryPointPark))!;
        private static readonly MethodInfo TryLoadTextureParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TryLoadTexturePark))!;
        private static readonly MethodInfo TrySpriteLoadParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TrySpriteLoadPark))!;
        private static readonly MethodInfo TryLoadTextureSheetParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TryLoadTextureSheetPark))!;
        private static readonly MethodInfo TrySpriteGetPixelsParkMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TrySpriteGetPixelsPark))!;
        private static readonly MethodInfo UdpRecvStepMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.UdpRecvStep))!;
        private static readonly MethodInfo UdpOpenMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.UdpOpen))!;
        private static readonly MethodInfo TcpOpenMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TcpOpen))!;
        private static readonly MethodInfo TcpCloseMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.TcpClose))!;
        private static readonly MethodInfo SqlOpenMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.SqlOpen), new[] { typeof(string), typeof(string) })!;
        private static readonly MethodInfo SqlCloseMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.SqlClose), new[] { typeof(object) })!;
        private static readonly MethodInfo UdpSendMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.UdpSend))!;
        private static readonly MethodInfo UdpCloseMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.UdpClose))!;
        private static readonly MethodInfo TcpSetOptMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.TcpSetOpt), [typeof(long), typeof(string), typeof(string)])!;
        private static readonly MethodInfo TcpShutMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.TcpShut), [typeof(long), typeof(string)])!;
        private static readonly MethodInfo IlRuntimeConnMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.Conn), [typeof(object)])!;
        private static readonly MethodInfo IlRuntimeNetRecvMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.NetRecv), [typeof(object)])!;
        private static readonly MethodInfo IlRuntimeTcpPeekMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.TcpPeek), [typeof(long), typeof(int)])!;
        private static readonly MethodInfo IlRuntimeHostMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.Host))!;
        private static readonly MethodInfo IlRuntimeGetHostIpMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.GetHostIp), [typeof(string)])!;
        private static readonly MethodInfo IlRuntimeLenMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.Len), [typeof(object)])!;
        private static readonly MethodInfo IlRuntimeAbsMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.Abs), [typeof(double)])!;
        private static readonly MethodInfo LocalPortMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.LocalPort))!;
        private static readonly MethodInfo PortMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.Port))!;
        private static readonly MethodInfo RemoteMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.Remote))!;
        private static readonly MethodInfo GoMaxProcsMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.GoMaxProcs))!;
        private static readonly MethodInfo ContextBackgroundMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextBackground))!;
        private static readonly MethodInfo ContextWithCancelMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextWithCancel))!;
        private static readonly MethodInfo ContextCancelMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextCancel))!;
        private static readonly MethodInfo ContextDoneMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextDone))!;
        private static readonly MethodInfo ContextCancelledMethod =
            typeof(IlRuntime).GetMethod(nameof(IlRuntime.ContextCancelled))!;
        private static readonly MethodInfo ArrayGetMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.ArrayGet))!;
        private static readonly MethodInfo BytesLengthMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.BytesLength))!;
        private static readonly MethodInfo NetworkDataToStringMethod =
            typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.NetworkDataToString))!;

        private readonly FunctionDefinition _def;
        private readonly IlCoroutineIr _ir;
        private readonly TypeBuilder _programType;
        private readonly Func<int, FunctionDefinition?> _procedureForId;
        private readonly System.Collections.Immutable.ImmutableHashSet<int>? _mayPark;
        private readonly Dictionary<string, int>? _nameToId;
        private readonly HashSet<int> _byRefSlots = new();
        private readonly HashSet<int> _futureSlots = new();
        private readonly HashSet<int> _globalSlots = new();
        private readonly Dictionary<int, int> _symbolForSlot = new();
        private int _channelForEachSlot = -1;
        private LocalBuilder? _feValueLocal;
        private LocalBuilder? _feOkLocal;
        private ILGenerator _il = null!;
        private readonly Dictionary<int, int> _slotOf = new();
        private readonly Dictionary<string, int> _slotOfName = new(StringComparer.OrdinalIgnoreCase);
        private int _localCount;
        private LocalBuilder? _transitionLocal;
        private Label _exitLabel;

        // G4 closure: quando _closureType è impostato lo StepV2 è un metodo D'ISTANZA della
        // classe __ClosureCtx (this = __context): task/frame si spostano a arg 1/2.
        private readonly TypeBuilder? _closureType;
        private readonly bool _isClosureMethod;
        private readonly Dictionary<int, int>? _closureClassOf;
        private readonly Dictionary<int, FieldBuilder>? _classFields;
        private readonly Dictionary<int, List<IlProgramCompiler.ClassConstructorInfo>>? _classConstructors;
        private readonly BasicProgram? _program;
        private int _taskArg;
        private int _frameArg;

        public MethodEmitter(
            FunctionDefinition def, IlCoroutineIr ir, TypeBuilder programType,
            Func<int, FunctionDefinition?> procedureForId, Dictionary<string, int>? nameToId = null,
            TypeBuilder? closureType = null,
            Dictionary<int, List<IlProgramCompiler.ClassConstructorInfo>>? classConstructors = null,
            Dictionary<int, FieldBuilder>? classFields = null,
            Dictionary<int, int>? closureClassOf = null,
            BasicProgram? program = null,
            System.Collections.Immutable.ImmutableHashSet<int>? mayPark = null)
        {
            _def = def;
            _ir = ir;
            _programType = programType;
            _procedureForId = procedureForId;
            _nameToId = nameToId;
            _mayPark = mayPark;
            _isClosureMethod = closureType is not null;
            _closureType = closureType;
            _closureClassOf = closureClassOf;
            _classConstructors = classConstructors;
            _classFields = classFields;
            _program = program;
            _taskArg = _isClosureMethod ? 1 : 0;
            _frameArg = _isClosureMethod ? 2 : 1;
            BuildSlotMap();
            if (_def.Parameters is { Length: > 0 } pars)
                for (var i = 0; i < pars.Length; i++)
                    if (pars[i].Flags.HasFlag(SymbolFlags.IsByRef))
                        _byRefSlots.Add(i);
            // Slot che ricevono un GO FUNCTION ... INTO x → future: il read farà DerefFuture.
            // Anche HTTP FETCH espressione (G5): HttpFetchExprAsync ritorna un future, la
            // lettura dereferenzia (implicit-await lite, stesso modello del GO FUNCTION).
            foreach (var st in _def.Body)
            {
                if (st is LetStmt lf)
                    foreach (var a in lf.Assignments)
                        if (a.Target is VariableTarget fvt
                            && (a.Value is GoFunctionExpr
                                || UnwrapConversions(a.Value) is HttpFetchExpr
                                || MayParkAnalysis.IsTeamParkBuiltin(UnwrapConversions(a.Value))))
                            _futureSlots.Add(SlotOf(fvt.SymbolId));
                if (st is DeclarationStmt dcl)
                    foreach (var decl in dcl.Declarations)
                        if (decl.InitialValue is HttpFetchExpr
                            || MayParkAnalysis.IsTeamParkBuiltin(UnwrapConversions(decl.InitialValue)))
                            _futureSlots.Add(SlotOf(decl.SymbolId));
            }
        }

        private void EmitBlock(IlCoroutineBlock b)
        {
            // Le istruzioni IR (Await e StoreSpill) sono markers: l'emissione reale dipende
            // dal terminator e dall'AstNode. Await è già coperto dal caso Park.
            var inTryRegion = !b.RegionPath.IsEmpty && b.RegionPath.Inner.Kind == IlIrRegionKind.Try;
            var wrapInCilCatch = true; // il driver è l'unica autorità sulle eccezioni (ON ERROR/TRY):
                                       // ogni blocco converte un errore runtime in transizione Throw.
                                       // (Il `leave` di EmitReturnTransition richiede la protected region.)

            // Entry block del TRY (AstNode=TryCatchStmt + regione Try nel path): pusha la
            // regione sul frame. Try-end (AstNode=TryCatchStmt SENZA regione): la popa.
            if (b.AstNode is TryCatchStmt && inTryRegion)
            {
                var r = b.RegionPath.Inner;
                EmitLdarg(_frameArg);
                _il.Emit(OpCodes.Ldc_I4, r.HandlerPc);
                _il.Emit(OpCodes.Ldc_I4, r.FinallyPc);
                _il.Emit(OpCodes.Ldc_I4, r.EndPc);
                _il.Emit(OpCodes.Call, EnterTryRegionMethod);
            }
            else if (b.AstNode is TryCatchStmt)
            {
                EmitLdarg(_frameArg);
                _il.Emit(OpCodes.Call, ExitTryRegionMethod);
            }
            // Entry block di un CATCH (AstNode=CatchClause): bind della variabile del catch
            // (ex AS ERR / ex AS ClassType) dall'eccezione corrente (frame.Unwind.Exception).
            else if (b.AstNode is CatchClause { Variable: { } ccv } cc)
            {
                var isErr = ccv.DataType is ErrType;
                EmitLdarg(_frameArg);
                _il.Emit(OpCodes.Ldc_I4, SlotOf(ccv.SymbolId));
                _il.Emit(OpCodes.Ldc_I4, isErr ? 1 : 0);
                _il.Emit(OpCodes.Call, BindCatchVariableMethod);
            }

            if (wrapInCilCatch)
                _il.BeginExceptionBlock();

            switch (b.Terminator.Kind)
            {
                case IlIrTermKind.Branch:
                    EmitBranch(b);
                    break;
                case IlIrTermKind.Switch:
                    EmitSwitch(b);
                    break;
                case IlIrTermKind.Call:
                    EmitCall(b);
                    break;
                case IlIrTermKind.Park:
                    EmitPark(b);
                    break;
                case IlIrTermKind.Return:
                    if (b.AstNode is ReturnStmt { Value: { } rvExpr })
                    {
                        // FUNCTION: RETURN expr — il valore va nella transizione (il driver lo
                        // scrive nella destinazione decisa alla Call).
                        EmitTransitionExprValue(IlStepKind.Return, b.Terminator.ResumePc, 0, b.Terminator.ReturnDest, rvExpr);
                    }
                    else
                    {
                        EmitTransition(IlStepKind.Return, b.Terminator.ResumePc, 0, null, b.Terminator.ReturnDest, null);
                    }
                    EmitReturnTransition();
                    break;
                case IlIrTermKind.Gosub:
                    EmitTransition(IlStepKind.Gosub, b.Terminator.ResumePc, b.Terminator.TargetProc, null, IlReturnDestination.Discard, null);
                    EmitReturnTransition();
                    break;
                case IlIrTermKind.ReturnGosub:
                    EmitTransition(IlStepKind.ReturnGosub, 0, 0, null, IlReturnDestination.Discard, null);
                    EmitReturnTransition();
                    break;
                case IlIrTermKind.ContinueUnwind:
                    // fine CATCH/FINALLY: dopo il try (ResumePc); il driver decide se riprendere
                    // l'unwind o continuare normalmente.
                    EmitSetPc(b.Terminator.ResumePc);
                    EmitTransition(IlStepKind.ContinueUnwind, b.Terminator.ResumePc, 0, null, IlReturnDestination.Discard, null);
                    EmitReturnTransition();
                    break;
                case IlIrTermKind.GoExit:
                    EmitTransition(IlStepKind.GoExit, 0, 0, null, IlReturnDestination.Discard, null);
                    EmitReturnTransition();
                    break;
                case IlIrTermKind.DeferPush:
                    // DEFER <body>: registra il blocco deferito sul frame (PushDeferred) e
                    // continua al successore. Il body gira LIFO all'uscita (driver).
                    EmitLdarg(_frameArg);
                    _il.Emit(OpCodes.Ldc_I4, b.Terminator.TargetPc);
                    _il.Emit(OpCodes.Call, PushDeferredMethod);
                    EmitContinueTo(b.Terminator.ResumePc);
                    break;
                case IlIrTermKind.DeferDone:
                    // Fine del corpo deferito: transizione DeferDone → il driver popa il
                    // successivo deferred LIFO o ricompleta il Return/GoExit interrotto.
                    EmitTransition(IlStepKind.DeferDone, 0, 0, null, IlReturnDestination.Discard, null);
                    EmitReturnTransition();
                    break;
                case IlIrTermKind.OnError:
                    // ON ERROR GOTO/GOSUB <label> | GOTO 0: installa l'handler poi continua.
                    EmitLdarg(_frameArg);
                    _il.Emit(OpCodes.Ldc_I4, b.Terminator.TargetProc);
                    var isGosub = (b.AstNode as OnErrorStmt)?.Keyword?.Kind == TokenKind.Gosub;
                    _il.Emit(OpCodes.Ldc_I4, isGosub ? 1 : 0);
                    _il.Emit(OpCodes.Call, SetOnErrorMethod);
                    EmitContinueTo(b.Terminator.ResumePc);
                    break;
                case IlIrTermKind.Resume:
                    // RESUME [NEXT]: il driver riprende da ErrorLinePc (+1 se NEXT).
                    EmitTransition(IlStepKind.Resume, b.Terminator.ResumePc, 0, null, IlReturnDestination.Discard, null);
                    EmitReturnTransition();
                    break;
                case IlIrTermKind.Select:
                    EmitGoSelect(b);
                    break;
                case IlIrTermKind.CompleteTask:
                    EmitTransition(IlStepKind.CompleteTask, 0, 0, null, IlReturnDestination.Discard, null);
                    EmitReturnTransition();
                    break;
                default:
                    throw new NotSupportedException($"StepV2: terminator {b.Terminator.Kind} non supportato (G2).");
            }

            if (wrapInCilCatch)
            {
                // Catch CIL: un'eccezione nel blocco → transizione Throw con l'eccezione.
                _il.BeginCatchBlock(typeof(Exception));
                var exLocal = _il.DeclareLocal(typeof(Exception));
                _il.Emit(OpCodes.Stloc, exLocal);
                _il.Emit(OpCodes.Ldc_I4, (int)IlStepKind.Throw);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldc_I4, 0);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldc_I4, (int)IlReturnDestination.Discard);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Ldloc, exLocal);
                _il.Emit(OpCodes.Call, StepExFactoryMethod);
                EmitReturnTransition();
                _il.EndExceptionBlock();
            }
            // Ogni blocco v2 è avvolto in una regione protetta CIL (wrapInCilCatch). Il label
            // di FINE della regione (markato da EndExceptionBlock) coincide col byte dove il
            // BLOCCO SUCCESSIVO inizia una NUOVA regione: il `leave` implicito verso quel
            // label atterrerebbe DENTRO la regione successiva → IL invalido (InvalidProgramException
            // "PCodeGpmAtomic/SuspendableFinally"). Un nop di separazione sposta il confine del
            // blocco successivo di un byte: il leave punta al nop (fuori da ogni regione).
            if (wrapInCilCatch)
                _il.Emit(OpCodes.Nop);
        }

        private void EmitBranch(IlCoroutineBlock b)
        {
            // Statement flat (AstNode) valutato inline, poi salto al target.
            var stmt = b.AstNode as Stmt;
            if (stmt is not null && stmt is not ExitStmt and not LabelStmt and not EnterLoopStmt and not ExitLoopStmt and not TryCatchStmt and not LocalProcedureStmt and not CatchClause)
            {
                if (stmt is NextStmt && b.Aux is ForStmt nextFor)
                {
                    // NEXT I: I = I + Step (default 1), poi loop-back al check.
                    var slot = SlotOf(nextFor.Variable.SymbolId);
                    LoadSlotDouble(slot);
                    if (nextFor.Step is { } step) EmitExprDouble(step);
                    else _il.Emit(OpCodes.Ldc_R8, 1.0);
                    _il.Emit(OpCodes.Add);
                    StoreSlotDouble(slot);
                }
                else if (stmt is ForStmt forInit)
                {
                    // Init del FOR: I = Start; end/step sono valutati dal check/next (Aux).
                    EmitExprObject(forInit.Start);
                    StoreSlotObject(SlotOf(forInit.Variable.SymbolId));
                }
                else
                {
                    EmitStatement(stmt, b);
                    if (stmt is EndStmt)
                    {
                        EmitTransition(IlStepKind.CompleteTask, 0, 0, null, IlReturnDestination.Discard, null);
                        EmitReturnTransition();
                        return;
                    }
                }
            }
            // G8: statement FLAT fusi dal post-pass (ExtraStatements) — eseguiti in sequenza
            // prima del Branch finale. Sono sempre statement "semplici" (il fusion esclude
            // NextStmt/ForStmt/EndStmt/LabelStmt), quindi EmitStatement li gestisce.
            if (b.ExtraStatements is { } extras)
                foreach (var es in extras)
                    EmitStatement(es, b);
            var target = b.Terminator.TargetPc;
            if (target == StacklessLowering.EndPc)
            {
                // Fine procedura: Return (il driver la tratta alla root come completion).
                // FUNCTION senza RETURN esplicito: il valore sta nel return-slot
                // (assegnazione al nome della funzione, es. `FortyTwo = 42` — il linker la
                // risolve al simbolo della procedura; i locali v2 sono object boxati come
                // da EmitAssignment). Senza slot (mai assegnato) o SUB: null come prima
                // (bug 2026-10-01).
                if (_def.ReturnType is not VoidType
                    && _slotOf.TryGetValue(_def.SymbolId, out var retSlot))
                    EmitTransitionSlotValue(IlStepKind.Return, 0, 0, IlReturnDestination.Discard, retSlot);
                else
                    EmitTransition(IlStepKind.Return, 0, 0, null, IlReturnDestination.Discard, null);
                EmitReturnTransition();
                return;
            }
            EmitContinueTo(target);
        }

        private void EmitSwitch(IlCoroutineBlock b)
        {
            var stmt = b.AstNode;
            switch (stmt)
            {
                case IfStmt ifs:
                    EmitIf(b, ifs);
                    break;
                case SelectStmt sel:
                    EmitSelect(b, sel);
                    break;
                case ForCheckAndJumpStmt check when b.Aux is ForStmt forStmt:
                    EmitForCheck(b, forStmt);
                    break;
                case ConditionalJumpStmt cj:
                    EmitCondJump(b, cj);
                    break;
                default:
                    throw new NotSupportedException($"StepV2: switch non supportato per {stmt?.GetType().Name ?? "null"} (G2).");
            }
        }

        private void EmitContinueTo(int pc)
        {
            EmitSetPc(pc);
            EmitTransition(IlStepKind.Continue, pc, 0, null, IlReturnDestination.Discard, null);
            EmitReturnTransition();
        }

        private void EmitSetPc(int pc)
        {
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Ldc_I4, pc);
            _il.Emit(OpCodes.Stfld, FramePcField);
        }

        /// <summary>Costruisce una IlStepTransition tramite la factory e la lascia sullo stack.</summary>
        private void EmitTransition(IlStepKind kind, int resumePc, int targetProc, object?[]? args, IlReturnDestination dest, object? value)
        {
            _il.Emit(OpCodes.Ldc_I4, (int)kind);
            _il.Emit(OpCodes.Ldc_I4, resumePc);
            _il.Emit(OpCodes.Ldc_I4, targetProc);
            EmitArgsArray(args);
            _il.Emit(OpCodes.Ldc_I4, (int)dest);
            EmitValue(value);
            _il.Emit(OpCodes.Ldc_I4, 0); // targetLocalCount (G4: call con argomenti)
            _il.Emit(OpCodes.Call, StepFactoryMethod);
        }

        /// <summary>Come EmitTransition ma con il value valutato INLINE (espressione RETURN).</summary>
        private void EmitTransitionExprValue(IlStepKind kind, int resumePc, int targetProc, IlReturnDestination dest, Expr valueExpr)
        {
            _il.Emit(OpCodes.Ldc_I4, (int)kind);
            _il.Emit(OpCodes.Ldc_I4, resumePc);
            _il.Emit(OpCodes.Ldc_I4, targetProc);
            _il.Emit(OpCodes.Ldnull); // args
            _il.Emit(OpCodes.Ldc_I4, (int)dest);
            EmitExprObject(valueExpr);
            _il.Emit(OpCodes.Ldc_I4, 0); // targetLocalCount
            _il.Emit(OpCodes.Call, StepFactoryMethod);
        }

        /// <summary>Come sopra ma il value è letto dallo slot frame indicato
        /// (return-slot implicito: LoadSlotObject al posto giusto nello stack).</summary>
        private void EmitTransitionSlotValue(IlStepKind kind, int resumePc, int targetProc, IlReturnDestination dest, int slot)
        {
            _il.Emit(OpCodes.Ldc_I4, (int)kind);
            _il.Emit(OpCodes.Ldc_I4, resumePc);
            _il.Emit(OpCodes.Ldc_I4, targetProc);
            _il.Emit(OpCodes.Ldnull); // args
            _il.Emit(OpCodes.Ldc_I4, (int)dest);
            LoadSlotObject(slot);
            _il.Emit(OpCodes.Ldc_I4, 0); // targetLocalCount
            _il.Emit(OpCodes.Call, StepFactoryMethod);
        }

        private void EmitArgsArray(object?[]? args)
        {
            if (args is null) { _il.Emit(OpCodes.Ldnull); return; }
            _il.Emit(OpCodes.Ldc_I4, args.Length);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var i = 0; i < args.Length; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                EmitValue(args[i]);
                _il.Emit(OpCodes.Stelem_Ref);
            }
        }

        private void EmitValue(object? value)
        {
            if (value is null) { _il.Emit(OpCodes.Ldnull); return; }
            if (value is double d) { _il.Emit(OpCodes.Ldc_R8, d); _il.Emit(OpCodes.Box, typeof(double)); return; }
            if (value is string s) { _il.Emit(OpCodes.Ldstr, s); return; }
            throw new NotSupportedException($"StepV2: valore transizione non supportato: {value.GetType().Name}");
        }

        // ─── Statement flat ────────────────────────────────────────────────────────────────

        private void EmitStatement(Stmt stmt, IlCoroutineBlock b)
        {
            switch (stmt)
            {
                case ForEachInitStmt fei:
                    // FOR EACH su canale: il canale è già nello slot del suo simbolo; registra
                    // lo slot per l'Advance (park). L'array foreach non è nel subset v2.
                    if (fei.CollectionExpr is VariableExpr fce
                        && _program is not null
                        && _program.Symbols.GetSymbol(fce.SymbolId).DataType is ChannelType)
                        _channelForEachSlot = SlotOf(fce.SymbolId);
                    break;
                case LetStmt let:
                    foreach (var a in let.Assignments)
                    {
                        if (a.Value is GoFunctionExpr gfe) EmitGoFunctionSpawn(a, gfe);
                        else EmitAssignment(a);
                    }
                    break;
                case DeclarationStmt declStmt:
                    // DIM: crea l'oggetto runtime per i tipi GPM (canale/waitgroup) nello
                    // slot del frame; gli altri tipi partono da null (0 per i numeri).
                    foreach (var decl in declStmt.Declarations)
                    {
                        if (decl.Dimensions.Length > 0)
                        {
                            // Array FISSO 1D (es. DIM px(10) AS INTEGER — pattern grafica in
                            // goroutine): newarr del tipo elemento CLR + store nello slot.
                            // Il supporto è minimale: lettura via IndexedAccessExpr + UBOUND.
                            if (decl.Dimensions.Length == 1)
                            {
                                var elType = decl.DataType is ArrayType arrT ? arrT.ElementType : decl.DataType;
                                var elClr = elType switch
                                {
                                    IntegerType => typeof(int),
                                    LongType => typeof(long),
                                    DoubleType => typeof(double),
                                    _ => null
                                };
                                if (elClr is not null)
                                {
                                    // DIM px(0 TO 10): size = upper - lower + 1; 0-based → upper+1.
                                    EmitExprDouble(decl.Dimensions[0]);
                                    _il.Emit(OpCodes.Conv_I4);
                                    if (decl.LowerBounds is { Length: 1 } lb)
                                    {
                                        EmitExprDouble(lb[0]);
                                        _il.Emit(OpCodes.Conv_I4);
                                        _il.Emit(OpCodes.Sub);
                                        _il.Emit(OpCodes.Ldc_I4_1);
                                        _il.Emit(OpCodes.Add);
                                    }
                                    else
                                    {
                                        _il.Emit(OpCodes.Ldc_I4_1);
                                        _il.Emit(OpCodes.Add);
                                    }
                                    _il.Emit(OpCodes.Newarr, elClr);
                                    StoreSlotObject(SlotOf(decl.SymbolId));
                                    continue;
                                }
                            }
                            continue; // array multidimensionale/oggetto: fuori subset
                        }
                        var slot = SlotOf(decl.SymbolId);
                        switch (decl.DataType)
                        {
                            case ChannelType ct:
                                // CAPACITY (parità Go make(chan T, n)): 0 → default 10 (ChannelInfo).
                                _il.Emit(OpCodes.Ldc_I4, ct.Capacity);
                                _il.Emit(OpCodes.Newobj, ChannelInfoCtor);
                                StoreSlotObject(slot);
                                break;
                            case WaitGroupType:
                                _il.Emit(OpCodes.Newobj, WaitGroupCtor);
                                StoreSlotObject(slot);
                                break;
                            case MutexType:
                                _il.Emit(OpCodes.Newobj, MutexCtor);
                                StoreSlotObject(slot);
                                break;
                            case RwMutexType:
                                _il.Emit(OpCodes.Newobj, RwMutexCtor);
                                StoreSlotObject(slot);
                                break;
                            case OnceType:
                                _il.Emit(OpCodes.Newobj, OnceCtor);
                                StoreSlotObject(slot);
                                break;
                            case CondType:
                                _il.Emit(OpCodes.Newobj, CondCtor);
                                StoreSlotObject(slot);
                                break;
                            case PoolType:
                                _il.Emit(OpCodes.Newobj, PoolCtor);
                                StoreSlotObject(slot);
                                break;
                            case ConcurrentMapType:
                                _il.Emit(OpCodes.Newobj, ConcurrentMapCtor);
                                StoreSlotObject(slot);
                                break;
                            case AtomicType:
                                _il.Emit(OpCodes.Newobj, AtomicCtor);
                                StoreSlotObject(slot);
                                break;
                            // Collezioni avanzate (senza NEW): crea l'istanza CommonLib (prima
                            // restava null → NPE/0 su programmi top-level).
                            case SparseSetType or RingBufferType or HashRingType or BPlusTreeType
                                or TrieType or GraphType or BinaryIndexedTreeType or BagType
                                or TreeType or AVLTreeType or RedBlackTreeType or BinaryHeapType
                                or CircularLinkedListType or DoublyLinkedListType:
                                EmitNewAdvancedCollectionIr(decl.DataType);
                                StoreSlotObject(slot);
                                break;
                            default:
                                // Inizializzatore esplicito (DIM X AS LONG = 10).
                                if (decl.InitialValue is { } initVal)
                                {
                                    EmitExprObject(initVal);
                                    StoreSlotObject(slot);
                                }
                                break;
                        }
                    }
                    break;
                case PrintStmt p:
                    _il.Emit(OpCodes.Call, PrintStageBeginMethod);
                    for (var i = 0; i < p.Arguments.Length; i++)
                    {
                        var arg = p.Arguments[i];
                        if (arg.Expression is { } ex)
                        {
                            EmitExprObject(ex);
                            _il.Emit(OpCodes.Call, PrintValueMethod);
                        }
                        var isLast = i == p.Arguments.Length - 1;
                        if (isLast && (arg.Separator is null || arg.Separator.Length == 0))
                            _il.Emit(OpCodes.Call, PrintNewLineMethod);
                    }
                    _il.Emit(OpCodes.Ldc_I4_1);
                    _il.Emit(OpCodes.Call, PrintStageEndMethod);
                    break;
                case RemStmt:
                    break;
                case FontStmt fnt:
                    EmitExprObject(fnt.FontPath);
                    EmitExprDouble(fnt.Size);
                    _il.Emit(OpCodes.Call, GraphicsFontMethod);
                    break;
                case ColorStmt clr:
                    if (clr.Foreground is { } cFg) EmitExprObject(cFg); else _il.Emit(OpCodes.Ldnull);
                    if (clr.Background is { } cBg) EmitExprObject(cBg); else _il.Emit(OpCodes.Ldnull);
                    _il.Emit(OpCodes.Call, GraphicsColorMethod);
                    break;
                case LocateStmt loc:
                    EmitExprDouble(loc.ColumnExpr);
                    EmitExprDouble(loc.LineExpr);
                    _il.Emit(OpCodes.Call, GraphicsLocateMethod);
                    break;
                case LineStmt ln:
                    if (ln.X1 is { } lx1) EmitExprDouble(lx1); else _il.Emit(OpCodes.Ldc_R8, 0.0);
                    if (ln.Y1 is { } ly1) EmitExprDouble(ly1); else _il.Emit(OpCodes.Ldc_R8, 0.0);
                    EmitExprDouble(ln.X2); EmitExprDouble(ln.Y2);
                    if (ln.Color is { } lc) EmitExprDouble(lc); else _il.Emit(OpCodes.Ldc_R8, 0.0);
                    // Stile LINE nudo (`B`/`BF` come identificatori, non stringhe): il flat
                    // emitter li mappa a stringa; qui EmitExprObject su una variabile
                    // inesistente creava un load senza slot con label non marcata
                    // ("Label N has not been marked", /tmp/shot1.bas 2026-10-03).
                    if (ln.Style is { } lStyle)
                    {
                        string? styleStr = lStyle switch
                        {
                            StringExpr se => se.Value,
                            VariableExpr v when v.NameCleaned.Lexeme.Equals("BF", StringComparison.OrdinalIgnoreCase) => "BF",
                            VariableExpr v when v.NameCleaned.Lexeme.Equals("B", StringComparison.OrdinalIgnoreCase) => "B",
                            _ => null
                        };
                        if (styleStr is not null) _il.Emit(OpCodes.Ldstr, styleStr);
                        else EmitExprObject(lStyle);
                    }
                    else _il.Emit(OpCodes.Ldnull);
                    _il.Emit(OpCodes.Call, GraphicsLineMethod);
                    break;
                case CircleStmt ci:
                    EmitExprDouble(ci.X); EmitExprDouble(ci.Y); EmitExprDouble(ci.Radius);
                    if (ci.Color is { } cc2) EmitExprDouble(cc2); else _il.Emit(OpCodes.Ldc_R8, 0.0);
                    _il.Emit(OpCodes.Call, GraphicsCircleMethod);
                    break;
                case PSetStmt ps:
                    EmitExprDouble(ps.X); EmitExprDouble(ps.Y);
                    if (ps.Color is { } pc2) EmitExprDouble(pc2); else _il.Emit(OpCodes.Ldc_R8, 0.0);
                    _il.Emit(OpCodes.Call, GraphicsPsetMethod);
                    break;
                case PresetStmt pr:
                    EmitExprDouble(pr.X); EmitExprDouble(pr.Y);
                    _il.Emit(OpCodes.Call, GraphicsPresetMethod);
                    break;
                case DrawStmt dr:
                    EmitExprObject(dr.CommandString);
                    _il.Emit(OpCodes.Call, GraphicsDrawMethod);
                    break;
                case SaveScreenStmt ss:
                    if (ss.Name is { } sName) EmitExprObject(sName); else _il.Emit(OpCodes.Ldnull);
                    _il.Emit(OpCodes.Call, GraphicsSaveScreenMethod);
                    break;
                case RestoreScreenStmt rs:
                    if (rs.Name is { } rName) EmitExprObject(rName); else _il.Emit(OpCodes.Ldnull);
                    _il.Emit(OpCodes.Call, GraphicsRestoreScreenMethod);
                    break;
                case CPrintStmt cp:
                    // CPRINT nel v2: emesso come PRINT verso l'output host (parità headless —
                    // il marker deve apparire nel console catturato, NullGraphicsService no-op).
                    // Stage per-statement come PRINT (atomicità tra task concorrenti).
                    _il.Emit(OpCodes.Call, PrintStageBeginMethod);
                    for (var ci = 0; ci < cp.Arguments.Length; ci++)
                    {
                        var carg = cp.Arguments[ci];
                        if (carg.Expression is { } cex)
                        {
                            EmitExprObject(cex);
                            _il.Emit(OpCodes.Call, PrintValueMethod);
                        }
                        var cLast = ci == cp.Arguments.Length - 1;
                        if (cLast && (carg.Separator is null || carg.Separator.Length == 0))
                            _il.Emit(OpCodes.Call, PrintNewLineMethod);
                    }
                    _il.Emit(OpCodes.Ldc_I4_1);
                    _il.Emit(OpCodes.Call, PrintStageEndMethod);
                    break;
                case ClsStmt:
                    // CLS in headless/console: no-op (il flat lo emette come Clear del console).
                    break;
                case EndStmt:
                    break;
                case EndProcedureStmt:
                    break;
                case LabelStmt or EnterLoopStmt or ExitLoopStmt:
                    break; // marker di controllo: nessuna emissione
                case GotoStmt g:
                    // risolto dal lowering (Branch verso il pc del target) — qui il terminator
                    // Branch gestisce già il salto; nessuna azione aggiuntiva.
                    break;
                case JumpStmt j:
                    break;
                case GoProcedureStmt gp:
                    EmitGoProcedure(gp);
                    break;
                case DoneStmt done:
                    // WaitGroupDone (non DoneAndWake diretto): se `wg` è lo stesso registrato
                    // su task.WaitGroup (GO SUB ... IN wg), lo azzera per disattivare il Done
                    // IMPLICITO di fine-task (HandleFinishedV2) — altrimenti un DONE esplicito
                    // + quello implicito decrementano due volte per una singola goroutine (bug
                    // reale: con 2+ goroutine sullo stesso WAITGROUP, WAIT tornava dopo UNA
                    // sola di esse). Vedi IlGpmRuntime.WaitGroupDone.
                    EmitExprObject(done.WaitGroup);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeWaitGroup));
                    EmitLdarg(_taskArg);
                    _il.Emit(OpCodes.Call, WaitGroupDoneMethod);
                    break;
                case MutexUnlockStmt mu:
                    // UNLOCK mu (parità Go mu.Unlock()): sveglia al più un waiter. RWMUTEX:
                    // `MUTEX UNLOCK rw` con rw RWMUTEX usa PCodeRwMutex.Unlock (scrittore).
                    var muClr = IsRwMutexExpr(mu.Mutex) ? typeof(PCodeRwMutex) : typeof(PCodeMutex);
                    EmitExprObject(mu.Mutex);
                    _il.Emit(OpCodes.Castclass, muClr);
                    EmitLdarg(_taskArg);
                    _il.Emit(OpCodes.Callvirt, TaskGetScheduler);
                    _il.Emit(OpCodes.Callvirt, muClr == typeof(PCodeRwMutex) ? RwMutexUnlockMethod : MutexUnlockMethod);
                    break;
                case MutexRUnlockStmt mru:
                    // RWMUTEX RUNLOCK (parità Go rw.RUnlock()): rilascia un read lock — flat.
                    EmitExprObject(mru.Mutex);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeRwMutex));
                    EmitLdarg(_taskArg);
                    _il.Emit(OpCodes.Callvirt, TaskGetScheduler);
                    _il.Emit(OpCodes.Callvirt, RwMutexRUnlockMethod);
                    break;
                case CondSignalStmt cs:
                    // COND SIGNAL c (parità Go c.Signal()): sveglia al più UN waiter — flat.
                    EmitExprObject(cs.Cond);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeCond));
                    EmitLdarg(_taskArg);
                    _il.Emit(OpCodes.Callvirt, TaskGetScheduler);
                    _il.Emit(OpCodes.Callvirt, CondSignalMethod);
                    break;
                case CondBroadcastStmt cb:
                    // COND BROADCAST c (parità Go c.Broadcast()): sveglia TUTTI i waiter — flat.
                    EmitExprObject(cb.Cond);
                    _il.Emit(OpCodes.Castclass, typeof(PCodeCond));
                    EmitLdarg(_taskArg);
                    _il.Emit(OpCodes.Callvirt, TaskGetScheduler);
                    _il.Emit(OpCodes.Callvirt, CondBroadcastMethod);
                    break;
                case ErrorStmt er:
                    EmitErrorStatement(er);
                    break;
                case ThrowStmt ts:
                    EmitThrowStatement(ts);
                    break;
                case CloseChannelStmt cc:
                    EmitExprObject(cc.Channel);
                    _il.Emit(OpCodes.Castclass, typeof(ChannelInfo));
                    _il.Emit(OpCodes.Call, CloseChannelMethod);
                    break;
                case ExpressionStmt es:
                    EmitExpressionStmt(es);
                    break;
                case WaitKeyStmt:
                    EmitWaitKeyPark(b.Pc, b.Terminator.ResumePc, null);
                    break;
                case GetKeyboardStmt gk:
                    EmitWaitKeyPark(b.Pc, b.Terminator.ResumePc, gk.Variable);
                    break;
                case AtGetStmt:
                    // @GET: no-op flat — i campi sono cotti nel blocco PARK del READ (ReadFields).
                    break;
                case InkeyStmt ik:
                    // INKEY senza timeout: poll NON bloccante (mai park) → continua sempre.
                    EmitInkeyPoll(ik.Variable);
                    break;
                case UdpOpenStmt udpOp:
                    // UDP OPEN [port], handle — veloce (non park).
                    if (udpOp.Port is { } up) EmitInt32Arg(up);
                    else _il.Emit(OpCodes.Ldc_I4, 0);
                    _il.Emit(OpCodes.Call, UdpOpenMethod);
                    if (udpOp.Handle is VariableTarget uvt)
                    {
                        _il.Emit(OpCodes.Box, typeof(long));
                        StoreSlotObject(SlotOf(uvt.SymbolId));
                    }
                    else _il.Emit(OpCodes.Pop);
                    break;
                case TcpOpenStmt tcpOp:
                    EmitInt32Arg(tcpOp.Port);
                    _il.Emit(OpCodes.Call, TcpOpenMethod);
                    if (tcpOp.Handle is VariableTarget tvt)
                    {
                        _il.Emit(OpCodes.Box, typeof(long));
                        StoreSlotObject(SlotOf(tvt.SymbolId));
                    }
                    else _il.Emit(OpCodes.Pop);
                    break;
                case TcpCloseStmt tcpClose:
                    EmitInt64Arg(tcpClose.Handle);
                    _il.Emit(OpCodes.Call, TcpCloseMethod);
                    break;
                case SqlOpenStmt soStep:
                    // SQL OPEN "path" ALIAS a — veloce, non park (solo registrazione alias).
                    // Mirror del flat (EmitSqlOpenStatement): l'handle di ritorno si scarta
                    // (Pop) come nel flat quando nessun simbolo omonimo lo riceve — la
                    // registrazione in sessione è l'effetto che conta (bug 2026-10-01,
                    // todo Coarse-Park: senza, Main non era stepper e il park IL mai).
                    EmitStringArg(soStep.FilePath);
                    if (soStep.OriginalAliasToken is not null) _il.Emit(OpCodes.Ldstr, soStep.OriginalAliasToken.Lexeme);
                    else _il.Emit(OpCodes.Ldnull);
                    _il.Emit(OpCodes.Call, SqlOpenMethod);
                    _il.Emit(OpCodes.Pop);
                    break;
                case SqlCloseStmt scStep:
                    // SQL CLOSE alias|handle — veloce, non park. Alias come Ldstr (parità
                    // flat EmitSqlAliasCore), altrimenti oggetto boxed.
                    if (scStep.DbAlias is VariableExpr scve) { _il.Emit(OpCodes.Ldstr, scve.NameCleaned.Lexeme); }
                    else if (scStep.DbAlias is QualifiedNameExpr scqn && scqn.Parts.Length == 1) { _il.Emit(OpCodes.Ldstr, scqn.Parts[0].Lexeme); }
                    else EmitExprObject(scStep.DbAlias);
                    _il.Emit(OpCodes.Call, SqlCloseMethod);
                    break;
                case UdpSendStmt udpS:
                    // UDP SEND handle, host, port, data$ — veloce (non park, G8-bis subset).
                    EmitInt64Arg(udpS.Handle);
                    EmitStringArg(udpS.Host);
                    EmitInt32Arg(udpS.Port);
                    EmitStringArg(udpS.Data);
                    _il.Emit(OpCodes.Call, UdpSendMethod);
                    break;
                case UdpCloseStmt udpC:
                    // UDP CLOSE handle — veloce (non park, G8-bis subset).
                    EmitInt64Arg(udpC.Handle);
                    _il.Emit(OpCodes.Call, UdpCloseMethod);
                    break;
                case SetOptionStmt setOpt:
                    // SETOPTION handle, "OPT", valore — veloce (non park, G8-bis subset).
                    // IlRuntime.TcpSetOpt è il wrapper statico di INetworkService.TcpSetOption
                    // (parità col flat EmitSetOptionStatement, Statements.Network.cs:897).
                    EmitInt64Arg(setOpt.Handle);
                    EmitStringArg(setOpt.Option);
                    EmitStringArg(setOpt.Value);
                    _il.Emit(OpCodes.Call, TcpSetOptMethod);
                    break;
                case ShutdownStmt shut:
                    // SHUTDOWN handle, modo — veloce (non park; IlRuntime.TcpShut wrapper di
                    // INetworkService.TcpShutdown, parità col flat EmitShutdownStatement).
                    EmitInt64Arg(shut.Handle);
                    EmitStringArg(shut.Mode);
                    _il.Emit(OpCodes.Call, TcpShutMethod);
                    break;
                case TcpConnectStmt tcpCon:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, TcpConnectStepMethod,
                        () => { EmitStringArg(tcpCon.Host); EmitInt32Arg(tcpCon.Port); },
                        tcpCon.Handle);
                    break;
                case SslOpenStmt sslOp:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, SslOpenStepMethod,
                        () =>
                        {
                            EmitInt32Arg(sslOp.Port);
                            EmitStringArg(sslOp.CertificatePath);
                            if (sslOp.CertificatePassword is { } cp) EmitExprObject(cp);
                            else _il.Emit(OpCodes.Ldnull);
                        },
                        sslOp.Handle);
                    break;
                case SslConnectStmt sslC2:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, SslConnectStepMethod,
                        () =>
                        {
                            EmitStringArg(sslC2.Host);
                            if (sslC2.Port is { } sp2) EmitInt32Arg(sp2);
                            else _il.Emit(OpCodes.Ldc_I4, 0);
                            _il.Emit(OpCodes.Ldnull); // certPath non supportato nel subset
                        },
                        sslC2.Handle);
                    break;
                case WebSocketConnectStmt wsC:
                    EmitNetworkPark(b.Pc, b.Terminator.ResumePc, WsConnectStepMethod,
                        () =>
                        {
                            EmitStringArg(wsC.Host);
                            EmitInt32Arg(wsC.Port);
                            if (wsC.Path is { } wp) EmitStringArg(wp);
                            else _il.Emit(OpCodes.Ldnull);
                            if (wsC.Timeout is { } wt) EmitExprObject(wt);
                            else _il.Emit(OpCodes.Ldnull);
                        },
                        wsC.Handle);
                    break;
                default:
                    throw new NotSupportedException($"StepV2: statement flat '{stmt.GetType().Name}' non supportato (G2).");
            }
        }

        /// <summary>WAITKEY / GET KEY (G5): park cooperativo via IlGpmRuntime.TryWaitKeyPark
        /// (il task si registra in IlRuntime.InkeyWaiters; QueueInkey lo sveglia). Su wake il
        /// tasto è in coda → rientra e lo consuma.</summary>
}
