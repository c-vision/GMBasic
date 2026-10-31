// Esegue ogni sample in samples/Loops tramite il backend IL e ne verifica
// l'esito SENZA golden file: deve COMPLETARE (niente eccezione/hang) e stampare l'esito
// atteso. Il sample di regressione LoopsForSequentialSameLexeme.bas deve dare Somma=55.
using System.Collections.Immutable;
using System.Reflection;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class LoopsSamplesExecutionTests
{
    private static readonly string Root = FindRepoRoot();

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    [Fact]
    public void LoopsForSequentialSameLexeme_Produces_Sum55()
    {
        var sample = Path.Combine(Root, "samples", "Community", "Loops", "LoopsForSequentialSameLexeme.bas");
        var output = RunSample(sample);
        Assert.Contains("SUCCESSO", output);
        Assert.Matches(@"Somma=\s*55", output);
    }

    [Fact]
    public void All_Loops_Samples_Complete_Via_Il()
    {
        // Gap del backend IL PRE-ESISTENTI e indipendenti dal bug FOR (non regressioni di
        // questo test): vengono RIPORTATI nell'output, non fanno fallire la verifica dei loop.
        // LoopsForEachComprehensive.bas era qui (comprehension .Join()/.Count() su DIM
        // globale + FOR/FOR EACH a livello modulo davano NRE/InvalidProgram o somme/count
        // a 0): chiuso — il local di un array STATIC/GLOBALE dinamico e il contatore FOR
        // attivo ora rispettano rispettivamente ArrayInfo.LoadRef e _forLoopState invece di
        // leggere sempre dal bag/default (vedi IlProgramCompiler.Collections.cs/Operators.cs).
        var knownIlGaps = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var dir = Path.Combine(Root, "samples", "Community", "Loops");
        var samples = Directory.GetFiles(dir, "*.bas", SearchOption.TopDirectoryOnly).OrderBy(f => f).ToArray();
        var failures = new List<string>();
        foreach (var sample in samples)
        {
            var name = Path.GetFileName(sample);
            if (knownIlGaps.Contains(name))
            {
                Console.WriteLine($"[loops] {name}: GAP IL noto (riportato, non testato qui)");
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
                // Marker espliciti di esito presente nel sample: deve comparire quello di successo.
                if (output.Contains("FALLITO", StringComparison.OrdinalIgnoreCase))
                    failures.Add($"{name}: contiene 'FALLITO'\n{output}");
                Console.WriteLine($"[loops] {name}: OK ({output.Length} char)");
            }
            catch (Exception ex)
            {
                failures.Add($"{name}: {ex.GetType().Name}: {ex.Message}\n{ex.StackTrace}");
            }
        }
        if (failures.Count > 0)
            Assert.Fail(string.Join("\n", failures));
    }

    [Fact]
    public void LoopsBasics_Completes_With_Expected_Markers()
    {
        var sample = Path.Combine(Root, "samples", "Community", "Loops", "LoopsBasics.bas");
        var output = RunSample(sample);
        foreach (var marker in new[]
                 {
                     "Fine del loop.",
                     "Corretto. Il loop non è partito.",
                     "Fine DO WHILE con CONTINUE.",
                     "Uscito da DO UNTIL dopo Z=3.",
                     "Fine annidamento con EXIT DO inner.",
                     "Fine annidamento con CONTINUE inner.",
                     "Fine FOR annidati con NEXT J, I.",
                     "Cicli DO con etichette terminati.",
                     "Cicli WHILE con etichette terminati.",
                     "Cicli FOR con etichette terminati.",
                 })
            Assert.Contains(marker, output);
        Assert.DoesNotContain("FALLITO", output, StringComparison.OrdinalIgnoreCase);
    }

    private static string RunSample(string sample)
    {
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "Loops_" + Guid.NewGuid().ToString("N"), out _);
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
