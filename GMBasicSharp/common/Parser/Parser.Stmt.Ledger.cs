// FILE: Interpreter.Parser.Stmt.Ledger.cs
/*
------------------------------------------------------------------------------------------------------------------------
        GUIDA DI RIFERIMENTO ALLA DOPPIA SINTASSI LEDGER (LEGACY E QUALIFICATA FUSA)
------------------------------------------------------------------------------------------------------------------------
I metodi di parsing in questo file supportano due stili di sintassi per ogni comando,
garantendo la retrocompatibilità e introducendo al contempo un'API moderna e non ambigua per il sistema contabile.

La sintassi "Legacy" o "Non Qualificata" è quella tradizionale, che richiede l'import del
namespace (`IMPORT LEDGER`) e usa una sintassi a più parole (`LEDGER SUBCOMMAND ...`).

La sintassi "Moderna" o "Qualificata Fusa" usa il nome del namespace per invocare il comando
(es. `LEDGER.OPEN ...`): il verbo è fuso in un unico identificatore e NON usa parentesi.
La forma qualificata fusa riusa le STESSE clausole/argomenti della forma legacy multi-parola.

--- Esempi di Doppia Sintassi ---

1.  OPEN
    Legacy:  LEDGER OPEN "file.dat" AS hndl
    Moderna: LEDGER.OPEN "file.dat" AS hndl

2.  ADD
    Legacy:  LEDGER ADD <handle> WITH <data>, <desc$> {<postings>} [INTO <nId>]
    Moderna: LEDGER.ADD <handle> WITH <data>, <desc$> {<postings>} [INTO <nId>]

3.  REGISTER
    Legacy:  LEDGER REGISTER hndl INTO arr() WHERE "query"
    Moderna: LEDGER.REGISTER hndl INTO arr() WHERE "query"

4.  BALANCE
    Legacy:  LEDGER BALANCE hndl INTO arr() WHERE "query"
    Moderna: LEDGER.BALANCE hndl INTO arr() WHERE "query"

5.  EDIT
    Legacy:  LEDGER EDIT <hndl> ID <txId> INTO <nId> WITH <data>, <desc$> {<postings>}
    Moderna: LEDGER.EDIT <hndl> ID <txId> INTO <nId> WITH <data>, <desc$> {<postings>}

6.  DELETE
    Legacy:  LEDGER DELETE hndl ID id
    Moderna: LEDGER.DELETE hndl ID id

7.  SAVE, CLOSE, PACK
    Legacy:  LEDGER SAVE hndl
    Moderna: LEDGER.SAVE hndl

8.  ACCOUNTS
    Legacy:  LEDGER ACCOUNTS hndl INTO arr()
    Moderna: LEDGER.ACCOUNTS hndl INTO arr()

9.  VALIDATE
    Legacy:  LEDGER VALIDATE hndl INTO isValid
    Moderna: LEDGER.VALIDATE hndl INTO isValid
------------------------------------------------------------------------------------------------------------------------
*/

using System.Collections.Immutable;

// ReSharper disable ParameterOnlyUsedForPreconditionCheck.Global

// ReSharper disable SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault

// ReSharper disable UnusedParameter.Local
// ReSharper disable ParameterOnlyUsedForPreconditionCheck.Local

// ReSharper disable InvalidXmlDocComment
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Contiene i metodi di parsing dedicati per tutte le istruzioni del namespace LEDGER.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi del namespace LEDGER che iniziano con la keyword 'LEDGER'
    /// (sintassi legacy a più parole, es. `LEDGER OPEN ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Dovrebbe essere sempre `null` per la sintassi legacy.</param>
    /// <returns>Il nodo Stmt per l'istruzione LEDGER parsata.</returns>
    private static Stmt ParseLedgerStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Ledger, "Internal parser error: Expected 'LEDGER' keyword.");

        var subCommand = parser.Peek();

        // Per i comandi che restituiscono un valore nella sintassi legacy, dobbiamo costruire un LetStmt.
        // Dobbiamo anticipare il parsing del target dell'assegnazione.
        return ParseLedgerSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER OPEN`, gestendo entrambe le sintassi di comando.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerOpenStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// LEDGER OPEN "path/to/ledger.dat" AS hndl
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// LEDGER.OPEN "path/to/ledger.dat" AS hndl
    /// </code>
    /// </example>
    public static LedgerOpenStmt ParseLedgerOpenStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Open, "Internal parser error: Expected 'OPEN'.");

        // Sintassi unificata (legacy e qualificata fusa): OPEN filePath AS handle
        Expr filePath = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' after file path in LEDGER OPEN.");
        Target handle = parser.ParseTarget();

        return new LedgerOpenStmt(filePath, handle);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER ADD`. Gestisce sia il comando legacy con clausole flessibili e `INTO`,
    /// sia il comando qualificato fuso (stesse clausole, senza parentesi).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerAddStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// LEDGER ADD hndl WITH DATETIME.TODAY(), "Salario" {
    ///     "Attivita:Banca": 1500.00,
    ///     "Entrate:Salario": -1500.00
    /// } INTO newTxId
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// LEDGER.ADD hndl WITH DATETIME.TODAY(), "Cena", {
    ///     "Spese:Cibo": 35.50,
    ///     "Attivita:Contanti": -35.50
    /// } INTO newTxId
    /// </code>
    /// </example>
    public static Stmt ParseLedgerAddStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Add, "Internal parser error: Expected 'ADD'.");

        // --- Sintassi unificata (legacy e qualificata fusa): clausole flessibili ---
        Expr? handle = null;
        Expr? date = null;
        Expr? description = null;
        DictionaryLiteralExpr? postings = null;
        Target? newIdVar = null;

        var flags = new Dictionary<string, bool>
            { { "handle", false }, { "newId", false }, { "dateDesc", false }, { "postings", false } };

        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Into))
            {
                if (flags["newId"]) throw CreateError(parser.Previous(), "L'ID di output è già stato specificato (INTO).");
                newIdVar = parser.ParseTarget();
                flags["newId"] = true;
            }
            else if (parser.Match(TokenKind.With))
            {
                if (flags["dateDesc"])
                    throw CreateError(parser.Previous(), "Data e descrizione sono già stati specificati (WITH).");
                date = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Attesa virgola per separare data e descrizione.");
                description = parser.ParseExpression();
                flags["dateDesc"] = true;
            }
            else if (parser.Check(TokenKind.LeftBrace))
            {
                if (flags["postings"]) throw CreateError(parser.Previous(), "I posting sono già stati specificati.");
                var expr = parser.ParseExpression();
                postings = expr as DictionaryLiteralExpr ??
                           throw CreateError(parser.Previous(),
                               "I postings devono essere specificati come Dizionario Literal { conto$:importo }.");
                flags["postings"] = true;
            }
            else
            {
                if (!flags["handle"])
                {
                    handle = parser.ParseExpression();
                    flags["handle"] = true;
                }
                else
                {
                    throw CreateError(parser.Peek(), $"Token inatteso '{parser.Peek().Lexeme}'.");
                }
            }
        }

        if (!flags["handle"] || !flags["newId"] || !flags["dateDesc"] || !flags["postings"])
            throw CreateError(parser.Previous(), "Mancano parametri obbligatori (handle, INTO, WITH, o posting {}).");

        var postingsArray = postings!.Entries
            .Select(e => new LedgerPosting(e.Key, e.Value))
            .ToImmutableArray();

        return new LedgerAddStmt(handle!, date!, description!, postingsArray, newIdVar!);
    }
    internal static Stmt ParseLedgerSubcommand(Parser parser, string expectedNamespace)
    {
        var subCommand = parser.Peek();
        return subCommand.Kind switch
        {
            TokenKind.Get => ParseLedgerGetStatement(parser, expectedNamespace),
            TokenKind.Register => ParseLedgerRegisterStatement(parser, expectedNamespace),
            TokenKind.Balance => ParseLedgerBalanceStatement(parser, expectedNamespace),
            TokenKind.Accounts => ParseLedgerAccountsStatement(parser, expectedNamespace),
            TokenKind.Validate => ParseLedgerValidateStatement(parser, expectedNamespace),
            TokenKind.Variance => ParseLedgerVarianceStatement(parser, expectedNamespace),
            TokenKind.Plot => ParseLedgerPlotStatement(parser, expectedNamespace),
            // Cursore (pattern DBF/ISAM): LEDGER GO <h> TOP|BOTTOM|<n> / LEDGER SKIP <h> [n]
            TokenKind.Go or TokenKind.Goto => ParseLedgerGoStatement(parser, expectedNamespace),
            TokenKind.Skip => ParseLedgerSkipStatement(parser, expectedNamespace),
            // Aggiungiamo i comandi che possono avere INTO
            TokenKind.Add => ParseLedgerAddStatement(parser, expectedNamespace),
            TokenKind.Edit => ParseLedgerEditStatement(parser, expectedNamespace),
            TokenKind.Open => ParseLedgerOpenStatement(parser, expectedNamespace),
            // Per i comandi puri, la logica è semplice.
            TokenKind.Delete => ParseLedgerDeleteStatement(parser, expectedNamespace),
            TokenKind.Save => ParseLedgerSaveStatement(parser, expectedNamespace),
            TokenKind.Close => ParseLedgerCloseStatement(parser, expectedNamespace),
            TokenKind.Pack => ParseLedgerPackStatement(parser, expectedNamespace),
            TokenKind.Budget => ParseLedgerBudgetStatement(parser, expectedNamespace),
            TokenKind.Import => ParseLedgerImportStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommand, $"Unrecognized LEDGER subcommand: '{subCommand.Lexeme}'.")
        };
    }



    /// <summary>
    /// Parsa un'istruzione `LEDGER EDIT` per sostituire una transazione esistente.
    /// Gestisce sia il comando legacy con clausole flessibili e `INTO`,
    /// sia il comando qualificato fuso (stesse clausole, senza parentesi).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerEditStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// LEDGER EDIT hndl ID 101 WITH DATETIME.TODAY(), "Nuova Descrizione" {
    ///     "Spese:Caffe": 5.00,
    ///     "Attivita:Contanti": -5.00
    /// } INTO nRet
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// LEDGER.EDIT hndl ID 101 WITH DATETIME.TODAY(), "Nuova Desc", {
    ///     "Spese:Varie": 10.00,
    ///     "Attivita:Banca": -10.00
    /// } INTO nRet
    /// </code>
    /// </example>
    public static Stmt ParseLedgerEditStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Edit, "Internal parser error: Expected 'EDIT'.");

        // --- Sintassi unificata (legacy e qualificata fusa): clausole flessibili ---
        Expr? handle = null;
        Expr? transactionId = null;
        Expr? date = null;
        Expr? description = null;
        DictionaryLiteralExpr? postings = null;
        Target? newIdVar = null;

        var flags = new Dictionary<string, bool>
        {
            { "handle", false }, { "id", false }, { "newId", false }, { "dateDesc", false }, { "postings", false }
        };

        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Id))
            {
                if (flags["id"]) throw CreateError(parser.Previous(), "ID transazione già specificato (ID).");
                transactionId = parser.ParseExpression();
                flags["id"] = true;
            }
            else if (parser.Match(TokenKind.Into))
            {
                if (flags["newId"]) throw CreateError(parser.Previous(), "L'ID di output è già stato specificato (INTO).");
                newIdVar = parser.ParseTarget();
                flags["newId"] = true;
            }
            else if (parser.Match(TokenKind.With))
            {
                if (flags["dateDesc"])
                    throw CreateError(parser.Previous(), "Data e descrizione sono già stati specificati (WITH).");
                date = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Attesa virgola per separare data e descrizione.");
                description = parser.ParseExpression();
                flags["dateDesc"] = true;
            }
            else if (parser.Check(TokenKind.LeftBrace))
            {
                if (flags["postings"]) throw CreateError(parser.Previous(), "I posting sono già stati specificati.");
                var expr = parser.ParseExpression();
                postings = expr as DictionaryLiteralExpr ??
                           throw CreateError(parser.Previous(),
                               "I postings devono essere specificati come Dizionario Literal { conto$:importo }.");
                flags["postings"] = true;
            }
            else
            {
                if (!flags["handle"])
                {
                    handle = parser.ParseExpression();
                    flags["handle"] = true;
                }
                else
                {
                    throw CreateError(parser.Peek(), $"Token inatteso '{parser.Peek().Lexeme}'.");
                }
            }
        }

        if (!flags["handle"] || !flags["id"] || !flags["newId"] || !flags["dateDesc"] || !flags["postings"])
            throw CreateError(parser.Previous(), "Mancano parametri obbligatori (handle, ID, INTO, WITH, o posting {}).");

        var postingsArray = postings!.Entries
            .Select(e => new LedgerPosting(e.Key, e.Value))
            .ToImmutableArray();

        return new LedgerEditStmt(handle!, transactionId!, date!, description!, postingsArray, newIdVar!);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER REGISTER`. Gestisce sia il comando legacy con `INTO`,
    /// sia il comando qualificato fuso (stesse clausole, senza parentesi).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerRegisterStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// DIM reg() AS LEDGERENTRY
    /// LEDGER REGISTER hndl WHERE "account:""Spese""" INTO reg()
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// DIM reg() AS LEDGERENTRY
    /// LEDGER.REGISTER hndl WHERE "account:""Spese""" INTO reg()
    /// </code>
    /// </example>
    public static Stmt ParseLedgerRegisterStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Register, "Internal parser error: Expected 'REGISTER'.");

        // Sintassi unificata (legacy e qualificata fusa): REGISTER handle [WHERE query] INTO destArray
        Expr handle = parser.ParseExpression();
        Expr? query = null;
        if (parser.Match(TokenKind.Where))
        {
            query = parser.ParseExpression();
        }
        parser.Consume(TokenKind.Into, "Expected 'INTO' in LEDGER REGISTER.");
        Target destinationArray = parser.ParseTarget();

        return new LedgerRegisterStmt(handle, destinationArray, query);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER BALANCE`. Gestisce sia il comando legacy con `INTO`,
    /// sia il comando qualificato fuso (stesse clausole, senza parentesi).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerBalanceStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// DIM saldi() AS SaldoConto
    /// LEDGER BALANCE hndl WHERE "Assets" INTO saldi()
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// DIM saldi() AS SaldoConto
    /// LEDGER.BALANCE hndl WHERE "Assets" INTO saldi()
    /// </code>
    /// </example>
    public static Stmt ParseLedgerBalanceStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Balance, "Internal parser error: Expected 'BALANCE'.");

        // Sintassi unificata (legacy e qualificata fusa): BALANCE handle [WHERE query] INTO destArray
        Expr handle = parser.ParseExpression();
        Expr? query = null;
        if (parser.Match(TokenKind.Where))
        {
            query = parser.ParseExpression();
        }
        parser.Consume(TokenKind.Into, "Expected 'INTO' in LEDGER BALANCE.");
        Target destinationArray = parser.ParseTarget();

        return new LedgerBalanceStmt(handle, destinationArray, query);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER GET`. Gestisce sia il comando legacy con `INTO`,
    /// sia il comando qualificato fuso (stesse clausole, senza parentesi).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerGetStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// DIM entries() AS LEDGERENTRY
    /// LEDGER GET hndl ID transId INTO entries()
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// DIM entries() AS LEDGERENTRY
    /// LEDGER.GET hndl ID transId INTO entries()
    /// </code>
    /// </example>
    public static LedgerGetStmt ParseLedgerGetStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Get, "Internal parser error: Expected 'GET'.");

        // Sintassi unificata (legacy e qualificata fusa): GET handle [ID transactionId] INTO destArray
        // Senza ID legge la transazione corrente del cursore (pattern DBF GET del record corrente).
        Expr handle = parser.ParseExpression();
        Expr? transactionId = null;
        if (parser.Match(TokenKind.Id))
        {
            transactionId = parser.ParseExpression();
        }
        parser.Consume(TokenKind.Into, "Expected 'INTO' after transaction ID.");
        Target destinationArray = parser.ParseTarget();

        return new LedgerGetStmt(handle, transactionId, destinationArray);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER DELETE`, che è un comando puro in entrambe le sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerDeleteStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// LEDGER DELETE hndl ID transId
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// LEDGER.DELETE hndl ID transId
    /// </code>
    /// </example>
    public static LedgerDeleteStmt ParseLedgerDeleteStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Delete, "Internal parser error: Expected 'DELETE'.");

        // Sintassi unificata (legacy e qualificata fusa): DELETE handle ID transactionId
        Expr handle = parser.ParseExpression();
        parser.Consume(TokenKind.Id, "Expected 'ID' keyword in LEDGER DELETE.");
        Expr transactionId = parser.ParseExpression();

        return new LedgerDeleteStmt(handle, transactionId);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER SAVE`, che è un comando puro in entrambe le sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerSaveStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// LEDGER SAVE hndl
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// LEDGER.SAVE hndl
    /// </code>
    /// </example>
    public static Stmt ParseLedgerSaveStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Save, "Internal parser error: Expected 'SAVE'.");

        // Sintassi unificata (legacy e qualificata fusa): SAVE handle
        Expr handle = parser.ParseExpression();

        return new LedgerSaveStmt(handle);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER CLOSE`, che è un comando puro in entrambe le sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerCloseStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// LEDGER CLOSE hndl
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// LEDGER.CLOSE hndl
    /// </code>
    /// </example>
    public static LedgerCloseStmt ParseLedgerCloseStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Close, "Internal parser error: Expected 'CLOSE'.");

        // Sintassi unificata (legacy e qualificata fusa): CLOSE handle
        Expr handle = parser.ParseExpression();

        return new LedgerCloseStmt(handle);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER PACK`, che è un comando puro in entrambe le sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerPackStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// LEDGER PACK hndl
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// LEDGER.PACK hndl
    /// </code>
    /// </example>
    public static LedgerPackStmt ParseLedgerPackStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Pack, "Internal parser error: Expected 'PACK'.");

        // Sintassi unificata (legacy e qualificata fusa): PACK handle
        Expr handle = parser.ParseExpression();

        return new LedgerPackStmt(handle);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER ACCOUNTS`. Gestisce sia il comando legacy con `INTO`,
    /// sia il comando qualificato fuso (stesse clausole, senza parentesi).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerAccountsStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// DIM accounts() AS STRING
    /// LEDGER ACCOUNTS hndl INTO accounts()
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// DIM accounts() AS STRING
    /// LEDGER.ACCOUNTS hndl INTO accounts()
    /// </code>
    /// </example>
    public static Stmt ParseLedgerAccountsStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Accounts, "Internal parser error: Expected 'ACCOUNTS'.");

        // Sintassi unificata (legacy e qualificata fusa): ACCOUNTS handle [rootAccount] INTO destArray
        Expr handle = parser.ParseExpression();
        Expr? rootAccount = null;
        if (!parser.Check(TokenKind.Into))
        {
            rootAccount = parser.ParseOptionalExpression();
        }
        parser.Consume(TokenKind.Into, "Expected 'INTO' in LEDGER ACCOUNTS.");
        Target destinationArray = parser.ParseTarget();

        return new LedgerAccountsStmt(handle, rootAccount, destinationArray);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER VALIDATE`. Gestisce sia il comando legacy con `INTO`,
    /// sia il comando qualificato fuso (stesse clausole, senza parentesi).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("LEDGER") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="LedgerValidateStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy (Comando):
    /// <code>
    /// DIM isValid AS BOOLEAN
    /// LEDGER VALIDATE hndl INTO isValid
    /// </code>
    /// Sintassi Qualificata Fusa (Comando):
    /// <code>
    /// DIM isValid AS BOOLEAN
    /// LEDGER.VALIDATE hndl INTO isValid
    /// </code>
    /// </example>
    public static Stmt ParseLedgerValidateStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Validate, "Internal parser error: Expected 'VALIDATE'.");

        // Sintassi unificata (legacy e qualificata fusa): VALIDATE handle INTO destinationVariable
        Expr handle = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected 'INTO' in LEDGER VALIDATE.");
        Target destinationVariable = parser.ParseTarget();

        return new LedgerValidateStmt(handle, destinationVariable);
    }

    public static LedgerBudgetStmt ParseLedgerBudgetStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Budget, "Internal parser error: Expected 'BUDGET'.");

        // Sintassi: BUDGET handle, account, amount, period — argomenti separati da virgola
        // (come la funzione LEDGER.BUDGET; il parser fonde i string literal adiacenti).
        var handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER BUDGET.");
        var account = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER BUDGET.");
        var amount = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER BUDGET.");
        var period = parser.ParseExpression();
        return new LedgerBudgetStmt(handle, account, amount, period);
    }

    public static Stmt ParseLedgerVarianceStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Variance, "Internal parser error: Expected 'VARIANCE'.");

        // Sintassi: VARIANCE handle, account, period INTO destination — gli argomenti sono
        // SEPARATI DA VIRGOLA (stesso schema della funzione LEDGER.VARIANCE(h, acc, period)):
        // il parser fonde i string literal adiacenti ("a" "b" → "ab", C-style), quindi
        // senza virgola due stringhe consecutive verrebbero unite in un unico argomento.
        Expr handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER VARIANCE.");
        Expr account = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER VARIANCE.");
        Expr period = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected 'INTO' in LEDGER VARIANCE.");
        Target destination = parser.ParseTarget();

        return new LedgerVarianceStmt(handle, account, period, destination);
    }

    public static Stmt ParseLedgerPlotStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Plot, "Internal parser error: Expected 'PLOT'.");

        // Sintassi: PLOT handle, query, chartType INTO destination (argomenti separati da virgola).
        Expr handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER PLOT.");
        Expr query = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER PLOT.");
        Expr chartType = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected 'INTO' in LEDGER PLOT.");
        Target destination = parser.ParseTarget();

        return new LedgerPlotStmt(handle, query, chartType, destination);
    }

    public static Stmt ParseLedgerImportStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Import, "Internal parser error: Expected 'IMPORT'.");

        // Sintassi: IMPORT handle, csvPath, mapping [INTO destination] (argomenti separati da virgola).
        Expr handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER IMPORT.");
        Expr csvPath = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Attesa ',' tra gli argomenti di LEDGER IMPORT.");
        Expr mapping = parser.ParseExpression();
        Target? destination = null;
        if (parser.Check(TokenKind.Into))
        {
            parser.Advance();
            destination = parser.ParseTarget();
        }

        return destination is null
            ? new LedgerImportStmt(handle, csvPath, mapping, null)
            : new LedgerImportStmt(handle, csvPath, mapping, destination);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER GO` per posizionare il cursore (pattern DBF/ISAM).
    /// Sintassi: `LEDGER GO handle TOP | LEDGER GO handle BOTTOM | LEDGER GO handle <transactionId>`
    /// </summary>
    public static LedgerGoStmt ParseLedgerGoStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Go, TokenKind.Goto);

        Expr handle = parser.ParseExpression();

        Expr destination;
        if (parser.Check(TokenKind.Top) || parser.Check(TokenKind.Bottom))
        {
            var kw = parser.Advance();
            destination = new StringExpr(kw.Lexeme, kw);
        }
        else
        {
            destination = parser.ParseExpression();
        }

        return new LedgerGoStmt(handle, destination);
    }

    /// <summary>
    /// Parsa un'istruzione `LEDGER SKIP` per muovere il cursore (pattern DBF/ISAM).
    /// Sintassi: `LEDGER SKIP handle [count]` — count di default 1, negativo per andare indietro.
    /// </summary>
    public static LedgerSkipStmt ParseLedgerSkipStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Skip, "Internal parser error: Expected 'SKIP'.");

        Expr handle = parser.ParseExpression();

        Expr? count = null;
        if (!parser.IsAtEndOfStatement())
        {
            count = parser.ParseExpression();
        }

        return new LedgerSkipStmt(handle, count);
    }
}