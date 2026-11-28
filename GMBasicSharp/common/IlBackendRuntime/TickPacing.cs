// FILE: TickPacing.cs — passo del frame per i cicli TICK (IL e PCodeVM).
//
// La formula è quella di `gmb_tick_advance` (runtime C condiviso, gmb_console.c) RISCRITTA in
// managed, non importata: il backend IL/PCode deve girare anche dove il runtime nativo non è
// caricato, e un P/Invoke per frame non è gratis. Le due copie sono un contratto: il ritmo
// misurato dalle sonde 60/20 fps sulle quattro gambe le tiene allineate (modifica qui =
// modifica là).
//
// La scadenza è il confine del frame e si attende solo il RESIDUO, non il passo intero DOPO il
// lavoro. Dormendo il passo intero il periodo diventava lavoro + passo (misurato su C99Native:
// `TICK 60` a 50 fps); qui il periodo è il passo e un ritardo non si accumula (in ritardo di un
// passo o più si riparte da adesso).
//
// Due consumatori, una formula:
//  * IL — <see cref="Pace"/>: `[ThreadStatic]`, come il `__thread` del runtime C (un TICK su un
//    worker GPM e uno sul main thread hanno cadenza propria). È il percorso FLAT: sul percorso
//    GPM l'END TICK parcheggia (IlGpmRuntime.TickStep) e Pace non viene chiamata. Perché resti
//    così, Pace porta la GUARDIA §0ter qui sotto: su un worker GPM fallisce subito invece di
//    dormirlo (stesso meccanismo di GpmWorkerGuard, hook IsOnGpmWorker registrato da IlGpmRuntime).
//  * PCodeVM — <see cref="Advance"/>: la cadenza è PER TASK (`PCodeTask.TickDeadlineMs`), non per
//    thread, perché sotto GOMAXPROCS i task verdi migrano tra i worker. L'handler TICK non dorme
//    (regola GPM §0ter: mai un Thread.Sleep dentro il worker): parcheggia il task su un wait
//    Timer fino alla scadenza.
using System;
using System.Threading;

namespace Basic.IlBackend;

internal static class TickPacing
{
    [ThreadStatic] private static long _deadlineMs;   // confine del frame; 0 = nessuno

    /// <summary>Passo del frame in ms per <paramref name="fps"/>; 0 = nessun limiter.</summary>
    private static long StepMs(double fps)
    {
        if (fps <= 0.0) return 0;
        double ms = 1000.0 / fps;
        if (ms < 1.0) ms = 1.0;      // rate > 1000: si attende almeno 1 ms (la granularità del clock)
        if (ms > 1000.0) ms = 1000.0;
        return (long)(ms + 0.5);
    }

    /// <summary>Calcola il confine del frame <paramref name="deadlineMs"/> SENZA attendere e
    /// restituisce il residuo da attendere in ms (0 = il frame può partire subito).
    /// <paramref name="fps"/> &lt;= 0 = nessun limiter (la scadenza resta quella che era, come
    /// il `return` anticipato di gmb_tick_pace).</summary>
    public static long Advance(ref long deadlineMs, double fps)
    {
        var step = StepMs(fps);
        if (step <= 0) return 0;
        long now = Environment.TickCount64;   // ms monotoni dall'avvio (come gmb_ticks_ms)
        long wait = 0;
        if (deadlineMs > now) wait = deadlineMs - now;                              // anticipo: si attende il confine
        else if (deadlineMs <= 0 || now - deadlineMs >= step) deadlineMs = now;     // primo frame o in ritardo di un passo
        deadlineMs += step;
        return wait;
    }

    /// <summary>Attende il confine del frame per <paramref name="fps"/> frame al secondo.
    /// <paramref name="fps"/> &lt;= 0 = nessun limiter (il frame presenta e riparte subito).</summary>
    public static void Pace(double fps)
    {
        // §0ter — GUARDIA ANTI-REGRESSIONE (stesso meccanismo di GpmWorkerGuard/MarkDriverThread).
        // Questa è l'ATTESA BLOCCANTE del TICK: su un worker GPM l'attesa deve essere un park
        // (IlGpmRuntime.TickStep + IlTimerAwait), mai Thread.Sleep. Se una futura modifica
        // rimette Pace sul percorso di un task, si ferma qui con la causa dichiarata invece di
        // dormire il worker e falsare la cadenza in modo intermittente.
        Basic.Agents.GpmWorkerGuard.Check("TickPacing.Pace (END TICK)");
        var wait = Advance(ref _deadlineMs, fps);
        if (wait > 0) Thread.Sleep((int)wait);
    }
}
