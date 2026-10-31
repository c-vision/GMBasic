// FILE: Basic.DBF.DbfIndex.Factory.cs

using Basic.Databases;

namespace Basic.DBF;

/// <summary>
/// Factory per handle di indice DBF (.ntx, .ndx, .mdx, .cdx).
/// </summary>
internal static class IndexFactory
{
  /// <summary>
  /// Espande un path di indice in uno o più handle (MDX/CDX → un handle per tag).
  /// </summary>
  public static IReadOnlyList<IIndexHandle> CreateHandles(
      string indexPath,
      ILockManager lockManager,
      LockingStrategy lockingStrategy)
  {
      if (!File.Exists(indexPath))
      {
          return [];
      }

      try
      {
          return Path.GetExtension(indexPath).ToLowerInvariant() switch
          {
              ".ntx" => CreateNtxHandle(indexPath, lockManager, lockingStrategy) is { } h ? [h] : [],
              ".ndx" => [NdxFile.OpenExisting(indexPath, lockManager, lockingStrategy)],
              ".mdx" => MdxCompound.ReadTagTable(indexPath)
                  .Select(t => (IIndexHandle)new MdxTagIndex(indexPath, t, lockManager))
                  .ToList(),
              ".cdx" => CdxCompound.ReadTagTable(indexPath)
                  .Select(t => (IIndexHandle)new CdxTagIndex(indexPath, t, lockManager))
                  .ToList(),
              _ => [],
          };
      }
      catch
      {
          return [];
      }
  }

  /// <summary>Primo handle o null (compatibilità API esistente).</summary>
  public static IIndexHandle? Create(string indexPath, ILockManager lockManager, LockingStrategy lockingStrategy) =>
      CreateHandles(indexPath, lockManager, lockingStrategy).FirstOrDefault();

  private static IIndexHandle? CreateNtxHandle(string indexPath, ILockManager lockManager, LockingStrategy lockingStrategy)
  {
      using var fs = new FileStream(indexPath, FileMode.Open, FileAccess.Read, FileShare.Read);
      if (fs.Length < ClipperNtxHeader.PageSize)
      {
          return null;
      }

      Span<byte> typeBytes = stackalloc byte[2];
      fs.ReadExactly(typeBytes);
      var type = BitConverter.ToUInt16(typeBytes);

      if (ClipperNtxHeader.IsClipperType(type))
      {
          return lockManager != null
              ? ClipperNtxWritable.OpenExisting(indexPath, lockManager, lockingStrategy)
              : new ClipperNtxFile(indexPath);
      }

      return new NtxFile(indexPath, lockManager, lockingStrategy);
  }
}
