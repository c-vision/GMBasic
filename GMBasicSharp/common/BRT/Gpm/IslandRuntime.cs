// FILE: Basic.Runtime.Gpm.IslandRuntime.cs
// ISLAND RUNTIME — l'UNICO modo legale per un'isola nativa di parcheggiare il task.
// Un'isola esegue sul thread del worker e NON deve mai bloccare l'OS thread (nessun
// Thread.Sleep, nessun semaforo reale). Per ogni operazione bloccante (SLEEP, SEND, RECEIVE,
// WAITGROUP, future, file I/O) DEVE usare i helper qui sotto, che implementano la disciplina
// del modello concorrente §16.3/§16.4 su BRT:
//   1. preparare lo stato di retry PRIMA di registrarsi (il chiamante pusha/riavvolge Ip);
//   2. impostare State=Blocked DENTRO lo stesso lock della registrazione;
//   3. il driver NON riaccoda il task su Blocked: lo risveglia la struttura d'attesa via WakeTask.
//
// L'isola riceve il PCodeTask dal driver (PCodeProcessor.DispatchIsland). I metodi con
// overload no-task (Send, Receive, ecc.) leggono il task da _currentTask (thread-static),
// impostato dal IlIslandShimCompiler: l'inner method CIL non ha accesso diretto al PCodeTask,
// quindi le istruzioni compilate in IL (SEND/RECV/WAITGROUP/FUTURE) passano per i metodi
// thread-static qui sotto.
//
// Strategia di blocco: ogni metodo che può parcheggiare il task lancia
// IslandParkedException se l'operazione non riesce subito. L'eccezione unwinda lo stack
// CIL fino allo shim, che ritorna IslandResult.Blocked. Al risveglio, il driver ri-esegue
// l'isola dall'inizio e l'operazione ritenta.

namespace Basic.Runtime.Gpm;

public static class IslandRuntime
{
    [ThreadStatic]
    private static PCodeTask? _currentTask;

    /// <summary>Imposta il task corrente per il thread worker. Chiamato dal IlIslandShimCompiler
    /// PRIMA di invocare l'inner method CIL. Da pulire con SetCurrentTask(null) DOPO.</summary>
    public static void SetCurrentTask(PCodeTask? task) => _currentTask = task;

    /// <summary>Il PCodeTask corrente per questo thread worker. Lancia se non impostato.</summary>
    public static PCodeTask CurrentTask =>
        _currentTask ?? throw new InvalidOperationException(
            "IslandRuntime: _currentTask non impostato. Chiamare SetCurrentTask(task) prima di eseguire codice isola.");

    // ── SLEEP ──────────────────────────────────────────────────────────────

    /// <summary>
    /// Sleep cooperativo: segna il task addormentato finché <paramref name="ms"/> non scade.
    /// Overload con PCodeTask esplicito per uso dal Bridge/Shim.
    /// </summary>
    public static IslandResult Sleep(PCodeTask task, double ms)
    {
        if (ms <= 0) return IslandResult.Continue;
        var nowMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
        var frame = task.Frame;
        if (frame != null && frame.IslandSleepDeadlineMs > 0 &&
            (nowMs >= frame.IslandSleepDeadlineMs || task.SleepUntilMs == 0))
        {
            frame.IslandSleepDeadlineMs = 0;
            return IslandResult.Continue;
        }
        var deadline = nowMs + (long)ms;
        if (frame != null) frame.IslandSleepDeadlineMs = deadline;
        var wait = task.BeginWait(PCodeWaitKind.Timer);
        task.ParkUntil(deadline, wait);
        return IslandResult.Blocked;
    }

    /// <summary>Sleep cooperativo via _currentTask. Lancia IslandParkedException se si blocca.
    /// Sul retry (dopo risveglio da PCodeScheduler), se il tempo è già scaduto, ritorna
    /// subito senza ribloccare.
    ///
    /// Porting da GM-Basic (commit 179321a9). Fuori da un'isola (_currentTask non impostato —
    /// es. SLEEP nel Main() top-level, che il backend IL compila con la STESSA chiamata a
    /// prescindere dal contesto: EmitSleepStatement non distingue "dentro un'isola" da
    /// "top-level", e SetCurrentTask viene chiamato SOLO da IlIslandShimCompiler prima di
    /// eseguire un'isola) questo metodo lanciava sempre InvalidOperationException — SLEEP era
    /// quindi INUTILIZZABILE in qualunque programma nativo compilato che non girasse dentro
    /// un'isola GO-spawnata. Fallback a un vero Thread.Sleep OS: la regola "mai bloccare l'OS
    /// thread" vale per le isole, che condividono un pool di worker thread con altre isole in
    /// coda (bloccarne uno affamerebbe le altre) — Main() non fa parte di quel pool, ha un
    /// proprio thread dedicato, quindi bloccarlo non affama nessuno.</summary>
    public static void Sleep(double ms)
    {
        if (_currentTask == null)
        {
            if (ms > 0) System.Threading.Thread.Sleep((int)ms);
            return;
        }
        var task = CurrentTask;
        if (ms <= 0) return;
        var nowMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
        var frame = task.Frame;
        if (frame != null && frame.IslandSleepDeadlineMs > 0 &&
            (nowMs >= frame.IslandSleepDeadlineMs || task.SleepUntilMs == 0))
        {
            frame.IslandSleepDeadlineMs = 0;
            return;
        }
        var deadline = nowMs + (long)ms;
        if (frame != null) frame.IslandSleepDeadlineMs = deadline;
        var wait = task.BeginWait(PCodeWaitKind.Timer);
        task.ParkUntil(deadline, wait);
        throw new IslandParkedException();
    }

    // ── BLOCK ──────────────────────────────────────────────────────────────

    /// <summary>
    /// Blocco generico su una struttura d'attesa. Overload con PCodeTask esplicito.
    /// </summary>
    public static IslandResult Block(PCodeTask task, Func<PCodeTask, bool> register)
    {
        return register(task) ? IslandResult.Blocked : IslandResult.Continue;
    }

    /// <summary>Blocco generico via _currentTask. Lancia IslandParkedException se si blocca.</summary>
    public static void Block(Func<bool> register)
    {
        if (register()) throw new IslandParkedException();
    }

    // ── RETURN ────────────────────────────────────────────────────────────

    public static IslandResult Return(PCodeTask task)
    {
        var frame = task.Frame;
        if (frame == null)
        {
            task.State = PCodeTaskState.Finished;
            return IslandResult.Continue;
        }

        object? rv = null;
        if (frame.FrameSp > 0) rv = frame.PopFrameValue();
        else if (task.Sp > frame.SavedStackPointer) { rv = task.PopRef(); }
        else rv = frame.ReturnValue;

        var savedSp = frame.SavedStackPointer;
        task.Ip = frame.ReturnIp;
        task.Frame = frame.Previous;
        task.Sp = savedSp;
        if (rv != null) task.PushRef(rv);
        return IslandResult.Continue;
    }

    // ── SEND / RECEIVE (canali) ────────────────────────────────────────────

    /// <summary>Send su canale via _currentTask. Parcheggia se il buffer è pieno (lancia
    /// IslandParkedException). Overload con PCodeTask esplicito per uso dal Bridge.</summary>
    public static IslandResult Send(PCodeTask task, ChannelInfo? ch, object? value)
    {
        if (ch == null) return IslandResult.Continue;

        task.PushRef(value);
        task.PushRef(ch);
        if (ch.TrySendOrPark(value, new ChannelSenderWaiter(task, null, null, 0), out var wokenReceiver))
        {
            task.Sp -= 2;
            if (wokenReceiver is { } rcv) rcv.TryWake(task.Scheduler);
            return IslandResult.Continue;
        }
        return IslandResult.Blocked;
    }

    /// <summary>Send INLINE (back-end IL): il GO SUB è eseguito in linea, senza scheduler
    /// cooperativo né task corrente — il canale opera sul buffer diretto (TrySend). Se il
    /// buffer è pieno in un contesto single-thread, non c'è nessuno che possa svuotarlo
    /// concorrentemente: lancia esplicito (mai park silenzioso).</summary>
    public static void SendInline(ChannelInfo? ch, object? value)
    {
        if (ch == null) return;
        if (!ch.TrySend(value)) throw new InvalidOperationException("SendInline: canale pieno (nessuno scheduler in esecuzione inline).");
    }

    /// <summary>Receive INLINE (back-end IL): buffer direct via TryReceive.</summary>
    public static object? ReceiveInline(ChannelInfo? ch)
    {
        if (ch == null) return null;
        return ch.TryReceive(out var value) ? value : null;
    }

    /// <summary>Send su canale via _currentTask. Blocca (lancia) se buffer pieno.
    /// Sul retry dopo wake, non ripushare gli argomenti: sono già sullo stack
    /// dal primo tentativo (flag IslandSendRetry sul frame).</summary>
    public static void Send(ChannelInfo? ch, object? value)
    {
        var task = CurrentTask;
        if (ch == null) return;

        if (task.Frame?.IslandSendRetry == true)
        {
            if (ch.TrySendOrPark(value, new ChannelSenderWaiter(task, null, null, 0), out var wokenReceiver))
            {
                task.Sp -= 2;
                if (task.Frame != null) task.Frame.IslandSendRetry = false;
                if (wokenReceiver is { } rcv) rcv.TryWake(task.Scheduler);
                return;
            }
            throw new IslandParkedException();
        }

        task.PushRef(value);
        task.PushRef(ch);
        if (ch.TrySendOrPark(value, new ChannelSenderWaiter(task, null, null, 0), out var wokenReceiver2))
        {
            task.Sp -= 2;
            if (wokenReceiver2 is { } rcv) rcv.TryWake(task.Scheduler);
            return;
        }
        if (task.Frame != null) task.Frame.IslandSendRetry = true;
        throw new IslandParkedException();
    }

    /// <summary>Receive da canale via _currentTask. Ritorna il valore o lancia
    /// IslandParkedException se il buffer è vuoto. Overload con PCodeTask esplicito.</summary>
    public static IslandResult Receive(PCodeTask task, ChannelInfo? ch, out object? value)
    {
        value = null;
        if (ch == null) return IslandResult.Continue;

        task.PushRef(ch);
        if (ch.TryReceiveOrPark(new ChannelReceiverWaiter(task, null, 0), out var rv, out var wokenSender))
        {
            task.Sp -= 1;
            if (wokenSender is { } snd) snd.TryWake(task.Scheduler);
            value = rv;
            return IslandResult.Continue;
        }
        return IslandResult.Blocked;
    }

    /// <summary>Receive da canale via _currentTask. Ritorna il valore. Lancia se buffer vuoto.
    /// Sul retry dopo wake, non ripushare ch: è già sullo stack (flag IslandRecvRetry).</summary>
    public static object? Receive(ChannelInfo? ch)
    {
        var task = CurrentTask;
        if (ch == null) return null;

        if (task.Frame?.IslandRecvRetry == true)
        {
            if (ch.TryReceiveOrPark(new ChannelReceiverWaiter(task, null, 0), out var rv, out var wokenSender))
            {
                task.Sp -= 1;
                if (task.Frame != null) task.Frame.IslandRecvRetry = false;
                if (wokenSender is { } snd) snd.TryWake(task.Scheduler);
                return rv;
            }
            throw new IslandParkedException();
        }

        task.PushRef(ch);
        if (ch.TryReceiveOrPark(new ChannelReceiverWaiter(task, null, 0), out var rv2, out var wokenSender2))
        {
            task.Sp -= 1;
            if (wokenSender2 is { } snd) snd.TryWake(task.Scheduler);
            return rv2;
        }
        if (task.Frame != null) task.Frame.IslandRecvRetry = true;
        throw new IslandParkedException();
    }

    // ── WAITGROUP ──────────────────────────────────────────────────────────

    /// <summary>WaitGroup.Wait via _currentTask. Parcheggia se contatore > 0 (lancia).</summary>
    public static void WaitGroupWait(PCodeWaitGroup? wg)
    {
        if (wg == null) return;
        var task = CurrentTask;
        if (wg.ShouldWait(task)) throw new IslandParkedException();
    }

    /// <summary>WaitGroup.Add - contatore atomico.</summary>
    public static void WaitGroupAdd(PCodeWaitGroup? wg, int count = 1)
    {
        wg?.Add(count);
    }

    /// <summary>WaitGroup.Done - decrementa e sveglia i waiter se arriva a 0.</summary>
    public static void WaitGroupDone(PCodeWaitGroup? wg)
    {
        wg?.DoneAndWake(CurrentTask.Scheduler);
    }

    // ── FUTURE ─────────────────────────────────────────────────────────────

    /// <summary>Attende la risoluzione di un future. Ritorna il valore se già risolto,
    /// lancia IslandParkedException se non ancora risolto. Overload con PCodeTask esplicito.</summary>
    public static IslandResult FutureWait(PCodeTask task, PCodeFutureValue? future, out object? value)
    {
        value = null;
        if (future == null) return IslandResult.Continue;
        if (!future.IsResolved)
        {
            if (future.RegisterWaiter(task)) return IslandResult.Blocked;
            // Risolto fra il check IsResolved e RegisterWaiter (rarissimo, ma possibile
            // sotto vero parallelismo): RegisterWaiter non ha parcheggiato perché IsResolved
            // era diventato true nel frattempo. Prosegue normalmente.
        }
        value = future.Result;
        return IslandResult.Continue;
    }

    /// <summary>Attende un future via _currentTask. Ritorna il valore o lancia se non risolto.</summary>
    public static object? FutureWait(PCodeFutureValue? future)
    {
        if (future == null) return null;
        var task = CurrentTask;
        if (!future.IsResolved)
        {
            if (future.RegisterWaiter(task)) throw new IslandParkedException();
        }
        return future.Result;
    }

    // ── HOST DELLO SHIM ──────────────────────────────────────────────────

    [ThreadStatic]
    private static IRuntimeHost? _fileHost;

    /// <summary>Imposta l'host di runtime per le operazioni file I/O dell'isola.
    /// Chiamato da PCodeVm prima dell'esecuzione. Condivide _currentTask.</summary>
    public static void SetFileHost(IRuntimeHost? host) => _fileHost = host;

    /// <summary>L'host di runtime corrente per questo thread worker. Può essere null.
    /// Usato da IlIslandShimCompiler per impostare IlRuntime.SetCurrent.</summary>
    public static IRuntimeHost? CurrentFileHost => _fileHost;

    // ── HTTP/FETCH ────────────────────────────────────────────────────────
    // Percorso lineare IL, escluso dalle isole da IslandEligibility. La PCodeVM usa invece
    // ParkHttpBuiltin/ParkAsync e non chiama questo helper sincrono sul worker.

    private static readonly HttpClient _httpClient = new();

    /// <summary>HTTP GET sincrona. Restituisce il body come stringa.</summary>
    public static string Fetch(string url)
    {
        var response = _httpClient.GetAsync(url).GetAwaiter().GetResult();
        response.EnsureSuccessStatusCode();
        return response.Content.ReadAsStringAsync().GetAwaiter().GetResult();
    }
}
