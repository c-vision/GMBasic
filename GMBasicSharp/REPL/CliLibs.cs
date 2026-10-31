// REPL/CliLibs.cs — `gmb libs ...`: le LIBRERIE NATIVE stanno su c-vision/GMBasicModules e si
// scaricano ON-DEMAND in `~/.gmbasic/libs/<rid>/` (CommonLib.LibsResolver). Il repo non le contiene.
//
//   gmb libs status  [--rid RID]                     cosa c'è in cache, per gruppo (file/byte/stato)
//   gmb libs check   [--rid RID] [--group G]         verifica sha256 di ogni file (exit 1 se guasto)
//   gmb libs ensure  [--rid RID] [--group G]... [--file F]... [--all]
//                                                    garantisce i gruppi/file (default: support+core+engine;
//                                                    --group llmmotor = motori AI, --all = +torch external)
//   gmb libs prune   [--rid RID] [--dry-run]         rimuove dalla cache i file NON più nell'indice
//   gmb libs index   [--rid RID]                     rigenera Resources/libs-manifest.json dalle release
//   gmb libs publish <support|engine|core> <rid> [--yes]
//                                                    pubblica un bundle (delega a tools/libs_publish_gh.sh)
//   gmb libs help
//
// MODELLO (Knowledge/GMBasicModules-Repo-Fidati.md):
//   support (B, terze parti) · engine (C, LLMEnv/FastLLM) · core (D, build GMBasic) ·
//   torch (external, pytorch.org) — quattro gruppi, lo STESSO protocollo, un solo indice.

namespace Basic.Executor;

public static class CliLibs
{
    private const string Repo = "c-vision/GMBasicModules";

    public static bool TryDispatch(string[] args)
    {
        if (args.Length == 0 || !args[0].Equals("libs", StringComparison.OrdinalIgnoreCase)) return false;
        var rest = args.Skip(1).ToArray();
        var cmd = rest.Length > 0 ? rest[0].ToLowerInvariant() : "help";
        var a = rest.Skip(1).ToArray();
        try
        {
            switch (cmd)
            {
                case "install": Install(a); break;
                case "status": Status(a); break;
                case "check": Check(a); break;
                case "ensure": case "get": Ensure(a); break;
                case "prune": Prune(a); break;
                case "index": Index(a); break;
                case "publish": Publish(a); break;
                default: Help(); break;
            }
        }
        catch (CommonLib.LibsResolutionException ex)
        {
            Console.Error.WriteLine($"?{ex.Message}");
            Environment.ExitCode = 1;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"?errore: {ex.Message}");
            Environment.ExitCode = 1;
        }
        return true;
    }

    // ── install ────────────────────────────────────────────────────────────────────────────
    /// <summary>`gmb install [--runtime-only|--dev|--components|--all] [--verify]`
    /// Scarica nella cache (~/.gmbasic/libs) TUTTO ciò che serve al sistema operativo corrente,
    /// usando il downloader del runtime C (`libgmb_boot.gmb_bootstrap`): un solo percorso, con
    /// progress bar su terminale. `--verify` fa solo il controllo sha (equivalente a `libs check`).</summary>
    private static void Install(string[] a)
    {
        var verifyOnly = a.Any(x => x.Equals("--verify", StringComparison.OrdinalIgnoreCase));
        if (verifyOnly) { Check(a); return; }

        var scope = CommonLib.GmbBoot.Scope.All;               // default: tutte le librerie del RID
        if (a.Any(x => x.Equals("--runtime-only", StringComparison.OrdinalIgnoreCase)))
            scope = CommonLib.GmbBoot.Scope.Runtime;
        else if (a.Any(x => x.Equals("--dev", StringComparison.OrdinalIgnoreCase)))
            scope = CommonLib.GmbBoot.Scope.Dev;
        else if (a.Any(x => x.Equals("--components", StringComparison.OrdinalIgnoreCase)))
            scope = CommonLib.GmbBoot.Scope.Components;

        var rid = CommonLib.GmbBoot.Rid();
        var cache = CommonLib.GmbBoot.CacheDir() ?? "~/.gmbasic/libs";
        Console.WriteLine($"[install] RID {rid} · cache {cache} · scope {scope}");
        if (!CommonLib.GmbBoot.EnsureLoaded())
        {
            Console.Error.WriteLine($"?{CommonLib.GmbBoot.LastError}");
            Environment.ExitCode = 1;
            return;
        }
        // `gmb install` è esplicito: rinfresca l'indice piatto (pochi KB) prima di scaricare.
        // Senza, un indice in cache da prima dell'aggiunta di un gruppo (es. `boot`) farebbe
        // risultare "tutto a posto" e il gruppo nuovo non verrebbe mai scaricato.
        try
        {
            var idx = Path.Combine(CommonLib.GmbBoot.CacheDir() ?? "", CommonLib.GmbBoot.Rid(), "index.txt");
            if (File.Exists(idx)) File.Delete(idx);
        }
        catch { /* best-effort */ }

        var ok = CommonLib.GmbBoot.Bootstrap(scope);
        if (!ok)
        {
            Console.Error.WriteLine($"?install incompleto: {CommonLib.GmbBoot.LastError ?? "vedi i messaggi sopra"}");
            Environment.ExitCode = 1;
            return;
        }
        Console.WriteLine("[install] OK — tutte le librerie richieste sono in cache");
    }

    // ── status ──────────────────────────────────────────────────────────────────────────────
    private static void Status(string[] a)
    {
        var rid = Arg(a, "--rid") ?? CommonLib.LibsResolver.Rid();
        var m = CommonLib.LibsResolver.LoadManifestAsync().GetAwaiter().GetResult();
        var states = CommonLib.LibsResolver.Status(m, rid);
        var cache = CommonLib.LibsResolver.CacheDir(rid);

        Console.WriteLine($"[libs] indice v{m.Version} · RID {rid} · cache {cache}");
        Console.WriteLine($"  {"gruppo",-8} {"file",5} {"byte",11}  {"in cache",9}  tipo/tag");
        foreach (var (group, files, bytes, kind, tag) in CommonLib.LibsResolver.Groups(m, rid))
        {
            var cached = states.Count(s => s.Group == group && s.Status == CommonLib.LibsFileStatus.Cached);
            Console.WriteLine($"  {group,-8} {files,5} {bytes,11:N0}  {cached,4}/{files,-4}  {kind} {(tag is null ? "" : tag)}");
        }
        var miss = states.Where(s => s.Status != CommonLib.LibsFileStatus.Cached).ToList();
        if (miss.Count == 0) { Console.WriteLine("[libs] cache completa per questo RID."); return; }
        Console.WriteLine($"[libs] {miss.Count} file non in cache (o sha errato):");
        foreach (var s in miss.Take(20)) Console.WriteLine($"   {s.Status,-7} {s.Name}  [{s.Group}]");
        if (miss.Count > 20) Console.WriteLine($"   … altri {miss.Count - 20}");
        Console.WriteLine("[libs] per scaricarli:  gmb libs ensure");
    }

    // ── check ───────────────────────────────────────────────────────────────────────────────
    private static void Check(string[] a)
    {
        var rid = Arg(a, "--rid") ?? CommonLib.LibsResolver.Rid();
        var group = Arg(a, "--group");
        // default: i gruppi che un runtime deve avere (torch è esterno e on-demand → solo con --all)
        var groups = group is not null ? new[] { group }
                   : a.Contains("--all") ? null
                   : [.. CommonLib.LibsResolver.BaseGroups];
        var states = CommonLib.LibsResolver.CheckAsync(groups, rid)
                                     .GetAwaiter().GetResult();
        var bad = states.Where(s => s.Status != CommonLib.LibsFileStatus.Cached).ToList();
        foreach (var s in bad) Console.WriteLine($"   {s.Status,-7} {s.Name}  [{s.Group}]");
        if (bad.Count == 0)
        {
            Console.WriteLine($"[libs] integrità OK: {states.Count} file verificati (sha256) per RID {rid}" +
                              (group is null ? "" : $" (gruppo {group})"));
            return;
        }
        Console.Error.WriteLine($"?{bad.Count} file mancanti/corrotti su {states.Count} — `gmb libs ensure` per ripararli");
        Environment.ExitCode = 1;
    }

    // ── ensure ──────────────────────────────────────────────────────────────────────────────
    private static void Ensure(string[] a)
    {
        var rid = Arg(a, "--rid") ?? CommonLib.LibsResolver.Rid();
        var files = Args(a, "--file");
        var groups = Args(a, "--group");
        var all = a.Contains("--all");

        if (files.Count > 0)
        {
            var n = CommonLib.LibsResolver.EnsureAsync(files, rid, Console.WriteLine).GetAwaiter().GetResult();
            Console.WriteLine($"[libs] {n} file installati in {CommonLib.LibsResolver.CacheDir(rid)}");
            return;
        }

        if (groups.Count == 0) groups = [.. CommonLib.LibsResolver.BaseGroups];
        if (all)
        {
            // `--all` = TUTTI i gruppi che l'indice dichiara per questo RID (runtime, llmmotor,
            // dev, support, torch…). Una lista fissa chiedeva gruppi che per il RID non esistono
            // e faceva fallire il comando con "il gruppo 'core' non ha file".
            var man = CommonLib.LibsResolver.LoadManifestAsync().GetAwaiter().GetResult();
            groups = CommonLib.LibsResolver.Groups(man, rid).Select(g => g.Group).ToList();
        }
        groups = groups.Distinct(StringComparer.OrdinalIgnoreCase).ToList();

        var total = 0;
        foreach (var g in groups)
        {
            Console.WriteLine($"[libs] gruppo '{g}'…");
            total += CommonLib.LibsResolver.EnsureGroupAsync(g, rid, Console.WriteLine).GetAwaiter().GetResult();
        }
        Console.WriteLine(total == 0
            ? $"[libs] nulla da fare: cache già completa in {CommonLib.LibsResolver.CacheDir(rid)}"
            : $"[libs] {total} file installati in {CommonLib.LibsResolver.CacheDir(rid)}");
    }

    // ── prune ───────────────────────────────────────────────────────────────────────────────
    /// <summary>Rimuove dalla cache i file che NON sono più nell'indice (es. lib rimosse da un
    /// bundle o spostate di gruppo). Senza, restano orfani che `check` non vede e che possono
    /// far credere che una libreria sia ancora disponibile.</summary>
    private static void Prune(string[] a)
    {
        var rid = Arg(a, "--rid") ?? CommonLib.LibsResolver.Rid();
        var dry = a.Contains("--dry-run");
        var m = CommonLib.LibsResolver.LoadManifestAsync().GetAwaiter().GetResult();
        var known = new HashSet<string>(
            m.Rids is not null && m.Rids.TryGetValue(rid, out var r) && r.Files is not null
                ? r.Files.Keys : [], StringComparer.Ordinal);
        var dir = CommonLib.LibsResolver.CacheDir(rid);
        if (!Directory.Exists(dir)) { Console.WriteLine($"[libs] cache assente: {dir}"); return; }

        var removed = 0;
        foreach (var path in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories))
        {
            if (Path.GetFileName(path).StartsWith('.')) continue;
            var rel = Path.GetRelativePath(dir, path).Replace(Path.DirectorySeparatorChar, '/');
            if (known.Contains(rel)) continue;
            // gli alias `lib<base>.dylib` sono SYMLINK derivati (creati da LibsResolver): non sono orfani
            if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0) continue;
            Console.WriteLine($"   {(dry ? "da rimuovere" : "rimosso")}: {rel}");
            if (!dry) { File.Delete(path); removed++; }
        }
        Console.WriteLine(dry
            ? $"[libs] dry-run: nessun file toccato ({dir})"
            : $"[libs] {removed} file orfani rimossi da {dir}");
    }

    // ── index / publish (delegano agli script, unica implementazione di zip+manifest) ────────
    private static void Index(string[] a)
    {
        var rid = Arg(a, "--rid") ?? CommonLib.LibsResolver.Rid();
        RunBash("libs_manifest_gh.sh", ["build", "--rid", rid, "--repo", Repo]);
    }

    private static void Publish(string[] a)
    {
        var positional = a.Where(x => !x.StartsWith('-')).ToArray();
        if (positional.Length < 2)
        {
            Console.Error.WriteLine("?uso: gmb libs publish <support|engine|core> <rid> [--yes]");
            Environment.ExitCode = 1;
            return;
        }
        var args = new List<string> { positional[0], positional[1], "--repo", Repo, "--index" };
        if (a.Contains("--yes")) args.Add("--yes");
        RunBash("libs_publish_gh.sh", args);
    }

    private static void RunBash(string script, IEnumerable<string> args)
    {
        var path = Path.Combine(CommonLib.GmbiPaths.RepoRoot, "tools", script);
        if (!File.Exists(path)) throw new FileNotFoundException($"script assente: {path}");
        var psi = new System.Diagnostics.ProcessStartInfo("bash") { UseShellExecute = false };
        psi.ArgumentList.Add(path);
        foreach (var x in args) psi.ArgumentList.Add(x);
        using var p = System.Diagnostics.Process.Start(psi)!;
        p.WaitForExit();
        if (p.ExitCode != 0) Environment.ExitCode = p.ExitCode;
    }

    // ── helper ──────────────────────────────────────────────────────────────────────────────
    private static string? Arg(string[] a, string name)
    {
        for (var i = 0; i < a.Length - 1; i++) if (a[i] == name) return a[i + 1];
        return null;
    }

    private static List<string> Args(string[] a, string name)
    {
        var list = new List<string>();
        for (var i = 0; i < a.Length - 1; i++) if (a[i] == name) list.Add(a[i + 1]);
        return list;
    }

    private static void Help()
    {
        Console.WriteLine("  gmb libs status  [--rid RID]                       cache vs indice, per gruppo");
        Console.WriteLine("  gmb libs check   [--rid RID] [--group G]           verifica sha256 (exit!=0 se guasto)");
        Console.WriteLine("  gmb libs ensure  [--rid RID] [--group G]... [--file F]... [--all]");
        Console.WriteLine("                                                     scarica dai bundle GMBasicModules in ~/.gmbasic/libs/<rid>");
        Console.WriteLine("                                                     default: support+core+engine (engine = plumbing codec/token)");
        Console.WriteLine("                                                     --group llmmotor = MLX/llama · --all = + llmmotor + torch");
        Console.WriteLine("  gmb libs prune   [--rid RID] [--dry-run]           rimuove dalla cache i file non più nell'indice");
        Console.WriteLine("  gmb libs index   [--rid RID]                       rigenera Resources/libs-manifest.json dalle release");
        Console.WriteLine("  gmb libs publish <support|engine|core> <rid> [--yes]");
        Console.WriteLine("                                                     pubblica un bundle su c-vision/GMBasicModules");
        Console.WriteLine("  gmb libs help");
        Console.WriteLine("  (le librerie NON stanno nel repo: ~/.gmbasic/libs/<rid> è la cache, l'indice è su GMBasicModules)");
    }
}
