// FILE: Ast.Exceptions.cs

namespace Basic;

/// <summary>
///     Definisce i codici di errore standard per il runtime di BASIC.
///     I valori sono scelti per essere compatibili con quelli storici e estesi per una diagnostica migliore.
/// </summary>
public enum ErrorCode
{
    // --- Errori Generali e di Flusso (1-19) ---
    NextWithoutFor = 1,
    SyntaxError = 2,
    ReturnWithoutGosub = 3,
    OutOfData = 4,
    IllegalFunctionCall = 5,
    Overflow = 6,
    OutOfMemory = 7,
    UndefinedLine = 8,
    SubscriptOutOfRange = 9,
    DuplicateDefinition = 10,
    DivisionByZero = 11,
    ExpectedArray = 12,
    TypeMismatch = 13,
    ArgumentCountMismatch = 14,
    StringTooLong = 15,
    ExpressionTooComplex = 16,
    ExitNotInLoop = 17,
    UndefinedSubOrFunction = 18,
    UndefinedVariable = 19,

    // --- Errori Matematici e Numerici (20-39) ---
    InvalidParameter = 20,
    IllegalQuantityError = 21,
    CalculationError = 22,
    LogarithmError = 23,

    // --- Errori di I/O su File (50-79) ---
    FileNotFound = 53,
    BadFileMode = 54,
    FileAlreadyOpen = 55,
    BadFileNumber = 56,
    InputPastEndOfFile = 62,
    FileAlreadyExists = 58,
    BadRecordLength = 59,
    BadFileName = 64,
    TooManyFiles = 67,
    DeviceUnavailable = 68,
    PermissionDenied = 70,
    DiskFull = 71,
    PathNotFound = 76,

    // --- Errori di Grafica e Suono (80-89) ---
    GraphicsError = 80,
    InvalidCoordinates = 81,
    FontNotLoaded = 82,
    SoundError = 83,

    // --- Errori di Rete e Connettività (90-95) ---
    HostNotFound = 90,
    ConnectionRefused = 91,
    ConnectionTimedOut = 92,
    NetworkError = 93,
    SslHandshakeFailed = 94,
    InvalidCertificate = 95,

    // --- Errori di Pre-Processore (90-99) ---
    IncludeFileNotFound = 96,
    CircularInclude = 97,
    IncludeFileWithLineNumbers = 98,

    // --- Errori Specifici diGM-BASIC (100+) ---
    ChannelOperationError = 100,
    WaitGroupError = 101,
    JsonParseError = 102,
    JsonPropertyNotFound = 103,
    InvalidArrayDimensions = 104,
    PreserveOnInvalidDimension = 105,
    InvalidObjectTypeForNew = 106,

    FormatError = 110,
    JsonSerializationError = 111,

    // --- Errori Database (ISAM & DBF) (120-139) ---
    NoDatabaseInUse = 120,
    DatabaseIsamError = 121,
    DatabaseDbfError = 122,
    IndexNotFound = 123,
    RecordLocked = 124,
    FileIsLocked = 125,
    RecordNotFound = 126,
    SchemaMismatch = 127,
    DuplicateFieldName = 128,
    DbfAreaNotInUse = 129,
    DbfAliasNotFound = 130,

    // --- Errori UDT, Proprietà e Collezioni (140-149) ---
    PropertyIsWriteOnly = 140,
    KeyNotFound = 141,
    CollectionIsEmpty = 142,
    TypeNotInstantiable = 143,
    StopIteration = 144,
    AmbiguousCall = 145,
    NullReferenceError = 146,
    TypeNotDefined = 147,
    PropertyIsReadOnly = 148,
    MemberNotFound = 149,
    NonVirtualMethodOverride = 150,
    ReturnTypeMismatchInOverride = 151,
    NamespaceNotImported = 152,

    // --- Errori FFI (150-159) ---
    FfiCallError = 160,
    FfiLoadError = 161,
    FfiInvalidSignature = 162,
    FfiUnsupportedType = 163,
    FfiMemoryError = 164,

    // --- Errori Interni del Runtime (200+) ---
    InternalError = 200,

    UserDefined = 256,

    UnspecifiedError = 0
}

/// <summary>
/// Rappresenta un'istanza a runtime dell'oggetto di errore, contenente
/// i dettagli di una specifica eccezione, incluso lo stack trace.
/// </summary>
public sealed record ErrInstance(int Number, string Description, int LineNumber, string SourceLineText, string StackTrace = "");

    /// <summary>
    ///     Thrown for BASIC runtime or syntax errors.
    /// </summary>
    public sealed class BasicException : Exception
    {
        /// <summary>Gets the numeric code of the error.</summary>
        public ErrorCode Code { get; }

        /// <summary>When ErrorCode is UserDefined, holds the thrown ClassInstance or other value.</summary>
        public object? UserExceptionInstance { get; init; }

    /// <summary>
    /// Ottiene le informazioni complete sulla riga di codice sorgente che ha causato l'errore,
    /// inclusi il file e il numero di riga originali. È null se non disponibile.
    /// </summary>
    public SourceLine? SourceLocation { get; }

    /// <summary>Gets the full source position (File ID, Line, Column) where the error occurred.</summary>
    public SourcePosition Position { get; }

    /// <summary>Gets the line number where the error occurred. (Legacy property)</summary>
    public int Line => Position.Line;

    /// <summary>Gets the column number where the error occurred. (Legacy property)</summary>
    public int Column => Position.Column;

    /// <summary>
    /// Initializes a new instance of the <see cref="BasicException"/> class.
    /// Questo è il costruttore principale.
    /// </summary>
    public BasicException(ErrorCode code, SourcePosition position, string? message = null,
        SourceLine? sourceLine = null, Exception? innerException = null)
        : base(message ?? GetDefaultMessage(code), innerException)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(position.Line);
        ArgumentOutOfRangeException.ThrowIfNegative(position.Column);

        Code = code;
        Position = position;
        SourceLocation = sourceLine;
    }

    /// <summary>
    /// Avvolge un'eccezione CLR nativa (MAI una BasicException — chi la cattura la
    /// ripropaga direttamente, senza passare da qui) in una BasicException con
    /// l'ErrorCode più vicino al suo significato reale, così che ON ERROR/TRY-CATCH
    /// possano intercettarla come un qualunque altro errore BASIC. Senza questo, un
    /// IndexOutOfRangeException/FileNotFoundException/... nativo del CLR bypassa del
    /// tutto la gestione errori del linguaggio (vedi AiWiki IL-OnError-Loop-Bug.md,
    /// gap array-out-of-range/file mancante).
    /// </summary>
    public static BasicException FromClrException(Exception ex, SourcePosition position)
    {
        var code = ex switch
        {
            IndexOutOfRangeException or ArgumentOutOfRangeException => ErrorCode.SubscriptOutOfRange,
            DivideByZeroException => ErrorCode.DivisionByZero,
            FileNotFoundException => ErrorCode.FileNotFound,
            DirectoryNotFoundException => ErrorCode.PathNotFound,
            UnauthorizedAccessException => ErrorCode.PermissionDenied,
            NullReferenceException => ErrorCode.NullReferenceError,
            InvalidCastException => ErrorCode.TypeMismatch,
            OverflowException => ErrorCode.Overflow,
            KeyNotFoundException => ErrorCode.KeyNotFound,
            _ => ErrorCode.InternalError
        };
        return new BasicException(code, position, ex.Message, null, ex);
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="BasicException"/> class, storing context from a token.
    /// </summary>
    public BasicException(Token token, string message, ErrorCode code = ErrorCode.SyntaxError, Exception? innerException = null)
        : this(
            code: code,
            position: token.Position,
            message: message,
            innerException: innerException
        )
    {
        // La risoluzione del SourceLine completo (SourceLine.Text) è delegata a chi cattura/logga l'errore,
        // che avrà accesso alla SourceMap completa del BasicProgram.
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="BasicException"/> class, storing context from an AST node.
    /// </summary>
    public BasicException(AstNode node, string message, ErrorCode code = ErrorCode.SyntaxError, Exception? innerException = null)
        : this(
            code: code,
            position: node.Position,
            message: message,
            innerException: innerException
        )
    {
    }

    /// <summary>
    /// Overrides ToString to provide a rich, user-friendly error message without the C# stack trace.
    /// </summary>
    public override string ToString()
    {
        if (SourceLocation == null)
        {
            // Fallback for errors without full source location info.
            return $"ERRORE DI ESECUZIONE: {Message} (Posizione: {Position})";
        }

        var sb = new System.Text.StringBuilder();
        // Utilizziamo Path.GetFileName solo se System.IO è disponibile e l'ambiente lo supporta.
        // Altrimenti usiamo il percorso completo o una versione pulita.
        // Per semplicità, manteniamo l'originale ma ci assicuriamo che System.IO sia implicito o aggiunto se necessario.
        // Assumo System.IO.Path.
        sb.Append(Path.GetFileName(SourceLocation.FilePath));
        sb.Append(':');
        sb.Append(SourceLocation.OriginalLineNumber);
        sb.Append(':');
        sb.Append(Column);
        sb.Append(": error: ");
        sb.AppendLine(Message);

        var sourceLineText = SourceLocation.Text;
        sb.AppendLine($"--> {sourceLineText.TrimEnd()}");

        var pointerPadding = 4 + Column - 1;
        if (pointerPadding > 0)
        {
            sb.Append(new string(' ', pointerPadding));
        }

        sb.Append('^');

        return sb.ToString();
    }

    /// <summary>
    /// Generates a default, user-friendly error message based on the error code.
    /// Pubblica: usata anche da PCode per popolare `ERR.Description` quando un errore viene
    /// sollevato senza messaggio (`ERROR 11`), così le 4 gambe riportano lo stesso testo
    /// (il C ha lo stesso catalogo in gmb_err_default_message).
    /// </summary>
    public static string GetDefaultMessage(ErrorCode code) => code switch
    {
        // Generali e Flusso
        ErrorCode.NextWithoutFor => "NEXT without FOR",
        ErrorCode.SyntaxError => "Syntax error",
        ErrorCode.ReturnWithoutGosub => "RETURN without GOSUB",
        ErrorCode.OutOfData => "Out of DATA",
        ErrorCode.IllegalFunctionCall => "Illegal function call",
        ErrorCode.Overflow => "Overflow",
        ErrorCode.OutOfMemory => "Out of memory",
        ErrorCode.UndefinedLine => "Undefined line number",
        ErrorCode.SubscriptOutOfRange => "Subscript out of range",
        ErrorCode.DuplicateDefinition => "Duplicate definition",
        ErrorCode.DivisionByZero => "Division by zero",
        ErrorCode.ExpectedArray => "Array expected",
        ErrorCode.TypeMismatch => "Type mismatch",
        ErrorCode.StringTooLong => "String too long",
        ErrorCode.ArgumentCountMismatch => "Wrong number of arguments",
        ErrorCode.ExpressionTooComplex => "Expression too complex",
        ErrorCode.ExitNotInLoop => "EXIT not within FOR or DO loop",
        ErrorCode.UndefinedSubOrFunction => "Subprogram or function not defined",
        ErrorCode.UndefinedVariable => "Variable not defined",

        // Matematici
        ErrorCode.InvalidParameter => "Invalid parameter",
        ErrorCode.IllegalQuantityError => "Illegal quantity",
        ErrorCode.CalculationError => "Calculation error",
        ErrorCode.LogarithmError => "Invalid logarithm argument",

        // I/O File
        ErrorCode.FileNotFound => "File not found",
        ErrorCode.BadFileMode => "Bad file mode",
        ErrorCode.FileAlreadyOpen => "File already open",
        ErrorCode.BadFileNumber => "Bad file number or handle",
        ErrorCode.InputPastEndOfFile => "Input past end of file",
        ErrorCode.FileAlreadyExists => "File already exists",
        ErrorCode.BadRecordLength => "Bad record length",
        ErrorCode.BadFileName => "Bad file name",
        ErrorCode.TooManyFiles => "Too many files open",
        ErrorCode.DeviceUnavailable => "Device unavailable",
        ErrorCode.PermissionDenied => "Permission denied",
        ErrorCode.DiskFull => "Disk full",
        ErrorCode.PathNotFound => "Path not found",

        // Grafica e Suono
        ErrorCode.GraphicsError => "Graphics error",
        ErrorCode.InvalidCoordinates => "Invalid coordinates",
        ErrorCode.FontNotLoaded => "Font not loaded",
        ErrorCode.SoundError => "Sound error",

        // Messaggi per Errori di Rete
        ErrorCode.HostNotFound => "Host not found or address could not be resolved",
        ErrorCode.ConnectionRefused => "Connection refused by host",
        ErrorCode.ConnectionTimedOut => "Connection timed out",
        ErrorCode.NetworkError => "A network error occurred",
        ErrorCode.SslHandshakeFailed => "SSL handshake failed",
        ErrorCode.InvalidCertificate => "Invalid SSL certificate",

        // Pre-Processore
        ErrorCode.IncludeFileNotFound => "Include file not found",
        ErrorCode.CircularInclude => "Circular include detected",
        ErrorCode.IncludeFileWithLineNumbers => "Included file cannot contain line numbers",

        // Specifici diGM-BASIC
        ErrorCode.ChannelOperationError => "Channel operation error",
        ErrorCode.WaitGroupError => "WaitGroup error",
        ErrorCode.JsonParseError => "JSON parse error",
        ErrorCode.JsonPropertyNotFound => "JSON property not found",
        ErrorCode.InvalidArrayDimensions => "Invalid array dimensions",
        ErrorCode.PreserveOnInvalidDimension => "REDIM PRESERVE can only change the last dimension",
        ErrorCode.InvalidObjectTypeForNew => "Type cannot be instantiated with NEW",

        // Formattazione e Serializzazione
        ErrorCode.FormatError => "Invalid format string or value",
        ErrorCode.JsonSerializationError => "JSON serialization error",

        // --- Errori Database ---
        ErrorCode.NoDatabaseInUse => "No database in use",
        ErrorCode.DatabaseIsamError => "ISAM file error",
        ErrorCode.DatabaseDbfError => "DBF file error",
        ErrorCode.IndexNotFound => "Index not found",
        ErrorCode.RecordLocked => "Record is locked by another user",
        ErrorCode.FileIsLocked => "File is locked by another user",
        ErrorCode.RecordNotFound => "Record not found",
        ErrorCode.SchemaMismatch => "File structure does not match TYPE definition",
        ErrorCode.DuplicateFieldName => "Duplicate field name",
        ErrorCode.DbfAreaNotInUse => "DBF work area not in use",
        ErrorCode.DbfAliasNotFound => "DBF alias not found",

        // --- Errori UDT, Proprietà e Collezioni ---
        ErrorCode.PropertyIsWriteOnly => "Property is write-only",
        ErrorCode.KeyNotFound => "Key not found in collection",
        ErrorCode.CollectionIsEmpty => "Operation cannot be performed on an empty collection",
        ErrorCode.TypeNotInstantiable => "Type cannot be instantiated",
        ErrorCode.StopIteration => "Stop iteration",
        ErrorCode.AmbiguousCall => "Ambiguous call",
        ErrorCode.NullReferenceError => "Null reference error",
        ErrorCode.TypeNotDefined => "User-defined type not defined",
        ErrorCode.PropertyIsReadOnly => "Property is read-only",
        ErrorCode.MemberNotFound => "Member not found in object or type",
        ErrorCode.NonVirtualMethodOverride => "Method override failed: base method is not declared VIRTUAL",
        ErrorCode.ReturnTypeMismatchInOverride => "Return type of overriding method does not match the base method",
        ErrorCode.NamespaceNotImported => "Namespace not imported",

        // ---  FFI ---
        ErrorCode.FfiCallError => "External function call error",
        ErrorCode.FfiLoadError => "Error loading external library or resolving symbol",
        ErrorCode.FfiInvalidSignature => "External function signature mismatch",
        ErrorCode.FfiUnsupportedType => "Unsupported type for FFI marshalling",
        ErrorCode.FfiMemoryError => "FFI memory management error",

        // --- Errori Interni del Runtime (200+) ---
        ErrorCode.InternalError => "Internal runtime error",

        _ => "Unspecified runtime error"
    };
}