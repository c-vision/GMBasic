// GMBasicSharp/Cli/CliTools.cs — comandi di servizio del CLI `gmb` recuperati dalla REPL C#
// eliminata (F6b): conversione documenti (CONVERT/CONVFMT) e download modelli (HF/MLX).
//
// Vivono qui, non nella REPL: la REPL C li invoca come processo (`gmb convert …`), così la
// funzione non sparisce e il mondo C# resta un engine opzionale.

using Basic.ConversionLib;
using Basic.ModelDownload;

namespace Basic.Executor;

public static class CliTools
{
    public static bool TryDispatch(string[] args)
    {
        if (args.Length == 0) return false;
        switch (args[0].ToLowerInvariant())
        {
            case "convert":
                ConvertFile(args);
                return true;
            case "convfmt":
                ConvFmt();
                return true;
            case "hf":
                Download(ModelDownloadKind.Hf, args);
                return true;
            case "mlx":
                Download(ModelDownloadKind.Mlx, args);
                return true;
            default:
                return false;
        }
    }

    private static void ConvertFile(string[] args)
    {
        var pos = new List<string>();
        string? outFile = null;
        for (var i = 1; i < args.Length; i++)
        {
            if (args[i] is "-o" or "--out" && i + 1 < args.Length) { outFile = args[++i]; continue; }
            pos.Add(args[i]);
        }
        if (pos.Count == 0)
        {
            Console.Error.WriteLine("?uso: gmb convert <file> [-o out.(md|html|txt)]");
            return;
        }
        var file = pos[0];
        if (!File.Exists(file)) { Console.Error.WriteLine($"?file non trovato: {file}"); return; }

        try
        {
            var inExt = Path.GetExtension(file);
            var outExt = outFile != null ? Path.GetExtension(outFile) : ".md";
            var result = DocumentConverter.FromBytes(File.ReadAllBytes(file), inExt, outExt);
            if (outFile != null)
            {
                File.WriteAllText(outFile, result);
                Console.WriteLine($"[OK] {outFile} ({result.Length} bytes)");
            }
            else Console.WriteLine(result);
        }
        catch (Exception ex) { Console.Error.WriteLine($"?convert: {ex.Message}"); }
    }

    private static void ConvFmt()
    {
        Console.WriteLine("Input formats:  " + string.Join(", ", DocumentConverter.SupportedFormats));
        Console.WriteLine("Output formats: md, html, txt");
    }

    private static void Download(ModelDownloadKind kind, string[] args)
    {
        var tokens = Tokenize(string.Join(' ', args.Skip(1)));
        var name = kind == ModelDownloadKind.Hf ? "HF" : "MLX";
        if (tokens.Count == 0 || tokens[0] is "--help" or "-h")
        {
            Console.WriteLine($"uso: gmb {name.ToLowerInvariant()} <repo-id> [--file F] [--dest D]");
            Console.WriteLine("     [--stall-timeout MIN] [--stall-retries N] [--workers N]");
            Console.WriteLine("     [--no-xet] [--no-token] [--no-curl] [--quiet]");
            Console.WriteLine(kind == ModelDownloadKind.Mlx
                ? "       MLX scarica sempre l'intero repo (--file non supportato)."
                : "       HF: --file seleziona un singolo file (o un pattern '*').");
            return;
        }

        var repoId = tokens[0];
        string? file = null, dest = null;
        int stallTimeout = 5, stallRetries = 1, workers = 8;
        bool noXet = false, noToken = false, noCurl = false, quiet = false;

        try
        {
            for (var i = 1; i < tokens.Count; i++)
            {
                switch (tokens[i])
                {
                    case "--file": file = Value(tokens, ref i, "--file"); break;
                    case "--dest": dest = Value(tokens, ref i, "--dest"); break;
                    case "--stall-timeout": stallTimeout = IntValue(Value(tokens, ref i, "--stall-timeout"), "--stall-timeout"); break;
                    case "--stall-retries": stallRetries = IntValue(Value(tokens, ref i, "--stall-retries"), "--stall-retries"); break;
                    case "--workers": workers = IntValue(Value(tokens, ref i, "--workers"), "--workers"); break;
                    case "--no-xet": noXet = true; break;
                    case "--no-token": noToken = true; break;
                    case "--no-curl": noCurl = true; break;
                    case "--quiet": quiet = true; break;
                    default:
                        Console.Error.WriteLine($"?opzione sconosciuta: {tokens[i]} (usa --help)");
                        return;
                }
            }
        }
        catch (ArgumentException ex) { Console.Error.WriteLine($"?{ex.Message}"); return; }

        if (kind == ModelDownloadKind.Mlx && file is not null)
        {
            Console.Error.WriteLine("?--file non supportato da MLX (scarica l'intero repo): usa HF per un singolo file.");
            return;
        }

        var opts = new ModelDownloadOptions
        {
            RepoId = repoId,
            Kind = kind,
            File = file,
            Dest = dest,
            StallTimeoutMinutes = stallTimeout,
            StallRetries = stallRetries,
            Workers = workers,
            NoXet = noXet,
            NoToken = noToken,
            NoCurl = noCurl,
            Quiet = quiet,
        };

        Console.WriteLine($"[{name}] download '{repoId}'...");
        try
        {
            using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(30) };
            var result = ModelDownloader.DownloadAsync(opts, http, Console.WriteLine, CancellationToken.None)
                .GetAwaiter().GetResult();
            if (!result.Success) { Console.Error.WriteLine($"?download fallito: {result.Error}"); return; }
            Console.WriteLine($"[OK] {result.Files.Count} file in {result.DestDir}");
            foreach (var f in result.Files) Console.WriteLine($"  {f}");
        }
        catch (Exception ex) { Console.Error.WriteLine($"?download: {ex.Message}"); }
    }

    private static string Value(List<string> t, ref int i, string flag)
    {
        if (i + 1 >= t.Count) throw new ArgumentException($"{flag} richiede un valore.");
        return t[++i];
    }

    private static int IntValue(string v, string flag)
    {
        if (!int.TryParse(v, out var n) || n < 0) throw new ArgumentException($"{flag}: valore non valido '{v}'.");
        return n;
    }

    private static List<string> Tokenize(string args)
    {
        var tokens = new List<string>();
        var cur = new System.Text.StringBuilder();
        var inQuotes = false;
        foreach (var ch in args)
        {
            if (ch == '"') { inQuotes = !inQuotes; continue; }
            if (!inQuotes && char.IsWhiteSpace(ch))
            {
                if (cur.Length > 0) { tokens.Add(cur.ToString()); cur.Clear(); }
                continue;
            }
            cur.Append(ch);
        }
        if (cur.Length > 0) tokens.Add(cur.ToString());
        return tokens;
    }
}
