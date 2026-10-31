using CommonLib.Screens;
using CommonLib.Consoles;
// FILE: Interpreter.Consoles.SdlConsole.IConsole.cs

using CommonGraphics.Terminal;
using SDL2;

namespace CommonGraphics.Consoles;

/// <summary>
/// Contiene la logica interna di rendering e gestione delle risorse grafiche
/// per la classe parziale SdlConsole.
/// </summary>
public sealed partial class SdlConsole
{
    /// <summary>
    /// Ottiene o imposta un valore che indica se la console deve eseguire un rendering automatico.
    /// </summary>
    public bool AutoFlush { get; set; } = true;

    /// <summary>
    /// Si verifica quando viene premuta una combinazione di tasti di annullamento (es. Ctrl+C).
    /// </summary>
    public event SdlConsoleCancelEventHandler? CancelKeyPress;

    /// <summary>
    /// Ottiene la configurazione corrente della console.
    /// </summary>
    public ScreenConfig Configuration { get; private set; }

    /// <summary>
    /// Ottiene il flusso di input standard.
    /// </summary>
    public TextReader In { get; private set; }

    /// <summary>
    /// Ottiene il flusso di output standard.
    /// </summary>
    public TextWriter Out { get; private set; }

    /// <summary>
    /// Ottiene il flusso di errore standard.
    /// </summary>
    public TextWriter Error { get; private set; }

    /// <summary>
    /// Ottiene o imposta il titolo della finestra della console.
    /// </summary>
    public string Title { get; set; }

    /// <summary>
    /// Ottiene o imposta il colore di sfondo corrente per il testo.
    /// </summary>
    public int BackgroundColor
    {
        get => ((ITerminalBuffer)this).CurrentBackgroundColor;
        set
        {
            ((ITerminalBuffer)this).CurrentBackgroundColor = value;
            _isContentDirty = true;
            _forceRedraw = true;
        }
    }

    /// <summary>
    /// Ottiene o imposta il colore di primo piano corrente per il testo.
    /// </summary>
    public int ForegroundColor
    {
        get => ((ITerminalBuffer)this).CurrentForegroundColor;
        set
        {
            ((ITerminalBuffer)this).CurrentForegroundColor = value;
            _isContentDirty = true;
        }
    }

    /// <summary>
    /// Ottiene la larghezza del buffer di schermo.
    /// </summary>
    public int BufferWidth
    {
        get => _width;
        set => throw new NotSupportedException("Buffer resize not supported.");
    }

    /// <summary>
    /// Ottiene l'altezza del buffer di schermo.
    /// </summary>
    public int BufferHeight
    {
        get => _height;
        set => throw new NotSupportedException("Buffer resize not supported.");
    }

    /// <summary>
    /// Ottiene o imposta la larghezza della finestra della console in caratteri.
    /// </summary>
    public int WindowWidth
    {
        get => _viewWidth;
        set => SetWindowSize(value, _viewHeight);
    }

    /// <summary>
    /// Ottiene o imposta l'altezza della finestra della console in caratteri.
    /// </summary>
    public int WindowHeight
    {
        get => _viewHeight;
        set => SetWindowSize(_viewWidth, value);
    }

    /// <summary>
    /// Ottiene o imposta la posizione orizzontale della vista sul buffer.
    /// </summary>
    public int WindowLeft
    {
        get => _viewLeft;
        set
        {
            if (!_isShown) return;
            _viewLeft = Math.Clamp(value, 0, _width - _viewWidth);
            _isContentDirty = true;
            _forceRedraw = true;
        }
    }

    /// <summary>
    /// Ottiene o imposta la posizione verticale della vista sul buffer.
    /// </summary>
    public int WindowTop
    {
        get => _viewTop;
        set
        {
            if (!_isShown) return;
            _viewTop = Math.Clamp(value, 0, _height - _viewHeight);
            _isContentDirty = true;
            _forceRedraw = true;
        }
    }

    /// <summary>
    /// Ottiene o imposta la colonna (posizione orizzontale) del cursore.
    /// </summary>
    public int CursorLeft
    {
        get => _cursorCol;
        set => SetCursorPosition(value, _cursorRow);
    }

    /// <summary>
    /// Ottiene o imposta la riga (posizione verticale) del cursore.
    /// </summary>
    public int CursorTop
    {
        get => _cursorRow;
        set => SetCursorPosition(_cursorCol, value);
    }

    /// <summary>
    /// Ottiene o imposta un valore che indica se il cursore è visibile.
    /// </summary>
    public bool CursorVisible { get; set; } = true;

    /// <summary>
    /// Ottiene un valore che indica se il contenuto della console è stato modificato dall'ultimo rendering.
    /// </summary>
    public bool IsDirty => _isContentDirty;

    /// <summary>
    /// Ottiene il percorso del file del font attualmente in uso.
    /// </summary>
    public string? ActiveFontPath => _activeFontPath;

    /// <summary>
    /// Riproduce un suono di avviso (beep).
    /// </summary>
    public void Beep() => Console.Beep();

    /// <summary>
    /// Pulisce il buffer della console e sposta il cursore in alto a sinistra.
    /// </summary>
    public void Clear() => ((ITerminalBuffer)this).ClearScreen();

    /// <summary>
    /// Ripristina i colori di primo piano e sfondo ai valori di default.
    /// </summary>
    public void ResetColor()
    {
        ((ITerminalBuffer)this).CurrentForegroundColor = Configuration.DefaultFgColor;
        ((ITerminalBuffer)this).CurrentBackgroundColor = Configuration.DefaultBgColor;
        ((ITerminalBuffer)this).CurrentAttributes = ScreenCellAttributes.None;
        _isContentDirty = true;
    }

    /// <summary>
    /// Imposta la posizione del cursore a una colonna e riga specificate.
    /// </summary>
    /// <param name="left">La nuova colonna del cursore.</param>
    /// <param name="top">La nuova riga del cursore.</param>
    public void SetCursorPosition(int left, int top)
    {
        lock (_renderLock)
        {
        _cursorCol = Math.Clamp(left, 0, _width - 1);
        _cursorRow = Math.Clamp(top, 0, _height - 1);
        _isContentDirty = true;
        }
        EnsureCursorIsVisible();
    }

    /// <summary>
    /// Imposta la posizione della finestra della console sullo schermo.
    /// </summary>
    /// <param name="left">La coordinata X della finestra.</param>
    /// <param name="top">La coordinata Y della finestra.</param>
    public void SetWindowPosition(int left, int top)
    {
        if (!_isShown || _context == null) return;
        SDL.SDL_SetWindowPosition(_context.WindowHandle, left, top);
    }

    /// <summary>
    /// Imposta la dimensione della finestra della console in numero di caratteri.
    /// </summary>
    /// <param name="width">La nuova larghezza in caratteri.</param>
    /// <param name="height">La nuova altezza in caratteri.</param>
    public void SetWindowSize(int width, int height)
    {
        if (width <= 0 || width > BufferWidth) throw new ArgumentOutOfRangeException(nameof(width));
        if (height <= 0 || height > BufferHeight) throw new ArgumentOutOfRangeException(nameof(height));
        _viewWidth = width;
        _viewHeight = height;
        if (_isShown && _context != null && _charWidth > 0 && _charHeight > 0)
        {
            SDL.SDL_SetWindowSize(_context.WindowHandle, _viewWidth * _charWidth, _viewHeight * _charHeight);
        }

        WindowLeft = _viewLeft;
        WindowTop = _viewTop;
        _isContentDirty = true;
        _forceRedraw = true;
    }

    /// <summary>
    /// Processa gli eventi di sistema in coda (mouse, finestra, etc.).
    /// </summary>
    /// <returns>True se è stata richiesta la chiusura, altrimenti false.</returns>
    public bool ProcessEvents()
    {
        SdlManager.Instance.PollEvents();
        return _quitRequested;
    }

    /// <summary>
    /// Rende visibile la finestra della console.
    /// </summary>
    public void Show()
    {
        if (_isShown || _context == null) return;
        SDL.SDL_ShowWindow(_context.WindowHandle);
        _isShown = true;
        _forceRedraw = true;
        Present();
    }

    /// <summary>
    /// Aggiorna e disegna il contenuto della finestra della console sullo schermo.
    /// </summary>
    public void Present()
    {
        RenderBuffer();
    }

    /// <summary>
    /// Imposta la visibilità della finestra della console.
    /// </summary>
    /// <param name="visible">True per mostrare, false per nascondere.</param>
    public void SetVisibility(bool visible)
    {
        if (!_isShown || _context is null) return;
        if (visible) SDL.SDL_ShowWindow(_context.WindowHandle);
        else SDL.SDL_HideWindow(_context.WindowHandle);
    }

    /// <summary>
    /// Imposta lo stile del cursore (es. blocco, barra).
    /// </summary>
    /// <param name="style">Lo stile del cursore da applicare.</param>
    public void SetCursorStyle(CursorStyle style)
    {
        ((ITerminalBuffer)this).CursorStyle = style;
        _isContentDirty = true;
    }

    /// <summary>
    /// Imposta un nuovo font per la console utilizzando una configurazione.
    /// </summary>
    /// <param name="config">L'oggetto di configurazione del font.</param>
    public void SetFont(ScreenConfig config)
    {
        ArgumentNullException.ThrowIfNull(config);
        Configuration = config;
        UpdateFontAndRecalibrate();
    }

    /// <summary>
    /// Imposta un nuovo font per la console specificandone i parametri.
    /// </summary>
    /// <param name="fontPath">Il percorso del file del font.</param>
    /// <param name="fontSize">La dimensione del font.</param>
    /// <param name="hinting">La modalità di hinting del font.</param>
    public void SetFont(string fontPath, int fontSize, SdlFontHinting? hinting = null)
    {
        Configuration = Configuration with
        {
            FontPath = fontPath, FontSize = fontSize, FontHinting = hinting ?? Configuration.FontHinting
        };
        UpdateFontAndRecalibrate();
    }

    /// <summary>
    /// Imposta un nuovo TextWriter come standard error.
    /// </summary>
    /// <param name="newError">Il nuovo TextWriter da usare.</param>
    public void SetError(TextWriter newError)
    {
        Error = newError ?? throw new ArgumentNullException(nameof(newError));
    }

    /// <summary>
    /// Imposta un nuovo TextReader come standard input.
    /// </summary>
    /// <param name="newIn">Il nuovo TextReader da usare.</param>
    public void SetIn(TextReader newIn)
    {
        In = newIn ?? throw new ArgumentNullException(nameof(newIn));
    }

    /// <summary>
    /// Imposta un nuovo TextWriter come standard output.
    /// </summary>
    /// <param name="newOut">Il nuovo TextWriter da usare.</param>
    public void SetOut(TextWriter newOut)
    {
        Out = newOut ?? throw new ArgumentNullException(nameof(newOut));
    }
    
    /// <summary>
    /// Modifica un colore nella palette di rendering della SdlConsole.
    /// </summary>
    public void SetScrollRegion(int top, int bottom)
    {
        if (top == 0 && bottom == 0)
        {
            _scrollTop = 0;
            _scrollBottom = -1; // full screen
        }
        else
        {
            _scrollTop = Math.Clamp(top - 1, 0, _height - 1);
            _scrollBottom = Math.Clamp(bottom - 1, _scrollTop, _height - 1);
        }
    }
    public void SetPaletteColor(int index, int r, int g, int b)
    {
        // Controlla che l'indice sia nei limiti della palette attuale.
        if (index < 0 || index >= _palette.Length)
            return; // Ignora indici non validi silenziosamente.

        // Valida i componenti del colore.
        r = Math.Clamp(r, 0, 255);
        g = Math.Clamp(g, 0, 255);
        b = Math.Clamp(b, 0, 255);

        // Aggiorna l'array della palette in memoria.
        _palette[index] = [(byte)r, (byte)g, (byte)b];

        // Forza un ridisegno completo della console al prossimo frame
        // per applicare il nuovo colore a tutte le celle che lo usano.
        _forceRedraw = true;
        _isContentDirty = true;
    }

    public void SetActivePalette(byte[][] palette, string? name = null)
    {
        _palette = palette;
        _activePaletteName = name;

        // Forza un ridisegno completo: tutte le celle che referenziano indici della
        // vecchia palette devono essere ridisegnate con i colori della nuova.
        _forceRedraw = true;
        _isContentDirty = true;
    }

    public byte[][] GetActivePalette() => _palette?.Select(c => new byte[] { c[0], c[1], c[2] }).ToArray() ?? [];

    public string? GetActivePaletteName() => _activePaletteName;
    
}