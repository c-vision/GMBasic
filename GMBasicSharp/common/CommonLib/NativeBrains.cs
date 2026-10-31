// FILE: CommonLib/NativeBrains.cs
// Binding P/Invoke dei CERVELLI (native/gmb_runtime_c/src/gmb_brains.c, esportato da libgmb_pkg).
// Direzione unica C#→C: la RISOLUZIONE di un cervello e il suo DOWNLOAD vivono in C (una sola
// implementazione per i 4 backend); qui non c'è una copia della logica.
//
// Perché serve una risoluzione in C: un cervello può stare in <progetto>/vendor/brains, accanto
// all'eseguibile o nella cache ~/.gmbasic/brains (dove lo mette `AI BRAIN INSTALL`). Se il C# si
// limitasse a AiConfig.BrainsDefaultDir, un cervello scaricato dal package manager NON verrebbe
// visto dagli agenti.
using System.Runtime.InteropServices;

namespace CommonLib;

public static class NativeBrains
{
    private const string Lib = "gmb_pkg";

    static NativeBrains()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(NativeBrains).Assembly, Resolve); }
        catch { /* resolver già impostato (altra lib dell'assembly) */ }
    }

    private static IntPtr Resolve(string name, System.Reflection.Assembly asm, DllImportSearchPath? sp)
        => GmbBoot.ResolveDllImport(name);

    private static string CStr(byte[] buf)
    {
        var n = Array.IndexOf(buf, (byte)0);
        if (n < 0) n = buf.Length;
        return System.Text.Encoding.UTF8.GetString(buf, 0, n);
    }

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_lookup", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_lookup(string name, string? version, [In, Out] byte[] outbuf, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_versions", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_versions(string name, [In, Out] byte[] outbuf, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_list", CallingConvention = CallingConvention.Cdecl)]
    private static extern int brain_list([In, Out] byte[] outbuf, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_modes", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_modes(string dir, string name, [In, Out] byte[] outbuf, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_search_path", CallingConvention = CallingConvention.Cdecl)]
    private static extern int brain_search_path([In, Out] byte[] outbuf, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_manifest_path", CallingConvention = CallingConvention.Cdecl)]
    private static extern int brain_manifest_path([In, Out] byte[] outbuf, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_available", CallingConvention = CallingConvention.Cdecl)]
    private static extern int brain_available([In, Out] byte[] outbuf, UIntPtr cap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_install", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_install(string name, string? version,
                                            [In, Out] byte[] outbuf, UIntPtr cap, [In, Out] byte[] errbuf, UIntPtr ecap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_install_source", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_install_source(string name, string source,
                                                   [In, Out] byte[] outbuf, UIntPtr cap, [In, Out] byte[] errbuf, UIntPtr ecap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_remove", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_remove(string name, string? version);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_emit_index", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_emit_index(string appSettings, string manifestPath, string outPath);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_has_rag", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_has_rag(string dir, string name);

    [DllImport(Lib, EntryPoint = "gmb_pkg_brain_has_graph", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int brain_has_graph(string dir, string name);

    // TEST-ONLY: limita la ricerca a una radice, per isolare i test dai cervelli REALMENTE
    // installati sulla macchina (~/.gmbasic/brains). AsyncLocal: non inquina altri test in
    // parallelo. null = comportamento normale (risoluzione C: vendor → exe → brainsDir → cache).
    private static readonly System.Threading.AsyncLocal<string?> _rootsOverride = new();

    /// <summary>TEST-ONLY: vedi <see cref="_rootsOverride"/>.</summary>
    public static void OverrideRootsForTests(string? root) => _rootsOverride.Value = root;

    /// <summary>Directory del cervello installato (vendor → exe → brainsDir → cache).
    /// <paramref name="version"/> null/vuoto = la più alta. null se non trovato o lib assente.</summary>
    public static string? Lookup(string name, string? version = null)
    {
        var roots = _rootsOverride.Value;
        if (roots is not null) return LookupIn(roots, name, version);
        try
        {
            var buf = new byte[4096];
            return brain_lookup(name, version, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : null;
        }
        catch { return null; }   // dylib non disponibile: il chiamante ricade sul comportamento storico
    }

    private static string? LookupIn(string roots, string name, string? version)
    {
        try
        {
            var baseDir = Path.Combine(roots, name);
            if (!string.IsNullOrEmpty(version))
            {
                var d = Path.Combine(baseDir, version);
                return IsBrainDir(d) ? d : null;
            }
            if (IsBrainDir(baseDir)) return baseDir;                 // non versionato
            if (!Directory.Exists(baseDir)) return null;
            foreach (var d in Directory.GetDirectories(baseDir).OrderByDescending(x => x, StringComparer.Ordinal))
                if (IsBrainDir(d)) return d;
            return null;
        }
        catch { return null; }
    }

    private static bool IsBrainDir(string dir)
        => Directory.Exists(Path.Combine(dir, "rag")) || Directory.Exists(Path.Combine(dir, "graphs"))
           || File.Exists(Path.Combine(dir, "brain.manifest.json"));

    /// <summary>Versioni installate, crescenti ("1.0.0;1.2.0").</summary>
    public static string[] Versions(string name)
    {
        try
        {
            var buf = new byte[4096];
            if (brain_versions(name, buf, (UIntPtr)buf.Length) != 1) return [];
            return CStr(buf).Split(';', StringSplitOptions.RemoveEmptyEntries);
        }
        catch { return []; }
    }

    /// <summary>Nomi dei cervelli trovati (tutte le radici).</summary>
    public static string[] List()
    {
        try
        {
            var buf = new byte[16384];
            if (brain_list(buf, (UIntPtr)buf.Length) < 1) return [];
            var outList = new List<string>();
            foreach (var e in CStr(buf).Split(';', StringSplitOptions.RemoveEmptyEntries))
            {
                var at = e.LastIndexOf('@');
                outList.Add(at > 0 ? e[..at] : e);
            }
            return outList.Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        }
        catch { return []; }
    }

    /// <summary>Modalità disponibili del cervello: "rag", "graph", "rag,graph" o "".</summary>
    public static string Modes(string dir, string name)
    {
        try
        {
            var buf = new byte[64];
            return brain_modes(dir, name, buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
        }
        catch { return ""; }
    }

    public static string SearchPath()
    {
        try
        {
            var buf = new byte[8192];
            return brain_search_path(buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
        }
        catch { return ""; }
    }

    public static string ManifestPath()
    {
        try
        {
            var buf = new byte[4096];
            return brain_manifest_path(buf, (UIntPtr)buf.Length) == 1 ? CStr(buf) : "";
        }
        catch { return ""; }
    }

    public static string Available()
    {
        try
        {
            var buf = new byte[16384];
            return brain_available(buf, (UIntPtr)buf.Length) > 0 ? CStr(buf) : "";
        }
        catch { return ""; }
    }

    /// <summary>Installa dal manifest di sistema. Ritorna (ok, dir, errore).</summary>
    public static (bool Ok, string Root, string Error) Install(string name, string? version = null)
    {
        try
        {
            var outb = new byte[4096];
            var errb = new byte[512];
            var rc = brain_install(name, version, outb, (UIntPtr)outb.Length, errb, (UIntPtr)errb.Length);
            return (rc == 0, rc == 0 ? CStr(outb) : "", rc == 0 ? "" : CStr(errb));
        }
        catch (Exception ex) { return (false, "", ex.Message); }
    }

    /// <summary>Installa da URL/zip/directory (semantica di <c>AI BRAIN INSTALL nome, sorgente</c>).</summary>
    public static (bool Ok, string Root, string Error) InstallSource(string name, string source)
    {
        try
        {
            var outb = new byte[4096];
            var errb = new byte[512];
            var rc = brain_install_source(name, source, outb, (UIntPtr)outb.Length, errb, (UIntPtr)errb.Length);
            return (rc == 0, rc == 0 ? CStr(outb) : "", rc == 0 ? "" : CStr(errb));
        }
        catch (Exception ex) { return (false, "", ex.Message); }
    }

    /// <summary>Rimuove dalla CACHE (mai da vendor/exe).</summary>
    public static bool Remove(string name, string? version = null)
    {
        try { return brain_remove(name, version) == 1; } catch { return false; }
    }

    /// <summary>Scrive l'indice `<exe>.brains.txt` dai cervelli dichiarati dal programma
    /// (`"brains": ["nome[@ver]"]` nel suo app.settings.json). Nessuna dichiarazione → nessun file.
    /// Ritorna true se il file è stato creato.</summary>
    public static bool EmitIndex(string appSettings, string? manifestPath, string outPath)
    {
        try
        {
            if (File.Exists(outPath)) File.Delete(outPath);
            var rc = brain_emit_index(appSettings, manifestPath ?? "", outPath);
            return rc > 0 && File.Exists(outPath);
        }
        catch { return false; }
    }

    public static bool HasRag(string dir, string name)
    {
        try { return brain_has_rag(dir, name) == 1; } catch { return false; }
    }

    public static bool HasGraph(string dir, string name)
    {
        try { return brain_has_graph(dir, name) == 1; } catch { return false; }
    }
}
