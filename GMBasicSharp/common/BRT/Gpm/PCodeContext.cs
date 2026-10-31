// FILE: Basic.Runtime.Gpm.PCodeContext.cs
// PCODE CONTEXT — cancellazione propagata (parità Go context.Context).
// BACKGROUND = radice; WITHCANCEL = figlio derivato (registrato nel parent); CANCEL cancella
// il contesto e PROPAGA ai figli (un figlio creato dopo la cancellazione del parent nasce già
// cancellato); DONE ritorna un canale chiuso alla cancellazione (pattern worker che termina
// su ctx.Done(), parità Go). Il canale è un ChannelInfo: i waiter (RECEIVE/GO SELECT) vengono
// svegliati con ok=0 alla Close().

namespace Basic.Runtime.Gpm;

public sealed class PCodeContext
{
    private readonly object _lock = new();
    private bool _cancelled;
    private ChannelInfo? _doneChannel;
    private readonly PCodeContext? _parent;
    private readonly List<PCodeContext> _children = new();

    public PCodeContext(PCodeContext? parent = null)
    {
        _parent = parent;
        // Parità Go: un contesto derivato da un parent già cancellato nasce cancellato.
        if (parent is { _cancelled: true })
            _cancelled = true;
        else
            parent?.RegisterChild(this);
    }

    public bool IsCancelled { get { lock (_lock) return _cancelled; } }

    public ChannelInfo Done()
    {
        ChannelInfo? ch;
        lock (_lock)
        {
            _doneChannel ??= new ChannelInfo(0);
            ch = _doneChannel;
        }
        // Se già cancellato, chiudi il canale subito (parità Go: ctx.Done() su ctx cancellato
        // è già chiuso). Close() è idempotente sul flag (doppio-close → false, ignorato).
        if (IsCancelled)
            ch.Close();
        return ch;
    }

    public void Cancel()
    {
        List<PCodeContext>? toPropagate = null;
        ChannelInfo? toClose = null;
        lock (_lock)
        {
            if (_cancelled) return;
            _cancelled = true;
            toClose = _doneChannel;
            if (_children.Count > 0)
            {
                toPropagate = new List<PCodeContext>(_children);
                _children.Clear();
            }
        }
        toClose?.Close(); // sveglia i waiter su DONE (RECEIVE con ok=0)
        if (toPropagate != null)
            foreach (var child in toPropagate)
                child.Cancel();
    }

    private void RegisterChild(PCodeContext child)
    {
        lock (_lock)
        {
            if (_cancelled)
                return; // il figlio nasce già cancellato (gestito nel costruttore)
            _children.Add(child);
        }
    }
}
