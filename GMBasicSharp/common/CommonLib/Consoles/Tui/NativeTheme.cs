// FILE: CommonLib/Consoles/Tui/NativeTheme.cs
// Binding P/Invoke del tema TUI: la palette vive nel runtime C (gmb_theme_t, gmb_theme_named).
// Il vecchio Theme.cs teneva 9 palette duplicate in C#: ora e' un proxy su questa struct.
using System;
using System.Runtime.InteropServices;

namespace CommonLib.Consoles.Tui;

public static class NativeTheme
{
    private const string Lib = "gmb_gfx";

    [StructLayout(LayoutKind.Sequential)]
    public struct CColor
    {
        public byte r, g, b;
        [MarshalAs(UnmanagedType.I1)] public bool is_default;
    }

    // Specchio esatto di gmb_theme_t (include/gmb_tui_widgets.h).
    [StructLayout(LayoutKind.Sequential)]
    public struct CTheme
    {
        public CColor desktop_bg, window_bg, title_focus, title_blur, title_fg, border,
            ctrl_fg, close_fg, text, dim, accent, active_bg, shadow;
        public CColor cal_today_bg, cal_today_fg, cal_sel_bg, cal_sel_fg, head_bg, head_fg;
        public CColor editor_bg, editor_fg, editor_frame, menu_bg, menu_fg, dlg_bg, dlg_fg,
            stat_bg, stat_fg;
        public IntPtr desktop_pattern;
    }

    [DllImport(Lib, CharSet = CharSet.Ansi)]
    public static extern CTheme gmb_theme_named([MarshalAs(UnmanagedType.LPStr)] string name);

    [DllImport(Lib, CharSet = CharSet.Ansi)]
    public static extern void gmb_ui_set_theme_name([MarshalAs(UnmanagedType.LPStr)] string name);

    public static CColor Default() => new() { r = 170, g = 170, b = 170, is_default = false };

    [DllImport(Lib, CharSet = CharSet.Ansi)]
    public static extern IntPtr gmb_ui_theme_name();

    [DllImport(Lib)]
    public static extern int gmb_ui_theme_is_set();

    public static string UiThemeName()
    {
        var p = gmb_ui_theme_name();
        return p == IntPtr.Zero ? "turbo" : (System.Runtime.InteropServices.Marshal.PtrToStringAnsi(p) ?? "turbo");
    }
}
