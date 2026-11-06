using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>Palette del tema TUI. I colori vivono nel runtime C (`gmb_theme_named`):
/// questa classe e' un binding/proxy, non una seconda tabella di palette.</summary>
public class Theme
{
    public string Name { get; set; } = "Turbo";
    public CellColor DesktopBg { get; set; }
    public CellColor WindowBg { get; set; }
    public CellColor TitleFocus { get; set; }
    public CellColor TitleBlur { get; set; }
    public CellColor TitleFg { get; set; }
    public CellColor Border { get; set; }
    public CellColor CtrlFg { get; set; }
    public CellColor CloseFg { get; set; }
    public CellColor Text { get; set; }
    public CellColor Dim { get; set; }
    public CellColor Accent { get; set; }
    public CellColor ActiveBg { get; set; }
    public CellColor Shadow { get; set; }
    public char DesktopPattern { get; set; } = '░';

    private Theme() { }

    private static CellColor C(in NativeTheme.CColor c) => new() { R = c.r, G = c.g, B = c.b };

    public static Theme Named(string name)
    {
        var c = NativeTheme.gmb_theme_named(name ?? "turbovision");
        var t = new Theme
        {
            Name = string.IsNullOrWhiteSpace(name) ? "TurboVision" : name,
            DesktopBg = C(c.desktop_bg),
            WindowBg = C(c.window_bg),
            TitleFocus = C(c.title_focus),
            TitleBlur = C(c.title_blur),
            TitleFg = C(c.title_fg),
            Border = C(c.border),
            CtrlFg = C(c.ctrl_fg),
            CloseFg = C(c.close_fg),
            Text = C(c.text),
            Dim = C(c.dim),
            Accent = C(c.accent),
            ActiveBg = C(c.active_bg),
            Shadow = C(c.shadow),
        };
        var pat = c.desktop_pattern == IntPtr.Zero ? null : System.Runtime.InteropServices.Marshal.PtrToStringUTF8(c.desktop_pattern);
        t.DesktopPattern = string.IsNullOrEmpty(pat) ? ' ' : pat[0];
        return t;
    }

    public static Theme TurboVision() => Named("turbovision");
    public static Theme Clipper() => Named("clipper");
    public static Theme Turbo() => Named("turbo");   /* alias storico: ora = TurboVision */
    public static Theme QBasic() => Named("qbasic");
    public static Theme Midnight() => Named("midnight");
    public static Theme Nord() => Named("nord");
    public static Theme Dracula() => Named("dracula");
    public static Theme Monokai() => Named("monokai");
    public static Theme SolarizedDark() => Named("solarized");
    public static Theme RetroGreen() => Named("retrogreen");
    public static Theme Amber() => Named("amber");

    public static Theme Default() => Named("turbovision");
}
