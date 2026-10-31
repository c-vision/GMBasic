using Basic.IlBackend.PCode;
using Compiler;
using Xunit;

namespace Basic.IlBackend.Tests;

/// <summary>
/// Guard della fusione nativa "tutto il ciclo in un opcode" (OP_NATIVE_STRING_APPEND_LOOP /
/// OP_NATIVE_LOOP_ACC): quegli opcode eseguono TUTTE le iterazioni del FOR e ne consumano lo
/// stato, quindi sono ammessi SOLO quando l'assegnazione è l'INTERO corpo del ciclo.
/// Con un'altra istruzione nel corpo (es. PRINT) il corpo veniva eseguito una volta sola e il
/// contatore saltava al valore finale — bug
/// `Bugs/(open) - PCode-For-In-Function-Corpo-Loop.md`, ripro qui sotto.
/// </summary>
[Collection("GpmRuntimeState")]
public sealed class PCodeForLoopFusionGuardTests
{
    private static string CompileAndRun(string source, out byte[] code)
    {
        var projectDirectory = Path.Combine(Path.GetTempPath(), $"PCodeForFusion_{Guid.NewGuid():N}");
        Directory.CreateDirectory(projectDirectory);
        var sourcePath = Path.Combine(projectDirectory, "Main.bas");
        try
        {
            File.WriteAllText(sourcePath, source);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            var chunk = new global::Compiler.Compiler().Compile(program);
            code = [.. chunk.Code];
            var result = PCodeVmRunner.Run(chunk);
            Assert.True(result.Success, result.Output);
            return result.Output;
        }
        finally
        {
            if (Directory.Exists(projectDirectory)) Directory.Delete(projectDirectory, recursive: true);
        }
    }

    [Fact]
    public void MixedBody_StringAppend_IsNotFused_AndRunsAllIterations()
    {
        var output = CompileAndRun("""
            SUB Main()
              DIM out$ AS STRING = ""
              DIM s AS LONG
              FOR s = 0 TO 3
                PRINT "cpos"; s
                out$ = out$ + "x"
              NEXT
              PRINT "clen="; LEN(out$)
            END SUB
            """, out var code);

        // La fusione NON deve essere emessa (il corpo ha anche il PRINT): senza questo guard il
        // corpo girava UNA sola volta e il contatore saltava a 4.
        Assert.DoesNotContain((byte)OpCode.OP_NATIVE_STRING_APPEND_LOOP, code);

        Assert.Contains("cpos 0", output);
        Assert.Contains("cpos 1", output);
        Assert.Contains("cpos 2", output);
        Assert.Contains("cpos 3", output);
        Assert.Contains("clen=4", output.Replace(" ", "", StringComparison.Ordinal));
    }

    [Fact]
    public void MixedBody_NumericAccumulator_IsNotFused_AndRunsAllIterations()
    {
        var output = CompileAndRun("""
            SUB Main()
              DIM s AS LONG
              DIM acc AS DOUBLE = 0
              FOR s = 0 TO 3
                PRINT "it="; s
                acc = acc + SQR(s)
              NEXT
              PRINT "acc="; acc
            END SUB
            """, out var code);

        // Stessa classe di bug per l'accumulatore numerico (OP_NATIVE_LOOP_ACC esegue tutto il
        // ciclo): con il PRINT nel corpo la fusione non è ammessa.
        Assert.DoesNotContain((byte)OpCode.OP_NATIVE_LOOP_ACC, code);

        Assert.Contains("it= 0", output);
        Assert.Contains("it= 1", output);
        Assert.Contains("it= 2", output);
        Assert.Contains("it= 3", output);
        // 0 + sqrt(1) + sqrt(2) + sqrt(3)
        Assert.Contains("acc=", output);
    }

    [Fact]
    public void WholeBody_StringAppend_IsStillFused()
    {
        var output = CompileAndRun("""
            SUB Main()
              DIM out$ AS STRING = ""
              DIM s AS LONG
              FOR s = 0 TO 3
                out$ = out$ + "x"
              NEXT
              PRINT "clen="; LEN(out$)
            END SUB
            """, out var code);

        // Il caso d'uso d'origine della fusione (corpo = la sola assegnazione) deve restare
        // fuso: il guard non deve spegnere l'ottimizzazione dove è corretta.
        Assert.Contains((byte)OpCode.OP_NATIVE_STRING_APPEND_LOOP, code);
        Assert.Contains("clen=4", output.Replace(" ", "", StringComparison.Ordinal));
    }

    [Fact]
    public void ChainOfThreeOperands_IsNotFused_AndKeepsEveryTerm()
    {
        var output = CompileAndRun("""
            SUB Main()
              DIM i AS LONG
              DIM acc AS DOUBLE = 0
              FOR i = 1 TO 3
                acc = acc + i + i
              NEXT
              PRINT "chain="; acc
              acc = 0
              FOR i = 1 TO 3
                acc = acc + ROUND(i) + i * 2
              NEXT
              PRINT "mixed="; acc
              acc = 0
              FOR i = 1 TO 3
                acc = acc + ROUND(i) + ABS(i)
              NEXT
              PRINT "two="; acc
            END SUB
            """, out var code);

        // `acc + I + I` e `acc + f(I) + I*m` NON sono pattern supportati. Lo scorporo
        // multi-termine del matcher li accettava comunque e IGNORAVA il sotto-terminale:
        // PCodeVM/VM-C davano 6 e 12 dove IL/C99 davano 12 e 18 (divergenza tra backend,
        // 2026-09-29 — TryEmitNativeLoopAccumulator, guard "IsCounterTerm" su entrambi i rami).
        var noSpace = output.Replace(" ", "", StringComparison.Ordinal);
        Assert.Contains("chain=12", noSpace);
        Assert.Contains("mixed=18", noSpace);

        // La forma legitima a due termini (acc + f(I) + g(I)) resta fusa e vale 12: il fix non
        // deve spegnere la fusione dove il pattern è reale.
        Assert.Contains("two=12", noSpace);
        Assert.Contains((byte)OpCode.OP_NATIVE_LOOP_ACC, code);
    }
}
