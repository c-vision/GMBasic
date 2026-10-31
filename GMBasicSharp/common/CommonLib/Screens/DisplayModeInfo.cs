// FILE: CommonLib.Screens.DisplayModeInfo.cs

/* 
 * DisplayModeInfo.cs: informazioni su una modalità di visualizzazione supportata dal display (larghezza, altezza, refresh, pixel format)
 * 
 * PROPRIETÀ:
 * - Width: larghezza in pixel
 * - Height: altezza in pixel
 * - RefreshRate: frequenza di aggiornamento in Hz
 * - PixelFormatName: nome del formato pixel
 * - DisplayIndex: indice del display fisico
 */

namespace CommonLib.Screens;

/// <summary>
/// Contiene informazioni dettagliate su una singola modalità di visualizzazione
/// supportata da un display, come ottenute da SDL. Usata da ScreenManager per
/// enumerare le capacità hardware del sistema. Record immutabile.
/// </summary>
public sealed record DisplayModeInfo
{
    /// <summary>Larghezza della modalità in pixel.</summary>
    public int Width { get; init; }

    /// <summary>Altezza della modalità in pixel.</summary>
    public int Height { get; init; }

    /// <summary>Frequenza di aggiornamento in Hertz (es. 60, 120, 144).</summary>
    public int RefreshRate { get; init; }

    /// <summary>Nome leggibile del formato dei pixel (es. "PIXELFORMAT_ARGB8888").</summary>
    public string PixelFormatName { get; init; } = string.Empty;

    /// <summary>Indice del display fisico su cui questa modalità è disponibile (0 = primario).</summary>
    public int DisplayIndex { get; init; }
}
