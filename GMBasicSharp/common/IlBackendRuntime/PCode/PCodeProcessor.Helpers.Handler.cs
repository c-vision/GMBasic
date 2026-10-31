// FILE: IlBackendRuntime/PCode/PCodeProcessor.Helpers.Handler.cs
// PARTIAL — dominio HELPERS/HANDLER: spawn task HTTP handler, dispatch eventi/audio/clip.
// Migrato da PCodeProcessor.Helpers.Ws.cs (2026-08-30 — split, file <= 1200 righe).

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
    private PCodeTask SpawnHttpHandlerTask(object?[] globalsTemplate, int handlerIp, object?[] args)
    {
        var task = _scheduler!.CreateTask(handlerIp);
        task.Scheduler = _scheduler;
        task.Globals = globalsTemplate;
        // ATTENZIONE: qui "args" e' gia' in ordine di dichiarazione (args[0]=primo parametro,
        // come per OP_NET_CMD — vedi ExecuteHttpRoute/ExecuteHttpServer), NON nell'ordine
        // "inverso" che LaunchGoTask usa per il proprio array locale (li' args[0] e' l'ULTIMO
        // parametro, perche' costruito scorrendo lo stack del chiamante dall'alto verso il
        // basso). Il prologo compilato (Compiler.cs, righe 619-625) fa STORE_LOCAL partendo
        // dall'ULTIMO parametro, quindi consuma per primo cio' che sta in CIMA allo stack: per
        // avere l'ultimo parametro in cima serve un push in ordine FORWARD (non reverse-index
        // come LaunchGoTask) quando, come qui, si parte gia' da un array in ordine dichiarativo.
        // Bug reale trovato in test: con push reverse args[0] (req) finiva in cima e veniva
        // legato a "res" (l'ultimo parametro) — req/res scambiati, StatusCode/Send finivano
        // sull'oggetto sbagliato.
        //
        // Qui "args" ha SEMPRE la forma fissa massima per il tipo di dispatch (es. 3 elementi
        // per una catena middleware: req/res/next), indipendentemente da quanti parametri la SUB
        // di destinazione dichiari davvero. Se l'handler dichiara MENO parametri (es. 2), il
        // prologo compilato fa comunque STORE_LOCAL solo per i parametri dichiarati, partendo
        // dalla cima dello stack: senza troncare "args" qui, l'ultimo valore push-ato in eccesso
        // (es. il next-handle) finirebbe legato all'ultimo parametro dichiarato (es. "res") al
        // posto del vero valore, disallineando tutto in silenzio — nessuna eccezione, nessun
        // errore visibile, solo un binding sbagliato (bug reale, vedi TODO-PCODE-BUGS.md).
        // ProcedureArities (CompiledChunk, popolato da Compiler._procedureArities) espone
        // l'arita' reale dichiarata per indirizzo Ip: se nota ed e' minore della shape fissa,
        // tronchiamo "args" ai soli parametri che la SUB si aspetta davvero.
        //
        // Prima di troncare: se fra gli argomenti c'e' un next-handle (dispatch di un passo della
        // catena middleware), lo registriamo SEMPRE in _httpTaskNextHandle per questo task, anche
        // se la SUB dichiara meno di 3 parametri e quindi il next-handle verra' troncato via sotto
        // e mai legato a nessuna variabile BASIC — serve come fallback per ExecuteHttpUseNext
        // quando la sintassi implicita "HTTP USE NEXT" (nessun parametro "next" dichiarato) risolve
        // a runtime in NIL invece che nel vero handle (vedi ExecuteHttpUseNext).
        foreach (var a in args)
            if (a is Basic.IlBackend.PCode.Http.PCodeHttpNextHandle nh) { _httpTaskNextHandle.AddOrUpdate(task, nh); break; }
        if (_procedureArities.TryGetValue(handlerIp, out var arity) && arity < args.Length)
            args = args[..arity];
        foreach (var a in args) task.PushRef(a);
        _scheduler.Schedule(task);
        return task;
    }

    /// <summary>Legge un campo booleano da un HTTPOPTIONS P-CODE (Dictionary). I letterali
    /// TRUE/FALSE della VM sono rappresentati come double (-1.0/0.0, convenzione BASIC classica —
    /// vedi OP_PUSH_TRUE/OP_PUSH_FALSE), NON come System.Boolean: un valore assegnato da codice
    /// BASIC (es. opts.AllowRedirects = FALSE) arriva qui come double, mentre i default
    /// precompilati in OP_NEW typeCode 22 sono C# bool letterali — questo helper accetta
    /// entrambe le rappresentazioni per non perdere in silenzio un'assegnazione BASIC esplicita
    /// (bug reale, trovato in test: un check "is bool" da solo ignorava FALSE assegnato da BASIC,
    /// perche' arrivava come double 0.0, non bool false).</summary>
    private static bool TryGetPCodeBool(System.Collections.IDictionary dict, string key, out bool value)
    {
        value = false;
        if (!dict.Contains(key)) return false;
        switch (dict[key])
        {
            case bool b: value = b; return true;
            case double d: value = d != 0; return true;
            default: return false;
        }
    }

    /// <summary>Converte l'oggetto HTTPOPTIONS della P-CODE VM (un Dictionary&lt;string,object?&gt;
    /// popolato da OP_NEW typeCode 22, vedi quel case) in un CommonLib.Http.HttpOptions reale,
    /// consumato da HttpClientHelper.PerformRequestAsync — stesso client usato dall'AST executor.
    /// null/qualunque valore non-Dictionary produce le HttpOptions di default (equivalente a
    /// FETCH(url) senza opzioni).</summary>
    private static CommonLib.Http.HttpOptions ConvertPCodeHttpOptions(object? optionsRef)
    {
        var options = new CommonLib.Http.HttpOptions();
        if (optionsRef is not System.Collections.IDictionary dict) return options;

        if (dict.Contains("Method") && dict["Method"] is string method && !string.IsNullOrEmpty(method))
            options.Method = method;
        if (dict.Contains("Body"))
            options.Body = dict["Body"];
        if (dict.Contains("Timeout") && dict["Timeout"] is double timeout && timeout > 0)
            options.Timeout = (int)timeout;
        if (TryGetPCodeBool(dict, "UseCookies", out var useCookies))
            options.UseCookies = useCookies;
        if (dict.Contains("CookieJar") && dict["CookieJar"] is CommonLib.Http.CookieJar cookieJar)
            options.CookieJar = cookieJar;
        if (dict.Contains("Username") && dict["Username"] is string username && !string.IsNullOrEmpty(username))
            options.Username = username;
        if (dict.Contains("Password") && dict["Password"] is string password)
            options.Password = password;
        if (dict.Contains("BearerToken") && dict["BearerToken"] is string bearerToken && !string.IsNullOrEmpty(bearerToken))
            options.BearerToken = bearerToken;
        if (dict.Contains("ProxyUrl") && dict["ProxyUrl"] is string proxyUrl && !string.IsNullOrEmpty(proxyUrl))
            options.ProxyUrl = proxyUrl;
        if (dict.Contains("ProxyUsername") && dict["ProxyUsername"] is string proxyUser)
            options.ProxyUsername = proxyUser;
        if (dict.Contains("ProxyPassword") && dict["ProxyPassword"] is string proxyPass)
            options.ProxyPassword = proxyPass;
        if (dict.Contains("ClientCertificatePath") && dict["ClientCertificatePath"] is string certPath && !string.IsNullOrEmpty(certPath))
            options.ClientCertificatePath = certPath;
        if (dict.Contains("ClientCertificatePassword") && dict["ClientCertificatePassword"] is string certPass)
            options.ClientCertificatePassword = certPass;
        if (TryGetPCodeBool(dict, "AllowRedirects", out var allowRedirects))
            options.AllowRedirects = allowRedirects;
        if (dict.Contains("MaxRedirects") && dict["MaxRedirects"] is double maxRedirects && maxRedirects > 0)
            options.MaxRedirects = (int)maxRedirects;
        if (TryGetPCodeBool(dict, "AllowInsecureSSL", out var allowInsecure))
            options.AllowInsecureSSL = allowInsecure;
        if (dict.Contains("BrowserPreset") && dict["BrowserPreset"] is string browserPreset && !string.IsNullOrEmpty(browserPreset))
            options.BrowserPreset = browserPreset;
        if (dict.Contains("PinnedCertificateSha256") && dict["PinnedCertificateSha256"] is string pin && !string.IsNullOrEmpty(pin))
            options.PinnedCertificateSha256 = pin;
        if (dict.Contains("Headers") && dict["Headers"] is System.Collections.IDictionary headersDict)
        {
            foreach (System.Collections.DictionaryEntry entry in headersDict)
            {
                var key = entry.Key?.ToString();
                var value = entry.Value?.ToString();
                if (string.IsNullOrEmpty(key) || value is null) continue;
                // Le proprieta' tipizzate di CommonLib.Http.HttpHeaders usano il nome C# (senza
                // trattino), non il vero nome dell'header HTTP — stessa mappatura fatta dall'AST
                // via reflection su HttpHeaders (Gpm.Expressions.cs).
                options.Headers.SetHeader(CanonicalHttpRequestHeaderName(key), value);
            }
        }
        return options;
    }

    /// <summary>Nome C# della proprieta' header (senza trattino) → vero nome HTTP. UNICA
    /// sorgente condivisa: usata da PCode (ConvertPCodeHttpOptions) e IL (IlRuntime.BuildHttpOptions),
    /// che modellano entrambi opts.Headers come dict duck-typed.</summary>
    internal static string CanonicalHttpRequestHeaderName(string key) => key switch
    {
        "ContentType" => "Content-Type",
        "UserAgent" => "User-Agent",
        _ => key
    };

    /// <summary>Semantica canonica "BodyJson" (unica sorgente managed, AGENTS §0bis): il body è
    /// parsato come JSON SOLO se Content-Type contiene "application/json" e non è vuoto; null in
    /// caso contrario o su JSON malformato. Chiamata da BuildRequestDict (PCodeVM) e da
    /// IlRuntime.HttpRequestToDict (backend IL) — i due consumer gestiti.</summary>
    internal static object? ComputeBodyJson(string? contentType, string? body)
    {
        if (contentType is null || body is null) return null;
        if (!contentType.Contains("application/json", StringComparison.OrdinalIgnoreCase)) return null;
        if (string.IsNullOrWhiteSpace(body)) return null;
        try { return JsonElementToObject(System.Text.Json.JsonDocument.Parse(body).RootElement); }
        catch { return null; }
    }

    /// <summary>Default di NEW HTTPCONFIG: UNICA sorgente managed, usata sia dal PCodeVM
    /// (OP_NEW typeCode 23) sia dal backend IL (via IlRuntime.NewHttpConfigDict). Chiavi e
    /// valori rispecchiano CommonLib.Http.HttpConfig. "DefaultRequestTimeout" è un alias SOLO
    /// dell'oggetto C# (getter→RequestTimeoutMs): nel modello duck-typed a dict non esiste
    /// sincronizzazione, quindi è seminato come chiave propria con lo stesso default — la
    /// parità è garantita dalle gambe C (gmb_httpconfig_seed semina identica).</summary>
    internal static Dictionary<string, object?> NewHttpConfigDefaults()
        => new(StringComparer.OrdinalIgnoreCase)
        {
            ["Port"] = 8080.0,
            ["CertificatePath"] = null,
            ["CertificatePassword"] = null,
            ["RequestTimeoutMs"] = 30000.0,
            ["DefaultRequestTimeout"] = 30000.0,
            ["MaxRequestBodySize"] = 10485760.0,
            ["RateLimitRequestsPerMinute"] = 60.0,
            ["RateLimitBurstSize"] = 120.0,
            ["BspCacheMs"] = 0.0,   // memoizzazione resa BSP (ms); 0 = disattivata
            ["BspEngine"] = "managed",   // "managed" | "c" (VM C, opt-in)
            ["BspEnginePages"] = "",     // pagine da servire con il motore C (es. "canvas.bsp,wasm.bsp")
            ["EnableKeepAlive"] = true,
        };

    /// <summary>Converte una HttpRequest (CommonLib, JsonObject-based) nel Dictionary che la VM
    /// gia' tratta come oggetto BASIC-accessibile — nessun tipo dedicato necessario.</summary>
    private static Dictionary<string, object?> BuildRequestDict(CommonLib.Http.HttpRequest request, Dictionary<string, object?> routeParams)
    {
        var headers = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in request.Headers) headers[kv.Key] = JsonNodeToString(kv.Value);

        var parameters = new Dictionary<string, object?>(routeParams, StringComparer.OrdinalIgnoreCase);
        var query = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in request.Query)
        {
            var val = JsonNodeToString(kv.Value);
            parameters["query." + kv.Key] = val;
            query[kv.Key] = val;
        }

        object? bodyJson = ComputeBodyJson(
            headers.TryGetValue("Content-Type", out var ct) ? ct?.ToString() : null, request.Body);

        return new Dictionary<string, object?>
        {
            ["Method"] = request.Method,
            ["Path"] = request.Path,
            ["Headers"] = headers,
            ["Params"] = parameters,
            ["Query"] = query,
            ["QueryString"] = request.QueryString,
            ["Body"] = request.Body,
            ["BodyJson"] = bodyJson,
            ["RemoteIp"] = request.RemoteIpAddress,
            ["SessionId"] = ParseSessionIdCookie(headers),
            // Istanza reale CommonLib.Http.HttpRequest: permette i metodi BASIC della
            // HTTPREQUEST (req.ParseForm()/req.ParseMultipartForm()) che OP_CALL_MEMBER
            // dispatcha per nome (prima erano inaccessibili perché il PCodeVM semina REQ
            // come Dictionary piatto, quindi req.ParseForm() non esisteva).
            ["__httpRequest"] = request,
        };
    }

    private static string? JsonNodeToString(System.Text.Json.Nodes.JsonNode? node)
    {
        if (node == null) return null;
        try { return node.GetValue<string>(); } catch { return node.ToString(); }
    }

    /// <summary>UNICA sorgente managed (AGENTS §0bis) dei metodi BASIC della HTTPREQUEST
    /// seminata come Dictionary (req): l'istanza reale vive in "__httpRequest" (seminata da
    /// BuildRequestDict per PCodeVM e da IlRuntime.HttpRequestToDict per backend IL).
    /// Ritorna true SOLO se il dict è una req HTTPREQUEST: in tal caso ogni metodo non
    /// riconosciuto vale null (semantica PCode originale intatta). Consumatori:
    /// OP_CALL_MEMBER PCodeVM e IlRuntime.CollectionInvoke (parità gestita).</summary>
    internal static bool PCodeHttpRequestDictInvoke(System.Collections.IDictionary reqDict, string methodName, int argCount, out object? result)
    {
        result = null;
        // Il probe NON deve assumere che le chiavi siano stringhe: su una collection TIPIZZATA
        // materializzata come SortedDictionary (TREE/AVL/REDBLACK con chiavi numeriche) il
        // comparer e' String.CompareTo(object) e cercare "__httpRequest" lancia
        // ArgumentException ("Object must be of type String") PRIMA di qualunque metodo.
        // Un dict che non e' una req HTTPREQUEST risponde false, com'e' dovuto.
        object? reqProbe;
        try { reqProbe = reqDict["__httpRequest"]; }
        catch (Exception) { return false; }
        if (reqProbe is not CommonLib.Http.HttpRequest httpReqInst) return false;
        if (methodName == "ParseForm" && argCount == 0)
        {
            var form = httpReqInst.ParseForm();
            if (form is null) return true;
            // Stesso formato di JSON.PARSE (JsonElementToObject): l'accesso campo
            // (form.nome) e gli indici funzionano solo su quel formato, non su un dict piatto.
            try
            {
                result = JsonElementToObject(System.Text.Json.JsonDocument.Parse(form.ToJsonString()).RootElement);
            }
            catch { result = new Dictionary<string, object?>(); }
            return true;
        }
        if (methodName == "ParseMultipartForm" && argCount == 0)
        {
            // Forma BASIC canonica (schema StdLib.Http HttpPostedData/FormField/FileField): lista di
            // dict con Data = byte[]. Il PCode OP_LOAD_FIELD NON fa riflessione su oggetti .NET crudi
            // (spinge null → FOR EACH a zero elementi); convertendo qui, FormFields/FileFields/campo.Name/
            // file.Data passano dai percorsi dict/IList generici già testati — e l'IL legge gli stessi
            // dict (parità managed, sample HttpMultiPart).
            result = HttpPostedDataToDict(httpReqInst.ParseMultipartForm());
            return true;
        }
        return true;
    }

    /// <summary>Converte il CommonLib.Http.HttpPostedData parsato nel formato BASIC canonico:
    /// dict { FormFields: List&lt;dict{Name,Value}&gt;, FileFields: List&lt;dict{Name,FileName,
    /// ContentType,Data=byte[]}&gt; } — oppure null se il parsing non è multipart (NIL in BASIC).</summary>
    internal static Dictionary<string, object?>? HttpPostedDataToDict(CommonLib.Http.HttpPostedData? posted)
    {
        if (posted is null) return null;
        var formList = new List<object?>();
        foreach (var f in posted.FormFields)
            formList.Add(new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase)
            {
                ["Name"] = f.Name,
                ["Value"] = f.Value,
            });
        var fileList = new List<object?>();
        foreach (var fl in posted.FileFields)
            fileList.Add(new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase)
            {
                ["Name"] = fl.Name,
                ["FileName"] = fl.FileName,
                ["ContentType"] = fl.ContentType,
                ["Data"] = fl.Data,
            });
        return new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase)
        {
            ["FormFields"] = formList,
            ["FileFields"] = fileList,
        };
    }

    /// <summary>Estrae il cookie GM_BASIC_SESSION dall'header Cookie — stesso nome fisso e
    /// stessa logica di Interpreter/Gpm.Http.Server.cs::ParseSessionId (l'unico comportamento
    /// REALE di "sessione" nell'AST: nessuno store server-side). UNICA sorgente managed: usata
    /// da PCodeVM e backend IL (IlRuntime.HttpRequestToDict) per parità managed.</summary>
    internal static string? ParseSessionIdCookie(Dictionary<string, object?> headers)
    {
        if (!headers.TryGetValue("Cookie", out var cookieObj) || cookieObj is not string cookieStr) return null;
        foreach (var part in cookieStr.Split(';', StringSplitOptions.TrimEntries))
        {
            var eq = part.IndexOf('=');
            // Stesso bug del gemello AST (Gpm.Http.Server.cs::ParseSessionId): SetCookie scrive il
            // valore crudo, senza percent-encoding — uno spazio iniziale significativo (es. STR$()
            // su un intero positivo) sopravvive nell'header Set-Cookie, ma questo .Trim() lo
            // stripperebbe SOLO alla lettura, rompendo asimmetricamente il round-trip: lo stesso
            // sessionId letto da req.SessionId non risultava piu' uguale, byte per byte, a se
            // stesso al confronto con l'operatore =.
            if (eq > 0 && part[..eq].Trim().Equals("GM_BASIC_SESSION", StringComparison.OrdinalIgnoreCase))
                return part[(eq + 1)..];
        }
        return null;
    }

    /// <summary>Chiama una procedura handler per nome (dalla coda RAISE EVENT).</summary>
    /// <summary>
    /// Dispatches ON SPRITE COLLISION / ON SPRITE COLLISIONS con le collisioni del SYNC
    /// precedente. Chiamato NEL WORKER (nel case SYNC) — mai dal main thread (DispatchEventHandler
    /// dal main su un task bloccato congelava il loop). Ogni callback torna al main (mainIp).
    /// </summary>
    private void DispatchSpriteCollisions(PCodeTask task)
    {
        if (_pendingSpriteCollisions is not { Count: > 0 } collisions || _eventHandlers == null) return;
        if (_dispatchNextPlural)
        {
            _dispatchNextPlural = false;
            if (_eventHandlers.TryGetValue("SPRITE_COLLISIONS", out var hPlur) && hPlur.Count > 0)
            {
                _pendingSpriteCollisions = null;
                var ids = new List<object?>();
                foreach (var (ia, ib, _, _) in collisions)
                {
                    if (ia >= 0 && !ids.Contains((double)ia)) ids.Add((double)ia);
                    if (ib >= 0 && !ids.Contains((double)ib)) ids.Add((double)ib);
                }
                DispatchEventHandler(task, hPlur[0], new object?[] { ids });
                System.Console.Error.WriteLine("[VM] dispatch PLURALE n=" + ids.Count);
            }
        }
        else
        {
            _dispatchNextPlural = true;
            if (_eventHandlers.TryGetValue("SPRITE_COLLISION", out var hSing) && hSing.Count > 0)
            {
                _pendingSpriteCollisions = null;
                var (id1, id2, cx, cy) = collisions[0];
                DispatchEventHandler(task, hSing[0], new object?[] { (double)cx, (double)cy });
                System.Console.Error.WriteLine("[VM] dispatch SINGOLARE (" + cx + "," + cy + ")");
            }
        }
    }

    private void DispatchEventHandler(PCodeTask task, string handlerName, object?[] args)
    {
        if (!_procedureAddresses.TryGetValue(handlerName, out var addr))
            return; // handler non trovato: ignora (già registrato solo se esisteva)
        for (var i = 0; i < args.Length; i++) task.PushRef(args[i]);
        var newFrame = new PCodeCallFrame
        {
            SavedStackPointer = task.Sp - args.Length,
            ReturnIp = task.Ip,
            Previous = task.Frame,
            LocalVariables = CreateLocals(),
            IsEventDispatch = true
        };
        EnterProcedureErrorScope(task, newFrame);
        task.Frame = newFrame;
        task.Ip = addr;
    }

    private void ExecuteAudioCmd(byte subCode, object?[] args)
    {
        // Audio reale via AudioManager (SDL2_mixer). SOUND/PLAY/FILTER sono stati
        // collegati 2026-08-07 (prima: EmitExprAndPop nel compiler → no-op silenzioso).
        try
        {
            var audio = _audioManager ??= new CommonGraphics.AudioManager();
            switch ((AudioSubOpCode)subCode)
            {
                case AudioSubOpCode.SOUND:
                    // args: [voice, freq, dur, vol?, wave?, att?, dec?, sus?, rel?, pw?]
                    // NB: il VOICE (args[0]) era ignorato → SoundAsync usava sempre voice 0 →
                    // le voci si sovrapponevano/interrompevano (niente polifonia). Ora passa
                    // TUTTI i parametri a SoundOnVoiceAsync (voice + waveform + ADSR + pw).
                    if (args.Length >= 3)
                    {
                        var voice = (int)SafeDouble(args[0] ?? 0.0);
                        var freq = SafeDouble(args[1] ?? 440.0);
                        var dur = SafeDouble(args[2] ?? 0.0);
                        var vol = args.Length > 3 && args[3] != null ? (int)SafeDouble(args[3]) : 100;
                        var wave = args.Length > 4 && args[4] != null ? (int)SafeDouble(args[4]) : 0;
                        var att = args.Length > 5 && args[5] != null ? SafeDouble(args[5]) : -1;
                        var dec = args.Length > 6 && args[6] != null ? SafeDouble(args[6]) : -1;
                        var sus = args.Length > 7 && args[7] != null ? SafeDouble(args[7]) : -1;
                        var rel = args.Length > 8 && args[8] != null ? SafeDouble(args[8]) : -1;
                        var pw = args.Length > 9 && args[9] != null ? (int)SafeDouble(args[9]) : 25;
                        // Fire-and-forget: Sound() usa .Wait() sincrono che bloccherebbe il task VM.
                        _ = audio.SoundOnVoiceAsync(voice, freq, Math.Max(dur, 0.001),
                            Math.Clamp(vol, 0, 100), wave, att, dec, sus, rel, pw);
                    }
                    break;
                case AudioSubOpCode.PLAY:
                    // args: [actionCode, ...] — 0=STANDARD, 2=QUEUE, 3=NAMED, 4=LOOP, 5=CONTROL
                    if (args.Length >= 2 && args[1] is not null)
                    {
                        var action = (int)SafeDouble(args[0]);
                        if (action == 4) // LOOP: [code, music, channel, times]
                            _ = audio.PlayLoopAsync("", args[1]?.ToString() ?? "", args.Length > 3 && args[3] != null ? (int)SafeDouble(args[3]) : -1);
                        else
                            audio.Play(args[1]?.ToString() ?? "");
                    }
                    break;
                case AudioSubOpCode.FILTER:
                    // args: [voice, type, cutoff, resonance]
                    if (args.Length >= 4)
                        audio.SetVoiceFilter((int)SafeDouble(args[0]), (int)SafeDouble(args[1]),
                            SafeDouble(args[2]), SafeDouble(args[3]));
                    break;
            }
        }
        catch { /* audio non disponibile: non è un errore fatale */ }
    }

    private object? ExecuteClipCmd(byte subCode, object?[] args)
    {
        switch ((ClipSubOpCode)subCode)
        {
            case ClipSubOpCode.GET:
                // GET pusha il testo (il compilatore assegna a Destination subito dopo).
                // Prima: letto mai pushato (il chiamante non riceveva nulla).
                return _runtime.Clipboard.GetText() ?? "";
            case ClipSubOpCode.SET:
                if (args.Length >= 1)
                    _runtime.Clipboard.SetText(args[0]?.ToString());
                break;
            case ClipSubOpCode.CLEAR:
                _runtime.Clipboard.Clear();
                break;
        }
        return null;
    }
    
    // === Formattazione PRINT (matcha ConversionUtils.AppendFormattedValue) ===
    private static string FormatPrintValue(object? val)
    {
        return val switch
        {
            string s => s,
            float f => (f >= 0.0f ? " " : "") + f.ToString("G9", System.Globalization.CultureInfo.InvariantCulture) + " ",
            double d => FormatDoubleValue(d),
            decimal dec => (dec >= 0.0m ? " " : "") + dec.ToString("G", System.Globalization.CultureInfo.InvariantCulture) + " ",
            bool b => b ? "-1 " : " 0 ",
            char ch => ch.ToString(),
            int i => (i >= 0 ? " " : "") + i.ToString(System.Globalization.CultureInfo.InvariantCulture) + " ",
            long l => (l >= 0 ? " " : "") + l.ToString(System.Globalization.CultureInfo.InvariantCulture) + " ",
            short s => (s >= 0 ? " " : "") + s.ToString(System.Globalization.CultureInfo.InvariantCulture) + " ",
            byte bt => " " + bt.ToString(System.Globalization.CultureInfo.InvariantCulture) + " ",
            System.Collections.DictionaryEntry de2 => "[[Key, " + (de2.Key?.ToString() ?? "") + "], [Value, " + FormatPrintValueCompact(de2.Value) + "]]",
            Array arrVal => "[" + string.Join(", ", arrVal.Cast<object>().Select(FormatPrintValueCompact)) + "]",
            System.Collections.IList listVal => FormatIListValue(listVal),
            System.Collections.IDictionary dictVal => "[" + string.Join(", ", dictVal.Cast<object>().Select(x => {
                if (x is System.Collections.DictionaryEntry de4) return "[" + (de4.Key?.ToString() ?? "") + ", " + FormatPrintValueCompact(de4.Value) + "]";
                if (x is System.Collections.Generic.KeyValuePair<object, object?> kvp) return "[" + (kvp.Key?.ToString() ?? "") + ", " + FormatPrintValueCompact(kvp.Value) + "]";
                if (x is System.Collections.Generic.KeyValuePair<string, object?> kvps) return "[" + (kvps.Key?.ToString() ?? "") + ", " + FormatPrintValueCompact(kvps.Value) + "]";
                return x?.ToString() ?? "";
            })) + "]",
            System.Collections.Generic.Stack<object?> stkVal => "[" + string.Join(", ", stkVal.ToArray().Select(FormatPrintValueCompact)) + "]",
            System.Collections.Generic.Queue<object?> quVal => "[" + string.Join(", ", quVal.ToArray().Select(FormatPrintValueCompact)) + "]",
            System.Collections.Generic.HashSet<object?> hsVal => "[" + string.Join(", ", hsVal.Select(FormatPrintValueCompact)) + "]",
            _ => val?.ToString() ?? ""
        };
    }

    /// <summary>Formattazione per la CONCATENAZIONE di stringhe (OP_ADD con un operando
    /// stringa): numeri in G15 cultura invariante (parità IL StringHelpers.DoubleToInvariantString
    /// e VM/C gmb_format_value_compact) e collezioni "[...]". Il bool resta "True"/"False"
    /// (parità IL Convert.ToString(bool) e comportamento storico di OP_ADD): non è il "-1"/"0"
    /// del PRINT.</summary>
    private static string ConcatFormat(object? val)
    {
        return val switch
        {
            null => "",
            string s => s,
            bool b => b ? "True" : "False",
            double d => d.ToString("G15", System.Globalization.CultureInfo.InvariantCulture),
            float f => f.ToString("G9", System.Globalization.CultureInfo.InvariantCulture),
            decimal m => m.ToString(System.Globalization.CultureInfo.InvariantCulture),
            _ => FormatPrintValueCompact(val)
        };
    }

    /// <summary>Formatta un ELEMENTO di collezione in forma COMPATTA, allineato al canone IL
    /// (JsonValueToString) e a gmb_bytes_dump del runtime C: `[144, 1, ...]` e `[x, y]` — niente
    /// padding di colonna attorno ai numeri (quello è il formato PRINT degli scalari, non degli
    /// elementi in lista). Un elemento doppio NON passa dal SET DECIMALS/FIXED Clipper: anche IL
    /// formatta gli elementi array con G15 puro (JsonValueToString), non con FormatPrintValue.</summary>
    private static string FormatPrintValueCompact(object? val)
    {
        return val switch
        {
            null => "",
            string s => s,
            double d => d.ToString("G15", System.Globalization.CultureInfo.InvariantCulture),
            float f => f.ToString("G9", System.Globalization.CultureInfo.InvariantCulture),
            bool b => b ? "-1" : "0",
            System.Collections.DictionaryEntry de2 => "[[Key, " + (de2.Key?.ToString() ?? "") + "], [Value, " + FormatPrintValueCompact(de2.Value) + "]]",
            Array arrVal => "[" + string.Join(", ", arrVal.Cast<object>().Select(FormatPrintValueCompact)) + "]",
            System.Collections.IEnumerable en => "[" + string.Join(", ", en.Cast<object?>().Select(FormatPrintValueCompact)) + "]",
            _ => val.ToString() ?? ""
        };
    }

    /// <summary>Formatta un IList come [elem, ...], invertendo l'ordine per Stack.</summary>
    private static string FormatIListValue(System.Collections.IList list)
    {
        var items = list.Cast<object>().ToList();
        var isStack = list.GetType().Name.Contains("Stack");
        if (isStack)
            items.Reverse();
        return "[" + string.Join(", ", items.Select(FormatPrintValueCompact)) + "]";
    }

    /// <summary>Formatta un double come numero (DateValue gestisce le date). Via runtime C
    /// (`gmb_clipper_set_format_number`: SET DECIMALS/FIXED + legacy G15, canon IL/PCode/VM-C).</summary>
    private static string FormatDoubleValue(double d)
    {
        if (d == 0.0) d = 0.0;   // normalizza -0.0 → +0.0
        return Basic.IlBackend.IlRuntime.Current.Graphics.ClipperFormatNumber(d) + " ";
    }

    /// <summary>Wrapper per valori data/ora, formatatto come dd/MM/yyyy o HH:mm:ss da FormatPrintValue.
    /// internal (non private) perche' PCodeHttpResponse.SendJson (file diverso, stesso assembly)
    /// ha bisogno di un JsonConverter&lt;DateValue&gt; dedicato — vedi PCodeDateValueJsonConverter.</summary>
    internal sealed class DateValue : IConvertible
    {
        public double OADate { get; }
        public DateValue(double oaDate) => OADate = oaDate;
        public override string ToString()
        {
            try
            {
                if (OADate > 0.0 && OADate < 1.0)
                    return DateTime.FromOADate(OADate).ToString("HH:mm:ss");
                var dt = DateTime.FromOADate(OADate);
                var frac = Math.Abs(OADate - Math.Round(OADate));
                if (frac < 1e-10)
                    return dt.ToString("dd/MM/yyyy");
                return new DateTime(dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second).ToString("dd/MM/yyyy HH:mm:ss");
            }
            catch { return OADate.ToString(System.Globalization.CultureInfo.InvariantCulture); }
        }
        public TypeCode GetTypeCode() => TypeCode.Double;
        public bool ToBoolean(IFormatProvider? provider) => OADate != 0;
        public byte ToByte(IFormatProvider? provider) => (byte)OADate;
        public char ToChar(IFormatProvider? provider) => (char)OADate;
        public DateTime ToDateTime(IFormatProvider? provider) => DateTime.FromOADate(OADate);
        public decimal ToDecimal(IFormatProvider? provider) => (decimal)OADate;
        public double ToDouble(IFormatProvider? provider) => OADate;
        public short ToInt16(IFormatProvider? provider) => (short)OADate;
        public int ToInt32(IFormatProvider? provider) => (int)OADate;
        public long ToInt64(IFormatProvider? provider) => (long)OADate;
        public sbyte ToSByte(IFormatProvider? provider) => (sbyte)OADate;
        public float ToSingle(IFormatProvider? provider) => (float)OADate;
        public string ToString(IFormatProvider? provider) => ToString();
        public object ToType(Type conversionType, IFormatProvider? provider)
        {
            // Conversione a STRINGA: usa il formato data (dd/MM/yyyy) — mai il seriale OADate
            // (bug: DATE$() assegnato a DIM ... AS STRING dava "46242" invece di "06/08/2026").
            if (conversionType == typeof(string) || conversionType == typeof(object))
                return ToString();
            return Convert.ChangeType(OADate, conversionType, provider);
        }
        public ushort ToUInt16(IFormatProvider? provider) => (ushort)OADate;
        public uint ToUInt32(IFormatProvider? provider) => (uint)OADate;
        public ulong ToUInt64(IFormatProvider? provider) => (ulong)OADate;
    }

    /// <summary>Converte un argomento builtin (array BASIC) in un Vector&lt;double&gt; MathNet —
    /// usato dai builtin OPTIMIZE che operano su MathNet.Numerics.LinearAlgebra.</summary>
    private static global::MathNet.Numerics.LinearAlgebra.Vector<double> OptVec(object? arg)
        => global::MathNet.Numerics.LinearAlgebra.Vector<double>.Build.Dense(ToDoubleArr(arg));

    /// <summary>Converte un Vector&lt;double&gt; MathNet in un array BASIC (List&lt;object?&gt;
    /// di double boxed) — stessa convenzione di ritorno usata da GETRANGE/TOLIST (Fase 4), usata
    /// anche per passare un vettore come singolo argomento a una closure BASIC.</summary>
    private static List<object?> OptList(global::MathNet.Numerics.LinearAlgebra.Vector<double> v)
        => new(v.Select(x => (object?)x));

    /// <summary>
    /// Chiama una closure (ClosureData) in modo sincrono con un argomento e restituisce il risultato.
    /// Usato dai builtin AFILTER/AMAP/AREDUCE per chiamare callback lambda.
    /// </summary>
    private object? CallClosureSync(PCodeTask task, ClosureData cd, params object?[] args)
    {
        var addr = cd.ConstIndex >= 0 && cd.ConstIndex < _constants.Count
            ? (int?)(_constants[cd.ConstIndex] is double d ? (int)d : _constants[cd.ConstIndex] is int i ? i : -1)
            : null;
        if (!addr.HasValue || addr.Value < 0 || addr.Value >= _code.Length) return null;

        // Push arguments in order (first arg deepest on stack)
        for (int ai = 0; ai < args.Length; ai++)
            task.PushRef(args[ai]);
        var savedSp = task.Sp;
        var savedFrame = task.Frame;
        var savedIp = task.Ip;

        // Create frame for the closure call
        var locals = CreateLocals();
        var upval0 = cd.GetUpvalue(0);
        if (upval0 is System.Collections.IDictionary upDict)
            locals[0] = upDict;
        var newFrame = new PCodeCallFrame
        {
            SavedStackPointer = savedSp,
            ReturnIp = savedIp,
            Previous = savedFrame,
            LocalVariables = locals,
            Closure = cd
        };
        EnterProcedureErrorScope(task, newFrame);
        task.Frame = newFrame;
        task.Ip = addr.Value;

        // Run until RETURN. Confronto contro savedFrame (non newFrame): se il corpo
        // della closure fa una propria chiamata (OP_CALL spinge un frame diverso da
        // newFrame), il loop deve continuare a girare attraverso la chiamata annidata
        // finché non si torna al frame di partenza, non abbandonarla dopo un dispatch.
        while (task.Frame != null && task.Frame != savedFrame && task.State == PCodeTaskState.Running && _code.Length > 0)
        {
            var op = (OpCode)_code[task.Ip];
            task.Ip++;
            if (!DispatchOpcode(task, op)) break;
        }

        // Get return value (stack value after RETURN popped the frame)
        object? result = null;
        if (task.Sp > savedSp) result = task.PopRef();

        // Restore state if frame was popped by RETURN. NB: gli args pushati all'inizio NON
        // vengono rimossi dal RETURN della closure (SavedStackPointer = savedSp li include):
        // senza riportare sp a `savedSp - args.Length` ogni CallClosureSync LASCIA gli args
        // sullo stack → dopo migliaia di chiamate (MathNet Levenberg-Marquardt, DE) lo stack
        // si riempie di garbage e i successivi builtin array-returning leggono risultati
        // sfasati (es. Best(0) = risultato[1]).
        if (task.Frame == null || task.Frame != savedFrame)
            task.Frame = savedFrame;
        task.Sp = savedSp - args.Length;
        task.Ip = savedIp;

        return result;
    }

    /// <summary>Invoca un callback con gli argomenti dati. Il callback è SEMPRE una ClosureData:
    /// le lambda letterali la producono direttamente, le funzioni nominate passate come valore
    /// la producono a 0 upvalue via l'opcode OP_CLOSURE (Compiler.VisitFunctionReferenceExpression
    /// → stessa convenzione di VM-C). Un valore numerico NON è mai un callback: la distinzione è
    /// per TIPO, mai per valore — un Double sarebbe indistinguibile da un valore reale e
    /// `ASCAN(arr, 160)` chiamerebbe l'offset 160 come predicato (stack overflow, bug reale).
    /// null se non invocabile.</summary>
    private object? CallFnSync(PCodeTask task, object? fn, params object?[] args)
        => fn is ClosureData cd ? CallClosureSync(task, cd, args) : null;

    /// <summary>True se l'argomento è un callback invocabile (sempre una ClosureData).</summary>
    private static bool IsCallable(object? fn)
        => fn is ClosureData;

    /// <summary>Esegue un code-block compilato (CompileExpressionToCodeBlock → indirizzo Ip
    /// come double nella constant pool) in modo sincrono, con il record passato in locals[0]
    /// (stessa convenzione della closure: OP_LOAD_FIELD legge i campi dall'oggetto in locals[0]).
    /// Usato da DBF SET FILTER e dagli scope FOR/WHILE (condizioni per-record).</summary>
    private object? CallCodeBlockSync(PCodeTask task, int ip, object? record)
    {
        if (ip < 0 || ip >= _code.Length) return null;
        var savedSp = task.Sp;
        var savedFrame = task.Frame;
        var savedIp = task.Ip;
        var locals = CreateLocals();
        locals[0] = record;
        var newFrame = new PCodeCallFrame
        {
            SavedStackPointer = savedSp,
            ReturnIp = savedIp,
            Previous = savedFrame,
            LocalVariables = locals,
            Closure = null
        };
        EnterProcedureErrorScope(task, newFrame);
        task.Frame = newFrame;
        task.Ip = ip;
        while (task.Frame != null && task.Frame != savedFrame && task.State == PCodeTaskState.Running && _code.Length > 0)
        {
            var op = (OpCode)_code[task.Ip];
            task.Ip++;
            if (!DispatchOpcode(task, op)) break;
        }
        object? result = null;
        if (task.Sp > savedSp) result = task.PopRef();
        if (task.Frame == null || task.Frame != savedFrame)
            task.Frame = savedFrame;
        task.Sp = savedSp;
        task.Ip = savedIp;
        return result;
    }

    // --- Helpers unificati via DataSession ---
    /// <summary>
    /// Risolve un riferimento ISAM/DBF (alias stringa, numero di area esplicito, o il sentinel
    /// -1 emesso dal parser per "nessun alias, usa l'area attiva" — vedi
    /// Parser.Stmt.Isam.cs::ParseIsamAppendStatement) a un numero di area.
    ///
    /// <paramref name="taskAmbientArea"/> (task.CurrentIsamArea o task.CurrentDbfArea a seconda
    /// del chiamante) ha PRECEDENZA su <c>_dataSession.CurrentArea</c> quando fileRef non
    /// specifica esplicitamente un'area: _dataSession.CurrentArea è uno stato CONDIVISO da tutte
    /// le goroutine (stesso bug strutturale gia' risolto altrove spostando lo stato ambient da
    /// PCodeProcessor a PCodeTask — vedi TODO-PCODE-BUGS.md Bug 17 Follow-up 4), quindi non puo'
    /// essere l'unico fallback per il sentinel "area attiva": una goroutine che ha fatto
    /// ISAM SELECT sulla propria area vedrebbe silenziosamente le proprie APPEND/SEEK/PUT
    /// dirottate sull'area attiva GLOBALE (che potrebbe essere quella di un'ALTRA goroutine, o
    /// cambiare sotto i piedi per via di un ISAM SELECT concorrente), non sulla propria.
    /// </summary>
    private int ResolveAreaNum(PCodeTask task, object? fileRef, int taskAmbientArea = 0)
    {
        if (fileRef is string alias)
        {
            var driver = _dataSession.FindAlias(alias);
            if (driver != null)
            {
                for (int a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
                {
                    var wa = _dataSession.GetWorkArea(a);
                    if (wa?.Driver == driver) return a;
                }
            }
            return 0;
        }
        if (fileRef is string s && int.TryParse(s, out var pn) && pn > 0 && _dataSession.GetWorkArea(pn)?.Driver != null)
            return pn;
        var num = fileRef is double d ? (int)d : fileRef is int i ? i : 0;
        if (num > 0 && _dataSession.GetWorkArea(num)?.Driver != null) return num;
        if (taskAmbientArea > 0) return taskAmbientArea;
        return _dataSession.CurrentArea;
    }

    private Basic.ISAM.Engine? ResolveIsamEngine(PCodeTask task, object? fileRef)
    {
        var area = ResolveAreaNum(task, fileRef, task.CurrentIsamArea);
        var wa = area > 0 ? _dataSession.GetWorkArea(area) : null;
        return (wa?.Driver as Basic.Databases.Rdd.IsamDriver)?._engine;
    }

    /// <summary>Tutti i motori ISAM attualmente aperti in una qualsiasi work area (deduplicati),
    /// usati da BEGIN TRANSACTION/COMMIT/ROLLBACK non qualificati per operare su tutte le
    /// aree ISAM aperte (nessun alias in questa sintassi legacy).</summary>
    private IEnumerable<Basic.ISAM.Engine> GetOpenIsamEngines()
    {
        var seen = new HashSet<Basic.ISAM.Engine>();
        for (var a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
        {
            var engine = (_dataSession.GetWorkArea(a)?.Driver as Basic.Databases.Rdd.IsamDriver)?._engine;
            if (engine != null && seen.Add(engine)) yield return engine;
        }
    }

    /// <summary>Il driver Rdd (Databases/Rdd/IsamDriver) che corrisponde allo stesso file/area
    /// risolto da ResolveIsamEngine — serve a SyncChildRelations per identificare "chi si è
    /// appena mosso" e trovarne i figli in relazione (IsamDriver.SetRelation/SyncRelation vive
    /// sul driver, non sull'engine sottostante).</summary>
    /// <summary>Driver DBF che possiede questo engine (i driver del C# hanno il testo del
    /// filtro; l'engine solo l'oggetto espressione). Cerca fra le work-area della sessione:
    /// non dipende dal tracking dell'area corrente, che può divergere.
    /// § MiniTests/dbf/dbf_lock_filter.bas: DBFILTER() restituiva "" perché il testo non era
    /// letto dal driver.</summary>
    private Basic.Databases.Rdd.DbfDriver? DbfDriverForEngine(Basic.DBF.DbfEngine? eng)
    {
        if (eng is null) return null;
        for (var a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
            if (_dataSession.GetWorkArea(a)?.Driver is Basic.Databases.Rdd.DbfDriver d && ReferenceEquals(d.Engine, eng))
                return d;
        return null;
    }

    /// <summary>Contesto LEDGER dall'handle, con fallback al ledger CORRENTE (l'ultimo aperto):
    /// i lettori LEDGER ricevono talvolta un handle 0 (ordine degli argomenti nel bytecode),
    /// mentre il mondo C ha un "current ledger" implicito. Senza il fallback COUNT/LASTREC/
    /// ACCOUNTS/BALANCE/REGISTER tornavano 0/vuoto. § MiniTests/ledger/ledger_ops.bas.</summary>
    private Basic.Ledger.LedgerContext? ResolveLedgerCtx(int handle)
    {
        if (_ledgerContexts.TryGetValue(handle, out var c)) return c;
        if (_ledgerContexts.Count == 0) return null;
        var last = -1;
        foreach (var k in _ledgerContexts.Keys) if (k > last) last = k;
        return _ledgerContexts[last];
    }

    private Basic.Databases.Rdd.IDatabaseDriver? ResolveIsamDriver(PCodeTask task, object? fileRef)
    {
        var area = ResolveAreaNum(task, fileRef, task.CurrentIsamArea);
        return area > 0 ? _dataSession.GetWorkArea(area)?.Driver : null;
    }

    /// <summary>
    /// Dopo che <paramref name="movedDriver"/> si è spostato (GO/SKIP/SEEK sull'ISAM.Engine
    /// sottostante, che bypassa IsamDriver.GoTop/Skip/GoTo e quindi non fa mai scattare
    /// IsamDriver.SyncCursorState/SyncRelation da solo — vedi AiWiki
    /// DbIsamFullTests-Relazione-Cursore-TODO.md): (1) risincronizza Eof/Bof/Found/RecNo del
    /// driver stesso dall'engine (altrimenti ISAM.FOUND/EOF/BOF letti via IDatabaseDriver
    /// restano fermi allo stato dell'apertura — bug scoperto verificando che ISAM.FOUND()
    /// tornava sempre falso anche dopo una SEEK riuscita), poi (2) riposiziona ogni area figlio
    /// la cui ISAM SET RELATION punta a movedDriver come padre. Scansione lineare di tutte le
    /// work area (max 255, economica): non esiste un indice "figli di X" già pronto, e SET
    /// RELATION è un'operazione rara rispetto a GO/SKIP.
    /// </summary>
    private void SyncChildRelations(Basic.Databases.Rdd.IDatabaseDriver? movedDriver)
    {
        if (movedDriver == null) return;
        if (movedDriver is Basic.Databases.Rdd.IsamDriver movedIsam) movedIsam.SyncCursorState();
        if (movedDriver is Basic.Databases.Rdd.DbfDriver movedDbf) movedDbf.SyncCursorState();
        for (var a = 1; a <= Basic.Databases.Rdd.DataSession.MaxWorkareas; a++)
        {
            var childDriver = _dataSession.GetWorkArea(a)?.Driver;
            if (childDriver != null && childDriver.HasActiveRelation && ReferenceEquals(childDriver.ParentRelationDriver, movedDriver))
            {
                try { childDriver.SyncRelation(); } catch { }
            }
        }
    }

    /// <summary>Converte l'indice di campo 1-based (convenzione Clipper FieldGet/FieldPut, usata da
    /// RDD.SETFIELD/GETFIELD/OLDVAL/GETFLDSTATE) nel SymbolId reale del compilatore per una work
    /// area ISAM/DBF (dove wa.Meta["Udt"] è presente). Senza questa conversione RDD.SETFIELD
    /// scriveva nel buffer sotto la chiave "1"/"2"/"3" invece del vero SymbolId del campo (es.
    /// 305 per "Salary"), un valore che GetCurrentRecord/PutCurrentRecord non usano mai: il
    /// commit successivo (TABLEUPDATE) scriveva quindi il record INVARIATO, con le modifiche del
    /// buffer silenziosamente perse — scoperto verificando RDD_Buffer_Demo.bas dopo il fix del
    /// crash LockRecursionException in RowsBuffer.TableUpdate. Per le work area SENZA "Udt" (es.
    /// SQL, il cui SchemaBuffer usa già indici di colonna 0-based sequenziali) l'indice passa
    /// invariato — nessuna regressione lì.</summary>
    /// <summary>Overload oggetto: ammette NOME di campo (chiave reale = SymbolId del
    /// driver, non la posizione di GetFieldIndex: solo su DbfDriver le due coincidono)
    /// oltre all'ordinale 1-based.</summary>
    private int RddResolveFieldSymbolId(Basic.Databases.Rdd.DatabaseWorkArea? wa, object? fieldObj)
    {
        if (fieldObj is string sn && sn.Length > 0 && !(fieldObj is double))
        {
            var drv = wa?.Driver;
            if (drv == null) return -1;
            Basic.Databases.Rdd.RddFieldInfo? hit = null;
            foreach (var ff in drv.Fields)
                if (string.Equals(ff.Name, sn, StringComparison.OrdinalIgnoreCase)) { hit = ff; break; }
            return hit?.SymbolId ?? -1;
        }
        return RddResolveFieldSymbolId(wa, (int)(fieldObj is double dd ? dd : 0));
    }

    private int RddResolveFieldSymbolId(Basic.Databases.Rdd.DatabaseWorkArea? wa, int fieldIdx)
    {
        if (wa?.Meta.GetValueOrDefault("Udt") is UserDefinedType udt && fieldIdx >= 1 && fieldIdx <= udt.Fields.Length)
            return udt.Fields[fieldIdx - 1].SymbolId;
        // Aree senza UDT (RDD USE puro, SQL): colonne 0-based → l'ordinale Clipper
        // va scalato qui, o il buffer scrive chiavi che GetCurrentRecord non legge
        // (stessa classe di bug documentata nel doc-comment above, lato no-UDT).
        return fieldIdx >= 1 ? fieldIdx - 1 : fieldIdx;
    }

    private IsamWorkArea? ResolveIsamArea(PCodeTask task, object? fileRef)
    {
        var area = ResolveAreaNum(task, fileRef, task.CurrentIsamArea);
        var wa = area > 0 ? _dataSession.GetWorkArea(area) : null;
        if (wa == null || wa.Driver is not Basic.Databases.Rdd.IsamDriver idrv || idrv._engine == null) return null;
        var meta = wa.Meta;
        return new IsamWorkArea
        {
            Engine = idrv._engine,
            Udt = (UserDefinedType?)meta.GetValueOrDefault("Udt"),
            Schema = (Basic.ISAM.Config.IsamSchema?)meta.GetValueOrDefault("Schema"),
            FieldSymbolIds = (List<int>?)meta.GetValueOrDefault("FieldSymbolIds"),
            FieldNames = (List<string>?)meta.GetValueOrDefault("FieldNames"),
            Driver = idrv
        };
    }

    private void SetCurrentIsamArea(PCodeTask task, object? area)
    {
        task.CurrentIsamArea = ResolveAreaNum(task, area, task.CurrentIsamArea);
    }

    private Basic.DBF.DbfEngine? ResolveDbfEngine(PCodeTask task, object? fileRef)
    {
        var area = ResolveAreaNum(task, fileRef, task.CurrentDbfArea);
        var wa = area > 0 ? _dataSession.GetWorkArea(area) : null;
        return (wa?.Driver as Basic.Databases.Rdd.DbfDriver)?._engine;
    }

    private Basic.Databases.Rdd.IDatabaseDriver? ResolveDbfDriver(PCodeTask task, object? fileRef)
    {
        var area = ResolveAreaNum(task, fileRef, task.CurrentDbfArea);
        return area > 0 ? _dataSession.GetWorkArea(area)?.Driver : null;
    }

    /// <summary>Come ResolveDbfEngine, ma restituisce anche i (Nome campo, SymbolId compilatore)
    /// registrati da DBF USE ... AS TypeName — servono a DBF_GET_RECORD per costruire il
    /// wrapper "__class__"/FieldNames che OP_LOAD_MEMBER richiede per leggere i campi via
    /// accesso a membro (rec.Campo). Senza questa metadata il dizionario restituito da
    /// GetCurrentRecordValues() è illeggibile da OP_LOAD_MEMBER (cade sempre su null).</summary>
    private (Basic.DBF.DbfEngine? Engine, List<(string Name, int SymbolId)>? FieldSymbols) ResolveDbfArea(PCodeTask task, object? fileRef)
    {
        var area = ResolveAreaNum(task, fileRef, task.CurrentDbfArea);
        var wa = area > 0 ? _dataSession.GetWorkArea(area) : null;
        var engine = (wa?.Driver as Basic.Databases.Rdd.DbfDriver)?._engine;
        var fieldSymbols = wa?.Meta.GetValueOrDefault("DbfFieldSymbols") as List<(string Name, int SymbolId)>;
        return (engine, fieldSymbols);
    }

    // ── SPEED helpers ──
    private Basic.Databases.Speed.Database? ResolveSpeedDatabase(PCodeTask task, object? fileRef)
    {
        var area = ResolveAreaNum(task, fileRef, task.CurrentDbfArea);
        var wa = area > 0 ? _dataSession.GetWorkArea(area) : null;
        return (wa?.Driver as Basic.Databases.Rdd.SpeedDriver)?._db;
    }

    // ── SPEED helpers ──
    private static Dictionary<string, object?> BytesToSpeedRecord(byte[] bytes)
    {
        // Deserializzazione: dict codificato JSON. Deserialize<Dictionary<string, object?>> lascia
        // ogni valore come System.Text.Json.JsonElement grezzo (comportamento di default di
        // System.Text.Json per proprietà tipizzate `object`): né SpeedEvaluateExpr né SpeedCompare
        // riconoscono JsonElement nei loro pattern match su string/bool/double, quindi OGNI
        // confronto (WHERE/JOIN/GROUP BY/HAVING/ORDER BY/SET FILTER) cadeva nel fallback
        // ToString()-vs-ToString() di SpeedCompare — che per coincidenza produce lo stesso testo
        // per bool/interi semplici (es. "True"/"3") mascherando il problema, ma fallisce silenziosamente
        // per stringhe a lunghezza fissa (es. "Roma" letterale vs "Roma                " dal campo
        // STRING*20, mai uguali). Va quindi "srotolato" esplicitamente nei tipi CLR nativi.
        try
        {
            var json = System.Text.Encoding.UTF8.GetString(bytes);
            using var doc = System.Text.Json.JsonDocument.Parse(json);
            var dict = new Dictionary<string, object?>();
            foreach (var prop in doc.RootElement.EnumerateObject())
                dict[prop.Name] = SpeedUnwrapJsonElement(prop.Value);
            return dict;
        }
        catch { return new Dictionary<string, object?> { ["__raw__"] = bytes }; }
    }

    private static object? SpeedUnwrapJsonElement(System.Text.Json.JsonElement el)
    {
        switch (el.ValueKind)
        {
            // TrimEnd('\0', ' ') come RecordMarshaller.cs (DBF/ISAM): i campi STRING*N a
            // lunghezza fissa arrivano qui già riempiti di spazi dall'assegnazione BASIC
            // (vedi STRING*N padding), quindi un confronto contro un letterale non-riempito
            // (es. WHERE Citta = "Roma") fallirebbe sempre senza normalizzare il padding qui.
            case System.Text.Json.JsonValueKind.String: return el.GetString()?.TrimEnd('\0', ' ');
            case System.Text.Json.JsonValueKind.Number: return el.GetDouble();
            case System.Text.Json.JsonValueKind.True: return true;
            case System.Text.Json.JsonValueKind.False: return false;
            case System.Text.Json.JsonValueKind.Null or System.Text.Json.JsonValueKind.Undefined: return null;
            case System.Text.Json.JsonValueKind.Object:
            {
                var d = new Dictionary<string, object?>();
                foreach (var p in el.EnumerateObject()) d[p.Name] = SpeedUnwrapJsonElement(p.Value);
                return d;
            }
            case System.Text.Json.JsonValueKind.Array:
                return el.EnumerateArray().Select(SpeedUnwrapJsonElement).ToList();
            default: return null;
        }
    }

    private static byte[] SpeedRecordToBytes(System.Collections.IDictionary dict)
    {
        var d = new Dictionary<string, object?>();
        foreach (System.Collections.DictionaryEntry kv in dict)
            d[kv.Key.ToString()!] = kv.Value;
        return System.Text.Encoding.UTF8.GetBytes(
            System.Text.Json.JsonSerializer.Serialize(d));
    }

    private static byte[] SpeedValueToBytes(object? v)
    {
        if (v == null) return Array.Empty<byte>();
        if (v is string s) return System.Text.Encoding.UTF8.GetBytes(s);
        if (v is double d) return SpeedSortableDoubleBytes(d);
        if (v is int i) return SpeedSortableDoubleBytes(i);
        if (v is long l) return SpeedSortableDoubleBytes(l);
        return System.Text.Encoding.UTF8.GetBytes(v.ToString() ?? "");
    }

    // Le chiavi SPEED sono ordinate dal motore di storage tramite confronto lessicografico
    // (byte-a-byte) dei loro byte grezzi (vedi ByteArrayComparer). I byte little-endian di
    // BitConverter.GetBytes(double) NON preservano l'ordinamento numerico sotto confronto
    // lessicografico: serve una codifica order-preserving (big-endian, con inversione bit
    // per i valori negativi/positivi secondo lo schema standard per double IEEE754).
    private static byte[] SpeedSortableDoubleBytes(double d)
    {
        var bits = BitConverter.DoubleToInt64Bits(d);
        bits = bits >= 0 ? bits ^ long.MinValue : ~bits;
        var bytes = BitConverter.GetBytes(bits);
        if (BitConverter.IsLittleEndian) Array.Reverse(bytes);
        return bytes;
    }

    private static Dictionary<string, object?> EntryToDict(LedgerEntry e)
    {
        // Formato IDENTICO al C# (DatabaseService.LedgerEntryJson): Date come "yyyy-MM-dd"
        // (prima l'oggetto DateOnly → {}), niente Currency/ExchangeRate/BaseAmount.
        return new Dictionary<string, object?>
        {
            ["TransactionId"] = (double)e.TransactionId,
            ["Date"] = e.Date.ToString("yyyy-MM-dd"),
            ["Status"] = e.Status.ToString(),
            ["Description"] = e.Description,
            ["Account"] = e.Account,
            ["Amount"] = (double)e.Amount,
            ["Comment"] = e.Comment,
            ["AmountIsExplicit"] = e.AmountIsExplicit
        };
    }

    /// <summary>
    /// Converte un valore runtime (array, ListWrapper, stringa, ecc.) in un IEnumerator — stessa
    /// logica di Gpm.Generators.cs::GetRuntimeEnumerator lato AST, riusata qui per garantire lo
    /// stesso comportamento su entrambi i backend.
    /// </summary>
    private static System.Collections.IEnumerator GetRuntimeEnumeratorForGenerator(object? value)
    {
        if (value is System.Collections.IEnumerable enumerable)
            return enumerable.GetEnumerator();
        if (value is System.Collections.IEnumerator enumerator)
            return enumerator;
        if (value is string str)
            return ((System.Collections.IEnumerable)str.ToCharArray()).GetEnumerator();

        throw new InvalidOperationException(
            $"Il tipo '{value?.GetType().Name ?? "null"}' non supporta l'iterazione (IEnumerable).");
    }

    /// <summary>
    /// Valore di ritorno sentinella per NEXT() quando un generatore è esaurito — intercettato da
    /// OP_CALL_BUILTIN PRIMA del PushRef incondizionato per reindirizzare al blocco TRY attivo
    /// (StopIteration, ERR.Number=144), invece di essere spinto come un valore normale.
    /// </summary>
    private sealed class StopIterationSignal
    {
        public static readonly StopIterationSignal Instance = new();
    }

    /// <summary>
    /// Estende List&lt;object?&gt; (invece di limitarsi a implementare IEnumerable) perché il FOR
    /// EACH di P-CODE (VisitForEachAdvanceAndAssignStatement, Compiler/Compiler.Others.cs) NON usa
    /// GetEnumerator()/MoveNext(): compila a LBOUND/UBOUND + OP_LOAD_ELEMENT indicizzato, come un
    /// vero array — OP_FOR_EACH_ADVANCE esiste nell'OpCode enum e nel Disassembler ma non viene
    /// MAI emesso dal Compiler (verificato via grep), quindi è dead code, non la strada giusta.
    /// Estendere List&lt;object?&gt; soddisfa LBOUND/UBOUND/OP_LOAD_ELEMENT (che già riconoscono
    /// IList) gratis, senza toccare quel meccanismo condiviso.
    /// Questo sacrifica la vera lazy-ness per sequenze infinite (la sequenza viene materializzata
    /// per intero al momento della creazione, non on-demand) — accettabile per ora: i generatori
    /// potenzialmente infiniti passano da GENERATOR...YIELD (non ancora supportato in P-CODE),
    /// non da un'espressione generatore su una sorgente finita.
    /// </summary>
    private sealed class PCodeGeneratorExprState : System.Collections.Generic.List<object?>
    {
        /// <summary>Posizione del prossimo elemento da restituire, per NEXT() ripetuto sulla stessa istanza.</summary>
        public int NextPosition;
    }

    /// <summary>
    /// Handle runtime per un GENERATOR PROCEDURE (<c>GENERATOR Nome(...) ... END GENERATOR</c>),
    /// creato da OP_NEW_GENERATOR_CALL. Incapsula il PCodeTask dedicato (sospeso, mai eseguito
    /// finché NEXT() non lo richiede) e lo stato di completamento — analogo a GeneratorInstance
    /// lato GPM (Interpreter/Gpm.VariableScope.cs), ma guidato in modo sincrono invece che tramite
    /// scheduler cooperativo/Future, dato che P-CODE non ha (e qui non serve) concorrenza reale
    /// tra generatore e chiamante: sono sempre mutuamente esclusivi nel tempo.
    /// </summary>
    private sealed class PCodeGeneratorInstance
    {
        public readonly PCodeTask Task;
        public bool Done;

        public PCodeGeneratorInstance(PCodeTask task)
        {
            Task = task;
        }
    }

    /// <summary>
    /// Guida un PCodeTask di GENERATOR PROCEDURE avanti fino al prossimo YIELD (OP_NEXT_GENERATOR,
    /// che salva il valore in task.PendingResult e restituisce false) o fino al suo completamento
    /// naturale (OP_RETURN senza più Frame → task.State = Finished, oppure IP fuori range).
    /// Esegue SEMPRE sullo stesso thread del chiamante (mai in parallelo, a differenza di una vera
    /// goroutine OP_GO): ogni chiamata riprende esattamente da dove l'ultimo YIELD (o l'inizio, la
    /// prima volta) aveva sospeso, dato che Ip/locals/ForState persistono naturalmente
    /// sull'oggetto PCodeTask dedicato tra una chiamata e l'altra.
    /// </summary>
    private object? DriveGeneratorTask(PCodeTask genTask)
    {
        try
        {
            while (!genTask.IsFinished)
            {
                if (genTask.Ip < 0 || genTask.Ip >= _code.Length)
                {
                    genTask.State = PCodeTaskState.Finished;
                    break;
                }
                var op = (OpCode)_code[genTask.Ip];
                genTask.Ip++;
                bool cont;
                try
                {
                    cont = DispatchOpcode(genTask, op);
                }
                catch (Exception ex)
                {
                    genTask.State = PCodeTaskState.Finished;
                    genTask.PendingException = ex;
                    throw;
                }
                if (op == OpCode.OP_NEXT_GENERATOR)
                {
                    var yielded = genTask.PendingResult;
                    genTask.PendingResult = null;
                    return yielded;
                }
                if (!cont) break;
            }
            return StopIterationSignal.Instance;
        }
        finally
        {
            // Stesso motivo di ExecuteTaskTick: un generatore guidato passo-passo (NEXT) non
            // passa mai da ExecuteTaskTick, quindi ha bisogno del proprio flush per non lasciare
            // i PRINT del suo corpo bufferati per sempre in genTask.PendingPrints.
            FlushPrintBuffer(genTask);
        }
    }

    private int SkipToNextInstruction(int fromIp)
    {
        for (int i = fromIp, end = Math.Min(fromIp + 8, _code.Length); i < end; i++)
        {
            if (Enum.IsDefined(typeof(OpCode), (int)_code[i]) && _code[i] != 0)
                return i;
        }
        return Math.Min(fromIp + 1, _code.Length);
    }

    // ── HELPER CONDIVISI StdLib.Runtime (adapter enum→tipizzato, Fase 2+ del piano
    // docs/StdLib-Runtime-Plan.md) — riportati qui da PCodeProcessor.StdLib.cs (Fase 6:
    // quel file conteneva SOLO il dispatcher morto ExecuteStdLibBuiltIn, mai chiamato, ma
    // questi 5 helper sono usati da decine di case reali in PCodeProcessor.Builtins.cs.
    // Data/ora: nel P-CODE una data e' boxed come DateValue (OADate). La stessa
    // conversione usata dai case esistenti (DateTime.FromOADate(SafeDouble(...))).
    private static DateTime ToDateTime(object? arg) => DateTime.FromOADate(SafeDouble(arg));

    private static DateTime[] ToDateTimeArr(object? arg)
    {
        if (arg is DateTime[] dt) return dt;
        if (arg is DateValue[] dv) return dv.Select(x => DateTime.FromOADate(x.OADate)).ToArray();
        if (arg is double[] da) return da.Select(DateTime.FromOADate).ToArray();
        if (arg is object?[] oa) return oa.Select(x => DateTime.FromOADate(SafeDouble(x))).ToArray();
        return Array.Empty<DateTime>();
    }

    // Byte array per funzioni crypto/hash: normalizza string|byte[]|double[]|object[]
    // nello stesso modo di IlRuntime.CryptoBytes.
    private static byte[] ToBytes(object? arg)
    {
        return arg switch
        {
            null => Array.Empty<byte>(),
            byte[] bytes => bytes,
            string text => System.Text.Encoding.UTF8.GetBytes(text),
            double[] da => da.Select(x => (byte)x).ToArray(),
            object?[] oa => oa.Select(x => x is byte b ? b : (byte)SafeDouble(x)).ToArray(),
            _ => System.Text.Encoding.UTF8.GetBytes(Convert.ToString(arg, System.Globalization.CultureInfo.InvariantCulture) ?? string.Empty)
        };
    }

    private static string ToHexStr(byte[] bytes) => Convert.ToHexString(bytes).ToLowerInvariant();

    // Restituisce args[index] oppure null se fuori range.
    private static object? ArgOr(object?[] args, int index) => index < args.Length ? args[index] : null;

    /// <summary>Avvia l'esecuzione dei body DEFER della procedura corrente (LIFO, parità Go
    /// defer). Se il frame ha deferred registrati, imposta Ip al primo body e marca
    /// PendingDeferReturn; ritorna true (il task deve eseguire il body). Se non ce ne sono,
    /// ritorna false e il RETURN completa normalmente.</summary>
    private bool BeginDeferredExecution(PCodeTask task)
    {
        var frame = task.Frame;
        // Goroutine radice (frame null): deferred task-level (task.DeferredIps).
        var deferred = frame != null ? frame.DeferredIps : task.DeferredIps;
        if (deferred is not { Count: > 0 })
            return false;
        task.PendingDeferReturn = true;
        task.Ip = deferred.Pop();
        return true;
    }

    /// <summary>Chiamato da OP_DEFER_RET: dopo l'esecuzione di un body DEFER, esegue il
    /// prossimo body LIFO della stessa procedura, oppure completa il RETURN originale
    /// (task.State = Finished) quando i deferred sono esauriti.</summary>
    private bool ExecuteNextDeferredOrCompleteReturn(PCodeTask task)
    {
        var frame = task.Frame;
        // Goroutine radice (frame null): deferred task-level (task.DeferredIps).
        var deferred = frame != null ? frame.DeferredIps : task.DeferredIps;
        if (deferred is { Count: > 0 })
        {
            task.Ip = deferred.Pop();
            return true;
        }
        task.PendingDeferReturn = false;
        // I DEFER sono esauriti: completa il RETURN originale (l'unwind era stato lasciato
        // sospeso da ContinuePendingUnwind quando ha iniettato i DEFER).
        var unwind = task.PendingUnwind;
        if (unwind != null)
        {
            task.PendingUnwind = null;
            // BUG risolto (loop infinito — divergenza DEFER tra IL/PCode e il vecchio emitter C#): qui
            // si riprendeva a ReturnOpcodeIp+1, l'istruzione DOPO l'OP_RETURN
            // NEL BYTECODE DI WORK STESSA — non l'indirizzo del CHIAMANTE (quello vive in
            // frame.ReturnIp, letto dal case OP_RETURN "vero" più sotto). +1 dopo un OP_RETURN
            // (nessun operando) capita di ricadere sull'inizio del corpo della PROSSIMA
            // procedura compilata in sequenza nello stream — qui, di nuovo Work stessa — che la
            // rieseguiva da capo all'infinito (mai raggiunto il RETURN reale del chiamante,
            // stack P-Code cresciuto fino all'overflow). Frame/Sp non erano mai stati
            // toccati: task.Frame punta ANCORA al frame di Work, esattamente come se
            // l'OP_RETURN originale non fosse mai stato eseguito.
            // Fix: riprendi ESATTAMENTE all'IP dell'OP_RETURN originale (non +1) e falla
            // rieseguire. PendingUnwind è già null e i DEFER del frame sono ormai vuoti, quindi
            // il case OP_RETURN prende questa volta il ramo normale (pop frame, Ip =
            // frame.ReturnIp, push valore di ritorno) — lo stesso identico meccanismo che già
            // usa un RETURN senza DEFER (ContinuePendingUnwind, ramo finale "return false" senza
            // +1, riga ~807 di PCodeProcessor.Helpers.cs).
            if (unwind.Frame != null)
            {
                task.Ip = unwind.ReturnOpcodeIp;
                return true;
            }
        }
        // Nessun unwind sospeso (RETURN da codice che non passa dall'unwind): completa.
        task.State = PCodeTaskState.Finished;
        return false;
    }
}
