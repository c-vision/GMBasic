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
    // Hook opzionale di stato (gmb_state_hook.h): il C chiede, il managed risponde.
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int StateFn(IntPtr ctx, IntPtr surface, IntPtr a, IntPtr b, IntPtr c, IntPtr outBuf, int cap);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void SetStateHookFn(IntPtr fn, IntPtr ctx);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void SetConcurrentPagesFn(IntPtr csv);

    private static readonly object Gate = new();
    private static bool _probed;
    private static bool _available;
    private static string? _loadedPath;
    private static IntPtr _handle;
    private static ServeFn? _serve;
    private static FreeFn? _free;
    private static AbiFn? _abi;
    private static InitFn? _init;
    private static SetStateHookFn? _setStateHook;
    private static SetConcurrentPagesFn? _setConcurrentPages;
    // Il delegato deve restare vivo per tutta la durata del processo (il C ne tiene il puntatore).
    private static readonly StateFn StateCallback = OnStateQuery;

    /// <summary>
    /// Stato degli agenti/team/workflow/memoria del runtime managed. Firmato
    /// `(superficie, a, b, c) → testo` (`null` = non gestita → il C usa il proprio registro).
    /// Letture: testo (liste separate da `\n`). Mutazioni: `"1"`/`"0"`.
    /// Va impostato dall'host managed prima di servire la prima pagina con il motore C.
    /// </summary>
    public static Func<string, string?, string?, string?, string?>? StateProvider { get; set; }

    private static string? _concurrentPages;

    /// <summary>
    /// Elenco (CSV) delle pagine renderizzabili in PARALLELO: il loro stato passa interamente dal
    /// client managed (hook di stato), quindi la VM/C non condivide stato mutabile. Le altre
    /// pagine restano serializzate. Vedi Knowledge/BSP-Engine-Audit-Concorrenza.
    /// </summary>
    public static void SetConcurrentPages(string? csv)
    {
        _concurrentPages = csv;
        if (!Available || _setConcurrentPages is null) return;   // memorizzato: applicato al probe
        ApplyConcurrentPages(csv ?? "");
    }

    private static void ApplyConcurrentPages(string csv)
    {
        var p = Marshal.StringToCoTaskMemUTF8(csv ?? "");
        try { _setConcurrentPages!(p); } finally { Marshal.FreeCoTaskMem(p); }
    }

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
                // Hook di stato: opzionale (una lib precedente non lo esporta → si ignora).
                if (NativeLibrary.TryGetExport(_handle, "gmb_bsp_shared_set_state_hook", out var setHook))
                    _setStateHook = Marshal.GetDelegateForFunctionPointer<SetStateHookFn>(setHook);
                _setStateHook?.Invoke(Marshal.GetFunctionPointerForDelegate(StateCallback), IntPtr.Zero);
                if (NativeLibrary.TryGetExport(_handle, "gmb_bsp_shared_set_concurrent_pages", out var setPages))
                    _setConcurrentPages = Marshal.GetDelegateForFunctionPointer<SetConcurrentPagesFn>(setPages);
                if (!string.IsNullOrEmpty(_concurrentPages) && _setConcurrentPages is not null)
                    ApplyConcurrentPages(_concurrentPages);
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
        };

        // Sviluppo: <repo>/GMBasic/native/gmb_vm_c/build/ — PRIMA della cache utente: in un
        // checkout il build del repo è sempre più fresco della copia installata (una copia
        // stale in ~/.gmbasic/libs veniva caricata al posto del build: hook assenti a runtime
        // pur essendo nel codice — bug reale, visto come "AGENTASK: not registered").
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

        candidates.Add(Path.Combine(CommonLib.GmbiPaths.HomeRoot, "libs", rid, name));

        foreach (var c in candidates)
            if (File.Exists(c)) return c;
        return null;
    }

    /// <summary>
    /// Callback invocata dal runtime C: legge lo stato managed e scrive la risposta UTF-8 nel
    /// buffer fornito dal C (nessuna allocazione attraversa il confine). Ritorna la lunghezza
    /// scritta/necessaria, oppure -1 = "non gestita" (il C usa il proprio registro).
    /// </summary>
    private static int OnStateQuery(IntPtr ctx, IntPtr surface, IntPtr a, IntPtr b, IntPtr c, IntPtr outBuf, int cap)
    {
        try
        {
            var provider = StateProvider;
            if (provider is null) return -1;
            var name = Marshal.PtrToStringUTF8(surface) ?? "";
            var arg1 = a == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(a);
            var arg2 = b == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(b);
            var arg3 = c == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(c);
            var result = provider(name, arg1, arg2, arg3);
            if (result is null) return -1;
            var bytes = System.Text.Encoding.UTF8.GetBytes(result);
            if (outBuf == IntPtr.Zero || bytes.Length > cap) return bytes.Length;   // il C rialloca
            Marshal.Copy(bytes, 0, outBuf, bytes.Length);
            return bytes.Length;
        }
        catch
        {
            return -1;
        }
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
