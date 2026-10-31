using Basic.Databases.Rdd;
using Xunit;

namespace Basic.IlBackend.Tests;

/// <summary>
/// SQL.CHANGES() e SQL.LASTINSERTROWID() portabili: verifica della mappatura
/// per-NOME-factory dei principali database (MySQL, MariaDB, MySql.Data, SQLite,
/// PostgreSQL, SQL Server, Oracle, Firebird) SENZA installare i driver/DB.
/// </summary>
public class SqlPortableFunctionsTests
{
    [Theory]
    [InlineData("Microsoft.Data.Sqlite.SqliteFactory", "SELECT last_insert_rowid()")]
    [InlineData("Npgsql.NpgsqlFactory", "SELECT lastval()")]
    [InlineData("MySqlConnector.MySqlConnectorFactory", "SELECT LAST_INSERT_ID()")]
    [InlineData("MySql.Data.MySqlClient.MySqlClientFactory", "SELECT LAST_INSERT_ID()")]
    [InlineData("MySqlConnector.MySqlConnectorFactory", "SELECT LAST_INSERT_ID()")] // MariaDB via MySqlConnector
    [InlineData("Microsoft.Data.SqlClient.SqlClientFactory", "SELECT SCOPE_IDENTITY()")]
    public void LastInsertRowIdSql_ProviderAgnostic(string factoryTypeName, string expected)
    {
        Assert.Equal(expected, SqlDriver.LastInsertRowIdSql(factoryTypeName));
    }

    [Theory]
    [InlineData("Oracle.ManagedDataAccess.Client.OracleClientFactory")]
    [InlineData("FirebirdSql.Data.FirebirdClient.FirebirdClientFactory")]
    [InlineData("System.Data.Odbc.OdbcFactory")]
    [InlineData("")]
    public void LastInsertRowIdSql_NoGenericRowId_ReturnsEmpty(string factoryTypeName)
    {
        // Oracle/Firebird non hanno un rowid di sessione recuperabile a posteriori
        // (serve RETURNING / GEN_ID(seq)) → stringa vuota → il driver ritorna 0.
        Assert.Equal("", SqlDriver.LastInsertRowIdSql(factoryTypeName));
    }

    [Theory]
    [InlineData("Server=db.example.com;Port=3306;Database=app;Uid=root")]
    [InlineData("Server=localhost;Uid=root;Database=app")]
    [InlineData("Server=localhost;Port=3307;Database=app;User=app")]
    public void Open_MySqlConnectionString_DetectsMySql(string connectionString)
    {
        // La detection di Open() deve instradare le stringhe MySQL/MariaDB a MySqlConnector
        // (non a Oracle/SqlClient): Uid= e Port=3306/3307 sono i segnali.
        var factory = SqlDriver.DetectFactory(connectionString);
        Assert.Contains("MySql", factory.GetType().Name);
    }

    [Theory]
    [InlineData("Host=localhost;Port=5432;Database=app;Username=u", "Npgsql")]
    [InlineData("Data Source=app.db", "Sqlite")]
    [InlineData("Server=sqlsrv;Database=app;Trusted_Connection=True", "SqlClient")]
    [InlineData("User Id=scott;Password=tiger;Data Source=orcl", "Oracle")]
    public void Open_OtherProviders_Detected(string connectionString, string expectedSubstring)
    {
        Assert.Contains(expectedSubstring, SqlDriver.DetectFactory(connectionString).GetType().Name);
    }
}
