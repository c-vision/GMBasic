// FILE: Basic.IlBackend.PCode.PCodeVm.cs
// P-CODE VM — GPM con parallelismo reale: N worker thread OS (GOMAXPROCS, default
// Environment.ProcessorCount, override GMBASIC_GOMAXPROCS) eseguono PCodeTask CONTEMPORANEAMENTE
// tramite P logici con injection globale, runnext, code locali e work stealing. A differenza dell'AST (GOMAXPROCS=1
// hardcoded, Executor.cs:298), qui più goroutine girano davvero in parallelo su core diversi —
// da cui i lock per-task/per-oggetto in GpmCore.cs/GpmChannels.cs. GMBASIC_GOMAXPROCS=1 riporta
// al comportamento deterministico single-thread (debug, rigenerazione golden file).

using System.Collections.Concurrent;
using System.Threading;
using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed class PCodeVm : IDisposable
{
    private readonly PCodeScheduler _scheduler;
    private readonly PCodeTask _mainTask;
    private readonly PCodeProcessor _processor;
    private readonly int _instructionQuantum = ResolveInstructionQuantum();
    private readonly object _workersLock = new();
    private Thread[] _workers = [];
    private CancellationTokenSource? _workerCts;

    /// <summary>Eccezioni non gestite di QUALUNQUE task (non solo _mainTask), raccolte per
    /// diagnostica — non cambia la semantica di propagazione esistente (vedi ExecutionException),
    /// è una superficie puramente additiva per chi vuole visibilità completa su fallimenti
    /// concorrenti di più goroutine indipendenti.</summary>
    private readonly ConcurrentBag<(PCodeTask Task, Exception Ex)> _allTaskExceptions = new();
    public IReadOnlyCollection<(PCodeTask Task, Exception Ex)> AllTaskExceptions => _allTaskExceptions;

    /// <summary>Exception from P-CODE execution, if any.</summary>
    public Exception? ExecutionException => _mainTask.PendingException;

    /// <summary>Task principale: esegue il chunk. Esposto per il RIUso della VM (filtri database
    /// per-record — vedi DbExprPCodeEvaluator): il chiamante resetta Ip/State/Sp/Globals tra una
    /// valutazione e l'altra senza allocare una nuova VM per record.</summary>
    public PCodeTask MainTask => _mainTask;

    /// <summary>Processor della VM — esposto per installare il provider genitore S_eval
    /// (EVAL annidato: MacroParentKnown/Get/Set) prima di Run/RunSync.</summary>
    internal PCodeProcessor Processor => _processor;

    /// <summary>S_eval §ii: tabella host-procs per OP_CALL_HOST nelle macro (in-memory,
    /// MAI serializzata). Null = radice senza host (chiamata = errore esplicito).</summary>
    internal List<Compiler.MacroHostProc>? MacroHostTable { get; set; }

    /// <summary>S_eval §ii: invocatore sincrono per OP_CALL_HOST (indice tabella + args).
    /// PCodeVM: sync-pump sul processore ospite; IL: registro CLR. Null = non chiamabile.</summary>
    internal Func<int, object?[], object?>? MacroHostInvoker { get; set; }

    public PCodeVm(CompiledChunk chunk, IRuntimeHost runtime)
    {
        // P0a2 (todo Tokenizer-WordPiece, decisione utente 2026-09-12): RIMOSSA l'installazione
        // incondizionata del FixtureEmbeddingProvider (e di ActiveDimensions=8). Prima: la prima
        // VM del processo installava il fixture per sempre e ogni ramo `if (Default is null)`
        // a valle (IlRuntime, REPL/CLI) era morto — AI RAG serviva pseudo-semantica stabile
        // senza dirlo. Ora il resolver resta null finché il backend IL o il REPL non cablino un
        // provider REALE, e le superfici AI RAG senza modello falliscono con errore esplicito
        // (AiVectorRuntime.WithCollection). I test che vogliono il fixture lo impostano a mano
        // (già tutti i siti in tests/ lo fanno).
        _scheduler = new PCodeScheduler();
        _scheduler.LocalCapacity = chunk.LocalCount > 0 ? chunk.LocalCount : 256;
        int startIp = chunk.EntryPoint > 0 ? chunk.EntryPoint : 0;
        _mainTask = _scheduler.CreateTask(startIp);
        _mainTask.Scheduler = _scheduler;
        // Unico resize dell'array Globals per l'intera esecuzione, qui, prima che qualunque
        // worker parta — vedi CompiledChunk.GlobalCount per il perché (elimina la race di resize
        // sotto vero parallelismo, non la mitiga soltanto).
        if (chunk.GlobalCount > 0)
            _mainTask.EnsureGlobals(chunk.GlobalCount);
        _processor = new PCodeProcessor(runtime, chunk, _scheduler, new object(), this);
        /* Slot locali numerici del root (top-level/Main) a 0.0: parità IL/C99Native. */
        _processor.InitRootNumericLocals(_mainTask, startIp);
    }

    /// <summary>Imposta un valore globale nel task principale prima dell'esecuzione.</summary>
    public void SetGlobal(int index, object? value)
    {
        _mainTask.EnsureGlobals(index + 1);
        _mainTask.Globals![index] = value;
    }

    /// <summary>Legge un valore globale dal task principale — usato dal chiamante DOPO
    /// l'esecuzione per rileggere gli effetti collaterali su variabili globali "catturate" dal
    /// programma ospite (vedi IlRuntime.MacroEvalPCode / Compiler.ExternalGlobalProbe).</summary>
    public object? GetGlobal(int index)
    {
        var globals = _mainTask.Globals;
        return globals != null && index >= 0 && index < globals.Length ? globals[index] : null;
    }

    /// <summary>Semina un valore nel "dynamic scope" per NOME (FORM/QUERY nelle pagine BSP —
    /// keyword non dichiarabili con DIM, quindi mai un vero slot globale). Va chiamato PRIMA di
    /// Run(). Vedi PCodeProcessor.SetDynamicScope/OP_LOAD_DYNAMIC_SCOPE.</summary>
    public void SetDynamicScope(string name, object? value) => _processor.SetDynamicScope(name, value);

    /// <summary>Legge il valore in cima allo stack del task principale dopo Run() — usato per
    /// leggere il risultato di un chunk compilato come singola espressione (OP_RETURN lascia il
    /// valore dell'espressione li', vedi Compiler.CompileExpressionBody), es. IlRuntime.MacroEvalExpr
    /// per `&amp;expr` usato come valore.</summary>
    public object? PeekResult() => _mainTask.PeekTop();

    /// <summary>
    /// Esegue il task principale FINO A COMPLETAMENTO sul thread CHIAMANTE, senza scheduler
    /// multi-thread — nessun worker OS, nessun sleep-watcher. <see cref="ExecuteTaskTick"/>
    /// (vedi PCodeProcessor.cs) gira in loop finché il task non finisce o non incontra un
    /// opcode di yield/blocco; per un'espressione/condizione semplice (nessun GO SUB/canale/
    /// SLEEP — mai atteso in un filtro database o in una macro) finisce sempre in questa unica
    /// chiamata. Usato per casi valutati potenzialmente MOLTE volte (filtri database per-record,
    /// vedi DbExprPCodeEvaluator): spawnare GOMAXPROCS thread OS a ogni chiamata (<see cref="Run"/>)
    /// sarebbe proibitivo per uno scan di migliaia di record. Se il task si blocca/va in yield,
    /// questo metodo si ferma senza completare (IsFinished resta false, non supportato per questo
    /// caso d'uso).
    /// </summary>
    public void RunSync() => _processor.ExecuteTaskTick(_mainTask);

    /// <summary>Driver: avvia GOMAXPROCS worker thread reali, uno per P logico, ed esegue un tick
    /// alla volta su injection globale, coda locale o lavoro rubato — più PCodeTask possono
    /// quindi girare SIMULTANEAMENTE su core diversi, a differenza del vecchio modello single-
    /// thread. Un thread separato (SleepWatcherLoop) gestisce i risvegli SLEEP, disaccoppiato dai
    /// worker così restano dedicati al 100% all'esecuzione dei tick. Il thread chiamante attende
    /// che tutti i task finiscano (HasActiveTasks==false), poi segnala lo shutdown e joina tutto.</summary>
    public int ProcessorCount => _scheduler.ProcessorCount;

    public void Run(int maxInstructions = 5000000)
    {
        Run(CancellationToken.None);
    }

    public void Run(CancellationToken externalCt)
    {
        int workerCount = ResolveGoMaxProcs();
        _scheduler.ConfigureProcessors(workerCount);
        _scheduler.Schedule(_mainTask);

        using var workerCts = new CancellationTokenSource();
        _workerCts = workerCts;
        var ct = workerCts.Token;
        var workers = new Thread[workerCount];
        for (int i = 0; i < workerCount; i++)
        {
            var processorIndex = i;
            // Stack AMPIO (32MB): il load del motore MLX/Metal (LLM QUALITY / REGISTRY MEASURE) e'
            // ricorsivo-profondo e con lo stack di default (~1MB) fa StackOverflow sul worker
            // (misurato 2026-10-08 sulla PPL reale). Gli altri backend (IL/VM-C/C99) girano su
            // stack piu' ampio, per questo non lo mostravano.
            var worker = new Thread(() => WorkerLoop(processorIndex, ct), 32 * 1024 * 1024) { IsBackground = true, Name = $"pcode-worker-{i}" };
            workers[i] = worker;
            worker.Start();
        }
        var sleepWatcher = new Thread(() => SleepWatcherLoop(ct)) { IsBackground = true, Name = "pcode-sleep-watcher" };
        sleepWatcher.Start();

        lock (_workersLock) _workers = workers;

        using var cancellationRegistration = externalCt.Register(_scheduler.RequestCancellation);
        _scheduler.WaitForAllTasks();

        workerCts.Cancel();
        _scheduler.SignalShutdown();
        lock (_workersLock) workers = _workers;
        foreach (var w in workers) w.Join();
        sleepWatcher.Join();
    }

    /// <summary>Parità Go `runtime.GOMAXPROCS(n)`: riconfigura a runtime il numero di P logici.
    /// Se il nuovo numero supera quello attuale, avvia nuovi worker thread; se è inferiore,
    /// i P eccedenti vengono rimossi (i loro task migrati alla coda globale dallo scheduler) e
    /// i worker corrispondenti terminano al prossimo controllo. Restituisce il valore precedente.</summary>
    public int SetGoMaxProcs(int newCount)    {
        if (newCount <= 0)
            throw new ArgumentOutOfRangeException(nameof(newCount));
        var previous = _scheduler.ProcessorCount;
        lock (_workersLock)
        {
            _scheduler.ConfigureProcessors(newCount);
            if (_workerCts is not { } cts) return previous; // non in esecuzione multi-worker
            var current = _workers;
            var active = Math.Min(newCount, current.Length);
            // Termina i worker eccedenti: li sveglia perché controllino IsProcessorRetired.
            for (var i = 0; i < current.Length; i++)
                _scheduler.WakeRetiredWorkerCheck(i);
            // Avvia nuovi worker per i P aggiunti.
            if (newCount > current.Length)
            {
                var grown = new Thread[newCount];
                Array.Copy(current, grown, current.Length);
                for (var i = current.Length; i < newCount; i++)
                {
                    var processorIndex = i;
                    var worker = new Thread(() => WorkerLoop(processorIndex, cts.Token), 32 * 1024 * 1024)
                    { IsBackground = true, Name = $"pcode-worker-{i}" };
                    grown[i] = worker;
                    worker.Start();
                }
                _workers = grown;
            }
        }
        return previous;
    }

    private static int ResolveGoMaxProcs()
    {
        var env = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        if (!string.IsNullOrEmpty(env) && int.TryParse(env, out var n) && n > 0)
            return n;
        return Math.Max(1, Environment.ProcessorCount);
    }

    private static int ResolveInstructionQuantum()
    {
        var env = Environment.GetEnvironmentVariable("GMBASIC_PCODE_QUANTUM");
        if (!string.IsNullOrEmpty(env) && int.TryParse(env, out var value) && value > 0)
            return Math.Min(value, 1_000_000);
        return 4_096;
    }

    private void WorkerLoop(int processorIndex, CancellationToken ct)
    {
        // NON marcare questo thread con IlGpmRuntime.MarkDriverThread: a differenza di IL v2
        // (che ha un percorso "flat" separato per i programmi senza GO SUB), PCodeVm.Run()
        // usa QUESTO STESSO WorkerLoop anche per il singolo _mainTask di un programma senza
        // alcuna concorrenza (vedi commento su PCodeVmRunner.RunWithGraphicsPump: "PCodeVm.Run()
        // spawna i propri worker thread... esattamente come IlGpmDriver.Run()" — ma qui e'
        // l'UNICO percorso, mai un fallback sincrono). Marcare staticamente per-thread
        // produrrebbe un FALSO POSITIVO su qualunque .bas esistente che chiama
        // AI.AGENTSWARM/BLACKBOARDWAIT/AgentDag dal Main sequenziale (nessun GO SUB in vista) —
        // verificato con /tmp/gpm_guard_repro3.bas: la guardia scattava anche fuori da una
        // goroutine. La versione corretta per PCodeVM richiede un controllo DINAMICO ("ci sono
        // davvero altri task attivi in questo scheduler ORA", non "sono su un worker thread") —
        // non implementato qui, vedi Bugs/(partial) - AgentBatch-AiAgentSwarm-Conflitto-GPM.md.
        while (!ct.IsCancellationRequested)
        {
            // Riconfigurazione runtime dei P (parità Go GOMAXPROCS): un worker il cui P è stato
            // rimosso termina, dopo aver lasciato che lo scheduler migri i task della sua coda
            // locale alla coda globale. I worker restanti continuano con i loro P.
            if (_scheduler.IsProcessorRetired(processorIndex))
                return;
            var task = _scheduler.FindRunnableTask(ct, processorIndex);
            if (task == null) continue; // cancellato, o coda completata e svuotata
            task.ClearPendingWaitCancellation();
            task.State = PCodeTaskState.Running;
            var result = _processor.ExecuteTaskSlice(task, _instructionQuantum);
            HandlePostTick(task, result, processorIndex);
        }
    }

    private void SleepWatcherLoop(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            _scheduler.WakeExpiredSleepers(NowMs());
            // Watchdog deadlock (parità Go): senza nessun timer pendente questo giro
            // aspetterebbe Timeout.Infinite (progettato così per evitare scansioni
            // periodiche inutili) — ma allora non riproveremmo mai a controllare un
            // deadlock. Cap a 1s SOLO in quel caso specifico (nessun impatto quando un
            // timer reale è già pendente, il caso comune): overhead trascurabile, ma
            // basta a rilevare un programma completamente bloccato in pochi secondi
            // invece di restare appeso per sempre.
            if (_scheduler.CheckDeadlock(NowMs()))
            {
                Console.Error.WriteLine("fatal error: all goroutines are asleep - deadlock!");
                Console.Error.WriteLine("[GMBASIC] nessun task puo' piu' fare progressi (nessun task runnable/in " +
                    "esecuzione, nessun timer in attesa) -- probabile SEND senza un RECEIVE corrispondente, o un " +
                    "WAIT su un WAITGROUP mai completato.");
                _scheduler.RequestCancellation();
            }
            var nextWake = _scheduler.EarliestSleepDeadline();
            var waitMs = nextWake == null
                ? 1000
                : (int)Math.Clamp(nextWake.Value - NowMs(), 0, int.MaxValue);
            _scheduler.WaitForTimerChange(waitMs, ct);
        }
    }

    /// <summary>Instrada il task dopo un tick, chiamato da un thread worker qualsiasi: Finished →
    /// registra l'eventuale eccezione non gestita per diagnostica (AllTaskExceptions) e risolve
    /// l'eventuale future collegato (GO FUNCTION...INTO, vedi OP_GO_FUNCTION) col valore lasciato
    /// in cima al suo stack da RETURN — o con Fail() se il task è terminato con un'eccezione non
    /// gestita, così un lettore bloccato su questo future viene comunque svegliato invece di
    /// restare in attesa per sempre; Blocked → niente (già registrato sulla struttura di attesa
    /// pertinente — canale, waitgroup, sleep, future — verrà risvegliato esplicitamente da chi lo
    /// sblocca, potenzialmente un ALTRO worker thread); altrimenti (il tick è finito con un
    /// OP_YIELD, non un blocco) → torna in coda per il prossimo giro.</summary>
    private void HandlePostTick(PCodeTask task, PCodeTickResult result, int processorIndex)
    {
        if (task.IsFinished)
        {
            task.PendingSelect?.Cancel();
            task.PendingSelect = null;
            if (task.PendingException != null)
                _allTaskExceptions.Add((task, task.PendingException));
            if (task.FutureToResolve is { } future)
            {
                if (task.CancellationUnwindStarted && task.PendingException is OperationCanceledException)
                    future.Cancel(_scheduler);
                else if (task.PendingException != null)
                    future.Fail(task.PendingException, _scheduler);
                else
                    future.Resolve(task.PeekTop(), _scheduler);
            }
            // GO SUB ... IN wg: Done implicito alla terminazione del task.
            if (task.WaitGroup is Basic.Runtime.Gpm.PCodeWaitGroup pwgDone)
                pwgDone.DoneAndWake(_scheduler);
            _scheduler.ReclaimFinishedTask(task);
            return;
        }
        // wasYield è un valore LOCALE restituito da ExecuteTaskTick, mai un campo condiviso sul
        // task (vedi ExecuteTaskTick): un campo come PCodeTask.LastTickWasYield (usato in una
        // versione precedente di questo fix) resta scritturabile da QUALUNQUE thread che in
        // seguito esegua un ALTRO tick dello stesso task PRIMA che questo metodo giri — sotto
        // vero parallelismo, se il task si blocca e viene svegliato e ri-eseguito da un altro
        // worker abbastanza in fretta (la finestra fra "ExecuteTaskTick ritorna" e "HandlePostTick
        // gira" può essere preemptata dall'OS), questo thread finirebbe per leggere l'esito del
        // tick SUCCESSIVO (magari un OP_YIELD) invece del PROPRIO (un blocco) — riaccodando il
        // task una SECONDA volta mentre è già in esecuzione altrove (bug reale, causa di
        // corruzione/hang intermittenti sotto stress in GoRoutinesWorkers.bas: due thread
        // eseguivano davvero la stessa PCodeTask in contemporanea, IP/stack corrotti). Un valore
        // di ritorno locale sullo stack del chiamante non è mai soggetto a questa razza.
        if (result is not (PCodeTickResult.Yielded or PCodeTickResult.Preempted))
            return;
        task.State = PCodeTaskState.Runnable;
        _scheduler.YieldTask(task, processorIndex);
    }

    private static long NowMs() => DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

    public void Dispose()
    {
        // Rilascia i DB aperti (SPEED/ISAM/DBF/SQL/LEDGER) a fine run: il WAL SPEED usa
        // FileShare.None e senza dispose resterebbe locked per il run successivo nello
        // stesso processo (harness di parità). Vedi PCodeProcessor.CloseAllDatabases.
        try { _processor.CloseAllDatabases(); } catch { /* best effort */ }
        try { _processor.DisposeIoWatchers(); } catch { /* best effort */ }
        // Server HTTP rimasto attivo (STOP non chiamato / non completato): senza questo il
        // TcpListener resta legato alla porta e il run successivo nello STESSO processo
        // (harness MiniTest) non riesce a Bind → richieste in TIMEOUT.
        try { _processor.StopHttpServerSync(); } catch { /* best effort */ }
        _scheduler?.Dispose();
    }
}
