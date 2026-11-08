// FILE: Basic.AI.Rag.IngestionBridge
// B3.1 — IngestionBridge: PdfParser → IndexTextWithMetadata + SemanticChunker; KNOWLEDGE IMPORT

using System.IO;
using System.Reflection;

namespace Basic.AI.Rag;

public static class IngestionBridge
{
    /// <summary>Importa un file (pdf, txt, md, docx via PdfParser fallback) come sorgente RAG.
    /// chunkSize 1000, overlap 100, metadata {source, fileName, importedAt}.
    /// Ritorna (chunks, indexed, sourceId) o errore.</summary>
    public static (int chunks, int indexed, string sourceId) Import(string filePath, string sourceId, string dbAlias = "kb", string collection = "docs")
    {
        if (string.IsNullOrWhiteSpace(filePath)) throw new ArgumentException("filePath richiesta", nameof(filePath));
        if (string.IsNullOrWhiteSpace(sourceId)) throw new ArgumentException("sourceId richiesta", nameof(sourceId));
        if (!File.Exists(filePath)) throw new FileNotFoundException($"File non trovato: {filePath}", filePath);

        string text;
        var ext = Path.GetExtension(filePath).ToLowerInvariant();
        if (ext == ".pdf")
        {
            // ConversionLib non è referenziabile (ciclo AI→ConversionLib→AI): chiama
            // PdfParser.Parse + DocumentConverter.ToMarkdown via reflection.
            try
            {
                var bytes = File.ReadAllBytes(filePath);
                var conv = Assembly.Load("Basic.ConversionLib");
                var parserType = conv.GetType("Basic.ConversionLib.Formats.PdfParser");
                var parse = parserType?.GetMethod("Parse", BindingFlags.Public | BindingFlags.Static);
                var doc = parse?.Invoke(null, new object[] { bytes });
                if (doc != null)
                {
                    var convType = conv.GetType("Basic.ConversionLib.DocumentConverter");
                    var toMd = convType?.GetMethod("ToMarkdown", BindingFlags.Public | BindingFlags.Static);
                    var md = toMd?.Invoke(null, new object[] { doc }) as string;
                    text = md ?? File.ReadAllText(filePath);
                }
                else text = File.ReadAllText(filePath);
            }
            catch { text = File.ReadAllText(filePath); }
        }
        else
        {
            text = File.ReadAllText(filePath);
        }

        if (string.IsNullOrWhiteSpace(text)) return (0, 0, sourceId);

        // SemanticChunker semplice: spezza per paragrafi doppi, poi per dimensione 1000 con overlap 100
        var chunks = Chunk(text, 1000, 100);
        // Apre (o riusa) la sessione RAG sotto dbAlias — riusa se l'alias è già aperto nel
        // DataSession condiviso, altrimenti registra un driver nuovo (stesso pattern BrainVault.IndexTo)
        var ragDir = Basic.AI.AiConfig.RagDefaultDir;
        if (!Directory.Exists(ragDir)) Directory.CreateDirectory(ragDir);
        RagDbSession.Open(Path.Combine(ragDir, "rag.db"), dbAlias);
        AiVectorRuntime.EnsureCollection(dbAlias, collection, Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions);
        int indexed = 0;
        for (int i = 0; i < chunks.Count; i++)
        {
            var chunkId = $"{sourceId}#c{i}";
            var meta = $"{{\"source\":\"{sourceId}\",\"file\":\"{Path.GetFileName(filePath)}\",\"chunk\":{i},\"importedAt\":\"{DateTime.UtcNow:O}\"}}";
            var res = AiVectorRuntime.IndexTextWithMetadata(dbAlias, collection, chunkId, chunks[i], meta);
            if (res is Basic.SQLite.Vector.IndexTextOutcome.Indexed or Basic.SQLite.Vector.IndexTextOutcome.Updated) indexed++;
        }
        return (chunks.Count, indexed, sourceId);
    }

    private static List<string> Chunk(string text, int size, int overlap)
    {
        var list = new List<string>();
        if (text.Length <= size) { list.Add(text); return list; }
        // Prima split per paragrafi doppi
        var paras = text.Split(new[] { "\n\n" }, StringSplitOptions.None);
        var buf = new System.Text.StringBuilder();
        foreach (var para in paras)
        {
            if (buf.Length + para.Length + 2 > size)
            {
                if (buf.Length > 0) { list.Add(buf.ToString()); buf.Clear(); }
                if (para.Length > size)
                {
                    // Split lungo paragrafo per size con overlap
                    for (int i = 0; i < para.Length; i += size - overlap)
                    {
                        var end = Math.Min(i + size, para.Length);
                        list.Add(para.Substring(i, end - i));
                        if (end == para.Length) break;
                    }
                }
                else
                {
                    buf.Append(para);
                }
            }
            else
            {
                if (buf.Length > 0) buf.Append("\n\n");
                buf.Append(para);
            }
        }
        if (buf.Length > 0) list.Add(buf.ToString());
        return list;
    }
}
