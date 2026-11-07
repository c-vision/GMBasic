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
    private CellColor? _fg, _bg, _fgFocus, _bgFocus, _fgError, _bgError, _selFg, _selBg;

    // I setter marcano il colore come ESPLICITO (oggetto inizializzato a mano o With*):
    // solo in quel caso il render nativo lo applica. Gli stili derivati dal tema
    // (Default/FromTheme) azzerano il flag: decide il runtime C.
    public CellColor? Fg
    {
        get => _fg;
        set { _fg = value; if (value != null) Explicit = true; }
    }
    public CellColor? Bg
    {
        get => _bg;
        set { _bg = value; if (value != null) Explicit = true; }
    }
    public CellColor? FgFocus
    {
        get => _fgFocus;
        set { _fgFocus = value; if (value != null) Explicit = true; }
    }
    public CellColor? BgFocus
    {
        get => _bgFocus;
        set { _bgFocus = value; if (value != null) Explicit = true; }
    }
    public CellColor? FgError
    {
        get => _fgError;
        set { _fgError = value; if (value != null) Explicit = true; }
    }
    public CellColor? BgError
    {
        get => _bgError;
        set { _bgError = value; if (value != null) Explicit = true; }
    }
    public CellColor? SelFg
    {
        get => _selFg;
        set { _selFg = value; if (value != null) Explicit = true; }
    }
    public CellColor? SelBg
    {
        get => _selBg;
        set { _selBg = value; if (value != null) Explicit = true; }
    }
    public char FillChar { get; set; } = ' ';

    /// <summary>True se il chiamante ha impostato colori ESPLICITI (WithColors/...).
    /// In quel caso il render nativo li applica; altrimenti decide il tema del runtime C.</summary>
    public bool Explicit { get; private set; }

    /// <summary>Tema globale attivo (letto dal runtime C).</summary>
    public static Theme ActiveTheme => Theme.Named(NativeTheme.UiThemeName());

    /// <summary>Imposta il tema globale nel runtime C (via unica, come la grafica).</summary>
    public static void SetTheme(Theme theme) => NativeTheme.gmb_ui_set_theme_name(string.IsNullOrWhiteSpace(theme.Name) ? "turbovision" : theme.Name);

    public static ControlStyle Default()
    {
        ControlStyle s;
        if (NativeTheme.gmb_ui_theme_is_set() == 0)
            s = new()
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
            };
        else
            s = FromTheme(ActiveTheme);
        s.Explicit = false;   /* derivato dal tema: decide il runtime C */
        return s;
    }

    /// <summary>Build a ControlStyle from a Theme palette (so themes apply to controls).</summary>
    public static ControlStyle FromTheme(Theme t)
    {
        var s = new ControlStyle
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
        s.Explicit = false;
        return s;
    }

    public ControlStyle Clone()
    {
        var s = new ControlStyle
        {
            Fg = Fg, Bg = Bg,
            FgFocus = FgFocus, BgFocus = BgFocus,
            FgError = FgError, BgError = BgError,
            SelFg = SelFg, SelBg = SelBg,
            FillChar = FillChar
        };
        s.Explicit = Explicit;
        return s;
    }

    public ControlStyle WithColors(CellColor fg, CellColor bg)
    {
        Fg = fg; Bg = bg; Explicit = true; return this;
    }

    public ControlStyle WithFocusColors(CellColor fg, CellColor bg)
    {
        FgFocus = fg; BgFocus = bg; Explicit = true; return this;
    }

    public ControlStyle WithErrorColors(CellColor fg, CellColor bg)
    {
        FgError = fg; BgError = bg; Explicit = true; return this;
    }

    public ControlStyle WithSelectedColors(CellColor fg, CellColor bg)
    {
        SelFg = fg; SelBg = bg; Explicit = true; return this;
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
