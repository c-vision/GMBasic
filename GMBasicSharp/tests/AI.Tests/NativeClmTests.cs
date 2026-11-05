using System.Net;
using System.Text;
using Basic.Decision;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Provider `clm` via P/Invoke su libgmb_clm (runtime C condiviso): il C# consuma la fonte C.
/// Server HTTP locale che risponde con la forma REALE di clm-serve ({"model","answers":{qid:…},"usage"}).
/// Richiede libgmb_clm raggiungibile (DYLD_LIBRARY_PATH o cartella libs): se manca il test FALLISCE.
/// I golden di prosa/richiesta/risposta sono nel selfcheck C (gmb_decision_clm_selfcheck).</summary>
[Trait("Category", "Native")]
public sealed class NativeClmTests
{
    private static (HttpListener l, string url, Task<string> body) Serve(string response, int status = 200)
    {
        var l = new HttpListener();
        var port = Random.Shared.Next(20000, 40000);
        var url = $"http://127.0.0.1:{port}/";
        l.Prefixes.Add(url);
        l.Start();
        var body = Task.Run(async () =>
        {
            var ctx = await l.GetContextAsync();
            using var rd = new StreamReader(ctx.Request.InputStream, Encoding.UTF8);
            var req = await rd.ReadToEndAsync() + "|" + ctx.Request.RawUrl + "|" + ctx.Request.Headers["Authorization"];
            var bytes = Encoding.UTF8.GetBytes(response);
            ctx.Response.StatusCode = status;
            ctx.Response.ContentType = "application/json";
            await ctx.Response.OutputStream.WriteAsync(bytes);
            ctx.Response.Close();
            return req;
        });
        return (l, url.TrimEnd('/'), body);
    }

    private static Dictionary<string, string> Cfg(string url, string key = "k123") =>
        new() { ["baseUrl"] = url, ["apiKey"] = key };

    [Fact]
    public async Task Choice_FormaRealeDelServer_RisultatoERichiestaCorretti()
    {
        var (l, url, body) = Serve("{\"model\":\"clm-latest\",\"answers\":{\"q\":{\"type\":\"choice\",\"choice\":\"A\",\"confidence\":0.5,\"probabilities\":{\"A\":0.75,\"B\":0.25}}},\"usage\":{\"billing_units\":1}}");
        try
        {
            var r = NativeClm.Run("{\"ticket\":\"Login fallito\"}", "CHOICE:auth|billing", Cfg(url));
            r.Should().Be("{\"kind\":\"CHOICE\",\"option\":\"A\",\"value\":\"auth\",\"probability\":0.750000,\"confidence\":0.500000,\"calibrated\":null}");
            var req = await body;
            req.Should().Be("{\"state\":{\"ticket\":\"Login fallito\"},\"questions\":{\"q\":{\"type\":\"choice\",\"instructions\":\"\",\"criteria\":{\"A\":\"auth\",\"B\":\"billing\"}}}}|/v1/systemone|Bearer k123");
        }
        finally { l.Stop(); }
    }

    [Fact]
    public async Task Noul_ProbabilitaSopraSoglia_EVero()
    {
        var (l, url, _) = Serve("{\"answers\":{\"q\":{\"type\":\"noul\",\"noul\":0.83}}}");
        try
        {
            NativeClm.Run("urgente?", "NOUL", Cfg(url)).Should().Be(
                "{\"kind\":\"NOUL\",\"option\":\"true\",\"value\":\"true\",\"probability\":0.830000,\"confidence\":0.660000,\"calibrated\":null}");
            await Task.CompletedTask;
        }
        finally { l.Stop(); }
    }

    [Fact]
    public void Http401_ErroreEsplicito()
    {
        var (l, url, _) = Serve("{\"detail\":\"unauthorized\"}", 401);
        try { NativeClm.Run("x", "NOUL", Cfg(url)).Should().Be("{\"error\":\"clm: HTTP 401\"}"); }
        finally { l.Stop(); }
    }

    [Fact]
    public void TipoIncoerente_ErroreEsplicito()
    {
        var (l, url, _) = Serve("{\"answers\":{\"q\":{\"type\":\"noul\",\"noul\":0.5}}}");
        try { NativeClm.Run("x", "CHOICE:a|b", Cfg(url)).Should().Contain("incoerente"); }
        finally { l.Stop(); }
    }

    [Fact]
    public void ConfigSenzaBaseUrl_EHead_ErroriEspliciti()
    {
        NativeClm.Run("x", "NOUL", new Dictionary<string, string>()).Should().Be("{\"error\":\"clm: baseUrl mancante nel config del provider %DECIDE\"}");
        NativeClm.Run("x", "NOUL", new Dictionary<string, string> { ["apiKey"] = "k" }).Should().Be("{\"error\":\"clm: baseUrl mancante nel config del provider %DECIDE\"}");
        NativeClm.Run("x", "NOUL", new Dictionary<string, string> { ["head"] = "/x/h.safetensors" }).Should().Contain("config locale incompleta");
    }
}
