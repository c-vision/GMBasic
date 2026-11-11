// CommonLib.FirstUseBootstrap.cs — PRIMO USO della CLI C# (`gmbcs`).
//
// PERCHÉ DELEGA. La logica di "procura quello che manca" — librerie native per RID e
// compilatore zig (gruppo `toolchain`) — vive UNA sola volta, nel runtime C
// (`GMBasic/native/gmb_runtime_c/src/gmb_first_use.c`), ed è invocabile con
// `gmbadmin first-use`. Qui non si duplica: si esegue quel comando. Vantaggi concreti:
//   * una sola politica di download (indice, sha256, estrazione `--strip-components=1`);
//   * niente decompressione xz in C# (System.Formats.Tar non legge `.tar.xz`);
//   * stessa scelta di ambito per tutti: `bootstrapScope` nei settings.
//
// COSTO A CACHE COMPLETA: un controllo di esistenza di `~/.gmbasic/.first-use-*` → si esce
// subito, senza processo e senza rete.
//
// NON BLOCCA MAI: se `gmbadmin` non c'è o la rete è giù, si stampa un avviso e `gmbcs`
// prosegue con ciò che ha.

using System;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace CommonLib;

public static class FirstUseBootstrap
{
    /// <summary>Marker di primo uso presente per QUALUNQUE RID (la cache è già completa).</summary>
    private static bool MarkerPresent()
    {
        try
        {
            var dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".gmbasic");
            return Directory.Exists(dir)
                && Directory.EnumerateFileSystemEntries(dir, ".first-use-*").Any();
        }
        catch { return false; }
    }

    /// <summary>Trova `gmbadmin`: accanto all'eseguibile (build e distribuzione), poi nel PATH.</summary>
    private static string? FindAdmin()
    {
        var exeName = OperatingSystem.IsWindows() ? "gmbadmin.exe" : "gmbadmin";
        // Layout cercati, dal più vicino: accanto all'eseguibile, poi la ricetta del monorepo
        // (`<root>/bin/gmbadmin`, dove `gmb_exe_relocatable` mette TUTTI i tool) e infine il
        // build tree. `gmbcs` vive in GMBasicSharp/CompilerCli/bin/<cfg>/<tfm>: senza la voce
        // "bin" il repo non veniva mai trovato e il primo uso veniva saltato in silenzio.
        var here = AppContext.BaseDirectory;
        for (var up = 0; up < 7 && !string.IsNullOrEmpty(here); up++)
        {
            foreach (var rel in new[] { ".", "bin", "../gmbadmin", "../gmbadmin/build",
                                        "../native/gmbadmin/build", "../../gmbadmin/build",
                                        "../../../gmbadmin/build", "../../../bin" })
            {
                try
                {
                    var cand = Path.GetFullPath(Path.Combine(here, rel, exeName));
                    if (File.Exists(cand)) return cand;
                }
                catch { /* path non valido: si prova il successivo */ }
            }
            here = Path.GetDirectoryName(here.TrimEnd(Path.DirectorySeparatorChar)) ?? "";
        }
        var path = Environment.GetEnvironmentVariable("PATH") ?? "";
        foreach (var d in path.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            try
            {
                var cand = Path.Combine(d, exeName);
                if (File.Exists(cand)) return cand;
            }
            catch { /* PATH con voci non valide */ }
        }
        return null;
    }

    /// <summary>Da chiamare all'inizio di `gmbcs`: scarica ciò che manca al primo uso.</summary>
    public static void EnsureAtFirstUse(string[] args)
    {
        if (args.Any(a => a == "--no-bootstrap")) return;
        if (MarkerPresent()) return;
        var admin = FindAdmin();
        if (admin is null)
        {
            Console.Error.WriteLine(
                "[first-use] gmbadmin non trovato: librerie/toolchain non verificate.\n" +
                "            rimedio: `gmb libs ensure` (oppure installare gmbadmin accanto a gmbcs).");
            return;
        }
        try
        {
            var psi = new ProcessStartInfo(admin, "first-use")
            {
                UseShellExecute = false,
                RedirectStandardError = false,
                RedirectStandardOutput = false,
            };
            using var p = Process.Start(psi);
            p?.WaitForExit();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[first-use] non eseguito: {ex.Message} — `gmb libs ensure` per il rimedio.");
        }
    }
}
