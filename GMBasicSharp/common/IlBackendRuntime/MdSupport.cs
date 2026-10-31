// FILE: Basic.IlBackend.MdSupport.cs
// Fase 5 — Markdown AST in CONVERT (IL/PCodeVM). Parser block-level C# → dict compatibile
// BASIC (Dictionary<string,object?> / List<object?>). Render HTML/text dal dict.
using System.Text;

namespace Basic.IlBackend;

internal static class MdSupport
{
    private static Dictionary<string, object?> Blk(string type) =>
        new() { ["type"] = type };

    /* ── Parse markdown → dict AST ── */
    public static object MdParse(string text)
    {
        var blocks = new List<object?>();
        var lines = (text ?? "").Replace("\r\n", "\n").Split('\n');
        int i = 0;
        while (i < lines.Length)
        {
            string line = lines[i];
            string t = line.Trim();
            // heading
            int h = 0;
            while (h < t.Length && t[h] == '#') h++;
            if (h >= 1 && h <= 6 && (h == t.Length || t[h] == ' '))
            {
                var b = Blk("heading");
                b["level"] = (double)h;
                b["text"] = InlineToText(t[h..].Trim());
                b["links"] = RawLinks(t[h..].Trim());
                blocks.Add(b); i++; continue;
            }
            // fenced code
            if (t.StartsWith("```"))
            {
                string lang = t[3..].Trim();
                var sb = new StringBuilder();
                i++;
                while (i < lines.Length && !lines[i].Trim().StartsWith("```"))
                {
                    if (sb.Length > 0) sb.Append('\n');
                    sb.Append(lines[i]);
                    i++;
                }
                if (i < lines.Length) i++; // closing fence
                var c = Blk("codeblock");
                c["language"] = lang;
                c["text"] = sb.ToString();
                blocks.Add(c); continue;
            }
            // table
            if (t.StartsWith("|") && i + 1 < lines.Length && IsDelimiterRow(lines[i + 1].Trim()))
            {
                var tb = Blk("table");
                var header = ParseTableRow(t);
                var rows = new List<object?>();
                i += 2; // salta header + delimiter
                while (i < lines.Length && lines[i].Trim().StartsWith("|"))
                {
                    rows.Add(ParseTableRow(lines[i].Trim()));
                    i++;
                }
                tb["header"] = header;
                tb["rows"] = rows;
                blocks.Add(tb); continue;
            }
            // list
            if ((t.StartsWith("- ") || t.StartsWith("* ") || RegexNum(t)) && (t.Length < 2 || t[1] == ' ' || char.IsDigit(t[0])))
            {
                bool ordered = char.IsDigit(t[0]);
                var items = new List<object?>();
                while (i < lines.Length)
                {
                    var li = lines[i].Trim();
                    if (li.StartsWith("- ") || li.StartsWith("* ")) items.Add(InlineToText(li[2..].Trim()));
                    else if (RegexNum(li)) { var sp = li.IndexOf(' '); items.Add(InlineToText(sp > 0 ? li[(sp + 1)..] : li)); }
                    else break;
                    i++;
                }
                var l = Blk("list");
                l["ordered"] = ordered ? -1.0 : 0.0;
                l["items"] = items;
                blocks.Add(l); continue;
            }
            // blockquote
            if (t.StartsWith(">"))
            {
                var q = Blk("quote");
                q["text"] = InlineToText(t[1..].Trim());
                blocks.Add(q); i++; continue;
            }
            // hr
            if (t is "---" or "***" or "___")
            {
                blocks.Add(Blk("hr")); i++; continue;
            }
            // empty line
            if (t.Length == 0) { i++; continue; }
            // paragraph (accumula finché c'è testo non vuoto)
            var pb = Blk("paragraph");
            var ptext = new StringBuilder();
            var plinks = new List<object?>();
            while (i < lines.Length && lines[i].Trim().Length > 0 && !IsSpecialLine(lines[i].Trim()))
            {
                plinks.AddRange(RawLinks(lines[i].Trim()));
                if (ptext.Length > 0) ptext.Append(' ');
                ptext.Append(InlineToText(lines[i].Trim()));
                i++;
            }
            pb["text"] = ptext.ToString();
            pb["links"] = plinks;
            blocks.Add(pb);
        }
        return new Dictionary<string, object?> { ["type"] = "document", ["blocks"] = blocks };
    }

    private static bool IsSpecialLine(string t) =>
        t.StartsWith("#") || t.StartsWith("```") || t.StartsWith("|") || t.StartsWith("- ") ||
        t.StartsWith("* ") || t.StartsWith(">") || t == "---" || t == "***" || RegexNum(t);

    private static bool RegexNum(string s) => s.Length > 1 && char.IsDigit(s[0]) && s[1] == '.';

    private static bool IsDelimiterRow(string t)
    {
        if (!t.StartsWith("|") || !t.EndsWith("|")) return false;
        var cells = t.Split('|', StringSplitOptions.RemoveEmptyEntries);
        return cells.Length > 0 && cells.All(c => c.Trim().Length > 0 && c.Trim().All(ch => ch is '-' or ':' or ' '));
    }

    private static List<object?> ParseTableRow(string t)
    {
        var cells = t.Split('|', StringSplitOptions.RemoveEmptyEntries);
        return cells.Select(c => (object?)InlineToText(c.Trim())).ToList();
    }

    /* ── inline: bold/italic/code/link/image → testo semplice ── */
    public static string InlineToText(string s)
    {
        var sb = new StringBuilder();
        int i = 0;
        while (i < s.Length)
        {
            if (s[i] == '[')
            {
                int close = s.IndexOf(']', i + 1);
                if (close > i && close + 1 < s.Length && s[close + 1] == '(')
                {
                    int p = s.IndexOf(')', close + 2);
                    if (p > close) { sb.Append(s[(i + 1)..close]); i = p + 1; continue; }
                }
            }
            if (s[i] == '!' && i + 1 < s.Length && s[i + 1] == '[')
            {
                int close = s.IndexOf(']', i + 2);
                if (close > i + 1 && close + 1 < s.Length && s[close + 1] == '(')
                {
                    int p = s.IndexOf(')', close + 2);
                    if (p > close) { sb.Append(s[(i + 2)..close]); i = p + 1; continue; }
                }
            }
            if (s[i] == '`')
            {
                int close = s.IndexOf('`', i + 1);
                if (close > i) { sb.Append(s[(i + 1)..close]); i = close + 1; continue; }
            }
            if (s[i] is '*' or '_' or '#' or '>' or '~')
            {
                i++; continue;
            }
            sb.Append(s[i]);
            i++;
        }
        return sb.ToString();
    }

    private static List<object?> RawLinks(string s)
    {
        var links = new List<object?>();
        int i = 0;
        while (i < s.Length)
        {
            int lb = s.IndexOf('[', i);
            if (lb < 0) break;
            int rb = s.IndexOf(']', lb + 1);
            if (rb < 0 || rb + 1 >= s.Length || s[rb + 1] != '(') { i = lb + 1; continue; }
            int rp = s.IndexOf(')', rb + 2);
            if (rp < 0) break;
            links.Add(new Dictionary<string, object?> { ["text"] = s[(lb + 1)..rb], ["url"] = s[(rb + 2)..rp] });
            i = rp + 1;
        }
        return links;
    }

    private static Dictionary<string, object?> AsDoc(object? docOrText)
    {
        if (docOrText is System.Collections.IDictionary d && d.Contains("blocks"))
        {
            var r = new Dictionary<string, object?>();
            foreach (var k in d.Keys) r[k.ToString()!] = d[k];
            return r;
        }
        // è testo: parsa
        return (Dictionary<string, object?>)MdParse(docOrText?.ToString() ?? "");
    }

    private static List<object?> Blocks(object? doc) =>
        AsDoc(doc).TryGetValue("blocks", out var b) && b is System.Collections.IEnumerable en
            ? en.Cast<object?>().ToList()
            : new List<object?>();

    private static string Field(object? blk, string key)
    {
        if (blk is System.Collections.IDictionary d && d.Contains(key)) return d[key]?.ToString() ?? "";
        return "";
    }

    private static object? RawField(object? blk, string key)
    {
        if (blk is System.Collections.IDictionary d && d.Contains(key)) return d[key];
        return null;
    }

    private static double NumField(object? blk, string key)
    {
        if (blk is System.Collections.IDictionary d && d.Contains(key))
            return Convert.ToDouble(d[key]);
        return 0;
    }

    public static object MdHeadings(object? doc, int level)
    {
        var out_ = new List<object?>();
        foreach (var b in Blocks(doc))
        {
            if (Field(b, "type") == "heading" && (level <= 0 || (int)NumField(b, "level") == level))
            {
                out_.Add(new Dictionary<string, object?>
                {
                    ["level"] = NumField(b, "level"),
                    ["text"] = Field(b, "text"),
                });
            }
        }
        return out_;
    }

    public static object MdLinks(object? doc)
    {
        var out_ = new List<object?>();
        var seen = new HashSet<string>();
        foreach (var b in Blocks(doc))
        {
            if (Field(b, "type") == "link")
                AddLink(out_, seen, Field(b, "text"), Field(b, "url"));
            if (b is System.Collections.IDictionary d && d.Contains("links") && d["links"] is System.Collections.IEnumerable ls)
                foreach (var l in ls)
                {
                    var t_ = l is System.Collections.IDictionary ld && ld.Contains("text") ? ld["text"]?.ToString() ?? "" : "";
                    var u_ = l is System.Collections.IDictionary ld2 && ld2.Contains("url") ? ld2["url"]?.ToString() ?? "" : "";
                    AddLink(out_, seen, t_, u_);
                }
        }
        return out_;
    }

    private static void AddLink(List<object?> out_, HashSet<string> seen, string text, string url)
    {
        if (!string.IsNullOrEmpty(url) && seen.Add(url + "|" + text))
            out_.Add(new Dictionary<string, object?> { ["text"] = text, ["url"] = url });
    }

    private static void ExtractLinks(List<object?> out_, HashSet<string> seen, string s)
    {
        int i = 0;
        while (i < s.Length)
        {
            int lb = s.IndexOf('[', i);
            if (lb < 0) break;
            int rb = s.IndexOf(']', lb + 1);
            if (rb < 0 || rb + 1 >= s.Length || s[rb + 1] != '(') { i = lb + 1; continue; }
            int rp = s.IndexOf(')', rb + 2);
            if (rp < 0) break;
            AddLink(out_, seen, s[(lb + 1)..rb], s[(rb + 2)..rp]);
            i = rp + 1;
        }
    }

    public static object MdImages(object? doc)
    {
        var out_ = new List<object?>();
        foreach (var b in Blocks(doc))
        {
            if (Field(b, "type") == "image")
                out_.Add(new Dictionary<string, object?> { ["alt"] = Field(b, "alt"), ["url"] = Field(b, "url") });
        }
        return out_;
    }

    public static object MdTables(object? doc)
    {
        var out_ = new List<object?>();
        foreach (var b in Blocks(doc))
        {
            if (Field(b, "type") == "table") out_.Add(b);
        }
        return out_;
    }

    public static object MdCodeBlocks(object? doc)
    {
        var out_ = new List<object?>();
        foreach (var b in Blocks(doc))
        {
            if (Field(b, "type") == "codeblock")
                out_.Add(new Dictionary<string, object?> { ["language"] = Field(b, "language"), ["text"] = Field(b, "text") });
        }
        return out_;
    }

    public static string MdTree(object? doc)
    {
        var sb = new StringBuilder();
        int idx = 0;
        foreach (var b in Blocks(doc))
        {
            var type = Field(b, "type");
            sb.Append('#').Append(idx++).Append(' ').Append(type);
            if (type == "heading") sb.Append(" level=").Append(NumField(b, "level"));
            if (type == "paragraph") sb.Append(" text=\"").Append(Field(b, "text")).Append('"');
            if (type == "codeblock") sb.Append(" lang=").Append(Field(b, "language")).Append(" len=").Append(Field(b, "text").Length);
            if (type == "list" && RawField(b, "items") is System.Collections.IEnumerable itemsN) sb.Append(" items=").Append(itemsN.Cast<object?>().Count());
            if (type == "table" && RawField(b, "rows") is System.Collections.IEnumerable rowsN) sb.Append(" rows=").Append(rowsN.Cast<object?>().Count());
            sb.Append('\n');
        }
        return sb.ToString();
    }

    public static object MdQuery(object? doc, string selector)
    {
        var parts = selector.Split('[', StringSplitOptions.RemoveEmptyEntries);
        string type = parts[0].Trim();
        int level = 0;
        if (parts.Length > 1)
        {
            var m = System.Text.RegularExpressions.Regex.Match(parts[1], @"level\s*=\s*(\d+)");
            if (m.Success) level = int.Parse(m.Groups[1].Value);
        }
        var out_ = new List<object?>();
        foreach (var b in Blocks(doc))
        {
            if (Field(b, "type") != type) continue;
            if (type == "heading" && level > 0 && (int)NumField(b, "level") != level) continue;
            out_.Add(b);
        }
        return out_;
    }

    public static string MdToHtml(object? docOrText)
    {
        var sb = new StringBuilder("<html><body>\n");
        foreach (var b in Blocks(docOrText))
        {
            string type = Field(b, "type");
            switch (type)
            {
                case "heading":
                    int lv = Math.Min(6, Math.Max(1, (int)NumField(b, "level")));
                    sb.Append($"<h{lv}>").Append(Escape(Field(b, "text"))).Append($"</h{lv}>\n");
                    break;
                case "paragraph": sb.Append("<p>").Append(Escape(Field(b, "text"))).Append("</p>\n"); break;
                case "codeblock":
                    sb.Append("<pre><code").Append(Field(b, "language").Length > 0 ? " class=\"language-" + Escape(Field(b, "language")) + "\"" : "")
                      .Append('>').Append(Escape(Field(b, "text"))).Append("</code></pre>\n");
                    break;
                case "list":
                    bool ordered = NumField(b, "ordered") != 0;
                    var items = RawField(b, "items") is System.Collections.IEnumerable en ? en : Array.Empty<object?>();
                    sb.Append(ordered ? "<ol>\n" : "<ul>\n");
                    foreach (var it in items) sb.Append("<li>").Append(Escape(it?.ToString() ?? "")).Append("</li>\n");
                    sb.Append(ordered ? "</ol>\n" : "</ul>\n");
                    break;
                case "quote": sb.Append("<blockquote>").Append(Escape(Field(b, "text"))).Append("</blockquote>\n"); break;
                case "hr": sb.Append("<hr/>\n"); break;
                case "table":
                    sb.Append("<table>\n");
                    if (RawField(b, "header") is System.Collections.IEnumerable hdr)
                    {
                        sb.Append("<tr>");
                        foreach (var c in hdr) sb.Append("<th>").Append(Escape(c?.ToString() ?? "")).Append("</th>");
                        sb.Append("</tr>\n");
                    }
                    if (RawField(b, "rows") is System.Collections.IEnumerable rows)
                    {
                        foreach (var row in rows)
                        {
                            sb.Append("<tr>");
                            if (row is System.Collections.IEnumerable cells)
                                foreach (var c in cells) sb.Append("<td>").Append(Escape(c?.ToString() ?? "")).Append("</td>");
                            sb.Append("</tr>\n");
                        }
                    }
                    sb.Append("</table>\n");
                    break;
            }
        }
        sb.Append("</body></html>");
        return sb.ToString();
    }

    public static string MdToText(object? docOrText)
    {
        var sb = new StringBuilder();
        foreach (var b in Blocks(docOrText))
        {
            string type = Field(b, "type");
            if (type is "paragraph" or "heading" or "quote") sb.Append(Field(b, "text")).Append('\n');
            else if (type == "codeblock") sb.Append(Field(b, "text")).Append('\n');
            else if (type == "list")
            {
                if (RawField(b, "items") is System.Collections.IEnumerable items)
                    foreach (var it in items) sb.Append("- ").Append(it).Append('\n');
            }
            else if (type == "table")
            {
                if (RawField(b, "rows") is System.Collections.IEnumerable rows)
                    foreach (var row in rows)
                        if (row is System.Collections.IEnumerable cells)
                            sb.Append(string.Join(" | ", cells.Cast<object?>().Select(c => c?.ToString() ?? ""))).Append('\n');
            }
        }
        return sb.ToString();
    }

    public static string MdValidate(string text)
    {
        int fences = (text ?? "").Split("```", StringSplitOptions.None).Length - 1;
        if (fences % 2 != 0) return "fence non chiusa: triple backtick dispari";
        return "";
    }

    /* Strip: markdown → testo semplice (senza marcatura). */
    public static string MdStrip(string text) => MdToText(MdParse(text));

    /* Format: AST → markdown canonico (ri-renderizzato dai blocchi). */
    public static string MdFormat(object? docOrText)
    {
        var sb = new StringBuilder();
        foreach (var b in Blocks(docOrText))
        {
            string type = Field(b, "type");
            switch (type)
            {
                case "heading":
                    int lv = Math.Min(6, Math.Max(1, (int)NumField(b, "level")));
                    sb.Append(new string('#', lv)).Append(' ').Append(Field(b, "text")).Append("\n\n");
                    break;
                case "paragraph": sb.Append(Field(b, "text")).Append("\n\n"); break;
                case "codeblock":
                    sb.Append("```").Append(Field(b, "language")).Append('\n').Append(Field(b, "text")).Append("\n```\n\n");
                    break;
                case "list":
                    bool ordered = NumField(b, "ordered") != 0;
                    if (RawField(b, "items") is System.Collections.IEnumerable items)
                    {
                        int idx = 1;
                        foreach (var it in items)
                        {
                            sb.Append(ordered ? idx++.ToString() + ". " : "- ").Append(it).Append('\n');
                        }
                        sb.Append('\n');
                    }
                    break;
                case "quote": sb.Append("> ").Append(Field(b, "text")).Append("\n\n"); break;
                case "hr": sb.Append("---\n\n"); break;
                case "table":
                    if (RawField(b, "header") is System.Collections.IEnumerable hdr)
                    {
                        sb.Append("| ").Append(string.Join(" | ", hdr.Cast<object?>().Select(c => c?.ToString() ?? ""))).Append(" |\n");
                        sb.Append("| ").Append(string.Join(" | ", hdr.Cast<object?>().Select(_ => "---"))).Append(" |\n");
                    }
                    if (RawField(b, "rows") is System.Collections.IEnumerable rows)
                        foreach (var row in rows)
                            if (row is System.Collections.IEnumerable cells)
                                sb.Append("| ").Append(string.Join(" | ", cells.Cast<object?>().Select(c => c?.ToString() ?? ""))).Append(" |\n");
                    sb.Append('\n');
                    break;
            }
        }
        return sb.ToString();
    }

    private static string Escape(string s) =>
        s.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");
}
