/* FILE: native/gmb_db_c/src/ISAM/IsamBackupRestore.c */
/* Port 1:1 di modules/Database/ISAM/IsamBackupRestore.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamBackupRestore
//
//using System.Text;
//using System.Text.Json;
//using Basic.Tools.Isam;
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Backup/restore file-level MVP (Phase G4): copia <c>.isdb</c> + <c>.wal</c> con manifest fingerprint.
///// </summary>
//public static class IsamBackupRestore
//{
//    private const string ManifestFileName = "manifest.json";
//    private const string DatabaseCopyName = "database.isdb";
//    private const string WalCopyName = "database.wal";
//
//    /// <summary>
//    /// Crea backup incrementale file-level nella cartella destinazione.
//    /// Se header fingerprint e WAL fingerprint coincidono con l'ultimo manifest, salta la copia.
//    /// Se solo il WAL è cambiato, copia solo <c>database.wal</c> (non delta a blocchi).
//    /// </summary>
//    /// <returns><see langword="true"/> se i file sono stati copiati; <see langword="false"/> se già aggiornato.</returns>
//    public static bool BackupIncremental(string sourceIsdb, string destinationDirectory)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(sourceIsdb);
//        ArgumentException.ThrowIfNullOrWhiteSpace(destinationDirectory);
//
//        if (!File.Exists(sourceIsdb))
//        {
//            throw new FileNotFoundException("ISAM file not found.", sourceIsdb);
//        }
//
//        Directory.CreateDirectory(destinationDirectory);
//        var snapshot = IsamOfflineToolkit.ReadSnapshot(sourceIsdb);
//        var fingerprint = BuildFingerprint(sourceIsdb, snapshot.Header);
//        var walSource = Path.ChangeExtension(sourceIsdb, ".wal");
//        var walDest = Path.Combine(destinationDirectory, WalCopyName);
//        var walFingerprint = BuildWalFingerprint(walSource);
//        var previous = ReadLatestManifest(destinationDirectory);
//
//        if (previous is not null && FingerprintsEqual(previous.Fingerprint, fingerprint))
//        {
//            if (WalFingerprintsEqual(previous.Wal, walFingerprint))
//            {
//                return false;
//            }
//
//            CopyWalOnly(walSource, walDest, destinationDirectory, sourceIsdb, fingerprint, walFingerprint);
//            return true;
//        }
//
//        var dbDest = Path.Combine(destinationDirectory, DatabaseCopyName);
//        File.Copy(sourceIsdb, dbDest, overwrite: true);
//
//        if (File.Exists(walSource))
//        {
//            File.Copy(walSource, walDest, overwrite: true);
//        }
//        else if (File.Exists(walDest))
//        {
//            File.Delete(walDest);
//        }
//
//        var manifest = new BackupManifest(
//            DateTimeOffset.UtcNow,
//            Path.GetFullPath(sourceIsdb),
//            fingerprint,
//            File.Exists(walSource),
//            walFingerprint);
//
//        WriteManifest(destinationDirectory, manifest);
//        return true;
//    }
//
//    private static void CopyWalOnly(
//        string walSource,
//        string walDest,
//        string destinationDirectory,
//        string sourceIsdb,
//        BackupFingerprint fingerprint,
//        WalFingerprint walFingerprint)
//    {
//        if (File.Exists(walSource))
//        {
//            File.Copy(walSource, walDest, overwrite: true);
//        }
//        else if (File.Exists(walDest))
//        {
//            File.Delete(walDest);
//        }
//
//        var manifest = new BackupManifest(
//            DateTimeOffset.UtcNow,
//            Path.GetFullPath(sourceIsdb),
//            fingerprint,
//            File.Exists(walSource),
//            walFingerprint);
//        WriteManifest(destinationDirectory, manifest);
//    }
//
//    /// <summary>
//    /// Ripristina <c>.isdb</c> (e <c>.wal</c> se presente) dal backup verso <paramref name="targetIsdb"/>.
//    /// </summary>
//    public static void Restore(string backupDirectory, string targetIsdb)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(backupDirectory);
//        ArgumentException.ThrowIfNullOrWhiteSpace(targetIsdb);
//
//        if (!Directory.Exists(backupDirectory))
//        {
//            throw new DirectoryNotFoundException($"Backup directory not found: {backupDirectory}");
//        }
//
//        var manifest = ReadLatestManifest(backupDirectory)
//                       ?? throw new InvalidOperationException("Backup manifest not found.");
//
//        var dbSource = Path.Combine(backupDirectory, DatabaseCopyName);
//        if (!File.Exists(dbSource))
//        {
//            throw new FileNotFoundException("Backup database copy missing.", dbSource);
//        }
//
//        var targetDir = Path.GetDirectoryName(Path.GetFullPath(targetIsdb));
//        if (!string.IsNullOrEmpty(targetDir))
//        {
//            Directory.CreateDirectory(targetDir);
//        }
//
//        File.Copy(dbSource, targetIsdb, overwrite: true);
//
//        var walTarget = Path.ChangeExtension(targetIsdb, ".wal");
//        var walSource = Path.Combine(backupDirectory, WalCopyName);
//        if (File.Exists(walSource))
//        {
//            File.Copy(walSource, walTarget, overwrite: true);
//        }
//        else if (File.Exists(walTarget))
//        {
//            File.Delete(walTarget);
//        }
//
//        IsamOfflineToolkit.RecoverWal(targetIsdb);
//        _ = manifest;
//    }
//
//    internal static BackupFingerprint BuildFingerprint(string sourceIsdb, Config.FileHeader header)
//    {
//        var info = new FileInfo(sourceIsdb);
//        return new BackupFingerprint(
//            header.ActiveRecordCount,
//            header.PrimaryKeyRootOffset,
//            info.LastWriteTimeUtc.Ticks,
//            info.Length);
//    }
//
//    internal static WalFingerprint BuildWalFingerprint(string walPath)
//    {
//        if (!File.Exists(walPath))
//        {
//            return new WalFingerprint(0, 0);
//        }
//
//        var info = new FileInfo(walPath);
//        return new WalFingerprint(info.Length, info.LastWriteTimeUtc.Ticks);
//    }
//
//    private static void WriteManifest(string directory, BackupManifest manifest)
//    {
//        var path = Path.Combine(directory, ManifestFileName);
//        var json = JsonSerializer.Serialize(manifest, new JsonSerializerOptions { WriteIndented = true });
//        File.WriteAllText(path, json, Encoding.UTF8);
//    }
//
//    private static BackupManifest? ReadLatestManifest(string directory)
//    {
//        var path = Path.Combine(directory, ManifestFileName);
//        if (!File.Exists(path))
//        {
//            return null;
//        }
//
//        return JsonSerializer.Deserialize<BackupManifest>(File.ReadAllText(path, Encoding.UTF8));
//    }
//
//    private static bool FingerprintsEqual(BackupFingerprint a, BackupFingerprint b) =>
//        a.ActiveRecordCount == b.ActiveRecordCount
//        && a.PrimaryKeyRootOffset == b.PrimaryKeyRootOffset
//        && a.SourceLastWriteUtcTicks == b.SourceLastWriteUtcTicks
//        && a.SourceFileLength == b.SourceFileLength;
//
//    private static bool WalFingerprintsEqual(WalFingerprint? previous, WalFingerprint current) =>
//        previous is not null
//        && previous.Length == current.Length
//        && previous.LastWriteUtcTicks == current.LastWriteUtcTicks;
//}
//
///// <summary>Fingerprint header per backup incrementale MVP.</summary>
//public sealed record BackupFingerprint(
//    long ActiveRecordCount,
//    long PrimaryKeyRootOffset,
//    long SourceLastWriteUtcTicks,
//    long SourceFileLength);
//
///// <summary>Fingerprint file WAL per backup wal-only.</summary>
//public sealed record WalFingerprint(long Length, long LastWriteUtcTicks);
//
///// <summary>Manifest JSON scritto nella cartella backup.</summary>
//public sealed record BackupManifest(
//    DateTimeOffset CreatedUtc,
//    string SourcePath,
//    BackupFingerprint Fingerprint,
//    bool IncludesWal,
//    WalFingerprint? Wal = null);

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#define BAK_MANIFEST "manifest.json"
#define BAK_DB "database.isdb"
#define BAK_WAL "database.wal"

typedef struct BakFp {
    int64_t ActiveRecordCount;
    int64_t PrimaryKeyRootOffset;
    int64_t SourceLastWriteUtcTicks;
    int64_t SourceFileLength;
} BakFp;

typedef struct WalFp {
    int64_t Length;
    int64_t LastWriteUtcTicks;
} WalFp;

static int bak_exists(const char* path)
{
    return path && access(path, F_OK) == 0;
}

static void bak_wal_path(const char* isdb, char* out, int cap)
{
    int n;
    snprintf(out, (size_t)cap, "%s", isdb ? isdb : "");
    n = (int)strlen(out);
    if (n >= 5 && gmb_strcasecmp(out + n - 5, ".isdb") == 0)
        snprintf(out + n - 5, (size_t)(cap - (n - 5)), "%s", ".wal");
    else
        snprintf(out, (size_t)cap, "%s.wal", isdb ? isdb : "");
}

static int bak_mkdir(const char* dir)
{
    if (!dir || !dir[0]) return GMB_DB_ARG;
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return GMB_DB_ERR;
    return GMB_DB_OK;
}

static int bak_copy(const char* src, const char* dst)
{
    FILE *in, *out;
    char buf[8192];
    size_t n;
    if (!src || !dst) return GMB_DB_ARG;
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

static int bak_stat_ticks(const char* path, int64_t* length, int64_t* ticks)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) {
        if (length) *length = 0;
        if (ticks) *ticks = 0;
        return GMB_DB_ERR;
    }
    if (length) *length = (int64_t)st.st_size;
    if (ticks) *ticks = (int64_t)st.st_mtime * 10000000LL;
    return GMB_DB_OK;
}

static int bak_read_header(const char* isdb, IsamFileHeader* h)
{
    FILE* f;
    if (!isdb || !h) return GMB_DB_ARG;
    f = fopen(isdb, "rb");
    if (!f) return GMB_DB_ERR;
    if (IsamFileHeader_Read(f, h) != GMB_DB_OK) { fclose(f); return GMB_DB_ERR; }
    fclose(f);
    return GMB_DB_OK;
}

static void bak_build_fp(const char* isdb, const IsamFileHeader* h, BakFp* fp)
{
    int64_t len = 0, ticks = 0;
    memset(fp, 0, sizeof(*fp));
    if (h) {
        fp->ActiveRecordCount = h->ActiveRecordCount;
        fp->PrimaryKeyRootOffset = h->PrimaryKeyRootOffset;
    }
    bak_stat_ticks(isdb, &len, &ticks);
    fp->SourceFileLength = len;
    fp->SourceLastWriteUtcTicks = ticks;
}

static void bak_build_wal_fp(const char* wal, WalFp* fp)
{
    memset(fp, 0, sizeof(*fp));
    if (bak_exists(wal))
        bak_stat_ticks(wal, &fp->Length, &fp->LastWriteUtcTicks);
}

static int bak_fp_eq(const BakFp* a, const BakFp* b)
{
    return a && b
        && a->ActiveRecordCount == b->ActiveRecordCount
        && a->PrimaryKeyRootOffset == b->PrimaryKeyRootOffset
        && a->SourceLastWriteUtcTicks == b->SourceLastWriteUtcTicks
        && a->SourceFileLength == b->SourceFileLength;
}

static int bak_wal_eq(const WalFp* a, const WalFp* b)
{
    return a && b && a->Length == b->Length && a->LastWriteUtcTicks == b->LastWriteUtcTicks;
}

static int bak_json_i64(const char* json, const char* key, int64_t* out)
{
    char pat[64];
    const char* p;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    *out = (int64_t)strtoll(p + 1, NULL, 10);
    return 1;
}

static int bak_read_manifest(const char* dir, BakFp* fp, WalFp* wal, int* has)
{
    char path[PATH_MAX], buf[2048];
    FILE* f;
    size_t n;
    snprintf(path, sizeof(path), "%s/%s", dir, BAK_MANIFEST);
    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    if (has) *has = 1;
    memset(fp, 0, sizeof(*fp));
    memset(wal, 0, sizeof(*wal));
    bak_json_i64(buf, "ActiveRecordCount", &fp->ActiveRecordCount);
    bak_json_i64(buf, "PrimaryKeyRootOffset", &fp->PrimaryKeyRootOffset);
    bak_json_i64(buf, "SourceLastWriteUtcTicks", &fp->SourceLastWriteUtcTicks);
    bak_json_i64(buf, "SourceFileLength", &fp->SourceFileLength);
    bak_json_i64(buf, "Length", &wal->Length);
    bak_json_i64(buf, "LastWriteUtcTicks", &wal->LastWriteUtcTicks);
    return 1;
}

static int bak_write_manifest(const char* dir, const char* sourceIsdb, const BakFp* fp,
                              int includesWal, const WalFp* wal)
{
    char path[PATH_MAX];
    FILE* f;
    snprintf(path, sizeof(path), "%s/%s", dir, BAK_MANIFEST);
    f = fopen(path, "wb");
    if (!f) return GMB_DB_ERR;
    fprintf(f,
            "{\n"
            "  \"CreatedUtc\": \"1970-01-01T00:00:00+00:00\",\n"
            "  \"SourcePath\": \"%s\",\n"
            "  \"Fingerprint\": {\n"
            "    \"ActiveRecordCount\": %lld,\n"
            "    \"PrimaryKeyRootOffset\": %lld,\n"
            "    \"SourceLastWriteUtcTicks\": %lld,\n"
            "    \"SourceFileLength\": %lld\n"
            "  },\n"
            "  \"IncludesWal\": %s,\n"
            "  \"Wal\": { \"Length\": %lld, \"LastWriteUtcTicks\": %lld }\n"
            "}\n",
            sourceIsdb ? sourceIsdb : "",
            (long long)fp->ActiveRecordCount,
            (long long)fp->PrimaryKeyRootOffset,
            (long long)fp->SourceLastWriteUtcTicks,
            (long long)fp->SourceFileLength,
            includesWal ? "true" : "false",
            (long long)(wal ? wal->Length : 0),
            (long long)(wal ? wal->LastWriteUtcTicks : 0));
    fclose(f);
    return GMB_DB_OK;
}

static int bak_sync_wal(const char* walSource, const char* walDest)
{
    if (bak_exists(walSource)) return bak_copy(walSource, walDest);
    if (bak_exists(walDest)) unlink(walDest);
    return GMB_DB_OK;
}

int IsamBackupRestore_BackupIncremental(const char* sourceIsdb, const char* destinationDirectory)
{
    IsamFileHeader h;
    BakFp fp, prev;
    WalFp walFp, prevWal;
    char walSrc[PATH_MAX], dbDest[PATH_MAX], walDest[PATH_MAX];
    int hasPrev = 0;
    if (!sourceIsdb || !sourceIsdb[0] || !destinationDirectory || !destinationDirectory[0])
        return GMB_DB_ARG;
    if (!bak_exists(sourceIsdb)) return GMB_DB_ERR;
    if (bak_mkdir(destinationDirectory) != GMB_DB_OK) return GMB_DB_ERR;
    if (bak_read_header(sourceIsdb, &h) != GMB_DB_OK) return GMB_DB_ERR;
    bak_build_fp(sourceIsdb, &h, &fp);
    bak_wal_path(sourceIsdb, walSrc, (int)sizeof(walSrc));
    bak_build_wal_fp(walSrc, &walFp);
    snprintf(dbDest, sizeof(dbDest), "%s/%s", destinationDirectory, BAK_DB);
    snprintf(walDest, sizeof(walDest), "%s/%s", destinationDirectory, BAK_WAL);
    if (bak_read_manifest(destinationDirectory, &prev, &prevWal, &hasPrev) && bak_fp_eq(&prev, &fp)) {
        if (bak_wal_eq(&prevWal, &walFp)) return 0;
        if (bak_sync_wal(walSrc, walDest) != GMB_DB_OK) return GMB_DB_ERR;
        if (bak_write_manifest(destinationDirectory, sourceIsdb, &fp, bak_exists(walSrc), &walFp) != GMB_DB_OK)
            return GMB_DB_ERR;
        return 1;
    }
    if (bak_copy(sourceIsdb, dbDest) != GMB_DB_OK) return GMB_DB_ERR;
    if (bak_sync_wal(walSrc, walDest) != GMB_DB_OK) return GMB_DB_ERR;
    if (bak_write_manifest(destinationDirectory, sourceIsdb, &fp, bak_exists(walSrc), &walFp) != GMB_DB_OK)
        return GMB_DB_ERR;
    return 1;
}

int IsamBackupRestore_Restore(const char* backupDirectory, const char* targetIsdb)
{
    char dbSrc[PATH_MAX], walSrc[PATH_MAX], walTgt[PATH_MAX], parent[PATH_MAX];
    BakFp fp;
    WalFp wal;
    const char* slash;
    int n;
    if (!backupDirectory || !targetIsdb) return GMB_DB_ARG;
    if (!bak_exists(backupDirectory)) return GMB_DB_ERR;
    if (!bak_read_manifest(backupDirectory, &fp, &wal, NULL)) return GMB_DB_ERR;
    snprintf(dbSrc, sizeof(dbSrc), "%s/%s", backupDirectory, BAK_DB);
    if (!bak_exists(dbSrc)) return GMB_DB_ERR;
    snprintf(parent, sizeof(parent), "%s", targetIsdb);
    slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        n = (int)(slash - parent);
        parent[n] = 0;
        if (bak_mkdir(parent) != GMB_DB_OK) return GMB_DB_ERR;
    }
    if (bak_copy(dbSrc, targetIsdb) != GMB_DB_OK) return GMB_DB_ERR;
    snprintf(walSrc, sizeof(walSrc), "%s/%s", backupDirectory, BAK_WAL);
    bak_wal_path(targetIsdb, walTgt, (int)sizeof(walTgt));
    if (bak_sync_wal(walSrc, walTgt) != GMB_DB_OK) return GMB_DB_ERR;
    WriteAheadLog_Recover(targetIsdb);
    return GMB_DB_OK;
}

#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#endif

/* Pulizia ricorsiva (best-effort) della dir di backup tra run successive:
 * senza, il secondo run vede un backup incrementale già completo e fallisce (-233). */
static void isam_rmrf(const char* path)
{
#ifdef _WIN32
    remove(path);
#else
    DIR* d = opendir(path);
    if (!d) { remove(path); return; }
    struct dirent* e;
    char full[1024];
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (e->d_type == DT_DIR) isam_rmrf(full);
        else remove(full);
    }
    closedir(d);
    rmdir(path);
#endif
}

int IsamBackupRestore_SelfCheck(void)
{
    char src[] = "native/gmb_db_c/build/isam_bak_src.isdb";
    char dir[] = "native/gmb_db_c/build/isam_bak_dir";
    char dst[] = "native/gmb_db_c/build/isam_bak_restored.isdb";
    isam_rmrf(dir);
    remove(src);
    remove(dst);
    IsamFieldSchema fields[2];
    IsamIndexDefinition sec;
    IsamSchema schema;
    Engine *e, *r;
    IsamFieldValue vals[2];
    int i, rc;
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    memset(&sec, 0, sizeof(sec));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    snprintf(sec.Name, sizeof(sec.Name), "%s", "Name");
    snprintf(sec.KeyExpression, sizeof(sec.KeyExpression), "%s", "Name");
    sec.KeyLength = 20;
    schema.SecondaryKeys = &sec;
    schema.SecondaryKeyCount = 1;
    if (IsamFileHandle_CreateNew(src, &schema) != GMB_DB_OK) return -230;
    e = Engine_Open(src, &schema);
    if (!e) return -231;
    for (i = 1; i <= 3; i++) {
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "N%d", i);
        memset(vals, 0, sizeof(vals));
        vals[0].i64 = i;
        vals[1].text = nbuf;
        if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -232; }
    }
    Engine_Dispose(e);
    rc = IsamBackupRestore_BackupIncremental(src, dir);
    if (rc != 1) return -233;
    rc = IsamBackupRestore_BackupIncremental(src, dir);
    if (rc != 0) return -234;
    if (IsamBackupRestore_Restore(dir, dst) != GMB_DB_OK) return -235;
    r = Engine_Open(dst, &schema);
    if (!r) return -236;
    if (Engine_GetTotalRecordCount(r) != 3) { Engine_Dispose(r); return -237; }
    Engine_Dispose(r);
    return GMB_DB_OK;
}
