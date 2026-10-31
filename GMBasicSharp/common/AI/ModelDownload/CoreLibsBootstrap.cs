// AI/ModelDownload/CoreLibsBootstrap.cs — BOOTSTRAP del primo avvio: garantisce in
// `~/.gmbasic/libs/<rid>/` le librerie native che i COMPONENTI ABILITATI richiedono, scaricandole
// dai bundle di c-vision/GMBasicModules (CommonLib.LibsResolver). Se qualcosa resta incompleto
// → notifica, e (se interattivo) chiede conferma prima di avviare a metà.
//
// QUALI GRUPPI (decisione: il set minimo dipende dai componenti, non è una lista fissa)
//   * support + core + engine → SEMPRE. `engine` non è il motore AI: è il PLUMBING che ogni dylib
//                       nostra linka (libllmcodec_*, libllmtoken) — senza, `libgmb_gfx`/`libgmb_pkg`
//                       NON si caricano ("Library not loaded", verificato con dlopen).
//   * llmmotor        → se il layer AI è configurato (config/ai.json o providers.json non vuoti):
//                       MLX (+mlx.metallib) e llama.cpp/ggml/mtmd + wrapper.
//   * torch           → MAI nel bootstrap: è `external` (pytorch.org) e si scarica per-FILE quando
//                       una funzione lo chiede (LibsResolver.EnsureAsync("libtorch_cpu.dylib")) o
//                       con `gmb libs ensure --all`. Chi ha già il componente TORCH lo trova in
//                       libs-runtime/ml/<rid> e non paga 324 MB nella cache libs.
//
// Confine: B (support) · C (engine) · D (core) → GMBasicModules; T (torch) → pytorch.org (external).

using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using CommonLib;

namespace Basic.AI.ModelDownload;

public static class CoreLibsBootstrap
{
    /// <summary>Gruppi necessari per i componenti abilitati sul RID corrente.</summary>
    public static IReadOnlyList<string> GroupsFor(string rid)
    {
        var groups = LibsResolver.BaseGroups.ToList();

        // llmmotor (motori AI: MLX + llama.cpp): solo se il layer AI è configurato
        // (nessuna env: si guarda la config) E il motore esiste per questo RID.
        // MLX è Apple-only per costruzione (Metal): su Linux/Windows il gruppo non è pubblicato
        // (l'indice non lo dichiara) e chiederlo produceva un errore "non disponibile" a ogni
        // avvio. Quando i bundle llama.cpp per gli altri RID saranno pubblicati
        // (tools/libs_build_release.sh llmmotor), basta togliere questa condizione.
        var aiJson = Path.Combine(GmbiPaths.ConfigDir, "ai.json");
        var providers = Path.Combine(GmbiPaths.ConfigDir, "providers.json");
        var motorOnThisRid = rid.StartsWith("osx-", StringComparison.OrdinalIgnoreCase);
        if (motorOnThisRid && (HasContent(aiJson) || HasContent(providers)))
            groups.Add(LibsResolver.MotorGroup);
        return groups;
    }

    private static bool HasContent(string path)
    {
        try
        {
            if (!File.Exists(path)) return false;
            var len = new FileInfo(path).Length;
            return len > 8;   // "{}" o vuoto non conta come configurato
        }
        catch { return false; }
    }

    /// <summary>
    /// Garantisce le librerie per i componenti abilitati. Ritorna true se la cache è completa
    /// (o se l'utente accetta di avviare con librerie incomplete), false se rifiuta.
    /// </summary>
    public static async Task<bool> EnsureAsync(bool interactive, Action<string>? log = null,
        CancellationToken ct = default)
    {
        var rid = LibsResolver.Rid();
        var dir = LibsResolver.CacheDir(rid);
        var groups = GroupsFor(rid);

        // 1) cache già completa per i gruppi richiesti → niente da fare
        try
        {
            var states = await LibsResolver.CheckAsync(groups, rid, null, ct).ConfigureAwait(false);
            if (states.Count > 0 && states.All(s => s.Status == LibsFileStatus.Cached)) return true;
        }
        catch (Exception ex) { log?.Invoke($"[bootstrap] indice non leggibile: {ex.Message}"); }

        log?.Invoke($"[bootstrap] librerie native incomplete per '{rid}' (gruppi: {string.Join(", ", groups)}) — scarico da GMBasicModules…");

        // 2) download dei gruppi mancanti (indice = GMBasicModules)
        var failed = new List<string>();
        foreach (var g in groups)
        {
            try
            {
                var n = await LibsResolver.EnsureGroupAsync(g, rid, log, null, ct).ConfigureAwait(false);
                log?.Invoke($"[bootstrap] gruppo '{g}': {n} file installati");
            }
            catch (Exception ex)
            {
                failed.Add(g);
                log?.Invoke($"[bootstrap] gruppo '{g}' NON disponibile: {ex.Message}");
            }
        }
        if (failed.Count == 0) return true;

        // 3) fallback: chiedi una cartella con le librerie (se interattivo)
        if (interactive)
        {
            Console.WriteLine();
            Console.WriteLine($"[bootstrap] Impossibile scaricare i gruppi: {string.Join(", ", failed)}");
            Console.WriteLine($"            La cache è: {dir}");
            Console.Write("[bootstrap] Cartella alternativa con le librerie (Invio per saltare): ");
            var path = Console.ReadLine()?.Trim();
            if (!string.IsNullOrEmpty(path) && Directory.Exists(path))
            {
                try
                {
                    Directory.CreateDirectory(dir);
                    foreach (var f in Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories))
                    {
                        var rel = Path.GetRelativePath(path, f);
                        var dst = Path.Combine(dir, rel);
                        Directory.CreateDirectory(Path.GetDirectoryName(dst)!);
                        File.Copy(f, dst, overwrite: true);
                    }
                    log?.Invoke($"[bootstrap] copiate le librerie da {path} → {dir}");
                    var states = await LibsResolver.CheckAsync(groups, rid, null, ct).ConfigureAwait(false);
                    if (states.Count > 0 && states.All(s => s.Status == LibsFileStatus.Cached)) return true;
                }
                catch (Exception ex) { log?.Invoke($"[bootstrap] copia fallita: {ex.Message}"); }
            }
        }

        // 4) integrità residua → notifica → conferma per avviare comunque
        log?.Invoke($"[bootstrap] ATTENZIONE: librerie INCOMPLETE per '{rid}' in {dir}.");
        // Rimedio esplicito: i gruppi mancanti per QUESTO RID (tipicamente core/engine, che
        // oggi sono pubblicati solo per osx-arm64) si compilano in loco. Senza questa riga
        // l'utente legge solo "non disponibile" e non sa che esiste uno script che lo fa.
        log?.Invoke($"[bootstrap] gruppi mancanti: {string.Join(", ", failed)} — " +
                    $"compilali con GMBasic/tools/libs_build_release.sh <gruppo> (o indica una cartella con le librerie).");
        if (!interactive) return false;

        Console.Write("[bootstrap] Avviare comunque con librerie non complete? [y/N] ");
        var a = Console.ReadLine();
        return a != null && (a.Trim().Equals("y", StringComparison.OrdinalIgnoreCase)
                          || a.Trim().Equals("yes", StringComparison.OrdinalIgnoreCase));
    }
}
