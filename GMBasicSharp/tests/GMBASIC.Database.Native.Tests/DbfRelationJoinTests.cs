// FILE: Basic.Database.Native.Tests.DbfRelationJoinTests
using System.Text;
using Basic.Databases;
using Basic.Databases.Rdd;
using Basic.DBF;

namespace Basic.Database.Native.Tests;

/// <summary>
/// SET RELATION + JOIN WITH sul driver C# (stesso contratto del self-check C CoreDb_RddSelfCheck).
/// </summary>
public sealed class DbfRelationJoinTests
{
    [Fact]
    public void SetRelation_Skip_Repositions_Child_And_Join_Emits_Matches()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        var enc = Encoding.GetEncoding(1252);
        var custPath = NativeDb.Tmp("rel_cust.dbf");
        var ordPath = NativeDb.Tmp("rel_ord.dbf");
        var ntxPath = NativeDb.Tmp("rel_ord.ntx");
        var joinPath = NativeDb.Tmp("rel_join.dbf");
        var empty = new Dictionary<IIndexHandle, byte[]>();

        DbfFile.CreateNew(custPath, enc,
        [
            new DbfField("ID", DbfFieldType.Character, 10, 0, 0),
            new DbfField("NAME", DbfFieldType.Character, 10, 0, 0),
        ]);
        DbfFile.CreateNew(ordPath, enc,
        [
            new DbfField("CID", DbfFieldType.Character, 10, 0, 0),
            new DbfField("ITEM", DbfFieldType.Character, 10, 0, 0),
        ]);

        using var custEngine = new DbfEngine(custPath, null, exclusive: true, LockingStrategy.IntraProcess);
        using var ordEngine = new DbfEngine(ordPath, null, exclusive: true, LockingStrategy.IntraProcess);
        custEngine.AppendRecord(new Dictionary<string, object?> { ["ID"] = "A", ["NAME"] = "ALICE" }, empty);
        custEngine.AppendRecord(new Dictionary<string, object?> { ["ID"] = "B", ["NAME"] = "BOB" }, empty);
        ordEngine.AppendRecord(new Dictionary<string, object?> { ["CID"] = "A", ["ITEM"] = "X" }, empty);
        ordEngine.AppendRecord(new Dictionary<string, object?> { ["CID"] = "A", ["ITEM"] = "Y" }, empty);
        ordEngine.AppendRecord(new Dictionary<string, object?> { ["CID"] = "B", ["ITEM"] = "Z" }, empty);

        var parent = new DbfDriver(custEngine, "CUST");
        var child = new DbfDriver(ordEngine, "ORD");
        child.CreateIndex(new DbOrderInfo { Name = ntxPath, KeyExpression = "CID", KeyLength = 10 });

        Assert.True(parent.FieldCount >= 2);
        Assert.Equal(0, parent.GetFieldIndex("ID"));

        child.SetRelation(parent, "ID");
        parent.GoTop();
        Assert.True(child.SyncRelation());
        Assert.True(child.Found);
        Assert.Equal("X", child.GetFieldValue(1)?.ToString()?.Trim());

        parent.Skip(1);
        Assert.True(child.SyncRelation());
        Assert.True(child.Found);
        Assert.Equal("Z", child.GetFieldValue(1)?.ToString()?.Trim());

        DbfDriver.NestedLoopJoin(parent, child, joinPath, ctx =>
            string.Equals(ctx["ID"]?.ToString()?.Trim(), ctx["CID"]?.ToString()?.Trim(),
                StringComparison.OrdinalIgnoreCase));

        using var joined = new DbfEngine(joinPath, null, exclusive: true, LockingStrategy.IntraProcess);
        Assert.Equal(3, joined.GetTotalRecordCount());
    }
}
