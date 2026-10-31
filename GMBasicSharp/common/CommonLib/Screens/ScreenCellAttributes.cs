// FILE: CommonLib.Screens.ScreenCellAttributes.cs

/* 
 * ScreenCellAttributes.cs: flags enum per gli attributi grafici di una cella del terminale (Bold, Underline, Inverse, Blink)
 * 
 * VALORI:
 * - None: nessun attributo
 * - Bold: grassetto / alta intensità
 * - Underline: sottolineato
 * - Inverse: video inverso
 * - Blink: lampeggiante
 */

namespace CommonLib.Screens;

/// <summary>
/// Definisce gli attributi grafici di una singola cella del terminale,
/// come grassetto, sottolineato o video inverso.
/// Flags enum — i valori possono essere combinati con OR bitwise.
/// Condiviso da tutte le implementazioni di console (SDL, System, Standard).
/// </summary>
[Flags]
public enum ScreenCellAttributes
{
    /// <summary>Nessun attributo speciale. Cella normale.</summary>
    None = 0,
    /// <summary>Carattere in grassetto o ad alta intensità. Potrebbe non avere effetto su tutte le console.</summary>
    Bold = 1,
    /// <summary>Carattere sottolineato. Potrebbe non avere effetto su tutte le console.</summary>
    Underline = 2,
    /// <summary>Colori di primo piano e sfondo invertiti (video inverso).</summary>
    Inverse = 4,
    /// <summary>Carattere lampeggiante. Potrebbe non avere effetto su tutte le console.</summary>
    Blink = 8
}
