// FILE: CommonLib.Numerics.World3DNative.cs
// Ponte C# → C della facciata `gmb_world_*` (motore 3D, dominio WORLD).
//
// PERCHÉ. Lo stato 3D vive SOLO nel runtime C (`gmb_3d_*`): settori, portali, mover, tempo,
// lampade. IL, PCodeVM e VM/C non ne hanno una copia e non devono averla — duplicare il
// caricatore o il mover significherebbe due mondi divergenti (AGENTS §0bis: una sola
// sorgente). Le gambe gestite chiamano quindi le STESSE funzioni delle gambe C tramite la
// libreria condivisa `libgmb_3d` (target CMake `gmb_3d`, che compila `gmb_3d_cmd.c`).
//
// CONVENZIONE. I comandi (WORLD LOAD|CLOSE|TICK|SECMOVE|HEIGHT|ANIM|SCROLL|LIGHTFX|LAMP) sono
// COMANDI: non hanno valore di ritorno leggibile dal BASIC (§1), quindi qui il codice di
// ritorno intero viene scartato dal chiamante e gli errori si osservano dallo stato — la stessa
// regola dei MiniTest sulla gamba nativa. Le letture (WORLD.MOVING|LIGHTNOW|TIME|FLOORZ|CEILZ)
// sono funzioni e ritornano `double`.
//
// NESSUNA LIB, NESSUN RISULTATO. Se `libgmb_3d` non è caricabile si propaga l'eccezione del
// loader (DllNotFoundException): un'alternativa gestita sarebbe uno stub, vietato (§0bis).

using System;
using System.Runtime.InteropServices;

namespace CommonLib;

public static class World3DNative
{
    private const string Lib = "gmb_3d";

    // --- comandi (rc < 0 = errore del runtime, scartato come sulle gambe C) ----------------
    [DllImport(Lib, EntryPoint = "gmb_world_load", CallingConvention = CallingConvention.Cdecl)]
    public static extern int Load([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport(Lib, EntryPoint = "gmb_world_free", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Free();

    [DllImport(Lib, EntryPoint = "gmb_world_tick", CallingConvention = CallingConvention.Cdecl)]
    public static extern int Tick(double seconds);

    [DllImport(Lib, EntryPoint = "gmb_world_secmove", CallingConvention = CallingConvention.Cdecl)]
    public static extern int SecMove(int sector, double speed, double floorZ);

    [DllImport(Lib, EntryPoint = "gmb_world_secmove_ceil", CallingConvention = CallingConvention.Cdecl)]
    public static extern int SecMoveCeil(int sector, double speed, double floorZ, double ceilZ);

    [DllImport(Lib, EntryPoint = "gmb_world_height", CallingConvention = CallingConvention.Cdecl)]
    public static extern int Height(int sector);

    [DllImport(Lib, EntryPoint = "gmb_world_anim", CallingConvention = CallingConvention.Cdecl)]
    public static extern int Anim(int sector, int first, int count, double fps);

    [DllImport(Lib, EntryPoint = "gmb_world_scroll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int Scroll(int sector, int what, double uPerS, double vPerS);

    [DllImport(Lib, EntryPoint = "gmb_world_lightfx", CallingConvention = CallingConvention.Cdecl)]
    public static extern int LightFx(int sector, int mode, double amount, double rate);

    [DllImport(Lib, EntryPoint = "gmb_world_lamp", CallingConvention = CallingConvention.Cdecl)]
    public static extern int Lamp(double x, double y, double z, double radius, double level);

    // --- letture (funzioni: ritornano un valore) ------------------------------------------
    [DllImport(Lib, EntryPoint = "gmb_world_moving", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Moving(int sector);

    [DllImport(Lib, EntryPoint = "gmb_world_lightnow", CallingConvention = CallingConvention.Cdecl)]
    public static extern double LightNow(int sector);

    [DllImport(Lib, EntryPoint = "gmb_world_time", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Time();

    [DllImport(Lib, EntryPoint = "gmb_world_floor", CallingConvention = CallingConvention.Cdecl)]
    public static extern double FloorZ(int sector);

    [DllImport(Lib, EntryPoint = "gmb_world_ceil", CallingConvention = CallingConvention.Cdecl)]
    public static extern double CeilZ(int sector);
}
