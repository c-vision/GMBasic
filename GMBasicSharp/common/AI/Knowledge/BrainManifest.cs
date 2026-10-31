// FILE: Basic.AI.Knowledge.BrainManifest
// Firma di un cervello (brain.manifest.json, §6.1ex del piano Second-Brain): formato,
// nome/versione, forme disponibili (rag/graph), SHA256 di ogni file, checksum del manifest
// stesso (calcolato escludendo il campo checksum). Letto da BrainRegistry/BrainInstaller
// per validare un cervello all'import; scritto da BrainPackager (EXPORT) e BrainSigner (SIGN).
using System.Security.Cryptography;
using System.Text.Json;

namespace Basic.AI.Knowledge;

public sealed class BrainManifest
{
    public const string FormatName = "gmbrain";
    public const int CurrentFormatVersion = 1;

    public string Format { get; set; } = FormatName;
    public int FormatVersionValue { get; set; } = CurrentFormatVersion;
    public string Name { get; set; } = "";
    public string? Version { get; set; }
    public string? Mode { get; set; }
    public string? CreatedAt { get; set; }
    public long Docs { get; set; }
    public List<string> Parsers { get; set; } = [];
    public List<string> Available { get; set; } = [];
    public Dictionary<string, string> Files { get; set; } = [];
    public string? Checksum { get; set; }
    public string? SignedBy { get; set; }
    public string? Source { get; set; }
    public Dictionary<string, string>? Probe { get; set; }

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    /// <summary>Serializza il manifest. Se <paramref name="withChecksum"/> è true, ricalcola e
    /// include il campo Checksum (hash del manifest SENZA il campo checksum, canonico all'export).</summary>
    public string ToJson(bool withChecksum = true)
    {
        if (withChecksum) Checksum = ComputeSelfChecksum();
        return JsonSerializer.Serialize(this, JsonOpts);
    }

    /// <summary>Calcola lo sha256 del manifest serializzato SENZA il campo checksum (così il
    /// checksum è auto-riferito e ri-verificabile all'import).</summary>
    public string ComputeSelfChecksum()
    {
        var without = new BrainManifest
        {
            Format = Format, FormatVersionValue = FormatVersionValue, Name = Name,
            Version = Version, Mode = Mode, CreatedAt = CreatedAt, Docs = Docs,
            Parsers = Parsers, Available = Available, Files = Files,
            SignedBy = SignedBy, Source = Source, Probe = Probe,
        };
        var json = JsonSerializer.Serialize(without, JsonOpts);
        return "sha256:" + Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(json)));
    }

    /// <summary>Verifica il checksum del manifest: il campo Checksum deve combaciare con
    /// ComputeSelfChecksum() del manifest corrente.</summary>
    public bool VerifySelfChecksum() =>
        !string.IsNullOrWhiteSpace(Checksum) && Checksum.Equals(ComputeSelfChecksum(), StringComparison.Ordinal);

    /// <summary>sha256 di un file in forma "sha256:&lt;hex&gt;".</summary>
    public static string HashFile(string path)
    {
        using var sha = SHA256.Create();
        var bytes = sha.ComputeHash(File.ReadAllBytes(path));
        return "sha256:" + Convert.ToHexString(bytes);
    }
}
