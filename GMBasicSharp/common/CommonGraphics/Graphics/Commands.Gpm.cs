using Basic;
using CommonLib.Screens;
using SDL2;
/*
----------------------------------------------------------------------
  Comandi Grafici GPM-Compatibili (Interpreter.Graphics.Commands.Gpm.cs)
----------------------------------------------------------------------
Questo file contiene i duplicati GPM-compatibili di tutti i comandi
pubblici del GraphicsManager. Questi metodi sono il nuovo punto di
ingresso per l'Executor GPM.

Ogni metodo in questo file:
- Ha un suffisso "_GPM" per distinguerlo dalla sua controparte async.
- Non restituisce un Task.
- Accetta un Action<object?, Exception?> completionCallback.
- Utilizza il metodo EnqueueGpmOperation per accodare il lavoro al
  thread principale in modo non bloccante, passando il callback
  per notificare all'Executor il completamento dell'operazione.
*/

namespace CommonGraphics.Graphics;

public sealed partial class GraphicsManager
{
    #region GPM-Compliant Public Graphics Commands

    /// <summary>
    /// Contiene lo stato testuale corrente letto dal GraphicsManager.
    /// Usato come valore di ritorno composito per le funzioni GPM.
    /// </summary>
    public sealed record TextMetrics(int CursorX, int CursorY, int CharWidth, int CharHeight);

    /// <summary>
    /// VERSIONE GPM: Legge la posizione corrente del cursore e le metriche del testo.
    /// Accoda l'operazione e restituisce un oggetto composito tramite callback.
    /// </summary>
    public void GET_TEXT_METRICS_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => new TextMetrics(
            _cursorPosition.x,
            _cursorPosition.y,
            TextWidthInternal(" "),
            TextHeightInternal()
        ), completionCallback);

    /// <summary>
    /// VERSIONE GPM: Sposta il cursore alla prossima zona di stampa.
    /// Accoda le operazioni necessarie e notifica il completamento.
    /// </summary>
    public void MOVE_TO_NEXT_PRINT_ZONE_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            var charWidth = TextWidthInternal(" ");
            if (charWidth == 0) charWidth = 8; // Fallback
            const int printZoneWidthChars = 14;
            var currentColumn = _cursorPosition.x / charWidth;
            var nextZoneStartColumn = (currentColumn / printZoneWidthChars + 1) * printZoneWidthChars;
            var targetX = nextZoneStartColumn * charWidth;
            LocateInternal(targetX, _cursorPosition.y);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Sposta il cursore a capo.
    /// Accoda le operazioni necessarie e notifica il completamento.
    /// </summary>
    public void NEWLINE_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            var newY = _cursorPosition.y + TextHeightInternal();
            LocateInternal(0, newY);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Legge la posizione corrente del cursore.
    /// Accoda l'operazione e restituisce una tupla (X, Y) tramite callback.
    /// </summary>
    public void GET_CURSOR_POSITION_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => CursorPosition, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la posizione del cursore.
    /// Questo è un alias per LOCATE_GPM per maggiore chiarezza semantica.
    /// </summary>
    public void SET_CURSOR_POSITION_GPM(int x, int y, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            LocateInternal(x, y);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Legge i componenti RGB di un colore dalla palette
    /// e restituisce il risultato tramite callback.
    /// Questa versione chiama il metodo interno sincrono 'GetColor' in modo thread-safe.
    /// </summary>
    public void GET_PALETTE_COLOR_GPM(int index, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => GetColor(index), completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la modalità dello schermo. Accoda l'operazione e notifica il completamento tramite callback.
    /// </summary>
    public void SCREEN_GPM(int m, int ap, int vp, ScreenConfig? config,
        ScreenWindowMode windowMode,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            ScreenInternal(m, ap, vp, config, performImplicitCls: true, windowMode: windowMode);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Cancella lo schermo attivo. Accoda l'operazione e notifica il completamento tramite callback.
    /// </summary>
    public void CLS_GPM(int? c, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            ClsInternal(c);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta i colori di primo piano e/o sfondo. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void COLOR_GPM(int? f, int? b, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            ColorInternal(f, b);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Disegna una linea o un rettangolo. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void LINE_GPM(int x1, int y1, int x2, int y2, int? c, string? s,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            LineInternal(x1, y1, x2, y2, c, s);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Disegna un singolo pixel. Accoda l'operazione e notifica il completamento.
    /// </summary>
    private (int, int, int, int)? _mouseLimit;

    // ── MOUSE (comandi config — SDL reale sul main thread) ────────────────────
    /// <summary>Abilita/disabilita il cursore mouse (SDL_ShowCursor).</summary>
    public void MouseEnable_GPM(bool enabled, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SDL2.SDL.SDL_ShowCursor(enabled ? 1 : 0);
            return null;
        }, completionCallback);

    /// <summary>Mostra/nasconde il cursore mouse (SDL_ShowCursor).</summary>
    public void MouseShow_GPM(bool visible, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SDL2.SDL.SDL_ShowCursor(visible ? 1 : 0);
            return null;
        }, completionCallback);

    /// <summary>Sposta il cursore del mouse nella finestra del programma.</summary>
    public void MouseSetPos_GPM(int x, int y, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            if (_programContext is not null)
            {
                if (_mouseLimit is { } lim)
                {
                    x = Math.Clamp(x, lim.Item1, lim.Item3);
                    y = Math.Clamp(y, lim.Item2, lim.Item4);
                }
                SDL2.SDL.SDL_WarpMouseInWindow(_programContext.WindowHandle, x, y);
            }
            return null;
        }, completionCallback);

    /// <summary>Imposta il tipo di cursore (SDL system cursor).</summary>
    public void MouseCursor_GPM(int cursorId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            var type = (SDL2.SDL.SDL_SystemCursor)cursorId;
            var cursor = SDL2.SDL.SDL_CreateSystemCursor(type);
            if (cursor != IntPtr.Zero) { SDL2.SDL.SDL_SetCursor(cursor); }
            return null;
        }, completionCallback);

    /// <summary>Limita il rettangolo di movimento del mouse: il limite viene salvato e
    /// applicato a ogni MouseSetPos successivo (clamp) — la SDL2-CS di questo progetto
    /// non espone SDL_SetWindowMouseRect.</summary>
    public void MouseLimit_GPM(int? x1, int? y1, int? x2, int? y2, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            _mouseLimit = x1.HasValue && y1.HasValue && x2.HasValue && y2.HasValue
                ? (Math.Min(x1.Value, x2.Value), Math.Min(y1.Value, y2.Value),
                   Math.Max(x1.Value, x2.Value), Math.Max(y1.Value, y2.Value))
                : ((int, int, int, int)?)null;
            return null;
        }, completionCallback);

    /// <summary>MOUSE WAIT: attende un movimento/click (delay breve, non bloccante).</summary>
    public void MouseWait_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            // Nessuna finestra grafica → niente da attendere; se c'è, il polling eventi
            // del game loop continuerà a girare — la chiamata è solo un punto di sync.
            Thread.Sleep(1);
            return null;
        }, completionCallback);

    /// <summary>GRAPHICS.BSAVE (IMAGEGET): salva l'area (x1,y1)-(x2,y2) in un buffer interno
    /// (parità GMO). Il buffer BASIC passato dal campione è ignorato.</summary>
    public void BSAVE_AREA_GPM(int x1, int y1, int x2, int y2,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            var sc = _renderScale > 0 ? _renderScale : 1.0f;
            var bx = (int)(Math.Min(x1, x2) * sc);
            var by = (int)(Math.Min(y1, y2) * sc);
            var bw = (int)((Math.Abs(x2 - x1) + 1) * sc);
            var bh = (int)((Math.Abs(y2 - y1) + 1) * sc);
            _savedAreas["scr"] = ReadActivePageToBuffer(new SDL.SDL_Rect { x = bx, y = by, w = bw, h = bh });
            return null;
        }, completionCallback);

    /// <summary>GRAPHICS.BLOAD (IMAGEPUT): ripristina a (x,y) l'area salvata con BSAVE.</summary>
    public void BLOAD_AREA_GPM(int x, int y,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            if (_savedAreas.TryGetValue("scr", out var area) && _programContext is not null)
            {
                var sc = _renderScale > 0 ? _renderScale : 1.0f;
                var side = (int)Math.Sqrt(area.Length);
                if (side < 1) return null;
                var page = ReadActivePageToBuffer();
                if (page.Length == 0) return null;
                var sw = _backingWidth > 0 ? _backingWidth : _screenWidth;
                var sh = _backingHeight > 0 ? _backingHeight : _screenHeight;
                var sx = (int)(x * sc);
                var sy = (int)(y * sc);
                for (var yy = 0; yy < side; yy++)
                    for (var xx = 0; xx < side; xx++)
                    {
                        var idx = yy * side + xx;
                        if (idx >= area.Length) continue;
                        var dx = sx + xx;
                        // ReadActivePageToBuffer restituisce le righe FLIPPATE (riga 0 del
                        // buffer = ultima del rect): la riga yy dell'area è la riga visiva
                        // (side-1-yy). Compensa il flip al ripristino (come ImagePutInternal).
                        var dy = sy + (side - 1 - yy);
                        if (dx < 0 || dx >= sw || dy < 0 || dy >= sh) continue;
                        var bufY = sh - 1 - dy;
                        if (bufY >= 0 && bufY < sh) page[bufY * sw + dx] = area[idx];
                    }
                UpdateActivePageFromBuffer(page);
            }
            return null;
        }, completionCallback);

    /// <summary>DRAW TEXT "testo", x, y — disegna testo grafico alla posizione data.</summary>
    public void DrawText_GPM(string text, int x, int y, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            if (_programContext is not null)
            {
                _cursorPosition = (x, y);
                PrintInternal(text);
            }
            return null;
        }, completionCallback);

    /// <summary>DRAW TEXT con allineamento (Left/Center/Right): DrawTextInternal con il
    /// colore corrente — parità PCodeVM per ALIGN_CENTER/ALIGN_RIGHT (DrawText_GPM non
    /// gestisce l'allineamento).</summary>
    public void DrawTextAlign_GPM(string text, int x, int y, int align,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            if (_programContext is not null)
            {
                var color = ((uint)_currentColor[0] << 24)
                          | ((uint)_currentColor[1] << 16)
                          | ((uint)_currentColor[2] << 8)
                          | 0xFFu;
                // Coordinate LOGICHE → native (× _renderScale), come PrintInternal mappa il
                // cursore. DrawTextInternal NON scala (blit 1:1 col font già nativo).
                // DrawTextInternal centra in verticale (destY = y - h/2): si passa il CENTRO
                // del testo in coordinate native (y*sc + TextHeight()*sc/2) così (x,y) logico
                // resta il bordo superiore del testo e l'align agisce solo sull'asse X.
                var sc = _renderScale > 0 ? _renderScale : 1.0f;
                var th = TextHeightInternal() * sc;
                var ty = (int)(y * sc) + (int)(th / 2);
                DrawTextInternal(text, (int)(x * sc), ty, 0, (TextAlign)align, color);
            }
            return null;
        }, completionCallback);

    public void PSET_GPM(double x, double y, int? c, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            PsetInternal(x, y, c);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Disegna un cerchio o un'ellisse. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void CIRCLE_GPM(int x, int y, int r, int? c, double? sa, double? ea, double? a,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            CircleInternal(x, y, r, c, sa, ea, a);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Riempie un'area grafica. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void PAINT_GPM(int x, int y, int? fc, int? bc, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            PaintInternal(x, y, fc, bc);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta il font corrente. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void FONT_GPM(string p, int s, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            FontInternal(p, s);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Stampa una stringa di testo. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void PRINT_GPM(string t, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            PrintInternal(t);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Posiziona il cursore grafico. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void LOCATE_GPM(int x, int y, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            LocateInternal(x, y);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Disegna un'immagine da un array. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void IMAGEPUT_GPM(int x, int y, int[] i, string m, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            ImagePutInternal(x, y, i, m);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Disegna in batch più istanze di uno sprite. Accoda l'operazione e notifica il completamento.
    /// Disegna subito (sulla pagina attiva = back buffer del doppio buffer): il CLS del frame
    /// è stato applicato sulla stessa pagina dall'istruzione CLS del BASIC, che lo precede nella
    /// coda FIFO — quindi CLS e IMAGEPUTS cadono nello stesso Present (stesso ordine della coda).
    /// </summary>
    public void IMAGEPUTS_GPM(int[] spriteData, int[] positions, string mode,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            ImagePutsInternal(spriteData, positions, mode);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Esegue una stringa di comandi DRAW. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void DRAW_GPM(string cs, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            DrawInternal(cs);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Copia una pagina grafica. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void PCOPY_GPM(int sp, int dp, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            PCopyInternal(sp, dp);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la pagina visibile e/o attiva. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SCREENSET_GPM(int? v, int? a, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            ScreenSetInternal(v, a);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Cattura un'area dello schermo e restituisce il risultato tramite callback.
    /// </summary>
    public void IMAGEGET_GPM(int x, int y, int w, int h, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => ImageGetInternal(x, y, w, h), completionCallback);

    /// <summary>
    /// VERSIONE GPM: Calcola la larghezza di un testo e restituisce il risultato tramite callback.
    /// </summary>
    public void TEXTWIDTH_GPM(string t, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => TextWidthInternal(t), completionCallback);

    /// <summary>
    /// VERSIONE GPM: Tab-zone della console in grafica (la virgola di PRINT).
    /// Replica il Deprecated: posiziona il cursore all'inizio della zona di 14
    /// caratteri successiva (larghezza calcolata col font corrente), stessa riga.
    /// </summary>
    public void TAB_ZONE_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            var charW = TextWidthInternal(" ");
            if (charW <= 0) charW = 8;
            const int zoneChars = 14;
            var curX = _cursorPosition.x;
            var curY = _cursorPosition.y;
            var currentCol = curX / charW;
            var nextZone = (currentCol / zoneChars + 1) * zoneChars;
            _cursorPosition = (nextZone * charW, curY);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Calcola l'altezza del font e restituisce il risultato tramite callback.
    /// </summary>
    public void TEXTHEIGHT_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => TextHeightInternal(), completionCallback);

    /// <summary>
    /// VERSIONE GPM: Salva lo stato dello schermo. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SAVE_SCREEN_GPM(string name, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SaveScreenInternal(name);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Ripristina lo stato dello schermo. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void RESTORE_SCREEN_GPM(string name, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            RestoreScreenInternal(name);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Legge il colore di un pixel e restituisce il risultato tramite callback.
    /// </summary>
    public void POINT_GPM(int x, int y, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => PointInternal(x, y), completionCallback);

    /// <summary>
    /// VERSIONE GPM: POLYGON — riempie un poligono (scanline) col colore dato.
    /// Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void POLYGON_GPM((float x, float y)[] points, int color, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { PolygonInternal(points, color); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Modifica la palette. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void PALETTE_GPM(int index, int r, int g, int b, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            PaletteInternal(index, r, g, b);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Carica una texture e restituisce il suo ID tramite callback.
    /// </summary>
    public void LOAD_TEXTURE_GPM(string filePath, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => LoadTextureInternal(filePath), completionCallback);

    /// <summary>LOAD TEXTURE … CELL(w,h): array di id, stesso enqueue GPM del load singolo.</summary>
    public void LOAD_TEXTURE_SHEET_GPM(string filePath, int cellW, int cellH,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => LoadTextureSheetInternal(filePath, cellW, cellH), completionCallback);


    /// <summary>
    /// VERSIONE GPM: Disegna una texture. Accoda l'operazione e notifica il completamento.
    /// `color` = tint ARGB per-sprite (&hAARRGGBB, 0 = texture invariata) — GRAFX-01.
    /// </summary>
    public void DRAW_TEXTURE_GPM(int textureId, int x, int y, double rotation, double scaleX, double? scaleY, int color,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            DrawTextureInternal(textureId, x, y, rotation, scaleX, scaleY, color);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Disegna in batch più texture. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void DRAW_TEXTURES_GPM(int textureId, Array xPos, Array yPos, Array rotations, Array scales,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            DrawTexturesInternal(textureId, xPos, yPos, rotations, scales);
            return null;
        }, completionCallback);

    #endregion

    #region GPM-Compliant Public Sprite & Matrix Commands

    /// <summary>
    /// VERSIONE GPM: Carica uno sprite e restituisce il suo ID tramite callback.
    /// </summary>
    public void SPRITE_LOAD_GPM(string filePath, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => LoadSpriteInternal(filePath), completionCallback);

    /// <summary>VERSIONE GPM: restituisce i pixel ARGB di uno sprite CARICATO nel formato array
    /// IMAGEGET [w,h,index...] con -1 per i pixel trasparenti. Eseguito sul main thread.</summary>
    public void GetSpriteImagePixelsGpm(int spriteId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            var px = GetSpriteImagePixels(spriteId);
            if (px == null || px.Length == 0) return new int[] { 0, 0 };
            return (object)px;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la posizione di uno sprite. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SPRITE_SET_POSITION_GPM(int spriteId, float x, float y,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetSpritePositionInternal(spriteId, x, y);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: imposta la scala di uno sprite (SPRITE SET SCALE id, sx, sy).
    /// </summary>
    public void SPRITE_SET_SCALE_GPM(int spriteId, float sx, float sy,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetSpriteScaleInternal(spriteId, sx, sy);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: imposta la rotazione di uno sprite in gradi (SPRITE SET ROTATION id, deg).
    /// </summary>
    public void SPRITE_SET_ROTATION_GPM(int spriteId, float degrees,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetSpriteRotationInternal(spriteId, degrees);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Rende uno sprite invisibile. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SPRITE_OFF_GPM(int spriteId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetSpriteVisibilityInternal(spriteId, false);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Rende uno sprite visibile. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SPRITE_ON_GPM(int spriteId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetSpriteVisibilityInternal(spriteId, true);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la priorità di uno sprite. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SPRITE_PRIORITY_GPM(int spriteId, int priority, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetSpritePriorityInternal(spriteId, priority);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Esegue l'aggiornamento e il disegno di tutti gli sprite. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SYNC_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            UpdateAndDrawAllSpritesInternal();
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Associa una matrice a uno sprite. Accoda l'operazione e notifica il completamento.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite.</param>
    /// <param name="matrixId">L'ID della matrice LINALG da associare (0 per rimuovere l'associazione).</param>
    /// <param name="completionCallback">Il callback da invocare al termine.</param>
    public void SPRITE_SET_MATRIX_GPM(int spriteId, long matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetSpriteMatrixInternal(spriteId, matrixId);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Applica una trasformazione a uno sprite. Accoda l'operazione e notifica il completamento.
    /// </summary>
    /// <param name="spriteId">L'ID dello sprite da trasformare.</param>
    /// <param name="matrixId">L'ID della matrice LINALG da applicare.</param>
    /// <param name="completionCallback">Il callback da invocare al termine.</param>
    public void SPRITE_TRANSFORM_GPM(int spriteId, long matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            TransformSpriteInternal(spriteId, matrixId);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Verifica collisione AABB tra due sprite. Restituisce -1L (collisione) o 0L (nessuna).
    /// </summary>
    /// <param name="id1">L'ID del primo sprite.</param>
    /// <param name="id2">L'ID del secondo sprite.</param>
    /// <param name="completionCallback">Il callback da invocare con il risultato.</param>
    public void SPRITECOLLIDE_GPM(int id1, int id2, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => CheckCollisionInternal(id1, id2) ? -1L : 0L, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Verifica collisione AABB di uno sprite con tutti gli altri visibili.
    /// Restituisce un array di int con gli ID degli sprite collisi.
    /// </summary>
    /// <param name="id">L'ID dello sprite da testare.</param>
    /// <param name="completionCallback">Il callback da invocare con il risultato.</param>
    public void SPRITECOLLIDE_ALL_GPM(int id, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => (object)CheckCollisionAllInternal(id), completionCallback);

    /// <summary>
    /// VERSIONE GPM: Aggiorna la matrice di MONDO (World) di rendering del GraphicsManager.
    /// Questa funzione orchestra la conversione da una matrice LINALG (MathNet) a una matrice grafica (System.Numerics).
    /// </summary>
    /// <param name="matrixId">L'handle della matrice LINALG da cui aggiornare.</param>
    /// <param name="completionCallback">Il callback da invocare al termine.</param>
    public void UPDATE_WORLD_MATRIX_GPM(long matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { UpdateWorldMatrixInternal(matrixId); return null; }, completionCallback);

    public void UPDATE_VIEW_MATRIX_GPM(long matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { UpdateViewMatrixInternal(matrixId); return null; }, completionCallback);

    public void UPDATE_PROJECTION_MATRIX_GPM(long matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { UpdateProjectionMatrixInternal(matrixId); return null; }, completionCallback);

    
    /*
    /// <summary>
    /// VERSIONE GPM: Crea una matrice e restituisce il suo ID tramite callback.
    /// </summary>
    public void CREATE_MATRIX_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => CreateMatrixInternal(), completionCallback);

    /// <summary>
    /// VERSIONE GPM: Resetta una matrice a identità. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void IDENTITY_MATRIX_GPM(int matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { IdentityMatrixInternal(matrixId); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Applica una traslazione a una matrice. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void TRANSLATE_MATRIX_GPM(int matrixId, float x, float y, float z, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { TranslateMatrixInternal(matrixId, x, y, z); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Applica una rotazione a una matrice. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void ROTATE_MATRIX_GPM(int matrixId, float ax, float ay, float az, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { RotateMatrixInternal(matrixId, ax, ay, az); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Applica una scalatura a una matrice. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void SCALE_MATRIX_GPM(int matrixId, float sx, float sy, float sz, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { ScaleMatrixInternal(matrixId, sx, sy, sz); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Moltiplica due matrici. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void MULTIPLY_MATRIX_GPM(int m1, int m2, int result, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { MultiplyMatrixInternal(m1, m2, result); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Salva una matrice sullo stack. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void MATRIX_PUSH_GPM(int matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { MatrixPushInternal(matrixId); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Ripristina una matrice dallo stack. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void MATRIX_POP_GPM(int matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { MatrixPopInternal(matrixId); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la matrice di vista. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void VIEW_MATRIX_GPM(int matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { SetViewMatrixInternal(matrixId); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la matrice di proiezione. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void PROJECTION_MATRIX_GPM(int matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { SetProjectionMatrixInternal(matrixId); return null; }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Imposta la matrice di mondo. Accoda l'operazione e notifica il completamento.
    /// </summary>
    public void WORLD_MATRIX_GPM(int matrixId, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() => { SetWorldMatrixInternal(matrixId); return null; }, completionCallback);
    */

    #endregion

    /// <summary>
    /// VERSIONE GPM: Imposta il titolo della finestra del programma in modo thread-safe.
    /// Accoda l'operazione e notifica il completamento.
    /// </summary>
    /// <param name="title">Il nuovo titolo da visualizzare.</param>
    /// <param name="completionCallback">Il callback da invocare al termine.</param>
    public void SET_WINDOW_TITLE_GPM(string title, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            SetWindowTitle(title);
            return null;
        }, completionCallback);

    /// <summary>
    /// VERSIONE GPM: Inserisce programmaticamente un evento SDL_QUIT nella coda degli eventi.
    /// Questo è il modo corretto e thread-safe per richiedere la chiusura dell'applicazione
    /// dall'Executor. Accoda l'operazione e notifica il completamento.
    /// </summary>
    /// <param name="completionCallback">Il callback da invocare al termine.</param>
    public void REQUEST_QUIT_GPM(Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            RequestQuit();
            return null;
        }, completionCallback);

/// <summary>
/// VERSIONE GPM: Disegna una bitmap da un array di byte a una posizione specifica.
/// Accoda l'operazione di decodifica e rendering sul thread grafico principale.
/// </summary>
/// <param name="bmpData">L'array di byte che rappresenta l'immagine in formato BMP.</param>
/// <param name="x">La coordinata X in cui disegnare l'angolo in alto a sinistra della bitmap.</param>
/// <param name="y">La coordinata Y in cui disegnare l'angolo in alto a sinistra della bitmap.</param>
/// <param name="completionCallback">Il callback da invocare al termine dell'operazione.</param>
public void DRAW_BITMAP_GPM(byte[] bmpData, int x, int y, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        DrawBitmapInternal(bmpData, x, y);
        return null;
    }, completionCallback);

/// <summary>
/// VERSIONE GPM: Carica un sprite definito da dati pixel SPRITEDEF in memoria.
/// Accoda l'operazione di creazione texture e registra l'handle.
/// </summary>
/// <param name="spriteId">L'handle ID da assegnare allo sprite.</param>
/// <param name="width">Larghezza in pixel.</param>
/// <param name="height">Altezza in pixel.</param>
/// <param name="pixelData">Array RGBA dei pixel (4 byte per pixel).</param>
/// <param name="completionCallback">Il callback da invocare al termine.</param>
public void SPRITEDEF_LOAD_GPM(int spriteId, int width, int height, byte[] pixelData, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        SpriteDefLoadInternal(spriteId, width, height, pixelData);
        return null;
    }, completionCallback);

/// <summary>
/// VERSIONE GPM per i sottocomandi TILEMAP. TilemapManager esegue chiamate SDL dirette
/// (SDL_RenderCopy in Draw()), quindi va sempre marshalizzato sul thread principale
/// tramite EnqueueGpmOperation come ogni altro comando grafico — mai chiamato direttamente
/// da un thread worker della P-CODE VM.
/// </summary>
public void TILEMAP_LOAD_GPM(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        TilemapManager.Instance.Load(textureId, tileW, tileH, tilesPerRow, mapW, mapH, tiles);
        return null;
    }, completionCallback);

public void TILEMAP_SET_GPM(int row, int col, int tileIndex, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        TilemapManager.Instance.SetTile(row, col, tileIndex);
        return null;
    }, completionCallback);

public void TILEMAP_OFFSET_GPM(float x, float y, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        TilemapManager.Instance.SetOffset(x, y);
        return null;
    }, completionCallback);

public void TILEMAP_SCROLL_GPM(float dx, float dy, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        TilemapManager.Instance.Scroll(dx, dy);
        return null;
    }, completionCallback);

public void TILEMAP_DRAW_GPM(Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        TilemapManager.Instance.Draw();
        return null;
    }, completionCallback);

public void VIEW_GPM(int x1, int y1, int x2, int y2, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        SetViewport(x1, y1, x2, y2);
        return null;
    }, completionCallback);

public void WINDOW_GPM(double x1, double y1, double x2, double y2, bool screen, Action<object?, Exception?> completionCallback) =>
    EnqueueGpmOperation(() =>
    {
        SetWindow(x1, y1, x2, y2, screen);
        return null;
    }, completionCallback);
}