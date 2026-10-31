using CommonLib.Screens;
using CommonLib.Consoles;

// FILE: Interpreter.Terminal.ITerminalBuffer.cs


namespace CommonGraphics.Terminal;

/// <summary>
/// Definisce le operazioni di base che un processore di sequenze di escape
/// può eseguire su un buffer di terminale, astraendolo dall'implementazione
/// specifica (es. SdlConsole o il futuro Terminal).
/// </summary>
public interface ITerminalBuffer
{
    /// <summary>
    /// Ottiene o imposta il colore di primo piano corrente per i nuovi caratteri.
    /// </summary>
    int CurrentForegroundColor { get; set; }

    /// <summary>
    /// Ottiene o imposta il colore di sfondo corrente per i nuovi caratteri.
    /// </summary>
    int CurrentBackgroundColor { get; set; }
        
    /// <summary>
    /// Ottiene o imposta gli attributi correnti (es. grassetto) per i nuovi caratteri.
    /// </summary>
    ScreenCellAttributes CurrentAttributes { get; set; }

    /// <summary>
    /// Ottiene la riga corrente del cursore (0-based).
    /// </summary>
    int CursorRow { get; }
        
    /// <summary>
    /// Ottiene la colonna corrente del cursore (0-based).
    /// </summary>
    int CursorCol { get; }
        
    /// <summary>
    /// Ottiene o imposta la visibilità del cursore.
    /// </summary>
    bool IsCursorVisible { get; set; }

    /// <summary>
    /// Stampa un singolo carattere "nudo" nella posizione corrente, gestendo
    /// l'avanzamento del cursore e i caratteri di controllo base.
    /// </summary>
    /// <param name="character">Il carattere da stampare.</param>
    void Print(char character);
        
    /// <summary>
    /// Pulisce l'intero schermo.
    /// </summary>
    void ClearScreen();
        
    /// <summary>
    /// Imposta la posizione del cursore.
    /// </summary>
    /// <param name="row">La riga (1-based).</param>
    /// <param name="col">La colonna (1-based).</param>
    void SetCursorPosition(int row, int col);

    /// <summary>
    /// Cancella il contenuto della riga corrente dalla posizione del cursore fino alla fine.
    /// </summary>
    void EraseToEndOfLine();
        
    /// <summary>
    /// Ottiene o imposta lo stile del cursore.
    /// </summary>
    CursorStyle CursorStyle { get; set; }
}