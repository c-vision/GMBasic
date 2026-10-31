// Surface BASIC P1-2b (multibrain): COMPILE-ONLY — verifica che la nuova sintassi
// (argomenti estesi con %RERANK_OFF/%RERANK_KEYWORD, minEvidence, explain) compili su IL.
// L'esecuzione/semantica è coperta dai test C# in AI.Tests (MultiBrainTests).
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

public sealed class AiRerankSurfaceCompileTests
{
    private static void AssertCompiles(string src)
    {
        var program = Basic.Orchestrator.CompileFromString(src, "rksurf.bas", null, verbose: false);
        Assert.NotNull(program.MainModule);
    }

    [Fact]
    public void SearchMulti_RerankOff_Compiles()
        => AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM r AS STRING
              r = AI.SEARCHMULTI("mean reversion", 3, "wiki", NULL, %RERANK_OFF)
              PRINT r
            END SUB
            """);

    [Fact]
    public void SearchMulti_FullSignature_Compiles()
        => AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM r AS STRING
              r = AI.SEARCHMULTI("mean reversion", 3, "wiki", NULL, %RERANK_KEYWORD, 0.2, TRUE)
              PRINT r
            END SUB
            """);

    [Fact]
    public void RagContext_Ask_RerankEvidence_Compile()
    {
        AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM c AS STRING
              c = AI.RAGCONTEXTMULTI("mean reversion", 3, %RERANK_KEYWORD, 0.2)
              PRINT c
            END SUB
            """);
        AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM a AS STRING
              a = AI.ASKMULTI("mean reversion", 3, %RERANK_OFF)
              PRINT a
            END SUB
            """);
    }

    [Fact]
    public void VectorSearch_FullSignature_Compiles()
        => AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM r AS STRING
              r = AIVECTOR.SEARCH("mybrain", "wiki", "mean reversion", 5, TRUE, NULL, %RERANK_KEYWORD, 0.3, TRUE)
              PRINT r
            END SUB
            """);

    [Fact]
    public void VectorRag_Ask_RerankEvidence_Compile()
    {
        AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM c AS STRING
              c = AIVECTOR.RAGCONTEXT("mybrain", "wiki", "mean reversion", 5, %RERANK_KEYWORD, 0.3)
              PRINT c
            END SUB
            """);
        AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM a AS STRING
              a = AIVECTOR.ASK("mybrain", "wiki", "mean reversion", 5, %RERANK_OFF)
              PRINT a
            END SUB
            """);
    }

    [Fact]
    public void BrowserStart_EngineConstants_Compile()
    {
        AssertCompiles("""
            IMPORT BROWSER
            IMPORT CONCURRENCY
            SUB Main()
              DIM s AS STRING
              s = BROWSER.START(%AUTO, 9222)
              PRINT s
              s = BROWSER.START(%CHROMIUM, 9255, "demo")
              PRINT s
              s = BROWSER.START(%OBSCURA)
              PRINT s
            END SUB
            """);
    }

    [Fact]
    public void AgentSet_Browser_EngineConstant_Compile()
        => AssertCompiles("""
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              AI AGENT CREATE "a1"
              AI AGENT SET "a1" BROWSER %BROWSER_OBSCURA
              PRINT AI.AGENTGET("a1", "browser")
            END SUB
            """);

    [Fact]
    public void SkillsList_Array_Assignment_RunsOnIL()
    {
        // IL: DIM a() AS STRING + a = AI.SKILLSLIST() (string[]) — verifica parità array-stringa
        const string src = """
            IMPORT AI
            IMPORT CONCURRENCY
            SUB Main()
              DIM a() AS STRING
              DIM p() AS STRING
              a = AI.SKILLSLIST()
              p = AI.SKILLSPATHS()
              PRINT "n="; UBOUND(a) + 1
              PRINT "np="; UBOUND(p) + 1
            END SUB
            """;
        var output = Run(src);
        Assert.Matches("(?s)n= \\d+.*np= \\d+", output);
    }

    private static string Run(string src)
    {
        var program = Basic.Orchestrator.CompileFromString(src, "skillrun.bas", null, verbose: false);
        var asmBuilder = Basic.IlBackend.IlProgramCompiler.CompileToAssembly(program, "SkillRun_" + System.Guid.NewGuid().ToString("N"), out _);
        using var ms = new System.IO.MemoryStream();
        asmBuilder.Save(ms);
        var assembly = System.Reflection.Assembly.Load(ms.ToArray());
        var main = assembly.GetType("GeneratedProgram")!.GetMethod("Main", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)!;
        var originalOut = System.Console.Out;
        var sw = new System.IO.StringWriter();
        try
        {
            System.Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(System.Console.Out, new Runtime.RuntimeConfig()));
            var task = System.Threading.Tasks.Task.Run(() => main.Invoke(null, null));
            Assert.True(task.Wait(System.TimeSpan.FromSeconds(30)), "TIMEOUT\n" + sw);
            task.GetAwaiter().GetResult();
            return sw.ToString();
        }
        finally { System.Console.SetOut(originalOut); }
    }
}
