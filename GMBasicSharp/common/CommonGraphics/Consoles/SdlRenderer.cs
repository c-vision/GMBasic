// FILE: CommonGraphics.Consoles.SdlRenderer.cs

using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using SDL2;

namespace CommonGraphics.Consoles;

/// <summary>
/// Renderer SDL2 per un <see cref="ScreenBuffer"/> TUI.
/// Disegna ogni cella del buffer come glyph usando il FontAtlas SDL2,
/// con supporto per colori true-color (RGB) e palette index.
/// </summary>
public sealed class SdlRenderer : IDisposable
{
    private readonly Fonts.FontAtlas _fontAtlas;
    private readonly IntPtr _renderer;
    private readonly byte[] _defaultBgRgb;
    private readonly int _cellW;
    private readonly int _cellH;

    public SdlRenderer(IntPtr sdlRenderer, Fonts.FontAtlas fontAtlas, byte[] defaultBgRgb, int cellW, int cellH)
    {
        _renderer = sdlRenderer;
        _fontAtlas = fontAtlas ?? throw new ArgumentNullException(nameof(fontAtlas));
        _defaultBgRgb = defaultBgRgb ?? throw new ArgumentNullException(nameof(defaultBgRgb));
        _cellW = cellW;
        _cellH = cellH;
    }

    /// <summary>
    /// Renderizza l'intero <paramref name="buffer"/> sulla superficie SDL corrente.
    /// </summary>
    public void Render(ScreenBuffer buffer)
    {
        var renderer = _renderer;
        var w = buffer.Width;
        var h = buffer.Height;

        SDL.SDL_SetRenderDrawColor(renderer, _defaultBgRgb[0], _defaultBgRgb[1], _defaultBgRgb[2], 255);
        SDL.SDL_RenderClear(renderer);

        for (var y = 0; y < h; y++)
        {
            for (var x = 0; x < w; x++)
            {
                var cell = buffer.Cells[y, x];
                if (cell.Character == '\0' || cell.Character == ' ')
                    continue;

                var bgRgb = cell.BackgroundRgb;
                if (bgRgb.HasValue && !bgRgb.Value.IsDefault)
                {
                    var bgRect = new SDL.SDL_Rect
                    {
                        x = x * _cellW, y = y * _cellH, w = _cellW, h = _cellH
                    };
                    SDL.SDL_SetRenderDrawColor(renderer, bgRgb.Value.R, bgRgb.Value.G, bgRgb.Value.B, 255);
                    SDL.SDL_RenderFillRect(renderer, ref bgRect);
                }

                var fgRgb = cell.ForegroundRgb ?? CellColor.White;
                var color = new SDL.SDL_Color { r = fgRgb.R, g = fgRgb.G, b = fgRgb.B, a = 255 };
                var destRect = new SDL.SDL_Rect
                {
                    x = x * _cellW, y = y * _cellH, w = _cellW, h = _cellH
                };
                _fontAtlas.DrawChar(renderer, cell.Character, ref destRect, color);
            }
        }
    }

    public void Dispose() { }
}
