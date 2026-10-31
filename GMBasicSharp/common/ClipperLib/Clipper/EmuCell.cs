using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Stile di una cella dell'emulatore: colori di primo piano/sfondo
/// (null = colore di default) e attributi grafici SGR.
/// </summary>
public class EmuStyle
{
    public CellColor? Fg;
    public CellColor? Bg;
    public bool Bold;
    public bool Dim;
    public bool Italic;
    public bool Underline;
    public bool Reverse;
    public bool Blink;

    public EmuStyle Clone() => new EmuStyle
    {
        Fg = Fg,
        Bg = Bg,
        Bold = Bold,
        Dim = Dim,
        Italic = Italic,
        Underline = Underline,
        Reverse = Reverse,
        Blink = Blink,
    };
}

/// <summary>
/// Cella della griglia dell'emulatore: un carattere, il suo stile e i flag
/// per i caratteri a doppia larghezza (wide) e le celle di continuazione.
/// </summary>
public class EmuCell
{
    public char Char = ' ';
    public EmuStyle Style = new();
    public bool IsWide;
    public bool IsContinuation;
}
