// FILE: REPL/Notebook/NotebookServer.cs
// NotebookServer — Fase 1 (piano Interactive-Notebook-Plan §4): trasporto HTTP del kernel
// notebook. Espone execute/result/error/introspection come JSON sopra HttpListener, lo stesso
// pattern già in produzione di McpHttpListenerHost (bind 127.0.0.1, loop accept, JSON).
//
// Protocollo (tutte le risposte: application/json; UTF-8):
//   GET  /health          → { "ok": true }
//   POST /execute         { "cellId": "...", "source": "..." }   → CellResult JSON
//   POST /vars            {}                                        → { "vars": [ {name, value} ] }
//   POST /reset           {}                                        → { "ok": true }
//   POST /shutdown        {}                                        → { "ok": true } (arresta il loop)
//
// Modello di concorrenza: UN kernel = UN notebook = UNA sessione, servita in SERIE dal loop
// (il kernel usa statici per-processo di IlRuntime — piano §5: multi-kernel = multi-processo;
// qui dentro NON c'è parallelismo di sessione, per costruzione). Il body è letto intero,
// eseguito, risposto: niente streaming in Fase 1 (le celle AI con streaming sono Fase 2).

using System.Net;
using System.Text;
using System.Text.Json;

namespace GMBasic.Notebook;

public sealed class NotebookServer : IDisposable
{
    private static readonly JsonSerializerOptions JsonOpts = new() { PropertyNameCaseInsensitive = true };
    private readonly NotebookKernel _kernel;
    private readonly HttpListener _listener = new();
    private readonly CancellationTokenSource _cts = new();
    private Task? _loop;
    private readonly int _port;

    public int Port => _port;

    public NotebookServer(int port = 0, string? autosavePath = null)
    {
        _port = port;
        _kernel = new NotebookKernel();
        if (!string.IsNullOrEmpty(autosavePath))
            _kernel.AutosavePath = autosavePath;
        _listener.Prefixes.Add($"http://127.0.0.1:{port}/");
    }

    /// <summary>Abilita/disabilita l'autosave .gmnb sul kernel del server.</summary>
    public string? AutosavePath
    {
        get => _kernel.AutosavePath;
        set => _kernel.AutosavePath = value;
    }

    /// <summary>Avvia il loop accept. Ritorna quando il server è in ascolto.</summary>
    public void Start()
    {
        _listener.Start();
        _loop = Task.Run(() => LoopAsync(_cts.Token));
    }

    public async Task StopAsync()
    {
        _cts.Cancel();
        try { _listener.Stop(); } catch { /* già fermato */ }
        if (_loop != null) { try { await _loop; } catch { /* loop interrotto */ } }
    }

    private async Task LoopAsync(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            HttpListenerContext ctx;
            try { ctx = await _listener.GetContextAsync(); }
            catch (Exception) when (ct.IsCancellationRequested) { break; }
            catch { break; }
            try
            {
                await HandleAsync(ctx, ct);
            }
            catch (Exception ex)
            {
                TryWriteJson(ctx, 500, JsonSerializer.Serialize(new { ok = false, error = ex.Message }));
            }
            finally
            {
                try { ctx.Response.Close(); } catch { }
            }
        }
    }

    private async Task HandleAsync(HttpListenerContext ctx, CancellationToken ct)
    {
        var path = ctx.Request.Url?.AbsolutePath ?? "/";
        var method = ctx.Request.HttpMethod;

        if (method == "GET" && path == "/")
        {
            WriteHtml(ctx, 200, FrontendHtml);
            return;
        }

        if (path == "/health" && method == "GET")
        {
            WriteJson(ctx, 200, JsonSerializer.Serialize(new { ok = true }));
            return;
        }

        // /cells è introspezione in sola lettura: ammessa anche in GET (il body non serve)
        if (path == "/cells" && method == "GET")
        {
            WriteJsonCells(ctx);
            return;
        }

        if (method != "POST")
        {
            WriteJson(ctx, 405, JsonSerializer.Serialize(new { ok = false, error = "POST only" }));
            return;
        }

        string body;
        using (var reader = new StreamReader(ctx.Request.InputStream, ctx.Request.ContentEncoding))
            body = await reader.ReadToEndAsync(ct);

        switch (path)
        {
            case "/execute":
                var req = JsonSerializer.Deserialize<ExecuteRequest>(body, JsonOpts);
                if (req == null || req.CellId == null || req.Source == null)
                {
                    WriteJson(ctx, 400, JsonSerializer.Serialize(
                        new { ok = false, error = "body: { cellId, source } obbligatori" }));
                    return;
                }
                var r = _kernel.Execute(req.CellId, req.Source);
                WriteJson(ctx, 200, JsonSerializer.Serialize(new
                {
                    ok = r.Ok,
                    error = r.Error,
                    output = r.Output,
                    value = r.Value,
                    valueFormatted = CellValueToString(r.Value),
                    compileMs = r.CompileMs.TotalMilliseconds,
                    executeMs = r.ExecuteMs.TotalMilliseconds,
                    fromCache = r.FromCache,
                    autosaved = r.Autosaved,
                }));
                return;

            case "/vars":
                WriteJson(ctx, 200, JsonSerializer.Serialize(new
                {
                    vars = _kernel.Vars().Select(v => new { name = v.Name, value = CellValueToString(v.Value) }),
                }));
                return;

            case "/ai":
                // Streaming %ai via SSE (text/event-stream): { prompt } → "data: <chunk>" per
                // ogni chunk del completer (IStreamingLlmCompleter) + "data: [DONE]". Il
                // frontend legge il flusso e aggiorna la cella token-per-token.
                var aiReq = JsonSerializer.Deserialize<AiRequest>(body, JsonOpts);
                var prompt = aiReq?.Prompt;
                if (string.IsNullOrWhiteSpace(prompt))
                {
                    WriteJson(ctx, 400, JsonSerializer.Serialize(new { ok = false, error = "body: { prompt } obbligatorio" }));
                    return;
                }
                try
                {
                    ctx.Response.ContentType = "text/event-stream";
                    ctx.Response.ContentEncoding = Encoding.UTF8;
                    var outStream = ctx.Response.OutputStream;
                    _kernel.AiStream(prompt, chunk =>
                    {
                        var payload = $"data: {chunk.Replace("\n", " ")}\n\n";
                        var bytes = Encoding.UTF8.GetBytes(payload);
                        outStream.Write(bytes, 0, bytes.Length);
                        outStream.Flush();
                    });
                    var done = Encoding.UTF8.GetBytes("data: [DONE]\n\n");
                    outStream.Write(done, 0, done.Length);
                    outStream.Flush();
                }
                catch (Exception ex)
                {
                    TryWriteJson(ctx, 200, JsonSerializer.Serialize(new { ok = false, error = ex.Message }));
                }
                return;

            case "/cells":
                // celle note al kernel in ordine di definizione + dipendenze (per il frontend)
                WriteJsonCells(ctx);
                return;

            case "/order":
                // ordine topologico di esecuzione (%order) — le celle che leggono un nome
                // vanno dopo quelle che lo scrivono
                WriteJson(ctx, 200, JsonSerializer.Serialize(new { order = _kernel.Order() }));
                return;

            case "/runall":
                // esegue TUTTE le celle note in ordine %order; risposte per cella in ordine
                var results = _kernel.RunAll();
                WriteJson(ctx, 200, JsonSerializer.Serialize(new
                {
                    results = results.Select(x => new
                    {
                        cellId = x.CellId,
                        ok = x.Result.Ok,
                        error = x.Result.Error,
                        output = x.Result.Output,
                        value = x.Result.Value,
                        valueFormatted = CellValueToString(x.Result.Value),
                        compileMs = x.Result.CompileMs.TotalMilliseconds,
                        executeMs = x.Result.ExecuteMs.TotalMilliseconds,
                        fromCache = x.Result.FromCache,
                    }),
                }));
                return;

            case "/reset":
                _kernel.Reset();
                WriteJson(ctx, 200, JsonSerializer.Serialize(new { ok = true }));
                return;

            case "/shutdown":
                WriteJson(ctx, 200, JsonSerializer.Serialize(new { ok = true }));
                _ = Task.Run(async () => { await Task.Delay(100); await StopAsync(); });
                return;

            default:
                WriteJson(ctx, 404, JsonSerializer.Serialize(new { ok = false, error = "unknown path " + path }));
                return;
        }
    }

    private void WriteJsonCells(HttpListenerContext ctx)
    {
        WriteJson(ctx, 200, JsonSerializer.Serialize(new
        {
            cells = _kernel.Cells().Select(c => new
            {
                cellId = c.CellId, source = c.Source,
                reads = c.Reads, writes = c.Writes,
            }),
        }));
    }

    private static string CellValueToString(object? v) => v switch
    {
        null => "NIL",
        bool b => b ? "TRUE" : "FALSE",
        double d => d.ToString("G15", System.Globalization.CultureInfo.InvariantCulture),
        _ => v.ToString() ?? ""
    };

    private static void WriteJson(HttpListenerContext ctx, int status, string json)
    {
        ctx.Response.StatusCode = status;
        ctx.Response.ContentType = "application/json";
        ctx.Response.ContentEncoding = Encoding.UTF8;
        var bytes = Encoding.UTF8.GetBytes(json);
        ctx.Response.ContentLength64 = bytes.Length;
        ctx.Response.OutputStream.Write(bytes);
    }

    private static void WriteHtml(HttpListenerContext ctx, int status, string html)
    {
        ctx.Response.StatusCode = status;
        ctx.Response.ContentType = "text/html; charset=utf-8";
        ctx.Response.ContentEncoding = Encoding.UTF8;
        var bytes = Encoding.UTF8.GetBytes(html);
        ctx.Response.ContentLength64 = bytes.Length;
        ctx.Response.OutputStream.Write(bytes);
    }

    private static void TryWriteJson(HttpListenerContext ctx, int status, string json)
    {
        try { WriteJson(ctx, status, json); } catch { /* client andato via */ }
    }

    /// <summary>Frontend web minimale (Fase 1): celle impilate, Ctrl+Enter per eseguire la
    /// cella corrente, run-all in ordine %order (pulsante ▶), Reset. Parla con gli endpoint
    /// HTTP del kernel (POST /execute, /reset) — nessuna dipendenza JS esterna.</summary>
    internal const string FrontendHtml = """
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8"><title>GMBASIC Notebook</title>
<style>
  body { font: 14px/1.4 ui-monospace, Menlo, monospace; margin: 0; background:#1e1e2e; color:#cdd6f4; }
  header { padding: 8px 14px; background:#313244; display:flex; gap:10px; align-items:center; position:sticky; top:0; }
  header h1 { font-size:14px; margin:0; color:#a6e3a1; }
  button { background:#45475a; color:#cdd6f4; border:1px solid #585b70; border-radius:4px;
           padding:3px 10px; cursor:pointer; font:inherit; }
  button:hover { background:#585b70; }
  #status { color:#a6adc8; font-size:12px; }
  main { padding: 14px; display:flex; flex-direction:column; gap:12px; max-width:900px; margin:0 auto; }
  .cell { background:#313244; border:1px solid #45475a; border-radius:6px; overflow:hidden; }
  .cell .src { width:100%; box-sizing:border-box; border:0; background:transparent; color:#cdd6f4;
               padding:10px; min-height:44px; resize:vertical; outline:none; font:inherit; white-space:pre; }
  .cell .src:focus { box-shadow: inset 2px 0 0 #89b4fa; }
  .cell .out { padding:8px 12px; border-top:1px solid #45475a; white-space:pre-wrap;
               background:#1e1e2e; color:#a6e3a1; display:none; }
  .cell .out.show { display:block; }
  .cell .out.err { color:#f38ba8; }
  .cell .meta { padding:3px 12px; font-size:11px; color:#a6adc8; background:#27273a; }
  .cellrow { display:flex; gap:6px; align-items:flex-start; }
  .idx { color:#585b70; padding-top:10px; width:34px; text-align:right; user-select:none; }
  #add { align-self:flex-start; }
</style>
</head>
<body>
<header>
  <h1>GMBASIC · Notebook</h1>
  <button id="runall" title="Esegui tutte le celle in ordine di dipendenza">▶ Run all</button>
  <button id="add">+ Cella</button>
  <button id="reset">Reset</button>
  <span id="status"></span>
</header>
<main id="cells"></main>
<script>
const $ = s => document.querySelector(s);
let seq = 0;

function el(tag, cls, html) { const e = document.createElement(tag); if (cls) e.className = cls; if (html) e.innerHTML = html; return e; }

async function api(path, body) {
  const r = await fetch(path, body ? { method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify(body) } : {});
  return r.json();
}

async function run(cellRow, id, src) {
  const out = cellRow.querySelector('.out');
  const meta = cellRow.querySelector('.meta');
  out.classList.remove('err'); out.classList.add('show'); out.textContent = '…';
  meta.textContent = '';
  try {
    // celle %ai: streaming via POST /ai (SSE) — aggiorna l'output token-per-token
    const first = (src || '').trimStart().split(/\s+/)[0] || '';
    if (first === '%ai' || first === '%ai:') {
      const prompt = src.replace(first, '').trim();
      const res = await fetch('/ai', { method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({ prompt }) });
      if (!res.ok || !res.body) { out.classList.add('err'); out.textContent = '✗ ' + res.status; return; }
      out.textContent = '';
      const reader = res.body.getReader(); const dec = new TextDecoder();
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        const text = dec.decode(value, { stream: true });
        for (const line of text.split('\n')) {
          if (!line.startsWith('data: ')) continue;
          const data = line.slice(6);
          if (data === '[DONE]') break;
          out.textContent += data;
        }
      }
      meta.textContent = 'streaming ✓';
      return;
    }
    const res = await api('/execute', { cellId: id, source: src });
    if (res.ok) {
      out.textContent = res.output || (res.valueFormatted && res.valueFormatted !== 'NIL' ? '= ' + res.valueFormatted : '✓');
      meta.textContent = (res.compileMs + res.executeMs).toFixed(1) + ' ms' + (res.fromCache ? ' [cached]' : '');
    } else {
      out.classList.add('err'); out.textContent = '✗ ' + (res.error || 'errore');
    }
  } catch (e) { out.classList.add('err'); out.textContent = '✗ ' + e.message; }
}

function addCell(source, id) {
  const row = el('div', 'cellrow');
  const idx = el('span', 'idx', String(++seq));
  const cell = el('div', 'cell');
  const ta = el('textarea', 'src'); ta.value = source || '';
  ta.placeholder = 'PRINT "ciao"   —  x = 42   —  x * 2   —  %sql SELECT 1';
  const out = el('div', 'out');
  const meta = el('div', 'meta');
  cell.append(ta, out, meta); row.append(idx, cell); $('#cells').append(row);
  const cid = id || 'c' + seq;
  row.dataset.cid = cid;
  ta.addEventListener('keydown', async (ev) => {
    if (ev.key === 'Enter' && (ev.ctrlKey || ev.metaKey)) { ev.preventDefault(); await run(row, cid, ta.value); }
    else if (ev.key === 'Enter' && ev.shiftKey) { ev.preventDefault(); ta.value += '\n'; }
  });
  return { row, cid, ta };
}

async function runAll() {
  $('#status').textContent = 'reset…';
  await api('/reset', {});
  // semantica Jupyter "Restart & Run All": esegui ogni cella UNA volta in ordine di
  // definizione (DOM). Il run-all TOPOLOGICO (%order) è POST /runall per i client API
  // (ri-esegue le celle già note al kernel nell'ordine di dipendenza).
  const rows = [...document.querySelectorAll('.cellrow')];
  for (let k = 0; k < rows.length; k++) {
    const ta = rows[k].querySelector('.src');
    const cid = rows[k].dataset.cid || ('c' + (k + 1));
    await run(rows[k], cid, ta.value);
  }
  $('#status').textContent = rows.length + ' celle eseguite ✓';
}

addCell(); // prima cella vuota
$('#add').onclick = () => addCell();
$('#reset').onclick = async () => { const r = await api('/reset', {}); $('#status').textContent = 'reset ✓'; };
$('#runall').onclick = runAll;
</script>
</body>
</html>
""";

    private sealed record ExecuteRequest(string? CellId, string? Source);
    private sealed record AiRequest(string? Prompt);

    public void Dispose()
    {
        _cts.Cancel();
        try { _listener.Stop(); } catch { }
        _kernel.Dispose();
        _cts.Dispose();
    }
}
