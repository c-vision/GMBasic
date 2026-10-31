// FILE: AI.Tests.SqlStatsUdfTests
// Aggregati statistici SQLite (parità C gmb_sql.c §0bis): VAR_POP/VAR_SAMP/VARIANCE/
// STDDEV_POP/STDDEV_SAMP/STDDEV/MEDIAN.
using Basic.SQLite;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class SqlStatsUdfTests : IDisposable
{
    private readonly SqliteConnection _connection = new("Data Source=:memory:");

    public SqlStatsUdfTests()
    {
        _connection.Open();
        SqlStatsUdf.Register(_connection);
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "CREATE TABLE sv(x REAL); INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)";
        cmd.ExecuteNonQuery();
    }

    public void Dispose() => _connection.Dispose();

    private object? Scalar(string sql)
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = sql;
        var v = cmd.ExecuteScalar();
        return v is DBNull ? null : v;
    }

    [Fact]
    public void Variance_And_Stddev_Population_And_Sample()
    {
        Convert.ToDouble(Scalar("SELECT VAR_POP(x) FROM sv")).Should().BeApproximately(4.0, 1e-12);
        Convert.ToDouble(Scalar("SELECT STDDEV_POP(x) FROM sv")).Should().BeApproximately(2.0, 1e-12);
        Convert.ToDouble(Scalar("SELECT VAR_SAMP(x) FROM sv")).Should().BeApproximately(32.0 / 7.0, 1e-12);
        Convert.ToDouble(Scalar("SELECT VARIANCE(x) FROM sv")).Should().BeApproximately(32.0 / 7.0, 1e-12);
        Convert.ToDouble(Scalar("SELECT STDDEV_SAMP(x) FROM sv")).Should().BeApproximately(Math.Sqrt(32.0 / 7.0), 1e-12);
        Convert.ToDouble(Scalar("SELECT STDDEV(x) FROM sv")).Should().BeApproximately(Math.Sqrt(32.0 / 7.0), 1e-12);
    }

    [Fact]
    public void Median_Odd_And_Even()
    {
        Convert.ToDouble(Scalar("SELECT MEDIAN(x) FROM sv")).Should().BeApproximately(4.5, 1e-12); // {2,4,4,4,5,5,7,9}
        Convert.ToDouble(Scalar("SELECT MEDIAN(x) FROM sv WHERE x IN (2,9)")).Should().BeApproximately(5.5, 1e-12);
        Convert.ToDouble(Scalar("SELECT MEDIAN(x) FROM sv WHERE x = 7")).Should().BeApproximately(7.0, 1e-12);
    }

    [Fact]
    public void Nulls_Skipped_And_Empty_IsNull()
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "CREATE TABLE n(v REAL); INSERT INTO n VALUES (1),(NULL),(3),(NULL)";
        cmd.ExecuteNonQuery();
        Convert.ToDouble(Scalar("SELECT MEDIAN(v) FROM n")).Should().BeApproximately(2.0, 1e-12);   // {1,3}
        Convert.ToDouble(Scalar("SELECT VAR_POP(v) FROM n")).Should().BeApproximately(1.0, 1e-12);  // {1,3}
        Scalar("SELECT VAR_POP(x) FROM sv WHERE 0").Should().BeNull();
        Scalar("SELECT MEDIAN(x) FROM sv WHERE 0").Should().BeNull();
        Scalar("SELECT VAR_SAMP(x) FROM sv WHERE x = 2").Should().BeNull();  // n<2
        Scalar("SELECT STDDEV(x) FROM sv WHERE x = 2").Should().BeNull();
    }

    [Fact]
    public void Mode_MostFrequent_TieSmallest()
    {
        Convert.ToDouble(Scalar("SELECT MODE(x) FROM sv")).Should().Be(4.0);   // {2,4,4,4,5,5,7,9}
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "CREATE TABLE tie(v REAL); INSERT INTO tie VALUES (2),(2),(1),(1),(3)";
        cmd.ExecuteNonQuery();
        Convert.ToDouble(Scalar("SELECT MODE(v) FROM tie")).Should().Be(1.0);  // tie {1,2} -> più piccolo
        Scalar("SELECT MODE(x) FROM sv WHERE 0").Should().BeNull();
    }

    [Fact]
    public void Quantile_LinearInterpolation()
    {
        Convert.ToDouble(Scalar("SELECT QUANTILE(x,0.0) FROM sv")).Should().BeApproximately(2.0, 1e-12);
        Convert.ToDouble(Scalar("SELECT QUANTILE(x,0.5) FROM sv")).Should().BeApproximately(4.5, 1e-12);
        Convert.ToDouble(Scalar("SELECT QUANTILE(x,1.0) FROM sv")).Should().BeApproximately(9.0, 1e-12);
        Convert.ToDouble(Scalar("SELECT QUANTILE(x,0.25) FROM sv")).Should().BeApproximately(4.0, 1e-12);
        Scalar("SELECT QUANTILE(x,1.5) FROM sv").Should().BeNull();
        Scalar("SELECT QUANTILE(x,-0.1) FROM sv").Should().BeNull();
        Scalar("SELECT QUANTILE(x,0.5) FROM sv WHERE 0").Should().BeNull();
    }

    [Fact]
    public void Covariance_And_Correlation()
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "CREATE TABLE xy(x REAL, y REAL); INSERT INTO xy VALUES (2,5),(4,9),(4,9),(4,9),(5,11),(5,11),(7,15),(9,19)";
        cmd.ExecuteNonQuery();
        Convert.ToDouble(Scalar("SELECT CORR(y,x) FROM xy")).Should().BeApproximately(1.0, 1e-9);        // y = 2x+1
        Convert.ToDouble(Scalar("SELECT COVAR_SAMP(y,x) FROM xy")).Should().BeApproximately(2.0 * (32.0 / 7.0), 1e-9);
        Convert.ToDouble(Scalar("SELECT COVAR_POP(y,x) FROM xy")).Should().BeApproximately(8.0, 1e-9);   // 2 * var_pop(x)=2*4
        Scalar("SELECT CORR(y,x) FROM xy WHERE 0").Should().BeNull();
        Scalar("SELECT COVAR_SAMP(y,x) FROM xy WHERE x = 2").Should().BeNull();  // n<2
    }
}
