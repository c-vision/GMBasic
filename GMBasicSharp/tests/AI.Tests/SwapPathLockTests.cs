// FILE: AI.Tests.SwapPathLockTests
using Basic.Agents;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class SwapPathLockTests : IDisposable
{
    private readonly string _pathPrefix = Path.Combine(Path.GetTempPath(), "gmbasic-swaplock-test-" + Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        try { File.Delete(_pathPrefix + ".lock"); } catch { /* best effort */ }
    }

    [Fact(DisplayName = "A second acquire on the same pathPrefix blocks while the first is held, then succeeds once released")]
    public void TryAcquire_SecondCallOnSamePrefix_BlocksUntilFirstReleased()
    {
        // Two INDEPENDENT FileStream handles on the same path -- the OS-level share-mode
        // enforcement SwapPathLock relies on (FileShare.None) is per-open-file-description,
        // not per-process-identity, on both Windows (per-HANDLE share modes) and Unix (.NET's
        // FileShare.None maps to flock/fcntl, which is per-open-file-description too) -- so two
        // independent opens conflicting here is the same mechanism that would make two
        // SEPARATE OS processes conflict, without needing to actually spawn a second process
        // for this specific assertion.
        using var first = SwapPathLock.TryAcquire(_pathPrefix, TimeSpan.FromSeconds(2));
        first.Should().NotBeNull("nothing else holds the lock yet");

        using var second = SwapPathLock.TryAcquire(_pathPrefix, TimeSpan.FromMilliseconds(300));
        second.Should().BeNull("the first lock is still held, so a second acquire must time out, not silently succeed");

        first!.Dispose();

        using var third = SwapPathLock.TryAcquire(_pathPrefix, TimeSpan.FromSeconds(2));
        third.Should().NotBeNull("the lock must become acquirable again once the holder releases it");
    }

    [Fact(DisplayName = "Locks on two DIFFERENT pathPrefixes never contend with each other")]
    public void TryAcquire_DifferentPrefixes_NeverContend()
    {
        var otherPrefix = _pathPrefix + "-other";
        try
        {
            using var lockA = SwapPathLock.TryAcquire(_pathPrefix, TimeSpan.FromSeconds(2));
            using var lockB = SwapPathLock.TryAcquire(otherPrefix, TimeSpan.FromSeconds(2));

            lockA.Should().NotBeNull();
            lockB.Should().NotBeNull();
        }
        finally
        {
            try { File.Delete(otherPrefix + ".lock"); } catch { /* best effort */ }
        }
    }
}
