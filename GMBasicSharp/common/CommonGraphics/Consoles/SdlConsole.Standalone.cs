/*// FILE: Interpreter.Consoles.SdlConsole.Standalone.cs

using System.Text;
using SDL2;

namespace CommonGraphics;

/// <summary>
/// Implementazione di IConsole che crea e gestisce una finestra SDL
/// per emulare una console di testo completa, con input e output.
/// Questa classe è una parte parziale; la logica di I/O e gestione dello stato
/// è definita in SdlConsole.cs.
/// </summary>
public sealed partial class SdlConsole
{
    /// <summary>
    /// Inizializza una nuova istanza della console SDL per uso standalone (senza KeyboardService), 
    /// usando una configurazione per i dettagli e permettendo di sovrascrivere titolo e dimensioni.
    /// </summary>
    /// <param name="title">Il titolo della finestra della console. Sovrascrive il titolo dalla configurazione.</param>
    /// <param name="gridWidth">La larghezza della griglia di caratteri. Sovrascrive la larghezza dalla configurazione.</param>
    /// <param name="gridHeight">L'altezza della griglia di caratteri. Sovrascrive l'altezza dalla configurazione.</param>
    /// <param name="config">La configurazione opzionale da cui leggere i parametri. Se null, viene usata la
    /// configurazione di default per la modalità testo.</param>
    public SdlConsole(string title = "GMBASIC AI POWERED REPL", int gridWidth = 80, int gridHeight = 50,
        ScreenConfig? config = null)
    {
        // Imposta sia la configurazione iniziale (immutabile) che quella corrente.
        _initialConfig = (config ?? ScreenConfig.GetDefault(ScreenMode.Text)) with
        {
            CharGridWidth = gridWidth,
            CharGridHeight = gridHeight
        };
        Configuration = _initialConfig;
        
        Title = title;
        Width = Configuration.CharGridWidth ?? 80;
        Height = Configuration.CharGridHeight ?? 50;
        _palette = Configuration.Palette;
        _viewWidth = Width;
        _viewHeight = Height;
        _screenBuffer = new ScreenCell[Height, Width];
        _escapeProcessor = new EscapeSequences(this);
        var consoleWriter = new SdlConsoleWriter(this);
        Out = consoleWriter;
        Error = consoleWriter;
        In = new StringReader("");
        
        SdlManager.Instance.Initialize();
        UpdateFontAndRecalibrate();

        ((ITerminalBuffer)this).CurrentForegroundColor = Configuration.DefaultFgColor;
        ((ITerminalBuffer)this).CurrentBackgroundColor = Configuration.DefaultBgColor;

        Clear();
        _cursorBlinkTimer.Start();
    }
    
    /// <summary>
    /// Processa internamente gli eventi SDL. Usato come fallback senza un event manager esterno.
    /// </summary>
    private bool ProcessEventsInternalPolling()
    {
        while (SDL.SDL_PollEvent(out var e) != 0)
        {
            switch (e.type)
            {
                case SDL.SDL_EventType.SDL_QUIT: _quitRequested = true; break;
                case SDL.SDL_EventType.SDL_WINDOWEVENT
                    when e.window.windowEvent == SDL.SDL_WindowEventID.SDL_WINDOWEVENT_CLOSE:
                    _quitRequested = true; break;
                case SDL.SDL_EventType.SDL_TEXTINPUT:
                    unsafe
                    {
                        var text = Encoding.UTF8.GetString(e.text.text, 32).TrimEnd('\0');
                        if (!string.IsNullOrEmpty(text))
                        {
                            _keyQueue.Enqueue(new TerminalKeyInfo(text[0], 0, false, false, false));
                        }
                    }

                    break;
                case SDL.SDL_EventType.SDL_KEYDOWN:
                    var keyCode = (int)e.key.keysym.sym;
                    var mod = e.key.keysym.mod;
                    var isCtrl = (mod & SDL.SDL_Keymod.KMOD_CTRL) != 0;
                    var isShift = (mod & SDL.SDL_Keymod.KMOD_SHIFT) != 0;
                    var isAlt = (mod & SDL.SDL_Keymod.KMOD_ALT) != 0;
                    if (isCtrl && e.key.keysym.sym == SDL.SDL_Keycode.SDLK_c)
                    {
                        var args = new SdlConsoleCancelEventArgs(ConsoleSpecialKey.ControlC);
                        CancelKeyPress?.Invoke(this, args);
                        if (!args.Cancel) _quitRequested = true;
                        continue;
                    }

                    var keyChar = keyCode switch
                    {
                        (int)SDL.SDL_Keycode.SDLK_RETURN => '\r',
                        (int)SDL.SDL_Keycode.SDLK_BACKSPACE => '\b',
                        _ => '\0'
                    };
                    _keyQueue.Enqueue(new TerminalKeyInfo(keyChar, keyCode, isShift, isAlt, isCtrl));
                    break;
            }
        }

        return _quitRequested;
    }
}*/