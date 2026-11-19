// FILE: CommonLib.SettingsLoader.cs

using System.Text.Json;
using System.Text.Json.Serialization;
using CommonLib.Screens;

/*
 * SettingsLoader.cs — caricamento e fusione di `system.settings.json` + `app.settings.json`
 * (EffectiveSettings), NEL PROGETTO `CommonLib` così che sia disponibile a TUTTI i pipeline
 * che compilano BASIC: l'Orchestrator (Executor) e i pipeline ISOLATI di `Basic.Pipeline`
 * (pagine BSP, EVAL). Prima questa logica esisteva solo in `Basic.SettingsManager`
 * (progetto Executor): i pipeline isolati non potevano caricarla e compilavano con
 * `ImportFolders = []`, quindi una pagina .bsp non poteva importare NESSUNA libreria
 * BASIC (`IMPORT DASHBOARD` → "Nessuna funzione pubblica ..."). Bug reale trovato
 * verificando `shared/webapp` end-to-end.
 *
 * La logica è UNA: `Basic.SettingsManager.LoadSettings` delega qui (stessa semantica,
 * nessuna copia divergente).
 */

namespace CommonLib;

/// <summary>Caricatore unico delle impostazioni effettive (system + app) per un progetto.</summary>
public static class SettingsLoader
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase) }
    };

    /// <summary>
    /// Carica le impostazioni per un progetto, fondendo `system.settings` con `app.settings`.
    /// Se il percorso è un FILE viene usata la sua cartella (i chiamanti passano spesso la entry).
    /// </summary>
    public static EffectiveSettings Load(string projectRootDirectory, string? systemSettingsPath = null)
    {
        if (File.Exists(projectRootDirectory))
            projectRootDirectory = Path.GetDirectoryName(Path.GetFullPath(projectRootDirectory))
                                    ?? projectRootDirectory;

        var systemSettings = LoadSystem(projectRootDirectory, systemSettingsPath);
        var appSettings = LoadApp(FindAppSettings(projectRootDirectory));
        return Merge(systemSettings, appSettings);
    }

    /// <summary>
    /// `app.settings.json` del progetto: nella cartella indicata o RISALENDO (project-file
    /// discovery). Allineato al resolver C (`gmb_inc_resolve.c`): un programma in una
    /// sottocartella del progetto (es. samples/Modules/FolderResolution/chain/Chain.bas)
    /// deve vedere le impostazioni del progetto, altrimenti importFolders/searchFolders/
    /// importFiles dichiarati lì non valgono e l'IMPORT di un modulo locale fallisce solo
    /// su IL/PCode/VM-C (divergenza trovata col gate C-first).
    /// </summary>
    private static string FindAppSettings(string projectRootDirectory)
    {
        string start;
        try { start = Path.GetFullPath(projectRootDirectory); }
        catch { return Path.Combine(projectRootDirectory, "app.settings.json"); }
        var dir = new DirectoryInfo(start);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, "app.settings.json");
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }
        return Path.Combine(projectRootDirectory, "app.settings.json");
    }

    /// <summary>Carica `system.settings.json` (esplicito → copia locale → dir dell'eseguibile).</summary>
    public static SystemSettings LoadSystem(string projectRootDirectory, string? explicitPath = null)
    {
        string systemSettingsPath;
        if (!string.IsNullOrWhiteSpace(explicitPath) && File.Exists(explicitPath))
        {
            systemSettingsPath = explicitPath;
        }
        else
        {
            // 1) copia LOCALE del progetto (sostituisce quella del compilatore).
            systemSettingsPath = Path.Combine(projectRootDirectory, "system.settings.json");
            if (!File.Exists(systemSettingsPath))
            {
                // 2) dir del compilatore/interprete (distribuzione).
                var executablePath = FileSystemUtils.CoreAppPath();
                systemSettingsPath = Path.Combine(executablePath, "system.settings.json");
                if (!File.Exists(systemSettingsPath))
                    systemSettingsPath = Path.Combine(executablePath, "Assets", "Config", "system.settings.json");
            }
            if (!File.Exists(systemSettingsPath))
                return new SystemSettings();
        }

        try
        {
            var json = File.ReadAllText(systemSettingsPath);
            return JsonSerializer.Deserialize<SystemSettings>(json, JsonOptions) ?? new SystemSettings();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(
                $"AVVISO: Impossibile leggere o parsare 'system.settings.json'. Verranno usate le impostazioni predefinite. Errore: {ex.Message}");
            return new SystemSettings();
        }
    }

    /// <summary>Carica `app.settings.json` (null se assente o illeggibile).</summary>
    public static AppSettings? LoadApp(string filePath)
    {
        if (!File.Exists(filePath)) return null;
        try
        {
            var json = File.ReadAllText(filePath);
            return JsonSerializer.Deserialize<AppSettings>(json, JsonOptions);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(
                $"ERRORE: Impossibile leggere o parsare 'app.settings.json'. Il file verrà ignorato. Errore: {ex.Message}");
            return null;
        }
    }

    /// <summary>Merge con `app.settings` che ha la precedenza; risolve le palette in due passate.</summary>
    public static EffectiveSettings Merge(SystemSettings system, AppSettings? app)
    {
        // PASSATA 1: palette custom dichiarate in app.settings.
        if (app?.CustomPalettes != null)
        {
            foreach (var (name, data) in app.CustomPalettes)
            {
                try { PaletteManager.RegisterPalette(name, data); }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"[AVVISO] Impossibile registrare la palette custom '{name}': {ex.Message}");
                }
            }
        }

        // PASSATA 2: risoluzione delle palette per ogni configurazione console.
        var consoleConfigsToProcess = app?.ConsoleConfiguration ?? system.ConsoleConfiguration;
        var resolvedConsoleConfigs = new List<ScreenConfig>();
        foreach (var config in consoleConfigsToProcess)
        {
            byte[][] finalPalette;

            if (!string.IsNullOrEmpty(config.PaletteName) && config.PaletteData != null)
            {
                try
                {
                    PaletteManager.RegisterPalette(config.PaletteName, config.PaletteData);
                    finalPalette = config.PaletteData;
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine(
                        $"[AVVISO] Impossibile registrare la palette inline '{config.PaletteName}': {ex.Message}. Verrà usata una palette di default.");
                    finalPalette = PaletteManager.GetPalette("VGA16");
                }
            }
            else if (!string.IsNullOrEmpty(config.PaletteName))
            {
                if (PaletteManager.TryGetPalette(config.PaletteName, out var foundPalette))
                    finalPalette = foundPalette!;
                else
                {
                    Console.Error.WriteLine(
                        $"[AVVISO] Palette '{config.PaletteName}' non trovata. Verrà usata Console.");
                    finalPalette = PaletteManager.GetPalette("Console");
                }
            }
            else if (config.PaletteData != null) finalPalette = config.PaletteData;
            else finalPalette = PaletteManager.GetPalette("Console");

            resolvedConsoleConfigs.Add(config with { ResolvedPalette = finalPalette });
        }

        // I flag del compilatore si SOSTITUISCONO (mai accodati): fusione per-chiave in Resolve.
        var compiler = CompilerSettings.Resolve(system.Compiler, app?.Compiler);

        if (app == null)
        {
            return new EffectiveSettings
            {
                DefaultNamespaces = system.DefaultNamespaces.AsReadOnly(),
                ImportFolders = system.ImportFolders.AsReadOnly(),
                ImportFiles = new List<string>().AsReadOnly(),
                ConsoleConfiguration = resolvedConsoleConfigs.AsReadOnly(),
                LibraryFolders = system.LibraryFolders.AsReadOnly(),
                DefaultProjectsFolder = system.DefaultProjectsFolder,
                Compiler = compiler
            };
        }

        return new EffectiveSettings
        {
            DefaultNamespaces = [.. system.DefaultNamespaces.Distinct(), .. (app.DefaultNamespaces ?? []).Distinct()],
            ImportFolders = [.. system.ImportFolders.Distinct(), .. (app.ImportFolders ?? []).Distinct()],
            ImportFiles = (app.ImportFiles ?? []).Distinct().ToList().AsReadOnly(),
            ConsoleConfiguration = resolvedConsoleConfigs.AsReadOnly(),
            LibraryFolders = [.. system.LibraryFolders.Distinct(), .. (app.LibraryFolders ?? []).Distinct()],
            DefaultProjectsFolder = system.DefaultProjectsFolder,
            Compiler = compiler
        };
    }
}
