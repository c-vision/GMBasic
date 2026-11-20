// FILE: Interpreter.IlBackendRuntime.IlRuntime.Clock.cs
// Dominio CLOCK (tempo a passo fisso) sulla gamba IL: qui non c'è stato né logica, solo
// l'inoltro alla facciata `gmb_clock_frame_*` del runtime C. I comandi sono void (§1: un
// comando non ritorna un valore); le letture ritornano double.

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    // --- comandi ---------------------------------------------------------------------------
    public static void ClockPause() => CommonLib.ClockNative.Pause(1);
    public static void ClockResume() => CommonLib.ClockNative.Pause(0);
    public static void ClockScale(double scale) => CommonLib.ClockNative.SetScale(scale);
    public static void ClockFps(double fps) => CommonLib.ClockNative.SetFps(fps);

    // --- letture (funzioni: ritornano un valore) --------------------------------------------
    public static double ClockDt() => CommonLib.ClockNative.Dt();
    public static double ClockAlpha() => CommonLib.ClockNative.Alpha();
    public static double ClockFpsValue() => CommonLib.ClockNative.Fps();
    public static double ClockScaleValue() => CommonLib.ClockNative.Scale();
}
