// REPL/CliBrain.cs — comandi CLI `gmb brain ...` per gestire i cervelli (Second Brain):
//   gmb brain use    <name> [--path dir]
//   gmb brain detach <name>
//   gmb brain list
//   gmb brain info   <name>
//   gmb brain install <name> <zip-or-url> [--path dir] [--activate]
//   gmb brain export <name> [--out file.zip]
//   gmb brain sign   <name> <dir> [--out file.zip]
// Riusa i servizi in common/AI/Knowledge (BrainRegistry/BrainInstaller/BrainPackager/
// BrainSigner) condivisi col REPL. La costruzione dei cervelli resta a `gmb rag`/`gmb graph`.
using Basic.AI.Knowledge;

namespace Basic.Executor;

public static class CliBrain
{
    public static bool TryDispatch(string[] args)
    {
        if (args.Length == 0) return false;
        if (!args[0].Equals("brain", StringComparison.OrdinalIgnoreCase)) return false;
        var rest = args.Skip(1).ToArray();
        Run(() => Dispatch(rest));
        return true;
    }

    private static void Run(Action fn)
    {
        try { fn(); }
        catch (Exception ex) { Console.Error.WriteLine($"?{ex.Message}"); }
    }

    private static void Dispatch(string[] args)
    {
        var cmd = args.Length > 0 ? args[0].ToLowerInvariant() : "help";
        switch (cmd)
        {
            case "use": Use(args.Skip(1).ToArray()); break;
            case "detach": Detach(args.Skip(1).ToArray()); break;
            case "list": List(); break;
            case "info": Info(args.Skip(1).ToArray()); break;
            case "resolve": Resolve(args.Skip(1).ToArray()); break;
            case "stats": Tool("stats", args.Skip(1).ToArray()); break;
            case "verify": Tool("verify", args.Skip(1).ToArray()); break;
            case "create": Tool("create", args.Skip(1).ToArray()); break;
            case "optimize": Tool("optimize", args.Skip(1).ToArray()); break;
            case "backup": Tool("backup", args.Skip(1).ToArray()); break;
            case "rename": Tool("rename", args.Skip(1).ToArray()); break;
            case "dedupe": Tool("dedupe", args.Skip(1).ToArray()); break;
            case "extract": Tool("extract", args.Skip(1).ToArray()); break;
            case "merge": Tool("merge", args.Skip(1).ToArray()); break;
            case "buildvault": Tool("buildvault", args.Skip(1).ToArray()); break;
            case "register": Tool("register", args.Skip(1).ToArray()); break;
            case "diff": Tool("diff", args.Skip(1).ToArray()); break;
            case "doctor": Tool("doctor", args.Skip(1).ToArray()); break;
            case "convert": Tool("convert", args.Skip(1).ToArray()); break;
            case "install": Install(args.Skip(1).ToArray()); break;
            case "export": Export(args.Skip(1).ToArray()); break;
            case "sign": Sign(args.Skip(1).ToArray()); break;
            default: Help(); break;
        }
    }

    private static void Use(string[] args)
    {
        var name = ArgValue(args, 0);
        if (name is null) { Help(); return; }
        if (!BrainRegistry.Use(name))
        {
            Console.Error.WriteLine($"?cervello '{name}' non disponibile: esegui 'gmb brain install' o 'gmb rag build'/'gmb graph build'");
            return;
        }
        Console.WriteLine($"[OK] brain '{name}' collegato — available: [{string.Join(", ", BrainRegistry.AvailableModes(name))}]");
    }

    private static void Detach(string[] args)
    {
        var name = ArgValue(args, 0);
        if (name is null) { Help(); return; }
        Console.WriteLine(BrainRegistry.Detach(name) ? $"[OK] brain '{name}' scollegato (i file restano)" : $"?brain '{name}' non era collegato");
    }

    private static void List()
    {
        var names = BrainRegistry.List();
        if (names.Length == 0) { Console.WriteLine("(nessun cervello registrato)"); return; }
        var used = BrainRegistry.Used();
        foreach (var n in names)
        {
            var info = BrainRegistry.Info(n);
            var mark = used.Contains(n, StringComparer.OrdinalIgnoreCase) ? "*" : " ";
            var avail = info is null ? "" : string.Join(",", info.Available);
            Console.WriteLine($"{mark} {n}  [{avail}]");
        }
        Console.WriteLine("* = collegato (BRAINUSED)");
    }

    private static void Info(string[] args)
    {
        var name = ArgValue(args, 0);
        if (name is null) { Help(); return; }
        var info = BrainRegistry.Info(name);
        if (info is null) { Console.Error.WriteLine($"?cervello '{name}' non trovato"); return; }
        Console.WriteLine($"name:      {info.Name}");
        Console.WriteLine($"mode:      {info.Mode}");
        Console.WriteLine($"available: [{string.Join(", ", info.Available)}]");
        Console.WriteLine($"checksum:  {info.Checksum ?? "-"}");
        Console.WriteLine($"attached:  {info.Attached}");
        Console.WriteLine($"source:    {info.Source ?? "-"}");
        Console.WriteLine($"docs:      {info.Docs}");
        if (info.Probe.Count > 0)
            foreach (var (k, v) in info.Probe) Console.WriteLine($"probe.{k}: {v}");
    }

    private static void Install(string[] args)
    {
        var name = ArgValue(args, 0);
        var source = ArgValue(args, 1);
        if (name is null || source is null) { Help(); return; }
        var dest = FlagValue(args, "--path");
        var activate = args.Contains("--activate", StringComparer.OrdinalIgnoreCase);
        var result = BrainInstaller.Install(name, source, dest, activate);
        if (!result.Ok)
        {
            Console.Error.WriteLine($"?{result.Error}");
            return;
        }
        Console.WriteLine($"[OK] brain '{name}' installato in {result.InstallRoot} — available: [{string.Join(", ", result.Available)}]");
    }

    private static void Export(string[] args)
    {
        var name = ArgValue(args, 0);
        if (name is null) { Help(); return; }
        var outZip = FlagValue(args, "--out");
        var result = BrainPackager.Export(name, outZip);
        if (result is null)
        {
            Console.Error.WriteLine($"?cervello '{name}' non disponibile: costruiscilo con 'gmb rag build'/'gmb graph build'");
            return;
        }
        Console.WriteLine($"[OK] esportato {result.ZipPath} — available: [{string.Join(", ", result.Available)}]");
    }

    private static void Sign(string[] args)
    {
        var name = ArgValue(args, 0);
        var dir = ArgValue(args, 1);
        if (name is null || dir is null) { Help(); return; }
        var outZip = FlagValue(args, "--out");
        var result = BrainSigner.Sign(name, dir);
        if (!result.Valid)
        {
            Console.Error.WriteLine($"?il cervello '{name}' NON è valido (nessun probe passato):");
            foreach (var (k, v) in result.Probe) Console.Error.WriteLine($"  {k}: {v}");
            return;
        }
        Console.WriteLine($"[OK] brain '{name}' firmato — available: [{string.Join(", ", result.Available)}]");
        foreach (var (k, v) in result.Probe) Console.WriteLine($"  probe.{k}: {v}");
        if (!string.IsNullOrEmpty(outZip))
        {
            if (BrainSigner.ExportSignedZip(name, outZip)) Console.WriteLine($"[OK] zip: {outZip}");
            else Console.Error.WriteLine($"?export zip fallito: {outZip}");
        }
    }

    /// <summary>
    /// `gmb brain resolve &lt;name|alias&gt; [--explain]` — diagnostica READ-ONLY della catena di scope
    /// (F1 di `Knowledge/Brain-Scope-Modello.md`). Stessa logica della REPL (`BRAIN RESOLVE`): non attiva
    /// nulla, non scrive il registry; oggi l'unico livello popolato è PROCESS.
    /// </summary>
    private static void Resolve(string[] args)
    {
        var name = ArgValue(args, 0);
        if (name is null) { Help(); return; }
        var explain = args.Contains("--explain", StringComparer.OrdinalIgnoreCase);
        // Nella CLI (processo per-invocazione, nessuna sessione) lo scope è PROCESS; `--without a,b` e
        // `--private` permettono di simulare un livello SESSION senza persistenza.
        var withoutArg = RagGraphFlags.Value(args, "--without", null);
        var without = string.IsNullOrWhiteSpace(withoutArg)
            ? null
            : withoutArg.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var isPrivate = args.Contains("--private", StringComparer.OrdinalIgnoreCase);
        var scope = without is null && !isPrivate
            ? BrainScope.FromRegistry()
            : BrainScope.FromRegistry().Session(null, isPrivate, without);
        if (explain)
        {
            Console.WriteLine("scope (dal più interno al più esterno):");
            foreach (var l in scope.Explain()) Console.WriteLine(l);
        }
        var res = scope.Resolve(name);
        if (res is null)
        {
            Console.Error.WriteLine($"?brain '{name}' non risolto nello scope corrente (vedi --explain per la catena)");
            Environment.ExitCode = 1;   // diagnostica usabile in CI
            return;
        }
        var info = BrainRegistry.Info(res.Ref.Name);
        Console.WriteLine($"[OK] '{name}' → brain '{res.Ref.Name}'"
            + (res.Ref.Alias is { Length: > 0 } a ? $" (alias '{a}')" : "")
            + $" — risolto al livello {res.Level}"
            + (info is null ? "" : $" — available: [{string.Join(", ", info.Available)}]"));
    }

    /// <summary>Argomenti posizionali escludendo i flag e i loro VALORI (v. ReplEngine.BrainPositionalArgs).</summary>
    private static List<string> BrainPositionalArgs(string[] args)
    {
        string[] withValue = ["--out", "--mode", "--source", "--from", "--to", "--on-conflict",
                              "--hops", "--min-edges", "--min-p1", "--topk", "-t", "--path", "--golden"];
        var result = new List<string>();
        for (var i = 0; i < args.Length; i++)
        {
            var a = args[i];
            if (a.StartsWith("--") || a.StartsWith("-"))
            {
                if (withValue.Contains(a, StringComparer.OrdinalIgnoreCase)) i++;
                continue;
            }
            result.Add(a);
        }
        return result;
    }

    /// <summary>Toolchain V1 (stessa implementazione della REPL): stats/verify/create/optimize/backup.</summary>
    private static void Tool(string op, string[] args)
    {
        var name = ArgValue(args, 0);
        var json = args.Contains("--json", StringComparer.OrdinalIgnoreCase);
        var dryRun = args.Contains("--dry-run", StringComparer.OrdinalIgnoreCase);
        Basic.AI.Knowledge.ToolResult r = op switch
        {
            "stats" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Stats(name, json),
            "verify" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : RagGraphFlags.Value(args, "--golden", null) is { } golden
                        ? Basic.AI.Knowledge.BrainToolchain.VerifyGolden(name, golden,
                            int.TryParse(RagGraphFlags.Value(args, "--topk", "-t"), out var gk) && gk > 0 ? gk : 5,
                            double.TryParse(RagGraphFlags.Value(args, "--min-p1", null), System.Globalization.NumberStyles.Float,
                                System.Globalization.CultureInfo.InvariantCulture, out var mp1) ? mp1 : double.NaN)
                        : Basic.AI.Knowledge.BrainToolchain.Verify(name, json),
            "diff" => name is null || args.Length < 2 || args[1].StartsWith("--")
                     ? Basic.AI.Knowledge.ToolResult.Fail("uso: gmb brain diff <a> <b> [--json]")
                     : Basic.AI.Knowledge.BrainToolchain.Diff(name, args[1], json),
            "doctor" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Doctor(name),
            "convert" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("uso: gmb brain convert <name> --to rag|graph [--out <nuovo>|--in-place] [--min-edges N] [--dry-run]")
                     : Basic.AI.Knowledge.BrainToolchain.ConvertMode(name, RagGraphFlags.Value(args, "--to", null) ?? "",
                         RagGraphFlags.Value(args, "--out", null), args.Contains("--in-place", StringComparer.OrdinalIgnoreCase),
                         int.TryParse(RagGraphFlags.Value(args, "--min-edges", null), out var me) ? me : 1, dryRun),
            "create" => Basic.AI.Knowledge.BrainToolchain.Create(
                     name ?? "", RagGraphFlags.Value(args, "--from", null) ?? "",
                     RagGraphFlags.Value(args, "--mode", null) ?? "both",
                     args.Contains("--register", StringComparer.OrdinalIgnoreCase),
                     args.Contains("--activate", StringComparer.OrdinalIgnoreCase),
                     dryRun,
                     path => Basic.ConversionLib.DocumentConverter.Convert(path)),
            "optimize" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Optimize(name, dryRun, args.Contains("--reindex-fts", StringComparer.OrdinalIgnoreCase)),
            "backup" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Backup(name, RagGraphFlags.Value(args, "--out", null)),
            "buildvault" => Basic.AI.Knowledge.BrainToolchain.BuildVault(RagGraphFlags.Value(args, "--out", null)),
            "register" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("uso: gmb brain register <name> [--mode rag|graph|both] [--source dir] [--activate]")
                     : Basic.AI.Knowledge.BrainToolchain.Register(name, RagGraphFlags.Value(args, "--mode", null),
                         RagGraphFlags.Value(args, "--source", null), args.Contains("--activate", StringComparer.OrdinalIgnoreCase)),
            "dedupe" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("serve il nome del cervello")
                     : Basic.AI.Knowledge.BrainToolchain.Dedupe(name,
                         apply: args.Contains("--apply", StringComparer.OrdinalIgnoreCase), dryRun: dryRun),
            "extract" => name is null ? Basic.AI.Knowledge.ToolResult.Fail("uso: gmb brain extract <name> --out <nuovo> [--source substr] [--node substr] [--hops N]")
                     : Basic.AI.Knowledge.BrainToolchain.Extract(name, RagGraphFlags.Value(args, "--out", null) ?? "",
                         RagGraphFlags.Value(args, "--source", null), RagGraphFlags.Value(args, "--node", null),
                         int.TryParse(RagGraphFlags.Value(args, "--hops", null), out var hp) && hp > 0 ? hp : 1, dryRun),
            "merge" => Basic.AI.Knowledge.BrainToolchain.Merge(
                         BrainPositionalArgs(args),
                         RagGraphFlags.Value(args, "--out", null) ?? "",
                         RagGraphFlags.Value(args, "--on-conflict", null) ?? "skip", dryRun),
            "rename" => name is null || args.Length < 2 || args[1].StartsWith("--")
                     ? Basic.AI.Knowledge.ToolResult.Fail("uso: gmb brain rename <old> <new> [--dry-run]")
                     : Basic.AI.Knowledge.BrainToolchain.Rename(name, args[1], dryRun),
            _ => Basic.AI.Knowledge.ToolResult.Fail($"operazione '{op}' non supportata")
        };
        if (!string.IsNullOrEmpty(r.Text)) Console.WriteLine(r.Text);
        if (!r.Ok)
        {
            Console.Error.WriteLine($"?{r.Error ?? "operazione fallita"}");
            Environment.ExitCode = 1;
        }
    }

    private static void Help()
    {
        Console.WriteLine("gmb brain use    <name> [--path dir]              collega un brain già disponibile");
        Console.WriteLine("gmb brain detach <name>                          scollega (i file restano)");
        Console.WriteLine("gmb brain list                                   elenca i brain registrati (* = collegato)");
        Console.WriteLine("gmb brain info   <name>                          info + available + checksum");
        Console.WriteLine("gmb brain resolve <name|alias> [--explain] [--private] [--without a,b]   risolve un nome/alias nella catena di scope");
        Console.WriteLine("gmb brain stats  <name> [--json]               statistiche del cervello");
        Console.WriteLine("gmb brain verify <name> [--json]               verifica di integrita' (manifest+checksum, db RAG, grafo)");
        Console.WriteLine("gmb brain verify <name> --golden <f.jsonl> [--topk N] [--min-p1 0.8]   qualita' del retrieval");
        Console.WriteLine("gmb brain convert <name> --to rag|graph [--out <nuovo>|--in-place] [--min-edges N] [--dry-run]");
        Console.WriteLine("gmb brain diff <a> <b> [--json]                differenze (documenti RAG e nodi del grafo)");
        Console.WriteLine("gmb brain doctor <name>                        VERIFY + fix sicuri + riverifica");
        Console.WriteLine("gmb brain create <name> --from <dir> [--mode rag|graph|both] [--register] [--activate] [--dry-run]");
        Console.WriteLine("gmb brain optimize <name> [--dry-run] [--reindex-fts]   manutenzione (VACUUM/optimize/FTS5, export grafo)");
        Console.WriteLine("gmb brain backup <name> [--out file.zip]       zip .gmbrain con timestamp");
        Console.WriteLine("gmb brain rename <old> <new> [--dry-run]       rinomina artefatti + registry");
        Console.WriteLine("gmb brain register <name> [--mode rag|graph|both] [--source dir] [--activate]   censce artefatti gia' su disco");
        Console.WriteLine("gmb brain buildvault [--out dir]               rigenera il brain di auto-conoscenza (StdLib -> pagine + RAG)");
        Console.WriteLine("gmb brain dedupe <name> [--apply] [--dry-run]  duplicati: chunk (testo identico + crumbs) e edge del grafo");
        Console.WriteLine("gmb brain extract <name> --out <nuovo> [--source substr] [--node substr] [--hops N]");
        Console.WriteLine("gmb brain merge <a> <b> [c...] --out <nuovo> [--on-conflict skip|replace]");
        Console.WriteLine("gmb brain install <name> <zip-or-url> [--path dir] [--activate]   importa uno zip .gmbrain");
        Console.WriteLine("gmb brain export <name> [--out file.zip]         genera lo zip di un brain esistente");
        Console.WriteLine("gmb brain sign   <name> <dir> [--out file.zip]   firma un cervello utente (probe minimi graph/vectordb)");
        Console.WriteLine("  costruzione dei cervelli: gmb rag build / gmb graph build");
    }

    private static string? ArgValue(string[] args, int index) =>
        args.Length > index && !args[index].StartsWith("--") && !args[index].StartsWith("-") ? args[index] : null;

    private static string? FlagValue(string[] args, string flag)
    {
        for (var i = 0; i < args.Length; i++)
            if (args[i].Equals(flag, StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                return args[i + 1];
        return null;
    }
}
