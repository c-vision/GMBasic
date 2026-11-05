// FILE: Interpreter.Orchestrator.cs

using System.Collections.Immutable;
using CommonLib;

// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

/// <summary>
/// La classe Orchestrator è il punto di ingresso e il coordinatore di alto livello
/// del processo di compilazione. Gestisce il flusso di lavoro completo:
/// PreProcessamento, Parsing di tutti i moduli, e Linking finale.
/// </summary>
public static class Orchestrator
{
    // ==================================================================================
    // --- PUNTO DI INGRESSO PUBBLICO ---
    // ==================================================================================

    /// <summary>
    /// Punto di ingresso principale per la compilazione di un intero progetto BASIC a partire da un file.
    /// Questo metodo orchestra l'intero processo, dalla scoperta dei file fino al linking finale,
    /// producendo un programma eseguibile.
    /// </summary>
    /// <param name="entryFilePath">Il percorso del file sorgente principale (.bas) che funge da punto di ingresso
    /// per la compilazione e la successiva esecuzione.</param>
    /// <param name="initialDefines">Un dizionario di simboli di compilazione condizionale (per #IF)
    /// da definire globalmente per l'intero progetto.</param>
    /// <param name="verbose">Se impostato a true, abilita l'output di log dettagliato su console
    /// per tutte le fasi del processo di compilazione.</param>
    /// <returns>
    /// Un'istanza di <see cref="BasicProgram"/> completamente compilata e linkata, pronta per essere eseguita.
    /// </returns>
    /// <remarks>
    /// Il flusso di compilazione è suddiviso in più fasi principali:
    /// 1.  Fase di Pre-Processamento (gestita da `PreProcessProject`):
    ///     - Viene creato il contesto di compilazione (`BasicProgram`).
    ///     - Viene eseguita una scansione del filesystem per scoprire tutti i file sorgente (.bas, .inc)
    ///       a partire dalla directory del file di ingresso.
    ///     - Vengono risolte le dipendenze tra file basate sulle direttive `IMPORT`.
    ///     - Vengono risolte le dipendenze esterne basate sulle direttive `REQUIRE`.
    ///     - Tutti i file necessari vengono letti e tokenizzati una sola volta, con i risultati salvati
    ///       nella cache `PreprocessedFiles`.
    /// 2.  Fase di Parsing e Linking (gestita da `ParseAndLinkAllModules`):
    ///     - Viene eseguita una pre-scansione unificata sui token per registrare le firme di tutti i simboli.
    ///     - Viene eseguito il parsing completo per costruire l'Abstract Syntax Tree (AST) di ogni modulo.
    ///     - Il Linker esegue la validazione semantica, la riscrittura dell'AST e l'assemblaggio finale
    ///       del programma.
    /// </remarks>
    /// <param name="backend">Backend di esecuzione ("il" | "c99" | "vm"): guida la selezione degli
    /// artefatti REQUIRE (SelectArtifact) e il caricamento dei moduli (dotnet→IL, ffi→c99, chunk→vm).
    /// Default "il".</param>
    public static BasicProgram Compile(string entryFilePath, IReadOnlyDictionary<string, string?>? initialDefines,
        bool? verbose = null, string backend = "il")
    {
        Opt.VerboseOrchestrator = verbose ?? Opt.VerboseOrchestrator;

        // §4.10(f): registra il folder semantico (AI.SYMBOL/IMPACT/CALLMAP) PRIMA del linking,
        // così il linker puo' foldare le call a costanti stringa (grafo del programma).
        // §4.10(f): registra il folder semantico via reflection (nessun ref a compile-time: il
        // progetto Conformance ricompila questo file senza il ref a SemanticGraph).
        try { System.Type.GetType("GMBasic.SemanticGraph.SemanticSurface, SemanticGraph")?.GetMethod("EnsureRegistered")?.Invoke(null, null); } catch { }

        // FASE 1: PRE-PROCESSING (Setup del contesto, tokenizzazione iniziale e risoluzione REQUIRE)
        var program = PreProcessProject(entryFilePath, initialDefines, backend);



        // FASE 2 & 3: PARSING E LINKING
        var linked = ParseAndLinkAllModules(entryFilePath, program);

        // Nomi delle procedure dichiarate nelle file di exports dei pacchetti vm-chunk: sono
        // l'API da LINKAre all'implementazione nel chunk (ChunkMerger). Dedotte dal FileId
        // della procedura (una procedura locale non può condividere il nome con un export:
        // sarebbe un errore di definizione duplicata al link).
        if (program.ResolvedExportsFiles.Count > 0)
        {
            var exportsIds = new HashSet<int>(
                program.ResolvedExportsFiles.Select(f => program.GetOrAddFileId(f)));
            var externNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var proc in linked.Symbols.GetAllProcedures())
                if (exportsIds.Contains(proc.OriginalName.Position.FileId))
                    externNames.Add(proc.OriginalName.Lexeme);
            program.ChunkExternProcedureNames = externNames;
        }

        return linked;
    }

    /// <summary>
    /// Compila una singola stringa di codice sorgente fornita direttamente in memoria.
    /// Questo metodo è progettato per ambienti di esecuzione dinamica come la REPL (Read-Eval-Print Loop),
    /// il debugger, o per la compilazione "just-in-time" di macro.
    /// A differenza della compilazione da file, non esegue la discovery di altri file nel filesystem.
    /// </summary>
    /// <param name="sourceText">La stringa contenente il codice BASIC da compilare.</param>
    /// <param name="mainProgramFileName">Un nome di file fittizio (es. "REPL" o "Macro") da associare
    /// al codice sorgente. È essenziale per la gestione interna dei moduli e per il reporting degli errori.</param>
    /// <param name="initialDefines">Un dizionario di simboli di compilazione condizionale (es. per #IF) da
    /// applicare a questa sessione di compilazione.</param>
    /// <param name="verbose">Se impostato a true, abilita l'output di log dettagliato su console
    /// per tutte le fasi del processo di compilazione.</param>
    /// <returns>
    /// Un'istanza di <see cref="BasicProgram"/> completamente compilata e linkata, pronta per l'esecuzione.
    /// </returns>
    /// <remarks>
    /// Il processo di compilazione eseguito da questo metodo è completo e comprende:
    /// 1.  Creazione del Contesto: Viene creato un oggetto `BasicProgram` isolato.
    /// 2.  Tokenizzazione: Il `sourceText` viene analizzato dal `Lexer` per produrre una sequenza di token.
    /// 3.  Pre-elaborazione: I risultati della tokenizzazione vengono salvati nella cache `PreprocessedFiles`.
    /// 4.  Compilazione Completa: Viene invocato il metodo orchestratore `ParseAndLinkAllModules`,
    ///     che gestisce tutte le passate successive (pre-scansione delle firme, parsing completo per i corpi e linking finale),
    ///     proprio come farebbe una compilazione da file.
    /// </remarks>
    public static BasicProgram CompileFromString(string sourceText, string mainProgramFileName,
        IReadOnlyDictionary<string, string?>? initialDefines, bool? verbose = null)
    {
        Opt.VerboseOrchestrator = verbose ?? Opt.VerboseOrchestrator;
        var programRootDirectory = Directory.GetCurrentDirectory();
        var entryFilePath = Path.Combine(programRootDirectory, mainProgramFileName);

        var effectiveSettings = SettingsManager.LoadSettings(entryFilePath);
        var program = new BasicProgram(effectiveSettings, initialDefines, programRootDirectory);

        // La logica di tokenizzazione è ora centralizzata in TokenizeFile.
        var preprocessedFile = TokenizeFile(entryFilePath, sourceText, program);

        // La sourceMap creata è l'unica per questa compilazione.
        program.SourceMap = preprocessedFile.SourceMap;
        program.PreprocessedFiles[entryFilePath] = preprocessedFile;

        // Pre-scansiona gli IMPORT per popolare ImportsByFile prima di ParseAndLinkAllModules
        // (CreateBuiltinNamespaces usa ImportsByFile per determinare quali namespace caricare)
        var imports = ScanSourceForImports(sourceText, entryFilePath);
        program.ImportsByFile[entryFilePath] = imports;

        // ponytail: CompileFromString skips PreProcessProject; linker rewrite resolves modulePath via FileIdToPathMap.
        program.FileIdToPathMap = program.GetFileIdMap().ToImmutableDictionary(kvp => kvp.Value, kvp => kvp.Key);

        // La logica di pre-scansione, parsing e linking è ora interamente
        // gestita da ParseAndLinkAllModules.

        // Parsing del modulo singolo e Linking
        return ParseAndLinkAllModules(entryFilePath, program);
    }

    // ==================================================================================
    // --- FASE 1: PRE-PROCESSING E DISCOVERY ---
    // (Presuppone l'esistenza di DiscoverSourceFiles, PreScanForNamespacesAndImports nel contesto statico)
    // ==================================================================================

    /// <summary>
    /// Esegue il pre-processing iniziale: crea il BasicProgram, scopre i file necessari seguendo
    /// il grafo degli `IMPORT`, risolve i `REQUIRE` e tokenizza tutti i sorgenti una sola volta.
    /// </summary>
    private static BasicProgram PreProcessProject(string entryFilePath,
        IReadOnlyDictionary<string, string?>? initialDefines, string backend = "il")
    {
        var programRootDirectory =
            Path.GetDirectoryName(Path.GetFullPath(entryFilePath)) ?? Directory.GetCurrentDirectory();
        var entryFileName = Path.GetFileName(entryFilePath);
        var effectiveSettings = SettingsManager.LoadSettings(Path.Combine(programRootDirectory, entryFileName));
        var program = new BasicProgram(effectiveSettings, initialDefines, programRootDirectory);

        Log("[Orchestrator] Inizio pre-processing: discovery e tokenizzazione `IMPORT`-driven.");

        // --- FASE 1: Mappatura preliminare dei namespace ---
        // Eseguiamo una scansione di TUTTI i file candidati UNA SOLA VOLTA, ma in modo
        // estremamente leggero (senza usare il Lexer completo), solo per costruire la
        // mappa `namespace -> file` necessaria per risolvere gli IMPORT.
        var allCandidateFiles = DiscoverSourceFiles(programRootDirectory, program.Settings);
        var canonicalEntryFilePath = Path.GetFullPath(entryFilePath);
        allCandidateFiles.Add(canonicalEntryFilePath);
        var namespaceToFileMap = BuildNamespaceMap(allCandidateFiles);
        Log($"[Orchestrator] Mappa preliminare dei namespace costruita da {allCandidateFiles.Count} file candidati.");

        // --- FASE 2: Tokenizzazione `IMPORT`-driven ---
        // Partiamo solo dal file di ingresso e seguiamo le dipendenze.
        var filesToProcess = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var processingQueue = new Queue<string>();
        var fullSourceMapBuilder = ImmutableArray.CreateBuilder<SourceLine>();

        processingQueue.Enqueue(canonicalEntryFilePath);

        while (processingQueue.Count > 0)
        {
            var currentFile = processingQueue.Dequeue();
            if (!filesToProcess.Add(currentFile)) continue; // Se già processato/in coda, salta.

            Log($"[Orchestrator] Processing: {Path.GetFileName(currentFile)}");
            var sourceText = File.ReadAllText(currentFile);

            // **UNICA TOKENIZZAZIONE** per questo file
            var preprocessedFile = TokenizeFile(currentFile, sourceText, program);
            program.PreprocessedFiles[currentFile] = preprocessedFile;
            fullSourceMapBuilder.AddRange(preprocessedFile.SourceMap);

            //Log($"[DIAG-LEXER] Dump dei token generati per il file: {Path.GetFileName(currentFile)}", true);
            //DumpSourceMapAndTokens(preprocessedFile.SourceMap, preprocessedFile.Tokens, currentFile);

            // Estrae gli import dai token appena generati
            var imports = ScanImportsFromTokens(preprocessedFile.Tokens.ToList());
            program.ImportsByFile[currentFile] = imports;

            // Aggiunge le dipendenze alla coda
            foreach (var importedNamespace in imports.Select(i => i.NamespaceName.Lexeme))
            {
                if (!namespaceToFileMap.TryGetValue(importedNamespace, out var dependencyFiles)) continue;
                foreach (var dependencyFile in dependencyFiles.Where(dependencyFile =>
                             !filesToProcess.Contains(dependencyFile)))
                {
                    processingQueue.Enqueue(dependencyFile);
                }
            }
        }

        Log($"[Orchestrator] Grafo delle dipendenze IMPORT risolto. Totale file da compilare: {filesToProcess.Count}.");

        // --- FASE 3: Risoluzione iterativa dei REQUIRE (PackageManager v2 multi-backend+FFI) ---
        var processedPackages = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var packageManager = new Parser.PackageManager();
        // Cache override per test/progetto (GMB_PKG_CACHE, legacy). Default: cache UNICA
        // GmbiPaths.PackagesDir (= <install GMBasic>/packages), condivisa da tutti i programmi.
        if (Environment.GetEnvironmentVariable("GMB_PKG_CACHE") is { } pkgCache && pkgCache.Length > 0)
            packageManager.CacheRoot = pkgCache;
        // Contesto di build: piattaforma corrente + edizione + backend default il.
        // Il C99/VM harness passa backend="c99"/"vm" via PackageBuildContext quando serve
        // (le librerie FFI/binaries vengono scelte di conseguenza).
        var buildCtx = new PackageBuildContext
        {
            PlatformKey = Parser.PackageManager.GetCurrentPlatformKey(),
            Backend = backend
        };

        // ── Lock di risoluzione (riproducibilita') ──────────────────────────────────────
        // `gmb-package.lock` nella cartella del progetto PINNA le versioni: se c'e', la
        // compilazione usa esattamente quelle (e fallisce se una non e' in cache, invece di
        // spostarsi sulla piu' recente). Se NON c'e', la compilazione si comporta come prima e
        // non si inventa alcun file, per non sporcare alberi versionati (samples/, MiniTests/).
        // Chi vuole il lock lo crea con `gmb pkg install` (REPL/CliPackages.DoRestore).
        var projectDir = Path.GetDirectoryName(Path.GetFullPath(entryFilePath)) ?? Directory.GetCurrentDirectory();
        var lockPath = Path.Combine(projectDir, "gmb-package.lock");
        var lockExists = File.Exists(lockPath);
        if (lockExists) Log($"[Orchestrator] Lock presente: {lockPath} (versioni pinned).");

        var iteration = 0;
        while (iteration++ < 10)
        {
            var allRequirements = program.PreprocessedFiles
                .SelectMany(pf =>
                    Parser.ScanRequirementsFromTokens(pf.Value.Tokens, pf.Value.SourceMap, program, pf.Key, StdLib.Instance))
                .ToList();

            var newRequirements = allRequirements.Where(r => processedPackages.Add(r.PackageName.Value)).ToList();
            if (newRequirements.Count == 0)
            {
                Log("[Orchestrator] Nessun nuovo REQUIRE trovato. Fase di risoluzione dipendenze completa.");
                break;
            }

            Log($"[Orchestrator] Trovati {newRequirements.Count} nuovi pacchetti REQUIRE. Avvio risoluzione...");
            try
            {
                packageManager.ResolveDependenciesAsync(newRequirements, buildCtx, lockPath).GetAwaiter().GetResult();
            }
            catch (BasicException ex) when (ex.Code == ErrorCode.FileNotFound)
            {
                // Se il LOCK pinna questo pacchetto, l'installazione deve baricentrare la versione
                // PINNATA, non quella richiesta: scaricare 1.5.10 mentre il lock dice 1.5.6
                // produrrebbe una build non riproducibile (e il lock non potrebbe comunque
                // pinnarla). Prima del fix l'errore utile del lock ("Lock pin X@V non in cache")
                // veniva coperto da "not found in cache and download failed", che non diceva
                // quale versione mancasse ne' che il lock era la causa.
                var locked = lockExists
                    ? Parser.PackageManager.ReadLockFile(lockPath)
                    : new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                foreach (var req in newRequirements)
                {
                    var name = req.PackageName.Value;
                    var pinned = locked.TryGetValue(name, out var lv) ? lv : null;
                    var ver = pinned ?? req.VersionSpecifier?.Value;   // il lock ha la precedenza
                    var pinSuffix = pinned != null ? $" (pinnata dal lock)" : "";
                    Log($"[Orchestrator] Downloading {name}" + (ver != null ? $"@{ver}" : "") + pinSuffix + "...");
                    try
                    {
                        packageManager.DownloadPackageAsync(name, ver, ctx: buildCtx).GetAwaiter().GetResult();
                        Log($"[Orchestrator] Installed {name}" + pinSuffix);
                    }
                    catch (Exception dlEx)
                    {
                        // Messaggio distinto quando e' il lock a impedire la risoluzione: la causa
                        // e l'azione sono diverse (lock vs cache/registry).
                        throw new BasicException(ErrorCode.FileNotFound, req.Token.Position,
                            pinned != null
                                ? $"Pacchetto '{name}' pinnato a {pinned} dal lock {lockPath}, ma la versione non e' in cache e il download fallisce: {dlEx.Message}"
                                : $"Package '{name}' not found in cache and download failed: {dlEx.Message}");
                    }
                }
                packageManager.ResolveDependenciesAsync(newRequirements, buildCtx, lockPath).GetAwaiter().GetResult();
            }

            var newPackageFiles = new List<string>();
            foreach (var path in packageManager.ResolvedModuleSourcePaths)
            {
                // Export che è un FILE (es. dichiarazioni FFI in `.inc`) va incluso così com'è.
                if (File.Exists(path))
                {
                    var full = Path.GetFullPath(path);
                    if (!program.PreprocessedFiles.ContainsKey(full)) newPackageFiles.Add(full);
                    continue;
                }
                if (!Directory.Exists(path)) continue;
                // Export che è una DIRECTORY: includi sia i moduli .bas sia gli include .inc.
                newPackageFiles.AddRange(Directory.GetFiles(path, "*.bas", SearchOption.AllDirectories)
                    .Concat(Directory.GetFiles(path, "*.inc", SearchOption.AllDirectories))
                    .Select(Path.GetFullPath)
                    .Where(file => !program.PreprocessedFiles.ContainsKey(file)));
            }

            if (newPackageFiles.Count == 0) continue;

            Log($"[Orchestrator] Tokenizzazione di {newPackageFiles.Count} nuovi file dai pacchetti...");
            // solo gli exports dei pacchetti contentType vm-chunk sono API da linkare al chunk
            // (i source hanno corpi veri compilati; i dotnet/c99 non hanno API P-Code).
            var vmChunkInstallPaths = packageManager.ResolvedArtifacts.Values
                .Where(a => a.ContentType == PackageContentType.VmChunk && a.InstallPath != null)
                .Select(a => Path.GetFullPath(a.InstallPath).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar)
                .ToList();
            foreach (var packageFile in newPackageFiles)
            {
                var sourceText = File.ReadAllText(packageFile);
                var preprocessedFile = TokenizeFile(packageFile, sourceText, program);
                program.PreprocessedFiles[packageFile] = preprocessedFile;
                fullSourceMapBuilder.AddRange(preprocessedFile.SourceMap);
                // file di EXPORTS di pacchetto vm-chunk: le procedure che dichiara sono l'API
                // linkata dal ChunkMerger all'implementazione nel chunk.
                var full = Path.GetFullPath(packageFile);
                if (vmChunkInstallPaths.Any(v => full.StartsWith(v, StringComparison.OrdinalIgnoreCase)))
                    program.ResolvedExportsFiles.Add(full);
            }
        }

        // Il lock viene RINNOVATO solo se esisteva gia': la compilazione non crea file nuovi
        // (un lock sparso in samples/ o MiniTests/ sarebbe rumore non tracciato). Chi vuole
        // partire da un lock lo crea con `gmb pkg install`.
        if (lockExists)
        {
            try
            {
                packageManager.WriteLockFile(projectDir);
                Log($"[Orchestrator] Lock aggiornato: {lockPath}");
            }
            catch (Exception lockEx)
            {
                // Non e' fatale: la compilazione e' gia' avvenuta con le versioni pinnate.
                Log($"[Orchestrator] Lock non aggiornabile ({lockEx.Message}): compilazione prosegue.");
            }
        }

        // --- FASE 3b: consumo artefatti REQUIRE per-backend + controllo compatibilità ---
        // Ogni pacchetto richiesto deve offrire un artefatto utilizzabile dal backend corrente:
        //   - source  → compilato subito (valido su tutti i backend)
        //   - dotnet  → backend il: la .dll viene caricata via IlModuleLoader (hook
        //     LoadExternalModule, nessuna dipendenza diretta da IlBackend) così i suoi
        //     namespace diventano `Ns.METHOD(...)` compilabili
        //   - c-ffi   → backend c99: libs/headers/link (usati dall'emitter C#)
        //   - vm-chunk→ backend vm: chunk A1/GMBC (usato dal VM runner)
        // Se il pacchetto non fornisce nulla di utilizzabile per QUESTO backend → errore
        // esplicito, invece di compilare e fallire tardi con "funzione non definita".
                var loadableBackend = backend.ToLowerInvariant();
        foreach (var artifact in packageManager.ResolvedArtifacts.Values)
        {
            // I chunk VM (.chunk/.prg) devono essere chunk REALI: hanno un magic header
            // (A1/GMBC: "GMBC"+version per .chunk, "PCOD"/0x50434F44 per .prg). Il package
            // manager seleziona il file solo per esistenza — qui lo validiamo, altrimenti
            // un pacchetto vm-chunk con un file qualsiasi verrebbe eseguito come P-Code.
            if (artifact.ContentType == PackageContentType.VmChunk
                && artifact.ChunkPath != null
                && !IsValidChunkFile(artifact.ChunkPath))
            {
                throw new BasicException(ErrorCode.FfiLoadError, SourcePosition.Unknown,
                    $"Pacchetto '{artifact.PackageName}'@{artifact.Version}: il chunk VM '{artifact.ChunkPath}' " +
                    "non è un chunk GMBasic valido (magic A1/GMBC 'GMBC'+version o .prg 'PCOD' mancante).");
            }
            // Usabile se offre qualcosa che QUESTO backend può consumare:
            //   source (tutti) · binary/ffi (il, c99, vm) · chunk (vm).
            // L'FFI è usabile anche su vm/pcode: il VM/C pre-carica le lib del pacchetto
            // (gmb_ffi_dl_preload, DECLARE LIB per basename) — verificato (una chiamata FFI
            // diretta carica dalla cache pacchetti). Prima il gate ammetteva solo il/c99 e
            // rifiutava i pacchetti c-ffi (mongo, dom, ...) sul backend vm.
            // SelectArtifact ha già scelto l'artefatto per il backend corrente: se BinaryPath
            // esiste è perché il backend lo supporta (altrimenti è null).
            bool usable = artifact.SourceDir != null
                || (loadableBackend is "il" or "c99" or "vm"
                    && (artifact.BinaryPath != null || artifact.FfiLibs.Length > 0))
                || (loadableBackend == "vm" && artifact.ChunkPath != null);
            if (!usable)
                throw new BasicException(ErrorCode.FfiLoadError, SourcePosition.Unknown,
                    $"Pacchetto '{artifact.PackageName}'@{artifact.Version} non compatibile con il backend '{loadableBackend}': " +
                    "nessun artefatto utilizzabile (sorgente, binario/FFI o chunk VM). " +
                    "Specificare un pacchetto con contentType adatto o con exports .bas.");

            if (loadableBackend == "il"
                && artifact.ContentType == PackageContentType.DotNet
                && artifact.BinaryPath != null
                && artifact.BinaryPath.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)
                && StdLib.Instance.LoadExternalModule != null
                && File.Exists(artifact.BinaryPath))
            {
                try
                {
                    var n = StdLib.Instance.LoadExternalModule(Path.GetFullPath(artifact.BinaryPath));
                    Log($"[Orchestrator] Modulo IL caricato da pacchetto {artifact.PackageName}: {n} namespace.");
                }
                catch (Exception modEx)
                {
                    throw new BasicException(ErrorCode.FfiLoadError, SourcePosition.Unknown,
                        $"Modulo IL '{artifact.PackageName}' non caricabile ({artifact.BinaryPath}): {modEx.GetBaseException().Message}");
                }
            }
        }

        // --- FASE 3c: esposizione artefatti ai backend ---
        // Le librerie FFI risolte (path assoluti) e i flag di link C99 restano sul programma:
        // il VM/C le pre-carica (--ffi-lib → gmb_ffi_dl_preload, DECLARE LIB per basename),
        // il C99 le appende al comando `cc` (GetC99LinkInfo), il managed/IL le carica via FfiService.
        program.ResolvedFfiLibs = packageManager.ResolvedArtifacts.Values
            .SelectMany(a => a.FfiLibs)
            .Where(p => !string.IsNullOrEmpty(p))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();
        program.ResolvedC99LinkFlags = packageManager.GetC99LinkInfo().ToCcFlags();
        program.ResolvedVmChunks = packageManager.ResolvedArtifacts.Values
            .Where(a => a.ContentType == PackageContentType.VmChunk && a.ChunkPath != null)
            .Select(a => Path.GetFullPath(a.ChunkPath!))
            .ToList();

        program.SourceMap = fullSourceMapBuilder.ToImmutable();

        // Popola FileIdToPathMap per l'uso O(1) nel Linker/Executor.
        // Inversione della mappa Path->ID.
        program.FileIdToPathMap = program.GetFileIdMap().ToImmutableDictionary(kvp => kvp.Value, kvp => kvp.Key);

        // Un `IMPORT` che non risolve a nessun modulo/built-in è un ERRORE, non un silenzio:
        // qui i sorgenti di progetto, i pacchetti REQUIRE e i moduli IL esterni sono già noti.
        EnsureImportsResolved(program, programRootDirectory);

        Log("[Orchestrator] Pre-processing (discovery e tokenizzazione) completato.");
        global::Basic.Linker.ApplySemanticPatches(program);   /* §4.10 write-side (post-link) */
        return program;
    }

    /// <summary>
    /// True se il lessema può essere il nome di un modulo in `IMPORT` (parola: lettera/_ iniziale,
    /// poi lettere/cifre/_). Serve perché il nome del modulo è la GRAFIA: una keyword del
    /// linguaggio (es. `DASHBOARD`) deve restare importabile anche se il lexer la classifica con
    /// un TokenKind dedicato.
    /// </summary>
    private static bool IsModuleNameLexeme(string lexeme)
    {
        if (string.IsNullOrEmpty(lexeme)) return false;
        if (!char.IsLetter(lexeme[0]) && lexeme[0] != '_') return false;
        foreach (var c in lexeme)
            if (!char.IsLetterOrDigit(c) && c != '_') return false;
        return true;
    }

    /// <summary>
    /// Costruisce una mappa `namespace -> file` (implementazione condivisa:
    /// <see cref="global::Basic.Pipeline.ModuleDiscovery.BuildNamespaceMap"/>).
    /// </summary>
    private static Dictionary<string, List<string>> BuildNamespaceMap(IEnumerable<string> files)
        // Una sola implementazione: Pipeline/ModuleDiscovery.cs (condivisa con BSP/EVAL).
        => global::Basic.Pipeline.ModuleDiscovery.BuildNamespaceMap(files);

    /// <summary>
    /// Esegue una scansione completa e corretta su una lista di token per estrarre
    /// tutte le direttive `IMPORT`, gestendo tutte le varianti sintattiche.
    /// Questa logica è autocontenuta per evitare di istanziare un Parser durante la pre-scansione.
    /// Regole di import:
    /// 1. IMPORT Namespace WITH ALIAS      (Qualificato con nome originale)
    /// 2. IMPORT Namespace AS Alias      (Qualificato con alias)
    /// 3. IMPORT Namespace               (Wildcard non qualificato, simboli importati globalmente)
    /// 4. IMPORT Namespace.{Membro...}   (Selettivo con alias per membro)
    /// 5. Gestisce più dichiarazioni separate da virgola.
    /// IMPORT Namespace1, Namespace2 AS Alias, Namespace3.*, Namespace4.{Membro...}
    /// </summary>
    /// <remarks>
    /// Questo metodo è il cuore della risoluzione delle dipendenze tra moduli nella fase di pre-elaborazione.
    /// Analizza il flusso di token di un file e produce una lista di oggetti <see cref="ImportStmt"/>
    /// che rappresentano fedelmente la sintassi usata nel codice sorgente.
    /// </remarks>
    /// <example>
    /// <b>Sintassi Gestite:</b>
    /// <code>
    /// ' 1. Wildcard non qualificato:
    /// IMPORT IO
    /// IMPORT MyLibrary.*
    /// 
    /// ' 2. Alias di Namespace:
    /// IMPORT System.Math AS Math
    /// 
    /// ' 3. Alias di Namespace (sintassi legacy):
    /// IMPORT MyLibrary WITH ALIAS
    /// 
    /// ' 4. Import Selettivo:
    /// IMPORT IO.{KILL, RENAME}
    /// IMPORT STRINGS.{Replace AS StrReplace, Left}
    /// 
    /// ' 5. Multi-Import:
    /// IMPORT IO, STRINGS, DBF
    /// </code>
    /// </example>
    /// <param name="tokens">La sequenza di token pre-elaborati del file da scansionare.</param>
    /// <returns>Una lista di nodi <see cref="ImportStmt"/> che rappresentano tutte le direttive trovate.</returns>
    /// <exception cref="BasicException">Lanciata in caso di errore di sintassi durante la scansione.</exception>
    private static List<ImportStmt> ScanImportsFromTokens(List<Token> tokens)
    {
        const bool log = false;
        var imports = new List<ImportStmt>();
        var current = 0;
        var atStartOfLine = true;

        Log($"[ScanImports] Inizio scansione di {tokens.Count} token per direttive IMPORT...", log);

        while (!IsAtEnd())
        {
            var token = Peek();

            if (token.Kind == TokenKind.Newline)
            {
                atStartOfLine = true;
                Advance();
                continue;
            }

            if (atStartOfLine)
            {
                if (token.Kind == TokenKind.LineNumber)
                {
                    Advance();
                    token = Peek();
                }
            }

            if (atStartOfLine && token.Kind == TokenKind.Import)
            {
                Match(TokenKind.Import);
                Log($"[ScanImports]   - Trovata keyword 'IMPORT' alla posizione token {current - 1}.", log);

                do
                {
                    // --- LOGICA UNIFICATA PER PARSARE NOMI QUALIFICATI ---
                    var namespaceParts = new List<Token>();
                    var savedPos = current; // Salva la posizione prima di provare a parsare il nome
                    
                    // Parsa una sequenza di identificatori separati da punti
                    do
                    {
                        var part = Peek();
                        // Accetta un token se è un Identifier oppure se è una keyword di namespace
                        // builtin (es. IMPORT WINDOW, IMPORT MATRIX) — i namespace sono anche TokenKind.
                        // Inoltre: il nome di un modulo è la sua GRAFIA, quindi qualunque parola è
                        // importabile anche quando il lexer la classifica come keyword (es.
                        // `IMPORT DASHBOARD`, dove DASHBOARD è TokenKind.Dashboard perché esiste il
                        // comando omonimo). Si rifiutano solo i token che NON possono essere un nome
                        // (newline, operatori, letterali, parentesi, virgole, ...).
                        if (part.Kind != TokenKind.Identifier &&
                            !StdLib.Instance.BuiltinProcedures.ContainsKey(part.Lexeme) &&
                            !StdLib.Instance.BuiltinNamespaceNames.Contains(part.Lexeme) &&
                            !IsModuleNameLexeme(part.Lexeme))
                        {
                             if (namespaceParts.Count > 0) break; 
                             Log($"[ScanImports]   - ERRORE: Trovato token non valido '{part.Lexeme}' dopo IMPORT. Interruzione analisi della riga.", log);
                             throw new BasicException(part, "Expected a namespace name after 'IMPORT' or ','.");
                        }
                        namespaceParts.Add(Advance());
                    } while (Match(TokenKind.Dot) && Check(TokenKind.Identifier)); // Continua solo se dopo il punto c'è un altro identificatore

                    // Se dopo un punto non c'è un identificatore (es. "NS.{"), riavvolgi l'ultimo punto.
                    if (current > 0 && tokens[current - 1].Kind == TokenKind.Dot)
                    {
                        current--;
                    }

                    var fullNamespaceName = string.Join(".", namespaceParts.Select(p => p.Lexeme));
                    var namespaceToken = namespaceParts.First() with { Lexeme = fullNamespaceName };
                    // --- FINE LOGICA NOMI QUALIFICATI ---

                    Log($"[ScanImports]   - Analizzo import per namespace '{namespaceToken.Lexeme}'.", log);

                    ImportStmt newImport;
                    if (Match(TokenKind.As))
                    {
                        var aliasToken = Consume(TokenKind.Identifier, "Expected an alias name after 'AS'.");
                        newImport = new ImportStmt(namespaceToken, aliasToken, ImportMode.Namespace,
                            ImmutableArray<ImportedMember>.Empty);
                        Log($"[ScanImports]     -> Modalità: Namespace con Alias '{aliasToken.Lexeme}'.", log);
                    }
                    else if (Match(TokenKind.With))
                    {
                        Consume(TokenKind.Alias, "Expected 'ALIAS' after 'WITH'.");
                        newImport = new ImportStmt(namespaceToken, namespaceToken, ImportMode.Namespace,
                            ImmutableArray<ImportedMember>.Empty);
                        Log("[ScanImports]     -> Modalità: Namespace con Alias legacy.", log);
                    }
                    else if (Check(TokenKind.LeftParen))
                    {
                        // IMPORT Ns (a, b, c) [AS alias] — selettivo con parentesi tonde (senza punto).
                        Advance(); // Consuma '('.
                        var members = ImmutableArray.CreateBuilder<ImportedMember>();
                        if (!Check(TokenKind.RightParen))
                        {
                            do
                            {
                                if (Check(TokenKind.RightParen)) break;
                                var originalName = ConsumeProcedureName();
                                Token? memberAlias = null;
                                if (Match(TokenKind.As))
                                {
                                    memberAlias = Consume(TokenKind.Identifier,
                                        "Expected an alias for the imported member.");
                                }

                                members.Add(new ImportedMember(originalName, memberAlias));
                            } while (Match(TokenKind.Comma));
                        }

                        Consume(TokenKind.RightParen, "Expected ')' to close selective import list.");

                        // Dopo la lista selettiva può esserci "AS alias" (alias del namespace).
                        Token? nsAlias = null;
                        if (Match(TokenKind.As))
                            nsAlias = Consume(TokenKind.Identifier, "Expected an alias name after 'AS'.");

                        newImport = new ImportStmt(namespaceToken, nsAlias, ImportMode.SpecificMembers,
                            members.ToImmutable());
                        Log($"[ScanImports]     -> Modalità: SpecificMembers tonde ({members.Count} membri).", log);
                    }
                    else if (Match(TokenKind.Dot))
                    {
                        if (Match(TokenKind.Multiply))
                        {
                            newImport = new ImportStmt(namespaceToken, null, ImportMode.AllMembers,
                                ImmutableArray<ImportedMember>.Empty);
                            Log("[ScanImports]     -> Modalità: AllMembers (wildcard '*').", log);
                        }
                        else
                        {
                            throw new BasicException(Peek(), "Expected '*' after '.' for an import statement.");
                        }
                    }
                    else
                    {
                        newImport = new ImportStmt(namespaceToken, null, ImportMode.AllMembers,
                            ImmutableArray<ImportedMember>.Empty);
                        Log("[ScanImports]     -> Modalità: AllMembers (default).", log);
                    }

                    imports.Add(newImport);
                } while (Match(TokenKind.Comma));
            }

            if (token.Kind != TokenKind.Newline)
            {
                atStartOfLine = false;
            }

            if (token == Peek())
            {
                Advance();
            }
        }

        Log($"[ScanImports] Scansione terminata. Trovati {imports.Count} import totali.", log);
        return imports;

        bool Match(TokenKind kind)
        {
            if (!Check(kind)) return false;
            Advance();
            return true;
        }

        bool Check(TokenKind kind) => !IsAtEnd() && Peek().Kind == kind;

        Token Consume(TokenKind kind, string message) =>
            Check(kind) ? Advance() : throw new BasicException(Peek(), message);

        Token Advance() => tokens[current++];

        Token Peek() => tokens[current];

        bool IsAtEnd() => current >= tokens.Count || tokens[current].Kind == TokenKind.Eof;

        Token ConsumeProcedureName()
        {
            var token = Peek();
            if (!Parser.IsStructuralKeyword(token.Kind)) return Advance();
            throw new BasicException(token,
                $"'{token.Lexeme}' è una keyword strutturale e non può essere usata come nome.");
        }
    }

    // ==================================================================================
    // --- VALIDAZIONE DEGLI IMPORT: un `IMPORT` senza modulo è un ERRORE ---
    // ==================================================================================

    /// <summary>Un `IMPORT &lt;Ns&gt;` deve risolvere a qualcosa: un file compilato che dichiara
    /// `NAMESPACE &lt;Ns&gt;`, un namespace built-in della StdLib, un namespace di default dei settings,
    /// uno esposto da un modulo IL esterno (`--il-module`) o uno dei namespace base del linguaggio.
    ///
    /// PERCHÉ: un import irrisolvibile veniva IGNORATO. Il programma compilava senza il modulo e
    /// falliva molto dopo ("Nessuna funzione pubblica ..."), oppure funzionava per caso quando il
    /// simbolo non veniva mai usato. Caso reale: `gmbcs` copiato fuori dal checkout, senza
    /// `shared/include/`, dichiarava `OK` un programma con `IMPORT PKGTOOLS` e poi sbagliava la
    /// chiamata a `PKGTOOLS.BASENAME(...)`.</summary>
    private static void EnsureImportsResolved(BasicProgram program, string programRootDirectory)
    {
        var declared = BuildNamespaceMap(program.PreprocessedFiles.Keys);
        var stdLib = StdLib.Instance;
        var baseNamespaces = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            { "SYSTEM", "CONSOLE", "CONSTANTS", "TRAIN", "GMB" };
        string? searchRoots = null;   // calcolato solo se serve (e solo per il messaggio)

        foreach (var (filePath, imports) in program.ImportsByFile)
        {
            foreach (var import in imports)
            {
                var ns = import.NamespaceName.Lexeme;
                if (string.IsNullOrWhiteSpace(ns)) continue;
                if (declared.ContainsKey(ns)) continue;
                if (baseNamespaces.Contains(ns)) continue;
                if (program.Settings.DefaultNamespaces.Contains(ns, StringComparer.OrdinalIgnoreCase)) continue;
                if (stdLib.BuiltinNamespaceNames.Contains(ns)) continue;
                if (stdLib.ExternalModuleNames.Contains(ns)) continue;
                if (stdLib.BuiltinProcedures.ContainsKey(ns) || stdLib.BuiltinTypes.ContainsKey(ns)
                    || stdLib.BuiltinGlobals.ContainsKey(ns) || stdLib.BuiltinConstants.ContainsKey(ns)) continue;

                searchRoots ??= string.Join(", ", global::Basic.Pipeline.ModuleDiscovery.ResolveSearchFolders(
                    program.Settings.Compiler.SearchFolders, programRootDirectory));
                throw new BasicException(ErrorCode.FileNotFound, import.NamespaceName.Position,
                    $"modulo non trovato: IMPORT {ns} ({Path.GetFileName(filePath)}) — nessun file dichiara " +
                    $"'NAMESPACE {ns}' e non è un namespace built-in. Cartelle di ricerca: {searchRoots}");
            }
        }
    }

    // ==================================================================================
    // --- FASE 2 & 3: ORCHESTRAZIONE DEL PARSING E LINKING ---
    // ==================================================================================

    /// <summary>
    /// Orchestra la "Passata 1" (Parsing Puro) e le fasi successive di linking e analisi semantica.
    /// </summary>
    /// <param name="mainFilePath">Il percorso del file principale.</param>
    /// <param name="program">L'istanza di BasicProgram contenente lo stato corrente della compilazione.</param>
    /// <returns>L'istanza di BasicProgram completamente linkata e pronta per l'esecuzione.</returns>
    /// <remarks>
    /// Il processo si articola in tre passate principali, tutte gestite in sequenza per garantire l'esecuzione
    /// deterministica:
    /// 
    /// FASE 1: Parsing Puro e Generazione AST Iniziale
    /// Trasforma i token in un Abstract Syntax Tree (AST) "spurio", raccogliendo tutte le dichiarazioni (procedure,
    /// tipi, classi, globali) in una lista grezza (module.Declarations) senza risolvere alcun simbolo definito dall'utente.
    /// 
    /// FASE 2: Analisi Semantica e Popolamento SymbolTable
    /// Attraversa le dichiarazioni generate nella FASE 1 e costruisce la SymbolTable finale. Assegna SymbolId univoci a
    /// ogni entità, risolvendo le firme delle procedure e creando la gerarchia di scope completa (Root, Namespace,
    /// Procedure, Class).
    /// 
    /// FASE 3: Linking, Inferenza Tipi e Riscrittura AST
    /// Esegue passate multiple sull'AST utilizzando la SymbolTable completa (FASE 2). Questo processo risolve tutti i
    /// riferimenti, inferisce i tipi impliciti (es. per le dichiarazioni senza clausola AS), esegue il constant folding
    /// e trasforma l'AST nel formato finale ottimizzato e validato, pronto per l'Executor.
    /// 
    /// </remarks>
    private static BasicProgram ParseAndLinkAllModules(string mainFilePath, BasicProgram program)
    {
        const bool log = false;
        ArgumentNullException.ThrowIfNull(program);
        Log("\n[Orchestrator] === INIZIO FLUSSO DI COMPILAZIONE (PIPELINE) ===", log);

        // --- FASE PRELIMINARE: Inizializzazione dei built-in ---
        Log("[Orchestrator] Inizializzazione libreria standard (Built-ins)...", log);
        CreateBuiltinNamespaces(program);

        var allFiles = program.PreprocessedFiles.Keys.ToList();

        // --- PASSATA 1: Parsing Puro per costruire l'AST "spurio" ---
        Log("\n[Orchestrator] === INIZIO PASSATA 1: Parsing Puro dell'AST ===", log);
        foreach (var filePath in allFiles)
        {
            if (!program.PreprocessedFiles.TryGetValue(filePath, out var preprocessedFile)) continue;

            program.Symbols.ResetScope();

            Log($"[Orchestrator] Passata 1 su: {Path.GetFileName(filePath)}", log);
            var parserPass1 = new Parser(preprocessedFile.Tokens, preprocessedFile.SourceMap, program, filePath, StdLib.Instance);

            var imports = program.ImportsByFile.GetValueOrDefault(filePath) ?? [];
            var moduleWithSpuriousAst = parserPass1.ParseModuleSource(imports);

            program.LoadedModules[filePath] = moduleWithSpuriousAst;
        }

        Log("[Orchestrator] === FINE PASSATA 1 ===", log);

        //Log("\n[Orchestrator] === DUMP AST FASE 1 ===", log);
        //Parser.DumpAstWithSymbolsAndScopes(program);

        // --- PASSATA 2: Analisi Semantica e Popolamento SymbolTable ---
        Log("\n[Orchestrator] === INIZIO PASSATA 2: Popolamento della SymbolTable ===", log);
        Linker.PopulateSymbolTable(program);
        Log("[Orchestrator] === FINE PASSATA 2 ===", log);

        // I simboli esistono soltanto dopo FASE 2 e sono stati risolti in FASE 2.5
        //Log("\n[Orchestrator] === DUMP SYMBOL TABLE DOPO FASE 2 ===", log);
        //Parser.DumpSymbols(program, userCodeOnly: true);

        //Log("\n[Orchestrator] === DUMP AST CON SYMBOLID E SCOPES FASE 2 ===", log);
        //Parser.DumpAstWithSymbolsAndScopes(program);

        // --- PASSATA 2.5: Risoluzione degli ID dei Simboli nell'AST ---
        Log("\n[Orchestrator] === INIZIO PASSATA 2.5: Risoluzione ID Simboli ===", log);
        Linker.ResolveSymbolIds(program);
        Log("[Orchestrator] === FINE PASSATA 2.5 ===", log);

        //Parser.DumpAstWithSymbolsAndScopes(program);

        // I simboli esistono soltanto dopo FASE 2 e sono stati risolti in FASE 2.5
        //Log("\n[Orchestrator] === DUMP SYMBOL TABLE DOPO FASE 2.5 (ID Risolti) ===", log);
        //Parser.DumpSymbols(program, userCodeOnly: true);

        //Log("\n[Orchestrator] === DUMP AST CON SYMBOLID E SCOPES FASE 2.5 (ID Risolti) ===", log);
        //Parser.DumpAstWithSymbolsAndScopes(program);

        // Log("\n[Orchestrator] === DUMP AST DOPO FASE 2.5 (ID Risolti) ===", true);
        // Parser.DumpASTHierarchical(program, showAllModules: true, useConsole: true);

        // --- FASE 3: LINKING FINALE (ORA SUDDIVISO IN PASSATE ESPLICITE) ---
        Log("\n[Orchestrator] === INIZIO PASSATA 3: Linking Finale ===", log);
        var canonicalMainPath = Path.GetFullPath(mainFilePath);

        // Passata 3.1: Esegue il "constant folding" e valida l'assenza di simboli duplicati.
        Linker.PerformStaticAnalysisAndValidation(program);
        Log("[Orchestrator] Passata 3.1 (Aggregazione e Validazione) completata.", log);

        //Log("\n[Orchestrator] === DUMP AST CON SYMBOLID E SCOPES PRIMA DI OptimizeAsts  ===", log);
        //Parser.DumpAstWithSymbolsAndScopes(program);

        // Passata 3.2: Applica ottimizzazioni all'AST, come l'appiattimento dei cicli (Loop Flattening).
        Linker.OptimizeAsts(program);
        Log("[Orchestrator] Passata 3.2 (Ottimizzazione AST) completata.", log);

        //Log("\n[Orchestrator] === DUMP SYMBOL TABLE DOPO OptimizeAsts ===", log);
        //Parser.DumpSymbols(program, userCodeOnly: true);

        // Passata 3.3: Risolve la gerarchia di ereditarietà delle classi.
        // DEVE essere eseguita PRIMA della riscrittura (3.4) per popolare `ResolvedBaseClass`,
        // informazione necessaria per il type-checking polimorfico.
        Linker.ResolveAndValidateClassInheritance(program);
        Log("[Orchestrator] Passata 3.3 (Ereditarietà Classi) completata.", log);

        // Passata 3.3a: scopre controller e azioni MVC usando esclusivamente simboli risolti.
        // Senza controller il passaggio è un no-op completo.
        Linker.PrepareMvcBridges(program, mainFilePath);
        Log("[Orchestrator] Passata 3.3a (Preparazione bridge MVC) completata.", log);

        // Passata 3.4: Esegue la riscrittura finale dell'AST. Risolve le chiamate a funzione,
        // inferisce i tipi di dato (`InferredType` -> `DoubleType`, etc.) e valida la semantica delle espressioni.
        Linker.ResolveAndRewriteModuleAsts(program);
        Log("[Orchestrator] Passata 3.4 (Riscrittura AST) completata.", log);

        // Passata 3.4b: la riscrittura sostituisce ogni nodo con una nuova istanza immutabile,
        // invalidando i riferimenti diretti (es. LabelLocation.BlockStatements) registrati dalla
        // Passata 3.2 sull'AST pre-riscrittura. Ricostruisce la mappa etichette sull'AST finale.
        Linker.RebuildBlockScopedLabelReferences(program);
        Log("[Orchestrator] Passata 3.4b (Ricostruzione etichette post-riscrittura) completata.", log);

        //Log("\n[Orchestrator] === DUMP AST CON SYMBOLID E SCOPES DOPO FASE 3  ===", log);
        //Parser.DumpAstWithSymbolsAndScopes(program);

        // Passata 3.5: Validazione finale dei tipi (Post-Rewrite).
        // Intercetta accessi ai campi su tipi primitivi e mismatch di assegnazione residui.
        Linker.ValidateTypes(program);
        Log("[Orchestrator] Passata 3.5 (Validazione Tipi) completata.", log);
        
        // Passata 3.6: Validazione finale del punto di ingresso del programma.
        if (!program.LoadedModules.TryGetValue(canonicalMainPath, out var rewrittenMainModule)
            && !program.LoadedModules.TryGetValue(mainFilePath, out rewrittenMainModule))
            throw new InvalidOperationException(
                $"Internal Linker Error: Rewritten main module not found for path '{canonicalMainPath}'.");

        program.MainModule = rewrittenMainModule;

        // Modalita' LIBRARY: nessun entry point (il prodotto e' una libreria, non un eseguibile).
        if (rewrittenMainModule.LibraryName == null && !Linker.HasEntryPoint(rewrittenMainModule, program))
            throw new BasicException(ErrorCode.SyntaxError, SourcePosition.Unknown,
                "No valid entry point found. The program must contain either a SUB Main or at least one line number.");

        // Passata 3.6b: vieta la chiamata esplicita all'entry-point Main (già auto-eseguito da
        // ogni backend) — senza questo controllo IL/PCode/VM-C rieseguono Main una seconda volta
        // in modo silenzioso e divergente da C99/C99Native (bug
        // "Call-Main-Esplicita-Entry-Doppia-VMC-Parita").
        Linker.ValidateNoExplicitMainCall(program, rewrittenMainModule);

        Log("[Orchestrator] === FINE PASSATA 3 ===", log);

        global::Basic.Linker.ApplySemanticPatches(program);   /* §4.10 write-side (post-link) */
        return program;
    }

    /// <summary>
    /// Inizializza la SymbolTable del programma corrente (program.Symbols) creando copie
    /// delle definizioni pre-caricate dalla libreria standard (StdLib.Instance).
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA: carica i namespace da SYSTEM,
    /// dalla configurazione e dagli IMPORT, ma solo quelli effettivamente presenti in StdLib.
    /// Gestisce inoltre la registrazione delle variabili globali built-in.
    /// </summary>
    /// <param name="program">Il contesto del programma corrente.</param>
    private static void CreateBuiltinNamespaces(BasicProgram program)
    {
        const bool log = false;
        Log("[BUILTINS] Inizio caricamento namespace built-in (copia da StdLib)...", log);

        Log(
            $"[BUILTINS-DIAG] Verifico contenuto di program.ImportsByFile. Conteggio file: {program.ImportsByFile.Count}",
            log);
        foreach (var (filePath, imports) in program.ImportsByFile)
        {
            var importNames = imports.Select(i => i.NamespaceName.Lexeme);
            Log(
                $"[BUILTINS-DIAG]   - File: {Path.GetFileName(filePath)} -> Imports: [{string.Join(", ", importNames)}]",
                log);
        }

        var stdLib = StdLib.Instance;

        // "CONSTANTS" ospita le costanti PowerBASIC-style (%RED, %ESC, %JOYUP, %FILE_CREATED...)
        // sempre disponibili senza IMPORT. SYSTEM e CONSOLE sono i namespace base del linguaggio:
        // le loro costanti, funzioni e comandi sono sempre disponibili anche se l'utente imposta
        // defaultNamespaces vuoto nei settings.
        // TRAIN (namespace PUL per la produzione/alterazione dei pesi) è sempre disponibile come
        // CONSTANTS: il suo dispatcher (F0) delega ai sottoparser AI/LLM esistenti, quindi non ha
        // simboli propri ma deve superare il gate dei namespace in ParseStatement.
        var namespacesToConsider = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "SYSTEM", "CONSOLE", "CONSTANTS", "TRAIN", "GMB" };
        Log("[BUILTINS]   - Aggiunto namespace 'SYSTEM' di base.", log);

        Log(
            $"[BUILTINS]   - Aggiungo {program.Settings.DefaultNamespaces.Count} namespace di default dalla configurazione...",
            log);
        foreach (var nsName in program.Settings.DefaultNamespaces)
        {
            namespacesToConsider.Add(nsName);
        }

        Log($"[BUILTINS]   - Aggiungo namespace dagli IMPORT di {program.ImportsByFile.Count} file...", log);
        foreach (var (filePath, imports) in program.ImportsByFile)
        {
            Log($"[BUILTINS]     - File: {Path.GetFileName(filePath)}", log);
            foreach (var nsName in imports.Select(import => import.NamespaceName.Lexeme))
            {
                namespacesToConsider.Add(nsName);
            }
        }

        // Moduli IL esterni caricati con IlModuleLoader (IL loader generico, non solo
        // PackageManager): i loro namespace devono essere visibili a parser/linker perché
        // `NS.METHOD(...)` venga risolto come chiamata builtin (l'emissione resta IL-only).
        foreach (var nsName in stdLib.ExternalModuleNames)
        {
            namespacesToConsider.Add(nsName);
        }

        program.Namespaces = namespacesToConsider;
        Log($"[BUILTINS] Lista finale dei namespace da considerare: [{string.Join(", ", namespacesToConsider)}]", log);

        foreach (var nsName in namespacesToConsider)
        {
            var hasProcedures = stdLib.BuiltinProcedures.ContainsKey(nsName);
            var hasTypes = stdLib.BuiltinTypes.ContainsKey(nsName);
            var hasGlobals = stdLib.BuiltinGlobals.ContainsKey(nsName);
            var hasConstants = stdLib.BuiltinConstants.ContainsKey(nsName);

            if (!hasProcedures && !hasTypes && !hasGlobals && !hasConstants)
            {
                Log($"[BUILTINS]   -> '{nsName}' non trovato in StdLib. Ignorato.", log);
                continue;
            }

            program.Symbols.EnterScope(nsName, SymbolTable.ScopeType.Namespace);
            Log($"[BUILTINS] Copio e registro definizioni per namespace '{nsName}' in SymbolTable.", log);
            try
            {
                if (hasProcedures)
                {
                    foreach (var procTemplate in stdLib.BuiltinProcedures[nsName])
                    {
                        var returnType = procTemplate switch
                        {
                            FunctionDefinition fd => fd.ReturnType,
                            GeneratorDefinition gd => new GeneratorType(gd.YieldType),
                            NativeProcedureDefinition npd => npd.ReturnType,
                            ExternalProcedureDefinition epd => epd.ReturnType ?? new VoidType(),
                            _ => new VoidType()
                        };
                        var newSymbolId = program.Symbols.RegisterSymbol(procTemplate.OriginalName, procTemplate.Flags,
                            returnType, procTemplate);

                        ProcedureDefinition copiedProcWithId = procTemplate switch
                        {
                            NativeProcedureDefinition npd => npd with { SymbolId = newSymbolId },
                            FunctionDefinition fd => fd with { SymbolId = newSymbolId },
                            GeneratorDefinition gd => gd with { SymbolId = newSymbolId },
                            ExternalProcedureDefinition epd => epd with { SymbolId = newSymbolId },
                            _ => throw new InvalidOperationException("Tipo di procedura built-in non supportato.")
                        };

                        program.Symbols.UpdateSymbolDeclaration(newSymbolId, copiedProcWithId);
                    }
                }

                if (hasTypes)
                {
                    foreach (var (typeName, typeTemplate) in stdLib.BuiltinTypes[nsName])
                    {
                        // La logica specifica per EnumDefinition è stata rimossa perché non è più un tipo.
                        // I tipi Enum (es. ONKEY.KEY) sono registrati come un alias di tipo.
                        const SymbolFlags flags = SymbolFlags.IsPublic |
                                                              SymbolFlags.IsTypeAlias;
                        var typeToken = new Token(TokenKind.Identifier, typeName, null, SourcePosition.Unknown);

                        if (typeTemplate is UserDefinedType udtTemplate)
                        {
                            // ponytail: inline RegisterUdt for builtin UDTs (LEDGERENTRY field SymbolIds); upgrade = shared helper
                            var typeSymbolId = program.Symbols.RegisterSymbol(typeToken, flags, udtTemplate, udtTemplate);
                            program.Symbols.EnterScope(typeName, SymbolTable.ScopeType.Type, typeSymbolId);
                            try
                            {
                                var udtScopeId = program.Symbols.GetCurrentScope().Id;
                                var updatedFields = udtTemplate.Fields.Select(field =>
                                {
                                    const SymbolFlags fieldFlags = SymbolFlags.IsField | SymbolFlags.IsPublic;
                                    var fieldId = program.Symbols.RegisterSymbol(field.Name, fieldFlags, field.DataType, field);
                                    return field with { SymbolId = fieldId, ScopeId = udtScopeId };
                                }).ToImmutableArray();

                                var finalUdt = udtTemplate with { SymbolId = typeSymbolId, Fields = updatedFields };
                                program.Symbols.UpdateSymbolDeclaration(typeSymbolId, finalUdt);
                            }
                            finally
                            {
                                program.Symbols.ExitScope();
                            }
                        }
                        else if (typeTemplate is ClassType classTypeTemplate)
                        {
                            // Base classi builtin (es. CONTROLS.Control): nodo di dichiarazione =
                            // ClassDefinition, così `INHERITS <base>` la trova (FindSymbolsByName).
                            program.Symbols.RegisterSymbol(typeToken, flags, classTypeTemplate, classTypeTemplate.Definition);
                        }
                        else
                        {
                            program.Symbols.RegisterSymbol(typeToken, flags, typeTemplate, typeTemplate);
                        }
                    }
                }

                if (hasGlobals)
                {
                    var isSystemNamespace = nsName.Equals("SYSTEM", StringComparison.OrdinalIgnoreCase);
                    if (isSystemNamespace)
                    {
                        program.Symbols.ExitScope();
                    }

                    foreach (var (name, type, flags) in stdLib.BuiltinGlobals[nsName])
                    {
                        var varToken = new Token(TokenKind.Identifier, name, null, SourcePosition.Unknown);
                        var declNode = new VariableDeclaration(-1, varToken, varToken, ImmutableArray<Expr>.Empty, type,
                            null,
                            false, true, flags);

                        var newSymbolId = program.Symbols.RegisterSymbol(varToken, flags, type, declNode);
                        var updatedDecl = declNode with { SymbolId = newSymbolId };
                        program.Symbols.UpdateSymbolDeclaration(newSymbolId, updatedDecl);
                    }

                    if (isSystemNamespace)
                    {
                        program.Symbols.EnterScope(nsName, SymbolTable.ScopeType.Namespace);
                    }
                }

                if (hasConstants)
                {
                    foreach (var constTemplate in stdLib.BuiltinConstants[nsName])
                    {
                        var newSymbolId = program.Symbols.RegisterSymbol(constTemplate.OriginalToken,
                            constTemplate.Flags, constTemplate.DataType ?? new InferredType(), constTemplate);
                        var updatedConst = constTemplate with { SymbolId = newSymbolId };
                        program.Symbols.UpdateSymbolDeclaration(newSymbolId, updatedConst);
                    }
                }
            }
            finally
            {
                program.Symbols.ExitScope();
            }
        }

        Log("[BUILTINS] Caricamento e sincronizzazione StdLib completati.", log);
    }

    /// <summary>
    /// Esegue la tokenizzazione di un singolo sorgente. Se il file è già stato tokenizzato e non è richiesto un refresh,
    /// restituisce i dati dalla cache. Altrimenti, crea la source map dal testo sorgente, esegue il lexer
    /// e restituisce il risultato. Assegna un FileId univoco al file.
    /// </summary>
    /// <param name="filePath">Il percorso del file da tokenizzare, usato come chiave per la cache.</param>
    /// <param name="sourceText">Il contenuto testuale completo del file sorgente.</param>
    /// <param name="program">Il contesto del programma che contiene la cache e la gestione dei FileId.</param>
    /// <param name="refresh">Se true, forza la ri-tokenizzazione anche se il file è in cache.</param>
    /// <returns>Un <see cref="BasicProgram.PreprocessedFile"/> contenente i token e la source map.</returns>
    private static BasicProgram.PreprocessedFile TokenizeFile(string filePath, string sourceText, BasicProgram program,
        bool refresh = false)
    {
        // Logica di caching: se non è richiesto un refresh e il file è già stato processato, restituisce il risultato dalla cache.
        if (!refresh && program.PreprocessedFiles.TryGetValue(filePath, out var cachedFile))
        {
            return cachedFile;
        }

        // Fase 1: Ottieni o crea un FileId univoco per questo percorso di file.
        // Questa logica deve essere aggiunta a BasicProgram.
        var fileId = program.GetOrAddFileId(filePath);

        // Fase 2: Crea la SourceMap usando il FileId corretto.
        var lines = sourceText.Split(["\r\n", "\r", "\n"], StringSplitOptions.None);
        var sourceMapBuilder = ImmutableArray.CreateBuilder<SourceLine>();
        for (var i = 0; i < lines.Length; i++)
        {
            // Ogni SourceLine ora contiene il FileId corretto.
            sourceMapBuilder.Add(new SourceLine(fileId, filePath, i + 1, lines[i]));
        }

        var sourceMap = sourceMapBuilder.ToImmutable();

        // Fase 3: Esegui il Lexer.
        var lexer = new Lexer(sourceMap, program, Opt.VerboseLexer);
        var tokens = lexer.ScanTokens().ToImmutableArray();

        // Forza il dump dei token e del codice ricostruito per ogni file.
        // Questo ci permetterà di ispezionare l'output del pre-processore
        // e confermare la duplicazione dei token nel file 'aab.bas'.
        //DumpSourceMapAndTokens(sourceMap, tokens, filePath);

        return new BasicProgram.PreprocessedFile(tokens, sourceMap);
    }

    /// <summary>Valida il magic di un chunk VM: .prg → PrgFormat.IsValid (magic "PCOD"),
    /// .chunk (A1/GMBC) → 5 byte "GMBC" + version 1. Ritorna false per file non-chunk.</summary>
    private static bool IsValidChunkFile(string path)
    {
        try
        {
            if (!File.Exists(path)) return false;
            if (path.EndsWith(".prg", StringComparison.OrdinalIgnoreCase))
                return Basic.Runtime.PrgFormat.IsValid(path);
            using var stream = File.OpenRead(path);
            if (stream.Length < 5) return false;
            Span<byte> hdr = stackalloc byte[5];
            stream.ReadExactly(hdr);
            return hdr[0] == (byte)'G' && hdr[1] == (byte)'M' && hdr[2] == (byte)'B' && hdr[3] == (byte)'C'
                && hdr[4] == 1; // GMB_CHUNK_VERSION
        }
        catch { return false; }
    }

    /// <summary>
    /// Scansione testuale del sorgente per trovare le istruzioni IMPORT e popolare ImportsByFile.
    /// Necessario per CompileFromString dove non c'è pre-processing file-based.
    /// </summary>
    private static List<ImportStmt> ScanSourceForImports(string sourceText, string entryFilePath)
    {
        var imports = new List<ImportStmt>();
        using var reader = new StringReader(sourceText);
        string? line;
        while ((line = reader.ReadLine()) != null)
        {
            var trimmed = line.Trim();
            var importIdx = trimmed.IndexOf("IMPORT ", StringComparison.OrdinalIgnoreCase);
            if (importIdx < 0)
                continue;

            var rest = trimmed[(importIdx + "IMPORT ".Length)..].Trim();
            if (string.IsNullOrWhiteSpace(rest))
                continue;

            // Il primo token è il nome del namespace.
            var nsEnd = rest.IndexOfAny([' ', '(']);
            string nsName;
            string remainder;
            if (nsEnd < 0)
            {
                nsName = rest;
                remainder = "";
            }
            else
            {
                nsName = rest[..nsEnd].Trim();
                remainder = rest[nsEnd..].Trim();
            }

            var nsToken = new Token(TokenKind.Identifier, nsName.TrimEnd('.'), null, SourcePosition.Unknown);
            ImportStmt import;

            // Forma wildcard: IMPORT Ns.* → AllMembers.
            if (nsName.EndsWith(".*", StringComparison.OrdinalIgnoreCase))
            {
                var cleanNs = new Token(TokenKind.Identifier, nsName[..^2].TrimEnd('.'), null, SourcePosition.Unknown);
                import = new ImportStmt(cleanNs, null, ImportMode.AllMembers, []);
                imports.Add(import);
                continue;
            }

            // Forma selettiva: IMPORT Ns (a, b, c) [AS alias].
            if (remainder.StartsWith('('))
            {
                var closeIndex = remainder.IndexOf(')');
                if (closeIndex < 0)
                {
                    import = new ImportStmt(nsToken, null, ImportMode.AllMembers, []);
                    imports.Add(import);
                    continue;
                }

                var content = remainder.Substring(1, closeIndex - 1).Trim();
                var afterClose = remainder[(closeIndex + 1)..].Trim();

                Token? nsAlias = null;
                if (afterClose.StartsWith("AS ", StringComparison.OrdinalIgnoreCase))
                    nsAlias = new Token(TokenKind.Identifier, afterClose[3..].Trim(), null, SourcePosition.Unknown);

                var members = new List<ImportedMember>();
                foreach (var raw in content.Split(',', StringSplitOptions.RemoveEmptyEntries))
                {
                    var memberSpec = raw.Trim();
                    if (memberSpec.Length == 0) continue;
                    var asParts = memberSpec.Split([" AS "], StringSplitOptions.None);
                    var origName = asParts[0].Trim();
                    Token? memberAlias = null;
                    if (asParts.Length > 1 && !string.IsNullOrWhiteSpace(asParts[1]))
                        memberAlias = new Token(TokenKind.Identifier, asParts[1].Trim(), null, SourcePosition.Unknown);
                    members.Add(new ImportedMember(
                        new Token(TokenKind.Identifier, origName, null, SourcePosition.Unknown),
                        memberAlias));
                }

                import = new ImportStmt(nsToken, nsAlias, ImportMode.SpecificMembers, members.ToImmutableArray());
            }
            else
            {
                // Forma completa: IMPORT Ns [AS alias | WITH ALIAS]
                var parts = remainder.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length >= 2
                    && parts[0].Equals("WITH", StringComparison.OrdinalIgnoreCase)
                    && parts[1].Equals("ALIAS", StringComparison.OrdinalIgnoreCase))
                {
                    import = new ImportStmt(nsToken, nsToken, ImportMode.Namespace, []);
                }
                else if (parts.Length >= 2 && parts[0].Equals("AS", StringComparison.OrdinalIgnoreCase))
                {
                    var aliasToken = new Token(TokenKind.Identifier, parts[1], null, SourcePosition.Unknown);
                    import = new ImportStmt(nsToken, aliasToken, ImportMode.Namespace, []);
                }
                else
                {
                    import = new ImportStmt(nsToken, null, ImportMode.AllMembers, []);
                }
            }

            imports.Add(import);
        }

        return imports;
    }

    /// <summary>
    /// Esegue la scansione dei file sorgente per trovare tutti i file .bas e .inc nel progetto,
    /// includendo la directory radice e le cartelle/file di importazione specificati nelle impostazioni.
    /// </summary>
    private static HashSet<string> DiscoverSourceFiles(string projectRoot, EffectiveSettings settings)
        // Una sola implementazione (condivisa con i pipeline isolati BSP/EVAL): vedi
        // Pipeline/ModuleDiscovery.cs. Prima stava qui come metodo privato e i pipeline isolati
        // non potevano risolvere gli IMPORT delle librerie BASIC.
        => global::Basic.Pipeline.ModuleDiscovery.DiscoverSourceFiles(projectRoot, settings);

    /// <summary>
    /// Metodo helper centralizzato per il logging condizionale durante la compilazione.
    /// Scrive un messaggio sulla console solo se la modalità "verbose" è attiva globalmente
    /// o se la scrittura del messaggio viene forzata.
    /// </summary>
    /// <param name="message">Il messaggio di log da scrivere.</param>
    /// <param name="force">Se true, il messaggio viene scritto indipendentemente dall'impostazione
    /// globale 'Verbose'. Utile per messaggi di errore critici o per i dump della symbol table.</param>
    /// <remarks>
    /// La condizione `if (Parser.Verbose || force)` è commentata per abilitare temporaneamente
    /// il logging completo per scopi di debug. In produzione, andrebbe ripristinata per
    /// controllare la verbosità dell'output.
    /// </remarks>
    // ReSharper disable once UnusedParameter.Local
    private static void Log(string message, bool force = false)
    {
        // Assicuriamoci che il log sia sempre visibile se Verbose è true.
        if (Opt.VerboseOrchestrator || force) Console.WriteLine(message);
    }
}
