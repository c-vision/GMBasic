using Basic.IlBackend.PCode;
using Compiler;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public sealed class PCodeNativeLoopSafepointTests
{
    [Fact]
    public void FusedAccumulatorLoop_YieldsToQueuedGoroutine_AndResumesExactly()
    {
        var projectDirectory = Path.Combine(Path.GetTempPath(), $"PCodeNativeLoop_{Guid.NewGuid():N}");
        Directory.CreateDirectory(projectDirectory);
        var sourcePath = Path.Combine(projectDirectory, "Main.bas");
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            File.WriteAllText(sourcePath, """
                IMPORT CONCURRENCY

                SUB Busy()
                  DIM Acc AS DOUBLE = 0
                  FOR I = 1 TO 100000
                    Acc = Acc + I
                  NEXT I
                  PRINT "BUSY:"; Acc
                END SUB

                SUB Quick()
                  PRINT "QUICK"
                END SUB

                SUB Main()
                  GO SUB Busy()
                  GO SUB Quick()
                END SUB
                """);

            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            var chunk = new global::Compiler.Compiler().Compile(program);
            Assert.Contains((byte)OpCode.OP_NATIVE_LOOP_ACC, chunk.Code);

            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
            var result = PCodeVmRunner.Run(chunk);

            Assert.True(result.Success, result.Output);
            var quick = result.Output.IndexOf("QUICK", StringComparison.Ordinal);
            var busy = result.Output.IndexOf("BUSY:", StringComparison.Ordinal);
            Assert.True(quick >= 0 && busy > quick, result.Output);
            Assert.Contains("5000050000", result.Output.Replace(".", "", StringComparison.Ordinal));
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
            if (Directory.Exists(projectDirectory))
                Directory.Delete(projectDirectory, recursive: true);
        }
    }
}
