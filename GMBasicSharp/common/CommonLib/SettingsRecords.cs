// FILE: CommonLib.SettingsRecords.cs

using System.Text.Json.Serialization;

/* 
 * SettingsRecords.cs: modelli per i file di configurazione system.settings.json e app.settings.json
 * 
 * RECORDS:
 * - AppSettings: modella il file app.settings.json (sovrascritture per progetto)
 * - SystemSettings: modella il file system.settings.json (default globali)
 * - EffectiveSettings: configurazione finale fusa da system.settings + app.settings
 * - PathSettings: sezione "paths" di system.settings.json (posizioni delle cartelle
 *   dati del layer AI/config — vedi GmbiPaths in CommonLib). Default = cartelle nella
 *   root del progetto (config/, skills/, rag/, graphs/, ...); ogni chiave può essere un
 *   percorso assoluto o relativo alla root; null/assente = default del record.
 */

namespace CommonLib;

/// <summary>Modella il file app.settings.json (sovrascritture selettive per progetto).</summary>
public sealed record AppSettings
{
    /// <summary>Namespaces predefiniti da importare automaticamente.</summary>
    [JsonPropertyName("defaultNamespaces")]
    public List<string>? DefaultNamespaces { get; init; }

    /// <summary>Cartelle da cui importare file BASIC.</summary>
    [JsonPropertyName("importFolders")]
    public List<string>? ImportFolders { get; init; }

    /// <summary>File BASIC da importare automaticamente.</summary>
    [JsonPropertyName("importFiles")]
    public List<string>? ImportFiles { get; init; }

    /// <summary>Configurazioni console (schermo) per il progetto.</summary>
    [JsonPropertyName("consoleConfiguration")]
    public List<global::CommonLib.Screens.ScreenConfig>? ConsoleConfiguration { get; init; }

    /// <summary>Palette di colori personalizzate.</summary>
    [JsonPropertyName("customPalettes")]
    public Dictionary<string, byte[][]>? CustomPalettes { get; init; }

    /// <summary>Cartelle di librerie aggiuntive.</summary>
    [JsonPropertyName("libraryFolders")]
    public List<string>? LibraryFolders { get; init; }

    /// <summary>Toolchain C e flag: OVERRIDE per-applicazione del default globale.
    /// Ogni lista presente SOSTITUISCE la lista globale (non si accoda) — vedi
    /// <see cref="CompilerSettings"/>; una lista vuota la disattiva, una assente la conserva.</summary>
    [JsonPropertyName("compiler")]
    public CompilerSettingsOverride? Compiler { get; init; }
}

/// <summary>Posizioni delle cartelle dati di GMBASIC (sezione "paths" di system.settings.json).
/// Default = cartelle nella root del progetto (pattern skills/plugins/rag). Ogni chiave può
/// essere un percorso assoluto o relativo alla root del repo; null/assente = default del
/// record. La risoluzione effettiva (root discovery, ~, variabili d'ambiente) è in
/// <see cref="CommonLib.GmbiPaths"/>.</summary>
public sealed record PathSettings
{
    /// <summary>Cartella dei file di stato/config del layer AI (ai.json, providers.json,
    /// mcp.json, vaults.db, brain.db, brains.json, ide.settings.json). Default: config/</summary>
    [JsonPropertyName("configDir")]
    public string? ConfigDir { get; init; }

    /// <summary>Archivio skills (SKILL.md). Default: skills/</summary>
    [JsonPropertyName("skillsDir")]
    public string? SkillsDir { get; init; }

    /// <summary>Archivio plugin operativi. Default: plugins/</summary>
    [JsonPropertyName("pluginsDir")]
    public string? PluginsDir { get; init; }

    /// <summary>Log degli agenti AI. Default: ai-logs/</summary>
    [JsonPropertyName("logsDir")]
    public string? LogsDir { get; init; }

    /// <summary>Database RAG costruiti da RAG BUILD. Default: rag/</summary>
    [JsonPropertyName("ragDir")]
    public string? RagDir { get; init; }

    /// <summary>Export graph (GRAPH BUILD). Default: graphs/</summary>
    [JsonPropertyName("graphsDir")]
    public string? GraphsDir { get; init; }

    /// <summary>Cervelli installati (BRAIN INSTALL). Default: brains/</summary>
    [JsonPropertyName("brainsDir")]
    public string? BrainsDir { get; init; }

    /// <summary>Export BRAIN EXPORT (zip .gmbrain). Default: brains-exports/</summary>
    [JsonPropertyName("brainsExportsDir")]
    public string? BrainsExportsDir { get; init; }

    /// <summary>Base dei COMPONENTI RUNTIME scaricati on-demand (torch/ml, chromium): i loader
    /// cercano qui, in &lt;libsDir&gt;/&lt;componente&gt;/&lt;rid&gt;/. Default: libs-runtime/
    /// (cartella NON versionata in root — in root solo file definitivi, ma le dylib scaricate
    /// ~2GB non si versionano; Resources/libs/&lt;rid&gt; resta SOLO per le libs definitive
    /// versionate gestite dal componente LIBS).</summary>
    [JsonPropertyName("libsDir")]
    public string? LibsDir { get; init; }

    /// <summary>Cartella dei PACCHETTI installati dal package manager (stile node_modules):
    /// ogni pacchetto scaricato dal registry vive qui, in &lt;packagesDir&gt;/&lt;nome&gt;/&lt;versione&gt;/.
    /// Default: packages/ (nella root del progetto: TUTTO ciò che è scaricato resta dentro
    /// GMBasic — niente file sparsi nel sistema operativo).</summary>
    [JsonPropertyName("packagesDir")]
    public string? PackagesDir { get; init; }

    /// <summary>Cartella temporanea/cache (kv-swap, marketplaces clone, staging download).
    /// Default: null → System.IO.Path.GetTempPath() (il SO la pulisce).</summary>
    [JsonPropertyName("tempDir")]
    public string? TempDir { get; init; }

    /// <summary>Cartella ABI di LLMEnv (i descrittori dei modelli `.gmbarch`, di cui LLMEnv e' l'unico proprietario).
    /// Default: null = non ancora localizzata. Non si calcola in C#: i dylib C (gmb_ai_modelio, gmb_ai_llm) la risolvono
    /// quando serve la prima volta (impostazione salvata → localizzazione di pkg/albero sorgente → scrittura
    /// qui) e propongono l'installazione di LLMEnv se manca. Va nel `system.settings.json` per il compilatore e
    /// nell'`app.settings.json` per l'applicazione BASIC.</summary>
    [JsonPropertyName("abiDir")]
    public string? AbiDir { get; init; }

    /// <summary>Cache delle LIBRERIE NATIVE scaricate on-demand (support/engine/core/torch) in
    /// `&lt;libsCacheDir&gt;/&lt;rid&gt;/`. Default: ~/.gmbasic/libs (l'unica posizione che vale sia per
    /// il checkout di sviluppo sia per il client installato senza compilatore).
    /// Vedi CommonLib.LibsResolver + Knowledge/GMBasicModules-Repo-Fidati.md.</summary>
    [JsonPropertyName("libsCacheDir")]
    public string? LibsCacheDir { get; init; }
}

/// <summary>Override della sezione "compiler" a un livello che STA SOPRA i settings:
/// per-APPLICAZIONE (`app.settings.json`) e one-shot da riga di comando
/// (`--cc=…` `--cflags=…` `--ldflags=…` `--defines=…`). Stesso record per i due: identica
/// semantica per chiave, e chiude il buco del gate che gira fuori dalla radice e
/// non può (non deve) modificare un settings condiviso per provare un profilo diverso.
///
/// Tutte le chiavi sono NULLABLE per una ragione precisa: devono distinguere "non dichiarato"
/// (→ vale il livello sottostante) da "dichiarato" (→ sovrascrive). Se riusassimo
/// <see cref="CompilerSettings"/>, che ha default NON NULL, un override che dichiara solo
/// `cflags` arriverebbe con Cc="cc" (il default del record) e sovrascriverebbe per sbaglio
/// un eventuale `cc` globale custom: il "non dichiarato" si perderebbe.
/// Le liste sono quindi List? — assente = conserva, [] = disattiva.</summary>
public sealed record CompilerSettingsOverride
{
    /// <summary>Compilatore C da usare. null = conserva quello globale.</summary>
    [JsonPropertyName("cc")]
    public string? Cc { get; init; }

    /// <summary>Flag di compilazione. null = conserva quelli globali; [] = nessun flag.</summary>
    [JsonPropertyName("cflags")]
    public List<string>? CFlags { get; init; }

    /// <summary>Flag di link. null = conserva quelli globali; [] = nessuno.</summary>
    [JsonPropertyName("ldflags")]
    public List<string>? LdFlags { get; init; }

    /// <summary>Macro -D. null = conserva quelle globali; [] = nessuna.</summary>
    [JsonPropertyName("defines")]
    public List<string>? Defines { get; init; }

    /// <summary>Cartelle di ricerca dei moduli BASIC (`*.bas`/`*.inc`). null = conserva
    /// quelle globali; [] = nessuna (solo progetto e import). Vedi <see cref="CompilerSettings.SearchFolders"/>.</summary>
    [JsonPropertyName("searchFolders")]
    public List<string>? SearchFolders { get; init; }
}

/// <summary>Flag e toolchain di compilazione C (sezione "compiler" dei settings).
///
/// DOVE STA IL DEFAULT: in <c>system.settings.json</c> (default GLOBALE condiviso da tutti
/// i backend che invocano il toolchain C: C99Native). I valori qui sotto sono
/// la stessa cosa che il file JSON dichiara: se il file manca, il record riproduce il default.
///
/// COME VIENE SOVRASCRITTO: <c>app.settings.json</c> (impostazioni dell'applicazione BASIC)
/// <b>sostituisce</b> il default globale, NON lo accoda — è la semantica opposta a
/// defaultNamespaces/importFolders (che fanno unione). Motivo: i flag del compilatore non
/// sono un elenco cumulativo: accodare "-O2" e "-O3" darebbe un comando ambiguo. Per questo
/// <see cref="Resolve"/> applica l'override per CHIAVE: una lista presente in app.settings
/// rimpiazza interamente la lista globale, una lista assente la lascia intatta.
///
/// LO STESSO FLAG PUO' ESSERE RIMOSSO: una lista vuota in app.settings.json ("cflags": [])
/// disattiva i flag globali (serve per togliere -w quando si vuole vedere i warning).
///
/// NESSUNA VARIABILE D'AMBIENTE: i parametri di compilazione passano solo da qui (o dalle
/// opzioni one-shot della CLI, vedi --cflags/--ldflags del driver C99Native).</summary>
public sealed record CompilerSettings
{
    /// <summary>Compilatore C da usare. "cc" = quello di sistema (clang/gcc).</summary>
    [JsonPropertyName("cc")]
    public string Cc { get; init; } = "cc";

    /// <summary>Flag di compilazione, nell'ordine in cui sono passati a cc.
    /// Default: -std=c99 (C stretto, niente estensioni GNU) · -O2 · -w (i warning del C
    /// generato inquinerebbero lo stdout catturato dagli harness → falso DIFF di parità).</summary>
    [JsonPropertyName("cflags")]
    public List<string> CFlags { get; init; } = ["-std=c99", "-O2", "-w"];

    /// <summary>Flag di link. Default vuoto: ogni sito aggiunge le sue librerie
    /// necessarie (librerie di sistema, dipendenze native scoperte a runtime, -lm).</summary>
    [JsonPropertyName("ldflags")]
    public List<string> LdFlags { get; init; } = [];

    /// <summary>Macro -D passate a cc (es. ["-DHAVE_OPENSSL", "-DNDEBUG"]).</summary>
    [JsonPropertyName("defines")]
    public List<string> Defines { get; init; } = [];

    /// <summary>Cartelle in cui il compilatore cerca i moduli BASIC (`*.bas`/`*.inc`).
    /// Sono le stesse per TUTTI i backend C (e per l'Orchestrator C#, che le usa in
    /// `DiscoverSourceFiles`): il vecchio elenco cablato `include/` · `Controls/` ·
    /// `&lt;install&gt;/include` · `&lt;install&gt;/Controls` · `&lt;install&gt;/libs/&lt;nome&gt;`
    /// è solo il DEFAULT, ora sostituibile dai settings.
    ///
    /// Sintassi di una voce:
    ///  • `{project}` = cartella della entry, `{install}` = root di GMBasic;
    ///  • una voce relativa senza placeholder è cercata nella cartella del progetto, poi
    ///    risalendo le cartelle (include/ e Controls/ della root), poi nella root dell'install;
    ///  • `*` finale = ogni sottocartella (una di livello).
    /// La scansione di ogni voce è ricorsiva.
    ///
    /// MONOREPO: gli asset condivisi sono in `&lt;monorepo&gt;/shared` e `{install}` è
    /// `&lt;monorepo&gt;/GMBasic`, perciò le voci d'install puntano a `{install}/../shared/…`
    /// (`GMBasic/Controls` è stato consolidato in `shared/Controls` il 2026-10-29).
    /// Stessa lista in `shared/settings/system.settings.json` e in
    /// `kDefaultSearch` di `native/gmb_compiler_c/frontend/src/gmb_inc_resolve.c`.</summary>
    [JsonPropertyName("searchFolders")]
    public List<string> SearchFolders { get; init; } =
    [
        "include",
        "Controls",
        "controls",
        "{install}/../shared/include",
        "{install}/../shared/Controls",
        "{install}/../shared/libs/*",
        "{install}/libs/*"
    ];

    /// <summary>Fonde i tre livelli — default GLOBALE (system.settings.json), override
    /// per-APPLICAZIONE (app.settings.json), override ONE-SHOT da riga di comando — per CHIAVE
    /// (non per unione: vedi la nota sulla semantica). Precedenza: cli &gt; app &gt; system.
    /// I livelli sono tutti facoltativi e "assente" non sovrascrive: la CLI può toccare solo
    /// `cflags` lasciando il `cc` dei settings, e viceversa.
    /// Le liste sono clonate: il record resta immutabile e i default del record globale non
    /// vengono mai modificati per leaking.</summary>
    public static CompilerSettings Resolve(CompilerSettings? system,
        CompilerSettingsOverride? app = null, CompilerSettingsOverride? cli = null)
        => Apply(Apply(system ?? new CompilerSettings(), app), cli);

    /// <summary> Applica un livello di override a un profilo già risolto (usato da Resolve e
    /// dai test). Stessa semantica per chiave: assente conserva, presente rimpiazza,
    /// lista vuota disattiva.</summary>
    public static CompilerSettings Apply(CompilerSettings baseProfile, CompilerSettingsOverride? over)
    {
        var s = baseProfile;
        if (over == null) return s with
        {
            CFlags = [..s.CFlags], LdFlags = [..s.LdFlags], Defines = [..s.Defines],
            SearchFolders = [..s.SearchFolders]
        };
        return s with
        {
            Cc = string.IsNullOrWhiteSpace(over.Cc) ? s.Cc : over.Cc.Trim(),
            CFlags = over.CFlags is null ? [..s.CFlags] : [..over.CFlags],
            LdFlags = over.LdFlags is null ? [..s.LdFlags] : [..over.LdFlags],
            Defines = over.Defines is null ? [..s.Defines] : [..over.Defines],
            SearchFolders = over.SearchFolders is null ? [..s.SearchFolders] : [..over.SearchFolders]
        };
    }

    /// <summary>I flag come stringa pronta per la command line (elenco vuoto = stringa vuota,
    /// così si può concatenare senza lasciare spazi doppi).</summary>
    public string CFlagsLine => Join(CFlags);
    /// <summary>Gli ldflags come stringa pronta per la command line.</summary>
    public string LdFlagsLine => Join(LdFlags);
    /// <summary>Le macro -D come stringa pronta per la command line.</summary>
    public string DefinesLine => Join(Defines);

    private static string Join(List<string> flags)
        => flags.Count == 0 ? "" : " " + string.Join(' ', flags);
}

/// <summary>Modella il file system.settings.json (default globali).</summary>
public sealed record SystemSettings
{
    /// <summary>Namespaces predefiniti globali.</summary>
    [JsonPropertyName("defaultNamespaces")]
    public List<string> DefaultNamespaces { get; init; } = ["SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME", "CONVERSIONS", "IO", "CONCURRENCY", "ARRAYS"];

    /// <summary>Cartelle di import predefinite.</summary>
    [JsonPropertyName("importFolders")]
    public List<string> ImportFolders { get; init; } = [];

    /// <summary>Configurazioni console predefinite.</summary>
    [JsonPropertyName("consoleConfiguration")]
    public List<global::CommonLib.Screens.ScreenConfig> ConsoleConfiguration { get; init; } = [];

    /// <summary>Cartelle di librerie predefinite.</summary>
    [JsonPropertyName("libraryFolders")]
    public List<string> LibraryFolders { get; init; } = [];

    /// <summary>Cartella predefinita per i progetti.</summary>
    [JsonPropertyName("defaultProjectsFolder")]
    public string DefaultProjectsFolder { get; init; } = "PROJECTS";

    /// <summary>Posizioni delle cartelle dati (config, skills, rag, graphs, libs, ...).
    /// Default: null → tutti i default del record (root del repo).</summary>
    [JsonPropertyName("paths")]
    public PathSettings? Paths { get; init; }

    /// <summary>Toolchain C e flag di compilazione/link (default GLOBALE condiviso).
    /// Default: null → i default del record <see cref="CompilerSettings"/>.</summary>
    [JsonPropertyName("compiler")]
    public CompilerSettings? Compiler { get; init; }
}

/// <summary>Configurazione finale fusa da system.settings + app.settings.</summary>
public sealed record EffectiveSettings
{
    /// <summary>Posizioni delle cartelle dati risolte (vedi <see cref="GmbiPaths"/>).</summary>
    public PathSettings? Paths { get; init; }

    /// <summary>Namespaces predefiniti da importare.</summary>
    public IReadOnlyList<string> DefaultNamespaces { get; init; } = [];
    /// <summary>Cartelle di import attive.</summary>
    public IReadOnlyList<string> ImportFolders { get; init; } = [];
    /// <summary>File di import attivi.</summary>
    public IReadOnlyList<string> ImportFiles { get; init; } = [];
    /// <summary>Configurazioni console attive.</summary>
    public IReadOnlyList<global::CommonLib.Screens.ScreenConfig> ConsoleConfiguration { get; init; } = [];
    /// <summary>Cartelle di librerie attive.</summary>
    public IReadOnlyList<string> LibraryFolders { get; init; } = [];
    /// <summary>Cartella predefinita per i progetti.</summary>
    public string DefaultProjectsFolder { get; init; } = "PROJECTS";

    /// <summary>Toolchain C e flag EFFETTIVI (globali + override per-applicazione, fusi da
    /// <see cref="CompilerSettings.Resolve"/>). Non null: se nessuno dei due file li dichiara,
    /// valgono i default del record. È la fonte unica che i backend C leggono per costruire
    /// la command line di cc.</summary>
    public CompilerSettings Compiler { get; init; } = new();
}
