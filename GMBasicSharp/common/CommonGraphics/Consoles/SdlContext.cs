// FILE: GraphicsManager.Consoles.SdlContext.cs

using SDL2;

namespace CommonLib.Consoles;

/// <summary>
/// Gestisce in modo sicuro il ciclo di vita di una coppia SDL Window e Renderer.
/// Incapsula i puntatori IntPtr e implementa IDisposable per garantire
/// che le risorse vengano sempre rilasciate correttamente.
/// </summary>
public sealed class SdlContext : IDisposable
{
    public IntPtr WindowHandle { get; private set; }
    public IntPtr RendererHandle { get; private set; }

    private bool _disposed;

    /// <summary>
    /// Crea una nuova finestra e il renderer associato.
    /// Lancia un'eccezione se la creazione fallisce.
    /// </summary>
    /// <param name="title">Il titolo per la barra della finestra.</param>
    /// <param name="x">La posizione X iniziale della finestra.</param>
    /// <param name="y">La posizione Y iniziale della finestra.</param>
    /// <param name="w">La larghezza iniziale in pixel della finestra.</param>
    /// <param name="h">L'altezza iniziale in pixel della finestra.</param>
    /// <param name="flags">I flag SDL da applicare alla creazione della finestra.</param>
    /// <exception cref="InvalidOperationException">Lanciata se la creazione della finestra o del renderer fallisce.</exception>
    public SdlContext(string title, int x, int y, int w, int h, SDL.SDL_WindowFlags flags)
    {
        WindowHandle = SDL.SDL_CreateWindow(title, x, y, w, h, flags);
        if (WindowHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException($"SDL_CreateWindow failed: {SDL.SDL_GetError()}");
        }

        // Porta la finestra in primo piano e le dà il focus di input: senza questo,
        // la finestra può nascere dietro altre e non ricevere i tasti.
        SDL.SDL_RaiseWindow(WindowHandle);
        SDL.SDL_SetWindowInputFocus(WindowHandle);

        // IMPOSTAZIONE CHIAVE: Dice a SDL di usare lo scaling Nearest (pixel-perfect)
        // per tutte le operazioni di rendering. Questo evita l'effetto "annacquato"
        // sui display Retina/HiDPI, mantenendo i colori vivaci e i bordi netti.
        // "0" = Nearest-pixel, "1" = Linear, "2" = Anisotropic. Come in GMO (commit
        // 24931646, rendering nativo HiDPI): resta "1" (irrilevante col blit per-pixel
        // degli sprite e col Present 1:1 — il testo è già rasterizzato nativo).
        SDL.SDL_SetHint(SDL.SDL_HINT_RENDER_SCALE_QUALITY, "1");
        
        // --- Impostazioni specifiche per macOS Retina/HiDPI ---
        // Se siamo su macOS, forziamo il driver Metal che gestisce meglio i colori Retina.
        // Con SDL_VIDEODRIVER=dummy (test headless, nessuna finestra) il driver Metal non
        // esiste: SDL_CreateRenderer fallirebbe e IsScreenOn resterebbe false.
        if (OperatingSystem.IsMacOS() &&
            Environment.GetEnvironmentVariable("SDL_VIDEODRIVER") != "dummy")
        {
            SDL.SDL_SetHint(SDL.SDL_HINT_RENDER_DRIVER, "metal");
            SDL.SDL_SetHint("SDL_RENDER_BATCHING", "1");
        }
        
        RendererHandle = SDL.SDL_CreateRenderer(WindowHandle, -1,
            SDL.SDL_RendererFlags.SDL_RENDERER_ACCELERATED | SDL.SDL_RendererFlags.SDL_RENDERER_PRESENTVSYNC);
        if (RendererHandle == IntPtr.Zero)
        {
            // Fallback software (es. SDL_VIDEODRIVER=dummy nei test headless): il driver
            // dummy non ha accelerazione, ma il renderer software funziona senza finestra.
            RendererHandle = SDL.SDL_CreateRenderer(WindowHandle, -1,
                SDL.SDL_RendererFlags.SDL_RENDERER_SOFTWARE);
        }
        if (RendererHandle == IntPtr.Zero)
        {
            // Se la creazione del renderer fallisce, distruggi la finestra che abbiamo appena creato.
            SDL.SDL_DestroyWindow(WindowHandle);
            throw new InvalidOperationException($"SDL_CreateRenderer failed: {SDL.SDL_GetError()}");
        }
    }

    public void Dispose()
    {
        if (_disposed) return;

        // La sequenza di distruzione è importante: prima il renderer, poi la finestra.
        if (RendererHandle != IntPtr.Zero)
        {
            SDL.SDL_DestroyRenderer(RendererHandle);
            RendererHandle = IntPtr.Zero;
        }

        if (WindowHandle != IntPtr.Zero)
        {
            SDL.SDL_DestroyWindow(WindowHandle);
            WindowHandle = IntPtr.Zero;
        }

        _disposed = true;
    }
}