// AI/Managed/CfInterop.cs — interop CoreFoundation/CoreGraphics/ImageIO (API Apple).
// Helper per CFString/CFArray/CFDictionary/CFNumber/CFURL + screenshot PNG. Nessun legame
// col runtime C di GMBasic: solo API di sistema macOS, usate dai backend gestiti (IL/PCode).
using System.Runtime.InteropServices;

namespace Basic.AI.Managed;

internal static class CfInterop
{
    // Il resolver dei framework macOS è quello CONDIVISO e idempotente dell'assembly AI
    // (Basic.Providers.AiNativeResolver): `SetDllImportResolver` si può impostare una sola volta
    // per assembly, quindi registrarne uno dedicato qui lanciava se un altro tipo l'aveva già
    // fatto (divergenza IL/PCode vs VM/C su COMPUTER.SCREENSHOT).
    static CfInterop() => Basic.Providers.AiNativeResolver.EnsureRegistered(typeof(CfInterop).Assembly);

    /// <summary>Forza l'init (registrazione unica del resolver per l'assembly).</summary>
    internal static void Ensure() { }

    // ---- CoreFoundation ----
    [DllImport("CoreFoundation", EntryPoint = "CFRelease")]
    internal static extern void CFRelease(IntPtr obj);
    [DllImport("CoreFoundation", EntryPoint = "CFRetain")]
    internal static extern IntPtr CFRetain(IntPtr obj);
    [DllImport("CoreFoundation", EntryPoint = "CFStringCreateWithBytes")]
    internal static extern IntPtr CFStringCreateWithBytes(IntPtr alloc, [In] byte[]? bytes, nint numBytes, uint encoding, byte isExternal);
    [DllImport("CoreFoundation", EntryPoint = "CFStringGetLength")]
    internal static extern nint CFStringGetLength(IntPtr str);
    [DllImport("CoreFoundation", EntryPoint = "CFStringGetCString")]
    internal static extern byte CFStringGetCString(IntPtr str, byte[] buf, nint max, uint encoding);
    [DllImport("CoreFoundation", EntryPoint = "CFArrayGetCount")]
    internal static extern nint CFArrayGetCount(IntPtr array);
    [DllImport("CoreFoundation", EntryPoint = "CFArrayGetValueAtIndex")]
    internal static extern IntPtr CFArrayGetValueAtIndex(IntPtr array, nint index);
    [DllImport("CoreFoundation", EntryPoint = "CFDictionaryGetValue")]
    internal static extern IntPtr CFDictionaryGetValue(IntPtr dict, IntPtr key);
    [DllImport("CoreFoundation", EntryPoint = "CFNumberGetValue")]
    internal static extern byte CFNumberGetValue(IntPtr num, nint type, out int value);
    [DllImport("CoreFoundation", EntryPoint = "CFURLCreateFromFileSystemRepresentation")]
    internal static extern IntPtr CFURLCreateFromFileSystemRepresentation(IntPtr alloc, byte[] path, nint len, byte dir);

    private const uint kCFStringEncodingUTF8 = 0x08000100;

    private static IntPtr _cfHandle;
    /// <summary>Indirizzo della costante CoreFoundation kCFBooleanTrue (usato da AX focus).</summary>
    internal static IntPtr KCFBooleanTrueRef()
    {
        try
        {
            if (_cfHandle == IntPtr.Zero &&
                !NativeLibrary.TryLoad("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", out _cfHandle))
                return IntPtr.Zero;
            return NativeLibrary.GetExport(_cfHandle, "kCFBooleanTrue");
        }
        catch { return IntPtr.Zero; }
    }

    internal static IntPtr CStr(string s)
    {
        var bytes = System.Text.Encoding.UTF8.GetBytes(s ?? "");
        return CFStringCreateWithBytes(IntPtr.Zero, bytes, bytes.Length, kCFStringEncodingUTF8, 0);
    }

    internal static string? GetCString(IntPtr cf)
    {
        if (cf == IntPtr.Zero) return null;
        var len = CFStringGetLength(cf);
        var buf = new byte[(int)Math.Min(Math.Max(len * 4 + 8, 16), 1 << 20)];
        if (CFStringGetCString(cf, buf, buf.Length, kCFStringEncodingUTF8) == 0) return null;
        var end = Array.IndexOf<byte>(buf, 0);
        return System.Text.Encoding.UTF8.GetString(buf, 0, end < 0 ? buf.Length : end);
    }

    internal static bool Contains(IntPtr hay, IntPtr needle)
    {
        var h = GetCString(hay);
        var n = GetCString(needle);
        return h is not null && n is not null && n.Length > 0 &&
               h.IndexOf(n, StringComparison.OrdinalIgnoreCase) >= 0;
    }

    internal static int GetInt(IntPtr num)
    {
        if (num == IntPtr.Zero) return 0;
        CFNumberGetValue(num, 3, out var v); // kCFNumberSInt32Type
        return v;
    }

    // ---- CoreGraphics ----
    [DllImport("CoreGraphics", EntryPoint = "CGMainDisplayID")]
    internal static extern uint CGMainDisplayID();
    [DllImport("CoreGraphics", EntryPoint = "CGDisplayCreateImage")]
    internal static extern IntPtr CGDisplayCreateImage(uint display);
    [DllImport("CoreGraphics", EntryPoint = "CGWindowListCopyWindowInfo")]
    internal static extern IntPtr CGWindowListCopyWindowInfo(uint options, uint windowId);
    // ImageIO: destinazione PNG
    [DllImport("ImageIO", EntryPoint = "CGImageDestinationCreateWithURL")]
    internal static extern IntPtr CGImageDestinationCreateWithURL(IntPtr url, IntPtr type, nint count, IntPtr opts);
    [DllImport("ImageIO", EntryPoint = "CGImageDestinationAddImage")]
    internal static extern void CGImageDestinationAddImage(IntPtr dest, IntPtr image, IntPtr props);
    [DllImport("ImageIO", EntryPoint = "CGImageDestinationFinalize")]
    internal static extern byte CGImageDestinationFinalize(IntPtr dest);

    internal const uint kCGWindowListOptionOnScreenOnly = 1;
    internal const uint kCGWindowListExcludeDesktopElements = 1 << 4;
    internal const uint kCGNullWindowID = 0;
}
