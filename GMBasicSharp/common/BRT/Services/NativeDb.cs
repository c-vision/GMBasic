// FILE: Basic.Runtime.NativeDb
using System.Runtime.InteropServices;

namespace Basic.Runtime;

/// <summary>
/// P/Invoke verso <c>gmb_db</c> (database su file, sempre disponibile). Se la SHARED non è nel
/// search path, i driver su file restano indisponibili (errore esplicito, nessuno stub).
/// </summary>
internal static class NativeDb
{
    static NativeDb()
    {
        try { NativeLibrary.SetDllImportResolver(typeof(NativeDb).Assembly, Resolve); }
        catch (InvalidOperationException) { /* resolver già installato (ModuleInitializer) */ }
    }

    private static IntPtr Resolve(string name, System.Reflection.Assembly assembly, DllImportSearchPath? search)
        => CommonLib.GmbBoot.ResolveDllImport(name);
    private static int _init; /* 0 unknown, 1 ok, -1 missing */

    internal static bool TryInit()
    {
        if (_init != 0) return _init > 0;
        try
        {
            _ = Version();
            _init = 1;
            return true;
        }
        catch (DllNotFoundException)
        {
            _init = -1;
            return false;
        }
        catch (EntryPointNotFoundException)
        {
            _init = -1;
            return false;
        }
    }

    internal static T Invoke<T>(Func<T> call, Func<T> fallback)
    {
        if (!TryInit()) return fallback();
        try { return call(); }
        catch (DllNotFoundException) { return fallback(); }
        catch (EntryPointNotFoundException) { return fallback(); }
    }

    internal static void Invoke(Action call, Action fallback)
    {
        if (!TryInit()) { fallback(); return; }
        try { call(); }
        catch (DllNotFoundException) { fallback(); }
        catch (EntryPointNotFoundException) { fallback(); }
    }

    [DllImport("gmb_db", EntryPoint = "gmb_db_version", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Version();

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_open", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long DbfOpen([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_create", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfCreate([MarshalAs(UnmanagedType.LPUTF8Str)] string path, [MarshalAs(UnmanagedType.LPUTF8Str)] string fieldDefs);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_close", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfClose(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_append", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfAppend(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string csv);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_go", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfGo(long h, int recno);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_skip", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfSkip(long h, int n);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_eof", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfEof(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_bof", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfBof(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_recno", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfRecno(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_lastrec", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfLastrec(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_delete", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfDelete(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_recall", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfRecall(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_pack", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfPack(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_seek", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfSeek(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string key);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_set_order", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfSetOrder(long h, int order);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_reindex", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfReindex(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_gotop", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfGoTop(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_gobottom", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfGoBottom(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_deleted", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfDeleted(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_found", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfFound(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_field_count", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfFieldCount(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_field_name", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfFieldName(long h, int idx, System.Text.StringBuilder buf, int cap);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_get_field", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfGetField(long h, int idx, System.Text.StringBuilder buf, int cap);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_replace_field", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfReplaceField(long h, int idx, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);

    [DllImport("gmb_db", EntryPoint = "gmb_db_dbf_index_on", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int DbfIndexOn(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string field,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string indexPath, int unique);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_open", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long IsamOpen([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_create", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamCreate([MarshalAs(UnmanagedType.LPUTF8Str)] string path, [MarshalAs(UnmanagedType.LPUTF8Str)] string schema);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_close", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamClose(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_insert", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamInsert(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string csv);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_go", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamGo(long h, int recno);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_skip", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamSkip(long h, int n);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_seek", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamSeek(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string key);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_eof", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamEof(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_count", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamCount(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_read", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamRead(long h, int fieldIdx, System.Text.StringBuilder buf, int cap);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_pack", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamPack(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_reindex", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamReindex(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_isam_import_dbf", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int IsamImportDbf(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string dbfPath);

    [DllImport("gmb_db", EntryPoint = "gmb_db_speed_open", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long SpeedOpen([MarshalAs(UnmanagedType.LPUTF8Str)] string dir);

    [DllImport("gmb_db", EntryPoint = "gmb_db_speed_close", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SpeedClose(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_speed_set", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SpeedSet(long h, int tableId, byte[] key, int klen, [MarshalAs(UnmanagedType.LPUTF8Str)] string csv);

    [DllImport("gmb_db", EntryPoint = "gmb_db_speed_get", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SpeedGet(long h, int tableId, byte[] key, int klen, System.Text.StringBuilder buf, int cap);

    [DllImport("gmb_db", EntryPoint = "gmb_db_speed_delete", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SpeedDelete(long h, int tableId, byte[] key, int klen);

    [DllImport("gmb_db", EntryPoint = "gmb_db_speed_compact", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SpeedCompact(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_ledger_open", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long LedgerOpen([MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport("gmb_db", EntryPoint = "gmb_db_ledger_close", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int LedgerClose(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_ledger_add", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long LedgerAdd(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string date,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string desc, [MarshalAs(UnmanagedType.LPUTF8Str)] string accounts,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string amounts);

    [DllImport("gmb_db", EntryPoint = "gmb_db_ledger_add_csv", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long LedgerAddCsv(long h, [MarshalAs(UnmanagedType.LPUTF8Str)] string date,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string desc, [MarshalAs(UnmanagedType.LPUTF8Str)] string accounts,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string amounts);

    [DllImport("gmb_db", EntryPoint = "gmb_db_ledger_count", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int LedgerCount(long h);

    [DllImport("gmb_db", EntryPoint = "gmb_db_ledger_get", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int LedgerGet(long h, long txId, System.Text.StringBuilder dateBuf, int dcap,
        System.Text.StringBuilder descBuf, int scap, System.Text.StringBuilder accountsBuf, int acap,
        System.Text.StringBuilder amountsBuf, int tcap);
}
