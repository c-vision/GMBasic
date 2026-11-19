// Security (2026-09-02): compila ED ESEGUE i nuovi sample BASIC samples/Security/*
// sul backend IL e verifica i marker d'esito in output (test a fondo delle nuove funzionalità).
using System.Reflection;
using System.Net.Http;
using System.Text;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SecuritySamplesExecutionTests
{
    private static readonly string Root = FindRepoRoot();

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private static string RunSample(string sample)
    {
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "Sec_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var assembly = Assembly.Load(ms.ToArray());
        var main = assembly.GetType("GeneratedProgram")?.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            if (!task.Wait(TimeSpan.FromSeconds(20)))
                throw new InvalidOperationException("TIMEOUT. Output:\n" + sw);
            task.GetAwaiter().GetResult();
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString();
    }

    [Fact]
    public void HmacAndBase64Url_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "HmacAndBase64Url.bas"));
        Assert.Contains("HMAC256 VECTOR OK", output);
        Assert.Contains("B64URL ROUNDTRIP OK", output);
        Assert.Contains("HMAC-SHA384", output);
        Assert.Contains("HMAC-SHA512", output);
    }

    [Fact]
    public void EncodingDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "EncodingDemo.bas"));
        Assert.Contains("HTML ENCODE OK", output);
        Assert.Contains("URL ENCODE OK", output);
        Assert.Contains("a\\\"b", output);   // JSENCODE("a"b) -> a\"b
        Assert.Contains("it\\'s ok", output);
    }

    [Fact]
    public void JwtDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "JwtDemo.bas"));
        Assert.Contains("VERIFY OK", output);
        Assert.Contains("TAMPER DETECTED", output);
        Assert.Contains("PAYLOAD MATCH", output);
    }

    [Fact]
    public void SecureTokens_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "SecureTokens.bas"));
        Assert.Contains("PWHASH OK", output);
        Assert.Contains("PWV NEG OK", output);
        Assert.Contains("SIG:", output);
    }

    [Fact]
    public void HttpConfigSecurity_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "HttpConfigSecurity.bas"));
        Assert.Contains("SEC_HEADERS OK", output);
        Assert.Contains("ALLOW OK", output);
        Assert.Contains("BLOCK OK", output);
        Assert.Contains("demo done", output);
    }

    [Fact]
    public void XssSafeOutput_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "XssSafeOutput.bas"));
        Assert.Contains("XSS SAFE OK", output);
        Assert.Contains("URL SAFE OK", output);
        Assert.Contains("demo done", output);
    }

    [Fact]
    public void AuthFlowDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "AuthFlowDemo.bas"));
        Assert.Contains("AUTH OK (admin)", output);
        Assert.Contains("AUTH OK (user)", output);
        Assert.Contains("WRONG PASSWORD", output);
        Assert.Contains("UNKNOWN USER", output);
    }

    [Fact]
    public void HttpSessionsDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "HttpSessionsDemo.bas"));
        Assert.Contains("SESSIONS OK", output);
        Assert.Contains("demo done", output);
    }

    [Fact]
    public void HttpSessionsFileDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "HttpSessionsFileDemo.bas"));
        Assert.Contains("SESSIONFILE OK", output);
        Assert.Contains("demo done", output);
    }

    [Fact]
    public void SessionsChooseBackend_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "SessionsChooseBackend.bas"));
        Assert.Contains("BACKEND FILE OK", output);
        Assert.Contains("demo done", output);
    }

    [Fact]
    public void BspEscapingPage_Compiles_With_AutoEscape_And_Raw()
    {
        var path = Path.Combine(Root, "shared", "samples", "Security", "pages", "EscapingDemo.bsp");
        var code = CommonLib.Http.BspCompiler.Compile(File.ReadAllText(path), path);
        Assert.Contains("IMPORT STRINGS", code);
        Assert.Contains("HTMLENCODE(userInput$)", code);   // <%= %> escaped di default
        Assert.Contains("PRINT trustedHtml$;", code);      // | raw
        Assert.DoesNotContain("HTMLENCODE(trustedHtml$", code);
    }

    [Fact]
    public void UploadValidationDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "UploadValidationDemo.bas"));
        Assert.Contains("SAFE NAME OK", output);
        Assert.Contains("demo done", output);
    }

    [Fact]
    public void ServerAuthDemo_Compiles()
    {
        var sample = Path.Combine(Root, "shared", "samples", "Security", "ServerAuthDemo.bas");
        Assert.True(File.Exists(sample));
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program);
        Assert.NotNull(program.MainModule);
    }

    [Fact]
    public void AdminBsp_AUTHORIZE_Compiles_With_SessionGuards()
    {
        var path = Path.Combine(Root, "shared", "samples", "Security", "pages", "admin.bsp");
        var code = CommonLib.Http.BspCompiler.Compile(File.ReadAllText(path), path);
        Assert.Contains("401", code);
        Assert.Contains("403", code);
        Assert.Contains("SESSION(\"user\")", code);
    }

    [Fact]
    public void HeadersProtectionDemo_Compiles()
    {
        var sample = Path.Combine(Root, "shared", "samples", "Security", "HeadersProtectionDemo.bas");
        Assert.True(File.Exists(sample));
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program);
        Assert.NotNull(program.MainModule);
    }

    [Fact]
    public void ValidationDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "ValidationDemo.bas"));
        Assert.Contains("REQ OK", output);
        Assert.Contains("EMAIL OK", output);
        Assert.Contains("URL OK", output);
        Assert.Contains("CC OK", output);
        Assert.Contains("LEN OK", output);
        Assert.Contains("ValidationDemo done", output);
    }

    [Fact]
    public void ValidationFluentDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "ValidationFluentDemo.bas"));
        Assert.Contains("INVALID OK", output);
        Assert.Contains("VALID OK", output);
        Assert.Contains("done", output);
    }

    [Fact]
    public void OAuthMockIdpServer_Subprocess_E2E()
    {
        var source = Path.Combine(Root, "shared", "samples", "Security", "OAuthMockIdpServer.bas");
        var program = Basic.Orchestrator.Compile(source, null, false);
        var sampleDir = Path.Combine(Path.GetTempPath(), "gm_oauthidp_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sampleDir);
        var exePath = IlProgramCompiler.Compile(program!, source, sampleDir, "OAuthMockIdp", rid: "osx-x64", linkRuntime: true);

        using var proc = new System.Diagnostics.Process
        {
            StartInfo = new System.Diagnostics.ProcessStartInfo(exePath)
            {
                WorkingDirectory = sampleDir,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
            }
        };
        proc.Start();

        try
        {
            using var http = new HttpClient();
            var deadline = DateTime.UtcNow.AddSeconds(20);
            var up = false;
            while (DateTime.UtcNow < deadline)
            {
                try
                {
                    using var probe = new System.Net.Sockets.TcpClient();
                    try { probe.Connect("127.0.0.1", 8097); }
                    catch { probe.Connect(System.Net.IPAddress.IPv6Loopback, 8097); }
                    up = true;
                    break;
                }
                catch { Thread.Sleep(200); }
            }
            Assert.True(up, "server mock OAuth non raggiungibile (subprocess)");

            var authz = http.GetStringAsync("http://127.0.0.1:8097/mock/authorize").GetAwaiter().GetResult();
            Assert.Contains("\"mock\":\"authorize\"", authz);

            var okBody = "grant_type=authorization_code&code=demo-auth-code&redirect_uri=http://localhost/cb";
            var okResp = http.PostAsync("http://127.0.0.1:8097/mock/token",
                new StringContent(okBody, Encoding.UTF8, "application/x-www-form-urlencoded")).GetAwaiter().GetResult();
            var okJson = okResp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            Assert.True(okResp.IsSuccessStatusCode, okJson);
            Assert.Contains("mock-token-abc", okJson);

            var badBody = "grant_type=authorization_code&code=wrong";
            var badResp = http.PostAsync("http://127.0.0.1:8097/mock/token",
                new StringContent(badBody, Encoding.UTF8, "application/x-www-form-urlencoded")).GetAwaiter().GetResult();
            var badJson = badResp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            Assert.Equal(400, (int)badResp.StatusCode);
            Assert.Contains("invalid_grant", badJson);
        }
        finally
        {
            try { if (!proc.HasExited) proc.Kill(); } catch { }
        }
    }

    [Fact]
    public void OAuthDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "OAuthDemo.bas"));
        Assert.Contains("AUTHURL OK", output);
        Assert.Contains("PKCE OK", output);
        Assert.Contains("CC OK", output);
        Assert.Contains("VERIFIER OK", output);
        Assert.Contains("OAuthDemo done", output);
    }

    [Fact]
    public void OAuthPKCEFlowDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "OAuthPKCEFlowDemo.bas"));
        Assert.Contains("AUTH URL OK", output);
        Assert.Contains("STATE OK", output);
        Assert.Contains("CALLBACK TOKEN BODY OK", output);
        Assert.Contains("done", output);
    }

    [Fact]
    public void OAuthClientCredentialsDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "OAuthClientCredentialsDemo.bas"));
        Assert.Contains("ENDPOINT OK", output);
        Assert.Contains("CC BODY OK", output);
        Assert.Contains("done", output);
    }

    [Fact]
    public void OAuthBspPages_Compile()
    {
        foreach (var name in new[] { "oauth-login.bsp", "oauth-callback.bsp" })
        {
            var path = Path.Combine(Root, "shared", "samples", "Security", "pages", name);
            var code = CommonLib.Http.BspCompiler.Compile(File.ReadAllText(path), path);
            Assert.Contains("IMPORT", code);
            Assert.Contains("OAUTH", code);
        }
    }

    [Fact]
    public void AuthMvcDemo_Compiles()
    {
        var sample = Path.Combine(Root, "shared", "samples", "Security", "AuthMvcDemo.bas");
        Assert.True(File.Exists(sample));
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program);
        Assert.NotNull(program.MainModule);
    }

    [Fact]
    public void RbacUnitDemo_Sample_Runs_And_Verifies()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Security", "RbacUnitDemo.bas"));
        Assert.Contains("ADMIN OK", output);
        Assert.Contains("ROLE DENY OK", output);
        Assert.Contains("NOTAUTH OK", output);
        Assert.Contains("AUTH ONLY OK", output);
        Assert.Contains("done", output);
    }
}
