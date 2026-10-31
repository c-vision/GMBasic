// FILE: Basic.Runtime.Services.MemoryService.cs

using System.Runtime.InteropServices;

namespace Basic.Runtime.Services;

public class MemoryService : IMemoryService
{
    private readonly Dictionary<long, byte[]> _allocations = new();
    private long _nextPtr = 0x1000;

    public long VarPtr(long symbolId) => throw new NotSupportedException("VARPTR not available in BRT.");
    public long StrPtr(string? text) => throw new NotSupportedException("STRPTR not available in BRT.");

    public void Poke(long address, byte value)
    {
        if (_allocations.TryGetValue(address, out var block))
            block[0] = value;
    }

    public byte Peek(long address)
    {
        if (_allocations.TryGetValue(address, out var block))
            return block[0];
        return 0;
    }

    public void MemCopy(long source, long dest, long length) { }
    public void MemSet(long address, byte value, long length) { }

    public long Alloc(long size)
    {
        var ptr = _nextPtr++;
        _allocations[ptr] = new byte[size];
        return ptr;
    }

    public void Free(long ptr)
    {
        _allocations.Remove(ptr);
    }
}
