// FILE: CommonLib.Http.BspC.cs
//
// Ponte P/Invoke verso il motore BSP in C (`libgmb_bsp.dylib/so/dll`, nativo/gmb_vm_c).
//
// Direzione UNICA C# → C (AGENTS §Regola primaria): il C non conosce il managed, il C# carica
// la libreria e la chiama. Nessuna callback inversa.
//
// PERCHÉ ESISTE. Il motore BSP managed esegue la pagina in una VM P-Code/IL dentro il processo
// C#: misurato ~2,2 ms per una pagina da 47 KB. Il motore C esegue la STESSA pagina (sidecar A1
// "<pagina>.bsp.chunk") in una VM C: misurato ~0,45 ms. MA il runtime C non ha lo stato
// managed (agenti/team/workflow/memoria del server AI): una pagina che li interroga rende
// vuoto. Per questo il motore C è **opt-in** (`HTTPCONFIG.BspEngine = "c"`), con il motore
// managed come default e fallback.
//
// CARICAMENTO. Non si usa un DllImportResolver: l'assembly CommonLib ne ha già uno registrato
// per le altre librerie native (registrarne un secondo solleva "A resolver is already set").
// Qui la libreria si carica esplicitamente con NativeLibrary.Load e si risolvono gli export con
// NativeLibrary.GetExport — stesso effetto, nessun conflitto.

using System.Reflection;
using System.Runtime.InteropServices;

namespace CommonLib.Http;

/// <summary>Motore BSP da usare per una pagina.</summary>
public enum BspEngine
{
    /// <summary>Motore managed (VM P-Code/IL in-process): ha lo stato del runtime (AI, DB, …).</summary>
    Managed = 0,
    /// <summary>Motore C (VM/C): più veloce ma senza stato managed. Richiede il sidecar.</summary>
    C = 1
}

/// <summary>Ponte P/Invoke al renderer BSP in C. Thread-safe (la lib serializza internamente).</summary>
public static class BspC
{
    private const string LibName = "gmb_bsp";
    private const int RequiredAbi = 1;

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int AbiFn();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InitFn();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int ServeFn(IntPtr dir, IntPtr urlPath, IntPtr query, IntPtr host,
                                 out IntPtr outHtml, out int outStatus);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void FreeFn(IntPtr p);

    private static readonly object Gate = new();
    private static bool _probed;
    private static bool _available;
    private static string? _loadedPath;
    private static IntPtr _handle;
    private static ServeFn? _serve;
    private static FreeFn? _free;
    private static AbiFn? _abi;
    private static InitFn? _init;

    /// <summary>Percorso della libreria caricata (diagnostica), o null se non disponibile.</summary>
    public static string? LoadedPath { get { EnsureProbe(); return _loadedPath; } }

    /// <summary>True se il motore C è caricabile in questo processo.</summary>
    public static bool Available { get { EnsureProbe(); return _available; } }

    private static void EnsureProbe()
    {
        if (_probed) return;
        lock (Gate)
        {
            if (_probed) return;
            _probed = true;
            try
            {
                var path = ResolveLibraryPath();
                if (path is null) return;
                _handle = NativeLibrary.Load(path);
                _abi = Marshal.GetDelegateForFunctionPointer<AbiFn>(NativeLibrary.GetExport(_handle, "gmb_bsp_shared_abi"));
                if (_abi() != RequiredAbi) return;   // ABI sconosciuta → non usare
                _init = Marshal.GetDelegateForFunctionPointer<InitFn>(NativeLibrary.GetExport(_handle, "gmb_bsp_shared_init"));
                _serve = Marshal.GetDelegateForFunctionPointer<ServeFn>(NativeLibrary.GetExport(_handle, "gmb_bsp_shared_serve"));
                _free = Marshal.GetDelegateForFunctionPointer<FreeFn>(NativeLibrary.GetExport(_handle, "gmb_bsp_shared_free"));
                _loadedPath = path;
                _init();
                _available = true;
            }
            catch
            {
                _available = false;
            }
        }
    }

    /// <summary>
    /// Cerca `libgmb_bsp` (dylib/dll/so) accanto all'eseguibile, nella cache utente
    /// `~/.gmbasic/libs/&lt;rid&gt;` e — in sviluppo — nel build del repo
    /// (`GMBasic/native/gmb_vm_c/build`), risalendo fino alla root del monorepo.
    /// Nessuna variabile d'ambiente: posizioni canoniche.
    /// </summary>
    private static string? ResolveLibraryPath()
    {
        var name = OperatingSystem.IsMacOS() ? "libgmb_bsp.dylib"
                 : OperatingSystem.IsWindows() ? "gmb_bsp.dll"
                 : "libgmb_bsp.so";
        var rid = RuntimeInformation.ProcessArchitecture switch
        {
            Architecture.Arm64 => OperatingSystem.IsMacOS() ? "osx-arm64" : "linux-arm64",
            Architecture.X64 => OperatingSystem.IsMacOS() ? "osx-x64" : "linux-x64",
            _ => "osx-arm64"
        };

        var candidates = new List<string>
        {
            Path.Combine(AppContext.BaseDirectory, name),
            Path.Combine(CommonLib.GmbiPaths.HomeRoot, "libs", rid, name),
        };

        // Sviluppo: <repo>/GMBasic/native/gmb_vm_c/build/
        try
        {
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null)
            {
                if (File.Exists(Path.Combine(dir.FullName, "GMBasic.sln")) ||
                    Directory.Exists(Path.Combine(dir.FullName, "GMBasicSharp")))
                {
                    candidates.Add(Path.Combine(dir.FullName, "GMBasic", "native", "gmb_vm_c", "build", name));
                    break;
                }
                dir = dir.Parent;
            }
        }
        catch { /* best effort */ }

        foreach (var c in candidates)
            if (File.Exists(c)) return c;
        return null;
    }

    /// <summary>
    /// Serve un URL `.bsp` con il motore C. True = gestita (`body`/`status` valorizzati);
    /// false = non gestita (sidecar assente/non fresco, pagina non compilabile, lib non
    /// disponibile) → il chiamante deve usare il motore managed.
    /// </summary>
    public static bool TryServe(string directory, string urlPath, string? query, string? host,
        out string body, out int status)
    {
        body = "";
        status = 0;
        if (!Available || _serve == null || _free == null) return false;

        IntPtr pDir = IntPtr.Zero, pUrl = IntPtr.Zero, pQuery = IntPtr.Zero, pHost = IntPtr.Zero;
        IntPtr outHtml = IntPtr.Zero;
        try
        {
            pDir = Marshal.StringToCoTaskMemUTF8(directory);
            pUrl = Marshal.StringToCoTaskMemUTF8(urlPath);
            pQuery = string.IsNullOrEmpty(query) ? IntPtr.Zero : Marshal.StringToCoTaskMemUTF8(query);
            pHost = string.IsNullOrEmpty(host) ? IntPtr.Zero : Marshal.StringToCoTaskMemUTF8(host);

            var rc = _serve(pDir, pUrl, pQuery, pHost, out outHtml, out status);
            if (rc != 1) return false;

            body = outHtml == IntPtr.Zero ? "" : Marshal.PtrToStringUTF8(outHtml) ?? "";
            return true;
        }
        catch
        {
            return false;
        }
        finally
        {
            if (outHtml != IntPtr.Zero) _free(outHtml);
            if (pDir != IntPtr.Zero) Marshal.FreeCoTaskMem(pDir);
            if (pUrl != IntPtr.Zero) Marshal.FreeCoTaskMem(pUrl);
            if (pQuery != IntPtr.Zero) Marshal.FreeCoTaskMem(pQuery);
            if (pHost != IntPtr.Zero) Marshal.FreeCoTaskMem(pHost);
        }
    }
}
