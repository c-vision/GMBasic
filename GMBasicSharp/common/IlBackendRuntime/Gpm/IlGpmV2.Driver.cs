// FILE: Basic.IlBackend.Gpm.IlGpmV2.Driver.cs
// PARTIAL per dominio (split da IlGpmV2.cs — direttiva 1200 righe): driver v2 — esecuzione
// dei task, apply delle transizioni, poller condiviso, worker loop. Stesso namespace di
// IlGpmV2; nessuna semantica nuova.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Gpm;

    public sealed class IlGpmV2Driver
    {
        private IlActivationFrame? _current;

        /// <summary>Il driver possiede SEMPRE il frame root (procedura di ingresso, tipicamente
        /// Main): le Call vengono impilate sopra di esso e CompleteTask/Return della root
        /// completano il task. Per default la root è la procedura 0.</summary>
        private readonly bool _captureRootExitCode;

        public IlGpmV2Driver(int rootProcedureId = 0, bool captureRootExitCode = false)
        {
            _captureRootExitCode = captureRootExitCode;
            _current = new IlActivationFrame
            {
                ProcedureId = rootProcedureId,
                Pc = 0,
                Caller = null,
                ReturnPc = 0,
                ReturnDestination = IlReturnDestination.Discard
            };
        }

        public IlActivationFrame? CurrentFrame => _current;

        /// <summary>Un tick v2: assicura il frame root del task, dispatch sullo stepper
        /// StepV2_&lt;id&gt; del frame corrente, applica la transizione e switch dell'esito
        /// (stesso contratto del path flat §9.3). Il frame chain vive su task.CurrentIlFrame
        /// (composizione): ogni task ha la propria call chain.</summary>
        public void ExecuteTaskTick(PCodeTask task, IlGpmV2Dispatcher dispatcher)
        {
            if (task.CurrentIlFrame is not IlActivationFrame frame)
            {
                var owner = task.Frame?.OwnerProcedure ?? -1;
                var locals = task.Frame?.LocalVariables;
                _current = new IlActivationFrame
                {
                    ProcedureId = owner,
                    Pc = 0,
                    Caller = null,
                    ReturnPc = 0,
                    ReturnDestination = IlReturnDestination.Discard,
                    Locals = locals is { Length: > 0 } ? locals : new object?[16]
                };
                task.CurrentIlFrame = _current;
            }
            else
            {
                _current = frame;
            }

            // G8: run-to-suspend — esegue i block CONTINUE consecutivi (flat, loop, call chain)
            // nello STESSO tick SENZA ri-dispatch dello scheduler, con un QUANTUM di step per
            // la fairness (un loop CPU-bound senza park non deve affamare le altre goroutine).
            // Un YIELD esplicito (IlStepKind.Yield) cede SUBITO. Il bug esposto (TryReceive che
            // non svegliava il sender) è stato FIXATO in GpmChannels (commit in preparazione).
            if (Basic.Providers.GmbEnv.Get("GMB_NORUNSUSPEND") == "1") { }
            var steps = 0;
            while (true)
            {
                var stepper = dispatcher.Dispatch(_current.ProcedureId)
                    ?? dispatcher.Dispatch(_current.ProcedureId, _current.ClosureContext);
                if (stepper is null)
                {
                    // G8: goroutine NON-may-park (nessun StepV2_<id>): eseguila via la Proc_<id>
                    // FLAT generata per ogni procedura e completa il task (wg.Done al termine).
                    // Parità Go: `go f()` con f senza park gira e termina.
                    IlGpmRuntime.RunFlatGoroutine(task, _current.ProcedureId, dispatcher.SourceAssembly);
                    HandleFinishedV2(task);
                    return;
                }
                var transition = stepper(task, _current);
                task.CurrentIlFrame = _current;
                if (transition.Kind == IlStepKind.Yield)
                {
                    Apply(task, transition);
                    task.Scheduler?.YieldTask(task);
                    return;
                }
                var result = Apply(task, transition);
                task.CurrentIlFrame = _current;

                switch (result)
                {
                    case IlGpmStepResult.Continue:
                        // prossimo block nello stesso tick (run-to-suspend), entro il quantum.
                        if (++steps >= V2RunQuantum)
                        {
                            task.Scheduler?.YieldTask(task);
                            return;
                        }
                        break;
                    case IlGpmStepResult.Blocked:
                        return;
                    case IlGpmStepResult.Finished:
                        HandleFinishedV2(task);
                        return;
                    default:
                        return;
                }
            }
        }

        /// <summary>G8: numero di block CONTINUE eseguiti nello stesso tick prima di cedere il
        /// turno (run-to-suspend). Bilancia l'overhead di dispatch (basso) con la fairness
        /// (un loop CPU-bound senza park deve lasciare spazio alle altre goroutine).</summary>
        private const int V2RunQuantum = 256;

        /// <summary>Task finito (v2): marca Finished, risolve il future (GO FUNCTION) e fa il
        /// Done implicito sul WaitGroup. Stesso contratto di HandleFinished del path flat.</summary>
        private static void HandleFinishedV2(PCodeTask task)
        {
            task.State = PCodeTaskState.Finished;
            if (task.FutureToResolve is { } future)
            {
                if (task.PendingException is { } ex)
                    future.Fail(ex, task.Scheduler);
                else
                    future.Resolve(task.PeekTop(), task.Scheduler);
            }
            if (task.WaitGroup is PCodeWaitGroup wg)
                wg.DoneAndWake(task.Scheduler);
        }

        /// <summary>Loop v2 inline (mirror di IlGpmDriver.RunInline): gira i tick sul thread
        /// chiamante + sleep-watcher, fino a esaurimento dei task. Per il test/harness v2.
        /// BOUNDED: oltre <paramref name="timeout"/> lancia TimeoutException — il limite è
        /// applicato anche al blocco di FindRunnableTask (task parcheggiati mai risvegliati
        /// NON devono appendere la suite).</summary>
        /// <summary>
        /// POLLER CONDIVISO (G8): un solo thread di background risveglia i SLEEP di TUTTI gli
        /// scheduler v2 attivi, invece di un thread sleep-watcher per run (RunV2Inline) — il
        /// churn di thread sotto carico (HTTP handler/GPM per-request) sparisce. Il poller si
        /// avvia pigramente alla prima registrazione e si ferma quando l'ultimo scheduler esce.
        /// </summary>
        private static readonly object PollerSync = new();
        private static readonly List<PCodeScheduler> PollerSchedulers = new();
        private static Thread? PollerThread;
        private static bool PollerStop;

        private static void RegisterForPolling(PCodeScheduler scheduler)
        {
            lock (PollerSync)
            {
                PollerSchedulers.Add(scheduler);
                if (PollerThread == null)
                {
                    PollerStop = false;
                    PollerThread = new Thread(SharedPollerLoop)
                    {
                        IsBackground = true,
                        Name = "gpm-il-v2-shared-poller"
                    };
                    PollerThread.Start();
                }
                Monitor.PulseAll(PollerSync);
            }
        }

        private static void UnregisterFromPolling(PCodeScheduler scheduler)
        {
            lock (PollerSync)
            {
                PollerSchedulers.Remove(scheduler);
                if (PollerSchedulers.Count == 0 && PollerThread != null)
                {
                    PollerStop = true;
                    Monitor.PulseAll(PollerSync);
                }
            }
        }

        /// <summary>Pulizia TOTALE dello stato condiviso del poller GPM (chiamata da
        /// IlRuntime.ResetProgramState, es. tra sample in un harness a processo condiviso):
        /// i sample TIMEOUT/abbandonati lasciano il loro scheduler registrato nel poller
        /// condiviso → il poller sveglia i task stantii durante il run del sample successivo
        /// (contaminazione cross-sample, es. OnErrorFramePropagation crash in sweep con
        /// 'Invalid cast String→ChannelInfo' da un proc di un assembly precedente).</summary>
        public static void ResetSharedPoller()
        {
            Thread? toJoin = null;
            lock (PollerSync)
            {
                PollerSchedulers.Clear();
                if (PollerThread != null)
                {
                    PollerStop = true;
                    Monitor.PulseAll(PollerSync);
                    toJoin = PollerThread;
                }
            }
            // Il thread esce al prossimo giro (check PollerStop → PollerThread=null); il Join
            // evita la race "due poller" se il sample successivo registra subito. Niente
            // Interrupt (romperebbe il Monitor.Wait con ThreadInterruptedException).
            toJoin?.Join(3000);
        }

        private static void SharedPollerLoop()
        {
            while (true)
            {
                PCodeScheduler[] snapshot;
                lock (PollerSync)
                {
                    if (PollerStop)
                    {
                        PollerThread = null;
                        PollerStop = false;
                        return;
                    }
                    if (PollerSchedulers.Count == 0)
                    {
                        Monitor.Wait(PollerSync);
                        continue;
                    }
                    snapshot = PollerSchedulers.ToArray();
                }

                var now = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                long? earliest = null;
                foreach (var s in snapshot)
                {
                    s.WakeExpiredSleepers(now);
                    if (s.EarliestSleepDeadline() is { } d && (earliest == null || d < earliest))
                        earliest = d;
                    // Watchdog deadlock: la RILEVAZIONE vive qui (throttlata internamente da
                    // CheckDeadlock a ~200ms, questo poller gira già ≤100ms), ma la REAZIONE no —
                    // scheduler.RequestCancellation() generico risveglia i task senza cancellare
                    // il loro IlActivationFrame.Await specifico (SendAwait/ReceiveAwait): il task
                    // si risveglia, RITENTA la stessa SEND/RECEIVE bloccante, e si riblocca subito
                    // — loop infinito di "risvegli" inutili invece di un unwind pulito. La
                    // sequenza di cancel CORRETTA (che chiama aw.Cancel() sul frame) vive già nel
                    // coordinatore di RunV2Inline (branch `deadline.IsCancellationRequested`) —
                    // qui ci limitiamo ad ARMARE il segnale, RunV2Inline lo consuma e fa la
                    // cancellazione vera lui stesso.
                    if (s.DeadlockDetected) continue;
                    // IlWaitKeyAwait/IlGraphicsAwait NON passano da PCodeTask.BeginWait (a
                    // differenza di IlNetworkAwait, che lo fa e viene già escluso dal filtro
                    // ExternalWaitKinds dentro CheckDeadlock) — usano una registrazione propria
                    // (IlRuntime.RegisterInkeyWaiter/...), quindi PendingWaitKind non li vede.
                    // Scoperto 2026-09-08: senza questo filtro aggiuntivo un programma fermo su
                    // WAITKEY (attesa legittima dell'utente, non un deadlock) veniva ucciso dopo
                    // ~2s. Se anche un solo task è fermo su uno di questi due, salta il
                    // controllo deadlock per questo giro — sono attese esterne per definizione.
                    if (s.GetTasks().Any(tt => (tt.CurrentIlFrame as IlActivationFrame)?.Await
                        is IlWaitKeyAwait or IlGraphicsAwait))
                        continue;
                    if (s.CheckDeadlock(now)) s.DeadlockDetected = true;
                }

                lock (PollerSync)
                {
                    if (PollerStop) continue; // esce al prossimo giro
                    if (earliest is { } ems)
                    {
                        var remaining = ems - DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                        // Cap ~100ms: un NUOVO timer registrato da un task mentre il poller
                        // attende un earliest più lontano (es. la SLEEP 5000 di una goroutine
                        // detached) NON sveglia questo wait (il v2 poller non osserva
                        // _timerChanged) → una deadline ravvicinata (es. GO SELECT TIMEOUT 200)
                        // sarebbe onorata solo alla scadenza del timer più lontano (hang/flaky
                        // PCodeGpmSelectTimeout). Il cap mantiene la granularità a ≤100ms.
                        if (remaining > 0)
                            Monitor.Wait(PollerSync, Math.Min((int)remaining, 100));
                    }
                    else
                    {
                        Monitor.Wait(PollerSync, 5);
                    }
                }
            }
        }

        /// <summary>Valore GOMAXPROCS (worker v2): override GMBASIC_GOMAXPROCS, default
        /// Environment.ProcessorCount — stessa risoluzione della PCodeVM multi-worker.</summary>
        private static int ResolveGoMaxProcs()
        {
            var env = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
            if (!string.IsNullOrEmpty(env) && int.TryParse(env, out var n) && n > 0)
                return n;
            return Math.Max(1, Environment.ProcessorCount);
        }

        /// <summary>Token condiviso worker/coordinatore: il coordinatore lo SWAPPA dal token
        /// del deadline al token post-cancel (dopo il cancel-drain) senza race.</summary>
        private sealed class V2TaskToken
        {
            private readonly object _lock = new();
            private CancellationToken _current;
            public V2TaskToken(CancellationToken initial) => _current = initial;
            public CancellationToken Current { get { lock (_lock) return _current; } }
            public void Set(CancellationToken t) { lock (_lock) _current = t; }
        }

        /// <summary>Box per il primo task fallito (il worker lo scrive, il coordinatore lo
        /// rilancia): un task fallito fa fallire l'intero run (stesso contratto del loop
        /// singolo, dove il tick lanciava fuori da RunV2Inline).</summary>
        private sealed class V2ExceptionBox { public Exception? Value; }

        /// <summary>Loop worker v2 (mirror di PCodeVm.WorkerLoop): ogni worker ha un driver
        /// PROPRIO (il campo _current di IlGpmV2Driver non è thread-safe). Il token dei park
        /// viene riletto a ogni giro dal box condiviso (deadline → post-cancel). Un worker il
        /// cui P è stato rimosso da GOMAXPROCS(n) termina (i task migrano alla coda globale).</summary>
        private static void V2WorkerLoop(PCodeScheduler scheduler, IlGpmV2Driver driver,
            IlGpmV2Dispatcher dispatcher, V2TaskToken token, CancellationToken shutdown,
            int processorIndex, V2ExceptionBox failure, object failureLock, long[] dispatchCounter)
        {
            // Marca l'intero thread (non tick per tick: stesso thread OS per tutta la sua vita,
            // [ThreadStatic] quindi nessuna interferenza fra worker) come "sto eseguendo un vero
            // task GPM cooperativo" — attiva GpmWorkerGuard (e i guard SSL/graphics preesistenti,
            // stesso meccanismo) per QUESTO thread, MAI per il percorso sincrono a task singolo
            // (RunSync/flat, dove bloccare è innocuo: nessun altro task da far morire di fame).
            IlGpmRuntime.MarkDriverThread(true);
            try
            {
            while (!shutdown.IsCancellationRequested)
            {
                if (scheduler.IsProcessorRetired(processorIndex)) return;
                var ct = token.Current;
                var task = scheduler.FindRunnableTask(ct, processorIndex);
                if (task == null)
                {
                    // Nessun task eseguibile: il coordinatore fa il cancel-drain alla deadline
                    // o lancia il Timeout; qui si continua (il token si rileggerà al prossimo
                    // giro) finché shutdown non è richiesto.
                    continue;
                }
                Interlocked.Increment(ref dispatchCounter[0]);
                task.State = PCodeTaskState.Running;
                try
                {
                    driver.ExecuteTaskTick(task, dispatcher);
                }
                catch (Exception ex)
                {
                    task.PendingException = ex;
                    if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                        System.Console.Error.WriteLine($"[V2TASK] proc {task.Frame?.OwnerProcedure} ex={ex} --- {ex.StackTrace}");
                    HandleFinishedV2(task);
                    lock (failureLock)
                        failure.Value ??= new InvalidOperationException(
                            $"IlGpmV2Driver: task {task.Id} (proc {task.Frame?.OwnerProcedure}) fallito.", ex);
                }
                finally
                {
                    scheduler.ReleaseExecuting(task);
                }
            }
            }
            finally
            {
                IlGpmRuntime.MarkDriverThread(false);
            }
        }

        public static void RunV2Inline(PCodeScheduler scheduler, IlGpmV2Dispatcher dispatcher, TimeSpan? timeout = null, bool captureRootExitCode = false)
        {
            using var shutdown = new CancellationTokenSource();
            using var deadline = new CancellationTokenSource();
            if (timeout is { } t) deadline.CancelAfter(t);
            RegisterForPolling(scheduler);
            // Scheduler corrente per i dispatch I/O dall'esterno (WS/HTTP async): le SUB
            // OnMessage/handler devono girare COME TASK GPM (SpawnGoSub), mai invocate in
            // diretta su un thread .NET (iniezione esterna nel GPM = race di contesto).
            IlGpmRuntime.CurrentScheduler = scheduler;

            // G8-septies: MULTI-WORKER (parità PCodeVM) — N worker OS reali (GOMAXPROCS),
            // uno per P logico, con work-stealing dello scheduler condiviso; il vecchio loop
            // singolo era una regressione vs la PCodeVM. Ogni worker ha un IlGpmV2Driver
            // PROPRIO (_current non è thread-safe). Il coordinatore (thread chiamante) fa il
            // cancel-drain alla deadline + TimeoutException.
            var workerCount = ResolveGoMaxProcs();
            scheduler.ConfigureProcessors(workerCount);

            // Scope globali (IlRuntime.BeginProgramScope, chiamato dal prologo dell'entry
            // point sul thread coordinatore PRIMA di arrivare qui) va propagato ai worker: sono
            // Thread NUOVI, il ThreadStatic di IlRuntime._activeGlobalScope non li eredita da
            // solo — altrimenti i loro StoreGlobal/LoadGlobal ricadrebbero sull'unico
            // ProgramGlobals statico condiviso da QUALSIASI altro programma in esecuzione nello
            // stesso processo (la stessa razza già vista sul path flat/non-GPM).
            var globalScope = IlRuntime.CurrentGlobalScope;
            // Stesso motivo per l'host (PRINT/Console, grafica, DB, ecc.): senza propagarlo, un
            // worker legge IlRuntime.Current tramite _processWideFallback (l'ultimo SetCurrent
            // di QUALUNQUE thread nel processo) invece dell'host di QUESTA esecuzione.
            var hostForWorkers = IlRuntime.Current;

            var token = new V2TaskToken(deadline.Token);
            var cancelSent = false;
            CancellationTokenSource? postCancel = null;
            var failureLock = new object();
            var failure = new V2ExceptionBox();
            Thread[] workers = [];
            var dispatchCounter = new long[1];
            var lastDispatch = -1L;
            var lastDispatchChange = Environment.TickCount64;

            try
            {
                workers = new Thread[workerCount];
                for (var i = 0; i < workerCount; i++)
                {
                    var driver = new IlGpmV2Driver(captureRootExitCode: captureRootExitCode);
                    var idx = i;
                    workers[i] = new Thread(() =>
                    {
                        IlRuntime.SetCurrentForThread(hostForWorkers);
                        if (globalScope != null) IlRuntime.BeginGlobalScope(globalScope);
                        V2WorkerLoop(scheduler, driver, dispatcher,
                            token, shutdown.Token, idx, failure, failureLock, dispatchCounter);
                    })
                    { IsBackground = true, Name = $"gpm-il-v2-worker-{idx}" };
                    workers[i].Start();
                }

                while (scheduler.HasActiveTasks && !shutdown.IsCancellationRequested)
                {
                    lock (failureLock)
                        if (failure.Value is { } taskEx)
                            throw new InvalidOperationException("IlGpmV2Driver: task fallito.", taskEx);

                    {
                        var d = Volatile.Read(ref dispatchCounter[0]);
                        if (d != lastDispatch)
                        {
                            lastDispatch = d;
                            lastDispatchChange = Environment.TickCount64;
                        }
                        else if (Environment.TickCount64 - lastDispatchChange > 3000 && scheduler.HasActiveTasks
                            && Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                        {
                            System.Console.Error.WriteLine($"[STUCK] 3s senza progresso: disp={d} qg={scheduler.GlobalQueueDepth} ql={scheduler.LocalQueueDepth} permits={scheduler.WorkPermitCount} tasks=[{string.Join(";", scheduler.GetTasks().Select(tt => { var f = tt.CurrentIlFrame as IlActivationFrame; var aw = f?.Await switch { IlChannelReceiveAwait r => r.DebugState, IlChannelSendAwait s => s.DebugState, null => "null", var x => x.GetType().Name }; return $"t{tt.Id}:{tt.State}:inq={tt.InQueue}:exec={tt.Executing}:wp={tt.WakePending}:wait={tt.DebugPendingWait}:proc={f?.ProcedureId}:pc={f?.Pc}:aw={aw}:owner={tt.Frame?.OwnerProcedure}"; }))}]");
                            lastDispatchChange = Environment.TickCount64;
                        }
                    }

                    // Watchdog deadlock (parità Go "fatal error: all goroutines are asleep -
                    // deadlock!"): scheduler.DeadlockDetected è armato da CheckDeadlock nel
                    // poller condiviso (SharedPollerLoop) quando TUTTI i task sono Blocked,
                    // nessun timer pendente, confermato per 2 controlli consecutivi. La
                    // REAZIONE (cancel vero, che chiama aw.Cancel() sull'IlActivationFrame di
                    // ogni task) resta qui — è l'UNICA sequenza che sa farlo correttamente per
                    // il modello v2 (un RequestCancellation() generico sveglierebbe i task senza
                    // cancellare il loro await, che ritenterebbero la stessa SEND/RECEIVE e si
                    // ribloccherebbero subito, vedi commento in SharedPollerLoop).
                    var deadlockTriggered = !cancelSent && scheduler.DeadlockDetected;
                    if (deadlockTriggered)
                    {
                        scheduler.DeadlockDetected = false;
                        Console.Error.WriteLine("fatal error: all goroutines are asleep - deadlock!");
                        Console.Error.WriteLine("[GMBASIC] nessun task puo' piu' fare progressi (nessun task " +
                            "runnable/in esecuzione, nessun timer in attesa) -- probabile SEND senza un RECEIVE " +
                            "corrispondente, o un WAIT su un WAITGROUP mai completato.");
                    }
                    if (deadline.IsCancellationRequested || deadlockTriggered)
                    {
                        // PRIMA del Timeout: cancella i task parcheggiati (parità PCodeVM
                        // cancel) — il task riprende, drena il FINALLY (unwind) e completa.
                        // Post-cancel usa un token FRESCO (il deadline è ormai scaduto, un
                        // FindRunnableTask col token cancellato ritornerebbe null subito).
                        if (!cancelSent)
                        {
                            cancelSent = true;
                            postCancel = new CancellationTokenSource(TimeSpan.FromSeconds(5));
                            token.Set(postCancel.Token);
                            var tasksNow = scheduler.GetTasks();
                            if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                                System.Console.Error.WriteLine($"[CANCEL] {tasksNow.Count} task: [{string.Join(";", tasksNow.Select(tt =>
                                {
                                    var aw = (tt.CurrentIlFrame as IlActivationFrame)?.Await;
                                    var chDbg = aw switch
                                    {
                                        IlChannelSendAwait sa => sa.DebugState,
                                        IlChannelReceiveAwait ra => ra.DebugState,
                                        _ => "-"
                                    };
                                    return $"t{tt.Id}:{tt.State}:{aw?.GetType().Name ?? "noaw"}:{(tt.CurrentIlFrame as IlActivationFrame)?.Pc}:{chDbg}";
                                }))}]");
                            foreach (var tt in tasksNow)
                            {
                                tt.CancellationRequested = true;
                                if (tt.CurrentIlFrame is IlActivationFrame f2 && f2.Await is { } aw)
                                    aw.Cancel();
                                scheduler.WakeTask(tt);
                            }
                            continue;
                        }
                        if (postCancel is { IsCancellationRequested: true })
                        {
                            var dump = string.Join(";",
                                scheduler.GetTasks().Select(tt =>
                                    $"t{tt.Id}:S={tt.State},Ip={tt.Ip},Sl={tt.SleepUntilMs},vPc={(tt.CurrentIlFrame as IlActivationFrame)?.Pc},proc={(tt.CurrentIlFrame as IlActivationFrame)?.ProcedureId},aw={(tt.CurrentIlFrame as IlActivationFrame)?.Await?.GetType().Name ?? "null"},ex={tt.PendingException?.GetType().Name}"));
                            throw new TimeoutException(
                                $"IlGpmV2Driver.RunV2Inline: superato il timeout {timeout} (task parcheggiati mai risvegliati, anche dopo il cancel). [{dump}]");
                        }
                    }
                    Thread.Sleep(5);
                }
                if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                    System.Console.Error.WriteLine($"[COORD] uscita: HasActiveTasks={scheduler.HasActiveTasks} shutdown={shutdown.IsCancellationRequested} tasks=[{string.Join(";", scheduler.GetTasks().Select(tt => $"t{tt.Id}:{tt.State}"))}]");
            }
            finally
            {
                shutdown.Cancel();
                deadline.Cancel();
                postCancel?.Cancel();
                scheduler.SignalShutdown();
                foreach (var w in workers) w.Join();
                UnregisterFromPolling(scheduler);
                IlGpmRuntime.CurrentScheduler = null;
                scheduler.Dispose();
            }
        }

        /// <summary>Applica la transizione prodotta da uno stepper v2. Ritorna l'esito del
        /// tick nello stesso contratto del driver flat (IlGpmStepResult).</summary>
        public IlGpmStepResult Apply(PCodeTask task, IlStepTransition t)
        {
            switch (t.Kind)
            {
                case IlStepKind.Call:
                {
                    var callee = new IlActivationFrame
                    {
                        ProcedureId = t.TargetProcedure,
                        Pc = 0,
                        Caller = _current,
                        ReturnPc = t.ResumePc,
                        ReturnDestination = t.Destination,
                        ReturnSlot = t.ReturnSlot,
                        ClosureContext = t.ClosureContext,
                        Locals = new object?[Math.Max(t.TargetLocalCount, t.Arguments?.Length ?? 0)]
                    };
                    if (t.Arguments is { Length: > 0 } args && callee.Locals is { } locs)
                        Array.Copy(args, locs, Math.Min(args.Length, locs.Length));
                    _current = callee;
                    return IlGpmStepResult.Continue;
                }
                case IlStepKind.Return:
                {
                    if (_current is null) return IlGpmStepResult.Finished;
                    // DEFER: il RETURN esegue PRIMA i corpi deferiti LIFO (parità Go defer).
                    // Le info del return (ReturnPc/dest/slot) restano sul frame e vengono usate
                    // quando lo stack DeferredBodies si svuota (handler DeferDone).
                    if (_current.DeferredBodies is { Count: > 0 } defsRet)
                    {
                        _current.PendingExit = t;
                        _current.Pc = defsRet.Pop();
                        return IlGpmStepResult.Continue;
                    }
                    // RETURN dentro un TRY attivo con FINALLY non ancora eseguito: prima
                    // l'unwind dei finally (parità PCodeVM), poi il return (ContinueUnwind
                    // ricompleta via PendingExit).
                    if (TryBeginReturnUnwind(_current, t, IlUnwindReason.Return))
                        return IlGpmStepResult.Continue;
                    if (_current.Caller is not { } caller)
                    {
                        // Root: FUNCTION Main con valore numerico = EXIT CODE del processo.
                        if (_captureRootExitCode && t.Value is not null)
                        {
                            try { IlRuntime.SetProgramExitCode(Convert.ToDouble(t.Value)); } catch { }
                        }
                        // Root: GO FUNCTION → risolvi il future col valore di ritorno (in v2 il
                        // valore sta nella transizione, non sullo stack del task come PCodeVM).
                        if (task.FutureToResolve is { } fut && t.Value is not null)
                        {
                            if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                                System.Console.Error.WriteLine($"[FUT] task {task.Id} resolve future val={t.Value}");
                            fut.Resolve(t.Value, task.Scheduler);
                        }
                        else if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1" && task.FutureToResolve is not null)
                            System.Console.Error.WriteLine($"[FUT] task {task.Id} ROOT RETURN ma value NULL (FutureToResolve set, val={t.Value})");
                        _current = null;
                        return IlGpmStepResult.Finished;
                    }
                    // Il pc di ripresa è quello salvato dal frame del callee alla CALL (ReturnPc),
                    // non quello della transizione (resume del SUB void non era significativo).
                    var retPc = _current.ReturnPc;
                    WriteReturnValue(caller, _current.ReturnDestination, _current.ReturnSlot, t.Value);
                    _current = caller;
                    _current.Pc = retPc;
                    return IlGpmStepResult.Continue;
                }
                case IlStepKind.Continue:
                    if (_current is not null) _current.Pc = t.ResumePc;
                    return IlGpmStepResult.Continue;
                case IlStepKind.Yield:
                    return IlGpmStepResult.Continue;
                case IlStepKind.Park:
                    // Il park è preparato dallo stepper (retry già salvato in frame.Pc o
                    // ResumePc per il SLEEP); il driver segna Blocked e ritorna. L'await
                    // operation è nel frame. Lo stato Blocked è la disciplina GPM (stesso
                    // contratto di SetGpmTaskState del path flat).
                    task.State = PCodeTaskState.Blocked;
                    return IlGpmStepResult.Blocked;
                case IlStepKind.Gosub:
                {
                    // GOSUB: pusha una continuazione sul GosubStack del frame (NON crea un
                    // activation frame — §6) e salta al target (t.TargetProcedure = pc del
                    // label, risolto dal lowering). RETURN nel corpo riprende da ResumePc.
                    if (_current is null) return IlGpmStepResult.Finished;
                    _current.GosubStack ??= new Stack<IlGosubContinuation>();
                    _current.GosubStack.Push(new IlGosubContinuation(t.ResumePc, 0, 0, false));
                    _current.Pc = t.TargetProcedure;
                    return IlGpmStepResult.Continue;
                }
                case IlStepKind.ReturnGosub:
                {
                    if (_current is null) return IlGpmStepResult.Finished;
                    // RETURN in un GOSUB dentro un TRY attivo con FINALLY non ancora eseguito:
                    // prima l'unwind dei finally (reason ReturnGosub), poi la continuazione.
                    if (_current.GosubStack is { Count: > 0 } gs)
                    {
                        if (TryBeginReturnUnwind(_current, t, IlUnwindReason.ReturnGosub))
                            return IlGpmStepResult.Continue;
                        var cont = gs.Pop();
                        _current.Pc = cont.ReturnPc;
                        return IlGpmStepResult.Continue;
                    }
                    // RETURN nudo fuori da un GOSUB = normale Return della procedura.
                    // DEFER: stesso trattamento del Return (§DEFER): prima i corpi deferiti LIFO.
                    if (_current is { DeferredBodies: { Count: > 0 } defsN } frN)
                    {
                        frN.PendingExit = t;
                        frN.Pc = defsN.Pop();
                        return IlGpmStepResult.Continue;
                    }
                    if (TryBeginReturnUnwind(_current, t, IlUnwindReason.Return))
                        return IlGpmStepResult.Continue;
                    if (_current.Caller is not { } caller2)
                    {
                        _current = null;
                        return IlGpmStepResult.Finished;
                    }
                    var retPc2 = _current.ReturnPc;
                    WriteReturnValue(caller2, _current.ReturnDestination, _current.ReturnSlot, t.Value);
                    _current = caller2;
                    _current.Pc = retPc2;
                    return IlGpmStepResult.Continue;
                }
                case IlStepKind.Resume:
                {
                    // RESUME [NEXT] dopo un handler ON ERROR: riprende da ErrorLinePc (ResumePc=0)
                    // o dal blocco successivo (ResumePc=1).
                    if (_current is not { } rFrame) return IlGpmStepResult.Finished;
                    IlGpmRuntime.ResumeOnError(rFrame, next: t.ResumePc == 1);
                    return IlGpmStepResult.Continue;
                }
                case IlStepKind.DeferDone:
                {
                    // Fine di un corpo DEFER: popa il successivo deferred LIFO, oppure
                    // ricompleta l'uscita interrotta (Return/GoExit salvato in PendingExit)
                    // rieseguendo la transizione originale con Apply ricorsivo.
                    if (_current is not { } dFrame) return IlGpmStepResult.Finished;
                    if (dFrame.DeferredBodies is { Count: > 0 } defsD)
                    {
                        dFrame.Pc = defsD.Pop();
                        return IlGpmStepResult.Continue;
                    }
                    if (dFrame.PendingExit is { } pendingD)
                    {
                        dFrame.PendingExit = null;
                        return Apply(task, pendingD);
                    }
                    return IlGpmStepResult.Continue;
                }
                case IlStepKind.GoExit:
                {
                    // GO EXIT (parità Go runtime.Goexit): termina il task ATRAVERSANDO i
                    // FINALLY (unwind reason GoExit, sospendibile). Senza FINALLY → Finished.
                    if (_current is null) return IlGpmStepResult.Finished;
                    var fe = _current;
                    // DEFER: il GoExit esegue PRIMA i corpi deferiti LIFO (parità Go defer).
                    if (fe.DeferredBodies is { Count: > 0 } defsGo)
                    {
                        fe.PendingExit = t;
                        fe.Pc = defsGo.Pop();
                        return IlGpmStepResult.Continue;
                    }
                    if (fe.Unwind is null)
                        fe.Unwind = new IlUnwindState { Reason = IlUnwindReason.GoExit };
                    while (fe.ExceptionStack is { Count: > 0 } es2)
                    {
                        var region2 = es2.Peek();
                        var finallyDone2 = fe.CleanupStack is { Count: > 0 } c2
                            && c2.Peek().CleanupPc == region2.FinallyPc;
                        if (region2.FinallyPc >= 0 && !finallyDone2)
                        {
                            fe.CleanupStack ??= new Stack<IlCleanupContinuation>();
                            fe.CleanupStack.Push(new IlCleanupContinuation
                            {
                                CleanupPc = region2.FinallyPc,
                                CleanupEndPc = region2.EndPc,
                                Reason = IlUnwindReason.GoExit
                            });
                            fe.Pc = region2.FinallyPc;
                            return IlGpmStepResult.Continue;
                        }
                        es2.Pop();
                    }
                    // Nessun FINALLY rimasto: il task esce. L'await in corso (se c'è) va
                    // cancellato: i wake successivi troveranno l'op Cancelled (generation-safe).
                    fe.Await?.Cancel();
                    task.CancellationRequested = true;
                    _current = null;
                    return IlGpmStepResult.Finished;
                }
                case IlStepKind.Throw:
                {
                    if (_current is null) return IlGpmStepResult.Finished;
                    var frame = _current;
                    if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                        System.Console.Error.WriteLine($"[THROW] proc {frame.ProcedureId} pc={frame.Pc} onErr={frame.OnError.IsSet}/{frame.OnError.TargetPc} ex={t.Exception?.GetType().Name}");

                    // ON ERROR (modalità BASIC) prima dell'unwind delle regioni TRY: un handler
                    // installato cattura l'errore e salta al suo blocco (o RESUME NEXT lo ignora).
                    var onErr = frame.OnError;
                    if (onErr.IsSet && !onErr.Suppressed)
                    {
                        onErr.ErrorLinePc = frame.Pc;
                        onErr.Suppressed = true;
                        if (onErr.TargetPc == IlGpmRuntime.GpmOnErrorResumeNext)
                        {
                            frame.Pc = onErr.ErrorLinePc + 1;
                            onErr.Suppressed = false;
                            return IlGpmStepResult.Continue;
                        }
                        if (onErr.IsGosub)
                        {
                            frame.GosubStack ??= new Stack<IlGosubContinuation>();
                            frame.GosubStack.Push(new IlGosubContinuation(onErr.ErrorLinePc + 1, 0, 0, true));
                        }
                        frame.Pc = onErr.TargetPc;
                        return IlGpmStepResult.Continue;
                    }

                    if (frame.Unwind is null)
                        frame.Unwind = new IlUnwindState { Reason = IlUnwindReason.Exception, Exception = t.Exception };

                    while (frame.ExceptionStack is { Count: > 0 } es)
                    {
                        var region = es.Peek();
                        // Il FINALLY di questa regione è già stato avviato se c'è un cleanup
                        // per il suo FinallyPc (in corso O completato) in cima.
                        var finallyHandled = frame.CleanupStack is { Count: > 0 } cs0
                            && cs0.Peek().CleanupPc == region.FinallyPc;
                        if (region.FinallyPc >= 0 && !finallyHandled)
                        {
                            frame.CleanupStack ??= new Stack<IlCleanupContinuation>();
                            frame.CleanupStack.Push(new IlCleanupContinuation
                            {
                                CleanupPc = region.FinallyPc,
                                CleanupEndPc = region.EndPc,
                                Reason = IlUnwindReason.Exception
                            });
                            frame.Pc = region.FinallyPc;
                            return IlGpmStepResult.Continue;
                        }
                        if (region.CatchPc >= 0)
                        {
                            // Il cleanup completato resta in cima finché il CATCH non termina
                            // (il ContinueUnwind successivo lo rimuove).
                            es.Pop();
                            frame.Pc = region.CatchPc;
                            return IlGpmStepResult.Continue;
                        }
                        es.Pop(); // nessun handler: risali alla regione esterna
                    }

                    // Nessun handler in-frame: PROPAGA al CALLER (parità PCodeVM/Go) — risali la
                    // call chain e ri-dispatch il Throw sul frame del chiamante, che può avere un
                    // ON ERROR/TRY. Solo se non c'è un caller → task fallito.
                    if (frame.Caller is { } callerThrow)
                    {
                        _current = callerThrow;
                        return Apply(task, new IlStepTransition(IlStepKind.Throw, 0, 0, null,
                            IlReturnDestination.Discard, null, null, frame.Unwind.Exception));
                    }
                    frame.Await?.Cancel();
                    task.PendingException = frame.Unwind.Exception;
                    _current = null;
                    return IlGpmStepResult.Finished;
                }
                case IlStepKind.ContinueUnwind:
                {
                    if (_current is null) return IlGpmStepResult.Finished;
                    var f = _current;
                    var pending = f.Unwind;
                    if (f.CleanupStack is { Count: > 0 } cs)
                    {
                        var top = cs.Peek();
                        if (!top.Completed)
                        {
                            // Il FINALLY in corso è completato: lo marca e riprende l'unwind.
                            top.Completed = true;
                            var reason = pending?.Reason;
                            var ex = pending?.Exception;
                            f.Unwind = null;
                            if (reason == IlUnwindReason.Exception && ex is not null)
                                return Apply(task, new IlStepTransition(IlStepKind.Throw, 0, 0, null,
                                    IlReturnDestination.Discard, null, null, ex));
                            if (reason == IlUnwindReason.GoExit)
                                return Apply(task, new IlStepTransition(IlStepKind.GoExit, 0, 0, null,
                                    IlReturnDestination.Discard, null, null, null));
                            if (reason is IlUnwindReason.Return or IlUnwindReason.ReturnGosub)
                            {
                                // FINALLY del RETURN/RETURN-GOSUB completati: ricompleta l'uscita
                                // interrotta rieseguendo la transizione originale (Apply ricorsivo).
                                var pendingR = f.PendingExit;
                                f.PendingExit = null;
                                if (pendingR is { } peR) return Apply(task, peR);
                            }
                            f.Pc = pending?.TargetPc ?? f.Pc;
                            return IlGpmStepResult.Continue;
                        }
                        // Cleanup completato (il CATCH è finito): rimuovilo.
                        cs.Pop();
                    }
                    return IlGpmStepResult.Continue;
                }
                case IlStepKind.Goto:
                    // G3: i GOTO sono già lowering → Branch; questo transizione resta per l'emitter.
                    if (_current is not null) _current.Pc = t.ResumePc;
                    return IlGpmStepResult.Continue;
                case IlStepKind.CompleteTask:
                    _current = null;
                    return IlGpmStepResult.Finished;
                default:
                    throw new NotSupportedException($"IlStepKind.{t.Kind} sconosciuto.");
            }
        }

        /// <summary>RETURN/RETURN-GOSUB dentro un TRY attivo con FINALLY non ancora eseguito:
        /// avvia l'unwind dei finally (stesso pattern del Throw, Reason = Return/ReturnGosub),
        /// imposta PendingExit e salta al finally. True se l'unwind è partito (il Return/GoExit
        /// verrà ricompletato quando i finally finiscono, via ContinueUnwind → Apply(PendingExit)).</summary>
        private bool TryBeginReturnUnwind(IlActivationFrame frame, IlStepTransition t, IlUnwindReason reason)
        {
            var hasUnfinishedFinally = false;
            if (frame.ExceptionStack is { Count: > 0 } esProbe)
                foreach (var r in esProbe)
                {
                    var handled = frame.CleanupStack is { Count: > 0 } c
                        && c.Peek().CleanupPc == r.FinallyPc;
                    if (r.FinallyPc >= 0 && !handled) { hasUnfinishedFinally = true; break; }
                }
            if (!hasUnfinishedFinally) return false;

            if (frame.Unwind is null)
                frame.Unwind = new IlUnwindState { Reason = reason };
            frame.PendingExit = t;
            while (frame.ExceptionStack is { Count: > 0 } es)
            {
                var region = es.Peek();
                var finallyHandled = frame.CleanupStack is { Count: > 0 } cs0
                    && cs0.Peek().CleanupPc == region.FinallyPc;
                if (region.FinallyPc >= 0 && !finallyHandled)
                {
                    frame.CleanupStack ??= new Stack<IlCleanupContinuation>();
                    frame.CleanupStack.Push(new IlCleanupContinuation
                    {
                        CleanupPc = region.FinallyPc,
                        CleanupEndPc = region.EndPc,
                        Reason = reason
                    });
                    frame.Pc = region.FinallyPc;
                    return true;
                }
                es.Pop(); // regione senza FINALLY (o già eseguito): risali
            }
            return true;
        }

        private static void WriteReturnValue(IlActivationFrame caller, IlReturnDestination destination, int slot, object? value)
        {
            switch (destination)
            {
                case IlReturnDestination.Discard:
                    // SUB void: nessuna destinazione (G1).
                    break;
                case IlReturnDestination.Local:
                    if (caller.Locals is { } l && slot >= 0 && slot < l.Length)
                        l[slot] = value;
                    break;
                case IlReturnDestination.Spill:
                    if (caller.Spills is { } s && slot >= 0 && slot < s.Length)
                        s[slot] = value;
                    break;
                case IlReturnDestination.Global:
                    caller.Globals ??= new object?[slot + 8];
                    if (slot >= 0 && slot < caller.Globals.Length)
                        caller.Globals[slot] = value;
                    break;
                case IlReturnDestination.ByRef:
                case IlReturnDestination.Future:
                    // ByRef (G4 con RefToVar) e Future (GO FUNCTION) arrivano con i prossimi passi.
                    throw new NotSupportedException(
                        $"IlReturnDestination.{destination} non ancora implementato (G4).");
                default:
                    break;
            }
        }
    }
