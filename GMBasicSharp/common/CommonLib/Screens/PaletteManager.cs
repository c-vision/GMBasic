// FILE: CommonLib.Screens.PaletteManager.cs

using System.Text.Json;
using System.Text.Json.Serialization;

/* 
 * PaletteManager.cs: registro centralizzato per la gestione delle palette di colori
 * 
 * FUNZIONI:
 * - RegisterPalette(name, data): registra una nuova palette
 * - GetPalette(name): recupera una palette per nome
 * - TryGetPalette(name, out palette): tentativo di recupero senza eccezioni
 * 
 * CLASSI:
 * - PaletteConverter: converter JSON personalizzato per palette
 */

namespace CommonLib.Screens;

/// <summary>
/// Fornisce un registro centralizzato e statico per la gestione delle palette di colori.
/// Inizializzato una sola volta con le palette predefinite (VGA16, VGA256, C64, C64CRT, C64Classic)
/// e può essere esteso con palette personalizzate caricate dai file di configurazione.
/// Thread-safe per le letture grazie a Dictionary&lt;,&gt; di .NET Core.
/// Usato da PaletteConverter per la deserializzazione JSON delle configurazioni schermo.
/// </summary>
public static class PaletteManager
{
    /// <summary>Registro interno che mappa nomi (case-insensitive) a definizioni di palette (byte[][] RGB).</summary>
    private static readonly Dictionary<string, byte[][]> Palettes;

    /// <summary>
    /// Costruttore statico: registra tutte le palette predefinite dal factory method Palettes.
    /// Eseguito una sola volta all'accesso alla classe.
    /// </summary>
    static PaletteManager()
    {
        Palettes = new Dictionary<string, byte[][]>(StringComparer.OrdinalIgnoreCase);

        // Registra tutte le palette predefinite
        RegisterPalette("VGA16", ScreenConfig.Palettes.GetVGA16Palette());
        RegisterPalette("VGA256", ScreenConfig.Palettes.GetVGA256Palette());
        RegisterPalette("C64", ScreenConfig.Palettes.GetC64Palette());
        RegisterPalette("C64CRT", ScreenConfig.Palettes.GetC64CrtPalette());
        RegisterPalette("C64Classic", ScreenConfig.Palettes.GetC64ClassicPalette());

        // Registra tutte le NamedPalette
        foreach (var np in ScreenConfig.Palettes.GetAllNamedPalettes())
            RegisterPalette(np.Name, np.ToRgbArray());
    }

    /// <summary>Registra una nuova palette (o aggiorna una esistente) nel registro globale.</summary>
    /// <param name="name">Nome univoco (case-insensitive) della palette.</param>
    /// <param name="data">Array RGB byte[][] che definisce i colori della palette.</param>
    public static void RegisterPalette(string name, byte[][] data)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name, nameof(name));
        ArgumentNullException.ThrowIfNull(data, nameof(data));
        Palettes[name] = data;
    }

    /// <summary>Recupera una palette dal registro per nome.</summary>
    /// <param name="name">Nome (case-insensitive) della palette.</param>
    /// <returns>Array RGB byte[][] della palette.</returns>
    /// <exception cref="KeyNotFoundException">Se nessuna palette con quel nome è registrata.</exception>
    public static byte[][] GetPalette(string name)
    {
        if (Palettes.TryGetValue(name, out var palette))
            return palette;
        throw new KeyNotFoundException($"Nessuna palette con nome '{name}' è stata trovata nel registro.");
    }

    /// <summary>Tentativo di recupero palette senza eccezioni.</summary>
    /// <param name="name">Nome della palette.</param>
    /// <param name="palette">La palette se trovata, null altrimenti.</param>
    /// <returns>True se trovata, false altrimenti.</returns>
    public static bool TryGetPalette(string name, out byte[][]? palette)
    {
        return Palettes.TryGetValue(name, out palette);
    }
}

/// <summary>
/// Converter JSON personalizzato per deserializzare la proprietà Palette di ScreenConfig.
/// Supporta due formati: nome di palette registrata (es. "VGA16") o array di byte inline.
/// In caso di errore, restituisce la palette VGA16 come fallback silenzioso.
/// </summary>
public sealed class PaletteConverter : JsonConverter<byte[][]>
{
    /// <summary>Legge una palette dal JSON: nome (stringa) o definizione inline (array byte[][]).</summary>
    public override byte[][]? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        if (reader.TokenType == JsonTokenType.String)
        {
            var paletteName = reader.GetString();
            if (string.IsNullOrEmpty(paletteName))
                return PaletteManager.GetPalette("Console");
            if (PaletteManager.TryGetPalette(paletteName, out var palette))
                return palette;
            Console.Error.WriteLine($"[AVVISO] Palette '{paletteName}' non registrata. Usata Console.");
            return PaletteManager.GetPalette("Console");
        }
        try
        {
            return JsonSerializer.Deserialize<byte[][]>(ref reader, options);
        }
        catch (JsonException)
        {
            Console.Error.WriteLine("[AVVISO] Formato palette inline non valido. Usata Console.");
            return PaletteManager.GetPalette("Console");
        }
    }

    /// <summary>Serializza la palette come array di byte nel JSON.</summary>
    public override void Write(Utf8JsonWriter writer, byte[][] value, JsonSerializerOptions options)
    {
        JsonSerializer.Serialize(writer, value, options);
    }
}
