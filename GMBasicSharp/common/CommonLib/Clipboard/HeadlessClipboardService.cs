// FILE: CommonLib.Clipboard.HeadlessClipboardService.cs

/* 
 * HeadlessClipboardService.cs: implementazione dummy di IClipboardService per ambienti senza GUI
 * 
 * FUNZIONI:
 * - GetText(): restituisce stringa vuota
 * - SetText(text): no-op
 * - Clear(): no-op
 */

namespace CommonLib.Clipboard;

/// <summary>Implementazione dummy di IClipboardService. Tutte le operazioni sono no-op.</summary>
public sealed class HeadlessClipboardService : IClipboardService
{
    /// <summary>Restituisce sempre stringa vuota (nessun appunti disponibile).</summary>
    /// <returns>Stringa vuota.</returns>
    public string GetText() => string.Empty;

    /// <summary>Nessuna operazione (ambiente headless).</summary>
    /// <param name="text">Testo ignorato.</param>
    public void SetText(string? text) { }

    /// <summary>Nessuna operazione (ambiente headless).</summary>
    public void Clear() { }
}
