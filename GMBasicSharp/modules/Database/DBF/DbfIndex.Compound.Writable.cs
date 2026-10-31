// FILE: Basic.DBF.DbfIndex.Compound.Writable.cs

using Basic.Databases;

namespace Basic.DBF;

/// <summary>Creates or updates tags inside MDX/CDX compound index files.</summary>
internal static class CompoundIndexFactory
{
    internal static IWritableIndexHandle CreateOrReplaceTag(
        string compoundPath,
        string tagName,
        string keyExpression,
        int keyLength,
        bool isUnique,
        string dbfBaseName,
        ILockManager lockManager,
        LockingStrategy lockingStrategy)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(compoundPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(tagName);
        ArgumentException.ThrowIfNullOrWhiteSpace(keyExpression);
        ArgumentNullException.ThrowIfNull(lockManager);

        var normalizedTag = NormalizeTagName(tagName);
        return Path.GetExtension(compoundPath).ToLowerInvariant() switch
        {
            ".mdx" => MdxTagIndex.CreateOrReplaceTag(
                compoundPath, dbfBaseName, normalizedTag, keyExpression, keyLength, isUnique, lockManager),
            ".cdx" => CdxTagIndex.CreateOrReplaceTag(
                compoundPath, normalizedTag, keyExpression, keyLength, isUnique, descending: false, lockManager),
            _ => throw new ArgumentException(
                $"Estensione indice compound non supportata: '{Path.GetExtension(compoundPath)}'. Usare .mdx o .cdx.",
                nameof(compoundPath)),
        };
    }

    internal static string NormalizeTagName(string tagName)
    {
        var trimmed = tagName.Trim();
        if (trimmed.Length == 0)
        {
            throw new ArgumentException("Il nome tag compound non può essere vuoto.", nameof(tagName));
        }

        if (trimmed.Length > CdxFormat.TagNameLength)
        {
            trimmed = trimmed[..CdxFormat.TagNameLength];
        }

        return trimmed.ToUpperInvariant();
    }
}

internal sealed partial class MdxTagIndex
{
    internal static MdxTagIndex CreateOrReplaceTag(
        string compoundPath,
        string dbfBaseName,
        string tagName,
        string keyExpression,
        int keyLength,
        bool isUnique,
        ILockManager lockManager)
    {
        var rebuilt = LoadExistingMdxTags(compoundPath, tagName);
        rebuilt.Add((tagName, keyExpression, keyLength, isUnique, []));
        MdxCompound.WriteCompound(compoundPath, dbfBaseName, rebuilt);

        var descriptor = MdxCompound.ReadTagTable(compoundPath)
            .First(t => t.Name.Equals(tagName, StringComparison.OrdinalIgnoreCase));
        return new MdxTagIndex(compoundPath, descriptor, lockManager);
    }

    private static List<(string TagName, string Expression, int KeyLength, bool Unique,
        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> Entries)> LoadExistingMdxTags(
        string compoundPath,
        string replacingTag)
    {
        if (!File.Exists(compoundPath))
        {
            return [];
        }

        var rebuilt = new List<(string, string, int, bool, IReadOnlyList<(ReadOnlyMemory<byte>, int)>)>();
        foreach (var tag in MdxCompound.ReadTagTable(compoundPath))
        {
            if (tag.Name.Equals(replacingTag, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var other = new MdxTagIndex(compoundPath, tag, null);
            rebuilt.Add((tag.Name, tag.Expression, tag.KeyLength, tag.Unique,
                other._entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
        }

        return rebuilt;
    }
}

internal sealed partial class CdxTagIndex
{
    internal static CdxTagIndex CreateOrReplaceTag(
        string compoundPath,
        string tagName,
        string keyExpression,
        int keyLength,
        bool isUnique,
        bool descending,
        ILockManager lockManager)
    {
        var rebuilt = LoadExistingCdxTags(compoundPath, tagName);
        rebuilt.Add((tagName, keyExpression, keyLength, isUnique, descending, []));
        CdxCompound.WriteCompound(compoundPath, rebuilt);

        var descriptor = CdxCompound.ReadTagTable(compoundPath)
            .First(t => t.Name.Equals(tagName, StringComparison.OrdinalIgnoreCase));
        return new CdxTagIndex(compoundPath, descriptor, lockManager);
    }

    private static List<(string TagName, string Expression, int KeyLength, bool Unique, bool Descending,
        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> Entries)> LoadExistingCdxTags(
        string compoundPath,
        string replacingTag)
    {
        if (!File.Exists(compoundPath))
        {
            return [];
        }

        var rebuilt = new List<(string, string, int, bool, bool, IReadOnlyList<(ReadOnlyMemory<byte>, int)>)>();
        foreach (var tag in CdxCompound.ReadTagTable(compoundPath))
        {
            if (tag.Name.Equals(replacingTag, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var other = new CdxTagIndex(compoundPath, tag, null);
            rebuilt.Add((tag.Name, tag.Expression, tag.KeyLength, tag.Unique, tag.Descending,
                other._entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
        }

        return rebuilt;
    }
}
