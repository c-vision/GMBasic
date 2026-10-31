using CommonLib.Screens;
using CommonLib.Consoles;
// FILE: Interpreter.Consoles.SdlConsole.Internals.cs

using System.Drawing;
using CommonGraphics.Fonts;
using CommonGraphics.Terminal;
using SDL2;
using System.Runtime.InteropServices;

namespace CommonGraphics.Consoles;

/// <summary>
/// Contiene la logica interna di rendering e gestione delle risorse grafiche
/// per la classe parziale SdlConsole.
/// </summary>
public sealed partial class SdlConsole
{
    /// <summary>
    /// Aggiorna e disegna il contenuto della finestra della console sullo schermo,
    /// ottimizzando il ridisegno alle sole celle modificate.
    /// </summary>
    private void RenderBuffer()
    {
        // Take a consistent snapshot of the shared state under lock.
        bool forceRedraw, contentDirty;
        int cursorRow, cursorCol, prevCursorRow, prevCursorCol;
        Point[] dirtySnapshot;
        lock (_renderLock)
        {
            forceRedraw = _forceRedraw;
            contentDirty = _isContentDirty;
            cursorRow = _cursorRow;
            cursorCol = _cursorCol;
            prevCursorRow = _previousCursorRow;
            prevCursorCol = _previousCursorCol;
        }

        if (_pendingResize && (DateTime.Now - _lastResizeTime).TotalMilliseconds > 200)
        {
            if (_context != null)
            {
                _pendingResize = false;
                // SDL_GetWindowSize restituisce le dimensioni in PUNTI logici, non in pixel
                // reali — su schermi ad alta densità (Retina, scala 2x) questo dimezzava
                // silenziosamente la griglia di caratteri risultante, perché _charWidth/
                // _charHeight sono misurati in pixel reali (vedi UpdateFontAndRecalibrate,
                // che scala il font tramite renderScale = outputPixels/windowPoints). Usare
                // SDL_GetRendererOutputSize (pixel reali) per restare coerenti con l'unità di
                // misura di _charWidth/_charHeight.
                SDL.SDL_GetRendererOutputSize(_context.RendererHandle, out var newPixelWidth, out var newPixelHeight);
                if (_charWidth > 0 && _charHeight > 0)
                {
                    var newGridWidth = (int)Math.Round((float)newPixelWidth / _charWidth);
                    var newGridHeight = (int)Math.Round((float)newPixelHeight / _charHeight);
                    Configuration = Configuration with { CharGridWidth = newGridWidth, CharGridHeight = newGridHeight };
                    _width = newGridWidth;
                    _height = newGridHeight;
                    _viewWidth = _width;
                    _viewHeight = _height;
                }
                forceRedraw = true;
                contentDirty = true;
            }
        }

        // Update previous cursor tracking (under lock to avoid race)
        lock (_renderLock)
        {
            if (_cursorRow != prevCursorRow || _cursorCol != prevCursorCol)
            {
                MarkCellDirty(prevCursorRow, prevCursorCol);
                _previousCursorRow = cursorRow;
                _previousCursorCol = cursorCol;
            }
        }

        var isCursorVisibleThisFrame = CursorVisible;
        if (isCursorVisibleThisFrame)
        {
            var style = ((ITerminalBuffer)this).CursorStyle;
            if (style is CursorStyle.BlinkingBar or CursorStyle.BlinkingBlock or CursorStyle.BlinkingUnderline)
            {
                if (_cursorBlinkTimer.ElapsedMilliseconds % 1000 > 500)
                {
                    isCursorVisibleThisFrame = false;
                }
            }
        }

        var cursorVisibilityChanged = isCursorVisibleThisFrame != _wasCursorVisibleLastFrame;
        if (cursorVisibilityChanged)
        {
            lock (_renderLock)
            {
                MarkCellDirty(cursorRow, cursorCol);
                _isContentDirty = true;
                contentDirty = true;
            }
        }

        // Lo snapshot va preso QUI, dopo tutte le MarkCellDirty sopra (tracking cursore
        // precedente + blink): prendendolo prima (come faceva la versione originale) la
        // cella appena marcata sporca dal cambio di stato del blink non entrava nello
        // snapshot di QUESTO frame e veniva scartata da _dirtyCells.Clear() più sotto senza
        // mai essere ridisegnata — il cursore lampeggiante restava sempre "acceso" a video
        // (bug: il calcolo del blink era corretto, ma la cella non veniva mai ridisegnata).
        lock (_renderLock)
        {
            dirtySnapshot = [.. _dirtyCells];
        }

        if (!contentDirty && !_isShown) return;
        if (_context is null) return;
        
        var renderer = _context.RendererHandle;
        SDL.SDL_SetRenderTarget(renderer, _screenTexture);
        
        if (forceRedraw)
        {
            var defaultBgRgb = _palette[Configuration.DefaultBgColor];
            SDL.SDL_SetRenderDrawColor(renderer, defaultBgRgb[0], defaultBgRgb[1], defaultBgRgb[2], 255);
            SDL.SDL_RenderClear(renderer);

            for (var r = 0; r < _height; r++)
            for (var c = 0; c < _width; c++)
                DrawCell(r, c, isCursorVisibleThisFrame);
        }
        else if (contentDirty)
        {
            foreach (var point in dirtySnapshot)
            {
                DrawCell(point.Y, point.X, isCursorVisibleThisFrame);
            }
        }

        lock (_renderLock)
        {
            _dirtyCells.Clear();
        }

        SDL.SDL_SetRenderTarget(renderer, IntPtr.Zero);
        SDL.SDL_RenderCopy(renderer, _screenTexture, IntPtr.Zero, IntPtr.Zero);
        SDL.SDL_RenderPresent(renderer);

        lock (_renderLock)
        {
            _isContentDirty = false;
            _forceRedraw = false;
        }
        _wasCursorVisibleLastFrame = isCursorVisibleThisFrame;
        
        // Sincronizza il buffer TUI condiviso
        SyncToTuiScreen();
    }
    
    /// <summary>
    /// Disegna una singola cella del buffer utilizzando il FontAtlas ad alte prestazioni.
    /// Gestisce l'inversione dei colori per il cursore e gli attributi della cella.
    /// </summary>
    /// <param name="bufferRow">La riga della cella nel buffer.</param>
    /// <param name="bufferCol">La colonna della cella nel buffer.</param>
    /// <param name="isCursorVisibleThisFrame">Indica se il cursore deve essere visibile in questo frame.</param>
    private void DrawCell(int bufferRow, int bufferCol, bool isCursorVisibleThisFrame)
    {
        if (_context is null || _fontAtlas is null) return;
        var renderer = _context.RendererHandle;

        if (_charWidth <= 0 || _charHeight <= 0) return;

        var viewRow = bufferRow - _viewTop;
        var viewCol = bufferCol - _viewLeft;

        if (viewRow < 0 || viewRow >= _viewHeight || viewCol < 0 || viewCol >= _viewWidth) return;

        var cell = _screenBuffer[bufferRow, bufferCol];
        var srcRect = new ScreenRect { x = viewCol * _charWidth, y = viewRow * _charHeight, w = _charWidth, h = _charHeight };
        var sdlRect = new SDL.SDL_Rect { x = srcRect.x, y = srcRect.y, w = srcRect.w, h = srcRect.h };

        var finalFg = cell.ForegroundColor;
        var finalBg = cell.BackgroundColor;

        var isCursorCell = isCursorVisibleThisFrame && bufferRow == _cursorRow && bufferCol == _cursorCol;
        if (isCursorCell)
        {
            finalFg = cell.BackgroundColor;
            finalBg = cell.ForegroundColor;
        }
        else if (cell.Attributes.HasFlag(ScreenCellAttributes.Inverse))
        {
            finalFg = cell.BackgroundColor;
            finalBg = cell.ForegroundColor;
        }

        var bgRgb = _palette[finalBg];
        SDL.SDL_SetRenderDrawColor(renderer, bgRgb[0], bgRgb[1], bgRgb[2], 255);
        SDL.SDL_RenderFillRect(renderer, ref sdlRect);

        if (cell.Character > ' ' && cell.Character != '\0')
        {
            var fgRgb = _palette[finalFg];
            var color = new SDL.SDL_Color { r = fgRgb[0], g = fgRgb[1], b = fgRgb[2], a = 255 };
            _fontAtlas.DrawChar(renderer, cell.Character, ref sdlRect, color);
        }
    }
        
    /// <summary>
    /// Assicura che la vista (viewport) sulla console sia posizionata in modo tale
    /// che il cursore sia sempre visibile. Sposta la vista se necessario.
    /// </summary>
    private void EnsureCursorIsVisible()
    {
        int cursorRow, cursorCol;
        lock (_renderLock)
        {
            cursorRow = _cursorRow;
            cursorCol = _cursorCol;
        }
        var viewChanged = false;
        if (cursorCol >= _viewLeft + _viewWidth) { _viewLeft = cursorCol - _viewWidth + 1; viewChanged = true; }
        if (cursorCol < _viewLeft) { _viewLeft = cursorCol; viewChanged = true; }
        if (cursorRow >= _viewTop + _viewHeight) { _viewTop = cursorRow - _viewHeight + 1; viewChanged = true; }
        if (cursorRow < _viewTop) { _viewTop = cursorRow; viewChanged = true; }

        if (viewChanged)
        {
            _isContentDirty = true;
            _forceRedraw = true;
        }
    }

    /// <summary>
    /// Ripristina l'intero stato della console alla sua configurazione iniziale.
    /// </summary>
    public void ResetToInitialState()
    {
        Configuration = _initialConfig;
        UpdateFontAndRecalibrate();

        var initialWidth = _initialConfig.CharGridWidth ?? 80;
        var initialHeight = _initialConfig.CharGridHeight ?? 50;

        BufferWidth = initialWidth;
        BufferHeight = initialHeight;
        SetWindowSize(initialWidth, initialHeight);

        if (_isShown && _context != null)
        {
            SDL.SDL_SetWindowPosition(_context.WindowHandle, SDL.SDL_WINDOWPOS_CENTERED, SDL.SDL_WINDOWPOS_CENTERED);
        }
        Clear();
    }
    
    /// <summary>
    /// Metodo helper centrale che ricalibra tutte le risorse grafiche
    /// basandosi sulla configurazione corrente della console.
    /// </summary>
    private void UpdateFontAndRecalibrate()
    {
        // Se il contesto non è stato creato (errore grave), esci per evitare crash.
        if (_context == null) return;

        _activeFontPath = FontManager.SelectMonospaceFont(Configuration.FontPath);
        
        _fontAtlas?.Dispose();
        _fontAtlas = null;
        if (_font != IntPtr.Zero) SDL_ttf.TTF_CloseFont(_font);
        if (_screenTexture != IntPtr.Zero) SDL.SDL_DestroyTexture(_screenTexture);

        SDL.SDL_GetRendererOutputSize(_context.RendererHandle, out var outputWidth, out var _);
        SDL.SDL_GetWindowSize(_context.WindowHandle, out var tempWindowWidth, out var _);
        var renderScale = outputWidth > 0 && tempWindowWidth > 0 ? (float)outputWidth / tempWindowWidth : 1.0f;
        var scaledFontSize = (int)Math.Round((Configuration.FontSize ?? 16) * renderScale);

        _font = SDL_ttf.TTF_OpenFont(_activeFontPath, scaledFontSize > 0 ? scaledFontSize : 1);
        if (_font == IntPtr.Zero)
            throw new InvalidOperationException($"Impossibile caricare font scalato: {SDL.SDL_GetError()}");
        
        var isC64Mode = _activeFontPath != null && Path.GetFileName(_activeFontPath).Equals("C64_Pro_Mono-STYLE.otf", StringComparison.OrdinalIgnoreCase);

        if (isC64Mode)
        {
            SDL_ttf.TTF_SetFontHinting(_font, SDL_ttf.TTF_HINTING_MONO);
            if (SDL_ttf.TTF_GlyphMetrics(_font, 'M', out _, out _, out _, out _, out var advance) != 0)
                throw new InvalidOperationException($"Impossibile leggere le metriche del font C64: {SDL.SDL_GetError()}");
            _charWidth = advance > 0 ? advance : 1;
        }
        else
        {
            SDL_ttf.TTF_SetFontHinting(_font, SDL_ttf.TTF_HINTING_NORMAL);
            if (SDL_ttf.TTF_SizeUTF8(_font, "M", out var pixelCharWidth, out _) != 0)
                throw new InvalidOperationException($"Impossibile misurare metriche font: {SDL.SDL_GetError()}");
            _charWidth = pixelCharWidth > 0 ? pixelCharWidth : 1;
        }
        _charHeight = SDL_ttf.TTF_FontHeight(_font) > 0 ? SDL_ttf.TTF_FontHeight(_font) : 1;
        
        try
        {
            var white = new SDL.SDL_Color { r = 255, g = 255, b = 255, a = 255 };
            _fontAtlas = new FontAtlas(renderer: _context.RendererHandle, fontPtr: _font,
                hinting: isC64Mode ? SdlFontHinting.Monospace : SdlFontHinting.Normal, initialColor: white,
                firstChar: (char)32, lastChar: (char)(isC64Mode ? 255 : 0x25FF),
                characterRemapping: isC64Mode ? ScreenConfig.GetC64PetsciiRemapping() : null);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[AVVISO] Creazione FontAtlas fallita: {ex.Message}.");
            _fontAtlas = null;
        }
        
        var finalGridWidth = Configuration.CharGridWidth ?? 80;
        var finalGridHeight = Configuration.CharGridHeight ?? 50;
        var totalPixelWidth = finalGridWidth * _charWidth;
        var totalPixelHeight = finalGridHeight * _charHeight;
        var finalWindowWidthInPoints = (int)Math.Round(totalPixelWidth / renderScale);
        var finalWindowHeightInPoints = (int)Math.Round(totalPixelHeight / renderScale);
        
        _width = finalGridWidth;
        _height = finalGridHeight;
        _viewWidth = _width;
        _viewHeight = _height;
        
        SDL.SDL_SetWindowSize(_context.WindowHandle, finalWindowWidthInPoints, finalWindowHeightInPoints);
        SDL.SDL_SetWindowPosition(_context.WindowHandle, Configuration.WindowX ?? SDL.SDL_WINDOWPOS_CENTERED, Configuration.WindowY ?? SDL.SDL_WINDOWPOS_CENTERED);

        _screenTexture = SDL.SDL_CreateTexture(_context.RendererHandle, SDL.SDL_PIXELFORMAT_RGBA8888,
            (int)SDL.SDL_TextureAccess.SDL_TEXTUREACCESS_TARGET, totalPixelWidth, totalPixelHeight);
        if (_screenTexture == IntPtr.Zero)
            throw new InvalidOperationException($"Impossibile creare la screen texture: {SDL.SDL_GetError()}");

        SDL.SDL_SetRenderDrawBlendMode(_context.RendererHandle, SDL.SDL_BlendMode.SDL_BLENDMODE_BLEND);

        _forceRedraw = true;
        _isContentDirty = true;
    }
    
    /// <summary>
    /// Esegue lo scroll del contenuto della console di una riga verso l'alto.
    /// </summary>
    private void ScrollUp()
    {
        var top = _scrollBottom >= 0 ? _scrollTop : 0;
        var bottom = _scrollBottom >= 0 ? _scrollBottom : _height - 1;

        for (var r = top; r < bottom; r++)
        for (var c = 0; c < _width; c++)
        {
            _screenBuffer[r, c] = _screenBuffer[r + 1, c];
            MarkCellDirty(r, c);
        }
        var emptyCell = new ScreenCell(' ', ((ITerminalBuffer)this).CurrentForegroundColor, ((ITerminalBuffer)this).CurrentBackgroundColor);
        for (var c = 0; c < _width; c++)
        {
            _screenBuffer[bottom, c] = emptyCell;
            MarkCellDirty(bottom, c);
        }
        _isContentDirty = true;
    }
    
    /// <summary>
    /// Marca una cella come "sporca", indicando che necessita di essere ridisegnata.
    /// </summary>
    /// <param name="row">La riga della cella da marcare.</param>
    /// <param name="col">La colonna della cella da marcare.</param>
    private void MarkCellDirty(int row, int col)
    {
        if (row >= 0 && row < _height && col >= 0 && col < _width)
        {
            lock (_renderLock)
            {
                _dirtyCells.Add(new Point(col, row));
            }
            _isContentDirty = true;
        }
    }
    
        #region Implementazione Metodi di Manipolazione del Buffer

    /// <summary>
    /// Legge un blocco rettangolare di celle dal buffer di schermo.
    /// Opera in coordinate di caratteri.
    /// </summary>
    /// <param name="rect">Il rettangolo (in caratteri) che definisce l'area da leggere.</param>
    /// <returns>Una matrice di ScreenCell con i dati dell'area specificata.</returns>
    public ScreenCell[,] Get(ScreenRect rect)
    {
        var result = new ScreenCell[rect.h, rect.w];
        var defaultFg = ((ITerminalBuffer)this).CurrentForegroundColor;
        var defaultBg = ((ITerminalBuffer)this).CurrentBackgroundColor;
        var emptyCell = new ScreenCell(' ', defaultFg, defaultBg);

        for (var r = 0; r < rect.h; r++)
        {
            for (var c = 0; c < rect.w; c++)
            {
                var sourceX = rect.x + c;
                var sourceY = rect.y + r;

                // Se le coordinate sorgente sono valide, copia la cella,
                // altrimenti inserisci una cella vuota.
                if (sourceX >= 0 && sourceX < _width && sourceY >= 0 && sourceY < _height)
                {
                    result[r, c] = _screenBuffer[sourceY, sourceX];
                }
                else
                {
                    result[r, c] = emptyCell;
                }
            }
        }
        return result;
    }

    /// <summary>
    /// Scrive un blocco di celle nel buffer in coordinate di caratteri, eseguendo il clipping.
    /// </summary>
    /// <param name="x">La colonna di destinazione dove iniziare a scrivere.</param>
    /// <param name="y">La riga di destinazione dove iniziare a scrivere.</param>
    /// <param name="cells">La matrice di ScreenCell da scrivere.</param>
    public void Put(int x, int y, ScreenCell[,] cells)
    {
        var sourceRows = cells.GetLength(0);
        var sourceCols = cells.GetLength(1);

        for (var r = 0; r < sourceRows; r++)
        {
            for (var c = 0; c < sourceCols; c++)
            {
                var destX = x + c;
                var destY = y + r;

                // Esegui la scrittura solo se la destinazione è all'interno dei limiti del buffer.
                if (destX >= 0 && destX < _width && destY >= 0 && destY < _height)
                {
                    _screenBuffer[destY, destX] = cells[r, c];
                    MarkCellDirty(destY, destX); // Marca la cella come "sporca" per il rendering
                }
            }
        }
        _isContentDirty = true;
    }

    /// <summary>
    /// Pulisce un'area rettangolare del buffer in coordinate di caratteri.
    /// </summary>
    /// <param name="rect">Il rettangolo (in caratteri) che definisce l'area da pulire.</param>
    /// <param name="fillCell">La cella da usare per il riempimento, o null per quella di default.</param>
    public void ClearRect(ScreenRect rect, ScreenCell? fillCell = null)
    {
        var defaultFg = ((ITerminalBuffer)this).CurrentForegroundColor;
        var defaultBg = ((ITerminalBuffer)this).CurrentBackgroundColor;
        
        // Se non viene fornita una cella, crea quella di default.
        var cellToFill = fillCell ?? new ScreenCell(' ', defaultFg, defaultBg);

        // Calcola l'intersezione tra il rettangolo richiesto e i limiti del buffer
        var startX = Math.Max(0, rect.x);
        var startY = Math.Max(0, rect.y);
        var endX = Math.Min(_width, rect.x + rect.w);
        var endY = Math.Min(_height, rect.y + rect.h);

        for (var r = startY; r < endY; r++)
        {
            for (var c = startX; c < endX; c++)
            {
                _screenBuffer[r, c] = cellToFill;
                MarkCellDirty(r, c); // Marca la cella come "sporca"
            }
        }
        _isContentDirty = true;
    }

    #endregion

    /// <summary>
    /// Sincronizza il buffer TUI condiviso con lo stato corrente del buffer interno.
    /// </summary>
    private void SyncToTuiScreen()
    {
        for (var r = 0; r < _height && r < _tuiScreen.Height; r++)
            for (var c = 0; c < _width && c < _tuiScreen.Width; c++)
                _tuiScreen.Cells[r, c] = _screenBuffer[r, c];
    }
}