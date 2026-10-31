// FILE: Basic.Database.Native.Tests.Oracle.SpeedLedgerOracleTests

using System.Text;
using Basic.Databases.Speed;
using Basic.Ledger;

namespace Basic.Database.Native.Tests.Oracle;

public sealed class SpeedLedgerOracleTests
{
    [Fact]
    public void Speed_SetGetDelete_Roundtrip()
    {
        var dir = NativeDb.Tmp("speed_cs");
        Directory.CreateDirectory(dir);
        using var db = new global::Basic.Databases.Speed.Database(dir, crossProcessLock: false);
        var key = Encoding.UTF8.GetBytes("k1");
        var val = Encoding.UTF8.GetBytes("hello");
        db.Set(1, key, val);
        Assert.Equal(val, db.Get(1, key));
        db.Delete(1, key);
        Assert.Null(db.Get(1, key));
    }

    [Fact]
    public void Ledger_AddTransaction_Writes_Journal_Header()
    {
        var path = NativeDb.Tmp("book.dat");
        using var ctx = new LedgerContext(path);
        var id = LedgerManager.AddTransaction(ctx, new DateOnly(2026, 8, 20), "seed",
        [
            new LedgerEntry { Description = "seed", Account = "Cash", Amount = 100m, AmountIsExplicit = true },
            new LedgerEntry { Description = "seed", Account = "Equity", Amount = -100m, AmountIsExplicit = true },
        ]);
        Assert.Equal(1, id);
        var text = File.ReadAllText(path, Encoding.UTF8);
        Assert.Contains("2026-08-20 seed", text);
        Assert.Contains("Cash", text);
        Assert.Contains("Equity", text);
    }
}
