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
        var sample = Path.Combine(root, "shared", "samples", "PackageRegistry", "PackageServer.bas");
        Assert.True(File.Exists(sample));
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program);
        Assert.NotNull(program.MainModule);
        Assert.NotEmpty(program.PreprocessedFiles);
    }
    /// <summary>Radice del monorepo: la directory che contiene GMBasicSharp/ e shared/.</summary>
    private static string FindRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "GMBasicSharp"))
                && Directory.Exists(Path.Combine(dir.FullName, "shared")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }
}
