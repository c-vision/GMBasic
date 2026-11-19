// FILE: Basic.Database.Native.Tests.NativeDb

using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Basic.Database.Native.Tests;

/// <summary>
/// Compila <c>native/gmb_db_c</c> in una SHARED e la carica via <see cref="NativeLibrary"/>.
/// </summary>
internal static class NativeDb
{
    private static readonly object Gate = new();
    private static IntPtr _lib;
    private static bool _ready;

    internal static string RepoRoot
    {
        get
        {
            var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
            while (dir != null)
            {
                if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx")))
                    return dir.FullName;
                dir = dir.Parent;
            }

            return Directory.GetCurrentDirectory();
        }
    }

    internal static void EnsureLoaded()
    {
        lock (Gate)
        {
            if (_ready) return;
            var root = RepoRoot;
            var srcDir = Path.Combine(root, "GMBasic", "native", "gmb_db_c");
            var outDir = Path.Combine(Path.GetTempPath(), "gmb_db_native");
            Directory.CreateDirectory(outDir);
            var libName = OperatingSystem.IsWindows() ? "gmb_db.dll"
                : OperatingSystem.IsMacOS() ? "libgmb_db.dylib" : "libgmb_db.so";
            var libPath = Path.Combine(outDir, libName);
            var inc = Path.Combine(srcDir, "include");
            var rtInc = Path.Combine(root, "GMBasic", "native", "gmb_runtime_c", "include");
            var sharedFlag = OperatingSystem.IsMacOS() ? "-dynamiclib" : "-shared";
            // Preferisce le lib statiche pre-buildate (cmake build, aggiornate e veloci);
            // se mancano, compila i sorgenti gmb_db_c (fallback lento per bootstrap).
            var dbA = Path.Combine(srcDir, "build", "libgmb_db.a");
            var rtA = Path.Combine(srcDir, "build", "gmb_runtime_c_from_db", "libgmb_runtime.a");
            string cmd;
            if (File.Exists(dbA) && File.Exists(rtA))
            {
                // force-load SOLO la lib db (per esportare l'ABI gmb_db_*); il runtime
                // resta linkato in modo incrementale (tira solo gli oggetti necessari,
                // non graphics/tui/network che richiedono SDL ecc.).
                var dbForce = OperatingSystem.IsMacOS()
                    ? $"-Wl,-force_load,\"{dbA}\""
                    : $"-Wl,--whole-archive \"{dbA}\" -Wl,--no-whole-archive";
                cmd = $"cc -std=c99 -fPIC {sharedFlag} -I\"{inc}\" -I\"{rtInc}\" {dbForce} \"{rtA}\" -lz -lm -lpthread -o \"{libPath}\"";
            }
            else
            {
                var sources = Directory.GetFiles(Path.Combine(srcDir, "src"), "*.c", SearchOption.AllDirectories)
                    .OrderBy(f => f, StringComparer.Ordinal)
                    .Select(s => $"\"{s}\"")
                    .ToArray();
                var common = Path.Combine(srcDir, "src", "common");
                cmd = $"cc -std=c99 -fPIC {sharedFlag} -Wall -I\"{inc}\" -I\"{common}\" -I\"{rtInc}\" {string.Join(" ", sources)} -o \"{libPath}\"";
            }
            Run("/bin/sh", ["-c", cmd], outDir);
            _lib = NativeLibrary.Load(libPath);
            Bind();
            _ready = true;
        }
    }

    private static void Bind()
    {
        Version = Get<VersionDel>("gmb_db_version");
        DbfCreate = Get<DbfCreateDel>("gmb_db_dbf_create");
        DbfOpen = Get<DbfOpenDel>("gmb_db_dbf_open");
        DbfClose = Get<DbfCloseDel>("gmb_db_dbf_close");
        DbfGotop = Get<DbfCloseDel>("gmb_db_dbf_gotop");
        DbfSkip = Get<DbfSkipDel>("gmb_db_dbf_skip");
        DbfGo = Get<DbfGoDel>("gmb_db_dbf_go");
        DbfEof = Get<DbfEofDel>("gmb_db_dbf_eof");
        DbfBof = Get<DbfEofDel>("gmb_db_dbf_bof");
        DbfFound = Get<DbfEofDel>("gmb_db_dbf_found");
        DbfRecno = Get<DbfEofDel>("gmb_db_dbf_recno");
        DbfLastrec = Get<DbfEofDel>("gmb_db_dbf_lastrec");
        DbfDeleted = Get<DbfEofDel>("gmb_db_dbf_deleted");
        DbfFieldCount = Get<DbfEofDel>("gmb_db_dbf_field_count");
        DbfFieldName = Get<DbfGetFieldDel>("gmb_db_dbf_field_name");
        DbfGetField = Get<DbfGetFieldDel>("gmb_db_dbf_get_field");
        DbfAppend = Get<DbfAppendDel>("gmb_db_dbf_append");
        DbfReplace = Get<DbfReplaceDel>("gmb_db_dbf_replace_field");
        DbfDelete = Get<DbfCloseDel>("gmb_db_dbf_delete");
        DbfRecall = Get<DbfCloseDel>("gmb_db_dbf_recall");
        DbfPack = Get<DbfCloseDel>("gmb_db_dbf_pack");
        DbfIndexOn = Get<DbfIndexOnDel>("gmb_db_dbf_index_on");
        DbfSetOrder = Get<DbfGoDel>("gmb_db_dbf_set_order");
        DbfSeek = Get<DbfAppendDel>("gmb_db_dbf_seek");
        DbfReindex = Get<DbfCloseDel>("gmb_db_dbf_reindex");
        SpeedOpen = Get<DbfOpenDel>("gmb_db_speed_open");
        SpeedClose = Get<DbfCloseDel>("gmb_db_speed_close");
        SpeedSet = Get<SpeedSetDel>("gmb_db_speed_set");
        SpeedGet = Get<SpeedGetDel>("gmb_db_speed_get");
        SpeedDelete = Get<SpeedDelDel>("gmb_db_speed_delete");
        SpeedCompact = Get<DbfCloseDel>("gmb_db_speed_compact");
        LedgerOpen = Get<DbfOpenDel>("gmb_db_ledger_open");
        DbfAppendBlank = Get<DbfCloseDel>("gmb_db_dbf_append_blank");
        DbfCopyTo = Get<DbfCopyToDel>("gmb_db_dbf_copy_to");
        DbfSortTo = Get<DbfSortToDel>("gmb_db_dbf_sort_to");
        DbfZap = Get<DbfCloseDel>("gmb_db_dbf_zap");
        DbfSetFilter = Get<DbfAppendDel>("gmb_db_dbf_set_filter");
        DbfBeginTxn = Get<DbfCloseDel>("gmb_db_dbf_begin_transaction");
        DbfCommit = Get<DbfCloseDel>("gmb_db_dbf_commit");
        DbfRollback = Get<DbfCloseDel>("gmb_db_dbf_rollback");
        SpeedCreateTable = Get<SpeedCreateTableDel>("gmb_db_speed_create_table");
        SpeedUse = Get<DbfGoDel>("gmb_db_speed_use");
        SpeedGoTop = Get<DbfGoDel>("gmb_db_speed_go_top");
        SpeedSkip = Get<SpeedSkipDel>("gmb_db_speed_skip");
        SpeedGetCurrent = Get<SpeedGetCurrentDel>("gmb_db_speed_get_current");
        LedgerAccounts = Get<LedgerQDel>("gmb_db_ledger_accounts");
        LedgerBalance = Get<LedgerQDel>("gmb_db_ledger_balance");
        LedgerRegister = Get<LedgerQDel>("gmb_db_ledger_register");
        LedgerClose = Get<DbfCloseDel>("gmb_db_ledger_close");
        LedgerAddCsv = Get<LedgerAddCsvDel>("gmb_db_ledger_add_csv");
        LedgerCount = Get<DbfEofDel>("gmb_db_ledger_count");
        LedgerGet = Get<LedgerGetDel>("gmb_db_ledger_get");
        IsamCreate = Get<DbfCreateDel>("gmb_db_isam_create");
        IsamOpen = Get<DbfOpenDel>("gmb_db_isam_open");
        IsamClose = Get<DbfCloseDel>("gmb_db_isam_close");
        IsamInsert = Get<DbfAppendDel>("gmb_db_isam_insert");
        IsamGo = Get<DbfGoDel>("gmb_db_isam_go");
        IsamSkip = Get<DbfSkipDel>("gmb_db_isam_skip");
        IsamSeek = Get<DbfAppendDel>("gmb_db_isam_seek");
        IsamEof = Get<DbfEofDel>("gmb_db_isam_eof");
        IsamCount = Get<DbfEofDel>("gmb_db_isam_count");
        IsamRead = Get<DbfGetFieldDel>("gmb_db_isam_read");
        IsamPack = Get<DbfCloseDel>("gmb_db_isam_pack");
        IsamReindex = Get<DbfCloseDel>("gmb_db_isam_reindex");
        IsamImportDbf = Get<DbfAppendDel>("gmb_db_isam_import_dbf");
        IsamIntegrity = Get<IsamIntegrityDel>("gmb_db_isam_integrity");
        IsamGoTop = Get<DbfCloseDel>("gmb_db_isam_go_top");
        IsamGoBottom = Get<DbfCloseDel>("gmb_db_isam_go_bottom");
        IsamRecno = Get<DbfEofDel>("gmb_db_isam_recno");
        IsamPut = Get<DbfAppendDel>("gmb_db_isam_put");
        IsamDelete = Get<DbfCloseDel>("gmb_db_isam_delete");
        IsamSetFilter = Get<DbfAppendDel>("gmb_db_isam_set_filter");
        IsamValidate = Get<DbfCloseDel>("gmb_db_isam_validate");
        IsamInfo = Get<IsamIntegrityDel>("gmb_db_isam_info");
        IsamCreateIndex = Get<IsamCreateIndexDel>("gmb_db_isam_create_index");
        WalOpen = Get<DbfOpenDel>("gmb_db_wal_open");
        WalAppend = Get<WalAppendDel>("gmb_db_wal_append");
        WalCheckpoint = Get<DbfCloseDel>("gmb_db_wal_checkpoint");
        WalReplayCount = Get<DbfEofDel>("gmb_db_wal_replay_count");
        WalClose = Get<DbfCloseDel>("gmb_db_wal_close");
        SpeedWalOpen = Get<DbfOpenDel>("gmb_db_speed_wal_open");
        SpeedWalLogSet = Get<SpeedSetDel>("gmb_db_speed_wal_log_set");
        SpeedWalLogDelete = Get<SpeedDelDel>("gmb_db_speed_wal_log_delete");
        SpeedWalClose = Get<DbfCloseDel>("gmb_db_speed_wal_close");
        LockFile = Get<LockDel>("gmb_db_lock_file");
        Unlock = Get<DbfEofDel>("gmb_db_unlock");
        ReplAppend = Get<DbfCreateDel>("gmb_db_repl_append");
        ReplCount = Get<ReplCountDel>("gmb_db_repl_count");
        ApplySyscall = Get<SyscallDel>("gmb_db_apply_syscall");
        LastError = Get<LastErrDel>("gmb_db_last_error");
    }

    private static T Get<T>(string name) where T : Delegate =>
        NativeLibrary.GetExport(_lib, name) == IntPtr.Zero
            ? throw new EntryPointNotFoundException(name)
            : Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(_lib, name));

    private static void Run(string file, string[] args, string workdir)
    {
        var psi = new ProcessStartInfo(file)
        {
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            WorkingDirectory = workdir,
        };
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = Process.Start(psi)!;
        var outp = p.StandardOutput.ReadToEnd();
        var err = p.StandardError.ReadToEnd();
        p.WaitForExit();
        if (p.ExitCode != 0)
            throw new InvalidOperationException($"cc failed ({p.ExitCode}): {err}\n{outp}");
    }

    internal delegate int VersionDel();
    internal delegate int DbfCreateDel([MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string defs);
    internal delegate long DbfOpenDel([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    internal delegate int DbfCloseDel(long h);
    internal delegate int DbfSkipDel(long h, int n);
    internal delegate int DbfGoDel(long h, int recno);
    internal delegate int DbfEofDel(long h);
    internal delegate int DbfGetFieldDel(long h, int idx, IntPtr buf, int cap);
    internal delegate int DbfAppendDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string csv);
    internal delegate int IsamIntegrityDel(long h, IntPtr buf, int cap);
    internal delegate int DbfCopyToDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string dest);
    internal delegate int DbfSortToDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string dest,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string field, int desc);
    internal delegate int SpeedCreateTableDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string typeName,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string fields);
    internal delegate int SpeedSkipDel(long h, int tableId, int count);
    internal delegate int SpeedGetCurrentDel(long h, int tableId, IntPtr buf, ref int vlen);
    internal delegate int LedgerQDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string? q, IntPtr buf, int cap);
    internal delegate int IsamCreateIndexDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string name,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string expr, int keylen);
    internal delegate int DbfReplaceDel(long h, int idx, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);
    internal delegate int DbfIndexOnDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string field,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path, int unique);
    internal delegate int SpeedSetDel(long h, int tableId, IntPtr key, int klen, IntPtr val, int vlen);
    internal delegate int SpeedGetDel(long h, int tableId, IntPtr key, int klen, IntPtr val, ref int vlen);
    internal delegate int SpeedDelDel(long h, int tableId, IntPtr key, int klen);
    internal delegate long LedgerAddCsvDel(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string date,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string desc, [MarshalAs(UnmanagedType.LPUTF8Str)] string accounts,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string amounts);
    internal delegate int LedgerGetDel(long h, long txId, IntPtr dateBuf, int dcap, IntPtr descBuf, int cap);
    internal delegate int WalAppendDel(long h, int op, long offset, IntPtr data, int len);
    internal delegate int LockDel([MarshalAs(UnmanagedType.LPUTF8Str)] string path, int exclusive);
    internal delegate int ReplCountDel([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    internal delegate int SyscallDel(int code, [MarshalAs(UnmanagedType.LPUTF8Str)] string? sarg, long iarg, out long result);
    internal delegate int LastErrDel(IntPtr buf, int cap);

    internal static VersionDel Version = null!;
    internal static DbfCreateDel DbfCreate = null!;
    internal static DbfOpenDel DbfOpen = null!;
    internal static DbfCloseDel DbfClose = null!;
    internal static DbfCloseDel DbfGotop = null!;
    internal static DbfSkipDel DbfSkip = null!;
    internal static DbfGoDel DbfGo = null!;
    internal static DbfEofDel DbfEof = null!;
    internal static DbfEofDel DbfBof = null!;
    internal static DbfEofDel DbfFound = null!;
    internal static DbfEofDel DbfRecno = null!;
    internal static DbfEofDel DbfLastrec = null!;
    internal static DbfEofDel DbfDeleted = null!;
    internal static DbfEofDel DbfFieldCount = null!;
    internal static DbfGetFieldDel DbfFieldName = null!;
    internal static DbfGetFieldDel DbfGetField = null!;
    internal static DbfAppendDel DbfAppend = null!;
    internal static DbfReplaceDel DbfReplace = null!;
    internal static DbfCloseDel DbfDelete = null!;
    internal static DbfCloseDel DbfRecall = null!;
    internal static DbfCloseDel DbfPack = null!;
    internal static DbfIndexOnDel DbfIndexOn = null!;
    internal static DbfGoDel DbfSetOrder = null!;
    internal static DbfAppendDel DbfSeek = null!;
    internal static DbfCloseDel DbfReindex = null!;
    internal static DbfOpenDel SpeedOpen = null!;
    internal static DbfCloseDel SpeedClose = null!;
    internal static SpeedSetDel SpeedSet = null!;
    internal static SpeedGetDel SpeedGet = null!;
    internal static SpeedDelDel SpeedDelete = null!;
    internal static DbfCloseDel SpeedCompact = null!;
    internal static DbfOpenDel LedgerOpen = null!;
    internal static DbfCloseDel DbfAppendBlank = null!;
    internal static DbfCopyToDel DbfCopyTo = null!;
    internal static DbfSortToDel DbfSortTo = null!;
    internal static DbfCloseDel DbfZap = null!;
    internal static DbfAppendDel DbfSetFilter = null!;
    internal static DbfCloseDel DbfBeginTxn = null!;
    internal static DbfCloseDel DbfCommit = null!;
    internal static DbfCloseDel DbfRollback = null!;
    internal static SpeedCreateTableDel SpeedCreateTable = null!;
    internal static DbfGoDel SpeedUse = null!;
    internal static DbfGoDel SpeedGoTop = null!;
    internal static SpeedSkipDel SpeedSkip = null!;
    internal static SpeedGetCurrentDel SpeedGetCurrent = null!;
    internal static LedgerQDel LedgerAccounts = null!;
    internal static LedgerQDel LedgerBalance = null!;
    internal static LedgerQDel LedgerRegister = null!;
    internal static DbfCloseDel LedgerClose = null!;
    internal static LedgerAddCsvDel LedgerAddCsv = null!;
    internal static DbfEofDel LedgerCount = null!;
    internal static LedgerGetDel LedgerGet = null!;
    internal static DbfCreateDel IsamCreate = null!;
    internal static DbfOpenDel IsamOpen = null!;
    internal static DbfCloseDel IsamClose = null!;
    internal static DbfAppendDel IsamInsert = null!;
    internal static DbfGoDel IsamGo = null!;
    internal static DbfSkipDel IsamSkip = null!;
    internal static DbfAppendDel IsamSeek = null!;
    internal static DbfEofDel IsamEof = null!;
    internal static DbfEofDel IsamCount = null!;
    internal static DbfGetFieldDel IsamRead = null!;
    internal static DbfCloseDel IsamPack = null!;
    internal static DbfCloseDel IsamReindex = null!;
    internal static DbfAppendDel IsamImportDbf = null!;
    internal static IsamIntegrityDel IsamIntegrity = null!;
    internal static DbfCloseDel IsamGoTop = null!;
    internal static DbfCloseDel IsamGoBottom = null!;
    internal static DbfEofDel IsamRecno = null!;
    internal static DbfAppendDel IsamPut = null!;
    internal static DbfCloseDel IsamDelete = null!;
    internal static DbfAppendDel IsamSetFilter = null!;
    internal static DbfCloseDel IsamValidate = null!;
    internal static IsamIntegrityDel IsamInfo = null!;
    internal static IsamCreateIndexDel IsamCreateIndex = null!;
    internal static DbfOpenDel WalOpen = null!;
    internal static WalAppendDel WalAppend = null!;
    internal static DbfCloseDel WalCheckpoint = null!;
    internal static DbfEofDel WalReplayCount = null!;
    internal static DbfCloseDel WalClose = null!;
    internal static DbfOpenDel SpeedWalOpen = null!;
    internal static SpeedSetDel SpeedWalLogSet = null!;
    internal static SpeedDelDel SpeedWalLogDelete = null!;
    internal static DbfCloseDel SpeedWalClose = null!;
    internal static LockDel LockFile = null!;
    internal static DbfEofDel Unlock = null!;
    internal static DbfCreateDel ReplAppend = null!;
    internal static ReplCountDel ReplCount = null!;
    internal static SyscallDel ApplySyscall = null!;
    internal static LastErrDel LastError = null!;

    internal static string GetUtf8(DbfGetFieldDel fn, long h, int idx)
    {
        var buf = Marshal.AllocHGlobal(256);
        try
        {
            AssertOk(fn(h, idx, buf, 256));
            return Marshal.PtrToStringUTF8(buf) ?? "";
        }
        finally
        {
            Marshal.FreeHGlobal(buf);
        }
    }

    internal static void AssertOk(int rc)
    {
        if (rc != 0)
            throw new InvalidOperationException($"native rc={rc} err={ReadErr()}");
    }

    internal static string ReadErr()
    {
        var buf = Marshal.AllocHGlobal(256);
        try
        {
            LastError(buf, 256);
            return Marshal.PtrToStringUTF8(buf) ?? "";
        }
        finally
        {
            Marshal.FreeHGlobal(buf);
        }
    }

    internal static string Tmp(string name) =>
        Path.Combine(Path.GetTempPath(), "gmb_db_fix_" + Guid.NewGuid().ToString("N")[..8] + "_" + name);
}
