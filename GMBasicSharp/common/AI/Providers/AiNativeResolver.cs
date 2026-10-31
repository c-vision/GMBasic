// FILE: Basic.Providers.AiNativeResolver
// Resolver condiviso per le librerie native AI di GMBasic (assembly `AI`): llm_ai_mlx,
// llmtoken, llm_ai_llamacpp, llm_ai_embedded, ...
//
// PERCHÉ: i provider AI (MlxProviders/LlamaCppProviders) usano [LibraryImport]
// e si affidavano alla ricerca OS di default → il binario del backend IL NATIVO (NativeAOT) viene
// pubblicato in <sample>/bin/native/ dove le dylib NON ci sono, e i provider fallivano con
// "Unable to load shared library 'llmtoken'" (Bugs/(open) - IL-Nativo-Dylib-AI-Non-Risolte.md).
//
// `NativeLibrary.SetDllImportResolver` è per-ASSEMBLY e si può impostare UNA sola volta: per
// questo c'è un unico resolver idempotente condiviso da tutte le classi native dell'assembly AI.
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Basic.Providers;

internal static class AiNativeResolver
{
    private static readonly object Gate = new();
    private static readonly HashSet<Assembly> Registered = new();

    /// <summary>Installa il resolver per l'assembly (idempotente). Chiamare in un ctor statico
    /// di ogni classe [LibraryImport] AI.</summary>
    public static void EnsureRegistered(Assembly assembly)
    {
        lock (Gate)
        {
            if (!Registered.Add(assembly)) return;
            try
            {
                NativeLibrary.SetDllImportResolver(assembly, Resolve);
            }
            catch (InvalidOperationException)
            {
                // Già impostato (es. da un altro entry point nello stesso processo): ignora.
            }
        }
    }

    // Lib AI effettivamente CARICATE in questo processo (per il teardown: non si chiama
    // `llm_ai_shutdown` su un motore che non è mai stato aperto — costringerebbe a caricare
    // `libllm_ai_llamacpp` (6,4 MB) solo per spegnerlo, anche in un programma che stampa una riga).
    private static readonly HashSet<string> Loaded = new(StringComparer.Ordinal);

    /// <summary>true se la lib AI `name` (es. "llm_ai_llamacpp") è stata caricata davvero.</summary>
    public static bool WasLoaded(string name)
    {
        lock (Loaded) return Loaded.Contains(name);
    }

    public static IntPtr Resolve(string name, Assembly assembly, DllImportSearchPath? search)
        => CommonLib.GmbBoot.ResolveDllImport(name);

}
