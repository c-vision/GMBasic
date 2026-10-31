// FILE: CommonLib.Screens.ScreenConfig.cs

using System.Text.Json.Serialization;

/* 
 * ScreenConfig.cs: parametri di configurazione per una console/schermo (dimensioni, palette, font, finestra)
 * 
 * CLASSI/ENUM:
 * - FontHinting: enum per hinting font SDL_ttf (Normal, Light, Monospace, None)
 * - ScreenConfig: record immutabile con tutti i parametri schermo (dimensioni, palette, font, finestra)
 * - ScreenConfig.Palettes: palette predefinite (VGA16, VGA256, C64, C64CRT, C64Classic)
 * - GetDefault(mode): configurazione predefinita per una data modalità
 * - GetOptimalConfigForMonospaceRepl(fontFamilyName, fontSize, gridWidth, gridHeight): configurazione ottimale REPL
 * - GetC64PetsciiRemapping(): mappa box drawing Unicode → PETSCII
 */

namespace CommonLib.Screens;

/// <summary>
/// Definisce la modalità di visualizzazione della finestra SDL.
/// </summary>
public enum ScreenWindowMode
{
    Windowed,
    Fullscreen,
    FullscreenDesktop,
    Borderless
}

/// <summary>
/// Definisce i livelli di hinting per il rendering dei font con SDL_ttf.
/// Il hinting controlla quanto il renderer aggiusta i contorni del font per allinearli
/// alla griglia di pixel, influenzando nitidezza e fedeltà al design originale.
/// </summary>
public enum SdlFontHinting
{
    /// <summary>Hinting normale, adatto per la maggior parte dei font moderni con anti-aliasing.</summary>
    Normal,
    /// <summary>Hinting leggero, produce un rendering più fedele al design originale del font.</summary>
    Light,
    /// <summary>Hinting ottimizzato per font a larghezza fissa (pixel-perfect). Ideale per font bitmap o retrò.</summary>
    Monospace,
    /// <summary>Disattiva l'hinting per un rendering senza aggiustamenti.</summary>
    None
}

/// <summary>
/// Contiene tutti i parametri per definire uno schermo, che sia la console REPL emulata
/// o la finestra grafica del programma BASIC. Record immutabile (con init-only properties),
/// popolato dal SettingsManager dopo aver interpretato le configurazioni da file JSON.
/// Supporta sia console a caratteri (CharGridWidth/Height) che aree di disegno pixel
/// (LogicalWidth/Height), con palette risolta, font e modalità di rendering HiDPI.
/// </summary>
public sealed record ScreenConfig
{
    /// <summary>La modalità con cui questa configurazione è stata creata (<see cref="GetDefault"/>).
    /// Usata per riconoscere in modo esplicito preset come C64 senza ispezionare i singoli
    /// parametri (dimensioni/palette) in modo fragile.</summary>
    public ScreenMode Mode { get; init; }

    // === Parametri Dimensioni ===

    /// <summary>Per la grafica, la larghezza logica dell'area di disegno in pixel.</summary>
    public int LogicalWidth { get; init; }

    /// <summary>Per la grafica, l'altezza logica dell'area di disegno in pixel.</summary>
    public int LogicalHeight { get; init; }

    /// <summary>Per le console a caratteri, la larghezza della griglia in numero di caratteri.</summary>
    public int? CharGridWidth { get; init; }

    /// <summary>Per le console a caratteri, l'altezza della griglia in numero di caratteri.</summary>
    public int? CharGridHeight { get; init; }

    // === Parametri Palette e Colori ===

    /// <summary>Palette di colori finale e risolta, pronta per essere usata dal renderer.
    /// Popolata dal SettingsManager dopo aver interpretato PaletteName e PaletteData.
    /// Non deserializzata direttamente dal JSON.</summary>
    [JsonIgnore]
    public byte[][] ResolvedPalette { get; set; } = [];

    /// <summary>Nome della palette da usare (es. "VGA16", "C64Classic"). Usata dalla deserializzazione JSON.</summary>
    [JsonPropertyName("palette")]
    public string? PaletteName { get; init; }

    /// <summary>Definizione inline di palette custom. Se presente con PaletteName, definisce una nuova palette registrabile.</summary>
    [JsonPropertyName("paletteData")]
    public byte[][]? PaletteData { get; init; }

    /// <summary>Indice nella palette del colore di primo piano (testo) predefinito.</summary>
    public int DefaultFgColor { get; init; }

    /// <summary>Indice nella palette del colore di sfondo predefinito.</summary>
    public int DefaultBgColor { get; init; }

    // === Parametri Finestra ===

    /// <summary>Larghezza desiderata della finestra in pixel.</summary>
    public int? WindowWidth { get; init; }

    /// <summary>Altezza desiderata della finestra in pixel.</summary>
    public int? WindowHeight { get; init; }

    /// <summary>Posizione X iniziale della finestra sullo schermo.</summary>
    public int? WindowX { get; init; }

    /// <summary>Posizione Y iniziale della finestra sullo schermo.</summary>
    public int? WindowY { get; init; }

    // === Parametri Font ===

    /// <summary>Percorso del file del font (es. "font.ttf") o nome della famiglia (es. "Fira Code").</summary>
    public string? FontPath { get; init; }

    /// <summary>Dimensione in punti del font.</summary>
    public int? FontSize { get; init; }

    /// <summary>Modalità di hinting per il rendering del font (ottimizzazione leggibilità).</summary>
    public SdlFontHinting FontHinting { get; init; } = SdlFontHinting.Normal;

    /// <summary>Se true, usa il meccanismo di scaling logico di SDL per gestione HiDPI.
    /// False per font retrò che richiedono controllo pixel-perfect.</summary>
    public bool UseLogicalSizeScaling { get; init; } = true;

    /// <summary>Se true, esegue il rendering alla risoluzione nativa del display (Retina/HiDPI).</summary>
    public bool UseNativeResolutionRendering { get; init; }

    /// <summary>Modalità di visualizzazione della finestra (Windowed, Fullscreen, FullscreenDesktop, Borderless).</summary>
    public ScreenWindowMode WindowMode { get; init; } = ScreenWindowMode.Windowed;

    #region Palette Predefinite

    /// <summary>
    /// Una palette nominativa: array di colori RGB dove ogni colore ha un nome.
    /// Usata da CONSOLE.PALETTE.USE e affini per esporre i colori con nome al BASIC.
    /// </summary>
    public sealed record NamedPalette(string Name, (string ColorName, byte R, byte G, byte B)[] Colors)
    {
        /// <summary>Converte in byte[][] (solo RGB, nessun nome) per compatibilità col sistema esistente.</summary>
        public byte[][] ToRgbArray() => Colors.Select(c => new byte[] { c.R, c.G, c.B }).ToArray();
    }

    /// <summary>Fornisce un accesso centralizzato alle palette di colori standard per tutte le modalità.</summary>
    public static class Palettes
    {
        public static NamedPalette[] GetAllNamedPalettes()
        {
            var names = NativeScreenConfig.NamedList();
            var list = new NamedPalette[names.Length];
            for (int i = 0; i < names.Length; i++) list[i] = NamedFromC(names[i]);
            return list;
        }

        public static NamedPalette GetNamed(string name) => NamedFromC(name);
        public static NamedPalette GetConsoleNamed() => NamedFromC("Console");

        private static NamedPalette NamedFromC(string name)
        {
            var flat = new byte[48];
            var cols = new (string, byte, byte, byte)[16];
            if (NativeScreenConfig.gmb_screen_palette_named(name, flat) != 0)
                for (int i = 0; i < 16; i++) cols[i] = ($"C{i}", flat[i * 3], flat[i * 3 + 1], flat[i * 3 + 2]);
            else
                for (int i = 0; i < 16; i++) cols[i] = ($"C{i}", (byte)0, (byte)0, (byte)0);
            return new NamedPalette(name, cols);
        }

        public static byte[][] GetVGA16Palette() { var b = new byte[48]; NativeScreenConfig.gmb_screen_palette_vga16(b); return NativeScreenConfig.Rgb(b, 16); }

        /// <summary>Palette VGA16 GESTITA (stessa tabella di <c>gmb_screen_palette_vga16</c> in
        /// <c>gmb_screen_config.c</c>): permette alla console testuale di partire senza caricare
        /// <c>libgmb_gfx</c> (che porta SDL/audio/crypto/compressori).</summary>
        public static byte[][] Vga16Managed { get; } =
        [
            [0,0,0], [0,0,170], [0,170,0], [0,170,170], [170,0,0], [170,0,170], [170,85,0], [170,170,170],
            [85,85,85], [85,85,255], [85,255,85], [85,255,255], [255,85,85], [255,85,255], [255,255,85], [255,255,255]
        ];
        public static byte[][] GetVGA256Palette() { var b = new byte[768]; NativeScreenConfig.gmb_screen_palette_vga256(b); return NativeScreenConfig.Rgb(b, 256); }
        public static byte[][] GetC64Palette() { var b = new byte[48]; NativeScreenConfig.gmb_screen_palette_c64(b); return NativeScreenConfig.Rgb(b, 16); }
        public static byte[][] GetC64CrtPalette() { var b = new byte[48]; NativeScreenConfig.gmb_screen_palette_c64_crt(b); return NativeScreenConfig.Rgb(b, 16); }
        public static byte[][] GetC64ClassicPalette() { var b = new byte[48]; NativeScreenConfig.gmb_screen_palette_c64_classic(b); return NativeScreenConfig.Rgb(b, 16); }
    }


    #endregion

    #region Factory Methods

    /// <summary>Ottiene una configurazione predefinita per una data modalità schermo.
    /// Ogni modalità ha dimensioni, palette e font ottimali preimpostati.</summary>
    /// <summary>Configurazione della console TESTUALE interamente gestita: stessi valori di
    /// <c>GMB_SM_TEXT</c> del runtime C, senza toccare la dylib grafica. Usata da
    /// <c>ReplEngine.InitConsole</c> quando NON si usa SDL: un programma che stampa non deve
    /// caricare `libgmb_gfx` (e con essa SDL2/3, mixer+codec audio, crypto/ssl, zstd/brotli/
    /// lzma/lz4, MoltenVK: ~26 MB).</summary>
    public static ScreenConfig ManagedTextDefault { get; } = new()
    {
        Mode = ScreenMode.Text,
        CharGridWidth = 100,
        CharGridHeight = 40,
        DefaultFgColor = 15,
        DefaultBgColor = 0,
        PaletteName = "Console",
        FontPath = "Fira Code",
        FontSize = 14,
        FontHinting = SdlFontHinting.Normal,
        UseNativeResolutionRendering = true,
        UseLogicalSizeScaling = true,
        WindowMode = ScreenWindowMode.Windowed,
    };

    public static ScreenConfig GetDefault(ScreenMode m)
    {
        var c = NativeScreenConfig.gmb_screen_config_get_default((int)m);
        return new ScreenConfig
        {
            Mode = m,
            LogicalWidth = c.logical_width,
            LogicalHeight = c.logical_height,
            CharGridWidth = c.char_grid_width > 0 ? c.char_grid_width : null,
            CharGridHeight = c.char_grid_height > 0 ? c.char_grid_height : null,
            ResolvedPalette = NativeScreenConfig.Rgb(c.palette, c.palette_size),
            PaletteName = NativeScreenConfig.Str(c.palette_name),
            DefaultFgColor = c.default_fg_color,
            DefaultBgColor = c.default_bg_color,
            WindowWidth = c.window_width > 0 ? c.window_width : null,
            WindowHeight = c.window_height > 0 ? c.window_height : null,
            WindowX = c.window_x != 0 ? c.window_x : null,
            WindowY = c.window_y != 0 ? c.window_y : null,
            FontPath = NativeScreenConfig.Str(c.font_path),
            FontSize = c.font_size > 0 ? c.font_size : null,
            FontHinting = (SdlFontHinting)c.font_hinting,
            UseLogicalSizeScaling = c.use_logical_size_scaling,
            UseNativeResolutionRendering = c.use_native_resolution_rendering,
            WindowMode = (ScreenWindowMode)c.window_mode,
        };
    }


    /// <summary>Genera una configurazione ottimizzata per un font monospaziato nella REPL.
    /// Applica hinting e scaling specifici in base al tipo di font (pixel-perfect vs moderno).</summary>
    /// <param name="fontFamilyName">Nome della famiglia di font (es. "Monaco", "DejaVuSansMono").</param>
    /// <param name="fontSize">Dimensione in punti del font.</param>
    /// <param name="gridWidth">Larghezza griglia di caratteri (default: 100).</param>
    /// <param name="gridHeight">Altezza griglia di caratteri (default: 40).</param>
    public static ScreenConfig GetOptimalConfigForMonospaceRepl(string fontFamilyName, int fontSize,
        int gridWidth = 100, int gridHeight = 40)
    {
        var baseConfig = GetDefault(ScreenMode.Text) with
        {
            FontPath = fontFamilyName,
            FontSize = fontSize,
            CharGridWidth = gridWidth,
            CharGridHeight = gridHeight
        };

        return fontFamilyName.Replace(" ", "").Trim() switch
        {
            "Monaco" or "AndaleMono" or "CourierNewPS" => baseConfig with
            {
                FontHinting = SdlFontHinting.Monospace,
                UseLogicalSizeScaling = false,
                UseNativeResolutionRendering = false
            },
            _ => baseConfig with
            {
                FontHinting = SdlFontHinting.Normal,
                UseLogicalSizeScaling = true,
                UseNativeResolutionRendering = true
            }
        };
    }

    /// <summary>Fornisce la mappa di conversione dai caratteri Unicode per box drawing
    /// ai codici PETSCII del Commodore 64. Usata dal FontAtlas per renderizzare
    /// correttamente i font C64 che usano il set di caratteri PETSCII.</summary>
    public static Dictionary<char, char> GetC64PetsciiRemapping() => new()
    {
        { '╔', (char)218 }, { '═', (char)196 }, { '╗', (char)191 },
        { '║', (char)179 }, { '╚', (char)192 }, { '╝', (char)217 },
        { '╠', (char)195 }, { '╣', (char)180 }, { '╦', (char)194 },
        { '╩', (char)193 }, { '╬', (char)197 },
        { '┌', (char)218 }, { '─', (char)196 }, { '┐', (char)191 },
        { '│', (char)179 }, { '└', (char)192 }, { '┘', (char)217 },
        { '├', (char)195 }, { '┤', (char)180 }, { '┬', (char)194 },
        { '┴', (char)193 }, { '┼', (char)197 },
        { '█', (char)219 }, { '▄', (char)220 }, { '▌', (char)221 },
        { '▐', (char)222 }, { '▀', (char)223 },
    };

    #endregion
}
