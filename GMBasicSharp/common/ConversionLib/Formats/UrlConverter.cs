// ConversionLib/Formats/UrlConverter.cs
using System.Net.Http;

namespace Basic.ConversionLib.Formats;

public static class UrlConverter
{
    private static readonly HttpClient _client = new()
    {
        Timeout = TimeSpan.FromSeconds(30)
    };

    static UrlConverter()
    {
        _client.DefaultRequestHeaders.UserAgent.ParseAdd("ConversionLib/1.0");
    }

    public static async Task<Model.Document> ParseUrlAsync(string url)
    {
        try
        {
            var html = await _client.GetStringAsync(url);
            var bytes = System.Text.Encoding.UTF8.GetBytes(html);
            return HtmlConverter.Parse(bytes);
        }
        catch
        {
            return Model.Document.Empty;
        }
    }

    public static Model.Document ParseUrl(string url) => ParseUrlAsync(url).GetAwaiter().GetResult();
}
