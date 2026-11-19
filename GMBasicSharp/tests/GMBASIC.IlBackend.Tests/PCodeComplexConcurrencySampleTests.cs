using Basic.IlBackend.PCode;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public sealed class PCodeComplexConcurrencySampleTests
{
    [Fact]
    public void GosubReturn_UsesSameFrameAndCrossesSuspendableFinally()
    {
        var result = RunSample("PCodeGpmGosubFinally.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("GOSUB-SAME-FRAME-FINALLY-OK", result.Output);
        Assert.DoesNotContain("FAIL:GOSUB-VALUE", result.Output);
        Assert.Contains("PCODE-GPM-GOSUB-FINALLY-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void SuspendableFinallySample_PreservesReturnAndGotoAcrossSleep()
    {
        var result = RunSample("PCodeGpmSuspendableFinally.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("RETURN-FINALLY-BEGIN", result.Output);
        Assert.Contains("RETURN-FINALLY-END", result.Output);
        Assert.Contains("RETURN-VALUE-OK", result.Output);
        Assert.Contains("GOTO-FINALLY-BEGIN", result.Output);
        Assert.Contains("GOTO-FINALLY-END", result.Output);
        Assert.Contains("GOTO-AFTER-FINALLY", result.Output);
        Assert.Contains("THROW-INNER-FINALLY-BEGIN", result.Output);
        Assert.Contains("THROW-INNER-FINALLY-END", result.Output);
        Assert.Contains("THROW-CAUGHT-AFTER-FINALLY", result.Output);
        Assert.Contains("THROW-OUTER-FINALLY-END", result.Output);
        Assert.Contains("PCODE-GPM-SUSPENDABLE-FINALLY-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void GoExit_UnwindsAllFramesAndCompletesImplicitWaitGroup()
    {
        var result = RunSample("PCodeGpmGoExitFinally.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("GOEXIT-INNER-FINALLY-BEGIN", result.Output);
        Assert.Contains("GOEXIT-INNER-FINALLY-END", result.Output);
        Assert.Contains("GOEXIT-OUTER-FINALLY-BEGIN", result.Output);
        Assert.Contains("GOEXIT-OUTER-FINALLY-END", result.Output);
        Assert.Contains("PCODE-GPM-GOEXIT-FINALLY-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void WaitGroup_UsesImplicitTaskCompletionAndWakesAfterAllWorkers()
    {
        var result = RunSample("GoRoutinesWorkers.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("La somma totale calcolata dal manager è: 240", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
        Assert.True(
            result.Output.IndexOf("Manager ha raccolto tutti i risultati.", StringComparison.Ordinal) <
            result.Output.IndexOf("Tutti i task hanno terminato.", StringComparison.Ordinal),
            result.Output);
        Assert.True(
            result.Output.LastIndexOf("ha ricevuto il segnale di stop.", StringComparison.Ordinal) <
            result.Output.IndexOf("Tutti i task hanno terminato.", StringComparison.Ordinal),
            result.Output);
    }

    [Fact]
    public void ExtremeUnwindSample_CompletesAllConcurrentPaths()
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(
            root,
            "shared",
            "samples",
            "Community",
            "Concurrency",
            "PCodeGpmExtremeUnwind.bas");
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            var program = Basic.Orchestrator.Compile(sample, null, false);
            var chunk = new global::Compiler.Compiler().Compile(program);
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");

            var result = PCodeVmRunner.Run(chunk);

            Assert.True(result.Success, result.Output);
            Assert.Contains("EXCEPTION-CAUGHT", result.Output);
            Assert.Contains("GOTO-UNWIND-OK", result.Output);
            Assert.Contains("CPU-RESUME-OK", result.Output);
            Assert.Contains("PCODE-GPM-EXTREME-DONE", result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    [Fact]
    public void OnErrorContinuationSample_PreservesGosubAndResumeNextState()
    {
        var result = RunSample("PCodeGpmOnErrorContinuations.bas");

        Assert.True(result.Success, result.Output);
        Assert.Equal(2, CountOccurrences(result.Output, "ON-ERROR-GOSUB-HANDLER"));
        Assert.Contains("ON-ERROR-GOSUB-AFTER-FIRST", result.Output);
        Assert.Contains("ON-ERROR-GOSUB-AFTER-SECOND", result.Output);
        Assert.Contains("ON-ERROR-GOSUB-STICKY-OK", result.Output);
        Assert.Contains("ON-ERROR-RESUME-NEXT-OK", result.Output);
        Assert.Contains("PCODE-GPM-ON-ERROR-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void OnErrorFramePropagationSample_UnwindsToEachGoroutineCaller()
    {
        var result = RunSample("PCodeGpmOnErrorFramePropagation.bas");

        Assert.True(result.Success, result.Output);
        Assert.Equal(2, CountOccurrences(result.Output, "CALLER-HANDLER"));
        Assert.Contains("CALLER-RESUMED:A", result.Output);
        Assert.Contains("CALLER-RESUMED:B", result.Output);
        Assert.Contains("PCODE-GPM-ON-ERROR-FRAME-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void FutureFailureSample_UnwindsAwaitingProcedureToCallerHandler()
    {
        var result = RunSample("PCodeGpmFutureErrorFramePropagation.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("FUTURE-CALLER-HANDLER", result.Output);
        Assert.Contains("FUTURE-CALLER-RESUMED", result.Output);
        Assert.Contains("PCODE-GPM-FUTURE-FRAME-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void ClassFrameSample_UnwindsConstructorMethodAndSetterToCaller()
    {
        var result = RunSample("PCodeGpmClassFrameErrors.bas");

        Assert.True(result.Success, result.Output);
        Assert.Equal(3, CountOccurrences(result.Output, "CLASS-FRAME-HANDLER"));
        Assert.Contains("CLASS-CONSTRUCTOR-RESUMED", result.Output);
        Assert.Contains("CLASS-METHOD-RESUMED", result.Output);
        Assert.Contains("CLASS-SETTER-RESUMED", result.Output);
        Assert.Contains("PCODE-GPM-CLASS-FRAME-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void StackOverflowSample_IsCaughtInsideConcurrentWorkers()
    {
        var previousMaximum = Environment.GetEnvironmentVariable("GMBASIC_PCODE_MAX_STACK");
        try
        {
            Environment.SetEnvironmentVariable("GMBASIC_PCODE_MAX_STACK", "8");
            var result = RunSample("PCodeGpmStackOverflowRecovery.bas");

            Assert.True(result.Success, result.Output);
            Assert.Contains("STACK-OVERFLOW-RECOVERED:A", result.Output);
            Assert.Contains("STACK-OVERFLOW-RECOVERED:B", result.Output);
            Assert.Contains("PCODE-GPM-STACK-OVERFLOW-DONE", result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_PCODE_MAX_STACK", previousMaximum);
        }
    }

    [Fact]
    public void SelectStressSample_CompletesEveryCaseWithoutLosingWakeups()
    {
        var result = RunSample("PCodeGpmSelectLoserCleanup.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-SELECT-CLEANUP-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void TimerHeapSample_WakesAllConcurrentDeadlines()
    {
        var result = RunSample("PCodeGpmTimerHeapStress.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-TIMER-HEAP-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void SleepZeroSample_YieldsSingleWorkerBeforeContinuing()
    {
        var result = RunSample("PCodeGpmSleepZeroYield.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-SLEEP-ZERO-YIELD-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void Cancellation_DetachesSleepingTaskAndDrainsSuspendableFinally()
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(root, "shared", "samples", "Concurrency", "PCodeGpmCancellationFinally.bas");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
            using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(100));

            var result = PCodeVmRunner.Run(chunk, cts.Token);

            Assert.Contains("P-Code task cancelled", result.Output);
            Assert.Contains("CANCEL-BODY-BEGIN", result.Output);
            Assert.Contains("CANCEL-FINALLY-BEGIN", result.Output);
            Assert.Contains("CANCEL-FINALLY-END", result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    [Fact]
    public void FinallyError_SupersedesOriginalUnwindAndReachesOuterCatch()
    {
        var result = RunSample("PCodeGpmFinallyErrorPrecedence.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("ERROR-FINALLY-BEGIN", result.Output);
        Assert.Contains("OUTER-CAUGHT:cleanup-error", result.Output.Replace(" ", ""));
        Assert.Contains("PCODE-GPM-FINALLY-ERROR-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void WaitKeyPark_DoesNotBlockWorkerAndAbortsOnCancel()
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(root, "shared", "samples", "Concurrency", "PCodeGpmWaitKeyPark.bas");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
            using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(200));

            var result = PCodeVmRunner.Run(chunk, cts.Token);

            Assert.True(result.Success, result.Output);
            Assert.Contains("INPUT-TICK", result.Output);
            Assert.Contains("INPUT-DONE", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    [Fact]
    public void MutexAndDefer_CounterIsDeterministicOnOneWorker()
    {
        // Parità Go sync.Mutex + defer: 10 goroutine incrementano un contatore condiviso
        // protetto da MUTEX LOCK/UNLOCK, con UNLOCK delegato a DEFER (parità Go defer
        // mu.Unlock()). Senza lock il totale sarebbe non deterministico; con lock = 10.
        var result = RunSample("PCodeGpmMutexDefer.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-MUTEX-DEFER-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void RwMutex_ReadersShared_WriterExclusive_OnOneWorker()
    {
        // Parità Go sync.RWMutex: lettori condivisi (RLOCK/RUNLOCK) + scrittore esclusivo
        // (LOCK/UNLOCK). Il writer incrementa il contatore condiviso; i lettori lo leggono
        // sotto RLOCK. Totale deterministico: Shared = 1.
        var result = RunSample("PCodeGpmRwMutex.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-RWMUTEX-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void Once_ProcedureRunsExactlyOnceAcrossGoroutines()
    {
        // Parità Go sync.Once: 5 goroutine chiamano ONCE RUN o, DoInit in concorrenza;
        // DoInit (che incrementa Counter) deve essere eseguita UNA sola volta → Counter=1.
        var result = RunSample("PCodeGpmOnce.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-ONCE-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void ChannelDirection_ProducerSendOnly_ConsumerReceiveOnly()
    {
        // Parità Go chan direzionali: producer invia su un canale SendOnly (chan<- T),
        // consumer riceve da un canale ReceiveOnly (<-chan T). Il runtime è invariato; la
        // direzione è un vincolo di tipo (SEND su ReceiveOnly / RECEIVE su SendOnly =
        // errore di compilazione, verificato in VisitSendStatement/VisitReceiveStatement).
        var result = RunSample("PCodeGpmChannelDirection.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-CHAN-DIR-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void Atomic_CounterDeterministicWithoutMutex_AndCompareSwap()
    {
        // Parità Go sync/atomic: 4 goroutine incrementano un box ATOMIC con
        // ATOMIC.INCREMENT (AddInt64) — totale deterministico 400 anche senza mutex.
        // Verifica anche FETCHVALUE (Load), STORE e COMPARESWAP (CAS).
        var result = RunSample("PCodeGpmAtomic.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-ATOMIC-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void Context_CancellationPropagates_WorkerStopsOnDone()
    {
        // Parità Go context.Context: CONTEXT.BACKGROUND → WITHCANCEL (figli registrati) →
        // CANCEL propaga ai figli; CONTEXT.DONE ritorna un canale chiuso alla cancellazione
        // (il worker termina su RECEIVE ok=0) e CONTEXT.CANCELLED=1 dopo la cancellazione.
        var result = RunSample("PCodeGpmContext.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-CONTEXT-OK", result.Output);
        Assert.Contains("PCODE-GPM-CONTEXT-PROPAGATION-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void LifecycleChurn_500GoroutinesReclaimOnOneWorker()
    {
        var result = RunSample("PCodeGpmLifecycleChurn.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-LIFECYCLE-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Theory]
    [InlineData("PCodeGpmStarvationStress.bas", "PCODE-GPM-STARVATION-OK", 2)]
    [InlineData("PCodeGpmStarvationStress.bas", "PCODE-GPM-STARVATION-OK", 3)]
    [InlineData("PCodeGpmStarvationStress.bas", "PCODE-GPM-STARVATION-OK", 8)]
    [InlineData("PCodeGpmStarvationStress.bas", "PCODE-GPM-STARVATION-OK", 16)]
    [InlineData("PCodeGpmLifecycleChurn.bas", "PCODE-GPM-LIFECYCLE-OK", 3)]
    [InlineData("PCodeGpmLifecycleChurn.bas", "PCODE-GPM-LIFECYCLE-OK", 8)]
    [InlineData("PCodeGpmStressMix.bas", "PCODE-GPM-STRESS-MIX-OK", 8)]
    [InlineData("PCodeGpmStressMix.bas", "PCODE-GPM-STRESS-MIX-OK", 16)]
    [InlineData("PCodeGpmStressMix.bas", "PCODE-GPM-STRESS-MIX-OK", 32)]
    [InlineData("PCodeGpmNetworkOwnership.bas", "PCODE-GPM-NET-OWN-OK", 4)]
    public void MultiProcessor_AllWorkersCompleteWithExactTotal(string fileName, string expectedMarker, int goMaxProcs)
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(root, "shared", "samples", "Concurrency", fileName);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            // Multi-P (2..16): i worker eseguono task CONTEMPORANEAMENTE (code locali,
            // runnext, work stealing). Il totale deterministico deve restare esatto: una race
            // dello scheduler (doppia esecuzione, wake perso, runnext perso) lo romperebbe o
            // appenderebbe — il CTS a 30s fa fallire pulito ogni hang.
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", goMaxProcs.ToString());
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            var result = PCodeVmRunner.Run(chunk, cts.Token);

            Assert.True(result.Success, result.Output);
            Assert.Contains(expectedMarker, result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    [Fact]
    public void Cancellation_MultiProcessor_DrainsAllWorkersCleanly()
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(root, "shared", "samples", "Concurrency", "PCodeGpmStressMix.bas");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            // Cancel a metà dello stress a P=8: la cancellazione deve propagarsi a TUTTI i
            // worker concorrenti (staccandoli dalle attese e drenando i FINALLY) senza appendere
            // lo scheduler — il CTS rende il test bounded (§15).
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "8");
            using var cts = new CancellationTokenSource(TimeSpan.FromMilliseconds(500));

            var result = PCodeVmRunner.Run(chunk, cts.Token);

            Assert.True(result.Success, result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    [Theory]
    [InlineData("NetTcpAdvanced.bas", "ECHO: Ciao Mondo!", "TCP-DISCONNECT-RESUME-OK")]
    [InlineData("NetUdpAdvanced.bas", "UDP echo: Messaggio UDP!", "UDP-TIMEOUT-RESUME-OK")]
    public void NetworkGoroutineSamples_CompleteWithoutBlockingWorkers(
        string fileName, string expectedEcho, string expectedTerminalWake)
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(root, "shared", "samples", "Network", fileName);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            // Un solo logical processor rende il test sensibile a qualunque I/O eseguito
            // sul worker GPM: server e client devono progredire tramite park/resume.
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
            var result = PCodeVmRunner.Run(chunk);

            Assert.True(result.Success, result.Output);
            Assert.Contains(expectedEcho, result.Output);
            Assert.Contains(expectedTerminalWake, result.Output);
            Assert.Contains("--- TEST COMPLETATO ---", result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    [Fact]
    public void WebSocketSample_CompletesAndOwnsReceiveLoopsOnOneWorker()
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(root, "shared", "samples", "Network", "WebSocketEcho.bas");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
            var result = PCodeVmRunner.Run(chunk);

            Assert.True(result.Success, result.Output);
            Assert.Contains("[client A] 3 messaggi inviati, connessione chiusa.", result.Output);
            Assert.Contains("[client B] 3 messaggi inviati, connessione chiusa.", result.Output);
            Assert.Contains("[client C] 3 messaggi inviati, connessione chiusa.", result.Output);
            Assert.Contains("[server] ricevuto:", result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    [Fact]
    public void LocalTlsRoundTrip_CompletesOnOneWorker()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"PCodeTls_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var sourcePath = Path.Combine(directory, "Main.bas");
        var certificatePath = Path.Combine(directory, "server.pfx");
        var listener = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        listener.Start();
        var port = ((System.Net.IPEndPoint)listener.LocalEndpoint).Port;
        listener.Stop();

        using var rsa = System.Security.Cryptography.RSA.Create(2048);
        var request = new System.Security.Cryptography.X509Certificates.CertificateRequest(
            "CN=localhost", rsa, System.Security.Cryptography.HashAlgorithmName.SHA256,
            System.Security.Cryptography.RSASignaturePadding.Pkcs1);
        var san = new System.Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder();
        san.AddDnsName("localhost");
        san.AddIpAddress(System.Net.IPAddress.Loopback);
        request.CertificateExtensions.Add(san.Build());
        request.CertificateExtensions.Add(new System.Security.Cryptography.X509Certificates.X509KeyUsageExtension(
            System.Security.Cryptography.X509Certificates.X509KeyUsageFlags.DigitalSignature |
            System.Security.Cryptography.X509Certificates.X509KeyUsageFlags.KeyEncipherment, false));
        var enhancedKeyUsages = new System.Security.Cryptography.OidCollection
        {
            new("1.3.6.1.5.5.7.3.1")
        };
        request.CertificateExtensions.Add(
            new System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension(enhancedKeyUsages, false));
        using var certificate = request.CreateSelfSigned(
            DateTimeOffset.UtcNow.AddMinutes(-1), DateTimeOffset.UtcNow.AddMinutes(10));
        File.WriteAllBytes(certificatePath, certificate.Export(
            System.Security.Cryptography.X509Certificates.X509ContentType.Pfx, "test"));

        File.WriteAllText(sourcePath, $$"""
            IMPORT NET
            IMPORT CONCURRENCY

            SUB TlsServer()
              DIM listenerHandle AS LONG
              SSL OPEN {{port}}, listenerHandle, "{{certificatePath}}", "test"
              IF listenerHandle <= 0 THEN
                PRINT "FAIL:TLS-LISTEN"
                RETURN
              END IF
              DIM clientHandle AS LONG
              TCP ACCEPT listenerHandle, clientHandle, 3000
              IF clientHandle <= 0 THEN
                PRINT "FAIL:TLS-ACCEPT"
                TCP CLOSE listenerHandle
                RETURN
              END IF
              DIM data$ AS STRING
              DIM bytesRead AS LONG
              TCP RECV clientHandle, data$, bytesRead, 3000
              IF data$ = "PING" THEN PRINT "TLS-SERVER-RECV-OK" ELSE PRINT "FAIL:TLS-SERVER-RECV"
              TCP SEND clientHandle, "PONG"
              TCP CLOSE clientHandle
              TCP CLOSE listenerHandle
            END SUB

            SUB TlsClient()
              SLEEP 100
              DIM clientHandle AS LONG
              SSL CONNECT "127.0.0.1", clientHandle, {{port}}, 3000
              IF clientHandle <= 0 THEN
                PRINT "FAIL:TLS-CONNECT"
                RETURN
              END IF
              TCP SEND clientHandle, "PING"
              DIM data$ AS STRING
              DIM bytesRead AS LONG
              TCP RECV clientHandle, data$, bytesRead, 3000
              IF data$ = "PONG" THEN PRINT "TLS-CLIENT-RECV-OK" ELSE PRINT "FAIL:TLS-CLIENT-RECV"
              TCP CLOSE clientHandle
            END SUB

            SUB Main()
              DIM workers AS WAITGROUP
              GO SUB TlsServer() IN workers
              GO SUB TlsClient() IN workers
              WAIT workers
              PRINT "PCODE-GPM-TLS-DONE"
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
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var result = PCodeVmRunner.Run(chunk, cancellation.Token);

            Assert.True(result.Success, result.Output);
            Assert.Contains("TLS-SERVER-RECV-OK", result.Output);
            Assert.Contains("TLS-CLIENT-RECV-OK", result.Output);
            Assert.Contains("PCODE-GPM-TLS-DONE", result.Output);
            Assert.DoesNotContain("FAIL:", result.Output);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
            Directory.SetCurrentDirectory(previousDirectory);
            Directory.Delete(directory, recursive: true);
        }
    }

    [Fact]
    public void CpuBuiltinSafepoint_AllowsAnotherGoroutineToProgressOnOneWorker()
    {
        var result = RunSample("PCodeGpmCpuBuiltinSafepoint.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("CPU-RESULT-OK", result.Output);
        Assert.Contains("CPU-IFFT-OK", result.Output);
        Assert.Contains("CPU-WINDOW-OK", result.Output);
        Assert.Contains("PCODE-GPM-CPU-SAFEPOINT-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
        Assert.True(
            result.Output.IndexOf("CPU-TICK", StringComparison.Ordinal) <
            result.Output.IndexOf("CPU-HEAVY-DONE", StringComparison.Ordinal),
            result.Output);
    }

    [Fact]
    public void AnalysisSafepoint_AllowsAnotherGoroutineToProgressOnOneWorker()
    {
        var result = RunSample("PCodeGpmAnalysisSafepoint.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("ANALYSIS-RESULT-OK", result.Output);
        Assert.Contains("PCODE-GPM-ANALYSIS-SAFEPOINT-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
        Assert.True(
            result.Output.IndexOf("ANALYSIS-TICK", StringComparison.Ordinal) <
            result.Output.IndexOf("ANALYSIS-HEAVY-DONE", StringComparison.Ordinal),
            result.Output);
    }

    [Fact]
    public void StarvationStress_AllWorkersCompleteOnOneWorker()
    {
        var result = RunSample("PCodeGpmStarvationStress.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-STARVATION-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void NetworkRecvInsideFinally_CompletesOnOneWorker()
    {
        var result = RunSample("PCodeGpmNetInFinally.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("SERVER-RECV-OK", result.Output);
        Assert.Contains("FINALLY-NET-RECV-OK", result.Output);
        Assert.Contains("PCODE-GPM-NET-FINALLY-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void NetworkOwnership_RepeatedDisconnectAndTimeoutOnOneWorker()
    {
        // Stress OWNERSHIP: 20 cicli TCP ACCEPT/RECV con disconnect remoto e timeout,
        // tutto su un solo worker (GOMAXPROCS=1). Un wake perso/duplicato o una
        // registration che non si ripulisce -> hang (CTS 30s) o conteggio errato.
        var result = RunSample("PCodeGpmNetworkOwnership.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-NET-OWN-OK", result.Output);
        Assert.Contains("PCODE-GPM-NET-OWN-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void GoMaxProcsResize_DynamicReduceKeepsAllTasksOnOneWorker()
    {
        // Parità Go runtime.GOMAXPROCS(n): il sample parte con i P di default, riduce a 2,
        // lancia 50 goroutine, poi riduce a 1 A RUNTIME mentre girano. La migrazione dei
        // task dalle code locali dei P rimossi alla coda globale non deve perdere nulla:
        // il totale deterministico (252500) verifica la correttezza; un task perso -> hang
        // (CTS 30s) o conteggio errato.
        var result = RunSample("PCodeGpmGoMaxProcsResize.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GMP-RESIZE-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void GoMaxProcsResizeStress_MultipleUpDownKeepsAllTasks()
    {
        // Parità Go GOMAXPROCS: 5 round di resize 4→2→1 durante 40 goroutine per round
        // (200 totali). Ogni riduzione migra i task dei P rimossi alla coda globale;
        // il totale deterministico 255000 verifica che nessun task venga perso.
        var result = RunSample("PCodeGpmGoMaxProcsResizeStress.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GMP-RESIZE-STRESS-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void WebSocketInGoroutine_LifecycleCompletesOnOneWorker()
    {
        // Parità GPM P7/P8: client WebSocket in goroutine che si connette, invia,
        // il server riceve (echo) e chiude. Il park WS (send/receive async) non
        // blocca il worker; a GOMAXPROCS=1 l'intero round-trip sta su un worker.
        var result = RunSample("PCodeGpmWebSocketInGoroutine.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("SERVER-RECV-msg- 1", result.Output);
        Assert.Contains("SERVER-RECV-msg- 2", result.Output);
        Assert.Contains("WS-CLIENT- 1 -OK", result.Output);
        Assert.Contains("PCODE-GPM-WS-GOROUTINE-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void ChannelClose_WorkersExitOnClosedFlag()
    {
        // Parità Go close(ch) + v, ok := <-ch: CHANNEL CLOSE dopo l'invio; i worker
        // escono dal loop RECEIVE quando ok=%CHCLOSED. Totale deterministico 110
        // (sum(1..10)*2) verifica che tutti i job siano stati processati.
        var result = RunSample("PCodeGpmChannelClose.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("PCODE-GPM-CHANNEL-CLOSE-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void SelectTimeout_ReceiveAndTimeoutCasesBothComplete()
    {
        // Parità Go case <-time.After(d): SELECT con TIMEOUT — il caso RECEIVE vince se un
        // producer invia entro la deadline, altrimenti scatta il TIMEOUT. Il sample copre
        // entrambi i rami; il marker finale verifica che entrambi siano stati scelti.
        var result = RunSample("PCodeGpmSelectTimeout.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("SELECT-RECV- 42", result.Output);
        Assert.Contains("SELECT-TIMEOUT-OK", result.Output);
        Assert.Contains("PCODE-GPM-SELECT-TIMEOUT-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void ChannelCapacity_ExplicitBufferSize()
    {
        // Parità Go make(chan T, n): DIM ch AS CHANNEL OF INTEGER (2) crea un canale con
        // buffer di 2. Producer invia 5, Consumer riceve 5 in parallelo — il SEND su buffer
        // pieno parcheggia e il RECEIVE libera. Totale deterministico 15 (1+2+3+4+5).
        var result = RunSample("PCodeGpmChannelCapacity.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("CONSUMER-TOTAL= 15", result.Output);
        Assert.Contains("CAPACITY-2-PRODUCER-CONSUMER-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void SelectNilCase_DisabledNeverChosen()
    {
        // Parità Go select su canale nil: il case su un canale non inizializzato è
        // disabilitato per sempre (mai pronto, mai scelto, non blocca). Solo il canale
        // pronto deve vincere.
        var result = RunSample("PCodeGpmSelectNil.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("SELECT-READY- 42", result.Output);
        Assert.Contains("PCODE-GPM-SELECT-NIL-OK", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void HttpFetchInGoroutine_ResumesWithValueOnOneWorker()
    {        var result = RunSample("PCodeGpmHttpInGoroutine.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("FETCH-GOROUTINE-OK", result.Output);
        Assert.Contains("PCODE-GPM-HTTP-GOROUTINE-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    [Fact]
    public void UdpRecvTimeoutInsideFinally_CompletesOnOneWorker()
    {
        var result = RunSample("PCodeGpmUdpInFinally.bas");

        Assert.True(result.Success, result.Output);
        Assert.Contains("UDP-RECV-OK", result.Output);
        Assert.Contains("FINALLY-UDP-TIMEOUT-OK", result.Output);
        Assert.Contains("PCODE-GPM-UDP-FINALLY-DONE", result.Output);
        Assert.DoesNotContain("FAIL:", result.Output);
    }

    private static PCodeVmResult RunSample(string fileName)
    {
        var root = FindRepositoryRoot();
        var sample = Path.Combine(root, "shared", "samples", "Concurrency", fileName);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var previousGoMaxProcs = Environment.GetEnvironmentVariable("GMBASIC_GOMAXPROCS");
        try
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", "1");
            // Timeout diagnostico (piano §15: tutti i test bounded): un task bloccato su un
            // park non consuma istruzioni, quindi il solo maxInstructions non basta a far
            // terminare un run appeso — la cancellazione a 30s lo fa fallire in modo pulito.
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            return PCodeVmRunner.Run(chunk, cts.Token);
        }
        finally
        {
            Environment.SetEnvironmentVariable("GMBASIC_GOMAXPROCS", previousGoMaxProcs);
        }
    }

    private static string FindRepositoryRoot()
    {
        for (var directory = new DirectoryInfo(Directory.GetCurrentDirectory());
             directory != null;
             directory = directory.Parent)
        {
            if (File.Exists(Path.Combine(directory.FullName, "GMBasicSharp", "GMBasicSharp.slnx")))
                return directory.FullName;
        }
        throw new DirectoryNotFoundException("GMBASIC.sln non trovata nella gerarchia corrente.");
    }

    private static int CountOccurrences(string text, string value) =>
        text.Split(value, StringSplitOptions.None).Length - 1;
}
