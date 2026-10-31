// FILE: Basic.Runtime.IServices.IMemoryService.cs

namespace Basic.Runtime;

/// <summary>Servizio memoria per il BASIC Runtime (VARPTR, STRPTR, POKE, PEEK).</summary>
public interface IMemoryService
{
    long VarPtr(long symbolId);
    long StrPtr(string? text);
    void Poke(long address, byte value);
    byte Peek(long address);
    void MemCopy(long source, long dest, long length);
    void MemSet(long address, byte value, long length);
    long Alloc(long size);
    void Free(long ptr);
}
