// FILE: REPL/CliGrep.cs
// CLI headless `gmb grep` / `gmb rg` — ricerca file via il motore C condiviso (P/Invoke
// NativeGrep su libgmb_grep, stessa sorgente di VM/C, C99 e delle due REPL).
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Basic.Executor;

public static class CliGrep
{
    public static bool TryDispatch(string[] args)
    {
        if (args.Length == 0) return false;
        var verb = args[0];
        if (verb is not ("grep" or "rg")) return false;

        var toks = new List<string>(args[1..]);
        // Separa i flag con valore attaccato (-A3 → -A 3), poi espandi i flag corti combinati
        // (es. -rn → -r -n). grep usa molto -Rin; -A/-B/-C/-m prendono un valore.
        var norm = new List<string>();
        foreach (var t in toks)
        {
            if (t.Length > 2 && t[0] == '-' && t[1] != '-' && (t[1] is 'A' or 'B' or 'C' or 'm') && char.IsDigit(t[2]))
            { norm.Add("-" + t[1]); norm.Add(t[2..]); }
            else norm.Add(t);
        }
        var flat = new List<string>();
        foreach (var t in norm)
        {
            if (t.Length > 2 && t[0] == '-' && t[1] != '-' && !t.Contains('=') && !(t[1] is 'A' or 'B' or 'C' or 'm'))
            { for (int k = 1; k < t.Length; k++) flat.Add("-" + t[k]); }
            else flat.Add(t);
        }
        toks = flat;
        var opts = new StringBuilder("{");
        bool first = true;
        void Add(string kv) { if (!first) opts.Append(','); first = false; opts.Append(kv); }
        string? pattern = null, path = null;
        for (int i = 0; i < toks.Count; i++)
        {
            var t = toks[i];
            switch (t)
            {
                case "-F" or "--fixed": Add("\"fixed\":true"); break;
                case "-i": Add("\"ignore_case\":true"); break;
                case "-w": Add("\"word\":true"); break;
                case "-v": Add("\"invert\":true"); break;
                case "-r" or "-R": Add("\"recursive\":true"); break;
                case "-n": Add("\"line_number\":true"); break;
                case "-l": Add("\"files_with_matches\":true"); break;
                case "-c": Add("\"count_only\":true"); break;
                case "-o": Add("\"only_matching\":true"); break;
                case "-.": Add("\"hidden\":true"); break;
                case "--type" when i + 1 < toks.Count: Add($"\"type\":{JsonStr(toks[++i])}"); break;
                case "--include" when i + 1 < toks.Count: Add($"\"include\":{JsonStr(toks[++i])}"); break;
                case "--exclude" when i + 1 < toks.Count: Add($"\"exclude\":{JsonStr(toks[++i])}"); break;
                case "--depth" when i + 1 < toks.Count: Add($"\"max_depth\":{int.Parse(toks[++i])}"); break;
                case "-m" when i + 1 < toks.Count: Add($"\"max_count\":{int.Parse(toks[++i])}"); break;
                case "-A" when i + 1 < toks.Count: Add($"\"context_after\":{int.Parse(toks[++i])}"); break;
                case "-B" when i + 1 < toks.Count: Add($"\"context_before\":{int.Parse(toks[++i])}"); break;
                case "-C" when i + 1 < toks.Count: { var cv = int.Parse(toks[++i]); Add($"\"context_before\":{cv}"); Add($"\"context_after\":{cv}"); } break;
                default:
                    if (pattern is null) pattern = t;
                    else if (path is null) path = t;
                    else { Console.Error.WriteLine($"?grep: argomento extra '{t}'"); return true; }
                    break;
            }
        }
        if (pattern is null)
        {
            Console.Error.WriteLine("uso: gmb grep [-rinlcFwvo-.] [--type EXT] [--include G] [--exclude G] [--depth N] PATTERN [PATH]");
            return true;
        }
        Add($"\"pattern\":{JsonStr(pattern)}");
        opts.Append('}');
        var target = string.IsNullOrEmpty(path) ? Directory.GetCurrentDirectory() : Path.GetFullPath(path);
        try
        {
            var hits = NativeGrep.SearchJson(opts.ToString(), target);
            var txt = NativeGrep.FormatJson(hits, true, true);
            if (txt.Length > 0) Console.Out.Write(txt);
        }
        catch (Exception ex) { Console.Error.WriteLine($"?grep non disponibile ({ex.Message})"); }
        return true;
    }

    private static string JsonStr(string s) =>
        "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
}
