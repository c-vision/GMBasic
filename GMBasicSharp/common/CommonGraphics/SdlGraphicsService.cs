// FILE: CommonGraphics.SdlGraphicsService.cs
// Implementazione reale (SDL2) di IGraphicsService che delega a GraphicsManager
// (CommonGraphics.Graphics). Usata dal backend IL quando una finestra grafica è
// attiva (es. runner test_graphics); in headless si usa NullGraphicsService.
// Riferimento: PCodeVM di GMBasic che delega ogni SysCall grafica a
// GraphicsManager.Instance.*_GPM.

using System;
using System.Linq;
using Basic.Runtime;
using CommonLib.Screens;
using SDL2;

namespace CommonGraphics.Graphics;

/// <summary>Wrapper IGraphicsService → GraphicsManager (SDL2 reale).</summary>
public sealed class SdlGraphicsService : IGraphicsService
{
    private static GraphicsManager Gfx => GraphicsManager.Instance;
    private static void Noop(object? _r, Exception? _e) { }

    public bool IsAvailable => Gfx.IsScreenOn;

    public void Screen(int mode, int width, int height, int bpp)
        => ScreenGpm(mode, width, height, bpp, Noop);
    public void ScreenGpm(int mode, int width, int height, int bpp, Action<object?, Exception?> completion)
    {
        ScreenConfig? custom = null;
        if (width > 8 || height > 8)
        {
            var baseCfg = ScreenConfig.GetDefault((ScreenMode)mode);
            custom = baseCfg with
            {
                LogicalWidth = width > 8 ? width : baseCfg.LogicalWidth,
                LogicalHeight = height > 8 ? height : baseCfg.LogicalHeight,
                WindowWidth = width > 8 ? width : baseCfg.WindowWidth,
                WindowHeight = height > 8 ? height : baseCfg.WindowHeight,
            };
        }
        Gfx.SCREEN_GPM(mode, 0, 0, custom, ScreenWindowMode.Windowed, completion);
    }
    public void TextMode(int mode) => Gfx.TextModeGpm(mode, Noop);
    public void PSet(int x, int y, int color)
        => Gfx.PSET_GPM(x, y, color, Noop);
    public void Preset(int x, int y)
        => Gfx.PSET_GPM(x, y, null, Noop);
    public int Point(int x, int y)
    {
        throw new InvalidOperationException(
            "POINT() richiede il parcheggio GPM (IlGpmRuntime.TryPointPark). Non si attende il thread .NET.");
    }
    public void PointGpm(int x, int y, Action<object?, Exception?> completion)
        => Gfx.POINT_GPM(x, y, completion);
    public void Line(int x1, int y1, int x2, int y2, int color, string? style)
        => Gfx.LINE_GPM(x1, y1, x2, y2, color, style, Noop);
    public void Circle(int x, int y, int radius, int color)
        => Gfx.CIRCLE_GPM(x, y, radius, color, null, null, null, Noop);
    public void Paint(int x, int y, int fillColor, int borderColor)
        => Gfx.PAINT_GPM(x, y, fillColor, borderColor, Noop);
    public void Draw(string command)
        => Gfx.DRAW_GPM(command ?? "", Noop);
    public void PCopy(int srcPage, int destPage)
        => Gfx.PCOPY_GPM(srcPage, destPage, Noop);
    public void ScreenSet(int width, int height, int bpp)
        => Gfx.SCREENSET_GPM(width, height, Noop);
    public void Font(string name, int size)
    {
        // Sync (runner lineare): accoda il FONT su `_commandQueue` e ATTENDE il completamento
        // (Gfx.FONT usa _commandQueue, processata dal main loop Present/PollEvents — come
        // LOAD_TEXTURE). Così TEXTHEIGHT()/TEXTWIDTH() subito dopo FONT leggono il font
        // CORRETTO, non quello precedente (il vecchio FONT_GPM asincrono lasciava _font
        // indietro → TEXTHEIGHT sbagliato → centraggio errato col font grande).
        try { Gfx.FONT(name ?? "", size).GetAwaiter().GetResult(); }
        catch { }
    }
    public int[] ImageGet(int x1, int y1, int x2, int y2)
        => Gfx.IMAGEGET(x1, y1, x2 - x1 + 1, y2 - y1 + 1).GetAwaiter().GetResult() ?? new[] { 0, 0 };
    public void ImagePut(int x, int y, int[] img, int op)
        => Gfx.IMAGEPUT_GPM(x, y, img ?? new int[0], "PSET", Noop);
    public void ImagePuts(int[] spriteData, int[] positions, int op)
        => Gfx.IMAGEPUTS_GPM(spriteData ?? new int[0], positions ?? new int[0], "PSET", Noop);
    public void Palette(int index, int r, int g, int b)
        => Gfx.PALETTE_GPM(index, r, g, b, Noop);
    public void SaveScreen(string path)
        => Gfx.SAVE_SCREEN_GPM(path ?? "", Noop);
    public void RestoreScreen(string path)
        => Gfx.RESTORE_SCREEN_GPM(path ?? "", Noop);
    public void ViewPort(int x1, int y1, int x2, int y2)
        => Gfx.VIEW_GPM(x1, y1, x2, y2, Noop);
    public void Window(int x1, int y1, int x2, int y2)
        => Gfx.WINDOW_GPM(x1, y1, x2, y2, false, Noop);
    public void BSave(int x1, int y1, int x2, int y2)
        => Gfx.BSAVE_AREA_GPM(x1, y1, x2, y2, Noop);
    public void BLoad(int x, int y)
        => Gfx.BLOAD_AREA_GPM(x, y, Noop);
    public void ClearRect(int x1, int y1, int x2, int y2)
        => Gfx.CLS_GPM(0, Noop);
    public void DrawTexture(int x, int y, int textureId)
        => DrawTextureEx(textureId, x, y, 0, 1, 1, 0);
    public void DrawTextureEx(int textureId, int x, int y, double rotation, double scaleX, double scaleY, int color)
        => Gfx.DRAW_TEXTURE_GPM(textureId, x, y, rotation, scaleX, scaleY, color, Noop);
    public int LoadTexture(string path, int textureId)
    {
        // Sync in modalità NON-GPM (runner lineare): Gfx.LOAD_TEXTURE accoda sul thread SDL
        // e ritorna un Task<int> — il GetResult dal thread IL è sicuro finché il main loop
        // del runner pompa la coda (mai wait .NET dentro un worker GPM: lì c'è il park).
        try { return Gfx.LOAD_TEXTURE(path ?? "").GetAwaiter().GetResult(); }
        catch { return 0; }
    }
    public void LoadTextureGpm(string path, Action<object?, Exception?> completion)
        => Gfx.LOAD_TEXTURE_GPM(path ?? "", completion);
    public void LoadTextureSheetGpm(string path, int cellW, int cellH, Action<object?, Exception?> completion)
        => Gfx.LOAD_TEXTURE_SHEET_GPM(path ?? "", cellW, cellH, completion);
    public void DrawTextures(int x, int y, int textureId, int frame)
        => Gfx.DRAW_TEXTURES_GPM(textureId, new[] { (double)x }, new[] { (double)y }, new[] { 0.0 }, new[] { 1.0 }, Noop);
    public void Polygon(float[] points, int color)
    {
        var pts = new (float x, float y)[(points?.Length ?? 0) / 2];
        for (int i = 0; i + 1 < (points?.Length ?? 0); i += 2) pts[i / 2] = ((float, float))(points![i], points[i + 1]);
        Gfx.POLYGON_GPM(pts, color, Noop);
    }
    public void Sync()
        => Gfx.SYNC_GPM(Noop);
    public void SyncGpm(Action<object?, Exception?> completion)
        => Gfx.SYNC_GPM(completion);
    public void SetAttribute(int attr) { }
    public void CursorState(bool visible) => Gfx.SetTextCursorVisible(visible);
    public void CursorShape(int shape)
    {
        Gfx.SetTextCursorStyle(shape switch
        {
            1 => CommonLib.Consoles.CursorStyle.BlinkingUnderline,
            2 => CommonLib.Consoles.CursorStyle.BlinkingBar,
            3 => CommonLib.Consoles.CursorStyle.SteadyBlock,
            4 => CommonLib.Consoles.CursorStyle.SteadyUnderline,
            5 => CommonLib.Consoles.CursorStyle.SteadyBar,
            _ => CommonLib.Consoles.CursorStyle.BlinkingBlock,
        });
    }
    public void CPrint(string text) => Console.Write(text);
    public void CPrintNewLine() => Console.WriteLine();
    // CLOCATE/CCLS forzano SEMPRE la console/pagina di base (bypassano la finestra
    // attiva per costruzione: il compilatore le emette come chiamata diretta a
    // IGraphicsService, senza mai passare da IlRuntime.LocateDispatch/ClsDispatch che
    // controllano ActivePixelWindow/ActiveWindowObj) — stessa semantica di
    // Locate/Clear, non della finestra. Erano stub vuoti: CLOCATE non spostava mai il
    // cursore, CCLS non cancellava mai nulla.
    public void CLocate(int line, int col) => Gfx.LOCATE_GPM(col, line, Noop);
    public void CCls() => Gfx.CLS_GPM(null, Noop);
    public void CSaveScreen(string path) => SaveScreen(path);
    public void CRestoreScreen(string path) => RestoreScreen(path);
    // PRINT BASIC in modalità grafica: delega a GraphicsManager.PRINT_GPM (come la PCodeVM
    // di GMBasic). Il testo si ACCODA (PrintInternal avanza il cursore in X) SENZA \n:
    // il newline lo emette PrintNewLineDispatch (fine del PRINT) — altrimenti ogni PRINT
    // produceva il doppio \n (testo+\n E newline finale) → riga vuota tra le PRINT.
    public void Print(string? text)
    {
        Gfx.PRINT_GPM(text ?? "\n", Noop);
    }
    // LOCATE BASIC con schermo grafico: LOCATE_GPM(col, line) senza -1 (come la PCodeVM di gmo).
    public void Locate(int col, int line) => Gfx.LOCATE_GPM(col, line, Noop);
    // COLOR BASIC con schermo grafico: numerico → indice palette; RGBA (double[]) → colore
    // RGB completo (flag 0x40000000 per distinguerlo dagli indici). COLOR_GPM.
    public void Color(object? foreground, object? background)
        => Gfx.COLOR_GPM(ToColorIndex(foreground), ToColorIndex(background), Noop);
    private static int? ToColorIndex(object? v)
    {
        if (v == null) return null;
        if (v is double d) return (int)d;
        if (v is int i) return i;
        if (v is System.Collections.IEnumerable en)
        {
            var vals = new double[4]; var k = 0;
            foreach (var it in en) { if (k >= 4) break; vals[k++] = Convert.ToDouble(it); }
            var rgb = ((int)Math.Clamp(vals[0], 0, 255) << 16)
                    | ((int)Math.Clamp(vals[1], 0, 255) << 8)
                    | (int)Math.Clamp(vals[2], 0, 255);
            return 0x40000000 | rgb;
        }
        return null;
    }
    // PIXELBUFFER: texture da buffer di pixel (0x00RRGGBB, row-major) blittata a schermo intero.
    // I bytes vengono costruiti qui (thread VM); il blit SDL è accodato (fire-and-forget,
    // come LINE/PSET) e avviene sul main thread IN ORDINE dopo SCREEN — l'equivalente IL
    // della PCodeVM, dove tutti i SysCall grafici passavano per la coda del renderer.
    public void Clear(int? color) => Gfx.CLS_GPM(color, Noop);
    public void SetPaletteColor(int index, int r, int g, int b) => Gfx.PALETTE_GPM(index, r, g, b, Noop);
    public void DrawPixelBuffer(int width, int height, object? buffer)
    {
        if (buffer is not System.Collections.IEnumerable en || width <= 0 || height <= 0) return;
        var pixels = new int[width * height];
        var k = 0;
        foreach (var it in en) { if (k >= pixels.Length) break; pixels[k++] = (int)Convert.ToDouble(it); }
        var bytes = new byte[width * height * 4];
        for (var i = 0; i < pixels.Length; i++)
        {
            bytes[i * 4] = (byte)(pixels[i] & 0xFF);
            bytes[i * 4 + 1] = (byte)((pixels[i] >> 8) & 0xFF);
            bytes[i * 4 + 2] = (byte)((pixels[i] >> 16) & 0xFF);
            bytes[i * 4 + 3] = 255;
        }
        Gfx.EnqueuePixelBuffer(bytes, width, height);
    }
    // DRAW TEXT "testo", x, y [, ALIGN_*]: testo grafico alla posizione data (colore corrente).
    public void DrawText(string text, int x, int y, double align)
        => Gfx.DrawTextAlign_GPM(text, x, y, (int)align, Noop);
    public double TextWidth(string text) => Gfx.TextWidthInternal(text);
    public double TextHeight() => Gfx.TextHeightInternal();

    // TAB(col) / zona tab nel PRINT grafico: sposta il cursore del testo (X=col, Y invariato).
    public void GraphicsTab(int col)
        => Gfx.SetTextCursor(col, Gfx.CursorPosition.Y);
    public void GraphicsTabZone()
        => Gfx.SetTextCursor(((Gfx.CursorPosition.X / 80) + 1) * 80, Gfx.CursorPosition.Y);

    // SPRITEDEF id, w, h, pixel[] (indici palette, 0 trasparente) → sprite in memoria.
    public void SpriteDef(int spriteId, int width, int height, int[] pixels)
    {
        var rgba = new byte[(pixels?.Length ?? 0) * 4];
        for (int i = 0; i < (pixels?.Length ?? 0); i++)
        {
            var c = pixels![i] & 0xFFFFFF;
            if (c == 0) { rgba[i * 4 + 3] = 0; continue; } // trasparente
            rgba[i * 4 + 0] = (byte)(c >> 16);
            rgba[i * 4 + 1] = (byte)(c >> 8);
            rgba[i * 4 + 2] = (byte)c;
            rgba[i * 4 + 3] = 255;
        }
        Gfx.SPRITEDEF_LOAD_GPM(spriteId, width, height, rgba, Noop);
    }
    // Collisioni sprite (AABB) — delega a GraphicsManager (Collide/CollideAll/CollideBatch).
    public double SpriteCollide(int id1, int id2) => Gfx.Collide(id1, id2) ? -1 : 0;
    public object[] SpriteCollideAll(int id) => Gfx.CollideAll(id).Cast<object>().ToArray();
    public object[] SpriteCollideBatch(float[] xs, float[] ys, float w, float h)
        => Gfx.CollideBatch(xs, ys, w, h).Cast<object>().ToArray();
    // Comandi sprite → delegati ai metodi GPM di GraphicsManager (come la PCodeVM).
    public int SpriteLoad(string filePath, int spriteId)
    {
        // Sync in modalità NON-GPM: SPRITE_LOAD accoda sul thread SDL e ritorna un Task<int>
        // (mai wait .NET dentro un worker GPM — lì il park di IlGpmRuntime.TrySpriteLoadPark).
        try { return Gfx.SPRITE_LOAD(filePath ?? "").GetAwaiter().GetResult(); }
        catch { return 0; }
    }
    public void SpriteLoadGpm(string filePath, Action<object?, Exception?> completion)
        => Gfx.SPRITE_LOAD_GPM(filePath ?? "", completion);
    public void SpriteGetPixelsGpm(int spriteId, Action<object?, Exception?> completion)
        => Gfx.GetSpriteImagePixelsGpm(spriteId, completion);
    public void SpriteSetTransform(int spriteId, long matrixId)
        => Gfx.SPRITE_TRANSFORM_GPM(spriteId, matrixId, Noop);
    public void SpriteSetMatrix(int spriteId, long matrixId)
        => Gfx.SPRITE_SET_MATRIX_GPM(spriteId, matrixId, Noop);
    public SpriteCollisionHit[] TakeLastCollisions()
    {
        var raw = Gfx.TakeLastCollisions();
        var hits = new SpriteCollisionHit[raw.Count];
        for (var i = 0; i < raw.Count; i++)
        {
            var (a, b, x, y) = raw[i];
            hits[i] = new SpriteCollisionHit(a, b, x, y);
        }
        return hits;
    }
    public void SpriteSetPosition(int spriteId, float x, float y)
        => Gfx.SPRITE_SET_POSITION_GPM(spriteId, x, y, Noop);
    public void SpriteSetScale(int spriteId, float sx, float sy)
        => Gfx.SPRITE_SET_SCALE_GPM(spriteId, sx, sy, Noop);
    public void SpriteSetRotation(int spriteId, float degrees)
        => Gfx.SPRITE_SET_ROTATION_GPM(spriteId, degrees, Noop);
    public void SpriteSetVisible(int spriteId, bool visible)
    {
        if (visible) Gfx.SPRITE_ON_GPM(spriteId, Noop);
        else Gfx.SPRITE_OFF_GPM(spriteId, Noop);
    }
    public void SpriteSetPriority(int spriteId, int priority)
        => Gfx.SPRITE_PRIORITY_GPM(spriteId, priority, Noop);

    // SPRITE GETPIXELS (sincrono, schema PCodeVM): [w, h, indice_palette...].
    public int[] SpriteGetPixels(int spriteId)
    {
        var pixels = Gfx.GetSpriteImagePixels(spriteId);
        return pixels is int[] arr && arr.Length >= 2 ? arr : new[] { 0, 0 };
    }

    // ─── TILEMAP (schema PCodeVM → CommonGraphics.TilemapManager) ───
    public void TilemapLoad(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles)
        => Graphics.TilemapManager.Instance.Load(textureId, tileW, tileH, tilesPerRow, mapW, mapH, tiles);
    public void TilemapSet(int row, int col, int tileIndex)
        => Graphics.TilemapManager.Instance.SetTile(row, col, tileIndex);
    public void TilemapSetOffset(float x, float y)
        => Graphics.TilemapManager.Instance.SetOffset(x, y);
    public void TilemapScroll(float dx, float dy)
        => Graphics.TilemapManager.Instance.Scroll(dx, dy);
    public void TilemapDraw()
        => Graphics.TilemapManager.Instance.Draw();

    // ─── CHART (schema PCodeVM → CommonGraphics.ChartPlotManager) ───
    private static readonly Graphics.ChartPlotManager.PlotManager Charts = new();
    public long ChartCreate(string title, string xLabel, string yLabel)
        => Charts.CreatePlot(title, xLabel, yLabel);
    public void ChartPlot(long handle, double[] xs, double[] ys, string plotType, string legend)
        => Charts.PlotData(handle, xs, ys, plotType, legend);
    public void ChartClear(long handle)
        => Charts.ClearPlot(handle);
    public void ChartClose(long handle)
        => Charts.ClosePlot(handle);
    public void ChartShow(long handle, int x, int y)
        => ChartSave(handle, System.IO.Path.GetTempFileName() + ".bmp");
    public void ChartSave(long handle, string path)
    {
        // Render su buffer BMP in memoria e scrittura su file (il driver SDL non ha una
        // finestra chart dedicata nel backend IL — schema PCodeVM CHART.SAVE/CHART.SHOW).
        try
        {
            var bytes = Charts.GetPlotBytes(handle, 640, 480);
            System.IO.File.WriteAllBytes(path, bytes);
        }
        catch (Exception)
        {
            // grafico chiuso/senza dati: no-op documentato (come CHART.SHOW su handle invalido).
        }
    }

    public void RenderTui(Basic.Runtime.TuiCell[] cells, int w, int h)
    {
        // Fase W3: disegna le finestre TUI (WindowManager) sulla pagina grafica. Converte
        // le celle ARGB in un ScreenBuffer e accoda il rendering al main thread.
        if (cells is null || w <= 0 || h <= 0) return;
        try
        {
            var buf = new CommonLib.Consoles.Tui.ScreenBuffer(w, h);
            for (var row = 0; row < h && row < buf.Height; row++)
            {
                for (var col = 0; col < w && col < buf.Width; col++)
                {
                    var idx = row * w + col;
                    if (idx >= cells.Length) break;
                    var c = cells[idx];
                    var fg = new CommonLib.Screens.CellColor { R = (byte)((c.FgArgb >> 16) & 0xFF), G = (byte)((c.FgArgb >> 8) & 0xFF), B = (byte)(c.FgArgb & 0xFF) };
                    var bg = new CommonLib.Screens.CellColor { R = (byte)((c.BgArgb >> 16) & 0xFF), G = (byte)((c.BgArgb >> 8) & 0xFF), B = (byte)(c.BgArgb & 0xFF) };
                    buf.SetChar(col, row, c.Character == '\0' ? ' ' : c.Character, fg, bg);
                }
            }
            Gfx.RenderScreenBufferGpm(buf, Noop);
        }
        catch { }
    }
}
