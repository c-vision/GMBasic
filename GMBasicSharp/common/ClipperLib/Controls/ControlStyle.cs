using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>
/// Visual style for a control. Centralizes every color role so controls never
/// hardcode CellColor literals. Color roles:
///  - Normal (no focus): Fg / Bg
///  - Focus:            FgFocus / BgFocus
///  - Error:            FgError / BgError
///  - Selected/Header:  SelFg / SelBg
/// </summary>
public class ControlStyle
{
    public CellColor? Fg { get; set; }
    public CellColor? Bg { get; set; }
    public CellColor? FgFocus { get; set; }
    public CellColor? BgFocus { get; set; }
    public CellColor? FgError { get; set; }
    public CellColor? BgError { get; set; }
    public CellColor? SelFg { get; set; }
    public CellColor? SelBg { get; set; }
    public char FillChar { get; set; } = ' ';

    /// <summary>Tema globale attivo (letto dal runtime C).</summary>
    public static Theme ActiveTheme => Theme.Named(NativeTheme.UiThemeName());

    /// <summary>Imposta il tema globale nel runtime C (via unica, come la grafica).</summary>
    public static void SetTheme(Theme theme) => NativeTheme.gmb_ui_set_theme_name(string.IsNullOrWhiteSpace(theme.Name) ? "turbovision" : theme.Name);

    public static ControlStyle Default() =>
        NativeTheme.gmb_ui_theme_is_set() == 0
            ? new()
            {
                Fg = new CellColor { R = 200, G = 208, B = 220 },
                Bg = CellColor.Blue,
                FgFocus = CellColor.Black,
                BgFocus = CellColor.Yellow,
                FgError = CellColor.White,
                BgError = CellColor.Red,
                SelFg = CellColor.Black,
                SelBg = CellColor.Cyan,
                FillChar = ' '
            }
            : FromTheme(ActiveTheme);

    /// <summary>Build a ControlStyle from a Theme palette (so themes apply to controls).</summary>
    public static ControlStyle FromTheme(Theme t) => new()
    {
        Fg = t.Text,
        Bg = t.WindowBg,
        FgFocus = t.WindowBg,
        BgFocus = t.Accent,
        FgError = CellColor.White,
        BgError = CellColor.Red,
        SelFg = t.WindowBg,
        SelBg = t.ActiveBg,
        FillChar = ' '
    };

    public ControlStyle Clone() => new()
    {
        Fg = Fg, Bg = Bg,
        FgFocus = FgFocus, BgFocus = BgFocus,
        FgError = FgError, BgError = BgError,
        SelFg = SelFg, SelBg = SelBg,
        FillChar = FillChar
    };

    public ControlStyle WithColors(CellColor fg, CellColor bg)
    {
        Fg = fg; Bg = bg; return this;
    }

    public ControlStyle WithFocusColors(CellColor fg, CellColor bg)
    {
        FgFocus = fg; BgFocus = bg; return this;
    }

    public ControlStyle WithErrorColors(CellColor fg, CellColor bg)
    {
        FgError = fg; BgError = bg; return this;
    }

    public ControlStyle WithSelectedColors(CellColor fg, CellColor bg)
    {
        SelFg = fg; SelBg = bg; return this;
    }

    public ControlStyle WithFill(char fill)
    {
        FillChar = fill; return this;
    }

    /// <summary>Resolve the (fg, bg) pair for the given state.</summary>
    public (CellColor fg, CellColor bg) Effective(bool focused = false, bool error = false, bool selected = false)
    {
        CellColor fg, bg;
        if (error)
        {
            fg = FgError ?? Fg ?? new CellColor { R = 200, G = 208, B = 220 };
            bg = BgError ?? Bg ?? CellColor.Blue;
        }
        else if (selected)
        {
            fg = SelFg ?? Fg ?? new CellColor { R = 200, G = 208, B = 220 };
            bg = SelBg ?? Bg ?? CellColor.Blue;
        }
        else if (focused)
        {
            fg = FgFocus ?? Fg ?? new CellColor { R = 200, G = 208, B = 220 };
            bg = BgFocus ?? Bg ?? CellColor.Blue;
        }
        else
        {
            fg = Fg ?? new CellColor { R = 200, G = 208, B = 220 };
            bg = Bg ?? CellColor.Blue;
        }
        return (fg, bg);
    }
}
