// Limite profondità JSON (P1 residuo, 2026-09-02): il parser JSON usa System.Text.Json con
// MaxDepth di default 64 — nesting più profondo fallisce (anti-DoS), non "va in ricorsione".
using System.Text.Json;
using System.Text.Json.Nodes;
using Xunit;

namespace Basic.IlBackend.Tests;

public class JsonDepthLimitTests
{
    private static string Nest(int depth)
    {
        var open = new string('[', depth);
        var close = new string(']', depth);
        return open + "1" + close;
    }

    [Fact]
    public void DeepJson_Beyond_Engine_MaxDepth_Throws()
    {
        var deep = Nest(70); // oltre il default MaxDepth=64
        Assert.ThrowsAny<JsonException>(() => JsonNode.Parse(deep));
    }

    [Fact]
    public void ModerateJson_Within_MaxDepth_Parses()
    {
        var ok = Nest(32);
        var node = JsonNode.Parse(ok);
        Assert.NotNull(node);
    }
}
