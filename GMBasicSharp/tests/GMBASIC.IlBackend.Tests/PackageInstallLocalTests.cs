// InstallIntoAsync — spostamento cache <-> installazione locale, e verifica dell'artefatto.
// Regola (direttiva utente): stesso pacchetto+versione -> SPOSTA (copia+rimozione dalla
// sorgente); se e' gia' nella destinazione non si tocca nulla; se manca ovunque si scarica
// (qui non si testa la rete: si testano spostamento e integrita' dell'artefatto).
using System;
using System.IO;
using System.Threading.Tasks;
using Basic;
using Xunit;

namespace Basic.IlBackend.Tests;

public class PackageInstallLocalTests : IDisposable
{
    private readonly string _root;

    public PackageInstallLocalTests()
    {
        _root = Path.Combine(Path.GetTempPath(), "gmbinstr_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_root);
    }

    public void Dispose()
    {
        try { if (Directory.Exists(_root)) Directory.Delete(_root, true); } catch { }
    }

    /// <summary>Crea un pacchetto minimo con manifest, export e (opzionale) lib finta.</summary>
    private string MakePkg(string root, string name, string ver, bool withLib = true)
    {
        var dir = Path.Combine(root, name, ver);
        Directory.CreateDirectory(dir);
        // Manifest costruito con JsonSerializer: niente escaping a mano (le graffe annidate
        // rompono le raw-string interpolate).
        var manifest = new
        {
            name,
            version = ver,
            contentType = "c-ffi",
            exports = new[] { name + ".inc" },
            ffi = new
            {
                libs = new System.Collections.Generic.Dictionary<string, string>
                {
                    ["osx-arm64"] = $"lib/osx-arm64/lib{name}.dylib",
                    ["osx-x64"] = $"lib/osx-x64/lib{name}.dylib",
                    ["linux-x64"] = $"lib/linux-x64/lib{name}.so",
                    ["linux-arm64"] = $"lib/linux-arm64/lib{name}.so",
                    ["win-x64"] = $"lib/win-x64/{name}.dll",
                }
            }
        };
        File.WriteAllText(Path.Combine(dir, "gmb-package.json"),
            System.Text.Json.JsonSerializer.Serialize(manifest));
        File.WriteAllText(Path.Combine(dir, $"{name}.inc"), $"FUNCTION {name.ToUpperInvariant()}OK() AS LONG\nEND FUNCTION\n");
        if (withLib)
        {
            var plat = Parser.PackageManager.GetCurrentPlatformKey();
            var ext = OperatingSystem.IsWindows() ? ".dll" : OperatingSystem.IsMacOS() ? ".dylib" : ".so";
            var libDir = Path.Combine(dir, "lib", plat);
            Directory.CreateDirectory(libDir);
            File.WriteAllText(Path.Combine(libDir, $"lib{name}{ext}"), "finta");
        }
        return dir;
    }

    [Fact]
    public async Task Sposta_Dalla_Cache_Alla_Destinazione_Locale()
    {
        var cache = Path.Combine(_root, "cache");
        var local = Path.Combine(_root, "app", "packages");
        MakePkg(cache, "mipkg", "1.0.0");

        var pm = new Parser.PackageManager { CacheRoot = cache };
        var pkg = await pm.InstallIntoAsync("mipkg", "1.0.0", local, cache);   // sorgente esplicita (la deduzione usa GmbiPaths)

        Assert.True(File.Exists(Path.Combine(local, "mipkg", "1.0.0", "gmb-package.json")));
        Assert.True(File.Exists(Path.Combine(local, "mipkg", "1.0.0", "mipkg.inc")));
        Assert.False(Directory.Exists(Path.Combine(cache, "mipkg", "1.0.0")), "la versione deve essere SPOSTATA, non copiata");
        Assert.Equal(Path.Combine(local, "mipkg", "1.0.0"), pkg.InstallPath);
    }

    [Fact]
    public async Task Sposta_Dalla_Destinazione_Locale_Alla_Cache()
    {
        var cache = Path.Combine(_root, "cache");
        var local = Path.Combine(_root, "app", "packages");
        MakePkg(local, "mipkg", "2.0.0");

        var pm = new Parser.PackageManager { CacheRoot = cache };
        // target = cache, sorgente = installazione locale (dedotta: target non e' il locale)
        var pkg = await pm.InstallIntoAsync("mipkg", "2.0.0", cache, local);

        Assert.True(File.Exists(Path.Combine(cache, "mipkg", "2.0.0", "gmb-package.json")));
        Assert.False(Directory.Exists(Path.Combine(local, "mipkg", "2.0.0")));
        Assert.Equal(Path.Combine(cache, "mipkg", "2.0.0"), pkg.InstallPath);
    }

    [Fact]
    public async Task Se_E_Gia_Nella_Destinazione_Non_Tocca_Nulla()
    {
        var cache = Path.Combine(_root, "cache");
        var local = Path.Combine(_root, "app", "packages");
        var dest = MakePkg(local, "mipkg", "3.0.0");
        var marker = Path.Combine(dest, "mio-marker.txt");
        File.WriteAllText(marker, "non toccarmi");

        var pm = new Parser.PackageManager { CacheRoot = cache };
        await pm.InstallIntoAsync("mipkg", "3.0.0", local);

        Assert.True(File.Exists(marker), "un pacchetto gia' presente non deve essere riscritto");
        Assert.False(Directory.Exists(Path.Combine(cache, "mipkg")), "non deve comparire nulla in cache");
    }

    [Fact]
    public void ArtifactPresent_Rileva_La_Lib_Mancante()
    {
        var dir = MakePkg(Path.Combine(_root, "p"), "rotto", "1.0.0");
        Assert.True(Parser.PackageManager.ArtifactPresent(dir));

        var plat = Parser.PackageManager.GetCurrentPlatformKey();
        Directory.Delete(Path.Combine(dir, "lib", plat), true);
        Assert.False(Parser.PackageManager.ArtifactPresent(dir), "senza lib/<piattaforma> NON e' installato");
    }

    [Fact]
    public void ArtifactPresent_Rileva_Un_Export_Mancante()
    {
        var dir = MakePkg(Path.Combine(_root, "p"), "senzaexp", "1.0.0");
        File.Delete(Path.Combine(dir, "senzaexp.inc"));
        Assert.False(Parser.PackageManager.ArtifactPresent(dir));
    }
}

/// <summary>Firma dei pacchetti: UNA sola famiglia, verificabile. `--key` non è più HMAC
/// (simmetrica, non verificabile dal client): il segreto è derivato in una seed Ed25519.</summary>
public class PackageSignatureTests : IDisposable
{
    private readonly string _root;
    public PackageSignatureTests()
    {
        _root = Path.Combine(Path.GetTempPath(), "gmbsign_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_root);
    }
    public void Dispose() { try { if (Directory.Exists(_root)) Directory.Delete(_root, true); } catch { } }

    private string MakePkg()
    {
        var dir = Path.Combine(_root, "sigpkg", "1.0.0");
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "gmb-package.json"),
            "{\"name\":\"sigpkg\",\"version\":\"1.0.0\",\"contentType\":\"source\",\"exports\":[\"sigpkg.inc\"]}");
        File.WriteAllText(Path.Combine(dir, "sigpkg.inc"), "FUNCTION SIGOK() AS LONG\nEND FUNCTION\n");
        return dir;
    }

    [Fact]
    public void Pack_Con_Key_Produce_Firma_Ed25519_Verificabile()
    {
        var dir = MakePkg();
        var outFile = Path.Combine(_root, "sigpkg-1.0.0.gmbpkg");
        PackageBuilder.Pack(dir, outFile, null, "segreto-di-test");

        Assert.True(PackageBuilder.VerifyGmbpkg(outFile), "la firma derivata da --key deve verificare");
        using var zip = System.IO.Compression.ZipFile.OpenRead(outFile);
        Assert.NotNull(zip.GetEntry(".gmbpkg.sig"));
        Assert.NotNull(zip.GetEntry(".gmbpkg.pub"));
    }

    [Fact]
    public void La_Stessa_Key_Verifica_Sempre_Una_Key_Diversa_No()
    {
        var dir = MakePkg();
        var a = Path.Combine(_root, "a.gmbpkg");
        var b = Path.Combine(_root, "b.gmbpkg");
        PackageBuilder.Pack(dir, a, null, "segreto-1");
        PackageBuilder.Pack(dir, b, null, "segreto-2");
        Assert.True(PackageBuilder.VerifyGmbpkg(a));
        Assert.True(PackageBuilder.VerifyGmbpkg(b));

        // le due chiavi pubbliche devono essere DIVERSE (segreti diversi → seed diverse)
        using var za = System.IO.Compression.ZipFile.OpenRead(a);
        using var zb = System.IO.Compression.ZipFile.OpenRead(b);
        string Pub(System.IO.Compression.ZipArchive z)
        {
            using var r = new StreamReader(z.GetEntry(".gmbpkg.pub")!.Open());
            return r.ReadToEnd().Trim();
        }
        Assert.NotEqual(Pub(za), Pub(zb));
    }
}

/// <summary>Ordine di ricerca dei pacchetti: vendor -> installazione locale -> cache.
/// È la regola documentata e deve valere nel resolver (usato da IL/PCode/REPL), non solo
/// nel compilatore C: prima il vendor era DOPO la cache e un pin offline veniva scavalcato.</summary>
public class PackageSearchOrderTests : IDisposable
{
    private readonly string _root;
    public PackageSearchOrderTests()
    {
        _root = Path.Combine(Path.GetTempPath(), "gmborder_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_root);
    }
    public void Dispose() { try { if (Directory.Exists(_root)) Directory.Delete(_root, true); } catch { } }

    private void MakePkg(string root, string name, string ver)
    {
        var dir = Path.Combine(root, name, ver);
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "gmb-package.json"),
            "{\"name\":\"" + name + "\",\"version\":\"" + ver + "\",\"contentType\":\"source\",\"exports\":[\"" + name + ".inc\"]}");
        File.WriteAllText(Path.Combine(dir, name + ".inc"), "FUNCTION OK() AS LONG\nEND FUNCTION\n");
    }

    private string? ResolveVersion(string cache, string vendor, string local)
    {
        var (items, error) = NativePkg.Resolve(
            cache, vendor, "il", Parser.PackageManager.GetCurrentPlatformKey(), "Community", true, null,
            new[] { ("ordpkg", "*") }, Array.Empty<(string, string)>(), Array.Empty<string>(),
            Array.Empty<(string, string)>(), Array.Empty<(string, string)>(),
            Array.Empty<(string, string)>(), local);
        if (error != null) throw new InvalidOperationException(error);
        return items.Count > 0 ? items[0].Version : null;
    }

    [Fact]
    public void Vendor_Vince_Su_Locale_E_Cache()
    {
        var cache = Path.Combine(_root, "cache");
        var local = Path.Combine(_root, "app", "packages");
        var vendor = Path.Combine(_root, "app", "vendor");
        MakePkg(cache, "ordpkg", "1.0.0");
        MakePkg(local, "ordpkg", "2.0.0");
        MakePkg(vendor, "ordpkg", "3.0.0");

        Assert.Equal("3.0.0", ResolveVersion(cache, vendor, local));
    }

    [Fact]
    public void Senza_Vendor_Vince_Locale()
    {
        var cache = Path.Combine(_root, "cache");
        var local = Path.Combine(_root, "app", "packages");
        var vendor = Path.Combine(_root, "app", "vendor");
        Directory.CreateDirectory(vendor);            // esiste ma vuoto
        MakePkg(cache, "ordpkg", "1.0.0");
        MakePkg(local, "ordpkg", "2.0.0");

        Assert.Equal("2.0.0", ResolveVersion(cache, vendor, local));
    }

    [Fact]
    public void Senza_Vendor_Ne_Locale_Vince_La_Cache()
    {
        var cache = Path.Combine(_root, "cache");
        MakePkg(cache, "ordpkg", "1.0.0");
        Assert.Equal("1.0.0", ResolveVersion(cache, Path.Combine(_root, "app", "vendor"), Path.Combine(_root, "app", "packages")));
    }
}

/// <summary>Guard di `gmb cache clean`: non deve mai cancellare la cartella dei pacchetti del
/// progetto in un checkout. Si testa la REGOLA (pura), non il comando: un test non deve poter
/// cancellare la cache vera se la regola è sbagliata.</summary>
public class CacheCleanGuardTests
{
    [Fact]
    public void La_Cache_Del_Progetto_In_Checkout_E_Protetta()
    {
        Assert.True(Parser.PackageManager.CacheCleanBlocked(CommonLib.GmbiPaths.PackagesDir, force: false));
    }

    [Fact]
    public void Force_Scavalca_La_Protezione()
    {
        Assert.False(Parser.PackageManager.CacheCleanBlocked(CommonLib.GmbiPaths.PackagesDir, force: true));
    }

    [Fact]
    public void Una_Cache_Separata_Non_E_Protetta()
    {
        var tmp = Path.Combine(Path.GetTempPath(), "gmbcache_" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(tmp);
            Assert.False(Parser.PackageManager.CacheCleanBlocked(tmp, force: false));
        }
        finally { try { Directory.Delete(tmp, true); } catch { } }
    }
}
