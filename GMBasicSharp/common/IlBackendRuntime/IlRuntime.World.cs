// FILE: Interpreter.IlBackendRuntime.IlRuntime.World.cs
// Dominio WORLD (motore 3D) sulla gamba IL: qui non c'è stato né logica, solo l'inoltro alla
// facciata `gmb_world_*` del runtime C. I comandi sono void (§1: un comando non ritorna un
// valore); le letture ritornano double.

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    // --- comandi ---------------------------------------------------------------------------
    public static void WorldLoad(string path) => CommonLib.World3DNative.Load(path);
    public static void WorldClose() => CommonLib.World3DNative.Free();
    public static void WorldTick(double seconds) => CommonLib.World3DNative.Tick(seconds);

    public static void WorldSecMove(long sector, double speed, double floorZ)
        => CommonLib.World3DNative.SecMove((int)sector, speed, floorZ);

    public static void WorldSecMoveCeil(long sector, double speed, double floorZ, double ceilZ)
        => CommonLib.World3DNative.SecMoveCeil((int)sector, speed, floorZ, ceilZ);

    public static void WorldHeight(long sector) => CommonLib.World3DNative.Height((int)sector);

    public static void WorldAnim(long sector, long first, long count, double fps)
        => CommonLib.World3DNative.Anim((int)sector, (int)first, (int)count, fps);

    public static void WorldScroll(long sector, long what, double uPerS, double vPerS)
        => CommonLib.World3DNative.Scroll((int)sector, (int)what, uPerS, vPerS);

    public static void WorldLightFx(long sector, long mode, double amount, double rate)
        => CommonLib.World3DNative.LightFx((int)sector, (int)mode, amount, rate);

    public static void WorldLamp(double x, double y, double z, double radius, double level)
        => CommonLib.World3DNative.Lamp(x, y, z, radius, level);

    // --- letture (funzioni: ritornano un valore) --------------------------------------------
    public static double WorldMoving(long sector) => CommonLib.World3DNative.Moving((int)sector);
    public static double WorldLightNow(long sector) => CommonLib.World3DNative.LightNow((int)sector);
    public static double WorldTime() => CommonLib.World3DNative.Time();
    public static double WorldFloorZ(long sector) => CommonLib.World3DNative.FloorZ((int)sector);
    public static double WorldCeilZ(long sector) => CommonLib.World3DNative.CeilZ((int)sector);
}
