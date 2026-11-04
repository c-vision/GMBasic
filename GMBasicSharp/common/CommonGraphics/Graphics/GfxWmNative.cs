// FILE: CommonGraphics/Graphics/GfxWmNative.cs
//
// P/Invoke verso il window manager PIXEL del runtime C (gmb_tui_windowstmt.c +
// gmb_tui_windowmanager.c). La versione C# del compositor (GfxWindowManager, 777 righe con
// stato proprio) è archiviata in GMBasicMonoRepo-Deprecated/CommonGraphics/Graphics/.
using System;
using System.Runtime.InteropServices;

namespace CommonGraphics.Graphics;

internal static class GfxWmNative
{
    private const string Lib = "gmb_gfx";

    private static IntPtr A(string? s) => s is null ? IntPtr.Zero : Marshal.StringToHGlobalAnsi(s);
    private static void F(IntPtr p) { if (p != IntPtr.Zero) Marshal.FreeHGlobal(p); }
    public static string Str(IntPtr p) => p == IntPtr.Zero ? "" : (Marshal.PtrToStringAnsi(p) ?? "");

    // Manager (condiviso col TUI: le finestre PIXEL si distinguono per il flag `pixel`)
    [DllImport(Lib)] public static extern IntPtr gmb_window_manager_static();
    [DllImport(Lib)] public static extern void gmb_window_manager_sync_workarea(IntPtr m, int width, int height);
    [DllImport(Lib)] public static extern void gmb_window_manager_work_area4(IntPtr m, int[] out4);
    [DllImport(Lib)] public static extern int gmb_window_manager_current_desktop(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_switch_desktop(IntPtr m, int d);
    [DllImport(Lib)] public static extern void gmb_window_manager_move_to_desktop(IntPtr m, int id, int d);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_count(IntPtr m, int desktop);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_id_at(IntPtr m, int index);
    [DllImport(Lib)] public static extern IntPtr gmb_window_manager_get(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_remove(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_focused(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_raise(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_select(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_clear_focus(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_topmost_at(IntPtr m, int x, int y);
    [DllImport(Lib)] public static extern void gmb_window_manager_move_to(IntPtr m, int id, int x, int y);
    [DllImport(Lib)] public static extern void gmb_window_manager_resize_by(IntPtr m, int id, int dw, int dh);
    [DllImport(Lib)] public static extern void gmb_window_manager_snap_zone(IntPtr m, int id, int zone);
    [DllImport(Lib)] public static extern void gmb_window_manager_maximize_toggle(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_minimize(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_unminimize(IntPtr m, int id);
    [DllImport(Lib)] public static extern void gmb_window_manager_restore(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_is_dragging(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_is_resizing(IntPtr m);
    [DllImport(Lib)] public static extern void gmb_window_manager_start_drag(IntPtr m, int id, int x, int y);
    [DllImport(Lib)] public static extern int gmb_window_manager_update_drag(IntPtr m, int x, int y);
    [DllImport(Lib)] public static extern int gmb_window_manager_end_drag(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_start_resize(IntPtr m, int id, int x, int y, IntPtr edges);
    [DllImport(Lib)] public static extern int gmb_window_manager_update_resize(IntPtr m, int x, int y);
    [DllImport(Lib)] public static extern void gmb_window_manager_end_resize(IntPtr m);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_z(IntPtr m, int id);
    [DllImport(Lib)] public static extern int gmb_window_manager_window_focused(IntPtr m, int id);
    [DllImport(Lib)] private static extern void gmb_window_manager_set_statusbar(IntPtr m, IntPtr text);
    [DllImport(Lib)] private static extern int gmb_window_manager_has_status(IntPtr m);

    public static void SetStatusBar(IntPtr m, string? text)
    { var p = A(text); try { gmb_window_manager_set_statusbar(m, p); } finally { F(p); } }
    public static bool HasStatus(IntPtr m) => gmb_window_manager_has_status(m) != 0;

    // Window-statement model (finestre PIXEL: define/render/hit-test/mouse/colori)
    [DllImport(Lib)] public static extern int gmb_winstmt_define(IntPtr name, IntPtr title,
        int top, int left, int bottom, int right, int flags);
    [DllImport(Lib)] public static extern int gmb_winstmt_id(IntPtr name);
    [DllImport(Lib)] public static extern void gmb_winstmt_render_all();
    [DllImport(Lib)] public static extern void gmb_winstmt_desktop(int desktop);
    [DllImport(Lib)] public static extern void gmb_winstmt_pixel_client_rect(IntPtr name, int[] out4);
    [DllImport(Lib)] public static extern int gmb_winstmt_pixel_control_at(IntPtr name, int x, int y);
    [DllImport(Lib)] public static extern int gmb_winstmt_pixel_resize_edge_at(IntPtr name, int x, int y);
    [DllImport(Lib)] public static extern int gmb_winstmt_pixel_taskbar_chip_at(int x, int y);
    [DllImport(Lib)] public static extern int gmb_winstmt_pixel_handle_mouse(int kind, int x, int y);
    [DllImport(Lib)] public static extern void gmb_winstmt_set_colors(IntPtr name, uint body, uint fg, uint title);
    [DllImport(Lib)] public static extern void gmb_winstmt_set_title(IntPtr name, IntPtr title);
    [DllImport(Lib)] public static extern void gmb_winstmt_reset();

    // ManagedWindow (binding generico)
    [DllImport(Lib)] public static extern int gmb_managed_window_get_int(IntPtr w, int field);
    [DllImport(Lib)] public static extern void gmb_managed_window_set_int(IntPtr w, int field, int v);
    [DllImport(Lib)] public static extern IntPtr gmb_managed_window_get_str(IntPtr w, int field);
    [DllImport(Lib)] public static extern void gmb_managed_window_set_str(IntPtr w, int field, IntPtr v);
    [DllImport(Lib)] public static extern int gmb_managed_window_lines_count(IntPtr w);
    [DllImport(Lib)] public static extern IntPtr gmb_managed_window_line_get(IntPtr w, int i);
    [DllImport(Lib)] public static extern void gmb_managed_window_line_set(IntPtr w, int i, IntPtr s);
    [DllImport(Lib)] public static extern void gmb_managed_window_line_insert(IntPtr w, int i, IntPtr s);
    [DllImport(Lib)] public static extern void gmb_managed_window_line_remove(IntPtr w, int i);
    [DllImport(Lib)] public static extern void gmb_managed_window_lines_clear(IntPtr w);
    [DllImport(Lib)] public static extern void gmb_managed_window_client_rect_px(IntPtr w, int[] out4);

    public static int Define(string name, string title, int x, int y, int w, int h, bool shadow)
    {
        var n = A(name); var t = A(title);
        try
        {
            const int PIXEL = 4, SHADOW = 2;
            return gmb_winstmt_define(n, t, y, x, y + h, x + w, PIXEL | (shadow ? SHADOW : 0));
        }
        finally { F(n); F(t); }
    }

    public static int Id(string name) { var n = A(name); try { return gmb_winstmt_id(n); } finally { F(n); } }

    public static int ControlAt(string name, int x, int y)
    { var n = A(name); try { return gmb_winstmt_pixel_control_at(n, x, y); } finally { F(n); } }

    public static int ResizeEdgeAt(string name, int x, int y)
    { var n = A(name); try { return gmb_winstmt_pixel_resize_edge_at(n, x, y); } finally { F(n); } }

    public static void ClientRect(string name, int[] out4)
    { var n = A(name); try { gmb_winstmt_pixel_client_rect(n, out4); } finally { F(n); } }

    public static void SetColors(string name, uint body, uint fg, uint title)
    { var n = A(name); try { gmb_winstmt_set_colors(n, body, fg, title); } finally { F(n); } }

    public static void SetTitle(string name, string title)
    { var n = A(name); var t = A(title); try { gmb_winstmt_set_title(n, t); } finally { F(n); F(t); } }

    public static int GetInt(IntPtr w, int f) => gmb_managed_window_get_int(w, f);
    public static void SetInt(IntPtr w, int f, int v) => gmb_managed_window_set_int(w, f, v);
    public static string GetStr(IntPtr w, int f) => Str(gmb_managed_window_get_str(w, f));
    public static void SetStr(IntPtr w, int f, string? v) { var p = A(v); try { gmb_managed_window_set_str(w, f, p); } finally { F(p); } }
    public static string GetLine(IntPtr w, int i) => Str(gmb_managed_window_line_get(w, i));
    public static int LinesCount(IntPtr w) => gmb_managed_window_lines_count(w);
    public static void LineSet(IntPtr w, int i, string? s) { var p = A(s); try { gmb_managed_window_line_set(w, i, p); } finally { F(p); } }
    public static void LineInsert(IntPtr w, int i, string? s) { var p = A(s); try { gmb_managed_window_line_insert(w, i, p); } finally { F(p); } }
    public static void LineRemove(IntPtr w, int i) => gmb_managed_window_line_remove(w, i);
    public static void LinesClear(IntPtr w) => gmb_managed_window_lines_clear(w);

    public static int StartResize(IntPtr m, int id, int x, int y, string? edges)
    { var e = A(edges); try { return gmb_window_manager_start_resize(m, id, x, y, e); } finally { F(e); } }

    public static void SyncWorkArea(int width, int height)
    { var m = gmb_window_manager_static(); if (m != IntPtr.Zero) gmb_window_manager_sync_workarea(m, width, height); }
}
