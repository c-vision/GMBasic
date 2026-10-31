// FILE: Interpreter.Parser.Stmt.VectorDb.cs
// VECTORDB: database vettoriale autonoma su SQLite.
// Sintassi legacy a spazio (comandi) + funzioni qualificate VECTORDB.XXX(...).

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene i metodi del Parser per le istruzioni del modulo database VECTORDB.
/// Solo comandi legacy multi-parola (direttiva: comandi a spazio, funzioni qualificate
/// con parentesi). Il modulo è autonomo: gli AI lo USANO, non lo sostituiscono.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher d'ingresso per i comandi legacy `VECTORDB ...` (consuma la keyword).
    /// </summary>
    private static Stmt ParseVectorDbStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.VectorDb, "Internal parser error: Expected 'VECTORDB' keyword.");
        return ParseVectorDbSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Dispatcher dei subcomandi VECTORDB (chiamato dopo che 'VECTORDB' è stato consumato).
    /// </summary>
    internal static Stmt ParseVectorDbSubcommand(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            TokenKind.Ensure => ParseVectorDbEnsureCollectionStatement(parser, expectedNamespace),
            TokenKind.Index => parser.At(1).Kind switch
            {
                TokenKind.Text => ParseVectorDbIndexTextStatement(parser, expectedNamespace),
                TokenKind.Meta => ParseVectorDbIndexMetaStatement(parser, expectedNamespace),
                _ => throw CreateError(parser.At(1), "Expected TEXT or META after INDEX.")
            },
            TokenKind.Search => ParseVectorDbSearchStatement(parser, expectedNamespace),
            TokenKind.Count => ParseVectorDbCountStatement(parser, expectedNamespace),
            TokenKind.List => ParseVectorDbListCollectionsStatement(parser, expectedNamespace),
            TokenKind.Remove => ParseVectorDbRemoveSourceStatement(parser, expectedNamespace),
            TokenKind.Load => ParseVectorDbLoadDocumentsStatement(parser, expectedNamespace),
            TokenKind.Purge => ParseVectorDbPurgeDeprecatedStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken,
                $"Comando '{commandToken.Lexeme}' non riconosciuto per il motore VECTORDB.")
        };
    }

    /// <summary>
    /// <c>VECTORDB ENSURE COLLECTION db, coll</c> — crea (o riusa) una collection vettoriale.
    /// </summary>
    private static Stmt ParseVectorDbEnsureCollectionStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Ensure, "Expected ENSURE.");
        parser.Consume(TokenKind.Collection, "Expected COLLECTION after ENSURE.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        return new VectorDbEnsureCollectionStmt(db, coll, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB INDEX TEXT db, coll, docId, text</c> — indicizza un documento.
    /// </summary>
    private static Stmt ParseVectorDbIndexTextStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Index, "Expected INDEX.");
        parser.Consume(TokenKind.Text, "Expected TEXT after INDEX.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after collection.");
        var docId = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after docId.");
        var text = parser.ParseExpression();
        return new VectorDbIndexTextStmt(db, coll, docId, text, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB INDEX META db, coll, docId, text, metaJson</c> — indicizza testo + metadata
    /// JSON (5 argomenti, come il gemello <c>AI RAG INDEX META</c>). Prima aveva solo 4
    /// argomenti (mancava `text`): l'emitter era costretto a spacciare metaJson per il testo
    /// indicizzato e azzerava sempre i metadata reali — VECTORDB PURGE DEPRECATED (che filtra
    /// per lo status nei metadata) restava un no-op silenzioso su ogni documento indicizzato
    /// così.
    /// </summary>
    private static Stmt ParseVectorDbIndexMetaStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Index, "Expected INDEX.");
        parser.Consume(TokenKind.Meta, "Expected META after INDEX.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after collection.");
        var docId = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after docId.");
        var text = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after text.");
        var metaJson = parser.ParseExpression();
        return new VectorDbIndexMetaStmt(db, coll, docId, text, metaJson, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB SEARCH db, coll, query, topK INTO result</c> — ricerca semantica (JSON in result).
    /// </summary>
    private static Stmt ParseVectorDbSearchStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Search, "Expected SEARCH.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after collection.");
        var query = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after query.");
        var topK = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected INTO after topK.");
        var result = parser.ParseTarget();
        return new VectorDbSearchStmt(db, coll, query, topK, result, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB COUNT db, coll INTO result</c> — conteggio embedding della collection.
    /// </summary>
    private static Stmt ParseVectorDbCountStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Count, "Expected COUNT.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected INTO after collection.");
        var result = parser.ParseTarget();
        return new VectorDbCountStmt(db, coll, result, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB LIST COLLECTIONS db INTO result</c> — elenco collection (JSON in result).
    /// </summary>
    private static Stmt ParseVectorDbListCollectionsStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.List, "Expected LIST.");
        parser.Consume(TokenKind.Collections, "Expected COLLECTIONS after LIST.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected INTO after db.");
        var result = parser.ParseTarget();
        return new VectorDbListCollectionsStmt(db, result, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB REMOVE SOURCE db, coll, sourceId</c> — elimina documento ed embedding.
    /// </summary>
    private static Stmt ParseVectorDbRemoveSourceStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Remove, "Expected REMOVE.");
        parser.Consume(TokenKind.Source, "Expected SOURCE after REMOVE.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after collection.");
        var sourceId = parser.ParseExpression();
        return new VectorDbRemoveSourceStmt(db, coll, sourceId, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB LOAD DOCUMENTS db, coll, dir INTO result</c> — indicizza i documenti di una directory.
    /// </summary>
    private static Stmt ParseVectorDbLoadDocumentsStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Load, "Expected LOAD.");
        parser.Consume(TokenKind.Documents, "Expected DOCUMENTS after LOAD.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after collection.");
        var dir = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected INTO after directory.");
        var result = parser.ParseTarget();
        return new VectorDbLoadDocumentsStmt(db, coll, dir, result, commandToken);
    }

    /// <summary>
    /// <c>VECTORDB PURGE DEPRECATED db, coll</c> — elimina i documenti con stato deprecato.
    /// </summary>
    private static Stmt ParseVectorDbPurgeDeprecatedStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Purge, "Expected PURGE.");
        parser.Consume(TokenKind.Deprecated, "Expected DEPRECATED after PURGE.");
        var db = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after db.");
        var coll = parser.ParseExpression();
        return new VectorDbPurgeDeprecatedStmt(db, coll, commandToken);
    }
}
