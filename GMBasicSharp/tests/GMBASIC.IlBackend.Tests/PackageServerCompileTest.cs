// Compile-only: verifica che PackageServer.bas (con config da package-registry.json)
// compili senza errori. Non esegue il server (evita hang).
using System.IO;
using Xunit;

namespace Basic.IlBackend.Tests;

public class PackageServerCompileTest
{
    [Fact]
    public void PackageServer_Compiles()
    {
        var root = FindRoot();
        var sample = Path.Combine(root, "samples", "Community", "PackageRegistry", "PackageServer.bas");
        Assert.True(File.Exists(sample));
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program);
        Assert.NotNull(program.MainModule);
        Assert.NotEmpty(program.PreprocessedFiles);
    }
    private static string FindRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            // MONOREPO: nessuna GMBASIC.sln; la root del repo C e GMBasic/ (accanto a GMBasicSharp/)
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx")))
            {
                // MONOREPO: i sample stanno in <repo>/shared/samples (accanto a GMBasic/).
                // Si restituisce la base che CONTIENE davvero samples/Community/PackageRegistry.
                var sharedBase = Path.Combine(dir.FullName, "shared");
                if (Directory.Exists(Path.Combine(sharedBase, "samples", "Community", "PackageRegistry")))
                    return sharedBase;
                var gm = Path.Combine(dir.FullName, "GMBasic");
                if (Directory.Exists(Path.Combine(gm, "samples", "Community", "PackageRegistry"))) return gm;
                return gm;
            }
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }
}
