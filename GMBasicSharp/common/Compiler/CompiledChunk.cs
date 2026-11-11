// FILE: Interpreter.Compiler.CompiledChunk.cs

namespace Compiler;

/// <summary>
/// Rappresenta un blocco di P-Code compilato, pronto per essere eseguito da una VM.
/// </summary>
public sealed class CompiledChunk
{
    /// <summary>
    /// Versione del formato A1/GMBC. **3** = firme callback per-slot in coda a OP_CALL_FFI_T
    /// (2026-11-11); **2** = descrittore FFI per argomento (2026-11-06); **1** = formato storico
    /// (maschere), valido solo per chunk SENZA chiamate FFI. I chunk compilati in-process sono
    /// sempre 3; il valore arriva da <c>ChunkA1Serializer.Deserialize</c> per i chunk da file.
    /// </summary>
    public int FormatVersion { get; set; } = 3;

    /// <summary>
    /// La sequenza di byte che rappresentano le istruzioni P-Code.
    /// </summary>
    public List<byte> Code { get; } = [];

    /// <summary>
    /// La "Constant Pool" che contiene tutti i valori letterali
    /// (numeri, stringhe) usati dal codice.
    /// </summary>
    public List<object?> Constants { get; } = [];

    /// <summary>
    /// Mappa ogni byte in <see cref="Code"/> all'indice della riga
    /// del codice sorgente da cui è stato generato. Questa lista ha
    /// sempre la stessa dimensione di <see cref="Code"/>.
    /// </summary>
    public List<int> Lines { get; } = [];
    
    /// <summary>
    /// Contiene tutti i valori letterali definiti nelle istruzioni DATA del programma,
    /// in ordine di apparizione.
    /// </summary>
    public List<object> Data { get; } = [];
    
    /// <summary>
    /// Indirizzo della procedura SUB Main (o 0 se non esiste).
    /// La VM parte da questo indirizzo invece di Ip=0.
    /// </summary>
    public int EntryPoint { get; set; }

    /// <summary>
    /// Numero totale di slot globali referenziati dal programma compilato (ogni indice di slot
    /// nel bytecode è un letterale scritto dal compilatore stesso — nessun path di creazione
    /// dinamica a runtime). Usato per dimensionare l'array Globals UNA SOLA VOLTA, prima che
    /// qualunque task/goroutine parta (vedi PCodeVm, costruttore) — necessario sotto vero
    /// parallelismo: un resize a runtime (il vecchio comportamento di EnsureGlobals, chiamato
    /// pigramente) potrebbe far divergere l'array visto da due goroutine diverse che condividono
    /// lo stesso riferimento Globals per copia (vedi PCodeTask.Globals/LaunchGoTask).
    /// </summary>
    public int GlobalCount { get; set; }

    /// <summary>
    /// Massimo numero di slot locali richiesto da una procedura del chunk. Gli opcode locali
    /// codificano l'indice in un ushort (formato §20.3, limite 65535); questo metadata evita
    /// però di preallocare sempre tutti gli slot in ogni frame. Zero identifica un chunk
    /// legacy che non contiene ancora il metadata.
    /// </summary>
    public int LocalCount { get; set; }

    /// <summary>
    /// Intervalli [Start, End) del bytecode emesso da OGNI istruzione BASIC (annidati: un
    /// corpo IF è dentro l'intervallo dell'IF). Servono a riprendere a granularità di
    /// ISTRUZIONE (RESUME NEXT / ON ERROR RESUME NEXT): l'istruzione che contiene l'ip
    /// fallito è quella con l'intervallo più PICCOLO che lo contiene, e la ripresa va al suo
    /// End — così un errore a metà di `C = A / B` non riprende sullo STORE (che scriverebbe
    /// NIL) e un errore nella CONDIZIONE non esegue il corpo dell'IF. Coda opzionale del
    /// formato A1: i chunk vecchi non ce l'hanno e si ricade sull'ip successivo all'opcode.
    /// </summary>
    public List<(int Start, int End)> StatementRanges { get; } = [];

    /// <summary>
    /// Per procedura (indirizzo iniziale): quali slot locali sono NUMERICI (default 0). La VM
    /// inizializza quegli slot a 0.0 alla creazione del frame; senza, una variabile la cui
    /// unica scrittura non è mai eseguita (es. dopo RESUME NEXT o dentro un IF falso) restava
    /// null → stampa vuota e TYPENAME NIL, mentre IL/C99Native danno 0/DOUBLE.
    /// Coda opzionale del formato A1 (i chunk vecchi non ce l'hanno).
    /// </summary>
    public Dictionary<int, bool[]> ProcedureLocalNumeric { get; } = new();

    /// <summary>Il programma usa il namespace TRAY: la coda delle funzioni TRAY va drenata dal
    /// MAIN THREAD del host (Cocoa/NSStatusItem), mentre il BASIC gira sui worker. Il pump
    /// (PCodeVm/VM-C) si attiva solo per questi programmi. Coda opzionale del formato A1.</summary>
    public bool UsesTray { get; set; }

    /// <summary>
    /// Nome procedura -> indirizzo Ip assoluto (copia di Compiler._procedureAddresses, che
    /// altrimenti esiste solo a compile-time e viene scartato). Serve alla VM per risolvere a
    /// runtime un riferimento a procedura passato come STRINGA (es. WEBSOCKET.OnMessage =
    /// "NomeSub" — sintassi legacy per registrare una callback per nome, a differenza di
    /// HTTP ROUTE/HTTP USE che risolvono l'handler a un indirizzo Ip gia' a compile-time via
    /// OP_PUSH_CODE_REF).
    /// </summary>
    public Dictionary<string, int> ProcedureAddresses { get; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Indirizzo Ip -> numero di parametri dichiarati dalla SUB/FUNCTION a quell'indirizzo
    /// (copia di Compiler._procedureArities). Serve a chiamanti come
    /// PCodeProcessor.SpawnHttpHandlerTask, che costruiscono l'elenco argomenti a runtime senza
    /// passare dal sito di chiamata compilato: senza questo, un handler con MENO parametri di
    /// quelli forniti riceve comunque tutti gli argomenti e il binding si disallinea in silenzio.
    /// </summary>
    public Dictionary<int, int> ProcedureArities { get; } = new();

    /// <summary>Metadata PCodeVM per l'unwind sospendibile: indirizzo di OP_PUSH_TRY ->
    /// inizio FINALLY e prima istruzione successiva a END TRY. Non modifica il bytecode e
    /// resta quindi compatibile con i consumer legacy che ignorano questa side-table.</summary>
    public Dictionary<int, TryRegionMetadata> TryRegions { get; } = new();

    /// <summary>
    /// Nomi delle variabili globali, indice = slot VM (<see cref="GlobalCount"/>).
    /// Usato da <c>&amp;</c>/<c>EVAL</c> nella VM/C per risolvere gli identificatori.
    /// </summary>
    public string[] GlobalNames { get; set; } = [];

    /// <summary>
    /// Nomi delle variabili locali di procedura, chiave = indirizzo Ip di inizio procedura,
    /// valore = nomi per slot (indice array = slot; null = slot senza nome noto, es. temporanei).
    /// Popolato a compile-time dai nomi dichiarati a LIVELLO PROCEDURA (return-slot, parametri,
    /// DIM di procedura; le locali di BLOCCO non sono incluse — EndScope le rimuove prima dello
    /// snapshot, vedi Compiler.SnapshotProcedureLocalNames). Serve a <c>&amp;</c>/<c>EVAL</c>
    /// (semantica S_eval) per risolvere le locali del chiamante: lookup locali-prima-dei-globali,
    /// write-back sullo slot del chiamante.
    /// SOLO in-memory: NON serializzato in A1/GMBC — il formato chunk condiviso con la VM/C
    /// (ChunkA1Serializer/gmb_chunk.c) resta intatto; la VM/C riceverà i nomi per altra via (fase 2).
    /// </summary>
    public Dictionary<int, string?[]> ProcedureLocalNames { get; } = new();

    /// <summary>
    /// Riversa le tabelle locali di un chunk figlio (lambda, LOCAL SUB/FUNCTION) nel chunk
    /// ospite, traslando gli indirizzi di <paramref name="hostAddress"/> + <paramref name="shift"/>
    /// (il prologo STORE_LOCAL/CLEAR_RETURN inserito in testa al codice figlio sposta il corpo).
    /// Chiamato dai siti di merge dei chunk figli nel compilatore.
    /// </summary>
    public void MergeChildLocalNames(CompiledChunk child, int hostAddress, int shift)
    {
        foreach (var (childAddr, flags) in child.ProcedureLocalNumeric)
            ProcedureLocalNumeric[hostAddress + shift + childAddr] = flags;
        foreach (var (childAddr, names) in child.ProcedureLocalNames)
            ProcedureLocalNames[hostAddress + shift + childAddr] = names;
    }

    /// <summary>
    /// Normalizza un nome identificatore per il confronto (rimuove il suffisso di tipo
    /// $, %, #, !, &amp;, @). Stessa regola di Compiler.ResolveLocal/AddLocal: i nomi snapshot
    /// sono già normalizzati, il lookup a runtime confronta OrdinalIgnoreCase.
    /// </summary>
    public static string NormalizeLocalName(string name)
    {
        if (string.IsNullOrEmpty(name)) return name;
        var lastChar = name[^1];
        if (lastChar is '$' or '%' or '#' or '!' or '&' or '@')
            return name[..^1];
        return name;
    }
}

public sealed record TryRegionMetadata(int FinallyIp, int EndIp, bool HasCatch = true);
