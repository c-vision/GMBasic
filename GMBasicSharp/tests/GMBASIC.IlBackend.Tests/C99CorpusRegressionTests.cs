// FILE: C99CorpusRegressionTests.cs — i sample che il backend C NON riusciva a emettere.
//
// PERCHÉ. Il gate `tools/sh/c99_emit_gate.sh` misura tutto il corpus, ma gira a mano/CI: qui
// i casi che erano ROSSI restano fissati nella suite, così una regressione si vede subito
// con `dotnet test` (pochi secondi) e non solo con uno sweep completo.
//
// CONTESTO (2026-11-19): partendo da 33/3659 non emettibili, i rossi erano riconducibili a
// quattro famiglie, tutte coperte qui sotto:
//   - risoluzione moduli/progetti (`importFiles`, `app.settings.json` risalendo);
//   - dominio TORCH (statement + nomi che collidono nella member-table + letterale FROM);
//   - `a[i][j] = v` (workaround con variabile intermedia);
//   - tipizzazione esplicita richiesta dal C per i valori dinamici (`DIM d AS JSON`) e
//     argomenti boxed in contesto stringa.
using System;
using System.IO;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class C99CorpusRegressionTests
{
    private static readonly string Root = FindRoot();

    private static string FindRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "GMBasicSharp"))
                && Directory.Exists(Path.Combine(dir.FullName, "shared")))
                return dir.FullName;
            dir = dir.Parent;
        }
        throw new DirectoryNotFoundException("root del monorepo non trovata");
    }

    [Theory]
    // Documents: `DIM d AS JSON` (il C esige il tipo per un val dinamico).
    [InlineData("shared/samples/Documents/MarkdownAstDemo.bas")]
    [InlineData("shared/samples/Documents/ReadmeKnowledgeBase.bas")]
    [InlineData("shared/samples/Documents/StaticSiteFromMarkdown.bas")]
    // MVC: `a[i][j] = v` → variabile intermedia.
    [InlineData("shared/samples/Http/Mvc/CrudDemo/MvcCrud.bas")]
    [InlineData("shared/samples/Web/Mvc/MvcApiDemo.bas")]
    // TORCH: statement di dominio + nomi fuori member-table + FROM letterale.
    [InlineData("shared/samples/Torch/TorchCommands.bas")]
    [InlineData("shared/samples/Torch/TorchAll.bas")]
    // Risoluzione moduli: importFiles + app.settings.json risalendo.
    [InlineData("shared/samples/Modules/FolderResolution/AllFolders.bas")]
    [InlineData("shared/samples/Modules/FolderResolution/chain/Chain.bas")]
    // AI CONNECTOR LIST (forma legacy, verbo senza TokenKind).
    [InlineData("shared/samples/Connectors/ConnectorBasic.bas")]
    // Server grande: crypto (FROMBASE64/VERIFYRAW) + JSON boxed.
    [InlineData("shared/samples/PackageRegistry/PackageServer.bas")]
    public void Sample_Emits_On_C99(string rel)
    {
        var sample = Path.Combine(Root, rel);
        Assert.True(File.Exists(sample), $"sample mancante: {rel}");
        C99Emission.Require(sample);
    }
}
