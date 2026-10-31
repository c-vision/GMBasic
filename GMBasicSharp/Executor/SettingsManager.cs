// FILE: Interpreter.SettingsManager.cs

using System.Text.Json;
using System.Text.Json.Serialization;
using CommonLib;
using CommonLib.Screens;

namespace Basic;
/*
------------------------------------------------------------------------------------------------------------------------
      GUIDA DI RIFERIMENTO ALLA GESTIONE DELLA CONFIGURAZIONE (Rev. 3 - Completa)
------------------------------------------------------------------------------------------------------------------------
Questo sistema permette di configurare il comportamento dell'interprete tramite due file JSON:
`system.settings.json` per i default globali e `app.settings.json` per le personalizzazioni di progetto.

1.  `system.settings.json`:
   - Posizione: Nella stessa cartella dell'eseguibile dell'interprete.
   - Scopo: Fornire le impostazioni predefinite a livello di sistema.

2.  `app.settings.json`:
   - Posizione: Nella stessa cartella del file .bas principale di un progetto.
   - Scopo: Permettere a un singolo progetto di personalizzare e sovrascrivere
            le impostazioni di sistema. Ha sempre la precedenza.

------------------------------------------------------------------------------------------------------------------------
                                      PARAMETRI DI CONFIGURAZIONE
------------------------------------------------------------------------------------------------------------------------

---
1. `defaultNamespaces` (Lista di Stringhe)
---
  Specifica quali namespace vengono importati automaticamente in ogni file, come se
  fosse presente un `IMPORT Namespace.*` all'inizio.
  
  Esempio (`app.settings.json`):
  "defaultNamespaces": [ "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "IO", "MYCOMPANY.UTILS" ]

---
2. `importFolders` (Lista di Stringhe)
---
  Specifica percorsi di cartelle aggiuntive (relativi alla cartella del progetto) in cui il
  compilatore cercherà i file `.bas` per risolvere i namespace.

  Esempio (`app.settings.json`):
  "importFolders": [ "lib/ui", "lib/data" ]

---
3. `importFiles` (Lista di Stringhe)
---
  Specifica percorsi di file `.bas` specifici (relativi alla cartella del progetto) da includere
  sempre nel processo di compilazione.

  Esempio (`app.settings.json`):
  "importFiles": [ "src/common/utils.bas" ]

---
4. `libraryFolders` (Lista di Stringhe)
---
  Specifica percorsi di cartelle aggiuntive (relativi alla cartella del progetto) in cui il
  runtime cercherà le librerie native (`.dll`, `.so`) per le chiamate FFI (`DECLARE`).

  Esempio (`app.settings.json`):
  "libraryFolders": [ "bin/x64", "vendor/lib" ]

---
5. `customPalettes` (Dizionario)
---
  Permette di definire palette di colori personalizzate che possono poi essere referenziate
  per nome nella sezione `consoleConfiguration`. La chiave è il nome della palette, il valore
  è un array di array, dove ogni array interno rappresenta un colore [R, G, B].

  Esempio (`app.settings.json`):
  "customPalettes": {
    "SolarizedDark": [
      [7, 54, 66], 
      [220, 50, 47], 
      [133, 153, 0]
    ],
    "Monokai": [
      [39, 40, 34],
      [249, 38, 114]
    ]
  }

---
6. `consoleConfiguration` (Lista di Oggetti)
---
  Configura le console predefinite (da 0 a 3). Ogni oggetto nella lista corrisponde a una console.

  **Sotto-parametri per ogni oggetto `consoleConfiguration`:**

  - `charGridWidth`, `charGridHeight`: Dimensioni della griglia di testo (es. 80, 25).
  - `fontPath`: Nome del font (es. "Fira Code") o percorso del file (es. "font/myfont.ttf").
  - `fontSize`: Dimensione del font in punti.
  - `palette` (stringa): Nome di una palette da usare (predefinita come "VGA16", "C64Classic"
    o una definita in `customPalettes`).
  - `paletteData` (array): Permette di definire una palette "al volo" solo per questa console.
    Se `palette` è presente, `paletteData` viene usato per registrare una nuova palette con quel nome.

  **Esempio Completo (`app.settings.json`):**
  "consoleConfiguration": [
    {
      // Console 0: Usa la palette built-in C64
      "charGridWidth": 40,
      "charGridHeight": 25,
      "fontPath": "C64_Pro_Mono-STYLE.otf",
      "fontSize": 16,
      "palette": "C64Classic"
    },
    {
      // Console 1: Usa la palette custom "SolarizedDark" definita sopra
      "charGridWidth": 120,
      "charGridHeight": 50,
      "fontPath": "Fira Code",
      "fontSize": 14,
      "palette": "SolarizedDark"
    },
    {
      // Console 2: Definisce e usa una palette "Grayscale" al volo
      "charGridWidth": 80,
      "charGridHeight": 25,
      "palette": "GrayscaleTheme",
      "paletteData": [
        [0, 0, 0], [85, 85, 85], [170, 170, 170], [255, 255, 255]
      ]
    }
  ]
------------------------------------------------------------------------------------------------------------------------
*/

/// <summary>
/// Gestisce il caricamento e il merging dei file di configurazione.
/// </summary>
public static class SettingsManager
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase) }
    };

    /// <summary>
    /// Carica le impostazioni per un dato progetto, fondendo `system.settings` con `app.settings`.
    /// </summary>
    /// <param name="projectRootDirectory">Il percorso della directory principale del progetto.</param>
    /// <returns>Un oggetto EffectiveSettings con la configurazione finale.</returns>
    public static EffectiveSettings LoadSettings(string projectRootDirectory)
        => LoadSettings(projectRootDirectory, null);

    /// <summary>
    /// Come <see cref="LoadSettings(string)"/>, ma con il percorso di `system.settings.json` esplicito.
    /// Serve ai gate/harness (C99, C99Native, parity), che girano FUORI dalla radice del progetto:
    /// senza questo overload leggerebbero la copia accanto all'eseguibile (in `bin/`, potenzialmente
    /// stantia invece del template canonico in `Resources/`).
    /// </summary>
    /// <param name="projectRootDirectory">Radice del progetto (per `app.settings.json` e i default relativi).</param>
    /// <param name="systemSettingsPath">Percorso di `system.settings.json`; null = ricerca standard.</param>
    public static EffectiveSettings LoadSettings(string projectRootDirectory, string? systemSettingsPath)
    {
        // Alcuni chiamanti (Orchestrator.Compile/CompileFromString) passano il FILE della entry
        // invece della cartella: senza questa normalizzazione `Path.Combine(..., "app.settings.json")`
        // finiva dentro `.../Entry.bas/app.settings.json` e l'app.settings del programma veniva
        // IGNORATO (importFolders/importFiles/searchFolders compresi).
        if (File.Exists(projectRootDirectory))
            projectRootDirectory = Path.GetDirectoryName(Path.GetFullPath(projectRootDirectory))
                                    ?? projectRootDirectory;

        var systemSettings = LoadSystemSettings(projectRootDirectory, systemSettingsPath);
        var appSettingsPath = Path.Combine(projectRootDirectory, "app.settings.json");
        var appSettings = LoadAppSettings(appSettingsPath);
        return MergeSettings(systemSettings, appSettings);
    }

    /// <summary>
    /// Carica il file `system.settings.json` dalla directory dell'interprete.
    /// </summary>
    private static SystemSettings LoadSystemSettings(string projectRootDirectory, string? explicitPath = null)
    {
        string systemSettingsPath;
        // 0) percorso ESPLICITO (gate/harness → il template canonico in Resources/).
        if (!string.IsNullOrWhiteSpace(explicitPath) && File.Exists(explicitPath))
        {
            systemSettingsPath = explicitPath;
        }
        else
        {
        // 1) copia LOCALE nella dir dell'app (se il programmatore l'ha copiata lì):
        //    sostituisce quella nella dir del compilatore.
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
        {
            return new SystemSettings();
        }
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

    /// <summary>
    /// Carica il file `app.settings.json` da un percorso specifico.
    /// </summary>
    private static AppSettings? LoadAppSettings(string filePath)
    {
        if (!File.Exists(filePath))
        {
            return null;
        }

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

    /// <summary>
    /// Esegue il merge delle impostazioni, con `app.settings` che ha la precedenza,
    /// e risolve le palette di colori in un processo a due passate.
    /// </summary>
    private static EffectiveSettings MergeSettings(SystemSettings system, AppSettings? app)
    {
        // PASSATA 1: Registra tutte le palette custom definite in app.settings.
        if (app?.CustomPalettes != null)
        {
            foreach (var (name, data) in app.CustomPalettes)
            {
                try
                {
                    PaletteManager.RegisterPalette(name, data);
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine(
                        $"[AVVISO] Impossibile registrare la palette custom '{name}': {ex.Message}");
                }
            }
        }

        // Determina la lista di configurazioni console da usare. Quella di App ha la precedenza.
        var consoleConfigsToProcess = app?.ConsoleConfiguration ?? system.ConsoleConfiguration;

        // PASSATA 2: Risolve le palette per ogni configurazione.
        var resolvedConsoleConfigs = new List<ScreenConfig>();
        foreach (var config in consoleConfigsToProcess)
        {
            byte[][] finalPalette;

            // Caso 1: Definizione inline di una nuova palette con nome.
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
            // Caso 2: Riferimento a una palette esistente tramite nome.
            else if (!string.IsNullOrEmpty(config.PaletteName))
            {
                if (PaletteManager.TryGetPalette(config.PaletteName, out var foundPalette))
                {
                    finalPalette = foundPalette!;
                }
                else
                {
                    Console.Error.WriteLine(
                        $"[AVVISO] Palette '{config.PaletteName}' non trovata. Verrà usata Console.");
                    finalPalette = PaletteManager.GetPalette("Console");
                }
            }
            // Caso 3: Definizione inline senza nome.
            else if (config.PaletteData != null)
            {
                finalPalette = config.PaletteData;
            }
            // Caso 4: Nessuna palette specificata, usa il default.
            else
            {
                finalPalette = PaletteManager.GetPalette("Console");
            }

            // Crea una nuova istanza di ScreenConfig con la palette risolta.
            resolvedConsoleConfigs.Add(config with { ResolvedPalette = finalPalette });
        }

        // Unisce le altre impostazioni.
        // `compiler` NON segue la semantica di unione delle liste sopra: i flag del
        // compilatore si sostituiscono (accodare -O2 e -O3 renderebbe il comando ambiguo).
        // La fusione per-chiave è in CompilerSettings.Resolve.
        var compiler = global::CommonLib.CompilerSettings.Resolve(system.Compiler, app?.Compiler);

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
            DefaultNamespaces = [..system.DefaultNamespaces.Distinct(), ..(app.DefaultNamespaces ?? []).Distinct()],
            ImportFolders = [..system.ImportFolders.Distinct(), ..(app.ImportFolders ?? []).Distinct()],
            ImportFiles = (app.ImportFiles ?? []).Distinct().ToList().AsReadOnly(),
            ConsoleConfiguration = resolvedConsoleConfigs.AsReadOnly(),
            LibraryFolders = [..system.LibraryFolders.Distinct(), ..(app.LibraryFolders ?? []).Distinct()],
            DefaultProjectsFolder = system.DefaultProjectsFolder,
            Compiler = compiler
        };
    }
}