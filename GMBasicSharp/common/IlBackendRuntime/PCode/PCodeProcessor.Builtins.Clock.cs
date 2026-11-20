// FILE: Interpreter.IlBackendRuntime.PCode.PCodeProcessor.Builtins.Clock.cs
// Letture del dominio CLOCK (tempo a passo fisso) sulla gamba PCode. Come per IL, qui non esiste
// un orologio managed: ogni lettura inoltra alla facciata `gmb_clock_frame_*` del runtime C
// (CommonLib.ClockNative), la sola proprietaria del tempo (§0bis). L'arità è controllata dal
// linker (una funzione, una firma): le letture non accettano argomenti.

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Builtin instradati a <see cref="ExecuteBuiltInClock"/>.</summary>
    private static readonly HashSet<BuiltInFunction> ClockBuiltInFunctions =
    [
        BuiltInFunction.CLOCK_DT,
        BuiltInFunction.CLOCK_ALPHA,
        BuiltInFunction.CLOCK_FPS,
        BuiltInFunction.CLOCK_SCALE,
    ];

    private object? ExecuteBuiltInClock(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        switch (fn)
        {
            // DT(): secondi per passo (1 / FPS nominale).
            case BuiltInFunction.CLOCK_DT: return CommonLib.ClockNative.Dt();
            // ALPHA(): residuo 0..1 che non basta a un passo.
            case BuiltInFunction.CLOCK_ALPHA: return CommonLib.ClockNative.Alpha();
            // FPS(): passi al secondo nominali.
            case BuiltInFunction.CLOCK_FPS: return CommonLib.ClockNative.Fps();
            // SCALE(): moltiplicatore del tempo reale.
            case BuiltInFunction.CLOCK_SCALE: return CommonLib.ClockNative.Scale();
        }
        return null;
    }
}
