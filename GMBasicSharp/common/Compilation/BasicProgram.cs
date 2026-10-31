// FILE: Interpreter.BasicProgram.cs

using System.Collections.Immutable;
using System.Runtime.CompilerServices;
using CommonLib;

namespace Basic;

/*
------------------------------------------------------------------------------------------------------------------------
                    Guida di Riferimento al Programma Compilato (BasicProgram.cs)
------------------------------------------------------------------------------------------------------------------------
Questa classe è ora l'oggetto di stato centrale per l'intero processo di compilazione e per l'esecuzione.
Agisce come un contenitore vivente che viene costruito in modo incrementale dal Parser e dal Linker,
e poi passato (in uno stato "congelato" o di sola lettura) all'Executor.

Questo design unifica i concetti precedentemente separati di `CompilationContext` e `BasicProgram`,
eliminando la ridondanza e creando un'unica fonte di verità per lo stato di una compilazione.
------------------------------------------------------------------------------------------------------------------------
*/
/// <summary>
/// Rappresenta l'intera applicazione BASIC, sia durante la sua costruzione (compilazione)
/// che durante l'esecuzione. Contiene tutti i moduli, i simboli, le mappe e le impostazioni.
/// </summary>
public sealed class BasicProgram : ICompilationContext, IDefinesProvider
{
    // --- STATO DELLA COMPILAZIONE (Mutevole durante la build) ---

    /// <summary>
    /// Ottiene le impostazioni di compilazione efficaci, determinate all'inizio del processo.
    /// </summary>
    public EffectiveSettings Settings { get; }

    /// <summary>
    /// Ottiene la tabella dei simboli unificata per l'intero programma. Questa è la fonte di verità
    /// per tutti i simboli (variabili, procedure, tipi, etc.) e i loro scope.
    /// </summary>
    public SymbolTable Symbols { get; }

    /// <summary>
    /// La lista dei path dei moduli risolti, popolata dall'Orchestrator dopo la risoluzione dipendenze.
    /// </summary>
    public List<string> ResolvedModuleSourcePaths { get; } = [];

    /// <summary>
    /// Contiene i risultati della pre-elaborazione (lexing) di un singolo file sorgente.
    /// Questo oggetto immutabile memorizza i token e la mappa del sorgente per evitare
    /// di leggere e tokenizzare lo stesso file più volte durante le diverse passate del compilatore.
    /// </summary>
    /// <param name="Tokens">La sequenza immutabile di token generata dal Lexer.</param>
    /// <param name="SourceMap">La mappa che associa gli indici di riga al loro contenuto e origine.</param>
    public sealed record PreprocessedFile(ImmutableArray<Token> Tokens, ImmutableArray<SourceLine> SourceMap);

    /// <summary>
    /// Ottiene un dizionario che mappa i percorsi dei file sorgente ai loro token e source map pre-elaborati.
    /// Questa cache previene la ri-lettura e ri-tokenizzazione dei file.
    /// </summary>
    public readonly Dictionary<string, PreprocessedFile> PreprocessedFiles;

    /// <summary>
    /// Ottiene un dizionario che mappa i percorsi dei file alla lista di direttive `IMPORT` trovate in essi.
    /// </summary>
    public Dictionary<string, List<ImportStmt>> ImportsByFile { get; }

    /// <summary>
    /// Librerie FFI (path assoluti) dei pacchetti REQUIRE risolti (contentType c-ffi/mixed):
    /// consumate a runtime dal VM/C (--ffi-lib → gmb_ffi_dl_preload) e dal backend IL.
    /// </summary>
    public List<string> ResolvedFfiLibs { get; set; } = [];

    /// <summary>
    /// Flag di link C99 (`-I... -L... -l...`) dai pacchetti REQUIRE risolti (GetC99LinkInfo):
    /// appesi al comando `cc` del backend C99 (C99CodeEmitter non usa più dlopen inline).
    /// </summary>
    public string? ResolvedC99LinkFlags { get; set; }

    /// <summary>
    /// Chunk VM (path assoluti .chunk/.prg) dei pacchetti REQUIRE contentType vm-chunk risolti:
    /// caricati e fusi nel chunk principale dal runner VM (ChunkMerger) prima dell'esecuzione.
    /// </summary>
    public List<string> ResolvedVmChunks { get; set; } = [];

    /// <summary>
    /// File .bas di EXPORTS dei pacchetti REQUIRE compilati nel programma (path assoluti).
    /// Le procedure dichiarate in questi file sono l'API di un pacchetto vm-chunk: la loro
    /// implementazione vive nel chunk del pacchetto e il ChunkMerger le LINKa (ridirigendo le
    /// chiamate compile-time verso l'indirizzo ribasato nel chunk).
    /// </summary>
    public List<string> ResolvedExportsFiles { get; set; } = [];

    /// <summary>
    /// Nomi delle procedure dichiarate nelle file di exports dei pacchetti vm-chunk (API da
    /// linkare al chunk). Calcolati dopo il linking dalle file in <see cref="ResolvedExportsFiles"/>.
    /// </summary>
    public HashSet<string> ChunkExternProcedureNames { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Ottiene un dizionario che mappa i percorsi dei moduli (file) alle loro rappresentazioni AST compilate.
    /// Questa collezione viene popolata durante la fase di parsing.
    /// </summary>
    public Dictionary<string, BasicModule> LoadedModules { get; }

    /// <summary>
    /// Dizionario dei type parameters risolti durante il linking.
    /// Mappa ogni TypeParameter al suo tipo concreto (es. T → INTEGER per LIST OF INTEGER).
    /// </summary>
    public Dictionary<TypeParameter, DataType> ResolvedTypeParameters { get; } = new();

    /// <summary>
    /// Cache delle specializzazioni generiche create durante il linking.
    /// Mappa ID negativi univoci alla FunctionDefinition specializzata.
    /// Non viene usata la SymbolTable globale per evitare ambiguità con la definizione originale.
    /// </summary>
    public Dictionary<int, FunctionDefinition> ResolvedSpecializations { get; } = new();

    /// <summary>
    /// Cache dei tipi UDT generici monomorfizzati, chiave: "NomeGenerico&lt;Arg1,Arg2&gt;".
    /// Garantisce che ogni istanziazione concreta (es. Box&lt;DOUBLE&gt;) sia specializzata
    /// una sola volta e condivida lo stesso SymbolId in tutto il programma.
    /// </summary>
    public Dictionary<string, UserDefinedType> GenericUdtInstantiations { get; } = new();

    /// <summary>
    /// Cache delle CLASS generiche monomorfizzate, stessa semantica di
    /// <see cref="GenericUdtInstantiations"/> ma per <see cref="ClassDefinition"/>.
    /// </summary>
    public Dictionary<string, ClassDefinition> GenericClassInstantiations { get; } = new();

    /// <summary>
    /// Ottiene un dizionario che mappa i percorsi dei moduli (file) al loro store aggregato di valori DATA.
    /// Questa collezione viene popolata durante la fase di parsing.
    /// </summary>
    public Dictionary<string, ImmutableArray<object>> DataStoresByModule { get; }

    /// <summary>
    /// Un dizionario MUTEVOLE usato durante il parsing per accumulare tutte le istruzioni DATA.
    /// Il Linker lo renderà poi immutabile.
    /// </summary>
    public readonly Dictionary<DataStmt, int> GlobalDataStatementMapBuilder;

    /// <summary>
    /// Ottiene la mappa aggregata di tutte le istruzioni DATA del programma al loro indice
    /// nello store aggregato del rispettivo modulo. Essenziale per `RESTORE [label]`.
    /// </summary>
    public IReadOnlyDictionary<DataStmt, int> GlobalDataStatementMap { get; internal set; }

    /// <summary>
    /// Il dizionario interno che mappa i nomi dei simboli di compilazione condizionale ai loro valori.
    /// Un valore 'null' o 'true' indica che il simbolo è semplicemente definito (come un flag).
    /// </summary>
    private readonly Dictionary<string, string?> _defines;
    IReadOnlyDictionary<string, string?> IDefinesProvider.Defines =>
        new Dictionary<string, string?>(_defines, StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Contatore globale e thread-safe per generare ID univoci per le etichette dei cicli.
    /// </summary>
    private int _globalLabelCounter;

    // --- STATO FINALE DEL LINKING (Immutabile dopo la build) ---

    /// <summary>
    /// Ottiene il modulo principale che funge da entry-point per l'applicazione.
    /// Questa proprietà viene impostata al termine del processo di linking.
    /// </summary>
    public BasicModule? MainModule { get; internal set; }

    /// <summary>
    /// Ottiene la mappa sorgente unificata per l'intero programma, che associa ogni indice di riga
    /// al suo contenuto e origine. Questa è l'unica fonte di verità per il codice sorgente.
    /// </summary>
    public ImmutableArray<SourceLine> SourceMap { get; internal set; }

    /// <summary>
    /// Ottiene la mappa aggregata e immutabile di tutti i namespace definiti nell'applicazione.
    /// Questa è la fonte di verità principale per la risoluzione dei simboli durante l'esecuzione.
    /// </summary>
    // NOTE: Poiché tutti i simboli sono nella SymbolTable, Namespaces è solo la lista dei nomi di namespace.
    public IReadOnlySet<string> Namespaces { get; internal set; }

    /// <summary>
    /// Ottiene la mappa di tutte le costanti risolte (constant folding) durante la compilazione.
    /// La chiave è il nome qualificato della costante (es. "MioNamespace.MiaCostante") e il valore
    /// è il suo valore letterale calcolato. Questa mappa viene impostata dal Linker.
    /// </summary>
    public IReadOnlyDictionary<string, object> ResolvedConstants { get; internal set; }

    /// <summary>
    /// Ottiene un dizionario che mappa gli ID dei file ai loro percorsi canonici.
    /// Viene popolato una sola volta durante la tokenizzazione nell'Orchestrator.
    /// </summary>
    public IReadOnlyDictionary<int, string> FileIdToPathMap { get; internal set; }

    /// <summary>
    /// Ottiene il percorso della directory principale del programma.
    /// </summary>
    public string ProgramRootDirectory { get; }

    /// <summary>
    /// Un dizionario che mappa i percorsi canonici dei file sorgente al loro ID numerico univoco.
    /// </summary>
    private readonly Dictionary<string, int> _fileIdMap = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Un contatore interno per generare il prossimo ID di file disponibile.
    /// </summary>
    private int _nextFileId;

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="BasicProgram"/> per avviare una nuova compilazione.
    /// </summary>
    public BasicProgram(EffectiveSettings settings, IReadOnlyDictionary<string, string?>? initialDefines,
        string programRootDirectory)
    {
        Settings = settings;
        ProgramRootDirectory = programRootDirectory;

        Symbols = new SymbolTable();
        PreprocessedFiles = new Dictionary<string, PreprocessedFile>(StringComparer.OrdinalIgnoreCase);
        ImportsByFile = new Dictionary<string, List<ImportStmt>>(StringComparer.OrdinalIgnoreCase);
        LoadedModules = new Dictionary<string, BasicModule>(StringComparer.OrdinalIgnoreCase);
        DataStoresByModule = new Dictionary<string, ImmutableArray<object>>(StringComparer.OrdinalIgnoreCase);
        GlobalDataStatementMapBuilder = new Dictionary<DataStmt, int>();
        _defines = new Dictionary<string, string?>(StringComparer.OrdinalIgnoreCase);
        if (initialDefines != null)
        {
            foreach (var define in initialDefines)
            {
                _defines[define.Key] = define.Value;
            }
        }

        AddPredefinedSymbols();

        MainModule = null!;
        SourceMap = ImmutableArray<SourceLine>.Empty;
        GlobalDataStatementMap = ImmutableDictionary<DataStmt, int>.Empty;
        Namespaces = ImmutableHashSet<string>.Empty;
        FileIdToPathMap = ImmutableDictionary<int, string>.Empty;
        ResolvedConstants = ImmutableDictionary<string, object>.Empty;
    }

    /// <summary>
    /// Restituisce un nuovo ID intero univoco per un'etichetta.
    /// </summary>
    public int GetNextLabelId([CallerMemberName] string callerName = "")
    {
        return Interlocked.Increment(ref _globalLabelCounter);
    }

    /// <summary>
    /// Aggiunge un simbolo di compilazione. Se il valore è null, il simbolo è considerato un flag.
    /// </summary>
    public void AddDefine(string name, string? value = null) => _defines[name] = value;

    /// <summary>
    /// Rimuove un simbolo di compilazione, se esiste.
    /// </summary>
    public void RemoveDefine(string name) => _defines.Remove(name);

    /// <summary>
    /// Verifica se un simbolo di compilazione è definito.
    /// </summary>
    public bool IsDefined(string name) => _defines.ContainsKey(name);

    /// <summary>
    ///   Restituisce il valore associato al simbolo definito con #DEF.
    ///   Se il simbolo esiste ma non ha valore esplicito, restituisce "true" (equivalente a simbolo definito).
    ///   Se il simbolo non esiste, restituisce null.
    /// </summary>
    public string? GetDefineValue(string name) => _defines.GetValueOrDefault(name);

    /// <summary>
    /// Tenta di ottenere il valore di un simbolo di compilazione.
    /// </summary>
    public string? GetValue(string name)
    {
        _defines.TryGetValue(name, out var value);
        return value;
    }

    private void AddPredefinedSymbols()
    {
        // --- Multi-target: short platform names for #IF / #IFDEF ---
        if (System.Runtime.InteropServices.RuntimeInformation.IsOSPlatform(System.Runtime.InteropServices.OSPlatform
                .Windows))
        {
            _defines["__OS_WINDOWS__"] = "true";
            _defines["__OS__"] = "WINDOWS";
            _defines["WINDOWS"] = "true";
        }
        else if (System.Runtime.InteropServices.RuntimeInformation.IsOSPlatform(System.Runtime.InteropServices
                     .OSPlatform.Linux))
        {
            _defines["__OS_LINUX__"] = "true";
            _defines["__OS__"] = "LINUX";
            _defines["LINUX"] = "true";
        }
        else if (System.Runtime.InteropServices.RuntimeInformation.IsOSPlatform(System.Runtime.InteropServices
                     .OSPlatform.OSX))
        {
            _defines["__OS_MACOS__"] = "true";
            _defines["__OS__"] = "MACOS";
            _defines["MACOS"] = "true";
        }
        else
        {
            _defines["__OS_OTHER__"] = "true";
            _defines["__OS__"] = "OTHER";
        }

        var arch = System.Runtime.InteropServices.RuntimeInformation.OSArchitecture.ToString().ToUpperInvariant();
        _defines["__ARCH__"] = arch;
        _defines[$"__ARCH_{arch}__"] = "true";
        // Short arch flags for #IF / #IFDEF
        _defines[arch] = "true";

        var now = DateTime.UtcNow;
        _defines["__DATE__"] = now.ToString("yyyyMMdd", System.Globalization.CultureInfo.InvariantCulture);
        _defines["__TIME__"] = now.ToString("HHmmss", System.Globalization.CultureInfo.InvariantCulture);
        _defines["_GM_BASIC_VERSION__"] = "V1.0";

        // WASM is never set by default — user sets it via -D WASM or #DEF WASM
        // DEBUG/RELEASE: RELEASE by default
#if DEBUG
        _defines["DEBUG"] = "true";
#else
        _defines["RELEASE"] = "true";
#endif
    }

    /// <summary>
    /// Restituisce la mappa completa Path -> FileId per uso interno dell'Orchestrator/Linker.
    /// </summary>
    internal IReadOnlyDictionary<string, int> GetFileIdMap() => _fileIdMap;

    /// <summary>
    /// Cerca in tutti i namespace del programma le definizioni di procedura candidate
    /// che corrispondono a un dato nome e sono compatibili con un dato numero di argomenti.
    /// </summary>
    public List<ProcedureDefinition> FindProcedureCandidates(string name, int argCount)
    {
        var candidates = new List<ProcedureDefinition>();

        void SearchNamespace(string nsName)
        {
            var overloads = GetProceduresForNamespace(nsName);
            candidates.AddRange(overloads.Where(p =>
                p.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase) &&
                SymbolTable.IsProcedureSignatureCompatible(p, argCount)));
        }

        foreach (var nsName in Namespaces)
            SearchNamespace(nsName);

        // Also search the global scope (which may not be in Namespaces)
        SearchNamespace(SymbolTable.GlobalScopeName);

        return candidates.Distinct().ToList();
    }

    /// <summary>
    /// Trova il percorso del file sorgente in cui è stata definita una specifica procedura.
    /// </summary>
    // NOTA: Questo metodo è ridondante se FindFilePathForSymbol è chiamato con un ProcDef, ma lo lasciamo per compatibilità.
    public string? FindFilePathForProcedure(ProcedureDefinition procToFind)
    {
        return FindFilePathForSymbol(procToFind);
    }

    /// <summary>
    /// Cerca tutte le definizioni di procedura candidate (SUB o FUNCTION) che corrispondono
    /// a un dato nome, rispettando le regole di visibilità del modulo specificato.
    /// </summary>
    public List<ProcedureDefinition> FindProcedureCandidatesByName(string name, string modulePath)
    {
        var candidates = new List<ProcedureDefinition>();

        var localProcedures = GetProceduresForModule(modulePath)
            .Where(p => p.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase))
            .ToList();
        candidates.AddRange(localProcedures);

        if (LoadedModules.TryGetValue(modulePath, out var currentModule))
        {
            foreach (var import in currentModule.Imports)
            {
                var nsName = import.NamespaceName.Lexeme;
                switch (import.Mode)
                {
                    case ImportMode.AllMembers:
                    {
                        var procs = GetProceduresForNamespace(nsName)
                            .Where(p => p.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase) &&
                                        p.Flags.HasFlag(SymbolFlags.IsPublic));
                        candidates.AddRange(procs);
                        break;
                    }
                    case ImportMode.SpecificMembers:
                    {
                        foreach (var member in import.SpecificMembers)
                        {
                            if (!(member.Alias?.Lexeme ?? member.OriginalName.Lexeme).Equals(name,
                                    StringComparison.OrdinalIgnoreCase)) continue;
                            var procs = GetProceduresForNamespace(nsName)
                                .Where(p => p.OriginalName.Lexeme.Equals(member.OriginalName.Lexeme,
                                                StringComparison.OrdinalIgnoreCase) &&
                                            p.Flags.HasFlag(SymbolFlags.IsPublic));
                            candidates.AddRange(procs);
                        }

                        break;
                    }
                }
            }
        }

        foreach (var nsName in Settings.DefaultNamespaces)
        {
            var procs = GetProceduresForNamespace(nsName)
                .Where(p => p.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase) &&
                            p.Flags.HasFlag(SymbolFlags.IsPublic));
            candidates.AddRange(procs);
        }

        return candidates.Distinct().ToList();
    }

    /// <summary>
    /// Verifica se un dato modulo contiene almeno una definizione di procedura.
    /// Ora interroga la SymbolTable in modo ottimale.
    /// </summary>
    public bool ModuleHasProcedures(BasicModule module)
    {
        var targetFilePath = module.FilePath;
        var targetFileId = GetOrAddFileId(targetFilePath);

        return Symbols.GetSymbolsByFileId(targetFileId)
            .Any(s => s.DeclarationNode is ProcedureDefinition);
    }

    /// <summary>
    /// Restituisce un elenco piatto di tutte le definizioni di procedura del programma.
    /// Questa operazione è ora ottimizzata tramite un indice nella SymbolTable.
    /// </summary>
    public IEnumerable<ProcedureDefinition> GetAllProcedures() => Symbols.GetAllProcedures();

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di procedura definite in un modulo specifico.
    /// </summary>
    public IEnumerable<ProcedureDefinition> GetProceduresForModule(BasicModule module)
    {
        return GetProceduresForModule(module.FilePath);
    }

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di procedura definite in un file specifico.
    /// Sfrutta l'indice FileId O(1) appena aggiunto in SymbolTable.
    /// </summary>
    public IEnumerable<ProcedureDefinition> GetProceduresForModule(string filePath)
    {
        var targetFileId = GetOrAddFileId(filePath);
        return Symbols.GetSymbolsByFileId(targetFileId)
            .Select(s => s.DeclarationNode)
            .OfType<ProcedureDefinition>()
            .Where(p => p.SymbolId >= 0);
    }

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di procedura definite in uno specifico namespace.
    /// </summary>
    public IEnumerable<ProcedureDefinition> GetProceduresForNamespace(string namespaceName)
    {
        return Symbols.GetProceduresForNamespace(namespaceName);
    }

    /// <summary>
    /// Restituisce un elenco piatto di tutte le definizioni di Classi del programma.
    /// Questa operazione è ora ottimizzata tramite un indice nella SymbolTable.
    /// </summary>
    public IEnumerable<ClassDefinition> GetAllClasses() => Symbols.GetAllClasses();

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di Classi definite in un modulo specifico.
    /// Sfrutta l'indice FileId O(1) aggiunto in SymbolTable.
    /// </summary>
    public IEnumerable<(ClassDefinition Definition, BasicModule Source)> GetClassSymbolsForModule(BasicModule module)
    {
        var targetFileId = GetOrAddFileId(module.FilePath);

        return Symbols.GetSymbolsByFileId(targetFileId)
            .Select(s => s.DeclarationNode)
            .OfType<ClassDefinition>()
            .Select(c => (c, module));
    }

    /// <summary>
    /// Restituisce un elenco piatto di tutti i Tipi Definiti dall'Utente (UDT) del programma.
    /// </summary>
    public IEnumerable<UserDefinedType> GetAllUserDefinedTypes() => Symbols.GetAllUserDefinedTypes();

    /// <summary>
    /// Restituisce un elenco piatto di tutte le dichiarazioni di variabili globali del programma.
    /// Sfrutta l'indice GlobalSymbolIds O(1) in SymbolTable.
    /// </summary>
    public IEnumerable<VariableDeclaration> GetAllGlobalVariables()
    {
        return Symbols.GetGlobalSymbolIds()
            .Select(id => Symbols.GetSymbol(id))
            .Where(sym => sym.Flags.HasFlag(SymbolFlags.IsGlobal))
            .Select(sym => sym.DeclarationNode)
            .OfType<VariableDeclaration>();
    }

    /// <summary>
    /// Restituisce un elenco piatto di tutte le dichiarazioni di costanti del programma.
    /// </summary>
    public IEnumerable<ConstDeclaration> GetAllConstants() => Symbols.GetAllConstants();

    /// <summary>
    /// Restituisce un elenco di tutte le istruzioni DATA definite nelle procedure di un modulo specifico.
    /// </summary>
    public IEnumerable<DataStmt> GetDataStatementsFromModuleProcedures(BasicModule module)
    {
        return GetProceduresForModule(module)
            .SelectMany(proc => proc.Body.OfType<DataStmt>());
    }

    /// <summary>
    /// Trova il percorso del file sorgente in cui è stato definito un simbolo specifico.
    /// Ora usa la mappa pre-calcolata per la risoluzione O(1).
    /// </summary>
    public string? FindFilePathForSymbol(object symbolDefinition)
    {
        if (symbolDefinition is not AstNode node) return null;
        var fileId = node.Position.FileId;
        return fileId < 0
            ? null
            :
            // Risoluzione O(1)
            FileIdToPathMap.GetValueOrDefault(fileId);
    }

    /// <summary>
    /// Ottiene un ID univoco per un dato percorso di file.
    /// </summary>
    public int GetOrAddFileId(string filePath)
    {
        if (!_fileIdMap.TryGetValue(filePath, out var fileId))
        {
            fileId = _nextFileId++;
            _fileIdMap[filePath] = fileId;
        }

        return fileId;
    }
    
    /// <summary>
    /// Determina il namespace primario associato a un modulo, basandosi sui simboli che contiene.
    /// Se il modulo non contiene simboli o è vuoto, restituisce lo scope globale di default.
    /// </summary>
    /// <param name="modulePath">Il percorso canonico del modulo da analizzare.</param>
    /// <returns>Il nome del namespace primario del modulo o il nome dello scope globale.</returns>
    public string GetNamespaceForModule(string modulePath)
    {
        var fileId = GetOrAddFileId(modulePath);
        
        // Cerca il primo simbolo definito in questo file.
        var firstSymbolInFile = Symbols.GetSymbolsByFileId(fileId).FirstOrDefault();
        
        // Se non ci sono simboli, il modulo appartiene implicitamente allo scope globale.
        if (firstSymbolInFile == null)
        {
            return SymbolTable.GlobalScopeName;
        }

        // Usa l'indice ottimizzato della SymbolTable per trovare il namespace del simbolo in O(1).
        return Symbols.GetNamespaceNameForSymbol(firstSymbolInFile.Id);
    }    
}