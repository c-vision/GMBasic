using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Controls;
namespace ClipperLib.Clipper;

public class ClipperState
{
    public ScreenBuffer Buffer { get; set; }
    public int Row { get; set; }
    public int Col { get; set; }
    public string ColorSpec { get; set; } = "W/N";
    public CellColor Fg { get; set; } = CellColor.White;
    public CellColor Bg { get; set; } = CellColor.Black;
    public Queue<byte> Typeahead { get; set; } = new();
    public int LastKey { get; set; }
    public int ReadKey { get; set; }
    public int MouseX { get; set; }
    public int MouseY { get; set; }
    public bool Updated { get; set; }
    public string? ReadVar { get; set; }
    public int? GetActive { get; set; }
    public List<Get> Gets { get; set; } = [];
    public Dictionary<int, Action> SetKeys { get; set; } = new();

    public Func<int>? KeySource { get; set; }

    /// <summary>Tasto prelevato da <see cref="KeySource"/>/typeahead da NEXTKEY() e in attesa di INKEY().</summary>
    public int? PendingKey { get; set; }

    public Action<ScreenBuffer>? RenderHook { get; set; }

    public bool BlinkOn { get; set; } = true;
    public bool BlinkEnabled { get; set; } = true;
    public bool CursorVisible { get; set; } = true;

    // ---- SET settings (Clipper 5.3) ----

    public bool SetConfirm;                          // SET CONFIRM
    public bool SetWrap;                             // SET WRAP
    public string SetDelimiters = "";                // SET DELIMITERS (default: nessun delimitatore)
    public bool SetEscape = true;                    // SET ESCAPE
    public bool SetBell = true;                      // SET BELL
    public Theme? CurrentTheme;                      // SET THEME TO
    public int SetTypeahead = 16;                    // SET TYPEAHEAD
    public bool SetIntensity = true;                 // SET INTENSITY
    public string SetDateFormat = "mm-dd-yyyy";      // SET DATE FORMAT
    public bool SetCentury;                          // SET CENTURY
    public int SetEpoch = 1900;                      // SET EPOCH
    public int SetDecimals = -1;                     // SET DECIMALS (-1 = mai impostato: formato legacy G15)
    public bool SetFixed;                            // SET FIXED

    // ---- Message / Prompt ----

    public int? MessageRow;                          // SET MESSAGE TO n
    public string MessageText = "";                  // testo corrente (da @...PROMPT)
    public int MessageCol;                           // colonna a cui mostrare il messaggio

    // ---- Printer (virtuale, in memoria) ----

    public bool PrinterOn;                           // SET PRINTER ON/OFF
    public string PrinterDevice = "LPT1";            // SET DEVICE
    public System.Text.StringBuilder PrinterBuffer = new();
    public int PrinterRow;
    public int PrinterCol;

    // ---- Mouse (Clipper 5.3) ----

    public bool MousePresent;
    public int MouseRow;                             // MROW()
    public int MouseCol;                             // MCOL()
    public bool[] MouseDown = new bool[3];           // tasto attualmente premuto
    public bool[] MousePressed = new bool[3];        // appena premuto (ultimo evento)
    public bool[] MouseReleased = new bool[3];       // appena rilasciato (ultimo evento)
    public bool[] MouseShift = new bool[3];
    public bool[] MouseCtrl = new bool[3];
    public bool MouseMoved;                          // spostato mentre un tasto è premuto
    public bool MouseCursorVisible = true;
    public string MouseCursorShape = "arrow";

    // ---- Mouse: eventi recenti (per le funzioni BASIC MOUSE.*) ----

    public int MouseWheelDelta;                      // ultimo scroll (positivo = up, negativo = down)
    public bool MouseDoubleClicked;                  // l'ultimo evento è stato un doppio click
    public int LastClickX;                           // coordinate dell'ultimo click
    public int LastClickY;

    // ---- Screen stack (SAVESCREEN/RESTSCREEN) ----

    public System.Collections.Generic.Stack<string> ScreenStack = new();

    // ---- READ exit key ----

    public int ReadExitKey;                          // READEXIT()

    public ClipperState(int width = 80, int height = 24)
    {
        Buffer = new ScreenBuffer(width, height);
    }

    public void ClearBuffer()
    {
        Buffer.Clear(Fg, Bg);
    }

    public void InvokeRenderHook()
    {
        RenderHook?.Invoke(Buffer);
    }

    public static ClipperState? ParseColorToken(string tok, out bool bright)
    {
        bright = false;
        return null; // static utility, handled in ColorParser
    }
}

public static class ColorParser
{
    public static (CellColor, bool) ParseToken(string tok)
    {
        var t = tok.Trim().ToUpperInvariant();
        var bright = t.EndsWith('+');
        var core = bright ? t[..^1] : t;

        if (string.IsNullOrEmpty(core) || core == "N")
            return bright ? (CellColor.Grey, bright) : (CellColor.Black, bright);
        if (core == "W")
            return (CellColor.White, bright);

        var r = core.Contains('R');
        var g = core.Contains('G');
        var b = core.Contains('B');

        if (r && g && b) return bright ? (CellColor.White, bright) : (CellColor.Grey, bright);
        if (r && g) return bright ? (CellColor.Yellow, bright) : (CellColor.DarkYellow, bright);
        if (r && b) return bright ? (CellColor.Magenta, bright) : (CellColor.DarkMagenta, bright);
        if (g && b) return bright ? (CellColor.Cyan, bright) : (CellColor.DarkCyan, bright);
        if (r) return bright ? (CellColor.Red, bright) : (CellColor.DarkRed, bright);
        if (g) return bright ? (CellColor.Green, bright) : (CellColor.DarkGreen, bright);
        if (b) return bright ? (CellColor.Blue, bright) : (CellColor.DarkBlue, bright);

        return (CellColor.Default, bright);
    }

    public static (CellColor fg, CellColor bg) ParsePair(string spec)
    {
        var parts = spec.Split('/');
        var fgTok = parts.Length > 0 ? parts[0].Trim() : "W";
        var bgTok = parts.Length > 1 ? parts[1].Trim() : "N";
        var (fg, _) = ParseToken(fgTok);
        var (bg, _) = ParseToken(bgTok);
        return (fg, bg);
    }
}
