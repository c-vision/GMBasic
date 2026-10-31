// FILE: Interpreter.Consoles.Headless.cs
using CommonLib.Screens;

using System.Text;

namespace CommonLib.Consoles;

/// <summary>
/// Implementazione "headless" di IConsole con buffer di schermo interno e screen log.
/// 
/// Scopo principale:
/// - Mantiene un buffer ScreenCell[,] che traccia lo stato visivo della console
/// - Salva snapshot del buffer PRIMA di ogni CLS (screen log)
/// - Cattura lo schermo finale a fine programma
/// - Output va sia al buffer screen che a uno stream lineare (per retrocompatibilità)
/// 
/// Usata da test_exec / test_exec_all per verificare l'output reale dei programmi BASIC.
/// In modalità script headless normale (non test), l'output viene scartato (TextWriter.Null).
/// </summary>
public sealed class HeadlessConsole : IConsole
{
    // --- Buffer di schermo ---
    private ScreenCell[,] _buffer = new ScreenCell[0, 0];
    private int _width = 80;
    private int _height = 25;

    // --- Screen log: snapshot prima di ogni CLS + finale ---
    private readonly List<ScreenSnapshot> _screenLog = new();

    // --- Scrollback: righe scartate da ScrollUp() prima che raggiungano la
    // riga superiore visibile. Senza questo, un programma che stampa piu'
    // righe dell'altezza del buffer (25) senza mai chiamare CLS perde per
    // sempre le righe piu' vecchie: GetAllScreenText() catturava solo gli
    // snapshot prima di ogni CLS + quello finale, quindi qualunque testo
    // (incluso un marker EXPECT) scrollato via PRIMA del prossimo CLS (o
    // della fine programma, se il programma non chiama mai CLS) non era mai
    // recuperabile da nessuno snapshot.
    private readonly List<string> _scrollbackLines = new();
    private int _scrollTop, _scrollBottom = -1; // -1 = full screen
    private byte[][] _activePalette = [];
    private string? _activePaletteName;

    // --- Writer interno: scrive su stream lineare + buffer screen ---
    private readonly HeadlessWriter _writer;

    // --- Stream lineare (output captured dal test runner) ---
    private readonly TextWriter _linearOut;

    // --- Flag: primo CLS non genera snapshot (schermo già vuoto) ---
    private bool _isFirstOutput = true;

    // --- Eventi ---
#pragma warning disable CS0067
    public event SdlConsoleCancelEventHandler? CancelKeyPress;
#pragma warning restore CS0067

    /// <summary>
    /// Crea una HeadlessConsole. Se <paramref name="linearOut"/> è null,
    /// l'output viene scartato (equivalente a TextWriter.Null).
    /// </summary>
    public HeadlessConsole(TextWriter? linearOut = null)
    {
        _linearOut = linearOut ?? TextWriter.Null;
        _writer = new HeadlessWriter(this);
        _writer.SetSecondaryOut(_linearOut);
        _activePalette = Configuration.ResolvedPalette;
        _activePaletteName = Configuration.PaletteName;

        InitializeBuffer();

        In = TextReader.Null;
        Out = _writer;
        Error = _writer;
    }

    // --- Proprietà IConsole ---

    public bool AutoFlush { get; set; } = true;
    public ScreenConfig Configuration { get; } = ScreenConfig.GetDefault(ScreenMode.Text);
    public TextReader In { get; private set; }
    public TextWriter Out { get; private set; }
    public TextWriter Error { get; private set; }
    public string Title { get; set; } = string.Empty;
    public int BackgroundColor { get; set; }
    public int ForegroundColor { get; set; }
    public int BufferWidth
    {
        get => _width;
        set => throw new NotSupportedException("Cannot resize HeadlessConsole at runtime.");
    }
    public int BufferHeight
    {
        get => _height;
        set => throw new NotSupportedException("Cannot resize HeadlessConsole at runtime.");
    }
    public int WindowWidth
    {
        get => _width;
        set => throw new NotSupportedException("Cannot resize HeadlessConsole at runtime.");
    }
    public int WindowHeight
    {
        get => _height;
        set => throw new NotSupportedException("Cannot resize HeadlessConsole at runtime.");
    }
    public int WindowLeft { get; set; }
    public int WindowTop { get; set; }
    public int CursorLeft { get; set; }
    public int CursorTop { get; set; }
    public bool CursorVisible { get; set; }
    public bool IsDirty => false;

    // --- Screen Log (accesso pubblico per i test runner) ---

    /// <summary>
    /// Lista di snapshot: uno prima di ogni CLS (tranne il primo), più lo snapshot finale.
    /// </summary>
    public IReadOnlyList<ScreenSnapshot> ScreenLog => _screenLog;

    /// <summary>
    /// Cattura lo schermo corrente come snapshot finale e lo aggiunge al log.
    /// Da chiamare a fine programma (quando HasActiveTasks == false).
    /// </summary>
    public ScreenSnapshot CaptureFinalScreen()
    {
        var snapshot = CaptureSnapshot("END");
        _screenLog.Add(snapshot);
        return snapshot;
    }

    /// <summary>
    /// Testo combinato di tutti gli snapshot nello screen log, separati da marker CLS.
    /// Ogni snapshot è renderizzato come testo piano.
    /// </summary>
    public string GetAllScreenText()
    {
        var sb = new StringBuilder();

        if (_scrollbackLines.Count > 0)
        {
            sb.AppendLine("--- SCROLLBACK ---");
            foreach (var line in _scrollbackLines)
                sb.AppendLine(line);
        }

        if (_screenLog.Count == 0)
            return sb.ToString().TrimEnd();

        for (var i = 0; i < _screenLog.Count; i++)
        {
            if (i > 0 || _scrollbackLines.Count > 0)
                sb.AppendLine("--- CLS ---");
            sb.AppendLine(_screenLog[i].RenderText());
        }
        return sb.ToString().TrimEnd();
    }

    // --- Metodi IConsole ---

    public void SetIn(TextReader newIn) => In = newIn ?? TextReader.Null;
    public void SetOut(TextWriter newOut)
    {
        // Quando il test runner chiama SetOut, sostituisce il writer interno
        // in modo che l'output vada ANCHE al nuovo stream.
        _writer.SetSecondaryOut(newOut);
    }
    public void SetError(TextWriter newError)
    {
        _writer.SetSecondaryError(newError);
    }

    public void Beep() { }

    public void Clear()
    {
        if (!_isFirstOutput)
        {
            // Salva lo schermo corrente PRIMA di pulire
            var snapshot = CaptureSnapshot("CLS");
            _screenLog.Add(snapshot);
        }
        _isFirstOutput = false;

        var emptyCell = new ScreenCell(' ', ForegroundColor, BackgroundColor);
        for (var r = 0; r < _height; r++)
        for (var c = 0; c < _width; c++)
            _buffer[r, c] = emptyCell;

        CursorLeft = 0;
        CursorTop = 0;
    }

    public void ResetColor()
    {
        BackgroundColor = Configuration.DefaultBgColor;
        ForegroundColor = Configuration.DefaultFgColor;
    }

    public void SetCursorPosition(int left, int top)
    {
        CursorLeft = Math.Clamp(left, 0, _width - 1);
        CursorTop = Math.Clamp(top, 0, _height - 1);
    }

    public void SetWindowPosition(int left, int top) { }
    public void SetWindowSize(int width, int height) { }
    public bool ProcessEvents() => false;
    public void Show() { }
    public void Present() { }
    public void SetVisibility(bool visible) { }
    public void SetCursorStyle(CursorStyle style) { }
    public void SetFont(ScreenConfig config) { }
    public void SetFont(string fontPath, int fontSize, SdlFontHinting? hinting = null) { }
    public void CopyBufferToClipboard() { }
    public void PasteFromClipboard() { }
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
    }
    public byte[][] GetActivePalette() => _activePalette;
    public string? GetActivePaletteName() => _activePaletteName;

    public ScreenCell[,] Get(ScreenRect rect)
    {
        var result = new ScreenCell[rect.h, rect.w];
        for (var r = 0; r < rect.h && r < _height; r++)
        for (var c = 0; c < rect.w && c < _width; c++)
            result[r, c] = _buffer[r, c];
        return result;
    }

    public void Put(int x, int y, ScreenCell[,] cells)
    {
        for (var r = 0; r < cells.GetLength(0) && y + r < _height; r++)
        for (var c = 0; c < cells.GetLength(1) && x + c < _width; c++)
            _buffer[y + r, x + c] = cells[r, c];
    }

    public void ClearRect(ScreenRect rect, ScreenCell? fillCell = null)
    {
        var cell = fillCell ?? new ScreenCell(' ', ForegroundColor, BackgroundColor);
        for (var r = 0; r < rect.h && rect.y + r < _height; r++)
        for (var c = 0; c < rect.w && rect.x + c < _width; c++)
            _buffer[rect.y + r, rect.x + c] = cell;
    }

    public void Dispose() { }

    // --- Metodi privati ---

    private void InitializeBuffer()
    {
        _buffer = new ScreenCell[_height, _width];
        var emptyCell = new ScreenCell(' ', ForegroundColor, BackgroundColor);
        for (var r = 0; r < _height; r++)
        for (var c = 0; c < _width; c++)
            _buffer[r, c] = emptyCell;
    }

    private ScreenSnapshot CaptureSnapshot(string label)
    {
        var cells = new ScreenCell[_height, _width];
        Array.Copy(_buffer, cells, _buffer.Length);
        return new ScreenSnapshot(label, cells, _width, _height);
    }

    // --- Writer interno ---

    /// <summary>
    /// TextWriter che scrive simultaneamente su:
    /// 1. Stream lineare (StringWriter del test runner)
    /// 2. Buffer screen (ScreenCell[,])
    /// </summary>
    private sealed class HeadlessWriter : TextWriter
    {
        private readonly HeadlessConsole _console;
        private TextWriter _secondaryOut = TextWriter.Null;
        private TextWriter _secondaryError = TextWriter.Null;

        public override Encoding Encoding => Console.OutputEncoding;

        public HeadlessWriter(HeadlessConsole console)
        {
            _console = console;
        }

        public void SetSecondaryOut(TextWriter writer)
        {
            _secondaryOut = writer ?? TextWriter.Null;
        }

        public void SetSecondaryError(TextWriter writer)
        {
            _secondaryError = writer ?? TextWriter.Null;
        }

        public override void Write(char value)
        {
            // Stream lineare
            _secondaryOut.Write(value);
            // Buffer screen
            WriteToBuffer(value);
        }

        public override void Write(string? value)
        {
            if (value == null) return;
            _secondaryOut.Write(value);
            foreach (var c in value)
                WriteToBuffer(c);
        }

        public override void WriteLine(string? value)
        {
            Write(value);
            WriteLine();
        }

        public override void WriteLine()
        {
            _secondaryOut.WriteLine();
            WriteToBuffer('\n');
        }

        public override void Flush()
        {
            _secondaryOut.Flush();
        }

        private void WriteToBuffer(char value)
        {
            var c = _console;

            // Traccia se è il primo output
            if (c._isFirstOutput && !char.IsWhiteSpace(value))
                c._isFirstOutput = false;

            switch (value)
            {
                case '\n': // Newline
                    c.CursorLeft = 0;
                    c.CursorTop++;
                    break;

                case '\r': // Carriage Return
                    c.CursorLeft = 0;
                    break;

                case '\b': // Backspace
                    if (c.CursorLeft > 0) c.CursorLeft--;
                    break;

                case '\t': // Tab
                    var tabStop = 8;
                    var spaces = tabStop - (c.CursorLeft % tabStop);
                    for (var i = 0; i < spaces; i++)
                        WriteToBuffer(' ');
                    break;

                default: // Carattere stampabile
                    if (c.CursorLeft >= c._width)
                    {
                        c.CursorLeft = 0;
                        c.CursorTop++;
                    }
                    if (c.CursorTop >= c._height)
                    {
                        ScrollUp();
                        c.CursorTop = c._height - 1;
                    }
                    if (c.CursorLeft < c._width && c.CursorTop < c._height)
                    {
                        c._buffer[c.CursorTop, c.CursorLeft] =
                            new ScreenCell(value, c.ForegroundColor, c.BackgroundColor);
                    }
                    c.CursorLeft++;
                    break;
            }

            // Scroll verticale se necessario (post-scrittura)
            if (c.CursorTop >= c._height)
            {
                ScrollUp();
                c.CursorTop = c._height - 1;
            }
        }

        private void ScrollUp()
        {
            var c = _console;
            var top = c._scrollBottom >= 0 ? c._scrollTop : 0;
            var bottom = c._scrollBottom >= 0 ? c._scrollBottom : c._height - 1;

            // Salva la riga superiore della regione di scroll
            var topLine = new StringBuilder(c._width);
            for (var col = 0; col < c._width; col++)
                topLine.Append(c._buffer[top, col].Character);
            c._scrollbackLines.Add(topLine.ToString().TrimEnd());

            // Shift solo le righe dentro la regione di scroll
            for (var r = top; r < bottom; r++)
            for (var col = 0; col < c._width; col++)
                c._buffer[r, col] = c._buffer[r + 1, col];

            // Pulisce l'ultima riga della regione
            var emptyCell = new ScreenCell(' ', c.ForegroundColor, c.BackgroundColor);
            for (var col = 0; col < c._width; col++)
                c._buffer[bottom, col] = emptyCell;
        }
    }
}
