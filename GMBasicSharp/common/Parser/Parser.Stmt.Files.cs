// FILE: Interpreter.Parser.Stmt.Files.cs
/*
------------------------------------------------------------------------------------------------------------------------
        GUIDA DI RIFERIMENTO ALLA DOPPIA SINTASSI (LEGACY E QUALIFICATA FUSA)
------------------------------------------------------------------------------------------------------------------------
I metodi di parsing in questo file supportano due stili di sintassi per ogni comando,
garantendo la retrocompatibilità e introducendo al contempo un'API moderna e non ambigua.

La sintassi "Legacy" o "Non Qualificata" è quella tradizionale del BASIC, che richiede l'import del
namespace (`IMPORT IO`) per essere utilizzata.

La sintassi "Moderna" o "Qualificata fusa" usa `IO.VERBOFUSO` SENZA parentesi (direttiva AGENTS.md §2.2):
le clausole e gli argomenti sono le STESSE della forma legacy multi-parola.

--- Esempi di Doppia Sintassi ---

1. OPEN
   Legacy:  OPEN "dati.txt" FOR RANDOM AS #1 LEN = 128
   Moderna: IO.OPEN "dati.txt" FOR RANDOM AS #1 LEN = 128

2. CLOSE
   Legacy:  CLOSE #1, #2
   Moderna: IO.CLOSE 1, 2

3. KILL / ERASE
   Legacy:  KILL "temp.dat"
   Moderna: IO.KILL "temp.dat"

4. RENAME
   Legacy:  RENAME "vecchio.txt" AS "nuovo.txt"
   Moderna: IO.RENAME "vecchio.txt" AS "nuovo.txt"

5. SEEK
   Legacy:  SEEK #1, 1024
   Moderna: IO.SEEK 1, 1024

6. GET
   Legacy:  GET #1, 5, mioRecord
   Moderna: IO.GET 1, 5, mioRecord

7. PUT
   Legacy:  PUT #1, 5, mioRecord
   Moderna: IO.PUT 1, 5, mioRecord

8. PRINT #
   Legacy:  PRINT #1, "Valore:"; valore;
   Moderna: IO.PRINT 1, "Valore:", valore

9. WRITE #
   Legacy:  WRITE #1, A, B, C$
   Moderna: IO.WRITE 1, A, B, C$

10. INPUT #
    Legacy:  INPUT #1, A, B, C$
    Moderna: IO.INPUT 1, A, B, C$

11. LINE INPUT #
    Legacy:  LINE INPUT #1, unaRiga$
    Moderna: IO.LINEINPUT 1, unaRiga$

12. MKDIR
    Legacy:  MKDIR "nuova_cartella"
    Moderna: IO.MKDIR "nuova_cartella"

13. RMDIR
    Legacy:  RMDIR "vecchia_cartella", -1 ' true
    Moderna: IO.RMDIR "vecchia_cartella", TRUE

14. CHDIR
    Legacy:  CHDIR "C:\progetti"
    Moderna: IO.CHDIR "C:\progetti"

15. COPY
    Legacy:  COPY "a.txt" TO "b.txt"
    Moderna: IO.COPY "a.txt" TO "b.txt"

16. MOVE
    Legacy:  MOVE "a.txt" TO "b.txt"
    Moderna: IO.MOVE "a.txt" TO "b.txt"

17. XCOPY
    Legacy:  XCOPY "src" TO "dest", -1, -1 ' recursive, overwrite
    Moderna: IO.XCOPY "src" TO "dest", TRUE, TRUE

NOTA: Le funzioni che restituiscono un valore (EOF, LOF, FREEFILE, FINPUT, CWD, FILEEXISTS,
DIREXISTS, GETFOLDERPATH, NORMALIZEPATH, FOLDERSIZE, APPPATH, PROJECTPATH, PROJECTFOLDER,
PARENTDIR, DIR, MEMORYMAP, MMAPPEEK, MMAPREADBYTES, ...) restano con parentesi: IO.EOF(x).
------------------------------------------------------------------------------------------------------------------------
*/
// ReSharper disable InvalidXmlDocComment

using System.Collections.Immutable;

// ReSharper disable UnusedParameter.Local

namespace Basic;

/// <summary>
///     Contiene i metodi per il parsing delle singole istruzioni, definizioni e clausole del linguaggio BASIC.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Parsa un'istruzione OPEN per I/O standard (non-database).
    /// Gestisce le modalità INPUT, OUTPUT, APPEND, RANDOM, BINARY e le clausole standard
    /// come ACCESS, LOCK, LEN e ENCODING.
    /// La logica per ISAM e DBF è stata spostata in parser dedicati.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token del qualificatore (es. 'IO'), se presente.</param>
    /// <returns>Un nodo <see cref="OpenStmt"/> configurato per l'I/O standard.</returns>
    /// <example>
    /// <b>Sintassi Standard I/O Gestita:</b>
    /// <code>
    /// OPEN "DATI.TXT" FOR INPUT AS #1
    /// OPEN "output.log" FOR APPEND ACCESS WRITE AS #2
    /// OPEN "ARCHIVIO.DAT" FOR RANDOM AS #3 LEN = 128
    /// IO.OPEN "binary.bin" FOR BINARY AS #4
    /// </code>
    /// </example>
    public static Stmt ParseIoOpenStatement(Parser parser, string expectedNamespace)
    {
        var openToken = parser.Consume(TokenKind.Open, "Internal parser error: Expected 'OPEN'.");

        Expr fileName;
        FileOpenMode mode;
        Expr fileNumber;
        Expr? recordLength = null;
        var accessMode = FileAccessMode.Default;
        var lockMode = FileLockMode.Default;
        DataType? fileContentType = null;
        Expr? encodingExpr = null;

        // --- Sintassi unificata (legacy e qualificata fusa): OPEN "file" FOR mode ... AS #handle ---
        fileName = parser.ParseExpression();

        parser.Consume(TokenKind.For, "Expected 'FOR' in OPEN statement.");

        if (parser.Match(TokenKind.Input) || parser.Match(TokenKind.Read)) mode = FileOpenMode.Input;
        else if (parser.Match(TokenKind.Output) || parser.Match(TokenKind.Write)) mode = FileOpenMode.Output;
        else if (parser.Match(TokenKind.Append)) mode = FileOpenMode.Append;
        else if (parser.Match(TokenKind.Random)) mode = FileOpenMode.Random;
        else if (parser.Match(TokenKind.Binary)) mode = FileOpenMode.Binary;
        else
        {
            // Se non è nessuna delle modalità standard, è un errore, perché ISAM/DBF ora hanno i loro parser.
            throw CreateError(parser.Peek(),
                "Expected a file mode (INPUT, OUTPUT, APPEND, RANDOM, BINARY) after FOR.");
        }

        // Parsa le clausole opzionali standard
        if (parser.Match(TokenKind.Encoding))
        {
            encodingExpr = parser.ParseExpression();
        }

        if (parser.Match(TokenKind.Access))
        {
            if (parser.Match(TokenKind.ReadWrite)) accessMode = FileAccessMode.ReadWrite;
            else if (parser.Match(TokenKind.Read)) accessMode = FileAccessMode.Read;
            else if (parser.Match(TokenKind.Write)) accessMode = FileAccessMode.Write;
            else throw CreateError(parser.Peek(), "Expected READ, WRITE, or READ WRITE after ACCESS.");
        }

        if (parser.Match(TokenKind.Lock))
        {
            if (parser.Match(TokenKind.Shared)) lockMode = FileLockMode.Shared;
            else if (parser.Match(TokenKind.Read)) lockMode = FileLockMode.LockRead;
            else if (parser.Match(TokenKind.Write)) lockMode = FileLockMode.LockWrite;
            else if (parser.Match(TokenKind.ReadWrite)) lockMode = FileLockMode.LockReadWrite;
            else throw CreateError(parser.Peek(), "Expected SHARED, READ, WRITE, or READ WRITE after LOCK.");
        }

        parser.Consume(TokenKind.As, "Expected 'AS' in OPEN statement.");
        parser.Match(TokenKind.Hash);
        fileNumber = parser.ParseExpression();

        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Type))
            {
                if (parser.Match(TokenKind.Json))
                {
                    fileContentType = new JsonType();
                }
                else
                {
                    throw CreateError(parser.Peek(), "Expected 'JSON' after 'TYPE' in OPEN statement.");
                }
            }
            else if (parser.Match(TokenKind.Len))
            {
                parser.Consume(TokenKind.Equal, "Expected '=' after LEN.");
                recordLength = parser.ParseExpression();
            }
            else
            {
                break;
            }
        }

        // DatabaseLockingMode e isUnsafe non sono applicabili all'I/O standard
        return new OpenStmt(fileName, mode, DatabaseLockingMode.Default, lockMode, accessMode, fileNumber,
            fileContentType,
            recordLength, null, ImmutableArray<Expr>.Empty, encodingExpr, false, openToken);
    }

    /// <summary>
    /// Parsa un'istruzione CLOSE per il namespace IO, gestendo la doppia sintassi.
    /// Questo comando chiude uno o più file aperti in modalità I/O standard (Input, Output, Random, etc.).
    /// </summary>
    /// <param name="expectedNamespace">Il namespace ("IO") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'IO' se la chiamata era qualificata.</param>
    /// <returns>Un nodo AST di tipo <see cref="CloseStmt"/>.</returns>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ' Legacy
    /// CLOSE #1, #2
    /// CLOSE ' Chiude tutti i file aperti
    /// 
    /// ' Moderna (Qualificata fusa)
    /// IO.CLOSE 1, 2
    /// IO.CLOSE ' Chiude tutti i file aperti
    /// </code>
    /// </example>
    public static Stmt ParseIoCloseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Close, "Internal parser error: Expected 'CLOSE'.");
        var fileNumbers = ImmutableArray.CreateBuilder<Expr>();

        // --- Sintassi unificata (legacy e qualificata fusa): CLOSE [#]handle [, ...] ---
        if (parser.IsAtEndOfStatement()) // Supporta CLOSE senza argomenti
            return new CloseStmt(fileNumbers.ToImmutable(), commandToken);

        do
        {
            parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy CLOSE #1, qualificata IO.CLOSE 1
            fileNumbers.Add(parser.ParseExpression());
        } while (parser.Match(TokenKind.Comma));

        return new CloseStmt(fileNumbers.ToImmutable(), commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione KILL o ERASE per cancellare un file, gestendo la doppia sintassi.
    /// </summary>
    public static Stmt ParseKillStatement(Parser parser, string expectedNamespace)
    {
        // Il dispatcher qualificato non consuma il token del comando.
        // Lo consumiamo qui, sia che sia qualificato o legacy.
        var killToken = parser.Consume(TokenKind.Kill, TokenKind.Erase);

        // La logica di parsing degli argomenti è identica per entrambe le sintassi.
        var fileName = parser.ParseExpression();

        return new KillStmt(fileName, killToken);
    }

    /// <summary>
    /// Parsa un'istruzione RENAME, gestendo la doppia sintassi.
    /// La sintassi, sia legacy che qualificata, è 'RENAME <vecchio> AS <nuovo>'.
    /// </summary>
    public static Stmt ParseRenameStatement(Parser parser, string expectedNamespace)
    {
        var renameToken = parser.Consume(TokenKind.Rename, "Internal parser error: Expected 'RENAME'.");

        // La logica è ora unificata per entrambe le sintassi.
        var oldFileName = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' after old filename in RENAME statement.");
        var newFileName = parser.ParseExpression();

        return new RenameStmt(oldFileName, newFileName, renameToken);
    }

    /// <summary>
    /// Parsa la variante di SEEK specifica del namespace IO, che non ha argomenti opzionali.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    public static SeekStmt ParseIoSeekStatement(Parser parser, string expectedNamespace)
    {
        var seekToken = parser.Consume(TokenKind.Seek, "Internal parser error: Expected 'SEEK'.");

        // --- Sintassi unificata (legacy e qualificata fusa): SEEK [#]handle, position ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy SEEK #1, qualificata IO.SEEK 1
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number.");
        var position = parser.ParseExpression();

        return new SeekStmt(fileNumber, position, seekToken);
    }

    /// <summary>Comando IO FREAD (B): <c>FREAD handle [, count] INTO target</c> — legge e
    /// SCRIVE nel buffer. La forma FUNZIONE <c>FREAD(handle, count)</c> ritorna i dati e NON
    /// scrive (AGENTS §1/§3: le funzioni non scrivono mai). Desugara in un'assegnazione, come
    /// gli altri comandi con `INTO` (pattern di AI ASKCHAIN / AI VERIFY).</summary>
    public static Stmt ParseIoFReadIntoStatement(Parser parser, string expectedNamespace)
    {
        var readToken = parser.Consume(TokenKind.FRead, "Internal parser error: Expected 'FREAD'.");
        parser.Match(TokenKind.Hash); // '#' opzionale: legacy chiuso FREAD #1 ...
        Expr handle = parser.ParseExpression();
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(handle);
        if (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        parser.Consume(TokenKind.Into, "Expected 'INTO' after FREAD handle (forma comando).");
        var target = parser.ParseTarget();
        var nsTok = new Token(TokenKind.Identifier, expectedNamespace, null, readToken.Position);
        var nameTok = new Token(TokenKind.FRead, "FREAD", null, readToken.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, nameTok, args.ToImmutable(), new StringType());
        return new LetStmt([new Assignment(target, call)], null);
    }

    /// <summary>
    /// Parsa un'istruzione GET per il namespace IO, che opera su file Random o Binary.
    /// Gestisce sia la sintassi legacy (`GET #...`) che quella moderna qualificata (`IO.GET(...)`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'IO' se la chiamata era qualificata.</param>
    /// <returns>Un nodo AST <see cref="GetFileStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi IO (Random/Binary):</b>
    /// <code>
    /// ' Legacy
    /// GET #2, 10, unaVariabile
    /// GET #2, , unaVariabile ' Record successivo
    /// 
    /// ' Moderna (Qualificata fusa)
    /// IO.GET 2, 10, unaVariabile
    /// </code>
    /// </example>
    private static GetFileStmt ParseIoGetStatement(Parser parser, string expectedNamespace)
    {
        var getToken = parser.Consume(TokenKind.Get, "Internal parser error: Expected 'GET'.");

        // --- Sintassi unificata (legacy e qualificata fusa): GET [#]handle, [recordNumber], destination ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy GET #1, qualificata IO.GET 1
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number.");

        // Il numero di record è opzionale
        Expr? recordNumber = null;
        if (!parser.Check(TokenKind.Comma))
        {
            recordNumber = parser.ParseExpression();
        }

        parser.Consume(TokenKind.Comma, "Expected ',' after record number.");
        var destination = parser.ParseTarget();

        return new GetFileStmt(fileNumber, recordNumber, destination, getToken);
    }

    /// <summary>
    /// Parsa un'istruzione PUT per il namespace IO, che opera su file Random o Binary.
    /// Gestisce sia la sintassi legacy (`PUT #...`) che quella moderna qualificata (`IO.PUT(...)`).
    /// </summary>
    /// <param name="expectedNamespace">Il namespace ("IO") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'IO' se la chiamata era qualificata.</param>
    /// <returns>Un nodo AST <see cref="PutFileStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi IO (Random/Binary):</b>
    /// <code>
    /// ' Legacy
    /// PUT #2, 10, unaVariabile
    /// PUT #2, , unaVariabile ' Record successivo
    /// 
    /// ' Moderna (Qualificata fusa)
    /// IO.PUT 2, 10, unaVariabile
    /// </code>
    /// </example>
    public static PutFileStmt ParseIoPutStatement(Parser parser, string expectedNamespace)
    {
        var putToken = parser.Consume(TokenKind.Put, "Internal parser error: Expected 'PUT'.");

        // --- Sintassi unificata (legacy e qualificata fusa): PUT [#]handle, [recordNumber], source ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy PUT #1, qualificata IO.PUT 1
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number.");

        // Il numero di record è opzionale
        Expr? recordNumber = null;
        if (!parser.Check(TokenKind.Comma))
        {
            recordNumber = parser.ParseExpression();
        }

        parser.Consume(TokenKind.Comma, "Expected ',' after record number.");
        var source = parser.ParseExpression();

        // isUnsafe è sempre false per IO
        return new PutFileStmt(fileNumber, recordNumber, source, false, putToken);
    }

    /// <summary>
    /// Parsa un'istruzione PRINT #, gestendo la doppia sintassi.
    /// </summary>
    private static PrintFileStmt ParsePrintFileStatement(Parser parser, string expectedNamespace)
    {
        var printToken = parser.Consume(TokenKind.Print, "Internal parser error: Expected 'PRINT'.");

        // --- Sintassi unificata (legacy e qualificata fusa): PRINT [#]handle, arg1; arg2 ... ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy PRINT #1, qualificata IO.PRINT 1
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number in PRINT #.");
        var arguments = parser.ParsePrintArguments();

        return new PrintFileStmt(fileNumber, arguments, printToken);
    }

    /// <summary>
    /// Parsa un'istruzione WRITE #, gestendo la doppia sintassi.
    /// </summary>
    public static Stmt ParseWriteFileStatement(Parser parser, string expectedNamespace)
    {
        var writeToken = parser.Consume(TokenKind.Write, "Internal parser error: Expected 'WRITE'.");

        // --- Sintassi unificata (legacy e qualificata fusa): WRITE [#]handle, arg1, arg2 ... ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy WRITE #1, qualificata IO.WRITE 1
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number in WRITE #.");
        var arguments = parser.ParseExpressionList(TokenKind.Newline);

        return new WriteFileStmt(fileNumber, arguments, writeToken);
    }

    /// <summary>
    /// Parsa un'istruzione INPUT #, gestendo la doppia sintassi.
    /// </summary>
    private static InputFileStmt ParseInputFileStatement(Parser parser, string expectedNamespace)
    {
        var inputToken = parser.Consume(TokenKind.Input, "Internal parser error: Expected 'INPUT'.");

        // --- Sintassi unificata (legacy e qualificata fusa): INPUT [#]handle, var1, var2 ... ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy INPUT #1, qualificata IO.INPUT 1
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number.");
        var variables = parser.ParseTargetList();

        return new InputFileStmt(fileNumber, variables, inputToken);
    }

    /// <summary>
    /// Parsa un'istruzione LINE INPUT #, gestendo la doppia sintassi.
    /// Questa versione corretta utilizza ParseTarget per supportare l'assegnazione
    /// a qualsiasi L-value valido.
    /// </summary>
    private static LineInputFileStmt ParseLineInputFileStatement(Parser parser, string expectedNamespace)
    {
        var lineInputToken = parser.Consume(TokenKind.LineInput, "Internal parser error: Expected 'LINE INPUT' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): LINE INPUT [#]handle, variable ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy LINE INPUT #1, qualificata IO.LINEINPUT 1
        var fileNumber = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after file number.");
        var variable = parser.ParseTarget();

        return new LineInputFileStmt(fileNumber, variable, lineInputToken);
    }

/// <summary>
    /// Parsa un'istruzione LOCK per bloccare un file o un record, gestendo la doppia sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'IO' se la chiamata era qualificata.</param>
    /// <returns>Un nodo AST <see cref="LockStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi Gestite:</b>
    /// <code>
    /// ' Legacy (file intero)
    /// LOCK #1
    /// 
    /// ' Legacy (singolo record)
    /// LOCK #1, 10
    /// 
    /// ' Moderna (file intero)
    /// IO.LOCK 1
    /// 
    /// ' Moderna (singolo record)
    /// IO.LOCK 1, 10
    /// </code>
    /// </example>
    public static Stmt ParseLockStatement(Parser parser, string expectedNamespace)
    {
        var lockToken = parser.Consume(TokenKind.Lock, "Internal parser error: Expected 'LOCK'.");

        // --- Sintassi unificata (legacy e qualificata fusa): LOCK [#]handle [, recordNumber] ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy LOCK #1, qualificata IO.LOCK 1
        var fileNumber = parser.ParseExpression();
        Expr? recordNumber = null;
        if (parser.Match(TokenKind.Comma))
        {
            recordNumber = parser.ParseExpression();
        }

        return new LockStmt(fileNumber, recordNumber, lockToken);
    }

    /// <summary>
    /// Parsa un'istruzione UNLOCK per sbloccare un file o un record, gestendo la doppia sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'IO' se la chiamata era qualificata.</param>
    /// <returns>Un nodo AST <see cref="UnlockStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi Gestite:</b>
    /// <code>
    /// ' Legacy (file intero)
    /// UNLOCK #1
    /// 
    /// ' Legacy (singolo record)
    /// UNLOCK #1, 10
    /// 
    /// ' Moderna (file intero)
    /// IO.UNLOCK 1
    /// 
    /// ' Moderna (singolo record)
    /// IO.UNLOCK 1, 10
    /// </code>
    /// </example>
    public static Stmt ParseUnlockStatement(Parser parser, string expectedNamespace)
    {
        var unlockToken = parser.Consume(TokenKind.Unlock, "Internal parser error: Expected 'UNLOCK'.");

        // --- Sintassi unificata (legacy e qualificata fusa): UNLOCK [#]handle [, recordNumber] ---
        parser.Match(TokenKind.Hash); // Il '#' è opzionale: legacy UNLOCK #1, qualificata IO.UNLOCK 1
        var fileNumber = parser.ParseExpression();
        Expr? recordNumber = null;
        if (parser.Match(TokenKind.Comma))
        {
            recordNumber = parser.ParseExpression();
        }

        return new UnlockStmt(fileNumber, recordNumber, unlockToken);
    }
    
    /// <summary>
    /// Parsa un'istruzione `WATCH` per configurare un monitoraggio del file system.
    /// Gestisce sia la sintassi legacy (`WATCH path, mask, callback`) che quella moderna qualificata
    /// fusa (`IO.WATCH path, mask, callback`), senza parentesi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IoWatchStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// WATCH "C:\Dati", FILE_CREATED OR FILE_CHANGED, MyCallback
    /// </code>
    /// Sintassi Moderna/Qualificata fusa:
    /// <code>
    /// IO.WATCH "C:\Dati", 1, MyCallback
    /// </code>
    /// </example>
    public static Stmt ParseIoWatchStatement(Parser parser, string expectedNamespace)
    {
        var watchToken = parser.Consume(TokenKind.Watch, "Internal parser error: Expected 'WATCH'.");

        // --- Sintassi unificata (legacy e qualificata fusa): WATCH path, mask, callback ---
        var path = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after path expression.");

        var eventsMask = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after events mask.");

        // Il callback è un identificatore (nome di subroutine), come nella forma legacy.
        var callback = parser.Consume(TokenKind.Identifier, "Expected callback subroutine name.");

        return new IoWatchStmt(path, eventsMask, callback, watchToken);
    }

    /// <summary>
    /// Parsa un'istruzione `UNWATCH` per rimuovere un monitoraggio del file system.
    /// Gestisce sia la sintassi legacy (`UNWATCH path`) che quella moderna qualificata
    /// fusa (`IO.UNWATCH path`), senza parentesi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="IoUnwatchStmt"/>.</returns>
    /// <example>
    /// Sintassi Legacy:
    /// <code>
    /// UNWATCH "C:\Dati"
    /// </code>
    /// Sintassi Moderna/Qualificata fusa:
    /// <code>
    /// IO.UNWATCH "C:\Dati"
    /// </code>
    /// </example>
    public static Stmt ParseIoUnwatchStatement(Parser parser, string expectedNamespace)
    {
        var unwatchToken = parser.Consume(TokenKind.Unwatch, "Internal parser error: Expected 'UNWATCH'.");

        // --- Sintassi unificata (legacy e qualificata fusa): UNWATCH path ---
        var path = parser.ParseExpression();

        return new IoUnwatchStmt(path, unwatchToken);
    }
    
    /// <summary>
    /// Parsa un'istruzione MKDIR.
    /// La sintassi unificata è 'MKDIR <percorso>'.
    /// </summary>
    /// <example>
    /// Legacy:  MKDIR "nuova_cartella"
    /// Moderna: IO.MKDIR "nuova_cartella"
    /// </example>
    public static Stmt ParseMkDirStatement(Parser parser, string expectedNamespace)
    {
        var token = parser.Consume(TokenKind.MkDir, "Internal parser error: Expected 'MKDIR'.");
        var path = parser.ParseExpression();
        return new MkDirStmt(path, token);
    }

    /// <summary>
    /// Parsa un'istruzione RMDIR, gestendo un flag ricorsivo opzionale.
    /// La sintassi unificata è 'RMDIR <percorso> [, ricorsivo]'.
    /// </summary>
    public static Stmt ParseRmDirStatement(Parser parser, string expectedNamespace)
    {
        var token = parser.Consume(TokenKind.RmDir, "Internal parser error: Expected 'RMDIR'.");
        Expr path;
        Expr? recursiveExpr = null;

        // IO.RMDIR(cWork, TRUE) and RMDIR (cWork, TRUE) — avoid TupleLiteralExpr from ParseExpression.
        if (parser.Match(TokenKind.LeftParen))
        {
            path = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma))
            {
                recursiveExpr = parser.ParseExpression();
            }

            parser.Consume(TokenKind.RightParen, "Expected ')' after RMDIR arguments.");
        }
        else
        {
            path = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma))
            {
                recursiveExpr = parser.ParseExpression();
            }
        }

        return new RmDirStmt(path, recursiveExpr, token);
    }

    /// <summary>
    /// Parsa un'istruzione CHDIR.
    /// La sintassi unificata è 'CHDIR <percorso>'.
    /// </summary>
    public static Stmt ParseChDirStatement(Parser parser, string expectedNamespace)
    {
        var token = parser.Consume(TokenKind.ChDir, "Internal parser error: Expected 'CHDIR'.");
        var path = parser.ParseExpression();
        return new ChDirStmt(path, token);
    }

    /// <summary>
    /// Parsa la variante di COPY specifica del namespace IO, gestendo la doppia sintassi.
    /// La sintassi unificata è 'COPY <sorgente> TO <destinazione>'.
    /// </summary>
    public static Stmt ParseIoCopyStatement(Parser parser, string expectedNamespace)
    {
        var copyToken = parser.Consume(TokenKind.Copy, "Internal parser error: Expected 'COPY'.");

        var source = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' in COPY statement.");
        var destination = parser.ParseExpression();

        return new CopyStmt(source, destination, copyToken);
    }

    /// <summary>
    /// Parsa un'istruzione MOVE, gestendo la doppia sintassi.
    /// La sintassi unificata è 'MOVE <sorgente> TO <destinazione>'.
    /// </summary>
    public static Stmt ParseMoveStatement(Parser parser, string expectedNamespace)
    {
        var token = parser.Consume(TokenKind.Move, "Internal parser error: Expected 'MOVE'.");

        var source = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' in MOVE statement.");
        var destination = parser.ParseExpression();

        return new MoveStmt(source, destination, token);
    }

    /// <summary>
    /// Parsa un'istruzione XCOPY, gestendo la doppia sintassi e i flag opzionali.
    /// La sintassi unificata è 'XCOPY <sorgente> TO <destinazione> [, ricorsivo] [, sovrascrivi]'.
    /// </summary>
    public static Stmt ParseXCopyStatement(Parser parser, string expectedNamespace)
    {
        var token = parser.Consume(TokenKind.XCopy, "Internal parser error: Expected 'XCOPY'.");

        var source = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' in XCOPY statement.");
        var destination = parser.ParseExpression();

        Expr? recursiveExpr = null;
        Expr? overwriteExpr = null;

        if (!parser.Match(TokenKind.Comma))
            return new XCopyStmt(source, destination, recursiveExpr, overwriteExpr, token);
        recursiveExpr = parser.ParseExpression();
        if (parser.Match(TokenKind.Comma))
        {
            overwriteExpr = parser.ParseExpression();
        }

        return new XCopyStmt(source, destination, recursiveExpr, overwriteExpr, token);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione IO.MEMORYMAP per mappare un file in memoria.
    /// Gestisce sia la sintassi legacy (`MEMORYMAP "file.dat" AS handle`) che quella moderna qualificata
    /// fusa (`IO.MEMORYMAP "file.dat" AS handle`), senza parentesi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'IO' se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="MemoryMapStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi Gestite:</b>
    /// <code>
    /// ' Legacy
    /// MEMORYMAP "mydata.bin" AS hData
    /// 
    /// ' Moderna (qualificata fusa)
    /// IO.MEMORYMAP "mydata.bin" AS hData
    /// </code>
    /// </example>
    public static Stmt ParseMemoryMapStatement(Parser parser, string expectedNamespace)
    {
        var memoryMapToken = parser.Consume(TokenKind.MemoryMap, "Internal parser error: Expected 'MEMORYMAP'.");

        // --- Sintassi unificata (legacy e qualificata fusa): MEMORYMAP filePath AS handle ---
        var filePath = parser.ParseExpression();
        parser.Consume(TokenKind.As, "Expected 'AS' after file path in MEMORYMAP statement.");
        var handle = parser.ParseTarget();

        return new MemoryMapStmt(filePath, handle, memoryMapToken);
    }
    
    /// <summary>
    /// [FASE 1] Parsa un'istruzione IO.POKE per scrivere un singolo byte in un Memory Mapped File.
    /// Gestisce sia la sintassi legacy (`MMAPPOKE handle, offset, value`) che quella moderna qualificata
    /// fusa (`IO.MMAPPOKE handle, offset, value`), senza parentesi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO" o "MEMORYMAP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token qualificatore (es. 'IO'), se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="MemoryMapPokeStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi Gestite:</b>
    /// <code>
    /// ' Legacy (dopo IMPORT MEMORYMAP o quando in namespace MEMORYMAP)
    /// MMAPPOKE myHandle, 100, 255 ' Scrive 255 (byte) all'offset 100 nel file mappato da myHandle
    /// 
    /// ' Moderna (qualificata fusa)
    /// IO.MMAPPOKE myHandle, 100, &hFF ' Scrive FFh (byte) all'offset 100
    /// </code>
    /// </example>
    public static Stmt ParseMemoryMapPokeStatement(Parser parser, string expectedNamespace)
    {
        // Consumiamo il TokenKind.MemoryMapPoke, che ora è specifico.
        var pokeToken = parser.Consume(TokenKind.MemoryMapPoke, "Internal parser error: Expected 'MMAPPOKE'.");

        // --- Sintassi unificata (legacy e qualificata fusa): MMAPPOKE handle, offset, value ---
        var handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after handle.");
        var offset = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after offset.");
        var value = parser.ParseExpression();

        return new MemoryMapPokeStmt(handle, offset, value, pokeToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione IO.WRITEBYTES per scrivere un blocco di byte in un Memory Mapped File.
    /// Gestisce sia la sintassi legacy (`MMAPWRITEBYTES handle, offset, sourceArray [, count]`) che quella moderna
    /// qualificata fusa (`IO.MMAPWRITEBYTES handle, offset, sourceArray [, count]`), senza parentesi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("IO" o "MEMORYMAP") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token qualificatore (es. 'IO'), se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="MemoryMapWriteBytesStmt"/>.</returns>
    /// <example>
    /// <b>Sintassi Gestite:</b>
    /// <code>
    /// DIM buffer() AS BYTE
    /// ' Legacy
    /// MMAPWRITEBYTES myHandle, 0, buffer() ' Scrive l'intero buffer dall'inizio del file
    /// MMAPWRITEBYTES myHandle, 1024, buffer(), 50 ' Scrive 50 byte dal buffer all'offset 1024
    /// 
    /// ' Moderna (qualificata fusa)
    /// IO.MMAPWRITEBYTES myHandle, 0, buffer()
    /// IO.MMAPWRITEBYTES myHandle, 1024, buffer(), 50
    /// </code>
    /// </example>
    public static Stmt ParseMemoryMapWriteBytesStatement(Parser parser, string expectedNamespace)
    {
        // Consumiamo il TokenKind.MemoryMapWriteBytes, che ora è specifico.
        var writeBytesToken = parser.Consume(TokenKind.MemoryMapWriteBytes, "Internal parser error: Expected 'MMAPWRITEBYTES'.");

        // --- Sintassi unificata (legacy e qualificata fusa): MMAPWRITEBYTES handle, offset, sourceArray [, count] ---
        var handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after handle.");
        var offset = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after offset.");
        var sourceArray = parser.ParseExpression();
        Expr? count = null;
        if (parser.Match(TokenKind.Comma))
        {
            count = parser.ParseExpression();
        }

        return new MemoryMapWriteBytesStmt(handle, offset, sourceArray, count, writeBytesToken);
    }
    
}