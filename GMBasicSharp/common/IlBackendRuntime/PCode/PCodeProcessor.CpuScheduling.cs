using Basic.Runtime.Gpm;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>
    /// Sposta un kernel numerico puro fuori dal worker P e parcheggia il PCB. Gli argomenti
    /// devono essere copiati prima della chiamata: il lavoro in background non legge stack o
    /// frame BASIC. Il pool limitato applica backpressure anche con molte goroutine concorrenti.
    /// </summary>
    private static object ParkCpuBuiltin<T>(PCodeTask task, Func<T> operation)
    {
        ParkAsync(task, PCodeWaitKind.Cpu,
            ct => new ValueTask<T>(RunCpuOperationAsync(operation, ct)),
            result => task.PushRef(result));
        return ParkedBuiltinResult;
    }

    private static async Task<T> RunCpuOperationAsync<T>(Func<T> operation, CancellationToken cancellationToken)
    {
        var entered = false;
        try
        {
            await BlockingNativeSlots.WaitAsync(cancellationToken).ConfigureAwait(false);
            entered = true;
            return await Task.Run(operation, cancellationToken).ConfigureAwait(false);
        }
        finally
        {
            if (entered) BlockingNativeSlots.Release();
        }
    }
}
