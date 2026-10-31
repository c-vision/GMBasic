using Basic.IlBackend.PCode;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public sealed class PCodeUnwindTests
{
    [Fact]
    public void ThrowAcrossCall_UnwindsToOuterCatch()
    {
        var output = CompileAndRun("""
            SUB FailDeep()
              THROW 77
            END SUB

            SUB Middle()
              FailDeep()
              PRINT "UNREACHABLE-MIDDLE"
            END SUB

            SUB Main()
              TRY
                Middle()
                PRINT "UNREACHABLE-MAIN"
              CATCH Ex AS ERR
                PRINT "CAUGHT"
              END TRY
              PRINT "DONE"
            END SUB
            """);

        Assert.Contains("CAUGHT", output);
        Assert.Contains("DONE", output);
        Assert.DoesNotContain("UNREACHABLE", output);
    }

    [Fact]
    public void ReturnFromTry_RemovesStaleHandlerBeforeLaterThrow()
    {
        var output = CompileAndRun("""
            SUB ReturnInsideTry()
              TRY
                RETURN
              CATCH InnerError AS ERR
                PRINT "STALE-INNER"
              END TRY
            END SUB

            SUB Main()
              ReturnInsideTry()
              TRY
                THROW 91
              CATCH OuterEx AS ERR
                PRINT "OUTER"
              END TRY
              PRINT "DONE"
            END SUB
            """);

        Assert.Contains("OUTER", output);
        Assert.Contains("DONE", output);
        Assert.DoesNotContain("STALE-INNER", output);
    }

    [Fact]
    public void GotoOutOfTry_RemovesOnlyTheAbandonedHandler()
    {
        var output = CompileAndRun("""
            SUB Main()
              TRY
                GOTO OutsideTry
              CATCH InnerEx AS ERR
                PRINT "STALE-INNER"
              END TRY

            OutsideTry:
              TRY
                THROW 33
              CATCH OuterEx AS ERR
                PRINT "OUTER"
              END TRY
              PRINT "DONE"
            END SUB
            """);

        Assert.Contains("OUTER", output);
        Assert.Contains("DONE", output);
        Assert.DoesNotContain("STALE-INNER", output);
    }

    [Fact]
    public void GotoIntoTry_IsRejectedBeforeExecution()
    {
        var error = Assert.Throws<InvalidOperationException>(() => CompileOnly("""
            SUB Main()
              TRY
            InsideTry:
                PRINT "ILLEGAL"
              CATCH Ex AS ERR
                PRINT "CATCH"
              END TRY
              GOTO InsideTry
            END SUB
            """));

        Assert.Contains("protected TRY/CATCH/FINALLY region", error.Message);
    }

    [Fact]
    public void GosubIntoFinally_IsRejectedBeforeExecution()
    {
        var error = Assert.Throws<InvalidOperationException>(() => CompileOnly("""
            SUB Main()
              GOSUB InsideFinally
              TRY
                PRINT "TRY"
              FINALLY
            InsideFinally:
                PRINT "ILLEGAL"
              END TRY
            END SUB
            """));

        Assert.Contains("OP_GOSUB", error.Message);
        Assert.Contains("protected TRY/CATCH/FINALLY region", error.Message);
    }

    [Fact]
    public void GotoWithinSameTry_RemainsLegal()
    {
        var output = CompileAndRun("""
            SUB Main()
              TRY
                GOTO InsideTry
                PRINT "UNREACHABLE"
            InsideTry:
                PRINT "INSIDE-OK"
              CATCH Ex AS ERR
                PRINT "CATCH"
              END TRY
            END SUB
            """);

        Assert.Contains("INSIDE-OK", output);
        Assert.DoesNotContain("UNREACHABLE", output);
    }

    [Fact]
    public void OnJumpIntoCatch_IsRejectedBeforeExecution()
    {
        var error = Assert.Throws<InvalidOperationException>(() => CompileOnly("""
            SUB Main()
              DIM Choice AS INTEGER = 1
              ON Choice GOTO InsideCatch
              TRY
                PRINT "TRY"
              CATCH Ex AS ERR
            InsideCatch:
                PRINT "ILLEGAL"
              END TRY
            END SUB
            """));

        Assert.Contains("OP_ON_JUMP_GOTO", error.Message);
        Assert.Contains("protected TRY/CATCH/FINALLY region", error.Message);
    }

    [Fact]
    public void OnErrorHandlerInsideTry_IsRejectedBeforeExecution()
    {
        var error = Assert.Throws<InvalidOperationException>(() => CompileOnly("""
            SUB Main()
              ON ERROR GOTO InsideTry
              TRY
            InsideTry:
                PRINT "ILLEGAL"
              CATCH Ex AS ERR
                PRINT "CATCH"
              END TRY
            END SUB
            """));

        Assert.Contains("OP_SET_ERR_HANDLER_GOTO", error.Message);
        Assert.Contains("protected TRY/CATCH/FINALLY region", error.Message);
    }

    [Fact]
    public void ResumeLabelInsideFinally_IsRejectedBeforeExecution()
    {
        var error = Assert.Throws<InvalidOperationException>(() => CompileOnly("""
            SUB Main()
              ON ERROR GOTO ErrorLabel
              ERROR 5
            ErrorLabel:
              RESUME InsideFinally
              TRY
                PRINT "TRY"
              FINALLY
            InsideFinally:
                PRINT "ILLEGAL"
              END TRY
            END SUB
            """));

        Assert.Contains("OP_RESUME", error.Message);
        Assert.Contains("protected TRY/CATCH/FINALLY region", error.Message);
    }

    [Fact]
    public void OnJumpLegalTarget_UsesPatchedAbsoluteAddress()
    {
        var output = CompileAndRun("""
            SUB Main()
              DIM Choice AS INTEGER = 2
              ON Choice GOTO FirstTarget, SecondTarget
              PRINT "FAIL:FELL-THROUGH"
            FirstTarget:
              PRINT "FAIL:FIRST-TARGET"
              GOTO FinishedLabel
            SecondTarget:
              PRINT "ON-JUMP-ABSOLUTE-OK"
            FinishedLabel:
            END SUB
            """);

        Assert.Contains("ON-JUMP-ABSOLUTE-OK", output);
        Assert.DoesNotContain("FAIL:", output);
    }

    [Fact]
    public void OnGosubLegalTarget_ReturnsThroughGosubContinuation()
    {
        var output = CompileAndRun("""
            SUB Main()
              DIM Choice AS INTEGER = 1
              ON Choice GOSUB SelectedTarget
              PRINT "ON-GOSUB-RETURNED"
              GOTO FinishedLabel
            SelectedTarget:
              PRINT "ON-GOSUB-TARGET-OK"
              RETURN
            FinishedLabel:
            END SUB
            """, TimeSpan.FromSeconds(1));

        Assert.Contains("ON-GOSUB-TARGET-OK", output);
        Assert.Contains("ON-GOSUB-RETURNED", output);
        Assert.DoesNotContain("FAIL:", output);
    }

    [Fact]
    public void FinallyError_SupersedesOriginalUnwindBeforeOnErrorHandler()
    {
        var output = CompileAndRun("""
            SUB Main()
              ON ERROR GOTO LegacyHandler
              TRY
                GOTO OriginalTarget
              FINALLY
                ERROR 702, "cleanup-error"
                PRINT "FAIL:FINALLY-CONTINUED"
              END TRY

            OriginalTarget:
              PRINT "FAIL:ORIGINAL-UNWIND-CONTINUED"
              GOTO Finished

            LegacyHandler:
              PRINT "ON-ERROR-CLEANUP-OK"
              ON ERROR GOTO 0

            Finished:
              PRINT "ON-ERROR-SUPERSEDE-DONE"
            END SUB
            """, TimeSpan.FromSeconds(1));

        Assert.Contains("ON-ERROR-CLEANUP-OK", output);
        Assert.Contains("ON-ERROR-SUPERSEDE-DONE", output);
        Assert.DoesNotContain("FAIL:", output);
    }

    private static global::Compiler.CompiledChunk CompileOnly(string source)
    {
        var directory = Path.Combine(Path.GetTempPath(), $"PCodeUnwindCompile_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            var sourcePath = Path.Combine(directory, "Main.bas");
            File.WriteAllText(sourcePath, source);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            return new global::Compiler.Compiler().Compile(program);
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    private static string CompileAndRun(string source, TimeSpan? timeout = null)
    {
        var directory = Path.Combine(Path.GetTempPath(), $"PCodeUnwind_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        try
        {
            var sourcePath = Path.Combine(directory, "Main.bas");
            File.WriteAllText(sourcePath, source);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            var chunk = new global::Compiler.Compiler().Compile(program);
            using var cancellation = timeout is { } duration
                ? new CancellationTokenSource(duration)
                : new CancellationTokenSource();
            var result = PCodeVmRunner.Run(chunk, cancellation.Token);
            Assert.True(result.Success, result.Output);
            return result.Output;
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }
}
