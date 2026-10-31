// ConversionLib/Formats/DataConverters.cs — JSON, XML, YAML → fenced code blocks

namespace Basic.ConversionLib.Formats;

public static class DataConverters
{
    public static Model.Document ParseJson(byte[] bytes)
    {
        var json = System.Text.Encoding.UTF8.GetString(bytes);
        return Fenced("json", json);
    }

    public static Model.Document ParseXml(byte[] bytes)
    {
        var xml = System.Text.Encoding.UTF8.GetString(bytes);
        return Fenced("xml", xml);
    }

    public static Model.Document ParseYaml(byte[] bytes)
    {
        var yaml = System.Text.Encoding.UTF8.GetString(bytes);
        return Fenced("yaml", yaml);
    }

    private static Model.Document Fenced(string lang, string text) =>
        text.Length > 0
            ? new Model.Document([new Model.CodeBlock(lang, text)], [])
            : Model.Document.Empty;
}
