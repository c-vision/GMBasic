// FILE: CommonLib.FileReadWriteUtils.cs

using System.Text;

/* 
 * FileReadWriteUtils.cs: metodi statici per lettura e scrittura di file (ReadAllText, ReadAllBytes, ReadAllLines, Write, Append, CSV)
 * 
 * FUNZIONI:
 * - CoreReadAllText(path): legge tutto il testo da un file
 * - ReadAllBytes(fileName/file): legge tutti i byte da un file
 * - CoreReadAllLines(file/encoding): legge tutte le righe di un file
 * - CoreReadLines(fileName/file/encoding): legge righe in modo lazy
 * - CoreWriteAllBytes(fileName/file, bytes): scrive byte array su file
 * - CoreWriteAllLines(fileName/file, contents/encoding): scrive righe su file
 * - CoreWriteAllText(fileName/file, contents/encoding): scrive testo su file
 * - CoreAppendAllLines(fileName/file, contents/encoding): appende righe a file
 * - CoreAppendAllText(fileName/file, contents/encoding): appende testo a file
 * - ReadBytes(reader, bytesToRead): legge byte da BinaryReader
 * - WriteBytes(writer, buffer, bytesToWrite): scrive byte su BinaryWriter
 * - SplitCsvLine(line): suddivide una riga CSV
 */

namespace CommonLib;

/// <summary>
/// Provides static convenience methods for reading from and writing to files.
/// Wraps the <see cref="System.IO.File"/> class with overloads accepting both string paths and <see cref="FileInfo"/> objects.
/// </summary>
public static class FileReadWriteUtils
{
    /// <summary>
    /// Reads all text from a file using the default encoding (UTF-8).
    /// </summary>
    /// <param name="path">The path to the file to read.</param>
    /// <returns>The complete text contents of the file.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static string CoreReadAllText(string path)
    {
        return File.ReadAllText(path);
    }

    /// <summary>
    /// Reads all bytes from the specified file.
    /// </summary>
    /// <param name="fileName">The path to the file to read.</param>
    /// <returns>A byte array containing the contents of the file.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static byte[] ReadAllBytes(string fileName) => ReadAllBytes(new FileInfo(fileName));

    /// <summary>
    /// Reads all bytes from the specified file.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to read.</param>
    /// <returns>A byte array containing the contents of the file.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static byte[] ReadAllBytes(FileInfo file) => File.ReadAllBytes(file.FullName);

    /// <summary>
    /// Reads all lines from the specified file using the default encoding (UTF-8).
    /// </summary>
    /// <param name="fileName">The path to the file to read.</param>
    /// <returns>A string array containing all lines of the file.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static string[] ReadAllLines(string fileName) => CoreReadAllLines(new FileInfo(fileName));

    /// <summary>
    /// Reads all lines from the specified file using the default encoding (UTF-8).
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to read.</param>
    /// <returns>A string array containing all lines of the file.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static string[] CoreReadAllLines(FileInfo file) => File.ReadAllLines(file.FullName);

    /// <summary>
    /// Reads all lines from the specified file using the given encoding.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to read.</param>
    /// <param name="encoding">The character encoding to use.</param>
    /// <returns>A string array containing all lines of the file.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static string[] CoreReadAllLines(FileInfo file, Encoding encoding) =>
        File.ReadAllLines(file.FullName, encoding);

    /// <summary>
    /// Lazily reads lines from the specified file using the default encoding (UTF-8).
    /// </summary>
    /// <param name="fileName">The path to the file to read.</param>
    /// <returns>An <see cref="IEnumerable{T}"/> that yields lines one at a time.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static IEnumerable<string> CoreReadLines(string fileName) => CoreReadLines(new FileInfo(fileName));

    /// <summary>
    /// Lazily reads lines from the specified file using the default encoding (UTF-8).
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to read.</param>
    /// <returns>An <see cref="IEnumerable{T}"/> that yields lines one at a time.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static IEnumerable<string> CoreReadLines(FileInfo file) => File.ReadLines(file.FullName);

    /// <summary>
    /// Lazily reads lines from the specified file using the given encoding.
    /// </summary>
    /// <param name="fileName">The path to the file to read.</param>
    /// <param name="encoding">The character encoding to use.</param>
    /// <returns>An <see cref="IEnumerable{T}"/> that yields lines one at a time.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static IEnumerable<string> CoreReadLines(string fileName, Encoding encoding) =>
        CoreReadLines(new FileInfo(fileName), encoding);

    /// <summary>
    /// Lazily reads lines from the specified file using the given encoding.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to read.</param>
    /// <param name="encoding">The character encoding to use.</param>
    /// <returns>An <see cref="IEnumerable{T}"/> that yields lines one at a time.</returns>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static IEnumerable<string> CoreReadLines(FileInfo file, Encoding encoding) =>
        File.ReadLines(file.FullName, encoding);

    /// <summary>
    /// Writes the specified byte array to a file, overwriting it if it exists.
    /// </summary>
    /// <param name="fileName">The path to the file to write.</param>
    /// <param name="bytes">The byte array to write.</param>
    public static void CoreWriteAllBytes(string fileName, byte[] bytes) =>
        CoreWriteAllBytes(new FileInfo(fileName), bytes);

    /// <summary>
    /// Writes the specified byte array to a file, overwriting it if it exists.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to write.</param>
    /// <param name="bytes">The byte array to write.</param>
    public static void CoreWriteAllBytes(FileInfo file, byte[] bytes) => File.WriteAllBytes(file.FullName, bytes);

    /// <summary>
    /// Writes a collection of lines to a file using the default encoding (UTF-8), overwriting it if it exists.
    /// </summary>
    /// <param name="fileName">The path to the file to write.</param>
    /// <param name="contents">The lines to write.</param>
    public static void CoreWriteAllLines(string fileName, IEnumerable<string> contents) =>
        CoreWriteAllLines(new FileInfo(fileName), contents);

    /// <summary>
    /// Writes a collection of lines to a file using the default encoding (UTF-8), overwriting it if it exists.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to write.</param>
    /// <param name="contents">The lines to write.</param>
    public static void CoreWriteAllLines(FileInfo file, IEnumerable<string> contents) =>
        File.WriteAllLines(file.FullName, contents);

    /// <summary>
    /// Writes a collection of lines to a file using the given encoding, overwriting it if it exists.
    /// </summary>
    /// <param name="fileName">The path to the file to write.</param>
    /// <param name="contents">The lines to write.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreWriteAllLines(string fileName, IEnumerable<string> contents, Encoding encoding) =>
        CoreWriteAllLines(new FileInfo(fileName), contents, encoding);

    /// <summary>
    /// Writes a collection of lines to a file using the given encoding, overwriting it if it exists.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to write.</param>
    /// <param name="contents">The lines to write.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreWriteAllLines(FileInfo file, IEnumerable<string> contents, Encoding encoding) =>
        File.WriteAllLines(file.FullName, contents, encoding);

    /// <summary>
    /// Writes text to a file using the default encoding (UTF-8), overwriting it if it exists.
    /// </summary>
    /// <param name="fileName">The path to the file to write.</param>
    /// <param name="contents">The text to write.</param>
    public static void CoreWriteAllText(string fileName, string contents) =>
        CoreWriteAllText(new FileInfo(fileName), contents);

    /// <summary>
    /// Writes text to a file using the default encoding (UTF-8), overwriting it if it exists.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to write.</param>
    /// <param name="contents">The text to write.</param>
    public static void CoreWriteAllText(FileInfo file, string contents) =>
        File.WriteAllText(file.FullName, contents);

    /// <summary>
    /// Writes text to a file using the given encoding, overwriting it if it exists.
    /// </summary>
    /// <param name="fileName">The path to the file to write.</param>
    /// <param name="contents">The text to write.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreWriteAllText(string fileName, string contents, Encoding encoding) =>
        CoreWriteAllText(new FileInfo(fileName), contents, encoding);

    /// <summary>
    /// Writes text to a file using the given encoding, overwriting it if it exists.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file to write.</param>
    /// <param name="contents">The text to write.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreWriteAllText(FileInfo file, string contents, Encoding encoding) =>
        File.WriteAllText(file.FullName, contents, encoding);

    /// <summary>
    /// Appends a collection of lines to a file using the default encoding (UTF-8).
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="fileName">The path to the file.</param>
    /// <param name="contents">The lines to append.</param>
    public static void CoreAppendAllLines(string fileName, IEnumerable<string> contents) =>
        CoreAppendAllLines(new FileInfo(fileName), contents);

    /// <summary>
    /// Appends a collection of lines to a file using the default encoding (UTF-8).
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file.</param>
    /// <param name="contents">The lines to append.</param>
    public static void CoreAppendAllLines(FileInfo file, IEnumerable<string> contents) =>
        File.AppendAllLines(file.FullName, contents);

    /// <summary>
    /// Appends a collection of lines to a file using the given encoding.
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="fileName">The path to the file.</param>
    /// <param name="contents">The lines to append.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreAppendAllLines(string fileName, IEnumerable<string> contents, Encoding encoding) =>
        CoreAppendAllLines(new FileInfo(fileName), contents, encoding);

    /// <summary>
    /// Appends a collection of lines to a file using the given encoding.
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file.</param>
    /// <param name="contents">The lines to append.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreAppendAllLines(FileInfo file, IEnumerable<string> contents, Encoding encoding) =>
        File.AppendAllLines(file.FullName, contents, encoding);

    /// <summary>
    /// Appends text to a file using the default encoding (UTF-8).
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="fileName">The path to the file.</param>
    /// <param name="contents">The text to append.</param>
    public static void CoreAppendAllText(string fileName, string contents) =>
        CoreAppendAllText(new FileInfo(fileName), contents);

    /// <summary>
    /// Appends text to a file using the default encoding (UTF-8).
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file.</param>
    /// <param name="contents">The text to append.</param>
    public static void CoreAppendAllText(FileInfo file, string contents) =>
        File.AppendAllText(file.FullName, contents);

    /// <summary>
    /// Appends text to a file using the given encoding.
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="fileName">The path to the file.</param>
    /// <param name="contents">The text to append.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreAppendAllText(string fileName, string contents, Encoding encoding) =>
        CoreAppendAllText(new FileInfo(fileName), contents, encoding);

    /// <summary>
    /// Appends text to a file using the given encoding.
    /// Creates the file if it does not exist.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file.</param>
    /// <param name="contents">The text to append.</param>
    /// <param name="encoding">The character encoding to use.</param>
    public static void CoreAppendAllText(FileInfo file, string contents, Encoding encoding) =>
        File.AppendAllText(file.FullName, contents, encoding);

    /// <summary>Legge fino a bytesToRead byte da un BinaryReader, gestendo fine file.</summary>
    public static byte[] ReadBytes(BinaryReader reader, int bytesToRead)
    {
        if (bytesToRead <= 0) return [];
        var bytesAvailable = reader.BaseStream.Length - reader.BaseStream.Position;
        if (bytesAvailable <= 0) return [];
        var actualBytesToRead = (int)Math.Min(bytesToRead, bytesAvailable);
        return reader.ReadBytes(actualBytesToRead);
    }

    /// <summary>Scrive byte in un BinaryWriter da un buffer (byte[] o sbyte[]).</summary>
    public static int WriteBytes(BinaryWriter writer, object buffer, int? bytesToWrite = null)
    {
        byte[] bytes;
        if (buffer is sbyte[] sbyteArray)
            bytes = (byte[])(Array)sbyteArray;
        else if (buffer is byte[] byteArray)
            bytes = byteArray;
        else
            return 0;
        var count = bytesToWrite ?? bytes.Length;
        count = Math.Min(count, bytes.Length);
        if (count > 0) writer.Write(bytes, 0, count);
        return count;
    }

    /// <summary>Suddivide una riga CSV gestendo virgolette e doppie virgolette di escape.</summary>
    public static string[] SplitCsvLine(string line)
    {
        var fields = new List<string>();
        var currentField = new StringBuilder();
        var i = 0;
        var inQuotes = false;
        while (i < line.Length)
        {
            var c = line[i];
            if (inQuotes)
            {
                if (c == '"')
                {
                    if (i + 1 < line.Length && line[i + 1] == '"')
                    { currentField.Append('"'); i++; }
                    else { inQuotes = false; }
                }
                else { currentField.Append(c); }
            }
            else
            {
                switch (c)
                {
                    case '"' when string.IsNullOrWhiteSpace(currentField.ToString()):
                        currentField.Clear(); inQuotes = true; break;
                    case ',':
                        fields.Add(currentField.ToString()); currentField.Clear(); break;
                    default: currentField.Append(c); break;
                }
            }
            i++;
        }
        fields.Add(currentField.ToString());
        return fields.ToArray();
    }
    private static int _lastFileError = 0;
    public static int FileError { get => _lastFileError; set => _lastFileError = value; }
    /// <summary>[LEGACY] Non esiste, in CommonLib, un registro degli handle aperti: questo
    /// helper ritorna sempre 1 ed è quindi INUTILIZZABILE per FREEFILE. L'implementazione
    /// corretta è <see cref="IlBackend.IlRuntime.FreeFile"/> (primo numero di file libero),
    /// ed è quella usata dal builtin PCode. Non usare in codice nuovo.</summary>
    [Obsolete("Usare IlRuntime.FreeFile(): questo helper ritorna sempre 1 (nessun registro handle).")]
    public static int FreeFile()
    {
        return 1;
    }
    public static string ReadValue(string file, string section, string key, string defaultValue = "") {
        try {
            if (!File.Exists(file)) { _lastFileError = 2; return defaultValue; }
            var text = File.ReadAllText(file);
            if (IsJsonContent(text))
            {
                var clean = StripJsonComments(text);
                var v = ReadJsonValue(clean, section, key);
                return v ?? defaultValue;
            }
            return ReadIniValue(text, section, key, defaultValue);
        } catch { _lastFileError = 1; return defaultValue; }
    }
    public static string[] ReadSection(string file, string section) {
        try {
            if (!File.Exists(file)) { _lastFileError = 2; return Array.Empty<string>(); }
            var text = File.ReadAllText(file);
            if (IsJsonContent(text))
            {
                var clean = StripJsonComments(text);
                var keys = ReadJsonSectionKeys(clean, section);
                if (keys is not null) { _lastFileError = 0; return keys; }
            }
            return ReadIniSection(text, section);
        } catch { _lastFileError = 1; return Array.Empty<string>(); }
    }

    /// <summary>READSECTION come OGGETTO sezione (parità canone VM/C): ritorna un
    /// Dictionary&lt;string,object?&gt; chiave→valore della sezione (formato JSON/JSONC o INI),
    /// così un accesso campo `sezione.Campo` legge il VALORE e `.Count` il numero di chiavi.
    /// (Il read di <see cref="ReadSection"/> ritorna solo nomi chiave/righe.) Valori INI
    /// coerced: long → double → bool → string; JSON via parse tipizzato.</summary>
    public static System.Collections.Generic.Dictionary<string, object?> ReadSectionObject(string file, string section) {
        var result = new System.Collections.Generic.Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        try {
            if (!File.Exists(file)) { _lastFileError = 2; return result; }
            var text = File.ReadAllText(file);
            if (IsJsonContent(text))
            {
                var clean = StripJsonComments(text);
                using var doc = System.Text.Json.JsonDocument.Parse(clean);
                var target = string.IsNullOrEmpty(section) ? doc.RootElement
                    : (doc.RootElement.ValueKind == System.Text.Json.JsonValueKind.Object
                        && doc.RootElement.TryGetProperty(section, out var sec)
                        && sec.ValueKind == System.Text.Json.JsonValueKind.Object ? sec : default);
                if (target.ValueKind != System.Text.Json.JsonValueKind.Object) { _lastFileError = 1; return result; }
                foreach (var p in target.EnumerateObject())
                    result[p.Name] = JsonElementToValue(p.Value);
                _lastFileError = 0; return result;
            }
            // INI: [section] + key=value
            var lines = text.Replace("\r\n", "\n").Split('\n');
            bool inSec = string.IsNullOrEmpty(section);
            foreach (var raw in lines)
            {
                var t = raw.Trim();
                if (t.StartsWith("[") && t.EndsWith("]"))
                { inSec = t[1..^1].Equals(section, StringComparison.OrdinalIgnoreCase); continue; }
                if (!inSec || t.Length == 0 || t.StartsWith(";") || t.StartsWith("#")) continue;
                int eq = t.IndexOf('=');
                if (eq <= 0) continue;
                result[t[..eq].Trim()] = CoerceScalar(t[(eq + 1)..].Trim());
            }
            _lastFileError = 0;
        } catch { _lastFileError = 1; }
        return result;
    }

    private static object? JsonElementToValue(System.Text.Json.JsonElement el)
    {
        switch (el.ValueKind)
        {
            case System.Text.Json.JsonValueKind.Object:
                var o = new System.Collections.Generic.Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
                foreach (var p in el.EnumerateObject()) o[p.Name] = JsonElementToValue(p.Value);
                return o;
            case System.Text.Json.JsonValueKind.Array:
                var l = new System.Collections.Generic.List<object?>();
                foreach (var it in el.EnumerateArray()) l.Add(JsonElementToValue(it));
                return l;
            case System.Text.Json.JsonValueKind.String: return el.GetString();
            case System.Text.Json.JsonValueKind.Number:
                return el.TryGetInt64(out var li) ? (object)li : el.GetDouble();
            case System.Text.Json.JsonValueKind.True: return -1.0;
            case System.Text.Json.JsonValueKind.False: return 0.0;
            default: return null;
        }
    }

    private static object? CoerceScalar(string v)
    {
        if (v.Length == 0) return "";
        if (v.Equals("true", StringComparison.OrdinalIgnoreCase)) return -1.0;
        if (v.Equals("false", StringComparison.OrdinalIgnoreCase)) return 0.0;
        if (long.TryParse(v, System.Globalization.NumberStyles.Integer,
                System.Globalization.CultureInfo.InvariantCulture, out var lg)) return lg;
        if (double.TryParse(v, System.Globalization.NumberStyles.Float,
                System.Globalization.CultureInfo.InvariantCulture, out var db)) return db;
        return v;
    }

    /// <summary>True se il contenuto (dopo BOM/spazi E rimozione di eventuali commenti JSONC
    /// iniziali, es. "// FILE: ...") inizia con '{' → JSON.</summary>
    private static bool IsJsonContent(string text)
    {
        if (string.IsNullOrEmpty(text)) return false;
        var t = StripJsonComments(text).TrimStart('\uFEFF', ' ', '\t', '\r', '\n');
        return t.StartsWith('{');
    }

    /// <summary>Rimuove commenti // e /* */ (JSONC) fuori dalle stringhe, per permettere la
    /// lettura di file come system.settings.json (template con commenti).</summary>
    internal static string StripJsonComments(string text)
    {
        var sb = new StringBuilder(text.Length);
        var inStr = false;
        for (var i = 0; i < text.Length; i++)
        {
            var c = text[i];
            if (inStr)
            {
                sb.Append(c);
                if (c == '\\' && i + 1 < text.Length) { sb.Append(text[++i]); continue; }
                if (c == '"') inStr = false;
                continue;
            }
            if (c == '"') { inStr = true; sb.Append(c); continue; }
            if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
            { while (i < text.Length && text[i] != '\n') i++; continue; }
            if (c == '/' && i + 1 < text.Length && text[i + 1] == '*')
            { i += 2; while (i + 1 < text.Length && !(text[i] == '*' && text[i + 1] == '/')) i++; i++; continue; }
            sb.Append(c);
        }
        return sb.ToString();
    }

    private static string? ReadJsonValue(string text, string section, string key)
    {
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(StripJsonComments(text));
            var root = doc.RootElement;
            if (!string.IsNullOrEmpty(section) && root.TryGetProperty(section, out var secObj) && secObj.ValueKind == System.Text.Json.JsonValueKind.Object)
                return ReadJsonScalar(secObj, key);
            if (string.IsNullOrEmpty(section))
                return ReadJsonScalar(root, key);
            return null;
        }
        catch { return null; }
    }

    private static string? ReadJsonScalar(System.Text.Json.JsonElement obj, string key)
    {
        if (obj.ValueKind != System.Text.Json.JsonValueKind.Object || !obj.TryGetProperty(key, out var el)) return null;
        return el.ValueKind switch
        {
            System.Text.Json.JsonValueKind.String => el.GetString(),
            System.Text.Json.JsonValueKind.Number => el.GetRawText(),
            System.Text.Json.JsonValueKind.True => "true",
            System.Text.Json.JsonValueKind.False => "false",
            System.Text.Json.JsonValueKind.Null => null,
            _ => null,
        };
    }

    /// <summary>Per JSON: ritorna le chiavi di primo livello della sezione (oggetto), o dell'oggetto
    /// radice se section è vuota; null se la sezione non esiste o non è un oggetto.</summary>
    private static string[]? ReadJsonSectionKeys(string text, string section)
    {
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(StripJsonComments(text));
            var root = doc.RootElement;
            var target = string.IsNullOrEmpty(section) ? root
                : (root.TryGetProperty(section, out var s) && s.ValueKind == System.Text.Json.JsonValueKind.Object ? s : default);
            if (target.ValueKind != System.Text.Json.JsonValueKind.Object) return null;
            var list = new List<string>();
            foreach (var p in target.EnumerateObject()) list.Add(p.Name);
            return list.ToArray();
        }
        catch { return null; }
    }

    private static string ReadIniValue(string text, string section, string key, string defaultValue)
    {
        var lines = text.Replace("\r\n", "\n").Split('\n');
        bool inSec = string.IsNullOrEmpty(section);
        foreach (var raw in lines) { var t = raw.Trim(); if (t.StartsWith("[") && t.EndsWith("]")) { inSec = t[1..^1].Equals(section, StringComparison.OrdinalIgnoreCase); continue; } if (!inSec) continue; int eq = t.IndexOf('='); if (eq < 0) continue; if (t[..eq].Trim().Equals(key, StringComparison.OrdinalIgnoreCase)) return t[(eq+1)..].Trim(); }
        _lastFileError = 0; return defaultValue;
    }

    private static string[] ReadIniSection(string text, string section)
    {
        var lines = text.Replace("\r\n", "\n").Split('\n');
        var r = new List<string>(); bool inSec = string.IsNullOrEmpty(section);
        foreach (var raw in lines) { var t = raw.Trim(); if (t.StartsWith("[") && t.EndsWith("]")) { inSec = t[1..^1].Equals(section, StringComparison.OrdinalIgnoreCase); continue; } if (!inSec) continue; if (t.Length > 0 && !t.StartsWith(";") && !t.StartsWith("#")) r.Add(t); }
        _lastFileError = 0; return r.ToArray();
    }

    /// <summary>Scrive (o aggiorna) la coppia key=value nel file di configurazione, in formato
    /// INI compatibile con <see cref="ReadValue"/>: righe `[section]` e `key=value`. Se il file
    /// non esiste lo crea; se la chiave esiste già nella sezione la aggiorna in-place.</summary>
    public static void WriteValue(string file, string section, string key, string value) {
        try {
            var lines = new List<string>();
            if (File.Exists(file)) lines.AddRange(File.ReadAllLines(file));
            var inSec = string.IsNullOrEmpty(section);
            int secStart = -1, secEnd = lines.Count, keyLine = -1;
            for (var i = 0; i < lines.Count; i++) {
                var t = lines[i].Trim();
                if (t.StartsWith("[") && t.EndsWith("]")) {
                    if (inSec) { secEnd = i; break; }
                    if (t[1..^1].Equals(section, StringComparison.OrdinalIgnoreCase)) { inSec = true; secStart = i; }
                }
                else if (inSec) {
                    var eq = t.IndexOf('=');
                    if (eq > 0 && t[..eq].Trim().Equals(key, StringComparison.OrdinalIgnoreCase)) keyLine = i;
                }
            }
            if (keyLine >= 0) { lines[keyLine] = key + "=" + value; }
            else {
                if (secStart < 0 && !string.IsNullOrEmpty(section)) {
                    if (lines.Count > 0 && lines[^1].Trim().Length > 0) lines.Add("");
                    lines.Add("[" + section + "]");
                    secEnd = lines.Count;
                }
                lines.Insert(Math.Max(secStart + 1, secEnd == lines.Count ? lines.Count : secEnd), key + "=" + value);
            }
            File.WriteAllLines(file, lines, Encoding.UTF8);
            _lastFileError = 0;
        } catch { _lastFileError = 1; }
    }

    /// <summary>Scrive (o aggiorna) una sezione intera nel file di configurazione, sostituendo
    /// tutte le chiavi presenti nella sezione con quelle del dizionario dato. Compatibile con
    /// <see cref="ReadSection"/>. Se la sezione non esiste la crea; se esiste, rimuove le vecchie
    /// righe della sezione prima di scrivere le nuove.</summary>
    public static void WriteSection(string file, string section, System.Collections.Generic.IDictionary<string, object?> values) {
        try {
            var lines = new List<string>();
            if (File.Exists(file)) lines.AddRange(File.ReadAllLines(file));
            // Trova l'intervallo della sezione esistente (inizio e fine).
            var inSec = string.IsNullOrEmpty(section);
            int secStart = -1, secEnd = lines.Count;
            for (var i = 0; i < lines.Count; i++) {
                var t = lines[i].Trim();
                if (t.StartsWith("[") && t.EndsWith("]")) {
                    if (inSec) { secEnd = i; break; }
                    if (t[1..^1].Equals(section, StringComparison.OrdinalIgnoreCase)) { inSec = true; secStart = i; }
                }
            }
            // Rimuovi le righe della sezione esistente.
            if (secStart >= 0) lines.RemoveRange(secStart, secEnd - secStart);
            else if (string.IsNullOrEmpty(section)) secStart = -1;
            // Costruisci le nuove righe della sezione.
            var insertIdx = secStart >= 0 ? secStart : (lines.Count > 0 && lines[^1].Trim().Length > 0 ? lines.Count : lines.Count);
            var newLines = new List<string>();
            if (!string.IsNullOrEmpty(section)) {
                if (insertIdx > 0 && lines.Count > 0 && lines[^1].Trim().Length > 0) newLines.Add("");
                newLines.Add("[" + section + "]");
            }
            foreach (var kv in values) newLines.Add(kv.Key + "=" + (kv.Value?.ToString() ?? ""));
            if (insertIdx < lines.Count) lines.InsertRange(insertIdx, newLines);
            else lines.AddRange(newLines);
            File.WriteAllLines(file, lines, Encoding.UTF8);
            _lastFileError = 0;
        } catch { _lastFileError = 1; }
    }

}
