// ObservationRegistry.cs — registry ContextObject (todo AI-Runtime consolidato, P1/asse A).
// SPECCHIO ESATTO del runtime C gmb_observation.c: JSON canonico con le chiavi in questo
// ordine, FNV-1a 64 bit (16 hex minuscole), id monotoni, ring 256 con drop-oldest, tail
// log default 200, testi d'errore identici. La parità è ancorata dai MiniTest con EXPECT:
// QUALSIASI divergenza di testo tra i due mondi è un bug da allineare (mai "vicino").
using System.Text;

namespace Basic.AI.Agents;

public sealed class ObservationEntry
{
    public int Id;
    public string Type = "";
    public string Source = "";
    public string Artifact = "";
    public string Hash = "";
    public long Size;
}

public static class ObservationRegistry
{
    public const int DefaultTailLines = 200;
    private const int Max = 256;
    private const string UnknownJson = "{\"error\":\"unknown observation\"}";
    private static readonly object Gate = new();
    private static readonly List<ObservationEntry> Entries = new();
    private static int _nextId = 1;

    /// <summary>Reset (hook harness/test): svuota il registry e riparte dagli id da 1.
    /// Serve al determinismo dei MiniTest nella stessa suite: senza, `delta=` di observe_file.bas
    /// dipendeva dalle osservazioni registrate dai file precedenti (singleton di processo).</summary>
    public static void Reset()
    {
        lock (Gate) { Entries.Clear(); _nextId = 1; }
    }

    public static string Fnv1a64Hex(byte[] data)
    {
        const ulong offset = 14695981039346656037UL; // FNV-1a64 offset basis (64 bit)
        const ulong prime = 1099511628211UL;
        var h = offset;
        foreach (var b in data) { h ^= b; h *= prime; }
        var chars = new char[16];
        for (var i = 0; i < 16; i++) chars[15 - i] = "0123456789abcdef"[(int)((h >> (i * 4)) & 0xF)];
        return new string(chars);
    }

    private static void Add(ObservationEntry e)
    {
        if (Entries.Count >= Max) Entries.RemoveAt(0);
        e.Id = _nextId++;
        Entries.Add(e);
    }

    private static ObservationEntry? Find(int id) => Entries.Find(e => e.Id == id);

    public static long RecordFile(string path)
    {
        if (string.IsNullOrEmpty(path)) return -1;
        byte[] bytes;
        try { bytes = File.ReadAllBytes(path); } catch { return -1; }
        lock (Gate)
        {
            Add(new ObservationEntry
            {
                Type = "file",
                Source = path,
                Artifact = path,
                Hash = Fnv1a64Hex(bytes),
                Size = bytes.LongLength,
            });
            return Entries[^1].Id;
        }
    }

    /// <summary>Registra un ContextObject con un TYPE esplicito (le nuove fonti OBSERVE: test,
    /// database, process, browser) hashandо l'artefatto passato. Speculare a gmb_obs_record_typed.
    /// -1 se il file non è leggibile (il chiamante alza un errore ESPLICITO, mai silenzioso).</summary>
    public static long RecordTyped(string type, string source, string artifactPath)
    {
        if (string.IsNullOrEmpty(artifactPath)) return -1;
        byte[] bytes;
        try { bytes = File.ReadAllBytes(artifactPath); } catch { return -1; }
        lock (Gate)
        {
            Add(new ObservationEntry
            {
                Type = string.IsNullOrEmpty(type) ? "file" : type,
                Source = source ?? "",
                Artifact = artifactPath,
                Hash = Fnv1a64Hex(bytes),
                Size = bytes.LongLength,
            });
            return Entries[^1].Id;
        }
    }

    public static long RecordLog(string path, double tail)
    {
        if (string.IsNullOrEmpty(path)) return -1;
        var tailLines = (int)tail;
        if (tailLines <= 0) tailLines = DefaultTailLines;
        byte[] bytes;
        try { bytes = File.ReadAllBytes(path); } catch { return -1; }
        // contenuto = ultime tailLines righe, newline finali esclusi (byte-level, come il C)
        var len = bytes.Length;
        while (len > 0 && (bytes[len - 1] == (byte)'\n' || bytes[len - 1] == (byte)'\r')) len--;
        var start = 0;
        var nl = 0;
        for (var i = len - 1; i >= 0; i--)
        {
            if (bytes[i] != (byte)'\n') continue;
            nl++;
            if (nl < tailLines) continue;
            start = i + 1;
            break;
        }
        var slice = new byte[len - start];
        Array.Copy(bytes, start, slice, 0, slice.Length);
        lock (Gate)
        {
            Add(new ObservationEntry
            {
                Type = "log",
                Source = path,
                Artifact = path,
                Hash = Fnv1a64Hex(slice),
                Size = slice.LongLength,
            });
            return Entries[^1].Id;
        }
    }

    public static long RecordScreen(string outPath)
    {
        var shot = Basic.AI.Managed.ManagedMacCapture.Screenshot(string.IsNullOrEmpty(outPath) ? "" : outPath);
        if (string.IsNullOrEmpty(shot) || shot.StartsWith("error", StringComparison.Ordinal)) return -1;
        return RecordFile(shot);
    }

    public static string LatestId()
    {
        lock (Gate) { return _nextId <= 1 ? "" : (_nextId - 1).ToString(); }
    }

    public static string Json(double id)
    {
        lock (Gate)
        {
            var e = Find((int)id);
            if (e is null) return UnknownJson;
            var sb = new StringBuilder(192);
            sb.Append("{\"id\":").Append(e.Id)
              .Append(",\"type\":\"").Append(e.Type)
              .Append("\",\"source\":\"").Append(e.Source)
              .Append("\",\"artifact\":\"").Append(e.Artifact)
              .Append("\",\"hash\":\"").Append(e.Hash)
              .Append("\",\"size\":").Append(e.Size).Append('}');
            return sb.ToString();
        }
    }

    public static string HashOf(double id)
    {
        lock (Gate) return Find((int)id)?.Hash ?? "";
    }

    public static double Count()
    {
        lock (Gate) return Entries.Count;
    }

    public static string List()
    {
        lock (Gate)
        {
            return string.Join("\n", Entries.Select(e => $"{e.Id}|{e.Type}|{e.Hash}"));
        }
    }

    public static string FileOf(double id)
    {
        lock (Gate) return Find((int)id)?.Artifact ?? "";
    }
}
