// GPU/GpuBuffer.cs — Buffer GPU unificato

namespace Basic.IlBackend.GPU;

/// <summary>
/// Buffer GPU: contiene dati e metadati per trasferimento CPU↔GPU.
/// </summary>
public class GpuBuffer
{
    public string Name { get; }
    public int ElementCount { get; }
    public int ElementSize { get; }

    internal object? NativeHandle = null; // MemoryBuffer1D o VkBuffer

    public GpuBuffer(string name, int elementCount, int elementSize)
    {
        Name = name;
        ElementCount = elementCount;
        ElementSize = elementSize;
    }
}

/// <summary>
/// Buffer tipizzato per trasferimenti type-safe.
/// </summary>
public sealed class GpuBuffer<T> : GpuBuffer where T : unmanaged
{
    public T[]? HostData { get; set; }

    public GpuBuffer(string name, int elementCount) 
        : base(name, elementCount, System.Runtime.InteropServices.Marshal.SizeOf<T>()) { }

    public GpuBuffer(string name, T[] data) : this(name, data.Length)
    {
        HostData = data;
    }
}
