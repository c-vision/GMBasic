using CommonLib.Screens;
using CommonLib.Consoles;
// FILE: Interpreter.Fonts.Atlas.cs

using System.Runtime.InteropServices;
using SDL2;

namespace CommonGraphics.Fonts;

/// <summary>
/// Gestisce un "Font Atlas", una singola texture che contiene i glifi pre-renderizzati
/// di un font. Questo approccio migliora drasticamente le prestazioni rispetto alla
/// creazione di una texture per ogni singolo carattere.
/// </summary>
public sealed class FontAtlas : IDisposable
{
    /// <summary>
    /// La texture SDL che contiene l'intero set di glifi.
    /// </summary>
    private IntPtr _atlasTexture = IntPtr.Zero;

    /// <summary>
    /// Mappa che associa ogni carattere alla sua posizione (rettangolo) all'interno della texture dell'atlante.
    /// </summary>
    private readonly Dictionary<char, SDL.SDL_Rect> _glyphRects = new();

    /// <summary>
    /// Flag per IDisposable.
    /// </summary>
    private bool _disposed;

    /// <summary>
    /// Set di caratteri semigrafici che, per loro natura, devono essere stirati verticalmente
    /// per connettersi correttamente con le celle adiacenti.
    /// Questo set include linee, angoli e intersezioni.
    /// </summary>
    private static readonly HashSet<char> VerticalFillChars =
    [
        '│', '┌', '┐', '└', '┘', '├', '┤', '┬', '┴', '┼',
        // Linee doppie
        '║', '╔', '╗', '╚', '╝', '╠', '╣', '╦', '╩', '╬',
        // Block Elements e linee spesse
        '█', '▌', '▐', '┃',
        // Altri connettori
        '╵', '╹', '╶', '╷', '╸', '╺', '╻', '╼', '╽', '╾', '╿'
    ];

    /// <summary>
    /// Inizializza un nuovo FontAtlas caricando il font da un percorso su file.
    /// Utilizza un approccio basato su SDL_Surface per garantire la massima compatibilità e preservare l'anti-aliasing.
    /// </summary>
    /// <param name="renderer">Il renderer SDL su cui creare la texture.</param>
    /// <param name="fontPath">Il percorso del file del font TTF/OTF.</param>
    /// <param name="fontSize">La dimensione in punti a cui renderizzare il font.</param>
    /// <param name="hinting">La modalità di hinting da applicare al font.</param>
    /// <param name="initialColor">Il colore iniziale dei glifi (solitamente bianco).</param>
    /// <param name="firstChar">Il primo carattere da includere nell'atlante.</param>
    /// <param name="lastChar">L'ultimo carattere da includere nell'atlante.</param>
    /// <param name="characterRemapping">Dizionario opzionale per mappare caratteri Unicode a glifi specifici del font (es. PETSCII).</param>
    /// <exception cref="InvalidOperationException">Se il caricamento del font o la creazione delle risorse fallisce.</exception>
    public FontAtlas(IntPtr renderer, string fontPath, int fontSize, SdlFontHinting hinting, SDL.SDL_Color initialColor,
        char firstChar = ' ', char lastChar = '~', Dictionary<char, char>? characterRemapping = null)
    {
        var font = SDL_ttf.TTF_OpenFont(fontPath, fontSize);
        if (font == IntPtr.Zero)
        {
            throw new InvalidOperationException($"TTF_OpenFont fallito per '{fontPath}': {SDL.SDL_GetError()}");
        }

        try
        {
            // Passa al costruttore principale.
            InitializeAtlas(renderer, font, hinting, initialColor, firstChar, lastChar, characterRemapping);
        }
        finally
        {
            SDL_ttf.TTF_CloseFont(font);
        }
    }

    /// <summary>
    /// Inizializza un nuovo FontAtlas utilizzando un puntatore a un font SDL già aperto.
    /// Questo costruttore non chiude il font, la responsabilità rimane al chiamante.
    /// </summary>
    /// <param name="renderer">Il renderer SDL su cui creare la texture.</param>
    /// <param name="fontPtr">Il puntatore al font TTF già aperto.</param>
    /// <param name="hinting">La modalità di hinting da applicare al font.</param>
    /// <param name="initialColor">Il colore iniziale dei glifi.</param>
    /// <param name="firstChar">Il primo carattere da includere nell'atlante.</param>
    /// <param name="lastChar">L'ultimo carattere da includere nell'atlante.</param>
    /// <param name="characterRemapping">Dizionario opzionale per mappare caratteri Unicode a glifi specifici del font (es. PETSCII).</param>
    /// <exception cref="InvalidOperationException">Se la creazione delle risorse fallisce.</exception>
    /// <summary>
    /// Costruttore che accetta un puntatore a un font già aperto.
    /// </summary>
    public FontAtlas(IntPtr renderer, IntPtr fontPtr, SdlFontHinting hinting, SDL.SDL_Color initialColor,
        char firstChar = ' ', char lastChar = '~', Dictionary<char, char>? characterRemapping = null)
    {
        if (fontPtr == IntPtr.Zero)
        {
            throw new ArgumentException("Il puntatore al font non può essere nullo.", nameof(fontPtr));
        }

        InitializeAtlas(renderer, fontPtr, hinting, initialColor, firstChar, lastChar, characterRemapping);
    }

    /// <summary>
    /// Metodo helper privato che contiene la logica comune di creazione dell'atlante.
    /// Applica una logica di "completamento texture" solo per i caratteri di linea
    /// verticali e solo quando è attiva la modalità di remapping (C64).
    /// </summary>
    private void InitializeAtlas(IntPtr renderer, IntPtr font, SdlFontHinting hinting, SDL.SDL_Color initialColor,
        char firstChar, char lastChar, Dictionary<char, char>? characterRemapping)
    {
        var sdlHinting = hinting switch
        {
            SdlFontHinting.Light => SDL_ttf.TTF_HINTING_LIGHT,
            SdlFontHinting.Monospace => SDL_ttf.TTF_HINTING_MONO,
            SdlFontHinting.None => SDL_ttf.TTF_HINTING_NONE,
            _ => SDL_ttf.TTF_HINTING_NORMAL
        };
        SDL_ttf.TTF_SetFontHinting(font, sdlHinting);

        var maxGlyphWidth = 0;
        var maxGlyphHeight = 0;
        var allCharsToProcess = new HashSet<char>();
        for (var c = firstChar; c <= lastChar; c++) allCharsToProcess.Add(c);
        if (characterRemapping != null)
            foreach (var key in characterRemapping.Keys)
                allCharsToProcess.Add(key);

        foreach (var logicalChar in allCharsToProcess)
        {
            var charToMeasure =
                characterRemapping != null && characterRemapping.TryGetValue(logicalChar, out var remapped)
                    ? remapped
                    : logicalChar;
            if (SDL_ttf.TTF_GlyphIsProvided(font, charToMeasure) <= 0) continue;
            SDL_ttf.TTF_SizeUTF8(font, charToMeasure.ToString(), out var w, out var h);
            maxGlyphWidth = Math.Max(maxGlyphWidth, w);
            maxGlyphHeight = Math.Max(maxGlyphHeight, h);
        }

        if (maxGlyphWidth == 0 || maxGlyphHeight == 0) return;

        var charCount = allCharsToProcess.Count;
        var atlasWidth = (int)Math.Ceiling(Math.Sqrt(charCount)) * maxGlyphWidth;
        var atlasHeight = (int)Math.Ceiling(charCount / (atlasWidth / (double)maxGlyphWidth)) * maxGlyphHeight;

        var atlasSurface =
            SDL.SDL_CreateRGBSurfaceWithFormat(0, atlasWidth, atlasHeight, 32, SDL.SDL_PIXELFORMAT_RGBA8888);
        if (atlasSurface == IntPtr.Zero)
            throw new InvalidOperationException("Impossibile creare la superficie per l'atlante.");

        try
        {
            SDL.SDL_SetSurfaceBlendMode(atlasSurface, SDL.SDL_BlendMode.SDL_BLENDMODE_NONE);
            var currentX = 0;
            var currentY = 0;

            foreach (var c in allCharsToProcess)
            {
                var charToRender = characterRemapping != null && characterRemapping.TryGetValue(c, out var remapped)
                    ? remapped
                    : c;
                var glyphSurface = SDL_ttf.TTF_RenderGlyph_Blended(font, charToRender, initialColor);
                if (glyphSurface == IntPtr.Zero) continue;

                var surfaceToBlit = glyphSurface;
                var surfaceToFree = IntPtr.Zero;

                try
                {
                    var boxedGlyphSurface = Marshal.PtrToStructure<SDL.SDL_Surface>(glyphSurface);

                    // --- IL VERO DOPPIO PERCORSO È QUI ---
                    // Applica la logica di completamento texture solo se:
                    // 1. Siamo in modalità C64 (characterRemapping non è null)
                    // 2. Il carattere è uno di quelli che lo richiedono.
                    if (characterRemapping != null && VerticalFillChars.Contains(c))
                    {
                        var fullHeightSurface = SDL.SDL_CreateRGBSurfaceWithFormat(0, boxedGlyphSurface.w,
                            maxGlyphHeight, 32, SDL.SDL_PIXELFORMAT_RGBA8888);
                        if (fullHeightSurface != IntPtr.Zero)
                        {
                            SDL.SDL_SetSurfaceBlendMode(fullHeightSurface, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);
                            var centerRect = new SDL.SDL_Rect { x = 0, y = (maxGlyphHeight - boxedGlyphSurface.h) / 2 };
                            SDL.SDL_BlitSurface(glyphSurface, IntPtr.Zero, fullHeightSurface, ref centerRect);
                            surfaceToBlit = fullHeightSurface;
                            surfaceToFree = fullHeightSurface; // Marca questa superficie per la pulizia
                        }
                    }

                    var destRectOnAtlas = new SDL.SDL_Rect { x = currentX, y = currentY };
                    SDL.SDL_BlitSurface(surfaceToBlit, IntPtr.Zero, atlasSurface, ref destRectOnAtlas);
                    var finalSurfaceBox = Marshal.PtrToStructure<SDL.SDL_Surface>(surfaceToBlit);
                    _glyphRects[c] = new SDL.SDL_Rect
                        { x = currentX, y = currentY, w = finalSurfaceBox.w, h = finalSurfaceBox.h };
                }
                finally
                {
                    SDL.SDL_FreeSurface(glyphSurface);
                    if (surfaceToFree != IntPtr.Zero)
                    {
                        SDL.SDL_FreeSurface(surfaceToFree);
                    }
                }

                currentX += maxGlyphWidth;
                if (currentX + maxGlyphWidth > atlasWidth)
                {
                    currentX = 0;
                    currentY += maxGlyphHeight;
                }
            }

            _atlasTexture = SDL.SDL_CreateTextureFromSurface(renderer, atlasSurface);
            if (_atlasTexture == IntPtr.Zero)
                throw new InvalidOperationException("Impossibile creare la texture finale dell'atlante.");
            SDL.SDL_SetTextureBlendMode(_atlasTexture, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);
        }
        finally
        {
            SDL.SDL_FreeSurface(atlasSurface);
        }
    }

    /// <summary>
    /// Disegna un singolo carattere dall'atlante al renderer di destinazione.
    /// </summary>
    /// <param name="renderer">Il renderer su cui disegnare.</param>
    /// <param name="character">Il carattere da disegnare.</param>
    /// <param name="destRect">Il rettangolo di destinazione sul renderer.</param>
    /// <param name="color">Il colore con cui tingere il carattere.</param>
    public void DrawChar(IntPtr renderer, char character, ref SDL.SDL_Rect destRect, SDL.SDL_Color color)
    {
        if (_atlasTexture == IntPtr.Zero || !_glyphRects.TryGetValue(character, out var srcRect))
        {
            return;
        }

        SDL.SDL_SetTextureColorMod(_atlasTexture, color.r, color.g, color.b);
        SDL.SDL_RenderCopy(renderer, _atlasTexture, ref srcRect, ref destRect);
    }

    /// <summary>
    /// Rilascia la texture dell'atlante.
    /// </summary>
    public void Dispose()
    {
        if (_disposed) return;

        if (_atlasTexture != IntPtr.Zero)
        {
            SDL.SDL_DestroyTexture(_atlasTexture);
            _atlasTexture = IntPtr.Zero;
        }

        _glyphRects.Clear();

        _disposed = true;
    }
}