// Anti-XSS (2026-09-02): BspCompiler auto-escaping di <%= %>/@Model con escape-hatch | raw,
// @Html.Encode/@Html.Raw, IMPORT STRINGS nel preambolo. Test a livello di codice generato.
using Xunit;
using CommonLib.Http;

namespace Basic.IlBackend.Tests;

public class BspAntiXssCompilationTests
{
    private const string Marker = "BspErrorHandler";

    [Fact]
    public void Preamble_Imports_Strings()
    {
        var code = BspCompiler.Compile("<p>ok</p>", "page.bsp");
        Assert.Contains("IMPORT STRINGS", code);
        Assert.Contains("IMPORT HTTP", code);
    }

    [Fact]
    public void ExpressionOutput_Is_Escaped_ByDefault()
    {
        var code = BspCompiler.Compile("<p><%= userInput$ %></p>", "page.bsp");
        Assert.Contains("PRINT STRINGS.HTMLENCODE(userInput$);", code);
    }

    [Fact]
    public void ExpressionOutput_Raw_EscapeHatch()
    {
        var code = BspCompiler.Compile("<p><%= trustedHtml$ | raw %></p>", "page.bsp");
        Assert.Contains("PRINT trustedHtml$;", code);
        Assert.DoesNotContain("HTMLENCODE(trustedHtml$)", code);
    }

    [Fact]
    public void ModelOutput_Is_Escaped_ByDefault()
    {
        var code = BspCompiler.Compile("<%@ MODEL Pagina %>\n<h1>@Model.Titolo</h1>", "page.bsp");
        Assert.Contains("PRINT STRINGS.HTMLENCODE(MODEL.Titolo);", code);
    }

    [Fact]
    public void HtmlEncode_And_HtmlRaw_Helpers()
    {
        var enc = BspCompiler.Compile("<p>@Html.Encode(Model.Descrizione)</p>", "page.bsp");
        Assert.Contains("PRINT STRINGS.HTMLENCODE(Model.Descrizione);", enc);

        var raw = BspCompiler.Compile("<p>@Html.Raw(\"<b>ok</b>\")</p>", "page.bsp");
        Assert.Contains("PRINT \"<b>ok</b>\";", raw);
    }

    [Fact]
    public void RenderSection_Stays_Raw()
    {
        var code = BspCompiler.Compile("@section S { <em>statico</em> }\n@RenderSection(\"S\")", "page.bsp");
        Assert.Contains("PRINT PAGE[\"__sec_S\"];", code);
    }
}
