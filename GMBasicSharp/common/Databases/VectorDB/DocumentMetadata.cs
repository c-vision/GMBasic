// FILE: Basic.SQLite.Vector.DocumentMetadata
namespace Basic.SQLite.Vector;

/// <summary>Governance metadata attached to indexed documents.</summary>
public sealed record DocumentMetadata(
    string Status = "current",
    string? TenantId = null,
    string? AccessRoles = null,
    string? MetadataJson = null);
