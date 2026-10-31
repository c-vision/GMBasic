// FILE: Basic.AI.AiConfig
// Configurazione AI UNICA di GMBASIC (<configDir>/ai.json — vedi CommonLib.GmbiPaths):
// percorsi modelli, percorsi skills, cartella default skills e percorso dell'indice
// (SKILLS_PROMPT). Condivisa tra BASIC (comandi AI PATH / AI SKILLS) e REPL
// (AIPATH distingue model/skill). La posizione di configDir è configurabile nella sezione
// "paths" di system.settings.json (default <root>/config).
using System.Text.Json;
using System.Text.Json.Serialization;
using CommonLib;

namespace Basic.AI;

/// <summary>Persistenza della configurazione AI unificata. Un solo file JSON
/// (<c>config/ai.json</c> nella root del progetto — posizione configurabile in system.settings.json sezione "paths") che sostituisce il vecchio <c>aipath.txt</c> e raccoglie
/// tutti i percorsi AI: modelli, skills, default skills e indice.</summary>
public static class AiConfig
{
    private static string? _pathOverride;
    private static Settings? _cache;

    public static string DefaultPath =>
        _pathOverride ?? GmbiPaths.AiConfigFile;

    /// <summary>Hook TEST-SCOPED: reindirizza il file di config a un path temporaneo e invalida la
    /// cache. I test NON devono mai toccare la config utente reale (config/ai.json nella root del
    /// progetto) — un run ucciso tra una mutazione e il ripristino la lascia inquinata (bug
    /// 2026-08-28).</summary>
    internal static void OverridePathForTests(string path)
    {
        _pathOverride = path;
        _cache = null;
    }

    internal static void ResetForTests()
    {
        _pathOverride = null;
        _cache = null;
    }

    public sealed record Settings(
        List<string> ModelPaths,
        List<string> SkillPaths,
        string? SkillsDefaultPath,
        string? SkillsIndexPath,
        string? PluginsDefaultPath,
        string? SystemPrompt,
        string? LogPath,
        string? HfToken = null,
        string? HfRepo = null,
        string? RagDefaultDir = null,
        string? GraphsDefaultDir = null,
        string? BrainsDefaultDir = null,
        string? BrainsExportsDir = null,
        int? RagChunkMaxChars = null,
        int? RagChunkOverlap = null,
        string? RagCollection = null,
        double? GraphPageRankDamping = null,
        int? GraphPageRankIterations = null)
    {
        /// <summary>
        /// Chiavi JSON presenti nel file ma NON mappate nel record (es. <c>defaultModel</c>/
        /// <c>defaultProvider</c> oggi in <c>config/ai.json</c>): System.Text.Json le conserva qui e le
        /// riscrive identiche a ogni salvataggio. Senza, <see cref="Save"/> (chiamato da OGNI setter di
        /// <see cref="AiConfig"/>) le CANCELLEREBBE: bug reale 2026-09-16 — una suite di test non isolata
        /// ha riscritto la config utente perdendo quelle chiavi
        /// (`Bugs/(solved) - AiConfig-Roundtrip-Perdita-Chiavi-JSON`).
        /// </summary>
        [JsonExtensionData]
        public Dictionary<string, JsonElement> Extra { get; init; } = new();

        public static Settings Empty() => new([], [], null, null, null, DefaultSystemPrompt, null);
    }

    /// <summary>Prompt di sistema di base per gli agenti autonomi. L'utente può personalizzarlo
    /// dal file di configurazione (config/ai.json, chiave systemPrompt). Regole pensate per
    /// un agente che gira da solo (stile OpenClaw): prudenza sui comandi, logging dei problemi,
    /// uso delle skill.</summary>
    public static string DefaultSystemPrompt =>
        """
        You are an autonomous AI agent running inside GMBASIC, a compiled BASIC environment.

        ## Operating rules
        - You may call tools to gather information, read/write files, or interact with the system.
        - NEVER run destructive system commands (rm -rf, formatting, dropping databases, mass
          deletion) without explicit user confirmation. Prefer safe, reversible operations.
        - If a task is ambiguous or would have irreversible effects, ask before proceeding.
        - Keep answers concise and factual. State assumptions when data is incomplete.
        - If something fails, do not silently retry forever: log the issue and stop with a clear
          explanation of what happened and what was attempted.

        ## Logging
        - If you encounter an error or malfunction, write a log line to the agent log folder
          (AI LOG PATH in config/ai.json, default ai-logs/) with a timestamp, the
          operation, and the error detail, then report it. Use AI LOG WRITE/WRITELINE.
        - On success of a notable operation, you may append to agent.log in the same folder.
        - The exact log folder is reported below under "Agent log folder".

        ## Skills
        - When a task matches a skill (see the Skills section below), READ the skill file at the
          given path and follow its instructions.

        ## Plugins (capacita' operative)
        - A plugin is an OPERATIONAL capability (web fetch, PDF, browser, report, ...): use it
          when a task matches its trigger (see the Plugin section below).
        - To use a plugin: READ its PLUGIN.md, then call its tool (if enabled) or invoke
          AI.PLUGIN.CALL("<nome>", arg).
        - Manage plugins with AI PLUGINS (LIST / ENABLE / DISABLE / REFRESH / ADD / REMOVE).
        - AUTONOMY: if you repeatedly need a capability that no plugin provides, CREATE one:
          AI PLUGINS CREATE <nome> <descrizione> (skeleton), add it (AI PLUGINS ADD <nome>),
          TEST it on the actual task, and keep it (leave enabled) if it works or remove it
          (AI PLUGINS REMOVE <nome>) if it fails. Do not leave broken plugins behind.
        """;

    public static Settings Load()
    {
        if (_cache is not null) return _cache;
        if (!File.Exists(DefaultPath))
        {
            _cache = Settings.Empty();
            return _cache;
        }
        try
        {
            var raw = File.ReadAllText(DefaultPath);
            _cache = JsonSerializer.Deserialize<Settings>(raw, JsonOpts) ?? Settings.Empty();
        }
        catch
        {
            _cache = Settings.Empty();
        }
        return _cache;
    }

    public static void Save(Settings settings)
    {
        _cache = settings;
        var dir = Path.GetDirectoryName(DefaultPath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        var json = JsonSerializer.Serialize(settings, JsonOpts);
        // Niente scrittura se il contenuto è identico: evita riscritture inutili del file utente
        // (mtime, watcher) quando un setter imposta un valore che era già quello corrente.
        try
        {
            if (File.Exists(DefaultPath) && File.ReadAllText(DefaultPath) == json) return;
        }
        catch { /* best effort: in caso di dubbio si scrive */ }
        File.WriteAllText(DefaultPath, json);
    }

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    // ── Model paths (ex AIPATH) ────────────────────────────────────────────

    public static IReadOnlyList<string> ModelPaths => Load().ModelPaths;

    public static void AddModelPath(string path)
    {
        var s = Load();
        var p = PathExpansionExpand(path);
        if (!s.ModelPaths.Contains(p, StringComparer.OrdinalIgnoreCase))
        {
            Save(s with { ModelPaths = [.. s.ModelPaths, p] });
        }
    }

    public static void RemoveModelPath(string path)
    {
        var s = Load();
        var p = PathExpansionExpand(path);
        var filtered = s.ModelPaths.Where(x => !string.Equals(x, p, StringComparison.OrdinalIgnoreCase)).ToList();
        if (filtered.Count != s.ModelPaths.Count) Save(s with { ModelPaths = filtered });
    }

    // ── Skill paths ────────────────────────────────────────────────────────

    public static IReadOnlyList<string> SkillPaths => Load().SkillPaths;

    public static void AddSkillPath(string path)
    {
        var s = Load();
        var p = PathExpansionExpand(path);
        if (!s.SkillPaths.Contains(p, StringComparer.OrdinalIgnoreCase))
        {
            Save(s with { SkillPaths = [.. s.SkillPaths, p] });
        }
    }

    public static void RemoveSkillPath(string path)
    {
        var s = Load();
        var p = PathExpansionExpand(path);
        var filtered = s.SkillPaths.Where(x => !string.Equals(x, p, StringComparison.OrdinalIgnoreCase)).ToList();
        if (filtered.Count != s.SkillPaths.Count) Save(s with { SkillPaths = filtered });
    }

    // ── Default skills path / index ────────────────────────────────────────

    public static string SkillsDefaultPath
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.SkillsDefaultPath)) return PathExpansionExpand(s.SkillsDefaultPath!);
            return GmbiPaths.SkillsDir;
        }
        set
        {
            var s = Load();
            Save(s with { SkillsDefaultPath = PathExpansionExpand(value) });
        }
    }

    public static string SkillsIndexPath
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.SkillsIndexPath)) return PathExpansionExpand(s.SkillsIndexPath!);
            // L'indice è un OUTPUT rigenerabile (lo riscrive SkillIndex.Refresh): sta nella
            // CONFIG (<GmbiPaths.ConfigDir>, accanto ad ai.json), non dentro l'archivio delle
            // skill. Due ragioni: (a) l'archivio può essere in sola lettura (app installata) e
            // la scrittura falliva; (b) il file generato dentro `shared/skills` portava i PATH
            // ASSOLUTI della macchina che lo aveva generato, sporcando il repository.
            return Path.Combine(GmbiPaths.ConfigDir, "SKILLS_PROMPT.md");
        }
        set
        {
            var s = Load();
            Save(s with { SkillsIndexPath = PathExpansionExpand(value) });
        }
    }

    // ── Default plugins path / index (archivio plugin operativi) ─────────

    public static string PluginsDefaultPath
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.PluginsDefaultPath)) return PathExpansionExpand(s.PluginsDefaultPath!);
            return GmbiPaths.PluginsDir;
        }
        set
        {
            var s = Load();
            // SET INDEX "" (o spazi) = RESET al default: salva null (chiave omessa dal JSON) —
            // parità del reset C (gmb_plugin_set_index con path vuoto). Permette ai MiniTest
            // di usare SET INDEX senza inquinare config/ai.json (lezione bug plugin §7.6).
            Save(s with { PluginsDefaultPath = string.IsNullOrWhiteSpace(value) ? null : PathExpansionExpand(value) });
        }
    }

    public static string PluginsPromptPath
    {
        // OUTPUT rigenerabile (PluginIndex.Refresh lo riscrive) nella CONFIG (GmbiPaths.ConfigDir,
        // accanto ad ai.json) come SkillsIndexPath: un archivio in sola lettura (app installata)
        // non deve impedire il refresh, e il file generato non deve finire dentro `shared/plugins`.
        get => Path.Combine(GmbiPaths.ConfigDir, "PLUGINS_PROMPT.md");
    }

    /// <summary>Prompt di sistema di base per gli agenti (modificabile da config).</summary>
    public static string SystemPrompt
    {
        get
        {
            var s = Load();
            return string.IsNullOrWhiteSpace(s.SystemPrompt) ? DefaultSystemPrompt : s.SystemPrompt!;
        }
        set
        {
            var s = Load();
            Save(s with { SystemPrompt = value });
        }
    }

    /// <summary>Cartella di log degli agenti (AI LOG PATH). Default: ai-logs/ (root del repo,
    /// git-ignorata — non è un esempio, non appartiene a samples/). Posizione configurabile
    /// nella sezione "paths" di system.settings.json (chiave logsDir).</summary>
    public static string LogPath
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.LogPath)) return PathExpansionExpand(s.LogPath!);
            return GmbiPaths.LogsDir;
        }
        set
        {
            var s = Load();
            Save(s with { LogPath = PathExpansionExpand(value) });
        }
    }

    /// <summary>Token HuggingFace per il marketplace RecursiveMAS (push/pull checkpoint).
    /// Configurabile via <c>AI SET HFTOKEN ...</c>. Persistito in <c>.env</c> (chiave HF_TOKEN,
    /// MAI in config/ai.json — quel file è versionato dal 2026-09-07, i segreti non ci vivono
    /// più in chiaro). Il campo <c>hfToken</c> di ai.json resta letto per compatibilità con
    /// installazioni pre-esistenti, ma non viene più scritto.</summary>
    public static string? HfToken
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.HfToken)) return s.HfToken;
            return CommonLib.EnvFile.Get("HF_TOKEN");
        }
        set
        {
            CommonLib.EnvFile.Set("HF_TOKEN", value ?? "");
        }
    }

    /// <summary>Repo HuggingFace del marketplace RecursiveMAS (es. "pyoptiontrader/recursivemas"
    /// o "recursivemas"). Configurabile via <c>AI SET HFREPO ...</c> o in config/ai.json
    /// (chiave hfRepo).</summary>
    public static string? HfRepo
    {
        get
        {
            var s = Load();
            return string.IsNullOrWhiteSpace(s.HfRepo) ? null : s.HfRepo;
        }
        set
        {
            var s = Load();
            Save(s with { HfRepo = string.IsNullOrWhiteSpace(value) ? null : value });
        }
    }

    // ── RAG / Graphs (comandi REPL/CLI RAG BUILD / GRAPH BUILD) ─────────────

    private static string ProjectRoot => GmbiPaths.RepoRoot;

    /// <summary>Cartella RAG di default (dove vivono i db .db costruiti da RAG BUILD).
    /// Default: &lt;root&gt;/rag/ (pattern skills/plugins; la cartella rag/ esiste già in root).
    /// Sovrascrivibile da config (paths.ragDir / ragDefaultDir legacy) o per-run con --rag-dir.</summary>
    public static string RagDefaultDir
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.RagDefaultDir)) return PathExpansionExpand(s.RagDefaultDir!);
            return GmbiPaths.RagDir;
        }
        set
        {
            var s = Load();
            Save(s with { RagDefaultDir = PathExpansionExpand(value) });
        }
    }

    /// <summary>Cartella graphs di default (dove vivono gli export .graphml/.mermaid/.json di
    /// GRAPH BUILD). Default: &lt;root&gt;/graphs/ (la cartella graphs/ va creata in root).
    /// Sovrascrivibile da config (paths.graphsDir / graphsDefaultDir legacy) o per-run con
    /// --graphs-dir.</summary>
    public static string GraphsDefaultDir
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.GraphsDefaultDir)) return PathExpansionExpand(s.GraphsDefaultDir!);
            return GmbiPaths.GraphsDir;
        }
        set
        {
            var s = Load();
            Save(s with { GraphsDefaultDir = PathExpansionExpand(value) });
        }
    }

    /// <summary>Cartella brains di default (dove si installano i cervelli importati da
    /// BRAIN INSTALL). Default: &lt;root&gt;/brains/. Sovrascrivibile da config
    /// (paths.brainsDir / brainsDefaultDir legacy) o per-run con --path.</summary>
    public static string BrainsDefaultDir
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.BrainsDefaultDir)) return PathExpansionExpand(s.BrainsDefaultDir!);
            return GmbiPaths.BrainsDir;
        }
        set
        {
            var s = Load();
            Save(s with { BrainsDefaultDir = PathExpansionExpand(value) });
        }
    }

    /// <summary>Cartella degli export BRAIN EXPORT (zip .gmbrain). Default:
    /// &lt;root&gt;/brains-exports/. Sovrascrivibile da config (paths.brainsExportsDir /
    /// brainsExportsDir legacy).</summary>
    public static string BrainsExportsDir
    {
        get
        {
            var s = Load();
            if (!string.IsNullOrWhiteSpace(s.BrainsExportsDir)) return PathExpansionExpand(s.BrainsExportsDir!);
            return GmbiPaths.BrainsExportsDir;
        }
        set
        {
            var s = Load();
            Save(s with { BrainsExportsDir = PathExpansionExpand(value) });
        }
    }

    /// <summary>Chunking del build RAG wiki. Default: MaxChars=2048, Overlap=256 (consigliato
    /// dal TODO per 27k file — più lungo del default 512/64, meno embedding, contesto tecnico
    /// migliore). Configurabile da config o per-run con --chunk/--overlap.</summary>
    public static Basic.SQLite.Vector.ChunkingOptions RagChunkOptions()
    {
        var s = Load();
        var max = s.RagChunkMaxChars is > 0 ? s.RagChunkMaxChars.Value : 2048;
        var ovl = s.RagChunkOverlap is >= 0 ? s.RagChunkOverlap.Value : 256;
        return new Basic.SQLite.Vector.ChunkingOptions(max, ovl);
    }

    /// <summary>Nome collection RAG di default (dove indicizza RAG BUILD). Default: "wiki".
    /// Configurabile da config (ragCollection) o per-run con --collection.</summary>
    public static string RagCollection
    {
        get
        {
            var s = Load();
            return string.IsNullOrWhiteSpace(s.RagCollection) ? "wiki" : s.RagCollection!;
        }
        set
        {
            var s = Load();
            Save(s with { RagCollection = string.IsNullOrWhiteSpace(value) ? null : value });
        }
    }

    /// <summary>Damping factor del PageRank del grafo (default 0.85, standard). Configurabile da
    /// config (graphPageRankDamping) o per-run con --pagerank-damping.</summary>
    public static double GraphPageRankDamping
    {
        get
        {
            var s = Load();
            return s.GraphPageRankDamping is > 0 and <= 1 ? s.GraphPageRankDamping.Value : 0.85;
        }
        set
        {
            var s = Load();
            Save(s with { GraphPageRankDamping = value is > 0 and <= 1 ? value : null });
        }
    }

    /// <summary>Iterazioni del PageRank del grafo (default 100). Configurabile da config
    /// (graphPageRankIterations) o per-run con --pagerank-iterations.</summary>
    public static int GraphPageRankIterations
    {
        get
        {
            var s = Load();
            return s.GraphPageRankIterations is > 0 ? s.GraphPageRankIterations.Value : 100;
        }
        set
        {
            var s = Load();
            Save(s with { GraphPageRankIterations = value > 0 ? value : null });
        }
    }

    /// <summary>Espande '~' e variabili d'ambiente in un path (riusa PathExpansion di
    /// ModelDownload per coerenza col resto del layer AI).</summary>
    private static string PathExpansionExpand(string path) =>
        Basic.ModelDownload.PathExpansion.Expand(path);
}
