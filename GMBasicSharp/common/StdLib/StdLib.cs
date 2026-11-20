// FILE: Interpreter.StdLib.cs
// ReSharper disable CheckNamespace

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene lo stato condiviso durante il processo di compilazione.
/// Ora include la mappa globale dei simboli e la tabella di dispatching per i comandi.
/// </summary>
public sealed partial class StdLib : IParserRegistry
{
    /// <summary>
    /// Helper generico per creare un token fittizio (non ha una posizione reale nel codice).
    /// </summary>
    private static Token T(TokenKind kind, string lexeme) => new(kind, lexeme, null, SourcePosition.Unknown);

    /// <summary>
    /// Helper per creare un parametro nativo (nome, opzionale). Convivide la firma con le
    /// registrazioni esplicite `new NativeProcedureDefinition(...)` (parametri stringa).
    /// </summary>
    private static Parameter P(string name, bool opt) =>
        new(-1, T(TokenKind.Identifier, name), new StringType(), opt ? SymbolFlags.IsOptional : SymbolFlags.None, null, opt);

    /// <summary>
    /// Ottiene l'unica istanza singleton del contesto statico.
    /// </summary>
    public static readonly Basic.StdLib Instance = new();

    /// <summary>
    /// Mappa un nome di comando stringa al suo TokenKind per tradurre le chiamate qualificate.
    /// </summary>
    public readonly Dictionary<string, TokenKind> CommandNameToTokenKindMap;
    
    /// <summary>
    /// Mappa un nome di funzione (stringa) alla lista di namespace che la forniscono.
    /// Sostituisce la dipendenza da TokenKind per la risoluzione delle funzioni.
    /// </summary>
    public readonly Dictionary<string, List<string>> FunctionNameToNamespacesMap;

    /// <summary>
    /// Mappa un TokenKind di una funzione built-in a una lista di tutti i namespace
    /// che forniscono un'implementazione per quella funzione.
    /// Essenziale per risolvere l'ambiguità delle chiamate non qualificate.
    /// Esempio: BuiltinFunctionProvidersMap[TokenKind.Replace] -> ["STRINGS", "DBF"]
    /// </summary>
    public readonly Dictionary<TokenKind, List<string>> BuiltinFunctionProvidersMap;

    /// <summary>
    /// Mappa un TokenKind di un comando built-in a una lista di tutti i namespace
    /// che forniscono un'implementazione per quel comando.
    /// Utile per un dispatching più efficiente e coerente.
    /// Esempio: BuiltinCommandProvidersMap[TokenKind.Delete] -> ["DBF", "ISAM", "LEDGER"]
    /// </summary>
    public readonly Dictionary<TokenKind, List<string>> BuiltinCommandProvidersMap;

    /// <summary>
    /// Mappa il nome di un namespace built-in alla lista delle sue procedure.
    /// </summary>
    public readonly Dictionary<string, IEnumerable<ProcedureDefinition>> BuiltinProcedures;

    /// <summary>
    /// Mappa il nome di un namespace built-in alla mappa dei suoi tipi esportati.
    /// </summary>
    public readonly Dictionary<string, IReadOnlyDictionary<string, DataType>> BuiltinTypes;

    /// <summary>
    /// Mappa il nome di un namespace built-in alla lista delle sue costanti predefinite.
    /// Questo permette alla libreria standard di esportare costanti globali (come i membri degli ENUM).
    /// </summary>
    public readonly Dictionary<string, List<ConstDeclaration>> BuiltinConstants;
    
    /// <summary>
    /// Un set di tutti i nomi di namespace built-in.
    /// Usato dal parser per disambiguare rapidamente tra un accesso a namespace (es. `IO.KILL`)
    /// e un accesso a un membro di una variabile (es. `myObject.Kill`).
    /// </summary>
    public readonly HashSet<string> BuiltinNamespaceNames;
    /// <summary>
    /// Namespace di moduli IL esterni registrati da IlModuleLoader (moduli .NET precompilati
    /// caricati con --il-module o Load). Vengono aggiunti ai namespace considerati dalla
    /// compilazione (CreateBuiltinNamespaces) così che parser/linker risolvano `NS.METHOD(...)`.
    /// Tenuti QUI (non in IlModuleLoader) perché Orchestrator.cs è compilato anche dal progetto
    /// Conformance che non referenzia IlBackend.
    /// </summary>
    public readonly HashSet<string> ExternalModuleNames = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Hook impostato da IlModuleLoader per caricare un assembly IL esterno da path e
    /// registrarne i namespace (restituisce il numero di namespace caricati). Tenuto QUI
    /// (non in IlModuleLoader) perché Orchestrator.cs è compilato anche dal progetto
    /// Conformance che non referenzia IlBackend: l'Orchestrator lo invoca solo se non-null.
    /// </summary>
    public Func<string, int>? LoadExternalModule { get; set; }

    /// <summary>
    /// Un set di tutti i nomi di funzioni built-in (senza qualifica di namespace).
    /// Usato dal parser per distinguere una chiamata a funzione (es. `SIN(x)`) da un accesso
    /// ad array (es. `myArray(x)`).
    /// </summary>
    public readonly HashSet<string> AllBuiltinFunctionNames;

    /// <summary>
    /// Mappa il nome di un namespace built-in alla lista delle sue variabili globali predefinite.
    /// </summary>
    public readonly Dictionary<string, List<(string Name, DataType Type, SymbolFlags Flags)>>
        BuiltinGlobals;

    public readonly Dictionary<Type, BuiltinTypeSchema> BuiltinTypeSchemas = new();
    
    /// <summary>
    /// Inizializza un nuovo contesto di compilazione con le impostazioni fornite.
    /// </summary>
    public StdLib()
    {
        CommandNameToTokenKindMap = new Dictionary<string, TokenKind>(StringComparer.OrdinalIgnoreCase);
        FunctionNameToNamespacesMap = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase); // Inizializzazione
        BuiltinFunctionProvidersMap = new Dictionary<TokenKind, List<string>>();
        BuiltinCommandProvidersMap = new Dictionary<TokenKind, List<string>>();
        BuiltinConstants = new Dictionary<string, List<ConstDeclaration>>(StringComparer.OrdinalIgnoreCase);
        BuiltinProcedures = new Dictionary<string, IEnumerable<ProcedureDefinition>>(StringComparer.OrdinalIgnoreCase);
        BuiltinTypes = new Dictionary<string, IReadOnlyDictionary<string, DataType>>(StringComparer.OrdinalIgnoreCase);
        BuiltinNamespaceNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        AllBuiltinFunctionNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        BuiltinGlobals =
            new Dictionary<string, List<(string Name, DataType Type, SymbolFlags Flags)>>(StringComparer
                .OrdinalIgnoreCase);
        
        CreateSystemNamespace();
        CreateJsonNamespace();
        CreateConsoleNamespace();
        CreateClipNamespace();
        CreateOnKeyNamespace();
        CreateCollectionsNamespace();
        CreateConversionsNamespace();
        CreateJoyNamespace();
        CreateWindowNamespace();
        CreateStringsNamespace();
        CreateDateTimeNamespace();
        CreateMathNamespace();
        CreateMatrixNamespace();
        CreateWorldNamespace();   // motore 3D: WORLD (comando) + MOVING/LIGHTNOW/TIME/FLOORZ/CEILZ
        CreateLinalgNamespace();
        // SPECIAL: namespace LIBERATO — ri-esposto via FFI (shared/include/special.inc, libgmbmath).
        CreateSignalNamespace();
        CreateVectorNamespace();
        CreateIONamespace();
        CreateFontsNamespace();
        CreateCompressionNamespace();
        CreateMemoryMapNamespace();
        CreateNetNamespace();
        CreateHttpNamespace();
        CreateGraphicsNamespace();
        CreateChartNamespace();
        CreateConcurrencyNamespace();
        CreateAtomicNamespace();
        CreatePoolNamespace();
        CreateSyncMapNamespace();
        CreateContextNamespace();
        CreateArraysNamespace();
        CreateDbfNamespace();
        CreateIsamNamespace();
        CreateSpeedNamespace();
        CreateLedgerNamespace();
        CreateRddNamespace();
        CreateConvertNamespace();
        CreateAiNamespace();
        CreateLlmNamespace();
        CreateBrainsNamespace();
        CreateVectorDbNamespace();
        CreateSqlNamespace();
        CreateGrepNamespace();
        CreateAsrNamespace();
        // CreateProviderNamespace();  // FUSO in AI (2026-09-03): il registry provider vive nel
        // namespace AI (funzioni AI.X col ritorno + comandi AI ... a spazio). Vedi StdLib.AI.cs.
        // CreateAiVectorNamespace();  // fusa nel namespace AI (2026-08-19)
        CreateTypeInfoNamespace();
        // CreateWorkflowNamespace();  // fuso nel namespace AI (2026-08-28): comandi AI WORKFLOW ...
        CreateConstantsNamespace();
        // CreateMcpNamespace();   // fusa nel namespace AI (2026-08-19): non esiste un namespace MCP
        CreateStatsNamespace();
        CreateNumericalNamespace();
        // FITTING: namespace LIBERATO — ri-esposto via FFI (shared/include/fitting.inc, libgmbstats).
        // SIMULATION: namespace LIBERATO — ri-esposto via FFI (shared/include/simulation.inc, libgmbstats).
        // REGRESSION: namespace eliminato 2026-11-08 — REGRESS vive in STATS
        // (STATS.REGRESS, one-pass; distinto da STATS.LINREGRESS).
        RegisterAnalysisResultTypeSchemas();
        // CreateOptimizeNamespace();  // DEPRECATO 2026-11-03: namespace OPTIMIZE dismesso,
        // sostituito dal pacchetto FFI `OPT` (libgmbopt). Copia archiviata in
        // GMBasicMonoRepo-Deprecated/optimize/StdLib.Optimize.cs.
        // BITS: namespace migrato a FFI (shared/libs/bits, libgmbbits) l'08-11-2026.
        CreateTrayNamespace();   // TRAY (2026-10-02): icona barra SO
        CreateMessageNamespace(); // MESSAGE (2026-10-05): messaging generico (bus) + ON MESSAGE
        CreateTorchNamespace();
        CreateToolNamespace();
        CreateUiNamespace();
        CreateBrowserNamespace();
        CreateComputerNamespace();
        CreateConnectorNamespace();
        CreateAgentNamespace();
        CreatePolymorphicCommands();
        // AUTH: namespace migrato a FFI (shared/libs/auth, libgmbauth) il 2026-11-08.
        CreateMouseNamespace();
        CreateControlsNamespace();
        
        // La logica di popolamento di AllBuiltinFunctionNames viene ora gestita dentro RegisterBuiltinNamespace.
    }

    /// <summary>Registra un comando per NOME/LESSEMA (senza TokenKind dedicato): usato dai
    /// controlli pixel e da CONTROL (nessun TokenKind nuovo, file TokenKind.cs intatto).</summary>
    public void RegisterCommandByName(string commandName, string namespaceName,
        Func<Parser, string, Stmt> parseFunction)
    {
        var registration = new CommandRegistration(namespaceName, parseFunction);
        if (!ParserRegistries.NamedCommandRegistry.TryGetValue(commandName, out var list))
        {
            list = [];
            ParserRegistries.NamedCommandRegistry[commandName] = list;
        }
        list.Add(registration);
    }

    /// <summary>
    /// Registra un comando SOLO per la sintassi legacy/non qualificata (basata su TokenKind).
    /// Non popola più il registro qualificato: i comandi non hanno forma qualificata.
    /// </summary>
    public void RegisterCommand(TokenKind commandKind, string commandName, string namespaceName,
        Func<Parser, string, Stmt> parseFunction)
    {
        var registration = new CommandRegistration(namespaceName, parseFunction);

        if (!ParserRegistries.UnifiedCommandRegistry.TryGetValue(commandKind, out var legacyRegistrationList))
        {
            legacyRegistrationList = [];
            ParserRegistries.UnifiedCommandRegistry[commandKind] = legacyRegistrationList;
        }

        legacyRegistrationList.Add(registration);

        CommandNameToTokenKindMap.TryAdd(commandName, commandKind);

        if (!BuiltinCommandProvidersMap.TryGetValue(commandKind, out var providerNsList))
        {
            providerNsList = [];
            BuiltinCommandProvidersMap[commandKind] = providerNsList;
        }

        if (!providerNsList.Contains(namespaceName))
        {
            providerNsList.Add(namespaceName);
        }
    }

    /// <summary>
    /// Metodo helper centralizzato per registrare un namespace built-in e le sue definizioni.
    /// <summary>Registra un modulo IL esterno (via IlModuleLoader) come namespace builtin:
    /// i metodi statici pubblici di un assembly [GmbHModule] diventano `NS.METHOD(...)`.</summary>
    public void RegisterExternalModule(string namespaceName, IEnumerable<ProcedureDefinition> procedures)
        => RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);

    /// <summary>Rimuove un modulo IL esterno precedentemente registrato (test/ricaricamento).</summary>
    public void UnregisterExternalModule(string namespaceName)
    {
        var names = BuiltinProcedures.TryGetValue(namespaceName, out var procs)
            ? procs.Select(p => p.OriginalName.Lexeme).ToList()
            : [];
        BuiltinProcedures.Remove(namespaceName);
        BuiltinTypes.Remove(namespaceName);
        BuiltinNamespaceNames.Remove(namespaceName);
        global::Basic.SymbolTable.BuiltinNamespaceNames.Remove(namespaceName);

        foreach (var funcName in names)
        {
            if (FunctionNameToNamespacesMap.TryGetValue(funcName, out var list) && list.Contains(namespaceName))
            {
                list.Remove(namespaceName);
                if (list.Count == 0) FunctionNameToNamespacesMap.Remove(funcName);
            }
            if (AllBuiltinFunctionNames.Contains(funcName)
                && (!FunctionNameToNamespacesMap.TryGetValue(funcName, out var remaining) || remaining.Count == 0))
                AllBuiltinFunctionNames.Remove(funcName);
        }
    }

    private void RegisterBuiltinNamespace(string namespaceName, 
        IEnumerable<ProcedureDefinition> procedures,
        IReadOnlyDictionary<string, DataType> types,
        IEnumerable<(string Name, DataType Type, SymbolFlags Flags)>? globals = null,
        IEnumerable<ConstDeclaration>? constants = null)
    {
        var procedureList = procedures.ToList();
        // MERGE, non replace: più file di dominio contribuiscono allo stesso namespace
        // (es. "AI" = StdLib.AI.cs + StdLib.Connector.cs). Senza merge, il secondo
        // RegisterBuiltinNamespace sovrascriveva l'intero namespace (bug: AI.* persi,
        // restavano solo i CONNECTOR*). Dedup per nome (OrdinalIgnoreCase).
        if (BuiltinProcedures.TryGetValue(namespaceName, out var existingProcs))
        {
            var merged = existingProcs.ToList();
            var seenNames = new HashSet<string>(merged.Select(p => p.OriginalName.Lexeme),
                StringComparer.OrdinalIgnoreCase);
            foreach (var p in procedureList)
                if (seenNames.Add(p.OriginalName.Lexeme)) merged.Add(p);
            procedureList = merged;
        }
        BuiltinProcedures[namespaceName] = procedureList;
        if (BuiltinTypes.TryGetValue(namespaceName, out var existingTypes) && existingTypes.Count > 0)
        {
            var mergedTypes = new Dictionary<string, DataType>(existingTypes, StringComparer.OrdinalIgnoreCase);
            foreach (var kv in types) mergedTypes[kv.Key] = kv.Value;
            BuiltinTypes[namespaceName] = mergedTypes;
        }
        else
        {
            BuiltinTypes[namespaceName] = types;
        }
    
        if (globals != null)
        {
            BuiltinGlobals[namespaceName] = globals.ToList();
        }
        
        if (constants != null)
        {
            BuiltinConstants[namespaceName] = constants.ToList();
        }
        
        BuiltinNamespaceNames.Add(namespaceName);
        global::Basic.SymbolTable.BuiltinNamespaceNames.Add(namespaceName);

        // Popola le mappe di lookup per funzioni
        foreach (var proc in procedureList)
        {
            var funcName = proc.OriginalName.Lexeme;
            AllBuiltinFunctionNames.Add(funcName);

            // Popola la nuova mappa basata su stringa
            if (!FunctionNameToNamespacesMap.TryGetValue(funcName, out var nsListByName))
            {
                nsListByName = [];
                FunctionNameToNamespacesMap[funcName] = nsListByName;
            }
            if (!nsListByName.Contains(namespaceName))
            {
                nsListByName.Add(namespaceName);
            }
            
            // Mantiene il popolamento della vecchia mappa basata su TokenKind per retrocompatibilità
            // (potrebbe essere usata dal Parser)
            if (proc is NativeProcedureDefinition nativeDef)
            {
                if (!BuiltinFunctionProvidersMap.TryGetValue(nativeDef.BuiltinKind, out var nsListByKind))
                {
                    nsListByKind = [];
                    BuiltinFunctionProvidersMap[nativeDef.BuiltinKind] = nsListByKind;
                }

                if (!nsListByKind.Contains(namespaceName))
                {
                    nsListByKind.Add(namespaceName);
                }
            }
        }
    }
    
    /// <summary>
    /// Metodo helper per registrare lo schema di un tipo built-in.
    /// </summary>
    public void RegisterTypeSchema(Type astNodeType, 
        Dictionary<string, DataType>? fields, 
        Dictionary<string, DataType>? methods = null)
    {
        var schema = new BuiltinTypeSchema(
            fields ?? new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase),
            methods ?? new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        );
        BuiltinTypeSchemas[astNodeType] = schema;
        TypeSchemaRegistry.Register(astNodeType, schema);
    }
    
    // --- IParserRegistry explicit interface implementation ---
    HashSet<string> IParserRegistry.AllBuiltinFunctionNames => AllBuiltinFunctionNames;
    Dictionary<TokenKind, List<string>> IParserRegistry.BuiltinFunctionProvidersMap => BuiltinFunctionProvidersMap;
    Dictionary<string, TokenKind> IParserRegistry.CommandNameToTokenKindMap => CommandNameToTokenKindMap;
    Dictionary<string, List<string>> IParserRegistry.FunctionNameToNamespacesMap => FunctionNameToNamespacesMap;
    IReadOnlyDictionary<string, IEnumerable<ProcedureDefinition>> IParserRegistry.BuiltinProcedures => BuiltinProcedures;

    /// <summary>
    /// Cerca un'interfaccia built-in registrata in un namespace.
    /// </summary>
    public InterfaceDefinition? FindBuiltinInterface(string name)
    {
        foreach (var nsTypes in BuiltinTypes.Values)
        {
            if (nsTypes.TryGetValue(name, out var dt) && dt is InterfaceType ifType)
                return ifType.Definition;
        }
        return null;
    }
    private static void Log(string message, bool force = false)
    {
        Console.WriteLine(message);
    }
}