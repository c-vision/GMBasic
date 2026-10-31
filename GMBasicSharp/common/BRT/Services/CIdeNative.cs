// FILE: Basic.Runtime.CIdeNative.cs
// Bridge P/Invoke al MODELLO IDE del runtime C (gmb_ide, libgmb_gfx — tutto in C).
// Documento multi-tab, analisi (outline/completion/goto/refs/lint), refactoring (rename),
// snippet, make (bas/lib). Direzione C# -> C (una sola sorgente: il C).
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Basic.Runtime;

public static class CIdeNative
{
    private const string Lib = "gmb_gfx";

    [DllImport(Lib)] public static extern IntPtr gmb_ide_static();
    [DllImport(Lib)] private static extern int gmb_ide_tab_open(IntPtr ide, IntPtr name, IntPtr text);
    [DllImport(Lib)] public static extern int gmb_ide_tab_close(IntPtr ide, int idx);
    [DllImport(Lib)] public static extern int gmb_ide_tab_activate(IntPtr ide, int idx);
    [DllImport(Lib)] public static extern int gmb_ide_tab_count(IntPtr ide);
    [DllImport(Lib)] private static extern IntPtr gmb_ide_tab_name(IntPtr ide, int idx);
    [DllImport(Lib)] private static extern void gmb_ide_set_text(IntPtr ide, int idx, IntPtr text);
    [DllImport(Lib)] private static extern int gmb_ide_doc_text(IntPtr ide, int idx, byte[] outBuf, int cap);
    [DllImport(Lib)] private static extern int gmb_ide_doc_lines(IntPtr ide, int idx);
    [DllImport(Lib)] private static extern IntPtr gmb_ide_doc_line(IntPtr ide, int idx, int line);
    [DllImport(Lib)] private static extern void gmb_ide_doc_set_line(IntPtr ide, int idx, int line, IntPtr text);
    [DllImport(Lib)] private static extern int gmb_ide_outline(IntPtr ide, byte[] outBuf, int cap);
    [DllImport(Lib)] private static extern int gmb_ide_complete(IntPtr ide, IntPtr prefix, byte[] outBuf, int cap);
    [DllImport(Lib)] private static extern int gmb_ide_goto_def(IntPtr ide, IntPtr name);
    [DllImport(Lib)] private static extern int gmb_ide_find_refs(IntPtr ide, IntPtr name, byte[] outBuf, int cap);
    [DllImport(Lib)] private static extern int gmb_ide_lint(IntPtr ide, byte[] outBuf, int cap);
    [DllImport(Lib)] private static extern int gmb_ide_rename(IntPtr ide, IntPtr from, IntPtr to);
    [DllImport(Lib)] private static extern int gmb_ide_snippet(IntPtr kind, byte[] outBuf, int cap);
    [DllImport(Lib)] private static extern int gmb_ide_make_bas(IntPtr ide, IntPtr outPath);
    [DllImport(Lib)] private static extern int gmb_ide_make_lib(IntPtr ide, IntPtr outPath);

    private static IntPtr A(string? s) => s is null ? IntPtr.Zero : Marshal.StringToHGlobalAnsi(s);
    private static void Free(IntPtr p) { if (p != IntPtr.Zero) Marshal.FreeHGlobal(p); }
    private static string Str(byte[] b) { int z = Array.IndexOf(b, (byte)0); if (z < 0) z = b.Length; return Encoding.UTF8.GetString(b, 0, z); }

    public static IntPtr Ide => gmb_ide_static();

    public static int TabOpen(string name, string? text)
    { var n = A(name); var t = A(text); try { return gmb_ide_tab_open(Ide, n, t); } finally { Free(n); Free(t); } }
    public static int TabClose(int idx) => gmb_ide_tab_close(Ide, idx);
    public static int TabActivate(int idx) => gmb_ide_tab_activate(Ide, idx);
    public static int TabCount() => gmb_ide_tab_count(Ide);
    public static string TabName(int idx) => Marshal.PtrToStringAnsi(gmb_ide_tab_name(Ide, idx)) ?? "";
    public static void SetText(int idx, string text) { var t = A(text); try { gmb_ide_set_text(Ide, idx, t); } finally { Free(t); } }
    public static string DocText(int idx) { var b = new byte[1 << 20]; gmb_ide_doc_text(Ide, idx, b, b.Length); return Str(b); }
    public static int DocLines(int idx) => gmb_ide_doc_lines(Ide, idx);
    public static string DocLine(int idx, int line) => Marshal.PtrToStringAnsi(gmb_ide_doc_line(Ide, idx, line)) ?? "";
    public static void SetLine(int idx, int line, string text) { var t = A(text); try { gmb_ide_doc_set_line(Ide, idx, line, t); } finally { Free(t); } }

    public static string Outline() { var b = new byte[1 << 20]; gmb_ide_outline(Ide, b, b.Length); return Str(b); }
    public static string Complete(string prefix) { var p = A(prefix); var b = new byte[1 << 16]; try { gmb_ide_complete(Ide, p, b, b.Length); return Str(b); } finally { Free(p); } }
    public static int GotoDef(string name) { var n = A(name); try { return gmb_ide_goto_def(Ide, n); } finally { Free(n); } }
    public static string FindRefs(string name) { var n = A(name); var b = new byte[1 << 16]; try { gmb_ide_find_refs(Ide, n, b, b.Length); return Str(b); } finally { Free(n); } }
    public static string Lint() { var b = new byte[1 << 20]; gmb_ide_lint(Ide, b, b.Length); return Str(b); }
    public static int Rename(string from, string to) { var f = A(from); var t = A(to); try { return gmb_ide_rename(Ide, f, t); } finally { Free(f); Free(t); } }
    public static string Snippet(string kind) { var k = A(kind); var b = new byte[4096]; try { gmb_ide_snippet(k, b, b.Length); return Str(b); } finally { Free(k); } }
    public static int MakeBas(string outPath) { var o = A(outPath); try { return gmb_ide_make_bas(Ide, o); } finally { Free(o); } }
    public static int MakeLib(string outPath) { var o = A(outPath); try { return gmb_ide_make_lib(Ide, o); } finally { Free(o); } }
}
