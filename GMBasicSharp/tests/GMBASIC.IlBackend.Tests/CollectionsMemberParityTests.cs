// FILE: Basic.IlBackend.CollectionsMemberParityTests.cs
// F1 Collections (piano Incongruenze): parità dei membri di collection su IL e PCode
// con grafie CASE-INSensitive (regola d'oro: il TokenKind è la chiave, il lexeme è
// solo grafia — .count()/.Count()/.COUNT() sono lo stesso metodo). Il probe esercita
// la catena parser(Remap)→linker(rewrite canonico)→backend(runtime).
using System;
using System.IO;
using System.Threading.Tasks;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public sealed class CollectionsMemberParityTests
{
    private const string Probe = """
        IMPORT COLLECTIONS
        IMPORT JSON

        SUB Main
            DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER
            l.Add(1) : l.Add(2) : l.Add(3)
            PRINT "count_lower="; l.count()
            PRINT "count_canon="; l.Count()
            DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
            d.set("a", 1) : d.add("b", 2)
            PRINT "dcount="; d.count()
            PRINT "get_lower="; d.get("a")
            PRINT "keys_lower="; JSON.Stringify(d.keys())
        END SUB
        """;

    private static string WriteProbe(out string directory)
    {
        directory = Path.Combine(Path.GetTempPath(), $"CollParity_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var sourcePath = Path.Combine(directory, "Main.bas");
        File.WriteAllText(sourcePath, Probe);
        return sourcePath;
    }

    [Fact]
    public void LowercaseMembers_Count_Get_Keys_OnIl()
    {
        var sourcePath = WriteProbe(out var directory);
        var previousDirectory = Directory.GetCurrentDirectory();
        try
        {
            Directory.SetCurrentDirectory(directory);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            Assert.NotNull(program?.MainModule);
            var asmBuilder = IlProgramCompiler.CompileToAssembly(program!, "IlCollParity_" + Guid.NewGuid().ToString("N"), out _);
            var dll = Path.Combine(directory, "IlCollParity.dll");
            using (var fs = File.Create(dll)) asmBuilder.Save(fs);
            var assembly = System.Reflection.Assembly.LoadFile(dll);
            var main = assembly.GetType("GeneratedProgram")!.GetMethod("Main", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)!;
            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var task = Task.Run(() => main.Invoke(null, null));
                Assert.True(task.Wait(TimeSpan.FromSeconds(20)), $"[IL] TIMEOUT\n{sw}");
                task.GetAwaiter().GetResult();
            }
            finally { Console.SetOut(originalOut); }

            var output = sw.ToString();
            Assert.Contains("count_lower= 3", output);
            Assert.Contains("count_lower=", output);
            Assert.DoesNotContain("count_lower= 2", output); // era UBOUND (bug case-sensitive)
            Assert.Contains("get_lower= 1", output);
            Assert.Contains("keys_lower=", output);
            Assert.Contains("\"a\"", output); // d.set("a",1) DEV'ESSERE scritto (era no-op silenzioso)
            Assert.Contains("\"b\"", output);
        }
        finally
        {
            Directory.SetCurrentDirectory(previousDirectory);
            try { Directory.Delete(directory, recursive: true); } catch { }
        }
    }

    [Fact]
    public void LowercaseMembers_Count_Get_Keys_OnPCode()
    {
        var sourcePath = WriteProbe(out var directory);
        var previousDirectory = Directory.GetCurrentDirectory();
        try
        {
            Directory.SetCurrentDirectory(directory);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            Assert.NotNull(program?.MainModule);
            var chunk = new global::Compiler.Compiler().Compile(program!);
            using var driver = new Basic.IlBackend.PCode.PCodeDebugDriver();
            driver.Start(chunk, []);
            var deadline = System.Diagnostics.Stopwatch.StartNew();
            while (deadline.ElapsedMilliseconds < 15000)
            {
                if (!driver.IsRunning) break;
                Thread.Sleep(20);
            }
            var output = driver.TakeOutput();
            Assert.Contains("count_lower= 3", output);
            Assert.DoesNotContain("count_lower= 2", output);
            Assert.Contains("get_lower= 1", output);
            Assert.Contains("\"a\"", output);
        }
        finally
        {
            Directory.SetCurrentDirectory(previousDirectory);
            try { Directory.Delete(directory, recursive: true); } catch { }
        }
    }
}
