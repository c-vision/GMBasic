using CommonLib.Screens;
// FILE: Interpreter.Graphics.Internals.cs

// ReSharper disable InconsistentNaming

using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using SDL2;

namespace CommonGraphics.Graphics;

public sealed partial class GraphicsManager
{
    /// <summary>
    /// Implementazione interna del metodo CLS. Cancella il contenuto della pagina attiva.
    /// Questa versione corretta si limita a pulire lo schermo con il colore specificato
    /// o con il colore di sfondo corrente, SENZA alterare lo stato dei colori per i comandi futuri.
    /// </summary>
    private void ClsInternal(int? color)
    {
        CurrentSink?.Clear(color ?? 0);
        if (_captureMode) { Capture($"CLS {color}"); return; }
        if (_programContext is null) return;

        byte[] clearColor;
        if (color is { } v && (v & 0x40000000) != 0)
        {
            // RGB marcato dal sistema (flag 0x40000000, stessa convenzione di
            // SdlGraphicsService.ToColorIndex/GfxResolveColor): 0x00RRGGBB, non un indice
            // palette. Senza questo controllo "CLS RGB(r,g,b)" cancellava con GetColor(v)
            // trattando il valore RGB marcato come un indice fuori range — mai il colore
            // richiesto.
            clearColor = [(byte)((v >> 16) & 0xFF), (byte)((v >> 8) & 0xFF), (byte)(v & 0xFF)];
        }
        else if (color.HasValue)
        {
            // Se CLS ha un argomento (es. CLS 4), usa quel colore per pulire.
            clearColor = GetColor(color.Value);
        }
        else
        {
            // Altrimenti (CLS semplice), usa il colore di sfondo attualmente impostato.
            clearColor = _backgroundColor;
        }

        SetRenderTargetToActivePage();
        SDL.SDL_SetRenderDrawColor(_programContext.RendererHandle, clearColor[0], clearColor[1], clearColor[2], 255);
        SDL.SDL_RenderClear(_programContext.RendererHandle);
        _cursorPosition = (0, 0);
    }

    /// <summary>
    /// Implementazione interna del comando COLOR.
    /// Imposta i colori di primo piano e/o sfondo per le operazioni future.
    /// </summary>
    private void ColorInternal(int? foreground, int? background)
    {
        Capture($"COLOR {foreground},{background}");
        // Se COLOR è chiamato senza parametri, resetta ai default della modalità grafica.
        if (foreground is null && background is null)
        {
            _currentColor = GetColor(_defaultForegroundColor);
            _backgroundColorIndex = _defaultBackgroundColor;
            _backgroundColor = GetColor(_defaultBackgroundColor);
            return;
        }

        // Imposta il colore di primo piano se fornito.
        if (foreground.HasValue)
        {
            _currentColor = GetColor(foreground.Value);
        }

        // Imposta il colore di sfondo se fornito.
        if (background.HasValue)
        {
            _backgroundColorIndex = background.Value;
            _backgroundColor = GetColor(background.Value);
        }
    }

    /// <summary>
    /// Implementazione interna del comando PALETTE.
    /// Modifica il colore all'indice specificato nella palette attiva.
    /// </summary>
    public void PaletteInternal(int index, int r, int g, int b)
    {
        // La modifica ha effetto solo se una modalità grafica è attiva.
        if (!IsScreenOn || _colorPalette is null) return;

        // Valida i parametri per evitare eccezioni.
        if (index < 0 || index >= _colorPalette.Length) return;

        var red = (byte)Math.Clamp(r, 0, 255);
        var green = (byte)Math.Clamp(g, 0, 255);
        var blue = (byte)Math.Clamp(b, 0, 255);

        // Aggiorna il colore nell'array della palette.
        // Tutte le successive chiamate a GetColor(index) useranno questo nuovo valore.
        _colorPalette[index] = [red, green, blue];
    }

    /// <summary>
    /// Sostituisce integralmente la palette attiva della superficie grafica (PSET/CIRCLE/LINE/...).
    /// Chiamata da PALETTE USE/DEFINE/REPLACE/LOAD per tenere la superficie grafica allineata
    /// alla console quando è attivo uno schermo grafico (<see cref="IsScreenOn"/>): i pixel già
    /// disegnati non cambiano retroattivamente (nessun buffer indicizzato per i pixel), ma tutti
    /// i disegni successivi useranno i nuovi colori.
    /// </summary>
    public void SetActivePalette(byte[][] palette, string? name)
    {
        if (!IsScreenOn) return;
        _colorPalette = palette;
        _activePaletteName = name;
    }

    /// <summary>Nome della palette attiva sulla superficie grafica, o null se non nominativa.</summary>
    public string? GetActivePaletteName() => _activePaletteName;

    /// <summary>
    /// Implementazione interna del metodo LINE. Disegna una linea o un rettangolo.
    /// Ora funziona sia per la finestra di programma che per quella della REPL.
    /// </summary>
    private void LineInternal(int x1, int y1, int x2, int y2, int? color, string? style)
    {
        // LINE -(x2,y2): x1/y1 NIL → parte dal punto corrente (semantica QuickBASIC,
        // parità runtime C). Prima il compiler passava 0,0 (linea dall’origine).
        if (x1 < 0) x1 = _cursorPosition.x;
        if (y1 < 0) y1 = _cursorPosition.y;
        if (style == "BF" || style == "B") CurrentSink?.Rect(Math.Min(x1,x2), Math.Min(y1,y2), Math.Abs(x2-x1), Math.Abs(y2-y1), color ?? 0);
        else CurrentSink?.Line(x1, y1, x2, y2, color ?? 0);
        if (_captureMode) { Capture($"LINE {x1},{y1}-{x2},{y2}"); return; }
        if (_programContext is null) return;
        SetRenderTargetToActivePage();
        SDL.SDL_SetRenderDrawBlendMode(_programContext.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_NONE);
        var c = color.HasValue ? GetColor(color.Value) : _currentColor;
        SDL.SDL_SetRenderDrawColor(_programContext.RendererHandle, c[0], c[1], c[2], 255);
        // WINDOW: mappa le coordinate logiche nello schermo prima di disegnare.
        var (mx1, my1) = TransformPoint(x1, y1);
        var (mx2, my2) = TransformPoint(x2, y2);
        var xa = Math.Min(mx1, mx2); var xb = Math.Max(mx1, mx2);
        var ya = Math.Min(my1, my2); var yb = Math.Max(my1, my2);
        switch (style)
        {
            case "BF":
            case "B":
                // SDL_RenderFillRect/DrawRect con la struct SDL_Rect by-ref e persino i
                // DrawLine/DrawPoint multipli sulla TEXTURE TARGET del renderer software
                // disegnavano solo una striscia (il clip interno si corrompe dopo poche
                // operazioni — verificato: BF 101×51 → ~larghezza px totali). Si aggira il
                // renderer: si legge la pagina in un buffer, si scrive il rettangolo (o il
                // perimetro) nel buffer e lo si riscrive nella texture (stessa tecnica del
                // flood fill di PaintInternal).
                var buf = ReadActivePageToBuffer();
                if (buf.Length == 0) break;
                var fCol = RgbToUint(c);
                var _bw = _backingWidth > 0 ? _backingWidth : _screenWidth;
                var _bh = _backingHeight > 0 ? _backingHeight : _screenHeight;
                var _sc = (int)_renderScale;
                // Il buffer è FLIPPATO e a risoluzione NATIVA: ogni pixel logico diventa un
                // blocco scale×scale (per lo stride nativo). Stride nativo per l'indice.
                if (style == "BF")
                {
                    for (var yy = ya; yy <= yb; yy++)
                        for (var dy = 0; dy < _sc; dy++)
                        {
                            var bufY = _bh - 1 - (yy * _sc + dy);
                            if (bufY < 0 || bufY >= _bh) continue;
                            for (var xx = xa; xx <= xb; xx++)
                                for (var dx = 0; dx < _sc; dx++)
                                {
                                    var fx = xx * _sc + dx;
                                    if (fx >= 0 && fx < _bw) buf[bufY * _bw + fx] = fCol;
                                }
                        }
                }
                else
                {
                    for (var xx = xa; xx <= xb; xx++)
                        for (var dx = 0; dx < _sc; dx++)
                        {
                            var fx = xx * _sc + dx;
                            if (fx < 0 || fx >= _bw) continue;
                            for (var dy = 0; dy < _sc; dy++)
                            {
                                if (ya >= 0 && ya < _screenHeight) buf[(_bh - 1 - (ya * _sc + dy)) * _bw + fx] = fCol;
                                if (yb >= 0 && yb < _screenHeight) buf[(_bh - 1 - (yb * _sc + dy)) * _bw + fx] = fCol;
                            }
                        }
                    for (var yy = ya; yy <= yb; yy++)
                        for (var dy = 0; dy < _sc; dy++)
                        {
                            var bufY = _bh - 1 - (yy * _sc + dy);
                            if (bufY < 0 || bufY >= _bh) continue;
                            for (var dx = 0; dx < _sc; dx++)
                            {
                                if (xa >= 0 && xa < _screenWidth) buf[bufY * _bw + xa * _sc + dx] = fCol;
                                if (xb >= 0 && xb < _screenWidth) buf[bufY * _bw + xb * _sc + dx] = fCol;
                            }
                        }
                }
                UpdateActivePageFromBuffer(buf);
                break;
            default:
                SDL.SDL_RenderDrawLine(_programContext.RendererHandle,
                    (int)(mx1 * _renderScale), (int)(my1 * _renderScale),
                    (int)(mx2 * _renderScale), (int)(my2 * _renderScale));
                break;
        }

        _cursorPosition = (x2, y2);
    }

    /// <summary>
    /// Implementazione interna del metodo PSET. Disegna un singolo punto.
    /// </summary>
    private void PsetInternal(double x, double y, int? color)
    {
        CurrentSink?.PSet((int)x, (int)y, color ?? 0);
        if (_captureMode) { Capture($"PSET {x},{y}"); return; }
        if (_programContext is null) return;
        // WINDOW/VIEWPORT: le coordinate logiche vanno mappate nello schermo (e clippate
        // al viewport) — senza, `WINDOW SCREEN (-4,-1.5)-(4,1.5); PSET (x, SIN(x))`
        // disegnava a coordinate raw (fuori schermo per x negative).
        var (px, py) = TransformPoint(x, y);
        if (!TransformAndClipPoint(ref px, ref py)) { _cursorPosition = ((int)x, (int)y); return; }
        SetRenderTargetToActivePage();
        SDL.SDL_SetRenderDrawBlendMode(_programContext.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_NONE);
        var c = color.HasValue ? GetColor(color.Value) : _currentColor;
        SDL.SDL_SetRenderDrawColor(_programContext.RendererHandle, c[0], c[1], c[2], 255);
        // Scala HiDPI: la pagina è nativa (1280×960) — coordinate logiche × _renderScale.
        SDL.SDL_RenderDrawPoint(_programContext.RendererHandle,
            (int)(px * _renderScale), (int)(py * _renderScale));
        _cursorPosition = ((int)x, (int)y);
    }

    /// <summary>
    /// Implementazione interna del metodo CIRCLE. Disegna un cerchio o un arco.
    /// </summary>
    private void CircleInternal(int x, int y, int radius, int? color, double? startAngle, double? endAngle,
        double? aspect)
    {
        CurrentSink?.Circle(x, y, radius, color ?? 0);
        Capture($"CIRCLE {x},{y},{radius} {(color.HasValue ? $"c={color}" : "")}");
        if (_programContext is null) return;
        SetRenderTargetToActivePage();
        // Reimposta il blend a NONE come LineInternal: il PRINT grafico lascia
        // SDL_BLENDMODE_BLEND e i punti del cerchio verrebbero mescolati con lo sfondo
        // (visivamente quasi invisibili — bug visto col sample con PRINT prima di CIRCLE).
        SDL.SDL_SetRenderDrawBlendMode(_programContext.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_NONE);
        var c = color.HasValue ? GetColor(color.Value) : _currentColor;
        SDL.SDL_SetRenderDrawColor(_programContext.RendererHandle, c[0], c[1], c[2], 255);
        var a = aspect ?? 1.0;
        if (Math.Abs(a) < 1e-9) a = 1.0;
        var s = startAngle ?? 0;
        var e = endAngle ?? 2 * Math.PI;
        var step = radius > 0 ? 0.01 / radius : 0.1;
        for (var an = s; an <= e; an += step)
        {
            var px = (int)Math.Round((x + radius * Math.Cos(an)) * _renderScale);
            var py = (int)Math.Round((y + radius * Math.Sin(an) / a) * _renderScale);
            SDL.SDL_RenderDrawPoint(_programContext.RendererHandle, px, py);
        }
    }

    /// <summary>
    /// Implementazione interna del metodo FONT. Apre e imposta un nuovo font.
    /// </summary>
    private void FontInternal(string path, int size)
    {
        // Rimosso il controllo: if (!_ttfInitialized)
        // Ora si presume che SdlManager sia stato inizializzato.
        // Se non lo è, TTF_OpenFont fallirà in modo sicuro.

        if (_font != IntPtr.Zero)
        {
            SDL_ttf.TTF_CloseFont(_font);
            _font = IntPtr.Zero;
        }
        if (_font2x != IntPtr.Zero)
        {
            SDL_ttf.TTF_CloseFont(_font2x);
            _font2x = IntPtr.Zero;
        }

        // I sample usano "font.ttf" (path relativo al CWD), ma il font di progetto sta in
        // Resources/: se il path non esiste, prova a risolverlo rispetto a Resources/ e alla
        // cartella del sample — altrimenti TTF_OpenFont fallisce e PRINT grafico non disegna.
        var resolvedFontPath = path;
        if (!File.Exists(path))
        {
            var candidates = new[]
            {
                Path.Combine(AppContext.BaseDirectory, "Resources", path),
                Path.Combine(AppContext.BaseDirectory, path),
                Path.Combine(Directory.GetCurrentDirectory(), "Resources", path),
                Path.Combine("Resources", path),
            };
            foreach (var cand in candidates)
            {
                if (File.Exists(cand)) { resolvedFontPath = cand; break; }
            }
        }
        // Il font viene aperto a risoluzione NATIVA (size × renderScale) — come SDLConsole.
        // Con le pagine a risoluzione nativa e il Present 1:1, un font da 14px logici deve
        // essere rasterizzato a 28px fisici su Retina: 14px di informazione per 14px visivi
        // (a display) invece di 14px scalati a 28 → testo nitido. _fontSize resta il valore
        // LOGICO (14) per TEXTWIDTH/posizionamento; i glifi reali sono a size*scale.
        var scaledSize = (int)Math.Round(size * (_renderScale > 0 ? _renderScale : 1.0f));
        if (scaledSize < 1) scaledSize = 1;
        var newFont = SDL_ttf.TTF_OpenFont(resolvedFontPath, scaledSize);
        if (newFont == IntPtr.Zero)
        {
            LogDebugGfx(
                $"[GFX_FONT_ERROR] Impossibile caricare il font da '{resolvedFontPath}' (richiesto '{path}' size {size}->{scaledSize}). Errore SDL_ttf: {SDL.SDL_GetError()}");
            //_currentFontPath = null;
            //_currentFontSize = 0;
            _charCellWidth = 0;
            _charCellHeight = 0;
        }
        else
        {
            // MONO per i font monospazio (allinea gli steli alla griglia del font scalato).
            SDL_ttf.TTF_SetFontHinting(newFont, SDL_ttf.TTF_HINTING_MONO);
            LogDebugGfx($"[GFX_FONT] Font '{resolvedFontPath}' caricato con successo (size {size}->{scaledSize}).");
            _font = newFont;
            _fontPath = resolvedFontPath;
            _fontSize = size;
            SDL_ttf.TTF_SizeUTF8(_font, "W", out var w, out var h);
            // Le metriche vanno DIVISE per scale: _charCellWidth/Height sono in unità LOGICHE
            // (usate da LOCATE/TEXTWIDTH/TEXTHEIGHT in coordinate BASIC).
            _charCellWidth = Math.Max(1, (int)Math.Round(w / (_renderScale > 0 ? _renderScale : 1.0f)));
            _charCellHeight = Math.Max(1, (int)Math.Round(SDL_ttf.TTF_FontHeight(_font) / (_renderScale > 0 ? _renderScale : 1.0f)));
        }
    }

    /// <summary>
    /// Implementazione interna del metodo PRINT. Stampa il testo a schermo.
    /// </summary>
    private void PrintInternal(string text)
    {
        Capture($"GFXPRINT \"{text}\"");
        if (_programContext is null || _font == IntPtr.Zero || string.IsNullOrEmpty(text)) return;

        // TAB() esplicito: il BuiltInFunction.TAB ritorna "\t" — in console la console lo
        // espande; in grafica va espanso a spazi fino alla zona di 14 caratteri successiva
        // (larghezza calcolata col font corrente), come il tab-zone della virgola.
        if (text.Contains('\t'))
        {
            var charW = TextWidthInternal(" ");
            if (charW <= 0) charW = 8;
            var sb = new System.Text.StringBuilder(text.Length + 16);
            foreach (var ch in text)
            {
                if (ch == '\t')
                {
                    var col = (_cursorPosition.x + sb.Length * charW) / charW;
                    var next = (col / 14 + 1) * 14;
                    var spaces = next - col;
                    for (var k = 0; k < spaces; k++) sb.Append(' ');
                }
                else sb.Append(ch);
            }
            text = sb.ToString();
        }

        // Assicura che la stringa non contenga newline, che devono essere gestiti dall'Executor.
        if (text.Contains('\n'))
        {
            // Questa logica gestisce i casi in cui PRINT è chiamato con una stringa che contiene \n espliciti.
            var lines = text.Split('\n');
            for (var i = 0; i < lines.Length; i++)
            {
                PrintInternal(lines[i]); // Chiamata ricorsiva per la singola riga
                if (i < lines.Length - 1) // Se non è l'ultima riga, vai a capo
                {
                    // Il newline del PRINT torna a colonna 0 (parità console): i PRINT di
                    // fila partono sempre da zero (x=0) e avanzano di una riga.
                    _cursorPosition.y += TextHeightInternal();
                    _cursorPosition.x = 0;
                }
            }

            return;
        }

        SDL.SDL_SetRenderDrawBlendMode(_programContext.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);
        SetRenderTargetToActivePage();

        var c = new SDL.SDL_Color { r = _currentColor[0], g = _currentColor[1], b = _currentColor[2], a = 255 };

        // Il font è già aperto a risoluzione NATIVA (size × _renderScale, vedi FontInternal):
        // i glifi sono rasterizzati a 28px per un font 14px logico su Retina. Il blit scrive
        // 1:1 nel buffer nativo (stride _backingWidth); il cursore (LOGICO) va mappato a
        // coordinate native (× _renderScale). Il Present copia la pagina 1:1 → testo nitido.
        var s = SDL_ttf.TTF_RenderUTF8_Blended(_font, text, c);
        if (s == IntPtr.Zero) return;

        var sur = System.Runtime.InteropServices.Marshal.PtrToStructure<SDL.SDL_Surface>(s);
        var w = sur.w; var h = sur.h;
        if (w <= 0 || h <= 0) { SDL.SDL_FreeSurface(s); return; }
        var buf = ReadActivePageToBuffer();
        if (buf.Length == 0) return;
        var _bw = _backingWidth > 0 ? _backingWidth : _screenWidth;
        var _bh = _backingHeight > 0 ? _backingHeight : _screenHeight;
        var _sc = _renderScale > 0 ? _renderScale : 1.0f;
        var sw = _bw; var sh = _bh;
        var cx = (int)Math.Round(_cursorPosition.x * _sc);
        var cy = (int)Math.Round(_cursorPosition.y * _sc);
        var pitch = sur.pitch;
        var pixelsPtr = sur.pixels;
        var ttfPad = sur.h - SDL_ttf.TTF_FontHeight(_font);
        var fg = _currentColor;
        for (var yy = 0; yy < h; yy++)
        {
            var dstY = cy + (yy - ttfPad);
            if (dstY < 0 || dstY >= sh) continue;
            var rowPtr = pixelsPtr + yy * pitch;
            for (var xx = 0; xx < w; xx++)
            {
                var dstX = cx + xx;
                if (dstX < 0 || dstX >= sw) continue;
                var px = System.Runtime.InteropServices.Marshal.ReadInt32(rowPtr + xx * 4);
                var alpha = (px >> 24) & 0xFF;
                if (alpha <= 0) continue;
                var bufY = sh - 1 - dstY;
                if (bufY < 0 || bufY >= sh) continue;
                // BLEND MANUALE col pixel di sfondo (output OPACO): l'alpha del TTF va fuso
                // col bg della pagina, non col contenuto dello schermo sotto la finestra.
                var bgPx = buf[bufY * sw + dstX];
                var bgR = (int)((bgPx >> 16) & 0xFF);
                var bgG = (int)((bgPx >> 8) & 0xFF);
                var bgB = (int)(bgPx & 0xFF);
                var a255 = 255 - alpha;
                var outR = (fg[0] * alpha + bgR * a255) / 255;
                var outG = (fg[1] * alpha + bgG * a255) / 255;
                var outB = (fg[2] * alpha + bgB * a255) / 255;
                buf[bufY * sw + dstX] = 0xFF000000u | ((uint)outR << 16) | ((uint)outG << 8) | (uint)outB;
            }
        }
        UpdateActivePageFromBuffer(buf);
        SDL.SDL_FreeSurface(s);

        // Avanza il cursore in unità LOGICHE (larghezza glifo / scale)
        _cursorPosition.x += (int)Math.Round(w / _sc);

    }

    /// <summary>
    /// Implementazione interna del metodo LOCATE. Calcola la posizione del cursore in base a riga e colonna.
    /// </summary>
    /// <summary>
    /// Implementazione interna del metodo LOCATE. Imposta direttamente la posizione
    /// del cursore grafico in coordinate pixel.
    /// </summary>
    private void LocateInternal(int x, int y)
    {
        _cursorPosition = (x, y);
    }

    /// <summary>
    /// Implementazione interna del metodo TEXTWIDTH. Calcola la larghezza in pixel di una stringa di testo.
    /// </summary>
    public int TextWidthInternal(string text)
    {
        if (_font == IntPtr.Zero) return text.Length * 8;
        SDL_ttf.TTF_SizeUTF8(_font, text, out var w, out _);
        // Il font è a risoluzione nativa (size×scale): la larghezza LOGICA è w/scale.
        return Math.Max(1, (int)Math.Round(w / (_renderScale > 0 ? _renderScale : 1.0f)));
    }

    /// <summary>
    /// Implementazione interna del metodo TEXTHEIGHT. Calcola l'altezza in pixel del font corrente.
    /// </summary>
    public int TextHeightInternal()
    {
        if (_font == IntPtr.Zero) return 16;
        return Math.Max(1, (int)Math.Round(SDL_ttf.TTF_FontHeight(_font) / (_renderScale > 0 ? _renderScale : 1.0f)));
    }

    /// <summary>
    /// Implementazione interna del metodo IMAGEGET. Cattura i pixel dello schermo.
    /// </summary>
    private int[]? ImageGetInternal(int x, int y, int width, int height)
    {
        if (!IsScreenOn) return null; // Aggiunta guardia
        CurrentSink?.GetImage(x, y, width, height);
        SetRenderTargetToActivePage();
        var r = new SDL.SDL_Rect { x = x, y = y, w = width, h = height };
        var p = ReadActivePageToBuffer(r);
        if (p.Length == 0) return null;
        var a = new int[2 + p.Length];
        a[0] = width;
        a[1] = height;
        // ReadActivePageToBuffer(rect) restituisce le righe FLIPPATE (la riga 0 del buffer
        // è l'ULTIMA del rect): lo sprite salvato risultava CAPOVOLTO al ripristino.
        // Riordina in senso visivo (riga 0 = in alto).
        for (var yy = 0; yy < height; yy++)
            for (var xx = 0; xx < width; xx++)
                a[2 + yy * width + xx] = FindColorIndex(p[(height - 1 - yy) * width + xx]);
        return a;
    }

    /// <summary>
    /// Implementazione interna del metodo IMAGEPUT. Disegna i pixel di un'immagine a schermo.
    /// </summary>
    /// <summary>
    /// Implementazione interna del metodo IMAGEPUT. Disegna i pixel di un'immagine a schermo.
    /// Ora supporta gli operatori logici PSET, AND, OR, NOT (XOR).
    /// </summary>
    private void ImagePutInternal(int x, int y, int[] img, string mode)
    {
        if (!IsScreenOn || _programContext is null) return;
        if (string.IsNullOrEmpty(mode)) mode = "PSET";  // default Clipper/QuickBASIC

        if (img.Length < 2) return;
        int w = img[0], h = img[1];
        if (w <= 0 || h <= 0 || img.Length < 2 + w * h) return;
        // Dual-write su WS (stateless, come PSet/Line): il browser replaya putimage
        CurrentSink?.PutImage(x, y, w, h, img[2..], mode);

        // Ottimizzazione: Se l'operatore è PSET, possiamo disegnare direttamente senza leggere.
        if (mode.Equals("PSET", StringComparison.OrdinalIgnoreCase))
        {
            // PATH PROFESSIONALE: il PSET pixel-per-pixel con SDL_RenderDrawPoint era il collo
            // di bottiglia (per 100 sprite 32×32 = ~102.400 draw point per frame → non fluido
            // oltre qualche decina di sprite). Ora: con il driver REALE (Metal) costruisce una
            // texture ARGB dal batch e la disegna con RenderCopy (1 draw call per sprite,
            // accelerata — stesso percorso degli sprite GPU); col renderer software (dummy)
            // usa il blit via buffer (RenderCopy texture→target non funziona col software).
            if (Environment.GetEnvironmentVariable("SDL_VIDEODRIVER") != "dummy")
            {
                var renderer = _programContext.RendererHandle;
                SetRenderTargetToActivePage();
                var tex = SDL.SDL_CreateTexture(renderer, SDL.SDL_PIXELFORMAT_ARGB8888,
                    (int)SDL.SDL_TextureAccess.SDL_TEXTUREACCESS_STREAMING, w, h);
                if (tex != IntPtr.Zero)
                {
                    try
                    {
                        // Converte gli indici palette in pixel ARGB; indice negativo = trasparente (alfa 0).
                        var pixels = new uint[w * h];
                        for (var pi = 0; pi < w * h; pi++)
                        {
                            var ci = img[2 + pi];
                            if (ci >= 0)
                            {
                                var c = GetColor(ci);
                                pixels[pi] = 0xFF000000u | ((uint)c[0] << 16) | ((uint)c[1] << 8) | (uint)c[2];
                            }
                        }
                        var pinned = System.Runtime.InteropServices.GCHandle.Alloc(pixels, System.Runtime.InteropServices.GCHandleType.Pinned);
                        try
                        {
                            SDL.SDL_UpdateTexture(tex, IntPtr.Zero, pinned.AddrOfPinnedObject(), w * sizeof(uint));
                        }
                        finally { pinned.Free(); }
                        SDL.SDL_SetTextureBlendMode(tex, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);
                        var psRect = new SDL.SDL_Rect { x = x, y = y, w = w, h = h };
                        SDL.SDL_RenderCopy(renderer, tex, IntPtr.Zero, ref psRect);
                    }
                    finally { SDL.SDL_DestroyTexture(tex); }
                }
                return;
            }

            // Fallback software (dummy): blit via buffer — legge la pagina, scrive i pixel
            // ARGB (flip Y come il testo/sprite 2D), riscrive la texture. Un solo read/write
            // per sprite invece di w×h draw point.
            SetRenderTargetToActivePage();
            var sw = _screenWidth; var sh = _screenHeight;
            uint[] sPage;
            try { sPage = ReadActivePageToBuffer(); }
            catch { return; }
            if (sPage.Length == 0) return;
            var sx2 = x; var sy2 = y;
            for (var j = 0; j < h; j++)
            {
                var dstY = sy2 + j;
                if (dstY < 0 || dstY >= sh) continue;
                var bufY = sh - 1 - dstY; // buffer flippato (come il testo)
                if (bufY < 0 || bufY >= sh) continue;
                for (var i = 0; i < w; i++)
                {
                    var dstX = sx2 + i;
                    if (dstX < 0 || dstX >= sw) continue;
                    var ci = img[2 + j * w + i];
                    if (ci < 0) continue; // trasparente
                    var c = GetColor(ci);
                    sPage[bufY * sw + dstX] = 0xFF000000u | ((uint)c[0] << 16) | ((uint)c[1] << 8) | (uint)c[2];
                }
            }
            UpdateActivePageFromBuffer(sPage);
            return;
        }

        // Per gli altri operatori, dobbiamo leggere i pixel di destinazione.
        // Leggiamo l'intera area di destinazione in una sola volta per efficienza.
        var destRect = new SDL.SDL_Rect { x = x, y = y, w = w, h = h };
        var destPixels = ReadActivePageToBuffer(destRect);
        if (destPixels.Length == 0) return;

        var finalPixels = new uint[destPixels.Length];

        for (var j = 0; j < h; j++)
        for (var i = 0; i < w; i++)
        {
            var sourceIndex = j * w + i;
            var sourceColorIndex = img[2 + sourceIndex];

            // Se il colore sorgente è trasparente, mantieni il pixel di destinazione.
            if (sourceColorIndex < 0)
            {
                finalPixels[sourceIndex] = destPixels[sourceIndex];
                continue;
            }

            var destColorIndex = FindColorIndex(destPixels[sourceIndex]);
            int finalColorIndex;

            // Applica l'operatore logico/bitwise
            switch (mode.ToUpperInvariant())
            {
                case "AND":
                    finalColorIndex = sourceColorIndex & destColorIndex;
                    break;
                case "OR":
                    finalColorIndex = sourceColorIndex | destColorIndex;
                    break;
                case "XOR":
                case "NOT": // XOR è l'equivalente funzionale di NOT in questo contesto
                    finalColorIndex = sourceColorIndex ^ destColorIndex;
                    break;
                default: // Se l'operatore non è riconosciuto, non fare nulla
                    finalColorIndex = destColorIndex;
                    break;
            }

            var finalRgb = GetColor(finalColorIndex);
            finalPixels[sourceIndex] = RgbToUint(finalRgb);
        }

        // Scrivi il buffer di pixel modificato a schermo in un'unica operazione.
        // Questo è molto più veloce che chiamare PSET per ogni pixel.
        if (!_programPageTextures.TryGetValue(_activePage, out var activeTexture)) return;

        var handle = GCHandle.Alloc(finalPixels, GCHandleType.Pinned);
        try
        {
            SDL.SDL_UpdateTexture(activeTexture, ref destRect, handle.AddrOfPinnedObject(), w * sizeof(uint));
        }
        finally
        {
            handle.Free();
        }
    }

    /// <summary>
    /// Implementazione interna del comando IMAGEPUTS per il disegno in batch.
    /// Esegue il ciclo di disegno in C# per la massima performance.
    /// </summary>
    /// <param name="spriteData">I dati dello sprite da disegnare.</param>
    /// <param name="positions">L'array di coordinate X,Y.</param>
    /// <param name="mode">La modalità di disegno (PSET, XOR, etc.).</param>
    private void ImagePutsInternal(int[] spriteData, int[] positions, string mode)
    {
        if (!IsScreenOn || _programContext is null) return;

        if (spriteData.Length < 2) return;
        int w = spriteData[0], h = spriteData[1];
        if (w <= 0 || h <= 0 || spriteData.Length < 2 + w * h) return;

        // Itera sull'array delle posizioni a passi di 2 (X, Y)
        for (var p = 0; p < positions.Length; p += 2)
        {
            // Assicura che ci sia anche la coordinata Y per formare una coppia
            if (p + 1 >= positions.Length) break;

            var x = positions[p];
            var y = positions[p + 1];

            // Riutilizza la logica di ImagePutInternal per disegnare il singolo sprite
            // in ogni posizione specificata. L'eliminazione dell'overhead dell'interprete
            // BASIC per ogni chiamata è ciò che garantisce l'aumento di performance.
            ImagePutInternal(x, y, spriteData, mode);
        }
    }

    /// <summary>
    /// Implementazione interna del metodo DRAW. Esegue i comandi di disegno.
    /// </summary>
    private void DrawInternal(string cs)
    {
        if (!IsScreenOn || _programContext is null) return;
        var targetRenderer = _programContext.RendererHandle;

        SetRenderTargetToActivePage();
        int cx = _cursorPosition.x, cy = _cursorPosition.y;
        double ad = 0;
        var sc = 4.0;
        var dc = _currentColor;
        var rx = DrawCommandRegex();
        foreach (Match m in rx.Matches(cs.ToUpperInvariant()))
        {
            var cmd = m.Groups[1].Value;
            var arg = m.Groups[2].Value;
            var dd = true;
            if (cmd is "B" or "N")
            {
                dd = false;
                if (string.IsNullOrEmpty(arg)) continue;
                cmd = arg[..1];
                arg = arg.Length > 1 ? arg[1..] : string.Empty;
            }

            if (!int.TryParse(arg, out var v)) v = (int)sc;
            switch (cmd)
            {
                case "C":
                    dc = GetColor(v);
                    continue;
                case "S":
                    sc = v;
                    continue;
                case "A":
                    ad = v % 4 * 90;
                    continue;
                case "TA":
                    ad = (ad + v) % 360;
                    continue;
            }

            if (cmd == "M")
            {
                // M x,y — sposta il cursore a coordinate assolute SENZA disegnare (e BM x,y
                // col prefisso B: dd=false). Senza questo comando il cursore restava al punto
                // del comando precedente (bug DRAW: "M400,100 R50 D50 L50 U50" disegnava il
                // quadrato da dove era il cursore, es. dopo un PRESET a (300,250)).
                var mParts = arg.Split(',');
                if (mParts.Length >= 2 &&
                    int.TryParse(mParts[0].Trim(), out var mx) &&
                    int.TryParse(mParts[1].Trim(), out var my))
                {
                    cx = mx; cy = my;
                }
                continue;
            }

            var rad = cmd switch
            {
                "U" => -Math.PI / 2, "D" => Math.PI / 2, "L" => Math.PI, "R" => 0, "E" => -Math.PI / 4,
                "F" => Math.PI / 4, "G" => 3 * Math.PI / 4, "H" => -3 * Math.PI / 4, _ => double.NaN
            };
            if (double.IsNaN(rad)) continue;
            var rot = ad * (Math.PI / 180.0);
            var fin = rad + rot;
            var mov = v * (sc / 4.0);
            var nx = cx + (int)Math.Round(Math.Cos(fin) * mov);
            var ny = cy + (int)Math.Round(Math.Sin(fin) * mov);

            if (dd)
            {
                SDL.SDL_SetRenderDrawColor(targetRenderer, dc[0], dc[1], dc[2], 255);
                SDL.SDL_RenderDrawLine(targetRenderer, cx, cy, nx, ny);
            }

            cx = nx;
            cy = ny;
        }

        _cursorPosition = (cx, cy);
    }

    /// <summary>
    /// Implementazione interna per disegnare una bitmap da un array di byte.
    /// Utilizza SDL_image per decodificare il formato BMP e renderizzarlo come texture.
    /// </summary>
    /// <param name="bmpData">L'array di byte contenente i dati del file BMP.</param>
    /// <param name="x">La coordinata X dove disegnare l'angolo in alto a sinistra della bitmap.</param>
    /// <param name="y">La coordinata Y dove disegnare l'angolo in alto a sinistra della bitmap.</param>
    private void DrawBitmapInternal(byte[] bmpData, int x, int y)
    {
        if (_programContext is null || bmpData.Length == 0)
        {
            return;
        }

        var surface = IntPtr.Zero;
        var texture = IntPtr.Zero;
        GCHandle handle = default;

        try
        {
            handle = GCHandle.Alloc(bmpData, GCHandleType.Pinned);
            var rw = SDL.SDL_RWFromMem(handle.AddrOfPinnedObject(), bmpData.Length);

            if (rw == IntPtr.Zero)
            {
                LogDebugGfx($"[GFX_BITMAP_ERROR] SDL_RWFromMem fallito: {SDL.SDL_GetError()}");
                return;
            }

            // Passiamo 1 (true) per il 'freesrc' in modo che SDL_FreeRW venga chiamato automaticamente su 'rw'.
            surface = SDL_image.IMG_LoadTyped_RW(rw, 1, "BMP");

            if (surface == IntPtr.Zero)
            {
                LogDebugGfx($"[GFX_BITMAP_ERROR] IMG_LoadTyped_RW fallito: {SDL.SDL_GetError()}");
                return;
            }

            texture = SDL.SDL_CreateTextureFromSurface(_programContext.RendererHandle, surface);
            if (texture == IntPtr.Zero)
            {
                LogDebugGfx($"[GFX_BITMAP_ERROR] SDL_CreateTextureFromSurface fallito: {SDL.SDL_GetError()}");
                return;
            }

            SDL.SDL_SetTextureBlendMode(texture, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);
            SDL.SDL_QueryTexture(texture, out _, out _, out var width, out var height);
            var destRect = new SDL.SDL_Rect { x = x, y = y, w = width, h = height };

            SetRenderTargetToActivePage();
            SDL.SDL_RenderCopy(_programContext.RendererHandle, texture, IntPtr.Zero, ref destRect);
        }
        finally
        {
            // Pulisce le risorse SDL create localmente.
            if (texture != IntPtr.Zero) SDL.SDL_DestroyTexture(texture);
            if (surface != IntPtr.Zero) SDL.SDL_FreeSurface(surface);
            // Libera l'handle del GC.
            if (handle.IsAllocated) handle.Free();
        }
    }

    /// <summary>
    /// Implementazione interna del metodo PCOPY. Copia il contenuto di una texture in un'altra,
    /// operando sempre sul set di pagine della finestra del programma.
    /// </summary>
    private void PCopyInternal(int sp, int dp)
    {
        if (!IsScreenOn || _programContext is null) return;
        var targetRenderer = _programContext.RendererHandle;

        if (!_programPageTextures.TryGetValue(sp, out var s) ||
            !_programPageTextures.TryGetValue(dp, out var d)) return;

        SDL.SDL_SetRenderTarget(targetRenderer, d);
        SDL.SDL_RenderCopy(targetRenderer, s, IntPtr.Zero, IntPtr.Zero);

        SetRenderTargetToActivePage();
    }

    /// <summary>
    /// Implementazione interna del metodo SCREENSET. Imposta la pagina visibile e/o attiva.
    /// </summary>
    private void ScreenSetInternal(int? v, int? a)
    {
        if (!IsScreenOn) return;
        if (v.HasValue) _visiblePage = v.Value;
        if (a.HasValue) _activePage = a.Value;
    }

    /// <summary>
    /// POLYGON: riempie un poligono (scanline) col colore dato. Prima il comando era un
    /// no-op (EmitExprAndPop) → non disegnava nulla. Scanline: per ogni riga y si
    /// calcolano le intersezioni con i lati e si riempiono i segmenti.
    /// </summary>
    private void PolygonInternal((float x, float y)[] pts, int color)
    {
        if (_programContext is null || pts.Length < 3) return;
        SetRenderTargetToActivePage();
        SDL.SDL_SetRenderDrawBlendMode(_programContext.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_NONE);
        var c = GetColor(color);
        SDL.SDL_SetRenderDrawColor(_programContext.RendererHandle, c[0], c[1], c[2], 255);
        var sc = _renderScale > 0 ? _renderScale : 1.0f;
        // Scala HiDPI: coordinate logiche → native.
        var n = pts.Length;
        var xs = new float[n]; var ys = new float[n];
        for (var i = 0; i < n; i++) { xs[i] = pts[i].x * sc; ys[i] = pts[i].y * sc; }
        var minY = (int)Math.Floor(ys.Min());
        var maxY = (int)Math.Ceiling(ys.Max());
        for (var y = minY; y <= maxY; y++)
        {
            var fy = y + 0.5f;
            var inter = new System.Collections.Generic.List<float>();
            for (var i = 0; i < n; i++)
            {
                var j = (i + 1) % n;
                var yi = ys[i]; var yj = ys[j];
                if ((yi <= fy && yj > fy) || (yj <= fy && yi > fy))
                {
                    var t = (fy - yi) / (yj - yi);
                    inter.Add(xs[i] + t * (xs[j] - xs[i]));
                }
            }
            inter.Sort();
            for (var k = 0; k + 1 < inter.Count; k += 2)
            {
                var x0 = (int)Math.Ceiling(inter[k]);
                var x1 = (int)Math.Floor(inter[k + 1]);
                if (x1 < x0) continue;
                SDL.SDL_RenderDrawLine(_programContext.RendererHandle, x0, y, x1, y);
            }
        }
    }

    /// <summary>
    /// Implementazione interna del metodo PAINT. Esegue un algoritmo di riempimento a inondazione.
    /// </summary>
    private void PaintInternal(int x, int y, int? fc, int? bc)
    {
        CurrentSink?.Paint(x, y, fc ?? 0, bc ?? 0);
        Capture($"PAINT {x},{y} fc={fc} bc={bc}");
        if (!IsScreenOn) return;
        if (!IsScreenOn || x < 0 || x >= _screenWidth || y < 0 || y >= _screenHeight) return;
        SetRenderTargetToActivePage();
        var p = ReadActivePageToBuffer();
        if (p.Length == 0) return;
        // Risoluzione NATIVA: lo stride è _backingWidth e le coordinate logiche vanno
        // moltiplicate per _renderScale (ogni pixel logico = blocco scale×scale).
        var _bw = _backingWidth > 0 ? _backingWidth : _screenWidth;
        var _bh = _backingHeight > 0 ? _backingHeight : _screenHeight;
        var _sc = (int)_renderScale;
        var s = _bw;
        var sx0 = x * _sc; var sy0 = y * _sc;
        // ReadActivePageToBuffer restituisce le righe FLIPPATE (buf[0] = texture h-1):
        // il punto di partenza va mappato come nel blit (bufY = sh-1-dy), altrimenti il
        // flood partiva dal punto capovolto (riempiva l'area sbagliata e "oltrepassava"
        // il bordo: il pixel di partenza era fuori dal recinto).
        var si = (long)(_bh - 1 - sy0) * s + sx0;
        var tc = p[si];
        var f = RgbToUint(fc.HasValue ? GetColor(fc.Value) : _currentColor);
        if (tc == f) return;
        if (bc.HasValue && tc == RgbToUint(GetColor(bc.Value))) return;
        var st = new Stack<(int, int)>();
        st.Push((sx0, _bh - 1 - sy0));  // riga FLIPPATA (coerente con si e col buffer)
        while (st.Count > 0)
        {
            var (cx, cy) = st.Pop();
            var wx = cx;
            while (wx >= 0 && p[(long)cy * s + wx] == tc) wx--;
            wx++;
            var ex = cx;
            while (ex < _bw && p[(long)cy * s + ex] == tc) ex++;
            ex--;
            for (var i = wx; i <= ex; i++) p[(long)cy * s + i] = f;
            if (cy + 1 < _bh) CheckNextLine(p, cy + 1, wx, ex, s, tc, st);
            if (cy - 1 >= 0) CheckNextLine(p, cy - 1, wx, ex, s, tc, st);
        }

        UpdateActivePageFromBuffer(p);
    }

    /// <summary>
    /// Implementazione interna del salvataggio dello schermo.
    /// Crea una copia della configurazione e della texture della pagina attiva
    /// dalla finestra del programma.
    /// </summary>
    private void SaveScreenInternal(string name)
    {
        if (!IsScreenOn || _programContext is null) return;
        var sourceRenderer = _programContext.RendererHandle;

        if (!_programPageTextures.TryGetValue(_activePage, out var activeTexture)) return;

        if (_savedScreens.TryGetValue(name, out var oldState))
        {
            SDL.SDL_DestroyTexture(oldState.Texture);
        }

        var savedTexture = SDL.SDL_CreateTexture(sourceRenderer, SDL.SDL_PIXELFORMAT_ARGB8888,
            (int)SDL.SDL_TextureAccess.SDL_TEXTUREACCESS_TARGET, _screenWidth, _screenHeight);

        if (savedTexture == IntPtr.Zero) return;

        // RenderCopy texture→texture non funziona col renderer software (stesso bug del
        // testo): copia via buffer — legge la pagina attiva e la riscrive nella texture
        // salvata. Il buffer è in orientamento visivo (come ReadActivePageToBuffer).
        var saveBuf = ReadActivePageToBuffer();
        var saveH = System.Runtime.InteropServices.GCHandle.Alloc(saveBuf, System.Runtime.InteropServices.GCHandleType.Pinned);
        try
        {
            SDL.SDL_UpdateTexture(savedTexture, IntPtr.Zero, saveH.AddrOfPinnedObject(), _screenWidth * sizeof(uint));
        }
        finally
        {
            saveH.Free();
        }

        SetRenderTargetToActivePage();

        var currentConfig = new ScreenConfig
        {
            LogicalWidth = _screenWidth, LogicalHeight = _screenHeight, PaletteData = _colorPalette ?? [],
            DefaultFgColor = _defaultForegroundColor, DefaultBgColor = _defaultBackgroundColor,
            CharGridWidth = _charGridWidth, CharGridHeight = _charGridHeight,
            FontPath = _currentFontPath, FontSize = _currentFontSize
        };

        _savedScreens[name] = new SavedScreenState(currentConfig, savedTexture);
    }

    /// <summary>
    /// Implementazione interna del ripristino dello schermo.
    /// Applica la configurazione salvata e copia i pixel sulla pagina attiva
    /// della finestra del programma.
    /// </summary>
    private void RestoreScreenInternal(string name)
    {
        if (!_savedScreens.TryGetValue(name, out var savedState)) return;

        // ScreenInternal ricrea renderer e pagine: la texture salvata (legata al renderer
        // corrente) diventerebbe invalida → RESTORE restituiva una pagina nera. La si chiama
        // SOLO se il contesto non esiste o la risoluzione è cambiata (es. SCREEN diverso
        // tra SAVE e RESTORE); nel caso comune (stessa modalità) si copia direttamente.
        if (_programContext is null ||
            _screenWidth != savedState.Config.LogicalWidth ||
            _screenHeight != savedState.Config.LogicalHeight)
        {
            ScreenInternal((int)ScreenMode.VGA, 0, 0, savedState.Config);
        }

        // Ora _programContext è garantito che esista e sia corretto.
        if (_programContext is null) return;
        var targetRenderer = _programContext.RendererHandle;

        if (_programPageTextures.TryGetValue(_activePage, out var activeTexture))
        {
            // RenderCopy texture→texture non funziona col renderer software: copia via
            // buffer — legge la texture salvata (raw) e la riscrive nella pagina attiva.
            var restoreBuf = new uint[_screenWidth * _screenHeight];
            var restoreH = System.Runtime.InteropServices.GCHandle.Alloc(restoreBuf, System.Runtime.InteropServices.GCHandleType.Pinned);
            try
            {
                SDL.SDL_SetRenderTarget(targetRenderer, savedState.Texture);
                var rr = new SDL.SDL_Rect { x = 0, y = 0, w = _screenWidth, h = _screenHeight };
                SDL.SDL_RenderReadPixels(targetRenderer, ref rr, SDL.SDL_PIXELFORMAT_ARGB8888,
                    restoreH.AddrOfPinnedObject(), _screenWidth * sizeof(uint));
            }
            finally
            {
                restoreH.Free();
            }
            SetRenderTargetToActivePage();
            UpdateActivePageFromBuffer(restoreBuf);
        }
    }

    /// <summary>
    /// Esposizione sincrona (non-GPM) per la VM: POINT(x, y) come funzione.
    /// Ritorna l'indice colore del pixel o -1 (fuori limiti / schermo spento).
    /// </summary>
    public int PointValue(int x, int y) => PointInternal(x, y);

    /// <summary>
    /// Implementazione interna della funzione POINT. Legge il colore di un singolo pixel
    /// e restituisce il suo indice nella palette corrente.
    /// </summary>
    /// <param name="x">La coordinata x del pixel da leggere.</param>
    /// <param name="y">La coordinata y del pixel da leggere.</param>
    /// <returns>L'indice del colore nella palette, o -1 se le coordinate sono fuori dai limiti.</returns>
    private int PointInternal(int x, int y)
    {
        // Controlla se le coordinate sono valide.
        if (!IsScreenOn || x < 0 || x >= _screenWidth || y < 0 || y >= _screenHeight)
        {
            return -1; // Restituisce -1 per indicare un errore o fuori dai limiti.
        }

        // Legge un rettangolo 1x1 pixel nella posizione specificata. La pagina è a
        // risoluzione NATIVA: le coordinate logiche vanno scalate (× _renderScale),
        // altrimenti POINT leggeva il pixel nativo a (x,y) — non dove PSET (scalato)
        // aveva scritto — e restituiva sempre lo sfondo (bug dopo il fix HiDPI).
        var sc = (int)(_renderScale > 0 ? _renderScale : 1.0f);
        var pixelBuffer = ReadActivePageToBuffer(new SDL.SDL_Rect { x = x * sc, y = y * sc, w = 1, h = 1 });

        // Se la lettura restituisce un buffer vuoto (es. in caso di errore), ritorna -1.
        // Il controllo '== null' è stato rimosso perché ReadActivePageToBuffer non restituisce mai null.
        if (pixelBuffer.Length == 0)
        {
            return -1;
        }

        // Il buffer contiene un solo pixel. Passalo a FindColorIndex per ottenere
        // l'indice del colore corrispondente nella palette.
        return FindColorIndex(pixelBuffer[0]);
    }

    /// <summary>
    /// Implementazione interna del caricamento di una texture.
    /// </summary>
    /// <param name="filePath">Il percorso del file immagine.</param>
    /// <returns>L'handle della texture o 0 se il caricamento fallisce.</returns>
    private int LoadTextureInternal(string filePath)
    {
                if (_programContext is null) return 0;

        var resolvedPath = ResolveAssetPath(filePath);

        // SDL_image.IMG_Load può caricare vari formati (PNG, JPG, BMP, etc.)
        var surface = SDL_image.IMG_Load(resolvedPath);
        if (surface == IntPtr.Zero)
        {
            LogDebugGfx(
                $"[GFX_TEXTURE_ERROR] Impossibile caricare l'immagine '{filePath}'. Errore SDL_image: {SDL.SDL_GetError()}");
            return 0;
        }

        // Crea una texture accelerata dalla surface caricata in RAM.
        var texture = SDL.SDL_CreateTextureFromSurface(_programContext.RendererHandle, surface);

        // Conserva i pixel ARGB della texture PRIMA di liberare la surface: DrawTexture/
        // DrawTextures blitta via buffer (RenderCopyEx non disegna col renderer software).
        uint[]? pixels = null;
        try
        {
            // IMG_Load può restituire una surface in un formato NON-ARGB (es. palette/8-bit
            // con pitch = w invece di w*4): converte esplicitamente ad ARGB8888 prima di
            // leggere i pixel (altrimenti pitch < w*4 e il blocco scarta tutto).
            var argb = SDL.SDL_ConvertSurfaceFormat(surface, SDL.SDL_PIXELFORMAT_ARGB8888, 0);
            if (argb != IntPtr.Zero)
            {
                var sur = System.Runtime.InteropServices.Marshal.PtrToStructure<SDL.SDL_Surface>(argb);
                if (sur.w > 0 && sur.h > 0 && sur.pitch >= sur.w * 4 && sur.pixels != IntPtr.Zero)
                {
                    pixels = new uint[sur.w * sur.h];
                    for (var yy = 0; yy < sur.h; yy++)
                    {
                        var row = sur.pixels + yy * sur.pitch;
                        for (var xx = 0; xx < sur.w; xx++)
                            pixels[yy * sur.w + xx] = (uint)System.Runtime.InteropServices.Marshal.ReadInt32(row + xx * 4);
                    }
                }
                SDL.SDL_FreeSurface(argb);
            }
        }
        catch { pixels = null; }

        // Libera la surface dalla RAM, ora non serve più.
        SDL.SDL_FreeSurface(surface);

        if (texture == IntPtr.Zero)
        {
            LogDebugGfx(
                $"[GFX_TEXTURE_ERROR] Impossibile creare la texture da '{filePath}'. Errore SDL: {SDL.SDL_GetError()}");
            return 0;
        }

        // Abilita il blending sulla texture per supportare la trasparenza (canale alfa).
        // Questo è il passo chiave per far sì che gli sprite .png con sfondi trasparenti
        // vengano disegnati correttamente.
        SDL.SDL_SetTextureBlendMode(texture, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);

        // Genera un nuovo handle, memorizza l'associazione e restituisci l'handle.
        var newId = _nextTextureId++;
        _loadedTextures[newId] = texture;
        if (pixels != null)
            _loadedTexturePixels[newId] = pixels;

        LogDebugGfx($"[GFX_TEXTURE] Texture '{filePath}' caricata con successo. Assegnato ID: {newId}");
        return newId;
    }

    /// <summary>Path relativo dei sample (CWD) vs Resources/bin del runner.</summary>
    private static string ResolveAssetPath(string filePath)
    {
        if (File.Exists(filePath)) return filePath;
        var candidates = new[]
        {
            Path.Combine(AppContext.BaseDirectory, filePath),
            Path.Combine(Directory.GetCurrentDirectory(), "bin", filePath),
            Path.Combine(Directory.GetCurrentDirectory(), "Resources", filePath),
            Path.Combine("bin", filePath),
        };
        foreach (var cand in candidates)
        {
            if (File.Exists(cand)) return cand;
        }
        return filePath;
    }

    /// <summary>LOAD TEXTURE … CELL(w,h): taglia lo spritesheet in texture 1:1 per cella,
    /// row-major. Fallimento → array vuoto (lo statement lascia gli slot a 0).</summary>
    private int[] LoadTextureSheetInternal(string filePath, int cellW, int cellH)
    {
        if (_programContext is null || cellW <= 0 || cellH <= 0) return [];

        var surface = SDL_image.IMG_Load(ResolveAssetPath(filePath));
        if (surface == IntPtr.Zero)
        {
            LogDebugGfx(
                $"[GFX_TEXTURE_ERROR] Impossibile caricare lo spritesheet '{filePath}'. Errore SDL_image: {SDL.SDL_GetError()}");
            return [];
        }

        uint[]? full = null;
        var fullW = 0;
        var fullH = 0;
        try
        {
            var argb = SDL.SDL_ConvertSurfaceFormat(surface, SDL.SDL_PIXELFORMAT_ARGB8888, 0);
            if (argb == IntPtr.Zero) return [];
            try
            {
                var sur = Marshal.PtrToStructure<SDL.SDL_Surface>(argb);
                if (sur.w <= 0 || sur.h <= 0 || sur.pitch < sur.w * 4 || sur.pixels == IntPtr.Zero)
                    return [];
                fullW = sur.w;
                fullH = sur.h;
                full = new uint[fullW * fullH];
                for (var yy = 0; yy < fullH; yy++)
                {
                    var row = sur.pixels + yy * sur.pitch;
                    for (var xx = 0; xx < fullW; xx++)
                        full[yy * fullW + xx] = (uint)Marshal.ReadInt32(row + xx * 4);
                }
            }
            finally
            {
                SDL.SDL_FreeSurface(argb);
            }
        }
        finally
        {
            SDL.SDL_FreeSurface(surface);
        }

        if (full is null) return [];
        var cols = fullW / cellW;
        var rows = fullH / cellH;
        if (cols <= 0 || rows <= 0) return [];

        var ids = new int[cols * rows];
        var n = 0;
        for (var r = 0; r < rows; r++)
        {
            for (var c = 0; c < cols; c++)
            {
                var cell = new uint[cellW * cellH];
                for (var yy = 0; yy < cellH; yy++)
                    Array.Copy(full, (r * cellH + yy) * fullW + c * cellW, cell, yy * cellW, cellW);
                ids[n++] = RegisterArgbTexture(cell, cellW, cellH);
            }
        }
        return ids;
    }

    /// <summary>Registra una texture ARGB (stesso formato di LoadTextureInternal).</summary>
    private int RegisterArgbTexture(uint[] pixels, int width, int height)
    {
        if (_programContext is null || width <= 0 || height <= 0 || pixels.Length < width * height)
            return 0;

        var handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        var surface = IntPtr.Zero;
        try
        {
            surface = SDL.SDL_CreateRGBSurfaceFrom(
                handle.AddrOfPinnedObject(),
                width, height, 32, width * 4,
                0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            if (surface == IntPtr.Zero) return 0;
            var texture = SDL.SDL_CreateTextureFromSurface(_programContext.RendererHandle, surface);
            if (texture == IntPtr.Zero) return 0;
            SDL.SDL_SetTextureBlendMode(texture, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);
            var newId = _nextTextureId++;
            _loadedTextures[newId] = texture;
            _loadedTexturePixels[newId] = pixels;
            return newId;
        }
        finally
        {
            if (surface != IntPtr.Zero) SDL.SDL_FreeSurface(surface);
            if (handle.IsAllocated) handle.Free();
        }
    }

    /// <summary>
    /// Implementazione interna SPRITEDEF: crea una texture SDL da dati pixel RGBA.
    /// </summary>
    private void SpriteDefLoadInternal(int spriteId, int width, int height, byte[] pixelData)
    {
        if (_programContext is null || pixelData.Length == 0) return;

        GCHandle handle = default;
        var surface = IntPtr.Zero;
        var texture = IntPtr.Zero;

        try
        {
            handle = GCHandle.Alloc(pixelData, GCHandleType.Pinned);

            surface = SDL.SDL_CreateRGBSurfaceFrom(
                handle.AddrOfPinnedObject(),
                width, height, 32, width * 4,
                0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
            );

            if (surface == IntPtr.Zero)
            {
                LogDebugGfx($"[SPRITEDEF_ERROR] SDL_CreateRGBSurfaceFrom fallito: {SDL.SDL_GetError()}");
                return;
            }

            texture = SDL.SDL_CreateTextureFromSurface(_programContext.RendererHandle, surface);
            if (texture == IntPtr.Zero)
            {
                LogDebugGfx($"[SPRITEDEF_ERROR] SDL_CreateTextureFromSurface fallito: {SDL.SDL_GetError()}");
                return;
            }

            SDL.SDL_SetTextureBlendMode(texture, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);
            _loadedTextures[spriteId] = texture;
            if (spriteId >= _nextTextureId)
                _nextTextureId = spriteId + 1;
        }
        finally
        {
            if (surface != IntPtr.Zero) SDL.SDL_FreeSurface(surface);
            if (handle.IsAllocated) handle.Free();
        }
    }

    /// <summary>
    /// Implementazione interna del disegno di una texture.
    /// </summary>
    private void DrawTextureInternal(int textureId, int x, int y, double rotation, double scaleX, double? scaleY, int color)
    {
        if (_programContext is null) return;
                if (!_loadedTextures.TryGetValue(textureId, out var texturePtr)) return;
        if (!_loadedTexturePixels.TryGetValue(textureId, out var pixels)) return;

        SDL.SDL_QueryTexture(texturePtr, out _, out _, out var width, out var height);
                if (width <= 0 || height <= 0 || pixels.Length < width * height) return;

        // RenderCopyEx (texture → pagina) NON disegna col renderer software + target texture
        // (stesso problema di RenderCopy/rect by-ref): si blittano i pixel ARGB salvati al
        // LOAD nel buffer della pagina. La rotazione non è supportata dal blit (si disegna
        // non ruotato — i sample sprite usano rotazione 0).
        var scaleYv = scaleY ?? scaleX;
        var dw = (int)(width * scaleX);
        var dh = (int)(height * scaleYv);
        if (dw <= 0 || dh <= 0) return;
        var sx = x - dw / 2;
        var sy = y - dh / 2;

        // Come in GMO (24931646): col driver REALE (Metal) RenderCopyEx è accelerato (GPU)
        // e supporta la rotazione (i sample ruotano gli sprite); il blit via buffer è solo
        // per il driver dummy (renderer software + target texture). Stesso codice GMO.
        // Tint per-sprite (GRAFX-01): 0 = invariato; altrimenti SDL_SetTextureColorMod
        // (RGB) prima della copia GPU — la mod è per-texture e va RESETTATA dopo.
        int tr = 255, tg = 255, tb = 255;
        if (color != 0) { tr = (color >> 16) & 0xFF; tg = (color >> 8) & 0xFF; tb = color & 0xFF; }
        if (Environment.GetEnvironmentVariable("SDL_VIDEODRIVER") != "dummy")
        {
            SetRenderTargetToActivePage();
            SDL.SDL_SetRenderDrawBlendMode(_programContext.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_NONE);
            if (color != 0) SDL.SDL_SetTextureColorMod(texturePtr, (byte)tr, (byte)tg, (byte)tb);
            // Scala HiDPI: pagina nativa — posizione e dimensione logiche × _renderScale.
            var destRect = new SDL.SDL_Rect
            {
                x = (int)(sx * _renderScale), y = (int)(sy * _renderScale),
                w = (int)(dw * _renderScale), h = (int)(dh * _renderScale)
            };
            SDL.SDL_RenderCopyEx(_programContext.RendererHandle, texturePtr, IntPtr.Zero, ref destRect,
                rotation, IntPtr.Zero, SDL.SDL_RendererFlip.SDL_FLIP_NONE);
            if (color != 0) SDL.SDL_SetTextureColorMod(texturePtr, 255, 255, 255);
            return;
        }

        uint[] page;
        try { page = ReadActivePageToBuffer(); }
        catch { return; }
        if (page.Length == 0) return;
        var sw = _backingWidth > 0 ? _backingWidth : _screenWidth;
        var sh = _backingHeight > 0 ? _backingHeight : _screenHeight;
        // Coordinate logiche → native (× _renderScale) e stride nativo.
        var sxN = (int)(sx * _renderScale);
        var syN = (int)(sy * _renderScale);
        var scX = scaleX * _renderScale;
        var scYv = scaleYv * _renderScale;

        for (var yy = 0; yy < height; yy++)
        {
            var dstY0 = syN + (int)(yy * scYv);
            var dstY1 = syN + (int)((yy + 1) * scYv);
            if (dstY1 <= dstY0) dstY1 = dstY0 + 1;
            for (var xx = 0; xx < width; xx++)
            {
                var px = pixels[yy * width + xx];
                var alpha = (px >> 24) & 0xFF;
                if (alpha == 0) continue;
                var dstX0 = sxN + (int)(xx * scX);
                var dstX1 = sxN + (int)((xx + 1) * scX);
                if (dstX1 <= dstX0) dstX1 = dstX0 + 1;
                for (var dy = dstY0; dy < dstY1; dy++)
                {
                    if (dy < 0 || dy >= sh) continue;
                    var bufY = sh - 1 - dy; // buffer flippato (come il testo)
                    if (bufY < 0 || bufY >= sh) continue;
                    for (var dx = dstX0; dx < dstX1; dx++)
                        if (dx >= 0 && dx < sw)
                        {
                            var dstPx = px;
                            if (color != 0)
                            {
                                // Tint RGB per-sprite (GRAFX-01): moltiplica i canali (A invariato).
                                var a = (uint)((px >> 24) & 0xFF);
                                var r = (uint)(((px >> 16) & 0xFF) * tr / 255);
                                var g = (uint)(((px >> 8) & 0xFF) * tg / 255);
                                var b = (uint)((px & 0xFF) * tb / 255);
                                dstPx = (a << 24) | (r << 16) | (g << 8) | b;
                            }
                            page[bufY * sw + dx] = dstPx;
                        }
                }
            }
        }
        UpdateActivePageFromBuffer(page);
    }

    /// <summary>
    /// Implementazione interna del disegno in batch di texture.
    /// Esegue un ciclo ottimizzato per disegnare più istanze della stessa texture.
    /// </summary>
    private void DrawTexturesInternalNoGeometry(int textureId, Array xPos, Array yPos, Array rotations, Array scales)
    {
                if (_programContext is null || !_loadedTextures.TryGetValue(textureId, out var texturePtr))
        {
            return;
        }

        // Ottieni le dimensioni della texture una sola volta prima del ciclo
        SDL.SDL_QueryTexture(texturePtr, out _, out _, out var width, out var height);

        // Imposta il target di rendering una sola volta
        SetRenderTargetToActivePage();

        var count = xPos.Length;

        // Ciclo principale. Questo ciclo viene eseguito interamente in C# nativo,
        // eliminando migliaia di chiamate dall'interprete BASIC e garantendo performance elevate.
        for (var i = 0; i < count; i++)
        {
            // Converte i valori dagli array (che sono di tipo 'object') ai tipi numerici necessari.
            // L'uso di 'dynamic' qui semplifica il codice, ma una conversione esplicita
            // (es. Convert.ToDouble) sarebbe leggermente più performante.
            dynamic x = xPos.GetValue(i) ?? 0.0;
            dynamic y = yPos.GetValue(i) ?? 0.0;
            dynamic rot = rotations.GetValue(i) ?? 0.0;
            dynamic scale = scales.GetValue(i) ?? 0.0;

            // RenderCopyEx non disegna col renderer software: blit via buffer
            // (DrawTextureInternal) per ogni istanza.
            DrawTextureInternal(textureId, (int)x, (int)y, (double)rot, (double)scale, (double)scale, 0);
        }
    }

    /// <summary>
    /// Implementazione interna del disegno in batch di texture, utilizzando
    /// il binding P/Invoke a SDL_RenderGeometry definito in GraphicsManagerExtensions.
    /// </summary>
    /// <param name="textureId">L'handle della texture da disegnare.</param>
    /// <param name="xPos">L'array delle coordinate X.</param>
    /// <param name="yPos">L'array delle coordinate Y.</param>
    /// <param name="rotations">L'array delle rotazioni in gradi.</param>
    /// <param name="scales">L'array dei fattori di scala.</param>
    private void DrawTexturesInternal(int textureId, Array xPos, Array yPos, Array rotations, Array scales)
    {
        if (_programContext is null || !_loadedTextures.TryGetValue(textureId, out var texturePtr)) return;

        var count = xPos.Length;
        if (count == 0) return;

        // NOTA: Non serve più fare SDL_QueryTexture qui, perché le coordinate UV sono normalizzate (0-1).

        // Dobbiamo comunque ottenere le dimensioni per calcolare la scala corretta in pixel
        SDL.SDL_QueryTexture(texturePtr, out _, out _, out var texW, out var texH);
        var halfTexW = texW / 2.0f;
        var halfTexH = texH / 2.0f;

        var vertices = new GraphicsManagerExtensions.SDL_Vertex[count * 4];
        var defaultValue = 0.0f;
        // Scala HiDPI: la pagina è nativa — coordinate e dimensioni logiche × _renderScale.
        var rsc = _renderScale > 0 ? _renderScale : 1.0f;

        for (var i = 0; i < count; i++)
        {
            var x = Convert.ToSingle(xPos.GetValue(i) ?? defaultValue) * rsc;
            var y = Convert.ToSingle(yPos.GetValue(i) ?? defaultValue) * rsc;
            var scale = Convert.ToSingle(scales.GetValue(i) ?? defaultValue) * rsc;
            var angleRad = Convert.ToSingle(rotations.GetValue(i) ?? defaultValue) * ((float)Math.PI / 180.0f);

            var cos = (float)Math.Cos(angleRad);
            var sin = (float)Math.Sin(angleRad);

            // Calcola la dimensione scalata in pixel per la posizione dei vertici
            var w = halfTexW * scale;
            var h = halfTexH * scale;
            var corners = new[]
                { new { X = -w, Y = -h }, new { X = w, Y = -h }, new { X = w, Y = h }, new { X = -w, Y = h } };

            var vertexIndex = i * 4;

            // Le coordinate della texture (tex_coord) devono essere normalizzate tra 0.0 e 1.0.
            vertices[vertexIndex + 0] = CreateVertex(x + corners[0].X * cos - corners[0].Y * sin,
                y + corners[0].X * sin + corners[0].Y * cos, 0.0f, 0.0f);
            vertices[vertexIndex + 1] = CreateVertex(x + corners[1].X * cos - corners[1].Y * sin,
                y + corners[1].X * sin + corners[1].Y * cos, 1.0f, 0.0f);
            vertices[vertexIndex + 2] = CreateVertex(x + corners[2].X * cos - corners[2].Y * sin,
                y + corners[2].X * sin + corners[2].Y * cos, 1.0f, 1.0f);
            vertices[vertexIndex + 3] = CreateVertex(x + corners[3].X * cos - corners[3].Y * sin,
                y + corners[3].X * sin + corners[3].Y * cos, 0.0f, 1.0f);
        }

        var indices = new int[count * 6];
        for (var i = 0; i < count; i++)
        {
            var baseVertex = i * 4;
            var baseIndex = i * 6;
            indices[baseIndex + 0] = baseVertex + 0;
            indices[baseIndex + 1] = baseVertex + 1;
            indices[baseIndex + 2] = baseVertex + 2;
            indices[baseIndex + 3] = baseVertex + 0;
            indices[baseIndex + 4] = baseVertex + 2;
            indices[baseIndex + 5] = baseVertex + 3;
        }

        SetRenderTargetToActivePage();

        try
        {
            var result = GraphicsManagerExtensions.SDL_RenderGeometry(
                _programContext.RendererHandle,
                texturePtr,
                vertices, vertices.Length,
                indices, indices.Length
            );

            // Se il risultato è negativo, c'è stato un errore. Lo stampiamo.
            if (result < 0)
            {
                //System.Diagnostics.Debug.WriteLine($"[SDL_RenderGeometry ERROR] {SDL.SDL_GetError()}");
            }
        }
        catch (EntryPointNotFoundException)
        {
            // Catturiamo specificamente l'eccezione che si verifica se la funzione non esiste nella libreria.
            //System.Diagnostics.Debug.WriteLine(
            //    $"[P/Invoke ERROR] EntryPointNotFoundException: La funzione 'SDL_RenderGeometry' non è stata trovata nella libreria SDL2. Controlla che la versione della libreria .dylib/.dll sia 2.0.18 o successiva. Messaggio: {ex.Message}");
            // Come fallback, potremmo chiamare il metodo non accelerato per non bloccare l'utente.
            DrawTexturesInternalNoGeometry(textureId, xPos, yPos, rotations, scales);
        }
        catch (Exception)
        {
            // Catturiamo qualsiasi altra eccezione dal P/Invoke.
            //Debug.WriteLine($"[P/Invoke UNHANDLED ERROR] {ex.GetType().Name}: {ex.Message}");
        }
    }

    /// <summary>
    /// Implementazione interna per aggiornare la matrice di MONDO (World) di rendering.
    /// Recupera la matrice MathNet dall'Executor tramite la callback e la converte,
    /// salvandola nella cache di rendering del GraphicsManager.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice LINALG da cui aggiornare.</param>
    private void UpdateWorldMatrixInternal(long matrixId)
    {
        if (_getLinearMatrixProvider == null)
        {
            LogDebugGfx(
                "[GFX_MATRIX_ERROR] _getLinearMatrixProvider non è impostato. Impossibile aggiornare la matrice World.");
            return;
        }

        var mathNetMatrix = _getLinearMatrixProvider(matrixId);

        _worldMatrix = ToSystemNumericsMatrix(mathNetMatrix);
    }

    /// <summary>
    /// Implementazione interna per aggiornare la matrice di VISTA (View) di rendering.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice LINALG da cui aggiornare.</param>
    private void UpdateViewMatrixInternal(long matrixId)
    {
        if (_getLinearMatrixProvider == null)
        {
            LogDebugGfx(
                "[GFX_MATRIX_ERROR] _getLinearMatrixProvider non è impostato. Impossibile aggiornare la matrice View.");
            return;
        }

        var mathNetMatrix = _getLinearMatrixProvider(matrixId);

        _viewMatrix = ToSystemNumericsMatrix(mathNetMatrix);
    }

    /// <summary>
    /// Implementazione interna per aggiornare la matrice di PROIEZIONE (Projection) di rendering.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice LINALG da cui aggiornare.</param>
    private void UpdateProjectionMatrixInternal(long matrixId)
    {
        if (_getLinearMatrixProvider == null)
        {
            LogDebugGfx(
                "[GFX_MATRIX_ERROR] _getLinearMatrixProvider non è impostato. Impossibile aggiornare la matrice Projection.");
            return;
        }

        var mathNetMatrix = _getLinearMatrixProvider(matrixId);

        _projectionMatrix = ToSystemNumericsMatrix(mathNetMatrix);
    }

    /// <summary>
    /// Funzione helper per creare una struttura SDL_Vertex definita in GraphicsManagerExtensions.
    /// </summary>
    /// <param name="x">La coordinata X della posizione del vertice.</param>
    /// <param name="y">La coordinata Y della posizione del vertice.</param>
    /// <param name="u">La coordinata U (orizzontale) della texture (normalizzata tra 0.0 e 1.0).</param>
    /// <param name="v">La coordinata V (verticale) della texture (normalizzata tra 0.0 e 1.0).</param>
    /// <returns>Una nuova istanza di <see cref="GraphicsManagerExtensions.SDL_Vertex"/>.</returns>
    private static GraphicsManagerExtensions.SDL_Vertex CreateVertex(float x, float y, float u, float v)
    {
        return new GraphicsManagerExtensions.SDL_Vertex
        {
            position = new GraphicsManagerExtensions.SDL_FPoint { x = x, y = y },
            color = new SDL.SDL_Color { r = 255, g = 255, b = 255, a = 255 },
            // Le coordinate UV devono essere normalizzate (da 0.0 a 1.0)
            tex_coord = new GraphicsManagerExtensions.SDL_FPoint { x = u, y = v }
        };
    }
}
