// FILE: CommonLib.Numerics.Scene3DNative.cs
// Ponte C# → C della facciata `gmb_scene_*` (motore 3D, dominio SCENE — renderer mesh).
//
// PERCHÉ. Lo stato della scena vive SOLO nel runtime C (`gmb_3d_cmd.c`): geometria accumulata,
// camera, target, statistiche dell'ultimo RENDER. IL, PCodeVM e VM/C non ne hanno una copia e non
// devono averla — duplicare l'accumulo o il disegno significherebbe due scene divergenti
// (AGENTS §0bis: una sola sorgente). Le gambe gestite chiamano quindi le STESSE funzioni delle
// gambe C tramite la libreria condivisa `libgmb_3d` (target CMake `gmb_3d`, che compila
// `gmb_3d_cmd.c` e il kernel `gmb_3d_frame.c`).
//
// CONVENZIONE. I comandi (SCENE RESET|VIEW|TEXTURE|OBJECT|VERTEX|QUAD|BOX|OBJPOS|CAMERA|RENDER|
// SAVE) sono COMANDI: non hanno valore di ritorno leggibile dal BASIC (§1), quindi qui il codice
// di ritorno intero viene scartato dal chiamante e gli errori si osservano dallo stato — la stessa
// regola dei MiniTest sulla gamba nativa. Le letture (SCENE.VERTS|TRIS|DRAWN|CULLED|PIXELS|HASH|
// PIXEL) sono funzioni e ritornano un numero.
//
// NESSUNA LIB, NESSUN RISULTATO. Se `libgmb_3d` non è caricabile si propaga l'eccezione del
// loader (DllNotFoundException): un'alternativa gestita sarebbe uno stub, vietato (§0bis).

using System;
using System.Runtime.InteropServices;

namespace CommonLib;

public static class Scene3DNative
{
    private const string Lib = "gmb_3d";

    // --- comandi (rc < 0 = errore del runtime, scartato come sulle gambe C) ----------------
    [DllImport(Lib, EntryPoint = "gmb_scene_reset", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Reset();

    [DllImport(Lib, EntryPoint = "gmb_scene_view", CallingConvention = CallingConvention.Cdecl)]
    public static extern long View(long w, long h, double fovDeg);

    [DllImport(Lib, EntryPoint = "gmb_scene_checker", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Texture(long cells);

    // SCENE TEXTURE "file.png": texture da PNG (RGBA8, riga 0 in alto). Il decode e' quello
    // del runtime (`png_decode`), la texture si sostituisce solo se il file e' valido.
    [DllImport(Lib, EntryPoint = "gmb_scene_texture_file", CallingConvention = CallingConvention.Cdecl)]
    public static extern long TextureFile([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport(Lib, EntryPoint = "gmb_scene_object", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Object(long r, long g, long b);

    [DllImport(Lib, EntryPoint = "gmb_scene_vertex", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Vertex(double x, double y, double z, double u, double v);

    [DllImport(Lib, EntryPoint = "gmb_scene_quad", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Quad(long a, long b, long c, long d);

    [DllImport(Lib, EntryPoint = "gmb_scene_box", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Box(double x0, double y0, double z0, double x1, double y1, double z1);

    [DllImport(Lib, EntryPoint = "gmb_scene_objpos", CallingConvention = CallingConvention.Cdecl)]
    public static extern long ObjPos(double x, double y, double z, double yawDeg);

    [DllImport(Lib, EntryPoint = "gmb_scene_camera", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Camera(double x, double y, double z, double yawDeg, double pitchDeg);

    // SCENE LIGHT dx, dy, dz, livello: luce direzionale per faccia (normale calcolata nel
    // runtime C, in aritmetica intera dopo la quantizzazione del coseno).
    [DllImport(Lib, EntryPoint = "gmb_scene_light", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Light(double dx, double dy, double dz, long level);

    // Flag di materiale (idea #5): espongono bilinear/masked/two_sided già presenti nel kernel.
    [DllImport(Lib, EntryPoint = "gmb_scene_smooth", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Smooth(long on);

    [DllImport(Lib, EntryPoint = "gmb_scene_alpha", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Alpha(long on);

    // ── P4: lampade, fog, FAR, cielo, colormap, thing ───────────────────────────────
    // Ogni voce è la facciata C omonima: il kernel resta l'unica implementazione (§0bis).
    [DllImport(Lib, EntryPoint = "gmb_scene_lamp", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Lamp(long n, double x, double y, double z, double radius, double amount);

    [DllImport(Lib, EntryPoint = "gmb_scene_lamp_spot", CallingConvention = CallingConvention.Cdecl)]
    public static extern long LampSpot(long n, double x, double y, double z, double radius,
                                       double amount, double dx, double dy, double dz, double angle);

    [DllImport(Lib, EntryPoint = "gmb_scene_fog", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Fog(double d);

    [DllImport(Lib, EntryPoint = "gmb_scene_far", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Far(double d);

    [DllImport(Lib, EntryPoint = "gmb_scene_sky", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Sky(double repeat, double shift);

    [DllImport(Lib, EntryPoint = "gmb_scene_colormap", CallingConvention = CallingConvention.Cdecl)]
    public static extern long ColorMap();

    [DllImport(Lib, EntryPoint = "gmb_scene_thing", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Thing(long n, double x, double y, double z, double w, double h);

    [DllImport(Lib, EntryPoint = "gmb_scene_object_flags", CallingConvention = CallingConvention.Cdecl)]
    public static extern long ObjectFlags(long r, long g, long b, long flags, long light);

    [DllImport(Lib, EntryPoint = "gmb_scene_twosided", CallingConvention = CallingConvention.Cdecl)]
    public static extern long TwoSided(long on);

    // SCENE PRESENT, lato 3D: guardia GPM e frame corrente. La COPIA nella pagina vive in
    // `gmb_gfx` (`gmb_gfx_present_argb`), perché le pagine 2D appartengono a quel modulo:
    // qui e lì c'è una copia ciascuno del runtime, quindi si passa il puntatore del frame.
    [DllImport(Lib, EntryPoint = "gmb_scene_present_check", CallingConvention = CallingConvention.Cdecl)]
    private static extern long PresentCheck();

    [DllImport(Lib, EntryPoint = "gmb_scene_frame", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr SceneFrame(ref long w, ref long h);

    [DllImport("gmb_gfx", EntryPoint = "gmb_gfx_present_argb", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GfxPresentArgb(IntPtr src, int w, int h);

    /// <summary>
    /// `SCENE PRESENT`: target 3D → pagina 2D corrente. Gira dentro il task GPM che esegue
    /// il comando (GPM = unico esecutore di BASIC, §0ter); non tocca SDL, che resta al pump
    /// del main thread. Ritorna 0 quando il frame è stato copiato, negativo altrimenti.
    /// </summary>
    public static long Present()
    {
        if (PresentCheck() != 0) return -1;
        long w = 0, h = 0;
        IntPtr px = SceneFrame(ref w, ref h);
        if (px == IntPtr.Zero || w <= 0 || h <= 0) return -1;
        return GfxPresentArgb(px, (int)w, (int)h) < 0 ? -1 : 0;
    }

    [DllImport(Lib, EntryPoint = "gmb_scene_render", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Render();

    [DllImport(Lib, EntryPoint = "gmb_scene_save", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Save([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    // --- letture (funzioni: ritornano un valore) ------------------------------------------
    [DllImport(Lib, EntryPoint = "gmb_scene_verts", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Verts();

    [DllImport(Lib, EntryPoint = "gmb_scene_tris", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Tris();

    [DllImport(Lib, EntryPoint = "gmb_scene_drawn", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Drawn();

    [DllImport(Lib, EntryPoint = "gmb_scene_culled", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Culled();

    [DllImport(Lib, EntryPoint = "gmb_scene_pixels", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Pixels();

    [DllImport(Lib, EntryPoint = "gmb_scene_hash", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Hash();

    [DllImport(Lib, EntryPoint = "gmb_scene_pixel", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Pixel(long x, long y);
}
