// FILE: Interpreter.IlBackendRuntime.PCode.PCodeProcessor.Builtins.World.cs
// Letture del dominio WORLD (motore 3D) sulla gamba PCode. Come per IL, qui non esiste stato 3D:
// ogni lettura inoltra alla facciata `gmb_world_*` del runtime C (CommonLib.World3DNative), la
// sola proprietaria del mondo (§0bis). L'arità è controllata dal linker (una funzione, una firma).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Builtin instradati a <see cref="ExecuteBuiltInWorld"/>.</summary>
    private static readonly HashSet<BuiltInFunction> WorldBuiltInFunctions =
    [
        BuiltInFunction.WORLD_MOVING,
        BuiltInFunction.WORLD_LIGHTNOW,
        BuiltInFunction.WORLD_TIME,
        BuiltInFunction.WORLD_FLOORZ,
        BuiltInFunction.WORLD_CEILZ,
    ];

    private object? ExecuteBuiltInWorld(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        switch (fn)
        {
            // MOVING(settore): −1 in moto, 0 fermo (T-SECMOVE).
            case BuiltInFunction.WORLD_MOVING: return CommonLib.World3DNative.Moving((int)ToDouble(args, 0));
            // LIGHTNOW(settore): livello di luce corrente.
            case BuiltInFunction.WORLD_LIGHTNOW: return CommonLib.World3DNative.LightNow((int)ToDouble(args, 0));
            // TIME(): orologio del mondo in secondi.
            case BuiltInFunction.WORLD_TIME: return CommonLib.World3DNative.Time();
            // FLOORZ/CEILZ(settore): quote attuali in sola lettura (non fermano il mover).
            case BuiltInFunction.WORLD_FLOORZ: return CommonLib.World3DNative.FloorZ((int)ToDouble(args, 0));
            case BuiltInFunction.WORLD_CEILZ: return CommonLib.World3DNative.CeilZ((int)ToDouble(args, 0));
        }
        return null;
    }
}
