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
      "fontPath": "Sixtyfour.ttf",
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
    // La logica (lettura system/app + merge + palette + CompilerSettings.Resolve) vive in
    // `CommonLib.SettingsLoader`, così che anche i pipeline ISOLATI di `Basic.Pipeline`
    // (pagine BSP, EVAL) carichino gli stessi ImportFolders/SearchFolders dell'Orchestrator.
    // Qui resta solo la facciata pubblica storica (namespace Basic) usata da Executor/REPL.
    public static EffectiveSettings LoadSettings(string projectRootDirectory)
        => CommonLib.SettingsLoader.Load(projectRootDirectory);

    public static EffectiveSettings LoadSettings(string projectRootDirectory, string? systemSettingsPath)
        => CommonLib.SettingsLoader.Load(projectRootDirectory, systemSettingsPath);

}