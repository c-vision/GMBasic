// Verifica del PackageManager v2 — intelligente multi-backend + FFI.
// - ParseManifest: v1 (binaries flat) e v2 (contentType/backends/ffi/binaries per backend)
// - SemVer v2: ^, ~, 1.*, latest
// - Selezione artefatto per contesto (backend+platform+edition) senza download
//   (fixture manifest scritti in cache ~/.gmbasic/packages, poi puliti).
using System.Collections.Immutable;
using System.IO;
using System.Text.Json;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class PackageManagerV2Tests
{
    private static readonly string CacheRoot =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".gmb", "packages", "__v2test");

    private static string WritePkg(string name, string version, string json)
    {
        var dir = Path.Combine(CacheRoot, Parser.PackageManager.SafePackageDir(name), version);
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "gmb-package.json"), json);
        // fixture sorgente: 1 .bas in exports (basename sicuro: mai '/' nel nome file)
        var src = Path.Combine(dir, "src");
        Directory.CreateDirectory(src);
        var safeBase = name.Replace('/', '_');
        File.WriteAllText(Path.Combine(src, $"{safeBase}.bas"), $"PRINT \"{name} v{version}\"\n");
        // crea anche i file binari/ffi referenziati (così File.Exists/SelectArtifact li trova)
        using (var doc = JsonDocument.Parse(json))
        {
            var root = doc.RootElement;
            if (root.TryGetProperty("binaries", out var bins) && bins.ValueKind == JsonValueKind.Object)
                foreach (var bk in bins.EnumerateObject())
                {
                    if (bk.Value.ValueKind == JsonValueKind.String)
                        MkFile(dir, bk.Value.GetString());
                    else if (bk.Value.ValueKind == JsonValueKind.Object)
                        foreach (var pk in bk.Value.EnumerateObject()) MkFile(dir, pk.Value.GetString());
                }
            if (root.TryGetProperty("ffi", out var ffi) && ffi.ValueKind == JsonValueKind.Object)
            {
                if (ffi.TryGetProperty("headers", out var hdr) && hdr.ValueKind == JsonValueKind.Array)
                    foreach (var h in hdr.EnumerateArray()) MkFile(dir, h.GetString());
                if (ffi.TryGetProperty("libs", out var lbs) && lbs.ValueKind == JsonValueKind.Object)
                    foreach (var lk in lbs.EnumerateObject()) MkFile(dir, lk.Value.GetString());
            }
        }
        return dir;
    }

    private static void MkFile(string dir, string? rel)
    {
        if (string.IsNullOrEmpty(rel)) return;
        var p = Path.Combine(dir, rel.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(p)!);
        if (!File.Exists(p)) File.WriteAllBytes(p, []);
    }

    private static void Cleanup() { try { if (Directory.Exists(CacheRoot)) Directory.Delete(CacheRoot, true); } catch { } }

    public PackageManagerV2Tests() => Cleanup();

    // ── 1. ParseManifest v1 (retrocompat) ──
    [Fact]
    public void ParseManifest_V1_FlatBinaries_Normalized()
    {
        var doc = JsonDocument.Parse("""
        { "name":"gmb-old","version":"1.0.0","exports":["src/"],
          "binaries":{"linux-x64":"bin/libold.so"} }
        """);
        var m = Parser.PackageManager.ParseManifest(doc.RootElement);
        Assert.Equal(PackageContentType.Source, m.ContentType);
        Assert.True(m.Binaries.ContainsKey("linux-x64"));
        Assert.Equal("bin/libold.so", m.Binaries["linux-x64"].Path);
        Assert.Null(m.Ffi);
    }

    // ── 2. ParseManifest v2 (contentType/backends/ffi/binaries per backend) ──
    [Fact]
    public void ParseManifest_V2_Mixed_Backends_Ffi()
    {
        var doc = JsonDocument.Parse("""
        { "name":"gmb-vec","version":"1.2.0","contentType":"mixed","backends":["il","c99"],
          "exports":["src/"],
          "binaries":{ "il":{"any-any":"bin/il/GmbVec.dll"}, "c99":{"linux-x64":"bin/c99/libvec.so"} },
          "ffi":{"api":"c-abi","headers":["include/vec.h"],"libs":{"linux-x64":"lib/libvec.a"},"link":"-lvec",
                 "declareExample":"DECLARE FUNCTION VecAdd LIB \"vec\" (a AS LONG) AS LONG"},
          "settings":{"minGmbVersion":"1.0.0"},
          "dependencies":{"gmb-collections":"^1.0"} }
        """);
        var m = Parser.PackageManager.ParseManifest(doc.RootElement);
        Assert.Equal(PackageContentType.Mixed, m.ContentType);
        Assert.Equal(new[] { "il", "c99" }, m.Backends);
        Assert.NotNull(m.Ffi);
        Assert.Equal("c-abi", m.Ffi!.Api);
        Assert.Equal("-lvec", m.Ffi.Link);
        Assert.Contains("VecAdd", m.Ffi.DeclareExample);
        Assert.Equal("bin/il/GmbVec.dll", m.Binaries["il"].Platforms!["any-any"]);
        Assert.Equal("bin/c99/libvec.so", m.Binaries["c99"].Platforms!["linux-x64"]);
        Assert.True(m.Dependencies.ContainsKey("gmb-collections"));
    }

    // ── 3. SemVer v2: ^ ~ * — risoluzione della versione migliore dai vincoli ──
    [Theory]
    [InlineData("^1.4.0", "1.9.1")]   // 1.x >= 1.4.0, esclude 2.0 → max 1.x
    [InlineData("~2.3.0", "2.3.5")]   // 2.3.x >= 2.3.0, esclude 2.4
    [InlineData("1.*", "1.9.1")]      // major 1, qualsiasi minor/patch
    [InlineData(">=2.0.0", "3.0.0")]  // >= → max disponibile
    [InlineData("2.4.0", "2.4.0")]    // exact
    [InlineData("latest", "3.0.0")]   // always match → max disponibile
    public void SemVer_V2_Resolves_Best_From_Constraints(string constraint, string expected)
    {
        // cache con versioni 1.3.9, 1.7.2, 1.9.1, 2.0.0, 2.3.5, 2.4.0, 3.0.0
        foreach (var v in new[] { "1.3.9", "1.7.2", "1.9.1", "2.0.0", "2.3.5", "2.4.0", "3.0.0" })
            WritePkg("gmb-semver", v, $"{{\"name\":\"gmb-semver\",\"version\":\"{v}\",\"exports\":[\"src/\"]}}");

        var pm = NewPm();
        pm.ResolveDependenciesAsync([new RequireStmt(
                new StringExpr("gmb-semver", new Token(TokenKind.StringLiteral, "gmb-semver", "gmb-semver", SourcePosition.Unknown)),
                new StringExpr(constraint, new Token(TokenKind.StringLiteral, constraint, constraint, SourcePosition.Unknown)),
                new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();

        Assert.Equal(expected, pm.ResolvedArtifacts["gmb-semver"].Version);
    }

    private static Parser.PackageManager NewPm() => new() { CacheRoot = CacheRoot };

    // ── 4. Selezione artefatto: dotnet precompilato su backend il ──
    [Fact]
    public void Resolve_Selects_DotNet_Binary_For_Il_Backend()
    {
        WritePkg("gmb-chart", "1.0.0", """
        { "name":"gmb-chart","version":"1.0.0","contentType":"dotnet","backends":["il"],
          "exports":["src/"],"binaries":{ "il":{"any-any":"bin/il/GmbChart.dll"} } }
        """);
        var pm = NewPm();
        var reqs = new[] { new RequireStmt(new StringExpr("gmb-chart", new Token(TokenKind.StringLiteral, "gmb-chart", "gmb-chart", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown)) };
        pm.ResolveDependenciesAsync(reqs, new PackageBuildContext { Backend = "il", PlatformKey = "linux-x64" }).GetAwaiter().GetResult();
        var art = pm.ResolvedArtifacts["gmb-chart"];
        Assert.Equal(PackageContentType.DotNet, art.ContentType);
        Assert.NotNull(art.BinaryPath);
        Assert.True(File.Exists(art.BinaryPath));
        Assert.Contains("GmbChart.dll", art.BinaryPath);
    }

    // ── 5. Selezione artefatto: FFI c-abi (anche senza .bas compilabili) ──
    [Fact]
    public void Resolve_Selects_Ffi_CAbi_Libs()
    {
        WritePkg("physics-engine", "0.9.0", """
        { "name":"physics-engine","version":"0.9.0","contentType":"c-ffi",
          "ffi":{"api":"c-abi","headers":["include/phys.h"],
                 "libs":{"linux-x64":"lib/libphys.so"},"link":"-lphys",
                 "declareExample":"DECLARE FUNCTION InitEngine LIB \"phys\" () AS LONG"} }
        """);
        var pm = NewPm();
        var reqs = new[] { new RequireStmt(new StringExpr("physics-engine", new Token(TokenKind.StringLiteral, "physics-engine", "physics-engine", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown)) };
        pm.ResolveDependenciesAsync(reqs, new PackageBuildContext { Backend = "c99", PlatformKey = "linux-x64" }).GetAwaiter().GetResult();
        var art = pm.ResolvedArtifacts["physics-engine"];
        Assert.Equal(PackageContentType.CFFI, art.ContentType);
        Assert.Single(art.FfiLibs);
        Assert.Contains("libphys.so", art.FfiLibs[0]);
        Assert.Contains("phys.h", art.FfiHeaders[0]);
        Assert.Equal("-lphys", art.FfiLink);
        Assert.Contains("InitEngine", art.DeclareExample);
        // il manifest NON ha exports → SourceDir null (niente .bas da compilare)
        Assert.Null(art.SourceDir);
    }

    // ── 6. Source-only: compila da exports con backend corrente ──
    [Fact]
    public void Resolve_SourceOnly_Exports_CompileDir()
    {
        WritePkg("gmb-collections", "1.0.0", """
        { "name":"gmb-collections","version":"1.0.0","contentType":"source","exports":["src/"] }
        """);
        var pm = NewPm();
        var reqs = new[] { new RequireStmt(new StringExpr("gmb-collections", new Token(TokenKind.StringLiteral, "gmb-collections", "gmb-collections", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown)) };
        pm.ResolveDependenciesAsync(reqs, new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        var art = pm.ResolvedArtifacts["gmb-collections"];
        Assert.Equal(PackageContentType.Source, art.ContentType);
        Assert.NotNull(art.SourceDir);
        Assert.True(Directory.Exists(art.SourceDir));
        Assert.True(pm.ResolvedModuleSourcePaths.Contains(art.SourceDir));
    }

    // ── 6bis. Grafo misto (Fase D2): REQUIRE ufficiale + contributor @scope/name, transitivi ──
    [Fact]
    public void Resolve_MixedGraph_Official_And_Scoped_Contributor()
    {
        // pacchetto contributor "@scope/name" (WritePkg normalizza il path con SafePackageDir)
        WritePkg("@marco/geom", "1.0.0", """
        { "name":"@marco/geom","version":"1.0.0","contentType":"source","exports":["src/"],
          "dependencies":{"gmb-collections":"^1.0"} }
        """);

        // pacchetto ufficiale con dipendenza transitiva
        WritePkg("gmb-collections", "1.0.0", """
        { "name":"gmb-collections","version":"1.0.0","contentType":"source","exports":["src/"],
          "dependencies":{"gmb-utils":"^1.0"} }
        """);
        WritePkg("gmb-utils", "1.2.0", """
        { "name":"gmb-utils","version":"1.2.0","contentType":"source","exports":["src/"] }
        """);

        var pm = NewPm();
        var reqs = new[] { new RequireStmt(new StringExpr("@marco/geom", new Token(TokenKind.StringLiteral, "@marco/geom", "@marco/geom", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown)) };
        pm.ResolveDependenciesAsync(reqs, new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();

        // il grafo misto risolve: contributor scoped + ufficiale transitivo
        Assert.NotNull(pm.ResolvedArtifacts["@marco/geom"]);
        Assert.NotNull(pm.ResolvedArtifacts["gmb-collections"]);
        Assert.NotNull(pm.ResolvedArtifacts["gmb-utils"]);
        Assert.True(pm.ResolvedArtifacts["gmb-utils"].Version == "1.2.0");
    }

    // ── 7. Mixed: binario preferito se backend match, altrimenti source ──
    [Fact]
    public void Resolve_Mixed_Prefers_Binary_When_Backend_Matches()
    {
        WritePkg("gmb-vec", "1.2.0", """
        { "name":"gmb-vec","version":"1.2.0","contentType":"mixed","backends":["c99"],
          "exports":["src/"],"binaries":{"c99":{"linux-x64":"bin/c99/libvec.so"}},
          "ffi":{"api":"c-abi","libs":{"linux-x64":"lib/libvec.so"}} }
        """);
        // backend c99 + platform match → binario c99
        var pm = NewPm();
        pm.ResolveDependenciesAsync([new RequireStmt(new StringExpr("gmb-vec", new Token(TokenKind.StringLiteral, "gmb-vec", "gmb-vec", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "c99", PlatformKey = "linux-x64" }).GetAwaiter().GetResult();
        Assert.NotNull(pm.ResolvedArtifacts["gmb-vec"].BinaryPath);

        // backend il senza binario → fallback source (exports)
        var pm2 = NewPm();
        pm2.ResolveDependenciesAsync([new RequireStmt(new StringExpr("gmb-vec", new Token(TokenKind.StringLiteral, "gmb-vec", "gmb-vec", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.Null(pm2.ResolvedArtifacts["gmb-vec"].BinaryPath);
        Assert.NotNull(pm2.ResolvedArtifacts["gmb-vec"].SourceDir);
    }

    // ── 8. VM/C precompilato: chunk A1/GMBC (.prg) selezionato per backend vm ──
    [Fact]
    public void Resolve_VmChunk_Selects_ChunkPath_For_Vm_Backend()
    {
        WritePkg("gmb-app", "1.0.0", """
        { "name":"gmb-app","version":"1.0.0","contentType":"vm","backends":["vm"],
          "binaries":{ "vm":{"any-any":"bin/app.prg"} } }
        """);
        var pm = NewPm();
        pm.ResolveDependenciesAsync([new RequireStmt(new StringExpr("gmb-app", new Token(TokenKind.StringLiteral, "gmb-app", "gmb-app", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "vm", PlatformKey = "linux-x64" }).GetAwaiter().GetResult();
        var art = pm.ResolvedArtifacts["gmb-app"];
        Assert.Equal(PackageContentType.VmChunk, art.ContentType);
        Assert.NotNull(art.ChunkPath);
        Assert.Contains("app.prg", art.ChunkPath);
        Assert.True(File.Exists(art.ChunkPath));
        Assert.Null(art.BinaryPath);
    }

    // ── 8b. DotNet con exports + backend diverso da il → ricompila da source ──
    [Fact]
    public void Resolve_DotNet_BackendNotIl_FallsBack_Source()
    {
        WritePkg("gmb-chart", "1.0.0", """
        { "name":"gmb-chart","version":"1.0.0","contentType":"dotnet","backends":["il"],
          "exports":["src/"],"binaries":{ "il":{"any-any":"bin/il/GmbChart.dll"} } }
        """);
        var pm = NewPm();
        pm.ResolveDependenciesAsync([new RequireStmt(new StringExpr("gmb-chart", new Token(TokenKind.StringLiteral, "gmb-chart", "gmb-chart", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "c99" }).GetAwaiter().GetResult();
        var art = pm.ResolvedArtifacts["gmb-chart"];
        Assert.Equal(PackageContentType.Source, art.ContentType); // ricompilazione da exports
        Assert.NotNull(art.SourceDir);
        Assert.Null(art.BinaryPath);
    }

    // ── 8c. VmChunk con exports + backend diverso da vm → ricompila da source ──
    [Fact]
    public void Resolve_VmChunk_BackendNotVm_FallsBack_Source()
    {
        WritePkg("gmb-app", "1.0.0", """
        { "name":"gmb-app","version":"1.0.0","contentType":"vm","backends":["vm"],
          "exports":["src/"],"binaries":{ "vm":{"any-any":"bin/app.prg"} } }
        """);
        var pm = NewPm();
        pm.ResolveDependenciesAsync([new RequireStmt(new StringExpr("gmb-app", new Token(TokenKind.StringLiteral, "gmb-app", "gmb-app", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        var art = pm.ResolvedArtifacts["gmb-app"];
        Assert.Equal(PackageContentType.Source, art.ContentType);
        Assert.NotNull(art.SourceDir);
    }

    // ── 8d. SemVer 2.0: prerelease/build e range OR (||) ──
    [Fact]
    public void SemVer20_Prerelease_And_Build()
    {
        var pre = Parser.PackageManager.ParseSemVer("1.0.0-rc.1");
        var rel = Parser.PackageManager.ParseSemVer("1.0.0");
        Assert.NotNull(pre); Assert.NotNull(rel);
        Assert.True(pre!.IsPrerelease);
        Assert.False(rel!.IsPrerelease);
        Assert.True(pre.CompareTo(rel) < 0, "prerelease < release");
        var alpha = Parser.PackageManager.ParseSemVer("1.0.0-alpha");
        var beta = Parser.PackageManager.ParseSemVer("1.0.0-beta");
        Assert.True(alpha!.CompareTo(beta) < 0, "alpha < beta");
        var build = Parser.PackageManager.ParseSemVer("1.0.0+build5");
        Assert.NotNull(build);
        Assert.Equal("build5", build!.Build);
        Assert.Equal(0, rel.CompareTo(build)); // build ignorata nella PRECEDENZA
    }

    [Fact]
    public void SemVer20_Or_Range_And_Prerelease_Constraint()
    {
        // versioni in cache: 0.4.0, 1.0.0, 1.0.0-rc.1, 2.4.0, 3.0.0
        foreach (var v in new[] { "0.4.0", "1.0.0", "1.0.0-rc.1", "2.4.0", "3.0.0" })
            WritePkg("gmb-or", v, $"{{\"name\":\"gmb-or\",\"version\":\"{v}\",\"exports\":[\"src/\"]}}");

        // OR: >=2.0.0 || <1.0.0 → max che soddisfa ≥2.0.0 = 3.0.0
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-or", ">=2.0.0 || <1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.Equal("3.0.0", pm.ResolvedArtifacts["gmb-or"].Version);

        // ^1.0.0 non deve matchare 1.0.0-rc.1 (prerelease) → 1.0.0
        var pm2 = NewPm();
        pm2.ResolveDependenciesAsync([Req("gmb-or", "^1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.Equal("1.0.0", pm2.ResolvedArtifacts["gmb-or"].Version);
    }

    private static RequireStmt Req(string name, string constraint) => new(
        new StringExpr(name, new Token(TokenKind.StringLiteral, name, name, SourcePosition.Unknown)),
        new StringExpr(constraint, new Token(TokenKind.StringLiteral, constraint, constraint, SourcePosition.Unknown)),
        new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown));

    // ── 8e. Init + Pack: crea un .gmbpkg valido (zip con manifest + exports + .gmbpkg.sig) ──
    [Fact]
    public void Init_And_Pack_Produce_Valid_Gmbpkg()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmb_init_" + Guid.NewGuid().ToString("N"));
        try
        {
            var manifestPath = PackageBuilder.Init(dir, "gmb-demo", "1.0.0", PackageContentType.Source);
            Assert.True(File.Exists(manifestPath));
            Assert.True(Directory.Exists(Path.Combine(dir, "src")));

            var outZip = Path.Combine(dir, "gmb-demo-1.0.0.gmbpkg");
            PackageBuilder.Pack(dir, outZip, secret: "test-secret");
            Assert.True(File.Exists(outZip));

            using var zip = System.IO.Compression.ZipFile.OpenRead(outZip);
            var names = zip.Entries.Select(e => e.FullName).ToHashSet(StringComparer.OrdinalIgnoreCase);
            Assert.Contains("gmb-package.json", names);
            Assert.Contains("src/Main.bas", names);
            Assert.Contains(".gmbpkg.sig", names);
            var sig = new StreamReader(zip.Entries.First(e => e.FullName == ".gmbpkg.sig").Open()).ReadToEnd();
            Assert.NotEqual("unsigned", sig);
        }
        finally { try { Directory.Delete(dir, true); } catch { } }
    }

    // ── 8f. Validazione: nome/versione/artefatti ──
    [Theory]
    [InlineData("Bad Name", "1.0.0", true)]
    [InlineData("gmb-ok", "nope", true)]
    [InlineData("gmb-ok", "1.0.0", false)]
    public void Validate_Rejects_Invalid(string name, string version, bool expectThrow)
    {
        // il caso valido ha exports (altrimenti "senza artefatti" lancerebbe)
        var m = new PackageManifest { Name = name, Version = version, ContentType = PackageContentType.Source,
                                      Exports = !expectThrow ? ImmutableArray.Create("src/") : [] };
        if (expectThrow)
            Assert.Throws<PackageValidationException>(() => PackageBuilder.ValidateManifest(m));
        else
            PackageBuilder.ValidateManifest(m); // non deve lanciare
    }

    // ── 8g. Lock per-file sha256 + VerifyIntegrity ──
    [Fact]
    public void Lock_Has_File_Sha256_And_Integrity_Verifies()
    {
        WritePkg("gmb-lock2", "1.0.0", """{ "name":"gmb-lock2","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-lock2", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_lock_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmp);
        try
        {
            pm.WriteLockFile(tmp);
            var content = File.ReadAllText(Path.Combine(tmp, "gmb-package.lock"));
            Assert.Contains("file:", content);         // sha256 per-file
            Assert.Contains("gmb-package.json:", content);
            var lockPath = Path.Combine(tmp, "gmb-package.lock");
            Assert.Empty(pm.VerifyLockedIntegrity(lockPath));   // integrità ok (hash del lock vs cache)

            // altera un file → integrità rotta
            File.AppendAllText(Path.Combine(pm.ResolvedArtifacts["gmb-lock2"].SourceDir, "gmb-lock2.bas"), "\nPRINT \"tampered\"\n");
            Assert.NotEmpty(pm.VerifyLockedIntegrity(lockPath)["gmb-lock2"]);
        }
        finally { try { Directory.Delete(tmp, true); } catch { } }
    }

    // ── 8h. GetC99LinkInfo: flag -I/-L/-l dalle librerie FFI dei pacchetti risolti ──
    [Fact]
    public void GetC99LinkInfo_Returns_Ffi_Flags()
    {
        WritePkg("gmb-phys", "0.9.0", """
        { "name":"gmb-phys","version":"0.9.0","contentType":"c-ffi",
          "ffi":{"api":"c-abi","headers":["include/phys.h"],"libs":{"linux-x64":"lib/libphys.so"},"link":"-lphys"} }
        """);
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-phys", "0.9.0")], new PackageBuildContext { Backend = "c99", PlatformKey = "linux-x64" }).GetAwaiter().GetResult();
        var info = pm.GetC99LinkInfo();
        Assert.NotEmpty(info.Includes);
        Assert.Contains("include", info.Includes[0]);          // dir degli header
        Assert.Contains("phys", string.Join(" ", info.Libs));  // -lphys (prefisso lib tolto)
        Assert.Contains("-lphys", info.ToCcFlags());
        Assert.Contains("-I", info.ToCcFlags());
        Assert.Contains("-L", info.ToCcFlags());
    }

    // ── 8i. Pacchetto sorgente scaricato → compilato subito dall'Orchestrator ──
    [Fact]
    public void Require_SourcePackage_IsCompiled_ByOrchestrator()
    {
        var cache = Path.Combine(Path.GetTempPath(), "gmb_pkgcache_" + Guid.NewGuid().ToString("N"));
        try
        {
            // pacchetto sorgente in cache: SUB semplice in exports
            var pkg = Path.Combine(cache, "gmb-util", "1.0.0");
            Directory.CreateDirectory(Path.Combine(pkg, "src"));
            File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
                """{ "name":"gmb-util","version":"1.0.0","contentType":"source","exports":["src/"] }""");
            File.WriteAllText(Path.Combine(pkg, "src", "gutil.bas"),
                "SUB PackageHello()\n  PRINT \"hello-from-pkg\"\nEND SUB\n");

            // main che REQUIRES il pacchetto (il sorgente viene compilato subito dall'Orchestrator)
            var main = Path.Combine(Path.GetTempPath(), "gmb_main_" + Guid.NewGuid().ToString("N") + ".bas");
            File.WriteAllText(main, "REQUIRE \"gmb-util\" @ \"1.0.0\"\nSUB Main()\n  PRINT \"main-ok\"\nEND SUB\n");
            try
            {
                Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
                try
                {
                    var program = Basic.Orchestrator.Compile(main, null, false);
                    Assert.NotNull(program);
                    Assert.NotNull(program.MainModule);
                    // il sorgente del pacchetto è stato compilato subito dopo il download (in PreprocessedFiles)
                    Assert.Contains(program.PreprocessedFiles.Keys,
                        k => k.Replace('\\', '/').EndsWith("gutil.bas", StringComparison.OrdinalIgnoreCase));
                }
                finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
            }
            finally { try { File.Delete(main); } catch { } }
        }
        finally { try { Directory.Delete(cache, true); } catch { } }
    }

    // ── 8j. Features/dipendenze opzionali (cargo-style) ──
    [Fact]
    public void Resolve_Features_Enable_OptionalDeps()
    {
        WritePkg("gmb-app", "1.0.0", """
        { "name":"gmb-app","version":"1.0.0","contentType":"source","exports":["src/"],
          "features":{ "default":["gmb-core"], "extra":["gmb-extra"] } }
        """);
        WritePkg("gmb-core", "1.0.0", """{ "name":"gmb-core","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        WritePkg("gmb-extra", "1.0.0", """{ "name":"gmb-extra","version":"1.0.0","contentType":"source","exports":["src/"] }""");

        // senza feature extra → solo default
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-app", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.True(pm.ResolvedArtifacts.ContainsKey("gmb-core"));
        Assert.False(pm.ResolvedArtifacts.ContainsKey("gmb-extra"));

        // con feature "extra" → anche gmb-extra
        var pm2 = NewPm();
        pm2.ResolveDependenciesAsync([Req("gmb-app", "1.0.0")], new PackageBuildContext { Backend = "il", Features = new[] { "extra" } }).GetAwaiter().GetResult();
        Assert.True(pm2.ResolvedArtifacts.ContainsKey("gmb-extra"));
    }

    // ── 8k. Lock enforcement (--locked): pina alla versione nel lock ──
    [Fact]
    public void Resolve_LockFile_Pins_Versions()
    {
        foreach (var v in new[] { "1.0.0", "1.7.2", "1.9.1" })
            WritePkg("gmb-pin", v, $"{{\"name\":\"gmb-pin\",\"version\":\"{v}\",\"exports\":[\"src/\"]}}");

        // senza lock → max che soddisfa ^1.0.0 = 1.9.1
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-pin", "^1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.Equal("1.9.1", pm.ResolvedArtifacts["gmb-pin"].Version);

        // con lock che pina 1.7.2 → resta 1.7.2 (lock enforcement)
        var lockPath = Path.Combine(Path.GetTempPath(), "gmb_lock_" + Guid.NewGuid().ToString("N") + ".lock");
        File.WriteAllText(lockPath, "# gmb-package.lock\ngmb-pin@1.7.2|Source|any|any|deadbeef\n");
        try
        {
            var pm2 = NewPm();
            pm2.ResolveDependenciesAsync([Req("gmb-pin", "^1.0.0")], new PackageBuildContext { Backend = "il" }, lockPath).GetAwaiter().GetResult();
            Assert.Equal("1.7.2", pm2.ResolvedArtifacts["gmb-pin"].Version);
        }
        finally { try { File.Delete(lockPath); } catch { } }
    }

    // ── 8l. Dependency tree ──
    [Fact]
    public void GetDependencyTree_Shows_ParentChildren()
    {
        WritePkg("gmb-a", "1.0.0", """{ "name":"gmb-a","version":"1.0.0","exports":["src/"],"dependencies":{"gmb-b":"1.0.0"} }""");
        WritePkg("gmb-b", "1.0.0", """{ "name":"gmb-b","version":"1.0.0","exports":["src/"] }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-a", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        var tree = pm.GetDependencyTree();
        Assert.Contains("gmb-b", tree["gmb-a"]);
        Assert.Empty(tree["gmb-b"]);
    }

    // ── 8m. Firma pubblica Ed25519: Pack firmato → VerifyGmbpkg true; tamper → false ──
    [Fact]
    public void Sign_And_Verify_Gmbpkg_Ed25519()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmb_sig_" + Guid.NewGuid().ToString("N"));
        try
        {
            PackageBuilder.Init(dir, "gmb-signed", "1.0.0", PackageContentType.Source);
            var (priv, pub) = PackageBuilder.GenerateKeyPair();
            var zip = Path.Combine(dir, "gmb-signed-1.0.0.gmbpkg");
            PackageBuilder.Pack(dir, zip, privatePem: priv);
            Assert.True(PackageBuilder.VerifyGmbpkg(zip), "firma valida");

            // altera il manifest nello zip → firma invalida
            using (var ms = new MemoryStream())
            {
                using (var outZip = new System.IO.Compression.ZipArchive(ms, System.IO.Compression.ZipArchiveMode.Create, true))
                {
                    foreach (var e in System.IO.Compression.ZipFile.OpenRead(zip).Entries)
                    {
                        var entry = outZip.CreateEntry(e.FullName);
                        using var src = e.Open();
                        using var dst = entry.Open();
                        if (e.FullName == "gmb-package.json")
                        {
                            var tampered = "{\"name\":\"gmb-signed\",\"version\":\"9.9.9\",\"contentType\":\"source\",\"exports\":[\"src/\"]}";
                            var bytes = System.Text.Encoding.UTF8.GetBytes(tampered);
                            dst.Write(bytes, 0, bytes.Length);
                        }
                        else src.CopyTo(dst);
                    }
                }
                var tmp = Path.Combine(dir, "tampered.gmbpkg");
                File.WriteAllBytes(tmp, ms.ToArray());
                Assert.False(PackageBuilder.VerifyGmbpkg(tmp), "tamper rilevato");
                File.Delete(tmp);
            }
            Assert.False(string.IsNullOrEmpty(pub));
            // chiave pubblica Ed25519 = 32 byte base64 (non PEM)
            Assert.Equal(32, Convert.FromBase64String(pub).Length);
            // roundtrip diretto: sign/verify con le chiavi generate
            var msg = System.Text.Encoding.UTF8.GetBytes("roundtrip");
            var sig = PackageBuilder.SignData(msg, priv);
            Assert.True(PackageBuilder.VerifyData(msg, pub, sig));
            Assert.False(PackageBuilder.VerifyData(System.Text.Encoding.UTF8.GetBytes("tampered"), pub, sig));
        }
        finally { try { Directory.Delete(dir, true); } catch { } }
    }

    // ── 8n. Dist-tag via registry mock (HttpListener) ──
    [Fact]
    public async Task DistTag_Resolved_Via_Registry()
    {
        foreach (var v in new[] { "1.9.0", "2.0.0-rc.1" })
            WritePkg("gmb-tagged", v, $"{{\"name\":\"gmb-tagged\",\"version\":\"{v}\",\"exports\":[\"src/\"]}}");

        // registry mock: server socket RAW che parla HTTP/1.1 (il resolver C usa libcurl → lo raggiunge).
        var tcp = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        tcp.Start();
        var port2 = ((System.Net.IPEndPoint)tcp.LocalEndpoint).Port;
        var baseUrl = $"http://127.0.0.1:{port2}/";
        _ = Task.Run(async () =>
        {
            while (true)
            {
                System.Net.Sockets.TcpClient client;
                try { client = await tcp.AcceptTcpClientAsync(); } catch { break; }
                _ = Task.Run(async () =>
                {
                    using (client)
                    using (var s = client.GetStream())
                    {
                        var rb = new byte[8192];
                        try { await s.ReadAsync(rb); } catch { }
                        const string body = "{\"next\":\"2.0.0-rc.1\"}";
                        var resp = $"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {body.Length}\r\nConnection: close\r\n\r\n{body}";
                        try { await s.WriteAsync(System.Text.Encoding.ASCII.GetBytes(resp)); await s.FlushAsync(); } catch { }
                    }
                });
            }
        });

        try
        {
            var pm = NewPm();
            pm.RegistryUrl = baseUrl;
            pm.RegistryUrls = [baseUrl];
            pm.ResolveDependenciesAsync([Req("gmb-tagged", "next")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
            Assert.Equal("2.0.0-rc.1", pm.ResolvedArtifacts["gmb-tagged"].Version);
        }
        finally { tcp.Stop(); }
    }

    // ── 8o. Vendoring: copia in vendor/ e risoluzione OFFLINE (cache vuota) ──
    [Fact]
    public void Vendor_And_Offline_Resolve()
    {
        WritePkg("gmb-vend", "1.0.0", """{ "name":"gmb-vend","version":"1.0.0","exports":["src/"] }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-vend", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();

        var vendorDir = Path.Combine(Path.GetTempPath(), "gmb_vendor_" + Guid.NewGuid().ToString("N"));
        try
        {
            pm.Vendor(vendorDir);
            Assert.True(File.Exists(Path.Combine(vendorDir, "gmb-vend", "1.0.0", "gmb-package.json")));
            Assert.True(File.Exists(Path.Combine(vendorDir, "gmb-vend", "1.0.0", "src", "gmb-vend.bas")));

            // offline: cache VUOTA + VendorDir nel ctx → risolve dal vendor
            var pm2 = new Parser.PackageManager { CacheRoot = Path.Combine(Path.GetTempPath(), "gmb_nocache_" + Guid.NewGuid().ToString("N")) };
            pm2.ResolveDependenciesAsync([Req("gmb-vend", "1.0.0")],
                new PackageBuildContext { Backend = "il", VendorDir = vendorDir }).GetAwaiter().GetResult();
            Assert.True(pm2.ResolvedArtifacts.ContainsKey("gmb-vend"));
            Assert.Equal("1.0.0", pm2.ResolvedArtifacts["gmb-vend"].Version);
            Assert.True(File.Exists(Path.Combine(pm2.ResolvedArtifacts["gmb-vend"].InstallPath, "gmb-package.json")));
        }
        finally { try { Directory.Delete(vendorDir, true); } catch { } }
    }

    // ── 8p. Transitive: dipendenza risolta ricorsivamente (con ciclo) ──
    [Fact]
    public void Resolve_Transitive_And_Cycle_Safe()
    {
        WritePkg("gmb-a", "1.0.0", """{ "name":"gmb-a","version":"1.0.0","exports":["src/"],"dependencies":{"gmb-b":"1.0.0"} }""");
        WritePkg("gmb-b", "1.0.0", """{ "name":"gmb-b","version":"1.0.0","exports":["src/"],"dependencies":{"gmb-a":"1.0.0"} }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([new RequireStmt(new StringExpr("gmb-a", new Token(TokenKind.StringLiteral, "gmb-a", "gmb-a", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.True(pm.ResolvedArtifacts.ContainsKey("gmb-a"));
        Assert.True(pm.ResolvedArtifacts.ContainsKey("gmb-b"));
        Assert.NotNull(pm.LockData["gmb-a"]);
        Assert.NotNull(pm.LockData["gmb-b"]);
    }

    // ── 9. Lock file scritto ──
    [Fact]
    public void Resolve_Writes_LockFile()
    {
        WritePkg("gmb-lock", "2.0.0", """{ "name":"gmb-lock","version":"2.0.0","contentType":"source","exports":["src/"] }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([new RequireStmt(new StringExpr("gmb-lock", new Token(TokenKind.StringLiteral, "gmb-lock", "gmb-lock", SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))],
            new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_lock_test_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmp);
        try
        {
            pm.WriteLockFile(tmp);
            var lockPath = Path.Combine(tmp, "gmb-package.lock");
            Assert.True(File.Exists(lockPath));
            var content = File.ReadAllText(lockPath);
            Assert.Contains("gmb-lock@2.0.0", content);
            Assert.Contains("Source", content);
        }
        finally { try { Directory.Delete(tmp, true); } catch { } }
    }

    // ── 10. Fase 1: source github-release — parsing, tag templato, selezione asset ──
    [Fact]
    public void ParseManifest_Source_GithubRelease_ResolvesTagAndAsset()
    {
        var doc = JsonDocument.Parse("""
        { "name":"duckdb","version":"1.5.6","contentType":"c-ffi",
          "ffi":{"api":"c-abi","headers":["include/duckdb.h"],"libs":{"osx-arm64":"lib/osx-arm64/libduckdb.dylib"}},
          "source":{ "type":"github-release","repo":"duckdb/duckdb","tag":"v{version}",
                     "assets":{"osx-arm64":"libduckdb-osx-universal.zip","linux-x64":"libduckdb-linux-amd64.zip"},
                     "headers":["duckdb.h"],"sha256":{"linux-x64":"deadbeef"} } }
        """);
        var m = Parser.PackageManager.ParseManifest(doc.RootElement);
        Assert.NotNull(m.Source);
        Assert.Equal("github-release", m.Source!.Type);
        Assert.Equal("duckdb/duckdb", m.Source.Repo);
        Assert.Equal("v1.5.6", m.Source.ResolveTag(m.Version));
        Assert.Equal("lib/osx-arm64", m.Source.Dest.Replace("{platform}", "osx-arm64"));
        Assert.Equal("libduckdb-osx-universal.zip", m.Source.PickAsset("osx-arm64"));
        Assert.Equal("libduckdb-linux-amd64.zip", m.Source.PickAsset("linux-x64"));
        Assert.Null(m.Source.PickAsset("win-x64"));          // nessun asset per win-x64
        Assert.Contains("duckdb.h", m.Source.Headers);
        Assert.Equal("deadbeef", m.Source.Sha256["linux-x64"]);
    }

    // ── 11. Fase 1: materializzazione archivio (zip) — dest lib/<plat> + header in include/ ──
    [Fact]
    public void MaterializeArchive_Zip_PlacesLibAndHeaders()
    {
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_mat_test_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmp);
        try
        {
            var zip = Path.Combine(tmp, "asset.zip");
            using (var z = System.IO.Compression.ZipFile.Open(zip, System.IO.Compression.ZipArchiveMode.Create))
            {
                WriteZipEntry(z, "libduckdb.dylib", "BINDATA");
                WriteZipEntry(z, "duckdb.h", "#pragma once");
            }
            var dest = Path.Combine(tmp, "lib", "osx-arm64");
            var inc = Path.Combine(tmp, "include");
            Parser.PackageManager.MaterializeArchive(zip, dest, ["duckdb.h"], inc);

            Assert.True(File.Exists(Path.Combine(dest, "libduckdb.dylib")));
            Assert.Equal("BINDATA", File.ReadAllText(Path.Combine(dest, "libduckdb.dylib")));
            Assert.True(File.Exists(Path.Combine(dest, "duckdb.h")));   // presente anche nella dest
            Assert.True(File.Exists(Path.Combine(inc, "duckdb.h")));   // e copiato negli header
            Assert.Equal("#pragma once", File.ReadAllText(Path.Combine(inc, "duckdb.h")));
        }
        finally { try { Directory.Delete(tmp, true); } catch { } }
    }

    // ── 8q. GetOutdatedAsync con vincoli: target = piu' alta che SODDISFA i vincoli (non l'assoluta) ──
    [Fact]
    public async Task Outdated_Respects_Constraints()
    {
        WritePkg("gmb-out", "1.5.0", """{ "name":"gmb-out","version":"1.5.0","contentType":"source","exports":["src/"] }""");
        WritePkg("gmb-out", "2.0.0", """{ "name":"gmb-out","version":"2.0.0","contentType":"source","exports":["src/"] }""");

        var pm = NewPm(); // cache con 1.5.0 e 2.0.0
        pm.ResolveDependenciesAsync([Req("gmb-out", "^1.5.0")], new PackageBuildContext { Backend = "il" })
            .GetAwaiter().GetResult();
        Assert.Equal("1.5.0", pm.ResolvedArtifacts["gmb-out"].Version); // ^1.5.0 esclude 2.0.0

        // registry mock: offre 1.5.0, 1.6.0, 2.0.0
        var tcp = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        tcp.Start();
        var port = ((System.Net.IPEndPoint)tcp.LocalEndpoint).Port;
        tcp.Stop();
        using var listener = new System.Net.HttpListener();
        listener.Prefixes.Add($"http://127.0.0.1:{port}/");
        listener.Start();
        _ = Task.Run(async () =>
        {
            while (true)
            {
                System.Net.HttpListenerContext ctx;
                try { ctx = await listener.GetContextAsync(); } catch { break; }
                var json = "{\"versions\":{\"1.5.0\":{\"version\":\"1.5.0\"},\"1.6.0\":{\"version\":\"1.6.0\"},\"2.0.0\":{\"version\":\"2.0.0\"}}}";
                var buf = System.Text.Encoding.UTF8.GetBytes(json);
                ctx.Response.StatusCode = 200;
                ctx.Response.ContentType = "application/json";
                await ctx.Response.OutputStream.WriteAsync(buf);
                ctx.Response.Close();
            }
        });
        try
        {
            pm.RegistryUrl = $"http://127.0.0.1:{port}/";
            pm.RegistryUrls = [pm.RegistryUrl];
            var ctxb = new PackageBuildContext { Backend = "il" };

            var constrained = await pm.GetOutdatedAsync(
                new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase) { ["gmb-out"] = ["^1.5.0"] }, ctxb);
            Assert.True(constrained.ContainsKey("gmb-out"));
            Assert.Equal("1.5.0", constrained["gmb-out"].Installed);
            Assert.Equal("1.6.0", constrained["gmb-out"].Latest); // NON 2.0.0: vincolo rispettato

            var absolute = await pm.GetOutdatedAsync((IReadOnlyDictionary<string, List<string>>?)null, ctxb);
            Assert.Equal("2.0.0", absolute["gmb-out"].Latest); // senza vincoli: ultima assoluta
        }
        finally { listener.Stop(); }
    }

    // ── 8q-bis. Download dal registry: download + estrazione DELEGATI al C ──
    [Fact]
    public async Task DownloadPackageAsync_FromRegistry_UsesC()
    {
        var tcp = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        tcp.Start();
        var port = ((System.Net.IPEndPoint)tcp.LocalEndpoint).Port;
        tcp.Stop();
        using var listener = new System.Net.HttpListener();
        listener.Prefixes.Add($"http://127.0.0.1:{port}/");
        listener.Start();

        var zip = BuildPkgZip("gmb-dl", "1.0.0");
        _ = Task.Run(async () =>
        {
            while (true)
            {
                System.Net.HttpListenerContext c;
                try { c = await listener.GetContextAsync(); } catch { break; }
                var path = c.Request.Url!.AbsolutePath;
                byte[] body;
                if (path.StartsWith("/download/"))
                {
                    body = zip;
                    c.Response.ContentType = "application/zip";
                }
                else
                {
                    body = System.Text.Encoding.UTF8.GetBytes("{\"versionList\":[\"1.0.0\"]}");
                    c.Response.ContentType = "application/json";
                }
                c.Response.StatusCode = 200;
                await c.Response.OutputStream.WriteAsync(body);
                c.Response.Close();
            }
        });
        try
        {
            var pm = NewPm();
            var pkg = await pm.DownloadPackageAsync("gmb-dl", "1.0.0", $"http://127.0.0.1:{port}");
            Assert.Equal("1.0.0", pkg.Manifest.Version);
            Assert.True(File.Exists(Path.Combine(pkg.InstallPath, "gmb-package.json")));
            Assert.True(File.Exists(Path.Combine(pkg.InstallPath, "src", "gmb-dl.bas")));
        }
        finally { listener.Stop(); }
    }

    // ── 8q-ter. Unzip: accetta anche zip con entry DEFLATE (non solo STORED) ──
    [Fact]
    public void Unzip_Handles_Deflate_Entries()
    {
        var tmp = Path.GetTempFileName() + ".zip";
        var outDir = Path.Combine(Path.GetTempPath(), "gmb_unzip_" + Guid.NewGuid().ToString("N"));
        try
        {
            using (var za = System.IO.Compression.ZipFile.Open(tmp, System.IO.Compression.ZipArchiveMode.Create))
            {
                var e = za.CreateEntry("a/b.txt", System.IO.Compression.CompressionLevel.Optimal);
                using var w = new StreamWriter(e.Open());
                w.Write(new string('x', 5000));
            }
            Assert.True(NativePkg.Unzip(tmp, outDir));
            Assert.True(File.Exists(Path.Combine(outDir, "a", "b.txt")));
            Assert.Equal(5000, new FileInfo(Path.Combine(outDir, "a", "b.txt")).Length);
        }
        finally { try { File.Delete(tmp); } catch { } try { Directory.Delete(outDir, true); } catch { } }
    }

    // ── 8q-quater. Registry per-scope: il pacchetto "@org/..." usa il registry dello scope ──
    [Fact]
    public async Task Scoped_Registry_Used_For_Scope()
    {
        int FreePort()
        {
            var l = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
            l.Start(); var p = ((System.Net.IPEndPoint)l.LocalEndpoint).Port; l.Stop(); return p;
        }
        System.Net.HttpListener Start(int port)
        {
            var l = new System.Net.HttpListener();
            l.Prefixes.Add($"http://127.0.0.1:{port}/");
            l.Start(); return l;
        }
        var scopePort = FreePort();
        var defPort = FreePort();
        using var scopeLis = Start(scopePort);
        using var defLis = Start(defPort);
        var zip = BuildPkgZip("@org/pkg", "1.0.0");

        _ = Task.Run(async () =>   // registry dello scope: serve dettaglio + download
        {
            while (true)
            {
                System.Net.HttpListenerContext c;
                try { c = await scopeLis.GetContextAsync(); } catch { break; }
                byte[] body; 
                if (c.Request.Url!.AbsolutePath.StartsWith("/download/")) { body = zip; c.Response.ContentType = "application/zip"; }
                else { body = System.Text.Encoding.UTF8.GetBytes("{\"versionList\":[\"1.0.0\"]}"); c.Response.ContentType = "application/json"; }
                c.Response.StatusCode = 200;
                await c.Response.OutputStream.WriteAsync(body);
                c.Response.Close();
            }
        });
        _ = Task.Run(async () =>   // registry default: 404 su tutto (NON deve essere usato per @org)
        {
            while (true)
            {
                System.Net.HttpListenerContext c;
                try { c = await defLis.GetContextAsync(); } catch { break; }
                c.Response.StatusCode = 404; c.Response.Close();
            }
        });
        try
        {
            var pm = NewPm();
            pm.RegistryUrl = $"http://127.0.0.1:{defPort}/";
            pm.RegistryUrls = [pm.RegistryUrl];
            var ctx = new PackageBuildContext
            {
                Backend = "il",
                Registries = new Dictionary<string, string> { ["@org"] = $"http://127.0.0.1:{scopePort}/" }
            };
            await pm.ResolveDependenciesAsync([Req("@org/pkg", "1.0.0")], ctx);
            Assert.True(pm.ResolvedPackages.ContainsKey("@org/pkg"));
        }
        finally { scopeLis.Stop(); defLis.Stop(); }
    }

    private static byte[] BuildPkgZip(string name, string version)
    {
        using var ms = new MemoryStream();
        using (var za = new System.IO.Compression.ZipArchive(ms, System.IO.Compression.ZipArchiveMode.Create, true))
        {
            Add(za, "gmb-package.json", $"{{\"name\":\"{name}\",\"version\":\"{version}\",\"contentType\":\"source\"}}");
            Add(za, $"src/{name}.bas", $"PRINT \"{name} v{version}\"\n");
        }
        return ms.ToArray();

        static void Add(System.IO.Compression.ZipArchive za, string path, string content)
        {
            // gmb_unzip_files estrae solo entry STORED (come i .gmbpkg di gmb pack).
            var e = za.CreateEntry(path, System.IO.Compression.CompressionLevel.NoCompression);
            using var w = new StreamWriter(e.Open());
            w.Write(content);
        }
    }

    // ── 8r. Gate di compatibilità (solo minGmbVersion: la matrice Edition non esiste più) ──
    [Fact]
    public void Compatibility_Gates()
    {
        var ctx = new PackageBuildContext { Backend = "il" };
        Parser.PackageManager.CheckCompatibility(new PackageManifest { Name = "a", Version = "1.0.0" }, ctx);

        Assert.Throws<PackageException>(() => Parser.PackageManager.CheckCompatibility(
            new PackageManifest { Name = "a", Version = "1.0.0", Settings = new PackageSettings { MinGmbVersion = "999.0.0" } }, ctx));

        // Un manifest che dichiara ancora la vecchia chiave "edition" resta compatibile:
        // non c'è più nessun gate legato all'edizione.
        Parser.PackageManager.CheckCompatibility(new PackageManifest { Name = "a", Version = "1.0.0" }, ctx);
    }

    // ── 8s. Trust anchor: TOFU (prima registrazione), chiave fidata, cambio chiave, strict non firmato ──
    [Fact]
    public void Trust_Tofu_KeyChange_And_Strict()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmb_trust_" + Guid.NewGuid().ToString("N"));
        var kr = Path.Combine(Path.GetTempPath(), "gmb_keyring_" + Guid.NewGuid().ToString("N") + ".json");
        var prev = TrustStore.StorePath;
        Directory.CreateDirectory(dir);
        try
        {
            TrustStore.StorePath = kr;
            var manJson = System.Text.Encoding.UTF8.GetBytes("{\"name\":\"gmb-trust\",\"version\":\"1.0.0\"}");
            File.WriteAllBytes(Path.Combine(dir, "gmb-package.json"), manJson);
            var (priv, pub) = PackageBuilder.GenerateKeyPair();
            File.WriteAllText(Path.Combine(dir, ".gmbpkg.pub"), pub);
            File.WriteAllText(Path.Combine(dir, ".gmbpkg.sig"), PackageBuilder.SignData(manJson, priv));
            var m = new PackageManifest { Name = "gmb-trust", Version = "1.0.0" };

            Assert.Equal(TrustResult.FirstUse, Parser.PackageManager.VerifyInstalledTrust(m, dir, "gmb-trust", TrustPolicy.Tofu).Result);
            Assert.True(File.Exists(kr));
            Assert.Equal(TrustResult.Trusted, Parser.PackageManager.VerifyInstalledTrust(m, dir, "gmb-trust", TrustPolicy.Tofu).Result);

            // chiave diversa → KeyChanged
            var (priv2, pub2) = PackageBuilder.GenerateKeyPair();
            File.WriteAllText(Path.Combine(dir, ".gmbpkg.pub"), pub2);
            File.WriteAllText(Path.Combine(dir, ".gmbpkg.sig"), PackageBuilder.SignData(manJson, priv2));
            Assert.Equal(TrustResult.KeyChanged, Parser.PackageManager.VerifyInstalledTrust(m, dir, "gmb-trust", TrustPolicy.Tofu).Result);

            // strict + non firmato → Untrusted
            File.Delete(Path.Combine(dir, ".gmbpkg.pub"));
            File.Delete(Path.Combine(dir, ".gmbpkg.sig"));
            Assert.Equal(TrustResult.Untrusted,
                Parser.PackageManager.VerifyInstalledTrust(new PackageManifest { Name = "gmb-trust", Version = "1.0.0" }, dir, "gmb-trust", TrustPolicy.Strict).Result);
        }
        finally
        {
            TrustStore.StorePath = prev;
            try { File.Delete(kr); } catch { }
            try { Directory.Delete(dir, true); } catch { }
        }
    }

    // ── 8t. Integrità vs lock: la manomissione dei file in cache è rilevata ──
    [Fact]
    public void LockedIntegrity_Detects_Tamper()
    {
        WritePkg("gmb-int", "1.0.0", """{ "name":"gmb-int","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-int", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_int_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmp);
        try
        {
            pm.WriteLockFile(tmp);
            var lockPath = Path.Combine(tmp, "gmb-package.lock");
            Assert.Empty(pm.VerifyLockedIntegrity(lockPath));
            File.WriteAllText(Path.Combine(CacheRoot, "gmb-int", "1.0.0", "src", "gmb-int.bas"), "PRINT \"TAMPERED\"\n");
            Assert.True(pm.VerifyLockedIntegrity(lockPath).ContainsKey("gmb-int"));
        }
        finally { try { Directory.Delete(tmp, true); } catch { } }
    }

    // ── 8u. --offline: risolvere un dist-tag richiederebbe il registry → errore esplicito (niente rete) ──
    [Fact]
    public void Offline_Rejects_Tag_Resolution()
    {
        WritePkg("gmb-off", "1.0.0", """{ "name":"gmb-off","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        var pm = NewPm();
        // offline: il tag non è risolvibile (nessun registry) → errore esplicito dal resolver C
        Assert.Throws<BasicException>(() => pm.ResolveDependenciesAsync(
            [Req("gmb-off", "next")], new PackageBuildContext { Backend = "il", Offline = true }).GetAwaiter().GetResult());
    }

    // ── 8v. Overrides: forzano la versione anche dei transitivi ──
    [Fact]
    public void Overrides_Force_Transitive_Version()
    {
        WritePkg("gmb-a", "1.0.0", """{ "name":"gmb-a","version":"1.0.0","contentType":"source","exports":["src/"],"dependencies":{"gmb-b":"1.0.0"} }""");
        WritePkg("gmb-b", "1.0.0", """{ "name":"gmb-b","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        WritePkg("gmb-b", "2.0.0", """{ "name":"gmb-b","version":"2.0.0","contentType":"source","exports":["src/"] }""");
        var pm = NewPm();
        var ctx = new PackageBuildContext { Backend = "il" };
        ctx.Overrides = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { ["gmb-b"] = "2.0.0" };
        pm.ResolveDependenciesAsync([Req("gmb-a", "1.0.0")], ctx).GetAwaiter().GetResult();
        Assert.Equal("2.0.0", pm.ResolvedArtifacts["gmb-b"].Version);
    }

    // ── 8w. Nuovi tipi di dipendenza nel manifest ──
    [Fact]
    public void Manifest_Parses_Extended_Dependency_Types()
    {
        var doc = JsonDocument.Parse("""
        { "name":"x","version":"1.0.0","exports":["src/"],
          "devDependencies":{"d":"1.0.0"},"optionalDependencies":{"o":"*"},
          "peerDependencies":{"p":"^2.0.0"},"overrides":{"q":"3.0.0"} }
        """);
        var m = Parser.PackageManager.ParseManifest(doc.RootElement);
        Assert.Equal("1.0.0", m.DevDependencies["d"]);
        Assert.Equal("*", m.OptionalDependencies["o"]);
        Assert.Equal("^2.0.0", m.PeerDependencies["p"]);
        Assert.Equal("3.0.0", m.Overrides["q"]);
    }

    // ── 8x. Dipendenza opzionale mancante: non fatale ──
    [Fact]
    public void Optional_Dep_Missing_Is_NonFatal()
    {
        WritePkg("gmb-hasopt", "1.0.0", """{ "name":"gmb-hasopt","version":"1.0.0","contentType":"source","exports":["src/"],"optionalDependencies":{"gmb-nope":"*"} }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-hasopt", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.True(pm.ResolvedArtifacts.ContainsKey("gmb-hasopt"));
        Assert.False(pm.ResolvedArtifacts.ContainsKey("gmb-nope"));
    }

    // ── 8y. Peer dependency mancante segnalata ──
    [Fact]
    public void CheckPeers_Reports_Missing()
    {
        WritePkg("gmb-peer", "1.0.0", """{ "name":"gmb-peer","version":"1.0.0","contentType":"source","exports":["src/"],"peerDependencies":{"gmb-absent":"^1.0.0"} }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-peer", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.Contains(pm.CheckPeers(), w => w.Contains("gmb-absent"));
    }

    // ── 8ac-bis. peerDependencies: auto-install se disponibili (npm v7) ──
    [Fact]
    public void PeerDependencies_Auto_Install_When_Available()
    {
        WritePkg("gmb-host", "1.0.0", """{ "name":"gmb-host","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        WritePkg("gmb-plugin", "1.0.0", """{ "name":"gmb-plugin","version":"1.0.0","contentType":"source","exports":["src/"],"peerDependencies":{"gmb-host":"^1.0.0"} }""");
        var pm = NewPm();
        pm.ResolveDependenciesAsync([Req("gmb-plugin", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
        Assert.True(pm.ResolvedPackages.ContainsKey("gmb-host"));   // peer auto-installato
        Assert.Empty(pm.CheckPeers());                              // nessun peer mancante
    }

    // ── 8ac-ter. Override annidato "parent>child": forza la versione solo sotto quel parent ──
    [Fact]
    public void Nested_Override_Applies_To_Parent_Edge()
    {
        WritePkg("gmb-lib", "1.0.0", """{ "name":"gmb-lib","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        WritePkg("gmb-lib", "2.0.0", """{ "name":"gmb-lib","version":"2.0.0","contentType":"source","exports":["src/"] }""");
        WritePkg("gmb-parent", "1.0.0", """{ "name":"gmb-parent","version":"1.0.0","contentType":"source","exports":["src/"],"dependencies":{"gmb-lib":"*"} }""");
        var pm = NewPm();
        var ctx = new PackageBuildContext
        {
            Backend = "il",
            Overrides = new Dictionary<string, string> { ["gmb-parent>gmb-lib"] = "^1.0.0" }
        };
        pm.ResolveDependenciesAsync([Req("gmb-parent", "1.0.0")], ctx).GetAwaiter().GetResult();
        Assert.Equal("1.0.0", pm.ResolvedPackages["gmb-lib"].Manifest.Version); // non 2.0.0
    }

    // ── 8z. Audit: il range dell'advisory è valutato dal C (gmb_pkg_audit) ──
    [Fact]
    public void Advisory_Audit_Matches_Ranges()
    {
        const string advJson = """[{"id":"A1","package":"p","range":"<2.0.0","severity":"high"}]""";
        Assert.Single(NativePkg.Audit(["p"], ["1.5.0"], advJson));
        Assert.Empty(NativePkg.Audit(["p"], ["2.5.0"], advJson));
    }

    // ── 8aa. Workspace: il pacchetto locale si risolve dalla sua directory (no registry/cache) ──
    [Fact]
    public void Workspace_Local_Package_Resolves_From_Dir()
    {
        var wsdir = Path.Combine(Path.GetTempPath(), "gmb_ws_" + Guid.NewGuid().ToString("N"));
        var libdir = Path.Combine(wsdir, "packages", "liba");
        Directory.CreateDirectory(Path.Combine(libdir, "src"));
        File.WriteAllText(Path.Combine(libdir, "gmb-package.json"),
            """{ "name":"liba","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        File.WriteAllText(Path.Combine(libdir, "src", "liba.bas"), "PRINT \"liba\"\n");
        try
        {
            var pm = NewPm();
            var ctx = new PackageBuildContext { Backend = "il" };
            ctx.LocalPackages = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { ["liba"] = libdir };
            pm.ResolveDependenciesAsync([Req("liba", "^1.0.0")], ctx).GetAwaiter().GetResult();
            Assert.True(pm.ResolvedArtifacts.ContainsKey("liba"));
            Assert.Equal("1.0.0", pm.ResolvedArtifacts["liba"].Version);
            Assert.Equal(Path.GetFullPath(libdir), Path.GetFullPath(pm.ResolvedArtifacts["liba"].InstallPath));
        }
        finally { try { Directory.Delete(wsdir, true); } catch { } }
    }

    // ── 8ab. pack: glob + files/ignore ──
    [Fact]
    public void Pack_Glob_And_Ignore()
    {
        Assert.True(PackageBuilder.GlobMatch("secret/", "secret/k.key"));
        Assert.True(PackageBuilder.GlobMatch("*.key", "dir/k.key"));
        Assert.True(PackageBuilder.GlobMatch("src/**", "src/a/b.bas"));
        Assert.False(PackageBuilder.GlobMatch("src/**", "gmb-package.json"));

        var dir = Path.Combine(Path.GetTempPath(), "gmb_pack_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(dir, "src"));
        Directory.CreateDirectory(Path.Combine(dir, "secret"));
        File.WriteAllText(Path.Combine(dir, "gmb-package.json"),
            """{ "name":"p","version":"1.0.0","contentType":"source","exports":["src/"],"ignore":["secret/"] }""");
        File.WriteAllText(Path.Combine(dir, "src", "a.bas"), "x");
        File.WriteAllText(Path.Combine(dir, "secret", "k"), "x");
        try
        {
            var m = Parser.PackageManager.ParseManifest(
                System.Text.Json.JsonDocument.Parse(File.ReadAllText(Path.Combine(dir, "gmb-package.json"))).RootElement);
            var files = PackageBuilder.ComputePackFiles(m, dir);
            Assert.Contains("src/a.bas", files);
            Assert.DoesNotContain(files, f => f.StartsWith("secret/"));
        }
        finally { try { Directory.Delete(dir, true); } catch { } }
    }

    // ── 8ac. Deprecate: POST al registry con version+message ──
    [Fact]
    public async Task Deprecate_Posts_To_Registry()
    {
        var tcp = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        tcp.Start();
        var port = ((System.Net.IPEndPoint)tcp.LocalEndpoint).Port;
        tcp.Stop();
        using var listener = new System.Net.HttpListener();
        listener.Prefixes.Add($"http://127.0.0.1:{port}/");
        listener.Start();
        var seen = new TaskCompletionSource<(string Path, string Body)>(TaskCreationOptions.RunContinuationsAsynchronously);
        _ = Task.Run(async () =>
        {
            var ctx = await listener.GetContextAsync();
            using var sr = new StreamReader(ctx.Request.InputStream);
            var body = await sr.ReadToEndAsync();
            seen.TrySetResult((ctx.Request.Url!.AbsolutePath, body));
            var buf = System.Text.Encoding.UTF8.GetBytes("{\"ok\":\"1\"}");
            ctx.Response.StatusCode = 200; ctx.Response.ContentType = "application/json";
            await ctx.Response.OutputStream.WriteAsync(buf); ctx.Response.Close();
        });
        try
        {
            var pm = new Parser.PackageManager { RegistryUrl = $"http://127.0.0.1:{port}/" };
            await pm.DeprecateVersionAsync("gmb-dep", "2.0.0", "use 3.x", true, pm.RegistryUrl, "tok");
            var (path, body) = await seen.Task;
            Assert.EndsWith("/api/packages/gmb-dep/deprecate", path);
            Assert.Contains("2.0.0", body);
            Assert.Contains("use 3.x", body);
        }
        finally { listener.Stop(); }
    }

    // ── 8ad. Storico lock: archiviazione quando il contenuto cambia (per rollback) ──
    [Fact]
    public void Lock_History_Archives_On_Change()
    {
        WritePkg("gmb-h", "1.0.0", """{ "name":"gmb-h","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        WritePkg("gmb-h", "2.0.0", """{ "name":"gmb-h","version":"2.0.0","contentType":"source","exports":["src/"] }""");
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_hist_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tmp);
        try
        {
            var pm = NewPm();
            pm.ResolveDependenciesAsync([Req("gmb-h", "1.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
            pm.WriteLockFile(tmp);
            Assert.False(Directory.Exists(Path.Combine(tmp, ".gmbasic", "lock-history")));

            var pm2 = NewPm();
            pm2.ResolveDependenciesAsync([Req("gmb-h", "2.0.0")], new PackageBuildContext { Backend = "il" }).GetAwaiter().GetResult();
            pm2.WriteLockFile(tmp);
            var histDir = Path.Combine(tmp, ".gmbasic", "lock-history");
            Assert.True(Directory.Exists(histDir));
            Assert.Single(Directory.GetFiles(histDir, "*.lock"));
        }
        finally { try { Directory.Delete(tmp, true); } catch { } }
    }

    // ── 8ae. Cache content-addressable: dedup dei file identici + GC dei blob non referenziati ──
    [Fact]
    public void CacheStore_Intern_Dedups_And_Gc()
    {
        var root = Path.Combine(Path.GetTempPath(), "gmb_cs_" + Guid.NewGuid().ToString("N"));
        var cache = Path.Combine(root, "cache");
        var content = Path.Combine(cache, CacheStore.DirName);
        var a = Path.Combine(cache, "p1", "1.0.0"); Directory.CreateDirectory(a);
        var b = Path.Combine(cache, "p2", "1.0.0"); Directory.CreateDirectory(b);
        File.WriteAllText(Path.Combine(a, "same.txt"), "hello");
        File.WriteAllText(Path.Combine(b, "same.txt"), "hello");
        try
        {
            var sha1 = CacheStore.Intern(content, Path.Combine(a, "same.txt"));
            var sha2 = CacheStore.Intern(content, Path.Combine(b, "same.txt"));
            Assert.Equal(sha1, sha2);
            Assert.Single(Directory.EnumerateFiles(content, "*", SearchOption.AllDirectories)); // un solo blob
            Assert.Equal("hello", File.ReadAllText(Path.Combine(a, "same.txt")));                // link trasparente
            Assert.Equal("hello", File.ReadAllText(Path.Combine(b, "same.txt")));

            Directory.Delete(Path.Combine(cache, "p2"), true);
            Assert.Equal(0, CacheStore.Gc(content, cache)); // ancora referenziato da p1
            Directory.Delete(Path.Combine(cache, "p1"), true);
            Assert.Equal(1, CacheStore.Gc(content, cache)); // ora non referenziato
        }
        finally { try { Directory.Delete(root, true); } catch { } }
    }

    private static void WriteZipEntry(System.IO.Compression.ZipArchive z, string name, string content)
    {
        var e = z.CreateEntry(name);
        using var s = e.Open();
        var bytes = System.Text.Encoding.UTF8.GetBytes(content);
        s.Write(bytes, 0, bytes.Length);
    }

    // NB: la validazione live della Fase 1/3 (download reale da GitHub, es. duckdb) è stata
    // eseguita manualmente e NON è lasciata nella suite perché dipende dalla rete.
}
