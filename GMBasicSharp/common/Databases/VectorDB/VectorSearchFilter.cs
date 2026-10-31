// FILE: Basic.SQLite.Vector.VectorSearchFilter
namespace Basic.SQLite.Vector;

/// <summary>Query-time metadata and score filters (governance).</summary>
public sealed record VectorSearchFilter
{
    public string? RequiredStatus { get; init; } = "current";

    public string? TenantId { get; init; }

    public string? RequiredRole { get; init; }

    public float MinScore { get; init; } = float.NegativeInfinity;

    /// <summary>When set, only embeddings with this model version are considered.</summary>
    public string? ModelVersion { get; init; }

    /// <summary>Prefisso del path (folder) del documento: match su SourceId che inizia con
    /// questo percorso (es. "research/ai/"). PATH_FILTER.</summary>
    public string? FolderPath { get; init; }

    /// <summary>Tag richiesto (match su metadataJson "tags" contenente il tag, senza '#').</summary>
    public string? RequiredTag { get; init; }

    /// <summary>Intervallo date (metadataJson "date", formato YYYY-MM-DD). DATE_RANGE.</summary>
    public string? MinDate { get; init; }

    public string? MaxDate { get; init; }

    public bool Matches(string status, string? tenantId, string? accessRoles, string? modelVersion = null,
        string? sourceId = null, string? metadataJson = null)
    {
        if (RequiredStatus is not null &&
            !string.Equals(status, RequiredStatus, StringComparison.OrdinalIgnoreCase))
            return false;

        if (TenantId is not null &&
            !string.Equals(tenantId, TenantId, StringComparison.OrdinalIgnoreCase))
            return false;

        if (RequiredRole is not null)
        {
            if (string.IsNullOrWhiteSpace(accessRoles)) return false;
            var roles = accessRoles.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (!roles.Any(r => string.Equals(r, RequiredRole, StringComparison.OrdinalIgnoreCase)))
                return false;
        }

        if (ModelVersion is not null &&
            !string.Equals(modelVersion, ModelVersion, StringComparison.OrdinalIgnoreCase))
            return false;

        // PATH_FILTER: sourceId inizia con il prefisso folder
        if (FolderPath is not null)
        {
            if (sourceId is null) return false;
            var norm = sourceId.Replace('\\', '/');
            if (!norm.StartsWith(FolderPath, StringComparison.OrdinalIgnoreCase)) return false;
        }

        // TAG_FILTER / DATE_RANGE: leggono i metadata JSON del chunk
        if (RequiredTag is not null || MinDate is not null || MaxDate is not null)
        {
            var (tags, date) = ExtractTagAndDate(metadataJson);
            if (RequiredTag is not null &&
                !tags.Any(t => string.Equals(t.TrimStart('#'), RequiredTag.TrimStart('#'), StringComparison.OrdinalIgnoreCase)))
                return false;
            if ((MinDate is not null || MaxDate is not null) && !string.IsNullOrWhiteSpace(date))
            {
                if (MinDate is not null &&
                    string.Compare(date, MinDate, StringComparison.Ordinal) < 0) return false;
                if (MaxDate is not null &&
                    string.Compare(date, MaxDate, StringComparison.Ordinal) > 0) return false;
            }
        }

        return true;
    }

    private static (List<string> Tags, string? Date) ExtractTagAndDate(string? metadataJson)
    {
        var tags = new List<string>();
        string? date = null;
        if (string.IsNullOrWhiteSpace(metadataJson)) return (tags, date);
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(metadataJson);
            var root = doc.RootElement;
            if (root.TryGetProperty("tags", out var t) && t.ValueKind == System.Text.Json.JsonValueKind.Array)
                foreach (var item in t.EnumerateArray())
                    if (item.ValueKind == System.Text.Json.JsonValueKind.String)
                        tags.Add(item.GetString() ?? "");
            else if (root.TryGetProperty("tags", out t) && t.ValueKind == System.Text.Json.JsonValueKind.String)
                tags.Add(t.GetString() ?? "");
            if (root.TryGetProperty("date", out var d) && d.ValueKind == System.Text.Json.JsonValueKind.String)
                date = d.GetString();
            else if (root.TryGetProperty("date", out d) && d.ValueKind == System.Text.Json.JsonValueKind.Number)
                date = d.GetRawText();
        }
        catch { }
        return (tags, date);
    }
}
