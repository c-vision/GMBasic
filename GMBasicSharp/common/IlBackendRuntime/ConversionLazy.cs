// FILE: Basic.IlBackend.ConversionLazy.cs
// Caricamento LAZY del modulo ConversionLib (conversione documenti: DOCX/XLSX/PDF/EPUB/RTF → MD/HTML/TXT).
//
// Perché: IlBackend.csproj ha ProjectReference a ConversionLib, quindi ilcs compila TUTTE le sue
// librerie (DocumentFormat.OpenXml, ClosedXML, PdfPig, RtfPipe, Epub...) nel binario NativeAOT —
// anche per un programma PRINT. Misurato: escludendo ConversionLib dal grafo il binario scende
// 85MB → 46MB (-31MB). Qui i call-sites del runtime (IlRuntime.Convert*, PCodeProcessor.Builtins)
// passano a questo loader che risolve i tipi PER NOME a runtime, così IlBackend può perdere il
// ProjectReference e ilcs non compila più ConversionLib finché il programma non usa CONVERT.
//
// Pattern identico a IlRuntime.EnsureSqlite() (Type.GetType per nome, best-effort). In un
// eseguibile NativeAOT che NON include ConversionLib, Type.GetType restituisce null → i metodi
// Convert* lanciano un errore chiaro invece di crashare. Nei programmi che usano CONVERT,
// ConversionLib è presente (linkata dal runner REPL) e il loading funziona.

using System.IO;
using System.Reflection;
using System.Text;

namespace Basic.IlBackend;

internal static class ConversionLazy
{
    private const string Asm = "ConversionLib";

    static ConversionLazy()
    {
        // RtfPipe (conversione RTF) richiede le codepage Windows (cp1252 ecc.):
        // senza provider, "Windows-1252 is not a supported encoding name". Stesso fix
        // già applicato a DbfEngine (cp1251). Registrato qui: primo accesso a CONVERT.
        try { Encoding.RegisterProvider(CodePagesEncodingProvider.Instance); }
        catch { /* best effort */ }
    }

    public static object? DocumentConverter =>
        Type.GetType($"Basic.ConversionLib.DocumentConverter, {Asm}");

    public static object? MarkdownRenderer =>
        Type.GetType($"Basic.ConversionLib.Render.MarkdownRenderer, {Asm}");

    public static object? HtmlRenderer =>
        Type.GetType($"Basic.ConversionLib.Render.HtmlRenderer, {Asm}");

    public static object? TextRenderer =>
        Type.GetType($"Basic.ConversionLib.Render.TextRenderer, {Asm}");

    private static object? InvokeStatic(object? target, string method, object?[] args)
    {
        if (target is not Type t) return null;
        return t.GetMethod(method, BindingFlags.Public | BindingFlags.Static)?.Invoke(null, args);
    }

    public static object? Parse(object bytes, string ext)
    {
        var dc = DocumentConverter as Type;
        return dc?.GetMethod("LegacyParse", BindingFlags.Public | BindingFlags.Static)
            ?.Invoke(null, [bytes, ext]);
    }

    public static object? RenderDoc(object? doc, string fmt)
    {
        var renderer = fmt == "html" ? HtmlRenderer : fmt == "txt" ? TextRenderer : MarkdownRenderer;
        return renderer is Type rt
            ? rt.GetMethod("Render", BindingFlags.Public | BindingFlags.Static)?.Invoke(null, [doc])
            : null;
    }

    // CONVERT.BATCH/CONVBAT — contratto canonico = gmb_doc_batch (runtime C): concatena
    // i documenti della cartella in un'unica stringa con separatore "--- FILE: nome ---".
    // NON si delega a ConversionLib.DocumentConverter.ConvertBatch (semantica diversa:
    // scrive i file e ritorna "[OK] nome"), così IL/PCode restano identici a VM/C/C99Native.
    // I formati primitivi (txt/md/log, csv, html, rtf) sono replicati byte-per-byte da
    // gmb_doc.c; i formati ricchi (docx/pdf/...) delegano a ConvertDocFile (ConversionLib).
    public static string ConvertBatch(string dir, string fmt, bool rec)
    {
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) return "";
        if (string.IsNullOrEmpty(fmt)) fmt = "md";
        var sb = new StringBuilder();
        BatchInto(sb, dir, fmt, rec);
        return sb.ToString();
    }

    private static void BatchInto(StringBuilder sb, string dir, string fmt, bool rec)
    {
        IEnumerable<string> entries;
        try { entries = Directory.EnumerateFileSystemEntries(dir); }
        catch { return; }
        foreach (var path in entries)
        {
            var name = Path.GetFileName(path);
            if (name.Length == 0 || name[0] == '.') continue;
            bool isDir;
            try { isDir = Directory.Exists(path); }
            catch { isDir = false; }
            if (isDir) { if (rec) BatchInto(sb, path, fmt, rec); continue; }
            sb.Append("--- FILE: ").Append(name).Append(" ---\n");
            var c = ConvertFile(path, fmt);
            if (c.Length > 0) { sb.Append(c); sb.Append("\n\n"); }
        }
    }

    // Conversione canonica file→fmt (parità gmb_doc_convert del runtime C). I formati
    // primitivi sono convertiti in managed (funzionano anche senza ConversionLib caricata);
    // i formati ricchi (docx/odt/xlsx/pptx/epub) delegano a ConversionLib.
    public static string ConvertFile(string path, string fmt)
    {
        var ext = Path.GetExtension(path).TrimStart('.').ToLowerInvariant();
        if (ext == "pdf") return ConvertDocFile(path, fmt);
        string text;
        try { text = File.ReadAllText(path); }
        catch { return ""; }
        string md = ext switch
        {
            "txt" or "md" or "log" => text,
            "csv" => CsvToMd(text),
            "html" or "htm" => HtmlToText(text),
            "rtf" => RtfToText(text),
            _ => RichOrRaw(path, text)
        };
        if (fmt.Equals("html", StringComparison.OrdinalIgnoreCase))
            return "<html><body>\n" + md + "\n</body></html>";
        return md;
    }

    private static string RichOrRaw(string path, string raw)
    {
        var rich = ConvertDocFile(path, "md");
        return rich.Length > 0 ? rich : raw;
    }

    private static bool At(string s, int i, string lit) =>
        i + lit.Length <= s.Length && string.CompareOrdinal(s, i, lit, 0, lit.Length) == 0;

    // Mirror byte-per-byte di gmb_doc_csv_to_md (native/gmb_doc_c/src/gmb_doc.c).
    private static string CsvToMd(string csv)
    {
        var sb = new StringBuilder();
        var cell = new StringBuilder();
        bool first = true;
        for (int i = 0; i < csv.Length; i++)
        {
            char ch = csv[i];
            if (ch == '\n' || ch == '\r')
            {
                if (ch == '\r' && i + 1 < csv.Length && csv[i + 1] == '\n') i++;
                if (cell.Length > 0)
                {
                    if (!first) sb.Append(" | ");
                    sb.Append(cell);
                    sb.Append(" |");
                    cell.Clear();
                }
                sb.Append(first ? "\n|---|---|" : "\n");
                first = false;
                continue;
            }
            if (ch == ',')
            {
                sb.Append("| ").Append(cell).Append(' ');
                cell.Clear();
                continue;
            }
            cell.Append(ch);
        }
        if (cell.Length > 0) { sb.Append("| ").Append(cell).Append(" |"); }
        return sb.ToString();
    }

    // Mirror byte-per-byte di gmb_doc_html_to_text (native/gmb_doc_c/src/gmb_doc.c).
    private static string HtmlToText(string html)
    {
        var sb = new StringBuilder();
        bool inTag = false;
        for (int i = 0; i < html.Length; i++)
        {
            char ch = html[i];
            if (ch == '<') { inTag = true; continue; }
            if (inTag) { if (ch == '>') inTag = false; continue; }
            if (ch == '&')
            {
                if (At(html, i, "&amp;")) { sb.Append('&'); i += 4; }
                else if (At(html, i, "&lt;")) { sb.Append('<'); i += 3; }
                else if (At(html, i, "&gt;")) { sb.Append('>'); i += 3; }
                else if (At(html, i, "&quot;")) { sb.Append('"'); i += 5; }
                else if (At(html, i, "&#39;")) { sb.Append('\''); i += 4; }
                else if (At(html, i, "&nbsp;")) { sb.Append(' '); i += 5; }
                else sb.Append('&');
                continue;
            }
            sb.Append(ch);
        }
        return sb.ToString();
    }

    // Mirror byte-per-byte di gmb_doc_rtf_to_text (native/gmb_doc_c/src/gmb_doc.c).
    private static string RtfToText(string rtf)
    {
        var sb = new StringBuilder();
        for (int i = 0; i < rtf.Length; i++)
        {
            char ch = rtf[i];
            if (ch == '\\')
            {
                i++;
                if (i >= rtf.Length) break;
                char n = rtf[i];
                if (n == '\\' || n == '{' || n == '}') { sb.Append(n); continue; }
                while (i < rtf.Length && !char.IsWhiteSpace(rtf[i]) && rtf[i] != ';') i++;
                continue;
            }
            if (ch == '{' || ch == '}') continue;
            sb.Append(ch);
        }
        return sb.ToString();
    }

    public static string ConvertUrl(string url)
    {
        // Senza ConversionLib (host di test / NativeAOT senza CONVERT) o con fetch fallita:
        // errore ESPLICITO e DETERMINISTICO — stesso marcatore del runtime C `gmb_doc_web`.
        // Prima `?? ""` dava stringa vuota (silent-wrong e divergenza vs C).
        var dc = DocumentConverter as Type;
        if (dc is null) return "?Error: web fetch failed";
        return InvokeStatic(dc, "ConvertUrl", [url])?.ToString() ?? "?Error: web fetch failed";
    }

    public static string ConvertFormats()
    {
        // Lista canonica = gmb_doc_formats_s() del backend C (parità byte su 4 backend).
        // La vecchia SupportedFormats di ConversionLib includeva anche json/xml/yaml/immagini
        // non convertibili dal C → divergenza CONVERT.FORMATS (fissata 2026-08-28).
        return "txt,csv,md,html,rtf,docx,epub,odt,xlsx,pptx,pdf,url";
    }

    public static string ConvertDocFile(string path, string fmt)
    {
        var bytes = File.ReadAllBytes(path);
        var doc = Parse(bytes, Path.GetExtension(path));
        return RenderDoc(doc, fmt)?.ToString() ?? "";
    }
}
