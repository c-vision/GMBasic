// FILE: CommonLib.Numerics.ClockNative.cs
// Ponte C# → C della facciata `gmb_clock_frame_*` (dominio CLOCK, tempo a passo fisso).
//
// PERCHÉ. L'orologio è UNO per processo e vive nel runtime C (`include/gmb_clock.h`): è lo
// stesso che alimenta DOEVENTS (`gmb_form_frame(0)`). Le gambe gestite (IL, PCodeVM) non ne
// hanno una copia managed e non devono averla — due orologi divergevano appena una delle due
// implementazioni cambiava (§0bis: una sola sorgente). Qui si chiama la libreria condivisa
// `libgmb_clock` (target CMake `gmb_clock`, che compila `gmb_clock.c`).
//
// CONVENZIONE. I comandi (CLOCK PAUSE|RESUME|SCALE|FPS) sono COMANDI: non hanno valore di
// ritorno leggibile dal BASIC (§1), quindi le scritture sono `void`. Le letture (CLOCK.DT|ALPHA|
// FPS|SCALE) sono funzioni e ritornano `double`.
//
// NESSUNA LIB, NESSUN RISULTATO. Se `libgmb_clock` non è caricabile si propaga l'eccezione del
// loader (DllNotFoundException): un'alternativa gestita sarebbe uno stub, vietato (§0bis).

using System.Runtime.InteropServices;

namespace CommonLib;

public static class ClockNative
{
    private const string Lib = "gmb_clock";

    // --- letture (funzioni: ritornano un valore) ------------------------------------------
    [DllImport(Lib, EntryPoint = "gmb_clock_frame_dt", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Dt();

    [DllImport(Lib, EntryPoint = "gmb_clock_frame_alpha", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Alpha();

    [DllImport(Lib, EntryPoint = "gmb_clock_frame_fps", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Fps();

    [DllImport(Lib, EntryPoint = "gmb_clock_frame_scale", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Scale();

    // --- comandi (scritture: nessun valore di ritorno, §1) --------------------------------
    [DllImport(Lib, EntryPoint = "gmb_clock_frame_pause", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Pause(int paused);

    [DllImport(Lib, EntryPoint = "gmb_clock_frame_set_scale", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetScale(double scale);

    [DllImport(Lib, EntryPoint = "gmb_clock_frame_set_fps", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetFps(double fps);
}
