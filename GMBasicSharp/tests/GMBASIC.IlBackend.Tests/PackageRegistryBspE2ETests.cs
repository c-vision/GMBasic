// PackageManager-BSP §3 — Frontend BSP del registry: verifica di integrità.
// Verifica che gli artefatti del §3 siano coerenti (senza server HTTP in-process,
// che su PCodeVM richiede il loop REPL e non è affidabile nei test):
//  1. PackageServer.bas (registry reale) compila;
//  2. le pagine BSP index.bsp/package.bsp compilano (SourcePipeline, come BSP <dir>);
//  3. le pagine usano lo schema minuscolo del registry reale (name/version/
//     contentType/edition/backends) e montano il frontend alla radice;
//  4. il registry reale espone il frontendDir + HTTP STATIC "/" per servire le pagine.
// La verifica end-to-end HTTP (GET /api/packages, /index.bsp renderizzato) è fatta
// manualmente via REPL (backend PCodeVM, curl 200) e documentata in Session-Log.
using System.IO;
using Xunit;
using Basic.IlBackend;
using Basic.Runtime;

namespace Basic.IlBackend.Tests;

public class PackageRegistryBspFrontendTests
{
    /// <summary>Radice del monorepo: la directory che contiene GMBasicSharp/ e shared/.</summary>
    private static string FindRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "GMBasicSharp"))
                && Directory.Exists(Path.Combine(dir.FullName, "shared")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    [Fact]
    public void PackageServer_Compiles()
    {
        var root = FindRoot();
        var sample = Path.Combine(root, "shared", "samples", "PackageRegistry", "PackageServer.bas");
        Assert.True(File.Exists(sample));
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program);
        Assert.NotNull(program.MainModule);
    }

    [Fact]
    public void RegistryBsp_Pages_Compile_And_Use_Lowercase_Schema()
    {
        var root = FindRoot();
        var pages = Path.Combine(root, "shared", "samples", "Web", "RegistryBsp");
        var index = Path.Combine(pages, "index.bsp");
        var detail = Path.Combine(pages, "package.bsp");
        Assert.True(File.Exists(index));
        Assert.True(File.Exists(detail));

        // compilazione BSP (SourcePipeline, come BSP <dir> / TryServeBspPage lazy)
        Assert.True(global::Basic.Pipeline.SourcePipeline.CompileBspPage(index, out _),
            "index.bsp non compila");
        Assert.True(global::Basic.Pipeline.SourcePipeline.CompileBspPage(detail, out _),
            "package.bsp non compila");

        // Fase D3: tutte le pagine BSP del registry compilano (vincolo §6bis)
        foreach (var page in new[] { "register.bsp", "publish.bsp", "owners.bsp", "yank.bsp", "admin.bsp" })
        {
            var p = Path.Combine(pages, page);
            Assert.True(File.Exists(p), $"{page} mancante");
            Assert.True(global::Basic.Pipeline.SourcePipeline.CompileBspPage(p, out _),
                $"{page} non compila");
        }

        // #8: admin.bsp è un PANNELLO funzionante (form con azioni che chiamano le API admin),
        // non più una pagina statica: contiene i form reserve/assign/revoke/usercreate/userstatus
        var adminSrc = File.ReadAllText(Path.Combine(pages, "admin.bsp"));
        Assert.Contains("QUERY[\"action\"]", adminSrc);
        Assert.Contains("users/create", adminSrc);
        Assert.Contains("reserve", adminSrc);
        Assert.Contains("assign", adminSrc);
        Assert.Contains("userstatus", adminSrc);
        Assert.Contains("BearerToken", adminSrc);

        // schema minuscolo (parità col registry reale PackageServer.bas, che espone
        // name/version/description/contentType/edition/backends)
        var indexSrc = File.ReadAllText(index);
        Assert.Contains("JSON.KEYS(pkgs)", indexSrc);
        Assert.Contains("pkgs[k][\"name\"]", indexSrc);
        var detailSrc = File.ReadAllText(detail);
        Assert.Contains("m[\"name\"]", detailSrc);
        Assert.Contains("m[\"contentType\"]", detailSrc);
        Assert.Contains("m[\"edition\"]", detailSrc);
        Assert.Contains("m[\"backends\"]", detailSrc);
        Assert.Contains("m[\"versions\"]", detailSrc);
    }

    [Fact]
    public void PackageServer_Mounts_Frontend_And_Index_Route()
    {
        var root = FindRoot();
        var sample = Path.Combine(root, "shared", "samples", "PackageRegistry", "PackageServer.bas");
        var src = File.ReadAllText(sample);

        // il registry reale serve le pagine BSP alla radice (route / + HTTP STATIC "/")
        Assert.Contains("frontendDir", src);
        Assert.Contains("HTTP STATIC \"/\", frontendDir", src);
        Assert.Contains("HTTP ROUTE \"GET\", \"/\", HandleIndex()", src);
        Assert.Contains("SUB HandleIndex", src);
        // campi estesi dell'API usati dalle pagine
        Assert.Contains("pkgData[\"contentType\"]", src);
        Assert.Contains("pkgData[\"edition\"]", src);
        Assert.Contains("pkgData[\"backends\"]", src);
        Assert.Contains("pkgData[\"version\"]", src);
    }

    [Fact]
    public void PackageServer_FaseA_Account_Claim_Immutabilita()
    {
        var root = FindRoot();
        var sample = Path.Combine(root, "shared", "samples", "PackageRegistry", "PackageServer.bas");
        var src = File.ReadAllText(sample);

        // Fase A1: utenti autorizzati gestiti SOLO dall'admin (niente self-register);
        // il token opaco è PASSWORDHASH (MAI in chiaro) e l'utente nasce "blocked"
        Assert.DoesNotContain("HandleApiUserRegister", src);
        Assert.Contains("newUserUC[\"tokenHash\"] = CRYPTOGRAPHY.PASSWORDHASH(tokenUC)", src);
        Assert.Contains("CRYPTOGRAPHY.PASSWORDVERIFY(token, u.tokenHash)", src);
        Assert.Contains("newUserUC[\"status\"] = \"blocked\"", src);

        // Fase A2: auth per-utente via Authorization: Bearer
        Assert.Contains("FUNCTION ApiAuthUser(req AS HTTPREQUEST) AS STRING", src);
        Assert.Contains("req.Headers.Get(\"Authorization\")", src);
        Assert.Contains("\"Bearer \"", src);

        // Fase A3: claim del nome al primo publish + ownedPackages (via AddOwned/RemoveOwned)
        Assert.Contains("gClaims[pname]", src);
        Assert.Contains("newClaim[\"owner\"] = ownerName", src);
        Assert.Contains("AddOwned(ownerName, pname)", src);
        Assert.Contains("SUB AddOwned", src);

        // Fase A4: immutabilità versione (409), niente RMDIR sul pacchetto pubblicato
        Assert.Contains("Versione già esistente", src);
        Assert.Contains("StatusCode = 409", src);
        Assert.DoesNotContain("IF IO.DirExists(destDir) THEN RMDIR destDir", src);

        // Fase A5: gestione owners (collaboratori), solo l'owner può modificare
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/packages/{name}/owners\", HandleApiOwners()", src);
        Assert.Contains("SUB HandleApiOwners", src);
        Assert.Contains("Solo l'owner può gestire i collaboratori", src);
        Assert.Contains("claim.collaborators[userO] = \"1\"", src);

        // Fase A6: yank (410 sul download delle versioni ritirate)
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/packages/{name}/yank\", HandleApiYank()", src);
        Assert.Contains("SUB HandleApiYank", src);
        Assert.Contains("FUNCTION yankedPath", src);
        Assert.Contains("StatusCode = 410", src);

        // Fase B/C1: registrazione pacchetti contributor (modello GO) via POST /api/packages/register
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/packages/register\", HandleApiRegisterRepo()", src);
        Assert.Contains("SUB HandleApiRegisterRepo", src);
        Assert.Contains("reposPath", src);
        Assert.Contains("\"status\"", src);

        // Fase C3bis: materializzazione binari (URL → cache locale, mai servire URL)
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/packages/{name}/materialize\", HandleApiMaterialize()", src);
        Assert.Contains("SUB HandleApiMaterialize", src);
        Assert.Contains("binDir", src);
        Assert.Contains("CRYPTOGRAPHY.FROMBASE64", src);
        Assert.Contains("\"/bin\"", src);

        // Fase C4: sync tag di un pacchetto contributor
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/packages/{name}/sync\", HandleApiSync()", src);
        Assert.Contains("SUB HandleApiSync", src);
        Assert.Contains("entryS[\"tag\"] = tagS", src);

        // Sistema admin: assegnazione/revoca permessi sui nomi (admin + utenti nella config unica)
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/admin/reserve\", HandleApiAdminReserve()", src);
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/admin/assign\", HandleApiAdminAssign()", src);
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/admin/revoke\", HandleApiAdminRevoke()", src);
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/admin/password\", HandleApiAdminPassword()", src);
        Assert.Contains("FUNCTION IsAdmin", src);
        Assert.Contains("gmb_admin", src); // token admin di primo accesso
        // il publish su nome riservato è bloccato
        Assert.Contains("Nome '\" + pname + \"' riservato dall'admin", src);
        Assert.Contains("StatusCode = 403", src);

        // #9: enforcement token admin default — IsAdmin restituisce "default" e AdminOk → 403
        Assert.Contains("FUNCTION AdminOk(req AS HTTPREQUEST, res AS HTTPRESPONSE) AS STRING", src);
        Assert.Contains("IsAdmin = \"default\"", src);
        Assert.Contains("PASSWORDVERIFY(\"gmb_admin\", gAdmin.tokenHash)", src);
        Assert.Contains("Il token admin è ancora il default", src);

        // Gestione utenti autorizzati: SOLO admin (niente self-register) + status active/blocked
        Assert.DoesNotContain("HTTP ROUTE \"POST\", \"/api/users/register\"", src); // rimosso
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/admin/users/create\", HandleApiAdminUserCreate()", src);
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/admin/users/setkey\", HandleApiAdminUserSetKey()", src);
        Assert.Contains("HTTP ROUTE \"POST\", \"/api/admin/users/status\", HandleApiAdminUserStatus()", src);
        Assert.Contains("HTTP ROUTE \"GET\", \"/api/admin/users/list\", HandleApiAdminUserList()", src);
        Assert.Contains("u.status = \"blocked\"", src);      // create parte bloccato
        Assert.Contains("UserStatus", src);                  // helper status
        Assert.Contains("UserPublicKey", src);               // helper chiave pubblica
        Assert.Contains("status = \"blocked\"", src);

        // Config unica (package-registry.json) con admin+users+limiti; RemoteIp per il rate-limit
        Assert.Contains("maxPackageBytes", src);
        Assert.Contains("rateLimitPerMinute", src);
        Assert.Contains("SaveConfig()", src);
        Assert.Contains("req.RemoteIp", src);
        Assert.Contains("RateLimitOk", src);

        // Verifica firma lato server (#3): chiave pubblica in config + VERIFYRAW
        Assert.Contains("VERIFYRAW", src);
        Assert.Contains("Pacchetto non firmato ma l'utente", src);
        Assert.Contains("Chiave pubblica del pacchetto non corrisponde", src);
        Assert.Contains("Firma non valida", src);

        // Quota dimensione (#2)
        Assert.Contains("StatusCode = 413", src);
        Assert.Contains("Pacchetto troppo grande", src);
    }

    [Fact]
    public void PackageServer_Advisories_Endpoint()
    {
        var root = FindRoot();
        var sample = Path.Combine(root, "shared", "samples", "PackageRegistry", "PackageServer.bas");
        var src = File.ReadAllText(sample);

        // GET /api/advisories — DB advisory di sicurezza interrogato da `gmb pkg audit`.
        Assert.Contains("HTTP ROUTE \"GET\", \"/api/advisories\", HandleApiAdvisories()", src);
        Assert.Contains("SUB HandleApiAdvisories", src);
        Assert.Contains("advisories.json", src);
        Assert.Contains("FUNCTION ReadOneLine", src);
    }

    [Fact]
    public void ScopedName_ValidAndSafePath()
    {
        // Fase D1: il nome contributor "@scope/name" è valido e mappa a un path cache sicuro
        Assert.True(Basic.PackageBuilder.IsValidName("@marco/geom"));
        Assert.True(Basic.PackageBuilder.IsValidName("geom"));
        Assert.False(Basic.PackageBuilder.IsValidName("Marco/geom")); // uppercase scope non ok
        Assert.False(Basic.PackageBuilder.IsValidName("@marco/Geom")); // uppercase name non ok
        // nessun "/" nel path cache
        var safe = Parser.PackageManager.SafePackageDir("@marco/geom");
        Assert.DoesNotContain('/', safe);
        Assert.Contains('_', safe);
        Assert.Equal("@marco_geom", safe);
    }

    [Fact]
    public void PackageManifest_Parses_TestSection()
    {
        // Fase B1: il manifest esteso legge il campo "test" (entry + backends)
        const string json = """
            { "name": "geom", "version": "0.9.1", "contentType": "source", "backends": ["il"],
              "exports": ["src/"], "test": { "entry": "test/self_test.bas", "backends": ["il"] } }
            """;
        using var doc = System.Text.Json.JsonDocument.Parse(json);
        var m = Parser.PackageManager.ParseManifest(doc.RootElement);
        Assert.NotNull(m.Test);
        Assert.Equal("test/self_test.bas", m.Test!.Entry);
        Assert.Contains("il", m.Test.Backends);
    }

    [Fact]
    public void GmbHConfig_Has_Username_And_Repos()
    {
        // Fase B1: GmbHConfig estesa con Username (account) e Repos (nome → git-url)
        var cfg = new Basic.GmbConfig { Username = "marco", RegistryUrl = "http://r:1" };
        cfg.Repos["geom"] = "https://github.com/marco/gmb-geom";
        Assert.Equal("marco", cfg.Username);
        Assert.True(cfg.Repos.ContainsKey("geom"));
        Assert.Equal("https://github.com/marco/gmb-geom", cfg.Repos["geom"]);
    }

    [Fact]
    public void SystemShell_Compiles_Parse_And_Link()
    {
        // SHELL/EXEC: il linguaggio espone SYSTEM.SHELL (comando), SYSTEM.SHELLOUT/EXECCODE
        // (funzioni) e SET SHELL ON|OFF (opt-in). Verifichiamo che compili e linki.
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_shell_test_" + Guid.NewGuid().ToString("N")[..6] + ".bas");
        File.WriteAllText(tmp, """
            SET SHELL ON
            DIM out AS STRING
            out = SYSTEM.SHELLOUT("echo hi")
            DIM code AS LONG
            code = SYSTEM.SHELLCODE("exit 0")
            SHELL "echo done"
            """);
        try
        {
            var program = Basic.Orchestrator.Compile(tmp, null, false);
            Assert.NotNull(program);
            Assert.NotNull(program.MainModule);
        }
        finally
        {
            try { File.Delete(tmp); } catch { }
        }
    }

    [Fact]
    public void BspCompiler_Sections_RenderSection_And_Partial()
    {
        // Fase B (View engine): @section / @RenderSection / @Html.Partial con @LAYOUT.
        var dir = Path.Combine(Path.GetTempPath(), "gmb_bsp_mvc_" + Guid.NewGuid().ToString("N")[..6]);
        Directory.CreateDirectory(dir);
        try
        {
            var layoutPath = Path.Combine(dir, "_Layout.bsp");
            File.WriteAllText(layoutPath, """
                <html><head><title>@RenderSection("Title")</title></head>
                <body><main><% RENDERBODY %></main><footer>@RenderSection("Footer")</footer></body></html>
                """);
            var partialPath = Path.Combine(dir, "header.bsp");
            File.WriteAllText(partialPath, "<nav>Home</nav>");
            var pagePath = Path.Combine(dir, "page.bsp");
            File.WriteAllText(pagePath, """
                <%@ LAYOUT "_Layout.bsp" %>
                @section Title { Pagina }
                @section Footer { &copy; 2026 }
                <h1>Contenuto</h1>
                @Html.Partial("header.bsp")
                """);

            var basic = CommonLib.Http.BspCompiler.Compile(File.ReadAllText(pagePath), pagePath);

            // le sezioni sono impostate PRIMA del layout (PAGE["__sec_..."] = "..."), e il
            // layout le renderizza con PRINT PAGE["__sec_..."] (dal marker @RenderSection)
            var secIdx = basic.IndexOf("PAGE[\"__sec_Title\"] =", StringComparison.Ordinal);
            var headIdx = basic.IndexOf("PRINT PAGE[\"__sec_Title\"]", StringComparison.Ordinal);
            Assert.True(secIdx >= 0 && headIdx >= 0, "sezione Title impostata e renderizzata");
            Assert.True(secIdx < headIdx, "la sezione Title è impostata PRIMA di essere renderizzata");
            Assert.Contains("PAGE[\"__sec_Footer\"] =", basic);
            Assert.Contains("PRINT PAGE[\"__sec_Footer\"]", basic);
            // il partial è inlilato
            Assert.Contains("<nav>Home</nav>", basic);
            // il corpo è iniettato nel RENDERBODY del layout
            Assert.Contains("<h1>Contenuto</h1>", basic);

            // la pagina precompila (come fa TryServeBspPage / BSP <dir>)
            Assert.True(global::Basic.Pipeline.SourcePipeline.CompileBspPage(pagePath, out _),
                "page.bsp con @section/@RenderSection/@Html.Partial non precompila");
        }
        finally
        {
            try { Directory.Delete(dir, true); } catch { }
        }
    }

    [Fact]
    public void BspCompiler_Model_Property_Expression()
    {
        // Fase B: @Model.Prop → PRINT STRINGS.HTMLENCODE(MODEL.Prop); (con <%@ MODEL Tipo %>
        // che genera DIM MODEL AS Tipo = NEW Tipo). Anti-XSS default (2026-09-02): l'output
        // è HTML-escaped; per HTML fidato usare @Html.Raw(Model.Prop).
        var src = "<%@ MODEL Utente %>\n<p>Ciao @Model.Nome</p>\n";
        var basic = CommonLib.Http.BspCompiler.Compile(src, "/tmp/model_test.bsp");
        Assert.Contains("DIM MODEL AS Utente", basic);
        Assert.Contains("PRINT STRINGS.HTMLENCODE(MODEL.Nome);", basic);
        // "@Model" nudo NON deve essere un marker (solo "Model.<Campo>") → niente PRINT MODEL.*
        var src2 = "prenota @Model";
        var basic2 = CommonLib.Http.BspCompiler.Compile(src2, "/tmp/model2.bsp");
        Assert.DoesNotContain("PRINT MODEL", basic2);
    }

    [Fact]
    public void Mvc_Samples_And_BspPages_Compile()
    {
        // Fase D: i sample MVC (Fasi A/C/D) compilano e le pagine BSP dell'advanced demo
        // (layout/section/partial) precompilano — regressione in CI.
        var root = FindRoot();
        foreach (var name in new[] { "MvcBasicDemo.bas", "MvcFormDemo.bas", "MvcAdvancedDemo.bas", "MvcApiDemo.bas" })
        {
            var f = Path.Combine(root, "shared", "samples", "Web", "Mvc", name);
            Assert.True(File.Exists(f), $"{name} mancante");
            var program = Basic.Orchestrator.Compile(f, null, false);
            Assert.NotNull(program);
            Assert.NotNull(program.MainModule);
        }
        // A4 (azioni senza req/res, iniezione contesto): demo isolata con il framework accanto
        var a4 = Path.Combine(root, "shared", "samples", "Web", "Mvc", "A4InjectionDemo");
        Assert.True(File.Exists(Path.Combine(a4, "HttpController.inc")), "HttpController.inc (A4) mancante");
        var a4Program = Basic.Orchestrator.Compile(Path.Combine(a4, "MvcA4Demo.bas"), null, false);
        Assert.NotNull(a4Program);
        Assert.NotNull(a4Program.MainModule);

        // Esempi MVC aggiuntivi in samples/Http/Mvc (auth + CRUD persistente)
        foreach (var rel in new[] { "AuthDemo", "CrudDemo" })
        {
            var dir = Path.Combine(root, "shared", "samples", "Http", "Mvc", rel);
            Assert.True(File.Exists(Path.Combine(dir, "HttpController.inc")), $"HttpController.inc ({rel}) mancante");
            var name = rel == "AuthDemo" ? "MvcAuth.bas" : "MvcCrud.bas";
            var prog = Basic.Orchestrator.Compile(Path.Combine(dir, name), null, false);
            Assert.NotNull(prog);
            Assert.NotNull(prog.MainModule);
        }
        foreach (var page in new[] { "_Layout.bsp", "_Header.bsp", "form.bsp" })
        {
            var p = Path.Combine(root, "shared", "samples", "Web", "Mvc", "pages", page);
            Assert.True(File.Exists(p), $"{page} mancante");
            Assert.True(global::Basic.Pipeline.SourcePipeline.CompileBspPage(p, out _),
                $"{page} non precompila");
        }
    }

    [Fact]
    public void Class_Delete_Is_Action_And_Destructor_Is_Destructor()
    {
        // Il distruttore si chiama SUB DESTRUCTOR (senza parentesi; ex SUB DELETE): SUB Delete(...)
        // con parametri è ora un METODO normale (azione REST DELETE), non un destructor.
        var tmp = Path.Combine(Path.GetTempPath(), "gmb_delete_" + Guid.NewGuid().ToString("N")[..6] + ".bas");
        try
        {
            File.WriteAllText(tmp, """
                PUBLIC CLASS Ctl
                    PUBLIC SUB Delete(a AS LONG, b AS LONG)
                        PRINT a + b
                    END SUB
                    PUBLIC SUB DESTRUCTOR
                        PRINT "bye"
                    END SUB
                END CLASS
                PRINT "ok"
                """);
            var program = Basic.Orchestrator.Compile(tmp, null, false);
            Assert.NotNull(program);
            Assert.NotNull(program.MainModule);
        }
        finally
        {
            try { File.Delete(tmp); } catch { }
        }
    }

    [Fact]
    public void Classes_NewFeatures_Samples_Compile()
    {
        // Nuove funzionalità classi (distruttore SUB DESTRUCTOR, Z1-Z3): i sample dedicati
        // compilano. (L'esecuzione e i limiti PCodeVM sono documentati in
        // Bugs/Classi-Distruttore-Ereditarieta-CrossFile-Bugs.md)
        var root = FindRoot();
        var cases = new[]
        {
            Path.Combine("shared", "samples", "Classes", "ClassesDestructor.bas"),
            Path.Combine("shared", "samples", "Classes", "ClassesUpcastReturn.bas"),
            Path.Combine("shared", "samples", "Classes", "ClassesInterfacesNamespace", "ClassesInterfacesNamespace.bas"),
            Path.Combine("shared", "samples", "Classes", "ClassesInheritsCrossFile", "MezziDemo.bas"),
        };
        foreach (var rel in cases)
        {
            var f = Path.Combine(root, rel);
            Assert.True(File.Exists(f), $"{rel} mancante");
            var program = Basic.Orchestrator.Compile(f, null, false);
            Assert.NotNull(program);
            Assert.NotNull(program.MainModule);
        }
    }

    [Fact]
    public void HttpRequest_ParseForm_Decodes_FormUrlEncoded()
    {
        // Fase C1: req.ParseForm() (CommonLib.Http) decodifica application/x-www-form-urlencoded —
        // alimenta il seeding FORM del backend (prima era sempre un dict vuoto).
        var req = new CommonLib.Http.HttpRequest
        {
            Headers = { ["Content-Type"] = "application/x-www-form-urlencoded" },
            Body = "name=Mario&email=mario%40x.it&age=30",
        };
        var form = req.ParseForm();
        Assert.NotNull(form);
        Assert.Equal("Mario", form!["name"]?.ToString());
        Assert.Equal("mario@x.it", form["email"]?.ToString());
        Assert.Equal("30", form["age"]?.ToString());
    }

    [Fact]
    public void Bsp_Page_With_Form_And_Query_Binding_Compiles()
    {
        // Fase C1: una pagina BSP che legge FORM["..."]/QUERY["..."] (dynamic scope keyword)
        // compila — è il contratto che il fix del seeding FORM (BspDispatch + TryServeBspPage)
        // deve alimentare a runtime (verificato e2e manualmente via curl, vedi commento testa).
        var dir = Path.Combine(Path.GetTempPath(), "gmb_bsp_form_" + Guid.NewGuid().ToString("N")[..6]);
        Directory.CreateDirectory(dir);
        try
        {
            var pagePath = Path.Combine(dir, "form.bsp");
            File.WriteAllText(pagePath,
                "<html><body>\n" +
                "<p>name: <% PRINT FORM[\"name\"]; %></p>\n" +
                "<p>q: <% PRINT QUERY[\"q\"]; %></p>\n" +
                "</body></html>\n");
            Assert.True(global::Basic.Pipeline.SourcePipeline.CompileBspPage(pagePath, out _),
                "form.bsp con FORM[\"name\"]/QUERY[\"q\"] non compila");
        }
        finally
        {
            try { Directory.Delete(dir, true); } catch { }
        }
    }
}
