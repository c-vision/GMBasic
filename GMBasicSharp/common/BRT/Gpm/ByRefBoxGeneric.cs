// FILE: Basic.Runtime.Gpm.ByRefBoxGeneric.cs
// BOX GENERICO PER CAMPI __ClosureCtx (piano Closure-ByRef-Box-Linker-Definitivo, Fase 1) —
// a differenza di ByRefBox (frame/slot, proxy nello stack PCodeVM), questo tipo è un
// semplice reference-type con un campo mutabile: incapsula un riferimento write-through
// rappresentabile come CAMPO di classe in QUALSIASI backend (CLR field, C struct member via
// puntatore) — un campo non può mai essere `T&`/BYREF nudo, ma un box (reference type) sì.
// Il ByRefBox esistente (frame/slot) resta invariato per la compatibilità PCodeVM upvalue;
// questo tipo è per i campi __ClosureCtx generati dal linker (Fase 2) quando una closure
// cattura un parametro BYREF del genitore.

namespace Basic.Runtime.Gpm;

public sealed class ByRefBox<T>
{
    public T Value;

    public ByRefBox() { Value = default!; }
    public ByRefBox(T value) { Value = value; }
}
