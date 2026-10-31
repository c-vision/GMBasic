// ConversionLib/Formats/RtfParser.cs — via RtfPipe
using System.Collections.Immutable;
using System.Text;
using RtfPipe;

namespace Basic.ConversionLib.Formats;

public static class RtfParser
{
    public static Model.Document Parse(byte[] bytes)
    {
        var rtf = Encoding.ASCII.GetString(bytes);
        var html = RtfPipe.Rtf.ToHtml(rtf);
        if (string.IsNullOrWhiteSpace(html)) return Model.Document.Empty;

        return HtmlConverter.Parse(Encoding.UTF8.GetBytes(html));
    }
}
