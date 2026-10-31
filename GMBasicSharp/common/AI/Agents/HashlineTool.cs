// FILE: Basic.Agents.HashlineTool — Orchestrazione agenti (A10): Hash-anchored edit (Hashline).
// Ogni riga è taggata `N#HASH| contenuto` (hash FNV-1a 32-bit, 4 cifre esadecimali): l'agente
// edita riferendo il tag, e l'edit è rifiutato se il contenuto è cambiato (hash mismatch) —
// niente errori da riga stale. Deterministico.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_hashline.c):
// stesso formato tag, stesso hash, stesse operazioni.
using System.Text;

namespace Basic.Agents;

/// <summary>Tool Hashline: tagga le righe e applica edit ancorati al contenuto.</summary>
public static class HashlineTool
{
    /// <summary>Hash FNV-1a 32-bit della riga, 4 cifre esadecimali maiuscole.</summary>
    private static uint Hash(string line)
    {
        uint h = 2166136261u;
        foreach (var c in line)
        {
            h ^= c;
            h *= 16777619u;
        }
        return h;
    }

    /// <summary>Tagga ogni riga: `N#HASH| contenuto` (N 1-based). Le righe vuote finali non
    /// producono un tag in più.</summary>
    public static string Tag(string content)
    {
        var lines = (content ?? "").Split('\n');
        var sb = new StringBuilder();
        for (var i = 0; i < lines.Length; i++)
        {
            if (i > 0) sb.Append('\n');
            var line = lines[i].TrimEnd('\r');
            sb.Append(i + 1).Append('#').Append(Hash(line).ToString("X4")).Append("| ").Append(line);
        }
        return sb.ToString();
    }

    /// <summary>Applica un edit: `lineRef` = "N#HASH" (o "N"). Verifica l'hash della riga N e
    /// sostituisce il contenuto. Su mismatch/indice fuori range → stringa "Error: ...".</summary>
    public static string Edit(string content, string lineRef, string newText)
    {
        var lines = (content ?? "").Split('\n');
        for (var i = 0; i < lines.Length; i++) lines[i] = lines[i].TrimEnd('\r');
        var (n, want) = ParseRef(lineRef);
        if (n < 1 || n > lines.Length) return $"Error: riga {n} fuori range (1..{lines.Length})";
        if (want.Length > 0)
        {
            var have = Hash(lines[n - 1]).ToString("X4");
            if (!have.Equals(want, System.StringComparison.OrdinalIgnoreCase))
                return $"Error: hash mismatch riga {n} (atteso {want}, trovato {have})";
        }
        lines[n - 1] = newText ?? "";
        return string.Join("\n", lines);
    }

    /// <summary>1 se il tag corrisponde alla riga corrente, 0 altrimenti.</summary>
    public static double Verify(string content, string lineRef)
    {
        var lines = (content ?? "").Split('\n');
        for (var i = 0; i < lines.Length; i++) lines[i] = lines[i].TrimEnd('\r');
        var (n, want) = ParseRef(lineRef);
        if (n < 1 || n > lines.Length) return 0.0;
        if (want.Length == 0) return 1.0;
        return Hash(lines[n - 1]).ToString("X4").Equals(want, System.StringComparison.OrdinalIgnoreCase) ? 1.0 : 0.0;
    }

    private static (int N, string Hash) ParseRef(string lineRef)
    {
        var s = (lineRef ?? "").Trim();
        var hashPos = s.IndexOf('#');
        if (hashPos < 0)
            return int.TryParse(s, out var nOnly) ? (nOnly, "") : (0, "");
        var numPart = s[..hashPos];
        var hashPart = s[(hashPos + 1)..].Trim();
        return int.TryParse(numPart, out var n) ? (n, hashPart) : (0, hashPart);
    }
}
