using CommonLib.Screens;
namespace CommonLib.Consoles.Tui;

/// <summary>
/// Central definition of every ANSI/terminal escape sequence used by the library.
/// All escape codes must be referenced from here (never hardcoded in controls).
/// </summary>
public static class Ansi
{
    // Introducers
    public const string ESC = "\x1b";
    public const string CSI = "\x1b[";   // Control Sequence Introducer
    public const string OSC = "\x1b]";   // Operating System Command
    public const string ST = "\x1b\\";   // String Terminator (for OSC / APC)
    public const string APC = "\x1b_";   // Application Program Command introducer

    // SGR — reset & attributes
    public const string Reset = "\x1b[0m";
    public const string Bold = "\x1b[1m";
    public const string Dim = "\x1b[2m";
    public const string Italic = "\x1b[3m";
    public const string Underline = "\x1b[4m";
    public const string Blink = "\x1b[5m";
    public const string Reverse = "\x1b[7m";
    public const string Hidden = "\x1b[8m";
    public const string Strikethrough = "\x1b[9m";
    public const string NoBold = "\x1b[22m";
    public const string NoItalic = "\x1b[23m";
    public const string NoUnderline = "\x1b[24m";
    public const string NoBlink = "\x1b[25m";
    public const string NoReverse = "\x1b[27m";
    public const string NoHidden = "\x1b[28m";
    public const string NoStrikethrough = "\x1b[29m";

    // Cursor visibility
    public const string CursorShow = "\x1b[?25h";
    public const string CursorHide = "\x1b[?25l";

    // Screen / alt-screen
    public const string AltScreenEnter = "\x1b[?1049h";
    public const string AltScreenExit = "\x1b[?1049l";
    public const string ClearScreen = "\x1b[2J";
    public const string ClearLine = "\x1b[K";

    // Mouse
    public const string MouseTrackingOn = "\x1b[?1002h";  // button-event tracking (press + drag + release)
    public const string MouseTrackingOff = "\x1b[?1002l";
    public const string MouseMotionOn = "\x1b[?1003h";  // any-event tracking (press + release + motion)
    public const string MouseMotionOff = "\x1b[?1003l";
    public const string MouseSgrOn = "\x1b[?1006h";
    public const string MouseSgrOff = "\x1b[?1006l";
    public const string MousePassThroughOn = "\x1b]1000;?\x1b\\";
    public const string MousePassThroughOff = "\x1b]1000;0\x1b\\";
    public const string BracketedPasteOn = "\x1b[?2004h";
    public const string BracketedPasteOff = "\x1b[?2004l";

    /// <summary>Foreground SGR for a 24-bit RGB color.</summary>
    public static string FgRgb(byte r, byte g, byte b) => $"\x1b[38;2;{r};{g};{b}m";

    /// <summary>Background SGR for a 24-bit RGB color.</summary>
    public static string BgRgb(byte r, byte g, byte b) => $"\x1b[48;2;{r};{g};{b}m";

    /// <summary>Foreground SGR for an xterm 256-color index.</summary>
    public static string Fg256(int idx) => $"\x1b[38;5;{idx}m";

    /// <summary>Background SGR for an xterm 256-color index.</summary>
    public static string Bg256(int idx) => $"\x1b[48;5;{idx}m";

    /// <summary>Cursor to 1-based (col, row).</summary>
    public static string MoveTo(int col, int row) => $"\x1b[{row};{col}H";

    /// <summary>Cursor-home (top-left).</summary>
    public const string Home = "\x1b[H";

    /// <summary>Erase from cursor to end of screen.</summary>
    public const string EraseToEnd = "\x1b[0J";

    /// <summary>Erase from cursor to end of line.</summary>
    public const string EraseLineToEnd = "\x1b[0K";
}
