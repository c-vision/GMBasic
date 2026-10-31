// FILE: Basic.Database.Native.Tests.Native.NativeParityTests

using System.Runtime.InteropServices;
using System.Text;
using Basic.Databases;
using Basic.DBF;
using Basic.Ledger;
using Compiler;

namespace Basic.Database.Native.Tests.Native;

public sealed class NativeParityTests
{
    public NativeParityTests() => NativeDb.EnsureLoaded();

    [Fact]
    public void Version_Is_V1()
    {
        Assert.Equal(0x00010000, NativeDb.Version());
    }

    [Fact]
    public void Dbf_C_Matches_Csharp_Oracle_Tuples_And_Record_Bytes()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        var csPath = NativeDb.Tmp("parity_cs.dbf");
        DbfFile.CreateNew(csPath, Encoding.GetEncoding(1252),
        [
            new DbfField("NAME", DbfFieldType.Character, 20, 0, 0),
            new DbfField("AGE", DbfFieldType.Numeric, 8, 0, 0),
        ]);
        var empty = new Dictionary<IIndexHandle, byte[]>();
        using (var engine = new DbfEngine(csPath, null, exclusive: true, LockingStrategy.IntraProcess))
        {
            engine.AppendRecord(new Dictionary<string, object?> { ["NAME"] = "ALICE", ["AGE"] = 30 }, empty);
            engine.AppendRecord(new Dictionary<string, object?> { ["NAME"] = "BOB", ["AGE"] = 21 }, empty);
            engine.DeleteCurrent(empty);
        }

        var cPath = NativeDb.Tmp("parity_c.dbf");
        File.Copy(csPath, cPath, overwrite: true);
        var h = NativeDb.DbfOpen(cPath);
        Assert.True(h > 0, NativeDb.ReadErr());
        NativeDb.AssertOk(NativeDb.DbfGotop(h));
        Assert.Equal(2, NativeDb.DbfLastrec(h));
        Assert.Equal(1, NativeDb.DbfRecno(h));
        Assert.Equal(0, NativeDb.DbfEof(h));
        Assert.Equal("ALICE", NativeDb.GetUtf8(NativeDb.DbfGetField, h, 0));
        Assert.Equal("30", NativeDb.GetUtf8(NativeDb.DbfGetField, h, 1));
        NativeDb.AssertOk(NativeDb.DbfSkip(h, 1));
        Assert.Equal("BOB", NativeDb.GetUtf8(NativeDb.DbfGetField, h, 0));
        Assert.Equal(1, NativeDb.DbfDeleted(h));
        NativeDb.AssertOk(NativeDb.DbfSkip(h, 1));
        Assert.Equal(1, NativeDb.DbfEof(h));
        NativeDb.AssertOk(NativeDb.DbfClose(h));

        var csBytes = File.ReadAllBytes(csPath);
        var cBytes = File.ReadAllBytes(cPath);
        Assert.Equal(csBytes.Length, cBytes.Length);
        Assert.Equal(csBytes.AsSpan(4).ToArray(), cBytes.AsSpan(4).ToArray());
    }

    [Fact]
    public void Dbf_C_Create_Append_Delete_Pack_Readable_By_Csharp()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        var path = NativeDb.Tmp("from_c.dbf");
        NativeDb.AssertOk(NativeDb.DbfCreate(path, "NAME:C:20,AGE:N:8:0"));
        var h = NativeDb.DbfOpen(path);
        NativeDb.AssertOk(NativeDb.DbfAppend(h, "ALICE,30"));
        NativeDb.AssertOk(NativeDb.DbfAppend(h, "BOB,21"));
        NativeDb.AssertOk(NativeDb.DbfGo(h, 2));
        NativeDb.AssertOk(NativeDb.DbfDelete(h));
        NativeDb.AssertOk(NativeDb.DbfPack(h));
        Assert.Equal(1, NativeDb.DbfLastrec(h));
        NativeDb.AssertOk(NativeDb.DbfClose(h));

        using var engine = new DbfEngine(path, null, exclusive: true, LockingStrategy.IntraProcess);
        engine.GoTop();
        Assert.Equal("ALICE", engine.GetCurrentRecordValues()!["NAME"]?.ToString()?.Trim());
        Assert.Equal(1, engine.GetTotalRecordCount());
    }

    [Fact]
    public void Dbf_Ntx_Ndx_Seek_SetOrder_Reindex()
    {
        var path = NativeDb.Tmp("idx.dbf");
        var ntx = NativeDb.Tmp("idx.ntx");
        var ndx = NativeDb.Tmp("idx.ndx");
        NativeDb.AssertOk(NativeDb.DbfCreate(path, "NAME:C:20,AGE:N:8:0"));
        var h = NativeDb.DbfOpen(path);
        NativeDb.AssertOk(NativeDb.DbfAppend(h, "BOB,21"));
        NativeDb.AssertOk(NativeDb.DbfAppend(h, "ALICE,30"));
        NativeDb.AssertOk(NativeDb.DbfAppend(h, "CAROL,40"));
        NativeDb.AssertOk(NativeDb.DbfIndexOn(h, "NAME", ntx, 0));
        Assert.Equal(0, NativeDb.DbfSeek(h, "ALICE"));
        Assert.Equal(1, NativeDb.DbfFound(h));
        Assert.Equal("ALICE", NativeDb.GetUtf8(NativeDb.DbfGetField, h, 0));
        NativeDb.AssertOk(NativeDb.DbfSetOrder(h, 1));
        NativeDb.AssertOk(NativeDb.DbfGotop(h));
        Assert.Equal("ALICE", NativeDb.GetUtf8(NativeDb.DbfGetField, h, 0));
        NativeDb.AssertOk(NativeDb.DbfSkip(h, 1));
        Assert.Equal("BOB", NativeDb.GetUtf8(NativeDb.DbfGetField, h, 0));
        NativeDb.AssertOk(NativeDb.DbfIndexOn(h, "NAME", ndx, 0));
        NativeDb.AssertOk(NativeDb.DbfReindex(h));
        Assert.Equal(0, NativeDb.DbfSeek(h, "CAROL"));
        Assert.Equal("CAROL", NativeDb.GetUtf8(NativeDb.DbfGetField, h, 0));
        NativeDb.AssertOk(NativeDb.DbfClose(h));
        Assert.True(new FileInfo(ntx).Length >= 1024);
        Assert.True(new FileInfo(ndx).Length >= 1024);
    }

    [Fact]
    public void Speed_Logical_Parity_Vs_Csharp()
    {
        var csDir = NativeDb.Tmp("spd_cs");
        var cDir = NativeDb.Tmp("spd_c");
        Directory.CreateDirectory(csDir);
        Directory.CreateDirectory(cDir);
        var key = Encoding.UTF8.GetBytes("user");
        var val = Encoding.UTF8.GetBytes("gaetano");
        using (var db = new global::Basic.Databases.Speed.Database(csDir, crossProcessLock: false))
        {
            db.Set(1, key, val);
            Assert.Equal(val, db.Get(1, key));
        }

        var h = NativeDb.SpeedOpen(cDir);
        var k = Marshal.AllocHGlobal(key.Length);
        var v = Marshal.AllocHGlobal(val.Length);
        var outBuf = Marshal.AllocHGlobal(64);
        try
        {
            Marshal.Copy(key, 0, k, key.Length);
            Marshal.Copy(val, 0, v, val.Length);
            NativeDb.AssertOk(NativeDb.SpeedSet(h, 1, k, key.Length, v, val.Length));
            var vlen = 64;
            NativeDb.AssertOk(NativeDb.SpeedGet(h, 1, k, key.Length, outBuf, ref vlen));
            var got = new byte[vlen];
            Marshal.Copy(outBuf, got, 0, vlen);
            Assert.Equal(val, got);
            NativeDb.AssertOk(NativeDb.SpeedDelete(h, 1, k, key.Length));
            vlen = 64;
            Assert.Equal(1, NativeDb.SpeedGet(h, 1, k, key.Length, outBuf, ref vlen));
            NativeDb.AssertOk(NativeDb.SpeedCompact(h));
        }
        finally
        {
            Marshal.FreeHGlobal(k);
            Marshal.FreeHGlobal(v);
            Marshal.FreeHGlobal(outBuf);
            NativeDb.SpeedClose(h);
        }
    }

    [Fact]
    public void Ledger_C_Journal_Readable_By_Csharp_And_Matches_Oracle_Shape()
    {
        var path = NativeDb.Tmp("cbook.dat");
        var h = NativeDb.LedgerOpen(path);
        var id = NativeDb.LedgerAddCsv(h, "2026-08-20", "seed", "Cash,Equity", "100,-100");
        Assert.Equal(1, id);
        Assert.Equal(1, NativeDb.LedgerCount(h));
        var dateBuf = Marshal.AllocHGlobal(16);
        var descBuf = Marshal.AllocHGlobal(64);
        try
        {
            NativeDb.AssertOk(NativeDb.LedgerGet(h, 1, dateBuf, 16, descBuf, 64));
            Assert.Equal("2026-08-20", Marshal.PtrToStringUTF8(dateBuf));
            Assert.Equal("seed", Marshal.PtrToStringUTF8(descBuf));
        }
        finally
        {
            Marshal.FreeHGlobal(dateBuf);
            Marshal.FreeHGlobal(descBuf);
        }
        NativeDb.AssertOk(NativeDb.LedgerClose(h));

        var text = File.ReadAllText(path, Encoding.UTF8);
        Assert.Contains("2026-08-20 seed", text);
        using var ctx = new LedgerContext(path);
        var csId = LedgerManager.AddTransaction(ctx, new DateOnly(2026, 8, 21), "follow",
        [
            new LedgerEntry { Description = "follow", Account = "Cash", Amount = 50m, AmountIsExplicit = true },
            new LedgerEntry { Description = "follow", Account = "Equity", Amount = -50m, AmountIsExplicit = true },
        ]);
        Assert.Equal(1, csId); /* C# index empty until reload — still appends as first in-memory */
        Assert.Contains("2026-08-21 follow", File.ReadAllText(path, Encoding.UTF8));
    }

    [Fact]
    public void Isam_Insert_Seek_ImportDbf()
    {
        var isamPath = NativeDb.Tmp("t.isam");
        NativeDb.AssertOk(NativeDb.IsamCreate(isamPath, "NAME:C:20,AGE:N:8:0"));
        var h = NativeDb.IsamOpen(isamPath);
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "ALICE,30"));
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "BOB,21"));
        Assert.Equal(2, NativeDb.IsamCount(h));
        Assert.Equal(0, NativeDb.IsamSeek(h, "BOB"));
        Assert.Equal("BOB", NativeDb.GetUtf8(NativeDb.IsamRead, h, 0));
        NativeDb.AssertOk(NativeDb.IsamReindex(h));
        NativeDb.AssertOk(NativeDb.IsamClose(h));

        var dbf = NativeDb.Tmp("imp.dbf");
        NativeDb.AssertOk(NativeDb.DbfCreate(dbf, "NAME:C:20,AGE:N:8:0"));
        var dh = NativeDb.DbfOpen(dbf);
        NativeDb.AssertOk(NativeDb.DbfAppend(dh, "CAROL,40"));
        NativeDb.AssertOk(NativeDb.DbfClose(dh));
        var h2 = NativeDb.IsamOpen(isamPath);
        NativeDb.AssertOk(NativeDb.IsamImportDbf(h2, dbf));
        Assert.Equal(3, NativeDb.IsamCount(h2));
        NativeDb.AssertOk(NativeDb.IsamClose(h2));
    }

    [Fact]
    public void Wal_Lock_Replica_Two_Wal_Modules()
    {
        var walPath = NativeDb.Tmp("dbf.wal");
        var wh = NativeDb.WalOpen(walPath);
        var payload = Marshal.AllocHGlobal(4);
        try
        {
            Marshal.WriteInt32(payload, 42);
            NativeDb.AssertOk(NativeDb.WalAppend(wh, 1, 32, payload, 4));
            NativeDb.AssertOk(NativeDb.WalCheckpoint(wh));
            Assert.True(NativeDb.WalReplayCount(wh) >= 2);
        }
        finally
        {
            Marshal.FreeHGlobal(payload);
            NativeDb.WalClose(wh);
        }

        var spdDir = NativeDb.Tmp("spd_wal");
        Directory.CreateDirectory(spdDir);
        var sh = NativeDb.SpeedWalOpen(spdDir);
        var k = Marshal.StringToHGlobalAnsi("k");
        var v = Marshal.StringToHGlobalAnsi("v");
        try
        {
            NativeDb.AssertOk(NativeDb.SpeedWalLogSet(sh, 1, k, 1, v, 1));
            NativeDb.AssertOk(NativeDb.SpeedWalLogDelete(sh, 1, k, 1));
        }
        finally
        {
            Marshal.FreeHGlobal(k);
            Marshal.FreeHGlobal(v);
            NativeDb.SpeedWalClose(sh);
        }
        Assert.True(File.Exists(Path.Combine(spdDir, "wal", "000001.wal")));

        var lockPath = NativeDb.Tmp("lock.dat");
        File.WriteAllText(lockPath, "x");
        var fd = NativeDb.LockFile(lockPath, 1);
        Assert.True(fd >= 0);
        NativeDb.Unlock(fd);

        var repl = NativeDb.Tmp("repl.log");
        NativeDb.AssertOk(NativeDb.ReplAppend(repl, "APPEND 1 NAME=ALICE"));
        NativeDb.AssertOk(NativeDb.ReplAppend(repl, "DELETE 1"));
        Assert.Equal(2, NativeDb.ReplCount(repl));
    }

    [Fact]
    public void Isam_Json_Lob_C_Writes_Csharp_Reads()
    {
        // Il motore C scrive un campo JsonType (LOB heap) → il motore C# lo rilegge.
        Basic.Databases.Rdd.DriverRegistry.Register("ISAM", typeof(Basic.Databases.Rdd.IsamDriver), [".isdb"]);
        var path = NativeDb.Tmp("json_c2cs.isdb");
        NativeDb.AssertOk(NativeDb.IsamCreate(path, "NAME:C:20,DATA:J"));
        var h = NativeDb.IsamOpen(path);
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "ALICE,\"{\"\"k\"\":7,\"\"v\"\":\"\"x\"\"}\""));
        NativeDb.AssertOk(NativeDb.IsamClose(h));

        var driver = Basic.Databases.Rdd.DriverRegistry.Create("ISAM")!;
        driver.Open(new Basic.Databases.Rdd.DbOpenInfo { FileName = path, Alias = "J" });
        driver.GoTop();
        Assert.Equal("ALICE", driver.GetFieldValue(driver.GetFieldIndex("Name"))?.ToString()?.Trim());
        var json = driver.GetFieldValue(driver.GetFieldIndex("Data"))?.ToString();
        Assert.NotNull(json);
        Assert.Contains("\"k\":7", json);
        Assert.Contains("\"v\":\"x\"", json);
        driver.Close();
    }

    [Fact]
    public void Syscall_Ordinals_Match_Compiler_And_Dispatch()
    {
        Assert.Equal(117, (int)SysCall.DBF_USE);
        Assert.Equal(65, (int)SysCall.ISAM_USE);
        Assert.Equal(163, (int)SysCall.SPEED_OPEN);
        Assert.Equal(181, (int)SysCall.LEDGER_OPEN);
        var path = NativeDb.Tmp("sys.dbf");
        NativeDb.AssertOk(NativeDb.DbfCreate(path, "NAME:C:10"));
        Assert.Equal(0, NativeDb.ApplySyscall(117, path, 0, out var handle));
        Assert.True(handle > 0);
        Assert.Equal(0, NativeDb.ApplySyscall(129, "", 1, out _));
        NativeDb.DbfClose(handle);
    }

    [Fact]
    public void C99_Emitter_And_Vm_Core_Wire_Commercial_Db()
    {
        var root = NativeDb.RepoRoot;
        // L'emissione .bas→C vive nel mondo C (native/gmb_c99native/src): cerca i
        // riferimenti al motore DB C in tutti i file dell'emitter.
        var c99Dir = Path.Combine(root, "native", "gmb_c99native", "src");
        var emitterText = string.Join("\n",
            Directory.EnumerateFiles(c99Dir, "*.c", SearchOption.AllDirectories)
                .Select(File.ReadAllText));
        Assert.Contains("gmb_db.h", emitterText);
        Assert.Contains("gmb_db_dbf_open", emitterText);
        var vm = File.ReadAllText(Path.Combine(root, "native", "gmb_vm_c", "gmb_vm_core.c"));
        Assert.Contains("GMB_HAS_COMMERCIAL_DB", vm);
        Assert.Contains("gmb_db_apply_syscall", vm);
        var svc = File.ReadAllText(Path.Combine(root, "common", "BRT", "Services", "NativeCommercialDb.cs"));
        Assert.Contains("class NativeCommercialDb", svc);
        Assert.Contains("gmb_db", svc);
    }

    [Fact]
    public void Isam_Integrity_C_Valid_And_Corrupt_Detected()
    {
        // ISAM.INTEGRITY() nel motore C: file valido -> {"valid":true}; header free-list
        // corrotto -> {"valid":false} (stessa semantica del C# IsamValidateJson).
        var path = NativeDb.Tmp("integrity_parity.isdb");
        NativeDb.AssertOk(NativeDb.IsamCreate(path, "NAME:C:20,DATA:J"));
        var h = NativeDb.IsamOpen(path);
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "ALICE,\"{\"\"k\"\":7}\""));
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "BOB,\"{\"\"k\"\":42}\""));

        var buf = new byte[2048];
        unsafe
        {
            fixed (byte* p = buf)
            {
                // file valido -> valid:true
                int rc = NativeDb.IsamIntegrity(h, (IntPtr)p, buf.Length);
                var json = System.Text.Encoding.UTF8.GetString(buf).TrimEnd('\0');
                Assert.Equal(0, rc);
                Assert.Contains("\"valid\":true", json);
            }
        }
        NativeDb.AssertOk(NativeDb.IsamClose(h));

        // corrompi PageFreeListHeadOffset (byte 88, pack=1) -> fuori heap -> valid:false
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Write))
        {
            fs.Position = 88;
            var bad = BitConverter.GetBytes((long)999999);
            fs.Write(bad, 0, bad.Length);
        }
        h = NativeDb.IsamOpen(path);
        Assert.True(h > 0);
        unsafe
        {
            fixed (byte* p = buf)
            {
                int rc = NativeDb.IsamIntegrity(h, (IntPtr)p, buf.Length);
                var json = System.Text.Encoding.UTF8.GetString(buf).TrimEnd('\0');
                Assert.NotEqual(0, rc);
                Assert.Contains("\"valid\":false", json);
            }
        }
        NativeDb.AssertOk(NativeDb.IsamClose(h));
    }


    [Fact]
    public void Isam_Abi_New_Features_C_And_Interop()
    {
        // Le nuove ABI ISAM (put/delete/go_top/go_bottom/recno/create_index/set_filter/
        // validate/info) funzionano via P/Invoke sul motore C.
        var path = NativeDb.Tmp("abi_new_features.isdb");
        NativeDb.AssertOk(NativeDb.IsamCreate(path, "NAME:C:20,DATA:J"));
        var h = NativeDb.IsamOpen(path);
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "ALICE,\"{\"\"k\"\":1}\""));
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "BOB,\"{\"\"k\"\":2}\""));
        NativeDb.AssertOk(NativeDb.IsamInsert(h, "CAROL,\"{\"\"k\"\":3}\""));

        NativeDb.AssertOk(NativeDb.IsamGoTop(h));
        Assert.Equal(1, NativeDb.IsamRecno(h));
        NativeDb.AssertOk(NativeDb.IsamGoBottom(h));
        Assert.Equal(3, NativeDb.IsamRecno(h));
        NativeDb.AssertOk(NativeDb.IsamPut(h, "CAROL2,\"{\"\"k\"\":30}\""));
        NativeDb.AssertOk(NativeDb.IsamCreateIndex(h, "BYNAME", "NAME", 20));

        var vbuf = new byte[1024];
        unsafe
        {
            fixed (byte* p = vbuf)
            {
                NativeDb.AssertOk(NativeDb.IsamValidate(h));
                NativeDb.AssertOk(NativeDb.IsamInfo(h, (IntPtr)p, vbuf.Length));
                var info = System.Text.Encoding.UTF8.GetString(vbuf).TrimEnd('\0');
                Assert.Contains("\"active_records\":3", info);
            }
        }
        NativeDb.AssertOk(NativeDb.IsamClose(h));

        // il file scritto dal C con le nuove operazioni resta leggibile dal C#
        Basic.Databases.Rdd.DriverRegistry.Register("ISAM", typeof(Basic.Databases.Rdd.IsamDriver), [".isdb"]);
        var driver = Basic.Databases.Rdd.DriverRegistry.Create("ISAM")!;
        driver.Open(new Basic.Databases.Rdd.DbOpenInfo { FileName = path, Alias = "N" });
        driver.GoTop();
        Assert.Equal("ALICE", driver.GetFieldValue(driver.GetFieldIndex("Name"))?.ToString()?.Trim());
        driver.Close();
    }


    [Fact]
    public void Dbf_Speed_Ledger_Abi_Extended()
    {
        // step tracer
        void A(int rc, string step) { if (rc != 0) throw new Xunit.Sdk.XunitException(step + " rc=" + rc); }
        // DBF esteso: append_blank/copy_to/sort_to/transazioni/zap/set_filter via P/Invoke.
        var dpath = NativeDb.Tmp("dbf_ext.dbf");
        NativeDb.AssertOk(NativeDb.DbfCreate(dpath, "ID:N:4,NAME:C:10"));
        var dh = NativeDb.DbfOpen(dpath);
        NativeDb.AssertOk(NativeDb.DbfAppend(dh, "1,ALICE"));
        NativeDb.AssertOk(NativeDb.DbfAppend(dh, "2,BOB"));
        A(NativeDb.DbfAppendBlank(dh), "append_blank");
        Assert.Equal(3, NativeDb.DbfLastrec(dh));
        A(NativeDb.DbfCopyTo(dh, NativeDb.Tmp("dbf_ext_copy.dbf")), "copy_to");
        A(NativeDb.DbfSortTo(dh, NativeDb.Tmp("dbf_ext_sort.dbf"), "NAME", 0), "sort_to");
        A(NativeDb.DbfBeginTxn(dh), "begin_txn");
        NativeDb.AssertOk(NativeDb.DbfAppend(dh, "9,DANA"));
        A(NativeDb.DbfRollback(dh), "rollback");
        Assert.Equal(3, NativeDb.DbfLastrec(dh));
        A(NativeDb.DbfBeginTxn(dh), "begin_txn");
        NativeDb.AssertOk(NativeDb.DbfAppend(dh, "9,DANA"));
        A(NativeDb.DbfCommit(dh), "commit");
        Assert.Equal(4, NativeDb.DbfLastrec(dh));
        A(NativeDb.DbfSetFilter(dh, ""), "set_filter");
        A(NativeDb.DbfZap(dh), "zap");
        Assert.Equal(0, NativeDb.DbfLastrec(dh));
        NativeDb.AssertOk(NativeDb.DbfClose(dh));

        // SPEED esteso: create_table/use/go_top/get_current/skip/validate.
        var spdDir = NativeDb.Tmp("speed_ext");
        if (Directory.Exists(spdDir)) Directory.Delete(spdDir, true);
        var sh = NativeDb.SpeedOpen(spdDir);
        int tid = NativeDb.SpeedCreateTable(sh, "Person", "Id:N:4,Name:C:20");
        Assert.True(tid > 0);
        A(NativeDb.SpeedUse(sh, tid), "speed_use");
        var k1 = new byte[] { 1, 0, 0, 0 };
        var v1 = System.Text.Encoding.UTF8.GetBytes("ALICE");
        var k2 = new byte[] { 2, 0, 0, 0 };
        var v2 = System.Text.Encoding.UTF8.GetBytes("BOB");
        unsafe
        {
            fixed (byte* kp1 = k1) fixed (byte* vp1 = v1) fixed (byte* kp2 = k2) fixed (byte* vp2 = v2)
            {
                NativeDb.AssertOk(NativeDb.SpeedSet(sh, tid, (IntPtr)kp1, k1.Length, (IntPtr)vp1, v1.Length));
                NativeDb.AssertOk(NativeDb.SpeedSet(sh, tid, (IntPtr)kp2, k2.Length, (IntPtr)vp2, v2.Length));
            }
        }
        A(NativeDb.SpeedGoTop(sh, tid), "speed_gotop");
        A(NativeDb.SpeedSkip(sh, tid, 1), "speed_skip");
        A(NativeDb.SpeedClose(sh), "speed_close");

        // LEDGER esteso: accounts/balance/register (JSON).
        var lpath = NativeDb.Tmp("ledger_ext.ledger");
        File.Delete(lpath);
        var idxp = Path.ChangeExtension(lpath, ".idx");
        if (File.Exists(idxp)) File.Delete(idxp);
        var lh = NativeDb.LedgerOpen(lpath);
        Assert.True(lh > 0);
        NativeDb.LedgerAddCsv(lh, "2026-01-01", "Apertura", "Cassa,Banca", "100,-100");
        NativeDb.LedgerAddCsv(lh, "2026-01-02", "Acquisto", "Cassa,Merce", "-30,30");
        var lbuf = new byte[2048];
        unsafe
        {
            fixed (byte* p = lbuf)
            {
                A(NativeDb.LedgerAccounts(lh, null, (IntPtr)p, lbuf.Length), "ledger_accounts");
                var acc = System.Text.Encoding.UTF8.GetString(lbuf).TrimEnd('\0');
                Assert.Contains("Cassa", acc);
                A(NativeDb.LedgerBalance(lh, "", (IntPtr)p, lbuf.Length), "ledger_balance");
                var bal = System.Text.Encoding.UTF8.GetString(lbuf).TrimEnd('\0');
                Assert.Contains("Cassa", bal);
                A(NativeDb.LedgerRegister(lh, "", (IntPtr)p, lbuf.Length), "ledger_register");
                var reg = System.Text.Encoding.UTF8.GetString(lbuf).TrimEnd('\0');
                Assert.Contains("TransactionId", reg);
            }
        }
        NativeDb.AssertOk(NativeDb.LedgerClose(lh));
    }
}
