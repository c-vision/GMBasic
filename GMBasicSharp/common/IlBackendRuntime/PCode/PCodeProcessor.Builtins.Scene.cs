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
        }
        return null;
    }
}
