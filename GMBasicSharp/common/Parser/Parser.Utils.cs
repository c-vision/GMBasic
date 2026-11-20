// FILE: Interpreter.Parser.utils.cs

namespace Basic;

/// <summary>
/// Contiene i metodi di utilità del Parser per la Fase 1
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Deduce il tipo di dato di una variabile in base al suo suffisso.
    /// Se nessun suffisso viene trovato, restituisce DoubleType come default.
    /// </summary>
    /// <param name="variableName">Il nome della variabile.</param>
    /// <returns>Il DataType corrispondente. Non restituisce mai null.</returns>
    private static DataType DeduceTypeFromSuffix(string variableName)
    {
        if (string.IsNullOrEmpty(variableName)) return new DoubleType();
        return variableName[^1] switch
        {
            '$' => new StringType(),
            '%' => new IntegerType(),
            '!' => new SingleType(),
            '#' => new DoubleType(),
            '&' => new LongType(),
            _ => new DoubleType()
        };
    }
    
    /// <summary>
    /// Separa il nome base di una variabile dal suo suffisso di tipo.
    /// </summary>
    /// <param name="variableName">Il nome completo della variabile (es. "MioValore#").</param>
    /// <returns>
    /// Una tupla contenente il nome base (es. "MioValore") e il DataType corrispondente 
    /// al suffisso (es. DoubleType), o null se non c'è un suffisso.
    /// </returns>
    private static (string BaseName, DataType? SuffixType) SplitNameAndSuffix(string variableName)
    {
        if (string.IsNullOrEmpty(variableName)) 
            return (variableName, null);

        var lastChar = variableName[^1];
        DataType? suffixType = lastChar switch
        {
            '$' => new StringType(),
            '%' => new IntegerType(),
            '!' => new SingleType(),
            '#' => new DoubleType(),
            '&' => new LongType(),
            _ => null
        };

        if (suffixType != null)
        {
            return (variableName[..^1], suffixType);
        }

        return (variableName, null);
    }

    /// <summary>
    /// Determina se un <see cref="TokenKind"/> rappresenta una keyword che funge da prefisso per un comando di namespace
    /// nella sintassi legacy (es. `DBF`, `ISAM`, `HTTP`).
    /// </summary>
    /// <remarks>
    /// Questo helper è cruciale per la logica di dispatching in `ParseStatement`. Permette di identificare
    /// rapidamente se un token richiede una verifica del contesto degli `IMPORT` prima di essere trattato come
    /// un comando specifico di un namespace, distinguendolo da un normale identificatore.
    /// </remarks>
    /// <param name="kind">Il <see cref="TokenKind"/> da controllare.</param>
    /// <returns><c>true</c> se il token è una keyword di comando di namespace; altrimenti, <c>false</c>.</returns>
    private static bool IsNamespaceCommandToken(TokenKind kind)
    {
        return kind switch
        {
            TokenKind.Dbf => true,
            TokenKind.Isam => true,
            TokenKind.Speed => true,
            TokenKind.Ledger => true,
            TokenKind.Rdd => true,
            TokenKind.Sql => true,
            TokenKind.Http => true,
            TokenKind.Tcp => true,
            TokenKind.Udp => true,
            TokenKind.Ssl => true,
            TokenKind.Chart => true,
            TokenKind.Matrix => true,
            TokenKind.World => true,   // motore 3D (P2)
            TokenKind.Clock => true,   // tempo a passo fisso (2026-11-20)
            TokenKind.Vector => true,
            TokenKind.VectorDb => true,
            TokenKind.Ai => true,
            TokenKind.Llm => true,
            TokenKind.Asr => true,
            TokenKind.Gmb => true,
            TokenKind.Train => true,
            TokenKind.Torch => true,
            TokenKind.Graph => true,
            TokenKind.Brains => true,
            TokenKind.Connector => true,
            TokenKind.Message => true,
            _ => false
        };
    }

    /// <summary>
    /// Mappa un <see cref="TokenKind"/> di un comando di namespace al nome stringa del namespace corrispondente.
    /// </summary>
    /// <remarks>
    /// Questo metodo complementa `IsNamespaceCommandToken`. Una volta che un token è stato identificato come
    /// un comando di namespace, questa funzione fornisce il nome del namespace richiesto per la
    /// validazione rispetto alla lista dei namespace importati.
    /// </remarks>
    /// <param name="kind">Il <see cref="TokenKind"/> da mappare (es. `TokenKind.Dbf`).</param>
    /// <returns>Il nome stringa del namespace (es. "DBF").</returns>
    /// <exception cref="ArgumentOutOfRangeException">Lanciata se il <paramref name="kind"/> fornito non è
    /// un token di comando di namespace valido.</exception>
    private static string GetNamespaceForToken(TokenKind kind)
    {
        return kind switch
        {
            TokenKind.Dbf => "DBF",
            TokenKind.Isam => "ISAM",
            TokenKind.Speed => "SPEED",
            TokenKind.Ledger => "LEDGER",
            TokenKind.Rdd => "RDD",
            TokenKind.Sql => "SQL",
            TokenKind.Http => "HTTP",
            TokenKind.Tcp => "NET",
            TokenKind.Udp => "NET",
            TokenKind.Ssl => "NET",
            TokenKind.Chart => "CHART",
            TokenKind.Matrix => "MATRIX",
            TokenKind.World => "WORLD",   // motore 3D (P2)
            TokenKind.Clock => "CLOCK",   // tempo a passo fisso (2026-11-20)
            TokenKind.Vector => "VECTOR",
            TokenKind.VectorDb => "VECTORDB",
            TokenKind.Ai => "AI",
            TokenKind.Llm => "LLM",
            TokenKind.Asr => "ASR",
            TokenKind.Gmb => "GMB",
            TokenKind.Train => "TRAIN",
            TokenKind.Torch => "TORCH",
            TokenKind.Graph => "GRAPH",
            TokenKind.Brains => "BRAINS",
            TokenKind.Connector => "CONNECTOR",
            TokenKind.Message => "MESSAGE",
            _ => throw new ArgumentOutOfRangeException(nameof(kind), "TokenKind is not a namespace command.")
        };
    }

    /// <summary>
    /// [NUOVO HELPER] Consuma il token corrente se è un nome valido per un evento.
    /// Accetta sia un `Identifier` generico sia keyword specifiche come `Click`, `Resize`, etc.
    /// </summary>
    /// <returns>Il token che rappresenta il nome dell'evento.</returns>
    private Token ConsumeEventName()
    {
        var token = Peek();
        // Controlla se il token è un identificatore generico O una delle keyword
        // che possono essere usate contestualmente come nome di un evento.
        if (IsValidNameToken(token))
        {
            return Advance();
        }
        throw CreateError(token, "Expected a valid event name.");
    }
    
    /// <summary>
    /// Crea e lancia un'eccezione di parsing formattata (`BasicException`), arricchita con
    /// informazioni contestuali come il token, la riga, la colonna e il testo della riga di codice sorgente.
    /// </summary>
    private static BasicException CreateError(Token token, string message, ErrorCode code = ErrorCode.SyntaxError) =>
        new(token, message, code);

    /// <summary>
    /// Crea e lancia un'eccezione di parsing formattata (`BasicException`) a partire da un nodo Target.
    /// </summary>
    private static BasicException CreateError(Target target, string message, ErrorCode code = ErrorCode.SyntaxError) =>
        new(code, target.Position, message);
    
    /// <summary>
    /// Metodo helper per il logging condizionale.
    /// </summary>
    private static void Log(string message, bool force = false)
    {
        if (force) Console.WriteLine(message);
    }    
}