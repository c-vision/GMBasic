// FILE: Basic.IlBackend.PCode.Http.PCodeHttpRouter.cs
// Router HTTP per la P-CODE VM — porta la logica di matching di
// Interpreter/HTTP/HttpServerManager.Router.cs (path template con parametri {nome}, priorita',
// fallback) ma con l'identita' del callback come indirizzo Ip (int) invece di nome stringa,
// coerente con come HTTP_ROUTE viene gia' compilato (CompileProcedureToCodeRef/OP_PUSH_CODE_REF
// risolvono l'handler a un indirizzo assoluto a compile-time, mai un nome runtime).

using System.Text.RegularExpressions;

namespace Basic.IlBackend.PCode.Http;

/// <summary>Una singola route registrata con HTTP ROUTE: metodo + path template + indirizzo handler.</summary>
internal sealed class PCodeHttpRoute
{
    public string Method { get; }
    public string PathTemplate { get; }
    public int HandlerIp { get; }
    public int Priority { get; }

    private readonly Regex _regex;
    private readonly List<string> _paramNames = new();

    public PCodeHttpRoute(string method, string pathTemplate, int handlerIp, int priority = 0)
    {
        Method = string.IsNullOrEmpty(method) ? "*" : method.ToUpperInvariant();
        PathTemplate = pathTemplate;
        HandlerIp = handlerIp;
        Priority = priority;

        // Regex.Escape trasforma "{id}" in "\{id}" — SOLO la graffa aperta viene escaped da
        // .NET (verificato empiricamente: Regex.Escape non tocca "}"), quindi il replace deve
        // cercare "\{nome}" (chiusa NON escaped), non "\{nome\}" come una prima versione
        // assumeva per analogia con altri motori regex — con quel pattern il placeholder non
        // veniva mai riconosciuto e ogni route con parametro falliva sempre il match.
        var pattern = Regex.Replace(Regex.Escape(pathTemplate), @"\\\{([a-zA-Z_][a-zA-Z0-9_]*)\}",
            m => { _paramNames.Add(m.Groups[1].Value); return "([^/]+)"; });
        _regex = new Regex("^" + pattern + "/?$", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    }

    public bool MatchesMethod(string method) => Method == "*" || Method.Equals(method, StringComparison.OrdinalIgnoreCase);

    public bool MatchesPath(string path) => _regex.IsMatch(path);

    public bool TryMatch(string path, out Dictionary<string, object?> parameters)
    {
        parameters = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        var m = _regex.Match(path);
        if (!m.Success) return false;
        for (var i = 0; i < _paramNames.Count; i++)
            parameters[_paramNames[i]] = Uri.UnescapeDataString(m.Groups[i + 1].Value);
        return true;
    }
}

/// <summary>Un middleware registrato con HTTP USE: metodo (sempre "*" nella sintassi legacy) +
/// prefisso di path (match per prefisso, non template — stessa semantica dell'AST) + indirizzo.</summary>
internal sealed class PCodeHttpMiddleware
{
    public string Method { get; }
    public string PathPrefix { get; }
    public int HandlerIp { get; }

    public PCodeHttpMiddleware(string method, string pathPrefix, int handlerIp)
    {
        Method = string.IsNullOrEmpty(method) ? "*" : method.ToUpperInvariant();
        PathPrefix = string.IsNullOrEmpty(pathPrefix) ? "/" : pathPrefix;
        HandlerIp = handlerIp;
    }

    public bool Matches(string method, string path) =>
        (Method == "*" || Method.Equals(method, StringComparison.OrdinalIgnoreCase)) &&
        path.StartsWith(PathPrefix, StringComparison.OrdinalIgnoreCase);
}

/// <summary>Riferimento mutabile al task attualmente attivo per una catena middleware→handler —
/// aggiornato da ExecuteHttpUseNext ogni volta che HTTP USE NEXT fa avanzare la catena, letto dal
/// loop di attesa in DispatchHttpRequestAsync per rilevare un'eccezione non gestita nel task
/// CORRENTE (non solo nel primo) senza dover aspettare l'intero timeout di richiesta.</summary>
internal sealed class PCodeHttpChainState
{
    public required Basic.Runtime.Gpm.PCodeTask CurrentTask { get; set; }
}

/// <summary>Handle passato come 3° argomento (next) a middleware/handler quando la catena ha piu'
/// di un passo — HTTP USE NEXT lo consuma per spawnare il passo successivo (vedi
/// PCodeProcessor.ExecuteHttpUseNext).</summary>
internal sealed class PCodeHttpNextHandle
{
    public required Queue<int> Remaining { get; init; }
    public required object? RequestValue { get; init; }
    public required PCodeHttpResponse Response { get; init; }
    public required PCodeHttpChainState ChainState { get; init; }
}

/// <summary>Esito di un match: catena middleware (in ordine di registrazione) + route finale +
/// parametri estratti (path template, fusi con la query string dal chiamante).</summary>
internal readonly struct PCodeHttpRouteMatch(IReadOnlyList<int> middlewareChain, int handlerIp, Dictionary<string, object?> parameters)
{
    public IReadOnlyList<int> MiddlewareChain { get; } = middlewareChain;
    public int HandlerIp { get; } = handlerIp;
    public Dictionary<string, object?> Parameters { get; } = parameters;
}

/// <summary>
/// Router HTTP: proprio lock interno per le mutazioni (AddRoute/AddMiddleware/SetFallback),
/// MAI per l'intera pipeline di dispatch delle richieste — quella gira in parallelo su piu'
/// connessioni per design (vedi piano Obsidian, filosofia "lock per sottosistema, non per
/// richiesta" gia' adottata per i lock database in GM-Basic).
/// </summary>
internal sealed class PCodeHttpRouter
{
    private readonly object _lock = new();
    private readonly List<PCodeHttpRoute> _routes = new();
    private readonly List<PCodeHttpMiddleware> _middlewares = new();

    public int? FallbackHandlerIp { get; set; }

    public void AddRoute(string method, string path, int handlerIp, int priority = 0)
    {
        lock (_lock)
        {
            _routes.RemoveAll(r => r.Method.Equals(method, StringComparison.OrdinalIgnoreCase) &&
                                    r.PathTemplate.Equals(path, StringComparison.OrdinalIgnoreCase));
            _routes.Add(new PCodeHttpRoute(method, path, handlerIp, priority));
            _routes.Sort((a, b) => b.Priority.CompareTo(a.Priority));
        }
    }

    public void AddMiddleware(string method, string pathPrefix, int handlerIp)
    {
        lock (_lock) _middlewares.Add(new PCodeHttpMiddleware(method, pathPrefix, handlerIp));
    }

    private readonly List<(string UrlPrefix, string PhysicalPath)> _staticPaths = new();

    public void RegisterStaticPath(string urlPrefix, string physicalPath)
    {
        lock (_lock) _staticPaths.Add((urlPrefix, physicalPath));
    }

    /// <summary>Mount statico vincente: il prefisso PIÙ LUNGO che combacia (a parità, il primo
    /// registrato). Con la sola regola "primo che matcha" un mount generico come "/" oscurava
    /// sempre quelli specifici tipo "/static" — l'app serviva le pagine ma non i suoi asset.</summary>
    public (string UrlPrefix, string PhysicalPath)? ResolveStaticPath(string path)
    {
        List<(string, string)> snapshot;
        lock (_lock) snapshot = new List<(string, string)>(_staticPaths);
        (string, string)? best = null;
        foreach (var sp in snapshot)
        {
            if (!path.StartsWith(sp.Item1, StringComparison.OrdinalIgnoreCase)) continue;
            if (best is null || sp.Item1.Length > best.Value.Item1.Length) best = sp;
        }
        return best;
    }

    public void Clear()
    {
        lock (_lock) { _routes.Clear(); _middlewares.Clear(); _staticPaths.Clear(); FallbackHandlerIp = null; }
    }

    /// <summary>Trova la route (o il fallback) per una richiesta e costruisce la catena middleware
    /// che deve precederla — replica la semantica dell'AST: le route esplicite hanno sempre
    /// precedenza sul fallback (HTTP SERVER porta, callback), il fallback riceve solo cio' che
    /// nessuna route copre.</summary>
    public PCodeHttpRouteMatch? Match(string method, string path)
    {
        List<PCodeHttpRoute> routesSnapshot;
        List<PCodeHttpMiddleware> middlewaresSnapshot;
        int? fallback;
        lock (_lock)
        {
            routesSnapshot = new List<PCodeHttpRoute>(_routes);
            middlewaresSnapshot = new List<PCodeHttpMiddleware>(_middlewares);
            fallback = FallbackHandlerIp;
        }

        int handlerIp;
        Dictionary<string, object?> parameters;
        var matched = false;
        foreach (var route in routesSnapshot)
        {
            if (!route.MatchesMethod(method)) continue;
            if (!route.TryMatch(path, out parameters)) continue;
            handlerIp = route.HandlerIp;
            matched = true;
            var chain = middlewaresSnapshot.Where(m => m.Matches(method, path)).Select(m => m.HandlerIp).ToList();
            return new PCodeHttpRouteMatch(chain, handlerIp, parameters);
        }
        if (!matched && fallback.HasValue)
        {
            var chain = middlewaresSnapshot.Where(m => m.Matches(method, path)).Select(m => m.HandlerIp).ToList();
            return new PCodeHttpRouteMatch(chain, fallback.Value, new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase));
        }
        return null;
    }
}
