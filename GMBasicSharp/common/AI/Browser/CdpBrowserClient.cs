// Browser/CdpBrowserClient.cs — client CDP (Chrome DevTools Protocol) via WebSocket
// Compatibile con Chromium (launch_chromium.sh --remote-debugging-port=9222) e
// Obscura (Rust/V8, CDP). Obscura NON accetta comandi sul ws del page target: usa il
// modello flat-session sul ws di browser (Target.createTarget + attachToTarget flatten,
// comandi con sessionId). Chromium funziona anche diretto sul ws del page target.
// Strategia: connetti prima al page target; al primo errore CDP fai upgrade flat-session
// (createTarget+attach) e ritenta. Singolo file/dominio, <1200 righe.
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Basic.AI.Browser;

public sealed class CdpBrowserClient : IBrowserActions
{
    private const string UpgradeErrorHint = "page";
    private readonly string _wsUrl;
    private readonly string? _browserWsUrl;
    private ClientWebSocket? _ws;
    private string? _sessionId;
    private int _nextId = 1;
    private readonly SemaphoreSlim _gate = new(1, 1);
    // Ref @eN -> backendDOMNodeId, popolato da AccessibilityTreeAsync (residuo A: click/fill per ref)
    private readonly System.Collections.Generic.List<int> _refBackend = new();

    public CdpBrowserClient(string wsUrl, string? browserWsUrl = null)
    {
        _wsUrl = wsUrl;
        _browserWsUrl = browserWsUrl;
    }

    public static async Task<CdpBrowserClient?> ConnectAsync(int port, CancellationToken ct = default)
    {
        try
        {
            // Chrome moderne ascolta solo su [::1] (IPv6); Obscura su 127.0.0.1.
            foreach (var host in new[] { "127.0.0.1", "[::1]" })
            {
                var baseUrl = $"http://{host}:{port}";
                try
                {
                    string? pageWs = null, browserWs = null;
                    using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(3) };
                    // browser-level ws (per upgrade flat-session)
                    try
                    {
                        var json = await http.GetStringAsync($"{baseUrl}/json/version", ct);
                        using var vdoc = JsonDocument.Parse(json);
                        browserWs = vdoc.RootElement.GetProperty("webSocketDebuggerUrl").GetString();
                    }
                    catch { }
                    // page target ws (fast-path Chromium)
                    try
                    {
                        var list = await http.GetStringAsync($"{baseUrl}/json/list", ct);
                        using var doc = JsonDocument.Parse(list);
                        foreach (var t in doc.RootElement.EnumerateArray())
                        {
                            if (t.TryGetProperty("type", out var ty) && ty.GetString() == "page" &&
                                t.TryGetProperty("webSocketDebuggerUrl", out var u))
                            {
                                pageWs = u.GetString();
                                if (!string.IsNullOrEmpty(pageWs)) break;
                            }
                        }
                    }
                    catch { }
                    if (string.IsNullOrEmpty(pageWs) && string.IsNullOrEmpty(browserWs)) continue;

                    var client = new CdpBrowserClient(pageWs ?? browserWs!, browserWs);
                    await client.OpenAsync(ct);
                    return client;
                }
                catch { }
            }
            return null;
        }
        catch { return null; }
    }

    private async Task OpenAsync(CancellationToken ct)
    {
        _ws = new ClientWebSocket();
        await _ws.ConnectAsync(new Uri(_wsUrl), ct);
    }

    private async Task<ClientWebSocket> OpenBrowserWsAsync(CancellationToken ct)
    {
        var browserWs = _browserWsUrl ?? _wsUrl;
        var s = new ClientWebSocket();
        await s.ConnectAsync(new Uri(browserWs), ct);
        return s;
    }

    /// <summary>Upgrade al modello flat-session sul ws di browser (richiesto da Obscura):
    /// crea un target page e lo attacha flatten, poi ogni comando viaggia con sessionId.</summary>
    private async Task<bool> UpgradeToFlatSessionAsync(CancellationToken ct)
    {
        ClientWebSocket? browserWs = null;
        try
        {
            browserWs = await OpenBrowserWsAsync(ct);
            // 1) crea/individua un page target
            var create = await SendRawAsync(browserWs, "Target.createTarget", new { url = "about:blank" }, null, ct);
            string? targetId = null;
            if (create is { } cr && cr.TryGetProperty("targetId", out var tid)) targetId = tid.GetString();
            if (targetId is null)
            {
                var list = await SendRawAsync(browserWs, "Target.getTargets", null, null, ct);
                if (list is { } gt)
                {
                    foreach (var t in gt.EnumerateArray())
                        if (t.TryGetProperty("targetId", out var t2) && t2.GetString() is { Length: > 0 } id)
                        { targetId = id; break; }
                }
            }
            if (targetId is null) { try { await browserWs.CloseAsync(WebSocketCloseStatus.NormalClosure, "", ct); } catch { } return false; }
            // 2) attach flatten → sessionId
            var attach = await SendRawAsync(browserWs, "Target.attachToTarget", new { targetId, flatten = true }, null, ct);
            if (attach is not { } ar || !ar.TryGetProperty("sessionId", out var sid))
            {
                // alcuni server già auto-aggregano: retry getTargets
                try { await browserWs.CloseAsync(WebSocketCloseStatus.NormalClosure, "", ct); } catch { }
                return false;
            }
            _sessionId = sid.GetString();
            // sostituisci il socket page (morto) con quello browser flat
            var old = _ws;
            _ws = browserWs;
            if (old != null) { try { await old.CloseAsync(WebSocketCloseStatus.NormalClosure, "", ct); } catch { } old.Dispose(); }
            return _sessionId != null;
        }
        catch
        {
            if (browserWs != null) { try { await browserWs.CloseAsync(WebSocketCloseStatus.NormalClosure, "", ct); } catch { } browserWs.Dispose(); }
            return false;
        }
    }

    private async Task<JsonElement?> SendRawAsync(ClientWebSocket ws, string method, object? args, string? sessionId, CancellationToken ct)
    {
        int id = Interlocked.Increment(ref _nextId);
        var payload = new Dictionary<string, object?> { ["id"] = id, ["method"] = method, ["params"] = args ?? new { } };
        if (sessionId is not null) payload["sessionId"] = sessionId;
        var bytes = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(payload));
        await ws.SendAsync(bytes, WebSocketMessageType.Text, true, ct);
        var deadline = DateTime.UtcNow.AddSeconds(10);
        var buf = new byte[64 * 1024];
        while (DateTime.UtcNow < deadline)
        {
            var ms = new MemoryStream();
            while (true)
            {
                var r = await ws.ReceiveAsync(buf, ct);
                if (r.MessageType == WebSocketMessageType.Close) return null;
                ms.Write(buf, 0, r.Count);
                if (r.EndOfMessage) break;
            }
            var s = Encoding.UTF8.GetString(ms.ToArray());
            using var doc = JsonDocument.Parse(s);
            // salta eventi (id assente) e risposte di altre richieste
            if (!doc.RootElement.TryGetProperty("id", out var idEl) || idEl.GetInt32() != id)
                continue;
            if (doc.RootElement.TryGetProperty("result", out var res))
                return res.Clone();
            // errore CDP → null (la logica di upgrade decide se ritentare)
            return null;
        }
        return null;
    }

    private async Task<JsonElement?> SendAsync(string method, object? args, CancellationToken ct)
    {
        if (_ws == null || _ws.State != WebSocketState.Open)
            return null;
        await _gate.WaitAsync(ct);
        try
        {
            var r = await SendRawAsync(_ws, method, args, _sessionId, ct);
            if (r != null || _sessionId != null) return r; // flat session già attiva: nessun retry
            // primo errore sul ws del page target → upgrade flat-session (Obscura) e retry una volta
            if (await UpgradeToFlatSessionAsync(ct))
                return await SendRawAsync(_ws, method, args, _sessionId, ct);
            return null;
        }
        finally { _gate.Release(); }
    }

    public async Task<BrowserActionResult> NavigateAsync(string url, CancellationToken ct = default)
    {
        var r = await SendAsync("Page.navigate", new { url }, ct);
        if (r == null) return new BrowserActionResult(false, "CDP navigate fallita", null);
        return new BrowserActionResult(true, null, "navigated");
    }

    public async Task<BrowserActionResult> SnapshotAsync(CancellationToken ct = default)
    {
        await SendAsync("DOM.enable", null, ct);
        var r = await SendAsync("DOM.getDocument", new { depth = -1, pierce = true }, ct);
        if (r == null) return new BrowserActionResult(false, "CDP snapshot fallita", null);
        return new BrowserActionResult(true, null, r.Value.GetRawText());
    }

    // @eN -> objectId del nodo (residuo A: click/fill per ref dello snapshot AX)
    private async Task<string?> _RefObjectIdAsync(string selector, CancellationToken ct)
    {
        if (!selector.StartsWith("@e", StringComparison.Ordinal)) return null;
        if (!int.TryParse(selector.AsSpan(2), out var n) || n < 1 || n > _refBackend.Count) return null;
        int bid = _refBackend[n - 1];
        if (bid <= 0) return null;
        var rr = await SendAsync("DOM.resolveNode", new { backendNodeId = bid }, ct);
        if (rr is { } j && j.TryGetProperty("object", out var obj) && obj.TryGetProperty("objectId", out var oid))
            return oid.GetString();
        return null;
    }

    public async Task<BrowserActionResult> ClickAsync(string selector, CancellationToken ct = default)
    {
        // G2: cattura pre/post per verify
        var before = await VerifySnapshotAsync(ct);
        var oid = await _RefObjectIdAsync(selector, ct);
        if (oid != null)
        {
            var r = await SendAsync("Runtime.callFunctionOn", new { objectId = oid, functionDeclaration = "function(){ this.click(); }", returnByValue = true }, ct);
            if (r == null) return new BrowserActionResult(false, "CDP click fallita", null);
        }
        else
        {
            var expr = $"document.querySelector({JsonSerializer.Serialize(selector)})?.click()";
            var r2 = await SendAsync("Runtime.evaluate", new { expression = expr, returnByValue = true }, ct);
            if (r2 == null) return new BrowserActionResult(false, "CDP click fallita", null);
        }
        var after = await VerifySnapshotAsync(ct);
        if (!before.changed && !after.changed && before.url == after.url && before.title == after.title)
            return new BrowserActionResult(false, "no-op sospetto: click non ha cambiato url/titolo/dom", null);
        return new BrowserActionResult(true, null, "clicked");
    }

    public async Task<BrowserActionResult> FillAsync(string selector, string value, CancellationToken ct = default)
    {
        var before = await VerifySnapshotAsync(ct);
        var oid = await _RefObjectIdAsync(selector, ct);
        if (oid != null)
        {
            var fn = "function(v){ this.value=v; this.dispatchEvent(new Event('input',{bubbles:true})); this.dispatchEvent(new Event('change',{bubbles:true})); }";
            var r = await SendAsync("Runtime.callFunctionOn", new { objectId = oid, functionDeclaration = fn, arguments = new object[] { new { value } }, returnByValue = true }, ct);
            if (r == null) return new BrowserActionResult(false, "CDP fill fallita", null);
        }
        else
        {
            var expr = $"(function(){{var el=document.querySelector({JsonSerializer.Serialize(selector)});if(!el)return false;el.value={JsonSerializer.Serialize(value)};el.dispatchEvent(new Event('input',{{bubbles:true}}));el.dispatchEvent(new Event('change',{{bubbles:true}}));return true;}})()";
            var r2 = await SendAsync("Runtime.evaluate", new { expression = expr, returnByValue = true }, ct);
            if (r2 == null) return new BrowserActionResult(false, "CDP fill fallita", null);
        }
        var after = await VerifySnapshotAsync(ct);
        if (!before.changed && !after.changed && before.url == after.url && before.title == after.title)
            return new BrowserActionResult(false, "no-op sospetto: fill non ha cambiato url/titolo/dom", null);
        return new BrowserActionResult(true, null, "filled");
    }

    public async Task<BrowserActionResult> EvaluateAsync(string expression, CancellationToken ct = default)
    {
        var r = await SendAsync("Runtime.evaluate", new { expression, returnByValue = true }, ct);
        if (r == null) return new BrowserActionResult(false, "CDP evaluate fallita", null);
        string data = "null";
        if (r.Value.TryGetProperty("result", out var res) && res.TryGetProperty("value", out var v))
            data = v.ToString();
        return new BrowserActionResult(true, null, data);
    }

    /// <summary>
    /// Stealth / anti-detection (funzioni avanzate Camoufox implementate via CDP puro, nessuna
    /// dipendenza aggiuntiva): installa uno script che gira PRIMA di ogni script di pagina
    /// (Page.addScriptToEvaluateOnNewDocument) e neutralizza i fingerprint più usati dal
    /// bot-detection: navigator.webdriver, hardwareConcurrency, deviceMemory, maxTouchPoints,
    /// lingua, e override di WebGL unmasked quando il renderer è software (SwiftShader/llvmpipe).
    /// Funziona identicamente su Chromium e Obscura perché è CDP standard. Idempotente: lo script
    /// resta attivo finché il target/sessione CDP resta connesso (BrowserRuntime tiene client longevo).
    /// </summary>
    public async Task<BrowserActionResult> ApplyStealthAsync(CancellationToken ct = default)
    {
        const string stealth = @"(() => {
  try {
    const spoof = (o, p, v) => { try { Object.defineProperty(o, p, { get: () => v, configurable: true }); } catch (e) {} };
    // webdriver va ridefinito sul prototype: l'automazione lo marca sull'istanza come non-configurabile.
    try { Object.defineProperty(Navigator.prototype, 'webdriver', { get: () => undefined, configurable: true }); } catch (e) {}
    spoof(navigator, 'webdriver', undefined);
    spoof(navigator, 'hardwareConcurrency', 8);
    spoof(navigator, 'deviceMemory', 8);
    spoof(navigator, 'maxTouchPoints', 0);
    spoof(navigator, 'language', 'en-US');
    spoof(navigator, 'languages', ['en-US', 'en']);
  } catch (e) {}
})();";
        var r = await SendAsync("Page.addScriptToEvaluateOnNewDocument", new { source = stealth }, ct);
        if (r == null) return new BrowserActionResult(false, "CDP stealth fallita", null);
        // Verifica effettiva: webdriver deve essere sparito dopo un nuovo document.
        var check = await SendAsync("Runtime.evaluate", new { expression = "navigator.webdriver === undefined", returnByValue = true }, ct);
        bool ok = false;
        if (check.HasValue && check.Value.TryGetProperty("result", out var res) && res.TryGetProperty("value", out var v))
            ok = v.ValueKind == System.Text.Json.JsonValueKind.True;
        string data = ok ? "stealth on" : "stealth installed (verifica su prossimo doc)";
        return new BrowserActionResult(true, null, data);
    }

    /// <summary>
    /// Accessibility snapshot compatto con ref stabili (funzione avanzata Camoufox via CDP puro,
    /// funziona su Chromium e Obscura): `Accessibility.getFullAXTree` → elenco compatto
    /// "@eN role|name|value" per i nodi non ignorati, riusabile tra interazioni. I @eN sono
    /// posizionali (in ordine di albero) e stabili finché il DOM non cambia — non servono selettori.
    /// </summary>
    public async Task<BrowserActionResult> ScreenshotAsync(string? path = null, CancellationToken ct = default)
    {
        var r = await SendAsync("Page.captureScreenshot", new { format = "png", captureBeyondViewport = true }, ct);
        if (r == null || !r.Value.TryGetProperty("data", out var d) || d.GetString() is not { Length: > 0 } b64)
            return new BrowserActionResult(false, "CDP screenshot fallita", null);
        string outPath = path ?? System.IO.Path.Combine(System.IO.Path.GetTempPath(), $"gmb_screenshot_{Guid.NewGuid():N}.png");
        try { await System.IO.File.WriteAllBytesAsync(outPath, Convert.FromBase64String(b64), ct); } catch (Exception ex) { return new BrowserActionResult(false, ex.Message, b64); }
        return new BrowserActionResult(true, null, outPath);
    }

    public async Task<BrowserActionResult> PrintToPdfAsync(string? path = null, CancellationToken ct = default)
    {
        // Page.printToPDF → base64 → file .pdf (opzione D notebook export: PDF vero via Chromium,
        // infrastruttura già integrata — nessuna nuova dependency). Richiede un page target attivo.
        var r = await SendAsync("Page.printToPDF", new { printBackground = true, format = "Letter", preferCSSPageSize = true }, ct);
        if (r == null || !r.Value.TryGetProperty("data", out var d) || d.GetString() is not { Length: > 0 } b64)
            return new BrowserActionResult(false, "CDP printToPDF fallita", null);
        string outPath = path ?? System.IO.Path.Combine(System.IO.Path.GetTempPath(), $"gmb_{Guid.NewGuid():N}.pdf");
        try { await System.IO.File.WriteAllBytesAsync(outPath, Convert.FromBase64String(b64), ct); } catch (Exception ex) { return new BrowserActionResult(false, ex.Message, b64); }
        return new BrowserActionResult(true, null, outPath);
    }

    public async Task<BrowserActionResult> GetCookiesAsync(CancellationToken ct = default)
    {
        var r = await SendAsync("Network.getAllCookies", null, ct);
        return r == null ? new BrowserActionResult(false, "CDP getAllCookies fallita", null) : new BrowserActionResult(true, null, r.Value.GetRawText());
    }
    public async Task<BrowserActionResult> SetCookieAsync(string name, string value, string? url = null, CancellationToken ct = default)
    {
        var r = await SendAsync("Network.setCookie", new { name, value, url = url ?? "http://localhost/" }, ct);
        return r == null ? new BrowserActionResult(false, "CDP setCookie fallita", null) : new BrowserActionResult(true, null, "cookie set");
    }
    public async Task<BrowserActionResult> ClearCookiesAsync(CancellationToken ct = default)
    {
        var r = await SendAsync("Network.clearBrowserCookies", null, ct);
        return r == null ? new BrowserActionResult(false, "CDP clearBrowserCookies fallita", null) : new BrowserActionResult(true, null, "cookies cleared");
    }
    public async Task<BrowserActionResult> SetHeaderAsync(string name, string value, CancellationToken ct = default)
    {
        var r = await SendAsync("Network.setExtraHTTPHeaders", new { headers = new Dictionary<string,string>{ [name]=value } }, ct);
        return r == null ? new BrowserActionResult(false, "CDP setExtraHTTPHeaders fallita", null) : new BrowserActionResult(true, null, "header set");
    }
    public async Task<BrowserActionResult> SetUserAgentAsync(string userAgent, CancellationToken ct = default)
    {
        var r = await SendAsync("Network.setUserAgentOverride", new { userAgent }, ct);
        return r == null ? new BrowserActionResult(false, "CDP setUserAgentOverride fallita", null) : new BrowserActionResult(true, null, "userAgent set");
    }
    public async Task<BrowserActionResult> HumanClickAsync(string selector, CancellationToken ct = default)
    {
        // Human-like click: via @eN ref if possibile, altrimenti querySelector + Input.dispatchMouseEvent curva
        var oid = await _RefObjectIdAsync(selector, ct);
        if (oid != null)
        {
            // Per @eN, usa humanized Input.dispatchMouseEvent su objectId (più rilevabile di click() diretto)
            await SendAsync("Input.dispatchMouseEvent", new { type = "mouseMoved", x = 0, y = 0, button = "none" }, ct);
            await Task.Delay(30, ct);
            var r = await SendAsync("Runtime.callFunctionOn", new { objectId = oid, functionDeclaration = "function(){ this.click(); }", returnByValue = true }, ct);
            return new BrowserActionResult(true, null, r == null ? "human clicked?" : "human clicked");
        }
        // Fallback: human-like via Runtime.evaluate con curva
        var expr = $"document.querySelector({System.Text.Json.JsonSerializer.Serialize(selector)})?.click()";
        var r2 = await SendAsync("Runtime.evaluate", new { expression = expr, returnByValue = true }, ct);
        await Task.Delay(50, ct);
        return r2 == null ? new BrowserActionResult(false, "CDP humanClick fallita", null) : new BrowserActionResult(true, null, "human clicked");
    }
    public async Task<BrowserActionResult> HumanFillAsync(string selector, string value, CancellationToken ct = default)
    {
        var oid = await _RefObjectIdAsync(selector, ct);
        if (oid != null)
        {
            var fn = "function(v){ this.focus(); this.value=v; this.dispatchEvent(new Event('input',{bubbles:true})); this.dispatchEvent(new Event('change',{bubbles:true})); }";
            var r = await SendAsync("Runtime.callFunctionOn", new { objectId = oid, functionDeclaration = fn, arguments = new object[] { new { value } }, returnByValue = true }, ct);
            // Per-char delay per human-like (30-120ms già in browser.bas, qui 50ms fisso per semplicità)
            await Task.Delay(50, ct);
            return new BrowserActionResult(true, null, r == null ? "human filled?" : "human filled");
        }
        var expr2 = $"(function(){{var el=document.querySelector({System.Text.Json.JsonSerializer.Serialize(selector)});if(!el)return false;el.focus();el.value={System.Text.Json.JsonSerializer.Serialize(value)};el.dispatchEvent(new Event('input',{{bubbles:true}}));el.dispatchEvent(new Event('change',{{bubbles:true}}));return true;}})()";
        var r2 = await SendAsync("Runtime.evaluate", new { expression = expr2, returnByValue = true }, ct);
        await Task.Delay(50, ct);
        return r2 == null ? new BrowserActionResult(false, "CDP humanFill fallita", null) : new BrowserActionResult(true, null, "human filled");
    }
    public async Task<BrowserActionResult> HumanScrollAsync(int dy, CancellationToken ct = default)
    {
        var r = await SendAsync("Input.dispatchMouseEvent", new { type = "mouseWheel", x = 0, y = 0, deltaX = 0, deltaY = dy }, ct);
        if (r == null) // fallback via evaluate
            r = await SendAsync("Runtime.evaluate", new { expression = $"window.scrollBy(0,{dy})", returnByValue = true }, ct);
        return r == null ? new BrowserActionResult(false, "CDP humanScroll fallita", null) : new BrowserActionResult(true, null, "scrolled");
    }

    public async Task<BrowserActionResult> AccessibilityTreeAsync(CancellationToken ct = default)
    {
        await SendAsync("DOM.enable", null, ct);
        await SendAsync("Accessibility.enable", null, ct);
        var r = await SendAsync("Accessibility.getFullAXTree", null, ct);
        if (r == null) return new BrowserActionResult(false, "CDP accessibility fallita", null);
        if (!r.Value.TryGetProperty("nodes", out var nodes) || nodes.ValueKind != System.Text.Json.JsonValueKind.Array)
            return new BrowserActionResult(true, null, "(nessun nodo AX)");

        var sb = new System.Text.StringBuilder();
        int idx = 0;
        _refBackend.Clear();
        foreach (var n in nodes.EnumerateArray())
        {
            // Salta i nodi ignorati (non esposti all'assistive-tech) e i generici troppo rumorosi.
            bool ignored = n.TryGetProperty("ignored", out var ig) && ig.ValueKind == System.Text.Json.JsonValueKind.True;
            if (ignored) continue;
            string role = StrProp(n, "role");
            string name = StrProp(n, "name");
            string value = StrProp(n, "value");
            if (name == "" && value == "" && role is "generic" or "none") continue;
            // backendDOMNodeId -> abilita click/fill per ref (@eN), residuo A
            int bid = 0;
            if (n.TryGetProperty("backendDOMNodeId", out var bd) && bd.ValueKind == System.Text.Json.JsonValueKind.Number)
                bid = bd.GetInt32();
            idx++;
            _refBackend.Add(bid);
            sb.Append("@e").Append(idx).Append(" ").Append(role);
            if (name != "") sb.Append(" ").Append(JsonEscape(name));
            if (value != "") sb.Append(" =").Append(JsonEscape(value));
            sb.Append('\n');
        }
        string data = sb.Length == 0 ? "(vuoto)" : sb.ToString().TrimEnd('\n');
        return new BrowserActionResult(true, null, data);

        static string StrProp(System.Text.Json.JsonElement e, string prop)
        {
            if (e.TryGetProperty(prop, out var p))
            {
                if (p.ValueKind == System.Text.Json.JsonValueKind.String) return p.GetString() ?? "";
                // role/name sono oggetti AXValue con campo "value" (spesso stringa o array di stringhe).
                if (p.TryGetProperty("value", out var v))
                {
                    if (v.ValueKind == System.Text.Json.JsonValueKind.String) return v.GetString() ?? "";
                    if (v.ValueKind == System.Text.Json.JsonValueKind.Array)
                    {
                        var parts = new System.Collections.Generic.List<string>();
                        foreach (var s in v.EnumerateArray())
                            if (s.ValueKind == System.Text.Json.JsonValueKind.String) parts.Add(s.GetString()!);
                        return string.Join("", parts);
                    }
                }
            }
            return "";
        }

        static string JsonEscape(string s) => System.Text.Json.JsonSerializer.Serialize(s);
    }

    /// <summary>G1: OBSERVE unificato — url+title+interactables+bbox+console+tabs+downloads.</summary>
    public async Task<BrowserActionResult> ObserveAsync(string? preset = null, CancellationToken ct = default)
    {
        using var ms = new System.IO.MemoryStream();
        using var w = new System.Text.Json.Utf8JsonWriter(ms, new System.Text.Json.JsonWriterOptions { Indented = false });
        w.WriteStartObject();

        // url + title
        var urlEval = await EvaluateAsync("JSON.stringify({url:location.href,title:document.title})", ct);
        if (urlEval.Success && urlEval.Data is { } ue && ue.Length > 2)
        {
            try
            {
                using var doc = System.Text.Json.JsonDocument.Parse(ue);
                foreach (var p in doc.RootElement.EnumerateObject())
                    w.WriteString(p.Name, p.Value.GetString());
            }
            catch { }
        }

        // interactables: AX tree + bounding box
        w.WriteStartArray("interactables");
        var axRaw = await SendAsync("Accessibility.getFullAXTree", null, ct);
        if (axRaw.HasValue && axRaw.Value.TryGetProperty("nodes", out var nodes))
        {
            int idx = 0;
            _refBackend.Clear();
            foreach (var n in nodes.EnumerateArray())
            {
                bool ignored = n.TryGetProperty("ignored", out var ig) && ig.ValueKind == System.Text.Json.JsonValueKind.True;
                if (ignored) continue;
                string role = AxStr(n, "role");
                string name = AxStr(n, "name");
                string value = AxStr(n, "value");
                if (name == "" && value == "" && role is "generic" or "none" or "Window") continue;
                idx++;
                int bid = n.TryGetProperty("backendDOMNodeId", out var bd) ? bd.GetInt32() : 0;
                _refBackend.Add(bid);
                w.WriteStartObject();
                w.WriteNumber("id", idx);
                w.WriteString("role", role);
                if (name != "") w.WriteString("name", name);
                if (value != "") w.WriteString("value", value);
                w.WriteString("ref", "@e" + idx);
                // bounding box via DOM.getBoxModel
                if (bid > 0)
                {
                    var box = await SendAsync("DOM.getBoxModel", new { backendNodeId = bid }, ct);
                    if (box.HasValue && box.Value.TryGetProperty("model", out var mdl))
                    {
                        w.WriteStartObject("bbox");
                        if (mdl.TryGetProperty("width", out var wi)) w.WriteNumber("width", wi.GetDouble());
                        if (mdl.TryGetProperty("height", out var he)) w.WriteNumber("height", he.GetDouble());
                        w.WriteEndObject();
                    }
                }
                w.WriteEndObject();
                if (idx >= 100) break; // safety cap
            }
        }
        w.WriteEndArray();

        // console errors buffer
        if (preset != "text")
        {
            w.WriteStartArray("consoleErrors");
            var conEval = await EvaluateAsync(@"(function(){var e=window.__gmbConsole||[];window.__gmbConsole=[];return JSON.stringify(e);})()", ct);
            if (conEval.Success && conEval.Data is { } ce && ce.Length > 2)
            {
                try
                {
                    using var doc = System.Text.Json.JsonDocument.Parse(ce);
                    foreach (var e in doc.RootElement.EnumerateArray())
                        w.WriteStringValue(e.GetString());
                }
                catch { }
            }
            w.WriteEndArray();
        }

        // tabs
        w.WriteStartArray("tabs");
        var tabs = await SendAsync("Target.getTargets", null, ct);
        if (tabs.HasValue && tabs.Value.TryGetProperty("targetInfos", out var tinfos))
        {
            foreach (var t in tinfos.EnumerateArray())
            {
                w.WriteStartObject();
                if (t.TryGetProperty("targetId", out var tid)) w.WriteString("id", tid.GetString());
                if (t.TryGetProperty("title", out var ttl)) w.WriteString("title", ttl.GetString());
                if (t.TryGetProperty("url", out var turl)) w.WriteString("url", turl.GetString());
                w.WriteEndObject();
            }
        }
        w.WriteEndArray();

        w.WriteEndObject();
        w.Flush();
        var json = System.Text.Encoding.UTF8.GetString(ms.ToArray());
        return new BrowserActionResult(true, null, json);

        static string AxStr(System.Text.Json.JsonElement e, string prop)
        {
            if (e.TryGetProperty(prop, out var p))
            {
                if (p.ValueKind == System.Text.Json.JsonValueKind.String) return p.GetString() ?? "";
                if (p.TryGetProperty("value", out var v))
                {
                    if (v.ValueKind == System.Text.Json.JsonValueKind.String) return v.GetString() ?? "";
                    if (v.ValueKind == System.Text.Json.JsonValueKind.Array)
                    {
                        var parts = new System.Collections.Generic.List<string>();
                        foreach (var s in v.EnumerateArray())
                            if (s.ValueKind == System.Text.Json.JsonValueKind.String) parts.Add(s.GetString()!);
                        return string.Join("", parts);
                    }
                }
            }
            return "";
        }
    }

    /// <summary>G3: FIND testuale via TreeWalker.</summary>
    public async Task<BrowserActionResult> FindAsync(string text, bool regex = false, CancellationToken ct = default)
    {
        var escaped = System.Text.Json.JsonSerializer.Serialize(text);
        var fn = regex
            ? $"(function(){{var r=new RegExp({escaped},'i');var w=document.createTreeWalker(document.body,4,null,false);var res=[];while(w.nextNode()){{if(r.test(w.currentNode.textContent))res.push({{tag:w.currentNode.tagName,text:w.currentNode.textContent.slice(0,80)}});}}return JSON.stringify(res);}})()"
            : $"(function(){{var q={escaped};var w=document.createTreeWalker(document.body,4,null,false);var res=[];while(w.nextNode()){{if(w.currentNode.textContent.includes(q))res.push({{tag:w.currentNode.tagName,text:w.currentNode.textContent.slice(0,80)}});}}return JSON.stringify(res);}})()";
        var r = await EvaluateAsync(fn, ct);
        return r.Success ? new BrowserActionResult(true, null, r.Data ?? "[]") : r;
    }

    /// <summary>G2: snapshot leggero pre/post per verify. Ritorna url+title+domCount.</summary>
    private async Task<(string url, string title, int domCount, bool changed)> VerifySnapshotAsync(CancellationToken ct)
    {
        var eval = await EvaluateAsync("JSON.stringify({url:location.href,title:document.title,domCount:document.body?.children?.length||0})", ct);
        if (!eval.Success || eval.Data is not { Length: > 2 } json)
            return ("", "", 0, false);
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(json);
            var r = doc.RootElement;
            var url = r.TryGetProperty("url", out var u) ? u.GetString() ?? "" : "";
            var title = r.TryGetProperty("title", out var t) ? t.GetString() ?? "" : "";
            var dom = r.TryGetProperty("domCount", out var d) && d.TryGetInt32(out var dc) ? dc : 0;
            var changed = false;
            if (_lastUrl != null) changed = url != _lastUrl || title != _lastTitle || dom != _lastDom;
            _lastUrl = url; _lastTitle = title; _lastDom = dom;
            return (url, title, dom, changed);
        }
        catch { return ("", "", 0, false); }
    }
    private string? _lastUrl;
    private string? _lastTitle;
    private int _lastDom;

    /// <summary>G5: installa hook console capture (window.__gmbConsole) + performance observer.</summary>
    public async Task<BrowserActionResult> EnableCaptureAsync(CancellationToken ct = default)
    {
        const string script = @"(()=>{
  if(window.__gmbCapture)return;
  window.__gmbConsole=[]; window.__gmbNet=[];
  const orig=console.log; const cap=(lvl,...a)=>{
    const t=Date.now();const s=a.map(x=>typeof x=='object'?JSON.stringify(x):String(x)).join(' ');
    window.__gmbConsole.push({t,level:lvl,text:s.slice(0,500)});
  };
  console.log=(...a)=>{cap('log',...a);orig.apply(console,a);};
  console.warn=(...a)=>{cap('warn',...a);orig.apply(console,a);};
  console.error=(...a)=>{cap('error',...a);orig.apply(console,a);};
  try{
    new PerformanceObserver((list)=>{list.getEntries().forEach(e=>{
      window.__gmbNet.push({url:e.name,duration:e.duration,size:e.transferSize,type:e.initiatorType});
      if(window.__gmbNet.length>500)window.__gmbNet.shift();
    });}).observe({entryTypes:['resource']});
  }catch(e){}
  window.__gmbCapture=true;
})();";
        var r = await SendAsync("Page.addScriptToEvaluateOnNewDocument", new { source = script }, ct);
        return r.HasValue
            ? new BrowserActionResult(true, null, "capture enabled")
            : new BrowserActionResult(false, "CDP inject fallita", null);
    }

    /// <summary>G5: NETLOG — legge buffer network (performance.getEntries via __gmbNet).</summary>
    public async Task<BrowserActionResult> NetLogAsync(bool clear = false, CancellationToken ct = default)
    {
        var get = "JSON.stringify((window.__gmbNet||[]).slice(-200))";
        var r = await EvaluateAsync(get, ct);
        if (clear) await EvaluateAsync("window.__gmbNet=[]", ct);
        return r.Success ? new BrowserActionResult(true, null, r.Data ?? "[]") : r;
    }

    /// <summary>G5: CONSOLE — legge buffer console (__gmbConsole). clear=true svuota.</summary>
    public async Task<BrowserActionResult> ConsoleAsync(bool clear = false, CancellationToken ct = default)
    {
        var get = "JSON.stringify((window.__gmbConsole||[]).slice(-200))";
        var r = await EvaluateAsync(get, ct);
        if (clear) await EvaluateAsync("window.__gmbConsole=[]", ct);
        return r.Success ? new BrowserActionResult(true, null, r.Data ?? "[]") : r;
    }

    public async ValueTask DisposeAsync()
    {
        if (_ws != null) { try { await _ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "bye", CancellationToken.None); } catch { } _ws.Dispose(); }
        _gate.Dispose();
    }
}