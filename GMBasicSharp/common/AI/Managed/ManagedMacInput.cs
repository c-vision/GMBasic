// AI/Managed/ManagedMacInput.cs — Computer L0 GESTITO (C# puro, macOS) per IL/PCodeVM.
// Mouse/tastiera via CGEvent (API di sistema Apple; NESSUN legame col runtime C GMBasic).
using System.Runtime.InteropServices;

namespace Basic.AI.Managed;

public static class ManagedMacInput
{
    static ManagedMacInput() { CfInterop.Ensure(); }

    [StructLayout(LayoutKind.Sequential)] private struct CGPoint { public double X, Y; }

    private const uint MouseMoved = 5, LeftMouseDown = 1, LeftMouseUp = 2;

    [DllImport("ApplicationServices", EntryPoint = "CGPreflightPostEventAccess")]
    private static extern bool CGPreflightPostEventAccess();
    [DllImport("ApplicationServices", EntryPoint = "CGEventCreateMouseEvent")]
    private static extern IntPtr CGEventCreateMouseEvent(IntPtr source, uint type, CGPoint pos, uint button);
    [DllImport("ApplicationServices", EntryPoint = "CGEventPost")]
    private static extern void CGEventPost(uint tap, IntPtr ev);
    [DllImport("CoreGraphics", EntryPoint = "CFRelease")]
    private static extern void CFRelease(IntPtr obj);
    [DllImport("ApplicationServices", EntryPoint = "CGEventCreateKeyboardEvent")]
    private static extern IntPtr CGEventCreateKeyboardEvent(IntPtr source, ushort keycode, bool down);
    [DllImport("ApplicationServices", EntryPoint = "CGEventSetFlags")]
    private static extern void CGEventSetFlags(IntPtr ev, ulong flags);
    [DllImport("ApplicationServices", EntryPoint = "CGEventKeyboardSetUnicodeString")]
    private static extern void CGEventKeyboardSetUnicodeString(IntPtr ev, uint len, [In] char[] str);

    private static bool Allowed() => CGPreflightPostEventAccess();

    private static void PostMouse(uint type, double x, double y)
    {
        var ev = CGEventCreateMouseEvent(IntPtr.Zero, type, new CGPoint { X = x, Y = y }, 0);
        if (ev == IntPtr.Zero) return;
        CGEventPost(0, ev);
        CFRelease(ev);
    }

    public static int Click(double x, double y)
    {
        if (!Allowed()) return -1;
        PostMouse(MouseMoved, x, y); System.Threading.Thread.Sleep(50);
        PostMouse(LeftMouseDown, x, y); System.Threading.Thread.Sleep(30);
        PostMouse(LeftMouseUp, x, y);
        return 0;
    }
    public static int DoubleClick(double x, double y)
    {
        if (Click(x, y) != 0) return -1;
        System.Threading.Thread.Sleep(120);
        return Click(x, y);
    }
    public static int Move(double x, double y)
    {
        if (!Allowed()) return -1;
        PostMouse(MouseMoved, x, y);
        return 0;
    }
    public static int Type(string text)
    {
        if (!Allowed()) return -1;
        var chars = (text ?? "").ToCharArray();
        if (chars.Length == 0) return 0;
        var down = CGEventCreateKeyboardEvent(IntPtr.Zero, 0, true);
        var up = CGEventCreateKeyboardEvent(IntPtr.Zero, 0, false);
        if (down == IntPtr.Zero || up == IntPtr.Zero) { if (down != IntPtr.Zero) CFRelease(down); if (up != IntPtr.Zero) CFRelease(up); return -1; }
        CGEventKeyboardSetUnicodeString(down, (uint)chars.Length, chars);
        CGEventKeyboardSetUnicodeString(up, (uint)chars.Length, chars);
        CGEventPost(0, down); System.Threading.Thread.Sleep(20); CGEventPost(0, up);
        CFRelease(down); CFRelease(up);
        return 0;
    }

    // Keycode macOS comuni (senza Carbon)
    private const ushort KReturn = 0x24, KTab = 0x30, KSpace = 0x31, KEscape = 0x35, KBack = 0x33,
        KDelete = 0x75, KUp = 0x7E, KDown = 0x7D, KLeft = 0x7B, KRight = 0x7C, KHome = 0x73,
        KEnd = 0x77, KPageUp = 0x74, KPageDown = 0x79, KA = 0x00;

    private static bool TryKeycode(string name, out ushort code)
    {
        var n = (name ?? "").ToLowerInvariant();
        switch (n)
        {
            case "return": case "enter": code = KReturn; return true;
            case "tab": code = KTab; return true;
            case "space": code = KSpace; return true;
            case "escape": case "esc": code = KEscape; return true;
            case "backspace": case "delete": code = KBack; return true;
            case "forwarddelete": code = KDelete; return true;
            case "home": code = KHome; return true;
            case "end": code = KEnd; return true;
            case "pageup": code = KPageUp; return true;
            case "pagedown": code = KPageDown; return true;
            case "up": code = KUp; return true;
            case "down": code = KDown; return true;
            case "left": code = KLeft; return true;
            case "right": code = KRight; return true;
        }
        if (n.Length == 1)
        {
            var c = n[0];
            if (c is >= 'a' and <= 'z') { code = (ushort)(KA + (c - 'a')); return true; }
            if (c is >= '0' and <= '9') { code = (ushort)(0x1D + (c - '0')); return true; }
        }
        code = 0;
        return false;
    }

    public static int Key(string keys)
    {
        if (!Allowed()) return -1;
        if (!TryKeycode(keys ?? "", out var code)) return Type(keys);
        var d = CGEventCreateKeyboardEvent(IntPtr.Zero, code, true);
        var u = CGEventCreateKeyboardEvent(IntPtr.Zero, code, false);
        if (d != IntPtr.Zero) { CGEventPost(0, d); CFRelease(d); }
        if (u != IntPtr.Zero) { CGEventPost(0, u); CFRelease(u); }
        return 0;
    }

    private const ulong FlagShift = 1UL << 17, FlagControl = 1UL << 18,
        FlagAlternate = 1UL << 19, FlagCommand = 1UL << 20;

    public static int HotKey(string combo)
    {
        if (!Allowed()) return -1;
        var parts = (combo ?? "").Split('+', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 2) return -1;
        if (!TryKeycode(parts[^1], out var code)) return -1;
        ulong flags = 0;
        foreach (var mod in parts[..^1])
        {
            switch (mod.ToLowerInvariant())
            {
                case "shift": flags |= FlagShift; break;
                case "ctrl": case "control": flags |= FlagControl; break;
                case "alt": case "option": case "opt": flags |= FlagAlternate; break;
                case "cmd": case "command": case "meta": flags |= FlagCommand; break;
                default: return -1;
            }
        }
        var d = CGEventCreateKeyboardEvent(IntPtr.Zero, code, true);
        var u = CGEventCreateKeyboardEvent(IntPtr.Zero, code, false);
        if (d == IntPtr.Zero || u == IntPtr.Zero) { if (d != IntPtr.Zero) CFRelease(d); if (u != IntPtr.Zero) CFRelease(u); return -1; }
        CGEventSetFlags(d, flags);
        CGEventSetFlags(u, flags);
        CGEventPost(0, d); System.Threading.Thread.Sleep(20); CGEventPost(0, u);
        CFRelease(d); CFRelease(u);
        return 0;
    }
}
