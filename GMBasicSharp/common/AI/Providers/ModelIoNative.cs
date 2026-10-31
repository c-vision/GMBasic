// FILE: Basic.Providers.ModelIoNative — binding P/Invoke al MODEL I/O C (dylib gmb_ai_modelio).
//
// PERCHÉ (AGENTS, REGOLA PRIMARIA 2026-09-23): convert/quantize/discover sono logica pesante GIÀ
// in C (safetensors, quant MLX affine, auto-discovery architettura). Il C# la CHIAMA invece di
// spawnare gli eseguibili gmb_model_quantize / gmb-discover (una sola
// sorgente; direzione C#→C ammessa, C→C# mai).
//
// Sorgente C: native/gmb_compiler_c/src/model/gmb_modelio_shim.c (ABI stabile).
// Dylib: Resources/libs/<rid>/libgmb_ai_modelio.dylib (target CMake `gmb_ai_modelio`), risolta da
// Basic.Providers.AiNativeResolver.
using System.Runtime.InteropServices;

namespace Basic.Providers;

public static partial class ModelIoNative
{
    private const string LibName = "gmb_ai_modelio";

    static ModelIoNative()
    {
        AiNativeResolver.EnsureRegistered(typeof(ModelIoNative).Assembly);
    }

    /// <summary>Converte un checkpoint HF/MLX in GGUF (full-grid, dense, ctx 4096). 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_modelio_convert(string model, string outPath);

    /// <summary>Variante completa (range layer/esperti, dense, ctx, meta). 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_model_convert_ex(string model, string outPath,
        uint l0, uint l1, uint e0, uint e1, int dense, uint ctx, long ml, long me);

    /// <summary>Quantizza un modello HF/MLX (MLX affine RTN). `imatrix` può essere null. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_modelio_quantize(string src, string outPath, int bits, int group, string? imatrix, int expertBits);

    /// <summary>Discovery: scrive il report in `outBuf` (cap byte) e ritorna la lunghezza (>0) o -1.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_modelio_inspect(string dir, byte[] outBuf, int cap);
}
