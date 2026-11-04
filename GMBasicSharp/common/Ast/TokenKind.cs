// FILE: Ast.TokenKind.cs
// ReSharper disable InvalidXmlDocComment
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
///     Definisce i tipi di token lessicali riconosciuti dall'interprete BASIC.
/// </summary>
public enum TokenKind
{
    // ========================================================================
    // --- Keyword: Direttive Pre-processore ---
    // ========================================================================
    Define, // #DEF
    Undef, // #UNDEF

    // ========================================================================
    // --- Token Speciali e Strutturali ---
    // ========================================================================
    Unknown,
    Eof,
    LineNumber,
    Newline,
    Colon,
    DictColon,
    Label,
    //RemText,

    // ========================================================================
    // --- Letterali e Identificatori ---
    // ========================================================================
    Number,
    StringLiteral,
    InterpolatedStringLiteral,
    Identifier,
    Id,

    // ========================================================================
    // --- Operatori e Delimitatori ---
    // ========================================================================
    Comma,
    Semicolon,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Dot,
    Hash,
    Arrow,
    FatArrow,
    InlineAssign,
    AtSign,
    Box,    // BOX statement
    Say,    // @...SAY
    Picture, // PICTURE clause
    Ampersand,
    QuestionMark,
    QuestionMarkQuestion, // OPERATORE ??
    Bar,

    // --- Operatori Aritmetici ---
    Plus,
    Minus,
    Multiply,
    Divide,
    IntegerDivide,
    Power,
    Mod,
    PlusPlus,
    MinusMinus,

    // --- Operatori di Comparazione ---
    Equal,
    NotEqual,
    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual,
    ExactlyEqual, // Per ==
    
    // ========================================================================
    // --- Keyword: Tipi di Dato ---
    // ========================================================================
    As,
    StringType,

    // --- Tipi Booleani ---
    Bit,
    Bool,
    Boolean,

    // --- Tipi Interi ---
    Byte,
    SByte,
    Integer,
    Short,
    Int,
    UInteger,
    UShort,
    UInt,
    Long,
    ULong,
    Quad,
    UQuad,

    // --- Tipi Virgola Mobile ---
    Single,
    Double,

    // --- Tipi Decimal ---
    Currency,

    // --- Tipo Void ---
    Void,
    
    // --- Tipo ANY ---
    Any,

    // --- Tipi Data e Ora ---
    Date,
    Time,
    DateTime,
    TimeSpan,

    // --- NUOVI TIPI DI DATO PER COLLEZIONI ---
    Array,
    List,
    Dict, // Abbreviazione di DICTIONARY
    HashSet,
    Queue, // anche modificatore per PLAY
    Stack,
    Sorted, // Abbreviazione di SORTEDDICTIONARY
    Tree,
    AVLTree,
    RedBlack,       // Per RedBlackTree
    BITree,         // Per BinaryIndexedTree
    Trie,
    LinkedList,
    DoublyLinkedList,
    Circular,       // Per CircularLinkedList
    Heap,
    MaxHeap,        // Per distinguere Min-Heap da Max-Heap
    Bag,
    Cache,
    Graph,
    Brains,   // namespace/command BRAINS (grafi dei brain; GRAPH resta per la teoria dei grafi)
    Timeline,
    HashRing,         // <-- (per ConsistentHash)
    RingBuffer,       // <-- (per CircularQueue)
    SparseSet,
    BPlusTree,
    StringBuilder,
    Regex,

    // --- Tipi Grafici ---
    DisplayModeType,
    RgbaType,

    // --- Tipi di Errore ---
    Err,

    // --- Tipi di Dato Algebra Lineare ---
    VectorType,

    // --- operatore NEW ---
    New,

    // ========================================================================
    // --- NUOVI TIPI PER SERVER HTTP ---
    // ========================================================================
    HttpRequest,
    HttpHeaders,
    HttpResponse,
    HttpOptions,
    HttpConfig,
    HttpFetchResult,
    HttpPostedData,
    HttpNextHandler,
    CookieJar,
    WebSocket,
    FormField,
    FileField,

    // ========================================================================
    // --- Keyword: Comandi, I/O e Assegnazione ---
    // ========================================================================
    Declare,
    Lib,
    CDecl,
    StdCall,
    ByRef,
    ByVal,
    Global,
    Local,
    Static,
    Optional,
    Import,
    Public,
    Private,
    Dynamic,
    Namespace,
    Var,
    Require,
    Version,
    Const,
    Delegate,
    Call,
    Option,
    ParamArray,

    // ========================================================================
    // --- Keyword: Eventi ---
    // ========================================================================
    Event,          // EVENT
    Raise,          // RAISE
    Handler,        // HANDLER

    // ========================================================================
    // --- Keyword: Flusso di Controllo e Strutture ---
    // ========================================================================
    Do,
    While,
    Wend,
    Loop,
    Until,
    If,
    Then,
    Else,
    ElseIf,
    EndIf,
    Select,
    Switch,
    Case,
    Is,
    EndSelect,
    Match,
    EndMatch,
    Exit,
    Continue,
    Function,
    EndFunction,
    //Sub,
    //EndSub,
    For,
    To,
    Step,
    Next,
    Each,
    Goto,
    Gosub,
    Return,
    End,
    Generator, // Per GENERATOR...END GENERATOR
    EndGenerator, // Per END GENERATOR / ENDGEN
    Yield, // Per YIELD <espressione>

    // --- TYPES ---
    Type,
    EndType,
    Property,
    EndProperty,
    EndGet,
    EndSet,
    Operator,
    EndOperator,
    Self,
    Enum,       // Per ENUM
    EndEnum,    // Per END ENUM
    
    // --- CLASSES ---
    Abstract,
    Class,
    EndClass,
    Inherits,
    Virtual,
    Override,
    Base,
    Protected,
    Friend,
    Interface,      // INTERFACE
    EndInterface,   // END INTERFACE
    Implements,     // IMPLEMENTS
    
    // --- KEYWORD PER GESTIONE ERRORI ---
    On,
    Error,
    Resume,
    Try,
    Catch,
    Finally,
    When,
    EndTry,
    Throw,
    
    // --- NUOVI KEYWORD PER LA CONCORRENZA ---
    Go,
    Wait,
    Done,
    In,

    // --- NUOVI KEYWORD PER IL BUFFER DI TESTO, CURSORE E CLIPBOARD ---
    BSave, // Per BSAVE (x1,y1)-(x2,y2), buffer$
    BLoad, // Per BLOAD (x,y), buffer$
    Clear, // Per CLEAR (x1,y1)-(x2,y2)
    Set, // Per SET BOLD ON
    Off, // Per SET BOLD OFF
    Shell,   // Per SET SHELL ON|OFF e SYSTEM.SHELL "<cmd>"
    ShellOut,   // Per SYSTEM.SHELLOUT("<cmd>") -> output
    ShellCode,  // Per SYSTEM.SHELLCODE("<cmd>") -> exit code
    Verbose,
    Cursor, // Per CURSOR ON/OFF/SHAPE
    Shape, // Per CURSOR SHAPE
    Bold, // Per SET BOLD ON
    Inverse, // Per SET INVERSE ON
    Underline, // Per SET UNDERLINE ON
    Blink, // Per SET BLINK ON
    Clip, // Per CLIP.GET, CLIP.SET, CLIP.CLEAR

    Cls,
    Color,
    Data,
    Dim,
    Redim,
    Preserve,
    Inkey,
    Input,
    Let,
    LineInput,
    Locate,
    Print,
    Read,
    Rem,
    Restore,
    Sleep,
    WaitKey,
    Swap,

    // --- NUOVI KEYWORD GRAFICI ---
    Screen,
    PSet,
    Preset,
    Line,
    Circle,
    Polygon,
    Draw,
    Paint,
    Font,
    PCopy,
    ScreenSet,
    CPalette,
    CPrint,
    CLocate,
    ImageGet,
    ImagePut,
    ImagePuts, // Per il disegno in batch
    Palette, // Per modificare la palette di colori
    CCLS,
    SaveScreen,
    RestoreScreen,
    CSaveScreen,
    CRestoreScreen,
    Texture,
    Textures,
    DrawTextures,
    Unload,
    Caption,
    SendKey,
    ScrollMode,

    // --- SPRITE ---
    Sprite, // La keyword 'SPRITE'
    Position, // La sub-keyword 'POSITION'
    Priority, // La sub-keyword 'PRIORITY'
    Sync, // Il comando 'SYNC'
    Tick, // TICK — game loop start
    TickEnd, // END TICK — frame pacing
    Tilemap, // TILEMAP — tilemap engine
    Transform, // Per SPRITE TRANSFORM
    SpriteLoad,
    SpriteSetPosition,
    SpriteOn,
    SpriteOff,
    SpritePriority,
    SpriteTransform,
    SpriteSetMatrix,
    SpriteGetPixels, // SPRITE GETPIXELS id → array IMAGEGET con -1 per trasparente
    Collide, // COLLIDE(id1[, id2]) → collision check AABB (namespace GRAPHICS)
    CollideBatch, // COLLIDEBATCH(X(), Y(), w, h) → int[] coppie (i,j) in collisione (broad+narrow phase)
    SpriteCollisionEvent,  // ON SPRITE COLLISION <handler>
    SpriteCollisionsEvent, // ON SPRITE COLLISIONS <handler>

    // --- TILEMAP Sub-commands ---
    TilemapLoad,
    Offset, // subcomando OFFSET per TILEMAP (keyword contestuale)
    TilemapSet,
    TilemapOffset,
    TilemapScroll,
    TilemapDraw,

    // --- SPRITEDEF Keywords ---
    Spritedef,
    EndSpritedef,
    Start, // subcomando START per SPRITEDEF (keyword contestuale)

    // --- Event Keywords ---
    Window,         // WINDOW
    DoEvents,       // DOEVENTS (VB6: processa i messaggi pendenti e ritorna)
    Viewport,       // VIEWPORT (finestra coordinate logiche, nel namespace WINDOWS)
    Resize,         // RESIZE
    Quit,           // QUIT
    Click,          // CLICK
    DblClick,       // DBLCLICK
    Scroll,         // SCROLL
    Up,             // UP
    Down,           // DOWN
    
    // --- NUOVI TOKEN PER MOUSE ---
    Mouse,
    Hide,
    Limit,
    Unlimit,
    MouseX,
    MouseY,
    MouseButton,
    MouseWheel,
    MouseClick,
    MouseDblClick,
    MouseMove,
    MousePresent,
    MouseLeft,
    MouseRight,
    MouseMiddle,

    // --- TOKEN PER JOYSTICK/GAMEPAD ---
    Joy,            // JOY(port%) → bitmask
    JoyCount,       // JOYCOUNT → numero joystick
    JoyName,        // JOYNAME$(port%) → nome joystick
    JoyPresent,     // JOYPRESENT(port%) → bool
    
    // --- NUOVI TOKEN PER PLOTTING ---
    Chart, // La keyword 'CHART'
    Financial, // Sottocomando FINANCIAL
    Show, // Sottocomando SHOW
    Plot, // Sottocomando PLOT
    ChartCreate, // Sottocomando CREATE
    ChartPlot, // Sottocomando PLOT
    ChartClear, // Sottocomando CLEAR
    ChartClose, // Sottocomando CLOSE
    ChartSave, // Sottocomando SAVE
    ChartShow, // Sottocomando SHOW

    // --- Comandi LINALG.MATRIX ---
    LinAlgMatrixIdentityCommand, // LINALG.MATRIX.IDENTITY matrixId
    LinAlgMatrixTranslateCommand, // LINALG.MATRIX.TRANSLATE matrixId, x, y, z
    LinAlgMatrixRotateCommand, // LINALG.MATRIX.ROTATE matrixId, ax, ay, az
    LinAlgMatrixScaleCommand, // LINALG.MATRIX.SCALE matrixId, sx, sy, sz
    LinAlgMatrixMultiplyCommand, // LINALG.MATRIX.MULTIPLY m1, m2, result
    LinAlgMatrixPushCommand, // LINALG.MATRIX.PUSH matrixId
    LinAlgMatrixPopCommand, // LINALG.MATRIX.POP matrixId
    LinAlgMatrixSetWorldCommand, // LINALG.MATRIX.WORLD matrixId
    LinAlgMatrixSetViewCommand, // LINALG.MATRIX.VIEW matrixId
    LinAlgMatrixSetProjectionCommand, // LINALG.MATRIX.PROJECTION matrixId

    // --- Comandi LINALG.VECTOR ---
    LinAlgVectorAddCommand, // LINALG.VECTOR.ADD dest(), source_or_scalar
    LinAlgVectorSubCommand, // LINALG.VECTOR.SUB dest(), source_or_scalar
    LinAlgVectorMulCommand, // LINALG.VECTOR.MUL dest(), source_or_scalar
    LinAlgVectorDivCommand, // LINALG.VECTOR.DIV dest(), source_or_scalar
    LinAlgVectorSetCommand, // LINALG.VECTOR.SET dest(), scalarValue
    LinAlgVectorFillRandomCommand, // LINALG.VECTOR.FILL RANDOM dest(), min, max
    LinAlgVectorWrapCommand, // LINALG.VECTOR.WRAP dest(), min, max
    LinAlgVectorAddWrapCommand, // LINALG.VECTOR.ADD WRAP dest(), source(), min, max
    LinAlgVectorBounce2dCommand, // LINALG.VECTOR.BOUNCE2D x_pos(), y_pos(), x_vel(), y_vel(), x_min, y_min, x_max, y_max

    // --- Funzioni LINALG (Creazione e Calcolo su Handle) ---
    LinAlgMatrixCreateFunc, // Per LINALG.MATRIXCREATE()
    LinAlgVectorCreateFunc, // Per LINALG.VECTORCREATE(...)
    LinAlgTransposeFunc, // LINALG.TRANSPOSE(matrixId) (funzione)
    LinAlgInverseFunc, // LINALG.INVERSE(matrixId) (funzione)
    LinAlgMultiplyFunc, // LINALG.MULTIPLY(handle1, handle2) (funzione)
    LinAlgLuFunc, // LINALG.LU() (funzione)
    LinAlgQrFunc, // LINALG.QR() (funzione)
    LinAlgSvdFunc, // LINALG.SVD() (funzione)
    LinAlgEvdFunc, // LINALG.EVD() (funzione)
    LinAlgCholeskyFunc, // LINALG.CHOLESKY() (funzione)
    LinAlgDecompGetFactorFunc, // LINALG.FACTOR(decompHandle, factorName$) (funzione)

    // --- Comandi per Vettori (Padre e Sotto-keywords) ---
    Vector, // Keyword 'VECTOR'
    Rag, // Keyword 'RAG' (nuova superficie del dominio RAG; `AI RAG` resta alias deprecato)
    Ensure, Collection, Collections, Text, Meta, Source, Purge, Deprecated, Documents, GenTest,
    Add, // Keyword 'ADD' (sottocomando per VECTOR ADD)
    Enable, // Keyword 'ENABLE' (sottocomando MCP SERVER ENABLE)
    Disable, // Keyword 'DISABLE' (sottocomando MCP SERVER DISABLE)
    Stream, // Keyword 'STREAM' (AI AGENT ASK STREAM)
    Mul, // Keyword 'MUL' (sottocomando per VECTOR MUL)
    Div, // Keyword 'DIV' (sottocomando per VECTOR DIV)
    Fill, // Keyword 'FILL' (sottocomando per VECTOR FILL RANDOM)
    Bounce2d,         // Keyword 'BOUNCE2D' (sottocomando per VECTOR BOUNCE2D)

    // --- MATRIX (Padre e Sotto-keywords) ---
    Matrix, // La keyword 'MATRIX'

    Identity,     // Per MATRIX IDENTITY (keyword contestuale)
    Translate,    // Per MATRIX TRANSLATE (keyword contestuale)
    Rotate,       // Per MATRIX ROTATE (keyword contestuale)
    Scale,        // Per MATRIX SCALE (keyword contestuale)
    Push,         // Per MATRIX PUSH (keyword contestuale)
    Pop,          // Per MATRIX POP (keyword contestuale)
    World,        // Per MATRIX WORLD (keyword contestuale)
    View, // Per MATRIX VIEW
    Projection,   // Per MATRIX PROJECTION (keyword contestuale)

    Sound, // Comando per riprodurre un suono
    Play, // Comando per riprodurre una melodia
    Concurrent, // Modificatore per PLAY
    Times, // Modificatore per PLAY LOOP
    Pause, // Modificatore per SOUND (es. SOUND PAUSE)
    Volume, // Per SOUND VOLUME

    // --- NUOVI KEYWORD PER I CANALI ---
    Send,
    Receive,
    From,

    // --- NUOVI KEYWORD PER RETE ---
    Tcp,
    Ssl,
    Accept,
    Recv,
    Udp,
    Connect,
    Shutdown,
    SetOption,

    // --- NUOVI KEYWORD PER WEB SERVER ---
    Http,
    Use,
    Install,
    Route,
    Server,
    Config,

    // --- NUOVI KEYWORD PER I/O SU FILE ---
    Open,
    Close,
    Get,
    Put,
    Seek,
    Kill,
    Rename,
    Write,
    WriteLine,
    Append,
    Random,
    Binary,
    Lock,
    Access,
    ReadWrite, // Per ACCESS READ WRITE
    Shared,
    Output,
    Watch,
    Note, Forget,
    Unwatch,
    Uninstall,
    
    // --- MEMORY MAPPED FILES ---
    MemoryMap,
    MemoryMapPeek,
    MemoryMapPoke,
    MemoryMapReadBytes,
    MemoryMapWriteBytes,

    // --- FUNZIONI DI I/O A BASSO LIVELLO (Stile Clipper) ---
    FOpen,
    FCreate,
    FRead,
    FWrite,
    FSeek,
    FClose,
    FError,

    // --- KEYWORDS SPECIFICI PER ISAM & DBF ---
    Begin,
    Transaction,
    Commit,
    Rollback,
    Level,
    Committed,
    Repeatable,
    Serializable,
    Required,
    Dbf,
    File,
    Folder,
    Schema,
    Strict,
    Isam,
    Indexed,
    By,
    Top,
    Bottom,
    Skip,
    Delete,
    Recall,
    Pack,
    Reindex,
    Repair,
    Backup,
    Record,
    Rest,
    Unlock,
    Order,
    Reprocess,
    Timeout,
    With,
    Zap,
    Create,
    Drop,
    Field,
    Index,
    Alias,
    Map,
    Blank, // Per APPEND BLANK
    Relation, // Per SET RELATION
    Into, // Per SET RELATION TO ... INTO ...
    Filter,
    Fields,
    Via,  // Per RDD USE file VIA driver ALIAS x
    Exact, // Per SET EXACT
    Total, // Per TOTAL ON
    Sort,
    Update, // Per UPDATE ON
    Default, // Per SET DEFAULT
    Path, // Per SET PATH
    Exclusive, // Per SET EXCLUSIVE
    Structure, // Per COPY STRUCTURE TO
    Report,         // Per REPORT FORM
    Form,           // Per REPORT FORM
    Databases,
    All,
    Export,
    Csv,
    Convert, // Per DBF CONVERT TO
    Encoding,
    Unsafe,
    Checked,
    TBrowse,
    TBColumn,

    // --- NUOVI TOKEN PER SPEED ---
    Speed, // La keyword principale "SPEED"
    Refresh, // Per REFRESH VIEW
    Using, // Per la clausola USING in SEEK
    Group,
    Having,

    // --- NUOVE FUNZIONI DI AGGREGAZIONE SQL ---
    Sum,
    Avg,

    // --- Nuovi Token per il Sottosistema Ledger ---
    Ledger, // La keyword principale "LEDGER"
    Rdd,
    Edit, // Il sub-comando "EDIT"
    Balance, // Il sub-comando "BALANCE"
    Register, // Il sub-comando "REGISTER"
    Accounts, // Il sub-comando "ACCOUNTS"
    Validate, // Il sub-comando "VALIDATE"
    Budget, // Il sub-comando "BUDGET"
    Desc, // La keyword "DESC" dentro LEDGER ADD
    Where, // La clausola "WHERE" per le query

    // --- NUOVI TIPI DI DATO PER LA CONCORRENZA ---
    Waitgroup,
    Channel,
    Of,
    Tag, // INDEX ON ... TAG tagname OF file.mdx (compound xBase)

    // --- NUOVI COMANDI FILESYSTEM ---
    MkDir,
    RmDir,
    ChDir,
    Copy,
    Move,
    Erase,
    XCopy,

    // --- ALTRI COMANDI ---
    Randomize,
    Run,

    // ========================================================================
    // --- Keyword: Operatori Logici ---
    // ========================================================================
    And,
    Or,
    Xor,
    Not,
    BitAnd,      // &&
    BitOr,       // ||
    BitXor,      // ^^
    BitNot,      // !
    True,
    False,
    Nil, // Per il valore NIL

    // ========================================================================
    // --- Keyword: Funzioni e Variabili di Sistema ---
    // ========================================================================

    // --- Funzioni Conversione ---
    CInt,
    Val,
    CSng,
    CDbl,
    CCur,
    CLng,
    CByte,
    CStr,
    DirectCast,
    Fix,
    Floor,
    Round,
    Trunc,
    Ceiling,
    Hex,
    Oct,
    Bin,
    CType,

    // --- Funzioni Matematiche ---
    IsNumeric,
    Abs,
    Asc,
    Atn,
    Chr,
    Cos,
    Exp,
    Len,
    Log,
    Rnd,
    Sgn,
    Sin,
    Sqr,
    Str,
    Tan,
    Sinh,
    Cosh,
    Tanh,
    // Nucleo scipy.special (SPECIAL namespace) — nomi con prefisso per evitare collisioni
    // con TokenKind.Gamma/Beta (FINANCE: opzioni greche).
    SpecialGamma,
    SpecialLGamma,
    SpecialBeta,
    SpecialErf,
    SpecialErfc,
    SpecialGammaInc,
    SpecialGammaIncc,
    SpecialBetaInc,
    SpecialDigamma,
    SpecialBesselJ,
    SpecialBesselY,
    SpecialBesselI,
    SpecialBesselK,
    SpecialBetaLn,
    SpecialErfInv,
    SpecialErfcInv,
    SpecialGammaIncInv,
    SpecialGammaInccInv,
    SpecialBetaIncInv,
    SignalConvolve,
    SignalCorrelate,
    SignalHann,
    SignalHamming,
    SignalBlackman,
    SignalButter,      // Design filtro Butterworth → handle (b, a)
    SignalCheby1,      // Design filtro Chebyshev tipo I → handle
    SignalCheby2,      // Design filtro Chebyshev tipo II → handle
    SignalEllip,       // Design filtro ellittico (Cauer) → handle
    SignalLfilter,     // Applica filtro IIR causale (direct form II transposed)
    SignalFiltfilt,    // Filtro zero-phase (filtfilt)
    SignalFreqz,       // Risposta in ampiezza |H| a n punti
    MatrixExp,
    MatrixKron,
    VectorManhattan,
    VectorChebyshev,
    VectorCosineDistance,
    VectorMinkowski,
    KdTreeCreate,       // Costruisce un kd-tree dai punti → handle (scipy.spatial.KDTree)
    KdTreeQuery,        // k-NN: indici dei k punti più vicini
    KdTreeQueryDist,    // k-NN: distanze dei k punti più vicini
    KdTreeQueryBall,    // punti entro raggio r (query_ball_point)
    LBound,
    UBound,
    Log10,
    Asin,
    Acos,
    Rad2Deg,
    Deg2Rad,
    Pi,
    Min,
    Max,
    Clamp,
    Hypot,
    Dist2d,
    Dist3d,
    PolarX,
    PolarY,
    Angle2d,
    Pythagoras,
    Fact,
    Comb,
    Perm,

    // --- NUOVE FUNZIONI STATISTICHE ---
    // --- Distribuzioni Continue ---
    NormalPdf,
    NormalCdf,
    LognormalPdf,
    LognormalCdf,
    ChiPdf,
    ChiCdf,
    ChiSquaredPdf,
    ChiSquaredCdf,
    BetaPdf,
    BetaCdf,
    StudentTPdf,
    StudentTCdf,
    WeibullPdf,
    WeibullCdf,
    FPdf,
    FCdf,
    ExponentialPdf,
    ExponentialCdf,
    ParetoPdf,
    LogisticPdf,
    TriangularPdf,
    UniformPdf,
    SkewedNormalPdf,
    // Inverse CDF (ppf) e ISF — quantili e valori critici (scipy.stats)
    NormalPpf, NormalIsf, LognormalPpf, ChiSquaredPpf, ChiSquaredIsf, ChiPpf,
    BetaPpf, FPpf, FIsf, StudentTPpf, StudentTIsf, ExponentialPpf, ExponentialIsf,
    WeibullPpf, LogisticPpf, UniformPpf, ParetoPpf, TriangularPpf,

    // ==========================
    // FUNZIONI STATISTICHE DESCRITTIVE DI BASE
    // ==========================
    Mean,             // Media aritmetica
    Median,           // Mediana
    Variance,         // Varianza campionaria
    StdDev,           // Deviazione standard campionaria
    Skewness,         // Skewness (asimmetria)
    Kurtosis,         // Kurtosis (appiattimento)
    Correlation,      // Correlazione di Pearson
    Percentile,       // Percentile (es: 95° percentile)
    Mad,              // Median Absolute Deviation
    
    // ==========================
    // FUNZIONI STATISTICHE AVANZATE / SERIE TEMPORALI / MACHINE LEARNING
    // ==========================
    Anova,                // Analisi della varianza
    Arima,                // Modello ARIMA per serie temporali
    Regress,              // Regressione lineare/multipla
    KMeans,               // Clustering K-means
    HoltWinters,          // Predizione serie temporali con Holt-Winters
    AdfTest,              // Test di Dickey-Fuller aumentato (stazionarietà)
    Pca,                  // Analisi delle Componenti Principali (PCA)
    InterpolateLinear,    // Interpolazione lineare
    InterpolateSpline,    // Interpolazione spline cubica (Akima)
    InterpolatePolynomial,// Interpolazione polinomiale N-1 (baricentrica efficiente)
    InterpolatePchip,     // Interpolazione PCHIP
    InterpolateStep,      // Interpolazione a gradino (zero-order hold)
    InterpolateClamp,     // Interpolazione con clamp ai bordi (tipo selezionabile)
    InterpolateGeneric,   // Interpolazione generica su tipo
    Integrate,            // Integrazione numerica (trapezi, Simpson)
    Differentiate,        // Derivazione numerica
    Fft,                  // Trasformata di Fourier veloce (FFT)
    Ifft,                 // Trasformata inversa di Fourier veloce (IFFT)
    FftComplex,           // FFT su dati reali + immaginari (due array)
    IfftComplex,          // IFFT su dati reali + immaginari (due array)
    Simpson,              // Integrazione con Simpson composita (fn, a, b, n)
    Trapezoid,            // Integrale trapezoidale di un campionamento (y, x)
    CumTrapezoid,         // Integrale trapezoidale cumulativo (y, x)
    SolveOde,             // Solutore ODE del 1° ordine (fn(t,y), y0, t0, t1, steps)
    CubicSpline,          // Spline cubica not-a-knot (default scipy CubicSpline)
    FftFreq,              // Frequenze discrete della FFT (scipy fftfreq)
    FftShift,             // Scosta la frequenza zero al centro (scipy fftshift)
    IfftShift,            // Inverso di FftShift (scipy ifftshift)

    // Media mobili
    MovingAverage,
    EMA,
    TsLowPass,        // Filtro lowpass Butterworth su serie (wrapper SIGNAL)
    TsHighPass,       // Filtro highpass Butterworth su serie
    TsBandPass,       // Filtro bandpass Butterworth su serie
    TsBandStop,       // Filtro bandstop Butterworth su serie

    // Statistiche rolling/mobili
    RollingStdDev,
    RollingMad,
    RollingZScore,
    RollingAutocorrelation,
    RollingCovariance,
    RollingCorrelation,
    RollingRegression,
    RollingDelta,

    // Analisi avanzata serie storiche
    Outliers,
    Difference,
    Returns,
    Volatility,
    Autocorrelation,
    Drawdown,
    Deseasonalize,
    Decompose,
    
    // --- NUOVE FUNZIONI LINALG ---
    Solve,
    Lu,
    Qr,
    Svd,
    Evd,
    Cholesky,

    // --- Distribuzioni Discrete ---
    BernoulliPmf,
    BinomialPmf,
    PoissonPmf,
    GeometricPmf,
    NegativeBinomialPmf,
    HypergeometricPmf,

    // --- Funzioni Finanziarie di Base e Avanzate ---
    Pmt, // Calcolo rata prestito (PMT)
    PmtExt, // Calcolo rata prestito avanzata (PMT_EXT)
    Fv, // Valore futuro (FV)
    FvExt, // Valore futuro avanzato (FV_EXT)
    Pv, // Valore attuale (PV)
    PvExt, // Valore attuale avanzato (PV_EXT)
    Nper, // Numero periodi (NPER)
    NperExt, // Numero periodi avanzato (NPER_EXT)

    Npv, // Valore attuale netto (NPV)
    Irr, // Tasso interno di rendimento (IRR)
    Mirr, // Tasso interno di rendimento modificato (MIRR)
    Mnpv, // NPV modificato con reinvestimento (MNPV)
    Xnpv, // NPV su flussi irregolari (XNPV)
    Xirr, // IRR su flussi irregolari (XIRR)

    Sln, // Ammortamento quota costante (SLN)
    Syd, // Ammortamento somma anni (SYD)
    Db, // Ammortamento bilancio declinante (DB)
    Ddb, // Ammortamento doppio bilancio (DDB)

    Cumipmt, // Interessi cumulati tra periodi (CUMIPMT)
    Cumprinc, // Capitale cumulato tra periodi (CUMPRINC)

    Price, // Prezzo obbligazione (PRICE)
    Duration, // Durata obbligazione (DURATION)
    Rate, // Tasso implicito (RATE)

    Days360, // Giorni finanziari (DAYS360)
    YearFrac, // Frazione di anno finanziaria (YEARFRAC)

    // --- NUOVE FUNZIONI PER TITOLI E OPZIONI ---
    BondPrice,
    BondYield,
    BsCall,
    BsPut,
    MertonCall,
    MertonPut,
    BinomialOption,
    Delta,
    Gamma,
    Theta,
    Vega,
    Rho,
    Vanna,
    Volga,
    Charm,
    Zomma,
    DeltaSpeed,
    GammaColor,
    

    // --- Namespace TRADING.PORTFOLIO (Funzioni di Ottimizzazione) ---
    MeanVariance,
    MinVolatility,
    MaxSharpe,
    BlackLitterman,
    MinSemiVariance,
    MinCvar,
    MinCdar,
    KellyCriterion,
    RiskParity,
    Evolve,
    EquallyWeighted,
    MaxReturn,
    PortfolioVar,         // Value-at-Risk portafoglio
    PortfolioCVar,        // Conditional Value-at-Risk portafoglio (Expected Shortfall)
    PortfolioMad, // Linear Optimization by Median Absolute Deviation portafoglio
    PortfolioSemiMad, // Linear Optimization by Median Absolute Deviation portafoglio
    IsLongOnly,  // Verifica se il portafoglio è long-only
    
    // --- Namespace TRADING.PORTFOLIO (Funzioni di Calcolo Base) ---
    PortfolioExpectedReturn,
    PortfolioStDev,
    PortfolioSharpeRatio,

    // --- Namespace TRADING.METRICS (Funzioni di Analisi) ---
    TotalReturn,
    AnnualizedReturn,
    SharpeRatio,
    SortinoRatio,
    MaxDrawdown,
    CalmarRatio,
    InformationRatio,
    Alpha,
    Beta,
    TreynorRatio,
    ValueAtRisk,
    ConditionalValueAtRisk,
    DownsideDeviation,
    ReturnsSkewness,
    ReturnsKurtosis,
    JarqueBeraTest,
    AnnualizedVolatility,
    AnnualizedTrackingError,
    AnnualizedDownsideDeviation,
    OmegaRatio,
    UlcerIndex,

    // --- NUOVE FUNZIONI PER AZIONI E VALUTAZIONI ---
    BookValuePerShare,
    EPS,
    DilutedEPS,
    DPS,
    EquityMultiplier,
    NAV,
    StockValueZeroGrowth,
    StockValueConstGrowth,
    PreferredStockValue,
    PERatio,
    PSRatio,
    PBRatio,
    DividendPayoutRatio,
    CAPM,
    RiskPremium,
    TotalStockReturn,
    CapitalGainsYield,
    DividendYield,
    HoldingPeriodReturn,
    GeometricMeanReturn,
    GrowthRate,
    BidAskSpread,
    BondEquivalentYield,
    CurrentYield,
    TaxEquivalentYield,
    YieldToMaturity,
    ZeroCouponBondValue,
    ZeroCouponBondYield,

    // --- Funzioni di Algebra Lineare (per namespace MATRIX e VECTOR) ---
    // Nota: CREATE e MULTIPLY sono gestiti come Identifier dal parser
    // dopo un punto, quindi non servono nuovi token per MATRIX.CREATE, etc.
    // Servono solo token per i nomi di funzione non in conflitto.
    Determinant,
    Transpose,
    Norm,
    DotProduct,

    // --- Funzioni Data ---
    StrTime,
    DateAdd,
    DateDiff,
    Eomonth,
    Weekday,
    Dow,
    Seconds,
    Cdow,
    Cmonth,
    Ctod,
    Dtoc,
    DateValue,
    DateSerial,
    TimeSerial,
    Year,
    Month,
    Day,
    Hour,
    Minute,
    Second,
    IsDate,
    Now,
    Today,
    DaysInMonth,
    IsLeapYear,
    FirstDayOfMonth,
    IsoWeekOfYear,
    DateFromUnix,
    UnixTimestamp,
    
    // --- Funzioni Stringa ---
    Format,
    Left,
    Right,
    Mid,
    Instr,
    Dup,
    Space,
    RepeatFunc,
    Lower,
    Upper,
    Pascal,
    Camel,
    Snake,
    Kebab,
    Title,
    Sentence,
    Trim,
    LTrim,
    RTrim,
    Replace,
    Reverse,
    Range,
    Wrap,
    Split,
    Join,
    Search,
    Count,
    Remove,
    Contains,
    StartsWith,
    EndsWith,
    Petscii,
    PadL,
    PadR,
    PadC,
    Tab,

    // --- FUNZIONI CRITTOGRAFICHE ---
    Md5,
    Sha1,
    Sha256,
    Sha512,
    RipeMd160,
    Keccak256,
    HashFile,
    ChaCha20Encrypt,
    ChaCha20Decrypt,
    ChaCha20Poly1305Encrypt,
    ChaCha20Poly1305Decrypt,
    // ---HASHING DI PASSWORD ---
    PasswordHash,
    PasswordVerify,
    
    // NUOVI TOKEN PER LE VERSIONI STRINGA ($)
    StrMd5,
    StrSha1,
    StrSha256,
    StrSha512,
    StrRipeMd160,
    StrKeccak256,
    StrHashFile,

    // ========================================================================
    // --- NUOVI TOKEN PER CRITTOGRAFIA E UTILITY ---
    // ========================================================================
    // Cifratura Simmetrica
    AesEncrypt,
    AesDecrypt,

    // Gestione Chiavi Asimmetriche
    KeyGen,
    KeyExport,
    KeyImport,
    KeyClose,

    // Firma Digitale
    Sign,
    Verify,
    VerifyRaw,   // VERIFYRAW(firma, dati, chiavePubblicaB64): verifica Ed25519 con chiave raw

    // Cifratura Asimmetrica
    EncryptAsym,
    DecryptAsym,

    // Utility
    RandomBytes,
    ToBase64,
    FromBase64,
    ToHex,
    FromHex,

    // Security (2026-09-02): HMAC + base64url (JWT) — coda, mai rinumero
    HmacSha256,
    HmacSha384,
    HmacSha512,
    ToBase64Url,
    FromBase64Url,
    // Security encoding (2026-09-02, STRINGS): HTML/URL encode — coda, mai rinumero
    HtmlEncode,
    UrlEncode,
    JsEncode,
    // Validation (2026-09-02, STRINGS) — coda
    IsRequired,
    IsEmail,
    IsUrl,
    IsCreditCard,
    IsLength,
    // Security JWT (2026-09-02, AUTH) — coda, mai rinumero
    JwtCreate,
    JwtVerify,
    JwtPayload,
    // Upload security (2026-09-02, HTTP) — coda
    SafeFileName,
    DetectContentType,
    SessionCreate,

    // Funzioni randomistiche (NUOVI TOKEN)
    RandomInt,
    RandomDouble,
    RandomNormal,
    RandomCryptoBytes,
    
    // --- Funzioni Blockchain ---
    Blake2b256,
    Base58Encode,
    Base58Decode,
    Base32Encode,
    Base32Decode,
    RlpEncode,
    VarIntEncode,
    VarIntDecode,
    EthAddressFromPub,
    EthAddressChecksum,
    BtcAddressFromPub,
    BlockDifficulty,
    EcdsaSecp256k1Sign,
    GenerateMnemonic,
    MnemonicToSeed,
    DeriveKey,
    GetPublicKey,
    Contract,
    AbiEncode,
    AbiDecode,
    
    // ========================================================================
    // --- NUOVE KEYWORD E FUNZIONI PER ARRAY (Stile Clipper) ---
    // ========================================================================
    AAdd,
    AIns,
    ADel,
    ASort,
    AClone,
    AScan,
    AMap,
    AFilter,
    AReduce,
    AUnique,
    AEval,
    AFill,
    ACopy,
    ATokens,
    ACount,
    AShuffle,
    AReverse,
    ABinarySearch,
    AJoin,
    AMax,
    AMin,
    ASum,
    AChunk,
    AFlat,
    APad,
    AZip,

    // --- NUOVE KEYWORD E FUNZIONI PER ARRAY (Stile CLIPPER) ---
    Empty,
    Iif,
    Choose,
    Dtos,
    Stod,

    // --- NUOVE FUNZIONI PER I/O SU FILE ---
    EofFunc,
    Lof,
    FreeFile,
    FileInput,

    // --- NUOVE FUNZIONI DI RETE ---
    Conn,
    RemoteIp,
    RemotePort,
    Host,
    LocalPort,
    GetHostIp,
    Peek,
    Fetch,
    HttpGet,
    HttpPost,
    HttpPut,
    HttpDelete,

    // --- FUNZIONI ISAM ---
    Bof,
    Found,
    Recno,
    Lastrec,
    RLock,
    FLock,
    Selected,
    IsamIntegrity,
    IsamInfo,
    IsamLog,
    IsamAppendMany,
    IsamSetReplication,
    IsamApplyReplication,
    IsamFollowReplication,
    IsamReplicationShipTcp,
    IsamReplicationRecvTcp,

    // --- FUNZIONI DBF ---
    DbfAppendMany,
    DbfSetDurability,
    DbfFlushDurability,
    DbfBeginGroupCommit,
    DbfEndGroupCommit,
    DbfSetReplication,
    DbfApplyReplication,
    DbfFollowReplication,
    DbfReplicationShipTcp,
    DbfReplicationRecvTcp,

    // --- FUNZIONI SPEED ---
    SpeedSetDurability,
    SpeedFlushDurability,
    SpeedBeginGroupCommit,
    SpeedEndGroupCommit,
    SpeedSetMany,
    SpeedInfo,
    SpeedValidate,

    // --- SQL engine (5th embedded DB) ---
    Sql,
    SqlExec,
    SqlQuery,
    SqlAttach,
    SqlDetach,
    SqlFetch,
    SqlLastInsertRowId,
    SqlChanges,
    SqlCloseResultSet,
    SqlResultSet,
    ResultSet, // sub-comando SQL RESULTSET (keyword contestuale)
    Savepoint,

    // --- VECTORDB (database vettoriale autonoma su SQLite) ---
    VectorDb,

    // --- AIVECTOR (semantic search / RAG on SQLite) ---
    AiVector,
    AiVectorEmbed,
    AiVectorIndexText,
    AiVectorRagContext,
    AiVectorAsk,
    AiVectorEnsureCollection,
    AiVectorIndexMeta,
    AiVectorRemoveSource,
    AiVectorPurgeDeprecated,
    AiVectorCountEmbeddings,
    AiVectorSearch,
    AiVectorLoadDocuments,
    AiVectorListCollections,
    AiVectorRerank,
    AiVectorGenTest,
    AiCacheStats,

    // --- PROVIDER server (embedding / completion registry) ---
    Provider,
    ProviderRegister,
    ProviderRegisterLocal,
    ProviderConfigure,
    ProviderSetActive,
    ProviderRotate,
    ProviderActive,
    ProviderList,
    ProviderUnregister,
    ProviderSave,
    ProviderLoad,
    ProviderTest,
    ProviderSetPriority,
    ProviderPriority,
    ProviderMarkFailed,
    ProviderResetFailed,
    ProviderSetStatus,
    ProviderStatus,
    ProviderStartHealthCheck,
    ProviderStopHealthCheck,
    ProviderHealthCheckInterval,
    ProviderRunHealthCheck,
    ProviderSetWeight,
    ProviderWeight,
    ProviderDiscover,
    ProviderSetCredits,
    ProviderCredits,

    // --- TYPEINFO reflection (Onda 1) ---
    TypeInfo,
    TypeInfoHas,
    MethodInfo,
    PropertyInfo,

    // --- AI AGENT (fuso nel namespace AI) ---
    AiAgent,
    Agent,
    AiAgentCreate,
    AiAgentDelete,
    AiAgentAsk,
    AiAnalyzeImage,
    AiAgentSet,
    AiAgentGet,
    AiAgentSave,
    AiAgentLoad,
    AiAgentRegisterTool,
    AiAgentServe,
    AiAgentStopServer,
    AiAgentSend,
    AiAgentReceive,
    AiAgentPause,
    AiAgentResume,
    AiAgentStop,
    AiAgentQueueStats,
    AiAgentQueueClear,
    AiChannelCreate,
    AiChannelPublish,
    AiChannelSubscribe,
    AiChannelUnsubscribe,
    AiAgentCode,
    AiAgentFix,
    AiKgAddNode,
    AiKgAddEdge,
    AiKgQuery,
    AiAgentList,
    AiAgentStatus,
    AiAgentTrace,
    AiAgentEvents,

    // --- MCP (Onda 1) ---
    Mcp,
    McpRegisterTool,
    McpUnregisterTool,
    McpListTools,
    McpResources,
    McpPrompts,
    McpRegisterAgent,

    // --- MCP SERVER (lista client, prefisso AI) ---
    McpServerAdd,
    McpServerRemove,
    McpServerEnable,
    McpServerDisable,
    McpServerRefresh,
    McpServerList,
    McpServerStatus,
    McpServerMarketplaceList,
    McpServerMarketplaceGet,
    McpListToolsServer,
    McpCall,

    // --- MEMORY (Onda 3) ---
    MemoryStore,
    MemoryLoad,
    MemorySearch,
    MemoryDelete,
    MemoryList,
    Memory, MemoryWatch, MemoryImport, MemoryNote, MemoryRefresh, MemorySave, MemoryForget, MemoryAsk,
    McpServe,
    McpStop,

    // --- VAULT (Second Brain L1, W1) — struttura vault = folder tree .md ---
    Vault,
    AiVaultCreate,
    AiVaultIndex,
    AiVaultInfo,
    AiVaultFolders,
    AiVaultCount,
    AiVaultList,
    AiVaultWatch,
    AiVaultPolicy,
    AiVaultAddFolder,
    AiVaultRemoveFolder,
    AiVaultSetFolderProps,
    AiNoteLink,
    AiNoteUnlink,
    AiNoteSetAlias,
    AiSyncFolder,
    AiIngestWeb,
    AiVerify,
    AiEvalValidate,
    AiDecide,
    AiAskChain,
    Unlink,
    Web,
    Auto,
    Manual,

    // --- BRAIN (Second Brain L6, R6) — collegamento/import cervelli già costruiti ---
    Brain,
    AiBrainUse,
    AiBrainDetach,
    AiBrainInstall,
    AiBrainUninstall,
    AiBrainList,
    AiBrainInfo,
    AiBrainUsed,

    // --- BRAIN multi-use (R7) — query distribuita sui cervelli collegati ---
    AiSearchMulti,
    AiRagContextMulti,
    AiAskMulti,

    // --- GRAPH (namespace autonomo, R7) — grafi wiki multi-brain ---
    GraphQueryMulti,
    GraphNeighbors,
    GraphPath,
    GraphPageRank,
    GraphConnected,
    GraphStats,
    GraphStronglyConnected,
    GraphTopoSort,
    GraphBipartite,
    GraphBetweenness,
    GraphClustering,
    GraphDiameter,
    GraphMst,
    GraphDijkstra,
    GraphHasCycle,
    GraphEccentricity,
    GraphDfs,
    GraphBfs,
    GraphBuild,
    GraphUpdate,
    GraphFindPath,
    GraphRdd,
    GraphDot,
    GraphSubgraph,
    GraphByTag,
    GraphByFolder,
    GraphIsValid,
    Build,
    Find,

    // --- NOTE (Second Brain L2) — CRUD note .md dentro un vault ---
    NoteCreate,
    NoteUpdate,
    NoteAppend,
    NoteDelete,
    NoteMove,
    NoteGet,
    NoteTitle,
    NoteTags,
    NoteWordCount,
    NoteExists,
    NoteFolder,

    // --- LINK (Second Brain L3) — wikilink/backlink ---
    LinksOut,
    Backlinks,
    LinkDangling,
    LinkJson,

    // --- INGEST (Second Brain L6) — import file/folder in un vault ---
    Ingest,
    IngestFile,
    IngestFolder,

    // --- MEMORY RANK (Second Brain §3) — ranking mem0 ---
    MemoryRank,
    MemoryConsolidate,

    // --- ENRICH / QA (Second Brain §2) — LLM enrichment + grounded QA ---
    Enrich,
    QaAnswer,
    SuggestTags,
    SignNote,
    VerifyNote,
    ShareEncrypt,
    ShareDecrypt,
    BrainVaultBuild,
    BrainVaultQuery,
    BrainVaultWrite,

    // --- PROCESS (E0: documento → agente con prompt) ---
    Process,
    Prompt,

    // --- WORKFLOW (Onda 3) ---
    WorkflowStatement,
    WorkflowStep,
    WorkflowExecute,
    WorkflowList,
    WorkflowStatus,
    WorkflowSteps,

    // --- TEAM (Onda 3) ---
    TeamCreate,
    TeamAddAgent,
    TeamRemoveAgent,
    Message,
    TeamList,
    TeamAgents,
    Role,       // sub-comando SET ROLE (keyword contestuale)
    Strategy,   // sub-comando SET STRATEGY (keyword contestuale)
    Coordinator, // sub-comando SET COORDINATOR (keyword contestuale)

    // --- TEAM RecursiveMAS R1/R4 ---
    Recursion,  // SET RECURSION
    Latent,     // SET LATENT
    Link,       // SET LINK
    Thoughts,   // SET THOUGHTS
    Dataset,    // SET DATASET (per TRAIN)
    Train,      // TEAM TRAIN INNER|OUTER
    Inner,      // TEAM TRAIN INNER team, agent, dataset
    Outer,      // TEAM TRAIN OUTER team, dataset
    Dag,        // SET DAG d,l,n (AGoT 9.4)
    Complexity, // SET COMPLEXITY threshold (AGoT 9.4)
    Reflection, // SET REFLECTION mode (MAR/MIRROR 9.5)
    Critic,     // SET CRITIC personas (MAR 9.5; keyword anche per TRAIN ACTOR CRITIC)
    Prune,      // SET PRUNE topK (ReTreVal 9.5)
    Thinking,   // TRAIN THINKING (Quiet-STaR 9.3)
    Actor,      // TRAIN ACTOR CRITIC (DPSDP 9.6)
    Distill,    // TEAM DISTILL THINKING (Quiet-STaR 9.3)
    HfToken,    // AI SET HFTOKEN (marketplace HF)
    HfRepo,     // AI SET HFREPO / AI.HFREPO (marketplace HF)
    AiSetHfToken, AiSetHfRepo, AiHfRepo,
    AiSetOption,   // AI SET OPTION <nome>, <valore> (registro opzioni generico)
    AiOption,      // AI.Option(<nome>) — lettura voce registro opzioni
    TeamSetDag, TeamSetComplexity, TeamSetReflection, TeamSetCritic, TeamSetPrune,
    TeamTrainThinking, TeamTrainActorCritic, TeamDistillThinking,
    TeamTrainStatus, // AI.TEAMTRAINSTATUS(team) → JSON
    TeamLinkInfo,    // AI.TEAMLINKINFO(team) → JSON
    TeamRecursion,   // AI.TEAMRECURSION(team) → Long (r)
    TeamLatent,      // AI.TEAMLATENT(team) → String (mode)
    TeamRuntimeStats, // AI.TEAMRUNTIMESTATS(team) → JSON per round
    TeamRunStart, TeamRunWait,
    TeamOnStream,
    BlackboardWrite, BlackboardRead, BlackboardWait,
    AgentSwarm,
    // Orchestrazione agenti (A1): categorie di task → provider/modello. Runtime condiviso
    // Basic.AI.Providers.CategoryRegistry (C#) / gmb_category.c (C). Append-only.
    AiCategorySet, AiCategoryFallback, AiCategoryAuto, AiCategoryPick, AiCategoryList,
    // Orchestrazione agenti (B1): Task Graph con dipendenze (Basic.Agents.TaskGraphRegistry /
    // gmb_taskgraph.c). Append-only.
    AiTaskDefine, AiTaskDepends, AiTaskAssign, AiTaskStatus, AiTaskList, AiTaskRun,
    // Orchestrazione agenti (A2): Planner — piani con approvazione (Basic.Agents.PlanRegistry /
    // gmb_plan.c). Append-only.
    AiPlanDefine, AiPlanAdd, AiPlanApprove, AiPlanStatus, AiPlanList, AiPlanRun,
    // Orchestrazione agenti (A3): lifecycle hooks (Basic.Agents.HookRegistry / gmb_hook.c).
    AiHookRegister, AiHookUnregister, AiHookList, AiHookEvents, AiHookFire,
    // Orchestrazione agenti (A6): Artifact Store (Basic.Agents.ArtifactStore / gmb_artifact.c).
    AiArtifactPut, AiArtifactLink, AiArtifactGet, AiArtifactList, AiArtifactRemove,
    // Orchestrazione agenti (A7): Resource Manager (Basic.Agents.ResourceRegistry / gmb_resource.c).
    AiResourceSet, AiResourceGet, AiResourceList, AiResourceAvailable, AiResourceSwarmSize,
    // Orchestrazione agenti (A9): Handoff (Basic.Agents.HandoffRegistry / gmb_handoff.c).
    AiHandoff, AiHandoffComplete, AiHandoffStatus, AiHandoffList, AiHandoffClear,
    // Orchestrazione agenti (A5): Rules injection gerarchica (Basic.Agents.RuleRegistry / gmb_rules.c).
    AiRulesAdd, AiRulesFor, AiRulesList, AiRulesRemove, AiRulesClear,
    // Orchestrazione agenti (A4): Goal + idle continuation (Basic.Agents.GoalRegistry / gmb_goal.c).
    AiGoalSet, AiGoalGet, AiGoalStatus, AiGoalTick, AiGoalAgent, AiGoalClear,
    // Orchestrazione agenti (A8): Session Replay (Basic.Agents.ReplayRegistry / gmb_replay.c).
    AiReplayStart, AiReplayRecord, AiReplayStop, AiReplayGet, AiReplayList, AiReplayClear, AiReplayRun,
    // Orchestrazione agenti (B6): metriche fleet-level (Basic.Agents.MetricsRegistry / gmb_metrics.c).
    AiMetricRecord, AiMetricGet, AiMetricList, AiFleetStats, AiMetricReset,
    // Orchestrazione agenti (A10): Hashline (Basic.Agents.HashlineTool / gmb_hashline.c).
    AiHashLine, AiHashEdit, AiHashVerify,
    // Orchestrazione agenti (B7): checkpoint/recovery (Basic.Agents.CheckpointRegistry / gmb_checkpoint.c).
    AiCheckpoint, AiResume, AiCheckpointList, AiCheckpointClear,
    // Persistenza orchestrazione su disco (Basic.Agents.OrchestrationPersistence / gmb_orchestration.c).
    AiOrchSave, AiOrchLoad,
    // Reset dei registri (determinismo dei MiniTest).
    AiTaskClear, AiPlanClear,

    Used,
    DbFilter,
    DbRelation,
    IndexKey,
    IndexOrd,
    Deleted,
    FieldName,
    LUpdate,
    RecSize,
    NetErr,
    DbfLookup,
    Aggregate,        // Per la keyword AGGREGATE
    Compute,          // Per la keyword COMPUTE

    // --- NUOVE FUNZIONI PER IL FILESYSTEM ---
    CwdFunc,
    FileExistsFunc,
    DirExistsFunc,
    GetFolderPathFunc,
    NormalizePathFunc,
    FolderSizeFunc,
    DirFunc,
    GetTempFileFunc,
    GetTempDirFunc, 
    AppPathFunc,         // Per APPPATH()
    BinPathFunc,         // Per BINPATH()
    ProjectFolderFunc,   // Per PROJECTFOLDER()
    ParentDirFunc,       // Per PARENTDIR()
    ReadBytesFunc,       // Per READBYTES()
    WriteBytesFunc,      // Per WRITEBYTES(path, bytes)
    TtRenderFunc,        // Per FONTS.TTRENDER(path, size, codepoint)
    TtMetricsFunc,       // Per FONTS.TTMETRICS(path, size, codepoint)
    TtGlyphsFunc,        // Per FONTS.TTGLYPHS(path)
    ReadLinesFunc,       // Per READLINES()
    ReadFileFunc,        // Per READFILE()
    CountLinesFunc,      // Per COUNTLINES()
    ResolveFilePathFunc, // Per RESOLVEFILEPATH()
    
    // --- NUOVE FUNZIONI GRAFICHE E DI LETTURA SCHERMO ---
    Rgb,
    PixelBuffer,
    Buffer,   // PIXEL BUFFER (comando legacy, token separato — GRAFX-03, §2.0)
    TextWidth,
    TextHeight,
    Point,
    Cell,
    CellStr,
    Screens,
    ScreenInfo,

    // --- Variabili di Sistema ---
    Ti,
    StrTi,
    Sys,
    SizeOf, // Per la funzione SIZEOF()
    VarPtr,
    StrPtr,
    Packed,    // KEYWORD: packed TYPE declaration
    UdtPacked,
    UdtAligned,
    
    TypeName,
    ValType,

    // JSON
    Json,
    Parse,
    Load,
    Stringify,
    Save,
    Keys,
    ObjectToJson,
    JsonToObject,
    ReadValue,
    ReadSection,
    WriteValue,
    WriteSection,
    Extract,

    // ON KEY
    Key,
    Stop,
    Make,

    // --- ASYNC/AWAIT ---
    Async,
    Await,

    // --- WITH STATEMENT ---
    EndWith,

    // --- USING STATEMENT ---
    EndUsing,

    // --- TICK STATEMENT ---
    EndTick,

    // --- NULLABLE TYPES ---
    Has,
    Value,

    // --- TEXT ALIGNMENT ---
    AlignLeft,
    AlignCenter,
    AlignRight,

    // --- SCREEN FULLSCREEN / VIEW PRINT / INPUT MASK ---
    Desktop,
    Windowed,
    ViewPrint,
    Mask,

    // --- COLLECTION METHODS (GMBasicAI sync) ---
    Enqueue,
    Dequeue,
    StackPush,
    StackPop,
    ToArray,
    ToList,
    ToDict,
    Insert,
    AddRange,
    TryGetValue,
    First,
    Last,
    IndexOf,
    GetRange,
    RemoveAt,

    // --- RDD functions ---
    RddGetField,
    RddSetField,
    RddFLock,
    RddRLock,
    RddToday,
    RddCurval,
    RddCtod,
    RddDtoc,

    CommandCount,
    Command,

    // Window Management
    Activate,
    Deactivate,
    Minimize,
    Shadow,
    Pixel,
    Release,
    Focus,
    Paste,
    Frame,
    Rounded,
    // Funzioni lettura finestra (WINDOWS.X/Y/WIDTH/HEIGHT/THEME/DESKTOP)
    WindowX,
    WindowY,
    WindowWidth,
    WindowHeight,
    WindowTheme,
    WindowDesktop,

    // Dialogs
    Alert,
    Confirm,

    // Standalone controls
    ListBox,
    ComboBox,
    CheckBox,
    Spinner,
    Menu,

    // More standalone controls
    StatusBar,
    FileDialog,
    PrinterDialog,
    Toggle,
    RadioGroup,
    Slider,
    NumericUpDown,
    StarRating,
    ScrollBar,
    ScrollView,
    LabelControl,
    DataList,
    PropertyGrid,
    LogView,
    HexView,
    BigText,
    CodeArea,
    ReorderListBox,
    AlertBox,
    ConfirmationBox,
    FileExplorer,
    Dashboard,
    Splitter,
    ContextMenu,
    Grid,
    DatePicker,
    ColorPicker,
    TreeView,
    Bell,
    Theme,
    FgRgb,
    BgRgb,
    DateFormat,
    Century,

    Zip,
    Unzip,
    Compress,
    Decompress,
    Gzip,
    Ungzip,
    Brotli,
    Zlib,
    Bzip2,
    Deflate,
    Algorithms,

    Unbrotli,
    Unzlib,
    Unbzip2,
    Undeflate,

    // Compressione algoritmi aggiunti (2026-10-10): zstd/lz4/xz.
    Zstd,
    Unzstd,
    Lz4,
    Unlz4,
    Xz,
    Unxz,

    RddBuffer,
    RddTableRevert,
    RddTableUpdate,
    RddOldVal,
    RddGetFieldState,
    RddSetDeleted,
    RddFloat,
    RddValType,
    RddDeleteScoped,
    RddRecallScoped,
    RddReplaceScoped,
    RddSetLockTimeout,

    // Comandi CONSOLE ibridi (2026-08-06): TUI/console.
    SetWindowTitle,
    GetCursorPos,
    GetTextMetrics,
    GetPaletteColor,

    // FINANCE varianti (2026-08-06): PRICESIMPLE (PmtExt/FvExt/PvExt/NperExt gia' presenti;
    // YIELD riusa TokenKind.Yield).
    PriceSimple,

    // SET legacy Clipper (2026-08-06): attributi non ancora esposti (Wrap esisteva; Message usa TeamMessage).
    Escape,
    Delimiters,
    Typeahead,
    Intensity,
    Epoch,
    Decimals,
    Fixed,
    Printer,
    Device,
    InkeyDollar,

    // Convert + AI commands (2026-08-07)
    ConvDoc, ConvHtml, ConvText, CvtBatch, WebToMd, ConvFormats,
    DocOpen, DocClose, DocPageCount, DocText, DocMarkdown, DocHtml, DocMetadata, DocSearch, DocImages, DocRender, DocExtract, DocExtractEngine,
    ConvertExtractTo, ConvertRenderTo,
    MdParse, MdHeadings, MdLinks, MdImages, MdTables, MdCodeBlocks, MdTree, MdToHtml, MdToText, MdValidate, MdQuery, MdStrip, MdFormat,
    AiOcr, AiTranscribe,
    AiKeys, AiKey, AiModel, AiTemp, AiBase, AiInfo,

    AiSave, AiLoad, AiDef, AiSetDefault, AiGetDefault, AiOn, AiOff, AiSaveConfig, AiLoadConfig,

    // AI namespace-command (2026-08-19, direttiva sintassi): keyword "AI" per il comando
    // legacy multi-parola "AI SET ACTIVE ..." + token fusi per la qualificata "AI.SETACTIVE".
    Ai,
    // Sottocomandi della keyword AI (dopo "AI SET ..." / "AI ...")
    AiSubActive, AiSubModel, AiSubTemp, AiSubCredits,
    AiSubWeight, AiSubStatus, AiSubConfigure, AiSubDiscover,
    AiSubUnregister, AiSubFail, AiSubResetFailed, AiSubTest,
    AiSubTool, AiSubServe, AiSubTrace, AiPermission,
    AiSubSkills, AiSubMarketplace,
    // Token fusi qualificati (AI.SETACTIVE ecc.)
    AiSetActive, AiSetActiveProvider, AiRegister, AiConfigure, AiDiscover,
    AiSetCredits, AiCredits, AiRotate, AiActive, AiList, AiUnregister,
    AiSaveProv, AiLoadProv, AiTest, AiSetPriority, AiPriority, AiSetWeight,
    AiWeight, AiFail, AiResetFailed, AiSetStatus, AiStatus, AiSetModel,
    AiGetModel, AiSetKey, AiGetKey, AiSetTemp, AiGetTemp, AiSetBase, AiGetBase,
    // Agent trace con sottocomando ON/OFF/VERBOSE (AI AGENT TRACE ON → AI.AGENTTRACEON).
    AiAgentTraceOn, AiAgentTraceOff, AiAgentTraceVerbose,
    // AI PATH MODEL/SKILL ADD|REMOVE + AI SKILLS ... (config percorsi e skills).
    AiPathModelAdd, AiPathModelRemove, AiPathSkillAdd, AiPathSkillRemove, AiPathPluginAdd, AiPathPluginRemove,
    AiSkillsAdd, AiSkillsRemove, AiSkillsRefresh, AiSkillsSetIndex,
    AiSkillsMarketplace, AiSkillsPrompt, AiSkillsList, AiSkillsPaths,
    AiSubPlugins, AiPluginsAdd, AiPluginsRemove, AiPluginsRefresh, AiPluginsSetIndex,
    AiPluginsList, AiPluginsEnable, AiPluginsDisable, AiPluginsMarketplace, AiPluginsCreate,
    AiSubPrefill, AiPrefill, AiPrefillStats, AiDecodeStats,
    AiSubDecode, AiDecode,
    AiCacheClear, AiCacheTrim,
    AiServerStart, AiServerStop, AiServerStatus, AiServerModel, AiServerUnload,
    AiSubBenchmark, AiBenchmark, AiBenchResult,
    AiPluginCall, AiPluginInfo, AiPluginList,
    // GRAPHICS: conversione celle<->pixel (dimensione font opzionale).
    C2PX, C2PY, P2CX, P2CY,
    // AI LOG PATH / AI LOG WRITE (logging agente).
    AiLogPath, AiLogWrite, AiLogWriteLine,
    // AI AGENT ASK ... INTO callback (streaming).
    AiAgentAskStream,
    // AI VERIFY (residuo deep R1): verbo AGENT/COMPILE + clausola MAXRETRIES.
    Compile,
    MaxRetries,
    // EVAL(expr): compila ed esegue expr come codice BASIC a runtime, ritorna il risultato.
    // Sintassi zucchero per &expr (MacroExpr) — stesso identico nodo AST, stessa esecuzione
    // (vedi Parser.Expressions.cs, ParseUnary — il ramo Eval costruisce un MacroExpr).
    Eval,

    UniqueCount,
    TTest, TTestPaired, TTestOneSample, StatsAnova, Chi2Test,
    FisherExact, ShapiroWilk, NormalTest, ZTest, KolmogorovSmirnov,
    StatsPearsonR, StatsSpearmanR, StatsLinRegress, StatsCovariance, StatsRange,
    StatsChi2Contingency, StatsMannWhitney, StatsWilcoxon,
    StatsGammaPdf, StatsGammaCdf, StatsGammaPpf, StatsGammaIsf,
    StatsSkewTest, StatsKurtosisTest, StatsJarqueBera,
    StatsMode, StatsMoment, StatsIqr, StatsDescribe,
    SignalFirwin, SignalFindPeaks, SolveIvp,
    SignalWelch, SignalSavgolFilter, SignalResample, SignalDecimate,
    InterpSplrep, InterpSplev,
    SpatialPdist, SpatialCdist, SpatialDistanceMatrix, SpatialConvexHull,
    FftRfft, FftIrfft,
    Torch, // keyword namespace per i comandi legacy TORCH SET/LOAD/UNLOAD/INFO (sintassi a spazio)
    TorchDevice, TorchLibPath, TorchLoaded, TorchCreate, TorchFrom, TorchAdd,
    TorchSub, TorchMul, TorchDiv, TorchMatmul, TorchRelu, TorchGet, TorchSet,
    TorchToArray, TorchShape, TorchFree, TorchAllclose, TorchSum, TorchMean,
    TorchZeros, TorchOnes, TorchEye, TorchRandn, TorchTranspose, TorchReshape,
    TorchSigmoid, TorchTanh, TorchSoftmax, TorchExp, TorchLog, TorchSqrt, TorchPow,
    TorchAbs, TorchMax, TorchMin,
    TorchModelCreate, TorchModelTrainStep, TorchModelLoss, TorchModelForward,
    TorchModelParameters, TorchModelState, TorchModelFree,
    TorchGelu, TorchPermute, TorchSlice, TorchConcat, TorchStack, TorchSplit,
    TorchTopK, TorchArgsort, TorchArgmax, TorchOneHot, TorchClip, TorchNormalize, TorchStandardize, TorchSimilarity,
    TorchBleu, TorchRouge,
    TorchDpoStep,
    TorchTextEncode, TorchTextDecode, TorchDatasetLoad,
    TorchModelExport,
    TorchImatrixLoad, TorchImatrixRanks,
    TorchDtype, TorchAvailable, TorchDeviceOf, TorchModelSave, TorchModelReset,
    TorchLossMse, TorchLossCrossEntropy, TorchLossBinary, TorchAccuracy, TorchModelInput, TorchModelOutput,
    TimeSeriesCovarianceMatrix, TimeSeriesMinMax,
    RollingMean, RollingSkewness, RollingKurtosis, RollingMedian,
    RollingQuantile, RollingSharpe, RollingSortino, RollingMinMax,
    RollingRegressionSingle,
    YuleWalker, MaCoefficients, EuclideanDistanceSquared,
    MatrixRowSums, MatrixColSums, MatrixRowMeans, MatrixColMeans,
    MatrixRowVariances, MatrixColVariances, MatrixFrobeniusNorm,
    MatrixOneNorm, MatrixInfinityNorm, MatrixIsSymmetric, MatrixIsOrthogonal,
    MatrixIsUpperTriangular, MatrixIsLowerTriangular, MatrixArgMax, MatrixArgMin,
    MatrixSlice, MatrixReshape, MatrixConcatRows, MatrixConcatCols,
    MatrixAddRowVector, MatrixAddColVector,
    MatrixMaxValue, MatrixMinValue, MatrixAboveMask, MatrixAboveMatrix,
    MatrixAsCsv, MatrixAsJson,
    MatrixRowCount, MatrixColumnCount, MatrixClone,
    MatrixMultiplyVector, MatrixMultiplyLeftVector,
    MatrixLoadCsv, MatrixLoadJson,
    MatrixPseudoInverse, MatrixRank, MatrixTrace, MatrixKronecker, MatrixBlockDiag,
    MatrixNullSpace, MatrixLuSolve, MatrixNorm,
    MatrixSqrt,        // Radice quadrata di matrice (scipy.linalg.sqrtm)
    MatrixLogm,        // Logaritmo di matrice (scipy.linalg.logm)
    MatrixHessenberg,  // Forma di Hessenberg superiore (scipy.linalg.hessenberg)
    MatrixSetValue,    // Imposta un elemento (h, r, c, v) → handle
    MatrixGetValue,    // Legge un elemento (h, r, c) → Double
    MatrixCosm,        // Coseno di matrice (scipy.linalg.cosm)
    MatrixSinm,        // Seno di matrice (scipy.linalg.sinm)
    MatrixTanm,        // Tangente di matrice (scipy.linalg.tanm)
    MatrixSignm,       // Segno di matrice (scipy.linalg.signm)
    MatrixFractionalPower, // Potenza frazionaria (scipy.linalg.fractional_matrix_power)
    MatrixOrth,        // Base ortonormale del range (scipy.linalg.orth)
    MatrixSubspaceAngles, // Angoli tra sottospazi (scipy.linalg.subspace_angles)
    MatrixPolar,       // Decomposizione polare (scipy.linalg.polar)
    MatrixCond,        // Numero di condizionamento (scipy.linalg.cond)
    MatrixNuclearNorm, // Norma nucleare (scipy.linalg.norm, ord='nuc')
    MatrixToeplitz,    // Matrice di Toeplitz (scipy.linalg.toeplitz)
    MatrixHankel,      // Matrice di Hankel (scipy.linalg.hankel)
    MatrixCirculant,   // Matrice circolante (scipy.linalg.circulant)
    MatrixSolveTriangular, // Sostituzione triangolare (scipy.linalg.solve_triangular)
    MatrixEig,         // Autovalori/autovettori generali (scipy.linalg.eig)
    MatrixSchur,       // Real Schur form (scipy.linalg.schur)
    GoMaxProcs,        // SYSTEM.GOMAXPROCS(n): runtime.GOMAXPROCS di Go
    NumGoroutine,      // SYSTEM.NUMGOROUTINE(): runtime.NumGoroutine di Go
    NumCpu,            // SYSTEM.NUMCPU(): runtime.NumCPU di Go
    Ok,                // RECEIVE v FROM ch OK flag (parità Go v, ok := <-ch)
    Defer,             // DEFER stmt (parità Go defer: cleanup a fine procedura)
    Mutex,             // DIM mu AS MUTEX (parità Go sync.Mutex)
    RwMutex,           // DIM rw AS RWMUTEX (parità Go sync.RWMutex)
    Runlock,           // sotto-comando MUTEX RUNLOCK rw (parità Go mu.RUnlock())
    Once,              // DIM o AS ONCE + comando ONCE RUN o, proc (parità Go sync.Once)
    Atomic,            // DIM x AS ATOMIC + namespace ATOMIC (parità Go sync/atomic)
    Cond,              // DIM c AS COND + comando COND WAIT/SIGNAL/BROADCAST (parità Go sync.Cond)
    AtomicIncrement,   // ATOMIC.INCREMENT(x, n) — Go AddInt64 (BuiltinKind)
    AtomicFetchValue,  // ATOMIC.FETCHVALUE(x) — Go LoadInt64 (BuiltinKind)
    AtomicStore,       // ATOMIC.STORE(x, v) — Go StoreInt64 (BuiltinKind)
    AtomicCompareSwap, // ATOMIC.COMPARESWAP(x, old, new) — Go CompareAndSwapInt64 (BuiltinKind)
    AtomicSwap,        // ATOMIC.SWAP(x, new) — Go SwapInt64 (BuiltinKind)
    PoolGet,           // POOL.GET(p) — Go sync.Pool.Get (BuiltinKind)
    PoolPut,           // POOL.PUT(p, v) — Go sync.Pool.Put (BuiltinKind)
    ConcurrentMap,     // DIM m AS CONCURRENTMAP (parità Go sync.Map)
    SyncMapStore,      // SYNCMAP.STORE(m, k, v) — Go m.Store (BuiltinKind)
    SyncMapLoad,       // SYNCMAP.LOAD(m, k) — Go m.Load (BuiltinKind)
    SyncMapDelete,     // SYNCMAP.DELETE(m, k) — Go m.Delete (BuiltinKind)
    SyncMapLoadOrStore,// SYNCMAP.LOADORSTORE(m, k, v) — Go m.LoadOrStore (BuiltinKind)
    MutexTryLock,      // MUTEX.TRYLOCK(mu) — Go mu.TryLock() non bloccante (BuiltinKind)
    Context,           // DIM ctx AS CONTEXT + namespace CONTEXT (parità Go context.Context)
    ContextBackground, // CONTEXT.BACKGROUND() (BuiltinKind)
    ContextWithCancel, // CONTEXT.WITHCANCEL(ctx) (BuiltinKind)
    ContextCancel,     // CONTEXT.CANCEL(ctx) (BuiltinKind)
    ContextDone,       // CONTEXT.DONE(ctx) → canale chiuso alla cancellazione (BuiltinKind)
    ContextCancelled,  // CONTEXT.CANCELLED(ctx) → BIT (BuiltinKind)
    // CUA — Universal Action Runtime (cross-OS, BrowserProvider dual)
    Browser,           // keyword BROWSER (namespace, provider dual Chromium|Obscura)
    BrowserSetProvider, // BROWSER SET PROVIDER "obscura"|"chromium"|"auto"
    BrowserProvider,   // BROWSER.PROVIDER() → stringa provider attivo
    BrowserNavigate, BrowserSnapshot, BrowserClick, BrowserFill, BrowserEvaluate, BrowserStart,
    BrowserAccessTree, BrowserScreenshot, BrowserGetCookies, BrowserSetCookie, BrowserClearCookies,
    BrowserSetHeader, BrowserSetUserAgent, BrowserHumanClick, BrowserHumanFill, BrowserHumanScroll,
    BrowserObserve, BrowserFind,
    BrowserNetLog, BrowserConsole,
    BrowserAuth,
    Computer,          // keyword COMPUTER (namespace, cross-OS)
    ComputerScreenshot, ComputerClick, ComputerType, ComputerKey, ComputerMove,
    ComputerDrag, ComputerScroll, ComputerWait, ComputerWindows, ComputerFocus,
    ComputerScreenRecord, ComputerScreenRecordStart, ComputerScreenRecordStop, ComputerScreenRecordStatus,
    ComputerDoubleClick, ComputerHotKey, // CUA #7 ASTRA: completano le 12 primitive (double-click, combo)
    // AI OBSERVE (todo AI-Runtime consolidato, P1): azione comando + letture registry
    AiSubObserve,
    AiObserveFile, AiObserveLog, AiObserveScreen,
    AiObservationLatest, AiObservationGet, AiObservationHash,
    AiObservationCount, AiObservationList, AiObservationFile,
    ToolSearch, ToolRegister, // TOOL.SEARCH "pdf" (registry dinamico, usa AiSubTool come TOOL)
    Ui, UiFind, UiFindAll, UiClick, UiType, UiExists, // UI semantico (ActionResolver)
    AgentRun,   // AGENT.RUN goal [, max_steps] — loop Brain→Azione (VM/C gmb_agent_run_goal)
    AgentRunCua, // AI.AGENTRUN name, goal [, rounds] — loop Brain→Azione NATIVO C# (modello locale)
    Destructor,        // SUB DESTRUCTOR — distruttore di classe/TYPE (senza parentesi, ex SUB DELETE,
                       // rimosso perché collideva col verbo REST DELETE). Aggiunto in coda per non
                       // spostare i valori numerici esistenti.
    AgentRunGraph,     // AI.AGENTRUNGRAPH name, prompt [, rounds] — l'agente esplora il knowledge graph
                       // Second Brain con i tool graph.* (loop NATIVO C#, GraphAgentLoop).
    AgentRunRag,       // AI.AGENTRUNRAG name, prompt [, rounds] — l'agente interroga i brain RAG del
                       // Second Brain con i tool rag.* (loop NATIVO C#, RagAgentLoop).

    // --- MLX fine-tune LoRA (superficie BASIC, proposal Piani) — coda, mai rinumero ---
    AiSubFinetune,     // keyword 'FINETUNE' dopo AI (comando AI FINETUNE ...)
    AiDistillBuild,    // AI DISTILL BUILD (Stream D: jsonl -> .bin)
    AiDistillStatus,   // funzione AI.DISTILLSTATUS()
    AiDistillSample,   // AI DISTILL SAMPLE (D1: genera coppie dal modello)
    AiDistillSampleN,  // funzione AI.DISTILLSAMPLEN()
    AiDistillSampleFail, // funzione AI.DISTILLSAMPLEFAIL()
    AiDistillSampleDone, // funzione AI.DISTILLSAMPLEDONE()
    AiDistillEval,      // AI DISTILL EVAL (D4: valutazione paired)
    AiDistillEvalTok,   // funzione AI.DISTILLEVALTOK(idx)
    AiDistillEvalPass,  // funzione AI.DISTILLEVALPASS(idx)
    AiDistillEvalAcc,   // funzione AI.DISTILLEVALACCEPT(idx) (D5 spec-decode)
    AiDistillApply,     // AI DISTILL APPLY (D3: verifica/backup adapter)
    AiDistillRestore,   // AI DISTILL RESTORE (D3: ripristina dal backup)
    AiDistillTrain,     // AI DISTILL TRAIN (D2: head-only)
    AiDistillLoss,      // funzione AI.DISTILLLOSS()
    AiDistillHold,      // funzione AI.DISTILLHOLD()
    AiDistillCkpt,      // funzione AI.DISTILLCKPT()
    AiDistillDone,      // funzione AI.DISTILLDONE()
    AiFinetune,        // builtin AI.FINETUNE(...) / comando legacy
    AiSubLora,         // keyword 'LORA' dopo AI (comando AI LORA APPLY ...)
    AiLora,            // builtin AI.LORA(...)
    AiLoraApply,       // comando AI LORA APPLY ...
    AiFinetuneStatus,  // AI.FINETUNESTATUS() → stringa stato
    AiFinetuneLoss,    // AI.FINETUNELOSS() → ultima loss train
    AiFinetuneHold,    // AI.FINETUNEHOLD() → ultima loss holdout
    AiFinetuneCkpt,    // AI.FINETUNECKPT() → path checkpoint
    AiFinetuneDone,    // AI.FINETUNEDONE() → -1 se l'ultima run è ok
    AiFinetuneGreedy,  // AI.FINETUNEGREEDY() → token greedy prima (negativo = n/d)
    AiSubLoraApply,    // keyword 'APPLY' dopo AI LORA (comando AI LORA APPLY ...)

    // --- CONNECTOR RUNTIME (MCP SYSTEM spec, F0) — in coda per non spostare i valori esistenti ---
    Connector,
    ConnectorLoad,
    ConnectorRegister,
    ConnectorUnregister,
    ConnectorList,
    ConnectorEnable,
    ConnectorDisable,
    ConnectorCall,
    ConnectorStatus,
    ConnectorResources,
    ConnectorPrompts,
    ConnectorMarketplaceList,
    ConnectorMarketplaceGet,
    ConnectorPolicySet,
    ConnectorPolicyGet,
    ConnectorSecretSet,
    ConnectorSecretGet,
    ConnectorAuth,
    ConnectorApproval,
    ConnectorSearch,
    ConnectorTools,
    ConnectorRefresh,

    // --- COLLECTIONS METODI (F1 piano Incongruenze) — in coda per non spostare i valori esistenti ---
    CollCount,
    CollAdd,
    CollAddUnique,
    CollAddRange,
    CollInsert,
    CollRemoveAt,
    CollRemove,
    CollClear,
    CollContains,
    CollIndexOf,
    CollJoin,
    CollSort,
    CollReverse,
    CollMax,
    CollMin,
    CollSum,
    CollUnion,
    CollIntersect,
    CollExcept,
    CollToArray,
    CollToList,
    CollPush,
    CollPop,
    CollPeek,
    CollEnqueue,
    CollDequeue,
    CollFirst,
    CollLast,
    CollAddAfter,
    CollAddBefore,
    CollAddFirst,
    CollAddLast,
    CollRemoveFirst,
    CollRemoveLast,
    CollUniqueCount,
    CollUniqueItems,
    CollHasItem,
    CollStartsWith,
    CollFindAll,
    CollCapacity,
    CollKeys,
    CollValues,
    CollSet,
    CollContainsKey,
    CollGet,
    CollTryGetValue,
    CollTryGet,
    CollToDict,
    CollMinPair,
    CollMaxPair,
    CollExtractMin,
    CollExtractMax,
    CollRotate,
    CollCurrent,
    CollUpdate,
    CollQuery,
    CollQueryRange,
    CollSetKeyValue,
    CollAddNode,
    CollAddEdge,
    CollHasNode,
    CollHasEdge,
    CollRemoveNode,
    CollRemoveEdge,
    CollGetEdgeWeight,
    CollGetInDegree,
    CollNodeCount,
    CollGetNeighbors,
    CollBFS,
    CollDFS,
    CollDijkstra,
    CollNodes,
    CollGetRange,
    CollToJSON,
    CollFront,
    CollBack,
    CollIsEmpty,
    CollAddNodeConsistent,
    // F3 multimodale (2026-09-11): in FONDO — gli ordinali sono registrati nel registry
    // canonico, mai inserire nel mezzo (sposterebbe gli id di tutte le voci successive).
    AiImageInfo, AiImageResize, AiImageCrop, AiImageConvert,
    // F2 multimodale (2026-09-11): estrazione strutturata documenti (stessa regola).
    AiDocFields, AiDocExtract,
    // F4 multimodale (2026-09-11): sintesi vocale (TTS) — in FONDO (stessa regola).
    AiSynthesize,
    // F5 multimodale (2026-09-11): embedding percettivo, compare, detect, classify.
    AiImageEmbed, AiImageCompare, AiImageDetect, AiImageClassify,
    // F6 multimodale (2026-09-11): agent ask con input multimodale (path).
    AiAgentAskFile,
    // F7 multimodale (2026-09-11): video (Y4M nativo).
    AiVideoInfo, AiVideoFrame, AiVideoSnapshot,
    // B3 fine-tune (2026-09-13): misure di apprendimento. In FONDO (gli ordinali sono nel
    // registry canonico: mai inserire nel mezzo).
    AiFinetuneGreedyAfter, AiFinetuneDemoChanged, AiFinetuneBaseTrain, AiFinetuneBaseHold,
    // M14 (2026-09-14): provider AI embedded "embedded" (MoE out-of-core) — comandi
    // AI MODEL LOAD/UNLOAD/CACHE/PREFETCH/DEVICE e funzione AI.STREAMSTATS. In FONDO.
    // M21 (2026-09-14): residenza del modello — AI MODEL PROMOTE nome | DEMOTE nome. In FONDO.
    AiModelLoad, AiModelUnload, AiModelCache, AiModelPrefetch, AiModelDevice, AiModelConvert,
    AiStreamStats, Prefetch, Resident,
    Promote, Demote, AiModelPromote, AiModelDemote,
    // RAG eval (2026-09-14): quartetto metriche retrieval (R1) + giudizi generazione tier-1
    // (R2), namespace VECTORDB. In FONDO (gli ordinali sono nel registry canonico: mai
    // inserire nel mezzo).
    VectorDbPrecisionAt, VectorDbRecallAt, VectorDbMrr, VectorDbNdcgAt,
    VectorDbFaithfulness, VectorDbRelevance, VectorDbHallucination, VectorDbCompleteness,
    // Self-Consistency / Semantic Entropy (Spanda R_sc) — 2026-09-14 (P1 done, P2 dispatch)
    AiSelfConsistency,    // AI.CONSISTENCY(prompt, opts) → risk 0..1
    AiLexicalEntropy,     // AI.LEXICALENTROPY(responses$) → entropy 0..1
    AiConsistencyDetail,  // AI.CONSISTENCYDETAIL$() → JSON detail report
    // E1 (2026-09-15): AI MODEL QUANTIZE src, dst, bits[, group] — comando (in FONDO).
    Quantize, AiModelQuantize,
    // Fase B (R4+R5+R6) — VECTORDB eval gaps (2026-09-15)
    VectorDbDiagnose,         // VECTORDB.DIAGNOSE(result$) → JSON diagnosis
    VectorDbDiagnoseReport,   // VECTORDB.DIAGNOSEREPORT() → aggregato
    VectorDbCalibrate,        // VECTORDB.CALIBRATE(cases$) → {kappa, wilson, far, frr}
    VectorDbEvalGate,         // VECTORDB.EVALGATE(baseline$, current$) → {pass|warn|fail}
    VectorDbEvalDiff,         // VECTORDB.EVALDIFF() → diff detail
    // Fase C (R7+R8) — monitor + compare (2026-09-15)
    VectorDbBaseline,         // VECTORDB.BASELINE(sample$) → build baseline
    VectorDbDrift,            // VECTORDB.DRIFT(window$) → PSI drift detection
    VectorDbAbTest,           // VECTORDB.ABTEST(variantA$, variantB$) → z-test/Welch
    VectorDbCompare,          // VECTORDB.COMPARE(configs$) → tabella comparativa
    // E1 (2026-09-15): AI MODEL INSPECT dir — dump read-only del descriptor (in FONDO).
    Inspect, AiModelInspect,
    // P2 (2026-09-15): AI CONTEXT engine — budget, selector, policy, view
    ContextSet, ContextView, ContextUsed, ContextBudget, ContextOverflow, ContextViewDebug,
    AiSubContext, AiContextSet, AiContextView, AiContextUsed, AiContextBudget, AiContextOverflow, AiContextViewDebug,
    AiContextViewCmd,
    // P3 (2026-09-15): AI WORKER + AI STATE
    AiSubWorker, AiSubState,
    AiWorkerNew, AiWorkerRun,
    AiStateSave, AiStateLoad,
    AiWorkerRounds, AiWorkerRescued, AiWorkerSpend,
    // P2 residuo (2026-09-16): AI CONTEXT SET POLICY|SELECTOR — rendono la policy EFFETTIVA
    // (normalizzazione della view) e completano la superficie dichiarata in §2 del todo.
    AiContextSetPolicy, AiContextSetSelector,
    // §3.6 PARSEC (2026-10-04): context builder a SEGMENTI (lato AGENTE) — CREATE/ADD/MAP/TRIM/GC/COMMIT.
    AiContextCreate, AiContextAdd, AiContextMap, AiContextTrim, AiContextGc, AiContextCommit,
    // §3.6 PARSEC: cache/version-hash (context-cache ↔ prefix KV) + metriche task.
    AiContextHash, AiContextValid, AiTaskMetrics,
    // P3 sostanza (2026-09-16): `AI WORKER "<TASK>"` = INVOCAZIONE PULITA (vista curata sul
    // workspace, isolamento dalla chat history).
    AiWorkerInvoke,
    // P4 residue (2026-09-16): `AI WORKER LOOP "<TASK>"[, rounds]` — loop isolato multi-round
    // con budget CUMULATIVO del run e best-so-far (assi C + D1 del todo AI-Runtime).
    AiWorkerLoop,
    // P4 (2026-09-16): `AI WORKER MANAGE` — manager loop sul task graph (cicli manager→worker,
    // auditor indipendente, dissenso loggato).
    AiWorkerManage,
    // P4 (2026-09-16): keyword del sub-verbo `AI WORKER MANAGE` (in coda: ID stabili). Nome
    // distinto da `Manager` (ruolo dei team) per non confondere i due concetti.
    Manage,

    // ── BITS (2026-09-16): manipolazione bit a 64 bit. Namespace NON-default (63→64),
    // appeso in coda: ID stabili (append-only, mai rinumerare). Tier 1 → Tier 3.
    BitCount,        // BITCOUNT(x) — popcount
    BitGet,          // BITGET(x, n) — bit n → 0/1
    BitSet,          // BITSET(x, n) — x | (1<<n)
    BitClear,        // BITCLEAR(x, n) — x & ~(1<<n)
    BitToggle,       // BITTOGGLE(x, n) — x ^ (1<<n)
    BitTest,         // BITTEST(x, n) — Bit (-1/0)
    BitField,        // BITFIELD(x, lo, hi) — bit lo..hi inclusi, allineati a destra
    BitReverse,      // BITREVERSE(x, width) — inverte i `width` bit bassi
    BitRotateLeft,   // BITROTATEL(x, n, width) — rotazione sinistra su `width` bit
    BitRotateRight,  // BITROTATER(x, n, width) — rotazione destra su `width` bit
    GrayCode,        // GRAYCODE(x) — x ^ (x>>1)
    FromGray,        // FROMGRAY(x) — inversa del codice Gray
    BitParity,       // PARITY(x) — 0 pari / 1 dispari
    BitScanFwd,      // BITSCANFWD(x) — indice del bit 1 più basso, -1 se 0
    BitScanRev,      // BITSCANREV(x) — indice del bit 1 più alto, -1 se 0
    ByteSwap,        // BYTESWAP(x) — inverte l'ordine dei byte
    HammingDist,     // HAMMINGDIST(a, b) — bit diversi
    BitInterleave,   // BITINTERLEAVE(a, b) — codice di Morton
    BitDeinterleave, // BITDEINTERLEAVE(m, which) — inversa del Morton
    NextPow2,        // NEXTPOW2(x) — più piccola potenza di 2 ≥ x
    IsPow2,          // ISPOW2(x) — Bit (-1/0)
    // E1 (2026-09-16): `AI REPLAY AUTO "<sessione>"` — riesecuzione automatica degli eventi
    // registrati. In coda: ID stabili per tutti (keyword REPLAY + builtin).
    Replay, AiReplayAuto,
    // A5 (2026-09-16): fonti OBSERVE di fase 2 (TEST/DATABASE/PROCESS/BROWSER) + keyword DATABASE
    // (il linguaggio aveva solo DATABASES). In coda: ID stabili.
    AiObserveTest, AiObserveDatabase, AiObserveProcess, AiObserveBrowser, Database,
    // Namespace LLM (2026-09-19, hard move AI->LLM): keyword "LLM" per il layer motore/modello/
    // compilatore. In coda: ID stabili per tutti (append-only, il C gmb_tokenkind.h segue).
    Llm,
    // Verbi del MODEL COMPILER (LLVM) nel namespace LLM (2026-09-20): PLAN/POLICY/TENSOR/KERNEL/
    // PROFILE/MODULE/QUANT/KV/MERGE. In coda: ID stabili (append-only).
    LlmPlan, LlmPolicy, LlmTensor, LlmKernel, LlmProfile, LlmModule, LlmQuant, LlmKv, LlmMerge,
    // LLM ARCH <RESOLVE|SHOW|LIST|CHECK> — descrizione d'architettura data-driven (.gmbarch).
    // In coda: ID stabili (append-only, il C gmb_tokenkind.h segue).
    LlmArch,
    // Verbi LLM PRUNE/EXTRACT: TokenKind DEDICATI. Le grafie "PRUNE"/"EXTRACT" sono condivise
    // (JSON.Extract, SET PRUNE) ma il TokenKind è la chiave canonica del dispatch: riusare
    // TokenKind.Prune/Extract collideva nella mappa globale Compiler (Dictionary.Add) e
    // instradava il comando sbagliato. In coda: append-only.
    LlmPrune, LlmExtract,
    // Namespace GMB: registry di moduli esterni (dylib su slot, priorità). In coda: append-only.
    Gmb, GmbModule,
    // Verbo DISTILL SAMPLE (Stream D): l'unico verbo DISTILL senza TokenKind esistente.
    // In coda: append-only (il C gmb_tokenkind.h segue).
    Sample,
    // EMITTER (modello→artefatto): verbo LLM MODEL SAVE <in>, <out> — round-trip/emissione
    // (usa gmb_llm_model_save, gia' esposto in LlmModelNative). In coda: append-only.
    AiModelSave,
    // ABLATE (model surgery MODIFICARE): LLM MODEL ABLATE <in>, <out>, <pattern> — azzera i
    // tensori il cui nome contiene pattern (per-ruolo/esperto). In coda: append-only.
    AiModelAblate,
    AiModelRemove,
    // DIFF (checkpoint/rollback + quality gate): LLM MODEL DIFF <a>, <b>, <out> — report strutturale. In coda.
    AiModelDiff,
    // CHECKPOINT/ROLLBACK (checkpoint-store): LLM MODEL CHECKPOINT <src>,<store>,<name> | ROLLBACK <store>,<name>,<dst>.
    AiModelCheckpoint,
    AiModelRollback,
    // ADD (AGGIUNGERE): LLM MODEL ADD <base>, <module>, <out> — merge tensori di un modulo. In coda.
    AiModelAdd,
    // ADAPTER (AGGIUNGERE adapter LoRA .safetensors): LLM MODEL ADAPTER <base>,<adapter>,<out>[,<alpha>]. In coda.
    AiModelAdapter,
    // MODULE (AGGIUNGERE modulo dichiarato .gmmod): LLM MODEL MODULE <base>,<gmmod>,<module>,<out>. In coda.
    AiModelModule,
    // MODALITY (emit .gmmod da modello+mmproj reali): LLM MODEL MODALITY <base>,<mmproj>,<out.gmmod>. In coda.
    AiModelModality,
    // VALIDATE (verifica integrità strutturale): LLM MODEL VALIDATE <path>, <out>. In coda.
    AiModelValidate,
    // LLM COMPILE <model>, <out.gmbllm> — artefatto eseguito specializzato (§4.9). In coda.
    LlmCompile,
    // LLM QUALITY <model>, <prompt>, <out> — gate NLL/perplexity (§4.5 I6). In coda.
    LlmQuality,
    // LLM.RUNSJSON() / LLM.RUNSCOUNT() — query del registro run (superficie TRAIN RUNS, F1). In coda.
    LlmRunsJson,
    LlmRunsCount,
    LlmRunsClear,
    // LLM.TRAINCONFIG() — default di sessione del training (TRAIN CONFIG SHOW, F1). In coda.
    LlmTrainConfig,
    // LLM.TRAINCONFIGSET(key, value) — aggiorna un default (TRAIN CONFIG SET, F1). In coda.
    LlmTrainConfigSet,
    // LLM.RUNSITEM(id) — JSON di una run (TRAIN RUNS SHOW, F1). In coda.
    LlmRunsItem,
    // LLM.DATASETSTATS(path) — statistiche dataset (TRAIN DATASET STATS, F1). In coda.
    LlmDatasetStats,
    // LLM.LORALIST() — adapter registrati (TRAIN LORA LIST, F2). In coda.
    LlmLoraList,
    LlmLoraSave,
    LlmLoraLoad,
    // TRAIN DPO <model>, {opts} — preference optimization (F3). In coda.
    AiDpo,
    // TRAIN GRPO <model>, {opts} — RLHF senza critic (G-opt2). In coda.
    AiGrpo,
    // TRAIN EXPORT <out> / TRAIN STOP <id>. In coda.
    LlmTrainExport,
    LlmRunStop,
    LlmEvalGate,
    // TRAY.* — icona nella barra del SO (menu bar/tray). In coda.
    TrayCreate,      // TRAYCREATE(icon, tooltip)
    TrayItem,        // TRAYITEM(id, label)
    TraySep,         // TRAYSEP()
    TrayTooltip,     // TRAYTOOLTIP(text)
    TrayPoll,        // TRAYPOLL()
    TrayRemove,      // TRAYREMOVE()
    TrayBackend,     // TRAYBACKEND() -> "macos"|"win"|"linux"|"none"
    TraySelfTest,    // TRAYSELFTEST()
    TrayAvailable,   // TRAYAVAILABLE()
    TrayLast,        // TRAYLAST() -> ultimo item cliccato (read-and-clear)
    // K1 (2026-10-03): LLM MODEL TRACE <nome>, <path|OFF> — trace esperti `.moetrace`
    // (replay offline moe_sim). In CODA (gli ordinali sono nel registry canonico).
    AiModelTrace,
    // LIBRARY "nome" — direttiva di modulo: produce una libreria condivisa (.gmblib) invece di un
    // eseguibile. In coda (append-only).
    Library,
    // TEXTMODE n — seleziona la modalita' console (0=%SDL,1=%C64,2=%ANSI,3=%SYSTEM). In coda.
    TextMode,
    // §4.10(f) — superficie BASIC del GMB Semantic Graph (program-side). Funzioni (ritornano valori).
    // In CODA (append-only: gli ordinali sono nel registry canonico).
    AiSymbol,
    AiImpact,
    AiCallmap,
    AiRevision,
    AiContext,
    AiCheckrev,
    AiPatch,
    AiFindsymbol,
    AiRepomap,
    AiFileoutline,   // §3.6 PARSEC: outline per-file (symbols definiti in un file del programma).
    // §4.10 SEMAPRAX write-side lifecycle (namespace LLM, read-only receipts): VERIFY/COMMIT.
    LlmVerify,
    LlmCommit,
    // §4.10 SEMAPRAX: INSPECT SYMBOL (hover: kind+label+callmap).
    LlmInspect,
    // §4.10 esposizione agenti: AI.AGENTRUNCODE name, prompt [, rounds] — l'agente interroga
    // il grafo semantico del CODICE (tool code.*; loop C# SemanticAgentLoop / C gmb_agent_run_code).
    AgentRunCode,
    // CONTROL ADD instance AT x, y SIZE w, h [PARENT container] — keyword (TokenKind).
    // In CODA (append-only).
    At, Size, Parent,
    // Larghezza/altezza singole nella sintassi VERBOSA dei controlli (LABEL WIDTH w, ...).
    Width, Height,
    // ITEMS (elenco) nella sintassi VERBOSA dei controlli lista (DATALIST ... ITEMS elenco).
    Items,
    // STRDATE()/DATE$ — data corrente UTC "dd/MM/yyyy" (canone data/ora: DATE() = seriale giorno numerico).
    StrDate,
    // B2 — LLM.TOKENCOUNT(model, testo): numero di token (superficie tokenizer). In coda.
    LlmTokenCount,
    // B2 (2026-10-30) — RISULTATO dei verbi LLM (il comando stampa, la funzione RESTITUISCE):
    //   LlmTokens         LLM.TOKENS(model, testo)      -> "74,71,..."   (LLM TOKENS ... INTO)
    //   LlmDetokenize     LLM.DETOKENIZE(model, "74,71") -> testo
    //   LlmTensorValue    LLM.TENSOR(sub, a, b, c)        -> testo di QUALUNQUE sotto-verbo TENSOR
    // In coda (gli ordinali sono nel registry canonico: mai inserire nel mezzo).
    LlmTokens, LlmDetokenize, LlmTensorValue,
    // B2 (2026-10-30, seconda ondata) — versioni-FUNZIONE delle altre famiglie LLM: il comando
    // stampa, la funzione restituisce l'output catturato (`LLM … INTO` usa le stesse).
    LlmPolicyOut, LlmArchOut, LlmModuleOut, LlmKernelOut, LlmKvOut,
    // G4 (2026-10-09): GRAPH.LOUVAIN(brain) / GRAPH.MAXFLOW(brain, from, to) — community e
    // max-flow. In coda (gli ordinali sono nel registry canonico: mai inserire nel mezzo).
    GraphLouvain, GraphMaxFlow,
    // G10 (2026-10-09): estensione SQL (DataFrame) — DESCRIBE/COL/TOCSV/FROMCSV.
    // In coda (gli ordinali sono nel registry canonico: mai inserire nel mezzo).
    SqlDfSchema, SqlCol, SqlToCsv, SqlFromCsv,
    // GREP (2026-10-10): namespace di ricerca file GREP.MATCHES/COUNT/FILES + comando GREP SEARCH.
    // In coda (ordinali canonici).
    Grep, GrepMatches, GrepCount, GrepFiles,
    // ASR (2026-10-10): namespace speech-to-text (Qwen3-ASR). ASR.TRANSCRIBE(model, wav) → testo.
    // In coda (ordinali canonici).
    Asr, AsrTranscribe,
    // MESSAGE (2026-10-30): MESSAGE.DELIVERY(bus) → "sent=S recv=R ack=A nack=N dup=D last=L",
    // contatori di consegna del bus (dup>0 && nack>0 = ri-consegna del broker osservata).
    // In coda (ordinali canonici).
    MessageDelivery,
    // LENB (2026-10-30): LENB(s) → BYTE UTF-8 della stringa. LEN conta i CARATTERI (code
    // point: "😀"=1), LENB i byte ("😀"=4): servono entrambi (limiti di buffer/file vs
    // conteggio visibile). In coda (ordinali canonici).
    LenB,
    // P_* (2026-11-03): sottocomando CHART GETS (non esiste un keyword dedicato).
    // In coda: gli ordinali sono nel registry canonico, mai inserire nel mezzo.
    ChartGets,
    // P_* (2026-11-03): sottocomandi CONTROL senza keyword dedicata. In coda (ordinali
    // canonici): mai inserire nel mezzo.
    ControlGets, ControlZorder, ControlInvalidate,
    // P_* (2026-11-03): chiavi delle clausole PIXEL senza keyword dedicata. In coda
    // (ordinali canonici): mai inserire nel mezzo.
    PixelName, PixelTooltip, PixelVisible, PixelEnabled, PixelTabIndex, PixelTabStop, PixelStyle,
    // ENV (2026-11-03): lettura esplicita dell'ambiente di processo (sola lettura).
    // In coda (ordinali canonici): mai inserire nel mezzo.
    EnvGet, EnvHas,
    // Coll* (2026-11-03): SymmetricExcept di HASHSET (runtime C gmb_collection_set_op).
    // In coda (ordinali canonici): mai inserire nel mezzo.
    CollSymmetricExcept,
    // HLL (2026-11-04): tipo contenitore HyperLogLog (stima cardinalità, runtime C
    // condiviso gmb_hll.c + mirror managed CommonLib.Collections.HyperLogLog).
    // In coda (ordinali canonici): mai inserire nel mezzo.
    Hll,
}
