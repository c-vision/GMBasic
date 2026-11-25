// FILE: Basic.IlBackend.LengthPseudoPropertyTests.cs
// `.Length` NON è una proprietà del linguaggio: solo StringBuilder.Length e
// RegexMatch.Length esistono. Su stringa/lista/dict/array la scrittura deve fallire nel
// LINKER, prima del backend, così IL/PCode/VM-C (e il C99, che ha lo stesso rifiuto
// nell'emitter) danno lo stesso errore invece di quattro comportamenti diversi
// (NotSupportedException, vuoto, numero di chiavi, 0).
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class LengthPseudoPropertyTests
{
    private static void AssertLengthRejected(string source)
    {
        var ex = Assert.Throws<BasicException>(() =>
            Basic.Orchestrator.CompileFromString(source, "length_pseudo.bas", null, verbose: false));
        Assert.Contains(".Length", ex.Message);
        Assert.Contains("non è una proprietà", ex.Message);
    }

    [Fact]
    public void StringLength_IsRejected() => AssertLengthRejected("""
        SUB Main()
          DIM s AS STRING
          s = "abcd"
          PRINT s.Length
        END SUB
        """);

    [Fact]
    public void ListLength_IsRejected() => AssertLengthRejected("""
        SUB Main()
          DIM l = [1, 2, 3]
          PRINT l.Length
        END SUB
        """);

    [Fact]
    public void DictLength_IsRejected() => AssertLengthRejected("""
        SUB Main()
          DIM d AS DICT
          d("a") = 1
          PRINT d.Length
        END SUB
        """);

    [Fact]
    public void ArrayLength_IsRejected() => AssertLengthRejected("""
        SUB Main()
          DIM a(3) AS LONG
          a(0) = 1
          PRINT a.Length
        END SUB
        """);

    [Fact]
    public void HyperLogLogLength_IsRejected() => AssertLengthRejected("""
        IMPORT COLLECTIONS
        SUB Main()
          DIM h AS HLL
          h.Add("a")
          PRINT h.Length
        END SUB
        """);

    [Fact]
    public void BagLength_IsRejected() => AssertLengthRejected("""
        IMPORT COLLECTIONS
        SUB Main()
          DIM b AS BAG
          b.Add(1)
          PRINT b.Length
        END SUB
        """);

    [Fact]
    public void StringBuilderLength_StillCompiles()
    {
        var program = Basic.Orchestrator.CompileFromString("""
            IMPORT STRINGS
            SUB Main()
              DIM sb AS STRINGBUILDER
              sb.Append("abcd")
              PRINT sb.Length
              PRINT sb.Capacity
            END SUB
            """, "sb_length.bas", null, verbose: false);
        Assert.NotNull(program.MainModule);
    }
}
