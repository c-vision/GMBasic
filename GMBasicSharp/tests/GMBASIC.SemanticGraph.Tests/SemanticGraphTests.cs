// FILE: tests/GMBASIC.SemanticGraph.Tests/SemanticGraphTests.cs — §4.10: test della libreria.
using Basic;
using Basic.Pipeline;
using GMBasic.SemanticGraph;
using Xunit;

namespace GMBasic.SemanticGraph.Tests;

public class SemanticGraphTests
{
    private static SemanticGraph Build(string source, string label = "v1")
    {
        var (_, program) = SourcePipeline.CompileFromStringWithRegistry(
            source, "test.bas", Basic.StdLib.Instance, SourcePipeline.DefaultNamespacesStandard);
        return SemanticGraphExtractor.Extract(program, label);
    }

    private const string Src = """
        SUB Saluta(nome AS STRING)
            PRINT "ciao " + nome
        END SUB

        FUNCTION Doppio(x AS INTEGER) AS INTEGER
            DIM r AS INTEGER
            r = x * 2
            Saluta("mondo")
            RETURN r
        END FUNCTION

        SUB Main()
            DIM v AS INTEGER
            v = Doppio(21)
            PRINT v
        END SUB
        """;

    [Fact]
    public void Sub_is_Procedure_function_is_Function()
    {
        var g = Build(Src);
        Assert.True(g.Symbols.Any(s => s.Name == "Saluta" && s.Kind == SemanticSymbolKind.Procedure), "Saluta SUB -> Procedure");
        Assert.True(g.Symbols.Any(s => s.Name == "Doppio" && s.Kind == SemanticSymbolKind.Function), "Doppio FUNCTION -> Function");
        Assert.True(g.Symbols.Any(s => s.Name == "Main" && s.Kind == SemanticSymbolKind.Procedure), "Main SUB -> Procedure");
    }

    [Fact]
    public void Extract_finds_variable_symbols()
    {
        var g = Build(Src);
        Assert.Contains(g.Symbols, s => s.Name == "r");
        Assert.Contains(g.Symbols, s => s.Name == "v");
        Assert.Contains(g.Symbols, s => s.Name == "nome" && s.Kind == SemanticSymbolKind.Parameter);
    }

    [Fact]
    public void Definitions_have_stable_ids_distinct_per_symbol()
    {
        var g = Build(Src);
        var ids = g.Symbols.Select(s => s.Id).ToList();
        Assert.Equal(ids.Count, ids.Distinct().Count());
        Assert.All(g.Symbols, s => Assert.False(string.IsNullOrWhiteSpace(s.Id)));
    }

    [Fact]
    public void Calls_are_resolved_by_name()
    {
        var g = Build(Src);
        var saluta = g.Definition("Saluta");
        Assert.NotNull(saluta);
        Assert.NotEmpty(g.Callers(saluta!.Id));
    }

    [Fact]
    public void Impact_includes_transitive_callers()
    {
        const string chain = """
            SUB LivelloC()
                PRINT "c"
            END SUB
            SUB LivelloB()
                LivelloC()
            END SUB
            SUB LivelloA()
                LivelloB()
            END SUB
            """;
        var g = Build(chain);
        var c = g.Definition("LivelloC");
        Assert.NotNull(c);
        var impact = g.Impact(c!.Id).Select(i => g.GetSymbol(i.SymbolId)?.Name).ToList();
        Assert.Contains("LivelloB", impact);
        Assert.Contains("LivelloA", impact);
    }

    [Fact]
    public void Snapshot_roundtrip_and_diff_detects_added_and_removed()
    {
        var g1 = Build(Src, "v1");
        const string Src2 = """
            SUB Saluta(nome AS STRING)
                PRINT "ciao " + nome
            END SUB
            SUB Main()
                Saluta("mondo")
            END SUB
            """;
        var g2 = Build(Src2, "v2");

        var snap1 = g1.ToSnapshot("v1");
        var snap2 = g2.ToSnapshot("v2");
        var diff = GraphDiff.Diff(snap1, snap2, GraphDiff.Materialize(snap2));

        Assert.Contains(diff.SymbolChanges, c => c.Kind == SymbolChangeKind.Removed && c.Before?.Name == "Doppio");
        Assert.Contains(g2.Symbols, s => s.Name == "Saluta");
        Assert.DoesNotContain(diff.SymbolChanges, c => c.Before?.Name == "Saluta" || c.After?.Name == "Saluta");
        Assert.Equal("v1", diff.FromLabel);
        Assert.Equal("v2", diff.ToLabel);
    }
}
