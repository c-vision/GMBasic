// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>COMPRESSION: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> COMPRESSIONBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.COMPRESSIONCOMPRESS,
        BuiltInFunction.COMPRESSIONDECOMPRESS,
        BuiltInFunction.COMPRESSIONGZIP,
        BuiltInFunction.COMPRESSIONUNGZIP,
        BuiltInFunction.COMPRESSIONBROTLI,
        BuiltInFunction.COMPRESSIONUNBROTLI,
        BuiltInFunction.COMPRESSIONZLIB,
        BuiltInFunction.COMPRESSIONUNZLIB,
        BuiltInFunction.COMPRESSIONBZIP2,
        BuiltInFunction.COMPRESSIONUNBZIP2,
        BuiltInFunction.COMPRESSIONDEFLATE,
        BuiltInFunction.COMPRESSIONUNDEFLATE,
        BuiltInFunction.COMPRESSIONALGORITHMS,
        BuiltInFunction.COMPRESSIONZIP,
        BuiltInFunction.COMPRESSIONUNZIP,
        BuiltInFunction.COMPRESSIONZSTD,
        BuiltInFunction.COMPRESSIONUNZSTD,
        BuiltInFunction.COMPRESSIONLZ4,
        BuiltInFunction.COMPRESSIONUNLZ4,
        BuiltInFunction.COMPRESSIONXZ,
        BuiltInFunction.COMPRESSIONUNXZ,
    };

    /// <summary>Compression: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _compressionDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> COMPRESSIONDispatch
        => _compressionDispatch ??= BuildCompressionDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildCompressionDispatch() => new()
    {
        [BuiltInFunction.COMPRESSIONCOMPRESS] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.COMPRESSIONDECOMPRESS] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.COMPRESSIONGZIP] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "GZIP");
        },
        [BuiltInFunction.COMPRESSIONUNGZIP] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "GZIP");
        },
        [BuiltInFunction.COMPRESSIONBROTLI] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "BROTLI");
        },
        [BuiltInFunction.COMPRESSIONUNBROTLI] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "BROTLI");
        },
        [BuiltInFunction.COMPRESSIONZLIB] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "ZLIB");
        },
        [BuiltInFunction.COMPRESSIONUNZLIB] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "ZLIB");
        },
        [BuiltInFunction.COMPRESSIONBZIP2] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "BZIP2");
        },
        [BuiltInFunction.COMPRESSIONUNBZIP2] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "BZIP2");
        },
        [BuiltInFunction.COMPRESSIONDEFLATE] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "DEFLATE");
        },
        [BuiltInFunction.COMPRESSIONUNDEFLATE] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "DEFLATE");
        },
        [BuiltInFunction.COMPRESSIONZSTD] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "ZSTD");
        },
        [BuiltInFunction.COMPRESSIONUNZSTD] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "ZSTD");
        },
        [BuiltInFunction.COMPRESSIONLZ4] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "LZ4");
        },
        [BuiltInFunction.COMPRESSIONUNLZ4] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "LZ4");
        },
        [BuiltInFunction.COMPRESSIONXZ] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.CompressString(ArgOr(args, 0)?.ToString() ?? "", "XZ");
        },
        [BuiltInFunction.COMPRESSIONUNXZ] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.DecompressString(ArgOr(args, 0)?.ToString() ?? "", "XZ");
        },
        [BuiltInFunction.COMPRESSIONALGORITHMS] = (task, args) =>
        {
                    return CommonLib.CompressionUtils.SupportedAlgorithms;
        },
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
