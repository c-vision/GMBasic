// FILE: Basic.IlBackend.PCode.PCodeGraphics.cs
// Router grafico della PCodeVM.
//
// Storico: la PCodeVM chiamava GraphicsManager (C#) direttamente per ogni
// operazione grafica. Con il motore unico C serve che le stesse chiamate vadano
// a IGraphicsService/CGfxGraphicsService quando l'host è in modalità C
// (GMB_GFX_C=1), mantenendo identico il comportamento C# altrimenti.
//
// Contratto: stesse firme dei metodi GraphicsManager.*_GPM usati dalla PCodeVM,
// completion park-based (mai attese). In modalità C le operazioni senza variante
// *Gpm dedicata usano CGfxGraphicsService.RunGpm (esecuzione sul main thread +
// completion dopo l'esecuzione). I colori delle finestre PIXEL e le matrici 3D
// restano sul percorso C# (canvas/finestre sono oggetti C#; le matrici 3D non
// hanno equivalente C: errore esplicito in modalità C).
using System;
using System.Linq;
using Basic.Runtime;
using CommonGraphics.Graphics;
using CommonLib.Screens;

namespace Basic.IlBackend.PCode;

internal sealed class PCodeGraphics
{
    internal static PCodeGraphics Instance { get; } = new();

    private CGfxGraphicsService? _c;

    /// <summary>Collega il router all'host corrente: motore C se l'host usa
    /// CGfxGraphicsService, altrimenti GraphicsManager (C#). Chiamato dal
    /// PCodeVmRunner prima di eseguire il chunk.</summary>
    internal static void Bind(IRuntimeHost host) => Instance._c = host.Graphics as CGfxGraphicsService;

    internal bool IsScreenOn => _c is null ? GraphicsManager.Instance.IsScreenOn : CGfxGraphicsService.IsScreenOnC;

    /// <summary>Spazio coordinate DICHIARATO (SCREEN ... USE CELLS|PIXELS). Motore unico: runtime C.</summary>
    internal void CoordMode(int mode)
    {
        if (_c is null) return;      /* motore C# legacy: nessuna conversione dichiarata */
        _c.ScreenCoords(mode);
    }

    internal void TextMode(int mode)
    {
        if (_c is null) GraphicsManager.Instance.TextModeGpm(mode, static (_, _) => { });
        else _c.TextMode(mode);
    }

    internal int CurrentScreenMode => _c is null ? GraphicsManager.Instance.CurrentScreenMode : CGfxGraphicsService.CurrentModeC;

    internal int CharCellWidth => _c is null ? GraphicsManager.Instance.CharCellWidth : (int)_c.CharCellWidthC;

    internal int ScreenWidth => _c is null ? GraphicsManager.Instance.ScreenWidth : _c.ScreenWidthC;

    internal int ScreenHeight => _c is null ? GraphicsManager.Instance.ScreenHeight : _c.ScreenHeightC;

    internal (int X, int Y) CursorPosition => _c is null ? GraphicsManager.Instance.CursorPosition : _c.CursorPositionC;

    /// <summary>Operazione grafica generica park-based (storico: GraphicsManager).</summary>
    /// <summary>WINDOW SHOW/HIDE (globale) — visibilita' della finestra dello screen.</summary>
    internal void WindowVisible(bool visible)
    {
        if (_c is not null) { _c.WindowVisible(visible); }
    }

    internal void EnqueueGpmOperation(Func<object?> op, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.EnqueueGpmOperation(op, cb); return; }
        _c.RunGpm(op, cb);
    }

    internal void SetViewport(int x1, int y1, int x2, int y2)
    {
        if (_c is null) { GraphicsManager.Instance.SetViewport(x1, y1, x2, y2); return; }
        _c.ViewPort(x1, y1, x2, y2);
    }

    internal int CharCellHeight => _c is null ? GraphicsManager.Instance.CharCellHeight : (int)_c.CharCellHeightC;

    /// <summary>Risoluzione colore per le finestre PIXEL (canvas C#): sempre
    /// GraphicsManager, indipendente dal motore attivo.</summary>
    internal uint GfxResolveColor(object? value, uint fallback)
        => GraphicsManager.Instance.GfxResolveColor(value, fallback);

    // ── Screen / console grafica ──
    internal void SCREEN_GPM(int m, int ap, int vp, ScreenConfig? config, ScreenWindowMode swm,
        Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SCREEN_GPM(m, ap, vp, config, swm, cb); return; }
        var cfg = config ?? ScreenConfig.GetDefault((ScreenMode)m);
        _c.ScreenGpm(m, cfg.LogicalWidth, cfg.LogicalHeight, 32, cb);
    }

    internal void SCREENSET_GPM(int? v, int? a, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SCREENSET_GPM(v, a, cb); return; }
        _c.RunGpm(() => _c.ScreenSetPages(v, a), cb);
    }

    internal void CLS_GPM(int? c, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.CLS_GPM(c, cb); return; }
        _c.RunGpm(() => _c.Clear(c), cb);
    }

    internal void COLOR_GPM(int? f, int? b, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.COLOR_GPM(f, b, cb); return; }
        _c.RunGpm(() => _c.Color(f, b), cb);
    }

    internal void LOCATE_GPM(int x, int y, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.LOCATE_GPM(x, y, cb); return; }
        _c.RunGpm(() => _c.Locate(x, y), cb);
    }

    internal void PRINT_GPM(string t, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.PRINT_GPM(t, cb); return; }
        _c.RunGpm(() => _c.Print(t), cb);
    }

    internal void PSET_GPM(double x, double y, int? c, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.PSET_GPM(x, y, c, cb); return; }
        // PSET con colore null = PRESET (pixel → BACKGROUND, parità IL Preset): passare
        // -1 al C userebbe il FOREGROUND (resolve_color(-1)=fg) → 15 invece di 0.
        _c.RunGpm(() => { if (c is null) _c.Preset((int)x, (int)y); else _c.PSet((int)x, (int)y, c.Value); }, cb);
    }

    internal void POINT_GPM(int x, int y, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.POINT_GPM(x, y, cb); return; }
        _c.PointGpm(x, y, cb);
    }

    internal void LINE_GPM(int x1, int y1, int x2, int y2, int? c, string? s, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.LINE_GPM(x1, y1, x2, y2, c, s, cb); return; }
        _c.RunGpm(() => _c.Line(x1, y1, x2, y2, c ?? -1, s), cb);
    }

    internal void CIRCLE_GPM(int x, int y, int r, int? c, double? sa, double? ea, double? a, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.CIRCLE_GPM(x, y, r, c, sa, ea, a, cb); return; }
        _c.RunGpm(() => _c.CircleEx(x, y, r, c ?? -1, sa ?? 0, ea ?? (2 * Math.PI), a ?? 1.0), cb);
    }

    internal void PAINT_GPM(int x, int y, int? fc, int? bc, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.PAINT_GPM(x, y, fc, bc, cb); return; }
        _c.RunGpm(() => _c.Paint(x, y, fc ?? -1, bc ?? -1), cb);
    }

    internal void DRAW_GPM(string cs, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.DRAW_GPM(cs, cb); return; }
        _c.RunGpm(() => _c.Draw(cs), cb);
    }

    internal void DrawText_GPM(string text, int x, int y, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.DrawText_GPM(text, x, y, cb); return; }
        _c.RunGpm(() => _c.DrawText(text, x, y, 0), cb);
    }

    internal void FONT_GPM(string p, int s, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.FONT_GPM(p, s, cb); return; }
        _c.RunGpm(() => _c.Font(p, s), cb);
    }

    internal void PCOPY_GPM(int sp, int dp, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.PCOPY_GPM(sp, dp, cb); return; }
        _c.RunGpm(() => _c.PCopy(sp, dp), cb);
    }

    internal void SAVE_SCREEN_GPM(string name, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SAVE_SCREEN_GPM(name, cb); return; }
        _c.RunGpm(() => _c.SaveScreen(name), cb);
    }

    internal void RESTORE_SCREEN_GPM(string name, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.RESTORE_SCREEN_GPM(name, cb); return; }
        _c.RunGpm(() => _c.RestoreScreen(name), cb);
    }

    internal void TAB_ZONE_GPM(Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.TAB_ZONE_GPM(cb); return; }
        _c.RunGpm(() => _c.GraphicsTabZone(), cb);
    }

    internal void PALETTE_GPM(int index, int r, int g, int b, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.PALETTE_GPM(index, r, g, b, cb); return; }
        _c.RunGpm(() => _c.Palette(index, r, g, b), cb);
    }

    internal void VIEW_GPM(int x1, int y1, int x2, int y2, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.VIEW_GPM(x1, y1, x2, y2, cb); return; }
        _c.RunGpm(() => _c.ViewPort(x1, y1, x2, y2), cb);
    }

    internal void SYNC_GPM(Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SYNC_GPM(cb); return; }
        _c.SyncGpm(cb);
    }

    // ── Immagini / texture ──
    internal void IMAGEGET_GPM(int x, int y, int w, int h, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.IMAGEGET_GPM(x, y, w, h, cb); return; }
        _c.RunGpm(() => (object?)_c.ImageGet(x, y, x + w - 1, y + h - 1), cb);
    }

    internal void IMAGEPUT_GPM(int x, int y, int[] i, string m, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.IMAGEPUT_GPM(x, y, i, m, cb); return; }
        _c.RunGpm(() => _c.ImagePut(x, y, i, 0), cb);
    }

    internal void IMAGEPUTS_GPM(int[] spriteData, int[] positions, string mode, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.IMAGEPUTS_GPM(spriteData, positions, mode, cb); return; }
        _c.RunGpm(() => _c.ImagePuts(spriteData, positions, 0), cb);
    }

    internal void LOAD_TEXTURE_GPM(string filePath, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.LOAD_TEXTURE_GPM(filePath, cb); return; }
        _c.LoadTextureGpm(filePath, cb);
    }

    internal void DRAW_TEXTURE_GPM(int textureId, int x, int y, double rotation, double scaleX, double? scaleY,
        int color, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.DRAW_TEXTURE_GPM(textureId, x, y, rotation, scaleX, scaleY, color, cb); return; }
        _c.RunGpm(() => _c.DrawTextureEx(textureId, x, y, rotation, scaleX, scaleY ?? scaleX, color), cb);
    }

    internal void DRAW_TEXTURES_GPM(int textureId, Array xPos, Array yPos, Array rotations, Array scales,
        Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.DRAW_TEXTURES_GPM(textureId, xPos, yPos, rotations, scales, cb); return; }
        _c.RunGpm(() =>
        {
            var n = Math.Min(Math.Min(xPos.Length, yPos.Length), Math.Min(rotations.Length, scales.Length));
            for (var i = 0; i < n; i++)
            {
                var sc = Convert.ToDouble(scales.GetValue(i));
                _c.DrawTextureEx(textureId, Convert.ToInt32(xPos.GetValue(i)), Convert.ToInt32(yPos.GetValue(i)),
                    Convert.ToDouble(rotations.GetValue(i)), sc, sc, -1);
            }
        }, cb);
    }

    internal void EnqueuePixelBuffer(byte[] argb, int width, int height)
    {
        if (_c is null) { GraphicsManager.Instance.EnqueuePixelBuffer(argb, width, height); return; }
        var px = new uint[width * height];
        for (var i = 0; i < px.Length; i++)
            px[i] = (uint)((argb[i * 4 + 3] << 24) | (argb[i * 4 + 2] << 16) | (argb[i * 4 + 1] << 8) | argb[i * 4]);
        _c.DrawPixelBuffer(width, height, px);
    }

    internal void UnloadTexture(int textureId)
    {
        if (_c is null) { GraphicsManager.Instance.UnloadTexture(textureId); return; }
        _c.UnloadTextureC(textureId);
    }

    internal void UnloadFont()
    {
        if (_c is null) { GraphicsManager.Instance.UnloadFont(); return; }
        _c.UnloadFontC();
    }

    // ── Finestre / viewport ──
    internal void SetWindow(double x1, double y1, double x2, double y2, bool screen)
    {
        if (_c is null) { GraphicsManager.Instance.SetWindow(x1, y1, x2, y2, screen); return; }
        _c.Window((int)x1, (int)y1, (int)x2, (int)y2);
    }

    internal void WINDOW_GPM(double x1, double y1, double x2, double y2, bool screen, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.WINDOW_GPM(x1, y1, x2, y2, screen, cb); return; }
        _c.RunGpm(() => _c.Window((int)x1, (int)y1, (int)x2, (int)y2), cb);
    }

    internal void ResetViewportAndWindow()
    {
        if (_c is null) { GraphicsManager.Instance.ResetViewportAndWindow(); return; }
        _c.ViewPort(0, 0, 0, 0);
        _c.Window(0, 0, 0, 0);
    }

    // ── Collisioni ──
    internal bool Collide(int id1, int id2)
        /* Convenzione BASIC: -1 = collisione, 0 = no. `_c.SpriteCollide` (motore C) ritorna
         * -1/0 → il vecchio `> 0` era SEMPRE falso: su PCode `COLLIDE()` rispondeva 0 anche
         * con gli sprite sovrapposti (C99/IL/VM-C davano -1). */
        => _c is null ? GraphicsManager.Instance.Collide(id1, id2) : _c.SpriteCollide(id1, id2) != 0;

    internal int[] CollideAll(int id)
    {
        if (_c is null) return GraphicsManager.Instance.CollideAll(id);
        return _c.SpriteCollideAll(id)
            .Select(o => (int)Convert.ToDouble(((object[])o)[1]))
            .ToArray();
    }

    // AABB puro su array (nessun sistema sprite): identico nei due modi, CPU-only.
    internal int[] CollideBatch(float[] xs, float[] ys, float w, float h)
        => GraphicsManager.Instance.CollideBatch(xs, ys, w, h);

    internal System.Collections.Generic.List<(int id1, int id2, float x, float y)> TakeLastCollisions()
    {
        if (_c is null) return GraphicsManager.Instance.TakeLastCollisions();
        return _c.TakeLastCollisions()
            .Select(h => (h.Id1, h.Id2, h.X, h.Y))
            .ToList();
    }

    // ── Sprite / tilemap / poligoni ──
    internal void POLYGON_GPM((float x, float y)[] points, int color, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.POLYGON_GPM(points, color, cb); return; }
        var flat = new float[points.Length * 2];
        for (var i = 0; i < points.Length; i++) { flat[i * 2] = points[i].x; flat[i * 2 + 1] = points[i].y; }
        _c.RunGpm(() => _c.Polygon(flat, color), cb);
    }

    internal void SPRITEDEF_LOAD_GPM(int spriteId, int width, int height, byte[] pixelData, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITEDEF_LOAD_GPM(spriteId, width, height, pixelData, cb); return; }
        // RGBA (byte) → packed 0xRRGGBB (0 = trasparente), contratto gmb_gfx_sprite_def.
        var packed = new int[width * height];
        for (var i = 0; i < packed.Length; i++)
        {
            var a = pixelData[i * 4 + 3];
            packed[i] = a == 0 ? 0 : (pixelData[i * 4] << 16) | (pixelData[i * 4 + 1] << 8) | pixelData[i * 4 + 2];
        }
        _c.RunGpm(() => _c.SpriteDef(spriteId, width, height, packed), cb);
    }

    internal void SPRITE_LOAD_GPM(string filePath, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_LOAD_GPM(filePath, cb); return; }
        _c.SpriteLoadGpm(filePath, cb);
    }

    internal void SPRITE_SET_POSITION_GPM(int spriteId, float x, float y, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_SET_POSITION_GPM(spriteId, x, y, cb); return; }
        _c.RunGpm(() => _c.SpriteSetPosition(spriteId, x, y), cb);
    }

    internal void SPRITE_SET_SCALE_GPM(int spriteId, float sx, float sy, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_SET_SCALE_GPM(spriteId, sx, sy, cb); return; }
        _c.RunGpm(() => _c.SpriteSetScale(spriteId, sx, sy), cb);
    }

    internal void SPRITE_SET_ROTATION_GPM(int spriteId, float degrees, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_SET_ROTATION_GPM(spriteId, degrees, cb); return; }
        _c.RunGpm(() => _c.SpriteSetRotation(spriteId, degrees), cb);
    }

    internal void SPRITE_ON_GPM(int spriteId, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_ON_GPM(spriteId, cb); return; }
        _c.RunGpm(() => _c.SpriteSetVisible(spriteId, true), cb);
    }

    internal void SPRITE_OFF_GPM(int spriteId, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_OFF_GPM(spriteId, cb); return; }
        _c.RunGpm(() => _c.SpriteSetVisible(spriteId, false), cb);
    }

    internal void SPRITE_PRIORITY_GPM(int spriteId, int priority, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_PRIORITY_GPM(spriteId, priority, cb); return; }
        _c.RunGpm(() => _c.SpriteSetPriority(spriteId, priority), cb);
    }

    internal void SPRITE_TRANSFORM_GPM(int spriteId, long matrixId, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_TRANSFORM_GPM(spriteId, matrixId, cb); return; }
        _c.RunGpm(() => _c.SpriteSetMatrix(spriteId, matrixId), cb);
    }

    internal void SPRITE_SET_MATRIX_GPM(int spriteId, long matrixId, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.SPRITE_SET_MATRIX_GPM(spriteId, matrixId, cb); return; }
        _c.RunGpm(() => _c.SpriteSetMatrix(spriteId, matrixId), cb);
    }

    internal void GetSpriteImagePixelsGpm(int spriteId, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.GetSpriteImagePixelsGpm(spriteId, cb); return; }
        _c.SpriteGetPixelsGpm(spriteId, cb);
    }

    internal void TILEMAP_LOAD_GPM(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles,
        Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.TILEMAP_LOAD_GPM(textureId, tileW, tileH, tilesPerRow, mapW, mapH, tiles, cb); return; }
        _c.RunGpm(() => _c.TilemapLoad(textureId, tileW, tileH, tilesPerRow, mapW, mapH, tiles), cb);
    }

    internal void TILEMAP_SET_GPM(int row, int col, int tileIndex, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.TILEMAP_SET_GPM(row, col, tileIndex, cb); return; }
        _c.RunGpm(() => _c.TilemapSet(row, col, tileIndex), cb);
    }

    internal void TILEMAP_OFFSET_GPM(float x, float y, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.TILEMAP_OFFSET_GPM(x, y, cb); return; }
        _c.RunGpm(() => _c.TilemapSetOffset(x, y), cb);
    }

    internal void TILEMAP_SCROLL_GPM(float dx, float dy, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.TILEMAP_SCROLL_GPM(dx, dy, cb); return; }
        _c.RunGpm(() => _c.TilemapScroll(dx, dy), cb);
    }

    internal void TILEMAP_DRAW_GPM(Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.TILEMAP_DRAW_GPM(cb); return; }
        _c.RunGpm(() => _c.TilemapDraw(), cb);
    }

    // ── Chart ──
    internal void ChartCreate_GPM(string title, string xLabel, string yLabel, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.ChartCreate_GPM(title, xLabel, yLabel, cb); return; }
        _c.RunGpm(() => (object?)(long)_c.ChartCreate(title, xLabel, yLabel), cb);
    }

    internal void ChartPlot_GPM(long handle, double[] xData, double[] yData, string plotType, string legend,
        Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.ChartPlot_GPM(handle, xData, yData, plotType, legend, cb); return; }
        _c.RunGpm(() => _c.ChartPlot(handle, xData, yData, plotType, legend), cb);
    }

    internal void ChartFinancial_GPM(long handle, object[] ohlc, string plotType, string legend,
        Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.ChartFinancial_GPM(handle, ohlc, plotType, legend, cb); return; }
        var data = new double[ohlc.Length];
        for (var i = 0; i < ohlc.Length; i++) data[i] = Convert.ToDouble(ohlc[i]);
        _c.RunGpm(() => _c.ChartFinancialC(handle, data, plotType, legend), cb);
    }

    internal void ChartClear_GPM(long handle, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.ChartClear_GPM(handle, cb); return; }
        _c.RunGpm(() => _c.ChartClear(handle), cb);
    }

    internal void ChartClose_GPM(long handle, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.ChartClose_GPM(handle, cb); return; }
        _c.RunGpm(() => _c.ChartClose(handle), cb);
    }

    internal void ChartRender_GPM(long handle, int width, int height, Action<object?, Exception?> cb)
    {
        if (_c is null) { GraphicsManager.Instance.ChartRender_GPM(handle, width, height, cb); return; }
        // Il motore C non espone il render del chart su byte[] PNG (solo CHART.SAVE
        // su schermo): errore esplicito, niente silenzio.
        cb(null, new NotSupportedException(
            "CHART.RENDER non supportato dal motore C (GMB_GFX_C=1): usare CHART.SAVE o il backend C#."));
    }

    /// <summary>CHART SHOW handle, x, y: mostra il grafico alla posizione (x,y). Con il motore C
    /// è gmb_gfx_chart_show (parity IL → IGraphicsService.ChartShow).</summary>
    internal void ChartShow_GPM(long handle, int x, int y, Action<object?, Exception?> cb)
    {
        // Motore C (default): gmb_gfx_chart_show. Motore C# legacy: nessun render a
        // schermo sincrono disponibile in PCode (completamento esplicito, niente eccezione).
        if (_c is null) { cb(null, null); return; }
        _c.RunGpm(() => _c.ChartShow(handle, x, y), cb);
    }

    // ── Matrici 3D (solo motore C#) ──
    internal void UPDATE_WORLD_MATRIX_GPM(long matrixId, Action<object?, Exception?> cb)
        => MatrixGpm("UPDATE_WORLD_MATRIX", matrixId, cb, () => GraphicsManager.Instance.UPDATE_WORLD_MATRIX_GPM(matrixId, cb));

    internal void UPDATE_VIEW_MATRIX_GPM(long matrixId, Action<object?, Exception?> cb)
        => MatrixGpm("UPDATE_VIEW_MATRIX", matrixId, cb, () => GraphicsManager.Instance.UPDATE_VIEW_MATRIX_GPM(matrixId, cb));

    internal void UPDATE_PROJECTION_MATRIX_GPM(long matrixId, Action<object?, Exception?> cb)
        => MatrixGpm("UPDATE_PROJECTION_MATRIX", matrixId, cb, () => GraphicsManager.Instance.UPDATE_PROJECTION_MATRIX_GPM(matrixId, cb));

    private void MatrixGpm(string name, long matrixId, Action<object?, Exception?> cb, Action fallback)
    {
        if (_c is null) { fallback(); return; }
        cb(null, new NotSupportedException($"{name} non supportato dal motore C (GMB_GFX_C=1)."));
    }
}
