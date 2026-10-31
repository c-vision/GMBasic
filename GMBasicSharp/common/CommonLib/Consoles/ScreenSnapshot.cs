// FILE: Interpreter.Consoles.ScreenSnapshot.cs
using CommonLib.Screens;

using System.Text;

namespace CommonLib.Consoles;

/// <summary>
/// Cattura istantanea del buffer di schermo di una console in un dato momento.
/// Usata per preservare la storia visiva della console prima di comandi CLS e a fine programma.
/// </summary>
public readonly record struct ScreenSnapshot
{
    /// <summary>Etichetta descrittiva (es. "CLS (screen cleared)", "END (final screen)").</summary>
    public readonly string Label;

    /// <summary>Larghezza del buffer in colonne.</summary>
    public readonly int Width;

    /// <summary>Altezza del buffer in righe.</summary>
    public readonly int Height;

    private readonly ScreenCell[,] _cells;

    public ScreenSnapshot(string label, ScreenCell[,] cells, int width, int height)
    {
        Label = label;
        _cells = cells;
        Width = width;
        Height = height;
    }

    /// <summary>
    /// Renderizza il buffer come testo piano, riga per riga.
    /// Le righe vuote in testa/coda vengono rimosse.
    /// </summary>
    public string RenderText()
    {
        var sb = new StringBuilder(Width * Height);
        for (var r = 0; r < Height; r++)
        {
            for (var c = 0; c < Width; c++)
                sb.Append(_cells[r, c].Character);
            sb.AppendLine();
        }
        return sb.ToString().TrimEnd();
    }

    /// <summary>
    /// Renderizza il buffer preservando le righe vuote di fine schermo
    /// (utile per debug). Non trima.
    /// </summary>
    public string RenderTextRaw()
    {
        var sb = new StringBuilder(Width * Height);
        for (var r = 0; r < Height; r++)
        {
            for (var c = 0; c < Width; c++)
                sb.Append(_cells[r, c].Character);
            sb.AppendLine();
        }
        return sb.ToString();
    }

    /// <summary>
    /// Verifica se il testo specificato è contenuto in questo snapshot.
    /// </summary>
    public bool Contains(string text, StringComparison comparison = StringComparison.Ordinal)
    {
        var rendered = RenderText();
        return rendered.Contains(text, comparison);
    }

    /// <summary>
    /// Verifica se lo snapshot contiene una delle stringhe di errore BASIC note.
    /// </summary>
    public bool ContainsErrorIndicators()
    {
        var rendered = RenderText();
        return rendered.Contains("?") && (
            rendered.Contains("Errore") ||
            rendered.Contains("FATAL") ||
            rendered.Contains("RUNTIME ERROR") ||
            rendered.Contains("eccezione") ||
            rendered.Contains("Exception") ||
            rendered.Contains("ERRORE") ||
            rendered.Contains("STOP") ||
            rendered.Contains("INTERNAL ERROR")
        );
    }
}
