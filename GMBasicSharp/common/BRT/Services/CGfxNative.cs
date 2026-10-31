// FILE: Basic.Runtime.CGfxNative.cs
// Resolver condiviso per la dylib del motore grafico C (libgmb_gfx.dylib).
// SetDllImportResolver e' per-assembly e puo' essere impostato UNA SOLA VOLTA:
// tutte le classi P/Invoke (CGfxGraphicsService, CGfxInputService, ...) passano
// da qui invece di registrare resolver propri (la seconda registrazione lancia).
using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Basic.Runtime;

internal static class CGfxNative
{
    internal const string Lib = "gmb_gfx";
    private static bool _registered;

    [System.Runtime.CompilerServices.ModuleInitializer]
    internal static void InitResolver() => EnsureResolver();

    internal static void EnsureResolver()
    {
        if (_registered) return;
        _registered = true;
        try
        {
            NativeLibrary.SetDllImportResolver(typeof(CGfxNative).Assembly, Resolve);
        }
        catch (InvalidOperationException)
        {
            // Un resolver per-assembly e' gia' presente (es. un altro punto del
            // processo): le DllImport per nome restano valide se la dylib e' gia'
            // caricata o risolvibile dal runtime.
        }
    }

    private static IntPtr Resolve(string name, Assembly assembly, DllImportSearchPath? paths)
        => CommonLib.GmbBoot.ResolveDllImport(name);

    /// <summary>Resolver generico dell'assembly BRT: più classi P/Invoke registrano un
    /// resolver per-assembly (vince il primo), quindi ognuna deve delegare qui per le librerie
    /// che non le appartengono. Cerca la lib per nome in libs-runtime, Resources e build native.</summary>
    internal static IntPtr ResolveAny(string name, Assembly assembly, DllImportSearchPath? paths)
        => CommonLib.GmbBoot.ResolveDllImport(name);
}
