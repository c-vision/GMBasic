// FILE: CommonLib.Screens.ScreenCell.cs

namespace CommonLib.Screens;

/// <summary>
/// Rappresenta una singola cella nel buffer di schermo di un terminale.
/// Supporta sia indici palette (ForegroundColor/BackgroundColor) che true-color RGB
/// (ForegroundRgb/BackgroundRgb) per backend di rendering diversi.
/// Record struct immutabile condivisa da tutte le implementazioni di IConsole e dal layer TUI.
/// </summary>
public readonly record struct ScreenCell
{
    public readonly char Character;
    public readonly int ForegroundColor;
    public readonly int BackgroundColor;
    public readonly ScreenCellAttributes Attributes;

    /// <summary>True-color RGB override per il primo piano (null = usa indice palette).</summary>
    public readonly CellColor? ForegroundRgb;

    /// <summary>True-color RGB override per lo sfondo (null = usa indice palette).</summary>
    public readonly CellColor? BackgroundRgb;

    public ScreenCell(char character, int foregroundColor, int backgroundColor, ScreenCellAttributes attributes = ScreenCellAttributes.None)
    {
        Character = character;
        ForegroundColor = foregroundColor;
        BackgroundColor = backgroundColor;
        Attributes = attributes;
        ForegroundRgb = null;
        BackgroundRgb = null;
    }

    public ScreenCell(char character, CellColor fg, CellColor bg, ScreenCellAttributes attributes = ScreenCellAttributes.None)
    {
        Character = character;
        ForegroundColor = (int)ConsoleColor.Gray; // fallback, il renderer userà ForegroundRgb
        BackgroundColor = (int)ConsoleColor.Black;
        Attributes = attributes;
        ForegroundRgb = fg;
        BackgroundRgb = bg;
    }

    public static readonly ScreenCell Empty = new(' ', (int)ConsoleColor.Gray, (int)ConsoleColor.Black);
}
