using CommonLib.Screens;
// FILE: Interpreter.Graphics.Tilemap.cs

using SDL2;

namespace CommonGraphics.Graphics;

public sealed class TilemapManager
{
    private static readonly Lazy<TilemapManager> _lazy = new(() => new TilemapManager());
    public static TilemapManager Instance => _lazy.Value;

    private sealed class TilemapData
    {
        public int TextureId;
        public int TileWidth;
        public int TileHeight;
        public int TilesPerRow;
        public int MapWidth;
        public int MapHeight;
        public int[] Tiles = [];
        public float OffsetX;
        public float OffsetY;
    }

    private TilemapData? _tilemap;

    private TilemapManager() { }

    public void Load(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles)
    {
        _tilemap = new TilemapData
        {
            TextureId = textureId,
            TileWidth = Math.Max(1, tileW),
            TileHeight = Math.Max(1, tileH),
            TilesPerRow = Math.Max(1, tilesPerRow),
            MapWidth = Math.Max(1, mapW),
            MapHeight = Math.Max(1, mapH),
            Tiles = tiles,
            OffsetX = 0,
            OffsetY = 0
        };
    }

    public void SetTile(int row, int col, int tileIndex)
    {
        if (_tilemap == null) return;
        if (row < 0 || row >= _tilemap.MapHeight || col < 0 || col >= _tilemap.MapWidth) return;
        _tilemap.Tiles[row * _tilemap.MapWidth + col] = Math.Max(0, tileIndex);
    }

    public void SetOffset(float x, float y)
    {
        if (_tilemap == null) return;
        _tilemap.OffsetX = x;
        _tilemap.OffsetY = y;
    }

    public void Scroll(float dx, float dy)
    {
        if (_tilemap == null) return;
        _tilemap.OffsetX += dx;
        _tilemap.OffsetY += dy;
    }

    public void Draw()
    {
        var tm = _tilemap;
        var gfx = GraphicsManager.Instance;
        var renderer = gfx.RendererHandle;
        if (tm == null || renderer == IntPtr.Zero) return;

        var texture = gfx.GetTexturePtr(tm.TextureId);
        if (texture == IntPtr.Zero) return;

        var screenW = gfx.ScreenWidth;
        var screenH = gfx.ScreenHeight;
        if (screenW <= 0 || screenH <= 0) { screenW = 640; screenH = 480; }

        var startCol = (int)Math.Floor(tm.OffsetX / tm.TileWidth);
        var startRow = (int)Math.Floor(tm.OffsetY / tm.TileHeight);
        var endCol = startCol + screenW / tm.TileWidth + 2;
        var endRow = startRow + screenH / tm.TileHeight + 2;

        startCol = Math.Max(0, startCol);
        startRow = Math.Max(0, startRow);
        endCol = Math.Min(tm.MapWidth, endCol);
        endRow = Math.Min(tm.MapHeight, endRow);

        var srcRect = new SDL.SDL_Rect();
        var destRect = new SDL.SDL_Rect();

        for (var row = startRow; row < endRow; row++)
        {
            for (var col = startCol; col < endCol; col++)
            {
                var tileIdx = tm.Tiles[row * tm.MapWidth + col];
                if (tileIdx < 0) continue;

                var tileCol = tileIdx % tm.TilesPerRow;
                var tileRow = tileIdx / tm.TilesPerRow;

                srcRect.x = tileCol * tm.TileWidth;
                srcRect.y = tileRow * tm.TileHeight;
                srcRect.w = tm.TileWidth;
                srcRect.h = tm.TileHeight;

                destRect.x = (int)(col * tm.TileWidth - tm.OffsetX);
                destRect.y = (int)(row * tm.TileHeight - tm.OffsetY);
                destRect.w = tm.TileWidth;
                destRect.h = tm.TileHeight;

                SDL.SDL_RenderCopy(renderer, texture, ref srcRect, ref destRect);
            }
        }
    }
}
