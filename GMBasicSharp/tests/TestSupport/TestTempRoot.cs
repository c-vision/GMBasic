// TestTempRoot — redirige TMPDIR a una root PER-RUN e la cancella all'uscita del processo.
//
// Così OGNI test (e i suoi eventuali sottoprocessi) scrive i propri artefatti temporanei sotto
// UNA sola root (<temp>/GmbTestRun-<pid>-<runid>), che viene rimossa alla chiusura: nessun
// residuo in $TMPDIR/tmp. Regola (§9/§9.5): "ogni test cancella i propri artefatti".
//
// Incluso automaticamente in tutti i progetti sotto tests/ da tests/Directory.Build.props.
// Se più assembly di test condividono un processo, riusano la stessa root (la prima vince).
using System.Runtime.CompilerServices;

internal static class TestTempRoot
{
    [ModuleInitializer]
    internal static void Init()
    {
        var cur = Environment.GetEnvironmentVariable("TMPDIR");
        if (cur != null && cur.Contains("GmbTestRun-", StringComparison.Ordinal)) return;   // già rediretto
        var root = Path.Combine(Path.GetTempPath(),
            "GmbTestRun-" + Environment.ProcessId + "-" + Guid.NewGuid().ToString("N")[..8]);
        try { Directory.CreateDirectory(root); } catch { return; }
        // Root di run precedenti interrotte (kill/timeout/crash: ProcessExit non scatta) piu' vecchie di 12 h.
        try
        {
            var cutoff = DateTime.UtcNow - TimeSpan.FromHours(12);
            foreach (var d in Directory.EnumerateDirectories(Path.GetTempPath(), "GmbTestRun-*"))
                try { if (Directory.GetLastWriteTimeUtc(d) < cutoff) Directory.Delete(d, true); } catch { }
        }
        catch { }
        Environment.SetEnvironmentVariable("TMPDIR", root);
        AppDomain.CurrentDomain.ProcessExit += (_, _) => { try { Directory.Delete(root, true); } catch { } };
    }
}
