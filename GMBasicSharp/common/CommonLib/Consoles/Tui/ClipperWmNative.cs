// FILE: CommonLib.Consoles.Tui.ClipperWmNative.cs
// P/Invoke alla WindowManager del runtime C (libgmb_gfx). Vive in CommonLib perché qui
// il resolver della dylib è già attivo (stesso canale di ScreenBuffer). Direzione C#->C.
using System;
using System.Runtime.InteropServices;

namespace CommonLib.Consoles.Tui;

public static class ClipperWmNative
{
    private const string Lib = "gmb_gfx";

    [DllImport(Lib)] public static extern IntPtr gmb_window_manager_new(int x, int y, int w, int h);
    [DllImport(Lib)] public static extern void gmb_window_manager_free(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_clear_windows(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_set_current_desktop(IntPtr m, int d);
    [DllImport(Lib)] public static extern int gmb_window_manager_push(IntPtr m, IntPtr title, int id, int x, int y, int w, int h,
        int z, int state, int minimized, int border, int focused, int desktop, int closable);
    [DllImport(Lib)] public static extern void gmb_window_manager_set_statusbar(IntPtr m, IntPtr text);
    [DllImport(Lib)] public static extern void gmb_window_manager_render_named(IntPtr m, IntPtr buf, IntPtr theme, IntPtr header);
    [DllImport(Lib)] public static extern void gmb_window_manager_send_to_cell(IntPtr m, int id, int rows, int cols, int row, int col, int gap);
    [DllImport(Lib)] public static extern void gmb_window_manager_tile_all(IntPtr m, int rows, int cols, int gap);
    [DllImport(Lib)] public static extern void gmb_window_manager_window_rect(IntPtr m, int id, int[] out4);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_state(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_focused(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_desktop(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_focused(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_switch_desktop(IntPtr m, int desktop);
    [DllImport(Lib)] public static extern void gmb_window_manager_move_to_desktop(IntPtr m, int id, int desktop);
    [DllImport(Lib)] public static extern void gmb_window_manager_move_by(IntPtr m, int id, int dx, int dy);
    [DllImport(Lib)] public static extern void gmb_window_manager_move_to(IntPtr m, int id, int x, int y);
    [DllImport(Lib)] public static extern void gmb_window_manager_resize_by(IntPtr m, int id, int dw, int dh);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_z(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_count(IntPtr m, int desktop);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_id_at(IntPtr m, int index);
    [DllImport(Lib)] public static extern int gmb_window_manager_current_desktop(IntPtr m);
    [DllImport(Lib)] [return: MarshalAs(UnmanagedType.LPStr)] public static extern string gmb_window_manager_status_text(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_set_title(IntPtr m, int id, IntPtr title);
    [DllImport(Lib)] public static extern int gmb_window_manager_is_dragging(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_is_resizing(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_drag_id(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_resize_id(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_clear_focus(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_work_area4(IntPtr m, int[] out4);
    [DllImport(Lib)] public static extern int gmb_window_manager_focus_next(IntPtr m, int forward);
    [DllImport(Lib)] public static extern int gmb_window_manager_taskbar_chip_at(IntPtr m, int x, int y);
    [DllImport(Lib)] public static extern int gmb_window_manager_topmost_at(IntPtr m, int x, int y);
    [DllImport(Lib)] public static extern int gmb_window_manager_has_status(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_sync_workarea(IntPtr m, int width, int height);
    [DllImport(Lib)] private static extern int gmb_window_manager_add_ex(IntPtr m, IntPtr name, IntPtr title,
        int x, int y, int w, int h, int border, int frame, int shadow, int desktop);
    [DllImport(Lib)] private static extern int gmb_window_manager_find_by_title(IntPtr m, IntPtr title);
    [DllImport(Lib)] private static extern int gmb_window_manager_find_by_name(IntPtr m, IntPtr name);

    /// <summary>Crea una finestra nel manager C (nome e titolo distinti).</summary>
    public static int AddWindowEx(IntPtr m, string? name, string? title,
        int x, int y, int w, int h, int border, int frame, int shadow, int desktop)
    {
        var n = A(name); var t = A(title);
        try { return gmb_window_manager_add_ex(m, n, t, x, y, w, h, border, frame, shadow, desktop); }
        finally { Free(n); Free(t); }
    }

    public static int FindByTitle(IntPtr m, string? title)
    {
        var t = A(title);
        try { return gmb_window_manager_find_by_title(m, t); } finally { Free(t); }
    }

    public static int FindByName(IntPtr m, string? name)
    {
        var n = A(name);
        try { return gmb_window_manager_find_by_name(m, n); } finally { Free(n); }
    }
    [DllImport(Lib)] public static extern void gmb_window_manager_raise(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_deactivate(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_select(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_minimize(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_restore(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_unminimize(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_maximize_toggle(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_start_drag(IntPtr m, int id, int x, int y);
    [DllImport(Lib)] public static extern int gmb_window_manager_update_drag(IntPtr m, int x, int y);
    [DllImport(Lib)] public static extern int gmb_window_manager_end_drag(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_start_resize(IntPtr m, int id, int x, int y, IntPtr edges);
    [DllImport(Lib)] public static extern int gmb_window_manager_update_resize(IntPtr m, int x, int y);
    [DllImport(Lib)] public static extern void gmb_window_manager_end_resize(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_snap_zone(IntPtr m, int id, int zone);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_at_titlebar(IntPtr m, int x, int y);
    public static int StartResize(IntPtr m, int id, int x, int y, string? edges)
    {
        var e = A(edges);
        try { return gmb_window_manager_start_resize(m, id, x, y, e); } finally { Free(e); }
    }

    private static IntPtr A(string? s) => s is null ? IntPtr.Zero : Marshal.StringToHGlobalAnsi(s);
    private static void Free(IntPtr p) { if (p != IntPtr.Zero) Marshal.FreeHGlobal(p); }

    public static int Push(IntPtr m, string? title, int id, int x, int y, int w, int h,
        int z, int state, int minimized, int border, int focused, int desktop, int closable)
    {
        var t = A(title);
        try { return gmb_window_manager_push(m, t, id, x, y, w, h, z, state, minimized, border, focused, desktop, closable); }
        finally { Free(t); }
    }

    public static void SetStatusBar(IntPtr m, string? text)
    {
        var t = A(text);
        try { gmb_window_manager_set_statusbar(m, t); } finally { Free(t); }
    }

    public static void RenderNamed(IntPtr m, IntPtr buf, string? theme, string? header)
    {
        var tn = A(theme); var th = A(header);
        try { gmb_window_manager_render_named(m, buf, tn, th); } finally { Free(tn); Free(th); }
    }

    // ── Accesso generico alla finestra C (ManagedWindow C# = proxy, nessun modello duplicato) ──
    [StructLayout(LayoutKind.Sequential)]
    public struct WmRect { public int x, y, w, h; }

    [DllImport(Lib)] public static extern IntPtr gmb_window_manager_get(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_remove(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_managed_window_get_int(IntPtr w, int field);
    [DllImport(Lib)] public static extern void gmb_managed_window_set_int(IntPtr w, int field, int v);
    [DllImport(Lib)] public static extern IntPtr gmb_managed_window_get_str(IntPtr w, int field);
    [DllImport(Lib)] public static extern void gmb_managed_window_set_str(IntPtr w, int field, IntPtr v);
    [DllImport(Lib)] public static extern IntPtr gmb_managed_window_get_line(IntPtr w, int i);
    [DllImport(Lib)] public static extern int gmb_managed_window_lines_count(IntPtr w);
    [DllImport(Lib)] public static extern void gmb_managed_window_line_set(IntPtr w, int i, IntPtr s);
    [DllImport(Lib)] public static extern void gmb_managed_window_line_insert(IntPtr w, int i, IntPtr s);
    [DllImport(Lib)] public static extern void gmb_managed_window_line_remove(IntPtr w, int i);
    [DllImport(Lib)] public static extern void gmb_managed_window_append_line(IntPtr w, IntPtr s);
    [DllImport(Lib)] public static extern void gmb_managed_window_set_lines(IntPtr w, IntPtr lines, int n);
    [DllImport(Lib)] public static extern void gmb_managed_window_scroll_to(IntPtr w, int off);
    [DllImport(Lib)] public static extern void gmb_managed_window_scroll_by(IntPtr w, int d);
    [DllImport(Lib)] public static extern IntPtr gmb_managed_window_say_at(IntPtr w, int row, int col, IntPtr text);
    [DllImport(Lib)] public static extern void gmb_managed_window_box_at(IntPtr w, int top, int left, int bottom, int right, IntPtr border);
    [DllImport(Lib)] public static extern WmRect gmb_managed_window_content_rect(IntPtr w);
    [DllImport(Lib)] public static extern WmRect gmb_managed_window_titlebar_rect(IntPtr w);
    [DllImport(Lib)] public static extern int gmb_managed_window_control_at(IntPtr w, int x, int y);
    [DllImport(Lib)] public static extern int gmb_managed_window_resize_edge_at(IntPtr w, int x, int y);

    public static string Str(IntPtr p) => p == IntPtr.Zero ? "" : (Marshal.PtrToStringAnsi(p) ?? "");
    public static int GetInt(IntPtr w, int f) => gmb_managed_window_get_int(w, f);
    public static void SetInt(IntPtr w, int f, int v) => gmb_managed_window_set_int(w, f, v);
    public static string GetStr(IntPtr w, int f) => Str(gmb_managed_window_get_str(w, f));
    public static void SetStr(IntPtr w, int f, string? v) { var p = A(v); try { gmb_managed_window_set_str(w, f, p); } finally { Free(p); } }
    public static string GetLine(IntPtr w, int i) => Str(gmb_managed_window_get_line(w, i));
    public static int LinesCount(IntPtr w) => gmb_managed_window_lines_count(w);
    public static void LineSet(IntPtr w, int i, string? s) { var p = A(s); try { gmb_managed_window_line_set(w, i, p); } finally { Free(p); } }
    public static void LineInsert(IntPtr w, int i, string? s) { var p = A(s); try { gmb_managed_window_line_insert(w, i, p); } finally { Free(p); } }
    public static void LineRemove(IntPtr w, int i) => gmb_managed_window_line_remove(w, i);
    public static void AppendLine(IntPtr w, string? s) { var p = A(s); try { gmb_managed_window_append_line(w, p); } finally { Free(p); } }
    public static void ScrollTo(IntPtr w, int off) => gmb_managed_window_scroll_to(w, off);
    public static void ScrollBy(IntPtr w, int d) => gmb_managed_window_scroll_by(w, d);
    public static string SayAt(IntPtr w, int row, int col, string? s) { var p = A(s); try { return Str(gmb_managed_window_say_at(w, row, col, p)); } finally { Free(p); } }
    public static void BoxAt(IntPtr w, int top, int left, int bottom, int right, string? b) { var p = A(b); try { gmb_managed_window_box_at(w, top, left, bottom, right, p); } finally { Free(p); } }
    public static void SetLines(IntPtr w, IReadOnlyList<string> lines)
    {
        var n = lines.Count;
        var arr = new IntPtr[n];
        try
        {
            for (int i = 0; i < n; i++) arr[i] = A(lines[i]);
            var buf = Marshal.AllocHGlobal(IntPtr.Size * n);
            try { Marshal.Copy(arr, 0, buf, n); gmb_managed_window_set_lines(w, buf, n); }
            finally { Marshal.FreeHGlobal(buf); }
        }
        finally { foreach (var p in arr) Free(p); }
    }
}
