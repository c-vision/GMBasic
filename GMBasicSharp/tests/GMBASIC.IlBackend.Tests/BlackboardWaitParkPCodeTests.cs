using System;
using System.IO;
using Basic.IlBackend.PCode;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public sealed class BlackboardWaitParkPCodeTests
{
    // Regressione 2026-09-09 (commit 5954bf07e, bug AgentBatch-AiAgentSwarm-Conflitto-GPM):
    // AI.BLACKBOARDWAIT chiamato dal MAIN mentre una goroutine (GO SUB) deve scrivere la
    // chiave. Pre-fix la Wait sincrona (tcs.Task.Wait) girava sul worker GPM: a GOMAXPROCS=1
    // il Writer non veniva MAI schedulato -> timeout (deadlock). Post-fix il dispatch usa
    // ParkBlocking: il task MAIN parcheggia, il worker esegue il Writer, la write sveglia il
    // MAIN. Su PCodeVM il park e' SEMPRE legale (il main e' comunque un task dello scheduler).
    //
    // NB: caso deliberatamente PCodeVM-only (il criterio di parita' 2026-09-09 e' il pattern
    // corretto GO SUB+WAITGROUP+WRITE/READ; BLACKBOARDWAIT-dal-MAIN ha semantica per-backend:
    // PCodeVM = park, IL v2 = fail-fast guardia, mondo C = feature assente). Test inline in
    // temp, MAI un sample condiviso (i runner multi-backend lo eseguirebbero su backend dove
    // la semantica/feature differisce).
    [Fact]
    public void BlackboardWait_InMain_ParksAndWakesWhenGoroutineWrites_OnOneWorker()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"BBWaitPark_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var sourcePath = Path.Combine(directory, "Main.bas");
        File.WriteAllText(sourcePath, """
            IMPORT AI WITH ALIAS

            SUB Writer(wg AS WAITGROUP)
                SLEEP 300
                DIM r AS DOUBLE
                r = AI.BLACKBOARDWRITE("park_k", "done")
            END SUB

            SUB Main
                DIM wg AS WAITGROUP
                GO SUB Writer(wg) IN wg
                DIM v AS STRING
                v = AI.BLACKBOARDWAIT("park_k", 20000)
                PRINT "V="; v
                WAIT wg
                IF v = "done" THEN
                    PRINT "CANARY-OK"
                ELSE
                    PRINT "CANARY-FAIL:["; v; "]"
                END IF
            END SUB
            """);

        var previousDirectory = Directory.GetCurrentDirectory();
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            Directory.SetCurrentDirectory(directory);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            var chunk = new global::Compiler.Compiler().Compile(program);
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));

            var result = PCodeVmRunner.Run(chunk, cts.Token);

            Assert.True(result.Success, result.Output);
            Assert.Contains("V=done", result.Output);
            Assert.Contains("CANARY-OK", result.Output);
            Assert.DoesNotContain("CANARY-FAIL", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
            Directory.SetCurrentDirectory(previousDirectory);
            Directory.Delete(directory, recursive: true);
        }
    }
}
