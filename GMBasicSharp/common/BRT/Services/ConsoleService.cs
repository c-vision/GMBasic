// FILE: Basic.Runtime.Services.ConsoleService.cs

namespace Basic.Runtime.Services;

/// <summary>
/// Implementazione di IConsoleService basata su System.Console.
/// </summary>
public sealed class ConsoleService : IConsoleService
{
    public void Clear() => System.Console.Clear();
    public void Print(string? text) => System.Console.Write(text);
    public void PrintLine(string? text) => System.Console.WriteLine(text);
    public void PrintNewLine() => System.Console.WriteLine();

    public void SetForegroundColor(int colorIndex)
    {
        if (colorIndex >= 0 && colorIndex <= 15)
            System.Console.ForegroundColor = (ConsoleColor)colorIndex;
    }

    public void SetBackgroundColor(int colorIndex)
    {
        if (colorIndex >= 0 && colorIndex <= 15)
            System.Console.BackgroundColor = (ConsoleColor)colorIndex;
    }

    public void ResetColor() => System.Console.ResetColor();

    public void SetCursorPosition(int left, int top)
    {
        try { System.Console.SetCursorPosition(left, top); } catch { }
    }

    public void Beep() => System.Console.Beep();

    public int CursorLeft
    {
        get => System.Console.CursorLeft;
        set => System.Console.CursorLeft = value;
    }

    public int CursorTop
    {
        get => System.Console.CursorTop;
        set => System.Console.CursorTop = value;
    }

    public int WindowWidth => System.Console.WindowWidth;
    public int WindowHeight => System.Console.WindowHeight;

    public string Title
    {
        get => System.OperatingSystem.IsWindows() ? System.Console.Title : "";
        set { if (System.OperatingSystem.IsWindows()) System.Console.Title = value; }
    }

    public void SetPaletteColor(int index, int r, int g, int b) { }
    public void SetActivePalette(byte[][] palette) { }
    public byte[][] GetActivePalette() => CommonLib.Screens.ScreenConfig.Palettes.GetVGA16Palette();
    public void SetScrollRegion(int top, int bottom) { }
    // ALERT/CONFIRM: via UNICA = il controllo C (gmb_console_alert/confirm → AlertBox via
    // runtime condiviso, che si rende su TUI/SDLConsole/grafica secondo il contesto attivo).
    // C#→C via P/Invoke (direzione ammessa). Fallback su System.Console solo se la dylib
    // non e' disponibile (headless/assente).
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_alert",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern double GmbConsoleAlert(string msg);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_confirm",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern double GmbConsoleConfirm(string msg);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_menu",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern long GmbConsoleMenu(string title, string itemsCsv);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_input",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern IntPtr GmbConsoleInput(string prompt);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_toggle",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern long GmbConsoleToggle(string title, string prompt, int initial);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_slider",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern double GmbConsoleSlider(string title, double mn, double mx, double initial);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_spinner",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern long GmbConsoleSpinner(string title, string itemsCsv);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_filedialog",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern IntPtr GmbConsoleFileDialog(string title, string pattern, int save);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_colorpicker",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern IntPtr GmbConsoleColorPicker(string title, string def);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_datepicker",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern IntPtr GmbConsoleDatePicker(string title, string def);
    [System.Runtime.InteropServices.DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_msgbox",
        CallingConvention = System.Runtime.InteropServices.CallingConvention.Cdecl)]
    private static extern void GmbConsoleMsgBox(string title, string message);

    private static string? PtrStr(IntPtr p) => p == IntPtr.Zero ? null : System.Runtime.InteropServices.Marshal.PtrToStringAnsi(p);

    public void Alert(string message, string title)
    {
        try { CGfxNative.EnsureResolver(); GmbConsoleAlert(message ?? string.Empty); }
        catch { System.Console.WriteLine($"[ALERT] {title}: {message}"); }
    }

    public string? GetInput(string prompt, string? mask)
    {
        try { CGfxNative.EnsureResolver(); var p = GmbConsoleInput(prompt ?? string.Empty); return p == IntPtr.Zero ? null : System.Runtime.InteropServices.Marshal.PtrToStringAnsi(p); }
        catch { System.Console.Write(prompt); return System.Console.ReadLine(); }
    }
    public string? GetPassword(string prompt)
    {
        try { CGfxNative.EnsureResolver(); var p = GmbConsoleInput(prompt ?? string.Empty); return p == IntPtr.Zero ? null : System.Runtime.InteropServices.Marshal.PtrToStringAnsi(p); }
        catch { System.Console.Write(prompt); return System.Console.ReadLine(); }
    }
    public bool Confirm(string message, string title)
    {
        try { CGfxNative.EnsureResolver(); return GmbConsoleConfirm(message ?? string.Empty) != 0.0; }
        catch { System.Console.Write($"{title}: {message} (Y/N) "); return System.Console.ReadLine()?.StartsWith("Y", StringComparison.OrdinalIgnoreCase) == true; }
    }
    public int? Menu(string title, string[] items)
    {
        try { CGfxNative.EnsureResolver(); long r = GmbConsoleMenu(title ?? string.Empty, items != null ? string.Join("|", items) : string.Empty); return r < 0 ? (int?)null : (int)r; }
        catch { return 0; }
    }
    public string? FileDialog(string title, string pattern, bool save)
    {
        try { CGfxNative.EnsureResolver(); return PtrStr(GmbConsoleFileDialog(title ?? string.Empty, pattern ?? "*", save ? 1 : 0)); }
        catch { return null; }
    }
    public string? ColorPicker(string title, int defaultColor)
    {
        try { CGfxNative.EnsureResolver(); return PtrStr(GmbConsoleColorPicker(title ?? string.Empty, defaultColor.ToString())); }
        catch { return null; }
    }
    public string? DatePicker(string title, string defaultDate)
    {
        try { CGfxNative.EnsureResolver(); return PtrStr(GmbConsoleDatePicker(title ?? string.Empty, defaultDate ?? string.Empty)); }
        catch { return null; }
    }
    public int? Spinner(string title, string[] items)
    {
        try { CGfxNative.EnsureResolver(); return (int)GmbConsoleSpinner(title ?? string.Empty, items != null ? string.Join("|", items) : string.Empty); }
        catch { return 0; }
    }
    public bool Toggle(string title, string prompt, bool initial)
    {
        try { CGfxNative.EnsureResolver(); return GmbConsoleToggle(title ?? string.Empty, prompt ?? string.Empty, initial ? 1 : 0) != 0; }
        catch { return initial; }
    }
    public double? Slider(string title, double min, double max, double initial)
    {
        try { CGfxNative.EnsureResolver(); return GmbConsoleSlider(title ?? string.Empty, min, max, initial); }
        catch { return initial; }
    }
    public void MessageBox(string title, string message)
    {
        try { CGfxNative.EnsureResolver(); GmbConsoleMsgBox(title ?? string.Empty, message ?? string.Empty); }
        catch { System.Console.WriteLine($"[{title}] {message}"); }
    }
}
