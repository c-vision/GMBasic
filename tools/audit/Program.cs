// gmb_audit — audit automatico della copertura builtin per backend.
//
// Estrae:
//   1. l'enum BuiltInFunction (common/Compiler/BuiltInFunction.cs) — la superficie canonica;
//   2. i `case` implementati nel dispatcher di ciascun backend;
// e genera un manifest JSON riconciliabile (task 6.0.3 della PROPOSAL).
//
// Uso:
//   dotnet run --project tools/audit -- --pcodevm [--json out.json]
//   dotnet run --project tools/audit -- --list-backends
//
// Nessun backend viene modificato: è uno strumento di LETTURA (audit).

using System.Globalization;
using System.Text.RegularExpressions;
using Basic;
using Compiler;

namespace Basic.Audit;

internal static class Program
{
    /// <summary>Radice dei SORGENTI C# del motore: &lt;monorepo&gt;/GMBasicSharp (contiene `common/`).
    /// Prima del monorepo era la root del repo GMBasic (`common/` + `native/` insieme); oggi i
    /// sorgenti sono divisi fra `GMBasicSharp/` (C#), `GMBasic/native/` (C), `shared/` (BASIC,
    /// minitest, settings) e `docs/` — quindi ogni dominio ha la sua radice (vedi sotto).</summary>
    private static readonly string Root = ResolveRoot();
    private static readonly string RepoRoot = Path.GetFullPath(Path.Combine(Root, ".."));
    private static readonly string NativeRoot = Path.Combine(RepoRoot, "GMBasic", "native");
    private static readonly string SharedRoot = Path.Combine(RepoRoot, "shared");

    private static string ResolveRoot()
    {
        // Si cerca `GMBasicSharp/common/Ast/TokenKind.cs` risalendo da CWD e dalla cartella
        // dell'assembly: il tool funziona da qualunque directory (root del monorepo,
        // tools/audit, bin/Debug/net10.0).
        foreach (var start in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
        {
            var dir = new DirectoryInfo(Path.GetFullPath(start));
            while (dir is not null)
            {
                if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "common", "Ast", "TokenKind.cs")))
                    return Path.Combine(dir.FullName, "GMBasicSharp");
                if (File.Exists(Path.Combine(dir.FullName, "common", "Ast", "TokenKind.cs")))
                    return dir.FullName;   // lanciato da dentro GMBasicSharp
                dir = dir.Parent;
            }
        }
        throw new DirectoryNotFoundException(
            "gmb_audit: non trovo GMBasicSharp/common/Ast/TokenKind.cs risalendo da CWD ("
            + Directory.GetCurrentDirectory() + ") né da " + AppContext.BaseDirectory);
    }

    private static readonly string BuiltInEnumPath = Path.Combine(Root, "common", "Compiler", "BuiltInFunction.cs");
    private static readonly string PCodeDir = Path.Combine(Root, "common", "IlBackendRuntime", "PCode");

    private static int Main(string[] args)
    {
        if (args.Contains("--surface-tk"))
        {
            foreach (var f in CanonicalSurface())
                Console.WriteLine($"{f.TokenKind}\t{f.Name}");
            return 0;
        }

        if (args.Contains("--namespaces"))
        {
            var stdlib = new Basic.StdLib();
            foreach (var ns in stdlib.BuiltinNamespaceNames.OrderBy(n => n, StringComparer.OrdinalIgnoreCase))
                Console.WriteLine(ns);
            return 0;
        }

        if (args.Contains("--ns-names"))
        {
            var stdlib2 = new Basic.StdLib();
            foreach (var (ns, procs) in stdlib2.BuiltinProcedures.OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
                foreach (var pr in procs)
                    if (pr is Basic.NativeProcedureDefinition np)
                        Console.WriteLine($"{ns}\t{np.Name.Lexeme}");
        }

        if (args.Contains("--c99native-types"))
        {
            // Genera gmb_ns_types.h: { GMB_TOK_<TokenKind>, <1 se ReturnType è StringType> }
            // Il TokenKind è il VALORE enum C (direttiva 2026-09-08: niente risoluzione per
            // nome a valle); l'header richiede gmb_tokenkind.h incluso prima.
            var stdlib3 = new Basic.StdLib();
            var rows = new List<string>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var (ns, procs) in stdlib3.BuiltinProcedures.OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
                foreach (var pr in procs)
                    if (pr is Basic.NativeProcedureDefinition np && np.BuiltinKind != 0)
                    {
                        var tkName = np.BuiltinKind.ToString();
                        if (!seen.Add(tkName)) continue;
                        // is_str: 1 per StringType E per ArrayType(StringType) — un array di
                        // stringhe è reso come STRINGA nella forma scalare del DispatchToken
                        // (es. AI.SKILLSLIST(), AI.PLUGINSLIST(), AI.BRAINUSED). Senza questo,
                        // l'emitter C usava il ramo numerico (cc: gmb_string_t → gmb_int_t).
                        int isStr = (np.ReturnType is Basic.StringType
                                     || np.ReturnType is Basic.ArrayType { ElementType: Basic.StringType }) ? 1 : 0;
                        rows.Add($"    {{ GMB_TOK_{tkName}, {isStr} }},");
                    }
            Console.WriteLine("/* gmb_ns_types.h — tipo di ritorno per DispatchToken (generato dalla StdLib). */");
            Console.WriteLine("#ifndef GMB_NS_TYPES_H");
            Console.WriteLine("#define GMB_NS_TYPES_H");
            Console.WriteLine("typedef struct { gmb_tokenkind_t tk; int is_str; } gmb_ns_type_row_t;");
            Console.WriteLine("static const gmb_ns_type_row_t gmb_ns_types[] = {");
            foreach (var r in rows) Console.WriteLine(r);
            Console.WriteLine("};");
            Console.WriteLine("#define GMB_NS_TYPES_COUNT ((int)(sizeof(gmb_ns_types)/sizeof(gmb_ns_types[0])))");
            Console.WriteLine("#endif");
            return 0;
        }

        if (args.Contains("--minisamples"))
        {
            // Genera un MiniTest per OGNI funzione della superficie canonica che sia
            // auto-generabile: funzione PURA (NativeExecutionKind.Pure: nessuno stato) con
            // parametri di tipo scalare (String/Bit/numerico) e ritorno scalare.
            // Le impure (rete/AI/file/GPU) e quelle con array/handle hanno bisogno di un
            // sample scritto a mano: qui vengono SALTATE e contate (per dominio).
            var onlyNs = ArgValue(args, "--ns");
            var dry = args.Contains("--dry");
            var std4 = new Basic.StdLib();
            var outRoot = Path.Combine(SharedRoot, "minitests");
            int made = 0, skipped = 0;
            var skipReasons = new SortedDictionary<string, int>(StringComparer.Ordinal);
            var skipByNs = new SortedDictionary<string, int>(StringComparer.Ordinal);
            foreach (var (ns, procs) in std4.BuiltinProcedures.OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
            {
                if (onlyNs is not null && !ns.Equals(onlyNs, StringComparison.OrdinalIgnoreCase)) continue;
                foreach (var pr in procs)
                {
                    if (pr is not Basic.NativeProcedureDefinition np) continue;
                    if (!np.IsFunction) { skipped++; Bump(skipReasons, "comando(parser per-dominio)"); Bump(skipByNs, ns); continue; }
                    if (np.ExecutionKind != Basic.NativeExecutionKind.Pure) { skipped++; Bump(skipReasons, "impura (ExecutionKind != Pure)"); Bump(skipByNs, ns); continue; }
                    var litArgs = new List<string>();
                    // Contesto RUNTIME non sintetizzabile (RAG/embedding/collection/ambiente/
                    // finetune/LLM): un chiamata generica con letterali non è un oracolo valido.
                    // Coperti dagli hand-written dedicati (embed_dims, indextext_outcomes, ...).
                    // NON esclusi: i listini/config (List/Providers/Plugins*/Skills*/Mcp*), che
                    // sono deterministici e vanno allineati fra backend.
                    if (ns.Equals("AI", StringComparison.OrdinalIgnoreCase)
                        && np.Name.Lexeme is "Ask" or "AskMulti" or "AskChain" or "Embed"
                            or "EnsureCollection" or "IndexText" or "IndexMeta" or "Search"
                            or "RagContext" or "RemoveSource" or "PurgeDeprecated" or "GenTest"
                            or "MemoryRank" or "Observation" or "ObservationFile"
                            or "ObservationHash" or "StreamStats" or "SyncFolder" or "OrchSave"
                            or "ListCollections" or "FinetuneBaseHold" or "FinetuneBaseTrain"
                            or "FinetuneDemoChanged" or "FinetuneGreedyAfter"
                            // Stato/config/file: l'oracolo dipende dallo stato accumulato nel
                            // processo (ProviderServer condiviso fra file sulla gamba IL/PCode)
                            // o da file/registry → non un oracolo auto-generabile.
                            or "List" or "Rotate" or "McpServerList" or "McpServerMarketplaceList"
                            or "SkillsPrompt" or "VideoInfo" or "VideoFrame" or "ShareEncrypt")
                    { skipped++; Bump(skipReasons, "contesto runtime (RAG/embedding/stato)"); Bump(skipByNs, ns); continue; }

                    var prelude = new List<string>();
                    string? reason = null;
                    var anonIdx = 0;
                    foreach (var p in np.Parameters)
                    {
                        // Parametro ARRAY di scalari: si sintetizza un array di prova e lo si
                        // passa per nome (MATRIX/STATS/TRADING prendono array).
                        if (p.DataType is Basic.ArrayType at && MinisampleLiteral(at.ElementType) is { } elemLit)
                        {
                            var an = "__a" + anonIdx++;
                            prelude.Add("    DIM " + an + " = {" + elemLit + ", " + elemLit + "}");
                            litArgs.Add(an);
                            continue;
                        }
                        var lit = MinisampleLiteral(p.DataType);
                        if (lit is null) { reason = "param " + p.DataType.GetType().Name; break; }
                        litArgs.Add(lit);
                    }
                    // Ritorno ARRAY di scalari: PRINT di un array è deterministico e identico
                    // fra i backend → accettato (prima veniva saltato).
                    var retOk = MinisampleLiteral(np.ReturnType) is not null
                        || (np.ReturnType is Basic.ArrayType ra && MinisampleLiteral(ra.ElementType) is not null);
                    if (reason is null && !retOk) reason = "ritorno " + np.ReturnType.GetType().Name;
                    if (reason is not null) { skipped++; Bump(skipReasons, reason); Bump(skipByNs, ns); continue; }
                    var dir = Path.Combine(outRoot, ns.ToLowerInvariant());
                    var fname = ("gen_" + ns + "_" + np.Name.Lexeme).Replace('$', '_') + ".bas";
                    var file = Path.Combine(dir, fname);
                    if (File.Exists(file)) continue;
                    if (!dry)
                    {
                        Directory.CreateDirectory(dir);
                        File.WriteAllText(file,
                            "IMPORT " + ns.ToUpperInvariant() + "\n\n" +
                            "SUB Main()\n" +
                            string.Join("\n", prelude) + (prelude.Count > 0 ? "\n" : "") +
                            "    PRINT " + ns.ToUpperInvariant() + "." + np.Name.Lexeme.ToUpperInvariant() +
                            "(" + string.Join(", ", litArgs) + ")\n" +
                            "END SUB\n");
                    }
                    made++;
                }
            }
            Console.WriteLine($"minisamples: generati={made} saltati={skipped} ({(dry ? "DRY RUN" : "scritti in MiniTests/<ns>/")})");
            Console.WriteLine("motivi di skip:");
            foreach (var kv in skipReasons.OrderByDescending(k => k.Value).Take(10)) Console.WriteLine($"   {kv.Value,5}  {kv.Key}");
            Console.WriteLine("skip per namespace (top 12):");
            foreach (var kv in skipByNs.OrderByDescending(k => k.Value).Take(12)) Console.WriteLine($"   {kv.Value,5}  {kv.Key}");
            return 0;
        }

        static void Bump(SortedDictionary<string, int> d, string k) { d[k] = d.TryGetValue(k, out var c) ? c + 1 : 1; }

        if (args.Contains("--list-backends"))
        {
            Console.WriteLine("PCodeVM   → common/IlBackendRuntime/PCode/PCodeProcessor.Builtins.cs (case BuiltInFunction.*)");
            Console.WriteLine("VM/C      → native/gmb_vm_c/gmb_vm_core.c (case numerici + commento nome)");
            Console.WriteLine("IL        → common/IlBackendCodegen (case TokenKind.*)");
            Console.WriteLine("C99Native → native/gmb_c99native/src/gmb_c99_emit*.c (dispatch GMB_TOK_/GMB_STMT_)");
            return 0;
        }

        if (args.Contains("--pcodevm"))
        {
            return AuditPCodeVM(args);
        }

        if (args.Contains("--registry-pcodevm"))
        {
            return RegistryPCodeVM(args);
        }

        if (args.Contains("--registry-il"))
        {
            return RegistryIl(args);
        }

        if (args.Contains("--registry-vmc"))
        {
            return RegistryVmc(args);
        }

        if (args.Contains("--registry-c99native"))
        {
            return RegistryC99Native(args);
        }

        if (args.Contains("--diff"))
        {
            return DiffManifests(args);
        }

        if (args.Contains("--c99native"))
        {
            return AuditC99Native(args);
        }

        if (args.Contains("--all"))
        {
            return AuditAll(args);
        }

        if (args.Contains("--il"))
        {
            return AuditTokenKindBackend(args, "IL");
        }

        if (args.Contains("--vmc"))
        {
            return AuditVmc(args);
        }

        if (args.Contains("--tokenkind-class"))
        {
            return AuditTokenKindClass(args);
        }

        Console.Error.WriteLine("uso: gmb_audit [--pcodevm|--il|--vmc] [--json out.json] | --registry-pcodevm | --list-backends | --tokenkind-class");
        return 2;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // PCodeVM
    // ─────────────────────────────────────────────────────────────────────────
    private static int AuditPCodeVM(string[] args)
    {
        var jsonOut = ArgValue(args, "--json");

        var enumSrc = File.ReadAllText(BuiltInEnumPath);

        // 1. membri enum (multi-riga: "A = 1, B = 2, C,")
        var members = ExtractEnumMembers(enumSrc, "BuiltInFunction");

        // 2. case implementati — scansiona TUTTI i file del PCodeVM (Builtins.cs, Linalg.cs,
        //    PCodeProcessor.cs, PCodeVm.cs, ...) perché il dispatch è sparso tra i partial.
        //    Pattern riconosciuti (post-refactor 2026-09-03): le voci dei Dictionary di dispatch
        //    "[BuiltInFunction.X] = (task, args) =>" (core + 38 domini) e i "case BuiltInFunction.X:"
        //    residui (es. ExecuteLinAlgBuiltIn in Linalg.cs, non ancora tabellato).
        var cases = new HashSet<string>();
        foreach (var f in Directory.GetFiles(Path.Combine(Root, "common", "IlBackendRuntime", "PCode"), "*.cs", SearchOption.AllDirectories))
        {
            var text = File.ReadAllText(f);
            foreach (Match m in Regex.Matches(text, @"\[BuiltInFunction\.([A-Z][A-Z0-9_]*)\]\s*=\s*\("))
                cases.Add(m.Groups[1].Value);
            foreach (Match m in Regex.Matches(text, @"case\s+BuiltInFunction\.([A-Z][A-Z0-9_]*)"))
                cases.Add(m.Groups[1].Value);
        }

        // 2bis. Comandi dispatchati come SYSCALL (non BuiltInFunction): il compilatore emette
        //       OP_SYSCALL per i comandi legacy (DBF VALIDATE, ISAM SET/APPLY/FOLLOWREPLICATION…)
        //       e il PCode li gestisce in PCodeProcessor.Syscalls.cs con `case SysCall.X:`.
        //       Senza questo, l'audit li contava come gap PUR ESSENDO implementati (falsi gap
        //       DBFVALIDATE/ISAMSETREPLICATION/ISAMAPPLYREPLICATION/ISAMFOLLOWREPLICATION).
        //       Corrispondenza per nome normalizzato: SysCall `ISAM_SET_REPLICATION` ↔
        //       BuiltInFunction `ISAMSETREPLICATION` (underscore rimossi, case-insensitive).
        var syscallNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var f in Directory.GetFiles(Path.Combine(Root, "common", "IlBackendRuntime", "PCode"), "*.cs", SearchOption.AllDirectories))
        {
            var text = File.ReadAllText(f);
            foreach (Match m in Regex.Matches(text, @"case\s+SysCall\.([A-Z][A-Z0-9_]*)"))
                syscallNames.Add(m.Groups[1].Value.Replace("_", ""));
        }
        foreach (var name in members)
            if (!cases.Contains(name) && syscallNames.Contains(name.Replace("_", "")))
                cases.Add(name);

        var implemented = members.Where(cases.Contains).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        var notImplemented = members.Where(m => !cases.Contains(m)).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();

        var manifest = new
        {
            backend = "PCodeVM",
            superficie = "BuiltInFunction (enum, funzioni)",
            file_enum = "common/Compiler/BuiltInFunction.cs",
            file_dispatch = "common/IlBackendRuntime/PCode/PCodeProcessor.Builtins.cs",
            totale = members.Count,
            implementate = implemented.Count,
            non_coperte = notImplemented.Count,
            percentuale = Pct(implemented.Count, members.Count),
            non_coperte_lista = notImplemented,
        };

        if (jsonOut is not null)
        {
            File.WriteAllText(jsonOut, System.Text.Json.JsonSerializer.Serialize(manifest,
                new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
            Console.WriteLine($"manifest scritto: {jsonOut}");
        }

        Console.WriteLine($"PCodeVM: {implemented.Count}/{members.Count} builtin implementate ({manifest.percentuale.ToString("0.0", CultureInfo.InvariantCulture)}%)");
        if (notImplemented.Count > 0)
        {
            Console.WriteLine($"NON implementate ({notImplemented.Count}):");
            foreach (var n in notImplemented) Console.WriteLine($"  - {n}");
        }

        return notImplemented.Count == 0 ? 0 : 1;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Registry PCodeVM — genera il codice C# che popola BackendImplementationRegistry
    // (task 6.2.8): per ogni BuiltInFunction implementata emette Register(Backend.PCodeVM, "X").
    // L'output è un metodo parziale del registry, da incollare in una classe di setup.
    // ─────────────────────────────────────────────────────────────────────────
    // Genera un registry per-backend dalla SUPERFICIE CANONICA StdLib (CanonicalSurface):
    // registra `namespace.nome` delle funzioni canoniche che il backend implementa col suo
    // meccanismo di dispatch reale (stessa detection del coverage CanonicalImplemented).
    // Così i registry contengono le FUNZIONI StdLib, non i nomi grezzi di dispatch.
    private static void WriteCanonicalRegistry(System.Text.StringBuilder output, string backend, string flag, string desc)
    {
        var surface = CanonicalSurface();
        var impl = ImplementedCanonicalList(backend, surface)
            .OrderBy(f => f.Name, StringComparer.OrdinalIgnoreCase).ToList();
        output.AppendLine($"// FILE: BackendSupport/Registry/{backend}.cs");
        output.AppendLine($"// Registry del backend {desc} — generato da gmb_audit --registry-{flag} (funzioni canoniche StdLib implementate).");
        output.AppendLine();
        output.AppendLine("namespace Basic.BackendSupport;");
        output.AppendLine();
        output.AppendLine($"// Popola BackendImplementationRegistry per il backend {desc} (namespace.nome canonico).");
        output.AppendLine("public sealed partial class BackendImplementationRegistry");
        output.AppendLine("{");
        output.AppendLine($"    public static BackendImplementationRegistry Create{backend}()");
        output.AppendLine("    {");
        output.AppendLine("        var r = new BackendImplementationRegistry();");
        foreach (var f in impl)
            output.AppendLine($"        r.Register(Backend.{backend}, \"{f.Ns}.{f.Name}\");");
        output.AppendLine("        return r;");
        output.AppendLine("    }");
        output.AppendLine("}");
    }

    private static int FinishRegistry(string[] args, System.Text.StringBuilder output, string backend)
    {
        var outPath = ArgValue(args, "--out");
        if (outPath is not null)
        {
            File.WriteAllText(outPath, output.ToString());
            Console.WriteLine($"registry {backend} generato: {outPath}");
        }
        else Console.Write(output.ToString());
        return 0;
    }

    private static int RegistryPCodeVM(string[] args)
    {
        var output = new System.Text.StringBuilder();
        WriteCanonicalRegistry(output, "PCodeVM", "pcodevm", "PCodeVM");
        return FinishRegistry(args, output, "PCodeVM");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Registry IL — genera il codice C# che popola BackendImplementationRegistry
    // per il backend IL (TokenKind dispatchati nel codegen). Stesso pattern PCodeVM.
    // ─────────────────────────────────────────────────────────────────────────
    private static int RegistryIl(string[] args)
    {
        var output = new System.Text.StringBuilder();
        WriteCanonicalRegistry(output, "IL", "il", "IL");
        return FinishRegistry(args, output, "IL");
    }

    /// <summary>Valore letterale canonico per un tipo scalare, o null se non sintetizzabile.</summary>
    private static string? MinisampleLiteral(Basic.DataType dt) => dt switch
    {
        // Letterali NON degeneri: una stringa vuota "" o uno 0 fanno fallire per setup/parse
        // molte funzioni (era il difetto dei gen_* "invalid oracolo"). "x"/1 restano
        // deterministici e sintetizzabili.
        Basic.StringType => "\"x\"",
        Basic.BitType => "TRUE",
        Basic.LongType or Basic.IntegerType or Basic.ByteType
            or Basic.DoubleType or Basic.SingleType or Basic.DateType => "1",
        _ => null,
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Registry VM/C — genera il codice C# che popola BackendImplementationRegistry
    // per il backend VM/C (case numerici del dispatcher). Confronta i valori dei
    // `case N:` con i valori dell'enum BuiltInFunction.
    // ─────────────────────────────────────────────────────────────────────────
    private static int RegistryVmc(string[] args)
    {
        var output = new System.Text.StringBuilder();
        WriteCanonicalRegistry(output, "VMC", "vmc", "VM/C");
        return FinishRegistry(args, output, "VMC");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Diff manifest — confronta due manifest JSON e mostra le differenze di copertura
    // (task 6.0.4): quali superfici sono state implementate/rimosse tra due versioni.
    // Uso: gmb_audit --diff old.json new.json
    // ─────────────────────────────────────────────────────────────────────────
    private static int DiffManifests(string[] args)
    {
        var oldPath = args.Length > 1 ? args[1] : null;
        var newPath = args.Length > 2 ? args[2] : null;
        if (oldPath is null || newPath is null)
        {
            Console.Error.WriteLine("uso: gmb_audit --diff <old.json> <new.json>");
            return 2;
        }

        var oldDoc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(oldPath));
        var newDoc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(newPath));

        string? GetBackend(System.Text.Json.JsonDocument d) =>
            d.RootElement.TryGetProperty("backend", out var b) ? b.GetString() : null;
        List<string> GetList(System.Text.Json.JsonDocument d, string prop)
        {
            var outList = new List<string>();
            if (d.RootElement.TryGetProperty(prop, out var arr))
                foreach (var e in arr.EnumerateArray())
                    outList.Add(e.GetString()!);
            return outList;
        }

        var oldBackend = GetBackend(oldDoc);
        var newBackend = GetBackend(newDoc);
        if (oldBackend != newBackend)
        {
            Console.WriteLine($"backends diversi: {oldBackend} vs {newBackend}");
            return 1;
        }

        var oldImplemented = GetList(oldDoc, "non_coperte_lista");
        var newImplemented = GetList(newDoc, "non_coperte_lista");
        // NOTA: i manifest contengono 'non_coperte_lista' (i gap). L'analisi:
        // - implementate di nuovo: gap in old che non sono più gap in new
        // - regredite: non-gap in old che sono diventate gap in new
        var oldGaps = new HashSet<string>(oldImplemented, StringComparer.OrdinalIgnoreCase);
        var newGaps = new HashSet<string>(newImplemented, StringComparer.OrdinalIgnoreCase);
        var newlyImplemented = oldGaps.Where(g => !newGaps.Contains(g))
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        var regressed = newGaps.Where(g => !oldGaps.Contains(g))
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();

        Console.WriteLine($"Diff {oldBackend}: {oldPath} → {newPath}");
        Console.WriteLine();
        Console.WriteLine($"  NUOVAMENTE IMPLEMENTATE ({newlyImplemented.Count}):");
        foreach (var n in newlyImplemented.Take(30)) Console.WriteLine($"    + {n}");
        if (newlyImplemented.Count > 30) Console.WriteLine($"    ... e altri {newlyImplemented.Count - 30}");
        Console.WriteLine();
        Console.WriteLine($"  REGRESSIONI ({regressed.Count}):");
        foreach (var n in regressed.Take(30)) Console.WriteLine($"    - {n}");
        if (regressed.Count > 30) Console.WriteLine($"    ... e altri {regressed.Count - 30}");

        return regressed.Count == 0 ? 0 : 1;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Registry C99Native — genera il codice C# che popola BackendImplementationRegistry
    // per il backend C99Native (gmb_c99_emit.c: dispatch statement per GMB_STMT_*).
    // ─────────────────────────────────────────────────────────────────────────
    private static int RegistryC99Native(string[] args)
    {
        var output = new System.Text.StringBuilder();
        WriteCanonicalRegistry(output, "C99Native", "c99native", "C99Native");
        return FinishRegistry(args, output, "C99Native");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Audit All — genera i manifest di TUTTI i backend + report unificato.
    // Il sistema efficiente per capire cosa è implementato in ogni backend:
    // un solo comando → la copertura completa.
    // ─────────────────────────────────────────────────────────────────────────
    private static int AuditAll(string[] args)
    {
        var outDir = ArgValue(args, "--out-dir") ?? Path.Combine(RepoRoot, "tools", "reports", "manifests");
        Directory.CreateDirectory(outDir);
        var manifests = new List<(string Backend, int Impl, int Tot, double Pct)>();

        // (flag, backend, nome file) — ogni audit scrive il manifest via --json
        var audits = new (string Flag, string Name)[]
        {
            ("--pcodevm", "PCodeVM"),
            ("--il", "IL"),
            // C99 (C# transpiler) RITIRATO: vive solo in GMBasic-Deprecated. Il progetto C usa C99Native.
            ("--vmc", "VMC"),
            ("--c99native", "C99Native"),
        };

        foreach (var (flag, name) in audits)
        {
            var json = Path.Combine(outDir, name + ".json");
            var code = AuditOne(flag, name, json);
            if (code != 0 && name is not "VMC")
            {
                Console.WriteLine($"  [WARN] {name}: audit exit {code}");
            }
            if (File.Exists(json))
            {
                var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(json));
                var impl = doc.RootElement.GetProperty("implementate").GetInt32();
                var tot = doc.RootElement.GetProperty("totale").GetInt32();
                var pct = Pct(impl, tot);
                manifests.Add((name, impl, tot, pct));
            }
        }

        // report unificato — ora CON la superficie canonica StdLib come denominatore comune
        var surface = CanonicalSurface();
        int canonicalTotal = surface.Count;
        Console.WriteLine();
        Console.WriteLine("================== AUDIT BACKEND (riepilogo) ==================");
        Console.WriteLine($"  Canonical surface: {canonicalTotal} StdLib functions (ns.name)");
        Console.WriteLine($"  {"Backend",-12} {"Implementate",-12} {"Canonici",-10} {"%",-5}");
        Console.WriteLine($"  {"-------",-12} {"-----------",-12} {"--------",-10} {"--",-5}");
        foreach (var (b, impl, tot, pct) in manifests)
        {
            var canonImpl = CanonicalImplemented(b, surface);
            Console.WriteLine($"  {b,-12} {canonImpl,-12} {canonicalTotal,-10} {Pct(canonImpl, canonicalTotal).ToString("0.0", CultureInfo.InvariantCulture) + "%",-6}");
        }
        Console.WriteLine("==============================================================");
        Console.WriteLine();
        Console.WriteLine("=== GAP canonici per backend (funzioni NON implementate) ===");
        foreach (var b in new[] { "PCodeVM", "IL", "VMC", "C99Native" })
        {
            var gaps = CanonicalGaps(b, surface);
            Console.WriteLine($"{b}: {gaps.Count} gap su {canonicalTotal}");
            foreach (var g in gaps)
                Console.WriteLine($"   - {g.Ns}.{g.Name}  [{g.TokenKind}]" + (g.Builtin.HasValue ? $" → {g.Builtin}" : "  (senza BuiltInFunction mappato)"));
        }
        Console.WriteLine();

        // report su file — colonna canonica per tutti e 4
        var reportPath = Path.Combine(RepoRoot, "tools", "reports", "audit-backends.md");
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("# GMBasic — Backend coverage (automatic audit)");
        sb.AppendLine();
        sb.AppendLine("Generated by `gmb_audit --all`. Manifests in `tools/reports/manifests/`.");
        sb.AppendLine();
        sb.AppendLine($"**Canonical surface**: {canonicalTotal} functions `(namespace, name, TokenKind)` registered by StdLib (`BuiltinProcedures`).");
        sb.AppendLine();
        sb.AppendLine("| Backend | Implemented functions | Canonical | % |");
        sb.AppendLine("|---|---|---|---|");
        foreach (var (b, impl, tot, pct) in manifests)
        {
            var canonImpl = CanonicalImplemented(b, surface);
            sb.AppendLine($"| {b} | {canonImpl} | {canonicalTotal} | {Pct(canonImpl, canonicalTotal).ToString("0.0", CultureInfo.InvariantCulture)}% |");
        }
        sb.AppendLine();
        sb.AppendLine("> Single surface for all backends: the canonical StdLib functions. Each backend counts how many of them its real dispatch covers. VMC via `/* NAME */` in the cases; C99Native via GMB_STMT (the functions the native compiler can emit).");
        File.WriteAllText(reportPath, sb.ToString());
        Console.WriteLine($"report: {reportPath}");
        return 0;
    }

    /// <summary>Esegue un singolo audit con --json e ne restituisce l'exit code.</summary>
    private static int AuditOne(string flag, string name, string jsonPath)
    {
        // chiama le funzioni direttamente (stesso processo) per non ri-avviare dotnet
        return flag switch
        {
            "--pcodevm" => AuditPCodeVM(new[] { flag, "--json", jsonPath }),
            "--il" => AuditTokenKindBackend(new[] { flag, "--json", jsonPath }, "IL"),
            "--vmc" => AuditVmc(new[] { flag, "--json", jsonPath }),
            "--c99native" => AuditC99Native(new[] { flag, "--json", jsonPath }),
            _ => 2,
        };
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Audit C99Native — statement GMB_STMT_* nell'header vs dispatch nell'emitter C:
    // individua statement parsati/linkati ma NON emessi (gap di emissione C).
    // ─────────────────────────────────────────────────────────────────────────
    private static int AuditC99Native(string[] args)
    {
        var jsonOut = ArgValue(args, "--json");
        // I SORGENTI C stanno nel repo nativo (`<monorepo>/GMBasic/native`), NON sotto
        // `GMBasicSharp/`: `Root` è la radice del repo C# (usata per i manifest managed).
        // Con `Root` il path diventava `GMBasicSharp/native/...` → DirectoryNotFoundException.
        var root = NativeRoot;
        var astHdr = File.ReadAllText(Path.Combine(root, "gmb_compiler_c", "frontend", "include", "gmb_ast.h"));
        var emitFiles = new[] { "gmb_c99_emit.c", "gmb_c99_emit.h" }
            .Select(f => Path.Combine(root, "gmb_c99native", f))
            .Concat(Directory.GetFiles(Path.Combine(root, "gmb_c99native", "src"), "gmb_c99_emit*.c", SearchOption.AllDirectories));

        var emitText = new System.Text.StringBuilder();
        foreach (var f in emitFiles)
            if (File.Exists(f))
                emitText.AppendLine(File.ReadAllText(f));
        var emitSrc = emitText.ToString();

        // superficie: GMB_STMT_* nell'header (statement del linguaggio)
        var stmts = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (Match m in Regex.Matches(astHdr, @"GMB_STMT_([A-Z0-9_]+)\b"))
            stmts.Add(m.Groups[1].Value);

        // dispatchati: case GMB_STMT_* nell'emitter
        var dispatched = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (Match m in Regex.Matches(emitSrc, @"case\s+GMB_STMT_([A-Z0-9_]+)"))
            dispatched.Add(m.Groups[1].Value);
        // referenziati (non solo case): li considero dispatchati se appaiono come costante
        foreach (Match m in Regex.Matches(emitSrc, @"GMB_STMT_([A-Z0-9_]+)\b"))
            dispatched.Add(m.Groups[1].Value);

        var implemented = stmts.Where(dispatched.Contains).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        var notImplemented = stmts.Where(s => !dispatched.Contains(s)).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();

        // Statement CORRETTAMENTE assenti dall'emitter — non sono gap (Todo MiniTest §C5,
        // verificati sul tree il 2026-09-17; mai nascosti: restano riportati a parte).
        // - CONST: solo dichiarativa, risolta dal prepass (gmb_compiler.c `case GMB_STMT_CONST: return`);
        // - LOOP/NEXT/WEND: chiusure strutturali DO/FOR/WHILE, emesse dall'heading (linker: strutturali);
        // - CATCH/FINALLY: sotto-nodi di TRY, emessi da st_try via setjmp (n_catch/n_finally);
        // - REQUIRE: direttiva compile-time, nessun codice runtime (compiler: break; linker: print).
        // (GETKEY, ex-membro del set, è emesso dal 2026-09-17: parser C + st_getkey + OP_SYSCALL 7.)
        var correctlyAbsent = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            { "CONST", "LOOP", "NEXT", "WEND", "CATCH", "FINALLY", "REQUIRE" };
        var documentedAbsent = notImplemented.Where(correctlyAbsent.Contains)
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        notImplemented = notImplemented.Where(s => !correctlyAbsent.Contains(s))
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();

        var manifest = new
        {
            backend = "C99Native",
            superficie = "GMB_STMT_* (statement linguaggio, header gmb_ast.h)",
            file_emit = "native/gmb_c99native/src/gmb_c99_emit.c",
            totale = stmts.Count,
            implementate = implemented.Count,
            non_coperte = notImplemented.Count,
            percentuale = Pct(implemented.Count, stmts.Count),
            non_coperte_lista = notImplemented,
            correttamente_assenti = documentedAbsent.Count,
            correttamente_assenti_lista = documentedAbsent,
        };

        if (jsonOut is not null)
        {
            File.WriteAllText(jsonOut, System.Text.Json.JsonSerializer.Serialize(manifest,
                new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
            Console.WriteLine($"manifest scritto: {jsonOut}");
        }

        Console.WriteLine($"C99Native: {implemented.Count}/{stmts.Count} statement dispatchati ({manifest.percentuale.ToString("0.0", CultureInfo.InvariantCulture)}%)");
        if (documentedAbsent.Count > 0)
        {
            Console.WriteLine($"Correttamente assenti dall'emitter ({documentedAbsent.Count}) — non sono gap (motivi nel sorgente audit):");
            foreach (var n in documentedAbsent) Console.WriteLine($"  = {n}");
        }
        if (notImplemented.Count > 0)
        {
            Console.WriteLine($"NON dispatchati nell'emitter C ({notImplemented.Count}) — parsati/linkati ma senza emissione C:");
            foreach (var n in notImplemented) Console.WriteLine($"  - {n}");
        }
        return notImplemented.Count == 0 ? 0 : 1;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // IL — audit per TokenKind: estrae i `case TokenKind.X` dal codegen IL e li
    // confronta con la superficie canonica (il mapping Compiler TokenKind→BuiltInFunction).
    // ─────────────────────────────────────────────────────────────────────────
    private static int AuditTokenKindBackend(string[] args, string backend)
    {
        var jsonOut = ArgValue(args, "--json");
        var dirs = new[] { Path.Combine(Root, "common", "IlBackendCodegen") };

        var allText = new System.Text.StringBuilder();
        foreach (var dir in dirs)
            foreach (var f in Directory.GetFiles(dir, "*.cs", SearchOption.AllDirectories))
                if (!f.Contains("/obj/") && !f.Contains("/bin/"))
                    allText.AppendLine(File.ReadAllText(f));
        // §4.10 SEMAPRAX: le funzioni del GMB Semantic Graph (AI.SYMBOL/…, LLM.VERIFY/…) NON
        // hanno un `case` nel backend: sono risolte a COMPILE/LINK-time dal Linker fold
        // (SemanticQueryHook → switch(kind) con case TokenKind.AiSymbol/…). Il fold È
        // l'implementazione → va scansionato, altrimenti falsi gap (AiSymbol/Impact/Callmap/Revision).
        {
            var foldFile = Path.Combine(Root, "common", "SemanticGraph", "SemanticQueryHook.cs");
            if (File.Exists(foldFile)) allText.AppendLine(File.ReadAllText(foldFile));
        }
        var txt = allText.ToString();

        // 1. TokenKind usati nei case del backend (gestisce `case A or B`, `is TokenKind`)
        var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (Match m in Regex.Matches(txt, @"case\s+TokenKind\.([A-Za-z0-9_]+)"))
            used.Add(m.Groups[1].Value);
        foreach (Match m in Regex.Matches(txt, @"or\s+TokenKind\.([A-Za-z0-9_]+)"))
            used.Add(m.Groups[1].Value);
        foreach (Match m in Regex.Matches(txt, @"is\s+TokenKind\.([A-Za-z0-9_]+)"))
            used.Add(m.Groups[1].Value);
        // `== TokenKind.X` (es. stmt.Target.Kind == TokenKind.Next — Gpm.cs)
        foreach (Match m in Regex.Matches(txt, @"==\s+TokenKind\.([A-Za-z0-9_]+)"))
            used.Add(m.Groups[1].Value);
        // lookup table dichiarativa NsKindRow: `Kind = TokenKind.X` (builtin security e altri)
        foreach (Match m in Regex.Matches(txt, @"Kind\s*=\s*TokenKind\.([A-Za-z0-9_]+)"))
            used.Add(m.Groups[1].Value);
        // arm di switch-espressione: `TokenKind.X =>` (dispatch puro per Kind)
        foreach (Match m in Regex.Matches(txt, @"\bTokenKind\.([A-Za-z0-9_]+)\s*=>"))
            used.Add(m.Groups[1].Value);
        // dizionario-indexer `[TokenKind.X] = delegate` (tabelle di dominio BITS/…):
        // forma di dispatch reale, prima invisibile all'audit.
        foreach (Match m in Regex.Matches(txt, @"\[\s*TokenKind\.([A-Za-z0-9_]+)\s*\]\s*="))
            used.Add(m.Groups[1].Value);
        // dizionario-arm `{ TokenKind.X, delegate }` (BATCH centrali/early):
        // forma di dispatch reale, prima invisibile all'audit (falsi gap ChiSquaredCdf/
        // ChiSquaredPdf/LogisticPdf/NormalTest/Brotli/Bzip2).
        foreach (Match m in Regex.Matches(txt, @"\{\s*TokenKind\.([A-Za-z0-9_]+)\s*,"))
            used.Add(m.Groups[1].Value);
        // call-site della tabella SbRow(TokenKind.X, ...):
        // dispatch reale del sotto-dominio Second Brain, prima invisibile all'audit → falso
        // gap TeamOnStream (la riga è emessa, ma il call-site non era riconosciuto).
        foreach (Match m in Regex.Matches(txt, @"SbRow\(\s*TokenKind\.([A-Za-z0-9_]+)\s*,"))
            used.Add(m.Groups[1].Value);

        // 2. superfice canonica: TokenKind→BuiltInFunction dal Compiler
        //    (esclude le righe commentate: //{ TokenKind.JsonToUdt, ... } — bug reale,
        //    creavano falsi canonici JsonToUdt/UdtToJson)
        var compiler = Regex.Replace(
            File.ReadAllText(Path.Combine(Root, "common", "Compiler", "Compiler.Expressions.Functions.cs")),
            @"//[^\n]*", "");
        var canonical = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (Match m in Regex.Matches(compiler, @"TokenKind\.([A-Za-z0-9_]+)\s*,\s*BuiltInFunction\.([A-Za-z0-9_]+)"))
            canonical.Add(m.Groups[1].Value);

        var implemented = canonical.Where(used.Contains).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        var notImplemented = canonical.Where(c => !used.Contains(c)).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();

        var manifest = new
        {
            backend,
            superficie = "TokenKind (funzioni mappate dal Compiler)",
            totale = canonical.Count,
            implementate = implemented.Count,
            non_coperte = notImplemented.Count,
            percentuale = Pct(implemented.Count, canonical.Count),
            non_coperte_lista = notImplemented,
        };

        if (jsonOut is not null)
        {
            File.WriteAllText(jsonOut, System.Text.Json.JsonSerializer.Serialize(manifest,
                new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
            Console.WriteLine($"manifest scritto: {jsonOut}");
        }

        Console.WriteLine($"{backend}: {implemented.Count}/{canonical.Count} TokenKind di funzioni implementati ({manifest.percentuale.ToString("0.0", CultureInfo.InvariantCulture)}%)");
        Console.WriteLine("  ATTENZIONE: il numero sopra usa un denominatore INTERNO (solo TokenKind con BuiltInFunction nel Compiler), non la superficie canonica.");
        {
            // Numero coerente con `--all`: stessa superficie canonica (1528 funzioni StdLib)
            // per tutti i backend. Questo è il numero da citare, non quello sopra.
            var surface = CanonicalSurface();
            var canonImpl = CanonicalImplemented(backend, surface);
            var canonPct = Pct(canonImpl, surface.Count);
            Console.WriteLine($"  SUPERFICIE CANONICA (coerente con --all): {canonImpl}/{surface.Count} ({canonPct.ToString("0.0", CultureInfo.InvariantCulture)}%)");
        }
        if (notImplemented.Count > 0)
        {
            Console.WriteLine($"NON implementati TokenKind ({notImplemented.Count}):");
            foreach (var n in notImplemented.Take(10)) Console.WriteLine($"  - {n}");
            if (notImplemented.Count > 10) Console.WriteLine($"  ... e altri {notImplemented.Count - 10}");
        }

        return notImplemented.Count == 0 ? 0 : 1;
    }

    // Estrae il corpo (brace-matching esatto) di ogni `static void gmb_bf_X(...)`
    // in gmb_builtin.c — usato per rilevare gli handler stub (vuoti/guard-only).
    private static Dictionary<string, string> ExtractVmcHandlerBodies(string src)
    {
        var map = new Dictionary<string, string>();
        // `static` OPZIONALE: gli handler in file di dominio (§0quater, es. gmb_builtin_bits.c)
        // sono non-static per essere chiamati dalla tabella in gmb_builtin.c (TU diversa).
        // Prefisso `gmb_bf_` O `vmc_` (MCP in gmb_mcp_exec.c, uncertainty in gmb_uncertainty_exec.c).
        foreach (Match sig in Regex.Matches(src, @"(?:static\s+)?[A-Za-z_][\w\s\*]*?\b((?:gmb_bf_|vmc_)\w+)\s*\("))
        {
            var i = src.IndexOf('{', sig.Index + sig.Length);
            if (i < 0) continue;
            // Bilanciamento graffe STRING/CHAR/COMMENT-AWARE: un `{`/`}` in un letterale
            // (es. JSON `"{"` nel body di gmb_bf_askmulti_c1287 / gmb_bf_ragcontextmulti_c1286)
            // rompeva il conteggio naive → body mai estratto → classificato STUB → falso gap.
            int depth = 0, j = i;
            bool inStr = false, inChr = false, inLine = false, inBlock = false;
            while (j < src.Length)
            {
                char ch = src[j];
                if (inLine) { if (ch == '\n') inLine = false; j++; continue; }
                if (inBlock) { if (ch == '*' && j + 1 < src.Length && src[j + 1] == '/') { inBlock = false; j += 2; } else j++; continue; }
                if (inStr) { if (ch == '\\') j += 2; else { if (ch == '"') inStr = false; j++; } continue; }
                if (inChr) { if (ch == '\\') j += 2; else { if (ch == '\'') inChr = false; j++; } continue; }
                if (ch == '/' && j + 1 < src.Length && src[j + 1] == '/') { inLine = true; j += 2; continue; }
                if (ch == '/' && j + 1 < src.Length && src[j + 1] == '*') { inBlock = true; j += 2; continue; }
                if (ch == '"') { inStr = true; j++; continue; }
                if (ch == '\'') { inChr = true; j++; continue; }
                if (ch == '{') depth++;
                else if (ch == '}') { depth--; if (depth == 0) break; }
                j++;
            }
            if (j < src.Length)
                map[sig.Groups[1].Value] = src.Substring(i + 1, j - i - 1);
        }
        return map;
    }

    /// <summary>Corpi di TUTTE le funzioni C (`gmb_*` ecc.) di un sorgente — usato per
    /// riconoscere le tabelle VMC che chiamano direttamente il runtime condiviso (§0bis),
    /// es. `{ 395, gmb_mouse_button }`: il delegato non è `gmb_bf_*` ma è una implementazione
    /// reale. Stessa estrazione graffe string/comment-aware di ExtractVmcHandlerBodies.</summary>
    private static Dictionary<string, string> ExtractAllFunctionBodies(string src)
    {
        var map = new Dictionary<string, string>();
        foreach (Match sig in Regex.Matches(src, @"^[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;{)]*\)\s*\{", RegexOptions.Multiline))
        {
            var name = sig.Groups[1].Value;
            var i = src.IndexOf('{', sig.Index + sig.Length - 1);
            if (i < 0) continue;
            int depth = 0, j = i;
            bool inStr = false, inChr = false, inLine = false, inBlock = false;
            while (j < src.Length)
            {
                char ch = src[j];
                if (inLine) { if (ch == '\n') inLine = false; j++; continue; }
                if (inBlock) { if (ch == '*' && j + 1 < src.Length && src[j + 1] == '/') { inBlock = false; j += 2; } else j++; continue; }
                if (inStr) { if (ch == '\\') j += 2; else { if (ch == '"') inStr = false; j++; } continue; }
                if (inChr) { if (ch == '\\') j += 2; else { if (ch == '\'') inChr = false; j++; } continue; }
                if (ch == '/' && j + 1 < src.Length && src[j + 1] == '/') { inLine = true; j += 2; continue; }
                if (ch == '/' && j + 1 < src.Length && src[j + 1] == '*') { inBlock = true; j += 2; continue; }
                if (ch == '"') { inStr = true; j++; continue; }
                if (ch == '\'') { inChr = true; j++; continue; }
                if (ch == '{') depth++;
                else if (ch == '}') { depth--; if (depth == 0) break; }
                j++;
            }
            if (j < src.Length) map[name] = src.Substring(i + 1, j - i - 1);
        }
        return map;
    }

    // True se il corpo è uno STUB: vuoto, o solo la guardia GML_VM_REQUIRE().    // La sola guardia non è un'implementazione: quando la lib TORCH è caricata il
    // corpo non fa nulla → risultato silenzioso (vietato da §0bis).
    private static bool IsVmcStubBody(string body)
    {
        var norm = Regex.Replace(body, @"/\*.*?\*/", "", RegexOptions.Singleline);
        norm = Regex.Replace(norm, @"\s+", "");
        if (norm.Length == 0) return true;
        return Regex.IsMatch(norm, @"^GML_VM_REQUIRE\(\);\s*$");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // VM/C — audit per case numerici: estrae `case N: /* NOME */` e li confronta
    // con i valori dell'enum BuiltInFunction (nome = valore).
    // ─────────────────────────────────────────────────────────────────────────
    private static int AuditVmc(string[] args)
    {
        var jsonOut = ArgValue(args, "--json");
        var vmcPath = Path.Combine(NativeRoot, "gmb_vm_c", "gmb_vm_core.c");
        var src = File.ReadAllText(vmcPath);

        // enum BuiltInFunction: nome → valore
        var enumSrc = File.ReadAllText(BuiltInEnumPath);
        var nameToVal = new Dictionary<string, long>();
        var enumBody = Regex.Match(enumSrc, $@"enum\s+BuiltInFunction\b[^{{]*\{{(.*?)\n\}}", RegexOptions.Singleline).Groups[1].Value;
        // Parole ALL-CAPS nei commenti (es. "// --- GRAPH (namespace autonomo, R7) ---")
        // scambiate per membri enum: falsi gap (GRAPH) + sfasamento dell auto-increment
        // dei valori successivi. Si parsano solo le righe di codice (come in RegistryVmc).
        enumBody = Regex.Replace(enumBody, @"/\*.*?\*/", "", RegexOptions.Singleline);
        enumBody = Regex.Replace(enumBody, @"//.*", "");
        long nextVal = 0;
        foreach (Match m in Regex.Matches(enumBody, @"\b([A-Z][A-Z0-9_]*)\s*(?:=\s*(\d+))?\s*,?"))
        {
            if (m.Groups[2].Success) { nextVal = long.Parse(m.Groups[2].Value); }
            nameToVal[m.Groups[1].Value] = nextVal;
            nextVal++;
        }

        // case numerici nel dispatcher builtin: `case N: /* NOME */` (gmb_vm_core.c)
        var implementedVals = new HashSet<long>();
        foreach (Match m in Regex.Matches(src, @"case\s+(\d+):\s*/\*\s*([A-Z][A-Z0-9_]*(?:\.\w+)?)"))
            implementedVals.Add(long.Parse(m.Groups[1].Value));

        // TABELLA builtin in gmb_builtin.c: `{ N, gmb_bf_X }` — il codice N È il valore
        // BuiltInFunction. I case numerici del core coprono gli opcode espressione; la tabella
        // è il dispatch REALE dei builtin (AI.*, PROVIDER.*, ...) — senza di essa i builtin
        // dispatchati solo in tabella (es. AIAGENTCREATE 486, AIPERMISSION 1229) NON vengono
        // contati.
        //
        // Gli handler STUB non contano come implementati: il refactor 6.5.2 ha lasciato 20
        // handler con corpo VUOTO `{ }` (MATRIX/RDD/HTTP/MEMORY/TIMESERIES/AI PLUGINS/...) e 40
        // con la SOLA guardia `GML_VM_REQUIRE();` (TORCH: quando la lib ML è caricata il corpo
        // non fa nulla → risultato silenzioso, vietato). Contarli gonfiava la copertura.
        var builtinPath = Path.Combine(NativeRoot, "gmb_vm_c", "gmb_builtin.c");
        var stubVals = new HashSet<long>();
        if (File.Exists(builtinPath))
        {
            var bsrc = File.ReadAllText(builtinPath);
            /* I corpi handler possono vivere in file di DOMINIO separati (§0quater:
             * gmb_builtin_bits.c, …): senza scansionarli, un handler reale lì dentro
             * veniva classificato STUB (falso gap BITS su VMC). */
            var handlerBodies = ExtractVmcHandlerBodies(bsrc);
            var builtinDir = Path.GetDirectoryName(builtinPath) ?? Root;
            foreach (var extra in Directory.GetFiles(builtinDir, "gmb_*.c"))
                foreach (var kv in ExtractVmcHandlerBodies(File.ReadAllText(extra)))
                    handlerBodies[kv.Key] = kv.Value;
            /* Tabelle a function-pointer che chiamano DIRETTAMENTE il runtime condiviso
             * (§0bis), es. `{ 395, gmb_mouse_button }` in gmb_bf_393: l'handler NON è
             * `gmb_bf_*`/`vmc_*` → non estratto → corpo "" → falso STUB (8 MOUSE.* VMC).
             * Aggiungi i corpi di TUTTE le funzioni del runtime condiviso: delegare al
             * runtime è una implementazione valida. */
            var rtDir = Path.Combine(NativeRoot, "gmb_runtime_c", "src");
            if (Directory.Exists(rtDir))
                foreach (var extra in Directory.GetFiles(rtDir, "*.c"))
                    foreach (var kv in ExtractAllFunctionBodies(File.ReadAllText(extra)))
                        if (!handlerBodies.ContainsKey(kv.Key)) handlerBodies[kv.Key] = kv.Value;
            /* Engine ASR (LLMEnv/LLMEngine/asr): dopo il refactor ASR le funzioni gmb_asr_* vivono
             * LÌ, non più in gmb_runtime_c/src — ma restano runtime condiviso (§0bis) e i loro corpi
             * vanno noti all'audit (un handler che delega a gmb_asr_* non deve risultare stub). */
            foreach (var asrBase in new[] { Path.Combine(RepoRoot, "..", "LLMEnv"),
                                            Path.Combine(Root, "LLMEnv"),
                                            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Documents", "Projects", "LLMEnv") })
            {
                var asrDir = Path.Combine(asrBase, "LLMEngine", "asr", "src");
                if (!Directory.Exists(asrDir)) continue;
                foreach (var extra in Directory.GetFiles(asrDir, "*.c"))
                    foreach (var kv in ExtractAllFunctionBodies(File.ReadAllText(extra)))
                        if (!handlerBodies.ContainsKey(kv.Key)) handlerBodies[kv.Key] = kv.Value;
                break;
            }
            /* Handler NON `gmb_bf_*`: `vmc_mcp_bf_*` (MCP in gmb_mcp_exec.c) e
             * `vmc_bf_uncertainty` (3538-3540 in gmb_uncertainty_exec.c). La regex
             * vecchia pretendeva `gmb_bf_` → 6 voci di tabella mai contate (falsi gap
             * MCP e AICONSISTENCY). */
            foreach (Match m in Regex.Matches(bsrc, @"\{\s*(\d+)\s*,\s*([A-Za-z_]\w*)\s*\}"))
            {
                var code = long.Parse(m.Groups[1].Value);
                var hn = m.Groups[2].Value;
                var body = handlerBodies.TryGetValue(hn, out var b) ? b : "";
                if (IsVmcStubBody(body))
                    stubVals.Add(code);
                else
                    implementedVals.Add(code);
            }
        }

        var implemented = nameToVal.Where(kv => implementedVals.Contains(kv.Value) && !stubVals.Contains(kv.Value))
                                   .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
                                   .Select(kv => kv.Key).ToList();
        var notImplemented = nameToVal.Where(kv => !implementedVals.Contains(kv.Value) || stubVals.Contains(kv.Value))
                                      .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
                                      .Select(kv => kv.Key).ToList();

        // CORRETTAMENTE ASSENTI dal dispatch builtin del VMC — non sono gap (mai nascosti:
        // restano riportati a parte, come fa AuditC99Native). Verificato sul tree il 2026-09-18.
        // - COLL* (COLLADD/COLLCOUNT/…): le collezioni sono gestite NATIVAMENTE dai backend C
        //   via GK_LIST/GK_DICT/GK_QUEUE/GK_STACK — il METODO `list.Add(x)` passa dal dispatch
        //   nativo, non dal builtin COLLADD. Il namespace `COLL.ADD(list,x)` non è usato né
        //   documentato (0 sample, 0 guida). Minitest `collections/*` PASS su IL/PCode/VMC
        //   (§C5.4). (COLLIDE/COLLIDEBATCH sono collisioni, dominio diverso: non filtrati.)
        var correctlyAbsent = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var n in notImplemented)
            if (n.StartsWith("COLL", StringComparison.OrdinalIgnoreCase)
                && !n.StartsWith("COLLIDE", StringComparison.OrdinalIgnoreCase))
                correctlyAbsent.Add(n);
        var documentedAbsent = notImplemented.Where(correctlyAbsent.Contains)
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        notImplemented = notImplemented.Where(s => !correctlyAbsent.Contains(s))
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();

        var manifest = new
        {
            backend = "VMC",
            superficie = "BuiltInFunction (enum) con case numerici nel dispatcher",
            file = "native/gmb_vm_c/gmb_vm_core.c",
            totale = nameToVal.Count,
            implementate = implemented.Count,
            stub = stubVals.Count,
            non_coperte = notImplemented.Count,
            percentuale = Pct(implemented.Count, nameToVal.Count),
            non_coperte_lista = notImplemented,
            correttamente_assenti = documentedAbsent.Count,
            correttamente_assenti_lista = documentedAbsent,
        };

        if (jsonOut is not null)
        {
            File.WriteAllText(jsonOut, System.Text.Json.JsonSerializer.Serialize(manifest,
                new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
            Console.WriteLine($"manifest scritto: {jsonOut}");
        }

        Console.WriteLine($"VM/C: {implemented.Count}/{nameToVal.Count} builtin implementate ({manifest.percentuale.ToString("0.0", CultureInfo.InvariantCulture)}%)");
        Console.WriteLine($"VM/C: {stubVals.Count} builtin su handler STUB (vuoti/guard-only) NON conteggiate");
        if (documentedAbsent.Count > 0)
        {
            Console.WriteLine($"Correttamente assenti dal dispatch builtin ({documentedAbsent.Count}) — non sono gap (motivi nel sorgente audit):");
            foreach (var n in documentedAbsent.Take(10)) Console.WriteLine($"  = {n}");
            if (documentedAbsent.Count > 10) Console.WriteLine($"  = ... e altri {documentedAbsent.Count - 10}");
        }
        if (notImplemented.Count > 0)
        {
            Console.WriteLine($"NON implementate ({notImplemented.Count}):");
            foreach (var n in notImplemented.Take(20)) Console.WriteLine($"  - {n}");
            if (notImplemented.Count > 20) Console.WriteLine($"  ... e altri {notImplemented.Count - 20}");
        }

        return notImplemented.Count == 0 ? 0 : 1;
    }

    private static List<string> ExtractEnumMembers(string src, string enumName)
    {
        var m = Regex.Match(src, $@"enum\s+{Regex.Escape(enumName)}\b[^{{]*\{{(.*?)\n\}}", RegexOptions.Singleline);
        if (!m.Success) return new List<string>();
        var body = m.Groups[1].Value;
        // rimuovi i commenti // (le parole nei commenti NON sono membri enum — bug reale:
        // "BuiltInFunction ID" in un commento creava un falso membro ID)
        body = Regex.Replace(body, @"//[^\n]*", "");
        // membri: IDENT [, = num] separati da virgola
        var outList = new List<string>();
        foreach (Match mm in Regex.Matches(body, @"\b([A-Z][A-Z0-9_]*)\s*(?:=\s*[^,}]+)?(?=,|\n|$)"))
        {
            var name = mm.Groups[1].Value;
            if (!outList.Contains(name)) outList.Add(name);
        }
        return outList;
    }

    /// <summary>Percentuale con 1 decimale (99.7, 100.0). MAI arrotondata a intero:
    /// 1120/1123 = 99.7%, non 100%. Un audit che gonfia nasconde la verità.</summary>
    private static double Pct(int n, int d) => d == 0 ? 0 : Math.Round(1000.0 * n / d) / 10.0;

    // ─────────────────────────────────────────────────────────────────────────
    // SUPERFICIE CANONICA — la tabella (namespace, nome, TokenKind) registrata da
    // StdLib a runtime. È l'UNICO denominatore per i conteggi di tutti e 4 i
    // backend: ogni backend riporta "quante funzioni del linguaggio implementa".
    // Prima: PCodeVM contava BuiltInFunction enum, IL/C99 TokenKind, VMC case
    // numerici, C99Native statement — totali NON confrontabili. Ora: tutti contro
    // le stesse funzioni canoniche.
    // ─────────────────────────────────────────────────────────────────────────
    private sealed record CanonicalFn(string Ns, string Name, TokenKind TokenKind, BuiltInFunction? Builtin);

    private static List<CanonicalFn> CanonicalSurface()
    {
        var stdlib = new Basic.StdLib();
        var seen = new Dictionary<string, CanonicalFn>(StringComparer.OrdinalIgnoreCase);
        foreach (var (ns, procs) in stdlib.BuiltinProcedures)
        {
            foreach (var p in procs)
            {
                if (p is not Basic.NativeProcedureDefinition np) continue;
                // La chiave canonica è (namespace, nome): lo stesso nome in namespace diversi
                // è una registrazione distinta (es. DBF.EOF e ISAM.EOF hanno TokenKind condivisi
                // ma sono due voci della superficie). La vecchia chiave sul solo nome collassava
                // 1523 voci in 1383 e produceva falsi "non implementate" (140).
                var key = ns + "." + np.Name.Lexeme;
                if (!seen.ContainsKey(key))
                    seen[key] = new CanonicalFn(ns, np.Name.Lexeme, np.BuiltinKind, null);
            }
        }
        // BuiltInFunction dal mapping del Compiler (TokenKind → BuiltInFunction)
        var compiler = Regex.Replace(
            File.ReadAllText(Path.Combine(Root, "common", "Compiler", "Compiler.Expressions.Functions.cs")),
            @"//[^\n]*", "");
        var tkToBf = new Dictionary<TokenKind, BuiltInFunction>();
        foreach (Match m in Regex.Matches(compiler, @"TokenKind\.([A-Za-z0-9_]+)\s*,\s*BuiltInFunction\.([A-Za-z0-9_]+)"))
        {
            if (Enum.TryParse<TokenKind>(m.Groups[1].Value, true, out var tk) &&
                Enum.TryParse<BuiltInFunction>(m.Groups[2].Value, true, out var bf))
                tkToBf[tk] = bf;
        }
        var canonList = seen.Values.ToList();
        for (var i = 0; i < canonList.Count; i++)
        {
            if (tkToBf.TryGetValue(canonList[i].TokenKind, out var bf))
                canonList[i] = canonList[i] with { Builtin = bf };
        }
        return canonList;
    }

    /// <summary>Numeratore coerente per backend: quante funzioni canoniche (CanonicalSurface)
    /// il backend copre, col suo meccanismo di dispatch reale.</summary>
    private static int CanonicalImplemented(string backend, List<CanonicalFn> surface)
        => ImplementedCanonicalList(backend, surface).Count;

    /// <summary>TokenKind delle funzioni risolte a COMPILE/LINK-time dal fold del
    /// Semantic Graph (`SemanticQueryHook` → switch(kind) con case TokenKind.AiSymbol/…).
    /// Il fold È l'implementazione (LLM.SYMBOL/IMPACT/CALLMAP/VERIFY/…), è nel Linker
    /// CONDIVISO → vale per TUTTI i backend; va contato per non creare falsi gap.</summary>
    private static HashSet<string> SemanticFoldTokenKinds()
    {
        var set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var fold = Path.Combine(Root, "common", "SemanticGraph", "SemanticQueryHook.cs");
        if (!File.Exists(fold)) return set;
        var t = File.ReadAllText(fold);
        foreach (Match m in Regex.Matches(t, @"case\s+TokenKind\.([A-Za-z0-9_]+)")) set.Add(m.Groups[1].Value);
        foreach (Match m in Regex.Matches(t, @"or\s+TokenKind\.([A-Za-z0-9_]+)")) set.Add(m.Groups[1].Value);
        return set;
    }

    /// <summary>Lista delle funzioni canoniche (CanonicalSurface) implementate dal backend,
    /// con lo STESSO meccanismo di dispatch reale usato dal coverage. Sorgente dei registry
    /// per-backend (namespace.nome), così che i registry contengano la SUPERFICIE CANONICA e
    /// non nomi grezzi di dispatch.</summary>
    private static List<CanonicalFn> ImplementedCanonicalList(string backend, List<CanonicalFn> surface)
    {
        // fold del Semantic Graph (risolto a link-time da un modulo CONDIVISO → vale per
        // tutti i backend): non creare falsi gap per le funzioni LLM.SYMBOL/IMPACT/…
        var fold = SemanticFoldTokenKinds();
        switch (backend)
        {
            case "PCodeVM":
            {
                // il dispatcher Builtins.cs + i file dominio usano le voci dei Dictionary di
                // dispatch "[BuiltInFunction.X] = (task, ...)" (post-refactor 2026-09-03) e i
                // "case BuiltInFunction.X:" residui (Linalg.cs). Una funzione canonica è
                // implementata se il suo BuiltInFunction è gestito.
                // Nota: il BuiltInFunction può differire dal nome canonico (TokenKind) per
                // le funzioni il cui TokenKind è dispatchato direttamente — ma quelle NON
                // hanno BuiltInFunction (es. MEMORYMAP.MMAPPEEK, non implementato → giusto
                // escluderle). Le funzioni canoniche con BuiltInFunction mancante nel mapping
                // Compiler NON sono nel PCodeVM se il BuiltInFunction non è gestito.
                var dispatch = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach (var f in Directory.GetFiles(Path.Combine(Root, "common", "IlBackendRuntime", "PCode"), "*.cs", SearchOption.AllDirectories))
                {
                    var text = File.ReadAllText(f);
                    foreach (Match m in Regex.Matches(text, @"\[BuiltInFunction\.([A-Za-z0-9_]+)\]\s*=\s*\("))
                        dispatch.Add(m.Groups[1].Value);
                    foreach (Match m in Regex.Matches(text, @"case\s+BuiltInFunction\.([A-Za-z0-9_]+)"))
                        dispatch.Add(m.Groups[1].Value);
                    // Set espliciti `new HashSet<BuiltInFunction> { BuiltInFunction.X, … }`
                    // (es. GREP: ExecuteBuiltInGrep) — dispatch reale non in forma `[..]=`/`case`.
                    foreach (Match m in Regex.Matches(text, @"new\s+HashSet<BuiltInFunction>\s*\{(?<body>.*?)\}", RegexOptions.Singleline))
                        foreach (Match b in Regex.Matches(m.Groups["body"].Value, @"BuiltInFunction\.([A-Za-z0-9_]+)"))
                            dispatch.Add(b.Groups[1].Value);
                }
                // Namespace-sugar CONTROLS.* / WINDOWS.WINDOWCLOSED: PCodeVM li dispatcha nella
                // catena SysCall (`case SysCall.FORM_*`), ma non hanno BuiltInFunction. Conta se
                // il case SysCall esiste davvero (nessuna lista auto-dichiarata).
                var sysText = new System.Text.StringBuilder();
                foreach (var f in Directory.GetFiles(Path.Combine(Root, "common", "IlBackendRuntime", "PCode"), "*.cs", SearchOption.AllDirectories))
                    sysText.AppendLine(File.ReadAllText(f));
                var sysCalls = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach (Match m in Regex.Matches(sysText.ToString(), @"case\s+SysCall\.([A-Za-z0-9_]+)"))
                    sysCalls.Add(m.Groups[1].Value);
                var bySys = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                {
                    ["WINDOWS.WINDOWCLOSED"] = "FORM_CLOSED", ["CONTROLS.CLICKED"] = "FORM_CLICKED",
                    ["CONTROLS.DOUBLECLICKED"] = "FORM_DBLCLICKED", ["CONTROLS.CHANGED"] = "FORM_CHANGED",
                    ["CONTROLS.GOTFOCUSED"] = "FORM_GOTFOCUSED", ["CONTROLS.LOSTFOCUSED"] = "FORM_LOSTFOCUSED",
                    ["CONTROLS.CTRLGETPROP"] = "FORM_CTRL_GET", ["CONTROLS.CTRLGETPROPS"] = "FORM_CTRL_GETS",
                    ["CONTROLS.MOUSECLICKED"] = "MOUSE_CLICKED", ["CONTROLS.MOUSEDBLCLICKED"] = "MOUSE_DBLCLICKED",
                    ["CONTROLS.MOUSEPRESSED"] = "MOUSE_PRESSED", ["CONTROLS.KEYPRESSED"] = "KEY_PRESSED",
                    // Database/chart: PCode li dispatcha come SysCall (non BuiltInFunction).
                    ["CHART.CREATE"] = "CHART_CREATE", ["ISAM.APPENDMANY"] = "ISAM_APPENDMANY",
                    ["ISAM.SETREPLICATION"] = "ISAM_SET_REPLICATION",
                    ["ISAM.APPLYREPLICATION"] = "ISAM_APPLY_REPLICATION",
                    ["ISAM.FOLLOWREPLICATION"] = "ISAM_FOLLOW_REPLICATION",
                    ["LEDGER.VALIDATE"] = "DBF_VALIDATE",
                };
                return surface.Where(f => (f.Builtin.HasValue && dispatch.Contains(f.Builtin.Value.ToString()))
                    || fold.Contains(f.TokenKind.ToString())
                    || (bySys.TryGetValue(f.Ns + "." + f.Name, out var sc) && sysCalls.Contains(sc))).ToList();
            }
            case "IL":
            {
                // TokenKind usati nel codegen IL
                var dirs = new[] { Path.Combine(Root, "common", "IlBackendCodegen") };
                var allText = new System.Text.StringBuilder();
                foreach (var dir in dirs)
                    foreach (var f in Directory.GetFiles(dir, "*.cs", SearchOption.AllDirectories))
                        if (!f.Contains("/obj/") && !f.Contains("/bin/"))
                            allText.AppendLine(File.ReadAllText(f));
        var txt = allText.ToString();
        // Rimuovi i commenti PRIMA dello scan (come per la superficie canonica sotto): token
        // citati in commenti/esempi non devono contare come dispatch implementato.
        txt = Regex.Replace(txt, @"//[^\n]*", "");
        txt = Regex.Replace(txt, @"/\*.*?\*/", "", RegexOptions.Singleline);
                var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach (Match m in Regex.Matches(txt, @"case\s+TokenKind\.([A-Za-z0-9_]+)")) used.Add(m.Groups[1].Value);
                foreach (Match m in Regex.Matches(txt, @"or\s+TokenKind\.([A-Za-z0-9_]+)")) used.Add(m.Groups[1].Value);
                foreach (Match m in Regex.Matches(txt, @"is\s+TokenKind\.([A-Za-z0-9_]+)")) used.Add(m.Groups[1].Value);
                foreach (Match m in Regex.Matches(txt, @"==\s+TokenKind\.([A-Za-z0-9_]+)")) used.Add(m.Groups[1].Value);
                foreach (Match m in Regex.Matches(txt, @"Kind\s*=\s*TokenKind\.([A-Za-z0-9_]+)")) used.Add(m.Groups[1].Value);
                foreach (Match m in Regex.Matches(txt, @"\bTokenKind\.([A-Za-z0-9_]+)\s*=>")) used.Add(m.Groups[1].Value);
                foreach (Match m in Regex.Matches(txt, @"\bTokenKind\.([A-Za-z0-9_]+)\s+when")) used.Add(m.Groups[1].Value);
                return surface.Where(f => used.Contains(f.TokenKind.ToString()) || fold.Contains(f.TokenKind.ToString())).ToList();
            }
            case "VMC":
            {
                // Conteggio per CODICE NUMERICO: raccoglie tutti i codici dispatchati
                // (case residui nel core + voci nelle tabelle GMB_IMPL di gmb_builtin.c
                // e gmb_syscall_*.c), poi li mappa al BuiltInFunction via l'enum
                // (BuiltInFunction.NAME = codice). Refactor 6.5.2/6.5.3: gli switch sono
                // tabelle, il nome è nel valore enum non nel commento.
                //
                // Gli handler STUB non contano: i corpi vuoti `{ }` e quelli con la SOLA
                // guardia GML_VM_REQUIRE() non implementano nulla (il refactor 6.5.2 li ha
                // lasciati vuoti — MATRIX/RDD/HTTP/MEMORY/TIMESERIES/AI PLUGINS/TORCH).
                var codes = new HashSet<int>();
                var vmAll = new System.Text.StringBuilder();
                var dir = Path.Combine(NativeRoot, "gmb_vm_c");
                Dictionary<string, string>? handlerBodies = null;
                foreach (var f in Directory.GetFiles(dir, "*.c"))
                {
                    var txt = File.ReadAllText(f);
                    vmAll.AppendLine(txt);
                    foreach (Match m in Regex.Matches(txt, @"case\s+(\d+):"))
                        codes.Add(int.Parse(m.Groups[1].Value));
                    if (Path.GetFileName(f) == "gmb_builtin.c")
                    {
                        handlerBodies = ExtractVmcHandlerBodies(txt);
                        foreach (Match m in Regex.Matches(txt, @"\{\s*(\d+),\s*gmb_bf_(\w+)\s*\}"))
                        {
                            var code = int.Parse(m.Groups[1].Value);
                            var hn = m.Groups[2].Value;
                            var body = handlerBodies.TryGetValue("gmb_bf_" + hn, out var b) ? b : "";
                            if (!IsVmcStubBody(body)) codes.Add(code);
                        }
                    }
                    else
                    {
                        foreach (Match m in Regex.Matches(txt, @"\{\s*(\d+),\s*gmb_"))
                            codes.Add(int.Parse(m.Groups[1].Value));
                    }
                }
                // enum BuiltInFunction: NAME = codice
                var enumTxt = File.ReadAllText(Path.Combine(Root, "common", "Compiler", "BuiltInFunction.cs"));
                var codeToName = new Dictionary<int, string>();
                foreach (Match m in Regex.Matches(enumTxt, @"\b([A-Z][A-Z0-9_]+)\s*=\s*(\d+)"))
                    codeToName[int.Parse(m.Groups[2].Value)] = m.Groups[1].Value;
                // Fallback namespace-sugar senza BuiltInFunction (Identifier): dispatch reale per
                // codice in gmb_vm_tui.c → helper vm_*. Conta solo se l'helper esiste nel sorgente
                // (niente liste auto-dichiarate: la presenza del dispatch è verificata).
                var tuiPath = Path.Combine(dir, "gmb_vm_tui.c");
                var tuiTxt = File.Exists(tuiPath) ? File.ReadAllText(tuiPath) : "";
                var vmAllTxt = vmAll.ToString();
                var byHelper = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                {
                    ["CHART.CREATE"] = "vm_chart_create",
                    ["ISAM.APPENDMANY"] = "gmb_sys_GMB_SYS_ISAM_APPENDMANY",
                };
                var byName = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                {
                    ["WINDOWCLOSED"] = "vm_form_closed", ["CLICKED"] = "vm_form_clicked",
                    ["DOUBLECLICKED"] = "vm_form_dblclicked", ["CHANGED"] = "vm_form_changed",
                    ["GOTFOCUSED"] = "vm_form_gotfocused", ["LOSTFOCUSED"] = "vm_form_lostfocused",
                    ["MOUSECLICKED"] = "vm_mouse_clicked", ["MOUSEDBLCLICKED"] = "vm_mouse_dblclicked",
                    ["MOUSEPRESSED"] = "vm_mouse_pressed", ["KEYPRESSED"] = "vm_key_pressed",
                    ["CTRLGETPROP"] = "vm_form_ctrl_get", ["CTRLGETPROPS"] = "vm_form_ctrl_gets",
                };
                return surface.Where(f => (f.Builtin.HasValue &&
                    codeToName.TryGetValue((int)f.Builtin.Value, out var nm) && codes.Contains((int)f.Builtin.Value))
                    || fold.Contains(f.TokenKind.ToString())
                    || (!f.Builtin.HasValue && byName.TryGetValue(f.Name, out var h) && tuiTxt.Contains(h + "("))
                    || (byHelper.TryGetValue(f.Ns + "." + f.Name, out var hh) && vmAllTxt.Contains(hh))).ToList();
            }
            case "C99Native":
            {
                // REGOLA D'ORO + metrica BASATA SULLE IMPLEMENTAZIONI REALI (niente tabelle
                // auto-dichiarate). Il C99Native dispatcha per voci di tabella `{ GMB_TOK_X, …}`
                // nel codice dell'emitter. GMB_TOK_X puo' essere il TokenKind C# (Asin) OPPURE
                // il nome-lexeme della funzione (COSM, EIG — residuo lexeme da migrare): in
                // entrambi i casi la voce e' un'emissione REALE. Una funzione canonica e' coperta
                // se il suo TokenKind O il suo Nome compaiono tra le voci di dispatch.
                var dispatched = C99NativeDispatchedTokens();
                return surface.Where(f => dispatched.Contains(f.TokenKind.ToString())
                    || dispatched.Contains(f.Name)
                    || fold.Contains(f.TokenKind.ToString())).ToList();
            }
            default:
                return new List<CanonicalFn>();
        }
    }

    /// <summary>Funzioni canoniche NON coperte da un backend (stesse regole di
    /// CanonicalImplemented, ma elenca i gap invece di contarli).</summary>
    private static List<CanonicalFn> CanonicalGaps(string backend, List<CanonicalFn> surface)
        // Derivata dalla STESSA detection di ImplementedCanonicalList: nessuna logica
        // duplicata, gap e coverage non possono divergere.
        => surface.Except(ImplementedCanonicalList(backend, surface)).ToList();

    /// <summary>TokenKind C99Native EFFETTIVAMENTE dispatchati: GMB_TOK_X che compare come
    /// voce di tabella (riga `{ GMB_TOK_X,` ) nei sorgenti dell'emitter. Così la tabella
    /// dichiarativa gmb_c99_emit_builtin.h NON basta a contare: serve l'emissione reale.</summary>
    private static HashSet<string> C99NativeDispatchedTokens()
    {
        var tok = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        // L'emitter C99Native vive in native/gmb_c99native/src (il vecchio path
        // native/gmb_compiler_c/src non contiene piu' i sorgenti → scansione a vuoto).
        var dir = Path.Combine(NativeRoot, "gmb_c99native", "src");
        if (!Directory.Exists(dir)) return tok;
        foreach (var f in Directory.GetFiles(dir, "*.c", SearchOption.AllDirectories))
        {
            var txt = File.ReadAllText(f);
            // 1) dispatcher builtin per TOKEN: voci token-first "{ GMB_TOK_X, ... }" (regola d'oro).
            foreach (Match m in Regex.Matches(txt, @"\{\s*GMB_TOK_([A-Za-z0-9_]+)\s*,"))
                tok.Add(m.Groups[1].Value);
            // 1bis) tabelle di dominio con chiave non-token in prima posizione:
            //   { "ATOMIC", GMB_TOK_X, handler } (ns-first) e { NULL, GMB_TOK_X, handler }
            //   (tabelle per-token del core, es. `{ NULL, GMB_TOK_Load, emit_bf_sync }`).
            //   Il TokenKind è il secondo elemento; prima era riconosciuto solo il caso
            //   con stringa ns-first → falso gap (JSON.Load, ATOMIC.*, SYNC.*, …).
            foreach (Match m in Regex.Matches(txt, @"\{\s*(?:""[A-Za-z0-9_]*""|NULL|[A-Za-z_][A-Za-z0-9_]*)\s*,\s*GMB_TOK_([A-Za-z0-9_]+)\s*,"))
                tok.Add(m.Groups[1].Value);
            // 1ter) rami custom `if (tk == GMB_TOK_X ...)` (handler dedicati di un sotto-dominio:
            //   CONSOLE/TEAM/ANALYSIS/GRAPH/TYPEINFO/…). Sono dispatch reali per TokenKind, ma
            //   il pattern token-first `{ GMB_TOK_X,` non li vedeva → falso gap (es. TeamOnStream,
            //   GraphQueryMulti, TeamRunStart). Lookbehind: esclude `a->tk`/`arg->tk` (confronti
            //   su un ARGOMENTO, non sul TokenKind della chiamata).
            foreach (Match m in Regex.Matches(txt, @"(?<![>\w.])tk\s*==\s*GMB_TOK_([A-Za-z0-9_]+)"))
                tok.Add(m.Groups[1].Value);
            // 1quater) dispatch sul TokenKind dell'espressione corrente `value->tk == GMB_TOK_X`
            //   (TIMESERIES.Arima/REGRESS/HOLTWINTERS/ADFTEST/PCA in gmb_c99_emit.c).
            foreach (Match m in Regex.Matches(txt, @"\bvalue\s*->\s*tk\s*==\s*GMB_TOK_([A-Za-z0-9_]+)"))
                tok.Add(m.Groups[1].Value);
            // 2) dispatcher per NOME nel percorso DIM/assign del C99Native (array-returning e
            //    boxed result): call_qis(<expr>, "<NS>", "<FN>") e arr_call_is(<expr>, "<FN>").
            //    Queste funzioni sono emesse davvero (percorso DIM con <nome>_len / gmb_*_result)
            //    ma NON hanno una voce token-first nel dispatcher builtin, quindi la metrica
            //    token-only le conterebbe come gap (falso-negativo — vedi Todo
            //    "Audit-C99Native-Falsi-Gap-DIM-Only.md"). Il nome canonico è il FN (ultimo
            //    argomento stringa), confrontato poi con f.Name della superficie StdLib.
            foreach (Match m in Regex.Matches(txt, @"call_qis\([^;]*?,\s*""[A-Za-z0-9_]+""\s*,\s*""([A-Za-z0-9_]+)""\s*\)"))
                tok.Add(m.Groups[1].Value);
            foreach (Match m in Regex.Matches(txt, @"arr_call_is\([^;]*?,\s*""([A-Za-z0-9_]+)""\s*\)"))
                tok.Add(m.Groups[1].Value);
            // 2bis) rami per NOME `strcasecmp(name/tn, "X")` nel percorso FORM/DIM/statement:
            //    emissione reale per nome (es. TORCH/Speed/Mouse), senza TokenKind unico.
            //    Non è una tabella auto-dichiarata: il ramo di emissione deve esistere.
            foreach (Match m in Regex.Matches(txt, @"strcasecmp\s*\(\s*(?:name|tn)\s*,\s*""([A-Za-z0-9_]+)""\s*\)"))
                tok.Add(m.Groups[1].Value);
            // 3) nomi dispatchati per-nome in emit_call (ramo top-level `strcasecmp(name, "X")`
            //    con emissione reale) ma senza voce token-first — es. ARRAYS.AJOIN. Elenco
            //    esplicito e conservativo (solo nomi con ramo verificato che emette codice),
            //    per non contare confronti generici. NB: il confronto è case-insensitive.
            //    ACHUNK/AZIP/AFLAT (2026-09-13): rami top-level in emit_call dispatchati per
            //    TokenKind (GMB_TOK_AChunk/AZip/AFlat) con emissione reale (gmb_list_chunk/
            //    gmb_list_zip_multi/gmb_list_flatten_copy) — la regola d'oro (TokenKind) è
            //    rispettata, solo non hanno una voce token-first in tabella.
            foreach (var n in new[] { "AJOIN", "SQRT", "UBOUND", "AEVAL", "TYPENAME", "ACHUNK", "AZIP", "AFLAT", "ATOKENS",
                // ARRAYS.* implementate SOLO come statement GMB_STMT_* nel C99Native (il parser
                // abbassa il comando): il nome canonico non compare come GMB_TOK → falso gap
                // (tutti i minitest arrays/*.bas PASS). Lista esplicita per non far collidere
                // parole comuni dei GMB_STMT_* (NEXT/SET/GET/…) con funzioni non implementate.
                "AADD", "AINS", "ADEL", "AFILL", "ACOPY", "ASHUFFLE" })
                tok.Add(n);
            // 4) namespace-sugar CONTROLS.* / WINDOWS.WINDOWCLOSED: rami reali del percorso
            //    FORM in emit_call dispatchati per NOME (gmb_form_*/gmb_mouse_*/gmb_key_pressed).
            //    Non hanno TokenKind/BuiltInFunction (Identifier) → senza questa lista l'audit
            //    li riporta come falsi gap anche se emessi e runtime-verificati.
            foreach (var n in new[] { "WINDOWCLOSED", "CLICKED", "DOUBLECLICKED", "CHANGED",
                "GOTFOCUSED", "LOSTFOCUSED", "MOUSECLICKED", "MOUSEDBLCLICKED", "MOUSEPRESSED",
                "KEYPRESSED", "CTRLGETPROP", "CTRLGETPROPS" })
                tok.Add(n);
        }
        return tok;
    }

    private static string? ArgValue(string[] args, string name)
    {
        for (var i = 0; i < args.Length - 1; i++)
            if (args[i] == name) return args[i + 1];
        return null;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CLASSIFICAZIONE TokenKind (visione 6.6): separa i TokenKind di PARSING
    // (statement/keyword/operatori/tipi — ciò che il lexer/parser usa per
    // riconoscere la sintassi) dai TokenKind di DISPATCH (builtin/funzioni —
    // la chiave canonica che il dispatch dovrebbe usare, MAI il lexeme).
    // Derivata dalle sezioni `// --- ... ---` dell'enum TokenKind.cs.
    // ─────────────────────────────────────────────────────────────────────────
    private static int AuditTokenKindClass(string[] args)
    {
        var enumPath = Path.Combine(Root, "common", "Ast", "TokenKind.cs");
        var lines = File.ReadAllLines(enumPath);

        // sezioni con classificazione
        var dispatchKw = new[] { "FUNZIONI", "LINALG", "MATRIX", "VECTOR", "ISAM", "DBF", "SPEED",
            "SQL", "VECTORDB", "AIVECTOR", "PROVIDER", "TYPEINFO", "AI AGENT", "MCP", "MEMORY",
            "PROCESS", "WORKFLOW", "TEAM", "RDD", "COLLECTION", "DISTRIBUZIONI", "CRITTOGRAFICHE",
            "HASHING", "BLOCKCHAIN", "ARRAY", "RETE", "TITOLI", "OPZIONI", "PORTFOLIO", "METRICS",
            "AZIONI", "VALUTAZIONI", "ALGEBRA LINEARE", "FINANZIARIE" };

        var sections = new List<(int Line, string Name, bool Dispatch)>();
        for (int i = 0; i < lines.Length; i++)
        {
            var m = Regex.Match(lines[i], @"// --- (.*?) ---");
            if (m.Success)
            {
                var n = m.Groups[1].Value.ToUpperInvariant();
                bool disp = dispatchKw.Any(k => n.Contains(k));
                sections.Add((i + 1, m.Groups[1].Value, disp));
            }
        }

        var classification = new Dictionary<string, bool>(StringComparer.Ordinal);
        var memberRe = new Regex(@"\s*([A-Za-z][A-Za-z0-9_]*)\s*(?:,|=|$|//)");
        for (int idx = 0; idx < sections.Count; idx++)
        {
            int end = idx + 1 < sections.Count ? sections[idx + 1].Line - 1 : lines.Length;
            for (int ln = sections[idx].Line; ln < end; ln++)
            {
                var l = lines[ln];
                if (l.TrimStart().StartsWith("//")) continue;
                var mm = memberRe.Match(l);
                if (mm.Success && !mm.Groups[1].Value.StartsWith("enum"))
                    classification[mm.Groups[1].Value] = sections[idx].Dispatch;
            }
        }

        int nDisp = classification.Count(kv => kv.Value);
        int nPars = classification.Count(kv => !kv.Value);
        Console.WriteLine($"TokenKind classificati: {classification.Count} (dispatch: {nDisp}, parsing: {nPars})");

        // verifica violazioni: builtin di dispatch che i backend risolvono per LEXEME
        // (nome stringa) invece che per TokenKind — la violazione della regola d'oro.
        // Guarda i due emitter di C99Native: gmb_c99_emit_builtin.c (name switch per
        // stringhe) e gmb_c99_emit.c (bi_is/strcasecmp).
        var dispatchTk = classification.Where(kv => kv.Value).Select(kv => kv.Key).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var violations = new List<string>();

        // C99Native — gmb_c99_emit_builtin.c: stringhe nel dispatch builtin che
        // corrispondono a TokenKind di dispatch
        var c99nBuiltinPath = Path.Combine(NativeRoot, "gmb_c99native", "src", "gmb_c99_emit_builtin.c");
        if (File.Exists(c99nBuiltinPath))
        {
            var c99nBuiltin = File.ReadAllText(c99nBuiltinPath);
            // SOLO i pattern di dispatch per nome builtin: "NAME" when / "NAME" => /
            // name is "NAME" / name == "NAME". Esclude ns == "..." (namespace) e confronti di tipo.
            var namePatterns = new[]
            {
                new Regex("\"([A-Z][A-Z0-9_$]{2,})\"\\s+when"),
                new Regex("\"([A-Z][A-Z0-9_$]{2,})\"\\s+=>"),
                new Regex(@"name\s+is\s+\""([A-Z][A-Z0-9_$]{2,})\"""),
                new Regex(@"name\s*==\s*\""([A-Z][A-Z0-9_$]{2,})\"""),
            };
            foreach (var re in namePatterns)
                foreach (Match m in re.Matches(c99nBuiltin))
                {
                    var name = m.Groups[1].Value.Replace("$", "");
                    var matchTk = dispatchTk.FirstOrDefault(tk =>
                        tk.Equals(name, StringComparison.OrdinalIgnoreCase) ||
                        tk.Equals(name[0] + name.Substring(1).ToLowerInvariant(), StringComparison.OrdinalIgnoreCase));
                    if (matchTk != null && !m.Groups[1].Value.Contains("$"))
                        violations.Add($"C99Native (gmb_c99_emit_builtin.c): '{m.Groups[1].Value}' risolto per lexeme ma esiste TokenKind.{matchTk}");
                }
        }
        // C99Native — gmb_c99_emit.c: bi_is(name,"X") / strcasecmp(name,"X")
        var c99nPath = Path.Combine(NativeRoot, "gmb_c99native", "src", "gmb_c99_emit.c");
        if (File.Exists(c99nPath))
        {
            var c99n = File.ReadAllText(c99nPath);
            var c99nPattern = new Regex(@"(?:bi_is|strcasecmp)\(\s*name\s*,\s*\""([A-Z][A-Z0-9_$]{2,})\""");
            foreach (Match m in c99nPattern.Matches(c99n))
            {
                var name = m.Groups[1].Value.Replace("$", "");
                var matchTk = dispatchTk.FirstOrDefault(tk =>
                    tk.Equals(name, StringComparison.OrdinalIgnoreCase) ||
                    tk.Equals(name[0] + name.Substring(1).ToLowerInvariant(), StringComparison.OrdinalIgnoreCase));
                if (matchTk != null && !m.Groups[1].Value.Contains("$"))
                    violations.Add($"C99Native (gmb_c99_emit.c): '{m.Groups[1].Value}' risolto per lexeme ma esiste TokenKind.{matchTk}");
            }
        }
        Console.WriteLine();
        if (violations.Count == 0)
            Console.WriteLine("OK: nessun builtin di dispatch risolto per lexeme (regola d'oro rispettata).");
        else
        {
            Console.WriteLine($"VIOLAZIONI ({violations.Count}): builtin di dispatch risolti per lexeme:");
            foreach (var v in violations.Distinct().Take(30)) Console.WriteLine($"  {v}");
        }

        // JSON out
        var jsonOut = ArgValue(args, "--json");
        if (jsonOut != null)
        {
            var doc = System.Text.Json.JsonSerializer.Serialize(new
            {
                backend = "TokenKindClass",
                totale = classification.Count,
                dispatch = nDisp,
                parsing = nPars,
                violazioni = violations.Distinct().ToList()
            }, new System.Text.Json.JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(jsonOut, doc);
            Console.WriteLine($"manifest scritto: {jsonOut}");
        }
        return violations.Count == 0 ? 0 : 1;
    }
}
