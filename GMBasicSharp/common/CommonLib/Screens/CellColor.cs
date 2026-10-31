// FILE: CommonLib.Screens.CellColor.cs

namespace CommonLib.Screens;

/// <summary>
/// Colore true-color RGB per una cella del terminale.
/// Supporta un flag IsDefault per indicare «nessun colore esplicito»
/// (utile per il rendering differenziale: il renderer eredita il default).
/// </summary>
public struct CellColor
{
    public byte R { get; set; }
    public byte G { get; set; }
    public byte B { get; set; }
    public bool IsDefault { get; set; }

    public static CellColor Black => new() { R = 0, G = 0, B = 0 };
    public static CellColor White => new() { R = 255, G = 255, B = 255 };
    public static CellColor Red => new() { R = 255, G = 0, B = 0 };
    public static CellColor Green => new() { R = 0, G = 255, B = 0 };
    public static CellColor Blue => new() { R = 0, G = 0, B = 255 };
    public static CellColor Yellow => new() { R = 255, G = 255, B = 0 };
    public static CellColor Cyan => new() { R = 0, G = 255, B = 255 };
    public static CellColor Magenta => new() { R = 255, G = 0, B = 255 };
    public static CellColor Grey => new() { R = 128, G = 128, B = 128 };
    public static CellColor DarkRed => new() { R = 128, G = 0, B = 0 };
    public static CellColor DarkGreen => new() { R = 0, G = 128, B = 0 };
    public static CellColor DarkBlue => new() { R = 0, G = 0, B = 128 };
    public static CellColor DarkYellow => new() { R = 128, G = 128, B = 0 };
    public static CellColor DarkCyan => new() { R = 0, G = 128, B = 128 };
    public static CellColor DarkMagenta => new() { R = 128, G = 0, B = 128 };

    public static CellColor Default => new() { IsDefault = true };

    public override readonly string ToString() =>
        IsDefault ? "default" : $"#{R:X2}{G:X2}{B:X2}";

    public override readonly bool Equals(object? obj) =>
        obj is CellColor c && this == c;

    public override readonly int GetHashCode() =>
        HashCode.Combine(R, G, B, IsDefault);

    public static bool operator ==(CellColor a, CellColor b) =>
        a.R == b.R && a.G == b.G && a.B == b.B && a.IsDefault == b.IsDefault;

    public static bool operator !=(CellColor a, CellColor b) => !(a == b);
}
