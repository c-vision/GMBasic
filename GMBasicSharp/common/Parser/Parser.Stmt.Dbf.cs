// FILE: Interpreter.Parser.Stmt.Dbf.cs
/*
------------------------------------------------------------------------------------------------------------------------
       GUIDA DI RIFERIMENTO ALLA DOPPIA SINTASSI DBF (LEGACY PREFISSATA E QUALIFICATA)
------------------------------------------------------------------------------------------------------------------------
I metodi di parsing in questo file sono stati aggiornati per supportare due stili di sintassi per ogni comando,
garantendo la retrocompatibilità (con l'aggiunta di un prefisso) e introducendo al contempo un'API moderna
e non ambigua per il database DBF.

La sintassi "Legacy Prefissata" è quella tradizionale di dBase/Clipper, ma ora richiede che ogni comando
sia preceduto dalla keyword `DBF` per risolvere le ambiguità.

La sintassi "Moderna" o "Qualificata" usa il nome del namespace per invocare il comando,
eliminando ogni ambiguità, non richiedendo import globali. Il verbo è fuso in un unico
identificatore (es. `DBF.SETORDER`) e NON usa parentesi: le clausole sono le stesse della
forma legacy multi-parola.

--- Esempi di Doppia Sintassi ---

1.  APERTURA E SELEZIONE
   Legacy:  DBF OPEN "sales.dbf" ALIAS Sales SHARED NEW INDEX "idx1.ntx"
   Moderna: DBF.OPEN "sales.dbf" ALIAS Sales

   Legacy:  DBF USE "clienti.dbf" INDEX "idx1.ntx" ALIAS Clienti
   Moderna: DBF.USE "clienti.dbf" INDEX "idx1.ntx" ALIAS Clienti

   Legacy:  DBF SELECT Clienti
   Moderna: DBF.SELECT Clienti

   Legacy:  DBF CLOSE DATABASES
            DBF CLOSE Clienti
            DBF CLOSE Clienti, Ordini
            DBF CLOSE cAlias
   Moderna: DBF.CLOSE DATABASES
            DBF.CLOSE Clienti
            DBF.CLOSE Clienti, Ordini
            DBF.CLOSE cAlias

2.  NAVIGAZIONE
   Legacy:  DBF GO TOP
   Moderna: DBF.GO TOP

   Legacy:  DBF SKIP -5
   Moderna: DBF.SKIP -5

   Legacy:  DBF SEEK "Rossi"
   Moderna: DBF.SEEK "Rossi"

3.  LETTURA DATI
   Legacy:  DBF GET mioRecord
   Moderna: DBF.GET mioRecord

   Legacy:  DBF LIST FIELDS Nome, Citta FOR Saldo > 0
   Moderna: DBF.LIST FIELDS Nome, Citta FOR Saldo > 0

4.  MODIFICA DATI
   Legacy:  DBF APPEND BLANK
   Moderna: DBF.APPENDBLANK

   Legacy:  DBF APPEND FROM "new.dbf" FOR Anno > 2023
   Moderna: DBF.APPENDFROM "new.dbf" FOR Anno > 2023

   Legacy:  DBF REPLACE Prezzo WITH Prezzo * 1.1 FOR Scontato
   Moderna: DBF.REPLACE Prezzo WITH Prezzo * 1.1 FOR Scontato

   Legacy:  DBF DELETE FOR Anno < 2000
   Moderna: DBF.DELETE FOR Anno < 2000

   Legacy:  DBF RECALL ALL
   Moderna: DBF.RECALL ALL

5.  MANUTENZIONE FILE
   Legacy:  DBF PACK
   Moderna: DBF.PACK

   Legacy:  DBF ZAP
   Moderna: DBF.ZAP

   Legacy:  DBF REINDEX
   Moderna: DBF.REINDEX

   Legacy:  DBF VALIDATE Clienti
   Moderna: DBF.VALIDATE Clienti

6.  IMPORT / EXPORT / COPIA
   Legacy:  DBF IMPORT Clienti FROM "data.csv" AS CSV
   Moderna: DBF.IMPORT("Clienti", "data.csv", "CSV")

   Legacy:  DBF EXPORT Clienti TO "data.json" AS JSON
   Moderna: DBF.EXPORT("Clienti", "data.json", "JSON")

   Legacy:  DBF COPY TO "backup.dbf"
   Moderna: DBF.COPYTO "backup.dbf"

   Legacy:  DBF COPY STRUCTURE TO "template.dbf"
   Moderna: DBF.COPYSTRUCTURE "template.dbf"

7.  INDICIZZAZIONE E ORDINAMENTO
   Legacy:  DBF INDEX ON Cognome TO "idx_cogn.ntx"
   Moderna: DBF.INDEXON Cognome TO "idx_cogn.ntx"

   Legacy:  DBF SORT TO "sorted.dbf" ON Cognome, Nome /D
   Moderna: DBF.SORTTO "sorted.dbf" ON Cognome, Nome /D

8.  COMANDI RELAZIONALI E DI AGGREGAZIONE
   Legacy:  DBF JOIN WITH Altro TO "joined.dbf" FOR Condizione
   Moderna: DBF.JOINWITH Altro TO "joined.dbf" FOR Condizione

   Legacy:  DBF UPDATE ON Codice FROM Altro REPLACE Prezzo WITH Altro.Prezzo
   Moderna: DBF.UPDATEON Codice FROM Altro REPLACE Prezzo WITH Altro.Prezzo

   Legacy:  DBF TOTAL ON Regione TO "sales.dbf" FIELDS Importo
   Moderna: DBF.TOTAL ON Regione TO "sales.dbf" FIELDS Importo

9.  COMANDI SET
   Legacy:  DBF SET RELATION TO ClienteID INTO Ordini
   Moderna: DBF.SETRELATION ClienteID INTO Ordini

   Legacy:  DBF SET ORDER TO 1
   Moderna: DBF.SETORDER 1

   Legacy:  DBF SET FILTER TO Anno = 2024
   Moderna: DBF.SETFILTER Anno = 2024

   Legacy:  DBF SET DELETED ON
   Moderna: DBF.SETDELETED ON

NOTA: Tutti i comandi del namespace DBF ora supportano entrambe le sintassi.
------------------------------------------------------------------------------------------------------------------------
*/

using System.Collections.Immutable;

// ReSharper disable SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
// ReSharper disable UnusedParameter.Global

// ReSharper disable UnusedParameter.Local
// ReSharper disable InvalidXmlDocComment
// ReSharper disable InconsistentNaming
namespace Basic;

/// <summary>
/// Contiene i metodi di parsing dedicati per tutte le istruzioni del namespace DBF.
/// Ogni metodo è "contestuale": sa di operare per il namespace DBF e valida
/// la sintassi (qualificata con parentesi o non qualificata legacy) di conseguenza.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi del sottosistema DBF che iniziano con la keyword 'DBF'.
    /// Gestisce sia la sintassi legacy non qualificata (es. `DBF USE ...`) che quella moderna qualificata (`DBF.USE ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'DBF' se la chiamata era qualificata o legacy.</param>
    /// <returns>Il nodo AST per l'istruzione DBF parsata.</returns>
    private static Stmt ParseDbfStatement(Parser parser, string expectedNamespace)
    {
        // Consuma la keyword 'DBF' che ha attivato questo dispatcher.
        parser.Consume(TokenKind.Dbf, "Internal parser error: Expected 'DBF' keyword.");
        var commandToken = parser.Peek();
        // Disambigua i comandi composti basandosi sul token successivo.
        return ParseDbfSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Nuovo helper che agisce come sub-dispatcher per i comandi `SET` del namespace DBF.
    /// Viene chiamato solo dopo che il parser ha già consumato la sequenza `DBF SET`.
    /// La sua unica responsabilità è leggere il sottocomando (es. ORDER, FILTER) e
    /// invocare il parser finale corretto.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'DBF' della chiamata legacy.</param>
    /// <returns>Il nodo Stmt corrispondente al comando SET di DBF parsato.</returns>
    private static Stmt ParseDbfSetSubCommand(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Set, "Internal parser error: Expected 'SET'.");
        var subCommandToken = parser.Peek();
        return subCommandToken.Kind switch
        {
            TokenKind.Order => ParseDbfSetOrderStatement(parser, expectedNamespace),
            TokenKind.Relation => ParseDbfSetRelationStatement(parser, expectedNamespace),
            TokenKind.Filter => ParseDbfSetFilterStatement(parser, expectedNamespace),
            TokenKind.Deleted => ParseDbfSetDeletedStatement(parser, expectedNamespace),
            TokenKind.Exact => ParseDbfSetExactStatement(parser, expectedNamespace),
            TokenKind.Exclusive => ParseDbfSetExclusiveStatement(parser, expectedNamespace),
            TokenKind.Default => ParseDbfSetDefaultStatement(parser, expectedNamespace),
            TokenKind.Path => ParseDbfSetPathStatement(parser, expectedNamespace),
            TokenKind.Schema => ParseDbfSetSchemaStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommandToken,
                $"Sottocomando '{subCommandToken.Lexeme}' non riconosciuto per DBF SET.")
        };
    }

    /// <summary>
    /// Parsa la variante DBF dell'istruzione OPEN. Questo comando è un sinonimo
    /// funzionale di USE, ma con una sintassi legacy leggermente diversa. Apre una work area con un alias.
    /// QUESTA VERSIONE È STATA CORRETTA per usare la clausola ALIAS invece di AS #handle e per
    /// supportare un'espressione (macro) per l'alias.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo <see cref="DbfUseStmt"/> che rappresenta la work area aperta.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy (con ALIAS obbligatorio)
    /// DBF OPEN "sales.dbf" ALIAS Sales SHARED NEW INDEX "idx1.ntx"
    /// DBF OPEN "sales.dbf" ALIAS &cAlias
    /// 
    /// ' Moderna (sintassi semplificata per coerenza)
    /// DBF.OPEN("sales.dbf", "Sales")
    /// </code>
    /// </example>
    public static DbfUseStmt ParseDbfOpenStatement(Parser parser, string expectedNamespace)
    {
        var openToken = parser.Consume(TokenKind.Open, "Internal parser error: Expected 'OPEN'.");

        Expr fileName;
        var indexPaths = ImmutableArray<Expr>.Empty;
        Token? aliasToken = null;
        Token? typeNameToken = null;
        var fieldMappings = ImmutableDictionary<string, string>.Empty;
        var dbLockingMode = DatabaseLockingMode.Default;
        var isUnsafe = false;
        var isNew = false;

        // Sintassi unificata (legacy e qualificata fusa): OPEN fileName [clausole...] ALIAS alias
        fileName = parser.ParseExpression();

        // Loop per clausole flessibili, che possono apparire in qualsiasi ordine.
        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Shared))
            {
                dbLockingMode = DatabaseLockingMode.Shared;
            }
            else if (parser.Match(TokenKind.Exclusive))
            {
                dbLockingMode = DatabaseLockingMode.Exclusive;
            }
            else if (parser.Match(TokenKind.Unsafe))
            {
                isUnsafe = true;
            }
            else if (parser.Match(TokenKind.New))
            {
                isNew = true;
            }
            else if (parser.Match(TokenKind.Index))
            {
                var builder = ImmutableArray.CreateBuilder<Expr>();
                do
                {
                    builder.Add(parser.ParseExpression());
                } while (parser.Match(TokenKind.Comma));

                indexPaths = builder.ToImmutable();
            }
            else if (parser.Match(TokenKind.Alias))
                {
                    if (aliasToken != null) throw CreateError(parser.Previous(), "Clausola ALIAS già specificata.");

                    // L'alias ora è un'espressione per supportare le macro (&cAlias)
                    var aliasExpr = parser.ParseExpression();
                    if (aliasExpr is VariableExpr ve) // Caso 'ALIAS MioAlias'
                    {
                        aliasToken = ve.NameCleaned;
                    }
                    else if (aliasExpr is StringExpr se) // Caso 'ALIAS "MioAlias"'
                    {
                        aliasToken = new Token(TokenKind.Identifier, se.Value, null, se.Position);
                    }
                    else if (aliasExpr is MacroExpr) // Caso 'ALIAS &cAlias'
                    {
                        // Se è una macro, creiamo un token fittizio. Il Rewriter gestirà l'espressione.
                        aliasToken = new Token(TokenKind.Identifier, aliasExpr.ToString(), null, aliasExpr.Position);
                    }
                    else
                    {
                        throw CreateError(parser.Previous(),
                            "L'alias deve essere un identificatore, una stringa o una macro.");
                    }
                }
                else if (parser.Match(TokenKind.As))
                {
                    if (typeNameToken != null)
                        throw CreateError(parser.Previous(), "Clausola AS Tipo già specificata.");
                    typeNameToken = parser.Consume(TokenKind.Identifier, "Expected type name after AS.");
                }
                else if (parser.Match(TokenKind.Map))
                {
                    if (fieldMappings.Count > 0)
                        throw CreateError(parser.Previous(), "Clausola MAP già specificata.");
                    fieldMappings = ParseDbfFieldMapClause(parser);
                }
                else
                {
                    break; // Nessuna clausola riconosciuta, esci dal loop
                }
            }

        if (aliasToken == null)
        {
            throw CreateError(openToken, "La clausola ALIAS è obbligatoria per il comando DBF OPEN.");
        }

        // Dato che DBF OPEN è semanticamente uguale a DBF USE, restituiamo un DbfUseStmt.
        // Questo centralizza la logica nel Linker e nell'Executor.
        return new DbfUseStmt(
            FileName: fileName,
            Locking: dbLockingMode,
            IsUnsafe: isUnsafe,
            IsNew: isNew,
            IndexPaths: indexPaths,
            AliasSymbolId: null, // Sarà risolto dal Binder
            OriginalAliasToken: aliasToken,
            TypeSymbolId: -1, // Sarà risolto dal Linker
            OriginalTypeName: typeNameToken,
            FieldMappings: fieldMappings,
            Token: openToken // Il token originale è 'OPEN'
        );
    }
    internal static Stmt ParseDbfSubcommand(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            // Per i comandi a parola singola, invoca direttamente il parser.
            TokenKind.Open => ParseDbfOpenStatement(parser, expectedNamespace),
            TokenKind.Use => ParseDbfUseStatement(parser, expectedNamespace),
            TokenKind.Select => ParseDbfSelectStatement(parser, expectedNamespace),
            TokenKind.Close => ParseDbfCloseStatement(parser, expectedNamespace),
            TokenKind.Go or TokenKind.Goto => ParseDbfGoStatement(parser, expectedNamespace),
            TokenKind.Skip => ParseDbfSkipStatement(parser, expectedNamespace),
            TokenKind.Seek => ParseDbfSeekStatement(parser, expectedNamespace),
            TokenKind.Get => ParseDbfGetStatement(parser, expectedNamespace),
            TokenKind.Pack => ParseDbfPackStatement(parser, expectedNamespace),
            TokenKind.Zap => ParseDbfZapStatement(parser, expectedNamespace),
            TokenKind.Reindex => ParseDbfReindexStatement(parser, expectedNamespace),
            TokenKind.Delete => ParseDbfDeleteStatement(parser, expectedNamespace),
            TokenKind.Recall => ParseDbfRecallStatement(parser, expectedNamespace),
            TokenKind.Replace => ParseDbfReplaceStatement(parser, expectedNamespace),
            TokenKind.Create => ParseDbfCreateStatement(parser, expectedNamespace),
            TokenKind.Validate => ParseDbfValidateStatement(parser, expectedNamespace, commandToken),
            TokenKind.Begin => ParseBeginTransactionStatement(parser, expectedNamespace),
            TokenKind.Commit => ParseCommitStatement(parser),
            TokenKind.Rollback => ParseRollbackStatement(parser),
            TokenKind.Import => ParseDbfImportStatement(parser, expectedNamespace),
            TokenKind.Export => ParseDbfExportStatement(parser, expectedNamespace),
            TokenKind.Copy => ParseDbfCopyStatement(parser, expectedNamespace), // Gestirà TO vs STRUCTURE
            TokenKind.Append => ParseDbfAppendStatement(parser, expectedNamespace), // Gestirà BLANK vs RECORD vs FROM
            TokenKind.Update => ParseDbfUpdateStatement(parser, expectedNamespace), // UPDATE ON
            TokenKind.Join => ParseDbfJoinStatement(parser, expectedNamespace), // JOIN WITH
            TokenKind.Sort => ParseDbfSortStatement(parser, expectedNamespace), // SORT TO
            TokenKind.Index => ParseDbfIndexOnStatement(parser, expectedNamespace), // INDEX ON
            TokenKind.Total => ParseDbfTotalOnStatement(parser, expectedNamespace), // TOTAL ON
            TokenKind.List => ParseDbfListStatement(parser, expectedNamespace), // TOTAL ON
            TokenKind.Report => ParseDbfReportStatement(parser, expectedNamespace),
            TokenKind.Set =>
                // Per la sintassi legacy, il dispatcher di SET è ancora utile
                ParseDbfSetSubCommand(parser, expectedNamespace),
            TokenKind.Aggregate => ParseDbfAggregateStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken,
                $"Comando '{commandToken.Lexeme}' non riconosciuto per il motore DBF.")
        };
    }



    /// <summary>
    /// Helper per determinare se un token agisce come delimitatore di clausola.
    /// </summary>
    private static bool IsKeywordDelimiter(TokenKind kind)
    {
        return kind switch
        {
            TokenKind.Shared or TokenKind.Exclusive or TokenKind.New or TokenKind.Index or TokenKind.Alias
                or TokenKind.As or TokenKind.Map => true,
            _ => false
        };
    }

    /// <summary>
    /// Parses <c>MAP { dbfField -> typeField [, ...] }</c> after <c>AS TypeName</c>.
    /// Returns TYPE field name (key) to DBF column name (value).
    /// </summary>
    private static ImmutableDictionary<string, string> ParseDbfFieldMapClause(Parser parser)
    {
        parser.Consume(TokenKind.Map, "Expected MAP.");
        parser.Consume(TokenKind.LeftBrace, "Expected '{' after MAP.");

        var builder = ImmutableDictionary.CreateBuilder<string, string>(StringComparer.OrdinalIgnoreCase);
        do
        {
            var dbfToken = parser.Consume(TokenKind.Identifier, "Expected DBF field name in MAP clause.");
            parser.Consume(TokenKind.Arrow, "Expected '->' in MAP field mapping.");
            var typeToken = parser.Consume(TokenKind.Identifier, "Expected TYPE field name after '->'.");

            if (builder.ContainsKey(typeToken.Lexeme))
            {
                throw CreateError(typeToken,
                    $"Duplicate TYPE field '{typeToken.Lexeme}' in MAP clause.");
            }

            builder[typeToken.Lexeme] = dbfToken.Lexeme;
        } while (parser.Match(TokenKind.Comma));

        parser.Consume(TokenKind.RightBrace, "Expected '}' to close MAP clause.");
        return builder.ToImmutable();
    }

    /// <summary>
    /// Parsa la variante DBF del comando `USE`, aggiornato per la piena compatibilità con Clipper 5.3
    /// e per supportare un'espressione (identificatore, stringa o macro) per la clausola ALIAS.
    /// </summary>
    ///
    /// Sintassi (Compatibile Clipper):
    /// Le clausole opzionali possono apparire in qualsiasi ordine.
    /// USE nomeFile [SHARED|EXCLUSIVE] [NEW] [INDEX indici...] [ALIAS alias] [AS TipoDefinito]
    ///
    /// ALIAS (Opzionale): Se omesso, l'alias sarà gestito a runtime dal Rewriter (fallback su nome file).
    /// NEW (Opzionale): Apre il file nella prossima work area disponibile invece di quella corrente.
    ///
    /// Esempi:
    /// DBF USE "clienti.dbf" ALIAS Clienti
    /// DBF USE "ordini.dbf" NEW
    /// DBF USE "fornitori.dbf" ALIAS &cAliasVar
    /// 
    /// Sintassi Moderna (Qualificata):
    /// I parametri sono posizionali e devono apparire in questo ordine.
    /// DBF.USE(fileName AS STRING, [isUnsafe AS BIT], [indexList AS STRING[]], [alias AS STRING], [asType AS STRING])
    ///
    /// Esempio:
    /// DBF.USE("clienti.dbf", FALSE, ["idx1.ntx"], "Clienti2", "Cliente")
    ///
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "DBF") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfUseStmt"/>.</returns>
    public static Stmt ParseDbfUseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Use, "Internal parser error: Expected 'USE'.");
        Expr fileName;
        var indexPaths = ImmutableArray<Expr>.Empty;
        Token? aliasToken = null;
        Token? typeNameToken = null;
        var fieldMappings = ImmutableDictionary<string, string>.Empty;
        var dbLockingMode = DatabaseLockingMode.Default;
        var isUnsafe = false;
        var isNew = false; // Aggiunto flag per NEW

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): clausole flessibili in qualsiasi ordine ---
        fileName = parser.ParseExpression();
        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Shared))
            {
                    dbLockingMode = DatabaseLockingMode.Shared;
                }
                else if (parser.Match(TokenKind.Exclusive))
                {
                    dbLockingMode = DatabaseLockingMode.Exclusive;
                }
                else if (parser.Match(TokenKind.Unsafe))
                {
                    isUnsafe = true;
                }
                else if (parser.Match(TokenKind.New))
                {
                    isNew = true;
                }
                else if (parser.Match(TokenKind.Index))
                {
                    var builder = ImmutableArray.CreateBuilder<Expr>();
                    do
                    {
                        builder.Add(parser.ParseExpression());
                    } while (parser.Match(TokenKind.Comma));

                    indexPaths = builder.ToImmutable();
                }
                else if (parser.Match(TokenKind.Alias))
                {
                    if (aliasToken != null) throw CreateError(parser.Previous(), "Clausola ALIAS già specificata.");

                    // L'alias ora è un'espressione per supportare le macro (&cAlias)
                    var aliasExpr = parser.ParseExpression();
                    if (aliasExpr is VariableExpr ve) // Caso 'ALIAS MioAlias'
                    {
                        aliasToken = ve.NameCleaned;
                    }
                    else if (aliasExpr is StringExpr se) // Caso 'ALIAS "MioAlias"'
                    {
                        aliasToken = new Token(TokenKind.Identifier, se.Value, null, se.Position);
                    }
                    else if (aliasExpr is MacroExpr) // Caso 'ALIAS &cAlias'
                    {
                        // Se è una macro, creiamo un token fittizio. Il Rewriter gestirà l'espressione.
                        aliasToken = new Token(TokenKind.Identifier, aliasExpr.ToString(), null, aliasExpr.Position);
                    }
                    else
                    {
                        throw CreateError(parser.Previous(),
                            "L'alias deve essere un identificatore, una stringa o una macro.");
                    }
                }
                else if (parser.Match(TokenKind.As))
                {
                    if (parser.IsAtEndOfStatement() || IsKeywordDelimiter(parser.Peek().Kind))
                        throw CreateError(parser.Previous(), "Expected type name after AS.");

                    typeNameToken = parser.Advance();
                }
                else if (parser.Match(TokenKind.Map))
                {
                    if (fieldMappings.Count > 0)
                        throw CreateError(parser.Previous(), "Clausola MAP già specificata.");
                    fieldMappings = ParseDbfFieldMapClause(parser);
                }
                else
                {
                    break;
                }
            }

        // In Fase 1, non si eseguono lookup o registrazioni nella SymbolTable.
        // Se l'alias è omesso, aliasToken rimane null e sarà compito del Rewriter gestirlo.
        const int typeSymbolId = -1;
        int? aliasSymbolId = null;

        return new DbfUseStmt(fileName, dbLockingMode, isUnsafe, isNew, indexPaths, aliasSymbolId, aliasToken,
            typeSymbolId, typeNameToken, fieldMappings, commandToken);
    }

    /// <summary>
    /// Parsa la variante DBF del comando `SELECT` per cambiare la work area attiva.
    /// Questo comando è specifico del namespace DBF.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "DBF") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSelectWorkAreaStmt"/>.</returns>
    public static Stmt ParseDbfSelectStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Select, "Internal parser error.");

        Expr workArea;
        // Sintassi unificata (legacy e qualificata fusa): SELECT workArea
        // In Clipper/xBase, un identificatore dopo SELECT e un alias (stringa), non una variabile.
        var next = parser.Peek();
        if (next.Kind == TokenKind.Identifier)
        {
            parser.Advance();
            workArea = new StringExpr(next.Lexeme, next);
        }
        else
        {
            workArea = parser.ParseExpression();
        }

        return new DbfSelectWorkAreaStmt(workArea, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione GO | GOTO per il namespace DBF per posizionare il cursore.
    /// QUESTA VERSIONE È CORRETTA E GESTISCE GO TOP, GO BOTTOM, e GO <n>.
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata
    /// DBF GO TOP
    /// DBF GO BOTTOM
    /// DBF GO 15
    /// DBF GO nRecord
    /// 
    /// ' Sintassi Qualificata
    /// DBF.GO("TOP")
    /// DBF.GO(15)
    /// </code>
    /// </example>
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfGoStmt"/>.</returns>
    public static DbfGoStmt ParseDbfGoStatement(Parser parser, string expectedNamespace)
    {
        Token commandTokenRef;
        Expr destination;

        // Sintassi unificata (legacy e qualificata fusa): GO TOP | GO BOTTOM | GO <n>
        commandTokenRef = parser.Consume(TokenKind.Go, TokenKind.Goto);

        if (parser.Check(TokenKind.Top) || parser.Check(TokenKind.Bottom))
        {
            var kw = parser.Advance();
            destination = new StringExpr(kw.Lexeme, kw);
        }
        else
        {
            destination = parser.ParseExpression();
        }

        return new DbfGoStmt(destination, commandTokenRef);
    }

    /// <summary>
    /// Parsa la variante di SEEK specifica del namespace DBF per la ricerca di una
    /// chiave nell'indice attivo della work area corrente.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "DBF") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSeekStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF SEEK "Rossi"
    /// DBF SEEK 12345
    /// 
    /// ' Sintassi Qualificata
    /// DBF.SEEK("Rossi")
    /// </code>
    /// </example>
    public static DbfSeekStmt ParseDbfSeekStatement(Parser parser, string expectedNamespace)
    {
        var seekToken = parser.Consume(TokenKind.Seek, "Internal parser error: Expected 'SEEK'.");
        Expr value;

        // Sintassi unificata (legacy e qualificata fusa): SEEK value
        value = parser.ParseExpression();

        return new DbfSeekStmt(value, seekToken);
    }

    /// <summary>
    /// Parsa la variante DBF del comando GET per leggere il record corrente in una variabile.
    /// Questo comando opera implicitamente sulla work area attiva e non richiede un handle di file.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "DBF") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfGetRecordStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF GET DbfRecordVar
    /// 
    /// ' Sintassi Qualificata (ipotetica, pronta per l'Executor)
    /// DBF.GET(DbfRecordVar)
    /// </code>
    /// </example>
    public static DbfGetRecordStmt ParseDbfGetStatement(Parser parser, string expectedNamespace)
    {
        var getToken = parser.Consume(TokenKind.Get, "Internal parser error: Expected 'GET'.");
        Target? destination = null;

        // Sintassi unificata (legacy e qualificata fusa): GET [destination]
        if (!parser.IsAtEndOfStatement())
        {
            destination = parser.ParseTarget();
        }

        return new DbfGetRecordStmt(destination, getToken);
    }

    /// <summary>
    /// Parsa la variante di SKIP specifica del namespace DBF per spostare il puntatore
    /// del record nella work area attualmente attiva.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "DBF") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSkipStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF SKIP         ' Avanza di 1 record
    /// DBF SKIP 10      ' Avanza di 10 record
    /// DBF SKIP -5      ' Torna indietro di 5 record
    /// 
    /// ' Sintassi Qualificata
    /// DBF.SKIP()
    /// DBF.SKIP(10)
    /// </code>
    /// </example>
    public static DbfSkipStmt ParseDbfSkipStatement(Parser parser, string expectedNamespace)
    {
        var skipToken = parser.Consume(TokenKind.Skip, "Internal parser error: Expected 'SKIP'.");
        Expr? recordCount = null;

        // Sintassi unificata (legacy e qualificata fusa): SKIP [recordCount]
        if (!parser.IsAtEndOfStatement())
        {
            recordCount = parser.ParseExpression();
        }

        return new DbfSkipStmt(recordCount, skipToken);
    }

    /// <summary>
    /// Parsa una clausola di scopo opzionale (ALL, FOR, WHILE, NEXT n), tipica dei comandi DBF.
    /// Questa è una funzione helper utilizzata da vari comandi come DELETE, RECALL, REPLACE.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <returns>Un nodo AST che rappresenta la clausola di scopo (<see cref="DbfScopeClause"/>) se presente; altrimenti, null.</returns>
    private static DbfScopeClause? ParseScopeClause(Parser parser)
    {
        if (parser.Match(TokenKind.All))
        {
            return new DbfAllClause();
        }

        if (parser.Match(TokenKind.For))
        {
            return new DbfForClause(parser.ParseExpression());
        }

        if (parser.Match(TokenKind.While))
        {
            return new DbfWhileClause(parser.ParseExpression());
        }

        if (parser.Match(TokenKind.Next))
        {
            return new DbfNextClause(parser.ParseExpression());
        }

        if (parser.Match(TokenKind.Record))
        {
            return new DbfRecordClause(parser.ParseExpression());
        }

        if (parser.Match(TokenKind.Rest))
        {
            return new DbfRestClause();
        }

        return null;
    }

    /// <summary>
    /// Parsa un'istruzione LIST per il namespace DBF.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// DBF LIST
    /// DBF LIST FIELDS Nome, Citta FOR Saldo > 0
    /// DBF LIST NEXT 10 TO PRINT
    /// DBF LIST TO FILE "output.txt"
    /// 
    /// ' Moderna (ipotetica, non standard ma supportata per coerenza)
    /// DBF.LIST("Nome, Citta", "Saldo > 0")
    /// </code>
    /// </example>
    public static Stmt ParseDbfListStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.List, "Internal parser error: Expected 'LIST'.");

        DbfScopeClause? scope = null;
        DbfFieldsClause? fields = null;
        Expr? destination = null;
        var toPrint = false;

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): clausole in ordine flessibile ---
        var clauseFound = true;
        while (clauseFound && !parser.IsAtEndOfStatement())
        {
            clauseFound = false;

            var parsedScope = ParseScopeClause(parser);
            if (parsedScope != null)
            {
                if (scope != null)
                    throw CreateError(parser.Previous(),
                        "È possibile specificare una sola clausola di scope (FOR, WHILE, etc.).");
                scope = parsedScope;
                clauseFound = true;
                continue;
            }

            if (parser.Match(TokenKind.Fields))
            {
                if (fields != null)
                    throw CreateError(parser.Previous(), "La clausola FIELDS è già stata specificata.");

                var fieldExpressions = new List<Expr>();
                do
                {
                    fieldExpressions.Add(parser.ParseExpressionFragment(TokenKind.Comma, TokenKind.All, TokenKind.For,
                        TokenKind.While, TokenKind.Next, TokenKind.Record, TokenKind.Rest, TokenKind.To));
                } while (parser.Match(TokenKind.Comma));

                fields = new DbfFieldsClause([..fieldExpressions]);
                clauseFound = true;
                continue;
            }

            if (parser.Match(TokenKind.To))
            {
                if (destination != null || toPrint)
                    throw CreateError(parser.Previous(),
                        "La destinazione (TO PRINT/TO FILE) è già stata specificata.");

                parser.Match(TokenKind.File);

                if (parser.Match(TokenKind.Print))
                {
                    toPrint = true;
                }
                else
                {
                    destination = parser.ParseExpression();
                }

                clauseFound = true;
            }
        }

        return new DbfListStmt(scope, fields, destination, toPrint, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione REPLACE per modificare i campi di uno o più record DBF.
    /// VERSIONE CORRETTA: La sintassi qualificata ora accetta sia una stringa
    /// per la condizione FOR, sia un'espressione lambda.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfReplaceStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// DBF REPLACE NOME WITH "Mario", CITTA WITH "Roma"
    /// DBF REPLACE PREZZO WITH PREZZO * 1.1 FOR CATEGORIA = "Hardware"
    /// DBF REPLACE &cCampo WITH 100
    /// 
    /// ' Moderna (Qualificata fusa)
    /// DBF.REPLACE NOME WITH "Mario", CITTA WITH "Roma"
    /// DBF.REPLACE PREZZO WITH PREZZO * 1.1 FOR CATEGORIA = "Hardware"
    /// </code>
    /// </example>
    public static Stmt ParseDbfReplaceStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Replace, "Internal parser error: Expected 'REPLACE' command.");
        var replacements = ImmutableArray.CreateBuilder<Assignment>();
        DbfScopeClause? scope = null;

        // Sintassi unificata (legacy e qualificata fusa): REPLACE field WITH value [, ...] [FOR cond]
        do
        {
            // Chiama ParseTarget() invece di Consume(Identifier).
            // ParseTarget sa già come gestire variabili, campi, e ora anche le macro.
            var target = parser.ParseTarget();

            parser.Consume(TokenKind.With, "Expected 'WITH' after field name.");
            // TokenKind.All mancava dallo stop-set: `REPLACE campo WITH valore ALL` faceva
            // assorbire "ALL" nell'espressione valore invece di fermarsi, lasciando
            // ParseScopeClause senza token da riconoscere — REPLACE ALL restava sempre
            // sullo scope di default (Current), aggiornando SOLO il record corrente invece
            // di tutti i record (scoperto verificando REPLACE ALL su un CSV con 3 righe:
            // solo la prima veniva modificata).
            var value = parser.ParseExpressionFragment(TokenKind.Comma, TokenKind.All, TokenKind.For,
                TokenKind.While, TokenKind.Next, TokenKind.Record, TokenKind.Rest);

            replacements.Add(new Assignment(target, value));
        } while (parser.Match(TokenKind.Comma));

        scope = ParseScopeClause(parser);

        return new DbfReplaceStmt(replacements.ToImmutable(), scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione APPEND per il namespace DBF, che funge da mini-dispatcher.
    /// La sintassi qualificata per APPEND FROM ora accetta la clausola
    /// FOR come una stringa, allineandosi agli altri comandi DBF.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST specifico per la variante di APPEND.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// DBF APPEND BLANK
    /// DBF APPEND RECORD clienteRecord
    /// DBF APPEND FROM "nuovi.dbf" FOR CITTA = "Roma"
    /// 
    /// ' Moderna (Qualificata fusa)
    /// DBF.APPENDBLANK
    /// DBF.APPENDRECORD clienteRecord
    /// DBF.APPENDFROM "nuovi.dbf" FOR CITTA = "Roma"
    /// </code>
    /// </example>
    public static Stmt ParseDbfAppendStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: DBF APPEND BLANK | RECORD expr | FROM file [FOR scope]
        var commandToken = parser.Consume(TokenKind.Append, "Internal parser error: Expected 'APPEND'.");
        var isBlank = parser.Match(TokenKind.Blank);
        var isRecord = !isBlank && parser.Match(TokenKind.Record);
        var isFrom = !isBlank && !isRecord && parser.Match(TokenKind.From);
        if (!isBlank && !isRecord && !isFrom)
            throw CreateError(parser.Peek(),
                "Invalid APPEND syntax for DBF. Expected 'BLANK', 'RECORD', or 'FROM'.");

        if (isBlank)
        {
            return new DbfAppendBlankStmt(commandToken);
        }

        if (isRecord)
        {
            var source = parser.ParseExpression();
            return new DbfAppendRecordStmt(source, commandToken);
        }

        var fileName = parser.ParseExpression();
        DbfForClause? scope = null;
        if (parser.Check(TokenKind.For))
        {
            var scopeClause = ParseScopeClause(parser);
            if (scopeClause is DbfForClause forClause)
            {
                scope = forClause;
            }
            else if (scopeClause is not null)
            {
                throw CreateError(parser.Previous(), "Only a FOR clause is valid for APPEND FROM.");
            }
        }

        return new DbfAppendFromStmt(fileName, scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione DELETE per marcare record nel namespace DBF.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfDeleteStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF DELETE
    /// DBF DELETE FOR SALDO = 0
    /// 
    /// ' Sintassi Qualificata
    /// DBF.DELETE()
    /// DBF.DELETE("SALDO = 0")
    /// DBF.DELETE(FN(rec) => rec.SALDO = 0)
    /// </code>
    /// </example>
    public static DbfDeleteStmt ParseDbfDeleteStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Delete, "Internal parser error.");
        DbfScopeClause? scope = null;

        // Sintassi unificata (legacy e qualificata fusa): DELETE [FOR condizione]
        scope = ParseScopeClause(parser);

        return new DbfDeleteStmt(scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione RECALL per ripristinare record nel namespace DBF.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfRecallStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF RECALL
    /// DBF RECALL FOR SALDO < 0
    /// 
    /// ' Sintassi Qualificata
    /// DBF.RECALL()
    /// DBF.RECALL("SALDO < 0")
    /// DBF.RECALL(FN(rec) => rec.SALDO < 0)
    /// </code>
    /// </example>
    public static DbfRecallStmt ParseDbfRecallStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Recall, "Internal parser error.");
        DbfScopeClause? scope = null;

        // Sintassi unificata (legacy e qualificata fusa): RECALL [FOR condizione]
        scope = ParseScopeClause(parser);

        return new DbfRecallStmt(scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione PACK per rimuovere fisicamente i record cancellati nel namespace DBF.
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF PACK
    /// 
    /// ' Sintassi Qualificata
    /// DBF.PACK()
    /// </code>
    /// </example>
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfPackStmt"/>.</returns>
    public static DbfPackStmt ParseDbfPackStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Pack, "Internal parser error.");
        return new DbfPackStmt(commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione ZAP per rimuovere tutti i record nel namespace DBF.
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF ZAP
    /// 
    /// ' Sintassi Qualificata
    /// DBF.ZAP()
    /// </code>
    /// </example>
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfZapStmt"/>.</returns>
    public static Stmt ParseDbfZapStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Zap, "Internal parser error.");
        return new DbfZapStmt(commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione REINDEX per ricostruire gli indici nel namespace DBF.
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF REINDEX
    /// 
    /// ' Sintassi Qualificata
    /// DBF.REINDEX()
    /// </code>
    /// </example>
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfReindexStmt"/>.</returns>
    public static DbfReindexStmt ParseDbfReindexStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Reindex, "Internal parser error.");
        return new DbfReindexStmt(commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione CLOSE per il namespace DBF, gestendo sia la chiusura totale
    /// (`CLOSE DATABASES`) che quella selettiva di una o più work area (`CLOSE <alias>`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfCloseStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Non Qualificata):
    /// <code>
    /// ' Chiude tutte le work area DBF aperte.
    /// DBF CLOSE DATABASES
    /// 
    /// ' Chiude la work area con alias "Clienti".
    /// DBF CLOSE Clienti
    /// 
    /// ' Chiude più work area.
    /// DBF CLOSE Clienti, Ordini
    /// 
    /// ' Chiude una work area specificata tramite una variabile.
    /// cAlias = "Fatture"
    /// DBF CLOSE cAlias
    /// </code>
    /// Sintassi Moderna (Qualificata):
    /// <code>
    /// ' Chiude tutte le work area DBF aperte.
    /// DBF.CLOSE()
    /// 
    /// ' Chiude la work area con alias "Clienti".
    /// DBF.CLOSE("Clienti")
    /// 
    /// ' Chiude più work area.
    /// DBF.CLOSE("Clienti", "Ordini")
    /// 
    /// ' Chiude una work area specificata tramite una variabile.
    /// cAlias = "Fatture"
    /// DBF.CLOSE(cAlias)
    /// </code>
    /// </example>
    public static Stmt ParseDbfCloseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Close, "Internal parser error: Expected 'CLOSE'.");
        var workAreas = ImmutableArray.CreateBuilder<Expr>();

        // Sintassi unificata (legacy e qualificata fusa): CLOSE [DATABASES|ALL] | CLOSE alias1, alias2, ...
        var all = false;
        if (parser.Match(TokenKind.Databases) || parser.Match(TokenKind.All))
        {
            // CLOSE DATABASES / CLOSE ALL → chiudi TUTTE le aree.
            all = true;
        }
        else if (parser.IsAtEndOfStatement())
        {
            // CLOSE nudo (Clipper) → chiudi SOLO l'area corrente.
        }
        else
        {
            // È un CLOSE selettivo.
            workAreas.AddRange(parser.ParseExpressionList(TokenKind.Newline));
        }

        return new DbfCloseStmt(workAreas.ToImmutable(), commandToken, all);
    }

    /// <summary>
    /// Parsa un'istruzione IMPORT per il namespace DBF.
    /// Gestisce sia la sintassi legacy prefissata (`DBF IMPORT ...`) che quella moderna qualificata (`DBF.IMPORT(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'DBF' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST di tipo <see cref="ImportFromFileStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy Prefissata
    /// DBF IMPORT Clienti FROM "clienti.csv" AS CSV
    /// 
    /// ' Moderna Qualificata
    /// DBF.IMPORT("Clienti", "clienti.csv", "CSV")
    /// </code>
    /// </example>
    private static ImportFromFileStmt ParseDbfImportStatement(Parser parser, string expectedNamespace)
    {
        var importToken = parser.Consume(TokenKind.Import, "Internal parser error: Expected 'IMPORT'.");
        var engineType = new Token(TokenKind.Dbf, "DBF", null, importToken.Position);

        // Sintassi legacy: DBF IMPORT alias FROM "sourceFile" AS CSV | JSON
        var area = parser.ParseExpression(); // L'alias della work area
        parser.Consume(TokenKind.From, "Expected 'FROM' after the work area alias in IMPORT statement.");
        var filePath = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' to specify format.");
        var format = parser.Consume(TokenKind.Csv, TokenKind.Json);

        return new ImportFromFileStmt(engineType, area, filePath, format, importToken);
    }

    /// <summary>
    /// Parsa un'istruzione EXPORT per il namespace DBF.
    /// Gestisce sia la sintassi legacy prefissata (`DBF EXPORT ...`) che quella moderna qualificata (`DBF.EXPORT(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'DBF' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST di tipo <see cref="ExportToFileStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy Prefissata
    /// DBF EXPORT Clienti TO "clienti.csv" AS CSV
    /// DBF EXPORT TO "current.csv" AS CSV ' Area corrente
    /// DBF EXPORT "file.csv" AS CSV // Sintassi con area implicita
    /// 
    /// ' Moderna Qualificata
    /// DBF.EXPORT("Clienti", "clienti.csv", "CSV")
    /// DBF.EXPORT(0, "current.csv", "CSV") // Area corrente specificata come 0
    /// </code>
    /// </example>
    private static ExportToFileStmt ParseDbfExportStatement(Parser parser, string expectedNamespace)
    {
        var exportToken = parser.Consume(TokenKind.Export, "Internal parser error: Expected 'EXPORT'.");
        var engineType = new Token(TokenKind.Dbf, "DBF", null, exportToken.Position);
        Expr area;
        Expr filePath;
        Token format;

        // --- Sintassi Legacy: DBF EXPORT [area] TO "file" AS CSV|JSON ---
        // Utilizza il lookahead (Check) prima di parsare.

        if (parser.Check(TokenKind.To))
        {
            // Scenario 1: DBF EXPORT TO "file.csv" AS CSV (Area implicita)
            parser.Consume(TokenKind.To, "Internal logic error."); // Consuma 'TO'
            area = new NumberExpr(0, new Token(TokenKind.Number, "0", 0.0, exportToken.Position));
            filePath = parser.ParseExpression();
        }
        else
        {
            // Scenario 2: DBF EXPORT area TO "file.csv" AS CSV
            // Scenario 3: DBF EXPORT "file.csv" AS CSV (obsoleto, ma gestito per robustezza)
            var firstExpression = parser.ParseExpression();

            if (parser.Match(TokenKind.To))
            {
                // È il caso "DBF EXPORT area TO 'file'"
                area = firstExpression;
                filePath = parser.ParseExpression();
            }
            else if (parser.Check(TokenKind.As))
            {
                // È il caso "DBF EXPORT 'file' AS" (area implicita)
                area = new NumberExpr(0,
                    new Token(TokenKind.Number, "0", 0.0, exportToken.Position));
                filePath = firstExpression;
            }
            else
            {
                throw CreateError(parser.Peek(),
                    "Expected 'TO' or 'AS' after expression in DBF EXPORT statement.");
            }
        }

        parser.Consume(TokenKind.As, "Expected 'AS' to specify format.");
        format = parser.Consume(TokenKind.Csv, TokenKind.Json);

        return new ExportToFileStmt(engineType, area, filePath, format, exportToken);
    }

    /// <summary>
    /// Parsa un'istruzione `TOTAL ON ... TO ...` per il namespace DBF.
    /// VERSIONE CORRETTA: La sintassi qualificata ora accetta sia una stringa
    /// per la condizione FOR, sia un'espressione lambda.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfTotalOnStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ' Legacy
    /// DBF TOTAL ON CodiceCliente TO "summary.dbf" FIELDS Importo
    /// DBF TOTAL ON Regione TO "sales.dbf" FOR Anno = 2024
    ///
    /// ' Moderna (Qualificata fusa)
    /// DBF.TOTAL ON CodiceCliente TO "summary.dbf" FIELDS Importo
    /// DBF.TOTAL ON Regione TO "sales.dbf" FOR Anno = 2024
    /// </code>
    /// </example>
    public static Stmt ParseDbfTotalOnStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Total, "Internal parser error: Expected 'TOTAL'.");
        Expr keyExpression;
        Expr fileName;
        DbfFieldsClause? fields = null;
        DbfForClause? scope = null;

        // Sintassi unificata (legacy e qualificata fusa): TOTAL ON key TO file [FIELDS ...] [FOR cond]
        parser.Consume(TokenKind.On, "Expected 'ON' after TOTAL.");
        keyExpression = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after key expression.");
        fileName = parser.ParseExpression();
        if (parser.Match(TokenKind.Fields))
        {
            // Parsa una lista di espressioni, che gestirà correttamente nomi semplici (Campo)
            // e nomi qualificati (Tabella.Campo). La lista termina all'inizio della clausola FOR
            // o alla fine della riga.
            var fieldExpressions = parser.ParseExpressionList(TokenKind.For);
            fields = new DbfFieldsClause(fieldExpressions);
        }

        if (!parser.Check(TokenKind.For))
            return new DbfTotalOnStmt(keyExpression, fileName, fields, scope, commandToken);
        var scopeClause = ParseScopeClause(parser);
        if (scopeClause is DbfForClause forClause)
        {
            scope = forClause;
        }
        else if (scopeClause is not null)
        {
            throw CreateError(parser.Previous(), "Only FOR clause is allowed in TOTAL.");
        }

        return new DbfTotalOnStmt(keyExpression, fileName, fields, scope, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `SORT TO ... ON ...` per il namespace DBF.
    /// VERSIONE CORRETTA: La sintassi qualificata ora accetta sia una stringa
    /// per la condizione FOR, sia un'espressione lambda.
    /// La sintassi legacy gestisce l'ordine flessibile delle clausole ON e TO.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSortStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ' Legacy
    /// DBF SORT ON Cognome, Nome TO "sorted.dbf"
    /// DBF SORT TO "top_clients.dbf" ON Fatturato /D FOR Attivo = TRUE
    ///
    /// ' Moderna (Qualificata fusa)
    /// DBF.SORTTO "sorted.dbf" ON Cognome, Nome
    /// DBF.SORTTO "top_clients.dbf" ON Fatturato /D FOR Attivo = TRUE
    /// </code>
    /// </example>
    public static Stmt ParseDbfSortStatement(Parser parser, string expectedNamespace)
    {
        Token sortToken;
        Expr fileName;
        var sortFields = ImmutableArray.CreateBuilder<DbfSortField>();
        DbfForClause? scope = null;

        void ParseSortFieldsLegacy()
        {
            do
            {
                Expr? fieldExpr = null;
                Token fieldNameToken;
                var descending = false;

                // Controlla se è una macro (&...)
                if (parser.Check(TokenKind.Ampersand))
                {
                    // Usa ParseExpression per catturare la macro completa (&var)
                    fieldExpr = parser.ParseExpression();
                    // Usiamo un token fittizio per compatibilità, ma il valore vero è nell'espressione
                    fieldNameToken = new Token(TokenKind.Identifier, fieldExpr.ToString(), null,
                        fieldExpr.Position);
                }
                else
                {
                    // Caso standard: Identificatore
                    fieldNameToken = parser.Consume(TokenKind.Identifier, "Expected field name for sorting.");
                }

                // Controlla il modificatore /D o /A
                if (parser.Match(TokenKind.Divide) && parser.Match(TokenKind.Identifier))
                {
                    if (parser.Previous().Lexeme.Equals("D", StringComparison.OrdinalIgnoreCase)) descending = true;
                }

                sortFields.Add(new DbfSortField(-1, fieldNameToken, descending, fieldExpr));
            } while (parser.Match(TokenKind.Comma));
        }

        // Sintassi legacy: SORT TO "file" ON campo1, campo2 /D [FOR cond] (o ON ... TO ...)
        var commandToken = parser.Consume(TokenKind.Sort, "Internal parser error.");
        sortToken = commandToken;

        if (parser.Match(TokenKind.On))
        {
            ParseSortFieldsLegacy();
            parser.Consume(TokenKind.To, "Expected 'TO' after field list.");
            fileName = parser.ParseExpression();
        }
        else if (parser.Match(TokenKind.To))
        {
            fileName = parser.ParseExpression();
            parser.Consume(TokenKind.On, "Expected 'ON' after file name.");
            ParseSortFieldsLegacy();
        }
        else
        {
            throw CreateError(parser.Peek(), "Expected 'ON' or 'TO' after SORT.");
        }

        var scopeClause = ParseScopeClause(parser);
        if (scopeClause is DbfForClause forClause) scope = forClause;
        else if (scopeClause is not null)
            throw CreateError(parser.Previous(), "Only FOR clause is allowed in SORT.");

        return new DbfSortStmt(fileName, sortFields.ToImmutable(), scope, sortToken);
    }

    /// <summary>
    /// Parsa un'istruzione `JOIN WITH ... TO ... FOR ...` per il namespace DBF.
    /// VERSIONE CORRETTA: La sintassi qualificata ora accetta sia una stringa
    /// per la condizione FOR, sia un'espressione lambda.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfJoinWithStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ' Legacy
    /// DBF JOIN WITH Altro TO "joined.dbf" FOR Condizione
    ///
    /// ' Moderna (Qualificata fusa)
    /// DBF.JOINWITH Altro TO "joined.dbf" FOR Ordini.IdCliente = Clienti.Id
    /// </code>
    /// </example>
    public static Stmt ParseDbfJoinStatement(Parser parser, string expectedNamespace)
    {
        Token aliasToken;
        Expr fileName;
        DbfForClause condition;
        DbfFieldsClause? fields = null;
        Token joinToken;

        // Sintassi legacy: JOIN WITH alias TO "file" FOR cond [FIELDS ...]
        var commandToken = parser.Consume(TokenKind.Join, "Internal parser error: Expected 'JOIN'.");
        joinToken = commandToken;
        parser.Consume(TokenKind.With, "Expected 'WITH' after JOIN.");

        aliasToken = parser.Consume(TokenKind.Identifier, "Expected alias of work area to join with.");
        parser.Consume(TokenKind.To, "Expected 'TO' after alias.");
        fileName = parser.ParseExpression();
        parser.Consume(TokenKind.For, "Expected 'FOR' clause to define join condition.");
        condition = new DbfForClause(parser.ParseExpression());
        if (parser.Match(TokenKind.Fields))
        {
            // Parsa una lista di espressioni, che gestirà correttamente nomi semplici (Campo)
            // e nomi qualificati (Tabella.Campo). La lista termina all'inizio della clausola FOR
            // o alla fine della riga.
            var fieldExpressions = parser.ParseExpressionList(TokenKind.For);
            fields = new DbfFieldsClause(fieldExpressions);
        }

        const int aliasSymbolId = -1;

        return new DbfJoinWithStmt(aliasSymbolId, aliasToken, fileName, condition, fields, joinToken);
    }

    /// <summary>
    /// Parsa un'istruzione `UPDATE ON ... FROM ... REPLACE ...` per il namespace DBF.
    /// VERSIONE CORRETTA: La sintassi qualificata ora accetta sia una stringa
    /// per la chiave di join (ON), sia un'espressione lambda.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfUpdateStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ' Legacy
    /// DBF UPDATE ON CodiceArticolo FROM Magazzino REPLACE Prezzo WITH Magazzino.NuovoPrezzo
    ///
    /// ' Moderna (Qualificata fusa)
    /// DBF.UPDATEON CodiceArticolo FROM Magazzino REPLACE Prezzo WITH Magazzino.NuovoPrezzo
    /// </code>
    /// </example>
    public static Stmt ParseDbfUpdateStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: UPDATE ON key FROM alias REPLACE field WITH value [, ...]
        var commandToken = parser.Consume(TokenKind.Update, "Internal parser error: Expected 'UPDATE'.");
        parser.Consume(TokenKind.On, "Expected 'ON' after UPDATE.");
        Expr keyExpr;
        Token aliasToken;
        var replacements = ImmutableArray.CreateBuilder<DbfReplacement>();

        keyExpr = parser.ParseExpression();
        parser.Consume(TokenKind.From, "Expected 'FROM' after key expression.");
        aliasToken = parser.Consume(TokenKind.Identifier, "Expected alias of source work area.");
        parser.Consume(TokenKind.Replace, "Expected 'REPLACE' after alias.");
        do
        {
            var fieldName = parser.Consume(TokenKind.Identifier, "Expected field name to replace.");

            var (baseName, _) = SplitNameAndSuffix(fieldName.Lexeme);
            var nameCleaned = fieldName with { Lexeme = baseName };

            parser.Consume(TokenKind.With, "Expected 'WITH' after field name.");
            var value = parser.ParseExpression();

            replacements.Add(new DbfReplacement(-1, fieldName, nameCleaned, value));
        } while (parser.Match(TokenKind.Comma));

        const int aliasSymbolId = -1;

        return new DbfUpdateStmt(keyExpr, aliasSymbolId, aliasToken, replacements.ToImmutable(), commandToken);
    }

    /// <summary>
    /// Dispatcher per il comando COPY del namespace DBF.
    /// Questo metodo analizza i token successivi per distinguere tra le varianti
    /// del comando (`COPY TO` e `COPY STRUCTURE TO`) e invoca il parser specifico.
    /// Gestisce sia la sintassi legacy non qualificata che quella moderna qualificata.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher centrale.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST specifico per il comando COPY parsato.</returns>
    /// <example>
    /// Sintassi gestite da questo dispatcher:
    /// <code>
    /// ' Legacy, reindirizzato a ParseDbfCopyToStatement
    /// DBF COPY TO "backup.dbf" FOR Anno = 2024
    /// 
    /// ' Legacy, reindirizzato a ParseDbfCopyStructureStatement
    /// DBF COPY STRUCTURE TO "template.dbf"
    /// 
    /// ' Moderno, reindirizzato a ParseDbfCopyToStatement
    /// DBF.COPYTO "backup.dbf" FOR Anno = 2024
    /// 
    /// ' Moderno, reindirizzato a ParseDbfCopyStructureStatement
    /// DBF.COPYSTRUCTURE "template.dbf"
    /// </code>
    /// </example>
    public static Stmt ParseDbfCopyStatement(Parser parser, string expectedNamespace)
    {
        // SINTASSI LEGACY: Consuma solo il comando base e il token successivo.
        var copyToken = parser.Consume(TokenKind.Copy, "Internal error.");
        if (parser.Match(TokenKind.Structure))
        {
            return ParseDbfCopyStructureStatement(parser, expectedNamespace, copyToken);
        }

        return parser.Match(TokenKind.To)
            ? ParseDbfCopyToStatement(parser, expectedNamespace)
            : throw CreateError(parser.Peek(), "Invalid COPY syntax. Expected 'TO' or 'STRUCTURE'.");
    }

    /// <summary>
    /// Parsa la variante di COPY specifica del namespace DBF per esportare dati.
    /// VERSIONE CORRETTA: La sintassi qualificata ora accetta sia una stringa
    /// per la condizione FOR, sia un'espressione lambda.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "DBF") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfCopyToStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF COPY TO "export.dbf"
    /// DBF COPY TO "filtered.dbf" FOR City = "Rome"
    /// 
    /// ' Sintassi Qualificata fusa
    /// DBF.COPYTO "export.dbf"
    /// DBF.COPYTO "filtered.dbf" FOR City = "Rome"
    /// </code>
    /// </example>
    public static DbfCopyToStmt ParseDbfCopyToStatement(Parser parser, string expectedNamespace)
    {
        Expr fileName;
        DbfForClause? scope = null;
        var commandToken = parser.Previous(); // Il token 'COPY'/'TO'

        // Sintassi legacy: COPY TO file [FOR cond]
        fileName = parser.ParseExpression();
        var scopeClause = ParseScopeClause(parser);
        if (scopeClause is DbfForClause forClause)
        {
            scope = forClause;
        }
        else if (scopeClause is not null)
        {
            throw CreateError(parser.Previous(), "Only the FOR clause is valid for the COPY TO command.");
        }

        return new DbfCopyToStmt(fileName, scope, commandToken);
    }

    /// <summary>
    /// Parsa la variante di COPY specifica del namespace DBF per duplicare la struttura di un file.
    /// Questo comando crea un nuovo file DBF vuoto basato sulla struttura della work area attiva.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace (es. "DBF") se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfCopyStructureStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Sintassi Non Qualificata (richiede IMPORT DBF)
    /// DBF COPY STRUCTURE TO "template.dbf"
    /// 
    /// ' Sintassi Qualificata fusa
    /// DBF.COPYSTRUCTURE "template.dbf"
    /// </code>
    /// </example>
    private static DbfCopyStructureStmt ParseDbfCopyStructureStatement(Parser parser, string expectedNamespace,
        Token? copyToken)
    {
        Expr fileName;

        if (copyToken?.Kind == TokenKind.Dbf)
        {
            // Sintassi qualificata fusa: il dispatcher ha già consumato 'COPYSTRUCTURE'.
            // La clausola TO è opzionale (COPYSTRUCTURE fonde COPY + STRUCTURE).
            parser.Match(TokenKind.To);
            fileName = parser.ParseExpression();
        }
        else
        {
            // Sintassi legacy: il dispatcher ha già consumato 'COPY' e 'STRUCTURE'.
            parser.Consume(TokenKind.To, "Expected 'TO' after 'STRUCTURE'.");
            fileName = parser.ParseExpression();
        }

        return new DbfCopyStructureStmt(fileName, copyToken ?? parser.Previous());
    }

    /// <summary>
    /// Parsa il comando `SET DELETED ON|OFF` specifico del namespace DBF.
    /// </summary>
    public static Stmt ParseDbfSetDeletedStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF) SET DELETED ON|OFF — il dispatcher ha già consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Deleted, "Internal error: Expected 'DELETED'.");
        var stateToken = parser.Advance();
        if (stateToken.Kind is not (TokenKind.On or TokenKind.Off))
            throw CreateError(stateToken, "Expected ON or OFF after 'SET DELETED'.");
        return new DbfSetDeletedStmt(stateToken, commandToken);
    }

    /// <summary>
    /// Parsa il comando `SET EXACT ON|OFF` specifico del namespace DBF.
    /// Gestisce correttamente entrambe le sintassi, riconoscendo che per la sintassi qualificata il token del comando
    /// è già stato identificato dal dispatcher.
    /// </summary>
    public static Stmt ParseDbfSetExactStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF) SET EXACT ON|OFF — il dispatcher ha già consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Exact, "Internal error: Expected 'EXACT'.");
        var stateToken = parser.Advance();
        if (stateToken.Kind is not (TokenKind.On or TokenKind.Off))
            throw CreateError(stateToken, "Expected ON or OFF after 'SET EXACT'.");
        return new DbfSetExactStmt(stateToken, commandToken);
    }

    /// <summary>
    /// Parsa il comando `SET FILTER TO [condizione]` specifico del namespace DBF.
    /// La condizione può essere un'espressione logica standard o una lambda expression.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfSetFilterStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi Gestite:</b>
    /// <code>
    /// ' 1. Rimuove il filtro
    /// DBF SET FILTER TO
    /// 
    /// ' 2. Filtro con espressione logica semplice
    /// DBF SET FILTER TO Prezzo > 100 AND Categoria = "Hardware"
    /// 
    /// ' 3. Filtro con Lambda a espressione singola (=>)
    /// DBF SET FILTER TO FUNCTION(r AS RecordTipo) AS BIT => r.Prezzo > 100
    /// 
    /// ' 4. Filtro con Lambda a blocco con graffe ({...})
    /// DBF SET FILTER TO FUNCTION(r AS RecordTipo) AS BIT { IF r.Attivo THEN FUNCTION = TRUE }
    ///
    /// ' 5. Filtro con Lambda a blocco tradizionale (:...END FUNCTION)
    /// DBF SET FILTER TO FUNCTION(r AS RecordTipo) AS BIT: FUNCTION = r.Saldo > 0: END FUNCTION
    /// </code>
    /// </example>
    public static Stmt ParseDbfSetFilterStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF) SET FILTER TO [condizione] — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Filter, "Internal error: Expected 'FILTER'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'FILTER'.");
        Expr? condition = null;
        if (!parser.IsAtEndOfStatement())
        {
            condition = parser.ParseExpression();
        }
        return new DbfSetFilterStmt(condition, commandToken);
    }

    /// <summary>
    /// Parsa gli argomenti per un comando `SET DEFAULT TO` (es. `TO path` o `(path)`).
    /// Questo metodo è un helper condiviso, chiamato dai sub-dispatcher di DBF e ISAM
    /// dopo che la sequenza `DBF SET DEFAULT` o `ISAM SET DEFAULT` è già stata consumata.
    /// </summary>
    /// <remarks>
    /// <para><b>Sintassi Legacy Prefissata Gestita:</b></para>
    /// <code>
    /// DBF SET DEFAULT TO "C:\DATI"
    /// ISAM SET DEFAULT TO "C:\DATI"
    /// </code>
    /// 
    /// <para><b>Sintassi Moderna (Qualificata fusa) Gestita:</b></para>
    /// <code>
    /// DBF.SETDEFAULT "C:\DATI"
    /// ISAM.SETDEFAULT App.DataPath + "\DBF"
    /// </code>
    /// </remarks>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF" o "ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del comando specifico (es. 'SET' per legacy, 'SETDEFAULT' per qualificato).</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSetDefaultStmt"/>.</returns>
    public static Stmt ParseDbfSetDefaultStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF/ISAM) SET DEFAULT TO path — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Default, "Internal error: Expected 'DEFAULT'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'DEFAULT'.");
        var path = parser.ParseExpression();
        return new DbfSetDefaultStmt(path, commandToken);
    }

    /// <summary>
    /// Parsa gli argomenti per un comando SET PATH (es. `TO path` o `(path)`).
    /// Questo metodo è un helper chiamato dai sub-dispatcher di DBF e ISAM
    /// dopo che la sequenza `DBF SET PATH` o `ISAM SET PATH` è già stata consumata.
    /// </summary>
    /// <remarks>
    /// <para><b>Sintassi Legacy Prefissata Gestita:</b></para>
    /// <code>
    /// DBF SET PATH TO "C:\DATI"
    /// ISAM SET PATH TO "C:\DATI"
    /// </code>
    /// 
    /// <para><b>Sintassi Moderna (Qualificata fusa) Gestita:</b></para>
    /// <code>
    /// DBF.SETPATH "C:\DATI"
    /// ISAM.SETPATH "C:\DATI"
    /// </code>
    /// </remarks>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF" o "ISAM") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del comando specifico (es. 'SET' per legacy, 'SETPATH' per qualificato).</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSetPathStmt"/>.</returns>
    public static Stmt ParseDbfSetPathStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF/ISAM) SET PATH TO pathList — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Path, "Internal error: Expected 'PATH'.");
        // CA-Clipper: 'SET PATH' (senza TO) o 'SET PATH TO' (senza espressione) SVUOTA la
        // lista (si cerca solo nella DEFAULT). Anche 'SET PATH TO ""' svuota (runtime).
        Expr pathList = new StringExpr("", commandToken);
        if (parser.Peek().Kind == TokenKind.To)
        {
            parser.Consume(TokenKind.To, "Expected 'TO' after 'PATH'.");
            var next = parser.Peek().Kind;
            if (next != TokenKind.Newline && next != TokenKind.Colon && next != TokenKind.Eof)
                pathList = parser.ParseExpression();
        }
        return new DbfSetPathStmt(pathList, commandToken);
    }

    /// <summary>
    /// Parsa il comando `SET EXCLUSIVE` per controllare se i file DBF vengono aperti
    /// in modalità esclusiva per impostazione predefinita.
    /// Gestisce sia la sintassi legacy che quella moderna qualificata.
    /// </summary>
    /// <remarks>
    /// <para><b>Sintassi Legacy:</b></para>
    /// <code>SET EXCLUSIVE ON|OFF</code>
    /// <example>
    /// DBF SET EXCLUSIVE ON
    /// </example>
    /// 
    /// <para><b>Sintassi Moderna (Qualificata fusa):</b></para>
    /// <code>DBF.SETEXCLUSIVE ON|OFF</code>
    /// <example>
    /// DBF.SETEXCLUSIVE ON
    /// </example>
    /// </remarks>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSetExclusiveStmt"/>.</returns>
    public static Stmt ParseDbfSetExclusiveStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF) SET EXCLUSIVE ON|OFF — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Exclusive, "Internal error: Expected 'EXCLUSIVE'.");
        var stateToken = parser.Advance();
        if (stateToken.Kind is not (TokenKind.On or TokenKind.Off))
            throw CreateError(stateToken, "Expected ON or OFF after 'SET EXCLUSIVE'.");
        return new DbfSetExclusiveStmt(stateToken, commandToken);
    }

    /// <summary>
    /// Parsa il comando `SET ORDER TO` per impostare l'indice attivo per la work area corrente.
    /// Gestisce sia la sintassi legacy che quella moderna qualificata.
    /// </summary>
    /// <remarks>
    /// <para><b>Sintassi Legacy:</b></para>
    /// <code>SET ORDER TO espressione_indice</code>
    /// <example>
    /// DBF SET ORDER TO 1
    /// DBF SET ORDER TO 0  ' Disattiva l'indice
    /// </example>
    /// 
    /// <para><b>Sintassi Moderna (Qualificata fusa):</b></para>
    /// <code>DBF.SETORDER espressione_indice</code>
    /// <example>
    /// DBF.SETORDER nIndex
    /// </example>
    /// </remarks>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSetOrderStmt"/>.</returns>
    public static Stmt ParseDbfSetOrderStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF) SET ORDER TO val — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Order, "Internal error: Expected 'ORDER'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'ORDER'.");
        var value = parser.ParseExpression();
        return new DbfSetOrderStmt(value, commandToken);
    }

    /// <summary>
    /// Parsa il comando `SET RELATION TO` per collegare due work area.
    /// Gestisce sia la sintassi legacy che quella moderna qualificata.
    /// </summary>
    /// <remarks>
    /// <para><b>Sintassi Legacy:</b></para>
    /// <code>DBF SET RELATION TO espressione_chiave INTO alias</code>
    /// <code>DBF SET RELATION TO  ' Disattiva la relazione</code>
    /// <example>
    /// DBF SET RELATION TO CustomerID INTO "Orders"
    /// </example>
    /// 
    /// <para><b>Sintassi Moderna (Qualificata fusa):</b></para>
    /// <code>DBF.SETRELATION espressione_chiave INTO alias</code>
    /// <code>DBF.SETRELATION  ' Disattiva la relazione</code>
    /// </remarks>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfSetRelationStmt"/>.</returns>
    public static Stmt ParseDbfSetRelationStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF) SET RELATION TO [key] INTO alias — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Relation, "Internal error: Expected 'RELATION'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'RELATION'.");

        Expr? keyExpression = null;
        Token? childAlias = null;
        if (parser.IsAtEndOfStatement()) return new DbfSetRelationStmt(keyExpression, childAlias, commandToken);
        keyExpression = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected 'INTO' after relation key expression.");
        childAlias = parser.Consume(TokenKind.Identifier, "Expected work area alias after 'INTO'.");

        return new DbfSetRelationStmt(keyExpression, childAlias, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `INDEX ON ... TO ...` per il namespace DBF.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST di tipo <see cref="DbfIndexOnStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// ' Legacy
    /// DBF INDEX ON Cognome TO "idx_cogn.ntx"
    /// DBF INDEX ON UPPER(TRIM(Cognome)) + UPPER(TRIM(Nome)) TO "idx_nomin.ntx"
    ///
    /// ' Moderna (Qualificata fusa)
    /// DBF.INDEXON Cognome TO "idx_cogn.ntx"
    /// DBF.INDEXON UPPER(TRIM(Cognome)) + UPPER(TRIM(Nome)) TO "idx_nomin.ntx"
    /// </code>
    /// </example>
    public static DbfIndexOnStmt ParseDbfIndexOnStatement(Parser parser, string expectedNamespace)
    {
        Token indexToken;
        Expr keyExpression;
        Expr fileName;
        Expr? tagName = null;

        // Sintassi legacy: DBF INDEX ON key TO file | TAG tag OF file
        var commandToken = parser.Consume(TokenKind.Index, "Internal parser error: Expected 'INDEX'.");
        indexToken = commandToken;
        parser.Consume(TokenKind.On, "Expected 'ON' after INDEX.");

        keyExpression = parser.ParseExpression();
        if (parser.Check(TokenKind.Tag))
        {
            parser.Advance();
            tagName = parser.ParseExpression();
            parser.Consume(TokenKind.Of, "Expected 'OF' after TAG name for compound index file.");
            fileName = parser.ParseExpression();
        }
        else
        {
            parser.Consume(TokenKind.To, "Expected 'TO' after key expression.");
            fileName = parser.ParseExpression();
            if (parser.Check(TokenKind.Tag))
            {
                parser.Advance();
                tagName = parser.ParseExpression();
            }
        }

        return new DbfIndexOnStmt(keyExpression, fileName, indexToken, tagName);
    }

    /// <summary>
    /// Parsa il comando `DBF SET SCHEMA STRICT ON|OFF` specifico del namespace DBF.
    /// È ora completamente autonomo e responsabile di consumare
    /// l'intera sequenza di token, a partire da 'SET'.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="DbfSetSchemaStmt"/>.</returns>
    public static Stmt ParseDbfSetSchemaStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: (DBF) SET SCHEMA STRICT ON|OFF — il dispatcher ha consumato 'SET'.
        var commandToken = parser.Previous();
        parser.Consume(TokenKind.Schema, "Internal error: Expected 'SCHEMA'.");
        parser.Consume(TokenKind.Strict, "Expected 'STRICT' after 'SCHEMA'.");
        var stateToken = parser.Advance();
        if (stateToken.Kind is not (TokenKind.On or TokenKind.Off))
            throw CreateError(stateToken, "Expected ON or OFF after 'SET SCHEMA STRICT'.");
        return new DbfSetSchemaStmt(stateToken, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `DBF VALIDATE [area]` per il namespace DBF.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <param name="validateToken">Il token 'VALIDATE' originale, passato dal dispatcher polimorfico.</param>
    /// <returns>Un nodo AST <see cref="DbfValidateStmt"/>.</returns>
    /// <example>
    /// Sintassi gestita:
    /// <code>
    /// DBF VALIDATE Clienti
    /// DBF.VALIDATE Clienti ' Qualificata fusa
    /// </code>
    /// </example>
    public static DbfValidateStmt ParseDbfValidateStatement(Parser parser, string expectedNamespace,
        Token validateToken)
    {
        Expr? area = null;

        // Sintassi unificata (legacy e qualificata fusa): VALIDATE [area]
        parser.Consume(TokenKind.Validate, "Internal parser error: Expected 'VALIDATE'.");
        if (!parser.IsAtEndOfStatement())
        {
            area = parser.ParseExpression();
        }

        return new DbfValidateStmt(area, validateToken);
    }

    /// <summary>
    /// Parsa un'istruzione `DBF AGGREGATE` per calcolare statistiche di riepilogo su un file DBF.
    /// Questo metodo gestisce sia la sintassi legacy prefissata (`DBF AGGREGATE ON ... COMPUTE ...`)
    /// che la sintassi moderna qualificata (`DBF.AGGREGATE ON ... COMPUTE ...`), normalizzando entrambe
    /// in un nodo AST <see cref="DbfAggregateStmt"/>.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'DBF' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST <see cref="DbfAggregateStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Non Qualificata):
    /// <code>
    /// ' Raggruppa per Regione, calcolando somma e conteggio.
    /// DBF AGGREGATE ON Regione TO "summary.dbf" COMPUTE SUM(Importo) AS Totale, COUNT(*) AS Numero
    /// 
    /// ' Aggregazione con filtro.
    /// DBF AGGREGATE ON Citta TO "city_sales.dbf" FOR Anno = 2024 COMPUTE AVG(Importo) AS MediaVendite
    /// </code>
    /// Sintassi Moderna (Qualificata fusa):
    /// <code>
    /// ' Definizioni di aggregazione in un array literal.
    /// DIM defs AS STRING[] = {"SUM(Importo) AS Totale", "COUNT(*) AS Numero"}
    /// DBF.AGGREGATE ON Regione TO "summary.dbf" COMPUTE SUM(Importo) AS Totale, COUNT(*) AS Numero
    /// 
    /// ' Aggregazione con filtro.
    /// DBF.AGGREGATE ON Citta TO "city_sales.dbf" FOR Anno = 2024 COMPUTE AVG(Importo) AS MediaVendite
    /// </code>
    /// </example>
    public static Stmt ParseDbfAggregateStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Aggregate, "Internal parser error: Expected 'AGGREGATE'.");

        Expr keyExpression;
        Expr fileName;
        DbfForClause? scope = null;
        Expr aggregationsExpr;

        // Sintassi unificata (legacy e qualificata fusa): AGGREGATE ON key TO file [FOR scope] COMPUTE ...
        parser.Consume(TokenKind.On, "Expected 'ON' after AGGREGATE.");
        keyExpression = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after key expression.");
        fileName = parser.ParseExpression();

        var parsedScope = ParseScopeClause(parser);
        if (parsedScope is DbfForClause fc) scope = fc;
        else if (parsedScope != null)
            throw CreateError(parser.Previous(), "Only a FOR clause is allowed in AGGREGATE.");

        parser.Consume(TokenKind.Compute, "Expected 'COMPUTE' keyword.");

        var aggStringExprs = new List<Expr>();
        do
        {
            var startPos = parser._current;

            // Avanza fino alla prossima virgola o alla fine dello statement,
            // rispettando le parentesi per le espressioni interne.
            var parenDepth = 0;
            while (!parser.IsAtEndOfStatement() && (!parser.Check(TokenKind.Comma) || parenDepth > 0))
            {
                if (parser.Check(TokenKind.LeftParen)) parenDepth++;
                else if (parser.Check(TokenKind.RightParen)) parenDepth--;
                parser.Advance();
            }

            var endPos = parser._current;

            var tokensForAgg = parser._tokens.Skip(startPos).Take(endPos - startPos).ToList();
            if (tokensForAgg.Any())
            {
                var aggString = string.Join(" ", tokensForAgg.Select(t => t.Lexeme));
                aggStringExprs.Add(new StringExpr(aggString.Trim(), tokensForAgg.First()));
            }
        } while (parser.Match(TokenKind.Comma));

        // Crea un ArrayLiteralExpr sintetico
        aggregationsExpr = new ArrayLiteralExpr([..aggStringExprs], commandToken);

        return new DbfAggregateStmt(keyExpression, fileName, aggregationsExpr, scope, commandToken);
    }

    /// <summary>
    /// [HELPER DI ISTANZA] Parsa una singola definizione di aggregazione dal flusso di token corrente
    /// (es. `SUM(Importo) AS Totale`).
    /// </summary>
    private AggregateDefinition ParseSingleAggregateDefinition()
    {
        var funcToken = Peek();
        AggregateKind kind;

        // Determina il tipo di funzione di aggregazione.
        if (Match(TokenKind.Count)) kind = AggregateKind.Count;
        else if (Match(TokenKind.Sum)) kind = AggregateKind.Sum;
        else if (Match(TokenKind.Avg)) kind = AggregateKind.Avg;
        else if (Match(TokenKind.Min)) kind = AggregateKind.Min;
        else if (Match(TokenKind.Max)) kind = AggregateKind.Max;
        else
        {
            throw CreateError(funcToken,
                $"Unknown aggregation function '{funcToken.Lexeme}'. Expected COUNT, SUM, AVG, MIN or MAX.");
        }

        Consume(TokenKind.LeftParen, "Expected '('.");

        Expr? expr = null;
        if (kind == AggregateKind.Count && Match(TokenKind.Multiply))
        {
            // Gestione COUNT(*) -> l'espressione rimane null.
        }
        else if (!Check(TokenKind.RightParen))
        {
            expr = ParseExpression();
        }
        else if (kind != AggregateKind.Count) // COUNT() è valida, le altre no.
        {
            throw CreateError(Previous(), "Expression required for this aggregation function.");
        }

        Consume(TokenKind.RightParen, "Expected ')'.");
        Consume(TokenKind.As, "Expected 'AS' for alias.");
        var alias = Consume(TokenKind.Identifier, "Expected alias name.");

        return new AggregateDefinition(kind, expr, alias);
    }

    /// <summary>
    /// Parsa un'istruzione `DBF CREATE` per creare un nuovo file DBF basato sulla struttura di un tipo definito dall'utente (UDT).
    /// Questo metodo gestisce sia la sintassi legacy prefissata (`DBF CREATE ... FROM TYPE ...`)
    /// che la sintassi moderna qualificata (`DBF.CREATE ... FROM TYPE ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'DBF' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST <see cref="DbfCreateStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Non Qualificata):
    /// <code>
    /// TYPE DatiCliente
    ///     Nome AS STRING * 30
    ///     Citta AS STRING * 20
    /// END TYPE
    /// DBF CREATE "clienti.dbf" FROM TYPE DatiCliente
    /// </code>
    /// Sintassi Moderna (Qualificata fusa):
    /// <code>
    /// DBF.CREATE "clienti.dbf" FROM TYPE DatiCliente
    /// </code>
    /// </example>
    public static Stmt ParseDbfCreateStatement(Parser parser, string expectedNamespace)
    {
        var createToken = parser.Consume(TokenKind.Create, "Internal parser error: Expected 'CREATE'.");
        if (parser.Check(TokenKind.View))
        {
            return ParseDbfCreateViewStatement(parser, expectedNamespace, createToken);
        }

        Expr fileName;
        Token typeNameToken;

        // Sintassi unificata (legacy e qualificata fusa): CREATE "file" FROM TYPE TypeName
        fileName = parser.ParseExpression();
        parser.Consume(TokenKind.From, "Expected 'FROM' after file name in CREATE statement.");
        parser.Consume(TokenKind.Type, "Expected 'TYPE' after 'FROM'.");
        typeNameToken = parser.Consume(TokenKind.Identifier, "Expected a user-defined type name after 'TYPE'.");

        return new DbfCreateStmt(fileName, typeNameToken, createToken);
    }

    /// <summary>
    /// Parsa un'istruzione `DBF REPORT FORM` per generare un report tabulare.
    /// Gestisce sia la sintassi legacy (`DBF REPORT FORM ...`) che quella moderna (`DBF.REPORTFORM ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("DBF") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'DBF' se la chiamata era qualificata o legacy.</param>
    /// <returns>Un nodo AST <see cref="DbfReportStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Non Qualificata):
    /// <code>
    /// ' Genera un report per tutti i record.
    /// DBF REPORT FORM "elenco.frx"
    /// 
    /// ' Report con filtro e output su stampante.
    /// DBF REPORT FORM "fatture.frx" FOR Saldo > 0 TO PRINT
    /// </code>
    /// Sintassi Moderna (Qualificata fusa):
    /// <code>
    /// ' Report con filtro e output su file.
    /// DBF.REPORTFORM "elenco.frx" FOR Saldo > 0 TO "output.txt"
    /// </code>
    /// </example>
    public static Stmt ParseDbfReportStatement(Parser parser, string expectedNamespace)
    {
        Token reportToken;
        Expr formFile;
        DbfScopeClause? scope = null;
        Expr? destination = null;
        var toPrint = false;

        // Sintassi legacy: REPORT FORM file [scope] [TO dest|PRINT]
        var commandToken = parser.Consume(TokenKind.Report, "Internal parser error: Expected 'REPORT'.");
        reportToken = commandToken;
        parser.Consume(TokenKind.Form, "Expected 'FORM' after 'REPORT'.");

        formFile = parser.ParseExpression();

        var clauseFound = true;
        while (clauseFound && !parser.IsAtEndOfStatement())
        {
            clauseFound = false;
            var parsedScope = ParseScopeClause(parser);
            if (parsedScope != null)
            {
                if (scope != null)
                    throw CreateError(parser.Previous(), "La clausola di scope è già stata specificata.");
                scope = parsedScope;
                clauseFound = true;
                continue;
            }

            if (parser.Match(TokenKind.To))
            {
                if (destination != null || toPrint)
                    throw CreateError(parser.Previous(), "La destinazione è già stata specificata.");
                if (parser.Match(TokenKind.Print))
                {
                    toPrint = true;
                }
                else
                {
                    parser.Match(TokenKind.File); // Opzionale
                    destination = parser.ParseExpression();
                }

                clauseFound = true;
            }
        }

        return new DbfReportStmt(formFile, scope, destination, toPrint, reportToken);
    }

    /// <summary>
    /// Parsa `DBF CREATE VIEW viewName AS SELECT ...` e `DBF.CREATEVIEW viewName AS SELECT ...`.
    /// </summary>
    public static DbfCreateViewStmt ParseDbfCreateViewStatement(Parser parser, string expectedNamespace,
        Token? createTokenOverride = null)
    {
        Token commandToken;
        Token viewNameToken;
        DbfSelectQuery? selectQuery = null;
        Expr? queryExpr = null;

        // Sintassi legacy: CREATE VIEW viewName AS SELECT ... | expr
        commandToken = createTokenOverride ?? parser.Consume(TokenKind.Create, "Expected 'CREATE'.");
        parser.Consume(TokenKind.View, "Expected 'VIEW' after 'CREATE'.");

        viewNameToken = parser.Consume(TokenKind.Identifier, "Expected view name.");
        parser.Consume(TokenKind.As, "Expected 'AS' after view name.");

        if (parser.Check(TokenKind.Select))
        {
            selectQuery = ParseDbfViewSelectQuery(parser);
        }
        else
        {
            queryExpr = parser.ParseExpression();
        }

        return new DbfCreateViewStmt(-1, viewNameToken, selectQuery, queryExpr, commandToken);
    }

    /// <summary>
    /// Parsa la query SELECT di una vista DBF (FROM obbligatorio; JOIN/WHERE opzionali).
    /// </summary>
    public static DbfSelectQuery ParseDbfViewSelectQuery(Parser parser, Token? selectTokenOverride = null)
    {
        var selectToken = selectTokenOverride ?? parser.Consume(TokenKind.Select, "Expected SELECT.");
        var fields = ParseSelectClause(parser);

        FromClause? from = null;
        JoinClause? join = null;
        Expr? where = null;

        while (true)
        {
            SkipNewlines(parser);
            if (parser.IsAtEnd()) break;

            switch (parser.Peek().Kind)
            {
                case TokenKind.From:
                    if (from != null) throw CreateError(parser.Peek(), "Clausola FROM duplicata.");
                    from = ParseFromClause(parser);
                    break;
                case TokenKind.Join:
                    if (join != null) throw CreateError(parser.Peek(), "Clausola JOIN duplicata.");
                    join = ParseJoinClause(parser);
                    break;
                case TokenKind.Where:
                    if (where != null) throw CreateError(parser.Peek(), "Clausola WHERE duplicata.");
                    where = ParseWhereClause(parser);
                    break;
                default:
                    goto end_clause_loop;
            }
        }

        end_clause_loop:

        if (from == null)
            throw CreateError(selectToken, "Clausola FROM mancante nell'istruzione CREATE VIEW.");

        return new DbfSelectQuery(fields, from, join, where, selectToken);
    }


}