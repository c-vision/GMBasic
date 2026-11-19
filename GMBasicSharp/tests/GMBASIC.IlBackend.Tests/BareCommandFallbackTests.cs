using System;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using Basic.IlBackend;
using Xunit;

namespace Basic.IlBackend.Tests;

/// <summary>
/// Prova il fallback dei comandi/funzioni "bare" (USE/REPLACE/DELETE/RECALL/IIF
/// senza prefisso di namespace) e i path runtime collegati: emissione IL di
/// RddDelete/RddRecall/RddReplace (pop del long di ritorno) e risoluzione del
/// driver RDD dal typeName (VIA "ARRAY") con auto-detect per ":memory:".
/// </summary>
[Collection("GpmRuntimeState")]
public class BareCommandFallbackTests
{
    private static string Root
    {
        get
        {
            var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
            while (dir != null && !File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) dir = dir.Parent;
            return dir?.FullName ?? Directory.GetCurrentDirectory();
        }
    }

    [Fact]
    public void BareCommandFallback_Sample_Runs_Via_Il()
    {
        var sample = Path.Combine(Root, "shared", "samples", "Databases", "BareCommandFallback.bas");
        var output = RunSample(sample);
        Assert.Contains("1. USE bare (VIA -> RDD) -> OK", output);
        Assert.Contains("2. REPLACE bare ALL -> OK", output);
        Assert.Contains("3. DELETE/RECALL bare -> OK", output);
        Assert.Contains("4. DELETE FOR bare -> OK", output);
        Assert.Contains("5. funzione bare IIF(SYSTEM) -> si", output);
        Assert.Contains("6. qualificata CONVERSIONS.CSTR(42) ->  42", output);
        Assert.Contains("TEST COMPLETATO", output);
    }

    [Fact]
    public void RddScopedStatements_EmitValidIl()
    {
        var program = Basic.Orchestrator.Compile(
            Path.Combine(Root, "shared", "samples", "Databases", "BareCommandFallback.bas"), null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "Bare_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var assembly = Assembly.Load(ms.ToArray());
        foreach (var type in assembly.GetTypes())
        {
            var flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance
                        | BindingFlags.Static | BindingFlags.DeclaredOnly;
            foreach (var method in type.GetMethods(flags))
                System.Runtime.CompilerServices.RuntimeHelpers.PrepareMethod(method.MethodHandle);
            foreach (var ctor in type.GetConstructors(flags))
                System.Runtime.CompilerServices.RuntimeHelpers.PrepareMethod(ctor.MethodHandle);
        }
    }

    private static string RunSample(string sample)
    {
        C99Emission.Require(sample);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "Bare_" + Guid.NewGuid().ToString("N"), out _);
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
