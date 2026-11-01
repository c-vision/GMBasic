// FILE: Basic.Pipeline.SourcePipeline.cs
// Pipeline condivisa Lexer→Parser→Linker→Compiler.

using System.Collections.Immutable;
using CommonLib.Http;

namespace Basic.Pipeline;

public static class SourcePipeline
{
    public static global::Compiler.CompiledChunk CompileFromString(string sourceText, string fileName,
        CommonLib.EffectiveSettings? settings = null)
        => CompileFromStringWithCompiler(sourceText, fileName, settings).Chunk;

    /// <summary>Come <see cref="CompileFromString"/>, ma espone anche il <see cref="global::Compiler.Compiler"/>
    /// usato — vedi CompileBspPage (GlobalSlots). Delega al core privato con StdLib.Instance.</summary>
    public static (global::Compiler.CompiledChunk Chunk, global::Compiler.Compiler Compiler) CompileFromStringWithCompiler(
        string sourceText, string fileName, CommonLib.EffectiveSettings? settings = null,
        string? rootDirectory = null)
    {
        var (compiler, program) = CompileFromStringCore(sourceText, fileName, settings,
            global::Basic.StdLib.Instance, DefaultNamespacesStandard, rootDirectory);
        return (compiler.Compile(program), compiler);
    }

    /// <summary>Come <see cref="CompileFromStringWithCompiler"/>, ma espone anche il
    /// <see cref="BasicProgram"/>: serve a chi deve conoscere i FILE caricati (dipendenze della
    /// cache BSP — la pagina + gli `.inc` importati).</summary>
    public static (global::Compiler.CompiledChunk Chunk, global::Compiler.Compiler Compiler, BasicProgram Program)
        CompileFromStringWithProgram(string sourceText, string fileName, CommonLib.EffectiveSettings? settings = null,
            string? rootDirectory = null)
    {
        var (compiler, program) = CompileFromStringCore(sourceText, fileName, settings,
            global::Basic.StdLib.Instance, DefaultNamespacesStandard, rootDirectory);
        return (compiler.Compile(program), compiler, program);
    }

    public static readonly string[] DefaultNamespacesStandard =
        ["SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME", "CONVERSIONS", "IO", "CONCURRENCY", "ARRAYS", "WINDOWS"];

    /// <summary>Seam di test + base del registry P0: registry e namespace di default ESPLICITI,
    /// materializzazione dei builtin nei Symbol DAL registry fornito, Program+Compiler esposti
    /// (il chunk si compila a parte: chi fa IL non deve eseguire il bytecode-compile).</summary>
    public static (global::Compiler.Compiler Compiler, BasicProgram Program) CompileFromStringWithRegistry(
        string sourceText, string fileName, global::Basic.IParserRegistry registry,
        IReadOnlyList<string>? defaultNamespaces = null, CommonLib.EffectiveSettings? settings = null,
        string? rootDirectory = null)
        => CompileFromStringCore(sourceText, fileName, settings, registry,
            defaultNamespaces ?? DefaultNamespacesStandard, rootDirectory);

    private static (global::Compiler.Compiler Compiler, BasicProgram Program) CompileFromStringCore(
        string sourceText, string fileName, CommonLib.EffectiveSettings? settings,
        global::Basic.IParserRegistry registry, IReadOnlyList<string> defaultNamespaces,
        string? rootDirectory = null)
    {
        // DefaultNamespaces = [] (invece della lista standard di Resources/system.settings.json:
        // SYSTEM/CONSOLE/GRAPHICS/MATH/STRINGS/DATETIME/CONVERSIONS/IO/CONCURRENCY/ARRAYS) rendeva
        // ogni builtin di quei namespace (LEN, VAL, SQR, ABS, STR$, ecc. — praticamente tutte le
        // funzioni "core" che un programma normale usa senza IMPORT esplicito) irriconoscibile dal
        // Parser durante una compilazione isolata (BSP, vedi CompileBspPage sotto): "SQR(9)" veniva
        // preso per un accesso ad array/identificatore sconosciuto (nodo ponte ArrayAccessExpr,
        // stesso meccanismo Fase 1 usato per "Nome(...) non ancora risolto"), risolto a runtime come
        // variabile NIL indicizzata → NULL silenzioso, mai un errore. Bug trovato dal vivo: una
        // pagina BSP con `STR$(VAL(SESSION("hits")) + 1)` produceva sempre output vuoto, non un
        // errore — isolato aggiungendo un dump del tipo di nodo AST visitato durante la compilazione
        // (VisitExpression), che ha mostrato ArrayAccessExpr al posto di BuiltinFunctionCallExpr per
        // ogni chiamata SQR/ABS/LEN/VAL in questo pipeline.
        // Impostazioni REALI (system.settings.json + app.settings.json del progetto): servono gli
        // ImportFolders / compiler.searchFolders, senza i quali il pipeline isolato non risolve
        // NESSUN `IMPORT` di una libreria BASIC (`include/*.inc`) — una pagina .bsp con
        // `IMPORT DASHBOARD` vedeva il namespace ma non le sue procedure. La soglia di default
        // namespaces resta quella del pipeline se il file non ne dichiara.
        var root = string.IsNullOrWhiteSpace(rootDirectory)
            ? Directory.GetCurrentDirectory()
            : Path.GetFullPath(rootDirectory);
        // `fileName` può essere un PERCORSO ASSOLUTO (runner/harness passano il file così):
        // Path.Combine(root, abs) restituisce abs, e la discovery deve partire dalla cartella
        // della ENTRY, non dalla CWD — altrimenti con CWD = repo si scandisce TUTTO il monorepo
        // e si caricano .inc omonimi di altre cartelle (bug: due Mezzi.inc → simbolo duplicato).
        var entryPath = Path.IsPathRooted(fileName)
            ? Path.GetFullPath(fileName)
            : Path.Combine(root, fileName);
        var discoveryRoot = Path.GetDirectoryName(entryPath) ?? root;

        settings ??= CommonLib.SettingsLoader.Load(entryPath);
        if (settings.DefaultNamespaces.Count == 0)
            settings = settings with { DefaultNamespaces = defaultNamespaces.ToList() };

        var program = new BasicProgram(settings, null, discoveryRoot);
        var preprocessed = TokenizeFile(entryPath, sourceText, program);
        program.SourceMap = preprocessed.SourceMap;
        program.PreprocessedFiles[entryPath] = preprocessed;

        var imports = ScanImports(sourceText, entryPath);
        program.ImportsByFile[entryPath] = imports;

        // Grafo degli IMPORT: aggiunge al programma i moduli delle librerie BASIC raggiungibili
        // (stessa discovery dell'Orchestrator — Pipeline/ModuleDiscovery.cs).
        ExpandImportedModules(entryPath, discoveryRoot, settings, program);

        program.FileIdToPathMap = program.GetFileIdMap().ToImmutableDictionary(kvp => kvp.Value, kvp => kvp.Key);

        CreateBuiltinNamespaces(program, registry);

        RegisterBspGlobal(program, "REQ", new HttpRequestType());
        RegisterBspGlobal(program, "RES", new HttpResponseType());
        RegisterBspGlobal(program, "SESSION", new JsonType());
        RegisterBspGlobal(program, "FORM", new JsonType());
        RegisterBspGlobal(program, "QUERY", new JsonType());

        foreach (var fp in program.PreprocessedFiles.Keys.ToList())
        {
            if (!program.PreprocessedFiles.TryGetValue(fp, out var pf)) continue;
            program.Symbols.ResetScope();
            var fi = program.ImportsByFile.GetValueOrDefault(fp) ?? [];
            var p = new global::Basic.Parser(pf.Tokens, pf.SourceMap, program, fp, registry);
            program.LoadedModules[fp] = p.ParseModuleSource(fi);
        }

        global::Basic.Linker.PopulateSymbolTable(program);
        global::Basic.Linker.ResolveSymbolIds(program);
        global::Basic.Linker.PerformStaticAnalysisAndValidation(program);
        global::Basic.Linker.OptimizeAsts(program);

        // Allineamento col percorso canonico (Orchestrator.Compile): le fasi 3.3-3.5 del linker
        // risolvono cio' che 1-3.2 lasciano in sospeso — costanti builtin (%CHCLOSED -> 0),
        // chiamate builtin qualificate (SQR/ABS/VAL -> BuiltinFunctionCallExpr) e campi di dict
        // (result.Centroids -> FieldSymbolId). SENZA di esse, la PCodeVM usata da BSP/EVAL/macro
        // compilava questi riferimenti come variabile NIL (PUSH_NIL) o ArrayAccessExpr, producendo
        // output vuoti o loop infiniti (bug %CHCLOSED nel pattern RECEIVE...OK + CLOSE). E' lo
        // stesso identico percorso di Orchestrator; non deve esistere una semantica separata.
        global::Basic.Linker.ResolveAndValidateClassInheritance(program);
        global::Basic.Linker.ResolveAndRewriteModuleAsts(program);
        global::Basic.Linker.RebuildBlockScopedLabelReferences(program);
        global::Basic.Linker.ValidateTypes(program);
        global::Basic.Linker.ApplySemanticPatches(program);   /* §4.10 write-side (post-link) */

        if (program.LoadedModules.TryGetValue(entryPath, out var mainMod))
            program.MainModule = mainMod;

        // Il bytecode-compile avviene nei wrapper (chi ha bisogno del Program per IL lo usa
        // senza eseguire il compile chunk).
        return (new global::Compiler.Compiler(), program);
    }

    /// <summary>
    /// Garantisce il sidecar A1 `&lt;pagina&gt;.bsp.chunk` (lo esegue il renderer BSP in C,
    /// nativo/gmb_vm_c): lo rigenera se manca o se la pagina / un `.inc` importato è più
    /// recente. Unica implementazione: la usano sia la precompilazione `-bsp` del REPL sia il
    /// server quando serve una pagina con il motore C (`HTTPCONFIG.BspEnginePages`).
    /// </summary>
    /// <summary>Serializzatore del sidecar A1 (chunk completo → byte). Lo imposta l'host
    /// (REPL / dispatcher PCode / dispatcher IL): il serializzatore vive in BRT, che Pipeline
    /// non può referenziare (dipendenza circolare). Se è null il server resta sul motore managed.</summary>
    public static Func<global::Compiler.CompiledChunk, byte[]>? SidecarWriter { get; set; }

    public static bool EnsureBspSidecar(string bspFilePath, bool force = false)
    {
        var sidecar = bspFilePath + ".chunk";
        try
        {
            if (!force && File.Exists(sidecar))
            {
                var cached = BspRuntime.CacheGet(bspFilePath);
                if (cached != null)
                {
                    var sc = File.GetLastWriteTimeUtc(sidecar);
                    var fresh = true;
                    foreach (var dep in cached.Dependencies ?? [bspFilePath])
                        if (File.Exists(dep) && File.GetLastWriteTimeUtc(dep) > sc) { fresh = false; break; }
                    if (fresh) return true;
                }
            }
            if (!CompileBspPage(bspFilePath, out var fullChunk) || fullChunk is null) return false;
            var writer = SidecarWriter;
            if (writer is null) return false;   // nessun host che sappia serializzare A1
            File.WriteAllBytes(sidecar, writer(fullChunk));
            return true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[BSP] sidecar failed for {bspFilePath}: {ex.Message}");
            return false;
        }
    }

    public static bool CompileBspPage(string bspFilePath) => CompileBspPage(bspFilePath, out _);

    /// <summary>
    /// Come <see cref="CompileBspPage(string)"/>, ma espone anche il CompiledChunk completo —
    /// serve al REPL per scrivere il sidecar A1 (nome.chunk) eseguito dalla VM/C per pagina.
    /// </summary>
    public static bool CompileBspPage(string bspFilePath, out global::Compiler.CompiledChunk? chunk)
    {
        chunk = null;
        try
        {
            if (!File.Exists(bspFilePath)) return false;
            var bspSource = File.ReadAllText(bspFilePath);
            var basicCode = BspCompiler.Compile(bspSource, bspFilePath);
            // ROOT = cartella della pagina (non la CWD): le impostazioni del progetto (app.settings)
            // e la discovery degli `include/` devono essere quelli della pagina, chiunque la serva.
            var (compiledChunk, compiler, bspProgram) = CompileFromStringWithProgram(
                basicCode, Path.GetFileName(bspFilePath),
                rootDirectory: Path.GetDirectoryName(Path.GetFullPath(bspFilePath)));
            chunk = compiledChunk;
            // Slot per NOME (REQ/RES/SESSION/FORM/QUERY/PAGE) invece di un ordine assunto: vedi
            // CompileFromStringWithCompiler per il perché (l'host HTTP deve seminare questi
            // globali prima di eseguire, senza dipendere da un numero di slot fisso).
            var globalNameSlots = compiler.GlobalSlots.ToDictionary(kv => kv.Key, kv => kv.Value);
            // EntryPoint (non solo Code/Constants/Lines/GlobalCount): una pagina .bsp ha SEMPRE
            // almeno un DIM globale con inizializzatore (REQ/RES/SESSION/PAGE seminati dall'host,
            // e le DIM della pagina stessa come `opts`/`r`/`pkgs`), quindi il Compiler fa partire
            // l'esecuzione da un PROLOGO che chiama __global_init PRIMA di __main — non da 0.
            // Senza salvare EntryPoint qui, il chunk ricostruito dalla cache (BspDispatch/
            // TryServeBspPage) partiva sempre da 0, che spesso coincide col corpo di
            // __global_init stesso: il task lo eseguiva DIRETTAMENTE (mai via OP_CALL dal
            // prologo) e al RETURN di chiusura (task.Frame ancora null) finiva subito, senza
            // mai raggiungere __main — bug: qualunque pagina BSP restava vuota.
            // Dipendenze = la pagina + TUTTI i file caricati (gli `.inc` importati): la cache
            // si invalida se uno qualsiasi cambia (hot reload degli helper senza riavviare).
            var __deps = bspProgram.PreprocessedFiles.Keys.ToArray();
            BspRuntime.CacheStore(bspFilePath, [.. compiledChunk.Code!], [.. compiledChunk.Constants!], [.. compiledChunk.Lines!],
                compiledChunk.GlobalCount, globalNameSlots, compiledChunk.EntryPoint, __deps);
            return true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[BSP] Compilation failed for {bspFilePath}: {ex.Message}");
            return false;
        }
    }

    private static BasicProgram.PreprocessedFile TokenizeFile(string path, string source, BasicProgram program)
    {
        if (program.PreprocessedFiles.TryGetValue(path, out var cached))
            return cached;
        var fileId = program.GetOrAddFileId(path);
        var lines = source.Split(["\r\n", "\r", "\n"], StringSplitOptions.None);
        var sm = ImmutableArray.CreateBuilder<SourceLine>();
        for (var i = 0; i < lines.Length; i++)
            sm.Add(new SourceLine(fileId, path, i + 1, lines[i]));
        var sourceMap = sm.ToImmutable();
        var lexer = new global::Basic.Lexer(sourceMap, program, false);
        var tokens = lexer.ScanTokens();
        return new BasicProgram.PreprocessedFile(tokens.ToImmutableArray(), sourceMap);
    }

    private static List<ImportStmt> ScanImports(string sourceText, string path)
    {
        var imports = new List<ImportStmt>();
        using var reader = new StringReader(sourceText);
        string? line;
        while ((line = reader.ReadLine()) != null)
        {
            var trimmed = line.Trim();
            if (!trimmed.StartsWith("IMPORT ", StringComparison.OrdinalIgnoreCase)) continue;
            var rest = trimmed["IMPORT ".Length..].Trim();
            if (string.IsNullOrWhiteSpace(rest)) continue;

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

            // Forma selettiva: IMPORT Ns (a, b, c) [AS alias].
            if (remainder.StartsWith('('))
            {
                var closeIndex = remainder.IndexOf(')');
                if (closeIndex < 0)
                {
                    imports.Add(new ImportStmt(nsToken, null, ImportMode.AllMembers, []));
                    continue;
                }

                var content = remainder.Substring(1, closeIndex - 1).Trim();

                // Dopo la chiusura può esserci "AS alias" (alias del namespace selettivo).
                var afterClose = remainder[(closeIndex + 1)..].Trim();
                Token? nsAlias = null;
                if (afterClose.StartsWith(" AS ", StringComparison.OrdinalIgnoreCase))
                    nsAlias = new Token(TokenKind.Identifier, afterClose[4..].Trim(), null, SourcePosition.Unknown);

                var members = new List<ImportedMember>();
                foreach (var raw in content.Split(',', StringSplitOptions.RemoveEmptyEntries))
                {
                    var memberSpec = raw.Trim();
                    if (memberSpec.Length == 0) continue;

                    // Ogni membro può essere "nome" oppure "nome AS alias".
                    var asParts = memberSpec.Split([" AS "], StringSplitOptions.None);
                    var origName = asParts[0].Trim();
                    Token? memberAlias = null;
                    if (asParts.Length > 1 && !string.IsNullOrWhiteSpace(asParts[1]))
                        memberAlias = new Token(TokenKind.Identifier, asParts[1].Trim(), null, SourcePosition.Unknown);

                    members.Add(new ImportedMember(
                        new Token(TokenKind.Identifier, origName, null, SourcePosition.Unknown),
                        memberAlias));
                }

                imports.Add(new ImportStmt(nsToken, nsAlias, ImportMode.SpecificMembers, members.ToImmutableArray()));
                continue;
            }

            // Forma con alias: IMPORT Ns AS Alias
            var aliasParts = remainder.Split([" AS "], StringSplitOptions.None);
            if (aliasParts.Length > 1 && !string.IsNullOrWhiteSpace(aliasParts[1]))
            {
                var aliasToken = new Token(TokenKind.Identifier, aliasParts[1].Trim(), null, SourcePosition.Unknown);
                imports.Add(new ImportStmt(nsToken, aliasToken, ImportMode.Namespace, []));
                continue;
            }

            // Forma completa (default): IMPORT Ns
            imports.Add(new ImportStmt(nsToken, null, ImportMode.AllMembers, []));
        }
        return imports;
    }

    /// <summary>
    /// Espande il grafo degli `IMPORT` a partire dalla entry: per ogni namespace importato cerca
    /// il/i file che lo dichiarano (`NAMESPACE X`) fra i candidati della discovery condivisa
    /// (<see cref="ModuleDiscovery"/>) e li tokenizza/registra nel programma, ripetendo sui loro
    /// import. Senza questo passo un pipeline ISOLATO (pagina BSP, EVAL) vedeva il namespace ma
    /// non le sue procedure: `IMPORT DASHBOARD` → "Nessuna funzione pubblica ... ".
    /// Stessa semantica dell'Orchestrator (che però parte dal file su disco, non da una stringa).
    /// </summary>
    private static void ExpandImportedModules(string entryPath, string root,
        CommonLib.EffectiveSettings settings, BasicProgram program)
    {
        Dictionary<string, List<string>> nsMap;
        try
        {
            nsMap = ModuleDiscovery.BuildNamespaceMap(ModuleDiscovery.DiscoverSourceFiles(root, settings));
        }
        catch
        {
            // Discovery non disponibile (permessi/cartelle): si resta sulla sola entry, come prima.
            return;
        }
        if (nsMap.Count == 0) return;

        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { entryPath };
        var queue = new Queue<string>();

        void EnqueueImportsOf(string file)
        {
            foreach (var imp in program.ImportsByFile.GetValueOrDefault(file) ?? [])
            {
                var ns = imp.NamespaceName.Lexeme;
                if (!nsMap.TryGetValue(ns, out var files)) continue;
                foreach (var dep in files)
                    if (!seen.Contains(dep)) queue.Enqueue(dep);
            }
        }

        EnqueueImportsOf(entryPath);
        while (queue.Count > 0)
        {
            var file = queue.Dequeue();
            if (!seen.Add(file)) continue;
            if (program.PreprocessedFiles.ContainsKey(file)) { EnqueueImportsOf(file); continue; }

            string text;
            try { text = File.ReadAllText(file); }
            catch { continue; }

            program.PreprocessedFiles[file] = TokenizeFile(file, text, program);
            program.ImportsByFile[file] = ScanImports(text, file);
            EnqueueImportsOf(file);
        }
    }

    private static void CreateBuiltinNamespaces(BasicProgram program, global::Basic.IParserRegistry procRegistry)
    {
        var std = global::Basic.StdLib.Instance;
        // "CONSTANTS" ospita le costanti PowerBASIC-style (%RED, %ESC, %JOYUP, %FILE_CREATED...)
        // sempre disponibili senza IMPORT — stesso trattamento già riservato a SYSTEM/WINDOW.
        // "TRAIN" (namespace PUL per la produzione/alterazione dei pesi) è sempre disponibile
        // come "CONSTANTS": il suo dispatcher (F0) delega ai sottoparser AI/LLM esistenti,
        // quindi non ha simboli propri ma deve superare il gate dei namespace in ParseStatement.
        var nsSet = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "SYSTEM", "WINDOW", "CONSTANTS", "TRAIN", "GMB" };
        foreach (var nsName in program.Settings.DefaultNamespaces)
            nsSet.Add(nsName);
        foreach (var (_, imports) in program.ImportsByFile)
            foreach (var nsName in imports.Select(i => i.NamespaceName.Lexeme))
                nsSet.Add(nsName);

        program.Namespaces = nsSet;

        foreach (var nsName in nsSet)
        {
            // Procedure dal registry FORNITO (seam/test/P0); tipi/globali/costanti da StdLib.
            var hasProc = procRegistry.BuiltinProcedures.ContainsKey(nsName);
            var hasType = std.BuiltinTypes.ContainsKey(nsName);
            var hasGlob = std.BuiltinGlobals.ContainsKey(nsName);
            var hasConst = std.BuiltinConstants.ContainsKey(nsName);
            if (!hasProc && !hasType && !hasGlob && !hasConst) continue;

            program.Symbols.EnterScope(nsName, SymbolTable.ScopeType.Namespace);
            try
            {
                if (hasProc)
                {
                    foreach (var pt in procRegistry.BuiltinProcedures[nsName])
                    {
                        var rt = pt switch
                        {
                            FunctionDefinition fd => fd.ReturnType,
                            GeneratorDefinition gd => new GeneratorType(gd.YieldType),
                            NativeProcedureDefinition npd => npd.ReturnType,
                            ExternalProcedureDefinition epd => epd.ReturnType ?? new VoidType(),
                            _ => new VoidType()
                        };
                        var sid = program.Symbols.RegisterSymbol(pt.OriginalName, pt.Flags, rt, pt);
                        ProcedureDefinition cp = pt switch
                        {
                            NativeProcedureDefinition npd => npd with { SymbolId = sid },
                            FunctionDefinition fd => fd with { SymbolId = sid },
                            GeneratorDefinition gd => gd with { SymbolId = sid },
                            ExternalProcedureDefinition epd => epd with { SymbolId = sid },
                            _ => throw new InvalidOperationException("bad builtin")
                        };
                        program.Symbols.UpdateSymbolDeclaration(sid, cp);
                    }
                }

                if (hasType)
                {
                    foreach (var (tn, tt) in std.BuiltinTypes[nsName])
                    {
                        const SymbolFlags tf = SymbolFlags.IsPublic | SymbolFlags.IsTypeAlias;
                        var tk = new Token(TokenKind.Identifier, tn, null, SourcePosition.Unknown);
                        if (tt is UserDefinedType udt)
                        {
                            var tsid = program.Symbols.RegisterSymbol(tk, tf, udt, udt);
                            program.Symbols.EnterScope(tn, SymbolTable.ScopeType.Type, tsid);
                            try
                            {
                                var usid = program.Symbols.GetCurrentScope().Id;
                                var uf = udt.Fields.Select(f =>
                                {
                                    const SymbolFlags ff = SymbolFlags.IsField | SymbolFlags.IsPublic;
                                    var fid = program.Symbols.RegisterSymbol(f.Name, ff, f.DataType, f);
                                    return f with { SymbolId = fid, ScopeId = usid };
                                }).ToImmutableArray();
                                program.Symbols.UpdateSymbolDeclaration(tsid, udt with { SymbolId = tsid, Fields = uf });
                            }
                            finally { program.Symbols.ExitScope(); }
                        }
                        else if (tt is ClassType ct)
                        {
                            // Base classi builtin (es. CONTROLS.Control): il nodo di dichiarazione è
                            // la ClassDefinition, così `INHERITS <base>` la trova (FindSymbolsByName).
                            program.Symbols.RegisterSymbol(tk, tf, ct, ct.Definition);
                        }
                        else
                        {
                            program.Symbols.RegisterSymbol(tk, tf, tt, tt);
                        }
                    }
                }

                if (hasGlob)
                {
                    var isSys = nsName.Equals("SYSTEM", StringComparison.OrdinalIgnoreCase);
                    if (isSys) program.Symbols.ExitScope();
                    foreach (var (gn, gt, gf) in std.BuiltinGlobals[nsName])
                    {
                        var gk = new Token(TokenKind.Identifier, gn, null, SourcePosition.Unknown);
                        var gd = new VariableDeclaration(-1, gk, gk, ImmutableArray<Expr>.Empty, gt, null, false, true, gf);
                        var gsid = program.Symbols.RegisterSymbol(gk, gf, gt, gd);
                        program.Symbols.UpdateSymbolDeclaration(gsid, gd with { SymbolId = gsid });
                    }
                    if (isSys) program.Symbols.EnterScope(nsName, SymbolTable.ScopeType.Namespace);
                }

                if (hasConst)
                {
                    foreach (var ct in std.BuiltinConstants[nsName])
                    {
                        var csid = program.Symbols.RegisterSymbol(ct.OriginalToken, ct.Flags, ct.DataType ?? new InferredType(), ct);
                        program.Symbols.UpdateSymbolDeclaration(csid, ct with { SymbolId = csid });
                    }
                }
            }
            finally { program.Symbols.ExitScope(); }
        }
    }

    private static void RegisterBspGlobal(BasicProgram program, string name, DataType type)
    {
        var token = new Token(TokenKind.Identifier, name, null, SourcePosition.Unknown);
        var decl = new VariableDeclaration(-1, token, token,
            ImmutableArray<Expr>.Empty, type, null, false, true,
            SymbolFlags.IsPublic | SymbolFlags.IsGlobal);
        program.Symbols.RegisterSymbol(token, decl.Flags, type, decl);
    }
}
