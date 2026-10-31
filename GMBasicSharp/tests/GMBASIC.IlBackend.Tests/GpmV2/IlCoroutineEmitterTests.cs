// G2/G3 — emitter StepV2_* + driver v2: assembly StepV2_<id> generati, run end-to-end dei
// sample GPM v2 (IF/SELECT/FOR/WHILE, GOSUB, TRY/FINALLY/CATCH, GO EXIT, ON ERROR, mutex,
// parità canali) e test driver-level di Gosub/ReturnGosub/Throw/ContinueUnwind.

using System.Collections.Immutable;
using System.Reflection;
using Xunit;
using Basic.IlBackend;
using Basic.IlBackend.Gpm;
using Basic.IlBackend.Gpm.Ir;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class IlCoroutineEmitterTests
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

    private static (BasicProgram program, ImmutableArray<FunctionDefinition> procedures,
        ImmutableHashSet<int> mayPark, Func<int, FunctionDefinition?> byId) CompileSample(string sampleRel)
    {
        var sample = Path.Combine(Root, sampleRel);
        Assert.True(File.Exists(sample), $"Sample mancante: {sample}");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var procedures = program.LoadedModules.Values
            .SelectMany(m => m.Declarations)
            .OfType<FunctionDefinition>()
            .ToImmutableArray();
        var byId = (int id) => procedures.FirstOrDefault(p => p.SymbolId == id);
        var mayPark = MayParkAnalysis.ComputeMayPark(procedures, byId);
        return (program, procedures, mayPark, byId);
    }

    private static Assembly CompileAssembly(BasicProgram program)
    {
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "GpmV2Test_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        return Assembly.Load(ms.ToArray());
    }

    private static string RunV2Sample(string sampleRel)
    {
        var (program, procedures, _, _) = CompileSample(sampleRel);
        var mainDef = procedures.First(p => p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase));
        var assembly = CompileAssembly(program);
        var dispatcher = IlGpmV2.CreateV2Dispatcher(assembly);

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig(),
                    graphics: new Runtime.NullGraphicsService()));
            var scheduler = new PCodeScheduler();
            var task = scheduler.CreateTask(0);
            task.Scheduler = scheduler;
            task.Frame = new PCodeCallFrame { OwnerProcedure = mainDef.SymbolId, LocalVariables = new object?[64] };
            scheduler.Schedule(task);
            try
            {
                IlGpmV2Driver.RunV2Inline(scheduler, dispatcher, timeout: TimeSpan.FromSeconds(60));
            }
            catch (TimeoutException)
            {
                // il run si è bloccato: ritorna l'output parziale per il debug dell'assert
            }
            if (task.PendingException is { } v2Ex)
                throw new InvalidOperationException("V2 task fallito con eccezione.", v2Ex);
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString().Replace("\r\n", "\n");
    }

    /// <summary>Esegue un sample: driver v2 se Main è may-park (ha uno stepper), altrimenti
    /// il Main IL standard (path lineare). Usato dai test G8-bis (POOL/SYNCMAP/ATOMIC/
    /// NUMGOROUTINE sono flat → nessuno StepV2).</summary>
    private static string RunSample(string sampleRel)
    {
        var (program, procedures, mayPark, _) = CompileSample(sampleRel);
        var mainDef = procedures.First(p => p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase));
        var assembly = CompileAssembly(program);
        var programType = assembly.GetType("GeneratedProgram")
            ?? throw new InvalidOperationException("GeneratedProgram non trovato.");
        var main = programType.GetMethod("Main", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig(),
                    graphics: new Runtime.NullGraphicsService()));
            if (mayPark.Contains(mainDef.SymbolId))
            {
                var dispatcher = IlGpmV2.CreateV2Dispatcher(assembly);
                var scheduler = new PCodeScheduler();
                var task = scheduler.CreateTask(0);
                task.Scheduler = scheduler;
                task.Frame = new PCodeCallFrame { OwnerProcedure = mainDef.SymbolId, LocalVariables = new object?[64] };
                scheduler.Schedule(task);
                try { IlGpmV2Driver.RunV2Inline(scheduler, dispatcher, timeout: TimeSpan.FromSeconds(60)); }
                catch (TimeoutException) { }
                if (task.PendingException is { } v2Ex)
                    throw new InvalidOperationException("V2 task fallito con eccezione.", v2Ex);
            }
            else
            {
                main.Invoke(null, null);
            }
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString().Replace("\r\n", "\n");
    }

    [Fact]
    public void GeneratedAssembly_Contains_StepV2_For_MayPark_Procedures()
    {
        var (program, procedures, mayPark, _) = CompileSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ControlFlow.bas"));
        Assert.True(mayPark.Count >= 3, $"Attese ≥3 procedure may-park, trovate {mayPark.Count}");

        var assembly = CompileAssembly(program);
        var programType = assembly.GetType("GeneratedProgram")
            ?? throw new InvalidOperationException("GeneratedProgram non trovato.");

        var v2Methods = programType
            .GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Where(m => m.Name.StartsWith("StepV2_", StringComparison.Ordinal))
            .Select(m => m.Name)
            .ToList();

        Assert.True(v2Methods.Count > 0, "Nessuno StepV2 generato.");
        foreach (var proc in procedures.Where(p => mayPark.Contains(p.SymbolId)))
            Assert.Contains("StepV2_" + proc.SymbolId, v2Methods);
        Assert.Contains("StepV2_Main", v2Methods);
    }

    [Fact]
    public void V2Driver_Runs_ControlFlow_Sample_With_ExpectedOutput()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ControlFlow.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "UNO", "DUE", "ALTRO  3 ", "ALTRO  4 ", "FINE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_TryFinally_WithParkInFinally()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2TryFinally.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "TRY  1 ", "FINALLY  11 ", "DOPO  11 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_TryCatch_Exception()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2TryCatch.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "CATCH 5", "DOPO-CATCH", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_ChannelClose_WithOkFlag_AndCapacity()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ChannelClose.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "OK  1   1 ", "OK  2   1 ", "OK -1   1 ", "FINE  0 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_GoExit_CrossesFinally()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2GoExit.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "TRY", "FINALLY" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_OnError_Goto_ResumeNext()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2OnError.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "HANDLER", "DOPO-ERRORE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_Mutex_LockUnlock_Serializes()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2Mutex.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries).ToHashSet();
        Assert.Contains("CRIT  1   1 ", lines);
        Assert.Contains("CRIT  1   2 ", lines);
        Assert.Contains("CRIT  2   1 ", lines);
        Assert.Contains("MAIN-DONE", lines);
        Assert.Equal(4, lines.Count);
    }

    [Fact]
    public void V2Driver_Runs_Function_ReturnValue_ToLocal()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2Function.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "X- 42 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_ByRef_ModifiesCaller()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ByRef.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "N- 12 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_Recursion_Factorial()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2Recursion.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "FACT- 24 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_GoFunction_Future()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2GoFunction.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "X- 42 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_LocalProcedure_NoCapture()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2Closure.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "IN 5 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void ClosureCapture_LinkerCreatesClosureContext()
    {
        // La closure con cattura è riscritta dal linker: crea un local `__context` del tipo
        // sintetico __ClosureCtx_* e riscrive la variabile catturata in `__context.fattore`.
        // Per la v2 serve la macchina __ClosureCtx (tipo classe + field access + passaggio del
        // contesto alla locale) — follow-up G4. Qui si verifica il rewrite del linker.
        var (program, procedures, _, _) = CompileSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ClosureCapture.bas"));
        var main = procedures.First(p => p.OriginalName.Lexeme == "Main");
        Assert.Contains(main.Body, s =>
            s is DeclarationStmt d && d.Declarations.Any(decl => decl.NameCleaned.Lexeme == "__context"));
    }

    [Fact]
    public void V2Driver_Runs_Closure_WithCapture_ByReference()
    {
        // G4: FUNCTION locale con cattura by-reference sotto la v2. La closure (Moltiplica)
        // parcheggia (SLEEP) → Main è may-park → entrambe girano su StepV2. La cattura vive
        // nel __ClosureCtx heap-promotato dal Linker: la modifica a `fattore` nel chiamante
        // si riflette nella closure (parità Go).
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ClosureCapture.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "M- 50 ", "M- 100 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_WaitKey_ResumesOnInjectedKey()
    {
        // G5: WAITKEY sotto la v2 — park cooperativo (TryWaitKeyPark → IlRuntime.InkeyWaiters);
        // il tasto iniettato da IlRuntime.QueueInkey sveglia il task che rientra e continua.
        var output = "";
        var runTask = Task.Run(() =>
        {
            Task.Delay(600).ContinueWith(_ => Basic.IlBackend.IlRuntime.QueueInkey("X"));
            output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2WaitKey.bas"));
            return output;
        });
        output = runTask.GetAwaiter().GetResult();
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Contains("KEY-PRESSED", lines);
        Assert.Contains("MAIN-DONE", lines);
    }

    [Fact]
    public void V2Driver_Runs_TcpConnect_ResolvesNetworkAwait()
    {
        // G5 §11.4: TCP CONNECT come await (IlNetworkAwait su BeginWait/PCodeWaitRegistration) —
        // il task parcheggia finché la connessione non è stabilita su un TcpListener locale.
        var listener = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 8140);
        listener.Start();
        var stop = new CancellationTokenSource();
        var acceptLoop = Task.Run(() =>
        {
            while (!stop.IsCancellationRequested)
            {
                try { using var client = listener.AcceptTcpClient(); }
                catch { break; }
            }
        });
        try
        {
            var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2TcpConnect.bas"));
            var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
            var cLine = lines.FirstOrDefault(l => l.StartsWith("C-", StringComparison.Ordinal));
            Assert.NotNull(cLine);
            var handle = double.Parse(cLine!.Trim()["C-".Length..]);
            Assert.True(handle > 0, $"handle atteso > 0, ottenuto {handle}");
            Assert.Contains("MAIN-DONE", lines);
        }
        finally
        {
            stop.Cancel();
            listener.Stop();
        }
    }

    [Fact]
    public void V2Driver_Runs_TcpEcho_SendRecvUnderGpm()
    {
        // G5: TCP SEND/RECV come park (TcpSendStep/TcpRecvStep su IlNetworkAwait) — echo server
        // locale: invia "hello", riceve l'eco.
        var listener = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 8140);
        listener.Start();
        var stop = new CancellationTokenSource();
        var echoLoop = Task.Run(() =>
        {
            while (!stop.IsCancellationRequested)
            {
                try
                {
                    using var client = listener.AcceptTcpClient();
                    using var stream = client.GetStream();
                    var buf = new byte[4096];
                    var n = stream.Read(buf, 0, buf.Length);
                    stream.Write(buf, 0, n); // echo
                }
                catch { break; }
            }
        });
        try
        {
            var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2TcpEcho.bas"));
            var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
            Assert.Contains("N- 5 ", lines);
            Assert.Contains("MAIN-DONE", lines);
        }
        finally
        {
            stop.Cancel();
            listener.Stop();
        }
    }

    [Fact]
    public void V2Driver_Runs_UdpRecv_ResolvesOnDatagram()
    {
        // G5: UDP RECV come park (UdpRecvStep su IlNetworkAwait) — un datagramma inviato al
        // socket (UDP OPEN 48156) sveglia il task che lo consuma.
        // Retry dell'INTERO run: se UDP OPEN fallisce (porta momentaneamente occupata sotto
        // contention del batch), il recv torna vuoto ("U-") — si riprova su una porta libera.
        string? output = null;
        for (var round = 0; round < 3; round++)
        {
            var sender = new System.Net.Sockets.UdpClient();
            // Retry periodico CONCORRENTE al run: la porta potrebbe non essere ancora bindata
            // (il sample sta compilando/partendo) — un send unico a 600ms andava perso (UDP:
            // datagramma scartato se nessuno ascolta ancora) sotto contention del batch.
            using var cts = new CancellationTokenSource();
            var senderTask = Task.Run(() =>
            {
                for (var attempt = 0; attempt < 200 && !cts.IsCancellationRequested; attempt++)
                {
                    try
                    {
                        var bytes = System.Text.Encoding.UTF8.GetBytes("hello");
                        sender.Send(bytes, bytes.Length, "127.0.0.1", 48156);
                    }
                    catch { }
                    System.Threading.Thread.Sleep(100);
                }
            });
            output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2UdpRecv.bas"));
            cts.Cancel();
            senderTask.GetAwaiter().GetResult();
            sender.Dispose();
            if (output.Contains("U-hello", StringComparison.Ordinal)) break;
        }
        var lines = output!.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Contains("U-hello", lines);
        Assert.Contains("MAIN-DONE", lines);
    }

    [Fact]
    public void V2Driver_Runs_GraphicsParks_Headless()
    {
        // G6: SCREEN/SYNC/POINT/LOAD TEXTURE come park grafici v2. In headless
        // (NullGraphicsService) completano subito: POINT=0, LOAD TEXTURE=-1.
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2Graphics.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Contains("P- 0 ", lines);
        Assert.Contains("T- 1 ", lines);
        Assert.Contains("S-OK", lines);
        Assert.Contains("G-OK", lines);
        Assert.Contains("MAIN-DONE", lines);
    }

    [Fact]
    public void V2Driver_Runs_GraphicsParks_InNestedContexts()
    {
        // G6: LOAD TEXTURE (park) dentro FOR + IF + TRY/FINALLY + call chain (SUB callee):
        // il park deve essere sospendibile e riprendere in ogni contesto.
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2GraphicsNested.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        // FOR i=1 TO 2: i=1 → IF TRUE → TRY (T-0, FIN); i=2 → ELSE → CALL LoadAndReport (L-0).
        Assert.Equal(new[] { "T- 1 ", "FIN", "L- 1 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void V2Driver_Runs_SleepGoroutine_WakesAndCompletes()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2SleepGoroutine.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Contains("WOKE", lines);
        Assert.Contains("MAIN-DONE", lines);
    }

    [Fact]
    public void V2Driver_Runs_SendSleepSend()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2SendSleepSend.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Contains("V1- 1 ", lines);
        Assert.Contains("V2- 2 ", lines);
        Assert.Contains("MAIN-DONE", lines);
    }

    [Fact]
    public void V2Driver_Runs_HttpFetch_ResolvesFuture()
    {
        // G5: HTTP FETCH espressione sotto la v2 — HttpFetchExprAsync ritorna un future;
        // la lettura di r.StatusCode dereferenzia il future. Listener locale su 127.0.0.1
        // (lo stesso host che i sample BSP usano — copre il fix del prefisso HttpService).
        var listener = new System.Net.HttpListener();
        listener.Prefixes.Add("http://127.0.0.1:8137/");
        listener.Start();
        var stop = new CancellationTokenSource();
        var pump = Task.Run(async () =>
        {
            while (!stop.IsCancellationRequested)
            {
                var ctx = await listener.GetContextAsync();
                var body = System.Text.Encoding.UTF8.GetBytes("OK");
                ctx.Response.StatusCode = 200;
                ctx.Response.ContentType = "application/json";
                await ctx.Response.OutputStream.WriteAsync(body, 0, body.Length);
                ctx.Response.Close();
            }
        });
        try
        {
            var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2HttpFetch.bas"));
            var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
            Assert.Contains("S- 200 ", lines);
            Assert.Contains("MAIN-DONE", lines);
        }
        finally
        {
            stop.Cancel();
            listener.Stop();
        }
    }

    [Fact]
    public void V2Driver_Runs_GoSelect_ReceiveCase()
    {
        var output = RunV2Sample(Path.Combine("samples", "Community", "Concurrency", "GpmV2GoSelect.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(new[] { "SEL- 42 ", "MAIN-DONE" }, lines);
    }

    [Fact]
    public void Driver_GosubReturn_PushesPopsContinuation()
    {
        // G3: GOSUB NON crea un activation frame — pusha una continuazione sul GosubStack del
        // frame e salta al target; RETURN nel corpo la popa e riprende la continuazione.
        var task = new PCodeTask { Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var driver = new IlGpmV2Driver();

        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.Gosub, ResumePc: 5, TargetProcedure: 10,
                null, IlReturnDestination.Discard, null, null, null)));
        Assert.Equal(10, driver.CurrentFrame!.Pc);
        Assert.Single(driver.CurrentFrame.GosubStack!);

        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.Continue, ResumePc: 11, TargetProcedure: 0,
                null, IlReturnDestination.Discard, null, null, null)));
        Assert.Equal(11, driver.CurrentFrame!.Pc);

        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.ReturnGosub, ResumePc: 0, TargetProcedure: 0,
                null, IlReturnDestination.Discard, null, null, null)));
        Assert.Equal(5, driver.CurrentFrame!.Pc);
        Assert.Empty(driver.CurrentFrame.GosubStack!);

        Assert.Equal(IlGpmStepResult.Finished,
            driver.Apply(task, new IlStepTransition(IlStepKind.ReturnGosub, ResumePc: 0, TargetProcedure: 0,
                null, IlReturnDestination.Discard, null, null, null)));
        Assert.Null(driver.CurrentFrame);
    }

    [Fact]
    public void Driver_Throw_Unwinds_FinallyFirst_ThenCatch()
    {
        var task = new PCodeTask { Frame = new PCodeCallFrame { OwnerProcedure = 0 } };
        var driver = new IlGpmV2Driver();

        driver.CurrentFrame!.ExceptionStack = new System.Collections.Generic.Stack<IlExceptionRegionState>();
        driver.CurrentFrame.ExceptionStack.Push(new IlExceptionRegionState { CatchPc = 20, FinallyPc = 10, EndPc = 30 });

        var ex = new InvalidOperationException("boom");
        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.Throw, 0, 0, null,
                IlReturnDestination.Discard, null, null, ex)));
        Assert.Equal(10, driver.CurrentFrame!.Pc);
        Assert.NotNull(driver.CurrentFrame.Unwind);
        Assert.Single(driver.CurrentFrame.CleanupStack!);

        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.Continue, ResumePc: 11, TargetProcedure: 0,
                null, IlReturnDestination.Discard, null, null, null)));
        Assert.Equal(11, driver.CurrentFrame!.Pc);
        Assert.NotNull(driver.CurrentFrame.Unwind);

        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.ContinueUnwind, 0, 0, null,
                IlReturnDestination.Discard, null, null, null)));
        Assert.Equal(20, driver.CurrentFrame!.Pc);
        Assert.True(driver.CurrentFrame.CleanupStack!.Peek().Completed);

        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.Continue, ResumePc: 21, TargetProcedure: 0,
                null, IlReturnDestination.Discard, null, null, null)));
        Assert.Equal(21, driver.CurrentFrame!.Pc);
        Assert.Equal(IlGpmStepResult.Continue,
            driver.Apply(task, new IlStepTransition(IlStepKind.ContinueUnwind, 0, 0, null,
                IlReturnDestination.Discard, null, null, null)));
    }
    // ─── G8-bis: parità v2 con la GPM PCodeVM estesa (2026-08-27) ───────────────────────────

    [Fact]
    public void V2Driver_Runs_Cond_WaitSignalWakeup()
    {
        // G8-bis: COND WAIT/SIGNAL (parità Go sync.Cond) — il Consumatore parcheggia sul cond
        // (rilasciando il mutex), Main imposta Ready=1 e fa SIGNAL → il Consumatore si sveglia,
        // riacquisisce il mutex e completa. Usa il GLOBALE `Ready` condiviso tra i task.
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2Cond.bas"));
        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Contains("CONSUMATORE-SVEGLIATO", lines);
        Assert.Contains("COND-DONE", lines);
    }

    [Fact]
    public void V2Driver_Runs_Pool_GetPut()
    {
        // G8-bis: POOL (parità Go sync.Pool) — Get dopo Put riusa l'oggetto; il secondo Get è vuoto.
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2Pool.bas"));
        Assert.Contains("POOL-OK", output);
    }

    [Fact]
    public void V2Driver_Runs_SyncMap_StoreLoadDelete()
    {
        // G8-bis: SYNCMAP (parità Go sync.Map) — Store/Load/Delete/LoadOrStore flat.
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2SyncMap.bas"));
        Assert.Contains("SYNCMAP-STORE-LOAD-OK", output);
    }

    [Fact]
    public void V2Driver_Runs_AtomicSwap_ReturnsPrevious()
    {
        // G8-bis: ATOMIC.SWAP (parità Go atomic.SwapInt64) — ritorna il valore PRECEDENTE.
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2AtomicSwap.bas"));
        Assert.Contains("ATOMIC-SWAP-OK", output);
    }

    [Fact]
    public void V2Driver_Runs_NumGoroutine_NumCpu()
    {
        // G8-bis: SYSTEM.NUMGOROUTINE/NUMCPU (parità Go runtime.NumGoroutine/NumCPU) — con 2
        // goroutine attive (Main + 1), NUMGOROUTINE >= 3 al momento della lettura.
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2NumGoroutine.bas"));
        Assert.Contains("NUMGOROUTINE-OK", output);
    }

    [Fact]
    public void V2Driver_Runs_ForEachChannel_IteratesUntilClosed()
    {
        // G8-bis: FOR EACH su canale (parità Go for v := range ch) — itera finché il canale
        // non è chiuso e svuotato; la RECEIVE parcheggia quando il canale è vuoto ma aperto.
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ForEachChannel.bas"));
        Assert.Contains("FOR-EACH-CH-OK", output);
    }

    [Fact]
    public void V2Driver_Runs_ChannelDirection_ProducerSendOnly_ConsumerReceiveOnly()
    {
        // G8-bis: canali direzionali (parità Go chan<- T / <-chan T) — il canale bidirezionale
        // viene passato ai worker col tipo direzionale (SEND/OF, RECEIVE/OF); il runtime è
        // invariato. Totale deterministico 1+2+3 = 6.
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2ChannelDirection.bas"));
        Assert.Contains("GPM-V2-CHAN-DIR-OK", output);
        Assert.Contains("GPM-V2-CHAN-DIR-DONE", output);
    }

    [Fact]
    public void ChannelDirection_Violation_SendOnReceiveOnly_ThrowsAtCompile()
    {
        // G8-bis: SEND su un canale ReceiveOnly ('<-chan T') è un errore di compilazione
        // (parità PCode/Go) — il lowering v2 lo rifiuta a emissione, non a runtime.
        const string src =
            "SUB Bad(recvCh AS CHANNEL RECEIVE OF INTEGER)\n" +
            "  SEND 1 TO recvCh\n" +
            "END SUB\n" +
            "SUB Main()\n" +
            "  DIM ch AS CHANNEL OF INTEGER\n" +
            "  Bad(ch)\n" +
            "END SUB\n";
        var program = Basic.Orchestrator.CompileFromString(src, "ChanDirViolation.bas", null);
        var ex = Assert.Throws<System.NotSupportedException>(() => IlProgramCompiler.CompileToAssembly(program, "ChanDirViolation", out _));
        Assert.Contains("ReceiveOnly", ex.Message);
    }

    [Fact]
    public void ChannelDirection_Violation_ReceiveOnSendOnly_ThrowsAtCompile()
    {
        // G8-bis: RECEIVE su un canale SendOnly ('chan<- T') è un errore di compilazione.
        const string src =
            "SUB Bad(sendCh AS CHANNEL SEND OF INTEGER)\n" +
            "  DIM v AS INTEGER\n" +
            "  RECEIVE v FROM sendCh\n" +
            "END SUB\n" +
            "SUB Main()\n" +
            "  DIM ch AS CHANNEL OF INTEGER\n" +
            "  Bad(ch)\n" +
            "END SUB\n";
        var program = Basic.Orchestrator.CompileFromString(src, "ChanDirViolation2.bas", null);
        var ex = Assert.Throws<System.NotSupportedException>(() => IlProgramCompiler.CompileToAssembly(program, "ChanDirViolation2", out _));
        Assert.Contains("SendOnly", ex.Message);
    }

    [Fact]
    public void V2Driver_Runs_StringComparison_InCondition()
    {
        // G8: confronto STRINGHE in condizione IF sotto la v2 (IF s$ = "x" / AND con stringa) —
        // prima EmitExprDouble(StringExpr) lanciava a metà emissione → StepV2 parziale →
        // "Label not marked" al Save (lezione 7f2ac888, riemersa in NetUdpAdvanced).
        var output = RunSample(Path.Combine("samples", "Community", "Concurrency", "GpmV2StringCmp.bas"));
        Assert.Contains("EQ-OK", output);
        Assert.Contains("NE-OK", output);
        Assert.Contains("AND-STR-OK", output);
        Assert.Contains("GPM-V2-STRCMP-DONE", output);
    }
}
