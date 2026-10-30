using System;
using System.IO;
using System.Reflection;
using System.Text;
using Basic;
using Basic.IlBackend;

// test_ilrun — esegue UN sample .bas e stampa OK/errore. Backend selezionabile con
// --engine=il (default, compila e invoca l'assembly IL generato in-process),
// --engine=pcode (PCodeVm C#, stesso motore attivato da `Executor --pcode`), o
// --engine=vmc (compila+serializza il chunk e lancia gmb_vm_host come sottoprocesso,
// stdout/stderr in streaming invece che bufferizzati a fine run — utile per diagnosticare
// un hang/timeout VM-C, dove il wrapper xUnit RunVmc mostra output solo a kill avvenuto;
// timeout configurabile via GMB_VMC_DIAG_TIMEOUT_MS, default 15000; allo scadere invia
// SIGQUIT al processo per un tentativo di backtrace prima del kill).
var engine = "il";
var gfx = false;
var positional = new System.Collections.Generic.List<string>();
foreach (var a in args)
{
    if (a.StartsWith("--engine=", StringComparison.OrdinalIgnoreCase))
        engine = a["--engine=".Length..].Trim().ToLowerInvariant();
    else if (a == "--gfx")
        gfx = true;
    else
        positional.Add(a);
}
if (engine is not ("il" or "pcode" or "vmc"))
{
    Console.Error.WriteLine($"[ARGS] --engine sconosciuto: {engine} (valori validi: il, pcode, vmc)");
    return 1;
}

// Headless SEMPRE (come test_ilrun-sweep e test_vmrun): l'entry IL generata chiama
// IlRuntime.PrepareGraphicsHost() solo se GMBASIC_HEADLESS != "1". Senza questa env
// l'entry cabla la SdlGraphicsService REALE e costruisce un host che SOSTITUISCE quello
// Null impostato sotto: i park grafici (SCREEN/SYNC/...) accodano sulla coda SDL che
// questo runner non pompa MAI → il task GPM resta parcheggiato per sempre (hang —
// sintomo A del bug GPM-Graphics-Park-SCREEN-MOUSE-Headless-Hang-CPU-Spin).
Environment.SetEnvironmentVariable("GMBASIC_HEADLESS", "1");

var root = FindRepoRoot();
var samplePath = positional.Count > 0 ? positional[0] : Path.Combine(root, "shared", "samples", "Community", "Collections", "CollectionsLinked.bas");
var noOpt = Environment.GetEnvironmentVariable("GAI_NO_ILOPT") == "1";
var dbg = Environment.GetEnvironmentVariable("GAI_DBG") == "1";
var method = Environment.GetEnvironmentVariable("GAI_METHOD");

static string FindRepoRoot()
{
    var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
    while (dir != null)
    {
        if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
        /* Monorepo: root = quella che contiene GMBasicSharp/GMBasicSharp.slnx + submodule GMBasic. */
        if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))
            && Directory.Exists(Path.Combine(dir.FullName, "GMBasic"))) return dir.FullName;
        dir = dir.Parent;
    }
    return Directory.GetCurrentDirectory();
}

// CWD = cartella del sample: senza questo, qualunque OPEN/BSAVE/SAVE relativo del
// programma BASIC scrive nella CWD del processo dotnet (root del repo se lanciato da
// lì) invece che accanto al sample — causa storica di sporcizia in root (AGENTS.md §9).
// samplePath va reso assoluto PRIMA di cambiare CWD, altrimenti un path relativo
// (es. dato da riga di comando) verrebbe poi risolto contro la CWD sbagliata.
samplePath = Path.GetFullPath(samplePath);
Directory.SetCurrentDirectory(Path.GetDirectoryName(samplePath)!);

BasicProgram program;
try { program = Orchestrator.Compile(samplePath, null, false); }
catch (Exception ex) { Console.Error.WriteLine($"[COMPILE] {ex.GetType().Name}: {ex.Message}"); return 2; }
if (program?.MainModule == null) { Console.Error.WriteLine("[COMPILE] null"); return 2; }

// Headless runtime host: i park grafici (SCREEN/SPRITE/LOAD TEXTURE) completano via
// NullGraphicsService — senza, IlRuntime.Current ha la grafica SDL reale che il runner
// non pompa → i park non si completano mai (hang). Stessa impostazione dello sweep.
// NB: DefaultGraphics è STATICO e può essere l'SDL — passare il Null ESPLICITO.
// --gfx: grafica NATIVA (CGfxGraphicsService → gmb_gfx), non Null. Serve a verificare i
// controlli/Clighter @GET in modalità grafica sul backend IL (SCREEN!=0); con NullGraphics
// lo SCREEN non attiva il motore C e la READ ricade sul percorso testo (GMB_TUI_SCRIPT).
Basic.Runtime.IGraphicsService __gfxSvc = gfx
    ? new Basic.Runtime.CGfxGraphicsService()
    : new Basic.Runtime.NullGraphicsService();
if (gfx)
{
    // L'entry IL generata crea il proprio host con HeadlessRuntimeHost.DefaultGraphics:
    // va impostato PRIMA (e PrepareGraphicsHost completa clipboard/audio/matrix resolver).
    Basic.Runtime.HeadlessRuntimeHost.DefaultGraphics = __gfxSvc;
    Basic.IlBackend.IlRuntime.PrepareGraphicsHost();
}
var __host = new Basic.Runtime.HeadlessRuntimeHost(Console.Out, new Basic.Runtime.RuntimeConfig(), __gfxSvc);
IlRuntime.SetCurrent(__host);
Basic.IlBackend.IlRuntime.InjectScriptFromEnv();
// Esecuzione IN-PROCESS: il BasicProgram è vivo → la cattura globali di EVAL/&/macro deve
// risolvere contro program.Symbols (SetMacroContext). Senza, EVAL/& che leggono/scrivono
// variabili GLOBALI reali falliscono silenziosamente (regola: ogni esecutore in-process deve
// impostare il contesto macro; i soli eseguibili separati usano la tabella .globals.json).
IlRuntime.SetMacroContext(program);
/* Argomenti del PROGRAMMA (dopo il file): COMMAND(1)=primo, non i flag dell'harness. */
Basic.IlBackend.IlRuntime.SetCommandArgs(positional.Skip(1).ToArray());

if (engine == "pcode")
{
    var chunk = new global::Compiler.Compiler().Compile(program);
    var result = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
    var output = result.Output;
    // Parità CLI con le gambe IL/VM-C (2026-09-23, todo Parita-5-Backend-Residui §2B):
    // PCodeVmRunner accoda "[P-CODE ERROR] <msg>" allo stdout catturato, mentre IL e VM/C
    // emettono il messaggio d'errore SOLO su stderr (IL: "[RUN] ...", VMC: "[GMBASIC] ERROR").
    // Lo si sposta su stderr: lo stdout resta l'output del programma, identico alle altre gambe.
    // (PCodeVmRunner resta invariato: l'harness xUnit MiniTestsParityTests estrae il marcatore
    // da result.Output — vedi pcodeMarker — e REPL/Executor non leggono il marcatore.)
    const string pcodeMarker = "[P-CODE ERROR] ";
    var midx = output.LastIndexOf(pcodeMarker, StringComparison.Ordinal);
    if (midx >= 0)
    {
        Console.Write(output[..midx]);
        Console.Error.WriteLine(output[midx..].TrimEnd());
    }
    else Console.Write(output);
    if (!result.RuntimeError) { Console.WriteLine("[OK] terminato senza eccezioni"); return result.ExitCode; }
    Console.Error.WriteLine($"[RUN] exit code {result.ExitCode}");
    return 6;
}

if (engine == "vmc")
{
    // Esegue via gmb_vm_host con streaming di stdout/stderr e timeout configurabile,
    // stampando l'output parziale anche in caso di timeout/kill (il wrapper xUnit
    // MiniTestsParityTests.RunVmc uccide a 60s senza mai mostrare output parziale —
    // utile per diagnosticare DOVE si blocca un hang VM-C).
    var chunk = new global::Compiler.Compiler().Compile(program);
    var chunkPath = Path.Combine(Path.GetTempPath(), "gmbasic_vmc_" + Guid.NewGuid().ToString("N") + ".chunk");
    File.WriteAllBytes(chunkPath, Basic.Runtime.ChunkA1Serializer.Serialize(chunk));
    var exePath = Path.Combine(root, "GMBasic", "native", "gmb_vm_c", "build", "gmb_vm_host");
    var psi = new System.Diagnostics.ProcessStartInfo(exePath, "\"" + chunkPath + "\"")
    {
        UseShellExecute = false,
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        WorkingDirectory = Path.GetDirectoryName(samplePath)!,
    };
    psi.Environment["GMBASIC_TEST_FIXTURES"] = "1";
    using var proc = System.Diagnostics.Process.Start(psi)!;
    var stdoutSb = new StringBuilder();
    var stderrSb = new StringBuilder();
    proc.OutputDataReceived += (_, e) => { if (e.Data != null) { stdoutSb.AppendLine(e.Data); Console.Error.WriteLine("[VMC-OUT] " + e.Data); } };
    proc.ErrorDataReceived += (_, e) => { if (e.Data != null) { stderrSb.AppendLine(e.Data); Console.Error.WriteLine("[VMC-ERR] " + e.Data); } };
    proc.BeginOutputReadLine();
    proc.BeginErrorReadLine();
    var timeoutMs = int.TryParse(Environment.GetEnvironmentVariable("GMB_VMC_DIAG_TIMEOUT_MS"), out var tms) ? tms : 15000;
    if (!proc.WaitForExit(timeoutMs))
    {
        Console.Error.WriteLine($"[VMC-DIAG] TIMEOUT dopo {timeoutMs}ms, pid={proc.Id} — invio SIGQUIT per backtrace, poi kill");
        try { System.Diagnostics.Process.Start("kill", $"-QUIT {proc.Id}")?.WaitForExit(2000); } catch { }
        System.Threading.Thread.Sleep(500);
        try { proc.Kill(entireProcessTree: true); } catch { }
        Console.Error.WriteLine("[VMC-DIAG] stdout parziale:\n" + stdoutSb);
        Console.Error.WriteLine("[VMC-DIAG] stderr parziale:\n" + stderrSb);
        try { File.Delete(chunkPath); } catch { }
        return 99;
    }
    try { File.Delete(chunkPath); } catch { }
    Console.WriteLine(stdoutSb.ToString());
    return proc.ExitCode;
}

// run precedenti interrotti (kill/timeout) lasciano la loro cartella: sweep di quelle vecchie (> 12 h).
IlProgramCompiler.CleanupStaleTempDirs("gmb_ilrun_", TimeSpan.FromHours(12));
var tmpDir = Path.Combine(Path.GetTempPath(), "gmb_ilrun_" + Guid.NewGuid().ToString("N")[..8]);
Directory.CreateDirectory(tmpDir);
string asmPath;
try
{
    asmPath = IlProgramCompiler.Compile(program, samplePath, tmpDir, "GeneratedProgram", null, noOpt, true, linkRuntime: true);
}
catch (Exception ex)
{
    Console.Error.WriteLine($"[ILGEN] {ex.GetType().Name}: {ex.Message}");
    if (dbg) Console.Error.WriteLine(ex.StackTrace);
    return 3;
}
if (!File.Exists(asmPath)) { Console.Error.WriteLine("[ILGEN] assembly non generato"); return 3; }

var dllPath = Path.ChangeExtension(asmPath, ".dll");
Assembly asm;
try { asm = Assembly.LoadFile(Path.GetFullPath(dllPath)); }
catch (Exception ex) { Console.Error.WriteLine($"[LOAD] {ex.GetType().Name}: {ex.Message}"); return 4; }

try
{
    var t = asm.GetType("GeneratedProgram");
    if (method != null && t != null)
    {
        var m = t.GetMethod(method, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
        if (m == null) { Console.Error.WriteLine($"[DBG] metodo {method} non trovato"); return 5; }
        var ps = m.GetParameters();
        var margs = new object?[ps.Length];
        for (var i = 0; i < ps.Length; i++) margs[i] = ps[i].ParameterType.IsValueType ? Activator.CreateInstance(ps[i].ParameterType) : null;
        m.Invoke(null, margs);
    }
    else
    {
        var main = t?.GetMethod("Main", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
        if (main == null) { Console.Error.WriteLine("[RUN] GeneratedProgram.Main non trovato"); return 5; }
        var injected = Environment.GetEnvironmentVariable("GMB_INJECT_KEY");
        if (injected != null)
        {
            /* INKEY legge dal KeyboardService headless (TryPollHeadlessKeyboard), NON dalla coda
             * statica InkeyQueue: inietta QUI (QueueKey), + QueueInkey per WAITKEY/altri path. */
            foreach (var ch in injected) __host.QueueKey(ch);
            IlRuntime.QueueInkey(injected);
            Console.Error.WriteLine($"[DBG] key iniettata: '{injected}'");
        }
        /* Dump opzionale dello schermo TUI renderizzato da IL (per confronto con C99Native):
         * setta il ScreenBuffer Clipper 80x25 PRIMA della run e lo serializza dopo. */
        var __dumpPath = Environment.GetEnvironmentVariable("GMB_SCREEN_DUMP");
        CommonLib.Consoles.Tui.ScreenBuffer? __scr = null;
        if (!string.IsNullOrEmpty(__dumpPath))
        {
            __scr = new CommonLib.Consoles.Tui.ScreenBuffer(80, 25);
            ClipperLib.Clipper.ClipperGlobal.State.Buffer = __scr;
            ClipperLib.Clipper.ClipperGlobal.State.RenderHook = _ => { };
        }
        var __ret = main.Invoke(null, null);
        if (__ret is int __ri) Basic.IlBackend.IlRuntime.SetProgramExitCode(__ri);
        else if (__ret is long __rl) Basic.IlBackend.IlRuntime.SetProgramExitCode((int)__rl);
        else if (__ret is double __rd) Basic.IlBackend.IlRuntime.SetProgramExitCode(__rd);
        if (__scr != null)
        {
            var sb = new StringBuilder();
            for (int y = 0; y < 25; y++)
            {
                for (int x = 0; x < 80; x++)
                {
                    var cell = __scr.Cells[y, x];
                    sb.Append(cell.Character != 0 ? (char)cell.Character : ' ');
                }
                sb.Append('\n');
            }
            File.WriteAllText(__dumpPath!, sb.ToString());
            Console.Error.WriteLine("[SCREEN] dumped " + __dumpPath);
        }
    }
    Console.WriteLine("[OK] terminato senza eccezioni");
    return Basic.IlBackend.IlRuntime.ProgramExitCode;
}
catch (Exception ex)
{
    var inner = ex is TargetInvocationException tie ? tie.InnerException ?? ex : ex;
    Console.Error.WriteLine($"[RUN] {inner.GetType().Name}: {inner.Message}");
    if (dbg) Console.Error.WriteLine(inner.StackTrace);
    return 6;
}
finally
{
    if (Environment.GetEnvironmentVariable("GAI_KEEP") == "1")
        Console.Error.WriteLine($"[KEEP] {tmpDir}");
    else
        try { Directory.Delete(tmpDir, true); } catch { }
}
