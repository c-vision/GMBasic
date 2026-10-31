// FILE: Basic.Runtime.CGfxClipboardService.cs
// Clipboard di sistema via motore C (SDL_SetClipboardText/GetClipboardText).
// Usato quando GMB_GFX_C=1: stessa clipboard del desktop, non l'in-memory headless.
using System;
using System.Runtime.InteropServices;
using System.Text;
using CommonLib.Clipboard;

namespace Basic.Runtime;

public sealed class CGfxClipboardService : IClipboardService
{
    private const string Lib = CGfxNative.Lib;

    [DllImport(Lib)] private static extern void gmb_clipboard_set(string text);
    [DllImport(Lib)] private static extern int gmb_clipboard_get_buf(byte[] outbuf, int maxlen);
    [DllImport(Lib)] private static extern void gmb_clipboard_clear();

    static CGfxClipboardService() => CGfxNative.EnsureResolver();

    public string? GetText()
    {
        try
        {
            var buf = new byte[8192];
            var n = gmb_clipboard_get_buf(buf, buf.Length);
            if (n <= 0) return string.Empty;
            return Encoding.UTF8.GetString(buf, 0, n);
        }
        catch { return string.Empty; }
    }

    public void SetText(string? text)
    {
        try { gmb_clipboard_set(text ?? string.Empty); } catch { }
    }

    public void Clear()
    {
        try { gmb_clipboard_clear(); } catch { }
    }
}
