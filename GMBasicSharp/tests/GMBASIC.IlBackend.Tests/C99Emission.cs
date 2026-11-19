// FILE: C99Emission.cs — la gamba C99Native è la BASE dei test (direttiva 2026-11-19).
//
// Ogni test che ESEGUE un sample `.bas` deve prima dimostrare che il backend C lo EMETTE:
// un sample che il C non riesce nemmeno a compilare non è "non testato", è rotto, e finché
// l'unico test compila su IL il buco resta invisibile (caso reale: PackageServer.bas, 1800
// righe, 7 errori di emissione, test solo su IL).
//
// Il helper invoca il driver C in modalità sola-emissione (`--no-exe`): veloce, nessuna
// esecuzione (niente hang/server). Se il driver non è compilato il test FALLISCE con il
// comando da lanciare: un gate che si salta da solo non è un gate.

using System;
using System.Diagnostics;
using System.IO;
using Xunit;

namespace Basic.IlBackend.Tests;

internal static class C99Emission
{
    private static readonly Lazy<string> DriverPath = new(Locate);

    /// <summary>Driver C (`gmbnative`): build del monorepo o `bin/`.</summary>
    internal static string Driver => DriverPath.Value;

    private static string Locate()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            foreach (var rel in new[]
                     {
                         "GMBasic/native/gmb_compiler_c/build/gmbnative",
                         "native/gmb_compiler_c/build/gmbnative",
                         "bin/gmbnative"
                     })
            {
                var p = Path.Combine(dir.FullName, rel);
                if (File.Exists(p)) return p;
            }
            dir = dir.Parent;
        }
        throw new InvalidOperationException(
            "driver C assente (gmbnative). Compila: cmake --build GMBasic/native/gmb_compiler_c/build " +
            "--target gmbnative -j 6");
    }

    /// <summary>
    /// Asserisce che il sample sia emettibile dal backend C. In caso contrario fallisce con
    /// l'errore del driver (prima riga utile) — cioè il test rosso è già la diagnosi.
    /// </summary>
    internal static void Require(string samplePath)
    {
        Assert.True(File.Exists(samplePath), $"sample inesistente: {samplePath}");
        var outC = Path.Combine(Path.GetTempPath(), "gmb_c99_emit_" + Guid.NewGuid().ToString("N") + ".c");
        try
        {
            var psi = new ProcessStartInfo(Driver)
            {
                WorkingDirectory = Path.GetDirectoryName(samplePath) ?? Directory.GetCurrentDirectory(),
                RedirectStandardError = true,
                RedirectStandardOutput = true,
                UseShellExecute = false
            };
            psi.ArgumentList.Add(Path.GetFullPath(samplePath));
            psi.ArgumentList.Add(outC);
            psi.ArgumentList.Add("--no-exe");

            using var p = Process.Start(psi) ?? throw new InvalidOperationException("driver C non avviato");
            var stderr = p.StandardError.ReadToEnd();
            p.StandardOutput.ReadToEnd();
            p.WaitForExit();
            if (p.ExitCode != 0)
            {
                var first = stderr.Split('\n', StringSplitOptions.RemoveEmptyEntries);
                var msg = first.Length > 0 ? first[0].Trim() : $"rc={p.ExitCode}";
                Assert.Fail($"C99Non emette {Path.GetFileName(samplePath)}: {msg}");
            }
        }
        finally
        {
            try { if (File.Exists(outC)) File.Delete(outC); } catch { /* temporaneo */ }
        }
    }
}
