// FILE: Basic.IlBackend.Gpm.IlGpmV2.Types.cs
// PARTIAL per dominio (split da IlGpmV2.cs — direttiva 1200 righe): transizioni tipizzate,
// enum del passo/return/unwind, stati (unwind/onerror/regione/cleanup), frame di attivazione.
// Stesso namespace di IlGpmV2; nessuna semantica nuova.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Gpm;

    /// <summary>Transizione tipizzata prodotta da uno stepper v2. Il driver è l'unica
    /// autorità che applica la transizione e decide push/pop, enqueue, park o completamento.</summary>
    public enum IlStepKind
    {
        Continue,
        Yield,
        Park,
        Call,
        Return,
        Goto,
        Gosub,
        ReturnGosub,
        Throw,
        ContinueUnwind,
        CompleteTask,
        GoExit,
        Resume,
        /// <summary>Fine di un corpo DEFER (parità Go defer): il driver popa il successivo
        /// deferred LIFO, oppure completa il Return/GoExit che era stato interrotto.</summary>
        DeferDone
    }

    /// <summary>Destinazione del risultato di RETURN (piano §4.3): il valore non viene mai
    /// lasciato sull'evaluation stack CIL a un safepoint; viene scritto nella destinazione.</summary>
    public enum IlReturnDestination
    {
        Discard,
        Local,
        Spill,
        Global,
        ByRef,
        Future
    }

/// <summary>Record della transizione di uno step v2 (piano §4.2).</summary>
public readonly record struct IlStepTransition(
    IlStepKind Kind,
    int ResumePc,
    int TargetProcedure,
    object?[]? Arguments,
    IlReturnDestination Destination,
    object? Value,
    IlAwaitOperation? Await,
    Exception? Exception)
{
    /// <summary>Numero di slot Locali che il frame del callee (Call) deve allocare: il driver
    /// li alloca perché copia gli Arguments in Locals[0..n-1] PRIMA del primo StepV2 del callee.</summary>
    public int TargetLocalCount { get; init; }

    /// <summary>Slot di destinazione del RETURN (Call di una FUNCTION): con Destination Local/
    /// Spill/Global, il driver scrive il valore di ritorno in caller.Locals/Spills/Globals
    /// all'indice ReturnSlot.</summary>
    public int ReturnSlot { get; init; } = -1;

    /// <summary>Istanze __ClosureCtx promossa dal Linker per la call a una procedura locale
    /// con cattura (G4): il driver lo memorizza nel frame del callee e lo passa al dispatch
    /// come `this` dello stepper d'istanza. Null per le call normali.</summary>
    public object? ClosureContext { get; init; }
}

/// <summary>Factory usata dall'emitter StepV2_* per costruire una transizione: evita
/// l'inizializzazione di record value-type in IL (newobj + stfld).</summary>
public static class IlStepTransitionFactory
{
    public static IlStepTransition Step(
        IlStepKind kind, int resumePc, int targetProcedure,
        object?[]? arguments, IlReturnDestination destination, object? value, int targetLocalCount = 0)
        => new(kind, resumePc, targetProcedure, arguments, destination, value, null, null)
        {
            TargetLocalCount = targetLocalCount
        };

    /// <summary>Variante con eccezione (Throw/ContinueUnwind): usata dall'emitter nel catch
    /// CIL dei blocchi in regione protetta.</summary>
    public static IlStepTransition StepEx(
        IlStepKind kind, int resumePc, int targetProcedure,
        object?[]? arguments, IlReturnDestination destination, object? value, Exception? ex)
        => new(kind, resumePc, targetProcedure, arguments, destination, value, null, ex);

    /// <summary>Call di una FUNCTION (G4): porta args, la destinazione del RETURN e il suo slot.
    /// Per una procedura locale con CATTURA (closure) porta anche l'istanza __ClosureCtx.</summary>
    public static IlStepTransition StepCall(
        int resumePc, int targetProcedure, object?[]? arguments,
        IlReturnDestination destination, int targetLocalCount, int returnSlot,
        object? closureContext = null)
        => new(IlStepKind.Call, resumePc, targetProcedure, arguments, destination, null, null, null)
        {
            TargetLocalCount = targetLocalCount,
            ReturnSlot = returnSlot,
            ClosureContext = closureContext
        };
}

    /// <summary>Continuazione GOSUB nello stesso activation frame (piano §6): NON crea un
    /// activation frame di procedura.</summary>
    public readonly record struct IlGosubContinuation(
        int ReturnPc,
        int ExceptionDepth,
        int CleanupDepth,
        bool IsOnErrorHandler);

    /// <summary>Stato di unwind (piano §7): GOTO, Return, Exception, Cancel. FINALLY è bytecode
    /// sospendibile: se parcheggia, Unwind e CleanupStack restano nel frame e l'unwind riprende.</summary>
    public sealed class IlUnwindState
    {
        public IlUnwindReason Reason;
        public int TargetPc;
        public IlActivationFrame? TargetFrame;
        public object? ReturnValue;
        public Exception? Exception;
    }

    public enum IlUnwindReason
    {
        Goto,
        Return,
        Exception,
        Cancel,
        GoExit,
        ReturnGosub
    }

    /// <summary>Await operation unificata (piano §9): generation, retry, una sola completion,
    /// callback tardive innocue.</summary>
    public enum IlAwaitStatus
    {
        Created,
        Pending,
        Completed,
        Faulted,
        Cancelled,
        Consumed
    }

    public interface IlAwaitOperation
    {
        long Generation { get; }
        IlAwaitStatus Status { get; }
        bool TryStart(PCodeTask task, IlActivationFrame frame);
        bool TryConsume(out object? result, out Exception? error);
        void Cancel();
    }

    /// <summary>Stato ON ERROR frame-scoped (GOTO/GOSUB, ErrorLinePc, RESUME).</summary>
    public sealed class IlOnErrorState
    {
        public bool IsSet;
        public bool IsGosub;
        public int TargetPc;
        public int ErrorLinePc;
        public bool Suppressed;
    }

    /// <summary>Regione protetta (TRY/CATCH/FINALLY) nello stack del frame.</summary>
    public sealed class IlExceptionRegionState
    {
        public int CatchPc;
        public int FinallyPc;
        public int EndPc;
        public int ExceptionSlot;
        public bool InHandler;
    }

    /// <summary>Cleanup pendente (FINALLY/USING): l'unwind deve eseguirlo una sola volta e
    /// riprendere lo stesso motivo originale.</summary>
    public sealed class IlCleanupContinuation
    {
        public int CleanupPc;
        public int CleanupEndPc;
        public IlUnwindReason Reason;
        public bool Completed;
    }

    /// <summary>Activation frame v2 (piano §4.1): materializza PC, locali, spill, destinazione,
    /// call chain, GOSUB, regioni, cleanup, unwind, ON ERROR, await e valore di ritorno.</summary>
    public sealed class IlActivationFrame
    {
        public int ProcedureId;
        public int Pc;
        public IlActivationFrame? Caller;
        public int ReturnPc;
        public IlReturnDestination ReturnDestination;
        /// <summary>Slot di destinazione del RETURN (per Local/Spill/Global) deciso alla Call.</summary>
        public int ReturnSlot = -1;
        /// <summary>Istanze __ClosureCtx della procedura locale con cattura in esecuzione: per
        /// gli stepper d'istanza fa da `this` (delegato creato in Dispatch(procedureId, context)).</summary>
        public object? ClosureContext;
        public object?[]? Locals;
        public object?[]? Spills;
        /// <summary>Slot globali del task (destinazione Global del RETURN).</summary>
        public object?[]? Globals;
        public Stack<IlGosubContinuation>? GosubStack;
        public Stack<IlExceptionRegionState>? ExceptionStack;
        public Stack<IlCleanupContinuation>? CleanupStack;
        /// <summary>Blocchi DEFER registrati dalla procedura (IR block PC), eseguiti LIFO
        /// all'uscita (parità Go defer). Vivi SOLO per la vita della procedura corrente.</summary>
        public Stack<int>? DeferredBodies;
        public IlAwaitOperation? Await;
        public long AwaitGeneration;
        /// <summary>SelectContext della GO SELECT v2 persistito tra i retry (il wake azzera
        /// SleepUntilMs; la deadline del TIMEOUT vive qui).</summary>
        public SelectContext? SelectCtx;
        public IlUnwindState? Unwind;
        public IlOnErrorState OnError = new();
        public object? ReturnValue;
        /// <summary>Transizione Return/GoExit interrotta dall'esecuzione dei DEFER: rieseguita
        /// (Apply ricorsivo) quando lo stack DeferredBodies si svuota.</summary>
        public IlStepTransition? PendingExit;
        /// <summary>Stato del READ (@GET … READ) nel percorso stepper v2: campo corrente + buffer
        /// di edit, per sopravvivere al park per-tasto (IlGpmRuntime.ReadFieldStep). Null fuori
        /// da un READ.</summary>
        public object? ReadState;
    }

    /// <summary>Driver v2 — l'unica autorità che applica le transizioni (piano §0, §5).
    /// Mantiene la call chain come composizione di IlActivationFrame (NON carica campi sul
    /// PCodeTask condiviso: la PCodeVM usa lo stesso tipo). Call crea il frame del callee e
    /// collega Caller; Return scrive il valore nella destinazione e riprende ReturnPc del
    /// caller; CompleteTask significa SOLO fine della root.</summary>
