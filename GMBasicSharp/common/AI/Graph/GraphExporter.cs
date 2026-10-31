// FILE: Basic.AI.Graph.GraphExporter
// Export del grafo wiki in mermaid (graph TD), GraphML (XML, yEd/Gephi) e JSON (tooling),
// più stats.json (nodi/edge/dangling/top-hub/PageRank). Riusabile da REPL/CLI/BASIC.
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace Basic.AI.Graph;

/// <summary>Risultato di un export grafo.</summary>
public sealed record GraphExport(string Mermaid, string Graphml, string Json, string StatsJson, string? Html = null, string? Report = null);

public static class GraphExporter
{
    /// <summary>Genera tutti i formati di export + le statistiche. Se <paramref name="withHtml"/>
    /// è true genera anche graph.html (viewer interattivo autonomo). Con <paramref name="louvain"/>
    /// true le community usano Louvain (modularity) invece del label propagation.</summary>
    public static GraphExport Export(WikiGraph g, string name, double pageRankDamping = 0.85, int pageRankIterations = 100, bool withHtml = false, bool louvain = false)
    {
        return new GraphExport(
            ToMermaid(g, name),
            ToGraphml(g, name),
            ToJson(g, louvain: louvain),
            ToStatsJson(g, name, pageRankDamping, pageRankIterations, louvain: louvain),
            withHtml ? ToHtml(g, name, louvain: louvain) : null,
            ToReport(g, name, pageRankDamping, pageRankIterations, louvain));
    }

    public static string ToMermaid(WikiGraph g, string name)
    {
        var sb = new StringBuilder();
        sb.Append("graph TD\n");
        foreach (var node in g.Nodes.Values)
            sb.Append($"    N{Sanitize(node.Path)}[\"{Escape(node.Title)}\"]\n");
        foreach (var e in g.Edges)
            sb.Append($"    N{Sanitize(e.From)} --> N{Sanitize(e.To)}\n");
        return sb.ToString();
    }

    public static string ToGraphml(WikiGraph g, string name)
    {
        var sb = new StringBuilder();
        sb.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        sb.Append("<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n");
        sb.Append("  <key id=\"title\" for=\"node\" attr.name=\"title\" attr.type=\"string\"/>\n");
        sb.Append("  <graph id=\"G\" edgedefault=\"directed\">\n");
        foreach (var node in g.Nodes.Values)
            sb.Append($"    <node id=\"{XmlEscape(node.Path)}\"><data key=\"title\">{XmlEscape(node.Title)}</data></node>\n");
        foreach (var e in g.Edges)
            sb.Append($"    <edge source=\"{XmlEscape(e.From)}\" target=\"{XmlEscape(e.To)}\"/>\n");
        sb.Append("  </graph>\n</graphml>\n");
        return sb.ToString();
    }

    /// <summary>Community del grafo, label propagation (default) o Louvain (modularity).</summary>
    private static Dictionary<string, int> CommunitiesOf(WikiGraph g, bool louvain) =>
        louvain ? g.ComputeCommunitiesLouvain() : g.ComputeCommunities();

    public static string ToJson(WikiGraph g, bool louvain = false)
    {
        var communities = CommunitiesOf(g, louvain);
        var degrees = g.DegreeCentrality();
        return JsonSerializer.Serialize(new
        {
            nodes = g.Nodes.Values.Select(n => new
            {
                id = n.Path,
                title = n.Title,
                frontmatter = n.Frontmatter,
                community = communities.GetValueOrDefault(n.Path),
                degree = degrees.GetValueOrDefault(n.Path),
            }),
            edges = g.Edges.Select(e => new { from = e.From, to = e.To, alias = e.Alias, type = e.Type, provenance = e.Provenance, confidence = e.GetConfidence() }),
            dangling = g.Dangling,
            folderCounts = g.FolderCounts,
        }, JsonOpts);
    }

    /// <summary>Dot (Graphviz) — versione pesata: ogni edge porta type/weight quando presenti.</summary>
    public static string ToDot(WikiGraph g, string name)
    {
        var sb = new StringBuilder();
        sb.AppendLine($"digraph {Sanitize(name)} {{");
        sb.AppendLine("    rankdir=LR;");
        foreach (var node in g.Nodes.Values)
            sb.Append($"    \"{Escape(node.Path)}\" [label=\"{Escape(node.Title)}\"];\n");
        foreach (var e in g.Edges)
            sb.Append($"    \"{Escape(e.From)}\" -> \"{Escape(e.To)}\" [label=\"{Escape(e.Alias ?? "")}\"];\n");
        sb.AppendLine("}");
        return sb.ToString();
    }

    /// <summary>Mermaid del sottografo a distanza ≤ depth dal nodo radice (BFS).</summary>
    public static string ToSubgraphMermaid(WikiGraph g, string root, int depth = 1)
    {
        var sb = new StringBuilder();
        sb.AppendLine("graph TD");
        if (!g.Nodes.ContainsKey(root)) return sb.ToString();
        var visited = new HashSet<string>(StringComparer.Ordinal) { root };
        var frontier = new Queue<string>();
        frontier.Enqueue(root);
        sb.Append($"    N{Sanitize(root)}[\"{Escape(g.Nodes[root].Title)}\"]\n");
        for (var d = 0; d < depth && frontier.Count > 0; d++)
        {
            var next = new Queue<string>();
            while (frontier.Count > 0)
            {
                var cur = frontier.Dequeue();
                foreach (var nb in g.Neighbors(cur))
                {
                    if (!visited.Add(nb)) continue;
                    next.Enqueue(nb);
                    sb.Append($"    N{Sanitize(nb)}[\"{Escape(g.Nodes[nb].Title)}\"]\n");
                    sb.Append($"    N{Sanitize(cur)} --> N{Sanitize(nb)}\n");
                }
            }
            frontier = next;
        }
        return sb.ToString();
    }

    /// <summary>JSON dei nodi che hanno il tag dato (match case-insensitive) + i loro edge.</summary>
    public static string ToJsonByTag(WikiGraph g, string tag)
    {
        var matches = g.Nodes.Values
            .Where(n => n.Frontmatter.TryGetValue("tags", out var t)
                && t.Any(x => string.Equals(x, tag, StringComparison.OrdinalIgnoreCase)))
            .Select(n => n.Path)
            .ToHashSet(StringComparer.Ordinal);
        return JsonSerializer.Serialize(new
        {
            tag,
            nodes = g.Nodes.Values.Where(n => matches.Contains(n.Path))
                .Select(n => new { id = n.Path, title = n.Title }),
            edges = g.Edges.Where(e => matches.Contains(e.From) && matches.Contains(e.To))
                .Select(e => new { from = e.From, to = e.To }),
            count = matches.Count,
        }, JsonOpts);
    }

    /// <summary>JSON dei nodi nella cartella data (o sotto) + i loro edge. Folder vuoto =
    /// tutti i nodi (root).</summary>
    public static string ToJsonByFolder(WikiGraph g, string folder)
    {
        var f = folder?.TrimEnd('/') ?? "";
        var matches = g.Nodes.Values
            .Where(n => f.Length == 0 || n.Path == f || n.Path.StartsWith(f + "/", StringComparison.OrdinalIgnoreCase))
            .Select(n => n.Path)
            .ToHashSet(StringComparer.Ordinal);
        return JsonSerializer.Serialize(new
        {
            folder,
            nodes = g.Nodes.Values.Where(n => matches.Contains(n.Path))
                .Select(n => new { id = n.Path, title = n.Title }),
            edges = g.Edges.Where(e => matches.Contains(e.From) && matches.Contains(e.To))
                .Select(e => new { from = e.From, to = e.To }),
            count = matches.Count,
        }, JsonOpts);
    }

    /// <summary>True se il grafo ha almeno un nodo (validità di base per un export).</summary>
    public static bool IsValid(WikiGraph g) => g is not null && g.NodeCount > 0;

    public static string ToStatsJson(WikiGraph g, string name, double pageRankDamping = 0.85, int pageRankIterations = 100, bool louvain = false)
    {
        var pagerank = g.PageRank(pageRankDamping, pageRankIterations);
        var communities = CommunitiesOf(g, louvain);
        var degrees = g.DegreeCentrality();
        var betweenness = g.BetweennessCentrality();
        var communityCount = communities.Values.Distinct().Count();
        return JsonSerializer.Serialize(new
        {
            name,
            nodes = g.NodeCount,
            edges = g.EdgeCount,
            dangling = g.Dangling.Count,
            communities = communityCount,
            topHubs = g.Edges.GroupBy(e => e.From).OrderByDescending(x => x.Count()).Take(10)
                .Select(x => new { node = x.Key, degree = x.Count() }),
            pageRankTop = pagerank.Take(10).Select(x => new { node = x.Node, rank = Math.Round(x.Rank, 6) }),
            degreeTop = degrees.OrderByDescending(kv => kv.Value).Take(10)
                .Select(x => new { node = x.Key, degree = x.Value }),
            betweennessTop = betweenness.OrderByDescending(kv => kv.Value).Take(10)
                .Select(x => new { node = x.Key, betweenness = Math.Round(x.Value, 4) }),
            edgeTypes = g.Edges.GroupBy(e => e.Type).ToDictionary(x => x.Key, x => x.Count()),
            edgeProvenance = g.Edges.GroupBy(e => e.Provenance).ToDictionary(x => x.Key, x => x.Count()),
            godNodes = g.GodNodes(10, pageRankIterations)
                .Select(x => new { node = x.Node, score = Math.Round(x.Score, 4), degree = Math.Round(x.Degree, 3), pagerank = Math.Round(x.Pagerank, 3), betweenness = Math.Round(x.Betweenness, 3) }),
            surprisingConnections = g.SurprisingConnections(communities, 10)
                .Select(x => new { from = x.From, to = x.To, commFrom = x.CommFrom, commTo = x.CommTo }),
            suggestedQuestions = g.SuggestedQuestions(10),
            folders = g.FolderCounts.Count,
        }, JsonOpts);
    }

    /// <summary>Viewer HTML interattivo e AUTONOMO del grafo (nessuna dipendenza esterna,
    /// nessun server): il JSON è embedded nello script. Force-directed su canvas con:
    /// zoom/pan, hover=tooltip, click=dettaglio (frontmatter/community/degree), colori per
    /// community, filtro per folder/tag, ricerca. Parità con graph.html di graphify ma in-file.
    /// Apri in browser: file://…/graph.html  (o `GRAPH VIEW`).</summary>
    public static string ToHtml(WikiGraph g, string name, bool louvain = false)
    {
        var json = ToJson(g, louvain: louvain).Replace("</", "<\\/");   // safe per embedding in </script>
        var title = Escape(name);
        // insights (god nodes, sorprese, domande) calcolati qui e iniettati al viewer
        var gods = g.GodNodes(6);
        var commH = CommunitiesOf(g, louvain);
        var insightJson = JsonSerializer.Serialize(new
        {
            gods = gods.Select(x => new { node = x.Node, score = Math.Round(x.Score, 3) }),
            surprising = g.SurprisingConnections(commH, 6)
                .Select(x => new { from = x.From, to = x.To }),
            questions = g.SuggestedQuestions(6),
        }, JsonOpts).Replace("</", "<\\/");
        return $$"""
        <!DOCTYPE html>
        <html lang="it">
        <head>
        <meta charset="utf-8">
        <title>Graph {{title}}</title>
        <style>
          :root { color-scheme: light dark; }
          body { margin:0; font-family: system-ui, -apple-system, sans-serif; background:#0f1117; color:#e6e6e6; }
          #wrap { position:fixed; inset:0; }
          canvas { display:block; width:100%; height:100%; }
          #sidebar { position:fixed; top:0; left:0; bottom:0; width:300px; background:rgba(15,17,23,.92);
                      border-right:1px solid #2a2e3a; overflow-y:auto; padding:14px; box-sizing:border-box; z-index:10; }
          #main { position:fixed; top:0; left:300px; right:0; bottom:0; }
          #search { width:100%; box-sizing:border-box; padding:8px; margin-bottom:10px; border-radius:6px;
                     border:1px solid #3a3f4d; background:#1a1d27; color:#eee; }
          h3 { margin:12px 0 6px; font-size:13px; text-transform:uppercase; color:#8b93a7; }
          .stat { font-size:12px; color:#aab; margin:2px 0; }
          .filters label { display:block; font-size:12px; margin:3px 0; cursor:pointer; }
          .filters input { margin-right:6px; }
          #detail { position:fixed; right:12px; bottom:12px; max-width:360px; background:#1a1d27;
                     border:1px solid #3a3f4d; border-radius:8px; padding:12px; display:none; z-index:20; font-size:12px; }
          #detail h4 { margin:0 0 6px; }
          #detail pre { white-space:pre-wrap; margin:0; max-height:200px; overflow:auto; }
          #controls { position:fixed; top:10px; left:310px; z-index:15; }
          #controls button { margin-right:6px; padding:6px 10px; border-radius:6px; border:1px solid #3a3f4d;
                              background:#1a1d27; color:#eee; cursor:pointer; }
          #legend { margin-top:10px; font-size:11px; }
          #legend .sw { display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px; }
        </style>
        </head>
        <body>
        <div id="sidebar">
          <h3>Graph: {{title}}</h3>
          <input id="search" placeholder="Cerca nodo… (invio per centro)">
          <div class="stat" id="stat-nodes"></div>
          <div class="stat" id="stat-edges"></div>
          <div class="stat" id="stat-comms"></div>
          <h3>Filtro folder</h3>
          <div class="filters" id="folder-filters"></div>
          <h3>Filtro edge</h3>
          <div class="filters" id="type-filters"></div>
          <div id="legend"></div>
          <h3>Hub (god nodes)</h3>
          <div class="insights" id="insights-gods" style="font-size:12px"></div>
          <h3>Connessioni sorprendenti</h3>
          <div class="insights" id="insights-surprising" style="font-size:12px"></div>
          <h3>Da esplorare</h3>
          <div class="insights" id="insights-questions" style="font-size:12px"></div>
        </div>
        <div id="main"><canvas id="cv"></canvas></div>
        <div id="controls">
          <button id="btn-comm">Vista community</button>
          <button id="btn-degree">Vista degree</button>
          <button id="btn-reset">Reset</button>
        </div>
        <div id="detail"></div>
        <script>
        const DATA = {{json}};
        const INSIGHT = {{insightJson}};
        const NODES = new Map(DATA.nodes.map(n => [n.id, { ...n, x: Math.random()*2-1, y: Math.random()*2-1, vx:0, vy:0 }]));
        const EDGES = DATA.edges.filter(e => NODES.has(e.from) && NODES.has(e.to));
        const COLORS = ["#4fc3f7","#ff8a65","#81c784","#ffd54f","#ba68c8","#4dd0e1","#f06292","#aed581","#7986cb","#ffb74d","#4db6ac","#e57373","#90a4ae","#dce775","#9575cd"];
        let mode = "community";
        const communities = [...new Set(DATA.nodes.map(n => n.community ?? 0))];
        const folderOf = id => { const i = id.lastIndexOf("/"); return i>0 ? id.slice(0,i) : "(root)"; };
        const folders = [...new Set(DATA.nodes.map(n => folderOf(n.id)))].sort();
        const edgeTypes = [...new Set(EDGES.map(e => e.type))];
        let hiddenFolders = new Set(), hiddenTypes = new Set();
        function nodeColor(n) {
          if (mode === "community") return COLORS[(n.community ?? 0) % COLORS.length];
          const d = n.degree ?? 0; const m = Math.max(1, ...DATA.nodes.map(x => x.degree ?? 0));
          const t = Math.min(1, d/m); const r = Math.round(255-150*t); const g = Math.round(80+150*t);
          return `rgb(${r},${g},80)`;
        }
        function isHidden(n) { return hiddenFolders.has(folderOf(n.id)); }
        // ── Layout deterministico: nodi per community su cerchi (leggibile sempre,
        //    niente fisica instabile che ammassa i nodi al centro lasciando solo le edge) ──
        function layout() {
          // raggruppa per community
          const groups = new Map();
          for (const n of DATA.nodes) { const c = n.community ?? 0; if (!groups.has(c)) groups.set(c, []); groups.get(c).push(n); }
          const comms = [...groups.keys()].sort((a,b)=>a-b);
          // centro dei nodi di una community = su un anello; i nodi su un cerchietto attorno
          const R1 = 0.36, R2 = Math.min(0.24, 0.08 + 0.013*Math.sqrt(Math.max(...comms.map(c=>groups.get(c).length), 8)));
          comms.forEach((c, i) => {
            const ang = (2*Math.PI*i)/comms.length;
            const ccx = R1*Math.cos(ang), ccy = R1*Math.sin(ang);
            const ns = groups.get(c);
            ns.forEach((n, j) => {
              if (ns.length === 1) { n.x = ccx; n.y = ccy; return; }
              const a2 = (2*Math.PI*j)/ns.length;
              n.x = ccx + R2*Math.cos(a2);
              n.y = ccy + R2*Math.sin(a2);
            });
          });
        }
        layout();
        // ── Camera zoom/pan (come Obsidian): lo spazio mondo è normalizzato; la camera ha
        //    scale + offset. Wheel = zoom attorno al cursore; drag vuoto = pan; drag nodo = muovi.
        //    I TITOLI compaiono solo quando il nodo a schermo supera una soglia (zoom abbastanza
        //    alto), altrimenti il grafo resterebbe sporco di testo sovrapposto. ──
        function bounds() {
          let minX=1e9,minY=1e9,maxX=-1e9,maxY=-1e9;
          for (const n of NODES.values()) { if (isHidden(n)) continue; minX=Math.min(minX,n.x);maxX=Math.max(maxX,n.x);minY=Math.min(minY,n.y);maxY=Math.max(maxY,n.y); }
          return {minX,minY,maxX,maxY};
        }
        function fitToScreen() {
          const W=cv.width, H=cv.height; const b=bounds(); const pad=60;
          const sx=(b.maxX-b.minX)<1e-6?1:(W-2*pad)/Math.max(1e-6,(b.maxX-b.minX));
          const sy=(b.maxY-b.minY)<1e-6?1:(H-2*pad)/Math.max(1e-6,(b.maxY-b.minY));
          const scale=Math.min(sx,sy);
          const ox=(W-(b.maxX-b.minX)*scale)/2 - b.minX*scale;
          const oy=(H-(b.maxY-b.minY)*scale)/2 - b.minY*scale;
          return {scale, ox, oy};
        }
        function size() { const m=document.getElementById('main'); const el=document.getElementById('cv');
          el.width=m.clientWidth; el.height=m.clientHeight; return el; }
        let cv = size();
        const ctx = cv.getContext('2d');
        let fitScale = 1;
        function setCam(c){ cam = c; }
        let cam = fitToScreen();
        fitScale = cam.scale;              // scala a vista completa (px per unità mondo)
        function toScreen(x,y){ return [cam.ox + x*cam.scale, cam.oy + y*cam.scale]; }
        function toWorld(mx,my){ return [(mx-cam.ox)/cam.scale, (my-cam.oy)/cam.scale]; }
        function zoomAt(mx,my,factor){
          const [wx,wy]=toWorld(mx,my);
          const maxS = Math.max(fitScale*200, 2000);
          cam.scale = Math.max(fitScale*0.2, Math.min(maxS, cam.scale*factor));
          cam.ox = mx - wx*cam.scale;
          cam.oy = my - wy*cam.scale;
        }
        // raggio del nodo in PIXEL SCHERMO (dimensione costante a schermo, indipendente dallo zoom)
        function nodeRadius(n){ return Math.min(14, Math.max(3.5, 3 + Math.sqrt(n.degree ?? 1))); }
        let drag = null;   // {kind:'node'|'pan', node?, sx, sy, ox0, oy0, wx0, wy0}
        function hitNode(mx,my){
          for (const n of NODES.values()) {
            if (isHidden(n)) continue;
            const [px,py]=toScreen(n.x,n.y); const rr=nodeRadius(n);
            if (Math.hypot(mx-px,my-py) < rr+4) return n;
          }
          return null;
        }
        function draw(ctx, W, H) {
          ctx.clearRect(0,0,W,H);
          ctx.save();
          ctx.translate(cam.ox, cam.oy);
          ctx.scale(cam.scale, cam.scale);
          // edge in spazio mondo: spessore costante a schermo (1/scale)
          const lw = Math.max(0.5, 1.2)/cam.scale;
          for (const e of EDGES) {
            const a=NODES.get(e.from), b=NODES.get(e.to); if(!a||!b) continue;
            if (isHidden(a)||isHidden(b)) continue; if (hiddenTypes.has(e.type)) continue;
            ctx.strokeStyle = "rgba(150,165,210,0.30)"; ctx.lineWidth = lw;
            ctx.beginPath(); ctx.moveTo(a.x,a.y); ctx.lineTo(b.x,b.y); ctx.stroke();
          }
          // nodi + titoli a soglia (raggio in unità mondo = pixelSchermo/scale → costante a schermo)
          ctx.textAlign = "left"; ctx.textBaseline = "middle";
          for (const n of NODES.values()) {
            if (isHidden(n)) continue;
            const screenR = nodeRadius(n);
            const rw = screenR/cam.scale;          // raggio mondo per canvas.scale
            ctx.fillStyle = nodeColor(n);
            ctx.beginPath(); ctx.arc(n.x,n.y,rw,0,7); ctx.fill();
            ctx.strokeStyle = "rgba(0,0,0,0.4)"; ctx.lineWidth = lw;
            ctx.stroke();
            // titolo SOLO quando siamo ingranditi abbastanza rispetto alla vista completa;
            // font espresso in PIXEL SCHERMO diviso lo scale (il ctx è già scalato), così il
            // testo resta leggibile e NON diventa enorme quando si zoomma
            const zoomRel = cam.scale / fitScale;
            if (zoomRel >= 1.8) {
              const label = (n.title || n.id);
              const fs = Math.max(10, Math.min(20, 12 + 1.5*zoomRel));
              ctx.font = `${(fs/cam.scale).toFixed(6)}px system-ui`;
              ctx.fillStyle = "rgba(230,235,245,0.95)";
              ctx.fillText(label.slice(0,60), n.x + rw + 5/cam.scale, n.y);
            }
          }
          ctx.restore();
        }
        function resetView(){ cam=fitToScreen(); fitScale=cam.scale; render(); }
        function render(){ draw(ctx, cv.width, cv.height); }
        window.addEventListener('resize', ()=>{ cv=size(); resetView(); });
        resetView();
        // zoom rotellina (attorno al cursore)
        cv.addEventListener('wheel', e => {
          e.preventDefault();
          const r=cv.getBoundingClientRect(); const mx=e.clientX-r.left, my=e.clientY-r.top;
          const factor = e.deltaY < 0 ? 1.18 : 1/1.18;
          zoomAt(mx,my,factor); render();
        }, { passive:false });
        cv.addEventListener('mousedown', e => {
          const r=cv.getBoundingClientRect(); const mx=e.clientX-r.left, my=e.clientY-r.top;
          const n = hitNode(mx,my);
          if (n) drag = { kind:'node', node:n, mx, my, wx:n.x, wy:n.y };
          else drag = { kind:'pan', mx, my, ox:cam.ox, oy:cam.oy };
        });
        cv.addEventListener('mousemove', e => {
          const r=cv.getBoundingClientRect(); const mx=e.clientX-r.left, my=e.clientY-r.top;
          if (!drag) { cv.style.cursor = hitNode(mx,my) ? 'pointer' : 'default'; return; }
          const dx=(mx-drag.mx)/cam.scale, dy=(my-drag.my)/cam.scale;
          if (drag.kind === 'node') { drag.node.x = drag.wx + dx; drag.node.y = drag.wy + dy; }
          else { cam.ox = drag.ox + (mx-drag.mx); cam.oy = drag.oy + (my-drag.my); }
          render();
        });
        cv.addEventListener('mouseup', ()=>{ drag=null; });
        cv.addEventListener('mouseleave', ()=>{ drag=null; });
        document.addEventListener('keydown', e => {   // F = fit-to-screen
          if (e.key === 'f' || e.key === 'F') resetView();
        });
        // sidebar
        document.getElementById('stat-nodes').textContent = `nodi: ${DATA.nodes.length}`;
        document.getElementById('stat-edges').textContent = `edge: ${EDGES.length}`;
        document.getElementById('stat-comms').textContent = `community: ${communities.length}`;
        // ── Insight: hub (god nodes), connessioni sorprendenti, domande da esplorare ──
        const short = id => { const n = NODES.get(id); const t = (n && n.title) || id; return t.length>42 ? t.slice(0,39)+"…" : t; };
        function focusNode(id) {
          const n = NODES.get(id); if (!n) return;
          cam.scale = fitScale * 6; const W=cv.width, H=cv.height;
          cam.ox = W/2 - n.x*cam.scale; cam.oy = H/2 - n.y*cam.scale;
          const fm = n.frontmatter ? Object.entries(n.frontmatter).map(([k,v])=>k+': '+(Array.isArray(v)?v.join(', '):v)).join('<br>') : '';
          document.getElementById('detail').innerHTML = `<h4>${n.title||n.id}</h4><div><b>degree:</b> ${n.degree}</div>${fm?'<pre>'+fm+'</pre>':''}`;
          document.getElementById('detail').style.display='block';
          render();
        }
        const godsEl = document.getElementById('insights-gods');
        if (INSIGHT && INSIGHT.gods && INSIGHT.gods.length) {
          INSIGHT.gods.forEach(g => {
            const a=document.createElement('a'); a.href='#'; a.style.display='block'; a.style.color='#7ec8ff';
            a.style.textDecoration='none'; a.style.margin='2px 0';
            a.textContent = `★ ${short(g.node)}`;
            a.title = `score composito ${g.score}`;
            a.onclick = ()=>{ focusNode(g.node); return false; };
            godsEl.appendChild(a);
          });
        }
        const surEl = document.getElementById('insights-surprising');
        if (INSIGHT && INSIGHT.surprising && INSIGHT.surprising.length) {
          INSIGHT.surprising.forEach(s => {
            const a=document.createElement('a'); a.href='#'; a.style.display='block'; a.style.color='#ffd57e';
            a.style.textDecoration='none'; a.style.margin='2px 0';
            a.textContent = `${short(s.from)} ⇄ ${short(s.to)}`;
            a.onclick = ()=>{ focusNode(s.from); return false; };
            surEl.appendChild(a);
          });
        }
        const qEl = document.getElementById('insights-questions');
        if (INSIGHT && INSIGHT.questions && INSIGHT.questions.length) {
          INSIGHT.questions.slice(0,4).forEach(q => {
            const d=document.createElement('div'); d.style.margin='3px 0'; d.style.color='#aab';
            d.textContent = q.length>70 ? q.slice(0,67)+"…" : q; qEl.appendChild(d);
          });
        }
        folders.forEach(f => { const l=document.createElement('label');
          const cb=document.createElement('input'); cb.type='checkbox'; cb.checked=true;
          cb.onchange=()=>{ cb.checked?hiddenFolders.delete(f):hiddenFolders.add(f); render(); };
          l.appendChild(cb); l.appendChild(document.createTextNode(f)); document.getElementById('folder-filters').appendChild(l); });
        edgeTypes.forEach(t => { const l=document.createElement('label');
          const cb=document.createElement('input'); cb.type='checkbox'; cb.checked=true;
          cb.onchange=()=>{ cb.checked?hiddenTypes.delete(t):hiddenTypes.add(t); render(); };
          l.appendChild(cb); l.appendChild(document.createTextNode(t)); document.getElementById('type-filters').appendChild(l); });
        // legenda: una riga per community con etichetta = titolo del nodo hub (degree max) o del
        // folder prevalente, MAI "(root)"/codici — parità Obsidian. Es: "Trend Following (23)".
        const legend = document.getElementById('legend');
        const byComm = new Map();
        for (const n of DATA.nodes) { const c=n.community??0; if(!byComm.has(c)) byComm.set(c,[]); byComm.get(c).push(n); }
        byComm.forEach((nodes, c) => {
          // nodo hub: degree massimo (o primo se tutti degree 0)
          let hub = nodes[0];
          for (const n of nodes) if ((n.degree ?? 0) > (hub.degree ?? 0)) hub = n;
          // folder prevalente tra i nodi NON-root (fallback: titolo hub)
          const counts = new Map();
          for (const n of nodes) { const f=folderOf(n.id); if (f !== "(root)") counts.set(f,(counts.get(f)||0)+1); }
          let label;
          if (counts.size > 0) label = [...counts.entries()].sort((a,b)=>b[1]-a[1])[0][0];
          else label = (hub.title || hub.id);
          if (label.length > 48) label = label.slice(0,45) + "…";
          const row=document.createElement('div');
          row.style.display='flex'; row.style.alignItems='center'; row.style.gap='6px'; row.style.margin='2px 0';
          const s=document.createElement('span'); s.className='sw'; s.style.background=COLORS[c%COLORS.length];
          const txt=document.createElement('span');
          txt.textContent = `${label} (${nodes.length})`;
          txt.style.flex='1'; txt.style.overflow='hidden'; txt.style.textOverflow='ellipsis'; txt.style.whiteSpace='nowrap';
          txt.title = `community ${c}: ${label} — ${nodes.length} nodi`;
          row.appendChild(s); row.appendChild(txt); legend.appendChild(row);
        });
        document.getElementById('btn-comm').onclick=()=>{ mode='community'; render(); };
        document.getElementById('btn-degree').onclick=()=>{ mode='degree'; render(); };
        document.getElementById('btn-reset').onclick=()=>{ resetView(); };
        const det=document.getElementById('detail');
        cv.addEventListener('click', e => {
          const r=cv.getBoundingClientRect(); const n=hitNode(e.clientX-r.left, e.clientY-r.top);
          if (!n) { det.style.display='none'; return; }
          const fm = n.frontmatter ? Object.entries(n.frontmatter).map(([k,v])=>k+': '+(Array.isArray(v)?v.join(', '):v)).join('<br>') : '';
          det.innerHTML = `<h4>${n.title||n.id}</h4><div><b>id:</b> ${n.id}</div><div><b>community:</b> ${n.community}</div><div><b>degree:</b> ${n.degree}</div>${fm?'<pre>'+fm+'</pre>':''}`;
          det.style.display='block';
        });
        document.getElementById('search').addEventListener('keydown', e => {
          if (e.key !== 'Enter') return; const q=document.getElementById('search').value.trim().toLowerCase();
          for (const n of NODES.values()) {
            if (isHidden(n)) continue;
            if (n.id.toLowerCase().includes(q) || (n.title||'').toLowerCase().includes(q)) {
              // centra il nodo e porta lo zoom a ~6x la vista completa (labels visibili)
              cam.scale = fitScale * 6;
              const W=cv.width, H=cv.height;
              cam.ox = W/2 - n.x*cam.scale; cam.oy = H/2 - n.y*cam.scale;
              det.style.display='none'; render(); return;
            }
          }
        });
        </script>
        </body>
        </html>
        """;
    }



    /// <summary>Export del grafo come VAULT OBSIDIAN navigabile (parità graphify --obsidian):
    /// genera index.md (panoramica del grafo) + una nota per community in communities/, con
    /// wikilink [[...]] ai nodi membri e tra community. I file .md vanno aperti in Obsidian:
    /// i wikilink risolvono se le note originali sono nel vault, altrimenti restano come link
    /// testuali (Obsidian li mostra comunque nella graph view).</summary>
    public static void WriteObsidianVault(WikiGraph g, string name, string outDir)
    {
        Directory.CreateDirectory(outDir);
        Directory.CreateDirectory(Path.Combine(outDir, "communities"));
        var communities = g.ComputeCommunities();
        var degrees = g.DegreeCentrality();
        // membri per community (solo nodi file reali: path con .md)
        var byComm = new Dictionary<int, List<WikiNode>>();
        foreach (var n in g.Nodes.Values)
        {
            if (!n.Path.EndsWith(".md", StringComparison.OrdinalIgnoreCase)) continue;   // skip tag/pseudo
            var c = communities.GetValueOrDefault(n.Path);
            if (!byComm.TryGetValue(c, out var l)) byComm[c] = l = new List<WikiNode>();
            l.Add(n);
        }
        var commIds = byComm.Keys.OrderBy(x => x).ToList();
        var commLabel = new Dictionary<int, string>();
        foreach (var c in commIds)
        {
            var members = byComm[c];
            // 1) folder prevalente tra i membri che hanno una cartella
            var folder = members.Select(m => FolderOf(m.Path)).Where(f => f.Length > 0)
                .GroupBy(f => f).OrderByDescending(gr => gr.Count()).FirstOrDefault()?.Key;
            // 2) topic condiviso dai TITOLI (per vault piatti): parole non-stop presenti in >=
            //    max(2, 30%) dei titoli — es. la community "Trend Following" diventa "Trend Following".
            //    Il titolo vero è il frontmatter "title" (WikiNode.Title è solo il nome file).
            string? topic = null;
            if (folder is null && members.Count >= 2)
            {
                string DisplayTitle(WikiNode m) =>
                    m.Frontmatter.TryGetValue("title", out var t) && t.Count > 0 && !string.IsNullOrWhiteSpace(t[0])
                        ? t[0] : m.Title;
                var titles = members.Select(DisplayTitle).ToList();
                var freq = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
                foreach (var t in titles)
                    foreach (var w in TokenizeTitle(t))
                        freq[w] = freq.GetValueOrDefault(w) + 1;
                var threshold = Math.Max(2, (int)Math.Ceiling(members.Count * 0.3));
                var words = freq.Where(kv => kv.Value >= threshold)
                    .OrderByDescending(kv => kv.Value).ThenBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
                    .Take(3).Select(kv => kv.Key).ToList();
                if (words.Count > 0) topic = string.Join(" ", words);
            }
            var raw = folder ?? topic ?? Path.GetFileNameWithoutExtension(members
                .OrderByDescending(m => degrees.GetValueOrDefault(m.Path)).First().Path);
            commLabel[c] = raw.Length > 48 ? raw[..45] + "…" : raw;
        }
        // slug unici per i file community (dedup case-insensitive: su filesystem APFS case-
        // insensitive due label "Trading"/"trading" colliderebbero e uno sovrascriverebbe l'altro)
        var commSlug = new Dictionary<int, string>();
        var usedSlugs = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var c in commIds)
        {
            var slug = Slug(commLabel[c]);
            if (!usedSlugs.Add(slug)) { var i = 2; while (!usedSlugs.Add(slug + "-" + i)) i++; slug += "-" + i; }
            commSlug[c] = slug;
        }

        // ── index.md ──
        var sb = new StringBuilder();
        sb.AppendLine("---");
        sb.AppendLine($"title: \"Graph: {name}\"");
        sb.AppendLine($"type: graph");
        sb.AppendLine($"nodes: {g.NodeCount}");
        sb.AppendLine($"edges: {g.EdgeCount}");
        sb.AppendLine($"communities: {commIds.Count}");
        sb.AppendLine("---");
        sb.AppendLine($"# Graph: {name}");
        sb.AppendLine();
        sb.AppendLine($"- **Nodi**: {g.NodeCount}");
        sb.AppendLine($"- **Edge**: {g.EdgeCount}");
        sb.AppendLine($"- **Community**: {commIds.Count}");
        sb.AppendLine($"- **Dangling link**: {g.Dangling.Count}");
        sb.AppendLine();
        sb.AppendLine("## Community");
        sb.AppendLine();
        foreach (var c in commIds)
            sb.AppendLine($"- [[{commSlug[c]}]] — {commLabel[c]} ({byComm[c].Count} note)");
        File.WriteAllText(Path.Combine(outDir, "index.md"), sb.ToString());

        // ── una nota per community ──
        foreach (var c in commIds)
        {
            var label = commLabel[c];
            var members = byComm[c].OrderByDescending(m => degrees.GetValueOrDefault(m.Path)).ToList();
            var hub = members[0];
            var hubName = Path.GetFileNameWithoutExtension(hub.Path);
            var b = new StringBuilder();
            b.AppendLine("---");
            b.AppendLine($"title: \"{EscapeMd(label)}\"");
            b.AppendLine($"type: graph-community");
            b.AppendLine($"nodes: {members.Count}");
            b.AppendLine("---");
            b.AppendLine($"# {label}");
            b.AppendLine();
            b.AppendLine($"Community del grafo **{name}** con {members.Count} note.");
            b.AppendLine();
            b.AppendLine($"- **Hub**: [[{hubName}]]");
            b.AppendLine();
            b.AppendLine("## Note");
            b.AppendLine();
            foreach (var m in members)
            {
                var n = Path.GetFileNameWithoutExtension(m.Path);
                var tags = m.Frontmatter.TryGetValue("tags", out var t) && t.Count > 0 ? $" — {string.Join(", ", t.Select(tag => $"#{tag}"))}" : "";
                b.AppendLine($"- [[{n}]]{tags}");
            }
            b.AppendLine();
            b.AppendLine("## Link tra community");
            b.AppendLine();
            var linksOut = members.SelectMany(m => g.Edges.Where(e => e.From == m.Path && e.Type == "link").Select(e => e.To))
                .Where(t => communities.TryGetValue(t, out var tc) && tc != c)
                .Select(t => communities[t]).Distinct().OrderBy(x => x).ToList();
            if (linksOut.Count == 0) b.AppendLine("_Nessun collegamento esterno._");
            else foreach (var tc in linksOut) b.AppendLine($"- [[{commSlug[tc]}]] — {commLabel[tc]}");
            File.WriteAllText(Path.Combine(outDir, "communities", commSlug[c] + ".md"), b.ToString());
        }
    }

    /// <summary>Report plain-language del grafo in Markdown (parità graphify GRAPH_REPORT.md):
    /// Summary (nodi/edge/community/provenance), Community Hubs (hub di navigazione), God Nodes
    /// (core abstractions), Surprising Connections (ponti tra community), Suggested Questions e
    /// le community principali con etichette leggibili. Nessuna dipendenza LLM: è derivato dalle
    /// metriche calcolate (stesso contenuto di stats.json, in forma leggibile per un umano).</summary>
    public static string ToReport(WikiGraph g, string name, double pageRankDamping = 0.85, int pageRankIterations = 100, bool louvain = false)
    {
        var communities = CommunitiesOf(g, louvain);
        var degrees = g.DegreeCentrality();
        var pagerank = g.PageRank(pageRankDamping, pageRankIterations);
        var commCount = communities.Values.Distinct().Count();
        var byProvenance = g.Edges.GroupBy(e => e.Provenance).ToDictionary(x => x.Key, x => x.Count());

        string NodeTitle(string path)
        {
            if (g.Nodes.TryGetValue(path, out var n))
            {
                if (n.Frontmatter.TryGetValue("title", out var t) && t.Count > 0 && !string.IsNullOrWhiteSpace(t[0]))
                    return t[0];
                if (!string.IsNullOrWhiteSpace(n.Title)) return n.Title;
            }
            return Path.GetFileNameWithoutExtension(path.Contains('/') ? path[(path.LastIndexOf('/') + 1)..] : path);
        }
        string Short(string path, int max = 60)
        {
            var s = NodeTitle(path);
            if (s.Length <= max) return s;
            var cut = Math.Max(1, max - 1);
            return cut >= s.Length ? s : s[..cut] + "…";
        }

        var sb = new StringBuilder();
        sb.AppendLine($"# Graph Report — {name}");
        sb.AppendLine();
        sb.AppendLine("## Summary");
        sb.AppendLine($"- {g.NodeCount} nodi · {g.EdgeCount} edge · {commCount} community · {g.Dangling.Count} dangling link");
        sb.AppendLine($"- Provenance: {Percent(byProvenance.GetValueOrDefault("EXTRACTED"), g.EdgeCount)}% EXTRACTED · " +
                      $"{Percent(byProvenance.GetValueOrDefault("INFERRED"), g.EdgeCount)}% INFERRED · " +
                      $"{Percent(byProvenance.GetValueOrDefault("AMBIGUOUS"), g.EdgeCount)}% AMBIGUOUS" +
                      (byProvenance.GetValueOrDefault("AMBIGUOUS") > 0
                        ? " (edge dedotti con fiducia bassa, da rivedere)"
                        : ""));
        sb.AppendLine();

        // Community Hubs (navigazione): i nodi più linkati, etichettati come hub per community
        sb.AppendLine("## Community Hubs (navigazione)");
        var hubPerComm = new Dictionary<int, string>();
        foreach (var n in g.Nodes.Keys)
        {
            if (!n.EndsWith(".md", StringComparison.OrdinalIgnoreCase)) continue;
            var c = communities.GetValueOrDefault(n);
            if (!hubPerComm.TryGetValue(c, out var cur) || degrees.GetValueOrDefault(n) > degrees.GetValueOrDefault(cur))
                hubPerComm[c] = n;
        }
        foreach (var hub in hubPerComm.OrderByDescending(kv => degrees.GetValueOrDefault(kv.Value)).Take(15))
            sb.AppendLine($"- {Short(hub.Value)}  (community {hub.Key}, grado {degrees.GetValueOrDefault(hub.Value)})");
        sb.AppendLine();

        // God Nodes
        sb.AppendLine("## God Nodes (le astrazioni piu' centrali)");
        var gods = g.GodNodes(10, pageRankIterations);
        for (var i = 0; i < gods.Count; i++)
        {
            var gr = gods[i];
            sb.AppendLine($"{i + 1}. `{Short(gr.Node)}` — score {gr.Score:F3} (degree {gr.Degree:F2} · pagerank {gr.Pagerank:F2} · betweenness {gr.Betweenness:F2})");
        }
        sb.AppendLine();

        // Surprising Connections
        sb.AppendLine("## Connessioni sorprendenti (ponti tra community)");
        var surprises = g.SurprisingConnections(communities, 8);
        if (surprises.Count == 0)
            sb.AppendLine("- Nessuna: le community sono ben separate.");
        else
            foreach (var s in surprises)
                sb.AppendLine($"- `{Short(s.From)}` → `{Short(s.To)}`  (community {s.CommFrom} → {s.CommTo})");
        sb.AppendLine();

        // Suggested Questions
        sb.AppendLine("## Domande da esplorare");
        foreach (var q in g.SuggestedQuestions(6))
            sb.AppendLine($"- {q}");
        sb.AppendLine();

        // Comunità principali
        sb.AppendLine($"## Community principali ({commCount} totali)");
        var byComm = new Dictionary<int, List<string>>();
        foreach (var (node, c) in communities)
            if (node.EndsWith(".md", StringComparison.OrdinalIgnoreCase))
            {
                if (!byComm.TryGetValue(c, out var l)) byComm[c] = l = new List<string>();
                l.Add(node);
            }
        foreach (var (c, members) in byComm.OrderByDescending(kv => kv.Value.Count).Take(10))
        {
            var hub = hubPerComm.GetValueOrDefault(c) ?? members.First();
            sb.AppendLine($"### Community {c} — \"{Short(hub)}\"");
            sb.AppendLine($"- {members.Count} note · hub: {Short(hub)}");
            sb.AppendLine($"- Esempi: {string.Join(", ", members.OrderByDescending(m => degrees.GetValueOrDefault(m)).Take(4).Select(Short))}");
            sb.AppendLine();
        }
        return sb.ToString();

        static int Percent(int part, int total) => total == 0 ? 0 : (int)Math.Round(100.0 * part / total);
    }

    /// <summary>Folder del path (senza basename), "" se nessuna cartella.</summary>
    private static string FolderOf(string path)
    {
        var i = path.LastIndexOf('/');
        return i > 0 ? path[..i] : "";
    }

    /// <summary>Nome file sicuro per una nota Obsidian (slug).</summary>
    private static string Slug(string s)
    {
        var sb = new StringBuilder();
        foreach (var ch in s)
        {
            if (char.IsLetterOrDigit(ch) || ch == '-' || ch == '_' || ch == ' ') sb.Append(ch);
            else sb.Append('-');
        }
        return sb.ToString().Trim();
    }

    private static string EscapeMd(string s) => s.Replace("\"", "\\\"").Replace("\n", " ");

    private static readonly HashSet<string> StopWords = new(StringComparer.OrdinalIgnoreCase)
    {
        "the","and","for","with","how","you","your","that","this","from","are","what","not","but","all",
        "using","use","can","will","into","over","more","other","such","their","than","when","then","also",
        "its","per","via","pdf","pdfs","di","il","lo","la","le","gli","un","una","che","come","con","della","nel",
    };

    /// <summary>Token rilevanti di un titolo per il topic detection: minuscolo, senza anno iniziale
    /// "2006 -", senza punteggiatura, parole con lunghezza 3..24, escluse le stop-word.</summary>
    private static IEnumerable<string> TokenizeTitle(string title)
    {
        var t = Regex.Replace(title, @"^\d{4}\s*[-–—]\s*", "");   // via l'anno iniziale
        foreach (Match m in Regex.Matches(t, @"[A-Za-zÀ-ÿ]{3,24}"))
        {
            var w = m.Value;
            if (StopWords.Contains(w)) continue;
            yield return w;
        }
    }



    private static string Sanitize(string path) =>
        string.Concat(path.Select(c => char.IsLetterOrDigit(c) ? c : '_'));

    private static string Escape(string s) => s.Replace("\"", "\\\"");
    private static string XmlEscape(string s) =>
        s.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");

    private static readonly JsonSerializerOptions JsonOpts = new() { WriteIndented = true };
}
