/* FILE: native/gmb_db_c/src/ISAM/SnapshotManager.c */
/* Port 1:1 di modules/Database/ISAM/SnapshotManager.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.SnapshotManager.cs
//
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.ISAM;
//
///// <summary>
///// Fornisce la funzionalità per creare una copia di backup sicura e consistente
///// di un file di database ISAM.
///// </summary>
//internal static class SnapshotManager
//{
//    /// <summary>
//    /// Crea uno snapshot consistente di un file di database ISAM.
//    /// La procedura garantisce che il backup sia atomico e non corrotto,
//    /// gestendo il flush dello stream e il locking del file.
//    /// </summary>
//    /// <param name="handle">L'handle del file ISAM aperto di cui fare il backup.</param>
//    /// <param name="backupFilePath">Il percorso completo del file di backup da creare.</param>
//    public static void CreateSnapshot(IsamFileHandle handle, string backupFilePath)
//    {
//        handle.LockFile();
//        try
//        {
//            if (IsamMemPath.IsMemPath(handle.FilePath))
//            {
//                throw new NotSupportedException("MEM: tables cannot be snapshotted via file copy.");
//            }
//
//            handle.FlushPhysical(true);
//            File.Copy(handle.FilePath, backupFilePath, overwrite: true);
//        }
//        finally
//        {
//            handle.UnlockFile();
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <sys/file.h>
#include <unistd.h>

static int snap_copy(const char* src, const char* dst)
{
    FILE *in, *out;
    char buf[8192];
    size_t n;
    in = fopen(src, "rb");
    if (!in) return GMB_DB_ERR;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return GMB_DB_ERR; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return GMB_DB_ERR; }
    }
    fclose(in);
    fclose(out);
    return GMB_DB_OK;
}

int SnapshotManager_CreateSnapshot(IsamFileHandle* handle, const char* backupFilePath)
{
    int rc, locked = 0;
    if (!handle || !backupFilePath || !backupFilePath[0]) return GMB_DB_ARG;
    if (IsamMemPath_IsMemPath(handle->FilePath)) return GMB_DB_ERR;
    if (handle->fs) {
        flock(fileno(handle->fs), LOCK_EX);
        locked = 1;
    }
    rc = IsamFileHandle_Flush(handle);
    if (rc == GMB_DB_OK)
        rc = snap_copy(handle->FilePath, backupFilePath);
    if (locked) flock(fileno(handle->fs), LOCK_UN);
    return rc;
}

static void snap_person_schema(IsamFieldSchema fields[2], IsamSchema* schema)
{
    memset(schema, 0, sizeof(*schema));
    memset(fields, 0, sizeof(IsamFieldSchema) * 2);
    snprintf(schema->TypeName, sizeof(schema->TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema->Fields = fields;
    schema->FieldCount = 2;
    snprintf(schema->PrimaryKey.Name, sizeof(schema->PrimaryKey.Name), "%s", "Id");
    snprintf(schema->PrimaryKey.KeyExpression, sizeof(schema->PrimaryKey.KeyExpression), "%s", "Id");
    schema->PrimaryKey.KeyLength = 10;
    schema->PrimaryKey.IsUnique = 1;
}

int SnapshotManager_SelfCheck(void)
{
    char src[] = "native/gmb_db_c/build/isam_snap_src.isdb";
    char bak[] = "native/gmb_db_c/build/isam_snap_bak.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    Engine *e, *b;
    IsamFieldValue vals[2];
    int i;
    snap_person_schema(fields, &schema);
    if (IsamFileHandle_CreateNew(src, &schema) != GMB_DB_OK) return -260;
    e = Engine_Open(src, &schema);
    if (!e) return -261;
    for (i = 1; i <= 3; i++) {
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "N%d", i);
        memset(vals, 0, sizeof(vals));
        vals[0].i64 = i;
        vals[1].text = nbuf;
        if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -262; }
    }
    if (Engine_CreateSnapshot(e, bak) != GMB_DB_OK) { Engine_Dispose(e); return -263; }
    Engine_Dispose(e);
    b = Engine_Open(bak, &schema);
    if (!b) return -264;
    if (Engine_GetTotalRecordCount(b) != 3) { Engine_Dispose(b); return -265; }
    Engine_Dispose(b);
    if (!IsamMemPath_IsMemPath("MEM:Foo") || IsamMemPath_IsMemPath("x.isdb")) return -266;
    return GMB_DB_OK;
}
