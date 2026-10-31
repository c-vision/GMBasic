using CommonLib.Consoles;
// FILE: Interpreter.Consoles.SdlManager.cs

using SDL2;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Consoles;

/// <summary>
/// Gestisce il ciclo di vita e gli eventi di SDL in un unico punto centralizzato.
/// È responsabile per l'inizializzazione, il polling degli eventi e la chiusura
/// dei sottosistemi SDL, garantendo che queste operazioni avvengano una sola volta.
/// </summary>
public sealed class SdlManager : IDisposable
{
    #region Singleton

    private static readonly Lazy<SdlManager> _lazyInstance = new(() => new SdlManager());
    public static SdlManager Instance => _lazyInstance.Value;

    private SdlManager() { }

    #endregion

    private bool _isInitialized;
    private bool _disposed;

    /// <summary>
    /// Inizializza i sottosistemi SDL (Video e TTF) in modo sicuro.
    /// Se chiamato più volte, esegue l'inizializzazione solo la prima volta.
    /// </summary>
    public void Initialize()
    {
        if (_isInitialized) return;

        // Se GMBASIC_HEADLESS o SDL_VIDEODRIVER=dummy, salta VIDEO init
        var isHeadless = Environment.GetEnvironmentVariable("GMBASIC_HEADLESS") == "1"
            || Environment.GetEnvironmentVariable("SDL_VIDEODRIVER") == "dummy";

        var initFlags = SDL.SDL_INIT_TIMER | SDL.SDL_INIT_AUDIO | SDL.SDL_INIT_JOYSTICK;
        if (!isHeadless)
            initFlags |= SDL.SDL_INIT_VIDEO;

        SDL.SDL_SetMainReady();
        if (SDL.SDL_Init(initFlags) < 0)
            throw new InvalidOperationException($"SDL_Init failed: {SDL.SDL_GetError()}");
        
        if (SDL_ttf.TTF_Init() == -1)
        {
            SDL.SDL_Quit(); // Pulisci SDL_Init se TTF fallisce
            throw new InvalidOperationException($"TTF_Init failed: {SDL.SDL_GetError()}");
        }

        _isInitialized = true;
    }

    // --- Eventi Pubblici ---

    public event Action<SDL.SDL_Event>? QuitRequested;
    public event Action<SDL.SDL_Event>? WindowEvent;
    public event Action<SDL.SDL_Event>? KeyboardEvent;
    public event Action<SDL.SDL_Event>? TextInputEvent;
    public event Action<SDL.SDL_Event>? OtherEvent;
    public event Action<SDL.SDL_Event>? MouseWheelEvent;
    public event Action<SDL.SDL_Event>? MouseButtonEvent;
    public event Action<SDL.SDL_Event>? MouseMotionEvent;

    /// <summary>
    /// Esegue il polling di tutti gli eventi SDL e li smista.
    /// Questo è l'unico metodo che deve chiamare SDL_PollEvent.
    /// </summary>
    public void PollEvents()
    {
        while (SDL.SDL_PollEvent(out var e) != 0)
        {
            switch (e.type)
            {
                case SDL.SDL_EventType.SDL_QUIT:
                    QuitRequested?.Invoke(e);
                    break;
                case SDL.SDL_EventType.SDL_WINDOWEVENT:
                    WindowEvent?.Invoke(e);
                    break;
                case SDL.SDL_EventType.SDL_KEYDOWN:
                case SDL.SDL_EventType.SDL_KEYUP:
                    KeyboardEvent?.Invoke(e);
                    break;
                case SDL.SDL_EventType.SDL_TEXTINPUT:
                    TextInputEvent?.Invoke(e);
                    break;
                case SDL.SDL_EventType.SDL_MOUSEWHEEL:
                    MouseWheelEvent?.Invoke(e);
                    break;
                case SDL.SDL_EventType.SDL_MOUSEBUTTONDOWN:
                case SDL.SDL_EventType.SDL_MOUSEBUTTONUP:
                    MouseButtonEvent?.Invoke(e);
                    break;
                case SDL.SDL_EventType.SDL_MOUSEMOTION:
                    MouseMotionEvent?.Invoke(e);
                    break;
                default:
                    OtherEvent?.Invoke(e);
                    break;
            }
        }
        // Aggiunge una pausa minima di 1ms per cedere il controllo alla CPU
        // ed evitare un tight-loop che consuma il 100% delle risorse.
        // Questo risolve i problemi di performance quando la REPL è inattiva o durante l'esecuzione
        // di programmi, senza impattare la reattività dell'interfaccia.
        SDL.SDL_Delay(1);
    }

    /// <summary>
    /// Chiude tutti i sottosistemi SDL in modo sicuro.
    /// Può essere chiamato più volte senza causare errori.
    /// </summary>
    public void Quit()
    {
        if (!_isInitialized || _disposed) return;
            
        if (SDL_ttf.TTF_WasInit() == 1)
            SDL_ttf.TTF_Quit();

        SDL.SDL_Quit();
        _disposed = true;
    }

    public void Dispose()
    {
        Quit();
    }
}