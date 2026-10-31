using CommonLib.Clipboard;
using CommonLib.Screens;
using CommonLib.Keyboard;
using CommonLib.Consoles;
using CommonLib.Consoles.Tui;
// FILE: Interpreter.Consoles.SdlConsole.cs

using System.Diagnostics;
using System.Drawing;
using CommonGraphics.Fonts;
using CommonGraphics.Terminal;
using SDL2;

namespace CommonGraphics.Consoles;

/// <summary>
/// Implementazione di IConsole che crea e gestisce una finestra SDL
/// per emulare una console di testo completa. Si basa su un buffer di celle interno,
/// gestisce il proprio ciclo di eventi e delega la logica di input bloccante
/// al KeyboardService per permettere l'intercettazione degli hook.
/// </summary>
public sealed partial class SdlConsole : IConsole, ITerminalBuffer
{
    #region Campi Privati

    // --- Dipendenze e Risorse SDL ---
    private readonly SdlContext? _context;
    private IntPtr _font;
    private IntPtr _screenTexture = IntPtr.Zero;
    private bool _isShown;
    private bool _quitRequested;
    private readonly EscapeSequences _escapeProcessor;
    private byte[][] _palette;
    private string? _activePaletteName;
    private readonly IClipboardService _clipboardService;
    private readonly IKeyboardService _keyboardService;

    // --- Stato del Buffer ---
    private readonly ScreenCell[,] _screenBuffer;
    
    /// <summary>
    /// Buffer TUI condiviso (ScreenBuffer) che avvolge _screenBuffer per
    /// l'integrazione con EventLoop e controlli ClipperLib.
    /// </summary>
    private ScreenBuffer _tuiScreen = null!;
    
    /// <summary>Espone il buffer TUI condiviso.</summary>
    public ScreenBuffer Screen => _tuiScreen;
    private readonly Stopwatch _cursorBlinkTimer = new();
    private int _previousCursorRow;
    private int _previousCursorCol;
    private bool _wasCursorVisibleLastFrame;

    // --- Stato del Rendering ---
    private bool _forceRedraw = true;
    private bool _isContentDirty = true;
    private DateTime _lastResizeTime;
    private bool _pendingResize;
    private readonly HashSet<Point> _dirtyCells = new();
    private readonly object _renderLock = new();  // protects _screenBuffer, _cursor*, _dirtyCells, _isContentDirty, _forceRedraw

    // --- Stato della Vista e Cursore ---
    private int _viewTop, _viewLeft, _viewWidth, _viewHeight;
    private int _cursorRow, _cursorCol;
    private int _scrollTop, _scrollBottom = -1;

    private int _charWidth, _charHeight;

    // --- Risorse Font ---
    private FontAtlas? _fontAtlas;
    private string? _activeFontPath;
    private readonly ScreenConfig _initialConfig;

    /// <summary>
    /// Il campo privato che memorizza la larghezza del buffer in colonne.
    /// </summary>
    private int _width;

    /// <summary>
    /// Il campo privato che memorizza l'altezza del buffer in righe.
    /// </summary>
    private int _height;

    #endregion

    #region Costruttore e Dispose

    /// <summary>
    /// Inizializza una nuova istanza della classe SdlConsole.
    /// </summary>
    /// <param name="clipboardService">Il servizio per l'interazione con gli appunti.</param>
    /// <param name="config">La configurazione dello schermo da applicare.</param>
    public SdlConsole(IClipboardService clipboardService, ScreenConfig? config = null)
    {
        _clipboardService = clipboardService;
        _keyboardService = Keyboard.Keyboard.KeyboardServiceSdl.Instance; 
        
        _initialConfig = config ?? ScreenConfig.GetDefault(ScreenMode.Text);
        Configuration = _initialConfig;

        Title = "GMBASIC - REPL";
        _width = Configuration.CharGridWidth ?? 80;
        _height = Configuration.CharGridHeight ?? 50;
        _palette = Configuration.ResolvedPalette.Length > 0
            ? Configuration.ResolvedPalette
            : Configuration.PaletteData ?? ScreenConfig.GetDefault(ScreenMode.Text).ResolvedPalette;
        _activePaletteName = Configuration.PaletteName;
        _viewWidth = _width;
        _viewHeight = _height;
        _screenBuffer = new ScreenCell[_height, _width];
        _tuiScreen = new ScreenBuffer(_width, _height);
        _escapeProcessor = new EscapeSequences(this);
        
        var consoleWriter = new SdlConsoleWriter(this);
        Out = consoleWriter;
        Error = consoleWriter;
        In = new StringReader(""); 
    
        SdlManager.Instance.Initialize();

        // --- Crea il contesto qui ---
        _context = new SdlContext(Title, SDL.SDL_WINDOWPOS_CENTERED, SDL.SDL_WINDOWPOS_CENTERED, 1, 1,
            SDL.SDL_WindowFlags.SDL_WINDOW_HIDDEN | SDL.SDL_WindowFlags.SDL_WINDOW_RESIZABLE | SDL.SDL_WindowFlags.SDL_WINDOW_ALLOW_HIGHDPI);
        
        // Si iscrive agli eventi una sola volta.
        SdlManager.Instance.WindowEvent += HandleWindowEvent;
        SdlManager.Instance.QuitRequested += HandleQuitRequest;
        SdlManager.Instance.MouseWheelEvent += HandleMouseWheel;
        
        UpdateFontAndRecalibrate();
    
        ((ITerminalBuffer)this).CurrentForegroundColor = Configuration.DefaultFgColor;
        ((ITerminalBuffer)this).CurrentBackgroundColor = Configuration.DefaultBgColor;

        Clear();
        _cursorBlinkTimer.Start();
        
        _keyboardService.BreakRequested += OnBreakRequested;
    }

    /// <summary>
    /// Rilascia tutte le risorse SDL utilizzate dalla console.
    /// </summary>
    public void Dispose()
    {
        SdlManager.Instance.WindowEvent -= HandleWindowEvent;
        SdlManager.Instance.QuitRequested -= HandleQuitRequest;
        _keyboardService.BreakRequested -= OnBreakRequested;
        SdlManager.Instance.MouseWheelEvent -= HandleMouseWheel;
        
        _fontAtlas?.Dispose();
        _fontAtlas = null;

        if (_font != IntPtr.Zero)
        {
            SDL_ttf.TTF_CloseFont(_font);
            _font = IntPtr.Zero;
        }

        if (_screenTexture != IntPtr.Zero)
        {
            SDL.SDL_DestroyTexture(_screenTexture);
            _screenTexture = IntPtr.Zero;
        }

        _context?.Dispose();
    }

    #endregion

    #region Gestori Eventi e Metodi Privati

    private void HandleMouseWheel(SDL.SDL_Event e)
    {
        // Assicurati che l'evento sia per la finestra di questa console
        if (_context is null || e.wheel.windowID != SDL.SDL_GetWindowID(_context.WindowHandle)) return;

        // Modifica l'offset della vista (_viewTop)
        // e.wheel.y > 0 è scroll verso l'alto (vista indietro nel buffer)
        // e.wheel.y < 0 è scroll verso il basso (vista avanti nel buffer)
        _viewTop -= e.wheel.y;

        // Limita lo scroll per non andare fuori dai limiti del buffer
        _viewTop = Math.Clamp(_viewTop, 0, Math.Max(0, _height - _viewHeight));

        // Forza un ridisegno completo della finestra per mostrare la nuova vista
        _isContentDirty = true;
        _forceRedraw = true;
    }
    
    /// <summary>
    /// Gestisce l'evento di richiesta di interruzione dal servizio di tastiera.
    /// </summary>
    private void OnBreakRequested()
    {
        var args = new SdlConsoleCancelEventArgs(ConsoleSpecialKey.ControlC);
        CancelKeyPress?.Invoke(this, args);
    }

    /// <summary>
    /// Gestisce gli eventi di finestra ricevuti da SdlManager.
    /// </summary>
    /// <param name="e">L'evento SDL.</param>
    private void HandleWindowEvent(SDL.SDL_Event e)
    {
        if (_context is null || e.window.windowID != SDL.SDL_GetWindowID(_context.WindowHandle)) return;
        switch (e.window.windowEvent)
        {
            case SDL.SDL_WindowEventID.SDL_WINDOWEVENT_CLOSE: _quitRequested = true; break;
            case SDL.SDL_WindowEventID.SDL_WINDOWEVENT_RESIZED:
                _lastResizeTime = DateTime.Now;
                _pendingResize = true;
                break;
        }
    }

    /// <summary>
    /// Gestisce la richiesta di chiusura globale da SdlManager.
    /// </summary>
    /// <param name="e">L'evento SDL.</param>
    private void HandleQuitRequest(SDL.SDL_Event e) => _quitRequested = true;

    #endregion
    
}