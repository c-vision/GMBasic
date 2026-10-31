using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Basic.Databases;
using Basic.Databases.Rdd;
using Basic.Runtime;
using Xunit;

namespace Basic.IlBackend.Tests;

/// <summary>
/// Test dei driver RDD su FILE REALI (DBF/ISAM/SPEED/CSV/DELIM/SDF + Ledger).
/// Convenzione percorsi:
///   - FIXTURE (dati di input, versionate) → samples/fixtures/
///   - FILE PRODOTTI dal test (scratch, git-ignorati) → samples/database/work/
/// </summary>
[Collection("GpmRuntimeState")]
public class RddRealFileDriversTests
{
    private static string RepoRoot
    {
        get
        {
            var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
            while (dir != null && !File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) dir = dir.Parent;
            return dir?.FullName ?? Directory.GetCurrentDirectory();
        }
    }

    private static string FixturesDir => Path.Combine(RepoRoot, "samples", "Fixtures");
    private static string WorkDir => Path.Combine(RepoRoot, "samples", "database", "work");
    private static string Fixture(string name) => Path.Combine(FixturesDir, name);
    private static string Work(string name) => Path.Combine(WorkDir, name);

    private const string IsamSchemaJson =
        "{\"TypeName\":\"Item\",\"Fields\":[" +
        "{\"Name\":\"ID\",\"Type\":\"LONG\"}," +
        "{\"Name\":\"Name\",\"Type\":\"STRING\",\"Length\":32}," +
        "{\"Name\":\"Price\",\"Type\":\"DOUBLE\"}]}";

    private static readonly object FixtureGate = new();
    private static bool _fixturesReady;
    private static long _ledgerTxId = 1;

    // ====================================================================
    // Fixture generation (file reali, generati una volta se mancanti)
    // ====================================================================
    private static void EnsureFixtures()
    {
        lock (FixtureGate)
        {
            if (_fixturesReady) return;
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
            // Il ctor statico di DatabaseService registra i driver su file
            // (DBF/ISAM/SPEED) nel DriverRegistry: forza la registrazione.
            _ = new DatabaseService();
            Directory.CreateDirectory(FixturesDir);
            Directory.CreateDirectory(WorkDir);
            var enc = Encoding.GetEncoding(1252);

            if (!File.Exists(Fixture("cust.dbf")))
            {
                Basic.DBF.DbfFile.CreateNew(Fixture("cust.dbf"), enc,
                [
                    new Basic.DBF.DbfField("ID", Basic.DBF.DbfFieldType.Character, 10, 0, 0),
                    new Basic.DBF.DbfField("NAME", Basic.DBF.DbfFieldType.Character, 20, 0, 0),
                    new Basic.DBF.DbfField("BALANCE", Basic.DBF.DbfFieldType.Numeric, 12, 2, 0),
                ]);
                using var eng = new Basic.DBF.DbfEngine(Fixture("cust.dbf"), null, exclusive: true,
                    Basic.Databases.LockingStrategy.IntraProcess);
                var noIdx = new Dictionary<Basic.DBF.IIndexHandle, byte[]>();
                eng.AppendRecord(new Dictionary<string, object?> { ["ID"] = "A", ["NAME"] = "ALICE", ["BALANCE"] = 100m }, noIdx);
                eng.AppendRecord(new Dictionary<string, object?> { ["ID"] = "B", ["NAME"] = "BOB", ["BALANCE"] = 250m }, noIdx);
            }

            if (!File.Exists(Fixture("people.csv")))
                File.WriteAllText(Fixture("people.csv"), "ID,NAME,AGE\n1,Alice,30\n2,Bob,41\n3,Carla,22\n");

            if (!File.Exists(Fixture("data.delim")))
                File.WriteAllText(Fixture("data.delim"), "id,label\n1,foo\n2,bar\n");

            if (!File.Exists(Fixture("data.sdf")))
                File.WriteAllText(Fixture("data.sdf"), "first-record-line\nsecond-record-line\n");

            var isamPath = Fixture("items.isdb");
            if (!File.Exists(isamPath))
            {
                var svc = new DatabaseService();
                var h = svc.IsamOpenSchema(isamPath, "ITEMS", IsamSchemaJson);
                svc.IsamInsert(h, [1L, "Widget", 19.99]);
                svc.IsamInsert(h, [2L, "Gadget", 49.99]);
                svc.IsamClose(h);
            }

            var speedDir = Fixture("kv.speed");
            if (!Directory.Exists(speedDir))
            {
                var db = new Basic.Databases.Speed.Database(speedDir, crossProcessLock: false);
                var meta = db.CreateTable(new Basic.Databases.Speed.Config.SpeedSchema("KV",
                    [
                        new Basic.Databases.Speed.Config.SpeedFieldSchema("Key", new Basic.StringType(), 32),
                        new Basic.Databases.Speed.Config.SpeedFieldSchema("Value", new Basic.StringType(), 32),
                    ],
                    new Basic.Databases.Speed.Config.SpeedIndexDefinition("PK", "Key", new Basic.StringType(), 32, true, false),
                    []));
                var spd = new SpeedDriver(db, "KV");
                SetSpeedTableId(spd, meta.Id);
                spd.Append(new Dictionary<int, object?> { [0] = "alpha", [1] = "first" });
                spd.Append(new Dictionary<int, object?> { [0] = "beta", [1] = "second" });
                spd.Close();
            }

            if (!File.Exists(Fixture("ledger.dat")))
            {
                using (var ctx = new Basic.Ledger.LedgerContext(Fixture("ledger.dat")))
                {
                    _ledgerTxId = Basic.Ledger.LedgerManager.AddTransaction(ctx, new DateOnly(2026, 8, 21), "seed",
                        [
                            new Basic.Ledger.LedgerEntry { Account = "1000", Amount = 100m, AmountIsExplicit = true, ExchangeRate = 1m, Currency = "EUR", Description = "seed" },
                            new Basic.Ledger.LedgerEntry { Account = "5000", Amount = -100m, AmountIsExplicit = true, ExchangeRate = 1m, Currency = "EUR", Description = "seed" }
                        ]);
                    Basic.Ledger.LedgerManager.SaveIndexToFile(ctx.IndexFilePath, ctx.TransactionIndex);
                }
            }

            _fixturesReady = true;
        }
    }

    private static void SetSpeedTableId(SpeedDriver driver, int id)
        => typeof(SpeedDriver).GetField("_tableId", BindingFlags.NonPublic | BindingFlags.Instance)!.SetValue(driver, id);

    private static SpeedDriver OpenSpeedFixture()
    {
        var db = new Basic.Databases.Speed.Database(Fixture("kv.speed"), crossProcessLock: false);
        var driver = new SpeedDriver(db, "KV");
        SetSpeedTableId(driver, 1); // prima (unica) tabella del fixture
        // Il SpeedDriver non ha lo schema UDT: i campi arrivano dall'USE ... OF Type.
        // Per il test KV (campo 0=chiave, 1=valore) li imposta esplicitamente.
        typeof(SpeedDriver).GetField("_fields", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(driver, new[]
            {
                new RddFieldInfo("Key", new Basic.StringType(), 32, 0, 0),
                new RddFieldInfo("Value", new Basic.StringType(), 32, 1, 1),
            });
        return driver;
    }

    // ====================================================================
    // DBF
    // ====================================================================
    [Fact]
    public void DbfDriver_ReadFixture_WriteWorkFile()
    {
        EnsureFixtures();
        var driver = DriverRegistry.Create("DBF")!;
        driver.Open(new DbOpenInfo { FileName = Fixture("cust.dbf"), Alias = "CUST" });
        driver.GoTop();
        Assert.Equal("A", driver.GetFieldValue(driver.GetFieldIndex("ID"))?.ToString()?.Trim());
        Assert.Equal("ALICE", driver.GetFieldValue(driver.GetFieldIndex("NAME"))?.ToString()?.Trim());
        driver.Skip(1);
        Assert.Equal("BOB", driver.GetFieldValue(driver.GetFieldIndex("NAME"))?.ToString()?.Trim());

        var workPath = Work("dbf_work.dbf");
        if (File.Exists(workPath)) File.Delete(workPath);
        Basic.DBF.DbfFile.CreateNew(workPath, Encoding.GetEncoding(1252),
        [
            new Basic.DBF.DbfField("ID", Basic.DBF.DbfFieldType.Character, 10, 0, 0),
            new Basic.DBF.DbfField("BALANCE", Basic.DBF.DbfFieldType.Numeric, 12, 2, 0),
        ]);
        using var eng = new Basic.DBF.DbfEngine(workPath, null, exclusive: true, Basic.Databases.LockingStrategy.IntraProcess);
        eng.AppendRecord(new Dictionary<string, object?> { ["ID"] = "X", ["BALANCE"] = 999m },
            new Dictionary<Basic.DBF.IIndexHandle, byte[]>());
        Assert.True(File.Exists(workPath), "work file prodotto in samples/database/work/");
    }

    [Fact]
    public void DbfDriver_IndexAndLocks()
    {
        EnsureFixtures();
        var driver = DriverRegistry.Create("DBF")!;
        driver.Open(new DbOpenInfo { FileName = Fixture("cust.dbf"), Alias = "CUST" });
        // Lock/Unlock file + record (prima erano no-op)
        Assert.True(driver.FileLock());
        driver.FileUnlock();
        driver.GoTop();
        Assert.True(driver.Lock(1));
        driver.Unlock(1);

        // Crea un indice NDX su NAME, setta ordine, SEEK, poi lo chiude.
        var ntx = Work("dbf_name.ntx");
        if (File.Exists(ntx)) File.Delete(ntx);
        driver.CreateIndex(new DbOrderInfo { Name = ntx, KeyExpression = "NAME", KeyLength = 20 });
        Assert.True(File.Exists(ntx), "indice NDX creato");
        var idxName = Path.GetFileNameWithoutExtension(ntx); // Name dell'handle NDX
        driver.SetOrder(idxName);
        Assert.True(driver.Seek(new DbSeekInfo { Key = "BOB" }));
        Assert.Equal("B", driver.GetFieldValue(driver.GetFieldIndex("ID"))?.ToString()?.Trim());
        driver.CloseIndex(idxName);
        // CLOSE INDEX scollega l'indice dall'engine ma NON cancella il file (xBase).
        Assert.True(File.Exists(ntx));
        // L'ordine sull'indice chiuso non è più valido → tornato a ordine fisico.
        Assert.Throws<System.IO.FileNotFoundException>(() => driver.SetOrder(idxName));
    }

    // ====================================================================
    // CSV / DELIM / SDF (read-only)
    // ====================================================================
    [Theory]
    [InlineData("CSV", "people.csv", "Alice")]
    [InlineData("DELIM", "data.delim", "foo")]
    public void TextDrivers_ReadFixture(string kind, string fixture, string expected)
    {
        EnsureFixtures();
        var driver = DriverRegistry.Create(kind)!;
        driver.Open(new DbOpenInfo { FileName = Fixture(fixture), Alias = "T" });
        driver.GoTop();
        Assert.Equal(expected, driver.GetFieldValue(1)?.ToString()?.Trim());
    }

    [Fact]
    public void SdfDriver_ReadFixture()
    {
        EnsureFixtures();
        var driver = DriverRegistry.Create("SDF")!;
        driver.Open(new DbOpenInfo { FileName = Fixture("data.sdf"), Alias = "S" });
        driver.GoTop();
        Assert.Equal("first-record-line", driver.GetFieldValue(0)?.ToString()?.Trim());
    }

    // ====================================================================
    // ISAM — via il DRIVER (IsamDriver.Open legge lo schema DAL FILE)
    // ====================================================================
    [Fact]
    public void IsamDriver_ReadFixture()
    {
        EnsureFixtures();
        var driver = DriverRegistry.Create("ISAM")!;
        driver.Open(new DbOpenInfo { FileName = Fixture("items.isdb"), Alias = "ITEMS" });
        driver.GoTop();
        Assert.Equal("Widget", driver.GetFieldValue(driver.GetFieldIndex("Name"))?.ToString()?.Trim());
        driver.Skip(1);
        Assert.Equal("Gadget", driver.GetFieldValue(driver.GetFieldIndex("Name"))?.ToString()?.Trim());

        // GoTo (prima no-op): 2° record via top+skip
        driver.GoTo(2);
        Assert.Equal("Gadget", driver.GetFieldValue(driver.GetFieldIndex("Name"))?.ToString()?.Trim());

        // CreateIndex secondario su Price + SetOrder + CloseIndex (prima no-op)
        driver.CreateIndex(new DbOrderInfo { Name = "IX_PRICE", KeyExpression = "Price", KeyLength = 8 });
        driver.SetOrder("IX_PRICE");
        driver.CloseIndex("IX_PRICE");

        // Zap su una COPIA del fixture (Zap distrugge i dati)
        var zapCopy = Work("isam_zap.isdb");
        File.Copy(Fixture("items.isdb"), zapCopy, overwrite: true);
        var zap = DriverRegistry.Create("ISAM")!;
        zap.Open(new DbOpenInfo { FileName = zapCopy, Alias = "Z" });
        zap.Zap();
        Assert.True(zap.RecCount == 0, $"dopo ZAP RecCount={zap.RecCount}");
        zap.Close();
    }

    // ====================================================================
    // SPEED (directory KV)
    // ====================================================================
    [Fact]
    public void SpeedDriver_ReadFixture()
    {
        EnsureFixtures();
        var driver = OpenSpeedFixture();
        driver.GoTop();
        var values = new List<string?>();
        while (!driver.Eof) { values.Add(driver.GetFieldValue(1)?.ToString()); driver.Skip(1); }
        Assert.Contains("first", values);
        Assert.Contains("second", values);
        driver.Close();
    }

    [Fact]
    public void SpeedDriver_GoToIndexZap()
    {
        EnsureFixtures();
        // GoTo (prima no-op): 2° record via top+skip.
        var driver = OpenSpeedFixture();
        driver.GoTo(2);
        var second = driver.GetFieldValue(1)?.ToString();
        Assert.True(second is "first" or "second", $"GoTo(2) = {second}");

        // CreateIndex secondario + SetOrder (prima no-op).
        driver.CreateIndex(new DbOrderInfo { Name = "IX_VAL", KeyExpression = "Value", KeyLength = 32 });
        driver.SetOrder("IX_VAL");
        driver.Close();

        // Zap su una COPIA del database (Zap distrugge i dati).
        var zapDir = Work("speed_zap");
        if (Directory.Exists(zapDir)) Directory.Delete(zapDir, recursive: true);
        CopyDirectory(Fixture("kv.speed"), zapDir);
        var zap = OpenSpeedAt(zapDir);
        zap.GoTop();
        Assert.False(zap.Eof, "prima di ZAP la copia ha record");
        zap.Zap();
        zap.GoTop();
        Assert.True(zap.Eof, "dopo ZAP la copia è vuota");
        zap.Close();
    }

    private static SpeedDriver OpenSpeedAt(string dir)
    {
        var db = new Basic.Databases.Speed.Database(dir, crossProcessLock: false);
        var driver = new SpeedDriver(db, "KV");
        SetSpeedTableId(driver, 1);
        typeof(SpeedDriver).GetField("_fields", BindingFlags.NonPublic | BindingFlags.Instance)!
            .SetValue(driver, new[]
            {
                new RddFieldInfo("Key", new Basic.StringType(), 32, 0, 0),
                new RddFieldInfo("Value", new Basic.StringType(), 32, 1, 1),
            });
        return driver;
    }

    private static void CopyDirectory(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        foreach (var file in Directory.GetFiles(src, "*", SearchOption.AllDirectories))
        {
            var rel = Path.GetRelativePath(src, file);
            var dest = Path.Combine(dst, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
            File.Copy(file, dest, overwrite: true);
        }
    }

    // ====================================================================
    // SQL (OpenIndex→ORDER BY reale, Recall lancia — non più no-op)
    // ====================================================================
    [Fact]
    public void SqlDriver_OrderAndRecall()
    {
        var dbPath = Work("sql_order.db");
        if (File.Exists(dbPath)) File.Delete(dbPath);
        var driver = DriverRegistry.Create("SQL")!;
        driver.Open(new DbOpenInfo { FileName = dbPath, Alias = "ORD" });
        driver.ExecRaw("CREATE TABLE ORD (id INTEGER PRIMARY KEY, name TEXT)", null, null);
        driver.ExecRaw("INSERT INTO ORD (name) VALUES ('b')", null, null);
        driver.ExecRaw("INSERT INTO ORD (name) VALUES ('a')", null, null);
        driver.ExecRaw("INSERT INTO ORD (name) VALUES ('c')", null, null);

        // OpenIndex → SetOrder (ORDER BY name): prima era no-op. In SQL l'ordine è
        // una COLONNA, non un file indice.
        driver.OpenIndex("name", "");
        driver.GoTop();
        Assert.Equal("a", driver.GetFieldValue(driver.GetFieldIndex("name"))?.ToString()?.Trim());

        // Recall → NotSupportedException (DELETE SQL è fisico), non più no-op.
        Assert.Throws<NotSupportedException>(() => driver.Recall());

        // Append/Delete con parametri (prima fallivano silenziosamente su SQLite/PG).
        driver.GoBottom();
        driver.Append(new Dictionary<int, object?> { [0] = 4.0, [1] = "delta" });
        driver.GoTo(1);
        driver.Delete();
        driver.GoTop();
        Assert.Equal("b", driver.GetFieldValue(driver.GetFieldIndex("name"))?.ToString()?.Trim());
        driver.GoBottom();
        Assert.Equal("delta", driver.GetFieldValue(driver.GetFieldIndex("name"))?.ToString()?.Trim());
        driver.Close();
    }

    // ====================================================================
    // ARRAY (indici in-memory: ordine di campo + SEEK)
    // ====================================================================
    [Fact]
    public void ArrayDriver_IndexOrderSeek()
    {
        var driver = new Basic.Databases.Rdd.ArrayDriver("ARR");
        driver.SetFields(new[]
        {
            new RddFieldInfo("ID", new Basic.DoubleType(), 0, 0, 0),
            new RddFieldInfo("NAME", new Basic.StringType(20), 20, 1, 1),
        });
        driver.Append(new Dictionary<int, object?> { [0] = 2.0, [1] = "beta" });
        driver.Append(new Dictionary<int, object?> { [0] = 1.0, [1] = "alpha" });
        driver.Append(new Dictionary<int, object?> { [0] = 3.0, [1] = "gamma" });

        // Ordine fisico iniziale
        driver.GoTop();
        Assert.Equal("beta", driver.GetFieldValue(1)?.ToString());

        // CreateIndex + SetOrder → ordina per NAME (prima no-op)
        driver.CreateIndex(new DbOrderInfo { Name = "IX_NAME", KeyExpression = "NAME", KeyLength = 20 });
        driver.SetOrder("IX_NAME");
        driver.GoTop();
        Assert.Equal("alpha", driver.GetFieldValue(1)?.ToString());
        driver.Skip(2);
        Assert.Equal("gamma", driver.GetFieldValue(1)?.ToString());

        // Seek con ordine attivo (cerca nel campo ordinato)
        Assert.True(driver.Seek(new DbSeekInfo { Key = "gamma" }));
        Assert.Equal(3.0, driver.GetFieldValue(0));

        // CloseIndex → torna a ordine fisico (prima no-op)
        driver.CloseIndex("IX_NAME");
        driver.GoTop();
        Assert.Equal("beta", driver.GetFieldValue(1)?.ToString());

        // Recall (array supporta undelete) + lock advisory
        Assert.True(driver.Lock(1));
        driver.Unlock(1);
    }

    // ====================================================================
    // CSV/DELIM/SDF — manipolazione + indice virtuale (riga) + write-back
    // ====================================================================
    [Fact]
    public void CsvDriver_ManipulateVirtualIndexWriteBack()
    {
        EnsureFixtures();
        var workPath = Work("people_rw.csv");
        File.Copy(Fixture("people.csv"), workPath, overwrite: true);

        // Lettura: indice virtuale = riga, SEEK per valore e per riga.
        var d = DriverRegistry.Create("CSV")!;
        d.Open(new DbOpenInfo { FileName = workPath, Alias = "P" });
        d.GoTop();
        Assert.True(d.Seek(new DbSeekInfo { Key = "Bob" }));
        Assert.Equal("Bob", d.GetFieldValue(d.GetFieldIndex("NAME"))?.ToString()?.Trim());
        Assert.Equal(2, d.CurrentKeyNo()); // riga 2 (indice virtuale)
        Assert.True(d.Seek(new DbSeekInfo { Key = 3.0 }));
        Assert.Equal("Carla", d.GetFieldValue(d.GetFieldIndex("NAME"))?.ToString()?.Trim());

        // Delete soft + Recall.
        d.Delete();
        Assert.True(d.GetCurrentRecord()!.ContainsKey(-1));
        d.Recall();
        Assert.False(d.GetCurrentRecord()!.ContainsKey(-1));

        // Append con campo che richiede quoting (virgola interna) → write-back RFC 4180.
        d.Append(new Dictionary<int, object?> { [0] = 4.0, [1] = "Rossi, Mario", [2] = 55.0 });
        d.Close();

        // Riapri: la riga quotata si legge correttamente (niente split naive).
        var d2 = DriverRegistry.Create("CSV")!;
        d2.Open(new DbOpenInfo { FileName = workPath, Alias = "P" });
        d2.GoBottom();
        Assert.Equal("Rossi, Mario", d2.GetFieldValue(d2.GetFieldIndex("NAME"))?.ToString()?.Trim());
        Assert.Equal(4, d2.RecCount);
        d2.Close();
    }

    [Fact]
    public void SdfDriver_Manipulate()
    {
        EnsureFixtures();
        var workPath = Work("data_rw.sdf");
        File.Copy(Fixture("data.sdf"), workPath, overwrite: true);
        var d = DriverRegistry.Create("SDF")!;
        d.Open(new DbOpenInfo { FileName = workPath, Alias = "S" });
        d.GoBottom();
        d.PutCurrentRecord(new Dictionary<int, object?> { [0] = "third-record-line" });
        d.Append(new Dictionary<int, object?> { [0] = "fourth" });
        d.Close();

        var d2 = DriverRegistry.Create("SDF")!;
        d2.Open(new DbOpenInfo { FileName = workPath, Alias = "S" });
        d2.GoTo(2);
        Assert.Equal("third-record-line", d2.GetFieldValue(0)?.ToString()?.Trim());
        Assert.Equal(3, d2.Seek(new DbSeekInfo { Key = "fourth" }) ? d2.CurrentKeyNo() : -1);
        d2.Close();
    }

    // ====================================================================
    // Ledger
    // ====================================================================
    [Fact]
    public void Ledger_ReadFixture()
    {
        EnsureFixtures();
        using var ctx = new Basic.Ledger.LedgerContext(Fixture("ledger.dat"));
        if (File.Exists(ctx.IndexFilePath))
            Basic.Ledger.LedgerManager.ReadIndexFromFile(ctx.IndexFilePath, ctx.TransactionIndex);
        var entries = Basic.Ledger.LedgerManager.GetTransaction(ctx, _ledgerTxId);
        Assert.NotNull(entries);
        Assert.Contains(entries!, e => e.Account == "1000");
    }
}
