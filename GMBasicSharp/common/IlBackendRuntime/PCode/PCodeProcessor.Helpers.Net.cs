// FILE: IlBackendRuntime/PCode/PCodeProcessor.Helpers.Net.cs
// PARTIAL — dominio HELPERS/NET: ExecuteNetCmd + HTTP server/route/use/config/static/bsp.
// Migrato da PCodeProcessor.Helpers.cs (2026-08-30 — split, file <= 1200 righe).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Basic.Ledger;
using Compiler;
using System.Globalization;
using System.Linq;
using System.Net;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private object? ExecuteNetCmd(PCodeTask task, byte subCode, object?[] args)
    {
        switch ((NetSubOpCode)subCode)
        {
            case NetSubOpCode.TCP_OPEN:
                // TCP OPEN porta[, engine] → listener server. Prima usava TcpConnect:
                // "Can't assign requested address" (trattava la porta come host).
                if (args.Length >= 1)
                    return _runtime.Network.TcpListen("", Convert.ToInt32(args[0]));
                return null;
            case NetSubOpCode.TCP_CONNECT:
                // TCP CONNECT host, porta[, engine[, timeout]] → client.
                if (args.Length >= 2)
                {
                    var host = args[0]?.ToString() ?? "";
                    var port = Convert.ToInt32(args[1]);
                    ParkAsync(task, PCodeWaitKind.Network,
                        ct => _runtime.Network.TcpConnectAsync(host, port, ct),
                        handle => task.PushRef(handle),
                        args.Length > 3 ? NetworkTimeoutMs(args[3]) : 0,
                        () => task.PushRef(-1L));
                    return ParkedNetResult;
                }
                return null;
            case NetSubOpCode.TCP_CLOSE:
                // Handle pushati da PushRef come long boxed — NON usare 'is double'
                // (fallisce silenziosamente → comando mai eseguito).
                if (args.Length >= 1 && args[0] != null)
                    _runtime.Network.TcpClose(Convert.ToInt64(args[0]));
                return null;
            case NetSubOpCode.TCP_SEND:
                if (args.Length >= 2 && args[0] != null)
                {
                    var handle = Convert.ToInt64(args[0]);
                    var data = System.Text.Encoding.UTF8.GetBytes(args[1]?.ToString() ?? "");
                    ParkAsync(task, PCodeWaitKind.Network, async ct =>
                    {
                        await _runtime.Network.TcpSendAsync(handle, data, ct).ConfigureAwait(false);
                        return true;
                    }, _ => { });
                    return ParkedNetResult;
                }
                return null;
            case NetSubOpCode.TCP_RECV:
                // TCP RECV handle, count → (bytes, buffer). Il compiler fa AssignVariable(BytesReceived)
                // poi AssignVariable(Buffer): pushiamo in ordine INVERSO (Buffer in fondo, BytesReceived
                // in cima) così il primo pop va a BytesReceived. Prima pushava un solo byte[] → Buffer
                // prendeva il byte[] e BytesReceived restava null.
                // PushRef (stack GLOBALE task), NON PushF: gli AssignVariable che seguono OP_NET_CMD
                // consumano con OP_STORE_LOCAL/OP_STORE_GLOBAL → task.PopRef() (stack globale). PushF
                // finisce su FrameStack quando task.Frame != null → nel Main sequenziale (frame attivo)
                // i due valori finivano sullo stack del frame, mai letti: bytesReceived restava 0 e la
                // DO...LOOP WHILE bytesReceived>0 usciva con risposta vuota (NetSslConnect su PCode).
                // Nelle GO SUB funzionava perché applyResult gira con task.Frame null → fallback PushRef.
                if (args.Length >= 2 && args[0] != null)
                {
                    var handle = Convert.ToInt64(args[0]);
                    const int count = 4096;
                    ParkAsync(task, PCodeWaitKind.Network,
                        ct => _runtime.Network.TcpReceiveAsync(handle, count, ct), tcpData =>
                    {
                        var recvStr = tcpData != null ? System.Text.Encoding.UTF8.GetString(tcpData) : "";
                        task.PushRef(recvStr);          // Buffer (pop #2)
                        task.PushRef((double)(tcpData?.Length ?? 0)); // BytesReceived (pop #1)
                    }, NetworkTimeoutMs(args[1]), () =>
                    {
                        task.PushRef("");
                        task.PushRef(0.0);
                    });
                    return ParkedNetResult;
                }
                return null;
            case NetSubOpCode.TCP_ACCEPT:
                if (args.Length >= 1 && args[0] != null)
                {
                    var listener = Convert.ToInt64(args[0]);
                    ParkAsync(task, PCodeWaitKind.Network,
                        ct => _runtime.Network.TcpAcceptAsync(listener, ct),
                        handle => task.PushRef(handle),
                        NetworkTimeoutMs(args[1]),
                        () => task.PushRef(-1L));
                    return ParkedNetResult;
                }
                return null;
            case NetSubOpCode.TCP_PEEK:
                // NET.PEEK(handle, count): come TCP_RECV ma senza consumare i byte
                // (SocketFlags.Peek) — park-based via TcpPeekAsync, mai piu' un
                // Socket.Receive sincrono sul worker GPM (vedi Bugs/(solved) -
                // NET-TcpPeek-Bloccante-Hang-Test-Goroutine.md). Firma BASIC senza
                // parametro di timeout esplicito → timeoutMs=0 (nessun timeout, il park
                // e' I/O asincrono reale: il worker non resta comunque occupato).
                if (args.Length >= 2 && args[0] != null)
                {
                    var handle = Convert.ToInt64(args[0]);
                    var count = Convert.ToInt32(args[1]);
                    ParkAsync(task, PCodeWaitKind.Network,
                        ct => _runtime.Network.TcpPeekAsync(handle, count, ct),
                        peekData => task.PushRef(peekData != null ? System.Text.Encoding.UTF8.GetString(peekData) : ""));
                    return ParkedNetResult;
                }
                return null;
            // ══════════ UDP (prima: nessun case → no-op silenziosi) ══════════
            case NetSubOpCode.UDP_OPEN:
                // UDP OPEN [porta][, engine] → handle. Porta omessa (UDP OPEN, handle) = 0 (auto).
                if (args.Length >= 1 && args[0] != null)
                    return _runtime.Network.UdpOpen(Convert.ToInt32(args[0]));
                return _runtime.Network.UdpOpen(0);
            case NetSubOpCode.UDP_SEND:
                // UDP SEND handle, host, porta, data (4 argomenti, ordine emesso dal compiler).
                if (args.Length >= 4 && args[0] != null)
                {
                    var handle = Convert.ToInt64(args[0]);
                    var data = System.Text.Encoding.UTF8.GetBytes(args[3]?.ToString() ?? "");
                    var host = args[1]?.ToString() ?? "";
                    var port = Convert.ToInt32(args[2]);
                    ParkAsync(task, PCodeWaitKind.Network, async ct =>
                    {
                        await _runtime.Network.UdpSendAsync(handle, data, host, port, ct).ConfigureAwait(false);
                        return true;
                    }, _ => { });
                    return ParkedNetResult;
                }
                return null;
            case NetSubOpCode.UDP_RECV:
                // UDP RECV handle[, timeout] → compiler assegna (RemotePort, RemoteIp, Buffer):
                // pushiamo (Buffer, RemoteIp, RemotePort) in ordine inverso. PushRef (stack GLOBALE),
                // non PushF: gli AssignVariable dopo OP_NET_CMD consumano task.PopRef() — vedi TCP_RECV.
                if (args.Length >= 1 && args[0] != null)
                {
                    var handle = Convert.ToInt64(args[0]);
                    ParkAsync(task, PCodeWaitKind.Network,
                        ct => _runtime.Network.UdpReceiveAsync(handle, ct), udpRes =>
                    {
                        var data = udpRes?.Data != null ? System.Text.Encoding.UTF8.GetString(udpRes.Value.Data) : "";
                        task.PushRef(data);                                // Buffer (pop #3)
                        task.PushRef(udpRes?.Host ?? "");                  // RemoteIp (pop #2)
                        task.PushRef((double)(udpRes?.Port ?? 0));         // RemotePort (pop #1)
                    }, args.Length > 1 ? NetworkTimeoutMs(args[1]) : 0, () =>
                    {
                        task.PushRef("");
                        task.PushRef("");
                        task.PushRef(0.0);
                    });
                    return ParkedNetResult;
                }
                return null;
            case NetSubOpCode.UDP_CLOSE:
                if (args.Length >= 1 && args[0] != null)
                    _runtime.Network.UdpClose(Convert.ToInt64(args[0]));
                return null;
            // ══════════ SSL (prima: nessun case → no-op silenziosi) ══════════
            case NetSubOpCode.SSL_OPEN:
                // SSL OPEN porta, certPath[, certPass] → listener TLS.
                if (args.Length >= 1 && args[0] != null)
                    return _runtime.Network.SslListen(Convert.ToInt32(args[0]),
                        args.Length > 1 ? args[1]?.ToString() : null,
                        args.Length > 2 ? args[2]?.ToString() : null);
                return null;
            case NetSubOpCode.SSL_CONNECT:
                // SSL CONNECT host, porta[, timeout] → client TLS.
                if (args.Length >= 2 && args[0] != null)
                {
                    var host = args[0]?.ToString() ?? "";
                    // Porta omessa (PUSH_NIL): Convert.ToInt32(null) = 0 → connessione su porta 0
                    // sempre fallita (esposto da "SSL CONNECT host, h" in NetSslConcurrency su
                    // pcode). Default 443 come IL (IlProgramCompiler.Statements.Network.cs:501),
                    // (C99CodeEmitter.Rete.cs:819) e C99Native (emit_dflt "443").
                    var port = args[1] is null ? 443 : Convert.ToInt32(args[1]);
                    ParkAsync(task, PCodeWaitKind.Network,
                        ct => _runtime.Network.SslConnectAsync(host, port, null, ct),
                        handle => task.PushRef(handle),
                        args.Length > 2 ? NetworkTimeoutMs(args[2]) : 0,
                        () => task.PushRef(-1L));
                    return ParkedNetResult;
                }
                return null;
            case NetSubOpCode.SHUTDOWN:
                // SHUTDOWN handle, mode (0=both, 1=send, 2=receive).
                if (args.Length >= 2 && args[0] != null)
                    _runtime.Network.TcpShutdown(Convert.ToInt64(args[0]), args[1]?.ToString() ?? "both");
                return null;
            case NetSubOpCode.SETOPTION:
                // SETOPTION handle, "TCP_NODELAY"|"SO_REUSEADDR"|"SO_KEEPALIVE", value.
                if (args.Length >= 3 && args[0] != null)
                    _runtime.Network.TcpSetOption(Convert.ToInt64(args[0]), args[1]?.ToString() ?? "", args[2]?.ToString() ?? "");
                return null;
            case NetSubOpCode.HTTP_SERVER:
                return ExecuteHttpServer(task, args);
            case NetSubOpCode.HTTP_ROUTE:
                ExecuteHttpRoute(args);
                return null;
            case NetSubOpCode.HTTP_USE:
                ExecuteHttpUse(args);
                return null;
            case NetSubOpCode.HTTP_USE_NEXT:
                ExecuteHttpUseNext(task, args);
                return null;
            case NetSubOpCode.HTTP_CONFIG:
                return ExecuteHttpConfig(task, args);
            case NetSubOpCode.HTTP_STATIC:
                ExecuteHttpStatic(args);
                return null;
            case NetSubOpCode.WS_CONNECT:
                return ExecuteWebSocketConnect(task, args);
            case NetSubOpCode.HTTP_BSP:
                ExecuteHttpBsp(args);
                return null;
            default:
                return null;
        }
    }

    /// <summary>
    /// HTTP SERVER [porta[, callback[, certPath[, certPass]]]] / HTTP SERVER STOP.
    /// Ordine argomenti reale emesso da Compiler.Network.cs::VisitHttpServerStatement (verificato
    /// leggendo il compilatore, non assunto): args[0]=isStopCommand (bool), args[1]=porta,
    /// args[2]=indirizzo Ip dell'handler (double, da OP_PUSH_CODE_REF) o null, args[3]=certPath,
    /// args[4]=certPassword. Il prototipo precedente in GMBasic aveva questi indici sbagliati
    /// (si aspettava un configDict che il compilatore non emette mai) — non riusato.
    /// </summary>
    /// <summary>
    /// Ferma l'eventuale server HTTP rimasto attivo a FINE RUN (chiamato da
    /// <see cref="PCodeVm.Dispose"/>). `HTTP SERVER STOP` passa dal park asincrono: se il
    /// programma termina mentre lo stop è in volo (o non chiama STOP) il <see cref="TcpListener"/>
    /// resta legato alla porta, e in un harness che esegue più programmi nello stesso processo
    /// (MiniTest xUnit) il run successivo — che usa la STESSA porta perché è lo stesso file —
    /// non riesce a fare bind e le richieste vanno in TIMEOUT. Best-effort, bounded: non deve
    /// mai far fallire il Dispose.
    /// </summary>
    public void StopHttpServerSync()
    {
        PCodeHttpServerInstance? toStop;
        lock (_httpLock) { toStop = _httpServer; _httpServer = null; _httpRouter = null; _httpGlobalsTemplate = null; }
        if (toStop == null) return;
        try { PCodeHttpServer.StopAsync(toStop).Wait(TimeSpan.FromSeconds(2)); } catch { /* best effort */ }
    }

    private object? ExecuteHttpServer(PCodeTask task, object?[] args)
    {
        var isStop = args.Length > 0 && args[0] is bool b && b;
        if (isStop)
        {
            PCodeHttpServerInstance? toStop;
            lock (_httpLock) { toStop = _httpServer; _httpServer = null; _httpRouter = null; _httpGlobalsTemplate = null; }
            if (toStop != null)
            {
                ParkAsync(task, PCodeWaitKind.Http,
                    async ct =>
                    {
                        await PCodeHttpServer.StopAsync(toStop).WaitAsync(ct).ConfigureAwait(false);
                        return true;
                    },
                    _ => { });
                return ParkedNetResult;
            }
            return null;
        }

        var port = args.Length > 1 && args[1] is double p ? (int)p : 8080;
        int? fallbackHandlerIp = args.Length > 2 && args[2] is double h ? (int)h : null;
        var certPathArg = args.Length > 3 ? args[3]?.ToString() : null;
        var certPasswordArg = args.Length > 4 ? args[4]?.ToString() : null;

        // HTTP CONFIG puo' gia' aver girato PRIMA di HTTP SERVER (ordine convenzionale usato dai
        // sample con TLS, es. HttpFetchAdvanced.bas: HTTP CONFIG cfg imposta CertificatePath, POI
        // HTTP SERVER porta senza riscrivere gli argomenti cert). Costruire qui un HttpConfig
        // completamente nuovo da zero, usando SOLO gli argomenti posizionali di HTTP SERVER (quasi
        // sempre solo la porta), scartava silenziosamente TUTTO cio' che HTTP CONFIG aveva appena
        // impostato — certificato incluso: il server partiva in chiaro (nessun certificato, nessun
        // SslStream) mentre il codice BASIC credeva di aver configurato HTTPS. Il client FETCH
        // faceva poi un handshake TLS contro un server che parlava HTTP puro — nessun ServerHello
        // sarebbe mai arrivato, quindi il client restava appeso fino al timeout di default di
        // HttpClient (100s), percepito come un hang indefinito ("Timeout / no output"). Bug reale,
        // non specifico al WebSocket: colpisce QUALUNQUE combinazione HTTP CONFIG (con certificato)
        // seguito da HTTP SERVER senza riscrivere gli argomenti cert nello statement stesso.
        // Fix: se _httpConfig esiste gia' (HTTP CONFIG e' gia' girato) e HTTP SERVER non specifica
        // esplicitamente i propri argomenti cert, riusare quelli di _httpConfig — la forma legacy
        // "HTTP SERVER porta, callback, certPath, certPass" (nessun HTTP CONFIG prima) resta invariata,
        // i suoi argomenti espliciti hanno sempre precedenza.
        var certPath = certPathArg ?? _httpConfig?.CertificatePath;
        var certPassword = certPasswordArg ?? _httpConfig?.CertificatePassword;
        var config = _httpConfig ?? new HttpConfig();
        config.Port = port;
        config.CertificatePath = certPath;
        config.CertificatePassword = certPassword;
        var certificate = PCodeHttpServer.LoadCertificate(certPath, certPassword);

        lock (_httpLock)
        {
            // HTTP ROUTE/HTTP USE possono gia' aver girato PRIMA di HTTP SERVER (e' l'ordine
            // convenzionale usato da tutti i sample .bas esistenti) — il router va RIUSATO, mai
            // ricreato qui, altrimenti le route registrate in precedenza sparirebbero.
            var router = _httpRouter ??= new PCodeHttpRouter();
            router.FallbackHandlerIp = fallbackHandlerIp;
            _httpConfig = config;
            // Il thread che accetta connessioni non e' un PCodeTask e non ha un proprio .Globals
            // da cui copiare — va catturato ORA, dal task che sta eseguendo lo statement HTTP
            // SERVER, esattamente come LaunchGoTask fa per OP_GO/OP_GO_FUNCTION.
            _httpGlobalsTemplate = task.Globals;
             _httpServer = PCodeHttpServer.Start(port, certificate, config, DispatchHttpRequestAsync);
             if (_httpBspDir == null) { var d = Path.Combine(Directory.GetCurrentDirectory(), "pages"); if (Directory.Exists(d)) _httpBspDir = d; }
             if (_httpBspDir == null && _httpStaticBaseDir != null) { var d = Path.GetFullPath(Path.Combine(_httpStaticBaseDir, "..", "pages")); if (Directory.Exists(d)) _httpBspDir = d; }
        }
        return null;
    }

    /// <summary>HTTP ROUTE metodo, path, handler — registra una route nel router. Puo' essere
    /// chiamata PRIMA di HTTP SERVER (ordine convenzionale nei sample esistenti): il router va
    /// quindi creato pigramente qui se non esiste ancora, mai assunto gia' presente.</summary>
    private void ExecuteHttpRoute(object?[] args)
    {
        if (args.Length < 3 || args[2] is not double h) return;
        var method = args[0]?.ToString() ?? "*";
        var path = args[1]?.ToString() ?? "/";
        lock (_httpLock) { (_httpRouter ??= new PCodeHttpRouter()).AddRoute(method, path, (int)h); }
    }

    /// <summary>HTTP USE [path,] handler — registra un middleware. Come HTTP ROUTE, puo' girare
    /// prima di HTTP SERVER (ordine convenzionale nei sample) — router creato pigramente se
    /// serve. args[0]=path (o nil), args[1]=indirizzo handler.</summary>
    private void ExecuteHttpUse(object?[] args)
    {
        if (args.Length < 2 || args[1] is not double h) return;
        var path = args[0]?.ToString() ?? "/";
        lock (_httpLock) { (_httpRouter ??= new PCodeHttpRouter()).AddMiddleware("*", path, (int)h); }
    }

    /// <summary>
    /// HTTP USE NEXT nextHandle — semantica "trampolino": se c'e' un passo successivo nella
    /// catena, lo spawna (stesso Request/Response, coda rimanente ridotta di uno) e aggiorna
    /// PCodeHttpChainState.CurrentTask cosi' che DispatchHttpRequestAsync possa rilevare
    /// un'eventuale eccezione del NUOVO task attivo, non solo del primo. In ogni caso il task
    /// CORRENTE termina immediatamente qui — il resto del corpo del middleware (dopo la chiamata
    /// a HTTP USE NEXT) non viene mai eseguito, esattamente come nell'AST (li' ottenuto con un
    /// unwind dello stack di chiamata; qui e' piu' semplice: e' gia' un task a parte).
    /// </summary>
    private void ExecuteHttpUseNext(PCodeTask task, object?[] args)
    {
        // Fallback per la sintassi implicita "HTTP USE NEXT" (senza handle esplicito) in un
        // middleware che NON dichiara un parametro "next" — caso legittimo, vedi HttpRoutes.bas
        // Esempio 1 (SUB LoggerMiddleware(req, res), nessun 3° parametro). Il parser genera comunque
        // un riferimento a una variabile chiamata "next" (Parser.Stmt.Network.cs, ParseHttpUseNextStatement)
        // per uniformità con la sintassi esplicita, ma nello scope di una SUB a 2 parametri quel nome
        // non e' mai dichiarato: il compilatore (Compiler.Variables.cs, LoadVariable) non lo tratta
        // come errore — nessun local/global "next" trovato, push NIL silenzioso, comportamento
        // generale e corretto per QUALUNQUE variabile non dichiarata in questo linguaggio, non
        // specifico a HTTP USE NEXT. Risultato: args[0] e' NIL, non un vero PCodeHttpNextHandle, e la
        // catena si bloccava qui per sempre (bug: "StatusCode=0 su tutte le richieste dopo la prima",
        // vedi TODO-PCODE-BUGS.md). Fix: SpawnHttpHandlerTask registra SEMPRE l'handle passato alla
        // catena in _httpTaskNextHandle (keyed per PCodeTask), indipendentemente da quanti parametri
        // dichiara la SUB di destinazione — qui, se l'argomento BASIC non e' un handle valido, si
        // ripesca da li'.
        var effective = args.Length >= 1 && args[0] is Basic.IlBackend.PCode.Http.PCodeHttpNextHandle explicitHandle
            ? explicitHandle
            : _httpTaskNextHandle.TryGetValue(task, out var stashed) ? stashed : null;
        if (effective is { } next && next.Remaining.Count > 0)
        {
            var nextIp = next.Remaining.Dequeue();
            var newNext = new Basic.IlBackend.PCode.Http.PCodeHttpNextHandle
            {
                Remaining = next.Remaining,
                RequestValue = next.RequestValue,
                Response = next.Response,
                ChainState = next.ChainState
            };
            var spawned = SpawnHttpHandlerTask(_httpGlobalsTemplate!, nextIp, [next.RequestValue, next.Response, newNext]);
            next.ChainState.CurrentTask = spawned;
        }
        task.State = PCodeTaskState.Finished;
    }

    /// <summary>
    /// HTTP CONFIG configObject — legge le proprieta' rilevanti (Port/CertificatePath/
    /// CertificatePassword/RequestTimeoutMs/RateLimitRequestsPerMinute/RateLimitBurstSize/
    /// EnableKeepAlive) da un Dictionary generico. Nota: NEW HTTPCONFIG non produce ancora un
    /// oggetto realmente popolabile nella P-CODE VM (OP_NEW non ha un case per il typeCode di
    /// HTTPOPTIONS/HTTPCONFIG — gap pre-esistente del compilatore/VM, non specifico a questo
    /// lavoro HTTP, non toccato qui per non rischiare di alterare altri tipi built-in che
    /// condividono lo stesso typeCode di fallback) — se l'oggetto passato non e' un dizionario
    /// utilizzabile, si applicano semplicemente i default (stessi di CommonLib.Http.HttpConfig),
    /// cosi' il rate limiter resta comunque configurato e funzionante.
    /// Applica per davvero il rate limiting (deviazione intenzionale rispetto all'AST, dove e'
    /// configurato ma mai controllato nel flusso vivo — vedi piano Obsidian) e, se un server e'
    /// gia' attivo, fa hot-reload chiudendo esplicitamente il vecchio listener prima di aprirne
    /// uno nuovo (l'AST invece lascia temporaneamente due listener attivi).
    /// </summary>
    private object? ExecuteHttpConfig(PCodeTask task, object?[] args)
    {
        var cfgDict = args.Length > 0 ? args[0] as System.Collections.IDictionary : null;
        /* Numeri: il dict BASIC può contenere double (default della VM) ma anche int/long
         * (assegnazione da un'espressione intera, es. `httpCfg.MaxRequestBodySize = n * 4 \ 3`).
         * Prima si accettava SOLO double: un valore intero veniva ignorato in silenzio e
         * restava il default (bug reale: MaxRequestBodySize non applicato → 413 sui pacchetti
         * grandi pubblicati, mentre su IL funzionava). */
        static double CoerceNum(object? v) => v switch
        {
            double d => d,
            float f => f,
            long l => l,
            int i => i,
            short s => s,
            decimal m => (double)m,
            _ => double.NaN,
        };
        double GetNum(string k, double def)
        {
            if (cfgDict == null || !cfgDict.Contains(k)) return def;
            var d = CoerceNum(cfgDict[k]);
            return double.IsNaN(d) ? def : d;
        }
        string? GetStr(string k) => cfgDict != null && cfgDict.Contains(k) ? cfgDict[k]?.ToString() : null;
        bool GetBool(string k, bool def) => cfgDict != null && cfgDict.Contains(k) && cfgDict[k] is bool b ? b : def;

        var defaults = new HttpConfig();
        var port = (int)GetNum("Port", defaults.Port);
        var certPath = GetStr("CertificatePath");
        var certPassword = GetStr("CertificatePassword");
        var requestTimeoutMs = (int)GetNum("RequestTimeoutMs", GetNum("DefaultRequestTimeout", defaults.RequestTimeoutMs));
        /* MaxRequestBodySize: in HttpConfig il default è 10 MB. Era l'UNICO campo numerico che
         * questo dispatcher non leggeva (parità rotta con IL, che lo applica in IlRuntime):
         * risultato, una pubblicazione > ~7,5 MB veniva rifiutata con 413 anche dichiarando
         * `maxPackageBytes` 100 MB nel server del registry. */
        var maxRequestBodySize = (long)GetNum("MaxRequestBodySize", defaults.MaxRequestBodySize);
        var rateLimitPerMin = (int)GetNum("RateLimitRequestsPerMinute", defaults.RateLimitRequestsPerMinute);
        var rateLimitBurst = (int)GetNum("RateLimitBurstSize", defaults.RateLimitBurstSize);
        var bspCacheMs = (int)GetNum("BspCacheMs", defaults.BspCacheMs);
        var bspEngine = GetStr("BspEngine") ?? defaults.BspEngine;
        var bspEnginePages = GetStr("BspEnginePages") ?? defaults.BspEnginePages;
        var bspConcurrentPages = GetStr("BspConcurrentPages") ?? defaults.BspConcurrentPages;
        var enableKeepAlive = GetBool("EnableKeepAlive", defaults.EnableKeepAlive);
        var securityHeaders = GetBool("SecurityHeaders", defaults.SecurityHeaders);
        var allowedIPs = GetStr("AllowedIPs") ?? defaults.AllowedIPs;
        var blockedIPs = GetStr("BlockedIPs") ?? defaults.BlockedIPs;
        var sessions = GetBool("Sessions", defaults.Sessions);
        var sessionFile = GetStr("SessionFile") ?? defaults.SessionFile;

        var newConfig = new HttpConfig
        {
            Port = port,
            CertificatePath = certPath,
            CertificatePassword = certPassword,
            RequestTimeoutMs = requestTimeoutMs,
            MaxRequestBodySize = maxRequestBodySize,
            RateLimitRequestsPerMinute = rateLimitPerMin,
            RateLimitBurstSize = rateLimitBurst,
            BspCacheMs = bspCacheMs,
            BspEngine = bspEngine ?? "managed",
            BspEnginePages = bspEnginePages ?? "",
            BspConcurrentPages = bspConcurrentPages ?? "",
            EnableKeepAlive = enableKeepAlive,
            SecurityHeaders = securityHeaders,
            AllowedIPs = allowedIPs,
            BlockedIPs = blockedIPs,
            Sessions = sessions,
            SessionFile = sessionFile
        };

        // Persistenza sessioni SQLite (RDD-SQL) configurata da BASIC: HTTP CONFIG .SessionFile
        if (!string.IsNullOrEmpty(sessionFile))
            Basic.Runtime.Services.SessionStorage.ConfigureFile(sessionFile);

        PCodeHttpServerInstance? oldServer;
        lock (_httpLock)
        {
            _httpConfig = newConfig;
            // Memoizzazione della resa BSP (statica: il server è per-processo).
            CommonLib.Http.BspRuntime.RenderCacheTtlMs = bspCacheMs;
            CommonLib.Http.BspRuntime.EnginePreference =
                string.Equals(bspEngine, "c", StringComparison.OrdinalIgnoreCase)
                    ? CommonLib.Http.BspEngine.C : CommonLib.Http.BspEngine.Managed;
            CommonLib.Http.BspRuntime.SetEngineCPages(bspEnginePages);
            CommonLib.Http.BspRuntime.ConcurrentPages = bspConcurrentPages ?? "";
            CommonLib.Http.BspC.SetConcurrentPages(CommonLib.Http.BspRuntime.ConcurrentPages);
            // "0 = nessun limite" è il contratto documentato di RateLimitRequestsPerMinute
            // (HttpServerSettings) e il backend IL lo rispetta (`if (... > 0)`) . Qui invece il
            // limiter veniva SEMPRE creato e configurato: con 0 il token-bucket partiva con 0
            // token → OGNI richiesta 429 (divergenza PCode/IL, trovata con `HTTPCONFIG` a 0).
            if (rateLimitPerMin > 0)
                (_httpRateLimiter ??= new Basic.IlBackend.PCode.Http.PCodeHttpRateLimiter())
                    .Configure(rateLimitPerMin, rateLimitBurst > 0 ? rateLimitBurst : 1);
            else
                _httpRateLimiter = null;
            oldServer = _httpServer;
            if (oldServer != null) _httpServer = null;
        }

        if (oldServer == null) return null;

        ParkAsync(task, PCodeWaitKind.Http,
            async ct =>
            {
                await PCodeHttpServer.StopAsync(oldServer).WaitAsync(ct).ConfigureAwait(false);
                return true;
            },
            _ =>
            {
                lock (_httpLock)
                {
                    // Un'altra goroutine puo' aver eseguito HTTP SERVER/CONFIG mentre lo stop era
                    // in volo. Riavvia solo se questa configurazione e' ancora quella corrente e
                    // nessun nuovo listener l'ha gia' sostituita.
                    if (_httpServer != null || !ReferenceEquals(_httpConfig, newConfig)) return;
                    var certificate = PCodeHttpServer.LoadCertificate(certPath, certPassword);
                    _httpServer = PCodeHttpServer.Start(port, certificate, newConfig, DispatchHttpRequestAsync);
                    if (_httpBspDir == null)
                    {
                        var d = Path.Combine(Directory.GetCurrentDirectory(), "pages");
                        if (Directory.Exists(d)) _httpBspDir = d;
                    }
                }
            });
        return ParkedNetResult;
    }

    /// <summary>HTTP STATIC urlPrefix, physicalPath — registra un prefisso URL da servire come
    /// directory di file statici. Il path fisico e' risolto rispetto alla CWD del processo (la
    /// VM non ha un concetto di "directory radice del programma" separato — stesso comportamento
    /// degli altri opcode di file I/O gia' esistenti, nessuna convenzione dedicata introdotta
    /// qui). args[0]=prefisso URL, args[1]=path fisico.</summary>
    private void ExecuteHttpStatic(object?[] args)
    {
        if (args.Length < 2) return;
        var urlPrefix = args[0]?.ToString() ?? "/";
        var physicalPath = args[1]?.ToString();
        if (string.IsNullOrEmpty(physicalPath)) return;
        var fullBase = Path.GetFullPath(physicalPath);
        lock (_httpLock)
        {
            (_httpRouter ??= new PCodeHttpRouter()).RegisterStaticPath(urlPrefix, fullBase);
            _httpStaticBaseDir = fullBase;
            if (_httpBspDir == null)
            {
                var pagesDir = Path.GetFullPath(Path.Combine(fullBase, "..", "pages"));
                if (Directory.Exists(pagesDir)) _httpBspDir = pagesDir;
                // Layout più comune (es. samples/Web/RegistryBsp): le pagine .bsp
                // vivono NELLA STESSA directory servita come statica, senza una sotto-cartella
                // "pages" dedicata — senza questo fallback, _httpBspDir restava null per
                // sempre e DispatchHttpRequestAsync non tentava mai TryServeBspPage: ogni
                // richiesta a *.bsp cadeva su TryServeStaticFile, che serve il SORGENTE
                // .bsp grezzo (mai renderizzato) invece della pagina compilata/eseguita.
                else if (Directory.Exists(fullBase)
                         && Directory.GetFiles(fullBase, "*.bsp", SearchOption.TopDirectoryOnly).Length > 0)
                    _httpBspDir = fullBase;
            }
        }
    }

    private void ExecuteHttpBsp(object?[] args)
    {
        if (args.Length < 2) return;
        var urlPattern = args[0]?.ToString();
        var physicalDir = args[1]?.ToString();
        if (string.IsNullOrEmpty(urlPattern) || string.IsNullOrEmpty(physicalDir)) return;
        lock (_httpLock) { _httpBspDir = Path.GetFullPath(physicalDir); }
    }

    private static readonly Dictionary<string, string> HttpStaticMimeTypes = new(StringComparer.OrdinalIgnoreCase)
    {
        [".html"] = "text/html", [".htm"] = "text/html", [".css"] = "text/css",
        [".js"] = "application/javascript", [".json"] = "application/json",
        [".png"] = "image/png", [".jpg"] = "image/jpeg", [".jpeg"] = "image/jpeg",
        [".gif"] = "image/gif", [".svg"] = "image/svg+xml", [".txt"] = "text/plain",
    };

    /// <summary>
    /// Invia il corpo di una pagina BSP: Content-Type HTML (se la pagina non ne ha già uno),
    /// ETag debole + 304 su If-None-Match, gzip se il client lo accetta e il corpo è &gt; 1 KB.
    /// Il costo dell'hash (~47 KB) è trascurabile rispetto all'esecuzione della pagina.
    /// </summary>
    private static void SendBspBody(PCodeHttpResponse response, CommonLib.Http.HttpRequest request, byte[] body)
    {
        if (!response.Headers.ContainsKey("Content-Type"))
            response.SetHeader("Content-Type", "text/html; charset=utf-8");

        var etag = CommonLib.Http.BspRuntime.ComputeETag(body);
        response.SetHeader("ETag", etag);
        if (!response.Headers.ContainsKey("Cache-Control"))
            response.SetHeader("Cache-Control", CommonLib.Http.BspRuntime.RenderCacheTtlMs > 0
                ? "public, max-age=" + (CommonLib.Http.BspRuntime.RenderCacheTtlMs / 1000)
                : "no-cache");

        string? HeaderOf(string name) =>
            request.Headers.TryGetPropertyValue(name, out var node) ? node?.ToString() : null;

        if (CommonLib.Http.BspRuntime.ETagMatches(HeaderOf("If-None-Match"), etag))
        {
            response.SetStatus(304);
            response.SendBytes([], "text/html; charset=utf-8");
            return;
        }

        if (body.Length >= 1024 && CommonLib.Http.BspRuntime.AcceptsGzip(HeaderOf("Accept-Encoding")))
        {
            response.SetHeader("Content-Encoding", "gzip");
            response.SetHeader("Vary", "Accept-Encoding");
            response.SendBytes(CommonLib.Http.BspRuntime.Gzip(body), "text/html; charset=utf-8");
            return;
        }
        response.SendBytes(body, "text/html; charset=utf-8");
    }

    /// <summary>
    /// Tenta di servire un file statico per il path richiesto. Stessa logica dell'AST
    /// (ETag debole lunghezza+timestamp, Last-Modified RFC1123, Cache-Control 1h) MA con una
    /// guardia path-traversal che l'AST non ha (deviazione intenzionale, vedi piano Obsidian):
    /// il path risolto deve restare dentro la directory fisica registrata, altrimenti 403
    /// invece di servire un file arbitrario del filesystem.
    /// </summary>
    private async Task<bool> TryServeBspPage(CommonLib.Http.HttpRequest request, PCodeHttpResponse response, string bspDir, object?[] globalsTemplate, CancellationToken ct)
    {
        var relPath = request.Path.TrimStart('/');
        var filePath = Path.GetFullPath(Path.Combine(bspDir, relPath));
        if (!File.Exists(filePath)) return false;

        try
        {
            // PRIVACY: la resa memoizzata è globale, quindi NON si applica a richieste con
            // cookie di sessione (una pagina autenticata/personalizzata non deve mai servire il
            // corpo di un altro utente). Le pagine anonime (cruscotti locali) sì.
            var canCache = !request.Headers.ContainsKey("Cookie");
            var renderKey = filePath + "?" + (request.QueryString ?? "");

            // Resa memoizzata: se c'è, si serve subito (vale per entrambi i motori).
            var cachedBody = canCache ? CommonLib.Http.BspRuntime.RenderGet(renderKey) : null;
            if (cachedBody != null)
            {
                SendBspBody(response, request, cachedBody);
                return true;
            }

            // Motore C (opt-in): stessa pagina, sidecar A1, VM C in-process via P/Invoke.
            // ~4,7× più veloce del motore managed, ma senza lo stato managed (agenti/team/
            // workflow/memoria del server AI): chi ha bisogno di quei dati resta su "managed".
            if (CommonLib.Http.BspRuntime.UseCEngine(filePath))
            {
                // Il renderer C esegue il sidecar A1: se manca (o è più vecchio della pagina /
                // di un .inc) lo si genera qui, così il motore C non richiede una
                // precompilazione manuale. Se il sidecar non si può produrre si resta su managed.
                Basic.Pipeline.SourcePipeline.SidecarWriter ??=
                    static chunk => Basic.Runtime.ChunkA1Serializer.Serialize(chunk);
                Basic.Pipeline.SourcePipeline.EnsureBspSidecar(filePath);
                // Stato (agenti/team/workflow/memoria) al motore C: senza questo la pagina
                // renderebbe le tabelle vuote (la VM/C ha solo il proprio registro).
                BspStateProvider.EnsureRegistered();

                request.Headers.TryGetPropertyValue("Host", out var hostNode);
                if (CommonLib.Http.BspC.TryServe(bspDir, request.Path, request.QueryString,
                        hostNode?.ToString(), out var cbody, out var cstatus))
                {
                    var cbytes = System.Text.Encoding.UTF8.GetBytes(cbody);
                    if (cstatus == 200)
                    {
                        CommonLib.Http.BspRuntime.RenderPut(renderKey, cbytes);
                        SendBspBody(response, request, cbytes);
                    }
                    else
                    {
                        response.SetStatus(cstatus);
                        response.SendBytes(cbytes, "text/plain; charset=utf-8");
                    }
                    return true;
                }
            }

            var cached = CommonLib.Http.BspRuntime.CacheGet(filePath);
            if (cached == null)
            {
                Basic.Pipeline.SourcePipeline.CompileBspPage(filePath);
                cached = CommonLib.Http.BspRuntime.CacheGet(filePath);
            }
            if (cached == null) { response.CompleteAsServerError("503 BSP compilation failed."); return true; }

            var compiledChunk = new CompiledChunk();
            foreach (var b in cached.PCode) compiledChunk.Code.Add(b);
            foreach (var c in cached.Constants) compiledChunk.Constants.Add(c);
            foreach (var l in cached.Lines) compiledChunk.Lines.Add(l);
            compiledChunk.GlobalCount = cached.GlobalCount;
            // Una pagina .bsp ha SEMPRE almeno un DIM globale con inizializzatore, quindi il
            // chunk parte da un PROLOGO (__global_init poi __main), non da 0 — vedi
            // BspDispatch.ServeBspPage per la stessa nota: senza copiare EntryPoint qui il
            // task eseguiva __global_init DIRETTAMENTE (mai via OP_CALL) e al RETURN di
            // chiusura (Frame ancora null) finiva subito, senza mai raggiungere __main.
            compiledChunk.EntryPoint = cached.EntryPoint;

            using var outputWriter = new StringWriter();
            var runtime = new Basic.Runtime.HeadlessRuntimeHost(outputWriter);

            var reqDict = BuildRequestDict(request, new Dictionary<string, object?>());
            using var vm = new PCodeVm(compiledChunk, runtime);
            // Semina REQ/RES/SESSION/PAGE per NOME (mai uno slot fisso assunto — l'ordine
            // dipende da BspCompiler.Compile/il linking, vedi BspDispatch.ServeBspPage per lo
            // stesso pattern): gli slot 1-6 hardcoded qui sotto erano una convenzione STALE
            // (non corrispondevano più all'ordine reale REQ/RES/SESSION/PAGE emesso da
            // BspCompiler) — scrivevano nei globali SBAGLIATI (es. il valore di FORM finiva
            // nello slot di PAGE o di una DIM della pagina stessa come `opts`/`r`/`pkgs`),
            // corrompendo silenziosamente lo stato della pagina subito dopo il seed iniziale.
            var slots = cached.GlobalNameSlots;
            if (slots != null)
            {
                void Seed(string name, object? value)
                {
                    if (slots.TryGetValue(name, out var slot)) vm.SetGlobal(slot, value);
                }
                // SESSION: STESSO dict condiviso tra il globale SESSION e REQ("Session") —
                // le pagine possono usare entrambe le sintassi (alias 2026-09-02) e il
                // write-back post-esecuzione (che rilegge SESSION) persiste anche gli
                // aggiornamenti fatti via REQ("Session").
                var sessionData = GetOrCreateSession(request);
                reqDict["Session"] = sessionData;
                Seed("REQ", reqDict);
                Seed("RES", response);
                Seed("SESSION", sessionData);
                Seed("PAGE", new Dictionary<string, object?>());
            }
            // FORM/QUERY sono keyword (non dichiarabili con DIM, vedi BspCompiler.Compile): il
            // Linker le marca con SymbolId = DynamicScopeSymbolId (nessuno slot in GlobalNameSlots
            // — il loop sopra non le vede), risolte per NOME a runtime da OP_LOAD_DYNAMIC_SCOPE
            // (vedi PCodeProcessor.SetDynamicScope). Bug reale: `QUERY["name"]` in package.bsp
            // restituiva sempre stringa vuota (nessuna eccezione) perché prima di questo fix il
            // backend P-Code non aveva alcun meccanismo dynamic-scope equivalente a
            // IlRuntime.DynamicScopeSet — pari a REQ["Query"] (già convertito per nome da
            // BuildRequestDict), FORM resta un dict vuoto: il parsing form-urlencoded del body
            // non è implementato in nessuno dei due backend (stessa parità di BspDispatch.ServeBspPage).
            vm.SetDynamicScope("QUERY", reqDict["Query"]);
            vm.SetDynamicScope("FORM", ParseFormToDict(request));
            // REGOLA INDEROGABILE (AGENTS.md 0ter): un worker non puo' mai bloccarsi — le
            // operazioni bloccanti sono PARK-based. TryServeBspPage e' un metodo async
            // invocato dalla pipeline HTTP (AcceptLoopAsync/HandleConnectionAsync/
            // DispatchHttpRequestAsync, tutta ConfigureAwait(false)), quindi gira su un
            // thread ThreadPool .NET, non su un worker GPM. Un `vm.Run()` SINCRONO qui
            // blocca quel thread ThreadPool per l'intera durata del render (fino a 30s se
            // la pagina fa una FETCH che rientra in questo stesso server): esattamente la
            // causa del deadlock/stallo osservato con RegistryServer.bas — la FETCH
            // self-referenziale, per completare, ha bisogno che AcceptLoopAsync/
            // HandleConnectionAsync/DispatchHttpRequestAsync per la NUOVA connessione
            // (verso /api/packages) ottengano un thread ThreadPool libero, e il pool
            // fatica a rimpiazzare quello appena bloccato in tempi brevi. Fix: la VM della
            // pagina gira su un thread dedicato (non ThreadPool, come i worker
            // GOMAXPROCS di PCodeVm.Run() stesso), e il chiamante si PARCHEGGIA su un
            // TaskCompletionSource invece di bloccarsi — il thread ThreadPool torna
            // libero immediatamente.
            var pageDone = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
            var pageThread = new Thread(() =>
            {
                try { vm.Run(CancellationToken.None); pageDone.TrySetResult(); }
                catch (Exception ex) { pageDone.TrySetException(ex); }
            })
            { IsBackground = true, Name = "bsp-page-worker" };
            pageThread.Start();
            await pageDone.Task.ConfigureAwait(false);
            if (!response.Sent)
            {
                var body = System.Text.Encoding.UTF8.GetBytes(outputWriter.ToString());
                // Non memoizzare se la pagina ha creato una sessione (Set-Cookie) o se la
                // richiesta portava cookie: sarebbe una fuga di contenuto fra utenti.
                if (canCache && !response.Headers.ContainsKey("Set-Cookie"))
                    CommonLib.Http.BspRuntime.RenderPut(renderKey, body);
                SendBspBody(response, request, body);
            }
            return true;
        }
        catch (Exception ex)
        {
            response.CompleteAsServerError($"500 BSP Error: {ex.Message}");
            return true;
        }
    }

    private Dictionary<string, object?> GetOrCreateSession(CommonLib.Http.HttpRequest request)
    {
        var cookieHeader = "";
        if (request.Headers.TryGetPropertyValue("Cookie", out var cNode) && cNode != null)
            cookieHeader = cNode.ToString();
        var sessionId = ExtractCookieValue(cookieHeader, "GMBSESSION");
        if (!string.IsNullOrEmpty(sessionId))
        {
            var data = _bspSessionManager.GetSession(sessionId);
            if (data != null)
                return ConvertJsonObjectToDict(data);
        }
        var (newId, _) = _bspSessionManager.CreateSession();
        var newData = _bspSessionManager.GetSession(newId);
        return newData != null ? ConvertJsonObjectToDict(newData) : new Dictionary<string, object?>();
    }

    private static string? ExtractCookieValue(string cookieHeader, string cookieName)
    {
        if (string.IsNullOrEmpty(cookieHeader)) return null;
        var prefix = cookieName + "=";
        var idx = cookieHeader.IndexOf(prefix, StringComparison.OrdinalIgnoreCase);
        if (idx < 0) return null;
        var start = idx + prefix.Length;
        var end = cookieHeader.IndexOf(';', start);
        return end < 0 ? cookieHeader[start..].Trim() : cookieHeader[start..end].Trim();
    }

    private static Dictionary<string, object?> ConvertJsonObjectToDict(System.Text.Json.Nodes.JsonObject obj)
    {
        var dict = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in obj) dict[kv.Key] = JsonNodeToString(kv.Value);
        return dict;
    }

    /// <summary>
    /// Popola FORM dal body SOLO per richieste application/x-www-form-urlencoded (mai per body
    /// JSON o GET). Implementazione UNICA condivisa (allineamento 2026-09-02): delega a
    /// CommonLib.Http.HttpRequest.ParseFormDictionary → RequestBodyParser.ParseFormDictionary.
    /// </summary>
    private static Dictionary<string, object?> ParseFormToDict(CommonLib.Http.HttpRequest request)
        => request.ParseFormDictionary();

    private static bool TryServeStaticFile(PCodeHttpRouter router, string requestPath, PCodeHttpResponse response)
    {
        var resolved = router.ResolveStaticPath(requestPath);
        if (resolved == null) return false;
        var (urlPrefix, physicalBase) = resolved.Value;

        var relative = requestPath[urlPrefix.Length..].TrimStart('/');
        var fullPath = Path.GetFullPath(Path.Combine(physicalBase, relative));

        var baseWithSep = physicalBase.EndsWith(Path.DirectorySeparatorChar) ? physicalBase : physicalBase + Path.DirectorySeparatorChar;
        if (!fullPath.Equals(physicalBase, StringComparison.Ordinal) && !fullPath.StartsWith(baseWithSep, StringComparison.Ordinal))
        {
            response.SetStatus(403);
            response.Send("403 Forbidden");
            return true;
        }

        if (!File.Exists(fullPath)) { response.CompleteAsNotFound(); return true; }

        var info = new FileInfo(fullPath);
        var etag = $"\"{info.Length:x}-{info.LastWriteTimeUtc.Ticks:x}\"";
        response.SetHeader("ETag", etag);
        response.SetHeader("Last-Modified", info.LastWriteTimeUtc.ToString("R"));
        response.SetHeader("Cache-Control", "public, max-age=3600");
        var ext = Path.GetExtension(fullPath);
        var contentType = HttpStaticMimeTypes.TryGetValue(ext, out var ct) ? ct : "application/octet-stream";
        response.SendBytes(File.ReadAllBytes(fullPath), contentType);
        return true;
    }

    /// <summary>
    /// Dispatch di una richiesta HTTP completa verso il codice BASIC. Instrada tramite il
    /// router, spawna il PCodeTask handler (stesso meccanismo di LaunchGoTask/OP_GO_FUNCTION —
    /// vedi SpawnHttpHandlerTask), attende il completamento (segnalato da
    /// PCodeHttpResponse.Sent, non da un evento dedicato — vedi PCodeHttpResponse.OnSent) con il
    /// timeout configurato, e converte in 500/504 i casi limite (eccezione non gestita nel
    /// task, o nessuna risposta mai inviata) — deviazione intenzionale rispetto all'AST, che in
    /// questi casi resta appeso per sempre (vedi piano Obsidian PCode-HTTP-Server-Plan.md).
    /// </summary>
    private async Task<bool> DispatchHttpRequestAsync(CommonLib.Http.HttpRequest request, PCodeHttpResponse response, Stream connectionStream, CancellationToken ct)
    {
        var router = _httpRouter;
        var globalsTemplate = _httpGlobalsTemplate;
        var config = _httpConfig ?? new HttpConfig();
        if (router == null || globalsTemplate == null || _scheduler == null)
        {
            response.CompleteAsServerError("500 Internal Server Error: HTTP server non completamente inizializzato");
            return false;
        }

        var rateLimiter = _httpRateLimiter;
        if (rateLimiter != null && !rateLimiter.IsAllowed(request.RemoteIpAddress))
        {
            response.SetStatus(429);
            response.Send("429 Too Many Requests");
            return false;
        }

        // BSP — serve pagine .bsp da cache
        if (_httpBspDir != null && request.Path.EndsWith(".bsp", StringComparison.OrdinalIgnoreCase))
        {
            var served = await TryServeBspPage(request, response, _httpBspDir, globalsTemplate, ct).ConfigureAwait(false);
            if (served) return false;
        }

        var match = router.Match(request.Method, request.Path);
        if (match == null)
        {
            // Stessa priorita' dell'AST: i file statici sono controllati SOLO quando nessuna
            // route/fallback ha risposto, mai prima.
            if (!TryServeStaticFile(router, request.Path, response)) response.CompleteAsNotFound();
            return false;
        }

        if (IsWebSocketUpgradeRequest(request))
        {
            // L'intera sessione (handshake + loop di ricezione) viene gestita QUI, per intero,
            // prima di tornare — il chiamante (PCodeHttpServer) si limita a chiudere la
            // connessione dopo, senza scrivere una risposta HTTP normale (vedi il ritorno bool).
            await HandleWebSocketUpgradeAsync(match.Value, request, connectionStream, globalsTemplate, ct).ConfigureAwait(false);
            return true;
        }

        var reqDict = BuildRequestDict(request, match.Value.Parameters);

        // Sessioni route HTTP (HTTP CONFIG .Sessions): REQ("Session") = JsonObject VIVO; il
        // Set-Cookie GMBSESSION va aggiunto SOLO alla prima richiesta (nuova sessione).
        if (config.Sessions)
        {
            var cookieHeader = GetHeaderCaseInsensitive(request, "Cookie") ?? "";
            var (sessionSetCookie, sessionData) = CommonLib.Http.HttpSessions.Ensure(cookieHeader);
            reqDict["Session"] = sessionData;
            if (sessionSetCookie != null)
                response.RawSetCookieHeaders.Add(sessionSetCookie);
        }

        // Catena completa = middleware (in ordine di registrazione) + handler finale in coda,
        // esattamente come nell'AST (routeMatch.MiddlewareChain seguita dalla route finale nella
        // stessa Queue — vedi audit ExecuteHttpConnectionHandlerLoop). Caso comune (nessun
        // middleware sul path): un solo passo, firma a 2 argomenti (req, res), invariato dalla
        // Fase B. Con middleware: TUTTI i passi (inclusa la route finale) ricevono 3 argomenti
        // (req, res, next) — per l'ultimo passo "next" ha semplicemente una coda vuota, quindi
        // HTTP USE NEXT su di esso e' un no-op che termina solo il task corrente.
        var chainState = new Basic.IlBackend.PCode.Http.PCodeHttpChainState { CurrentTask = null! };
        if (match.Value.MiddlewareChain.Count == 0)
        {
            chainState.CurrentTask = SpawnHttpHandlerTask(globalsTemplate, match.Value.HandlerIp, [reqDict, response]);
        }
        else
        {
            var remaining = new Queue<int>(match.Value.MiddlewareChain);
            remaining.Enqueue(match.Value.HandlerIp);
            var firstIp = remaining.Dequeue();
            var nextHandle = new Basic.IlBackend.PCode.Http.PCodeHttpNextHandle
            { Remaining = remaining, RequestValue = reqDict, Response = response, ChainState = chainState };
            chainState.CurrentTask = SpawnHttpHandlerTask(globalsTemplate, firstIp, [reqDict, response, nextHandle]);
        }

        var tcs = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        response.OnSent(() => tcs.TrySetResult());
        _ = Task.Run(async () =>
        {
            try
            {
                while (!tcs.Task.IsCompleted && !ct.IsCancellationRequested)
                {
                    // Legge sempre il task ATTUALMENTE attivo della catena — ExecuteHttpUseNext
                    // aggiorna chainState.CurrentTask a ogni passo, cosi' un'eccezione in un
                    // middleware intermedio (non solo nel primo/ultimo) viene rilevata senza
                    // aspettare l'intero timeout di richiesta.
                    var active = chainState.CurrentTask;
                    if (active is { IsFinished: true, PendingException: not null }) { tcs.TrySetResult(); return; }
                    await Task.Delay(15, ct).ConfigureAwait(false);
                }
            }
            catch (OperationCanceledException) { }
        }, ct);

        try { await tcs.Task.WaitAsync(TimeSpan.FromMilliseconds(config.RequestTimeoutMs), ct).ConfigureAwait(false); }
        catch (TimeoutException) { }
        catch (OperationCanceledException) { }

        if (!response.Sent)
        {
            var active = chainState.CurrentTask;
            if (active?.PendingException != null)
                response.CompleteAsServerError("500 Internal Server Error: " + active.PendingException.Message.Split('\n')[0]);
            else
                response.CompleteAsTimeout();
        }
        return false;
    }

    /// <summary>Cerca un header per nome in modo case-INsensitive. <see cref="CommonLib.Http.HttpRequest"/>.Headers
    /// e' un JsonObject costruito da HttpRequestParser con le chiavi ESATTAMENTE come arrivano sul
    /// filo (CommonLib/Http/RequestParser.cs: <c>_headers[parts[0].Trim()] = ...</c>, nessuna
    /// normalizzazione), e JsonObject.TryGetPropertyValue confronta le chiavi con
    /// StringComparer.Ordinal (case-sensitive) in assenza di JsonNodeOptions espliciti — un lookup
    /// diretto con un nome canonico tipo "Sec-WebSocket-Key" fallisce silenziosamente per QUALUNQUE
    /// client che invia i nomi header in minuscolo, prassi comune e pienamente conforme a HTTP
    /// (i nomi header sono case-insensitive per specifica) — es. il WebSocket nativo di Node.js
    /// (v22+, basato su undici) invia sempre "sec-websocket-key"/"upgrade"/"connection" in
    /// minuscolo (verificato catturando la richiesta grezza byte-per-byte). Bug reale trovato
    /// verificando BUG 25 con un client rigoroso: un test manuale con header scritti a mano nel
    /// case canonico "funzionava" per puro caso, mascherando che QUALUNQUE client reale (Node,
    /// probabilmente molti altri) veniva silenziosamente RIFIUTATO — IsWebSocketUpgradeRequest
    /// restituiva false (niente branch WebSocket, fallback a dispatch HTTP normale, niente 101
    /// mai inviato) e ComputeWebSocketAcceptKey calcolava l'accept su una Sec-WebSocket-Key vuota
    /// anche nei rari casi in cui il primo controllo passava. Vedi TODO-PCODE-BUGS.md BUG 25.</summary>
    private static string? GetHeaderCaseInsensitive(CommonLib.Http.HttpRequest request, string name)
    {
        foreach (var kv in request.Headers)
        {
            if (string.Equals(kv.Key, name, StringComparison.OrdinalIgnoreCase))
                return kv.Value?.GetValue<string>();
        }
        return null;
    }

    private static bool IsWebSocketUpgradeRequest(CommonLib.Http.HttpRequest request)
    {
        var upgrade = GetHeaderCaseInsensitive(request, "Upgrade");
        var connection = GetHeaderCaseInsensitive(request, "Connection");
        return upgrade != null && upgrade.Contains("websocket", StringComparison.OrdinalIgnoreCase)
            && connection != null && connection.Contains("Upgrade", StringComparison.OrdinalIgnoreCase);
    }

    // GUID RFC6455 corretto: "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" — verificato tramite
    // decompilazione IL di System.Net.WebSockets.HttpWebSocket.GetSecWebSocketAcceptString
    // (ilspycmd su System.Net.HttpListener.dll). Una versione con l'ultimo gruppo scambiato
    // ("5AB9DC11B85B" invece di "C5AB0DC85B11") era presente qui (ereditata dal porting da
    // GMBasicAI prima della scoperta del bug — vedi GMBasicAI TODO-PCODE-BUGS.md BUG 24),
    // producendo un Sec-WebSocket-Accept sempre sbagliato, rifiutato da qualunque client
    // WebSocket standard/rigoroso (System.Net.WebSockets.ClientWebSocket, il WebSocket nativo
    // di Node.js). Mai notato perche' nessun client WebSocket rigoroso e' mai stato usato per
    // testare il server P-CODE di GMBasic.
    private const string WebSocketMagicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    private static string ComputeWebSocketAcceptKey(CommonLib.Http.HttpRequest request)
    {
        var key = GetHeaderCaseInsensitive(request, "Sec-WebSocket-Key") ?? "";
        var combined = key + WebSocketMagicGuid;
        var hash = System.Security.Cryptography.SHA1.HashData(System.Text.Encoding.UTF8.GetBytes(combined));
        return Convert.ToBase64String(hash);
    }

    /// <summary>
    /// WEBSOCKET CONNECT (client) — WS_CONNECT, Fase 5 FETCH/WebSocket avanzato (porting da
    /// GMBasicAI). A differenza dell'AST (che usa ancora l'handshake scritto a mano sopra
    /// HighPerformance, l'ex stack libuv ora su socket .NET puri — vedi Interpreter/
    /// Gpm.Network.cs), qui si usa direttamente System.Net.WebSockets.ClientWebSocket, stesso
    /// principio "niente frame-decoder a mano" gia' adottato per il lato server (vedi
    /// HandleWebSocketUpgradeAsync sotto). PCodeWebSocketHandle avvolge indifferentemente un
    /// WebSocket lato server (WebSocket.CreateFromStream) o lato client (ClientWebSocket, che
    /// eredita da WebSocket) — nessuna classe nuova necessaria. Prima di questo fix,
    /// WebSocketConnectStmt passava per EmitExprAndPop (riflessione generica sulle proprieta'
    /// Expr del record — vedi commento su NetSubOpCode.WS_CONNECT): Host/Port/Path/Timeout
    /// venivano valutati e scartati, Handle (di tipo Target, non Expr) veniva ignorato del
    /// tutto — WEBSOCKET CONNECT era quindi un no-op silenzioso nella VM, la variabile ws
    /// restava sempre nil.
    /// </summary>
}
