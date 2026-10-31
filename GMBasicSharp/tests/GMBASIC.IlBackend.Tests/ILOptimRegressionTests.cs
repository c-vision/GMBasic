// ILOptimRegressionTests: oracoli che esercitano la pipeline IL REALE (IlProgramCompiler.Compile,
// ILOptim.Optimize ON di default) via subprocess — a differenza di RunIl/RunSample (usati altrove
// in questo progetto), che chiamano CompileToAssembly direttamente e quindi NON eseguono mai
// ILOptim. Vedi AiWiki "(open) - IL-NOT-Bool-ILOptim-Miscompila": un bug di const-folding in
// common/ILOptim/IR/Utils/ConstFolding.cs (FoldUnary/Not) non era raggiungibile dai test esistenti
// per questo motivo. Uno solo di questi test basta a bastonare la regressione se ILOptim la
// reintroduce.
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

public class ILOptimRegressionTests
{
    private static readonly string Root = FindRepoRoot();

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    /// <summary>Compila con la pipeline REALE (ILOptim ON) ed esegue come sottoprocesso.</summary>
    private static string RunOptimized(string basSource, string testName)
    {
        var srcDir = Path.Combine(Path.GetTempPath(), "gm_iloptim_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(srcDir);
        var basPath = Path.Combine(srcDir, testName + ".bas");
        File.WriteAllText(basPath, basSource);

        var program = Basic.Orchestrator.Compile(basPath, null, false);
        Assert.NotNull(program);
        var exePath = IlProgramCompiler.Compile(program!, basPath, srcDir, testName, linkRuntime: true);

        using var proc = new System.Diagnostics.Process
        {
            StartInfo = new System.Diagnostics.ProcessStartInfo(exePath)
            {
                WorkingDirectory = srcDir,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
            }
        };
        proc.Start();
        var stdout = proc.StandardOutput.ReadToEnd();
        var stderr = proc.StandardError.ReadToEnd();
        Assert.True(proc.WaitForExit(20000), $"[ILOptim] {testName} TIMEOUT\n{stdout}\nSTDERR: {stderr}");
        Assert.True(proc.ExitCode == 0, $"[ILOptim] {testName} exit={proc.ExitCode}\n{stdout}\nSTDERR: {stderr}");
        return stdout;
    }

    [Fact]
    public void Not_Of_False_Nullable_IsNil_Gives_Minus1_With_ILOptim_On()
    {
        // Bug: NOT (n IS NIL) con n valorizzato (IS NIL falso) dava 255 invece di -1 quando
        // ILOptim e' attivo (const-folding di Not su un operando dal tipo troppo stretto).
        var output = RunOptimized(
            "SUB Main()\n  DIM n AS INTEGER?\n  n = 42\n  PRINT \"not2=\"; NOT (n IS NIL)\nEND SUB\n",
            "NotIsNilFalse");
        Assert.Contains("not2=-1", output);
        Assert.DoesNotContain("255", output);
    }

    [Fact]
    public void Not_Of_True_Nullable_IsNil_Gives_0_With_ILOptim_On()
    {
        // Caso complementare (gia' corretto anche col bug, vedi doc): NOT(vero) = 0.
        var output = RunOptimized(
            "SUB Main()\n  DIM n AS INTEGER?\n  PRINT \"not1=\"; NOT (n IS NIL)\nEND SUB\n",
            "NotIsNilTrue");
        Assert.Contains("not1= 0", output);
    }
}
