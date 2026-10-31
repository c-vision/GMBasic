// Sessioni route HTTP e2e (2026-09-02): HTTP CONFIG .Sessions su server IL (path TLS, collaudato).
// Prima richiesta a /set -> Set-Cookie GMBSESSION + sessione nuova; seconda a /read con lo stesso
// cookie -> legge il valore scritto nella prima (persistenza server-side, JsonObject vivo).
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json.Nodes;
using Xunit;

namespace Basic.IlBackend.Tests;

public class HttpSessionsServerTests
{
    [Fact]
    public async Task Session_Survives_Across_Two_Requests_With_Cookie()
    {
        var certPath = Path.Combine(Path.GetTempPath(), "gm_sess_" + Guid.NewGuid().ToString("N") + ".pfx");
        try
        {
            using (var rsa = RSA.Create(2048))
            {
                var req = new CertificateRequest("CN=localhost", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
                using var cert = req.CreateSelfSigned(DateTimeOffset.Now.AddDays(-1), DateTimeOffset.Now.AddYears(1));
                File.WriteAllBytes(certPath, cert.Export(X509ContentType.Pfx, "pw"));
            }

            var port = FreePort();
            using var service = new Basic.Runtime.HttpService();
            var handle = service.HttpServerStartTls(port, certPath, "pw");
            Assert.True(handle >= 0);
            service.HttpConfigure(handle, new Basic.Runtime.HttpServerSettings { Sessions = true });

            service.HttpAddRoute(handle, "GET", "/set", req =>
            {
                if (req.SessionData is JsonObject session) session["user"] = "alice";
                return new Basic.Runtime.HttpResponse { Body = "set-ok" };
            });
            service.HttpAddRoute(handle, "GET", "/read", req =>
            {
                var user = (req.SessionData as JsonObject)?["user"]?.ToString() ?? "";
                return new Basic.Runtime.HttpResponse { Body = "user=" + user };
            });

            try
            {
                var first = await TlsGetAsync(port, "/set", null);
                var setCookieLine = first.Split('\n').FirstOrDefault(l => l.StartsWith("Set-Cookie:", StringComparison.OrdinalIgnoreCase));
                Assert.True(setCookieLine != null, "Nessun Set-Cookie. Risposta:\n" + first);
                Assert.Contains("GMBSESSION=", setCookieLine);
                Assert.Contains("set-ok", first);

                var cookie = setCookieLine.Split(':', 2)[1].Trim().Split(';')[0];

                var second = await TlsGetAsync(port, "/read", "Cookie: " + cookie);
                Assert.Contains("user=alice", second);

                var noCookie = await TlsGetAsync(port, "/read", null);
                Assert.DoesNotContain("user=alice", noCookie);
            }
            finally
            {
                service.HttpServerStop(handle);
            }
        }
        finally
        {
            try { File.Delete(certPath); } catch { }
        }
    }

    [Fact]
    public async Task Authorize_Flow_Login_Then_Protected_With_Role()
    {
        var certPath = Path.Combine(Path.GetTempPath(), "gm_auth_" + Guid.NewGuid().ToString("N") + ".pfx");
        try
        {
            using (var rsa = RSA.Create(2048))
            {
                var req = new CertificateRequest("CN=localhost", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
                using var cert = req.CreateSelfSigned(DateTimeOffset.Now.AddDays(-1), DateTimeOffset.Now.AddYears(1));
                File.WriteAllBytes(certPath, cert.Export(X509ContentType.Pfx, "pw"));
            }

            var port = FreePort();
            using var service = new Basic.Runtime.HttpService();
            var handle = service.HttpServerStartTls(port, certPath, "pw");
            Assert.True(handle >= 0);
            service.HttpConfigure(handle, new Basic.Runtime.HttpServerSettings { Sessions = true });

            // /login: il server crea la sessione admin e restituisce il cookie
            service.HttpAddRoute(handle, "GET", "/login", _ =>
            {
                var cookie = Basic.IlBackend.IlRuntime.HttpSessionCreate("alice", "admin");
                return new Basic.Runtime.HttpResponse
                {
                    Body = "login-ok",
                    Headers = new Dictionary<string, string> { ["Set-Cookie"] = cookie }
                };
            });
            // /admin: emula i guard di <%@ AUTHORIZE admin %>
            service.HttpAddRoute(handle, "GET", "/admin", req =>
            {
                var role = (req.SessionData as JsonObject)?["role"]?.ToString() ?? "";
                if (role == "admin")
                    return new Basic.Runtime.HttpResponse { Body = "admin-ok" };
                return new Basic.Runtime.HttpResponse { StatusCode = 403, Body = "403 Forbidden" };
            });

            try
            {
                var login = await TlsGetAsync(port, "/login", null);
                Assert.Contains("login-ok", login);
                var setCookieLine = login.Split('\n').FirstOrDefault(l => l.StartsWith("Set-Cookie:", StringComparison.OrdinalIgnoreCase));
                Assert.True(setCookieLine != null, "Nessun Set-Cookie dal login:\n" + login);
                var cookie = setCookieLine.Split(':', 2)[1].Trim().Split(';')[0];

                var denied = await TlsGetAsync(port, "/admin", null);   // nessun cookie (o sessione vuota)
                Assert.Contains("403", denied);

                var ok = await TlsGetAsync(port, "/admin", "Cookie: " + cookie);
                Assert.Contains("admin-ok", ok);
            }
            finally
            {
                service.HttpServerStop(handle);
            }
        }
        finally
        {
            try { File.Delete(certPath); } catch { }
        }
    }

    private static async Task<string> TlsGetAsync(int port, string path, string? extraHeader)    {
        using var client = new TcpClient();
        await client.ConnectAsync(IPAddress.Loopback, port);
        using var stream = client.GetStream();
        using var ssl = new SslStream(stream, false, (_, _, _, _) => true);
        await ssl.AuthenticateAsClientAsync("localhost");
        var request = $"GET {path} HTTP/1.1\r\nHost: localhost:{port}\r\nConnection: close\r\n" +
                      (extraHeader != null ? extraHeader + "\r\n" : "") + "\r\n";
        var bytes = Encoding.ASCII.GetBytes(request);
        await ssl.WriteAsync(bytes);
        await ssl.FlushAsync();
        var buf = new byte[65536];
        int n;
        var sb = new StringBuilder();
        do { n = await ssl.ReadAsync(buf); sb.Append(Encoding.UTF8.GetString(buf, 0, n)); }
        while (n > 0);
        return sb.ToString();
    }

    private static int FreePort()
    {
        var l = new TcpListener(IPAddress.Loopback, 0);
        l.Start();
        int p = ((IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return p;
    }
}
