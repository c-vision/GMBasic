// FILE: CommonLib/GmbBoot.cs
// UNICO ponte del mondo C# verso il risolutore delle librerie native.
//
// REGOLA (direttiva 2026-10-29). Il percorso di ricerca/caricamento delle librerie vive SOLO nel
// runtime C (`libgmb_boot`): il C# non cerca più nulla e non registra più un resolver per ogni
// classe che fa P/Invoke. Qui c'è l'unica eccezione tecnica ammessa — trovare `libgmb_boot`
// stessa (chicken-and-egg: serve una funzione per caricare la lib che sa caricare le lib) — e
// poi tutte le decisioni sono delegate al C.
//
// USO
//     var p = GmbBoot.LibPath("gmb_gfx");     // il C decide dove sta (e la scarica se manca)
//     if (p is null) throw new DllNotFoundException(GmbBoot.LastError);
//     GmbBoot.Open("gmb_gfx");                 // oppure: path + dlopen fatti dal C
//
// Le funzioni sono prese con `NativeLibrary.GetExport` (niente `DllImport("gmb_boot")`): così non
// serve alcun assemblaggio di nome e il caricamento è esplicito, con errore chiaro se manca.

using System.Runtime.InteropServices;

namespace CommonLib;

public static class GmbBoot
{
    private const string LibName = "libgmb_boot";

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate IntPtr RidFn();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int BufFn(byte[] buf, nuint cap);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int NameFn([MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int NameBufFn([MarshalAs(UnmanagedType.LPUTF8Str)] string name, byte[] buf, nuint cap);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ScopeFn(int scope);

    private static IntPtr _handle;
    private static RidFn? _rid;
    private static BufFn? _cache;
    private static NameBufFn? _libPath;
    private static NameBufFn? _libPathAny;
    private static NameFn? _libOpen;
    private static NameBufFn? _component;
    private static ScopeFn? _bootstrap;

    /// <summary>Ultimo errore (caricamento della boot lib o chiamata C fallita).</summary>
    public static string? LastError { get; private set; }

    /// <summary>Scope del bootstrap (devono combaciare con `gmb_runtime.h`).</summary>
    [Flags]
    public enum Scope { Runtime = 1, Dev = 2, Components = 4, All = 8 }

    /// <summary>Carica `libgmb_boot` e risolve gli entry point (idempotente).</summary>
    public static bool EnsureLoaded()
    {
        if (_handle != IntPtr.Zero) return true;
        var candidates = FindBootLibs(out var tried);
        if (candidates.Count == 0)
        {
            LastError = "libgmb_boot non trovata; cercata in:\n  " + string.Join("\n  ", tried) +
                        "\n  (rimedio: `gmb install` oppure indica la cache con paths.libsCacheDir)";
            return false;
        }
        // VALIDAZIONE del candidato: una copia VECCHIA nella cache può non avere gli export
        // correnti (caso reale: boot pre-`gmb_lib_path_any` → "Unable to find an entry point").
        // Se il caricamento o la risoluzione degli export fallisce, si prova il candidato dopo.
        foreach (var path in candidates)
        {
            try { return LoadFrom(path); }
            catch (Exception ex)
            {
                LastError = $"libgmb_boot non utilizzabile ({path}): {ex.GetBaseException().Message}";
                try { if (_handle != IntPtr.Zero) NativeLibrary.Free(_handle); } catch { }
                _handle = IntPtr.Zero;
            }
        }
        return false;
    }

    private static bool LoadFrom(string path)
    {
        {
            _handle = NativeLibrary.Load(path);
            _rid = Get<RidFn>("gmb_rid");
            _cache = Get<BufFn>("gmb_cache");
            _libPath = Get<NameBufFn>("gmb_lib_path");
            _libPathAny = Get<NameBufFn>("gmb_lib_path_any");
            _libOpen = Get<NameFn>("gmb_lib_open");
            _component = Get<NameBufFn>("gmb_component");
            _bootstrap = Get<ScopeFn>("gmb_bootstrap");
            return true;
        }
    }

    private static T Get<T>(string symbol) where T : Delegate
        => Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(_handle, symbol));

    /// <summary>L'UNICA ricerca rimasta nel C#: la boot lib stessa. Ordine:
    /// cache del RID → accanto all'eseguibile (deploy offline) → checkout (sviluppo).</summary>
    private static List<string> FindBootLibs(out List<string> tried)
    {
        tried = new List<string>();
        var file = OperatingSystem.IsWindows() ? "gmb_boot.dll"
                 : OperatingSystem.IsMacOS() ? "libgmb_boot.dylib" : "libgmb_boot.so";
        var candidates = new List<string>();

        // 1) cache canonica (GmbiPaths.LibsCacheRoot/<rid>) — la stessa che usa il C
        try { candidates.Add(Path.Combine(GmbiPaths.LibsCacheRoot, LibsResolver.Rid(), file)); } catch { }
        // 2) accanto all'eseguibile (deploy offline: `-all` la copia lì)
        candidates.Add(Path.Combine(AppContext.BaseDirectory, file));
        // 3) checkout (sviluppo): libs-runtime/<rid> — sparisce in P3, resta come fallback dev
        try { candidates.Add(Path.Combine(GmbiPaths.RepoRoot, "build", "libs", LibsResolver.Rid(), file)); } catch { }
        candidates.Add(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "build", "libs", LibsResolver.Rid(), file));

        // SVILUPPO: se la copia in `build/libs/<rid>` è PIÙ NUOVA di quella in cache, va caricata
        // quella. La boot lib è il PUNTO UNICO di risoluzione delle altre: caricando la copia
        // vecchia dalla cache, tutte le lib AI verrebbero risolte con la logica vecchia (bug
        // (open) - Lib-AI-Pubblicate-Stale-Cache-IL-PCode: cache runtime-v1 ≠ sorgenti correnti).
        // La regola è la stessa del C (`gmb_lib_path`): vince il file con mtime più recente.
        try
        {
            var rid = LibsResolver.Rid();
            var cacheP = Path.GetFullPath(Path.Combine(GmbiPaths.LibsCacheRoot, rid, file));
            var buildP = Path.GetFullPath(Path.Combine(GmbiPaths.RepoRoot, "build", "libs", rid, file));
            if (File.Exists(cacheP) && File.Exists(buildP)
                && File.GetLastWriteTimeUtc(buildP) > File.GetLastWriteTimeUtc(cacheP))
            {
                candidates.RemoveAll(c => string.Equals(Path.GetFullPath(c), cacheP, StringComparison.Ordinal));
                candidates.Insert(0, buildP);
            }
        }
        catch { /* niente build dir / path non espandibili: si resta sull'ordine di prima */ }

        var found = new List<string>();
        foreach (var c in candidates)
        {
            var full = Path.GetFullPath(c);
            tried.Add(full);
            if (File.Exists(full) && !found.Contains(full)) found.Add(full);
        }
        return found;
    }

    /// <summary>RID del processo (deciso dal C: una sola mappatura).</summary>
    public static string Rid()
    {
        if (!EnsureLoaded() || _rid is null) return FallbackRid();
        try { return Marshal.PtrToStringUTF8(_rid()) ?? FallbackRid(); } catch { return FallbackRid(); }
    }

    private static string FallbackRid()
        => OperatingSystem.IsMacOS()
            ? (RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64")
            : OperatingSystem.IsWindows() ? "win-x64"
            : (RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "linux-arm64" : "linux-x64");

    /// <summary>Cache delle librerie (`~/.gmbasic/libs`), decisa dal C.</summary>
    public static string? CacheDir()
    {
        if (!EnsureLoaded() || _cache is null) return null;
        var buf = new byte[2048];
        return _cache(buf, (nuint)buf.Length) == 1 ? Utf8(buf) : null;
    }

    /// <summary>Path assoluto della libreria nella cache (null se non c'è).</summary>
    public static string? LibPath(string name)
    {
        if (!EnsureLoaded() || _libPath is null) return null;
        var buf = new byte[2600];
        return _libPath(name, buf, (nuint)buf.Length) == 1 ? Utf8(buf) : null;
    }

    /// <summary>Path + download on-demand + dlopen, tutto nel C. true = caricata.</summary>
    public static bool Open(string name)
    {
        if (!EnsureLoaded() || _libOpen is null) return false;
        try { return _libOpen(name) == 1; } catch { return false; }
    }

    private static readonly Dictionary<string, IntPtr> _resolved = new(StringComparer.Ordinal);

    /// <summary>Risoluzione per `DllImport`: dal nome usato in `[DllImport("SDL2")]` al modulo
    /// già caricato. La mappatura nome→file e la ricerca stanno nel C (`gmb_lib_path_any`);
    /// qui c'è solo la cache e `NativeLibrary.Load`. `IntPtr.Zero` = "non mia, prosegui col
    /// probing standard" (nulla viene mai nascosto).</summary>
    public static IntPtr ResolveDllImport(string name)
    {
        if (string.IsNullOrEmpty(name)) return IntPtr.Zero;
        lock (_resolved)
        {
            if (_resolved.TryGetValue(name, out var h)) return h;
        }
        // macOS: i FRAMEWORK di sistema non sono dylib e non stanno nei nostri percorsi.
        // `[DllImport("ApplicationServices")]` (COMPUTER/CUA gestito: CGEvent, AXUIElement)
        // falliva con "Unable to load shared library 'ApplicationServices'": il probing .NET
        // non cerca dentro /System/Library/Frameworks. Se il framework esiste, si carica lì.
        // Su altre piattaforme il ramo è inerte (nessun path).
        if (!name.Contains('/') && !name.Contains('.'))
        {
            var fw = $"/System/Library/Frameworks/{name}.framework/{name}";
            if (File.Exists(fw))
            {
                try
                {
                    var fh = NativeLibrary.Load(fw);
                    lock (_resolved) _resolved[name] = fh;
                    return fh;
                }
                catch (Exception ex)
                {
                    LastError = $"framework '{name}' trovato in {fw} ma non caricabile: {ex.GetBaseException().Message}";
                    return IntPtr.Zero;
                }
            }
        }
        if (!EnsureLoaded() || _libPathAny is null) return IntPtr.Zero;
        byte[] buf = new byte[2600];
        if (_libPathAny(name, buf, (nuint)buf.Length) != 1) return IntPtr.Zero;
        var path = Utf8(buf);
        try
        {
            var handle = NativeLibrary.Load(path);
            lock (_resolved) _resolved[name] = handle;
            return handle;
        }
        catch (Exception ex)
        {
            LastError = $"'{name}' trovata in {path} ma non caricabile: {ex.GetBaseException().Message}";
            return IntPtr.Zero;
        }
    }

    /// <summary>Path assoluto risolvendo il nome come fa P/Invoke (lib<name>.<ext>, …).</summary>
    public static string? LibPathAny(string name)
    {
        if (!EnsureLoaded() || _libPathAny is null) return null;
        var buf = new byte[2600];
        return _libPathAny(name, buf, (nuint)buf.Length) == 1 ? Utf8(buf) : null;
    }

    /// <summary>Cartella di un componente esterno (torch/chromium/obscura/ocr).</summary>
    public static string? ComponentDir(string name)
    {
        if (!EnsureLoaded() || _component is null) return null;
        var buf = new byte[2600];
        return _component(name, buf, (nuint)buf.Length) == 1 ? Utf8(buf) : null;
    }

    /// <summary>Bootstrap: scarica l'indice piatto del RID e i file dello scope (con progress
    /// bar su terminale). true = tutti i file richiesti sono in cache.</summary>
    public static bool Bootstrap(Scope scope = Scope.Runtime)
    {
        if (!EnsureLoaded() || _bootstrap is null)
        {
            LastError ??= "libgmb_boot non disponibile: impossibile scaricare le librerie";
            return false;
        }
        try { return _bootstrap((int)scope) != 0; }
        catch (Exception ex) { LastError = ex.GetBaseException().Message; return false; }
    }

    private static string Utf8(byte[] buf)
    {
        var end = Array.IndexOf(buf, (byte)0);
        if (end < 0) end = buf.Length;
        return System.Text.Encoding.UTF8.GetString(buf, 0, end);
    }
}
