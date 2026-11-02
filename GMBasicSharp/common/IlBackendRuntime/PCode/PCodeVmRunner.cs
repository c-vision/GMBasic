// FILE: Basic.IlBackend.PCode.PCodeVmRunner.cs
// Esegue un CompiledChunk con la PCodeVM interna (C#, in-process). Usato dal percorso
// P-Code dell'Executor (default, REPL, IDE) e dai test vmtest. Il bridge C nativo
// (libgmb_vm_bridge) è stato eliminato: la PCodeVM C# è l'unico esecutore P-Code.

using System.Text;
using Basic.Runtime;
using Compiler;

namespace Basic.IlBackend.PCode;

/// <summary>Risultato dell'esecuzione di un chunk tramite PCodeVmRunner.</summary>
public sealed class PCodeVmResult
{
    public required int ExitCode { get; init; }
    public required string Output { get; init; }
    /// <summary>Errore di esecuzione (non un EXIT CODE voluto da FUNCTION Main).</summary>
    public bool RuntimeError { get; init; }
    public bool Success => !RuntimeError;
}

/// <summary>
/// Esegue un <see cref="CompiledChunk"/> con la PCodeVM interna (in-process, niente
/// bridge C). L'output è catturato su uno <see cref="StringWriter"/> collegato alla
/// console dell'host headless.
/// </summary>
public static class PCodeVmRunner
{
    /// <summary>Esegue il chunk e cattura l'output. <paramref name="useMt"/> è mantenuto
    /// per compatibilità di firma; la PCodeVM interna gestisce già la concorrenza (GPM).</summary>
    public static PCodeVmResult Run(CompiledChunk chunk, bool useMt = true)
        => Run(chunk, CancellationToken.None);

    /// <summary>Esegue il chunk propagando la cancellazione come unwind strutturato della VM:
    /// i task vengono staccati dalle attese e i FINALLY sono drenati prima dello shutdown.</summary>
    public static PCodeVmResult Run(CompiledChunk chunk, CancellationToken cancellationToken)
        => Run(chunk, cancellationToken, prepareGraphicsHost: true);

    /// <summary><paramref name="prepareGraphicsHost"/>=false → NON avvia l'host grafico SDL/C
    /// (DefaultGraphics resta Null): i park grafici completano subito e nessuna finestra viene
    /// aperta. Usato dalla CLI quando lo stdout NON è un TTY (pipe/CI/capture): aprire l'host
    /// grafico solo quando serve davvero (terminale interattivo), ed evitare l'hang da park
    /// non pompato.</summary>
    public static PCodeVmResult Run(CompiledChunk chunk, CancellationToken cancellationToken, bool prepareGraphicsHost)
        => Run(chunk, cancellationToken, prepareGraphicsHost, liveOut: null);

    /// <summary>Come <see cref="Run(CompiledChunk, CancellationToken, bool)"/>, ma con output
    /// LIVE: <paramref name="liveOut"/> (di norma la console) riceve le righe mentre il programma
    /// gira, mentre la cattura per <see cref="PCodeVmResult.Output"/> resta identica (tee).
    /// Serve ai processi lunghi (server/daemon): senza, l'utente non vede nulla fino all'uscita
    /// e la memoria cresce senza limite (bug reale: log vuoto della webapp).</summary>
    public static PCodeVmResult Run(CompiledChunk chunk, CancellationToken cancellationToken,
        bool prepareGraphicsHost, TextWriter? liveOut)
    {
        ArgumentNullException.ThrowIfNull(chunk);

        // PrepareGraphicsHost DEVE girare prima di costruire HeadlessRuntimeHost: imposta
        // HeadlessRuntimeHost.DefaultGraphics (letto dal costruttore), altrimenti la PCodeVM
        // resta headless per sempre anche se SCREEN viene chiamato (stesso ordine richiesto
        // dal backend IL, vedi IlRuntime.RunGpmDriverWithGraphicsPump).
        if (prepareGraphicsHost) IlRuntime.PrepareGraphicsHost();

        var sw = new StringWriter();
        var sink = liveOut is null ? (TextWriter)sw : new CommonLib.TeeTextWriter(sw, liveOut);
        var host = new HeadlessRuntimeHost(sink, null);
        try
        {
            IlRuntime.SetCurrent(host);
            PCodeGraphics.Bind(host); /* routing grafico: motore C se host.Graphics è CGfx */
            using var vm = new PCodeVm(chunk, host);
            RunWithGraphicsPump(vm, host, cancellationToken);
            if (vm.ExecutionException is { } ex)
            {
                sw.Write($"[P-CODE ERROR] {ex.Message}");
                // Errore fatale (nessun TRY/ON ERROR): exit code canonico 6 su tutte le gambe
                // (IL/PCode, VM-C, C99Native). Vedi Bugs/(solved) - Semantica-Errore-Builtin...
                return new PCodeVmResult { ExitCode = 6, Output = sw.ToString(), RuntimeError = true };
            }
            return new PCodeVmResult { ExitCode = Basic.IlBackend.IlRuntime.ProgramExitCode, Output = sw.ToString() };
        }
        catch (Exception ex)
        {
            sw.Write($"[P-CODE ERROR] {ex.Message}");
            return new PCodeVmResult { ExitCode = 6, Output = sw.ToString(), RuntimeError = true };
        }
    }

    /// <summary>PCodeVm.Run() spawna i propri worker thread (GOMAXPROCS) ed esegue un
    /// busy-wait BLOCCANTE sul thread chiamante finché i task non finiscono — esattamente
    /// come IlGpmDriver.Run() lato backend IL. Cocoa/SDL impongono che le chiamate
    /// SDL/UI stiano SEMPRE sul vero thread main del processo: se il chiamante lo è, si
    /// usa quel tempo di attesa per pompare la finestra grafica (stesso schema di
    /// IlRuntime.RunGpmDriverWithGraphicsPump, duplicato qui invece di condiviso perché
    /// PCodeVm non è un IlGpmDriver). Se il chiamante non è il vero main thread (es. un
    /// harness esterno ha già la responsabilità del pump), esegue in modo sincrono senza
    /// toccare SDL da qui.</summary>
    private static void RunWithGraphicsPump(PCodeVm vm, IRuntimeHost host, CancellationToken cancellationToken)
    {
        // Headless (GMBASIC_HEADLESS=1, es. stdout non-TTY): niente SDL/host grafico da
        // inizializzare e pompare → esecuzione sincrona (i park completano subito).
        if (Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") == "1")
        {
            vm.Run(cancellationToken);
            return;
        }
        if (Environment.CurrentManagedThreadId != 1)
        {
            vm.Run(cancellationToken);
            return;
        }

        // Motore grafico unico C di DEFAULT (GMB_GFX_C=0 per il vecchio motore C#).
        var useCGfx = CGfxGraphicsService.UseCGraphics;
        var quitRequested = false;
        using var linked = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);

        if (useCGfx)
        {
            try { CGfxGraphicsService.MainThreadStart(); } catch { /* ignore */ }
        }
        else
        {
            try { CommonGraphics.Graphics.GraphicsManager.Instance.InitializeOnMainThread(); } catch { /* ignore */ }
            try
            {
                CommonGraphics.Consoles.SdlManager.Instance.QuitRequested += _ => quitRequested = true;
                CommonGraphics.Graphics.GraphicsManager.Instance.WindowCloseRequested += () => quitRequested = true;
            }
            catch { /* ignore */ }
        }

        Exception? vmException = null;
        var vmThread = new Thread(() =>
        {
            try { vm.Run(linked.Token); }
            catch (Exception ex) { vmException = ex; }
        })
        { IsBackground = true, Name = "pcode-vm-main-driver" };
        vmThread.Start();

        while (vmThread.IsAlive && !quitRequested)
        {
            try
            {
                if (useCGfx)
                {
                    CGfxGraphicsService.MainThreadTick();
                    if (CGfxGraphicsService.MainThreadShouldQuit()) { quitRequested = true; linked.Cancel(); }
                    DrainCGfxInputPCode(host, linked);
                }
                else
                {
                    CommonGraphics.Consoles.SdlManager.Instance.PollEvents();
                    CommonGraphics.Graphics.GraphicsManager.Instance.Present();
                }
                // TRAY: il pump della coda va fatto SUL MAIN THREAD (Cocoa/NSStatusItem);
                // qui il BASIC gira sul thread del driver. Solo se il programma usa TRAY.
                if (vm.UsesTray) { try { TrayUtils.Poll(); } catch { /* lib assente */ } }
            }
            catch { /* ignore */ }
            Thread.Sleep(16);
        }
        vmThread.Join(2000);

        // Ultimo flush: la coda grafica può contenere comandi accodati dall'ultimo chunk
        // eseguito (es. render finale) — dare tempo al Present di eseguirli prima di uscire.
        try
        {
            if (useCGfx)
            {
                CGfxGraphicsService.MainThreadTick();
                DrainCGfxInputPCode(host, linked);
                CGfxGraphicsService.MainThreadStop();
            }
            else
            {
                CommonGraphics.Consoles.SdlManager.Instance.PollEvents();
                CommonGraphics.Graphics.GraphicsManager.Instance.Present();
            }
        }
        catch { /* ignore */ }

        if (vmException != null) throw vmException;
    }

    /// <summary>Drena l'input del motore C per la PCodeVM (SOLO main thread): i tasti
    /// vanno al KeyboardService dell'host (INKEY/WAITKEY/GET li leggono lì), il mouse allo
    /// stato condiviso di IlRuntime (MOUSE.X/Y/LEFT + ON MOUSE), Ctrl+C annulla il token.</summary>
    private static void DrainCGfxInputPCode(IRuntimeHost host, CancellationTokenSource linked)
    {
        try
        {
            while (CGfxInputService.TryPollKey(out var k))
                foreach (var ch in k) host.Keyboard.SendKey(ch);
            while (CGfxInputService.TryPollMouse(out var t, out var x, out var y, out var b, out var w))
                IlRuntime.OnCGfxMouseEvent(t, x, y, b, w);
            if (CGfxInputService.TakeCtrlC()) { linked.Cancel(); }
        }
        catch { /* ignore */ }
    }
}
