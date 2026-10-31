using CommonLib.Screens;
using CommonLib.Consoles;
// FILE: Interpreter.Consoles.SdlConsole.ITerminalBuffer.cs

using System.Drawing;
using CommonGraphics.Terminal;

namespace CommonGraphics.Consoles;

/// <summary>
/// Contiene la logica interna di rendering e gestione delle risorse grafiche
/// per la classe parziale SdlConsole.
/// </summary>
public sealed partial class SdlConsole
{

    /// <summary>
    /// Ottiene o imposta il colore di primo piano corrente per il buffer.
    /// </summary>
    int ITerminalBuffer.CurrentForegroundColor { get; set; }

    /// <summary>
    /// Ottiene o imposta il colore di sfondo corrente per il buffer.
    /// </summary>
    int ITerminalBuffer.CurrentBackgroundColor { get; set; }

    /// <summary>
    /// Ottiene o imposta gli attributi correnti della cella (es. inverso).
    /// </summary>
    ScreenCellAttributes ITerminalBuffer.CurrentAttributes { get; set; }

    /// <summary>
    /// Ottiene la riga corrente del cursore.
    /// </summary>
    int ITerminalBuffer.CursorRow => _cursorRow;

    /// <summary>
    /// Ottiene la colonna corrente del cursore.
    /// </summary>
    int ITerminalBuffer.CursorCol => _cursorCol;

    /// <summary>
    /// Ottiene o imposta la visibilità del cursore.
    /// </summary>
    bool ITerminalBuffer.IsCursorVisible
    {
        get => CursorVisible;
        set => CursorVisible = value;
    }

    /// <summary>
    /// Ottiene o imposta lo stile del cursore.
    /// </summary>
    CursorStyle ITerminalBuffer.CursorStyle { get; set; } = CursorStyle.BlinkingBlock;

    /// <summary>
    /// Metodo helper che gestisce l'avanzamento alla riga successiva,
    /// eseguendo lo scroll del buffer se il cursore supera l'altezza massima.
    /// Questa logica è centralizzata qui per essere riutilizzata sia da un
    /// a-capo esplicito (\n) sia dal wrapping automatico della linea.
    /// </summary>
    private void AdvanceLine()
    {
        _cursorRow++;
        //Debug.WriteLine($"[ADVLINE] Cursor row advanced to {_cursorRow}.");
        if (_cursorRow >= _height)
        {
            //Debug.WriteLine($"[ADVLINE SCROLL] Cursor ({_cursorCol}, {_cursorRow}) is out of bounds ({_height}). SCROLLING UP.");
            ScrollUp();
            _cursorRow = _height - 1; // Riposiziona il cursore sull'ultima riga valida
            //Debug.WriteLine($"[ADVLINE SCROLL] After scroll, cursor is clamped to: ({_cursorCol}, {_cursorRow})");
        }
    }
    
    /// <summary>
    /// Scrive un carattere nel buffer nella posizione corrente del cursore.
    /// La logica è stata refattorizzata per utilizzare un metodo helper
    /// centralizzato (AdvanceLine) che gestisce correttamente lo scrolling
    /// sia per i newline espliciti (\n) sia per il line wrapping automatico,
    /// risolvendo un bug di sovrascrittura della riga "READY.".
    /// </summary>
    /// <param name="character">Il carattere da scrivere.</param>
    void ITerminalBuffer.Print(char character)
    {
        lock (_renderLock)
        {
        _isContentDirty = true;
        //ebug.WriteLine($"[PRINT START] Char: '{(character == '\n' ? "\\n" : character.ToString())}', Cursor: ({_cursorCol}, {_cursorRow})");

        switch (character)
        {
            case '\n':
                _cursorCol = 0;
                AdvanceLine(); // Usa il metodo centralizzato per l'a-capo
                break;
            case '\r':
                _cursorCol = 0;
                //Debug.WriteLine($"[PRINT \\r]    Cursor moved to: ({_cursorCol}, {_cursorRow})");
                break;
            case '\b':
                if (_cursorCol > 0) _cursorCol--;
                //Debug.WriteLine($"[PRINT \\b]    Cursor moved to: ({_cursorCol}, {_cursorRow})");
                break;
            case '\t':
                // Il TAB non deve causare un a-capo, si ferma al bordo.
                _cursorCol = Math.Min((_cursorCol / 8 + 1) * 8, _width - 1);
                //Debug.WriteLine($"[PRINT \\t]    Cursor moved to: ({_cursorCol}, {_cursorRow})");
                break;
            default: // Carattere stampabile
                // Se il cursore si trova già sul bordo, va a capo PRIMA di scrivere.
                if (_cursorCol >= _width)
                {
                    _cursorCol = 0;
                    AdvanceLine(); // Usa il metodo centralizzato
                }

                //Debug.WriteLine($"[PRINT WRITE]  Writing '{character}' at final position: ({_cursorCol}, {_cursorRow})");
                _screenBuffer[_cursorRow, _cursorCol] = new ScreenCell(
                    character, ((ITerminalBuffer)this).CurrentForegroundColor,
                    ((ITerminalBuffer)this).CurrentBackgroundColor, ((ITerminalBuffer)this).CurrentAttributes);
                MarkCellDirty(_cursorRow, _cursorCol);

                // Avanza la colonna per il prossimo carattere.
                _cursorCol++;
                break;
        }
        } // _renderLock

        EnsureCursorIsVisible();
        //Debug.WriteLine($"[PRINT END]   Final cursor position for next char: ({_cursorCol}, {_cursorRow})");
    }

    /// <summary>
    /// Pulisce l'intero buffer di schermo.
    /// </summary>
    void ITerminalBuffer.ClearScreen()
    {
        var emptyCell = new ScreenCell(' ', ((ITerminalBuffer)this).CurrentForegroundColor,
            ((ITerminalBuffer)this).CurrentBackgroundColor);
        lock (_renderLock)
        {
            _dirtyCells.Clear();
            for (var r = 0; r < _height; r++)
            for (var c = 0; c < _width; c++)
            {
                _screenBuffer[r, c] = emptyCell;
                _dirtyCells.Add(new Point(c, r));
            }
        }

        _cursorRow = 0;
        _cursorCol = 0;
        _viewLeft = 0;
        _viewTop = 0;
        _forceRedraw = true;
        _isContentDirty = true;
    }

    /// <summary>
    /// Imposta la posizione del cursore.
    /// </summary>
    /// <param name="row">La riga (1-based).</param>
    /// <param name="col">La colonna (1-based).</param>
    void ITerminalBuffer.SetCursorPosition(int row, int col) => SetCursorPosition(col - 1, row - 1);

    /// <summary>
    /// Cancella la riga dalla posizione del cursore fino alla fine.
    /// </summary>
    void ITerminalBuffer.EraseToEndOfLine()
    {
        lock (_renderLock)
        {
        var emptyCell = new ScreenCell(' ', ((ITerminalBuffer)this).CurrentForegroundColor,
            ((ITerminalBuffer)this).CurrentBackgroundColor);
        for (var c = _cursorCol; c < _width; c++)
        {
            _screenBuffer[_cursorRow, c] = emptyCell;
            MarkCellDirty(_cursorRow, c);
        }

        _isContentDirty = true;
        }
    }
    
}