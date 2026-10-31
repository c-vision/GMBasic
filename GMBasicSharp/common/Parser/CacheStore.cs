// FILE: Interpreter.Parser.CacheStore.cs
// Cache content-addressable: dedup dei file dei pacchetti tramite un blob store per contenuto
// (<contentRoot>/<sha[:2]>/<sha>) e link (hardlink → symlink → copia) al posto dei file duplicati.
// Il LAYOUT della cache resta invariato: <CacheRoot>/<name>/<ver>/ continuano a essere file "veri"
// (linkati ai blob), così resolver/lock/vendor/integrità non cambiano comportamento.
using System.Runtime.InteropServices;

namespace Basic;

/// <summary>Store content-addressable per la cache pacchetti (dedup + integrità per costruzione).</summary>
public static class CacheStore
{
    public const string DirName = ".content";

    /// <summary>Interna un file: lo memorizza come blob (sha256) e lo sostituisce con un link
    /// (hardlink → symlink → copia). Delegato al riferimento C (gmb_pkg_content_intern).</summary>
    public static string? Intern(string contentRoot, string filePath)
        => File.Exists(filePath) ? NativePkg.ContentIntern(contentRoot, filePath) : null;

    /// <summary>Interna tutti i file di una directory (ricorsivo). Delegato al C.</summary>
    public static int InternDir(string contentRoot, string dir) => NativePkg.ContentInternDir(contentRoot, dir);

    /// <summary>Rimuove i blob non più referenziati. Delegato al C.</summary>
    public static int Gc(string contentRoot, string cacheRoot) => NativePkg.ContentGc(contentRoot, cacheRoot);
}
