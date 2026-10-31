using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Globalization;

namespace ClipperLib.Clipper;

/// <summary>
/// Emulatore di terminale VT (OS-agnostico, TTY-free): consuma le
/// <see cref="Sequence"/> prodotte dal <see cref="Parser"/> esistente in
/// modalita *emulator* e mantiene la <see cref="EmuGrid"/>, il cursore, la
/// regione di scroll e le modalita. Non fa I/O: si alimenta via
/// <see cref="Feed(byte[])"/> e si renderizza con <c>EmuRender</c>.
/// </summary>
public class Emulator
{
    /// <summary>Griglia corrente (schermo + stato di basso livello).</summary>
    public EmuGrid Grid { get; }

    /// <summary>Autowrap (modalita DEC ?7).</summary>
    public bool AutoWrap
    {
        get => Grid.AutoWrap;
        set => Grid.AutoWrap = value;
    }

    /// <summary>Origin mode (modalita DEC ?6): gli indici di cursore sono relativi alla scroll region.</summary>
    public bool OriginMode
    {
        get => Grid.OriginMode;
        set => Grid.OriginMode = value;
    }

    /// <summary>Il cursore e visibile? (modalita DEC ?25).</summary>
    public bool CursorVisible => Grid.CursorVisible;

    /// <summary>Posizione del cursore (0-based).</summary>
    public (int X, int Y) Cursor() => (Grid.CursorX, Grid.CursorY);

    public int Cols => Grid.Cols;
    public int Rows => Grid.Rows;

    private readonly Parser _parser;
    private EmuStyle _curStyle = new();
    private EmuStyle _savedStyle = new();

    public Emulator(int cols, int rows)
    {
        Grid = new EmuGrid(cols, rows);
        _parser = Parser.CreateEmulator();
    }

    /// <summary>Alimenta l'emulatore con un buffer di byte UTF-8 (es. da una PTY).</summary>
    public void Feed(byte[] data) => Feed((ReadOnlySpan<byte>)data);

    /// <summary>Alimenta l'emulatore con uno span di byte UTF-8.</summary>
    public void Feed(ReadOnlySpan<byte> data)
    {
        _parser.Advance(System.Text.Encoding.UTF8.GetString(data.ToArray()), false);
        foreach (var seq in _parser.Drain())
            Process(seq);
    }

    /// <summary>Processa una singola <see cref="Sequence"/> gia decodificata.</summary>
    public void Process(Sequence seq)
    {
        switch (seq)
        {
            case SeqPrint(var ch):
                Print(ch);
                break;
            case SeqExecute(var b):
                Execute(b);
                break;
            case SeqSgr(var changes):
                ApplySgr(changes);
                break;
            case SeqCursorUp(var n):
                Grid.CursorY = Math.Max(RegionMinY(), Grid.CursorY - n);
                break;
            case SeqCursorDown(var n):
                Grid.CursorY = Math.Min(Grid.CursorY + n, RegionMaxY());
                break;
            case SeqCursorForward(var n):
                Grid.CursorX = Math.Min(Grid.CursorX + n, Cols - 1);
                break;
            case SeqCursorBackward(var n):
                Grid.CursorX = Math.Max(Grid.CursorX - n, 0);
                break;
            case SeqCursorNextLine(var n):
                Grid.CursorX = 0;
                Grid.CursorY = Math.Min(Grid.CursorY + n, RegionMaxY());
                break;
            case SeqCursorPrevLine(var n):
                Grid.CursorX = 0;
                Grid.CursorY = Math.Max(Grid.CursorY - n, RegionMinY());
                break;
            case SeqCursorColumn(var n):
                Grid.CursorX = Math.Min(Math.Max(n - 1, 0), Cols - 1);
                break;
            case SeqCursorLine(var n):
                Grid.CursorY = Math.Min(Math.Max(n - 1, 0), RegionMaxY());
                break;
            case SeqCursorPosition(var x, var y):
                Grid.CursorX = Math.Min(Math.Max(x - 1, 0), Cols - 1);
                Grid.CursorY = Math.Min(Math.Max(y - 1, 0), RegionMaxY());
                break;
            case SeqEraseInDisplay(var mode):
                EraseDisplay(mode);
                break;
            case SeqEraseInLine(var mode):
                EraseLine(mode);
                break;
            case SeqScrollUp(var n):
                Grid.ScrollUp(Grid.ScrollTop, Grid.ScrollBottom, n);
                break;
            case SeqScrollDown(var n):
                Grid.ScrollDown(Grid.ScrollTop, Grid.ScrollBottom, n);
                break;
            case SeqInsertLines(var n):
                Grid.ScrollDown(Math.Max(Grid.ScrollTop, Grid.CursorY), Grid.ScrollBottom, n);
                break;
            case SeqDeleteLines(var n):
                Grid.ScrollUp(Math.Max(Grid.ScrollTop, Grid.CursorY), Grid.ScrollBottom, n);
                break;
            case SeqInsertChars(var n):
                Grid.InsertChars(Grid.CursorY, Grid.CursorX, n);
                break;
            case SeqDeleteChars(var n):
                Grid.DeleteChars(Grid.CursorY, Grid.CursorX, n);
                break;
            case SeqSetScrollRegion(var top, var bottom):
                Grid.SetScrollRegion(top, bottom);
                break;
            case SeqSaveCursor:
                Grid.SavedStyle = _curStyle.Clone();
                Grid.SaveCursor();
                break;
            case SeqRestoreCursor:
                Grid.RestoreCursor();
                _curStyle = Grid.SavedStyle.Clone();
                break;
            case SeqSetMode(var m):
                SetMode(m, true);
                break;
            case SeqResetMode(var m):
                SetMode(m, false);
                break;
            case SeqSoftReset:
                SoftReset();
                break;
            // Input / OSC / window: ignorati dall'emulatore di output.
            case SeqKey:
            case SeqMouse:
            case SeqFocusIn:
            case SeqFocusOut:
            case SeqPaste:
            case SeqOsc:
            case SeqWindowOp:
                break;
        }
    }

    private int RegionMinY() => OriginMode ? Grid.ScrollTop : 0;
    private int RegionMaxY() => OriginMode ? Grid.ScrollBottom : Rows - 1;

    private void Print(char ch)
    {
        var w = CharWidth(ch);
        if (w == 0) return; // combining: non avanza il cursore

        if (w == 1)
        {
            if (Grid.CursorX < Cols && Grid.CursorY < Rows)
                Grid.SetCell(Grid.CursorX, Grid.CursorY, ch, _curStyle);
            Grid.CursorX++;
            if (Grid.CursorX >= Cols)
            {
                if (AutoWrap) { Grid.CursorX = 0; Grid.NewLine(); }
                else Grid.CursorX = Cols - 1;
            }
            return;
        }

        // w >= 2: carattere double-width; la seconda cella e una continuazione.
        if (Grid.CursorX >= Cols - 1)
        {
            Grid.CursorX = 0;
            Grid.NewLine();
        }
        if (Grid.CursorX < Cols && Grid.CursorY < Rows)
        {
            Grid.SetCell(Grid.CursorX, Grid.CursorY, ch, _curStyle, wide: true);
            if (Grid.CursorX + 1 < Cols)
                Grid.SetCell(Grid.CursorX + 1, Grid.CursorY, ' ', _curStyle, cont: true);
        }
        Grid.CursorX += 2;
        if (AutoWrap && Grid.CursorX >= Cols)
        {
            Grid.CursorX = 0;
            Grid.NewLine();
        }
        else if (Grid.CursorX >= Cols)
        {
            Grid.CursorX = Cols - 1;
        }
    }

    private void Execute(byte b)
    {
        switch (b)
        {
            case 0x07: // BEL
                break;
            case 0x08: // BS
                if (Grid.CursorX > 0) Grid.CursorX--;
                break;
            case 0x09: // TAB
                var next = (Grid.CursorX / 8 + 1) * 8;
                Grid.CursorX = Math.Min(next, Cols - 1);
                break;
            case 0x0A: // LF
            case 0x0B: // VT
            case 0x0C: // FF
                Grid.NewLine();
                break;
            case 0x0D: // CR
                Grid.CursorX = 0;
                break;
        }
    }

    private void EraseDisplay(byte mode)
    {
        switch (mode)
        {
            case 0:
                Grid.ClearRect(Grid.CursorX, Grid.CursorY, Cols - 1, Grid.ScrollBottom);
                break;
            case 1:
                Grid.ClearRect(0, Grid.ScrollTop, Grid.CursorX, Grid.CursorY);
                break;
            case 2:
            case 3:
                Grid.ClearRect(0, Grid.ScrollTop, Cols - 1, Grid.ScrollBottom);
                break;
        }
    }

    private void EraseLine(byte mode)
    {
        switch (mode)
        {
            case 0:
                Grid.ClearRect(Grid.CursorX, Grid.CursorY, Cols - 1, Grid.CursorY);
                break;
            case 1:
                Grid.ClearRect(0, Grid.CursorY, Grid.CursorX, Grid.CursorY);
                break;
            case 2:
                Grid.ClearRect(0, Grid.CursorY, Cols - 1, Grid.CursorY);
                break;
        }
    }

    private void SetMode(TerminalMode m, bool on)
    {
        switch (m)
        {
            case TerminalMode.AutoWrap:
                AutoWrap = on;
                break;
            case TerminalMode.CursorVisible:
                Grid.CursorVisible = on;
                break;
            case TerminalMode.Origin:
                OriginMode = on;
                break;
            case TerminalMode.AltScreen:
                Grid.SwapAltScreen(on);
                break;
        }
    }

    private void SoftReset()
    {
        _curStyle = new EmuStyle();
        AutoWrap = true;
        Grid.CursorVisible = true;
        OriginMode = false;
        Grid.CursorX = 0;
        Grid.CursorY = 0;
        Grid.ScrollTop = 0;
        Grid.ScrollBottom = Rows - 1;
    }

    private void ApplySgr(System.Collections.Generic.IReadOnlyList<SgrAttr> changes)
    {
        foreach (var c in changes)
        {
            switch (c)
            {
                case SgrReset:
                    _curStyle = new EmuStyle();
                    break;
                case SgrBold:
                    _curStyle.Bold = true;
                    break;
                case SgrDim:
                    _curStyle.Dim = true;
                    break;
                case SgrItalic:
                    _curStyle.Italic = true;
                    break;
                case SgrUnderline:
                    _curStyle.Underline = true;
                    break;
                case SgrBlink:
                case SgrRapidBlink:
                    _curStyle.Blink = true;
                    break;
                case SgrReverse:
                    _curStyle.Reverse = true;
                    break;
                case SgrNotBoldOrDim:
                    _curStyle.Bold = false;
                    _curStyle.Dim = false;
                    break;
                case SgrNotItalic:
                    _curStyle.Italic = false;
                    break;
                case SgrNotUnderline:
                    _curStyle.Underline = false;
                    break;
                case SgrNotBlink:
                    _curStyle.Blink = false;
                    break;
                case SgrNotReverse:
                    _curStyle.Reverse = false;
                    break;
                case SgrFg(var fgCol1):
                    _curStyle.Fg = ToCell(fgCol1);
                    break;
                case SgrFgExtended(var fgCol2):
                    _curStyle.Fg = ToCell(fgCol2);
                    break;
                case SgrDefaultFg:
                    _curStyle.Fg = null;
                    break;
                case SgrBg(var bgCol1):
                    _curStyle.Bg = ToCell(bgCol1);
                    break;
                case SgrBgExtended(var bgCol2):
                    _curStyle.Bg = ToCell(bgCol2);
                    break;
                case SgrDefaultBg:
                    _curStyle.Bg = null;
                    break;
            }
        }
    }

    private static CellColor? ToCell(SgrColor c) => c switch
    {
        SgrColorDefault => null,
        SgrColorIndexed i => IndexedToCell(i.Index),
        SgrColorRgb r => new CellColor { R = r.R, G = r.G, B = r.B },
        _ => null,
    };

    private static CellColor IndexedToCell(byte i)
    {
        if (i < 16)
            return i switch
            {
                0 => CellColor.Black, 1 => CellColor.Red, 2 => CellColor.Green, 3 => CellColor.Yellow,
                4 => CellColor.Blue, 5 => CellColor.Magenta, 6 => CellColor.Cyan, 7 => CellColor.White,
                _ => CellColor.Grey,
            };
        if (i <= 231)
        {
            var n = i - 16;
            int r = n / 36, g = n / 6 % 6, b = n % 6;
            byte V(int t) => t == 0 ? (byte)0 : (byte)(55 + 40 * t);
            return new CellColor { R = V(r), G = V(g), B = V(b) };
        }
        var t = i - 232;
        var v = (byte)(8 + 10 * t);
        return new CellColor { R = v, G = v, B = v };
    }

    /// <summary>Larghezza di visualizzazione di un carattere (CJK = 2, combining = 0, altro = 1).</summary>
    internal static int CharWidth(char ch)
    {
        if (CharUnicodeInfo.GetUnicodeCategory(ch) == UnicodeCategory.NonSpacingMark)
            return 0;
        return IsWide(ch) ? 2 : 1;
    }

    private static bool IsWide(char c) => c switch
    {
        >= '\u1100' and <= '\u115F' => true,
        >= '\u2E80' and <= '\u303E' => true,
        >= '\u3041' and <= '\u33FF' => true,
        >= '\u3400' and <= '\u4DBF' => true,
        >= '\u4E00' and <= '\u9FFF' => true,
        >= '\uA000' and <= '\uA4CF' => true,
        >= '\uAC00' and <= '\uD7A3' => true,
        >= '\uF900' and <= '\uFAFF' => true,
        >= '\uFE30' and <= '\uFE4F' => true,
        >= '\uFF00' and <= '\uFF60' => true,
        >= '\uFFE0' and <= '\uFFE6' => true,
        '\u2329' or '\u232A' => true,
        _ => false,
    };
}
