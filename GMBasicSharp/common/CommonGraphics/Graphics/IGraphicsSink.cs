// FILE: CommonGraphics.Graphics.IGraphicsSink.cs
// V1.1 — sink grafico intercettabile (SDL nativo vs buffer serializzato per Canvas WebSocket)
// Il GraphicsManager resta il default (SDL); questo sink è per la webapp Canvas:
// i comandi PSET/LINE/RECT/CIRCLE/CLEAR vengono serializzati in JSON e inviati su WS.

namespace CommonGraphics.Graphics;

/// <summary>Sink per comandi grafici — implementazioni: SDL (nativo) vs WebSocket (buffer).</summary>
public interface IGraphicsSink
{
    void PSet(int x, int y, int color);
    void Line(int x1, int y1, int x2, int y2, int color);
    void Rect(int x, int y, int w, int h, int color);
    void Circle(int x, int y, int r, int color);
    void Paint(int x, int y, int fillColor, int borderColor);
    void PutImage(int x, int y, int w, int h, int[] pixels, string mode);
    int[] GetImage(int x, int y, int w, int h);
    void Clear(int color);
    void Flush();
}

/// <summary>Null sink — no-op, usato quando nessun canvas remoto è collegato.</summary>
public sealed class NullGraphicsSink : IGraphicsSink
{
    public static readonly NullGraphicsSink Instance = new();
    public void PSet(int x, int y, int color) { }
    public void Line(int x1, int y1, int x2, int y2, int color) { }
    public void Rect(int x, int y, int w, int h, int color) { }
    public void Circle(int x, int y, int r, int color) { }
    public void Paint(int x, int y, int fillColor, int borderColor) { }
    public void PutImage(int x, int y, int w, int h, int[] pixels, string mode) { }
    public int[] GetImage(int x, int y, int w, int h) => new int[2 + w * h];
    public void Clear(int color) { }
    public void Flush() { }
}

/// <summary>WebSocket sink — serializza i comandi in JSON e li invia su un WEBSOCKET.
/// Il JSON è lo stesso di include/WebCanvas.inc → webapp/static/canvas.js (editabile in BASIC+JS).</summary>
public sealed class WebSocketGraphicsSink : IGraphicsSink
{
    private readonly Func<string, bool> _send;
    private const int V = 1;
    public WebSocketGraphicsSink(Func<string, bool> send) => _send = send ?? throw new ArgumentNullException(nameof(send));
    public void PSet(int x, int y, int color) => _send($"{{\"op\":\"pset\",\"x\":{x},\"y\":{y},\"c\":{color},\"v\":{V}}}");
    public void Line(int x1, int y1, int x2, int y2, int color) => _send($"{{\"op\":\"line\",\"x1\":{x1},\"y1\":{y1},\"x2\":{x2},\"y2\":{y2},\"c\":{color},\"v\":{V}}}");
    public void Rect(int x, int y, int w, int h, int color) => _send($"{{\"op\":\"rect\",\"x\":{x},\"y\":{y},\"w\":{w},\"h\":{h},\"c\":{color},\"v\":{V}}}");
    public void Circle(int x, int y, int r, int color) => _send($"{{\"op\":\"circle\",\"x\":{x},\"y\":{y},\"r\":{r},\"c\":{color},\"v\":{V}}}");
    public void Paint(int x, int y, int fillColor, int borderColor) => _send($"{{\"op\":\"paint\",\"x\":{x},\"y\":{y},\"fc\":{fillColor},\"bc\":{borderColor},\"v\":{V}}}");
    public void PutImage(int x, int y, int w, int h, int[] pixels, string mode)
    {
        // pixels = [w,h,p0,p1,...] già incluso header? No, qui pixels è solo i dati (w*h) senza header
        // Serializziamo come array JSON compatto (per sprite 32x32 = 1k int, ~5KB JSON)
        var data = System.Text.Json.JsonSerializer.Serialize(pixels);
        _send($"{{\"op\":\"putimage\",\"x\":{x},\"y\":{y},\"w\":{w},\"h\":{h},\"data\":{data},\"mode\":\"{mode}\",\"v\":{V}}}");
    }
    public int[] GetImage(int x, int y, int w, int h)
    {
        _send($"{{\"op\":\"getimage\",\"x\":{x},\"y\":{y},\"w\":{w},\"h\":{h},\"v\":{V}}}");
        return new int[2 + w * h];
    }
    public void Clear(int color) => _send($"{{\"op\":\"clear\",\"c\":{color},\"v\":{V}}}");
    public void Flush() => _send($"{{\"op\":\"flush\",\"v\":{V}}}");
}
