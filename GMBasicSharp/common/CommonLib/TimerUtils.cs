// FILE: CommonLib.TimerUtils.cs

using System.Collections.Concurrent;
using System.Diagnostics;

/* 
 * TimerUtils.cs: utilità per la gestione di timer ad alta precisione (stopwatch) e delay
 * 
 * FUNZIONI:
 * - StopwatchStart(): avvia uno stopwatch e restituisce un handle numerico
 * - StopwatchStop(handle): ferma uno stopwatch e restituisce il tempo in nanosecondi
 * - StopwatchElapsed(handle): restituisce il tempo trascorso senza fermare il timer
 * - Delay(ms): delay sincrono in millisecondi
 */

namespace CommonLib;

/// <summary>
/// Utility per la gestione di timer ad alta precisione (stopwatch) e delay.
/// Mantiene un registro thread-safe di stopwatch accessibili tramite handle numerico.
/// I metodi sono statici e stateless, utilizzabili senza istanza.
/// </summary>
public static class TimerUtils
{
    /// <summary>Registro degli stopwatch attivi, accessibili per handle.</summary>
    private static readonly ConcurrentDictionary<int, Stopwatch> Timers = new();

    /// <summary>Contatore atomico per generare handle univoci per ogni timer.</summary>
    private static int _nextTimerHandle = 1;

    /// <summary>
    /// Avvia uno stopwatch ad alta precisione e restituisce un handle numerico
    /// per riferirsi ad esso nelle successive chiamate a StopwatchStop e StopwatchElapsed.
    /// </summary>
    /// <returns>Handle numerico univoco per lo stopwatch creato.</returns>
    public static int StopwatchStart()
    {
        var handle = Interlocked.Increment(ref _nextTimerHandle);
        var sw = Stopwatch.StartNew();
        Timers[handle] = sw;
        return handle;
    }

    /// <summary>
    /// Ferma uno stopwatch identificato dall'handle e restituisce il tempo trascorso
    /// in nanosecondi ad alta precisione. Lo stopwatch viene rimosso dal registro.
    /// </summary>
    /// <param name="handle">Handle dello stopwatch da fermare.</param>
    /// <returns>Tempo trascorso in nanosecondi.</returns>
    /// <exception cref="ArgumentException">Se l'handle non corrisponde a nessun timer attivo.</exception>
    public static long StopwatchStop(int handle)
    {
        if (Timers.TryRemove(handle, out var sw))
        {
            sw.Stop();
            return sw.ElapsedTicks * 1_000_000_000L / Stopwatch.Frequency;
        }
        throw new ArgumentException("Handle timer non valido.");
    }

    /// <summary>
    /// Restituisce il tempo trascorso in nanosecondi per l'handle specificato,
    /// senza fermare il timer. Lo stopwatch continua a misurare.
    /// </summary>
    /// <param name="handle">Handle dello stopwatch da interrogare.</param>
    /// <returns>Tempo trascorso in nanosecondi.</returns>
    /// <exception cref="ArgumentException">Se l'handle non corrisponde a nessun timer attivo.</exception>
    public static long StopwatchElapsed(int handle)
    {
        if (Timers.TryGetValue(handle, out var sw))
            return sw.ElapsedTicks * 1_000_000_000L / Stopwatch.Frequency;
        throw new ArgumentException("Handle timer non valido.");
    }

    /// <summary>Delay sincrono multipiattaforma in millisecondi. Blocca il thread corrente.</summary>
    /// <param name="ms">Millisecondi di attesa.</param>
    public static void Delay(int ms) => Thread.Sleep(ms);
}
