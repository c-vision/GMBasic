// FILE: Basic.SQLite.SqlCsvExtension
using System.Runtime.InteropServices;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite;

/// <summary>Carica le estensioni SQLite esterne (virtual table `csv` e `parquet`) sulla
/// connessione corrente. Sorgente unico §0bis: `native/gmb_runtime_c/src/gmb_csv_vtab.c` e
/// `gmb_parquet_vtab.c`, compilati come MODULE (i backend C li registrano staticamente dal
/// runtime condiviso). Caricamento OPZIONALE e guardato: se un'estensione o il supporto
/// load-extension non ci sono, quella feature resta non disponibile — nessuna regressione.</summary>
public static class SqlCsvExtension
{
    public static void TryLoad(SqliteConnection connection)
    {
        ArgumentNullException.ThrowIfNull(connection);
        TryLoadOne(connection, "gmb_sql_csv_ext", "sqlite3_gmbcsvext_init");
        TryLoadOne(connection, "gmb_sql_parquet_ext", "sqlite3_gmbparquetext_init");
    }

    private static void TryLoadOne(SqliteConnection connection, string baseName, string entryPoint)
    {
        try
        {
            var path = ResolvePath(baseName);
            if (path is null) return;
            connection.LoadExtension(path, entryPoint);
        }
        catch (Exception)
        {
            /* opzionale: provider senza load-extension o file mancante → feature off */
        }
    }

    private static string? ResolvePath(string baseName)
    {
        var env = Environment.GetEnvironmentVariable(baseName == "gmb_sql_parquet_ext" ? "GMB_SQL_PARQUET_EXT" : "GMB_SQL_CSV_EXT");
        if (!string.IsNullOrEmpty(env) && File.Exists(env)) return env;

        var rid = Rid();
        var name = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? baseName + ".dll" : baseName + ".dylib";
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux)) name = baseName + ".so";

        var bases = new[]
        {
            Directory.GetCurrentDirectory(),
            Path.Combine(Directory.GetCurrentDirectory(), ".."),
            Path.Combine(Directory.GetCurrentDirectory(), "..", ".."),
            Path.Combine(Directory.GetCurrentDirectory(), "..", "..", ".."),
            Path.Combine(Directory.GetCurrentDirectory(), "..", "..", "..", ".."),
            AppContext.BaseDirectory,
        };
        var rids = new[] { rid, "osx-arm64", "osx-x64", "linux-x64", "linux-arm64", "win-x64" };
        foreach (var b in bases)
            foreach (var r in rids)
            {
                var p = Path.GetFullPath(Path.Combine(b, "Resources", "libs", r, name));
                if (File.Exists(p)) return p;
                var mono = Path.GetFullPath(Path.Combine(b, "GMBasic", "Resources", "libs", r, name));
                if (File.Exists(mono)) return mono;
                /* Artefatti di BUILD: `<root>/build/libs/<rid>/` è dove finiscono le
                 * estensioni appena compilate (gmb_lib/CMake) e ha la precedenza sulla
                 * cache — stesso ordine del loader FFI C (gmb_ffi_dl.c). Senza questo,
                 * `sql/csv_vtab` e `parquet_*` davano "no such module: csv" nelle gambe
                 * managed (il file esisteva solo in build/libs). */
                var buildMono = Path.GetFullPath(Path.Combine(b, "GMBasic", "build", "libs", r, name));
                if (File.Exists(buildMono)) return buildMono;
                var buildRoot = Path.GetFullPath(Path.Combine(b, "build", "libs", r, name));
                if (File.Exists(buildRoot)) return buildRoot;
            }
        return null;
    }

    private static string Rid()
    {
        var os = RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "osx"
            : RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "win"
            : "linux";
        var arch = RuntimeInformation.ProcessArchitecture switch
        {
            Architecture.Arm64 => "arm64",
            Architecture.X64 => "x64",
            Architecture.X86 => "x86",
            _ => "unknown",
        };
        return $"{os}-{arch}";
    }
}
