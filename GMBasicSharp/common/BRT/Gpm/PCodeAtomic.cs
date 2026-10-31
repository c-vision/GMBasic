// FILE: Basic.Runtime.Gpm.PCodeAtomic.cs
// PCODE ATOMIC — box Int64 con accesso atomico (parità Go sync/atomic).
// Interlocked.Add/Read/Exchange/CompareExchange su .NET; sicuro sotto GOMAXPROCS>1.

namespace Basic.Runtime.Gpm;

public sealed class PCodeAtomic
{
    private long _value;

    public long Add(long delta) => Interlocked.Add(ref _value, delta);

    public long Load() => Interlocked.Read(ref _value);

    public void Store(long value) => Interlocked.Exchange(ref _value, value);

    public bool CompareSwap(long expected, long replacement) =>
        Interlocked.CompareExchange(ref _value, replacement, expected) == expected;

    /// <summary>SWAP(x, newValue) — valore PRECEDENTE (parità Go atomic.SwapInt64).</summary>
    public long Swap(long replacement) => Interlocked.Exchange(ref _value, replacement);

    /// <summary>Lettura per stampa/diagnostica (non atomica per la modifica, solo lettura).</summary>
    public long Value => Interlocked.Read(ref _value);
}
