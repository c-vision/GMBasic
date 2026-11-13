using System.Reflection;
// MiniTests — parità cross-backend: ogni mini-programma in MiniTests/ deve produrre
// lo STESSO output su IL e su P-Code (oracolo a vicenda). Il gate C99 li esegue a parte.
using System.IO;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class MiniTestsParityTests
{
    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null)
        {
            // Monorepo: workspace = parent di GMBasicSharp (contiene shared/ e GMBasic/).
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp.slnx")))
                return dir.Parent?.FullName ?? dir.FullName;
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return ".";
    }

    /// <summary>Isola i backend: RunPCode e RunIl girano nello STESSO processo e condividono il
    /// singleton AgentServer (history per-agente). Senza reset, il secondo run vede la history
    /// del primo (transcript accumulato) e divergerebbe da VMC (processo fresco). B4/gap agent.</summary>
    private static void ResetAgents()
    {
        // Policy LLM: lo stato (rule-coppie role->fmt, policy NOMINATE DSL, policy attiva,
        // override per-tensore e piano) vive in GLOBALI C della dylib `gmb_ai_llm` → senza reset
        // trapela da un file all'altro e da una gamba all'altra (IL e PCode girano nello stesso
        // processo): `LLM POLICY SAVE/SHOW/ITEMS` vedevano la policy dei test precedenti e gli
        // EXPECT di `llm/*` erano registrati su stato sporco. Stesso punto d'ingresso del BASIC:
        // `LLM POLICY RESET` (dopo il fix azzera TUTTO lo stato).
        try { Basic.Providers.LlmModelNative.llmmodelio_policy_cmd("RESET", "", "", ""); }
        catch { /* dylib non caricabile: si prosegue (le gambe C usano processi freschi) */ }
        // Scheduler/poller v2 e motore grafico sono STATICI di processo: tra una gamba e
        // l'altra si torna a TEXT (SCREEN n di un MiniTest lasciava la grafica ON e i
        // successivi PRINT/UserControl finivano sul buffer grafico non pompato → output vuoto).
        try { Basic.IlBackend.Gpm.IlGpmV2Driver.ResetSharedPoller(); } catch { }
        try { Basic.Runtime.CGfxGraphicsService.ForceScreenOff(); } catch { }
        try { Basic.IlBackend.IlRuntime.FormReset(); } catch { }
        // Aree DBF/ISAM e mappe alias statiche: un programma che non chiude le sue aree (alias `C`,
        // `A`…) altrimenti le lascia risolvere al programma successivo (isam_all dopo dbf_select).
        try { Basic.IlBackend.IlRuntime.ResetDatabaseState(); } catch { }
        try
        {
            var server = Basic.Agents.AgentServer.Instance;
            foreach (var a in server.List().ToArray()) server.Delete(a.Name);
        }
        catch { /* best effort */ }
        // Registri di orchestrazione: singleton di processo → vanno azzerati tra i backend
        // (PCode/IL condividono il processo), altrimenti lo stato trapela tra i MiniTest.
        try
        {
            Basic.Agents.ArtifactStore.Clear();
            Basic.Agents.HandoffRegistry.Clear();
            Basic.Agents.MetricsRegistry.Reset();
            Basic.Agents.CheckpointRegistry.Clear();
            Basic.Agents.ReplayRegistry.Clear();
            Basic.AI.Providers.CategoryRegistry.Clear();
            // ProviderServer è un singleton di processo: senza reset, `AI.Rotate` avanza il
            // contatore di rotazione (weighted) e PCode/IL (stesso processo, gambe sequenziali)
            // vedevano provider diversi (ai_rotate: IL 'rem-http' vs PCode 'fixture-completion').
            Basic.Providers.ProviderServer.Instance.ResetAll();
            Basic.Agents.ResourceRegistry.Clear();
            Basic.Agents.RuleRegistry.Clear();
            Basic.Agents.TaskGraphRegistry.Clear();
            Basic.Agents.PlanRegistry.Clear();
            Basic.Agents.HookRegistry.Clear();
            Basic.Agents.GoalRegistry.Clear();
            Basic.AI.Knowledge.KnowledgeGraphRuntime.Reset();
            // P3 (2026-09-16): worker/context engine sono singleton di processo e NON erano
            // azzerati → i contatori (rounds/rescued/spend) e le viste trapelavano tra PCode e IL
            // (misurato: worker.bas dava rounds=1 su PCode e 2 su IL). MiniTest deterministico
            // ⇒ ogni gamba deve partire dallo stesso stato.
            Basic.AI.Agents.AiWorkerRuntime.Reset();
            Basic.AI.Context.AiContextEngine.Reset();
            Basic.AI.Agents.ObservationRegistry.Reset();   // observe_*.bas: delta deterministico
            Basic.Agents.AgentQueueManager.Instance.ClearAll();
            // Vector :memory: persistente per-run (parità g_vdb C): va azzerato tra le gambe
            // (PCode/IL condividono il processo) altrimenti il 2° run vede i dati del 1°.
            Basic.AI.Rag.AiVectorRuntime.ResetMemoryConnections();
        }
        catch { /* best effort */ }
    }

    /// <summary>Pulisce gli artefatti creati dai gen-test (es. il path "x" usato come file/dir).</summary>
    private static void CleanupArtifacts(string file)
    {
        try
        {
            var dir = Path.GetDirectoryName(Path.GetFullPath(file))!;
            var x = Path.Combine(dir, "x");
            if (Directory.Exists(x)) Directory.Delete(x, true);
            else if (File.Exists(x)) File.Delete(x);
        }
        catch { }
    }

    /// <summary>Rimuove la cartella `bin/` accanto al MiniTest PRIMA di ogni gamba.
    /// I MiniTest DB (SPEED/DBF/ISAM) e gli altri output scrivono in `bin/&lt;file&gt;`: è la
    /// cartella di output di default del compilatore (AGENTS §9.6) e `work/` NON si usa fuori da
    /// `shared/samples/` (AGENTS §11.1 cat.4: solo artefatti-dati dei sample). Il RMDIR dei DB è
    /// non ricorsivo e fallisce silenziosamente su una dir non vuota, quindi una gamba ritrovava
    /// il database della precedente (e per SPEED il `.speed.lock`/WAL faceva fallire la gamba C —
    /// vedi Bugs/(open) - SPEED-Parity-Runner-WAL-Lock). Ogni gamba parte pulita.</summary>
    private static void CleanupBinDir(string file)
    {
        try
        {
            var dir = Path.GetDirectoryName(Path.GetFullPath(file))!;
            var bin = Path.Combine(dir, "bin");
            if (Directory.Exists(bin)) Directory.Delete(bin, true);
            // La cartella si RICREA: diversi test scrivono in `bin/` senza crearla
            // (es. llm/llm_dense.bas → `LLM TENSOR DENSE … "bin/dense.gguf"`).
            Directory.CreateDirectory(bin);
        }
        catch { }
    }

    /// <summary>Pulisce i file DBF temporanei in /tmp/ per evitare collisioni
    /// tra PCode e IL (stesso processo, fixture residue nei percorsi assoluti).
    /// I minitest DBF usano path /tmp/gmb_* — una gamba lascia il file che la
    /// gamba successiva rilegge come se fosse output fresco.</summary>
    private static void CleanupDbfTempFiles()
    {
        try
        {
            foreach (var f in Directory.GetFiles("/tmp", "*.dbf"))
            {
                try { File.Delete(f); } catch { }
            }
        }
        catch { }
    }

    /// <summary>Esegue <paramref name="body"/> su un thread con STACK AMPIO (64MB): il load del
    /// motore MLX/Metal (LLM QUALITY / REGISTRY MEASURE) e' ricorsivo-profondo e con lo stack di
    /// default del thread-pool (~1MB) fa StackOverflow (misurato 2026-10-08 sulla PPL reale).
    /// Ritorna false su timeout; l'eventuale eccezione del corpo in <paramref name="error"/>.</summary>
    private static bool RunOnBigStack(Action body, TimeSpan timeout, out Exception? error)
    {
        Exception? err = null;
        var t = new System.Threading.Thread(() => { try { body(); } catch (Exception e) { err = e; } }, 64 * 1024 * 1024);
        t.IsBackground = true;
        t.Start();
        var ok = t.Join(timeout);
        error = err;
        return ok;
    }

    private static string RunPCode(string file)
    {
        var originalCwd = Directory.GetCurrentDirectory();
        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            CleanupDbfTempFiles();
            // §9.1: il percorso iniziale e' la dir del file BASIC (entry point), NON la CWD di processo.
            Directory.SetCurrentDirectory(Path.GetDirectoryName(Path.GetFullPath(file))!);
            CleanupArtifacts(file);
            CleanupBinDir(file);
            var program = Basic.Orchestrator.Compile(file, null, false, "il");
            // Macro &/EVAL: compilazione ed esecuzione nello STESSO processo → registra il
            // BasicProgram vivo (la cattura dei GLOBALI per nome non deve dipendere dalla
            // tabella globals.json su disco, che in-process resta quella del PRIMO MiniTest
            // eseguito → macro con globali = NIL nei test successivi).
            Basic.IlBackend.IlRuntime.SetMacroContext(program);
            var chunk = new global::Compiler.Compiler().Compile(program);
            Console.SetOut(sw);
            var pcHost = new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig());
            Basic.IlBackend.IlRuntime.SetCurrent(pcHost);
            // TIMEOUT OBBLIGATORIO: un MiniTest interattivo (park modale/GUI su SYNC) bloccherebbe
            // il runner in-process PER SEMPRE. 20s come la gamba IL.
            global::Basic.IlBackend.PCode.PCodeVmResult r = null!;
            if (!RunOnBigStack(() =>
            {
                Basic.IlBackend.IlRuntime.SetCurrent(pcHost);
                Basic.IlBackend.IlRuntime.SetCurrentForThread(pcHost);
                r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
            }, TimeSpan.FromSeconds(20), out var pcerr))
                throw new Exception($"[PCode] {file} TIMEOUT (20s) — park/interattivo");
            if (pcerr != null) throw pcerr;
            Assert.True(r.Success, $"[PCode] {file} fallito:\n{r.Output}");
            return r.Output;
        }
        finally { Console.SetOut(originalOut); Directory.SetCurrentDirectory(originalCwd); }
    }

    private static string RunIl(string file)
    {
        var originalCwd = Directory.GetCurrentDirectory();
        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            CleanupDbfTempFiles();
            Directory.SetCurrentDirectory(Path.GetDirectoryName(Path.GetFullPath(file))!);
            CleanupBinDir(file);
            var program = Basic.Orchestrator.Compile(file, null, false, "il");
            // Macro &/EVAL in-process: registra il BasicProgram vivo (stesso motivo di RunPCode).
            Basic.IlBackend.IlRuntime.SetMacroContext(program);
            var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "IlMini_" + Path.GetFileNameWithoutExtension(file), out _);
            var dll = Path.Combine(Path.GetTempPath(), "IlMini_" + Path.GetFileNameWithoutExtension(file) + ".dll");
            using (var fs = File.Create(dll)) asmBuilder.Save(fs);
            var assembly = System.Reflection.Assembly.LoadFile(dll);
            var main = assembly.GetType("GeneratedProgram")!.GetMethod("Main", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)!;
            Console.SetOut(sw);
            var ilHost = new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig());
            Basic.IlBackend.IlRuntime.SetCurrent(ilHost);
            var task = RunOnBigStack(() =>
            {
                // Il big-stack thread riusa lo stesso worker: IlRuntime.Current è ThreadStatic
                // e conserva l'host del MiniTest PRECEDENTE (writer StringWriter morto) →
                // PRINT finiva nel buffer vecchio (output vuoto). Reinstalla l'host QUI.
                Basic.IlBackend.IlRuntime.SetCurrent(ilHost);
                Basic.IlBackend.IlRuntime.SetCurrentForThread(ilHost);
                main.Invoke(null, null);
            }, TimeSpan.FromSeconds(20), out var ilerr);
            Assert.True(task, $"[IL] {file} TIMEOUT\n{sw}");
            if (ilerr != null) throw ilerr;
            return sw.ToString();
        }
        finally
        {
            Console.SetOut(originalOut);
            Directory.SetCurrentDirectory(originalCwd);
            // Rilascia i DB SPEED aperti (WAL FileShare.None): senza, la gamba successiva
            // nello stesso processo (VMC via subprocess che condivide la dir work/) non
            // riesce ad aprire il database. Vedi IlRuntime.SpeedCloseAll.
            try { Basic.IlBackend.IlRuntime.SpeedCloseAll(); } catch { /* best effort */ }
            try { File.Delete(Path.Combine(Path.GetTempPath(), "IlMini_" + Path.GetFileNameWithoutExtension(file) + ".dll")); } catch { }
        }
    }

    /// <summary>ORACOLO = IL MINITEST STESSO: legge la riga `' EXPECT: <righe unite da |>`
    /// (normalizzata: righe trimmate, vuote scartate). Ritorna null se il MiniTest non la
    /// dichiara. Serve a validare OGNI backend contro un atteso esplicito e non solo l'uno
    /// contro l'altro: tre backend che condividono lo stesso bug risultavano tutti "verdi"
    /// (e' cosi' che sono passati SEEK n,pos ignorato in silenzio e GETFOLDERPATH = parentdir).</summary>
    private static string? ReadExpect(string file)
    {
        foreach (var line in File.ReadLines(file))
        {
            var t = line.TrimStart();
            if (t.StartsWith("' EXPECT: ", StringComparison.Ordinal))
                return t.Substring("' EXPECT: ".Length).TrimEnd();
        }
        return null;
    }

    /// <summary>Scrive/aggiorna la riga `' EXPECT: ...` del MiniTest (backfill dell'oracolo).
    /// La riga viene inserita dopo il blocco di commenti iniziale se assente.</summary>
    private static bool WriteExpect(string file, string expect)
    {
        var lines = File.ReadAllLines(file).ToList();
        var marker = "' EXPECT: ";
        var idx = lines.FindIndex(l => l.TrimStart().StartsWith(marker, StringComparison.Ordinal));
        if (idx >= 0)
        {
            if (lines[idx].TrimStart().Substring(marker.Length).TrimEnd() == expect) return false;
            lines[idx] = marker + expect;
            File.WriteAllLines(file, lines);
            return true;
        }
        var ins = 0;
        foreach (var (l, i) in lines.Select((l, i) => (l, i)))
        {
            if (l.StartsWith("'", StringComparison.Ordinal) || l.Trim().Length == 0) ins = i + 1;
            else break;
        }
        lines.Insert(ins, marker + expect);
        File.WriteAllLines(file, lines);
        return true;
    }

    /// <summary>MiniTest INTERATTIVO (per design blocca: GUI/park modale/event-loop/attesa
    /// input): marcato con una riga header `' TAG: interactive`. Sono ESCLUSI di default da
    /// ogni esecuzione (mirata o --all) e INCLUSI solo con il parametro esplicito
    /// `GMB_RUN_INTERACTIVE=1` (o `--interactive` negli script `tools/test_minitests_*`).
    /// Il marker e' nel FILE (autodescrittivo), non in una lista hardcoded.</summary>
    private static bool IsInteractive(string file)
    {
        try
        {
            foreach (var line in File.ReadLines(file))
            {
                var t = line.TrimStart();
                if (t.Length == 0) continue;
                if (t[0] != '\'') break;   // fine del blocco di commenti iniziale
                if (t.StartsWith("' TAG: interactive", StringComparison.Ordinal)) return true;
            }
        }
        catch { }
        return false;
    }

    private static bool InteractiveEnabled() =>
        string.Equals(Environment.GetEnvironmentVariable("GMB_RUN_INTERACTIVE"), "1", StringComparison.Ordinal);

    /// <summary>MiniTest HEAVY (per design richiede un modello reale su disco + minuti di
    /// training: LLM FINETUNE / LLM LORA APPLY): marcato con una riga header `' TAG: heavy`.
    /// ESCLUSI di default da ogni esecuzione automatica (mirata o --all); INCLUSI solo col
    /// parametro esplicito `GMB_RUN_HEAVY=1`. Il marker e' nel FILE (autodescrittivo).</summary>
    private static bool IsHeavy(string file)
    {
        try
        {
            foreach (var line in File.ReadLines(file))
            {
                var t = line.TrimStart();
                if (t.Length == 0) continue;
                if (t[0] != '\'') break;
                if (t.StartsWith("' TAG: heavy", StringComparison.Ordinal)) return true;
            }
        }
        catch { }
        return false;
    }

    private static bool HeavyEnabled() =>
        string.Equals(Environment.GetEnvironmentVariable("GMB_RUN_HEAVY"), "1", StringComparison.Ordinal);

    private static void CheckExpect(string file, string who, string output, List<string> mismatches)
    {
        var expect = ReadExpect(file);
        if (expect is null) return;
        var got = output.Replace("\n", "|");
        if (!string.Equals(got, expect, StringComparison.Ordinal))
            mismatches.Add($"{Path.GetFileName(file)}: {who} != EXPECT\n  EXPECT:   {expect}\n  OTTENUTO: {got}");
    }

    private static string Norm(string s)
    {
        // Rimuove le sequenze di escape ANSI (CSI ... finale) come fa la `norm()` dello
        // script bash: i comandi grafici headless (BLOAD/IMAGEPUT) ne emettono su VM/C
        // (fallback off-screen "[?47h/l") e inquinerebbero l'oracolo (2026-09-22).
        s = System.Text.RegularExpressions.Regex.Replace(s, "\u001b\\[[0-9;?]*[A-Za-z]", "");
        var lines = s.Replace("\r\n", "\n").Split('\n')
            .Select(l => l.Trim()).Where(l => l.Length > 0 && !IsEngineNoise(l)).ToArray();
        var joined = string.Join("\n", lines);
        // Suffix "(line N)" dei runtime C (vm_error/host): rumore diagnostico, mai parte
        // dell'oracolo (2026-09-22, MiniTest-Oracolo §0.52; nessun MiniTest lo contiene).
        // Vale anche per il backfill: non scrive mai il rumore negli EXPECT.
        return System.Text.RegularExpressions.Regex.Replace(joined, @"\s*\(line \d+\)$", "");
    }

    /// <summary>Rumore diagnostico del motore MLX (caricamento modello): righe che iniziano con uno
    /// dei prefissi in `tests/engine_noise_prefixes.txt` (FONTE UNICA, condivisa con
    /// tools/test_minitests_c99native). Non fanno parte dell'output del programma — dipendono a
    /// runtime da RAM/cache/hash macchina — quindi non entrano nell'oracolo. (Serve ai MiniTest di
    /// PPL reale: LLM REGISTRY MEASURE.) NB: match SPECIFICO, mai "^[" generico.</summary>
    private static bool IsEngineNoise(string l)
    {
        foreach (var p in EngineNoisePrefixes) if (l.StartsWith(p, StringComparison.Ordinal)) return true;
        return false;
    }

    private static readonly string[] EngineNoisePrefixes = LoadEngineNoisePrefixes();
    private static string[] LoadEngineNoisePrefixes()
    {
        try
        {
            var path = Path.Combine(FindRepoRoot(), "GMBasicSharp", "tests", "engine_noise_prefixes.txt");
            if (File.Exists(path))
                return File.ReadAllLines(path)
                    .Select(x => x.Trim())
                    .Where(x => x.Length > 0 && !x.StartsWith("#", StringComparison.Ordinal))
                    .ToArray();
        }
        catch { /* fallback sotto */ }
        return new[] { "[arch]", "[kvquant]", "[p4-plan]", "[kvdisk]", "[prefix]" };
    }

    /// <summary>Host VM/C da lanciare, garantito più nuovo dei sorgenti che lo compilano.
    /// `gmb_vm_host` è linkato BY-PATH (nessuna dipendenza di build dai sorgenti C): un binario
    /// vecchio esegue il runtime vecchio IN SILENZIO e la gamba VMC giudica codice che non è più
    /// quello del working tree. Caso reale 2026-10-30: dopo una modifica a `gmb_vm_core.c` la
    /// gamba VMC stampava ancora l'output del codice precedente, e il MiniTest sembrava fallire
    /// per la modifica invece che per l'host stantio. `VmHostFreshness` (lo stesso file usato da
    /// test_vmrun) ricostruisce in modo incrementale e, se non può, LANCIA: meglio un test rosso
    /// con un messaggio chiaro che un verde su un binario vecchio. `GMBASIC_VM_HOST` resta un
    /// override esplicito (usato da test_vmrun per il figlio) e viene rispettato così com'è.
    /// L'esito è memorizzato: la scansione mtime / il rebuild avvengono una volta per processo.</summary>
    private static string? _vmcHost;
    private static readonly object _vmcHostLock = new();

    private static string ResolveVmcHost()
    {
        var env = Environment.GetEnvironmentVariable("GMBASIC_VM_HOST");
        if (!string.IsNullOrEmpty(env) && File.Exists(env)) return env;
        var cached = _vmcHost;
        if (cached is not null) return cached;
        lock (_vmcHostLock)
        {
            _vmcHost ??= GmbTooling.VmHostFreshness.Ensure(FindRepoRoot());
            return _vmcHost;
        }
    }

    private static string RunVmc(string file)
    {
        CleanupArtifacts(file);
        CleanupBinDir(file);
        var exePath = ResolveVmcHost();
        Assert.True(File.Exists(exePath), $"gmb_vm_host non trovato ({exePath}). Build: cmake --build native/gmb_vm_c/build");
        var program = Basic.Orchestrator.Compile(file, null, false, "il");
        var chunk = new global::Compiler.Compiler().Compile(program);
        var chunkPath = Path.Combine(Path.GetTempPath(), "gmbasic_mini_" + Guid.NewGuid().ToString("N") + ".chunk");
        try
        {
            File.WriteAllBytes(chunkPath, Basic.Runtime.ChunkA1Serializer.Serialize(chunk));
            var psi = new System.Diagnostics.ProcessStartInfo(exePath, "\"" + chunkPath + "\"")
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                WorkingDirectory = Path.GetDirectoryName(file)!,
            };
            // Parità col lato C#: usa i provider fixture, non l'intero providers.json.
            // I test che leggono config (brain/skills/providers reali) NON devono essere isolati:
            // il lato C deve vedere gli stessi file del lato C#.
            var name = System.IO.Path.GetFileName(file);
            if (!name.Contains("Brain", StringComparison.OrdinalIgnoreCase)
                && !name.Contains("SkillsPaths", StringComparison.OrdinalIgnoreCase)
                && !name.Equals("ai_ns_probe.bas", StringComparison.OrdinalIgnoreCase))
                psi.Environment["GMBASIC_TEST_FIXTURES"] = "1";
            /* Headless deterministico: la gamba VM/C crea pagine SOFTWARE (dummy),
             * come C99Native — senza, SDL senza display fa fallire la finestra e POINT
             * ritorna -1 (divergenza fake nei MiniTest pixel). */
            psi.Environment["SDL_VIDEODRIVER"] = "dummy";
            psi.Environment["GMBASIC_ROOT"] = Path.Combine(FindRepoRoot(), "GMBasic");
            // TORCH: libgmb_torch ha una LC_RPATH stale verso il torch di una virtualenv (stub
            // incompatibile) → dyld carica il torch SBAGLIATO → risultati nondeterministici o
            // SIGSEGV. Indichiamo la dir ML col torch corretto (GMB_TORCH_PATH) e, su macOS, la
            // search path dyld (DYLD_LIBRARY_PATH) — servono al LANCIO del processo VM.
            {
                var rid = System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture switch
                {
                    System.Runtime.InteropServices.Architecture.Arm64 => OperatingSystem.IsMacOS() ? "osx-arm64" : "linux-arm64",
                    System.Runtime.InteropServices.Architecture.X64 => OperatingSystem.IsMacOS() ? "osx-x64" : "linux-x64",
                    _ => "osx-arm64",
                };
                var vmRoot = Path.Combine(FindRepoRoot(), "GMBasic");
                var mlDir = Path.Combine(vmRoot, "libs-runtime", "ml", rid);
                var resDir = Path.Combine(vmRoot, "Resources", "libs", rid);
                var rtDir = Path.Combine(vmRoot, "libs-runtime", rid);
                // ORDINE (2026-11-11): build del working tree PRIMA della cache utente. La
                // cache è la release pubblicata e può avere mtime più nuovo ma sorgenti più
                // vecchi (libgmbmath/libgmbor senza i simboli appena aggiunti → VMC "simbolo
                // non trovato"). Stessa regola di tools/sh/parity4 e del runner test_vmrun.
                var buildDir = Path.Combine(vmRoot, "build", "libs", rid);
                var cacheDir = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                    ".gmbasic", "libs", rid);
                psi.Environment["GMB_TORCH_PATH"] = mlDir;
                if (OperatingSystem.IsMacOS())
                    psi.Environment["DYLD_LIBRARY_PATH"] = $"{mlDir}:{buildDir}:{cacheDir}:{resDir}:{rtDir}:{Environment.GetEnvironmentVariable("DYLD_LIBRARY_PATH")}";
            }
            using var proc = System.Diagnostics.Process.Start(psi)!;
            // Timeout OBBLIGATORIO: un programma BASIC che blocca il VMC (attesa su
            // canale/goroutine, input modale, ecc.) altrimenti lascia il processo appeso
            // per SEMPRE — ReadToEnd()/WaitForExit() senza limite → gmb_vm_host orfani
            // (parent=1) che sopravvivono all'abort del test. Letture async + kill
            // dell'albero di processi allo scadere.
            var stdoutTask = proc.StandardOutput.ReadToEndAsync();
            var stderrTask = proc.StandardError.ReadToEndAsync();
            if (!proc.WaitForExit(60000))
            {
                try { proc.Kill(entireProcessTree: true); } catch { }
                throw new Exception($"[VMC] {file} TIMEOUT (60s) — gmb_vm_host ucciso");
            }
            var stdout = stdoutTask.GetAwaiter().GetResult();
            var stderr = stderrTask.GetAwaiter().GetResult();
            Assert.True(proc.ExitCode == 0, $"[VMC] {file} exit={proc.ExitCode}\n{stdout}\nSTDERR: {stderr}");
            return stdout;
        }
        finally { try { File.Delete(chunkPath); } catch { } }
    }

    [Fact]
    public void MiniTests_IL_PCode_Parity()
    {
        var root = FindRepoRoot();
        var dir = Path.Combine(root, "shared", "minitests");
        Assert.True(Directory.Exists(dir), "MiniTests mancante: " + dir);
        // Esecuzione SOLO su richiesta esplicita: il runner NON deve girare mai da solo su
        // tutti i file (decine di processi gmb_vm_host). Serve GMB_MINI_ONLY=<file> per una
        // verifica mirata su uno o piu' file, oppure GMB_RUN_ALL=1 per l'esecuzione completa
        // della suite voluta esplicitamente. In assenza: uscita immediata, nessun processo.
        var only = Environment.GetEnvironmentVariable("GMB_MINI_ONLY");
        var runAll = Environment.GetEnvironmentVariable("GMB_RUN_ALL");
        if (string.IsNullOrEmpty(only) && !string.Equals(runAll, "1", StringComparison.Ordinal))
        {
            Assert.True(true, "Esecuzione suite disabilitata: impostare GMB_MINI_ONLY=<file> o GMB_RUN_ALL=1 per eseguire.");
            return;
        }
        var files = Directory.GetFiles(dir, "*.bas", SearchOption.AllDirectories).OrderBy(f => f).ToArray();
        // Verifica mirata: GMB_MINI_ONLY=<sottostringa> (uno o PIU' filtri separati da virgola,
        // match ANY) processa solo i file che contengono almeno uno dei filtri — un SOLO
        // processo gmb_vm_host per l'intero blocco, mai uno per dominio.
        if (!string.IsNullOrEmpty(only))
        {
            var needles = only.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            files = files.Where(f => needles.Any(n => f.Contains(n, StringComparison.OrdinalIgnoreCase))).ToArray();
        }
        // File SELEZIONATI dal filtro, PRIMA delle esclusioni per design (parità/rete, TAG
        // interactive/heavy): sotto serve a distinguere "il filtro non corrisponde a nulla"
        // (errore dell'utente → deve fallire) da "selezionati ma esclusi per design" (legittimo).
        var selectedByFilter = files.Length;
        // MiniTest con dipendenze ESTERNE (rete) o non deterministici per design non sono
        // confrontabili cross-leg: esclusi dal confronto (restano verificabili per-simbolo
        // sul gate sul driver C99Native, dove il runtime C è deterministico).
        // Es.: `AI.DISCOVER` esegue una richiesta HTTP reale (il mondo C# solleva, il C
        // ritorna lista vuota).
        // TORCH (2026-09-30): il crash nativo del processo host era dyld che risolveva
        // `@rpath/libtorch.dylib` verso un torch stale di una virtualenv (stub) invece del torch
        // corretto della dir ML. Fix: export DYLD_LIBRARY_PATH/GMB_TORCH_PATH PRIMA del lancio,
        // in `tools/test_minitests_parity` (testhost in-process) e in `RunVmc` (processo VM-C).
        // Ora `minitests/torch/` è INCLUSA nel cross-leg (parità piena), senza esclusioni.
        files = files.Where(f => !IsParityExcluded(Path.GetFileName(f))).ToArray();
        // INTERATTIVI (marker `' TAG: interactive`): esclusi SEMPRE di default; inclusi solo col param.
        var interactive = files.Where(IsInteractive).ToArray();
        if (!InteractiveEnabled())
            files = files.Where(f => !IsInteractive(f)).ToArray();
        if (interactive.Length > 0 && !InteractiveEnabled())
            Console.WriteLine($"SKIP {interactive.Length} MiniTest INTERATTIVI (marker 'TAG: interactive'); usa GMB_RUN_INTERACTIVE=1 per includerli.");
        // HEAVY (marker `' TAG: heavy`): richiedono modello reale + training; esclusi di default.
        var heavy = files.Where(IsHeavy).ToArray();
        if (!HeavyEnabled())
            files = files.Where(f => !IsHeavy(f)).ToArray();
        if (heavy.Length > 0 && !HeavyEnabled())
            Console.WriteLine($"SKIP {heavy.Length} MiniTest HEAVY (marker 'TAG: heavy'); usa GMB_RUN_HEAVY=1 per includerli.");
        if (files.Length == 0)
        {
            // Due casi MOLTO diversi, prima confusi in un unico "verde" (`Assert.True(true, ...)`),
            // che rendeva un nome file sbagliato indistinguibile da un'esclusione legittima:
            //  - il filtro non corrisponde a NULLA → filtro errato, deve FALLIRE;
            //  - i file selezionati esistono ma sono TUTTI esclusi per design (parità/rete,
            //    TAG interactive/heavy) → esito legittimo, si dichiara e si esce.
            if (!string.IsNullOrEmpty(only) && selectedByFilter == 0)
            {
                Assert.Fail($"GMB_MINI_ONLY=\"{only}\" non corrisponde ad alcun MiniTest in {dir}: "
                    + "nessun file selezionato (filtro errato, non un esito verde).");
            }
            if (!string.IsNullOrEmpty(only))
            {
                Assert.True(true, $"MiniTest selezionati ma esclusi dal confronto cross-leg (per design): {only}");
                return;
            }
            Assert.True(files.Length > 0, "nessun MiniTest");
        }
        // Backfill dell'oracolo: scrive in ogni MiniTest l'output atteso ricavato dal leg IL
        // IN-PROCESS (nessun processo gmb_vm_host). Uso:
        //   GMB_MINI_WRITE_EXPECT=1 GMB_MINI_ONLY=<dominio> dotnet test ...
        if (Environment.GetEnvironmentVariable("GMB_MINI_WRITE_EXPECT") == "1")
        {
            int written = 0, same = 0, skipped = 0;
            foreach (var f in files)
            {
                ResetAgents();
                string il;
                try { il = Norm(RunIl(f)).Replace("\n", "|"); }
                catch (Exception ex) { skipped++; Console.WriteLine($"SKIP {Path.GetFileName(f)} (IL {ex.GetType().Name})"); continue; }
                if (il.Length == 0) { skipped++; continue; }
                if (WriteExpect(f, il)) { written++; Console.WriteLine($"EXPECT {Path.GetFileName(f)} :: {il}"); }
                else same++;
            }
            Console.WriteLine($"EXPECT: scritti {written}, gia' corretti {same}, saltati {skipped}");
            Assert.True(true, "backfill oracolo completato");
            return;
        }

        var mismatches = new List<string>();
        foreach (var f in files)
        {
            // Ogni gamba produce un OUTPUT NORMALIZZATO: stdout se riesce, il MESSAGGIO
            // d'errore (normalizzato cross-formato) se lancia. Prima un'eccezione PCode
            // abortiva il file con "PCode eccezione" e le gambe non erano confrontabili:
            // per i `gen_*` con argomenti invalidi (il C# lancia ArgumentException, corretto)
            // la parità è proprio "stesso errore". Ora IL/PCode/VM-C sono confrontati anche
            // sul messaggio d'errore.
            ResetAgents();
            Basic.IlBackend.IlRuntime.FormReset();   /* isola lo stato del form tra i MiniTest (registry widget/bottoni) */
            var pcode = RunLegOutput(() => RunPCode(f));
            ResetAgents();
            Basic.IlBackend.IlRuntime.FormReset();
            var il = RunLegOutput(() => RunIl(f));
            if (!string.Equals(pcode, il, StringComparison.Ordinal))
                mismatches.Add($"{Path.GetFileName(f)}: output PCode != IL\n  PCode: {pcode}\n  IL:    {il}");
            ResetAgents();
            var vmc = RunLegOutput(() => RunVmc(f));
            if (!string.Equals(pcode, vmc, StringComparison.Ordinal))
                mismatches.Add($"{Path.GetFileName(f)}: output != VMC\n  PCode: {pcode}\n  IL:    {il}\n  VMC:   {vmc}");
            // Oracolo esplicito dichiarato dal MiniTest (se presente).
            CheckExpect(f, "PCode", pcode, mismatches);
            CheckExpect(f, "IL", il, mismatches);
            CheckExpect(f, "VMC", vmc, mismatches);
        }
        Assert.True(mismatches.Count == 0, "MiniTest parity falliti:\n" + string.Join("\n", mismatches));
    }

    /// <summary>Output normalizzato di una gamba: stdout (Norm) se riesce, altrimenti il
    /// messaggio d'errore normalizzato — così le gambe restano confrontabili anche
    /// quando la funzione lancia (parità = stesso errore).</summary>
    private static string RunLegOutput(Func<string> run)
    {
        try { return Norm(run()); }
        catch (Exception ex) { return NormalizeError(ex); }
    }

    /// <summary>Normalizza il messaggio d'errore delle gambe a un testo comune:
    /// le gambe C (PCode/VM-C) riportano "BASIC Runtime Error: <msg>", la gamba IL in-process
    /// l'eccezione .NET (<ArgumentException>.Message). Estrae <msg> in entrambi i casi.</summary>
    private static string NormalizeError(Exception ex)
    {
        var inner = ex;
        while (inner is AggregateException or TargetInvocationException && inner.InnerException is not null)
            inner = inner.InnerException;
        var m = (inner.Message ?? "").Replace("\r", "").Replace("\n", " | ");
        // ArgumentException con paramName: il .NET appende " (Parameter 'x')"; il C non lo ha
        // (vm_error riceve solo il testo) — rimosso per confrontare i messaggi core.
        m = System.Text.RegularExpressions.Regex.Replace(m, @"\s*\(Parameter '[^']*'\)", "");
        const string marker = "Runtime Error: ";
        var idx = m.IndexOf(marker, StringComparison.Ordinal);
        if (idx >= 0)
        {
            var s = m[(idx + marker.Length)..];
            var e = s.IndexOf(" | ", StringComparison.Ordinal);
            if (e >= 0) s = s[..e];
            return "ERROR: " + s.Trim();
        }
        // Gamba VMC esterna quando l'host esce != 0: l'harness avvolge path+exit+stderr
        // ("[VMC] <file> exit=N ... STDERR: [GMBASIC] ERROR <n>: <msg> (line <l>)").
        // Estrae <msg> così gli error-minitest con oracolo valgono anche per VMC
        // (2026-09-22, MiniTest-Oracolo §0.52: nessun EXPECT esistente usa la forma wrapper).
        const string stderrMarker = "STDERR:";
        var sidx = m.IndexOf(stderrMarker, StringComparison.Ordinal);
        if (sidx >= 0)
        {
            var s = m[(sidx + stderrMarker.Length)..].Trim();
            // Toglie i separatori di riga collassati ("|") e spazi di coda PRIMA di
            // cercare il suffix "(line N)" (che altrimenti non è a fine stringa).
            s = s.Trim().TrimEnd('|').Trim();
            var gm = System.Text.RegularExpressions.Regex.Match(s, @"^\[GMBASIC\] ERROR \d+: (.*)$");
            if (gm.Success) s = gm.Groups[1].Value.Trim();
            s = System.Text.RegularExpressions.Regex.Replace(s, @"\s*\(line \d+\)\s*$", "").Trim();
            var e2 = s.IndexOf(" | ", StringComparison.Ordinal);
            if (e2 >= 0) s = s[..e2].Trim();
            if (s.Length > 0) return "ERROR: " + s.Trim();
        }
        // Gamba PCode via harness xUnit (non CLI test_ilrun): avvolge diversamente, senza
        // marcatore "Runtime Error: " ("ERROR: [PCode] <path> fallito: | | [P-CODE ERROR] <msg>").
        const string pcodeMarker = "[P-CODE ERROR] ";
        var pidx = m.IndexOf(pcodeMarker, StringComparison.Ordinal);
        if (pidx >= 0)
        {
            var s = m[(pidx + pcodeMarker.Length)..];
            var e2 = s.IndexOf(" | ", StringComparison.Ordinal);
            if (e2 >= 0) s = s[..e2];
            return "ERROR: " + s.Trim();
        }
        // Suffix "(line N)" dei runtime C (stdout o stderr): mai parte del messaggio core.
        var lineSfx = System.Text.RegularExpressions.Regex.Replace(m, @"\s*\(line \d+\)\s*(\|\s*)?$", "");
        if (!ReferenceEquals(lineSfx, m)) m = lineSfx;
        return "ERROR: " + m.Trim();
    }

    /// <summary>Parità su un insieme di SAMPLE (non MiniTest): la stessa verifica del MiniTest
    /// parity (IL in-process, PCode, VM/C) applicata a una directory di sample, in UN solo
    /// processo (uno `dotnet test`), così lo sweep dei domini è rapido.
    ///
    /// Uso:
    ///   GMB_SAMPLE_DIR=samples/Databases [GMB_SAMPLE_ONLY=rdd] dotnet test --filter Sample_Parity_Sweep
    /// Esclude di default i sample non deterministici/che richiedono risorse esterne
    /// (browser, rete, postgres): vedi `IsExcluded`.</summary>
    [Fact]
    public void Sample_Parity_Sweep()
    {
        var dirEnv = Environment.GetEnvironmentVariable("GMB_SAMPLE_DIR");
        if (string.IsNullOrEmpty(dirEnv))
        {
            Assert.True(true, "GMB_SAMPLE_DIR non impostato: sweep non eseguito.");
            return;
        }
        var root = FindRepoRoot();
        var dir = Path.IsPathRooted(dirEnv) ? dirEnv : Path.Combine(root, dirEnv);
        Assert.True(Directory.Exists(dir), "directory sample non trovata: " + dir);
        var only = Environment.GetEnvironmentVariable("GMB_SAMPLE_ONLY");
        var files = Directory.GetFiles(dir, "*.bas", SearchOption.AllDirectories)
            .Where(f => string.IsNullOrEmpty(only) || f.Contains(only, StringComparison.OrdinalIgnoreCase))
            .Where(f => !IsExcluded(Path.GetFileName(f)))
            .OrderBy(f => f).ToArray();
        Assert.True(files.Length > 0, "nessun sample nella directory/filtro");

        var mismatches = new List<string>();
        var ok = new List<string>();
        foreach (var f in files)
        {
            var nome = Path.GetFileName(f);
            string pcode, il, vmc;
            try { ResetAgents(); pcode = Norm(RunPCode(f)); }
            // Messaggio INTERO: troncarlo alla prima riga nascondeva la causa (stesso fix del loop MiniTest).
            catch (Exception ex) { mismatches.Add($"{nome}: PCode {ex.GetType().Name}: {ex.Message.Replace("\n", " | ")}"); continue; }
            try { ResetAgents(); il = Norm(RunIl(f)); }
            // Messaggio INTERO: troncarlo alla prima riga nascondeva la causa (stesso fix del loop MiniTest).
            catch (Exception ex) { mismatches.Add($"{nome}: IL {ex.GetType().Name}: {ex.Message.Replace("\n", " | ")}"); continue; }
            try { vmc = Norm(RunVmc(f)); }
            // Messaggio INTERO: troncarlo alla prima riga nascondeva la causa (stesso fix del loop MiniTest).
            catch (Exception ex) { mismatches.Add($"{nome}: VMC {ex.GetType().Name}: {ex.Message.Replace("\n", " | ")}"); continue; }
            var diff = new List<string>();
            if (!string.Equals(pcode, il, StringComparison.Ordinal))
                diff.Add($"PCode != IL\n    PCode: {pcode.Replace("\n", "|")}\n    IL:    {il.Replace("\n", "|")}");
            if (!string.Equals(pcode, vmc, StringComparison.Ordinal))
                diff.Add($"PCode != VMC\n    PCode: {pcode.Replace("\n", "|")}\n    VMC:   {vmc.Replace("\n", "|")}");
            if (diff.Count == 0) ok.Add(nome); else mismatches.Add($"{nome}:\n    " + string.Join("\n    ", diff));
        }
        Console.WriteLine($"SAMPLE SWEEP: ok={ok.Count} fail={mismatches.Count} (dir={dirEnv})");
        foreach (var m in mismatches) Console.WriteLine("  " + m);
        Assert.True(mismatches.Count == 0, $"Sample parity sweep falliti ({mismatches.Count}):\n" + string.Join("\n", mismatches));
    }

    /// <summary>Sample esclusi dallo sweep: non deterministici, interattivi o con dipendenze
    /// esterne (browser/CDP, reti, database remoti). Ognuno di questi va verificato a mano.</summary>
    private static bool IsExcluded(string nome) =>
        nome.Contains("Postgres", StringComparison.OrdinalIgnoreCase)
        || nome.Contains("ReplicationHttp", StringComparison.OrdinalIgnoreCase)
        || nome.Contains("ReplicationTcp", StringComparison.OrdinalIgnoreCase)
        || nome.Contains("AiLoadDocs", StringComparison.OrdinalIgnoreCase)
        || nome.Contains("Vector", StringComparison.OrdinalIgnoreCase)
        || nome.Contains("bench_", StringComparison.OrdinalIgnoreCase)
        || nome.Contains("accum_check", StringComparison.OrdinalIgnoreCase)
        || nome.Contains("SelfConsistencyDemo", StringComparison.OrdinalIgnoreCase);

    /// <summary>MiniTest esclusi dal confronto cross-leg: dipendenze esterne (rete) o non
    /// deterministici per design. Restano verificabili per-simbolo sul gate sul
    /// driver C99Native (dove il runtime C è deterministico).</summary>
    private static bool IsParityExcluded(string nome) =>
        nome.Equals("gen_AI_Discover.bas", StringComparison.OrdinalIgnoreCase)
        // GATE dei NAMESPACE (2026-11-13): `constructs/ns_import_gate.bas` è un test NEGATIVO di
        // compilazione (`' EXPECT: ERROR: …`): il frontend C deve RIFIUTARE `ANGLE2D(...)` senza
        // `IMPORT VECTOR`. Il cross-leg managed compila in-process e si aspetta un run riuscito,
        // quindi il caso si verifica sulla gamba C (runner `tools/sh/test_minitests_c99native`);
        // le forme LECITE sono coperte da geom/dist2d, geom/dist3d, math/math3, math/math_all.
        || nome.Equals("ns_import_gate.bas", StringComparison.OrdinalIgnoreCase)
        // CLIPPER macro/EVAL/code-block (2026-10-02): batteria macro_core/eval_block/
        // eval_macro_string ora a parita' 4 backend (D1 EVAL dispatch, D2 text substitution,
        // D3 macro annidate, D4 corpo-espressione, D5 macro->block) — NON piu' escluse.
        // (macro_var_block/len_array_param: divergenze CHIUSE il 2026-10-02 — espansione
        // compile-time delle macro su variabile costante e LEN(array parametro) via bound
        // compagna `__b0` — non piu' escluse.)
        // ON KEY: FIX VM-C 2026-09-21 (gmb_vm_events: registry + dispatch alla lettura,
        // spawn task handler = parita' FireKeyEvent IL/PCode); il minitest ora passa
        // 3/3 con EXPECT. C99Native resta gap (kind=26 non emesso) → verificato lì.
        // (events_onkey non più escluso.)
        // ON SPRITE COLLISION: hitch PCode leg senza guardia (RunPCode senza timeout — il
        // park grafico su SYNC blocca il scheduler in-process); collisioni headless mai
        // calcolate (hits= 0 su tutte le gambe). Verifica per-gamba chirurgica.
        || nome.Equals("events_spritecollision.bas", StringComparison.OrdinalIgnoreCase)
        // BROWSER CDP (2026-09-22, blocco BROWSER §0.20/§0.48): su IL/PCode è attivo il
        // browser MOCK (about:blank, [] ...) mentre gli oracoli dichiarano l'errore CDP
        // esplicito dei backend C — divergenza di CONTRATTO, non gap: nessuna EXPECT unica
        // può servire entrambe le famiglie. Restano verificabili per-simbolo su VMC (stesso
        // runtime C), gate driver C99Native (5/5 C99Native verificato 2026-09-22
        // per Observe/Find/NetLog/Console/Auth; VMC Auth verde). Auth è inoltre env-state
        // (profili in $TMPDIR/gmb_browser_auth: oracolo valido solo a dir pulita).
        || (nome.StartsWith("gen_BROWSER_", StringComparison.OrdinalIgnoreCase)
            && !nome.Equals("gen_BROWSER_HumanScroll.bas", StringComparison.OrdinalIgnoreCase)
            && !nome.Equals("gen_BROWSER_Start.bas", StringComparison.OrdinalIgnoreCase))
        // ai_listcollections era escluso (PCode legacy syscall → ""); RISOLTO 2026-09-22
        // (delega a IlRuntime.AiVectorListCollections) → torna nel cross-leg.
        // ai_envreads: McpServerList/McpServerMarketplaceList/SkillsPrompt/MemoryRank leggono
        // STATO LOCALE (config/mcp.json, skills/, brain.db). C# e C lo rendono diversamente
        // (l'harness VMC forza GMBASIC_TEST_FIXTURES=1 -> config diversa) e il contenuto varia
        // per macchina: nessun oracolo unico possibile, non è una divergenza di codice.
        // Copertura census garantita; verifica per-gamba chirurgica.
        || nome.Equals("ai_envreads.bas", StringComparison.OrdinalIgnoreCase)
        // Grafica legacy: POLYGON/SPRITEDEF/IMAGEGET/IMAGEPUT/IMAGEPUTS APPENDONO l'harness
        // in-process (park grafico/modal su SYNC) — RunPCode/RunIl senza timeout del park
        // bloccherebbero il runner. Verificati per-gamba su CLI (exit 0). (2026-09-22,
        // MiniTest-Oracolo §0.53: copertura dei comandi senza-namespace.)
        || nome.Equals("gfx_parked_perleg.bas", StringComparison.OrdinalIgnoreCase)
        // Widget TUI senza-namespace: MODALI, appendono l'harness in-process (park su SYNC
        // senza timeout). Verificati via gate emitter C# (GMB_TUI_SCRIPT) dove PASSANO
        // (9 widget + fix gate 2026-09-22). C99Native non li emette ancora (gap noto).
        || nome.Equals("widgets_gate.bas", StringComparison.OrdinalIgnoreCase)
        // SPINNER/STATUSBAR/TBROWSE: disegnano il framebuffer su stdout (render) — non
        // confrontabili con EXPECT statico né nel cross-leg in-process (park modale).
        // Verificati per-gamba (emissione+compile+link su 4 backend; C99Native 2026-09-22).
        || nome.Equals("widgets_draw_perleg.bas", StringComparison.OrdinalIgnoreCase)
        // TRAY: il tray ha bisogno del PUMP sul main thread del host (la coda OP_CREATE/OP_ITEM
        // viene drenata da `TrayPump`). Il test host xUnit non ha un main loop: in-process
        // `TRAYSELFTEST()` resta 0 mentre sul VMC/C99 (processo con main thread) è -1 →
        // divergenza di HOST, non di linguaggio. Verificato sulle gambe con pump (VMC, C99).
        || nome.Equals("tray_parity.bas", StringComparison.OrdinalIgnoreCase)
        // gen_AI_*: campioni di COPERTURA SIMBOLO generati; le loro EXPECT catturano STATO di
        // macchina/sessione — `AI.BRAINUSED` = brain usati nel processo corrente,
        // `AI.PROVIDERS` = provider configurati, `AI.VAULTLIST` = vault creati dai test
        // precedenti nella stessa JVM, `AI.COUNT` = documenti indicizzati. Come gen_BROWSER_*:
        // la copertura del simbolo resta garantita dal census; l'oracolo unico non esiste.
        || nome.StartsWith("gen_AI_", StringComparison.OrdinalIgnoreCase)
        // Proprietà base + TEXTBOX + metodi CONTROL: GUI pixel (screen), solo
        // C99Native per ora; verificato per-gamba.
        || nome.Equals("control_methods_perleg.bas", StringComparison.OrdinalIgnoreCase);
        // DEFER: FIX 2026-09-21 (IL: linear-path support aggiunto; PCode: loop infinito
        // risolto — divergenza DEFER tra IL/PCode ed emitter C# chiusa). 4/4 backend
        // allineati, EXPECT esplicito. (statements_defer non più escluso.)
        // INTERFACE su UDT: FIX 2026-09-21 (root cause reale: bare-assignment return-slot
        // mai wired per i metodi di TYPE, non dispatch via interfaccia — bug
        // "UDT-Interface-Dispatch-Default-Su-Gambe-CSharp" chiuso su IL/PCode/VM-C). 3/5
        // allineati, EXPECT esplicito. emitter C# resta un gap separato, più profondo
        // (r.Area() non viene proprio emesso) — vedi "(open) -
        //-UDT-Method-Call-Non-Emesso.md". (types_interface non più escluso qui:
        // il confronto 3-vie IL/PCode/VM-C che questo file verifica è ora corretto; il gap
        // si verifica separatamente sul gate C99.)
}
