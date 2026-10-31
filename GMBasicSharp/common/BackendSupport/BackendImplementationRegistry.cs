// FILE: BackendSupport/BackendImplementationRegistry.cs
//
// REGOLA PRIMARIA — ogni backend dichiara cosa implementa, il conteggio e l'audit
// sono DERIVATI dal registro, non parsati dal codice del backend (v.
// PROPOSAL-dispatch-refactor §2-3). Questo è il "registro di implementazione"
// (Opzione B) che rende `NotImplemented(backend)` una query, non un'analisi.

using System.Collections.Generic;
using System.Linq;

namespace Basic.BackendSupport;

/// <summary>Backend del linguaggio. C99Native è il compilatore C puro (zero C#).</summary>
public enum Backend
{
    IL,
    PCodeVM,
    VMC,
    C99Native,
}

/// <summary>Stato di implementazione di una superficie (TokenKind/statement/builtin).</summary>
public enum ImplementationState
{
    /// <summary>Non dichiarato: il backend non lo gestisce → errore esplicito, mai stub.</summary>
    NotImplemented,
    /// <summary>Il backend lo implementa (path completo, runtime condiviso dove applicabile).</summary>
    Implemented,
    /// <summary>Gestito ma con deviazione deliberata documentata (es. parità su PCodeVM).</summary>
    ImplementedWithDivergence,
}

/// <summary>
/// Registro condiviso dell'implementazione per backend.
/// La chiave canonica è la stringa "NS.NAME" (o "NAME" per i non-qualificati);
/// il valore dichiara lo stato. La fonte di quali chiavi ESISTONO è il registry
/// semantico di StdLib; questo registro dice solo cosa IMPLEMENTA ogni backend.
/// </summary>
public sealed partial class BackendImplementationRegistry
{
    private readonly Dictionary<Backend, Dictionary<string, ImplementationState>> _impl;

    public BackendImplementationRegistry()
    {
        _impl = new Dictionary<Backend, Dictionary<string, ImplementationState>>();
        foreach (var b in System.Enum.GetValues<Backend>())
            _impl[b] = new Dictionary<string, ImplementationState>(System.StringComparer.OrdinalIgnoreCase);
    }

    /// <summary>Dichiara una superficie come implementata per il backend.</summary>
    public BackendImplementationRegistry Register(Backend backend, string nsName,
        ImplementationState state = ImplementationState.Implemented)
    {
        _impl[backend][nsName] = state;
        return this;
    }

    public bool IsImplemented(Backend backend, string nsName) =>
        _impl[backend].TryGetValue(nsName, out var s) && s != ImplementationState.NotImplemented;

    public ImplementationState StateOf(Backend backend, string nsName) =>
        _impl[backend].TryGetValue(nsName, out var s) ? s : ImplementationState.NotImplemented;

    /// <summary>Elenco queryabile di ciò che il backend NON implementa (l'audit gratis).</summary>
    public IEnumerable<string> NotImplemented(Backend backend) =>
        _impl[backend].Where(kv => kv.Value == ImplementationState.NotImplemented)
                      .Select(kv => kv.Key)
                      .OrderBy(k => k, System.StringComparer.OrdinalIgnoreCase);

    /// <summary>Elenco queryabile di ciò che il backend IMPLEMENTA.</summary>
    public IEnumerable<string> Implemented(Backend backend) =>
        _impl[backend].Where(kv => kv.Value != ImplementationState.NotImplemented)
                      .Select(kv => kv.Key)
                      .OrderBy(k => k, System.StringComparer.OrdinalIgnoreCase);

    public int Count(Backend backend) => _impl[backend].Count;
}
