// FILE: Basic.IlBackend.IlConsoleKeyFeeder.cs
// Feeder console → coda INKEY del backend IL (GPM-safe, pattern IlSdlInput).
//
// PROBLEMA CHIUSO: nel percorso GPM v2 (Main may-park → driver v2) WAITKEY/GET/INKEY
// NON leggono mai la console: WaitKeyStep consuma SOLO la coda statica
// IlRuntime.InkeyQueue e parcheggia in InkeyWaiters (vedi IlGpmRuntime.WaitKeyStep).
// La coda è alimentata da feeder esterni: SDL (IlSdlInput.Bind), test (--key/QueueInkey).
// Per la CONSOLE REALE non esisteva alcun feeder → un eseguibile v2 su console reale che
// fa WAITKEY non vedeva MAI i tasti (solo il percorso flat/lineare leggeva System.Console
// via fallback in IlRuntime.ReadKey, che NON è mai chiamato nel v2).
//
// QUESTO FILE è il feeder console equivalente a IlSdlInput: un loop che legge
// System.Console e chiama IlRuntime.QueueInkey per ogni tasto.
//
// REGOLE GPM rispettate (vedi AGENTS §0ter + piani canonici ILBackend v2):
//   • il loop gira su un task dedicato (feeder), MAI su un worker GPM;
//   • chiama SOLO IlRuntime.QueueInkey (enqueue + Task.Wake dei waiter parcheggiati),
//     MAI codice BASIC, MAI handler in diretta (il dispatch ON KEY avviene nel contesto
//     del consumatore via TryDequeueInkey);
//   • NON usa KeyboardServiceConsole: il suo ListenerLoop invocherebbe gli hook ON KEY
//     sul thread del listener (violazione GPM) e il suo ctor sottoscrive
//     Console.CancelKeyPress (doppia gestione Ctrl+C con l'host HeadlessRuntimeHost);
//   • il listener console resta l'host (HeadlessKeyboardService gestisce SIGINT/Ctrl+C
//     via PosixSignalRegistration → BreakRequested → OnKeyboardBreak).
//
// Vedi TODO (open) Attivare-KeyboardServiceConsole-Start-e-aggancio-ClipperKeyInput.

namespace Basic.IlBackend;

/// <summary>Feeder console → IlRuntime.QueueInkey (GPM-safe). Attivo solo quando la
/// console è reale interattiva (input non rediretta).</summary>
public static class IlConsoleKeyFeeder
{
    private static readonly object Sync = new();
    private static CancellationTokenSource? _cts;
    private static Task? _loop;

    /// <summary>Avvia il feeder console se la console è reale (input NON rediretta) e non
    /// è già attivo. No-op in test/CI (stdin pipe/redirect) e quando GMBASIC_HEADLESS=1.</summary>
    public static void Start()
    {
        try
        {
            if (Console.IsInputRedirected) return;
            if (Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") == "1") return;
        }
        catch
        {
            return; // console non disponibile
        }

        lock (Sync)
        {
            if (_cts is not null) return; // già attivo
            var cts = new CancellationTokenSource();
            _cts = cts;
            _loop = Task.Run(() => LoopAsync(cts.Token));
        }
    }

    /// <summary>Ferma il feeder console (se attivo). Idempotente.</summary>
    public static void Stop()
    {
        CancellationTokenSource? cts;
        Task? loop;
        lock (Sync)
        {
            cts = _cts;
            loop = _loop;
            _cts = null;
            _loop = null;
        }
        if (cts is null) return;
        try { cts.Cancel(); } catch { /* già cancellato */ }
        try { loop?.Wait(500); } catch { /* task terminato/annullato */ }
        cts.Dispose();
    }

    /// <summary>True se un feeder console è attivo (usato per disabilitare i fallback
    /// System.Console concorrenti: quando il feeder drena la console, i lettori diretti
    /// non devono più leggere System.Console).</summary>
    public static bool IsActive
    {
        get { lock (Sync) return _cts is not null; }
    }

    private static async Task LoopAsync(CancellationToken token)
    {
        try
        {
            while (!token.IsCancellationRequested)
            {
                try
                {
                    if (Console.KeyAvailable)
                    {
                        var key = Console.ReadKey(intercept: true);
                        RouteKey(key);
                    }
                }
                catch (InvalidOperationException)
                {
                    return; // console non più disponibile (es. stdin chiuso)
                }
                catch (OperationCanceledException)
                {
                    return;
                }
                catch
                {
                    // qualsiasi altro errore di lettura: non far morire il feeder
                }
                try { await Task.Delay(15, token); } catch (OperationCanceledException) { return; }
            }
        }
        catch (OperationCanceledException)
        {
            // stop richiesto
        }
    }

    /// <summary>Mappa un ConsoleKeyInfo al formato stringa atteso da InkeyQueue
    /// (stessa convenzione di IlSdlInput: "\r" Enter, "\x1b" ESC, "\b" Backspace,
    /// "\t" Tab, "\x03" Ctrl+C interceptato, altrimenti il carattere).</summary>
    private static void RouteKey(ConsoleKeyInfo key)
    {
        // Ctrl+C: se il programma ha un hook ON KEY (IsCtrlCIntercepted) lo si inoltra
        // come "\x03" (il dispatch lo fa il consumatore); altrimenti abort.
        if (key.Modifiers.HasFlag(ConsoleModifiers.Control) && key.Key == ConsoleKey.C)
        {
            if (IlRuntime.IsCtrlCIntercepted())
                IlRuntime.QueueInkey("\x03");
            else
                IlRuntime.RequestAbort();
            return;
        }
        switch (key.Key)
        {
            case ConsoleKey.Enter: IlRuntime.QueueInkey("\r"); return;
            case ConsoleKey.Escape: IlRuntime.QueueInkey("\x1b"); return;
            case ConsoleKey.Backspace: IlRuntime.QueueInkey("\b"); return;
            case ConsoleKey.Tab: IlRuntime.QueueInkey("\t"); return;
        }
        var c = key.KeyChar;
        if (c != '\0' && !char.IsControl(c))
            IlRuntime.QueueInkey(c.ToString());
        // tasti funzione/frecce/modifier senza KeyChar stampabile: ignorati (coerente col
        // percorso flat che legge System.Console.ReadKey e usa solo il KeyChar).
    }
}
