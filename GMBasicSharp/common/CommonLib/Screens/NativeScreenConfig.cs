// FILE: CommonLib/Screens/NativeScreenConfig.cs
// Binding P/Invoke della configurazione schermo C (gmb_screen_config.h): la logica di
// GetDefault e delle palette vive nel runtime C; ScreenConfig.cs e' un proxy.
using System;
using System.Runtime.InteropServices;

namespace CommonLib.Screens;

public static class NativeScreenConfig
{
    private const string Lib = "gmb_gfx";

    [StructLayout(LayoutKind.Sequential)]
    public struct Cfg
    {
        public int mode;
        public int logical_width, logical_height;
        public int char_grid_width, char_grid_height;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 768)] public byte[] palette;
        public int palette_size;
        public IntPtr palette_name;
        public int default_fg_color, default_bg_color;
        public int window_width, window_height, window_x, window_y;
        public IntPtr font_path;
        public int font_size;
        public int font_hinting;
        [MarshalAs(UnmanagedType.I1)] public bool use_logical_size_scaling;
        [MarshalAs(UnmanagedType.I1)] public bool use_native_resolution_rendering;
        public int window_mode;
    }

    [DllImport(Lib)] public static extern Cfg gmb_screen_config_get_default(int mode);
    [DllImport(Lib)] public static extern void gmb_screen_config_apply(ref Cfg cfg);

    [DllImport(Lib)] public static extern void gmb_screen_palette_vga16([Out] byte[] out48);
    [DllImport(Lib)] public static extern void gmb_screen_palette_vga256([Out] byte[] out768);
    [DllImport(Lib)] public static extern void gmb_screen_palette_c64([Out] byte[] out48);
    [DllImport(Lib)] public static extern void gmb_screen_palette_c64_crt([Out] byte[] out48);
    [DllImport(Lib)] public static extern void gmb_screen_palette_c64_classic([Out] byte[] out48);
    [DllImport(Lib, CharSet = CharSet.Ansi)]
    public static extern int gmb_screen_palette_named([MarshalAs(UnmanagedType.LPStr)] string name, [Out] byte[] out48);
    [DllImport(Lib)] public static extern int gmb_screen_palette_named_list([Out] byte[] names, int cap);

    public static byte[][] Rgb(byte[] flat, int count)
    {
        var n = Math.Max(0, Math.Min(count, flat.Length / 3));
        var outv = new byte[n][];
        for (int i = 0; i < n; i++) outv[i] = [flat[i * 3], flat[i * 3 + 1], flat[i * 3 + 2]];
        return outv;
    }

    public static string? Str(IntPtr p) =>
        p == IntPtr.Zero ? null : (Marshal.PtrToStringUTF8(p) ?? Marshal.PtrToStringAnsi(p));

    public static string[] NamedList()
    {
        const int cap = 64, stride = 32;
        var buf = new byte[cap * stride];
        var n = gmb_screen_palette_named_list(buf, cap);
        var names = new string[Math.Max(0, n)];
        for (int i = 0; i < n; i++)
        {
            var s = 0; while (s < stride && buf[i * stride + s] != 0) s++;
            names[i] = System.Text.Encoding.UTF8.GetString(buf, i * stride, s);
        }
        return names;
    }
}
