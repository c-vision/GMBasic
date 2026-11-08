// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.Compression.cs
//
// COMPRESSION: dopo la migrazione a pacchetto FFI (`compression`) restano QUI
// solo ZIP/UNZIP — archivi STORE del runtime condiviso (usati anche da package
// manager e brains). Una sola implementazione per funzione: nessun handler
// builtin per COMPRESS/GZIP/ZSTD/... (vivono in libgmbcompression).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>COMPRESSION: restano solo i builtin ZIP/UNZIP (archivi).</summary>
    private static readonly HashSet<BuiltInFunction> COMPRESSIONBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.COMPRESSIONZIP,
        BuiltInFunction.COMPRESSIONUNZIP,
    };

    /// <summary>Compression: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch.</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _compressionDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> COMPRESSIONDispatch
        => _compressionDispatch ??= BuildCompressionDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildCompressionDispatch() => new()
    {
        [BuiltInFunction.COMPRESSIONZIP] = (task, args) =>
        {
                {
                    var dest = ArgOr(args, 1)?.ToString() ?? "";
                    var sources = new System.Collections.Generic.List<string>();
                    if (args.Length > 0 && args[0] is System.Collections.IList srcList)
                {
                    foreach (var it in srcList) if (it != null) sources.Add(it.ToString() ?? "");
                }
                    else if (args.Length > 0 && args[0] is string srcStr)
                {
                    foreach (var part in srcStr.Split(';'))
                    if (!string.IsNullOrWhiteSpace(part)) sources.Add(part.Trim());
                }
                    // GPM (§0ter): le operazioni bloccanti si eseguono FUORI dal worker (Task.Run) e il
                    // task verde si PARCHEGGIA (ParkBlocking). Un worker non può mai bloccarsi: altrimenti
                    // un COMPRESSION.ZIP di un file grande congela il worker e, saturando i worker della
                    // GPM, blocca anche gli altri handler HTTP (es. il download del registry).
                    ParkBlocking(task,
                        () => (object?)(CompressionZipSync(dest, sources)),
                        result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
        [BuiltInFunction.COMPRESSIONUNZIP] = (task, args) =>
        {
                {
                    var archive = ArgOr(args, 0)?.ToString() ?? "";
                    var dir = ArgOr(args, 1)?.ToString() ?? "";
                    // stessa regola GPM: unzip fuori dal worker, task verde parcheggiato.
                    ParkBlocking(task,
                        () => (object?)(CompressionUnzipSync(archive, dir)),
                        result => task.PushRef(result));
                    return ParkedBuiltinResult;
                }
        },
    };

    private object? ExecuteBuiltInCompression(PCodeTask task, BuiltInFunction fn, object?[] args)
        => COMPRESSIONDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInCompression (fn={fn}).");


    /// <summary>COMPRESSION.ZIP sincrono (eseguito fuori dal worker GPM via ParkBlocking).</summary>
    private static double CompressionZipSync(string dest, List<string> sources)
    {
        try { CommonLib.CompressionUtils.CreateZip(dest, sources); return -1.0; }
        catch { return 0.0; }
    }

    /// <summary>COMPRESSION.UNZIP sincrono (eseguito fuori dal worker GPM via ParkBlocking).</summary>
    private static double CompressionUnzipSync(string archive, string dir)
    {
        try { CommonLib.CompressionUtils.ExtractZip(archive, dir); return -1.0; }
        catch { return 0.0; }
    }
}
