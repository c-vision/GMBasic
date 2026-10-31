// Regressioni del prologo sintetico del Compiler P-Code condiviso da PCodeVM e VM/C.
// Gli opcode e gli operandi condividono lo stesso byte stream: un post-pass non deve mai
// cercare un opcode confrontando ogni singolo byte, perché 0x3f è anche un operando valido.
using Basic.IlBackend.PCode;
using Compiler;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class PCodeCompilerMainPrologueTests
{
    [Fact]
    public void MainWithoutInitializers_PreservesConstantIndexWhoseLowByteEqualsHalt()
    {
        var prints = string.Join('\n', Enumerable.Range(0, 70).Select(i => $"PRINT \"C{i:D2}\""));
        var source = $"""
            SUB Main
            {prints}
            END SUB
            """;

        var program = Basic.Orchestrator.CompileFromString(
            source,
            "PCodeMainPrologueOperand63.bas",
            null,
            verbose: false);
        Assert.NotNull(program?.MainModule);

        var chunk = new global::Compiler.Compiler().Compile(program!);

        Assert.Equal("C63", chunk.Constants[63]);
        Assert.Contains(
            Enumerable.Range(0, chunk.Code.Count - 2),
            ip => chunk.Code[ip] == (byte)OpCode.OP_PUSH_CONST &&
                  chunk.Code[ip + 1] == 0 &&
                  chunk.Code[ip + 2] == 63);

        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        var result = PCodeVmRunner.Run(chunk, cancellation.Token);

        Assert.True(result.Success, result.Output);
        var actual = result.Output
            .Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var expected = Enumerable.Range(0, 70).Select(i => $"C{i:D2}").ToArray();
        Assert.Equal(expected, actual);
    }
}
