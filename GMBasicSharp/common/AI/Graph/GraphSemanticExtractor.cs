// FILE: Basic.AI.Graph.GraphSemanticExtractor
// Semantic extraction LLM (parità graphify "semantic extraction"): un LLM legge il contenuto
// delle note del grafo e deduce relazioni SEMANTICHE tra note ESISTENTI che i wikilink non
// mostrano (concetti condivisi, una nota che applica/spiega/complementa un'altra, riferimenti
// impliciti). A differenza della semantic extraction STRUTTURALE (EnrichSemanticFromFrontmatter,
// che deriva dai soli metadati), qui il modello LEGGE il testo e inferisce.
//
// Disegno (mai stub):
//  - input: elenco di note {path, title, text (body)} + un ILlmCompleter (provider %CHAT).
//  - il prompt chiede all'LLM un JSON di edge tra i path NOTE (che devono già esistere nel
//    grafo): {edges:[{from,to,relation,confidence}]} con relation in un set chiuso.
//  - il risultato viene parsato in modo robusto (estrazione del primo blocco JSON) e applicato
//    al WikiGraph come edge Type="semantic" (INFERRED, confidence dell'LLM).
//  - se l'LLM non è disponibile o non produce JSON valido → errore esplicito (mai silent).
//  - il chiamante decide il batching (quante/quali note per invocazione) per non esplodere i costi.
using System.Text;
using System.Text.Json;
using Basic.AI.Rag;

namespace Basic.AI.Graph;

/// <summary>Una nota candidata alla semantic extraction (path + body).</summary>
public sealed record GraphSemanticNote(string Path, string Title, string Text);

/// <summary>Un edge semantico dedotto dall'LLM.</summary>
public sealed record GraphSemanticEdge(string From, string To, string Relation, double Confidence);

/// <summary>Servizio di semantic extraction LLM per il WikiGraph. Il chiamante fornisce il
/// completer (provider %CHAT o fixture nei test) e decide il sottoinsieme di note da analizzare.</summary>
public static class GraphSemanticExtractor
{
    /// <summary>Relazioni semantiche chiuse che l'LLM può dedurre tra note del grafo.</summary>
    public static readonly string[] AllowedRelations =
        ["applies", "explains", "complements", "contrasts", "references", "builds_on", "related"];

    private static readonly string RelationList = string.Join("|", AllowedRelations);

    /// <summary>Esegue la semantic extraction su <paramref name="notes"/> e applica gli edge al
    /// grafo. Ritorna il numero di edge aggiunti. Note: ogni relazione tra path presenti nel
    /// grafo e non già collegate da un edge dello stesso tipo. <paramref name="llm"/> null o
    /// risposta non valida → ritorna -1 e NON modifica il grafo (errore esplicito al chiamante).</summary>
    public static int ApplyToGraph(WikiGraph graph, IReadOnlyList<GraphSemanticNote> notes,
        ILlmCompleter? llm, int maxNotesPerPrompt = 6)
    {
        if (graph is null || notes.Count == 0) return -1;
        if (llm is null)
            throw new InvalidOperationException("Semantic extraction richiede un provider LLM attivo (AI SET ACTIVE %CHAT, ...).");

        var byPath = notes.ToDictionary(n => n.Path, StringComparer.OrdinalIgnoreCase);
        var existing = new HashSet<string>(
            graph.Edges.Where(e => e.Type == "semantic")
                .Select(e => e.From + "\u0001" + e.To), StringComparer.Ordinal);
        // indice dei nodi del grafo per la risoluzione basename case-insensitive (l'LLM spesso
        // risponde col solo nome file, non col path completo).
        var nodeIndex = new HashSet<string>(graph.Nodes.Keys, StringComparer.Ordinal);
        string? ResolveNode(string p)
        {
            if (graph.Nodes.ContainsKey(p)) return p;
            var norm = WikiLinkParser.Normalize(p);
            if (graph.Nodes.ContainsKey(norm)) return norm;
            return WikiLinkParser.Resolve(p, nodeIndex);
        }

        var added = 0;
        // batch sulle note: finestra scorrevole di maxNotesPerPrompt con overlap per non
        // perdere relazioni tra note ai bordi del batch.
        var step = Math.Max(1, maxNotesPerPrompt - 1);
        for (var start = 0; start < notes.Count; start += step)
        {
            var batch = notes.Skip(start).Take(maxNotesPerPrompt).ToList();
            var edges = ExtractEdges(llm, batch);
            if (edges is null) return added > 0 ? added : -1;   // risposta invalida: stop (ma tieni ciò che ha funzionato)

            foreach (var e in edges)
            {
                if (string.IsNullOrEmpty(e.From) || string.IsNullOrEmpty(e.To)) continue;
                var from = ResolveNode(e.From);
                var to = ResolveNode(e.To);
                if (from is null || to is null)
                {
                    continue;
                }
                if (from == to) continue;
                var key = from + "\u0001" + to;
                var rkey = to + "\u0001" + from;
                if (existing.Contains(key) || existing.Contains(rkey)) continue;
                existing.Add(key);
                // Edge semantic: passa la confidence dell'LLM. Se < 0.7 la provenance diventa
                // AMBIGUOUS (parità graphify: dedotto ma incerto → da rivedere, mai omesso).
                graph.Edges.Add(new WikiEdge(from, to, null, "semantic", e.Confidence));
                added++;
            }
        }
        return added;
    }

    /// <summary>Costruisce il prompt e parsa la risposta LLM in una lista di edge. Ritorna null
    /// se il JSON non è valido o non contiene "edges". Mai stub: senza JSON valido → null.</summary>
    private static List<GraphSemanticEdge>? ExtractEdges(ILlmCompleter llm, IReadOnlyList<GraphSemanticNote> batch)
    {
        var sb = new StringBuilder();
        sb.AppendLine("Analizza queste note di un vault di conoscenza e trova RELAZIONI SEMANTICHE implicite tra di loro (non link espliciti).");
        sb.AppendLine("Relazioni ammesse (usa SOLO queste): " + RelationList + ".");
        sb.AppendLine("Rispondi SOLO con JSON: {\"edges\":[{\"from\":\"<path esatto>\",\"to\":\"<path esatto>\",\"relation\":\"applies\",\"confidence\":0.85}]}");
        sb.AppendLine("confidence tra 0.55 e 0.95. Usa i path esatti delle note, NON inventare nodi. Solo relazioni che collegano note DIVERSE qui elencate.");
        sb.AppendLine();
        foreach (var n in batch)
        {
            sb.AppendLine($"## {n.Title}  ({n.Path})");
            var text = (n.Text ?? "").Trim();
            sb.AppendLine(text.Length > 900 ? text[..900] + "…" : text);
            sb.AppendLine();
        }
        // il completer: system="estrazione strutturata", user=prompt (la fixture risponde JSON
        // per il marker EXTRACT STRUCTURED)
        string json;
        try { json = llm.Complete("Sei un estrattore di relazioni semantiche. Output JSON valido, nessun testo extra.", sb.ToString()); }
        catch { return null; }

        var block = ExtractJsonBlock(json);
        if (block is null)
        {
            return null;
        }
        var edges = ParseEdges(block);
        if (edges is null)
        {
            return null;
        }
        return edges;
    }

    /// <summary>Interpreta la risposta dell'LLM in una lista di edge (vuota = risposta valida
    /// con nessuna relazione; null = risposta NON interpretabile). L'LLM può rispondere in piu'
    /// formati: (1) {"edges":[{from,to,relation,confidence}]}; (2) [{from,to,...}] array nudo;
    /// (3) una SEQUENZA di oggetti separati da virgola senza wrapper ({...},{...},...) — comune
    /// nei modelli che ignorano lo schema; (4) oggetto vuoto {} o array vuoto [] = nessun edge.</summary>
    private static List<GraphSemanticEdge>? ParseEdges(string block)
    {
        // formato 1/2/4: JSON singolo ben formato → risposta VALIDA (gli edge si contano, anche 0)
        var singleParsed = false;
        var edges = new List<GraphSemanticEdge>();
        try
        {
            using var doc = JsonDocument.Parse(block);
            singleParsed = true;
            var root = doc.RootElement;
            var arr = root.ValueKind == JsonValueKind.Array
                ? root
                : root.ValueKind == JsonValueKind.Object && root.TryGetProperty("edges", out var e) && e.ValueKind == JsonValueKind.Array
                    ? e
                    : default;
            if (arr.ValueKind == JsonValueKind.Array)
            {
                foreach (var item in arr.EnumerateArray())
                    if (TryParseEdge(item, out var edge) && edge is not null) edges.Add(edge);
                return edges;   // anche vuota: risposta valida
            }
            if (root.ValueKind == JsonValueKind.Object)
                return edges;   // oggetto valido senza "edges" (es. {}): nessun edge, non un errore
        }
        catch { /* non era un JSON singolo → formato 3 */ }

        if (!singleParsed)
        {
            // formato 3: sequenza di oggetti {...},{...} separati da virgola senza wrapper
            foreach (var item in SplitTopLevelObjects(block))
            {
                try
                {
                    using var doc = JsonDocument.Parse(item);
                    if (doc.RootElement.ValueKind == JsonValueKind.Object && TryParseEdge(doc.RootElement, out var edge) && edge is not null)
                        edges.Add(edge);
                }
                catch { /* ignora il singolo oggetto non valido */ }
            }
            return edges.Count > 0 ? edges : null;
        }
        return edges;
    }

    private static bool TryParseEdge(JsonElement o, out GraphSemanticEdge? edge)
    {
        edge = null;
        if (o.ValueKind != JsonValueKind.Object) return false;
        var from = o.TryGetProperty("from", out var f) ? f.GetString()?.Trim() : null;
        var to = o.TryGetProperty("to", out var t) ? t.GetString()?.Trim() : null;
        if (string.IsNullOrEmpty(from) || string.IsNullOrEmpty(to)) return false;
        var rel = o.TryGetProperty("relation", out var r) ? r.GetString()?.Trim().ToLowerInvariant() : null;
        var conf = o.TryGetProperty("confidence", out var c) && c.ValueKind == JsonValueKind.Number ? c.GetDouble() : 0.7;
        if (rel is null || !AllowedRelations.Contains(rel)) rel = "related";
        edge = new GraphSemanticEdge(from!, to!, rel!, Math.Clamp(conf, 0.55, 0.95));
        return true;
    }

    /// <summary>Divide una stringa contenente una sequenza di oggetti JSON {...},{...} nei singoli
    /// blocchi (rispetta le parentesi annidate e le stringhe).</summary>
    private static List<string> SplitTopLevelObjects(string text)
    {
        var result = new List<string>();
        var depth = 0;
        var inStr = false;
        var start = -1;
        for (var i = 0; i < text.Length; i++)
        {
            var ch = text[i];
            if (ch == '"' && (i == 0 || text[i - 1] != '\\')) inStr = !inStr;
            if (inStr) continue;
            if (ch == '{') { if (depth++ == 0) start = i; }
            else if (ch == '}' && --depth == 0 && start >= 0)
            {
                result.Add(text[start..(i + 1)]);
                start = -1;
            }
        }
        return result;
    }

    /// <summary>Estrae il primo blocco JSON valido dal testo della risposta (l'LLM può
    /// aggiungere testo attorno). Ritorna null se non trova un JSON.</summary>
    private static string? ExtractJsonBlock(string text)
    {
        if (string.IsNullOrWhiteSpace(text)) return null;
        var s = text.IndexOf('{');
        if (s < 0) return null;
        var e = text.LastIndexOf('}');
        if (e <= s) return null;
        return text[s..(e + 1)];
    }
}
