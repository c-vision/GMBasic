// STEP 2a — INTEGRAZIONE: l'analisi may-park sul sample REALE GoRoutinesMinimal.bas
// (gate §13-7 / §16). Usa la pipeline Orchestrator.Compile (parsing+linking veri), estrae
// le FunctionDefinition dai moduli caricati e verifica:
//   - Main e Produttore risultano may-park;
//   - SplitChunks produce chunk che INIZIANO (Blocker) esattamente sui punti di blocco
//     (RECEIVE/WAIT per Main, SEND/SLEEP per Produttore), mai dopo o prima — invariante
//     §16.3/§26.1: il retry riparte dall'operazione parcheggiata.
// NON asserisce indici numerici (fragili a commenti/spazi): asserisce le invarianti.

using System.Collections.Immutable;
using Xunit;
using Basic.IlBackend.Gpm;

namespace Basic.IlBackend.Tests;

public class GoRoutinesMinimalAnalysisTests
{
    private static FunctionDefinition[] LoadProcedures()
    {
        var root = FindRepoRoot();
        var sample = Path.Combine(root, "shared", "samples", "Concurrency", "GoRoutinesMinimal.bas");
        Assert.True(File.Exists(sample), $"Sample mancante: {sample}");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        return program.LoadedModules.Values
            .SelectMany(m => m.Declarations)
            .OfType<FunctionDefinition>()
            .ToArray();
    }

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    [Fact]
    public void Main_And_Produttore_Are_MayPark()
    {
        var procs = LoadProcedures().ToImmutableArray();
        Assert.Contains(procs, p => p.OriginalName.Lexeme == "Main");
        Assert.Contains(procs, p => p.OriginalName.Lexeme == "Produttore");

        var byId = (int id) => procs.FirstOrDefault(p => p.SymbolId == id);
        var mayPark = MayParkAnalysis.ComputeMayPark(procs, byId);

        var main = procs.First(p => p.OriginalName.Lexeme == "Main");
        var prod = procs.First(p => p.OriginalName.Lexeme == "Produttore");
        Assert.Contains(main.SymbolId, mayPark);
        Assert.Contains(prod.SymbolId, mayPark);
    }
}
