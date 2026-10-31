// FILE: CommonLib/NativeFixtures.cs
// Binding P/Invoke delle FIXTURE (native/gmb_runtime_c/src/gmb_fixtures.c, esportato da libgmb_pkg).
// Le fixture sono dati del COMPILATORE: si scaricano SEMPRE e SOLO in
// <monorepo>/shared/samples/Fixtures/<nome>/ (manifest in shared/settings/fixtures-manifest.json),
// mai da un disco esterno. Il compilatore e il REPL assicurano quelle dichiarate dal programma
// (`"fixtures": ["markets"]` nel suo app.settings.json) prima di eseguirlo.
using System.Runtime.InteropServices;

namespace CommonLib;

public static class NativeFixtures
{
    private const string Lib = "gmb_pkg";

    static NativeFixtures()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(NativeFixtures).Assembly, Resolve); }
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

    [DllImport(Lib, EntryPoint = "gmb_pkg_fixtures_ensure_declared", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern int fixtures_ensure_declared(string appSettings, [In, Out] byte[] errbuf, UIntPtr ecap);

    [DllImport(Lib, EntryPoint = "gmb_pkg_fixtures_ensure_all", CallingConvention = CallingConvention.Cdecl)]
    private static extern int fixtures_ensure_all([In, Out] byte[] errbuf, UIntPtr ecap);

    /// <summary>Scarica le fixture dichiarate dal programma che MANCANO (sempre in
    /// <c>samples/Fixtures</c>). Ritorna il numero scaricate (0 = niente da fare), negativo = errori.</summary>
    public static int EnsureDeclared(string? appSettingsPath)
    {
        if (string.IsNullOrWhiteSpace(appSettingsPath) || !File.Exists(appSettingsPath)) return 0;
        try
        {
            var errb = new byte[512];
            return fixtures_ensure_declared(appSettingsPath, errb, (UIntPtr)errb.Length);
        }
        catch { return 0; }   // dylib assente: il programma fallirà sulla fixture mancante, non qui
    }

    /// <summary>Scarica TUTTE le fixture mancanti dichiarate nel manifest.</summary>
    public static int EnsureAllMissing()
    {
        try
        {
            var errb = new byte[512];
            return fixtures_ensure_all(errb, (UIntPtr)errb.Length);
        }
        catch { return 0; }
    }

    /// <summary>Come sopra, con il messaggio d'errore del C.</summary>
    public static (int Count, string Error) EnsureAllMissingWithError()
    {
        try
        {
            var errb = new byte[512];
            var n = fixtures_ensure_all(errb, (UIntPtr)errb.Length);
            return (n, n < 0 ? CStr(errb) : "");
        }
        catch (Exception ex) { return (0, ex.Message); }
    }
}
