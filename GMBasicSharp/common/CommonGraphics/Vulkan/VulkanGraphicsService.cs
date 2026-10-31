// GPU/VulkanGraphicsService.cs — IGraphicsService con accelerazione Vulkan per sprite

using CommonGraphics.Vulkan;
using Basic.Runtime;

namespace CommonGraphics.Vulkan;

public sealed class VulkanGraphicsService : IGraphicsService
{
    private readonly IGraphicsService _sdl;
    private readonly VulkanBackend _vk;

    // Sprite batch state
    private readonly List<SpriteInstance> _sprites = new();
    private readonly Dictionary<int, IntPtr> _textures = new(); // id → GPU handle

    public VulkanGraphicsService(IGraphicsService sdl)
    {
        _sdl = sdl;
        _vk = new VulkanBackend();
        if (_vk.IsAvailable) _vk.Initialize();
    }

    public bool IsAvailable => _vk.IsAvailable;

    // ===== SPRITE: caricamento e tracking =====

    public int LoadTexture(string path, int id)
    {
        var result = _sdl.LoadTexture(path, id);
        if (_vk.IsAvailable)
        {
            var h = _vk.CreateTexture(0, 0, []); // placeholder, TODO: real upload
            if (h != IntPtr.Zero) _textures[id] = h;
        }
        return result;
    }

    public void DrawTexture(int x, int y, int id)
    {
        if (_vk.IsAvailable && _textures.TryGetValue(id, out var h))
        {
            _sprites.Add(new SpriteInstance { TexHandle = h, X = x, Y = y, W = 64, H = 64 });
        }
        else _sdl.DrawTexture(x, y, id);
    }
    public void DrawTextureEx(int textureId, int x, int y, double rotation, double scaleX, double scaleY, int color)
        => _sdl.DrawTextureEx(textureId, x, y, rotation, scaleX, scaleY, color);

    public void DrawTextures(int x, int y, int id, int frame)
    {
        DrawTexture(x, y, id);
    }

    public void Polygon(float[] points, int color)
    {
        _sdl.Polygon(points, color);
    }

    // ===== SYNC: batch render via Vulkan =====

    public void Sync()
    {
        if (_vk.IsAvailable && _sprites.Count > 0)
        {
            var positions = new float[_sprites.Count * 4 * 2];
            var uvs = new float[_sprites.Count * 4 * 2];
            var texHandles = new IntPtr[_sprites.Count];

            for (int i = 0; i < _sprites.Count; i++)
            {
                var s = _sprites[i];
                int o = i * 8;
                positions[o + 0] = s.X; positions[o + 1] = s.Y;
                positions[o + 2] = s.X + s.W; positions[o + 3] = s.Y;
                positions[o + 4] = s.X + s.W; positions[o + 5] = s.Y + s.H;
                positions[o + 6] = s.X; positions[o + 7] = s.Y + s.H;
                uvs[o + 0] = 0; uvs[o + 1] = 0;
                uvs[o + 2] = 1; uvs[o + 3] = 0;
                uvs[o + 4] = 1; uvs[o + 5] = 1;
                uvs[o + 6] = 0; uvs[o + 7] = 1;
                texHandles[i] = s.TexHandle;
            }

            _vk.DrawSprites(texHandles[0], positions, uvs, _sprites.Count);
            _sprites.Clear();
        }
        _vk.PresentFrame();
        _sdl.Sync();
    }

    // ===== Delegate a SDL2 =====

    public void Screen(int m, int w, int h, int bpp) => _sdl.Screen(m, w, h, bpp);
    public void TextMode(int mode) => _sdl.TextMode(mode);
    public void PSet(int x, int y, int c) => _sdl.PSet(x, y, c);
    public void Preset(int x, int y) => _sdl.Preset(x, y);
    public int Point(int x, int y) => _sdl.Point(x, y);
    public void Line(int x1, int y1, int x2, int y2, int c, string? style) => _sdl.Line(x1, y1, x2, y2, c, style);
    public void Circle(int x, int y, int r, int c) => _sdl.Circle(x, y, r, c);
    public void Paint(int x, int y, int fc, int bc) => _sdl.Paint(x, y, fc, bc);
    public void Draw(string cmd) => _sdl.Draw(cmd);
    public void PCopy(int s, int d) => _sdl.PCopy(s, d);
    public void ScreenSet(int w, int h, int bpp) => _sdl.ScreenSet(w, h, bpp);
    public void Font(string n, int s) => _sdl.Font(n, s);
    public int[] ImageGet(int x1, int y1, int x2, int y2) => _sdl.ImageGet(x1, y1, x2, y2);
    public void ImagePut(int x, int y, int[] img, int op) => _sdl.ImagePut(x, y, img, op);
    public void ImagePuts(int[] spriteData, int[] positions, int op) => _sdl.ImagePuts(spriteData, positions, op);
    public void Palette(int i, int r, int g, int b) => _sdl.Palette(i, r, g, b);
    public void SaveScreen(string p) => _sdl.SaveScreen(p);
    public void RestoreScreen(string p) => _sdl.RestoreScreen(p);
    public void ViewPort(int x1, int y1, int x2, int y2) => _sdl.ViewPort(x1, y1, x2, y2);
    public void Window(int x1, int y1, int x2, int y2) => _sdl.Window(x1, y1, x2, y2);
    public void ClearRect(int x1, int y1, int x2, int y2) => _sdl.ClearRect(x1, y1, x2, y2);
    public void SetAttribute(int a) => _sdl.SetAttribute(a);
    public void CursorState(bool v) => _sdl.CursorState(v);
    public void CursorShape(int s) => _sdl.CursorShape(s);
    public void CPrint(string t) => _sdl.CPrint(t);
    public void CPrintNewLine() => _sdl.CPrintNewLine();
    public void CLocate(int l, int c) => _sdl.CLocate(l, c);
    public void CCls() => _sdl.CCls();
    public void CSaveScreen(string p) => _sdl.CSaveScreen(p);
    public void CRestoreScreen(string p) => _sdl.CRestoreScreen(p);
    public double SpriteCollide(int id1, int id2) => _sdl.SpriteCollide(id1, id2);
    public object[] SpriteCollideAll(int id) => _sdl.SpriteCollideAll(id);
    public object[] SpriteCollideBatch(float[] xs, float[] ys, float w, float h) => _sdl.SpriteCollideBatch(xs, ys, w, h);
    public int SpriteLoad(string filePath, int spriteId) => _sdl.SpriteLoad(filePath, spriteId);
    public void SpriteLoadGpm(string filePath, Action<object?, Exception?> completion) => _sdl.SpriteLoadGpm(filePath, completion);
    public void LoadTextureGpm(string path, Action<object?, Exception?> completion) => _sdl.LoadTextureGpm(path, completion);
    public void LoadTextureSheetGpm(string path, int cellW, int cellH, Action<object?, Exception?> completion)
        => _sdl.LoadTextureSheetGpm(path, cellW, cellH, completion);
    public void SyncGpm(Action<object?, Exception?> completion) => _sdl.SyncGpm(completion);
    public void PointGpm(int x, int y, Action<object?, Exception?> completion) => _sdl.PointGpm(x, y, completion);
    public void ScreenGpm(int mode, int width, int height, int bpp, Action<object?, Exception?> completion)
        => _sdl.ScreenGpm(mode, width, height, bpp, completion);
    public void SpriteGetPixelsGpm(int spriteId, Action<object?, Exception?> completion) => _sdl.SpriteGetPixelsGpm(spriteId, completion);
    public void SpriteSetTransform(int spriteId, long matrixId) => _sdl.SpriteSetTransform(spriteId, matrixId);
    public void SpriteSetMatrix(int spriteId, long matrixId) => _sdl.SpriteSetMatrix(spriteId, matrixId);
    public SpriteCollisionHit[] TakeLastCollisions() => _sdl.TakeLastCollisions();
    public void SpriteSetPosition(int spriteId, float x, float y) => _sdl.SpriteSetPosition(spriteId, x, y);
    public void SpriteSetVisible(int spriteId, bool visible) => _sdl.SpriteSetVisible(spriteId, visible);
    public void SpriteSetPriority(int spriteId, int priority) => _sdl.SpriteSetPriority(spriteId, priority);
    public void Print(string? text) => _sdl.Print(text);
    public void Locate(int col, int line) => _sdl.Locate(col, line);
    public void Color(object? foreground, object? background) => _sdl.Color(foreground, background);
    public void DrawPixelBuffer(int width, int height, object? buffer) => _sdl.DrawPixelBuffer(width, height, buffer);
    public void Clear(int? color) => _sdl.Clear(color);
    public void BSave(int x1, int y1, int x2, int y2) => _sdl.BSave(x1, y1, x2, y2);
    public void BLoad(int x, int y) => _sdl.BLoad(x, y);
    public void SetPaletteColor(int index, int r, int g, int b) => _sdl.SetPaletteColor(index, r, g, b);
    public void DrawText(string text, int x, int y, double align) => _sdl.DrawText(text, x, y, align);
    public double TextWidth(string text) => _sdl.TextWidth(text);
    public double TextHeight() => _sdl.TextHeight();
    public void GraphicsTab(int col) => _sdl.GraphicsTab(col);
    public void GraphicsTabZone() => _sdl.GraphicsTabZone();
    public void SpriteDef(int spriteId, int width, int height, int[] pixels) => _sdl.SpriteDef(spriteId, width, height, pixels);
    public int[] SpriteGetPixels(int spriteId) => _sdl.SpriteGetPixels(spriteId);
    public void TilemapLoad(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles) => _sdl.TilemapLoad(textureId, tileW, tileH, tilesPerRow, mapW, mapH, tiles);
    public void TilemapSet(int row, int col, int tileIndex) => _sdl.TilemapSet(row, col, tileIndex);
    public void TilemapSetOffset(float x, float y) => _sdl.TilemapSetOffset(x, y);
    public void TilemapScroll(float dx, float dy) => _sdl.TilemapScroll(dx, dy);
    public void TilemapDraw() => _sdl.TilemapDraw();
    public long ChartCreate(string title, string xLabel, string yLabel) => _sdl.ChartCreate(title, xLabel, yLabel);
    public void ChartPlot(long handle, double[] xs, double[] ys, string plotType, string legend) => _sdl.ChartPlot(handle, xs, ys, plotType, legend);
    public void ChartShow(long handle, int x, int y) => _sdl.ChartShow(handle, x, y);
    public void ChartClear(long handle) => _sdl.ChartClear(handle);
    public void ChartClose(long handle) => _sdl.ChartClose(handle);
    public void ChartSave(long handle, string path) => _sdl.ChartSave(handle, path);
    public void RenderTui(Basic.Runtime.TuiCell[] cells, int w, int h) => _sdl.RenderTui(cells, w, h);

    private struct SpriteInstance
    {
        public IntPtr TexHandle;
        public float X, Y, W, H;
    }
}
