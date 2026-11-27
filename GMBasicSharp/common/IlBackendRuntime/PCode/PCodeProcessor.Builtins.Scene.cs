// FILE: Interpreter.IlBackendRuntime.PCode.PCodeProcessor.Builtins.Scene.cs
// Letture del dominio SCENE (motore 3D, renderer mesh) sulla gamba PCode. Come per IL, qui non
// esiste stato della scena: ogni lettura inoltra alla facciata `gmb_scene_*` del runtime C
// (CommonLib.Scene3DNative), la sola proprietaria di geometria e target (§0bis). L'arità è
// controllata dal linker (una funzione, una firma).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Builtin instradati a <see cref="ExecuteBuiltInScene"/>.</summary>
    private static readonly HashSet<BuiltInFunction> SceneBuiltInFunctions =
    [
        BuiltInFunction.SCENE_VERTS,
        BuiltInFunction.SCENE_TRIS,
        BuiltInFunction.SCENE_DRAWN,
        BuiltInFunction.SCENE_CULLED,
        BuiltInFunction.SCENE_PIXELS,
        BuiltInFunction.SCENE_HASH,
        BuiltInFunction.SCENE_PIXEL,
        BuiltInFunction.SCENE_PICK,
        // P5: camera/corpo, quote e zone.
        BuiltInFunction.SCENE_ZONE,
        BuiltInFunction.SCENE_ZONETAG,
        BuiltInFunction.SCENE_X,
        BuiltInFunction.SCENE_Y,
        BuiltInFunction.SCENE_Z,
        BuiltInFunction.SCENE_EYE,
        BuiltInFunction.SCENE_YAW,
        BuiltInFunction.SCENE_PITCH,
        BuiltInFunction.SCENE_HIT,
        BuiltInFunction.SCENE_FLOORAT,
        BuiltInFunction.SCENE_CEILAT,
        // P5/M-RAYS: raggi e stato dell'ultimo colpo.
        BuiltInFunction.SCENE_SHOOT,
        BuiltInFunction.SCENE_USE,
        BuiltInFunction.SCENE_SEES,
        BuiltInFunction.SCENE_SHOTKIND,
        BuiltInFunction.SCENE_SHOTOBJ,
        BuiltInFunction.SCENE_SHOTDIST,
        BuiltInFunction.SCENE_SHOTX,
        BuiltInFunction.SCENE_SHOTY,
        BuiltInFunction.SCENE_SHOTZ,
        // P5/M-STATS: misure della scena per nome.
        BuiltInFunction.SCENE_STATS,
    ];

    private object? ExecuteBuiltInScene(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        switch (fn)
        {
            // Contatori di geometria e dell'ultimo RENDER.
            case BuiltInFunction.SCENE_VERTS: return (double)CommonLib.Scene3DNative.Verts();
            case BuiltInFunction.SCENE_TRIS: return (double)CommonLib.Scene3DNative.Tris();
            case BuiltInFunction.SCENE_DRAWN: return (double)CommonLib.Scene3DNative.Drawn();
            case BuiltInFunction.SCENE_CULLED: return (double)CommonLib.Scene3DNative.Culled();
            case BuiltInFunction.SCENE_PIXELS: return (double)CommonLib.Scene3DNative.Pixels();
            // Hash del frame: due RENDER identici danno lo stesso numero.
            case BuiltInFunction.SCENE_HASH: return (double)CommonLib.Scene3DNative.Hash();
            // PIXEL(x, y): colore 0x00RRGGBB del target, −1 fuori dai limiti.
            case BuiltInFunction.SCENE_PIXEL:
                return (double)CommonLib.Scene3DNative.Pixel((long)ToDouble(args, 0), (long)ToDouble(args, 1));
            // PICK(x, y): id dell'oggetto nel pixel (0 fondo, 1..256 oggetto,
            // -1 senza target o fuori target, -2 buffer di SCENE PICK spento).
            case BuiltInFunction.SCENE_PICK:
                return (double)CommonLib.Scene3DNative.Pick((long)ToDouble(args, 0), (long)ToDouble(args, 1));
            // P5: camera e corpo.
            case BuiltInFunction.SCENE_X: return CommonLib.Scene3DNative.X();
            case BuiltInFunction.SCENE_Y: return CommonLib.Scene3DNative.Y();
            case BuiltInFunction.SCENE_Z: return CommonLib.Scene3DNative.Z();
            case BuiltInFunction.SCENE_EYE: return CommonLib.Scene3DNative.Eye();
            case BuiltInFunction.SCENE_YAW: return CommonLib.Scene3DNative.Yaw();
            case BuiltInFunction.SCENE_PITCH: return CommonLib.Scene3DNative.Pitch();
            case BuiltInFunction.SCENE_HIT: return CommonLib.Scene3DNative.Hit();
            // Quote e zone (punto x, y, z).
            case BuiltInFunction.SCENE_FLOORAT:
                return CommonLib.Scene3DNative.FloorAt(ToDouble(args, 0), ToDouble(args, 1), ToDouble(args, 2));
            case BuiltInFunction.SCENE_CEILAT:
                return CommonLib.Scene3DNative.CeilAt(ToDouble(args, 0), ToDouble(args, 1), ToDouble(args, 2));
            case BuiltInFunction.SCENE_ZONE:
                return (double)CommonLib.Scene3DNative.ZoneAt(ToDouble(args, 0), ToDouble(args, 1), ToDouble(args, 2));
            case BuiltInFunction.SCENE_ZONETAG:
                return CommonLib.Scene3DNative.ZoneTag((long)ToDouble(args, 0));
            // P5/M-RAYS: SHOOT/USE agiscono e tornano l'id (−1 = niente); SHOT*
            // leggono l'ultimo colpo; SEES è 1/0/−1.
            case BuiltInFunction.SCENE_SHOOT: return CommonLib.Scene3DNative.Shoot(ToDouble(args, 0));
            case BuiltInFunction.SCENE_USE: return CommonLib.Scene3DNative.Use(ToDouble(args, 0));
            case BuiltInFunction.SCENE_SEES: return (double)CommonLib.Scene3DNative.Sees((long)ToDouble(args, 0));
            case BuiltInFunction.SCENE_SHOTKIND: return CommonLib.Scene3DNative.ShotKind();
            case BuiltInFunction.SCENE_SHOTOBJ: return CommonLib.Scene3DNative.ShotObj();
            case BuiltInFunction.SCENE_SHOTDIST: return CommonLib.Scene3DNative.ShotDist();
            case BuiltInFunction.SCENE_SHOTX: return CommonLib.Scene3DNative.ShotX();
            case BuiltInFunction.SCENE_SHOTY: return CommonLib.Scene3DNative.ShotY();
            case BuiltInFunction.SCENE_SHOTZ: return CommonLib.Scene3DNative.ShotZ();
            // STATS(nome$): una misura della scena (−1 = chiave sconosciuta).
            case BuiltInFunction.SCENE_STATS: return CommonLib.Scene3DNative.Stats(args.Length > 0 ? args[0]?.ToString() ?? "" : "");
        }
        return null;
    }
}
