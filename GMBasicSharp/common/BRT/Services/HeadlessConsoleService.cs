// FILE: Basic.Runtime.Services.HeadlessConsoleService.cs

namespace Basic.Runtime.Services;

using CommonLib.Screens;
using CommonLib.Consoles;

/// <summary>
/// ConsoleService headless che scrive su un TextWriter invece che su System.Console.
/// </summary>
public class HeadlessConsoleService : IConsoleService
{
    private readonly TextWriter _writer;
    private int _cursorLeft, _cursorTop;
    // Buffer a griglia reale per SAVE/RESTORE SCREEN in modalità testo
    private ScreenCell[] _cells;
    private const int BufferWidth = 80, BufferHeight = 25;

    public HeadlessConsoleService(TextWriter writer)
    {
        _writer = writer;
        _cells = new ScreenCell[BufferWidth * BufferHeight];
    }

    public void Clear()
    {
        _cells = new ScreenCell[BufferWidth * BufferHeight];
        _cursorLeft = 0; _cursorTop = 0;
    }

    public void Print(string? text)
    {
        if (text == null) return;
        _writer.Write(text);
        foreach (var ch in text)
        {
            if (ch == '\n') { _cursorLeft = 0; _cursorTop = Math.Min(_cursorTop + 1, BufferHeight - 1); }
            else if (ch == '\r') { _cursorLeft = 0; }
            else if (_cursorLeft < BufferWidth && _cursorTop < BufferHeight)
            {
                _cells[_cursorTop * BufferWidth + _cursorLeft] = new ScreenCell(ch, 15, 0, default);
                _cursorLeft++;
            }
        }
    }

    public void PrintLine(string? text)
    {
        Print(text);
        PrintNewLine();
        // Flush a fine riga: senza, un processo LUNGO (daemon, server) con stdout
        // rediretto su file non mostra NULLA finché non esce (il TextWriter sottostante
        // bufferizza). Bug reale: `tools/sh/webapp_up.sh logs` restava vuoto mentre la
        // dashboard rispondeva. Chi stampa molto può usare PRINT con `;` (flush alla riga
        // successiva) o il flush esplicito del writer.
        try { _writer.Flush(); } catch { /* writer chiuso */ }
    }

    public void PrintNewLine()
    {
        _writer.WriteLine();
        _cursorLeft = 0;
        _cursorTop = Math.Min(_cursorTop + 1, BufferHeight - 1);
    }

    public ScreenCell[,]? GetScreen(ScreenRect rect)
    {
        var result = new ScreenCell[rect.h, rect.w];
        for (var r = 0; r < rect.h; r++)
            for (var c = 0; c < rect.w; c++)
                result[r, c] = _cells[Math.Clamp(r + rect.y, 0, BufferHeight - 1) * BufferWidth + Math.Clamp(c + rect.x, 0, BufferWidth - 1)];
        return result;
    }

    public void PutScreen(int x, int y, ScreenCell[,] cells)
    {
        var h = cells.GetLength(0); var w = cells.GetLength(1);
        for (var r = 0; r < h; r++)
            for (var c = 0; c < w; c++)
            {
                var rr = y + r; var cc = x + c;
                if (rr >= 0 && rr < BufferHeight && cc >= 0 && cc < BufferWidth)
                    _cells[rr * BufferWidth + cc] = cells[r, c];
            }
        _cursorLeft = 0; _cursorTop = 0;
    }

    public void SetForegroundColor(int colorIndex) { }
    public void SetBackgroundColor(int colorIndex) { }
    public void ResetColor() { }

    public void SetCursorPosition(int left, int top)
    {
        _cursorLeft = left;
        _cursorTop = top;
    }

    public void Beep() { }

    public int CursorLeft
    {
        get => _cursorLeft;
        set => _cursorLeft = value;
    }

    public int CursorTop
    {
        get => _cursorTop;
        set => _cursorTop = value;
    }

    public int WindowWidth => 80;
    public int WindowHeight => 24;

    public string Title
    {
        get => "";
        set { }
    }

    public void SetPaletteColor(int index, int r, int g, int b) { }
    public void SetActivePalette(byte[][] palette) { }
    public byte[][] GetActivePalette() => CommonLib.Screens.ScreenConfig.Palettes.GetVGA16Palette();
    public void SetScrollRegion(int top, int bottom) { }
    public void Alert(string message, string title) { }
    public string? GetInput(string prompt, string? mask) => null;
    public string? GetPassword(string prompt) => null;
    public bool Confirm(string message, string title) => false;
    public int? Menu(string title, string[] items) => null;
    public string? FileDialog(string title, string pattern, bool save) => null;
    public string? ColorPicker(string title, int defaultColor) => null;
    public string? DatePicker(string title, string defaultDate) => null;
    public int? Spinner(string title, string[] items) => null;
    public bool Toggle(string title, string prompt, bool initial) => initial;
    public double? Slider(string title, double min, double max, double initial) => initial;
    public void MessageBox(string title, string message) { }
}
