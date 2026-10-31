// FILE: CommonLib.Clipboard.IClipboardService.cs

namespace CommonLib.Clipboard;

/// <summary>Definisce il contratto per un servizio che gestisce l'interazione con gli appunti di sistema.</summary>
public interface IClipboardService
{
    string? GetText();
    void SetText(string? text);
    void Clear();
}
