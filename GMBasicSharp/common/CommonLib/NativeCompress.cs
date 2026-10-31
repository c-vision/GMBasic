// FILE: CommonLib.NativeCompress.cs
// Binding P/Invoke alla compressione del runtime C condiviso (native/gmb_runtime_c →
// libgmb_compress): zstd/lz4/xz + zlib/bz2/brotli dalla STESSA sorgente C. Direzione
// unica C# → C. API a stringhe (base64) — ABI stabile.
using System.Runtime.InteropServices;

namespace CommonLib;

public static class NativeCompress
{
    private const string Lib = "gmb_compress";

    static NativeCompress()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(NativeCompress).Assembly, Resolve); }
        catch { /* resolver già impostato (altra lib dell'assembly) */ }
    }

    // Module initializer: vince sui resolver per-lib dei static ctor. L'assembly CommonLib
    // contiene P/Invoke a più dylib (gmb_compress, gmb_gfx, gmb_tray, ...): il resolver deve
    // risolvere QUALSIASI nome, altrimenti la prima classe inizializzata "blocca" le altre.
    [System.Runtime.CompilerServices.ModuleInitializer]
    internal static void InitResolver()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(NativeCompress).Assembly, Resolve); }
        catch { /* già installato */ }
    }

    private static IntPtr Resolve(string name, System.Reflection.Assembly asm, DllImportSearchPath? sp)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    [DllImport(Lib, EntryPoint = "gmb_compress_b64", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr CompressB64Native(string algo, string text);

    [DllImport(Lib, EntryPoint = "gmb_decompress_b64", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr DecompressB64Native(string algo, string b64);

    [DllImport(Lib, EntryPoint = "gmb_compress_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void CompressFree(IntPtr p);

    /// <summary>True se la lib condivisa è caricabile (altrimenti il chiamante usa il fallback C#).</summary>
    public static bool Available { get; } = Probe();

    private static bool Probe()
    {
        try
        {
            var p = CompressB64Native("gzip", "probe");
            if (p != IntPtr.Zero) CompressFree(p);
            return true;
        }
        catch { return false; }
    }

    public static string CompressB64(string algo, string text)
    {
        var p = CompressB64Native(algo, text);
        if (p == IntPtr.Zero) return "";
        var s = Marshal.PtrToStringAnsi(p) ?? "";
        CompressFree(p);
        return s;
    }

    public static string DecompressB64(string algo, string b64)
    {
        var p = DecompressB64Native(algo, b64);
        if (p == IntPtr.Zero) return "";
        var s = Marshal.PtrToStringAnsi(p) ?? "";
        CompressFree(p);
        return s;
    }
}
