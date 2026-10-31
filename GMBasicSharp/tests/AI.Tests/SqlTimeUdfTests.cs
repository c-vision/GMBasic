// FILE: AI.Tests.SqlTimeUdfTests
// UDF temporali TIMEBUCKET/FIRST_TS/LAST_TS (parità C gmb_sql.c §0bis).
using Basic.SQLite;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class SqlTimeUdfTests : IDisposable
{
    private readonly SqliteConnection _connection = new("Data Source=:memory:");

    public SqlTimeUdfTests()
    {
        _connection.Open();
        SqlTimeUdf.Register(_connection);
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "CREATE TABLE ticks(ts INTEGER, price INTEGER, vol INTEGER); INSERT INTO ticks VALUES (0,10,100),(60,12,200),(120,11,150),(300,20,50),(360,18,75),(420,22,125),(-60,9,10)";
        cmd.ExecuteNonQuery();
    }

    public void Dispose() => _connection.Dispose();

    private List<object?[]> Query(string sql)
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = sql;
        using var r = cmd.ExecuteReader();
        var rows = new List<object?[]>();
        while (r.Read())
        {
            var row = new object?[r.FieldCount];
            r.GetValues(row);
            rows.Add(row);
        }
        return rows;
    }

    [Fact]
    public void TimeBucket_Ohlcv_Parity()
    {
        var rows = Query("SELECT TIMEBUCKET(ts,300) AS b, FIRST_TS(price,ts) AS o, MAX(price) AS h, MIN(price) AS l, LAST_TS(price,ts) AS c, SUM(vol) AS v FROM ticks GROUP BY b ORDER BY b");
        rows.Should().HaveCount(3);
        rows[0].Select(v => Convert.ToString(v)).Should().Equal("-300", "9", "9", "9", "9", "10");
        rows[1].Select(v => Convert.ToString(v)).Should().Equal("0", "10", "12", "10", "11", "450");
        rows[2].Select(v => Convert.ToString(v)).Should().Equal("300", "20", "22", "18", "22", "250");
    }

    [Fact]
    public void TimeBucket_Negative_Floor_Not_Truncate()
    {
        Query("SELECT TIMEBUCKET(-1,300)").Single()[0].Should().Be(-300L);
    }

    [Fact]
    public void TimeBucket_BadArgs_Null()
    {
        Query("SELECT TIMEBUCKET(7,0)").Single()[0].Should().Be(DBNull.Value);
        Query("SELECT TIMEBUCKET(NULL,300)").Single()[0].Should().Be(DBNull.Value);
        Query("SELECT TIMEBUCKET('abc',300)").Single()[0].Should().Be(DBNull.Value);
    }

    [Fact]
    public void FirstLast_Empty_Null()
    {
        Query("SELECT FIRST_TS(price,ts) FROM ticks WHERE 0").Single()[0].Should().Be(DBNull.Value);
    }

    [Fact]
    public void FirstLast_Tie_FirstRow_Wins()
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "CREATE TABLE tie(ts INTEGER, v TEXT); INSERT INTO tie VALUES (5,'a'),(5,'b'),(9,'c')";
        cmd.ExecuteNonQuery();
        Query("SELECT FIRST_TS(v,ts) FROM tie").Single()[0].Should().Be("a");
        Query("SELECT LAST_TS(v,ts) FROM tie").Single()[0].Should().Be("c");
    }

    [Fact]
    public void FirstLast_Preserves_Text_Type()
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "CREATE TABLE sym(ts INTEGER, s TEXT); INSERT INTO sym VALUES (2,'x'),(1,'y')";
        cmd.ExecuteNonQuery();
        Query("SELECT FIRST_TS(s,ts) FROM sym").Single()[0].Should().Be("y");
    }
}
