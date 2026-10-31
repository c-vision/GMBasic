// FILE: Basic.Runtime.CGfxInputService.cs
// Bridge C# -> C per l'input tastiera/mouse del motore grafico unico.
//
// Flusso: la pump SDL (gmb_gfx_present_tick, main thread) accoda gli eventi nel
// modulo C (gmb_gfx_input); il main thread gestito li drena con TryPollKey/
// TryPollMouse e li consegna al runtime (IlRuntime.DrainCGfxInput):
//   - tasti  -> IlRuntime.QueueInkey (INKEY/WAITKEY/GET/ON KEY invariati)
//   - mouse  -> stato _guiMouse*/ClipperGlobal.State + DispatchMouse + finestre
//   - Ctrl+C -> flag: il runtime decide (intercettato -> tasto, altrimenti abort)
// L'iniezione (InjectKey/InjectMouse) serve a test/CI e funziona anche headless.
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Basic.Runtime;

public static class CGfxInputService
{
    private const string Lib = CGfxNative.Lib;

    // Parita' con GMB_GFX_MOUSE_* (gmb_gfx_input.h).
    public const int MouseMove = 1;
    public const int MouseDown = 2;
    public const int MouseUp = 3;
    public const int MouseWheel = 4;

    [DllImport(Lib)] private static extern void gmb_gfx_input_push_key(string key);
    [DllImport(Lib)] private static extern void gmb_gfx_input_push_mouse(int type, int x, int y, int button, int wheel);
    [DllImport(Lib)] private static extern int gmb_gfx_input_poll_key(byte[] outbuf, int maxlen);
    [DllImport(Lib)] private static extern int gmb_gfx_input_poll_mouse(out int type, out int x, out int y, out int button, out int wheel);
    [DllImport(Lib)] private static extern int gmb_gfx_input_take_ctrl_c();
    [DllImport(Lib)] private static extern int gmb_gfx_input_key_down(int scancode);
    [DllImport(Lib)] private static extern void gmb_gfx_input_reset();
    [DllImport(Lib)] private static extern void gmb_gfx_present_show_cursor(int visible);
    [DllImport(Lib)] private static extern void gmb_gfx_present_warp_mouse(int x, int y);
    [DllImport(Lib)] private static extern void gmb_gfx_present_set_cursor(int cursorId);
    [DllImport(Lib)] private static extern void gmb_gfx_present_mouse_limit(int x1, int y1, int x2, int y2);

    static CGfxInputService() => CGfxNative.EnsureResolver();

    /// <summary>Accoda un tasto UTF-8 (test/CI: stesso percorso della pump SDL).</summary>
    public static void InjectKey(string key)
    {
        if (string.IsNullOrEmpty(key)) return;
        try { gmb_gfx_input_push_key(key); } catch { }
    }

    /// <summary>Accoda un evento mouse. button = numero SDL (1=L,2=M,3=R); wheel solo per MouseWheel.</summary>
    public static void InjectMouse(int type, int x, int y, int button, int wheel = 0)
    {
        try { gmb_gfx_input_push_mouse(type, x, y, button, wheel); } catch { }
    }

    public static void Reset() { try { gmb_gfx_input_reset(); } catch { } }

    /// <summary>Prossimo tasto (stringa UTF-8). False se la coda e' vuota.</summary>
    public static bool TryPollKey(out string key)
    {
        key = string.Empty;
        try
        {
            var buf = new byte[64];
            if (gmb_gfx_input_poll_key(buf, buf.Length) == 0) return false;
            var len = Array.IndexOf(buf, (byte)0);
            if (len < 0) len = buf.Length;
            key = Encoding.UTF8.GetString(buf, 0, len);
            return key.Length > 0;
        }
        catch { return false; }
    }

    /// <summary>Prossimo evento mouse. False se la coda e' vuota.</summary>
    public static bool TryPollMouse(out int type, out int x, out int y, out int button, out int wheel)
    {
        type = x = y = button = wheel = 0;
        try { return gmb_gfx_input_poll_mouse(out type, out x, out y, out button, out wheel) != 0; }
        catch { return false; }
    }

    /// <summary>Ctrl+C premuto dall'ultimo check (consuma il flag).</summary>
    public static bool TakeCtrlC() { try { return gmb_gfx_input_take_ctrl_c() != 0; } catch { return false; } }

    /// <summary>Stato fisico di un tasto (scancode SDL), per usi futuri.</summary>
    public static bool KeyDown(int scancode) { try { return gmb_gfx_input_key_down(scancode) != 0; } catch { return false; } }

    // ── Cursore/puntatore (stesse semantiche di GraphicsManager.Mouse*_GPM) ──
    public static void ShowCursor(bool visible) { try { gmb_gfx_present_show_cursor(visible ? 1 : 0); } catch { } }
    public static void WarpMouse(int x, int y) { try { gmb_gfx_present_warp_mouse(x, y); } catch { } }
    public static void SetCursor(int cursorId) { try { gmb_gfx_present_set_cursor(cursorId); } catch { } }
    public static void MouseLimit(int x1, int y1, int x2, int y2) { try { gmb_gfx_present_mouse_limit(x1, y1, x2, y2); } catch { } }
}
