// FILE: Basic.IlBackend.IlGpmRuntime.cs
// IL GPM RUNTIME — helper chiamati dal codice stepper generato (vedi IlGpm.cs e
// AiWiki GM-Basic-ILBackend-GPM-Chunk-State-Machine §11, §16, §26).
//
// I metodi qui sono i "task-aware" che lo stepper CIL usa al posto delle funzioni
// inline no-task (IslandRuntime.SendInline etc.): leggono/parcheggiano il task sul
// canale/waitgroup seguendo la disciplina §16.3 (State=Blocked DENTRO il lock della
// registrazione — già garantita da ChannelInfo.TrySendOrPark/TryReceiveOrPark e
// PCodeWaitGroup.ShouldWait) e ritornano un bool/completion per il flusso dello stepper
// (niente IslandParkedException: il chunk la sostituisce con un normale `return Blocked`).

using System.Reflection;
using Basic.IlBackend.Gpm;
using Basic.Runtime;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public static class IlGpmRuntime
{
    // Collega l'hook Basic.Agents.GpmWorkerGuard (bug 2026-09-08, AgentBatch-AiAgentSwarm-
    // Conflitto-GPM): AI.csproj non può referenziare questo assembly (direzione opposta a
    // quella esistente), quindi il backend IL registra qui, a startup di questa classe, come
    // riconoscere "sto girando su un worker GPM" — vedi InGpmDriver più sotto.
    static IlGpmRuntime()
    {
        Basic.Agents.GpmWorkerGuard.IsOnGpmWorker = () => InGpmDriver;
    }

    // ─── Helper valore usati dall'emitter StepV2_* (G2) ──────────────────────────────────
    /// <summary>Numero da un oggetto boxato (locali/spill del frame v2). Convenzione BASIC:
    /// i numeri sono double; accettiamo anche int/long con conversione safe.</summary>
    public static double AsDouble(object? v) => v switch
    {
        double d => d,
        int i => i,
        long l => l,
        float f => f,
        bool b => b ? -1 : 0,   /* convenzione VMBASIC TRUE = -1 */
        null => 0,
        _ => TryConvert(v)
    };

    private static double TryConvert(object v)
        => double.TryParse(v.ToString(), System.Globalization.NumberStyles.Float,
            System.Globalization.CultureInfo.InvariantCulture, out var d) ? d : 0.0;

    /// <summary>Stringa da un oggetto boxato.</summary>
    public static string? AsString(object? v) => v as string;

    /// <summary>Truthiness BASIC: 0 (o stringa vuota) = falso, altrimenti vero.</summary>
    public static bool IsTruthy(object? v) => v switch
    {
        null => false,
        string s => s.Length > 0,
        _ => AsDouble(v) != 0.0
    };

    /// <summary>PRINT (v2): accumula nel stage per-statement di IlRuntime (canone parità
    /// flat/PCode/VM-C/C99: la riga intera è emessa in UN SOLO write a fine istruzione —
    /// con i chunk scritti a uno a uno i task GPM concorrenti INTERLEAVANO le righe, bug
    /// NetSslConcurrency 2026-10-07). Senza stage attivo → scrittura diretta.</summary>
    public static void PrintValue(object? v) => IlRuntime.PrintStageAppend(IlRuntime.PrintFormatValue(v));

    /// <summary>PRINT: newline finale (flush atomico testo+'\n', vedi PrintStageNewLine).</summary>
    public static void PrintNewLine() => IlRuntime.PrintStageNewLine();

    /// <summary>Garantisce che il frame abbia almeno <paramref name="count"/> slot Locali,
    /// preservando i valori già presenti (es. gli argomenti copiati dal driver o da
    /// SpawnGoSub in un array più piccolo). Chiamato a inizio di ogni StepV2_*.</summary>
    public static void EnsureFrameLocals(IlActivationFrame frame, int count)
    {
        var cur = frame.Locals;
        if (cur != null && cur.Length >= count) return;
        var grown = new object?[count];
        if (cur is { Length: > 0 }) Array.Copy(cur, grown, Math.Min(cur.Length, count));
        frame.Locals = grown;
    }

    /// <summary>CHANNEL CLOSE (parità Go close(ch)): chiude il canale atomicamente; i receiver
    /// in attesa vengono svegliati con ok=false, i sender in attesa ricevono errore. Un doppio
    /// close (Close() = false) lancia (catturabile in TRY).</summary>
    public static void CloseChannel(ChannelInfo ch)
    {
        if (ch == null) return;
        if (!ch.Close())
            throw new InvalidOperationException("close su canale già chiuso");
    }

    /// <summary>Entra in una regione protetta (TRY): pusha l'IlExceptionRegionState sul
    /// ExceptionStack del frame. Chiamato dal primo blocco del corpo del TRY.</summary>
    public static void EnterTryRegion(IlActivationFrame frame, int catchPc, int finallyPc, int endPc)
    {
        frame.ExceptionStack ??= new System.Collections.Generic.Stack<IlExceptionRegionState>();
        frame.ExceptionStack.Push(new IlExceptionRegionState
        {
            CatchPc = catchPc,
            FinallyPc = finallyPc,
            EndPc = endPc,
            ExceptionSlot = -1,
            InHandler = false
        });
    }

    /// <summary>Esce dalla regione protetta più interna (fine normale del TRY senza FINALLY,
    /// o dopo il CATCH/FINALLY completato).</summary>
    public static void ExitTryRegion(IlActivationFrame frame)
    {
        if (frame.ExceptionStack is { Count: > 0 })
            frame.ExceptionStack.Pop();
    }

    /// <summary>GO SELECT (parità Go select): registra atomico tutti i case RECEIVE + il TIMEOUT
    /// su un SelectContext condiviso e ritorna true se un case è GIÀ pronto (vinto sincrono).
    /// Altrimenti il task resta parcheggiato; al wake il select block viene ri-eseguito e il case
    /// vincente è pronto. Modellato su OP_GO_SELECT_SETUP della PCodeVM.</summary>
    public static bool TrySelectSetup(
        PCodeTask task, ChannelInfo[] channels, bool[] isTimeout, bool[] isSend, object?[] sendValues,
        long[] timeoutsMs, SelectContext ctx, out int caseIndex, out object? value, out bool closed)
    {
        caseIndex = -1;
        value = null;
        closed = false;
        var registration = task.BeginWait(PCodeWaitKind.Select, ctx.Cancel);
        task.PendingSelect = ctx;
        for (var i = 0; i < channels.Length; i++)
        {
            if (isTimeout[i])
            {
                var now = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                var dur = timeoutsMs[i];
                if (dur <= 0)
                {
                    // TIMEOUT 0 / negativo → case pronto subito.
                    if (ctx.TrySetResult(i, null)) { caseIndex = i; return true; }
                    continue;
                }
                // RETRY dopo il timer: il TIMEOUT è scaduto (deadline persistita sul ctx) →
                // vince. Senza questo check il retry ri-parcheggerebbe (SleepUntilMs azzerato).
                if (ctx.TimeoutDeadlineMs > 0 && ctx.TimeoutDeadlineMs <= now)
                {
                    if (ctx.TrySetResult(i, null)) { caseIndex = i; return true; }
                    continue;
                }
                var deadline = now + dur;
                ctx.TimeoutDeadlineMs = deadline;
                task.ParkUntil(deadline, registration);
                continue;
            }
            var ch = channels[i];
            if (ch == null) continue; // canale nil: case disabilitato (parità Go)
            ctx.RegisterChannel(ch);
            if (isSend[i])
            {
                // Case SEND (parità Go select { case ch <- v: }): TrySendOrPark rivendica via
                // SelectCtx se un receiver è pronto o il buffer ha spazio.
                if (ch.TrySendOrPark(sendValues[i], new ChannelSenderWaiter(task, sendValues[i], ctx, i, registration), out var wokenRcv))
                {
                    if (wokenRcv is { } r) r.TryWake(task.Scheduler);
                    if (ctx.WinningCaseIndex == i) { caseIndex = i; return true; }
                }
                continue;
            }
            if (ch.TryReceiveOrPark(new ChannelReceiverWaiter(task, ctx, i, registration), out var rv, out var ws))
            {
                // TryReceiveOrPark ha GIÀ rivendicato il case via SelectCtx (case vincente):
                // qui non si ri-chiama TrySetResult (doppio claim → false).
                if (ws is { } w) w.TryWake(task.Scheduler);
                if (ctx.WinningCaseIndex == i)
                {
                    caseIndex = i;
                    value = rv;
                    closed = ch.IsClosed;
                    return true;
                }
            }
            else if (ctx.WinningCaseIndex == i && ch.TryReceive(out rv))
            {
                // Retry dopo il wake del SEND: TryDequeueActiveReceiver ha già rivendicato il
                // case via SelectCtx (TrySetResult(case, null)) ma il VALORE è rimasto nel
                // buffer (il SEND lo ha accodato). Il retry di TryReceiveOrPark non può più
                // ri-rivendicare → recupera il valore dal buffer senza secondo claim (parità
                // Go: il receiver risvegliato consuma il valore).
                caseIndex = i;
                value = rv;
                closed = false;
                return true;
            }
        }
        return false;
    }

    /// <summary>Future (GO FUNCTION): dereferenzia un PCodeFutureValue al suo risultato. La
    /// lettura di un future NON risolto è implicit-await (G5): qui il read lite ritorna il
    /// valore (o null se pendente) — l'await vero arriva con G5.</summary>
    public static object? DerefFuture(object? v)
    {
        if (v is PCodeFutureValue fut)
        {
            // Future FALLITO (GO FUNCTION terminato con eccezione): la lettura PROPAGA la
            // FailureException (parità PCodeVM) — al TRY/CATCH/ON ERROR del lettore.
            if (fut.FailureException is { } failEx)
                System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failEx).Throw();
            return fut.Result;
        }
        return v;
    }

    /// <summary>Implicit-await PARCHEGGIANTE del future (parità Go): il task si registra come
    /// waiter e PARCHEGGIA se il future è ancora Pending; al wake il blocco rientra e rilegge
    /// il valore risolto (scritto di nuovo nello slot). Un future FALLITO PROPAGA la
    /// FailureException (TRY/CATCH/ON ERROR del lettore). Ritorna true se il valore è pronto.</summary>
    public static bool TryReadFuturePark(PCodeTask task, IlActivationFrame frame, int slot)
    {
        object? v = frame.Locals != null && slot >= 0 && slot < frame.Locals.Length ? frame.Locals[slot] : null;
        if (v is not PCodeFutureValue fut)
            return true; // non un future (o già sostituito dal risultato): si continua
        if (fut.FailureException is { } failEx)
            System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failEx).Throw();
        if (fut.IsResolved)
        {
            // Scrive il risultato NELLO SLOT (il read successivo trova il valore, non il future)
            // e sveglia un eventuale sender/waiter come per la ricezione.
            frame.Locals[slot] = fut.Result;
            return true;
        }
        fut.RegisterWaiter(task);
        return false; // parcheggiato: il blocco fa la transizione Park
    }

    /// <summary>Literal LIST nel v2 (es. DIM x AS LIST&lt;INTEGER&gt; = [1,2,3]): costruisce
    /// una List&lt;object&gt; dagli elementi boxed. Il v2 usa slot object — il COUNT/indice via
    /// campo dinamico (reflection) funziona sulla List&lt;object&gt;.</summary>
    public static object BuildList(object?[] elements)
        => new System.Collections.Generic.List<object?>(elements);

    /// <summary>Restituisce (creandolo se serve) il SelectContext persistito sul frame della
    /// GO SELECT v2. La creazione avviene QUI, in codice MANAGED — NON via `newobj` nel IL
    /// generato: il `newobj SelectContext..ctor` emesso nell'assembly dinamico produceva un
    /// oggetto col campo `_channels` corrotto (crash GoSelect: NRE/SEGV in RegisterChannel,
    /// `_channels.Add` su un HashSet danneggiato; la PCodeVM crea il ctx nell'interprete,
    /// mai nel codice generato).</summary>
    public static SelectContext GetOrCreateSelectContext(IlActivationFrame frame)
        => frame.SelectCtx ??= new SelectContext();

    /// <summary>BYREF (G4): dereferenzia una cella RefToVar al valore corrente (parità Go
    /// passaggio per riferimento). Se non è una RefToVar, ritorna il valore così com'è.</summary>
    public static object? DerefRef(object? v) => v is RefToVar rv ? rv.Array[rv.Index] : v;

    /// <summary>BYREF (G4): scrive il valore ATTRAVERSO la cella RefToVar (il chiamante vede
    /// la modifica). Se il valore corrente non è una RefToVar, la scrittura è no-op.</summary>
    public static void StoreRef(object? slotValue, object? value)
    {
        if (slotValue is RefToVar rv) rv.Array[rv.Index] = value;
    }

    /// <summary>ON ERROR GOTO 0: disabilita l'handler (v2).</summary>
    public const int GpmOnErrorDisable = -3;

    /// <summary>Installa l'handler ON ERROR sul frame v2 (equivalente stepper di
    /// SetGpmErrorHandler): targetPc = pc del blocco handler, GpmOnErrorResumeNext = modalità
    /// "ignora e riprendi", GpmOnErrorDisable = disabilita.</summary>
    public static void SetOnError(IlActivationFrame frame, int targetPc, bool isGosub)
    {
        frame.OnError = new IlOnErrorState
        {
            IsSet = targetPc != GpmOnErrorDisable,
            IsGosub = isGosub,
            TargetPc = targetPc,
            ErrorLinePc = -1,
            Suppressed = false
        };
    }

    /// <summary>RESUME [NEXT] dopo un handler ON ERROR: riprende dall'ErrorLinePc (RESUME) o dal
    /// blocco successivo (RESUME NEXT). Sblocca la suppression dell'handler.</summary>
    public static void ResumeOnError(IlActivationFrame frame, bool next)
    {
        var onErr = frame.OnError;
        frame.Pc = onErr.ErrorLinePc + (next ? 1 : 0);
        onErr.Suppressed = false;
    }

    // ─── ON ERROR nello stepper GPM (procedura) ─────────────────────────────────────────
    // Lo stato dell'handler persiste sul PCodeTask (ErrorHandlerIp/ErrorHandlerIsGosub/
    // ErrorLineIp), NON nei locali CIL: a ogni tick lo stepper riparte da zero e i locali
    // si azzerano. Valori di ErrorHandlerIp: null = disabilitato (rethrow), 
    // GpmOnErrorResumeNext = ON ERROR RESUME NEXT, >= 0 = chunk dell'handler.
    /// <summary>ON ERROR RESUME NEXT (mode): ignora l'errore e riprende dal chunk successivo.</summary>
    public const int GpmOnErrorResumeNext = -2;
    /// <summary>Esito dispatch: nessun handler installato → il chiamante deve Rethrow.</summary>
    public const int GpmOnErrorNoHandler = -1;

    /// <summary>Installa/disinstalla l'handler ON ERROR sul task (equivalente stepper di
    /// OP_SET_ERR_HANDLER_GOTO/GOSUB): target null = ON ERROR GOTO 0 (disabilita), target
    /// GpmOnErrorResumeNext = ON ERROR RESUME NEXT, altrimenti l'indice del chunk handler.</summary>
    public static void SetGpmErrorHandler(PCodeTask task, int? target, bool isGosub)
    {
        task.ErrorHandlerIp = target;
        task.ErrorHandlerIsGosub = isGosub;
    }

    /// <summary>Dispatch ON ERROR nel catch di un chunk (equivalente stepper di
    /// OP_SET_ERR_HANDLER + salto): registra il chunk dell'errore in ErrorLineIp, poi ritorna
    /// l'azione da eseguire — GpmOnErrorNoHandler (rethrow), GpmOnErrorResumeNext (riprendi dal
    /// chunk successivo), oppure l'indice del chunk dell'handler. Per ON ERROR GOSUB puspha il
    /// resume-point (errorChunk+1) su task.IpStack prima di tornare il chunk handler. Rispetta
    /// la GPM: nessun blocco, il chiamante setta task.Ip e ritorna Continue (il scheduler
    /// riprende il task al chunk giusto nel tick successivo).</summary>
    public static int GpmOnErrorDispatch(PCodeTask task, int errorChunk)
    {
        task.ErrorLineIp = errorChunk;
        if (task.ErrorHandlerIp is not { } target)
            return GpmOnErrorNoHandler;
        if (target == GpmOnErrorResumeNext)
            return GpmOnErrorResumeNext;
        if (task.ErrorHandlerIsGosub)
        {
            task.IpStack ??= new Stack<int>();
            task.IpStack.Push(errorChunk + 1);
        }
        return target;
    }

    /// <summary>Riprende dopo l'handler ON ERROR GOSUB: un RETURN dentro l'handler (GOSUB) popa
    /// il resume-point pushato da GpmOnErrorDispatch e riprende lì; se lo stack è vuoto (RETURN
    /// nudo di una SUB normale) termina il task (stessa semantica del OP_RETURN p-code con
    /// IpStack vuoto). Ritorna true se il task deve FINIRE (nessun resume-point), false se deve
    /// proseguire — il chiamante stepper ha già impostato task.Ip al resume-point.</summary>
    public static bool GpmOnErrorGosubReturn(PCodeTask task)
    {
        if (task.IpStack is { Count: > 0 } stack)
        {
            task.Ip = stack.Pop();
            return false;
        }
        return true;
    }

    /// <summary>SEND con park (§11.4): TrySendOrPark già registra il mittente e imposta
    /// State=Blocked in un solo lock quando il buffer è pieno. Ritorna true se il send è
    /// riuscito (e sveglia l'eventuale receiver pendente), false se il task è stato
    /// parcheggiato. Il chiamante (stepper) DEVE già aver salvato i locali e impostato
    /// task.Ip al chunk corrente PRIMA di ritornare Blocked.</summary>
    public static bool TrySendPark(PCodeTask task, ChannelInfo? ch, object? value)
    {
        if (ch == null) return true;
        if (ch.TrySendOrPark(value, new ChannelSenderWaiter(task, value, null, 0), out var wokenReceiver))
        {
            if (wokenReceiver is { } rcv) rcv.TryWake(task.Scheduler);
            return true;
        }
        return false;
    }

    /// <summary>RECEIVE con park (§11.4): TryReceiveOrPark già registra il ricevente e
    /// imposta State=Blocked in un solo lock quando il buffer è vuoto. Ritorna true se la
    /// ricezione è riuscita (e sveglia l'eventuale sender pendente), false se parcheggiato.</summary>
    public static bool TryReceivePark(PCodeTask task, ChannelInfo? ch, out object? value)
    {
        value = null;
        if (ch == null) return true;
        if (ch.TryReceiveOrPark(new ChannelReceiverWaiter(task, null, 0), out var rv, out var wokenSender))
        {
            if (wokenSender is { } snd) snd.TryWake(task.Scheduler);
            value = rv;
            return true;
        }
        return false;
    }

    /// <summary>SLEEP unificato G5 (piano §9, timer await generation-safe): true = si continua
    /// subito (durata ≤ 0, o wake dopo la completion già consumato), false = parcheggiato.
    /// Al primo ingresso crea IlTimerAwait(deadline) e lo salva su frame.Await (l'op è il
    /// "generation" corrente). Al wake il blocco rientra: TryConsume accetta SOLO se la
    /// deadline è passata — un wake prematuro/duplicato NON consuma e il retry ri-parcha.</summary>
    public static bool SleepStep(PCodeTask task, IlActivationFrame frame, double ms)
    {
        if (ms <= 0) return true;
        if (frame.Await is IlTimerAwait ta && ta.TryConsume(out _, out _))
            return true;
        var deadline = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + (long)ms;
        var op = new IlTimerAwait(deadline);
        frame.Await = op;
        op.TryStart(task, frame);
        return false;
    }

    /// <summary>RECEIVE unificato G5 (IlChannelReceiveAwait): registra il receiver una volta,
    /// consuma al wake. true = valore ricevuto (out value), false = parcheggiato.</summary>
    public static bool ReceiveStep(PCodeTask task, IlActivationFrame frame, ChannelInfo? ch, out object? value)
    {
        value = null;
        if (ch == null) return true;
        if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
            System.Console.Error.WriteLine($"[RECV] task {task.Id} ch={(ch == null ? "null" : ch.GetHashCode().ToString())} closed={(ch?.IsClosed)} reentry={frame.Await is IlChannelReceiveAwait}");
        if (frame.Await is IlChannelReceiveAwait op)
        {
            // re-entry (wake): consuma se completata; wake prematuro → resta parcheggiato.
            if (op.TryConsume(out value, out var err))
            {
                frame.Await = null;
                        if (err != null) throw err;
                return true;
            }
                return false;
        }
        var newOp = new IlChannelReceiveAwait(task, ch!);
        frame.Await = newOp;
        if (newOp.TryStart(task, frame))
        {
            frame.Await = null;
            if (!newOp.TryConsume(out value, out var err2))
                throw new InvalidOperationException("IlChannelReceiveAwait: TryStart ok ma consume fallito.");
                if (err2 != null) throw err2;
            return true;
        }
        return false;
    }

    /// <summary>FOR EACH su canale (parità Go for v := range ch): RECEIVE con flag OK — valore
    /// + ok. ok=true se un valore è arrivato (il loop continua); ok=false se il canale è chiuso
    /// e svuotato (EXIT). Parcheggia se il canale è vuoto ma non chiuso. Stesso meccanismo di
    /// ReceiveStep (IlChannelReceiveAwait), con l'aggiunta del flag.</summary>
    public static bool ForEachChannelStep(PCodeTask task, IlActivationFrame frame, ChannelInfo? ch,
        out object? value, out bool ok)
    {
        value = null;
        ok = false;
        if (ch == null) return true; // canale nil: il loop non esegue (ok=false)
        if (frame.Await is IlChannelReceiveAwait op)
        {
            if (op.TryConsume(out value, out var err))
            {
                frame.Await = null;
                if (err != null) throw err;
                ok = value != null || !ch.IsClosed;
                return true;
            }
            return false;
        }
        var newOp = new IlChannelReceiveAwait(task, ch!);
        frame.Await = newOp;
        if (newOp.TryStart(task, frame))
        {
            frame.Await = null;
            if (!newOp.TryConsume(out value, out var err2))
                throw new InvalidOperationException("IlChannelReceiveAwait: TryStart ok ma consume fallito.");
            if (err2 != null) throw err2;
            ok = value != null || !ch.IsClosed;
            return true;
        }
        return false; // parcheggiato
    }

    /// <summary>SEND unificato G5 (IlChannelSendAwait): registra il sender una volta (valore
    /// consegnato alla registrazione), consuma al wake. true = consegnato, false = parcheggiato.</summary>
    public static bool SendStep(PCodeTask task, IlActivationFrame frame, ChannelInfo? ch, object? value)
    {
        if (ch == null) return true;
        if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
            System.Console.Error.WriteLine($"[SEND] task {task.Id} ch={(ch.GetHashCode())} val={value} reentry={frame.Await is IlChannelSendAwait}");
        if (frame.Await is IlChannelSendAwait op)
        {
            if (op.TryConsume(out _, out var err))
            {
                frame.Await = null;
                if (err != null) throw err;
                return true;
            }
            return false;
        }
        var newOp = new IlChannelSendAwait(task, ch, value);
        frame.Await = newOp;
        if (newOp.TryStart(task, frame))
        {
            frame.Await = null;
            if (!newOp.TryConsume(out _, out var err2))
                throw new InvalidOperationException("IlChannelSendAwait: TryStart ok ma consume fallito.");
            if (err2 != null) throw err2;
            return true;
        }
        return false;
    }

    /// <summary>WAIT unificato G5 (IlWaitGroupAwait): parcheggia finché il contatore del gruppo
    /// è a zero (verificato al resume; un wake per Done parziale non consuma). true = pronto,
    /// false = parcheggiato.</summary>
    public static bool WaitStep(PCodeTask task, IlActivationFrame frame, object? wg)
    {
        // WAIT <numero> (legacy delay) o espressione non-waitgroup: NO-OP — parità flat
        // (EmitWaitStatement) e PCodeVM. Solo la WAIT nuda (null) consulta il gruppo
        // implicito: attende i figli lanciati con GO SUB senza IN (bug 2026-10-01).
        PCodeWaitGroup? group = wg as PCodeWaitGroup;
        if (group is null && wg is null) group = Basic.Runtime.Gpm.GpmImplicitChildren.GetGroup(task);
        if (group is null) return true;
        if (frame.Await is IlWaitGroupAwait op)
        {
            if (op.TryConsume(out _, out var err))
            {
                frame.Await = null;
                if (err != null) throw err;
                return true;
            }
            return false;
        }
        var newOp = new IlWaitGroupAwait(task, group);
        frame.Await = newOp;
        if (newOp.TryStart(task, frame)) { frame.Await = null; return true; }
        return false;
    }

    /// <summary>COND WAIT c, mu (parità Go sync.Cond.Wait): rilascia il mutex, parcheggia sul
    /// cond (PCodeCond.TryWait → registrazione → il SIGNAL/BROADCAST sveglia) e al wake
    /// RIACQUISISCE il mutex (PCodeMutex.TryLock — handoff se Unlock l'ha già assegnato a
    /// questo task, altrimenti park sul mutex). Fase 1 = wait (park sul cond); fase 2 = re-lock
    /// (può parkare di nuovo se il mutex è conteso). Stessa disciplina di EmitMutexLock: al
    /// wake si PROSEGUE (handoff), non si ri-ritenta la wait.</summary>
    public static bool CondWaitStep(PCodeTask task, IlActivationFrame frame, PCodeCond? cond, PCodeMutex? mutex)
    {
        if (cond == null || mutex == null) return true;
        if (frame.Await is IlCondWaitAwait)
        {
            // Fase 2 — wake dal cond: riacquisisce il mutex (handoff via TryLock).
            if (mutex.TryLock(task))
            {
                frame.Await = null;
                return true;
            }
            return false; // park sul mutex (Unlock di un altro task lo sveglierà → retry)
        }
        // Fase 1 — COND WAIT c, mu: rilascia il mutex e parcheggia sul cond.
        mutex.Unlock(task.Scheduler);
        frame.Await = new IlCondWaitAwait();
        cond.TryWait(task);
        return false;
    }

    /// <summary>Op di RETE/asincrona unificata G5 (IlNetworkAwait, §11.4): avvia l'op UNA volta
    /// dietro Task.Run, consuma al wake. true = risultato pronto (out value), false = in corso.</summary>
    public static bool NetworkStep(PCodeTask task, IlActivationFrame frame,
        Action<Action<object?, Exception?>> start, out object? value)
    {
        value = null;
        if (frame.Await is IlNetworkAwait op)
        {
            // re-entry (wake della completion): consuma; se non ancora completata (wake
            // spurious), resta in corso (l'op è già avviata, TryStart idempotente).
            if (op.TryConsume(out value, out var err))
            {
                frame.Await = null;
                if (err != null) throw err;
                return true;
            }
            return false;
        }
        var newOp = new IlNetworkAwait(task, start);
        frame.Await = newOp;
        newOp.TryStart(task, frame);
        return false;
    }

    // ─── Step di RETE v2 (G5, §11.4): wrapper di NetworkStep per le connessioni TCP/SSL/WS.
    // L'op gira su Task.Run (mai bloccare il worker) e completa via PCodeWaitRegistration
    // (il primitivo CONDIVISO con la PCodeVM — BeginWait/TryComplete), mai wake manuale. ───

    public static bool TcpConnectStep(PCodeTask task, IlActivationFrame frame, string host, int port, out object? handle)
        => NetworkStep(task, frame, done =>
        {
            var net = IlRuntime.Current.Network; // cattura sul thread driver: Task.Run gira su un pool-thread che NON vede il ThreadStatic
            _ = Task.Run(() =>
            {
                try { var h = net.TcpConnect(host, port); done((double)h, null); }
                catch (Exception ex) { done(0.0, ex); }
            });
        }, out handle);

    public static bool SslOpenStep(PCodeTask task, IlActivationFrame frame, int port, string certPath, object? certPass, out object? handle)
        => NetworkStep(task, frame, done =>
        {
            var net = IlRuntime.Current.Network;
            _ = Task.Run(() =>
            {
                try { var h = net.SslListen(port, certPath, certPass?.ToString() ?? ""); done((double)h, null); }
                catch (Exception ex) { done(0.0, ex); }
            });
        }, out handle);

    public static bool SslConnectStep(PCodeTask task, IlActivationFrame frame, string host, int port, string? certPath, out object? handle)
        => NetworkStep(task, frame, done =>
        {
            var net = IlRuntime.Current.Network;
            _ = Task.Run(() =>
            {
                try { var h = net.SslConnect(host, port, certPath); done((double)h, null); }
                catch (Exception ex) { done(0.0, ex); }
            });
        }, out handle);

    public static bool WsConnectStep(PCodeTask task, IlActivationFrame frame, string host, int port, string? path, object? timeout, out object? handle)
        => NetworkStep(task, frame, done => _ = Task.Run(() =>
        {
            try { var h = IlRuntime.WsConnect(host, port, path, timeout); done(h, h is null ? new InvalidOperationException("WebSocket connect failed") : null); }
            catch (Exception ex) { done(null, ex); }
        }), out handle);

    public static bool TcpSendStep(PCodeTask task, IlActivationFrame frame, long handle, object? data, out object? ok)
        => NetworkStep(task, frame, done =>
        {
            var net = IlRuntime.Current.Network;
            _ = Task.Run(() =>
            {
                try { net.TcpSend(handle, IlRuntime.ValueToBytes(data)); done(true, null); }
                catch (Exception ex) { done(false, ex); }
            });
        }, out ok);

    public static bool TcpRecvStep(PCodeTask task, IlActivationFrame frame, long handle, out object? data)
        => NetworkStep(task, frame, done =>
        {
            var net = IlRuntime.Current.Network;
            _ = Task.Run(() =>
            {
                try { var b = net.TcpReceive(handle, 4096); done(b, null); }
                catch (Exception ex) { done(null, ex); }
            });
        }, out data);

    public static bool TcpAcceptStep(PCodeTask task, IlActivationFrame frame, long listener, out object? handle)
        => NetworkStep(task, frame, done =>
        {
            var net = IlRuntime.Current.Network;
            _ = Task.Run(() =>
            {
                try { var h = net.TcpAccept(listener); done((double)h, null); }
                catch (Exception ex) { done(0.0, ex); }
            });
        }, out handle);

    /// <summary>UDP RECV (G5): park (NetworkStep) — riceve un datagramma e produce l'array
    /// object[] {data, host, port} (stesso formato del flat).</summary>
    public static bool UdpRecvStep(PCodeTask task, IlActivationFrame frame, long handle, out object? result)
        => NetworkStep(task, frame, done =>
        {
            var net = IlRuntime.Current.Network;
            _ = Task.Run(() =>
            {
                try
                {
                    var r = net.UdpReceive(handle);
                    var data = r?.Data != null ? System.Text.Encoding.UTF8.GetString(r.Value.Data) : "";
                    done(new object[] { data, r?.Host ?? "", (double)(r?.Port ?? 0) }, null);
                }
                catch (Exception ex) { done(new object[] { "", "", 0.0 }, ex); }
            });
        }, out result);

    /// <summary>Elemento di un array (risultato UDP RECV: [data, host, port]) come object.</summary>
    public static object? ArrayGet(object? arr, int index)
        => arr is object[] a && index >= 0 && index < a.Length ? a[index] : null;

    /// <summary>UDP OPEN (G5): crea il socket UDP (operazione veloce, NON park).</summary>
    public static long TcpOpen(int port) => IlRuntime.Current.Network.TcpListen("0.0.0.0", port);
    public static void TcpClose(long handle) => IlRuntime.Current.Network.TcpClose(handle);
    public static string NetworkDataToString(object? data) => data switch
    {
        null => string.Empty,
        byte[] bytes => System.Text.Encoding.UTF8.GetString(bytes),
        string text => text,
        _ => Convert.ToString(data, System.Globalization.CultureInfo.InvariantCulture) ?? string.Empty
    };
    public static long UdpOpen(int port) => IlRuntime.Current.Network.UdpOpen(port);

    /// <summary>UDP CLOSE handle — veloce (non park), per il lowering v2.</summary>
    public static void UdpClose(long handle) => IlRuntime.Current.Network.UdpClose(handle);

    /// <summary>UDP SEND handle, host, port, data$ — veloce (non park), per il lowering v2
    /// (stessa conversione UTF-8 del path lineare, EmitUdpSendStatement → ValueToBytes).</summary>
    public static void UdpSend(long handle, string host, int port, string data)
    {
        var bytes = System.Text.Encoding.UTF8.GetBytes(data ?? "");
        IlRuntime.Current.Network.UdpSend(handle, bytes, host, port);
    }

    /// <summary>Lunghezza del buffer ricevuto da TCP RECV (byte[] o string) come double; 0 se null.</summary>
    public static double BytesLength(object? data)
        => data switch
        {
            byte[] b => b.Length,
            string s => s.Length,
            _ => 0
        };

    /// <summary>Crea lo spawn di un task GO SUB ... [IN wg]. Prepara frame con
    /// OwnerProcedure=ownerSymbolId e i parametri già disposti nei PRIMI slot delle
    /// LocalVariables (lo stepper del target li ripristina dagli stessi slot), incrementa
    /// il WaitGroup PRIMA dello schedule (garantisce che un WAIT immediato del chiamante
    /// veda il contatore corretto), e accoda il task. Ritorna il task (inutilizzato per
    /// SUB). Stessa sequenza di PCodeVm.LaunchGoTask.</summary>
    /// <summary>G8: esegue una goroutine NON-may-park (nessun StepV2) invocando la Proc_&lt;id&gt;
    /// FLAT generata per ogni procedura, con gli args del task convertiti ai tipi dei parametri.
    /// Parità Go: `go f()` con f senza park gira e termina (wg.Done al completamento).
    /// Guardia anti-ricorsione: un body flat che chiama RunProcedureAsGpm (es. trampoline
    /// grafica/HTTP) aprirebbe un RunV2Inline ANNIDATO che ri-invoca lo stesso flat → stack
    /// overflow. Oltre la profondità limite, errore chiaro invece di crash.</summary>
    private static readonly System.Threading.ThreadLocal<int> FlatGoroutineDepth = new(() => 0);

    public static void RunFlatGoroutine(PCodeTask task, int procedureId)
        => RunFlatGoroutine(task, procedureId, null);

    public static void RunFlatGoroutine(PCodeTask task, int procedureId, System.Reflection.Assembly? preferred)
    {
        if (FlatGoroutineDepth.Value >= 8)
            throw new InvalidOperationException(
                $"Backend IL GPM v2: ricorsione RunFlatGoroutine su Proc_{procedureId} (body flat che riapre un RunV2Inline annidato — trampoline grafica/HTTP?).");
        var method = preferred is not null
            ? IlRuntime.FindProcedureMethod(preferred, procedureId, null)
            : IlRuntime.FindProcedureMethod(procedureId, null);
        if (method == null)
            throw new InvalidOperationException($"Backend IL GPM v2: nessuna Proc_{procedureId} per la goroutine flat.");
        var locals = task.Frame?.LocalVariables;
        var ps = method.GetParameters();
        var args = new object?[ps.Length];
        for (var i = 0; i < ps.Length; i++)
        {
            var boxed = locals != null && i < locals.Length ? locals[i] : null;
            var pt = ps[i].ParameterType;
            if (boxed != null && !pt.IsInstanceOfType(boxed))
                boxed = pt == typeof(string) ? boxed.ToString()
                    : System.Convert.ChangeType(boxed, pt, System.Globalization.CultureInfo.InvariantCulture);
            args[i] = boxed;
        }
        FlatGoroutineDepth.Value++;
        try
        {
            method.Invoke(null, args);
        }
        catch (System.Reflection.TargetInvocationException tie)
        {
            // MethodInfo.Invoke avvolge l'eccezione della sub: per GO FUNCTION il future deve
            // ricevere il BasicException REALE (ERROR n) — altrimenti l'ON ERROR/TRY del
            // chiamante vede un TargetInvocationException e non cattura (FutureErrorFramePropagation).
            if (tie.InnerException is { } inner) System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(inner).Throw();
            throw;
        }
        finally { FlatGoroutineDepth.Value--; }
    }

    /// <summary>G8-bis: ONCE RUN o, DoInit (parità Go sync.Once.Do). Esito dello step:
    /// Skip (già eseguito → salta la procedura), Run (questo chiamante esegue DoInit + Complete),
    /// Parked (un altro lo sta eseguendo → parcheggia; al wake il Once è Done → Skip).</summary>
    public enum OnceStepResult { Skip, Run, Parked }

    public static OnceStepResult OnceStep(PCodeTask task, IlActivationFrame frame, PCodeOnce? once)
    {
        if (frame.Await is IlOnceAwait) frame.Await = null; // re-entry dopo il wake
        if (once == null)
        {
            if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                System.Console.Error.WriteLine($"[ONCE] OnceStep: once == null");
            return OnceStepResult.Skip;
        }
        switch (once.TryBegin(task))
        {
            case PCodeOnce.OnceBeginResult.Started:
                if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                    System.Console.Error.WriteLine($"[ONCE] OnceStep: Started");
                return OnceStepResult.Run;
            case PCodeOnce.OnceBeginResult.Done:
                if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                    System.Console.Error.WriteLine($"[ONCE] OnceStep: Done");
                return OnceStepResult.Skip;
            default:
                if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                    System.Console.Error.WriteLine($"[ONCE] OnceStep: Parked");
                frame.Await = new IlOnceAwait();
                return OnceStepResult.Parked;
        }
    }

    /// <summary>ONCE RUN: esegue la procedura flat (arity 0) registrata dal DEFER/ONCE e marca il
    /// Once completato. Parità Go once.Do(f).</summary>
    public static void RunOnceBody(PCodeOnce once, int procedureId)
    {
        if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
            System.Console.Error.WriteLine($"[ONCE] RunOnceBody proc={procedureId}");
        // GetCallingAssembly qui = lo stepper (assembly del sample) — lo passa a RunFlatSub
        // (che altrimenti vedrebbe IlGpmRuntime come chiamante → proc id collision).
        RunFlatSub(procedureId, System.Reflection.Assembly.GetCallingAssembly());
        once.Complete(null);
    }

    /// <summary>Invocazione della Proc_&lt;id&gt; FLAT (arity 0), per ONCE/DEFER.</summary>
    public static void RunFlatSub(int procedureId)
        => RunFlatSub(procedureId, System.Reflection.Assembly.GetCallingAssembly());

    /// <summary>Come <see cref="RunFlatSub(int)"/> ma limitato all'assembly preferito (sample
    /// corrente) — i proc id collidono tra assembly in un processo condiviso.</summary>
    public static void RunFlatSub(int procedureId, System.Reflection.Assembly? preferred)
    {
        var method = preferred is not null
            ? IlRuntime.FindProcedureMethod(preferred, procedureId, Type.EmptyTypes)
                ?? IlRuntime.FindProcedureMethod(preferred, procedureId, null)
            : IlRuntime.FindProcedureMethod(procedureId, Type.EmptyTypes)
                ?? IlRuntime.FindProcedureMethod(procedureId, null);
        if (method == null)
            throw new InvalidOperationException($"Backend IL GPM v2: nessuna Proc_{procedureId} flat.");
        method.Invoke(null, null);
    }

    /// <summary>Chiamata a una procedura FLAT (non may-park) da un corpo v2: invoca
    /// Proc_&lt;id&gt; coerciendo gli argomenti boxed al tipo dei parametri (stesso pattern
    /// di RunFlatGoroutine). Il return non è gestito (call statement void).</summary>
    public static void CallFlatSub(int procedureId, object?[]? args)
        => CallFlatSub(procedureId, args, System.Reflection.Assembly.GetCallingAssembly());

    /// <summary>Come <see cref="CallFlatSub(int, object?[])"/> ma limitato all'assembly
    /// preferito (sample corrente) — i proc id collidono tra assembly in un processo condiviso.</summary>
    public static void CallFlatSub(int procedureId, object?[]? args, System.Reflection.Assembly? preferred)
    {
        var method = preferred is not null
            ? IlRuntime.FindProcedureMethod(preferred, procedureId, Type.EmptyTypes)
                ?? IlRuntime.FindProcedureMethod(preferred, procedureId, null)
            : IlRuntime.FindProcedureMethod(procedureId, Type.EmptyTypes)
                ?? IlRuntime.FindProcedureMethod(procedureId, null);
        if (method == null)
            throw new InvalidOperationException($"Backend IL GPM v2: nessuna Proc_{procedureId} flat per la call.");
        var ps = method.GetParameters();
        try
        {
            if (ps.Length == 0) { method.Invoke(null, null); return; }
            var coerced = new object?[ps.Length];
            for (var i = 0; i < ps.Length; i++)
            {
                var boxed = args != null && i < args.Length ? args[i] : null;
                var pt = ps[i].ParameterType;
                if (boxed != null && !pt.IsInstanceOfType(boxed))
                    boxed = pt == typeof(string) ? boxed.ToString()
                        : System.Convert.ChangeType(boxed, pt, System.Globalization.CultureInfo.InvariantCulture);
                coerced[i] = boxed;
            }
            method.Invoke(null, coerced);
        }
        catch (System.Reflection.TargetInvocationException tie)
        {
            // MethodInfo.Invoke avvolge l'eccezione della sub in TargetInvocationException: il
            // v2 (TRY/CATCH/ON ERROR) la gestisce come transizione Throw e deve vedere il
            // BasicException REALE (es. THROW n), non l'involucro.
            if (tie.InnerException is { } inner) System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(inner).Throw();
            throw;
        }
    }

    /// <summary>GO FUNCTION verso una procedura FLAT (non may-park) da un corpo v2: esegue
    /// Proc_&lt;id&gt; INLINE (nessun future/v2 task — il callee non parcheggia) e ritorna il
    /// risultato. Un'eccezione (ERROR/THROW) PROPAGA al lettore (TRY/CATCH/ON ERROR) — senza
    /// questo path il future flat restava non-risolto e il read leggeva un default
    /// (FutureErrorFramePropagation).</summary>
    public static object? CallFlatFunction(int procedureId, object?[]? args)
    {
        var asm = System.Reflection.Assembly.GetCallingAssembly();
        var method = IlRuntime.FindProcedureMethod(asm, procedureId, null)
            ?? IlRuntime.FindProcedureMethod(procedureId, null);
        if (method == null)
            throw new InvalidOperationException($"Backend IL GPM v2: nessuna Proc_{procedureId} flat per la function.");
        var ps = method.GetParameters();
        object?[]? coerced = null;
        if (ps.Length > 0)
        {
            coerced = new object?[ps.Length];
            for (var i = 0; i < ps.Length; i++)
            {
                var boxed = args != null && i < args.Length ? args[i] : null;
                var pt = ps[i].ParameterType;
                if (boxed != null && !pt.IsInstanceOfType(boxed))
                    boxed = pt == typeof(string) ? boxed.ToString()
                        : System.Convert.ChangeType(boxed, pt, System.Globalization.CultureInfo.InvariantCulture);
                coerced[i] = boxed;
            }
        }
        try
        {
            return method.Invoke(null, coerced);
        }
        catch (System.Reflection.TargetInvocationException tie)
        {
            if (tie.InnerException is { } inner) System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(inner).Throw();
            throw;
        }
    }

    public static PCodeTask SpawnGoSub(PCodeScheduler? scheduler, int ownerSymbolId, object?[] args, PCodeWaitGroup? wg)
        => SpawnGoSub(scheduler, null, ownerSymbolId, args, wg);

    /// <summary>Come sopra ma con il task genitore: senza gruppo esplicito il figlio si
    /// registra sul gruppo implicito del genitore (WAIT nuda; bug 2026-10-01).</summary>
    public static PCodeTask SpawnGoSub(PCodeScheduler? scheduler, PCodeTask? parent, int ownerSymbolId, object?[] args, PCodeWaitGroup? wg)    {
        if (scheduler == null)
            throw new InvalidOperationException("IlGpmRuntime.SpawnGoSub: nessuno scheduler attivo (GO SUB fuori dalla GPM IL).");
        var task = scheduler.CreateTask(0);
        task.Scheduler = scheduler;
        task.Frame = new PCodeCallFrame
        {
            OwnerProcedure = ownerSymbolId,
            LocalVariables = args.Length == 0 ? new object?[16] : (object?[])args.Clone()
        };
        if (wg != null)
        {
            wg.Add(1);
            task.WaitGroup = wg;
        }
        else if (parent != null)
        {
            Basic.Runtime.Gpm.GpmImplicitChildren.RegisterChild(parent, task);
        }
        scheduler.Schedule(task);
        return task;
    }

    /// <summary>GO FUNCTION ... INTO x (§11.5): come <see cref="SpawnGoSub"/> ma il task
    /// nasce con <c>FutureToResolve</c> collegato — quando il task finisce (RETURN), il
    /// driver risolve il future con il valore in cima allo stack del task, svegliando chi
    /// attende. Ritorna il future (per metterlo nella variabile INTO).</summary>
    public static PCodeFutureValue SpawnGoFunction(PCodeScheduler? scheduler, int ownerSymbolId, object?[] args, PCodeWaitGroup? wg)
        => SpawnGoFunction(scheduler, null, ownerSymbolId, args, wg);

    /// <summary>Come sopra ma con il task genitore (gruppo implicito; bug 2026-10-01).</summary>
    public static PCodeFutureValue SpawnGoFunction(PCodeScheduler? scheduler, PCodeTask? parent, int ownerSymbolId, object?[] args, PCodeWaitGroup? wg)
    {        if (scheduler == null)
            throw new InvalidOperationException("IlGpmRuntime.SpawnGoFunction: nessuno scheduler attivo (GO FUNCTION fuori dalla GPM IL).");
        var future = new PCodeFutureValue();
        var task = scheduler.CreateTask(0);
        task.Scheduler = scheduler;
        task.FutureToResolve = future;
        task.Frame = new PCodeCallFrame
        {
            OwnerProcedure = ownerSymbolId,
            LocalVariables = args.Length == 0 ? new object?[16] : (object?[])args.Clone()
        };
        if (wg != null)
        {
            wg.Add(1);
            task.WaitGroup = wg;
        }
        else if (parent != null)
        {
            Basic.Runtime.Gpm.GpmImplicitChildren.RegisterChild(parent, task);
        }
        scheduler.Schedule(task);
        return future;
    }

    /// <summary>RETURN di una FUNCTION may-park (§11.5): pusha il valore (già boxed) nello
    /// stack del task — il driver HandleFinished lo userà per risolvere il future.</summary>
    public static void ReturnValue(PCodeTask task, object? value) => task.PushRef(value);

    /// <summary>DONE wg esplicito dentro una goroutine spawnata con GO SUB ... IN wg: se `wg` è
    /// lo STESSO WaitGroup registrato su task.WaitGroup, lo azzera per disattivare il Done
    /// IMPLICITO che HandleFinishedV2 farebbe comunque a fine task (§WaitGroup, PCodeTask.cs).
    /// Senza questo, un DONE esplicito + il Done implicito a fine task decrementano DUE volte
    /// per una singola goroutine: con 2+ goroutine sullo stesso WAITGROUP il contatore tocca
    /// zero dopo UNA sola di esse (2 decrementi anziché 1) e WAIT ritorna prematuramente, prima
    /// che le altre goroutine abbiano finito (bug reale, misurato con GMB_DBG_WG: DoneAndWake
    /// chiamato 2 volte per ogni singolo statement `DONE wg` —
    /// Bugs/(open) - WaitGroup-Wait-Non-Attende-Tutte-Goroutine-Multiple.md). Se `wg` è un
    /// WaitGroup DIVERSO da task.WaitGroup (DONE su un gruppo non quello di spawn — caso raro
    /// ma legittimo), task.WaitGroup resta intatto e il Done implicito sul PROPRIO gruppo
    /// avverrà comunque a fine task, come da contratto originale.</summary>
    public static void WaitGroupDone(PCodeWaitGroup wg, PCodeTask task)
    {
        wg.DoneAndWake(task.Scheduler);
        if (ReferenceEquals(task.WaitGroup, wg)) task.WaitGroup = null;
    }

    /// <summary>Lettura di una variabile GO FUNCTION ... INTO x (§11.5): se il valore è un
    /// PCodeFutureValue non ancora risolto, RegisterWaiter parcheggia il task e ritorna
    /// false (il chiamante fa il park/Blocked); se risolto, risultato = future.Result e
    /// true; se il valore NON è un future (variabile mai assegnata con GO FUNCTION), il
    /// valore stesso. Tutta la logica in C# — niente IL con isinst/dup/brtrue che il
    /// verifier rifiuta (maxstack impreciso con branch complessi).</summary>
    public static bool TryReadFuture(PCodeTask task, object? value, out object? result)
    {
        result = null;
        if (value is PCodeFutureValue future)
        {
            if (future.RegisterWaiter(task)) return false;
            // Future FALLITO (GO FUNCTION il cui task è terminato con un'eccezione): la
            // lettura PROPAGA la FailureException — al TRY/CATCH del lettore se attivo,
            // altrimenti al suo ON ERROR GOTO (parità PCodeVM). Prima il v2 leggeva .Result
            // (default) e l'errore si perdeva (FutureErrorFramePropagation).
            if (future.FailureException is { } failEx)
                System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failEx).Throw();
            result = future.Result;
            return true;
        }
        result = value;
        return true;
    }

    /// <summary>FETCH forma espressione asincrona (§E.5, stesso pattern del PCodeVM):
    /// la richiesta parte in background (Task.Run .NET, non blocca il worker), e il metodo
    /// ritorna SUBITO un PCodeFutureValue. Chi legge la variabile target (es. res.StatusCode)
    /// fa l'implicit-await via TryReadFuture nel load — il task si parcheggia fino a che il
    /// thread risolve il future col FetchResult.</summary>
    public static PCodeFutureValue HttpFetchExprAsync(object? url, object? options)
    {
        var future = new PCodeFutureValue();
        var urlStr = url?.ToString() ?? "";
        _ = Task.Run(async () =>
        {
            try
            {
                var helper = IlRuntime.SharedHttpClient;
                var opts = IlRuntime.BuildHttpOptions(options);
                var result = await helper.PerformRequestAsync(urlStr, opts).ConfigureAwait(false);
                future.Resolve(result, null);
            }
            catch (Exception ex)
            {
                future.Fail(ex, null);
            }
        });
        return future;
    }

    /// <summary>AI.BLACKBOARDWAIT in goroutine (GPM, Strada C): l'attesa bloccante
    /// (Blackboard.Wait → tcs.Task.Wait) gira su un thread del blocking pool (Task.Run), MAI
    /// sul worker GPM — così il GpmWorkerGuard.Check dentro Blackboard.Wait non scatta (il
    /// thread pool .NET non è un worker GPM). Ritorna un PCodeFutureValue: la lettura della
    /// variabile target fa l'implicit-await parcheggiante (stesso modello di HttpFetchExprAsync).</summary>
    public static PCodeFutureValue BlackboardWaitAsync(string key, double timeoutMs)
    {
        var future = new PCodeFutureValue();
        _ = Task.Run(() =>
        {
            try { future.Resolve(IlRuntime.BlackboardWait(key, timeoutMs), null); }
            catch (Exception ex) { future.Fail(ex, null); }
        });
        return future;
    }

    /// <summary>AI.AGENTSWARM in goroutine (GPM, Strada C): come BlackboardWaitAsync —
    /// AgentBatch.Run (Task.Run+WaitAll) gira sul blocking pool, mai sul worker GPM; il task
    /// GPM parcheggia sul future e riprende alla risoluzione.</summary>
    public static PCodeFutureValue AgentSwarmAsync(double n, string agentPrefix, string task, double maxConcurrency)
    {
        var future = new PCodeFutureValue();
        _ = Task.Run(() =>
        {
            try { future.Resolve(IlRuntime.AiAgentSwarm(n, agentPrefix, task, maxConcurrency), null); }
            catch (Exception ex) { future.Fail(ex, null); }
        });
        return future;
    }

    /// <summary>AI.HOOKFIRE in goroutine (GPM): il dispatch degli handler-agente (agent ask,
    /// potenzialmente bloccante) gira sul blocking pool, mai sul worker GPM; il task parcheggia
    /// sul future e riprende alla risoluzione.</summary>
    public static PCodeFutureValue HookFireAsync(string ev, string payload)
    {
        var future = new PCodeFutureValue();
        _ = Task.Run(() =>
        {
            try { future.Resolve(IlRuntime.HookFire(ev, payload), null); }
            catch (Exception ex) { future.Fail(ex, null); }
        });
        return future;
    }

    /// <summary>AI.GOALTICK in goroutine (GPM): la continuazione idle (agent ask) gira sul
    /// blocking pool, mai sul worker GPM.</summary>
    public static PCodeFutureValue GoalTickAsync()
    {
        var future = new PCodeFutureValue();
        _ = Task.Run(() =>
        {
            try { future.Resolve(IlRuntime.GoalTick(), null); }
            catch (Exception ex) { future.Fail(ex, null); }
        });
        return future;
    }

    /// <summary>SPRITE LOAD: enqueue SPRITE_LOAD_GPM, park sul chunk corrente, al wake
    /// ritenta e restituisce l'id. Fallimento → -1 (gmo PushNum(-1)). Mai wait .NET.</summary>
    public static bool TrySpriteLoadPark(PCodeTask task, IlActivationFrame frame, string? path, out int id)
        => GraphicsIntStep(task, frame, done => Gfx.SpriteLoadGpm(path ?? "", done), missing: -1, out id);

    /// <summary>DEFER <body> (parità Go defer): registra il blocco deferito sul frame
    /// (IR block pc). I corpi girano LIFO all'uscita della procedura (Return/GoExit).</summary>
    public static void PushDeferred(IlActivationFrame frame, int bodyPc)
    {
        frame.DeferredBodies ??= new Stack<int>();
        frame.DeferredBodies.Push(bodyPc);
    }

    /// <summary>CATCH ex AS ERR/ClassType (v2): bind della variabile del catch dall'eccezione
    /// corrente (frame.Unwind.Exception) nello slot del frame. bindKind: 1 = ErrInstance,
    /// 0 = istanza CLASS utente (UserExceptionInstance).</summary>
    public static void BindCatchVariable(IlActivationFrame frame, int slot, int bindKind)
    {
        var ex = frame.Unwind?.Exception as Basic.BasicException;
        if (ex is null || slot < 0 || frame.Locals is null || slot >= frame.Locals.Length) return;
        frame.Locals[slot] = bindKind == 1
            ? new Basic.ErrInstance((int)ex.Code, ex.Message, 0, "", "")
            : ex.UserExceptionInstance;
    }

    // ─── I/O di RETE bloccanti (SSL/WebSocket): park su task GPM via thread in background.
    // Mai bloccare il worker: l'op gira su Task.Run (completa da sé con timeout) e il task
    // GPM si parcheggia; al wake rilegge GpmIoResult. Fallimento → 0 (come TcpConnect). ───

    /// <summary>Generico park "object": avvia l'op in background e parcheggia il task
    /// (stesso meccanismo di TryGraphicsObjPark, ma per op che NON passano dal thread SDL).</summary>
    public static bool TryLoadTexturePark(PCodeTask task, IlActivationFrame frame, string? path, out int id)
        => GraphicsIntStep(task, frame, done => Gfx.LoadTextureGpm(path ?? "", done), missing: -1, out id);

    /// <summary>LOAD TEXTURE … CELL(w,h): callback int[] (vuoto se il file non c'è).</summary>
    public static bool TryLoadTextureSheetPark(PCodeTask task, IlActivationFrame frame, string? path, int cellW, int cellH, out object? ids)
        => GraphicsObjStep(task, frame, done => Gfx.LoadTextureSheetGpm(path ?? "", cellW, cellH, done), out ids);

    /// <summary>POINT(x,y): parità SysCall.POINT gmo (lettura pixel, default 0).</summary>
    public static bool TryPointPark(PCodeTask task, IlActivationFrame frame, int x, int y, out int color)
        => GraphicsIntStep(task, frame, done => Gfx.PointGpm(x, y, done), missing: 0, out color);

    /// <summary>SPRITE GETPIXELS: array [w,h,...] con -1 per il trasparente.</summary>
    public static bool TrySpriteGetPixelsPark(PCodeTask task, IlActivationFrame frame, int spriteId, out object? pixels)
        => GraphicsObjStep(task, frame, done => Gfx.SpriteGetPixelsGpm(spriteId, done), out pixels);

    /// <summary>SQL EXEC (FORMA FUNZIONE) con coarse-park: come TrySqlExecPark ma produce il
    /// valore di ritorno (changes). Se heavy, esegue su Task.Run e parcheggia; altrimenti
    /// sincrono. Stesso contratto dei Try*Park: true=pronto (changes valorizzato), false=parkato.</summary>
    public static bool TrySqlExecFnPark(PCodeTask task, IlActivationFrame frame, string alias, string sql, object?[] parameters, out double changes)
    {
        changes = 0;
        if (IlRuntime.AbortRequested) return true;
        if (frame.Await is IlGraphicsAwait op)
        {
            if (op.TryConsume(out var v, out var err)) { frame.Await = null; changes = ToDouble(v); if (err != null) throw err; return true; }
            return false;
        }
        var extra = parameters ?? Array.Empty<object?>();
        if (!IlRuntime.SqlIsHeavyWrite(alias, sql))
        {
            changes = IlRuntime.SqlExec(alias, sql, extra);
            return true;
        }
        var newOp = new IlGraphicsAwait(task, done =>
        {
            System.Threading.Tasks.Task.Run(() =>
            {
                try { var c = IlRuntime.SqlExec(alias, sql, extra); done(c, null); }
                catch (Exception ex) { done(0.0, ex); }
            });
        });
        frame.Await = newOp;
        newOp.TryStart(task, frame);
        if (newOp.TryConsume(out var vSync, out var errSync)) { frame.Await = null; changes = ToDouble(vSync); if (errSync != null) throw errSync; return true; }
        return false;
    }

    private static double ToDouble(object? v)
        => v is double d ? d : v is null ? 0.0 : Convert.ToDouble(v, System.Globalization.CultureInfo.InvariantCulture);

    /// <summary>SQL EXEC (statement) con coarse-park: se lo statement è una scrittura pesante
    /// (riscrive un file csv/parquet) viene eseguito su un thread del pool e il task si parcheggia;
    /// altrimenti esecuzione sincrona. Stesso contratto dei Try*Park: true=pronto, false=parkato.</summary>
    public static bool TrySqlExecPark(PCodeTask task, IlActivationFrame frame, object? dbOrAlias, string sql, object?[] parameters)
    {
        if (IlRuntime.AbortRequested) return true;
        if (frame.Await is IlGraphicsAwait op)
        {
            if (op.TryConsume(out _, out var err)) { frame.Await = null; if (err != null) throw err; return true; }
            return false;
        }
        if (IlRuntime.SqlIsHeavyWrite(dbOrAlias, sql))
        {
            var newOp = new IlGraphicsAwait(task, done =>
            {
                System.Threading.Tasks.Task.Run(() =>
                {
                    try { IlRuntime.SqlExecDriver(dbOrAlias, sql, parameters); done(null, null); }
                    catch (Exception ex) { done(null, ex); }
                });
            });
            frame.Await = newOp;
            newOp.TryStart(task, frame);
            if (newOp.TryConsume(out _, out var errSync)) { frame.Await = null; if (errSync != null) throw errSync; return true; }
            return false;
        }
        IlRuntime.SqlExecDriver(dbOrAlias, sql, parameters);
        return true;
    }

    /// <summary>SYNC: accoda Present; retry sullo stesso chunk (GpmIoReady). Prima
    /// dell'enqueue, nel worker, dispatch collisioni del frame precedente (gmo).</summary>
    public static bool TrySyncPark(PCodeTask task, IlActivationFrame frame)
    {
        IlRuntime.DispatchPendingSpriteCollisions();
        return GraphicsVoidStep(task, frame, done => Gfx.SyncGpm((r, e) =>
        {
            IlRuntime.CaptureSpriteCollisionsFromMainThread();
            done(r, e);
        }));
    }

    /// <summary>SCREEN: parcheggio come gmo SysCall.SCREEN.</summary>
    public static bool TryScreenPark(PCodeTask task, IlActivationFrame frame, int mode, int width, int height, int bpp)
        => GraphicsVoidStep(task, frame, done => Gfx.ScreenGpm(mode, width, height, bpp, done));

    /// <summary>WAITKEY (§bug ILBackend-Gpm-Stepper-For-FrameChain-Hang): a differenza degli
    /// altri park grafici (TryScreenPark/TrySyncPark, che avviano un'operazione in background
    /// e vengono svegliati dal suo completamento), qui non c'è un'operazione da avviare — la
    /// coda tasti (IlRuntime.InkeyQueue) è alimentata in modo asincrono da SDL/dai test
    /// headless (--key/QueueInkey). Se un tasto è già disponibile, lo consuma subito
    /// (nessun parcheggio). Altrimenti registra il task come "in attesa di un tasto"
    /// (IlRuntime.RegisterInkeyWaiter) e parcheggia: IlRuntime.QueueInkey sveglierà ogni
    /// waiter registrato al prossimo tasto in arrivo, che ri-controllerà la coda al tick
    /// successivo (stesso schema retry-sullo-stesso-chunk di TryScreenPark/TrySyncPark,
    /// EmitGpmPark chiama di nuovo questa funzione). PRIMA di questo fix, WAITKEY chiamava
    /// IlRuntime.ReadKey() direttamente (EmitWaitKeyStatement) anche dentro uno stepper:
    /// quella funzione blocca SINCRONAMENTE il thread con uno spin-wait (Thread.Sleep) —
    /// dentro il driver GPM questo congela l'INTERO scheduler (nessun altro task/goroutine
    /// può avanzare) finché non arriva un tasto, non solo il task che ha chiamato WAITKEY.</summary>
    /// <summary>INKEY (senza timeout) come POLL NON-BLOCCANTE nello stepper: se c'è un tasto
    /// in coda lo consuma (key = tasto), altrimenti key = "" — MAI park (ritorna sempre true,
    /// Continue). INKEY NON è un WAITKEY: il loop di gioco "INKEY K$ : IF K$ = ESC THEN EXIT DO"
    /// deve girare ogni frame senza congelarsi. Prima (fix cda6fdba) INKEY usava TryWaitKeyPark
    /// e parcheggiava → sprite fermi quando nessun tasto è disponibile.</summary>
    public static bool TryInkeyPoll(PCodeTask task, out string key)
    {
        key = "";
        if (IlRuntime.AbortRequested) return true;
        if (IlRuntime.TryDequeueInkey(out key)) return true;
        return true;
    }

    /// <summary>WAITKEY (G7, frame.Await — niente campi I/O task-global): se un tasto è in coda
    /// lo consuma; altrimenti crea IlWaitKeyAwait su frame.Await (registra il task in
    /// IlRuntime.InkeyWaiters) e parcheggia. QueueInkey sveglia → re-entry consuma.</summary>
    public static bool WaitKeyStep(PCodeTask task, IlActivationFrame frame, out string key)
    {
        key = "";
        if (IlRuntime.TryDequeueInkey(out key)) { frame.Await = null; return true; }
        if (frame.Await is IlWaitKeyAwait op)
        {
            if (op.TryConsume(out var v, out var err))
            {
                frame.Await = null;
                key = v as string ?? "";
                if (err != null) throw err;
                return true;
            }
            return false;
        }
        var newOp = new IlWaitKeyAwait(task);
        frame.Await = newOp;
        newOp.TryStart(task, frame);
        return false;
    }

    // ─── READ (@GET … READ) nel percorso GPM (stepper v2) ──────────────────────────────
    // Parità col modello C (gmb_read_run): il campo è reso ad ogni tasto e il task PARCHEGGIA
    // finché non arriva un tasto (WaitKeyStep). Lo stato (campo corrente + buffer di edit)
    // vive nel frame per sopravvivere al park; l'emitter (IlCoroutineEmitter) cicla i campi
    // @GET, valutando inline WHEN/VALID e assegnando la variabile.

    public sealed class IlReadFieldState
    {
        public int FieldIndex;
        public bool FieldStarted;
        public readonly System.Text.StringBuilder Buffer = new();
        public string Value = "";
    }

    private static IlReadFieldState ReadStateOf(IlActivationFrame frame)
    {
        if (frame.ReadState is IlReadFieldState s) return s;
        s = new IlReadFieldState();
        frame.ReadState = s;
        return s;
    }

    /// <summary>Inizio READ: crea lo stato SOLO se assente (idempotente al re-entry dopo un
    /// park — NON azzera i progressi).</summary>
    public static void ReadBegin(IlActivationFrame frame)
    {
        if (frame.ReadState is not IlReadFieldState) frame.ReadState = new IlReadFieldState();
    }
    public static int ReadFieldIndex(IlActivationFrame frame) => ReadStateOf(frame).FieldIndex;
    public static void ReadSetFieldIndex(IlActivationFrame frame, int i) => ReadStateOf(frame).FieldIndex = i;
    public static string ReadFieldValue(IlActivationFrame frame) => ReadStateOf(frame).Value;
    public static void ReadResetField(IlActivationFrame frame)
    { var s = ReadStateOf(frame); s.FieldStarted = false; s.Buffer.Clear(); }
    public static void ReadEnd(IlActivationFrame frame) => frame.ReadState = null;
    /// <summary>Ri-prompt VALID: riprende il loop nativo dal campo idx (buffer conservati).</summary>
    public static void ReadNativeResume(int idx) => Basic.Runtime.Services.NativeTui.ReadResume(idx);

    /// <summary>Campo READ SEQUENZIALE (blocchi @GET con WHEN): un campo per volta, cosi' il
    /// predicato vede i valori gia' confermati dei campi precedenti (semantica Clipper).
    /// skip != 0 → nessun fuoco, la variabile resta al valore iniziale.</summary>
    public static string ReadSequentialField(double row, double col, double w, object? initial,
        string? type, double vmin, double vmax, double step, int skip)
    {
        /* initial e' il VALORE CORRENTE della variabile (boxato: string o double), non gia'
         * testo: cosi' un campo numerico non digitato conserva il valore invece di azzerarsi
         * (ToDouble("") == 0) e un campo saltato da WHEN restituisce il valore invariato. */
        var init = EditInitial(initial);
        if (skip != 0) return init;
        Basic.Runtime.Services.NativeTui.ReadReset();
        int idx = Basic.Runtime.Services.NativeTui.ReadAddTyped((int)row, (int)col,
            w > 0 ? (int)w : 10, init, type ?? "", "", "", vmin, vmax, step);
        int st = Basic.Runtime.Services.NativeTui.ReadRun();
        string val = (st == 1 && idx >= 0) ? Basic.Runtime.Services.NativeTui.ReadValue(idx) : init;
        Basic.Runtime.Services.NativeTui.ReadReset();
        return val;
    }

    /// <summary>Valore iniziale di edit per un campo @GET: la stringa corrente per i campi
    /// stringa, la rappresentazione numerica per i numerici (così un Enter su un campo non
    /// digitato NON perde il valore — niente ToDouble("")).</summary>
    public static string EditInitial(object? v) => v switch
    {
        null => "",
        string s => s,
        double d => (d == System.Math.Floor(d) && !double.IsInfinity(d))
            ? ((long)d).ToString(System.Globalization.CultureInfo.InvariantCulture)
            : d.ToString(System.Globalization.CultureInfo.InvariantCulture),
        _ => v.ToString() ?? ""
    };

    /// <summary>Un passo del campo @GET corrente: render + attesa di UN tasto (park). Ritorna
    /// 0=parca (nessun tasto), 1=tasto applicato (campo ancora aperto), 2=campo confermato
    /// (Enter), 3=annullato (ESC). Il valore confermato/annullato è in ReadFieldValue.</summary>
    // ─── READ unificato NATIVO (una sola implementazione per tutte le gambe) ───
    public static int ReadNativeAdd(double row, double col, double w, object? initial, string? type,
                                    string? picture, string? items, double vmin, double vmax, double step)
        => Basic.Runtime.Services.NativeTui.ReadAddTyped((int)row, (int)col, (int)w, EditInitial(initial),
                                                         type ?? "", picture ?? "", items ?? "", vmin, vmax, step);
    public static int ReadNativeRun() => Basic.Runtime.Services.NativeTui.ReadRun();
    public static string ReadNativeValue(int idx) => Basic.Runtime.Services.NativeTui.ReadValue(idx);
    public static void ReadNativeReset() => Basic.Runtime.Services.NativeTui.ReadReset();

    /* MESSAGE/ERROR Clipper: testo di stato dell'ultimo campo @GET. */
    public static void ReadNativeSetMessage(string? text) => Basic.Runtime.Services.NativeTui.ReadSetMessageLast(text ?? "");
    public static void ReadNativeSetError(string? text) => Basic.Runtime.Services.NativeTui.ReadSetErrorLast(text ?? "");

    public static int ReadFieldStep(PCodeTask task, IlActivationFrame frame, int row, int col, int width, bool isStr, string initial)
    {
        var s = ReadStateOf(frame);
        if (!s.FieldStarted)
        {
            s.Buffer.Clear(); s.Buffer.Append(initial ?? "");
            if (width > 0 && s.Buffer.Length > width) s.Buffer.Length = width;
            s.FieldStarted = true;
        }
        IlRuntime.ReadRenderField(row, col, width, s.Buffer.ToString());
        if (!WaitKeyStep(task, frame, out var key)) return 0;   // park: nessun tasto ancora
        if (string.IsNullOrEmpty(key)) return 1;
        foreach (var ch in key)
        {
            if (ch == '\r' || ch == '\n') { s.Value = s.Buffer.ToString(); s.FieldStarted = false; return 2; }
            if (ch == '\x1b')             { s.Value = s.Buffer.ToString(); s.FieldStarted = false; return 3; }
            if (ch == '\b') { if (s.Buffer.Length > 0) s.Buffer.Length--; }
            else if (ch >= ' ' && (width <= 0 || s.Buffer.Length < width)) s.Buffer.Append(ch);
        }
        IlRuntime.ReadRenderField(row, col, width, s.Buffer.ToString());
        return 1;
    }

    private static IGraphicsService Gfx => IlRuntime.Current.Graphics;
    // ─── Primitivi grafici FLAT nel v2 (G6): FONT/LINE/CIRCLE/PSET/PRESET/DRAW/SAVE/
    // RESTORE/COLOR/LOCATE — chiamano il servizio grafico (SDL reale o Null headless).
    // Il v2 non ha IlTypeContext: il runtime fa da ponte con argomenti boxed/doppie. ───
    public static void GraphicsFont(object? path, double size)
        => Gfx.Font(path?.ToString() ?? "", (int)size);
    public static void GraphicsLine(double x1, double y1, double x2, double y2, double color, object? style)
        => Gfx.Line((int)x1, (int)y1, (int)x2, (int)y2, (int)color, style?.ToString());
    public static void GraphicsCircle(double x, double y, double radius, double color)
        => Gfx.Circle((int)x, (int)y, (int)radius, (int)color);
    public static void GraphicsPset(double x, double y, double color)
        => Gfx.Line((int)x, (int)y, (int)x, (int)y, (int)color, null);
    public static void GraphicsPreset(double x, double y)
        => Gfx.Preset((int)x, (int)y);
    public static void GraphicsDraw(object? commands)
        => Gfx.Draw(commands?.ToString() ?? "");
    public static void GraphicsSaveScreen(object? name)
        => Gfx.SaveScreen(name?.ToString() ?? "");
    public static void GraphicsRestoreScreen(object? name)
        => Gfx.RestoreScreen(name?.ToString() ?? "");
    public static void GraphicsColor(object? foreground, object? background)
        => Gfx.Color(foreground, background);
    public static void GraphicsLocate(double column, double line)
        => Gfx.Locate((int)column, (int)line);

    [ThreadStatic] private static bool _inGpmDriver;

    /// <summary>Scheduler GPM corrente del programma in esecuzione (impostato da
    /// <c>IlGpmV2Driver.RunV2Inline</c> a inizio run, azzerato a fine run). Serve ai loop di
    /// I/O asincroni (WS/HTTP) per dispatchare le SUB OnMessage/handler come TASK GPM
    /// (SpawnGoSub) invece di invocarle in diretta su un thread .NET — niente iniezione
    /// esterna nel GPM, niente race di contesto ([ThreadStatic]/EnsureContext).</summary>
    public static PCodeScheduler? CurrentScheduler { get; set; }

    public static bool InGpmDriver => _inGpmDriver;

    /// <summary>Guard runtime: se un'op "deve parkare" viene eseguita su un worker GPM senza
    /// chunk boundary → lancia esplicito invece di bloccare/stallare gli altri task.</summary>
    public static void ThrowIfWorker(string op)
    {
        if (_inGpmDriver)
            throw new InvalidOperationException($"{op} richiede il parcheggio GPM (chunk stepper): mai bloccarsi in un worker.");
    }

    /// <summary>Marca (o libera) il thread corrente come thread del driver GPM (worker).</summary>
    public static void MarkDriverThread(bool on) => _inGpmDriver = on;

    private static bool GraphicsIntStep(PCodeTask task, IlActivationFrame frame,
        Action<Action<object?, Exception?>> enqueue, int missing, out int id)
    {
        id = missing;
        if (IlRuntime.AbortRequested) return true;
        if (frame.Await is IlGraphicsAwait op)
        {
            if (op.TryConsume(out var v, out var err))
            {
                frame.Await = null;
                id = ToInt(v, missing);
                if (err != null) throw err;
                return true;
            }
            return false;
        }
        var newOp = new IlGraphicsAwait(task, enqueue);
        frame.Await = newOp;
        newOp.TryStart(task, frame);
        // NullGraphicsService (host headless, es. test_ilrun) completa ogni *Gpm() SEMPRE
        // sincronamente, dentro TryStart — a differenza di SDL reale, dove la callback
        // arriva sempre più tardi su un altro thread. TryStart ritorna comunque false: se
        // non consumiamo subito qui, l'op resta "parcheggiata" in attesa di un wake che
        // task.Scheduler potrebbe non inviare mai (Scheduler nullo per un Main() semplice,
        // senza GOROUTINE) — SCREEN restava bloccato per sempre (visto dal vivo:
        // samples/Mouse/MouseGraphicState.bas, CPU al 900%+ con ON KEY/ON MOUSE
        // che forza un retry loop che ripolla lo stesso park mai risolto).
        if (newOp.TryConsume(out var vSync, out var errSync))
        {
            frame.Await = null;
            id = ToInt(vSync, missing);
            if (errSync != null) throw errSync;
            return true;
        }
        return false;
    }

    private static bool GraphicsObjStep(PCodeTask task, IlActivationFrame frame,
        Action<Action<object?, Exception?>> enqueue, out object? value)
    {
        value = null;
        if (IlRuntime.AbortRequested) return true;
        if (frame.Await is IlGraphicsAwait op)
        {
            if (op.TryConsume(out var v, out var err))
            {
                frame.Await = null;
                value = v;
                if (err != null) throw err;
                return true;
            }
            return false;
        }
        var newOp = new IlGraphicsAwait(task, enqueue);
        frame.Await = newOp;
        newOp.TryStart(task, frame);
        // Vedi commento in GraphicsIntStep: consuma subito un completamento sincrono
        // (NullGraphicsService) invece di affidarsi a un wake che potrebbe non arrivare mai.
        if (newOp.TryConsume(out var vSync, out var errSync))
        {
            frame.Await = null;
            value = vSync;
            if (errSync != null) throw errSync;
            return true;
        }
        return false;
    }

    private static bool GraphicsVoidStep(PCodeTask task, IlActivationFrame frame,
        Action<Action<object?, Exception?>> enqueue)
    {
        if (IlRuntime.AbortRequested) return true;
        if (frame.Await is IlGraphicsAwait op)
        {
            if (op.TryConsume(out _, out var err))
            {
                frame.Await = null;
                if (err != null) throw err;
                return true;
            }
            return false;
        }
        var newOp = new IlGraphicsAwait(task, enqueue);
        frame.Await = newOp;
        newOp.TryStart(task, frame);
        // Vedi commento in GraphicsIntStep: consuma subito un completamento sincrono
        // (NullGraphicsService) invece di affidarsi a un wake che potrebbe non arrivare mai.
        if (newOp.TryConsume(out _, out var errSync))
        {
            frame.Await = null;
            if (errSync != null) throw errSync;
            return true;
        }
        return false;
    }


    private static int ToInt(object? v, int missing) => v switch
    {
        int i => i,
        null => missing,
        _ => Convert.ToInt32(v, System.Globalization.CultureInfo.InvariantCulture),
    };

    /// <summary>Esegue una SUB may-park FLAT grafica (LOAD TEXTURE/SPRITE LOAD/…) come
    /// task GPM: il Proc_* lineare chiamerebbe <c>IGraphicsService.LoadTexture</c> e
    /// lancerebbe. Il Present resta sul thread SDL; questo Run sta sul caller (vmThread).
    /// ponytail: un driver GPM nidificato per SUB, non la frame-chain §11.5. Upgrade:
    /// chiamate may-park dallo stepper Main senza secondo scheduler.
    /// NoInlining: il dispatcher riflette sugli StepV2_* via GetCallingAssembly — se il
    /// JIT inlina questo metodo nel chiamante (o un wrapper lo interpone), il frame del
    /// programma generato sparisce e la reflection vede l'assembly sbagliata.</summary>
    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
    public static void RunProcedureAsGpm(int ownerSymbolId, object?[]? args)
    {
        // G7: ponytail rimosso — il DRIVER v2 (IlGpmV2Driver) esegue la procedura come task
        // stackless (stesso scheduler inline, nessun thread pool).
        var dispatcher = IlGpmV2.CreateV2Dispatcher(Assembly.GetCallingAssembly());
        var scheduler = new PCodeScheduler();
        var locals = new object?[64];
        if (args is { Length: > 0 })
        {
            if (args.Length > locals.Length) locals = new object?[args.Length + 16];
            Array.Copy(args, locals, args.Length);
        }
        var task = scheduler.CreateTask(0);
        task.Scheduler = scheduler;
        task.Frame = new PCodeCallFrame
        {
            OwnerProcedure = ownerSymbolId,
            LocalVariables = locals
        };
        scheduler.Schedule(task);
        IlGpmV2Driver.RunV2Inline(scheduler, dispatcher, timeout: null);
        if (IlRuntime.AbortRequested)
            throw new IlRuntime.ProgramAbortedException();
    }

    /// <summary>Entry point GPM v2 (G7: v2 default, ponytail in rollback): crea lo scheduler, il
    /// task Main e avvia il DRIVER v2 (IlGpmV2Driver.RunV2Inline). Il dispatcher riflette sugli
    /// StepV2_&lt;id&gt; dell'assembly chiamante. Usato quando Main è nel sottoinsieme v2.</summary>
    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
    public static void RunV2Entry(int mainSymbolId)
        => RunV2Entry(mainSymbolId, Assembly.GetCallingAssembly());

    /// <summary>Variante con assembly ESPLICITO: il dispatcher v2 riflette sugli StepV2_&lt;id&gt;
    /// di <paramref name="sourceAssembly"/>. Necessaria perché l'entry emesso dal codegen passa
    /// da wrapper (RunV2EntryConsole) VIVENTI NEL RUNTIME: lì Assembly.GetCallingAssembly()
    /// ritornerebbe l'assembly del runtime (nessuno StepV2_*), il dispatcher risultava VUOTO e
    /// ogni task cadeva sul fallback FLAT RunFlatGoroutine → Main lineare, GO SUB inline,
    /// BLACKBOARDWAIT/AGENTSWARM sincrone sul driver (GpmWorkerGuard fail-fast / deadlock).
    /// NoInlining su ENTRAMBI i wrapper: l'inlining JIT sposterebbe il frame del chiamante
    /// originario e GetCallingAssembly mentirebbe di nuovo.</summary>
    public static void RunV2Entry(int mainSymbolId, Assembly? sourceAssembly)
    {
        var dispatcher = IlGpmV2.CreateV2Dispatcher(sourceAssembly ?? Assembly.GetCallingAssembly());
        var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask(0);
        task.Scheduler = scheduler;
        task.Frame = new PCodeCallFrame
        {
            OwnerProcedure = mainSymbolId,
            LocalVariables = new object?[64]
        };
        scheduler.Schedule(task);
        // Timeout per il run v2 (env-gate GMBASIC_V2_TIMEOUT_MS): con tutti i task parcheggiati
        // e un wake perso il driver bloccherebbe all'infinito — il timeout produce il dump dei
        // task (TimeoutException) invece di un hang inspiegabile nei test.
        TimeSpan? runTimeout = null;
        if (Environment.GetEnvironmentVariable("GMBASIC_V2_TIMEOUT_MS") is { } tm
            && int.TryParse(tm, out var tms) && tms > 0)
            runTimeout = TimeSpan.FromMilliseconds(tms);
        IlGpmV2Driver.RunV2Inline(scheduler, dispatcher, timeout: runTimeout, captureRootExitCode: true);
        if (IlRuntime.AbortRequested)
            throw new IlRuntime.ProgramAbortedException();
    }

    /// <summary>Entry point GPM v2 per la CONSOLE REALE (eseguibile interattivo): avvia il
    /// feeder console → IlRuntime.QueueInkey prima del driver e lo ferma nel finally. Nel v2
    /// WAITKEY/GET/INKEY NON leggono System.Console (WaitKeyStep consuma solo InkeyQueue e
    /// parcheggia): senza un feeder la console reale non alimentava MAI la coda → WAITKEY non
    /// vedeva i tasti. Il feeder è GPM-safe (loop dedicato, chiama solo QueueInkey = enqueue +
    /// Task.Wake dei waiter parcheggiati; il dispatch ON KEY avviene nel consumatore). Nei
    /// runner headless/test (input redirect o GMBASIC_HEADLESS) Start è no-op → nessun cambio
    /// di comportamento. Vedi TODO (open) Attivare-KeyboardServiceConsole-Start.</summary>
    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
    public static void RunV2EntryConsole(int mainSymbolId)
    {
        // L'assembly dei StepV2_* è quello del CHIAMANTE ORIGINARIO (il programma generato):
        // catturato QUI, prima del wrapper, e passato esplicito — GetCallingAssembly dentro
        // RunV2Entry vedrebbe questo wrapper (l'assembly runtime) e il dispatcher sarebbe vuoto.
        var sourceAssembly = Assembly.GetCallingAssembly();
        IlConsoleKeyFeeder.Start();
        try
        {
            RunV2Entry(mainSymbolId, sourceAssembly);
        }
        finally
        {
            IlConsoleKeyFeeder.Stop();
        }
    }
}
