// Verifica che i comandi RDD scoped (DELETE/RECALL/REPLACE con clausole
// ALL/NEXT/FOR/WHILE) funzionino su qualunque driver RDD (community e commerciale):
// il motore CoreDb.ScopedOperation è driver-agnostico (usa solo IDatabaseDriver) e
// le condizioni FOR/WHILE sono valutate per-record da RuntimeExpressionEvaluator.
using System.Collections.Generic;
using System.IO;
using Basic.Databases.Rdd;
using Basic.IlBackend;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class RddScopeCommandsTests
{
    private const int NomeId = 10;
    private const int SaldoId = 11;

    private static ArrayDriver OpenArray(DataSession session, string alias)
    {
        var driver = new ArrayDriver();
        driver.SetFields([
            new RddFieldInfo("Nome", new StringType(32), 32, 0, NomeId),
            new RddFieldInfo("Saldo", new DoubleType(), 8, 1, SaldoId),
        ]);
        session.Open(driver, new DbOpenInfo { FileName = ":memory:", Alias = alias, Driver = "ARRAY" });
        return driver;
    }

    private static void Seed(DataSession session, int area)
    {
        CoreDb.Append(session, area, new Dictionary<int, object?> { [NomeId] = "Alice", [SaldoId] = 150.0 });
        CoreDb.Append(session, area, new Dictionary<int, object?> { [NomeId] = "Bob", [SaldoId] = 80.0 });
        CoreDb.Append(session, area, new Dictionary<int, object?> { [NomeId] = "Carol", [SaldoId] = 200.0 });
        CoreDb.Append(session, area, new Dictionary<int, object?> { [NomeId] = "Dave", [SaldoId] = 50.0 });
        CoreDb.GoTop(session, area);
    }

    private static bool IsDeleted(IDatabaseDriver driver)
        => driver.GetCurrentRecord()?.ContainsKey(-1) == true;

    [Fact]
    public void Delete_For_Condition_Marks_Matching_Records()
    {
        var session = new DataSession();
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        long affected = CoreDb.ScopedDelete(session, area,
            ScopeInfoData.All, forCond: "Saldo > 100");

        Assert.Equal(2, affected); // Alice, Carol
        CoreDb.GoTo(session, area, 1); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 2); Assert.False(IsDeleted(driver));
        CoreDb.GoTo(session, area, 3); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 4); Assert.False(IsDeleted(driver));
    }

    [Fact]
    public void Recall_For_Condition_Restores_Matching_Records()
    {
        var session = new DataSession();
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        CoreDb.ScopedDelete(session, area, ScopeInfoData.All, forCond: "Saldo > 100");
        long affected = CoreDb.ScopedRecall(session, area,
            ScopeInfoData.All, forCond: "Saldo > 100");

        Assert.Equal(2, affected);
        CoreDb.GoTo(session, area, 1); Assert.False(IsDeleted(driver));
        CoreDb.GoTo(session, area, 3); Assert.False(IsDeleted(driver));
    }

    [Fact]
    public void Delete_While_Condition_Stops_At_First_Non_Matching()
    {
        var session = new DataSession();
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        // Dal top: Alice(150) ok, Bob(80) fallisce → si ferma.
        long affected = CoreDb.ScopedDelete(session, area,
            ScopeInfoData.All, whileCond: "Saldo > 100");

        Assert.Equal(1, affected);
        CoreDb.GoTo(session, area, 1); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 2); Assert.False(IsDeleted(driver));
        CoreDb.GoTo(session, area, 3); Assert.False(IsDeleted(driver));
    }

    [Fact]
    public void Replace_For_Condition_Updates_Only_Matching_And_Preserves_Other_Fields()
    {
        var session = new DataSession();
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        long affected = CoreDb.ScopedReplace(session, area,
            ScopeInfoData.All, new Dictionary<string, object?> { ["Saldo"] = 0.0 },
            forCond: "Saldo < 100");

        Assert.Equal(2, affected); // Bob, Dave
        CoreDb.GoTo(session, area, 1); Assert.Equal("Alice", driver.GetCurrentRecord()![NomeId]);
        CoreDb.GoTo(session, area, 2); Assert.Equal(0.0, driver.GetCurrentRecord()![SaldoId]);
        CoreDb.GoTo(session, area, 2); Assert.Equal("Bob", driver.GetCurrentRecord()![NomeId]);
        CoreDb.GoTo(session, area, 4); Assert.Equal("Dave", driver.GetCurrentRecord()![NomeId]);
    }

    [Fact]
    public void Delete_Next_Limits_To_Count()
    {
        var session = new DataSession();
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        long affected = CoreDb.ScopedDelete(session, area, ScopeInfoData.NextN(2));

        Assert.Equal(2, affected);
        CoreDb.GoTo(session, area, 1); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 2); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 3); Assert.False(IsDeleted(driver));
    }

    [Fact]
    public void Delete_Record_Specific_Record()
    {
        var session = new DataSession();
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        long affected = CoreDb.ScopedDelete(session, area, ScopeInfoData.RecordN(2));

        Assert.Equal(1, affected);
        CoreDb.GoTo(session, area, 1); Assert.False(IsDeleted(driver));
        CoreDb.GoTo(session, area, 2); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 3); Assert.False(IsDeleted(driver));
        CoreDb.GoTo(session, area, 4); Assert.False(IsDeleted(driver));
    }

    [Fact]
    public void Delete_Rest_From_Current_To_End()
    {
        var session = new DataSession();
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        CoreDb.GoTo(session, area, 2);
        long affected = CoreDb.ScopedDelete(session, area, ScopeInfoData.Rest);

        Assert.Equal(3, affected); // record 2,3,4
        CoreDb.GoTo(session, area, 1); Assert.False(IsDeleted(driver));
        CoreDb.GoTo(session, area, 2); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 4); Assert.True(IsDeleted(driver));
    }

    [Fact]
    public void IlRuntime_RddDelete_Record_And_Rest_Map_Correctly()
    {
        IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(TextWriter.Null, new Runtime.RuntimeConfig()));
        var session = IlRuntime.Current.SqlDataSession;
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        // scopeType 2 = Record 3 → solo il record 3.
        Assert.Equal(1, IlRuntime.RddDelete(2, 3.0, null, "M"));
        CoreDb.GoTo(session, area, 3); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 4); Assert.False(IsDeleted(driver));

        // scopeType 3 = Rest → da top (GoTop) fino a fine file: tutti i 4 record
        // (il record 3 era già marcato, Delete è idempotente ma conta l'azione).
        CoreDb.GoTop(session, area);
        Assert.Equal(4, IlRuntime.RddDelete(3, null, null, "M"));
        CoreDb.GoTo(session, area, 1); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 2); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 4); Assert.True(IsDeleted(driver));
    }

    [Fact]
    public void IlRuntime_RddDelete_For_Forwards_Condition()
    {
        IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(TextWriter.Null, new Runtime.RuntimeConfig()));
        var session = IlRuntime.Current.SqlDataSession;
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        // scopeType 5 = For → All + condizione come stringa.
        long affected = IlRuntime.RddDelete(5, null, "Saldo > 100", "M");

        Assert.Equal(2, affected);
        CoreDb.GoTo(session, area, 1); Assert.True(IsDeleted(driver));
        CoreDb.GoTo(session, area, 3); Assert.True(IsDeleted(driver));
    }

    [Fact]
    public void IlRuntime_RddReplace_For_Forwards_Condition_And_Preserves_Fields()
    {
        IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(TextWriter.Null, new Runtime.RuntimeConfig()));
        var session = IlRuntime.Current.SqlDataSession;
        var driver = OpenArray(session, "M");
        var area = session.CurrentArea;
        Seed(session, area);

        var dict = IlRuntime.RddNewReplaceDict();
        IlRuntime.RddReplaceDictSet(dict, "Saldo", 1.0);
        long affected = IlRuntime.RddReplace(dict, 5, null, "Saldo < 100", "M");

        Assert.Equal(2, affected); // Bob, Dave
        CoreDb.GoTo(session, area, 2);
        Assert.Equal("Bob", driver.GetCurrentRecord()![NomeId]); // preservato
        Assert.Equal(1.0, driver.GetCurrentRecord()![SaldoId]);
    }
}
