// FILE: Basic.SQLite.Vector.TextChunker
using System.Text;

namespace Basic.SQLite.Vector;

/// <summary>Splits text into overlapping chunks for indexing.
/// Strategia (fix 2026-09-06, qualità RAG su libri OCR): i paragrafi corti vengono
/// ACCUMULATI in un chunk finché aggiungere il successivo non supera MaxChars — prima
/// ogni paragrafo era un chunk autonomo, quindi un PDF OCR con `\n\n` sparsi (righe
/// spezzate, intestazioni, numeri pagina) produceva migliaia di chunk-crumbs di rumore
/// (libro 725KB → 4059 chunk, 28% &lt;50 char) che inondavano il ranking RAG con embedding
/// quasi vuoti. Un paragrafo singolo &gt; MaxChars resta windowed (overlap).</summary>
public static class TextChunker
{
    public static IReadOnlyList<string> Chunk(string text, ChunkingOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(text);
        options ??= new ChunkingOptions();
        if (options.MaxChars <= 0)
            throw new ArgumentOutOfRangeException(nameof(options));

        if (text.Length == 0)
            return [];

        var paragraphs = text.Split("\n\n", StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (paragraphs.Length == 0)
            paragraphs = [text];

        var chunks = new List<string>();
        var buffer = new StringBuilder();
        foreach (var paragraph in paragraphs)
        {
            if (paragraph.Length > options.MaxChars)
            {
                Flush(chunks, buffer);                       // paragrafo gigante: chiudi l'accumulo
                AppendWindowed(chunks, paragraph, options.MaxChars, options.OverlapChars);
                continue;
            }
            // accumulo greedy: aggiungi il paragrafo al buffer se ci sta, altrimenti flush
            var sep = buffer.Length > 0 ? "\n\n" : "";
            if (buffer.Length + sep.Length + paragraph.Length > options.MaxChars)
            {
                Flush(chunks, buffer);
                sep = "";
            }
            buffer.Append(sep).Append(paragraph);
        }
        Flush(chunks, buffer);
        return chunks;
    }

    private static void Flush(List<string> chunks, StringBuilder buffer)
    {
        if (buffer.Length == 0) return;
        chunks.Add(buffer.ToString());
        buffer.Clear();
    }

    private static void AppendWindowed(List<string> chunks, string paragraph, int maxChars, int overlap)
    {
        if (paragraph.Length <= maxChars)
        {
            chunks.Add(paragraph);
            return;
        }

        var step = Math.Max(1, maxChars - overlap);
        for (var start = 0; start < paragraph.Length; start += step)
        {
            var len = Math.Min(maxChars, paragraph.Length - start);
            chunks.Add(paragraph.Substring(start, len));
            if (start + len >= paragraph.Length) break;
        }
    }
}
