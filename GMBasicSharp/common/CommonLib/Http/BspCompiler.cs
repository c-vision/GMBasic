using System.Text;

namespace CommonLib.Http;

/// <summary>
/// Compila un file .bsp (BASIC Scripting Page) in sorgente BASIC eseguibile dalla P-Code VM / IL Backend.
/// Trasforma la sintassi mista BASIC+HTML in un programma BASIC puro con PRINT per l'output HTML.
/// </summary>
public static class BspCompiler
{
    /// <summary>Compila il sorgente .bsp in un programma BASIC.</summary>
    /// <param name="bspSource">Contenuto del file .bsp</param>
    /// <param name="pagePath">Percorso del file (per diagnostica)</param>
    /// <returns>Sorgente BASIC compilabile</returns>
    public static string Compile(string bspSource, string pagePath, bool emitPreamble = true)
    {
        var sb = new StringBuilder();
        var included = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        included.Add(Path.GetFullPath(pagePath));
        CompileCore(bspSource, pagePath, sb, included, emitPreamble);
        return sb.ToString();
    }

    private static void CompileCore(string bspSource, string pagePath, StringBuilder sb, HashSet<string> included, bool emitPreamble = true)
    {
        var lines = bspSource.Replace("\r\n", "\n").Replace('\r', '\n').Split('\n');
        // Estrai direttive e contenuto
        var directives = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var contentLines = new List<string>();

        foreach (var rawLine in lines)
        {
            var line = rawLine.Trim();

            if (line.StartsWith("<%@") && line.EndsWith("%>"))
            {
                ParseDirective(line, directives);
                continue;
            }
            contentLines.Add(rawLine);
        }

        var content = string.Join("\n", contentLines);

        // Preambolo - HTTP sempre importato (necessario per REQ/RES). Emesso SOLO per la pagina
        // principale (emitPreamble): il LAYOUT concatenato non deve ri-dichiarare DIM PAGE (e
        // REQ/RES/SESSION) altrimenti nel runtime ottiene un NUOVO oggetto PAGE e le sezioni
        // impostate dalla pagina (PAGE("__sec_...")) non sono più visibili al layout.
        if (emitPreamble)
        {
            sb.AppendLine("' Generated BSP: " + pagePath);
            sb.AppendLine("IMPORT HTTP");
            // Anti-XSS (2026-09-02): STRINGS.HTMLENCODE per l'auto-escaping di <%= %> / @Model.
            sb.AppendLine("IMPORT STRINGS");
            if (directives.TryGetValue("IMPORT", out var imports))
            {
                foreach (var ns in imports.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                    if (!ns.Equals("HTTP", StringComparison.OrdinalIgnoreCase))
                        sb.AppendLine($"IMPORT {ns}");
            }

            sb.AppendLine("ON ERROR GOTO BspErrorHandler");
            sb.AppendLine();
            sb.AppendLine("' === Variabili built-in BSP ===");
            sb.AppendLine("' REQ/RES/SESSION sono DICHIARATI (non solo pre-registrati) perché il backend IL");
            sb.AppendLine("' ha un emitter statico: campo/metodo su di essi (RES.StatusCode, RES.Send, REQ.Path)");
            sb.AppendLine("' si risolve solo con un tipo dichiarato (HttpRequestType/HttpResponseType/JsonType —");
            sb.AppendLine("' JsonFieldGet/JsonFieldSet/CollectionInvoke). Il backend P-Code li semina per NOME");
            sb.AppendLine("' in ogni caso (GlobalSlots): i DIM non interferiscono con il seeding. FORM/QUERY sono");
            sb.AppendLine("' keyword del linguaggio (TokenKind.Form/SqlQuery) e restano non dichiarate: vengono");
            sb.AppendLine("' usate solo via accesso a indice (FORM(\"x\"), QUERY(\"x\")), che il backend IL emette");
            sb.AppendLine("' come JsonIndexGet/JsonIndexSet anche su un globale non tipizzato.");
            sb.AppendLine("DIM REQ AS HTTPREQUEST");
            sb.AppendLine("DIM RES AS HTTPRESPONSE");
            sb.AppendLine("DIM SESSION AS JSON");
            // PAGE DEVE essere inizializzata con NEW JSON: senza, il PCodeVM la lascia null e
            // ogni scrittura (PAGE["__sec_..."] = "...", PAGE["Title"] = "") fallisce in
            // silenzio → le sezioni del layout non vengono mai viste. REQ/RES/SESSION invece
            // sono seminate dall'host (BspDispatch/TryServeBspPage) nello slot globale, quindi
            // il DIM nudo basta per il type-checking statico del backend IL.
            sb.AppendLine("DIM PAGE AS JSON = NEW JSON");
            sb.AppendLine("PAGE[\"Title\"] = \"\"");
            sb.AppendLine();
        }

        // @MODEL
        if (directives.TryGetValue("MODEL", out var modelType))
        {
            var mt = modelType.Trim();
            sb.AppendLine($"' === Model ({mt}) ===");
            sb.AppendLine($"DIM MODEL AS {mt}");
            sb.AppendLine($"MODEL = NEW {mt}");
            sb.AppendLine();
        }

        // @AUTHORIZE
        if (directives.TryGetValue("AUTHORIZE", out var authRole))
        {
            var role = authRole.Trim().Trim('"');
            sb.AppendLine("' === Authorization guard ===");
            sb.AppendLine("IF SESSION(\"user\") = \"\" THEN");
            sb.AppendLine("  RES.StatusCode = 401");
            sb.AppendLine("  PRINT \"<h1>401 - Unauthorized</h1>\";");
            sb.AppendLine("  PRINT \"<p>Please login.</p>\";");
            sb.AppendLine("  END");
            sb.AppendLine("END IF");
            if (!string.IsNullOrEmpty(role) && !role.Equals("TRUE", StringComparison.OrdinalIgnoreCase))
            {
                sb.AppendLine($"IF SESSION(\"role\") <> \"{role}\" THEN");
                sb.AppendLine("  RES.StatusCode = 403");
                sb.AppendLine("  PRINT \"<h1>403 - Forbidden</h1>\";");
                sb.AppendLine($"  PRINT \"<p>Role '{role}' required.</p>\";");
                sb.AppendLine("  END");
                sb.AppendLine("END IF");
            }
            sb.AppendLine();
        }

        sb.AppendLine("' === Inizializzazione REQ/RES (iniettata dal server HTTP) ===");
        sb.AppendLine("' REQ, RES, SESSION, FORM, QUERY sono popolati dall'host prima dell'esecuzione");
        sb.AppendLine();

        // Layout
        var hasLayout = directives.TryGetValue("LAYOUT", out var layout)
                     || directives.TryGetValue("MASTER", out layout);

        // Cache
        if (directives.TryGetValue("CACHE", out var cacheSeconds))
            sb.AppendLine($"REM BspSetCache {cacheSeconds}");

        if (hasLayout)
        {
            // Layout: prima raccoglie le sezioni della pagina (devono esistere PRIMA che il
            // layout le usi), poi compila il corpo e lo wrappa nel layout.
            var sections = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var bodySb = new StringBuilder();
            EmitContent(content, bodySb, pagePath, included, sections);
            var bodyCode = bodySb.ToString();

            // Emette le sezioni PRIMA del layout (impostano PAGE("__sec_<nome>"))
            foreach (var (secName, secHtml) in sections)
                sb.AppendLine($"PAGE[\"__sec_{secName}\"] = {EscapedBasicString(secHtml)}");

            // Risolvi percorso layout relativo alla pagina
            var layoutPath = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(pagePath)) ?? "", layout!);
            if (File.Exists(layoutPath))
            {
                var layoutSrc = File.ReadAllText(layoutPath);
                // emitPreamble: false — il layout NON deve ri-dichiarare DIM PAGE/REQ/RES/SESSION
                // (altrimenti nel runtime ottiene un NUOVO oggetto PAGE e le sezioni impostate
                // dalla pagina, PAGE("__sec_<nome>"), non sono visibili al layout)
                var layoutCode = Compile(layoutSrc, layoutPath, emitPreamble: false);
                // Sostituisci RENDERBODY con il corpo della pagina
                layoutCode = layoutCode.Replace("REM BspRenderBody", bodyCode);
                sb.Append(layoutCode);
            }
            else
            {
                sb.AppendLine($"' Layout not found: {layoutPath}");
                sb.Append(bodyCode);
            }
        }
        else
        {
            var sections2 = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            EmitContent(content, sb, pagePath, included, sections2);
            foreach (var (secName, secHtml) in sections2)
                sb.AppendLine($"PAGE[\"__sec_{secName}\"] = {EscapedBasicString(secHtml)}");
        }

        // Error handler: emesso SEMPRE alla fine del programma principale (emitPreamble). Il
        // layout è concatenato senza preambolo, quindi senza END né BspErrorHandler propri —
        // il flusso cade qui dopo l'ultimo PRINT del layout
        if (emitPreamble)
        {
            sb.AppendLine();
            // Senza questo END, l'esecuzione normale continuava dritta nella label sottostante
            // (nessun salto implicito su un'etichetta in BASIC) — ogni pagina, anche senza
            // errori, finiva per eseguire l'handler ed emettere HTTP 500 con il corpo corretto
            // già inviato prima. Con RESUME NEXT il gestore resta raggiungibile solo via
            // ON ERROR GOTO, mai per caduta.
            sb.AppendLine("END");
            sb.AppendLine("' === Error handler BSP ===");
            sb.AppendLine("BspErrorHandler:");
            sb.AppendLine("  RES.StatusCode = 500");
            sb.AppendLine("  PRINT \"<h1>500 - Server Error</h1>\";");
            sb.AppendLine("  PRINT \"<pre>\"; ERROR$; \"</pre>\";");
            sb.AppendLine("  RESUME NEXT");
        }
    }

    private static void ParseDirective(string line, Dictionary<string, string> directives)
    {
        // <%@ IMPORT HTTP, STRINGS %>
        // <%@ LAYOUT "Main.bsp" %>
        // <%@ CACHE 60 %>
        var inner = line[3..^2].Trim(); // Remove <%@ and %>
        var spaceIdx = inner.IndexOf(' ');
        if (spaceIdx < 0) return;

        var key = inner[..spaceIdx].Trim().ToUpperInvariant();
        var value = inner[(spaceIdx + 1)..].Trim().Trim('"');

        if (directives.ContainsKey(key))
            directives[key] += ", " + value;
        else
            directives[key] = value;
    }

    private static void EmitContent(string content, StringBuilder sb, string pagePath, HashSet<string> included, Dictionary<string,string> sections)
    {
        var pos = 0;
        var inBlock = false;
        var blockStart = 0;

        while (pos < content.Length)
        {
            if (!inBlock)
            {
                var tagStart = content.IndexOf("<%", pos, StringComparison.Ordinal);
                if (tagStart < 0)
                {
                    EmitHtmlWithTemplateDirectives(content[pos..], sb, pagePath, included, sections);
                    break;
                }

                EmitHtmlWithTemplateDirectives(content[pos..tagStart], sb, pagePath, included, sections);
                pos = tagStart + 2;

                if (pos < content.Length && content[pos] == '=')
                {
                    // <%= expr %> → output HTML-escaped di default (anti-XSS 2026-09-02);
                    // escape-hatch esplicito "<%= expr | raw %>" per HTML fidato già codificato.
                    pos++;
                    var endExpr = content.IndexOf("%>", pos, StringComparison.Ordinal);
                    if (endExpr < 0) break;
                    var expr = content[pos..endExpr].Trim();
                    var isRaw = expr.EndsWith("| raw", StringComparison.OrdinalIgnoreCase);
                    if (isRaw) expr = expr[..^5].TrimEnd();
                    sb.AppendLine(isRaw ? $"PRINT {expr};" : $"PRINT STRINGS.HTMLENCODE({expr});");
                    pos = endExpr + 2;
                }
                else
                {
                    // <% code %>
                    inBlock = true;
                    blockStart = pos;
                }
            }
            else
            {
                var endBlock = content.IndexOf("%>", pos, StringComparison.Ordinal);
                if (endBlock < 0)
                {
                    // EOF inside block → emit remaining
                    var code = content[blockStart..].Trim();
                    if (!string.IsNullOrWhiteSpace(code))
                        sb.AppendLine(code);
                    break;
                }

                var codeBlock = content[blockStart..endBlock].Trim();
                if (!string.IsNullOrWhiteSpace(codeBlock))
                {
                    // Gestisci RENDERBODY e RENDERCOMPONENT
                    ProcessSpecialBlocks(codeBlock, sb);
                }
                pos = endBlock + 2;
                inBlock = false;
            }
        }
    }

    /// <summary>Processa il testo HTML cercando le direttive template MVC: @section, @RenderSection,
    /// @Html.Partial / @Html.RenderPartial. Il resto è HTML normale (PRINT).</summary>
    private static void EmitHtmlWithTemplateDirectives(string html, StringBuilder sb, string pagePath, HashSet<string> included, Dictionary<string,string> sections)
    {
        var pos = 0;
        while (pos < html.Length)
        {
            var at = html.IndexOf('@', pos);
            if (at < 0) { EmitHtml(html[pos..], sb); return; }

            // HTML prima del marker @
            EmitHtml(html[pos..at], sb);

            // Riconosci i marker noti (seguono @). Evita falsi positivi (@ in email/attributi).
            var rest = html[(at + 1)..];
            if (rest.StartsWith("section ", StringComparison.OrdinalIgnoreCase))
            {
                // @section <nome> { <html> } — salva la sezione nel dict (emessa PRIMA del layout)
                var sec = ParseSection(rest, out var name, out var consumed);
                if (sec != null)
                {
                    var safe = name ?? "unnamed";
                    sections[safe] = sec;
                    pos = at + 1 + consumed;
                    continue;
                }
                // se il parse fallisce, emette il resto come HTML
                EmitHtml(rest, sb);
                return;
            }
            if (rest.StartsWith("RenderSection", StringComparison.OrdinalIgnoreCase)
                || rest.StartsWith("RenderSection(", StringComparison.OrdinalIgnoreCase))
            {
                // @RenderSection("nome") → PRINT PAGE("__sec_<nome>");
                var m = System.Text.RegularExpressions.Regex.Match(rest,
                    @"^RenderSection\(\s*""([^""]+)""\s*\)", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                if (m.Success)
                {
                    sb.AppendLine($"PRINT PAGE[\"__sec_{m.Groups[1].Value}\"];");
                    pos = at + 1 + m.Length;
                    continue;
                }
            }
            if (rest.StartsWith("Html.Partial", StringComparison.OrdinalIgnoreCase)
                || rest.StartsWith("Html.RenderPartial", StringComparison.OrdinalIgnoreCase))
            {
                // @Html.Partial("file.bsp") / @Html.RenderPartial("file.bsp") → inline del partial
                var m2 = System.Text.RegularExpressions.Regex.Match(rest,
                    @"^Html\.(?:RenderPartial|Partial)\(\s*""([^""]+)""\s*\)",
                    System.Text.RegularExpressions.RegexOptions.IgnoreCase);
                if (m2.Success)
                {
                    EmitPartial(m2.Groups[1].Value, pagePath, sb, included, sections);
                    pos = at + 1 + m2.Length;
                    continue;
                }
            }

            // @Html.Encode(expr) / @Html.Raw(expr) → encode esplicito / output raw fidato.
            // Supporta espressioni con parentesi annidate e stringhe quotate.
            var mHtmlFn = System.Text.RegularExpressions.Regex.Match(rest,
                @"^Html\.(Encode|Raw)\(", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            if (mHtmlFn.Success)
            {
                var isRaw = mHtmlFn.Groups[1].Value.Equals("Raw", StringComparison.OrdinalIgnoreCase);
                var openIdx = mHtmlFn.Length - 1; // '(' in rest
                var inner = ReadBalancedExpr(rest, openIdx);
                if (inner != null)
                {
                    sb.AppendLine(isRaw ? $"PRINT {inner};" : $"PRINT STRINGS.HTMLENCODE({inner});");
                    pos = at + 1 + openIdx + inner.Length + 2; // fino alla ')' inclusa
                    continue;
                }
            }

            // @Model.Prop → PRINT MODEL.Prop; (accesso a un campo del @MODEL dichiarato con
            // <%@ MODEL Tipo %> → DIM MODEL AS Tipo = NEW Tipo). Solo "Model.<Campo>" (mai "@Model"
            // nudo, per non catturare falsi positivi). Output HTML-escaped di default (anti-XSS);
            // per HTML fidato usare @Html.Raw(Model.Prop).
            var mModel = System.Text.RegularExpressions.Regex.Match(rest,
                @"^Model\.([A-Za-z_][A-Za-z0-9_]*)", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            if (mModel.Success)
            {
                sb.AppendLine($"PRINT STRINGS.HTMLENCODE(MODEL.{mModel.Groups[1].Value});");
                pos = at + 1 + mModel.Length;
                continue;
            }

            // @ generico non riconosciuto → lo emette letterale
            EmitHtml("@", sb);
            pos = at + 1;
        }
    }

    /// <summary>Inlina un partial .bsp: risolve il path accanto alla pagina, legge il file e
    /// processa il suo contenuto (con EmitContent). Guard anti-loop su `included`.</summary>
    private static void EmitPartial(string file, string pagePath, StringBuilder sb, HashSet<string> included, Dictionary<string,string> sections)
    {
        var dir = Path.GetDirectoryName(Path.GetFullPath(pagePath)) ?? ".";
        var partialPath = Path.GetFullPath(Path.Combine(dir, file));
        if (!included.Add(partialPath))
        {
            sb.AppendLine($"' Partial loop evitato: {file}");
            return;
        }
        if (!File.Exists(partialPath))
        {
            sb.AppendLine($"' Partial non trovato: {file}");
            return;
        }
        var src = File.ReadAllText(partialPath);
        // Estrai solo il CONTENUTO (salta le direttive <%@ ... %> e il preambolo che verrebbe
        // duplicato): il partial è un frammento HTML/BASIC, non una pagina completa.
        var contentLines = new List<string>();
        foreach (var rawLine in src.Replace("\r\n", "\n").Replace('\r', '\n').Split('\n'))
        {
            var t = rawLine.Trim();
            if (t.StartsWith("<%@") && t.EndsWith("%>")) continue;
            contentLines.Add(rawLine);
        }
        EmitContent(string.Join("\n", contentLines), sb, pagePath, included, sections);
    }

    /// <summary>Estrae `section <nome> { <html> }` dall'input (inizia subito dopo "@").
    /// Ritorna l'HTML della sezione, il nome, e i caratteri consumati (incl. "@").</summary>
    private static string? ParseSection(string input, out string? name, out int consumed)
    {
        name = null;
        consumed = 0;
        var m = System.Text.RegularExpressions.Regex.Match(input,
            @"^section\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        if (!m.Success) return null;
        name = m.Groups[1].Value;
        var start = m.Index + m.Length;   // dopo "{"
        // bilancia le graffe
        int depth = 1;
        var i = start;
        for (; i < input.Length; i++)
        {
            if (input[i] == '{') depth++;
            else if (input[i] == '}') { depth--; if (depth == 0) break; }
        }
        if (i >= input.Length) return null;  // graffa non chiusa
        consumed = i + 1;                    // fino alla "}" inclusa
        return input[start..i].Trim();
    }

    private static string EscapedBasicString(string s)
        => "\"" + s.Replace("\"", "\"\"") + "\"";

    /// <summary>Legge l'espressione tra parentesi bilanciate a partire da openIdx (che deve
    /// puntare a '('), rispettando stringhe quotate. Ritorna il contenuto interno o null.</summary>
    private static string? ReadBalancedExpr(string s, int openIdx)
    {
        if (openIdx < 0 || openIdx >= s.Length || s[openIdx] != '(') return null;
        int depth = 0;
        bool inStr = false;
        for (var i = openIdx; i < s.Length; i++)
        {
            var c = s[i];
            if (inStr)
            {
                if (c == '"') inStr = false;
                continue;
            }
            if (c == '"') { inStr = true; continue; }
            if (c == '(') depth++;
            else if (c == ')')
            {
                depth--;
                if (depth == 0) return s[(openIdx + 1)..i].Trim();
            }
        }
        return null;
    }

    private static void ProcessSpecialBlocks(string code, StringBuilder sb)
    {
        foreach (var rawLine in code.Split('\n'))
        {
            var line = rawLine.TrimEnd('\r').TrimStart();
            if (line.Length == 0) continue;
            if (line.StartsWith("RENDERCOMPONENT ", StringComparison.OrdinalIgnoreCase))
            {
                // RENDERCOMPONENT "Name" → REM BspRenderComponent "Name"
                var rest = line["RENDERCOMPONENT".Length..].Trim();
                if (rest.StartsWith('"'))
                {
                    var endQuote = rest.IndexOf('"', 1);
                    if (endQuote > 0)
                    {
                        var name = rest[1..endQuote];
                        var withPart = rest[(endQuote + 1)..].Trim();
                        if (withPart.StartsWith("WITH ", StringComparison.OrdinalIgnoreCase))
                        {
                            var withExpr = withPart[5..].Trim();
                            sb.AppendLine($"REM BspRenderComponent \"{name}\", {withExpr}");
                        }
                        else
                        {
                            sb.AppendLine($"REM BspRenderComponent \"{name}\"");
                        }
                        continue;
                    }
                }
            }
            if (line.StartsWith("RENDERBODY", StringComparison.OrdinalIgnoreCase))
            {
                sb.AppendLine("REM BspRenderBody");
                continue;
            }
            sb.AppendLine(line);
        }
    }

    private static void EmitHtml(string html, StringBuilder sb)
    {
        if (string.IsNullOrEmpty(html) || html.Trim().Length == 0) return;

        // Escape double quotes for BASIC string
        var escaped = html.Replace("\"", "\"\"");
        // Split long lines
        foreach (var line in escaped.Split('\n'))
        {
            var l = line.TrimEnd('\r');
            if (l.Length > 0)
                sb.AppendLine($"PRINT \"{l}\";");
        }
    }
}
