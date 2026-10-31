// FILE: Basic.Runtime.Services.NativeWindowManager.cs
// Via UNICA del WindowManager (C) per i backend gestiti (IL/PCode): NIENTE rendering/logica
// in C#. Geometria + modello di contenuto (righe/scroll/SayAt) vivono nel runtime C.
// P/Invoke C#->C (direzione ammessa). Handle stabile = gmb_window_manager_static().
using System;
using System.Runtime.InteropServices;

namespace Basic.Runtime.Services;

public static class NativeWindowManager
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int x, y, w, h; public Rect(int x, int y, int w, int h) { this.x = x; this.y = y; this.w = w; this.h = h; } }

    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_static", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr MStatic();
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_add", CallingConvention = CallingConvention.Cdecl)]
    private static extern int MAdd(IntPtr m, string title, Rect rect);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_get", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr MGet(IntPtr m, int id);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_focused", CallingConvention = CallingConvention.Cdecl)]
    private static extern int MFocused(IntPtr m);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_topmost_at", CallingConvention = CallingConvention.Cdecl)]
    private static extern int MTopmostAt(IntPtr m, int x, int y);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_find_by_title", CallingConvention = CallingConvention.Cdecl)]
    private static extern int MFindByTitle(IntPtr m, string title);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_raise", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MRaise(IntPtr m, int id);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_deactivate", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MDeactivate(IntPtr m, int id);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_move_by", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MMoveBy(IntPtr m, int id, int dx, int dy);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_move_to", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MMoveTo(IntPtr m, int id, int x, int y);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_resize_by", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MResizeBy(IntPtr m, int id, int dw, int dh);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_minimize", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MMinimize(IntPtr m, int id);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_restore", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MRestore(IntPtr m, int id);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_maximize", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MMaximize(IntPtr m, int id);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_tile", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MTile(IntPtr m, int rows, int cols);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_window_manager_snap_zone", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MSnap(IntPtr m, int id, int zone);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_managed_window_say_at", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr MSayAt(IntPtr w, int row, int col, string text);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_managed_window_append_line", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MAppendLine(IntPtr w, string line);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_managed_window_set_lines", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MSetLines(IntPtr w, IntPtr lines, int n);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_managed_window_scroll_by", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MScrollBy(IntPtr w, int delta);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_managed_window_scroll_to", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MScrollTo(IntPtr w, int offset);
    [DllImport(CGfxNative.Lib, EntryPoint = "gmb_managed_window_box_at", CallingConvention = CallingConvention.Cdecl)]
    private static extern void MBoxAt(IntPtr w, int top, int left, int bottom, int right, string border);

    private static IntPtr Mgr() { CGfxNative.EnsureResolver(); return MStatic(); }

    public static IntPtr Manager() => Mgr();
    public static int Add(string title, Rect r) => MAdd(Mgr(), title ?? "", r);
    public static IntPtr Get(int id) => MGet(Mgr(), id);
    public static int Focused() => MFocused(Mgr());
    public static int TopmostAt(int x, int y) => MTopmostAt(Mgr(), x, y);
    public static int FindByTitle(string title) => MFindByTitle(Mgr(), title ?? "");
    public static void Raise(int id) => MRaise(Mgr(), id);
    public static void Deactivate(int id) => MDeactivate(Mgr(), id);
    public static void MoveBy(int id, int dx, int dy) => MMoveBy(Mgr(), id, dx, dy);
    public static void MoveTo(int id, int x, int y) => MMoveTo(Mgr(), id, x, y);
    public static void ResizeBy(int id, int dw, int dh) => MResizeBy(Mgr(), id, dw, dh);
    public static void Minimize(int id) => MMinimize(Mgr(), id);
    public static void Restore(int id) => MRestore(Mgr(), id);
    public static void Maximize(int id) => MMaximize(Mgr(), id);
    public static void Tile(int rows, int cols) => MTile(Mgr(), rows, cols);
    public static void Snap(int id, int zone) => MSnap(Mgr(), id, zone);
    public static string SayAt(IntPtr w, int row, int col, string text)
    { try { var p = MSayAt(w, row, col, text ?? ""); return p == IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(p) ?? ""; } catch { return ""; } }
    public static void AppendLine(IntPtr w, string line) => MAppendLine(w, line ?? "");
    public static void SetLines(IntPtr w, string[] lines)
    {
        var arr = lines ?? Array.Empty<string>();
        IntPtr[] ptrs = new IntPtr[arr.Length];
        IntPtr block = Marshal.AllocHGlobal(IntPtr.Size * arr.Length);
        try
        {
            for (int i = 0; i < arr.Length; i++) ptrs[i] = Marshal.StringToHGlobalAnsi(arr[i] ?? "");
            Marshal.Copy(ptrs, 0, block, ptrs.Length);
            MSetLines(w, block, arr.Length);
        }
        finally
        {
            for (int i = 0; i < ptrs.Length; i++) if (ptrs[i] != IntPtr.Zero) Marshal.FreeHGlobal(ptrs[i]);
            Marshal.FreeHGlobal(block);
        }
    }
    public static void ScrollBy(IntPtr w, int delta) => MScrollBy(w, delta);
    public static void ScrollTo(IntPtr w, int offset) => MScrollTo(w, offset);
    public static void BoxAt(IntPtr w, int top, int left, int bottom, int right, string? border) => MBoxAt(w, top, left, bottom, right, border ?? "single");
}
