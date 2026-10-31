// FILE: Basic.ISAM.IsamPageCache

namespace Basic.ISAM;

/// <summary>
/// Cache pagine 4 KiB allineate per letture B+ e metadati record (ISAM-C1).
/// ponytail: LRU 256 pagine default; upgrade path: SQLite pcache ref-count.
/// </summary>
internal sealed class IsamPageCache
{
    internal const int PageSize = IsamPageChecksum.PageSize;
    internal const int DefaultMaxPages = 256;

    private readonly LruCache<long, byte[]> _pages;

    /// <summary>Hit su <see cref="TryGetPage"/> (debug/test).</summary>
    internal int CacheHits { get; private set; }

    /// <summary>Miss su <see cref="TryGetPage"/> (debug/test).</summary>
    internal int CacheMisses { get; private set; }

    public IsamPageCache(int maxPages = DefaultMaxPages) =>
        _pages = new LruCache<long, byte[]>(maxPages);

    public bool TryGetPage(long pageOffset, long fileLength, out byte[] page)
    {
        page = null!;
        if (pageOffset < 0 || fileLength - pageOffset < PageSize)
        {
            return false;
        }

        if (_pages.TryGetValue(pageOffset, out var cached) && cached is not null)
        {
            CacheHits++;
            page = cached;
            return true;
        }

        CacheMisses++;
        page = null!;
        return false;
    }

    public void AddPage(long pageOffset, byte[] pageBytes)
    {
        if (pageBytes.Length != PageSize)
        {
            return;
        }

        _pages.AddOrUpdate(pageOffset, pageBytes);
    }

    public void InvalidatePage(long pageOffset) => _pages.Remove(pageOffset);

    public void Clear()
    {
        _pages.Clear();
        CacheHits = 0;
        CacheMisses = 0;
    }
}
