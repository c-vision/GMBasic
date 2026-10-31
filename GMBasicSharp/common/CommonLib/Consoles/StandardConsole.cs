using CommonLib.Screens;
using CommonLib.Clipboard;
using CommonLib.Keyboard;
using CommonLib.Consoles.Tui;
// FILE: Interpreter.Consoles.StandardConsole.cs

using System.Text;

namespace CommonLib.Consoles;

/// <summary>
/// Implementazione di <see cref="IConsole"/> che emula una console di testo
/// utilizzando <see cref="System.Console"/>, ma con un buffer di schermo interno
/// per supportare funzionalità avanzate come la copia del contenuto e il rendering ottimizzato.
/// </summary>
public sealed partial class StandardConsole : IConsole
{
    #region Campi Interni Centralizzati

    /// <summary>
    /// La configurazione iniziale con cui la console è stata creata (immutabile).
    /// </summary>
    private readonly ScreenConfig _configuration;
    
    /// <summary>
    /// Il servizio di clipboard iniettato, usato per le operazioni di copia/incolla.
    /// </summary>
    private readonly IClipboardService _clipboardService;

    // --- Stato del Buffer ---
    
    /// <summary>
    /// Buffer TUI condiviso (ScreenCell con RGB opzionali) per il rendering e i controlli ClipperLib.
    /// </summary>
    private ScreenBuffer _screen = null!;

    /// <summary>
    /// Il buffer principale unificato che contiene tutte le informazioni di ogni cella.
    /// </summary>
    private ScreenCell[,] _buffer = null!;

    /// <summary>
    /// Espone il buffer TUI per l'integrazione con EventLoop e controlli ClipperLib.
    /// </summary>
    public ScreenBuffer Screen => _screen;

    /// <summary>
    /// Uno snapshot del buffer dopo l'ultimo rendering, usato per determinare quali celle sono cambiate.
    /// </summary>
    private ScreenCell[,] _previousBuffer = null!;
    // Stdout REALE catturato alla costruzione: RenderBuffer scrive qui, così la console può
    // essere usata anche con Console.SetOut reindirizzato sul suo Out (buffer) — altrimenti
    // il rendering sarebbe circolare (Console.Write finirebbe nel buffer).
    private readonly TextWriter _renderOut = Console.Out;

    /// <summary>
    /// La larghezza del buffer in colonne, sincronizzata con la larghezza fisica
    /// della console al momento dell'inizializzazione.
    /// </summary>
    private int _width;
    
    /// <summary>
    /// L'altezza del buffer in righe, sincronizzata con l'altezza fisica
    /// della console al momento dell'inizializzazione.
    /// </summary>
    private int _height;
    private int _scrollTop, _scrollBottom = -1;

    /// <summary>
    /// Palette attiva corrente (mutabile via PALETTE USE/DEFINE/LOAD).
    /// </summary>
    private byte[][] _activePalette = null!;

    /// <summary>
    /// Nome della palette attiva (null se non proveniente da una palette nominativa).
    /// </summary>
    private string? _activePaletteName;

    /// <summary>
    /// Se true, bypassa completamente il buffer a griglia interno: le scritture vanno
    /// dirette a System.Console (come SystemConsole), sfruttando lo scrollback nativo del
    /// terminale invece del rendering ottimizzato a celle. Commutabile a runtime tramite
    /// CONSOLE.SCROLLMODE("NATIVE"|"BUFFERED"). Default: true (NATIVE) — la console deve
    /// scrollare fin dall'avvio, come un terminale normale; chi ha bisogno di VIEW PRINT o
    /// SAVE SCREEN/RESTORE SCREEN sulla console deve passare esplicitamente a BUFFERED
    /// (quei comandi generano un errore runtime se usati mentre si è in NATIVE).
    /// </summary>
    private bool _nativeScrollMode = true;

    /// <summary>Esposto per i comandi che richiedono il buffer a griglia (VIEW PRINT, SAVE/RESTORE SCREEN).</summary>
    public bool IsNativeScrollMode => _nativeScrollMode;

    #endregion

    // ConsoleColor RGB approximations for palette distance matching
    private static readonly (byte R, byte G, byte B)[] _consoleColorRgb =
    [
        (0,0,0), (0,0,128), (0,128,0), (0,128,128), (128,0,0), (128,0,128),
        (128,128,0), (192,192,192), (128,128,128), (0,0,255), (0,255,0),
        (0,255,255), (255,0,0), (255,0,255), (255,255,0), (255,255,255)
    ];

    private ConsoleColor MapToConsoleColor(int paletteIndex)
    {
        var palette = _activePalette;
        if (palette.Length == 0 || paletteIndex < 0 || paletteIndex >= palette.Length)
            return (ConsoleColor)(paletteIndex % 16);

        var target = palette[paletteIndex];
        int best = 0, bestDist = int.MaxValue;
        for (int i = 0; i < 16; i++)
        {
            int dr = target[0] - _consoleColorRgb[i].R;
            int dg = target[1] - _consoleColorRgb[i].G;
            int db = target[2] - _consoleColorRgb[i].B;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return (ConsoleColor)best;
    }

    /// <summary>
    /// Scrive una sequenza ANSI truecolor per il primo piano, attingendo dalla palette attiva.
    /// </summary>
    private void WriteAnsiFg(int paletteIndex)
    {
        var palette = _activePalette;
        if (paletteIndex >= 0 && paletteIndex < palette.Length)
        {
            var c = palette[paletteIndex];
            _renderOut.Write($"[38;2;{c[0]};{c[1]};{c[2]}m");
        }
    }

    /// <summary>
    /// Scrive una sequenza ANSI truecolor per lo sfondo, attingendo dalla palette attiva.
    /// </summary>
    private void WriteAnsiBg(int paletteIndex)
    {
        var palette = _activePalette;
        if (paletteIndex >= 0 && paletteIndex < palette.Length)
        {
            var c = palette[paletteIndex];
            _renderOut.Write($"[48;2;{c[0]};{c[1]};{c[2]}m");
        }
    }

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="StandardConsole"/>.
    /// Prepara i buffer interni, inizializza i servizi, imposta le dimensioni della finestra
    /// e registra gli hook di sistema.
    /// </summary>
    /// <param name="config">La configurazione da applicare per dimensioni e colori.</param>
    /// <param name="keyboardService">Il servizio di tastiera per registrare gli hook (es. Ctrl+C).</param>
    /// <param name="clipboardService">Il servizio di clipboard per le operazioni di copia/incolla.</param>
    public StandardConsole(ScreenConfig config, Keyboard.IKeyboardService keyboardService, IClipboardService clipboardService)
    {
        _configuration = config;
        _clipboardService = clipboardService;
        _activePalette = config.ResolvedPalette;
        _activePaletteName = config.PaletteName;

        TryApplyWindowSizeFromConfig();

        // Console.WindowWidth/WindowHeight restituiscono 0 su macOS/Linux ogni volta che
        // non e' collegato un vero terminale interattivo (output rediretto, sandbox, CI,
        // processo lanciato in background) — TryApplyWindowSizeFromConfig sopra applica un
        // fallback esplicito SOLO su Windows (SetWindowSize), quindi su altre piattaforme
        // _width/_height finivano a 0 senza alcuna correzione. Un buffer 0x0 fa si' che
        // WriteInternal calcoli CursorTop = _height - 1 = -1 al primissimo carattere
        // stampato (Consoles/StandardConsole.Buffer.cs), scrivendo su un indice negativo di
        // _buffer[,] e crashando con IndexOutOfRangeException — riproducibile con
        // QUALUNQUE programma in esecuzione reale con StandardConsole (default solo con -c/
        // -console, dato che UseSdl e' true di default) ogni volta che il processo non ha
        // un TTY reale collegato — es. `-c` passato in automazione/CI headless.
        var rawWidth = Console.WindowWidth;
        var rawHeight = Console.WindowHeight;
        _width = rawWidth > 0 ? rawWidth : (_configuration.CharGridWidth ?? 80);
        _height = rawHeight > 0 ? rawHeight : (_configuration.CharGridHeight ?? 25);

        //Debug.WriteLine($"[StandardConsole.Ctor] Initialized. Final logical dimensions set to: {_width}x{_height}");
        
        InitializeBuffers();
        
        var writer = new StandardConsoleWriter(this);
        In = Console.In;
        Out = writer;
        Error = writer;
        
        try
        {
            Console.CancelKeyPress += HandleConsoleCancelKeyPress;
            if (!Console.IsOutputRedirected) 
                Console.OutputEncoding = Encoding.UTF8;
        }
        catch (IOException) 
        { 
            // Ignora errori se la console non è interattiva e non può modificare l'encoding.
        }

        ResetColor();
        Clear();
        try { Console.CursorVisible = true; } catch (PlatformNotSupportedException) { }
        
        keyboardService.RegisterHook(new(ConsoleKey.C, ConsoleModifiers.Control | ConsoleModifiers.Shift), CopyBufferToClipboard);
        keyboardService.RegisterHook(new(ConsoleKey.V, ConsoleModifiers.Control | ConsoleModifiers.Shift), PasteFromClipboard);
    }
    
    #region Implementazione Interfaccia IConsole
    
    /// <summary>
    /// Si verifica quando viene premuta una combinazione di tasti di annullamento (es. Ctrl+C).
    /// </summary>
    public event SdlConsoleCancelEventHandler? CancelKeyPress;

    /// <summary>
    /// Ottiene la configurazione con cui la console è stata inizializzata.
    /// </summary>
    public ScreenConfig Configuration => _configuration;
    
    /// <summary>
    /// Ottiene il flusso di input standard. Può essere modificato tramite SetIn.
    /// </summary>
    public TextReader In { get; private set; }
    
    /// <summary>
    /// Ottiene il flusso di output standard. Può essere modificato tramite SetOut.
    /// </summary>
    public TextWriter Out { get; private set; }
    
    /// <summary>
    /// Ottiene il flusso di errore standard. Può essere modificato tramite SetError.
    /// </summary>
    public TextWriter Error { get; private set; }

    /// <summary>
    /// Imposta un nuovo TextReader come stream di input standard per questa istanza di console.
    /// </summary>
    /// <param name="newIn">Il nuovo TextReader da usare.</param>
    public void SetIn(TextReader newIn)
    {
        In = newIn ?? throw new ArgumentNullException(nameof(newIn));
    }

    /// <summary>
    /// Imposta un nuovo TextWriter come stream di output standard per questa istanza di console.
    /// </summary>
    /// <param name="newOut">Il nuovo TextWriter da usare.</param>
    public void SetOut(TextWriter newOut)
    {
        Out = newOut ?? throw new ArgumentNullException(nameof(newOut));
    }
    
    /// <summary>
    /// Imposta un nuovo TextWriter come stream di errore standard per questa istanza di console.
    /// </summary>
    /// <param name="newError">Il nuovo TextWriter da usare.</param>
    public void SetError(TextWriter newError)
    {
        Error = newError ?? throw new ArgumentNullException(nameof(newError));
    }

    /// <summary>
    /// Ottiene o imposta un valore che indica se la console deve eseguire
    /// un rendering automatico e immediato dopo ogni operazione di scrittura.
    /// Se impostato a false, è necessario chiamare Present() manualmente per visualizzare il buffer.
    /// Il valore predefinito è true, per garantire la reattività della REPL.
    /// </summary>
    public bool AutoFlush { get; set; } = true;
    
    /// <summary>
    /// Ottiene o imposta il titolo della finestra della console. Funziona solo su Windows.
    /// </summary>
    public string Title
    {
        get => OperatingSystem.IsWindows() ? Console.Title : "GMBASIC AI POWERED";
        set { if (OperatingSystem.IsWindows()) Console.Title = value; }
    }

    /// <summary>
    /// Ottiene o imposta il colore di sfondo corrente per le successive operazioni di scrittura.
    /// L'operazione modifica lo stato interno, non la console fisica.
    /// </summary>
    public int BackgroundColor { get; set; }

    /// <summary>
    /// Ottiene o imposta il colore di primo piano (testo) corrente per le successive operazioni di scrittura.
    /// L'operazione modifica lo stato interno, non la console fisica.
    /// </summary>
    public int ForegroundColor { get; set; }

    /// <summary>
    /// Ottiene la larghezza del buffer di schermo. L'impostazione non è supportata dopo l'inizializzazione.
    /// </summary>
    public int BufferWidth
    {
        get => _width;
        set => throw new NotSupportedException("Cannot resize a buffered StandardConsole at runtime.");
    }
    
    /// <summary>
    /// Ottiene l'altezza del buffer di schermo. L'impostazione non è supportata dopo l'inizializzazione.
    /// </summary>
    public int BufferHeight
    {
        get => _height;
        set => throw new NotSupportedException("Cannot resize a buffered StandardConsole at runtime.");
    }
    
    /// <summary>
    /// Ottiene la larghezza della finestra della console. Riflette la dimensione reale rilevata all'avvio.
    /// </summary>
    public int WindowWidth
    {
        get => _width;
        set => throw new NotSupportedException("Cannot resize a buffered StandardConsole at runtime.");
    }
    
    /// <summary>
    /// Ottiene l'altezza della finestra della console. Riflette la dimensione reale rilevata all'avvio.
    /// </summary>
    public int WindowHeight
    {
        get => _height;
        set => throw new NotSupportedException("Cannot resize a buffered StandardConsole at runtime.");
    }
    
    /// <summary>
    /// Impostare la posizione della finestra non è supportato in modo affidabile da System.Console.
    /// </summary>
    public int WindowLeft { get; set; }
    
    /// <summary>
    /// Impostare la posizione della finestra non è supportato in modo affidabile da System.Console.
    /// </summary>
    public int WindowTop { get; set; }
    
    /// <summary>
    /// Ottiene o imposta la colonna (posizione X) del cursore logico nel buffer.
    /// </summary>
    public int CursorLeft { get; set; }

    /// <summary>
    /// Ottiene o imposta la riga (posizione Y) del cursore logico nel buffer.
    /// </summary>
    public int CursorTop { get; set; }

    /// <summary>
    /// Ottiene o imposta un valore che indica se il cursore è visibile.
    /// </summary>
    public bool CursorVisible { get; set; } = true;
            
    /// <summary>
    /// Ottiene un valore che indica se il buffer è stato modificato dall'ultima operazione di rendering.
    /// </summary>
    public bool IsDirty { get; private set; } = true;

    /// <summary>
    /// Riproduce un suono di avviso (beep) tramite la console.
    /// </summary>
    public void Beep() => Console.Beep();
    
    /// <summary>
    /// Pulisce il buffer di schermo interno, riempiendolo con spazi e il colore di sfondo corrente.
    /// Imposta il flag 'dirty' per forzare un ridisegno completo al prossimo Present().
    /// </summary>
    public void Clear()
    {
        if (_nativeScrollMode)
        {
            // Console.Clear() riempie l'area cancellata con qualunque colore di sfondo SGR sia
            // attivo sul terminale in quel momento: senza impostarlo prima esplicitamente resta
            // quello che il terminale aveva di default all'avvio (es. nero), non il colore di
            // sfondo della palette attiva — a differenza di WriteInternalNative, che applica
            // WriteAnsiBg/Fg ad ogni carattere scritto e quindi "sembra" corretto solo dove è
            // già stato scritto testo.
            WriteAnsiFg(ForegroundColor);
            WriteAnsiBg(BackgroundColor);
            Console.Clear();
            CursorLeft = 0;
            CursorTop = 0;
            return;
        }

        IsDirty = true;
        var emptyCell = new ScreenCell(' ', ForegroundColor, BackgroundColor);

        for (var r = 0; r < _height; r++)
        for (var c = 0; c < _width; c++)
        {
            _buffer[r, c] = emptyCell;
        }
        CursorLeft = 0;
        CursorTop = 0;

        // In modalità a buffer fisso non c'è mai nulla di utile "sopra" lo schermo visibile
        // (nessuno scroll lineare, si riscrive sempre la stessa griglia): senza questo, lo
        // scrollback del terminale reale può comunque contenere righe residue (es. da una
        // precedente sessione CONSOLE.SCROLLMODE("NATIVE"), o dall'uso della shell prima di
        // avviare l'interprete), lasciando visibile e attiva la scrollbar nativa — l'utente
        // può scrollare e vedersi uno sfondo vuoto/obsoleto che non riflette lo stato attuale.
        ClearNativeScrollback();
    }

    /// <summary>
    /// Commuta StandardConsole tra la modalità a buffer fisso (default, con supporto
    /// VIEW PRINT e rendering ottimizzato a celle) e la modalità "nativa": in modalità
    /// nativa le scritture vanno dirette a System.Console, sfruttando lo scrollback reale
    /// del terminale — a costo di perdere il rendering a celle e le scroll region esplicite,
    /// che non hanno senso senza un buffer delimitato.
    /// </summary>
    /// <param name="native">true per la modalità nativa, false per tornare al buffer.</param>
    public void SetScrollMode(bool native)
    {
        if (_nativeScrollMode == native) return;
        _nativeScrollMode = native;
        if (native)
        {
            try { Console.SetCursorPosition(Math.Clamp(CursorLeft, 0, _width - 1), Math.Clamp(CursorTop, 0, _height - 1)); }
            catch (Exception) { /* Ignora se il terminale non supporta il posizionamento. */ }
        }
        else
        {
            // Si torna da NATIVE (dove lo scrollback è cresciuto legittimamente) a BUFFERED:
            // quello storico non ha più senso per un buffer fisso, va ripulito subito.
            ClearNativeScrollback();
        }
    }

    /// <summary>
    /// Invia la sequenza ANSI/xterm per svuotare lo scrollback del terminale reale (a
    /// differenza di Console.Clear(), che pulisce solo lo schermo visibile lasciando intatto
    /// lo storico sopra di esso). Supportata da tutti i terminali moderni (Terminal.app,
    /// iTerm2, la maggior parte dei terminali Linux); no-op silenzioso altrove.
    /// </summary>
    private static void ClearNativeScrollback()
    {
        if (Console.IsOutputRedirected) return;
        try { Console.Out.Write("\x1b[3J"); }
        catch (IOException) { /* Ignora se lo stream non è più scrivibile. */ }
    }

    /// <summary>
    /// Ripristina i colori di primo piano e sfondo ai valori di default specificati nella configurazione iniziale.
    /// </summary>
    public void ResetColor()
    {
        BackgroundColor = Configuration.DefaultBgColor;
        ForegroundColor = Configuration.DefaultFgColor;
    }

    /// <summary>
    /// Imposta la posizione del cursore logico, bloccandola entro i limiti del buffer.
    /// </summary>
    /// <param name="left">La nuova colonna (posizione orizzontale) del cursore.</param>
    /// <param name="top">La nuova riga (posizione verticale) del cursore.</param>
    public void SetCursorPosition(int left, int top)
    {
        if (_nativeScrollMode)
        {
            try { Console.SetCursorPosition(left, top); } catch (Exception) { /* Ignora se fuori dai limiti reali. */ }
            CursorLeft = left;
            CursorTop = top;
            return;
        }

        CursorLeft = Math.Clamp(left, 0, _width - 1);
        CursorTop = Math.Clamp(top, 0, _height - 1);
    }
    
    /// <summary>
    /// Imposta la posizione della finestra (non supportato in modo affidabile).
    /// </summary>
    /// <param name="left">Coordinata X.</param>
    /// <param name="top">Coordinata Y.</param>
    public void SetWindowPosition(int left, int top) { }
    
    /// <summary>
    /// Tenta di impostare le dimensioni della finestra della console e del buffer logico.
    /// Questa operazione è supportata principalmente su Windows. Se l'operazione ha
    /// successo, i buffer interni vengono ri-allocati e lo schermo viene pulito.
    /// </summary>
    /// <param name="width">La larghezza desiderata in colonne.</param>
    /// <param name="height">L'altezza desiderata in righe.</param>
    public void SetWindowSize(int width, int height)
    {
        // Il ridimensionamento programmatico è affidabile solo su Windows.
        if (!OperatingSystem.IsWindows())
        {
            //Debug.WriteLine("[StandardConsole.SetWindowSize] Skipped: Not running on Windows.");
            return;
        }

        // Ignora se le dimensioni richieste sono identiche a quelle attuali.
        if (width == _width && height == _height) return;

        try
        {
            //Debug.WriteLine($"[StandardConsole.SetWindowSize] Attempting to resize to {width}x{height}.");
            
            // Applica prima la modifica alla console fisica.
            // Potrebbe essere necessario aggiustare prima il buffer di sistema.
            if (Console.BufferWidth < width || Console.BufferHeight < height)
            {
                Console.SetBufferSize(Math.Max(Console.BufferWidth, width), Math.Max(Console.BufferHeight, height));
            }
            Console.SetWindowSize(width, height);

            // Se l'operazione fisica ha successo, ricalibra lo stato interno.
            Recalibrate(width, height);
        }
        catch (Exception)
        {
            // Se l'API di sistema fallisce, registra l'errore e non fa nulla.
            // Lo stato interno rimane coerente con le dimensioni fisiche precedenti.
            //Debug.WriteLine($"[StandardConsole.SetWindowSize] Failed to apply physical window size: {ex.Message}");
        }
    }

    /// <summary>
    /// Per le console di sistema, non esegue alcuna elaborazione di eventi.
    /// </summary>
    /// <returns>Restituisce sempre false.</returns>
    public bool ProcessEvents() => false;
    
    /// <summary>
    /// La console di sistema è sempre visibile.
    /// </summary>
    public void Show() { }
    
    /// <summary>
    /// Disegna il contenuto del buffer interno sulla console fisica.
    /// La logica dettagliata di rendering si trova nel file parziale.
    /// </summary>
    public void Present()
    {
        RenderBuffer();
    }
    
    /// <summary>
    /// La visibilità della console di sistema non può essere controllata.
    /// </summary>
    /// <param name="visible">Parametro ignorato.</param>
    public void SetVisibility(bool visible) { }
    
    /// <summary>
    /// Rilascia le risorse, in particolare deregistra l'handler dell'evento CancelKeyPress.
    /// </summary>
    public void Dispose()
    {
        Console.CancelKeyPress -= HandleConsoleCancelKeyPress;
    }
            
    /// <summary>
    /// L'impostazione dello stile del cursore non è implementata in questa versione
    /// per mantenere la compatibilità cross-platform con System.Console.
    /// </summary>
    /// <param name="style">Parametro ignorato.</param>
    public void SetCursorStyle(CursorStyle style) { }
    
    /// <summary>
    /// Il font della console di sistema non può essere modificato dal programma.
    /// </summary>
    /// <param name="config">Parametro ignorato.</param>
    public void SetFont(ScreenConfig config) { }

    /// <summary>
    /// Il font della console di sistema non può essere modificato dal programma.
    /// </summary>
    /// <param name="fontPath">Parametro ignorato.</param>
    /// <param name="fontSize">Parametro ignorato.</param>
    /// <param name="hinting">Parametro ignorato.</param>
    public void SetFont(string fontPath, int fontSize, SdlFontHinting? hinting = null) { }

    /// <summary>
    /// La modifica della palette non è supportata dalla console di sistema.
    /// Questo metodo non ha alcun effetto.
    /// </summary>
    public void SetScrollRegion(int top, int bottom)
    {
        if (top == 0 && bottom == 0)
        {
            _scrollTop = 0;
            _scrollBottom = -1;
        }
        else
        {
            _scrollTop = Math.Clamp(top - 1, 0, _height - 1);
            _scrollBottom = Math.Clamp(bottom - 1, _scrollTop, _height - 1);
        }
    }
    public void SetPaletteColor(int index, int r, int g, int b)
    {
        if (index >= 0 && index < _activePalette.Length)
            _activePalette[index] = [(byte)Math.Clamp(r, 0, 255), (byte)Math.Clamp(g, 0, 255), (byte)Math.Clamp(b, 0, 255)];
    }

    public void SetActivePalette(byte[][] palette, string? name = null)
    {
        _activePalette = palette;
        _activePaletteName = name;
        IsDirty = true;
    }

    public byte[][] GetActivePalette() => _activePalette;

    public string? GetActivePaletteName() => _activePaletteName;
    
    #endregion
    
    #region Handler e Metodi Privati
    
    /// <summary>
    /// Gestisce l'evento di sistema per Ctrl+C e lo traduce nell'evento CancelKeyPress dell'interfaccia.
    /// </summary>
    /// <param name="sender">Oggetto che ha originato l'evento.</param>
    /// <param name="e">Argomenti dell'evento.</param>
    private void HandleConsoleCancelKeyPress(object? sender, ConsoleCancelEventArgs e)
    {
        var args = new SdlConsoleCancelEventArgs(e.SpecialKey);
        CancelKeyPress?.Invoke(this, args);
        e.Cancel = args.Cancel;
    }

    /// <summary>
    /// Tenta di impostare le dimensioni della finestra della console fisica
    /// affinché corrispondano a quelle specificate nella configurazione.
    /// </summary>
    private void TryApplyWindowSizeFromConfig()
    {
        if (!OperatingSystem.IsWindows()) return;
        try
        {
            var w = _configuration.CharGridWidth ?? 80;
            var h = _configuration.CharGridHeight ?? 25;
            
            if (Console.BufferWidth < w || Console.BufferHeight < h)
                Console.SetBufferSize(Math.Max(Console.BufferWidth, w), Math.Max(Console.BufferHeight, h));

            Console.SetWindowSize(w, h);
        }
        catch (Exception)
        {
            //Debug.WriteLine($"[StandardConsole.Config] Failed to apply window size: {ex.Message}");
        }
    }

    /// <summary>
    /// Ricalibra il buffer logico in modo non distruttivo, imitando il comportamento
    /// di un terminale dove il contenuto aderisce al fondo della nuova area.
    /// La responsabilità di forzare il ridisegno è demandata al chiamante.
    /// </summary>
    /// <param name="newWidth">La nuova larghezza in colonne.</param>
    /// <param name="newHeight">La nuova altezza in righe.</param>
    private void Recalibrate(int newWidth, int newHeight)
    {
        //Debug.WriteLine($"[StandardConsole.Recalibrate] Recalibrating LOGICAL buffer from {_width}x{_height} to {newWidth}x{newHeight}.");

        var newBuffer = new ScreenCell[newHeight, newWidth];
        var emptyCell = new ScreenCell(' ', ForegroundColor, BackgroundColor);

        // 1. Riempie l'intero nuovo buffer con celle vuote del colore corretto.
        for(var r = 0; r < newHeight; r++)
            for(var c = 0; c < newWidth; c++)
                newBuffer[r,c] = emptyCell;

        // 2. Calcola le coordinate per la copia, facendo aderire il contenuto al fondo.
        var rowsToCopy = Math.Min(_height, newHeight);
        var colsToCopy = Math.Min(_width, newWidth);
        
        var sourceRowStart = _height - rowsToCopy;
        var destRowStart = newHeight - rowsToCopy;

        // 3. Copia la porzione visibile del vecchio buffer nella posizione corretta del nuovo buffer.
        for (var i = 0; i < rowsToCopy; i++)
        {
            for (var c = 0; c < colsToCopy; c++)
            {
                newBuffer[destRowStart + i, c] = _buffer[sourceRowStart + i, c];
            }
        }
        
        // 4. Sostituisce il vecchio buffer principale con quello nuovo.
        _buffer = newBuffer;
        _screen = new ScreenBuffer(newWidth, newHeight);
        
        // 5. Aggiorna le dimensioni memorizzate.
        _width = newWidth;
        _height = newHeight;
        
        // 6. Aggiorna la posizione del cursore in modo relativo.
        CursorTop = Math.Max(0, CursorTop - sourceRowStart + destRowStart);
        CursorLeft = Math.Min(CursorLeft, _width - 1);

        // 7. Marca lo stato come sporco per indicare che è necessario un rendering.
        IsDirty = true;
    }
    
    /// <summary>
    /// Esegue un controllo attivo (polling) per rilevare se le dimensioni fisiche
    /// della console sono cambiate. Se viene rilevato un cambiamento,
    /// innesca l'intera sequenza di ricalibrazione e ridisegno.
    /// Questo metodo è pensato per essere chiamato in un loop continuo
    /// dal gestore dell'applicazione (es. Executor) per rendere il ridimensionamento proattivo.
    /// </summary>
    public void PollForResize()
    {
        // Controlla le dimensioni fisiche attuali. Se il terminale reale non e' (piu')
        // disponibile (0x0, es. output rediretto o processo senza TTY), ignora il poll
        // invece di ricalibrare verso un buffer 0x0 — vedi il commento nel costruttore
        // per il crash che ne conseguirebbe al primo carattere scritto.
        var physicalWidth = Console.WindowWidth;
        var physicalHeight = Console.WindowHeight;
        if (physicalWidth <= 0 || physicalHeight <= 0) return;

        // Se sono diverse da quelle che abbiamo memorizzato, significa che è avvenuto un resize.
        if (physicalWidth != _width || physicalHeight != _height)
        {
            // Chiama Present() per innescare la logica di ricalibrazione
            // già presente in RenderBuffer().
            Present();
        }
    }
    
    #endregion
}