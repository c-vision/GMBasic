// test_pcode — esegue un sample .bas sulla PCodeVM (compilazione SourcePipeline).
// Uso: dotnet run --project test_pcode -- <Sample.bas>
using Basic.IlBackend.PCode;

if (args.Length > 0 && args[^1] == "plugin-cmd")
{
    var list = Basic.IlBackend.IlRuntime.AiPluginsList();
    Console.WriteLine("LIST=[" + string.Join(",", list) + "]");
    var info = Basic.IlBackend.IlRuntime.AiPluginInfo("web-fetch");
    Console.WriteLine("INFO=" + info);
    var en = Basic.IlBackend.IlRuntime.AiPluginsEnable("web-fetch");
    Console.WriteLine("ENABLE=" + en + " REG=" + (Basic.AI.PluginRegistry.Get("web-fetch") != null));
    var dis = Basic.IlBackend.IlRuntime.AiPluginsDisable("web-fetch");
    Console.WriteLine("DISABLE=" + dis);
    var cr = Basic.IlBackend.IlRuntime.AiPluginsCreate("my-plugin", "Mio plugin di test");
    Console.WriteLine("CREATE=" + cr);
    var rm = Basic.IlBackend.IlRuntime.AiPluginsRemove("my-plugin");
    Console.WriteLine("REMOVE=" + rm);
    return 0;
}
if (args.Length > 0 && args[^1] == "plugin-mkt")
{
    var dest = "/tmp/gmb-mkt/dest";
    var r = Basic.IlBackend.IlRuntime.AiPluginsMarketplace("file:///tmp/gmb-mkt/repo", dest);
    Console.WriteLine("MARKETPLACE=" + r + " EXISTS=" + System.IO.Directory.Exists(System.IO.Path.Combine(dest, "demo-web")));
    return 0;
}
if (args.Length > 0 && args[^1] == "plugin-test")
{
    var refreshed = Basic.AI.PluginIndex.Refresh();
    Console.WriteLine("REFRESH_OK=" + refreshed.Contains("web-fetch"));
    var reg = Basic.AI.PluginRegistry.Register("web-fetch");
    var rec = Basic.AI.PluginRegistry.Get("web-fetch");
    Console.WriteLine("REGISTER=" + reg + " NAME=" + rec?.Name + " DESC=" + rec?.Description + " TOOLS=" + (rec?.Tools.Length ?? 0));
    var en = Basic.AI.PluginRegistry.Enable("web-fetch");
    var tool = Basic.Mcp.McpToolRegistry.Instance.Get("web_fetch");
    Console.WriteLine("ENABLE=" + en + " TOOL_REG=" + (tool != null) + " TOOL_PROC=" + tool?.QualifiedProcedureName);
    Basic.AI.PluginRegistry.Disable("web-fetch");
    Console.WriteLine("DISABLE_TOOL_REG=" + (Basic.Mcp.McpToolRegistry.Instance.Get("web_fetch") == null));
    var created = Basic.AI.PluginRegistry.Create("sample-plugin", "Plugin di esempio generato");
    Console.WriteLine("CREATE=" + created + " EXISTS=" + File.Exists(Path.Combine(Basic.AI.AiConfig.PluginsDefaultPath, "sample-plugin", "PLUGIN.md")));
    return 0;
}
var root = Directory.GetCurrentDirectory();
var sampleName = args.Length > 0 ? args[^1] : "PCodeGpmRwMutex.bas";
var candidates = new[]
{
    sampleName.Contains("/") || sampleName.Contains("\\") ? sampleName : null,
    Path.Combine(root, "shared", "samples", "Community", "Concurrency", sampleName),
    Path.Combine(root, "shared", "samples", "Community", "Algebra", sampleName),
    Path.Combine(root, "shared", "samples", "Community", "Concurrency", "Algebra", sampleName),
    Path.Combine(root, "shared", "samples", sampleName),
};
var sample = candidates.FirstOrDefault(File.Exists);
if (sample == null)
{
    Console.Error.WriteLine($"Sample non trovato: {sampleName}");
    return 2;
}

// Parità con i test del gate (tests/GMBASIC.IlBackend.Tests/PCodeComplexConcurrencySampleTests.cs):
// alcuni sample richiedono configurazione specifica che il runner di default non ha.
// - PCodeGpmStackOverflowRecovery: il gate forza lo stack piccolo (GMBASIC_PCODE_MAX_STACK=8)
//   per far scattare l'overflow; senza, il sample stampa FAIL:OVERFLOW-NOT-RAISED.
// - PCodeGpmCancellationFinally: il gate cancella a 100ms per staccare il task dal SLEEP 10000;
//   con il timeout di 20s il sample non viene cancellato e stampa i rami FAIL del caso senza
//   cancellazione.
string? previousMaxStack = null;
if (sampleName.Contains("StackOverflowRecovery"))
{
    previousMaxStack = Environment.GetEnvironmentVariable("GMBASIC_PCODE_MAX_STACK");
    Environment.SetEnvironmentVariable("GMBASIC_PCODE_MAX_STACK", "8");
}
var cancelAfter = sampleName.Contains("CancellationFinally")
    ? TimeSpan.FromMilliseconds(100)
    : TimeSpan.FromSeconds(20);
// Override per run lunghi (es. piloti LLM K× sampling): GMB_PCODE_TIMEOUT_S
// (secondi; 0 = nessun timeout). Default 20s invariato per CI.
if (double.TryParse(Environment.GetEnvironmentVariable("GMB_PCODE_TIMEOUT_S"),
        System.Globalization.NumberStyles.Float,
        System.Globalization.CultureInfo.InvariantCulture, out var timeoutS)
    && timeoutS >= 0)
    cancelAfter = timeoutS == 0 ? Timeout.InfiniteTimeSpan : TimeSpan.FromSeconds(timeoutS);

try
{
    var sourceText = File.ReadAllText(sample);
    global::Compiler.CompiledChunk chunk;
    if (Environment.GetEnvironmentVariable("NOCONCURRENCY") == "1")
    {
        var ns = new[] { "SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME", "CONVERSIONS", "IO", "ARRAYS" };
        var st = new CommonLib.EffectiveSettings
        {
            DefaultNamespaces = ns,
            ImportFolders = [],
            ImportFiles = [],
            ConsoleConfiguration = [],
            LibraryFolders = [],
            DefaultProjectsFolder = "."
        };
        chunk = Basic.Pipeline.SourcePipeline.CompileFromString(sourceText, sample, st);
    }
    else
    {
        chunk = Basic.Pipeline.SourcePipeline.CompileFromString(sourceText, sample);
    }
    System.IO.File.WriteAllText("/tmp/d.txt", global::Compiler.Disassembler.Disassemble(chunk, "D"));
    Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
    using var cts = new CancellationTokenSource(cancelAfter);
    // AGENTS.md §9.1: CWD = cartella del SAMPLE (mai la root del repo). Prima il sample
    // veniva eseguito con la CWD del processo (root quando lanciato con dotnet run dalla
    // root) → i path relativi del .bas (OPEN/SPEED/bin/) creavano cartelle in root.
    Directory.SetCurrentDirectory(Path.GetDirectoryName(Path.GetFullPath(sample))!);
    // Test-only: iniezione input nel motore C (GMB_TEST_KEYS="x", GMB_TEST_MOUSE="x,y[,btn]").
    var testKeys = Environment.GetEnvironmentVariable("GMB_TEST_KEYS");
    var testMouse = Environment.GetEnvironmentVariable("GMB_TEST_MOUSE");
    if (Basic.Runtime.CGfxGraphicsService.UseCGraphics && (!string.IsNullOrEmpty(testKeys) || !string.IsNullOrEmpty(testMouse)))
    {
        _ = System.Threading.Tasks.Task.Run(async () =>
        {
            await System.Threading.Tasks.Task.Delay(1200);
            if (!string.IsNullOrEmpty(testMouse))
            {
                var mp = testMouse.Split(',');
                if (mp.Length >= 2 && int.TryParse(mp[0], out var mx) && int.TryParse(mp[1], out var my))
                {
                    var mb = mp.Length > 2 && int.TryParse(mp[2], out var b) ? b : 0;
                    Basic.Runtime.CGfxInputService.InjectMouse(Basic.Runtime.CGfxInputService.MouseMove, mx, my, 0);
                    if (mb > 0)
                    {
                        Basic.Runtime.CGfxInputService.InjectMouse(Basic.Runtime.CGfxInputService.MouseDown, mx, my, mb);
                        await System.Threading.Tasks.Task.Delay(120);
                        Basic.Runtime.CGfxInputService.InjectMouse(Basic.Runtime.CGfxInputService.MouseUp, mx, my, mb);
                    }
                }
            }
            if (!string.IsNullOrEmpty(testKeys))
            {
                foreach (var ch in testKeys)
                {
                    try { Basic.Runtime.CGfxInputService.InjectKey(ch.ToString()); } catch { }
                    await System.Threading.Tasks.Task.Delay(300);
                }
            }
        });
    }
    var result = PCodeVmRunner.Run(chunk, cts.Token);
    // Verifica visiva opzionale (motore C): salva la pagina come PNG.
    var pngOut = Environment.GetEnvironmentVariable("GMB_TEST_PNG");
    if (!string.IsNullOrEmpty(pngOut) && Basic.Runtime.CGfxGraphicsService.UseCGraphics)
    {
        try
        {
            var rc = Basic.Runtime.CGfxGraphicsService.SavePng(pngOut);
            Console.WriteLine($"PNG={pngOut} rc={rc}");
        }
        catch { }
    }
    Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", null);
    var blocked = result.Output.Contains("[P-CODE ERROR]");
    Console.WriteLine($"SUCCESS={result.Success} EXC={result.ExitCode} BLOCKED={blocked}");
    Console.WriteLine(result.Output);
    return result.Success && !blocked ? 0 : 1;
}
finally
{
    Environment.SetEnvironmentVariable("GMBASIC_PCODE_MAX_STACK", previousMaxStack);
}
