// FILE: Basic.Runtime.Services.NativeTui.cs
// Via UNICA dei controlli TUI per i backend gestiti (IL/PCode): NIENTE rendering proprio.
// P/Invoke al runtime C (Label): C#->C, direzione ammessa. Il controllo si rende su
// TUI/SDLConsole/grafica secondo il contesto attivo. @GET/READ usano la via unica già
// esistente (gmb_read_* → TextInput): nessun doppione qui.
using System;
using System.Runtime.InteropServices;

namespace Basic.Runtime.Services;

public static class NativeTui
{
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_at_say", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GAtSay(int row, int col, string text);

    public static void AtSay(int row, int col, string text)
    { try { CGfxNative.EnsureResolver(); GAtSay(row, col, text ?? string.Empty); } catch { } }

    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_add_field_typed", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GReadAddTyped(int row, int col, int width, string initial, string type,
                                            string picture, string items, double vmin, double vmax, double step);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_native_run", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GReadRun();
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_native_value", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GReadValue(int idx);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_set_message", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GReadSetMessage(int idx, string text);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_set_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GReadSetError(int idx, string text);
    /// <summary>MESSAGE/ERROR dell'ultimo campo @GET (idx -1): riga di stato in TUI.</summary>
    public static void ReadSetMessageLast(string text)
    { try { CGfxNative.EnsureResolver(); GReadSetMessage(-1, text ?? ""); } catch { } }
    public static void ReadSetErrorLast(string text)
    { try { CGfxNative.EnsureResolver(); GReadSetError(-1, text ?? ""); } catch { } }
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_skip_field", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GReadSkipField(int idx);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_native_reset", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GReadReset();

    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_tui_inject_keys", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GInjectKeys(string keys);

    /// <summary>Inietta l'input scriptato dei test nella typeahead nativa (stessa
    /// variabile GMB_TUI_SCRIPT usata dalle gambe C99/VM-C).</summary>
    public static void InjectScriptFromEnv()
    {
        var script = Basic.Providers.GmbEnv.Get("GMB_TUI_SCRIPT");
        if (string.IsNullOrEmpty(script)) return;
        try { CGfxNative.EnsureResolver(); GInjectKeys(script); } catch { }
    }

    /// <summary>READ unificato NATIVO (una sola implementazione per tutte le gambe).</summary>
    public static int ReadAddTyped(int row, int col, int width, string initial, string type,
                                   string picture, string items, double vmin, double vmax, double step)
    {
        try { CGfxNative.EnsureResolver();
              return GReadAddTyped(row, col, width, initial ?? "", type ?? "", picture ?? "", items ?? "", vmin, vmax, step); }
        catch { return -1; }
    }
    public static int ReadRun() { try { CGfxNative.EnsureResolver(); return GReadRun(); } catch { return 2; } }
    public static string ReadValue(int idx)
    { try { CGfxNative.EnsureResolver(); var p = GReadValue(idx); return p == IntPtr.Zero ? "" : (Marshal.PtrToStringUTF8(p) ?? ""); } catch { return ""; } }
    public static void ReadReset() { try { CGfxNative.EnsureResolver(); GReadReset(); } catch { } }
    /// <summary>WHEN .F.: il campo non riceve il fuoco nel loop READ (skip, nessun park).</summary>
    public static void ReadSkipField(int idx) { try { CGfxNative.EnsureResolver(); GReadSkipField(idx); } catch { } }
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_set_when_fn", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GReadSetWhenFn(int idx, ReadValidFn fn, IntPtr user);
    public static void ReadSetWhenFn(int idx, Func<bool> when)
    {
        if (idx < 0 || when is null) return;
        int slot = _readCbNext++;
        ReadValidFn fn = _ => { try { return when() ? 1 : 0; } catch { return 1; } };
        _readDelegates.Add(fn);
        try { CGfxNative.EnsureResolver(); GReadSetWhenFn(idx, fn, (IntPtr)slot); } catch { }
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ReadValidFn(IntPtr user);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate void ReadApplyFn(IntPtr user, IntPtr text);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_set_valid_fn", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GReadSetValidFn(int idx, ReadValidFn fn, IntPtr user);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_read_set_apply_text", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GReadSetApplyText(int idx, ReadApplyFn fn, IntPtr user);
    private static readonly System.Collections.Generic.List<object> _readDelegates = new();
    private static int _readCbNext;

    /// <summary>Predicato VALID/WHEN sul campo: il runtime nativo lo richiama a ogni conferma
    /// (falso = ripropone il campo). Umano solo dal lato C#: il valore si legge dal buffer.</summary>
    public static void ReadSetValidFn(int idx, Func<bool> valid)
    {
        if (idx < 0 || valid is null) return;
        int slot = _readCbNext++;
        ReadValidFn fn = _ => { try { return valid() ? 1 : 0; } catch { return 1; } };
        _readDelegates.Add(fn);
        try { CGfxNative.EnsureResolver(); GReadSetValidFn(idx, fn, (IntPtr)slot); } catch { }
    }

    /// <summary>Write-through: il campo scrive nella variabile a ogni tasto, così il predicato
    /// VALID legge il testo corrente (semantica Clipper del buffer Get).</summary>
    public static void ReadSetApplyText(int idx, Action<string> apply)
    {
        if (idx < 0 || apply is null) return;
        int slot = _readCbNext++;
        ReadApplyFn fn = (_, text) => { try { apply(Marshal.PtrToStringUTF8(text) ?? ""); } catch { } };
        _readDelegates.Add(fn);
        try { CGfxNative.EnsureResolver(); GReadSetApplyText(idx, fn, (IntPtr)slot); } catch { }
    }

    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_console_alert_ex", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GAlertEx(string msg, string b0, string b1, string b2, int align, int width, int border,
        int hasFg, int fr, int fg, int fb, int hasBg, int br, int bg, int bb);

    /// <summary>ALERT/CONFIRM a parità AlertBox.cs.Show: pulsanti/align/width/border/colori.
    /// Ritorna 1..N (pulsante) o 0 (chiusa).</summary>
    public static int AlertEx(string msg, string? b0, string? b1, string? b2, int align, int width, int border,
        int hasFg, int fr, int fg, int fb, int hasBg, int br, int bg, int bb)
    {
        try
        {
            CGfxNative.EnsureResolver();
            return GAlertEx(msg ?? string.Empty, b0 ?? string.Empty, b1 ?? string.Empty, b2 ?? string.Empty,
                align, width, border, hasFg, fr, fg, fb, hasBg, br, bg, bb);
        }
        catch { return 0; }
    }
}
