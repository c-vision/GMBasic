// FILE: Interpreter.Ast.VectorDB.cs
// VectorDB legacy statement syntax (VECTORDB SEARCH ... INTO ...)
// Namespace database vettoriale autonomo: comandi legacy a spazio +
// funzioni qualificate VECTORDB.XXX(...). Complementa AIVECTOR.*.

using System.Collections.Immutable;
using System.Text;

namespace Basic;

public sealed record VectorDbEnsureCollectionStmt(Expr DbPath, Expr CollectionName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB ENSURECOLLECTION {DbPath}, {CollectionName}";
    public override AstNode DeepClone() => new VectorDbEnsureCollectionStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbIndexTextStmt(Expr DbPath, Expr CollectionName, Expr DocId, Expr Text, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB INDEXTEXT {DbPath}, {CollectionName}, {DocId}, {Text}";
    public override AstNode DeepClone() => new VectorDbIndexTextStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Expr)DocId.DeepClone(), (Expr)Text.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbIndexMetaStmt(Expr DbPath, Expr CollectionName, Expr DocId, Expr Text, Expr MetaJson, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB INDEXMETA {DbPath}, {CollectionName}, {DocId}, {Text}, {MetaJson}";
    public override AstNode DeepClone() => new VectorDbIndexMetaStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Expr)DocId.DeepClone(), (Expr)Text.DeepClone(), (Expr)MetaJson.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbSearchStmt(Expr DbPath, Expr CollectionName, Expr Query, Expr TopK, Target Result, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB SEARCH {DbPath}, {CollectionName}, {Query}, {TopK} INTO {Result}";
    public override AstNode DeepClone() => new VectorDbSearchStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Expr)Query.DeepClone(), (Expr)TopK.DeepClone(), (Target)Result.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbCountStmt(Expr DbPath, Expr CollectionName, Target Result, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB COUNT {DbPath}, {CollectionName} INTO {Result}";
    public override AstNode DeepClone() => new VectorDbCountStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Target)Result.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbListCollectionsStmt(Expr DbPath, Target Result, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB LISTCOLLECTIONS {DbPath} INTO {Result}";
    public override AstNode DeepClone() => new VectorDbListCollectionsStmt((Expr)DbPath.DeepClone(), (Target)Result.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbRemoveSourceStmt(Expr DbPath, Expr CollectionName, Expr SourceId, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB REMOVE SOURCE {DbPath}, {CollectionName}, {SourceId}";
    public override AstNode DeepClone() => new VectorDbRemoveSourceStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Expr)SourceId.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbLoadDocumentsStmt(Expr DbPath, Expr CollectionName, Expr DirectoryPath, Target Result, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB LOAD DOCUMENTS {DbPath}, {CollectionName}, {DirectoryPath} INTO {Result}";
    public override AstNode DeepClone() => new VectorDbLoadDocumentsStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Expr)DirectoryPath.DeepClone(), (Target)Result.DeepClone(), (Token)Token.DeepClone());
}

public sealed record VectorDbPurgeDeprecatedStmt(Expr DbPath, Expr CollectionName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"VECTORDB PURGEDEPRECATED {DbPath}, {CollectionName}";
    public override AstNode DeepClone() => new VectorDbPurgeDeprecatedStmt((Expr)DbPath.DeepClone(), (Expr)CollectionName.DeepClone(), (Token)Token.DeepClone());
}
