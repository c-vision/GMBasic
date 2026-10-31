// FILE: CommonLib.Screens.ScreenMode.cs

/* 
 * ScreenMode.cs: enum che definisce le modalità grafiche standard supportate dal runtime BASIC
 * 
 * VALORI:
 * - Text: modalità testo puro (100x40 caratteri)
 * - CGA: 320x200 pixel, 16 colori
 * - EGA: 640x350 pixel, 16 colori
 * - VGA: 640x480 pixel, 16 colori
 * - SVGA: 320x200 pixel, 256 colori
 * - XGA: 640x480 pixel, 256 colori
 * - C64: Commodore 64 (80x50 caratteri, palette PETSCII)
 * - CRT: 320x200 pixel + 40x25 caratteri
 */

namespace CommonLib.Screens;

/// <summary>
/// Definisce le modalità grafiche standard supportate dal runtime BASIC.
/// Ogni modalità corrisponde a una risoluzione e profondità colore specifica,
/// richiamabile da BASIC tramite il comando SCREEN.
/// I valori numerici corrispondono a quelli storici del BASIC (es. SCREEN 12 = VGA).
/// </summary>
public enum ScreenMode
{
    /// <summary>Modalità testo puro (100x40 caratteri). Nessuna area grafica.</summary>
    Text = 0,
    /// <summary>Modalità CGA 320x200 pixel, 16 colori.</summary>
    CGA = 1,
    /// <summary>Modalità EGA 640x350 pixel, 16 colori.</summary>
    EGA = 9,
    /// <summary>Modalità VGA 640x480 pixel, 16 colori, griglia 80x30 caratteri.</summary>
    VGA = 12,
    /// <summary>Modalità SVGA 320x200 pixel, 256 colori.</summary>
    SVGA = 13,
    /// <summary>Modalità XGA 640x480 pixel, 256 colori.</summary>
    XGA = 16,
    /// <summary>Modalità Commodore 64: 80x50 caratteri, palette C64 classica, font PETSCII.</summary>
    C64 = 64,
    /// <summary>Modalità CRT: 320x200 pixel + 40x25 caratteri, palette CRT slavata.</summary>
    CRT = 65
}
