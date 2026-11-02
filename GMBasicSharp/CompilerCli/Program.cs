// gmbcs — CLI headless dell'engine C# di GMBASIC.
//
// È il gemello di `gmb` (compilatore C) per il mondo managed: stessa superficie di comandi,
// stessi exit code, output su stdout senza banner/prompt (adatto a essere pilotato dalla REPL).
//
//   gmbcs check <file> [--json]        (oppure --project <dir> [--entry <file>])
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

    /* --quiet/-q: stessa semantica di `gmb` (sopprime il progresso, non gli errori).
     * Il progresso va su STDERR (stdout resta per i dati: --json), come nel CLI C. */
    /* realpath(3): il CLI C usa realpath() (es. /tmp → /private/tmp su macOS); con
     * Path.GetFullPath i due `--json` non sarebbero confrontabili. */
    [System.Runtime.InteropServices.DllImport("libc", SetLastError = true)]
    private static extern IntPtr realpath(string path, IntPtr resolved);
    [System.Runtime.InteropServices.DllImport("libc")]
    private static extern void free(IntPtr p);

    private static string RealPath(string path)
    {
        try
        {
            if (!OperatingSystem.IsWindows())
            {
                var r = realpath(path, IntPtr.Zero);
                if (r != IntPtr.Zero)
                {
                    var s = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(r);
                    free(r);
                    if (!string.IsNullOrEmpty(s)) return s;
                }
            }
        }
        catch { /* fallback */ }
        return Path.GetFullPath(path);
    }

    /* Schema UNICO di `--json` (identico al CLI C): command/project/entry/sources/modules/
     * [exe/library]/ok/diagnostics. */
    private static void EmitJson(string command, string file, string? project, BasicProgram? program,
                                 string? exe = null, bool library = false)
    {
        var entry = RealPath(file);
        var proj = project != null ? RealPath(project) : (Path.GetDirectoryName(entry) ?? ".");
        var modules = program?.ResolvedModuleSourcePaths.Count ?? 0;
        var sources = CountProjectBas(proj);
        Console.WriteLine("{");
        Console.WriteLine($"  \"command\": {Json(command)},");
        Console.WriteLine($"  \"project\": {Json(proj)},");
        Console.WriteLine($"  \"entry\": {Json(entry)},");
        Console.WriteLine($"  \"sources\": {sources},");
        Console.WriteLine($"  \"modules\": {modules},");
        if (exe != null)
        {
            Console.WriteLine($"  \"exe\": {Json(RealPath(exe))},");
            Console.WriteLine($"  \"library\": {(library ? "true" : "false")},");
        }
        Console.WriteLine("  \"ok\": true,");
        Console.WriteLine("  \"diagnostics\": [");
        Console.WriteLine("  ]");
        Console.WriteLine("}");
    }

    /* Conta i sorgenti `.bas` del progetto (stessa semantica di `gmb_project_sources`:
     * ricorsivo, esclusi bin/obj/work/.git). Prima `sources` era modules+1 e in un progetto
     * multi-file non coincideva col CLI C. */
    private static int CountProjectBas(string dir)
    {
        try
        {
            if (!Directory.Exists(dir)) return 1;
            int n = 0;
            foreach (var f in Directory.EnumerateFiles(dir, "*.bas", SearchOption.AllDirectories))
            {
                var rel = Path.GetRelativePath(dir, f).Replace('\\', '/');
                if (rel.StartsWith("bin/") || rel.Contains("/bin/")
                    || rel.StartsWith("obj/") || rel.Contains("/obj/")
                    || rel.StartsWith("work/") || rel.Contains("/work/")
                    || rel.Contains("/.git/") || rel.StartsWith(".git/")) continue;
                n++;
            }
            return n > 0 ? n : 1;
        }
        catch { return 1; }
    }

    private static string BuildExePath(string file, string? outDir, string target)
    {
        var baseName = Path.GetFileNameWithoutExtension(file);
        if (target.Equals("pcode", StringComparison.OrdinalIgnoreCase))
        {
            var d = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin");
            return Path.Combine(d, baseName + ".prg");
        }
        if (target.Equals("native", StringComparison.OrdinalIgnoreCase))
        {
            var d = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin", "native");
            return Path.Combine(d, OperatingSystem.IsWindows() ? baseName + ".exe" : baseName);
        }
        {
            var d = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(file))!, "bin");
            return Path.Combine(d, OperatingSystem.IsWindows() ? baseName + ".exe" : baseName);
        }
    }

    private static bool Quiet;
    /* Contesto per `--json` in caso di ERRORE (il CLI C emette comunque il JSON con
     * ok:false + diagnostics: unico formato per tool/IDE, identico fra C e C#). */
    private static bool JsonMode;
    private static string JsonCmd = "";
    private static string? JsonFile, JsonProj, JsonOut, JsonTarget;

    private static bool IsParseError(Exception ex)
    {
        var m = ex.Message;
        return m.Contains("Expected", StringComparison.OrdinalIgnoreCase)
            || m.Contains("parse", StringComparison.OrdinalIgnoreCase)
            || m.Contains("attesa", StringComparison.OrdinalIgnoreCase)
            || m.Contains("Unexpected", StringComparison.OrdinalIgnoreCase);
    }

    private static int JsonRc(Exception ex)
    {
        var m = ex.Message;
        if (IsParseError(ex)) return 3;   /* come il CLI C per build/run */
        if (m.Contains("Expected", StringComparison.OrdinalIgnoreCase)
            || m.Contains("parse", StringComparison.OrdinalIgnoreCase)
            || m.Contains("attesa", StringComparison.OrdinalIgnoreCase)
            || m.Contains("Unexpected", StringComparison.OrdinalIgnoreCase)) return 3;
        if (ex is FileNotFoundException) return 2;
        return 2;
    }

    private static void EmitJsonError(string message)
    {
        var file = JsonFile ?? "";
        var entry = file.Length > 0 ? RealPath(file) : "";
        var proj = JsonProj != null ? RealPath(JsonProj)
                 : (entry.Length > 0 ? (Path.GetDirectoryName(entry) ?? ".") : ".");
        Console.WriteLine("{");
        Console.WriteLine($"  \"command\": {Json(JsonCmd)},");
        Console.WriteLine($"  \"project\": {Json(proj)},");
        Console.WriteLine($"  \"entry\": {(entry.Length > 0 ? Json(entry) : "null")},");
        Console.WriteLine($"  \"sources\": {(JsonCmd == "check" ? CountProjectBas(proj) : 0)},");
        Console.WriteLine("  \"modules\": 0,");
        var code = IsParseError(new InvalidOperationException(message)) ? "parse" : "compile";
        if (JsonCmd is "build" or "run" && JsonTarget != null)
        {
            Console.WriteLine($"  \"exe\": {Json(RealPath(BuildExePath(file, JsonOut, JsonTarget)))},");
            Console.WriteLine("  \"library\": false,");
        }
        Console.WriteLine("  \"ok\": false,");
        Console.WriteLine("  \"diagnostics\": [");
        Console.WriteLine($"    {{\"severity\": \"error\", \"code\": \"{code}\", \"file\": {Json(entry)}, \"line\": 0, \"message\": {Json(message)}}}");
        Console.WriteLine("  ]");
        Console.WriteLine("}");
    }
    private static void Info(string s) { if (!Quiet) Console.Error.WriteLine(s); }
    private static T Quietly<T>(Func<T> f)
    {
        if (!Quiet) return f();
        var err = Console.Error;
        Console.SetError(TextWriter.Null);
        try { return f(); } finally { Console.SetError(err); }
    }

    public static int Main(string[] args)
    {
        try { return Run(args); }
        catch (Exception ex)
        {
            if (JsonMode)
            {
                EmitJsonError(ex.Message);
                return JsonCmd == "check" ? 2 : JsonRc(ex);
            }
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
            if ((a == "-D" || a == "--define") && i + 1 < list.Count)
            { raw = list[i + 1]; list.RemoveAt(i + 1); list.RemoveAt(i); i--; }
            else if (a.StartsWith("-D") && a.Length > 2) { raw = a[2..]; list.RemoveAt(i); i--; }
            else if (a.StartsWith("--define=", StringComparison.Ordinal) && a.Length > 9)
            { raw = a[9..]; list.RemoveAt(i); i--; }
            if (raw == null) continue;
            var eq = raw.IndexOf('=');
            if (eq > 0) defines[raw[..eq]] = raw[(eq + 1)..];
            else if (raw.Length > 0) defines[raw] = "true";
        }

        if (list.Remove("--quiet") | list.Remove("-q")) Quiet = true;
        /* --strict: il linker C# non produce "avvisi"; accettato per parità col CLI C. */
        list.Remove("--strict");

        var engine = TakeOption(list, "--engine") ?? "il";
        var project = TakeOption(list, "--project");
        var entry = TakeOption(list, "--entry");
        var outPath = TakeOption(list, "--out");
        /* ASSOLUTI subito: `CompileFile` fa `Directory.SetCurrentDirectory(dir)`, quindi un
         * path relativo usato dopo (bundle, sourcePath, --out) verrebbe risolto nella cartella
         * del sorgente. Il CLI C li risolve rispetto alla CWD di invocazione. */
        if (project != null) project = Path.GetFullPath(project);
        if (entry != null) entry = Path.GetFullPath(entry);
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
                Info($"[IL-MODULE] caricato {Path.GetFileName(mod)}: {nmod} namespace.");
            }
            catch (Exception ex) { Console.Error.WriteLine($"?gmbcs: modulo IL {mod}: {ex.GetBaseException().Message}"); return 2; }
        }

        if (engine.Equals("c", StringComparison.OrdinalIgnoreCase))
        {
            Console.Error.WriteLine("?gmbcs è l'engine C#: per l'engine C usa `gmb --engine=c` (CLI nativo).");
            return 2;
        }

        if (list.Count == 0) { Usage(); return 2; }
        var cmd = list[0].ToLowerInvariant();
        /* Opzioni sconosciute = ERRORE (prima venivano ignorate in silenzio: `--pippo`
         * passava). `eval` è l'eccezione: il resto sono istruzioni, anche con `-`. */
        {
            string[] known = ["--target", "--full", "-full", "--all", "--libs", "-libs",
                              "--alllibs", "-alllib", "--all-libs", "--plugins", "-plugins",
                              "--skills", "-skills", "--brains", "-brains", "--break", "--step",
                              "--il-module", "--help", "-h"];
            if (cmd != "eval")
                foreach (var a in list.Skip(1))
                {
                    if (a.Length < 2 || a[0] != '-') continue;
                    if (known.Any(k => a == k || a.StartsWith(k + "=", StringComparison.Ordinal))) continue;
                    Console.Error.WriteLine($"?gmbcs: opzione sconosciuta '{a}'");
                    return 2;
                }
        }

        switch (cmd)
        {
            case "check":
            {
                var file = Path.GetFullPath(ResolveEntry(list, project, entry, 1));
                JsonMode = json; JsonCmd = "check"; JsonFile = file; JsonProj = project; JsonTarget = null;
                var program = CompileFile(file, defines);
                /* STESSO schema del CLI C (`gmb check --json`). */
                if (json) EmitJson("check", file, project, program);
                else if (!Quiet) Console.WriteLine($"OK {file}");
                return 0;
            }
            case "dump":
            {
                if (list.Count < 2) { Console.Error.WriteLine("?uso: gmbcs dump <ast|pcode|il|disasm> [<file>|--project D [--entry F]] [--out f]"); return 2; }
                var what = list[1].ToLowerInvariant();
                var file = ResolveEntry(list, project, entry, 2);
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
                /* Bundle di distribuzione — STESSO contratto del CLI C:
                 *   --libs[=used|all|runtime|dev|ffi|llmmotor|torch|ml|chromium|obscura|ocr|nomi]
                 *   --plugins[=all|nomi] --skills[=all|nomi] --brains[=all|nomi]
                 * Alias storici di --libs: -alllib, --alllibs, --all-libs; --full lo implica. */
                var libs = TakeBundle(list, "--libs", "-libs", "--alllibs", "-alllib", "--all-libs");
                var libsOn = libs.On || full;
                var plugins = TakeBundle(list, "--plugins", "-plugins");
                var skills = TakeBundle(list, "--skills", "-skills");
                var brains = TakeBundle(list, "--brains", "-brains");
                var file = Path.GetFullPath(ResolveEntry(list, project, entry, 1));
                JsonMode = json; JsonCmd = "build"; JsonFile = file; JsonProj = project;
                JsonOut = outPath; JsonTarget = target;
                var program = CompileFile(file, defines);
                var brc = target.ToLowerInvariant() switch
                {
                    "pcode" => BuildPCode(program, file, outPath),
                    "il" => BuildIl(program, file, outPath, full, all, libsOn, libs.Sel, plugins, skills, brains),
                    "native" => BuildNative(program, file, outPath, libsOn, libs.Sel, plugins, skills, brains),
                    _ => throw new ArgumentException($"target sconosciuto: {target} (il|pcode|native)"),
                };
                if (brc == 0 && json) EmitJson("build", file, project, program, BuildExePath(file, outPath, target));
                return brc;
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
                var file = Path.GetFullPath(ResolveEntry(list, project, entry, 1));
                var chunk = global::Basic.Runtime.PrgFormat.Read(file);
                Console.WriteLine($"{Path.GetFileName(file)}: {chunk.Code.Length} opcodes, {chunk.Constants.Length} constants");
                return 0;
            }
            case "run":
            {
                var full = list.Remove("--full") | list.Remove("-full");
                var libs = TakeBundle(list, "--libs", "-libs", "--alllibs", "-alllib", "--all-libs");
                var libsOn = libs.On || full;
                var plugins = TakeBundle(list, "--plugins", "-plugins");
                var skills = TakeBundle(list, "--skills", "-skills");
                var brains = TakeBundle(list, "--brains", "-brains");
                var file = Path.GetFullPath(ResolveEntry(list, project, entry, 1));
                JsonMode = json; JsonCmd = "run"; JsonFile = file; JsonProj = project;
                JsonOut = outPath; JsonTarget = engine.Equals("native", StringComparison.OrdinalIgnoreCase) ? "native" : null;
                var program = CompileFile(file, defines);
                if (engine.Equals("native", StringComparison.OrdinalIgnoreCase))
                {
                    var nrc = RunNative(program, file, outPath, libsOn, libs.Sel, plugins, skills, brains);
                    if (nrc == 0 && json) EmitJson("run", file, project, program, BuildExePath(file, outPath, "native"));
                    return nrc;
                }
                if (libsOn || plugins.On || skills.On || brains.On)
                    Info("[bundle] ignorato con target gestito 'il' in run: usa `gmbcs build --libs …`");
                /* Con --json l'artefatto viene MATERIALIZZATO in bin/ (come fa il CLI C) e il
                 * JSON riporta l'exe che esiste davvero; l'esecuzione resta in-process. */
                string? builtExe = null;
                if (json)
                {
                    var brc2 = BuildIl(program, file, outPath, false, false, false);
                    if (brc2 != 0) return brc2;
                    builtExe = BuildExePath(file, outPath, "il");
                }
                Console.Write(RunProgram(program, file, engine));
                if (json) EmitJson("run", file, project, program, builtExe);
                return 0;
            }
            case "debug":
            {
                var file = Path.GetFullPath(ResolveEntry(list, project, entry, 1));
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
        var program = Quietly(() => Orchestrator.Compile(full, defines, false));
        if (program?.MainModule == null) throw new InvalidOperationException($"compilazione fallita: {file}");
        return program;
    }

    private static BasicProgram CompileSnippet(string code, IReadOnlyDictionary<string, string?> defines)
    {
        var src = "SUB Main()\n" + code + "\nEND SUB\n";
        var program = Quietly(() => Orchestrator.CompileFromString(src, "eval.bas", defines, false));
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
    private static string CompileNativeInto(BasicProgram program, string sourcePath, string? outDir, bool allLibs = false,
                                            string? libsSel = null, BundleSel plugins = default,
                                            BundleSel skills = default, BundleSel brains = default)
    {
        var dir = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(sourcePath))!, "bin", "native");
        var exe = Quietly(() => IlProgramCompiler.CompileNative(program, sourcePath, dir));
        try { CommonLib.ProgramBundle.CopyNextTo(dir, sourcePath, Console.WriteLine); } catch { /* best effort */ }
        if (allLibs || plugins.On || skills.On || brains.On)
            DeployBundle(dir, allLibs ? (libsSel ?? "used") : null,
                         plugins.On, plugins.Sel, skills.On, skills.Sel, brains.On, brains.Sel, sourcePath);
        return exe;
    }

    private static int BuildNative(BasicProgram program, string sourcePath, string? outDir, bool allLibs = false,
                                   string? libsSel = null, BundleSel plugins = default,
                                   BundleSel skills = default, BundleSel brains = default)
    {
        var exe = CompileNativeInto(program, sourcePath, outDir, allLibs, libsSel, plugins, skills, brains);
        Info($"[NATIVE] {sourcePath} -> {exe}");
        return 0;
    }

    private static int RunNative(BasicProgram program, string sourcePath, string? outDir, bool allLibs = false,
                                 string? libsSel = null, BundleSel plugins = default,
                                 BundleSel skills = default, BundleSel brains = default)
    {
        var exe = CompileNativeInto(program, sourcePath, outDir, allLibs, libsSel, plugins, skills, brains);
        Info($"[NATIVE] {sourcePath} -> {exe}");
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
        Info($"[P-Code] {sourcePath} -> {prgFile} ({chunk.Code.Count} opcodes)");
        return 0;
    }

    /* ── bundle di distribuzione: --libs / --plugins / --skills / --brains ─────────────
     * Stesso contratto del CLI C (`gmb`): selettore opzionale con `=`, alias storici.
     * Le sottocartelle nella destinazione sono libs/<rid>, plugins/, skills/, brains/. */
    private readonly record struct BundleSel(bool On, string? Sel);   /* struct: `default` è valido (On=false) */

    private static BundleSel TakeBundle(List<string> list, params string[] names)
    {
        foreach (var n in names)
            for (int i = 0; i < list.Count; i++)
            {
                var a = list[i];
                if (a == n) { list.RemoveAt(i); return new(true, null); }
                if (a.StartsWith(n + "=", StringComparison.Ordinal))
                {
                    var v = a[(n.Length + 1)..];
                    list.RemoveAt(i);
                    return new(true, v);
                }
            }
        return new(false, null);
    }

    private static bool SelAll(string? sel) => string.IsNullOrWhiteSpace(sel) || sel.Equals("all", StringComparison.OrdinalIgnoreCase);

    private static bool SelHas(string? sel, string name)
    {
        if (string.IsNullOrWhiteSpace(sel)) return false;
        foreach (var t in sel.Split(new[] { ',', ' ' }, StringSplitOptions.RemoveEmptyEntries))
            if (t.Equals(name, StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    private static void CopyTree(string src, string dst, int[] n)
    {
        if (!Directory.Exists(src)) return;
        Directory.CreateDirectory(dst);
        foreach (var f in Directory.EnumerateFiles(src))
        {
            try { File.Copy(f, Path.Combine(dst, Path.GetFileName(f)), overwrite: true); n[0]++; }
            catch { /* file in uso: gli altri si copiano */ }
        }
        foreach (var d in Directory.EnumerateDirectories(src))
            CopyTree(d, Path.Combine(dst, Path.GetFileName(d)), n);
    }

    private static string RepoSharedDir(string name)
    {
        // <repo>/shared/<name> a partire dalla cartella dell'eseguibile della toolchain.
        var dir = AppContext.BaseDirectory;
        for (int up = 0; up < 8 && dir != null; up++)
        {
            var cand = Path.Combine(dir, "shared", name);
            if (Directory.Exists(cand)) return cand;
            dir = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar));
        }
        return "";
    }

    /* Copia le librerie native secondo il selettore:
     *   used (default) → il set che il runtime IL carica per nome;
     *   all            → tutte le lib del RID dalla cache;
     *   gruppo         → le voci dell'indice (`index.txt`, 5ª colonna) di quel gruppo;
     *   ml|chromium|obscura|ocr → la sottocartella componente;
     *   nomi,...       → i file richiesti. */
    private static void DeployLibs(string dir, string? sel, string? sourcePath = null)
    {
        var rid = CommonLib.LibsResolver.Rid();
        var cache = CommonLib.LibsResolver.CacheDir(rid);
        var dst = Path.Combine(dir, "libs", rid);
        Directory.CreateDirectory(dst);
        var n = new[] { 0 };
        string[] exts = [".dylib", ".so", ".dll", ".metallib"];

        void CopyFrom(string srcDir, string name)
        {
            foreach (var e in exts)
            {
                var p = Path.Combine(srcDir, name + e);
                if (File.Exists(p)) { File.Copy(p, Path.Combine(dst, Path.GetFileName(p)), true); n[0]++; return; }
                p = Path.Combine(srcDir, "lib" + name + e);
                if (File.Exists(p)) { File.Copy(p, Path.Combine(dst, Path.GetFileName(p)), true); n[0]++; return; }
            }
            // libfoo.1.dylib ecc.
            if (!Directory.Exists(srcDir)) return;
            foreach (var f in Directory.EnumerateFiles(srcDir))
            {
                var baseName = Path.GetFileName(f);
                var cut = baseName.IndexOf(".dylib", StringComparison.Ordinal);
                if (cut < 0) cut = baseName.IndexOf(".so", StringComparison.Ordinal);
                if (cut <= 0) continue;
                var stem = baseName[..cut];
                if (stem.StartsWith("lib", StringComparison.Ordinal)) stem = stem[3..];
                if (stem.Equals(name, StringComparison.OrdinalIgnoreCase))
                {
                    File.Copy(f, Path.Combine(dst, baseName), true); n[0]++; return;
                }
            }
        }

        if (string.IsNullOrWhiteSpace(sel) || sel.Equals("used", StringComparison.OrdinalIgnoreCase))
        {
            // Set che il runtime gestito carica per nome (P/Invoke o dlopen): i moduli nostri.
            string[] used = ["gmb_gfx", "gmb_ai_llm", "gmb_ai_modelio", "gmb_pkg", "gmb_bsp",
                             "gmb_tray", "gmb_runtime_ai", "gmb_sql_csv_ext", "gmb_sql_parquet_ext",
                             "llmtoken", "e_sqlite3"];
            foreach (var u in used) CopyFrom(cache, u);
        }
        else if (SelAll(sel))
        {
            foreach (var f in Directory.EnumerateFiles(cache))
            {
                var name = Path.GetFileName(f);
                if (!exts.Any(e => name.EndsWith(e, StringComparison.OrdinalIgnoreCase))) continue;
                try { File.Copy(f, Path.Combine(dst, name), true); n[0]++; } catch { }
            }
        }
        else
        {
            // gruppo dall'indice?
            string[] groups = ["runtime", "dev", "ffi", "llmmotor", "torch"];
            var grp = groups.FirstOrDefault(g => g.Equals(sel, StringComparison.OrdinalIgnoreCase));
            if (grp != null)
            {
                var idx = Path.Combine(cache, "index.txt");
                if (File.Exists(idx))
                    foreach (var line in File.ReadLines(idx))
                    {
                        var cols = line.Split('\t');
                        if (cols.Length < 5 || !cols[4].Equals(grp, StringComparison.OrdinalIgnoreCase)) continue;
                        var src = Path.Combine(cache, cols[0]);
                        if (!File.Exists(src)) continue;
                        var target = Path.Combine(dst, cols[0]);
                        try
                        {
                            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                            File.Copy(src, target, true);
                            n[0]++;
                        }
                        catch { }
                    }
            }
            // componente?
            else if (new[] { "ml", "chromium", "obscura", "ocr" }.FirstOrDefault(c => c.Equals(sel, StringComparison.OrdinalIgnoreCase)) is { } comp)
            {
                var libsRoot = Path.GetDirectoryName(cache)!;
                var src = Path.Combine(libsRoot, comp, rid);
                var target = Path.Combine(dir, "libs", comp, rid);
                CopyTree(src, target, n);
                Info($"[libs] componente {comp}: {n[0]} file -> {target}");
                return;
            }
            // elenco di nomi
            else
            {
                foreach (var t in sel.Split(new[] { ',', ' ' }, StringSplitOptions.RemoveEmptyEntries))
                    CopyFrom(cache, t);
            }
        }
        // FFI (2026-11-02): le lib aperte con dlopen non compaiono in nessuna chiusura —
        // si raccolgono dai `DECLARE … LIB "nome"` del sorgente e si copiano nel bundle.
        if (!string.IsNullOrWhiteSpace(sourcePath) && File.Exists(sourcePath))
        {
            var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var srcDirFfi = Path.GetDirectoryName(Path.GetFullPath(sourcePath)) ?? ".";
            /* I DECLARE possono stare anche in un MODULO (`PUBLIC DECLARE ...`) o in un .inc:
             * si leggono tutti i .bas/.inc del progetto (bin/obj/work esclusi). */
            var srcFiles = new List<string> { sourcePath };
            try
            {
                foreach (var f in Directory.EnumerateFiles(srcDirFfi, "*.*", SearchOption.AllDirectories))
                {
                    var ext = Path.GetExtension(f).ToLowerInvariant();
                    if (ext != ".bas" && ext != ".inc") continue;
                    var rel = Path.GetRelativePath(srcDirFfi, f).Replace('\\', '/');
                    if (rel.StartsWith("bin/") || rel.Contains("/bin/") || rel.StartsWith("obj/")
                        || rel.Contains("/obj/") || rel.StartsWith("work/") || rel.Contains("/work/")
                        || rel.Contains("/.git/") || rel.StartsWith(".git/")) continue;
                    srcFiles.Add(f);
                }
            }
            catch { /* best effort */ }
            foreach (var srcFile in srcFiles)
            foreach (var line in File.ReadLines(srcFile))
            {
                for (var i = 0; i + 3 <= line.Length; i++)
                {
                    if (!line.AsSpan(i, 3).Equals("LIB", StringComparison.OrdinalIgnoreCase)) continue;
                    var j = i + 3;
                    while (j < line.Length && char.IsWhiteSpace(line[j])) j++;
                    if (j >= line.Length || (line[j] != '"' && line[j] != '\'')) continue;
                    var q = line[j];
                    var k = line.IndexOf(q, j + 1);
                    if (k < 0) break;
                    var nm = line[(j + 1)..k].Trim();
                    if (nm.Length > 0) names.Add(nm);
                    i = k;
                }
            }
            var srcDir = Path.GetDirectoryName(Path.GetFullPath(sourcePath)) ?? ".";
            var dirsF = new List<string>
            {
                srcDir, Path.Combine(srcDir, "libs"),
                Directory.GetCurrentDirectory(), Path.Combine(Directory.GetCurrentDirectory(), "libs")
            };
            var up = srcDir;
            for (var u = 0; u < 5; u++)
            {
                var p = Path.GetDirectoryName(up);
                if (string.IsNullOrEmpty(p)) break;
                up = p;
                dirsF.Add(Path.Combine(up, "libs"));
            }
            dirsF.Add(cache);
            foreach (var nm in names)
            {
                var before = n[0];
                foreach (var d in dirsF) { CopyFrom(d, nm); if (n[0] > before) break; }
                if (n[0] == before)
                    Console.Error.WriteLine($"[libs] FFI '{nm}' non trovata (non copiata nel bundle)");
            }
        }
        if (n[0] == 0 && !string.IsNullOrWhiteSpace(sel))
            Console.Error.WriteLine($"[libs] selettore '{sel}': nessun file trovato nella cache");
        Info($"[libs] {n[0]} file -> {dst}");
    }

    private static void DeployEntries(string srcRoot, string dstRoot, string? sel, string label)
    {
        if (string.IsNullOrEmpty(srcRoot) || !Directory.Exists(srcRoot)) return;
        Directory.CreateDirectory(dstRoot);
        var n = new[] { 0 };
        foreach (var d in Directory.EnumerateDirectories(srcRoot))
        {
            var name = Path.GetFileName(d);
            if (name.StartsWith(".", StringComparison.Ordinal)) continue;
            if (!SelAll(sel) && !SelHas(sel, name)) continue;
            CopyTree(d, Path.Combine(dstRoot, name), n);
        }
        if (n[0] > 0) Info($"[{label}] {n[0]} file -> {dstRoot}");
    }

    private static void DeployBundle(string dir, string? libsSel, bool plugins, string? pluginsSel,
                                     bool skills, string? skillsSel, bool brains, string? brainsSel,
                                     string? sourcePath = null)
    {
        if (libsSel != null) DeployLibs(dir, libsSel, sourcePath);
        if (plugins) DeployEntries(RepoSharedDir("plugins"), Path.Combine(dir, "plugins"), pluginsSel, "plugins");
        if (skills) DeployEntries(RepoSharedDir("skills"), Path.Combine(dir, "skills"), skillsSel, "skills");
        if (brains)
        {
            var brainsCache = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".gmbasic", "brains");
            DeployEntries(brainsCache, Path.Combine(dir, "brains"), brainsSel, "brains");
        }
    }

    private static int BuildIl(BasicProgram program, string sourcePath, string? outDir, bool full = false, bool all = false,
                               bool allLibs = false, string? libsSel = null,
                               BundleSel plugins = default, BundleSel skills = default, BundleSel brains = default)
    {
        var dir = outDir != null ? Path.GetFullPath(outDir) : Path.Combine(Path.GetDirectoryName(Path.GetFullPath(sourcePath))!, "bin");
        /* `linkRuntime: !allLibs`: con --libs/--full le dipendenze sono COPIE reali (una
         * distribuzione non può contenere symlink verso il build tree o l'assembly della
         * toolchain). Senza il flag resta il percorso di iterazione (symlink, ~140 KB). */
        var exe = Quietly(() => IlProgramCompiler.Compile(program, sourcePath, dir, Path.GetFileNameWithoutExtension(sourcePath),
            noOptimize: false, dumpIl: false, linkRuntime: !allLibs, dllOnly: false, slim: !all));
        if (allLibs || plugins.On || skills.On || brains.On)
            DeployBundle(dir, allLibs ? (libsSel ?? "used") : null,
                         plugins.On, plugins.Sel, skills.On, skills.Sel, brains.On, brains.Sel, sourcePath);
        /* skill/plugin/config DICHIARATI in app.settings.json: stesso bundle del target
         * native e del compilatore C (prima solo `--target native` lo faceva). */
        try { CommonLib.ProgramBundle.CopyNextTo(dir, sourcePath, m => Info(m)); } catch { /* best effort */ }
        if (full) CopyNetRuntime(dir);
        Info($"[IL] {sourcePath} -> {exe}{(allLibs ? " (bundle: dipendenze copiate)" : "")}{(full ? " (full: runtime .NET copiato)" : "")}");
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

    /* Contratto UNICO con `gmb` (C): il sorgente si indica come `--project <dir>` +
     * `--entry <file>` oppure, in forma abbreviata, come primo argomento posizionale.
     * Con solo `--project` l'entry viene scoperta (unico `.bas` con `SUB Main`). */
    private static string ResolveEntry(List<string> list, string? project, string? entry, int position)
    {
        if (!string.IsNullOrEmpty(entry)) return entry!;
        if (!string.IsNullOrEmpty(project)) return DiscoverEntry(project!);
        if (list.Count > position && !list[position].StartsWith("-")) return list[position];
        throw new ArgumentException("manca il file (usa <file.bas> oppure --project <dir> [--entry <file>])");
    }

    private static string DiscoverEntry(string project)
    {
        var root = Path.GetFullPath(project);
        if (!Directory.Exists(root)) throw new DirectoryNotFoundException($"progetto non trovato: {project}");
        var sep = Path.DirectorySeparatorChar;
        var files = Directory.EnumerateFiles(root, "*.bas", SearchOption.AllDirectories)
            .Where(f => !f.Contains($"{sep}bin{sep}") && !f.Contains($"{sep}obj{sep}") &&
                        !f.Contains($"{sep}work{sep}") && !f.Contains($"{sep}vendor{sep}") &&
                        !f.Contains($"{sep}.git{sep}"))
            .Where(f =>
            {
                try { return System.Text.RegularExpressions.Regex.IsMatch(File.ReadAllText(f), @"(?im)^\s*SUB\s+Main\b"); }
                catch { return false; }
            })
            .OrderBy(f => f, StringComparer.Ordinal).ToList();
        if (files.Count == 1) return files[0];
        if (files.Count == 0) throw new InvalidOperationException($"nessuna entry con SUB Main in {project}");
        throw new InvalidOperationException("più entry candidati: " +
            string.Join(", ", files.Select(f => Path.GetRelativePath(root, f))) + " — usa --entry <file>");
    }

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
              gmbcs build <file> [--target il|pcode|native] [--out dir]
                           [--libs[=used|all|gruppo|ml|chromium|obscura|ocr|nomi]] [--full] [--all]
                           [--plugins[=all|nomi]] [--skills[=all|nomi]] [--brains[=all|nomi]]
              gmbcs prginfo <file.prg>
              gmbcs run  <file> [--engine il|pcode|native]
              gmbcs eval "<istruzioni>" [--engine il|pcode]
              gmbcs debug <file> [--break N]… [--step]
              gmbcs aibench [RUN] [path]

            opzioni: -D name=val, --il-module <dll>, --out <file>, --json,
                     --project <dir> [--entry <file>]  (stesso contratto di `gmb` C)
            build: --libs[=…] (bundle: dipendenze managed reali + libs native in <out>/libs/<rid>,
                    incluse le FFI dichiarate con DECLARE … LIB "nome";
                    selettore =used|all|gruppo|componente|nomi), --plugins/--skills/--brains
                    (sottocartelle <out>/plugins|skills|brains), --full (= --libs + runtime .NET),
                    --all (nessun filtro ClosureSlim). Alias storici di --libs: -alllib, --alllibs.
            """);
    }
}
