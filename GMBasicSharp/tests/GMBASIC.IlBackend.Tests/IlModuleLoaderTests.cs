// Loader IL generico (IlModuleLoader): carica moduli .NET esterni con [GmbHModule] e ne
// espone i metodi pubblici statici come `Namespace.Method(...)` nel backend IL — uso
// generico, non limitato al PackageManager. Testa: Register(type), Load(path), chiamata
// con marshalling di double/string/bool e ritorno void, e pulizia Unregister.
using System.IO;
using System.Reflection;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class IlModuleLoaderTests
{
    [GmbHModule("TestNs")]
    public static class TestModule
    {
        public static double Double(double x) => x * 2;
        public static double Add(double a, double b) => a + b;
        public static string Greet(string name) => "hello " + name;
        public static bool IsPositive(double x) => x > 0;
        public static string Nope() => "done";
        public static void DoNothing() { }
    }

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp.slnx")))
                return dir.Parent?.FullName ?? dir.FullName;
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) return dir.FullName;
            dir = dir.Parent;
        }
        throw new DirectoryNotFoundException("GMBasicSharp.slnx non trovato risalendo da " + AppContext.BaseDirectory);
    }

    private static string RunBas(string source, string assemblyPrefix)
    {
        var main = Path.Combine(Path.GetTempPath(), "gmb_ilmod_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(main, source);
        try { return RunBasFile(main, assemblyPrefix); }
        finally { try { File.Delete(main); } catch { } }
    }

    private static string RunBasFile(string main, string assemblyPrefix)
    {
        var program = Basic.Orchestrator.Compile(main, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, assemblyPrefix + Guid.NewGuid().ToString("N"), out _);
            var dll = Path.Combine(Path.GetTempPath(), assemblyPrefix.TrimEnd('_') + ".dll");
            using (var fs = File.Create(dll)) asmBuilder.Save(fs);
            var ilDump = Path.Combine(Path.GetTempPath(), assemblyPrefix.TrimEnd('_') + "_" + Guid.NewGuid().ToString("N") + ".il");
            Basic.IlBackend.IlDisassembler.Dump(dll, ilDump);
            var assembly = Assembly.LoadFile(dll);
            var programType = assembly.GetType("GeneratedProgram")!;
            var mainMethod = programType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)!;

            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var task = Task.Run(() => mainMethod.Invoke(null, null));
                Assert.True(task.Wait(TimeSpan.FromSeconds(15)), "driver TIMEOUT\n" + sw);
                task.GetAwaiter().GetResult();
            }
            finally { Console.SetOut(originalOut); }
            return sw.ToString();
    }

    [Fact]
    public void Register_And_Call_ExternalMethods()
    {
        IlModuleLoader.Register(typeof(TestModule));
        try
        {
            var output = RunBas("""
                SUB Main()
                  PRINT TestNs.Double(21)
                  PRINT TestNs.Add(2, 3)
                  PRINT TestNs.Greet("bob")
                  IF TestNs.IsPositive(5) THEN PRINT "POS" ELSE PRINT "NEG"
                  TestNs.DoNothing()
                  PRINT TestNs.Nope()
                END SUB
                """, "IlMod_");
            Assert.Contains("42", output);
            Assert.Contains("5", output);
            Assert.Contains("hello bob", output);
            Assert.Contains("POS", output);
            Assert.Contains("done", output);
        }
        finally { IlModuleLoader.Unregister("TestNs"); }
    }

    [Fact]
    public void Load_Path_Registers_From_Assembly()
    {
        // il test assembly stesso contiene TestModule → Load deve registrarlo da disco
        var asmPath = typeof(TestModule).Assembly.Location;
        var before = IlModuleLoader.Namespaces.Count;
        var count = IlModuleLoader.Load(asmPath);
        Assert.True(count >= 1, "Load non ha trovato alcun modulo [GmbHModule]");
        Assert.Contains("TestNs", IlModuleLoader.Namespaces);
        Assert.True(IlModuleLoader.TryGet("TestNs", "Double", out var mi));
        Assert.Equal(typeof(double), mi!.ReturnType);
        Assert.True(before <= IlModuleLoader.Namespaces.Count);
        IlModuleLoader.Unregister("TestNs");
        Assert.DoesNotContain("TestNs", IlModuleLoader.Namespaces);
        Assert.False(IlModuleLoader.TryGet("TestNs", "Double", out _));
    }

    // REQUIRE di un pacchetto contentType dotnet → Orchestrator (FASE 3b) carica la .dll via
    // IlModuleLoader e i namespace diventano compilabili. Prova end-to-end col backend IL.
    [Fact]
    public void Require_DotNet_Package_AutoLoads_External_Module()
    {
        var cache = Path.Combine(Path.GetTempPath(), "gmb_ilmod_cache_" + Guid.NewGuid().ToString("N"));
        var pkg = Path.Combine(cache, "il-dotnet", "1.0.0");
        Directory.CreateDirectory(Path.Combine(pkg, "bin"));
        // la .dll del test assembly contiene TestModule ([GmbHModule("TestNs")])
        File.Copy(typeof(TestModule).Assembly.Location, Path.Combine(pkg, "bin", "GmbTest.dll"));
        File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
            """{ "name":"il-dotnet","version":"1.0.0","contentType":"dotnet","backends":["il"],"binaries":{"il":{"any-any":"bin/GmbTest.dll"}} }""");

        var main = Path.Combine(Path.GetTempPath(), "gmb_req_dotnet_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(main, """
            REQUIRE "il-dotnet" @ "1.0.0"
            SUB Main()
              PRINT TestNs.Add(20, 22)
              PRINT TestNs.Greet("pkg")
            END SUB
            """);
        try
        {
            Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
            try
            {
                IlModuleLoader.Unregister("TestNs"); // pulizia da test precedenti
                var output = RunBasFile(main, "IlReq_");
                Assert.Contains("42", output);
                Assert.Contains("hello pkg", output);
                Assert.Contains("TestNs", IlModuleLoader.Namespaces);
            }
            finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
        }
        finally
        {
            try { File.Delete(main); } catch { }
            try { Directory.Delete(cache, true); } catch { }
            IlModuleLoader.Unregister("TestNs");
        }
    }

    // Un pacchetto contentType dotnet richiesto su un backend che non può usarlo (vm) deve
    // dare un errore esplicito, non compilare e fallire tardi con "funzione non definita".
    [Fact]
    public void Require_DotNet_Package_On_Vm_Backend_Errors_Clearly()
    {
        var cache = Path.Combine(Path.GetTempPath(), "gmb_ilmod_cache_" + Guid.NewGuid().ToString("N"));
        var pkg = Path.Combine(cache, "il-only", "1.0.0");
        Directory.CreateDirectory(Path.Combine(pkg, "bin"));
        File.Copy(typeof(TestModule).Assembly.Location, Path.Combine(pkg, "bin", "GmbTest.dll"));
        File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
            """{ "name":"il-only","version":"1.0.0","contentType":"dotnet","backends":["il"],"binaries":{"il":{"any-any":"bin/GmbTest.dll"}} }""");

        var main = Path.Combine(Path.GetTempPath(), "gmb_req_vm_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(main, "REQUIRE \"il-only\" @ \"1.0.0\"\nSUB Main()\n  PRINT TestNs.Add(1, 1)\nEND SUB\n");
        try
        {
            Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
            try
            {
                var ex = Assert.Throws<Basic.BasicException>(() =>
                    Basic.Orchestrator.Compile(main, null, false, "vm"));
                Assert.Contains("non compatibile", ex.Message);
                Assert.Contains("il-only", ex.Message);
            }
            finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
        }
        finally
        {
            try { File.Delete(main); } catch { }
            try { Directory.Delete(cache, true); } catch { }
        }
    }

    // Consumo effettivo lato C99/VM: un pacchetto c-ffi risolto sul backend c99 espone
    // FfiLibs (path assoluti, pre-caricate dal VM/C via --ffi-lib) e i flag di link
    // C99 (-I/-L/-l) che RunC99Safe appende al comando cc.
    [Fact]
    public void Resolve_Cffi_Package_Exposes_FfiLibs_And_C99LinkFlags()
    {
        var platformKey = Parser.PackageManager.GetCurrentPlatformKey();
        var cache = Path.Combine(Path.GetTempPath(), "gmb_ilmod_cache_" + Guid.NewGuid().ToString("N"));
        var pkg = Path.Combine(cache, "phys", "0.9.0");
        Directory.CreateDirectory(Path.Combine(pkg, "lib"));
        Directory.CreateDirectory(Path.Combine(pkg, "include"));
        File.WriteAllText(Path.Combine(pkg, "lib", "libphys.dylib"), "dummy");
        File.WriteAllText(Path.Combine(pkg, "include", "phys.h"), "/* header */");
        File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
            "{ \"name\":\"phys\",\"version\":\"0.9.0\",\"contentType\":\"c-ffi\"," +
            "\"ffi\":{ \"api\":\"c-abi\",\"headers\":[\"include/phys.h\"]," +
            $"\"libs\":{{\"{platformKey}\":\"lib/libphys.dylib\"}},\"link\":\"-lphys\" }} }}");

        var main = Path.Combine(Path.GetTempPath(), "gmb_req_cffi_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(main, "REQUIRE \"phys\" @ \"0.9.0\"\nSUB Main()\nEND SUB\n");
        try
        {
            Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
            try
            {
                var program = Basic.Orchestrator.Compile(main, null, false, "c99");
                Assert.NotNull(program);
                Assert.Single(program.ResolvedFfiLibs);
                Assert.EndsWith("libphys.dylib", program.ResolvedFfiLibs[0], StringComparison.OrdinalIgnoreCase);
                Assert.Contains("-I", program.ResolvedC99LinkFlags ?? "");
                Assert.Contains("-L", program.ResolvedC99LinkFlags ?? "");
                Assert.Contains("-lphys", program.ResolvedC99LinkFlags ?? "");
            }
            finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
        }
        finally
        {
            try { File.Delete(main); } catch { }
            try { Directory.Delete(cache, true); } catch { }
        }
    }

    // I vm-chunk (.chunk/.prg) hanno un magic header (A1/GMBC "GMBC"+version, .prg "PCOD"):
    // la risoluzione REQUIRE lo valida, così un pacchetto vm-chunk con un file qualsiasi
    // viene rifiutato con errore chiaro, mentre un chunk reale passa.
    [Fact]
    public void Require_VmChunk_Package_Validates_Chunk_Magic()
    {
        var cache = Path.Combine(Path.GetTempPath(), "gmb_ilmod_cache_" + Guid.NewGuid().ToString("N"));
        var pkg = Path.Combine(cache, "vmmod", "1.0.0");
        Directory.CreateDirectory(Path.Combine(pkg, "bin"));
        File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
            """{ "name":"vmmod","version":"1.0.0","contentType":"vm-chunk","backends":["vm"],"binaries":{"vm":{"any-any":"bin/mod.chunk"}} }""");
        var chunkFile = Path.Combine(pkg, "bin", "mod.chunk");

        var main = Path.Combine(Path.GetTempPath(), "gmb_req_vmchunk_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(main, "REQUIRE \"vmmod\" @ \"1.0.0\"\nSUB Main()\nEND SUB\n");
        try
        {
            Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
            try
            {
                // chunk finto → errore chiaro (magic mancante)
                File.WriteAllText(chunkFile, "questo non e' un chunk");
                var ex = Assert.Throws<Basic.BasicException>(() =>
                    Basic.Orchestrator.Compile(main, null, false, "vm"));
                Assert.Contains("chunk", ex.Message);
                Assert.Contains("vmmod", ex.Message);

                // chunk reale (CompiledChunk serializzato in A1/GMBC) → passa
                var src = Path.Combine(Path.GetTempPath(), "gmb_vmchunk_src_" + Guid.NewGuid().ToString("N") + ".bas");
                File.WriteAllText(src, "SUB Main()\nEND SUB\n");
                try
                {
                    var prog = Basic.Orchestrator.Compile(src, null, false, "il");
                    var chunk = new global::Compiler.Compiler().Compile(prog);
                    File.WriteAllBytes(chunkFile, Basic.Runtime.ChunkA1Serializer.Serialize(chunk));
                    var program = Basic.Orchestrator.Compile(main, null, false, "vm");
                    Assert.NotNull(program);
                    Assert.NotNull(program.MainModule);
                }
                finally { try { File.Delete(src); } catch { } }
            }
            finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
        }
        finally
        {
            try { File.Delete(main); } catch { }
            try { Directory.Delete(cache, true); } catch { }
        }
    }

    // ChunkMerger: fonde i chunk VM dei pacchetti nel chunk principale con re-basing
    // (costanti/globali/indirizzi). Verifica: (a) la tabella procedure della lib è ribasata,
    // (b) il chunk fuso col Main della lib esegue il codice lib ribasato (global+string+call)
    // e stampa l'output giusto, (c) il chunk fuso col Main del programma principale resta
    // intatto (nessuna corruzione del bytecode).
    [Fact]
    public void ChunkMerger_Merges_VmChunk_Package_Into_Main()
    {
        var srcMain = Path.Combine(Path.GetTempPath(), "gmb_merge_main_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(srcMain, "SUB Main()\n  PRINT \"M\"\nEND SUB\n");
        var srcLib = Path.Combine(Path.GetTempPath(), "gmb_merge_lib_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(srcLib, """
            GLOBAL g AS LONG
            SUB Main()
              g = 40
              PrintIt()
            END SUB
            SUB PrintIt()
              PRINT "LIB:" + STR(g + 2)
            END SUB
            """);
        try
        {
            var progMain = Basic.Orchestrator.Compile(srcMain, null, false, "il");
            var progLib = Basic.Orchestrator.Compile(srcLib, null, false, "il");
            var mainChunk = new global::Compiler.Compiler().Compile(progMain);
            var libChunk = new global::Compiler.Compiler().Compile(progLib);
            var libPrintItOriginal = libChunk.ProcedureAddresses["PRINTIT"];

            // la lib arriva come FILE .chunk (A1/GMBC): passa da Serialize→Deserialize come
            // fa MergeVmChunks per i pacchetti vm-chunk.
            libChunk = Basic.Runtime.ChunkA1Serializer.Deserialize(
                Basic.Runtime.ChunkA1Serializer.Serialize(libChunk));

            var merged = global::Compiler.ChunkMerger.Merge(mainChunk, [libChunk]);

            // (a) rebasing strutturale: indirizzo procedura + globalCount
            Assert.Equal(libPrintItOriginal + mainChunk.Code.Count, merged.ProcedureAddresses["PRINTIT"]);
            Assert.True(merged.ProcedureArities.ContainsKey(libPrintItOriginal + mainChunk.Code.Count));
            Assert.Equal(mainChunk.GlobalCount + libChunk.GlobalCount, merged.GlobalCount);
            Assert.Equal(mainChunk.Code.Count + libChunk.Code.Count, merged.Code.Count);

            // (b) esecuzione del codice lib ribasato (entry = Main della lib, ribasato)
            var mergedLib = global::Compiler.ChunkMerger.Merge(mainChunk, [libChunk]);
            mergedLib.EntryPoint = mainChunk.Code.Count + libChunk.EntryPoint;
            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(mergedLib);
                Assert.True(r.Success, "chunk fuso (entry lib) fallito:\n" + r.Output);
                Assert.Contains("LIB: 42", r.Output);
            }
            finally { Console.SetOut(originalOut); }

            // (c) esecuzione col Main del programma principale (bytecode main non corrotto)
            var mergedMain = global::Compiler.ChunkMerger.Merge(mainChunk, [libChunk]);
            var sw2 = new StringWriter();
            try
            {
                Console.SetOut(sw2);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(mergedMain);
                Assert.True(r.Success, "chunk fuso (entry main) fallito:\n" + r.Output);
                Assert.Contains("M", r.Output);
            }
            finally { Console.SetOut(originalOut); }
        }
        finally
        {
            try { File.Delete(srcMain); } catch { }
            try { File.Delete(srcLib); } catch { }
        }
    }

    // ChunkMerger + OP_CALL_FFI_T (2026-10, fix regressione): i due case OP_CALL_FFI_T in
    // ApplyRedirects/RebaseCode si fermavano dopo il byte 'kind' da quando array/STRING/BYREF/
    // UDT/callback sono stati introdotti (sempre presenti nel bytecode da allora) — mai
    // esercitato prima perché nessun test mergeva un pacchetto con una DECLARE FFI tipizzata.
    // Il lib .bas sotto usa una FFI a struct/UDT (coda a lunghezza VARIABILE con DUE indici
    // costante PER FOGLIA da ribasare — il caso più esposto alla corruzione) e una stampa DOPO
    // la chiamata, per rivelare sia il disallineamento byte (ApplyRedirects) sia la mancata
    // ribasatura delle costanti foglia (RebaseCode). Verifica anche l'entry del MAIN dopo il
    // merge — bug gemello e scorrelato trovato nello stesso giro (ChunkMerger.cs linking
    // cross-chunk: l'alias "Nome#arità" di "Main"/"__main"/... non era escluso dal matching
    // per nome come il nome nudo, quindi "Main#0" di QUALUNQUE libreria rediriggeva "Main#0"
    // del programma principale — presente in ogni programma con 0 parametri — verso il Main
    // della libreria. Fix: esclusione per ADDRESS originale, non per stringa dell'alias).
    [Fact]
    public void ChunkMerger_Merges_TypedFfiStructCall_Correctly()
    {
        var srcMain = Path.Combine(Path.GetTempPath(), "gmb_merge_ffi_main_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(srcMain, "SUB Main()\n  PRINT \"M\"\nEND SUB\n");
        var ffiDir = Path.Combine(FindRepoRoot(), "shared", "minitests", "ffi");
        var srcLib = Path.Combine(ffiDir, "gmb_merge_ffi_lib_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(srcLib, """
            TYPE Point
                X AS DOUBLE
                Y AS DOUBLE
            END TYPE
            DECLARE FUNCTION VecMakePoint LIB "libvec_module" ALIAS "vec_make_point" (x AS DOUBLE, y AS DOUBLE) AS Point
            SUB Main()
              DIM p AS Point
              p = VecMakePoint(3, 4)
              PRINT "LIB:" + STR(p.X) + "," + STR(p.Y)
            END SUB
            """);
        // La DECLARE ... LIB "libvec_module" risolve relativa alla CWD del processo (vedi
        // FfiService.LoadLibrary) — come MiniTestsParityTests.cs, serve impostarla sulla
        // cartella del sample (dove sta libvec_module.dylib) prima di compilare/eseguire.
        var originalCwd = Directory.GetCurrentDirectory();
        try
        {
            Directory.SetCurrentDirectory(ffiDir);
            var progMain = Basic.Orchestrator.Compile(srcMain, null, false, "il");
            var progLib = Basic.Orchestrator.Compile(srcLib, null, false, "il");
            var mainChunk = new global::Compiler.Compiler().Compile(progMain);
            var libChunk = new global::Compiler.Compiler().Compile(progLib);
            libChunk = Basic.Runtime.ChunkA1Serializer.Deserialize(
                Basic.Runtime.ChunkA1Serializer.Serialize(libChunk));

            var mergedLib = global::Compiler.ChunkMerger.Merge(mainChunk, [libChunk]);
            mergedLib.EntryPoint = mainChunk.Code.Count + libChunk.EntryPoint;
            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(mergedLib);
                Assert.True(r.Success, "chunk fuso (entry lib, FFI struct) fallito:\n" + r.Output);
                Assert.Contains("LIB: 3, 4", r.Output);
            }
            finally { Console.SetOut(originalOut); }

            // Il chunk main (entry INVARIATA, non quella della lib) deve restare eseguibile e
            // intatto — nessuna corruzione residua dal merge della FFI tipizzata della lib, e
            // nessuna confusione tra il "Main" del programma e quello della libreria (bug
            // gemello risolto in ChunkMerger.cs, vedi ChunkMerger_Merges_VmChunk_Package_Into_Main).
            var mergedMain = global::Compiler.ChunkMerger.Merge(mainChunk, [libChunk]);
            var sw2 = new StringWriter();
            try
            {
                Console.SetOut(sw2);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(mergedMain);
                Assert.True(r.Success, "chunk fuso (entry main, FFI struct) fallito:\n" + r.Output);
                Assert.Contains("M", r.Output);
            }
            finally { Console.SetOut(originalOut); }
        }
        finally
        {
            Directory.SetCurrentDirectory(originalCwd);
            try { File.Delete(srcMain); } catch { }
            try { File.Delete(srcLib); } catch { }
        }
    }

    // End-to-end HTTP: REQUIRE di un pacchetto NON in cache → il FileNotFound in
    // ResolveDependenciesAsync scatena DownloadPackageAsync via registry HTTP
    // (GET /api/packages/{name} + GET /download/{name}/{ver}) → installa → compila gli
    // exports → il programma gira. Chiude l'ultimo gap di verifica (download reale).
    [Fact]
    public void Require_Http_Download_EndToEnd()
    {
        var listener = new System.Net.HttpListener();
        var port = GetFreePort();
        listener.Prefixes.Add($"http://127.0.0.1:{port}/");
        listener.Start();

        // staging del pacchetto → zip (come HandleDownload di PackageServer.bas)
        var staging = Path.Combine(Path.GetTempPath(), "gmb_http_pkg_" + Guid.NewGuid().ToString("N"));
        var zip = Path.Combine(Path.GetTempPath(), "gmb_http_pkg_" + Guid.NewGuid().ToString("N") + ".zip");
        Directory.CreateDirectory(Path.Combine(staging, "src"));
        File.WriteAllText(Path.Combine(staging, "gmb-package.json"),
            """{ "name":"gmb-http","version":"1.0.0","contentType":"source","exports":["src/"] }""");
        File.WriteAllText(Path.Combine(staging, "src", "httputil.bas"),
            "SUB PkgHttpGreet(name$)\n  PRINT \"hi \" + name$\nEND SUB\n");
        System.IO.Compression.ZipFile.CreateFromDirectory(staging, zip);
        var zipBytes = File.ReadAllBytes(zip);

        try
        {
            var done = new System.Threading.ManualResetEvent(false);
            _ = Task.Run(() =>
            {
                while (!done.WaitOne(0))
                {
                    try
                    {
                        var ctx = listener.GetContext();
                        var path = ctx.Request.Url!.AbsolutePath;
                        if (path.StartsWith("/api/packages/", StringComparison.OrdinalIgnoreCase))
                        {
                            var body = """{"versions":{"1.0.0":{"version":"1.0.0"}}}""";
                            var bytes = System.Text.Encoding.UTF8.GetBytes(body);
                            ctx.Response.ContentType = "application/json";
                            ctx.Response.ContentLength64 = bytes.Length;
                            ctx.Response.OutputStream.Write(bytes);
                        }
                        else if (path.StartsWith("/download/", StringComparison.OrdinalIgnoreCase))
                        {
                            ctx.Response.ContentType = "application/zip";
                            ctx.Response.ContentLength64 = zipBytes.Length;
                            ctx.Response.OutputStream.Write(zipBytes);
                        }
                        else
                        {
                            ctx.Response.StatusCode = 404;
                        }
                        ctx.Response.Close();
                    }
                    catch { break; }
                }
            });

            var cache = Path.Combine(Path.GetTempPath(), "gmb_http_cache_" + Guid.NewGuid().ToString("N"));
            var main = Path.Combine(Path.GetTempPath(), "gmb_http_main_" + Guid.NewGuid().ToString("N") + ".bas");
            File.WriteAllText(main, """
                REQUIRE "gmb-http" @ "1.0.0"
                SUB Main()
                  PkgHttpGreet("http")
                END SUB
                """);
            try
            {
                Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
                Environment.SetEnvironmentVariable("GMB_REGISTRY", $"http://127.0.0.1:{port}");
                try
                {
                    var output = RunBasFile(main, "IlHttp_");
                    Assert.Contains("hi http", output);
                    Assert.True(File.Exists(Path.Combine(cache, "gmb-http", "1.0.0", "src", "httputil.bas")),
                        "pacchetto non scaricato nella cache dal registry HTTP");
                }
                finally
                {
                    Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null);
                    Environment.SetEnvironmentVariable("GMB_REGISTRY", null);
                }
            }
            finally { done.Set(); try { File.Delete(main); } catch { } try { Directory.Delete(cache, true); } catch { } }
        }
        finally
        {
            listener.Stop();
            try { Directory.Delete(staging, true); } catch { }
            try { File.Delete(zip); } catch { }
        }
    }

    private static int GetFreePort()
    {
        var l = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        l.Start();
        var port = ((System.Net.IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return port;
    }

    // Consumo FFI del VM managed: FfiService.Preload registra le lib dei pacchetti (path
    // assoluto) e LoadLibrary("nome") le risolve per basename (stessa semantica di
    // gmb_ffi_dl_open nel runtime C) → DECLARE LIB "nome" funziona anche da cache.
    [Fact]
    public void FfiService_Preload_Resolves_ByBasename()
    {
        var libm = "/usr/lib/libm.dylib";
        if (!File.Exists(libm)) return; // non-macOS: salta
        var svc = new global::Basic.Runtime.Services.FfiService();
        var h = global::Basic.Runtime.Services.FfiService.Preload(libm);
        Assert.NotEqual(IntPtr.Zero, h);
        var h2 = svc.LoadLibrary("m"); // basename senza prefisso lib- né estensione
        Assert.Equal(h, h2);
        var sin = svc.GetFunction(h2, "sin");
        Assert.NotEqual(IntPtr.Zero, sin);
        var v = svc.CallFunction(sin, new object[] { 0.5 });
        Assert.InRange(Convert.ToDouble(v), 0.47, 0.48);
    }

    // Regressione: un pacchetto mixed con binaries[c99] NON va classificato DotNet (il
    // binario != .dll IL): il C99 lo linka (CFFI + -l) e l'IL non tenta di caricarlo come
    // assembly .NET.
    [Fact]
    public void Mixed_C99_Binary_Classified_As_Cffi_And_Linked()
    {
        var cache = Path.Combine(Path.GetTempPath(), "gmb_ilmod_cache_" + Guid.NewGuid().ToString("N"));
        var pkg = Path.Combine(cache, "geom", "1.0.0");
        Directory.CreateDirectory(Path.Combine(pkg, "lib"));
        File.WriteAllText(Path.Combine(pkg, "lib", "libgeom.so"), "dummy");
        File.WriteAllText(Path.Combine(pkg, "lib", "libgeom.dll"), "dummy");
        File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
            """
            { "name":"geom","version":"1.0.0","contentType":"mixed",
              "binaries":{ "c99":{"any-any":"lib/libgeom.so"},
                           "il":{"any-any":"lib/libgeom.dll"} } }
            """);

        var main = Path.Combine(Path.GetTempPath(), "gmb_req_geom_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(main, "REQUIRE \"geom\" @ \"1.0.0\"\nSUB Main()\nEND SUB\n");
        try
        {
            Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
            try
            {
                // classificazione lato risolutore: su c99 il binario è C → CFFI, su il è .dll → DotNet
                var pm = new Parser.PackageManager { CacheRoot = cache };
                var req = new RequireStmt(new StringExpr("geom", new Token(TokenKind.StringLiteral, "geom", "geom", SourcePosition.Unknown)), null,
                    new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown));
                pm.ResolveDependenciesAsync([req], new PackageBuildContext { Backend = "c99", PlatformKey = Parser.PackageManager.GetCurrentPlatformKey() }).GetAwaiter().GetResult();
                Assert.Equal(PackageContentType.CFFI, pm.ResolvedArtifacts["geom"].ContentType);
                Assert.NotNull(pm.ResolvedArtifacts["geom"].BinaryPath);
                Assert.EndsWith(".so", pm.ResolvedArtifacts["geom"].BinaryPath!, StringComparison.OrdinalIgnoreCase);

                // il backend → .dll → DotNet (non CFFI): nessun caricamento .so come IL
                var pm2 = new Parser.PackageManager { CacheRoot = cache };
                pm2.ResolveDependenciesAsync([req], new PackageBuildContext { Backend = "il", PlatformKey = Parser.PackageManager.GetCurrentPlatformKey() }).GetAwaiter().GetResult();
                Assert.Equal(PackageContentType.DotNet, pm2.ResolvedArtifacts["geom"].ContentType);

                // il C99 emitter riceve -L/-l per il binario C selezionato
                var program = Basic.Orchestrator.Compile(main, null, false, "c99");
                Assert.NotNull(program);
                Assert.Contains("-lgeom", program.ResolvedC99LinkFlags ?? "");
                Assert.Contains("-L", program.ResolvedC99LinkFlags ?? "");
            }
            finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
        }
        finally
        {
            try { File.Delete(main); } catch { }
            try { Directory.Delete(cache, true); } catch { }
        }
    }

    // Chiamata diretta cross-chunk senza nuovo syntax: le exports del pacchetto vm-chunk
    // dichiarano l'API (stub), il main la chiama normalmente, il ChunkMerger LINka le chiamate
    // compile-time verso l'implementazione nel chunk del pacchetto.
    [Fact]
    public void VmChunk_Exports_Api_Direct_Call_Linked_To_Chunk()
    {
        var cache = Path.Combine(Path.GetTempPath(), "gmb_ilmod_cache_" + Guid.NewGuid().ToString("N"));
        var pkg = Path.Combine(cache, "pkglib", "1.0.0");
        Directory.CreateDirectory(Path.Combine(pkg, "api"));
        Directory.CreateDirectory(Path.Combine(pkg, "bin"));
        File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
            """{ "name":"pkglib","version":"1.0.0","contentType":"vm-chunk","backends":["vm"],"exports":["api/"],"binaries":{"vm":{"any-any":"bin/pkglib.chunk"}} }""");
        // API (stub compilato nel programma)
        File.WriteAllText(Path.Combine(pkg, "api", "pkga.bas"), "SUB PkgHello()\nEND SUB\n");
        // implementazione reale nel chunk del pacchetto (compilata standalone)
        var libSrc = Path.Combine(Path.GetTempPath(), "gmb_pkglib_src_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(libSrc, """
            SUB Main()
            END SUB
            SUB PkgHello()
              PRINT "from-lib"
            END SUB
            """);
        try
        {
            var libProg = Basic.Orchestrator.Compile(libSrc, null, false, "il");
            var libChunk = new global::Compiler.Compiler().Compile(libProg);
            File.WriteAllBytes(Path.Combine(pkg, "bin", "pkglib.chunk"),
                Basic.Runtime.ChunkA1Serializer.Serialize(libChunk));

            var main = Path.Combine(Path.GetTempPath(), "gmb_pkglib_main_" + Guid.NewGuid().ToString("N") + ".bas");
            File.WriteAllText(main, """
                REQUIRE "pkglib" @ "1.0.0"
                SUB Main()
                  PkgHello()
                END SUB
                """);
            try
            {
                Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
                try
                {
                    var program = Basic.Orchestrator.Compile(main, null, false, "vm");
                    Assert.NotNull(program);
                    Assert.Contains("PKGHELLO", program.ChunkExternProcedureNames);
                    Assert.Single(program.ResolvedVmChunks);
                    Assert.Single(program.ResolvedExportsFiles);

                    var mainChunk = new global::Compiler.Compiler().Compile(program);
                    var lib = Basic.Runtime.ChunkA1Serializer.Deserialize(
                        File.ReadAllBytes(program.ResolvedVmChunks[0]));
                    var merged = global::Compiler.ChunkMerger.Merge(mainChunk, [lib], program.ChunkExternProcedureNames);

                    var originalOut = Console.Out;
                    var sw = new StringWriter();
                    try
                    {
                        Console.SetOut(sw);
                        Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                        var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(merged);
                        Assert.True(r.Success, "chunk fuso (chiamata diretta) fallito:\n" + r.Output);
                        Assert.Contains("from-lib", r.Output); // implementazione dal CHUNK, non dallo stub
                    }
                    finally { Console.SetOut(originalOut); }
                }
                finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
            }
            finally { try { File.Delete(main); } catch { } }
        }
        finally
        {
            try { File.Delete(libSrc); } catch { }
            try { Directory.Delete(cache, true); } catch { }
        }
    }

    // PackageManager-BSP §1 — vm-chunk STANDALONE: un pacchetto il cui binario è un chunk
    // A1/GMBC completo (con il suo SUB Main) si esegue come PROGRAMMA, non si fonde come
    // libreria. Prova end-to-end: pacchetto installato in cache → risoluzione →
    // ResolveVmChunkPath → deserializza → PCodeVmRunner esegue il MAIN del pacchetto →
    // output atteso.
    [Fact]
    public void PackageRun_Standalone_VmChunk_Executes_Package_Main()
    {
        var cache = Path.Combine(Path.GetTempPath(), "gmb_pkgrun_cache_" + Guid.NewGuid().ToString("N"));
        var pkg = Path.Combine(cache, "greeter", "1.0.0");
        Directory.CreateDirectory(Path.Combine(pkg, "bin"));
        File.WriteAllText(Path.Combine(pkg, "gmb-package.json"),
            """{ "name":"greeter","version":"1.0.0","contentType":"vm-chunk","backends":["vm"],"binaries":{"vm":{"any-any":"bin/greeter.chunk"}} }""");

        // chunk standalone: il pacchetto È il programma (ha il suo SUB Main con output)
        var src = Path.Combine(Path.GetTempPath(), "gmb_pkgrun_src_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(src, """
            SUB Main()
              PRINT "GREETER-STANDALONE"
            END SUB
            """);
        try
        {
            var prog = Basic.Orchestrator.Compile(src, null, false, "il");
            var chunk = new global::Compiler.Compiler().Compile(prog);
            File.WriteAllBytes(Path.Combine(pkg, "bin", "greeter.chunk"),
                Basic.Runtime.ChunkA1Serializer.Serialize(chunk));

            Environment.SetEnvironmentVariable("GMB_PKG_CACHE", cache);
            try
            {
                // PACKAGE RUN: risolve il chunk standalone dal pacchetto in cache
                var pm = new Parser.PackageManager { CacheRoot = cache };
                using var doc = System.Text.Json.JsonDocument.Parse(
                    File.ReadAllText(Path.Combine(pkg, "gmb-package.json")));
                var manifest = Parser.PackageManager.ParseManifest(doc.RootElement);
                var installed = new global::Basic.InstalledPackage(manifest, pkg);
                var chunkPath = pm.ResolveVmChunkPath(installed);
                Assert.NotNull(chunkPath);
                Assert.True(File.Exists(chunkPath));

                // deserializza (come HandlePackageRunAsync) ed esegue il Main del pacchetto
                var standalone = Basic.Runtime.ChunkA1Serializer.Deserialize(File.ReadAllBytes(chunkPath!));
                var originalOut = Console.Out;
                var sw = new StringWriter();
                try
                {
                    Console.SetOut(sw);
                    Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                    var r = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(standalone);
                    Assert.True(r.Success, "esecuzione standalone fallita:\n" + r.Output);
                    Assert.Contains("GREETER-STANDALONE", r.Output);
                }
                finally { Console.SetOut(originalOut); }
            }
            finally { Environment.SetEnvironmentVariable("GMB_PKG_CACHE", null); }
        }
        finally
        {
            try { File.Delete(src); } catch { }
            try { Directory.Delete(cache, true); } catch { }
        }
    }
}
