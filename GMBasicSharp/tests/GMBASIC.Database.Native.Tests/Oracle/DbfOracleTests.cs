// FILE: Basic.Database.Native.Tests.Oracle.DbfOracleTests

using System.Text;
using Basic.Databases;
using Basic.DBF;

namespace Basic.Database.Native.Tests.Oracle;

public sealed class DbfOracleTests
{
    [Fact]
    public void Create_Append_GoSkip_Yields_Recno_Fields_Eof()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        var path = NativeDb.Tmp("oracle.dbf");
        DbfFile.CreateNew(path, Encoding.GetEncoding(1252),
        [
            new DbfField("NAME", DbfFieldType.Character, 20, 0, 0),
            new DbfField("AGE", DbfFieldType.Numeric, 8, 0, 0),
        ]);

        using var engine = new DbfEngine(path, indexPaths: null, exclusive: true, LockingStrategy.IntraProcess);
        var empty = new Dictionary<IIndexHandle, byte[]>();
        engine.AppendRecord(new Dictionary<string, object?> { ["NAME"] = "ALICE", ["AGE"] = 30 }, empty);
        engine.AppendRecord(new Dictionary<string, object?> { ["NAME"] = "BOB", ["AGE"] = 21 }, empty);
        engine.GoTop();

        Assert.Equal(1, engine.GetCurrentRecordNumber());
        Assert.False(engine.Eof);
        Assert.Equal("ALICE", engine.GetCurrentRecordValues()!["NAME"]?.ToString()?.Trim());
        Assert.Equal("30", Convert.ToDecimal(engine.GetCurrentRecordValues()!["AGE"]).ToString("0"));

        engine.Skip(1);
        Assert.Equal(2, engine.GetCurrentRecordNumber());
        Assert.Equal("BOB", engine.GetCurrentRecordValues()!["NAME"]?.ToString()?.Trim());

        engine.Skip(1);
        Assert.True(engine.Eof);
        Assert.Equal(2, engine.GetTotalRecordCount());
        Assert.Equal(2, engine.GetFields().Count);
        Assert.Equal("NAME", engine.GetFields()[0].Name.TrimEnd('\0'));
    }
}
