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
