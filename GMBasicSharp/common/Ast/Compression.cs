// FILE: Ast.Compression.cs
using System.Text.Json.Serialization;

namespace Basic;

/// <summary>ZIP &lt;sorgente&gt; TO &lt;dest.zip&gt; — comprime file/cartelle in un archivio ZIP.</summary>
[JsonDerivedType(typeof(ZipStmt), nameof(ZipStmt))]
public sealed record ZipStmt(Expr Source, Expr Destination, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"ZIP {Source} TO {Destination}";

    public override AstNode DeepClone() =>
        new ZipStmt((Expr)Source.DeepClone(), (Expr)Destination.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>UNZIP &lt;archivio.zip&gt; TO &lt;cartella&gt; — estrae un archivio ZIP.</summary>
[JsonDerivedType(typeof(UnzipStmt), nameof(UnzipStmt))]
public sealed record UnzipStmt(Expr Archive, Expr Destination, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"UNZIP {Archive} TO {Destination}";

    public override AstNode DeepClone() =>
        new UnzipStmt((Expr)Archive.DeepClone(), (Expr)Destination.DeepClone(), (Token)Token.DeepClone());
}
