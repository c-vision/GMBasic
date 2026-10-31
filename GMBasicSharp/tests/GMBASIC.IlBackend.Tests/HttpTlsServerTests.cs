// HTTPS su IL (2026-09-02): HttpServerStartTls (TcpListener+SslStream) e2e con certificato
// self-signed: route /hello servita in TLS, header di sicurezza presenti se configurati.
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using Xunit;

namespace Basic.IlBackend.Tests;

public class HttpTlsServerTests
{
    [Fact]
    public async Task TlsServer_Serves_Route_And_SecurityHeaders()
    {
        var root = FindRepoRoot();
        _ = root;
        var certPath = Path.Combine(Path.GetTempPath(), "gm_tls_" + Guid.NewGuid().ToString("N") + ".pfx");
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
            Assert.True(handle >= 0, "HttpServerStartTls fallito");
            service.HttpConfigure(handle, new Basic.Runtime.HttpServerSettings { SecurityHeaders = true });
            service.HttpAddRoute(handle, "GET", "/hello",
                _ => new Basic.Runtime.HttpResponse { StatusCode = 200, Body = "hello-tls", ContentType = "text/plain" });

            string response = await TlsGetAsync(port, "/hello");
            Assert.Contains("200", response);
            Assert.Contains("hello-tls", response);
            Assert.Contains("Strict-Transport-Security", response);
            Assert.Contains("X-Content-Type-Options", response);

            service.HttpServerStop(handle);
        }
        finally
        {
            try { File.Delete(certPath); } catch { }
        }
    }

    private static async Task<string> TlsGetAsync(int port, string path)
    {
        using var client = new TcpClient();
        await client.ConnectAsync(IPAddress.Loopback, port);
        using var stream = client.GetStream();
        using var ssl = new SslStream(stream, false, (_, _, _, _) => true);
        await ssl.AuthenticateAsClientAsync("localhost");
        var request = $"GET {path} HTTP/1.1\r\nHost: localhost:{port}\r\nConnection: close\r\n\r\n";
        var bytes = System.Text.Encoding.ASCII.GetBytes(request);
        await ssl.WriteAsync(bytes);
        await ssl.FlushAsync();
        var buf = new byte[65536];
        int n;
        var sb = new System.Text.StringBuilder();
        do { n = await ssl.ReadAsync(buf); sb.Append(System.Text.Encoding.UTF8.GetString(buf, 0, n)); }
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

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }
}
