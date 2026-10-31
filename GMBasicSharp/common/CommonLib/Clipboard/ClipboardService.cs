// FILE: Basic.Runtime.Services.ClipboardService.cs

namespace CommonLib.Clipboard;

/// <summary>ClipboardService di base (in-memory fallback).</summary>
public class ClipboardService : IClipboardService
{
    private string? _text;

    public string? GetText() => _text;

    public void SetText(string? text)
    {
        _text = text;
    }

    public void Clear() => _text = null;
}
