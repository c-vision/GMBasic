// FILE: CommonLib/Consoles/Tui/NativeControlDraw.cs
// Binding P/Invoke dell'API di disegno dei controlli del runtime C
// (gmb_tui_drawapi.h / gmb_tui_drawapi.c). Vive in CommonLib perché qui il resolver
// delle dylib è già attivo (stesso canale di ScreenBuffer / ClipperWmNative).
// Direzione unica: C# -> C.
using System;
using System.Runtime.InteropServices;

namespace CommonLib.Consoles.Tui;

public static class NativeControlDraw
{
    private const string Lib = "gmb_gfx";

    /// <summary>Specchio esatto di <c>gmb_draw_req_t</c>.</summary>
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Req
    {
        public int Kind, X, Y, W, H;
        public int State, Sel, Min, Max;
        public double Value;
        [MarshalAs(UnmanagedType.LPStr)] public string? Text;
        [MarshalAs(UnmanagedType.LPStr)] public string? Extra;
        public int HasFg, HasBg;
        public uint FgRgb, BgRgb;
    }

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int gmb_tui_draw_control(IntPtr buf, ref Req req);

    // ---- RichText / Paragraph (gmb_tui_richtext.c) ----
    // gmb_cell_color_t = { uint8 r,g,b; bool is_default; } → Pack 1.
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct RichColor
    {
        public byte R, G, B;
        [MarshalAs(UnmanagedType.I1)] public bool IsDefault;
    }

    /// <summary>Specchio di <c>gmb_rich_span_t</c> (char text[256] + stati).</summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Ansi)]
    public struct RichSpanC
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string Text;
        [MarshalAs(UnmanagedType.I1)] public bool HasFg;
        public RichColor Fg;
        [MarshalAs(UnmanagedType.I1)] public bool HasBg;
        public RichColor Bg;
        [MarshalAs(UnmanagedType.I1)] public bool Bold;
        [MarshalAs(UnmanagedType.I1)] public bool Italic;
        [MarshalAs(UnmanagedType.I1)] public bool Underline;
    }

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int gmb_richtext_parse(
        [MarshalAs(UnmanagedType.LPStr)] string markup, [Out] RichSpanC[] spans, int max);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int gmb_richtext_parse_color(
        [MarshalAs(UnmanagedType.LPStr)] string tok, ref RichColor outColor);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void gmb_richtext_render_to_buffer(
        IntPtr buf, int x, int y, [In] RichSpanC[] spans, int count, int maxWidth);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int gmb_richtext_render_paragraph(
        IntPtr buf, int x, int y, int w, int h, [In] RichSpanC[] spans, int count, int wrap);

    // ---- BigText: glifi e metrica nel runtime C (gmb_tui_bigfont.c) ----
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int gmb_bigtext_to_block(
        [MarshalAs(UnmanagedType.LPStr)] string text, [Out] byte[] outRows7x512);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int gmb_bigtext_measure(
        [MarshalAs(UnmanagedType.LPStr)] string? text, out int w, out int h);
}
