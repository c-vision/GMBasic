// FILE: Basic.IlBackend.PCodeDebugDriverTests.cs
// Smoke test del PCodeDebugDriver: compila un BASIC, esegue il chunk col driver in debug,
// verifica che si fermi al breakpoint, che Continue/Step funzionino e che l'output sia
// catturato. NON dipende da ClipperLib (gira nel progetto IlBackend).
using System.Threading;
using Basic.Runtime;
using Compiler;
using Xunit;
using Xunit.Abstractions;

namespace Basic.IlBackend.Tests;

public sealed class PCodeDebugDriverTests
{
    private readonly ITestOutputHelper _out;
    public PCodeDebugDriverTests(ITestOutputHelper o) => _out = o;

    [Fact]
    public void Breakpoint_Step_And_Output_Work()
    {
        var bas = PickSample();
        C99Emission.Require(bas);
        var program = Basic.Orchestrator.Compile(bas, null, false);
        Assert.NotNull(program?.MainModule);
        var chunk = new global::Compiler.Compiler().Compile(program!);
        _out.WriteLine($"code={chunk.Code.Count} lines={chunk.Lines.Count}");

        // Breakpoint sulla riga sorgente 7 (PRINT "T1_OK") — codice eseguito garantito.
        int bp = 7;
        using var driver = new Basic.IlBackend.PCode.PCodeDebugDriver();
        driver.Start(chunk, [bp]);

        var deadline = System.Diagnostics.Stopwatch.StartNew();
        while (deadline.ElapsedMilliseconds < 10000)
        {
            if (driver.IsPaused || !driver.IsRunning) break;
            Thread.Sleep(20);
        }
        _out.WriteLine($"after start: paused={driver.IsPaused} running={driver.IsRunning} line={driver.CurrentLine} bp={bp}");
        Assert.True(driver.IsPaused, $"il driver non si è fermato al breakpoint {bp}");

        // Continue → arriva alla fine.
        driver.Continue();
        deadline.Restart();
        while (deadline.ElapsedMilliseconds < 10000)
        {
            if (!driver.IsRunning) break;
            Thread.Sleep(20);
        }
        _out.WriteLine($"after continue: running={driver.IsRunning}");

        var output = driver.TakeOutput();
        _out.WriteLine($"output len={output.Length}: {Truncate(output)}");
        Assert.Contains("T1_OK", output);
    }

    [Fact]
    public void StepIn_Produces_Pauses()
    {
        var bas = PickSample();
        C99Emission.Require(bas);
        var program = Basic.Orchestrator.Compile(bas, null, false);
        Assert.NotNull(program?.MainModule);
        var chunk = new global::Compiler.Compiler().Compile(program!);

        using var driver = new Basic.IlBackend.PCode.PCodeDebugDriver();
        driver.Start(chunk, []);
        driver.StepIn(); // ferma alla prima riga

        var deadline = System.Diagnostics.Stopwatch.StartNew();
        while (deadline.ElapsedMilliseconds < 10000)
        {
            if (driver.IsPaused || !driver.IsRunning) break;
            Thread.Sleep(20);
        }
        _out.WriteLine($"step: paused={driver.IsPaused} running={driver.IsRunning} line={driver.CurrentLine}");
        Assert.True(driver.IsPaused, "lo step non ha fermato il task");
        Assert.True(driver.CurrentLine >= 1, $"CurrentLine={driver.CurrentLine} non valido");
        driver.Stop();
    }

    private static string Truncate(string s, int n = 200) => s.Length <= n ? s : s[..n] + "...";

    private static string PickSample()
    {
        var root = FindRepoRoot();
        var candidates = new[]
        {
            System.IO.Path.Combine(root, "shared", "samples", "Clipper", "ClipperSayTest.bas"),
            System.IO.Path.Combine(root, "shared", "samples", "Collections", "CollectionsLinked.bas"),
            System.IO.Path.Combine(root, "shared", "samples", "Console", "ConsoleBasics.bas"),
            System.IO.Path.Combine(root, "samples", "Native", "HelloNative.bas"),
        };
        foreach (var c in candidates)
            if (System.IO.File.Exists(c)) return c;
        return candidates[0];
    }

    private static string FindRepoRoot()
    {
        var dir = new System.IO.DirectoryInfo(System.IO.Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (System.IO.File.Exists(System.IO.Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return System.IO.Directory.GetCurrentDirectory();
    }
}
