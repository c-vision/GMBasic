// FILE: common/CommonLib/TrayUtils.cs
// TRAY.* — controparte gestita (IL/PCodeVM) della libreria C del runtime
// (native/gmb_runtime_c/{include/gmb_tray.h,src/gmb_tray*.c}).
// I backend C (VMC/C99/C99Native) chiamano la libreria direttamente; qui si fa
// P/Invoke agli stessi simboli. Se la lib non è caricata → fallback no-op
// (available=0, backend="none"), senza eccezioni.
//
// REGOLE: create/item/tooltip/remove/poll sul MAIN THREAD; il callback degli item
// va dispatchato come TASK GPM (mai eseguito direttamente sul thread UI).
using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Basic;

public static class TrayUtils
{
    private const string LibName = "gmb_tray";

    /// <summary>Risoluzione della lib tray: cerca Resources/libs/<rid>/ risalendo dalla base
    /// dell'app (funziona in QUALSIASI host: REPL, test_ilrun, gate C99, daemon) — parità backend.</summary>
    static TrayUtils()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(TrayUtils).Assembly, ResolveTray); }
        catch { /* resolver già impostato: ignora */ }
    }

    private static string LibFileName()
        => OperatingSystem.IsMacOS() ? "libgmb_tray.dylib"
         : OperatingSystem.IsWindows() ? "gmb_tray.dll"
         : "libgmb_tray.so";

    private static IntPtr ResolveTray(string name, Assembly asm, DllImportSearchPath? paths)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TrayCallback(int itemId, IntPtr user);

    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_available();
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr gmb_tray_backend();
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_create(string? iconPath, string? tooltip);
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_item(int id, string? label);
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_sep();
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_tooltip(string? text);
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_tray_set_callback(TrayCallback? cb, IntPtr user);
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_poll();
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern void gmb_tray_remove();
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_selftest();
    [DllImport(LibName, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int gmb_tray_last();

    private static TrayCallback? _cbKeepAlive;   // evita il GC del delegate marshallato
    private static Action<int>? _managedCb;

    public static bool Available()
    {
        try { return gmb_tray_available() != 0; } catch { return false; }
    }

    public static string Backend()
    {
        try
        {
            var p = gmb_tray_backend();
            return p == IntPtr.Zero ? "none" : (Marshal.PtrToStringUTF8(p) ?? "none");
        }
        catch { return "none"; }
    }

    public static int Create(string? iconPath, string? tooltip)
    {
        try { return gmb_tray_create(iconPath, tooltip); } catch { return -1; }
    }

    public static int Item(int id, string? label)
    {
        try { return gmb_tray_item(id, label); } catch { return -1; }
    }

    public static int Sep()
    {
        try { return gmb_tray_sep(); } catch { return -1; }
    }

    public static int Tooltip(string? text)
    {
        try { return gmb_tray_tooltip(text); } catch { return -1; }
    }

    /// <summary>Registra il callback degli item. NB: dispatchare come task GPM nel chiamante.</summary>
    public static void SetCallback(Action<int>? cb)
    {
        _managedCb = cb;
        _cbKeepAlive = cb == null ? null : (id, _) => _managedCb?.Invoke(id);
        try { gmb_tray_set_callback(_cbKeepAlive, IntPtr.Zero); } catch { /* lib assente */ }
    }

    public static int Poll()
    {
        try { return gmb_tray_poll(); } catch { return 0; }
    }

    public static void Remove()
    {
        try { gmb_tray_remove(); } catch { /* lib assente */ }
    }

    public static long Last()
    {
        try { return gmb_tray_last(); } catch { return -1; }
    }

    public static bool SelfTest()
    {
        try { return gmb_tray_selftest() != 0; } catch { return false; }
    }
}
