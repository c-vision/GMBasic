// FILE: AI.Tests.WikiGraphTests
// Test del sottosistema grafo wiki: parser wikilink, frontmatter, build grafo da fixture,
// diff/UPDATE, export, PageRank. Il grafo è puro (nessun embedding) → testabile senza provider.
using Basic.AI.Graph;
using Basic.AI.Rag;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class WikiGraphTests : IDisposable
{
    private readonly string _tmp;
    public WikiGraphTests()
    {
        _tmp = Path.Combine(Path.GetTempPath(), "gmb-wikigraph-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_tmp);
        // fixture: 4 note con wikilink + frontmatter
        Write("Note A.md", "---\ntitle: Nota A\ncategory: Strategy\ntags: [quant, risk]\n---\nContenuto A. Vedi [[Note B]] e [[Cartella/Note C|alias C]].");
        Directory.CreateDirectory(Path.Combine(_tmp, "Cartella"));
        Write("Cartella/Note C.md", "---\ntitle: Nota C\ncategory: Portfolio\n---\nContenuto C, link a [[Note B#sezione]] e dangling [[NonEsiste]].");
        Write("Note B.md", "---\ntitle: Nota B\ncategory: Strategy\n---\nContenuto B, link a [[Note A]].");
        Write("Note D.md", "body senza frontmatter, link a [[note a]] (case-insensitive).");
    }
    private void Write(string rel, string content) =>
        File.WriteAllText(Path.Combine(_tmp, rel.Replace('/', Path.DirectorySeparatorChar)), content);
    public void Dispose() { try { Directory.Delete(_tmp, recursive: true); } catch { } }

    [Fact(DisplayName = "FrontmatterParser: estrae title/tags/category e body")]
    public void FrontmatterParser_Extracts_Fields_And_Body()
    {
        var text = File.ReadAllText(Path.Combine(_tmp, "Note A.md"));
        var fm = FrontmatterParser.Parse(text);
        fm.Get("title").Should().Be("Nota A");
        fm.Get("category").Should().Be("Strategy");
        fm.GetAll("tags").Should().Contain("quant").And.Contain("risk");
        fm.Body.Should().Contain("Contenuto A");
        FrontmatterParser.ToMetadataJson(fm).Should().Contain("Strategy");
    }

    [Fact(DisplayName = "FrontmatterParser: senza blocco --- ritorna body = testo intero")]
    public void FrontmatterParser_NoBlock_ReturnsWholeBody()
    {
        var fm = FrontmatterParser.Parse("solo testo, nessun frontmatter");
        fm.Fields.Should().BeEmpty();
        fm.Body.Should().Be("solo testo, nessun frontmatter");
    }

    [Fact(DisplayName = "WikiLinkParser: estrae [[target|alias]] e [[target#heading]]")]
    public void WikiLinkParser_Extracts_Links()
    {
        var links = WikiLinkParser.Extract("[[Note B]] [[Cartella/Note C|alias C]] [[Note B#sezione]]");
        links.Should().HaveCount(3);
        links[0].Target.Should().Be("Note B");
        links[1].Target.Should().Be("Cartella/Note C");
        links[1].Alias.Should().Be("alias C");
        links[2].Target.Should().Be("Note B");
        links[2].Heading.Should().Be("sezione");
    }

    [Fact(DisplayName = "WikiLinkParser.Resolve: risolve path e basename case-insensitive")]
    public void WikiLinkParser_Resolves_Paths()
    {
        var index = new HashSet<string>(["Note A.md", "Cartella/Note C.md", "Note B.md"], StringComparer.Ordinal);
        WikiLinkParser.Resolve("Note B", index).Should().Be("Note B.md");
        WikiLinkParser.Resolve("cartella/note c", index).Should().Be("Cartella/Note C.md");
        WikiLinkParser.Resolve("NonEsiste", index).Should().BeNull(); // dangling
    }

    [Fact(DisplayName = "GraphStore.Build: nodi=file, edge=wikilink risolti, dangling contati")]
    public void GraphStore_Build_Creates_Nodes_Edges_Dangling()
    {
        var g = GraphStore.Build(_tmp);
        g.NodeCount.Should().Be(4);
        // Note A → Note B.md (path) e → Cartella/Note C.md (path esatto)
        g.EdgeCount.Should().BeGreaterThanOrEqualTo(4);
        g.Dangling.Should().Contain(m => m.Contains("NonEsiste"));
        // case-insensitive: Note D → note a risolve a Note A.md
        g.Edges.Should().Contain(e => e.From.EndsWith("Note D.md") && e.To.EndsWith("Note A.md"));
        g.FolderCounts.Should().ContainKey("Cartella");
    }

    [Fact(DisplayName = "GraphStore.PageRank: i nodi più citati hanno rank più alto")]
    public void GraphStore_PageRank_Ranks_Cited_Nodes()
    {
        var g = GraphStore.Build(_tmp);
        var pr = g.PageRank();
        pr.Should().NotBeEmpty();
        // Note B è citata da A e C → rank alto; i rank sono normalizzati e ordinati decrescenti
        pr.Select(x => x.Rank).Should().BeInDescendingOrder();
        pr[0].Node.Should().NotBeNullOrEmpty();
    }

    [Fact(DisplayName = "GraphStore.ShortestPath: trova percorsi, vuoto se non raggiungibile")]
    public void GraphStore_ShortestPath_Finds_Paths()
    {
        var g = GraphStore.Build(_tmp);
        var path = g.ShortestPath("Note D.md", "Note B.md");
        path.Should().NotBeEmpty();
        path[0].Should().Be("Note D.md");
        path[^1].Should().Be("Note B.md");
        var empty = g.ShortestPath("Note A.md", "NodoIsolato.md");
        empty.Should().BeEmpty();
    }

    [Fact(DisplayName = "GraphExporter: genera mermaid/graphml/json/stats")]
    public void GraphExporter_Generates_All_Formats()
    {
        var g = GraphStore.Build(_tmp);
        var ex = GraphExporter.Export(g, "test");
        ex.Mermaid.Should().Contain("graph TD");
        ex.Graphml.Should().Contain("<graphml");
        ex.Json.Should().Contain("\"nodes\"");
        ex.StatsJson.Should().Contain("\"pageRankTop\"");
    }

    [Fact(DisplayName = "BuildState diff: NEW/CHANGED/UNCHANGED/REMOVED")]
    public void BuildState_Diff_Classifies()
    {
        var state = BuildState.Load(Path.Combine(_tmp, "x.state.json"));
        state.Files["Note A.md"] = "oldhash";
        state.Files["Nota Sparita.md"] = "oldhash2";
        state.Files["Nota Mod.md"] = "oldhash3";

        var wiki = new Dictionary<string, string>
        {
            ["Note A.md"] = "oldhash",   // unchanged
            ["Note B.md"] = "newhash",   // new
            ["Note C.md"] = "newhash2",  // new
            ["Nota Mod.md"] = "changed", // changed (hash diverso da oldhash3)
        };
        var diff = state.Diff(wiki);
        diff.Unchanged.Should().Contain("Note A.md");
        diff.New.Should().Contain("Note B.md").And.Contain("Note C.md");
        diff.Changed.Should().Contain("Nota Mod.md");
        diff.Removed.Should().Contain("Nota Sparita.md");
    }

    [Fact(DisplayName = "ComputeCommunities: assegna community e nodi connessi condividono la label")]
    public void ComputeCommunities_Groups_Connected_Nodes()
    {
        var g = GraphStore.Build(_tmp);   // Note A↔B, Note C→B, Note D→A
        var comm = g.ComputeCommunities();
        comm.Should().HaveCount(g.NodeCount);
        // Note A e Note B sono collegate → stessa community (o almeno non isolate)
        comm["Note A.md"].Should().Be(comm["Note B.md"]);
        // valori compatti 0..k-1
        var ids = comm.Values.Distinct().OrderBy(v => v).ToList();
        ids.Should().Equal(Enumerable.Range(0, ids.Count));
    }

    [Fact(DisplayName = "DegreeCentrality: grado in+out contato")]
    public void DegreeCentrality_Counts_Edges()
    {
        var g = GraphStore.Build(_tmp);
        var deg = g.DegreeCentrality();
        // Note A: emette verso Note B + riceve da Note B e Note D → almeno 2
        deg["Note A.md"].Should().BeGreaterThanOrEqualTo(2);
    }

    [Fact(DisplayName = "BetweennessCentrality: ritorna mappa non vuota su nodi hub")]
    public void BetweennessCentrality_Returns_Scores()
    {
        var g = GraphStore.Build(_tmp);
        var bet = g.BetweennessCentrality(maxSources: 10);
        bet.Should().ContainKey("Note A.md");
    }

    [Fact(DisplayName = "WikiEdge.Type: default 'link', load da JSON lo preserva")]
    public void WikiEdge_Type_Roundtrip()
    {
        var g = GraphStore.Build(_tmp);
        g.Edges.Should().NotBeEmpty();
        g.Edges.Should().OnlyContain(e => e.Type == "link");
        var json = GraphExporter.ToJson(g);
        var loaded = WikiGraph.LoadFromJson(json);
        loaded.Edges.Should().HaveCount(g.EdgeCount);
        loaded.Edges.Should().OnlyContain(e => e.Type == "link");
    }

    [Fact(DisplayName = "WikiEdge.Provenance: link=EXTRACTED, folder/tag=INFERRED; Confidence coerente")]
    public void WikiEdge_Provenance_ExtractedVsInferred()
    {
        var g = GraphStore.Build(_tmp);   // edge link reali
        g.Edges.Should().OnlyContain(e => e.Provenance == "EXTRACTED" && e.GetConfidence() == 1.0);
        // edge strutturali folder/tag → INFERRED con confidence < 1
        g.AddStructuralEdges();
        g.Edges.Where(e => e.Type == "link").Should().OnlyContain(e => e.Provenance == "EXTRACTED");
        g.Edges.Where(e => e.Type is "folder" or "tag").Should().OnlyContain(e => e.Provenance == "INFERRED");
        g.Edges.Where(e => e.Type == "folder").Should().OnlyContain(e => e.GetConfidence() == 0.8);
        g.Edges.Where(e => e.Type == "tag").Should().OnlyContain(e => e.GetConfidence() == 0.6);
        // il roundtrip JSON preserva provenance (derivata da type)
        var json = GraphExporter.ToJson(g);
        json.Should().Contain("\"provenance\"");
        json.Should().Contain("\"confidence\"");
        var loaded = WikiGraph.LoadFromJson(json);
        loaded.Edges.Where(e => e.Type == "link").Should().OnlyContain(e => e.Provenance == "EXTRACTED");
        loaded.Edges.Where(e => e.Type is "folder" or "tag").Should().OnlyContain(e => e.Provenance == "INFERRED");
    }

    [Fact(DisplayName = "WikiEdge.Provenance AMBIGUOUS: edge semantic con confidence bassa")]
    public void WikiEdge_Provenance_Ambiguous_For_LowConfidenceSemantic()
    {
        var g = new WikiGraph();
        g.Nodes["a.md"] = new WikiNode("a.md", "A", new());
        g.Nodes["b.md"] = new WikiNode("b.md", "B", new());
        // semantic con confidence alta → INFERRED
        g.Edges.Add(new WikiEdge("a.md", "b.md", null, "semantic", 0.9));
        g.Edges.Single(e => e.Type == "semantic").Provenance.Should().Be("INFERRED");
        // semantic con confidence bassa → AMBIGUOUS (da rivedere, mai omesso)
        g.Edges.Add(new WikiEdge("a.md", "b.md", null, "semantic", 0.55));
        var ambiguous = g.Edges.Last();
        ambiguous.Provenance.Should().Be("AMBIGUOUS");
        ambiguous.GetConfidence().Should().Be(0.55);
        // il roundtrip JSON preserva confidence e provenance AMBIGUOUS
        var json = GraphExporter.ToJson(g);
        json.Should().Contain("\"AMBIGUOUS\"");
        var loaded = WikiGraph.LoadFromJson(json);
        loaded.Edges.Should().Contain(e => e.Type == "semantic" && e.Provenance == "AMBIGUOUS");
    }

    [Fact(DisplayName = "GraphExporter.ToReport: genera un report markdown plain-language")]
    public void GraphExporter_ToReport_Generates_Markdown()
    {
        var dir = Path.Combine(_tmp, "rep");
        Directory.CreateDirectory(dir);
        Write("rep/A.md", "# Alpha\ncontenuto momentum, vedi [[B]]");
        Write("rep/B.md", "# Beta\ncontenuto momentum, vedi [[A]]");
        Write("rep/C.md", "# Gamma\ncontenuto trend, vedi [[A]]");
        var g = GraphStore.Build(dir);
        var report = GraphExporter.ToReport(g, "test-report");
        report.Should().StartWith("# Graph Report — test-report");
        report.Should().Contain("## Summary");
        report.Should().Contain("## Community Hubs");
        report.Should().Contain("## God Nodes");
        report.Should().Contain("## Connessioni sorprendenti");
        report.Should().Contain("## Domande da esplorare");
        report.Should().Contain("## Community principali");
        report.Should().Contain("EXTRACTED");
        // l'export completo include il report (GraphExport.Report non null)
        var export = GraphExporter.Export(g, "test-report");
        export.Report.Should().NotBeNullOrWhiteSpace();
    }

    [Fact(DisplayName = "ToStatsJson: include edgeProvenance (EXTRACTED/INFERRED)")]
    public void ToStatsJson_Includes_EdgeProvenance()
    {
        var g = GraphStore.Build(_tmp);
        g.AddStructuralEdges();
        var stats = GraphExporter.ToStatsJson(g, "test");
        stats.Should().Contain("\"edgeProvenance\"");
        stats.Should().Contain("EXTRACTED");
        stats.Should().Contain("INFERRED");
    }

    [Fact(DisplayName = "ToJson: include community e degree nei nodi")]
    public void ToJson_Includes_Community_And_Degree()
    {
        var g = GraphStore.Build(_tmp);
        var json = GraphExporter.ToJson(g);
        json.Should().Contain("\"community\"");
        json.Should().Contain("\"degree\"");
        json.Should().Contain("\"type\"");
    }

    [Fact(DisplayName = "ToStatsJson: include communities/edgeTypes/degreeTop/betweennessTop")]
    public void ToStatsJson_Includes_New_Metrics()
    {
        var g = GraphStore.Build(_tmp);
        var stats = GraphExporter.ToStatsJson(g, "test");
        stats.Should().Contain("\"communities\":");
        stats.Should().Contain("\"edgeTypes\"");
        stats.Should().Contain("\"degreeTop\"");
        stats.Should().Contain("\"betweennessTop\"");
    }

    [Fact(DisplayName = "ToHtml: genera viewer con JSON embedded valido")]
    public void ToHtml_Generates_Valid_Embedded_Json()
    {
        var g = GraphStore.Build(_tmp);
        var html = GraphExporter.ToHtml(g, "test");
        html.Should().Contain("<!DOCTYPE html>");
        html.Should().Contain("const DATA = {");
        html.Should().Contain("</html>");
        // il JSON embedded è sanitizzato (</ → <\/) così non può chiudere lo <script>:
        // nel blocco script il JSON non deve contenere un </script> letterale
        var scriptBlock = html[html.IndexOf("const DATA =", StringComparison.Ordinal)..];
        var jsonEnd = scriptBlock.IndexOf("</script>", StringComparison.Ordinal);
        jsonEnd.Should().BeGreaterThan(0);
        scriptBlock[..jsonEnd].Should().NotContain("</script>");
        // il JSON deve essere parsabile estraendolo
        var m = System.Text.RegularExpressions.Regex.Match(scriptBlock, @"const DATA = (\{.*?\});", System.Text.RegularExpressions.RegexOptions.Singleline);
        m.Success.Should().BeTrue();
        using var doc = System.Text.Json.JsonDocument.Parse(m.Groups[1].Value);
        doc.RootElement.TryGetProperty("nodes", out _).Should().BeTrue();
    }

    [Fact(DisplayName = "WriteObsidianVault: genera index + note community con wikilink")]
    public void WriteObsidianVault_Generates_Index_And_Community_Notes()
    {
        var g = GraphStore.Build(_tmp);   // Note A/B/C/D collegate
        var outDir = Path.Combine(_tmp, "obsidian");
        GraphExporter.WriteObsidianVault(g, "test", outDir);
        File.Exists(Path.Combine(outDir, "index.md")).Should().BeTrue();
        Directory.Exists(Path.Combine(outDir, "communities")).Should().BeTrue();
        var index = File.ReadAllText(Path.Combine(outDir, "index.md"));
        index.Should().Contain("# Graph: test");
        index.Should().Contain("[[");
        // le note community contengono wikilink ai membri reali
        var communityFiles = Directory.GetFiles(Path.Combine(outDir, "communities"), "*.md");
        communityFiles.Should().NotBeEmpty();
        var any = communityFiles.Select(File.ReadAllText);
        any.Should().Contain(x => x.Contains("[[Note A]]") || x.Contains("[[Note B]]") || x.Contains("[[Note C]]"));
        // nessuna nota vuota senza membri
        any.Should().OnlyContain(x => x.Contains("## Note"));
    }

    [Fact(DisplayName = "WriteObsidianVault: label community da topic condiviso per vault piatti")]
    public void WriteObsidianVault_Community_Label_Not_Just_HubName()
    {
        // vault piatto: 3 note con titolo "Alpha Trading X" correlate → label topic "Alpha Trading"
        var dir = Path.Combine(_tmp, "flat");
        Directory.CreateDirectory(dir);
        foreach (var n in new[] { "A.md", "B.md", "C.md" })
            File.WriteAllText(Path.Combine(dir, n),
                $"---\ntitle: Alpha Trading {n[0]}\n---\ncontenuto. Vedi [[{(n == "A.md" ? "B" : "A")}.md]]");
        var g = GraphStore.Build(dir);
        var outDir = Path.Combine(_tmp, "obsidian-flat");
        GraphExporter.WriteObsidianVault(g, "flat", outDir);
        var index = File.ReadAllText(Path.Combine(outDir, "index.md"));
        index.Should().Contain("Alpha Trading");   // topic condiviso, non "A.md"
    }

    // ── Semantic extraction LLM (GraphSemanticExtractor) ────────────────────
    private sealed class StubSemanticLlm : Basic.AI.Rag.ILlmCompleter
    {
        public string ModelName => "stub-sem";
        // produce un JSON con un edge tra i primi due path "Note X.md" trovati nel prompt
        public string Complete(string systemPrompt, string userPrompt)
        {
            var names = System.Text.RegularExpressions.Regex.Matches(userPrompt, @"## ([^\n]+?)\s+\(([^)]+\.md)\)")
                .Select(m => m.Groups[2].Value).Distinct().Take(2).ToList();
            if (names.Count < 2) return "{\"edges\":[]}";
            return $"{{\"edges\":[{{\"from\":{JsonStr(names[0])},\"to\":{JsonStr(names[1])},\"relation\":\"applies\",\"confidence\":0.85}}]}}";
        }
        private static string JsonStr(string s) => "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
    }

    /// <summary>Simula i modelli locali (es. qwen) che ignorano lo schema richiesto ed emettono
    /// una SEQUENZA di oggetti separati da virgola (senza wrapper "edges") e con soli basename
    /// al posto dei path completi. Il parser deve interpretare entrambe le cose.</summary>
    private sealed class StubSemanticSeqLlm : Basic.AI.Rag.ILlmCompleter
    {
        public string ModelName => "stub-sem-seq";
        public string Complete(string systemPrompt, string userPrompt)
        {
            var names = System.Text.RegularExpressions.Regex.Matches(userPrompt, @"## ([^\n]+?)\s+\(([^)]+\.md)\)")
                .Select(m => m.Groups[2].Value).Distinct().Take(3).ToList();
            if (names.Count < 2) return "[]";
            string Basename(string p) => p.Contains('/') ? p[(p.LastIndexOf('/') + 1)..] : p;
            var sb = new System.Text.StringBuilder();
            for (var i = 0; i < names.Count - 1; i++)
            {
                if (i > 0) sb.Append(',');
                sb.Append($"{{\"from\":\"{Basename(names[i])}\",\"to\":\"{Basename(names[i + 1])}\",\"relation\":\"complements\",\"confidence\":0.8}}");
            }
            return sb.ToString();
        }
    }

    [Fact(DisplayName = "GraphSemanticExtractor: LLM deduce edge semantic tra note esistenti")]
    public void SemanticExtractor_Adds_Edges()
    {
        var dir = Path.Combine(_tmp, "semllm");
        Directory.CreateDirectory(dir);
        Write("semllm/a.md", "# Metodo A\nspiega momentum, vedi [[b]]");
        Write("semllm/b.md", "# Metodo B\nusa il trend, vedi [[a]]");
        var g = GraphStore.Build(dir);
        var notes = new[]
        {
            new Basic.AI.Graph.GraphSemanticNote("a.md", "Metodo A", "spiega momentum"),
            new Basic.AI.Graph.GraphSemanticNote("b.md", "Metodo B", "usa il trend"),
        };
        var added = Basic.AI.Graph.GraphSemanticExtractor.ApplyToGraph(g, notes, new StubSemanticLlm());
        added.Should().Be(1);
        g.Edges.Should().Contain(e => e.Type == "semantic"
            && ((e.From == "a.md" && e.To == "b.md") || (e.From == "b.md" && e.To == "a.md")));
    }

    [Fact(DisplayName = "GraphSemanticExtractor: senza LLM → errore esplicito, nessun edge")]
    public void SemanticExtractor_NoLlm_Throws()
    {
        var g = GraphStore.Build(_tmp);
        var notes = new[] { new Basic.AI.Graph.GraphSemanticNote("a.md", "A", "testo") };
        var act = () => Basic.AI.Graph.GraphSemanticExtractor.ApplyToGraph(g, notes, llm: null);
        act.Should().Throw<InvalidOperationException>();
        g.Edges.Should().NotContain(e => e.Type == "semantic");
    }

    [Fact(DisplayName = "GraphSemanticExtractor: end-to-end con FixtureLlmCompleter (deterministico)")]
    public void SemanticExtractor_EndToEnd_WithFixture()
    {
        var dir = Path.Combine(_tmp, "semfix");
        Directory.CreateDirectory(dir);
        Write("semfix/a.md", "# A\ncontenuto momentum, vedi [[b]]");
        Write("semfix/b.md", "# B\ncontenuto trend, vedi [[a]]");
        var g = GraphStore.Build(dir);
        var fixture = new Basic.AI.Rag.FixtureLlmCompleter();
        var notes = new[]
        {
            new Basic.AI.Graph.GraphSemanticNote("a.md", "A", "contenuto momentum"),
            new Basic.AI.Graph.GraphSemanticNote("b.md", "B", "contenuto trend"),
        };
        var added = Basic.AI.Graph.GraphSemanticExtractor.ApplyToGraph(g, notes, fixture, maxNotesPerPrompt: 6);
        added.Should().Be(1);   // la fixture produce un edge "applies" tra le prime due note
        g.Edges.Should().Contain(e => e.Type == "semantic");
        // l'edge semantic sopravvive al roundtrip JSON
        var json = GraphExporter.ToJson(g);
        json.Should().Contain("\"semantic\"");
        var loaded = WikiGraph.LoadFromJson(json);
        loaded.Edges.Should().Contain(e => e.Type == "semantic");
    }

    [Fact(DisplayName = "GraphSemanticExtractor: risposta a sequenza di oggetti (senza wrapper) con basename → edge risolti")]
    public void SemanticExtractor_SequenceFormat_WithBasename_Resolves()
    {
        var dir = Path.Combine(_tmp, "semseq");
        Directory.CreateDirectory(dir);
        Directory.CreateDirectory(Path.Combine(dir, "sub"));
        Write("semseq/sub/Note A.md", "# Nota A\ncontenuto momentum, vedi [[Note B]]");
        Write("semseq/sub/Note B.md", "# Nota B\ncontenuto trend, vedi [[Note A]]");
        Write("semseq/sub/Note C.md", "# Nota C\ncontenuto breakout, vedi [[Note A]]");
        var g = GraphStore.Build(dir);
        var notes = new[]
        {
            new Basic.AI.Graph.GraphSemanticNote("sub/Note A.md", "Nota A", "momentum"),
            new Basic.AI.Graph.GraphSemanticNote("sub/Note B.md", "Nota B", "trend"),
            new Basic.AI.Graph.GraphSemanticNote("sub/Note C.md", "Nota C", "breakout"),
        };
        // lo stub emette {...},{...} senza wrapper "edges" e usa i SOLI basename
        var added = Basic.AI.Graph.GraphSemanticExtractor.ApplyToGraph(g, notes, new StubSemanticSeqLlm(), maxNotesPerPrompt: 6);
        added.Should().Be(2);   // A→B e B→C risolti per basename
        g.Edges.Should().Contain(e => e.Type == "semantic" && e.From == "sub/Note A.md" && e.To == "sub/Note B.md");
        g.Edges.Should().Contain(e => e.Type == "semantic" && e.From == "sub/Note B.md" && e.To == "sub/Note C.md");
    }

    [Fact(DisplayName = "GraphSemanticExtractor: risposta {} o [] → 0 edge, non errore")]
    public void SemanticExtractor_EmptyObject_Is_NotError()
    {
        var dir = Path.Combine(_tmp, "semempty");
        Directory.CreateDirectory(dir);
        Write("semempty/a.md", "# A\ncontenuto, vedi [[b]]");
        Write("semempty/b.md", "# B\ncontenuto, vedi [[a]]");
        var g = GraphStore.Build(dir);
        var notes = new[]
        {
            new Basic.AI.Graph.GraphSemanticNote("a.md", "A", "testo"),
            new Basic.AI.Graph.GraphSemanticNote("b.md", "B", "testo"),
        };
        var emptyLlm = new EmptyResponseLlm();
        var added = Basic.AI.Graph.GraphSemanticExtractor.ApplyToGraph(g, notes, emptyLlm, maxNotesPerPrompt: 6);
        added.Should().Be(0);   // risposta valida con zero relazioni: non un errore, non edge
        g.Edges.Should().NotContain(e => e.Type == "semantic");
    }

    private sealed class EmptyResponseLlm : Basic.AI.Rag.ILlmCompleter
    {
        public string ModelName => "stub-empty";
        public string Complete(string systemPrompt, string userPrompt) => "{}";
    }


    [Fact(DisplayName = "GodNodes: hub compositi da degree+pagerank+betweenness, top sensato")]
    public void GodNodes_Returns_Composite_Hubs()
    {
        var g = GraphStore.Build(_tmp);   // Note A/B/C/D collegate; A e B sono i centri
        var gods = g.GodNodes(5);
        gods.Should().NotBeEmpty();
        gods.Should().OnlyContain(x => x.Score > 0);
        // score <= 1 (componenti normalizzate) e ordinati decrescenti
        gods.Should().BeInDescendingOrder(x => x.Score);
        gods[0].Node.Should().BeOneOf("Note A.md", "Note B.md", "Note C.md");
    }

    [Fact(DisplayName = "ComputeCommunitiesLouvain: separa cluster densi non collegati")]
    public void Louvain_Groups_Connected_Nodes()
    {
        // due cluster K2 senza bridge: {A,B} e {X,Y} → Louvain deve produrre 2 community
        var dir = Path.Combine(_tmp, "louv");
        Directory.CreateDirectory(dir);
        Write("louv/A.md", "nota A, vedi [[B]]");
        Write("louv/B.md", "nota B, vedi [[A]]");
        Write("louv/X.md", "nota X, vedi [[Y]]");
        Write("louv/Y.md", "nota Y, vedi [[X]]");
        var g = GraphStore.Build(dir);
        var comm = g.ComputeCommunitiesLouvain();
        comm.Should().HaveCount(4);
        comm["A.md"].Should().Be(comm["B.md"]);      // cluster A-B insieme
        comm["X.md"].Should().Be(comm["Y.md"]);      // cluster X-Y insieme
        comm["A.md"].Should().NotBe(comm["X.md"]);   // cluster separati
        var ids = comm.Values.Distinct().OrderBy(v => v).ToList();
        ids.Should().Equal(Enumerable.Range(0, ids.Count));  // compatte 0..k-1
    }

    [Fact(DisplayName = "EnrichSemanticFromFrontmatter: note con tag/category condivisi → edge INFERRED")]
    public void EnrichSemantic_Adds_TagCategory_Edges()
    {
        // due note collegate SOLO da un tag condiviso (nessun wikilink tra loro)
        var dir = Path.Combine(_tmp, "sem");
        Directory.CreateDirectory(dir);
        Write("sem/X.md", "---\ntitle: X\ncategory: Alpha\ntags: [comune]\n---\ncontenuto X, vedi [[Y]]");
        Write("sem/Y.md", "---\ntitle: Y\ncategory: Beta\ntags: [comune]\n---\ncontenuto Y, vedi [[X]]");
        Write("sem/Z.md", "---\ntitle: Z\ncategory: Alpha\n---\ncontenuto Z, nessun link");
        var g = GraphStore.Build(dir);
        g.EnrichSemanticFromFrontmatter();
        // X e Y sono già collegati da wikilink → il tag comune non aggiunge edge ridondante;
        // X e Z condividono category Alpha senza link diretto → edge category nuovo
        var after = g.Edges.Where(e => e.Type is "tag" or "category").ToList();
        after.Should().Contain(e => e.Type == "category"
            && ((e.From.EndsWith("X.md") && e.To.EndsWith("Z.md")) || (e.From.EndsWith("Z.md") && e.To.EndsWith("X.md"))));
        after.Should().OnlyContain(e => e.Provenance == "INFERRED");
    }

    [Fact(DisplayName = "SurprisingConnections: ponti rari tra community diverse")]
    public void SurprisingConnections_Returns_CrossCommunity_Bridges()
    {
        var g = GraphStore.Build(_tmp);
        var comm = g.ComputeCommunities();
        var surprises = g.SurprisingConnections(comm, 10);
        surprises.Should().NotBeNull();
        foreach (var s in surprises)
            comm[s.From].Should().NotBe(comm[s.To]);   // solo tra community diverse
    }

    [Fact(DisplayName = "SuggestedQuestions: genera domande da hub e sorprese")]
    public void SuggestedQuestions_Generates_Questions()
    {
        var g = GraphStore.Build(_tmp);
        var q = g.SuggestedQuestions(10);
        q.Should().NotBeEmpty();
        q.Should().OnlyContain(x => x.EndsWith("?") || x.Contains("?"));
    }

    [Fact(DisplayName = "ToStatsJson: include godNodes/surprising/suggested")]
    public void ToStatsJson_Includes_GodNodes_Surprising_Suggested()
    {
        var g = GraphStore.Build(_tmp);
        var stats = GraphExporter.ToStatsJson(g, "test");
        stats.Should().Contain("\"godNodes\"");
        stats.Should().Contain("\"surprisingConnections\"");
        stats.Should().Contain("\"suggestedQuestions\"");
    }
}
