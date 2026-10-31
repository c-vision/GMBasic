// Session storage SQLite (RDD-SQL, 2026-09-02): BspSessionManager con persistenza durevole —
// una sessione creata e mutata sopravvive al riavvio del manager (nuova istanza, stesso file).
using Xunit;
using CommonLib.Http;
using Basic.Runtime.Services;

namespace Basic.IlBackend.Tests;

public class SqliteSessionStorageTests
{
    [Fact]
    public void Session_Persists_Across_Manager_Restart()
    {
        var path = Path.Combine(Path.GetTempPath(), "gm_sessions_" + Guid.NewGuid().ToString("N") + ".db");
        try
        {
            string sessionId;
            using (var m1 = new BspSessionManager(null, new SqliteSessionPersistence(path)))
            {
                var res = m1.GetOrCreate(null);
                Assert.NotNull(res.SetCookie);
                sessionId = res.SessionId;
                res.Data["user"] = "alice";
                m1.Flush();
            }

            using var m2 = new BspSessionManager(null, new SqliteSessionPersistence(path));
            var reloaded = m2.GetSession(sessionId);
            Assert.NotNull(reloaded);
            Assert.Equal("alice", reloaded["user"]?.GetValue<string>());
        }
        finally
        {
            try { File.Delete(path); } catch { }
            try { File.Delete(path + "-wal"); } catch { }
            try { File.Delete(path + "-shm"); } catch { }
        }
    }
}
