// Security (2026-09-02): CRYPTOGRAPHY.HMACSHA256/384/512 + TOBASE64URL/FROMBASE64URL.
// Vettori: RFC 4231 (HMAC "key"/"The quick brown fox...") e RFC 4648 §5 (base64url).
using System.Reflection;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class CryptographyHmacBase64UrlTests
{
    private static readonly string Root = FindRepoRoot();

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

    private static string RunSource(string source)
    {
        var tmp = Path.Combine(Path.GetTempPath(), "gm_crypto_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(tmp, source);
        try
        {
            var program = Basic.Orchestrator.Compile(tmp, null, false);
            var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "Crypto_" + Guid.NewGuid().ToString("N"), out _);
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
        finally
        {
            try { File.Delete(tmp); } catch { /* best effort */ }
        }
    }

    [Fact]
    public void HmacSha256_Matches_Rfc4231_Vector()
    {
        var output = RunSource(
            "IMPORT CRYPTOGRAPHY\n" +
            "PRINT \"A=\"; CRYPTOGRAPHY.TOBASE64(\"hello\");\n" +
            "PRINT \"B=\"; CRYPTOGRAPHY.HMACSHA256(\"The quick brown fox jumps over the lazy dog\", \"key\");");
        Assert.Contains("aGVsbG8", output);
        Assert.Contains("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8", output);
    }

    [Fact]
    public void HmacSha384_Matches_Rfc4231_Vector()
    {
        var output = RunSource(
            "IMPORT CRYPTOGRAPHY\n" +
            "PRINT CRYPTOGRAPHY.HMACSHA384(\"The quick brown fox jumps over the lazy dog\", \"key\");");
        Assert.Contains("d7f4727e2c0b39ae0f1e40cc96f60242d5b7801841cea6fc592c5d3e1ae50700582a96cf35e1e554995fe4e03381c237", output);
    }

    [Fact]
    public void HmacSha512_Matches_Rfc4231_Vector()
    {
        var output = RunSource(
            "IMPORT CRYPTOGRAPHY\n" +
            "PRINT CRYPTOGRAPHY.HMACSHA512(\"The quick brown fox jumps over the lazy dog\", \"key\");");
        Assert.Contains("b42af09057bac1e2d41708e48a902e09b5ff7f12ab428a4fe86653c73dd248fb82f948a549f7b791a5b41915ee4d1ec3935357e4e2317250d0372afa2ebeeb3a", output);
    }

    [Fact]
    public void ToBase64Url_NoPadding_Rfc4648_Vectors()
    {
        var output = RunSource(
            "IMPORT CRYPTOGRAPHY\n" +
            "PRINT CRYPTOGRAPHY.TOBASE64URL(\"hello\");" + "\n" +
            "PRINT \"|\"; CRYPTOGRAPHY.TOBASE64URL(\"f\");");
        Assert.Contains("aGVsbG8", output);
        Assert.Contains("|Zg", output);
    }

    [Fact]
    public void FromBase64Url_RoundTrips_ToHex()
    {
        var output = RunSource(
            "IMPORT CRYPTOGRAPHY\n" +
            "PRINT CRYPTOGRAPHY.TOHEX(CRYPTOGRAPHY.FROMBASE64URL(\"aGVsbG8\"));");
        Assert.Contains("68656c6c6f", output);
    }

    [Fact]
    public void Strings_HtmlEncode_Escapes_Forbidden()
    {
        var output = RunSource(
            "IMPORT STRINGS\n" +
            "PRINT STRINGS.HTMLENCODE(\"<a href=\"\"x\"\">Tom & Jerry 'ok'</a>\");");
        Assert.Contains("&lt;a href=&quot;x&quot;&gt;Tom &amp; Jerry &#39;ok&#39;&lt;/a&gt;", output);
    }

    [Fact]
    public void Strings_HtmlEncode_Passes_NonAscii()
    {
        var output = RunSource(
            "IMPORT STRINGS\n" +
            "PRINT STRINGS.HTMLENCODE(\"caf\u00e9\");");
        Assert.Contains("caf\u00e9", output);
    }

    [Fact]
    public void Strings_UrlEncode_PercentEncodes()
    {
        var output = RunSource(
            "IMPORT STRINGS\n" +
            "PRINT STRINGS.URLENCODE(\"a b&c=d/x?y\");");
        Assert.Contains("a%20b%26c%3Dd%2Fx%3Fy", output);
    }

    [Fact]
    public void Strings_JsEncode_Escapes_Quote_Backslash_Newline()
    {
        var output = RunSource(
            "IMPORT STRINGS\n" +
            "PRINT STRINGS.JSENCODE(\"a\" + CHR$(34) + \"b\");\n" +
            "PRINT \"|\"; STRINGS.JSENCODE(\"x\" + CHR$(10) + \"y\");\n" +
            "PRINT \"|\"; STRINGS.JSENCODE(\"it's ok\");");
        Assert.Contains("a\\\"b", output);     // a"b → a\"b
        Assert.Contains("|x\\ny", output);     // LF → \n
        Assert.Contains("|it\\'s ok", output); // ' → \'
    }

    [Fact]
    public void Auth_Jwt_Roundtrip_Verify_Tamper_Payload()
    {
        var output = RunSource(
            "IMPORT AUTH\n" +
            "DIM claims$ = \"{\" + CHR$(34) + \"sub\" + CHR$(34) + \":\" + CHR$(34) + \"123\" + CHR$(34) + \"}\"\n" +
            "DIM t$ = AUTH.JWTCREATE(claims$, \"s3cret\")\n" +
            "IF AUTH.JWTVERIFY(t$, \"s3cret\") THEN PRINT \"OK1\"; ELSE PRINT \"NO1\";\n" +
            "IF AUTH.JWTVERIFY(t$, \"wrong\") THEN PRINT \"BAD\"; ELSE PRINT \"OK2\";\n" +
            "PRINT \"P=\"; AUTH.JWTPAYLOAD(t$);");
        Assert.Contains("OK1", output);
        Assert.Contains("OK2", output);
        Assert.Contains("P=" + "{\"sub\":\"123\"}", output);
    }
}
