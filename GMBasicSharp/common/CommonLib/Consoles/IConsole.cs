// FILE: Interpreter.Consoles.IConsole.cs
using CommonLib.Screens;


namespace CommonLib.Consoles;

/// <summary>Rettangolo dello schermo (x, y, w, h).</summary>
public struct ScreenRect
{
    public int x, y, w, h;
    public ScreenRect(int x, int y, int w, int h) { this.x = x; this.y = y; this.w = w; this.h = h; }
}

/// <summary>
/// Definisce i possibili stili/forme del cursore del terminale.
/// </summary>
public enum CursorStyle
{
    BlinkingBlock,
    SteadyBlock,
    BlinkingUnderline,
    SteadyUnderline,
    BlinkingBar,
    SteadyBar
}

/// <summary>
/// Rappresenta le informazioni su un tasto premuto. Simile a System.ConsoleKeyInfo.
/// </summary>
public readonly record struct TerminalKeyInfo(char KeyChar, int KeyCode, bool Shift, bool Alt, bool Ctrl);

public class SdlConsoleCancelEventArgs : EventArgs
{
    public bool Cancel { get; set; }
    public ConsoleSpecialKey SpecialKey { get; }
    
    public SdlConsoleCancelEventArgs(ConsoleSpecialKey specialKey)
    {
        SpecialKey = specialKey;
    }
}
    
// Aggiungi il tuo delegato personalizzato qui
public delegate void SdlConsoleCancelEventHandler(object sender, SdlConsoleCancelEventArgs e);

/// <summary>
/// Definisce un'interfaccia astratta e completa per un dispositivo console,
/// che astrae System.Console per permettere implementazioni alternative (es. SDL).
/// </summary>
public interface IConsole : IDisposable
{
    // --- Eventi ---

    /// <summary>
    /// Ottiene o imposta un valore che indica se la console deve eseguire
    /// un rendering automatico e immediato dopo ogni operazione di scrittura.
    /// Se impostato a false, è necessario chiamare Present() manualmente per visualizzare il buffer.
    /// </summary>
    bool AutoFlush { get; set; }
    
    /// <summary>
    /// Si verifica quando viene premuta la combinazione di tasti Ctrl+C.
    /// Utilizza il delegato standard di .NET per la massima compatibilità.
    /// </summary>
    event SdlConsoleCancelEventHandler? CancelKeyPress;

    /// <summary>
    /// Ottiene la configurazione con cui la console è stata inizializzata.
    /// Questo oggetto contiene tutti i parametri di avvio come dimensioni,
    /// colori di default e impostazioni del font.
    /// </summary>
    ScreenConfig Configuration { get; }
    
    // --- Stream Standard ---
        
    /// <summary>
    /// Ottiene il flusso di input standard.
    /// </summary>
    TextReader In { get; }
        
    /// <summary>
    /// Ottiene il flusso di output standard.
    /// </summary>
    TextWriter Out { get; }

    /// <summary>
    /// Ottiene il flusso di errore standard.
    /// </summary>
    TextWriter Error { get; }

    /// <summary>
    /// Imposta un nuovo TextReader come stream di input standard per la console.
    /// </summary>
    /// <param name="newIn">Il nuovo TextReader da usare. Non può essere nullo.</param>
    void SetIn(TextReader newIn);

    /// <summary>
    /// Imposta un nuovo TextWriter come stream di output standard per la console.
    /// </summary>
    /// <param name="newOut">Il nuovo TextWriter da usare. Non può essere nullo.</param>
    void SetOut(TextWriter newOut);

    /// <summary>
    /// Imposta un nuovo TextWriter come stream di errore standard per la console.
    /// </summary>
    /// <param name="newError">Il nuovo TextWriter da usare. Non può essere nullo.</param>
    void SetError(TextWriter newError);
    
    // --- Proprietà di Stato e Aspetto ---
        
    string Title { get; set; }
    int BackgroundColor { get; set; }
    int ForegroundColor { get; set; }
    int BufferWidth { get; set; }
    int BufferHeight { get; set; }
    int WindowWidth { get; set; }
    int WindowHeight { get; set; }
    int WindowLeft { get; set; }
    int WindowTop { get; set; }
    int CursorLeft { get; set; }
    int CursorTop { get; set; }
    bool CursorVisible { get; set; }
        
    // --- Metodi di Output ---

    void Beep();
    void Clear();
    void ResetColor();
    void SetCursorPosition(int left, int top);
    void SetWindowPosition(int left, int top);
    void SetWindowSize(int width, int height);
    
    /// <summary>
    /// Imposta la regione di scroll. top=0, bottom=0 ripristina lo scroll a tutto schermo.
    /// Le righe fuori dalla regione restano fisse.
    /// </summary>
    void SetScrollRegion(int top, int bottom);
        
    /// <summary>
    /// Ottiene un valore che indica se il contenuto della console è stato
    /// modificato e necessita di essere ridisegnato.
    /// Per le console di sistema, può restituire sempre true.
    /// </summary>
    bool IsDirty { get; }
    
    // --- Metodi di Controllo del Ciclo di Vita (per implementazioni non standard) ---
        
    /// <summary>
    /// Per le console grafiche, esegue un ciclo di elaborazione degli eventi.
    /// Per le console di sistema, potrebbe non fare nulla.
    /// </summary>
    /// <returns>True se è stata richiesta la chiusura, altrimenti false.</returns>
    bool ProcessEvents();

    /// <summary>
    /// Inizializza e mostra la console. Necessario per implementazioni grafiche.
    /// </summary>
    void Show();
        
    /// <summary>
    /// Aggiorna/presenta il contenuto della console a schermo.
    /// Per le console di sistema non fa nulla, per quelle grafiche esegue il rendering.
    /// </summary>
    void Present();

    /// <summary>
    /// Per le console grafiche, imposta la visibilità della finestra.
    /// Per le console di sistema, non fa nulla.
    /// </summary>
    /// <param name="visible">True per mostrare la finestra, false per nasconderla.</param>
    void SetVisibility(bool visible);
    
    /// <summary>
    /// Imposta lo stile del cursore (blocco, barra, ecc.).
    /// </summary>
    /// <param name="style">Lo stile del cursore da applicare.</param>
    void SetCursorStyle(CursorStyle style);

    /// <summary>
    /// Per le console grafiche, imposta un nuovo font utilizzando un oggetto di configurazione.
    /// Per le console di sistema, questo metodo non ha effetto.
    /// </summary>
    /// <param name="config">L'oggetto ScreenConfig contenente i nuovi parametri per il font.</param>
    void SetFont(ScreenConfig config);

    /// <summary>
    /// Per le console grafiche, imposta un nuovo font specificando i suoi parametri.
    /// Per le console di sistema, questo metodo non ha effetto.
    /// </summary>
    /// <param name="fontPath">Il percorso o nome del nuovo font.</param>
    /// <param name="fontSize">La nuova dimensione in punti del font.</param>
    /// <param name="hinting">La nuova modalità di hinting per il rendering. Se null, mantiene l'impostazione corrente.</param>
    void SetFont(string fontPath, int fontSize, SdlFontHinting? hinting = null);
 
    /// <summary>
    /// Legge il contenuto del buffer di schermo della console (se applicabile)
    /// e lo copia negli appunti di sistema.
    /// Le implementazioni senza un buffer interno possono non eseguire alcuna operazione.
    /// </summary>
    void CopyBufferToClipboard();

    /// <summary>
    /// Innesca un'operazione di incolla dagli appunti di sistema.
    /// La gestione effettiva dell'inserimento del testo è delegata al servizio di tastiera attivo.
    /// </summary>
    void PasteFromClipboard();    
    
    /// <summary>
    /// Modifica un colore nella palette della console, se supportato.
    /// </summary>
    /// <param name="index">L'indice del colore nella palette (da 0 a 255).</param>
    /// <param name="r">La componente Rossa del nuovo colore (0-255).</param>
    /// <param name="g">La componente Verde del nuovo colore (0-255).</param>
    /// <param name="b">La componente Blu del nuovo colore (0-255).</param>
    void SetPaletteColor(int index, int r, int g, int b);

    /// <summary>Imposta la palette attiva della console.</summary>
    /// <param name="palette">I colori RGB della nuova palette attiva.</param>
    /// <param name="name">Il nome registrato della palette, se proviene da <c>PaletteManager</c> (altrimenti null).</param>
    void SetActivePalette(byte[][] palette, string? name = null);

    /// <summary>Restituisce la palette attualmente attiva.</summary>
    byte[][] GetActivePalette();

    /// <summary>Restituisce il nome della palette attiva, o null.</summary>
    string? GetActivePaletteName();
    
    #region Nuovi Metodi di Manipolazione del Buffer

    /// <summary>
    /// Legge un blocco rettangolare di celle dal buffer di schermo.
    /// L'implementazione deve gestire correttamente i limiti del rettangolo,
    /// senza lanciare eccezioni se l'area specificata è parzialmente o
    /// totalmente fuori dai limiti del buffer.
    /// </summary>
    /// <param name="rect">Il rettangolo che definisce l'area da leggere, in coordinate del buffer.</param>
    /// <returns>
    /// Una matrice bidimensionale di ScreenCell contenente i dati dell'area.
    /// La dimensione della matrice corrisponderà a quella del rettangolo richiesto.
    /// </returns>
    ScreenCell[,] Get(ScreenRect rect);

    /// <summary>
    /// Scrive un blocco rettangolare di celle nel buffer di schermo in una posizione specificata.
    /// L'implementazione deve eseguire il clipping, ignorando le parti del blocco
    /// che cadono fuori dai limiti del buffer di destinazione.
    /// </summary>
    /// <param name="x">La coordinata X di destinazione (colonna) dove iniziare a scrivere il blocco.</param>
    /// <param name="y">La coordinata Y di destinazione (riga) dove iniziare a scrivere il blocco.</param>
    /// <param name="cells">La matrice bidimensionale di ScreenCell da scrivere.</param>
    void Put(int x, int y, ScreenCell[,] cells);

    /// <summary>
    /// Pulisce un'area rettangolare del buffer di schermo, riempiendola con una cella
    /// specifica. L'implementazione deve gestire i limiti dell'area,
    /// agendo solo sulla porzione del rettangolo che si sovrappone al buffer.
    /// </summary>
    /// <param name="rect">Il rettangolo che definisce l'area da pulire, in coordinate del buffer.</param>
    /// <param name="fillCell">
    /// La cella campione da usare per il riempimento. Se null, l'implementazione
    /// deve usare una cella vuota (' ') con i colori di default correnti della console.
    /// </param>
    void ClearRect(ScreenRect rect, ScreenCell? fillCell = null);

    #endregion
}