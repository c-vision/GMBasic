using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Griglia di celle dell'emulatore (OS-agnostica). Contiene lo schermo
/// corrente e tutte le operazioni di basso livello (scroll, insert/delete di
/// righe e caratteri, cancellazione, regione di scroll, cursore salvato,
/// swap dell'alternate screen). Le coordinate sono 0-based.
/// </summary>
public class EmuGrid
{
    public int Cols { get; private set; }
    public int Rows { get; private set; }
    public EmuCell[,] Cells { get; private set; }

    public int CursorX;
    public int CursorY;

    public int SavedX;
    public int SavedY;
    public EmuStyle SavedStyle = new();

    public EmuStyle DefaultStyle = new();

    public bool AutoWrap = true;
    public bool OriginMode;
    public bool CursorVisible = true;

    public int ScrollTop;
    public int ScrollBottom;

    public bool AltActive;
    private EmuCell[,]? _altCells;

    public EmuGrid(int cols, int rows)
    {
        Cols = cols;
        Rows = rows;
        Cells = NewBlank();
        ScrollTop = 0;
        ScrollBottom = rows - 1;
    }

    private EmuCell[,] NewBlank()
    {
        var c = new EmuCell[Rows, Cols];
        for (var y = 0; y < Rows; y++)
            for (var x = 0; x < Cols; x++)
                c[y, x] = new EmuCell { Char = ' ', Style = DefaultStyle.Clone() };
        return c;
    }

    /// <summary>Legge una cella (clampata all'ultima cella valida se fuori range).</summary>
    public EmuCell CellAt(int x, int y)
    {
        x = Math.Clamp(x, 0, Cols - 1);
        y = Math.Clamp(y, 0, Rows - 1);
        return Cells[y, x];
    }

    /// <summary>Scrive una cella (ignorata se fuori range).</summary>
    public void SetCell(int x, int y, char ch, EmuStyle style, bool wide = false, bool cont = false)
    {
        if (x < 0 || x >= Cols || y < 0 || y >= Rows) return;
        Cells[y, x] = new EmuCell
        {
            Char = ch,
            Style = style.Clone(),
            IsWide = wide,
            IsContinuation = cont,
        };
    }

    private EmuCell Blank() => new EmuCell { Char = ' ', Style = DefaultStyle.Clone() };

    /// <summary>Cancella l'intera griglia (spazi con stile di default).</summary>
    public void Clear()
    {
        for (var y = 0; y < Rows; y++)
            for (var x = 0; x < Cols; x++)
                Cells[y, x] = Blank();
    }

    /// <summary>Cancella il rettangolo [x0..x1] x [y0..y1] (estremi inclusi).</summary>
    public void ClearRect(int x0, int y0, int x1, int y1)
    {
        x1 = Math.Min(x1, Cols - 1);
        y1 = Math.Min(y1, Rows - 1);
        for (var y = Math.Max(0, y0); y <= y1; y++)
            for (var x = Math.Max(0, x0); x <= x1; x++)
                Cells[y, x] = Blank();
    }

    /// <summary>Ridimensiona la griglia (i contenuti precedenti vengono persi).</summary>
    public void Resize(int cols, int rows)
    {
        Cols = cols;
        Rows = rows;
        Cells = NewBlank();
        ScrollTop = 0;
        ScrollBottom = rows - 1;
        CursorX = Math.Min(CursorX, cols - 1);
        CursorY = Math.Min(CursorY, rows - 1);
        AltActive = false;
        _altCells = null;
    }

    /// <summary>Scroll della regione [top..bottom] di n righe verso l'alto.</summary>
    public void ScrollUp(int top, int bottom, int n)
    {
        if (n <= 0 || top >= bottom) return;
        n = Math.Min(n, bottom - top);
        for (var y = top + n; y <= bottom; y++)
            for (var x = 0; x < Cols; x++)
                Cells[y - n, x] = Cells[y, x];
        for (var y = bottom - n + 1; y <= bottom; y++)
            for (var x = 0; x < Cols; x++)
                Cells[y, x] = Blank();
    }

    /// <summary>Scroll della regione [top..bottom] di n righe verso il basso.</summary>
    public void ScrollDown(int top, int bottom, int n)
    {
        if (n <= 0 || top >= bottom) return;
        n = Math.Min(n, bottom - top);
        for (var y = bottom; y >= top + n; y--)
            for (var x = 0; x < Cols; x++)
                Cells[y, x] = Cells[y - n, x];
        for (var y = top; y < top + n; y++)
            for (var x = 0; x < Cols; x++)
                Cells[y, x] = Blank();
    }

    /// <summary>Inserisce n spazi a partire dalla colonna at nella riga y.</summary>
    public void InsertChars(int y, int at, int n)
    {
        if (y < 0 || y >= Rows || at < 0 || at >= Cols || n <= 0) return;
        n = Math.Min(n, Cols - at);
        var row = new EmuCell[Cols];
        for (var x = 0; x < Cols; x++) row[x] = Cells[y, x];
        var rot = new EmuCell[Cols];
        for (var x = 0; x < Cols; x++) rot[(x + n) % Cols] = row[x];
        for (var x = 0; x < Cols; x++) Cells[y, x] = rot[x];
        for (var x = at; x < at + n; x++) Cells[y, x] = Blank();
    }

    /// <summary>Cancella n caratteri a partire dalla colonna at nella riga y.</summary>
    public void DeleteChars(int y, int at, int n)
    {
        if (y < 0 || y >= Rows || at < 0 || at >= Cols || n <= 0) return;
        n = Math.Min(n, Cols - at);
        var row = new EmuCell[Cols];
        for (var x = 0; x < Cols; x++) row[x] = Cells[y, x];
        var rot = new EmuCell[Cols];
        for (var x = 0; x < Cols; x++) rot[(x - n + Cols) % Cols] = row[x];
        for (var x = 0; x < Cols; x++) Cells[y, x] = rot[x];
        for (var x = Cols - n; x < Cols; x++) Cells[y, x] = Blank();
    }

    /// <summary>Line feed: avanza il cursore di una riga, scroll se sull'ultima della regione.</summary>
    public void NewLine()
    {
        if (CursorY == ScrollBottom)
            ScrollUp(ScrollTop, ScrollBottom, 1);
        else if (CursorY < Rows - 1)
            CursorY++;
    }

    /// <summary>Ritorno a capo: cursore alla colonna 0.</summary>
    public void CarriageReturn() => CursorX = 0;

    /// <summary>Imposta la regione di scroll (valori 1-based; 0 = intero schermo).</summary>
    public void SetScrollRegion(int top1, int bottom1)
    {
        var top = Math.Max(1, top1);
        var bottom = bottom1 == 0 ? Rows : bottom1;
        top = Math.Min(top, Rows);
        bottom = Math.Min(bottom, Rows);
        if (top < bottom)
        {
            ScrollTop = top - 1;
            ScrollBottom = bottom - 1;
            CursorX = 0;
            CursorY = OriginMode ? ScrollTop : 0;
        }
    }

    /// <summary>Salva la posizione del cursore. Lo stile salvato e gestito dall'Emulator.</summary>
    public void SaveCursor()
    {
        SavedX = CursorX;
        SavedY = CursorY;
    }

    /// <summary>Ripristina il cursore salvato (posizione + stile).</summary>
    public void RestoreCursor()
    {
        CursorX = Math.Min(SavedX, Cols - 1);
        CursorY = Math.Min(SavedY, Rows - 1);
    }

    /// <summary>Swap con l'alternate screen (modalita DEC ?47/?1047/?1049).</summary>
    public void SwapAltScreen(bool on)
    {
        if (on && !AltActive)
        {
            _altCells = Cells;
            Cells = NewBlank();
            AltActive = true;
            CursorX = 0;
            CursorY = 0;
        }
        else if (!on && AltActive)
        {
            Cells = _altCells ?? NewBlank();
            _altCells = null;
            AltActive = false;
            CursorX = 0;
            CursorY = 0;
        }
    }
}
