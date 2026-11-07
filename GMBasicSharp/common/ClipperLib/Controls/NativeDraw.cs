// FILE: ClipperLib/Controls/NativeDraw.cs
// P/Invoke al render dei controlli del runtime C (libgmb_gfx, gmb_tui_drawapi.c).
// Il disegno dei controlli vive SOLO in C: qui si passa lo stato del controllo C#
// alla funzione canonica `gmb_tui_draw_control`, che istanzia il widget reale,
// applica lo stato e scrive nel buffer condiviso (ScreenBuffer = gmb_screen_buffer_t).
// Direzione unica: C# -> C.
using System;
using CommonLib.Consoles.Tui;

namespace ClipperLib.Controls;

/// <summary>Binding del disegno nativo dei controlli (unica fonte: runtime C).</summary>
public static class NativeDraw
{
    // --- kind (gmb_tui_drawapi.h, GMB_DK_*) ---
    public const int None = 0;
    public const int Button = 1;
    public const int CheckBox = 2;
    public const int Toggle = 3;
    public const int Label = 4;
    public const int TextInput = 5;
    public const int ListBox = 6;
    public const int ComboBox = 7;
    public const int RadioGroup = 8;
    public const int Progress = 9;
    public const int ScrollBar = 10;
    public const int Tab = 11;
    public const int Panel = 12;
    public const int Spinner = 13;
    public const int Slider = 14;
    public const int StatusBar = 15;
    public const int Menu = 16;
    public const int TextArea = 17;
    public const int CodeArea = 18;
    public const int AlertBox = 19;
    public const int BigText = 20;
    public const int Numeric = 21;
    public const int HLine = 22;
    public const int VLine = 23;
    public const int StarRating = 24;
    public const int FileExplorer = 25;
    public const int FunctionKeyBar = 26;
    public const int Splitter = 27;
    public const int ContextMenu = 28;
    public const int ScrollView = 29;
    public const int Window = 30;
    public const int TBrowse = 31;

    // --- state (GMB_DS_*) ---
    public const int StFocused = 1 << 0;
    public const int StError = 1 << 1;
    public const int StUppercase = 1 << 2;
    public const int StChecked = 1 << 3;
    public const int StOpen = 1 << 4;
    public const int StReadOnly = 1 << 5;
    public const int StPassword = 1 << 6;

    /// <summary>Mappa PanelBorder C# → GMB_PB_* del runtime C (SINGLE=0, DOUBLE=1, ROUNDED=2, NONE=3).</summary>
    public static int CgPanelBorder(PanelBorder b) => b switch
    {
        PanelBorder.Double => 1,
        PanelBorder.Rounded => 2,
        PanelBorder.None => 3,
        _ => 0,
    };

    /// <summary>Disegno nativo per oggetti NON-Control (rect esplicito). Unica fonte: runtime C.</summary>
    public static bool Show(ScreenBuffer buf, int kind, int x, int y, int w, int h,
                            string? text = null, string? extra = null, int sel = -1,
                            int min = 0, int max = 0, double value = 0, int state = 0)
    {
        if (buf.Handle == IntPtr.Zero) return false;
        var r = new NativeControlDraw.Req
        {
            Kind = kind, X = x, Y = y, W = w, H = h,
            Sel = sel, Min = min, Max = max, Value = value,
            Text = text, Extra = extra, State = state
        };
        return NativeControlDraw.gmb_tui_draw_control(buf.Handle, ref r) != 0;
    }

    /// <summary>
    /// Disegna il controllo nel buffer tramite il runtime C. Ritorna false se il kind
    /// non è gestito (mai un fallback silenzioso: il chiamante può segnalarlo).
    /// </summary>
    public static bool Show(ScreenBuffer buf, int kind, Control c,
                            string? text = null, string? extra = null, int sel = -1,
                            int min = 0, int max = 0, double value = 0,
                            int extraState = 0, int? w = null, int? h = null)
    {
        if (buf.Handle == IntPtr.Zero) return false;
        var r = new NativeControlDraw.Req
        {
            Kind = kind,
            X = c.X, Y = c.Y,
            W = w ?? c.Width,
            H = h ?? c.Height,
            Sel = sel, Min = min, Max = max, Value = value,
            Text = text, Extra = extra
        };
        if (c.Focused) r.State |= StFocused;
        if (c.HasError) r.State |= StError;
        if (c.Uppercase) r.State |= StUppercase;
        r.State |= extraState;

        // Colori: solo se il chiamante li ha impostati ESPLICITAMENTE. Altrimenti
        // decide il runtime C (tema attivo + palette Turbo Vision per tipo).
        if (c.Style.Explicit)
        {
            var (fg, bg) = c.Style.Effective(c.Focused, c.HasError);
            r.HasFg = 1; r.HasBg = 1;
            r.FgRgb = (uint)((fg.R << 16) | (fg.G << 8) | fg.B);
            r.BgRgb = (uint)((bg.R << 16) | (bg.G << 8) | bg.B);
        }
        return NativeControlDraw.gmb_tui_draw_control(buf.Handle, ref r) != 0;
    }
}
