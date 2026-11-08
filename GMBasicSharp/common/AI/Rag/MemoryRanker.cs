// FILE: Basic.AI.Rag.MemoryRanker
// Second Brain — §3 Memoria semantica: ranking stile mem0 (recency/frequency/importance) sui
// fatti del brain. Estende MemoryBrain (che oggi ritorna solo similarità lessicale/cosina) con
// uno score composito: score = w_r*recency + w_f*frequency + w_i*importance, con decay
// temporale esponenziale. La frequency conta quante volte un fatto (stesso hash testo) è stato
// aggiunto; la recency decresce col tempo; l'importance è un campo opzionale.
using Microsoft.Data.Sqlite;

namespace Basic.AI.Rag;

public sealed record RankedMemory(long Id, string Kind, string Text, double Score,
    int Frequency, double Recency, double Importance);

public static class MemoryRanker
{
    public const double DefaultWRecency = 0.4;
    public const double DefaultWFrequency = 0.3;
    public const double DefaultWImportance = 0.3;
    public const double DecayLambda = 0.1; // per giorno

    /// <summary>Top-k memorie del brain, rankate con scoring mem0. Frequenza = conteggio delle
    /// occorrenze dello stesso testo; recency = exp(-lambda * giorni); importance da metadata.
    /// Formato ritorno: "id|kind|text|score|freq|recency" per riga.</summary>
    public static string SearchRanked(string query, int topK = 5, int dimensions = 0)
    {
        var rows = LoadRanked(dimensions);
        var scored = rows
            .Select(r => (r, Score: r.Score))
            .OrderByDescending(x => x.Score)
            .Take(topK)
            .ToList();
        var sb = new System.Text.StringBuilder();
        foreach (var (r, s) in scored)
            sb.AppendLine($"{r.Id}|{r.Kind}|{r.Text}|{s:F4}|{r.Frequency}|{r.Recency:F4}");
        return sb.ToString().TrimEnd();
    }

    private static List<RankedMemory> LoadRanked(int dimensions)
    {
        var dir = Path.GetDirectoryName(MemoryBrain.DbPath)!;
        if (!Directory.Exists(dir)) return [];
        using var conn = new SqliteConnection($"Data Source={MemoryBrain.DbPath};Pooling=False");
        conn.Open();
        var rows = new List<RankedMemory>();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT id, kind, text, created_at FROM memories;";
        using var r = cmd.ExecuteReader();
        while (r.Read())
        {
            var id = r.GetInt64(0);
            var kind = r.GetString(1);
            var text = r.GetString(2);
            var created = r.IsDBNull(3) ? DateTime.UtcNow : DateTime.Parse(r.GetString(3), null,
                System.Globalization.DateTimeStyles.RoundtripKind);
            var freq = FrequencyOf(text);
            var days = Math.Max(0, (DateTime.UtcNow - created).TotalDays);
            var recency = Math.Exp(-DecayLambda * days);
            var importance = ImportanceOf(text);
            var score = DefaultWRecency * recency + DefaultWFrequency * freq + DefaultWImportance * importance;
            rows.Add(new RankedMemory(id, kind, text, score, freq, recency, importance));
        }
        return rows;
    }

    /// <summary>Frequenza: quante volte lo stesso testo appare nel brain (dedup per hash).</summary>
    public static int FrequencyOf(string text)
    {
        var dir = Path.GetDirectoryName(MemoryBrain.DbPath)!;
        if (!Directory.Exists(dir)) return 1;
        using var conn = new SqliteConnection($"Data Source={MemoryBrain.DbPath};Pooling=False");
        conn.Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT COUNT(*) FROM memories WHERE text=$t;";
        cmd.Parameters.AddWithValue("$t", text);
        var n = (long)(cmd.ExecuteScalar() ?? 1L);
        return Math.Max(1, (int)n);
    }

    /// <summary>Importance: proxy deterministico — fatti dichiarativi più lunghi/puntuali contano
    /// di più; il testo con "decisione/preferenza/importante" ha peso maggiore.</summary>
    public static double ImportanceOf(string text)
    {
        if (string.IsNullOrWhiteSpace(text)) return 0;
        double baseScore = Math.Min(1, text.Length / 200.0);
        if (text.Contains("preferisce", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("decisione", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("importante", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("sempre", StringComparison.OrdinalIgnoreCase)) baseScore += 0.3;
        return Math.Min(1, baseScore);
    }

    /// <summary>Consolidazione: dedup per similarità > threshold (stesso testo normalizzato).</summary>
    public static void Consolidate(double similarityThreshold = 0.85)
    {
        var dir = Path.GetDirectoryName(MemoryBrain.DbPath)!;
        if (!Directory.Exists(dir)) return;
        using var conn = new SqliteConnection($"Data Source={MemoryBrain.DbPath};Pooling=False");
        conn.Open();
        // Gruppa per testo normalizzato (minuscolo, spazi singoli) — le voci duplicate si
        // consolidano nel primo id (la frequency la cattura MemoryRanker).
        using var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT id, text FROM memories ORDER BY id;";
        var seen = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
        var toDelete = new List<long>();
        using var r = cmd.ExecuteReader();
        while (r.Read())
        {
            var id = r.GetInt64(0);
            var text = r.GetString(1);
            var norm = string.Join(' ', text.Trim().Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries)).ToLowerInvariant();
            if (seen.TryGetValue(norm, out var first))
            {
                if (first != id) toDelete.Add(id);
            }
            else seen[norm] = id;
        }
        foreach (var id in toDelete)
        {
            using var del = conn.CreateCommand();
            del.CommandText = "DELETE FROM memories WHERE id=$id;";
            del.Parameters.AddWithValue("$id", id);
            del.ExecuteNonQuery();
        }
    }
}
