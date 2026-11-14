// FILE: Basic.Runtime.Gpm.GpmCore.cs
// GPM CORE — scheduler, task, frame. RIUSABILE da qualsiasi VM.
// NON contiene dispatch — PCodeVm/ fornisce il dispatch opcode.

using System.Collections.Concurrent;
using System.Linq;

namespace Basic.Runtime.Gpm;

// ========================================================================
// Task State
// ========================================================================
public enum PCodeTaskState { Runnable, Running, Syscall, Blocked, Finished }
public enum PCodeErrorMode { None, ResumeNext }
public enum PCodeFutureState { Pending, Value, Error, Cancelled }
public enum PCodeWaitKind { Channel, WaitGroup, Future, Timer, Select, Graphics, Network, Http, Input, Cpu, External, Mutex, Once, Cond }

public readonly record struct PCodeWaitMetrics(
    long Completed,
    long Cancelled,
    TimeSpan TotalDuration,
    TimeSpan MaximumDuration);

/// <summary>Registration generazionale comune per callback esterne. Completion e cancel
/// competono su un unico winner; soltanto il vincitore può mutare il PCB e risvegliarlo.</summary>
public sealed class PCodeWaitRegistration
{
    private readonly PCodeTask _task;
    private readonly PCodeScheduler? _scheduler;
    private readonly Action? _detach;
    private readonly long _startedTimestamp = System.Diagnostics.Stopwatch.GetTimestamp();
    private int _winner;

    internal PCodeWaitRegistration(PCodeTask task, long generation, PCodeWaitKind kind, Action? detach)
    {
        _task = task;
        _scheduler = task.Scheduler;
        _detach = detach;
        Generation = generation;
        Kind = kind;
    }

    public long Generation { get; }
    public PCodeWaitKind Kind { get; }
    public bool IsPending => Volatile.Read(ref _winner) == 0;

    public bool TryComplete(Action applyCompletion)
    {
        ArgumentNullException.ThrowIfNull(applyCompletion);
        if (Interlocked.CompareExchange(ref _winner, 1, 0) != 0)
            return false;
        _scheduler?.RecordWaitTerminal(Kind, cancelled: false, _startedTimestamp);
        try
        {
            applyCompletion();
        }
        catch (Exception ex)
        {
            _task.PendingException = ex;
        }
        finally
        {
            _scheduler?.WakeTask(_task);
        }
        return true;
    }

    public bool Cancel()
    {
        if (Interlocked.CompareExchange(ref _winner, 2, 0) != 0)
            return false;
        _scheduler?.RecordWaitTerminal(Kind, cancelled: true, _startedTimestamp);
        _detach?.Invoke();
        return true;
    }
}

/// <summary>Informazioni per TRY/CATCH handler.</summary>
public sealed class TryInfo
{
    /// <summary>Primo indirizzo protetto, subito dopo gli operandi di OP_PUSH_TRY.</summary>
    public int ProtectedStartIp { get; set; }
    public int HandlerIp { get; set; }
    public PCodeCallFrame? Frame { get; set; }
    /// <summary>Stack pointer da ripristinare prima di entrare nel CATCH.</summary>
    public int StackPointer { get; set; }
    public int FinallyIp { get; set; } = -1;
    public int EndIp { get; set; } = -1;
    public bool HasCatch { get; set; } = true;
}

public enum PCodeUnwindReason { Return, ReturnGosub, Goto, Exception, Cancel, GoExit }

public sealed record PCodeGosubContinuation(
    int ReturnIp,
    int StackPointer,
    int TryDepth,
    int CleanupDepth,
    PCodeCallFrame? Frame);

public sealed class PCodeUnwindState
{
    public PCodeUnwindReason Reason { get; init; }
    public int TargetIp { get; init; }
    public int ReturnOpcodeIp { get; init; }
    public int OriginIp { get; init; }
    public PCodeCallFrame? Frame { get; init; }
    public TryInfo? TargetHandler { get; init; }
    public object? ErrorValue { get; init; }
    public PCodeGosubContinuation? GosubContinuation { get; init; }
    public IReadOnlyList<object?> SuppressedErrors { get; init; } = Array.Empty<object?>();
}

/// <summary>Riferimento a una variabile (per BYREF).</summary>
public sealed class RefToVar
{
    public object?[] Array { get; }
    public int Index { get; }
    public RefToVar(object?[] array, int index) { Array = array; Index = index; }
}

/// <summary>Dati per una closure a runtime.</summary>
public sealed class ClosureData
{
    public int ConstIndex { get; }
    private readonly Dictionary<int, object?> _upvalues = new();
    public ClosureData(int constIndex) { ConstIndex = constIndex; }
    public object? GetUpvalue(int index) => _upvalues.TryGetValue(index, out var v) ? v : null;
    public void SetUpvalue(int index, object? value) { _upvalues[index] = value; }
}

/// <summary>
/// Risultato differito di una <c>GO FUNCTION ... INTO X</c> (OP_GO_FUNCTION). Creato quando la
/// goroutine viene lanciata, risolto dal driver dello scheduler (PCodeVm.Run/HandlePostTick)
/// quando il PCodeTask collegato (PCodeTask.FutureToResolve) finisce, con il valore che RETURN
/// ha lasciato in cima al suo stack — o con <see cref="Fail"/> se il task è terminato con
/// un'eccezione non gestita. La lettura di una variabile che contiene un future non ancora
/// risolto (OP_LOAD_LOCAL/OP_LOAD_GLOBAL) blocca il task lettore finché non arriva Resolve/Fail
/// — stesso pattern cooperativo (Queue di waiter + WakeTask) già usato per canali e waitgroup,
/// non un vero await/TaskCompletionSource. Un future FALLITO che nessuno legge mai non causa
/// nulla (nessun crash silenzioso) — l'eccezione emerge SOLO quando (e se) qualcuno prova a
/// leggere il valore, esattamente come un'eccezione .NET normale non gestita esplode solo dove
/// viene effettivamente propagata, non nel momento in cui viene creata.
/// </summary>
public sealed class PCodeFutureValue
{
    public PCodeFutureState State { get; private set; } = PCodeFutureState.Pending;
    public bool IsResolved => State != PCodeFutureState.Pending;
    public bool IsCancelled => State == PCodeFutureState.Cancelled;
    public object? Result { get; private set; }
    /// <summary>Non-null se il task collegato è terminato con un'eccezione non gestita invece
    /// che con un RETURN normale — vedi PCodeVm.HandlePostTick. Chi legge questo future (implicit
    /// await in OP_LOAD_LOCAL/OP_LOAD_GLOBAL) la propaga: al proprio TRY/CATCH se attivo,
    /// altrimenti al proprio ON ERROR GOTO, altrimenti termina con un'eccezione chiara — MAI
    /// silenziosamente ignorata, e MAI un blocco infinito del lettore (il bug che questo
    /// campo risolve: prima un future il cui task falliva restava per sempre IsResolved=false,
    /// e qualunque lettore restava bloccato all'infinito senza nessun segnale d'errore).</summary>
    public Exception? FailureException { get; private set; }
    private readonly object _lock = new();
    private sealed record FutureWaiter(PCodeTask Task, PCodeWaitRegistration Registration);
    private readonly Queue<FutureWaiter> _waiters = new();

    /// <summary>Risolve/Fail e RegisterWaiter condividono lo stesso lock: sotto vero parallelismo,
    /// senza questo lock un resolver potrebbe svuotare _waiters proprio nella finestra fra il
    /// check IsResolved e l'Enqueue di un lettore concorrente in RegisterWaiter, lasciandolo
    /// bloccato per sempre — la combinazione "check poi enqueue/drain" deve essere atomica come
    /// UNITÀ, non solo le singole operazioni sulla coda.</summary>
    public void Resolve(object? value, PCodeScheduler? scheduler)
    {
        List<FutureWaiter> toWake;
        lock (_lock)
        {
            if (State != PCodeFutureState.Pending) return;
            State = PCodeFutureState.Value;
            Result = value;
            toWake = new List<FutureWaiter>(_waiters);
            _waiters.Clear();
        }
        foreach (var waiter in toWake) waiter.Registration.TryComplete(() => { });
    }

    /// <summary>Risolve il future in stato di FALLIMENTO: il task collegato è terminato con
    /// un'eccezione non gestita invece che con RETURN. Sveglia comunque tutti i waiter — leggono
    /// un future "risolto" ma con FailureException valorizzata, propagata da loro (vedi sopra).</summary>
    public void Fail(Exception ex, PCodeScheduler? scheduler)
    {
        List<FutureWaiter> toWake;
        lock (_lock)
        {
            if (State != PCodeFutureState.Pending) return;
            State = PCodeFutureState.Error;
            FailureException = ex;
            toWake = new List<FutureWaiter>(_waiters);
            _waiters.Clear();
        }
        foreach (var waiter in toWake) waiter.Registration.TryComplete(() => { });
    }

    /// <summary>Completa il future in stato Cancelled, distinto da Error. Come Resolve/Fail,
    /// compete sotto lo stesso lock e sveglia i waiter una sola volta; completamenti tardivi
    /// non possono sovrascrivere lo stato terminale.</summary>
    public void Cancel(PCodeScheduler? scheduler)
    {
        List<FutureWaiter> toWake;
        lock (_lock)
        {
            if (State != PCodeFutureState.Pending) return;
            State = PCodeFutureState.Cancelled;
            FailureException = new OperationCanceledException("P-Code future cancelled.");
            toWake = new List<FutureWaiter>(_waiters);
            _waiters.Clear();
        }
        foreach (var waiter in toWake) waiter.Registration.TryComplete(() => { });
    }

    /// <summary>Registra il task come in attesa. Ritorna true se il chiamante deve bloccarsi
    /// (non ancora risolto al momento della chiamata). Imposta anche <c>task.State = Blocked</c>
    /// DENTRO lo stesso lock, nello stesso istante in cui il task diventa visibile in _waiters —
    /// il chiamante deve aver già preparato Ip/stack per il retry PRIMA di questa chiamata (vedi
    /// OP_LOAD_GLOBAL/OP_LOAD_LOCAL), non dopo: un Resolve/Fail concorrente che arriva subito dopo
    /// la registrazione potrebbe altrimenti trovare il task già "in attesa" ma con Ip/stack non
    /// ancora pronti per il retry, o con State ancora "Running" (ri-accodabile mentre il thread
    /// originale sta ancora eseguendo il proprio tick) — stessa classe di hang intermittente
    /// osservata sotto vero parallelismo per canali e waitgroup.</summary>
    public bool RegisterWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            if (IsResolved) return false;
            var registration = task.BeginWait(PCodeWaitKind.Future, () => RemoveWaiter(task));
            _waiters.Enqueue(new FutureWaiter(task, registration));
            return true;
        }
    }

    public void RemoveWaiter(PCodeTask task)
    {
        lock (_lock)
        {
            var count = _waiters.Count;
            for (var i = 0; i < count; i++)
            {
                var waiter = _waiters.Dequeue();
                if (!ReferenceEquals(waiter.Task, task))
                    _waiters.Enqueue(waiter);
            }
        }
    }
}

// ========================================================================
// PCodeCallFrame — stack frame
// ========================================================================
public sealed class PCodeCallFrame
{
    private const int DefaultMaximumStackCapacity = 1_048_576;

    public int ReturnIp { get; set; }
    public int? ContextOffset { get; set; }
    public int OperationState { get; set; }
    public PCodeCallFrame? Previous { get; set; }
    public object?[]? SavedLocals { get; set; }
    /// <summary>Stato dei cicli FOR di QUESTO frame: (counter, end, step) per slot.
    /// Vive sul frame, non sul task: lo stato di un ciclo appartiene al frame che lo esegue,
    /// quindi una chiamata annidata (che riusa gli stessi numeri di slot) non può più
    /// sovrascrivere né rimuovere lo stato del ciclo del chiamante al ritorno.
    /// Vedi OP_FOR_PREPARE/OP_FOR_LOOP in PCodeProcessor.</summary>
    public Dictionary<int, (double Counter, double End, double Step)>? ForState { get; set; }
    public object?[] LocalVariables { get; set; } = Array.Empty<object?>();
    /// <summary>Variabili DYNAMIC dichiarate da questa procedura (scope dinamico, spec
    /// Variables-Scope §2.5). La catena dei frame DI CHIAMATA è la catena dello scope dinamico:
    /// la lettura risale <see cref="Previous"/>, la scrittura colpisce il frame più interno che
    /// definisce il nome. Alla pop del frame le dichiarazioni spariscono (ripristino automatico).
    /// Vedi OP_DYN_* in PCodeProcessor. Null = nessuna dichiarazione (frame vuoto).</summary>
    public System.Collections.Generic.Dictionary<string, object?>? DynamicVars { get; set; }
    public object? ConstructorInstance { get; set; }
    public int SavedStackPointer { get; set; }
    public object?[] FrameStack { get; set; } = Array.Empty<object?>();
    public int FrameSp { get; set; }  // Stack pointer within FrameStack
    /// <summary>Limite di sicurezza configurabile; non rappresenta un limite semantico BASIC.</summary>
    public int MaximumFrameStackCapacity { get; set; } = DefaultMaximumStackCapacity;
    public object? ReturnValue { get; set; }  // Function return value (per-frame)
    public ClosureData? Closure { get; set; }  // Current closure for upvalue access
    public long IslandSleepDeadlineMs { get; set; }
    public bool IslandSendRetry { get; set; }
    public bool IslandRecvRetry { get; set; }
    /// <summary>True per i frame creati dal DISPATCH degli eventi (ON SPRITE COLLISION...):
    /// il RETURN NON deve popare un valore "di ritorno" casuale dallo stack del chiamante
    /// (rv = PopRef() + PushRef → Sp = savedSp+1 → leak di 1 entry per dispatch → overflow
    /// dello stack dopo ~4000 collisioni).</summary>
    public bool IsEventDispatch { get; set; }
    /// <summary>True per i frame creati da OP_CALL_CTOR (BASE.NEW, setter di proprietà,
    /// BASE.metodo()): una chiamata DIRETTA per indirizzo, non una nuova istanza. In OP_RETURN
    /// questi frame devono usare il check RELATIVO (task.Sp &gt; SavedStackPointer) e non
    /// l'assoluto (task.Sp &gt; 0): un costruttore/setter VOID non pusha nulla, ma con lo stack
    /// del chiamante non vuoto (es. `L.Add(NEW B(...))` con L sullo stack) il check assoluto
    /// poppava lo stack residuo (la LIST) come valore di ritorno spurio, corrompendo lo stack
    /// (bug: elemento derivato mai aggiunto alla lista, count=0).</summary>
    public bool IsDirectCall { get; set; }
    /// <summary>SymbolId della procedura BPM (Stepper) di cui questo frame esegue i chunk.
    /// Campo minimo aggiunto per il modello chunk-resumeIp (vedi AiWiki
    /// GMBasic-ILBackend-GPM-Chunk-State-Machine §26.4): il driver usa questo per
    /// disassemblare quale stepper invocare quando il task riprende (dispatch
    /// frame → stepper). -1 = main/top-level.</summary>
    public int OwnerProcedure { get; set; } = -1;
    /// <summary>True quando la frame rappresenta una nuova procedura e quindi delimita
    /// lo scope di ON ERROR. I frame GOSUB legacy restano nello scope del chiamante.</summary>
    public bool IsErrorScopeBoundary { get; set; }
    public int? SavedErrorHandlerIp { get; set; }
    public bool SavedErrorHandlerIsGosub { get; set; }
    public PCodeErrorMode SavedErrorMode { get; set; }
    public int SavedErrorLineIp { get; set; } = -1;
    public int SavedErrorResumeNextIp { get; set; } = -1;

    /// <summary>Stack LIFO degli indirizzi di cleanup DEFER registrati da questa procedura
    /// (parità Go defer): alla fine della procedura (RETURN/GOSUB-RETURN/unwind) il runtime
    /// esegue ogni deferred body nell'ordine inverso di registrazione. Solo la procedura
    /// proprietaria del frame esegue i propri DEFER; le chiamate annidate ne creano altri nel
    /// proprio frame. LIFO come Go.</summary>
    public Stack<int>? DeferredIps { get; set; }

    /// <summary>Inserisce un valore nello stack del frame, espandendolo geometricamente.</summary>
    public void PushFrameValue(object? value)
    {
        EnsureFrameStackCapacity(1);
        FrameStack[FrameSp++] = value;
    }

    /// <summary>Estrae e cancella il valore in cima allo stack del frame.</summary>
    public object? PopFrameValue()
    {
        if (FrameSp <= 0)
            return null;
        var index = --FrameSp;
        var value = FrameStack[index];
        FrameStack[index] = null;
        return value;
    }

    private void EnsureFrameStackCapacity(int additionalEntries)
    {
        var required = checked(FrameSp + additionalEntries);
        if (required > MaximumFrameStackCapacity)
            throw new PCodeStackOverflowException("frame", required, MaximumFrameStackCapacity);
        if (required <= FrameStack.Length)
            return;

        var capacity = Math.Max(1, FrameStack.Length);
        while (capacity < required)
        {
            var doubled = (long)capacity * 2;
            capacity = (int)Math.Min(doubled, MaximumFrameStackCapacity);
            if (capacity < required && capacity == MaximumFrameStackCapacity)
                throw new PCodeStackOverflowException("frame", required, MaximumFrameStackCapacity);
        }
        var stack = FrameStack;
        Array.Resize(ref stack, capacity);
        FrameStack = stack;
    }
}

/// <summary>Overflow deterministico di uno stack P-Code configurabile.</summary>
public sealed class PCodeStackOverflowException : InvalidOperationException
{
    /// <summary>Crea un errore che identifica stack, capacità richiesta e limite configurato.</summary>
    public PCodeStackOverflowException(string stackKind, int requiredCapacity, int maximumCapacity)
        : base($"P-Code {stackKind} stack requires {requiredCapacity} entries, maximum is {maximumCapacity}.")
    {
    }
}

/// <summary>Entry unico dello stack: double + reference + tipo + type tag BASIC.</summary>
public struct StackEntry
{
    public double Num;
    public object? Ref;
    public byte Type;      // 0=vuoto, 1=numerico, 2=reference
    public byte BasicType;  // StackBasicType: 0=unset, 1=Number, 2=String, 3=Bool, 4=Date, 5=Object, 6=Null
}

/// <summary>Type tag BASIC per PopNum/VALTYPE sicuri senza parsing di stringhe.</summary>
public static class StackBasicType
{
    public const byte Unset = 0;
    public const byte Number = 1;   // DOUBLE/INTEGER/LONG/FLOAT
    public const byte String = 2;   // STRING
    public const byte Bool = 3;     // TRUE/FALSE (BIT)
    public const byte Date = 4;     // DateTime/DateOnly/DateValue/GbDate
    public const byte Object = 5;   // DICT/LIST/UDT/CLASS
    public const byte Null = 6;     // NIL/NULL
    public const byte Currency = 7; // GbCurrency (fixed-point 4 dec)
}

// ========================================================================
// PCodeTask — green thread (PCB)
// ========================================================================
public sealed class PCodeTask
{
    private const int DefaultMaximumStackCapacity = 1_048_576;

    public int Id { get; } = Interlocked.Increment(ref _nextId);
    private static int _nextId;

    public PCodeTaskState State { get; set; } = PCodeTaskState.Runnable;
    /// <summary>Task che ha eseguito GO per creare questo PCB. Null identifica root, handler
    /// esterni e task deliberatamente detached. La relazione e' ownership di cancellazione,
    /// non una call frame e non modifica globals/stack.</summary>
    public PCodeTask? Parent { get; internal set; }
    public bool InQueue { get; set; }
    /// <summary>Richiesta di yield cooperativo (TaskYield = Gosched): letta e azzerata DENTRO la
    /// stessa ExecuteTaskTick (mai letta da un'altra tick — vedi HandlePostTick/LastTickWasYield
    /// per il perché un campo condiviso tra tick diverse è pericoloso).</summary>
    public bool YieldRequested { get; set; }
    public bool CancellationRequested { get; set; }
    public bool CancellationUnwindStarted { get; set; }
    public int LastProcessorIndex { get; internal set; } = -1;
    internal long RunnableSinceTimestamp { get; set; }
    private long _waitGeneration;
    private PCodeWaitRegistration? _pendingWait;

    public PCodeWaitRegistration BeginWait(PCodeWaitKind kind, Action? detach = null)
    {
        var registration = new PCodeWaitRegistration(
            this, Interlocked.Increment(ref _waitGeneration), kind, detach);
        Interlocked.Exchange(ref _pendingWait, registration)?.Cancel();
        State = PCodeTaskState.Blocked;
        return registration;
    }

    /// <summary>Re-park di un canale: riusa la registration CORRENTE se è ancora Pending e dello
    /// stesso kind — evita che BeginWait la CANCELLI mentre un receiver/sender concorrente sta
    /// per completarla (wake perso, deadlock multi-worker GOMAXPROCS&gt;1 nel handoff 1-slot:
    /// la re-entry del task ri-parcheggiata senza un wake reale annullava la registration del
    /// canale → il TryWake del concorrente non svegliava più il task).</summary>
    public PCodeWaitRegistration ReuseOrBeginWait(PCodeWaitKind kind, Action? detach = null)
    {
        var current = _pendingWait;
        if (current is { IsPending: true } && current.Kind == kind)
            return current;
        return BeginWait(kind, detach);
    }

    public void ClearPendingWaitCancellation()
    {
        Interlocked.Exchange(ref _pendingWait, null);
    }

    /// <summary>Codice/descrizione dell'ULTIMO errore BASIC sollevato su questo task. Serve ai
    /// driver che devono RILANCIARE l'errore in un altro contesto (generatori): quando
    /// RaiseBasicError non trova handler lancia una InvalidOperationException e il codice
    /// andrebbe perso (il CATCH del chiamante vedeva 200).</summary>
    public int LastErrorCode;
    public string? LastErrorDescription;

    public string DebugPendingWait
    {
        get
        {
            var wait = Volatile.Read(ref _pendingWait);
            return wait is null
                ? "none"
                : $"{wait.Kind}/g{wait.Generation}/pending={wait.IsPending}";
        }
    }

    /// <summary>Kind del wait CORRENTE se il task è Blocked su un'attesa ancora pending, o null
    /// (nessun wait registrato, o registrazione già consumata/cancellata). Usato dal watchdog
    /// deadlock (PCodeScheduler.CheckDeadlock) per distinguere un'attesa PURAMENTE interna
    /// (Channel/WaitGroup/Future/Select/Mutex/Once/Cond — può sbloccarsi SOLO per azione di un
    /// altro task nello stesso scheduler) da un'attesa che dipende da qualcosa di ESTERNO allo
    /// scheduler (Network/Http/Input/Graphics/Cpu/External — una richiesta HTTP, una pressione
    /// di tasto, un file — che può risolversi in qualunque momento indipendentemente da cosa
    /// fanno gli altri task). Solo la prima categoria conta come "nessuno può più fare
    /// progressi": un programma con un task fermo su WAITKEY non è deadlockato, sta aspettando
    /// l'utente.</summary>
    public PCodeWaitKind? PendingWaitKind
    {
        get
        {
            var wait = Volatile.Read(ref _pendingWait);
            return wait is { IsPending: true } ? wait.Kind : null;
        }
    }

    public void CancelPendingWait()
    {
        Interlocked.Exchange(ref _pendingWait, null)?.Cancel();
    }
    /// <summary>True dal momento in cui FindRunnableTask consegna questo task a un worker fino al
    /// momento in cui quel worker ha completato per intero il tick (flush di PendingPrints incluso,
    /// vedi PCodeProcessor.ExecuteTaskTick + PCodeScheduler.ReleaseExecuting). Copre una finestra
    /// che InQueue da solo NON copre: un canale/waitgroup può marcare il task Blocked e visibile
    /// per il wake MENTRE è ancora dentro DispatchOpcode, cioè PRIMA che il worker proprietario
    /// arrivi al proprio finally e scarichi PendingPrints. Se un secondo thread accodasse e
    /// ripescasse il task in quella finestra (InQueue era già stato resettato a false al momento
    /// del pickup), due worker eseguirebbero ExecuteTaskTick sullo STESSO PCodeTask in parallelo:
    /// stesso output stampato due volte (bug osservato in GoRoutinesMinimal.bas — "duplicated
    /// startup output"), e in generale Ip/stack condivisi scritti da due thread insieme. Guardato
    /// da EnqueueLock, stesso lock di InQueue.</summary>
    public bool Executing { get; set; }
    /// <summary>Impostato da Enqueue quando arriva un wake mentre Executing è ancora true: invece
    /// di accodare subito (che riprodurrebbe la race), l'accodamento viene rimandato — il worker
    /// che sta finendo il tick corrente lo controlla nel proprio ReleaseExecuting, sotto lo stesso
    /// EnqueueLock, e si riaccoda da solo. Senza questo, il wake andrebbe perso silenziosamente.</summary>
    public bool WakePending { get; set; }
    /// <summary>Lock dedicato per-task che protegge la sequenza check-then-act "IsFinished/InQueue,
    /// poi InQueue=true" in PCodeScheduler.Enqueue, e ora anche Executing/WakePending. Un lock per
    /// task (non uno globale sullo scheduler) evita che l'accodamento di task INDIPENDENTI si
    /// serializzi a vicenda sotto vero parallelismo — solo due thread che tentano di accodare o
    /// eseguire LO STESSO task contendono.</summary>
    public readonly object EnqueueLock = new();
    public int Ip { get; set; }

    // Stack UNIFICATO: stesso array per double + reference + tipo
    // Locality: un solo cache line per entry, non 3 separati
    public StackEntry[] Stack { get; set; } = Array.Empty<StackEntry>();
    public int Sp { get; set; }
    /// <summary>Limite di sicurezza configurabile; non rappresenta un limite semantico BASIC.</summary>
    public int MaximumStackCapacity { get; set; } = DefaultMaximumStackCapacity;

    public void PushNum(double v) { EnsureStackCapacity(1); Stack[Sp].Num = v; Stack[Sp].Ref = null; Stack[Sp].Type = 1; Stack[Sp].BasicType = StackBasicType.Number; Sp++; }
    public double PopNum() {
        if (Sp <= 0) return 0.0;
        Sp--;
        var entry = Stack[Sp];
        Stack[Sp] = default;
        if (entry.Type == 1) return entry.Num;
        // Type-tag based fast path: Bool → -1.0 (true) or 0.0 (false)
        if (entry.BasicType == StackBasicType.Bool)
            return entry.Ref is bool b && b ? -1.0 : 0.0;
        // Currency: extract double from GbCurrency
        if (entry.BasicType == StackBasicType.Currency)
            return entry.Ref is Basic.Databases.Rdd.GbCurrency c ? c.ToDouble() : 0.0;
        // Non-numeric types: return 0.0 senza tentare parsing
        if (entry.BasicType == StackBasicType.String ||
            entry.BasicType == StackBasicType.Date ||
            entry.BasicType == StackBasicType.Null)
            return 0.0;
        // Fallback: try double reference or parse string (legacy, pre-BasicType)
        return entry.Ref is double d ? d :
               double.TryParse(entry.Ref?.ToString(), System.Globalization.NumberStyles.Any,
                   System.Globalization.CultureInfo.InvariantCulture, out var r) ? r : 0.0;
    }

    public void PushRef(object? v) { 
        EnsureStackCapacity(1);
        Stack[Sp].Ref = v; Stack[Sp].Type = 2;
        Stack[Sp].BasicType = v switch {
            string => StackBasicType.String,
            bool => StackBasicType.Bool,
            DateTime or DateOnly or Basic.Databases.Rdd.GbDate => StackBasicType.Date,
            Basic.Databases.Rdd.GbCurrency => StackBasicType.Currency,
            null => StackBasicType.Null,
            double or int or long or float or decimal or short or byte => StackBasicType.Number,
            _ => v.GetType().Name == "DateValue" ? StackBasicType.Date : StackBasicType.Object
        };
        Sp++;
    }

    public void PushString(string? v) {
        EnsureStackCapacity(1);
        Stack[Sp].Ref = v; Stack[Sp].Type = 2; Stack[Sp].BasicType = StackBasicType.String; Sp++;
    }

    public void PushBool(bool v) {
        EnsureStackCapacity(1);
        Stack[Sp].Ref = v; Stack[Sp].Type = 2; Stack[Sp].BasicType = StackBasicType.Bool; Sp++;
    }

    /// <summary>Restituisce il tipo BASIC del valore in cima allo stack. Per VALTYPE().</summary>
    public byte PeekBasicType() => Sp > 0 ? Stack[Sp - 1].BasicType : StackBasicType.Unset;
    public object? PopRef() { if (Sp <= 0) return null; Sp--; var entry = Stack[Sp]; Stack[Sp] = default; return entry.Type == 2 ? entry.Ref : (object)entry.Num; }
    /// <summary>Legge il valore in cima allo stack SENZA rimuoverlo. Usato dal driver dello
    /// scheduler per leggere il valore di RETURN di un task finito lanciato con OP_GO_FUNCTION,
    /// senza dover alterare Sp di un task che ha già smesso di eseguire.</summary>
    public object? PeekTop() => Sp > 0 ? (Stack[Sp - 1].Type == 2 ? Stack[Sp - 1].Ref : (object)Stack[Sp - 1].Num) : null;

    public bool IsEmpty => Sp <= 0;

    // Backward compat: OperandStack emulato
    public int OldStackCount => Sp;

    private void EnsureStackCapacity(int additionalEntries)
    {
        var required = checked(Sp + additionalEntries);
        if (required > MaximumStackCapacity)
            throw new PCodeStackOverflowException("operand", required, MaximumStackCapacity);
        if (required <= Stack.Length)
            return;

        var capacity = Math.Max(1, Stack.Length);
        while (capacity < required)
        {
            var doubled = (long)capacity * 2;
            capacity = (int)Math.Min(doubled, MaximumStackCapacity);
            if (capacity < required && capacity == MaximumStackCapacity)
                throw new PCodeStackOverflowException("operand", required, MaximumStackCapacity);
        }
        var stack = Stack;
        Array.Resize(ref stack, capacity);
        Stack = stack;
    }

    public PCodeCallFrame? Frame { get; set; }
    public bool IsFinished => State == PCodeTaskState.Finished;
    /// <summary>Attivazione corrente della GPM IL v2 (IlActivationFrame, in Basic.IlBackend.Gpm).
    /// Composizione voluta dal piano: la P-CODE VM non legge MAI questo slot; è un campo
    /// object? per non caricare PCodeCallFrame con campi ambigui tra i due runtime.</summary>
    public object? CurrentIlFrame { get; set; }
    public object? PendingResult { get; set; }
    public Exception? PendingException { get; set; }
    /// <summary>Transazione GO SELECT parcheggiata. Al wake il case CAS vincente viene
    /// ritentato prima della normale scansione, poi il contesto viene eliminato.</summary>
    public SelectContext? PendingSelect { get; set; }
    /// <summary>Quando OP_RETURN sta eseguendo i DEFER della procedura corrente (parità Go
    /// defer): non-null durante la fase di esecuzione dei body LIFO; azzerato da OP_DEFER_RET
    /// quando l'ultimo defer è eseguito, così il RETURN può completare la procedura.</summary>
    public bool PendingDeferReturn { get; set; }
    // Stack di ritorno per goroutine (OP_GO)
    public System.Collections.Generic.Stack<int>? IpStack { get; set; }
    /// <summary>Continuazioni GOSUB ordinarie. Restano nello stesso activation frame e
    /// conservano le profondità TRY/cleanup necessarie al RETURN strutturato.</summary>
    public System.Collections.Generic.Stack<PCodeGosubContinuation> GosubContinuations { get; } = new();

    /// <summary>Falling-back dello scope dinamico (DYNAMIC) quando il task non ha ancora un
    /// frame di chiamata (prologo/top-level). Normalmente le dichiarazioni vivono su
    /// <see cref="PCodeCallFrame.DynamicVars"/> lungo la catena dei frame; questo bag base
    /// copre i casi senza frame. Vedi OP_DYN_* in PCodeProcessor.</summary>
    public System.Collections.Generic.Dictionary<string, object?> BaseDynamicVars { get; }
        = new(System.StringComparer.OrdinalIgnoreCase);
    
    // === GPM blocking support ===
    // Quando un task e' bloccato (RECEIVE, WAIT), viene messo in questa coda
    // e NON riaccodato finche' il blocco non si risolve.
    public PCodeScheduler? Scheduler { get; set; }
    public bool IsBlocked { get; set; }

    /// <summary>Se non-null, questo task è stato lanciato con OP_GO_FUNCTION: quando finisce
    /// (State=Finished, nessuna eccezione pendente) il driver dello scheduler lo risolve con il
    /// valore lasciato in cima allo stack da RETURN.</summary>
    public PCodeFutureValue? FutureToResolve { get; set; }

    /// <summary>WaitGroup associato a un task lanciato con GO SUB ... IN wg: il Done implicito
    /// (Decremento) avviene quando il task termina, in HandlePostTick — senza questo, un
    /// `GO SUB X IN wg` + `WAIT wg` con X senza `DONE wg` esplicito bloccava per sempre
    /// (contatore mai decrementato).</summary>
    public object? WaitGroup { get; set; }

    /// <summary>Buffer dei PRINT accumulati durante il tick corrente (Kind: 0=Print, 1=PrintLine,
    /// 2=PrintNewLine). Svuotato in un colpo solo, sotto un unico lock, da ExecuteTaskTick al
    /// termine del tick — così una PRINT multi-argomento (es. <c>PRINT "a"; ID; "b"</c>, più
    /// opcode separati) non può essere spezzata a metà da un'altra goroutine che stampa in
    /// parallelo sullo stesso Console/writer condiviso (bug osservato sotto vero parallelismo:
    /// testo di due lavoratori diversi interlacciato a metà parola — locking per singolo opcode
    /// non basta, protegge ogni scrittura individualmente ma non l'intera istruzione).
    /// NOTA: questo da solo NON basta se il TICK stesso finisce a metà istruzione PRINT — vedi
    /// <see cref="PrintLineStaging"/>, che è la vera protezione per quel caso.</summary>
    public List<(byte Kind, string? Text, bool PrinterOnly, bool PrinterRouting)>? PendingPrints { get; set; }

    /// <summary>Accumula i pezzi di una singola istruzione PRINT multi-argomento (es. <c>PRINT
    /// "a"; espr; "b"</c>, che compila in più opcode OP_PRINT_VALUE/OP_PRINT_TAB_ZONE separati)
    /// PRIMA che entrino in <see cref="PendingPrints"/> — perché valutare uno degli argomenti
    /// (es. un campo su un <c>PCodeFutureValue</c> non ancora risolto) può risospendere il task a
    /// metà istruzione (implicit await, OP_LOAD_LOCAL/OP_LOAD_GLOBAL), facendo terminare il TICK
    /// corrente prima che l'istruzione PRINT sia completa. Se i pezzi già valutati fossero già in
    /// PendingPrints a quel punto, il flush di fine-tick in ExecuteTaskTick li stamperebbe SUBITO,
    /// PRIMA del resto — esattamente lo stesso interlacciamento che PendingPrints doveva evitare,
    /// solo spostato da "un opcode" a "un tick" come unità di granularità sbagliata (bug reale,
    /// osservato come "[CLIENT] GET /: Richiesta ricevuta: GET /" seguito da un valore su una
    /// riga a sé: il prefisso di un task interlacciato col PRINT COMPLETO di un altro task,
    /// prima che il primo si risvegliasse per stampare il resto). Solo quando l'istruzione PRINT
    /// raggiunge il suo ultimo pezzo (OP_PRINT_LINE) o un OP_PRINT_NEWLINE la trova non vuota,
    /// tutto il contenuto qui accumulato viene spostato in PendingPrints in un solo colpo.</summary>
    public System.Text.StringBuilder? PrintLineStaging { get; set; }

    /// <summary>Frame della PROCEDURA che ha iniziato lo staging corrente. Serve a riconoscere
    /// un PRINT ANNIDATO (un PRINT dentro una procedura chiamata come argomento): lo staging
    /// appartiene a un frame più esterno e va SOSPESO, non accodato — altrimenti il testo del
    /// PRINT interno si fonde col prefisso dell'esterno (`b=[dentro]` invece di `[dentro]` +
    /// `b= 5`). Stesso canone di C99Native/IL (PrintStageBegin/End).</summary>
    public PCodeCallFrame? PrintLineStagingFrame { get; set; }

    /// <summary>Livelli di staging SOSPESI (PRINT annidati), LIFO. Ripresi al commit della
    /// riga interna. Sul percorso d'errore si scartano tutti (DiscardIncompletePrintStatement).</summary>
    public System.Collections.Generic.Stack<(System.Text.StringBuilder Buf, PCodeCallFrame? Frame)>? PrintLineStagingSaved { get; set; }

    // === Stato ambient database, per-task (non più su PCodeProcessor condiviso) ===
    // "Area corrente" ISAM/DBF e "ultimo SEEK trovato" sono semanticamente "la MIA area
    // corrente"/"il risultato del MIO ultimo SEEK", non uno stato di programma condiviso — se
    // restassero campi su PCodeProcessor (condiviso da tutte le goroutine), un ISAM USE di una
    // goroutine potrebbe silenziosamente redirigere gli opcode impliciti-su-area di un'ALTRA
    // goroutine: un lock per singolo opcode non basterebbe a prevenirlo, perché il gap è FRA due
    // opcode separati (anche statement separati) della stessa goroutine, durante il quale un'altra
    // goroutine può intromettersi. Renderli per-task elimina la condivisione stessa — non solo la
    // mitiga: locazioni di memoria diverse per task diversi, nessuna interferenza possibile per
    // costruzione.
    public int CurrentIsamArea { get; set; }
    public int CurrentDbfArea { get; set; }
    public bool LastIsamFound { get; set; }

    /// <summary>Risveglia un task bloccato: lo riaccoda.</summary>
    public void Wake()
    {
        IsBlocked = false;
        State = PCodeTaskState.Runnable;
        Scheduler?.WakeTask(this);
    }

    // === Variables ===
    public object?[]? Globals { get; set; }

    /// <summary>Stack LIFO degli indirizzi dei body DEFER registrati da un task SENZA frame
    /// (goroutine radice GO SUB): parità Go defer, stesso scopo di
    /// <see cref="PCodeCallFrame.DeferredIps"/> ma a livello task, perché la goroutine radice
    /// non ha un PCodeCallFrame proprio. Letto da OP_DEFER (frame ?? task) e dai deferred
    /// runner (BeginDeferredExecution/ExecuteNextDeferredOrCompleteReturn).</summary>
    public Stack<int>? DeferredIps { get; set; }

    /// <summary>ONCE RUN (parità Go sync.Once): il PCodeOnce attualmente "in esecuzione" da
    /// questo task (impostato da OP_ONCE_BEGIN, consumato da OP_ONCE_END). Per-task perché
    /// ogni chiamante concorrente ha il proprio flusso di esecuzione.</summary>
    public PCodeOnce? PendingOnce { get; set; }
    public int LocalCapacity { get; set; } = 256;
    private object?[] _locals = [];

    // === FOR loop state ===
    public readonly Dictionary<int, (double Counter, double End, double Step)> ForState = new();
    
    /// <summary>Enumerator per i cicli FOR EACH, indicizzati per slot del contatore.</summary>
    public readonly Dictionary<int, System.Collections.IEnumerator> ForEachEnumerators = new();

    // === File handles ===
    public readonly Dictionary<int, long> FileHandles = new();

    /// <summary>Content type per file handle: 0=Default, 1=Json (mirrors Compiler.ContentTypeCode).</summary>
    public readonly Dictionary<int, byte> FileContentTypes = new();
    
    // === SLEEP counter ===
    private long _sleepUntilMs;
    private long _sleepGeneration;
    public long SleepUntilMs
    {
        get => Volatile.Read(ref _sleepUntilMs);
        set
        {
            Volatile.Write(ref _sleepUntilMs, value);
            var generation = Interlocked.Increment(ref _sleepGeneration);
            if (value > 0)
                Scheduler?.RegisterSleep(this, value, generation);
        }
    }
    public long SleepGeneration => Volatile.Read(ref _sleepGeneration);

    /// <summary>Parcheggia questo PCB su una deadline usando la registration generazionale
    /// comune. Usato sia dagli opcode P-Code sia da IslandRuntime.</summary>
    public void ParkUntil(long deadlineMs, PCodeWaitRegistration registration)
    {
        Volatile.Write(ref _sleepUntilMs, deadlineMs);
        Scheduler?.RegisterSleep(this, deadlineMs, registration);
    }

    private long _epochTicks;
    private bool _epochInitialized;
    public long EpochTicks
    {
        get
        {
            if (!_epochInitialized) { _epochTicks = DateTime.UtcNow.Ticks; _epochInitialized = true; }
            return _epochTicks;
        }
    }

    // === Error handling ===
    public int? ErrorHandlerIp { get; set; }
    public bool ErrorHandlerIsGosub { get; set; }
    public PCodeErrorMode ErrorMode { get; set; } = PCodeErrorMode.None;
    /// <summary>Indirizzo dell'opcode che ha sollevato l'ultimo errore BASIC.</summary>
    public int ErrorLineIp { get; set; } = -1;
    /// <summary>Prima istruzione successiva all'opcode che ha sollevato l'ultimo errore.</summary>
    public int ErrorResumeNextIp { get; set; } = -1;
    /// <summary>Indirizzo dell'opcode attualmente in dispatch nella PCodeVM.</summary>
    public int CurrentInstructionIp { get; set; } = -1;
    // ErrorLineIp è condiviso dai due esecutori gestiti: nello stepper IL identifica il chunk,
    // nella PCodeVM l'offset dell'opcode. In entrambi i casi deve persistere tra tick.

    // === TRY/CATCH ===
    public readonly Stack<TryInfo> TryStack = new();
    public readonly Stack<TryInfo> CleanupStack = new();
    public PCodeUnwindState? PendingUnwind { get; set; }
    public IReadOnlyList<object?> LastSuppressedErrors { get; set; } = Array.Empty<object?>();

    // === Loop context ===
    public int LoopDepth { get; set; }

    // === DATA pointer ===
    private int _dataIndex;
    public object? ReadNextData()
    {
        return _dataIndex++; // Placeholder — DATA table access needs BasicProgram ref
    }
    public void ResetDataPointer() { _dataIndex = 0; }

    public object? GetGlobal(int index)
    {
        if (Globals != null && index >= 0 && index < Globals.Length)
            return Globals[index];
        return null;
    }

    public void EnsureGlobals(int size)
    {
        if (Globals == null || Globals.Length < size)
        {
            var newGlobals = new object?[size];
            if (Globals != null)
                Array.Copy(Globals, newGlobals, Globals.Length);
            Globals = newGlobals;
        }
    }

    public object?[] GetLocals()
    {
        if (_locals == null || _locals.Length == 0)
            _locals = new object?[LocalCapacity];
        return _locals;
    }
}

// ========================================================================
// PCodeScheduler — scheduler con parallelismo reale: N worker thread OS (PCodeVm.WorkerLoop,
// GOMAXPROCS=Environment.ProcessorCount di default, override GMBASIC_GOMAXPROCS) rappresentano
// i P logici. Wake/injection esterni entrano nella coda globale; preemption/yield torna nella
// coda locale del P; un P inattivo ruba dal lato opposto della coda di una vittima. Runnext
// conserva l'affinità dei wake e concede al task risvegliato il turno successivo. Più PCodeTask possono eseguire
// SIMULTANEAMENTE su core diversi — da cui il lock per-task su Enqueue/FindRunnableTask
// (PCodeTask.EnqueueLock) e i lock per-oggetto su ChannelInfo/PCodeWaitGroup/PCodeFutureValue.
// ========================================================================
public sealed class PCodeScheduler : IDisposable
{
    public int LocalCapacity { get; set; } = 256;
    private sealed class ProcessorRunQueue
    {
        private readonly object _lock = new();
        private readonly LinkedList<PCodeTask> _tasks = new();

        public int Count { get { lock (_lock) return _tasks.Count; } }

        public void Push(PCodeTask task)
        {
            lock (_lock) _tasks.AddLast(task);
        }

        public bool TryPop(out PCodeTask? task)
        {
            lock (_lock)
            {
                if (_tasks.Last is not { } node) { task = null; return false; }
                task = node.Value;
                _tasks.RemoveLast();
                return true;
            }
        }

        public bool TrySteal(out PCodeTask? task)
        {
            lock (_lock)
            {
                if (_tasks.First is not { } node) { task = null; return false; }
                task = node.Value;
                _tasks.RemoveFirst();
                return true;
            }
        }

        public void Clear()
        {
            lock (_lock) _tasks.Clear();
        }
    }

    private readonly ConcurrentQueue<PCodeTask> _globalQueue = new();
    private readonly SemaphoreSlim _workAvailable = new(initialCount: 0);
    private readonly object _processorsLock = new();
    private ProcessorRunQueue[] _processorQueues = [];
    private PCodeTask?[] _runNext = [];
    private volatile bool _shutdown;
    private long _stealCount;
    private long _dispatchCount;
    private long _totalRunnableDelayTicks;
    private long _maximumRunnableDelayTicks;
    private sealed class WaitCounters
    {
        public long Completed;
        public long Cancelled;
        public long TotalDurationTicks;
        public long MaximumDurationTicks;
    }
    private readonly ConcurrentDictionary<PCodeWaitKind, WaitCounters> _waitMetrics = new();
    private readonly List<PCodeTask> _allTasks = new();
    private readonly ManualResetEventSlim _allTasksCompleted = new(initialState: true);
    private readonly object _timerLock = new();
    private readonly PriorityQueue<(PCodeTask Task, long Generation, PCodeWaitRegistration? Registration), long> _sleepTimers = new();
    private readonly AutoResetEvent _timerChanged = new(initialState: false);
    private sealed record ExternalOperation(PCodeTask? Owner, Action Cancel);
    private readonly ConcurrentDictionary<long, ExternalOperation> _externalOperations = new();
    private long _nextExternalOperationId;

    /// <summary>Configura (o RICONFIGURA a runtime, come Go `runtime.GOMAXPROCS(n)`) il numero
    /// di P logici usati dalla PCodeVM. Gli altri host GPM possono omettere la chiamata e
    /// continuare a usare soltanto la coda globale. Se il nuovo numero è inferiore a quello
    /// attuale, i task ancora nelle code locali e nei runnext dei P rimossi vengono MIGRATI alla
    /// coda globale (mai persi — invariante Go: la riduzione di GOMAXPROCS non perde goroutine);
    /// i worker eccedenti vengono segnalati (retired) e terminano al prossimo giro.</summary>
    public void ConfigureProcessors(int count)
    {
        if (count <= 0) throw new ArgumentOutOfRangeException(nameof(count));
        lock (_processorsLock)
        {
            if (count == _processorQueues.Length) return;
            if (_processorQueues.Length == 0)
            {
                _processorQueues = Enumerable.Range(0, count).Select(_ => new ProcessorRunQueue()).ToArray();
                _runNext = new PCodeTask?[count];
                return;
            }
            if (count < _processorQueues.Length)
            {
                // Migra i task dei P rimossi (coda locale LIFO + runnext) alla coda globale.
                for (var i = count; i < _processorQueues.Length; i++)
                {
                    while (_processorQueues[i].TryPop(out var task)) _globalQueue.Enqueue(task!);
                    var runNextTask = Interlocked.Exchange(ref _runNext[i], null);
                    if (runNextTask != null) _globalQueue.Enqueue(runNextTask);
                }
                Array.Resize(ref _processorQueues, count);
                Array.Resize(ref _runNext, count);
            }
            else
            {
                var old = _processorQueues.Length;
                Array.Resize(ref _processorQueues, count);
                Array.Resize(ref _runNext, count);
                for (var i = old; i < count; i++)
                    _processorQueues[i] = new ProcessorRunQueue();
            }
            // Un worker eccedente potrebbe essere parcheggiato sul semaforo: svegliamolo perché
            // veda il proprio indice retired e termini. (Gli slot con lavoro migrato hanno già
            // rilasciato permessi via coda globale; i permessi extra sono innocui.)
            for (var i = 0; i < Math.Max(1, count); i++)
                _workAvailable.Release();
        }
    }

    /// <summary>True se il worker con l'indice dato deve uscire (P rimosso da una riconfigurazione
    /// a runtime). Un worker con indice < ProcessorCount continua invece a servire il proprio P.</summary>
    public bool IsProcessorRetired(int processorIndex)
    {
        lock (_processorsLock)
            return processorIndex >= _processorQueues.Length;
    }

    /// <summary>Sveglia il worker all'indice dato (se parcheggiato sul semaforo) perché controlli
    /// lo stato di retirement dopo una riconfigurazione dei P. I permessi extra sono innocui:
    /// un permesso senza lavoro visibile fa solo ri-scansionare le code.</summary>
    public void WakeRetiredWorkerCheck(int processorIndex)
    {
        _workAvailable.Release();
    }

    public int ProcessorCount => _processorQueues.Length;
    public int GlobalQueueDepth => _globalQueue.Count;
    public int LocalQueueDepth => _processorQueues.Sum(queue => queue.Count) + _runNext.Count(task => task != null);
    public int WorkPermitCount => _workAvailable.CurrentCount;
    public long StealCount => Interlocked.Read(ref _stealCount);
    public long DispatchCount => Interlocked.Read(ref _dispatchCount);
    public TimeSpan AverageRunnableDelay => TimeSpan.FromTicks(
        DispatchCount == 0 ? 0 : Interlocked.Read(ref _totalRunnableDelayTicks) / DispatchCount);
    public TimeSpan MaximumRunnableDelay => TimeSpan.FromTicks(Interlocked.Read(ref _maximumRunnableDelayTicks));

    public PCodeWaitMetrics GetWaitMetrics(PCodeWaitKind kind)
    {
        if (!_waitMetrics.TryGetValue(kind, out var counters))
            return default;
        return new PCodeWaitMetrics(
            Interlocked.Read(ref counters.Completed),
            Interlocked.Read(ref counters.Cancelled),
            TimeSpan.FromTicks(Interlocked.Read(ref counters.TotalDurationTicks)),
            TimeSpan.FromTicks(Interlocked.Read(ref counters.MaximumDurationTicks)));
    }

    internal void RecordWaitTerminal(PCodeWaitKind kind, bool cancelled, long startedTimestamp)
    {
        var elapsedTicks = System.Diagnostics.Stopwatch.GetElapsedTime(startedTimestamp).Ticks;
        var counters = _waitMetrics.GetOrAdd(kind, static _ => new WaitCounters());
        if (cancelled) Interlocked.Increment(ref counters.Cancelled);
        else Interlocked.Increment(ref counters.Completed);
        Interlocked.Add(ref counters.TotalDurationTicks, elapsedTicks);
        UpdateMaximum(ref counters.MaximumDurationTicks, elapsedTicks);
    }

    /// <summary>Registra I/O avviato da un opcode non bloccante (per esempio FETCH con future o
    /// callback). Non esiste un PCB parcheggiato che possa possedere il token: lo scheduler lo
    /// annulla insieme alle goroutine e la registration viene rimossa dal completamento.</summary>
    public IDisposable RegisterExternalOperation(Action cancel, PCodeTask? owner = null)
    {
        var id = Interlocked.Increment(ref _nextExternalOperationId);
        var operation = new ExternalOperation(owner, cancel);
        _externalOperations[id] = operation;
        // Chiude la race con RequestCancellation(root): se lo snapshot delle operazioni è già
        // passato ma l'owner è stato marcato cancellato, questa registrazione tardiva si annulla
        // da sola. Se invece la registrazione precede lo snapshot, sarà lo snapshot a rimuoverla.
        if (owner?.CancellationRequested == true && _externalOperations.TryRemove(id, out _))
            try { cancel(); } catch { }
        return new ExternalOperationRegistration(this, id);
    }

    public int PendingExternalOperationCount => _externalOperations.Count;

    private sealed class ExternalOperationRegistration(PCodeScheduler owner, long id) : IDisposable
    {
        private PCodeScheduler? _owner = owner;
        public void Dispose() => Interlocked.Exchange(ref _owner, null)?._externalOperations.TryRemove(id, out _);
    }

    public int TaskCount { get { lock (_allTasks) return _allTasks.Count(t => !t.IsFinished); } }

    /// <summary>Numero di PCB ancora posseduti dallo scheduler, incluso un task terminale
    /// soltanto durante la breve finestra fra la fine del tick e il relativo reclaim.</summary>
    public int TrackedTaskCount { get { lock (_allTasks) return _allTasks.Count; } }

    /// <summary>True se esiste almeno un task non ancora Finished (goroutine + main).
    /// Il driver dello scheduler (PCodeVm.Run) usa questo per decidere quando fermarsi.</summary>
    public bool HasActiveTasks { get { lock (_allTasks) return _allTasks.Any(t => !t.IsFinished); } }

    public PCodeTask CreateTask(int startIp = 0, PCodeTask? parent = null)
    {
        var maximumStackCapacity = ResolveMaximumOperandStackCapacity();
        var task = new PCodeTask
        {
            Ip = startIp,
            Parent = parent,
            // Un GO nato durante il FINALLY di cancellazione appartiene ancora alla nursery:
            // come un context Go già cancellato, deve partire cancellato e non eseguire il body.
            CancellationRequested = parent?.CancellationRequested == true,
            Scheduler = this,
            LocalCapacity = LocalCapacity,
            MaximumStackCapacity = maximumStackCapacity
        };
        lock (_allTasks)
        {
            _allTasks.Add(task);
            _allTasksCompleted.Reset();
        }
        return task;
    }

    /// <summary>Rilascia il riferimento forte dello scheduler a un task terminale.
    /// Future, waitgroup e diagnostica devono essere completati dal driver prima della chiamata.</summary>
    public void ReclaimFinishedTask(PCodeTask task)
    {
        if (!task.IsFinished)
            throw new InvalidOperationException("Cannot reclaim a runnable P-Code task.");
        task.SleepUntilMs = 0;
        lock (_allTasks)
        {
            _allTasks.Remove(task);
            if (_allTasks.Count == 0) _allTasksCompleted.Set();
        }
    }

    public void WaitForAllTasks() => _allTasksCompleted.Wait();

    /// <summary>Attesa con TIMEOUT: false = timeout (ci sono ancora task attivi). Usata dal
    /// pump main-thread del host (TRAY): alterna attese brevi e drenaggio della coda TRAY
    /// mentre il BASIC gira sui worker.</summary>
    public bool WaitForAllTasks(int millisecondsTimeout) => _allTasksCompleted.Wait(millisecondsTimeout);

    public void RegisterSleep(PCodeTask task, long deadlineMs, long generation)
    {
        lock (_timerLock)
            _sleepTimers.Enqueue((task, generation, null), deadlineMs);
        // Sveglia il watcher anche quando era parcheggiato senza deadline oppure stava
        // aspettando una deadline successiva. Le entry obsolete restano innocue via generation.
        _timerChanged.Set();
    }


    public void RegisterSleep(PCodeTask task, long deadlineMs, PCodeWaitRegistration registration)
    {
        lock (_timerLock)
            _sleepTimers.Enqueue((task, registration.Generation, registration), deadlineMs);
        _timerChanged.Set();
    }

    public int PendingTimerCount { get { lock (_timerLock) return _sleepTimers.Count; } }

    private static int ResolveMaximumOperandStackCapacity()
    {
        var configured = Environment.GetEnvironmentVariable("GMBASIC_PCODE_MAX_STACK");
        return !string.IsNullOrWhiteSpace(configured) &&
               int.TryParse(configured, out var value) && value > 0
            ? Math.Min(value, 1_048_576)
            : 1_048_576;
    }

    /// <summary>Accoda il task, garantendo che non possa mai comparire due volte nella coda anche
    /// se chiamato da più thread in concorrenza sullo stesso task (es. un send su canale e una
    /// DONE su waitgroup che risvegliano lo stesso task nello stesso istante). Il check-then-act
    /// "non è già in coda? allora entra" è protetto dal lock per-task — senza, due thread
    /// potrebbero vedere entrambi InQueue==false ed entrambi accodare lo stesso PCodeTask, che
    /// verrebbe poi eseguito da due worker insieme.</summary>
    private void Enqueue(PCodeTask task, int processorIndex = -1, bool preferRunNext = false)
    {
        lock (task.EnqueueLock)
        {
            if (task.IsFinished) return;
            // Il worker proprietario sta ancora finendo il tick corrente (vedi PCodeTask.Executing):
            // rimanda l'accodamento, altrimenti un secondo worker riprenderebbe lo stesso task
            // mentre il primo non ha ancora scaricato PendingPrints — vedi ReleaseExecuting.
            if (task.Executing) { task.WakePending = true; return; }
            if (task.InQueue) return;
            task.InQueue = true;
        }
        task.State = PCodeTaskState.Runnable;
        task.RunnableSinceTimestamp = System.Diagnostics.Stopwatch.GetTimestamp();
        if (_shutdown)
        {
            lock (task.EnqueueLock) task.InQueue = false;
            return;
        }

        if (processorIndex >= 0 && processorIndex < _processorQueues.Length)
        {
            if (preferRunNext && Interlocked.CompareExchange(ref _runNext[processorIndex], task, null) == null)
            {
                _workAvailable.Release();
                return;
            }
            _processorQueues[processorIndex].Push(task);
        }
        else
        {
            _globalQueue.Enqueue(task);
        }
        _workAvailable.Release();
    }

    /// <summary>Chiamato dal worker (PCodeProcessor.ExecuteTaskTick, nel finally, DOPO il flush di
    /// PendingPrints) per chiudere la finestra "Executing" aperta da FindRunnableTask. Se nel
    /// frattempo è arrivato un wake che Enqueue ha dovuto rimandare (WakePending), lo accoda solo
    /// ORA — mai perso, mai eseguito in anticipo mentre il tick precedente era ancora in corso.</summary>
    public void ReleaseExecuting(PCodeTask task)
    {
        bool rewake;
        lock (task.EnqueueLock)
        {
            task.Executing = false;
            rewake = task.WakePending;
            task.WakePending = false;
        }
        if (rewake) Enqueue(task);
    }

    /// <summary>Nuovo task (GO SUB/FUNCTION appena creato).</summary>
    public void Schedule(PCodeTask task) => Enqueue(task);

    /// <summary>Task risvegliato da un blocco (canale, waitgroup, sleep scaduto).</summary>
    public void WakeTask(PCodeTask task) => Enqueue(task, task.LastProcessorIndex, preferRunNext: true);

    /// <summary>Task che ha ceduto il controllo a fine tick (OP_YIELD) senza bloccarsi:
    /// torna in fondo alla stessa coda, round-robin puro.</summary>
    public void YieldTask(PCodeTask task, int processorIndex = -1) => Enqueue(task, processorIndex);

    /// <summary>Blocca il thread chiamante (un worker) finché non c'è un task pronto, oppure
    /// finché non arriva la richiesta di cancellazione (fine esecuzione: SignalShutdown+coda
    /// svuotata, o shutdown esterno). Nessun polling: il semaforo conta injection, runnext e
    /// inserimenti nelle code locali.</summary>
    public PCodeTask? FindRunnableTask(CancellationToken ct, int processorIndex = -1)
    {
        while (!ct.IsCancellationRequested)
        {
            // Worker il cui P è stato rimosso da GOMAXPROCS(n) a runtime: deve USCIRE, non restare nel
            // loop di attesa. Un worker "ritirato" ancora in Wait consumava il permesso del semaforo di un
            // wake destinato a un task in runNext/coda locale di un P vivo (che solo quel worker può
            // prendere): permesso perso → task mai ripreso (stallo dopo un park, es. SHELLOUT).
            if (processorIndex >= 0 && IsProcessorRetired(processorIndex)) return null;
            try
            {
                _workAvailable.Wait(ct);
            }
            catch (OperationCanceledException)
            {
                return null;
            }
            if (processorIndex >= 0 && IsProcessorRetired(processorIndex))
            {
                _workAvailable.Release();   // restituisce il permesso a un worker vivo
                return null;
            }

            if (TryTakeRunnable(processorIndex, out var task))
            {
                lock (task!.EnqueueLock)
                {
                    task.InQueue = false;
                    task.Executing = true;
                    task.LastProcessorIndex = processorIndex;
                }
                RecordDispatch(task);
                return task;
            }

            // Wake senza lavoro visibile: Enqueue pubblica il task PRIMA del release del
            // semaforo (ordine push→release), quindi se ci siamo svegliati il lavoro esiste
            // (o è appena stato preso da un altro worker). Su memoria debole (ARM64) la coda
            // può risultare non ancora visibile per una finestra microscopica: ri-scansionare
            // con yield un numero limitato di volte chiude la finestra SENZA perdere il
            // permesso già consumato — altrimenti il permesso va perso e il task resta in coda
            // con semaforo a 0 (deadlock intermittente osservato a N alto × GOMAXPROCS alto).
            for (var retry = 0; retry < 2048; retry++)
            {
                Thread.Yield();
                if (TryTakeRunnable(processorIndex, out task))
                {
                    lock (task!.EnqueueLock)
                    {
                        task.InQueue = false;
                        task.Executing = true;
                        task.LastProcessorIndex = processorIndex;
                    }
                    RecordDispatch(task);
                    return task;
                }
            }

            if (_shutdown)
                return null;
        }
        return null;
    }

    private bool TryTakeRunnable(int processorIndex, out PCodeTask? task)
    {
        task = null;
        if (processorIndex >= 0 && processorIndex < _processorQueues.Length)
        {
            task = Interlocked.Exchange(ref _runNext[processorIndex], null);
            if (task != null) return true;
        }

        // Dopo l'eventuale runnext, l'injection globale ha precedenza sulla coda locale: un task
        // CPU-bound che si preempte continuamente non può affamare una nuova goroutine.
        if (_globalQueue.TryDequeue(out task)) return true;

        if (processorIndex >= 0 && processorIndex < _processorQueues.Length)
        {
            if (_processorQueues[processorIndex].TryPop(out task)) return true;
            // runnext degli ALTRI processor: il semaforo _workAvailable è globale, quindi un
            // permesso rilasciato per lo slot runnext di P può essere consumato da un worker W≠P.
            // W non può vedere lo slot altrui → torna ad attendere col permesso sprecato e il task
            // resta intrappolato nel runnext di P (deadlock a GOMAXPROCS≥2). Scansionando TUTTI gli
            // slot runnext qui, qualunque worker svegliato può soddisfare quel permesso: l'affinità
            // runnext resta una preferenza (lo slot proprio è controllato per primo), non più un
            // vincolo esclusivo che può perdere i wake.
            for (var offset = 1; offset < _processorQueues.Length; offset++)
            {
                var other = (processorIndex + offset) % _processorQueues.Length;
                task = Interlocked.Exchange(ref _runNext[other], null);
                if (task != null) return true;
            }
            for (var offset = 1; offset < _processorQueues.Length; offset++)
            {
                var victim = (processorIndex + offset) % _processorQueues.Length;
                if (!_processorQueues[victim].TrySteal(out task)) continue;
                Interlocked.Increment(ref _stealCount);
                return true;
            }
        }
        return false;
    }

    private void RecordDispatch(PCodeTask task)
    {
        var started = task.RunnableSinceTimestamp;
        task.RunnableSinceTimestamp = 0;
        if (started == 0)
            return;
        var elapsedTicks = System.Diagnostics.Stopwatch.GetElapsedTime(started).Ticks;
        Interlocked.Increment(ref _dispatchCount);
        Interlocked.Add(ref _totalRunnableDelayTicks, elapsedTicks);
        UpdateMaximum(ref _maximumRunnableDelayTicks, elapsedTicks);
    }

    private static void UpdateMaximum(ref long location, long candidate)
    {
        var current = Volatile.Read(ref location);
        while (candidate > current)
        {
            var observed = Interlocked.CompareExchange(ref location, candidate, current);
            if (observed == current) return;
            current = observed;
        }
    }

    /// <summary>Segnala che non arriverà più nuovo lavoro e sblocca i worker in attesa.</summary>
    public void SignalShutdown()
    {
        _shutdown = true;
        for (var i = 0; i < Math.Max(1, _processorQueues.Length); i++)
            _workAvailable.Release();
    }

    /// <summary>Estrae dal min-heap soltanto i timer scaduti. Generation rende innocue le entry
    /// superate da un nuovo SLEEP, da un wake anticipato o dal completamento del task.</summary>
    public void WakeExpiredSleepers(long nowMs)
    {
        var expired = new List<(PCodeTask Task, long Generation, PCodeWaitRegistration? Registration, long Deadline)>();
        lock (_timerLock)
        {
            while (_sleepTimers.TryPeek(out var entry, out var deadline) && deadline <= nowMs)
            {
                _sleepTimers.Dequeue();
                expired.Add((entry.Task, entry.Generation, entry.Registration, deadline));
            }
        }
        foreach (var timer in expired)
        {
            if (timer.Registration != null)
            {
                timer.Registration.TryComplete(() => timer.Task.SleepUntilMs = 0);
                continue;
            }
            var shouldWake = false;
            lock (timer.Task.EnqueueLock)
            {
                shouldWake = !timer.Task.IsFinished &&
                             timer.Task.State == PCodeTaskState.Blocked &&
                             timer.Task.SleepGeneration == timer.Generation &&
                             timer.Task.SleepUntilMs == timer.Deadline;
                if (shouldWake) timer.Task.SleepUntilMs = 0;
            }
            if (shouldWake) WakeTask(timer.Task);
        }
    }

    /// <summary>Istante (ms epoch UTC) del prossimo risveglio SLEEP pendente, o null se
    /// nessun task sta dormendo. Usato dal driver per capire quanto puo' dormire il thread
    /// sleep-watcher quando non c'e' nessun task addormentato.</summary>
    public long? EarliestSleepDeadline()
    {
        lock (_timerLock)
        {
            while (_sleepTimers.TryPeek(out var entry, out var deadline))
            {
                if (entry.Registration != null)
                {
                    if (!entry.Task.IsFinished && entry.Registration.IsPending &&
                        entry.Task.SleepUntilMs == deadline)
                        return deadline;
                    _sleepTimers.Dequeue();
                    continue;
                }
                if (!entry.Task.IsFinished &&
                    entry.Task.SleepGeneration == entry.Generation &&
                    entry.Task.SleepUntilMs == deadline)
                    return deadline;
                _sleepTimers.Dequeue();
            }
            return null;
        }
    }

    /// <summary>Parcheggia il watcher fino a una nuova/variata deadline, alla scadenza corrente
    /// o allo shutdown. Evita la scansione periodica anche quando non esistono timer.</summary>
    public void WaitForTimerChange(int timeoutMs, CancellationToken cancellationToken)
    {
        WaitHandle.WaitAny(
            new[] { _timerChanged, cancellationToken.WaitHandle },
            timeoutMs < 0 ? Timeout.Infinite : timeoutMs);
    }

    public List<PCodeTask> GetTasks()
    {
        lock (_allTasks)
            return _allTasks.Where(t => !t.IsFinished).ToList();
    }

    private long _lastDeadlockCheckMs = -1;
    private int _deadlockStreak;

    /// <summary>Segnale armato da CheckDeadlock per un consumatore esterno che possiede la
    /// sequenza di cancel CORRETTA per questo backend (es. IlGpmV2Driver.RunV2Inline, che sa
    /// cancellare l'IlActivationFrame.Await specifico di ogni task — un
    /// RequestCancellation() generico non basta, sveglia i task senza cancellare il loro
    /// await e li fa rimbloccare subito). Il consumatore lo resetta a false dopo aver
    /// reagito.</summary>
    public bool DeadlockDetected { get; set; }

    /// <summary>Kind di wait che NON contano come "nessuno può più fare progressi": dipendono
    /// da qualcosa fuori dallo scheduler (un tasto premuto dall'utente, una risposta HTTP, un
    /// evento grafico) che può arrivare in qualunque momento indipendentemente dagli altri
    /// task — mai una condizione di deadlock. Scoperto 2026-09-08: senza questo filtro un
    /// programma legittimamente fermo su WAITKEY veniva ucciso come "deadlockato" dopo ~2s.</summary>
    private static readonly PCodeWaitKind[] ExternalWaitKinds =
        [PCodeWaitKind.Network, PCodeWaitKind.Http, PCodeWaitKind.Input, PCodeWaitKind.Graphics,
         PCodeWaitKind.Cpu, PCodeWaitKind.External];

    /// <summary>Kind di wait risolvibili SOLO da un altro task dello stesso scheduler (mai da
    /// un timeout o da un evento esterno) — usato da CancelTasks per NON forzarne la
    /// cancellazione diretta (lascia che chi li soddisfa, es. un GO SUB...IN wg che si sblocca
    /// su un'attesa esterna cancellata e chiama DoneAndWake, vinca la race — vedi CancelTasks).
    /// Deliberatamente DIVERSO da ExternalWaitKinds (quello serve al watchdog deadlock, dove
    /// Timer/Sleep è già escluso a parte via EarliestSleepDeadline): Timer QUI resta
    /// cancellabile perché nessun altro task può "completarlo" prima della scadenza — solo un
    /// timeout o una cancellazione esterna lo sblocca.</summary>
    private static readonly PCodeWaitKind[] InternalCooperativeWaitKinds =
        [PCodeWaitKind.Channel, PCodeWaitKind.WaitGroup, PCodeWaitKind.Future, PCodeWaitKind.Select,
         PCodeWaitKind.Mutex, PCodeWaitKind.Once, PCodeWaitKind.Cond];

    /// <summary>Watchdog deadlock (parità Go "fatal error: all goroutines are asleep -
    /// deadlock!"): se TUTTI i task non finiti sono Blocked (nessuno Runnable/Running/
    /// Syscall) su un'attesa PURAMENTE interna (mai su Network/Http/Input/Graphics/Cpu/
    /// External — vedi ExternalWaitKinds) e non c'è nessun timer SLEEP pendente, nessuno potrà
    /// MAI più fare progressi da solo — es. SEND senza un RECEIVE corrispondente, o WAIT su un
    /// WAITGROUP mai completato. Il chiamante (SleepWatcherLoop/SharedPollerLoop) lo invoca a
    /// ogni giro della propria cadenza — qui internamente throttlato (un controllo reale ogni
    /// ~200ms) e richiede la stessa condizione confermata per 2 controlli consecutivi prima di
    /// dichiarare deadlock, per escludere una race transitoria (un altro thread sta per
    /// svegliare un task). Ritorna true UNA SOLA VOLTA quando scatta (il chiamante deve
    /// reagire, es. RequestCancellation()).</summary>
    public bool CheckDeadlock(long nowMs)
    {
        if (EarliestSleepDeadline() != null) { _deadlockStreak = 0; return false; }
        if (_lastDeadlockCheckMs >= 0 && nowMs - _lastDeadlockCheckMs < 200) return false;
        _lastDeadlockCheckMs = nowMs;
        var tasks = GetTasks();
        if (tasks.Count == 0
            || tasks.Any(t => t.State != PCodeTaskState.Blocked)
            || tasks.Any(t => t.PendingWaitKind is { } k && ExternalWaitKinds.Contains(k)))
        {
            _deadlockStreak = 0;
            return false;
        }
        if (++_deadlockStreak < 2) return false;
        _deadlockStreak = 0;
        return true;
    }

    public void RequestCancellation()
    {
        CancelTasks(GetTasks());
        CancelExternalOperations();
    }

    /// <summary>Annulla una nursery implicita: root e tutti i discendenti GO, senza toccare
    /// handler/server root o altre goroutine indipendenti dello stesso scheduler.</summary>
    public void RequestCancellation(PCodeTask root)
    {
        var all = GetTasks();
        var subtree = new HashSet<PCodeTask> { root };
        var changed = true;
        while (changed)
        {
            changed = false;
            foreach (var candidate in all)
            {
                if (candidate.Parent != null && subtree.Contains(candidate.Parent) && subtree.Add(candidate))
                    changed = true;
            }
        }
        CancelTasks(subtree);
        CancelExternalOperations(operation => IsDescendantOrSelf(operation.Owner, root));
    }

    private static bool IsDescendantOrSelf(PCodeTask? candidate, PCodeTask root)
    {
        for (var current = candidate; current != null; current = current.Parent)
            if (ReferenceEquals(current, root)) return true;
        return false;
    }

    private void CancelTasks(IEnumerable<PCodeTask> tasks)
    {
        foreach (var task in tasks)
        {
            // Un'attesa PURAMENTE interna (Channel/WaitGroup/Future/Select/Mutex/Once/Cond —
            // vedi PendingWaitKind) si sblocca SOLO per azione di un altro task nello stesso
            // scheduler — MAI direttamente qui: forzarne la cancellazione (o anche solo
            // marcare CancellationRequested/WakeTask, che fa scattare BeginCancellationUnwind
            // al prossimo tick — vedi ExecuteTaskSlice) vince sempre la race con chi la sta per
            // soddisfare (es. un GO SUB...IN wg che sta per unwindare su un'attesa ESTERNA
            // cancellata in QUESTO stesso giro e chiamare DoneAndWake sul WaitGroup del
            // chiamante). Se la sua attesa è interna e NON ancora soddisfatta, il task resta
            // TOCCATO SOLO se non ha un'attesa pending: altrimenti si lascia intatto, a
            // drenare per via normale (TryComplete di chi lo soddisfa chiamerà WakeTask da
            // solo). Tutte le altre attese (Network/Http/Input/Graphics/Cpu/External, e Timer/
            // Sleep — quest'ultimo NON è "interno cooperativo": nessun altro task può
            // completarlo prima della scadenza, solo un timeout o una cancellazione lo sblocca)
            // restano cancellate direttamente. Scoperto dal vivo:
            // WaitKeyPark_DoesNotBlockWorkerAndAbortsOnCancel — il WAIT wg del chiamante
            // falliva sempre con "P-Code task cancelled" invece di completare quando il WAITKEY
            // parcheggiato veniva cancellato e il suo DONE implicito (GO SUB...IN wg) drenava
            // il WaitGroup. Il deadlock watchdog (CheckDeadlock) resta il backstop per un'attesa
            // interna genuinamente mai soddisfatta da nessuno.
            if (task.PendingWaitKind is { } waitKind && InternalCooperativeWaitKinds.Contains(waitKind))
                continue;
            task.CancellationRequested = true;
            task.PendingSelect?.Cancel();
            task.PendingSelect = null;
            task.CancelPendingWait();
            task.SleepUntilMs = 0;
            WakeTask(task);
        }
        _timerChanged.Set();
    }

    private void CancelExternalOperations(Func<ExternalOperation, bool>? predicate = null)
    {
        foreach (var (id, operation) in _externalOperations.ToArray())
        {
            if ((predicate == null || predicate(operation)) && _externalOperations.TryRemove(id, out _))
                try { operation.Cancel(); } catch { }
        }
    }

    public void Dispose()
    {
        CancelExternalOperations();
        SignalShutdown();
        while (_globalQueue.TryDequeue(out _)) { }
        foreach (var queue in _processorQueues) queue.Clear();
        Array.Clear(_runNext);
        _workAvailable.Dispose();
        _timerChanged.Dispose();
        _allTasksCompleted.Dispose();
        _allTasks.Clear();
        lock (_timerLock) _sleepTimers.Clear();
    }
}
