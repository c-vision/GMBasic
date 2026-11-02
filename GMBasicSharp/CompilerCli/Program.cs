// gmbcs — CLI headless dell'engine C# di GMBASIC.
//
// È il gemello di `gmb` (compilatore C) per il mondo managed: stessa superficie di comandi,
// stessi exit code, output su stdout senza banner/prompt (adatto a essere pilotato dalla REPL).
//
//   gmbcs check <file> [--json]
//   gmbcs dump ast    <file>
//   gmbcs dump pcode  <file> [--out f]
//   gmbcs dump il     <file> [--out f]
//   gmbcs dump disasm <file> [--out f]      (alias di pcode)
//   gmbcs build <file> [--target il|pcode] [--out dir]
//   gmbcs prginfo <file.prg>
//   gmbcs run  <file> [--engine il|pcode]
//   gmbcs eval "<istruzioni>" [--engine il|pcode]
//   gmbcs debug <file> [--break N]… [--step]   (debugger P-Code, comandi su stdin)
//   gmbcs aibench [RUN] [path]                 (AI Eval Harness: suite metriche)
//
// Opzioni comuni: -D name=val (ripetibile), --out <file>.
// Nessuna variabile d'ambiente per il comportamento.

using System.Reflection;
using System.Text;
using Basic;
using Basic.IlBackend;

namespace Basic.CliEngine;

/// <summary>CLI headless dell'engine C# (`gmbcs`). Pubblica `Main` anche per il CLI `gmb`.</summary>
public static class Program
{
    private const string Version = "gmbcs 1.0.0 (GMBASIC C# engine CLI)";

    public static int Main(string[] args)
    {
        try { return Run(args); }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"?gmbcs: {ex.Message}");
            return 1;
        }
    }

    private static int Run(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help" or "help") { Usage(); return args.Length == 0 ? 2 : 0; }
        if (args[0] is "-v" or "--version") { Console.WriteLine(Version); return 0; }

        var list = new List<string>(args);
        var defines = new Dictionary<string, string?>(StringComparer.OrdinalIgnoreCase);

        // -D name=val / -Dname=val
        for (var i = 0; i < list.Count; i++)
        {
            var a = list[i];
            string? raw = null;
            if (a == "-D" && i + 1 < list.Count) { raw = list[i + 1]; list.RemoveAt(i + 1); list.RemoveAt(i); i--; }
            else if (a.StartsWith("-D") && a.Length > 2) { raw = a[2..]; list.RemoveAt(i); i--; }
            if (raw == null) continue;
            var eq = raw.IndexOf('=');
            if (eq > 0) defines[raw[..eq]] = raw[(eq + 1)..];
            else if (raw.Length > 0) defines[raw] = "true";
        }

        var engine = TakeOption(list, "--engine") ?? "il";
        var outPath = TakeOption(list, "--out");
        var json = list.Remove("--json");
        var breaks = TakeAllInts(list, "--break");
        var step = list.Remove("--step");
        var ilModules = TakeAllStrings(list, "--il-module");

        /* --il-module <dll> (ripetibile): moduli IL esterni registrati PRIMA di ogni
         * compilazione, come faceva il vecchio REPL. */
        foreach (var mod in ilModules)
        {
            if (!File.Exists(mod)) { Console.Error.WriteLine($"?gmbcs: modulo IL non trovato: {mod}"); return 2; }
            try
            {
                var nmod = Basic.IlBackend.IlModuleLoader.Load(Path.GetFullPath(mod));
                Console.Error.WriteLine($"[IL-MODULE] caricato {Path.GetFileName(mod)}: {nmod} namespace.");
            }
            catch (Exception ex) { Console.Error.WriteLine($"?gmbcs: modulo IL {mod}: {ex.GetBaseException().Message}"); return 2; }
        }

        if (list.Count == 0) { Usage(); return 2; }
        var cmd = list[0].ToLowerInvariant();

        switch (cmd)
        {
            case "check":
            {
                var file = FileArg(list, 1);
                var program = CompileFile(file, defines);
                if (json) Console.WriteLine($"{{\"ok\":true,\"file\":{Json(file)}}}");
                else Console.WriteLine($"OK {file}");
                _ = program;
                return 0;
            }
            case "dump":
            {
                if (list.Count < 3) { Console.Error.WriteLine("?uso: gmbcs dump <ast|pcode|il|disasm> <file> [--out f]"); return 2; }
                var what = list[1].ToLowerInvariant();
                var file = list[2];
                var isPrg = file.EndsWith(".prg", StringComparison.OrdinalIgnoreCase);
                if (isPrg && what is "pcode" or "disasm")
                    return Emit(CapturePCodeFile(file), outPath);
                var program = CompileFile(file, defines);
                switch (what)
                {
                    case "ast":
                        return Emit(CaptureAst(program), outPath);
                    case "pcode":
                    case "disasm":
                        return Emit(file.EndsWith(".prg", StringComparison.OrdinalIgnoreCase)
                            ? CapturePCodeFile(file)
                            : CapturePCode(program, file), outPath);
                    case "il":
                        return Emit(CaptureIl(program, file), outPath);
                    default:
                        Console.Error.WriteLine($"?gmbcs dump: tipo sconosciuto '{what}'");
                        return 2;
                }
            }
            case "build":
            {
                /* Le opzioni si consumano PRIMA di leggere il file: i chiamanti possono
                 * scrivere sia `build <file> --target il` sia `build --target il <file>`. */
                var target = TakeOption(list, "--target") ?? "il";
                var full = list.Remove("--full") | list.Remove("-full");
                var all = list.Remove("--all");
                var file = FileArg(list, 1);
                var program = CompileFile(file, defines);
                return target.ToLowerInvariant() switch
                {
                    "pcode" => BuildPCode(program, file, outPath),
                    "il" => BuildIl(program, file, outPath, full, all),
                    "native" => BuildNative(program, file, outPath),
                    _ => throw new ArgumentException($"target sconosciuto: {target} (il|pcode|native)"),
                };
            }
            case "aibench":
            {
                /* AI Eval Harness (ex comando EVAL della REPL C#): suite metriche
                 * llm.tokps, rag.ndcg, team.acc, credit.blocked. Fixture deterministica
                 * quando nessun provider LLM è attivo (mai rete implicita). */
                if (list.Count > 1 && list[1].Equals("RUN", StringComparison.OrdinalIgnoreCase))
                    list.RemoveAt(1);
                var path = list.Count > 1 ? list[1] : null;
                var fixture = Basic.AI.Rag.LlmCompleterResolver.Default is null;
                Console.WriteLine($"AIBENCH RUN (fixture={fixture})...");
                var report = Basic.AI.Eval.EvalHarness.RunAll(path, fixture: fixture);
                using var doc = System.Text.Json.JsonDocument.Parse(report);
                var root = doc.RootElement;
                Console.WriteLine($"total: {root.GetProperty("total").GetProperty("passed").GetInt32()}/{root.GetProperty("total").GetProperty("cases").GetInt32()} pass");
                foreach (var suite in root.GetProperty("suites").EnumerateArray())
                {
                    var name = suite.GetProperty("Name").GetString();
                    var metrics = suite.GetProperty("Metrics").EnumerateArray()
                        .Select(m => $"{m.GetProperty("Key").GetString()}={m.GetProperty("Value")}")
                        .ToList();
                    Console.WriteLine($"  {name}: {string.Join("  ", metrics)}");
                }
                if (path != null) Console.WriteLine($"report → {path}");
                return 0;
            }
            case "prginfo":
            {
                var file = FileArg(list, 1);
                var chunk = global::Basic.Runtime.PrgFormat.Read(file);
                Console.WriteLine($"{Path.GetFileName(file)}: {chunk.Code.Length} opcodes, {chunk.Constants.Length} constants");
                return 0;
            }
            case "run":
            {
                var file = FileArg(list, 1);
                var program = CompileFile(file, defines);
                if (engine.Equals("native", StringComparison.OrdinalIgnoreCase))
                    return RunNative(program, file, outPath);
                Console.Write(RunProgram(program, file, engine));
                return 0;
            }
            case "debug":
            {
                var file = FileArg(list, 1);
                return RunDebug(file, defines, breaks, step, engine);
            }
            case "eval":
            {
                if (list.Count < 2) { Console.Error.WriteLine("?uso: gmbcs eval \"<istruzioni>\""); return 2; }
                var code = string.Join(' ', list.Skip(1));
                var program = CompileSnippet(code, defines);
                Console.Write(RunProgram(program, "<eval>", engine));
                return 0;
            }
            default:
                Console.Error.WriteLine($"?gmbcs: comando sconosciuto '{cmd}'");
                Usage();
                return 2;
        }
    }

    // ── compilazione ─────────────────────────────────────────────────────────────

    private static BasicProgram CompileFile(string file, IReadOnlyDictionary<string, string?> defines)
    {
        var full = Path.GetFullPath(file);
        if (!File.Exists(full)) throw new FileNotFoundException($"file non trovato: {file}");
        var dir = Path.GetDirectoryName(full)!;
        Directory.SetCurrentDirectory(dir);
        var program = Orchestrator.Compile(full, defines, false);
        if (program?.MainModule == null) throw new InvalidOperationException($"compilazione fallita: {file}");
        return program;
    }

    private static BasicProgram CompileSnippet(string code, IReadOnlyDictionary<string, string?> defines)
    {
        var src = "SUB Main()\n" + code + "\nEND SUB\n";
        var program = Orchestrator.CompileFromString(src, "eval.bas", defines, false);
        if (program?.MainModule == null) throw new InvalidOperationException("compilazione fallita: eval");
        return program;
    }

    // ── dump ─────────────────────────────────────────────────────────────────────

    private static string CaptureAst(BasicProgram program)
    {
        var sw = new StringWriter();
        var old = Console.Out;
        Console.SetOut(sw);
        try { Basic.Parser.DumpAstWithSymbolsAndScopes(program); }
        finally { Console.SetOut(old); }
        return sw.ToString();
    }

    private static string CapturePCodeFile(string prgPath)
    {
        var prg = global::Basic.Runtime.PrgFormat.Read(prgPath);
        var chunk = new global::Compiler.CompiledChunk();
        chunk.Code.AddRange(prg.Code);
        foreach (var c in prg.Constants) chunk.Constants.Add(c);
        foreach (var l in prg.Lines) chunk.Lines.Add(l);
        return global::Compiler.Disassembler.Disassemble(chunk, Path.GetFileName(prgPath));
    }

    /* NativeAOT: DLL managed intermedia (temp) + ilc via NativeBackend → singolo eseguibile
     * senza runtime .NET. Artefatti accanto al binario (globals.json per &/EVAL) e bundle
     * settings/Resources come il vecchio `-n`. */
    private static string CompileNativeInto(BasicProgram program, string sourcePath, string? outDir)
    {
        var dir = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(sourcePath))!, "bin", "native");
        var exe = IlProgramCompiler.CompileNative(program, sourcePath, dir);
        try { CommonLib.ProgramBundle.CopyNextTo(dir, sourcePath, Console.WriteLine); } catch { /* best effort */ }
        return exe;
    }

    private static int BuildNative(BasicProgram program, string sourcePath, string? outDir)
    {
        var exe = CompileNativeInto(program, sourcePath, outDir);
        Console.WriteLine($"[NATIVE] {sourcePath} -> {exe}");
        return 0;
    }

    private static int RunNative(BasicProgram program, string sourcePath, string? outDir)
    {
        var exe = CompileNativeInto(program, sourcePath, outDir);
        Console.WriteLine($"[NATIVE] {sourcePath} -> {exe}");
        var psi = new System.Diagnostics.ProcessStartInfo(exe)
        {
            UseShellExecute = false,
            WorkingDirectory = Path.GetDirectoryName(exe) ?? ".",
        };
        using var proc = System.Diagnostics.Process.Start(psi);
        if (proc == null) { Console.Error.WriteLine("?NATIVE: processo non avviato"); return 1; }
        proc.WaitForExit();
        return proc.ExitCode;
    }

    private static int BuildPCode(BasicProgram program, string sourcePath, string? outDir)
    {
        var chunk = new global::Compiler.Compiler().Compile(program);
        var dir = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(sourcePath))!, "bin");
        Directory.CreateDirectory(dir);
        var baseName = Path.GetFileNameWithoutExtension(sourcePath);
        var prgFile = Path.Combine(dir, baseName + ".prg");
        var prg = new global::Basic.Runtime.PrgChunk
        {
            Code = [.. chunk.Code],
            Constants = [.. chunk.Constants],
            Lines = [.. chunk.Lines],
            SourceName = Path.GetFileName(sourcePath),
        };
        global::Basic.Runtime.PrgFormat.Write(prgFile, prg);
        Console.WriteLine($"[P-Code] {sourcePath} -> {prgFile} ({chunk.Code.Count} opcodes)");
        return 0;
    }

    private static int BuildIl(BasicProgram program, string sourcePath, string? outDir, bool full = false, bool all = false)
    {
        var dir = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(sourcePath))!, "bin");
        var exe = IlProgramCompiler.Compile(program, sourcePath, dir, Path.GetFileNameWithoutExtension(sourcePath),
            noOptimize: false, dumpIl: false, linkRuntime: true, dllOnly: false, slim: !all);
        if (full) CopyNetRuntime(dir);
        Console.WriteLine($"[IL] {sourcePath} -> {exe}{(full ? " (full: runtime .NET copiato)" : "")}");
        return 0;
    }

    /* -full (dal vecchio runner): copia il framework .NET accanto all'eseguibile, così la
     * distribuzione managed non dipende dal runtime installato. */
    private static void CopyNetRuntime(string destDir)
    {
        var fxDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            ".dotnet", "shared", "Microsoft.NETCore.App");
        if (!Directory.Exists(fxDir))
        {
            fxDir = "/usr/local/share/dotnet/shared/Microsoft.NETCore.App";
            if (!Directory.Exists(fxDir)) return;
        }
        var latestVer = Directory.GetDirectories(fxDir).Select(Path.GetFileName).OrderDescending().FirstOrDefault();
        if (latestVer == null) return;
        var srcDir = Path.Combine(fxDir, latestVer);
        foreach (var f in Directory.GetFiles(srcDir, "*"))
        {
            var dest = Path.Combine(destDir, Path.GetFileName(f));
            if (!File.Exists(dest)) File.Copy(f, dest, true);
        }
    }

    private static string CapturePCode(BasicProgram program, string sourcePath)
    {
        var chunk = new global::Compiler.Compiler().Compile(program);
        return global::Compiler.Disassembler.Disassemble(chunk, Path.GetFileName(sourcePath));
    }

    private static string CaptureIl(BasicProgram program, string sourcePath)
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmbcs_" + Guid.NewGuid().ToString("N"));
        var baseName = Path.GetFileNameWithoutExtension(sourcePath);
        try
        {
            IlProgramCompiler.Compile(program, sourcePath, dir, baseName,
                noOptimize: false, dumpIl: true, linkRuntime: false, dllOnly: true);
            var ilPath = Path.Combine(dir, baseName + ".il");
            return File.Exists(ilPath) ? File.ReadAllText(ilPath) : "";
        }
        finally
        {
            try { Directory.Delete(dir, true); } catch { /* best effort */ }
        }
    }

    private static int Emit(string text, string? outPath)
    {
        if (string.IsNullOrEmpty(text)) { Console.Error.WriteLine("?gmbcs: dump vuoto"); return 1; }
        if (outPath != null)
        {
            File.WriteAllText(outPath, text);
            Console.WriteLine(outPath);
        }
        else Console.Write(text);
        return 0;
    }

    // ── debugger P-Code (unico backend C# con debugger) ─────────────────────────
    //
    // Protocollo su stdin/stdout (usato dalla REPL che lancia il processo con stdio
    // ereditato): a ogni pausa stampa `* stop line N` e legge un comando.
    //   c / continue · s / step (entra) · o / stepover · u / stepout
    //   b <linea> (breakpoint) · bt (stack) · p <espressione> · q / quit
    private static int RunDebug(string file, IReadOnlyDictionary<string, string?> defines,
                                List<int> breaks, bool step, string engine)
    {
        var program = CompileFile(file, defines);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var driver = new global::Basic.IlBackend.PCode.PCodeDebugDriver();
        try
        {
            driver.Start(chunk, breaks);
            if (step) driver.StepIn();

            var lastOut = "";
            while (driver.IsRunning || driver.IsPaused)
            {
                if (!driver.IsPaused) { Thread.Sleep(10); continue; }

                var outText = driver.TakeOutput();
                if (outText.Length > 0) { Console.Write(outText); lastOut += outText; }
                var line = driver.CurrentLine;
                var frames = driver.StackFrames;
                var ip = frames is { Count: > 0 } ? frames[0].Ip : -1;
                Console.WriteLine(line > 0 ? $"* stop line {line}" : $"* stop (ip {ip})");
                Console.Write("(gmbcs) ");

                var cmd = Console.ReadLine();
                if (cmd == null) { driver.Stop(); break; }
                cmd = cmd.Trim();
                if (cmd.Length == 0) cmd = "c";
                var parts = cmd.Split(' ', 2, StringSplitOptions.RemoveEmptyEntries);
                var op = parts[0].ToLowerInvariant();
                var arg = parts.Length > 1 ? parts[1] : "";

                switch (op)
                {
                    case "c": case "continue": case "cont": driver.Continue(); break;
                    case "s": case "step": driver.StepIn(); break;
                    case "o": case "over": case "stepover": driver.StepOver(); break;
                    case "u": case "out": case "stepout": driver.StepOut(); break;
                    case "b": case "break":
                        if (int.TryParse(arg, out var bl)) { driver.SetBreakpoint(bl); Console.WriteLine($"breakpoint line {bl}"); }
                        else Console.WriteLine("uso: b <linea>");
                        break;
                    case "bt": case "stack":
                        if (frames == null || frames.Count == 0) { Console.WriteLine("(stack vuoto)"); break; }
                        foreach (var f in frames) Console.WriteLine($"  #{f.Id} {f.Name} line {f.Line} ip {f.Ip}");
                        break;
                    case "p": case "print":
                        if (arg.Length == 0) { Console.WriteLine("uso: p <espressione>"); break; }
                        Console.WriteLine(driver.Evaluate(arg) ?? "(non valutabile)");
                        break;
                    case "q": case "quit": case "exit": driver.Stop(); break;
                    default: Console.WriteLine("comandi: c s o u b <linea> bt p <expr> q"); break;
                }
            }
            var tail = driver.TakeOutput();
            if (tail.Length > 0) Console.Write(tail);
            return 0;
        }
        finally { try { driver.Dispose(); } catch { /* best effort */ } }
    }

    // ── esecuzione in-process (stesse garanzie dei test di parità) ───────────────

    private static string RunProgram(BasicProgram program, string sourcePath, string engine)
    {
        var sw = new StringWriter();
        var old = Console.Out;
        var oldCwd = Directory.GetCurrentDirectory();
        try
        {
            if (sourcePath != "<eval>")
                Directory.SetCurrentDirectory(Path.GetDirectoryName(Path.GetFullPath(sourcePath))!);

            Console.SetOut(sw);
            if (string.Equals(engine, "pcode", StringComparison.OrdinalIgnoreCase)) RunPCode(program);
            else RunIl(program, sourcePath);
            return sw.ToString();
        }
        finally
        {
            Console.SetOut(old);
            Directory.SetCurrentDirectory(oldCwd);
            try { IlRuntime.SpeedCloseAll(); } catch { /* best effort */ }
        }
    }

    private static void RunPCode(BasicProgram program)
    {
        IlRuntime.SetMacroContext(program);
        var chunk = new global::Compiler.Compiler().Compile(program);
        var host = new Basic.Runtime.HeadlessRuntimeHost(Console.Out, new Basic.Runtime.RuntimeConfig());
        IlRuntime.SetCurrent(host);
        IlRuntime.SetCurrentForThread(host);
        /* prepareGraphicsHost=false: la CLI è headless, nessuna finestra SDL.
         * liveOut=Console.Out: l'output arriva mentre il programma gira (tee).
         * Il run avviene su un thread dedicato (stack grande, non main): la VM non entra
         * nel pump grafico SDL (stessa garanzia degli harness di test). */
        global::Basic.IlBackend.PCode.PCodeVmResult? result = null;
        Exception? err = null;
        var t = new Thread(() =>
        {
            try
            {
                result = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(
                    chunk, CancellationToken.None, prepareGraphicsHost: false, liveOut: Console.Out);
            }
            catch (Exception ex) { err = ex; }
        }, 64 * 1024 * 1024) { IsBackground = true };
        t.Start();
        t.Join();
        if (err != null) throw err;
        if (result == null || !result.Success)
            throw new InvalidOperationException("esecuzione P-Code fallita" + (result?.Output is { Length: > 0 } o ? ":\n" + o : ""));
    }

    private static void RunIl(BasicProgram program, string sourcePath)
    {
        IlRuntime.SetMacroContext(program);
        var name = "Gmbcs_" + Path.GetFileNameWithoutExtension(sourcePath) + "_" + Guid.NewGuid().ToString("N");
        var asm = IlProgramCompiler.CompileToAssembly(program, name, out _);
        var dll = Path.Combine(Path.GetTempPath(), name + ".dll");
        try
        {
            using (var fs = File.Create(dll)) asm.Save(fs);
            var assembly = Assembly.LoadFile(dll);
            var type = assembly.GetType("GeneratedProgram")
                       ?? throw new InvalidOperationException("entry point IL non trovato (GeneratedProgram)");
            var main = type.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
                       ?? throw new InvalidOperationException("Metodo Main non trovato");
            var host = new Basic.Runtime.HeadlessRuntimeHost(Console.Out, new Basic.Runtime.RuntimeConfig());
            IlRuntime.SetCurrent(host);
            IlRuntime.SetCurrentForThread(host);
            main.Invoke(null, null);
        }
        finally { try { File.Delete(dll); } catch { /* best effort */ } }
    }

    // ── utilità ──────────────────────────────────────────────────────────────────

    private static string FileArg(List<string> list, int index)
    {
        if (list.Count <= index) throw new ArgumentException("manca il file");
        return list[index];
    }

    private static List<string> TakeAllStrings(List<string> list, string name)
    {
        var v = new List<string>();
        for (var i = 0; i < list.Count; i++)
        {
            if (list[i] != name) continue;
            list.RemoveAt(i);
            if (i < list.Count) { v.Add(list[i]); list.RemoveAt(i); i--; }
            i--;
        }
        return v;
    }

    private static List<int> TakeAllInts(List<string> list, string name)
    {
        var v = new List<int>();
        for (var i = 0; i < list.Count; i++)
        {
            if (list[i] != name) continue;
            list.RemoveAt(i);
            if (i < list.Count && int.TryParse(list[i], out var n)) { v.Add(n); list.RemoveAt(i); i--; }
            i--;
        }
        return v;
    }

    private static string? TakeOption(List<string> list, string name)
    {
        var i = list.IndexOf(name);
        if (i < 0) return null;
        if (i + 1 >= list.Count) { list.RemoveAt(i); return null; }
        var v = list[i + 1];
        list.RemoveAt(i + 1);
        list.RemoveAt(i);
        return v;
    }

    private static string Json(string s)
    {
        var sb = new StringBuilder("\"");
        foreach (var c in s)
            sb.Append(c switch { '\\' => "\\\\", '"' => "\\\"", '\n' => "\\n", '\r' => "\\r", '\t' => "\\t", _ => c.ToString() });
        return sb.Append('"').ToString();
    }

    private static void Usage()
    {
        Console.Error.WriteLine("""
            gmbcs — CLI headless dell'engine C# di GMBASIC

            uso:
              gmbcs check <file> [--json]
              gmbcs dump ast    <file>
              gmbcs dump pcode  <file> [--out f]
              gmbcs dump il     <file> [--out f]
              gmbcs dump disasm <file> [--out f]
              gmbcs build <file> [--target il|pcode|native] [--out dir] [--full] [--all]
              gmbcs prginfo <file.prg>
              gmbcs run  <file> [--engine il|pcode|native]
              gmbcs eval "<istruzioni>" [--engine il|pcode]
              gmbcs debug <file> [--break N]… [--step]
              gmbcs aibench [RUN] [path]

            opzioni: -D name=val, --il-module <dll>, --out <file>, --json
            build: --full (copia il runtime .NET), --all (nessun filtro ClosureSlim)
            """);
    }
}
