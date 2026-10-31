// FILE: Parser.Stmt.Compression.cs
namespace Basic;

public sealed partial class Parser
{
    /// <summary>ZIP &lt;sorgente&gt; TO &lt;dest.zip&gt; — sorgente: file singolo, cartella, o più percorsi separati da ';'.</summary>
    public static Stmt ParseZipStatement(Parser parser, string expectedNamespace)
    {
        var zipToken = parser.Consume(TokenKind.Zip, "Internal parser error: Expected 'ZIP'.");
        var source = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' in ZIP statement.");
        var destination = parser.ParseExpression();
        return new ZipStmt(source, destination, zipToken);
    }

    /// <summary>UNZIP &lt;archivio.zip&gt; TO &lt;cartella&gt;.</summary>
    public static Stmt ParseUnzipStatement(Parser parser, string expectedNamespace)
    {
        var unzipToken = parser.Consume(TokenKind.Unzip, "Internal parser error: Expected 'UNZIP'.");
        var archive = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' in UNZIP statement.");
        var destination = parser.ParseExpression();
        return new UnzipStmt(archive, destination, unzipToken);
    }
}
