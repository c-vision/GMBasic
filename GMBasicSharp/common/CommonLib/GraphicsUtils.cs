// FILE: CommonLib.GraphicsUtils.cs

/* 
 * GraphicsUtils.cs: utility per conversioni e descrizioni di formati grafici
 * 
 * FUNZIONI:
 * - GetBppFromPixelFormat(sdlFormat): stima la profondità di colore (BPP) dal nome del formato pixel SDL
 * - GetColorDescriptionFromSdlFormat(sdlFormat): genera una descrizione leggibile dal nome del formato pixel SDL
 * - RgbToUint(r): converte un array RGB in uint ARGB
 */

namespace CommonLib;

/// <summary>Utility per conversioni e descrizioni di formati grafici.</summary>
public static class GraphicsUtils
{
    /// <summary>Stima la profondità di colore (BPP) dal nome del formato pixel SDL.</summary>
    public static int GetBppFromPixelFormat(string sdlFormat)
    {
        if (string.IsNullOrEmpty(sdlFormat)) return 0;
        if (sdlFormat.Contains("INDEX8")) return 8;
        if (sdlFormat.Contains("1555") || sdlFormat.Contains("565")) return 16;
        if (sdlFormat.Contains("8888")) return 32;
        return 24;
    }

    /// <summary>Genera una descrizione leggibile dal nome del formato pixel SDL.</summary>
    public static string GetColorDescriptionFromSdlFormat(string sdlFormat)
    {
        if (string.IsNullOrEmpty(sdlFormat)) return "N/A";
        if (sdlFormat.Contains("INDEX8")) return "256 Colors";
        if (sdlFormat.Contains("1555") || sdlFormat.Contains("565")) return "High Color (16-bit)";
        if (sdlFormat.Contains("8888")) return "True Color (32-bit)";
        if (sdlFormat.Contains("888")) return "True Color (24-bit)";
        return "Custom Color";
    }

    /// <summary>Converte un array RGB in uint ARGB (alfa=255 opaco).</summary>
    public static uint RgbToUint(byte[] r) => (uint)(255 << 24 | r[0] << 16 | r[1] << 8 | r[2]);
}
