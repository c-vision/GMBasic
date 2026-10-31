// FILE: Interpreter.Ast.Files.cs

using System.Text;

// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Rappresenta le diverse modalità di apertura di un file.
/// </summary>
public enum FileOpenMode
{
    Input,
    Output,
    Append,
    Random,
    Binary,
    Isam,
    Dbf
}

/// <summary>
/// Rappresenta le opzioni di lock per l'apertura di un file.
/// </summary>
public enum FileLockMode
{
    Default,
    Shared,
    LockRead,
    LockWrite,
    LockReadWrite
}

/// <summary>
/// Rappresenta i permessi di accesso per l'apertura di un file.
/// </summary>
public enum FileAccessMode
{
    Default,
    Read,
    Write,
    ReadWrite
}

/// <summary>
/// Rappresenta la modalità di locking a livello di motore di database (condiviso o esclusivo).
/// </summary>
public enum DatabaseLockingMode
{
    /// <summary>
    /// La modalità di default verrà scelta dall'Executor.
    /// </summary>
    Default,

    /// <summary>
    /// Apertura in modalità condivisa, sicura per l'accesso multi-processo.
    /// Mapperà a LockingStrategy.CrossProcess.
    /// </summary>
    Shared,

    /// <summary>
    /// Apertura in modalità esclusiva, ottimizzata per l'accesso single-process.
    /// Mapperà a LockingStrategy.IntraProcess.
    /// </summary>
    Exclusive
}

/// <summary>
/// Rappresenta l'istruzione OPEN per tutte le modalità (standard, ISAM, DBF).
/// </summary>
public sealed record OpenStmt(
    Expr FileName,
    FileOpenMode Mode,
    DatabaseLockingMode Locking,
    FileLockMode LockMode,
    FileAccessMode Access,
    Expr FileNumber,
    // Clausole opzionali
    DataType? FileContentType,
    Expr? RecordLength,
    IsamIndexedByClause? IndexedBy,
    ImmutableArray<Expr> IndexPaths,
    Expr? EncodingExpr,
    bool IsUnsafe, // Flag UNSAFE per l'intera istruzione (solo ISAM)
    Token Token
) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder();
        sb.Append($"OPEN {FileName}");
        if (Locking != DatabaseLockingMode.Default) sb.Append($" {Locking.ToString().ToUpper()}");
        sb.Append($" FOR {Mode}");
        if (IsUnsafe) sb.Append(" UNSAFE");
        if (EncodingExpr is not null) sb.Append($" ENCODING {EncodingExpr}");
        if (Access != FileAccessMode.Default) sb.Append($" ACCESS {Access}");
        if (LockMode != FileLockMode.Default) sb.Append($" LOCK {LockMode}");

        // Ordine corretto: AS #filenumber viene prima delle altre clausole opzionali.
        sb.Append($" AS #{FileNumber}");

        // Ora gestisce le clausole che seguono AS #
        if (FileContentType is not null)
        {
            var typeString = FileContentType switch
            {
                JsonType => "TYPE JSON",
                UserDefinedType udt => $"OF {udt.Name.Lexeme}",
                _ => $"OF {FileContentType}"
            };
            sb.Append($" {typeString}");
        }

        if (RecordLength is not null) sb.Append($" LEN = {RecordLength}");
        if (IndexedBy is not null) sb.Append($" {IndexedBy}");
        if (IndexPaths.Any()) sb.Append($" INDEX {string.Join(", ", IndexPaths)}");
        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo OpenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di OpenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileName = (Expr)FileName.DeepClone();
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedFileContentType = FileContentType?.DeepClone() as DataType;
        var clonedRecordLength = (Expr?)RecordLength?.DeepClone();
        var clonedIndexedBy = IndexedBy?.DeepClone();
        var clonedIndexPaths = IndexPaths.Select(p => (Expr)p.DeepClone()).ToImmutableArray();
        var clonedEncodingExpr = (Expr?)EncodingExpr?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();

        return new OpenStmt(clonedFileName, Mode, Locking, LockMode, Access, clonedFileNumber, clonedFileContentType,
            clonedRecordLength, clonedIndexedBy, clonedIndexPaths, clonedEncodingExpr, IsUnsafe, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione CLOSE per chiudere uno o più canali di I/O.
/// <see cref="IsIsam"/> = true per `ISAM CLOSE [alias, ...]` (stesso nodo di `CLOSE #n`, prodotto da
/// ParseIsamCloseStatement): le espressioni sono ALIAS di work area ISAM (non handle di file) e il
/// comando chiude davvero l'area. Prima il nodo non distingueva i due casi e `ISAM CLOSE` non chiudeva nulla
/// su IL/PCode/VM-C/C99 (l'area restava aperta e raggiungibile).
/// </summary>
public sealed record CloseStmt(ImmutableArray<Expr> FileNumbers, Token Token, bool IsIsam = false) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        FileNumbers.Any() ? string.Join(", ", FileNumbers.Select(f => $"#{f}")) : string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo CloseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CloseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumbers = FileNumbers.Select(f => (Expr)f.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new CloseStmt(clonedFileNumbers, clonedToken, IsIsam);
    }
}

/// <summary>
/// Rappresenta l'istruzione PRINT # per scrivere dati in un file sequenziale.
/// </summary>
public sealed record PrintFileStmt(Expr FileNumber, ImmutableArray<PrintArg> Arguments, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"#{FileNumber}, {string.Join(" ", Arguments)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo PrintFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PrintFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedArguments = Arguments.Select(a => a.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new PrintFileStmt(clonedFileNumber, clonedArguments, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione WRITE # per scrivere dati formattati (CSV-like) in un file sequenziale.
/// </summary>
public sealed record WriteFileStmt(Expr FileNumber, ImmutableArray<Expr> Arguments, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"#{FileNumber}, {string.Join(", ", Arguments)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo WriteFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di WriteFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedArguments = Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new WriteFileStmt(clonedFileNumber, clonedArguments, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione INPUT # per leggere dati strutturati da un file sequenziale.
/// </summary>
public sealed record InputFileStmt(Expr FileNumber, ImmutableArray<Target> Variables, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"#{FileNumber}, {string.Join(", ", Variables)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo InputFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di InputFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedVariables = Variables.Select(v => (Target)v.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new InputFileStmt(clonedFileNumber, clonedVariables, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione LINE INPUT # per leggere un'intera riga da un file di testo.
/// Questa versione è stata corretta per accettare un Target, consentendo l'assegnazione a L-values complessi.
/// </summary>
public sealed record LineInputFileStmt(Expr FileNumber, Target Variable, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"#{FileNumber}, {Variable}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LineInputFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LineInputFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedVariable = (Target)Variable.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new LineInputFileStmt(clonedFileNumber, clonedVariable, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione GET # per leggere dati da un file in modalità RANDOM o BINARY.
/// </summary>
public sealed record GetFileStmt(Expr FileNumber, Expr? RecordNumber, Target Destination, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>True per `ISAM GET area, rec` (record corrente di un'area ISAM): stesso nodo di `GET #n, rec, dest`
    /// (file RANDOM/BINARY), che i backend a compile-time devono tenere distinti.</summary>
    public bool IsIsam { get; init; }

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"#{FileNumber}, {(RecordNumber is not null ? $"{RecordNumber}, " : "")}{Destination}";

    /// <summary>
    /// Crea una copia profonda di questo nodo GetFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GetFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedRecordNumber = (Expr?)RecordNumber?.DeepClone();
        var clonedDestination = (Target)Destination.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new GetFileStmt(clonedFileNumber, clonedRecordNumber, clonedDestination, clonedToken) { IsIsam = IsIsam };
    }
}

/// <summary>
/// Rappresenta l'istruzione PUT # per scrivere dati in un file.
/// Gestisce la modalità RANDOM/BINARY (con RecordNumber) e la modalità ISAM (senza RecordNumber).
/// Per la modalità ISAM, supporta un flag opzionale per la valutazione "unsafe" delle chiavi.
/// </summary>
public sealed record PutFileStmt(Expr FileNumber, Expr? RecordNumber, Expr Source, bool IsUnsafe, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"PUT #{FileNumber}, {(RecordNumber is not null ? $"{RecordNumber}, " : "")}{Source}{(IsUnsafe ? " UNSAFE" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo PutFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di PutFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedRecordNumber = (Expr?)RecordNumber?.DeepClone();
        var clonedSource = (Expr)Source.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new PutFileStmt(clonedFileNumber, clonedRecordNumber, clonedSource, IsUnsafe, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione SEEK # per spostare il puntatore in un file.
/// </summary>
public sealed record SeekStmt(Expr FileNumber, Expr PositionExpr, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"#{FileNumber}, {PositionExpr}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SeekStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SeekStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFileNumber = (Expr)FileNumber.DeepClone();
        var clonedPosition = (Expr)PositionExpr.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new SeekStmt(clonedFileNumber, clonedPosition, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione KILL per cancellare un file.
/// </summary>
public sealed record KillStmt(Expr FileName, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => FileName.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo KillStmt.
    /// </summary>
    /// <returns>Una nuova istanza di KillStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new KillStmt((Expr)FileName.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione NAME per rinominare un file.
/// </summary>
public sealed record RenameStmt(Expr OldFileName, Expr NewFileName, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{OldFileName} AS {NewFileName}";

    /// <summary>
    /// Crea una copia profonda di questo nodo RenameStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RenameStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedOldFileName = (Expr)OldFileName.DeepClone();
        var clonedNewFileName = (Expr)NewFileName.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new RenameStmt(clonedOldFileName, clonedNewFileName, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione MKDIR per creare una directory.
/// </summary>
public sealed record MkDirStmt(Expr DirectoryPath, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => DirectoryPath.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo MkDirStmt.
    /// </summary>
    /// <returns>Una nuova istanza di MkDirStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new MkDirStmt((Expr)DirectoryPath.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione RMDIR per rimuovere una directory.
/// </summary>
public sealed record RmDirStmt(Expr DirectoryPath, Expr? RecursiveExpr, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{DirectoryPath}{(RecursiveExpr is not null ? $", {RecursiveExpr}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo RmDirStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RmDirStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedDirectoryPath = (Expr)DirectoryPath.DeepClone();
        var clonedRecursiveExpr = (Expr?)RecursiveExpr?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new RmDirStmt(clonedDirectoryPath, clonedRecursiveExpr, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione CHDIR per cambiare la directory corrente.
/// </summary>
public sealed record ChDirStmt(Expr DirectoryPath, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => DirectoryPath.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo ChDirStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ChDirStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ChDirStmt((Expr)DirectoryPath.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione COPY per copiare un file.
/// </summary>
public sealed record CopyStmt(Expr SourceFile, Expr DestinationFile, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{SourceFile} TO {DestinationFile}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CopyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CopyStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSourceFile = (Expr)SourceFile.DeepClone();
        var clonedDestinationFile = (Expr)DestinationFile.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new CopyStmt(clonedSourceFile, clonedDestinationFile, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione MOVE per spostare un file.
/// </summary>
public sealed record MoveStmt(Expr SourceFile, Expr DestinationFile, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{SourceFile} TO {DestinationFile}";

    /// <summary>
    /// Crea una copia profonda di questo nodo MoveStmt.
    /// </summary>
    /// <returns>Una nuova istanza di MoveStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSourceFile = (Expr)SourceFile.DeepClone();
        var clonedDestinationFile = (Expr)DestinationFile.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new MoveStmt(clonedSourceFile, clonedDestinationFile, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione XCOPY per copiare directory.
/// </summary>
public sealed record XCopyStmt(
    Expr SourceDirectory,
    Expr DestinationDirectory,
    Expr? RecursiveExpr,
    Expr? OverwriteExpr,
    Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{SourceDirectory} TO {DestinationDirectory}";

    /// <summary>
    /// Crea una copia profonda di questo nodo XCopyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di XCopyStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSourceDirectory = (Expr)SourceDirectory.DeepClone();
        var clonedDestinationDirectory = (Expr)DestinationDirectory.DeepClone();
        var clonedRecursiveExpr = (Expr?)RecursiveExpr?.DeepClone();
        var clonedOverwriteExpr = (Expr?)OverwriteExpr?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new XCopyStmt(clonedSourceDirectory, clonedDestinationDirectory, clonedRecursiveExpr,
            clonedOverwriteExpr, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione LOCK per bloccare un file intero o un singolo record.
/// </summary>
/// <param name="FileNumber">L'espressione che restituisce l'handle del file da bloccare.</param>
/// <param name="RecordNumber">L'espressione opzionale che indica il numero del record da bloccare. Se omesso, viene bloccato l'intero file.</param>
/// <param name="Token">Il token 'LOCK' per riferimento.</param>
public sealed record LockStmt(Expr FileNumber, Expr? RecordNumber, Token Token) 
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LOCK #{FileNumber}{(RecordNumber is not null ? $", {RecordNumber}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LockStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LockStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LockStmt(
            (Expr)FileNumber.DeepClone(),
            (Expr?)RecordNumber?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione UNLOCK per sbloccare un file intero o un singolo record.
/// </summary>
/// <param name="FileNumber">L'espressione che restituisce l'handle del file da sbloccare.</param>
/// <param name="RecordNumber">L'espressione opzionale che indica il numero del record da sbloccare. Se omesso, viene sbloccato l'intero file.</param>
/// <param name="Token">Il token 'UNLOCK' per riferimento.</param>
public sealed record UnlockStmt(Expr FileNumber, Expr? RecordNumber, Token Token) 
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"UNLOCK #{FileNumber}{(RecordNumber is not null ? $", {RecordNumber}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo UnlockStmt.
    /// </summary>
    /// <returns>Una nuova istanza di UnlockStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new UnlockStmt(
            (Expr)FileNumber.DeepClone(),
            (Expr?)RecordNumber?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione IO.WATCH per monitorare in modo asincrono i cambiamenti nel file system.
/// Registra una subroutine di callback che verrà invocata quando si verificano eventi specifici
/// (creazione, modifica, cancellazione) nella directory target.
/// </summary>
/// <param name="Path">L'espressione che restituisce il percorso della directory da monitorare.</param>
/// <param name="EventsMask">L'espressione che restituisce la maschera bitwise degli eventi da ascoltare.</param>
/// <param name="Callback">Il token che rappresenta il nome della subroutine da invocare al verificarsi dell'evento.</param>
/// <param name="Token">Il token 'WATCH' per riferimento alla posizione nel sorgente.</param>
public sealed record IoWatchStmt(Expr Path, Expr EventsMask, Token Callback, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"WATCH {Path}, {EventsMask}, {Callback.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IoWatchStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IoWatchStmt con copie profonde delle espressioni.</returns>
    public override AstNode DeepClone()
    {
        return new IoWatchStmt(
            (Expr)Path.DeepClone(), 
            (Expr)EventsMask.DeepClone(), 
            (Token)Callback.DeepClone(), 
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione IO.UNWATCH per interrompere il monitoraggio attivo su una directory.
/// Rilascia le risorse del FileSystemWatcher associato al percorso specificato.
/// </summary>
/// <param name="Path">L'espressione che restituisce il percorso della directory da smettere di monitorare.</param>
/// <param name="Token">Il token 'UNWATCH' per riferimento alla posizione nel sorgente.</param>
public sealed record IoUnwatchStmt(Expr Path, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"UNWATCH {Path}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IoUnwatchStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new IoUnwatchStmt((Expr)Path.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione IO.MEMORYMAP per mappare un file in memoria.
/// </summary>
/// <param name="FilePath">L'espressione che restituisce il percorso del file da mappare.</param>
/// <param name="Handle">La variabile di destinazione per l'handle del file mappato.</param>
/// <param name="Token">Il token 'MEMORYMAP' per riferimento.</param>
public sealed record MemoryMapStmt(Expr FilePath, Target Handle, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"MEMORYMAP {FilePath} AS {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo MemoryMapStmt.
    /// </summary>
    /// <returns>Una nuova istanza di MemoryMapStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new MemoryMapStmt(
            (Expr)FilePath.DeepClone(),
            (Target)Handle.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione IO.POKE per scrivere un singolo byte in un Memory Mapped File.
/// </summary>
/// <param name="Handle">L'espressione che restituisce l'handle del file mappato.</param>
/// <param name="Offset">L'espressione che indica l'offset nel file (0-based).</param>
/// <param name="Value">L'espressione che restituisce il valore del byte da scrivere.</param>
/// <param name="Token">Il token 'POKE' per riferimento.</param>
public sealed record MemoryMapPokeStmt(Expr Handle, Expr Offset, Expr Value, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"POKE {Handle}, {Offset}, {Value}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IoPokeStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IoPokeStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new MemoryMapPokeStmt(
            (Expr)Handle.DeepClone(),
            (Expr)Offset.DeepClone(),
            (Expr)Value.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione IO.WRITEBYTES per scrivere un blocco di byte in un Memory Mapped File.
/// </summary>
/// <param name="Handle">L'espressione che restituisce l'handle del file mappato.</param>
/// <param name="Offset">L'espressione che indica l'offset di partenza nel file (0-based).</param>
/// <param name="SourceArray">L'espressione (array di byte) contenente i byte da scrivere.</param>
/// <param name="Count">L'espressione opzionale che indica il numero di byte da scrivere. Se omesso, scrive tutti i byte nell'array.</param>
/// <param name="Token">Il token 'WRITEBYTES' per riferimento.</param>
public sealed record MemoryMapWriteBytesStmt(Expr Handle, Expr Offset, Expr SourceArray, Expr? Count, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"WRITEBYTES {Handle}, {Offset}, {SourceArray}{(Count is not null ? $", {Count}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IoWriteBytesStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IoWriteBytesStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new MemoryMapWriteBytesStmt(
            (Expr)Handle.DeepClone(),
            (Expr)Offset.DeepClone(),
            (Expr)SourceArray.DeepClone(),
            (Expr?)Count?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}
