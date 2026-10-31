// FILE: Interpreter.Parser.NativeGrep.cs
// Binding P/Invoke al motore grep del runtime C condiviso (native/gmb_runtime_c →
// libgmb_grep). Direzione unica C# → C: il C# CONSUMA la fonte C, non tiene una copia
// divergente. API a stringhe JSON (ABI stabile): ricerca + formattazione.
using System.Runtime.InteropServices;

namespace Basic;

public static class NativeGrep
{
    private const string Lib = "gmb_grep";

    /// <summary>Resolver condiviso dell'assembly (ModuleInitializer: vince sui resolver per-lib
    /// che verrebbero installati nei ctor statici). Risolve per NOME qualsiasi
    /// <c>lib&lt;name&gt;.{dylib,so,dll}</c> cercando in Resources/libs/&lt;rid&gt; e nei build dir.</summary>
    [System.Runtime.CompilerServices.ModuleInitializer]
    internal static void InitResolver()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(NativeGrep).Assembly, ResolveAny); }
        catch { /* già impostato */ }
    }

    private static IntPtr ResolveAny(string name, System.Reflection.Assembly asm, DllImportSearchPath? sp)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    static NativeGrep() { }

    private static IntPtr Resolve(string name, System.Reflection.Assembly asm, DllImportSearchPath? sp)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    [DllImport(Lib, EntryPoint = "gmb_grep_search_json", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr SearchJsonNative(string optsJson, string path);

    [DllImport(Lib, EntryPoint = "gmb_grep_format_json", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr FormatJsonNative(IntPtr hitsJson, int withLine, int withColumn);

    [DllImport(Lib, EntryPoint = "gmb_grep_free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void GrepFree(IntPtr p);

    [DllImport(Lib, EntryPoint = "gmb_grep_matches_text", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr MatchesTextNative(string pattern, string path);

    [DllImport(Lib, EntryPoint = "gmb_grep_files_text", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr FilesTextNative(string pattern, string path);

    [DllImport(Lib, EntryPoint = "gmb_grep_count_path", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern long CountPathNative(string pattern, string path);

    [DllImport(Lib, EntryPoint = "gmb_grep_matches_json", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr MatchesJsonNative(string pattern, string path);

    [DllImport(Lib, EntryPoint = "gmb_grep_files_json", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr FilesJsonNative(string pattern, string path);

    /// <summary>GREP.MATCHES(pattern, path) → "file:line:col:text" (una riga per hit).</summary>
    public static string Matches(string pattern, string path)
    {
        var p = MatchesTextNative(pattern ?? "", path ?? "");
        if (p == IntPtr.Zero) return "";
        var s = Marshal.PtrToStringAnsi(p) ?? "";
        GrepFree(p);
        return s;
    }

    /// <summary>GREP.FILES(pattern, path) → nomi file distinti, uno per riga.</summary>
    public static string Files(string pattern, string path)
    {
        var p = FilesTextNative(pattern ?? "", path ?? "");
        if (p == IntPtr.Zero) return "";
        var s = Marshal.PtrToStringAnsi(p) ?? "";
        GrepFree(p);
        return s;
    }

    /// <summary>GREP.COUNT(pattern, path) → numero di hit.</summary>
    public static long Count(string pattern, string path) => CountPathNative(pattern ?? "", path ?? "");

    /// <summary>GREP.MATCHES(pattern, path) → array di stringhe "file:line:col:text".</summary>
    public static string[] MatchesArray(string pattern, string path) => ParseJsonArray(MatchesJson(pattern, path));

    /// <summary>GREP.FILES(pattern, path) → array di nomi file distinti.</summary>
    public static string[] FilesArray(string pattern, string path) => ParseJsonArray(FilesJson(pattern, path));

    private static string MatchesJson(string pattern, string path)
    {
        var p = MatchesJsonNative(pattern ?? "", path ?? "");
        if (p == IntPtr.Zero) return "[]";
        var s = Marshal.PtrToStringAnsi(p) ?? "[]";
        GrepFree(p);
        return s;
    }
    private static string FilesJson(string pattern, string path)
    {
        var p = FilesJsonNative(pattern ?? "", path ?? "");
        if (p == IntPtr.Zero) return "[]";
        var s = Marshal.PtrToStringAnsi(p) ?? "[]";
        GrepFree(p);
        return s;
    }
    private static string[] ParseJsonArray(string json)
    {
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(json);
            var list = new System.Collections.Generic.List<string>();
            foreach (var e in doc.RootElement.EnumerateArray()) list.Add(e.GetString() ?? "");
            return list.ToArray();
        }
        catch { return System.Array.Empty<string>(); }
    }

    /// <summary>Esegue la ricerca; ritorna il JSON array degli hit (o "[]").</summary>
    public static string SearchJson(string optsJson, string path)
    {
        var p = SearchJsonNative(optsJson, path);
        if (p == IntPtr.Zero) return "[]";
        var s = Marshal.PtrToStringAnsi(p) ?? "[]";
        GrepFree(p);
        return s;
    }

    /// <summary>Formatta un array JSON di hit come testo "file:line:column:text".</summary>
    public static string FormatJson(string hitsJson, bool withLine = true, bool withColumn = true)
    {
        var ph = Marshal.StringToHGlobalAnsi(hitsJson);
        try
        {
            var p = FormatJsonNative(ph, withLine ? 1 : 0, withColumn ? 1 : 0);
            if (p == IntPtr.Zero) return "";
            var s = Marshal.PtrToStringAnsi(p) ?? "";
            GrepFree(p);
            return s;
        }
        finally { Marshal.FreeHGlobal(ph); }
    }
}
