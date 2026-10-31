// FILE: Interpreter.Consoles.TeeWriter.cs

using System.Text;

namespace CommonLib.Consoles;

/// <summary>
/// TextWriter che duplica l'output su due destinazioni.
/// Due costruttori:
/// - (TextWriter primary, StringBuilder capture): scrive su primary + cattura in StringBuilder
/// - (TextWriter a, TextWriter b): scrive su entrambi i TextWriter
/// </summary>
public sealed class TeeWriter : TextWriter
{
    private readonly TextWriter _primary;
    private readonly StringBuilder? _capture;
    private readonly StringWriter? _stringWriter;
    private readonly TextWriter? _secondary;
    private readonly bool _dualWriter;

    /// <summary>
    /// Crea un TeeWriter che scrive su <paramref name="primary"/> e cattura in <paramref name="capture"/>.
    /// </summary>
    public TeeWriter(TextWriter primary, StringBuilder capture)
    {
        _primary = primary;
        _capture = capture;
        _stringWriter = new StringWriter(capture);
        _dualWriter = false;
    }

    /// <summary>
    /// Crea un TeeWriter che scrive su ENTRAMBI i TextWriter specificati.
    /// </summary>
    public TeeWriter(TextWriter a, TextWriter b)
    {
        _primary = a;
        _secondary = b;
        _dualWriter = true;
    }

    public override Encoding Encoding =>
        _dualWriter ? (_primary.Encoding) : _primary.Encoding;

    public override void Write(char value)
    {
        _primary.Write(value);
        if (_dualWriter)
            _secondary!.Write(value);
        else
            _stringWriter!.Write(value);
    }

    public override void Write(char[] buffer, int index, int count)
    {
        _primary.Write(buffer, index, count);
        if (_dualWriter)
            _secondary!.Write(buffer, index, count);
        else
            _stringWriter!.Write(buffer, index, count);
    }

    public override void Write(string? value)
    {
        _primary.Write(value);
        if (_dualWriter)
            _secondary!.Write(value);
        else
            _stringWriter!.Write(value);
    }

    public override void WriteLine(string? value)
    {
        _primary.WriteLine(value);
        if (_dualWriter)
            _secondary!.WriteLine(value);
        else
            _stringWriter!.WriteLine(value);
    }

    public override void WriteLine()
    {
        _primary.WriteLine();
        if (_dualWriter)
            _secondary!.WriteLine();
        else
            _stringWriter!.WriteLine();
    }

    public override void Flush()
    {
        _primary.Flush();
        if (_dualWriter)
            _secondary!.Flush();
        else
            _stringWriter!.Flush();
    }

    public string GetCapturedOutput() =>
        _dualWriter ? "" : (_capture?.ToString() ?? "");

    public void ClearCapture()
    {
        if (!_dualWriter)
            _capture?.Clear();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing && !_dualWriter)
        {
            _stringWriter?.Dispose();
        }
        base.Dispose(disposing);
    }
}
