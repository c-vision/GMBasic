using CommonGraphics.Consoles;
using CommonLib.Consoles;
// FILE: Interpreter.Fonts.Manager.cs

using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.RegularExpressions;
using SDL2;
#if WINDOWS
using Microsoft.Win32;
#endif

namespace CommonGraphics.Fonts;

/// <summary>
/// Definisce il tipo di font richiesto, per guidare la logica di fallback.
/// </summary>
public enum FontType
{
    /// <summary>
    /// Un font a larghezza fissa, adatto per console e codice.
    /// </summary>
    Monospace,

    /// <summary>
    /// Un font a larghezza variabile, adatto per grafica e UI.
    /// </summary>
    Proportional
}

/// <summary>
/// Rappresenta una voce nella cache dei font, includendo il percorso e un flag
/// che indica se è monospaced, per evitare controlli ripetuti ad ogni avvio.
/// </summary>
public sealed record FontCacheEntry(string Path, bool IsMonospaced);

/// <summary>
/// Risultato della verifica di un font. Contiene sempre un percorso di file fisico.
/// </summary>
public sealed record FontVerificationResult(string FontPath, int CharWidth, int CharHeight, bool IsMonospaced);

/// <summary>
/// Gestisce la scoperta, la risoluzione, il filtraggio e il caching dei font di sistema,
/// locali (nella cartella dell'eseguibile) e caricati dinamicamente.
/// Separa la gestione dei font di sistema (persistente) da quella dei font runtime (volatile).
/// </summary>
public sealed class FontManager
{
    #region Singleton & Caching

    private static readonly Lazy<FontManager> LazyInstance = new(() => new FontManager());
    public static FontManager Instance => LazyInstance.Value;

    private readonly string _cachePath;
    private const string SampleCacheFileName = "fontcache_system.json";

    private static readonly char[] MonoTestChars =
        ['i', 'a', 'Z', '%', '#', 'B', 'l', 'm', ',', '.', 'W', '0', 'M', ' '];

    /// <summary>
    /// Cache per i font di sistema, caricata da file.
    /// </summary>
    private readonly Dictionary<string, FontCacheEntry> _systemFontCache = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Registro in memoria per i font locali (trovati nella cartella del programma)
    /// o aggiunti dinamicamente. Questa cache è volatile e non viene salvata su disco.
    /// </summary>
    private readonly Dictionary<string, FontCacheEntry> _runtimeFonts = new(StringComparer.OrdinalIgnoreCase);

    private sealed record FontTestResult(int CharWidth, int CharHeight, bool IsMonospaced);

    #endregion

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="FontManager"/>.
    /// Il costruttore è privato per applicare il pattern Singleton.
    /// </summary>
    private FontManager()
    {
        var appDataPath = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        _cachePath = Path.Combine(appDataPath, "GMBASICInterpreter");
        Directory.CreateDirectory(_cachePath);

        SdlManager.Instance.Initialize();

        // Inizializza le due fonti di font in modo separato.
        InitializeSystemCache();
        ScanAndCacheLocalFonts();
    }

    /// <summary>
    /// Inizializza la cache dei font di sistema. Tenta di caricare i dati dalla cache su disco.
    /// Se la cache non esiste o è invalida, esegue una nuova scansione.
    /// </summary>
    private void InitializeSystemCache()
    {
        var cacheFile = Path.Combine(_cachePath, SampleCacheFileName);
        if (File.Exists(cacheFile))
        {
            try
            {
                var json = File.ReadAllText(cacheFile);
                var cachedMap = JsonSerializer.Deserialize<Dictionary<string, FontCacheEntry>>(json);
                if (cachedMap != null && cachedMap.Count != 0)
                {
                    foreach (var pair in cachedMap) _systemFontCache[pair.Key] = pair.Value;
                    return;
                }
            }
            catch (Exception)
            {
                // Debug.WriteLine(
                //     $"[FontManager] Impossibile leggere la cache di sistema, verrà ricreata. Errore: {ex.Message}");
            }
        }

        ScanAndCacheSystemFonts();
    }

    /// <summary>
    /// Scansiona la directory dell'eseguibile alla ricerca di font (.ttf, .otf),
    /// li testa e li aggiunge al registro dei font runtime.
    /// </summary>
    private void ScanAndCacheLocalFonts()
    {
        try
        {
            var exePath = AppContext.BaseDirectory;
            var fontFiles = Directory.EnumerateFiles(exePath, "*.ttf")
                .Concat(Directory.EnumerateFiles(exePath, "*.otf"));

            foreach (var fontFile in fontFiles)
            {
                // Per i font locali, usiamo il nome del file (senza estensione) come alias.
                var fontAlias = Path.GetFileNameWithoutExtension(fontFile);
                if (!_runtimeFonts.ContainsKey(fontAlias))
                {
                    var isMonospaced = IsFontMonospaced(fontFile);
                    _runtimeFonts[fontAlias] = new FontCacheEntry(fontFile, isMonospaced);
                    // Debug.WriteLine(
                    //     $"[FontManager] Registrato font locale: '{fontAlias}' (Monospaced: {isMonospaced})");
                }
            }
        }
        catch (Exception)
        {
            //Debug.WriteLine($"[FontManager] Errore durante la scansione dei font locali: {ex.Message}");
        }
    }

    /// <summary>
    /// Registra un nuovo font da un percorso di file specifico, aggiungendolo
    /// al registro dei font runtime. Utile per il comando FONT.
    /// </summary>
    /// <param name="fontPath">Il percorso completo del file del font.</param>
    /// <param name="registeredName">Il nome (alias) con cui il font è stato registrato.</param>
    /// <returns>True se il font è stato registrato con successo, altrimenti false.</returns>
    public bool RegisterFont(string fontPath, out string registeredName)
    {
        registeredName = string.Empty;
        if (string.IsNullOrWhiteSpace(fontPath) || !File.Exists(fontPath))
            return false;

        var fontAlias = Path.GetFileNameWithoutExtension(fontPath);
        if (_runtimeFonts.ContainsKey(fontAlias))
        {
            registeredName = fontAlias;
            return true; // Già registrato
        }

        var isMonospaced = IsFontMonospaced(fontPath);
        _runtimeFonts[fontAlias] = new FontCacheEntry(fontPath, isMonospaced);
        registeredName = fontAlias;
        //Debug.WriteLine($"[FontManager] Registrato font dinamico: '{fontAlias}' (Monospaced: {isMonospaced})");
        return true;
    }

    /// <summary>
    /// Esegue il test di dimensioni e monospaziatura su un puntatore a font già aperto,
    /// calcolando le metriche massime su un set di caratteri rappresentativo.
    /// </summary>
    /// <param name="font">Il puntatore al font SDL_ttf da testare.</param>
    /// <returns>Un risultato del test contenente le dimensioni massime e un flag booleano per la monospaziatura.</returns>
    private static FontTestResult TestFontDimensions(IntPtr font)
    {
        // Set di test esteso per catturare varie larghezze, inclusi i box drawing.
        char[] comprehensiveTestChars = ['i', 'a', 'Z', '%', '#', 'B', 'l', 'm', 'W', '0', 'M', ' ', '╔', '═', '█'];
        var maxWidth = 0;
        var maxHeight = 0;
        
        // Ottiene l'avanzamento del primo carattere di test per il check di monospaziatura.
        if (SDL_ttf.TTF_GlyphMetrics(font, comprehensiveTestChars[0], out _, out _, out _, out _, out var firstAdvance) != 0)
        {
            return new FontTestResult(0, 0, false); // Fallimento nel leggere le metriche
        }

        if (firstAdvance <= 0) return new FontTestResult(0, 0, false);
        
        var isMonospaced = true;

        // Itera su tutti i caratteri per trovare le dimensioni massime e verificare la monospaziatura.
        foreach (var ch in comprehensiveTestChars)
        {
            if (SDL_ttf.TTF_GlyphIsProvided(font, ch) <= 0) continue;

            if (SDL_ttf.TTF_GlyphMetrics(font, ch, out _, out _, out _, out _, out var currentAdvance) != 0)
            {
                // Se un glifo fallisce, non è affidabile.
                isMonospaced = false; 
                continue;
            }

            SDL_ttf.TTF_SizeUTF8(font, ch.ToString(), out var w, out var h);
            maxWidth = Math.Max(maxWidth, w);
            maxHeight = Math.Max(maxHeight, h);

            if (currentAdvance != firstAdvance)
            {
                isMonospaced = false;
            }
        }
        
        // Se non è stato trovato nessun carattere valido, usa l'altezza del font come fallback.
        if (maxHeight == 0)
        {
            maxHeight = SDL_ttf.TTF_FontHeight(font);
        }

        // Se è risultato monospaziato, la larghezza massima deve essere l'avanzamento costante.
        var finalWidth = isMonospaced ? firstAdvance : maxWidth;
        
        return new FontTestResult(finalWidth > 0 ? finalWidth : maxWidth, maxHeight, isMonospaced);
    }

    /// <summary>
    /// Esegue un test completo su un font (apertura e verifica) a partire dal suo percorso.
    /// </summary>
    private static FontTestResult? RunFontTest(string fontPath, int fontSize)
    {
        if (string.IsNullOrWhiteSpace(fontPath) || !File.Exists(fontPath) || fontSize <= 0)
            return null;

        var font = SDL_ttf.TTF_OpenFont(fontPath, fontSize);
        if (font == IntPtr.Zero)
        {
            //Debug.WriteLine($"[SDL_ttf] Impossibile aprire il font '{fontPath}': {SDL.SDL_GetError()}");
            return null;
        }

        try
        {
            return TestFontDimensions(font);
        }
        finally
        {
            SDL_ttf.TTF_CloseFont(font);
        }
    }

    /// <summary>
    /// Metodo di utility per determinare se un font è monospaziato.
    /// </summary>
    private static bool IsFontMonospaced(string fontPath, int fontSize = 16)
    {
        var result = RunFontTest(fontPath, fontSize);
        return result?.IsMonospaced ?? false;
    }

    /// <summary>
    /// Seleziona e verifica un font monospace valido, gestendo la logica di fallback.
    /// Questo è il punto di ingresso principale per ottenere un font per la console.
    /// </summary>
    /// <param name="requestedFontName">Il nome del font preferito (può essere un nome di famiglia o un alias di file).</param>
    /// <param name="fontSize">La dimensione in punti del font.</param>
    /// <returns>Un <see cref="FontVerificationResult"/> contenente il percorso fisico di un font monospace valido.</returns>
    /// <exception cref="InvalidOperationException">Se nessun font monospace valido può essere trovato.</exception>
    /// <summary>
    /// Seleziona e verifica un font monospace valido, gestendo la logica di fallback.
    /// Questo è il punto di ingresso principale per ottenere il PERCORSO di un font per la console.
    /// </summary>
    /// <param name="requestedFontName">Il nome del font preferito (può essere un nome di famiglia o un alias di file).</param>
    /// <returns>Il percorso fisico di un font ritenuto monospaziato.</returns>
    /// <exception cref="InvalidOperationException">Se nessun font monospace valido può essere trovato.</exception>
    public static string SelectMonospaceFont(string? requestedFontName)
    {
        // 1. Tenta di risolvere il font richiesto
        if (!string.IsNullOrWhiteSpace(requestedFontName))
        {
            var (path, entry) = Instance.FindFontEntry(requestedFontName);
            // 2. Se trovato, verifica che sia effettivamente monospaziato
            if (entry is { IsMonospaced: true } && path != null)
            {
                return path;
            }
        }

        // 3. Se il font richiesto non è valido o non è monospaziato, avvia il fallback
        string[] fallbackFonts;
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) fallbackFonts = ["Consolas", "Courier New"];
        else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX)) fallbackFonts = ["DejaVuSansMono", "Monaco", "Andale Mono", "Menlo"];
        else fallbackFonts = ["DejaVu Sans Mono", "Liberation Mono"];

        foreach (var fallbackName in fallbackFonts)
        {
            var (path, entry) = Instance.FindFontEntry(fallbackName);
            if (entry is { IsMonospaced: true } && path != null)
            {
                return path;
            }
        }
        
        throw new InvalidOperationException("CRITICO: Nessun font monospace valido trovato nel sistema o localmente.");
    }

    /// <summary>
    /// Esegue la scansione dei font di sistema e salva i risultati nella cache persistente.
    /// </summary>
    private void ScanAndCacheSystemFonts()
    {
        _systemFontCache.Clear();
        var tempMap = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) ScanWindowsFonts(tempMap);
        else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX)) ScanMacOsFonts(tempMap);
        else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux)) ScanLinuxFonts(tempMap);

        foreach (var pair in tempMap)
        {
            var isMonospaced = IsFontMonospaced(pair.Value);
            _systemFontCache[pair.Key] = new FontCacheEntry(pair.Value, isMonospaced);
        }

        if (_systemFontCache.Count != 0)
        {
            try
            {
                var cacheFile = Path.Combine(_cachePath, SampleCacheFileName);
                var json = JsonSerializer.Serialize(_systemFontCache,
                    new JsonSerializerOptions { WriteIndented = true });
                File.WriteAllText(cacheFile, json);
            }
            catch (Exception)
            {
                //Debug.WriteLine($"[FontManager] Impossibile salvare la cache di sistema: {ex.Message}");
            }
        }
    }

    #region Metodi di Scansione per Piattaforma (OS-specific)

#if WINDOWS
    [System.Runtime.Versioning.SupportedOSPlatform("windows")]
    private void ScanWindowsFonts(Dictionary<string, string> map)
    {
        using var key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts");
        if (key == null) return;
        var fontsFolderPath = Environment.GetFolderPath(Environment.SpecialFolder.Fonts);
        foreach (var valueName in key.GetValueNames())
        {
            var familyName = valueName.Split(new[] { " & ", " (" }, StringSplitOptions.RemoveEmptyEntries)[0];
            var fileName = key.GetValue(valueName) as string;
            if (!string.IsNullOrEmpty(fileName))
            {
                var fullPath = Path.IsPathRooted(fileName) ? fileName : Path.Combine(fontsFolderPath, fileName);
                map.TryAdd(familyName, fullPath);
            }
        }
    }
#else
    private void ScanWindowsFonts(Dictionary<string, string> map)
    {
    }
#endif

    [System.Runtime.Versioning.SupportedOSPlatform("macos")]
    private void ScanMacOsFonts(Dictionary<string, string> map)
    {
        try
        {
            using var process = new Process();
            process.StartInfo = new ProcessStartInfo
            {
                FileName = "/usr/sbin/system_profiler", Arguments = "-json SPFontsDataType",
                RedirectStandardOutput = true, UseShellExecute = false, CreateNoWindow = true,
                StandardOutputEncoding = System.Text.Encoding.UTF8
            };
            process.Start();
            var output = process.StandardOutput.ReadToEnd();
            process.WaitForExit();

            using var doc = JsonDocument.Parse(output);
            if (!doc.RootElement.TryGetProperty("SPFontsDataType", out var fontsData)) return;

            foreach (var fontEntry in fontsData.EnumerateArray())
            {
                if (fontEntry.TryGetProperty("path", out var pathElement) &&
                    fontEntry.TryGetProperty("typefaces", out var typefacesElement) &&
                    typefacesElement.ValueKind == JsonValueKind.Array && typefacesElement.EnumerateArray().Any())
                {
                    var filePath = pathElement.GetString();
                    if (string.IsNullOrEmpty(filePath)) continue;

                    var firstTypeface = typefacesElement.EnumerateArray().First();
                    if (firstTypeface.TryGetProperty("_name", out var nameElement))
                    {
                        var familyName = nameElement.GetString();
                        if (string.IsNullOrEmpty(familyName)) continue;

                        familyName = Regex.Replace(familyName, @"(PS-)?(Bold|Italic|Regular|Medium|Light|MT|Pro)$", "",
                            RegexOptions.IgnoreCase).Trim(' ', '-');
                        map.TryAdd(familyName, filePath);
                    }
                }
            }
        }
        catch (Exception)
        {
            //Debug.WriteLine($"[FontManager] Errore durante l'analisi di 'system_profiler': {ex.Message}");
        }
    }

    [System.Runtime.Versioning.SupportedOSPlatform("linux")]
    private void ScanLinuxFonts(Dictionary<string, string> map)
    {
        try
        {
            using var process = new Process();
            process.StartInfo = new ProcessStartInfo
            {
                FileName = "fc-list", Arguments = ": family file", RedirectStandardOutput = true,
                UseShellExecute = false, CreateNoWindow = true,
            };
            process.Start();
            while (!process.StandardOutput.EndOfStream)
            {
                var line = process.StandardOutput.ReadLine();
                if (string.IsNullOrEmpty(line)) continue;
                var parts = line.Split(':');
                if (parts.Length < 2) continue;
                var familyPart = parts[1].Trim();
                var familyName = familyPart.Split(',')[0];
                var filePath = parts[0].Trim();
                map.TryAdd(familyName, filePath);
            }
        }
        catch (Exception)
        {
            //Debug.WriteLine($"[FontManager] Impossibile eseguire 'fc-list': {ex.Message}.");
        }
    }

    #endregion

    /// <summary>
    /// Trova il percorso e la voce di cache per un dato nome di font, cercando
    /// prima tra i font runtime e poi tra quelli di sistema.
    /// </summary>
    /// <param name="fontName">Nome/alias/percorso del font da trovare.</param>
    /// <returns>Una tupla contenente il percorso e la voce di cache, o (null, null) se non trovato.</returns>
    private (string? Path, FontCacheEntry? Entry) FindFontEntry(string fontName)
    {
        // 1. Controlla se il nome è un percorso di file valido.
        if (File.Exists(fontName))
        {
            if (!RegisterFont(fontName, out var registeredName)) return (null, null);
            return (fontName, _runtimeFonts[registeredName]);
        }

        // 2. Cerca tra i font runtime (locali/dinamici).
        if (_runtimeFonts.TryGetValue(fontName, out var runtimeEntry))
            return (runtimeEntry.Path, runtimeEntry);

        // 2b. Alias senza estensione: la config usa "C64_Pro_Mono-STYLE.otf", i font locali
        //     sono registrati col nome-file senza estensione ("C64_Pro_Mono-STYLE").
        var stem = Path.GetFileNameWithoutExtension(fontName);
        if (stem.Length > 0 && _runtimeFonts.TryGetValue(stem, out var stemEntry))
            return (stemEntry.Path, stemEntry);

        // 3. Cerca tra i font di sistema.
        if (_systemFontCache.TryGetValue(fontName, out var systemEntry))
            return (systemEntry.Path, systemEntry);

        return (null, null);
    }

    /// <summary>
    /// Restituisce il percorso di un font, cercando in tutti i registri e usando un fallback.
    /// Questo metodo è agnostico al tipo di font e serve per usi generici (es. grafica).
    /// </summary>
    /// <param name="fontName">Il nome del font da risolvere.</param>
    /// <param name="type">Il tipo di font richiesto per guidare il fallback (Monospace o Proportional).</param>
    /// <returns>Il percorso del file del miglior font trovato, o una stringa vuota.</returns>
    public string GetFontPath(string? fontName, FontType type)
    {
        // 1. Tenta di risolvere il font richiesto tramite il suo nome/alias/percorso.
        if (!string.IsNullOrEmpty(fontName))
        {
            var (path, _) = FindFontEntry(fontName);
            if (path != null) return path;
        }

        // 2. Se non trovato, avvia la logica di fallback basata sul tipo richiesto.
        string[] fallbackNames = type == FontType.Monospace
            ? RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? ["Consolas", "Courier New"] :
            RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? ["DejaVuSansMono", "Monaco", "Menlo"] :
            ["DejaVu Sans Mono", "Liberation Mono"]
            : RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? ["Arial", "Calibri"] :
                RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? ["Helvetica", "Geneva"] :
                    ["DejaVu Sans", "Liberation Sans"];

        foreach (var fallback in fallbackNames)
        {
            if (_systemFontCache.TryGetValue(fallback, out var fallbackEntry))
            {
                return fallbackEntry.Path;
            }
        }

        // 3. Come ultima risorsa, restituisce il primo font disponibile di qualsiasi tipo.
        if (_runtimeFonts.Count != 0) return _runtimeFonts.Values.First().Path;
        if (_systemFontCache.Count != 0) return _systemFontCache.Values.First().Path;

        return string.Empty; // Nessun font trovato
    }

    /// <summary>
    /// Restituisce una lista di tutti i nomi di font disponibili (sistema e runtime).
    /// </summary>
    /// <returns>Una lista in sola lettura dei nomi dei font.</returns>
    public IReadOnlyList<string> GetAvailableFontFamilies()
    {
        return _systemFontCache.Keys.Union(_runtimeFonts.Keys, StringComparer.OrdinalIgnoreCase)
            .OrderBy(name => name).ToList().AsReadOnly();
    }
}