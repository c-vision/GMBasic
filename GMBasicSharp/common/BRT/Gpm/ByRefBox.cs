// FILE: Basic.Runtime.Gpm.ByRefBox.cs
// BYREF PARAMETER WRAPPER — proxy per un argomento BYREF. Spostato qui da PCodeVm.cs
// (namespace Basic.PCodeVm) per renderlo accessibile a IslandRuntime e al IlBackend
// (che non dipendono da PCodeVm). Le scritture attraverso .Value raggiungono lo slot
// originale nel frame del chiamante.

namespace Basic.Runtime.Gpm;

public sealed class ByRefBox
{
    internal object?[] _frame;
    internal int _slot;
    public object? Value
    {
        get => _slot >= 0 && _slot < (_frame?.Length ?? 0) ? _frame![_slot] : null;
        set { if (_slot >= 0 && _slot < (_frame?.Length ?? 0)) _frame![_slot] = value; }
    }
    public ByRefBox(object?[] frame, int slot) { _frame = frame; _slot = slot; }
}
