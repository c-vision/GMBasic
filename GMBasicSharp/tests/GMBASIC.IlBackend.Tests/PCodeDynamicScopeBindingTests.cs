// FILE: tests/GMBASIC.IlBackend.Tests/PCodeDynamicScopeBindingTests.cs
//
// Il buco di copertura che ha lasciato vivere il bug `QUERY` (P-Code):
// il test BSP asseriva solo che `QUERY["action"]` COMPASSSE nel sorgente e che la pagina
// compilasse — mai che la lettura restituisse il valore. Qui si asserisce direttamente
// l'opcode emesso, che e' deterministico e non richiede un server.
//
// Due comportamenti da bloccare, perche' sono opposti e si escludono:
//   1) FORM/QUERY usate come NOME BSP (`QUERY["k"]`, nessun simbolo omonimo) -> DEVE emettere
//      OP_LOAD_DYNAMIC_SCOPE (convenzione dell'host, vedi TryServeBspPage).
//   2) Una variabile REALE che si chiama come la keyword (`DIM Query = 9`) -> NON DEVE passare
//      dal dynamic scope: vince il simbolo dichiarato. Prima del fix la lettura restituiva
//      silenziosamente il vuoto della keyword (bug bd5f2b734).

using System;
using System.Linq;
using Basic.IlBackend.PCode;
using Compiler;
using Xunit;

namespace Basic.IlBackend.Tests;

public class PCodeDynamicScopeBindingTests
{
    private static bool EmitsDynamicScope(string source, string fileName)
    {
        var program = global::Basic.Orchestrator.CompileFromString(source, fileName, null, verbose: false);
        Assert.NotNull(program?.MainModule);
        var chunk = new global::Compiler.Compiler().Compile(program!);
        return Enumerable.Range(0, Math.Max(0, chunk.Code.Count - 2))
            .Any(ip => chunk.Code[ip] == (byte)OpCode.OP_LOAD_DYNAMIC_SCOPE);
    }

    [Fact]
    public void Query_UsataComeNomeBsp_Emette_Dynamic_Scope()
    {
        // Nessun simbolo `Query` dichiarato: e' il nome BSP, va risolto dall'host.
        var source = """
            SUB Main()
              DIM nome = QUERY["name"]
              PRINT nome
            END SUB
            """;

        Assert.True(EmitsDynamicScope(source, "BspQueryName.bas"),
            "QUERY[\"name\"] senza variabile omonima deve passare da OP_LOAD_DYNAMIC_SCOPE: " +
            "e' la convenzione con cui l'host (TryServeBspPage) serve le pagine BSP.");
    }

    [Fact]
    public void Variabile_Dichiarata_Con_Nome_Keyword_Non_Emette_Dynamic_Scope()
    {
        // Il simbolo dichiarato vince: la keyword non deve rubare la lettura.
        var source = """
            SUB Main()
              DIM Query = 9
              PRINT Query
            END SUB
            """;

        Assert.False(EmitsDynamicScope(source, "BspQueryShadowedByVariable.bas"),
            "Con `DIM Query` il simbolo dichiarato deve prevalere: emettere OP_LOAD_DYNAMIC_SCOPE " +
            "restituirebbe il vuoto della keyword invece di 9 (bug silenzioso, vedi bd5f2b734).");
    }

    [Fact]
    public void Variabile_Dichiarata_Legge_Il_Valore_Assegnato()
    {
        // Stessa cosa a runtime, non solo a livello opcode: il valore deve arrivare, non il vuoto.
        var source = """
            SUB Main()
              DIM Query = 9
              PRINT "Q="; Query
            END SUB
            """;

        var program = global::Basic.Orchestrator.CompileFromString(source, "BspQueryValue.bas", null, verbose: false);
        Assert.NotNull(program?.MainModule);
        var chunk = new global::Compiler.Compiler().Compile(program!);

        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        var result = PCodeVmRunner.Run(chunk, cancellation.Token);

        Assert.True(result.Success, result.Output);
        Assert.Contains("Q= 9", result.Output);
    }
}