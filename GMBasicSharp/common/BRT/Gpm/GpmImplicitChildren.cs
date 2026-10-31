// FILE: Basic.Runtime.Gpm.GpmImplicitChildren.cs
// WAIT nudo (senza gruppo) attende i figli lanciati con GO SUB senza IN.
// Ogni task che lancia almeno un figlio senza IN riceve un WaitGroup implicito
// (lazy, uno per task): il figlio vi si registra (Add + Done implicito a fine
// task via il normale meccanismo task.WaitGroup) e la WAIT nuda vi si parcheggia
// con ShouldWait. Senza figli mai lanciati non esiste il gruppo → no-op, identico
// a prima. I figli lanciati con IN restano di competenza del loro gruppo esplicito.
// Condiviso da PCodeVM (OP_GO/OP_WG_WAIT) e backend IL (SpawnGoSub/WaitStep).

namespace Basic.Runtime.Gpm;

public static class GpmImplicitChildren
{
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<PCodeTask, PCodeWaitGroup> _groups = new();

    /// <summary>Registra un figlio lanciato SENZA gruppo esplicito sul gruppo
    /// implicito del genitore (creato se assente) e lo collega al figlio per il
    /// Done implicito di fine task. Ritorna il gruppo usato.</summary>
    public static PCodeWaitGroup RegisterChild(PCodeTask parent, PCodeTask child)
    {
        var g = _groups.GetValue(parent, _ => new PCodeWaitGroup());
        g.Add(1);
        child.WaitGroup = g;
        return g;
    }

    /// <summary>Gruppo implicito del task, o null se non ha mai lanciato figli
    /// senza IN (caso in cui la WAIT nuda resta no-op come prima).</summary>
    public static PCodeWaitGroup? GetGroup(PCodeTask task)
        => _groups.TryGetValue(task, out var g) ? g : null;
}
