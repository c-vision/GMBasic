using CommonLib.Consoles;
using CommonLib.Clipboard;
// FILE: Interpreter.Consoles.SystemConsole.cs
using CommonLib.Screens;
// Questo file è basato sul vecchio StandardConsole, rinominato e adattato alla nuova IConsole.
// Non utilizza alcun buffer interno e interagisce direttamente con System.Console.

using System.Text;

// ReSharper disable CanSimplifyStringEscapeSequence

namespace CommonLib.Consoles;

/// <summary>
/// Implementazione di <see cref="IConsole"/> che funge da wrapper diretto
/// per <see cref="System.Console"/>, garantendo la piena compatibilità API e
/// gestendo correttamente la portabilità tra piattaforme. Questa implementazione
/// non utilizza un buffer interno; ogni operazione di output è immediata.
/// </summary>
public sealed class SystemConsole : IConsole
{
    /// <summary>
    /// La configurazione con cui la console è stata inizializzata.
    /// </summary>
    private readonly ScreenConfig _configuration;

    /// <summary>
    /// Inizializza una nuova istanza di SystemConsole con una configurazione specifica.
    /// </summary>
    /// <param name="config">La configurazione da applicare. Se null, verrà usata una configurazione di default.</param>
    public SystemConsole(ScreenConfig? config = null)
    {
        // Se non viene fornita una configurazione, ne crea una di default per la console standard.
        _configuration = config ?? new ScreenConfig
        {
             CharGridWidth = 80,
             CharGridHeight = 25,
             DefaultFgColor = (int)ConsoleColor.Gray,
             DefaultBgColor = (int)ConsoleColor.Black
        };
        _activePalette = _configuration.ResolvedPalette;
        _activePaletteName = _configuration.PaletteName;

        try
        {
            Console.CancelKeyPress += HandleConsoleCancelKeyPress;
            if (!Console.IsOutputRedirected)
                Console.OutputEncoding = Encoding.UTF8;
        }
        catch (IOException) { /* Ignora se non è possibile impostare l'encoding */ }
    }
    
    // --- Eventi ---
    
    /// <summary>
    /// Delega interno per l'evento CancelKeyPress.
    /// </summary>
    private SdlConsoleCancelEventHandler? _cancelKeyPress;
    
    /// <summary>
    /// Gestisce l'evento CancelKeyPress di System.Console e lo inoltra all'evento personalizzato.
    /// </summary>
    /// <param name="sender">L'oggetto che ha generato l'evento.</param>
    /// <param name="e">I dati dell'evento.</param>
    private void HandleConsoleCancelKeyPress(object? sender, ConsoleCancelEventArgs e)
    {
        var args = new SdlConsoleCancelEventArgs(e.SpecialKey);
        _cancelKeyPress?.Invoke(this, args);
        // Non sovrascrivere e.Cancel se e' gia' true (KeyboardServiceConsole
        // lo setta a true per evitare la chiusura del processo)
        if (!e.Cancel)
            e.Cancel = args.Cancel;
    }

    // --- Metodi di Input (non parte di IConsole ma specifici dell'implementazione) ---
    
    /// <summary>
    /// Ottiene un valore che indica se è disponibile un tasto nella coda di input.
    /// </summary>
    public bool KeyAvailable => Console.KeyAvailable;
    
    /// <summary>
    /// Ottiene il prossimo carattere o tasto funzione premuto dall'utente.
    /// </summary>
    /// <param name="intercept">Se true, il tasto non viene visualizzato; se false, viene visualizzato.</param>
    /// <returns>Un oggetto TerminalKeyInfo che descrive il tasto premuto.</returns>
    public TerminalKeyInfo ReadKey(bool intercept)
    {
        var keyInfo = Console.ReadKey(intercept);
        return new TerminalKeyInfo(keyInfo.KeyChar, (int)keyInfo.Key, 
            keyInfo.Modifiers.HasFlag(ConsoleModifiers.Shift), 
            keyInfo.Modifiers.HasFlag(ConsoleModifiers.Alt), 
            keyInfo.Modifiers.HasFlag(ConsoleModifiers.Control));
    }
    
    /// <summary>
    /// Legge il prossimo carattere dallo stream di input.
    /// </summary>
    /// <returns>Il codice del carattere letto, o -1 se non ci sono più caratteri da leggere.</returns>
    public int Read() => Console.Read();
    
    /// <summary>
    /// Legge la prossima riga di caratteri dallo standard input.
    /// </summary>
    /// <returns>La riga letta, o null se lo stream di input è terminato.</returns>
    public string? ReadLine() => Console.ReadLine();

    #region Implementazione IConsole
    
    /// <summary>
    /// Si verifica quando viene premuta la combinazione di tasti Ctrl+C.
    /// Utilizza il delegato standard di .NET per la massima compatibilità.
    /// </summary>
    public event SdlConsoleCancelEventHandler? CancelKeyPress
    {
        add => _cancelKeyPress += value;
        remove => _cancelKeyPress -= value;
    }

    /// <summary>
    /// Ottiene o imposta un valore che indica se la console deve eseguire
    /// un rendering automatico e immediato dopo ogni operazione di scrittura.
    /// Poiché System.Console è tipicamente non bufferizzata, questa proprietà ha un effetto limitato.
    /// </summary>
    public bool AutoFlush { get; set; } = true;

    /// <summary>
    /// Ottiene la configurazione con cui la console è stata inizializzata.
    /// Questo oggetto contiene tutti i parametri di avvio come dimensioni,
    /// colori di default e impostazioni del font.
    /// </summary>
    public ScreenConfig Configuration => _configuration;
    
    /// <summary>
    /// Ottiene il flusso di input standard, reindirizzando a System.Console.In.
    /// </summary>
    public TextReader In => Console.In;
    
    /// <summary>
    /// Ottiene il flusso di output standard, reindirizzando a System.Console.Out.
    /// </summary>
    public TextWriter Out => Console.Out;
    
    /// <summary>
    /// Ottiene il flusso di errore standard, reindirizzando a System.Console.Error.
    /// </summary>
    public TextWriter Error => Console.Error;

    /// <summary>
    /// Imposta un nuovo TextReader come standard input, chiamando Console.SetIn.
    /// </summary>
    /// <param name="newIn">Il nuovo TextReader da usare.</param>
    public void SetIn(TextReader newIn) => Console.SetIn(newIn);
    
    /// <summary>
    /// Imposta un nuovo TextWriter come standard output, chiamando Console.SetOut.
    /// </summary>
    /// <param name="newOut">Il nuovo TextWriter da usare.</param>
    public void SetOut(TextWriter newOut) => Console.SetOut(newOut);
    
    /// <summary>
    /// Imposta un nuovo TextWriter come standard error, chiamando Console.SetError.
    /// </summary>
    /// <param name="newError">Il nuovo TextWriter da usare.</param>
    public void SetError(TextWriter newError) => Console.SetError(newError);
            
    /// <summary>
    /// Ottiene o imposta il titolo della finestra della console. Funziona solo su Windows.
    /// </summary>
    public string Title
    {
        get => OperatingSystem.IsWindows() ? Console.Title : "GMBASIC AI POWERED";
        set 
        {
            if (!OperatingSystem.IsWindows()) return;
            try { Console.Title = value; } catch (IOException) { /* Ignora errori in ambienti non interattivi */ }
        }
    }

    /// <summary>
    /// Palette RGB attiva (indice → colore), usata per emettere sequenze ANSI truecolor a 24
    /// bit invece di affidarsi ai 16 <see cref="ConsoleColor"/> nativi.
    /// </summary>
    private byte[][] _activePalette = [];

    private int _currentBg;
    private int _currentFg;

    /// <summary>
    /// Scrive la sequenza ANSI truecolor per il colore di sfondo all'indice di palette dato.
    /// </summary>
    private void WriteAnsiBg(int paletteIndex)
    {
        if (paletteIndex < 0 || paletteIndex >= _activePalette.Length) return;
        var c = _activePalette[paletteIndex];
        Console.Write($"\x1b[48;2;{c[0]};{c[1]};{c[2]}m");
    }

    /// <summary>
    /// Scrive la sequenza ANSI truecolor per il colore di primo piano all'indice di palette dato.
    /// </summary>
    private void WriteAnsiFg(int paletteIndex)
    {
        if (paletteIndex < 0 || paletteIndex >= _activePalette.Length) return;
        var c = _activePalette[paletteIndex];
        Console.Write($"\x1b[38;2;{c[0]};{c[1]};{c[2]}m");
    }

    /// <summary>
    /// Ottiene o imposta il colore di sfondo della console (indice nella palette attiva).
    /// Scrive direttamente una sequenza ANSI truecolor a 24 bit — a differenza della vecchia
    /// implementazione, che ignorava la palette impostando <see cref="Console.BackgroundColor"/>
    /// (i 16 <see cref="ConsoleColor"/> nativi, il cui RGB reale dipende dal tema del terminale).
    /// </summary>
    public int BackgroundColor
    {
        get => _currentBg;
        set { _currentBg = value; WriteAnsiBg(value); }
    }

    /// <summary>
    /// Ottiene o imposta il colore del testo della console (indice nella palette attiva).
    /// Stessa logica truecolor di <see cref="BackgroundColor"/>.
    /// </summary>
    public int ForegroundColor
    {
        get => _currentFg;
        set { _currentFg = value; WriteAnsiFg(value); }
    }

    /// <summary>
    /// Ottiene o imposta la larghezza del buffer della console. Supportato solo su Windows.
    /// </summary>
    public int BufferWidth
    {
        get => OperatingSystem.IsWindows() ? Console.BufferWidth : Configuration.CharGridWidth ?? Console.WindowWidth;
        set
        {
            if (!OperatingSystem.IsWindows()) return;
            try { Console.BufferWidth = value; } catch (IOException) { /* Ignora errori */ }
        }
    }

    /// <summary>
    /// Ottiene o imposta l'altezza del buffer della console. Supportato solo su Windows.
    /// </summary>
    public int BufferHeight
    {
        get => OperatingSystem.IsWindows() ? Console.BufferHeight : Configuration.CharGridHeight ?? Console.WindowHeight;
        set
        {
            if (!OperatingSystem.IsWindows()) return;
            try { Console.BufferHeight = value; } catch (IOException) { /* Ignora errori */ }
        }
    }

    /// <summary>
    /// Ottiene o imposta la larghezza della finestra della console in colonne. Supportato solo su Windows.
    /// </summary>
    public int WindowWidth
    {
        get => Console.WindowWidth;
        set { if (OperatingSystem.IsWindows()) try { Console.WindowWidth = value; } catch (IOException) { /* Ignora errori */ } }
    }

    /// <summary>
    /// Ottiene o imposta l'altezza della finestra della console in righe. Supportato solo su Windows.
    /// </summary>
    public int WindowHeight
    {
        get => Console.WindowHeight;
        set { if (OperatingSystem.IsWindows()) try { Console.WindowHeight = value; } catch (IOException) { /* Ignora errori */ } }
    }

    /// <summary>
    /// Ottiene o imposta la coordinata X della finestra della console. Supportato solo su Windows.
    /// </summary>
    public int WindowLeft
    {
        get => OperatingSystem.IsWindows() ? Console.WindowLeft : 0;
        set { if (OperatingSystem.IsWindows()) try { Console.WindowLeft = value; } catch (IOException) { /* Ignora errori */ } }
    }

    /// <summary>
    /// Ottiene o imposta la coordinata Y della finestra della console. Supportato solo su Windows.
    /// </summary>
    public int WindowTop
    {
        get => OperatingSystem.IsWindows() ? Console.WindowTop : 0;
        set { if (OperatingSystem.IsWindows()) try { Console.WindowTop = value; } catch (IOException) { /* Ignora errori */ } }
    }

    /// <summary>
    /// Ottiene o imposta la colonna del cursore, reindirizzando a System.Console.CursorLeft.
    /// </summary>
    public int CursorLeft
    {
        get => Console.CursorLeft;
        set => Console.CursorLeft = value;
    }

    /// <summary>
    /// Ottiene o imposta la riga del cursore, reindirizzando a System.Console.CursorTop.
    /// </summary>
    public int CursorTop
    {
        get => Console.CursorTop;
        set => Console.CursorTop = value;
    }

    /// <summary>
    /// Ottiene o imposta la visibilità del cursore. Su Windows, usa la proprietà nativa.
    /// Su altre piattaforme, invia sequenze di escape ANSI.
    /// </summary>
    public bool CursorVisible
    {
        get => !OperatingSystem.IsWindows() || Console.CursorVisible;
        set 
        {
            try
            {
                if (OperatingSystem.IsWindows()) Console.CursorVisible = value;
                else Console.Write(value ? "\x1B[?25h" : "\x1B[?25l");
            }
            catch (IOException) { /* Ignora errori */ }
        }
    }
    
    /// <summary>
    /// Riproduce un suono di avviso tramite la console di sistema.
    /// </summary>
    public void Beep() => Console.Beep();
    
    /// <summary>
    /// Pulisce lo schermo e il buffer della console, spostando il cursore in alto a sinistra.
    /// </summary>
    public void Clear() => Console.Clear();
    
    /// <summary>
    /// Ripristina i colori di primo piano e sfondo a quelli definiti nella configurazione iniziale.
    /// </summary>
    public void ResetColor()
    {
        BackgroundColor = Configuration.DefaultBgColor;
        ForegroundColor = Configuration.DefaultFgColor;
    }

    /// <summary>
    /// Imposta la posizione del cursore a una colonna e riga specificate.
    /// </summary>
    /// <param name="left">La nuova colonna del cursore (0-based).</param>
    /// <param name="top">La nuova riga del cursore (0-based).</param>
    public void SetCursorPosition(int left, int top) => Console.SetCursorPosition(left, top);
    
    /// <summary>
    /// Imposta la posizione della finestra della console. Supportato solo su Windows.
    /// </summary>
    /// <param name="left">La nuova coordinata X della finestra.</param>
    /// <param name="top">La nuova coordinata Y della finestra.</param>
    public void SetWindowPosition(int left, int top) 
    {
        if (!OperatingSystem.IsWindows()) return;
        try { Console.SetWindowPosition(left, top); } catch (IOException) { /* Ignora errori */ }
    }
    
    /// <summary>
    /// Imposta le dimensioni della finestra della console. Supportato solo su Windows.
    /// </summary>
    /// <param name="width">La nuova larghezza in colonne.</param>
    /// <param name="height">La nuova altezza in righe.</param>
    public void SetWindowSize(int width, int height)
    {
        if (!OperatingSystem.IsWindows()) return;
        try { Console.SetWindowSize(width, height); } catch (IOException) { /* Ignora errori */ }
    }
    
    /// <summary>
    /// Per le console di sistema, non esegue alcuna elaborazione di eventi.
    /// </summary>
    /// <returns>Restituisce sempre false, indicando che non è stata richiesta la chiusura.</returns>
    public bool ProcessEvents() => false;
    
    /// <summary>
    /// La console di sistema è sempre visibile; questo metodo non ha alcun effetto.
    /// </summary>
    public void Show() { }
    
    /// <summary>
    /// Poiché System.Console non è bufferizzata, questo metodo non ha alcun effetto.
    /// L'output è già stato presentato.
    /// </summary>
    public void Present() { }

    /// <summary>
    /// Ottiene un valore che indica se il contenuto della console è stato
    /// modificato. Per una console non bufferizzata, restituisce sempre true
    /// poiché non c'è modo di tracciare lo stato precedente.
    /// </summary>
    public bool IsDirty => true;
    
    /// <summary>
    /// La visibilità della console di sistema non può essere controllata; questo metodo non ha alcun effetto.
    /// </summary>
    /// <param name="visible">Questo parametro viene ignorato.</param>
    public void SetVisibility(bool visible) { }
    
    /// <summary>
    /// Rilascia le risorse gestite, in particolare rimuovendo l'handler dell'evento CancelKeyPress.
    /// </summary>
    public void Dispose()
    {
        Console.CancelKeyPress -= HandleConsoleCancelKeyPress;
    }
            
    /// <summary>
    /// Imposta lo stile del cursore. Su Windows non ha effetto. Su altre piattaforme (Linux, macOS),
    /// invia una sequenza di escape ANSI per modificare la forma del cursore.
    /// </summary>
    /// <param name="style">Lo stile del cursore da applicare.</param>
    public void SetCursorStyle(CursorStyle style)
    {
        if (OperatingSystem.IsWindows()) return;
        var sequence = style switch
        {
            CursorStyle.BlinkingBlock => "\x1B[1 q",
            CursorStyle.SteadyBlock => "\x1B[2 q",
            CursorStyle.BlinkingUnderline => "\x1B[3 q",
            CursorStyle.SteadyUnderline => "\x1B[4 q",
            CursorStyle.BlinkingBar => "\x1B[5 q",
            CursorStyle.SteadyBar => "\x1B[6 q",
            _ => "\x1B[1 q"
        };
        Console.Write(sequence);
    }
    
    /// <summary>
    /// Il font della console di sistema non può essere modificato programmaticamente.
    /// Questo metodo non ha alcun effetto.
    /// </summary>
    /// <param name="config">L'oggetto ScreenConfig contenente i nuovi parametri per il font.</param>
    public void SetFont(ScreenConfig config)
    {
        // Non fa nulla, come previsto per la console di sistema.
    }

    /// <summary>
    /// Il font della console di sistema non può essere modificato programmaticamente.
    /// Questo metodo non ha alcun effetto.
    /// </summary>
    /// <param name="fontPath">Il percorso o nome del nuovo font.</param>
    /// <param name="fontSize">La nuova dimensione in punti del font.</param>
    /// <param name="hinting">La nuova modalità di hinting per il rendering. Se null, mantiene l'impostazione corrente.</param>
    public void SetFont(string fontPath, int fontSize, SdlFontHinting? hinting = null)
    {
        // Non fa nulla, come previsto per la console di sistema.
    }
    
    /// <summary>
    /// Copia il contenuto dello schermo negli appunti. Non è implementato
    /// per SystemConsole perché non c'è un buffer interno da cui leggere.
    /// </summary>
    public void CopyBufferToClipboard()
    {
        // Non implementato per la console di sistema senza buffer.
    }

    /// <summary>
    /// Incolla il testo dagli appunti. Non è implementato perché la gestione
    /// è a carico del terminale dell'utente, non del programma.
    /// </summary>
    public void PasteFromClipboard()
    {
        // Non implementato per la console di sistema. La gestione è a livello di terminale.
    }

    /// <summary>
    /// Legge un blocco rettangolare di celle. Non è supportato da SystemConsole
    /// perché non c'è un buffer di schermo da cui leggere.
    /// </summary>
    /// <param name="rect">Il rettangolo che definisce l'area da leggere.</param>
    /// <returns>Una matrice vuota di ScreenCell.</returns>
    public ScreenCell[,] Get(ScreenRect rect)
    {
        // Restituisce un array vuoto perché non c'è un buffer da cui leggere.
        return new ScreenCell[0, 0];
    }

    /// <summary>
    /// Scrive un blocco di celle. Non è supportato da SystemConsole
    /// perché non c'è un buffer di schermo su cui scrivere in blocco.
    /// </summary>
    /// <param name="x">La coordinata X di destinazione (colonna).</param>
    /// <param name="y">La coordinata Y di destinazione (riga).</param>
    /// <param name="cells">La matrice bidimensionale di ScreenCell da scrivere.</param>
    public void Put(int x, int y, ScreenCell[,] cells)
    {
        // Non implementato per la console di sistema senza buffer.
    }

    /// <summary>
    /// Pulisce un'area rettangolare dello schermo. Non è supportato da SystemConsole
    /// perché non c'è un buffer su cui eseguire un'operazione parziale.
    /// </summary>
    /// <param name="rect">Il rettangolo che definisce l'area da pulire.</param>
    /// <param name="fillCell">La cella campione da usare per il riempimento.</param>
    public void ClearRect(ScreenRect rect, ScreenCell? fillCell = null)
    {
        // Non implementato per la console di sistema senza buffer.
    }
    
    /// <summary>
    /// Modifica un colore nella palette della console. Non è supportato
    /// da SystemConsole poiché la palette del terminale di sistema è fissa.
    /// Questo metodo non ha alcun effetto.
    /// </summary>
    /// <param name="index">L'indice del colore nella palette (da 0 a 15).</param>
    /// <param name="r">La componente Rossa del nuovo colore (0-255).</param>
    /// <param name="g">La componente Verde del nuovo colore (0-255).</param>
    /// <param name="b">La componente Blu del nuovo colore (0-255).</param>
    public void SetScrollRegion(int top, int bottom) { } // not supported
    public void SetPaletteColor(int index, int r, int g, int b)
    {
        if (index < 0 || index >= _activePalette.Length) return;
        _activePalette[index] = [(byte)Math.Clamp(r, 0, 255), (byte)Math.Clamp(g, 0, 255), (byte)Math.Clamp(b, 0, 255)];
        // Se il colore appena modificato è quello correntemente in uso, riemetti subito la
        // sequenza ANSI aggiornata (altrimenti il cambiamento non sarebbe visibile finché non
        // si stampa qualcosa con un ForegroundColor/BackgroundColor diverso e poi si ritorna).
        if (index == _currentFg) WriteAnsiFg(index);
        if (index == _currentBg) WriteAnsiBg(index);
    }

    private string? _activePaletteName;

    /// <summary>
    /// Sostituisce la palette attiva e riemette subito le sequenze ANSI per i colori
    /// correnti, così il cambio di palette è visibile immediatamente.
    /// </summary>
    public void SetActivePalette(byte[][] palette, string? name = null)
    {
        _activePalette = palette;
        _activePaletteName = name;
        WriteAnsiFg(_currentFg);
        WriteAnsiBg(_currentBg);
    }
    public byte[][] GetActivePalette() => _activePalette;
    public string? GetActivePaletteName() => _activePaletteName;

    #endregion
}