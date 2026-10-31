// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.Message.cs
// P-CODE VM: funzione del namespace MESSAGE — MESSAGE.DELIVERY(bus).
// Il bus è identificato dal NOME della variabile (stesso contratto dei comandi MESSAGE_OP);
// i contatori vivono in PBus e li legge MsgDelivery (PCodeProcessor.Message.cs).

using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>MESSAGE: contatori di consegna del bus (lettura, nessun effetto).</summary>
    private static readonly HashSet<BuiltInFunction> MESSAGEBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.MESSAGEDELIVERY,
    };

    private object? ExecuteBuiltInMessage(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        var handle = ArgOr(args, 0)?.ToString() ?? "";
        return fn switch
        {
            BuiltInFunction.MESSAGEDELIVERY => MsgDelivery(handle),
            _ => throw new InvalidOperationException($"MESSAGE: fn {fn} non gestito (ExecuteBuiltInMessage)."),
        };
    }
}
