// FILE: CommonLib.TeeTextWriter.cs

using System.Text;

namespace CommonLib;

/// <summary>
/// TextWriter che scrive su DUE destinazioni: una di CATTURA (StringWriter, per il valore di
/// ritorno dei runner) e una LIVE (la console reale).
///
/// PERCHÉ ESISTE. I runner (PCode, IL in-process) catturavano TUTTO l'output del programma in
/// uno StringWriter e lo stampavano solo alla fine: va bene per un test, è sbagliato per un
/// processo LUNGO (server/daemon): l'utente non vede nulla (e `tools/sh/webapp_up.sh logs`
/// restava vuoto) e la memoria cresce senza limite. Con il tee l'output è live e la cattura
/// resta identica.
/// </summary>
public sealed class TeeTextWriter : TextWriter
{
    private readonly TextWriter _capture;
    private readonly TextWriter? _live;
    private readonly object _gate = new();

    public TeeTextWriter(TextWriter capture, TextWriter? live)
    {
        _capture = capture ?? throw new ArgumentNullException(nameof(capture));
        _live = live;
    }

    public override Encoding Encoding => _capture.Encoding;

    public override void Write(char value)
    {
        lock (_gate) { _capture.Write(value); _live?.Write(value); }
    }

    public override void Write(string? value)
    {
        lock (_gate) { _capture.Write(value); _live?.Write(value); }
    }

    public override void WriteLine(string? value)
    {
        lock (_gate) { _capture.WriteLine(value); _live?.WriteLine(value); }
    }

    public override void WriteLine()
    {
        lock (_gate) { _capture.WriteLine(); _live?.WriteLine(); }
    }

    public override void Flush()
    {
        lock (_gate) { _capture.Flush(); _live?.Flush(); }
    }
}
