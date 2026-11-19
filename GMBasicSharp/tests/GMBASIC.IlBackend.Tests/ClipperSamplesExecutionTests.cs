// Esegue ogni sample TUI/Clipper in samples/Clipper tramite il backend IL e
// verifica la PARITÀ col riferimento (PCodeVM → VM/C, community): ogni sample deve
// COMPLETARE (niente eccezione/hang) e le righe di asserzione (RISULTATO: / OK / PASSED)
// devono coincidere con quelle del golden generato dalla VM/C di riferimento.
using System.Reflection;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class ClipperSamplesExecutionTests
{
    private static readonly string Root = FindRepoRoot();

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private static readonly string GoldenDir =
        Path.Combine(FindRepoRoot(), "tests", "GMBASIC.IlBackend.Tests", "Golden", "Clipper");

    // Differenze documentate vs il riferimento VM/C (non regressioni IL):
    // - ClipperSyntax: la VM/C confronta le stringhe come numeri (0==0 → sempre uguali) e
    //   tratta NIL come 0 (DIM non tipizzato = double): l'IL è più corretto (confronto
    //   stringhe reale, == esatto) e NON deve replicare quei bug.
    // - ClipperFormDemo/ClipperPCodeTest/ClipperTBrowse: la VM/C esegue il RENDERING TUI
    //   reale dei widget (marker "compiled OK", stato del controllo), l'IL è headless
    //   (DrawWidget disegna un segnaposto e assegna il default al target INTO).
    private static readonly Dictionary<string, string> KnownReferenceDiffs = new(StringComparer.OrdinalIgnoreCase)
    {
        ["ClipperSyntax"] = "riferimento VM/C: stringhe confrontate come numeri (0==0) e NIL→0; IL corretto",
        ["ClipperFormDemo"] = "rendering widget TUI: VM/C reale vs IL headless",
        ["ClipperPCodeTest"] = "rendering widget TUI: VM/C reale vs IL headless",
        ["ClipperTBrowse"] = "rendering widget TUI: VM/C reale vs IL headless",
    };

    // Sample con controlli TUI modali (CHECKBOX/LISTBOX/COMBOBOX/...): il backend IL ora
    // usa gli stessi widget ClipperLib modali della PCodeVM/VM-C → in headless senza input
    // il loop del controllo resta in attesa (comportamento corretto, parità). Non vanno
    // eseguiti dallo sweep cieco: vanno verificati col runner TUI con iniezione
    // (test_graphics_headless --key / GMB_TUI_SCRIPT), come i sample interattivi.
    private static readonly HashSet<string> ModalWidgetSamples = new(StringComparer.OrdinalIgnoreCase)
    {
        "ClipperAdvanced", "ClipperControlsStandalone", "ClipperFileDialog",
        "ClipperFormDemo", "ClipperFormFull", "ClipperFullSuite", "ClipperGetDemo",
        "ClipperMenu", "ClipperMenuNested", "ClipperPCodeTest", "ClipperPrinterDialog",
        "ClipperTBrowse",
        // Campioni con input utente (WAITKEY/CONTROLLI modali o finestre interattive):
        // in headless senza iniezione restano in attesa (marker solo con runner TUI).
        "ClipperEvents", "WindowEditable", "WindowPixel", "WindowPixelColorTest",
        "WindowTurboVisionApp", "WindowTurboVisionGui",
        "ClipperDialogs", // ALERTBOX/CONFIRM modali: richiedono input (loop ClipperInput.InKey)
    };

    [Fact]
    public void ClipperSayTest_All_Steps_Ok()
    {
        var sample = Path.Combine(Root, "shared", "samples", "Clipper", "ClipperSayTest.bas");
        var output = RunSample(sample);
        Assert.Contains("ALL_SAY_TESTS_PASSED", output);
        Assert.Contains("T1_OK", output);
        Assert.Contains("T2_OK", output);
        Assert.Contains("T3_OK", output);
        Assert.Contains("T4_OK", output);
        Assert.Contains("T5_OK", output);
        Assert.Contains("T6_OK", output);
    }

    [Fact]
    public void All_Clipper_Samples_Complete_Via_Il_And_Match_Reference()
    {
        var dir = Path.Combine(Root, "shared", "samples", "Clipper");
        var samples = Directory.GetFiles(dir, "*.bas", SearchOption.TopDirectoryOnly).OrderBy(f => f).ToArray();
        var failures = new List<string>();
        var ok = new List<string>();
        foreach (var sample in samples)
        {
            var name = Path.GetFileNameWithoutExtension(sample);
            if (ModalWidgetSamples.Contains(name))
            {
                ok.Add($"{name}: SKIP (controlli TUI modali — verificato con runner iniettato)");
                continue;
            }
            try
            {
                var output = RunSample(sample);
                if (string.IsNullOrWhiteSpace(output))
                {
                    failures.Add($"{name}: output vuoto");
                    continue;
                }
                // Riferimento: golden VM/C (se disponibile — i sample interattivi non hanno golden).
                var goldenPath = Path.Combine(GoldenDir, name + ".txt");
                if (File.Exists(goldenPath))
                {
                    var goldenText = File.ReadAllText(goldenPath);
                    // Il golden VM/C include preamble e diagnostiche di compilazione: usa
                    // SOLO la sezione di output del sample (dopo il marker "VM/C OUTPUT").
                    var outIdx = goldenText.LastIndexOf("VM/C OUTPUT", StringComparison.OrdinalIgnoreCase);
                    var goldenMarkers = ExtractMarkers(outIdx >= 0 ? goldenText[(outIdx + "VM/C OUTPUT".Length)..] : goldenText);
                    var ilMarkers = ExtractMarkers(output);
                    if (!goldenMarkers.SequenceEqual(ilMarkers))
                    {
                        if (KnownReferenceDiffs.TryGetValue(name, out var reason))
                        {
                            Console.WriteLine($"[clipper] {name}: differenza DOCUMENTATA vs riferimento — {reason}");
                        }
                        else
                        {
                            failures.Add($"{name}: marker differiscono dal riferimento VM/C.\n  VM/C: {string.Join(" | ", goldenMarkers)}\n  IL:   {string.Join(" | ", ilMarkers)}");
                            continue;
                        }
                    }
                }
                ok.Add($"{name}: OK ({output.Length} char)");
            }
            catch (Exception ex)
            {
                var inner = ex;
                while (inner is AggregateException or System.Reflection.TargetInvocationException && inner.InnerException is not null)
                    inner = inner.InnerException;
                failures.Add($"{name}: {inner.GetType().Name}: {inner.Message}\n{inner.StackTrace}");
            }
        }
        Console.WriteLine("== Clipper OK ==");
        foreach (var o in ok) Console.WriteLine($"  {o}");
        if (failures.Count > 0)
        {
            Console.WriteLine("== Clipper FAILURES ==");
            foreach (var f in failures) Console.WriteLine($"  {f}");
            Assert.Fail(string.Join("\n", failures));
        }
    }

    /// <summary>Estrae le righe di asserzione/valutazione (parità semantiche tra backend).</summary>
    private static string[] ExtractMarkers(string text)
    {
        return text.Split('\n')
            .Select(l => l.Trim().Trim('\r'))
            .Where(l => l.Contains("RISULTATO:", StringComparison.OrdinalIgnoreCase)
                        || l.Contains("OK", StringComparison.OrdinalIgnoreCase)
                        || l.Contains("PASSED", StringComparison.OrdinalIgnoreCase)
                        || l.Contains("FALLITO", StringComparison.OrdinalIgnoreCase)
                        || l.Contains("SUCCESSO", StringComparison.OrdinalIgnoreCase))
            .ToArray();
    }

    private static string RunSample(string sample)
    {
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "Clipper_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var assembly = Assembly.Load(ms.ToArray());
        var main = assembly.GetType("GeneratedProgram")?.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            if (!task.Wait(TimeSpan.FromSeconds(20)))
                throw new InvalidOperationException("TIMEOUT (driver bloccato). Output:\n" + sw);
            task.GetAwaiter().GetResult();
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString();
    }
}
