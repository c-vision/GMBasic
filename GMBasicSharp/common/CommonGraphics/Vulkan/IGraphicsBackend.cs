// GPU/IGraphicsBackend.cs — Interfaccia backend grafico

namespace CommonGraphics.Vulkan;

/// <summary>
/// Interfaccia backend grafico (Vulkan, software).
/// Il compute è gestito separatamente da ILGPU.
/// </summary>
public interface IGraphicsBackend
{
    string Name { get; }
    bool IsAvailable { get; }
    void Initialize();
    void Dispose();

    void DrawSprites(IntPtr textureId, float[] positions, float[] uvs, int count);
    void PresentFrame();
    IntPtr CreateTexture(int width, int height, byte[] rgba);
    void DestroyTexture(IntPtr textureId);
    void ClearScreen(float r, float g, float b, float a);
}

/// <summary>
/// Backend grafico nullo — usato quando nessuna GPU disponibile.
/// </summary>
internal sealed class NullGraphicsBackend : IGraphicsBackend
{
    public string Name => "None (CPU only)";
    public bool IsAvailable => true;
    public void Initialize() { }
    public void Dispose() { }
    public void DrawSprites(IntPtr textureId, float[] positions, float[] uvs, int count) { }
    public void PresentFrame() { }
    public IntPtr CreateTexture(int width, int height, byte[] rgba) => IntPtr.Zero;
    public void DestroyTexture(IntPtr textureId) { }
    public void ClearScreen(float r, float g, float b, float a) { }
}
