// FILE: Basic.IlBackend.PCode.BspDispatch.cs
// Collega le pagine BSP (.bsp, Basic Server Pages) al server HTTP reale
// (Basic.Runtime.Services.HttpService) tramite il suo hook di iniezione
// GlobalBspPageHandler. BRT non può referenziare IlBackend (la dipendenza va nella direzione
// opposta: IlBackend -> BRT), quindi il collegamento va fatto da qui, non da BRT.
//
// Prima di questo file: TryServeBspPage esisteva solo nel nome, in un commento di ReplEngine.cs
// (vedi report Obsidian Dynamic-Code-EVAL-Macro-Backends). Nessun codice, su nessun backend,
// serviva mai una pagina .bsp in risposta a una richiesta HTTP reale — solo compilazione manuale
// via REPL/CLI (comando BSP <dir>) che popolava la cache senza che nulla la leggesse mai.

using System.Runtime.CompilerServices;
using Basic.Runtime;

namespace Basic.IlBackend.PCode;

internal static class BspDispatch
{
    /// <summary>
    /// Una sola sessione di processo per pagine E route (CommonLib.Http.SessionStores.Default):
    /// condivisa tra BSP e REQ("Session"), sostituibile con backend persistente da HTTP CONFIG.
    /// </summary>

    /// <summary>
    /// Compila il BASIC BSP in un assembly IL caricabile in-process (assembly + BasicProgram).
    /// Iniettato dall'host C# (Executor/REPL): IlBackendRuntime non può referenziare Executor
    /// (dove vive Orchestrator.CompileFromString) per layering, quindi il path ILBackend del BSP
    /// riceve il compilatore tramite questo hook. Se null, <c>GMB_BSP_BACKEND=il</c> risponde 501.
    /// </summary>
    public delegate (System.Reflection.Assembly Assembly, Basic.BasicProgram Program) IlBspCompilerDelegate(
        string basicCode, string fileName);

    private static IlBspCompilerDelegate? _ilBspCompiler;

    /// <summary>Registra (o rimuove, con null) il compilatore BSP→IL fornito dall'host C#.</summary>
    public static void SetIlBspCompiler(IlBspCompilerDelegate? compiler) => _ilBspCompiler = compiler;

    /// <summary>
    /// Eseguito automaticamente al caricamento di questo assembly (prima che qualunque codice
    /// BASIC possa chiamare HTTP.Listen/HTTP.Route) — garantisce che l'hook sia impostato senza
    /// bisogno di un punto di bootstrap esplicito da trovare/modificare in IlRuntime o altrove.
    /// </summary>
#pragma warning disable CA2255 // ModuleInitializer intenzionale: hook impostato all'load dell'assembly
    [ModuleInitializer]
    internal static void Register()
    {
        HttpService.GlobalBspPageHandler = ServeBspPage;
    }

    /// <summary>
    /// Compila (se non già in cache, con hot-reload su mtime — vedi BspRuntime.CacheGet) ed
    /// esegue una pagina .bsp con l'interprete P-Code interno (porting di PCodeVm, mai un bridge
    /// verso C). Semina REQ/RES/SESSION/FORM/QUERY per NOME, leggendo gli slot da
    /// <see cref="CommonLib.Http.BspRuntime.CachedPage.GlobalNameSlots"/> (mai un numero di slot
    /// fisso assunto — vedi SourcePipeline.CompileFromStringWithCompiler per il perché).
    /// </summary>
    /// <summary>
    /// Rifinisce una risposta BSP come HTML: Content-Type (se la pagina non l'ha già impostato),
    /// ETag debole, 304 su If-None-Match, gzip se il client lo accetta e il corpo è &gt; 1 KB.
    /// Stessa semantica del percorso P-Code (PCodeProcessor.SendBspBody), unica implementazione
    /// per entrambe le gambe lato IL.
    /// </summary>
    private static HttpResponse FinishBsp(HttpResponse result, HttpRequest req)
    {
        var body = result.BodyBytes ?? System.Text.Encoding.UTF8.GetBytes(result.Body ?? "");
        if (string.IsNullOrEmpty(result.ContentType)) result.ContentType = "text/html; charset=utf-8";

        var etag = CommonLib.Http.BspRuntime.ComputeETag(body);
        result.Headers["ETag"] = etag;
        if (!result.Headers.ContainsKey("Cache-Control"))
            result.Headers["Cache-Control"] = CommonLib.Http.BspRuntime.RenderCacheTtlMs > 0
                ? "public, max-age=" + (CommonLib.Http.BspRuntime.RenderCacheTtlMs / 1000)
                : "no-cache";

        // BspDispatch lavora sul HttpRequest del BRT: Headers è Dictionary<string,string>
        // (il percorso P-Code usa CommonLib.Http.HttpRequest con JsonObject → helper separato).
        string? HeaderOf(string name) =>
            req.Headers.TryGetValue(name, out var value) ? value : null;

        if (CommonLib.Http.BspRuntime.ETagMatches(HeaderOf("If-None-Match"), etag))
        {
            result.StatusCode = 304;
            result.Body = "";
            result.BodyBytes = [];
            return result;
        }

        // br/gzip negoziati, come le altre gambe (prima SOLO gzip: un client che offriva `br`
        // riceveva gzip, e la gamba IL restava l'unica a non conoscere brotli).
        var enc = CommonLib.Http.HttpCompression.PickEncoding(HeaderOf("Accept-Encoding"));
        if (body.Length >= 1024 && enc != null)
        {
            var packed = CommonLib.Http.HttpCompression.Encode(enc, body);
            if (packed.Length < body.Length)
            {
                result.BodyBytes = packed;
                result.Headers["Content-Encoding"] = enc;
                result.Headers["Vary"] = "Accept-Encoding";
            }
            else
            {
                result.BodyBytes = body;
                result.Headers["Vary"] = "Accept-Encoding";
            }
        }
        else
        {
            result.BodyBytes = body;
        }
        return result;
    }

    private static HttpResponse? ServeBspPage(string filePath, HttpRequest req)
    {
        if (!File.Exists(filePath)) return null;
        try
        {
            req.Headers.TryGetValue("Cookie", out var cookieHeader);
            var (_, setCookie, sessionData) = CommonLib.Http.SessionStores.Default.GetOrCreate(cookieHeader);

            // Backend selezionato per la pagina (env GMB_BSP_BACKEND): il BSP pompa lo
            // scheduler del backend scelto — default P-Code (vm.Run, multi-worker), "il" per
            // il backend IL (entry generata; FETCH sincrono o driver v2 se may-park).
            var backend = Basic.Providers.GmbEnv.Get("GMB_BSP_BACKEND");
            if (backend != null && backend.Equals("il", StringComparison.OrdinalIgnoreCase))
                return ServeBspPageIlBackend(filePath, req, setCookie, sessionData);

            // Come nel percorso P-Code: niente resa memoizzata per richieste con cookie
            // (contenuto potenzialmente per-utente).
            var canCache = !req.Headers.ContainsKey("Cookie");
            var renderKey = filePath + "?" + (req.QueryString ?? "");
            var rendered = canCache ? CommonLib.Http.BspRuntime.RenderGet(renderKey) : null;
            if (rendered != null)
                return FinishBsp(new HttpResponse { BodyBytes = rendered }, req);

            // Motore C (opt-in): stessa semantica del percorso P-Code.
            if (CommonLib.Http.BspRuntime.UseCEngine(filePath))
            {
                Basic.Pipeline.SourcePipeline.SidecarWriter ??=
                    static chunk => Basic.Runtime.ChunkA1Serializer.Serialize(chunk);
                Basic.Pipeline.SourcePipeline.EnsureBspSidecar(filePath);
                BspStateProvider.EnsureRegistered();

                req.Headers.TryGetValue("Host", out var cHost);
                if (CommonLib.Http.BspC.TryServe(Path.GetDirectoryName(filePath) ?? "", "/" + Path.GetFileName(filePath),
                        req.QueryString, cHost, out var cBody, out var cStatus))
                    return FinishBsp(new HttpResponse
                    {
                        StatusCode = cStatus == 0 ? 200 : cStatus,
                        BodyBytes = System.Text.Encoding.UTF8.GetBytes(cBody),
                        ContentType = cStatus == 200 ? "text/html; charset=utf-8" : "text/plain; charset=utf-8"
                    }, req);
            }

            var cached = CommonLib.Http.BspRuntime.CacheGet(filePath);
            if (cached == null)
            {
                Basic.Pipeline.SourcePipeline.CompileBspPage(filePath);
                cached = CommonLib.Http.BspRuntime.CacheGet(filePath);
            }
            if (cached == null)
                return new HttpResponse { StatusCode = 503, Body = "503 BSP compilation failed.", ContentType = "text/plain" };

            var chunk = new global::Compiler.CompiledChunk();
            foreach (var b in cached.PCode) chunk.Code.Add(b);
            foreach (var c in cached.Constants) chunk.Constants.Add(c);
            foreach (var l in cached.Lines) chunk.Lines.Add(l);
            chunk.GlobalCount = cached.GlobalCount;
            // Una pagina .bsp ha SEMPRE almeno un DIM globale con inizializzatore (REQ/RES/
            // SESSION/PAGE + le DIM della pagina), quindi il chunk parte da un PROLOGO
            // (__global_init poi __main), non da 0 — senza copiare EntryPoint qui il task
            // eseguiva __global_init DIRETTAMENTE (mai via OP_CALL) e al suo RETURN di
            // chiusura (Frame ancora null) finiva subito, senza mai raggiungere __main:
            // pagina resa vuota (200, body vuoto).
            chunk.EntryPoint = cached.EntryPoint;

            using var outputWriter = new StringWriter();
            var runtime = new HeadlessRuntimeHost(outputWriter, null, new NullGraphicsService());
            var pcodeResponse = new Http.PCodeHttpResponse();

            using var vm = new PCodeVm(chunk, runtime);
            var savedPCodeOut = System.Console.Out;
            int? sessionSlot = null;
            try
            {
                // Come il path IL: i PRINT della pagina passano anche da Console.Out (il
                // PCodeVm/PrintDispatch possono usarlo in alcuni percorsi) — senza il redirect
                // il body BSP restava vuoto (200 con len 0) pur senza eccezioni.
                System.Console.SetOut(outputWriter);
                var slots = cached.GlobalNameSlots;
                if (slots != null)
                {
                    void Seed(string name, object? value)
                    {
                        if (slots.TryGetValue(name, out var slot)) vm.SetGlobal(slot, value);
                    }
                    Seed("REQ", ConvertRequest(req));
                    Seed("RES", pcodeResponse);
                    if (slots.TryGetValue("SESSION", out var ss)) sessionSlot = ss;
                    Seed("SESSION", JsonObjectToDict(sessionData));
                }
                // FORM/QUERY sono keyword (non dichiarabili con DIM): il Linker le marca con
                // SymbolId = DynamicScopeSymbolId, quindi NON compaiono mai in GlobalNameSlots —
                // Seed("FORM"/"QUERY", ...) sopra era sempre un no-op silenzioso (slots.TryGetValue
                // falliva sempre per questi due nomi). Bug reale, stessa classe di
                // PCodeProcessor.Helpers.TryServeBspPage: fix identico, dynamic scope per nome
                // via OP_LOAD_DYNAMIC_SCOPE (vedi PCodeProcessor.SetDynamicScope).
                vm.SetDynamicScope("FORM", ParseFormToDict(req));
                vm.SetDynamicScope("QUERY", req.Query.ToDictionary(kv => kv.Key, kv => (object?)kv.Value));

                vm.Run();
            }
            finally { System.Console.SetOut(savedPCodeOut); }

            // Riscrive nella sessione VIVA (stesso oggetto tenuto da SessionManager, vedi
            // BspSessionManager.GetSession) le eventuali modifiche fatte dalla pagina — senza
            // questo, SESSION("x") = "y" funzionerebbe solo nell'ambito della singola richiesta:
            // il dizionario seminato in Seed("SESSION", ...) e' una COPIA, non lo stesso oggetto.
            if (sessionSlot.HasValue && vm.GetGlobal(sessionSlot.Value) is Dictionary<string, object?> mutatedSession)
            {
                sessionData.Clear();
                foreach (var kv in mutatedSession)
                    sessionData[kv.Key] = kv.Value?.ToString();
            }

            HttpResponse result;
            if (vm.ExecutionException != null)
            {
                result = new HttpResponse
                {
                    StatusCode = 500, ContentType = "text/plain",
                    Body = $"500 BSP Error: {vm.ExecutionException.Message}",
                };
            }
            else if (pcodeResponse.Sent)
            {
                result = new HttpResponse
                {
                    StatusCode = pcodeResponse.StatusCode,
                    Body = System.Text.Encoding.UTF8.GetString(pcodeResponse.Body),
                    ContentType = pcodeResponse.Headers.TryGetValue("Content-Type", out var ct) ? ct : "text/html; charset=utf-8",
                    Headers = pcodeResponse.Headers,
                };
            }
            else
            {
                // Nessun RES.Send*/Redirect esplicito nella pagina: l'output e' quello scritto
                // dai PRINT generati da BspCompiler dall'HTML/`<%= expr %>` — semantica
                // ASP-classic, la pagina "e'" l'output.
                var body = System.Text.Encoding.UTF8.GetBytes(outputWriter.ToString());
                if (canCache && !pcodeResponse.Headers.ContainsKey("Set-Cookie"))
                    CommonLib.Http.BspRuntime.RenderPut(renderKey, body);
                result = new HttpResponse { BodyBytes = body, ContentType = "text/html; charset=utf-8" };
            }

            // setCookie e' non-null SOLO alla creazione di una sessione nuova (nessun cookie
            // GMBSESSION valido in arrivo) — una sessione già esistente non ha bisogno di
            // reinviare lo stesso cookie a ogni richiesta.
            if (setCookie != null)
                result.Headers["Set-Cookie"] = setCookie;
            return FinishBsp(result, req);
        }
        catch (Exception ex)
        {
            return new HttpResponse { StatusCode = 500, Body = $"500 BSP Error: {ex.Message}", ContentType = "text/plain" };
        }
    }

    /// <summary>
    /// Esegue una pagina .bsp sul BACKEND IL (path GMB_BSP_BACKEND=il): il BASIC BSP viene
    /// compilato in IL (via il compilatore iniettato dall'host C# — Orchestrator, che
    /// IlBackendRuntime non può referenziare) e la entry generata viene invocata. Semina
    /// REQ/RES/SESSION/FORM/QUERY per NOME scandendo la SymbolTable del programma compilato.
    /// La entry generata è lineare per i programmi top-level (FETCH sincrono) e dispatcia
    /// automaticamente al driver GPM v2 (IlGpmRuntime.RunV2Entry) se il Main fosse may-park:
    /// in entrambi i casi la pagina "pompa" lo scheduler del backend IL scelto.
    /// </summary>
    private static HttpResponse? ServeBspPageIlBackend(string filePath, HttpRequest req,
        string? setCookie, System.Text.Json.Nodes.JsonObject sessionData)
    {
        var compiler = _ilBspCompiler;
        if (compiler == null)
            return new HttpResponse
            {
                StatusCode = 501, ContentType = "text/plain",
                Body = "501 GMB_BSP_BACKEND=il richiede l'host IL (Executor/REPL) che registra il compilatore BSP→IL.",
            };

        // Resa memoizzata (opt-in) anche su questo percorso: evita la ricompilazione+esecuzione
        // della pagina identica per ogni client in polling.
        var renderKey = filePath + "?" + (req.QueryString ?? "");
        var rendered = CommonLib.Http.BspRuntime.RenderGet(renderKey);
        if (rendered != null)
            return FinishBsp(new HttpResponse { BodyBytes = rendered }, req);

        var bspSource = File.ReadAllText(filePath);
        var basicCode = CommonLib.Http.BspCompiler.Compile(bspSource, filePath);
        var (assembly, program) = compiler(basicCode, Path.GetFileName(filePath));

        using var outputWriter = new StringWriter();
        // Isolamento dallo stato del programma ospite: la pagina ha una SymbolTable propria con
        // symbolId che si sovrappongono a quelli del server. BeginGlobalScope instrada i
        // LoadGlobal/StoreGlobal del THREAD della pagina verso un dict privato (e non la bag di
        // processo condivisa): il seed REQ/RES non sovrascrive `pkgs` del server e un FETCH della
        // pagina verso la STESSA API (es. index.bsp → /api/packages) trova il `pkgs` dell'host
        // ancora vivo negli handler (thread separati, scope nullo → ProgramGlobals).
        var savedConsoleOut = System.Console.Out;
        try
        {
            IlRuntime.BeginGlobalScope(new System.Collections.Concurrent.ConcurrentDictionary<int, object?>());
            // Graphics esplicito NullGraphicsService: PrintDispatch inoltra i PRINT alla console
            // dell'host SOLO se la grafica non è disponibile — se il processo ha settato
            // HeadlessRuntimeHost.DefaultGraphics a una SDL reale (runner grafico), l'output della
            // pagina finirebbe su SDL invece che nel StringWriter → body BSP vuoto (200 con len 0).
            IlRuntime.SetCurrent(new HeadlessRuntimeHost(outputWriter, null, new NullGraphicsService()));
            System.Console.SetOut(outputWriter);
            var pcodeResponse = new Http.PCodeHttpResponse();

            // Semina i globali BSP per NOME (mai un SymbolId assunto — l'ordine della SymbolTable
            // dipende dal linking; REQ/RES/SESSION ora sono anche DIM tipizzati dal BspCompiler).
            var sessionSeed = JsonObjectToDict(sessionData);
            var formDict = ParseFormToDict(req);
            for (var i = 0; i < program.Symbols.Count; i++)
            {
                var sym = program.Symbols.GetSymbol(i);
                switch (sym.OriginalName)
                {
                    case "REQ":
                        {
                            // REQ("Session") = alias di SESSION (stesso dict, 2026-09-02):
                            // le pagine possono usare entrambe le sintassi.
                            var reqDict = ConvertRequest(req);
                            reqDict["Session"] = sessionSeed;
                            IlRuntime.StoreGlobal(i, reqDict);
                        }
                        break;
                    case "RES": IlRuntime.StoreGlobal(i, pcodeResponse); break;
                    case "SESSION": IlRuntime.StoreGlobal(i, sessionSeed); break;
                    case "FORM": IlRuntime.StoreGlobal(i, formDict); break;
                    case "QUERY": IlRuntime.StoreGlobal(i, req.Query.ToDictionary(kv => kv.Key, kv => (object?)kv.Value)); break;
                }
            }
            // FORM/QUERY sono keyword (TokenKind.Form/SqlQuery): NON dichiarabili con DIM — il
            // linker le assegna a DynamicScopeSymbolId (-3) che NON appare nella SymbolTable, quindi
            // il loop sopra non le vede. L'accesso `FORM("x")`/`QUERY["x"]` risolve per NOME dal bag
            // dynamic-scope: va iniettato QUI, incondizionato (stesso contratto del PCode, che le
            // semina per nome via GlobalSlots).
            IlRuntime.DynamicScopeSet("FORM", formDict);
            IlRuntime.DynamicScopeSet("QUERY", req.Query.ToDictionary(kv => kv.Key, kv => (object?)kv.Value));

            var entry = assembly.GetType("GeneratedProgram")?.GetMethod("Main",
                System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
            if (entry == null)
                return new HttpResponse { StatusCode = 500, ContentType = "text/plain", Body = "500 BSP IL: entry point non trovato." };

            Exception? runException = null;
            try { entry.Invoke(null, null); }
            catch (System.Reflection.TargetInvocationException tie) { runException = tie.InnerException ?? tie; }
            catch (Exception ex) { runException = ex; }

            // Riscrive nella sessione VIVA le modifiche fatte dalla pagina (stesso contratto del
            // path P-Code: il dict seminato è una COPIA di sessionData, va riportata indietro).
            // DA FARE PRIMA del restore dei GLOBAL (il dict mutato vive ancora in ProgramGlobals).
            for (var i = 0; i < program.Symbols.Count; i++)
            {
                var sym = program.Symbols.GetSymbol(i);
                if (sym.OriginalName == "SESSION")
                {
                    if (IlRuntime.LoadGlobal(i) is Dictionary<string, object?> mutatedSession)
                    {
                        sessionData.Clear();
                        foreach (var kv in mutatedSession)
                            sessionData[kv.Key] = kv.Value?.ToString();
                    }
                    break;
                }
            }

            HttpResponse result;
            if (runException != null)
            {
                result = new HttpResponse
                {
                    StatusCode = 500, ContentType = "text/plain",
                    Body = $"500 BSP IL Error: {runException.Message}",
                };
            }
            else if (pcodeResponse.Sent)
            {
                result = new HttpResponse
                {
                    StatusCode = pcodeResponse.StatusCode,
                    Body = System.Text.Encoding.UTF8.GetString(pcodeResponse.Body),
                    ContentType = pcodeResponse.Headers.TryGetValue("Content-Type", out var ct) ? ct : "text/html; charset=utf-8",
                    Headers = pcodeResponse.Headers,
                };
            }
            else
            {
                // Nessun RES.Send*/Redirect esplicito: l'output è quello dei PRINT (stessa
                // semantica ASP-classic del path P-Code).
                var body = System.Text.Encoding.UTF8.GetBytes(outputWriter.ToString());
                CommonLib.Http.BspRuntime.RenderPut(renderKey, body);
                result = new HttpResponse { BodyBytes = body, ContentType = "text/html; charset=utf-8" };
            }

            if (setCookie != null)
                result.Headers["Set-Cookie"] = setCookie;
            return FinishBsp(result, req);
        }
        finally
        {
            System.Console.SetOut(savedConsoleOut);
            IlRuntime.EndGlobalScope();
        }
    }

    private static Dictionary<string, object?> JsonObjectToDict(System.Text.Json.Nodes.JsonObject obj)
    {
        var dict = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in obj) dict[kv.Key] = kv.Value?.ToString();
        return dict;
    }

    /// <summary>
    /// Popola FORM dal body SOLO per richieste application/x-www-form-urlencoded (mai per body
    /// JSON o GET). Implementazione UNICA condivisa con il backend PCode/IL: delega a
    /// CommonLib.Http.RequestBodyParser.ParseFormDictionary (allineamento 2026-09-02).
    /// </summary>
    private static Dictionary<string, object?> ParseFormToDict(HttpRequest req)
    {
        var ct = req.Headers.TryGetValue("Content-Type", out var c) ? c : null;
        return CommonLib.Http.RequestBodyParser.ParseFormDictionary(ct, req.Body);
    }

    private static Dictionary<string, object?> ConvertRequest(HttpRequest req) => new()
    {
        ["Method"] = req.Method,
        ["Path"] = req.Path,
        ["Body"] = req.Body,
        ["Headers"] = req.Headers.ToDictionary(kv => kv.Key, kv => (object?)kv.Value),
        ["Query"] = req.Query.ToDictionary(kv => kv.Key, kv => (object?)kv.Value),
    };
}
