// FILE: Basic.SQLite.Vector.VectorMetadataJson
using System.Text.Json;

namespace Basic.SQLite.Vector;

/// <summary>Parses governance JSON from BASIC into <see cref="DocumentMetadata"/> / <see cref="VectorSearchFilter"/>.</summary>
public static class VectorMetadataJson
{
    public static DocumentMetadata ParseDocumentMetadata(string? json)
    {
        if (string.IsNullOrWhiteSpace(json))
            return new DocumentMetadata();

        try
        {
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;
            return new DocumentMetadata(
                NormalizeStatus(root.TryGetProperty("status", out var status) ? status.GetString() : null),
                root.TryGetProperty("tenant", out var tenant) ? tenant.GetString() : null,
                root.TryGetProperty("role", out var role) ? role.GetString() : null,
                json);
        }
        catch (JsonException)
        {
            return new DocumentMetadata();
        }
    }

    /// <summary>Normalizza uno `status` dal frontmatter/metadata ai SOLI valori ammessi dallo
    /// schema (`CHECK (status IN ('draft','current','deprecated'))`, SqlVectorSchema). Il
    /// frontmatter di un vault può avere status arbitrari (es. "bozza", "published", "wip"):
    /// passandoli tal quali l'INSERT falliva con `CHECK constraint failed` e il RAG build del
    /// brain si bloccava (bug 2026-09-09, AiAirlineWiki `status: bozza`). Valori non riconosciuti
    /// → "current" (default sicuro, coerente col percorso C `rag_parse_meta` che defaulta
    /// "current"). Case-insensitive.</summary>
    private static string NormalizeStatus(string? raw)
    {
        if (string.IsNullOrWhiteSpace(raw)) return "current";
        var s = raw.Trim();
        if (s.Equals("draft", StringComparison.OrdinalIgnoreCase)) return "draft";
        if (s.Equals("current", StringComparison.OrdinalIgnoreCase)) return "current";
        if (s.Equals("deprecated", StringComparison.OrdinalIgnoreCase)) return "deprecated";
        return "current";
    }

    public static VectorSearchFilter ParseSearchFilter(string? json)
    {
        if (string.IsNullOrWhiteSpace(json))
            return new VectorSearchFilter();

        try
        {
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;
            var minScore = root.TryGetProperty("minScore", out var ms) && ms.TryGetSingle(out var f) ? f : float.NegativeInfinity;
            return new VectorSearchFilter
            {
                RequiredStatus = root.TryGetProperty("status", out var status) ? status.GetString() : "current",
                TenantId = root.TryGetProperty("tenant", out var tenant) ? tenant.GetString() : null,
                RequiredRole = root.TryGetProperty("role", out var role) ? role.GetString() : null,
                ModelVersion = root.TryGetProperty("modelVersion", out var mv) ? mv.GetString() : null,
                MinScore = minScore,
                FolderPath = root.TryGetProperty("path", out var path) ? path.GetString() : null,
                RequiredTag = root.TryGetProperty("tag", out var tag) ? tag.GetString() : null,
                MinDate = root.TryGetProperty("minDate", out var mind) ? mind.GetString() : null,
                MaxDate = root.TryGetProperty("maxDate", out var maxd) ? maxd.GetString() : null,
            };
        }
        catch (JsonException)
        {
            return new VectorSearchFilter();
        }
    }
}
