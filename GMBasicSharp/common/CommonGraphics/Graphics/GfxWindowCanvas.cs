// FILE: CommonGraphics.Graphics.GfxWindowCanvas.cs
// Fase F1 del piano Window-Pixel-Based: primitivi grafici ad alto livello per il
// compositor di finestre PIXEL. Ogni primitivo disegna sul buffer della pagina attiva
// (nativo, flipped, con scaling _renderScale) — lo stesso meccanismo di LineInternal BF
// (ReadActivePageToBuffer/UpdateActivePageFromBuffer), che è l'unico path affidabile anche
// col renderer software (headless). Anche il testo è reso nel buffer (TTF surface → blit
// ARGB), così il compositor ha un ordine deterministico (tutto 1 buffer, 1 write-back).
//
// Convenzione coordinate: TUTTI i metodi accettano coordinate LOGICHE (0..screen-1) e
// dimensioni in pixel logici; internamente scalano per _renderScale e clippano alla pagina.
// La classe è un'estensione `partial` di GraphicsManager (nessun nuovo servizio globale).

using System.Runtime.InteropServices;
using Basic;
using SDL2;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Graphics;

public sealed partial class GraphicsManager
{
    #region Public High-Level Pixel Primitives

    /// <summary>Rettangolo con angoli arrotondati: riempito o solo bordo. rgba = 0xAARRGGBB.
    /// radius in pixel logici (0 = spigolo vivo). Gli angoli sono anti-aliasing (coverage
    /// per-pixel sulla distanza dal centro dell'arco), non un test binario: senza questo la
    /// transizione tra bordo e sfondo era un gradino netto, visibilmente diverso dalla resa
    /// di un compositor OS nativo (macOS/Windows arrotondano SEMPRE con AA).</summary>
    public void GfxRoundedRect(int x, int y, int w, int h, int radius, uint rgba, bool filled)
    {
        var (rect, bw, bh, sc, sc2) = BeginBuffer();
        if (rect.Length == 0) return;
        if (radius <= 0)
        {
            if (filled) FillRectNative(rect, bw, bh, x, y, w, h, rgba, sc2);
            else StrokeRectNative(rect, bw, bh, x, y, w, h, rgba, sc2);
            EndBuffer(rect);
            return;
        }
        var r = Math.Max(1, (int)Math.Round(radius * sc));
        var x0 = (int)Math.Round((double)(x * sc2));
        var y0 = (int)Math.Round((double)(y * sc2));
        var wN = (int)Math.Round((double)(w * sc2));
        var hN = (int)Math.Round((double)(h * sc2));
        r = Math.Min(r, Math.Max(1, wN / 2));
        r = Math.Min(r, Math.Max(1, hN / 2));

        if (filled)
        {
            FillRectNative(rect, bw, bh, x, y + radius, w, h - 2 * radius, rgba, sc2);
            FillRectNative(rect, bw, bh, x + radius, y, w - 2 * radius, h, rgba, sc2);
            FillRoundedCornersAA(rect, bw, bh, x0, y0, wN, hN, r, rgba);
        }
        else
        {
            FillRectNative(rect, bw, bh, x + radius, y, w - 2 * radius, 1, rgba, sc2);
            FillRectNative(rect, bw, bh, x + radius, y + h - 1, w - 2 * radius, 1, rgba, sc2);
            FillRectNative(rect, bw, bh, x, y + radius, 1, h - 2 * radius, rgba, sc2);
            FillRectNative(rect, bw, bh, x + w - 1, y + radius, 1, h - 2 * radius, rgba, sc2);
            StrokeRoundedCornersAA(rect, bw, bh, x0, y0, wN, hN, r, rgba);
        }
        EndBuffer(rect);
    }

    /// <summary>Cerchio pieno anti-aliasing (pulsanti titlebar in stile OS: traffic-light
    /// macOS). (cx,cy) = centro in pixel LOGICI, d = diametro in pixel LOGICI.</summary>
    public void GfxFilledCircle(int cx, int cy, int d, uint rgba)
    {
        var (rect, bw, bh, sc, sc2) = BeginBuffer();
        if (rect.Length == 0) return;
        var cxN = cx * (double)sc2;
        var cyN = cy * (double)sc2;
        var rN = d * (double)sc2 / 2.0;
        var x0 = (int)Math.Floor(cxN - rN - 1);
        var x1 = (int)Math.Ceiling(cxN + rN + 1);
        var y0 = (int)Math.Floor(cyN - rN - 1);
        var y1 = (int)Math.Ceiling(cyN + rN + 1);
        for (var py = y0; py <= y1; py++)
        for (var px = x0; px <= x1; px++)
        {
            var ddx = px + 0.5 - cxN;
            var ddy = py + 0.5 - cyN;
            var dist = Math.Sqrt(ddx * ddx + ddy * ddy);
            var coverage = Math.Clamp(rN + 0.5 - dist, 0.0, 1.0);
            if (coverage <= 0) continue;
            BlendPixelNative(rect, bw, bh, px, py, rgba, coverage);
        }
        EndBuffer(rect);
    }

    /// <summary>Riempie i 4 angoli arrotondati (bounding box nativo x0,y0,wN,hN) con coverage
    /// anti-aliasing sulla distanza dal centro dell'arco di ciascun angolo.</summary>
    private static void FillRoundedCornersAA(uint[] buf, int bw, int bh, int x0, int y0, int wN, int hN, int r, uint rgba)
    {
        Span<(int cx, int cy, int bx, int by)> corners =
        [
            (x0 + r, y0 + r, x0, y0),
            (x0 + wN - r, y0 + r, x0 + wN - r, y0),
            (x0 + r, y0 + hN - r, x0, y0 + hN - r),
            (x0 + wN - r, y0 + hN - r, x0 + wN - r, y0 + hN - r),
        ];
        foreach (var (cx, cy, bx, by) in corners)
        {
            for (var dy = 0; dy < r; dy++)
            for (var dx = 0; dx < r; dx++)
            {
                var px = bx + dx;
                var py = by + dy;
                var ddx = px + 0.5 - cx;
                var ddy = py + 0.5 - cy;
                var dist = Math.Sqrt(ddx * ddx + ddy * ddy);
                var coverage = Math.Clamp(r + 0.5 - dist, 0.0, 1.0);
                if (coverage <= 0) continue;
                BlendPixelNative(buf, bw, bh, px, py, rgba, coverage);
            }
        }
    }

    /// <summary>Traccia i 4 archi (anello di ~1px, coordinato con StrokeRectNative che disegna
    /// i lati dritti) con coverage anti-aliasing centrata sul raggio r-0.5 (continuità
    /// geometrica con le linee dritte, che passano per x0/y0 = centro pixel + 0.5).</summary>
    private static void StrokeRoundedCornersAA(uint[] buf, int bw, int bh, int x0, int y0, int wN, int hN, int r, uint rgba)
    {
        Span<(int cx, int cy, int bx, int by)> corners =
        [
            (x0 + r, y0 + r, x0, y0),
            (x0 + wN - r, y0 + r, x0 + wN - r, y0),
            (x0 + r, y0 + hN - r, x0, y0 + hN - r),
            (x0 + wN - r, y0 + hN - r, x0 + wN - r, y0 + hN - r),
        ];
        foreach (var (cx, cy, bx, by) in corners)
        {
            for (var dy = 0; dy < r; dy++)
            for (var dx = 0; dx < r; dx++)
            {
                var px = bx + dx;
                var py = by + dy;
                var ddx = px + 0.5 - cx;
                var ddy = py + 0.5 - cy;
                var dist = Math.Sqrt(ddx * ddx + ddy * ddy);
                var coverage = Math.Clamp(1.0 - Math.Abs(dist - (r - 0.5)), 0.0, 1.0);
                if (coverage <= 0) continue;
                BlendPixelNative(buf, bw, bh, px, py, rgba, coverage);
            }
        }
    }

    /// <summary>Fonde rgba (con la SUA alpha) sopra il pixel esistente, pesato da un
    /// coverage 0..1 aggiuntivo (copertura sub-pixel per l'anti-aliasing dei bordi curvi).
    /// Il buffer del compositor è sempre opaco (vedi DrawWindowText/SetPixelNative), quindi
    /// il risultato è sempre scritto con alpha piena.</summary>
    private static void BlendPixelNative(uint[] buf, int bw, int bh, int x, int y, uint rgba, double coverage)
    {
        if (x < 0 || x >= bw || y < 0 || y >= bh) return;
        var idx = PixIdx(bw, bh, x, y);
        var shapeAlpha = ((rgba >> 24) & 0xFF) / 255.0;
        var effAlpha = Math.Clamp(coverage * shapeAlpha, 0.0, 1.0);
        if (effAlpha <= 0) return;
        if (effAlpha >= 1.0) { buf[idx] = rgba | 0xFF000000u; return; }
        var bg = buf[idx];
        var bgR = (int)((bg >> 16) & 0xFF); var bgG = (int)((bg >> 8) & 0xFF); var bgB = (int)(bg & 0xFF);
        var fgR = (int)((rgba >> 16) & 0xFF); var fgG = (int)((rgba >> 8) & 0xFF); var fgB = (int)(rgba & 0xFF);
        var outR = (byte)Math.Clamp(Math.Round(fgR * effAlpha + bgR * (1 - effAlpha)), 0, 255);
        var outG = (byte)Math.Clamp(Math.Round(fgG * effAlpha + bgG * (1 - effAlpha)), 0, 255);
        var outB = (byte)Math.Clamp(Math.Round(fgB * effAlpha + bgB * (1 - effAlpha)), 0, 255);
        buf[idx] = 0xFF000000u | ((uint)outR << 16) | ((uint)outG << 8) | outB;
    }

    /// <summary>Gradiente lineare (cStart→cEnd se vertical, altrimenti da sinistra a
    /// destra) su un rettangolo. rgba in 0xAARRGGBB.</summary>
    public void GfxGradientRect(int x, int y, int w, int h, uint cStart, uint cEnd, bool vertical = true)
    {
        var (rect, bw, bh, sc, sc2) = BeginBuffer();
        if (rect.Length == 0) return;
        var x0 = (int)Math.Round((double)(x * sc2));
        var y0 = (int)Math.Round((double)(y * sc2));
        var wN = (int)Math.Round((double)(w * sc2));
        var hN = (int)Math.Round((double)(h * sc2));
        var steps = vertical ? hN : wN;
        if (steps <= 0) { EndBuffer(rect); return; }
        for (var i = 0; i <= steps; i++)
        {
            var t = (double)i / steps;
            var col = BlendRgba(cStart, cEnd, t);
            if (vertical)
                for (var fx = 0; fx < wN; fx++)
                    SetPixelNative(rect, bw, bh, x0 + fx, y0 + i, col);
            else
                for (var fy = 0; fy < hN; fy++)
                    SetPixelNative(rect, bw, bh, x0 + i, y0 + fy, col);
        }
        EndBuffer(rect);
    }

    /// <summary>Ombra morbida sotto un rettangolo: area (x+off, y+off) espansa di `radius`
    /// riempita con alpha decrescente con la distanza dal bordo (edge soft, per-pixel).
    /// shadowRgba = colore base ombra (es. 0x80000000).</summary>
    public void GfxDropShadow(int x, int y, int w, int h, int offset, uint shadowRgba, int radius = 8)
    {
        var (rect, bw, bh, sc, sc2) = BeginBuffer();
        if (rect.Length == 0) return;
        var off = Math.Max(1, (int)Math.Round((double)(offset * sc2)));
        var rad = Math.Max(2, (int)Math.Round((double)(radius * sc2)));
        var x0 = (int)Math.Round((double)(x * sc2)) + off;
        var y0 = (int)Math.Round((double)(y * sc2)) + off;
        var wN = (int)Math.Round((double)(w * sc2));
        var hN = (int)Math.Round((double)(h * sc2));
        var baseAlpha = (shadowRgba >> 24) & 0xFF;
        var rgb = shadowRgba & 0x00FFFFFFu;
        for (var yy = y0 - rad; yy < y0 + hN + rad; yy++)
        {
            if (yy < 0 || yy >= bh) continue;
            var dy = yy < y0 ? y0 - yy : (yy >= y0 + hN ? yy - (y0 + hN - 1) : 0);
            for (var xx = x0 - rad; xx < x0 + wN + rad; xx++)
            {
                if (xx < 0 || xx >= bw) continue;
                var dx = xx < x0 ? x0 - xx : (xx >= x0 + wN ? xx - (x0 + wN - 1) : 0);
                var d = Math.Sqrt((double)dx * dx + dy * dy);
                if (d >= rad) continue;
                // Smoothstep invece di falloff lineare: più pieno vicino al bordo della
                // finestra e più morbido verso l'esterno, coerente con le ombre native OS.
                var t = 1 - d / rad;
                var a = (byte)Math.Round(baseAlpha * (t * t * (3 - 2 * t)));
                if (a <= 0) continue;
                SetPixelNative(rect, bw, bh, xx, yy, (rgb & 0xFFFFFFu) | ((uint)a << 24));
            }
        }
        EndBuffer(rect);
    }

    /// <summary>Testo antialias per le finestre, disegnato NEL buffer del compositor (stessa
    /// convenzione Y diretta delle forme — verificato su Metal: le forme sono dritte). NON
    /// usa PrintInternal (che ha il proprio ciclo Read+Update, incoerente col compositor e
    /// specchiato su Metal). Si legge l'ALPHA dalla surface TTF e si fonde col colore
    /// richiesto (il byte order della surface non conta). (x,y) = posizione LOGICA.</summary>
    public void DrawWindowText(string text, int x, int y, uint rgba)
    {
        if (_font == IntPtr.Zero || string.IsNullOrEmpty(text)) return;
        var (rect, bw, bh, sc, sc2) = BeginBuffer();
        if (rect.Length == 0) return;
        var r = (byte)((rgba >> 16) & 0xFF);
        var g = (byte)((rgba >> 8) & 0xFF);
        var b = (byte)(rgba & 0xFF);
        var surface = SDL_ttf.TTF_RenderUTF8_Blended(_font, text, new SDL.SDL_Color { r = r, g = g, b = b, a = 255 });
        if (surface == IntPtr.Zero) { EndBuffer(rect); return; }
        try
        {
            var box = System.Runtime.InteropServices.Marshal.PtrToStructure<SDL.SDL_Surface>(surface);
            var wT = box.w;
            var hT = box.h;
            if (wT <= 0 || hT <= 0) return;
            var pitch = box.pitch;
            var pixelsPtr = box.pixels;
            var ttfPad = hT - SDL_ttf.TTF_FontHeight(_font);
            var x0 = (int)Math.Round((double)(x * sc2));
            var y0 = (int)Math.Round((double)(y * sc2));
            for (var ty = 0; ty < hT; ty++)
            {
                var dstY = y0 + (ty - ttfPad);
                if (dstY < 0 || dstY >= bh) continue;
                var rowPtr = pixelsPtr + ty * pitch;
                for (var tx = 0; tx < wT; tx++)
                {
                    var dstX = x0 + tx;
                    if (dstX < 0 || dstX >= bw) continue;
                    var src = System.Runtime.InteropServices.Marshal.ReadInt32(rowPtr + tx * 4);
                    var alpha = (src >> 24) & 0xFF;
                    if (alpha <= 0) continue;
                    var idx = PixIdx(bw, bh, dstX, dstY);
                    var bgPx = rect[idx];
                    var bgR = (int)((bgPx >> 16) & 0xFF);
                    var bgG = (int)((bgPx >> 8) & 0xFF);
                    var bgB = (int)(bgPx & 0xFF);
                    var inv = 255 - alpha;
                    var outR = (r * alpha + bgR * inv) / 255;
                    var outG = (g * alpha + bgG * inv) / 255;
                    var outB = (b * alpha + bgB * inv) / 255;
                    rect[idx] = 0xFF000000u | ((uint)outR << 16) | ((uint)outG << 8) | (uint)outB;
                }
            }
        }
        finally
        {
            SDL.SDL_FreeSurface(surface);
        }
        EndBuffer(rect);
    }

    /// <summary>Risolve un colore BASIC in uint ARGB 0xAARRGGBB, supportando TUTTI i formati
    /// del linguaggio (stessa convenzione di IGraphicsService.Color):
    ///   - numerico 0..255            → indice palette
    ///   - numerico con flag 0x40000000 → RGB marcato dal sistema (0x00RRGGBB)
    ///   - numerico &gt; 0xFFFFFF       → ARGB diretto (0xAARRGGBB)
    ///   - array [r,g,b] o [r,g,b,a]  → RGBA (alpha default 255)
    /// Usato dal dispatch COLOR per le finestre pixel (fg → testo/titolo, bg → sfondo).</summary>
    public uint GfxResolveColor(object? value, uint fallback)
    {
        if (value == null) return fallback;
        try
        {
            if (value is double d) return ResolveNumericColor((int)((long)d & 0xFFFFFFFF), fallback);
            if (value is uint u) return ResolveNumericColor((int)u, fallback);
            if (value is int i) return ResolveNumericColor(i, fallback);
            if (value is long l) return ResolveNumericColor((int)(l & 0xFFFFFFFF), fallback);
            if (value is System.Collections.IEnumerable en)
            {
                var vals = new double[4]; var k = 0;
                foreach (var it in en) { if (k >= 4) break; vals[k++] = System.Convert.ToDouble(it); }
                if (k == 0) return fallback;
                var r = (byte)Math.Clamp(vals[0], 0, 255);
                var g = (byte)Math.Clamp(vals[1], 0, 255);
                var b = (byte)Math.Clamp(vals[2], 0, 255);
                var a = k >= 4 ? (byte)Math.Clamp(vals[3], 0, 255) : (byte)255;
                return ((uint)a << 24) | ((uint)r << 16) | ((uint)g << 8) | b;
            }
        }
        catch { }
        return fallback;
    }

    private uint ResolveNumericColor(int value, uint fallback)
    {
        // Flag RGB marcato dal sistema (0x40000000): 0x00RRGGBB → alpha pieno.
        if ((value & 0x40000000) != 0)
            return 0xFF000000u | ((uint)value & 0x00FFFFFFu);
        // Indice palette valido (0..255).
        if (value >= 0 && value <= 255) return GetColorUint(value);
        // ARGB diretto 0xAARRGGBB (se alpha è 0 ma c'è RGB, lo si forza a pieno: un colore
        // "puro" RGB senza alpha non deve risultare invisibile).
        var argb = (uint)value;
        if (((argb >> 24) & 0xFF) == 0 && (argb & 0x00FFFFFFu) != 0)
            argb |= 0xFF000000u;
        return argb;
    }

    /// <summary>Colore palette alla risoluzione NATIVA (byte[] RGB) convertito in uint ARGB.</summary>
    private uint GetColorUint(int index)
    {
        var c = GetColor(index);
        if (c == null || c.Length < 3) return 0xFFFFFFFFu;
        return 0xFF000000u | ((uint)c[0] << 16) | ((uint)c[1] << 8) | c[2];
    }

    /// <summary>Snapshot dello sfondo "pulito" (schermo prima che qualunque finestra pixel
    /// venga mai composita) — presa alla PRIMA GfxWindowManager.Render(). Senza questo, il
    /// compositor disegna le finestre ma non ripristina mai i pixel sottostanti: spostando
    /// una finestra la vecchia posizione restava impressa (nessun redraw completo, ogni
    /// frame si accumula sul precedente — "scia" visibile).</summary>
    private uint[]? _pixelWindowBackground;

    /// <summary>Cattura lo sfondo corrente UNA SOLA VOLTA (no-op se già catturato). Va
    /// chiamato prima di comporre qualunque finestra pixel nel frame.</summary>
    public void GfxCaptureBackgroundIfNeeded()
    {
        if (_pixelWindowBackground != null) return;
        var (rect, bw, bh, _, _) = BeginBuffer();
        if (rect.Length == 0) return;
        _pixelWindowBackground = rect;
    }

    /// <summary>Ripristina lo sfondo catturato (no-op se non ancora catturato): DEVE essere
    /// chiamato a ogni frame PRIMA di ridisegnare le finestre, altrimenti i pixel della
    /// posizione/stato precedente restano impressi sullo schermo.</summary>
    public void GfxRestoreBackground()
    {
        if (_pixelWindowBackground == null) return;
        UpdateActivePageFromBuffer((uint[])_pixelWindowBackground.Clone());
    }

    #endregion

    #region Internal Pixel Helpers

    /// <summary>Legge il buffer pagina una volta (bw/bh nativi, sc = renderScale).</summary>
    private (uint[] rect, int bw, int bh, float sc, int sc2) BeginBuffer()
    {
        var rect = ReadActivePageToBuffer();
        var bw = _backingWidth > 0 ? _backingWidth : _screenWidth;
        var bh = _backingHeight > 0 ? _backingHeight : _screenHeight;
        var sc = _renderScale > 0 ? _renderScale : 1.0f;
        return (rect, bw, bh, sc, (int)Math.Round(sc));
    }

    private void EndBuffer(uint[] rect) => UpdateActivePageFromBuffer(rect);

    private static uint BlendRgba(uint a, uint b, double t)
    {
        // ATTENZIONE: i canali vanno estratti come INT — con uint, `br - ar` per br<ar fa
        // underflow a un numero enorme e il cast a byte dà 255 (gradiente verso lo scuro
        // diventava bianco). Bug trovato via probe pixel su GfxGradientRect.
        var ar = (int)((a >> 16) & 0xFF); var ag = (int)((a >> 8) & 0xFF); var ab = (int)(a & 0xFF);
        var br = (int)((b >> 16) & 0xFF); var bg = (int)((b >> 8) & 0xFF); var bb = (int)(b & 0xFF);
        var r = (byte)Math.Clamp(Math.Round(ar + (br - ar) * t), 0, 255);
        var g = (byte)Math.Clamp(Math.Round(ag + (bg - ag) * t), 0, 255);
        var bl = (byte)Math.Clamp(Math.Round(ab + (bb - ab) * t), 0, 255);
        return 0xFF000000u | ((uint)r << 16) | ((uint)g << 8) | bl;
    }

    /// <summary>Scrive un pixel nel buffer nativo (indice row-major). Il buffer tra
    /// ReadActivePageToBuffer e UpdateActivePageFromBuffer è in ordine VISIVO top-down
    /// (riga 0 = top): i flip di Read/Update si annullano → Y DIRETTA. Verificato su
    /// dummy (renderScale=1): con Y diretta il testo appare dove deve (y=70), con
    /// Y invertita in fondo (y=403). Coerente con tutte le primitive del canvas.</summary>
    /// <summary>Indice row-major di un pixel nel buffer nativo del compositor. Punto UNICO
    /// di conversione Y: la finestra reale (Present) mostrava il contenuto CAPOVOLTO con la
    /// convenzione "Y diretta" (verificato visivamente sulla finestra Cocoa/Metal reale, non
    /// via screenshot — SaveActivePageScreenshot passa dagli stessi Read/Update flip usati
    /// per disegnare ed è quindi "sempre autoconsistente" anche quando il risultato REALE è
    /// capovolto, vedi nota nel Todo Window-Pixel-Orientation-Bug.md). Un solo punto di
    /// conversione qui rende banale invertire di nuovo se servisse.</summary>
    private static int PixIdx(int bw, int bh, int x, int y) => (bh - 1 - y) * bw + x;

    private static void SetPixelNative(uint[] buf, int bw, int bh, int x, int y, uint color)
    {
        if (x < 0 || x >= bw || y < 0 || y >= bh) return;
        buf[PixIdx(bw, bh, x, y)] = color;
    }

    /// <summary>Rettangolo pieno: coordinate LOGICHE scalate e clippate al buffer nativo.</summary>
    private void FillRectNative(uint[] buf, int bw, int bh, int x, int y, int w, int h, uint color, int sc)
    {
        if (w <= 0 || h <= 0) return;
        var x0 = (int)Math.Round((double)(x * sc));
        var y0 = (int)Math.Round((double)(y * sc));
        var wN = (int)Math.Round((double)(w * sc));
        var hN = (int)Math.Round((double)(h * sc));
        for (var yy = y0; yy < y0 + hN; yy++)
            for (var xx = x0; xx < x0 + wN; xx++)
                SetPixelNative(buf, bw, bh, xx, yy, color);
    }

    /// <summary>Perimetro rettangolo: coordinate LOGICHE scalate.</summary>
    private void StrokeRectNative(uint[] buf, int bw, int bh, int x, int y, int w, int h, uint color, int sc)
    {
        if (w <= 0 || h <= 0) return;
        var x0 = (int)Math.Round((double)(x * sc));
        var y0 = (int)Math.Round((double)(y * sc));
        var wN = (int)Math.Round((double)(w * sc));
        var hN = (int)Math.Round((double)(h * sc));
        for (var xx = x0; xx < x0 + wN; xx++)
        {
            SetPixelNative(buf, bw, bh, xx, y0, color);
            SetPixelNative(buf, bw, bh, xx, y0 + hN - 1, color);
        }
        for (var yy = y0; yy < y0 + hN; yy++)
        {
            SetPixelNative(buf, bw, bh, x0, yy, color);
            SetPixelNative(buf, bw, bh, x0 + wN - 1, yy, color);
        }
    }

    #endregion
}
